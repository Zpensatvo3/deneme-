#pragma once

#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>

namespace modules
{
    class module;

    class config_visitor
    {
    public:
        virtual ~config_visitor() = default;

        virtual bool reading() const = 0;
        virtual bool updating() const { return false; }
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
        virtual void bounded_enum(const char* name, int& value, int minimum, int maximum)
        {
            enum_value(name, value);
            if (importing() && (value < minimum || value > maximum)) reject("Invalid enum value.");
        }

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

        template <typename Enum, typename = std::enable_if_t<std::is_enum_v<Enum>>>
        void enum_field(const char* name, Enum& value)
        {
            int stored = static_cast<int>(value);
            enum_value(name, stored);
            if (reading()) value = static_cast<Enum>(stored);
        }
    };

    std::string serialize_config_fields(module& owner);
    bool validate_config_fields(module& owner, const std::string& object);
    bool deserialize_config_fields(module& owner, const std::string& object);
    std::string config_validation_error(module& owner, const std::string& object);
}
