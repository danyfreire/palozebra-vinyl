# Palozebra // VINYL — V0.2.0 prototype

A one-wheel VST3 audio effect that behaves like grabbing a spinning record.

**New user? Read the DAW-independent [User Guide](USER_GUIDE.md).**

## V0.2 workflow

Palozebra Vinyl now supports two ways to work:

### Insert mode

```text
Audio source -> Palozebra Vinyl -> Output
```

For an instrument track:

```text
MIDI -> Instrument -> Palozebra Vinyl -> Output
```

### Dedicated turntable track

The plug-in exposes a second audio input bus named **Source In**. Put Vinyl on its own track/bus and route another track into that auxiliary input using your DAW's sidechain/aux-routing controls.

```text
SOURCE TRACK
     |
     | route / send / sidechain
     v
VINYL TRACK: Palozebra Vinyl / Source In
     |
     v
   OUTPUT
```

The host owns the routing; the plug-in simply exposes the Source In bus. The UI reports **SOURCE IN** when that auxiliary bus is active and **INSERT MODE** otherwise.

## Internal gesture recorder

V0.2 adds a DAW-independent wheel-performance recorder:

- **REC** starts a new gesture take.
- **STOP REC** ends it.
- **PLAY** replays the recorded wheel movement.
- **STOP** stops playback.
- **CLEAR** deletes the take.

The gesture is stored as platter-speed movement and can be replayed over whatever audio is currently entering Vinyl. The current prototype keeps one take, up to about 60 seconds, in memory.

`Wheel Speed` remains a published automatable parameter for users who prefer editing automation in the DAW. Optional MIDI CC74 output also remains available, but neither automation nor MIDI is required for basic internal gesture recording.

## Interaction

- **Released:** platter runs at `1.0x`.
- **Mouse down / hold:** grabs the record and drives speed to `0`.
- **Drag clockwise / counter-clockwise:** playback speed follows angular hand velocity, including reverse.
- **Release:** the motor returns to `1.0x` and playback continues from the virtual record position. It does not auto-fast-forward to catch the DAW timeline.

## Signal model

`Selected audio input -> circular history buffer -> variable-speed read head -> cubic interpolation -> output`

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

## V0.2 known tradeoffs

- ~750 ms nominal history latency gives room for reverse/forward manipulation.
- After a scratch, playback continues from the virtual record position rather than silently catching up to the DAW timeline.
- Drag sensitivity currently maps one mouse revolution/second to 1x platter speed.
- No oversampling/de-click stage yet beyond speed smoothing + cubic interpolation.
- One internal gesture take, maximum about 60 seconds.
- The internal gesture is not yet serialized into the DAW project state; unloading the plug-in clears it.
- Audio printing still uses the DAW's normal render/bounce/resample/record-output workflow.
- Source routing terminology and controls differ by DAW because the host controls plug-in buses.

## Next candidates

- Persist/compress gesture takes in plug-in state.
- Multiple gesture takes / slots.
- Explicit **Sync / Return to Live** gesture with a short crossfade.
- Low-latency / long-buffer mode switch.
- Adjustable platter inertia/torque.
- Better transient-safe de-clicking at extreme direction changes.
- Internal audio capture/export if it proves useful across hosts.
- Hardware-controller learn / dedicated MIDI input control.
