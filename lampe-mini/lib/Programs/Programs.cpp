#include "Programs.h"

void selectProgram(uint8_t menuOption, Lampe& lampe, Mic& mic) {
  //Serial.print("Menu option: ");
  //Serial.println(menuOption);
	switch (menuOption) {
	case 0:
    rainbow(lampe);
    break;
  case 1:
    solid_color(lampe);
    break;
  case 2:
    mic2(lampe);
    break;
  case 3:
    beat_blink(lampe, mic);
    break;
  case 4:
    beat_blink2(lampe, mic);
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

void solid_color(Lampe& lampe) {
  fill_solid(lampe.leds, lampe.num_leds, CRGB(120, 120, 120));
}

void beat_blink(Lampe& lampe, Mic& mic) {
  uint8_t period = 0;
  if (lampe.sampleInit || (lampe.getSampleTimer() > 60000)) { 
    fill_solid(lampe.leds, lampe.num_leds, CRGB(255, 0, 0));
    period = mic.detectBeat(lampe);
    Serial.print("BPM: ");
    Serial.println(period);
    fill_solid(lampe.leds, lampe.num_leds, CRGB(0, 255, 0));
  }

  EVERY_N_MILLISECONDS( period ) {
    fill_solid(lampe.leds, lampe.num_leds, CRGB(0, 255, 0));
  }
}


void beat_blink2(Lampe& lampe, Mic& mic) {
  mic.detectBeatOld(lampe);
}

void mic2(Lampe& lampe)
{
  const uint16_t sampleWindow = 50; // Sample window width in mS (50 mS = 20Hz)
  uint16_t sample;
  uint32_t startMillis= millis();  // Start of sample window
  uint16_t peakToPeak = 0;   // peak-to-peak level
 
  uint16_t signalMax = 0;
  uint16_t signalMin = 1024;
 
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

  uint8_t linval = lampe.linearReduce(peakToPeak * 255 / 1024);
  fill_solid(lampe.leds, lampe.num_leds, CRGB(2.5 * linval, 0,0));
  Serial.print(" peakToPeak: ");
  Serial.println(peakToPeak * 255 / 1024);
}
