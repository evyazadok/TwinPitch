#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Presets. Each preset starts from the default of every parameter, then sets these.
// Division index: 0 1/64, 1 1/32T, 2 1/32, 3 1/16T, 4 1/32., 5 1/16, 6 1/8T, 7 1/16., 8 1/8, 9 1/4T, 10 1/8., 11 1/4
// Machine: 0 Clean, 1 Analog, 2 Radio, 3 Studio Reel, 4 Cassette, 5 Broken 4-Trk, 6 Lo-Fi, 7 Dictaphone
const std::vector<Preset>& getPresets()
{
    static const std::vector<Preset> p = {
        // ---------------- READY: put it on and it's fire
        { "Front Row", "READY", "Close, tight, tiny room. The default for a rap lead.",
          { { IDs::division, 2 }, { IDs::mix, 18 }, { IDs::distance, 12 }, { IDs::room, 15 }, { IDs::tone, -25 }, { IDs::width, 35 }, { IDs::duck, 55 } } },
        { "Hook Lift", "READY", "A little wider and brighter for pop hooks.",
          { { IDs::division, 5 }, { IDs::mix, 24 }, { IDs::distance, 22 }, { IDs::room, 35 }, { IDs::tone, 15 }, { IDs::width, 70 }, { IDs::feedback, 12 }, { IDs::duck, 45 } } },
        { "Trap Slap 1/16", "READY", "Hard ducking, only speaks between the words.",
          { { IDs::division, 5 }, { IDs::mix, 30 }, { IDs::distance, 10 }, { IDs::tone, -10 }, { IDs::width, 55 }, { IDs::feedback, 8 }, { IDs::duck, 95 } } },
        { "Ad-Lib Corner", "READY", "Pushes ad-libs back and to the sides.",
          { { IDs::division, 8 }, { IDs::mix, 35 }, { IDs::distance, 70 }, { IDs::room, 60 }, { IDs::tone, -40 }, { IDs::width, 100 }, { IDs::machine, 2 }, { IDs::feedback, 15 }, { IDs::duck, 40 } } },
        { "Glue", "READY", "You don't hear it. You miss it when it's off.",
          { { IDs::division, 2 }, { IDs::mix, 12 }, { IDs::distance, 18 }, { IDs::room, 25 }, { IDs::tone, -45 }, { IDs::width, 40 }, { IDs::machine, 1 }, { IDs::duck, 30 } } },
        { "Wide Double", "READY", "Almost a double, a few cents apart on each side.",
          { { IDs::sync, 0 }, { IDs::timeMs, 42 }, { IDs::mix, 28 }, { IDs::distance, 10 }, { IDs::tone, 0 }, { IDs::width, 100 }, { IDs::doubler, 1 }, { IDs::duckOn, 0 } } },

        // ---------------- TAPE
        { "Reel 15", "TAPE", "Warm classic studio tape slap.",
          { { IDs::sync, 0 }, { IDs::timeMs, 120 }, { IDs::mix, 26 }, { IDs::machine, 3 }, { IDs::speed, 1 }, { IDs::age, 25 }, { IDs::saturate, 45 }, { IDs::tone, -20 }, { IDs::distance, 20 } } },
        { "Cassette Diary", "TAPE", "Intimate, soft and a bit wobbly.",
          { { IDs::division, 5 }, { IDs::mix, 28 }, { IDs::machine, 4 }, { IDs::speed, 0 }, { IDs::age, 55 }, { IDs::saturate, 40 }, { IDs::tone, -50 }, { IDs::distance, 30 }, { IDs::room, 20 } } },
        { "Sun Room", "TAPE", "Dark '50s slapback.",
          { { IDs::sync, 0 }, { IDs::timeMs, 120 }, { IDs::mix, 32 }, { IDs::machine, 3 }, { IDs::speed, 0 }, { IDs::age, 35 }, { IDs::saturate, 60 }, { IDs::tone, -65 }, { IDs::width, 10 }, { IDs::duckOn, 0 }, { IDs::distance, 25 } } },
        { "Three Heads", "TAPE", "Three playback heads, rhythmic slap.",
          { { IDs::division, 5 }, { IDs::mix, 30 }, { IDs::machine, 3 }, { IDs::heads, 2 }, { IDs::feedback, 10 }, { IDs::age, 30 }, { IDs::tone, -30 }, { IDs::width, 60 } } },
        { "Melting", "TAPE", "Old tape, lots of wow. Intros and soft hooks.",
          { { IDs::division, 8 }, { IDs::mix, 34 }, { IDs::machine, 4 }, { IDs::speed, 0 }, { IDs::age, 95 }, { IDs::saturate, 50 }, { IDs::feedback, 28 }, { IDs::tone, -55 }, { IDs::distance, 40 }, { IDs::room, 45 } } },
        { "Stop & Drop", "TAPE", "Broken tape and a dirty throw on every phrase.",
          { { IDs::division, 5 }, { IDs::mix, 30 }, { IDs::machine, 5 }, { IDs::age, 70 }, { IDs::saturate, 65 }, { IDs::feedback, 15 }, { IDs::tone, -35 }, { IDs::tipsy, 25 },
            { IDs::chReverse, 0 }, { IDs::chOctave, 0 }, { IDs::chStutter, 0 }, { IDs::chRoom, 0 }, { IDs::chWobble, 1 } } },

        // ---------------- FEELING TIPSY: good mistakes
        { "Reverse Whisper", "TIPSY", "Some repeats play backwards.",
          { { IDs::division, 8 }, { IDs::mix, 34 }, { IDs::machine, 1 }, { IDs::tone, -30 }, { IDs::tipsy, 70 },
            { IDs::chReverse, 1 }, { IDs::chOctave, 0 }, { IDs::chStutter, 0 }, { IDs::chRoom, 0 }, { IDs::chWobble, 0 } } },
        { "Octave Ghost", "TIPSY", "A quiet repeat an octave up, now and then.",
          { { IDs::division, 5 }, { IDs::mix, 26 }, { IDs::tone, 10 }, { IDs::width, 80 }, { IDs::tipsy, 60 },
            { IDs::chReverse, 0 }, { IDs::chOctave, 1 }, { IDs::chStutter, 0 }, { IDs::chRoom, 0 }, { IDs::chWobble, 0 } } },
        { "Skipping CD", "TIPSY", "Small random stutters.",
          { { IDs::division, 5 }, { IDs::mix, 30 }, { IDs::machine, 6 }, { IDs::tipsy, 65 },
            { IDs::chReverse, 0 }, { IDs::chOctave, 0 }, { IDs::chStutter, 1 }, { IDs::chRoom, 0 }, { IDs::chWobble, 0 } } },
        { "Wrong Room", "TIPSY", "The room changes every few seconds.",
          { { IDs::division, 5 }, { IDs::mix, 24 }, { IDs::distance, 55 }, { IDs::room, 50 }, { IDs::machine, 2 }, { IDs::tipsy, 55 },
            { IDs::chReverse, 0 }, { IDs::chOctave, 0 }, { IDs::chStutter, 0 }, { IDs::chRoom, 1 }, { IDs::chWobble, 0 } } },
        { "Dictaphone Dream", "TIPSY", "Narrow, dirty, a little unstable.",
          { { IDs::division, 8 }, { IDs::mix, 36 }, { IDs::machine, 7 }, { IDs::age, 60 }, { IDs::tone, -20 }, { IDs::feedback, 20 }, { IDs::tipsy, 40 },
            { IDs::chReverse, 0 }, { IDs::chOctave, 0 }, { IDs::chStutter, 1 }, { IDs::chRoom, 0 }, { IDs::chWobble, 1 } } },
        { "Glitch Throw", "TIPSY", "Every throw does something different.",
          { { IDs::division, 5 }, { IDs::mix, 30 }, { IDs::machine, 5 }, { IDs::feedback, 20 }, { IDs::tipsy, 90 }, { IDs::duck, 70 } } },
    };
    return p;
}

