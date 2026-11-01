#include "../../Source/PluginProcessor.h"
#include "../../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
using namespace juce;
// usage: bosnap out.png bounce(0..1) [sampleRate]
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    BouncerProcessor proc;
    proc.setParam (IDs::bounce, argc > 2 ? (float) atof (argv[2]) : 0.55f);
    const double sr = argc > 3 ? atof (argv[3]) : 48000.0;
    proc.prepareToPlay (sr, 512);
    std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
    AudioBuffer<float> b (2, 512); MidiBuffer m; Random rnd (3);
    for (int blk = 0; blk < 60; ++blk)
    {
        for (int i = 0; i < 512; ++i) { const float s = (rnd.nextFloat() - 0.5f) * 0.4f; b.setSample (0, i, s); b.setSample (1, i, s); }
        proc.processBlock (b, m);
        MessageManager::getInstance()->runDispatchLoopUntil (2);
    }
    MessageManager::getInstance()->runDispatchLoopUntil (200);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
    File png (argv[1]); png.deleteFile(); FileOutputStream os (png); PNGImageFormat().writeImageToStream (img, os);
    printf ("latency %d\n", proc.getLatencySamples());
    return 0;
}
