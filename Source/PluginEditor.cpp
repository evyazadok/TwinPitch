#include "PluginEditor.h"
#if TP_HAS_FONT
 #include "BinaryData.h"
#endif

using namespace juce;

Font tpui::mono (float size, bool bold)
{
   #if TP_HAS_FONT
    static auto reg = Typeface::createSystemTypefaceFor (BinaryData::SpaceMonoRegular_ttf, BinaryData::SpaceMonoRegular_ttfSize);
    static auto bld = Typeface::createSystemTypefaceFor (BinaryData::SpaceMonoBold_ttf, BinaryData::SpaceMonoBold_ttfSize);
    return Font (FontOptions (bold ? bld : reg).withHeight (size));
   #else
    return Font (FontOptions ("Menlo", size, bold ? Font::bold : Font::plain));
   #endif
}

//==============================================================================
void TPLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider& s)
{
    auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const bool big = (bool) s.getProperties().getWithDefault ("big", false);
    const Colour col = s.getProperties().getWithDefault ("wine", false) ? tpui::wine : tpui::ink;
    const String label = s.getProperties()["label"].toString();
    if (big)
    {
        const auto c = area.getCentre(); const float R = 142.0f;
        for (int i = 0; i < 41; ++i)
        {
            const float t = (float) i / 40.0f, a = degreesToRadians (-225.0f + 270.0f * t);
            const bool lit = t <= pos + 1.0e-4f && pos > 0.001f; const float sz = lit ? 12.0f : 5.0f;
            g.setColour (lit ? col : col.withAlpha (0.35f));
            g.fillRect (c.x + R * std::cos (a) - sz * 0.5f, c.y + R * std::sin (a) - sz * 0.5f, sz, sz);
        }
        for (int i = 0; i < 36; ++i)
        {
            const float a = degreesToRadians ((float) i * 10.0f);
            g.setColour (col.withAlpha (0.2f + 0.45f * pos));
            g.fillRect (c.x + 98.0f * std::cos (a) - 1.5f, c.y + 98.0f * std::sin (a) - 1.5f, 3.0f, 3.0f);
        }
        g.setColour (col);
        g.setFont (tpui::mono (14.0f));
        g.drawText (label, Rectangle<float> (c.x - 100.0f, c.y - 44.0f, 200.0f, 20.0f), Justification::centred);
        g.setFont (tpui::mono (34.0f, true));
        g.drawFittedText (s.getTextFromValue (s.getValue()), Rectangle<float> (c.x - 96.0f, c.y - 22.0f, 192.0f, 50.0f).toNearestInt(), Justification::centred, 1);
        return;
    }
    auto labelArea = area.removeFromBottom (22.0f);
    const float size = jmin (area.getWidth(), area.getHeight());
    auto ring = area.withSizeKeepingCentre (size, size);
    const auto c = ring.getCentre(); const float r = size * 0.5f - 6.0f;
    for (int i = 0; i < 21; ++i)
    {
        const float t = (float) i / 20.0f, a = degreesToRadians (-225.0f + 270.0f * t);
        const bool lit = t <= pos + 1.0e-4f && pos > 0.001f; const float sz = lit ? 6.0f : 3.0f;
        g.setColour (lit ? col : col.withAlpha (0.35f));
        g.fillRect (c.x + r * std::cos (a) - sz * 0.5f, c.y + r * std::sin (a) - sz * 0.5f, sz, sz);
    }
    g.setColour (col);
    g.setFont (tpui::mono (12.0f, true));
    g.drawFittedText (s.getTextFromValue (s.getValue()), ring.reduced (size * 0.16f).toNearestInt(), Justification::centred, 1);
    g.setFont (tpui::mono (12.0f));
    g.drawText (label, labelArea, Justification::centred);
}

void TPLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    if (b.getToggleState()) { g.setColour (tpui::ink); g.fillRect (r); }
    else
    {
        if (over) { g.setColour (tpui::ink.withAlpha (0.12f)); g.fillRect (r); }
        g.setColour (tpui::ink.withAlpha (0.85f)); g.drawRect (r, 1.0f);
    }
}

void TPLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? tpui::ground : tpui::ink);
    g.setFont (tpui::mono (15.0f, true));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), Justification::centred, 1);
}

