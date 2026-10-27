// Test host: loads the built TwinPitch.vst3, renders audio through it, and snapshots the editor.
// usage: tphost <plugin.vst3> <in.wav> <out.wav> <snapshot.png> [style 0-2] [detune%] [delayMs] [mix%]
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

static void setParam (AudioPluginInstance& p, const String& name, float plainValue)
{
    for (auto* prm : p.getParameters())
        if (prm->getName (64) == name)
        {
            if (auto* r = dynamic_cast<RangedAudioParameter*> (prm))
                r->setValueNotifyingHost (r->convertTo0to1 (plainValue));
            else
                prm->setValueNotifyingHost (prm->getValueForText (String (plainValue)));
            return;
        }
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    if (argc < 5) { std::puts ("usage"); return 1; }

    VST3PluginFormat fmt;
    OwnedArray<PluginDescription> descs;
    fmt.findAllTypesForFile (descs, argv[1]);
    if (descs.isEmpty()) { std::puts ("no plugin found"); return 2; }
    String err;
    auto plugin = fmt.createInstanceFromDescription (*descs[0], 48000.0, 512, err);
    if (! plugin) { std::printf ("load failed: %s\n", err.toRawUTF8()); return 3; }
    std::printf ("Loaded: %s by %s, params:\n", descs[0]->name.toRawUTF8(), descs[0]->manufacturerName.toRawUTF8());
    for (auto* prm : plugin->getParameters())
        std::printf ("  %s = %s\n", prm->getName (32).toRawUTF8(), prm->getCurrentValueAsText().toRawUTF8());

    // parameters
    const int style = argc > 5 ? atoi (argv[5]) : 0;
    for (auto* prm : plugin->getParameters())
        if (prm->getName (32) == "Style") prm->setValueNotifyingHost (style / 2.0f);
    if (argc > 6) setParam (*plugin, "Detune", (float) atof (argv[6]));
    if (argc > 7) setParam (*plugin, "Delay",  (float) atof (argv[7]));
    if (argc > 8) setParam (*plugin, "Mix",    (float) atof (argv[8]));

    // audio
    AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> reader (fm.createReaderFor (File (argv[2])));
    if (! reader) { std::puts ("bad input"); return 4; }
    const int len = (int) reader->lengthInSamples;
    AudioBuffer<float> in (2, len);
    reader->read (&in, 0, len, 0, true, true);

    plugin->setPlayConfigDetails (2, 2, reader->sampleRate, 512);
    plugin->prepareToPlay (reader->sampleRate, 512);
    AudioBuffer<float> out (2, len);
    MidiBuffer midi;
    for (int pos = 0; pos < len; pos += 512)
    {
        const int n = jmin (512, len - pos);
        AudioBuffer<float> blk (2, n);
        for (int c = 0; c < 2; ++c) blk.copyFrom (c, 0, in, c, pos, n);
        plugin->processBlock (blk, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, blk, c, 0, n);
    }
    plugin->releaseResources();

    WavAudioFormat wav;
    File outFile (argv[3]); outFile.deleteFile();
    if (auto w = std::unique_ptr<AudioFormatWriter> (wav.createWriterFor (new FileOutputStream (outFile),
                                                          reader->sampleRate, 2, 24, {}, 0)))
        w->writeFromAudioSampleBuffer (out, 0, len);

    // editor snapshot
    if (auto* ed = plugin->createEditorIfNeeded())
    {
        ed->setVisible (true);
        MessageManager::getInstance()->runDispatchLoopUntil (300);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        File png (argv[4]); png.deleteFile();
        FileOutputStream os (png);
        PNGImageFormat().writeImageToStream (img, os);
        std::printf ("editor %dx%d snapshot written\n", ed->getWidth(), ed->getHeight());
        delete ed;
    }
    std::puts ("OK");
    return 0;
}
