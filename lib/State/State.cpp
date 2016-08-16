#include "State.h"

State::State() : lightTimer{0, 0, 0, 0, 0} {
	state = INIT;
}

enum States State::getState() {
	return state;		
}

void State::setState(enum States newState) {
	state = newState;
} 
