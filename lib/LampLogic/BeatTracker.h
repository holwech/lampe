#pragma once
#include <stdint.h>
#include <string.h>

namespace LampLogic {

// Time-domain onset autocorrelation for the 2 KB AVR. No FFT, heap, or floats.
// Search 60–200 BPM over 5.12 s of microphone history. One lag is evaluated
// per 10 ms window, spreading the work across the lamp loop.
class BeatTracker {
public:
    void reset(uint32_t now) {
        memset(history_, 0, sizeof(history_));
        memset(scores_, 0, sizeof(scores_));
        windowAt_ = lastOnset_ = lastSupport_ = beatAt_ = now;
        minimum_ = 1023; maximum_ = previousPeak_ = previousRise_ = 0;
        head_ = samples_ = period_ = candidate_ = 0;
        scanLag_ = MinLag;
        confirmations_ = confidence_ = misses_ = 0;
    }

    void sample(uint16_t reading, uint32_t now) {
        // A suspended simulator or disconnected sensor must not invent beats.
        const uint32_t elapsed = now - windowAt_;
        if (elapsed > 100) reset(now);
        if (reading <= 1023) {
            if (reading < minimum_) minimum_ = reading;
            if (reading > maximum_) maximum_ = reading;
        }
        if (now - windowAt_ < WindowMs) return;
        const uint16_t peak = maximum_ >= minimum_ ? maximum_ - minimum_ : 0;
        minimum_ = 1023; maximum_ = 0;
        windowAt_ = now;
        // Positive energy changes reject DC, steady tones and falling tails.
        const uint16_t difference = peak > previousPeak_ ? peak - previousPeak_ : 0;
        const uint8_t rise = difference < 4 ? 0 : difference > 510 ? 255 : uint8_t(difference / 2);
        previousPeak_ = peak;
        const uint8_t onset = (uint16_t(rise) + previousRise_) / 2;
        previousRise_ = rise;
        history_[head_] = onset;
        head_ = (head_ + 1) & Mask;
        if (samples_ < HistorySize) ++samples_;

        if (rise >= 2 && now - lastOnset_ >= 120) {
            lastOnset_ = now;
            if (period_) {
                const uint16_t phase = (now - beatAt_) % period_;
                const int16_t error = phase > period_ / 2 ? int16_t(phase) - period_ : phase;
                if (absolute(error) <= period_ / 5) {
                    // Follow the beat gently instead of flashing on every sound.
                    beatAt_ += error / 4;
                    lastSupport_ = now;
                }
            }
        }
        if (period_ && now - lastSupport_ > 3000) unlock();
        if (samples_ < 400) return;
        scores_[scanLag_ - MinLag] = correlation(scanLag_);
        if (++scanLag_ > MaxLag) {
            scanLag_ = MinLag;
            estimate(now);
        }
    }

    uint8_t bpm() const { return period_ ? (60000UL + period_ / 2) / period_ : 0; }
    uint8_t confidence() const { return period_ ? confidence_ : 0; }
    uint8_t onset() const { return samples_ ? at(0) : 0; }
    uint8_t pulse(uint32_t now) const {
        if (!period_) return 0;
        const uint16_t phase = (now - beatAt_) % period_;
        constexpr uint16_t WidthMs = 90;
        return phase < WidthMs ? uint32_t(WidthMs - phase) * 255 / WidthMs : 0;
    }

private:
    static constexpr uint16_t WindowMs = 10, HistorySize = 512, Mask = HistorySize - 1;
    static constexpr uint8_t MinLag = 30, MaxLag = 100;
    static uint16_t absolute(int16_t value) { return value < 0 ? -value : value; }
    uint8_t at(uint16_t age) const { return history_[(head_ - 1 - age) & Mask]; }
    uint8_t correlation(uint8_t lag) const {
        uint32_t product = 0, energy = 0, sumA = 0, sumB = 0;
        for (uint16_t age = 0; age < 256; ++age) {
            const uint16_t a = at(age), b = at(age + lag);
            product += uint32_t(a) * b;
            energy += uint32_t(a) * a + uint32_t(b) * b;
            sumA += a; sumB += b;
        }
        // Remove the mean: random positive changes otherwise look correlated.
        const uint32_t baseline = sumA * sumB / 256;
        if (product <= baseline) return 0;
        product -= baseline;
        energy -= sumA * sumA / 256 + sumB * sumB / 256;
        // Widen before multiplying: AVR int is only 16 bits.
        return energy < 128 ? 0 : product * 200UL / energy;
    }
    void unlock() { period_ = confidence_ = confirmations_ = misses_ = 0; }
    void estimate(uint32_t now) {
        uint8_t best = MinLag;
        for (uint8_t lag = MinLag + 1; lag <= MaxLag; ++lag) {
            if (scores_[lag - MinLag] > scores_[best - MinLag]) best = lag;
        }
        // Prefer the shortest convincing repetition to its double/triple.
        // Strong subdivisions can still yield half/double tempo, as with any
        // microphone-only energy tracker; this is an estimate, not song metadata.
        for (uint8_t lag = MinLag; lag < best; ++lag) {
            const uint8_t multiple = (best + lag / 2) / lag;
            if (uint16_t(scores_[lag - MinLag]) * 100 >= uint16_t(scores_[best - MinLag]) * 90 &&
                multiple >= 2 && absolute(int16_t(best) - multiple * lag) <= 2) {
                best = lag; break;
            }
        }
        const uint8_t score = scores_[best - MinLag];
        if (score < 55) {
            confirmations_ = 0;
            if (++misses_ >= 3) unlock();
            return;
        }
        misses_ = 0;
        int16_t refined = best * WindowMs;
        if (best > MinLag && best < MaxLag) {
            const int16_t left = scores_[best - MinLag - 1], right = scores_[best - MinLag + 1];
            const int16_t curvature = 2 * int16_t(score) - left - right;
            if (curvature > 0) refined += 5 * (right - left) / curvature;
        }
        if (refined < 300) refined = 300;
        if (refined > 1000) refined = 1000;
        if (period_ && absolute(refined - period_) <= period_ / 12) {
            const uint16_t next = (3UL * period_ + refined) / 4;
            // Rebase the oscillator without jumping phase when tempo changes.
            beatAt_ = now - uint32_t((now - beatAt_) % period_) * next / period_;
            period_ = next;
            confidence_ = score;
            confirmations_ = 0;
            return;
        }
        if (absolute(refined - candidate_) <= uint16_t(refined) / 20) ++confirmations_;
        else confirmations_ = 1;
        candidate_ = refined;
        if (confirmations_ < 2) return;
        period_ = candidate_;
        confidence_ = score;
        confirmations_ = 0;
        uint16_t strongestAge = 0;
        for (uint16_t age = 1; age < period_ / WindowMs; ++age) {
            if (at(age) > at(strongestAge)) strongestAge = age;
        }
        beatAt_ = now - strongestAge * WindowMs;
        lastSupport_ = now;
    }

    uint8_t history_[HistorySize] = {}, scores_[MaxLag - MinLag + 1] = {};
    uint32_t windowAt_ = 0, lastOnset_ = 0, lastSupport_ = 0, beatAt_ = 0;
    uint16_t minimum_ = 1023, maximum_ = 0, previousPeak_ = 0;
    uint16_t head_ = 0, samples_ = 0, period_ = 0, candidate_ = 0;
    uint8_t previousRise_ = 0, scanLag_ = MinLag, confirmations_ = 0;
    uint8_t confidence_ = 0, misses_ = 0;
};
} // namespace LampLogic
