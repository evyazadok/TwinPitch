#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Division index for tremolo: 0 1/2, 1 1/4, 2 1/8, 3 1/8T, 4 1/16, 5 1/16T
const TremDiv* getTremDivs()
{
    static const TremDiv d[kNumTremDivs] = { { "1/2", 2.0 }, { "1/4", 1.0 }, { "1/8", 0.5 }, { "1/8T", 1.0 / 3.0 }, { "1/16", 0.25 }, { "1/16T", 1.0 / 6.0 } };
    return d;
}

// Presets. Every preset starts from a neutral setting (all modules off, mix and output kept),
// then these values are applied. A module whose level is listed is switched on.
// Mode: 0 GUITAR, 1 KEYS, 2 PAD, 3 LOOP.   fadeBars: 0 = 2 bars, 1 = 4 bars, 2 = 8 bars.
const std::vector<Preset>& getPresets()
{
    static const std::vector<Preset> p = {
#include "presets.inc"
    };
    return p;
}

//==============================================================================
HangoverProcessor::HangoverProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  AudioChannelSet::stereo(), true)
                        .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Hangover", createLayout())
{
    currentPreset = 6;   // Dusty Keys
}

AudioProcessorValueTreeState::ParameterLayout HangoverProcessor::createLayout()
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

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { IDs::mode, 1 }, "Mode", StringArray { "Guitar", "Keys", "Pad", "Loop" }, 1));
    addPct (IDs::mix, "Mix", 100);
    addPct (IDs::magnitude, "Magnitude", 100);
    addBool (IDs::wobbleOn, "Wobble On", true);   addPct (IDs::wobble, "Wobble", 55);   addBool (IDs::wobbleFlux, "Wobble Flux", false);
    addBool (IDs::tremOn, "Tremolo On", true);    addPct (IDs::trem, "Tremolo", 40);
    StringArray divs; for (int i = 0; i < kNumTremDivs; ++i) divs.add (getTremDivs()[i].name);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { IDs::tremDiv, 1 }, "Tremolo Rate", divs, 2));
    addBool (IDs::tremPan, "Tremolo Pan", true);
    addBool (IDs::digOn, "Digital On", true);     addPct (IDs::digital, "Digital", 62);
    addBool (IDs::distOn, "Distort On", true);    addPct (IDs::distort, "Distort", 35);
    addBool (IDs::spaceOn, "Space On", false);    addPct (IDs::space, "Space", 30);
    addBool (IDs::dropsOn, "Drops On", true);     addPct (IDs::drops, "Drops", 25);   addBool (IDs::dropsFlux, "Drops Flux", false);
    addBool (IDs::fadeIn, "Fade In", false);
    addBool (IDs::fadeOut, "Fade Out", false);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { IDs::fadeBars, 1 }, "Fade Length", StringArray { "2 bars", "4 bars", "8 bars" }, 1));
    addPct (IDs::tipsy, "Feeling Tipsy", 0);
    addBool (IDs::chDive, "Tipsy: Tape Dive", true);
    addBool (IDs::chCrush, "Tipsy: Crush", true);
    addBool (IDs::chSwell, "Tipsy: Swell", true);
    addBool (IDs::chHiccup, "Tipsy: Hiccup", true);
    addBool (IDs::chMuffle, "Tipsy: Muffle", true);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { IDs::output, 1 }, "Output",
                     NormalisableRange<float> (-24, 12, 0.1f), 0,
                     AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));
    return { p.begin(), p.end() };
}

void HangoverProcessor::prepareToPlay (double sampleRate, int)
{
    sampleRateHz = sampleRate;
    engine.prepare (sampleRate);
    setLatencySamples (engine.latencySamples());
    wasPlaying = false; standalonePpq = 0.0;
}

bool HangoverProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != AudioChannelSet::stereo()) return false;
    return in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
}

void HangoverProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (getTotalNumInputChannels() == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);

    // ---- transport: tempo, and where we are for the fades
    double bpm = 120.0, ppq = 0.0; int num = 4; bool playing = true, haveHost = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) if (*b > 20.0 && *b < 400.0) bpm = *b;
            if (auto q = pos->getPpqPosition()) { ppq = *q; haveHost = true; }
            if (auto ts = pos->getTimeSignature()) num = std::max (1, ts->numerator);
            playing = pos->getIsPlaying();
        }
    if (! haveHost) { ppq = standalonePpq; standalonePpq += (double) n / sampleRateHz * bpm / 60.0; playing = true; }
    if (playing && (! wasPlaying || ppq < lastPpq - 1.0e-6)) startPpq = ppq;   // play pressed or the loop wrapped
    wasPlaying = playing; lastPpq = ppq;

    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const bool fin = g (IDs::fadeIn) > 0.5f, fout = g (IDs::fadeOut) > 0.5f;
    const double N = std::pow (2.0, 1 + (int) g (IDs::fadeBars));   // 2, 4, 8 bars
    const double bars = std::max (0.0, ppq - startPpq) / (double) num;
    const double t = bars / N;
    double fade = 1.0;
    if (fin && fout)       fade = t < 1.0 ? t : (t < 2.0 ? 2.0 - t : 0.0);
    else if (fin)          fade = std::min (1.0, t);
    else if (fout)         fade = std::max (0.0, 1.0 - t);
    fadeNow.store ((float) fade);

    hangover::Params q;
    q.mode = (int) g (IDs::mode);
    q.wobble  = g (IDs::wobbleOn) > 0.5f ? g (IDs::wobble)  / 100.0 : 0.0;
    q.trem    = g (IDs::tremOn)   > 0.5f ? g (IDs::trem)    / 100.0 : 0.0;
    q.digital = g (IDs::digOn)    > 0.5f ? g (IDs::digital) / 100.0 : 0.0;
    q.distort = g (IDs::distOn)   > 0.5f ? g (IDs::distort) / 100.0 : 0.0;
    q.space   = g (IDs::spaceOn)  > 0.5f ? g (IDs::space)   / 100.0 : 0.0;
    q.drops   = g (IDs::dropsOn)  > 0.5f ? g (IDs::drops)   / 100.0 : 0.0;
    q.wobbleFlux = g (IDs::wobbleFlux) > 0.5f;
    q.dropsFlux  = g (IDs::dropsFlux) > 0.5f;
    q.tremPan = g (IDs::tremPan) > 0.5f;
    q.tremBeats = getTremDivs()[(int) g (IDs::tremDiv)].beats;
    q.bpm = bpm;
    q.mix = g (IDs::mix) / 100.0;
    q.magnitude = g (IDs::magnitude) / 100.0 * fade;
    q.tipsy = g (IDs::tipsy) / 100.0;
    q.chaos = (g (IDs::chDive) > 0.5f ? hangover::ChaosDive : 0u) | (g (IDs::chCrush) > 0.5f ? hangover::ChaosCrush : 0u)
            | (g (IDs::chSwell) > 0.5f ? hangover::ChaosSwell : 0u) | (g (IDs::chHiccup) > 0.5f ? hangover::ChaosHiccup : 0u)
            | (g (IDs::chMuffle) > 0.5f ? hangover::ChaosMuffle : 0u);
    q.outputDb = g (IDs::output);
    engine.setParams (q);
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
}

