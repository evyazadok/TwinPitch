#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace toui
{
    inline const juce::Colour ground { 0xfff3e6c8 };   // cream
    inline const juce::Colour ink    { 0xffb3350f };   // burnt red
    inline const juce::Colour deep   { 0xff1c0b2e };   // little dark specks
    inline const juce::Colour pale   { 0xfffffaf0 };
    juce::Font mono (float size, bool bold = false);
}

class TOLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

class ToasterEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ToasterEditor (ToasterProcessor&);
    ~ToasterEditor() override;
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

    ToasterProcessor& proc;
    TOLookAndFeel lnf;
    Knob toast, width, silk, tame;
    Toggle autoBtn, onBtn, msBtn;
    juce::OwnedArray<juce::TextButton> freqBtns;
    std::unique_ptr<juce::ParameterAttachment> freqAtt;
    float meterIn = 0.0f, meterOut = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToasterEditor)
};
