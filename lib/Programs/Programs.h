#ifndef PROGRAMS_H
#define PROGRAMS_H

#include <Arduino.h>
#include <Tlc5940.h>
#include <Lampe.h>
#include <State.h>
#include <Config.h>


void init(Lampe& Lampe, Tlc5940& Tlc);
void mainMenu(Lampe& Lampe, Tlc5940&  Tlc, State& State);
void menuTransitionOff(Lampe& Lampe, Tlc5940& Tlc, State& State);
void secondMenu(Lampe& Lampe, Tlc5940&  Tlc, State& State);
void programs(Lampe& Lampe, Tlc5940&  Tlc, State& State);

#endif
