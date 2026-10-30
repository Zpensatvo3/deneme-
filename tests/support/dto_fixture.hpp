#pragma once
#include "../../src/modules/core/setting_schema.hpp"

// Local DTO codec fixture, not a production settings owner. Legacy golden
// format tests remain independent of the JNI transport and its read-only cache.
template<class Model> class dto_fixture
{
public:
    Model read() const { return value; }
    bool visit(modules::config_visitor& visitor, std::span<const modules::settings::descriptor<Model>> schema)
    {
        if constexpr (requires { Model::visit(visitor, value); }) return Model::visit(visitor, value);
        else return modules::settings::visit(visitor, value, schema);
    }
    template<class Editor> bool update(Editor&& editor) {
        auto candidate = value;
        if (!editor(candidate) || candidate == value) return false;
        value = candidate; return true;
    }
private:
    Model value{};
};
