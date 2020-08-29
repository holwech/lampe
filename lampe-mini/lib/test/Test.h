#ifndef TEST_H
#define TEST_H

#include "FastLED.h"
#include "Arduino.h"

FASTLED_USING_NAMESPACE

#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif

#define DATA_PIN    3
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS    16
#define BRIGHTNESS         100
#define FRAMES_PER_SECOND  120
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))


class Test
{
  public:
    Test();
    void solid_color();
    void blink_loop();
    uint8_t gHue;
    uint8_t num_leds;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    CRGB leds[NUM_LEDS];
};



#endif
