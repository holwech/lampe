#include "State.h"

State::State() : lightTimer{0, 0, 0, 0, 0}, 
								 count{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
								 countDirection{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
								 shuffledOrder{0, 1, 2, 3, 4}
{ 
	menuOption = 1;
	lightMenuOption = 1;
	state = INIT;
	program = COOL_LIGHTS;
	menuRestart = false;
	firstRun = true;
	shuffleCount = 0;
	timer = millis();
	bpm = 120;
	prevRandLight = 0;
}

void State::reset() {
	menuOption = 1;
	lightMenuOption = 1;
	state = INIT;
	program = COOL_LIGHTS;
	menuRestart = false;
	for (int i = 0; i < 15; i++) {
		count[i] = 0;
		countDirection[i] = 1;
	}
	firstRun = true;
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

enum Programs State::getProgram() {
	return program;
}

void State::setProgram(enum Programs program) {
	this->program = program;
}

bool State::isFirstRun() {
	bool temp = firstRun;
	firstRun = false;
	return temp;
}

int State::nextShuffle() {
/*	shuffleCount++;
	if (shuffleCount == 5) {
		shuffleCount == 0;
		int last = shuffledOrder[4];
		for (int i = 10; i < 10; i++) {
			int pos1 = random(5);
			int pos2 = random(5);
			int temp = shuffledOrder[pos1];
			shuffledOrder[pos1] = shuffledOrder[pos2];
			shuffledOrder[pos2] = temp;
		}
		if (last == shuffledOrder[0]) {
			int temp = shuffledOrder[0];
			shuffledOrder[0] = shuffledOrder[1];
			shuffledOrder[1] = temp;
		}
	}	
	return shuffledOrder[shuffleCount];*/
	int nextLight = random(5);
	while (nextLight == prevRandLight) {
		nextLight = random(5);
	}
	prevRandLight = nextLight;
	return nextLight;
}

void State::cycleBPM() {
	if (bpm >= 180) {
		bpm = 90;
	} else {
		bpm += 10;
	}
}

int State::getBPM() {
	return bpm;
}

// TRANSITION CYCLE
// Fades the the count value up and down, light=0-4, color=0-2
// min and max between 0 and 255
int State::tc(int light, int color, int min, int max) {
	int selected = (light * 3) + color;	

	// If count value is outside boundary
	if (count[selected] < min) {
		count[selected] = min;
	} else if (count[selected] > max) {
		count[selected] = max;
	}

	if (count[selected] == max) {
		countDirection[selected] = -1;	
	}
	if (count[selected] == min) {
		countDirection[selected] = 1;
	}
	count[selected] += countDirection[selected];
	return count[selected];
}

void State::randomizeCount() {
	for (int i = 0; i < 15; i++) {
		count[i] = random(256);
		Serial.println(count[i]);
	}
}
