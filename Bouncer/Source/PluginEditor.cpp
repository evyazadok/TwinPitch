#include "PluginEditor.h"
#if BO_HAS_FONT
 #include "BinaryData.h"
#endif

using namespace juce;

Font boui::mono (float size, bool bold)
{
   #if BO_HAS_FONT
    static auto reg = Typeface::createSystemTypefaceFor (BinaryData::SpaceMonoRegular_ttf, BinaryData::SpaceMonoRegular_ttfSize);
    static auto bld = Typeface::createSystemTypefaceFor (BinaryData::SpaceMonoBold_ttf, BinaryData::SpaceMonoBold_ttfSize);
    return Font (FontOptions (bold ? bld : reg).withHeight (size));
   #else
    return Font (FontOptions ("Menlo", size, bold ? Font::bold : Font::plain));
   #endif
}

//==============================================================================
void BOLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider& s)
{
    auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const bool big = (bool) s.getProperties().getWithDefault ("big", false);
    if (big)
    {
        const auto c = area.getCentre(); const float R = 142.0f;
        const auto lit = pos > 0.67f ? boui::amber : boui::ink;
        for (int i = 0; i < 41; ++i)
        {
            const float t = (float) i / 40.0f, a = degreesToRadians (-225.0f + 270.0f * t);
            const bool on = t <= pos + 1.0e-4f && pos > 0.001f; const float sz = on ? 12.0f : 5.0f;
            g.setColour (on ? (t > 0.67f ? boui::amber : boui::ink) : boui::ink.withAlpha (0.35f));
            g.fillRect (c.x + R * std::cos (a) - sz * 0.5f, c.y + R * std::sin (a) - sz * 0.5f, sz, sz);
        }
        for (int i = 0; i < 36; ++i)
        {
            const float a = degreesToRadians ((float) i * 10.0f);
            g.setColour (lit.withAlpha (0.2f + 0.45f * pos));
            g.fillRect (c.x + 98.0f * std::cos (a) - 1.5f, c.y + 98.0f * std::sin (a) - 1.5f, 3.0f, 3.0f);
        }
        const String word = bouncer::wordFor (s.getValue());
        g.setColour (lit);
        g.setFont (boui::mono (word.length() > 5 ? 27.0f : 40.0f, true));
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
        const bool on = t <= pos + 1.0e-4f && pos > 0.001f; const float sz = on ? 6.0f : 3.0f;
        g.setColour (on ? boui::ink : boui::ink.withAlpha (0.35f));
        g.fillRect (c.x + r * std::cos (a) - sz * 0.5f, c.y + r * std::sin (a) - sz * 0.5f, sz, sz);
    }
    g.setColour (boui::ink);
    g.setFont (boui::mono (12.0f));
    g.drawText (s.getProperties()["label"].toString(), labelArea, Justification::centred);
}

void BOLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    if (b.getToggleState()) { g.setColour (boui::ink); g.fillRect (r); }
    else
    {
        if (over) { g.setColour (boui::ink.withAlpha (0.12f)); g.fillRect (r); }
        g.setColour (boui::ink.withAlpha (0.85f)); g.drawRect (r, 1.0f);
    }
}

void BOLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? boui::ground : boui::ink);
    g.setFont (boui::mono ((float) b.getProperties().getWithDefault ("fontSize", 12.0), true));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), Justification::centred, 1);
}

//==============================================================================
BouncerEditor::BouncerEditor (BouncerProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setupKnob (bounce, IDs::bounce, "", true);
    setupKnob (room, IDs::room, "ROOM", false);
    setupKnob (floorK, IDs::floorLevel, "FLOOR", false);
    setupToggle (onBtn, IDs::on, "ON");
    setupToggle (trashBtn, IDs::trash, "HEAR THE TRASH");
    for (auto* t : { &onBtn, &trashBtn }) t->button.getProperties().set ("fontSize", 12.0);
    setSize (960, 780);
    setScaleFactor (0.75f);     // the plugin window is 720 x 585
    startTimerHz (30);
}

