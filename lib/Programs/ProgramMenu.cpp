#include "Programs.h"
#include <LampLogic.h>

namespace Programs {
namespace {
struct Program {
    void (*render)(LampEngine &, uint32_t);
    bool usesAudio;
};

// This table determines both dispatch and the number of button-selectable effects.
const Program programs[] = {
#define LAMP_PROGRAM(function, label, audio) {function, audio},
#include "ProgramList.def"
#undef LAMP_PROGRAM
};
constexpr uint8_t ProgramCount = sizeof(programs) / sizeof(programs[0]);
static_assert(sizeof(programs) / sizeof(programs[0]) <= 255, "Program index is 8-bit");
} // namespace

uint8_t count() { return ProgramCount; }

uint8_t next(uint8_t current) {
    return LampLogic::wrapIndex(current, ProgramCount);
}

bool usesAudio(uint8_t program) {
    return program < ProgramCount && programs[program].usesAudio;
}

void render(uint8_t program, LampEngine &lampe, uint32_t now) {
    if (program < ProgramCount) {
        programs[program].render(lampe, now);
    }
}
} // namespace Programs
