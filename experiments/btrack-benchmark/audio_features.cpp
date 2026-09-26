#include <OnsetDetectionFunction.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <algorithm>

// Upstream causal audio front end at the captured 4 kHz sample rate.
// 40 new samples per hop -> 100 Hz onsets; 128-sample trailing Hann window.
int main() {
    char line[256];
    if (!std::fgets(line, sizeof(line), stdin) || std::strncmp(line, "sample_index,adc", 16)) return 1;
    OnsetDetectionFunction onset(40, 128, ComplexSpectralDifferenceHWR, HanningWindow);
    unsigned used = 0, low = 1023, high = 0;
    unsigned long previous = 0, count = 0;
    double block[40], dc = 512;
    std::puts("time_ms,peak,onset");
    while (std::fgets(line, sizeof(line), stdin)) {
        unsigned long index;
        unsigned adc;
        if (std::sscanf(line, "%lu,%u", &index, &adc) != 2 || adc > 1023 ||
            (count && index != previous + 1)) return 1;
        previous = index;
        ++count;
        dc += (double(adc) - dc) / 64;
        block[used++] = (double(adc) - dc) / 512;
        low = std::min(low, adc); high = std::max(high, adc);
        if (used == 40) {
            std::printf("%lu,%u,%.8f\n", count / 4, high - low,
                        onset.calculateOnsetDetectionFunctionSample(block));
            used = high = 0; low = 1023;
        }
    }
    return count ? 0 : 1;
}
