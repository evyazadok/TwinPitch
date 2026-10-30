#include "PluginEditor.h"
#if GS_HAS_FONT
 #include "BinaryData.h"
#endif

using namespace juce;

//==============================================================================
Font gsui::mono (float size, bool bold)
{
   #if GS_HAS_FONT
    static auto reg = Typeface::createSystemTypefaceFor (BinaryData::SpaceMonoRegular_ttf, BinaryData::SpaceMonoRegular_ttfSize);
    static auto bld = Typeface::createSystemTypefaceFor (BinaryData::SpaceMonoBold_ttf, BinaryData::SpaceMonoBold_ttfSize);
    return Font (FontOptions (bold ? bld : reg).withHeight (size));
   #else
    return Font (FontOptions ("Menlo", size, bold ? Font::bold : Font::plain));
   #endif
}

Font gsui::sansItalic (float size)
{
   #if GS_HAS_FONT
    static auto it = Typeface::createSystemTypefaceFor (BinaryData::SpaceMonoItalic_ttf, BinaryData::SpaceMonoItalic_ttfSize);
    return Font (FontOptions (it).withHeight (size));
   #else
    return Font (FontOptions ("Avenir Next", size, Font::italic));
   #endif
}

//==============================================================================
GSLookAndFeel::GSLookAndFeel()
{
    setColour (PopupMenu::backgroundColourId, Colour (0xff2c1a8f));
    setColour (PopupMenu::textColourId, gsui::white);
    setColour (PopupMenu::headerTextColourId, Colour (0xffb9adff));
    setColour (PopupMenu::highlightedBackgroundColourId, gsui::white);
    setColour (PopupMenu::highlightedTextColourId, gsui::ground);
}

void GSLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider& s)
{
    const bool big = s.getProperties().getWithDefault ("big", false);
    const bool on = s.isEnabled();
    const float labelH = big ? 24.0f : 20.0f;
    auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    auto labelArea = area.removeFromBottom (labelH);
    const float size = jmin (area.getWidth(), area.getHeight());
    auto ring = area.withSizeKeepingCentre (size, size);
    const auto c = ring.getCentre();
    const int N = big ? 27 : 19;
    const float r = size * 0.5f - (big ? 10.0f : 7.0f);
    const float litS = big ? 8.0f : 6.0f, offS = big ? 4.0f : 3.0f;

    for (int i = 0; i < N; ++i)
    {
        const float t = (float) i / (float) (N - 1);
        const float a = degreesToRadians (-225.0f + 270.0f * t);
        const bool lit = t <= pos + 1.0e-4f;
        const float sz = lit ? litS : offS;
        g.setColour (lit ? (on ? gsui::white : gsui::faint) : gsui::white.withAlpha (on ? 0.35f : 0.18f));
        g.fillRect (c.x + r * std::cos (a) - sz * 0.5f, c.y + r * std::sin (a) - sz * 0.5f, sz, sz);
    }

    g.setColour (on ? gsui::white : gsui::faint);
    g.setFont (gsui::mono (big ? 19.0f : 13.0f, true));
    const String valueText = s.getProperties().contains ("valueOverride") ? s.getProperties()["valueOverride"].toString()
                                                                         : s.getTextFromValue (s.getValue());
    g.drawFittedText (valueText, ring.reduced (size * 0.2f).toNearestInt(), Justification::centred, 2);

    g.setColour (gsui::white);
    g.setFont (gsui::mono (big ? 13.0f : 11.0f));
    g.drawText (s.getProperties()["label"].toString(), labelArea, Justification::centred);
}

void GSLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool solid = b.getProperties().getWithDefault ("solid", false);
    const bool filled = solid || b.getToggleState() || (down && b.getProperties().getWithDefault ("momentary", false));
    if (filled)
    {
        g.setColour (solid && down ? Colour (0xffd9d0ff) : gsui::white);
        g.fillRect (r);
    }
    else
    {
        if (over) { g.setColour (gsui::white.withAlpha (0.12f)); g.fillRect (r); }
        g.setColour (gsui::white.withAlpha (b.isEnabled() ? 0.75f : 0.3f));
        g.drawRect (r, 1.0f);
    }
}

void GSLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool down)
{
    const bool solid = b.getProperties().getWithDefault ("solid", false);
    const bool filled = solid || b.getToggleState() || (down && b.getProperties().getWithDefault ("momentary", false));
    g.setColour (filled ? gsui::ground : gsui::white.withAlpha (b.isEnabled() ? 1.0f : 0.4f));
    const float fs = (float) b.getProperties().getWithDefault ("fontSize", 12.0);
    g.setFont (gsui::mono (fs, true));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), Justification::centred, 1);
}

//==============================================================================
// one short spoken phrase (four syllables), used when nothing is playing
static float demoAmp (float u)
{
    static const float syl[4][3] = { { 0.04f, 0.16f, 0.95f }, { 0.20f, 0.30f, 0.6f }, { 0.34f, 0.58f, 1.0f }, { 0.62f, 0.74f, 0.7f } };
    float a = 0.0f;
    for (auto& s : syl)
        if (u >= s[0] && u <= s[1]) a = jmax (a, s[2] * std::pow (std::sin (MathConstants<float>::pi * (u - s[0]) / (s[1] - s[0])), 0.6f));
    return a;
}

void SlapView::timerCallback()
{
    auto& v = proc.engine.viz;
    const int wi = v.writeIndex.load (std::memory_order_acquire);
    if (wi != lastWrite)
    {
        // copy the most recent kCols entries, oldest first
        bool any = false;
        for (int c = 0; c < kCols; ++c)
        {
            const int idx = (wi - kCols + c + goatslap::VizBuffer::kSize * 4) % goatslap::VizBuffer::kSize;
            dry[c] = v.dry[(size_t) idx].load (std::memory_order_relaxed);
            wet[c] = v.wet[(size_t) idx].load (std::memory_order_relaxed);
            ev[c] = v.event[(size_t) idx].load (std::memory_order_relaxed);
            any |= dry[c] > 0.003f || wet[c] > 0.003f;
        }
        silentFrames = any ? 0 : silentFrames + 1;
        lastWrite = wi;
    }
    repaint();
}

