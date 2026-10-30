#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "GoatSlapDSP.h"

namespace IDs
{
    #define GS_ID(x) inline constexpr const char* x = #x;
    GS_ID (sync) GS_ID (division) GS_ID (timeMs) GS_ID (feel) GS_ID (mix) GS_ID (aux)
    GS_ID (distance) GS_ID (room) GS_ID (tone) GS_ID (width) GS_ID (feedback)
    GS_ID (duckOn) GS_ID (duck) GS_ID (machine) GS_ID (speed) GS_ID (age) GS_ID (saturate)
    GS_ID (heads) GS_ID (doubler) GS_ID (throwOn) GS_ID (tipsy)
    GS_ID (chReverse) GS_ID (chOctave) GS_ID (chStutter) GS_ID (chRoom) GS_ID (chWobble) GS_ID (output)
    #undef GS_ID
}

struct Preset
{
    const char* name;
    const char* category;   // READY / TAPE / TIPSY
    const char* description;
    std::vector<std::pair<const char*, float>> values;   // plain parameter values
};
const std::vector<Preset>& getPresets();

class GoatSlapProcessor : public juce::AudioProcessor
{
public:
    GoatSlapProcessor();

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
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // UI helpers (message thread)
    void roll();      // Feeling Tipsy: a fresh random setting inside musical limits
    void mutate();    // nudge the current setting a little
    void setParam (const char* id, float plainValue);
    float getParam (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    double getHostBpm() const { return hostBpm.load(); }
    juce::String getTimeText() const;

    juce::AudioProcessorValueTreeState apvts;
    goatslap::Engine engine;
    std::atomic<int> lastRoll { 0 };
    int currentPreset = 0;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    std::atomic<double> hostBpm { 120.0 };
    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GoatSlapProcessor)
};
