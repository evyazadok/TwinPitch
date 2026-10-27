#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "VoxMorphDSP.h"

namespace IDs
{
    inline constexpr const char* mode      = "mode";
    inline constexpr const char* pitch     = "pitch";
    inline constexpr const char* formant   = "formant";
    inline constexpr const char* link      = "link";
    inline constexpr const char* robotNote = "robotNote";
    inline constexpr const char* drive     = "drive";
    inline constexpr const char* mix       = "mix";
    inline constexpr const char* output    = "output";
}

juce::String noteName (int midi);

class VoxMorphProcessor : public juce::AudioProcessor
{
public:
    VoxMorphProcessor();

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
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    voxmorph::Engine engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    std::atomic<float> *pMode, *pPitch, *pFormant, *pLink, *pRobot, *pDrive, *pMix, *pOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxMorphProcessor)
};
