#include "../../Source/PluginProcessor.h"
#include "../../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
using namespace juce;
// usage: swsnap out.png [melody|none]   (feeds a short A minor melody so the roll has something to show)
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    SecretWeaponProcessor proc;
    proc.setParam (IDs::density, 90.0f);
    proc.prepareToPlay (48000, 512);
    std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
    const bool melody = argc > 2 && String (argv[2]) == "melody";
    const int mel[] = { 69, 72, 76, 74, 72, 68, 69, 67, 69, 72, 74, 76 };
    AudioBuffer<float> b (2, 512); MidiBuffer m;
    int64_t pos = 0;
    for (int blk = 0; blk < 420 && melody; ++blk)
    {
        for (int i = 0; i < 512; ++i)
        {
            const double t = (double) (pos + i) / 48000.0, tn = std::fmod (t - 0.3, 0.5); const int k = ((int) std::floor ((t - 0.3) / 0.5)) % 12;
            float s = 0;
            if (t > 0.3 && tn < 0.45) { const double f = 440.0 * std::pow (2.0, (mel[k < 0 ? 0 : k] - 69) / 12.0); const double env = std::min (1.0, tn / 0.005) * std::exp (-tn / 0.25);
                for (int h = 1; h <= 4; ++h) s += (float) (std::sin (2 * MathConstants<double>::pi * f * h * tn) / h * 0.3 * env); }
            b.setSample (0, i, s); b.setSample (1, i, s);
        }
        pos += 512;
        proc.processBlock (b, m);
        if (blk % 4 == 0) MessageManager::getInstance()->runDispatchLoopUntil (2);
    }
    MessageManager::getInstance()->runDispatchLoopUntil (300);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    File png (argv[1]); png.deleteFile(); FileOutputStream os (png); PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