//==============================================================================
GoatSlapProcessor::GoatSlapProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  AudioChannelSet::stereo(), true)
                        .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GoatSlap", createLayout())
{
    setCurrentProgram (0);
}

AudioProcessorValueTreeState::ParameterLayout GoatSlapProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto pct = [] (float v, int) { return String (v, 0) + " %"; };
    auto addPct = [&] (const char* id, const char* name, float def)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0, 100, 0.1f), def,
                         AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (pct)));
    };
    auto addBool = [&] (const char* id, const char* name, bool def)
    { p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    StringArray divs; for (int i = 0; i < goatslap::kNumDivisions; ++i) divs.add (goatslap::getDivisions()[i].name);
    addBool (IDs::sync, "Sync", true);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { IDs::division, 1 }, "Division", divs, 5));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { IDs::timeMs, 1 }, "Time",
                     NormalisableRange<float> (20, 500, 0.1f, 0.5f), 110,
                     AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction ([] (float v, int) { return String (v, 0) + " ms"; })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { IDs::feel, 1 }, "Feel",
                     NormalisableRange<float> (-20, 20, 0.1f), 0,
                     AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction ([] (float v, int)
                         { return std::abs (v) < 0.05f ? String ("on grid") : (v > 0 ? "+" : "") + String (v, 1) + " ms"; })));
    addPct (IDs::mix, "Mix", 22);
    addBool (IDs::aux, "Aux (100% wet)", false);
    addPct (IDs::distance, "Distance", 25);
    addPct (IDs::room, "Room", 30);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { IDs::tone, 1 }, "Tone",
                     NormalisableRange<float> (-100, 100, 0.1f), -30,
                     AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                         { return v < -60 ? String ("very dark") : v < -15 ? String ("dark") : v <= 15 ? String ("neutral") : v <= 60 ? String ("bright") : String ("very bright"); })));
    addPct (IDs::width, "Width", 50);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { IDs::feedback, 1 }, "Feedback",
                     NormalisableRange<float> (0, 90, 0.1f), 5, AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (pct)));
    addBool (IDs::duckOn, "Duck", true);
    addPct (IDs::duck, "Duck Amount", 50);
    StringArray machines; for (int i = 0; i < 8; ++i) machines.add (goatslap::getMachine (i).name);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { IDs::machine, 1 }, "Machine", machines, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { IDs::speed, 1 }, "Tape Speed", StringArray { "7.5 ips", "15 ips", "30 ips" }, 1));
    addPct (IDs::age, "Age", 30);
    addPct (IDs::saturate, "Saturate", 30);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { IDs::heads, 1 }, "Heads", StringArray { "1", "2", "3" }, 0));
    addBool (IDs::doubler, "Double", false);
    addBool (IDs::throwOn, "Throw", false);
    addPct (IDs::tipsy, "Feeling Tipsy", 0);
    addBool (IDs::chReverse, "Tipsy: Reverse", true);
    addBool (IDs::chOctave, "Tipsy: Octave", true);
    addBool (IDs::chStutter, "Tipsy: Stutter", true);
    addBool (IDs::chRoom, "Tipsy: Room Swap", true);
    addBool (IDs::chWobble, "Tipsy: Wobble", true);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { IDs::output, 1 }, "Output",
                     NormalisableRange<float> (-24, 12, 0.1f), 0,
                     AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));
    return { p.begin(), p.end() };
}

