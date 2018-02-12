#ifndef PROGRAMS_H
#define PROGRAMS_H

#include "FastLED.h"
#include "Arduino.h"
#include "Lampe.h"

FASTLED_USING_NAMESPACE



void selectProgram(uint8_t menuOption, Lampe& lampe);
void rainbow(Lampe& lampe);
void solid_red(Lampe& lampe);
void mic(Lampe& lampe);

#endif
