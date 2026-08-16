# Palozebra // VINYL — V0.3.0 prototype

A one-wheel VST3 audio effect that behaves like grabbing a spinning record.

**New user? Read the DAW-independent [User Guide](USER_GUIDE.md).**

## Core workflow

V0.3 returns to the simplest architecture: **one track, one plug-in, no routing setup**.

For audio:

```text
WAV / MP3 / recorded audio -> Palozebra Vinyl -> Output
```

For an instrument track:

```text
MIDI -> Instrument / Synth -> Palozebra Vinyl -> Output
```

Palozebra Vinyl is a normal non-destructive insert. The original audio/MIDI source is not modified; bypassing or removing the plug-in returns the clean source.

## Internal gesture recorder

You do not need DAW automation just to capture a scratch performance:

- **REC** starts a new wheel-performance take.
- **STOP REC** ends it.
- **PLAY** replays the recorded wheel movement.
- **STOP** stops gesture playback.
- **CLEAR** deletes the take.

The plug-in records **the platter movement**, not the audio. The gesture is sampled at 200 Hz, interpolated during playback, and stored inside the plug-in state so it can return with the DAW project.

`Wheel Speed` remains a published automatable parameter for advanced users who prefer host automation.

## Interaction

- **Released:** platter runs at `1.0x`.
- **Mouse down / hold:** grabs the record and stops it under your hand.
- **Drag:** playback speed follows angular hand velocity, including reverse.
- **Release:** a short motor-like speed curve returns the platter to `1.0x`, producing a natural pitch bend. It is roughly 45–105 ms depending on release speed and direction, capped at 120 ms.
- **No catch-up:** after scratching, Vinyl continues from the virtual record position. It never fast-forwards to catch the DAW timeline.

The release transition changes speed/pitch, not volume. There is no intentional fade-out/fade-in.

## Signal model

`Audio input -> circular history buffer -> variable-speed read head -> cubic interpolation -> output`

Pitch and time stay coupled, like physical vinyl. There is deliberately no time-stretch, crackle, vinyl EQ, or cosmetic "lo-fi" processing.

## Build prerequisites

- CMake 3.22+
- C++20 compiler (MSVC on Windows)
- JUCE 8.0.8 checkout in `./JUCE`, **or** internet access so CMake can fetch JUCE automatically.

### Windows / Visual Studio Build Tools

From PowerShell in the repository root:

```powershell
.\build-windows.ps1
```

The script checks Git/CMake, downloads JUCE 8.0.8 if needed, configures the project, runs the smoke test, and builds the VST3.

The resulting `.vst3` bundle will typically be under:

`build/PalozebraVinyl_artefacts/Release/VST3/Palozebra Vinyl.vst3`

On Windows, install the **whole `.vst3` bundle** in a VST3 location scanned by your DAW, typically:

`C:\Program Files\Common Files\VST3\Palozebra Vinyl.vst3`

Do not copy only the inner binary from `Contents/x86_64-win`.

## V0.3 known tradeoffs

- ~750 ms nominal history latency gives room for reverse/forward manipulation.
- After a scratch, playback continues from the virtual record position rather than returning to the DAW timeline.
- Drag sensitivity currently maps one mouse revolution/second to 1x platter speed.
- No oversampling/de-click stage yet beyond speed smoothing + cubic interpolation.
- One internal gesture take, maximum about 60 seconds.
- Audio printing still uses the DAW's normal render/bounce/resample/record-output workflow.

## Next candidates

- Explicit **Sync / Return to Live** gesture, separate from normal wheel release.
- Multiple gesture takes / slots.
- Low-latency / long-buffer mode switch.
- Adjustable platter inertia/torque.
- Better transient-safe de-clicking at extreme direction changes.
- Hardware-controller input if it proves useful.
