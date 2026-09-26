#ifndef MIC_H
#define MIC_H

#include "Lampe.h"

// Thanks to Damian Peckett, http://dpeckett.com/beat-detection-on-the-arduino
// for beat detection.
// Our Global Sample Rate, 5000hz
#define SAMPLEPERIODUS 200
#define NUM_INTERVAL 25

// defines for setting and clearing register bits
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif


class Mic {
  public:
    Mic();
    uint8_t detectBeat(Lampe& lampe);
    void detectBeatOld(Lampe& lampe);
    uint8_t getBPM (uint32_t period);
  private:
    uint32_t getIntervalAverage(uint32_t beat_times[NUM_INTERVAL]);
    float bassFilter(float sample);
    float envelopeFilter(float sample);
    float beatFilter(float sample);
    uint32_t getTimer();
    void resetTimer();
    uint32_t timer;
    uint8_t count;
    uint32_t time;
    uint32_t up;
};


#endif
