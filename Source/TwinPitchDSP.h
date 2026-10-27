// TwinPitch — stereo micro-pitch widener
// Header-only DSP engine, independent of JUCE (so it can be unit tested on its own).
//
// Principle (the same one used by the classic Eventide H3000 / AMS DMX "micro shift" patches):
//   * Each output side gets its own copy of the signal, pitch-shifted by a few cents.
//     Left goes one way (e.g. up), right goes the other way (down).
//   * Each side is also delayed by a slightly different short time (a few ms to ~30 ms).
//   * The opposing detune + unequal delays decorrelate L and R, so the ear hears width
//     and "doubling" without phasey, fixed comb-filter chorus wobble.
//   * The pitch shifter itself is a delay-line "rotating head" shifter: two read taps sweep
//     through a delay window at a rate set by the pitch ratio, 180 degrees apart, and are
//     crossfaded with complementary sin^2 / cos^2 windows so one tap is always silent while it jumps.
//   * Focus = high-pass on the effect signal only, so low end stays mono and tight.
//
// Three styles:
//   I   - smooth, wide, long splice window, static detune (H3000-like)
//   II  - tighter, grainier, more detune, slightly darker (DMX-like)
//   III - detune amount drifts slowly and independently per side (animated, chorus-like width)

#pragma once
#include <cmath>
#include <vector>
#include <cstdint>
#include <algorithm>

namespace twinpitch
{
static constexpr double kPi = 3.14159265358979323846;

//==============================================================================
// One-pole parameter smoother
struct Smoother
{
    double coeff = 0.0, value = 0.0, target = 0.0;
    void prepare (double sampleRate, double timeMs)
    {
        coeff = std::exp (-1.0 / (0.001 * timeMs * sampleRate));
    }
    void reset (double v) { value = target = v; }
    void setTarget (double t) { target = t; }
    inline double next() { value = target + coeff * (value - target); return value; }
};

//==============================================================================
// RBJ biquad (Direct Form I, double precision)
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    void reset() { x1 = x2 = y1 = y2 = 0; }

    void setHighPass (double sr, double freq, double q = 0.7071)
    {
        freq = std::clamp (freq, 5.0, sr * 0.45);
        const double w0 = 2.0 * kPi * freq / sr, cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (1.0 + cw) * 0.5 / a0; b1 = -(1.0 + cw) / a0; b2 = b0;
        a1 = -2.0 * cw / a0;        a2 = (1.0 - alpha) / a0;
    }
    void setLowPass (double sr, double freq, double q = 0.7071)
    {
        freq = std::clamp (freq, 20.0, sr * 0.45);
        const double w0 = 2.0 * kPi * freq / sr, cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (1.0 - cw) * 0.5 / a0; b1 = (1.0 - cw) / a0; b2 = b0;
        a1 = -2.0 * cw / a0;        a2 = (1.0 - alpha) / a0;
    }
    inline double process (double x)
    {
        const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
};

//==============================================================================
// Slow smoothed random LFO (for style III drift). Deterministic per seed.
struct DriftLFO
{
    uint32_t state = 1;
    double current = 0, from = 0, to = 0, phase = 1.0, inc = 0.0, sr = 48000.0;
    double minHz = 0.12, maxHz = 0.35;

    void prepare (double sampleRate, uint32_t seed) { sr = sampleRate; state = seed ? seed : 1; current = from = to = 0; phase = 1.0; }
    double rnd() { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return (state & 0xFFFFFF) / double (0xFFFFFF); }
    inline double next()
    {
        if (phase >= 1.0)
        {
            from = current; to = rnd() * 2.0 - 1.0; phase = 0.0;
            inc = (minHz + rnd() * (maxHz - minHz)) / sr;
        }
        phase += inc;
        const double s = 0.5 - 0.5 * std::cos (kPi * std::min (phase, 1.0)); // smooth segment
        current = from + (to - from) * s;
        return current; // -1..1
    }
};

//==============================================================================
// Delay-line pitch shifter with two crossfaded sweeping taps ("rotating heads")
class MicroShifter
{
public:
    void prepare (double sampleRate, int maxDelaySamples)
    {
        sr = sampleRate;
        int size = 1;
        while (size < maxDelaySamples + 8) size <<= 1;
        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writePos = 0;
        phase = 0.0;
    }
    void reset() { std::fill (buffer.begin(), buffer.end(), 0.0f); writePos = 0; phase = 0.0; }

