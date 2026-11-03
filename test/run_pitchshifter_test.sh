#!/bin/sh
# Builds PitchShifter.h natively against a minimal stand-in for the OWL
# library and checks that sines come out one octave up.
# PitchShifter.h includes "Commons.h" from its own folder, so the headers
# under test are copied next to the stand-in before compiling.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$HERE")
BUILD=$(mktemp -d)
cp "$HERE"/shim/* "$BUILD"/
cp "$ROOT"/PitchShifter.h "$ROOT"/DelayLine.h "$BUILD"/
grep -E "^constexpr .*kShimmer" "$ROOT"/Commons.h > "$BUILD"/shimmer_consts.inc
clang++ -std=gnu++17 -O2 -Wall -I"$BUILD" "$HERE"/pitchshifter_test.cpp -o "$BUILD"/pitchshifter_test
"$BUILD"/pitchshifter_test
rm -rf "$BUILD"
