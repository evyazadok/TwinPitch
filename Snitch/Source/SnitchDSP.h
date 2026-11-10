#pragma once
// SNITCH engine: finds the key and scale of whatever is playing.
//
// Audio thread (cheap):  mono sum -> low-pass -> resample to 11.025 kHz -> lock-free ring buffer. Nothing else.
// Analysis thread:       every 93 ms, one 8192-point FFT (0.74 s window) and a handful of small loops.
//
// What makes the answer better than a plain "chroma + key profile" detector:
//   1. tuning:     the song's tuning offset (samples pitched up or down, A=432 and so on) is measured from where the
//                  spectral peaks sit, and every note is assigned after correcting for it.
//   2. harmonics:  each strong note's overtones are subtracted before counting, so a G does not also count for D and B.
//   3. drums:      only pitch that is still there a moment later counts (kicks, snares and hats do not hold a pitch).
//   4. bass:       the lowest strong note is tracked on its own (in hip-hop the 808 is the home note) and decides
//                  between a major key and its relative minor.
//   5. memory:     evidence builds up with a slow decay (the MEMORY knob); a new key must win for several seconds
//                  before the answer changes, so a passing note never flips it.
//   6. honesty:    the confidence is the gap to the best answer that has DIFFERENT notes (A minor vs C major are the
//                  same notes, so that is not a disagreement), and the second choice is reported.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstring>
#include <thread>
#include <vector>

namespace snitch
{
static constexpr double kPi = 3.14159265358979323846;
static constexpr double kRate = 11025.0;
static constexpr int kN = 8192, kHop = 1024;
static constexpr double kHopSec = (double) kHop / kRate;

inline const char* noteName (int pc) { static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return n[((pc % 12) + 12) % 12]; }

struct ScaleDef { const char* name; int count; int notes[7]; int tier; int parentUp; };   // parentUp: semitones from the root up to the major scale with the same notes, -1 if none
inline const ScaleDef* scaleDefs (int& n)
{
    static const ScaleDef d[] = {
        { "MAJOR",       7, { 0, 2, 4, 5, 7, 9, 11 }, 0, 0 },
        { "MINOR",       7, { 0, 2, 3, 5, 7, 8, 10 }, 0, 3 },
        { "DORIAN",      7, { 0, 2, 3, 5, 7, 9, 10 }, 1, 10 },
        { "PHRYGIAN",    7, { 0, 1, 3, 5, 7, 8, 10 }, 1, 8 },
        { "LYDIAN",      7, { 0, 2, 4, 6, 7, 9, 11 }, 1, 7 },
        { "MIXOLYDIAN",  7, { 0, 2, 4, 5, 7, 9, 10 }, 1, 5 },
        { "HARM MINOR",  7, { 0, 2, 3, 5, 7, 8, 11 }, 2, -1 },
        { "MELODIC MIN", 7, { 0, 2, 3, 5, 7, 9, 11 }, 2, -1 },
        { "LOCRIAN",     7, { 0, 1, 3, 5, 6, 8, 10 }, 2, 1 },
        { "PENTA MAJOR", 5, { 0, 2, 4, 7, 9, 0, 0 },  2, -1 },
        { "PENTA MINOR", 5, { 0, 3, 5, 7, 10, 0, 0 }, 2, -1 },
        { "BLUES",       6, { 0, 3, 5, 6, 7, 10, 0 }, 2, -1 } };
    n = (int) (sizeof (d) / sizeof (d[0])); return d;
}

inline unsigned scaleMask (int root, int scaleIdx)
{
    int n; const ScaleDef& s = scaleDefs (n)[scaleIdx]; unsigned m = 0;
    for (int i = 0; i < s.count; ++i) m |= 1u << ((root + s.notes[i]) % 12);
    return m;
}

class FFTN
{
public:
    FFTN()
    {
        tw.resize ((size_t) kN / 2); rev.resize ((size_t) kN);
        for (int i = 0; i < kN / 2; ++i) tw[(size_t) i] = std::polar (1.0f, (float) (-2.0 * kPi * i / kN));
        int bits = 0; while ((1 << bits) < kN) ++bits;
        for (int i = 0; i < kN; ++i) { int r = 0; for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b); rev[(size_t) i] = r; }
    }
    void run (std::complex<float>* a) const
    {
        for (int i = 0; i < kN; ++i) if (rev[(size_t) i] > i) std::swap (a[i], a[rev[(size_t) i]]);
        for (int len = 2; len <= kN; len <<= 1)
        {
            const int half = len / 2, step = kN / len;
            for (int i = 0; i < kN; i += len)
                for (int j = 0; j < half; ++j)
                {
                    const std::complex<float> u = a[i + j], v = a[i + j + half] * tw[(size_t) (j * step)];
                    a[i + j] = u + v; a[i + j + half] = u - v;
                }
        }
    }
private:
    std::vector<std::complex<float>> tw; std::vector<int> rev;
};

