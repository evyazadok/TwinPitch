#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace tpui
{
    inline const juce::Colour ground { 0xffb19aff };   // cyber lavender: the main colour
    inline const juce::Colour ink    { 0xff2c1a8f };   // deep violet: buttons, titles, left voice
    inline const juce::Colour wine   { 0xff6e1a33 };   // right voice
    inline const juce::Colour rose   { 0xffff4d6d };
    inline const juce::Colour white  { 0xffffffff };
    juce::Font mono (float size, bool bold = false);
}

class TPLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

// two voices drawn in dots: the left one sweeps up, the right one down, each offset in time
class SpreadView : public juce::Component, private juce::Timer
{
public:
    explicit SpreadView (TwinPitchProcessor& p) : proc (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { t += 1.0f / 30.0f; repaint(); }
    TwinPitchProcessor& proc;
    float t = 0.0f;
};

class MeterRows : public juce::Component, private juce::Timer
{
public:
    explicit MeterRows (TwinPitchProcessor& p) : proc (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    TwinPitchProcessor& proc;
    float levL = 0.0f, levR = 0.0f;
};

class TwinPitchEditor : public juce::AudioProcessorEditor
{
public:
    explicit TwinPitchEditor (TwinPitchProcessor&);
    ~TwinPitchEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob { juce::Slider slider; std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment; };
    void setupKnob (Knob&, const juce::String& paramId, const juce::String& name, bool big);
    void updateStyleButtons();

    TwinPitchProcessor& proc;
    TPLookAndFeel lnf;
    Knob detune, delay, focus, mix, output;
    juce::TextButton styleButtons[3];
    std::unique_ptr<juce::ParameterAttachment> styleAttachment;
    int currentStyle = 0;
    SpreadView spread;
    MeterRows meters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TwinPitchEditor)
};
