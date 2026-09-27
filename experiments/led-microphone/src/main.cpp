// Temporary, open-loop LED/microphone experiment. No beat detector or feedback.
#include <Arduino.h>
#include <FastLED.h>
#include <avr/pgmspace.h>
#include "../../../lib/LampEngine/LampConfig.h"

constexpr uint8_t LedCount = 16, SampleCount = 12, RingMask = 31;
constexpr uint32_t FrameUs = 8333, SampleUs = 500;
CRGB leds[LedCount];
struct Plan { uint8_t kind, low, high, cycles; uint16_t ramp, hold; };
// kind: 0=no LED writes, 1=constant red, 2=constant white,
// 3=step/ramp cycles, 4=the normal 90 ms fading red beat pulse.
const Plan plans[] PROGMEM = {
    {0, 0, 0, 1, 0, 960}, {1, 0, 0, 1, 0, 960},
    {1, 64, 64, 1, 0, 960}, {1, 255, 255, 1, 0, 960},
    {2, 64, 64, 1, 0, 960},
    {3, 0, 64, 6, 0, 180}, {3, 64, 128, 6, 0, 180},
    {3, 0, 255, 6, 0, 180},
    {3, 0, 255, 4, 12, 180}, {3, 0, 255, 4, 60, 180},
    {3, 0, 255, 4, 240, 180}, {4, 0, 255, 24, 11, 60},
    {1, 0, 0, 1, 0, 960},
};
constexpr uint8_t PlanCount = sizeof(plans) / sizeof(plans[0]);
struct Sample { uint32_t time; uint16_t adc; uint8_t level; };
Sample samples[32];
uint8_t head = 0, tail = 0, stage = 0, level = 0, lastPhase = 255, lastCycle = 255;
uint16_t sequence = 0, dropped = 0;
uint32_t epoch = 0, frameAt = 0, sampleAt = 0, stageFrame = 0, frames = 0, readyAt = 0;
bool running = false, finished = false, completed = false, eventPending = false;
uint8_t event[25] = {};

