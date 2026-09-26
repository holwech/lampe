#include <Lampe.h>
#include <Programs.h>

#include <cstdlib>
#include <iostream>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << __FILE__ << ':' << __LINE__ << ": " #condition "\n"; \
        std::exit(1); \
    } \
} while (false)

void testColors() {
    CHECK(CRGB(CHSV(0, 255, 255)) == CRGB(255, 0, 0));
    CHECK(CRGB(CHSV(96, 255, 255)) == CRGB(0, 255, 0));
    CHECK(CRGB(CHSV(160, 255, 255)) == CRGB(0, 0, 255));
    // FastLED's modern saturation curve differs from the old 3.1.8 version.
    CHECK(CRGB(CHSV(0, 240, 255)) == CRGB(255, 1, 1));
    CHECK(CRGB(CHSV(96, 200, 150)) == CRGB(4, 88, 4));
    CRGB color(255, 128, 1);
    fadeToBlackBy(&color, 1, 128);
    CHECK(color == CRGB(127, 64, 0));
    color += CRGB(200, 200, 200);
    CHECK(color == CRGB(255, 255, 200)); // Addition saturates rather than wrapping.
}

void testPatternsAndTiming() {
    Lampe flow;
    Programs::render(1, flow, 0);
    CHECK(flow.leds[7] == CRGB(255, 0, 0));
    for (uint8_t i = 0; i < Hardware::LedCount / 2; ++i) {
        CHECK(flow.leds[i] == flow.leds[Hardware::LedCount - 1 - i]);
    }
    Programs::render(1, flow, 19);
    CHECK(flow.hue == 0);
    Programs::render(1, flow, 20);
    CHECK(flow.hue == 1);

    Lampe ambulance;
    Programs::render(3, ambulance, 29);
    CHECK(ambulance.effect.position == 0);
    Programs::render(3, ambulance, 30);
    CHECK(ambulance.leds[0] == CRGB(255, 0, 0));
    CHECK(ambulance.leds[8] == CRGB(0, 0, 255));
    CHECK(ambulance.effect.position == 1);
    Programs::render(3, ambulance, 59);
    CHECK(ambulance.effect.position == 1);
    Programs::render(3, ambulance, 60);
    CHECK(ambulance.leds[1] == CRGB(255, 0, 0));
    CHECK(ambulance.leds[9] == CRGB(0, 0, 255));

    Lampe silence;
    fill_solid(silence.leds, Hardware::LedCount, CRGB(255, 255, 255));
    Programs::render(2, silence, 0);
    for (const auto &pixel : silence.leds) CHECK(pixel == CRGB(0, 0, 0));
}

uint32_t renderTrace(uint8_t program, uint16_t seed) {
    // Start each effect with black pixels and zero state, without touching GPIO.
    Lampe lamp;
    fill_solid(lamp.leds, Hardware::LedCount, CRGB(0, 0, 0));
    random16_set_seed(seed);
    uint32_t hash = 2166136261UL;
    for (uint32_t frame = 0; frame < 1800; ++frame) {
        const uint32_t now = frame * Hardware::FrameIntervalUs / 1000;
        Programs::render(program, lamp, now);
        for (const auto &pixel : lamp.leds) {
            for (uint8_t channel : {pixel.r, pixel.g, pixel.b}) {
                // FNV-1a fingerprint of every channel in every frame.
                hash = (hash ^ channel) * 16777619UL;
            }
        }
    }
    return hash;
}

void testEffectTraces() {
    // FastLED 3.10.5: 15 seconds per effect at the firmware's frame interval.
    // Review intentional appearance changes before updating these fingerprints.
    const uint32_t expected[] = {
        0x9893472d, 0xea33412f, 0x05b6abc5, 0x7bb16565,
        0x9bef1f34, 0x35e1865c, 0x6866b283, 0x6405bae8,
    };
    CHECK(Programs::count() == sizeof(expected) / sizeof(expected[0]));
    for (uint8_t program = 0; program < Programs::count(); ++program) {
        const uint32_t actual = renderTrace(program, 1337);
        if (actual != expected[program]) {
            std::cerr << "Effect " << static_cast<unsigned>(program)
                      << " changed: 0x" << std::hex << actual << '\n';
            std::exit(1);
        }
        // Check that restarting also resets random effects deterministically.
        CHECK(renderTrace(program, 1337) == actual);
    }
    CHECK(renderTrace(0, 42) != expected[0]);
    CHECK(renderTrace(5, 42) != expected[5]);
}

int main() {
    testColors();
    testPatternsAndTiming();
    testEffectTraces();
    std::cout << "Passed: FastLED color math, effect patterns/timing, and all eight rendering traces\n";
}
