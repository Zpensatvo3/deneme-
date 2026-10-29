#pragma once

// Gameplay option renderers still use the ImGui spelling at their call sites.
// Keep the compatibility mapping isolated from module declarations so a module
// header no longer pulls the complete GUI and every other module into a TU.
#include "../gui/widgets.hpp"

#ifndef ONESTEP_UI_NO_REDIRECT
#define Checkbox OneStepCheckbox
#define SliderFloat OneStepSliderFloat
#define SliderInt OneStepSliderInt
#define Combo OneStepCombo
#define ColorEdit4 OneStepColorEdit4
#define SeparatorText OneStepSeparatorText
#endif
