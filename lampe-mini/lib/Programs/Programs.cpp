#include "Programs.h"

void selectProgram(uint8_t menuOption, Lampe &lampe, Mic &mic)
{
    //Serial.print("Menu option: ");
    //Serial.println(menuOption);
    switch (menuOption)
    {
    case 0:
        quarter_blink(lampe);
        break;
    case 1:
        flow(lampe);
        break;
    case 2:
        amplitude_sensor(lampe);
        break;
    case 3:
        ambulance(lampe);
        break;
    case 4:
        ambulance_hue(lampe);
        break;
    case 5:
        fire_place(lampe);
        break;
    case 6:
        northern_lights(lampe);
        break;
    case 7:
        rainbow(lampe);
        break;
    default:
        Serial.print("No menuOption with this value in programs ");
        Serial.println(menuOption);
        break;
    }
}

void quarter_blink(Lampe &lampe)
{
    EVERY_N_MILLISECONDS(200)  
    {
        //fill_solid(lampe.leds, lampe.num_leds, CRGB(0, 0, 0));
        CHSV color = CHSV(random8(255), 255, 255);
        for (uint8_t i = lampe.stateValues[0]; i < (lampe.stateValues[0] + 8); i++)
        {
            int ledPos = i % lampe.num_leds;
            lampe.leds[ledPos] = color;
        }
        lampe.stateValues[0] = lampe.cycleNumber(lampe.stateValues[0], lampe.num_leds, random8(lampe.num_leds));
    }
}

void ambulance_hue(Lampe &lampe)
{
    EVERY_N_MILLISECONDS(2000) { 
        lampe.stateValues[1] = random8(255);
        lampe.stateValues[2] = random8(255);
    }
    EVERY_N_MILLISECONDS(50) { 
        CHSV color1 = CHSV(lampe.stateValues[1], 255, 255);
        CHSV color2 = CHSV(lampe.stateValues[2], 255, 255);
        lampe.leds[lampe.stateValues[0]] = color1;
        lampe.leds[(lampe.stateValues[0] + NUM_LEDS / 2) % NUM_LEDS] = color2;
        lampe.stateValues[0] = lampe.cycleNumber(lampe.stateValues[0], NUM_LEDS, 1);
    }
}

void rainbow(Lampe &lampe)
{
    fill_rainbow(lampe.leds, lampe.num_leds, lampe.gHue, 5);
    EVERY_N_MILLISECONDS(40) { lampe.gHue++; }
}

void flow(Lampe &lampe)
{
    for (int i = 0; i < lampe.num_leds / 2; i++)
    {
        int ledPosFirst = (FLOW_START - i - 1) % lampe.num_leds;
        int ledPosSecond = (FLOW_START + i) % lampe.num_leds;
        lampe.leds[ledPosFirst] = CHSV(lampe.gHue + HUE_STEP_SIZE * i, 255, 255);
        lampe.leds[ledPosSecond] = CHSV(lampe.gHue + HUE_STEP_SIZE * i, 255, 255);
    }
    EVERY_N_MILLISECONDS(20) { lampe.gHue++; }
}

void ambulance(Lampe &lampe)
{
    EVERY_N_MILLISECONDS(30) { 
        lampe.leds[lampe.stateValues[0]] = CRGB(255, 0, 0);
        lampe.leds[(lampe.stateValues[0] + NUM_LEDS / 2) % NUM_LEDS] = CRGB(0, 0, 255);
        lampe.stateValues[0] = lampe.cycleNumber(lampe.stateValues[0], NUM_LEDS, 1);
    }
}

void fire_place(Lampe &lampe)
{
    fadeToBlackBy( lampe.leds, NUM_LEDS, 1);
    EVERY_N_MILLISECONDS(401)
    { 
        int pos = random16(NUM_LEDS);
        lampe.leds[pos] += CHSV( -10 + random8(60), 255, 255);
    }
    EVERY_N_MILLISECONDS(300)
    { 
        int pos = random16(NUM_LEDS);
        lampe.leds[pos] += CHSV( -10 + random8(60), 255, 255);
    }
    EVERY_N_MILLISECONDS(152)
    { 
        int pos = random16(NUM_LEDS);
        lampe.leds[pos] += CHSV( -10 + random8(60), 255, 255);
    }
}

void northern_lights(Lampe &lampe) 
{
    fadeToBlackBy( lampe.leds, NUM_LEDS, 2);
    EVERY_N_MILLISECONDS(30)
    {
        int pos = random16(NUM_LEDS);
        lampe.leds[pos] += CHSV( lampe.gHue + random8(64), 200, 150);
    }
    EVERY_N_MILLISECONDS(300) { lampe.gHue++; }
}

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

void amplitude_sensor(Lampe &lampe)
{
    const uint16_t sampleWindow = 50; // Sample window width in mS (50 mS = 20Hz)
    uint16_t sample;
    uint32_t startMillis = millis(); // Start of sample window
    uint16_t peakToPeak = 0;         // peak-to-peak level

    uint16_t signalMax = 0;
    uint16_t signalMin = 1024;

    // collect data for 50 mS
    while (millis() - startMillis < sampleWindow)
    {
        sample = analogRead(0);
        if (sample < 1024) // toss out spurious readings
        {
            if (sample > signalMax)
            {
                signalMax = sample; // save just the max levels
            }
            else if (sample < signalMin)
            {
                signalMin = sample; // save just the min levels
            }
        }
    }
    peakToPeak = signalMax - signalMin;       // max - min = peak-peak amplitude
    double volts = (peakToPeak * 5.0) / 1024; // convert to volts

    uint8_t linval = lampe.linearReduce(peakToPeak * 255 / 1024);
    fill_solid(lampe.leds, lampe.num_leds, CRGB(2.5 * linval, 0, 0));
    Serial.print(" peakToPeak: ");
    Serial.println(peakToPeak * 255 / 1024);
}
