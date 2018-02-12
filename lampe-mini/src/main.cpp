#include "FastLED.h"
#include "Arduino.h"
#include "Programs.h"
#include "Lampe.h"

FASTLED_USING_NAMESPACE

Lampe lampe;

void setup() {
  Serial.begin(115200);
  delay(1000); // 3 second delay for recovery
}


void loop() {
  rainbow(lampe);
  //selectProgram(lampe.nextMenuOptionOnClick(), lampe);
  lampe.update();
}
