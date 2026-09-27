#pragma once
// Opt-in measurement build only. No buffers, counters or writes in production.
#include <Arduino.h>
#include <Telemetry.h>

extern "C" {
extern char __heap_start;
extern char *__brkval;
}

namespace Telemetry {
class Timing {
public:
    void begin() {
        // Mark unused SRAM after setup; later stack writes reveal the high-water
        // mark, including nested work and interrupt handlers. Leave current stack.
        volatile uint8_t *p = heapEnd();
        const uint8_t *end = reinterpret_cast<uint8_t *>(SP - 32);
        while (p < end) *p++ = 0xa5;
        reportedAt_ = millis();
    }
    void frame(uint32_t now) {
        if (lastFrame_) {
            const uint32_t interval = now - lastFrame_;
            if (interval < minimum_) minimum_ = interval;
            if (interval > maximum_) maximum_ = interval > 65535 ? 65535 : interval;
            late_ += interval > LampConfig::FrameIntervalUs + 500;
        }
        lastFrame_ = now;
        ++frames_;
    }
    void input(uint32_t elapsed, bool analysisBusy) {
        if (elapsed > input_) input_ = elapsed;
        busyWindows_ += analysisBusy;
    }
    void work(uint32_t elapsed) {
        if (elapsed > work_) work_ = elapsed;
        ++calls_;
    }
    void tryWrite() {
        constexpr uint8_t Size = 27;
        const uint32_t now = millis();
        if (now - reportedAt_ < 1000 || Serial.availableForWrite() < Size) return;
        uint8_t bytes[Size] = {'L', 'T', 1, Size};
        for (uint8_t i = 0; i < 4; ++i) bytes[4 + i] = now >> (8 * i);
        const uint16_t fields[] = {frames_, minimum_, maximum_, work_, input_, late_, busyWindows_, freeStack(), calls_};
        for (uint8_t i = 0; i < 9; ++i) {
            bytes[8 + 2 * i] = fields[i]; bytes[9 + 2 * i] = fields[i] >> 8;
        }
        bytes[Size - 1] = checksum(bytes, Size - 1);
        Serial.write(bytes, Size);
        reportedAt_ = now;
        frames_ = maximum_ = work_ = input_ = late_ = busyWindows_ = calls_ = 0;
        minimum_ = 65535;
    }
private:
    static uint8_t *heapEnd() {
        return reinterpret_cast<uint8_t *>(__brkval ? __brkval : &__heap_start);
    }
    static uint16_t freeStack() {
        const volatile uint8_t *p = heapEnd();
        const uint8_t *end = reinterpret_cast<uint8_t *>(SP);
        while (p < end && *p == 0xa5) ++p;
        return p - heapEnd();
    }
    uint32_t lastFrame_ = 0, reportedAt_ = 0;
    uint16_t frames_ = 0, minimum_ = 65535, maximum_ = 0, work_ = 0, input_ = 0;
    uint16_t late_ = 0, busyWindows_ = 0, calls_ = 0;
};
} // namespace Telemetry
