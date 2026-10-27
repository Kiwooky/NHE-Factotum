#!/usr/bin/env python3
"""Tests for Factotum (traits: every, nonlinear/gain, level-sensitive).
Guitar level: gtr_train peaks around -16 dBFS."""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
for d in (os.environ.get('PB_TOOLS', ''), os.path.join(HERE, '..', 'tools')):
    if d and os.path.exists(os.path.join(d, 'lv2test.py')):
        sys.path.insert(0, d); break
import numpy as np
from lv2test import Plugin, check, done, db, rms, tick_count
sys.path.insert(0, HERE)
from gtr import gtr_train

ORDER = ['gain', 'clean', 'contour', 'tone', 'level', 'starve', 'octave', 'glow', 'lv2_enabled']
DEF = dict(gain=5, clean=7, contour=5, tone=5, level=5, starve=0, octave=0, glow=0, lv2_enabled=1)
p = Plugin(order=ORDER, defaults=DEF, n_in=1, n_out=1)
sr = 48000
xg = gtr_train(sr, 9.0)
def vs_bypass(**kw):
    y = p.run(xg, sr, **kw)[:, 0]; return db(rms(y) / rms(xg))

# ---- [gain] defaults about as loud as bypass (RMS, guitar level)
g = vs_bypass()
check('defaults within 3 dB of bypass', abs(g) < 3, '%+.1f dB' % g)
g0 = vs_bypass(gain=0)
check('Gain 0: the clean blend carries it (v1.0.1 without blend: -19.9 dB)', g0 > -14, '%+.1f dB vs bypass' % g0)
for kw in (dict(gain=10), dict(contour=0), dict(contour=10), dict(tone=0), dict(tone=10)):
    print('   info: %s  %+.1f dB vs bypass' % (kw, vs_bypass(**kw)))
g = vs_bypass(level=10)
check('Level 10: a real boost over bypass (+10 dB or more)', g > 10, '%+.1f dB' % g)
y = p.run(xg, sr, level=0)[:, 0]
check('Level 0: silent', np.abs(y).max() < 1e-6)


# ---- Clean knob (v1.0.5): 7 = the v1.0.4 blend, 0 = none, 10 = more
g7 = vs_bypass(gain=0)
g0c = vs_bypass(gain=0, clean=0)
g10c = vs_bypass(gain=0, clean=10)
check('Clean 7 = v1.0.4 (Gain 0 measured -10.8 dB then)', abs(g7 + 10.8) < 0.7, '%+.1f dB' % g7)
check('Clean 0: no clean, Gain 0 drops to the fuzz path alone (v1.0.1: -19.9 dB)', abs(g0c + 19.9) < 1.5, '%+.1f dB' % g0c)
check('Clean 10: more clean than 7 (+5 to +9 dB at Gain 0)', 5 < g10c - g7 < 9, '%+.1f dB' % (g10c - g7))
for c in (0, 10):
    print('   info: defaults with Clean %d: %+.1f dB vs bypass' % (c, vs_bypass(clean=c)))

