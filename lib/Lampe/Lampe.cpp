#include "Lampe.h"
#include <FastLED.h>
#include <Hardware.h>
#include <Programs.h>
#include <Telemetry.h>
#ifdef LAMP_TIMING_DIAGNOSTICS
#include <Timing.h>
static Telemetry::Timing timing;
#endif

void Lampe::begin() {
    pinMode(Hardware::ButtonPin, INPUT);
    engine_.reset(millis(), digitalRead(Hardware::ButtonPin) == HIGH);
    FastLED.addLeds<WS2812B, Hardware::LedDataPin, GRB>(engine_.leds, LampConfig::LedCount)
        .setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(LampConfig::Brightness);
    FastLED.show();
    frameClock_.reset(micros());
    telemetryFrames_ = sequence_ = 0;
#if defined(LAMP_TIMING_STRESS) || defined(LAMP_SAMPLING_DIAGNOSTICS)
    engine_.selectProgram(2, millis());
#endif
#ifdef LAMP_TIMING_DIAGNOSTICS
    timing.begin();
#endif
}

void Lampe::update() {
    // LED deadlines have priority over ADC reads, commands and analysis.
    // Bounded analysis work is only started with at least 1 ms to spare.
    const uint32_t frameTime = micros();
    const uint32_t now = millis();
    if (frameClock_.due(frameTime)) {
        engine_.render(now);
#ifdef LAMP_SAMPLING_DIAGNOSTICS
        // Open-loop comparison, independent of the detected beat. Both builds
        // run the same renderer, tracker and 120 Hz LED writes.
#ifdef LAMP_SAMPLING_PULSED
        diagnosticLedLevel_ = now % 500 < 90 ? 255 : 0;
#else
        diagnosticLedLevel_ = 64;
#endif
        fill_solid(engine_.leds, LampConfig::LedCount, CRGB(diagnosticLedLevel_, 0, 0));
#endif
#ifdef LAMP_MICROPHONE_STEADY_LEDS
        // Diagnostic comparison: keep sampling/rendering cadence unchanged,
        // but remove changing LED current as a possible microphone input.
        fill_solid(engine_.leds, LampConfig::LedCount, CRGB(64, 0, 0));
#endif
#ifdef LAMP_TIMING_DIAGNOSTICS
        timing.frame(micros());
#endif
        FastLED.show();
        if (++telemetryFrames_ == 4) {
            telemetryFrames_ = 0;
            const uint8_t sequence = sequence_++;
            Telemetry::tryWrite(Serial, engine_, now, sequence, LampConfig::Brightness);
        }
    }
    if (frameClock_.spare(micros()) < 750) return;
    engine_.pollButton(digitalRead(Hardware::ButtonPin) == HIGH, now);
    commands_.poll(Serial, engine_, now);
#ifndef LAMP_SAMPLING_DIAGNOSTICS
    const uint8_t capture = Programs::usesAudio(engine_.program()) ? commands_.audioMode(now) : 0;
    if (!capture) microphone_.reset();
#endif
    if (Programs::usesAudio(engine_.program())) {
#ifdef LAMP_TIMING_DIAGNOSTICS
        const uint32_t inputAt = micros();
        const bool pendingBefore = engine_.audioWorkPending();
        const uint32_t windowBefore = engine_.audioWindowTime();
#endif
        uint16_t reading = analogRead(Hardware::AudioPin);
#ifdef LAMP_TIMING_STRESS
        // Exercise analysis without requiring music. Includes loud attacks and
        // fluctuating sound between them; actual ADC conversion still runs.
        static uint16_t random = 42;
        random = random * 2053U + 13849U;
        const uint16_t phase = millis() % 500;
        const uint16_t amplitude = 30 + (random & 63) + (phase < 70 ? (70 - phase) * 3 : 0);
        reading = (random & 256) ? 512 + amplitude : 512 - amplitude;
#endif
        const uint32_t sampledAt = micros();
#ifdef LAMP_SAMPLING_DIAGNOSTICS
        // Conversion-end timestamps, including gaps across window boundaries.
        sampling_.sample(reading, sampledAt, pendingBefore);
#endif
        engine_.sampleAudio(reading, millis(), true);
#ifdef LAMP_SAMPLING_DIAGNOSTICS
        if (windowBefore != engine_.audioWindowTime())
            sampling_.window(engine_, diagnosticLedLevel_);
#endif
#ifdef LAMP_TIMING_DIAGNOSTICS
        timing.input(micros() - inputAt, pendingBefore && windowBefore != engine_.audioWindowTime());
#endif
#ifndef LAMP_SAMPLING_DIAGNOSTICS
        if (capture == 1) microphone_.sample(reading, engine_.audioOnset(), engine_.beatPulse(now) > 0, sampledAt);
        if (capture == 2) microphone_.window(engine_.audioWindowPeak(), engine_.audioOnset(), engine_.audioWindowTime());
#endif
    }

    if (frameClock_.spare(micros()) >= 1000 && engine_.audioWorkPending()) {
#ifdef LAMP_TIMING_DIAGNOSTICS
        const uint32_t workAt = micros();
#endif
        engine_.workAudio(millis());
#ifdef LAMP_TIMING_DIAGNOSTICS
        timing.work(micros() - workAt);
#endif
    }
#ifdef LAMP_SAMPLING_DIAGNOSTICS
    if (frameClock_.spare(micros()) >= 1000) sampling_.tryWrite();
#else
    // Leave room in the transmit queue before the next LED snapshot.
    if (capture && frameClock_.spare(micros()) >= 500 &&
        (capture == 2 || telemetryFrames_ != 3 || frameClock_.spare(micros()) > 6800))
        microphone_.tryWrite(Serial);
#endif
#ifdef LAMP_TIMING_DIAGNOSTICS
    if (frameClock_.spare(micros()) >= 1000) timing.tryWrite();
#endif
}

static_assert(Telemetry::PacketSize < SERIAL_TX_BUFFER_SIZE,
              "A full telemetry packet must fit in the UART's transmit buffer");
static_assert(Telemetry::Microphone::Size < SERIAL_TX_BUFFER_SIZE,
              "A microphone packet must fit in the UART's transmit buffer");
#ifdef LAMP_SAMPLING_DIAGNOSTICS
static_assert(Telemetry::Sampling::Size < SERIAL_TX_BUFFER_SIZE,
              "A sampling audit packet must fit in the UART's transmit buffer");
#endif
