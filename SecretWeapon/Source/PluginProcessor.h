#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "SecretWeaponDSP.h"

namespace IDs
{
    #define SW_ID(x) inline constexpr const char* x = #x;
    SW_ID (on) SW_ID (mix) SW_ID (key) SW_ID (scale) SW_ID (source) SW_ID (density) SW_ID (level) SW_ID (range) SW_ID (length)
    SW_ID (stGhost) SW_ID (stSparkle) SW_ID (stSteps) SW_ID (stChord) SW_ID (stAnswer) SW_ID (stTipsy)
    #undef SW_ID
}

class SecretWeaponProcessor : public juce::AudioProcessor
{
public:
    SecretWeaponProcessor();

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
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // UI helpers (message thread)
    void setParam (const char* id, float plainValue);
    float getParam (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void listen (int bars = 8);     // learn the key and scale from the next 8 bars
    double getBpm() const { return bpm.load(); }

    juce::AudioProcessorValueTreeState apvts;
    secretweapon::Engine engine;
    std::atomic<int> listenBars { 8 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    std::atomic<double> bpm { 120.0 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SecretWeaponProcessor)
};
