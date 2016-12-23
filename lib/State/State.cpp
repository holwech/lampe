#include "State.h"

State::State() : lightTimer{0, 0, 0, 0, 0}, 
								 count{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
								 countDirection{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
{ 
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

// Fades the the count value up and down, light=0-4, color=0-2
int State::transitionCycle(int light, int color, int min, int max) {
	int selected = (light + 1) * (color + 1) - 1;	
	if (countDirection[selected] == 1) {
		if (count[selected] <= max) {
			count[selected]++;
			return count[selected];
		} else {
			countDirection[selected] = -1;
			count[selected]--;
			return count[selected];
		}
	} else {
		if (count[selected] >= min) {
			count[selected]--;
			return count[selected];
		} else {
			countDirection[selected] = 1;
			count[selected]--;
			return count[selected];
		}
	}
}
