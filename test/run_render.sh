#!/bin/sh
# Builds the Iroi DSP chain natively (test/render.cpp) against OwlProgram's
# library and renders a WAV through it at several shimmer amounts.
#
#   test/run_render.sh [in.wav] [key=value ...]
#
# Without in.wav a dry demo source (three chords, then silence) is used.
# Renders go to test/out/ (git-ignored). Settings are passed to every render,
# see render.cpp for the keys. OWLPROGRAM defaults to ../OwlProgram.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$HERE")
OWL=${OWLPROGRAM:-$(dirname "$ROOT")/OwlProgram}
OUT="$HERE/out"
BUILD="$OUT/build"
mkdir -p "$BUILD"

# Only the library sources the DSP chain links against. The C ones are
# compiled as C so their symbols keep C linkage.
INC="-I$ROOT -I$OWL/LibSource -I$OWL/Source -I$OWL/Libraries/CMSIS/DSP/Include"
for f in basicmaths fastlog fastpow; do
    clang -O2 -ffast-math -w $INC -c "$OWL/LibSource/$f.c" -o "$BUILD/$f.o"
done
# macOS spells exp10f as __exp10f.
clang++ -std=gnu++17 -O2 -ffast-math -w -Dexp10f=__exp10f $INC \
    "$HERE/render.cpp" "$OWL/LibSource/FloatArray.cpp" "$OWL/LibSource/SimpleValue.cpp" \
    "$BUILD/basicmaths.o" "$BUILD/fastlog.o" "$BUILD/fastpow.o" -o "$BUILD/render"

if [ $# -gt 0 ] && [ "${1#*=}" = "$1" ]; then
    IN="$1"
    shift
    NAME=$(basename "$IN" .wav)
else
    IN="$OUT/demo_dry.wav"
    NAME=demo
    "$BUILD/render" --demo "$IN"
fi

for s in 0 0.5 0.75 1; do
    "$BUILD/render" "$IN" "$OUT/${NAME}_shimmer$s.wav" tail=6 shimmer=$s "$@"
done
