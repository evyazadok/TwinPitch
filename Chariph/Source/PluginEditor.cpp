#include "PluginEditor.h"
#if CH_HAS_FONT
 #include "BinaryData.h"
#endif

using namespace juce;

Font chui::mono (float size, bool bold)
{
   #if CH_HAS_FONT
    static auto reg = Typeface::createSystemTypefaceFor (BinaryData::SpaceMonoRegular_ttf, BinaryData::SpaceMonoRegular_ttfSize);
    static auto bld = Typeface::createSystemTypefaceFor (BinaryData::SpaceMonoBold_ttf, BinaryData::SpaceMonoBold_ttfSize);
    return Font (FontOptions (bold ? bld : reg).withHeight (size));
   #else
    return Font (FontOptions ("Menlo", size, bold ? Font::bold : Font::plain));
   #endif
}

//==============================================================================
void CHLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider& s)
{
    auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const bool big = (bool) s.getProperties().getWithDefault ("big", false);
    if (big)
    {
        const auto c = area.getCentre(); const float R = 142.0f;
        const auto lit = chui::ink;                                  // the outer dots turn chili red once it burns
        for (int i = 0; i < 41; ++i)
        {
            const float t = (float) i / 40.0f, a = degreesToRadians (-225.0f + 270.0f * t);
            const bool on = t <= pos + 1.0e-4f && pos > 0.001f; const float sz = on ? 12.0f : 5.0f;
            g.setColour (on ? (t > 0.67f ? chui::hot : chui::ink) : chui::ink.withAlpha (0.35f));
            g.fillRect (c.x + R * std::cos (a) - sz * 0.5f, c.y + R * std::sin (a) - sz * 0.5f, sz, sz);
        }
        for (int i = 0; i < 36; ++i)
        {
            const float a = degreesToRadians ((float) i * 10.0f);
            g.setColour (lit.withAlpha (0.2f + 0.45f * pos));
            g.fillRect (c.x + 98.0f * std::cos (a) - 1.5f, c.y + 98.0f * std::sin (a) - 1.5f, 3.0f, 3.0f);
        }
        const String word = chariph::wordFor (s.getValue());
        g.setColour (lit);
        if (word.length() > 6)      // GIVE HIM / SOME MILK on two lines
        {
            g.setFont (chui::mono (25.0f, true));
            g.drawText (word.upToFirstOccurrenceOf (" SOME", false, false), Rectangle<float> (c.x - 100.0f, c.y - 34.0f, 200.0f, 32.0f), Justification::centred);
            g.drawText ("SOME MILK", Rectangle<float> (c.x - 100.0f, c.y + 2.0f, 200.0f, 32.0f), Justification::centred);
        }
        else
        {
            g.setFont (chui::mono (40.0f, true));
            g.drawText (word, Rectangle<float> (c.x - 100.0f, c.y - 28.0f, 200.0f, 56.0f), Justification::centred);
        }
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
        g.setColour (on ? chui::ink : chui::ink.withAlpha (0.35f));
        g.fillRect (c.x + r * std::cos (a) - sz * 0.5f, c.y + r * std::sin (a) - sz * 0.5f, sz, sz);
    }
    // MIX shows its percentage, OUTPUT its dB
    const int kind = (int) s.getProperties().getWithDefault ("kind", 0);
    String readout;
    if (kind == 1) readout = String (roundToInt ((float) s.getValue() * 100.0f)) + "%";
    if (kind == 2) readout = (s.getValue() > 0.05 ? "+" : "") + String (s.getValue(), 1);
    if (readout.isNotEmpty())
    {
        g.setColour (chui::ink); g.setFont (chui::mono (15.0f, true));
        g.drawText (readout, Rectangle<float> (c.x - 30.0f, c.y - 10.0f, 60.0f, 20.0f), Justification::centred);
    }
    g.setColour (chui::ink);
    g.setFont (chui::mono (12.0f));
    g.drawText (s.getProperties()["label"].toString(), labelArea, Justification::centred);
}

void CHLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    if (b.getToggleState()) { g.setColour (chui::ink); g.fillRect (r); }
    else
    {
        if (over) { g.setColour (chui::ink.withAlpha (0.12f)); g.fillRect (r); }
        g.setColour (chui::ink.withAlpha (0.85f)); g.drawRect (r, 1.0f);
    }
}

void CHLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? chui::ground : chui::ink);
    g.setFont (chui::mono ((float) b.getProperties().getWithDefault ("fontSize", 12.0), true));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 0), Justification::centred, 1);
}

