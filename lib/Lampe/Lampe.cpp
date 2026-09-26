#include "Lampe.h"
#include <Programs.h>

void Lampe::begin() {
    pinMode(Hardware::ButtonPin, INPUT);
    const uint32_t now = millis();
    button_.reset(digitalRead(Hardware::ButtonPin) == HIGH, now);
    program_ = 0;
    hue = 0;
    resetEffect(now);

    FastLED.addLeds<WS2812B, Hardware::LedDataPin, GRB>(leds, Hardware::LedCount)
        .setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(Hardware::Brightness);
    // Retain the initial rainbow underneath the first block-color animation.
    fill_rainbow(leds, Hardware::LedCount, hue, 5);
    FastLED.show();
    frameAtUs_ = micros();
    printProgram();
}

void Lampe::update() {
    const uint32_t now = millis();
    if (button_.released(digitalRead(Hardware::ButtonPin) == HIGH, now)) {
        program_ = Programs::next(program_);
        resetEffect(now);
        printProgram();
    }
    if (Programs::usesAudio(program_)) {
        audio_.sample(analogRead(Hardware::AudioPin), now);
    }

    // Input and audio continue to run between frames; no delay or sampling loop.
    const uint32_t frameTime = micros();
    if (LampLogic::intervalElapsed(frameTime, frameAtUs_, Hardware::FrameIntervalUs)) {
        Programs::render(program_, *this, now);
        FastLED.show();
    }
}

void Lampe::resetEffect(uint32_t now) {
    effect = EffectState{};
    effect.stepAt = effect.colorAt = effect.sparkAt = now;
    audio_.reset(now);
}

void Lampe::printProgram() const {
    // Log transitions only, keeping serial writes out of the sampling path.
    Serial.print(F("Program: "));
    Serial.println(program_);
}
