#include "Programs.h"

void exitProgram(Lampe&, State&);

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

// This function controls the first page of the lamp menu
void mainMenu(Lampe& Lampe, Tlc5940&  Tlc, State& State) {
	Lampe.updateTouch(0);
	if (Lampe.touch(0)) {
		// Change color of the top light if being touched
		if (Lampe.longTouch(0)) {
			Lampe.setLight(0, mainTouchSelected);
		} else {
			Lampe.setLight(0, mainTouchActive);
		}

		if (Lampe.hold(0)) {
			// If there is a hold click, turn of lamp
			State.setState(OFF);
			menuTransitionOff(Lampe, Tlc, State);
			return;
		}
	} else {
		// If not touched, set to inactive color on top
		Lampe.setLight(0, mainTouchInactive);
	}
	if (Lampe.click(0)) {
		// Cycle through menu with a click
		Lampe.setLight(State.getMenuOption(), mainBase);
		Lampe.setLight(State.nextMenuOption(), mainSelect);
		if (State.menuRestarted()) {
			// If the menu has been cycled through, go to second menu
			State.setState(SECOND_MENU);
			Lampe.setLight(0, secondTouchInactive);
			Lampe.setLight(1, secondSelect);
			Lampe.setLight(2, secondBase);
			Lampe.setLight(3, secondBase);
			Lampe.setLight(4, secondBase);
			return;
		}
	}	else if (Lampe.longClick(0)) {
		// Long click menu selection
		Tlc.clear();
		Tlc.update();
		State.setState(PROGRAMS);
		switch(State.getMenuOption()) {
		case 1:
			State.setProgram(COOL_LIGHTS);
			break;
		case 2:
			State.setProgram(FLOW);
			break;
		case 3:
			State.setProgram(FLOW_DIMMED);
			break;
		case 4:
			State.setProgram(SINGLE_COLOR);
			break;
		}
		return;
	}
	Tlc.update();
}

// This function controls the second page of the lampe menu
void secondMenu(Lampe& Lampe, Tlc5940&  Tlc, State& State) {
	Lampe.updateTouch(0);
	if (Lampe.touch(0)) {
		// Change color of the top light if being touched
		if (Lampe.longTouch(0)) {
			Lampe.setLight(0, secondTouchSelected);
		} else {
			Lampe.setLight(0, secondTouchActive);
		}
		if (Lampe.hold(0)) {
			// If there is a hold click, turn of lamp
			State.setState(OFF);
			menuTransitionOff(Lampe, Tlc, State);
			return;
		}
	} else {
		// If not touched, set to inactive color on top
		Lampe.setLight(0, secondTouchInactive);
	}
	if (Lampe.click(0)) {
		// Cycle through menu with a click
		Lampe.setLight(State.getMenuOption(), secondBase);
		Lampe.setLight(State.nextMenuOption(), secondSelect);
		if (State.menuRestarted()) {
			// If the menu has been cycled through, go to main menu
			State.setState(MAIN_MENU);
			Lampe.setLight(0, mainTouchInactive);
			Lampe.setLight(1, mainSelect);
			Lampe.setLight(2, mainBase);
			Lampe.setLight(3, mainBase);
			Lampe.setLight(4, mainBase);
			return;
		}
	}	else if (Lampe.longClick(0)) {
		// Long click menu selection
		Tlc.clear();
		Tlc.update();
		State.setState(PROGRAMS);
		switch(State.getMenuOption()) {
		case 1:
			State.setProgram(COOL_LIGHTS);
			break;
		case 2:
			State.setProgram(FLOW);
			break;
		case 3:
			State.setProgram(FLOW);
			break;
		case 4:
			State.setProgram(FLOW);
			break;
		}
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


void programs(Lampe& Lampe, Tlc5940&  Tlc, State& State) {
	switch(State.getProgram()) {
	case COOL_LIGHTS:
		if (Lampe.click(0)) {
			exitProgram(Lampe, State);
			break;
		}
		if ((State.timer - State.lightTimer[0]) > 200) {
			int light	= rand() % 5;
			int red = rand() % 256;
			int green = rand() % 256;
			int blue = rand() % 256;
			Lampe.setLight(light, red, green, blue); 
			Tlc.update();
			State.lightTimer[0] = millis();
		}
		break;
	case FLOW:
		if (Lampe.click(0)) {
			exitProgram(Lampe, State);
			break;
		}
		if (State.isFirstRun()) {
			State.randomizeCount();
		}
		if (State.timer > 25) {
			Lampe.setLight(0, State.tc(0, 0, 0,	255), State.tc(0, 1, 0, 150), State.tc(0, 2, 150, 255));
			Lampe.setLight(1, State.tc(1, 0, 0,	150), State.tc(1, 1, 0, 255), State.tc(1, 2, 100, 255));
			Lampe.setLight(2, State.tc(2, 0, 150, 255), State.tc(2, 1, 0, 200), State.tc(2, 2, 0, 255));
			Lampe.setLight(3, State.tc(3, 0, 200, 255), State.tc(3, 1, 0, 155), State.tc(3, 2, 0, 180));
			Lampe.setLight(4, State.tc(4, 0, 0,	150), State.tc(4, 1, 0, 205), State.tc(4, 2, 0, 255));
			Tlc.update();
			State.timer = millis();
		}
		break;
	case FLOW_DIMMED:
		if (Lampe.click(0)) {
			exitProgram(Lampe, State);
			break;
		}
		if (State.isFirstRun()) {
			State.randomizeCount();
		}
		if (State.timer > 25) {
			Lampe.setLight(0, State.tc(0, 0, 0,	100), State.tc(0, 1, 0, 100), State.tc(0, 2, 50, 100));
			Lampe.setLight(1, State.tc(1, 0, 0,	50), State.tc(1, 1, 0, 100), State.tc(1, 2, 10, 100));
			Lampe.setLight(2, State.tc(2, 0, 50, 100), State.tc(2, 1, 0, 20), State.tc(2, 2, 0, 100));
			Lampe.setLight(3, State.tc(3, 0, 20, 100), State.tc(3, 1, 0, 55), State.tc(3, 2, 0, 180));
			Lampe.setLight(4, State.tc(4, 0, 0,	50), State.tc(4, 1, 0, 100), State.tc(4, 2, 0, 100));
			Tlc.update();
			State.timer = millis();
		}
		break;
	case SINGLE_COLOR:
		if (Lampe.longClick(0)) {
			exitProgram(Lampe, State);
			break;
		}
		if (Lampe.click(0)) {
			State.cycleBPM();
		}
		if ((millis() - State.timer) > (30000 / State.getBPM())) {
			Tlc.clear();
			Lampe.setLight(State.nextShuffle(), random(256), random(256), random(256));
			Tlc.update();
			State.timer = millis();
		}
		break;
	}
}

void exitProgram(Lampe& Lampe, State& State) {
	Lampe.setLight(0, mainTouchInactive);
	Lampe.setLight(1, mainSelect);
	Lampe.setLight(2, mainBase);
	Lampe.setLight(3, mainBase);
	Lampe.setLight(4, mainBase);
	Tlc.update();
	State.setState(MAIN_MENU);
	State.reset();
}
