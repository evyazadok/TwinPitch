// VoxMorph — monophonic voice pitch & formant shifter
// Header-only DSP engine (no JUCE dependency, so it can be tested offline).
//
// Technique: TD-PSOLA (pitch-synchronous overlap-add), the classic approach for
// real-time voice pitch shifting with independent formant control.
//
//  1. Pitch tracking   - YIN on a 2x-decimated copy of the input, every ~5 ms.
//  2. Pitch marks      - one analysis mark per glottal period, placed on the waveform
//                        peak of a low-passed copy, so every grain is aligned to the same
//                        point of the cycle.
//  3. Synthesis        - grains (two periods long, Hann-windowed, centred on a mark) are
//                        overlap-added at a NEW spacing. Closer spacing = higher pitch,
//                        wider = lower. The spectral envelope (formants) is untouched,
//                        because each grain keeps its original shape.
//  4. Formant shift    - each grain is resampled (squeezed or stretched in time) before
//                        it's added. That moves the formants without moving the pitch.
//  5. Unvoiced sounds  - consonants/breath are passed through grain-by-grain at their
//                        original timing (only the formant resampling applies).
//
// Modes: Transpose (pitch knob), Quantize (snap to nearest semitone = hard tune),
//        Robot (monotone at a chosen note).
// Link:  formant follows the pitch shift (chipmunk / monster).

#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include <deque>
#include <algorithm>
#include <atomic>

namespace voxmorph
{
static constexpr double kPi = 3.14159265358979323846;

//==============================================================================
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    void reset() { x1 = x2 = y1 = y2 = 0; }
    void lowPass (double sr, double f, double q = 0.7071)
    {
        f = std::clamp (f, 10.0, sr * 0.45);
        const double w = 2 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
        b0 = (1 - c) * 0.5 / a0; b1 = (1 - c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
    }
    void highPass (double sr, double f, double q = 0.7071)
    {
        f = std::clamp (f, 5.0, sr * 0.45);
        const double w = 2 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
        b0 = (1 + c) * 0.5 / a0; b1 = -(1 + c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
    }
    inline double process (double x)
    {
        const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y; return y;
    }
};

struct Smoother
{
    double c = 0, v = 0, t = 0;
    void prepare (double sr, double ms) { c = std::exp (-1.0 / (0.001 * ms * sr)); }
    void reset (double x) { v = t = x; }
    inline double next() { v = t + c * (v - t); return v; }
};

//==============================================================================
// YIN pitch detector working on a decimated signal
class PitchTracker
{
public:
    void prepare (double decimatedRate, double minHz, double maxHz)
    {
        sr = decimatedRate;
        tauMin = std::max (2, (int) std::floor (sr / maxHz));
        tauMax = (int) std::ceil (sr / minHz);
        W = (int) std::ceil (0.022 * sr);
        N = W + tauMax + 2;
        int size = 1; while (size < N * 2) size <<= 1;
        ring.assign ((size_t) size, 0.0f); mask = size - 1; pos = 0;
        frame.assign ((size_t) N, 0.0);
        d.assign ((size_t) tauMax + 2, 0.0);
    }
    inline void push (float x) { ring[(size_t) (pos & mask)] = x; ++pos; }

    // returns period in decimated samples, or 0 if unvoiced
    double analyze (double gateRms)
    {
        if (pos < N) return 0.0;
        double e = 0;
        for (int i = 0; i < N; ++i)
        {
            frame[(size_t) i] = ring[(size_t) ((pos - N + i) & mask)];
            e += frame[(size_t) i] * frame[(size_t) i];
        }
        if (std::sqrt (e / N) < gateRms) return 0.0;

        d[0] = 1.0;
        double running = 0.0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double s = 0.0;
            const double* a = frame.data();
            const double* b = frame.data() + tau;
            for (int j = 0; j < W; ++j) { const double df = a[j] - b[j]; s += df * df; }
            running += s;
            d[(size_t) tau] = running > 0 ? s * tau / running : 1.0;   // cumulative-mean normalised
        }

        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (d[(size_t) tau] < threshold)
            {
                while (tau + 1 < tauMax && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau;
                best = tau; break;
            }
        }
        if (best < 0) return 0.0;  // no clear periodicity -> unvoiced

        // parabolic interpolation
        double t = best;
        if (best > 1 && best < tauMax)
        {
            const double y0 = d[(size_t) best - 1], y1 = d[(size_t) best], y2 = d[(size_t) best + 1];
            const double den = y0 - 2 * y1 + y2;
            if (std::abs (den) > 1e-12) t += std::clamp (0.5 * (y0 - y2) / den, -1.0, 1.0);
        }
        return t;
    }

    double threshold = 0.15;

private:
    double sr = 24000;
    int tauMin = 24, tauMax = 300, W = 512, N = 800, mask = 0;
    int64_t pos = 0;
    std::vector<float> ring;
    std::vector<double> frame, d;
};

//==============================================================================
enum Mode { Transpose = 0, Quantize = 1, Robot = 2 };

struct Params
{
    int    mode      = Transpose;
    double pitch     = 0.0;   // semitones, -12..+12
    double formant   = 0.0;   // semitones, -12..+12
    bool   link      = false;
    int    robotNote = 48;    // MIDI note (C3)
    double drive     = 0.0;   // 0..1
    double mix       = 1.0;   // 0..1
    double outputDb  = 0.0;
};

//==============================================================================
class Engine
{
public:
    static constexpr double kMinHz = 80.0, kMaxHz = 1000.0;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        Pmax  = sr / kMinHz;
        Pu    = std::round (sr / 200.0);                    // grain spacing for unvoiced sounds
        lead  = (int) std::ceil (2.4 * Pmax) + 2;           // how far ahead grains are scheduled
        latency = lead + (int) std::ceil (1.95 * Pmax) + 4; // total delay through the effect

