#pragma once

#include <LampEngine.h>
#include <stddef.h>

namespace Telemetry {
constexpr uint8_t Version = 2;
constexpr size_t PacketSize = 15 + 3 * LampConfig::LedCount;

inline uint8_t checksum(const uint8_t *data, size_t size) {
    uint8_t crc = 0;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07)
                               : static_cast<uint8_t>(crc << 1);
        }
    }
    return crc;
}

inline void encode(uint8_t *out, const LampEngine &lamp, uint32_t now,
                   uint8_t sequence, uint8_t brightness) {
    out[0] = 'L'; out[1] = 'M'; out[2] = Version;
    out[3] = LampConfig::LedCount;
    out[4] = lamp.program(); out[5] = brightness;
    out[6] = lamp.audioLevel(); out[7] = sequence;
    for (uint8_t i = 0; i < 4; ++i) out[8 + i] = static_cast<uint8_t>(now >> (8 * i));
    for (uint8_t i = 0; i < LampConfig::LedCount; ++i) {
        out[12 + 3 * i] = lamp.leds[i].r;
        out[13 + 3 * i] = lamp.leds[i].g;
        out[14 + 3 * i] = lamp.leds[i].b;
    }
    out[60] = lamp.bpm();
    out[61] = lamp.beatConfidence();
    out[PacketSize - 1] = checksum(out, PacketSize - 1);
}
// Checking capacity before the only serial writer runs keeps Serial.write nonblocking.
template <typename SerialPort>
bool tryWrite(SerialPort &serial, const LampEngine &lamp, uint32_t now,
              uint8_t sequence, uint8_t brightness) {
    if (serial.availableForWrite() < static_cast<int>(PacketSize)) return false;
    uint8_t packet[PacketSize];
    encode(packet, lamp, now, sequence, brightness);
    serial.write(packet, sizeof(packet));
    return true;
}
} // namespace Telemetry
