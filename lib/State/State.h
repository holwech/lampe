#ifndef STATE_H
#define STATE_H

#include <Arduino.h>

enum States{INIT, MENU, OFF, DEB, COOL_LIGHTS};

class State
{
	public:
		State();
		enum States getState();	
		void setState(enum States newState);
		unsigned long lightTimer[5];
	private:
		enum States state;
};

#endif
