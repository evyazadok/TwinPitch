# HANGOVER

Retro wear and tear for guitars, keys, pads and loops. Think "the same record, a few months later".
Built on the same dot-matrix look as GOAT SLAP, in red wine.

**No noise.** HANGOVER never adds hiss, hum or crackle. Everything it does is made from the signal itself,
so when nothing plays, nothing comes out (except the small room's tail, if you turn SPACE on).

## Modules
| Module | What it does |
|---|---|
| WOBBLE | Tape pitch drift (slow wow, small flutter, random drift). FLUX makes the speed and depth wander. |
| TREMOLO | Sine tremolo, synced to the host tempo (1/2 ... 1/16T). PAN makes the sides move in opposite directions. |
| DIGITAL | Old sampler: lower bit depth and sample rate, with soft filtering on both sides. |
| DISTORT | Warm tape-style saturation with a soft knee and a darker top end. |
| SPACE | Small damped room. |
| DROPS | Short level dips, only while there is signal. FLUX makes timing and depth random; off means steady dips on the beat. |

MODE (GUITAR / KEYS / PAD / LOOP) changes how every module behaves for that instrument (wobble speed, how much each
module bites, room size).

MAGNITUDE scales all modules together. FADE IN / FADE OUT move the magnitude over 2, 4 or 8 bars, measured from
where the transport started (or where the loop wrapped): fade in grows from clean to worn, fade out goes from worn to
clean, and both together make a swell up and back down.

## Feeling Tipsy
TIPSY sets how often "good mistakes" happen (only while there is signal): tape dive, crush, swell, hiccup, muffle.
ROLL picks a fresh random setting inside musical limits. 28 presets: GUITAR, KEYS, PAD, LOOP and TIPSY.

## Build
Same as the other plugins: push, then download the artifact from GitHub Actions (`build-hangover.yml` for macOS,
`build-windows.yml` for Windows). Locally: `cmake -B build && cmake --build build --config Release`.

Latency: 20 ms (reported to the host), so the MIX control never combs.
