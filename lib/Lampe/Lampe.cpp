#include "Lampe.h"
#include <FastLED.h>
#include <Hardware.h>
#include <Programs.h>
#include <Telemetry.h>

void Lampe::begin() {
    pinMode(Hardware::ButtonPin, INPUT);
    engine_.reset(millis(), digitalRead(Hardware::ButtonPin) == HIGH);
    FastLED.addLeds<WS2812B, Hardware::LedDataPin, GRB>(engine_.leds, LampConfig::LedCount)
        .setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(LampConfig::Brightness);
    FastLED.show();
    frameAtUs_ = micros();
    telemetryFrames_ = sequence_ = 0;
}

void Lampe::update() {
    const uint32_t now = millis();
    engine_.pollButton(digitalRead(Hardware::ButtonPin) == HIGH, now);
    commands_.poll(Serial, engine_, now);
    const bool capture = commands_.audioEnabled(now) && Programs::usesAudio(engine_.program());
    if (!capture) microphone_.reset();
    if (Programs::usesAudio(engine_.program())) {
        const uint16_t reading = analogRead(Hardware::AudioPin);
        const uint32_t sampledAt = micros();
        engine_.sampleAudio(reading, now);
        if (capture) microphone_.sample(reading, engine_.audioOnset(), engine_.beatPulse(now) > 0, sampledAt);
    }

    const uint32_t frameTime = micros();
    if (LampLogic::intervalElapsed(frameTime, frameAtUs_, LampConfig::FrameIntervalUs)) {
        engine_.render(now);
        FastLED.show();

        // About 30 Hz. Never wait for the computer or for space in the TX queue.
        if (++telemetryFrames_ == 4) {
            telemetryFrames_ = 0;
            const uint8_t sequence = sequence_++;
            Telemetry::tryWrite(Serial, engine_, now, sequence, LampConfig::Brightness);
        }
    }
    // Reserve UART time before the next LED snapshot (60 bytes take ~5.2 ms).
    if (capture && !(telemetryFrames_ == 3 && uint32_t(micros() - frameAtUs_) > 1500))
        microphone_.tryWrite(Serial);
}

static_assert(Telemetry::PacketSize < SERIAL_TX_BUFFER_SIZE,
              "A full telemetry packet must fit in the UART's transmit buffer");
static_assert(Telemetry::Microphone::Size < SERIAL_TX_BUFFER_SIZE,
              "A microphone packet must fit in the UART's transmit buffer");
