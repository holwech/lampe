#ifndef PROGRAMS_H
#define PROGRAMS_H

#include <Arduino.h>
#include <Tlc5940.h>
#include <Lampe.h>
#include <State.h>

void init(Lampe& Lampe, Tlc5940& Tlc);
void mainMenu(Lampe& Lampe, Tlc5940&  Tlc, State& state);
void mainMenuTransistionOff(Lampe& Lampe, Tlc5940& Tlc, State& State);

#endif
