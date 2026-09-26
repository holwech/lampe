#include <Arduino.h>
#include <Hardware.h>
#include <Lampe.h>

Lampe lampe;

void setup() {
    Serial.begin(Hardware::SerialBaud);
    delay(1000); // One-second startup delay before driving the LEDs.
    lampe.begin();
}

void loop() {
    lampe.update();
}
