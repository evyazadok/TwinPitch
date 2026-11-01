#pragma once
// BOUNCER engine. Mono, 48 kHz in and out (the plugin resamples around it).
//
//   input -> RNNoise (neural net that tells voice from noise, band by band)
//         -> pushed harder than normal (gain exponent, faster release, optional second pass)
//         -> late-reverb remover (spectral, tuned for untreated rooms)
//         -> + a little of the gently cleaned signal if FLOOR is up (so it never sounds dead)
//
// The BOUNCE knob is deliberately lopsided: the first half is a normal clean-up, the second half goes far past
// what a "safe" plugin would allow, because for a bare room recording that is where the useful range is.
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstring>
#include <vector>
#include "rnnoise/rnnoise.h"

namespace bouncer
{
static constexpr double kPi = 3.14159265358979323846;
static constexpr int kFrame = 480;                 // RNNoise frame, 10 ms at 48 kHz
static constexpr int kN = 1024, kHop = 256;        // reverb remover STFT
static constexpr int kLatency = 3 * kFrame + kN;   // samples at 48 kHz: our frame + two RNNoise passes (10 ms overlap each) + reverb remover

struct Params
{
    bool on = true;
    double bounce = 0.6;     // 0..1
    double room = 0.5;       // 0..1  reverb removal
    double floorDb = -100.0; // level of gently cleaned signal mixed back, -100 = none
    bool listenRemoved = false;
};

inline const char* wordFor (double b) { return b < 0.34 ? "Polite" : (b < 0.67 ? "Strict" : "Ruthless"); }

class FFT
{
public:
    FFT() { const int n = kN; tw.resize ((size_t) n / 2); rev.resize ((size_t) n);
            for (int i = 0; i < n / 2; ++i) tw[(size_t) i] = std::polar (1.0f, (float) (-2.0 * kPi * i / n));
            int bits = 0; while ((1 << bits) < n) ++bits;
            for (int i = 0; i < n; ++i) { int r = 0; for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b); rev[(size_t) i] = r; } }
    void run (std::complex<float>* a, bool inverse) const
    {
        const int n = kN;
        for (int i = 0; i < n; ++i) if (rev[(size_t) i] > i) std::swap (a[i], a[rev[(size_t) i]]);
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    std::complex<float> w = tw[(size_t) (j * step)]; if (inverse) w = std::conj (w);
                    const std::complex<float> u = a[i + j], v = a[i + j + half] * w;
                    a[i + j] = u + v; a[i + j + half] = u - v;
                }
        }
        if (inverse) for (int i = 0; i < n; ++i) a[i] /= (float) n;
    }
private:
    std::vector<std::complex<float>> tw; std::vector<int> rev;
};

