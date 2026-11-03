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
    float window_, targetWindow_, maxWindow_, ratio_, rate_, phase_, lp_, lpCoeff_;

    inline float Window(float phase)
    {
        return 4.f * phase * (1.f - phase);
    }

public:
    PitchShifter(int32_t window, float ratio, int32_t maxWindow)
    {
        window_ = targetWindow_ = window;
        maxWindow_ = maxWindow > window ? maxWindow : window;
        line_ = DelayLine::create(maxWindow_ + kShimmerMinDelaySamples + 2);
        phase_ = 0.f;
        lp_ = 0.f;
        lpCoeff_ = kShimmerLpCoeff;
        SetRatio(ratio);
    }
    ~PitchShifter()
    {
        DelayLine::destroy(line_);
    }

    /**
     * @param maxWindow Longest window SetWindow will be given, if longer than window
     */
    static PitchShifter* create(int32_t window, float ratio, int32_t maxWindow = 0)
    {
        return new PitchShifter(window, ratio, maxWindow);
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
        ratio_ = ratio;
        rate_ = (ratio_ - 1.f) / window_;
    }

    /**
     * @param window In samples, glides there to avoid clicks
     */
    void SetWindow(float window)
    {
        targetWindow_ = Clamp(window, 1.f, maxWindow_);
    }

    /**
     * @param coeff One-pole low pass coefficient, 1 is fully open
     */
    void SetLpCoeff(float coeff)
    {
        lpCoeff_ = coeff;
    }

    float Process(const float in)
    {
        line_->write(in);

        if (window_ != targetWindow_)
        {
            ONE_POLE(window_, targetWindow_, kShimmerWindowSlew);
            if (fabsf(targetWindow_ - window_) < 0.5f)
            {
                window_ = targetWindow_;
            }
            rate_ = (ratio_ - 1.f) / window_;
        }

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

        ONE_POLE(lp_, out, lpCoeff_);

        return lp_;
    }
};
