#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct fast_exp
    {
        int min_cps = 16;
        int max_cps = 20;
        int legacy_cps = 0;
        bool operator==(const fast_exp&) const = default;
        static std::span<const modules::settings::descriptor<fast_exp>> schema()
        {
            using model = fast_exp;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::integer("min_cps", {"Minimum CPS"}, &model::min_cps, 1, 50, "%d CPS").display_minimum(5).display_maximum(40),
                setting::integer("max_cps", {"Maximum CPS"}, &model::max_cps, 1, 50, "%d CPS").display_minimum(5).display_maximum(40),
                setting::integer("cps", {"Legacy CPS"}, &model::legacy_cps, -1000000, 1000000).hidden(),
            };
            return fields;
        }
        static bool visit(modules::config_visitor& v, fast_exp& live)
        {
            auto c = live;
            const bool importing = v.importing();
            if (importing) v.stage_values(true);
            modules::settings::visit_fields(v, c, schema());
            if (!importing) return false;
            if (c.legacy_cps > 0 && c.min_cps == 16 && c.max_cps == 20) {
                c.min_cps = (std::max)(5, c.legacy_cps - 2);
                c.max_cps = (std::min)(40, c.legacy_cps + 2);
            }
            c.legacy_cps = 0;
            if (!modules::settings::normalize(c, schema())) v.reject("Invalid EXP settings.");
            v.stage_values(false);
            if (!v.reading() || !v.valid()) return false;
            live = c; return true;
        }
    };
}
