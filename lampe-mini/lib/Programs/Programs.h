#ifndef PROGRAMS_H
#define PROGRAMS_H

#include "FastLED.h"
#include "Arduino.h"
#include "Lampe.h"
#include "Mic.h"

FASTLED_USING_NAMESPACE

#define HUE_STEP_SIZE   8
#define FLOW_START      8

void selectProgram(uint8_t menuOption, Lampe& lampe, Mic& mic);
void rainbow(Lampe& lampe);
void quarter_blink(Lampe &lampe);
void flow(Lampe& lampe);
void ambulance(Lampe &lampe);
void ambulance_hue(Lampe &lampe);
void fire_place(Lampe &lampe);
void northern_lights(Lampe &lampe);
void beat_blink(Lampe& lampe, Mic& mic);
void beat_blink2(Lampe& lampe, Mic& mic);
void amplitude_sensor(Lampe& lampe);

#endif
