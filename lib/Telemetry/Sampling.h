#pragma once
// Opt-in production-loop sampling audit. Replaces the microphone capture buffer
// only in diagnostic builds; never changes the detector's input or scheduling.
#include <Telemetry.h>
#ifndef LAMP_TIMING_DIAGNOSTICS
#error "Sampling diagnostics require LAMP_TIMING_DIAGNOSTICS"
#endif

namespace Telemetry {
class Sampling {
public:
    static constexpr uint8_t Size = 42;
    void sample(uint16_t adc, uint32_t us, bool busy) {
        if (!count_) first_ = us;
        if (last_) {
            const uint32_t gap = us - last_;
            const uint16_t bounded = gap > 65535 ? 65535 : gap;
            if (bounded < minimumGap_) minimumGap_ = bounded;
            if (bounded > maximumGap_) maximumGap_ = bounded;
        }
        last_ = us;
        ++count_;
        busy_ += busy;
        sum_ += adc;
        if (adc < minimum_) minimum_ = adc;
        if (adc > maximum_) maximum_ = adc;
    }
    void window(const LampEngine &engine, uint8_t ledLevel) {
        ++sequence_;
        if (ready_) {
            ++dropped_;
        } else {
            bytes_[0] = 'L'; bytes_[1] = 'S'; bytes_[2] = 1; bytes_[3] = Size;
            put16(4, sequence_);
            put32(6, engine.audioWindowTime());
            put32(10, first_); put32(14, last_);
            put16(18, count_); put16(20, minimumGap_); put16(22, maximumGap_);
            put16(24, minimum_); put16(26, maximum_);
            put16(28, engine.audioWindowPeak()); put32(30, sum_);
            bytes_[34] = engine.audioOnset(); bytes_[35] = engine.bpm();
            bytes_[36] = engine.beatConfidence(); bytes_[37] = busy_;
            put16(38, dropped_); bytes_[40] = ledLevel;
            // CRC is deferred until transmission, outside the ADC input budget.
            ready_ = true;
        }
        count_ = maximumGap_ = maximum_ = busy_ = sum_ = 0;
        minimumGap_ = 65535; minimum_ = 1023;
    }
    void tryWrite() {
        if (!ready_ || Serial.availableForWrite() < Size) return;
        bytes_[Size - 1] = checksum(bytes_, Size - 1);
        Serial.write(bytes_, Size);
        ready_ = false;
    }
private:
    void put16(uint8_t at, uint16_t value) {
        bytes_[at] = value; bytes_[at + 1] = value >> 8;
    }
    void put32(uint8_t at, uint32_t value) {
        for (uint8_t i = 0; i < 4; ++i) bytes_[at + i] = value >> (8 * i);
    }
    uint8_t bytes_[Size] = {};
    uint32_t first_ = 0, last_ = 0, sum_ = 0;
    uint16_t count_ = 0, minimumGap_ = 65535, maximumGap_ = 0;
    uint16_t minimum_ = 1023, maximum_ = 0, sequence_ = 0, dropped_ = 0;
    uint8_t busy_ = 0;
    bool ready_ = false;
};
} // namespace Telemetry
