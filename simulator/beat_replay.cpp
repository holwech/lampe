#include <BeatTracker.h>
#include <cstdio>
#include <cstdint>
#include <cstring>

// Input is LA v2 windows.csv, never the lossy LA v1 raw waveform stream.
int main(int argc, char **argv) {
    if (argc > 2 || (argc == 2 && std::strcmp(argv[1], "--deferred"))) return 1;
    const bool deferred = argc == 2;
    LampLogic::BeatTracker beat;
    char line[256];
    if (!std::fgets(line, sizeof(line), stdin) ||
        std::strncmp(line, "device_ms,peak,onset", 20) != 0) return 1;
    bool first = true;
    std::puts("device_ms,bpm,confidence,onset,pulse");
    while (std::fgets(line, sizeof(line), stdin)) {
        unsigned long timestamp;
        unsigned peak, onset;
        if (std::sscanf(line, "%lu,%u,%u", &timestamp, &peak, &onset) != 3 ||
            timestamp > UINT32_MAX || peak > 1023 || onset > 255) return 1;
        const uint32_t now = timestamp;
        if (first) { beat.reset(now - 10); first = false; }
        beat.sampleWindow(peak, now, deferred);
        unsigned calls = 0;
        while (beat.workPending()) {
            if (++calls > 28) return 1;
            beat.work(now);
        }
        std::printf("%lu,%u,%u,%u,%u\n", timestamp, beat.bpm(),
                    beat.confidence(), beat.onset(), beat.pulse(now));
    }
    return first ? 1 : 0;
}
