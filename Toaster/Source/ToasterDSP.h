#pragma once
#include <cmath>
#include <algorithm>
#include <atomic>

// TOASTER: one big knob of top end, shaped like a Pultec EQP-1A boost (a tuned peak plus a little air shelf),
// a tame shelf that fights it the way the real attenuator does, and a touch of tube and iron.
namespace toaster
{
    static const double kPi = 3.14159265358979323846;
    static const double kFreqs[4] = { 8000.0, 10000.0, 12000.0, 16000.0 };

    inline const char* wordFor (double toast)
    {
        return toast < 0.34 ? "WARM" : toast < 0.67 ? "HOT" : "BURNT";
    }

    struct Params
    {
        double toast = 0.3, width = 0.5, silk = 0.4, tame = 0.0;
        int freq = 1;
        bool autoGain = true, on = true, midSide = false;
    };

    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        inline double process (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
        void reset() { z1 = z2 = 0; }
        void set (double B0, double B1, double B2, double A0, double A1, double A2) { b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0; }
        void peaking (double fs, double f, double Q, double dB)
        {
            const double A = std::pow (10.0, dB / 40.0), w = 2 * kPi * f / fs, al = std::sin (w) / (2 * Q), c = std::cos (w);
            set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
        }
        void highShelf (double fs, double f, double dB, double S = 0.8)
        {
            const double A = std::pow (10.0, dB / 40.0), w = 2 * kPi * f / fs, c = std::cos (w), sn = std::sin (w);
            const double al = sn / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2), r = 2 * std::sqrt (A) * al;
            set (A * ((A + 1) + (A - 1) * c + r), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - r),
                 (A + 1) - (A - 1) * c + r, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - r);
        }
        void lowShelf (double fs, double f, double dB, double S = 0.8)
        {
            const double A = std::pow (10.0, dB / 40.0), w = 2 * kPi * f / fs, c = std::cos (w), sn = std::sin (w);
            const double al = sn / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2), r = 2 * std::sqrt (A) * al;
            set (A * ((A + 1) - (A - 1) * c + r), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - r),
                 (A + 1) + (A - 1) * c + r, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - r);
        }
        void highPass (double fs, double f, double Q = 0.7071)
        {
            const double w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * Q);
            set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
        }
    };

    struct Shape { double f0, Q, boostDb, airDb, tameDb, silk, drive, harm, ironDb, lpCoef; };

    inline Shape makeShape (double fs, const Params& p, double f0, double toast, double width, double silk, double tame, double scale)
    {
        Shape s;
        s.f0 = std::min (f0, fs * 0.42);
        s.Q = 1.9 * std::pow (0.22, width);                       // sharp .. broad
        const double lift = std::pow (toast, 1.6);                // the knob is not in dB: it opens slowly, then fast
        s.boostDb = 15.0 * lift * scale;
        s.airDb = 0.28 * s.boostDb;                               // a little shelf above the peak, like the real network
        s.tameDb = -10.0 * tame * scale;
        s.silk = silk;
        s.drive = 1.3 + 2.0 * toast;
        s.harm = silk * (0.25 + 0.75 * toast) * 0.6;
        s.ironDb = 0.9 * silk;
        const double top = 0.45 * fs, fc = top - silk * (top - 17000.0);
        s.lpCoef = 1.0 - std::exp (-2 * kPi * std::min (fc, top) / fs);
        (void) p;
        return s;
    }

    class Chain
    {
    public:
        void reset() { peak.reset(); air.reset(); tame.reset(); iron.reset(); hp.reset(); lp = 0; dx = dy = 0; }
        void set (double fs, const Shape& s)
        {
            peak.peaking (fs, s.f0, s.Q, s.boostDb);
            air.highShelf (fs, s.f0 * 0.85, s.airDb);
            tame.highShelf (fs, std::min (s.f0 * 1.35, fs * 0.42), s.tameDb);
            iron.lowShelf (fs, 90.0, s.ironDb);
            hp.highPass (fs, 2500.0);
            sh = s;
        }
        inline double process (double x)
        {
            double y = peak.process (x);
            y = air.process (y);
            y = tame.process (y);
            y = iron.process (y);
            if (sh.harm > 1.0e-6)
            {
                const double h = hp.process (y), b = 0.2, t0 = std::tanh (b);
                const double sat = (std::tanh (sh.drive * h + b) - t0) / sh.drive;
                double d = sat - 0.96 * h;                      // keep only what the tubes add
                const double o = d - dx + 0.9985 * dy; dx = d; dy = o;
                y += sh.harm * o;
            }
            else hp.process (y);
            lp += sh.lpCoef * (y - lp);
            return lp;
        }
    private:
        Biquad peak, air, tame, iron, hp; Shape sh {}; double lp = 0, dx = 0, dy = 0;
    };

    class Engine
    {
    public:
        void prepare (double sr)
        {
            fs = sr; aSmooth = 1.0 - std::exp (-1.0 / (0.03 * fs)); aOn = 1.0 - std::exp (-1.0 / (0.01 * fs));
            aPow = 1.0 - std::exp (-1.0 / (0.4 * fs)); aGain = 1.0 - std::exp (-1.0 / (0.7 * fs));
            for (auto& w : wIn) { w[0].highPass (fs, 150.0); w[1].highShelf (fs, 2000.0, 4.0); }
            for (auto& w : wOut) { w[0].highPass (fs, 150.0); w[1].highShelf (fs, 2000.0, 4.0); }
            static_cast<Params&> (cur) = target; cur.f0 = kFreqs[target.freq]; cur.scaleA = target.midSide ? 0.35 : 1.0;
            chA.reset(); chB.reset(); updateCoefs();
            pIn = pOut = 0; gain = 1; onMix = target.on ? 1 : 0; counter = 0;
        }
        void setParams (const Params& p) { target = p; }
        void process (float* l, float* r, int n)
        {
            float mIn = 0, mOut = 0;
            for (int i = 0; i < n; ++i)
            {
                cur.toast += aSmooth * (target.toast - cur.toast);
                cur.width += aSmooth * (target.width - cur.width);
                cur.silk += aSmooth * (target.silk - cur.silk);
                cur.tame += aSmooth * (target.tame - cur.tame);
                cur.f0 += aSmooth * 0.5 * (kFreqs[target.freq] - cur.f0);
                cur.scaleA += aSmooth * ((target.midSide ? 0.35 : 1.0) - cur.scaleA);
                if ((counter++ & 15) == 0) updateCoefs();

                const double dl = l[i], dr = r[i];
                double xa = dl, xb = dr;
                if (target.midSide) { xa = (dl + dr) * 0.5; xb = (dl - dr) * 0.5; }
                const double ya = chA.process (xa), yb = chB.process (xb);
                double wl = ya, wr = yb;
                if (target.midSide) { wl = ya + yb; wr = ya - yb; }

                // auto gain: compare how loud the top-weighted input and the EQ'd signal are, move the gain slowly
                const double si = wIn[0][1].process (wIn[0][0].process (dl)) + wIn[1][1].process (wIn[1][0].process (dr));
                const double so = wOut[0][1].process (wOut[0][0].process (wl)) + wOut[1][1].process (wOut[1][0].process (wr));
                pIn += aPow * (si * si - pIn); pOut += aPow * (so * so - pOut);
                double tg = 1.0;
                if (target.autoGain && pIn > 1.0e-7 && pOut > 1.0e-9) tg = std::min (1.58, std::max (0.63, std::pow (pIn / pOut, 0.3)));   // about 60% of the way, max 4 dB
                gain += aGain * (tg - gain);

                onMix += aOn * ((target.on ? 1.0 : 0.0) - onMix);
                const double ol = dl + onMix * (wl * gain - dl), orr = dr + onMix * (wr * gain - dr);
                l[i] = (float) ol; r[i] = (float) orr;
                mIn = std::max (mIn, (float) std::max (std::abs (dl), std::abs (dr)));
                mOut = std::max (mOut, (float) std::max (std::abs (ol), std::abs (orr)));
            }
            peakIn.store (std::max (peakIn.load() * 0.0f, mIn)); peakOut.store (mOut);
            gainDb.store ((float) (20.0 * std::log10 (std::max (1.0e-4, gain))));
        }
        std::atomic<float> peakIn { 0 }, peakOut { 0 }, gainDb { 0 };
    private:
        struct Cur : Params { double f0 = 10000, scaleA = 1; };
        void updateCoefs()
        {
            chA.set (fs, makeShape (fs, cur, cur.f0, cur.toast, cur.width, cur.silk, cur.tame, cur.scaleA));
            chB.set (fs, makeShape (fs, cur, cur.f0, cur.toast, cur.width, cur.silk, cur.tame, 1.0));
        }
        double fs = 48000, aSmooth = 0.001, aOn = 0.01, aPow = 1e-5, aGain = 1e-5;
        Params target; Cur cur; Chain chA, chB;
        Biquad wIn[2][2], wOut[2][2];
        double pIn = 0, pOut = 0, gain = 1, onMix = 1; unsigned counter = 0;
    };
}
