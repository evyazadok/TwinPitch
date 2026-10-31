#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

ToasterProcessor::ToasterProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  AudioChannelSet::stereo(), true)
                        .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Toaster", createLayout())
{
}

AudioProcessorValueTreeState::ParameterLayout ToasterProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto addFloat = [&] (const char* id, const char* name, float def)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f, 0.001f), def,
                        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + "%"; })));
    };
    addFloat (IDs::toast, "Toast", 0.3f);
    addFloat (IDs::width, "Width", 0.5f);
    addFloat (IDs::silk, "Silk", 0.4f);
    addFloat (IDs::tame, "Tame", 0.0f);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { IDs::freq, 1 }, "Frequency", StringArray { "8K", "10K", "12K", "16K" }, 1));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { IDs::autoGain, 1 }, "Auto Gain", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { IDs::on, 1 }, "On", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { IDs::midSide, 1 }, "Mid/Side", false));
    return { p.begin(), p.end() };
}

void ToasterProcessor::prepareToPlay (double sampleRate, int)
{
    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    toaster::Params q;
    q.toast = g (IDs::toast); q.width = g (IDs::width); q.silk = g (IDs::silk); q.tame = g (IDs::tame);
    q.freq = (int) g (IDs::freq); q.autoGain = g (IDs::autoGain) > 0.5f; q.on = g (IDs::on) > 0.5f; q.midSide = g (IDs::midSide) > 0.5f;
    engine.setParams (q);
    engine.prepare (sampleRate);
}

bool ToasterProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != AudioChannelSet::stereo()) return false;
    return in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
}

void ToasterProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (getTotalNumInputChannels() == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    toaster::Params q;
    q.toast = g (IDs::toast); q.width = g (IDs::width); q.silk = g (IDs::silk); q.tame = g (IDs::tame);
    q.freq = jlimit (0, 3, (int) g (IDs::freq)); q.autoGain = g (IDs::autoGain) > 0.5f; q.on = g (IDs::on) > 0.5f; q.midSide = g (IDs::midSide) > 0.5f;
    engine.setParams (q);
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
}

void ToasterProcessor::setParam (const char* id, float plain)
{
    if (auto* prm = dynamic_cast<RangedAudioParameter*> (apvts.getParameter (id)))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        prm->endChangeGesture();
    }
}

void ToasterProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void ToasterProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* ToasterProcessor::createEditor() { return new ToasterEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ToasterProcessor(); }
