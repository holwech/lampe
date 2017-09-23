#ifndef MIC_H
#define MIC_H

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

#define NUM_INTERVAL 100

float bassFilter(float sample);
float envelopeFilter(float sample);
float beatFilter(float sample);
unsigned int getBPM (Lampe);
void mic(Lampe& Lampe, State& state);
void sample(unsigned int beatInterval[10], unsigned long timer);


#endif
