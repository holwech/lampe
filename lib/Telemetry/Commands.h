#pragma once
#include <Telemetry.h>
#include <Programs.h>

namespace Commands {
// Host -> lamp: 'L', 'C', version 1, opcode, argument, CRC-8.
// Fixed storage, bounded polling, and no blocking serial reads.
class Reader {
public:
    bool push(uint8_t byte, uint32_t now, LampEngine &lamp) {
        if (used_ && uint32_t(now - lastByteAt_) > 100) used_ = 0;
        lastByteAt_ = now;
        bytes_[used_++] = byte;
        while (used_) {
            if (bytes_[0] == 'L' && (used_ < 2 || bytes_[1] == 'C')) {
                if (used_ < sizeof(bytes_)) return false;
                if (bytes_[2] == 1 &&
                    ((bytes_[3] == 1 && bytes_[4] < Programs::count()) ||
                     (bytes_[3] == 2 && bytes_[4] <= 1)) &&
                    bytes_[5] == Telemetry::checksum(bytes_, 5)) {
                    used_ = 0;
                    if (bytes_[3] == 2) {
                        audioRequested_ = bytes_[4] != 0;
                        audioRequestedAt_ = now;
                        return true;
                    }
                    // Repeated selections must not reset an effect or its BPM lock.
                    if (lamp.program() != bytes_[4]) lamp.selectProgram(bytes_[4], now);
                    return true;
                }
            }
            --used_;
            for (uint8_t i = 0; i < used_; ++i) bytes_[i] = bytes_[i + 1];
        }
        return false;
    }

    bool audioEnabled(uint32_t now) const {
        return audioRequested_ && uint32_t(now - audioRequestedAt_) < 3000;
    }

    template <typename SerialPort>
    void poll(SerialPort &serial, LampEngine &lamp, uint32_t now) {
        for (uint8_t budget = 0; budget < 8 && serial.available() > 0; ++budget)
            push(static_cast<uint8_t>(serial.read()), now, lamp);
    }

private:
    uint8_t bytes_[6] = {}, used_ = 0;
    uint32_t lastByteAt_ = 0;
    uint32_t audioRequestedAt_ = 0;
    bool audioRequested_ = false;
};
} // namespace Commands
