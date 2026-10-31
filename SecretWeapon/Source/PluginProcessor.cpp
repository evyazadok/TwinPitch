#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

SecretWeaponProcessor::SecretWeaponProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  AudioChannelSet::stereo(), true)
                        .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SecretWeapon", createLayout())
{
}

AudioProcessorValueTreeState::ParameterLayout SecretWeaponProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto addBool = [&] (const char* id, const char* name, bool def) { p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    addBool (IDs::on, "On", true);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { IDs::mix, 1 }, "Mix", StringArray { "100%", "75%", "50%", "25%" }, 0));
    StringArray keys; for (int i = 0; i < 12; ++i) keys.add (secretweapon::keyName (i));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { IDs::key, 1 }, "Key", keys, 9));
    StringArray scales; for (int i = 0; i < 10; ++i) scales.add (secretweapon::getScale (i).name);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { IDs::scale, 1 }, "Scale", scales, 1));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { IDs::source, 1 }, "Source", StringArray { "Guitar", "Keys", "Vox", "Bass", "Synth", "Loop" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { IDs::density, 1 }, "Density", NormalisableRange<float> (0, 100, 0.1f), 50,
                     AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 0) + "%"; })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { IDs::level, 1 }, "Level", NormalisableRange<float> (-24, 3, 0.1f), -7,
                     AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 0) + " dB"; })));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { IDs::range, 1 }, "Range", StringArray { "+-1/2 oct", "+-1 oct", "+-2 oct" }, 1));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { IDs::length, 1 }, "Length", StringArray { "1/32", "1/16", "1/8T", "1/8", "1/4" }, 1));
    addBool (IDs::stGhost, "Ghost Notes", true);
    addBool (IDs::stSparkle, "Sparkle", true);
    addBool (IDs::stSteps, "Steps", false);
    addBool (IDs::stChord, "Chord Tones", false);
    addBool (IDs::stAnswer, "Answer", false);
    addBool (IDs::stTipsy, "Tipsy Notes", true);
    return { p.begin(), p.end() };
}

void SecretWeaponProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    setLatencySamples (engine.latencySamples());
}

bool SecretWeaponProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != AudioChannelSet::stereo()) return false;
    return in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
}

void SecretWeaponProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (getTotalNumInputChannels() == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    double ppqNow = -1.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) if (*b > 20.0 && *b < 400.0) bpm.store (*b);
            if (pos->getIsPlaying()) if (auto pq = pos->getPpqPosition()) ppqNow = *pq;     // lets added notes land on the beat grid
        }

    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    static const double mixes[4] = { 1.0, 0.75, 0.5, 0.25 };
    static const double lens[5] = { 0.125, 0.25, 1.0 / 3.0, 0.5, 1.0 };
    static const double ranges[3] = { 0.5, 1.0, 2.0 };
    secretweapon::Params q;
    q.on = g (IDs::on) > 0.5f;
    q.mix = mixes[(int) g (IDs::mix)];
    q.key = (int) g (IDs::key); q.scale = (int) g (IDs::scale); q.source = (int) g (IDs::source);
    q.density = g (IDs::density) / 100.0; q.levelDb = g (IDs::level);
    q.rangeOct = ranges[(int) g (IDs::range)]; q.lengthBeats = lens[(int) g (IDs::length)];
    q.styles = (g (IDs::stGhost) > 0.5f ? secretweapon::StGhost : 0u) | (g (IDs::stSparkle) > 0.5f ? secretweapon::StSparkle : 0u)
             | (g (IDs::stSteps) > 0.5f ? secretweapon::StSteps : 0u) | (g (IDs::stChord) > 0.5f ? secretweapon::StChord : 0u)
             | (g (IDs::stAnswer) > 0.5f ? secretweapon::StAnswer : 0u) | (g (IDs::stTipsy) > 0.5f ? secretweapon::StTipsy : 0u);
    q.bpm = bpm.load(); q.ppq = ppqNow;
    engine.setParams (q);
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
}

void SecretWeaponProcessor::setParam (const char* id, float plain)
{
    if (auto* prm = dynamic_cast<RangedAudioParameter*> (apvts.getParameter (id)))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        prm->endChangeGesture();
    }
}

void SecretWeaponProcessor::listen (int bars)
{
    listenBars.store (bars);
    engine.startListen ((double) bars * 4.0 * 60.0 / bpm.load());
}

void SecretWeaponProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void SecretWeaponProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* SecretWeaponProcessor::createEditor() { return new SecretWeaponEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SecretWeaponProcessor(); }
