// Compile the pinned library's real color algorithms for native tests and WASM.
// The Arduino drivers and unrelated FastLED modules are not needed here.
// These internal paths are intentionally reviewed when upgrading FastLED.
#include <hsv2rgb.cpp.hpp>
#include <lib8tion.cpp.hpp>
#include <crgb.cpp.hpp>
#include <fl/gfx/colorutils.cpp.hpp>
#include <fl/gfx/fill.cpp.hpp>
#include <fl/gfx/crgb_extra.cpp.hpp>