//==============================================================================
ChariphEditor::ChariphEditor (ChariphProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setupKnob (drive, IDs::drive, "", 0);
    drive.slider.getProperties().set ("big", true);
    drive.slider.setMouseDragSensitivity (260);
    setupKnob (mix, IDs::mix, "MIX", 1);
    setupKnob (output, IDs::output, "OUTPUT", 2);
    setupToggle (autoBtn, IDs::autoGain, "AUTO GAIN");
    setupToggle (onBtn, IDs::on, "ON");
    for (auto* t : { &autoBtn, &onBtn }) t->button.getProperties().set ("fontSize", 12.0);

    for (int i = 0; i < chariph::kNumStyles; ++i)
    {
        auto* b = styleBtns.add (new TextButton (chariph::kStyles[i].name));
        b->getProperties().set ("fontSize", 12.0);
        b->onClick = [this, i] { styleAtt->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    styleAtt = std::make_unique<ParameterAttachment> (*proc.apvts.getParameter (IDs::style), [this] (float v)
    {
        const int idx = (int) std::round (v);
        for (int i = 0; i < styleBtns.size(); ++i) styleBtns[i]->setToggleState (i == idx, dontSendNotification);
    });
    styleAtt->sendInitialUpdate();
    setSize (1040, 780);
    setScaleFactor (0.75f);     // the plugin window is 780 x 585
    startTimerHz (30);
}

ChariphEditor::~ChariphEditor() { setLookAndFeel (nullptr); }

void ChariphEditor::setupKnob (Knob& k, const char* id, const String& label, int kind)
{
    k.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    k.slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    k.slider.getProperties().set ("label", label);
    k.slider.getProperties().set ("kind", kind);
    k.slider.setMouseDragSensitivity (200);
    k.att = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
    if (auto* prm = proc.apvts.getParameter (id))
        k.slider.setDoubleClickReturnValue (true, (double) dynamic_cast<RangedAudioParameter*> (prm)->convertFrom0to1 (prm->getDefaultValue()));
    addAndMakeVisible (k.slider);
}

void ChariphEditor::setupToggle (Toggle& t, const char* id, const String& text)
{
    t.button.setButtonText (text); t.button.setClickingTogglesState (true);
    t.att = std::make_unique<ButtonAtt> (proc.apvts, id, t.button); addAndMakeVisible (t.button);
}

void ChariphEditor::timerCallback()
{
    auto level = [] (float peak, float prev) { return jmax (peak, prev * 0.88f); };
    meterIn = level (proc.engine.peakIn.load(), meterIn);
    meterOut = level (proc.engine.peakOut.load(), meterOut);
    onBtn.button.setButtonText (proc.getParam (IDs::on) > 0.5f ? "ON" : "OFF");
    repaint();
}

static constexpr int kGridX = 540, kCols = 5, kBtnW = 76, kBtnH = 38, kGap = 6;
static constexpr int kStdY = 168, kWeirdY = 400;

void ChariphEditor::resized()
{
    drive.slider.setBounds (290 - 170, 362 - 170, 340, 340);
    for (int i = 0; i < chariph::kNumStyles; ++i)
    {
        const int group = i / 20, k = i % 20, row = k / kCols, col = k % kCols;
        styleBtns[i]->setBounds (kGridX + col * (kBtnW + kGap), (group == 0 ? kStdY : kWeirdY) + row * (kBtnH + kGap), kBtnW, kBtnH);
    }
    mix.slider.setBounds (580 - 46, 664 - 46, 92, 114);
    output.slider.setBounds (692 - 46, 664 - 46, 92, 114);
    autoBtn.button.setBounds (764, 646, 124, 40);
    onBtn.button.setBounds (896, 646, 56, 40);
}

static const char* kFont[7][7] = {
    { "01111", "10000", "10000", "10000", "10000", "10000", "01111" },   // C
    { "10001", "10001", "10001", "11111", "10001", "10001", "10001" },   // H
    { "01110", "10001", "10001", "11111", "10001", "10001", "10001" },   // A
    { "11110", "10001", "10001", "11110", "10100", "10010", "10001" },   // R
    { "11111", "00100", "00100", "00100", "00100", "00100", "11111" },   // I
    { "11110", "10001", "10001", "11110", "10000", "10000", "10000" },   // P
    { "10001", "10001", "10001", "11111", "10001", "10001", "10001" } }; // H

void ChariphEditor::paint (Graphics& g)
{
    g.fillAll (chui::ground);
    g.setColour (chui::ink.withAlpha (0.5f));
    for (int y : { 124, 600 }) g.fillRect (0, y, getWidth(), 1);
    for (int x : { 72, 968 }) g.fillRect (x, 0, 1, getHeight());

    g.setColour (chui::ink);
    for (int li = 0; li < 7; ++li)
        for (int ry = 0; ry < 7; ++ry)
            for (int rx = 0; rx < 5; ++rx)
                if (kFont[li][ry][rx] == '1') g.fillRect (96 + li * 56 + rx * 8, 34 + ry * 8, 7, 7);

    struct Speck { int x, y; bool dark; };
    for (auto s : { Speck { 40, 60, true }, { 190, 110, false }, { 1000, 170, true }, { 30, 380, false }, { 1000, 560, true }, { 330, 612, true }, { 44, 700, false }, { 520, 140, false } })
    { g.setColour (s.dark ? chui::ink : chui::pale); g.fillRect (s.x, s.y, 4, 4); }

    // the two families
    g.setColour (chui::ink); g.setFont (chui::mono (12.0f, true));
    g.drawText ("STANDARD", kGridX, kStdY - 28, 200, 18, Justification::centredLeft);
    g.drawText ("WEIRD", kGridX, kWeirdY - 28, 200, 18, Justification::centredLeft);
    g.setColour (chui::ink.withAlpha (0.45f));
    g.fillRect (kGridX, kStdY - 8, kCols * (kBtnW + kGap) - kGap, 1);
    g.fillRect (kGridX, kWeirdY - 8, kCols * (kBtnW + kGap) - kGap, 1);

    // IN / OUT meters
    g.setColour (chui::ink); g.setFont (chui::mono (11.0f));
    g.drawText ("IN", 96, 622, 30, 16, Justification::centredLeft);
    g.drawText ("OUT", 96, 652, 30, 16, Justification::centredLeft);
    auto seg = [&] (int y, float peak)
    {
        const float db = Decibels::gainToDecibels (peak, -60.0f);
        for (int i = 0; i < 24; ++i)
        {
            const bool lit = db > -48.0f + 2.0f * (float) i;
            g.setColour (chui::ink.withAlpha (lit ? 1.0f : 0.3f));
            g.fillRect (128 + i * 14, y, 10, 10);
        }
    };
    seg (626, meterIn); seg (656, meterOut);
}
