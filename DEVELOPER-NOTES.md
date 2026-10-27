# Factotum: developer notes

Not needed to play it; for anyone building, testing or changing it. Every control mapping is in
[`docs/spec.md`](docs/spec.md); history in [`CHANGELOG.md`](CHANGELOG.md).

URI `urn:mod-cookbook:nhe-factotum` (kept from the prototype so pedalboards and presets keep
working) · mono in, mono out · DistortionPlugin · DPF as a git submodule at `dpf/`, pinned to the
commit the [MOD plugin playbook](https://github.com/Kiwooky/MOD-plugin-playbook) tests against.

## Build and test

```sh
git clone --recursive https://github.com/Kiwooky/NHE-Factotum.git
make                                   # bin/nhe-factotum.lv2
tools/check.sh --src plugins/nhe-factotum --bundle bundle/nhe-factotum.lv2 --test tests/test_nhe_factotum.py
python3 tools/face_click_test.py bundle/nhe-factotum.lv2
python3 tools/render_face.py bundle/nhe-factotum.lv2            # screenshot + thumbnail
python3 tools/face_prototype.py bundle/nhe-factotum.lv2 bench.html   # face test bench in a browser
```

`check.sh` builds native, Duo (arm32) and Duo X/Dwarf (arm64) through the builder's own hooks,
compares the hand-written TTL with DPF's generator, and runs the audio tests natively and on the
Duo build under qemu (44 tests). Packages: `g++ g++-arm-linux-gnueabihf g++-aarch64-linux-gnu
qemu-user lilv-utils`, Python `numpy scipy rdflib` (plus `playwright pillow` for the face tools).

**Releasing:** push, then set `NHE_FACTOTUM_VERSION` in `mod-plugin-builder/nhe-factotum/nhe-factotum.mk`
to the full 40-character commit hash and upload that file at builder.mod.audio. Bump
`lv2:microVersion` (TTL) and `d_version()` (code) together on every release: MOD caches the face
and ports by version.

**Ports are frozen.** Pedalboards and presets save values by symbol and the face by index; add a
port only before `lv2_enabled`, with a version bump and a "re-add the plugin" note.

## Measured (native and Duo build under qemu, identical)

- Defaults +1.0 dB vs bypass (RMS, guitar level). Level 10: +19.1 dB.
- Gain 0: −10.8 dB vs bypass with Clean 7, −19.8 dB with Clean 0; Clean 10 is +5.9 dB over 7.
- Octave: the octave leads the fundamental by +22.6 dB on a 196 Hz note; +18.5 dB on a −34 dBFS
  note. Sustain to −30 dB 2.18 s (octave off 4.82 s).
- Starve at defaults −0.1 dB vs off; at Gain 2 −5.7 dB (the make-up is matched at noon and up).
  Plays 60 % of the time vs 86 % (the gating).
- Contour 10: 700 Hz −24.1 dB vs 100 Hz, −20.7 dB vs 3 kHz, the same at 44.1, 48 and 96 kHz.
  Contour 5: −6.1 dB.
- Aliasing below a 4 kHz sine at Gain 10: −168 dB (−186 dB with the octave).
- CPU, relative only (Duo build under qemu, vs NHE Taj Mahal): defaults 1.66×, Starve 1.69×,
  Octave 1.90×, both 1.85×, bypassed 0.03×. Most of it is the 4× oversampling around the LM386
  (two 64-tap filters). *The Duo's own meter is the real number.*
- Glow: peak 0.82 at guitar level, 0.18 twenty dB down, dark within 0.6 s of silence and while
  bypassed.

## Face

Artwork by Niels. 325 × 400 CSS px from 650 × 800 art (2× images drawn at 1×), quantised to 256
colours. Positions come from the black holes in the face art and template matching against the
approved mockup (mean difference 4.8/255 with everything on).

- Knobs: `knob.png`, 101 frames of 128 px (127 px art on a 128 px canvas so the CSS frame is a
  whole 64 px: mod-ui counts frames as strip width ÷ element width, and a fractional width
  shows two half frames). Made with `tools/knob_filmstrip.py` from Knob-body + Knob-index, 270°.
- Light panel: two layers in one container pinned to its own compositor layer (`will-change`),
  so the fade doesn't repaint the pedal. The script sets opacity `0.12 + 0.88 × glow`
  (`modgui:monitoredOutputs glow`) and the `starve` class picks the art. Lit by default in the
  CSS, so the screenshot shows it and it's visible before the script starts.
- LEDs: the left one is mod-ui's `bypass-light`; the right one follows Octave from the script.
  Nothing is drawn when they're off: the panel art has the LED bodies.
- Footswitches: bypass classes are inverted in mod-ui (`.off` = effect on → the ON art).

## Known issues

- **Frozen panel on MOD OS 1.14:** if the panel stops following your playing, the web interface
  has missed a `data_ready` handshake (the sound is fine). Reload with every other MOD tab closed.
