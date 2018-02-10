#include "FastLED.h"
#include "Arduino.h"

FASTLED_USING_NAMESPACE

#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif

#define DATA_PIN    3
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS    12
CRGB leds[NUM_LEDS];
#define BRIGHTNESS         100
#define FRAMES_PER_SECOND  120

bool buttonClick();
bool buttonClickSimple();
void nextPattern();


#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
int prevButtonState = 0;

const int sampleWindow = 15; // Sample window width in mS (50 mS = 20Hz)
unsigned int sample;
uint8_t brightness = 255;

void setup() {
	Serial.begin(115200);
  delay(3000); // 3 second delay for recovery
  pinMode(2, INPUT);
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
}


typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = {mic, rainbow, confetti, sinelon, circular, circularHue, bpm, juggle };
uint8_t gCurrentPatternNumber = 0;
uint8_t gHue = 0;
  
void loop()
{
  gPatterns[gCurrentPatternNumber]();
  FastLED.show();  
  FastLED.delay(1000/FRAMES_PER_SECOND); 
  EVERY_N_MILLISECONDS( 20 ) { gHue++; }
  if (buttonClick()) {
    gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE( gPatterns);
  }
  //EVERY_N_SECONDS( 10 ) { nextPattern(); } // change patterns periodically
}

bool buttonClickSimple() {
  int buttonState = digitalRead(2);
  if (buttonState == HIGH) {
    EVERY_N_MILLISECONDS( 10 ) { gHue++; }
  }
}

bool buttonClick() {
  int buttonState = digitalRead(2);
  if (buttonState == LOW && prevButtonState == HIGH) {
    prevButtonState = LOW;
    return true;
  } else if (buttonState == HIGH) {
    prevButtonState = HIGH;
  }
  return false;
}



void nextPattern()
{
  // add one to the current pattern number, and wrap around at the end
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE( gPatterns);
}

void mic()
{
  sample = analogRead(0);
  fill_solid (leds, NUM_LEDS, CHSV(gHue, 255, floor(sample * (255.0 / 1024.0))));

  Serial.println(floor(sample * (255.0 / 1024.0)));
}


