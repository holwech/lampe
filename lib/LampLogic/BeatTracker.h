#pragma once
#include <stdint.h>
#include <string.h>

namespace LampLogic {

// Time-domain onset autocorrelation for the 2 KB AVR. No FFT, heap, or floats.
// Search 60–200 BPM using repeated attacks and persistent tempo hypotheses.
// Firmware calls work() only between LED deadlines; replay drains the same work.
class BeatTracker {
public:
    void reset(uint32_t now) {
        memset(history_, 0, sizeof(history_));
        memset(scores_, 0, sizeof(scores_));
        memset(evidence_, 0, sizeof(evidence_));
        windowAt_ = lastOnset_ = lastSupport_ = beatAt_ = now;
        minimum_ = 1023; peakMaximum_ = previousPeak_ = previousRise_ = 0;
        head_ = samples_ = period_ = candidate_ = onsetFloor_ = envelopeQ4_ = previousEnvelope_ = 0;
        scanLag_ = MaxCorrelationLag;
        confirmations_ = confidence_ = misses_ = 0;
        windows_ = scanStarted_ = scanHead_ = 0;
        work_ = Work::Idle;
    }

    void sample(uint16_t reading, uint32_t now, bool deferred = false) {
        // A suspended simulator or disconnected sensor must not invent beats.
        const uint32_t elapsed = now - windowAt_;
        if (elapsed > 100) reset(now);
        if (reading <= 1023) {
            if (reading < minimum_) minimum_ = reading;
            if (reading > peakMaximum_) peakMaximum_ = reading;
        }
        if (now - windowAt_ < WindowMs) return;
        const uint16_t peak = peakMaximum_ >= minimum_ ? peakMaximum_ - minimum_ : 0;
        minimum_ = 1023; peakMaximum_ = 0;
        sampleWindow(peak, now, deferred);
    }

    // Replay the peak windows exported by LA v2 diagnostics on the same path
    // as the live ADC. A gap invalidates history instead of inventing samples.
    void sampleWindow(uint16_t peak, uint32_t now, bool deferred = false) {
        if (now - windowAt_ > 15) reset(now);
        windowAt_ = now;
        // Smooth rapid window fluctuations before finding attacks. Q4 retains
        // weak changes; relative gain keeps loud passages from drowning out
        // quieter attacks. The offset and deadband limit near-silence gain.
        envelopeQ4_ += (int32_t(peak) * 16 - envelopeQ4_) / 2;
        const uint16_t envelope = envelopeQ4_ / 16;
        const uint16_t difference = envelope > previousEnvelope_ ? envelope - previousEnvelope_ : 0;
        previousEnvelope_ = envelope;
        // difference <= envelope, so this ratio is bounded below 128.
        const uint8_t rise = difference < 2 ? 0 : uint32_t(difference) * 128 / (envelope + 16);
        previousPeak_ = peak;
        const uint8_t onset = (uint16_t(rise) + previousRise_) / 2;
        previousRise_ = rise;
        history_[head_] = onset;
        head_ = (head_ + 1) & Mask;
        if (samples_ < HistorySize) ++samples_;
        ++windows_;

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
        if (samples_ >= CorrelationSize + MaxCorrelationLag && work_ == Work::Idle) {
            if (scanLag_ == MaxCorrelationLag) {
                scanHead_ = head_;
                scanStarted_ = windows_;
            }
            product_ = energy_ = sumA_ = sumB_ = 0;
            age_ = 0;
            work_ = Work::Correlation;
        }
        if (!deferred) while (workPending()) work(now);
    }

    bool workPending() const { return work_ != Work::Idle; }

