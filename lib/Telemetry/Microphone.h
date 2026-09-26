#pragma once
#include <Telemetry.h>

namespace Telemetry {
// LA v1: header (9), ten timestamped samples (5 each), CRC (1).
// Subsample existing ADC reads at <=1 kHz; never delay the lamp for capture.
class Microphone {
public:
    static constexpr uint8_t SampleCount = 10, Size = 60;
    void reset() { used_ = 0; sampled_ = false; }
    void sample(uint16_t raw, uint8_t onset, bool beat, uint32_t us) {
        if (sampled_ && uint32_t(us - lastUs_) < 1000) return;
        sampled_ = true;
        lastUs_ = us;
        if (used_ && uint32_t(us - startUs_) > 65535) used_ = 0;
        if (!used_) startUs_ = us;
        if (used_ == SampleCount) return; // At most one bounded packet in flight.
        const uint8_t index = 9 + used_ * 5;
        const uint16_t offset = us - startUs_;
        const uint16_t value = (raw & 1023) | (beat ? 0x8000 : 0);
        bytes_[index] = offset; bytes_[index + 1] = offset >> 8;
        bytes_[index + 2] = value; bytes_[index + 3] = value >> 8;
        bytes_[index + 4] = onset;
        ++used_;
    }
    template <typename SerialPort>
    bool tryWrite(SerialPort &serial) {
        if (used_ != SampleCount) return false;
        used_ = 0;
        const uint8_t sequence = sequence_++;
        // Drop a full batch if the queue is busy; no retry or sampling backlog.
        if (serial.availableForWrite() < Size) return false;
        bytes_[0] = 'L'; bytes_[1] = 'A'; bytes_[2] = 1;
        bytes_[3] = SampleCount; bytes_[4] = sequence;
        for (uint8_t i = 0; i < 4; ++i) bytes_[5 + i] = startUs_ >> (8 * i);
        bytes_[Size - 1] = checksum(bytes_, Size - 1);
        serial.write(bytes_, Size);
        return true;
    }
private:
    uint8_t bytes_[Size] = {}, used_ = 0, sequence_ = 0;
    uint32_t startUs_ = 0, lastUs_ = 0;
    bool sampled_ = false;
};
} // namespace Telemetry
