#pragma once

#include <LampEngine.h>
#include <Commands.h>
#include <Microphone.h>
#ifdef LAMP_SAMPLING_DIAGNOSTICS
#include <Sampling.h>
#endif
#include <FrameClock.h>

// Arduino adapter: sample inputs, accept commands, drive LEDs, publish snapshots.
class Lampe {
public:
    void begin();
    void update();

private:
    LampEngine engine_;
    Commands::Reader commands_;
#ifdef LAMP_SAMPLING_DIAGNOSTICS
    Telemetry::Sampling sampling_;
    uint8_t diagnosticLedLevel_ = 0;
#else
    Telemetry::Microphone microphone_;
#endif
    LampLogic::FrameClock frameClock_{LampConfig::FrameIntervalUs};
    uint8_t telemetryFrames_ = 0;
    uint8_t sequence_ = 0;
};
