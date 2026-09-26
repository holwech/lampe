#pragma once

#include <FastLED.h>
#include <LampConfig.h>
#include <LampLogic.h>

// Shared by AVR firmware, host tests and WebAssembly. No GPIO or wall clock here.
class LampEngine {
public:
    void reset(uint32_t now, bool buttonHigh = false, uint16_t seed = LampConfig::RandomSeed);
    bool selectProgram(uint8_t program, uint32_t now);
    bool pollButton(bool high, uint32_t now);
    void sampleAudio(uint16_t reading, uint32_t now);
    void render(uint32_t now);
    uint8_t program() const { return program_; }
    uint8_t audioLevel() const { return audio_.level(); }

    CRGB leds[LampConfig::LedCount] = {};
    uint8_t hue = 0;
    struct EffectState {
        uint8_t position = 0;
        uint8_t firstHue = 0;
        uint8_t secondHue = 0;
        uint32_t stepAt = 0;
        uint32_t colorAt = 0;
        uint32_t sparkAt = 0;
    } effect;

private:
    void resetEffect(uint32_t now);
    LampLogic::DebouncedButton button_;
    LampLogic::AudioEnvelope audio_;
    uint8_t program_ = 0;
    uint16_t randomSeed_ = LampConfig::RandomSeed;
};
