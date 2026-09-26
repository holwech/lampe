#ifndef PROGRAMS_H
#define PROGRAMS_H

#include <stdint.h>

class LampEngine;

namespace Programs {
uint8_t count();
uint8_t next(uint8_t current);
bool usesAudio(uint8_t program);
void render(uint8_t program, LampEngine &lampe, uint32_t now);

void quarterBlink(LampEngine &lampe, uint32_t now);
void flow(LampEngine &lampe, uint32_t now);
void amplitude(LampEngine &lampe, uint32_t now);
void ambulance(LampEngine &lampe, uint32_t now);
void ambulanceHue(LampEngine &lampe, uint32_t now);
void fireplace(LampEngine &lampe, uint32_t now);
void northernLights(LampEngine &lampe, uint32_t now);
void rainbow(LampEngine &lampe, uint32_t now);
} // namespace Programs

#endif
