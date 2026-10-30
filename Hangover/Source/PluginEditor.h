#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace houi
{
    inline const juce::Colour ground { 0xff6e1a33 };   // red wine
    inline const juce::Colour white  { 0xffffffff };
    inline const juce::Colour faint  { 0x8cffffff };
    inline const juce::Colour line   { 0x8cffffff };
    inline const juce::Colour yellow { 0xffffd166 };
    inline const juce::Colour mint   { 0xff7cf5c8 };
    juce::Font mono (float size, bool bold = false);
    juce::Font sansItalic (float size);
}

class HOLookAndFeel : public juce::LookAndFeel_V4
{
public:
    HOLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    juce::Font getPopupMenuFont() override { return houi::mono (14.0f); }
};

// The wear view: one clean wave that gets worn from left to right. It follows what the plugin is really doing.
class WearView : public juce::Component, private juce::Timer
{
public:
    explicit WearView (HangoverProcessor& p) : proc (p) { startTimerHz (20); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    HangoverProcessor& proc;
    float level = 0.0f, phase = 0.0f;
    int lastWrite = -1, silentFrames = 1000, frame = 0;
};

class HangoverEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HangoverEditor (HangoverProcessor&);
    ~HangoverEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob { juce::Slider slider; std::unique_ptr<SliderAtt> att; };
    struct Toggle { juce::TextButton button; std::unique_ptr<ButtonAtt> att; };
    struct Module
    {
        const char* title; const char* levelId; const char* onId; const char* fluxId;
        Knob knob; Toggle on, flux;
        juce::Rectangle<int> card;
    };

    void setupKnob (Knob&, const char* id, const juce::String& label, bool big, bool inside);
    void setupToggle (Toggle&, const char* id, const juce::String& text);
    void timerCallback() override;
    void showPresetMenu();
    void stepPreset (int delta);
    void cycleParam (const char* id, int count);

    HangoverProcessor& proc;
    HOLookAndFeel lnf;

    Module mods[6];
    juce::OwnedArray<juce::TextButton> modeButtons;
    std::unique_ptr<juce::ParameterAttachment> modeAtt;
    int modeValue = 1;
    juce::TextButton tremDivBtn, tremPanBtn, barsBtn;
    Knob mix;
    juce::Slider magnitude, tipsy;
    std::unique_ptr<SliderAtt> magAtt, tipsyAtt;
    Toggle fadeIn, fadeOut;
    juce::TextButton rollButton { "ROLL" };
    juce::TextButton presetButton, prevButton { "<" }, nextButton { ">" };

    WearView view;
    int die = 5;
    float pulse = 0.0f;
    juce::String eventText { "nothing yet" };
    juce::Rectangle<int> tipsyBox, dieBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HangoverEditor)
};
