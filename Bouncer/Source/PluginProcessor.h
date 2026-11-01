#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "BouncerChain.h"

namespace IDs
{
    #define BO_ID(x) inline constexpr const char* x = #x;
    BO_ID (bounce) BO_ID (room) BO_ID (floorLevel) BO_ID (on) BO_ID (trash)
    #undef BO_ID
}

class BouncerProcessor : public juce::AudioProcessor
{
public:
    BouncerProcessor();
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
    std::atomic<float> peakIn { 0.0f }, peakOut { 0.0f }, voice { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    bouncer::Params currentParams() const;
    bouncer::Chain chains[2];
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BouncerProcessor)
};