// removes the long tail a room adds behind every word (late reverberation), per frequency
class Dereverb
{
public:
    Dereverb() { win.resize (kN); for (int i = 0; i < kN; ++i) win[(size_t) i] = (float) std::sin (kPi * (i + 0.5) / kN); reset(); }
    void reset()
    {
        std::fill (inBuf.begin(), inBuf.end(), 0.0f); std::fill (ola.begin(), ola.end(), 0.0f);
        for (auto& v : hist) v.assign (kN / 2 + 1, 1.0e-9f); histPos = 0;
        late.assign (kN / 2 + 1, 0.0f); gPrev.assign (kN / 2 + 1, 1.0f);
        count = 0; qr = 0; qw = kHop; std::fill (outQ.begin(), outQ.end(), 0.0f); qCount = kHop;   // start with a hop of silence queued
        inPos = 0;
    }
    // amount 0..1
    float push (float x, double amount)
    {
        inBuf[(size_t) inPos] = x; inPos = (inPos + 1) % kN;
        float y = qCount > 0 ? outQ[(size_t) qr] : 0.0f;
        if (qCount > 0) { qr = (qr + 1) % kQ; --qCount; }
        if (++count >= kHop) { count = 0; frame (amount); }
        return y;
    }
private:
    static constexpr int kQ = 4 * kHop;
    static constexpr int kD = 10;                  // frames of delay before sound counts as "late"
    void frame (double amount)
    {
        std::complex<float> a[kN];
        for (int i = 0; i < kN; ++i) a[i] = inBuf[(size_t) ((inPos + i) % kN)] * win[(size_t) i];
        fft.run (a, false);
        const int bins = kN / 2 + 1;
        // the room's decay: 60 dB over T60 seconds. Longer reverb means the knob also assumes a longer tail.
        const double t60 = 0.35 + 0.5 * amount;
        const double d = std::pow (10.0, -6.0 * (double) kHop / (t60 * 48000.0));     // power decay per hop
        const float alpha = (float) (1.0 + 0.6 * amount);                              // how much of the estimate is taken away
        const float beta = (float) (0.4 * amount);                                      // how much of the earlier sound is assumed to be room
        const float gmin = (float) std::pow (10.0, -(8.0 + 26.0 * amount) / 20.0);
        const auto& old = hist[(size_t) ((histPos + kHist - kD) % kHist)];
        auto& cur = hist[(size_t) histPos];
        for (int k = 0; k < bins; ++k)
        {
            const float p = std::norm (a[k]) + 1.0e-9f;
            cur[(size_t) k] = p;
            late[(size_t) k] = (float) (d * late[(size_t) k] + (1.0 - d) * beta * old[(size_t) k]);
            float g = amount < 0.001 ? 1.0f : 1.0f - alpha * late[(size_t) k] / p;
            g = std::sqrt (std::max (gmin * gmin, g));
            // gain may fall quickly (the tail is over) but comes back more slowly so words are not chopped at the start
            const float gp = gPrev[(size_t) k];
            g = g < gp ? gp + (g - gp) * 0.85f : gp + (g - gp) * 0.6f;
            gPrev[(size_t) k] = g;
            a[k] *= g;
            if (k > 0 && k < kN / 2) a[kN - k] = std::conj (a[k]);
        }
        a[0] = std::complex<float> (a[0].real(), 0.0f); a[kN / 2] = std::complex<float> (a[kN / 2].real(), 0.0f);
        histPos = (histPos + 1) % kHist;
        fft.run (a, true);
        for (int i = 0; i < kN; ++i) ola[(size_t) i] += a[i].real() * win[(size_t) i] * 0.5f;     // sin window twice, 75 % overlap
        for (int i = 0; i < kHop; ++i) { outQ[(size_t) qw] = ola[(size_t) i]; qw = (qw + 1) % kQ; ++qCount; }
        std::memmove (ola.data(), ola.data() + kHop, sizeof (float) * (size_t) (kN - kHop));
        std::fill (ola.begin() + (kN - kHop), ola.end(), 0.0f);
    }
    static constexpr int kHist = 32;
    FFT fft; std::vector<float> win;
    std::array<float, kN> inBuf {}, ola {}; std::array<float, kQ> outQ {};
    std::array<std::vector<float>, kHist> hist; std::vector<float> late, gPrev;
    int histPos = 0, count = 0, qr = 0, qw = 0, qCount = 0, inPos = 0;
};

class Engine
{
public:
    Engine() { hardA = rnnoise_create (nullptr); hardB = rnnoise_create (nullptr); light = rnnoise_create (nullptr); reset(); }
    ~Engine() { rnnoise_destroy (hardA); rnnoise_destroy (hardB); rnnoise_destroy (light); }
    Engine (const Engine&) = delete; Engine& operator= (const Engine&) = delete;

    static int latencySamples() { return kLatency; }
    void reset()
    {
        rnnoise_init (hardA, nullptr); rnnoise_init (hardB, nullptr); rnnoise_init (light, nullptr);
        derev.reset(); fill = 0; outHave = 0; outPos = 0;
        std::fill (dryD.begin(), dryD.end(), 0.0f); std::fill (lightD.begin(), lightD.end(), 0.0f); std::fill (hPrev.begin(), hPrev.end(), 0.0f); dPos = 0; lPos = 0;
        std::fill (outBuf.begin(), outBuf.end(), 0.0f); std::fill (inBuf.begin(), inBuf.end(), 0.0f);
        vad = 0;
    }
    void setParams (const Params& p) { P = p; }
    float voiceActivity() const { return vad; }

