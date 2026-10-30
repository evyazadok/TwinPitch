// GOAT SLAP — vocal slap delay + depth
// Header-only DSP engine (no JUCE dependency, so it can be tested offline).
//
// Signal flow (per channel):
//
//   in ──┬──────────────────────────────── dry ──(Distance pushes it back)──┐
//        ├─ Space: early reflections only (no tail) ─────────────────────────┤
//        └─ de-ess ─▶ [delay line] ─▶ heads 1..3 ─▶ machine (tape/analog/…) ─┤──▶ out
//                         ▲                    │  ─▶ doubler ─▶ duck/throw ─┘
//                         └──── feedback ◀─────┘
//
//  * Space is the "front-to-back panner" idea: early reflections with no reverb tail.
//    Distance = how far back the voice sits; Room = size of the reflections.
//  * The slap: a short delay (synced to tempo or in ms), slightly different time on
//    each side (Width), optional extra playback heads, low feedback.
//  * Machine colours the repeat: Clean, Analog, Radio, Studio Reel, Cassette,
//    Broken 4-Track, Lo-Fi, Dictaphone. Tape machines have wow/flutter, saturation,
//    head bump and a Speed control; Age adds wobble, noise and dropouts.
//  * Duck: the repeat gets out of the way while the singer sings.
//  * Throw: momentarily raises feedback and level for end-of-phrase throws.
//  * Feeling Tipsy: on some repeats something "goes wrong" on purpose — the repeat
//    plays reversed, an octave up, stutters, wobbles, or the room changes.

#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <atomic>
#include <array>

