#include "Lampe.h"

Lampe::Lampe()
{
    uint32_t startupTime = millis();

    prevButtonState = LOW;
    timer = startupTime;
    menuOption = 0;
    menuRestart = false;
    linearReduceTimer = startupTime;
    sampleTimer = startupTime;
    sampleInit = true;
    linearValue = 0;

    pinMode(2, INPUT); // Button sensor

    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(BRIGHTNESS);

    gHue = 0;
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    num_leds = NUM_LEDS;
}

void Lampe::update()
{
    FastLED.show();
    FastLED.delay(1000 / FRAMES_PER_SECOND);
    updateLinearReduce();
}

uint8_t Lampe::cycleNumber(uint8_t num, uint8_t peak, uint8_t stepSize)
{
    uint8_t i = num + stepSize;
    if (i >= peak) {
        return 0;
    }
    return i;
}

void Lampe::updateLinearReduce()
{
    unsigned long currTime = millis();
    if ((linearValue > 0) && ((currTime - linearReduceTimer) > 5))
    {
        linearValue -= 1;
        Serial.print("Linval: ");
        Serial.print(linearValue);
        linearReduceTimer = currTime;
    }
}

uint8_t Lampe::linearReduce(uint8_t peak)
{
    if (peak > 255)
    {
        linearValue = 255;
    }
    else if (peak > linearValue)
    {
        linearValue = peak;
    }
    return linearValue;
}

uint8_t Lampe::nextMenuOptionOnClick()
{
    if (buttonClick())
    {
        return nextMenuOption();
    }
    return menuOption;
}

bool Lampe::buttonClick()
{
    int buttonState = digitalRead(BUTTON_PIN);
    if (buttonState == LOW && prevButtonState == HIGH)
    {
        prevButtonState = LOW;
        return true;
    }
    else if (buttonState == HIGH)
    {
        prevButtonState = HIGH;
    }
    return false;
}

uint8_t Lampe::nextMenuOption()
{
    newStateVarReset();
    incrementMenu();
    return menuOption;
}

void Lampe::newStateVarReset()
{
    sampleInit = true;
    for (int i = 0; i < NUM_LEDS; i++)
    {
        stateValues[i] = 0;
    }
}

void Lampe::incrementMenu()
{
    if (menuOption >= NUM_MENU_OPTIONS)
    {
        menuOption = 0;
        menuRestart = true;
    }
    else
    {
        menuOption++;
        menuRestart = false;
    }
}

uint32_t Lampe::getSampleTimer()
{
    return millis() - sampleTimer;
}

void Lampe::resetSampleTimer()
{
    sampleTimer = millis();
}

uint32_t Lampe::getTimer()
{
    return millis() - timer;
}

void Lampe::resetTimer()
{
    timer = millis();
}
