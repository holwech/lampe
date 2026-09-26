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
    if (Programs::usesAudio(engine_.program())) {
        engine_.sampleAudio(analogRead(Hardware::AudioPin), now);
    }

    const uint32_t frameTime = micros();
    if (!LampLogic::intervalElapsed(frameTime, frameAtUs_, LampConfig::FrameIntervalUs)) return;
    engine_.render(now);
    FastLED.show();

    // About 30 Hz. Never wait for the computer or for space in the TX queue.
    if (++telemetryFrames_ == 4) {
        telemetryFrames_ = 0;
        const uint8_t sequence = sequence_++;
        Telemetry::tryWrite(Serial, engine_, now, sequence, LampConfig::Brightness);
    }
}

static_assert(Telemetry::PacketSize < SERIAL_TX_BUFFER_SIZE,
              "A full telemetry packet must fit in the UART's transmit buffer");
