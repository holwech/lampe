#include "Mic.h"


Mic::Mic() {
  timer = millis();
  count = 0;
  time = micros(); // Used to track rate
  up = time + SAMPLEPERIODUS;
}

// 20 - 200hz Single Pole Bandpass IIR Filter
float Mic::bassFilter(float sample) {
	static float xv[3] = {0,0,0}, yv[3] = {0,0,0};
	xv[0] = xv[1]; xv[1] = xv[2]; 
	xv[2] = sample / 9.1f;
	yv[0] = yv[1]; yv[1] = yv[2]; 
	yv[2] = (xv[2] - xv[0]) + (-0.7960060012f * yv[0]) + (1.7903124146f * yv[1]);
	return yv[2];
}

// 10hz Single Pole Lowpass IIR Filter
float Mic::envelopeFilter(float sample) { //10hz low pass
	static float xv[2] = {0,0}, yv[2] = {0,0};
	xv[0] = xv[1]; 
	xv[1] = sample / 160.f;
	yv[0] = yv[1]; 
	yv[1] = (xv[0] + xv[1]) + (0.9875119299f * yv[0]);
	return yv[1];
}

// 1.7 - 3.0hz Single Pole Bandpass IIR Filter
float Mic::beatFilter(float sample) {
	static float xv[3] = {0,0,0}, yv[3] = {0,0,0};
	xv[0] = xv[1]; xv[1] = xv[2]; 
	xv[2] = sample / 7.015f;
	yv[0] = yv[1]; yv[1] = yv[2]; 
	yv[2] = (xv[2] - xv[0]) + (-0.7169861741f * yv[0]) + (1.4453653501f * yv[1]);
	return yv[2];
}

/*
unsigned int Mic::getBPM (Lampe& Lampe, State& State) {
  unsigned short* = sample();
  for (unsigned short i = 0; i < NUM_INTERVAL; i++) {

  }
}
*/


uint8_t Mic::detectBeat(Lampe& lampe) {
  float sample, value, envelope, beat, thresh;
  uint32_t beat_times[NUM_INTERVAL];
  uint8_t beat_idx = 0;

  time = micros();

  for(count = 0;;++count){
    // Read ADC and center so +-512
    sample = (float)analogRead(0)-503.f;

    // Filter only bass component
    value = bassFilter(sample);

    // Take signal amplitude and filter
    if(value < 0)value=-value;
    envelope = envelopeFilter(value);

    Serial.println(count);
    // Every 200 samples (25hz) filter the envelope 
    if(count == 200) {
      // Filter out repeating bass sounds 100 - 180bpm
      beat = beatFilter(envelope);

      thresh = 0.1;

      // If we are above threshold, light up LED. Timer constraint so that it doesn't activate more often
      // than 210 bpm.
      Serial.print("Beat val: ");
      Serial.println(beat);
      if((beat > thresh) && (getTimer() > 280)) {
        beat_times[beat_idx] = getTimer();
        beat_idx++;
      }
      //Reset sample counter
    }
    if (beat_idx == NUM_INTERVAL) {
      return getBPM(beat_times);
    }
    for(
        uint32_t up = time+SAMPLEPERIODUS;
        time > 20 && time < up;
        time = micros()
    );
  }
}

uint32_t Mic::getIntervalAverage(uint32_t beat_times[NUM_INTERVAL]) {
  uint32_t min_val = beat_times[0];
  uint8_t min_idx = 0;
  uint32_t max_val = beat_times[0];
  uint8_t max_idx = 0;
  for (uint8_t i = 0; i < NUM_INTERVAL; i++) {
    if (min_val > beat_times[i]) {
      min_val = beat_times[i];
      min_idx = i;
    }
    if (max_val < beat_times[i]) {
      max_val = beat_times[i];
      max_idx = i;
    }
  }
  uint32_t sum = 0;
  for (uint8_t i = 0; i < NUM_INTERVAL; i++) {
    if ((i != min_idx) && (i != max_idx)) {
      sum += beat_times[i];
    }
  }
  uint32_t avg_beat_period = sum / (NUM_INTERVAL - 2);
  return avg_beat_period;
}

uint8_t Mic::getBPM(uint32_t period) {
  uint8_t bpm = (1 / period) * 60000; // (1 / (T * 1000)) * 60
  return bpm;
}


void Mic::detectBeatOld(Lampe& lampe) {
	unsigned long timet = micros(); // Used to track rate
	float sample, value, envelope, beat, thresh;
	unsigned char i;

	for(i = 0;;++i){
		// Read ADC and center so +-512
		sample = (float)analogRead(0)-503.f;

		// Filter only bass component
		value = bassFilter(sample);

		// Take signal amplitude and filter
		if(value < 0)value=-value;
		envelope = envelopeFilter(value);
    Serial.println(i);

		// Every 200 samples (25hz) filter the envelope 
		if(i == 200) {
			// Filter out repeating bass sounds 100 - 180bpm
			beat = beatFilter(envelope);

			thresh = 0;

			// If we are above threshold, light up LED. Timer constraint so that it doesn't activate more often
      // than 210 bpm.
      //Serial.println(beat);
			if((beat > 1.5) && lampe.getTimer() > 280) {
        int red = rand() % 256;
        int green = rand() % 256;
        int blue = rand() % 256;
        fill_solid(lampe.leds, lampe.num_leds, CRGB(red, green, blue));
        lampe.update();
        lampe.resetTimer();
			}
      //Reset sample counter
			i = 0;
		}

		// Consume excess clock cycles, to keep at 5000 hz
		for(unsigned long up = timet+SAMPLEPERIODUS; timet > 20 && timet < up; timet = micros());
	}  
}


unsigned long Mic::getTimer() {
  return millis() - timer;
}

void Mic::resetTimer() {
  timer = millis();
}
