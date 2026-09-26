#ifndef HARDWARE_H
#define HARDWARE_H

#include <Arduino.h>
#include <LampConfig.h>

namespace Hardware {
constexpr uint8_t LedDataPin = 3;
constexpr uint8_t ButtonPin = 2;
constexpr uint8_t AudioPin = A0;
constexpr uint32_t SerialBaud = 115200;
} // namespace Hardware

#endif
