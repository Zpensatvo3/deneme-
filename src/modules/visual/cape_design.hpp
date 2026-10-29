#pragma once
#include <algorithm>
#include <cmath>
#include <span>

namespace cape_design
{
    struct uv { float x,y; };
    struct line { uv a,b; };
    inline std::span<const line> strokes(int style)
    {
        static constexpr line emblem[]{
            {{.5f,.19f},{.73f,.39f}},{{.73f,.39f},{.5f,.65f}},
            {{.5f,.65f},{.27f,.39f}},{{.27f,.39f},{.5f,.19f}},
            {{.38f,.38f},{.5f,.51f}},{{.5f,.51f},{.62f,.38f}}
        };
        static constexpr line split[]{
            {{.70f,.06f},{.30f,.94f}},{{.75f,.06f},{.35f,.94f}},
            {{.25f,.20f},{.37f,.20f}},{{.25f,.20f},{.25f,.32f}},
            {{.63f,.78f},{.75f,.78f}},{{.75f,.66f},{.75f,.78f}}
        };
        static constexpr line constellation[]{
            {{.24f,.30f},{.43f,.21f}},{{.43f,.21f},{.68f,.34f}},
            {{.68f,.34f},{.55f,.52f}},{{.55f,.52f},{.72f,.73f}},
            {{.55f,.52f},{.31f,.70f}},{{.31f,.70f},{.24f,.30f}},
            {{.20f,.30f},{.28f,.30f}},{{.24f,.26f},{.24f,.34f}},
            {{.39f,.21f},{.47f,.21f}},{{.43f,.17f},{.43f,.25f}},
            {{.64f,.34f},{.72f,.34f}},{{.68f,.30f},{.68f,.38f}},
            {{.51f,.52f},{.59f,.52f}},{{.55f,.48f},{.55f,.56f}},
            {{.68f,.73f},{.76f,.73f}},{{.72f,.69f},{.72f,.77f}},
            {{.27f,.70f},{.35f,.70f}},{{.31f,.66f},{.31f,.74f}}
        };
        static constexpr line circuit[]{
            {{.50f,.16f},{.68f,.35f}},{{.68f,.35f},{.50f,.54f}},
            {{.50f,.54f},{.32f,.35f}},{{.32f,.35f},{.50f,.16f}},
            {{.50f,.16f},{.50f,.07f}},{{.68f,.35f},{.85f,.35f}},
            {{.32f,.35f},{.15f,.35f}},{{.50f,.54f},{.50f,.92f}},
            {{.40f,.50f},{.28f,.62f}},{{.28f,.62f},{.28f,.85f}},
            {{.60f,.50f},{.72f,.62f}},{{.72f,.62f},{.72f,.85f}},
            {{.18f,.08f},{.18f,.20f}},{{.18f,.20f},{.28f,.28f}},
            {{.82f,.08f},{.82f,.20f}},{{.82f,.20f},{.72f,.28f}},
            {{.40f,.35f},{.50f,.45f}},{{.50f,.45f},{.60f,.35f}},
            {{.23f,.85f},{.33f,.85f}},{{.67f,.85f},{.77f,.85f}}
        };
        static constexpr line wings[]{
            {{.48f,.45f},{.23f,.22f}},{{.23f,.22f},{.13f,.37f}},
            {{.13f,.37f},{.22f,.64f}},{{.22f,.64f},{.45f,.73f}},
            {{.52f,.45f},{.77f,.22f}},{{.77f,.22f},{.87f,.37f}},
            {{.87f,.37f},{.78f,.64f}},{{.78f,.64f},{.55f,.73f}},
            {{.23f,.28f},{.43f,.51f}},{{.19f,.36f},{.43f,.56f}},
            {{.19f,.44f},{.43f,.61f}},{{.22f,.53f},{.44f,.66f}},
            {{.77f,.28f},{.57f,.51f}},{{.81f,.36f},{.57f,.56f}},
            {{.81f,.44f},{.57f,.61f}},{{.78f,.53f},{.56f,.66f}},
            {{.50f,.39f},{.50f,.79f}},{{.45f,.79f},{.55f,.79f}}
        };
        switch (style) {
        case 2: return emblem;
        case 3: return split;
        case 5: return constellation;
        case 6: return circuit;
        case 7: return wings;
        default: return {};
        }
    }
    struct fabric { float shade=1, ember=0; };
    inline fabric sample(int style,double u,double v)
    {
        switch (style) {
        case 3: return {u < .73-.4*v ? .22f : .9f,0};
        case 4: {
            // Three asymmetric flame tongues; fixed to the cloth UVs, not
            // screen coordinates. Cloth animation provides their movement.
            const double tips=.45+.22*std::sin(u*19.)+.12*std::cos(u*31.);
            const float heat=static_cast<float>(std::clamp((v-tips)*3.2+.20,0.,1.));
            return {.24f+heat*.65f,heat};
        }
        case 5: return {.18f+static_cast<float>(v)*.12f,0};
        case 6: return {.20f,0};
        case 7: return {.30f,0};
        default: return {};
        }
    }
}
