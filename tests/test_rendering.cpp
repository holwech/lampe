#include <LampEngine.h>
#include <Programs.h>
#include <Telemetry.h>

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
    LampEngine flow;
    Programs::render(1, flow, 0);
    CHECK(flow.leds[7] == CRGB(255, 0, 0));
    for (uint8_t i = 0; i < LampConfig::LedCount / 2; ++i) {
        CHECK(flow.leds[i] == flow.leds[LampConfig::LedCount - 1 - i]);
    }
    Programs::render(1, flow, 19);
    CHECK(flow.hue == 0);
    Programs::render(1, flow, 20);
    CHECK(flow.hue == 1);

    LampEngine ambulance;
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

    LampEngine silence;
    fill_solid(silence.leds, LampConfig::LedCount, CRGB(255, 255, 255));
    Programs::render(2, silence, 0);
    for (const auto &pixel : silence.leds) CHECK(pixel == CRGB(0, 0, 0));
}

uint32_t renderTrace(uint8_t program, uint16_t seed) {
    // Start each effect with black pixels and zero state, without touching GPIO.
    LampEngine lamp;
    fill_solid(lamp.leds, LampConfig::LedCount, CRGB(0, 0, 0));
    random16_set_seed(seed);
    uint32_t hash = 2166136261UL;
    for (uint32_t frame = 0; frame < 1800; ++frame) {
        const uint32_t now = frame * LampConfig::FrameIntervalUs / 1000;
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

void testEngineAndTelemetry() {
    LampEngine lamp;
    lamp.reset(100, false, 42);
    CHECK(lamp.program() == 0);
    CHECK(!lamp.selectProgram(255, 100));
    CHECK(lamp.program() == 0);
    CHECK(lamp.selectProgram(2, 100));
    lamp.sampleAudio(0, 100);
    lamp.sampleAudio(1023, 150);
    lamp.render(150);
    CHECK(lamp.audioLevel() == 255);
    CHECK(lamp.leds[0] == CRGB(255, 0, 0));
    lamp.selectProgram(0, 150);
    CHECK(lamp.audioLevel() == 0);
    CHECK(!lamp.pollButton(true, 150));
    CHECK(!lamp.pollButton(true, 171));
    CHECK(!lamp.pollButton(false, 180));
    CHECK(lamp.pollButton(false, 201));
    CHECK(lamp.program() == 1);
    CHECK(!lamp.pollButton(false, 250));

    LampEngine first, second;
    first.reset(0, false, 1337); second.reset(0, false, 1337);
    first.selectProgram(5, 0); second.selectProgram(5, 0);
    for (uint32_t time = 0; time < 10000; time += 8) {
        first.render(time); second.render(time);
        for (uint8_t i = 0; i < LampConfig::LedCount; ++i) CHECK(first.leds[i] == second.leds[i]);
    }
    const uint8_t known[] = {'1','2','3','4','5','6','7','8','9'};
    CHECK(Telemetry::checksum(known, sizeof(known)) == 0xf4);
    struct FakeSerial {
        int capacity = 0;
        int writes = 0;
        uint8_t bytes[Telemetry::PacketSize] = {};
        int availableForWrite() { return capacity; }
        void write(const uint8_t *data, size_t size) {
            CHECK(size <= static_cast<size_t>(capacity));
            CHECK(size == Telemetry::PacketSize);
            for (size_t i = 0; i < size; ++i) bytes[i] = data[i];
            ++writes;
        }
    } serial;
    for (int capacity = 0; capacity < static_cast<int>(Telemetry::PacketSize); ++capacity) {
        serial.capacity = capacity;
        CHECK(!Telemetry::tryWrite(serial, lamp, 0x12345678, 254, 100));
    }
    CHECK(serial.writes == 0);
    serial.capacity = Telemetry::PacketSize;
    CHECK(Telemetry::tryWrite(serial, lamp, 0x12345678, 254, 100));
    CHECK(serial.writes == 1);
    CHECK(serial.bytes[0] == 'L' && serial.bytes[1] == 'M');
    CHECK(serial.bytes[2] == 2 && serial.bytes[3] == 16);
    CHECK(serial.bytes[4] == 1 && serial.bytes[5] == 100 && serial.bytes[7] == 254);
    CHECK(serial.bytes[8] == 0x78 && serial.bytes[9] == 0x56 && serial.bytes[10] == 0x34 && serial.bytes[11] == 0x12);
    CHECK(serial.bytes[12] == lamp.leds[0].r);
    CHECK(serial.bytes[60] == 0 && serial.bytes[61] == 0);
    CHECK(serial.bytes[62] == Telemetry::checksum(serial.bytes, 62));
}

void testBeatRendering() {
    LampEngine lamp;
    lamp.reset(0);
    lamp.selectProgram(2, 0);
    for (uint32_t now = 0; now < 14000; ++now) {
        const uint16_t amplitude = now % 500 < 40 ? 200 : 0;
        lamp.sampleAudio((now & 1) ? 512 + amplitude : 512 - amplitude, now);
        lamp.render(now);
    }
    CHECK(lamp.bpm() >= 118 && lamp.bpm() <= 122);
    CHECK(lamp.beatConfidence() >= 55);
    // Beat-clock pulses continue through a missing microphone hit.
    bool flashed = false, dark = false;
    for (uint32_t now = 14000; now < 15000; ++now) {
        lamp.sampleAudio(512, now);
        lamp.render(now);
        flashed |= lamp.leds[0].r > 100;
        dark |= lamp.leds[0].r == 0;
    }
    CHECK(flashed && dark);
    uint8_t packet[Telemetry::PacketSize];
    Telemetry::encode(packet, lamp, 15000, 1, 100);
    CHECK(packet[60] == lamp.bpm() && packet[61] == lamp.beatConfidence());
    lamp.selectProgram(0, 15000);
    CHECK(lamp.bpm() == 0 && lamp.beatConfidence() == 0);
    lamp.selectProgram(2, 15000);
    lamp.render(15000);
    CHECK(lamp.leds[0] == CRGB(0, 0, 0));
}

int main() {
    testColors();
    testPatternsAndTiming();
    testEffectTraces();
    testEngineAndTelemetry();
    testBeatRendering();
    std::cout << "Passed: FastLED colors, eight effect traces, shared engine, and nonblocking telemetry\n";
}
