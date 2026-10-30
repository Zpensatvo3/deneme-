#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct auto_totem
    {
        int mode = modules::catalog::auto_totem_mode_default; // Argon-style Blatant / Random selection.
        int delay = modules::catalog::auto_totem_delay_default;
        bool hotbar = modules::catalog::auto_totem_hotbar_default;
        int totem_slot = modules::catalog::auto_totem_totem_slot_default;
        bool auto_switch = modules::catalog::auto_totem_auto_switch_default;
        bool force_totem = modules::catalog::auto_totem_force_totem_default;
        bool auto_open = modules::catalog::auto_totem_auto_open_default;
        int stay_open_for = modules::catalog::auto_totem_stay_open_for_default;
        bool operator==(const auto_totem&) const = default;
        static std::span<const modules::settings::descriptor<auto_totem>> schema()
        {
            using model = auto_totem;
            using setting = modules::settings::descriptor<model>;
            static constexpr modules::settings::label modes[]{{"Blatant"}, {"Random"}};
            static const setting fields[]{
                setting::choice("mode", {"Mode"}, &model::mode, modes),
                setting::integer("delay", {"Delay"}, &model::delay, modules::catalog::auto_totem_delay_minimum, modules::catalog::auto_totem_delay_maximum, "%d ticks"),
                setting::toggle("hotbar", {"Hotbar"}, &model::hotbar),
                setting::integer("totem_slot", {"Totem Slot"}, &model::totem_slot, modules::catalog::auto_totem_totem_slot_minimum, modules::catalog::auto_totem_totem_slot_maximum, "Slot %d").when([](const model& c) { return c.hotbar || c.auto_switch; }),
                setting::toggle("auto_switch", {"Auto Switch"}, &model::auto_switch),
                setting::toggle("force_totem", {"Force Totem"}, &model::force_totem).when([](const model& c) { return c.hotbar; }),
                setting::toggle("auto_open", {"Auto Open"}, &model::auto_open),
                setting::integer("stay_open_for", {"Stay Open For"}, &model::stay_open_for, modules::catalog::auto_totem_stay_open_for_minimum, modules::catalog::auto_totem_stay_open_for_maximum, "%d ticks").when([](const model& c) { return c.auto_open; }),
            };
            return fields;
        }
    };
}
