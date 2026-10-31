#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace vmui
{
    inline const juce::Colour ground { 0xffff4d6d };   // rose: the main colour
    inline const juce::Colour ink    { 0xff4a0f22 };   // dark wine: buttons, titles
    inline const juce::Colour violet { 0xff2c1a8f };   // the formant side
    inline const juce::Colour butter { 0xffffd166 };
    inline const juce::Colour white  { 0xffffffff };
    juce::Font mono (float size, bool bold = false);
}

class VMLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

// the note coming in and the note going out
class NoteDisplay : public juce::Component, private juce::Timer
{
public:
    explicit NoteDisplay (VoxMorphProcessor& p) : proc (p) { startTimerHz (20); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    VoxMorphProcessor& proc;
    float inHz = 0, outHz = 0;
    int holdIn = 0, holdOut = 0;
};

class VoxMorphEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VoxMorphEditor (VoxMorphProcessor&);
    ~VoxMorphEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    void setupKnob (Knob&, const char* id, const juce::String& name, bool big, bool violet);
    void timerCallback() override;

    VoxMorphProcessor& proc;
    VMLookAndFeel lnf;
    Knob pitch, formant, robot, drive, mix, output;
    juce::TextButton modeButtons[3];
    std::unique_ptr<juce::ParameterAttachment> modeAtt;
    juce::TextButton linkButton { "LINK" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> linkAtt;
    NoteDisplay display;
    int mode = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxMorphEditor)
};
