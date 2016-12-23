#ifndef STATE_H
#define STATE_H

#include <Arduino.h>

enum States{INIT, MAIN_MENU, SECOND_MENU, PROGRAMS, OFF, DEB, TEST};

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
		int transitionCycle(int light, int color, int min, int max);
	private:
		uint8_t menuOption;
		uint8_t lightMenuOption;
		enum States state;
		bool menuRestart;
		int count[15];
		int countDirection[15];
};

#endif
