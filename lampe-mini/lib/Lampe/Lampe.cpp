#include "Lampe.h"


Lampe::Lampe() {
  prevButtonState = LOW;
  timer = millis();
  menuOption = 0;
  menuRestart = false;
  linearReduceTimer = millis();
  linearValue = 0;


  pinMode(2, INPUT); // Button sensor

  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  gHue = 0;
  num_leds = NUM_LEDS;
}

void Lampe::update() {
  FastLED.show();  
  FastLED.delay(1000/FRAMES_PER_SECOND); 
  updateLinearReduce();
}

void Lampe::updateLinearReduce() {
  unsigned long currTime = millis();
  if ((linearValue > 0) && ((currTime - linearReduceTimer) > 1)) {
    uint8_t updateVal = (linearValue / 50) * (linearValue / 50);
    linearValue -= (updateVal < 1) ? 1 : updateVal;
    linearReduceTimer = currTime;
  }
}

uint8_t Lampe::linearReduce(uint8_t peak) {
  if (peak > 255) {
    linearValue = 255;
  } else if (peak > linearValue) {
    linearValue = peak;
  }
  return linearValue;
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
	if (menuOption >= 3) {
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