void SlapView::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float step = 8.0f, dot = 4.0f, top = 26.0f;
    const int rows = (int) ((b.getHeight() - 40.0f) / step);
    const float mid = (float) (rows - 1) * 0.5f;
    const float tipsyAmt = proc.getParam (IDs::tipsy) / 100.0f;
    const bool tape = (int) proc.getParam (IDs::machine) >= 3 && (int) proc.getParam (IDs::machine) != 6;

    // frame lines like a viewfinder
    g.setColour (gsui::line);
    g.fillRect (b.getX(), top - 7.0f, b.getWidth(), 1.0f);
    g.fillRect (b.getX(), top + rows * step, b.getWidth(), 1.0f);

    const bool demo = silentFrames > 60;   // nothing playing for ~2 s: show what it would do
    const float timeMs = (float) (proc.getParam (IDs::sync) > 0.5f
                             ? goatslap::getDivisions()[(int) proc.getParam (IDs::division)].beats * 60000.0 / proc.getHostBpm()
                             : proc.getParam (IDs::timeMs));
    const int offsetCols = jlimit (1, kCols - 1, (int) std::round (timeMs / 12.0f));
    const float mix = proc.getParam (IDs::aux) > 0.5f ? 1.0f : proc.getParam (IDs::mix) / 100.0f;
    Random rnd (1234);

    for (int c = 0; c < kCols; ++c)
    {
        float dA, wA; uint8_t e = 0;
        if (demo)
        {
            const int dryCols = kCols / 2;
            dA = c < dryCols ? demoAmp ((float) c / dryCols) : 0.0f;
            const int ec = c - offsetCols;
            wA = (ec >= 0 && ec < dryCols) ? demoAmp ((float) ec / dryCols) * (0.45f + 0.4f * mix) : 0.0f;
            if (tipsyAmt > 0 && wA > 0 && rnd.nextFloat() < tipsyAmt * 0.08f) e = 1;
        }
        else
        {
            auto lvl = [] (float a) { return jlimit (0.0f, 1.0f, (Decibels::gainToDecibels (a, -100.0f) + 48.0f) / 44.0f); };
            dA = lvl (dry[c]);
            wA = lvl (wet[c] * 3.0f);   // the shadow is drawn a bit louder than it is, so it reads
            e = ev[c];
        }
        // the voice stands on the centre line; its slap hangs under it like a shadow
        const float x = b.getX() + 8.0f + c * step;
        for (int r = 0; r < rows; ++r)
        {
            const float y = top + r * step;
            const bool upper = (float) r <= mid;
            const float dr = upper ? (mid - (float) r) / (mid + 0.5f) : ((float) r - mid) / (mid + 0.5f);
            if (upper)
            {
                if (dr <= dA && dA > 0.01f) { g.setColour (gsui::white); g.fillRect (x, y, dot, dot); }
            }
            else if (dr <= wA && wA > 0.01f)
            {
                const float jx = tipsyAmt > 0 ? (rnd.nextFloat() - 0.5f) * 6.0f * tipsyAmt : 0.0f;
                if (e != 0 && rnd.nextFloat() < 0.35f) g.setColour (rnd.nextBool() ? gsui::red : gsui::green);
                else g.setColour (gsui::white.withAlpha (0.3f + 0.45f * mix));
                const float s = tape ? dot + 1.5f : dot;
                g.fillRect (x + jx, y, s, s);
            }
        }
    }

    g.setColour (gsui::white);
    g.setFont (gsui::mono (10.0f));
    g.drawText ("DRY", (int) b.getX() + 10, 4, 60, 14, Justification::left);
    g.drawText ("SLAP +" + String ((int) std::round (timeMs)) + "ms", (int) b.getX() + 80, 4, 160, 14, Justification::left);
    g.setFont (gsui::sansItalic (12.0f));
    g.drawText (demo ? "play something" : (tipsyAmt > 0.5f ? "the shadow forgets itself" : "one voice, one shadow"),
                getLocalBounds().removeFromBottom (16).reduced (8, 0), Justification::right);
}

//==============================================================================
GoatSlapEditor::GoatSlapEditor (GoatSlapProcessor& p)
    : AudioProcessorEditor (&p), proc (p), view (p)
{
    setLookAndFeel (&lnf);

    setupKnob (timeDiv, IDs::division, "TIME", true);
    setupKnob (timeMs, IDs::timeMs, "TIME", true);
    setupKnob (mix, IDs::mix, "MIX", true);
    setupKnob (distance, IDs::distance, "DISTANCE", true);
    setupKnob (tone, IDs::tone, "TONE", true);

    setupKnob (room, IDs::room, "ROOM", false);
    setupKnob (width, IDs::width, "WIDTH", false);
    setupKnob (feedback, IDs::feedback, "FEEDBACK", false);
    setupKnob (duckAmt, IDs::duck, "DUCK AMT", false);
    setupKnob (feel, IDs::feel, "FEEL", false);
    setupKnob (age, IDs::age, "AGE", false);
    setupKnob (saturate, IDs::saturate, "SATURATE", false);
    setupKnob (output, IDs::output, "OUTPUT", false);
    setupKnob (tipsy, IDs::tipsy, "TIPSY", false);

    setupToggle (sync, IDs::sync, "SYNC");
    setupToggle (chRev, IDs::chReverse, "REV");
    setupToggle (chOct, IDs::chOctave, "OCT");
    setupToggle (chStut, IDs::chStutter, "STUT");
    setupToggle (chRoom, IDs::chRoom, "ROOM");
    setupToggle (chWob, IDs::chWobble, "WOB");
    setupChoice (heads, IDs::heads, { "1", "2", "3" });
    setupChoice (speed, IDs::speed, { "7.5", "15", "30" });

    StringArray machines; for (int i = 0; i < 8; ++i) machines.add (goatslap::getMachine (i).name);
    setupChoice (machine, IDs::machine, machines);
    setupToggle (duck, IDs::duckOn, "DUCK");
    setupToggle (dbl, IDs::doubler, "DOUBLE");
    setupToggle (aux, IDs::aux, "INSERT");

    throwButton.getProperties().set ("solid", true);
    throwButton.getProperties().set ("fontSize", 15.0);
    throwButton.setTooltip ("Hold for an end-of-phrase throw");
    throwButton.onStateChange = [this]
    {
        const bool d = throwButton.isDown();
        if (d != throwDown) { throwDown = d; proc.setParam (IDs::throwOn, d ? 1.0f : 0.0f); }
    };
    addAndMakeVisible (throwButton);

    rollButton.getProperties().set ("solid", true);
    rollButton.onClick = [this] { proc.roll(); };
    mutateButton.onClick = [this] { proc.mutate(); };
    addAndMakeVisible (rollButton);
    addAndMakeVisible (mutateButton);

    presetButton.getProperties().set ("fontSize", 13.0);
    presetButton.onClick = [this] { showPresetMenu(); };
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    moreButton.onClick = [this] { showMore = ! showMore; resized(); timerCallback(); };
    for (auto* b : { &presetButton, &prevButton, &nextButton, &moreButton }) addAndMakeVisible (*b);

    addAndMakeVisible (view);
    setSize (960, 780);
    timerCallback();
    startTimerHz (15);
}

