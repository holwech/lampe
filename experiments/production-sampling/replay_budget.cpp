#include <BeatTracker.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Scheduling-sensitivity probe, NOT a cycle-accurate AVR simulation. Restrict
// work calls per input window; unlike the normal replay, allow work to cross
// window boundaries. A work call has variable cost depending on tracker state.
int main(int argc, char **argv) {
    if (argc != 2) return 1;
    const int budget = std::atoi(argv[1]);
    if (budget < 1 || budget > 28) return 1;
    LampLogic::BeatTracker beat;
    char line[256];
    if (!std::fgets(line, sizeof(line), stdin) ||
        std::strncmp(line, "device_ms,peak,onset", 20)) return 1;
    bool first = true;
    std::puts("device_ms,bpm");
    while (std::fgets(line, sizeof(line), stdin)) {
        unsigned long timestamp;
        unsigned peak, onset;
        if (std::sscanf(line, "%lu,%u,%u", &timestamp, &peak, &onset) != 3 ||
            timestamp > UINT32_MAX || peak > 1023 || onset > 255) return 1;
        const uint32_t now = timestamp;
        if (first) { beat.reset(now - 10); first = false; }
        beat.sampleWindow(peak, now, true);
        for (int n = 0; n < budget && beat.workPending(); ++n) beat.work(now);
        std::printf("%lu,%u\n", timestamp, beat.bpm());
    }
    return first ? 1 : 0;
}
