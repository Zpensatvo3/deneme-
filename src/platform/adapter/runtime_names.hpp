#pragma once
#include "target_version.hpp"

// Only explicit binding sites use this macro. Generic JNI/Java/library names
// and the entire 1.21.x resolver remain unchanged.
#if ONESTEP_MINECRAFT_26_1
#include "../../sdk/minecraft/fabric_26_1/official_names.hpp"
#define ONESTEP_MC_NAME(source) ::mc261::names::symbol<source>::value
#else
#define ONESTEP_MC_NAME(source) source
#endif