        int size = 1; while (size < 4 * latency + 8192) size <<= 1;
        inBuf.assign ((size_t) size, 0.0f);
        lpBuf.assign ((size_t) size, 0.0f);
        acc.assign ((size_t) size, 0.0f);
        mask = size - 1;
        dryL.assign ((size_t) size, 0.0f); dryR.assign ((size_t) size, 0.0f);

        tracker.prepare (sr * 0.5, kMinHz, kMaxHz);
        hop = std::max (64, (int) std::round (sr * 0.005));
        decLP.lowPass (sr, sr * 0.2);
        markLP.lowPass (sr, 1000.0, 0.6);
        dcBlock.highPass (sr, 15.0);
        tone.lowPass (sr, 16000.0);

        for (auto* sm : { &driveSm, &mixSm, &outSm }) sm->prepare (sr, 40.0);
        primed = false;
        reset();
    }

    void reset()
    {
        std::fill (inBuf.begin(), inBuf.end(), 0.0f);
        std::fill (lpBuf.begin(), lpBuf.end(), 0.0f);
        std::fill (acc.begin(), acc.end(), 0.0f);
        std::fill (dryL.begin(), dryL.end(), 0.0f);
        std::fill (dryR.begin(), dryR.end(), 0.0f);
        marks.clear();
        n = 0; lastMark = 0; hopCount = 0; curP = 0;
        s = (double) latency;     // first grain maps to input time 0
        quantNote = -1000;
        decLP.reset(); markLP.reset(); dcBlock.reset(); tone.reset();
    }

    int getLatencySamples() const { return latency; }
    void setParams (const Params& p) { params = p; }

    // UI readouts
    std::atomic<float> inputHz { 0.0f }, outputHz { 0.0f };

