#include <BeatTracker.h>
#include <cmath>
#include <cstdlib>
#include <iostream>

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << __LINE__ << ": " #condition "\n"; std::exit(1); \
} } while (false)

static uint32_t randomState = 42;
int noise(int range) {
    randomState = randomState * 1664525UL + 1013904223UL;
    return int((randomState >> 16) % (2 * range + 1)) - range;
}

// ADC samples, including a DC bias, decaying drum attacks, quieter offbeats,
// background noise and optional missing kicks. The detector sees only samples.
void music(LampLogic::BeatTracker &beat, uint32_t start, int duration, double bpm,
           int amplitude = 180, bool busy = false, bool missing = false) {
    const double period = 60000 / bpm;
    for (int t = 0; t < duration; ++t) {
        const int index = int(t / period);
        const double phase = t - index * period;
        const double strength = (index % 4 == 0 ? 1.0 : 0.85);
        int envelope = phase < 70 && !(missing && index % 5 == 3)
            ? int(amplitude * strength * std::exp(-phase / 22)) : 0;
        if (busy) {
            const double offbeat = phase - period / 2;
            if (offbeat >= 0 && offbeat < 35) envelope += int(amplitude * .25 * std::exp(-offbeat / 12));
        }
        beat.sample(512 + ((t & 1) ? envelope : -envelope) + noise(2), start + t);
    }
}

int main() {
    for (double bpm : {60., 73., 90., 100., 120., 127., 140., 173., 200.}) {
        LampLogic::BeatTracker beat;
        beat.reset(0);
        music(beat, 0, 14000, bpm);
        std::cout << "Tempo " << bpm << " -> " << int(beat.bpm()) << " (" << int(beat.confidence()) << ")\n";
        CHECK(std::abs(int(beat.bpm()) - bpm) <= 3);
        CHECK(beat.confidence() >= 55);
    }
    for (int amplitude : {12, 60, 200}) {
        LampLogic::BeatTracker beat;
        beat.reset(0);
        music(beat, 0, 16000, 120, amplitude, true, true);
        std::cout << "Busy/missing, amplitude " << amplitude << " -> " << int(beat.bpm()) << '\n';
        CHECK(std::abs(int(beat.bpm()) - 120) <= 3);
    }
    LampLogic::BeatTracker beat;
    beat.reset(0);
    for (uint32_t t = 0; t < 12000; ++t) beat.sample(512, t);
    CHECK(beat.bpm() == 0);
    for (uint32_t t = 12000; t < 24000; ++t) beat.sample((t & 1) ? 700 : 300, t);
    CHECK(beat.bpm() == 0); // A steady carrier is not a beat.
    for (uint32_t t = 24000; t < 44000; ++t) {
        beat.sample(512 + noise(120), t);
        CHECK(beat.bpm() == 0); // Broadband noise must not lock.
    }
    beat.reset(0);
    music(beat, 0, 14000, 120);
    CHECK(beat.pulse(14030) > 0);
    CHECK(beat.pulse(14250) == 0);
    // A clean lock should generate one short flash per half second, including
    // during a missed beat; it must not simply track instantaneous amplitude.
    int flashes = 0;
    bool lit = false;
    for (uint32_t t = 14000; t < 16000; ++t) {
        beat.sample(512, t);
        const bool next = beat.pulse(t) > 0;
        if (next && !lit) ++flashes;
        lit = next;
    }
    CHECK(flashes >= 3 && flashes <= 5);
    for (uint32_t t = 16000; t < 18000; ++t) beat.sample(512, t);
    CHECK(beat.bpm() == 0 && beat.pulse(18000) == 0);
    music(beat, 18000, 14000, 90);
    CHECK(std::abs(int(beat.bpm()) - 90) <= 3);
    music(beat, 32000, 16000, 140);
    CHECK(std::abs(int(beat.bpm()) - 140) <= 3);
    beat.reset(0xffffe000UL);
    music(beat, 0xffffe000UL, 14000, 120);
    CHECK(std::abs(int(beat.bpm()) - 120) <= 3); // millis() rollover.
    beat.reset(10);
    CHECK(beat.bpm() == 0 && beat.confidence() == 0 && beat.pulse(10) == 0);
    music(beat, 10, 14000, 120);
    beat.sample(512, 15000); // A gap cannot carry an old tempo into a new session.
    CHECK(beat.bpm() == 0);
    std::cout << "Passed: tempo range, weak/loud input, offbeats, missed beats, noise rejection, beat clock, silence, tempo changes and rollover\n";
}
