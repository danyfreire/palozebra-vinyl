# Palozebra // VINYL — User Guide

Palozebra Vinyl is a VST3 audio effect that lets you grab incoming audio as if it were a spinning record.

## The simple idea

Put Vinyl **on the same track you want to scratch**.

For audio:

```text
WAV / MP3 / recorded audio
          ↓
   Palozebra Vinyl
          ↓
        Output
```

For MIDI/instruments:

```text
MIDI
 ↓
Instrument / Synth
 ↓
Palozebra Vinyl
 ↓
Output
```

Vinyl processes audio, so on an instrument track it goes after the instrument.

You do not need a second Vinyl track, sidechain, send, aux input, or special routing.

---

## Is this destructive?

No.

Palozebra Vinyl is a normal insert effect. It does not rewrite your WAV, MP3, MIDI clip, or instrument part.

If you bypass or remove Vinyl, the original source is still there.

Only a later render/bounce/freeze operation can create a new processed audio file, depending on how your DAW works.

---

## How to scratch

- **Released:** normal playback at `1.0x`.
- **Click / hold:** grab the virtual record and stop it under your hand.
- **Drag:** move the audio forward or backward, including reverse.
- **Release:** the platter motor returns to `1.0x` with a very short speed curve, creating a natural pitch bend like releasing a real record.

The release is intentionally short: roughly 45–105 ms in normal cases, with a 120 ms maximum.

There is no intentional volume fade during release. The audible transition comes from **speed and pitch changing together**.

Vinyl also does **not** fast-forward afterward to catch the original DAW timeline. It continues from the virtual record position.

---

# Recording a scratch in place

From v0.4, a scratch take is attached to the **DAW timeline**.

There is no separate PLAY button inside Vinyl. Once a take is recorded, normal DAW playback triggers it automatically at the place where you performed it.

### Workflow

```text
1. Put Palozebra Vinyl on the track you want to scratch.
2. Start playback in your DAW.
3. Press REC in Vinyl. Vinyl is now armed.
4. Wait for the musical moment you want.
5. Touch the virtual record. That first touch fixes TAKE 01 to this timeline position.
6. Perform the scratch.
7. Press STOP REC when the take is finished.
8. Rewind the DAW and press Play normally.
9. When the transport reaches TAKE 01, Vinyl repeats the scratch automatically.
```

**REC records your hand movement on the virtual record. It does not create a new audio file.**

The wheel movement is sampled internally at 200 points per second and interpolated during playback.

The interface shows the take position and duration, for example:

```text
TAKE 01 · 00:37.420 · 1.83 s
```

If you press REC and change your mind before touching the platter, press **CANCEL**. The previous take remains intact.

Press **CLEAR** to delete TAKE 01.

The current prototype stores one take of up to about 60 seconds.

---

## What happens when I rewind, seek, or loop?

Vinyl reads the standard transport position supplied by the host.

When the DAW explicitly rewinds, seeks, or loops to another position, Vinyl re-aligns its internal virtual record with that transport jump. This is different from normal wheel release:

```text
Release the wheel → short pitch bend to 1x, no automatic catch-up.
DAW rewind/seek/loop → re-align to the new host timeline so TAKE 01 is repeatable.
```

This lets the same recorded scratch happen over the same musical material on another pass.

If a host does not supply a usable timeline position to the plug-in, Vinyl cannot place an in-place take; the interface will leave REC armed rather than silently placing it at the wrong time.

---

## What exactly remains original?

Your track still contains its original source:

```text
ORIGINAL SOURCE
      ↓
Palozebra Vinyl
      ↑
TAKE 01 @ timeline position
      ↓
what you hear
```

Bypass Vinyl:

```text
ORIGINAL SOURCE → clean output
```

So you do not need to duplicate the track just to protect the original.

Duplicate only when you actually want to hear **clean + scratched** versions at the same time.

---

## Saving the project

After STOP REC, TAKE 01 is stored in the plug-in state together with its timeline position.

Saving and reopening the DAW project should therefore restore the placed take.

---

## How do I make a permanent audio file?

Once the scratch is right, use your DAW's normal way of printing processed audio.

Depending on the DAW, this may be called:

- Render
- Bounce
- Resample
- Record Output
- Freeze / Flatten
- Print FX

This part is intentionally handled by the DAW so Palozebra Vinyl behaves consistently as a normal VST3 insert.

---

## What about DAW automation?

`Wheel Speed` remains exposed as an automatable plug-in parameter for advanced editing.

Automation is optional. The normal v0.4 workflow is:

```text
REC → touch platter → scratch → STOP REC
                   ↓
        saved at DAW timeline position
                   ↓
             rewind + DAW Play
```
