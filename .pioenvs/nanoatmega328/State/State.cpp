#include "State.h"

State::State() {
	state = INIT;
}

enum States State::getState() {
	return state;		
}

void State::setState(enum States newState) {
	state = newState;
} 
