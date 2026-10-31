# TOASTER

One big knob of top end, built like a Pultec EQP-1A boost: a tuned peak with a small air shelf above it, the
attenuation shelf that fights it the way the real one does, and a little tube and iron colour. The word in the middle
changes with the knob: CUTE, HOT, FABULOUS.

- Big knob: the boost. It is not in dB; it opens slowly, then fast (about 17 dB at the very top).
- 8K / 10K / 12K / 16K: where the boost sits (default 10K).
- WIDTH: sharp to broad. SILK: tube harmonics and a touch of iron. TAME: calms the very top.
- AUTO GAIN (on by default): keeps the loudness close to the input, up to 4 dB. Turn it off to hear the raw boost.
- ON/OFF, MID/SIDE (boost mostly on the side signal, the middle gets about a third of it).
- No latency. Window size 720 x 585.

tests/measure.cpp prints the curves (gain per frequency for each setting) and checks auto gain and bypass.
