#include "../../Source/PluginProcessor.h"
#include "../../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
using namespace juce;
// usage: tosnap out.png toast(0..1) [freqIdx]
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    ToasterProcessor proc;
    proc.setParam (IDs::toast, argc > 2 ? (float) atof (argv[2]) : 0.3f);
    if (argc > 3) proc.setParam (IDs::freq, (float) atoi (argv[3]));
    proc.prepareToPlay (48000, 512);
    std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
    AudioBuffer<float> b (2, 512); MidiBuffer m; Random rnd (3);
    for (int blk = 0; blk < 30; ++blk)
    {
        for (int i = 0; i < 512; ++i) { const float s = (rnd.nextFloat() - 0.5f) * 0.4f; b.setSample (0, i, s); b.setSample (1, i, s); }
        proc.processBlock (b, m);
        MessageManager::getInstance()->runDispatchLoopUntil (2);
    }
    MessageManager::getInstance()->runDispatchLoopUntil (200);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
    File png (argv[1]); png.deleteFile(); FileOutputStream os (png); PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
