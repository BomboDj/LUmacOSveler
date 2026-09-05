# LUmacOSveler

LUmacOSveler is a JUCE/CMake VST3 loudness normalisation plugin for macOS.
Its interface is designed around a compact loudness-control workflow, with
Target Level, source loudness reference, dynamic correction, true-peak control,
and a fixed final limiter.

## Features

- K-weighted loudness analysis based on the ITU-R BS.1770 model.
- 400 ms momentary and 3 second short-term loudness windows.
- 400 ms integrated-gating blocks with a 100 ms hop.
- Fixed BS.1770/R128 gate values: -70 LUFS absolute and -10 LU relative.
- Target Level loudness matching.
- Input Level reference for known or previously measured source loudness.
- Learn Input mode for capturing gated integrated loudness from the input.
- Correction High and Correction Low dynamic gain controls.
- Four correction mix curves: linear/linear, linear/log, log/linear, log/log.
- Max Gain protection for the total loudness compensation.
- Freeze Level protection against isolated peaks raising the background.
- 4x oversampled True Peak protection with a 0.2 dB internal safety margin.
- Fixed linked-channel final limiter.
- Resizable editor with live gain-reduction meter and one-second peak hold.
- Sample-rate support from 8 kHz to 192 kHz.

## Default Parameters

| Parameter | Default |
| --- | ---: |
| Target Level | -20 LUFS |
| Max Gain | +12 dB |
| True Peak | -1 dBTP |
| Freeze Level | -30 LUFS |
| Input Level | -20 LUFS |
| Correction High | 100% |
| Correction Low | 100% |
| Correction Mix Mode | Linear / Linear |

## Gain Model

Input Level represents the known or measured integrated loudness of the source.
The fixed gain is calculated as:

```text
fixed gain = Target Level - Input Level
```

The controlled correction is calculated from the current loudness relative to
Input Level. The selected High or Low correction and Mix Mode determine how much
of that controlled correction is applied. The resulting total gain is always
clamped by Max Gain.

For example, with a source at -23 LUFS and a target of -16 LUFS:

```text
fixed gain = -16 - (-23) = +7 dB
```

If a quiet section measures -33 LUFS, the total correction can reach +17 dB,
provided Max Gain is set to at least +17 dB.

## Learn Input

1. Press `LEARN INPUT` before playback.
2. Play the desired programme or passage.
3. Press `STOP LEARN`.

The gated integrated loudness measured during the capture is written to Input
Level and remains fixed for subsequent gain processing. Continuous automatic
following is intentionally not used, because it would make the control loop
chase its own gain changes.

## Processing Chain

```text
K-weighted loudness analysis
-> Target and Input Level gain calculation
-> Correction High/Low and Mix Mode
-> Max Gain limit
-> Freeze Level increase protection
-> Gain application
-> 4x True Peak protection
-> Fixed final limiter
-> Output
```

The fixed final limiter uses:

```text
Threshold: 0 dB
Output: 0 dB
Lookahead: 0.1 ms
Knee: 0.1 dB
Release: 0.1 ms
```

## Build

JUCE is downloaded by CMake through `FetchContent`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target LUmacOSveler_VST3
```

The VST3 output is generated in:

```text
build/LUmacOSveler_artefacts/Release/VST3/LUmacOSveler.vst3
```

## Build and Install VST3/AU on macOS

The following commands build both plugin formats, install them in the current
user's standard macOS plugin folders, remove quarantine attributes, and apply
an ad-hoc code signature:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

cmake --build build \
    --config Release \
    -j"$(sysctl -n hw.ncpu)"

VST3_SOURCE="build/LUmacOSveler_artefacts/Release/VST3/LUmacOSveler.vst3"
AU_SOURCE="build/LUmacOSveler_artefacts/Release/AU/LUmacOSveler.component"

VST3_DEST="$HOME/Library/Audio/Plug-Ins/VST3/LUmacOSveler.vst3"
AU_DEST="$HOME/Library/Audio/Plug-Ins/Components/LUmacOSveler.component"

rm -rf "$VST3_DEST" "$AU_DEST"

mkdir -p \
    "$HOME/Library/Audio/Plug-Ins/VST3" \
    "$HOME/Library/Audio/Plug-Ins/Components"

ditto "$VST3_SOURCE" "$VST3_DEST"
ditto "$AU_SOURCE" "$AU_DEST"

xattr -c "$VST3_DEST"
xattr -c "$AU_DEST"

codesign --force --deep --sign - "$VST3_DEST"
codesign --force --deep --sign - "$AU_DEST"

codesign --verify --deep --strict --verbose=2 "$VST3_DEST"
codesign --verify --deep --strict --verbose=2 "$AU_DEST"
```

## Standards Note

The DSP follows the BS.1770/R128 processing structure, including K-weighting,
time windows, gating, and true-peak oversampling. It should not yet be treated
as a certified compliance meter: validation against official reference files,
all sample rates, true-peak meters, and complete programme-length integrated
loudness tests is still required.
