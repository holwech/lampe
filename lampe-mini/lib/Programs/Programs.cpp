#include "Programs.h"

void selectProgram(uint8_t menuOption, Lampe& lampe) {
	switch (menuOption) {
	case 0:
    rainbow(lampe);
  case 1:
    solid_red(lampe);
	}
}


void rainbow(Lampe& lampe) 
{
  fill_rainbow(lampe.leds, lampe.num_leds, lampe.gHue, 7);
  EVERY_N_MILLISECONDS( 20 ) { lampe.gHue++; }
}

void solid_red(Lampe& lampe) {
  fill_solid(lampe.leds, lampe.num_leds, CRGB(255, 0,0));
}

void mic(Lampe& lampe)
{
  uint16_t sample = analogRead(0);
  fill_solid (lampe.leds, lampe.num_leds, CHSV(lampe.gHue, 255, floor(sample * (255.0 / 1024.0))));

  Serial.println(floor(sample * (255.0 / 1024.0)));
}
