#pragma once
// HANGOVER DSP: wear and tear for melodic sounds. No noise source anywhere: everything here
// is made of the signal itself, so silence in = silence out (except a reverb tail).
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <atomic>

namespace hangover
{
constexpr double kPi = 3.14159265358979323846;

// Per-instrument character. Mode scales how each module behaves.
struct ModeCfg { const char* name; float wobRate, wob, dig, dist, spaceSize, space, trem, drops; };
inline const ModeCfg& getMode (int i)
{
    static const ModeCfg m[4] = {
        { "GUITAR", 0.90f, 1.00f, 0.80f, 1.10f, 0.35f, 0.80f, 1.00f, 0.90f },
        { "KEYS",   0.70f, 0.80f, 1.00f, 0.80f, 0.45f, 1.00f, 1.20f, 1.00f },
        { "PAD",    0.35f, 1.30f, 0.70f, 0.60f, 0.75f, 1.40f, 0.80f, 0.60f },
        { "LOOP",   0.80f, 1.00f, 1.10f, 1.00f, 0.40f, 0.80f, 1.00f, 1.20f } };
    return m[std::max (0, std::min (3, i))];
}

enum Chaos : unsigned { ChaosDive = 1, ChaosCrush = 2, ChaosSwell = 4, ChaosHiccup = 8, ChaosMuffle = 16 };
inline const char* chaosName (int e)
{
    switch (e) { case 1: return "tape dive"; case 2: return "crush"; case 3: return "swell"; case 4: return "hiccup"; case 5: return "muffle"; default: return "-"; }
}

struct Params
{
    int mode = 1;
    double wobble = 0, trem = 0, digital = 0, distort = 0, space = 0, drops = 0;   // 0..1, 0 when the module is off
    bool wobbleFlux = false, dropsFlux = false, tremPan = true;
    double tremBeats = 0.5;     // length of one tremolo cycle in beats
    double bpm = 120.0;
    double mix = 1.0;
    double magnitude = 1.0;     // global wear (already includes the fade)
    double tipsy = 0.0;
    unsigned chaos = 31;
    double outputDb = 0.0;
};

// -------- small helpers
struct Rng
{
    uint32_t s = 2463534242u;
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xFFFFFF) / 16777216.0f; }
};
struct OnePole
{
    float z = 0;
    float lp (float x, float a) { z += a * (x - z); return z; }
};
inline float coefHz (double hz, double sr) { return (float) (1.0 - std::exp (-2.0 * kPi * hz / sr)); }

struct Smooth
{
    double v = 0; bool init = false;
    double step (double target, double a) { if (! init) { v = target; init = true; } v += a * (target - v); return v; }
};

// ring of recent output levels for the editor's visualiser
struct VizBuffer
{
    static constexpr int kSize = 128;
    std::atomic<float> level[kSize];
    std::atomic<int> writeIndex { 0 };
    VizBuffer() { for (auto& l : level) l.store (0.0f); }
    void push (float v) { const int i = writeIndex.load (std::memory_order_relaxed); level[i % kSize].store (v, std::memory_order_relaxed); writeIndex.store (i + 1, std::memory_order_release); }
};

