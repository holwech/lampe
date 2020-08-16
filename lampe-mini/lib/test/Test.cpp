#include "Test.h"

Test::Test() {
    FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(BRIGHTNESS);

    gHue = 0;
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    num_leds = NUM_LEDS;
}


void Test::solid_color() {
    fill_solid(leds, num_leds, CRGB(120, 120, 120));
    FastLED.show();  
}

void Test::blink_loop() {
    for(int dot = 0; dot < NUM_LEDS; dot++) { 
        leds[dot] = CRGB::Blue;
        FastLED.show();
        // clear this led for the next time around the loop
        leds[dot] = CRGB::Black;
        delay(30);
    }
}