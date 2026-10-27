#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::String noteName (int midi)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
}

VoxMorphProcessor::VoxMorphProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VoxMorph", createLayout())
{
    pMode    = apvts.getRawParameterValue (IDs::mode);
    pPitch   = apvts.getRawParameterValue (IDs::pitch);
    pFormant = apvts.getRawParameterValue (IDs::formant);
    pLink    = apvts.getRawParameterValue (IDs::link);
    pRobot   = apvts.getRawParameterValue (IDs::robotNote);
    pDrive   = apvts.getRawParameterValue (IDs::drive);
    pMix     = apvts.getRawParameterValue (IDs::mix);
    pOut     = apvts.getRawParameterValue (IDs::output);
}

juce::AudioProcessorValueTreeState::ParameterLayout VoxMorphProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { IDs::mode, 1 }, "Mode",
                     StringArray { "Transpose", "Quantize", "Robot" }, 0));

    auto semis = [] (float v, int)
    {
        if (std::abs (v) < 0.005f) return String ("0 st");
        return (v > 0 ? "+" : "") + String (v, std::abs (v - std::round (v)) < 0.005f ? 0 : 2) + " st";
    };
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { IDs::pitch, 1 }, "Pitch",
                     NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
                     AudioParameterFloatAttributes().withLabel ("st").withStringFromValueFunction (semis)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { IDs::formant, 1 }, "Formant",
                     NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
                     AudioParameterFloatAttributes().withLabel ("st").withStringFromValueFunction (semis)));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { IDs::link, 1 }, "Link", false));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { IDs::robotNote, 1 }, "Robot Note", 36, 84, 48,
                     AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return noteName (v); })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { IDs::drive, 1 }, "Drive",
                     NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
                     AudioParameterFloatAttributes().withLabel ("%")
                         .withStringFromValueFunction ([] (float v, int) { return String (v, 0) + " %"; })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { IDs::mix, 1 }, "Mix",
                     NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                     AudioParameterFloatAttributes().withLabel ("%")
                         .withStringFromValueFunction ([] (float v, int) { return String (v, 0) + " %"; })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { IDs::output, 1 }, "Output",
                     NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                     AudioParameterFloatAttributes().withLabel ("dB")
                         .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));
    return { p.begin(), p.end() };
}

void VoxMorphProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    setLatencySamples (engine.getLatencySamples());
}

bool VoxMorphProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    const auto mono = juce::AudioChannelSet::mono(), st = juce::AudioChannelSet::stereo();
    if (out == mono) return in == mono;
    if (out == st)   return in == mono || in == st;
    return false;
}

void VoxMorphProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();

    voxmorph::Params p;
    p.mode      = (int) pMode->load();
    p.pitch     = pPitch->load();
    p.formant   = pFormant->load();
    p.link      = pLink->load() > 0.5f;
    p.robotNote = (int) pRobot->load();
    p.drive     = pDrive->load() / 100.0;
    p.mix       = pMix->load() / 100.0;
    p.outputDb  = pOut->load();
    engine.setParams (p);

    float* l = buffer.getWritePointer (0);
    if (numIn == 1 || numOut == 1)
    {
        engine.process (l, l, n);                 // mono processing
        if (numOut > 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    }
    else
    {
        engine.process (l, buffer.getWritePointer (1), n);
    }
}

juce::AudioProcessorEditor* VoxMorphProcessor::createEditor() { return new VoxMorphEditor (*this); }

void VoxMorphProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void VoxMorphProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VoxMorphProcessor(); }