// ============================================================================
class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        int n = 1; while (n < (int) (sr * 0.16)) n <<= 1;
        dl[0].assign ((size_t) n, 0.0f); dl[1].assign ((size_t) n, 0.0f); dmask = n - 1; wpos = 0;
        reset();
        // small reverb: 4 combs + 2 allpasses per channel
        static const int combBase[4] = { 1116, 1188, 1277, 1356 };
        static const int apBase[2] = { 556, 441 };
        for (int c = 0; c < 2; ++c)
        {
            for (int i = 0; i < 4; ++i) { comb[c][i].buf.assign ((size_t) (combBase[i] * 2.4 * sr / 44100.0 + c * 23 + 8), 0.0f); comb[c][i].pos = 0; comb[c][i].lp = 0; }
            for (int i = 0; i < 2; ++i) { ap[c][i].buf.assign ((size_t) (apBase[i] * sr / 44100.0 + c * 11 + 4), 0.0f); ap[c][i].pos = 0; }
        }
        // dry path delay equal to the wobble centre delay so the mix control never combs
        for (int c = 0; c < 2; ++c) dryBuf[c].assign ((size_t) latencySamples(), 0.0f);
        dryPos = 0;
    }

    int latencySamples() const { return (int) std::lround (sr * baseMs / 1000.0); }

    void reset()
    {
        std::fill (dl[0].begin(), dl[0].end(), 0.0f); std::fill (dl[1].begin(), dl[1].end(), 0.0f);
        wph[0] = 0.0; wph[1] = 0.3; fph = 0.0; tph = 0.0;
        env = 0; dropSamplesLeft = -1; nextDropSec = 1.5; evCountSec = 6.0;
        for (auto& e : ev) e = 0.0f;
        for (auto& z : dcZ) z = 0;
        diveRamp = 0;
    }

    void setParams (const Params& p) { P = p; }

    VizBuffer viz;
    std::atomic<int> lastEvent { 0 };      // 1..5, 0 none
    std::atomic<float> eventPulse { 0.0f };

    void process (float* L, float* R, int n)
    {
        const ModeCfg& M = getMode (P.mode);
        const double mag = P.magnitude;
        const double aW = std::min (1.0, P.wobble * M.wob * mag), aT = std::min (1.0, P.trem * M.trem * mag),
                     aD = std::min (1.0, P.digital * M.dig * mag), aX = std::min (1.0, P.distort * M.dist * mag),
                     aS = std::min (1.0, P.space * M.space * mag), aR = std::min (1.0, P.drops * M.drops * mag);
        const double outG = std::pow (10.0, P.outputDb / 20.0);
        const float envA = coefHz (400.0, sr), envR = coefHz (8.0, sr);
        const double dt = 1.0 / sr;
        const double smA = 1.0 - std::exp (-1.0 / (0.03 * sr));
        const float evA = coefHz (14.0, sr), evR = coefHz (3.0, sr);

        for (int i = 0; i < n; ++i)
        {
            const float inL = L[i], inR = R[i];

            // ---- signal presence (the clock for drops and tipsy only runs while something plays)
            const float mono = std::fabs (0.5f * (inL + inR));
            env += (mono > env ? envA : envR) * (mono - env);
            const bool present = env > 0.0025f;   // about -52 dBFS

            // ---- tipsy events
            if (present && P.tipsy > 0.001 && P.chaos != 0)
            {
                evCountSec -= dt;
                if (evCountSec <= 0)
                {
                    const double mean = 14.0 - 11.5 * P.tipsy;
                    evCountSec = mean * (0.5 + rng.next());
                    startEvent();
                }
            }
            // event envelopes: attack/hold/release
            for (int k = 0; k < 5; ++k)
            {
                if (evHold[k] > 0) { evHold[k] -= dt; ev[k] += evA * (1.0f - ev[k]); }
                else ev[k] += evR * (0.0f - ev[k]);
            }
            // tape dive: the delay line stretches, then snaps back
            double diveMs = 0;
            if (diveT >= 0)
            {
                diveT += dt;
                const double up = 0.26, down = 0.7;
                if (diveT < up) diveMs = diveMax * (diveT / up);
                else if (diveT < up + down) diveMs = diveMax * (1.0 - (diveT - up) / down);
                else diveT = -1;
            }

            // ---- WOBBLE: modulated delay line
            float wetL = inL, wetR = inR;
            {
                const double rate = M.wobRate * (P.wobbleFlux ? (1.0 + 0.45 * fluxA) : 1.0);
                const double depthMs = 4.0 * aW * (P.wobbleFlux ? (1.0 + 0.45 * fluxB) : 1.0) * (1.0 + 1.0 * ev[2]);
                const double flutMs = 0.06 * aW;
                // slow random drift (smoothed random walk, ~0.5 s)
                driftT -= dt;
                if (driftT <= 0) { driftT = 0.25 + 0.5 * rng.next(); driftTarget[0] = rng.next() * 2.0f - 1.0f; driftTarget[1] = rng.next() * 2.0f - 1.0f; fluxTarget[0] = rng.next() * 2.0f - 1.0f; fluxTarget[1] = rng.next() * 2.0f - 1.0f; }
                const float da = coefHz (0.6, sr);
                for (int c = 0; c < 2; ++c) drift[c] += da * (driftTarget[c] - drift[c]);
                const float fa = coefHz (0.25, sr);
                fluxA += fa * (fluxTarget[0] - fluxA); fluxB += fa * (fluxTarget[1] - fluxB);

                wph[0] += rate * dt; wph[1] += rate * dt; fph += 7.3 * dt;
                if (wph[0] > 1) wph[0] -= 1;
                if (wph[1] > 1) wph[1] -= 1;
                if (fph > 1) fph -= 1;
                const double ph0 = wph[0], ph1 = wph[1] + 0.27;   // right side a little out of phase: wide
                double mL = depthMs * (0.8 * std::sin (2 * kPi * ph0) + 0.5 * drift[0]) + flutMs * std::sin (2 * kPi * fph);
                double mR = depthMs * (0.8 * std::sin (2 * kPi * ph1) + 0.5 * drift[1]) + flutMs * std::sin (2 * kPi * (fph + 0.31));
                mL = std::max (-9.0, std::min (9.0, mL)); mR = std::max (-9.0, std::min (9.0, mR));
                dl[0][(size_t) wpos] = inL; dl[1][(size_t) wpos] = inR;
                wetL = readDelay (0, std::min (100.0, std::max (1.0, baseMs + mL + diveMs)));
                wetR = readDelay (1, std::min (100.0, std::max (1.0, baseMs + mR + diveMs)));
                wpos = (wpos + 1) & dmask;
            }

            // dry path, same latency
            float dryL, dryR;
            {
                const size_t sz = dryBuf[0].size();
                dryL = dryBuf[0][(size_t) dryPos]; dryR = dryBuf[1][(size_t) dryPos];
                dryBuf[0][(size_t) dryPos] = inL; dryBuf[1][(size_t) dryPos] = inR;
                dryPos = (int) ((dryPos + 1) % sz);
            }

            // ---- DIGITAL
            {
                const double a = std::min (1.0, aD + 0.9 * ev[1]);
                if (a > 0.005)
                {
                    const double bits = 16.0 - 6.5 * a;
                    const double srEff = sr * (1.0 - 0.75 * std::pow (a, 1.1));
                    const double levels = std::pow (2.0, bits - 1.0);
                    const float pre = coefHz (std::min (0.42 * srEff, 18000.0), sr);
                    const float post = coefHz (std::min (0.48 * srEff, 17000.0), sr);
                    const float w = (float) std::min (1.0, a * 5.0);
                    dctr += srEff / sr;
                    const float fl = pf[0].lp (pf[2].lp (wetL, pre), pre), fr = pf[1].lp (pf[3].lp (wetR, pre), pre);
                    if (dctr >= 1.0) { dctr -= 1.0; hold[0] = (float) (std::round (fl * levels) / levels); hold[1] = (float) (std::round (fr * levels) / levels); }
                    const float qL = qf[0].lp (hold[0], post), qR = qf[1].lp (hold[1], post);
                    wetL += w * (qL - wetL); wetR += w * (qR - wetR);
                }
            }

            // ---- DISTORT (warm tape-like saturation)
            if (aX > 0.005)
            {
                const double drive = 1.0 + aX * 7.0, bias = 0.15 * aX;
                const float comp = (float) (1.0 / std::pow (drive, 0.6));
                const float w = (float) std::min (1.0, aX * 4.0);
                const float lpA = coefHz (18000.0 - 12500.0 * aX, sr);
                float* s[2] = { &wetL, &wetR };
                for (int c = 0; c < 2; ++c)
                {
                    const float x = *s[c];
                    float y = (float) (std::tanh (drive * x + bias) - std::tanh (bias)) * comp;
                    y = tf[c].lp (y, lpA);
                    // remove the DC that the bias adds
                    dcZ[c] += 0.0008f * (y - dcZ[c]); y -= dcZ[c];
                    *s[c] = x + w * (y - x);
                }
            }

            // ---- TREMOLO
            if (aT > 0.005)
            {
                const double rate = (P.bpm / 60.0) / std::max (0.05, P.tremBeats);
                tph += rate * dt; if (tph > 1) tph -= 1;
                const float d = (float) aT;
                const float gl = 1.0f - d * (0.5f - 0.5f * (float) std::sin (2 * kPi * tph));
                const float gr = P.tremPan ? 1.0f - d * (0.5f - 0.5f * (float) std::sin (2 * kPi * (tph + 0.5))) : gl;
                wetL *= gl; wetR *= gr;
            }

            // ---- SPACE (small room)
            if (aS > 0.005)
            {
                const float fb = (float) (0.62 + 0.26 * M.spaceSize), damp = (float) (0.25 + 0.45 * (1.0 - M.spaceSize));
                const float inM = (wetL + wetR) * 0.5f * 0.3f;
                float oL = 0, oR = 0;
                for (int c = 0; c < 2; ++c)
                {
                    float acc = 0;
                    for (int k = 0; k < 4; ++k)
                    {
                        auto& cb = comb[c][k];
                        float y = cb.buf[(size_t) cb.pos];
                        cb.lp += damp * 0.0f; cb.lp = y * (1.0f - damp) + cb.lp * damp;
                        cb.buf[(size_t) cb.pos] = inM + cb.lp * fb;
                        if (++cb.pos >= (int) cb.buf.size()) cb.pos = 0;
                        acc += y;
                    }
                    for (int k = 0; k < 2; ++k)
                    {
                        auto& a2 = ap[c][k];
                        const float b = a2.buf[(size_t) a2.pos];
                        const float y = -acc + b;
                        a2.buf[(size_t) a2.pos] = acc + b * 0.5f;
                        if (++a2.pos >= (int) a2.buf.size()) a2.pos = 0;
                        acc = y;
                    }
                    (c == 0 ? oL : oR) = acc;
                }
                const float lvl = (float) (aS * 0.9);
                wetL += lvl * oL; wetR += lvl * oR;
            }

            // ---- MUFFLE event (low-pass closes and opens)
            if (ev[4] > 0.002f)
            {
                const double fc = 18000.0 * std::pow (1100.0 / 18000.0, (double) ev[4]);
                const float a = coefHz (fc, sr);
                wetL = mf[0].lp (wetL, a); wetR = mf[1].lp (wetR, a);
            }
            else { mf[0].z = wetL; mf[1].z = wetR; }

            // ---- DROPS: level dips, only while there is signal to dip
            {
                double gain = 1.0;
                if (present)
                {
                    nextDropSec -= dt;
                    if (nextDropSec <= 0 && dropSamplesLeft < 0 && aR > 0.005) startDrop ((float) aR, false);
                    if (hiccupLeft > 0) { hiccupWait -= dt; if (hiccupWait <= 0 && dropSamplesLeft < 0) { startDrop (0.9f, true); --hiccupLeft; hiccupWait = 0.05; } }
                }
                if (dropSamplesLeft >= 0)
                {
                    const double t = 1.0 - (double) dropSamplesLeft / (double) dropLen;
                    gain = 1.0 - dropDepth * (0.5 - 0.5 * std::cos (2 * kPi * t));
                    if (--dropSamplesLeft < 0) dropSamplesLeft = -1;
                }
                wetL *= (float) gain; wetR *= (float) gain;
            }

            // ---- mix
            const float m = (float) P.mix;
            L[i] = (float) ((dryL * (1.0f - m) + wetL * m) * outG);
            R[i] = (float) ((dryR * (1.0f - m) + wetR * m) * outG);

            // level history for the visualiser
            vizAcc += (double) (L[i] * L[i] + R[i] * R[i]) * 0.5; if (++vizN >= (int) (sr / 60)) { viz.push ((float) std::sqrt (vizAcc / vizN)); vizAcc = 0; vizN = 0; }
        }
        (void) smA;
    }

