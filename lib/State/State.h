#ifndef STATE_H
#define STATE_H

#include <Arduino.h>

enum States{INIT, MENU};

class State
{
	public:
		State();
		enum States getState();	
		void setState(enum States newState);
	private:
		enum States state;
};

#endif
