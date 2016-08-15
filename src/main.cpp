#include <Tlc5940.h>
#include <tlc_config.h>
#include <Lampe.h>
#include <Arduino.h>
#include <CapacitiveSensor.h>
#include <State.h>

#define DEBUG true

Lampe Lampe;
unsigned long printTimer = millis();
unsigned long loopTimer;
int max;
State state;
int option = 1;

void printDebug() {
	int temp = Lampe.readCS(0,50);
	if (max < temp) {
		max = temp;
	}
	if (DEBUG && millis() - printTimer > 500) {
		Serial.println("-----");
		Serial.println("Note click/longclick/hold can only be read once");
		Serial.print("Loop time: ");
		Serial.println(millis() - loopTimer);
		Serial.print("Touch value: ");
		Serial.println(temp);
		Serial.print("Touch: ");
		Serial.println(Lampe.touch(0));
		Serial.print("Click: ");
		Serial.println(Lampe.click(0));
		Serial.print("Long click: ");
		Serial.println(Lampe.longClick(0));
		Serial.print("Hold: ");
		Serial.println(Lampe.hold(0));
		Serial.print("Max: ");
		Serial.println(max);
		printTimer = millis();
	}
}

void setup() {
  Tlc.init(0);
	Serial.begin(115200);
}


void loop() {
	loopTimer = millis();
	Lampe.updateTouch(0);
	printDebug();
	switch (state.getState()) {
	case INIT:
		Lampe.setLight(0,0,4095);
		Lampe.setLight(1,2,4095);
		Lampe.setLight(2,1,4095);
		Lampe.setLight(3,1,4095);
		Lampe.setLight(4,1,4095);
		Tlc.update();
		state.setState(MENU);
	case MENU:
		if (Lampe.touch(0)) {
			Lampe.setLight(0, 0, 0);
			Lampe.setLight(0, 1, 4095);		
			Lampe.setLight(0, 2, 0);
			if (Lampe.hold(0)) {
				Lampe.setLight(0, 1, 0);
				Lampe.setLight(0, 2, 4095);
			}
		} else {
			Lampe.setLight(0, 0, 4095);
			Lampe.setLight(0, 1, 0);
			Lampe.setLight(0, 2, 0);
		}
		if (Lampe.click(0)) {
			Lampe.setLight(option, 0, 0);
			Lampe.setLight(option, 1, 4095);
			Lampe.setLight(option, 2, 0);
			option++;
			if (option > 4) {
				option = 1;
			}
			Lampe.setLight(option, 0, 0);
			Lampe.setLight(option, 1, 0);
			Lampe.setLight(option, 2, 4095);
		}	
		Tlc.update();
	}
}