//==============================================================================
int HangoverProcessor::getNumPrograms() { return (int) getPresets().size(); }

const String HangoverProcessor::getProgramName (int i)
{
    const auto& ps = getPresets();
    return i >= 0 && i < (int) ps.size() ? String (ps[(size_t) i].name) : String();
}

void HangoverProcessor::setParam (const char* id, float plain)
{
    if (auto* prm = dynamic_cast<RangedAudioParameter*> (apvts.getParameter (id)))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        prm->endChangeGesture();
    }
}

void HangoverProcessor::setCurrentProgram (int index)
{
    const auto& ps = getPresets();
    if (index < 0 || index >= (int) ps.size()) return;
    currentPreset = index;
    // back to the neutral setting, except what belongs to the user's session
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<RangedAudioParameter*> (prm))
        {
            const String id = r->paramID;
            if (id == IDs::mix || id == IDs::output || id == IDs::fadeIn || id == IDs::fadeOut || id == IDs::fadeBars) continue;
            r->setValueNotifyingHost (r->getDefaultValue());
        }
    for (auto id : { IDs::wobbleOn, IDs::tremOn, IDs::digOn, IDs::distOn, IDs::spaceOn, IDs::dropsOn }) setParam (id, 0.0f);

    std::vector<const char*> listed;
    for (auto& [id, v] : ps[(size_t) index].values) { setParam (id, v); listed.push_back (id); }
    auto has = [&] (const char* id) { for (auto* s : listed) if (String (s) == id) return true; return false; };
    // a module whose level is given is on, unless the preset says otherwise
    const std::pair<const char*, const char*> pairs[] = { { IDs::wobble, IDs::wobbleOn }, { IDs::trem, IDs::tremOn }, { IDs::digital, IDs::digOn },
                                                          { IDs::distort, IDs::distOn }, { IDs::space, IDs::spaceOn }, { IDs::drops, IDs::dropsOn } };
    for (auto& [lvl, on] : pairs)
        if (has (lvl) && ! has (on)) setParam (on, 1.0f);
}

void HangoverProcessor::roll()
{
    auto r = [this] (float lo, float hi) { return lo + random.nextFloat() * (hi - lo); };
    auto coin = [this] (float pr) { return random.nextFloat() < pr; };
    setParam (IDs::wobbleOn, coin (0.9f) ? 1.0f : 0.0f);   setParam (IDs::wobble, r (20, 80));
    setParam (IDs::wobbleFlux, coin (0.4f) ? 1.0f : 0.0f);
    setParam (IDs::tremOn, coin (0.45f) ? 1.0f : 0.0f);    setParam (IDs::trem, r (15, 60));
    setParam (IDs::tremDiv, (float) random.nextInt (kNumTremDivs));
    setParam (IDs::tremPan, coin (0.6f) ? 1.0f : 0.0f);
    setParam (IDs::digOn, coin (0.8f) ? 1.0f : 0.0f);      setParam (IDs::digital, r (15, 85));
    setParam (IDs::distOn, coin (0.8f) ? 1.0f : 0.0f);     setParam (IDs::distort, r (15, 65));
    setParam (IDs::spaceOn, coin (0.4f) ? 1.0f : 0.0f);    setParam (IDs::space, r (15, 60));
    setParam (IDs::dropsOn, coin (0.8f) ? 1.0f : 0.0f);    setParam (IDs::drops, r (10, 50));
    setParam (IDs::dropsFlux, coin (0.5f) ? 1.0f : 0.0f);
    setParam (IDs::tipsy, r (35, 90));
    bool any = false;
    for (auto id : { IDs::chDive, IDs::chCrush, IDs::chSwell, IDs::chHiccup, IDs::chMuffle })
    { const bool on = coin (0.5f); any |= on; setParam (id, on ? 1.0f : 0.0f); }
    if (! any) setParam (IDs::chDive, 1.0f);
    lastRoll.store (1 + random.nextInt (6));
}

//==============================================================================
void HangoverProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void HangoverProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = ValueTree::fromXml (*xml);
            currentPreset = vt.getProperty ("preset", 6);
            apvts.replaceState (vt);
        }
}

AudioProcessorEditor* HangoverProcessor::createEditor() { return new HangoverEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HangoverProcessor(); }
