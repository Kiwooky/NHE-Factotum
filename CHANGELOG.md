# Changelog

## 1.0.8 (build 8) — the face, and a GitHub repo
- Niels's faceplate: five knobs on a 101-frame filmstrip, Starve button, On/Off and Octave
  footswitches, and the light panel in the middle.
- The light panel's opacity follows a new **Glow** output (peak follower: instant rise, 180 ms
  fall, −48 … −12 dBFS → 0 … 1, dark while bypassed). Starve swaps the panel art (red-orange off,
  yellow on). The On/Off and Octave LEDs sit on top and don't fade with it.
- New output port `glow` (index 9) before `lv2_enabled`, which moves to 10. **Re-add the plugin on
  the pedalboard.** Sound unchanged.
- Now a GitHub repo (playbook path B): top-level Makefile, package `.mk`, DPF as a submodule, check
  workflow, playbook tools. URI unchanged.

## 1.0.7 (build 7) — factory presets
- Niels's 11 presets made on the unit: A bit of an edge, Death Valley, Bass sic, Blown speaker,
  7ender 8lender, Dandy Warhol, Garage Fuzz, Clean presence, Mid boost OD, Contour Fuzz,
  Contour Fuzz II.

## 1.0.6 (build 6) — octave sustain
- The octave's germanium diodes are pre-biased (returns at about 0.15 V, a trimmer on a real
  build), so the octave fades with the note instead of gating shut: sustain 1.1 s → 2.2 s to −30 dB.
- Octave make-up now 100k ‖ 150k in the mixer (+4.4 dB).

## 1.0.5 (build 5) — Clean knob, Octave
- **Clean** knob: sets the clean blend (7 = the 1.0.4 amount, 0 = none, 10 = about +6 dB more).
- **Octave** footswitch, Foxx Tone Machine style: phase splitter into two germanium diodes, before
  the Gain pot. The clean feed stays un-octaved.

## 1.0.4 (build 4) — renamed Factotum
- New name, new URI `urn:mod-cookbook:nhe-factotum`.

## 1.0.3 (build 3) — renamed Gruffdriver
- No longer a Parallel Universe, so it got its own name.

## 1.0.2 (build 2) — Tone range, clean blend
- Tone now a treble cut from 2 kHz to 8 kHz (the 500 Hz … 2 kHz half was muffled with any Contour).
- Klon-style clean blend riding the Gain knob: lots of clean at Gain 0, a little at Gain 10.

## 1.0.1 (build 1) — no oscillator, Starve make-up, Contour
- Oscillate switch and Freq knob removed (a digital stand-in for the pin-7 feedback loop didn't
  behave like the circuit).
- Starve gets an automatic make-up boost, so it stays at the same loudness.
- **Contour**: the Marshall Shredmaster's passive mid scoop, with a cubic taper so the scoop is
  spread over the whole turn.

## 1.0.0 (build 0) — Parallel Universe prototype
- An emulation of the EAE Parallel Universe from Colin Raffel's 2006 schematic: TL072 input amp,
  LM386 at gain 200, Starve, Tone, Level, Oscillate.
