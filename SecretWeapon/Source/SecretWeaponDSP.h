#pragma once
// SECRET WEAPON DSP. It listens to a melody, finds the notes, and plays extra notes that belong to the
// chosen scale. The extra notes are copies of the player's own sound, replayed at a new pitch
// (like a sampler), so they always have the character of the original instrument.
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <atomic>
#include <limits>

namespace secretweapon
{
constexpr double kPi = 3.14159265358979323846;

struct Scale { const char* name; int n; int semis[7]; };
inline const Scale& getScale (int i)
{
    static const Scale s[10] = {
        { "MAJOR", 7, { 0, 2, 4, 5, 7, 9, 11 } },       { "MINOR", 7, { 0, 2, 3, 5, 7, 8, 10 } },
        { "DORIAN", 7, { 0, 2, 3, 5, 7, 9, 10 } },      { "PHRYGIAN", 7, { 0, 1, 3, 5, 7, 8, 10 } },
        { "MIXOLYDIAN", 7, { 0, 2, 4, 5, 7, 9, 10 } },  { "HARM. MINOR", 7, { 0, 2, 3, 5, 7, 8, 11 } },
        { "HIJAZ", 7, { 0, 1, 4, 5, 7, 8, 10 } },       { "PENTA MAJ", 5, { 0, 2, 4, 7, 9 } },
        { "PENTA MIN", 5, { 0, 3, 5, 7, 10 } },         { "BLUES", 6, { 0, 3, 5, 6, 7, 10 } } };
    return s[std::max (0, std::min (9, i))];
}
inline const char* keyName (int k) { static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return n[((k % 12) + 12) % 12]; }

enum Source { Guitar = 0, Keys, Vox, Bass, Synth, Loop };
enum Style : unsigned { StGhost = 1, StSparkle = 2, StSteps = 4, StChord = 8, StAnswer = 16, StTipsy = 32 };

struct Params
{
    bool on = true;
    double mix = 1.0;          // how loud the secret notes are (the dry sound is always full)
    int key = 9, scale = 1, source = 0;
    double density = 0.5, levelDb = -7.0, rangeOct = 1.0, lengthBeats = 0.25;
    unsigned styles = StGhost | StSparkle | StTipsy;
    double bpm = 120.0;
    double ppq = -1.0;         // host position in quarter notes, negative when unknown (then timing is free)
};

struct Rng
{
    uint32_t s = 88172645u;
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xFFFFFF) / 16777216.0f; }
    int pick (int n) { return std::min (n - 1, (int) (next() * n)); }
};

// ring of events for the editor (melody notes and secret notes)
struct VizEvent { double start = 0, end = 0; float midi = 0; int kind = 0; float level = 0; };
struct VizLog
{
    static constexpr int kSize = 256;
    VizEvent ev[kSize];
    std::atomic<int> writeIndex { 0 };
    std::atomic<double> nowSample { 0.0 };
    std::atomic<double> sampleRate { 48000.0 };
};

/*  How it works (and what can go wrong, and what stops it)
    1. LISTEN: the engine keeps the last 12 s of the input. Every 10 ms it measures the level. A note starts when the level
       jumps 5 dB over its own recent average, so it works at any loudness (no fixed threshold except a quiet floor).
       If the sound never has a clear attack (pads, held chords, a loop) a "virtual note" is made every couple of beats,
       so something is always happening while there is sound.
    2. UNDERSTAND: 100 ms after a note starts it looks at the sound two ways. YIN finds one clear pitch (single notes).
       A 12-note chroma (which notes are in the sound, chords included) is always measured too.
       - One clear note: extra notes are exact scale steps from it.
       - Anything else: the engine only uses a shift if the chroma says the shifted sound still lands inside the scale
         (octaves are always safe). It never guesses a pitch it cannot hear.
       - If the playing itself barely fits the chosen scale it only plays octaves and tells you (press LISTEN).
    3. PLAY: the extra notes are replayed pieces of the player's own sound at a new speed (like a sampler), on the beat
       grid of the host when it is known. Level is relative to the original, so LEVEL -7 dB really is 7 dB under it.
    4. SHOW: the status counters tell the user what it hears, so it never looks like "reading, then nothing". */

class Engine
{
public:
    static constexpr double kLatencyMs = 180.0;

    void prepare (double sampleRate)
    {
        sr = sampleRate; viz.sampleRate.store (sampleRate);
        int n = 1; while (n < (int) (sr * 12.0)) n <<= 1;
        for (auto& h : hist) h.assign ((size_t) n, 0.0f);
        hmask = n - 1;
        latency = (int) std::lround (sr * kLatencyMs / 1000.0);
        for (auto& d : dry) d.assign ((size_t) latency, 0.0f);
        dryPos = 0;
        decF = std::max (1, (int) std::lround (sr / 12000.0));
        decSr = sr / decF;
        dec.assign (8192, 0.0f); decW = 0;
        hopDec = 128; winDec = 320; tauMax = (int) (decSr / 40.0);
        if (tauMax > 400) tauMax = 400;
        yd.assign ((size_t) tauMax + 2, 0.0f);
        buildGoertzel();
        reset();
    }
    int latencySamples() const { return latency; }

