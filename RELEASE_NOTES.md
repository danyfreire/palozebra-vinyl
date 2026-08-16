# Palozebra Vinyl v0.4.1

First public MVP release of Palozebra Vinyl for Windows x64 (VST3).

## What it does

- Non-destructive VST3 insert: put Vinyl directly on the audio/instrument track you want to scratch.
- Grab and drag the animated record to control playback speed and direction, with pitch coupled to speed like physical vinyl.
- Short motor-like pitch bend back to 1.0x when the platter is released.
- Record one scratch gesture **in place** on the DAW timeline: `REC` -> touch platter -> scratch -> `STOP REC`.
- Rewind/seek and normal DAW playback automatically replays the recorded scratch at the same timeline position.
- The take is stored in the plug-in state so it can return with the project.
- `Wheel Speed` remains available as an automatable parameter for advanced use.

## Install (Windows)

1. Download `Palozebra-Vinyl-v0.4.1-Windows-x64-VST3.zip`.
2. Extract the archive.
3. Copy the whole `Palozebra Vinyl.vst3` bundle to:
   `C:\Program Files\Common Files\VST3\`
4. Restart/rescan your DAW.

Do not copy only the inner binary from `Contents\x86_64-win`.

## Current prototype limits

- Windows x64 VST3 build only.
- One internal scratch take, up to about 60 seconds.
- About 750 ms nominal history latency to provide room for forward/reverse manipulation.
- Final audio printing uses the DAW's normal render/bounce/resample/record-output workflow.

Palozebra Creative Lab experiment — https://palozebra.com
