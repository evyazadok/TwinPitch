#pragma once
// Host-rate wrapper around the 48 kHz engine: resamples in, runs the engine, resamples out.
// At 48 kHz it is a straight pass (no resampling). Latency is reported by latencySamples().
#include "BouncerDSP.h"
#include <deque>

namespace bouncer
{
// streaming 4-point Lagrange resampler, ratio = input rate / output rate
class Stream
{
public:
    void prepare (double inRate, double outRate)
    {
        ratio = inRate / outRate; pos = 1.0; buf.assign (1, 0.0f);
        // a gentle low-pass before/after the rate change so nothing folds back in (only when a rate change > 20 % is involved)
        useLp = std::max (inRate, outRate) > 1.2 * std::min (inRate, outRate) || false;
        const double fc = 20000.0 / std::max (inRate, outRate) * 1.0;   // relative to the higher rate
        if (useLp) { const double w = std::tan (kPi * std::min (0.45, fc)), q1 = 0.5412, q2 = 1.3066;
                     for (int s = 0; s < 2; ++s) { const double q = s ? q2 : q1, n = 1.0 / (1.0 + w / q + w * w);
                         b0[s] = w * w * n; b1[s] = 2 * b0[s]; b2[s] = b0[s]; a1[s] = 2.0 * (w * w - 1.0) * n; a2[s] = (1.0 - w / q + w * w) * n; z1[s] = z2[s] = 0; } }
        lpOnOutputSide = outRate > inRate;
    }
    void push (const float* in, int n)
    {
        for (int i = 0; i < n; ++i) buf.push_back (useLp && ! lpOnOutputSide ? lp (in[i]) : in[i]);
    }
    int pull (float* out, int maxOut)
    {
        int c = 0;
        while (c < maxOut)
        {
            const int idx = (int) pos;
            if (idx + 2 >= (int) buf.size()) break;
            const double f = pos - idx;
            const double y0 = buf[(size_t) idx - 1], y1 = buf[(size_t) idx], y2 = buf[(size_t) idx + 1], y3 = buf[(size_t) idx + 2];
            const double c0 = -f * (f - 1) * (f - 2) / 6.0, c1 = (f + 1) * (f - 1) * (f - 2) / 2.0, c2 = -(f + 1) * f * (f - 2) / 2.0, c3 = (f + 1) * f * (f - 1) / 6.0;
            const float y = (float) (c0 * y0 + c1 * y1 + c2 * y2 + c3 * y3);
            out[c++] = useLp && lpOnOutputSide ? lp (y) : y;
            pos += ratio;
        }
        const int drop = (int) pos - 1;
        if (drop > 0) { buf.erase (buf.begin(), buf.begin() + drop); pos -= drop; }
        return c;
    }
private:
    float lp (float x)
    {
        double v = x;
        for (int s = 0; s < 2; ++s) { const double y = b0[s] * v + z1[s]; z1[s] = b1[s] * v - a1[s] * y + z2[s]; z2[s] = b2[s] * v - a2[s] * y; v = y; }
        return (float) v;
    }
    double ratio = 1.0, pos = 1.0; std::vector<float> buf; bool useLp = false, lpOnOutputSide = false;
    double b0[2] {}, b1[2] {}, b2[2] {}, a1[2] {}, a2[2] {}, z1[2] {}, z2[2] {};
};

class Chain
{
public:
    void prepare (double hostRate)
    {
        rate = hostRate; direct = std::abs (hostRate - 48000.0) < 1.0;
        down.prepare (hostRate, 48000.0); up.prepare (48000.0, hostRate);
        prefill = direct ? 0 : 96 + (int) (hostRate / 48000.0 * 16);                // keeps the output fed while the resampler settles
        reset();
    }
    void reset()
    {
        eng.reset(); fifoOut.clear(); for (int i = 0; i < prefill; ++i) fifoOut.push_back (0.0f);
        down.prepare (rate, 48000.0); up.prepare (48000.0, rate);
    }
    void setParams (const Params& p) { eng.setParams (p); }
    int latencySamples() const { return (int) std::ceil (Engine::latencySamples() * rate / 48000.0) + (direct ? 0 : prefill + 3); }
    float voice() const { return eng.voiceActivity(); }

    void process (float* x, int n)
    {
        if (direct) { eng.process (x, n); return; }
        down.push (x, n);
        int got;
        do { got = down.pull (tmp, kTmp); if (got > 0) { eng.process (tmp, got); up.push (tmp, got); } } while (got == kTmp);
        do { got = up.pull (tmp, kTmp); for (int i = 0; i < got; ++i) fifoOut.push_back (tmp[i]); } while (got == kTmp);
        for (int i = 0; i < n; ++i) { if (fifoOut.empty()) x[i] = 0.0f; else { x[i] = fifoOut.front(); fifoOut.pop_front(); } }
    }
private:
    static constexpr int kTmp = 512;
    Engine eng; Stream down, up; std::deque<float> fifoOut; float tmp[kTmp] {};
    double rate = 48000.0; bool direct = true; int prefill = 0;
};
}
