#include <BeatTracker.h>
#include <BTrack.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <cmath>

// Reference tempos are deliberately absent from this process. An optional
// constant initialization is applied equally to every input for sensitivity tests.
int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--info") == 0) {
        std::puts("{\"lamp_header_sha256\":\"" LAMP_HEADER_SHA256 "\","
                  "\"btrack_commit\":\"9d6127618a5679e9caa74c594b88f1d74f0e035f\","
                  "\"libsamplerate_commit\":\"c96f5e3de9c4488f4e6c97f59f5245f22fda22f7\"}");
        return 0;
    }
    const int initial = argc == 2 ? std::atoi(argv[1]) : 0;
    if (argc > 2 || (argc == 2 && (initial < 80 || initial > 160))) return 1;
    char line[256];
    if (!std::fgets(line, sizeof(line), stdin)) return 1;
    const bool suppliedOnset = std::strcmp(line, "time_ms,peak,onset\n") == 0;
    if (!suppliedOnset && std::strcmp(line, "time_ms,peak\n")) return 1;
    LampLogic::BeatTracker lamp;
    std::unique_ptr<BTrack> reference;
    unsigned long previous = 0, segmentStart = 0, nextTick = 0, ticks = 0;
    unsigned segment = 0;
    double previousOnset = 0;
    std::puts("time_ms,segment,segment_ms,peak,onset,lamp_bpm,lamp_pulse,btrack_bpm,btrack_beat,btrack_score,btrack_ticks,source_onset");
    while (std::fgets(line, sizeof(line), stdin)) {
        unsigned long now;
        unsigned peak;
        double inputOnset = 0;
        const int fields = std::sscanf(line, "%lu,%u,%lf", &now, &peak, &inputOnset);
        if (fields != (suppliedOnset ? 3 : 2) || !std::isfinite(inputOnset) || inputOnset < 0 ||
            now > UINT32_MAX || peak > 1023 ||
            (reference && now <= previous)) return 1;
        if (!reference || now - previous > 15) {
            // 441 samples / 44100 Hz = exactly 10 ms per onset sample.
            // Audio FFT is unused; 1024 is a valid frame size for construction.
            reference = std::make_unique<BTrack>(441, 1024);
            if (initial) reference->setTempo(initial);
            lamp.reset(static_cast<uint32_t>(now) - 10);
            segmentStart = nextTick = now;
            ticks = previousOnset = 0;
            ++segment;
        }
        lamp.sampleWindow(peak, static_cast<uint32_t>(now));
        if (!suppliedOnset) inputOnset = lamp.onset();
        bool beat = false;
        // Causal zero-order hold onto a 100 Hz clock, respecting real timestamps.
        // Never interpolate from a future sample or bridge a diagnostic gap.
        while (nextTick <= now) {
            reference->processOnsetDetectionFunctionSample(nextTick == now ? inputOnset : previousOnset);
            beat |= reference->beatDueInCurrentFrame();
            nextTick += 10;
            ++ticks;
        }
        std::printf("%lu,%u,%lu,%u,%u,%u,%u,%.6f,%u,%.6f,%lu,%.8f\n", now, segment,
                    now - segmentStart, peak, lamp.onset(), lamp.bpm(), lamp.pulse(now),
                    reference->getCurrentTempoEstimate(), beat,
                    reference->getLatestCumulativeScoreValue(), ticks, inputOnset);
        previous = now;
        previousOnset = inputOnset;
    }
    return reference ? 0 : 1;
}
