#include "PluginEditor.h"
#if SW_HAS_FONT
 #include "BinaryData.h"
#endif

using namespace juce;

//==============================================================================
Font swui::mono (float size, bool bold)
{
   #if SW_HAS_FONT
    static auto reg = Typeface::createSystemTypefaceFor (BinaryData::SpaceMonoRegular_ttf, BinaryData::SpaceMonoRegular_ttfSize);
    static auto bld = Typeface::createSystemTypefaceFor (BinaryData::SpaceMonoBold_ttf, BinaryData::SpaceMonoBold_ttfSize);
    return Font (FontOptions (bold ? bld : reg).withHeight (size));
   #else
    return Font (FontOptions ("Menlo", size, bold ? Font::bold : Font::plain));
   #endif
}
Font swui::sansItalic (float size)
{
   #if SW_HAS_FONT
    static auto it = Typeface::createSystemTypefaceFor (BinaryData::SpaceMonoItalic_ttf, BinaryData::SpaceMonoItalic_ttfSize);
    return Font (FontOptions (it).withHeight (size));
   #else
    return Font (FontOptions ("Menlo", size, Font::italic));
   #endif
}

//==============================================================================
void SWLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider& s)
{
    auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    auto labelArea = area.removeFromBottom (22.0f);
    const float size = jmin (area.getWidth(), area.getHeight());
    auto ring = area.withSizeKeepingCentre (size, size);
    const auto c = ring.getCentre();
    const int N = 21; const float r = size * 0.5f - 8.0f;
    for (int i = 0; i < N; ++i)
    {
        const float t = (float) i / (float) (N - 1), a = degreesToRadians (-225.0f + 270.0f * t);
        const bool lit = t <= pos + 1.0e-4f && pos > 0.001f; const float sz = lit ? 6.0f : 3.0f;
        g.setColour (lit ? swui::ink : swui::ink.withAlpha (0.4f));
        g.fillRect (c.x + r * std::cos (a) - sz * 0.5f, c.y + r * std::sin (a) - sz * 0.5f, sz, sz);
    }
    g.setColour (swui::ink);
    g.setFont (swui::mono (15.0f, true));
    g.drawFittedText (s.getTextFromValue (s.getValue()), ring.reduced (size * 0.2f).toNearestInt(), Justification::centred, 1);
    g.setFont (swui::mono (12.0f));
    g.drawText (s.getProperties()["label"].toString(), labelArea, Justification::centred);
}

void SWLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool solid = b.getProperties().getWithDefault ("solid", false);
    if (solid || b.getToggleState()) { g.setColour (solid && down ? swui::ink.withAlpha (0.8f) : swui::ink); g.fillRect (r); }
    else
    {
        if (over) { g.setColour (swui::ink.withAlpha (0.12f)); g.fillRect (r); }
        g.setColour (swui::ink.withAlpha (b.isEnabled() ? 0.85f : 0.3f));
        g.drawRect (r, 1.0f);
    }
}

void SWLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    const bool filled = (bool) b.getProperties().getWithDefault ("solid", false) || b.getToggleState();
    g.setColour (filled ? swui::ground : swui::ink);
    g.setFont (swui::mono ((float) b.getProperties().getWithDefault ("fontSize", 12.0), true));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), (bool) b.getProperties().getWithDefault ("left", false) ? Justification::centredLeft : Justification::centred, 1);
}

