#include "config_fields.hpp"
#include "module.hpp"
#include "config_json.hpp"

#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <string_view>

namespace
{
    std::string escape_json(std::string_view value)
    {
        std::string output;
        output.reserve(value.size() + 8);
        for (const unsigned char character : value)
        {
            switch (character)
            {
            case '\\': output += "\\\\"; break;
            case '"': output += "\\\""; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (character < 0x20)
                {
                    static constexpr char digits[] = "0123456789abcdef";
                    output += "\\u00";
                    output.push_back(digits[(character >> 4) & 0x0f]);
                    output.push_back(digits[character & 0x0f]);
                }
                else output.push_back(static_cast<char>(character));
                break;
            }
        }
        return output;
    }

    void skip_spaces(std::string_view text, std::size_t& position)
    {
        while (position < text.size() && static_cast<unsigned char>(text[position]) <= 0x20) ++position;
    }

    bool parse_json_string(std::string_view raw, std::string& output)
    {
        const auto parsed = modules::config_json::parse(raw);
        if (!parsed || parsed->type != modules::config_json::kind::string) return false;
        output = parsed->text;
        return true;
    }

    template <typename Number>
    bool parse_integer(std::string_view raw, Number& output)
    {
        Number parsed{};
        const auto result = std::from_chars(raw.data(), raw.data() + raw.size(), parsed);
        if (result.ec != std::errc{} || result.ptr != raw.data() + raw.size()) return false;
        if (parsed < static_cast<Number>(-1000000) || parsed > static_cast<Number>(1000000)) return false;
        output = parsed;
        return true;
    }

    template <typename Number>
    bool parse_decimal(std::string_view raw, Number& output)
    {
        double parsed{};
        const auto result = std::from_chars(raw.data(), raw.data() + raw.size(), parsed);
        if (result.ec != std::errc{} || result.ptr != raw.data() + raw.size() || !std::isfinite(parsed)) return false;
        if (parsed < -1000000.0 || parsed > 1000000.0) return false;
        output = static_cast<Number>(parsed);
        return true;
    }

    template <typename Value, typename Parser>
    bool parse_number_array(std::string_view raw, Value* output, std::size_t count, Parser parser, bool assign)
    {
        if (raw.size() < 2 || raw.front() != '[' || raw.back() != ']') return false;
        std::size_t position = 1;
        for (std::size_t index = 0; index < count; ++index)
        {
            skip_spaces(raw, position);
            const std::size_t start = position;
            while (position + 1 < raw.size() && raw[position] != ',' && raw[position] != ']') ++position;
            std::size_t end = position;
            while (end > start && static_cast<unsigned char>(raw[end - 1]) <= 0x20) --end;
            Value parsed{};
            if (start == end || !parser(raw.substr(start, end - start), parsed)) return false;
            if (assign) output[index] = parsed;
            skip_spaces(raw, position);
            if (index + 1 < count)
            {
                if (position >= raw.size() || raw[position++] != ',') return false;
            }
        }
        skip_spaces(raw, position);
        return position + 1 == raw.size() && raw[position] == ']';
    }

    class writer final : public modules::config_visitor
    {
    public:
        bool reading() const override { return false; }
        void field(const char* name, bool& value) override { prefix(name); output += value ? "true" : "false"; }
        void field(const char* name, int& value) override { prefix(name); output += std::to_string(value); }
        void field(const char* name, float& value) override { number(name, value); }
        void field(const char* name, double& value) override { number(name, value); }
        void field(const char* name, std::string& value) override { prefix(name); output += '"' + escape_json(value) + '"'; }
        void field(const char* name, int* values, std::size_t count) override { array(name, values, count); }
        void field(const char* name, float* values, std::size_t count) override { array(name, values, count); }
        void field(const char* name, std::vector<std::string>& values) override
        {
            prefix(name); output.push_back('[');
            for (std::size_t index = 0; index < values.size(); ++index)
            {
                if (index) output.push_back(',');
                output += '"' + escape_json(values[index]) + '"';
            }
            output.push_back(']');
        }

        std::string finish() { return '{' + output + '}'; }

    private:
        std::string output;
        void prefix(const char* name)
        {
            if (!output.empty()) output.push_back(',');
            output += '"' + escape_json(name ? name : "") + "\":";
        }
        template <typename Number> void number(const char* name, Number value)
        {
            prefix(name);
            std::ostringstream stream;
            stream.imbue(std::locale::classic());
            stream << std::setprecision(std::numeric_limits<Number>::max_digits10) << value;
            output += stream.str();
        }
        template <typename Number> void array(const char* name, Number* values, std::size_t count)
        {
            prefix(name); output.push_back('[');
            for (std::size_t index = 0; index < count; ++index)
            {
                if (index) output.push_back(',');
                if constexpr (std::is_integral_v<Number>) output += std::to_string(values[index]);
                else
                {
                    std::ostringstream stream;
                    stream.imbue(std::locale::classic());
                    stream << std::setprecision(std::numeric_limits<Number>::max_digits10) << values[index];
                    output += stream.str();
                }
            }
            output.push_back(']');
        }
    };