void GoatSlapProcessor::prepareToPlay (double sampleRate, int) { engine.prepare (sampleRate); }

bool GoatSlapProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != AudioChannelSet::stereo()) return false;
    return in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
}

void GoatSlapProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (getTotalNumInputChannels() == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 20.0 && *bpm < 400.0) hostBpm.store (*bpm);

    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    goatslap::Params p;
    const bool sync = g (IDs::sync) > 0.5f;
    p.timeMs = sync ? goatslap::getDivisions()[(int) g (IDs::division)].beats * 60000.0 / hostBpm.load() : g (IDs::timeMs);
    p.feelMs = g (IDs::feel);
    p.mix = g (IDs::mix) / 100.0;
    p.aux = g (IDs::aux) > 0.5f;
    p.distance = g (IDs::distance) / 100.0;
    p.room = g (IDs::room) / 100.0;
    p.tone = g (IDs::tone) / 100.0;
    p.width = g (IDs::width) / 100.0;
    p.feedback = g (IDs::feedback) / 100.0;
    p.duckOn = g (IDs::duckOn) > 0.5f;
    p.duck = g (IDs::duck) / 100.0;
    p.machine = (int) g (IDs::machine);
    p.speed = (int) g (IDs::speed);
    p.age = g (IDs::age) / 100.0;
    p.saturate = g (IDs::saturate) / 100.0;
    p.heads = (int) g (IDs::heads) + 1;
    p.dbl = g (IDs::doubler) > 0.5f;
    p.throwOn = g (IDs::throwOn) > 0.5f;
    p.tipsy = g (IDs::tipsy) / 100.0;
    p.chaos = (g (IDs::chReverse) > 0.5f ? goatslap::ChaosReverse : 0u) | (g (IDs::chOctave) > 0.5f ? goatslap::ChaosOctave : 0u)
            | (g (IDs::chStutter) > 0.5f ? goatslap::ChaosStutter : 0u) | (g (IDs::chRoom) > 0.5f ? goatslap::ChaosRoom : 0u)
            | (g (IDs::chWobble) > 0.5f ? goatslap::ChaosWobble : 0u);
    p.outputDb = g (IDs::output);
    engine.setParams (p);
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
}

