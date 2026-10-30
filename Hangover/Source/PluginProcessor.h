#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "HangoverDSP.h"

namespace IDs
{
    #define HO_ID(x) inline constexpr const char* x = #x;
    HO_ID (mode) HO_ID (mix) HO_ID (magnitude)
    HO_ID (wobbleOn) HO_ID (wobble) HO_ID (wobbleFlux)
    HO_ID (tremOn) HO_ID (trem) HO_ID (tremDiv) HO_ID (tremPan)
    HO_ID (digOn) HO_ID (digital) HO_ID (distOn) HO_ID (distort)
    HO_ID (spaceOn) HO_ID (space) HO_ID (dropsOn) HO_ID (drops) HO_ID (dropsFlux)
    HO_ID (fadeIn) HO_ID (fadeOut) HO_ID (fadeBars)
    HO_ID (tipsy) HO_ID (chDive) HO_ID (chCrush) HO_ID (chSwell) HO_ID (chHiccup) HO_ID (chMuffle)
    HO_ID (output)
    #undef HO_ID
}

struct Preset
{
    const char* name;
    const char* category;   // GUITAR / KEYS / PAD / LOOP / TIPSY
    const char* description;
    std::vector<std::pair<const char*, float>> values;   // plain parameter values
};
const std::vector<Preset>& getPresets();

struct TremDiv { const char* name; double beats; };
const TremDiv* getTremDivs();
constexpr int kNumTremDivs = 6;

class HangoverProcessor : public juce::AudioProcessor
{
public:
    HangoverProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.5; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // UI helpers (message thread)
    void roll();                       // Feeling Tipsy: a fresh random setting inside musical limits
    void setParam (const char* id, float plainValue);
    float getParam (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    juce::AudioProcessorValueTreeState apvts;
    hangover::Engine engine;
    std::atomic<int> lastRoll { 0 };
    std::atomic<float> fadeNow { 1.0f };
    int currentPreset = 0;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::Random random;
    // transport tracking for the fades
    bool wasPlaying = false;
    double startPpq = 0.0, standalonePpq = 0.0, lastPpq = 0.0;
    double sampleRateHz = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HangoverProcessor)
};
