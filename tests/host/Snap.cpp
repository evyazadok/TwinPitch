#include "../../Source/PluginProcessor.h"
#include "../../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
using namespace juce;
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    TwinPitchProcessor proc;
    proc.apvts.getParameter ("style")->setValueNotifyingHost (atoi (argv[2]) / 2.0f);
    proc.apvts.getParameter ("detune")->setValueNotifyingHost (0.7f);
    proc.apvts.getParameter ("delay")->setValueNotifyingHost (0.6f);
    proc.prepareToPlay (48000, 512);
    AudioBuffer<float> b (2, 512); MidiBuffer m;
    for (int i = 0; i < 512; ++i) b.setSample (0, i, 0.3f * std::sin (i * 0.05f)), b.setSample (1, i, b.getSample (0, i));
    std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
    for (int blk = 0; blk < 12; ++blk) { proc.processBlock (b, m); MessageManager::getInstance()->runDispatchLoopUntil (30); }
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
    File png (argv[1]); png.deleteFile(); FileOutputStream os (png); PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
