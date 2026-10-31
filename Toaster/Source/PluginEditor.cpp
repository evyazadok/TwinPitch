#include "PluginEditor.h"
#if TO_HAS_FONT
 #include "BinaryData.h"
#endif

using namespace juce;

Font toui::mono (float size, bool bold)
{
   #if TO_HAS_FONT
    static auto reg = Typeface::createSystemTypefaceFor (BinaryData::SpaceMonoRegular_ttf, BinaryData::SpaceMonoRegular_ttfSize);
    static auto bld = Typeface::createSystemTypefaceFor (BinaryData::SpaceMonoBold_ttf, BinaryData::SpaceMonoBold_ttfSize);
    return Font (FontOptions (bold ? bld : reg).withHeight (size));
   #else
    return Font (FontOptions ("Menlo", size, bold ? Font::bold : Font::plain));
   #endif
}

//==============================================================================
void TOLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider& s)
{
    auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const bool big = (bool) s.getProperties().getWithDefault ("big", false);
    if (big)
    {
        const auto c = area.getCentre(); const float R = 142.0f;
        for (int i = 0; i < 41; ++i)
        {
            const float t = (float) i / 40.0f, a = degreesToRadians (-225.0f + 270.0f * t);
            const bool lit = t <= pos + 1.0e-4f && pos > 0.001f; const float sz = lit ? 12.0f : 5.0f;
            g.setColour (lit ? toui::ink : toui::ink.withAlpha (0.35f));
            g.fillRect (c.x + R * std::cos (a) - sz * 0.5f, c.y + R * std::sin (a) - sz * 0.5f, sz, sz);
        }
        for (int i = 0; i < 36; ++i)
        {
            const float a = degreesToRadians ((float) i * 10.0f);
            g.setColour (toui::ink.withAlpha (0.2f + 0.45f * pos));
            g.fillRect (c.x + 98.0f * std::cos (a) - 1.5f, c.y + 98.0f * std::sin (a) - 1.5f, 3.0f, 3.0f);
        }
        const String word = toaster::wordFor (s.getValue());
        g.setColour (toui::ink);
        g.setFont (toui::mono (word.length() > 5 ? 27.0f : 40.0f, true));
        g.drawText (word, Rectangle<float> (c.x - 100.0f, c.y - 28.0f, 200.0f, 56.0f), Justification::centred);
        return;
    }
    auto labelArea = area.removeFromBottom (22.0f);
    const float size = jmin (area.getWidth(), area.getHeight());
    const auto c = area.withSizeKeepingCentre (size, size).getCentre();
    const float r = size * 0.5f - 8.0f;
    for (int i = 0; i < 21; ++i)
    {
        const float t = (float) i / 20.0f, a = degreesToRadians (-225.0f + 270.0f * t);
        const bool lit = t <= pos + 1.0e-4f && pos > 0.001f; const float sz = lit ? 6.0f : 3.0f;
        g.setColour (lit ? toui::ink : toui::ink.withAlpha (0.35f));
        g.fillRect (c.x + r * std::cos (a) - sz * 0.5f, c.y + r * std::sin (a) - sz * 0.5f, sz, sz);
    }
    g.setColour (toui::ink);
    g.setFont (toui::mono (12.0f));
    g.drawText (s.getProperties()["label"].toString(), labelArea, Justification::centred);
}

void TOLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    if (b.getToggleState()) { g.setColour (toui::ink); g.fillRect (r); }
    else
    {
        if (over) { g.setColour (toui::ink.withAlpha (0.12f)); g.fillRect (r); }
        g.setColour (toui::ink.withAlpha (0.85f)); g.drawRect (r, 1.0f);
    }
}

void TOLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? toui::ground : toui::ink);
    g.setFont (toui::mono ((float) b.getProperties().getWithDefault ("fontSize", 12.0), true));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), Justification::centred, 1);
}