uint8_t crc8(const uint8_t *data, uint8_t count) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < count; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit) crc = crc & 128 ? (crc << 1) ^ 7 : crc << 1;
    }
    return crc;
}
void put32(uint8_t *at, uint32_t value) {
    for (uint8_t i = 0; i < 4; ++i) at[i] = value >> (8 * i);
}
void makeEvent(uint8_t id, uint8_t cycle, uint8_t phase, uint8_t color,
               uint32_t before, uint32_t after) {
    // Phases are at least 90 ms apart: a pending event must be sent first.
    if (eventPending) ++dropped;
    event[0] = 'L'; event[1] = 'E'; event[2] = 1; event[3] = sizeof(event);
    event[4] = id; event[5] = cycle; event[6] = phase; event[7] = level;
    event[8] = color; event[9] = LampConfig::Brightness;
    put32(event + 10, before); put32(event + 14, after); put32(event + 18, frames);
    event[22] = dropped; event[23] = dropped >> 8;
    event[24] = crc8(event, 24); eventPending = true;
}
void setup() {
    Serial.begin(115200);
    FastLED.addLeds<WS2812B, 3, GRB>(leds, LedCount).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(LampConfig::Brightness);
    fill_solid(leds, LedCount, CRGB::Black);
    FastLED.show();
    analogRead(A0); // Prime the ADC before the measurement.
}
void renderFrame() {
    Plan plan;
    memcpy_P(&plan, plans + stage, sizeof(plan));
    uint32_t cycleFrames = plan.kind == 3 ? 2UL * (plan.hold + plan.ramp) : plan.hold;
    if (stageFrame >= cycleFrames * plan.cycles) {
        if (++stage == PlanCount) { running = false; finished = true; return; }
        stageFrame = 0; lastPhase = lastCycle = 255;
        memcpy_P(&plan, plans + stage, sizeof(plan));
        cycleFrames = plan.kind == 3 ? 2UL * (plan.hold + plan.ramp) : plan.hold;
    }
    const uint8_t cycle = stageFrame / cycleFrames;
    const uint16_t offset = stageFrame % cycleFrames;
    uint8_t phase = 0;
    level = plan.low;
    if (plan.kind == 3) {
        if (offset < plan.hold) { phase = 0; }
        else if (offset < plan.hold + plan.ramp) {
            phase = 1;
            level = plan.low + uint32_t(plan.high - plan.low) * (offset - plan.hold + 1) / plan.ramp;
        } else if (offset < 2UL * plan.hold + plan.ramp) { phase = 2; level = plan.high; }
        else {
            phase = 3;
            level = plan.high - uint32_t(plan.high - plan.low) * (offset - 2UL * plan.hold - plan.ramp + 1) / plan.ramp;
        }
    } else if (plan.kind == 4) {
        phase = offset < plan.ramp ? 1 : 0;
        level = offset < plan.ramp ? uint32_t(plan.ramp - offset) * plan.high / plan.ramp : 0;
    }
    const uint32_t before = micros() - epoch;
    if (plan.kind) {
        fill_solid(leds, LedCount, plan.kind == 2 ? CRGB(level, level, level) : CRGB(level, 0, 0));
        FastLED.show();
    }
    const uint32_t after = micros() - epoch;
    // Report the exact final ramp frame, not the following plateau frame.
    const uint8_t eventPhase = phase == 1 && plan.kind == 3 && level == plan.high ? 4 :
                               phase == 3 && level == plan.low ? 5 : phase;
    if (eventPhase != lastPhase || cycle != lastCycle) {
        makeEvent(stage, cycle, eventPhase, plan.kind == 2 ? 7 : 1, before, after);
        lastPhase = eventPhase; lastCycle = cycle;
    }
    ++stageFrame; ++frames;
}
void sendSamples() {
    const uint8_t available = (head - tail) & RingMask;
    if (available < SampleCount && !finished) return;
    if (!available || Serial.availableForWrite() < 61) return;
    const uint8_t count = available < SampleCount ? available : SampleCount;
    uint8_t packet[61] = {'L', 'F', 1, count};
    packet[4] = sequence; packet[5] = sequence >> 8;
    const uint32_t base = samples[tail].time;
    put32(packet + 6, base);
    packet[10] = dropped; packet[11] = dropped >> 8;
    for (uint8_t i = 0; i < count; ++i) {
        const Sample &sample = samples[tail];
        const uint32_t delta = (sample.time - base) / 4;
        if (delta > 16383) ++dropped;
        put32(packet + 12 + 4 * i, sample.adc | ((delta & 16383) << 10) | (uint32_t(sample.level) << 24));
        tail = (tail + 1) & RingMask;
    }
    packet[60] = crc8(packet, 60); Serial.write(packet, sizeof(packet)); ++sequence;
}
void loop() {
    if (!running && !finished && !completed) {
        if (Serial.available() && Serial.read() == 'G') {
            epoch = frameAt = sampleAt = micros(); running = true;
        } else if (millis() - readyAt >= 500 && !eventPending) {
            readyAt = millis(); makeEvent(254, 0, 0, 0, 0, 0);
        }
    }
    if (running) {
        uint32_t now = micros();
        if (now - frameAt >= FrameUs) {
            frameAt = now - frameAt < 2 * FrameUs ? frameAt + FrameUs : now;
            renderFrame();
        }
        now = micros();
        if (running && now - sampleAt >= SampleUs) {
            sampleAt = now; // Actual ADC start is recorded; never invent missed samples.
            const uint8_t next = (head + 1) & RingMask;
            const uint16_t reading = analogRead(A0);
            if (next == tail) ++dropped;
            else { samples[head] = {now - epoch, reading, level}; head = next; }
        }
    }
    if (eventPending && Serial.availableForWrite() >= sizeof(event)) {
        Serial.write(event, sizeof(event)); eventPending = false;
    }
    sendSamples();
    if (finished && head == tail && !eventPending) {
        makeEvent(255, 0, 0, 0, micros() - epoch, micros() - epoch);
        finished = false;
        completed = true;
    }
}
