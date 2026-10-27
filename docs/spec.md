# Factotum: spec

Every control mapping, as built. Source: `plugins/nhe-factotum/FactotumPlugin.cpp`.

## Ports

| Index | Symbol | Range | Default | Notes |
| --- | --- | --- | --- | --- |
| 0 | `in` | audio | | mono |
| 1 | `out` | audio | | mono |
| 2 | `gain` | 0 … 10 | 5 | |
| 3 | `clean` | 0 … 10 | 7 | |
| 4 | `contour` | 0 … 10 | 5 | |
| 5 | `tone` | 0 … 10 | 5 | |
| 6 | `level` | 0 … 10 | 5 | |
| 7 | `starve` | toggle | 0 | |
| 8 | `octave` | toggle | 0 | |
| 9 | `glow` | output, 0 … 1 | | drives the face's light panel |
| 10 | `lv2_enabled` | toggle | 1 | bypass, `lv2:designation lv2:enabled` |

Ports are frozen from 1.0.8: new ones go before `lv2_enabled` only with a version bump and a
"re-add the plugin" note.

## Signal flow

in → TL072 input amp (×3.13, soft rails ±4 V) → 4× oversampling →
[Octave: phase splitter, 0.99 / −0.93, two pre-biased Ge diodes (0.05 V left of the turn-on, knee
`w²/(w + 0.04)`), 10 Hz coupling cap; the footswitch crossfades the feed] → Gain pot →
[Starve: small-signal gate, 0.03 V] → LM386 ×200, asymmetric soft clip to the rails →
mixer (fuzz × make-ups + clean) → decimate → 8 Hz DC block → Contour → Tone → Level → out.

## Mappings

- **Gain**: −46 dB … 0 dB into the LM386, linear in dB.
- **Clean**: blend `(0.6 + 1.2 (1 − gain/10)) × (10^(1.2 c) − 1) / 5.9183`, c = clean/10 (a 50kA
  pot feel; 7 → 1.0).
- **Level**: `(level/10)³`.
- **Tone**: one-pole low-pass at `2000 × 4^(tone/10)` Hz (2 … 8 kHz).
- **Contour**: the Shredmaster network (R 100 Ω, 33k, 33k, 100k pot; C 1 nF, 100 nF, 47 nF),
  third-order, bilinear; pot position `1 − (1 − contour/10)³` (cubic taper).
- **Starve**: rail 9 V → 3.6 V, settling over ~30 ms, sagging up to 1.2 V with the output;
  LM386 swing `+max(0.25, rail/2 − 1)`, `−max(0.35, rail/2 − 0.5)`. Make-up ×6.24 on the fuzz.
- **Octave**: make-up ×1.667 on the fuzz (100k ‖ 150k).
- **Output scale**: 0.336 (defaults ≈ bypass loudness).
- All controls smoothed over 10 ms; bypass is a 10 ms crossfade to dry.

## Glow (the light panel)

Peak follower on the input: instant rise, 180 ms exponential fall. Level in dBFS mapped
−48 … −12 → 0 … 1, then `x^1.5`, times the bypass crossfade (dark while bypassed). Updated once per
block. The face shows it as the panel's opacity, never below 0.12.

## Face

325 × 400 CSS px, 2× art. Panel layers `panel.png` (Starve off) and `panel-starve.png` (Starve on)
in one container whose opacity the script sets; LEDs and controls above it. Positions from the
holes in the face art, fitted to Niels's mockup.
