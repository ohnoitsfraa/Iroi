# Iroi

Iroi is Oneiroi's effects section in a module.

## Shimmer fork

This fork adds a shimmer to the Ambience reverb. A pitch-shifted copy of the
reverb tail is fed back into the reverb, so the tail climbs an interval every
time it goes around.

All controls are on the SHIFT layer: hold SHIFT and move the control.

| Control | What it does | Default |
|---|---|---|
| **SHIFT + DECAY** | **Amount**: how much shifted sound goes back into the reverb. Fully counter-clockwise is the stock reverb. | off |
| **SHIFT + ECHO REPEATS** | **Interval**, in six steps: octave down, fourth, fifth, octave, octave + fifth, two octaves. MAP blinks on each step. | octave |
| **SHIFT + RESONATOR FEEDBACK** | **Tone**: darker to brighter shimmer, 1.2 kHz to fully open. The bright side shows most with SPACETIME low. | noon |
| **SHIFT + MOD LEVEL** | **Spread**: detunes left and right against each other, up to 30 cents, for a wide, chorused shimmer. | off |
| **SHIFT + AMBIENCE fader** | **Climb**: up, every pass climbs another interval (rising choir). Down, the input is shifted once (a single layer). | up |
| **SHIFT + ECHO fader** | **Grain**: the shifter's window, from short and grainy (10 ms) to smooth (100 ms). | 50 ms |

- Moving a fader with SHIFT held leaves its volume where it was. The volume
  jumps to the fader position the next time you move it without SHIFT.
- All shimmer settings are saved with the other SHIFT settings (long press
  MAP). Saves from the stock patch load with the shimmer off.
- A good starting point: amount around three-quarters, interval on the octave
  or the fifth, a long DECAY, SPACETIME a bit past noon, Climb up.
- CPU, as reported by the module: 89% with the shimmer against 83% for the
  stock patch, with the same settings.

## Building

The patch is built with [OwlProgram](https://github.com/RebelTechnology/OwlProgram)
for the OWL3 platform. It needs the Arm GNU Toolchain **12.x** from
[Arm](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads).
Versions 13 and up don't build OwlProgram, and the Homebrew
`arm-none-eabi-gcc` formula has no newlib. Pass the toolchain's `bin/` folder
as `TOOLROOT`.

From the OwlProgram folder:

```
make PLATFORM=OWL3 CONFIG=Release TOOLROOT=/path/to/toolchain/bin/ libs
make PLATFORM=OWL3 CONFIG=Release TOOLROOT=/path/to/toolchain/bin/ PATCHSOURCE=../iroi-shimmer PATCHNAME=Iroi_1_0_1 clean sysex
```

The `sysex`, `load` and `store` targets need
[FirmwareSender](https://github.com/pingdynasty/FirmwareSender) in
`OwlProgram/Tools`.

## Flashing

Connect the module's USB to the computer, with the module powered from the
rack.

- `tools/owl.sh load` builds and runs the patch from RAM. A power cycle
  brings back the stored patch.
- `tools/owl.sh store` builds and stores it in flash slot 1.
- `tools/owl.sh status` shows what the module reports: patch, CPU, errors.

The script expects OwlProgram next to this repo and the toolchain in
`~/opt`; override with `OWLPROGRAM` and `TOOLROOT`. It sends with a small
CoreMIDI tool (macOS), because FirmwareSender can't send on recent macOS. On
other systems, `make … clean load` and `make … clean store SLOT=1` from
OwlProgram should work.

Or upload `Build/patch.syx` with the
[OpenWare Laboratory](https://pingdynasty.github.io/OwlWebControl/extended.html)
in Chrome, then press Store and pick slot 1.

To go back to the stock patch, upload `patch.syx` from the
[Befaco releases](https://github.com/Befaco/Iroi/releases) the same way.

## Listening without a module

`test/run_render.sh [in.wav] [key=value ...]` runs the effects chain on your
computer and writes the result at several shimmer amounts to `test/out/`.
Without a WAV it uses a built-in demo. Keys include `shimmer`, `interval`,
`spread`, `tone`, `climb`, `grain`, `decay`, `spacetime` and `ambvol`, as knob
positions from 0 to 1. See `test/render.cpp` for the full list.
