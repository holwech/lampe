#include "Mic.h"

void beat_blink(Lampe &lampe, Mic &mic)
{
    uint8_t period = 0;
    if (lampe.sampleInit || (lampe.getSampleTimer() > 60000))
    {
        fill_solid(lampe.leds, lampe.num_leds, CRGB(255, 0, 0));
        period = mic.detectBeat(lampe);
        Serial.print("BPM: ");
        Serial.println(period);
        fill_solid(lampe.leds, lampe.num_leds, CRGB(0, 255, 0));
    }

    EVERY_N_MILLISECONDS(period)
    {
        fill_solid(lampe.leds, lampe.num_leds, CRGB(0, 255, 0));
    }
}

// Deprecated
void beat_blink2(Lampe &lampe, Mic &mic)
{
    mic.detectBeatOld(lampe);
}
