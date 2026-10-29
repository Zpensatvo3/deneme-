#pragma once

#include "../modules/core/setting_schema.hpp"
#include "../modules/core/settings_store.hpp"
#include "../gui/widgets.hpp"
#include <vector>

namespace gui::settings
{
    template<class Config>
    void render(Config& candidate, std::span<const modules::settings::descriptor<Config>> schema)
    {
        using modules::settings::control;
        for (const auto& d : schema)
        {
            if (!d.visible || (d.visible_if && !(candidate.*d.visible_if)) || (d.condition && !d.condition(candidate))) continue;
            const char* title = d.title.english;
            const char* format = d.format;
            ImGui::PushID(d.key);
            switch (d.type)
            {
            case control::color: gui::option_color(title, candidate.*d.float_array); break;
            case control::string_list: break; // Custom pickers own list selection UI.
            case control::real_pair:
            {
                auto& raw = candidate.*d.float_pair;
                float values[]{raw[0] * d.display_scale, raw[1] * d.display_scale};
                if (gui::option_range(title, values, d.minimum * d.display_scale, d.maximum * d.display_scale, format)) {
                    raw[0] = values[0] / d.display_scale; raw[1] = values[1] / d.display_scale;
                }
                break;
            }
            case control::integer_pair:
                gui::option_range(title, candidate.*d.int_pair, d.integer_minimum, d.integer_maximum, format);
                break;
            case control::integer:
            {
                const int minimum = d.minimum_from
                    ? std::clamp(candidate.*d.minimum_from, d.integer_minimum, d.integer_maximum)
                    : d.integer_minimum;
                gui::option_slider(title, &(candidate.*d.integer_value), (std::max)(minimum, d.integer_display_minimum),
                    (std::max)(minimum, d.integer_display_maximum), format);
                break;
            }
            case control::toggle:
                gui::option_toggle(title, &(candidate.*d.boolean));
                break;
            case control::number:
                gui::option_slider(title, &(candidate.*d.scalar), d.number_display_minimum, d.number_display_maximum, format);
                break;
            case control::range:
            {
                float values[]{candidate.*d.scalar, candidate.*d.upper};
                gui::option_range(title, values, d.minimum, d.maximum, format);
                candidate.*d.scalar = values[0]; candidate.*d.upper = values[1];
                break;
            }
            case control::choice:
            {
                std::vector<const char*> names;
                names.reserve(d.choices.size());
                for (const auto& option : d.choices) names.push_back(option.english);
                int index = d.selected(candidate);
                if (!d.choice_values.empty()) {
                    const auto found = std::find(d.choice_values.begin(), d.choice_values.end(), index);
                    index = found == d.choice_values.end() ? 0 : static_cast<int>(found - d.choice_values.begin());
                }
                if (gui::option_combo(title, &index, names.data(), static_cast<int>(names.size())))
                    d.select(candidate, d.choice_values.empty() ? index : d.choice_values[index]);
                break;
            }
            }
            ImGui::PopID();
        }
    }

    template<class Config>
    bool edit(modules::settings::store<Config>& settings,
        std::span<const modules::settings::descriptor<Config>> schema, auto&& panel)
    {
        return settings.update([&](Config& candidate) {
            panel(candidate);
            return modules::settings::normalize(candidate, schema);
        });
    }

    template<class Config>
    bool render(modules::settings::store<Config>& settings,
        std::span<const modules::settings::descriptor<Config>> schema)
    {
        return settings.update([&](Config& candidate) {
            render(candidate, schema);
            return modules::settings::normalize(candidate, schema);
        });
    }
}
