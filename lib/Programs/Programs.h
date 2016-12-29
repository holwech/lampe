#ifndef PROGRAMS_H
#define PROGRAMS_H

#include <Arduino.h>
#include <Tlc5940.h>
#include <Lampe.h>
#include <State.h>
#include <Config.h>


// Thanks to Damian Peckett, http://dpeckett.com/beat-detection-on-the-arduino
// for beat detection.
// Our Global Sample Rate, 5000hz
#define SAMPLEPERIODUS 200

// defines for setting and clearing register bits
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif


void init(Lampe& Lampe, Tlc5940& Tlc);
void mainMenu(Lampe& Lampe, Tlc5940&  Tlc, State& State);
void menuTransitionOff(Lampe& Lampe, Tlc5940& Tlc, State& State);
void secondMenu(Lampe& Lampe, Tlc5940&  Tlc, State& State);
void programs(Lampe& Lampe, Tlc5940&  Tlc, State& State);
void mic(Lampe& Lampe, State& state);

#endif
