#include "PluginEditor.h"
#if HO_HAS_FONT
 #include "BinaryData.h"
#endif

using namespace juce;

//==============================================================================
Font houi::mono (float size, bool bold)
{
   #if HO_HAS_FONT
    static auto reg = Typeface::createSystemTypefaceFor (BinaryData::SpaceMonoRegular_ttf, BinaryData::SpaceMonoRegular_ttfSize);
    static auto bld = Typeface::createSystemTypefaceFor (BinaryData::SpaceMonoBold_ttf, BinaryData::SpaceMonoBold_ttfSize);
    return Font (FontOptions (bold ? bld : reg).withHeight (size));
   #else
    return Font (FontOptions ("Menlo", size, bold ? Font::bold : Font::plain));
   #endif
}

Font houi::sansItalic (float size)
{
   #if HO_HAS_FONT
    static auto it = Typeface::createSystemTypefaceFor (BinaryData::SpaceMonoItalic_ttf, BinaryData::SpaceMonoItalic_ttfSize);
    return Font (FontOptions (it).withHeight (size));
   #else
    return Font (FontOptions ("Menlo", size, Font::italic));
   #endif
}

//==============================================================================
HOLookAndFeel::HOLookAndFeel()
{
    setColour (PopupMenu::backgroundColourId, Colour (0xff4a0f22));
    setColour (PopupMenu::textColourId, houi::white);
    setColour (PopupMenu::headerTextColourId, Colour (0xffe9a7b8));
    setColour (PopupMenu::highlightedBackgroundColourId, houi::white);
    setColour (PopupMenu::highlightedTextColourId, houi::ground);
}

void HOLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider& s)
{
    const bool big = s.getProperties().getWithDefault ("big", false);
    const bool noLabel = s.getProperties().getWithDefault ("nolabel", false);
    const bool on = s.isEnabled();
    auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    Rectangle<float> labelArea;
    if (! noLabel) labelArea = area.removeFromBottom (24.0f);
    const float size = jmin (area.getWidth(), area.getHeight());
    auto ring = area.withSizeKeepingCentre (size, size);
    const auto c = ring.getCentre();
    const int N = 21;
    const float r = size * 0.5f - 8.0f;
    const float litS = 6.0f, offS = 3.0f;
    for (int i = 0; i < N; ++i)
    {
        const float t = (float) i / (float) (N - 1);
        const float a = degreesToRadians (-225.0f + 270.0f * t);
        const bool lit = t <= pos + 1.0e-4f && pos > 0.001f;
        const float sz = lit ? litS : offS;
        g.setColour (lit ? (on ? houi::white : houi::faint) : houi::white.withAlpha (on ? 0.35f : 0.18f));
        g.fillRect (c.x + r * std::cos (a) - sz * 0.5f, c.y + r * std::sin (a) - sz * 0.5f, sz, sz);
    }
    g.setColour (on ? houi::white : houi::faint);
    g.setFont (houi::mono (big ? 19.0f : 15.0f, true));
    const String valueText = on ? s.getTextFromValue (s.getValue()) : String ("OFF");
    g.drawFittedText (valueText, ring.reduced (size * 0.2f).toNearestInt(), Justification::centred, 1);
    if (! noLabel)
    {
        g.setColour (houi::white);
        g.setFont (houi::mono (12.0f));
        g.drawText (s.getProperties()["label"].toString(), labelArea, Justification::centred);
    }
}

void HOLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float, float, float, Slider::SliderStyle, Slider& s)
{
    const float pos = (float) s.valueToProportionOfLength (s.getValue());
    const bool dots = String (s.getProperties()["style"].toString()) == "dots";
    if (dots)
    {
        const int n = 24;
        const float step = (float) w / n;
        for (int i = 0; i < n; ++i)
        {
            const bool lit = (float) (i + 1) / n <= pos + 0.001f && pos > 0.001f;
            g.setColour (houi::white.withAlpha (lit ? 1.0f : 0.35f));
            const float sz = lit ? 6.0f : 3.0f;
            g.fillRect ((float) x + i * step + (step - sz) * 0.5f, (float) y + h * 0.5f - sz * 0.5f, sz, sz);
        }
    }
    else
    {
        const int n = 30;
        const float step = (float) w / n;
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (n - 1);
            const bool lit = (float) (i + 1) / n <= pos + 0.001f && pos > 0.001f;
            const float bh = 8.0f + (float) (h - 8) * t;
            g.setColour (lit ? houi::white : houi::white.withAlpha (0.38f));
            g.fillRect ((float) x + i * step, (float) (y + h) - bh, step - 3.0f, bh);
        }
    }
}

void HOLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool solid = b.getProperties().getWithDefault ("solid", false);
    const bool filled = solid || b.getToggleState();
    if (filled)
    {
        g.setColour (solid && down ? Colour (0xfff2cfd9) : houi::white);
        g.fillRect (r);
    }
    else
    {
        if (over) { g.setColour (houi::white.withAlpha (0.12f)); g.fillRect (r); }
        g.setColour (houi::white.withAlpha (b.isEnabled() ? 0.75f : 0.3f));
        g.drawRect (r, 1.0f);
    }
}

void HOLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    const bool solid = b.getProperties().getWithDefault ("solid", false);
    const bool filled = solid || b.getToggleState();
    g.setColour (filled ? houi::ground : houi::white.withAlpha (b.isEnabled() ? 1.0f : 0.4f));
    const float fs = (float) b.getProperties().getWithDefault ("fontSize", 12.0);
    g.setFont (houi::mono (fs, true));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 0), Justification::centred, 1);
}

//==============================================================================
static String dotSep() { return String (CharPointer_UTF8 (" \xc2\xb7 ")); }

static float hash01 (int a, int b)
{
    uint32_t h = (uint32_t) a * 374761393u + (uint32_t) b * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float) ((h ^ (h >> 16)) & 0xFFFF) / 65535.0f;
}

void WearView::timerCallback()
{
    auto& v = proc.engine.viz;
    const int wi = v.writeIndex.load (std::memory_order_acquire);
    if (wi != lastWrite)
    {
        float m = 0.0f;
        for (int k = 1; k <= 6; ++k) m = jmax (m, v.level[(size_t) ((wi - k + hangover::VizBuffer::kSize * 2) % hangover::VizBuffer::kSize)].load (std::memory_order_relaxed));
        level += 0.5f * (m - level);
        silentFrames = m > 0.003f ? 0 : silentFrames + 1;
        lastWrite = wi;
    }
    else ++silentFrames;
    phase += 0.045f; ++frame;
    repaint();
}

