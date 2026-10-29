#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct auto_totem
    {
        int mode = 0; // Argon-style Blatant / Random selection.
        int delay = 0;
        bool hotbar = true;
        int totem_slot = 1;
        bool auto_switch = false;
        bool force_totem = false;
        bool auto_open = false;
        int stay_open_for = 0;
        bool operator==(const auto_totem&) const = default;
        static std::span<const modules::settings::descriptor<auto_totem>> schema()
        {
            using model = auto_totem;
            using setting = modules::settings::descriptor<model>;
            static constexpr modules::settings::label modes[]{{"Blatant"}, {"Random"}};
            static const setting fields[]{
                setting::choice("mode", {"Mode"}, &model::mode, modes),
                setting::integer("delay", {"Delay"}, &model::delay, 0, 20, "%d ticks"),
                setting::toggle("hotbar", {"Hotbar"}, &model::hotbar),
                setting::integer("totem_slot", {"Totem Slot"}, &model::totem_slot, 1, 9, "Slot %d").when([](const model& c) { return c.hotbar || c.auto_switch; }),
                setting::toggle("auto_switch", {"Auto Switch"}, &model::auto_switch),
                setting::toggle("force_totem", {"Force Totem"}, &model::force_totem).when([](const model& c) { return c.hotbar; }),
                setting::toggle("auto_open", {"Auto Open"}, &model::auto_open),
                setting::integer("stay_open_for", {"Stay Open For"}, &model::stay_open_for, 0, 20, "%d ticks").when([](const model& c) { return c.auto_open; }),
            };
            return fields;
        }
    };
}
