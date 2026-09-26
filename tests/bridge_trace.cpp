// This exact input trace is repeated in web/tests/simulator.test.mjs.
#include <stdint.h>
#include <cstdio>
#include <initializer_list>
extern "C" {
void lamp_reset(uint16_t);
int lamp_select(uint8_t);
void lamp_brightness(uint8_t);
void lamp_advance(uint32_t, uint16_t, int);
const uint8_t *lamp_frame();
uint32_t lamp_frame_size();
}
int main() {
    for (uint16_t seed : {uint16_t(42), uint16_t(1337)}) {
        for (uint8_t program = 0; program < 8; ++program) {
            lamp_reset(seed);
            lamp_select(program);
            lamp_brightness(177);
            for (uint32_t frame = 0; frame < 360; ++frame) {
                lamp_advance(1, static_cast<uint16_t>((frame * 73) % 1024), frame >= 100 && frame < 110);
                const uint8_t *packet = lamp_frame();
                for (uint32_t byte = 0; byte < lamp_frame_size(); ++byte) std::printf("%02x", packet[byte]);
                std::puts("");
            }
        }
    }
    lamp_reset(1337);
    lamp_select(2);
    for (uint32_t frame = 0; frame < 3000; ++frame) {
        const uint32_t time = frame * 8333UL / 1000;
        const uint16_t period = frame < 1440 ? 500 : 428;
        lamp_advance(1, frame < 2640 && time % period < 40 ? 700 : 0, 0);
        const uint8_t *packet = lamp_frame();
        for (uint32_t byte = 0; byte < lamp_frame_size(); ++byte) std::printf("%02x", packet[byte]);
        std::puts("");
    }
}
