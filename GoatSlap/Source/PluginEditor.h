#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace gsui
{
    inline const juce::Colour ground { 0xff4b2bd6 };
    inline const juce::Colour white  { 0xffffffff };
    inline const juce::Colour faint  { 0x8cffffff };
    inline const juce::Colour line   { 0x8cffffff };
    inline const juce::Colour red    { 0xffff4d6d };
    inline const juce::Colour green  { 0xff3dff9a };
    juce::Font mono (float size, bool bold = false);
    juce::Font sansItalic (float size);
}

class GSLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GSLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    juce::Font getPopupMenuFont() override { return gsui::mono (14.0f); }
};

// Live dot-matrix view: the dry voice and its slap "shadow" following it in time
class SlapView : public juce::Component, private juce::Timer
{
public:
    explicit SlapView (GoatSlapProcessor& p) : proc (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    GoatSlapProcessor& proc;
    static constexpr int kCols = 95;
    float dry[kCols] {}, wet[kCols] {};
    uint8_t ev[kCols] {};
    int lastWrite = -1;
    int silentFrames = 1000;
};

class GoatSlapEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GoatSlapEditor (GoatSlapProcessor&);
    ~GoatSlapEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::String label;
        std::unique_ptr<SliderAtt> att;
    };
    struct Choice
    {
        juce::OwnedArray<juce::TextButton> buttons;
        std::unique_ptr<juce::ParameterAttachment> att;
        int value = 0;
    };
    struct Toggle
    {
        juce::TextButton button;
        std::unique_ptr<ButtonAtt> att;
    };

    void setupKnob (Knob&, const char* id, const juce::String& label, bool big);
    void setupChoice (Choice&, const char* id, const juce::StringArray& labels);
    void setupToggle (Toggle&, const char* id, const juce::String& text);
    void timerCallback() override;
    void showPresetMenu();
    void stepPreset (int delta);

    GoatSlapProcessor& proc;
    GSLookAndFeel lnf;

    // main view
    Knob timeDiv, timeMs, mix, distance, tone;
    // MORE view
    Knob room, width, feedback, duckAmt, feel, age, saturate, output;
    Toggle sync, chRev, chOct, chStut, chRoom, chWob;
    Choice heads, speed;
    // bottom
    Choice machine;
    Toggle duck, dbl;
    juce::TextButton throwButton { "THROW" };
    Knob tipsy;
    juce::TextButton rollButton { "ROLL" }, mutateButton { "MUTATE" };
    // top
    juce::TextButton presetButton, prevButton { "<" }, nextButton { ">" }, moreButton { "MORE" };
    Toggle aux;

    SlapView view;
    bool showMore = false, throwDown = false;
    int die = 5;
    juce::Rectangle<int> knobArea, tipsyBox, dieBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GoatSlapEditor)
};
