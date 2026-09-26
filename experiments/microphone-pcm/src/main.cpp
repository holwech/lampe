// Temporary measurement firmware. No LED writes or beat analysis while recording.
// Restore the normal lamp firmware after use (capture.py does this in finally).
#include <Arduino.h>
#include <util/atomic.h>

static_assert(F_CPU == 16000000UL, "Timer configuration requires the fitted 16 MHz board");
constexpr uint8_t RingMask = 127, Count = 24, PacketSize = 63;
volatile uint16_t samples[128];
volatile uint8_t head = 0, tail = 0;
volatile uint16_t dropped = 0;
uint32_t sent = 0;
uint16_t sequence = 0;

ISR(ADC_vect) {
    const uint16_t reading = ADC;
    TIFR1 = _BV(OCF1B); // Clear the trigger flag so the next timer edge triggers ADC.
    const uint8_t next = (head + 1) & RingMask;
    if (next == tail) {
        if (dropped != UINT16_MAX) ++dropped;
    } else {
        samples[head] = reading;
        head = next;
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(3, OUTPUT);
    digitalWrite(3, LOW); // Keep the LEDs latched; never transmit during capture.
    delay(1000);
    ADMUX = _BV(REFS0); // AVcc reference, ADC0/A0.
    DIDR0 = _BV(ADC0D);
    ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0); // 125 kHz ADC clock.
    ADCSRA |= _BV(ADSC); // Prime and discard the longer first conversion.
    while (ADCSRA & _BV(ADSC)) {}
    (void)ADC;
    TCCR1A = TCCR1B = 0;
    TCNT1 = 0;
    OCR1A = 499; // 16 MHz / 8 / 500 = 4000 Hz.
    OCR1B = 249;
    TIFR1 = _BV(OCF1B);
    ADCSRB = _BV(ADTS2) | _BV(ADTS0); // Timer1 compare B auto trigger.
    ADCSRA |= _BV(ADIF) | _BV(ADATE) | _BV(ADIE);
    TCCR1B = _BV(WGM12) | _BV(CS11);
}

void loop() {
    if (((head - tail) & RingMask) < Count || Serial.availableForWrite() < PacketSize) return;
    uint8_t packet[PacketSize] = {'L', 'P', 1, Count, 0xa0, 0x0f}; // 4000 Hz, little endian.
    packet[6] = sequence; packet[7] = sequence >> 8;
    for (uint8_t i = 0; i < 4; ++i) packet[8 + i] = sent >> (8 * i);
    uint16_t lost;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { lost = dropped; }
    packet[12] = lost; packet[13] = lost >> 8;
    for (uint8_t i = 0; i < Count; ++i) {
        // Producer never overwrites the consumer's current slot.
        const uint16_t value = samples[tail];
        tail = (tail + 1) & RingMask;
        packet[14 + 2 * i] = value; packet[15 + 2 * i] = value >> 8;
    }
    uint8_t crc = 0;
    for (uint8_t i = 0; i < PacketSize - 1; ++i) {
        crc ^= packet[i];
        for (uint8_t bit = 0; bit < 8; ++bit) crc = crc & 128 ? (crc << 1) ^ 7 : crc << 1;
    }
    packet[PacketSize - 1] = crc;
    Serial.write(packet, PacketSize);
    sent += Count;
    ++sequence;
}