GoatSlapEditor::~GoatSlapEditor() { setLookAndFeel (nullptr); }

void GoatSlapEditor::setupKnob (Knob& k, const char* id, const String& label, bool big)
{
    k.label = label;
    k.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    k.slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    k.slider.getProperties().set ("big", big);
    k.slider.getProperties().set ("label", label);
    k.slider.setMouseDragSensitivity (big ? 260 : 200);
    k.att = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
    if (auto* prm = proc.apvts.getParameter (id))
        k.slider.setDoubleClickReturnValue (true, (double) dynamic_cast<RangedAudioParameter*> (prm)->convertFrom0to1 (prm->getDefaultValue()));
    addAndMakeVisible (k.slider);
}

void GoatSlapEditor::setupChoice (Choice& c, const char* id, const StringArray& labels)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = c.buttons.add (new TextButton (labels[i]));
        b->getProperties().set ("fontSize", 12.0);
        b->onClick = [&c, i] { c.att->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    c.att = std::make_unique<ParameterAttachment> (*proc.apvts.getParameter (id), [&c] (float v)
    {
        c.value = (int) std::round (v);
        for (int i = 0; i < c.buttons.size(); ++i) c.buttons[i]->setToggleState (i == c.value, dontSendNotification);
    });
    c.att->sendInitialUpdate();
}

void GoatSlapEditor::setupToggle (Toggle& t, const char* id, const String& text)
{
    t.button.setButtonText (text);
    t.button.setClickingTogglesState (true);
    t.att = std::make_unique<ButtonAtt> (proc.apvts, id, t.button);
    addAndMakeVisible (t.button);
}

void GoatSlapEditor::timerCallback()
{
    const bool s = proc.getParam (IDs::sync) > 0.5f;
    const bool isAux = proc.getParam (IDs::aux) > 0.5f;
    const int m = (int) proc.getParam (IDs::machine);
    const bool tapeMachine = goatslap::getMachine (m).isTape;

    timeDiv.slider.setVisible (! showMore && s);
    timeMs.slider.setVisible (! showMore && ! s);
    for (auto* k : { &mix, &distance, &tone }) k->slider.setVisible (! showMore);
    for (auto* k : { &room, &width, &feedback, &duckAmt, &feel, &age, &saturate, &output }) k->slider.setVisible (showMore);
    for (auto* t : { &sync, &chRev, &chOct, &chStut, &chRoom, &chWob }) t->button.setVisible (showMore);
    for (auto* b : heads.buttons) b->setVisible (showMore);
    for (auto* b : speed.buttons) { b->setVisible (showMore); b->setEnabled (tapeMachine); }
    age.slider.setEnabled (m != 0);
    saturate.slider.setEnabled (m != 0);
    duckAmt.slider.setEnabled (proc.getParam (IDs::duckOn) > 0.5f);

    mix.slider.setEnabled (! isAux);
    if (isAux) mix.slider.getProperties().set ("valueOverride", "100%"); else mix.slider.getProperties().remove ("valueOverride");
    mix.slider.repaint();
    aux.button.setButtonText (isAux ? "AUX 100% WET" : "INSERT");
    moreButton.setButtonText (showMore ? "BACK" : "MORE");

    const auto& ps = getPresets();
    const int cur = jlimit (0, (int) ps.size() - 1, proc.currentPreset);
    const String cat = ps[(size_t) cur].category;
    presetButton.setButtonText (String (ps[(size_t) cur].name) + "  " + (cat == "TIPSY" ? String ("~") : cat == "TAPE" ? String ("T") : String ("*")));

    if (const int rolled = proc.lastRoll.exchange (0)) { die = rolled; repaint (dieBox); }
    if (showMore) repaint (knobArea);
}