void WearView::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float step = 8.0f, dot = 5.0f;
    const int cols = (int) (b.getWidth() / step), rows = 11;
    const float top = 22.0f, mid = (rows - 1) * 0.5f;
    const bool demo = silentFrames > 40;
    const float amp = demo ? 0.85f : 0.5f + 0.45f * jlimit (0.0f, 1.0f, std::sqrt (level) * 2.2f);
    const auto& M = hangover::getMode ((int) proc.getParam (IDs::mode));
    auto lvl = [this] (const char* on, const char* id, float scale, float modeScale)
    { return proc.getParam (on) > 0.5f ? jmin (1.0f, proc.getParam (id) / 100.0f * scale * modeScale) : 0.0f; };
    const float mag = proc.getParam (IDs::magnitude) / 100.0f * proc.fadeNow.load();
    const float wob = lvl (IDs::wobbleOn, IDs::wobble, mag, M.wob), trm = lvl (IDs::tremOn, IDs::trem, mag, M.trem),
                dig = lvl (IDs::digOn, IDs::digital, mag, M.dig), dst = lvl (IDs::distOn, IDs::distort, mag, M.dist),
                drp = lvl (IDs::dropsOn, IDs::drops, mag, M.drops);
    const float tipsyAmt = proc.getParam (IDs::tipsy) / 100.0f;
    const float pulseNow = proc.engine.eventPulse.load();

    g.setColour (houi::white);
    g.setFont (houi::mono (11.0f));
    g.drawText ("CLEAN", 0, 0, 80, 14, Justification::left);
    g.drawText ("WORN", getWidth() - 80, 0, 80, 14, Justification::right);

    int prevRow = -1;
    for (int c = 0; c < cols; ++c)
    {
        const float u = (float) c / (float) (cols - 1);
        const float wear = u * u * (3.0f - 2.0f * u);         // smoothstep: clean on the left, worn on the right
        float ph = phase + u * 15.0f;
        ph += wear * wob * 2.6f * (hash01 (c / 4, frame / 12) - 0.5f);           // pitch wobble
        float y = std::sin (ph) * amp;
        y *= 1.0f - wear * trm * 0.55f * (0.5f + 0.5f * std::sin (u * 26.0f - phase * 2.0f));   // tremolo
        y *= 1.0f + wear * dst * 1.4f;                                            // distortion: pushes into the ceiling
        y = jlimit (-1.0f, 1.0f, y);
        const float q = 1.0f + wear * dig * 4.0f;                                 // digital: coarser steps
        float rowF = mid - y * mid;
        float row = std::round (rowF / q) * q;
        if (dst > 0.0f && std::fabs (y) >= 0.999f) row = y > 0 ? 0.0f : (float) (rows - 1);
        const int r = jlimit (0, rows - 1, (int) std::round (row));
        const bool gap = wear * drp > 0.02f && hash01 (c / 3, frame / 25 + 7) < wear * drp * 0.55f;
        if (! gap)
        {
            const bool spark = (wear > 0.5f) && tipsyAmt > 0.01f && hash01 (c, frame / 6) < tipsyAmt * 0.04f * (1.0f + pulseNow * 3.0f);
            g.setColour (spark ? (hash01 (c, frame) > 0.5f ? houi::mint : houi::yellow) : houi::white);
            g.fillRect (c * step, top + r * step, dot, dot);
            // fill steps between neighbours so the line reads as a line
            if (prevRow >= 0 && std::abs (r - prevRow) > 1 && dig * wear < 0.15f)
            {
                const int a = jmin (r, prevRow) + 1, e = jmax (r, prevRow) - 1;
                g.setColour (houi::white.withAlpha (0.5f));
                for (int k = a; k <= e; ++k) g.fillRect (c * step, top + k * step, dot * 0.6f, dot * 0.6f);
            }
        }
        prevRow = gap ? -1 : r;
    }
    g.setFont (houi::sansItalic (12.0f));
    g.setColour (houi::white);
    g.drawText (demo ? "play something" : "same wave, a few months later", getLocalBounds().removeFromBottom (13).reduced (8, 0), Justification::centred);
}

