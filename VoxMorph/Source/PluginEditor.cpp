#include "PluginEditor.h"
#if VM_HAS_FONT
 #include "BinaryData.h"
#endif

using namespace juce;

Font vmui::mono (float size, bool bold)
{
   #if VM_HAS_FONT
    static auto reg = Typeface::createSystemTypefaceFor (BinaryData::SpaceMonoRegular_ttf, BinaryData::SpaceMonoRegular_ttfSize);
    static auto bld = Typeface::createSystemTypefaceFor (BinaryData::SpaceMonoBold_ttf, BinaryData::SpaceMonoBold_ttfSize);
    return Font (FontOptions (bold ? bld : reg).withHeight (size));
   #else
    return Font (FontOptions ("Menlo", size, bold ? Font::bold : Font::plain));
   #endif
}

//==============================================================================
void VMLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider& s)
{
    auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const bool big = (bool) s.getProperties().getWithDefault ("big", false);
    const float fade = s.isEnabled() ? 1.0f : 0.3f;
    const Colour col = (s.getProperties().getWithDefault ("violet", false) ? vmui::violet : vmui::ink).withAlpha (fade);
    const String label = s.getProperties()["label"].toString();
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zero = bipolar ? (float) ((0.0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) : 0.0f;
    auto isLit = [&] (float t) { return bipolar ? (pos >= zero ? (t >= zero - 1.0e-4f && t <= pos + 1.0e-4f) : (t <= zero + 1.0e-4f && t >= pos - 1.0e-4f))
                                               : (t <= pos + 1.0e-4f && pos > 0.001f); };
    if (big)
    {
        const auto c = area.getCentre(); const float R = 142.0f;
        for (int i = 0; i < 41; ++i)
        {
            const float t = (float) i / 40.0f, a = degreesToRadians (-225.0f + 270.0f * t);
            const bool lit = isLit (t); const float sz = lit ? 12.0f : 5.0f;
            g.setColour (lit ? col : col.withAlpha (0.35f * fade));
            g.fillRect (c.x + R * std::cos (a) - sz * 0.5f, c.y + R * std::sin (a) - sz * 0.5f, sz, sz);
        }
        for (int i = 0; i < 36; ++i)
        {
            const float a = degreesToRadians ((float) i * 10.0f);
            g.setColour (col.withAlpha ((0.2f + 0.45f * pos) * fade));
            g.fillRect (c.x + 98.0f * std::cos (a) - 1.5f, c.y + 98.0f * std::sin (a) - 1.5f, 3.0f, 3.0f);
        }
        g.setColour (col);
        g.setFont (vmui::mono (14.0f));
        g.drawFittedText (label, Rectangle<float> (c.x - 100.0f, c.y - 44.0f, 200.0f, 20.0f).toNearestInt(), Justification::centred, 1);
        g.setFont (vmui::mono (34.0f, true));
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
        const bool lit = isLit (t); const float sz = lit ? 6.0f : 3.0f;
        g.setColour (lit ? col : col.withAlpha (0.35f * fade));
        g.fillRect (c.x + r * std::cos (a) - sz * 0.5f, c.y + r * std::sin (a) - sz * 0.5f, sz, sz);
    }
    g.setColour (col);
    g.setFont (vmui::mono (12.0f, true));
    g.drawFittedText (s.getTextFromValue (s.getValue()), ring.reduced (size * 0.16f).toNearestInt(), Justification::centred, 1);
    g.setFont (vmui::mono (12.0f));
    g.drawText (label, labelArea, Justification::centred);
}

void VMLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const Colour c = b.getProperties().getWithDefault ("violet", false) ? vmui::violet : vmui::ink;
    if (b.getToggleState()) { g.setColour (c); g.fillRect (r); }
    else
    {
        if (over) { g.setColour (c.withAlpha (0.12f)); g.fillRect (r); }
        g.setColour (c.withAlpha (0.85f)); g.drawRect (r, 1.0f);
    }
}

void VMLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    const Colour c = b.getProperties().getWithDefault ("violet", false) ? vmui::violet : vmui::ink;
    g.setColour (b.getToggleState() ? vmui::ground : c);
    g.setFont (vmui::mono ((float) b.getProperties().getWithDefault ("fontSize", 12.0), true));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), Justification::centred, 1);
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
    auto side = [&] (Rectangle<float> a, const char* title, float hz)
    {
        g.setColour (vmui::ink); g.setFont (vmui::mono (12.0f));
        g.drawText (title, a.removeFromTop (20.0f), Justification::centred);
        float cents = 0;
        const String note = hz > 0 ? hzToNote (hz, cents) : String ("--");
        g.setColour (hz > 0 ? vmui::ink : vmui::ink.withAlpha (0.35f));
        g.setFont (vmui::mono (60.0f, true));
        g.drawText (note, a.removeFromTop (70.0f), Justification::centred);
        g.setFont (vmui::mono (12.0f));
        g.setColour (vmui::ink);
        g.drawText (hz > 0 ? String (hz, 1) + " Hz   " + (cents >= 0 ? "+" : "") + String ((int) std::round (cents)) + "c" : String(), a.removeFromTop (18.0f), Justification::centred);
    };
    auto left = r.removeFromLeft (r.getWidth() * 0.5f);
    side (left, "IN", inHz); side (r, "OUT", outHz);
    // arrow made of dots
    const float cx = left.getRight(), cy = left.getY() + 66.0f;
    g.setColour (vmui::ink);
    for (int i = 0; i < 6; ++i) g.fillRect (cx - 34.0f + (float) i * 8.0f, cy - 2.0f, 6.0f, 6.0f);
    for (int k = 1; k <= 3; ++k) { g.fillRect (cx + 8.0f - (float) k * 8.0f + 8.0f, cy - 2.0f - (float) k * 8.0f + 8.0f, 6.0f, 6.0f); g.fillRect (cx + 8.0f - (float) k * 8.0f + 8.0f, cy - 2.0f + (float) k * 8.0f - 8.0f, 6.0f, 6.0f); }
}

