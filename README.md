# Factotum

*by New Horizon Electronics · driven by a need to turn it's hand at anything, for MOD Duo, Duo X and Dwarf*

![Factotum on a MOD pedalboard](factotum-face.png)

## Go! Do everything!

The Factotum started life as my tribute to the EA Parallel Universe, but wandered off to become its
own thing. At its heart is an LM386: a favourite of the DIY community that's been making tiny practice amps
and gain pedals sound huge for decades. Colin's (Experimentalists Anonymous) original "starve" switch remained
to provide sweet spluttering gated fuzz, but instead of the oscillator it gained a clean blend that keeps your 
pick attack in the room, a mid-scoop borrowed from the Marshall Shredmaster, and a vintage octave-up on its own footswitch. 

Overdrive, fuzz, gated splutter, scooped metal, ring-mod-ish octave squeal. It's all in there. And the best part is
that the way it was modelled still allows it to be built as a physical analog pedal.

## The controls

- **Gain**: how hard the LM386 gets pushed. Low is a gritty boost, high is a squared-off fuzz.
- **Clean**: how much clean signal is blended in underneath, Klon-style. It follows the Gain knob:
  lots of clean at low gain, a touch at high gain. 0 is fuzz only, 10 is a lot of you.
- **Level**: output volume. Noon is roughly unity, 10 is a serious boost.
- **Tone**: a treble cut, from dark-but-useful (2 kHz) to wide open (8 kHz).
- **Contour**: the Shredmaster's mid scoop. 0 is flat, noon takes a decent bite out of the mids,
  10 is the full scoop.
- **Starve**: drops the chip's supply to about 3.6 V. Lower, more lopsided clipping, and quiet notes
  gate and sputter as they decay. Volume is matched from about noon on the Gain knob up, so you can stomp it mid-song.
- **Octave** (footswitch): a Foxx Tone Machine-style octave up. Germanium diodes fold the wave in
  half, so the pitch doubles. Best on single notes above the 12th fret.
- **On/Off** (footswitch).

## Presets

| Preset | What it is |
| --- | --- |
| A bit of an edge | Light drive, a touch of scoop |
| Death Valley | Starved and dark |
| Bass sic | Starved fuzz with the octave, plenty of clean underneath |
| Blown speaker | Full gain, no scoop, flappy |
| 7ender 8lender | Octave fuzz, wide open, lots of clean |
| Dandy Warhol | Big fuzz with a strong clean blend |
| Garage Fuzz | Everything on, no clean: raw and spitting |
| Clean presence | Gain at 0: a starved, octaved clean boost |
| Mid boost OD | Medium drive, a moderate scoop |
| Contour Fuzz | Starved fuzz, barely scooped |
| Contour Fuzz II | Starved fuzz, more scoop, all the clean |

## Install

1. Connect your MOD by USB.
2. Open [builder.mod.audio](https://builder.mod.audio/buildroot) in Chrome and upload
   `mod-plugin-builder/nhe-factotum/nhe-factotum.mk` from this repo.
3. When the build finishes, click **Install**. Factotum appears under Distortion.

Mono in, mono out. Light on CPU.

## Credits and licence

Designed by Niels, New Horizon Electronics. Borrowing from Colin Raffel's 2006 schematic of the
Electronic Audio Experiments Parallel Universe. https://experimentalistsanonymous.com/stuff/paralleluniverse/firstedition.html
The Contour network's transfer function follows 
davemollen's [dm-Shredmaster](https://github.com/davemollen/dm-Shredmaster).

Code GPL-3.0-or-later (`LICENSE`). Artwork © 2026 New Horizon Electronics (`ARTWORK-LICENSE.md`).
Built with [DPF](https://github.com/DISTRHO/DPF) and the
[MOD plugin playbook](https://github.com/Kiwooky/MOD-plugin-playbook).

Building it yourself, the tests and how it works inside: [`DEVELOPER-NOTES.md`](DEVELOPER-NOTES.md)
and [`docs/spec.md`](docs/spec.md).
