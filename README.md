# Palozebra // VINYL — V0.4.0 prototype

A one-wheel VST3 audio effect that behaves like grabbing a spinning record.

**New user? Read the DAW-independent [User Guide](USER_GUIDE.md).**

## Core workflow

Palozebra Vinyl uses the simplest architecture: **one track, one plug-in, no routing setup**.

For audio:

```text
WAV / MP3 / recorded audio -> Palozebra Vinyl -> Output
```

For an instrument track:

```text
MIDI -> Instrument / Synth -> Palozebra Vinyl -> Output
```

Palozebra Vinyl is a normal non-destructive insert. The original audio/MIDI source is not modified; bypassing or removing the plug-in returns the clean source.

## V0.4 timeline recorder

V0.4 changes the internal recorder so scratch performances are stored **in place on the DAW timeline**.

- **REC** arms a new take but does not place it yet.
- The **first platter touch** while the host is playing fixes TAKE 01 to that timeline position.
- **STOP REC** ends the take.
- Normal DAW playback automatically replays the take when the transport reaches its saved position.
- **CLEAR** deletes the take.
- There is no separate internal PLAY button.

The plug-in records **the platter movement**, not the source audio. The gesture is sampled at 200 Hz, interpolated at audio rate, and stored in the plug-in state together with its timeline position.

`Wheel Speed` remains a published automatable parameter for advanced users who prefer host automation.

The implementation uses JUCE's `AudioPlayHead::PositionInfo` to read the host transport time. If the host does not provide usable timeline information, Vinyl leaves REC armed rather than silently placing the take incorrectly.

## Transport behavior

Normal platter release and DAW transport jumps are intentionally different:

- **Wheel release:** short motor-like pitch bend back to `1.0x`; no catch-up to the host timeline.
- **DAW rewind / seek / loop:** Vinyl re-aligns its internal read head to the new transport position so a placed take manipulates the same source material on the next pass.

This means a scratch can remain physically displaced after release during continuous playback, while still being repeatable after an explicit transport reposition.

## Interaction

- **Released:** platter runs at `1.0x`.
- **Mouse down / hold:** grabs the record and stops it under your hand.
- **Drag:** playback speed follows angular hand velocity, including reverse.
- **Release:** a short motor-like speed curve returns the platter to `1.0x`, producing a natural pitch bend. It is roughly 45–105 ms depending on release speed and direction, capped at 120 ms.
- **No release catch-up:** Vinyl never fast-forwards just because you let go of the record.

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

## V0.4 known tradeoffs

- ~750 ms nominal history latency gives room for reverse/forward manipulation.
- One placed internal gesture take, maximum about 60 seconds.
- The first platter touch is placed at audio-block resolution; normal block sizes make this a few milliseconds.
- A host must expose transport position through the plug-in API for in-place takes to work.
- After a scratch, continuous playback remains at the virtual record position rather than automatically returning to the unscripted source position.
- No oversampling/de-click stage yet beyond speed smoothing + cubic interpolation.
- Audio printing still uses the DAW's normal render/bounce/resample/record-output workflow.

## Next candidates

- Multiple placed gesture takes / slots.
- Explicit **Sync / Return to Live** control, separate from normal wheel release.
- Low-latency / long-buffer mode switch.
- Adjustable platter inertia/torque.
- Better transient-safe de-clicking at extreme direction changes.
- Hardware-controller input if it proves useful.
