#pragma once
#include <cmath>
#include <algorithm>
#include <atomic>
#include <vector>
#include <cstdint>
#include <cstring>

// SLEDGE: a distortion box in the Decapitator family. 40 styles (20 "standard" ones for vocals and electric guitars,
// 20 stranger ones with wobble, flanger, phaser, vowel filters, folders...), one big DRIVE knob, MIX, OUTPUT, AUTO GAIN.
//
// Every style is a short chain of blocks. Blocks that bend the wave run at 4x oversampling (two halfband FIR stages),
// filters and modulation effects run at the normal rate (before or after the bending). Nothing here depends on JUCE.
namespace sledge
{
    static const double kPi = 3.14159265358979323846;
    static constexpr int kNumStyles = 40;
    static constexpr int kMaxBlocks = 8;
    static constexpr int kOS = 4;

    inline const char* wordFor (double drive)
    {
        return drive < 0.34 ? "HIT" : drive < 0.67 ? "SMASH" : "CRASH";
    }

    //==========================================================================================================
    // Style table
    //==========================================================================================================
    enum BlkType : uint8_t
    {
        B_END = 0,
        F_HP, F_LP, F_BP, F_PK, F_LS, F_HS,
        BAND_BEGIN, BAND_END,
        S_TANH, S_PENT, S_DIODE, S_FUZZ, S_CLIP, S_FOLDS, S_FOLDT, S_RECT, S_XOVER, S_SAG, S_CHEB, S_SLEW,
        X_WOB, X_SIREN, X_FLG, X_PHS, X_WAH, X_VOW, X_RING, X_TREM, X_CRUSH, X_COMB
    };

    // dom: 0 = normal rate, before the oversampled part. 1 = oversampled. 2 = normal rate, after it.
    struct Blk { BlkType type; uint8_t dom; float a, b, c, d, e; };
    struct StyleDef { const char* name; Blk blk[kMaxBlocks]; };

