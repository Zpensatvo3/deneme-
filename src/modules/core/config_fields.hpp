#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <functional>

namespace modules
{
    class config_visitor
    {
    public:
        virtual ~config_visitor() = default;

        virtual bool reading() const = 0;
        virtual bool importing() const { return reading(); }
        virtual bool has_field(const char*) const { return false; }
        virtual bool valid() const { return true; }
        virtual void reject(const char*) {}
        // Validation assignments must target local candidates, never live fields.
        virtual void stage_values(bool) {}
        virtual void field(const char* name, bool& value) = 0;
        virtual void field(const char* name, int& value) = 0;
        virtual void field(const char* name, float& value) = 0;
        virtual void field(const char* name, double& value) = 0;
        virtual void field(const char* name, std::string& value) = 0;
        virtual void field(const char* name, int* values, std::size_t count) = 0;
        virtual void field(const char* name, float* values, std::size_t count) = 0;
        virtual void field(const char* name, std::vector<std::string>& values) = 0;
        virtual void enum_value(const char* name, int& value) { field(name, value); }

        template <std::size_t Count>
        void field(const char* name, int (&values)[Count])
        {
            field(name, values, Count);
        }

        template <std::size_t Count>
        void field(const char* name, float (&values)[Count])
        {
            field(name, values, Count);
        }
    };

    // DTO codecs only. Live state and migrations belong to Java ConfigManager.
    std::string serialize_config_fields(const std::function<void(config_visitor&)>& visit);
    bool validate_config_fields(const std::function<void(config_visitor&)>& visit, const std::string& object);
    bool deserialize_config_fields(const std::function<void(config_visitor&)>& visit, const std::string& object);
    std::string config_validation_error(const std::function<void(config_visitor&)>& visit, const std::string& object);
}
