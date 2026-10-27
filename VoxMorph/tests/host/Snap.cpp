#include "../../Source/PluginProcessor.h"
#include "../../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
using namespace juce;
int main (int, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    VoxMorphProcessor proc;
    proc.apvts.getParameter ("pitch")->setValueNotifyingHost (proc.apvts.getParameter ("pitch")->convertTo0to1 (5.0f));
    proc.apvts.getParameter ("formant")->setValueNotifyingHost (proc.apvts.getParameter ("formant")->convertTo0to1 (-3.0f));
    proc.apvts.getParameter ("drive")->setValueNotifyingHost (0.25f);
    proc.prepareToPlay (48000, 512);
    AudioBuffer<float> b (2, 512); MidiBuffer m; double ph = 0;
    for (int blk = 0; blk < 60; ++blk)
    {
        for (int i = 0; i < 512; ++i) { ph += 196.0 / 48000.0; float v = (float) (0.3 * std::sin (2 * MathConstants<double>::pi * ph) + 0.1 * std::sin (4 * MathConstants<double>::pi * ph)); b.setSample (0, i, v); b.setSample (1, i, v); }
        proc.processBlock (b, m);
    }
    std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
    MessageManager::getInstance()->runDispatchLoopUntil (500);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    File png (argv[1]); png.deleteFile(); FileOutputStream os (png); PNGImageFormat().writeImageToStream (img, os);
    std::printf ("latency %d\n", proc.getLatencySamples());
    return 0;
}