    void reset()
    {
        clk = 0; for (auto& h : hist) std::fill (h.begin(), h.end(), 0.0f);
        for (auto& d : dry) std::fill (d.begin(), d.end(), 0.0f);
        for (auto& v : voices) v.active = false;
        nTrig = 0; nextFire = 1e18; decAcc = 0; decCnt = 0; decSince = 0; decW = 0;
        slowDb = -120; fastDb = -120; hopCount = 0; lastOnsetHop = -100; pending = false; active = false;
        nNotes = 0; belowCount = 0; chgCount = 0; onSmooth = 1.0f; lastEventClk = 0; heardMode.store (0); coverPct.store (0); addedCount.store (0); heardCount.store (0);
    }

    void setParams (const Params& p)
    {
        P = p;
        if (p.ppq >= 0.0) { ppqBlock = p.ppq; clkBlock = (double) clk; ppqKnown = true; } else ppqKnown = false;
    }

    // ---- listening for key and scale
    void startListen (double seconds) { listenLeft = seconds; listenTotal = seconds; std::fill (chroma, chroma + 12, 0.0); listenState.store (1); listenSinceFrame = 0; }
    std::atomic<int> listenState { 0 };    // 0 idle, 1 listening, 2 done
    std::atomic<float> listenProgress { 0.0f };
    std::atomic<int> heardKey { 9 }, heardScale { 1 }, heardPct { 0 };

    // ---- what it hears right now (for the screen)
    std::atomic<int> heardMode { 0 };      // 0 nothing, 1 single notes, 2 chords or loop
    std::atomic<int> coverPct { 0 };       // how much of the last sound is inside the chosen scale
    std::atomic<int> addedCount { 0 };     // secret notes played so far
    std::atomic<int> heardCount { 0 };     // notes/hits heard so far
    std::atomic<float> inLevelDb { -120.0f };

    VizLog viz;
    std::atomic<int> triggerCount { 0 };
    // test hook: every secret note that gets scheduled
    struct Fired { double at; float targetMidi; float ratio; int style; bool reverse; float srcMidi; double srcStart; };
    std::vector<Fired>* firedLog = nullptr;

    void process (float* L, float* R, int n)
    {
        const double lvl = std::pow (10.0, P.levelDb / 20.0);
        const double gain = lvl * P.mix;
        const float onTarget = P.on ? 1.0f : 0.0f;
        const float onA = (float) (1.0 - std::exp (-1.0 / (0.02 * sr)));
        for (int i = 0; i < n; ++i)
        {
            const float inL = L[i], inR = R[i];
            const size_t wi = (size_t) (clk & (int64_t) hmask);
            hist[0][wi] = inL; hist[1][wi] = inR;

            float dL = inL, dR = inR;
            if (latency > 0) { dL = dry[0][(size_t) dryPos]; dR = dry[1][(size_t) dryPos]; dry[0][(size_t) dryPos] = inL; dry[1][(size_t) dryPos] = inR; if (++dryPos >= latency) dryPos = 0; }

            decAcc += 0.5f * (inL + inR);
            if (++decCnt >= decF)
            {
                dec[(size_t) (decW & 8191)] = decAcc / (float) decF; decAcc = 0; decCnt = 0; ++decW;
                if (++decSince >= hopDec) { decSince = 0; analyse(); }
            }

            if ((double) clk >= nextFire) fireDue();

            float wL = 0, wR = 0;
            for (auto& v : voices) if (v.active) renderVoice (v, wL, wR);
            onSmooth += onA * (onTarget - onSmooth);
            const float g = (float) gain * onSmooth;
            wL = std::tanh (wL * g); wR = std::tanh (wR * g);
            L[i] = dL + wL; R[i] = dR + wR;
            ++clk;
        }
        viz.nowSample.store ((double) clk);
    }

private:
    // ============================================================ types
    struct Note
    {
        double start = 0, end = -1; float midi = NAN; float level = 0; int vizIdx = -1;
        float chroma[12] = {}; float cover = 1.0f; bool virt = false;
    };
    struct Voice
    {
        bool active = false, rev = false, loopOK = false; double pos = 0, pos2 = 0, rate = 1, rateTarget = 1, glide = 0, srcLow = 0;
        int64_t age = 0, len = 0, att = 0, rel = 0; int xf = -1; int noteId = 0; double noteStart = 0;
        float gain = 1, gl = 1, gr = 1, lp[2] = { 0, 0 }; float lpA = 1.0f;
    };
    struct Trig { double at; double srcStart; int noteId; float srcMidi; float targetMidi; float ratio; double lenSamples; float gain; bool rev; int style; float pan; double glideSec; bool loopOK; };

