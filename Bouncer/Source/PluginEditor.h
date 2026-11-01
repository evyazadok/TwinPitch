#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace boui
{
    inline const juce::Colour ground { 0xff086e75 };   // petrol teal
    inline const juce::Colour ink    { 0xffeafff6 };   // pale mint
    inline const juce::Colour amber  { 0xffffc857 };   // lit when it gets ruthless
    juce::Font mono (float size, bool bold = false);
}

class BOLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

class BouncerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BouncerEditor (BouncerProcessor&);
    ~BouncerEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    struct Knob { juce::Slider slider; std::unique_ptr<SliderAtt> att; };
    struct Toggle { juce::TextButton button; std::unique_ptr<ButtonAtt> att; };

    void setupKnob (Knob&, const char* id, const juce::String& label, bool big);
    void setupToggle (Toggle&, const char* id, const juce::String& text);
    void timerCallback() override;

    BouncerProcessor& proc;
    BOLookAndFeel lnf;
    Knob bounce, room, floorK;
    Toggle onBtn, trashBtn;
    float meterIn = 0.0f, meterOut = 0.0f, voiceLevel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BouncerEditor)
};
