#include <LampEngine.h>
#include <Programs.h>
#include <Telemetry.h>
#include <Microphone.h>

namespace {
LampEngine lamp;
uint64_t timeUs = 0;
uint64_t nextSampleUs = 0;
uint8_t brightness = LampConfig::Brightness;
uint8_t sequence = 0;
uint8_t packet[Telemetry::PacketSize];
Telemetry::Microphone microphone;
struct AudioOutput {
    uint8_t bytes[Telemetry::Microphone::Size * 2] = {};
    uint16_t used = 0;
    int availableForWrite() { return sizeof(bytes) - used; }
    void write(const uint8_t *data, size_t size) {
        memcpy(bytes + used, data, size);
        used += size;
    }
} audio;
uint32_t now() { return static_cast<uint32_t>(timeUs / 1000); }
const char *const names[] = {
#define LAMP_PROGRAM(function, label, audio) label,
#include <ProgramList.def>
#undef LAMP_PROGRAM
};
}

extern "C" {
void lamp_reset(uint16_t seed) {
    timeUs = nextSampleUs = 0;
    sequence = 0;
    brightness = LampConfig::Brightness;
    lamp.reset(0, false, seed);
    microphone = Telemetry::Microphone{};
    audio.used = 0;
}
int lamp_select(uint8_t program) {
    microphone.reset(); audio.used = 0;
    return lamp.selectProgram(program, now());
}
void lamp_brightness(uint8_t value) { brightness = value; }
void lamp_button(int high) { lamp.pollButton(high != 0, now()); }
// A 1 kHz square-wave source exercises the same ADC envelope as the real lamp.
void lamp_advance(uint32_t frames, uint16_t peakToPeak, int buttonHigh) {
    audio.used = 0;
    if (frames > 1200) frames = 1200;
    if (peakToPeak > 1023) peakToPeak = 1023;
    for (uint32_t frame = 0; frame < frames; ++frame) {
        timeUs += LampConfig::FrameIntervalUs;
        while (nextSampleUs <= timeUs) {
            uint16_t reading = 512;
            if ((nextSampleUs / 1000) & 1) reading += peakToPeak / 2;
            else reading -= (peakToPeak + 1) / 2;
            lamp.sampleAudio(reading, static_cast<uint32_t>(nextSampleUs / 1000));
            if (Programs::usesAudio(lamp.program())) {
                microphone.sample(reading, lamp.audioOnset(), lamp.beatPulse(nextSampleUs / 1000) > 0,
                                  static_cast<uint32_t>(nextSampleUs));
                microphone.tryWrite(audio);
            } else microphone.reset();
            nextSampleUs += 1000;
        }
        lamp.pollButton(buttonHigh != 0, now());
        lamp.render(now());
        ++sequence;
    }
}
const uint8_t *lamp_frame() {
    Telemetry::encode(packet, lamp, now(), sequence, brightness);
    return packet;
}
uint32_t lamp_frame_size() { return Telemetry::PacketSize; }
const uint8_t *lamp_audio_data() { return audio.bytes; }
uint32_t lamp_audio_size() { return audio.used; }
uint32_t lamp_program_count() { return Programs::count(); }
const char *lamp_program_name(uint8_t program) { return program < Programs::count() ? names[program] : "Unknown"; }
int lamp_program_uses_audio(uint8_t program) { return Programs::usesAudio(program); }
uint32_t lamp_frame_interval_us() { return LampConfig::FrameIntervalUs; }
}
