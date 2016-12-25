#ifndef CONFIG_H
#define CONFIG_H

enum Programs{COOL_LIGHTS, FLOW, FLOW_DIMMED, SINGLE_COLOR};

struct Color {
	int red, green, blue;
	Color(int red, int green, int blue) :
				red(red), green(green), blue(blue) {};
};

extern Color off;

extern Color mainBase;
extern Color mainSelect;
extern Color mainTouchInactive;
extern Color mainTouchActive;
extern Color mainTouchSelected;

extern Color secondBase;
extern Color secondSelect;
extern Color secondTouchInactive;
extern Color secondTouchActive;
extern Color secondTouchSelected;


#endif
