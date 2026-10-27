#include "PluginEditor.h"

using namespace juce;

//==============================================================================
VMLookAndFeel::VMLookAndFeel()
{
    setColour (Label::textColourId, dim);
}

void VMLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                      float a0, float a1, Slider& s)
{
    const bool on = s.isEnabled();
    const auto b = Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
    const float radius = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float track = jmax (3.0f, radius * 0.09f);
    const float ang = a0 + pos * (a1 - a0);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zero = bipolar ? a0 + (float) ((0.0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) * (a1 - a0) : a0;
    Colour col = s.getProperties().getWithDefault ("alt", false) ? alt : accent;
    if (! on) col = col.withSaturation (0.1f).withAlpha (0.35f);

    Path bgArc; bgArc.addCentredArc (c.x, c.y, radius - track, radius - track, 0, a0, a1, true);
    g.setColour (Colour (0xff332d3c));
    g.strokePath (bgArc, PathStrokeType (track, PathStrokeType::curved, PathStrokeType::rounded));
    Path val; val.addCentredArc (c.x, c.y, radius - track, radius - track, 0, jmin (zero, ang), jmax (zero, ang), true);
    g.setColour (col);
    g.strokePath (val, PathStrokeType (track, PathStrokeType::curved, PathStrokeType::rounded));

    const float br = radius - track * 2.6f;
    g.setGradientFill (ColourGradient (Colour (0xff3a3444), c.x, c.y - br, Colour (0xff1a1720), c.x, c.y + br, false));
    g.fillEllipse (c.x - br, c.y - br, br * 2, br * 2);
    g.setColour (Colour (0xff0d0b10));
    g.drawEllipse (c.x - br, c.y - br, br * 2, br * 2, 1.2f);

    Path ptr; const float pw = jmax (2.0f, br * 0.09f);
    ptr.addRoundedRectangle (-pw * 0.5f, -br * 0.92f, pw, br * 0.42f, pw * 0.5f);
    g.setColour (on ? text : dim.withAlpha (0.5f));
    g.fillPath (ptr, AffineTransform::rotation (ang).translated (c.x, c.y));
}

void VMLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    const Colour onCol = b.getProperties().getWithDefault ("alt", false) ? alt : accent;
    g.setColour (on ? onCol : (over ? Colour (0xff3a3444) : Colour (0xff2b2633)));
    g.fillRoundedRectangle (r, 6.0f);
}

void VMLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? bg : text);
    g.setFont (FontOptions (13.0f, Font::bold));
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred);
}

//==============================================================================
static String hzToNote (float hz, float& centsOut)
{
    const float m = 69.0f + 12.0f * std::log2 (hz / 440.0f);
    const int r = (int) std::round (m);
    centsOut = (m - (float) r) * 100.0f;
    return noteName (r);
}

void NoteDisplay::timerCallback()
{
    const float i = proc.engine.inputHz.load(), o = proc.engine.outputHz.load();
    if (i > 0) { inHz = i; holdIn = 8; } else if (holdIn > 0 && --holdIn == 0) inHz = 0;
    if (o > 0) { outHz = o; holdOut = 8; } else if (holdOut > 0 && --holdOut == 0) outHz = 0;
    repaint();
}

void NoteDisplay::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0f0d13));
    g.fillRoundedRectangle (r, 10.0f);

    auto drawSide = [&] (Rectangle<float> a, const String& title, float hz, Colour col)
    {
        g.setColour (VMLookAndFeel::dim);
        g.setFont (FontOptions (11.0f, Font::bold));
        g.drawText (title, a.removeFromTop (22.0f), Justification::centred);
        float cents = 0;
        const String note = hz > 0 ? hzToNote (hz, cents) : String ("--");
        g.setColour (hz > 0 ? col : VMLookAndFeel::dim.withAlpha (0.4f));
        g.setFont (FontOptions (40.0f, Font::bold));
        g.drawText (note, a.removeFromTop (48.0f), Justification::centred);
        g.setFont (FontOptions (12.0f));
        g.setColour (VMLookAndFeel::dim);
        g.drawText (hz > 0 ? String (hz, 1) + " Hz   " + (cents >= 0 ? "+" : "") + String ((int) std::round (cents)) + "c" : String(),
                    a.removeFromTop (18.0f), Justification::centred);
    };

    auto inner = r.reduced (10.0f, 0.0f).withSizeKeepingCentre (r.getWidth() - 20.0f, 90.0f);
    auto left = inner.removeFromLeft (inner.getWidth() * 0.5f);
    drawSide (left, "IN", inHz, VMLookAndFeel::text);
    drawSide (inner, "OUT", outHz, VMLookAndFeel::accent);

    g.setColour (VMLookAndFeel::dim.withAlpha (0.5f));
    const float cx = r.getCentreX(), cy = r.getCentreY() - 4.0f;
    Path arrow; arrow.addTriangle (cx - 5, cy - 7, cx - 5, cy + 7, cx + 6, cy);
    g.fillPath (arrow);
}

