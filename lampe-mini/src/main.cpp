#include "FastLED.h"
#include "Arduino.h"
#include "Test.h"

FASTLED_USING_NAMESPACE

Test test;

void setup() {
    Serial.begin(115200);
    delay(1000); // 3 second delay for recovery
}


void loop() {
    test.blink_loop();    
}