    // ratio: pitch ratio (e.g. 1.0058 for +10 cents)
    // baseDelay, window: in samples
    inline double process (float in, double ratio, double baseDelay, double window)
    {
        buffer[(size_t) writePos] = in;

        // Phase moves by (1 - ratio) / window per sample: ratio > 1 shortens the delay -> pitch up.
        phase += (1.0 - ratio) / window;
        phase -= std::floor (phase);

        double p2 = phase + 0.5; if (p2 >= 1.0) p2 -= 1.0;

        const double d1 = baseDelay + phase * window;
        const double d2 = baseDelay + p2    * window;

        // complementary crossfade: sin^2 + cos^2 = 1
        const double s = std::sin (kPi * phase);
        const double g1 = s * s, g2 = 1.0 - g1;

        const double out = g1 * read (d1) + g2 * read (d2);
        writePos = (writePos + 1) & mask;
        return out;
    }

private:
    // 4-point Hermite interpolated read, `delay` samples behind the sample just written
    inline double read (double delay) const
    {
        const double rp = (double) writePos - delay;
        const double fl = std::floor (rp);
        const double f = rp - fl;
        const int i = (int) fl;
        const double xm1 = buffer[(size_t) ((i - 1) & mask)];
        const double x0  = buffer[(size_t) ((i    ) & mask)];
        const double x1  = buffer[(size_t) ((i + 1) & mask)];
        const double x2  = buffer[(size_t) ((i + 2) & mask)];
        const double c1 = 0.5 * (x1 - xm1);
        const double c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2;
        const double c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    std::vector<float> buffer;
    int mask = 0, writePos = 0;
    double phase = 0.0, sr = 48000.0;
};

//==============================================================================
struct StyleDef
{
    double maxCentsL, maxCentsR;     // detune at 100% (sign = direction)
    double delayRatioL, delayRatioR; // multiplier on the Delay knob time
    double delayOffsetMsL, delayOffsetMsR;
    double windowMs;                 // splice window length
    double lowPassHz;                // tone of the effect path (0 = off)
    double driftDepth;               // 0 = static, 1 = detune swings fully around its set value
};

inline const StyleDef& getStyle (int style)
{
    static const StyleDef styles[3] = {
        // I  : smooth & wide, long window, bright
        { +9.0,  -9.0,  1.00, 1.45, 0.0, 1.5, 48.0,     0.0, 0.0 },
        // II : more detune, short tight window (grainier), darker
        { +14.0, -12.0, 1.00, 1.80, 0.5, 0.0, 22.0, 11000.0, 0.0 },
        // III: animated, detune drifts independently on each side
        { +12.0, -12.0, 1.00, 1.30, 1.0, 2.0, 34.0, 14000.0, 0.85 },
    };
    return styles[std::clamp (style, 0, 2)];
}

//==============================================================================
struct Params
{
    int    style   = 0;     // 0,1,2
    double detune  = 0.5;   // 0..1
    double delayMs = 10.0;  // 0..40
    double focusHz = 20.0;  // 20..2000 high-pass on effect
    double mix     = 0.5;   // 0..1
    double outputDb = 0.0;  // -24..+12
};

//==============================================================================
class Engine
{
public:
    static constexpr double kMaxDelayMs = 40.0;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        // max base delay (with ratio and offset) + max window, with headroom
        const int maxSamples = (int) std::ceil ((kMaxDelayMs * 1.8 + 5.0 + 60.0) * 0.001 * sr) + 16;
        for (auto& s : shifters) s.prepare (sr, maxSamples);

        for (auto* sm : { &centsL, &centsR, &delayL, &delayR, &windowS, &dryGain, &wetGain, &outGain })
            sm->prepare (sr, 60.0);
        driftAmt.prepare (sr, 200.0);
        // these two are stepped once per 32 samples (control rate)
        lpCutoff.prepare (sr / 32.0, 60.0);
        focusSm.prepare (sr / 32.0, 60.0);

        driftL.prepare (sr, 0x1234567u);
        driftR.prepare (sr, 0x89abcdeu);
        reset();
        primed = false;
    }

    void reset()
    {
        for (auto& s : shifters) s.reset();
        for (auto& f : hp) f.reset();
        for (auto& f : hp2) f.reset();
        for (auto& f : lp) f.reset();
    }

    void setParams (const Params& p) { params = p; }

