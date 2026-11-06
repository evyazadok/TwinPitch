#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace chui
{
    inline const juce::Colour ground { 0xff5cb82e };   // jalapeno green
    inline const juce::Colour ink    { 0xff0c2a0a };   // deep pepper-leaf
    inline const juce::Colour pale   { 0xffe6ffcc };   // little pale specks
    inline const juce::Colour hot    { 0xffe8281c };   // chili red, lit when it burns
    juce::Font mono (float size, bool bold = false);
}

class CHLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

class ChariphEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ChariphEditor (ChariphProcessor&);
    ~ChariphEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    struct Knob { juce::Slider slider; std::unique_ptr<SliderAtt> att; };
    struct Toggle { juce::TextButton button; std::unique_ptr<ButtonAtt> att; };

    void setupKnob (Knob&, const char* id, const juce::String& label, int kind);
    void setupToggle (Toggle&, const char* id, const juce::String& text);
    void timerCallback() override;

    ChariphProcessor& proc;
    CHLookAndFeel lnf;
    Knob drive, mix, output;
    Toggle autoBtn, onBtn;
    juce::OwnedArray<juce::TextButton> styleBtns;
    std::unique_ptr<juce::ParameterAttachment> styleAtt;
    float meterIn = 0.0f, meterOut = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChariphEditor)
};