//==============================================================================
void SpreadView::paint (Graphics& g)
{
    const auto& ap = proc.apvts;
    const float det   = ap.getRawParameterValue (ParamIDs::detune)->load() / 100.0f;
    const float dly   = ap.getRawParameterValue (ParamIDs::delay)->load() / 40.0f;
    const int   style = (int) ap.getRawParameterValue (ParamIDs::style)->load();
    const float mix   = ap.getRawParameterValue (ParamIDs::mix)->load() / 100.0f;

    const int cols = 40, rows = 17; const float pitch = 8.0f;
    const float w = cols * pitch, hgt = rows * pitch;
    const float ox = ((float) getWidth() - w) * 0.5f, oy = ((float) getHeight() - hgt) * 0.5f;
    const int mid = rows / 2;
    // centre row = the dry signal
    for (int c = 0; c < cols; ++c)
    { g.setColour (tpui::ink.withAlpha (0.2f + 0.3f * (1.0f - mix))); g.fillRect (ox + c * pitch + 2.0f, oy + mid * pitch + 2.0f, 3.0f, 3.0f); }

    auto voice = [&] (float dir, float timeOffset, Colour col, float seed)
    {
        for (int c = 0; c < cols; ++c)
        {
            const float u = (float) c / (float) (cols - 1);
            float amount = det;
            if (style == 2) amount *= 0.6f + 0.4f * std::sin (t * 1.3f + seed + u * 4.0f);
            const float onset = jlimit (0.0f, 1.0f, (u - timeOffset) * 6.0f);
            const float wave = 0.10f * std::sin (u * 34.0f - t * 6.0f + seed);
            const float yy = dir * (amount * onset * 0.85f + wave * mix);
            const int row = jlimit (0, rows - 1, mid - (int) std::lround (yy * (float) mid * 1.0f));
            g.setColour (col.withAlpha (0.3f + 0.7f * mix));
            g.fillRect (ox + c * pitch, oy + row * pitch, 6.0f, 6.0f);
        }
    };
    voice (+1.0f, 0.04f + dly * 0.18f, tpui::ink, 0.0f);
    voice (-1.0f, 0.04f + dly * 0.18f * (style == 1 ? 1.8f : 1.4f), tpui::wine, 2.1f);
}

void MeterRows::timerCallback()
{
    levL = jmax (proc.meterL.exchange (0.0f), levL * 0.88f);
    levR = jmax (proc.meterR.exchange (0.0f), levR * 0.88f);
    repaint();
}

void MeterRows::paint (Graphics& g)
{
    g.setFont (tpui::mono (11.0f));
    auto row = [&] (int y, const char* name, float lev, Colour col)
    {
        g.setColour (col); g.drawText (name, 0, y - 4, 30, 16, Justification::centredLeft);
        const float db = Decibels::gainToDecibels (lev, -60.0f);
        for (int i = 0; i < 24; ++i)
        {
            const bool lit = db > -48.0f + 2.0f * (float) i;
            g.setColour (col.withAlpha (lit ? 1.0f : 0.3f));
            g.fillRect (32 + i * 14, y, 10, 10);
        }
    };
    row (4, "L", levL, tpui::ink);
    row (34, "R", levR, tpui::wine);
}