//==============================================================================
void RollView::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const int rows = 14; const float rh = 11.0f;
    const int key = (int) proc.getParam (IDs::key), sc = (int) proc.getParam (IDs::scale);
    const auto& S = secretweapon::getScale (sc);
    auto inScale = [&] (int midi) { const int pc = ((midi - key) % 12 + 12) % 12; for (int d = 0; d < S.n; ++d) if (S.semis[d] == pc) return true; return false; };

    auto& v = proc.engine.viz;
    const double now = v.nowSample.load();
    const double sr = jmax (8000.0, v.sampleRate.load());
    const double secs = 6.0, pxPerSec = b.getWidth() / secs;
    const int wi = v.writeIndex.load (std::memory_order_acquire);
    int live = 0;
    for (int k = 1; k <= jmin (wi, (int) secretweapon::VizLog::kSize); ++k)
    {
        const auto e = v.ev[(wi - k) & (secretweapon::VizLog::kSize - 1)];
        if (now - e.start < secs * sr && e.midi >= 0) ++live;
    }
    const bool demo = live == 0;

    // keep the window around the music
    float target = (float) (key + 57 + 5);
    if (! demo) { float s = 0; int c = 0; for (int k = 1; k <= jmin (wi, 24); ++k) { const auto e = v.ev[(wi - k) & (secretweapon::VizLog::kSize - 1)]; if (e.kind == 0 && e.midi > 0) { s += e.midi; ++c; } } if (c) target = s / (float) c; }
    centre += 0.15f * (target - centre);
    const int topMidi = (int) std::lround (centre) + 7;

    for (int r = 0; r < rows; ++r)
        if (inScale (topMidi - r)) { g.setColour (swui::ink.withAlpha (0.14f)); g.fillRect (0.0f, r * rh, b.getWidth(), 9.0f); }

    auto drawNote = [&] (float x, float w, float midi, bool secret, float alpha)
    {
        float m = midi; while (m > topMidi + 0.5f) m -= 12.0f; while (m < topMidi - rows + 0.5f) m += 12.0f;
        const float y = (topMidi - std::round (m)) * rh;
        const Rectangle<float> rc (x, y, jmax (4.0f, w), 9.0f);
        if (secret) { g.setColour (swui::white.withAlpha (alpha)); g.fillRect (rc); g.setColour (swui::ink.withAlpha (alpha)); g.drawRect (rc, 1.0f); }
        else { g.setColour (swui::ink.withAlpha (alpha)); g.fillRect (rc); }
    };

    if (demo)
    {
        // what it will look like: a short melody with a few secret notes beside it
        const int deg[12] = { 0, 2, 4, 2, 5, 4, 2, 0, 4, 6, 5, 4 };
        for (int i = 0; i < 12; ++i)
        {
            const int d = deg[i]; const int semi = S.semis[d % S.n] + 12 * (d / S.n);
            const float m = (float) (key + 57 + semi + 12);
            const float x = (float) i * 62.0f;
            drawNote (x, 48.0f, m, false, 1.0f);
            if (((i * 37 + sc * 11 + key * 7) % 100) < 55 && proc.getParam (IDs::on) > 0.5f)
            {
                const int d2 = d + (int[]) { 2, -2, 1, 4, -3 }[i % 5]; const int s2 = S.semis[((d2 % S.n) + S.n) % S.n] + 12 * (int) std::floor ((double) d2 / S.n);
                drawNote (x + 26.0f, 22.0f, (float) (key + 57 + s2 + 12), true, 0.95f);
            }
        }
        g.setColour (swui::ink); g.setFont (swui::sansItalic (12.0f));
        g.drawText ("play something", getLocalBounds().removeFromBottom (13), Justification::centred);
        return;
    }
    for (int k = jmin (wi, (int) secretweapon::VizLog::kSize); k >= 1; --k)
    {
        const auto e = v.ev[(wi - k) & (secretweapon::VizLog::kSize - 1)];
        if (e.midi < 0) continue;
        const double age = now - e.start; if (age > secs * sr + 2.0 * sr) continue;
        const float x = b.getWidth() - (float) (age / sr * pxPerSec);
        const float w = (float) ((e.end - e.start) / sr * pxPerSec) - 2.0f;
        if (x + w < 0 || x > b.getWidth()) continue;
        drawNote (x, w, e.midi, e.kind == 1, 1.0f);
    }
}

//==============================================================================
SecretWeaponEditor::SecretWeaponEditor (SecretWeaponProcessor& p)
    : AudioProcessorEditor (&p), proc (p), roll (p)
{
    setLookAndFeel (&lnf);
    StringArray keys; for (int i = 0; i < 12; ++i) keys.add (secretweapon::keyName (i));
    StringArray scales; for (int i = 0; i < 10; ++i) scales.add (secretweapon::getScale (i).name);
    setupChoice (key, IDs::key, keys, 13.0f);
    setupChoice (scale, IDs::scale, scales, 11.0f);
    setupChoice (source, IDs::source, { "GUITAR", "KEYS", "VOX", "BASS", "SYNTH", "LOOP" }, 12.0f);
    setupKnob (density, IDs::density, "DENSITY");
    setupKnob (level, IDs::level, "LEVEL");
    setupKnob (range, IDs::range, "RANGE");
    setupKnob (length, IDs::length, "LENGTH");
    const char* ids[6] = { IDs::stGhost, IDs::stSparkle, IDs::stSteps, IDs::stChord, IDs::stAnswer, IDs::stTipsy };
    const char* names[6] = { "GHOST NOTES", "SPARKLE", "STEPS", "CHORD TONES", "ANSWER", "TIPSY NOTES" };
    for (int i = 0; i < 6; ++i) { setupToggle (styles[i], ids[i], names[i]); styles[i].button.getProperties().set ("left", true); styles[i].button.getProperties().set ("fontSize", 11.0); }
    setupToggle (onBtn, IDs::on, "ON");
    onBtn.button.getProperties().set ("fontSize", 13.0);
    mixBtn.getProperties().set ("fontSize", 12.0);
    mixBtn.onClick = [this] { proc.setParam (IDs::mix, (float) (((int) proc.getParam (IDs::mix) + 1) % 4)); timerCallback(); };
    listenBtn.getProperties().set ("solid", true);
    listenBtn.onClick = [this] { proc.listen (8); listenPending = true; heard = "listening..."; timerCallback(); };
    for (auto* b : { &mixBtn, &listenBtn }) addAndMakeVisible (*b);
    addAndMakeVisible (roll);
    setSize (960, 780);
    setScaleFactor (0.75f);     // the plugin window is 720 x 585
    timerCallback();
    startTimerHz (12);
}