    // Parameter meaning per block type (drive 0 -> 1 moves a..b where noted):
    //  F_HP F_LP F_BP : a = Hz at drive 0, b = Hz at drive 1, c = Q
    //  F_PK           : a = Hz, b = Q, c = dB at drive 0, d = dB at drive 1
    //  F_LS F_HS      : a = Hz, b = slope, c = dB at drive 0, d = dB at drive 1
    //  BAND_BEGIN     : a = 0 process lows / 1 process highs / 2 process the band f..f2, b = f, c = f2, d = level of the untouched rest
    //  S_*            : a = gain dB at drive 0, b = gain dB at drive 1, then c, d as listed in the table below
    //  X_*            : see the comments in Chan::runBase
    #define BL(t, dom, a, b, c, d, e) Blk { t, dom, a, b, c, d, e }
    static const StyleDef kStyles[kNumStyles] =
    {
        //------------------------------------------------ 20 standard: vocals, guitars, anything
        { "TAPE", {   // soft, round, a little head bump, highs fold away as it gets hotter
            BL (F_PK, 1, 70, 0.9f, 1.0f, 3.5f, 0), BL (S_TANH, 1, 0, 28, 0.03f, 0, 0),
            BL (F_HS, 1, 7000, 0.8f, -1.5f, -4.0f, 0), BL (F_LP, 1, 16000, 11000, 0.7f, 0, 0) } },
        { "REEL", {   // tape pushed into the red: more squash, darker
            BL (F_HP, 1, 25, 25, 0.7f, 0, 0), BL (F_PK, 1, 100, 0.8f, 2.5f, 5.0f, 0), BL (S_TANH, 1, 8, 40, 0.06f, 0, 0),
            BL (F_HS, 1, 6000, 0.8f, -3.0f, -7.0f, 0), BL (F_LP, 1, 13000, 8500, 0.7f, 0, 0) } },
        { "IRON", {   // transformer: the lows saturate first, so you get weight without fizz
            BL (BAND_BEGIN, 1, 0, 250, 0, 1.0f, 0), BL (S_TANH, 1, 6, 38, 0.04f, 0, 0), BL (BAND_END, 1, 0, 0, 0, 0, 0),
            BL (S_TANH, 1, -2, 12, 0.02f, 0, 0) } },
        { "CONSOLE", {   // solid mids, a touch of 2nd harmonic, a little presence
            BL (F_PK, 1, 2500, 0.8f, 0.0f, 3.0f, 0), BL (S_TANH, 1, 3, 36, 0.12f, 0, 0), BL (F_LS, 1, 120, 0.8f, 1.0f, 3.0f, 0),
            BL (F_HS, 1, 10000, 0.8f, -1.0f, -3.0f, 0) } },
        { "VELVET", {   // beefy lows, silky highs
            BL (F_LS, 1, 100, 0.8f, 2.0f, 5.0f, 0), BL (S_TANH, 1, 2, 34, 0.08f, 0, 0),
            BL (F_HS, 1, 5000, 0.8f, 0.0f, -4.0f, 0), BL (F_LP, 1, 18000, 9000, 0.6f, 0, 0) } },
        { "TRIODE", {   // even harmonics, warm and fat
            BL (F_HP, 1, 30, 30, 0.7f, 0, 0), BL (S_TANH, 1, 4, 36, 0.5f, 0, 0), BL (F_LP, 1, 15000, 9000, 0.7f, 0, 0) } },
        { "PENTODE", {   // odd harmonics, harder knee, edgy
            BL (F_HS, 1, 3000, 0.8f, 1.5f, 3.0f, 0), BL (S_PENT, 1, 4, 38, 3.0f, 0.08f, 0), BL (F_LP, 1, 14000, 8000, 0.7f, 0, 0) } },
        { "GLOW", {   // barely there: warm preamp, the "put it on and it just sounds better" one
            BL (F_LS, 1, 200, 0.8f, 1.5f, 3.0f, 0), BL (S_TANH, 1, 0, 24, 0.25f, 0, 0), BL (F_HS, 1, 8000, 0.8f, -1.5f, -3.0f, 0) } },
        { "SIZZLE", {   // bright tube: highs get pushed into the curve harder than the rest
            BL (F_HS, 1, 3000, 0.8f, 3.0f, 7.0f, 0), BL (S_TANH, 1, 8, 42, 0.35f, 0, 0), BL (F_HS, 1, 3000, 0.8f, -3.0f, -7.0f, 0),
            BL (F_LP, 1, 16000, 11000, 0.7f, 0, 0) } },
        { "CRUNCH", {   // amp in a box: tight lows, mid push, speaker roll-off
            BL (F_HP, 1, 90, 180, 0.7f, 0, 0), BL (F_PK, 1, 1800, 0.8f, 2.0f, 5.0f, 0), BL (S_TANH, 1, 10, 38, 0.12f, 0, 0),
            BL (F_PK, 1, 250, 0.8f, 1.5f, 2.5f, 0), BL (F_LP, 1, 6500, 4200, 0.7f, 0, 0) } },
        { "BRIT", {   // old British stack: mids up front, hard clip, presence
            BL (F_HP, 1, 120, 120, 0.7f, 0, 0), BL (F_PK, 1, 1200, 1.0f, 4.0f, 7.0f, 0), BL (S_CLIP, 1, 14, 46, 3.0f, 0, 0),
            BL (F_PK, 1, 3200, 1.2f, 2.0f, 4.0f, 0), BL (F_LP, 1, 5200, 4000, 0.8f, 0, 0) } },
        { "TWEED", {   // low-gain old amp: bright, soft, sags when you hit it
            BL (F_HS, 1, 2500, 0.8f, 3.0f, 3.0f, 0), BL (S_SAG, 1, 6, 32, 0.35f, 0.4f, 0), BL (F_LP, 1, 5800, 5800, 0.7f, 0, 0) } },
        { "SCREAM", {   // overdrive pedal: lows stay clean, the mids scream
            BL (BAND_BEGIN, 1, 1, 220, 0, 1.0f, 0), BL (F_PK, 1, 720, 0.7f, 8.0f, 11.0f, 0), BL (S_DIODE, 1, 10, 40, 0.75f, 0, 0),
            BL (F_LP, 1, 4200, 3000, 0.7f, 0, 0), BL (BAND_END, 1, 0, 0, 0, 0, 0) } },
        { "FUZZ", {   // germanium fuzz: asymmetric, spitty, sustains forever
            BL (F_HP, 1, 70, 70, 0.7f, 0, 0), BL (S_FUZZ, 1, 16, 50, 0.3f, 0.02f, 0), BL (F_PK, 1, 500, 0.8f, 3.0f, 3.0f, 0),
            BL (F_LP, 1, 5800, 5800, 0.8f, 0, 0) } },
        { "WALL", {   // two clipping stages with the mids scooped out: a wall of sustain
            BL (F_HP, 1, 60, 60, 0.7f, 0, 0), BL (S_CLIP, 1, 14, 42, 2.0f, 0, 0), BL (F_LP, 1, 5000, 5000, 0.7f, 0, 0),
            BL (S_CLIP, 1, 12, 30, 4.0f, 0, 0), BL (F_PK, 1, 900, 0.6f, -7.0f, -10.0f, 0), BL (F_LP, 1, 6000, 3500, 0.7f, 0, 0) } },
        { "AIR", {   // only the top gets distorted: sizzle and shine for a vocal
            BL (BAND_BEGIN, 1, 1, 2800, 0, 1.0f, 0), BL (S_TANH, 1, 6, 36, 0.1f, 0, 0), BL (F_LP, 1, 17000, 17000, 0.7f, 0, 0),
            BL (BAND_END, 1, 0, 0, 0, 0, 0) } },
        { "BODY", {   // only the low-mids get distorted: weight and chest
            BL (BAND_BEGIN, 1, 2, 100, 450, 1.0f, 0), BL (S_TANH, 1, 8, 38, 0.3f, 0, 0), BL (BAND_END, 1, 0, 0, 0, 0, 0) } },
        { "EXCITER", {   // synthesises clean 2nd and 3rd harmonics of the top end
            BL (BAND_BEGIN, 1, 1, 3500, 0, 1.0f, 0), BL (S_CHEB, 1, 0, 14, 0.5f, 0.35f, 0), BL (F_HP, 1, 2500, 2500, 0.7f, 0, 0),
            BL (F_LP, 1, 16000, 16000, 0.7f, 0, 0), BL (BAND_END, 1, 0, 0, 0, 0, 0) } },
        { "RADIO", {   // band-limited and honky: hip-hop vocal on a transistor radio
            BL (F_HP, 1, 420, 700, 0.8f, 0, 0), BL (F_LP, 1, 3800, 2800, 0.8f, 0, 0), BL (F_PK, 1, 1500, 1.2f, 4.0f, 4.0f, 0),
            BL (S_TANH, 1, 8, 36, 0.1f, 0, 0), BL (F_HP, 1, 300, 300, 0.7f, 0, 0), BL (F_LP, 1, 4200, 3200, 0.7f, 0, 0) } },
        { "CLIPPER", {   // clean and tight, shaves peaks and nothing else
            BL (S_CLIP, 1, 0, 24, 8.0f, 0, 0), BL (F_LP, 1, 19000, 19000, 0.7f, 0, 0) } },

        //------------------------------------------------ 20 strange: motion, filters, folding
        { "WOBBLE", {   // a resonant low-pass wobbles in time with the song, then hits the curve
            BL (X_WOB, 0, 160, 2800, 3.0f, 2.0f, 0), BL (S_TANH, 1, 6, 34, 0.05f, 0, 0), BL (F_LP, 1, 9000, 6000, 0.7f, 0, 0) } },
        { "WUB", {   // faster, squarer, meaner wobble with a hard knee and extra sub
            BL (X_WOB, 0, 90, 4200, 6.0f, 0.5f, 2), BL (S_PENT, 1, 10, 42, 3.0f, 0.1f, 0), BL (F_PK, 1, 100, 0.8f, 3.0f, 5.0f, 0),
            BL (F_LP, 1, 9000, 7000, 0.7f, 0, 0) } },
        { "FLANGUS", {   // slow flanger before the distortion: the comb notches get crushed into new harmonics
            BL (X_FLG, 0, 0.5f, 5.0f, 0.7f, 8.0f, 1.0f), BL (S_TANH, 1, 8, 36, 0.1f, 0, 0), BL (F_HS, 1, 6000, 0.8f, -2.0f, -5.0f, 0) } },
        { "JET", {   // flanger after the distortion with strong feedback: a jet engine
            BL (S_PENT, 1, 6, 34, 3.0f, 0.05f, 0), BL (F_HP, 1, 100, 100, 0.7f, 0, 0), BL (X_FLG, 2, 0.3f, 3.5f, 0.88f, 4.0f, 0.9f) } },
        { "PHASER", {   // six-stage phaser into a fuzz
            BL (X_PHS, 0, 200, 3500, 0.6f, 4.0f, 6), BL (S_FUZZ, 1, 14, 44, 0.25f, 0.01f, 0), BL (F_LP, 1, 6500, 4500, 0.7f, 0, 0) } },
        { "TALKBOX", {   // hard clip, then vowel filters that morph A-E-I-O-U in time with the song
            BL (F_HP, 1, 100, 100, 0.7f, 0, 0), BL (S_CLIP, 1, 10, 38, 3.0f, 0, 0), BL (X_VOW, 2, 0, 5, 1.0f, 4.0f, 0.9f) } },
        { "VOWEL", {   // the same idea parked on an "oh": a growl
            BL (S_PENT, 1, 8, 38, 3.0f, 0.05f, 0), BL (X_VOW, 2, 3, 0, 1.1f, 4.0f, 0.85f) } },
        { "AUTOWAH", {   // the harder you sing or play, the higher the filter opens
            BL (X_WAH, 0, 250, 3200, 4.0f, 1.0f, 0), BL (S_DIODE, 1, 8, 36, 0.8f, 0, 0), BL (F_LP, 1, 6000, 4500, 0.7f, 0, 0) } },
        { "FOLD", {   // sine wave folder: the peaks turn over instead of clipping
            BL (F_HP, 1, 40, 40, 0.7f, 0, 0), BL (S_FOLDS, 1, 3, 30, 0, 0, 0), BL (F_LP, 1, 12000, 7000, 0.7f, 0, 0) } },
        { "FOLDBACK", {   // triangle foldback: brittle and metallic
            BL (F_HP, 1, 60, 60, 0.7f, 0, 0), BL (S_FOLDT, 1, 4, 34, 0, 0, 0), BL (S_TANH, 1, 0, 0, 0, 0, 0), BL (F_LP, 1, 11000, 6500, 0.7f, 0, 0) } },
        { "RECTIFY", {   // full-wave rectifier: octave-up fuzz
            BL (F_HP, 1, 50, 50, 0.7f, 0, 0), BL (S_RECT, 1, 6, 30, 0.5f, 0.95f, 0), BL (F_LP, 1, 9000, 9000, 0.7f, 0, 0) } },
        { "CRUSH", {   // bit depth and sample rate fall as you turn it up
            BL (X_CRUSH, 0, 12, 4, 1.5f, 10.0f, 0), BL (S_TANH, 1, 0, 14, 0, 0, 0), BL (F_LP, 1, 10000, 7000, 0.7f, 0, 0) } },
        { "SLEW", {   // slew limiting: loud highs can't keep up, so everything goes dark and gritty
            BL (S_SLEW, 1, 10, 40, 0.5f, 0.05f, 0), BL (F_LP, 1, 12000, 12000, 0.7f, 0, 0) } },
        { "METAL", {   // a tuned comb rings after the distortion: pitched, metallic
            BL (S_PENT, 1, 6, 32, 3.0f, 0.05f, 0), BL (X_COMB, 2, 3.6f, 0.78f, 6000, 0.9f, 0) } },
        { "DALEK", {   // ring modulation, and the carrier climbs as you turn it up
            BL (X_RING, 0, 40, 400, 0.8f, 0, 0), BL (S_CLIP, 1, 8, 34, 3.0f, 0, 0), BL (F_LP, 1, 8000, 6000, 0.7f, 0, 0) } },
        { "CROSS", {   // crossover distortion: dead zone around zero, crackly and gritty
            BL (S_XOVER, 1, 8, 38, 0.08f, 0.30f, 0), BL (F_LP, 1, 9000, 9000, 0.7f, 0, 0) } },
        { "SAG", {   // a starved power supply: bias drifts with the level, it chokes and sputters
            BL (F_HP, 1, 40, 40, 0.7f, 0, 0), BL (S_SAG, 1, 10, 44, 0.9f, 0.8f, 0), BL (F_LP, 1, 7000, 7000, 0.7f, 0, 0) } },
        { "TREMOR", {   // tremolo before the curve: the distortion pulses in time
            BL (X_TREM, 0, 0.85f, 0.5f, 0, 0, 0), BL (S_TANH, 1, 10, 36, 0.15f, 0, 0), BL (F_LP, 1, 8000, 6000, 0.7f, 0, 0) } },
        { "SIREN", {   // a band-pass sweeps through the distorted signal in time with the song
            BL (S_TANH, 1, 8, 34, 0.1f, 0, 0), BL (X_SIREN, 2, 300, 4200, 2.8f, 1.0f, 0) } },
        { "HELL", {   // everything at once: two hard clips and a crusher
            BL (F_HP, 1, 120, 120, 0.7f, 0, 0), BL (S_CLIP, 1, 24, 54, 2.0f, 0, 0), BL (F_LP, 1, 6000, 6000, 0.7f, 0, 0),
            BL (S_CLIP, 1, 10, 20, 6.0f, 0, 0), BL (X_CRUSH, 2, 8, 6, 1.0f, 3.0f, 0) } },
    };
    #undef BL

