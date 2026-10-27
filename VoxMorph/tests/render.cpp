// usage: render in.raw(mono f32) out.raw(mono f32) mode pitch formant link robotNote drive mix [automate]
#include "../Source/VoxMorphDSP.h"
#include <cstdio>
#include <cstdlib>
#include <random>
int main (int argc, char** argv)
{
    std::vector<float> x; FILE* f = std::fopen (argv[1], "rb"); float v;
    while (std::fread (&v, 4, 1, f) == 1) x.push_back (v); std::fclose (f);
    voxmorph::Params p;
    p.mode = std::atoi (argv[3]); p.pitch = std::atof (argv[4]); p.formant = std::atof (argv[5]);
    p.link = std::atoi (argv[6]) != 0; p.robotNote = std::atoi (argv[7]); p.drive = std::atof (argv[8]); p.mix = std::atof (argv[9]);
    const bool autom = argc > 10;
    voxmorph::Engine e; e.prepare (48000.0); e.setParams (p);
    std::fprintf (stderr, "latency %d\n", e.getLatencySamples());
    std::mt19937 g (3); std::uniform_real_distribution<double> u (0, 1);
    for (size_t pos = 0; pos < x.size(); pos += 512)
    {
        if (autom && (pos / 512) % 30 == 0)
        {
            voxmorph::Params q; q.mode = (int) (u (g) * 3); q.pitch = -12 + 24 * u (g); q.formant = -12 + 24 * u (g);
            q.link = u (g) > 0.5; q.robotNote = 36 + (int) (u (g) * 36); q.drive = u (g); q.mix = u (g); e.setParams (q);
        }
        const int len = (int) std::min<size_t> (512, x.size() - pos);
        e.process (x.data() + pos, x.data() + pos, len);
    }
    f = std::fopen (argv[2], "wb"); std::fwrite (x.data(), 4, x.size(), f); std::fclose (f);
}
