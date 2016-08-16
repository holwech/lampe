#ifndef LAMPE_H
#define LAMPE_H
#define NUM_LIGHTS 5

#include <Arduino.h>
#include <Tlc5940.h> 
#include <CapacitiveSensor.h>

class Lampe
{
	public:
		Lampe();
		void setLight(int light, int red, int green, int blue);
		bool touch(int light);
		bool click(int light);
		bool longClick(int light);
		bool hold(int light);
		void updateAllTouch();
		void updateTouch(int light);
		int readCS(int light, int samples);
	private:
		bool touchList[NUM_LIGHTS];
		bool clickList[NUM_LIGHTS];
		bool longClickList[NUM_LIGHTS];
		bool holdList[NUM_LIGHTS];
		unsigned long clickTimer[NUM_LIGHTS];
		unsigned long longClickTimer[NUM_LIGHTS];
		unsigned long holdTimer[NUM_LIGHTS];
		unsigned long touchTimer[NUM_LIGHTS];
		int touchThreshold;
		CapacitiveSensor CS[NUM_LIGHTS];
};

#endif