//==============================================================================
ToasterEditor::ToasterEditor (ToasterProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setupKnob (toast, IDs::toast, "", true);
    setupKnob (width, IDs::width, "WIDTH", false);
    setupKnob (silk, IDs::silk, "SILK", false);
    setupKnob (tame, IDs::tame, "TAME", false);
    setupToggle (autoBtn, IDs::autoGain, "AUTO GAIN");
    setupToggle (onBtn, IDs::on, "ON");
    setupToggle (msBtn, IDs::midSide, "MID/SIDE");
    for (auto* t : { &autoBtn, &onBtn, &msBtn }) t->button.getProperties().set ("fontSize", 12.0);
    const char* names[4] = { "8K", "10K", "12K", "16K" };
    for (int i = 0; i < 4; ++i)
    {
        auto* b = freqBtns.add (new TextButton (names[i]));
        b->getProperties().set ("fontSize", 14.0);
        b->onClick = [this, i] { freqAtt->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    freqAtt = std::make_unique<ParameterAttachment> (*proc.apvts.getParameter (IDs::freq), [this] (float v)
    {
        const int idx = (int) std::round (v);
        for (int i = 0; i < freqBtns.size(); ++i) freqBtns[i]->setToggleState (i == idx, dontSendNotification);
    });
    freqAtt->sendInitialUpdate();
    setSize (960, 780);
    setScaleFactor (0.75f);     // the plugin window is 720 x 585
    startTimerHz (30);
}

ToasterEditor::~ToasterEditor() { setLookAndFeel (nullptr); }

void ToasterEditor::setupKnob (Knob& k, const char* id, const String& label, bool big)
{
    k.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    k.slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    k.slider.getProperties().set ("label", label);
    k.slider.getProperties().set ("big", big);
    k.slider.setMouseDragSensitivity (big ? 260 : 200);
    k.att = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
    if (auto* prm = proc.apvts.getParameter (id))
        k.slider.setDoubleClickReturnValue (true, (double) dynamic_cast<RangedAudioParameter*> (prm)->convertFrom0to1 (prm->getDefaultValue()));
    addAndMakeVisible (k.slider);
}

void ToasterEditor::setupToggle (Toggle& t, const char* id, const String& text)
{
    t.button.setButtonText (text); t.button.setClickingTogglesState (true);
    t.att = std::make_unique<ButtonAtt> (proc.apvts, id, t.button); addAndMakeVisible (t.button);
}

void ToasterEditor::timerCallback()
{
    auto level = [] (float peak, float prev) { return jmax (peak, prev * 0.88f); };
    meterIn = level (proc.engine.peakIn.load(), meterIn);
    meterOut = level (proc.engine.peakOut.load(), meterOut);
    onBtn.button.setButtonText (proc.getParam (IDs::on) > 0.5f ? "ON" : "OFF");
    repaint();
}

void ToasterEditor::resized()
{
    toast.slider.setBounds (290 - 170, 350 - 170, 340, 340);
    const int xs[3] = { 600, 712, 824 };
    Knob* ks[3] = { &width, &silk, &tame };
    for (int i = 0; i < 3; ++i) ks[i]->slider.setBounds (xs[i] - 46, 400 - 46, 92, 114);
    for (int i = 0; i < 4; ++i) freqBtns[i]->setBounds (556 + i * 76, 40, 68, 40);
    autoBtn.button.setBounds (560, 628, 150, 40);
    onBtn.button.setBounds (722, 628, 62, 40);
    msBtn.button.setBounds (796, 628, 84, 40);
}

static const char* kFont[7][7] = {
    { "11111", "00100", "00100", "00100", "00100", "00100", "00100" },   // T
    { "01110", "10001", "10001", "10001", "10001", "10001", "01110" },   // O
    { "01110", "10001", "10001", "11111", "10001", "10001", "10001" },   // A
    { "01111", "10000", "10000", "01110", "00001", "00001", "11110" },   // S
    { "11111", "00100", "00100", "00100", "00100", "00100", "00100" },   // T
    { "11111", "10000", "10000", "11110", "10000", "10000", "11111" },   // E
    { "11110", "10001", "10001", "11110", "10100", "10010", "10001" } }; // R

void ToasterEditor::paint (Graphics& g)
{
    g.fillAll (toui::ground);
    g.setColour (toui::ink.withAlpha (0.5f));
    for (int y : { 124, 600 }) g.fillRect (0, y, getWidth(), 1);
    for (int x : { 72, 888 }) g.fillRect (x, 0, 1, getHeight());

    g.setColour (toui::ink);
    for (int li = 0; li < 7; ++li)
        for (int ry = 0; ry < 7; ++ry)
            for (int rx = 0; rx < 5; ++rx)
                if (kFont[li][ry][rx] == '1') g.fillRect (96 + li * 56 + rx * 8, 34 + ry * 8, 7, 7);

    struct Speck { int x, y; bool dark; };
    for (auto s : { Speck { 40, 60, true }, { 160, 110, false }, { 930, 170, true }, { 30, 380, false }, { 912, 560, true }, { 300, 612, true }, { 44, 700, false } })
    { g.setColour (s.dark ? toui::deep : toui::pale); g.fillRect (s.x, s.y, 4, 4); }

    // IN / OUT meters
    g.setColour (toui::ink); g.setFont (toui::mono (11.0f));
    g.drawText ("IN", 96, 622, 30, 16, Justification::centredLeft);
    g.drawText ("OUT", 96, 652, 30, 16, Justification::centredLeft);
    auto seg = [&] (int y, float peak)
    {
        const float db = Decibels::gainToDecibels (peak, -60.0f);
        for (int i = 0; i < 24; ++i)
        {
            const bool lit = db > -48.0f + 2.0f * (float) i;
            g.setColour (toui::ink.withAlpha (lit ? 1.0f : 0.3f));
            g.fillRect (128 + i * 14, y, 10, 10);
        }
    };
    seg (626, meterIn); seg (656, meterOut);
}
