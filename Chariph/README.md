# CHARIPH

A distortion box in the Decapitator family: one big DRIVE knob, a MIX, an OUTPUT, AUTO GAIN, and 40 styles on buttons.
Twenty of them are "standard" (made to sit on vocals and electric guitars), twenty are stranger (motion, filters, folding).

The word in the middle of the knob changes with the drive: SPICY, HOT, GIVE HIM SOME MILK.

## Controls

- **Style buttons**: one button per style, 20 standard and 20 weird.
- **DRIVE (big knob)**: how hard the signal hits the distortion. It changes the *sound*: the amount of harmonics, the compression, which filters open or close. Double-click returns it to SPICY.
- **MIX**: how much of the distorted signal is blended with the clean one (in percent). It only changes the balance, not the character. Everything is delay-compensated, so 0% is exactly your dry signal.
- **OUTPUT**: output level, plus or minus 18 dB.
- **AUTO GAIN** (on by default): keeps the distorted signal at the loudness of what went in, so turning DRIVE up does not make it louder and you judge the sound, not the volume. Turn it off to hear the raw level.
- **ON/OFF**: bypass.

## How it works

- Every style is a short chain of blocks (filters, bending curves, modulation effects). The blocks that bend the wave run at **4x oversampling** (two linear-phase halfband stages), which keeps the aliasing of the standard styles under about -70 dB (about -100 dB for most). Latency is 39 samples and is reported to the host.
- Sources of the curves: tape (soft tanh with head bump and highs that fold away), transformer (only the lows saturate), triode (asymmetric, even harmonics), pentode (sharp knee, odd harmonics), tube-screamer-style overdrive (clean lows, mid hump), germanium fuzz (asymmetric with gating), wall-of-fuzz (two clips, scooped mids), and the weird ones: sine and triangle wave folders, rectifier, crossover (dead zone), slew limiter, bit and sample-rate crusher, ring modulator, and a starved supply that drifts its own bias.
- **Wobble, flanger, phaser, siren, talkbox and tremolo run in time with the song**: the speed is set in beats and follows the host tempo and position. Without a host tempo they run at 120 BPM.
- Changing style crossfades between the old and the new one over 25 ms, so it does not click.
- Silence in gives silence out (no noise, no hiss).
- Works on mono (becomes stereo) and stereo. Window size 780 x 585.

## The 40 styles

### Standard

| style | what it is |
|---|---|
| **TAPE** | Soft, round, a little head bump, highs fold away as it gets hotter |
| **REEL** | Tape pushed into the red: more squash, darker |
| **IRON** | Transformer: the lows saturate first, so you get weight without fizz |
| **CONSOLE** | Solid mids, a touch of 2nd harmonic, a little presence |
| **VELVET** | Beefy lows, silky highs |
| **TRIODE** | Even harmonics, warm and fat |
| **PENTODE** | Odd harmonics, harder knee, edgy |
| **GLOW** | Barely there: warm preamp, the "put it on and it just sounds better" one |
| **SIZZLE** | Bright tube: highs get pushed into the curve harder than the rest |
| **CRUNCH** | Amp in a box: tight lows, mid push, speaker roll-off |
| **BRIT** | Old British stack: mids up front, hard clip, presence |
| **TWEED** | Low-gain old amp: bright, soft, sags when you hit it |
| **SCREAM** | Overdrive pedal: lows stay clean, the mids scream |
| **FUZZ** | Germanium fuzz: asymmetric, spitty, sustains forever |
| **WALL** | Two clipping stages with the mids scooped out: a wall of sustain |
| **AIR** | Only the top gets distorted: sizzle and shine for a vocal |
| **BODY** | Only the low-mids get distorted: weight and chest |
| **EXCITER** | Synthesises clean 2nd and 3rd harmonics of the top end |
| **RADIO** | Band-limited and honky: hip-hop vocal on a transistor radio |
| **CLIPPER** | Clean and tight, shaves peaks and nothing else |

### Weird

| style | what it is |
|---|---|
| **WOBBLE** | A resonant low-pass wobbles in time with the song, then hits the curve |
| **WUB** | Faster, squarer, meaner wobble with a hard knee and extra sub |
| **FLANGUS** | Slow flanger before the distortion: the comb notches get crushed into new harmonics |
| **JET** | Flanger after the distortion with strong feedback: a jet engine |
| **PHASER** | Six-stage phaser into a fuzz |
| **TALKBOX** | Hard clip, then vowel filters that morph A-E-I-O-U in time with the song |
| **VOWEL** | The same idea parked on an "oh": a growl |
| **AUTOWAH** | The harder you sing or play, the higher the filter opens |
| **FOLD** | Sine wave folder: the peaks turn over instead of clipping |
| **FOLDBACK** | Triangle foldback: brittle and metallic |
| **RECTIFY** | Full-wave rectifier: octave-up fuzz |
| **CRUSH** | Bit depth and sample rate fall as you turn it up |
| **SLEW** | Slew limiting: loud highs can't keep up, so everything goes dark and gritty |
| **METAL** | A tuned comb rings after the distortion: pitched, metallic |
| **DALEK** | Ring modulation, and the carrier climbs as you turn it up |
| **CROSS** | Crossover distortion: dead zone around zero, crackly and gritty |
| **SAG** | A starved power supply: bias drifts with the level, it chokes and sputters |
| **TREMOR** | Tremolo before the curve: the distortion pulses in time |
| **SIREN** | A band-pass sweeps through the distorted signal in time with the song |
| **HELL** | Everything at once: two hard clips and a crusher |

## Tests

`tests/measure.cpp` builds without JUCE (`g++ -O2 -std=c++17 measure.cpp -o measure && ./measure`, add `os` to run only the oversampler test, or `all 96000` to try another sample rate). It checks the oversampler (passband, delay), prints harmonics and non-harmonic (alias) energy for every style at four drive settings, checks that silence stays silent, that AUTO GAIN keeps speech-like material within a fraction of a dB of the input, that MIX at 0 is the delayed dry signal, that switching styles does not click, and CPU load.
