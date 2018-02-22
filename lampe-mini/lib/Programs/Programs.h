#ifndef PROGRAMS_H
#define PROGRAMS_H

#include "FastLED.h"
#include "Arduino.h"
#include "Lampe.h"
#include "Mic.h"

FASTLED_USING_NAMESPACE



void selectProgram(uint8_t menuOption, Lampe& lampe);
void rainbow(Lampe& lampe);
void solid_red(Lampe& lampe);
void beat_blink(Lampe& lampe);
void mic2(Lampe& lampe);

#endif
