// usage: render <in.raw stereo f32> <out.raw> [key=value ...] [auto]
#include "../Source/GoatSlapDSP.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
int main (int argc, char** argv)
{
    std::vector<float> L, R; FILE* f = std::fopen (argv[1], "rb"); float fr[2];
    while (std::fread (fr, 4, 2, f) == 2) { L.push_back (fr[0]); R.push_back (fr[1]); }
    std::fclose (f);
    goatslap::Params p; bool autom = false;
    for (int i = 3; i < argc; ++i)
    {
        std::string a = argv[i]; if (a == "auto") { autom = true; continue; }
        auto e = a.find ('='); std::string k = a.substr (0, e); double v = std::atof (a.c_str() + e + 1);
        if (k == "time") p.timeMs = v; else if (k == "mix") p.mix = v; else if (k == "aux") p.aux = v > 0;
        else if (k == "dist") p.distance = v; else if (k == "room") p.room = v; else if (k == "tone") p.tone = v;
        else if (k == "width") p.width = v; else if (k == "fb") p.feedback = v; else if (k == "duckon") p.duckOn = v > 0;
        else if (k == "duck") p.duck = v; else if (k == "machine") p.machine = (int) v; else if (k == "speed") p.speed = (int) v;
        else if (k == "age") p.age = v; else if (k == "sat") p.saturate = v; else if (k == "heads") p.heads = (int) v;
        else if (k == "dbl") p.dbl = v > 0; else if (k == "throw") p.throwOn = v > 0; else if (k == "tipsy") p.tipsy = v;
        else if (k == "chaos") p.chaos = (uint32_t) v; else if (k == "feel") p.feelMs = v;
    }
    goatslap::Engine e; e.prepare (48000.0); e.setParams (p);
    goatslap::Rng g; g.s = 777;
    for (size_t pos = 0; pos < L.size(); pos += 256)
    {
        if (autom && (pos / 256) % 25 == 0)
        {
            goatslap::Params q; q.timeMs = 20 + g.uni() * 600; q.mix = g.uni(); q.aux = g.uni() > 0.7; q.distance = g.uni(); q.room = g.uni();
            q.tone = g.bi(); q.width = g.uni(); q.feedback = g.uni() * 0.9; q.duckOn = g.uni() > 0.5; q.duck = g.uni(); q.machine = (int) (g.uni() * 8);
            q.speed = (int) (g.uni() * 3); q.age = g.uni(); q.saturate = g.uni(); q.heads = 1 + (int) (g.uni() * 3); q.dbl = g.uni() > 0.5;
            q.throwOn = g.uni() > 0.8; q.tipsy = g.uni(); q.chaos = g.u32() & 31; q.feelMs = g.bi() * 20; e.setParams (q);
        }
        const int n = (int) std::min<size_t> (256, L.size() - pos);
        e.process (L.data() + pos, R.data() + pos, n);
    }
    f = std::fopen (argv[2], "wb");
    for (size_t i = 0; i < L.size(); ++i) { fr[0] = L[i]; fr[1] = R[i]; std::fwrite (fr, 4, 2, f); }
    std::fclose (f);
}
