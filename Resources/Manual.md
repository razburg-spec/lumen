# Lumen User Guide

Lumen is a reverb, delay and granular shimmer processor by Raz Burg AudioCrafts. Three engines can run alone or together, in series or in parallel, with a global Coloration stage on the output.

## Quick start

- Lumen opens fully dry: every engine is off and Coloration sits at Neutral. You hear your track unchanged.
- Switch an engine on with the round power button at the top right of its column.
- Pick a starting point from the preset menu in the toolbar, then adjust to taste.
- Use Mix to blend the effect with the dry signal. On a send/aux track set Mix to 100%.
- Undo and Redo step back and forward through your last 100 changes. Reset returns every control to its default (fully dry).

## Toolbar

- Preset menu: click the preset name to browse the factory presets by category. The arrows step to the previous or next preset.
- Undo / Redo: each finished move of a control, switch or menu is one step. Loading a preset or pressing Reset is also one step, so it can be undone.
- Reset: returns the whole plugin to its initial state (all engines off, Coloration neutral).
- ? : opens this guide.

## Header

- Signal Flow chooses how the engines are connected:
- Serial: Delay feeds Shimmer, which feeds Reverb. Echoes are smeared into the space.
- Parallel: each engine hears the dry signal on its own, and their outputs are summed. Clearer and more defined.
- Reverb › Delay: the reverb comes first and the delay repeats the reverb tail. Good for big ambient throws.
- Licence badge: shows the trial or licence state and opens the activation panel.
- Advanced: shows every parameter of each engine, below its main knobs.

## Display

The display shows one time line with the dry hit, the delay repeats, the shimmer grains and the reverb envelope, each in its engine's colour. It follows every knob move immediately.

## Delay

A tempo-synced stereo delay with analog, tape and lo-fi characters.

- Time: delay time in milliseconds when Sync is off, or a note division (1/64 to 4/1, including dotted and triplet values) when Sync is on.
- Feedback: how many repeats. Above 100% the repeats grow and saturate, for runaway dub effects.
- Tone: high-cut on the repeats. Lower values give darker, softer echoes.
- Level: output level of the delay.
- Mode: Stereo, Ping-Pong (repeats bounce left and right), Dual (independent left and right lines) or Wide (the right side is offset by a few milliseconds for extra width).
- Character: Clean (digital), Analog (warm, slightly saturated), Tape (wow, flutter and tape saturation) or Lo-Fi (band-limited and gritty).
- Sync: locks the time to the host tempo.
- Freeze: holds the current repeats forever and ignores new input. Turn it off to release them.
- Advanced: Low-Cut (thins the repeats), Drive (saturation inside the feedback loop), Ducking (lowers the repeats while you play, so they bloom in the gaps), Mod Rate and Mod Depth (pitch movement of the repeats).

## Shimmer

A granular pitch shifter in a feedback loop, for octave-up halos and evolving textures.

- Pitch A and Pitch B: two grain voices, each shifted from -24 to +24 semitones. +12 is an octave up, +7 a fifth.
- Feedback: feeds the shifted grains back into the shifter, so each pass climbs or falls again.
- Level: output level of the shimmer.
- Reverse: Off, On (grains play backwards) or Random.
- Advanced: Grain (length of each grain, short is glassy and long is smooth), Density (how many grains overlap), Spray (random timing of the grains).
- Tip: in Serial routing the shimmer feeds the reverb, which turns the grains into a smooth wash.

## Reverb

An algorithmic reverb with ten modes and five era colors.

- Decay: how long the tail lasts (RT60), from 0.2 to 20 seconds.
- Size: the size of the space. Larger sizes spread the early reflections and slow the build-up.
- Pre-Delay: time before the reverb starts. 20 to 80 ms keeps vocals and drums clear in front of the space.
- Level: output level of the reverb.
- Mode: Hall, Plate, Room, Chamber, Bloom (slow swell), Cloud (dense, diffuse wash), and four vintage studio models:
- 60s Chamber: a tiled echo chamber fed by a tape delay and a speaker and picked up by a microphone pair. Warm and dense in the low mids.
- 60s Mirror: a smaller chamber with hard, reflective walls. Brighter, with more flutter.
- 90s Hall: a modern digital hall with randomised, constantly moving delay lines for a smooth, lush tail.
- 90s Concert: the same algorithm with slower, chorus-like movement, a slower build and more bass.
- Color: the era voicing for the first six modes. Now is clean and full-range, 80s adds the grainy, band-limited sound of early digital units, 70s is darker and softer, 60s adds chamber-like midrange and saturation, and 90s adds random modulation and a fuller low end. The four vintage modes have their own voicing, so Color is disabled for them.
- Advanced: Diffusion (how quickly echoes blur into a tail), Mod Rate and Mod Depth (movement that avoids metallic ringing), Low-Cut and High-Cut (shape the tail), Width (from mono to full stereo).

## Output

- Mix: balance between dry and processed sound. With every engine off Lumen stays fully dry whatever Mix is set to.
- Cross-Feed: sends the end of the chain back to its start, so the engines feed each other. Use small amounts for richer, longer tails.
- Coloration: a global tone stage after everything else. Shine (right) adds bright 2nd and 3rd harmonics to the upper frequencies and a gentle presence lift. Dark (left) adds warm low-order harmonics to the low mids and softens the top. Neutral (centre) is an exact bypass. Double-click to return to Neutral.
- Output: final output level.

## Working with controls

- Drag up/down or left/right to turn a knob. Hold Cmd (Mac) or Ctrl (Windows) while dragging for fine control.
- Double-click a knob to return it to its default value.
- Hover over a control to see a short description.
- The window can be resized from the bottom-right corner. Everything scales and stays sharp.

## Using Lumen on an insert or a send

- Insert: put Lumen directly on a track and set Mix to taste, usually 10 to 35%.
- Send/aux: put Lumen on an aux or FX track, set Mix to 100%, and send to it from several tracks. The presets in the Send category are set up this way.
- Keep Pre-Delay above 20 ms and use Low-Cut on the reverb to keep the mix clear.

## Preset categories

- Vocals: plates, halls, slapback and throws for lead and background vocals.
- Drums: snare plates, drum rooms and big 80s snares.
- Guitars: echoes, swells, rooms and doublers.
- Keys & Piano: halls and rooms for piano, Rhodes and organ.
- Synths & Pads: shimmer pads, lush halls and rhythmic echoes.
- Strings & Orchestral: scoring stages and concert halls.
- Ambient & FX: freezes, reverse grains and runaway feedback.
- Vintage: the 60s, 70s, 80s and 90s studio sounds.
- Send (100% Wet): ready-made aux returns.

## Licence and trial

- Lumen runs fully for a 14-day trial. After that, enter the licence key from your purchase email in the activation panel.
- A licence activates one computer. To move Lumen to another computer, open the licence badge and choose Deactivate first.
- Lumen checks the licence online now and then, and keeps working offline in between.

## Support

Visit audiocrafts.razburg.com for updates and support.
