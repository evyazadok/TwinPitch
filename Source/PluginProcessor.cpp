#include "PluginProcessor.h"
#include "PluginEditor.h"

TwinPitchProcessor::TwinPitchProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TwinPitch", createLayout())
{
    pStyle  = apvts.getRawParameterValue (ParamIDs::style);
    pDetune = apvts.getRawParameterValue (ParamIDs::detune);
    pDelay  = apvts.getRawParameterValue (ParamIDs::delay);
    pFocus  = apvts.getRawParameterValue (ParamIDs::focus);
    pMix    = apvts.getRawParameterValue (ParamIDs::mix);
    pOutput = apvts.getRawParameterValue (ParamIDs::output);
}

juce::AudioProcessorValueTreeState::ParameterLayout TwinPitchProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::style, 1 }, "Style",
                                                         StringArray { "I", "II", "III" }, 0));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::detune, 1 }, "Detune",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f,
        AudioParameterFloatAttributes().withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return String (v, 0) + " %"; })));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::delay, 1 }, "Delay",
        NormalisableRange<float> (0.0f, 40.0f, 0.01f, 0.6f), 10.0f,
        AudioParameterFloatAttributes().withLabel ("ms")
            .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " ms"; })));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::focus, 1 }, "Focus",
        NormalisableRange<float> (20.0f, 2000.0f, 1.0f, 0.3f), 20.0f,
        AudioParameterFloatAttributes().withLabel ("Hz")
            .withStringFromValueFunction ([] (float v, int)
            {
                if (v <= 20.5f) return String ("Full");
                return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (v, 0) + " Hz";
            })));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::mix, 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f,
        AudioParameterFloatAttributes().withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return String (v, 0) + " %"; })));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::output, 1 }, "Output",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));

    return { p.begin(), p.end() };
}

void TwinPitchProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

bool TwinPitchProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;          // it's a stereo widener
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void TwinPitchProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();

    // mono in -> stereo out: copy the input to the right channel first
    if (numIn == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    twinpitch::Params p;
    p.style    = (int) pStyle->load();
    p.detune   = pDetune->load() / 100.0;
    p.delayMs  = pDelay->load();
    p.focusHz  = pFocus->load();
    p.mix      = pMix->load() / 100.0;
    p.outputDb = pOutput->load();
    engine.setParams (p);

    float* l = buffer.getWritePointer (0);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : l;
    engine.process (l, r, n);

    meterL.store (juce::jmax (meterL.load(), buffer.getMagnitude (0, 0, n)));
    if (buffer.getNumChannels() > 1)
        meterR.store (juce::jmax (meterR.load(), buffer.getMagnitude (1, 0, n)));
}

juce::AudioProcessorEditor* TwinPitchProcessor::createEditor()
{
    return new TwinPitchEditor (*this);
}

void TwinPitchProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TwinPitchProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TwinPitchProcessor();
}
