#include "PluginEditor.h"
#if SN_HAS_FONT
 #include "BinaryData.h"
#endif

using namespace juce;

Font snui::mono (float size, bool bold)
{
   #if SN_HAS_FONT
    static auto reg = Typeface::createSystemTypefaceFor (BinaryData::SpaceMonoRegular_ttf, BinaryData::SpaceMonoRegular_ttfSize);
    static auto bld = Typeface::createSystemTypefaceFor (BinaryData::SpaceMonoBold_ttf, BinaryData::SpaceMonoBold_ttfSize);
    return Font (FontOptions (bold ? bld : reg).withHeight (size));
   #else
    return Font (FontOptions ("Menlo", size, bold ? Font::bold : Font::plain));
   #endif
}

// 5 x 7 dot letters
static const char* glyph (juce::juce_wchar c)
{
    switch (c)
    {
        case 'A': return "01110100011000111111100011000110001"; case 'B': return "11110100011000111110100011000111110";
        case 'C': return "01110100011000010000100001000101110"; case 'D': return "11110100011000110001100011000111110";
        case 'E': return "11111100001000011110100001000011111"; case 'F': return "11111100001000011110100001000010000";
        case 'G': return "01110100011000010111100011000101111"; case 'H': return "10001100011000111111100011000110001";
        case 'I': return "11111001000010000100001000010011111"; case 'J': return "00111000100001000010100101001001100";
        case 'K': return "10001100101010011000101001001010001"; case 'L': return "10000100001000010000100001000011111";
        case 'M': return "10001110111010110101100011000110001"; case 'N': return "10001110011010110101100111000110001";
        case 'O': return "01110100011000110001100011000101110"; case 'P': return "11110100011000111110100001000010000";
        case 'Q': return "01110100011000110001101011001001101"; case 'R': return "11110100011000111110101001001010001";
        case 'S': return "01111100001000001110000010000111110"; case 'T': return "11111001000010000100001000010000100";
        case 'U': return "10001100011000110001100011000101110"; case 'V': return "10001100011000110001100010101000100";
        case 'W': return "10001100011000110101101011101110001"; case 'X': return "10001100010101000100010101000110001";
        case 'Y': return "10001100010101000100001000010000100"; case 'Z': return "11111000010001000100010001000011111";
        case '#': return "01010010101111101010111110101001010"; case '.': return "00000000000000000000000000001100011";
        default:  return nullptr;
    }
}

//==============================================================================
void SNLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider& s)
{
    auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    auto labelArea = area.removeFromBottom (22.0f);
    const float size = jmin (area.getWidth(), area.getHeight());
    const auto c = area.withSizeKeepingCentre (size, size).getCentre();
    const float r = size * 0.5f - 8.0f;
    for (int i = 0; i < 21; ++i)
    {
        const float t = (float) i / 20.0f, a = degreesToRadians (-225.0f + 270.0f * t);
        const bool on = t <= pos + 1.0e-4f && pos > 0.001f; const float sz = on ? 6.0f : 3.0f;
        g.setColour (on ? snui::ink : snui::ink.withAlpha (0.35f));
        g.fillRect (c.x + r * std::cos (a) - sz * 0.5f, c.y + r * std::sin (a) - sz * 0.5f, sz, sz);
    }
    const String word = pos < 0.34f ? "Now" : (pos < 0.67f ? "Section" : "Song");
    g.setColour (snui::ink); g.setFont (snui::mono (13.0f, true));
    g.drawText (word, Rectangle<float> (c.x - 40.0f, c.y - 10.0f, 80.0f, 20.0f), Justification::centred);
    g.setFont (snui::mono (12.0f));
    g.drawText (s.getProperties()["label"].toString(), labelArea, Justification::centred);
}

void SNLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    if (b.getToggleState()) { g.setColour (snui::ink); g.fillRect (r); }
    else
    {
        if (over) { g.setColour (snui::ink.withAlpha (0.12f)); g.fillRect (r); }
        g.setColour (snui::ink.withAlpha (0.85f)); g.drawRect (r, 1.0f);
    }
}

void SNLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? snui::ground : snui::ink);
    g.setFont (snui::mono (12.0f, true));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), Justification::centred, 1);
}