    // in-place; for mono pass the same pointer twice
    void process (float* left, float* right, int numSamples)
    {
        const double tDrive = std::clamp (params.drive, 0.0, 1.0);
        const double tMix   = std::clamp (params.mix, 0.0, 1.0);
        const double tOut   = std::pow (10.0, params.outputDb / 20.0);
        if (! primed) { driveSm.reset (tDrive); mixSm.reset (tMix); outSm.reset (tOut); primed = true; }
        driveSm.t = tDrive; mixSm.t = tMix; outSm.t = tOut;

        for (int i = 0; i < numSamples; ++i)
        {
            const float l = left[i], r = right[i];
            const float x = (left == right) ? l : 0.5f * (l + r);
            const size_t idx = (size_t) (n & mask);

            inBuf[idx] = x;
            lpBuf[idx] = (float) markLP.process (x);
            dryL[idx] = l; dryR[idx] = r;

            const double dec = decLP.process (x);
            if (n & 1) tracker.push ((float) dec);
            if (++hopCount >= hop)
            {
                hopCount = 0;
                const double pd = tracker.analyze (0.003);
                curP = pd > 0 ? 2.0 * pd : 0.0;
                inputHz.store (curP > 0 ? (float) (sr / curP) : 0.0f);
            }

            generateMarks();
            while (s <= (double) (n + lead)) placeGrain();

            // read wet output for this sample
            double wet = acc[idx]; acc[idx] = 0.0f;

            // drive
            const double dr = driveSm.next();
            if (dr > 1e-4)
            {
                const double g = std::pow (10.0, dr * 30.0 / 20.0);
                const double bias = 0.15 * dr;
                const double y = std::tanh (g * wet + bias) - std::tanh (bias);
                wet = (1.0 - dr) * wet + dr * (y / std::pow (g, 0.72));
                wet = tone.process (dcBlock.process (wet));
            }

            const size_t didx = (size_t) ((n - latency) & mask);
            const double m = mixSm.next(), og = outSm.next();
            const double dL = n >= latency ? dryL[didx] : 0.0;
            const double dR = n >= latency ? dryR[didx] : 0.0;
            left[i]  = (float) (((1.0 - m) * dL + m * wet) * og);
            if (right != left) right[i] = (float) (((1.0 - m) * dR + m * wet) * og);

            ++n;
        }

        // drop marks that can no longer be used
        const double oldest = s - latency - 4.0 * Pmax;
        while (marks.size() > 2 && (double) marks[1].pos < oldest) marks.pop_front();
    }

private:
    struct Mark { int64_t pos; double period; bool voiced; };

    void generateMarks()
    {
        if (curP > 0)
        {
            const double P = curP;
            const int64_t span = (int64_t) std::ceil (1.25 * P);
            if (n - lastMark > (int64_t) (6 * Pmax)) lastMark = n - (int64_t) (2 * P);
            while (lastMark + span < n)
            {
                const int64_t lo = lastMark + (int64_t) std::floor (0.75 * P);
                const int64_t hi = lastMark + span;
                int64_t best = lo; float bv = -1e30f;
                for (int64_t k = lo; k <= hi; ++k)
                {
                    const float v = lpBuf[(size_t) (k & mask)];
                    if (v > bv) { bv = v; best = k; }
                }
                marks.push_back ({ best, P, true });
                lastMark = best;
            }
        }
        else
        {
            while (lastMark + (int64_t) Pu < n)
            {
                lastMark += (int64_t) Pu;
                marks.push_back ({ lastMark, Pu, false });
            }
        }
    }

    // nearest mark to input time a (marks are sorted by position)
    int nearestMark (double a) const
    {
        if (marks.empty()) return -1;
        int lo = 0, hi = (int) marks.size() - 1;
        while (lo < hi)
        {
            const int mid = (lo + hi) / 2;
            if ((double) marks[(size_t) mid].pos < a) lo = mid + 1; else hi = mid;
        }
        if (lo > 0 && std::abs ((double) marks[(size_t) lo - 1].pos - a) < std::abs ((double) marks[(size_t) lo].pos - a))
            --lo;
        return lo;
    }

    static double midiToHz (double m) { return 440.0 * std::exp2 ((m - 69.0) / 12.0); }
    static double hzToMidi (double f) { return 69.0 + 12.0 * std::log2 (f / 440.0); }