namespace goatslap
{
static constexpr double kPi = 3.14159265358979323846;

//==============================================================================
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    void reset() { x1 = x2 = y1 = y2 = 0; }
    static double clampF (double sr, double f) { return std::clamp (f, 10.0, sr * 0.45); }
    void lowPass (double sr, double f, double q = 0.7071)
    {
        const double w = 2 * kPi * clampF (sr, f) / sr, c = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
        b0 = (1 - c) * 0.5 / a0; b1 = (1 - c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
    }
    void highPass (double sr, double f, double q = 0.7071)
    {
        const double w = 2 * kPi * clampF (sr, f) / sr, c = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
        b0 = (1 + c) * 0.5 / a0; b1 = -(1 + c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
    }
    void peak (double sr, double f, double q, double dB)
    {
        const double A = std::pow (10.0, dB / 40.0), w = 2 * kPi * clampF (sr, f) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        const double a0 = 1 + al / A;
        b0 = (1 + al * A) / a0; b1 = -2 * c / a0; b2 = (1 - al * A) / a0; a1 = -2 * c / a0; a2 = (1 - al / A) / a0;
    }
    inline double process (double x)
    {
        const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
};

struct Smoother
{
    double c = 0, v = 0, t = 0;
    void prepare (double sr, double ms) { c = std::exp (-1.0 / (0.001 * ms * sr)); }
    void reset (double x) { v = t = x; }
    inline double next() { v = t + c * (v - t); return v; }
};

struct Rng
{
    uint32_t s = 0x9E3779B9u;
    inline uint32_t u32() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline double uni() { return (u32() & 0xFFFFFF) / double (0xFFFFFF); }   // 0..1
    inline double bi() { return uni() * 2.0 - 1.0; }                        // -1..1
};

//==============================================================================
// Delay line with 4-point Hermite interpolation
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        int size = 1; while (size < maxSamples + 8) size <<= 1;
        buf.assign ((size_t) size, 0.0f); mask = size - 1; w = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    inline void write (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    // delay measured back from the most recently written sample (1 = previous write)
    inline double read (double d) const
    {
        const double rp = (double) w - d;
        const double fl = std::floor (rp);
        const double f = rp - fl;
        const int i = (int) fl;
        const double xm1 = buf[(size_t) ((i - 1) & mask)], x0 = buf[(size_t) (i & mask)];
        const double x1 = buf[(size_t) ((i + 1) & mask)], x2 = buf[(size_t) ((i + 2) & mask)];
        const double c1 = 0.5 * (x1 - xm1), c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2, c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
private:
    std::vector<float> buf;
    int mask = 0, w = 0;
};

//==============================================================================
// Small "rotating heads" pitch shifter (same principle as TwinPitch) — used for the
// doubler (a few cents) and the Tipsy octave.
class MicroShifter
{
public:
    void prepare (double sampleRate) { sr = sampleRate; line.prepare ((int) (0.2 * sr)); phase = 0; }
    void reset() { line.reset(); phase = 0; }
    inline double process (double x, double ratio, double baseMs, double windowMs)
    {
        line.write ((float) x);
        const double win = windowMs * 0.001 * sr, base = baseMs * 0.001 * sr;
        phase += (1.0 - ratio) / win; phase -= std::floor (phase);
        double p2 = phase + 0.5; if (p2 >= 1.0) p2 -= 1.0;
        const double s = std::sin (kPi * phase), g1 = s * s;
        return g1 * line.read (base + phase * win + 1.0) + (1.0 - g1) * line.read (base + p2 * win + 1.0);
    }
private:
    DelayLine line; double sr = 48000, phase = 0;
};

//==============================================================================
// Early reflections only (no tail): the depth / "front-to-back" control
class EarlyReflections
{
public:
    static constexpr int kTaps = 8;
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& l : lines) l.prepare ((int) (0.08 * sr));
        for (auto& f : tone) f.reset();
    }
    void reset() { for (auto& l : lines) l.reset(); for (auto& f : tone) f.reset(); }
    void setTone (double cutoff) { for (auto& f : tone) f.lowPass (sr, cutoff, 0.6); }
    // roomScale 0..1
    inline void process (double inL, double inR, double roomScale, double& outL, double& outR)
    {
        static const double tL[kTaps] = { 7.1, 11.3, 16.9, 23.2, 29.4, 37.1, 43.3, 53.0 };
        static const double tR[kTaps] = { 8.9, 13.2, 19.4, 25.8, 31.3, 39.6, 47.2, 57.4 };
        static const double g[kTaps]  = { 0.82, -0.70, 0.61, -0.52, 0.45, -0.38, 0.32, -0.27 };
        lines[0].write ((float) inL); lines[1].write ((float) inR);
        const double scale = (0.22 + 0.78 * roomScale) * 0.001 * sr;
        double l = 0, r = 0;
        for (int i = 0; i < kTaps; ++i)
        {
            // cross-feed alternate taps for a wider, more natural room
            const bool cross = (i & 1) != 0;
            l += g[i] * lines[cross ? 1 : 0].read (tL[i] * scale);
            r += g[i] * lines[cross ? 0 : 1].read (tR[i] * scale);
        }
        outL = tone[0].process (l * 0.55);
        outR = tone[1].process (r * 0.55);
    }
private:
    double sr = 48000;
    DelayLine lines[2];
    Biquad tone[2];
};

//==============================================================================
struct Machine
{
    const char* name;
    double lpHz, hpHz, sat, wowMs, flutterMs, noise;
    int crushBits, downsample;
    double bumpDb;
    bool isTape;
};

inline const Machine& getMachine (int i)
{
    static const Machine m[8] = {
        //  name              lp     hp    sat   wow    flut   noise   bits ds bump tape
        { "Clean",          17000, 110, 0.00, 0.00,  0.00,  0.0,     0, 1, 0.0, false },
        { "Analog",          3800, 150, 0.30, 0.06,  0.00,  0.0006,  0, 1, 1.0, false },
        { "Radio",           3200, 650, 0.60, 0.00,  0.00,  0.0008,  0, 1, 0.0, false },
        { "Studio Reel",    11500,  70, 0.35, 0.16,  0.03,  0.0004,  0, 1, 2.5, true  },
        { "Cassette",        7000,  90, 0.45, 0.45,  0.06,  0.0015,  0, 1, 1.5, true  },
        { "Broken 4-Trk",    5200, 120, 0.70, 0.90,  0.12,  0.0030,  0, 1, 2.0, true  },
        { "Lo-Fi",           6000, 200, 0.20, 0.00,  0.00,  0.0010,  8, 4, 0.0, false },
        { "Dictaphone",      3500, 450, 0.50, 0.30,  0.20,  0.0040, 10, 2, 0.0, true  },
    };
    return m[std::clamp (i, 0, 7)];
}

//==============================================================================
enum Chaos : uint32_t { ChaosReverse = 1, ChaosOctave = 2, ChaosStutter = 4, ChaosRoom = 8, ChaosWobble = 16 };

struct Params
{
    double timeMs   = 107.0;   // final slap time (sync already resolved by the host wrapper)
    double feelMs   = 0.0;     // push/pull, -20..20
    double mix      = 0.22;    // 0..1 (insert mode)
    bool   aux      = false;   // 100% wet
    double distance = 0.25;    // 0..1
    double room     = 0.3;     // 0..1
    double tone     = -0.3;    // -1 dark .. +1 bright
    double width    = 0.5;     // 0..1
    double feedback = 0.05;    // 0..0.9
    bool   duckOn   = true;
    double duck     = 0.5;     // 0..1
    int    machine  = 0;
    int    speed    = 1;       // 0=7.5 ips, 1=15, 2=30
    double age      = 0.3;     // 0..1
    double saturate = 0.3;     // 0..1
    int    heads    = 1;       // 1..3
    bool   dbl      = false;   // doubler
    bool   throwOn  = false;
    double tipsy    = 0.0;     // 0..1
    uint32_t chaos  = ChaosReverse | ChaosOctave | ChaosStutter | ChaosRoom | ChaosWobble;
    double outputDb = 0.0;
};

//==============================================================================
// Live data for the UI visualizer (lock-free, written by the audio thread)
struct VizBuffer
{
    static constexpr int kSize = 128;
    std::array<std::atomic<float>, kSize> dry {}, wet {};
    std::array<std::atomic<uint8_t>, kSize> event {};
    std::atomic<int> writeIndex { 0 };
};

//==============================================================================
class Engine
{
public:
    static constexpr double kMaxTimeMs = 1000.0;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const int maxD = (int) std::ceil ((kMaxTimeMs * 3.0 * 1.2 + 60.0) * 0.001 * sr);
        for (auto& l : lines) l.prepare (maxD);
        er.prepare (sr);
        for (auto& s : dblShift) s.prepare (sr);
        for (auto& s : octShift) s.prepare (sr);

        for (auto* s : { &timeL, &timeR, &mixDry, &mixWet, &erGain, &dryGain, &fbSm, &duckSm, &throwSm, &dblSm, &outSm, &roomSm, &wobbleSm, &evSm })
            s->prepare (sr, 40.0);
        timeL.prepare (sr, 120.0); timeR.prepare (sr, 120.0);   // slower = tape-like pitch bend on time changes
        roomSm.prepare (sr, 60.0);
        throwSm.prepare (sr, 90.0);
        evSm.prepare (sr, 4.0);
        envAtt = std::exp (-1.0 / (0.004 * sr)); envRel = std::exp (-1.0 / (0.120 * sr));
        hfAtt  = std::exp (-1.0 / (0.002 * sr)); hfRel  = std::exp (-1.0 / (0.060 * sr));
        vizEvery = std::max (1, (int) (sr * 0.012));
        primed = false;
        reset();
    }

    void reset()
    {
        for (auto& l : lines) l.reset();
        er.reset();
        for (auto& s : dblShift) s.reset();
        for (auto& s : octShift) s.reset();
        for (int c = 0; c < 2; ++c) { lp[c].reset(); hp[c].reset(); bump[c].reset(); deessLP[c].reset(); hfEnv[c] = allEnv[c] = 0; held[c] = 0; }
        env = 0; wowPh = flutPh = 0; driftL = driftR = 0;
        segCounter = 0; segLen = 1; event = 0; stutterSlice = 1;
        dropGain = 1.0; dropTimer = 0;
        vizCount = 0; vizDry = vizWet = 0; vizEv = 0;
        filterCountdown = 0;
    }

    void setParams (const Params& p) { params = p; }
    VizBuffer viz;

    void process (float* left, float* right, int n)
    {
        const Params& p = params;
        const Machine& m = getMachine (p.machine);

        // ---- targets
        const double t = std::clamp (p.timeMs + p.feelMs, 10.0, kMaxTimeMs);
        const double wd = std::clamp (p.width, 0.0, 1.0) * 0.09;
        const double tL = t * (1.0 - wd) * 0.001 * sr, tR = t * (1.0 + wd) * 0.001 * sr;
        const double mix = std::clamp (p.mix, 0.0, 1.0);
        const double tDry = p.aux ? 0.0 : std::min (1.0, 2.0 * (1.0 - mix));
        const double tWet = p.aux ? 1.0 : std::min (1.0, 2.0 * mix);
        const double dist = std::clamp (p.distance, 0.0, 1.0);
        const double tEr = std::pow (dist, 1.1) * 1.1;
        const double tDryDist = 1.0 - 0.35 * dist;   // the voice steps back as the room comes up
        const double tFb = std::clamp (p.feedback, 0.0, 0.9);
        const double tOut = std::pow (10.0, p.outputDb / 20.0);

        if (! primed)
        {
            timeL.reset (tL); timeR.reset (tR); mixDry.reset (tDry); mixWet.reset (tWet);
            erGain.reset (tEr); dryGain.reset (tDryDist); fbSm.reset (tFb); duckSm.reset (p.duckOn ? p.duck : 0.0);
            throwSm.reset (p.throwOn ? 1.0 : 0.0); dblSm.reset (p.dbl ? 1.0 : 0.0); outSm.reset (tOut);
            roomSm.reset (p.room); wobbleSm.reset (0.0); evSm.reset (0.0);
            segLen = std::max (1, (int) tL);
            primed = true;
        }
        timeL.t = tL; timeR.t = tR; mixDry.t = tDry; mixWet.t = tWet; erGain.t = tEr; dryGain.t = tDryDist;
        fbSm.t = tFb; duckSm.t = p.duckOn ? p.duck : 0.0; throwSm.t = p.throwOn ? 1.0 : 0.0;
        dblSm.t = p.dbl ? 1.0 : 0.0; outSm.t = tOut;

        // ---- machine-dependent constants
        const double speedMul = m.isTape ? (p.speed == 0 ? 0.62 : p.speed == 2 ? 1.35 : 1.0) : 1.0;
        const double ageMul = 0.3 + 1.4 * std::clamp (p.age, 0.0, 1.0);
        const double satAmt = std::clamp (m.sat * (0.3 + 1.4 * std::clamp (p.saturate, 0.0, 1.0)), 0.0, 1.5);
        const double satDrive = 1.0 + 6.0 * satAmt;
        const double wowDepth = m.wowMs * ageMul * 0.001 * sr;
        const double flutDepth = m.flutterMs * ageMul * 0.001 * sr;
        const double wowRate = (m.isTape ? 0.55 / speedMul : 0.35) / sr;
        const double flutRate = 7.3 / sr;
        const double noiseAmt = m.noise * ageMul;
        const bool dropouts = p.machine == 5 || (m.isTape && p.age > 0.8);
        const double crushStep = m.crushBits > 0 ? std::pow (2.0, -(double) m.crushBits + 1) : 0.0;
        const int ds = std::max (1, m.downsample);
        const double toneDark = std::min (0.0, p.tone), toneBright = std::max (0.0, p.tone);
        const double lpHz = m.lpHz * speedMul * std::exp2 (toneDark * 2.6);
        const double hpHz = m.hpHz * std::exp2 (toneBright * 1.6);
        const int heads = std::clamp (p.heads, 1, 3);
        const double headG[3] = { 1.0, heads >= 2 ? 0.72 : 0.0, heads >= 3 ? 0.52 : 0.0 };
        const double headNorm = 1.0 / (headG[0] + headG[1] + headG[2]);
        const double tipsy = std::clamp (p.tipsy, 0.0, 1.0);

        for (int i = 0; i < n; ++i)
        {
            if (filterCountdown-- <= 0)
            {
                filterCountdown = 32;
                for (int c = 0; c < 2; ++c)
                {
                    lp[c].lowPass (sr, lpHz, 0.6);
                    hp[c].highPass (sr, hpHz, 0.6);
                    bump[c].peak (sr, 95.0 * speedMul, 0.9, m.bumpDb);
                    deessLP[c].lowPass (sr, 5500.0, 0.7071);
                }
                er.setTone (7500.0 * (1.0 - 0.55 * dist) + 1500.0);
            }

            const double inL = left[i], inR = right[i];

            // ---- ducking envelope (from the dry voice)
            const double a = std::max (std::abs (inL), std::abs (inR));
            env = a > env ? a + envAtt * (env - a) : a + envRel * (env - a);
            const double envDb = 20.0 * std::log10 (env + 1e-9);
            const double thr = std::clamp ((envDb + 42.0) / 30.0, 0.0, 1.0);
            const double thw = throwSm.next();
            const double duckDb = -20.0 * duckSm.next() * thr * (1.0 - thw);
            const double duckG = std::pow (10.0, duckDb / 20.0);

            // ---- tipsy event scheduler (one decision per repeat period)
            if (++segCounter >= segLen)
            {
                segCounter = 0;
                segLen = std::max ((int) (0.05 * sr), (int) timeL.v);
                event = 0;
                if (tipsy > 0.0 && p.chaos != 0 && rng.uni() < tipsy * 0.65)
                {
                    uint32_t opts[5]; int k = 0;
                    for (uint32_t b : { (uint32_t) ChaosReverse, (uint32_t) ChaosOctave, (uint32_t) ChaosStutter, (uint32_t) ChaosRoom, (uint32_t) ChaosWobble })
                        if (p.chaos & b) opts[k++] = b;
                    event = opts[rng.u32() % (uint32_t) k];
                    if (event == ChaosRoom) roomSm.t = rng.uni();
                    stutterSlice = std::max ((int) (0.035 * sr), segLen / (2 + (int) (rng.u32() % 3)));
                }
                if (event != ChaosRoom) roomSm.t = p.room;
            }
            // fade window around the segment (avoids clicks when events start/stop)
            const int fadeLen = std::max (16, (int) (0.006 * sr));
            const double segEnv = std::min (1.0, std::min ((double) segCounter / fadeLen, (double) (segLen - segCounter) / fadeLen));
            const bool altActive = event == ChaosReverse || event == ChaosOctave || event == ChaosStutter;
            evSm.t = altActive ? 1.0 : 0.0;
            const double eMix = evSm.next() * std::max (0.0, segEnv);
            wobbleSm.t = event == ChaosWobble ? 1.0 : 0.0;
            const double wob = wobbleSm.next();

            // ---- tape modulation
            wowPh += wowRate; if (wowPh >= 1.0) wowPh -= 1.0;
            flutPh += flutRate; if (flutPh >= 1.0) flutPh -= 1.0;
            driftL += (rng.bi() * 0.02 - driftL) * 0.0005; driftR += (rng.bi() * 0.02 - driftR) * 0.0005;
            const double wowS = std::sin (2 * kPi * wowPh), wowC = std::cos (2 * kPi * wowPh), fl = std::sin (2 * kPi * flutPh);
            const double deepWob = wob * 0.0035 * sr * std::sin (2 * kPi * wowPh * 4.0);
            const double modL = wowDepth * (wowS + 40.0 * driftL) + flutDepth * fl + deepWob;
            const double modR = wowDepth * (wowC + 40.0 * driftR) + flutDepth * fl * 0.8 + deepWob;

            if (dropouts)
            {
                if (dropTimer <= 0 && rng.uni() < 0.00004 * (0.5 + p.age)) dropTimer = (int) (sr * (0.03 + 0.1 * rng.uni()));
                const double target = dropTimer > 0 ? 0.25 : 1.0;
                if (dropTimer > 0) --dropTimer;
                dropGain += (target - dropGain) * 0.004;
            }
            else dropGain = 1.0;

            const double dL = timeL.next(), dR = timeR.next();
            const double fb = std::max (fbSm.next(), 0.62 * thw);

            double wetOut[2];
            const double ins[2] = { inL, inR };
            const double dly[2] = { dL, dR };
            const double mods[2] = { modL, modR };

            for (int c = 0; c < 2; ++c)
            {
                const double D = std::max (8.0, dly[c] + mods[c]);
                // heads
                double tap = 0.0;
                for (int h = 0; h < heads; ++h) tap += headG[h] * lines[c].read (D * (h + 1));
                tap *= headNorm * (heads > 1 ? 1.6 : 1.0);

                // tipsy alternatives for this repeat
                if (eMix > 1e-4)
                {
                    double alt = tap;
                    const double k = (double) segCounter;
                    if (event == ChaosReverse) alt = lines[c].read (std::max (1.0, 2.0 * k + 1.0 + (D - segLen)));
                    else if (event == ChaosStutter)
                    {
                        const double kk = std::fmod (k, (double) stutterSlice);
                        const double w = std::min (1.0, std::min (kk / fadeLen, ((double) stutterSlice - kk) / fadeLen));
                        alt = w * lines[c].read (D + k - kk);
                    }
                    else if (event == ChaosOctave) alt = octShift[c].process (tap, 2.0, 2.0, 28.0) * 0.9;
                    tap = (1.0 - eMix) * tap + eMix * alt;
                }
                if (event != ChaosOctave) octShift[c].process (tap, 2.0, 2.0, 28.0);   // keep it primed

                // machine colour
                double y = hp[c].process (lp[c].process (tap));
                if (std::abs (m.bumpDb) > 1.0e-9) y = bump[c].process (y);
                if (satAmt > 1e-4) y = std::tanh (satDrive * y + 0.08 * satAmt) / std::tanh (satDrive) - std::tanh (0.08 * satAmt) / std::tanh (satDrive);
                if (ds > 1) { if ((sampleCounter % (uint64_t) ds) == 0) held[c] = y; y = held[c]; }
                if (crushStep > 0) y = std::round (y / crushStep) * crushStep;
                if (noiseAmt > 0) y += noiseAmt * rng.bi();
                y *= dropGain;

                // de-ess the send (so S's don't spit into the repeats)
                const double x = ins[c];
                const double low = deessLP[c].process (x), hi = x - low;
                const double ah = std::abs (hi), aa = std::abs (x);
                hfEnv[c] = ah > hfEnv[c] ? ah + hfAtt * (hfEnv[c] - ah) : ah + hfRel * (hfEnv[c] - ah);
                allEnv[c] = aa > allEnv[c] ? aa + hfAtt * (allEnv[c] - aa) : aa + hfRel * (allEnv[c] - aa);
                const double ratio = hfEnv[c] / (allEnv[c] + 1e-9);
                const double hiG = ratio > 0.45 ? std::max (0.25, 0.45 / ratio) : 1.0;
                const double send = low + hi * hiG;

                // write: input + feedback (soft-limited so it never runs away)
                const double w = send + fb * y;
                lines[c].write ((float) (std::abs (w) > 0.8 ? std::tanh (w) : w));

                wetOut[c] = y;
            }

            // doubler: the repeat a few cents apart on each side
            const double dbl = dblSm.next();
            if (dbl > 1e-4)
            {
                const double sl = dblShift[0].process (wetOut[0], std::exp2 (8.0 / 1200.0), 4.0, 32.0);
                const double sr2 = dblShift[1].process (wetOut[1], std::exp2 (-8.0 / 1200.0), 6.0, 32.0);
                wetOut[0] = (1.0 - dbl) * wetOut[0] + dbl * sl;
                wetOut[1] = (1.0 - dbl) * wetOut[1] + dbl * sr2;
            }
            else { dblShift[0].process (wetOut[0], 1.0, 4.0, 32.0); dblShift[1].process (wetOut[1], 1.0, 6.0, 32.0); }

            // space (early reflections of the dry voice)
            double erL, erR;
            er.process (inL, inR, roomSm.next(), erL, erR);

            const double wg = mixWet.next() * duckG * (1.0 + 0.4 * thw);
            const double dg = mixDry.next(), eg = erGain.next(), dd = dryGain.next(), og = outSm.next();
            const double wetL = wetOut[0] * wg, wetR = wetOut[1] * wg;
            const double outL = (inL * dd * dg + erL * eg * (p.aux ? 1.0 : dg) + wetL) * og;
            const double outR = (inR * dd * dg + erR * eg * (p.aux ? 1.0 : dg) + wetR) * og;
            left[i] = (float) outL;
            right[i] = (float) outR;

            // visualizer
            vizDry = std::max (vizDry, a);
            vizWet = std::max (vizWet, std::max (std::abs (wetL), std::abs (wetR)));
            if (eMix > 0.5 || wob > 0.5 || event == ChaosRoom) vizEv = (uint8_t) event;
            if (++vizCount >= vizEvery)
            {
                const int wi = viz.writeIndex.load (std::memory_order_relaxed);
                viz.dry[(size_t) wi].store ((float) vizDry, std::memory_order_relaxed);
                viz.wet[(size_t) wi].store ((float) vizWet, std::memory_order_relaxed);
                viz.event[(size_t) wi].store (vizEv, std::memory_order_relaxed);
                viz.writeIndex.store ((wi + 1) % VizBuffer::kSize, std::memory_order_release);
                vizCount = 0; vizDry = vizWet = 0; vizEv = 0;
            }
            ++sampleCounter;
        }
    }

private:
    double sr = 48000;
    Params params;
    DelayLine lines[2];
    EarlyReflections er;
    MicroShifter dblShift[2], octShift[2];
    Biquad lp[2], hp[2], bump[2], deessLP[2];
    double hfEnv[2] {}, allEnv[2] {}, held[2] {};
    Smoother timeL, timeR, mixDry, mixWet, erGain, dryGain, fbSm, duckSm, throwSm, dblSm, outSm, roomSm, wobbleSm, evSm;
    double env = 0, envAtt = 0, envRel = 0, hfAtt = 0, hfRel = 0;
    double wowPh = 0, flutPh = 0, driftL = 0, driftR = 0, dropGain = 1.0;
    int dropTimer = 0;
    int segCounter = 0, segLen = 1, stutterSlice = 1, filterCountdown = 0;
    uint32_t event = 0;
    int vizEvery = 576, vizCount = 0;
    double vizDry = 0, vizWet = 0; uint8_t vizEv = 0;
    uint64_t sampleCounter = 0;
    Rng rng;
    bool primed = false;
};

//==============================================================================
// Tempo divisions (in quarter-note beats)
struct Division { const char* name; double beats; };
inline const Division* getDivisions()
{
    static const Division d[12] = {
        { "1/64", 1.0 / 16 }, { "1/32T", 1.0 / 12 }, { "1/32", 1.0 / 8 }, { "1/16T", 1.0 / 6 },
        { "1/32.", 3.0 / 16 }, { "1/16", 1.0 / 4 }, { "1/8T", 1.0 / 3 }, { "1/16.", 3.0 / 8 },
        { "1/8", 1.0 / 2 }, { "1/4T", 2.0 / 3 }, { "1/8.", 3.0 / 4 }, { "1/4", 1.0 } };
    return d;
}
static constexpr int kNumDivisions = 12;

} // namespace goatslap