//==============================================================================
SnitchEditor::SnitchEditor (SnitchProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    memory.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    memory.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    memory.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    memory.getProperties().set ("label", "MEMORY");
    memory.setDoubleClickReturnValue (true, 0.5);
    memoryAtt = std::make_unique<SliderAtt> (proc.apvts, IDs::memory, memory);
    addAndMakeVisible (memory);

    setupToggle (lockBtn, IDs::lock, "LOCK");
    setupToggle (midiBtn, IDs::midiOut, "MIDI OUT");
    setupToggle (listenBtn, IDs::listen, "LISTENING");

    basicBtn.setButtonText ("BASIC"); modesBtn.setButtonText ("MODES"); allBtn.setButtonText ("ALL"); copyBtn.setButtonText ("COPY");
    int idx = 0;
    for (auto* b : { &basicBtn, &modesBtn, &allBtn })
    {
        const int v = idx++;
        b->onClick = [this, v] { proc.setParam (IDs::scales, (float) v); };
        addAndMakeVisible (*b);
    }
    copyBtn.onClick = [this] { SystemClipboard::copyTextToClipboard (answerName()); };
    addAndMakeVisible (copyBtn);

    setSize (960, 780);
    setScaleFactor (0.75f);     // the plugin window is 720 x 585
    startTimerHz (30);
}

SnitchEditor::~SnitchEditor() { setLookAndFeel (nullptr); }

void SnitchEditor::setupToggle (Toggle& t, const char* id, const String& text)
{
    t.button.setButtonText (text); t.button.setClickingTogglesState (true);
    t.att = std::make_unique<ButtonAtt> (proc.apvts, id, t.button); addAndMakeVisible (t.button);
}

void SnitchEditor::timerCallback()
{
    const int sc = (int) proc.getParam (IDs::scales);
    basicBtn.setToggleState (sc == 0, dontSendNotification);
    modesBtn.setToggleState (sc == 1, dontSendNotification);
    allBtn.setToggleState (sc == 2, dontSendNotification);
    listenBtn.button.setButtonText (proc.getParam (IDs::listen) > 0.5f ? "LISTENING" : "PAUSED");
    repaint();
}

void SnitchEditor::resized()
{
    memory.setBounds (790 - 46, 215 - 46, 92, 114);
    listenBtn.button.setBounds (740, 44, 140, 40);
    basicBtn.setBounds (96, 628, 84, 40); modesBtn.setBounds (188, 628, 84, 40); allBtn.setBounds (280, 628, 84, 40);
    lockBtn.button.setBounds (560, 628, 90, 40); midiBtn.button.setBounds (658, 628, 110, 40); copyBtn.setBounds (776, 628, 104, 40);
}

String SnitchEditor::answerName() const
{
    const int r = proc.engine.root.load();
    if (proc.engine.state.load() != 2 || r < 0) return {};
    int n; const auto& d = snitch::scaleDefs (n)[proc.engine.scaleIdx.load()];
    String name = String (d.name).toLowerCase();
    return String (snitch::noteName (r)) + " " + name;
}

void SnitchEditor::drawDotText (Graphics& g, const String& text, int x, int y, int width)
{
    const int n = text.length(); if (n == 0) return;
    const int d = jlimit (6, 13, width / (n * 6));
    for (int i = 0; i < n; ++i)
    {
        const char* gl = glyph (text[i]); if (gl == nullptr) continue;
        for (int ry = 0; ry < 7; ++ry) for (int rx = 0; rx < 5; ++rx)
            if (gl[ry * 5 + rx] == '1') g.fillRect (x + i * 6 * d + rx * d, y + ry * d, d - 1, d - 1);
    }
}

static const char* kLogo[6][7] = {
    { "01111", "10000", "10000", "01110", "00001", "00001", "11110" },   // S
    { "10001", "11001", "10101", "10101", "10011", "10001", "10001" },   // N
    { "11111", "00100", "00100", "00100", "00100", "00100", "11111" },   // I
    { "11111", "00100", "00100", "00100", "00100", "00100", "00100" },   // T
    { "01110", "10001", "10000", "10000", "10000", "10001", "01110" },   // C
    { "10001", "10001", "10001", "11111", "10001", "10001", "10001" } }; // H

