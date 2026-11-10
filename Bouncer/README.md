# BOUNCER

Kicks the noise and the room out of a recording. Made for vocals recorded in an untreated room: fans, computer hum,
street, keyboard clicks, and the echo of bare walls.

- Big knob (BOUNCE): how hard it throws things out. The word in the middle changes: POLITE, STRICT, RUTHLESS.
  The knob is lopsided: the first half is a normal clean-up, the second half goes far past it. The default (55 %) is
  already strong, so you should not need to turn it all the way.
- ROOM: removes the echo behind every word (late reverberation), per frequency. Default is gentle (25 %) because
  pushing it makes voices hollow.
- FLOOR: leaves a little of the gently cleaned sound under everything (breaths, room tone) so it never sounds dead
  between words. Default 30 %.
- HEAR THE TRASH: plays only what is being thrown out. If you hear parts of the voice in it, turn BOUNCE down.
- VOICE row: lights up while it hears a voice.
- ON/OFF. Latency is about 41 ms (reported to the host).

## How it works
1. RNNoise (BSD licence, Xiph.Org / Mozilla): a small neural net that listens to 10 ms frames and, for 22 frequency
   bands, decides how much is voice and how much is noise. Its pitch comb filter is switched off (it made sung and
   rapped voices sound watery) and its decisions are smoothed across bands.
2. BOUNCE pushes those decisions down, but: less where the voice is (a voice-activity value that falls slowly, so word
   ends and tails survive), less in the top bands (air, s and t sounds), and gains fall slowly. The deep cuts come from
   a gate that only builds up after the voice has been gone for about a quarter of a second (up to 34 dB at the top).
3. A small high shelf at 8 kHz puts back the air the net takes with the noise.
4. ROOM: STFT (1024 / hop 256). The late reverb of each bin is estimated from what was heard about 50 ms earlier,
   decayed by an assumed room time, and subtracted (never deeper than 16 dB, smoothed across bins).
5. At 44.1 / 88.2 / 96 kHz the plugin resamples to 48 kHz around the engine and reports the extra delay.

Source/rnnoise is RNNoise v0.1.1 with changes: `rnnoise_process_frame_x` (the steps above), fixed-size buffers instead
of variable-length arrays (MSVC), and `M_PI` defined. See Source/rnnoise/COPYING for its licence.
