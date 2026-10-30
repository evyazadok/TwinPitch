// Offline harness for the HANGOVER engine (no JUCE). usage:
//   render <mode> <out.raw> [key=value ...]
// modes: sine (1 kHz, 12 s), burst (sine bursts with silence), silence (10 s), chord (A minor triad)
// keys: mode wobble trem digital distort space drops mix mag tipsy chaos flux dflux bpm
#include "../Source/HangoverDSP.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <map>
int main (int argc, char** argv)
{
    if (argc < 3) return 1;
    std::string mode = argv[1];
    std::map<std::string, double> kv = { { "mode", 1 }, { "wobble", 0 }, { "trem", 0 }, { "digital", 0 }, { "distort", 0 }, { "space", 0 }, { "drops", 0 },
                                         { "mix", 1 }, { "mag", 1 }, { "tipsy", 0 }, { "chaos", 31 }, { "flux", 0 }, { "dflux", 0 }, { "bpm", 120 }, { "pan", 1 }, { "div", 0.5 } };
    for (int i = 3; i < argc; ++i) { std::string s = argv[i]; auto e = s.find ('='); if (e != std::string::npos) kv[s.substr (0, e)] = atof (s.c_str() + e + 1); }
    const double sr = 48000.0;
    const int n = (int) (sr * (mode == "silence" ? 10 : 12));
    std::vector<float> L ((size_t) n), R ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr; float s = 0;
        if (mode == "sine") s = (float) (0.4 * std::sin (2 * hangover::kPi * 1000 * t));
        else if (mode == "burst") { const double ph = std::fmod (t, 3.0); s = ph < 1.5 ? (float) (0.4 * std::sin (2 * hangover::kPi * 440 * t)) : 0.0f; }
        else if (mode == "chord") s = (float) (0.15 * (std::sin (2 * hangover::kPi * 220 * t) + std::sin (2 * hangover::kPi * 261.63 * t) + std::sin (2 * hangover::kPi * 329.63 * t)));
        L[(size_t) i] = R[(size_t) i] = s;
    }
    hangover::Engine e; e.prepare (sr);
    hangover::Params p;
    p.mode = (int) kv["mode"]; p.wobble = kv["wobble"]; p.trem = kv["trem"]; p.digital = kv["digital"]; p.distort = kv["distort"]; p.space = kv["space"]; p.drops = kv["drops"];
    p.mix = kv["mix"]; p.magnitude = kv["mag"]; p.tipsy = kv["tipsy"]; p.chaos = (unsigned) kv["chaos"];
    p.wobbleFlux = kv["flux"] > 0.5; p.dropsFlux = kv["dflux"] > 0.5; p.bpm = kv["bpm"]; p.tremPan = kv["pan"] > 0.5; p.tremBeats = kv["div"];
    e.setParams (p);
    for (int pos = 0; pos < n; pos += 256) e.process (L.data() + pos, R.data() + pos, std::min (256, n - pos));
    FILE* o = std::fopen (argv[2], "wb");
    for (int i = 0; i < n; ++i) { float fr[2] = { L[(size_t) i], R[(size_t) i] }; std::fwrite (fr, 4, 2, o); }
    std::fclose (o);
    std::printf ("latency %d samples, last event %d\n", e.latencySamples(), e.lastEvent.load());
    return 0;
}
