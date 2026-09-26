#include <LampLogic.h>
#include <Programs.h>

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << __FILE__ << ':' << __LINE__ << ": " #condition "\n"; \
        std::exit(1); \
    } \
} while (false)

// Link the real menu to recording callbacks; these tests exercise dispatch,
// not FastLED's pixel rendering or hardware I/O.
class Lampe {};
static const char *rendered = nullptr;
static uint32_t renderedAt = 0;
namespace Programs {
#define RECORD_EFFECT(name) \
    void name(Lampe &, uint32_t now) { rendered = #name; renderedAt = now; }
RECORD_EFFECT(quarterBlink)
RECORD_EFFECT(flow)
RECORD_EFFECT(amplitude)
RECORD_EFFECT(ambulance)
RECORD_EFFECT(ambulanceHue)
RECORD_EFFECT(fireplace)
RECORD_EFFECT(northernLights)
RECORD_EFFECT(rainbow)
#undef RECORD_EFFECT
} // namespace Programs

void testBrightness() {
    CHECK(LampLogic::amplitudeLevel(0) == 0);
    CHECK(LampLogic::amplitudeLevel(512) == 127);
    CHECK(LampLogic::amplitudeLevel(1023) == 255);
    CHECK(LampLogic::amplitudeLevel(65535) == 255);
    uint8_t previous = 0;
    for (uint16_t sample = 0; sample <= 1023; ++sample) {
        const uint8_t level = LampLogic::amplitudeLevel(sample);
        CHECK(level == static_cast<uint64_t>(sample) * 255 / 1023);
        CHECK(level >= previous);
        previous = level;
    }
    CHECK(LampLogic::amplifiedBrightness(0) == 0);
    CHECK(LampLogic::amplifiedBrightness(100) == 250);
    for (uint16_t level = 102; level <= 255; ++level) {
        CHECK(LampLogic::amplifiedBrightness(static_cast<uint8_t>(level)) == 255);
    }
}

void testAudioWindows() {
    LampLogic::AudioEnvelope audio;
    audio.reset(0);
    audio.sample(100, 0);
    audio.sample(200, 10);
    CHECK(audio.level() == 0); // No partial-window output.
    audio.sample(300, 50);
    CHECK(audio.level() == LampLogic::amplitudeLevel(200)); // Rising samples still update the minimum.

    audio.reset(100);
    audio.sample(512, 100);
    audio.sample(512, 150);
    CHECK(audio.level() == 0); // Constant DC is silence, regardless of offset.
    audio.sample(65535, 200);
    CHECK(audio.level() == 0); // An empty/invalid window cannot underflow min/max.

    audio.reset(0);
    audio.sample(0, 0);
    audio.sample(1023, 50);
    CHECK(audio.level() == 255);
    audio.sample(512, 55);
    CHECK(audio.level() == 254);
    audio.sample(512, 100);
    CHECK(audio.level() == 245); // Decay follows elapsed time, not loop count.
    audio.sample(512, 2000);
    CHECK(audio.level() == 0); // Long pauses saturate at zero.
    audio.reset(2000);
    CHECK(audio.level() == 0);

    const uint32_t start = std::numeric_limits<uint32_t>::max() - 25;
    audio.reset(start);
    audio.sample(0, start);
    audio.sample(1023, 24);
    CHECK(audio.level() == 255); // Window spans millis() rollover.
    audio.sample(512, 34);
    CHECK(audio.level() == 253);
}

void testButtonDebounce() {
    LampLogic::DebouncedButton button;
    button.reset(false, 0);
    CHECK(!button.released(false, 100)); // No phantom startup click.
    CHECK(!button.released(true, 110));
    CHECK(!button.released(false, 115)); // Press bounce.
    CHECK(!button.released(true, 120));
    CHECK(!button.released(true, 140)); // Stable press, no release yet.
    CHECK(!button.released(true, 1000)); // Holding never repeats.
    CHECK(!button.released(false, 1010));
    CHECK(!button.released(true, 1015)); // Release bounce.
    CHECK(!button.released(false, 1020));
    CHECK(!button.released(false, 1039));
    CHECK(button.released(false, 1040));
    CHECK(!button.released(false, 1100)); // Exactly one event per release.

    button.reset(false, 0);
    CHECK(!button.released(true, 1));
    CHECK(!button.released(false, 10));
    CHECK(!button.released(false, 100)); // Ignore a pulse shorter than the debounce interval.

    const uint32_t start = std::numeric_limits<uint32_t>::max() - 10;
    button.reset(false, start - 10);
    CHECK(!button.released(true, start));
    CHECK(!button.released(true, 9)); // Stable press across timer rollover.
    CHECK(!button.released(false, 10));
    CHECK(button.released(false, 30));
    button.reset(true, 50);
    CHECK(!button.released(true, 100)); // Starting while held is also quiet.
    CHECK(!button.released(false, 101));
    CHECK(button.released(false, 121));
}

void testMenu() {
    const char *expected[] = {"quarterBlink", "flow", "amplitude", "ambulance",
                              "ambulanceHue", "fireplace", "northernLights", "rainbow"};
    CHECK(Programs::count() == sizeof(expected) / sizeof(expected[0]));
    Lampe lamp;
    uint8_t index = 0;
    for (uint8_t pass = 0; pass < 2; ++pass) {
        for (uint8_t i = 0; i < Programs::count(); ++i) {
            CHECK(index == i);
            Programs::render(index, lamp, 1234);
            CHECK(std::string(rendered) == expected[i]);
            CHECK(renderedAt == 1234);
            CHECK(Programs::usesAudio(index) == (i == 2));
            index = Programs::next(index);
        }
        CHECK(index == 0);
    }
    rendered = nullptr;
    Programs::render(255, lamp, 1234);
    CHECK(rendered == nullptr);
    CHECK(!Programs::usesAudio(255));
}

void testIndexAndTiming() {
    CHECK(LampLogic::wrapIndex(15, 16) == 0);
    CHECK(LampLogic::wrapIndex(15, 16, 2) == 1); // Preserve overshoot instead of jumping to zero.
    CHECK(LampLogic::wrapIndex(14, 16, 15) == 13);
    CHECK(LampLogic::wrapIndex(255, 16, 255) == 14); // Widen before addition.
    CHECK(LampLogic::wrapIndex(0, 0) == 0);
    uint32_t last = 100;
    CHECK(!LampLogic::intervalElapsed(119, last, 20));
    CHECK(last == 100);
    CHECK(LampLogic::intervalElapsed(120, last, 20));
    CHECK(last == 120);
    CHECK(!LampLogic::intervalElapsed(120, last, 20));
    last = std::numeric_limits<uint32_t>::max() - 5;
    CHECK(!LampLogic::intervalElapsed(3, last, 10));
    CHECK(LampLogic::intervalElapsed(4, last, 10));
}

int main() {
    testBrightness();
    testAudioWindows();
    testButtonDebounce();
    testMenu();
    testIndexAndTiming();
    std::cout << "Passed: brightness, audio windows/decay, button debounce, all eight menu effects, and timer/index wrapping\n";
}
