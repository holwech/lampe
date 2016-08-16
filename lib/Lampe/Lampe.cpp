#include "Lampe.h"

Lampe::Lampe() : touchList{false, false, false, false, false},
								 touchTimer{0, 0, 0, 0, 0},
							 	 CS{
									 CapacitiveSensor(2,4),
									 CapacitiveSensor(2,5),
									 CapacitiveSensor(2,6),
									 CapacitiveSensor(2,7),
									 CapacitiveSensor(2,8)
								 } {
	touchThreshold = 300;	
}

bool Lampe::touch(int light) {
	return touchList[light];
}


bool Lampe::click(int light) {
	bool click = clickList[light];
	if (clickList[light]) {
		clickList[light] = false;
	}
	return click;
}

bool Lampe::longClick(int light) {
	bool longClick = longClickList[light];
	if (longClickList[light]) {
		longClickList[light] = false;
	}
	return longClick;
}

bool Lampe::hold(int light) {
	return holdList[light];
}


void Lampe::updateAllTouch() {
	updateTouch(0);
	updateTouch(1);
	updateTouch(2);
	updateTouch(3);
	updateTouch(4);
}

// Needs to be optimized
void Lampe::updateTouch(int light) { 
	unsigned long currentTime = millis();
	unsigned long timeDiff = currentTime - touchTimer[light];
	if ((timeDiff) > 50) {
		int touchVal = CS[light].capacitiveSensor(50); 
		if (touchVal > touchThreshold && !touchList[light]) {
			touchList[light] = true;
			touchTimer[light] = currentTime; 
			holdTimer[light] = currentTime;
		} else if (touchVal < (touchThreshold - 100) && touchList[light]) {
			touchList[light] = false;
			touchTimer[light] = currentTime; 
			holdList[light] = false;	
			if (timeDiff < 1500) {
				clickList[light] = true;
				clickTimer[light] = currentTime;
			} else if (timeDiff < 3000) {
				longClickList[light] = true;
				longClickTimer[light] = currentTime;
			}
		}
		if (clickList[light] && (currentTime - clickTimer[light]) > 1500) {
			clickList[light] = false;
		}
		if (longClickList[light] && (currentTime - longClickTimer[light]) > 3000) {
			longClickList[light] = false;
		}
		if (touchList[light] && (currentTime - holdTimer[light]) > 3000) {
			holdList[light] = true;
		}
	}
}

int Lampe::readCS(int light, int samples) {
	int reading = CS[light].capacitiveSensor(samples); 
	return reading;
}

void Lampe::setLight(int light, int red, int green, int blue) {
	if (red != -1){
		Tlc.set((light * 9), red);
		Tlc.set((light * 9) + 1, red);
		Tlc.set((light * 9) + 2, red);
	}
	if (green != -1){
		Tlc.set((light * 9) + 3, green);
		Tlc.set((light * 9) + 4, green);
		Tlc.set((light * 9) + 5, green);
	}
	if (blue != -1){
		Tlc.set((light * 9) + 6, blue);
		Tlc.set((light * 9) + 7, blue);
		Tlc.set((light * 9) + 8, blue);
	}
}

