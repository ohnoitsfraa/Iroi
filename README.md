# Iroi

Iroi is Oneiroi's effects section in a module.

## Shimmer fork

This fork adds a shimmer to the Ambience reverb: the reverb tail is pitch
shifted one octave up every time it's fed back, so it keeps climbing.

- **SHIFT + DECAY**: shimmer amount. Fully counter-clockwise is the stock
  reverb.
- The amount is saved together with the other SHIFT parameters.

## Building

The patch is built with [OwlProgram](https://github.com/RebelTechnology/OwlProgram)
for the OWL3 platform. It needs an ARM toolchain that includes newlib, such as
the official Arm GNU Toolchain (`brew install --cask gcc-arm-embedded`). The
Homebrew `arm-none-eabi-gcc` formula doesn't include it.

From the OwlProgram folder:

```
make PLATFORM=OWL3 CONFIG=Release libs
make PLATFORM=OWL3 CONFIG=Release PATCHSOURCE=../iroi-shimmer PATCHNAME=Iroi_1_0_1 clean sysex
```

The `sysex`, `load` and `store` targets need
[FirmwareSender](https://github.com/pingdynasty/FirmwareSender) in
`OwlProgram/Tools`.

## Flashing

- `make … clean load` runs the patch from RAM, a power cycle brings back the
  stored one.
- `make … clean store SLOT=1` stores it in flash.
- Or upload `Build/patch.syx` with the
  [OpenWare Laboratory](https://pingdynasty.github.io/OwlWebControl/extended.html)
  in Chrome, then press Store and pick slot 1.

To go back to the stock patch, upload `patch.syx` from the
[Befaco releases](https://github.com/Befaco/Iroi/releases) the same way.
