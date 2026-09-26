#include "LampEngine.h"
#include <Programs.h>

void LampEngine::reset(uint32_t now, bool buttonHigh, uint16_t seed) {
    program_ = 0;
    hue = 0;
    randomSeed_ = seed;
    button_.reset(buttonHigh, now);
    resetEffect(now);
    fill_rainbow(leds, LampConfig::LedCount, hue, 5);
}

bool LampEngine::selectProgram(uint8_t program, uint32_t now) {
    if (program >= Programs::count()) return false;
    program_ = program;
    resetEffect(now);
    return true;
}

bool LampEngine::pollButton(bool high, uint32_t now) {
    if (!button_.released(high, now)) return false;
    return selectProgram(Programs::next(program_), now);
}

void LampEngine::sampleAudio(uint16_t reading, uint32_t now) {
    if (Programs::usesAudio(program_)) {
        audio_.sample(reading, now);
        beat_.sample(reading, now);
    }
}

void LampEngine::render(uint32_t now) {
    // Keep randomness per engine so independent simulations remain reproducible.
    random16_set_seed(randomSeed_);
    Programs::render(program_, *this, now);
    randomSeed_ = random16_get_seed();
}

void LampEngine::resetEffect(uint32_t now) {
    effect = EffectState{};
    effect.stepAt = effect.colorAt = effect.sparkAt = now;
    audio_.reset(now);
    beat_.reset(now);
}