    double sr = 48000, decSr = 12000; int decF = 4, latency = 5760;
    Params P; Rng rng;
    std::vector<float> hist[2], dry[2], dec, yd; int hmask = 0, dryPos = 0;
    int64_t clk = 0; float decAcc = 0; int decCnt = 0, decSince = 0; int64_t decW = 0;
    int hopDec = 128, winDec = 320, tauMax = 300;
    Voice voices[24];
    Trig trigs[64]; int nTrig = 0; double nextFire = 1e18;
    float onSmooth = 1.0f;
    double ppqBlock = 0, clkBlock = 0; bool ppqKnown = false;

    // note tracking
    float slowDb = -120, fastDb = -120; bool pending = false, active = false; int hopCount = 0, lastOnsetHop = -100;
    int belowCount = 0, chgCount = 0; double pendStart = 0; int pendHops = 0; float pendMidi[10]; int pendVoiced = 0; float pendPeak = 0;
    float curMidi = NAN, chgMidi = NAN;
    Note notes[64]; int nNotes = 0;
    double lastPhraseEnd = 0; int64_t lastEventClk = 0;
    float avgLevel = 0.01f;

    // key finder + chroma
    double listenLeft = 0, listenTotal = 0; double chroma[12] = {}; int listenSinceFrame = 0;
    std::vector<float> gCos; std::vector<double> gFreqMidi; std::vector<float> gWin, gWinS;
    static constexpr int kG = 4096, kGS = 2048, kGLo = 36, kGHi = 84;

