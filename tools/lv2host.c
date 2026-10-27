/*
 * Minimal offline LV2 host for testing DPF plugins (lv2apply can't: DPF
 * requires the options + urid:map features, which lv2apply doesn't supply).
 *
 * Build:  gcc -O2 -I<DPF>/distrho/src -I<DPF>/distrho/src/lv2 lv2host.c -o lv2host -ldl
 *         (32-bit ARM: arm-linux-gnueabihf-gcc ... then run with
 *          qemu-arm -L /usr/arm-linux-gnueabihf ./lv2host ...)
 *
 * Usage:  lv2host plugin_dsp.so in.raw out.raw samplerate n_in n_out ctl0 ctl1 ... [@sample:port=value ...]
 *   in.raw  : float32 interleaved, n_in channels
 *   out.raw : float32 interleaved, n_out channels
 *   ctlN    : values for control ports in index order (ports n_in+n_out ...)
 *   @S:P=V  : at sample S set control port index P to V; repeatable (up to 64),
 *             applied at the start of the 256-frame block containing S
 *   PB_TRACE_PORT=<index> PB_TRACE_FILE=<path> (environment): after every block, append
 *             that control port's value (float32) to the file: reads output ports
 *
 * Event ports (environment; all optional, off by default). DPF puts its event ports after
 * the audio ports, so control ports then start that many indices later:
 *   PB_ATOM_IN=1   an atom input (time:Position, as DPF_PLUGIN_WANT_TIMEPOS adds)
 *   PB_ATOM_OUT=1  an atom output (MIDI out, as DISTRHO_PLUGIN_WANT_MIDI_OUTPUT adds)
 *   Transport, sent as time:Position objects to the atom input:
 *     PB_BPM (120), PB_BPB beats per bar (4),
 *     PB_ROLL=<start>[:<stop>] samples where the transport rolls (bar 0, beat 0 at start;
 *             applied at the block containing them); unset = stopped throughout,
 *     PB_POS_EVERY=1  send the position every block while rolling (default: only when it
 *             changes, so DPF extrapolates in between, as it must when a host sends sparsely)
 *   PB_MIDI_FILE=<path>  write every MIDI event from the atom output as int32 quads:
 *             absolute frame, status, data1, data2
 */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lv2/lv2.h"
#include "lv2/atom.h"
#include "lv2/options.h"
#include "lv2/urid.h"
#include "lv2/buf-size.h"
#include "lv2/parameters.h"
#include "lv2/atom-forge.h"
#include "lv2/atom-util.h"
#include "lv2/midi.h"
#include "lv2/time.h"
static const char* uris[128]; static int nuris = 0;
static LV2_URID map(LV2_URID_Map_Handle h, const char* u) {
    (void)h; for (int i = 0; i < nuris; i++) if (!strcmp(uris[i], u)) return i + 1;
    uris[nuris] = strdup(u); return ++nuris; }