SecretWeaponEditor::~SecretWeaponEditor() { setLookAndFeel (nullptr); }

void SecretWeaponEditor::setupKnob (Knob& k, const char* id, const String& label)
{
    k.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    k.slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    k.slider.getProperties().set ("label", label);
    k.slider.setMouseDragSensitivity (220);
    k.att = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
    if (auto* prm = proc.apvts.getParameter (id))
        k.slider.setDoubleClickReturnValue (true, (double) dynamic_cast<RangedAudioParameter*> (prm)->convertFrom0to1 (prm->getDefaultValue()));
    addAndMakeVisible (k.slider);
}
void SecretWeaponEditor::setupToggle (Toggle& t, const char* id, const String& text)
{
    t.button.setButtonText (text); t.button.setClickingTogglesState (true);
    t.att = std::make_unique<ButtonAtt> (proc.apvts, id, t.button); addAndMakeVisible (t.button);
}
void SecretWeaponEditor::setupChoice (Choice& c, const char* id, const StringArray& labels, float fontSize)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = c.buttons.add (new TextButton (labels[i]));
        b->getProperties().set ("fontSize", (double) fontSize);
        b->onClick = [&c, i, this] { c.att->setValueAsCompleteGesture ((float) i); heard = "set by hand"; };
        addAndMakeVisible (b);
    }
    c.att = std::make_unique<ParameterAttachment> (*proc.apvts.getParameter (id), [&c] (float v)
    {
        c.value = (int) std::round (v);
        for (int i = 0; i < c.buttons.size(); ++i) c.buttons[i]->setToggleState (i == c.value, dontSendNotification);
    });
    c.att->sendInitialUpdate();
}

void SecretWeaponEditor::timerCallback()
{
    static const char* mixes[4] = { "100", "75", "50", "25" };
    mixBtn.setButtonText (String ("MIX ") + mixes[(int) proc.getParam (IDs::mix)] + "%");
    onBtn.button.setButtonText (proc.getParam (IDs::on) > 0.5f ? "ON" : "OFF");
    auto& e = proc.engine;
    const int ls = e.listenState.load();
    if (ls == 1) heard = "listening... " + String (juce::jlimit (0, proc.listenBars.load(), (int) std::floor (e.listenProgress.load() * proc.listenBars.load()))) + "/" + String (proc.listenBars.load()) + " bars";
    if (ls == 2 && listenPending)
    {
        listenPending = false; e.listenState.store (0);
        const int k = e.heardKey.load(), s = e.heardScale.load();
        proc.setParam (IDs::key, (float) k); proc.setParam (IDs::scale, (float) s);
        heard = String ("heard: ") + secretweapon::keyName (k) + " " + String (secretweapon::getScale (s).name).toLowerCase() + " (" + String (e.heardPct.load()) + "% in scale)";
    }
    if (ls == 0 && listenPending) { listenPending = false; heard = "heard nothing, try again"; }
    repaint();
}

//==============================================================================
static const char* kFont[13][7] = {
    { "01111", "10000", "10000", "01110", "00001", "00001", "11110" },   // S
    { "11111", "10000", "10000", "11110", "10000", "10000", "11111" },   // E
    { "01111", "10000", "10000", "10000", "10000", "10000", "01111" },   // C
    { "11110", "10001", "10001", "11110", "10100", "10010", "10001" },   // R
    { "11111", "00100", "00100", "00100", "00100", "00100", "00100" },   // T
    { "10001", "10001", "10001", "10101", "10101", "10101", "01010" },   // W
    { "01110", "10001", "10001", "11111", "10001", "10001", "10001" },   // A
    { "11110", "10001", "10001", "11110", "10000", "10000", "10000" },   // P
    { "01110", "10001", "10001", "10001", "10001", "10001", "01110" },   // O
    { "10001", "11001", "10101", "10011", "10001", "10001", "10001" },   // N
    {}, {}, {} };

