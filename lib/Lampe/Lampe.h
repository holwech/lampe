#pragma once

#include <LampEngine.h>
#include <Commands.h>

// Arduino adapter: sample inputs, accept commands, drive LEDs, publish snapshots.
class Lampe {
public:
    void begin();
    void update();

private:
    LampEngine engine_;
    Commands::Reader commands_;
    uint32_t frameAtUs_ = 0;
    uint8_t telemetryFrames_ = 0;
    uint8_t sequence_ = 0;
};
