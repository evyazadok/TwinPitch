// Measures SLEDGE without JUCE: oversampler, harmonics and aliasing per style, auto gain, silence, style switching, speed.
// build: g++ -O2 -std=c++17 measure.cpp -o measure && ./measure
#include "../Source/SledgeDSP.h"
#include <complex>
#include <cstdio>
#include <random>
#include <chrono>
#include <string>

using namespace sledge;
typedef std::complex<double> cd;

static void fft (std::vector<cd>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2 * kPi / (double) len; const cd wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len) { cd w (1); for (size_t j = 0; j < len / 2; ++j) { cd u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; } }
    }
}

static double rmsOf (const std::vector<float>& v, size_t from = 0)
{
    double s = 0; for (size_t i = from; i < v.size(); ++i) s += (double) v[i] * v[i]; return std::sqrt (s / (double) (v.size() - from));
}

static Params mk (int style, double drive, double mix = 1.0, bool ag = true)
{
    Params p; p.style = style; p.drive = drive; p.mix = mix; p.autoGain = ag; p.bpm = 120; return p;
}

static void run (Engine& e, std::vector<float>& L, std::vector<float>& R)
{
    const size_t n = L.size();
    for (size_t i = 0; i < n; i += 512) { const int b = (int) std::min<size_t> (512, n - i); e.process (L.data() + i, R.data() + i, b); }
}