    // In-place stereo processing. For mono, pass the same pointer for left and right.
    void process (float* left, float* right, int numSamples)
    {
        const StyleDef& st = getStyle (params.style);
        const double det = std::clamp (params.detune, 0.0, 1.0);

        const double tCentsL = st.maxCentsL * det;
        const double tCentsR = st.maxCentsR * det;
        const double dms = std::clamp (params.delayMs, 0.0, kMaxDelayMs);
        const double tDelL = (dms * st.delayRatioL + st.delayOffsetMsL) * 0.001 * sr;
        const double tDelR = (dms * st.delayRatioR + st.delayOffsetMsR) * 0.001 * sr;
        const double tWin  = st.windowMs * 0.001 * sr;
        const double m = std::clamp (params.mix, 0.0, 1.0);
        const double tDry = std::min (1.0, 2.0 * (1.0 - m));
        const double tWet = std::min (1.0, 2.0 * m);
        const double tOut = std::pow (10.0, params.outputDb / 20.0);
        const double tLp  = st.lowPassHz > 0.0 ? st.lowPassHz : sr * 0.45;
        const double tFocus = std::clamp (params.focusHz, 20.0, 2000.0);

        centsL.setTarget (tCentsL); centsR.setTarget (tCentsR);
        delayL.setTarget (tDelL);   delayR.setTarget (tDelR);
        windowS.setTarget (tWin);
        dryGain.setTarget (tDry);   wetGain.setTarget (tWet); outGain.setTarget (tOut);
        lpCutoff.setTarget (tLp);   focusSm.setTarget (tFocus);
        driftDepth = st.driftDepth;

        if (! primed)
        {
            centsL.reset (tCentsL); centsR.reset (tCentsR);
            delayL.reset (tDelL);   delayR.reset (tDelR);
            windowS.reset (tWin);
            dryGain.reset (tDry);   wetGain.reset (tWet); outGain.reset (tOut);
            lpCutoff.reset (tLp);   focusSm.reset (tFocus);
            driftAmt.reset (driftDepth);
            updateFilters();
            primed = true;
        }
        driftAmt.setTarget (driftDepth);

        for (int i = 0; i < numSamples; ++i)
        {
            if ((i & 31) == 0) updateFilters(); // filter coefficients at control rate

            const float inL = left[i];
            const float inR = right[i];

            const double dA = driftAmt.next();
            double cL = centsL.next(), cR = centsR.next();
            if (dA > 1.0e-4)
            {
                // drift between 15% and 100% of the set detune (scaled by depth)
                const double mL = 0.575 + 0.425 * driftL.next();
                const double mR = 0.575 + 0.425 * driftR.next();
                cL *= (1.0 - dA) + dA * mL;
                cR *= (1.0 - dA) + dA * mR;
            }
            const double rL = std::exp2 (cL / 1200.0);
            const double rR = std::exp2 (cR / 1200.0);
            const double w  = windowS.next();

            double wl = shifters[0].process (inL, rL, delayL.next(), w);
            double wr = shifters[1].process (inR, rR, delayR.next(), w);

            // tone + focus on the effect only
            wl = hp2[0].process (hp[0].process (lp[0].process (wl)));
            wr = hp2[1].process (hp[1].process (lp[1].process (wr)));

            const double dg = dryGain.next(), wg = wetGain.next(), og = outGain.next();
            left[i]  = (float) ((inL * dg + wl * wg) * og);
            right[i] = (float) ((inR * dg + wr * wg) * og);
        }
    }

private:
    void updateFilters()
    {
        const double f = focusSm.next(); // step the smoother at control rate too (good enough)
        const double c = lpCutoff.next();
        for (int ch = 0; ch < 2; ++ch)
        {
            hp[ch].setHighPass (sr, f, 0.5412);  // 4th-order Butterworth (two sections)
            hp2[ch].setHighPass (sr, f, 1.3066);
            lp[ch].setLowPass (sr, c);
        }
    }

    double sr = 48000.0;
    Params params;
    MicroShifter shifters[2];
    Biquad hp[2], hp2[2], lp[2];
    Smoother centsL, centsR, delayL, delayR, windowS, dryGain, wetGain, outGain, lpCutoff, focusSm, driftAmt;
    DriftLFO driftL, driftR;
    double driftDepth = 0.0;
    bool primed = false;
};

} // namespace twinpitch
