#pragma once
#include <stdint.h>

namespace LampLogic {
// Keep a fixed cadence instead of adding each loop's lateness to every frame.
class FrameClock {
public:
    explicit FrameClock(uint32_t interval) : interval_(interval) {}
    void reset(uint32_t now) { previous_ = now; }
    uint32_t spare(uint32_t now) const {
        const uint32_t elapsed = now - previous_;
        return elapsed >= interval_ ? 0 : interval_ - elapsed;
    }
    bool due(uint32_t now) {
        const uint32_t elapsed = now - previous_;
        if (elapsed < interval_) return false;
        // After a suspension, render once rather than a burst of stale frames.
        previous_ = elapsed < 2 * interval_ ? previous_ + interval_ : now;
        return true;
    }
private:
    uint32_t interval_, previous_ = 0;
};
} // namespace LampLogic
