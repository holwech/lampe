#include "Programs.h"

void init(Lampe& Lampe, Tlc5940& Tlc) {
	Lampe.setLight(2, -1, 255, -1);
	Tlc.update();
	delay(200);
	Lampe.setLight(3, -1, 255, -1);
	Tlc.update();
	delay(200);
	Lampe.setLight(4, -1, 255, -1);
	Tlc.update();
	delay(200);
	Lampe.setLight(1, -1, -1, 255);
	Tlc.update();
	delay(200);
	Lampe.setLight(0, 255, -1, -1);
	Tlc.update();
	delay(200);
}

void mainMenu(Lampe& Lampe, Tlc5940&  Tlc, State& State) {
	Lampe.updateTouch(0);
	if (Lampe.touch(0)) {
		Lampe.setLight(0, 0, 255, 0);
		if (Lampe.hold(0)) {
			State.setState(OFF);
			mainMenuTransistionOff(Lampe, Tlc, State);
			return;
		}
	} else {
		Lampe.setLight(0, 255, 0, 0);
	}
	if (Lampe.click(0)) {
		Lampe.setLight(State.getMainMenuOption(), 0, 255, 0);
		Lampe.setLight(State.nextMainMenuOption(), 0, 0, 255);
	}	else if (Lampe.longClick(0)) {
		Tlc.clear();
		Tlc.update();
		State.setState(COOL_LIGHTS);
		return;
	}
	Tlc.update();
}

void mainMenuTransistionOff(Lampe& Lampe, Tlc5940& Tlc, State& State) {
	Lampe.setLight(State.nextMainMenuOption(), 0, 0, 0);
	Tlc.update();
	delay(200);
	Lampe.setLight(State.nextMainMenuOption(), 0, 0, 0);
	Tlc.update();
	delay(200);
	Lampe.setLight(State.nextMainMenuOption(), 0, 0, 0);
	Tlc.update();
	delay(200);
	Lampe.setLight(State.nextMainMenuOption(), 0, 0, 0);
	Tlc.update();
	delay(200);
	Lampe.setLight(0, 0, 0, 0);
	Tlc.update();
	delay(200);
}
