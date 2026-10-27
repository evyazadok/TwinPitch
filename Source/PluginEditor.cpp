#include "PluginEditor.h"

using namespace juce;

//==============================================================================
TwinLookAndFeel::TwinLookAndFeel()
{
    setColour (Slider::textBoxTextColourId, text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Label::textColourId, dim);
}

void TwinLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, Slider& s)
{
    const auto bounds = Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float track = jmax (3.0f, radius * 0.09f);
    const auto col = s.getProperties().getWithDefault ("teal", false) ? accent2 : accent;

    // track
    Path bgArc;
    bgArc.addCentredArc (c.x, c.y, radius - track, radius - track, 0.0f, startAngle, endAngle, true);
    g.setColour (Colour (0xff34343c));
    g.strokePath (bgArc, PathStrokeType (track, PathStrokeType::curved, PathStrokeType::rounded));

    Path valArc;
    valArc.addCentredArc (c.x, c.y, radius - track, radius - track, 0.0f, startAngle, angle, true);
    g.setColour (col);
    g.strokePath (valArc, PathStrokeType (track, PathStrokeType::curved, PathStrokeType::rounded));

    // body
    const float br = radius - track * 2.6f;
    g.setGradientFill (ColourGradient (Colour (0xff3a3a42), c.x, c.y - br,
                                       Colour (0xff1c1c21), c.x, c.y + br, false));
    g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
    g.setColour (Colour (0xff0e0e11));
    g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.2f);

    // pointer
    Path ptr;
    const float pw = jmax (2.0f, br * 0.09f);
    ptr.addRoundedRectangle (-pw * 0.5f, -br * 0.92f, pw, br * 0.42f, pw * 0.5f);
    g.setColour (text);
    g.fillPath (ptr, AffineTransform::rotation (angle).translated (c.x, c.y));
}

void TwinLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? accent : (over ? Colour (0xff3a3a42) : Colour (0xff2c2c33)));
    g.fillRoundedRectangle (r, 6.0f);
}

void TwinLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? bg : text);
    g.setFont (FontOptions (17.0f, Font::bold));
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred);
}

//==============================================================================
SpreadView::SpreadView (TwinPitchProcessor& p) : proc (p) { startTimerHz (30); }

void SpreadView::timerCallback()
{
    t += 1.0f / 30.0f;
    levL = jmax (proc.meterL.exchange (0.0f), levL * 0.85f);
    levR = jmax (proc.meterR.exchange (0.0f), levR * 0.85f);
    repaint();
}

void SpreadView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff111114));
    g.fillRoundedRectangle (r, 10.0f);

    const auto& ap = proc.apvts;
    const float det   = ap.getRawParameterValue (ParamIDs::detune)->load() / 100.0f;
    const float dly   = ap.getRawParameterValue (ParamIDs::delay)->load() / 40.0f;
    const int   style = (int) ap.getRawParameterValue (ParamIDs::style)->load();
    const float mix   = ap.getRawParameterValue (ParamIDs::mix)->load() / 100.0f;

    auto area = r.reduced (16.0f, 14.0f);
    const float cy = area.getCentreY();
    const float half = area.getHeight() * 0.42f;

    // centre line = dry signal
    g.setColour (TwinLookAndFeel::dim.withAlpha (0.35f + 0.4f * (1.0f - mix)));
    g.drawHorizontalLine ((int) cy, area.getX(), area.getRight());

    auto drawVoice = [&] (float dir, float timeOffset, Colour col, float wobbleSeed)
    {
        Path p;
        const int steps = 120;
        for (int i = 0; i <= steps; ++i)
        {
            const float u = (float) i / steps;
            const float xx = area.getX() + u * area.getWidth();
            float amount = det;
            if (style == 2)
                amount *= 0.6f + 0.4f * std::sin (t * 1.3f + wobbleSeed + u * 4.0f);
            // the voice "leaves" the centre after its delay, then sits detuned
            const float onset = jlimit (0.0f, 1.0f, (u - timeOffset) * 6.0f);
            const float wave = 0.10f * std::sin (u * 34.0f - t * 6.0f + wobbleSeed);
            const float yy = cy - dir * half * (amount * onset * 0.85f + wave * mix);
            if (i == 0) p.startNewSubPath (xx, yy); else p.lineTo (xx, yy);
        }
        g.setColour (col.withAlpha (0.25f + 0.75f * mix));
        g.strokePath (p, PathStrokeType (2.2f, PathStrokeType::curved));
    };

    drawVoice (+1.0f, 0.04f + dly * 0.18f, TwinLookAndFeel::accent, 0.0f);
    drawVoice (-1.0f, 0.04f + dly * 0.18f * (style == 1 ? 1.8f : 1.4f), TwinLookAndFeel::accent2, 2.1f);

    g.setFont (FontOptions (12.0f, Font::bold));
    g.setColour (TwinLookAndFeel::accent);
    g.drawText ("L  +", area.removeFromTop (16.0f), Justification::topRight);
    g.setColour (TwinLookAndFeel::accent2);
    g.drawText ("R  -", area.removeFromBottom (16.0f), Justification::bottomRight);

    // meters on the left edge
    auto m = r.reduced (6.0f, 14.0f).removeFromLeft (6.0f);
    auto meterH = [&] (float lev) { return jlimit (0.0f, 1.0f, (Decibels::gainToDecibels (lev, -60.0f) + 60.0f) / 60.0f); };
    const float hL = meterH (levL) * m.getHeight() * 0.5f, hR = meterH (levR) * m.getHeight() * 0.5f;
    g.setColour (TwinLookAndFeel::accent);
    g.fillRect (m.getX(), m.getCentreY() - hL, 3.0f, hL);
    g.setColour (TwinLookAndFeel::accent2);
    g.fillRect (m.getX(), m.getCentreY(), 3.0f, hR);
}

