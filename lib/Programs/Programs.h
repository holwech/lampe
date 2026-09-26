#ifndef PROGRAMS_H
#define PROGRAMS_H

#include <stdint.h>

class Lampe;

namespace Programs {
uint8_t count();
uint8_t next(uint8_t current);
bool usesAudio(uint8_t program);
void render(uint8_t program, Lampe &lampe, uint32_t now);

void quarterBlink(Lampe &lampe, uint32_t now);
void flow(Lampe &lampe, uint32_t now);
void amplitude(Lampe &lampe, uint32_t now);
void ambulance(Lampe &lampe, uint32_t now);
void ambulanceHue(Lampe &lampe, uint32_t now);
void fireplace(Lampe &lampe, uint32_t now);
void northernLights(Lampe &lampe, uint32_t now);
void rainbow(Lampe &lampe, uint32_t now);
} // namespace Programs

#endif
