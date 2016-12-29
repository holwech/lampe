#ifndef STATE_H
#define STATE_H

#include <Arduino.h>
#include <Config.h>


enum States{INIT, MAIN_MENU, SECOND_MENU, PROGRAMS, OFF, DEB, TEST, MIC};

class State
{
	public:
		State();
		void reset();
		enum States getState();	
		void setState(enum States newState);
		unsigned long lightTimer[5];
		unsigned long loopTimer;
		unsigned long timer;
		uint8_t nextMenuOption();
		void incrementMenu();
		uint8_t getMenuOption();
		enum Programs getProgram();
		void setProgram(enum Programs program);
		bool menuRestarted();
		bool isFirstRun();
		void randomizeCount();
		int nextShuffle();
		void cycleBPM();
		int getBPM();
		int tc(int light, int color, int min, int max);
	private:
		uint8_t menuOption;
		uint8_t lightMenuOption;
		enum States state;
		enum Programs program;
		bool menuRestart;
		int count[15];
		int countDirection[15];
		int shuffledOrder[5];
		int shuffleCount;
		int prevRandLight;
		int bpm;
		bool firstRun;
};

#endif