String GoatSlapProcessor::getTimeText() const
{
    if (getParam (IDs::sync) > 0.5f) return goatslap::getDivisions()[(int) getParam (IDs::division)].name;
    return String ((int) std::round (getParam (IDs::timeMs))) + "ms";
}

//==============================================================================
int GoatSlapProcessor::getNumPrograms() { return (int) getPresets().size(); }

const String GoatSlapProcessor::getProgramName (int i)
{
    const auto& ps = getPresets();
    return i >= 0 && i < (int) ps.size() ? String (ps[(size_t) i].name) : String();
}

void GoatSlapProcessor::setParam (const char* id, float plain)
{
    if (auto* prm = dynamic_cast<RangedAudioParameter*> (apvts.getParameter (id)))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        prm->endChangeGesture();
    }
}

void GoatSlapProcessor::setCurrentProgram (int index)
{
    const auto& ps = getPresets();
    if (index < 0 || index >= (int) ps.size()) return;
    currentPreset = index;
    // reset everything except where the user sits in the mix chain (insert/aux) and output
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<RangedAudioParameter*> (prm))
            if (r->paramID != IDs::aux && r->paramID != IDs::output && r->paramID != IDs::throwOn)
                r->setValueNotifyingHost (r->getDefaultValue());
    for (auto& [id, v] : ps[(size_t) index].values) setParam (id, v);
}

void GoatSlapProcessor::roll()
{
    auto r = [this] (float lo, float hi) { return lo + random.nextFloat() * (hi - lo); };
    static const int divs[] = { 2, 3, 4, 5, 5, 6, 7, 8 };
    setParam (IDs::sync, 1);
    setParam (IDs::division, (float) divs[random.nextInt (8)]);
    setParam (IDs::machine, (float) random.nextInt (8));
    setParam (IDs::speed, (float) random.nextInt (3));
    setParam (IDs::tone, r (-75, 35));
    setParam (IDs::distance, r (5, 60));
    setParam (IDs::room, r (0, 80));
    setParam (IDs::width, r (25, 100));
    setParam (IDs::feedback, r (0, 35));
    setParam (IDs::age, r (10, 90));
    setParam (IDs::saturate, r (10, 80));
    setParam (IDs::heads, (float) (random.nextFloat() < 0.7f ? 0 : random.nextInt (3)));
    setParam (IDs::doubler, random.nextFloat() < 0.3f ? 1.0f : 0.0f);
    setParam (IDs::tipsy, r (25, 90));
    bool any = false;
    for (auto id : { IDs::chReverse, IDs::chOctave, IDs::chStutter, IDs::chRoom, IDs::chWobble })
    { const bool on = random.nextFloat() < 0.5f; any |= on; setParam (id, on ? 1.0f : 0.0f); }
    if (! any) setParam (IDs::chReverse, 1);
    lastRoll.store (1 + random.nextInt (6));
}

void GoatSlapProcessor::mutate()
{
    for (auto id : { IDs::tone, IDs::distance, IDs::room, IDs::width, IDs::feedback, IDs::age, IDs::saturate, IDs::tipsy })
        if (auto* prm = dynamic_cast<RangedAudioParameter*> (apvts.getParameter (id)))
        {
            const float v = jlimit (0.0f, 1.0f, prm->getValue() + (random.nextFloat() - 0.5f) * 0.16f);
            prm->beginChangeGesture(); prm->setValueNotifyingHost (v); prm->endChangeGesture();
        }
}

//==============================================================================
void GoatSlapProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void GoatSlapProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = ValueTree::fromXml (*xml);
            currentPreset = vt.getProperty ("preset", 0);
            apvts.replaceState (vt);
        }
}

AudioProcessorEditor* GoatSlapProcessor::createEditor() { return new GoatSlapEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GoatSlapProcessor(); }