//==============================================================================
HangoverEditor::HangoverEditor (HangoverProcessor& p)
    : AudioProcessorEditor (&p), proc (p), view (p)
{
    setLookAndFeel (&lnf);

    const struct { const char* title; const char* lvl; const char* on; const char* flux; } defs[6] = {
        { "WOBBLE", IDs::wobble, IDs::wobbleOn, IDs::wobbleFlux }, { "TREMOLO", IDs::trem, IDs::tremOn, nullptr },
        { "DIGITAL", IDs::digital, IDs::digOn, nullptr },          { "DISTORT", IDs::distort, IDs::distOn, nullptr },
        { "SPACE", IDs::space, IDs::spaceOn, nullptr },            { "DROPS", IDs::drops, IDs::dropsOn, IDs::dropsFlux } };
    for (int i = 0; i < 6; ++i)
    {
        auto& m = mods[i];
        m.title = defs[i].title; m.levelId = defs[i].lvl; m.onId = defs[i].on; m.fluxId = defs[i].flux;
        setupKnob (m.knob, defs[i].lvl, defs[i].title, false, true);
        setupToggle (m.on, defs[i].on, "ON");
        m.on.button.getProperties().set ("fontSize", 13.0);
        if (defs[i].flux) { setupToggle (m.flux, defs[i].flux, "FLUX"); m.flux.button.getProperties().set ("fontSize", 10.0); }
    }

    // mode
    const StringArray modeNames { "GUITAR", "KEYS", "PAD", "LOOP" };
    for (int i = 0; i < 4; ++i)
    {
        auto* b = modeButtons.add (new TextButton (modeNames[i]));
        b->getProperties().set ("fontSize", 14.0);
        b->onClick = [this, i] { modeAtt->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    modeAtt = std::make_unique<ParameterAttachment> (*proc.apvts.getParameter (IDs::mode), [this] (float v)
    {
        modeValue = (int) std::round (v);
        for (int i = 0; i < modeButtons.size(); ++i) modeButtons[i]->setToggleState (i == modeValue, dontSendNotification);
        repaint();
    });
    modeAtt->sendInitialUpdate();

    tremDivBtn.getProperties().set ("fontSize", 12.0);
    tremDivBtn.onClick = [this] { cycleParam (IDs::tremDiv, kNumTremDivs); };
    tremPanBtn.setClickingTogglesState (false);
    tremPanBtn.getProperties().set ("fontSize", 12.0);
    tremPanBtn.onClick = [this] { proc.setParam (IDs::tremPan, proc.getParam (IDs::tremPan) > 0.5f ? 0.0f : 1.0f); timerCallback(); };
    barsBtn.getProperties().set ("fontSize", 13.0);
    barsBtn.onClick = [this] { cycleParam (IDs::fadeBars, 3); };
    addAndMakeVisible (tremDivBtn); addAndMakeVisible (tremPanBtn); addAndMakeVisible (barsBtn);

    setupKnob (mix, IDs::mix, "MIX", true, false);

    magnitude.setSliderStyle (Slider::LinearHorizontal);
    magnitude.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    magAtt = std::make_unique<SliderAtt> (proc.apvts, IDs::magnitude, magnitude);
    magnitude.setDoubleClickReturnValue (true, 100.0);
    tipsy.setSliderStyle (Slider::LinearHorizontal);
    tipsy.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    tipsy.getProperties().set ("style", "dots");
    tipsyAtt = std::make_unique<SliderAtt> (proc.apvts, IDs::tipsy, tipsy);
    tipsy.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (magnitude); addAndMakeVisible (tipsy);

    setupToggle (fadeIn, IDs::fadeIn, "FADE IN");
    setupToggle (fadeOut, IDs::fadeOut, "FADE OUT");
    fadeIn.button.getProperties().set ("fontSize", 13.0);
    fadeOut.button.getProperties().set ("fontSize", 13.0);

    rollButton.getProperties().set ("solid", true);
    rollButton.getProperties().set ("fontSize", 16.0);
    rollButton.onClick = [this] { proc.roll(); };
    addAndMakeVisible (rollButton);

    presetButton.getProperties().set ("fontSize", 14.0);
    presetButton.onClick = [this] { showPresetMenu(); };
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    for (auto* b : { &presetButton, &prevButton, &nextButton }) addAndMakeVisible (*b);

    addAndMakeVisible (view);
    setSize (960, 780);
    timerCallback();
    startTimerHz (15);
}

HangoverEditor::~HangoverEditor() { setLookAndFeel (nullptr); }

void HangoverEditor::setupKnob (Knob& k, const char* id, const String& label, bool big, bool inside)
{
    k.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    k.slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    k.slider.getProperties().set ("big", big);
    k.slider.getProperties().set ("nolabel", inside);
    k.slider.getProperties().set ("label", label);
    k.slider.setMouseDragSensitivity (220);
    k.att = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
    if (auto* prm = proc.apvts.getParameter (id))
        k.slider.setDoubleClickReturnValue (true, (double) dynamic_cast<RangedAudioParameter*> (prm)->convertFrom0to1 (prm->getDefaultValue()));
    addAndMakeVisible (k.slider);
}

void HangoverEditor::setupToggle (Toggle& t, const char* id, const String& text)
{
    t.button.setButtonText (text);
    t.button.setClickingTogglesState (true);
    t.att = std::make_unique<ButtonAtt> (proc.apvts, id, t.button);
    addAndMakeVisible (t.button);
}

void HangoverEditor::cycleParam (const char* id, int count)
{
    const int v = ((int) std::round (proc.getParam (id)) + 1) % count;
    proc.setParam (id, (float) v);
    timerCallback();
}

void HangoverEditor::timerCallback()
{
    for (auto& m : mods)
    {
        const bool on = proc.getParam (m.onId) > 0.5f;
        m.on.button.setButtonText (on ? "ON" : "OFF");
        m.knob.slider.setEnabled (on);
    }
    tremDivBtn.setButtonText (getTremDivs()[(int) proc.getParam (IDs::tremDiv)].name);
    tremPanBtn.setButtonText (proc.getParam (IDs::tremPan) > 0.5f ? "PAN" : "MONO");
    const bool tOn = proc.getParam (IDs::tremOn) > 0.5f;
    tremDivBtn.setEnabled (tOn); tremPanBtn.setEnabled (tOn);
    const int bars = 2 << (int) proc.getParam (IDs::fadeBars);
    barsBtn.setButtonText (String (bars) + " BARS");

    const auto& ps = getPresets();
    const int cur = jlimit (0, (int) ps.size() - 1, proc.currentPreset);
    const String cat = ps[(size_t) cur].category;
    presetButton.setButtonText (String (ps[(size_t) cur].name) + (cat == "TIPSY" ? "  ~" : ""));

    if (const int rolled = proc.lastRoll.exchange (0)) { die = rolled; }
    const int ev = proc.engine.lastEvent.load();
    eventText = ev > 0 ? String (hangover::chaosName (ev)) : String ("nothing yet");
    pulse = proc.engine.eventPulse.load();
    proc.engine.eventPulse.store (pulse * 0.6f);
    repaint();
}

void HangoverEditor::stepPreset (int delta)
{
    const int n = (int) getPresets().size();
    proc.setCurrentProgram ((proc.currentPreset + delta + n) % n);
    timerCallback();
}

void HangoverEditor::showPresetMenu()
{
    PopupMenu menu;
    const auto& ps = getPresets();
    for (const char* cat : { "GUITAR", "KEYS", "PAD", "LOOP", "TIPSY" })
    {
        menu.addSectionHeader (String (cat) == "TIPSY" ? "FEELING TIPSY" : String (cat));
        for (int i = 0; i < (int) ps.size(); ++i)
            if (String (ps[(size_t) i].category) == cat)
                menu.addItem (i + 1, ps[(size_t) i].name, true, i == proc.currentPreset);
    }
    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&presetButton).withMinimumWidth (presetButton.getWidth()),
                        [this] (int r) { if (r > 0) { proc.setCurrentProgram (r - 1); timerCallback(); } });
}