    void buildGoertzel()
    {
        gCos.clear(); gFreqMidi.clear();
        for (int m = kGLo; m <= kGHi; ++m) { const double f = 440.0 * std::pow (2.0, (m - 69) / 12.0); gCos.push_back ((float) (2.0 * std::cos (2.0 * kPi * f / decSr))); gFreqMidi.push_back (m); }
        gWin.resize (kG); for (int i = 0; i < kG; ++i) gWin[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * kPi * i / (kG - 1)));
        gWinS.resize (kGS); for (int i = 0; i < kGS; ++i) gWinS[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * kPi * i / (kGS - 1)));
    }

    float decAt (int64_t k) const { return dec[(size_t) (k & 8191)]; }

    // ============================================================ scale helpers
    bool inScalePc (int pc) const
    {
        const Scale& sc = getScale (P.scale); const int d = (((pc - P.key) % 12) + 12) % 12;
        for (int i = 0; i < sc.n; ++i) if (sc.semis[i] == d) return true;
        return false;
    }
    int scaleTones (int* out) const
    {
        const Scale& sc = getScale (P.scale); int n = 0;
        for (int oct = -1; oct <= 10; ++oct)
            for (int d = 0; d < sc.n; ++d) { const int m = P.key + 12 * oct + sc.semis[d]; if (m >= 0 && m < 128) out[n++] = m; }
        std::sort (out, out + n); return n;
    }
    int nearestIdx (const int* t, int n, float midi) const
    {
        int best = 0; float bd = 1e9f;
        for (int i = 0; i < n; ++i) { const float d = std::fabs ((float) t[i] - midi); if (d < bd) { bd = d; best = i; } }
        return best;
    }
    float scaleStep (float midi, int steps, float rangeSemis) const
    {
        int t[200]; const int n = scaleTones (t); const int i = nearestIdx (t, n, midi);
        int j = std::max (0, std::min (n - 1, i + steps)); float target = (float) t[j];
        while (target > midi + rangeSemis + 0.5f) target -= 12.0f;
        while (target < midi - rangeSemis - 0.5f) target += 12.0f;
        return target;
    }
    float scaleNearest (float midi) const { int t[200]; const int n = scaleTones (t); return (float) t[nearestIdx (t, n, midi)]; }
    float sourceRange() const
    {
        double r = P.rangeOct;
        if (P.source == Vox) r = std::min (r, 0.5);
        if (P.source == Synth) r *= 1.5;
        if (P.source == Bass) r = std::min (r, 1.0);
        return (float) (12.0 * r);
    }

    // how much of a chroma lands inside the scale after moving it by `semis`
    float coverAfter (const float* c, int semis) const
    {
        double in = 0, tot = 0;
        for (int i = 0; i < 12; ++i) { tot += c[i]; if (inScalePc ((i + semis % 12 + 12) % 12)) in += c[i]; }
        return tot > 0 ? (float) (in / tot) : 1.0f;
    }
    // a shift that is safe for this sound. `list` is in order of preference; returns 99 when none passes.
    int safeShift (const Note& n, const int* list, int count)
    {
        const float c0 = n.cover;
        int ok[32]; int nok = 0;
        for (int i = 0; i < count && nok < 32; ++i)
        {
            const int k = list[i];
            if (k % 12 == 0) { ok[nok++] = k; continue; }             // octaves keep the same notes
            if (c0 < 0.8f || n.virt || P.source == Loop) continue;                         // not in the scale, or a guess made from a mix (a chord may change under it): octaves only
            if (coverAfter (n.chroma, k) >= std::min (c0, 0.95f) - 0.02f) ok[nok++] = k;
        }
        if (nok == 0) return 99;
        return ok[rng.pick (nok)];
    }

    // ============================================================ grid
    // the next beat-grid time at or after `t` (in samples); `t` itself when the host position is unknown
    double snapToGrid (double t) const
    {
        if (! ppqKnown || P.bpm < 20) return t;
        const double g = std::max (0.0625, P.lengthBeats);
        const double spp = 60.0 * sr / P.bpm;                           // samples per quarter note
        const double ppqAtT = ppqBlock + (t - clkBlock) / spp;
        const double q = std::ceil (ppqAtT / g - 1.0e-6) * g;
        return t + (q - ppqAtT) * spp;
    }

    // ============================================================ analysis
    float yin (float& conf)
    {
        const int W = winDec, T = tauMax;
        const int64_t base = decW - W - T - 1;
        if (base < 0) { conf = 0; return NAN; }
        for (int tau = 1; tau <= T; ++tau)
        {
            double s = 0;
            for (int j = 0; j < W; j += 1) { const float d = decAt (base + j) - decAt (base + j + tau); s += (double) d * d; }
            yd[(size_t) tau] = (float) s;
        }
        double run = 0; int tauEst = -1;
        static thread_local std::vector<float> cm; cm.assign ((size_t) T + 2, 1.0f);
        for (int tau = 1; tau <= T; ++tau) { run += yd[(size_t) tau]; cm[(size_t) tau] = run > 0 ? (float) (yd[(size_t) tau] * tau / run) : 1.0f; }
        for (int tau = 8; tau < T; ++tau)
            if (cm[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < T && cm[(size_t) tau + 1] < cm[(size_t) tau]) ++tau;
                tauEst = tau; break;
            }
        if (tauEst < 0) { conf = 0; return NAN; }
        conf = 1.0f - cm[(size_t) tauEst];
        const double a = cm[(size_t) tauEst - 1], b = cm[(size_t) tauEst], c = cm[(size_t) std::min (tauEst + 1, T)];
        const double den = a - 2 * b + c; const double off = std::fabs (den) > 1e-9 ? 0.5 * (a - c) / den : 0.0;
        const double f0 = decSr / (tauEst + off);
        return (float) (69.0 + 12.0 * std::log2 (f0 / 440.0));
    }
    static bool near (float a, float b, float tol) { return ! std::isnan (a) && ! std::isnan (b) && std::fabs (a - b) < tol; }

    // which notes are in the sound right now (12 numbers). window = the last kGS decimated samples (about 170 ms)
    void measureChroma (float* out)
    {
        double c[12] = {}; const int64_t base = decW - kGS;
        if (base < 0) { for (int i = 0; i < 12; ++i) out[i] = 0; return; }
        const int lo = P.source == Loop ? 55 : kGLo;      // loops: leave out the kick and the bass, they smear the chroma
        for (size_t k = 0; k < gCos.size(); ++k)
        {
            if (gFreqMidi[k] < lo) continue;
            float s1 = 0, s2 = 0; const float cc = gCos[k];
            for (int i = 0; i < kGS; ++i) { const float x = decAt (base + i) * gWinS[(size_t) i] + cc * s1 - s2; s2 = s1; s1 = x; }
            const double mag = (double) s1 * s1 + (double) s2 * s2 - (double) cc * s1 * s2;
            c[(int) gFreqMidi[k] % 12] += std::sqrt (std::max (0.0, mag));
        }
        double mean = 0; for (double v : c) mean += v; mean /= 12.0;
        double tot = 0; for (int i = 0; i < 12; ++i) { c[i] = std::max (0.0, c[i] - 0.6 * mean); tot += c[i]; }
        for (int i = 0; i < 12; ++i) out[i] = tot > 0 ? (float) (c[i] / tot) : 0.0f;
    }

    void analyse()
    {
        ++hopCount;
        double e = 0; for (int j = 1; j <= 256; ++j) { const float v = decAt (decW - j); e += (double) v * v; }
        const float rms = (float) std::sqrt (e / 256.0);
        fastDb = 20.0f * std::log10 (rms + 1.0e-9f);
        inLevelDb.store (fastDb);
        const float floorDb = -58.0f;                  // quiet floor only; everything above is judged against the sound's own level
        const bool loud = fastDb > floorDb;
        float conf = 0; const float m = loud ? yin (conf) : NAN;
        const bool voiced = ! std::isnan (m) && conf > 0.8f;
        const double hopNow = (double) clk;
        const double hopLen = hopDec * decF;

        if (listenState.load() == 1)
        {
            listenLeft -= hopLen / sr; listenProgress.store ((float) (1.0 - listenLeft / listenTotal));
            listenSinceFrame += (int) hopLen;
            if (listenSinceFrame >= 2048 * decF && decW > kG && loud) { listenSinceFrame = 0; addChroma(); }
            if (listenLeft <= 0) finishListen();
        }

        // a note starts when the level jumps 5 dB over its own recent average
        const bool attack = loud && fastDb > slowDb + 5.0f && (hopCount - lastOnsetHop) > 8;
        slowDb += (fastDb > slowDb ? 0.45f : 0.10f) * (fastDb - slowDb);
        if (slowDb < -100) slowDb = fastDb;
        if (! loud) ++belowCount; else belowCount = 0;

        if (attack && ! pending)
        {
            pending = true; pendHops = 0; pendVoiced = 0; pendPeak = rms;
            pendStart = hopNow - 0.01 * sr;
            lastOnsetHop = hopCount;
        }
        if (pending)
        {
            if (voiced && pendVoiced < 10 && pendHops >= 2) pendMidi[pendVoiced++] = m;
            pendPeak = std::max (pendPeak, rms);
            if (++pendHops >= 16)                                          // 170 ms after the attack: now it can be understood
            {
                pending = false; float med = NAN;
                if (pendVoiced >= 4)
                {
                    const float a = pendMidi[pendVoiced / 2]; int agree = 0; float s = 0;
                    for (int k = 0; k < pendVoiced; ++k) if (near (pendMidi[k], a, 0.9f)) { ++agree; s += pendMidi[k]; }
                    if (agree >= 4) med = s / (float) agree;
                }
                startNote (pendStart, med, pendPeak, false);
            }
        }
        else if (active && voiced && ! std::isnan (curMidi))
        {
            if (std::fabs (m - curMidi) > 1.2f)
            {
                if (near (m, chgMidi, 0.8f)) ++chgCount; else { chgMidi = m; chgCount = 1; }
                if (chgCount >= 4) { endNote (hopNow - 4 * hopLen); startNote (hopNow - 4 * hopLen, chgMidi, rms, false); chgCount = 0; lastOnsetHop = hopCount; }
            }
            else chgCount = 0;
        }
        if (active && belowCount >= 4) endNote (hopNow - 4 * hopLen, true);
        if (active && hopNow - notes[(nNotes - 1) & 63].start > 3.0 * sr) endNote (hopNow);     // very long notes end by themselves

        // nothing starts by itself (pad, held chord, loop): make a virtual note every couple of beats while there is sound
        if (! pending && loud && P.on)
        {
            const double beat = 60.0 / std::max (30.0, P.bpm) * sr;
            const double gap = beat * (3.0 - 1.6 * P.density);
            if ((double) (clk - lastEventClk) > gap) startNote (hopNow - 0.15 * sr, NAN, rms, true);
        }
    }

    void startNote (double start, float midi, float level, bool virt)
    {
        if (active) endNote (start);
        Note& n = notes[nNotes & 63];
        n.start = start; n.end = virt ? start + 0.7 * sr : -1; n.midi = midi; n.level = level; n.virt = virt;
        measureChroma (n.chroma);
        double tot = 0, in = 0; for (int i = 0; i < 12; ++i) { tot += n.chroma[i]; if (inScalePc (i)) in += n.chroma[i]; }
        n.cover = tot > 0 ? (float) (in / tot) : 1.0f;
        // one clear pitch only counts when the chroma agrees that one note dominates
        if (! std::isnan (midi))
        {
            // a single note has one dominant pitch class and no strong second one. A power chord or two notes
            // together does not, and then it is treated as a chord (shifts that keep every note inside the scale)
            float top = 0, second = 0; int topPc = 0; for (int i = 0; i < 12; ++i) { const float c = n.chroma[i]; if (c > top) { second = top; top = c; topPc = i; } else if (c > second) second = c; }
            const int pitchPc = (((int) std::lround (midi)) % 12 + 12) % 12;          // the pitch finder must agree with the chroma
            if (top < 0.55f || second > 0.2f || pitchPc != topPc || P.source == Loop) n.midi = midi = NAN;
        }
        heardMode.store (std::isnan (midi) ? 2 : 1); coverPct.store ((int) std::lround (n.cover * 100.0f));
        heardCount.fetch_add (1);
        VizEvent ev; ev.start = start; ev.end = start + 0.2 * sr; ev.midi = std::isnan (midi) ? -1.0f : midi; ev.kind = 0; ev.level = level;
        n.vizIdx = logViz (ev);
        const int id = nNotes++;
        active = ! virt; curMidi = midi; chgCount = 0; lastEventClk = clk;
        generate (id);
    }
    void endNote (double end, bool silence = false)
    {
        if (! active) return;
        Note& n = notes[(nNotes - 1) & 63];
        n.end = std::max (end, n.start + 0.02 * sr);
        if (n.vizIdx >= 0) viz.ev[n.vizIdx & (VizLog::kSize - 1)].end = (float) 0 + n.end;
        active = false; curMidi = NAN;
        lastPhraseEnd = n.end;
        if (silence && (P.styles & StAnswer) && P.on) scheduleAnswer();
    }
    int logViz (const VizEvent& e)
    {
        const int i = viz.writeIndex.load (std::memory_order_relaxed);
        viz.ev[i & (VizLog::kSize - 1)] = e; viz.writeIndex.store (i + 1, std::memory_order_release); return i;
    }

    // ============================================================ generators
    void generate (int id)
    {
        if (! P.on || P.styles == 0) return;
        const Note& n = notes[id & 63];
        const double D = P.lengthBeats * 60.0 / std::max (30.0, P.bpm) * sr;
        const double base0 = n.start + latency;                         // when the delayed dry sound plays this note
        const double minAt = std::max (base0, (double) clk + 8.0);
        const bool mono = ! std::isnan (n.midi);
        const double pr = std::min (1.0, 0.25 + P.density * 1.5);
        const float rangeS = sourceRange();
        const bool bass = P.source == Bass, vox = P.source == Vox, synth = P.source == Synth, guitar = P.source == Guitar;
        const double lenMul = synth ? 2.6 : (guitar ? 0.9 : (vox ? 1.2 : 1.0));
        const float srcLvl = vox ? 0.8f : 1.0f;
        const bool sustained = n.virt || ! mono;

        auto when = [&] (double t) { return snapToGrid (std::max (t, minAt)); };
        auto add = [&] (double at, float semisOrTarget, bool isTarget, double len, float g, int style, bool rev = false)
        {
            Trig t; t.at = at; t.srcStart = n.start; t.noteId = id; t.srcMidi = n.midi; t.loopOK = sustained;
            if (isTarget && mono) { t.targetMidi = semisOrTarget; t.ratio = (float) std::pow (2.0, (semisOrTarget - n.midi) / 12.0); }
            else { const float s = isTarget ? (semisOrTarget - (std::isnan (n.midi) ? 0.0f : n.midi)) : semisOrTarget; t.targetMidi = std::isnan (n.midi) ? NAN : n.midi + s; t.ratio = (float) std::pow (2.0, s / 12.0); }
            t.lenSamples = std::max (0.05 * sr, len); t.gain = g * srcLvl; t.rev = rev; t.style = style;
            t.pan = (rng.next() - 0.5f) * 0.9f; t.glideSec = synth && mono ? 0.05 : 0.0;
            if (t.ratio < 0.3f || t.ratio > 4.1f) return;
            queue (t);
        };
        // shifts for sounds with no single clear pitch: only ones that keep the sound inside the scale
        auto poly = [&] (const int* list, int cnt, int fallback) -> int { const int k = safeShift (n, list, cnt); return k == 99 ? fallback : k; };
        static const int kGhostSteps[6] = { 2, 2, -2, 1, -1, 4 };
        static const int kGhostL[] = { 7, -5, 4, 3, -4, -3, 5, -7, 2, -2, 12, -12 };
        static const int kSparkL[] = { 12, 19, 24, 12, 12, 7 };
        static const int kStepL[] = { 2, -2, 1, -1, 3, -3 };
        static const int kChordL[] = { -12, 4, 3, 7, -5, -8, -9 };
        static const int kTipsyL[] = { 5, -5, 7, -7, 3, -3, 4, -4, 9, -9, 2, -2, 12, -12, 24, -24, 1, -1 };
        const float holdLen = (float) (sustained ? 1.5 : 1.0);

        if ((P.styles & StGhost) && rng.next() < pr)
        {
            if (mono && ! bass) add (when (base0 + D), scaleStep (n.midi, kGhostSteps[rng.pick (6)], rangeS), true, D * 0.9 * lenMul, 1.0f, 1);
            else if (mono) add (when (base0 + D), scaleNearest (n.midi + (rng.next() < 0.5f ? 7.0f : (n.midi + 12.0f > 72.0f ? -12.0f : 12.0f))), true, D * 0.9, 0.95f, 1);
            else { const int k = bass ? (rng.next() < 0.5f ? 12 : -12) : poly (kGhostL, 12, 12); add (when (base0 + D), (float) k, false, D * 1.1 * lenMul * holdLen, 0.95f, 1); }
        }
        if ((P.styles & StSparkle) && ! bass && rng.next() < pr)
        {
            const double at = when (base0 + D * ((P.styles & StGhost) ? 2.0 : 1.0));
            if (mono) add (at, scaleNearest (n.midi + (rng.next() < 0.7f ? 12.0f : 24.0f) + (rng.next() < 0.3f ? 7.0f : 0.0f)), true, D * 0.6, 0.8f, 2);
            else add (at, (float) poly (kSparkL, 6, 12), false, D * 0.7 * holdLen, 0.75f, 2);
        }
        if ((P.styles & StSteps) && ! bass && rng.next() < pr)
        {
            if (mono) add (when (base0 + D * 0.5), scaleStep (n.midi, rng.next() < 0.5f ? 1 : -1, 5.0f), true, D * 0.6 * lenMul, 0.8f, 3);
            else { const int k = safeShift (n, kStepL, 6); if (k != 99) add (when (base0 + D * 0.5), (float) k, false, D * 0.6 * holdLen, 0.7f, 3); }
        }
        if ((P.styles & StChord) && rng.next() < std::min (1.0, pr + 0.2))
        {
            if (mono) add (when (base0), scaleStep (n.midi, rng.next() < 0.65f ? -2 : -4, rangeS), true, D * 1.6 * lenMul, 0.9f, 4);
            else { const int k = bass ? -12 : poly (kChordL, 7, -12); add (when (base0), (float) k, false, D * 1.6 * lenMul * holdLen, 0.85f, 4); }
        }
        if ((P.styles & StTipsy) && rng.next() < pr * 0.9)
        {
            const double at = when (base0 + D * (0.25 + 1.5 * rng.next()));
            const bool rev = rng.next() < 0.3f;
            if (mono)
            {
                float t = scaleStep (n.midi, rng.pick (11) - 5, rangeS);
                if (rng.next() < 0.25f) t = scaleNearest (t + (rng.next() < 0.5f ? 12.0f : -12.0f));
                add (at, t, true, D * (0.5 + rng.next()) * lenMul, 0.85f, 6, rev);
            }
            else add (at, (float) (bass ? -12 : poly (kTipsyL, 18, 12)), false, D * (0.5 + rng.next()) * lenMul * holdLen, 0.8f, 6, rev);
        }
    }

    void scheduleAnswer()
    {
        const double D = P.lengthBeats * 60.0 / std::max (30.0, P.bpm) * sr;
        int idx[3]; int cnt = 0;
        for (int k = 1; k <= 3 && k <= std::min (nNotes, 64); ++k)
        {
            const int id = nNotes - k; const Note& n = notes[id & 63];
            if (lastPhraseEnd - n.start > 2.5 * sr) break;
            if (! std::isnan (n.midi) && ! n.virt) idx[cnt++] = id; else break;
        }
        if (cnt < 2) return;
        if (rng.next() > std::min (1.0, 0.35 + P.density * 1.5)) return;
        const Note& last = notes[idx[0] & 63];
        const float root = scaleNearest (last.midi + (rng.next() < 0.5f ? 0.0f : 7.0f));
        const double t0 = snapToGrid (lastPhraseEnd + latency + D * 2.0);
        for (int k = cnt - 1, step = 0; k >= 0; --k, ++step)
        {
            const Note& n = notes[idx[k] & 63];
            const float shift = root - last.midi;
            const float tgt = scaleNearest (n.midi + shift);
            Trig t; t.at = t0 + step * D * 0.75; t.srcStart = n.start; t.noteId = idx[k]; t.srcMidi = n.midi; t.loopOK = false;
            t.targetMidi = tgt; t.ratio = (float) std::pow (2.0, (tgt - n.midi) / 12.0);
            t.lenSamples = std::min (D * 0.7, 0.25 * sr); t.gain = 0.85f * (P.source == Vox ? 0.8f : 1.0f); t.rev = false; t.style = 5;
            t.pan = (rng.next() - 0.5f) * 0.6f; t.glideSec = 0.0;
            if (t.ratio < 0.3f || t.ratio > 4.1f) continue;
            queue (t);
        }
    }

    void queue (const Trig& t)
    {
        if (nTrig >= 64) return;
        trigs[nTrig++] = t; nextFire = std::min (nextFire, t.at);
        if (firedLog) firedLog->push_back ({ t.at, t.targetMidi, t.ratio, t.style, t.rev, t.srcMidi, t.srcStart });
    }

    void fireDue()
    {
        double nf = 1e18;
        for (int i = 0; i < nTrig;)
        {
            if (trigs[i].at <= (double) clk) { spawn (trigs[i]); trigs[i] = trigs[--nTrig]; }
            else { nf = std::min (nf, trigs[i].at); ++i; }
        }
        nextFire = nf;
    }

    void spawn (const Trig& t)
    {
        Voice* v = nullptr;
        for (auto& x : voices) if (! x.active) { v = &x; break; }
        if (! v) { int64_t oldest = -1; for (auto& x : voices) if (x.age > oldest) { oldest = x.age; v = &x; } }
        const Note& n = notes[t.noteId & 63];
        v->active = true; v->rev = t.rev; v->age = 0; v->xf = -1; v->loopOK = t.loopOK; v->noteId = t.noteId; v->noteStart = n.start;
        v->len = (int64_t) t.lenSamples; v->att = (int64_t) (0.006 * sr); v->rel = (int64_t) std::min (0.06 * sr, t.lenSamples * 0.4);
        v->gain = t.gain;
        const float p = t.pan; v->gl = p > 0 ? 1.0f - p : 1.0f; v->gr = p < 0 ? 1.0f + p : 1.0f;
        v->rateTarget = t.ratio; v->rate = t.glideSec > 0 ? 1.0 : t.ratio;
        v->glide = t.glideSec > 0 ? 1.0 - std::exp (-1.0 / (t.glideSec * sr)) : 1.0;
        v->lp[0] = v->lp[1] = 0; v->lpA = P.source == Bass ? (float) (1.0 - std::exp (-2.0 * kPi * 1400.0 / sr)) : 1.0f;
        v->srcLow = t.srcStart;
        if (! t.rev) v->pos = t.srcStart;
        else
        {
            const double endPos = (n.end > 0 ? n.end : (double) clk - 0.01 * sr);
            v->pos = std::min (endPos, t.srcStart + t.lenSamples * t.ratio);
        }
        VizEvent ev; ev.start = (double) clk; ev.end = (double) clk + t.lenSamples; ev.kind = 1; ev.level = t.gain;
        ev.midi = std::isnan (t.targetMidi) ? -1.0f : t.targetMidi; logViz (ev);
        triggerCount.fetch_add (1); addedCount.fetch_add (1);
    }

    float readHist (int c, double pos) const
    {
        const double fl = std::floor (pos); const float f = (float) (pos - fl);
        const int64_t i1 = (int64_t) fl;
        auto h = [&] (int64_t k) { return hist[c][(size_t) (k & (int64_t) hmask)]; };
        const float y0 = h (i1 - 1), y1 = h (i1), y2 = h (i1 + 1), y3 = h (i1 + 2);
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    void renderVoice (Voice& v, float& outL, float& outR)
    {
        if (v.age >= v.len) { v.active = false; return; }
        // the source note may have ended since the voice started: never read past it (that would play the next note, out of key)
        double srcEnd = 1e18;
        { const Note& n = notes[v.noteId & 63]; if (n.start == v.noteStart && n.end > 0) srcEnd = n.end + 0.02 * sr; }
        const double xfLen = 0.012 * sr;
        double room;
        if (! v.rev) room = std::min ((double) clk - 8.0, srcEnd) - v.pos;
        else room = v.pos - v.srcLow;
        // sustained sources (pads, chords, loops) loop back a little instead of ending, with a short cross-fade
        if (! v.rev && v.loopOK && v.xf < 0 && room < xfLen * std::max (1.0, v.rate) + 64.0)
        {
            const double back = std::min (0.09 * sr, v.pos - v.srcLow);
            if (back > 0.03 * sr) { v.pos2 = v.pos - back; v.xf = 0; }
        }
        if (room <= 0 && v.xf < 0) { v.active = false; return; }
        if (((double) clk - v.pos) > (double) hmask - 64.0) { v.active = false; return; }
        if (v.glide < 1.0) v.rate += v.glide * (v.rateTarget - v.rate);
        float e = 1.0f;
        if (v.age < v.att) e = (float) v.age / (float) v.att;
        const int64_t left = v.len - v.age; if (left < v.rel) e *= (float) left / (float) v.rel;
        const double fadeRoom = 0.008 * sr; if (v.xf < 0 && room < fadeRoom) e *= (float) std::max (0.0, room / fadeRoom);
        float l = readHist (0, v.pos), r = readHist (1, v.pos);
        if (v.xf >= 0)
        {
            const float a = (float) v.xf / (float) xfLen;
            const float w2 = std::sin (a * 1.5707963f), w1 = std::cos (a * 1.5707963f);
            l = l * w1 + readHist (0, v.pos2) * w2; r = r * w1 + readHist (1, v.pos2) * w2;
            v.pos2 += v.rate;
            if (++v.xf >= (int) xfLen) { v.pos = v.pos2; v.xf = -1; }
        }
        if (v.lpA < 1.0f) { v.lp[0] += v.lpA * (l - v.lp[0]); v.lp[1] += v.lpA * (r - v.lp[1]); l = v.lp[0]; r = v.lp[1]; }
        const float g = e * v.gain;
        outL += l * g * v.gl; outR += r * g * v.gr;
        v.pos += v.rev ? -v.rate : v.rate; ++v.age;
    }

    // ============================================================ key finder
    void addChroma()
    {
        const int64_t base = decW - kG;
        for (size_t k = 0; k < gCos.size(); ++k)
        {
            float s1 = 0, s2 = 0; const float c = gCos[k];
            for (int i = 0; i < kG; ++i) { const float x = decAt (base + i) * gWin[(size_t) i] + c * s1 - s2; s2 = s1; s1 = x; }
            const double mag = (double) s1 * s1 + (double) s2 * s2 - (double) c * s1 * s2;
            chroma[(int) gFreqMidi[k] % 12] += std::sqrt (std::max (0.0, mag));
        }
    }
    void finishListen()
    {
        double tot = 0; for (double c : chroma) tot += c;
        if (tot <= 0) { listenState.store (0); return; }
        double h[12]; for (int i = 0; i < 12; ++i) h[i] = chroma[i] / tot;
        double best = -1e9; int bk = 9, bs = 1; double bcov = 0;
        for (int s = 0; s < 10; ++s)
            for (int k = 0; k < 12; ++k)
            {
                const Scale& sc = getScale (s); double in = 0;
                for (int d = 0; d < sc.n; ++d) in += h[(k + sc.semis[d]) % 12];
                double score = in - 0.35 * (1.0 - in) + 0.5 * h[k] + 0.25 * h[(k + 7) % 12] + ((s == 0 || s == 1) ? 0.02 : 0.0);
                if (score > best) { best = score; bk = k; bs = s; bcov = in; }
            }
        heardKey.store (bk); heardScale.store (bs); heardPct.store ((int) std::lround (bcov * 100.0));
        listenState.store (2);
    }
};
} // namespace secretweapon
