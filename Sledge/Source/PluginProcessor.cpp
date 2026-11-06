#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

SledgeProcessor::SledgeProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  AudioChannelSet::stereo(), true)
                        .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Sledge", createLayout())
{
}

AudioProcessorValueTreeState::ParameterLayout SledgeProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { IDs::drive, 1 }, "Drive", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f,
                    AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (sledge::wordFor (v)); })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { IDs::mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f,
                    AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { IDs::output, 1 }, "Output", NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f,
                    AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));
    StringArray names;
    for (int i = 0; i < sledge::kNumStyles; ++i) names.add (sledge::kStyles[i].name);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { IDs::style, 1 }, "Style", names, 0));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { IDs::autoGain, 1 }, "Auto Gain", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { IDs::on, 1 }, "On", true));
    return { p.begin(), p.end() };
}

sledge::Params SledgeProcessor::readParams() const
{
    sledge::Params q;
    q.drive = getParam (IDs::drive); q.mix = getParam (IDs::mix); q.outDb = getParam (IDs::output);
    q.style = jlimit (0, sledge::kNumStyles - 1, (int) getParam (IDs::style));
    q.autoGain = getParam (IDs::autoGain) > 0.5f; q.on = getParam (IDs::on) > 0.5f;
    return q;
}

void SledgeProcessor::prepareToPlay (double sampleRate, int)
{
    engine.setParams (readParams());
    engine.prepare (sampleRate);
    setLatencySamples (sledge::Engine::latencySamples());
}

bool SledgeProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != AudioChannelSet::stereo()) return false;
    return in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
}

void SledgeProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (getTotalNumInputChannels() == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    auto q = readParams();
    // the wobble, flanger, phaser and vowel styles run in time with the song
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) q.bpm = *bpm;
            q.playing = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition()) q.ppq = *ppq;
        }
    engine.setParams (q);
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
}

void SledgeProcessor::setParam (const char* id, float plain)
{
    if (auto* prm = dynamic_cast<RangedAudioParameter*> (apvts.getParameter (id)))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        prm->endChangeGesture();
    }
}

void SledgeProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void SledgeProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* SledgeProcessor::createEditor() { return new SledgeEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SledgeProcessor(); }
