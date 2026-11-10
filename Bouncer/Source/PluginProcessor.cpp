#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

BouncerProcessor::BouncerProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  AudioChannelSet::stereo(), true)
                        .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Bouncer", createLayout())
{
}

AudioProcessorValueTreeState::ParameterLayout BouncerProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto addFloat = [&] (const char* id, const char* name, float def)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f, 0.001f), def,
                        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + "%"; })));
    };
    addFloat (IDs::bounce, "Bounce", 0.55f);
    addFloat (IDs::room, "Room", 0.25f);
    addFloat (IDs::floorLevel, "Floor", 0.3f);
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { IDs::on, 1 }, "On", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { IDs::trash, 1 }, "Trash", false));
    return { p.begin(), p.end() };
}

bouncer::Params BouncerProcessor::currentParams() const
{
    bouncer::Params q;
    q.on = getParam (IDs::on) > 0.5f;
    q.bounce = getParam (IDs::bounce);
    q.room = getParam (IDs::room);
    const float f = getParam (IDs::floorLevel);
    q.floorDb = f < 0.02f ? -100.0 : -46.0 + 32.0 * (double) f;          // up to -14 dB of the gently cleaned signal
    q.listenRemoved = getParam (IDs::trash) > 0.5f;
    return q;
}

void BouncerProcessor::prepareToPlay (double sampleRate, int)
{
    for (auto& c : chains) { c.prepare (sampleRate); c.setParams (currentParams()); }
    setLatencySamples (chains[0].latencySamples());
}

bool BouncerProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != AudioChannelSet::stereo()) return false;
    return in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
}

void BouncerProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const bool mono = getTotalNumInputChannels() == 1;
    if (mono) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    peakIn.store (jmax (buffer.getMagnitude (0, 0, n), buffer.getMagnitude (1, 0, n)));
    const auto q = currentParams();
    for (int ch = 0; ch < (mono ? 1 : 2); ++ch)
    {
        chains[ch].setParams (q);
        chains[ch].process (buffer.getWritePointer (ch), n);
    }
    if (mono) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    peakOut.store (jmax (buffer.getMagnitude (0, 0, n), buffer.getMagnitude (1, 0, n)));
    voice.store (chains[0].voice());
}

void BouncerProcessor::setParam (const char* id, float plain)
{
    if (auto* prm = dynamic_cast<RangedAudioParameter*> (apvts.getParameter (id)))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        prm->endChangeGesture();
    }
}

void BouncerProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void BouncerProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* BouncerProcessor::createEditor() { return new BouncerEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BouncerProcessor(); }
