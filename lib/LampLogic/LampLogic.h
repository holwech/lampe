#ifndef LAMP_LOGIC_H
#define LAMP_LOGIC_H

#include <stdint.h>

namespace LampLogic {

constexpr uint16_t AdcMax = 1023;
constexpr uint32_t AudioWindowMs = 50;
constexpr uint32_t DecayStepMs = 5;
constexpr uint32_t DebounceMs = 20;

// Widen before multiplying: int is only 16 bits on the ATmega328P.
constexpr uint8_t amplitudeLevel(uint16_t peakToPeak) {
    return peakToPeak >= AdcMax
        ? 255
        : static_cast<uint8_t>(static_cast<uint32_t>(peakToPeak) * 255 / AdcMax);
}

constexpr uint8_t amplifiedBrightness(uint8_t level) {
    return static_cast<uint16_t>(level) * 5 / 2 > 255
        ? 255
        : static_cast<uint8_t>(static_cast<uint16_t>(level) * 5 / 2);
}

constexpr uint8_t wrapIndex(uint8_t index, uint8_t count, uint8_t step = 1) {
    return count == 0 ? 0 : static_cast<uint8_t>((static_cast<uint16_t>(index) + step) % count);
}

inline bool intervalElapsed(uint32_t now, uint32_t &last, uint32_t interval) {
    if (static_cast<uint32_t>(now - last) < interval) {
        return false;
    }
    last = now;
    return true;
}

// The existing sensor is HIGH while pressed; advance on a stable release.
// No internal pull-up is assumed: the hardware supplies the input level.
class DebouncedButton {
public:
    void reset(bool high, uint32_t now) {
        candidate_ = stable_ = high;
        changedAt_ = now;
    }

    bool released(bool high, uint32_t now) {
        if (high != candidate_) {
            candidate_ = high;
            changedAt_ = now;
        }
        if (candidate_ != stable_ && static_cast<uint32_t>(now - changedAt_) >= DebounceMs) {
            stable_ = candidate_;
            return !stable_;
        }
        return false;
    }

private:
    bool candidate_ = false;
    bool stable_ = false;
    uint32_t changedAt_ = 0;
};

// One ADC reading per loop, summarized into 50 ms peak-to-peak windows.
class AudioEnvelope {
public:
    void reset(uint32_t now) {
        windowAt_ = decayAt_ = now;
        level_ = 0;
        clearWindow();
    }

    void sample(uint16_t reading, uint32_t now) {
        decay(now);
        if (reading <= AdcMax) {
            if (reading < minimum_) minimum_ = reading;
            if (reading > maximum_) maximum_ = reading;
            hasSample_ = true;
        }
        if (static_cast<uint32_t>(now - windowAt_) >= AudioWindowMs) {
            const uint8_t peak = hasSample_ ? amplitudeLevel(maximum_ - minimum_) : 0;
            if (peak > level_) {
                level_ = peak;
                decayAt_ = now;
            }
            windowAt_ = now;
            clearWindow();
        }
    }

    uint8_t level() const { return level_; }

private:
    void clearWindow() {
        minimum_ = AdcMax;
        maximum_ = 0;
        hasSample_ = false;
    }

    void decay(uint32_t now) {
        const uint32_t steps = static_cast<uint32_t>(now - decayAt_) / DecayStepMs;
        if (steps != 0) {
            level_ = steps >= level_ ? 0 : static_cast<uint8_t>(level_ - steps);
            decayAt_ += steps * DecayStepMs;
        }
    }

    uint16_t minimum_ = AdcMax;
    uint16_t maximum_ = 0;
    bool hasSample_ = false;
    uint8_t level_ = 0;
    uint32_t windowAt_ = 0;
    uint32_t decayAt_ = 0;
};

static_assert(amplitudeLevel(1023) == 255, "Full-scale audio must survive AVR arithmetic");
static_assert(amplitudeLevel(512) == 127, "Scale intermediate values with 32-bit arithmetic");
static_assert(amplifiedBrightness(255) == 255, "LED brightness must saturate");

} // namespace LampLogic

#endif
