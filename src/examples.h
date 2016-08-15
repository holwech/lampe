#include <Tlc5940.h>
#include <tlc_config.h>
#include <Lampe.h>
#include <Arduino.h>
#include <CapacitiveSensor.h>

Lampe Lampe;
const int conf = 9;
int prevLight = -1;
int average = 0;
CapacitiveSensor cs_4_2 = CapacitiveSensor(2,4);
const int sampleWindow = 50; // Sample window width in mS (50 mS = 20Hz)
unsigned int sample;
void setup() {
  Tlc.init(0);
	Serial.begin(115200);
}



void loop() {
  if (conf == 1) {
    for(int i = 0; i < 4095; i++) {
      Lampe.setLight(0, 0, i);
      Lampe.setLight(1, 1, i);
      Lampe.setLight(2, 2, i);
      Lampe.setLight(3, 1, i);
      Lampe.setLight(4, 0, i);
      Tlc.update();
      delay(1);
    }
    for(int i = 4095; i > 0; i--) {
      Lampe.setLight(0, 0, i);
      Lampe.setLight(1, 1, i);
      Lampe.setLight(2, 2, i);
      Lampe.setLight(3, 1, i);
      Lampe.setLight(4, 0, i);
      Tlc.update();
      delay(1);
    }
  } else if (conf == 2) {
    Lampe.setLight(0, 4, 4095);
    Tlc.update();
    delay(500);
    Lampe.setLight(0, 4, 0);
    Lampe.setLight(2, 4, 4095);
    Tlc.update();
    delay(500);
    Lampe.setLight(2, 4, 0);
    Lampe.setLight(4, 4, 4095);
    Tlc.update();
    delay(500);
    Lampe.setLight(4, 4, 0);
    Lampe.setLight(3, 4, 4095);
    Tlc.update();
    delay(500);
    Lampe.setLight(3, 4, 0);
    Lampe.setLight(1, 4, 4095);
    Tlc.update();
    delay(500);
    Lampe.setLight(1, 4, 0);
	} else if (conf == 3) {
		Lampe.setLight(0, 0, 4095);
		Lampe.setLight(1, 2, 4095);
		Lampe.setLight(2, 1, 4095);
		Lampe.setLight(3, 1, 4095);
		Lampe.setLight(4, 1, 4095);
		Tlc.update();
		delay(200);
		Tlc.clear();
		Lampe.setLight(0, 0, 4095);
		Lampe.setLight(1, 1, 4095);
		Lampe.setLight(2, 2, 4095);
		Lampe.setLight(3, 1, 4095);
		Lampe.setLight(4, 1, 4095);
		Tlc.update();
		delay(200);
		Tlc.clear();
		Lampe.setLight(0, 0, 4095);
		Lampe.setLight(1, 1, 4095);
		Lampe.setLight(2, 1, 4095);
		Lampe.setLight(3, 2, 4095);
		Lampe.setLight(4, 1, 4095);
		Tlc.update();
		delay(200);
		Tlc.clear();
		Lampe.setLight(0, 0, 4095);
		Lampe.setLight(1, 1, 4095);
		Lampe.setLight(2, 1, 4095);
		Lampe.setLight(3, 1, 4095);
		Lampe.setLight(4, 2, 4095);
		Tlc.update();
		delay(200);
		Tlc.clear();
	} else if (conf == 4) {
		Tlc.clear();
		Lampe.setLight(0, 0, 4095);
		Lampe.setLight(1, 2, 4095);
		Lampe.setLight(2, 2, 4095);
		Lampe.setLight(3, 2, 4095);
		Lampe.setLight(4, 2, 4095);
		Tlc.update();
		delay(5000);
		Tlc.clear();
	} else if (conf == 5) {
		int randNum = rand() % 5;
		int randColor = rand() % 3;
		while (randNum == prevLight) {
			randNum = rand() % 5;
		}	
		prevLight = randNum;
		Tlc.clear();
		Lampe.setLight(randNum, randColor, 4095);
		Tlc.update();
		delay(11);
	} else if (conf == 6) {
	  long start = millis();
    long total1 =  cs_4_2.capacitiveSensor(5);
		int val = (total1 + average) / 2;
		Lampe.setLight(0,3,val * 30);
		Lampe.setLight(1,3,val * 30);
		Lampe.setLight(2,3,val * 30);
		Lampe.setLight(3,3,val * 30);
		Lampe.setLight(4,3,val * 30);
		Tlc.update();

    Serial.println(millis() - start);
    Serial.print(total1);
    Serial.print("\n\n");
	} else if (conf == 7) {
	
		unsigned long startMillis= millis();  // Start of sample window
		unsigned int peakToPeak = 0;   // peak-to-peak level
		
		unsigned int signalMax = 0;
		unsigned int signalMin = 1024;
		
		// collect data for 50 mS
		while (millis() - startMillis < sampleWindow)
		{
			 sample = analogRead(0);
			 if (sample < 3250)  // toss out spurious readings
			 {
					if (sample > signalMax)
					{
						 signalMax = sample;  // save just the max levels
					}
					else if (sample < signalMin)
					{
						 signalMin = sample;  // save just the min levels
					}
			 }
		}
		peakToPeak = signalMax - signalMin;  // max - min = peak-peak amplitude
		Serial.println(peakToPeak);		
		int intensity = peakToPeak * 10;
		Serial.println(peakToPeak);
	} else if (conf == 8) {
    long total1 =  cs_4_2.capacitiveSensor(50);
		Tlc.clear();
		if (total1 > 150) {
			Lampe.setLight(0,2,555);
			Lampe.setLight(0,1,4095);
			Lampe.setLight(1,2,4095);
			Lampe.setLight(2,2,4095);
			Lampe.setLight(3,2,4095);
			Lampe.setLight(4,2,4095);
		} else {
			Lampe.setLight(0,0,4095);
			Lampe.setLight(1,0,4095);
			Lampe.setLight(2,0,4095);
			Lampe.setLight(3,0,4095);
			Lampe.setLight(4,0,4095);
		}
		Tlc.update();
	} else if (conf == 9) {
		int intensityRed = rand() % 4095;
		int intensityGreen = rand() % 4095;
		int intensityBlue = rand() % 4095;
		int light = rand() % 5;
		while (light == prevLight) {
			light = rand() % 5;
		}
		Lampe.setLight(light, 0, intensityRed);
		Lampe.setLight(light, 1, intensityGreen);
		Lampe.setLight(light, 2, intensityBlue);
		Tlc.update();
		delay(200);
	}
}

