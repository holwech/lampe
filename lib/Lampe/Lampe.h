#ifndef LAMPE_H
#define LAMPE_H

#include <FastLED.h>
#include <Hardware.h>
#include <LampLogic.h>

class Lampe {
public:
    // Hardware initialization belongs in setup(), after Arduino initializes timers.
    void begin();
    void update();
    uint8_t audioLevel() const { return audio_.level(); }

    CRGB leds[Hardware::LedCount] = {};
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
    void printProgram() const;

    LampLogic::DebouncedButton button_;
    LampLogic::AudioEnvelope audio_;
    uint8_t program_ = 0;
    uint32_t frameAtUs_ = 0;
};

#endif