//==============================================================================
TwinPitchEditor::TwinPitchEditor (TwinPitchProcessor& p)
    : AudioProcessorEditor (&p), proc (p), spread (p), meters (p)
{
    setLookAndFeel (&lnf);
    setupKnob (detune, ParamIDs::detune, "DETUNE", true);
    setupKnob (delay,  ParamIDs::delay,  "DELAY", false);
    setupKnob (focus,  ParamIDs::focus,  "FOCUS", false);
    setupKnob (mix,    ParamIDs::mix,    "MIX", false);
    setupKnob (output, ParamIDs::output, "OUTPUT", false);
    delay.slider.getProperties().set ("wine", true);
    focus.slider.getProperties().set ("wine", true);

    const char* names[] = { "I", "II", "III" };
    styleAttachment = std::make_unique<ParameterAttachment> (*proc.apvts.getParameter (ParamIDs::style),
        [this] (float v) { currentStyle = (int) v; updateStyleButtons(); spread.repaint(); });
    for (int i = 0; i < 3; ++i)
    {
        auto& b = styleButtons[i];
        b.setButtonText (names[i]);
        b.setClickingTogglesState (false);
        b.onClick = [this, i] { styleAttachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    styleAttachment->sendInitialUpdate();
    addAndMakeVisible (spread);
    addAndMakeVisible (meters);
    setSize (960, 780);
    setScaleFactor (0.75f);     // the plugin window is 720 x 585
}

TwinPitchEditor::~TwinPitchEditor() { setLookAndFeel (nullptr); }

void TwinPitchEditor::setupKnob (Knob& k, const String& id, const String& name, bool big)
{
    k.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    k.slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    k.slider.getProperties().set ("label", name);
    k.slider.getProperties().set ("big", big);
    k.slider.setMouseDragSensitivity (big ? 260 : 200);
    k.attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
    k.slider.setDoubleClickReturnValue (true, proc.apvts.getParameter (id)->convertFrom0to1 (proc.apvts.getParameter (id)->getDefaultValue()));
    addAndMakeVisible (k.slider);
}

void TwinPitchEditor::updateStyleButtons()
{
    for (int i = 0; i < 3; ++i) styleButtons[i].setToggleState (i == currentStyle, dontSendNotification);
}

static const char* kGlyph[9][7] = {
    { "11111", "00100", "00100", "00100", "00100", "00100", "00100" },   // T
    { "10001", "10001", "10001", "10101", "10101", "10101", "01010" },   // W
    { "01110", "00100", "00100", "00100", "00100", "00100", "01110" },   // I
    { "10001", "11001", "10101", "10011", "10001", "10001", "10001" },   // N
    { "11110", "10001", "10001", "11110", "10000", "10000", "10000" },   // P
    { "01110", "00100", "00100", "00100", "00100", "00100", "01110" },   // I
    { "11111", "00100", "00100", "00100", "00100", "00100", "00100" },   // T
    { "01111", "10000", "10000", "10000", "10000", "10000", "01111" },   // C
    { "10001", "10001", "10001", "11111", "10001", "10001", "10001" },   // H
};


void TwinPitchEditor::paint (Graphics& g)
{
    g.fillAll (tpui::ground);
    g.setColour (tpui::ink.withAlpha (0.5f));
    for (int y : { 124, 600 }) g.fillRect (0, y, getWidth(), 1);
    for (int x : { 72, 888 }) g.fillRect (x, 0, 1, getHeight());

    for (int li = 0; li < 9; ++li)
    {
        g.setColour (li < 4 ? tpui::ink : tpui::wine);   // TWIN in teal, PITCH in wine
        for (int ry = 0; ry < 7; ++ry)
            for (int rx = 0; rx < 5; ++rx)
                if (kGlyph[li][ry][rx] == '1') g.fillRect (96 + li * 56 + rx * 8, 34 + ry * 8, 7, 7);
    }
    g.setColour (tpui::ink); g.setFont (tpui::mono (11.0f));
    g.drawText ("STYLE", 660, 24, 52, 16, Justification::centredLeft);

    struct Speck { int x, y; bool dark; };
    for (auto s : { Speck { 40, 60, true }, { 160, 110, false }, { 930, 170, true }, { 30, 380, false }, { 912, 560, true }, { 300, 612, true }, { 44, 700, false } })
    { g.setColour (s.dark ? tpui::rose : tpui::white); g.fillRect (s.x, s.y, 4, 4); }
}

void TwinPitchEditor::resized()
{
    detune.slider.setBounds (290 - 170, 350 - 170, 340, 340);
    for (int i = 0; i < 3; ++i) styleButtons[i].setBounds (660 + i * 72, 44, 64, 40);
    spread.setBounds (560, 150, 320, 180);
    const int xs[4] = { 592, 676, 760, 844 };
    Knob* ks[4] = { &delay, &focus, &mix, &output };
    for (int i = 0; i < 4; ++i) ks[i]->slider.setBounds (xs[i] - 40, 440 - 40, 80, 102);
    meters.setBounds (96, 622, 400, 60);
}
