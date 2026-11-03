#pragma once

#include "Commons.h"
#include "DelayLine.h"

/**
 * @brief Delay line pitch shifter: two read taps sweep through a short
 *        window half a cycle apart and are crossfaded, so the jump of
 *        each tap back to the start of the window is never heard.
 */
class PitchShifter
{
private:
    DelayLine* line_;
    float window_, rate_, phase_, lp_;

    inline float Window(float phase)
    {
        return 4.f * phase * (1.f - phase);
    }

public:
    PitchShifter(int32_t window, float ratio)
    {
        window_ = window;
        line_ = DelayLine::create(window + kShimmerMinDelaySamples + 2);
        phase_ = 0.f;
        lp_ = 0.f;
        SetRatio(ratio);
    }
    ~PitchShifter()
    {
        DelayLine::destroy(line_);
    }

    static PitchShifter* create(int32_t window, float ratio)
    {
        return new PitchShifter(window, ratio);
    }

    static void destroy(PitchShifter* obj)
    {
        delete obj;
    }

    /**
     * @param ratio Frequency ratio, 2 is one octave up
     */
    void SetRatio(float ratio)
    {
        // Each tap's delay must shrink by (ratio - 1) samples per sample for
        // the read pointer to move at ratio times the write speed.
        rate_ = (ratio - 1.f) / window_;
    }

    float Process(const float in)
    {
        line_->write(in);

        phase_ -= rate_;
        if (phase_ < 0.f)
        {
            phase_ += 1.f;
        }
        else if (phase_ >= 1.f)
        {
            phase_ -= 1.f;
        }

        float phase2 = phase_ + 0.5f;
        if (phase2 >= 1.f)
        {
            phase2 -= 1.f;
        }

        float a = line_->read(kShimmerMinDelaySamples + phase_ * window_);
        float b = line_->read(kShimmerMinDelaySamples + phase2 * window_);

        // Normalized for constant power: the taps are half a window apart,
        // which for a reverb tail makes them uncorrelated. Normalizing the
        // sum to one instead loses up to 3 dB at the crossover.
        float wa = Window(phase_);
        float wb = Window(phase2);
        float out = (a * wa + b * wb) / sqrtf(wa * wa + wb * wb);

        ONE_POLE(lp_, out, kShimmerLpCoeff);

        return lp_;
    }
};
