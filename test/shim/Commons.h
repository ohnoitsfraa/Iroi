#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define ONE_POLE(out, in, coefficient) out += (coefficient) * ((in) - out);
template <typename T> inline T Clamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
struct FloatArray {
    float* d = nullptr; uint32_t n = 0;
    static FloatArray create(uint32_t n) { FloatArray a; a.d = (float*)calloc(n, sizeof(float)); a.n = n; return a; }
    static void destroy(FloatArray a) { free(a.d); }
    void clear() { memset(d, 0, n * sizeof(float)); }
    float& operator[](size_t i) { return d[i]; }
};
#include "shimmer_consts.inc" // Generated from ../Commons.h by run_pitchshifter_test.sh
