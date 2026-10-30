#pragma once

#include <charconv>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Small bounded JSON reader shared by config documents and module settings.
// Views refer to the input document, which must outlive the parsed tree.
namespace modules::config_json
{
    enum class kind { object, array, string, number, boolean, null };
    struct value
    {
        kind type = kind::null;
        std::string_view raw;
        std::string text;
        bool boolean = false;
        std::map<std::string, value, std::less<>> members;
        std::vector<value> items;
        const value* find(std::string_view key) const
        {
            const auto it = members.find(key);
            return it == members.end() ? nullptr : &it->second;
        }
        std::optional<int> integer() const
        {
            if (type != kind::number) return std::nullopt;
            int result{};
            const auto parsed = std::from_chars(raw.data(), raw.data() + raw.size(), result);
            if (parsed.ec != std::errc{} || parsed.ptr != raw.data() + raw.size()) return std::nullopt;
            return result;
        }
    };

    class parser
    {
    public:
        explicit parser(std::string_view input, std::size_t maximum_bytes = 96 * 1024) : input_(input), maximum_bytes_(maximum_bytes) {}
        std::optional<value> parse()
        {
            if (input_.empty() || input_.size() > maximum_bytes_) return std::nullopt;
            value result;
            if (!read(result, 0)) return std::nullopt;
            spaces();
            if (position_ != input_.size()) return std::nullopt;
            return result;
        }
    private:
        std::string_view input_;
        std::size_t maximum_bytes_;
        std::size_t position_ = 0, nodes_ = 0;
        void spaces()
        {
            while (position_ < input_.size() && (input_[position_] == ' ' || input_[position_] == '\t' ||
                input_[position_] == '\r' || input_[position_] == '\n')) ++position_;
        }
        bool take(char c)
        {
            if (position_ >= input_.size() || input_[position_] != c) return false;
            ++position_; return true;
        }
        bool word(std::string_view text)
        {
            if (input_.substr(position_, text.size()) != text) return false;
            position_ += text.size(); return true;
        }
        static bool digit(char c) { return c >= '0' && c <= '9'; }
        bool hex(unsigned& code)
        {
            code = 0;
            for (int i = 0; i < 4; ++i) {
                if (position_ >= input_.size()) return false;
                const char c = input_[position_++];
                const int n = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                    c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
                if (n < 0) return false;
                code = code * 16 + static_cast<unsigned>(n);
            }
            return true;
        }
        static void utf8(std::string& output, unsigned code)
        {
            if (code < 0x80) output += static_cast<char>(code);
            else if (code < 0x800) {
                output += static_cast<char>(0xc0 | (code >> 6));
                output += static_cast<char>(0x80 | (code & 63));
            } else if (code < 0x10000) {
                output += static_cast<char>(0xe0 | (code >> 12));
                output += static_cast<char>(0x80 | ((code >> 6) & 63));
                output += static_cast<char>(0x80 | (code & 63));
            } else {
                output += static_cast<char>(0xf0 | (code >> 18));
                output += static_cast<char>(0x80 | ((code >> 12) & 63));
                output += static_cast<char>(0x80 | ((code >> 6) & 63));
                output += static_cast<char>(0x80 | (code & 63));
            }
        }
        bool string(std::string& output)
        {
            if (!take('"')) return false;
            while (position_ < input_.size()) {
                const auto c = static_cast<unsigned char>(input_[position_++]);
                if (c == '"') return true;
                if (c < 0x20) return false;
                if (c != '\\') {
                    if (c < 0x80) { output += static_cast<char>(c); continue; }
                    const int count = c >= 0xc2 && c <= 0xdf ? 1 : c >= 0xe0 && c <= 0xef ? 2 :
                        c >= 0xf0 && c <= 0xf4 ? 3 : -1;
                    if (count < 0 || position_ + count > input_.size()) return false;
                    unsigned code = c & (count == 1 ? 31 : count == 2 ? 15 : 7);
                    for (int i = 0; i < count; ++i) {
                        const auto next = static_cast<unsigned char>(input_[position_++]);
                        if ((next & 0xc0) != 0x80) return false;
                        code = (code << 6) | (next & 63);
                    }
                    if (code < (count == 1 ? 0x80u : count == 2 ? 0x800u : 0x10000u) ||
                        code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return false;
                    utf8(output, code); continue;
                }
                if (position_ >= input_.size()) return false;
                switch (input_[position_++]) {
                case '"': output += '"'; break;
                case '\\': output += '\\'; break;
                case '/': output += '/'; break;
                case 'b': output += '\b'; break;
                case 'f': output += '\f'; break;
                case 'n': output += '\n'; break;
                case 'r': output += '\r'; break;
                case 't': output += '\t'; break;
                case 'u': {
                    unsigned code{};
                    if (!hex(code)) return false;
                    if (code >= 0xd800 && code <= 0xdbff) {
                        unsigned low{};
                        if (!take('\\') || !take('u') || !hex(low) || low < 0xdc00 || low > 0xdfff) return false;
                        code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
                    } else if (code >= 0xdc00 && code <= 0xdfff) return false;
                    utf8(output, code); break;
                }
                default: return false;
                }
            }
            return false;
        }
        bool number()
        {
            take('-');
            if (!take('0')) {
                if (position_ >= input_.size() || input_[position_] < '1' || input_[position_] > '9') return false;
                while (position_ < input_.size() && digit(input_[position_])) ++position_;
            }
            if (take('.')) {
                const auto start = position_;
                while (position_ < input_.size() && digit(input_[position_])) ++position_;
                if (start == position_) return false;
            }
            if (take('e') || take('E')) {
                if (!take('+')) take('-');
                const auto start = position_;
                while (position_ < input_.size() && digit(input_[position_])) ++position_;
                if (start == position_) return false;
            }
            return true;
        }
        bool read(value& output, unsigned depth)
        {
            if (depth > 32 || ++nodes_ > 16384) return false;
            spaces();
            if (position_ >= input_.size()) return false;
            const auto start = position_;
            if (take('{')) {
                output.type = kind::object; spaces();
                if (!take('}')) for (;;) {
                    std::string key;
                    if (!string(key)) return false;
                    spaces();
                    if (!take(':')) return false;
                    value child;
                    if (!read(child, depth + 1) || !output.members.emplace(std::move(key), std::move(child)).second) return false;
                    spaces();
                    if (take('}')) break;
                    if (!take(',')) return false;
                    spaces();
                }
            } else if (take('[')) {
                output.type = kind::array; spaces();
                if (!take(']')) for (;;) {
                    value child;
                    if (!read(child, depth + 1)) return false;
                    output.items.push_back(std::move(child));
                    spaces();
                    if (take(']')) break;
                    if (!take(',')) return false;
                }
            } else if (input_[position_] == '"') {
                output.type = kind::string;
                if (!string(output.text)) return false;
            } else if (word("true")) { output.type = kind::boolean; output.boolean = true; }
            else if (word("false")) { output.type = kind::boolean; output.boolean = false; }
            else if (word("null")) output.type = kind::null;
            else {
                output.type = kind::number;
                if (!number()) return false;
            }
            output.raw = input_.substr(start, position_ - start);
            return true;
        }
    };

    inline std::optional<value> parse(std::string_view input) { return parser(input).parse(); }
    inline std::optional<value> protocol(std::string_view input) { return parser(input, 6 * 96 * 1024 + 4096).parse(); }
    inline std::string escape(std::string_view input)
    {
        std::string output;
        static constexpr char hex[] = "0123456789abcdef";
        for (const unsigned char c : input) {
            if (c == '"' || c == '\\') { output += '\\'; output += static_cast<char>(c); }
            else if (c < 0x20) { output += "\\u00"; output += hex[c >> 4]; output += hex[c & 15]; }
            else output += static_cast<char>(c);
        }
        return output;
    }
}
