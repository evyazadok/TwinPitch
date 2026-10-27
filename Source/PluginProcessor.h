#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "TwinPitchDSP.h"

namespace ParamIDs
{
    inline constexpr const char* style  = "style";
    inline constexpr const char* detune = "detune";
    inline constexpr const char* delay  = "delay";
    inline constexpr const char* focus  = "focus";
    inline constexpr const char* mix    = "mix";
    inline constexpr const char* output = "output";
}

class TwinPitchProcessor : public juce::AudioProcessor
{
public:
    TwinPitchProcessor();
    ~TwinPitchProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.15; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // simple output metering for the UI (L/R peak, decays in the editor)
    std::atomic<float> meterL { 0.0f }, meterR { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    twinpitch::Engine engine;
    std::atomic<float>* pStyle  = nullptr;
    std::atomic<float>* pDetune = nullptr;
    std::atomic<float>* pDelay  = nullptr;
    std::atomic<float>* pFocus  = nullptr;
    std::atomic<float>* pMix    = nullptr;
    std::atomic<float>* pOutput = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TwinPitchProcessor)
};