//==============================================================================
static const char* kLogo[8][7] = {
    { "10001", "10001", "10001", "11111", "10001", "10001", "10001" },   // H
    { "01110", "10001", "10001", "11111", "10001", "10001", "10001" },   // A
    { "10001", "11001", "10101", "10011", "10001", "10001", "10001" },   // N
    { "01111", "10000", "10000", "10111", "10001", "10001", "01110" },   // G
    { "01110", "10001", "10001", "10001", "10001", "10001", "01110" },   // O
    { "10001", "10001", "10001", "10001", "10001", "01010", "00100" },   // V
    { "11111", "10000", "10000", "11110", "10000", "10000", "11111" },   // E
    { "11110", "10001", "10001", "11110", "10100", "10010", "10001" } }; // R

void HangoverEditor::paint (Graphics& g)
{
    g.fillAll (houi::ground);
    g.setColour (houi::line);
    for (int y : { 112, 476, 610 }) g.fillRect (0, y, getWidth(), 1);
    for (int x : { 72, 888 }) g.fillRect (x, 0, 1, getHeight());

    const struct { int x, y; Colour c; } specks[] = {
        { 40, 60, houi::yellow }, { 160, 100, houi::mint }, { 700, 20, houi::mint }, { 932, 172, houi::yellow }, { 30, 380, houi::mint },
        { 300, 618, houi::mint }, { 912, 562, houi::white }, { 44, 700, houi::white } };
    for (auto& s : specks) { g.setColour (s.c); g.fillRect (s.x, s.y, 4, 4); }

    g.setColour (houi::white);
    for (int li = 0; li < 8; ++li)
        for (int ry = 0; ry < 7; ++ry)
            for (int rx = 0; rx < 5; ++rx)
                if (kLogo[li][ry][rx] == '1') g.fillRect (96 + li * 42 + rx * 6, 34 + ry * 6, 5, 5);

    g.setFont (houi::mono (11.0f));
    g.drawFittedText ("SILENT\nWHEN\nSILENT", Rectangle<int> (800, 36, 68, 44), Justification::topRight, 3);
    {
        Graphics::ScopedSaveState ss (g);
        g.addTransform (AffineTransform::rotation (-MathConstants<float>::halfPi, 914.0f, 330.0f));
        g.setFont (houi::sansItalic (14.0f));
        g.drawText ("sounds better the morning after", 914, 330, 300, 18, Justification::left);
    }

    g.setFont (houi::mono (12.0f));
    g.drawText ("MODE", 96, 142, 60, 16, Justification::left);

    // module cards
    const auto& M = hangover::getMode (modeValue);
    for (int i = 0; i < 6; ++i)
    {
        auto& m = mods[i];
        const bool on = proc.getParam (m.onId) > 0.5f;
        const float a = on ? 1.0f : 0.45f;
        const auto c = m.card;
        g.setColour (houi::white.withAlpha (on ? 1.0f : 0.4f));
        g.drawRect (c.toFloat().reduced (0.5f), 1.0f);
        g.setColour (houi::white.withAlpha (a));
        g.setFont (houi::mono (15.0f, true));
        g.drawText (m.title, c.getX() + 112, c.getY() + 12, 130, 20, Justification::left);
        g.setFont (houi::mono (11.5f));
        const float lv = proc.getParam (m.levelId) / 100.0f;
        String l1, l2;
        switch (i)
        {
            case 0: l1 = "tape pitch drift"; l2 = String (M.wobRate, 1) + " Hz" + dotSep() + (proc.getParam (IDs::wobbleFlux) > 0.5f ? "flux" : "wide"); break;
            case 1: l2 = String ("sine") + dotSep() + (proc.getParam (IDs::tremPan) > 0.5f ? "stereo pan" : "mono"); break;
            case 2: { const float d = jmin (1.0f, lv * M.dig); l1 = String (16 - (int) std::round (6.5f * d)) + "-bit" + dotSep() + String ((int) std::round (44.1f * (1.0f - 0.75f * std::pow (d, 1.1f)))) + " kHz"; l2 = "old sampler"; break; }
            case 3: l1 = String ("tape") + dotSep() + "warm"; l2 = "soft knee"; break;
            case 4: l1 = "small room"; l2 = String (0.2f + 1.2f * M.spaceSize, 1) + " s"; break;
            case 5: l1 = "level dips"; l2 = "on the signal only"; break;
        }
        if (l1.isNotEmpty()) g.drawText (l1, c.getX() + 112, c.getY() + 40, 130, 16, Justification::left);
        g.drawText (l2, c.getX() + 112, c.getY() + 62, 134, 16, Justification::left);
    }

    // magnitude
    g.setColour (houi::white);
    g.setFont (houi::mono (12.0f));
    g.drawText ("MAGNITUDE", 96, 632, 160, 16, Justification::left);
    const float fade = proc.fadeNow.load();
    String mg = String ((int) std::round (proc.getParam (IDs::magnitude))) + "%";
    if (fade < 0.995f) mg += "  now " + String ((int) std::round (proc.getParam (IDs::magnitude) * fade)) + "%";
    g.drawText (mg, 256, 632, 260, 16, Justification::right);

    // feeling tipsy box
    g.drawRect (tipsyBox, 1);
    g.setFont (houi::sansItalic (15.0f));
    g.drawText ("feeling tipsy", tipsyBox.getX() + 10, tipsyBox.getY() + 6, 130, 20, Justification::left);
    g.setFont (houi::sansItalic (12.0f));
    g.setColour (pulse > 0.3f ? houi::yellow : houi::white);
    g.drawText (eventText, tipsyBox.getX() + 100, tipsyBox.getY() + 8, tipsyBox.getWidth() - 110, 16, Justification::right);
    g.setColour (houi::white);
    g.setFont (houi::mono (11.0f));
    g.drawText ("TIPSY " + String ((int) std::round (proc.getParam (IDs::tipsy))) + "%", tipsyBox.getX() + 12, tipsyBox.getBottom() - 22, 120, 14, Justification::left);

    // die
    g.drawRect (dieBox, 1);
    static const int faces[7][6][2] = {
        {}, { { 1, 1 } }, { { 0, 0 }, { 2, 2 } }, { { 0, 0 }, { 1, 1 }, { 2, 2 } }, { { 0, 0 }, { 2, 0 }, { 0, 2 }, { 2, 2 } },
        { { 0, 0 }, { 2, 0 }, { 1, 1 }, { 0, 2 }, { 2, 2 } }, { { 0, 0 }, { 2, 0 }, { 0, 1 }, { 2, 1 }, { 0, 2 }, { 2, 2 } } };
    for (int i = 0; i < die; ++i)
        g.fillRect (dieBox.getX() + 6 + faces[die][i][0] * 12, dieBox.getY() + 6 + faces[die][i][1] * 12, 6, 6);
}

