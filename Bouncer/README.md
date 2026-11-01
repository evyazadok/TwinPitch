# BOUNCER

Kicks the noise and the room out of a recording. Made for vocals recorded in an untreated room: fans, computer hum,
street, keyboard clicks, and the echo of bare walls.

- Big knob (BOUNCE): how hard it throws things out. The word in the middle changes: POLITE, STRICT, RUTHLESS.
  The knob is not linear: the first half is a normal clean-up, the second half goes far past it (a second pass of the
  neural net joins in from 50 %, and the gains are pushed harder and let go faster). The default (55 %) is already
  strong, so you should not need to turn it all the way.
- ROOM: removes the echo behind every word (late reverberation), per frequency.
- FLOOR: leaves a little of the gently cleaned sound under everything so it never sounds dead between words.
- HEAR THE TRASH: plays only what is being thrown out. If you hear parts of the voice in it, turn BOUNCE down.
- VOICE row: lights up while it hears a voice.
- ON/OFF. Latency is about 51 ms (reported to the host).

## How it works
1. RNNoise (BSD licence, Xiph.Org / Mozilla): a small neural net that listens to 10 ms frames and, for 22 frequency
   bands, decides how much is voice and how much is noise.
2. BOUNCE raises those decisions to a power (gain^1 .. gain^4.2), shortens how long the gain holds after a word, and
   from 50 % puts a second RNNoise in series.
3. ROOM: STFT (1024 / hop 256). The late reverb of each bin is estimated from what was heard about 50 ms earlier,
   decayed by an assumed room time, and subtracted.
4. At 44.1 / 88.2 / 96 kHz the plugin resamples to 48 kHz around the engine and reports the extra delay.

Source/rnnoise is RNNoise v0.1.1 with three changes: a function that takes the gain power and release
(`rnnoise_process_frame_x`), fixed-size buffers instead of variable-length arrays (MSVC), and `M_PI` defined.
See Source/rnnoise/COPYING for its licence.
