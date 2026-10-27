// Factotum (New Horizon Electronics; called Gruffdriver until v1.0.4). Started as an emulation of the Electronic Audio
// Experiments Parallel Universe, from Colin Raffel's 2006 schematic ("revised with input and
// output buffers"), with thanks. It grew into its own pedal: the oscillator is gone, a
// Klon-style clean blend and a Shredmaster-style Contour are in. Renamed Gruffdriver in v1.0.3, Factotum in v1.0.4.
// Face: Niels's artwork (v1.0.8); the light panel follows the Glow output.
//
// Traits (docs/effect-profile.md): [every], nonlinear/gain, level-sensitive.
// No buffer, no tail, no feedback loop, no Mix knob.
//
// Signal flow:
//   TL072 input amp, gain 3.13 (1 + 100k/47k)
//   -> Gain pot: attenuator into the LM386 (about -46 dB .. 0 dB)          [guess: taper]
//   -> LM386 at gain 200 (10 uF across pins 1-8), soft-clipping to its rails,
//      asymmetric (it idles at half supply and swings closer to ground)    [approximate]
//   + Octave (ours, after the Foxx Tone Machine, v1.0.5): phase splitter + germanium diodes
//     before the Gain pot, on its own footswitch. The clean feed stays un-octaved
//   + Clean blend (ours): like a Klon, the Gain knob also fades a clean feed into the
//     fuzz, lots at Gain 0, a little at Gain 10. The Clean knob (v1.0.5) sets how much
//   -> Starve: supply through 2k2. The LM386's own current drops it to about 3.6 V:
//      lower, more lopsided clipping, and a gated, sputtery input stage   [approximate]
//      + a make-up boost (ours, not on the pedal) so Starve stays at the same loudness
//   -> Contour: the Marshall Shredmaster's passive mid-scoop network (100k pot, 33k/33k,
//      1n/100n/47n), transfer function as derived in davemollen's dm-Shredmaster. Ours, not on the pedal
//   -> TL072 output buffer -> "stupidly wonderful" tone (R15 1k, 10k pot, 22 nF)
//      as a treble cut, 2 kHz .. 8 kHz (the pedal goes darker; that half was useless here)
//   -> Level (100k pot, audio-taper response here)
// Left out: the Oscillate switch and Freq knob (the pin-7 feedback loop; v1.0.0 had a
// band-pass stand-in, which didn't behave like the circuit), Send/Return and expression.
// The LM386 stage runs at 4x the sample rate: fuzz aliases badly at 1x.
#include "DistrhoPlugin.hpp"

#include <cmath>
#include <cstring>
#include <cstdint>

START_NAMESPACE_DISTRHO

static inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static inline float onePoleCoef(float hz, float sr) { return 1.0f - std::exp(-6.2831853f * hz / sr); }

