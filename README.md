# Palozebra // VINYL — V0.1.1 prototype

A one-wheel VST3 audio effect that behaves like grabbing a spinning record.

**New user? Read the DAW-independent [User Guide](USER_GUIDE.md).**

## Interaction

- **Released:** platter runs at `1.0x`.
- **Mouse down / hold:** grabs the record and drives speed to `0`.
- **Drag clockwise / counter-clockwise:** playback speed follows angular hand velocity, including reverse.
- **Release:** the motor returns to `1.0x` and playback continues from the point where the virtual record was left. It does not auto-fast-forward to catch the DAW timeline.
- **Automation:** `Wheel Speed` is a published plug-in parameter intended to record and replay scratch gestures in any DAW that supports plug-in automation.
- **Optional MIDI:** emits CC74 (channel 1) mapped from `-4x..+4x` to `0..127`. This is experimental/secondary; host automation is the intended recording path.

## Signal model

`Audio input -> circular history buffer -> variable-speed read head -> cubic interpolation -> output`

Pitch and time stay coupled, like physical vinyl. There is deliberately no time-stretch, crackle, vinyl EQ, or cosmetic "lo-fi" processing in V0.1.x.

## Recording model

The core workflow is DAW-independent:

```text
Audio source -> Palozebra Vinyl -> Output
                    ↑
            Wheel Speed automation
```

For a MIDI instrument:

```text
MIDI -> Instrument -> Palozebra Vinyl -> Output
                           ↑
                   Wheel Speed automation
```

The scratch performance is normally recorded as **plug-in automation**, not as MIDI. Once the performance is right, the processed result can optionally be rendered/bounced/resampled to audio.

See [USER_GUIDE.md](USER_GUIDE.md) for the full explanation.

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

## V0.1.1 known tradeoffs

- ~750 ms nominal history latency gives room for reverse/forward manipulation. This is intentional but too high for some live-performance uses.
- After a scratch, playback continues from the virtual record position rather than silently catching up to the DAW timeline.
- Drag sensitivity currently maps one mouse revolution/second to 1x platter speed.
- No oversampling/de-click stage yet beyond speed smoothing + cubic interpolation.
- Optional MIDI CC74 output is experimental and secondary to automation.

## V0.2 candidates after listening

- Explicit **Sync / Return to Live** gesture with a short crossfade instead of audible catch-up.
- Low-latency / long-buffer mode switch.
- Adjustable platter inertia/torque hidden in an advanced panel.
- Better transient-safe de-clicking at extreme direction changes.
- Optional internal gesture recorder/export, if host automation is not enough.
- Hardware-controller learn / dedicated MIDI input control.
