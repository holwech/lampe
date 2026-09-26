#ifndef LAMPE_H
#define LAMPE_H

#include "FastLED.h"
#include "Arduino.h"

FASTLED_USING_NAMESPACE

#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif

#define DATA_PIN            3
#define LED_TYPE            WS2812B
#define COLOR_ORDER         GRB
#define NUM_LEDS            16
#define BRIGHTNESS          100
#define FRAMES_PER_SECOND   120
#define ARRAY_SIZE(A)       (sizeof(A) / sizeof((A)[0]))
#define NUM_MENU_OPTIONS    5
#define BUTTON_PIN          2


class Lampe
{
  public:
    Lampe();
    bool buttonClick();
    void updateLinearReduce();
    uint8_t linearReduce(uint8_t peak);
    uint32_t getTimer();
    void resetTimer();
    uint32_t getSampleTimer();
    void resetSampleTimer();
    uint8_t nextMenuOptionOnClick();
    uint8_t nextMenuOption();
    void incrementMenu();
    void update();
    uint8_t cycleNumber(uint8_t i, uint8_t peak, uint8_t stepSize);
    bool sampleInit;
    uint8_t gHue;
    uint8_t num_leds;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    CRGB leds[NUM_LEDS];
    uint8_t stateValues[NUM_LEDS] = {};
  private:
    void newStateVarReset();
    uint32_t timer;
    uint32_t linearReduceTimer;
    uint32_t sampleTimer;
    uint8_t linearValue;
    uint8_t prevButtonState;
    uint8_t menuOption;
    bool menuRestart;
};



#endif
