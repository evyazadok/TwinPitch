#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ToasterDSP.h"

namespace IDs
{
    #define TO_ID(x) inline constexpr const char* x = #x;
    TO_ID (toast) TO_ID (width) TO_ID (silk) TO_ID (tame) TO_ID (freq) TO_ID (autoGain) TO_ID (on) TO_ID (midSide)
    #undef TO_ID
}

class ToasterProcessor : public juce::AudioProcessor
{
public:
    ToasterProcessor();
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
    double getTailLengthSeconds() const override { return 0.0; }
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
    toaster::Engine engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToasterProcessor)
};
