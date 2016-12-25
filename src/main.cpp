#include <Tlc5940.h>
#include <tlc_config.h>
#include <Lampe.h>
#include <Arduino.h>
#include <CapacitiveSensor.h>
#include <State.h>
#include <Programs.h>
#include <Config.h>

#define DEBUG true

Lampe Lampe;
unsigned long printTimer = millis();
unsigned long loopTimer;
int max;
State State;
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
		Serial.println(millis() - State.loopTimer);
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
		Serial.print("State: ");
		Serial.println(State.getState());
		printTimer = millis();
	}
}

void setup() {
  Tlc.init(0);
	randomSeed(analogRead(0));
	Serial.begin(115200);
}


void loop() {
	State.loopTimer = millis();
	printDebug();
	switch (State.getState()) {
	case INIT:
		init(Lampe, Tlc);
		State.setState(MAIN_MENU);
		break;
	case MAIN_MENU:
		mainMenu(Lampe, Tlc, State);
		break;
	case SECOND_MENU:
		secondMenu(Lampe, Tlc, State);
		break;
	case PROGRAMS:
		Lampe.updateTouch(0);
		programs(Lampe, Tlc, State);
		break;
	case OFF:
		Lampe.updateTouch(0);
		if (Lampe.click(0)) {
			State.setState(INIT);
			break;
		}
		Tlc.clear();
		Tlc.update();
		break;
	case DEB:
		Lampe.updateTouch(0);
		Serial.print(Lampe.click(0));
		Serial.print(Lampe.longClick(0));
		Serial.print(Lampe.hold(0));
		Serial.println(Lampe.readCS(0,50));
		break;
	case TEST:
		Lampe.setLight(0, 255, 0, 0);
		Lampe.setLight(1, 255, 0, 0);
		Lampe.setLight(2, 255, 0, 0);
		Lampe.setLight(3, 255, 0, 0);
		Lampe.setLight(4, 255, 0, 0);
		Tlc.update();
		break;
	}
}
