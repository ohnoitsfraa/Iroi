#pragma once
struct Interpolator { static float linear(float a, float b, float f) { return a + (b - a) * f; } };
