# Palozebra // VINYL — User Guide

Palozebra Vinyl is a VST3 audio effect that lets you grab incoming audio as if it were a spinning record.

## The basic idea

You have two ways to use it:

### 1. Insert mode

Put Vinyl directly after the sound source.

```text
AUDIO → PALOZEBRA VINYL → OUTPUT
```

or:

```text
MIDI → INSTRUMENT → PALOZEBRA VINYL → OUTPUT
```

### 2. Turntable / Source In mode

Put Palozebra Vinyl on its **own track or bus** and route another track into the plug-in's auxiliary input, named **Source In**.

```text
SOURCE TRACK
WAV / MP3 / synth / voice / drums
          │
          │ route / send / sidechain
          ▼
VINYL TRACK
PALOZEBRA VINYL — Source In
          │
          ▼
        OUTPUT
```

Your DAW controls this routing. Depending on the host, the control may be called **Sidechain**, **Aux Input**, **Audio From**, **Input Pins**, **Route**, or something similar.

When the auxiliary input is active, the plug-in interface shows **SOURCE IN**. Otherwise it shows **INSERT MODE**.

---

## How to scratch

- **Released:** normal playback at `1.0x`.
- **Click / hold:** grab the record and stop it under your hand.
- **Drag:** move the audio forward or backward, including reverse.
- **Release:** the platter returns to `1.0x` and continues from the virtual record position.

Pitch and speed stay linked, like a physical turntable.

---

# Recording a scratch

From v0.2, you do **not** need to learn your DAW's automation system just to record a scratch.

Palozebra Vinyl has its own gesture recorder:

- **REC** — starts a new wheel-performance take.
- **STOP REC** — ends the take.
- **PLAY** — replays the recorded wheel movement.
- **STOP** — stops gesture playback.
- **CLEAR** — deletes the current gesture take.

The plug-in records **the movement of the virtual record**, not the source audio itself.

That means you can record a scratch gesture once and replay the same movement over another sound later.

Example:

```text
SOURCE A → Vinyl → REC gesture → scratch

then change the source:

SOURCE B → Vinyl → PLAY same gesture
```

The gesture recorder currently supports one take of up to about 60 seconds.

> Prototype limitation: the internal gesture take is currently held in memory and is not yet saved permanently inside the DAW project when the plug-in is unloaded.

---

## So what exactly am I recording?

There are three different things and they should not be confused:

### A. The source

Your WAV, MP3, recorded audio, synth, sampler, etc.

### B. The scratch gesture

The movement of the Vinyl wheel.

Use **REC** inside Palozebra Vinyl to capture this.

### C. The final processed audio

If you want a permanent WAV/audio clip containing the scratch, use your DAW's normal audio-printing workflow after the performance is ready.

Depending on the DAW this may be called:

- Record Output
- Render
- Bounce
- Resample
- Freeze / Flatten
- Print FX

Conceptually:

```text
SOURCE → PALOZEBRA VINYL → PROCESSED AUDIO
              ↑
       recorded gesture
```

Then:

```text
PROCESSED AUDIO → render / record output → NEW AUDIO FILE
```

Palozebra Vinyl does not need to know whether the host calls a track “audio”, “MIDI”, “instrument”, or something else.

---

## What about MIDI?

MIDI is not required for normal scratch recording.

If the source begins as MIDI:

```text
MIDI → synth / sampler → audio → Palozebra Vinyl
```

Palozebra Vinyl scratches the resulting **audio**.

The optional **MIDI CC** switch emits CC74 from wheel movement. This remains an experimental secondary feature for routing/control; it is not the main recording workflow.

---

## What about DAW automation?

`Wheel Speed` is still exposed as an automatable plug-in parameter.

Advanced users can therefore record/edit the wheel through the host's automation system if they prefer. This is useful for precise editing, but it is no longer required for the basic workflow.

```text
Simple use:      REC button inside Vinyl
Advanced use:    DAW automation of Wheel Speed
Final audio:     render / bounce / record output in the DAW
```

---

# Quick start

## Insert mode

```text
1. Put Vinyl after the sound.
2. Press REC in Vinyl.
3. Move the record.
4. Press STOP REC.
5. Press PLAY to replay the gesture.
6. Render/record output only if you want a permanent audio file.
```

## Dedicated Vinyl track

```text
1. Put Palozebra Vinyl on an empty track/bus.
2. Route the source track to Vinyl's Source In / sidechain input.
3. Confirm that Vinyl shows SOURCE IN and signal.
4. Press REC.
5. Scratch the animated record.
6. Press STOP REC.
7. Press PLAY to replay the gesture over the incoming source.
8. Render/record the output when you want audio printed to a file/clip.
```

This concept is the same in every DAW; only the host's routing and render/record-output button names change.
