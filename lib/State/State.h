#ifndef STATE_H
#define STATE_H

#include <Arduino.h>

enum States{INIT, MAIN_MENU, OFF, DEB, COOL_LIGHTS};

class State
{
	public:
		State();
		void reset();
		enum States getState();	
		void setState(enum States newState);
		unsigned long lightTimer[5];
		unsigned long loopTimer;
		uint8_t nextMainMenuOption();
		void incrementMainMenu();
		uint8_t getMainMenuOption();
	private:
		uint8_t mainMenuOption;
		uint8_t lightMenuOption;
		enum States state;
};

#endif