BouncerEditor::~BouncerEditor() { setLookAndFeel (nullptr); }

void BouncerEditor::setupKnob (Knob& k, const char* id, const String& label, bool big)
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

void BouncerEditor::setupToggle (Toggle& t, const char* id, const String& text)
{
    t.button.setButtonText (text); t.button.setClickingTogglesState (true);
    t.att = std::make_unique<ButtonAtt> (proc.apvts, id, t.button); addAndMakeVisible (t.button);
}

void BouncerEditor::timerCallback()
{
    auto level = [] (float peak, float prev) { return jmax (peak, prev * 0.88f); };
    meterIn = level (proc.peakIn.load(), meterIn);
    meterOut = level (proc.peakOut.load(), meterOut);
    voiceLevel = jmax (proc.voice.load(), voiceLevel * 0.85f);
    onBtn.button.setButtonText (proc.getParam (IDs::on) > 0.5f ? "ON" : "OFF");
    repaint();
}

void BouncerEditor::resized()
{
    bounce.slider.setBounds (290 - 170, 350 - 170, 340, 340);
    room.slider.setBounds (640 - 46, 400 - 46, 92, 114);
    floorK.slider.setBounds (790 - 46, 400 - 46, 92, 114);
    trashBtn.button.setBounds (560, 628, 210, 40);
    onBtn.button.setBounds (790, 628, 90, 40);
}

static const char* kFont[7][7] = {
    { "11110", "10001", "10001", "11110", "10001", "10001", "11110" },   // B
    { "01110", "10001", "10001", "10001", "10001", "10001", "01110" },   // O
    { "10001", "10001", "10001", "10001", "10001", "10001", "01110" },   // U
    { "10001", "11001", "10101", "10101", "10011", "10001", "10001" },   // N
    { "01110", "10001", "10000", "10000", "10000", "10001", "01110" },   // C
    { "11111", "10000", "10000", "11110", "10000", "10000", "11111" },   // E
    { "11110", "10001", "10001", "11110", "10100", "10010", "10001" } }; // R

void BouncerEditor::paint (Graphics& g)
{
    g.fillAll (boui::ground);
    g.setColour (boui::ink.withAlpha (0.5f));
    for (int y : { 124, 600 }) g.fillRect (0, y, getWidth(), 1);
    for (int x : { 72, 888 }) g.fillRect (x, 0, 1, getHeight());

    g.setColour (boui::ink);
    for (int li = 0; li < 7; ++li)
        for (int ry = 0; ry < 7; ++ry)
            for (int rx = 0; rx < 5; ++rx)
                if (kFont[li][ry][rx] == '1') g.fillRect (96 + li * 56 + rx * 8, 34 + ry * 8, 7, 7);

    // VOICE: lights up while it hears someone talking or singing
    g.setColour (boui::ink); g.setFont (boui::mono (11.0f));
    g.drawText ("VOICE", 556, 46, 60, 16, Justification::centredLeft);
    const int lit = (int) std::round (jlimit (0.0f, 1.0f, voiceLevel) * 14.0f);
    for (int i = 0; i < 14; ++i)
    {
        g.setColour (i < lit ? boui::amber : boui::ink.withAlpha (0.3f));
        g.fillRect (622 + i * 18, 46 + (i < lit ? 0 : 3), i < lit ? 12 : 8, i < lit ? 12 : 8);
    }

    // IN / OUT meters
    g.setColour (boui::ink); g.setFont (boui::mono (11.0f));
    g.drawText ("IN", 96, 622, 30, 16, Justification::centredLeft);
    g.drawText ("OUT", 96, 652, 30, 16, Justification::centredLeft);
    auto seg = [&] (int y, float peak)
    {
        const float db = Decibels::gainToDecibels (peak, -60.0f);
        for (int i = 0; i < 24; ++i)
        {
            const bool on = db > -48.0f + 2.0f * (float) i;
            g.setColour (boui::ink.withAlpha (on ? 1.0f : 0.3f));
            g.fillRect (128 + i * 14, y, 10, 10);
        }
    };
    seg (626, meterIn); seg (656, meterOut);
}