class Engine
{
public:
    // ---- what the UI reads (all lock-free) ----
    std::atomic<int>   state { 0 };            // 0 nothing heard yet, 1 listening / collecting, 2 has an answer
    std::atomic<int>   root { -1 }, scaleIdx { 0 }, confPct { 0 };
    std::atomic<int>   root2 { -1 }, scaleIdx2 { 0 };
    std::atomic<float> tuningCents { 0.0f };
    std::atomic<float> shown[12];              // WHAT IT HEARD: instant attack, slow release (about 4 s)
    std::atomic<unsigned> notesMask { 0 };
    std::atomic<int>   changeCounter { 0 };    // bumps every time the answer changes

    Engine() { for (auto& s : shown) s.store (0.0f); hann.resize (kN); for (int i = 0; i < kN; ++i) hann[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * kPi * i / kN)); reset(); }

    void prepare (double hostRate)
    {
        rate = hostRate; ratio = hostRate / kRate;
        const double fc = std::min (4500.0, 0.45 * hostRate), w = std::tan (kPi * fc / hostRate);
        for (int s = 0; s < 2; ++s)
        {
            const double q = s ? 1.3066 : 0.5412, n = 1.0 / (1.0 + w / q + w * w);
            b0[s] = w * w * n; b1[s] = 2 * b0[s]; b2[s] = b0[s]; a1[s] = 2.0 * (w * w - 1.0) * n; a2[s] = (1.0 - w / q + w * w) * n;
        }
        reset();
    }
    void reset()
    {
        std::memset (z1, 0, sizeof z1); std::memset (z2, 0, sizeof z2); std::memset (h4, 0, sizeof h4); ph = 0.0;
        rdIdx = wrIdx = 0; frame.assign ((size_t) kN, 0.0f); fresh = 0;
        acc.fill (0.0); bassAcc.fill (0.0); shortAcc.fill (0.0); dispF.fill (0.0); prevFine.fill (0.0f);
        tuneAcc = { 1.0, 0.0 }; tuneOffset = 0.0; hops = 0; silentHops = 0; cur = -1; curScale = 0; candRoot = -1; dwell = 0;
        for (auto& s : shown) s.store (0.0f);
        state.store (0); root.store (-1); confPct.store (0); root2.store (-1); notesMask.store (0); tuningCents.store (0.0f);
    }

    // knobs (any thread)
    void setOptions (double memory01, int tier, bool lock) { memory.store ((float) memory01); maxTier.store (tier); locked.store (lock); }

    const std::array<double, 12>& debugAcc() const { return acc; }
    const std::array<double, 12>& debugBass() const { return bassAcc; }
    std::vector<std::pair<unsigned, double>> dbgGroups;

    // ---- audio thread ----
    void feed (const float* mono, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            double v = mono[i];
            for (int s = 0; s < 2; ++s) { const double y = b0[s] * v + z1[s]; z1[s] = b1[s] * v - a1[s] * y + z2[s]; z2[s] = b2[s] * v - a2[s] * y; v = y; }
            h4[0] = h4[1]; h4[1] = h4[2]; h4[2] = h4[3]; h4[3] = (float) v;
            ph += 1.0;
            while (ph >= ratio)
            {
                ph -= ratio;
                const double f = 1.0 - ph;                                              // between h4[1] and h4[2]
                const double c0 = -f * (f - 1) * (f - 2) / 6.0, c1 = (f + 1) * (f - 1) * (f - 2) / 2.0, c2 = -(f + 1) * f * (f - 2) / 2.0, c3 = (f + 1) * f * (f - 1) / 6.0;
                const float y = (float) (c0 * h4[0] + c1 * h4[1] + c2 * h4[2] + c3 * h4[3]);
                const size_t w = wrIdx.load (std::memory_order_relaxed);
                ring[w & (kRing - 1)] = y; wrIdx.store (w + 1, std::memory_order_release);
            }
        }
    }

    // ---- analysis thread (or called directly in tests) ----
    bool pump()
    {
        bool did = false;
        while (wrIdx.load (std::memory_order_acquire) - rdIdx >= (size_t) kHop)
        {
            if (wrIdx.load (std::memory_order_acquire) - rdIdx > (size_t) (kRing - 2 * kHop)) rdIdx = wrIdx.load() - (size_t) kHop;   // fell far behind: skip ahead
            std::memmove (frame.data(), frame.data() + kHop, sizeof (float) * (size_t) (kN - kHop));
            for (int i = 0; i < kHop; ++i) frame[(size_t) (kN - kHop + i)] = ring[(rdIdx + (size_t) i) & (kRing - 1)];
            rdIdx += (size_t) kHop;
            if (fresh < kN) { fresh += kHop; }
            if (! locked.load()) analyse();
            did = true;
        }
        return did;
    }

