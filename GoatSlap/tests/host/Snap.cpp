#include "../../Source/PluginProcessor.h"
#include "../../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
using namespace juce;
// usage: gssnap out.png [preset] [more] [voice.raw]
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    GoatSlapProcessor proc;
    if (argc > 2) proc.setCurrentProgram (atoi (argv[2]));
    proc.prepareToPlay (48000, 512);
    std::vector<float> v;
    if (argc > 4) { FILE* f = fopen (argv[4], "rb"); float fr[2]; while (fread (fr, 4, 2, f) == 2) v.push_back (fr[0]); fclose (f); }
    std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
    if (argc > 3 && atoi (argv[3]) == 1)
        if (auto* b = dynamic_cast<TextButton*> (ed->getChildComponent (0))) (void) b;
    AudioBuffer<float> b (2, 512); MidiBuffer m;
    size_t pos = 0;
    for (int blk = 0; blk < 130; ++blk)
    {
        for (int i = 0; i < 512; ++i) { float s = v.empty() ? 0.0f : v[(pos + (size_t) i) % v.size()]; b.setSample (0, i, s); b.setSample (1, i, s); }
        pos += 512;
        proc.processBlock (b, m);
        MessageManager::getInstance()->runDispatchLoopUntil (3);
    }
    // click MORE if asked
    if (argc > 3 && atoi (argv[3]) == 1)
        for (auto* c : ed->getChildren())
            if (auto* tb = dynamic_cast<TextButton*> (c)) if (tb->getButtonText() == "MORE") tb->triggerClick();
    MessageManager::getInstance()->runDispatchLoopUntil (400);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    File png (argv[1]); png.deleteFile(); FileOutputStream os (png); PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
