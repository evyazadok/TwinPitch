#include "../Source/ToasterDSP.h"
#include <cstdio>
#include <vector>
#include <random>
using namespace toaster;
static double gainAt (const Params& p, double f, double fs = 48000, double amp = 0.05)
{
    Engine e; Params q = p; q.autoGain = false; e.setParams (q); e.prepare (fs);
    const int N = 48000; std::vector<float> l (N), r (N);
    for (int i = 0; i < N; ++i) l[i] = r[i] = (float) (amp * std::sin (2 * kPi * f * i / fs));
    for (int b = 0; b < N; b += 512) e.process (l.data() + b, r.data() + b, std::min (512, N - b));
    double s = 0, s0 = 0; for (int i = N / 2; i < N; ++i) { s += l[i] * l[i]; s0 += std::pow (amp * std::sin (2 * kPi * f * i / fs), 2); }
    return 10 * std::log10 (s / s0);
}
int main()
{
    const double fr[] = { 100, 1000, 3000, 5000, 7000, 8000, 10000, 12000, 14000, 16000, 20000 };
    printf ("freq    "); for (double f : fr) printf ("%7.0f", f); printf ("\n");
    auto row = [&] (const char* nm, Params p) { printf ("%-8s", nm); for (double f : fr) printf ("%7.2f", gainAt (p, f)); printf ("\n"); };
    Params p; p.silk = 0; p.tame = 0;
    for (double t : { 0.2, 0.5, 0.8, 1.0 }) { p.toast = t; char n[16]; snprintf (n, 16, "t%.1f", t); row (n, p); }
    p.toast = 0.8; for (double w : { 0.0, 1.0 }) { p.width = w; char n[16]; snprintf (n, 16, "w%.0f", w); row (n, p); } p.width = 0.5;
    for (int f = 0; f < 4; ++f) { p.freq = f; char n[16]; snprintf (n, 16, "f%d", f); row (n, p); } p.freq = 1;
    p.tame = 1; row ("tame1", p); p.tame = 0;
    p.silk = 1; row ("silk1", p);
    // harmonic content: 5 kHz sine hot, measure 10k (2nd) vs fundamental
    // auto gain: pink-ish noise, how much does it move
    std::mt19937 rng (1); std::normal_distribution<float> nd (0, 0.1f);
    for (double t : { 0.3, 0.7, 1.0 })
    {
        Params q; q.toast = t; q.silk = 0.5; q.autoGain = true;
        Engine e; e.setParams (q); e.prepare (48000);
        double lp = 0; const int N = 48000 * 6; std::vector<float> l (N), r (N), l0;
        for (int i = 0; i < N; ++i) { lp += 0.15 * (nd (rng) - lp); l[i] = r[i] = (float) (lp * 2.0); }
        l0 = l;
        for (int b = 0; b < N; b += 512) e.process (l.data() + b, r.data() + b, std::min (512, N - b));
        double a = 0, b2 = 0; for (int i = N - 48000; i < N; ++i) { a += l0[i] * l0[i]; b2 += l[i] * l[i]; }
        printf ("autogain t=%.1f: out-in = %.2f dB (gain %.2f dB), nan=%d\n", t, 10 * log10 (b2 / a), e.gainDb.load(), std::isnan (l[N - 1]));
    }
    // bypass exact
    { Params q; q.on = false; Engine e; e.setParams (q); e.prepare (48000); std::vector<float> l (4800, 0.3f), r (4800, 0.3f); e.process (l.data(), r.data(), 4800); printf ("bypass err %.2e\n", std::abs (l[4799] - 0.3f)); }
    // mid/side: mono signal at 10k should be nearly untouched (side only)
    { Params q; q.toast = 1; q.silk = 0; q.midSide = true; q.autoGain = false; printf ("mid/side mono 10k: %.2f dB (mid scale .35)\n", gainAt (q, 10000)); }
    for (double fs : { 44100.0, 96000.0 }) { Params q; q.toast = 1; q.silk = 0; printf ("fs %.0f 10k at t=1: %.2f dB\n", fs, gainAt (q, 10000, fs)); }
}
