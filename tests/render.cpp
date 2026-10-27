// Offline test renderer for the DSP engine.
// usage: render <mode> <out.raw> [style] [detune] [delayMs] [focusHz] [mix]
//   mode = sine   : 1 kHz sine, 20 s
//   mode = noise  : white noise with random parameter automation every block, 30 s
//   mode = file   : reads stereo float32 raw from stdin-equivalent path in argv[8]
// Output: interleaved stereo float32 raw at 48 kHz.
#include "../Source/TwinPitchDSP.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <random>

int main (int argc, char** argv)
{
    if (argc < 3) { std::fprintf (stderr, "usage\n"); return 1; }
    const std::string mode = argv[1];
    const double sr = 48000.0;
    twinpitch::Params p;
    p.style   = argc > 3 ? std::atoi (argv[3]) : 0;
    p.detune  = argc > 4 ? std::atof (argv[4]) : 1.0;
    p.delayMs = argc > 5 ? std::atof (argv[5]) : 10.0;
    p.focusHz = argc > 6 ? std::atof (argv[6]) : 20.0;
    p.mix     = argc > 7 ? std::atof (argv[7]) : 1.0;

    std::vector<float> L, R;
    if (mode == "sine" || mode == "sineauto")
    {
        const int n = (int) (sr * 20);
        L.resize (n); R.resize (n);
        for (int i = 0; i < n; ++i) L[i] = R[i] = (float) (0.5 * std::sin (2 * twinpitch::kPi * 1000.0 * i / sr));
    }
    else if (mode == "noise")
    {
        const int n = (int) (sr * 30);
        L.resize (n); R.resize (n);
        std::mt19937 g (42); std::uniform_real_distribution<float> u (-0.5f, 0.5f);
        for (int i = 0; i < n; ++i) { L[i] = u (g); R[i] = u (g); }
    }
    else if (mode == "file")
    {
        FILE* f = std::fopen (argv[8], "rb");
        if (! f) return 2;
        float fr[2];
        while (std::fread (fr, sizeof (float), 2, f) == 2) { L.push_back (fr[0]); R.push_back (fr[1]); }
        std::fclose (f);
    }

    twinpitch::Engine e;
    e.prepare (sr);
    e.setParams (p);

    std::mt19937 g (7); std::uniform_real_distribution<double> u (0.0, 1.0);
    const int block = 256;
    for (int pos = 0; pos < (int) L.size(); pos += block)
    {
        const int len = std::min (block, (int) L.size() - pos);
        if ((mode == "noise" || mode == "sineauto") && (pos / block) % 40 == 0)
        {
            twinpitch::Params q;
            q.style = (int) (u (g) * 3); q.detune = u (g); q.delayMs = u (g) * 40;
            q.focusHz = 20 + u (g) * 1980; q.mix = u (g); q.outputDb = mode == "sineauto" ? 0.0 : -24 + u (g) * 36;
            e.setParams (q);
        }
        e.process (L.data() + pos, R.data() + pos, len);
    }

    FILE* o = std::fopen (argv[2], "wb");
    for (size_t i = 0; i < L.size(); ++i) { float fr[2] = { L[i], R[i] }; std::fwrite (fr, sizeof (float), 2, o); }
    std::fclose (o);
    return 0;
}