    //==========================================================================================================
    // Building blocks
    //==========================================================================================================
    inline double dbToLin (double db) { return std::pow (10.0, db / 20.0); }
    inline double lerp (double a, double b, double t) { return a + (b - a) * t; }
    inline double logLerp (double a, double b, double t) { return std::exp (lerp (std::log (std::max (a, 1.0e-3)), std::log (std::max (b, 1.0e-3)), t)); }

    // u / (1 + |u|^n)^(1/n): a soft clip whose knee gets sharper with n. Whole-number n take fast paths.
    inline double clipN (double u, double n)
    {
        const double a = std::abs (u);
        if (n == 2.0) return u / std::sqrt (1.0 + u * u);
        if (n == 3.0) return u / std::cbrt (1.0 + a * a * a);
        if (n == 4.0) { const double t = u * u; return u / std::sqrt (std::sqrt (1.0 + t * t)); }
        if (n == 6.0) { const double t = u * u, t3 = t * t * t; return u / std::cbrt (std::sqrt (1.0 + t3)); }
        if (n == 8.0) { double t = u * u; t *= t; t *= t; return u / std::sqrt (std::sqrt (std::sqrt (1.0 + t))); }
        return u / std::pow (1.0 + std::pow (a, n), 1.0 / n);
    }

    struct BQ
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        inline double process (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
        void reset() { z1 = z2 = 0; }
        void set (double B0, double B1, double B2, double A0, double A1, double A2) { b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0; }
        static double clampF (double fs, double f) { return std::min (std::max (f, 5.0), fs * 0.45); }
        void lowpass (double fs, double f, double Q)
        {
            const double w = 2 * kPi * clampF (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * std::max (Q, 0.1));
            set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
        }
        void highpass (double fs, double f, double Q)
        {
            const double w = 2 * kPi * clampF (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * std::max (Q, 0.1));
            set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
        }
        void bandpass (double fs, double f, double Q)
        {
            const double w = 2 * kPi * clampF (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * std::max (Q, 0.1));
            set (al, 0, -al, 1 + al, -2 * c, 1 - al);
        }
        void peaking (double fs, double f, double Q, double dB)
        {
            const double A = std::pow (10.0, dB / 40.0), w = 2 * kPi * clampF (fs, f) / fs, al = std::sin (w) / (2 * std::max (Q, 0.1)), c = std::cos (w);
            set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
        }
        void highShelf (double fs, double f, double dB, double S)
        {
            const double A = std::pow (10.0, dB / 40.0), w = 2 * kPi * clampF (fs, f) / fs, c = std::cos (w), sn = std::sin (w);
            const double al = sn / 2 * std::sqrt (std::max (0.0, (A + 1 / A) * (1 / S - 1) + 2)), r = 2 * std::sqrt (A) * al;
            set (A * ((A + 1) + (A - 1) * c + r), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - r),
                 (A + 1) - (A - 1) * c + r, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - r);
        }
        void lowShelf (double fs, double f, double dB, double S)
        {
            const double A = std::pow (10.0, dB / 40.0), w = 2 * kPi * clampF (fs, f) / fs, c = std::cos (w), sn = std::sin (w);
            const double al = sn / 2 * std::sqrt (std::max (0.0, (A + 1 / A) * (1 / S - 1) + 2)), r = 2 * std::sqrt (A) * al;
            set (A * ((A + 1) - (A - 1) * c + r), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - r),
                 (A + 1) + (A - 1) * c + r, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - r);
        }
    };

    // Topology-preserving state variable filter: safe to modulate every sample.
    struct SVF
    {
        double ic1 = 0, ic2 = 0, g = 0.1, k = 1, a1 = 1, a2 = 0, a3 = 0;
        void reset() { ic1 = ic2 = 0; }
        void set (double fs, double f, double Q)
        {
            g = std::tan (kPi * std::min (std::max (f, 10.0), fs * 0.45) / fs); k = 1.0 / std::max (Q, 0.3);
            a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        inline void process (double x, double& lp, double& bp, double& hp)
        {
            const double v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
            bp = v1; lp = v2; hp = x - k * v1 - v2;
        }
    };

    // Halfband FIR (Kaiser window) shared by every oversampling stage.
    struct Halfband
    {
        int M = 0, J = 0; double c[32] = {};
        static double bessel0 (double x) { double s = 1, t = 1; for (int k = 1; k < 40; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; } return s; }
        void design (int m, double beta)
        {
            M = m; J = (M + 1) / 2;
            for (int j = 0; j < J; ++j)
            {
                const int d = 2 * j + 1; const double t = (double) d / (double) M;
                const double w = bessel0 (beta * std::sqrt (std::max (0.0, 1 - t * t))) / bessel0 (beta);
                c[j] = std::sin (kPi * d / 2.0) / (kPi * d) * w;
            }
        }
    };

    struct Up2
    {
        const Halfband* hb = nullptr; double h[64] = {}; int pos = 0;
        void reset() { std::memset (h, 0, sizeof h); pos = 0; }
        inline double at (int k) const { return h[(pos + k) & 63]; }
        inline void process (double x, double& y0, double& y1)
        {
            pos = (pos - 1) & 63; h[pos] = x;
            const int M = hb->M; double s = 0;
            for (int j = 0; j < hb->J; ++j) { const int d = 2 * j + 1; s += 2.0 * hb->c[j] * (at ((M - d) / 2) + at ((M + d) / 2)); }
            y0 = s; y1 = at ((M - 1) / 2);
        }
    };

    struct Down2
    {
        const Halfband* hb = nullptr; double h[128] = {}; int pos = 0;
        void reset() { std::memset (h, 0, sizeof h); pos = 0; }
        inline double at (int k) const { return h[(pos + k) & 127]; }
        inline double process (double v0, double v1)
        {
            pos = (pos - 1) & 127; h[pos] = v0; pos = (pos - 1) & 127; h[pos] = v1;
            const int M = hb->M; double s = 0.5 * at (M);
            for (int j = 0; j < hb->J; ++j) { const int d = 2 * j + 1; s += hb->c[j] * (at (M - d) + at (M + d)); }
            return s;
        }
    };

    struct OSDesign
    {
        Halfband s1, s2;      // 1x -> 2x, and 2x -> 4x
        double lat = 0;       // up + down together, in samples at the normal rate (measured from an impulse, so it is always right)
        OSDesign()
        {
            s1.design (39, 8.0); s2.design (11, 8.0);
            Up2 u1, u2a, u2b; Down2 d1, d2a, d2b;
            u1.hb = &s1; u2a.hb = u2b.hb = &s2; d1.hb = &s1; d2a.hb = d2b.hb = &s2;
            double sw = 0, swn = 0;
            for (int i = 0; i < 400; ++i)
            {
                const double x = i == 0 ? 1.0 : 0.0; double a0, a1, o[4];
                u1.process (x, a0, a1); u2a.process (a0, o[0], o[1]); u2b.process (a1, o[2], o[3]);
                const double b0 = d2a.process (o[0], o[1]), b1 = d2b.process (o[2], o[3]), y = d1.process (b0, b1);
                sw += i * y * y; swn += y * y;
            }
            lat = sw / swn;
            if (std::abs (lat - std::round (lat)) < 0.01) lat = std::round (lat);
        }
        double latency() const { return lat; }
    };
    inline const OSDesign& osDesign() { static const OSDesign d; return d; }

    //==========================================================================================================
    // One channel of one style
    //==========================================================================================================
    struct BS
    {
        BQ q[2]; SVF s[3];
        double z[8] = {};
        double g = 1, gT = 1, p0 = 0, p1 = 0, p2 = 0, p3 = 0, ph = 0, env = 0, env2 = 0, held = 0, acc = 0;
        double stash = 0;
    };

    inline double lfoShape (double ph, int shape)
    {
        ph -= std::floor (ph);
        if (shape == 1) return ph < 0.5 ? 2 * ph : 2 - 2 * ph;                                   // triangle 0..1
        if (shape == 2) return 0.5 + 0.5 * std::tanh (2.2 * std::sin (2 * kPi * ph)) / std::tanh (2.2);   // soft square
        return 0.5 - 0.5 * std::cos (2 * kPi * ph);                                                // sine 0..1
    }

    class Chan
    {
    public:
        Chan() : dl (8192, 0.0f) {}

        void prepare (double sr, int channel)
        {
            fs = sr; fsOS = sr * kOS; ch = channel; lfoOff = 0.12 * channel;
            const auto& d = osDesign();
            for (auto* u : { &up1, &up2a, &up2b }) u->reset();
            up1.hb = &d.s1; up2a.hb = &d.s2; up2b.hb = &d.s2;
            dn1.hb = &d.s1; dn2a.hb = &d.s2; dn2b.hb = &d.s2;
            dn1.reset(); dn2a.reset(); dn2b.reset();
            aEnvA = 1.0 - std::exp (-1.0 / (0.003 * fsOS)); aEnvR = 1.0 - std::exp (-1.0 / (0.15 * fsOS));
            aGain = 1.0 - std::exp (-1.0 / (0.004 * fsOS));
            dcR = std::exp (-2 * kPi * 12.0 / fs);
        }

        void setStyle (int idx)
        {
            style = &kStyles[std::min (std::max (idx, 0), kNumStyles - 1)];
            n = 0; while (n < kMaxBlocks && style->blk[n].type != B_END) ++n;
            for (auto& b : bs) b = BS();
            std::fill (dl.begin(), dl.end(), 0.0f); wp = 0;
            up1.reset(); up2a.reset(); up2b.reset(); dn1.reset(); dn2a.reset(); dn2b.reset();
            dcX = dcY = 0; first = true;
        }

        // drive in 0..1; called every few samples
        void updateCoefs (double dr)
        {
            for (int i = 0; i < n; ++i)
            {
                const Blk& k = style->blk[i]; BS& s = bs[i];
                const double rate = k.dom == 1 ? fsOS : fs;
                switch (k.type)
                {
                    case F_HP: s.q[0].highpass (rate, logLerp (k.a, k.b, dr), k.c); break;
                    case F_LP: s.q[0].lowpass  (rate, logLerp (k.a, k.b, dr), k.c); break;
                    case F_BP: s.q[0].bandpass (rate, logLerp (k.a, k.b, dr), k.c); break;
                    case F_PK: s.q[0].peaking  (rate, k.a, k.b, lerp (k.c, k.d, dr)); break;
                    case F_LS: s.q[0].lowShelf  (rate, k.a, lerp (k.c, k.d, dr), k.b); break;
                    case F_HS: s.q[0].highShelf (rate, k.a, lerp (k.c, k.d, dr), k.b); break;
                    case BAND_BEGIN:
                        if (k.a < 1.5f) { s.q[0].lowpass (rate, k.b, 0.7071); s.q[1].reset(); }
                        else { s.q[0].highpass (rate, k.b, 0.7071); s.q[1].lowpass (rate, k.c, 0.7071); }
                        break;
                    case S_TANH: s.gT = dbToLin (lerp (k.a, k.b, dr)); s.p0 = k.c; s.p1 = std::tanh (k.c); break;
                    case S_PENT: s.gT = dbToLin (lerp (k.a, k.b, dr)); s.p0 = k.c; s.p1 = k.d; s.p2 = clipN (k.d, k.c); break;
                    case S_DIODE: s.gT = dbToLin (lerp (k.a, k.b, dr)); s.p0 = k.c; break;
                    case S_FUZZ: s.gT = dbToLin (lerp (k.a, k.b, dr)); s.p0 = k.c; s.p1 = k.d; s.p2 = std::tanh (2.2 * k.c); break;
                    case S_CLIP: s.gT = dbToLin (lerp (k.a, k.b, dr)); s.p0 = k.c; break;
                    case S_FOLDS: case S_FOLDT: s.gT = dbToLin (lerp (k.a, k.b, dr)); break;
                    case S_RECT: s.gT = dbToLin (lerp (k.a, k.b, dr)); s.p0 = lerp (k.c, k.d, dr); break;
                    case S_XOVER: s.gT = dbToLin (lerp (k.a, k.b, dr)); s.p0 = lerp (k.c, k.d, dr); break;
                    case S_SAG: s.gT = dbToLin (lerp (k.a, k.b, dr)); s.p0 = k.c; s.p1 = k.d; break;
                    case S_CHEB: { s.gT = dbToLin (lerp (k.a, k.b, dr)); const double amt = 0.25 + 0.75 * dr; s.p0 = k.c * amt; s.p1 = k.d * amt; break; }
                    case S_SLEW: s.gT = dbToLin (lerp (k.a, k.b, dr)); s.p0 = lerp (k.c, k.d, dr) * 192000.0 / fsOS; break;
                    case X_RING: s.p0 = 2 * kPi * logLerp (k.a, k.b, dr) / fs; break;
                    case X_CRUSH: s.p0 = std::pow (2.0, lerp (k.a, k.b, dr) - 1.0); s.p1 = lerp (k.c, k.d, dr); break;
                    default: break;
                }
                if (first) s.g = s.gT;
            }
            first = false;
        }

        inline float process (float xin, double beat)
        {
            double x = xin;
            for (int i = 0; i < n; ++i) if (style->blk[i].dom == 0) x = runBase (i, x, beat);
            // up x4
            double a0, a1, b0, b1, os[4];
            up1.process (x, a0, a1);
            up2a.process (a0, os[0], os[1]); up2b.process (a1, os[2], os[3]);
            for (int j = 0; j < 4; ++j)
            {
                double v = os[j];
                for (int i = 0; i < n; ++i) if (style->blk[i].dom == 1) v = runOS (i, v);
                os[j] = v;
            }
            b0 = dn2a.process (os[0], os[1]); b1 = dn2b.process (os[2], os[3]);
            x = dn1.process (b0, b1);
            for (int i = 0; i < n; ++i) if (style->blk[i].dom == 2) x = runBase (i, x, beat);
            // DC blocker
            const double y = x - dcX + dcR * dcY; dcX = x; dcY = y;
            if (! std::isfinite (y)) { setStyle ((int) (style - kStyles)); return 0.0f; }
            return (float) std::min (std::max (y, -16.0), 16.0);
        }

    private:
        //---------------------------------------------------------------- oversampled blocks
        inline double runOS (int i, double x)
        {
            const Blk& k = style->blk[i]; BS& s = bs[i];
            switch (k.type)
            {
                case F_HP: case F_LP: case F_BP: case F_PK: case F_LS: case F_HS: return s.q[0].process (x);
                case BAND_BEGIN:
                {
                    if (k.a < 1.5f)
                    {
                        const double lo = s.q[0].process (x);
                        if (k.a < 0.5f) { s.stash = x - lo; return lo; }       // process lows, keep highs aside
                        s.stash = lo; return x - lo;                             // process highs, keep lows aside
                    }
                    const double band = s.q[1].process (s.q[0].process (x));
                    s.stash = x - band; return band;
                }
                case BAND_END:
                {
                    for (int j = i - 1; j >= 0; --j) if (style->blk[j].type == BAND_BEGIN) return x + style->blk[j].d * bs[j].stash;
                    return x;
                }
                default: break;
            }
            s.g += aGain * (s.gT - s.g);
            const double u = s.g * x;
            switch (k.type)
            {
                case S_TANH: return std::tanh (u + s.p0) - s.p1;
                case S_PENT: return clipN (u + s.p1, s.p0) - s.p2;
                case S_DIODE: return u >= 0 ? 1 - std::exp (-u) : -s.p0 * (1 - std::exp (u / s.p0));
                case S_FUZZ:
                {
                    const double v = u + s.p0, y = std::tanh ((v > 0 ? 2.2 : 1.0) * v) - s.p2;
                    return y * (std::abs (u) / (std::abs (u) + s.p1 + 1.0e-9));
                }
                case S_CLIP: return clipN (u, s.p0);
                case S_FOLDS: return std::sin (0.5 * kPi * std::min (std::max (u, -400.0), 400.0));
                case S_FOLDT: { const double t = 0.25 * u + 0.25; return 4.0 * std::abs (t - std::round (t)) - 1.0; }
                case S_RECT: return std::tanh (u * (1 - s.p0) + std::abs (u) * s.p0);
                case S_XOVER:
                {
                    const double a = std::abs (u) - s.p0;
                    return a <= 0 ? 0.0 : std::tanh ((u > 0 ? a : -a) * (1 + s.p0));
                }
                case S_SAG:
                {
                    const double au = std::abs (u);
                    s.env += (au > s.env ? aEnvA : aEnvR) * (au - s.env);
                    const double e = std::min (s.env, 1.5), bias = s.p0 * e;
                    return std::tanh (u / (1 + s.p1 * e) + bias) - std::tanh (bias);
                }
                case S_CHEB:
                {
                    const double v = std::tanh (u);
                    return v + s.p0 * (2 * v * v - 1) + s.p1 * (4 * v * v * v - 3 * v);
                }
                case S_SLEW:
                {
                    double d = u - s.env2; d = std::min (std::max (d, -s.p0), s.p0); s.env2 += d;
                    return std::tanh (0.9 * s.env2);
                }
                default: return x;
            }
        }

        //---------------------------------------------------------------- normal-rate blocks
        inline double phaseFor (double beat, double beats) const { return beat / std::max (beats, 1.0e-3) + lfoOff; }

        inline double runBase (int i, double x, double beat)
        {
            const Blk& k = style->blk[i]; BS& s = bs[i];
            switch (k.type)
            {
                case F_HP: case F_LP: case F_BP: case F_PK: case F_LS: case F_HS: return s.q[0].process (x);

                case X_WOB: case X_SIREN:   // a = Hz low, b = Hz high, c = Q, d = beats per cycle, e = lfo shape
                {
                    const double l = lfoShape (phaseFor (beat, k.d), (int) k.e);
                    s.s[0].set (fs, k.a * std::pow ((double) k.b / k.a, l), k.c);
                    double lp, bp, hp; s.s[0].process (x, lp, bp, hp);
                    if (k.type == X_WOB) return lp;
                    return 0.65 * s.s[0].k * bp + 0.35 * x;
                }
                case X_FLG:   // a = ms min, b = ms max, c = feedback, d = beats per cycle, e = wet
                {
                    const double l = lfoShape (phaseFor (beat, k.d), 0);
                    const double dly = (k.a + (k.b - k.a) * l) * 0.001 * fs;
                    double rp = (double) wp - dly; while (rp < 0) rp += 8192.0;
                    const int i0 = (int) rp & 8191, i1 = (i0 + 1) & 8191; const double fr = rp - std::floor (rp);
                    const double del = dl[(size_t) i0] * (1 - fr) + dl[(size_t) i1] * fr;
                    s.z[0] += 0.5 * (del - s.z[0]);                               // damp the feedback a little
                    dl[(size_t) wp] = (float) (x + std::tanh (k.c * s.z[0]));
                    wp = (wp + 1) & 8191;
                    return x + k.e * del;
                }
                case X_PHS:   // a = Hz low, b = Hz high, c = feedback, d = beats per cycle, e = stages (4 or 6)
                {
                    const double l = lfoShape (phaseFor (beat, k.d), 0);
                    const double t = std::tan (kPi * std::min (k.a * std::pow ((double) k.b / k.a, l), fs * 0.45) / fs), a = (t - 1) / (t + 1);
                    double v = x + k.c * s.z[7];
                    const int st = k.e > 5 ? 6 : 4;
                    for (int j = 0; j < st; ++j) { const double y = a * v + s.z[j]; s.z[j] = v - a * y; v = y; }
                    s.z[7] = std::tanh (v);
                    return 0.5 * (x + v);
                }
                case X_WAH:   // a = Hz low, b = Hz high, c = Q, d = sensitivity
                {
                    // fast envelope against a slow average of the level: it opens when you play louder than usual
                    const double ax = std::abs (x), cA = 1.0 - std::exp (-1.0 / (0.004 * fs)), cR = 1.0 - std::exp (-1.0 / (0.12 * fs));
                    s.env += (ax > s.env ? cA : cR) * (ax - s.env);
                    s.env2 += (1.0 - std::exp (-1.0 / (1.5 * fs))) * (ax - s.env2);
                    const double r = std::min (1.0, k.d * 0.5 * s.env / (s.env2 + 1.0e-4));
                    s.p0 += (1.0 - std::exp (-1.0 / (0.02 * fs))) * (r - s.p0);
                    s.s[0].set (fs, k.a * std::pow ((double) k.b / k.a, s.p0), k.c);
                    double lp, bp, hp; s.s[0].process (x, lp, bp, hp);
                    return 1.3 * s.s[0].k * bp + 0.12 * x;
                }
                case X_VOW:   // a = start vowel, b = how far it travels (0 = parked), c = Q scale, d = beats per cycle, e = wet
                {
                    static const double F[6][3] = { { 800, 1150, 2900 }, { 400, 1700, 2600 }, { 270, 2300, 3000 },
                                                    { 450, 800, 2830 }, { 325, 700, 2530 }, { 800, 1150, 2900 } };
                    double pos = k.a;
                    if (k.b > 0.001f) pos = std::fmod (k.a + k.b * (phaseFor (beat, k.d) - std::floor (phaseFor (beat, k.d))), 5.0);
                    const int v0 = std::min (4, (int) pos); const double fr = pos - v0;
                    double out = 0; static const double amp[3] = { 1.0, 0.6, 0.35 }, q[3] = { 6, 9, 12 };
                    for (int j = 0; j < 3; ++j)
                    {
                        s.s[j].set (fs, std::exp (lerp (std::log (F[v0][j]), std::log (F[v0 + 1][j]), fr)), q[j] * k.c);
                        double lp, bp, hp; s.s[j].process (x, lp, bp, hp); out += amp[j] * s.s[j].k * bp;
                    }
                    return (1 - k.e) * x + k.e * 2.2 * out;
                }
                case X_RING:   // a/b = carrier Hz at drive 0/1, c = blend
                {
                    s.ph += s.p0; if (s.ph > 2 * kPi) s.ph -= 2 * kPi;
                    return x * (1 - k.c) + x * std::sin (s.ph + lfoOff) * k.c;
                }
                case X_TREM:   // a = depth, b = beats per cycle, c = shape
                    return x * (1 - k.a * lfoShape (phaseFor (beat, k.b), (int) k.c));
                case X_CRUSH:   // bits and rate divider come from the drive (p0 = levels, p1 = divider)
                {
                    s.acc += 1.0;
                    if (s.acc >= s.p1) { s.acc -= s.p1; s.held = std::round (x * s.p0) / s.p0; }
                    return s.held;
                }
                case X_COMB:   // a = ms, b = feedback, c = damp Hz, d = wet
                {
                    const double dly = k.a * 0.001 * fs;
                    double rp = (double) wp - dly; while (rp < 0) rp += 8192.0;
                    const int i0 = (int) rp & 8191, i1 = (i0 + 1) & 8191; const double fr = rp - std::floor (rp);
                    const double del = dl[(size_t) i0] * (1 - fr) + dl[(size_t) i1] * fr;
                    const double c = 1.0 - std::exp (-2 * kPi * k.c / fs);
                    s.z[0] += c * (del - s.z[0]);
                    dl[(size_t) wp] = (float) (x + std::tanh (k.b * s.z[0]));
                    wp = (wp + 1) & 8191;
                    return x + k.d * del;
                }
                default: return x;
            }
        }

        double fs = 48000, fsOS = 192000, lfoOff = 0, aEnvA = 0.01, aEnvR = 0.0001, aGain = 0.01, dcR = 0.998, dcX = 0, dcY = 0;
        int ch = 0, n = 0, wp = 0; bool first = true;
        const StyleDef* style = &kStyles[0];
        BS bs[kMaxBlocks];
        std::vector<float> dl;
        Up2 up1, up2a, up2b; Down2 dn1, dn2a, dn2b;
    };

    //==========================================================================================================
    // Engine: two voices for click-free style changes, dry delay, auto gain, mix, output
    //==========================================================================================================
    struct Params
    {
        double drive = 0.3, mix = 1.0, outDb = 0.0;
        int style = 0;
        bool autoGain = true, on = true;
        double bpm = 120.0, ppq = -1.0; bool playing = false;
    };

    class Engine
    {
    public:
        static int latencySamples() { return (int) std::ceil (osDesign().latency()); }

        void prepare (double sr)
        {
            fs = sr;
            for (int v = 0; v < 2; ++v) for (int c = 0; c < 2; ++c) voice[v][c].prepare (fs, c);
            cur = target; active = 0;
            for (int c = 0; c < 2; ++c) voice[0][c].setStyle (cur.style);
            fading = false; fade = 1.0;
            for (int c = 0; c < 2; ++c) { voice[0][c].updateCoefs (cur.drive); }
            aSm = 1.0 - std::exp (-1.0 / (0.02 * fs)); aOn = 1.0 - std::exp (-1.0 / (0.01 * fs));
            aPow = 1.0 - std::exp (-1.0 / (0.15 * fs)); aGainSm = 1.0 - std::exp (-1.0 / (0.10 * fs));
            fadeStep = 1.0 / (0.025 * fs);
            // dry path: fractional delay matching the oversamplers
            const double T = osDesign().latency();
            dryInt = std::abs (T - std::round (T)) < 1.0e-6; dryDelay = (int) std::round (T);
            if (! dryInt)       // half-sample latencies: a short windowed-sinc fractional delay
            {
                const int H = (int) std::ceil (T) + 4;
                dryLen = 2 * H + 1; dryTaps.assign ((size_t) dryLen, 0.0); double sum = 0;
                for (int k = 0; k < dryLen; ++k)
                {
                    const double x = k - T, w = 0.42 + 0.5 * std::cos (kPi * x / (H + 0.5)) + 0.08 * std::cos (2 * kPi * x / (H + 0.5));
                    const double sinc = std::abs (x) < 1.0e-9 ? 1.0 : std::sin (kPi * x) / (kPi * x);
                    dryTaps[(size_t) k] = std::abs (x) <= H + 0.5 ? sinc * w : 0.0; sum += dryTaps[(size_t) k];
                }
                for (auto& t : dryTaps) t /= sum;
            }
            for (int c = 0; c < 2; ++c) { dryBuf[c].assign (512, 0.0f); dryPos[c] = 0; }
            pIn = pOut = 0; agGain = 1; onMix = cur.on ? 1 : 0; counter = 0;
            beat = std::max (0.0, target.ppq);
        }

        void setParams (const Params& p) { target = p; }

        void process (float* l, float* r, int n)
        {
            if (target.style != cur.style) startFade (target.style);
            const double bpm = std::min (std::max (target.bpm, 30.0), 300.0);
            const double inc = bpm / 60.0 / fs;
            if (target.ppq >= 0 && target.playing) beat = target.ppq;
            float mIn = 0, mOut = 0;
            for (int i = 0; i < n; ++i)
            {
                cur.drive += aSm * (target.drive - cur.drive);
                cur.mix += aSm * (target.mix - cur.mix);
                cur.outDb += aSm * (target.outDb - cur.outDb);
                onMix += aOn * ((target.on ? 1.0 : 0.0) - onMix);
                if ((counter++ & 31) == 0)
                {
                    voice[active][0].updateCoefs (cur.drive); voice[active][1].updateCoefs (cur.drive);
                    if (fading) { voice[1 - active][0].updateCoefs (cur.drive); voice[1 - active][1].updateCoefs (cur.drive); }
                }
                const float dl0 = l[i], dr0 = r[i];
                float wl = voice[active][0].process (dl0, beat), wr = voice[active][1].process (dr0, beat);
                if (fading)
                {
                    const float ol = voice[1 - active][0].process (dl0, beat), orr = voice[1 - active][1].process (dr0, beat);
                    const float f = (float) fade;
                    wl = ol + f * (wl - ol); wr = orr + f * (wr - orr);
                    fade += fadeStep; if (fade >= 1.0) { fade = 1.0; fading = false; }
                }
                // auto gain: the wet signal is brought to the loudness of the input, slowly
                pIn += aPow * ((double) dl0 * dl0 + (double) dr0 * dr0 - pIn);
                pOut += aPow * ((double) wl * wl + (double) wr * wr - pOut);
                double tg = 1.0;
                if (target.autoGain && pIn > 1.0e-8 && pOut > 1.0e-10) tg = std::min (4.0, std::max (0.06, std::sqrt (pIn / pOut)));
                else if (target.autoGain) tg = agGain;                                  // quiet: hold
                agGain += aGainSm * (tg - agGain);
                const float g = (float) agGain;

                const float dryL = delayedDry (0, dl0), dryR = delayedDry (1, dr0);
                const double m = cur.mix * onMix, og = dbToLin (cur.outDb);
                float ol2 = (float) (og * (dryL + m * (wl * g - dryL))), or2 = (float) (og * (dryR + m * (wr * g - dryR)));
                ol2 = std::min (std::max (ol2, -32.0f), 32.0f); or2 = std::min (std::max (or2, -32.0f), 32.0f);
                l[i] = ol2; r[i] = or2;
                mIn = std::max (mIn, std::max (std::abs (dl0), std::abs (dr0)));
                mOut = std::max (mOut, std::max (std::abs (ol2), std::abs (or2)));
                beat += inc;
            }
            peakIn.store (mIn); peakOut.store (mOut);
        }

        std::atomic<float> peakIn { 0 }, peakOut { 0 };

    private:
        void startFade (int style)
        {
            const int nxt = 1 - active;
            for (int c = 0; c < 2; ++c) { voice[nxt][c].setStyle (style); voice[nxt][c].updateCoefs (cur.drive); }
            active = nxt; fading = true; fade = 0.0; cur.style = style;
        }
        inline float delayedDry (int c, float x)
        {
            dryBuf[c][(size_t) dryPos[c]] = x;
            double s = 0;
            if (dryInt) s = dryBuf[c][(size_t) ((dryPos[c] - dryDelay) & 511)];
            else for (int k = 0; k < dryLen; ++k) s += dryTaps[(size_t) k] * dryBuf[c][(size_t) ((dryPos[c] - k) & 511)];
            dryPos[c] = (dryPos[c] + 1) & 511;
            return (float) s;
        }
        double fs = 48000, aSm = 0.001, aOn = 0.01, aPow = 1e-4, aGainSm = 1e-4, fadeStep = 0.001, fade = 1.0;
        double pIn = 0, pOut = 0, agGain = 1, onMix = 1, beat = 0; unsigned counter = 0;
        int active = 0, dryLen = 73, dryDelay = 0, dryPos[2] = { 0, 0 }; bool fading = false, dryInt = true;
        std::vector<double> dryTaps; std::vector<float> dryBuf[2];
        Params target, cur;
        Chan voice[2][2];
    };
}
