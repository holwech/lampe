#include "State.h"

State::State() : lightTimer{0, 0, 0, 0, 0} { 
	mainMenuOption = 1;
	lightMenuOption = 1;
	state = INIT;
}

void State::reset() {
	mainMenuOption = 1;
	lightMenuOption = 1;
	state = INIT;
}

enum States State::getState() {
	return state;		
}

void State::setState(enum States newState) {
	state = newState;
} 


uint8_t State::nextMainMenuOption() {
	if (mainMenuOption >= 4) {
		mainMenuOption = 1;
	} else {
		mainMenuOption++;
	}
	return mainMenuOption;
}

void State::incrementMainMenu() {
	if (mainMenuOption >= 4) {
		mainMenuOption = 1;
	} else {
		mainMenuOption++;
	}
}

uint8_t State::getMainMenuOption() {
	return mainMenuOption;
}
