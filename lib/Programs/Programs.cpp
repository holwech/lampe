#include "Programs.h"

void init(Lampe& Lampe, Tlc5940& Tlc) {
	Lampe.setLight(2, mainBase);
	Tlc.update();
	delay(200);
	Lampe.setLight(3, mainBase);
	Tlc.update();
	delay(200);
	Lampe.setLight(4, mainBase);
	Tlc.update();
	delay(200);
	Lampe.setLight(1, mainSelect);
	Tlc.update();
	delay(200);
	Lampe.setLight(0, mainTouchInactive);
	Tlc.update();
	delay(200);
}

void mainMenu(Lampe& Lampe, Tlc5940&  Tlc, State& State) {
	Lampe.updateTouch(0);
	if (Lampe.touch(0)) {
		Lampe.setLight(0, mainTouchActive);
		if (Lampe.hold(0)) {
			State.setState(OFF);
			menuTransitionOff(Lampe, Tlc, State);
			return;
		}
	} else {
		Lampe.setLight(0, mainTouchInactive);
	}
	if (Lampe.click(0)) {
		Lampe.setLight(State.getMenuOption(), mainBase);
		Lampe.setLight(State.nextMenuOption(), mainSelect);
		if (State.menuRestarted()) {
			State.setState(SECOND_MENU);
			Lampe.setLight(0, secondTouchInactive);
			Lampe.setLight(1, secondSelect);
			Lampe.setLight(2, secondBase);
			Lampe.setLight(3, secondBase);
			Lampe.setLight(4, secondBase);
			return;
		}
	}	else if (Lampe.longClick(0)) {
		Tlc.clear();
		Tlc.update();
		State.setState(COOL_LIGHTS);
		return;
	}
	Tlc.update();
}

void secondMenu(Lampe& Lampe, Tlc5940&  Tlc, State& State) {
	Lampe.updateTouch(0);
	if (Lampe.touch(0)) {
		Lampe.setLight(0, secondTouchActive);
		if (Lampe.hold(0)) {
			State.setState(OFF);
			menuTransitionOff(Lampe, Tlc, State);
			return;
		}
	} else {
		Lampe.setLight(0, secondTouchInactive);
	}
	if (Lampe.click(0)) {
		Lampe.setLight(State.getMenuOption(), secondBase);
		Lampe.setLight(State.nextMenuOption(), secondSelect);
		if (State.menuRestarted()) {
			State.setState(MAIN_MENU);
			Lampe.setLight(0, mainTouchInactive);
			Lampe.setLight(1, mainSelect);
			Lampe.setLight(2, mainBase);
			Lampe.setLight(3, mainBase);
			Lampe.setLight(4, mainBase);
			return;
		}
	}	else if (Lampe.longClick(0)) {
		Tlc.clear();
		Tlc.update();
		State.setState(COOL_LIGHTS);
		return;
	}
	Tlc.update();
}

void menuTransitionOff(Lampe& Lampe, Tlc5940& Tlc, State& State) {
	Lampe.setLight(State.nextMenuOption(), off);
	Tlc.update();
	delay(200);
	Lampe.setLight(State.nextMenuOption(), off);
	Tlc.update();
	delay(200);
	Lampe.setLight(State.nextMenuOption(), off);
	Tlc.update();
	delay(200);
	Lampe.setLight(State.nextMenuOption(), off);
	Tlc.update();
	delay(200);
	Lampe.setLight(0, off);
	Tlc.update();
	delay(200);
}
