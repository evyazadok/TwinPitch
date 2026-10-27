#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class VMLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VMLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;

    static inline const juce::Colour bg     { 0xff15131a };
    static inline const juce::Colour panel  { 0xff211d29 };
    static inline const juce::Colour accent { 0xffff5c8a };   // pink
    static inline const juce::Colour alt    { 0xff8f7cff };   // violet
    static inline const juce::Colour text   { 0xffeeeaf4 };
    static inline const juce::Colour dim    { 0xff8d879a };
};

// Shows the detected input note and the note coming out
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
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    void setupKnob (Knob&, const char* id, const juce::String& name, bool altColour = false);
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
