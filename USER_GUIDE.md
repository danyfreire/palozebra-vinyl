# Palozebra // VINYL — User Guide

Palozebra Vinyl is a VST3 audio effect that lets you grab incoming audio as if it were a spinning record.

The most important idea is simple:

> **You normally record the scratch as plug-in automation, not as MIDI and not directly as a new audio file.**

The exact buttons have different names in each DAW, but the workflow is the same.

---

## 1. Where do I put the plug-in?

Put **Palozebra Vinyl after the sound source**.

### Audio file

```text
WAV / MP3 / recorded audio
        ↓
Palozebra Vinyl
        ↓
Track output
```

### MIDI instrument

```text
MIDI notes
   ↓
Synth / sampler / instrument
   ↓
Palozebra Vinyl
   ↓
Track output
```

Palozebra Vinyl processes **audio**. If your track starts as MIDI, the instrument generates the audio first and Vinyl goes after it.

---

## 2. What happens when I move the record?

- **Released:** normal playback at `1.0x`.
- **Click / hold:** grab the record and stop it under your hand.
- **Drag:** move the audio forward or backward, including reverse.
- **Release:** the platter returns to `1.0x` and continues from the point where you left the virtual record.

Pitch and speed stay linked, like a physical turntable.

---

## 3. What exactly do I record?

Palozebra Vinyl exposes a plug-in parameter called:

**Wheel Speed**

That parameter describes the movement of the virtual record.

When your DAW records automation, it stores your scratch gesture as a curve:

```text
Wheel Speed

 1.0x ────────╲        ╱────────
               ╲      ╱
 0.0x           ╲____╱
                   ╲
-1.0x               ╲___
```

On playback, the DAW sends that movement back to Palozebra Vinyl and the plug-in repeats the scratch.

### Therefore

- **Audio/MIDI source:** remains where it already is.
- **Scratch performance:** record `Wheel Speed` automation.
- **Final processed sound:** optionally render/bounce/resample it to audio later.

You do **not** need to record MIDI for normal use.

---

## 4. DAW-independent recording workflow

Every major DAW has a way to write plug-in automation. The names vary, but the procedure is the same:

1. Put Palozebra Vinyl after your audio source or instrument.
2. Make the plug-in parameter **Wheel Speed** available for automation.
3. Enable your DAW's automation-write mode (`Touch`, `Latch`, `Write`, Automation Record, or equivalent).
4. Start playback/recording.
5. Open Palozebra Vinyl and perform the scratch with the mouse.
6. Stop recording.
7. Return the track to its normal automation-read mode.
8. Play the project again: the scratch should repeat automatically.

The recorded automation can usually be edited like any other envelope or automation lane.

---

## 5. Do I need another track?

### To perform and edit the scratch: **No**

The simplest setup is one track:

```text
SOURCE → PALOZEBRA VINYL → OUTPUT
              ↑
       Wheel Speed automation
```

### To create a permanent audio file: **Maybe**

Once the scratch is right, use your DAW's normal method to print processed audio. Depending on the DAW this may be called:

- Render
- Bounce
- Freeze / Flatten
- Resample
- Record track output
- Print FX

Conceptually:

```text
SOURCE → PALOZEBRA VINYL
              ↑
         automation
              ↓
        processed audio
              ↓
        NEW AUDIO FILE
```

This final audio file contains the scratch exactly as heard.

---

## 6. Audio track vs MIDI track

The workflow is the same.

### Audio track

```text
Audio clip → Vinyl
```

### MIDI track

```text
MIDI → Instrument → Vinyl
```

The scratch itself is still recorded as **Wheel Speed automation** in both cases.

---

## 7. What is the MIDI CC option?

Palozebra Vinyl can optionally emit MIDI CC74 based on wheel movement.

This is an experimental/secondary feature for routing gestures to other devices or tracks. It is **not required to record a scratch**.

For normal use, prefer your DAW's plug-in automation.

---

## 8. The shortest possible version

```text
1. Put Vinyl after the sound.
2. Record automation for Wheel Speed.
3. Scratch with the mouse.
4. Play back the automation.
5. Bounce/render to audio only when you want a permanent audio file.
```

---

# Guía rápida en español

La idea principal es esta:

> **Normalmente grabas el scratch como automatización del parámetro `Wheel Speed`; no como MIDI ni directamente como un archivo de audio nuevo.**

El flujo es igual en cualquier DAW:

```text
FUENTE DE AUDIO
      ↓
PALOZEBRA VINYL
      ↑
Automatización de Wheel Speed
      ↓
SALIDA
```

Si la fuente es MIDI:

```text
MIDI → instrumento/synth → Palozebra Vinyl → salida
```

Para grabar tu interpretación:

1. Activa la automatización del parámetro **Wheel Speed**.
2. Pon el DAW en modo de escritura de automatización (`Touch`, `Latch`, `Write` o equivalente).
3. Reproduce/graba y mueve el disco con el mouse.
4. El DAW guarda esos movimientos como una curva de automatización.
5. Al reproducir nuevamente, Palozebra Vinyl repite el scratch.

Cuando ya te guste el resultado, puedes convertirlo en audio usando el método normal de tu DAW: **Render, Bounce, Resample, Freeze/Flatten, Record Output**, etc.

El MIDI CC74 es opcional y experimental; no hace falta para el uso normal del plug-in.
