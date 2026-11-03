#!/bin/sh
# Builds the patch with OwlProgram and talks to the module over USB MIDI.
#
#   tools/owl.sh build          build Build/patch.bin in OwlProgram
#   tools/owl.sh load           build, then run it from RAM (power cycle restores)
#   tools/owl.sh store [SLOT]   build, then store it in flash (default slot 1)
#   tools/owl.sh status         print what the module reports (patch, CPU, errors)
#
# OwlProgram's FirmwareSender can't send on recent macOS (CoreMIDI error -50
# from its old JUCE), so it only converts the binary to sysex here and
# sendsyx.swift does the sending.
#
# OWLPROGRAM defaults to ../OwlProgram, TOOLROOT to the Arm GNU Toolchain 12.3
# in ~/opt (GCC 13+ doesn't build OwlProgram), OWLDEVICE to OWL-IROI (matched
# as a substring of the MIDI port name).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$HERE")
OWL=${OWLPROGRAM:-$(dirname "$ROOT")/OwlProgram}
TOOLROOT=${TOOLROOT:-$HOME/opt/arm-gnu-toolchain-12.3.rel1-darwin-arm64-arm-none-eabi/bin/}
DEVICE=${OWLDEVICE:-OWL-IROI}
BUILD="$HERE/build"
mkdir -p "$BUILD"

tool() {
    if [ ! -x "$BUILD/$1" ] || [ "$HERE/$1.swift" -nt "$BUILD/$1" ]; then
        swiftc -O "$HERE/$1.swift" -o "$BUILD/$1"
    fi
    echo "$BUILD/$1"
}

build() {
    make -C "$OWL" PLATFORM=OWL3 CONFIG=Release TOOLROOT="$TOOLROOT" \
        PATCHSOURCE="$ROOT" PATCHNAME=Iroi_1_0_1 clean patch
}

send() {
    "$OWL/Tools/FirmwareSender" -q -d 0 -in "$OWL/Build/patch.bin" -save "$BUILD/patch.syx" "$@"
    "$(tool sendsyx)" "$BUILD/patch.syx" "$DEVICE" 2
}

case "$1" in
    build) build ;;
    load) build && send -run ;;
    store) build && send -store "${2:-1}" ;;
    status) "$(tool owlstatus)" "$DEVICE" 2 ;;
    *) sed -n 2,8p "$0" | sed 's/^# \{0,1\}//'; exit 1 ;;
esac
