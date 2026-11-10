#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace snui
{
    inline const juce::Colour ground { 0xffff8a3d };   // tangerine
    inline const juce::Colour ink    { 0xff3a1300 };   // burnt brown
    juce::Font mono (float size, bool bold = false);
}

class SNLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

class SnitchEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SnitchEditor (SnitchProcessor&);
    ~SnitchEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    struct Toggle { juce::TextButton button; std::unique_ptr<ButtonAtt> att; };

    void setupToggle (Toggle&, const char* id, const juce::String& text);
    void timerCallback() override;
    juce::String answerName() const;
    void drawDotText (juce::Graphics&, const juce::String&, int x, int y, int width);

    SnitchProcessor& proc;
    SNLookAndFeel lnf;
    juce::Slider memory;
    std::unique_ptr<SliderAtt> memoryAtt;
    Toggle lockBtn, midiBtn, listenBtn;
    juce::TextButton basicBtn, modesBtn, allBtn, copyBtn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnitchEditor)
};
