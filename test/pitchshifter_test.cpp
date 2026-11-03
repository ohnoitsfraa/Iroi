#include <stdio.h>
#include <math.h>
#include "PitchShifter.h"
int main() {
    const float fs = 48000.f;
    float freqs[] = { 110.f, 440.f, 1000.f };
    for (float f : freqs) {
        PitchShifter* ps = PitchShifter::create(kShimmerWindowSamples, kShimmerRatio);
        int n = 3 * fs, crossings = 0; float prev = 0, peak = 0, sumsq = 0; int cnt = 0;
        for (int i = 0; i < n; i++) {
            float y = ps->Process(sinf(2.f * M_PI * f * i / fs));
            if (i >= fs) { // skip 1 s settling
                if (prev <= 0.f && y > 0.f) crossings++;
                peak = fmaxf(peak, fabsf(y)); sumsq += y * y; cnt++;
            }
            prev = y;
        }
        printf("in %6.1f Hz -> out %7.1f Hz (ratio %.3f), rms %.3f, peak %.3f\n", f, crossings / 2.f, crossings / 2.f / f, sqrtf(sumsq / cnt), peak);
        PitchShifter::destroy(ps);
    }
}
