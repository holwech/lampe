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
		Serial.print("State: ");
		Serial.println(state.getState());
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
		Lampe.setLight(0, 4095, -1, -1);
		Lampe.setLight(1, -1, -1, 4095);
		Lampe.setLight(2, -1, 4095, -1);
		Lampe.setLight(3, -1, 4095, -1);
		Lampe.setLight(4, -1, 4095, -1);
		Tlc.update();
		delay(100);
		state.setState(MENU);
		break;
	case MENU:
		if (Lampe.touch(0)) {
			Lampe.setLight(0, 0, 4095, 0);
			if (Lampe.hold(0)) {
				state.setState(OFF);
				break;
			}
		} else {
			Lampe.setLight(0, 4095, 0, 0);
		}
		if (Lampe.click(0)) {
			Lampe.setLight(option, 0, 4095, 0);
			option++;
			if (option > 4) {
				option = 1;
			}
			Lampe.setLight(option, 0, 0, 4095);
		}	else if (Lampe.longClick(0)) {
			Tlc.clear();
			Tlc.update();
			state.setState(COOL_LIGHTS);
			break;
		}
		Tlc.update();
		break;
	case COOL_LIGHTS:
		if (Lampe.click(0)) {
			Lampe.setLight(0, 4095, 0, 0);
			Lampe.setLight(1, 0, 0, 4095);
			Lampe.setLight(2, 0, 4095, 0);
			Lampe.setLight(3, 0, 4095, 0);
			Lampe.setLight(4, 0, 4095, 0);
			Tlc.update();
			state.setState(MENU);
			break;
		}
		if ((loopTimer - state.lightTimer[0]) > 500) {
			int light	= rand() % 5;
			int red = rand() % 4096;
			int green = rand() % 4096;
			int blue = rand() % 4096;
			Lampe.setLight(light, red, green, blue); 
			Tlc.update();
			state.lightTimer[0] = loopTimer;
		}
		break;
	case OFF:
		if (Lampe.click(0)) {
			state.setState(INIT);
			break;
		}
		Tlc.clear();
		Tlc.update();
		break;
	case DEB:
		Serial.print(Lampe.click(0));
		Serial.print(Lampe.longClick(0));
		Serial.print(Lampe.hold(0));
		Serial.println(Lampe.readCS(0,50));
		break;
	}
}