    // Bounded work units: 32 sample pairs, one linear transition pass pair,
    // four tempo observations, or one short final commit. No heap or FFT.
    void work(uint32_t now) {
        switch (work_) {
        case Work::Idle: return;
        case Work::Correlation: {
            // A delayed consumer must not read history that has been overwritten.
            if (uint32_t(uint16_t(windows_ - scanStarted_)) + CorrelationSize + scanLag_ > HistorySize) {
                scanLag_ = MaxCorrelationLag;
                work_ = Work::Idle;
                return;
            }
            for (uint8_t n = 0; n < 32; ++n, ++age_) {
                const uint8_t a = history_[(scanHead_ - 1 - age_) & Mask];
                const uint8_t b = history_[(scanHead_ - 1 - age_ - scanLag_) & Mask];
                product_ += uint16_t(a) * b;
                energy_ += uint16_t(a) * a + uint16_t(b) * b;
                sumA_ += a; sumB_ += b;
            }
            if (age_ < CorrelationSize) return;
            const uint32_t baseline = meanProduct(sumA_, sumB_);
            uint8_t score = 0;
            if (product_ > baseline) {
                energy_ -= meanProduct(sumA_, sumA_) + meanProduct(sumB_, sumB_);
                if (energy_ >= 128) score = (product_ - baseline) * 200UL / energy_;
            }
            scores_[scanLag_ - MinCorrelationLag] = score;
            work_ = Work::Idle;
            if (--scanLag_ < MinCorrelationLag) {
                scanLag_ = MaxCorrelationLag;
                if (now - lastOnset_ <= 1500) work_ = Work::Transitions;
                else confirmations_ = 0;
            }
            return;
        }
        case Work::Transitions:
            // A Laplace transition model in O(N), keeping all candidate periods.
            for (uint8_t i = 1; i < TempoCount; ++i)
                if (evidence_[i - 1] - TransitionCost > evidence_[i])
                    evidence_[i] = evidence_[i - 1] - TransitionCost;
            for (int16_t i = TempoCount - 2; i >= 0; --i)
                if (evidence_[i + 1] - TransitionCost > evidence_[i])
                    evidence_[i] = evidence_[i + 1] - TransitionCost;
            observation_ = MinLag;
            best_ = MinLag;
            maximum_ = -32767;
            work_ = Work::Observations;
            return;
        case Work::Observations:
            for (uint8_t n = 0; n < 4 && observation_ <= MaxLag; ++n, ++observation_) {
                const uint16_t value = rank(observation_) + 1;
                uint16_t power = 1;
                int16_t logarithm = 0;
                while (power * 2 <= value) { power *= 2; logarithm += 16; }
                logarithm += (value - power) * 16 / power;
                int16_t &evidence = evidence_[observation_ - MinLag];
                evidence += logarithm;
                if (evidence > maximum_) { maximum_ = evidence; best_ = observation_; }
            }
            if (observation_ > MaxLag) work_ = Work::Commit;
            return;
        case Work::Commit:
            for (uint8_t i = 0; i < TempoCount; ++i) evidence_[i] -= maximum_;
            commit(now, best_);
            work_ = Work::Idle;
            return;
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
    static constexpr uint16_t CorrelationSize = 256;
    static constexpr uint8_t MinLag = 30, MaxLag = 100;
    static constexpr uint8_t MinCorrelationLag = 15, MaxCorrelationLag = 200;
    static constexpr uint8_t TempoCount = MaxLag - MinLag + 1;
    static constexpr int16_t TransitionCost = 8;
    enum class Work : uint8_t { Idle, Correlation, Transitions, Observations, Commit };
    static uint16_t absolute(int16_t value) { return value < 0 ? -value : value; }
    uint8_t at(uint16_t age) const { return history_[(head_ - 1 - age) & Mask]; }
    // Sums are bounded by 256 * 127. Divide first to keep intermediates wide
    // enough on AVR (whose ordinary int is only 16 bits).
    static uint32_t meanProduct(uint32_t a, uint32_t b) {
        return (a / 256) * b + (a % 256) * b / 256;
    }
    uint16_t rank(uint8_t lag) const {
        uint16_t result = 2 * uint16_t(scores_[lag - MinCorrelationLag]);
        for (uint8_t multiple = 2; multiple <= 4; ++multiple) {
            const uint16_t center = lag * multiple;
            if (center > MaxCorrelationLag) break;
            uint16_t sum = 0; uint8_t count = 0;
            for (int16_t delta = 1 - int16_t(multiple); delta < multiple; ++delta) {
                const int16_t at = center + delta;
                if (at <= MaxCorrelationLag) { sum += scores_[at - MinCorrelationLag]; ++count; }
            }
            result += sum / count;
        }
        // A broad, mild preference for ordinary musical tempos; strong evidence
        // still wins across the full 60–200 BPM range (covered by tests).
        return uint32_t(result) * (200 - absolute(int16_t(lag) - 50)) / 200;
    }
    void unlock() { period_ = confidence_ = confirmations_ = misses_ = 0; }
    void commit(uint32_t now, uint8_t best) {
        const uint8_t score = scores_[best - MinCorrelationLag];
        if (score < 4) { if (++misses_ >= 3) unlock(); return; }
        misses_ = 0;
        int16_t refined = best * WindowMs;
        if (best > MinLag && best < MaxLag) {
            const int16_t left = scores_[best - MinCorrelationLag - 1], right = scores_[best - MinCorrelationLag + 1];
            const int16_t curvature = 2 * int16_t(score) - left - right;
            // The temporal model may favor a period whose immediate correlation
            // is not a local peak. Parabolic interpolation is only valid at one;
            // otherwise it can jump many milliseconds away from that period.
            if (curvature > 0 && score >= left && score >= right)
                refined += 5 * (right - left) / curvature;
        }
        if (refined < 300) refined = 300;
        if (refined > 1000) refined = 1000;
        if (period_ && absolute(refined - period_) <= period_ / 12) {
            // Smooth tempo updates and preserve the current beat phase.
            const uint16_t next = (3UL * period_ + refined + 2) / 4;
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
        // Three consistent scans avoid latching onto an early subdivision.
        if (confirmations_ < 3) return;
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

    int16_t evidence_[TempoCount] = {};
    uint8_t history_[HistorySize] = {}, scores_[MaxCorrelationLag - MinCorrelationLag + 1] = {};
    uint32_t windowAt_ = 0, lastOnset_ = 0, lastSupport_ = 0, beatAt_ = 0;
    uint16_t envelopeQ4_ = 0, previousEnvelope_ = 0;
    uint16_t minimum_ = 1023, peakMaximum_ = 0, previousPeak_ = 0;
    uint16_t head_ = 0, samples_ = 0, period_ = 0, candidate_ = 0, onsetFloor_ = 0;
    uint8_t previousRise_ = 0, scanLag_ = MaxCorrelationLag, confirmations_ = 0;
    uint8_t confidence_ = 0, misses_ = 0;
    Work work_ = Work::Idle;
    uint16_t windows_ = 0, scanStarted_ = 0, scanHead_ = 0, age_ = 0;
    uint32_t product_ = 0, energy_ = 0, sumA_ = 0, sumB_ = 0;
    uint8_t observation_ = MinLag, best_ = MinLag;
    int16_t maximum_ = 0;
};
} // namespace LampLogic
