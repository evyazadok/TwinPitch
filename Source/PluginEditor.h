#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

//==============================================================================
class TwinLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TwinLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;

    static inline const juce::Colour bg      { 0xff17171b };
    static inline const juce::Colour panel   { 0xff222228 };
    static inline const juce::Colour accent  { 0xffe8a33d };   // amber
    static inline const juce::Colour accent2 { 0xff4fc3c9 };   // teal (right side)
    static inline const juce::Colour text    { 0xffe9e6df };
    static inline const juce::Colour dim     { 0xff8a8780 };
};

//==============================================================================
// Animated picture of what the effect does: the left voice sweeps up, the right voice down,
// each offset in time. Spread follows Detune, offset follows Delay, wobble follows Style III.
class SpreadView : public juce::Component, private juce::Timer
{
public:
    explicit SpreadView (TwinPitchProcessor& p);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    TwinPitchProcessor& proc;
    float t = 0.0f, levL = 0.0f, levR = 0.0f;
};

//==============================================================================
class TwinPitchEditor : public juce::AudioProcessorEditor
{
public:
    explicit TwinPitchEditor (TwinPitchProcessor&);
    ~TwinPitchEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const juce::String& paramId, const juce::String& name);
    void updateStyleButtons();

    TwinPitchProcessor& proc;
    TwinLookAndFeel lnf;

    Knob detune, delay, focus, mix, output;
    juce::TextButton styleButtons[3];
    std::unique_ptr<juce::ParameterAttachment> styleAttachment;
    int currentStyle = 0;

    SpreadView spread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TwinPitchEditor)
};
