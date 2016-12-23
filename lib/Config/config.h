#ifndef CONFIG_H
#define CONFIG_H

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

extern Color secondBase;
extern Color secondSelect;
extern Color secondTouchInactive;
extern Color secondTouchActive;


#endif