// rational tanh: slope 1 at 0, flat at +-1 beyond |x| = 3, no libm call
static inline float softClip(float x)
{
    if (x >  3.0f) return  1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// 4x oversampling: one 64-tap low-pass (Kaiser, -64 dB from 28 kHz at 48 kHz), polyphase
static const int kOS = 4;
static const int kTaps = 64;
static const int kPhaseTaps = kTaps / kOS;
static const float kFir[kTaps] = {
    1.269509306e-05f,
    6.982197704e-05f,
    1.447470168e-04f,
    1.600094188e-04f,
    1.785443369e-05f,
    -3.161541367e-04f,
    -7.249508335e-04f,
    -9.250025701e-04f,
    -5.853117058e-04f,
    4.364418813e-04f,
    1.861691008e-03f,
    2.944524962e-03f,
    2.743598200e-03f,
    6.749242491e-04f,
    -2.909334729e-03f,
    -6.500336395e-03f,
    -7.872855920e-03f,
    -5.174534760e-03f,
    1.768877929e-03f,
    1.065783483e-02f,
    1.709915049e-02f,
    1.634997098e-02f,
    5.885891271e-03f,
    -1.236340187e-02f,
    -3.139596955e-02f,
    -4.088258837e-02f,
    -3.097269177e-02f,
    3.231143876e-03f,
    5.849298645e-02f,
    1.232045874e-01f,
    1.805819436e-01f,
    2.142844376e-01f,
    2.142844376e-01f,
    1.805819436e-01f,
    1.232045874e-01f,
    5.849298645e-02f,
    3.231143876e-03f,
    -3.097269177e-02f,
    -4.088258837e-02f,
    -3.139596955e-02f,
    -1.236340187e-02f,
    5.885891271e-03f,
    1.634997098e-02f,
    1.709915049e-02f,
    1.065783483e-02f,
    1.768877929e-03f,
    -5.174534760e-03f,
    -7.872855920e-03f,
    -6.500336395e-03f,
    -2.909334729e-03f,
    6.749242491e-04f,
    2.743598200e-03f,
    2.944524962e-03f,
    1.861691008e-03f,
    4.364418813e-04f,
    -5.853117058e-04f,
    -9.250025701e-04f,
    -7.249508335e-04f,
    -3.161541367e-04f,
    1.785443369e-05f,
    1.600094188e-04f,
    1.447470168e-04f,
    6.982197704e-05f,
    1.269509306e-05f
};

static const float kVoltsPerFS  = 2.0f;     // 0 dBFS at the MOD input = 2 V peak [guess]
static const float kInGain      = 3.1277f;  // 1 + 100k/47k
static const float kAmpGain     = 200.0f;   // LM386 with 10 uF across pins 1 and 8
static const float kOutScale    = 0.336f;   // volts back to full scale: defaults ~ bypass loudness (measured)
static const float kStarveBoost = 6.24f;  // make-up for Starve's level drop (measured at defaults)
static const float kOctBoost    = 0.667f;   // Octave make-up: 100k || 150k in the mixer, x1.67 (+4.4 dB; v1.0.5 had x2 before the pre-bias)

class FactotumPlugin : public Plugin
{
public:
    FactotumPlugin()
        : Plugin(kParameterCount, 0, 0)
    {
        fParams[kGain] = 5.0f;
        fParams[kClean] = 7.0f;
        fParams[kContour] = 5.0f;
        fParams[kTone] = 5.0f;
        fParams[kLevel] = 5.0f;
        fParams[kStarve] = 0.0f;
        fParams[kOctave] = 0.0f;
        fParams[kGlow] = 0.0f;
        fParams[kBypass] = 0.0f;
        reset();
    }

protected:
    const char* getLabel()       const override { return "Factotum"; }
    const char* getDescription() const override { return "LM386 drive with a Foxx-style octave, a Klon-style clean blend and a Shredmaster-style Contour. Started from Colin Raffel's Parallel Universe schematic."; }
    const char* getMaker()       const override { return DISTRHO_PLUGIN_BRAND; }
    const char* getLicense()     const override { return "GPL-3.0-or-later"; }
    uint32_t    getVersion()     const override { return d_version(1, 0, 8); }   // = lv2:minorVersion 2, microVersion 8
    int64_t     getUniqueId()    const override { return d_cconst('n', 'F', 'c', 't'); }

    void initParameter(uint32_t index, Parameter& p) override
    {
        p.hints = kParameterIsAutomatable;
        switch (index) {
        case kGain:
            p.name = "Gain"; p.symbol = "gain";
            p.ranges.min = 0.0f; p.ranges.max = 10.0f; p.ranges.def = 5.0f;
            break;
        case kClean:
            p.name = "Clean"; p.symbol = "clean";
            p.ranges.min = 0.0f; p.ranges.max = 10.0f; p.ranges.def = 7.0f;
            break;
        case kContour:
            p.name = "Contour"; p.symbol = "contour";
            p.ranges.min = 0.0f; p.ranges.max = 10.0f; p.ranges.def = 5.0f;
            break;
        case kTone:
            p.name = "Tone"; p.symbol = "tone";
            p.ranges.min = 0.0f; p.ranges.max = 10.0f; p.ranges.def = 5.0f;
            break;
        case kLevel:
            p.name = "Level"; p.symbol = "level";
            p.ranges.min = 0.0f; p.ranges.max = 10.0f; p.ranges.def = 5.0f;
            break;
        case kStarve:
            p.hints |= kParameterIsInteger | kParameterIsBoolean;
            p.name = "Starve"; p.symbol = "starve";
            p.ranges.min = 0.0f; p.ranges.max = 1.0f; p.ranges.def = 0.0f;
            break;
        case kOctave:
            p.hints |= kParameterIsInteger | kParameterIsBoolean;
            p.name = "Octave"; p.symbol = "octave";
            p.ranges.min = 0.0f; p.ranges.max = 1.0f; p.ranges.def = 0.0f;
            break;
        case kGlow:                                           // for the face: the panel lights up as you play
            p.hints = kParameterIsOutput;
            p.name = "Glow"; p.symbol = "glow";
            p.ranges.min = 0.0f; p.ranges.max = 1.0f; p.ranges.def = 0.0f;
            break;
        case kBypass:
            p.initDesignation(kParameterDesignationBypass);   // TTL: lv2_enabled, lv2:designation lv2:enabled
            break;
        }
    }

    float getParameterValue(uint32_t i) const override { return i < kParameterCount ? fParams[i] : 0.0f; }
    void  setParameterValue(uint32_t i, float v) override { if (i < kParameterCount) fParams[i] = v; }

    void activate() override
    {
        fSr = (float)getSampleRate();
        fSmooth = onePoleCoef(1.0f / (6.2831853f * 0.010f), fSr);     // 10 ms
        fRailSm = onePoleCoef(1.0f / (6.2831853f * 0.030f), fSr);     // supply settles over ~30 ms
        fEnvAtk = onePoleCoef(1.0f / (6.2831853f * 0.002f), fSr);
        fEnvRel = onePoleCoef(1.0f / (6.2831853f * 0.060f), fSr);
        fDcR = 1.0f - 6.2831853f * 8.0f / fSr;                         // DC block at 8 Hz
        fOr = 1.0f - 6.2831853f * 10.0f / (fSr * kOS);                 // octave coupling cap, 10 Hz
        reset();
        fFirst = true;
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        const float* in = inputs[0];
        float* out = outputs[0];

        const float g01 = clampf(fParams[kGain] * 0.1f, 0.0f, 1.0f);
        const float l01 = clampf(fParams[kLevel] * 0.1f, 0.0f, 1.0f);
        const bool  starve = fParams[kStarve] > 0.5f;
        const bool  bypass = fParams[kBypass] > 0.5f;
        const float octT   = fParams[kOctave] > 0.5f ? 1.0f : 0.0f;
        // Clean: a 50kA volume pot on the clean feed. 7 = the v1.0.4 amount, 10 = about +8 dB, 0 = none
        const float c01 = clampf(fParams[kClean] * 0.1f, 0.0f, 1.0f);
        const float cmT = (std::pow(10.0f, 1.2f * c01) - 1.0f) * (1.0f / 5.9183f);

        // Gain: -46 dB .. 0 dB into the LM386 [guess: the pot's taper isn't readable from the schematic]
        const float attT    = std::pow(10.0f, (-46.0f + 46.0f * g01) * 0.05f);
        const float levT    = l01 * l01 * l01;                         // audio-taper feel
        const float railT   = starve ? 3.6f : 9.0f;                    // 9 V, or 9 V through 2k2 against the LM386's own draw
        const float starveT = starve ? 1.0f : 0.0f;
        const float wetT    = bypass ? 0.0f : 1.0f;
        const float contT   = clampf(fParams[kContour] * 0.1f, 0.0f, 1.0f);
        const float toneT   = clampf(fParams[kTone], 0.0f, 10.0f);

        if (fFirst) {                    // controls arrive with the first run()
            fFirst = false;
            fG = g01; fCm = cmT; fOct = octT; fAtt = attT; fLev = levT; fRail = railT; fStarve = starveT; fWet = wetT;
            fContK = contT; fToneK = toneT;
            updateCoefs();
            fCtl = 0;
        }

        for (uint32_t i = 0; i < frames; ++i) {
            fG      += fSmooth * (g01 - fG);
            fCm     += fSmooth * (cmT - fCm);
            fOct    += fSmooth * (octT - fOct);
            fAtt    += fSmooth * (attT - fAtt);
            fLev    += fSmooth * (levT - fLev);
            fStarve += fSmooth * (starveT - fStarve);
            fWet    += fSmooth * (wetT - fWet);
            fContK  += fSmooth * (contT - fContK);
            fToneK  += fSmooth * (toneT - fToneK);
            // the supply: settles slowly, and sags under load when starved
            fRail   += fRailSm * (railT - fRail);
            if (++fCtl >= 16) { fCtl = 0; updateCoefs(); }

            const float rail = fRail - 1.2f * fStarve * fEnv;
            // the LM386 idles at half supply; it swings closer to ground than to the top
            const float lp = std::fmax(0.25f, 0.5f * rail - 1.0f);
            const float ln = std::fmax(0.35f, 0.5f * rail - 0.5f);
            const float ilp = 1.0f / lp, iln = 1.0f / ln;
            const float d = 0.03f * fStarve;                           // starved input stage: gate, volts at the LM386 input [guess]
            const float d2 = d * d + 1e-30f;

            // input amp (TL072, rails about +-4 V) then the Gain pot
            const float vin = 4.0f * softClip(in[i] * kVoltsPerFS * kInGain * 0.25f);
            fUp[fUpPos] = fUp[fUpPos + kPhaseTaps] = vin;
            const float* h = &fUp[fUpPos];   // h[0] is the newest
            fUpPos = (fUpPos + kPhaseTaps - 1) % kPhaseTaps;

            // Klon-style clean blend, on the Gain knob (ours, not on the pedal): lots of clean
            // at Gain 0, a little at Gain 10. Mixed in the oversampled domain, so the clean
            // goes through the same filters as the fuzz and lines up with it exactly
            const float clean = (0.6f + 1.2f * (1.0f - fG)) * fCm;     // [guess: by ear] x Clean knob
            // make-ups, the fuzz only. On the pedal each footswitch/toggle's second pole puts a
            // resistor in parallel with the fuzz's mixing resistor (100k): Starve 18k, Octave 150k
            const float makeup = 1.0f + (kStarveBoost - 1.0f) * fStarve + kOctBoost * fOct;
            float pk = 0.0f;
            for (int ph = 0; ph < kOS; ++ph) {
                float xo = 0.0f;
                for (int k = 0; k < kPhaseTaps; ++k) xo += kFir[k * kOS + ph] * h[k];
                xo *= (float)kOS;
                // Octave, Foxx Tone Machine style: a transistor phase splitter feeds two
                // germanium diodes, so both halves of the wave come out positive (full-wave
                // rectified: the pitch doubles). The two phases aren't quite equal, which leaves
                // a trace of the fundamental. Coupling cap after it blocks the DC.  [approximate]
                // v1.0.6: the diodes are pre-biased (their returns sit at about 0.15 V, set by a
                // trimmer on the pedal), so only about 0.05 V of the germanium's 0.2 V turn-on is
                // left. The octave fades with the note instead of gating: sustain 1.1 s -> 2.2 s.
                const float up = 0.99f * xo, dn = -0.93f * xo;
                const float wu = up - 0.05f, wd = dn - 0.05f;          // Ge diode, pre-biased
                const float r = (wu > 0.0f ? wu * wu / (wu + 0.04f) : 0.0f)
                              + (wd > 0.0f ? wd * wd / (wd + 0.04f) : 0.0f);
                fOh = r - fOx + fOr * fOh + 1e-18f;
                fOx = r;
                const float xf = xo + fOct * (fOh - xo);               // the Octave footswitch picks the feed
                float v = fAtt * xf;
                v = v * v * v / (v * v + d2);                          // starve: small signals die (gating)
                const float u = kAmpGain * v;
                const float y = u > 0.0f ? lp * softClip(u * ilp) : ln * softClip(u * iln);
                pk = std::fmax(pk, std::fabs(y));
                const float sum = makeup * y + clean * xo;
                fDn[fDnPos] = fDn[fDnPos + kTaps] = sum;
                fDnPos = (fDnPos + kTaps - 1) % kTaps;
            }
            // decimate: newest sample is at fDnPos + 1
            const float* q = &fDn[fDnPos + 1];
            float yd = 0.0f;
            for (int j = 0; j < kTaps; ++j) yd += kFir[j] * q[j];

            // starve sag follows the LM386's output
            fEnv += (pk > fEnv ? fEnvAtk : fEnvRel) * (pk - fEnv);

            // DC block (the clipping is lopsided, so the bias moves)
            fDc = yd - fDcX + fDcR * fDc + 1e-18f;
            fDcX = yd;

            // Contour: third-order IIR, transposed direct form II, in double (low poles)
            const double cx = (double)fDc;
            const double cy = fB[0] * cx + fZ[0];
            fZ[0] = fB[1] * cx - fA[1] * cy + fZ[1];
            fZ[1] = fB[2] * cx - fA[2] * cy + fZ[2];
            fZ[2] = fB[3] * cx - fA[3] * cy;

            // Tone, Level, Starve make-up
            fTone += fToneC * ((float)cy - fTone);
            const float wet = fTone * fLev * kOutScale;

            out[i] = fWet * wet + (1.0f - fWet) * in[i];
        }

        // Glow out (for the face's light panel): a peak follower on the input (instant rise,
        // ~180 ms fall, so MOD's few-times-a-second updates catch each pluck), mapped
        // -48 dBFS -> 0 .. -12 dBFS -> 1, then ^1.5 (as Trance Atlantic's Input Level).
        // Scaled by the bypass fade, so the panel goes dark when the pedal is off.
        {
            const float rel = std::exp(-1.0f / (0.180f * fSr));
            for (uint32_t i = 0; i < frames; ++i) {
                const float a = std::fabs(in[i]);
                fLevel = a > fLevel ? a : fLevel * rel;
            }
            fLevel += 1e-18f;
            const float dB = 20.0f * std::log10(fLevel + 1e-9f);
            const float lv = clampf((dB + 48.0f) / 36.0f, 0.0f, 1.0f);
            fParams[kGlow] = lv * std::sqrt(lv) * fWet;
        }
    }

private:
    void reset()
    {
        std::memset(fUp, 0, sizeof(fUp));
        std::memset(fDn, 0, sizeof(fDn));
        fUpPos = 0; fDnPos = 0;
        fDc = fDcX = fTone = fEnv = 0.0f;
        fOh = fOx = 0.0f;
        fLevel = 0.0f;
        fZ[0] = fZ[1] = fZ[2] = 0.0;
    }

    void updateCoefs()
    {
        // Contour (Shredmaster network), s-domain, highest power first, then bilinear
        const double R1 = 100.0, R2 = 33000.0, R3 = 33000.0, R4 = 100000.0;
        const double C1 = 1e-9, C2 = 1e-7, C3 = 4.7e-8;
        // cubic taper (ours): a linear pot leaves the whole scoop in the last tenth of the turn;
        // this spreads it, about -6 dB at noon, -24 dB at full (700 Hz vs 100 Hz)
        const double ik = 1.0 - (double)fContK;
        const double m  = 0.001 + 0.998 * (1.0 - ik * ik * ik);
        const double ra = m * R4, rb = (1.0 - m) * R4;
        const double c12 = C1 * C2, c13 = C1 * C3, c23 = C2 * C3, c123 = c12 * C3;
        const double b3 = rb * c123 * R3 * ra + rb * c123 * R2 * ra + c123 * R2 * R3 * ra;
        const double b2 = rb * c23 * ra + rb * c13 * R3 + c12 * R3 * ra + rb * c13 * R2 + c12 * R2 * ra + c13 * R2 * R3;
        const double b1 = rb * C3 + C2 * ra + C1 * R3 + C1 * R2;
        const double b0 = 1.0;   // as in dm-Shredmaster: the contour passes DC (its derived network's own numerator has no constant term)
        const double a3 = b3 + rb * c123 * R1 * R3 + c123 * R1 * R3 * ra + rb * c123 * R1 * R2 + c123 * R1 * R2 * ra + c123 * R1 * R2 * R3;
        const double a2 = b2 + c23 * R2 * ra + rb * c23 * R1 + c23 * R1 * ra + c12 * R1 * R3 + c13 * R1 * R3 + c12 * R1 * R2 + c13 * R1 * R2 + c23 * R1 * R2;
        const double a1 = b1 + C3 * R2 + C2 * R1 + C3 * R1;
        const double a0 = 1.0;
        const double K = 2.0 * (double)fSr, K2 = K * K, K3 = K2 * K;
        const double n0 =  b3 * K3 + b2 * K2 + b1 * K + b0;
        const double n1 = -3.0 * b3 * K3 - b2 * K2 + b1 * K + 3.0 * b0;
        const double n2 =  3.0 * b3 * K3 - b2 * K2 - b1 * K + 3.0 * b0;
        const double n3 = -b3 * K3 + b2 * K2 - b1 * K + b0;
        const double d0 =  a3 * K3 + a2 * K2 + a1 * K + a0;
        const double d1 = -3.0 * a3 * K3 - a2 * K2 + a1 * K + 3.0 * a0;
        const double d2 =  3.0 * a3 * K3 - a2 * K2 - a1 * K + 3.0 * a0;
        const double d3 = -a3 * K3 + a2 * K2 - a1 * K + a0;
        const double inv = 1.0 / d0;
        fB[0] = n0 * inv; fB[1] = n1 * inv; fB[2] = n2 * inv; fB[3] = n3 * inv;
        fA[1] = d1 * inv; fA[2] = d2 * inv; fA[3] = d3 * inv;
        // Tone: treble cut, 2 kHz .. 8 kHz (v1.0.1's 500 Hz .. 2 kHz half was muffled with any Contour)
        fToneC = onePoleCoef(2000.0f * std::pow(4.0f, fToneK * 0.1f), fSr);
    }

    float fParams[kParameterCount];
    float fUp[2 * kPhaseTaps];
    float fDn[2 * kTaps];
    int   fUpPos = 0, fDnPos = 0, fCtl = 0;
    float fSr = 48000.0f;
    float fSmooth = 0.0f, fRailSm = 0.0f, fEnvAtk = 0.0f, fEnvRel = 0.0f, fDcR = 0.999f;
    float fG = 0.5f, fAtt = 0.07f, fLev = 0.125f, fRail = 9.0f, fStarve = 0.0f, fWet = 1.0f;
    float fContK = 0.5f, fToneK = 5.0f, fToneC = 0.1f;
    double fB[4] = { 1.0, 0.0, 0.0, 0.0 }, fA[4] = { 1.0, 0.0, 0.0, 0.0 }, fZ[3] = { 0.0, 0.0, 0.0 };
    float fDc = 0.0f, fDcX = 0.0f, fTone = 0.0f, fEnv = 0.0f;
    float fLevel = 0.0f;
    float fCm = 1.0f, fOct = 0.0f, fOh = 0.0f, fOx = 0.0f, fOr = 0.9997f;
    bool  fFirst = true;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FactotumPlugin)
};

Plugin* createPlugin() { return new FactotumPlugin(); }

END_NAMESPACE_DISTRHO