private:
    struct Peak { double f, amp, res, midi, sharp; };

    void analyse()
    {
        // -- is there anything to hear?
        double e = 0.0; for (int i = kN - kHop; i < kN; ++i) e += (double) frame[(size_t) i] * frame[(size_t) i];
        const double rms = std::sqrt (e / kHop);
        if (rms < 3.0e-4) { if (++silentHops > 30 && state.load() == 1) state.store (0); decayDisplay (0.0); return; }
        silentHops = 0;
        if (fresh < kN) { return; }                                                    // wait until the window is full of real signal

        // -- spectrum
        static thread_local std::vector<std::complex<float>> a; a.assign ((size_t) kN, {});
        for (int i = 0; i < kN; ++i) a[(size_t) i] = frame[(size_t) i] * hann[(size_t) i];
        fft.run (a.data());
        const double binHz = kRate / kN;
        const int kLo = (int) std::ceil (38.0 / binHz), kHi = std::min (kN / 2 - 2, (int) (2000.0 / binHz));
        static thread_local std::vector<float> mag; mag.resize ((size_t) kN / 2 + 1);
        float maxA = 0.0f; for (int k = 0; k <= kN / 2; ++k) { mag[(size_t) k] = std::abs (a[(size_t) k]); if (k >= kLo && k <= kHi) maxA = std::max (maxA, mag[(size_t) k]); }
        if (maxA <= 0.0f) return;

        // -- peaks
        std::vector<Peak> pk; const float thr = maxA * 0.035f;
        for (int k = std::max (2, kLo); k <= kHi; ++k)
        {
            const float m = mag[(size_t) k];
            if (m > thr && m > mag[(size_t) k - 1] && m >= mag[(size_t) k + 1])
            {
                const double al = std::log ((double) mag[(size_t) k - 1] + 1e-12), be = std::log ((double) m + 1e-12), ga = std::log ((double) mag[(size_t) k + 1] + 1e-12);
                const double den = al - 2.0 * be + ga, p = den < -1e-9 ? 0.5 * (al - ga) / den : 0.0;
                Peak q; q.f = (k + p) * binHz; q.amp = std::exp (be - 0.25 * (al - ga) * p); q.res = q.amp; q.midi = 69.0 + 12.0 * std::log2 (q.f / 440.0);
                // a held note is a narrow peak; a kick or a snare is smeared over many bins
                q.sharp = std::clamp (1.0 - 1.3 * (double) std::max (mag[(size_t) k - 2], mag[(size_t) k + 2]) / (double) m, 0.15, 1.0);
                pk.push_back (q);
            }
        }
        if (pk.empty()) return;

        // -- 1. tuning offset (circular mean of how far peaks sit from the nearest semitone)
        {
            std::complex<double> v (0, 0); double sw = 0.0;
            for (auto& p : pk) if (p.f >= 100.0 && p.f <= 1600.0) { const double dev = p.midi - std::round (p.midi); v += p.amp * std::polar (1.0, 2.0 * kPi * dev); sw += p.amp; }
            if (sw > 0.0) { v /= sw; tuneAcc = tuneAcc * 0.97 + v * 0.03; }
            tuneOffset = std::arg (tuneAcc) / (2.0 * kPi);
            tuningCents.store ((float) (tuneOffset * 100.0));
        }
        for (auto& p : pk) p.midi -= tuneOffset;

        // -- 2. overtones: strong notes subtract the harmonics they would produce
        for (size_t i = 0; i < pk.size(); ++i)
        {
            if (pk[i].res < 0.2 * maxA) continue;
            for (int h = 2; h <= 6; ++h)
            {
                const double ef = pk[i].f * h; double best = 1e9; size_t bj = 0;
                for (size_t j = i + 1; j < pk.size(); ++j) { const double d = std::abs (pk[j].f - ef) / ef; if (d < best) { best = d; bj = j; } if (pk[j].f > ef * 1.05) break; }
                if (best < 0.03) pk[bj].res = std::max (0.0, pk[bj].res - pk[i].amp * std::pow (0.7, h - 1));
            }
        }

        // -- 3. only pitch that is still there counts (drums do not hold a pitch)
        static constexpr int kFine = 190;                                               // 3 bins per semitone from MIDI 36 up
        std::array<float, kFine> cur_f; cur_f.fill (0.0f);
        for (auto& p : pk)
        {
            const double w = std::pow (std::max (0.0, p.res) / maxA, 0.6) * p.sharp;
            if (w <= 0.0) continue;
            if (p.f < 110.0) continue;                                                        // below this is the bass's job
            const int idx = (int) std::lround (3.0 * (p.midi - 36.0));
            if (idx >= 0 && idx < kFine) cur_f[(size_t) idx] += (float) w;
        }
        std::array<double, 12> inst; inst.fill (0.0);
        for (int i = 0; i < kFine; ++i)
        {
            const float pv = std::max ({ i > 0 ? prevFine[(size_t) i - 1] : 0.0f, prevFine[(size_t) i], i < kFine - 1 ? prevFine[(size_t) i + 1] : 0.0f });
            const float st = std::min (cur_f[(size_t) i], pv);
            inst[(size_t) ((36 + (i + 1) / 3) % 12)] += st;
        }
        prevFine = cur_f;
        // the bass: subharmonic summation (the 808's fundamental can sit below what the FFT resolves, or be weak next to
        // its third harmonic, so each candidate note is scored by all of its harmonics together)
        int bassPc = -1; double bassWeight = 0.0;
        {
            double best = 0.0, bestMidi = 0.0, lowMax = 0.0;
            for (int k = (int) (20.0 / binHz); k <= (int) (200.0 / binHz); ++k) lowMax = std::max (lowMax, (double) mag[(size_t) k]);
            if (lowMax > 0.2 * (double) maxA)
            for (int c = 0; c <= 3 * 31; ++c)
            {
                const double midi = 24.0 + c / 3.0, f0 = 440.0 * std::pow (2.0, (midi + tuneOffset - 69.0) / 12.0);
                double sc = 0.0;
                for (int h = 1; h <= 6; ++h)
                {
                    const int k = (int) std::lround (f0 * h / binHz); if (k < 2 || k >= kN / 2 - 1) break;
                    sc += std::pow (0.85, h - 1) * std::max ({ mag[(size_t) k - 1], mag[(size_t) k], mag[(size_t) k + 1] });
                }
                if (sc > best) { best = sc; bestMidi = midi; }
            }
            // a bass note is a narrow line; a kick drum smears across the whole low end, so it should not count
            double lowSum = 0.0, peak3 = 0.0;
            for (int k = (int) (20.0 / binHz); k <= (int) (200.0 / binHz); ++k)
            { const double e2 = (double) mag[(size_t) k] * mag[(size_t) k]; lowSum += e2; peak3 = std::max (peak3, e2 + (double) mag[(size_t) k - 1] * mag[(size_t) k - 1] + (double) mag[(size_t) k + 1] * mag[(size_t) k + 1]); }
            bassWeight = lowSum > 0.0 ? std::clamp ((peak3 / lowSum - 0.25) / 0.35, 0.0, 1.0) : 0.0;
            if (best > 0.35 * (double) maxA && bassWeight > 0.0) bassPc = ((int) std::lround (bestMidi) % 12 + 12) % 12;
        }
        double sumI = 0.0; for (double v : inst) sumI += v;
        if (sumI < 1e-6) return;
        for (double& v : inst) v /= sumI;

        // -- 4. accumulate with a slow decay (MEMORY)
        const double m = memory.load(), tau = 3.0 * std::exp (m * std::log (30.0));    // 3 s .. 90 s
        const double df = std::exp (-kHopSec / tau);
        for (int k = 0; k < 12; ++k) acc[(size_t) k] = acc[(size_t) k] * df + inst[(size_t) k];
        if (bassPc >= 0) { for (auto& v : bassAcc) v *= df; bassAcc[(size_t) bassPc] += bassWeight; }
        else for (auto& v : bassAcc) v *= df;
        ++hops;

        // -- 5. WHAT IT HEARD: instant attack, slow release
        decayDisplay (0.0);
        for (int k = 0; k < 12; ++k) shortAcc[(size_t) k] = shortAcc[(size_t) k] * std::exp (-kHopSec / 0.9) + inst[(size_t) k];
        double mx = 0.0; for (double v : shortAcc) mx = std::max (mx, v);
        if (mx > 0.0) for (int k = 0; k < 12; ++k) { const double t = shortAcc[(size_t) k] / mx; dispF[(size_t) k] = std::max (dispF[(size_t) k], t); shown[k].store ((float) dispF[(size_t) k]); }

        decide (m);
    }

    void decayDisplay (double) { const double d = std::exp (-kHopSec / 4.0); for (int k = 0; k < 12; ++k) { dispF[(size_t) k] *= d; shown[k].store ((float) dispF[(size_t) k]); } }

    struct Cand { int root, scale; double score; unsigned mask; double fit; double bass; };

    void decide (double m)
    {
        int nS; const ScaleDef* defs = scaleDefs (nS); const int tier = maxTier.load();
        double sum = 0.0; for (double v : acc) sum += v; if (sum <= 0.0) return;
        std::array<double, 12> x; double mean = 0.0; for (int k = 0; k < 12; ++k) { x[(size_t) k] = acc[(size_t) k] / sum; mean += x[(size_t) k] / 12.0; }
        double bsum = 0.0; for (double v : bassAcc) bsum += v;
        std::vector<Cand> cs; cs.reserve (12 * (size_t) nS);
        double vx = 0.0; for (double v : x) vx += (v - mean) * (v - mean);
        for (int s = 0; s < nS; ++s)
        {
            if (defs[s].tier > tier) continue;
            for (int r = 0; r < 12; ++r)
            {
                std::array<double, 12> t; t.fill (0.0);
                for (int i = 0; i < defs[s].count; ++i)
                {
                    const int d = defs[s].notes[i]; double w = 0.45;
                    if (d == 0) w = 1.0; else if (d == 7) w = 0.75; else if (d == 3 || d == 4) w = 0.7;
                    t[(size_t) ((r + d) % 12)] = w;
                }
                double tm = 0.0; for (double v : t) tm += v / 12.0;
                double cov = 0.0, vt = 0.0; for (int k = 0; k < 12; ++k) { cov += (x[(size_t) k] - mean) * (t[(size_t) k] - tm); vt += (t[(size_t) k] - tm) * (t[(size_t) k] - tm); }
                const double corr = cov / (std::sqrt (vx * vt) + 1e-12);
                double sc = corr + (defs[s].tier == 0 ? 0.02 : (defs[s].tier == 1 ? 0.0 : -0.03));    // common scales first
                cs.push_back ({ r, s, sc, scaleMask (r, s), corr, bsum > 0.0 ? bassAcc[(size_t) r] / bsum - 1.0 / 12.0 : 0.0 });
            }
        }
        if (cs.empty()) return;
        // the notes are decided by the chroma alone (best fit per set of notes); the bass only chooses the home note
        // among the answers that share those notes
        std::vector<std::pair<unsigned, double>> gb;                                    // set of notes -> best fit
        for (auto& c : cs) { bool f = false; for (auto& g : gb) if (g.first == c.mask) { g.second = std::max (g.second, c.score); f = true; } if (! f) gb.push_back ({ c.mask, c.score }); }
        auto groupScore = [&] (unsigned mk) { for (auto& g : gb) if (g.first == mk) return g.second; return -1e9; };
        std::sort (gb.begin(), gb.end(), [] (const auto& p, const auto& q) { return p.second > q.second; });
        dbgGroups = gb;
        const unsigned bestMask = gb[0].first; const double gap = gb.size() > 1 ? gb[0].second - gb[1].second : 0.2;
        const unsigned secondMask = gb.size() > 1 ? gb[1].first : 0;
        // among answers with the same notes, plain major / minor come first
        Cand best { -1, 0, -1e9, 0, 0.0, 0.0 }; const Cand* second = nullptr;
        for (auto& c : cs) { const double v = c.score + 0.6 * c.bass + (defs[c.scale].tier == 0 ? 0.08 : (defs[c.scale].tier == 1 ? 0.0 : -0.05));; if (c.mask == bestMask && v > best.score + 0.0 && (best.root < 0 || v > best.score)) { best = c; best.score = v; } }
        double sBest = -1e9; for (auto& c : cs) if (c.mask == secondMask && c.score > sBest) { sBest = c.score; second = &c; }
        double conf = std::clamp (gap / 0.12, 0.0, 1.0) * std::clamp ((best.fit - 0.45) / 0.35, 0.0, 1.0);
        if (hops < 30) conf *= (double) hops / 30.0;                                    // the first three seconds are a guess

        // -- 6. a new answer has to win for a while before it replaces the old one
        const int needHops = (int) ((2.0 + 10.0 * m) / kHopSec);
        if (cur < 0) { if (hops >= 30) { cur = best.root; curScale = best.scale; ++changeCounter; } }
        else if (best.root != cur || best.scale != curScale)
        {
            const unsigned curMask = scaleMask (cur, curScale);
            if (best.mask == curMask)                                                       // same notes: only the home note would move, and that needs a few seconds of agreement too
            {
                if (best.root == homeRoot && best.scale == homeScale) ++homeDwell; else { homeRoot = best.root; homeScale = best.scale; homeDwell = 1; }
                if (homeDwell >= (int) ((3.0 + 8.0 * m) / kHopSec)) { cur = best.root; curScale = best.scale; ++changeCounter; homeDwell = 0; }
                dwell = 0;
            }
            else if (groupScore (best.mask) - groupScore (curMask) > 0.012)
            {
                if (best.mask == candMask) ++dwell; else { candMask = best.mask; dwell = 1; }
                if (dwell >= needHops) { cur = best.root; curScale = best.scale; dwell = 0; ++changeCounter; }
            }
            else dwell = 0;
        }
        else { dwell = 0; homeDwell = 0; }

        if (cur >= 0)
        {
            root.store (cur); scaleIdx.store (curScale); notesMask.store (scaleMask (cur, curScale));
            if (second) { root2.store (second->root); scaleIdx2.store (second->scale); } else root2.store (-1);
            confPct.store ((int) std::lround (conf * 100.0)); state.store (2);
        }
        else state.store (1);
    }

    // ring buffer between the audio thread and the analysis thread
    static constexpr size_t kRing = 1u << 16;
    std::array<float, kRing> ring {};
    std::atomic<size_t> wrIdx { 0 }; size_t rdIdx = 0;

    double rate = 48000.0, ratio = 48000.0 / kRate, ph = 0.0;
    double b0[2] {}, b1[2] {}, b2[2] {}, a1[2] {}, a2[2] {}, z1[2] {}, z2[2] {}; float h4[4] {};

    std::atomic<float> memory { 0.5f }; std::atomic<int> maxTier { 1 }; std::atomic<bool> locked { false };
    FFTN fft; std::vector<float> hann, frame; int fresh = 0;
    std::array<double, 12> acc {}, bassAcc {}, shortAcc {}, dispF {};
    std::array<float, 190> prevFine {};
    std::complex<double> tuneAcc { 1.0, 0.0 }; double tuneOffset = 0.0;
    int hops = 0, silentHops = 0, cur = -1, curScale = 0, candRoot = -1, dwell = 0, homeRoot = -1, homeScale = 0, homeDwell = 0; unsigned candMask = 0;
};
}
