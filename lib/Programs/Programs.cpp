#include "Programs.h"
#include <FastLED.h>
#include <LampEngine.h>

namespace Programs {
using LampLogic::intervalElapsed;
using LampLogic::wrapIndex;

void quarterBlink(LampEngine &lampe, uint32_t now) {
    if (intervalElapsed(now, lampe.effect.stepAt, 200)) {
        const CHSV color(random8(255), 255, 255);
        for (uint8_t i = 0; i < LampConfig::LedCount / 2; ++i) {
            lampe.leds[wrapIndex(lampe.effect.position, LampConfig::LedCount, i)] = color;
        }
        lampe.effect.position = wrapIndex(lampe.effect.position, LampConfig::LedCount,
                                          random8(LampConfig::LedCount));
    }
}

void flow(LampEngine &lampe, uint32_t now) {
    constexpr uint8_t Half = LampConfig::LedCount / 2;
    for (uint8_t i = 0; i < Half; ++i) {
        const CHSV color(lampe.hue + 8 * i, 255, 255);
        lampe.leds[Half - i - 1] = color;
        lampe.leds[Half + i] = color;
    }
    if (intervalElapsed(now, lampe.effect.stepAt, 20)) ++lampe.hue;
}

void amplitude(LampEngine &lampe, uint32_t) {
    const uint8_t red = LampLogic::amplifiedBrightness(lampe.audioLevel());
    fill_solid(lampe.leds, LampConfig::LedCount, CRGB(red, 0, 0));
}

void ambulance(LampEngine &lampe, uint32_t now) {
    if (intervalElapsed(now, lampe.effect.stepAt, 30)) {
        lampe.leds[lampe.effect.position] = CRGB(255, 0, 0);
        lampe.leds[wrapIndex(lampe.effect.position, LampConfig::LedCount, LampConfig::LedCount / 2)]
            = CRGB(0, 0, 255);
        lampe.effect.position = wrapIndex(lampe.effect.position, LampConfig::LedCount);
    }
}

void ambulanceHue(LampEngine &lampe, uint32_t now) {
    if (intervalElapsed(now, lampe.effect.colorAt, 2000)) {
        lampe.effect.firstHue = random8(255);
        lampe.effect.secondHue = random8(255);
    }
    if (intervalElapsed(now, lampe.effect.stepAt, 50)) {
        lampe.leds[lampe.effect.position] = CHSV(lampe.effect.firstHue, 255, 255);
        lampe.leds[wrapIndex(lampe.effect.position, LampConfig::LedCount, LampConfig::LedCount / 2)]
            = CHSV(lampe.effect.secondHue, 255, 255);
        lampe.effect.position = wrapIndex(lampe.effect.position, LampConfig::LedCount);
    }
}

void fireplace(LampEngine &lampe, uint32_t now) {
    fadeToBlackBy(lampe.leds, LampConfig::LedCount, 1);
    if (intervalElapsed(now, lampe.effect.stepAt, 401)) {
        lampe.leds[random16(LampConfig::LedCount)] += CHSV(-10 + random8(60), 255, 255);
    }
    if (intervalElapsed(now, lampe.effect.colorAt, 300)) {
        lampe.leds[random16(LampConfig::LedCount)] += CHSV(-10 + random8(60), 255, 255);
    }
    if (intervalElapsed(now, lampe.effect.sparkAt, 152)) {
        lampe.leds[random16(LampConfig::LedCount)] += CHSV(-10 + random8(60), 255, 255);
    }
}

void northernLights(LampEngine &lampe, uint32_t now) {
    fadeToBlackBy(lampe.leds, LampConfig::LedCount, 2);
    if (intervalElapsed(now, lampe.effect.stepAt, 30)) {
        lampe.leds[random16(LampConfig::LedCount)] += CHSV(lampe.hue + random8(64), 200, 150);
    }
    if (intervalElapsed(now, lampe.effect.colorAt, 300)) ++lampe.hue;
}

void rainbow(LampEngine &lampe, uint32_t now) {
    fill_rainbow(lampe.leds, LampConfig::LedCount, lampe.hue, 5);
    if (intervalElapsed(now, lampe.effect.stepAt, 40)) ++lampe.hue;
}

static_assert(LampConfig::LedCount > 0 && LampConfig::LedCount % 2 == 0,
              "The mirrored effects require an even, nonzero LED count");
} // namespace Programs