//==============================================================================
VoxMorphEditor::VoxMorphEditor (VoxMorphProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p)
{
    setLookAndFeel (&lnf);
    setupKnob (pitch,   IDs::pitch,     "PITCH");
    setupKnob (formant, IDs::formant,   "FORMANT", true);
    setupKnob (robot,   IDs::robotNote, "ROBOT NOTE");
    setupKnob (drive,   IDs::drive,     "DRIVE", true);
    setupKnob (mix,     IDs::mix,       "MIX");
    setupKnob (output,  IDs::output,    "OUTPUT");

    const char* names[] = { "TRANSPOSE", "QUANTIZE", "ROBOT" };
    modeAtt = std::make_unique<ParameterAttachment> (*proc.apvts.getParameter (IDs::mode),
        [this] (float v) { mode = (int) v; for (int i = 0; i < 3; ++i) modeButtons[i].setToggleState (i == mode, dontSendNotification); timerCallback(); });
    for (int i = 0; i < 3; ++i)
    {
        modeButtons[i].setButtonText (names[i]);
        modeButtons[i].onClick = [this, i] { modeAtt->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (modeButtons[i]);
    }
    modeAtt->sendInitialUpdate();

    linkButton.setClickingTogglesState (true);
    linkButton.getProperties().set ("alt", true);
    linkButton.setTooltip ("Formant follows pitch");
    linkAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::link, linkButton);
    addAndMakeVisible (linkButton);

    addAndMakeVisible (display);
    setSize (720, 470);
    startTimerHz (10);
}

VoxMorphEditor::~VoxMorphEditor() { setLookAndFeel (nullptr); }

void VoxMorphEditor::timerCallback()
{
    pitch.slider.setEnabled (mode != 2);   // robot ignores the pitch knob
    robot.slider.setEnabled (mode == 2);
    pitch.label.setText (mode == 1 ? "TRANSPOSE + SNAP" : "PITCH", dontSendNotification);
}

void VoxMorphEditor::setupKnob (Knob& k, const char* id, const String& name, bool altColour)
{
    auto* prm = proc.apvts.getParameter (id);
    k.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 90, 18);
    k.slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    k.slider.setColour (Slider::textBoxTextColourId, VMLookAndFeel::text);
    k.slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    if (altColour) k.slider.getProperties().set ("alt", true);
    k.att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
    k.slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    k.label.setText (name, dontSendNotification);
    k.label.setJustificationType (Justification::centred);
    k.label.setFont (FontOptions (12.0f, Font::bold));
    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
}

void VoxMorphEditor::paint (Graphics& g)
{
    g.fillAll (VMLookAndFeel::bg);
    g.setColour (VMLookAndFeel::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (10.0f), 14.0f);

    auto top = getLocalBounds().reduced (28, 20).removeFromTop (34);
    const auto f = FontOptions (26.0f, Font::bold);
    const int w1 = GlyphArrangement::getStringWidthInt (f, "VOX");
    g.setFont (f);
    g.setColour (VMLookAndFeel::text);  g.drawText ("VOX", top, Justification::centredLeft);
    g.setColour (VMLookAndFeel::accent); g.drawText ("MORPH", top.withTrimmedLeft (w1 + 2), Justification::centredLeft);
    g.setColour (VMLookAndFeel::dim);
    g.setFont (FontOptions (12.0f));
    g.drawText ("pitch & formant shifter", top.withTrimmedLeft (w1 + GlyphArrangement::getStringWidthInt (f, "MORPH") + 18),
                Justification::centredLeft);
}

void VoxMorphEditor::resized()
{
    auto r = getLocalBounds().reduced (28, 20);
    r.removeFromTop (46);

    auto modes = r.removeFromTop (34);
    const int bw = modes.getWidth() / 3;
    for (auto& b : modeButtons) b.setBounds (modes.removeFromLeft (bw).reduced (3, 0));
    r.removeFromTop (14);

    auto upper = r.removeFromTop (190);
    auto placeBig = [] (Knob& k, Rectangle<int> a)
    {
        k.label.setBounds (a.removeFromTop (16));
        k.slider.setBounds (a);
    };
    placeBig (pitch, upper.removeFromLeft (190));
    auto rightK = upper.removeFromRight (190);
    placeBig (formant, rightK);
    auto mid = upper.reduced (12, 6);
    linkButton.setBounds (mid.removeFromBottom (30).withSizeKeepingCentre (90, 28));
    mid.removeFromBottom (6);
    display.setBounds (mid);

    r.removeFromTop (14);
    Knob* small[] = { &robot, &drive, &mix, &output };
    const int kw = r.getWidth() / 4;
    for (auto* k : small)
    {
        auto a = r.removeFromLeft (kw).reduced (12, 0);
        k->label.setBounds (a.removeFromTop (16));
        k->slider.setBounds (a);
    }
}