    void placeGrain()
    {
        const double a = s - latency;          // input time this output moment maps to
        const int mi = nearestMark (a);

        bool voiced = false;
        double centre = a, hInL = Pu, hInR = Pu, Ps = Pu, shiftSemis = 0.0;

        if (mi >= 0)
        {
            const Mark& m = marks[(size_t) mi];
            if (m.voiced && std::abs ((double) m.pos - a) <= 0.75 * m.period)
            {
                voiced = true;
                const double P = m.period;
                centre = (double) m.pos;
                auto clampP = [P] (double v) { return std::clamp (v, 0.6 * P, 1.2 * P); };
                hInL = (mi > 0 && marks[(size_t) mi - 1].voiced) ? clampP ((double) (m.pos - marks[(size_t) mi - 1].pos)) : P;
                hInR = ((size_t) mi + 1 < marks.size() && marks[(size_t) mi + 1].voiced) ? clampP ((double) (marks[(size_t) mi + 1].pos - m.pos)) : P;

                const double f0 = sr / P;
                double target;
                switch (params.mode)
                {
                    case Robot:
                        target = midiToHz ((double) params.robotNote);
                        break;
                    case Quantize:
                    {
                        const double want = hzToMidi (f0) + params.pitch;
                        if (std::abs (want - quantNote) > 0.62) quantNote = std::round (want);  // hysteresis
                        target = midiToHz (quantNote);
                        break;
                    }
                    default:
                        target = 0.0;
                }

                if (params.mode == Transpose)
                {
                    const double ratio = std::exp2 (params.pitch / 12.0);
                    Ps = hInR / ratio;
                    shiftSemis = params.pitch;
                    outputHz.store ((float) (f0 * ratio));
                }
                else
                {
                    target = std::clamp (target, 40.0, 2000.0);
                    Ps = sr / target;
                    shiftSemis = 12.0 * std::log2 (target / f0);
                    outputHz.store ((float) target);
                }
            }
        }
        if (! voiced)
        {
            shiftSemis = params.mode == Transpose ? params.pitch : 0.0;
            outputHz.store (0.0f);
        }

        const double fSemis = params.formant + (params.link ? shiftSemis : 0.0);
        const double f = std::clamp (std::exp2 (fSemis / 12.0), 0.5, 2.0);

        double hOutL = std::min (hInL / f, (double) lead - 2.0);
        double hOutR = std::min (hInR / f, (double) lead - 2.0);
        if (! voiced) Ps = hOutR;   // keep 50% overlap so noise stays smooth

        // At unity pitch, keep the wet signal sample-aligned with the dry one (so Mix
        // doesn't comb-filter when only the formant is shifted): place the grain exactly
        // where its mark maps to, as long as that's still ahead of the output pointer.
        if (voiced && params.mode == Transpose && std::abs (params.pitch) < 1.0e-3)
        {
            const double snapped = centre + latency;
            if (snapped - hOutL > (double) n + 1.0) s = snapped;
        }

        const int64_t t0 = (int64_t) std::ceil (s - hOutL);
        const int64_t t1 = (int64_t) std::floor (s + hOutR);
        for (int64_t t = t0; t <= t1; ++t)
        {
            const double k = (double) t - s;
            const double h = k < 0 ? hOutL : hOutR;
            const double w = 0.5 + 0.5 * std::cos (kPi * k / h);
            acc[(size_t) (t & mask)] += (float) (w * readIn (centre + k * f));
        }

        s += std::max (Ps, 8.0);
    }

    inline double readIn (double p) const
    {
        const double fl = std::floor (p);
        const double fr = p - fl;
        const int64_t i = (int64_t) fl;
        const double xm1 = inBuf[(size_t) ((i - 1) & mask)], x0 = inBuf[(size_t) (i & mask)];
        const double x1 = inBuf[(size_t) ((i + 1) & mask)], x2 = inBuf[(size_t) ((i + 2) & mask)];
        const double c1 = 0.5 * (x1 - xm1);
        const double c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2;
        const double c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);
        return ((c3 * fr + c2) * fr + c1) * fr + x0;
    }

    double sr = 48000, Pmax = 600, Pu = 240;
    int lead = 1442, latency = 2616, hop = 240, hopCount = 0;
    int64_t mask = 0, n = 0, lastMark = 0;
    double s = 0, curP = 0, quantNote = -1000;

    std::vector<float> inBuf, lpBuf, acc, dryL, dryR;
    std::deque<Mark> marks;
    PitchTracker tracker;
    Biquad decLP, markLP, dcBlock, tone;
    Smoother driveSm, mixSm, outSm;
    Params params;
    bool primed = false;
};

} // namespace voxmorph
