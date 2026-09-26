#pragma once

#include <LampEngine.h>

// Arduino adapter: sample inputs, drive LEDs, and publish best-effort snapshots.
class Lampe {
public:
    void begin();
    void update();

private:
    LampEngine engine_;
    uint32_t frameAtUs_ = 0;
    uint8_t telemetryFrames_ = 0;
    uint8_t sequence_ = 0;
};
