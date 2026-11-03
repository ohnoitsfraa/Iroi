# Iroi shimmer fork

Fork of [Befaco/Iroi](https://github.com/Befaco/Iroi) (remote `upstream`) at
`ohnoitsfraa/Iroi` (remote `origin`). Goal: a really nice shimmer reverb on the
Befaco Iroi Eurorack module. Work happens on the `shimmer` branch. Never open
PRs against Befaco unless asked.

## The platform

- Iroi is an OWL patch (Rebel Technology OpenWare, `PLATFORM=OWL3`): STM32H7
  Cortex-M7, 512 KB patch RAM, 8 MB SDRAM (`new`/`FloatArray::create` land
  there), 48 kHz. Library headers (`Patch.h`, `FloatArray`, `BiquadFilter`,
  `DcBlockingFilter`, ...) come from OwlProgram's `LibSource`, not this repo.
- Entry point `Iroi_1_0_1Patch.hpp`, chain in `Iroi.h`:
  Filter → Resonator → Echo → Ambience (filter position is movable).
- Style: classes with static `create`/`destroy`, trailing-underscore members,
  constants as `constexpr k...` in `Commons.h`, controls in `PatchCtrls`.

## What's done (commit 0a58b07)

- `PitchShifter.h`: two delay taps half a cycle apart over a 50 ms window,
  parabolic windows normalized, one-pole LP (~5.3 kHz). Right channel window
  is 300 samples longer for width. Constants `kShimmer*` in `Commons.h`.
- `Ambience.h`: the shifter is in the cross-feedback (`GetFbOut()` of the
  other channel's diffuser), crossfaded by `shimmer_` before the damp filters.
  Shifters always run so their buffers stay current.
- `Ui.h`: SHIFT + DECAY = `ambienceShimmer` (that ALT slot was `NULL`),
  squared in `Ambience.h`. Saved/loaded as ALT `values[6]`/`cfg[6]` in
  `iroi.alt`; old saves load as 0.
- No MIDI CC for shimmer (MIDI CC numbers are the `ParamMidi` enum index and
  every `midiOuts_` entry must be created, so adding one needs care).

## Verified so far

- Whole patch syntax-checks with host clang against OwlProgram headers, same
  12 warnings as stock, none new.
- `test/run_pitchshifter_test.sh`: shifter alone, 440 → 880.5 Hz and
  1000 → 2000.5 Hz. 110 Hz reads ~230 Hz because splice artifacts add zero
  crossings at low frequencies.
- NOT yet: an ARM build, the full reverb rendered offline, or listening on
  hardware.

## Build and flash

- Toolchain: the Homebrew `arm-none-eabi-gcc` formula has no newlib
  (`inttypes.h` missing) and can't build OWL patches. Use
  `brew install --cask gcc-arm-embedded`.
- Clone [OwlProgram](https://github.com/RebelTechnology/OwlProgram) next to
  this repo (`~/Git/OwlProgram`). `git submodule update --init` fails on
  `Tools/hvcc` without git-lfs; that's only needed for Pure Data patches.
- `sysex`/`load`/`store` need [FirmwareSender](https://github.com/pingdynasty/FirmwareSender)
  built into `OwlProgram/Tools/FirmwareSender`.
- From OwlProgram:
  - `make PLATFORM=OWL3 CONFIG=Release libs` (once)
  - `make PLATFORM=OWL3 CONFIG=Release PATCHSOURCE=../iroi-shimmer PATCHNAME=Iroi_1_0_1 clean load`
    runs from RAM (power cycle restores the stored patch), use this to try things
  - `... clean store SLOT=1` stores to flash, `... clean sysex` writes
    `Build/patch.syx` for the
    [OpenWare Laboratory](https://pingdynasty.github.io/OwlWebControl/extended.html)
    web flasher (Chrome).
- Stock restore: `patch.syx` from the Befaco v1.0.1-beta release.
- In auto mode, running OwlProgram's `make` was blocked as external code; ask
  the user to approve it.

## Next steps

1. Install the Arm toolchain, clone OwlProgram, build FirmwareSender, build
   stock then shimmer for OWL3 (check `make size` for RAM).
2. Offline render: build the full patch natively (OwlProgram `native`/`run`,
   or a custom harness) and run a WAV through it so the user can listen
   before flashing. The user prefers testing off-device before flashing.
3. `load` on the module, listen, tune `kShimmer*` (window, LP, amount curve).
4. Ideas: octave/fifth choice on SHIFT + ECHO REPEATS (ALT slot also `NULL`),
   shimmer CV/MIDI, LED feedback.
