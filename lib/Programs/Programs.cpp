#include "Programs.h"
#include <Lampe.h>

namespace Programs {
using LampLogic::intervalElapsed;
using LampLogic::wrapIndex;

void quarterBlink(Lampe &lampe, uint32_t now) {
    if (intervalElapsed(now, lampe.effect.stepAt, 200)) {
        const CHSV color(random8(255), 255, 255);
        for (uint8_t i = 0; i < Hardware::LedCount / 2; ++i) {
            lampe.leds[wrapIndex(lampe.effect.position, Hardware::LedCount, i)] = color;
        }
        lampe.effect.position = wrapIndex(lampe.effect.position, Hardware::LedCount,
                                          random8(Hardware::LedCount));
    }
}

void flow(Lampe &lampe, uint32_t now) {
    constexpr uint8_t Half = Hardware::LedCount / 2;
    for (uint8_t i = 0; i < Half; ++i) {
        const CHSV color(lampe.hue + 8 * i, 255, 255);
        lampe.leds[Half - i - 1] = color;
        lampe.leds[Half + i] = color;
    }
    if (intervalElapsed(now, lampe.effect.stepAt, 20)) ++lampe.hue;
}

void amplitude(Lampe &lampe, uint32_t) {
    const uint8_t red = LampLogic::amplifiedBrightness(lampe.audioLevel());
    fill_solid(lampe.leds, Hardware::LedCount, CRGB(red, 0, 0));
}

void ambulance(Lampe &lampe, uint32_t now) {
    if (intervalElapsed(now, lampe.effect.stepAt, 30)) {
        lampe.leds[lampe.effect.position] = CRGB(255, 0, 0);
        lampe.leds[wrapIndex(lampe.effect.position, Hardware::LedCount, Hardware::LedCount / 2)]
            = CRGB(0, 0, 255);
        lampe.effect.position = wrapIndex(lampe.effect.position, Hardware::LedCount);
    }
}

void ambulanceHue(Lampe &lampe, uint32_t now) {
    if (intervalElapsed(now, lampe.effect.colorAt, 2000)) {
        lampe.effect.firstHue = random8(255);
        lampe.effect.secondHue = random8(255);
    }
    if (intervalElapsed(now, lampe.effect.stepAt, 50)) {
        lampe.leds[lampe.effect.position] = CHSV(lampe.effect.firstHue, 255, 255);
        lampe.leds[wrapIndex(lampe.effect.position, Hardware::LedCount, Hardware::LedCount / 2)]
            = CHSV(lampe.effect.secondHue, 255, 255);
        lampe.effect.position = wrapIndex(lampe.effect.position, Hardware::LedCount);
    }
}

void fireplace(Lampe &lampe, uint32_t now) {
    fadeToBlackBy(lampe.leds, Hardware::LedCount, 1);
    if (intervalElapsed(now, lampe.effect.stepAt, 401)) {
        lampe.leds[random16(Hardware::LedCount)] += CHSV(-10 + random8(60), 255, 255);
    }
    if (intervalElapsed(now, lampe.effect.colorAt, 300)) {
        lampe.leds[random16(Hardware::LedCount)] += CHSV(-10 + random8(60), 255, 255);
    }
    if (intervalElapsed(now, lampe.effect.sparkAt, 152)) {
        lampe.leds[random16(Hardware::LedCount)] += CHSV(-10 + random8(60), 255, 255);
    }
}

void northernLights(Lampe &lampe, uint32_t now) {
    fadeToBlackBy(lampe.leds, Hardware::LedCount, 2);
    if (intervalElapsed(now, lampe.effect.stepAt, 30)) {
        lampe.leds[random16(Hardware::LedCount)] += CHSV(lampe.hue + random8(64), 200, 150);
    }
    if (intervalElapsed(now, lampe.effect.colorAt, 300)) ++lampe.hue;
}

void rainbow(Lampe &lampe, uint32_t now) {
    fill_rainbow(lampe.leds, Hardware::LedCount, lampe.hue, 5);
    if (intervalElapsed(now, lampe.effect.stepAt, 40)) ++lampe.hue;
}

static_assert(Hardware::LedCount > 0 && Hardware::LedCount % 2 == 0,
              "The mirrored effects require an even, nonzero LED count");
} // namespace Programs
