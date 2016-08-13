#ifndef LAMPE_H
#define LAMPE_H

#include <Arduino.h>
#include <Tlc5940.h> 

class Lampe
{
	public:
		void setLight(int light, int color, int value);
};

#endif
