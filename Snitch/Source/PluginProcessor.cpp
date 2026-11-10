#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

SnitchProcessor::SnitchProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  AudioChannelSet::stereo(), true)
                        .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Snitch", createLayout())
{
    running = true;
    thread = std::thread ([this] { worker(); });
}

SnitchProcessor::~SnitchProcessor()
{
    running = false;
    if (thread.joinable()) thread.join();
}

AudioProcessorValueTreeState::ParameterLayout SnitchProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { IDs::memory, 1 }, "Memory", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { IDs::scales, 1 }, "Scales", StringArray { "Basic", "Modes", "All" }, 1));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { IDs::lock, 1 }, "Lock", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { IDs::midiOut, 1 }, "Midi Out", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { IDs::listen, 1 }, "Listen", true));
    return { p.begin(), p.end() };
}

File SnitchProcessor::keyFile()
{
    return File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("DrorAudio").getChildFile ("key.json");
}

void SnitchProcessor::applyOptions()
{
    engine.setOptions (getParam (IDs::memory), (int) getParam (IDs::scales), getParam (IDs::lock) > 0.5f);
}

void SnitchProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    { std::lock_guard<std::mutex> g (engineLock); engine.prepare (sampleRate); }
    mono.assign ((size_t) jmax (samplesPerBlock, 4096), 0.0f);
    sentOn = false; sentMask = 0; sentRoot = -1;
    applyOptions();
}

bool SnitchProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (in != out) return false;
    return in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
}

void SnitchProcessor::sendNotes (MidiBuffer& midi, bool on, unsigned mask, int root)
{
    auto msg = [&] (int note, int vel) { return on ? MidiMessage::noteOn (1, note, (uint8) vel) : MidiMessage::noteOff (1, note); };
    if (root >= 0) midi.addEvent (msg (36 + root, 90), 0);                                  // the home note, low
    for (int pc = 0; pc < 12; ++pc) if (mask & (1u << pc)) midi.addEvent (msg (60 + pc, 70), 0);   // the notes of the scale, one octave from middle C
}

void SnitchProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    midi.clear();
    const int n = buffer.getNumSamples(), chans = buffer.getNumChannels();
    applyOptions();

    if (getParam (IDs::listen) > 0.5f && chans > 0)
    {
        if ((int) mono.size() < n) mono.resize ((size_t) n);                                // only when a host sends a block bigger than promised
        const float* l = buffer.getReadPointer (0); const float* r = chans > 1 ? buffer.getReadPointer (1) : l;
        for (int i = 0; i < n; ++i) mono[(size_t) i] = 0.5f * (l[i] + r[i]);
        engine.feed (mono.data(), n);
    }

    // MIDI out: the notes of the answer are held until the answer changes (they never flicker)
    const bool wantOn = getParam (IDs::midiOut) > 0.5f && engine.state.load() == 2;
    const int root = engine.root.load(); const unsigned mask = engine.notesMask.load();
    if (sentOn && (! wantOn || mask != sentMask || root != sentRoot)) { sendNotes (midi, false, sentMask, sentRoot); sentOn = false; }
    if (wantOn && ! sentOn) { sendNotes (midi, true, mask, root); sentOn = true; sentMask = mask; sentRoot = root; }
}

void SnitchProcessor::writeKeyFile()
{
    const int r = engine.root.load(); if (r < 0 || engine.state.load() != 2) return;
    int nS; const auto& sd = snitch::scaleDefs (nS)[engine.scaleIdx.load()];
    String notes; for (int pc = 0; pc < 12; ++pc) if (engine.notesMask.load() & (1u << pc)) notes << (notes.isEmpty() ? "" : ",") << pc;
    auto f = keyFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText ("{\"root\":" + String (r) + ",\"scale\":\"" + String (sd.name) + "\",\"scaleIndex\":" + String (engine.scaleIdx.load())
                       + ",\"notes\":[" + notes + "],\"name\":\"" + String (snitch::noteName (r)) + " " + String (sd.name) + "\",\"time\":" + String (Time::currentTimeMillis()) + "}");
}

void SnitchProcessor::worker()
{
    while (running)
    {
        bool did = false;
        { std::lock_guard<std::mutex> g (engineLock); did = engine.pump(); }
        const int ch = engine.changeCounter.load();
        if (ch != lastWritten) { lastWritten = ch; writeKeyFile(); }       // other plugins of ours can follow this
        if (! did) std::this_thread::sleep_for (std::chrono::milliseconds (25));
    }
}

void SnitchProcessor::setParam (const char* id, float plain)
{
    if (auto* prm = dynamic_cast<RangedAudioParameter*> (apvts.getParameter (id)))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        prm->endChangeGesture();
    }
}

void SnitchProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void SnitchProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* SnitchProcessor::createEditor() { return new SnitchEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SnitchProcessor(); }