//==============================================================================
VoxMorphEditor::VoxMorphEditor (VoxMorphProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p)
{
    setLookAndFeel (&lnf);
    setupKnob (pitch,   IDs::pitch,     "PITCH",      true,  false);
    setupKnob (formant, IDs::formant,   "FORMANT",    false, true);
    setupKnob (robot,   IDs::robotNote, "ROBOT NOTE", false, false);
    setupKnob (drive,   IDs::drive,     "DRIVE",      false, true);
    setupKnob (mix,     IDs::mix,       "MIX",        false, false);
    setupKnob (output,  IDs::output,    "OUTPUT",     false, false);

    const char* names[] = { "TRANSPOSE", "QUANTIZE", "ROBOT" };
    modeAtt = std::make_unique<ParameterAttachment> (*proc.apvts.getParameter (IDs::mode),
        [this] (float v) { mode = (int) v; for (int i = 0; i < 3; ++i) modeButtons[i].setToggleState (i == mode, dontSendNotification); timerCallback(); });
    for (int i = 0; i < 3; ++i)
    {
        modeButtons[i].setButtonText (names[i]);
        modeButtons[i].getProperties().set ("fontSize", 11.0);
        modeButtons[i].onClick = [this, i] { modeAtt->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (modeButtons[i]);
    }
    modeAtt->sendInitialUpdate();

    linkButton.setClickingTogglesState (true);
    linkButton.getProperties().set ("violet", true);
    linkButton.setTooltip ("Formant follows pitch");
    linkAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::link, linkButton);
    addAndMakeVisible (linkButton);
    addAndMakeVisible (display);
    setSize (960, 780);
    setScaleFactor (0.75f);     // the plugin window is 720 x 585
    startTimerHz (10);
}

VoxMorphEditor::~VoxMorphEditor() { setLookAndFeel (nullptr); }

void VoxMorphEditor::timerCallback()
{
    pitch.slider.setEnabled (mode != 2);   // robot ignores the pitch knob
    robot.slider.setEnabled (mode == 2);
    pitch.slider.getProperties().set ("label", mode == 1 ? "TRANSPOSE + SNAP" : "PITCH");
    repaint();
}

void VoxMorphEditor::setupKnob (Knob& k, const char* id, const String& name, bool big, bool violet)
{
    auto* prm = proc.apvts.getParameter (id);
    k.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    k.slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    k.slider.getProperties().set ("label", name);
    k.slider.getProperties().set ("big", big);
    k.slider.getProperties().set ("violet", violet);
    k.slider.setMouseDragSensitivity (big ? 260 : 200);
    k.att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
    k.slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    addAndMakeVisible (k.slider);
}

static const char* kGlyph[8][7] = {
    { "10001", "10001", "10001", "10001", "10001", "01010", "00100" },   // V
    { "01110", "10001", "10001", "10001", "10001", "10001", "01110" },   // O
    { "10001", "10001", "01010", "00100", "01010", "10001", "10001" },   // X
    { "10001", "11011", "10101", "10101", "10001", "10001", "10001" },   // M
    { "01110", "10001", "10001", "10001", "10001", "10001", "01110" },   // O
    { "11110", "10001", "10001", "11110", "10100", "10010", "10001" },   // R
    { "11110", "10001", "10001", "11110", "10000", "10000", "10000" },   // P
    { "10001", "10001", "10001", "11111", "10001", "10001", "10001" },   // H
};


void VoxMorphEditor::paint (Graphics& g)
{
    g.fillAll (vmui::ground);
    g.setColour (vmui::ink.withAlpha (0.5f));
    for (int y : { 124, 600 }) g.fillRect (0, y, getWidth(), 1);
    for (int x : { 72, 888 }) g.fillRect (x, 0, 1, getHeight());

    for (int li = 0; li < 8; ++li)
    {
        g.setColour (li < 3 ? vmui::ink : vmui::violet);   // VOX in wine, MORPH in violet
        for (int ry = 0; ry < 7; ++ry)
            for (int rx = 0; rx < 5; ++rx)
                if (kGlyph[li][ry][rx] == '1') g.fillRect (96 + li * 56 + rx * 8, 34 + ry * 8, 7, 7);
    }
    struct Speck { int x, y; bool bright; };
    for (auto s : { Speck { 40, 60, true }, { 160, 110, false }, { 930, 170, true }, { 30, 380, false }, { 912, 560, true }, { 300, 612, true }, { 44, 700, false } })
    { g.setColour (s.bright ? vmui::butter : vmui::white); g.fillRect (s.x, s.y, 4, 4); }
}

void VoxMorphEditor::resized()
{
    pitch.slider.setBounds (290 - 170, 350 - 170, 340, 340);
    for (int i = 0; i < 3; ++i) modeButtons[i].setBounds (564 + i * 106, 44, 100, 40);
    Knob* top[3] = { &formant, &robot, &drive };
    const int xs[3] = { 600, 712, 824 };
    for (int i = 0; i < 3; ++i) top[i]->slider.setBounds (xs[i] - 46, 300 - 46, 92, 114);
    mix.slider.setBounds (600 - 46, 480 - 46, 92, 114);
    output.slider.setBounds (712 - 46, 480 - 46, 92, 114);
    linkButton.setBounds (788, 454, 92, 40);
    display.setBounds (96, 628, 620, 110);
}