void SecretWeaponEditor::paint (Graphics& g)
{
    g.fillAll (swui::ground);
    g.setColour (swui::ink.withAlpha (0.55f));
    for (int y : { 112, 462 }) g.fillRect (0, y, getWidth(), 1);
    for (int x : { 72, 888 }) g.fillRect (x, 0, 1, getHeight());

    // dot-matrix logo
    static const int word[13] = { 0, 1, 2, 3, 1, 4, -1, 5, 1, 6, 7, 8, 9 };
    int x0 = 96;
    g.setColour (swui::ink);
    for (int li = 0; li < 13; ++li)
    {
        if (word[li] < 0) { x0 += 20; continue; }
        for (int ry = 0; ry < 7; ++ry) for (int rx = 0; rx < 5; ++rx)
            if (kFont[word[li]][ry][rx] == '1') g.fillRect (x0 + rx * 5, 28 + ry * 5, 4, 4);
        x0 += 36;
    }
    g.setFont (swui::sansItalic (13.0f));
    g.drawText ("it listens to your melody and slips in notes that belong", 96, 72, 560, 18, Justification::left);

    g.setFont (swui::mono (11.0f));
    g.drawText ("KEY", 96, 138, 60, 14, Justification::left);
    g.drawText ("SCALE", 96, 178, 60, 14, Justification::left);
    g.setFont (swui::mono (12.0f));
    g.drawText (heard, 322, 252, 480, 16, Justification::left);

    g.setFont (swui::mono (11.0f));
    g.drawText ("WHAT IT ADDS", 568, 486, 200, 14, Justification::left);

    // legend
    g.setFont (swui::mono (10.0f));
    g.setColour (swui::ink);
    g.fillRect (640, 470, 8, 8);
    g.drawText ("your melody", 652, 466, 100, 14, Justification::left);
    g.setColour (swui::white); g.fillRect (766, 470, 8, 8);
    g.setColour (swui::ink); g.drawRect (766, 470, 8, 8, 1);
    g.drawText ("secret notes", 778, 466, 100, 14, Justification::left);

    // help text
    static const char* styleHelp[6] = {
        "GHOST NOTES - quiet repeats of the melody, moved to another note in the scale", "SPARKLE - short high plinks, an octave or two above",
        "STEPS - a quick neighbour note, one step away in the scale", "CHORD TONES - a 3rd or 5th under the strong notes",
        "ANSWER - after a phrase, a short reply made from its last notes, ending on a note of the scale", "TIPSY NOTES - wrong on purpose, but never out of the scale" };
    static const char* srcHelp[6] = {
        "plucked notes: it follows the attacks and keeps added notes short and soft", "follows chords too: adds only safe notes (octaves and fifths) when it hears more than one",
        "follows pitch and breath: added notes are quiet harmonies, shaped like the voice", "low and mono: only roots, fifths and octaves, nothing muddy",
        "long notes allowed: wide range, glides between added notes", "full mix or sample: plays it safe, octaves and fifths only" };
    int first = 0; for (int i = 0; i < 6; ++i) if (styles[i].button.getToggleState()) { first = i; break; }
    g.setColour (swui::ink);
    g.setFont (swui::mono (12.0f));
    g.drawText (styleHelp[first], 96, 636, 780, 16, Justification::left);
    g.setFont (swui::sansItalic (12.0f));
    g.drawText ("single notes get the whole scale. chords and loops play it safe with octaves and fifths", 96, 658, 780, 16, Justification::left);
    g.setFont (swui::mono (11.0f));
    g.drawText ("SOURCE", 96, 716, 70, 14, Justification::left);
    g.setFont (swui::sansItalic (12.0f));
    g.drawText (String (source.buttons[source.value]->getButtonText()) + " - " + srcHelp[source.value], 164, 750, 700, 16, Justification::left);
}

void SecretWeaponEditor::resized()
{
    mixBtn.setBounds (716, 30, 88, 44);
    onBtn.button.setBounds (810, 30, 54, 44);
    const int kw = 54;
    for (int i = 0; i < 12; ++i) key.buttons[i]->setBounds (164 + i * (kw + 4), 128, kw, 32);
    for (int i = 0; i < 10; ++i) scale.buttons[i]->setBounds (164 + (i % 5) * 141, 168 + (i / 5) * 36, 137, 32);
    listenBtn.setBounds (164, 244, 148, 30);
    roll.setBounds (96, 292, 768, 154);
    Knob* ks[4] = { &density, &level, &range, &length };
    for (int i = 0; i < 4; ++i) ks[i]->slider.setBounds (96 + i * 118, 484, 108, 128);
    for (int i = 0; i < 6; ++i) styles[i].button.setBounds (568 + (i % 2) * 150, 504 + (i / 2) * 44, 146, 40);
    const int sw[6] = { 100, 100, 100, 100, 100, 100 };
    int x = 164; (void) sw;
    for (int i = 0; i < 6; ++i) { const int w = (i == 0 ? 100 : 110); source.buttons[i]->setBounds (x, 706, w, 34); x += w + 4; }
    source.buttons[0]->setBounds (164, 706, 112, 34);
    for (int i = 1; i < 6; ++i) source.buttons[i]->setBounds (164 + i * 116, 706, 112, 34);
}
