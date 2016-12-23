#include "State.h"

State::State() : lightTimer{0, 0, 0, 0, 0} { 
	menuOption = 1;
	lightMenuOption = 1;
	state = INIT;
	menuRestart = false;
}

void State::reset() {
	menuOption = 1;
	lightMenuOption = 1;
	state = INIT;
}

enum States State::getState() {
	return state;		
}

void State::setState(enum States newState) {
	state = newState;
} 


uint8_t State::nextMenuOption() {
	incrementMenu();
	return menuOption;
}

void State::incrementMenu() {
	if (menuOption >= 4) {
		menuOption = 1;
		menuRestart = true;
	} else {
		menuOption++;
		menuRestart = false;
	}
}

uint8_t State::getMenuOption() {
	return menuOption;
}

bool State::menuRestarted() {
	return menuRestart;
}
