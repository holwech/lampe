#include "FastLED.h"
#include "Arduino.h"
#include "Programs.h"
#include "Lampe.h"

FASTLED_USING_NAMESPACE

Lampe lampe;
Mic mic;

void setup() {
  Serial.begin(115200);
  delay(1000); // 3 second delay for recovery
  rainbow(lampe);
}


void loop() {
  selectProgram(lampe.nextMenuOptionOnClick(), lampe, mic);
  lampe.update();
}