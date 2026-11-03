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
  parabolic windows normalized for constant power (the taps are uncorrelated
  on a reverb tail), one-pole LP (~5.3 kHz). Right channel window
  is 300 samples longer for width. Constants `kShimmer*` in `Commons.h`.
- `Ambience.h`: the shifter is in the cross-feedback (`GetFbOut()` of the
  other channel's diffuser): the shifted copy is added to the tail at
  `shimmer_ * kShimmerMaxAmount` (0.5) before the damp filters. The tail
  itself is left unchanged.
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
- ARM build (GCC 12.3, commit e8a7ce3): compiles, no new warnings.
  `arm-none-eabi-size`: text 79719 / data 1252 / bss 1100 vs stock
  78703 / 1236 / 1100. Shifter buffers are heap (SDRAM), not in these.
- Offline render: `test/run_render.sh [in.wav] [key=value ...]` builds
  `test/render.cpp` (the `Iroi` chain + `Clock`, no `Ui`, controls set as
  knob positions, filter fully open since it's also the input VCA, only the
  ambience mixed in) against OwlProgram and writes `test/out/*_shimmer{0,0.5,0.75,1}.wav`
  (32-bit float). Without a WAV it renders a demo source (three chords then
  silence). Needs the OwlProgram fast pow/log tables installed by hand.
- Render result (decay 0.8, spacetime 0.75, ambvol 0.6), level in dB per
  window:
  - First version (crossfading the tail into the shifted copy) killed the tail:
    at shimmer 1, the 9–12 s window was at −98 dB against 34 dB at shimmer 0.
    That's because the played notes drain by whatever share is shifted on every
    pass, and the octaves climbing past ~5 kHz die in the LP and damping.
    Equal-power crossfading plus a cap only got that to 13 dB.
  - Now additive: 12–18 s window is 29.3 dB at shimmer 0 and 33.8 dB at
    shimmer 1, with energy moved from below 250 Hz into 500 Hz–2 kHz. Bright
    spacetime (0.68) sustains, dark (0.9) keeps the shimmer subtle since the
    damping eats the octaves. Decay 1 doesn't run away, and peaks are unchanged
    (about −6 dBFS).
  - Shifter alone, RMS gain: about −6.5 dB on white noise (mostly the LP), and
    −5 to +1.7 dB on sines depending on how the two taps line up.
- ARM build of the additive version: text 79751.
- NOT yet: listening on hardware.

## Build and flash

- Toolchain: needs Arm GNU Toolchain **12.x**. GCC 13+ libstdc++ headers
  `#error` under OwlProgram's `-ffreestanding`, so the 15.x cask and the
  Homebrew formula (also no newlib) don't work. Installed without sudo from
  the Arm tarball at
  `~/opt/arm-gnu-toolchain-12.3.rel1-darwin-arm64-arm-none-eabi/` (15.3 is
  also in `~/opt`, unused). Homebrew's gcc is first on PATH, so always pass
  `TOOLROOT=$HOME/opt/arm-gnu-toolchain-12.3.rel1-darwin-arm64-arm-none-eabi/bin/`.
- [OwlProgram](https://github.com/RebelTechnology/OwlProgram) is cloned at
  `~/Git/OwlProgram`. Only `git submodule update --init Libraries/DaisySP`
  is needed (`Tools/hvcc` needs git-lfs, Pure Data only).
- `make libs` also builds the web libs and fails at `emcc` missing; the ARM
  libs it needs are already built by then, so ignore that.
- [FirmwareSender](https://github.com/pingdynasty/FirmwareSender) is cloned at
  `~/Git/FirmwareSender`. Its old JUCE won't build for arm64, so it's built
  x86_64 (runs via Rosetta) with `xcodebuild -configuration Release
  MACOSX_DEPLOYMENT_TARGET=12.0 ARCHS=x86_64 CODE_SIGNING_ALLOWED=NO` in
  `Builds/MacOSX`, copied to `OwlProgram/Tools/FirmwareSender`.
  `Tools/FirmwareSender -l` lists MIDI ports.
- From OwlProgram (all with `TOOLROOT=...` above):
  - `make PLATFORM=OWL3 CONFIG=Release libs` (once, done)
  - `make PLATFORM=OWL3 CONFIG=Release PATCHSOURCE=../iroi-shimmer PATCHNAME=Iroi_1_0_1 clean load`
    runs from RAM (power cycle restores the stored patch), use this to try things
  - `... clean store SLOT=1` stores to flash, `... clean sysex` writes
    `Build/patch.syx` for the
    [OpenWare Laboratory](https://pingdynasty.github.io/OwlWebControl/extended.html)
    web flasher (Chrome).
- Stock restore: `patch.syx` from the Befaco v1.0.1-beta release.
- In auto mode, running OwlProgram's `make` is blocked as external code until
  the user says to go ahead in the conversation.

## Next steps

1. User listens to `test/out/demo_shimmer*.wav` (additive version) and decides
   whether the amount (`kShimmerMaxAmount`) and brightness are right. The user
   prefers testing off-device before flashing.
2. `load` on the module, listen, tune `kShimmer*` (window, LP, amount curve).
3. Ideas: octave/fifth choice on SHIFT + ECHO REPEATS (ALT slot also `NULL`),
   shimmer CV/MIDI, LED feedback.
