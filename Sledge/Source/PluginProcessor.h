#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "SledgeDSP.h"

namespace IDs
{
    #define SL_ID(x) inline constexpr const char* x = #x;
    SL_ID (drive) SL_ID (mix) SL_ID (output) SL_ID (style) SL_ID (autoGain) SL_ID (on)
    #undef SL_ID
}

class SledgeProcessor : public juce::AudioProcessor
{
public:
    SledgeProcessor();
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
    double getTailLengthSeconds() const override { return 0.5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    void setParam (const char* id, float plainValue);
    float getParam (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    juce::AudioProcessorValueTreeState apvts;
    sledge::Engine engine;

private:
    sledge::Params readParams() const;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SledgeProcessor)
};