# ---- Octave (v1.0.5): full-wave rectified, so the pitch doubles
def bands(y, f0):
    y = y[len(y) // 2:]
    F = np.abs(np.fft.rfft(y * np.hanning(len(y)))); fr = np.fft.rfftfreq(len(y), 1 / sr)
    a = lambda f: F[(fr > f * 0.97) & (fr < f * 1.03)].max()
    return db(a(2 * f0) / a(f0))
t = np.arange(sr) / sr
x = 0.15 * np.sin(2 * np.pi * 196 * t) * np.clip(t / 0.01, 0, 1)
kw = dict(gain=2, clean=0, contour=0, tone=10)
r_on, r_off = bands(p.run(x, sr, octave=1, **kw)[:, 0], 196), bands(p.run(x, sr, octave=0, **kw)[:, 0], 196)
check('Octave on: 392 Hz well above 196 Hz (the octave leads)', r_on > 6, '%+.1f dB' % r_on)
check('Octave off: 196 Hz leads', r_off < -6, '%+.1f dB' % r_off)
r_trace = bands(p.run(x, sr, octave=1, gain=0, clean=0, contour=0, tone=10)[:, 0], 196)
print('   info: Octave at Gain 0, Clean 0: octave vs fundamental %+.1f dB (some fundamental left, as on the pedal)' % r_trace)
for kw2 in (dict(), dict(gain=10), dict(starve=1)):
    print('   info: Octave on %s: %+.1f dB vs bypass' % (kw2 or 'defaults', vs_bypass(octave=1, **kw2)))
# v1.0.6 hardware report: "with the octave engaged the gate closes extremely quickly"
tt = np.arange(int(6 * sr)) / sr
note = sum(np.sin(2 * np.pi * 196 * h * tt + 0.3 * h) / h * np.exp(-tt / (1.2 / h ** 0.6)) for h in range(1, 8))
note *= np.minimum(1, tt / 0.003); note = 0.15 * note / np.abs(note).max()
def sustain(y, drop=30):
    w = int(0.02 * sr); e = np.array([rms(y[i:i + w]) for i in range(0, len(y) - w, w)]) + 1e-12
    e = 20 * np.log10(e / e.max()); i = int(np.argmax(e)); k = np.nonzero(e[i:] < -drop)[0]
    return (k[0] if len(k) else len(e) - i) * 0.02
s_on, s_off = sustain(p.run(note, sr, clean=0, octave=1)[:, 0]), sustain(p.run(note, sr, clean=0)[:, 0])
check('Octave sustain: at least 1.8 s to -30 dB (v1.0.5 gated at 1.1 s)', s_on >= 1.8, '%.2f s (octave off %.2f s)' % (s_on, s_off))
r_small = bands(p.run(0.02 * np.sin(2 * np.pi * 196 * t) * np.clip(t / 0.01, 0, 1), sr, octave=1, **kw)[:, 0], 196)
check('Octave on a quiet note (-34 dBFS): the octave still leads', r_small > 6, '%+.1f dB' % r_small)
g = vs_bypass(octave=1)
check('Octave at defaults: within 4 dB of bypass', abs(g) < 4, '%+.1f dB' % g)
x4 = 0.1 * np.sin(2 * np.pi * 4000 * t) * np.clip(t / 0.01, 0, 1)
y = p.run(x4, sr, gain=10, tone=10, contour=0, octave=1)[sr // 2:, 0]
F = np.abs(np.fft.rfft(y * np.hanning(len(y)))); fr = np.fft.rfftfreq(len(y), 1 / sr)
alias = db(F[(fr > 100) & (fr < 3800)].max() / F.max())
check('Octave: no aliasing below a 4 kHz sine at Gain 10', alias < -50, '%.1f dB re strongest' % alias)
xs5 = gtr_train(sr, 6.0, attack=0.005)
y = p.run(xs5, sr, events=[(int(1.0 * sr), 'octave', 1), (int(2.5 * sr), 'clean', 0), (int(3.5 * sr), 'clean', 10), (int(4.5 * sr), 'octave', 0)])
check('switching Octave and moving Clean while playing: finite', np.isfinite(y).all(), 'peak %.1f dBFS' % db(np.abs(y).max()))
# Octave: tick_count flags the octave fuzz's own bright edges since v1.0.6 (the pre-biased
# octave is stronger). A click is a sample step out of proportion to the signal; check that instead:
# the biggest step, relative to the peak, no worse than with the octave off (measured 0.56 vs 0.58)
def step_ratio(y):
    s_ = slice(int(0.19 * sr), int(0.26 * sr))
    return np.abs(np.diff(y[s_])).max() / np.abs(y[s_]).max()
ro, rf = step_ratio(p.run(xs5, sr, octave=1)[:, 0]), step_ratio(p.run(xs5, sr)[:, 0])
check('Octave: no clicks at the pick (biggest step vs peak, same as octave off)', ro < 1.15 * rf, '%.2f vs %.2f' % (ro, rf))

# ---- Starve: same loudness as normal (first hardware test: Level 5 -> 9 to match), but gated
for gn in (5, 2, 8, 10):
    d = vs_bypass(gain=gn, starve=1) - vs_bypass(gain=gn)
    if gn == 5:
        check('Starve at defaults: within 1.5 dB of Starve off (make-up boost)', abs(d) < 1.5, '%+.1f dB' % d)
    else:
        print('   info: Starve vs off at Gain %d: %+.1f dB' % (gn, d))
x = gtr_train(sr, 4.0)
def sounding(y):
    e = np.convolve(np.abs(y), np.ones(480) / 480, 'same'); return (e > 10 ** (-50 / 20) * np.abs(y).max() / 0.05).mean()
n0 = p.run(x, sr)[:, 0]; n1 = p.run(x, sr, starve=1)[:, 0]
check('Starve still gates the decays (sounds for less of the time)', sounding(n1) < 0.8 * sounding(n0),
      '%.0f%% vs %.0f%% of the time' % (100 * sounding(n1), 100 * sounding(n0)))

# ---- Contour: Shredmaster mid scoop. Read at Gain 0 and a small signal, so the stage is linear
def band(f, **kw):
    t = np.arange(sr) / sr
    x = 0.003 * np.sin(2 * np.pi * f * t) * np.clip(t / 0.01, 0, 1)
    return rms(p.run(x, sr, gain=0, tone=10, **kw)[sr // 2:, 0])
for c in (0, 5, 10):
    lo, mid, hi = band(100, contour=c), band(700, contour=c), band(3000, contour=c)
    print('   info: Contour %d: 700 Hz is %+.1f dB vs 100 Hz, %+.1f dB vs 3 kHz' % (c, db(mid / lo), db(mid / hi)))
lo, mid, hi = band(100, contour=10), band(700, contour=10), band(3000, contour=10)
check('Contour 10: deep scoop at 700 Hz (Shredmaster: about -24 dB vs 100 Hz)', db(mid / lo) < -18 and db(mid / hi) < -10,
      '%+.1f dB vs 100 Hz, %+.1f dB vs 3 kHz' % (db(mid / lo), db(mid / hi)))
lo, mid = band(100, contour=0), band(700, contour=0)
check('Contour 0: about flat', abs(db(mid / lo)) < 1.5, '%+.1f dB' % db(mid / lo))
for srt in (44100, 96000):
    t = np.arange(srt) / srt
    r = []
    for f in (100, 700):
        xx = 0.003 * np.sin(2 * np.pi * f * t) * np.clip(t / 0.01, 0, 1)
        r.append(rms(p.run(xx, srt, gain=0, tone=10, contour=10)[srt // 2:, 0]))
    check('Contour 10 scoop the same at %d Hz' % srt, db(r[1] / r[0]) < -18, '%+.1f dB' % db(r[1] / r[0]))

# ---- Gain sweeps from overdrive to fuzz: more gain = more squashed (crest factor falls)
def crest(y): return db(np.abs(y).max() / rms(y))
x = 0.1 * np.sin(2 * np.pi * 196 * np.arange(sr) / sr) * np.clip(np.arange(sr) / (0.01 * sr), 0, 1)
c0 = crest(p.run(x, sr, gain=0, contour=0, tone=10)[sr // 2:, 0]); c10 = crest(p.run(x, sr, gain=10, contour=0, tone=10)[sr // 2:, 0])
check('Gain 0 nearly clean, Gain 10 squared off', c0 > 2.6 and c10 < 1.5, 'crest %.1f dB / %.1f dB (sine 3.0, square 0)' % (c0, c10))

# ---- [gain] aliasing: a 4 kHz sine at full gain makes nothing below it
t = np.arange(sr) / sr
x = 0.1 * np.sin(2 * np.pi * 4000 * t) * np.clip(t / 0.01, 0, 1)
for st in (0, 1):
    y = p.run(x, sr, gain=10, tone=10, contour=0, starve=st)[sr // 2:, 0]
    F = np.abs(np.fft.rfft(y * np.hanning(len(y)))); fr = np.fft.rfftfreq(len(y), 1 / sr)
    alias = db(F[(fr > 100) & (fr < 3800)].max() / F.max())
    check('no aliasing below a 4 kHz sine at Gain 10 (Starve %d)' % st, alias < -50, '%.1f dB re fundamental' % alias)

# ---- [every] bypass: dry at unity, click-free
x = 0.1 * np.sin(2 * np.pi * 220 * np.arange(int(2.5 * sr)) / sr)
y = p.run(x, sr, events=[(sr, 'lv2_enabled', 0)])[:, 0]
a, b = int(1.2 * sr), int(2.4 * sr)
check('bypass: dry at unity', np.abs(y[a:b] - x[a:b]).max() < 1e-4, 'resid %.6f' % np.abs(y[a:b] - x[a:b]).max())
y = p.run(x, sr, lv2_enabled=0)[:, 0]
check('first run bypassed: dry from sample 0', np.abs(y - x).max() < 1e-5)
xs = gtr_train(sr, 6.0, attack=0.005)
y = p.run(xs, sr, events=[(int(1.0 * sr), 'lv2_enabled', 0), (int(2.0 * sr), 'lv2_enabled', 1),
                          (int(3.0 * sr), 'starve', 1), (int(3.5 * sr), 'contour', 10), (int(4.0 * sr), 'gain', 10),
                          (int(4.5 * sr), 'contour', 0), (int(5.0 * sr), 'starve', 0), (int(5.5 * sr), 'tone', 0)])
check('switching and knob moves while playing: finite', np.isfinite(y).all(), 'peak %.1f dBFS' % db(np.abs(y).max()))
for kw in ({}, dict(contour=10)):
    y = p.run(xs, sr, **kw)[:, 0]
    check('no ticks %s' % (kw or 'defaults'), tick_count(y, sr) == 0, '%d' % tick_count(y, sr))
# Starve: tick_count flags every pluck's attack, because the gating empties the treble out of
# the decays around it (the detector compares against nearby treble). Check the attack itself:
# no brighter than the same attack with Starve off (measured: -41.4 vs -42.5 dB, v1.0.1)
from scipy.signal import butter, sosfilt
def attack_hf(y):
    h = sosfilt(butter(4, 12000, 'highpass', fs=sr, output='sos'), y)
    s = slice(int(0.20 * sr), int(0.23 * sr))
    return db(np.sqrt(np.mean(h[s] ** 2) / np.mean(y[s] ** 2)))
a0, a1 = attack_hf(p.run(xs, sr)[:, 0]), attack_hf(p.run(xs, sr, starve=1)[:, 0])
check('Starve: pluck attacks no brighter than with Starve off (no clicks)', a1 < a0 + 3, 'treble %.1f dB vs %.1f dB re signal' % (a1, a0))

# ---- Glow output (v1.0.8): drives the face's light panel. Rises with the playing,
# falls over a couple of hundred ms, dark when bypassed or silent
def glow(x, **kw):
    return p.run(x, sr, trace='glow', **kw)[1]
gp = glow(xg)
check('Glow: lights up while playing (peak 0.4..1)', 0.4 < gp.max() <= 1.0, 'peak %.2f' % gp.max())
check('Glow: always within 0..1', gp.min() >= 0 and gp.max() <= 1, '%.2f..%.2f' % (gp.min(), gp.max()))
x1 = np.concatenate([xg[:2 * sr], np.zeros(sr)])
g1 = glow(x1); blk = sr / 256.0
late = g1[int(2.6 * blk):]
check('Glow: dark again within 0.6 s of silence', late.max() < 0.02, '%.3f' % late.max())
gq = glow(xg * 0.1)
print('   info: Glow peak at -20 dB playing: %.2f, full: %.2f' % (gq.max(), gp.max()))
check('Glow: follows playing level (quiet < loud)', gq.max() < gp.max() - 0.2)
gb = glow(xg, lv2_enabled=0)
check('Glow: dark while bypassed', gb.max() < 0.02, '%.3f' % gb.max())
gs = glow(np.zeros(sr))
check('Glow: dark on silence', gs.max() < 1e-3)

# ---- [every] torture: extremes, hot input, three rates: finite
for srt in (44100, 48000, 96000):
    xt = 0.8 * np.random.default_rng(1).standard_normal(int(2 * srt))
    for st in (0, 1):
        y = p.run(xt, srt, gain=10, level=10, starve=st, contour=0, tone=10, octave=st, clean=10)
        check('torture %d Hz Starve %d: finite' % (srt, st), np.isfinite(y).all(), 'peak %.1f dBFS' % db(np.abs(y).max()))

# ---- [every] silence
for st in (0, 1):
    y = p.run(np.zeros(sr), sr, starve=st)
    check('silence in, silence out (Starve %d)' % st, np.abs(y).max() < 1e-6)
done()