int main (int argc, char** argv)
{
    const double fs = argc > 2 ? std::atof (argv[2]) : 48000.0;
    int fails = 0;

    // ---------------------------------------------------------------- 1. oversampler: delay and passband
    {
        const auto& d = osDesign(); Up2 u1, u2a, u2b; Down2 d1, d2a, d2b;
        u1.hb = &d.s1; u2a.hb = u2b.hb = &d.s2; d1.hb = &d.s1; d2a.hb = d2b.hb = &d.s2;
        auto passThrough = [&] (double f, std::vector<double>& out, std::vector<double>& in, size_t N)
        {
            out.assign (N, 0); in.assign (N, 0);
            for (size_t i = 0; i < N; ++i)
            {
                const double x = 0.5 * std::sin (2 * kPi * f * (double) i / fs); in[i] = x;
                double a0, a1, o[4]; u1.process (x, a0, a1); u2a.process (a0, o[0], o[1]); u2b.process (a1, o[2], o[3]);
                const double b0 = d2a.process (o[0], o[1]), b1 = d2b.process (o[2], o[3]); out[i] = d1.process (b0, b1);
            }
        };
        std::printf ("oversampler (design latency %.2f samples)\n", d.latency());
        for (double f : { 200.0, 1000.0, 5000.0, 10000.0, 16000.0, 19000.0 })
        {
            std::vector<double> o, in; passThrough (f, o, in, 48000);
            // amplitude and delay via correlation with sin/cos over the last 24000 samples
            double sr = 0, si = 0, sri = 0, sii = 0;
            for (size_t i = 24000; i < 48000; ++i) { const double w = 2 * kPi * f * (double) i / fs; sr += o[i] * std::sin (w); si += o[i] * std::cos (w); sri += in[i] * std::sin (w); sii += in[i] * std::cos (w); }
            const double amp = std::sqrt (sr * sr + si * si) / std::sqrt (sri * sri + sii * sii);
            double ph = std::atan2 (si, sr) - std::atan2 (sii, sri); while (ph > kPi) ph -= 2 * kPi; while (ph < -kPi) ph += 2 * kPi;
            const double delay = -ph / (2 * kPi * f / fs);
            std::printf ("  %6.0f Hz: gain %+.3f dB, delay %.2f samples\n", f, 20 * std::log10 (amp), delay);
            if (std::abs (20 * std::log10 (amp)) > 0.5 && f < 0.36 * fs) { std::printf ("  FAIL passband\n"); ++fails; }
            { double dd = delay - d.latency(); const double per = fs / f; dd -= per * std::round (dd / per); if (std::abs (dd) > 0.05) { std::printf ("  FAIL delay\n"); ++fails; } }
        }
    }

    if (argc > 1 && std::string (argv[1]) == "os") return fails ? 1 : 0;

    // ---------------------------------------------------------------- 2. per style: levels, harmonics, aliasing, silence
    std::printf ("\nper style, 220 Hz sine at -14 dBFS peak (autogain on).  H2/H3/H5 relative to the fundamental in dB; alias = non-harmonic energy in dB below total\n");
    std::printf ("%-9s %5s | %7s %7s %7s %7s | %6s | %6s\n", "style", "drive", "H2", "H3", "H5", "THD%", "alias", "gain");
    const double f0 = 218.75;              // 48000/219.43..: pick a bin-exact value for an 8192 FFT: 1 cycle = 219.43 samples; use N such that f0*N/fs integer
    const size_t N = 32768;                // 218.75 Hz * 32768 / 48000 = 149.33 -> use exact below
    const double fExact = 150.0 * fs / (double) N;   // 150 cycles in the window
    (void) f0;
    for (int s = 0; s < kNumStyles; ++s)
    {
        for (double drive : { 0.0, 0.35, 0.7, 1.0 })
        {
            Engine e; e.setParams (mk (s, drive)); e.prepare (fs); e.setParams (mk (s, drive));
            const size_t total = N + 48000; std::vector<float> L (total), R (total);
            for (size_t i = 0; i < total; ++i) L[i] = R[i] = (float) (0.2 * std::sin (2 * kPi * fExact * (double) i / fs));
            run (e, L, R);
            std::vector<cd> a (N); for (size_t i = 0; i < N; ++i) a[i] = cd (L[48000 + i] * (0.5 - 0.5 * std::cos (2 * kPi * (double) i / (double) N)), 0);
            fft (a);
            auto binPow = [&] (int k) { double p = 0; for (int j = -2; j <= 2; ++j) { const int b = k + j; if (b > 0 && b < (int) N / 2) p += std::norm (a[(size_t) b]); } return p; };
            const double p1 = binPow (150); double thd = 0, tot = 0, harm = 0, harmAll = 0;
            for (int b = 3; b < (int) N / 2; ++b) tot += std::norm (a[(size_t) b]);
            double hp[8] = {};
            for (int h = 2; h <= 7; ++h) { const int k = 150 * h; if (k < (int) N / 2 - 3) { hp[h] = binPow (k); harm += hp[h]; } }
            for (int h = 2; 150 * h < (int) N / 2 - 3; ++h) harmAll += binPow (150 * h);
            thd = std::sqrt (harm / p1) * 100.0;
            const double alias = tot > 0 ? 10 * std::log10 (std::max (1e-30, (tot - p1 - harmAll) / tot)) : -200;
            const double gain = 20 * std::log10 (rmsOf (L, 48000) / (0.2 / std::sqrt (2.0)));
            std::printf ("%-9s %5.2f | %7.1f %7.1f %7.1f %7.1f | %6.1f | %+5.1f\n", kStyles[s].name, drive,
                         10 * std::log10 (std::max (1e-30, hp[2] / p1)), 10 * std::log10 (std::max (1e-30, hp[3] / p1)),
                         10 * std::log10 (std::max (1e-30, hp[5] / p1)), thd, alias, gain);
            for (float v : L) if (! std::isfinite (v) || std::abs (v) > 30.0f) { std::printf ("  FAIL non-finite / huge\n"); ++fails; break; }
        }
    }

    // ---------------------------------------------------------------- 3. silence in -> silence out
    std::printf ("\nsilence test\n");
    for (int s = 0; s < kNumStyles; ++s)
    {
        Engine e; e.setParams (mk (s, 1.0)); e.prepare (fs); e.setParams (mk (s, 1.0));
        std::vector<float> L (48000, 0.0f), R (48000, 0.0f); run (e, L, R);
        const double r = rmsOf (L, 4800);
        if (r > 1.0e-5) { std::printf ("  %s leaks %.2e rms\n", kStyles[s].name, r); ++fails; }
    }

    // ---------------------------------------------------------------- 4. auto gain on speech-like noise
    std::printf ("\nauto gain: level out vs in on a speech-like signal (dB), drive 0.35 / 0.7 / 1.0 (off: raw gain at 0.7)\n");
    {
        std::mt19937 rng (7); std::normal_distribution<double> nd (0, 1);
        const size_t T = 6 * 48000; std::vector<float> src (T);
        double lp1 = 0, lp2 = 0;
        for (size_t i = 0; i < T; ++i)
        {
            const double t = (double) i / fs, env = 0.5 + 0.5 * std::sin (2 * kPi * 3.0 * t) * std::sin (2 * kPi * 0.7 * t);
            const double voice = std::sin (2 * kPi * 140 * t) + 0.5 * std::sin (2 * kPi * 280 * t) + 0.3 * std::sin (2 * kPi * 560 * t) + 0.1 * nd (rng);
            lp1 += 0.3 * (voice - lp1); lp2 += 0.3 * (lp1 - lp2);
            src[i] = (float) (0.16 * env * lp2 * 2.0);
        }
        for (int s = 0; s < kNumStyles; ++s)
        {
            double res[4];
            int k = 0;
            for (double drive : { 0.35, 0.7, 1.0, -0.7 })
            {
                const bool ag = drive > 0; const double dd = std::abs (drive);
                Engine e; e.setParams (mk (s, dd, 1.0, ag)); e.prepare (fs); e.setParams (mk (s, dd, 1.0, ag));
                std::vector<float> L (src), R (src); run (e, L, R);
                std::vector<float> a (src.begin() + 24000, src.end()), b (L.begin() + 24000, L.end());
                res[k++] = 20 * std::log10 (rmsOf (b) / rmsOf (a));
            }
            std::printf ("%-9s %+5.1f %+5.1f %+5.1f   (raw %+5.1f)%s\n", kStyles[s].name, res[0], res[1], res[2], res[3],
                         (std::abs (res[0]) > 3.0 || std::abs (res[1]) > 3.0 || std::abs (res[2]) > 3.0) ? "   <-- off by more than 3 dB" : "");
        }
    }

    // ---------------------------------------------------------------- 5. mix = 0 is the dry signal, delayed
    {
        Engine e; e.setParams (mk (5, 1.0, 0.0)); e.prepare (fs); e.setParams (mk (5, 1.0, 0.0));
        std::vector<float> L (48000), R (48000);
        for (size_t i = 0; i < L.size(); ++i) L[i] = R[i] = (float) (0.3 * std::sin (2 * kPi * 3000.0 * (double) i / fs));
        std::vector<float> src (L);
        run (e, L, R);
        double err = 0, ref = 0;
        for (size_t i = 24000; i < 40000; ++i)
        {
            const double want = 0.3 * std::sin (2 * kPi * 3000.0 * ((double) i - osDesign().latency()) / fs);
            err += (L[i] - want) * (L[i] - want); ref += want * want;
        }
        std::printf ("\nmix 0 error vs ideal delayed dry: %.1f dB\n", 10 * std::log10 (err / ref + 1e-30));
        if (10 * std::log10 (err / ref + 1e-30) > -50) { std::printf ("  FAIL\n"); ++fails; }
    }

    // ---------------------------------------------------------------- 6. style switching: no clicks
    {
        Engine e; e.setParams (mk (0, 0.7)); e.prepare (fs);
        const size_t T = 8 * 48000; std::vector<float> L (T), R (T);
        for (size_t i = 0; i < T; ++i) L[i] = R[i] = (float) (0.25 * std::sin (2 * kPi * 330.0 * (double) i / fs));
        double maxJump = 0; float prev = 0; int st = 0;
        for (size_t i = 0; i < T; i += 512)
        {
            if ((i / 512) % 40 == 0) st = (st + 7) % kNumStyles;
            Params p = mk (st, 0.6); e.setParams (p);
            e.process (L.data() + i, R.data() + i, 512);
            for (size_t j = i; j < i + 512; ++j) { maxJump = std::max (maxJump, (double) std::abs (L[j] - prev)); prev = L[j]; }
        }
        std::printf ("style switching: largest sample-to-sample step %.3f\n", maxJump);
        if (maxJump > 1.2) { std::printf ("  FAIL (click)\n"); ++fails; }
    }

    // ---------------------------------------------------------------- 7. speed
    {
        double worst = 0; const char* worstName = "";
        for (int s = 0; s < kNumStyles; ++s)
        {
            Engine e; e.setParams (mk (s, 0.8)); e.prepare (fs); e.setParams (mk (s, 0.8));
            std::vector<float> L (48000 * 4), R (48000 * 4);
            for (size_t i = 0; i < L.size(); ++i) L[i] = R[i] = (float) (0.25 * std::sin (2 * kPi * 330.0 * (double) i / fs));
            const auto t0 = std::chrono::steady_clock::now(); run (e, L, R);
            const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            const double load = secs / 4.0 * 100.0; if (load > worst) { worst = load; worstName = kStyles[s].name; }
        }
        std::printf ("\nworst CPU load: %.1f%% of one core for a stereo instance (%s)\n", worst, worstName);
    }

    std::printf ("\n%s (%d problems)\n", fails ? "SOME CHECKS FAILED" : "all checks passed", fails);
    return fails ? 1 : 0;
}