//==============================================================================
TwinPitchEditor::TwinPitchEditor (TwinPitchProcessor& p)
    : AudioProcessorEditor (&p), proc (p), spread (p)
{
    setLookAndFeel (&lnf);

    setupKnob (detune, ParamIDs::detune, "DETUNE");
    setupKnob (delay,  ParamIDs::delay,  "DELAY");
    setupKnob (focus,  ParamIDs::focus,  "FOCUS");
    setupKnob (mix,    ParamIDs::mix,    "MIX");
    setupKnob (output, ParamIDs::output, "OUTPUT");
    delay.slider.getProperties().set ("teal", true);
    focus.slider.getProperties().set ("teal", true);

    const char* names[] = { "I", "II", "III" };
    auto* styleParam = proc.apvts.getParameter (ParamIDs::style);
    styleAttachment = std::make_unique<ParameterAttachment> (*styleParam,
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
    setSize (680, 400);
}

TwinPitchEditor::~TwinPitchEditor() { setLookAndFeel (nullptr); }

void TwinPitchEditor::setupKnob (Knob& k, const String& id, const String& name)
{
    k.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k.slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    k.slider.setColour (Slider::textBoxTextColourId, TwinLookAndFeel::text);
    k.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 90, 18);
    k.slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    k.slider.setDoubleClickReturnValue (true, proc.apvts.getParameter (id)->convertFrom0to1 (
                                                  proc.apvts.getParameter (id)->getDefaultValue()));
    k.label.setText (name, dontSendNotification);
    k.label.setJustificationType (Justification::centred);
    k.label.setFont (FontOptions (12.5f, Font::bold));
    k.attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
}

void TwinPitchEditor::updateStyleButtons()
{
    for (int i = 0; i < 3; ++i)
        styleButtons[i].setToggleState (i == currentStyle, dontSendNotification);
}

void TwinPitchEditor::paint (Graphics& g)
{
    g.fillAll (TwinLookAndFeel::bg);
    auto r = getLocalBounds().toFloat().reduced (10.0f);
    g.setColour (TwinLookAndFeel::panel);
    g.fillRoundedRectangle (r, 14.0f);

    // title
    auto top = getLocalBounds().reduced (28, 20).removeFromTop (36);
    g.setColour (TwinLookAndFeel::text);
    g.setFont (FontOptions (26.0f, Font::bold));
    g.drawText ("TWIN", top, Justification::centredLeft);
    const int tw = GlyphArrangement::getStringWidthInt (FontOptions (26.0f, Font::bold), "TWIN");
    g.setColour (TwinLookAndFeel::accent);
    g.drawText ("PITCH", top.withTrimmedLeft (tw + 2), Justification::centredLeft);
    g.setColour (TwinLookAndFeel::dim);
    g.setFont (FontOptions (12.0f));
    g.drawText ("stereo micro-pitch widener", top.withTrimmedLeft (tw * 2 + 30), Justification::centredLeft);

    g.setFont (FontOptions (11.0f, Font::bold));
    g.drawText ("STYLE", getLocalBounds().reduced (28, 20).removeFromTop (36).removeFromRight (190),
                Justification::centredLeft);
}

void TwinPitchEditor::resized()
{
    auto r = getLocalBounds().reduced (28, 20);
    auto top = r.removeFromTop (36);
    auto styleArea = top.removeFromRight (150);
    const int bw = styleArea.getWidth() / 3;
    for (auto& b : styleButtons) b.setBounds (styleArea.removeFromLeft (bw));

    r.removeFromTop (12);
    auto upper = r.removeFromTop (170);

    // big detune knob on the left, spread view to the right
    auto detArea = upper.removeFromLeft (180);
    detune.label.setBounds (detArea.removeFromTop (16));
    detune.slider.setBounds (detArea);
    upper.removeFromLeft (14);
    spread.setBounds (upper);

    r.removeFromTop (14);
    Knob* small[] = { &delay, &focus, &mix, &output };
    const int kw = r.getWidth() / 4;
    for (auto* k : small)
    {
        auto a = r.removeFromLeft (kw).reduced (10, 0);
        k->label.setBounds (a.removeFromTop (16));
        k->slider.setBounds (a);
    }
}
