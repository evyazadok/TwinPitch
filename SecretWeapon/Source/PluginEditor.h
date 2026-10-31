#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace swui
{
    inline const juce::Colour ground { 0xff7cf5c8 };   // light mint: the main colour
    inline const juce::Colour ink    { 0xff0b2e33 };   // dark teal: buttons and titles
    inline const juce::Colour white  { 0xffffffff };
    juce::Font mono (float size, bool bold = false);
    juce::Font sansItalic (float size);
}

class SWLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

// piano roll: what the plugin heard (dark) and what it added (white with a dark edge)
class RollView : public juce::Component, private juce::Timer
{
public:
    explicit RollView (SecretWeaponProcessor& p) : proc (p) { startTimerHz (24); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    SecretWeaponProcessor& proc;
    float centre = 69.0f;
};

class SecretWeaponEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SecretWeaponEditor (SecretWeaponProcessor&);
    ~SecretWeaponEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    struct Knob { juce::Slider slider; std::unique_ptr<SliderAtt> att; };
    struct Toggle { juce::TextButton button; std::unique_ptr<ButtonAtt> att; };
    struct Choice
    {
        juce::OwnedArray<juce::TextButton> buttons;
        std::unique_ptr<juce::ParameterAttachment> att;
        int value = 0;
    };

    void setupKnob (Knob&, const char* id, const juce::String& label);
    void setupToggle (Toggle&, const char* id, const juce::String& text);
    void setupChoice (Choice&, const char* id, const juce::StringArray& labels, float fontSize);
    void timerCallback() override;

    SecretWeaponProcessor& proc;
    SWLookAndFeel lnf;
    Choice key, scale, source;
    Knob density, level, range, length;
    Toggle styles[6], onBtn;
    juce::TextButton mixBtn, listenBtn { "LISTEN . 8 BARS" };
    RollView roll;
    juce::String status, heard { "set by hand" };
    bool listenPending = false;
    juce::Rectangle<int> area;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SecretWeaponEditor)
};
