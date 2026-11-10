#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <mutex>
#include "SnitchDSP.h"

namespace IDs
{
    #define SN_ID(x) inline constexpr const char* x = #x;
    SN_ID (memory) SN_ID (scales) SN_ID (lock) SN_ID (midiOut) SN_ID (listen)
    #undef SN_ID
}

class SnitchProcessor : public juce::AudioProcessor
{
public:
    SnitchProcessor();
    ~SnitchProcessor() override;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
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
    static juce::File keyFile();

    juce::AudioProcessorValueTreeState apvts;
    snitch::Engine engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void applyOptions();
    void worker();
    void writeKeyFile();
    void sendNotes (juce::MidiBuffer&, bool on, unsigned mask, int root);

    std::thread thread; std::atomic<bool> running { false }; std::mutex engineLock;
    std::vector<float> mono;
    unsigned sentMask = 0; int sentRoot = -1; bool sentOn = false;
    int lastWritten = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnitchProcessor)
};