private:
    struct Comb { std::vector<float> buf; int pos = 0; float lp = 0; };
    struct AP { std::vector<float> buf; int pos = 0; };

    float readDelay (int c, double ms) const
    {
        const double d = ms * sr / 1000.0;
        const double rp = (double) wpos - d;
        const double fl = std::floor (rp);
        const float f = (float) (rp - fl);
        const int i1 = ((int) fl) & dmask, i0 = (i1 - 1) & dmask, i2 = (i1 + 1) & dmask, i3 = (i1 + 2) & dmask;
        const float y0 = dl[c][(size_t) i0], y1 = dl[c][(size_t) i1], y2 = dl[c][(size_t) i2], y3 = dl[c][(size_t) i3];
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    void startDrop (float depth, bool hiccup)
    {
        const double meanSec = 7.0 - 6.1 * (double) std::min (1.0f, depth);
        if (P.dropsFlux || hiccup)
        {
            dropLen = (int) (sr * (0.04 + 0.18 * rng.next()));
            dropDepth = std::min (0.92, (double) depth * (0.55 + 0.45 * rng.next()) + 0.1);
            if (! hiccup) nextDropSec = meanSec * (0.3 + 1.4 * rng.next());
        }
        else
        {
            dropLen = (int) (sr * 0.12);
            dropDepth = std::min (0.92, (double) depth * 0.85 + 0.08);
            nextDropSec = (60.0 / std::max (40.0, P.bpm)) * 2.0 * std::max (1.0, std::round (meanSec / ((60.0 / std::max (40.0, P.bpm)) * 2.0)));
        }
        dropSamplesLeft = dropLen;
    }

    void startEvent()
    {
        unsigned avail[5]; int na = 0;
        for (int k = 0; k < 5; ++k) if (P.chaos & (1u << k)) avail[na++] = (unsigned) k;
        if (! na) return;
        const unsigned k = avail[(int) (rng.next() * na) % na];
        switch (k)
        {
            case 0: diveT = 0; diveMax = 14.0 + 10.0 * rng.next(); break;
            case 1: evHold[1] = 0.25 + 0.3 * rng.next(); break;
            case 2: evHold[2] = 0.8 + 0.9 * rng.next(); break;
            case 3: hiccupLeft = 2; hiccupWait = 0.0; break;
            case 4: evHold[4] = 0.5 + 0.5 * rng.next(); break;
        }
        lastEvent.store ((int) k + 1);
        eventPulse.store (1.0f);
    }

    double sr = 48000.0;
    const double baseMs = 20.0;
    Params P;
    Rng rng;
    std::vector<float> dl[2]; int dmask = 0, wpos = 0;
    std::vector<float> dryBuf[2]; int dryPos = 0;
    double wph[2] = { 0, 0.3 }, fph = 0, tph = 0;
    float drift[2] = { 0, 0 }, driftTarget[2] = { 0, 0 }, fluxTarget[2] = { 0, 0 }, fluxA = 0, fluxB = 0;
    double driftT = 0;
    float env = 0;
    // digital
    OnePole pf[4], qf[2]; float hold[2] = { 0, 0 }; double dctr = 0;
    // distort
    OnePole tf[2]; float dcZ[2] = { 0, 0 };
    OnePole mf[2];
    // reverb
    Comb comb[2][4]; AP ap[2][2];
    // drops
    int dropSamplesLeft = -1, dropLen = 1; double dropDepth = 0.5, nextDropSec = 1.5;
    int hiccupLeft = 0; double hiccupWait = 0;
    // tipsy
    double evCountSec = 6.0; float ev[5] = { 0, 0, 0, 0, 0 }; double evHold[5] = { 0, 0, 0, 0, 0 };
    double diveT = -1, diveMax = 18.0; double diveRamp = 0;
    double vizAcc = 0; int vizN = 0;
};
} // namespace hangover
