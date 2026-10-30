// Offline harness for the SECRET WEAPON engine. usage: render <melody|chord|silence> out.raw [key=value ...]
// keys: key scale source density styles bpm mix len(beats) range listen
#include "../Source/SecretWeaponDSP.h"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
using namespace secretweapon;
static double mtof (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }
int main (int argc, char** argv)
{
    std::string mode = argv[1];
    std::map<std::string, double> kv = { { "key", 9 }, { "scale", 1 }, { "source", 1 }, { "density", 1.0 }, { "styles", 63 }, { "bpm", 120 }, { "mix", 1 }, { "len", 0.25 }, { "range", 1 }, { "listen", 0 }, { "dur", 16 } };
    for (int i = 3; i < argc; ++i) { std::string s = argv[i]; auto e = s.find ('='); if (e != std::string::npos) kv[s.substr (0, e)] = atof (s.c_str() + e + 1); }
    const double sr = 48000; const int n = (int) (sr * kv["dur"]);
    std::vector<float> L ((size_t) n, 0.0f), R;
    // melody in A minor with one stray G# (68). notes of 0.35 s every 0.5 s, pluck envelope, 4 harmonics
    const int mel[] = { 69, 72, 76, 74, 72, 68, 69, 67, 69, 72, 74, 76, 79, 76, 72, 69 };
    if (mode == "melody")
        for (int k = 0; k < 16; ++k)
        {
            const double t0 = 0.3 + k * 0.5;
            for (int i = 0; i < (int) (0.45 * sr); ++i)
            {
                const size_t j = (size_t) (t0 * sr) + (size_t) i; if (j >= L.size()) break;
                const double t = i / sr, env = std::min (1.0, t / 0.005) * std::exp (-t / 0.25);
                double s = 0; for (int h = 1; h <= 4; ++h) s += std::sin (2 * kPi * mtof (mel[k]) * h * t) / h;
                L[j] += (float) (0.3 * env * s);
            }
        }
    if (mode == "chord")
        for (int k = 0; k < 8; ++k)
        {
            const double t0 = 0.3 + k * 1.0; const int ch[3] = { 57, 60, 64 };
            for (int i = 0; i < (int) (0.8 * sr); ++i)
            {
                const size_t j = (size_t) (t0 * sr) + (size_t) i; if (j >= L.size()) break;
                const double t = i / sr, env = std::min (1.0, t / 0.005) * std::exp (-t / 0.5);
                for (int c = 0; c < 3; ++c) L[j] += (float) (0.12 * env * std::sin (2 * kPi * mtof (ch[c]) * t));
            }
        }
    R = L;
    Engine e; e.prepare (sr);
    Params p; p.key = (int) kv["key"]; p.scale = (int) kv["scale"]; p.source = (int) kv["source"]; p.density = kv["density"]; p.styles = (unsigned) kv["styles"];
    p.bpm = kv["bpm"]; p.mix = kv["mix"]; p.lengthBeats = kv["len"]; p.rangeOct = kv["range"];
    e.setParams (p);
    std::vector<Engine::Fired> log; e.firedLog = &log;
    if (kv["listen"] > 0) e.startListen (kv["listen"]);
    for (int pos = 0; pos < n; pos += 256) e.process (L.data() + pos, R.data() + pos, std::min (256, n - pos));
    FILE* o = std::fopen (argv[2], "wb");
    for (int i = 0; i < n; ++i) { float fr[2] = { L[(size_t) i], R[(size_t) i] }; std::fwrite (fr, 4, 2, o); }
    std::fclose (o);
    std::printf ("latency %d\n", e.latencySamples());
    std::printf ("fired %zu\n", log.size());
    for (auto& f : log) std::printf ("T %.3f style %d src %.2f target %.2f ratio %.3f rev %d\n", f.at / sr, f.style, f.srcMidi, f.targetMidi, f.ratio, (int) f.reverse);
    if (kv["vizdump"] > 0) { int wi = e.viz.writeIndex.load(); for (int k = std::max (0, wi - 40); k < wi; ++k) { auto v = e.viz.ev[k & 255]; std::printf ("V kind %d start %.3f end %.3f midi %.2f\n", v.kind, v.start / sr, v.end / sr, v.midi); } }
    if (kv["listen"] > 0) std::printf ("HEARD %s %s cov %d state %d\n", keyName (e.heardKey.load()), getScale (e.heardScale.load()).name, e.heardPct.load(), e.listenState.load());
    return 0;
}
