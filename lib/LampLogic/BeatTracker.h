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
        head_ = samples_ = period_ = candidate_ = onsetFloor_ = 0;
        scanLag_ = MaxLag;
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
        sampleWindow(peak, now);
    }

    // Replay the peak windows exported by LA v2 diagnostics on the same path
    // as the live ADC. A gap invalidates history instead of inventing samples.
    void sampleWindow(uint16_t peak, uint32_t now) {
        if (now - windowAt_ > 15) reset(now);
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

        onsetFloor_ += (int32_t(onset) * 256 - onsetFloor_) / 32;
        const uint16_t threshold = 3 + (onsetFloor_ >> 7);
        if (onset >= threshold && now - lastOnset_ >= 120) {
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
        if (samples_ < 484) return;
        scores_[scanLag_ - MinLag] = correlation(scanLag_);
        if (--scanLag_ < MinLag) {
            scanLag_ = MaxLag;
            estimate(now);
        }
    }

    uint8_t bpm() const { return period_ ? (60000UL + period_ / 2) / period_ : 0; }
    uint8_t confidence() const { return period_ ? confidence_ : 0; }
    uint8_t onset() const { return samples_ ? at(0) : 0; }
    uint16_t windowPeak() const { return previousPeak_; }
    uint32_t windowTime() const { return windowAt_; }
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
        // Descending lags compensate for the advancing ring head: every score
        // uses the same 384-window endpoint. Even the oldest read fits in 512.
        for (uint16_t age = 0; age < 384; ++age) {
            const uint16_t a = at(age + MaxLag - lag), b = at(age + MaxLag);
            product += uint32_t(a) * b;
            energy += uint32_t(a) * a + uint32_t(b) * b;
            sumA += a; sumB += b;
        }
        // Remove the mean: random positive changes otherwise look correlated.
        const uint32_t baseline = meanProduct(sumA, sumB);
        if (product <= baseline) return 0;
        product -= baseline;
        energy -= meanProduct(sumA, sumA) + meanProduct(sumB, sumB);
        // Widen before multiplying: AVR int is only 16 bits.
        return energy < 128 ? 0 : product * 200UL / energy;
    }
    // Divide first without discarding the remainder: 384 full-scale windows
    // can overflow a 32-bit squared sum, including on the native replay host.
    static uint32_t meanProduct(uint32_t a, uint32_t b) {
        return (a / 384) * b + (a % 384) * b / 384;
    }
    uint16_t rank(uint8_t lag) const {
        return uint16_t(scores_[lag - MinLag]) * 2 +
            (lag * 2 <= MaxLag ? scores_[lag * 2 - MinLag] : 0);
    }
    void unlock() { period_ = confidence_ = confirmations_ = misses_ = 0; }
    void estimate(uint32_t now) {
        // Old periodic samples remain in the ring after music stops. They must
        // not immediately reacquire a lock that just expired from silence.
        if (now - lastOnset_ > 1500) { confirmations_ = 0; return; }
        // Repetition at twice the interval supports the shorter beat. This
        // prevents alternating strong/weak kicks from looking like half tempo.
        // Keep the base correlation for thresholds and phase refinement.
        uint8_t best = MinLag;
        for (uint8_t lag = MinLag + 1; lag <= MaxLag; ++lag) {
            if (rank(lag) > rank(best)) best = lag;
        }
        // A weak passage should not replace an established tempo with a
        // subdivision. Keep it only while its own correlation still supports it.
        const uint16_t anchor = period_ ? period_ : confirmations_ ? candidate_ : 0;
        if (anchor) {
            uint8_t current = (anchor + WindowMs / 2) / WindowMs;
            if (current < MinLag) current = MinLag;
            if (current > MaxLag) current = MaxLag;
            uint8_t nearby = current;
            for (uint8_t lag = current > MinLag ? current - 1 : current;
                 lag <= current + 1 && lag <= MaxLag; ++lag) {
                if (scores_[lag - MinLag] > scores_[nearby - MinLag]) nearby = lag;
            }
            const uint8_t longer = best > nearby ? best : nearby;
            const uint8_t shorter = best < nearby ? best : nearby;
            const uint8_t multiple = (longer + shorter / 2) / shorter;
            const bool harmonic = multiple >= 2 && absolute(int16_t(longer) - multiple * shorter) <= 2;
            if (scores_[nearby - MinLag] >= (period_ ? 20 : 25) && (harmonic ||
                uint16_t(scores_[nearby - MinLag]) * 100 >= uint16_t(scores_[best - MinLag]) * 65))
                best = nearby;
        }
        const uint8_t score = scores_[best - MinLag];
        const bool supporting = period_ && absolute(int16_t(best * WindowMs) - period_) <= period_ / 12;
        // Require a distinct peak above the lag-score background as well as an
        // absolute floor. Correlation is not a probability of a correct BPM.
        uint16_t sum = 0;
        uint32_t squares = 0;
        for (uint8_t value : scores_) { sum += value; squares += uint16_t(value) * value; }
        constexpr uint8_t Count = MaxLag - MinLag + 1;
        const uint16_t mean = sum / Count;
        const uint32_t variance = squares / Count - mean * mean;
        const int16_t prominence = int16_t(score) - mean;
        if (score < (supporting ? 20 : 25) || prominence <= 0 ||
            uint32_t(prominence) * prominence < 4 * variance) {
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
        // Adjacent scans overlap heavily. Weak real-world peaks need nearly
        // three seconds of persistence; clean pulses can acquire sooner.
        if (confirmations_ < (!period_ && score >= 55 ? 2 : 5)) return;
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
    uint16_t head_ = 0, samples_ = 0, period_ = 0, candidate_ = 0, onsetFloor_ = 0;
    uint8_t previousRise_ = 0, scanLag_ = MaxLag, confirmations_ = 0;
    uint8_t confidence_ = 0, misses_ = 0;
};
} // namespace LampLogic
