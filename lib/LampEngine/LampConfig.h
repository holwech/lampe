#pragma once
#include <stdint.h>

namespace LampConfig {
constexpr uint8_t LedCount = 16;
constexpr uint8_t Brightness = 100;
constexpr uint32_t FrameIntervalUs = 1000000UL / 120;
constexpr uint16_t RandomSeed = 1337;
} // namespace LampConfig
