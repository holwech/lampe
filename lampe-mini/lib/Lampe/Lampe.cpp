#include "Lampe.h"


Lampe::Lampe() {
  prevButtonState = LOW;
  timer = millis();
  menuOption = 0;
  menuRestart = false;

  pinMode(2, INPUT); // Button sensor

  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  uint8_t gHue = 0;
  uint8_t num_leds = NUM_LEDS;
}

uint8_t Lampe::testgHue() {
  return gHue;
}

uint8_t Lampe::testnum_leds() {
  return num_leds;
}

void Lampe::update() {
  FastLED.show();  
  FastLED.delay(1000/FRAMES_PER_SECOND); 
}

uint8_t Lampe::nextMenuOptionOnClick() {
  if (buttonClick()) {
    return nextMenuOption();
  }
}

bool Lampe::buttonClick() {
  int buttonState = digitalRead(2);
  if (buttonState == LOW && prevButtonState == HIGH) {
    prevButtonState = LOW;
    return true;
  } else if (buttonState == HIGH) {
    prevButtonState = HIGH;
  }
  return false;
}


uint8_t Lampe::nextMenuOption() {
	incrementMenu();
	return menuOption;
}

void Lampe::incrementMenu() {
	if (menuOption >= 2) {
		menuOption = 0;
		menuRestart = true;
	} else {
		menuOption++;
		menuRestart = false;
	}
}


unsigned long Lampe::getTimer() {
  return millis() - timer;
}

void Lampe::resetTimer() {
  timer = millis();
}