    // in-place, 48 kHz. Output is the input delayed by latencySamples().
    void process (float* x, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float y = outPos < outHave ? outBuf[(size_t) outPos++] : 0.0f;
            inBuf[(size_t) fill++] = x[i];
            if (fill == kFrame) { frameNow(); fill = 0; outHave = kFrame; outPos = 0; }
            x[i] = y;
        }
    }
private:
    void frameNow()
    {
        const double b = std::clamp (P.bounce, 0.0, 1.0);
        float s[kFrame], h[kFrame], t[kFrame], l[kFrame];
        for (int i = 0; i < kFrame; ++i) s[i] = inBuf[(size_t) i] * 32768.0f;

        // --- BOUNCE: 0..1 mapped hard to the right. The upper half is far beyond a normal clean-up.
        const float e1 = (float) (1.0 + 3.2 * std::pow (b, 1.35));
        const float dec = (float) (0.6 - 0.28 * std::pow (b, 1.2));
        vad = rnnoise_process_frame_x (hardA, h, s, e1, dec);
        const double second = std::clamp ((b - 0.5) / 0.25, 0.0, 1.0);                     // second pass fades in from 50 %
        {
            // the second RNNoise pass delays by one more frame; the first pass result is always held back one frame
            // so the two can be crossfaded without drifting apart
            const float e2 = (float) (1.0 + 2.0 * std::clamp ((b - 0.5) / 0.5, 0.0, 1.0));
            rnnoise_process_frame_x (hardB, t, h, e2, dec);
            for (int i = 0; i < kFrame; ++i) { const float held = hPrev[(size_t) i]; hPrev[(size_t) i] = h[i]; h[i] = held + (float) second * (t[i] - held); }
        }
        const bool wantFloor = P.floorDb > -60.0;
        if (wantFloor) rnnoise_process_frame_x (light, l, s, 1.0f, 0.6f);

        const double room = std::clamp (P.room, 0.0, 1.0);
        const float fl = wantFloor ? (float) std::pow (10.0, P.floorDb / 20.0) : 0.0f;
        for (int i = 0; i < kFrame; ++i)
        {
            const float dryNow = inBuf[(size_t) i];
            float hard = h[i] / 32768.0f;
            if (room > 0.001) { const float dr = derev.push (hard, room); hard = dr; }
            else { const float dr = derev.push (hard, 0.0); hard = dr; }                      // same delay with the room control down
            // delay lines so everything lines up with the longest path
            const float lightNow = wantFloor ? l[i] / 32768.0f : 0.0f;
            const float lightLate = lightD[(size_t) lPos]; lightD[(size_t) lPos] = lightNow; lPos = (lPos + 1) % kLightDelay;
            const float dryLate = dryD[(size_t) dPos];   dryD[(size_t) dPos] = dryNow;
            dPos = (dPos + 1) % kDelay;
            float y = hard + fl * lightLate;
            if (P.listenRemoved) y = dryLate - y;
            outBuf[(size_t) i] = P.on ? y : dryLate;
        }
    }
    // the dry path sees: RNNoise frame (kFrame, already inherent: inBuf delays it by one frame) plus the reverb remover (kN)
    static constexpr int kDelay = 2 * kFrame + kN;   // dry path: two RNNoise overlaps + reverb remover (the frame buffering is shared by all paths)
    static constexpr int kLightDelay = kFrame + kN;  // gently cleaned path: one RNNoise overlap + reverb remover
    DenoiseState *hardA = nullptr, *hardB = nullptr, *light = nullptr;
    Params P; Dereverb derev;
    std::array<float, kFrame> inBuf {}, outBuf {};
    std::array<float, kDelay> dryD {}; std::array<float, kLightDelay> lightD {}; std::array<float, kFrame> hPrev {};
    int fill = 0, outHave = 0, outPos = 0, dPos = 0, lPos = 0; float vad = 0;
};
}
