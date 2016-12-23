#ifndef STATE_H
#define STATE_H

#include <Arduino.h>

enum States{INIT, MAIN_MENU, SECOND_MENU, OFF, DEB, COOL_LIGHTS, TEST};

class State
{
	public:
		State();
		void reset();
		enum States getState();	
		void setState(enum States newState);
		unsigned long lightTimer[5];
		unsigned long loopTimer;
		uint8_t nextMenuOption();
		void incrementMenu();
		uint8_t getMenuOption();
		bool menuRestarted();
	private:
		uint8_t menuOption;
		uint8_t lightMenuOption;
		enum States state;
		bool menuRestart;
};

#endif
