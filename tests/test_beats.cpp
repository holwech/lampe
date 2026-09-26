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
    // Real captures had a continuously varying 30–200 ADC peak range, not
    // isolated kicks over a silent floor. Vary attacks and add competing sound
    // across the tempo range; a detector that only handles clean pulses fails.
    randomState = 42;
    for (int bpm : {73, 100, 120, 140, 173}) {
        LampLogic::BeatTracker noisy;
        noisy.reset(0);
        int matching = 0;
        for (uint32_t t = 10; t < 30000; t += 10) {
            const double period = 60000. / bpm;
            const double phase = std::fmod(t, period);
            const double offbeat = std::fmod(t + period / 2, period);
            const uint16_t peak = 60 + noise(30) +
                int((100 + noise(30)) * std::exp(-phase / 40)) +
                int(20 * std::exp(-offbeat / 25));
            noisy.sampleWindow(peak, t);
            if (t >= 10000) {
                const bool matches = std::abs(int(noisy.bpm()) - bpm) <= 4;
                CHECK(noisy.bpm() == 0 || matches);
                matching += matches;
            }
        }
        CHECK(matching > 1500); // At least 75% of the final 20 seconds.
    }
    // Window replay and the ADC path must produce identical decisions.
    LampLogic::BeatTracker direct, replay;
    direct.reset(0); replay.reset(0);
    uint32_t previousWindow = 0;
    for (uint32_t t = 0; t < 16000; ++t) {
        const int phase = t % 500;
        const int envelope = phase < 80 ? 200 * std::exp(-phase / 22.) : 0;
        direct.sample(512 + ((t & 1) ? envelope : -envelope), t);
        if (direct.windowTime() != previousWindow) {
            previousWindow = direct.windowTime();
            replay.sampleWindow(direct.windowPeak(), previousWindow);
            CHECK(direct.bpm() == replay.bpm());
            CHECK(direct.confidence() == replay.confidence());
            CHECK(direct.onset() == replay.onset());
            CHECK(direct.pulse(t) == replay.pulse(t));
        }
    }
    CHECK(replay.bpm() == 120);
    replay.sampleWindow(100, previousWindow + 20); // Missing diagnostic window.
    CHECK(replay.bpm() == 0);
    // Sustained non-musical energy at full ADC range exercises wide sums and
    // must not manufacture a tempo through arithmetic overflow.
    replay.reset(0);
    for (uint32_t t = 10; t < 20000; t += 10) {
        replay.sampleWindow((t / 10) % 2 ? 1023 : 0, t);
        CHECK(replay.bpm() == 0);
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