void GoatSlapEditor::stepPreset (int delta)
{
    const int n = (int) getPresets().size();
    proc.setCurrentProgram ((proc.currentPreset + delta + n) % n);
    timerCallback();
}

void GoatSlapEditor::showPresetMenu()
{
    PopupMenu menu;
    const auto& ps = getPresets();
    for (const char* cat : { "READY", "TAPE", "TIPSY" })
    {
        menu.addSectionHeader (String (cat) == "TIPSY" ? "FEELING TIPSY" : String (cat) == "READY" ? "READY  -  put it on, it's fire" : "TAPE");
        for (int i = 0; i < (int) ps.size(); ++i)
            if (String (ps[(size_t) i].category) == cat)
                menu.addItem (i + 1, ps[(size_t) i].name, true, i == proc.currentPreset);
    }
    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&presetButton).withMinimumWidth (presetButton.getWidth()),
                        [this] (int r) { if (r > 0) { proc.setCurrentProgram (r - 1); timerCallback(); } });
}

//==============================================================================
static const char* kFont[7][7] = {
    // G      O        A        T        S        L        P
    { "01110", "01110", "01110", "11111", "01111", "10000", "11110" },
    { "10001", "10001", "10001", "00100", "10000", "10000", "10001" },
    { "10000", "10001", "10001", "00100", "10000", "10000", "10001" },
    { "10111", "10001", "11111", "00100", "01110", "10000", "11110" },
    { "10001", "10001", "10001", "00100", "00001", "10000", "10000" },
    { "10001", "10001", "10001", "00100", "00001", "10000", "10000" },
    { "01111", "01110", "10001", "00100", "11110", "11111", "10000" },
};

