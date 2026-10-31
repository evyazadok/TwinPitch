# SECRET WEAPON

Listens to a melody and slips in extra notes that belong to your scale. The extra notes are copies of your own
sound played at a new pitch (like a sampler), so they always sound like the same instrument.

## How to use
1. Pick SOURCE (what you put it on): GUITAR, KEYS, VOX, BASS, SYNTH or LOOP.
2. Set KEY and SCALE by hand, or press LISTEN . 8 BARS and play the part: it finds the key and scale and sets them.
3. Choose WHAT IT ADDS and set DENSITY (how often), LEVEL, RANGE (how far from the played note) and LENGTH (note length and spacing).
4. MIX sets how loud the secret notes are (the original sound is always full). ON/OFF switches them.

Single-note lines (a guitar line, a voice, a bass) get the whole scale. When it hears chords or a loop it can not
tell the notes apart, so it plays it safe: octaves and fifths only.

## Styles
GHOST NOTES quiet repeats of the melody, moved to another scale note. SPARKLE short high plinks. STEPS a quick neighbour note.
CHORD TONES a 3rd or 5th under the strong notes. ANSWER after a phrase, a short reply from its last notes that lands on a scale note.
TIPSY NOTES wrong on purpose (some play backwards) but never outside the scale.

## Notes
Latency is 180 ms (reported to the host): the plugin needs to hear the start of a note to know its pitch, and the original
sound is delayed by the same amount so the added notes line up with it. Added notes are always in the chosen scale; the
offline test checks every added note across all 10 scales, 3 keys, 5 sources and 7 style settings.

Window size is 720 x 585.