int main(int c, char** v) {
    if (c < 7) { fprintf(stderr, "usage: see header\n"); return 1; }
    void* lib = dlopen(v[1], RTLD_NOW); if (!lib) { puts(dlerror()); return 1; }
    const LV2_Descriptor* d = ((LV2_Descriptor_Function)dlsym(lib, "lv2_descriptor"))(0);
    double sr = atof(v[4]); int ni = atoi(v[5]), no = atoi(v[6]);
    FILE* f = fopen(v[2], "rb"); fseek(f, 0, 2); long n = ftell(f) / 4 / ni; rewind(f);
    float* in = malloc(n * ni * 4); if (fread(in, 4, n * ni, f) != (size_t)(n * ni)) return 1; fclose(f);
    float** ib = malloc(ni * sizeof(float*)); float** ob = malloc(no * sizeof(float*));
    int bs = 256; for (int i = 0; i < ni; i++) ib[i] = malloc(bs * 4); for (int i = 0; i < no; i++) ob[i] = malloc(bs * 4);
    float* out = malloc(n * no * 4); float srf = sr; int32_t bsi = bs;
    LV2_URID_Map m = { 0, map }; LV2_URID fl = map(0, LV2_ATOM__Float), it = map(0, LV2_ATOM__Int);
    LV2_Options_Option opts[] = {
        { LV2_OPTIONS_INSTANCE, 0, map(0, LV2_PARAMETERS__sampleRate), 4, fl, &srf },
        { LV2_OPTIONS_INSTANCE, 0, map(0, LV2_BUF_SIZE__maxBlockLength), 4, it, &bsi },
        { LV2_OPTIONS_INSTANCE, 0, map(0, LV2_BUF_SIZE__nominalBlockLength), 4, it, &bsi },
        { 0, 0, 0, 0, 0, 0 } };
    LV2_Feature fm = { LV2_URID__map, &m }, fo = { LV2_OPTIONS__options, opts };
    const LV2_Feature* feats[] = { &fm, &fo, 0 };
    LV2_Handle h = d->instantiate(d, sr, "", feats); if (!h) { puts("instantiate failed"); return 1; }
    const char* e;
    int nai = (e = getenv("PB_ATOM_IN")) ? atoi(e) : 0, nao = (e = getenv("PB_ATOM_OUT")) ? atoi(e) : 0;
    double bpm = (e = getenv("PB_BPM")) ? atof(e) : 120.0, bpb = (e = getenv("PB_BPB")) ? atof(e) : 4.0;
    long rollA = -1, rollB = -1; if ((e = getenv("PB_ROLL"))) { if (sscanf(e, "%ld:%ld", &rollA, &rollB) < 2) rollB = -1; }
    int every = (e = getenv("PB_POS_EVERY")) ? atoi(e) : 0;
    const char* mf = getenv("PB_MIDI_FILE"); FILE* mfp = mf ? fopen(mf, "wb") : 0;
    static uint64_t abin[1024], about[1024];               /* 8 KB each, 8-byte aligned */
    LV2_Atom_Forge forge; lv2_atom_forge_init(&forge, &m);
    LV2_URID uPos = map(0, LV2_TIME__Position), uFrame = map(0, LV2_TIME__frame), uSpeed = map(0, LV2_TIME__speed),
             uBpm = map(0, LV2_TIME__beatsPerMinute), uBar = map(0, LV2_TIME__bar), uBarBeat = map(0, LV2_TIME__barBeat),
             uBpb = map(0, LV2_TIME__beatsPerBar), uUnit = map(0, LV2_TIME__beatUnit), uMidi = map(0, LV2_MIDI__MidiEvent),
             uChunk = map(0, LV2_ATOM__Chunk), uSeq = map(0, LV2_ATOM__Sequence);
    if (nai > 0) d->connect_port(h, ni + no, abin);
    if (nao > 0) d->connect_port(h, ni + no + nai, about);
    int lastRoll = -1;
    float ctl[64]; int nc = 0;
    long evS[64]; int evP[64]; float evV[64]; int ne = 0;
    for (int a = 7; a < c; a++) {
        if (v[a][0] == '@') { if (ne < 64 && sscanf(v[a], "@%ld:%d=%f", &evS[ne], &evP[ne], &evV[ne]) == 3) ne++; }
        else { ctl[nc] = atof(v[a]); d->connect_port(h, ni + no + nai + nao + nc, &ctl[nc]); nc++; } }
    for (int i = 0; i < ni; i++) d->connect_port(h, i, ib[i]);
    for (int i = 0; i < no; i++) d->connect_port(h, ni + i, ob[i]);
    const char* trf = getenv("PB_TRACE_FILE"); const char* trp = getenv("PB_TRACE_PORT");
    FILE* tf = (trf && trp) ? fopen(trf, "wb") : 0; int ti = trp ? atoi(trp) - ni - no - nai - nao : -1;
    d->activate(h);
    for (long p = 0; p < n; p += bs) {
        long k = (n - p < bs) ? n - p : bs;
        for (int e = 0; e < ne; e++)
            if (evS[e] >= 0 && p + k > evS[e]) { ctl[evP[e] - ni - no - nai - nao] = evV[e]; evS[e] = -1; }
        for (long s = 0; s < k; s++) for (int i = 0; i < ni; i++) ib[i][s] = in[(p + s) * ni + i];
        if (nai > 0) {   /* transport: a time:Position on change (or every block) */
            int roll = rollA >= 0 && p + k > rollA && (rollB < 0 || p + k <= rollB);
            lv2_atom_forge_set_buffer(&forge, (uint8_t*)abin, sizeof(abin));
            LV2_Atom_Forge_Frame sq; lv2_atom_forge_sequence_head(&forge, &sq, 0);
            if (roll != lastRoll || (roll && every)) {
                long el = roll ? p - (rollA / bs) * bs : 0; if (el < 0) el = 0;
                double beats = el * bpm / (60.0 * sr); long bar = (long)(beats / bpb);
                LV2_Atom_Forge_Frame ob; lv2_atom_forge_frame_time(&forge, 0);
                lv2_atom_forge_object(&forge, &ob, 0, uPos);
                lv2_atom_forge_key(&forge, uFrame);   lv2_atom_forge_long(&forge, el);
                lv2_atom_forge_key(&forge, uSpeed);   lv2_atom_forge_float(&forge, roll ? 1.0f : 0.0f);
                lv2_atom_forge_key(&forge, uBpm);     lv2_atom_forge_float(&forge, (float)bpm);
                lv2_atom_forge_key(&forge, uBar);     lv2_atom_forge_long(&forge, bar);
                lv2_atom_forge_key(&forge, uBarBeat); lv2_atom_forge_float(&forge, (float)(beats - bar * bpb));
                lv2_atom_forge_key(&forge, uBpb);     lv2_atom_forge_float(&forge, (float)bpb);
                lv2_atom_forge_key(&forge, uUnit);    lv2_atom_forge_int(&forge, 4);
                lv2_atom_forge_pop(&forge, &ob);
            }
            lv2_atom_forge_pop(&forge, &sq);
            lastRoll = roll;
        }
        if (nao > 0) { LV2_Atom* oa = (LV2_Atom*)about; oa->type = uChunk; oa->size = sizeof(about) - sizeof(LV2_Atom); }
        d->run(h, k);
        const LV2_Atom* oat = (const LV2_Atom*)(const void*)about;
        if (nao > 0 && mfp && oat->type == uSeq) {   /* DPF leaves the buffer alone in a block with no events */
            LV2_Atom_Sequence* sq = (LV2_Atom_Sequence*)about;
            LV2_ATOM_SEQUENCE_FOREACH(sq, ev) {
                if (ev->body.type != uMidi) continue;
                const uint8_t* md = (const uint8_t*)LV2_ATOM_BODY(&ev->body);
                int32_t q[4] = { (int32_t)(p + ev->time.frames), md[0], ev->body.size > 1 ? md[1] : 0, ev->body.size > 2 ? md[2] : 0 };
                fwrite(q, 4, 4, mfp);
            }
        }
        if (tf && ti >= 0 && ti < nc) fwrite(&ctl[ti], 4, 1, tf);
        for (long s = 0; s < k; s++) for (int i = 0; i < no; i++) out[(p + s) * no + i] = ob[i][s]; }
    if (tf) fclose(tf);
    if (mfp) fclose(mfp);
    f = fopen(v[3], "wb"); fwrite(out, 4, n * no, f); fclose(f);
    d->deactivate(h); d->cleanup(h); return 0; }
