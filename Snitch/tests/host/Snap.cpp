#include "../../Source/PluginProcessor.h"
#include "../../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
using namespace juce;
// usage: snsnap out.png [seconds]   plays an A minor loop (Am F C G) with a bass through the plugin, then snapshots the editor
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    SnitchProcessor proc;
    const double sr = 44100.0;
    proc.prepareToPlay (sr, 512);
    proc.setParam (IDs::midiOut, 1.0f);
    std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
    const double secs = argc > 2 ? atof (argv[2]) : 14.0;
    const int chords[4][3] = { { 57, 60, 64 }, { 53, 57, 60 }, { 48, 52, 55 }, { 55, 59, 62 } };
    const int bass[4] = { 33, 29, 36, 31 };
    AudioBuffer<float> b (2, 512); MidiBuffer m; double t = 0; int midiNotes = 0;
    for (int blk = 0; blk < (int) (secs * sr / 512); ++blk)
    {
        const int ci = ((int) (t / 1.6)) % 4;
        for (int i = 0; i < 512; ++i)
        {
            const double tt = t + i / sr; float s = 0.0f;
            for (int c = 0; c < 3; ++c) { const double f = 440.0 * std::pow (2.0, (chords[ci][c] + 12 - 69) / 12.0); for (int h = 1; h <= 5; ++h) s += (float) (0.05 * std::sin (2 * MathConstants<double>::pi * f * h * tt) / h); }
            const double fb = 440.0 * std::pow (2.0, (bass[ci] + 12 - 69) / 12.0); s += (float) (0.2 * std::tanh (2.0 * std::sin (2 * MathConstants<double>::pi * fb * tt)));
            b.setSample (0, i, s); b.setSample (1, i, s);
        }
        t += 512 / sr; proc.processBlock (b, m); midiNotes += m.getNumEvents();
        MessageManager::getInstance()->runDispatchLoopUntil (blk % 4 == 0 ? 11 : 0);       // roughly real time
    }
    MessageManager::getInstance()->runDispatchLoopUntil (300);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
    File png (argv[1]); png.deleteFile(); FileOutputStream os (png); PNGImageFormat().writeImageToStream (img, os);
    printf ("answer %s %s, conf %d%%, midi events %d\n", proc.engine.root.load() >= 0 ? snitch::noteName (proc.engine.root.load()) : "-", "", proc.engine.confPct.load(), midiNotes);
    printf ("key file: %s\n", SnitchProcessor::keyFile().loadFileAsString().toRawUTF8());
    return 0;
}