void SnitchEditor::paint (Graphics& g)
{
    g.fillAll (snui::ground);
    g.setColour (snui::ink.withAlpha (0.5f));
    for (int y : { 124, 600 }) g.fillRect (0, y, getWidth(), 1);
    for (int x : { 72, 888 }) g.fillRect (x, 0, 1, getHeight());

    g.setColour (snui::ink);
    for (int li = 0; li < 6; ++li)
        for (int ry = 0; ry < 7; ++ry)
            for (int rx = 0; rx < 5; ++rx)
                if (kLogo[li][ry][rx] == '1') g.fillRect (96 + li * 56 + rx * 8, 34 + ry * 8, 7, 7);

    const auto& e = proc.engine;
    const int st = e.state.load(), root = e.root.load();
    int nS; const auto* defs = snitch::scaleDefs (nS);

    // the answer
    String big, sub, alt;
    if (st == 2 && root >= 0)
    {
        const auto& d = defs[e.scaleIdx.load()];
        big = String (snitch::noteName (root)) + " " + String (d.name);
        if (d.parentUp >= 0)
        {
            const int parent = (root + d.parentUp) % 12;
            sub = d.parentUp == 0 ? "same notes as " + String (snitch::noteName ((root + 9) % 12)) + " MINOR" : "same notes as " + String (snitch::noteName (parent)) + " MAJOR";
            sub += "  -  home note " + String (snitch::noteName (root));
        }
        else
        {
            sub = "notes"; const unsigned m = e.notesMask.load();
            for (int pc = 0; pc < 12; ++pc) { const int k = (root + pc) % 12; if (m & (1u << k)) sub << " " << snitch::noteName (k); }
        }
        if (e.confPct.load() < 66 && e.root2.load() >= 0) alt = "or " + String (snitch::noteName (e.root2.load())) + " " + String (defs[e.scaleIdx2.load()].name);
    }
    else if (st == 1) { big = "LISTENING"; sub = "collecting notes..."; }
    else { big = "PLAY ME"; sub = "waiting for sound"; }
    g.setColour (snui::ink);
    drawDotText (g, big, 96, 160, 640);
    g.setFont (snui::mono (15.0f, true)); g.drawText (sub, 96, 284, 700, 24, Justification::centredLeft);

    // confidence
    const int conf = st == 2 ? e.confPct.load() : 0;
    const String word = conf >= 66 ? "SURE" : (conf >= 33 ? "MAYBE" : "GUESSING");
    g.setFont (snui::mono (12.0f)); g.drawText (st == 2 ? word : "", 96, 326, 70, 16, Justification::centredLeft);
    const int litSq = (int) std::round (conf / 10.0);
    for (int i = 0; i < 10; ++i) { g.setColour (snui::ink.withAlpha (i < litSq ? 1.0f : 0.3f)); g.fillRect (172 + i * 16, 326, 11, 11); }
    g.setColour (snui::ink); g.setFont (snui::mono (12.0f));
    if (alt.isNotEmpty()) g.drawText (alt, 350, 322, 300, 18, Justification::centredLeft);

    // WHAT IT HEARD: instant attack, slow release (the engine does the release)
    g.setFont (snui::mono (11.0f)); g.drawText ("WHAT IT HEARD", 96, 380, 200, 16, Justification::centredLeft);
    const unsigned mask = st == 2 ? e.notesMask.load() : 0u;
    for (int k = 0; k < 12; ++k)
    {
        const int x = 110 + k * 62; const float v = e.shown[k].load(); const int h = (int) std::round (jlimit (0.0f, 1.0f, v) * 14.0f);
        const bool inScale = (mask & (1u << k)) != 0 || mask == 0;
        for (int j = 0; j < 14; ++j)
        {
            const bool on = j < h; const float sz = on ? (inScale ? 9.0f : 5.0f) : 3.0f;
            g.setColour (snui::ink.withAlpha (on ? (inScale ? 1.0f : 0.45f) : 0.2f));
            g.fillRect (x + (12.0f - sz) * 0.5f, 520.0f - j * 12.0f + (12.0f - sz) * 0.5f, sz, sz);
        }
        g.setColour (snui::ink); g.setFont (snui::mono (12.0f, k == root));
        g.drawText (snitch::noteName (k), x - 12, 530, 36, 16, Justification::centredLeft);
        if (k == root) g.fillRect (x, 550, 12, 3);
    }
    const float cents = e.tuningCents.load();
    if (st >= 1 && std::abs (cents) >= 8.0f)
    {
        g.setFont (snui::mono (11.0f));
        g.drawText ("TUNING " + String (cents > 0 ? "+" : "") + String (juce::roundToInt (cents)) + " CENTS", 600, 380, 280, 16, Justification::centredRight);
    }
    g.setFont (snui::mono (11.0f)); g.drawText ("SCALES", 96, 608, 100, 16, Justification::centredLeft);
}