    class reader final : public modules::config_visitor
    {
    public:
        reader(std::string_view object, bool apply) : parsed(modules::config_json::parse(object)), apply(apply)
        {
            if (!parsed || parsed->type != modules::config_json::kind::object) {
                valid_state = false; error = "Invalid settings JSON.";
            }
        }
        bool reading() const override { return apply; }
        bool valid() const override { return valid_state; }
        bool importing() const override { return true; }
        bool has_field(const char* name) const override { return raw_field(name).has_value(); }
        void stage_values(bool enabled) override { staged = enabled; }
        void reject(const char* message) override { valid_state = false; if (error.empty()) error = message ? message : "Invalid settings."; }
        const std::string& message() const { return error; }

        void field(const char* name, bool& value) override
        {
            const auto raw = raw_field(name); if (!raw) return;
            bool parsed{};
            if (*raw == "true") parsed = true;
            else if (*raw == "false") parsed = false;
            else { reject_field(name); return; }
            if (apply || staged) value = parsed;
        }
        void field(const char* name, int& value) override
        {
            const auto raw = raw_field(name); if (!raw) return;
            int parsed{}; if (!parse_integer(*raw, parsed)) { reject_field(name); return; }
            if (apply || staged) value = parsed;
        }
        void field(const char* name, float& value) override { decimal(name, value); }
        void field(const char* name, double& value) override { decimal(name, value); }
        void field(const char* name, std::string& value) override
        {
            const auto raw = raw_field(name); if (!raw) return;
            std::string parsed; if (!parse_json_string(*raw, parsed)) { reject_field(name); return; }
            if (apply || staged) value = std::move(parsed);
        }
        void field(const char* name, int* values, std::size_t count) override
        {
            const auto raw = raw_field(name); if (!raw) return;
            if (!parse_number_array(*raw, values, count, parse_integer<int>, apply || staged)) reject_field(name);
        }
        void field(const char* name, float* values, std::size_t count) override
        {
            const auto raw = raw_field(name); if (!raw) return;
            if (!parse_number_array(*raw, values, count, parse_decimal<float>, apply || staged)) reject_field(name);
        }
        void field(const char* name, std::vector<std::string>& values) override
        {
            const auto raw = raw_field(name); if (!raw) return;
            if (raw->size() < 2 || raw->front() != '[' || raw->back() != ']') { reject_field(name); return; }
            std::vector<std::string> parsed;
            std::size_t position = 1;
            while (position + 1 < raw->size())
            {
                skip_spaces(*raw, position);
                if ((*raw)[position] == ']') break;
                if ((*raw)[position] != '"') { reject_field(name); return; }
                const std::size_t start = position++;
                bool escaped = false;
                while (position < raw->size())
                {
                    const char character = (*raw)[position++];
                    if (escaped) escaped = false;
                    else if (character == '\\') escaped = true;
                    else if (character == '"') break;
                }
                std::string item;
                if (!parse_json_string(raw->substr(start, position - start), item)) { reject_field(name); return; }
                if (item.size() > 128 || parsed.size() >= 256) { reject_field(name); return; }
                parsed.push_back(std::move(item));
                skip_spaces(*raw, position);
                if (position < raw->size() && (*raw)[position] == ',') ++position;
                else if (position < raw->size() && (*raw)[position] != ']') { reject_field(name); return; }
            }
            if (apply || staged) values = std::move(parsed);
        }

    private:
        std::optional<modules::config_json::value> parsed;
        std::optional<std::string_view> raw_field(const char* name) const
        {
            if (!parsed) return std::nullopt;
            const auto* field = parsed->find(name);
            return field ? std::optional<std::string_view>(field->raw) : std::nullopt;
        }
        void reject_field(const char* name)
        {
            const std::string detail = std::string(name) + ": invalid value or type.";
            reject(detail.c_str());
        }
        bool apply = false;
        bool valid_state = true;
        bool staged = false;
        std::string error;
        template <typename Number> void decimal(const char* name, Number& value)
        {
            const auto raw = raw_field(name); if (!raw) return;
            Number parsed{}; if (!parse_decimal(*raw, parsed)) { reject_field(name); return; }
            if (apply || staged) value = parsed;
        }
    };
}

std::string modules::serialize_config_fields(module& owner)
{
    writer output;
    owner.visit_config(output);
    return output.finish();
}

bool modules::deserialize_config_fields(module& owner, const std::string& object)
{
    if (!validate_config_fields(owner, object)) return false;
    reader application(object, true);
    owner.visit_config(application);
    return application.valid();
}

bool modules::validate_config_fields(module& owner, const std::string& object)
{
    reader validation(object, false);
    owner.visit_config(validation);
    return validation.valid();
}

std::string modules::config_validation_error(module& owner, const std::string& object)
{
    reader validation(object, false);
    owner.visit_config(validation);
    return validation.message().empty() ? "Invalid module settings." : validation.message();
}