void HangoverEditor::resized()
{
    prevButton.setBounds (470, 34, 36, 44);
    presetButton.setBounds (512, 34, 230, 44);
    nextButton.setBounds (748, 34, 36, 44);

    const int mw = 171;
    for (int i = 0; i < 4; ++i) modeButtons[i]->setBounds (162 + i * (mw + 6), 128, mw, 38);

    for (int i = 0; i < 6; ++i)
    {
        auto& m = mods[i];
        m.card = { 96 + (i % 3) * 260, 184 + (i / 3) * 144, 248, 132 };
        m.knob.slider.setBounds (m.card.getX() + 4, m.card.getY() + 14, 104, 104);
        m.on.button.setBounds (m.card.getX() + 112, m.card.getY() + 92, 56, 28);
        if (m.fluxId) m.flux.button.setBounds (m.card.getRight() - 54, m.card.getY() + 6, 48, 18);
    }
    // tremolo card: rate and pan buttons stand where the first text line would be
    tremDivBtn.setBounds (mods[1].card.getX() + 112, mods[1].card.getY() + 36, 54, 24);
    tremPanBtn.setBounds (mods[1].card.getX() + 172, mods[1].card.getY() + 36, 56, 24);

    view.setBounds (96, 480, 768, 126);

    magnitude.setBounds (96, 654, 420, 44);
    fadeIn.button.setBounds (96, 706, 135, 38);
    fadeOut.button.setBounds (238, 706, 135, 38);
    barsBtn.setBounds (380, 706, 136, 38);

    mix.slider.setBounds (542, 636, 100, 124);

    tipsyBox = { 652, 626, 212, 132 };
    dieBox = { 664, 660, 42, 42 };
    rollButton.setBounds (716, 660, 136, 42);
    tipsy.setBounds (662, 710, 192, 24);
}