void GoatSlapEditor::paint (Graphics& g)
{
    g.fillAll (gsui::ground);

    // grid lines
    g.setColour (gsui::line);
    for (int y : { 118, 430, 610 }) g.fillRect (0, y, getWidth(), 1);
    for (int x : { 72, 888 }) g.fillRect (x, 0, 1, getHeight());

    // specks
    const struct { int x, y; Colour c; } specks[] = {
        { 40, 60, gsui::red }, { 160, 104, gsui::green }, { 930, 170, gsui::red }, { 30, 380, gsui::green }, { 910, 560, gsui::white },
        { 300, 622, gsui::green }, { 700, 20, gsui::green }, { 44, 700, gsui::white }, { 930, 740, gsui::green }, { 520, 12, gsui::red } };
    for (auto& s : specks) { g.setColour (s.c); g.fillRect (s.x, s.y, 4, 4); }

    // dot-matrix logo: G O A T S L A P
    const int letters[8] = { 0, 1, 2, 3, 4, 5, 2, 6 };
    g.setColour (gsui::white);
    for (int li = 0; li < 8; ++li)
        for (int ry = 0; ry < 7; ++ry)
            for (int rx = 0; rx < 5; ++rx)
                if (kFont[ry][letters[li]][rx] == '1')
                    g.fillRect (96 + li * 42 + (li >= 4 ? 12 : 0) + rx * 6, 38 + ry * 6, 5, 5);

    // vertical caption
    {
        Graphics::ScopedSaveState ss (g);
        g.addTransform (AffineTransform::rotation (-MathConstants<float>::halfPi, 914.0f, 330.0f));
        g.setFont (gsui::sansItalic (13.0f));
        g.drawText ("slap it back, softly", 914, 330, 240, 18, Justification::left);
    }

    g.setFont (gsui::mono (11.0f));
    g.drawText ("MACHINE", 96, 632, 200, 16, Justification::left);

    if (showMore)
    {
        g.drawText ("TIME", 96, 276, 90, 14, Justification::left);
        g.drawText ("HEADS", 206, 276, 132, 14, Justification::left);
        g.drawText ("TAPE SPEED (IPS)", 357, 276, 168, 14, Justification::left);
        g.drawText ("WHAT CAN GO WRONG", 544, 276, 320, 14, Justification::left);
    }

    // feeling tipsy box
    g.drawRect (tipsyBox, 1);
    g.setFont (gsui::sansItalic (15.0f));
    g.drawText ("feeling tipsy", tipsyBox.getX() + 10, tipsyBox.getY() + 6, 200, 20, Justification::left);

    // die
    g.drawRect (dieBox, 1);
    static const int faces[7][6][2] = {
        {}, { { 1, 1 } }, { { 0, 0 }, { 2, 2 } }, { { 0, 0 }, { 1, 1 }, { 2, 2 } }, { { 0, 0 }, { 2, 0 }, { 0, 2 }, { 2, 2 } },
        { { 0, 0 }, { 2, 0 }, { 1, 1 }, { 0, 2 }, { 2, 2 } }, { { 0, 0 }, { 2, 0 }, { 0, 1 }, { 2, 1 }, { 0, 2 }, { 2, 2 } } };
    const int counts[7] = { 0, 1, 2, 3, 4, 5, 6 };
    for (int i = 0; i < counts[die]; ++i)
        g.fillRect (dieBox.getX() + 6 + faces[die][i][0] * 12, dieBox.getY() + 6 + faces[die][i][1] * 12, 6, 6);
}

void GoatSlapEditor::resized()
{
    // top bar
    prevButton.setBounds (470, 36, 36, 42);
    presetButton.setBounds (512, 36, 180, 42);
    nextButton.setBounds (698, 36, 36, 42);
    aux.button.setBounds (744, 36, 120, 42);

    knobArea = { 96, 130, 768, 290 };
    // main knobs
    const int kw = 768 / 4;
    Knob* bigs[] = { &timeDiv, &mix, &distance, &tone };
    for (int i = 0; i < 4; ++i) bigs[i]->slider.setBounds (96 + i * kw, 146, kw, 200);
    timeMs.slider.setBounds (timeDiv.slider.getBounds());
    moreButton.setBounds (784, 384, 80, 30);

    // MORE view
    Knob* smalls[] = { &room, &width, &feedback, &duckAmt, &feel, &age, &saturate, &output };
    for (int i = 0; i < 8; ++i) smalls[i]->slider.setBounds (96 + i * 96, 136, 96, 132);
    sync.button.setBounds (96, 292, 90, 36);
    for (int i = 0; i < 3; ++i) heads.buttons[i]->setBounds (206 + i * 44, 292, 40, 36);
    for (int i = 0; i < 3; ++i) speed.buttons[i]->setBounds (357 + i * 56, 292, 52, 36);
    Toggle* ch[] = { &chRev, &chOct, &chStut, &chRoom, &chWob };
    for (int i = 0; i < 5; ++i) ch[i]->button.setBounds (544 + i * 64, 292, 60, 36);

    view.setBounds (96, 440, 768, 160);

    // bottom
    for (int i = 0; i < 8; ++i) machine.buttons[i]->setBounds (96 + (i % 4) * 96, 652 + (i / 4) * 46, 92, 42);
    duck.button.setBounds (488, 652, 68, 42);
    dbl.button.setBounds (560, 652, 68, 42);
    throwButton.setBounds (488, 698, 140, 42);

    tipsyBox = { 640, 632, 224, 112 };
    tipsy.slider.setBounds (644, 652, 80, 92);
    dieBox = { 734, 662, 42, 42 };
    rollButton.setBounds (784, 662, 72, 42);
    mutateButton.setBounds (734, 710, 122, 28);
}
