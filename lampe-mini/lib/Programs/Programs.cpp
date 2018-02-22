#include "Programs.h"

void selectProgram(uint8_t menuOption, Lampe& lampe) {
  //Serial.print("Menu option: ");
  //Serial.println(menuOption);
	switch (menuOption) {
	case 0:
    rainbow(lampe);
    break;
  case 1:
    solid_red(lampe);
    break;
  case 2:
    mic2(lampe);
    break;
  case 3:
    beat_blink(lampe);
    break;
  default:
    Serial.print("No menuOption with this value in programs ");
    Serial.println(menuOption);
    break;
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

void beat_blink(Lampe& lampe) {
  mic(lampe);
}

void mic2(Lampe& lampe)
{
  const int sampleWindow = 50; // Sample window width in mS (50 mS = 20Hz)
  unsigned int sample;
  unsigned long startMillis= millis();  // Start of sample window
  unsigned int peakToPeak = 0;   // peak-to-peak level
 
  unsigned int signalMax = 0;
  unsigned int signalMin = 1024;
 
  // collect data for 50 mS
  while (millis() - startMillis < sampleWindow)
  {
     sample = analogRead(0);
     if (sample < 1024)  // toss out spurious readings
     {
        if (sample > signalMax)
        {
           signalMax = sample;  // save just the max levels
        }
        else if (sample < signalMin)
        {
           signalMin = sample;  // save just the min levels
        }
     }
  }
  peakToPeak = signalMax - signalMin;  // max - min = peak-peak amplitude
  double volts = (peakToPeak * 5.0) / 1024;  // convert to volts

  fill_solid(lampe.leds, lampe.num_leds, CRGB(lampe.linearReduce(peakToPeak * 255 / 1024), 0,0));
 
  Serial.println(lampe.linearReduce(peakToPeak));
}
