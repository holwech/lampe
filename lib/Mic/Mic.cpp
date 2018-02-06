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

unsigned short* Mic::sample() {
	unsigned long time = micros(); // Used to track rate
	float sample, value, envelope, beat, thresh;
	unsigned char i;
  unsigned int startTime = millis();
  unsigned long timer = startTime;
  unsigned short* beatInterval = new unsigned short[NUM_INTERVAL];


	for(i = 0;;++i){
		// Read ADC and center so +-512
		sample = (float)analogRead(0)-503.f;

		// Filter only bass component
		value = bassFilter(sample);

		// Take signal amplitude and filter
		if(value < 0)value=-value;
		envelope = envelopeFilter(value);

		// Every 200 samples (25hz) filter the envelope 
		if(i == 200) {
			// Filter out repeating bass sounds 100 - 180bpm
			beat = beatFilter(envelope);

			// Threshold it based on potentiometer on AN1
			thresh = 0.02f * (float)analogRead(1);

			// If we are above threshold, light up LED. Timer constraint so that it doesn't activate more often
      // than 210 bpm.
      unsigned short currTime = millis();
			if((beat > thresh) && currTime - timer > 280) {
        interval = currTime - timer;
        timer = currTime;
			}
      //Reset sample counter
			i = 0;
		}
    if (millis() - startTime > 10000) {
      return interval;
    }

		// Consume excess clock cycles, to keep at 5000 hz
		for(unsigned long up = time+SAMPLEPERIODUS; time > 20 && time < up; time = micros());
	}
  return interval;
}


void Mic::mic(Lampe& Lampe, State& State) {
	unsigned long time = micros(); // Used to track rate
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

		// Every 200 samples (25hz) filter the envelope 
		if(i == 200) {
			// Filter out repeating bass sounds 100 - 180bpm
			beat = beatFilter(envelope);

			// Threshold it based on potentiometer on AN1
			thresh = 0.02f * (float)analogRead(1);

			// If we are above threshold, light up LED. Timer constraint so that it doesn't activate more often
      // than 210 bpm.
			if((beat > thresh) && State.getTimer() > 280) {
        int light	= rand() % 5;
        int red = rand() % 256;
        int green = rand() % 256;
        int blue = rand() % 256;
        Lampe.setLight(light, red, green, blue); 
        State.resetTimer();
        Tlc.update();
			}
      //Reset sample counter
			i = 0;
		}

		// Consume excess clock cycles, to keep at 5000 hz
		for(unsigned long up = time+SAMPLEPERIODUS; time > 20 && time < up; time = micros());
	}  
}
