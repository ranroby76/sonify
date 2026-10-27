// C:\workspace\SONIFY PLUGIN natura\src\PluginEditor.cpp
#include "PluginEditor.h"

using namespace natura;

//==============================================================================
//  MODEL
//==============================================================================
ModelPage::ModelPage (NaturaProcessor& p) : Page (p)
{
    matrix.onPick = [this] (int a)
    {
        if (a != (int) proc.value (P::archetype)) proc.loadArchetypeRoot (a);
        matrix.setCurrent (a);
    };
    fModel = addFrame ("SOUND MODEL", { &matrix }, { 5.2f });
    fExciter = addFrame ("EXCITER",   { slider (P::exc_strength), slider (P::exc_hardness), slider (P::exc_noise), slider (P::exc_transient),
                                        slider (P::exc_sample), slider (P::exc_source) }, { 1, 1, 1, 1, 1, 1.25f });
    auto* uni = slider (P::res_unison);
    auto* det = slider (P::res_detune);
    unisonSliders[0] = uni; unisonSliders[1] = det;
    fRes  = addFrame ("RESONATOR", { slider (P::res_decay), slider (P::res_damping), slider (P::res_inharm), slider (P::res_oddeven), uni, det });
    fBody = addFrame ("BODY",      { slider (P::body_size), slider (P::body_material), slider (P::body_reso), slider (P::body_mix) });
    fTone = addFrame ("TONE LAYER",{ slider (P::tl_mix), slider (P::tl_source), slider (P::tl_color), slider (P::tl_formant),
                                     slider (P::tl_detune), slider (P::tl_sub) }, { 1, 1.25f, 1, 1, 1, 1 });

    for (int a = 0; a < kNumArchetypes - 1; ++a)
    {
        const auto& sl = specsFor ((Archetype) a);
        std::vector<juce::Component*> items;
        std::vector<float> units;
        for (int i = 0; i < sl.count; ++i)
        {
            items.push_back (slider (sl.ids[i]));
            units.push_back (info (sl.ids[i]).unit == Unit::Drawbar ? 0.8f : 1.0f);
        }
        fSpec.push_back (addFrame (archetypeName (a), items, units));
    }

    auto list = [this] (const std::vector<P>& ids)
    {
        std::vector<juce::Component*> v;
        for (auto id : ids) v.push_back (slider (id));
        return v;
    };
    fOsc1   = addFrame ("OSC 1",      list (synthOsc1()));
    fOsc2   = addFrame ("OSC 2",      list (synthOsc2()));
    fMix    = addFrame ("MIX",        list (synthMix()));
    fLadder = addFrame ("LADDER",     list (synthLadder()));
    fLadEnv = addFrame ("LADDER ENV", list (synthLadEnv()));
    fUnison = addFrame ("UNISON",     { uni, det });

    setArchetype ((int) proc.value (P::archetype));
}

void ModelPage::setArchetype (int a)
{
    arch = juce::jlimit (0, kNumArchetypes - 1, a);
    const bool synth = arch == (int) Archetype::Synth;
    for (int i = 0; i < (int) fSpec.size(); ++i) setFrameVisible (fSpec[(size_t) i], false);
    matrix.setCurrent (arch);
    for (int f : { fExciter, fRes, fBody, fTone, fOsc1, fOsc2, fMix, fLadder, fLadEnv, fUnison }) setFrameVisible (f, false);
    if (synth)
        for (int f : { fOsc1, fOsc2, fMix, fLadder, fLadEnv, fUnison }) setFrameVisible (f, true);
    else
    {
        for (int f : { fExciter, fRes, fBody }) setFrameVisible (f, true);
        setFrameVisible (fTone, arch != (int) Archetype::Tonewheel);
        setFrameVisible (fSpec[(size_t) arch], true);
    }

    for (auto* s : sliders)
    {
        const bool used = usesShared ((Archetype) arch, s->id);
        s->setEnabled (used);
        s->setAlpha (used ? 1.0f : 0.3f);
    }
    resized();
}

void ModelPage::resized()
{
    const auto area = getLocalBounds().reduced (10, 8);
    // the model matrix first, top left; everything else after it
    if (arch == (int) Archetype::Synth)
        layoutRows ({ { fModel, fOsc1, fOsc2 }, { fMix, fLadder, fLadEnv, fUnison } }, area);
    else if (arch == (int) Archetype::Tonewheel)       // no tone layer on the tonewheels: the drawbars are their colour
        layoutRows ({ { fModel, fExciter, fBody }, { fRes, fSpec[(size_t) arch] } }, area);
    else
        layoutRows ({ { fModel, fExciter, fTone }, { fRes, fBody, fSpec[(size_t) arch] } }, area);
}

//==============================================================================
//  ARTIC
//==============================================================================
ArticPage::ArticPage (NaturaProcessor& p) : Page (p)
{
    for (int i = 0; i < kMaxArts; ++i)
    {
        auto* b = artButtons.add (new juce::TextButton());
        b->onClick = [this, i] { proc.requestArticulation (i); };
        artGrid.addAndMakeVisible (b);
    }
    fArts = addFrame ("ARTICULATIONS", { &artGrid }, { 12.0f });

    fKs   = addFrame ("KEYSWITCHES",    { column ({ toggle (P::art_ks_on, "ON") }), slider (P::art_ks_base) }, { 1.2f, 1.0f });
    fCc   = addFrame ("CC ZONES",       { column ({ toggle (P::art_cc_on, "ON") }), slider (P::art_cc_num) }, { 1.2f, 1.0f });
    fVel  = addFrame ("VELOCITY ZONES", { column ({ toggle (P::art_vel_on, "ON") }), slider (P::art_vel_split),
                                          slider (P::art_vel_lo), slider (P::art_vel_hi) }, { 1.2f, 1.0f, 1.3f, 1.3f });
    fAuto = addFrame ("AUTO",           { column ({ toggle (P::art_auto_on, "ON") }), slider (P::art_auto_ms) }, { 1.2f, 1.0f });
    fRates= addFrame ("TRILL / RATE",   { slider (P::art_trill), slider (P::art_rate) });

    for (auto* s : sliders)
        if (s->id == P::art_vel_lo || s->id == P::art_vel_hi)
            s->displayFn = [this] (float v) { return juce::String (artName (artAt ((Archetype) arch, (int) std::lround (v)))); };

    setArchetype ((int) proc.value (P::archetype));
    startTimerHz (12);
}

void ArticPage::setArchetype (int a)
{
    arch = juce::jlimit (0, kNumArchetypes - 1, a);
    const auto& list = artsFor ((Archetype) arch);
    for (int i = 0; i < kMaxArts; ++i)
    {
        artButtons[i]->setVisible (i < list.count);
        if (i < list.count) artButtons[i]->setButtonText (artName (list.arts[i]));
    }
    frames[(size_t) fArts].title = "ARTICULATIONS  -  " + juce::String (archetypeName (arch));
    shown = -1;
    for (auto* s : sliders) s->repaint();
    resized();
    refreshButtons();
}

void ArticPage::refreshButtons()
{
    const int cur = proc.engine.currentArt.load();
    if (cur == shown) return;
    shown = cur;
    for (int i = 0; i < kMaxArts; ++i) InstrEditStyle::styleSquareButton (*artButtons[i], i == cur);
    repaint();
}

void ArticPage::timerCallback() { refreshButtons(); }

void ArticPage::resized()
{
    layoutRows ({ { fArts }, { fKs, fCc, fVel, fAuto, fRates } }, getLocalBounds().reduced (10, 8));
    const auto& list = artsFor ((Archetype) arch);
    const int cols = 7, rows = 2;
    auto r = artGrid.getLocalBounds().reduced (6, 4).withTrimmedBottom (26);
    const int w = r.getWidth() / cols, h = juce::jmin (44, r.getHeight() / rows);
    for (int i = 0; i < list.count; ++i)
        artButtons[i]->setBounds (r.getX() + (i % cols) * w + 4, r.getY() + (i / cols) * (h + 8) + 6, w - 8, h);
}

void ArticPage::paint (juce::Graphics& g)
{
    Page::paint (g);
    // where the keyswitches are on the keyboard
    const auto& list = artsFor ((Archetype) arch);
    const int base = (int) proc.value (P::art_ks_base);
    const juce::String hint = proc.value (P::art_ks_on) >= 0.5f
        ? "Keyswitches " + noteName (base) + " - " + noteName (base + list.count - 1) + "  (lit on the keyboard)"
        : "Keyswitches off  -  pick here, by CC zones, by velocity zones or let AUTO choose";
    auto r = artGrid.getBounds().removeFromBottom (24);
    g.setColour (InstrEditStyle::kPanelLabel);
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawText (hint, r, juce::Justification::centred);
}

//==============================================================================
//  SYNTHESIS
//==============================================================================
SynthesisPage::CurveBox::CurveBox (NaturaProcessor& proc)
    : attach (*proc.param (P::vel_curve), [this] (float v) { display.setCurve (v / 100.0f, juce::dontSendNotification); }, nullptr)
{
    addAndMakeVisible (display);
    display.onChange = [this] (float c) { attach.setValueAsCompleteGesture (std::round (c * 100.0f)); };
    attach.sendInitialUpdate();
}

SynthesisPage::SynthesisPage (NaturaProcessor& p) : Page (p)
{
    filterType = std::make_unique<ParamRadio> (proc, P::flt_type, juce::StringArray { "LP", "HP", "BP", "NT" });
    voiceMode  = std::make_unique<ParamRadio> (proc, P::voice_mode, juce::StringArray { "POLY", "MONO LEGATO", "MONO RETRIG" });
    curve      = std::make_unique<CurveBox> (proc);

    fAmp    = addFrame ("AMP ENV. + GAIN", { slider (P::amp_a), slider (P::amp_d), slider (P::amp_s), slider (P::amp_r),
                                             slider (P::amp_shape), slider (P::amp_gain), slider (P::amp_var) });
    fFilter = addFrame ("FILTER",          { filterType.get(), slider (P::flt_cut), slider (P::flt_res), slider (P::flt_key) }, { 0.9f, 1, 1, 1 });
    fFenv   = addFrame ("FILTER ENV.",     { slider (P::fenv_a), slider (P::fenv_d), slider (P::fenv_s), slider (P::fenv_r), slider (P::fenv_amt) });
    fCurve  = addFrame ("VELOCITY CURVE",  { curve.get() }, { 2.3f });
    fVoice  = addFrame ("VOICE MODE",      { voiceMode.get(),
                                             column ({ toggle (P::mono_hold, "HOLD STOLEN"), toggle (P::mono_retrig, "RETRIG STOLEN") }),
                                             slider (P::port), slider (P::voices) }, { 1.6f, 1.7f, 1, 1 });
    fOct    = addFrame ("OCTAVE",          { slider (P::octave) });
    fRange  = addFrame ("ALLOWED NOTES",   { column ({ toggle (P::range_on, "ON") }), slider (P::range_lo), slider (P::range_hi) }, { 1.0f, 1, 1 });
    fChar   = addFrame ("CHARACTER",       { slider (P::chr_drive), slider (P::chr_sat), slider (P::chr_tone) });
}

void SynthesisPage::resized()
{
    layoutRows ({ { fAmp, fFilter, fFenv }, { fCurve, fVoice, fOct, fRange, fChar } }, getLocalBounds().reduced (10, 8));
}

//==============================================================================
//  MODULATION
//==============================================================================
ModulationPage::ModulationPage (NaturaProcessor& p) : Page (p)
{
    fAlfo  = addFrame ("AMP LFO",      { slider (P::alfo_rate), slider (P::alfo_depth), slider (P::alfo_delay) });
    fFlfo  = addFrame ("FILTER LFO",   { slider (P::flfo_rate), slider (P::flfo_depth), slider (P::flfo_delay) });
    fVib   = addFrame ("VIBRATO",      { slider (P::vib_rate), slider (P::vib_depth), slider (P::vib_delay), slider (P::vib_wheel) });
    fGlide = addFrame ("ATTACK GLIDE", { slider (P::ag_mode), slider (P::ag_depth), slider (P::ag_time), slider (P::ag_shape),
                                         slider (P::ag_every), slider (P::ag_odds), slider (P::ag_vel) }, { 1.3f, 1, 1, 1, 1, 1.1f, 1 });
    fPerf  = addFrame ("PERFORMANCE",  { slider (P::humanize), slider (P::pb_range), slider (P::dyn_src), slider (P::mw_target),
                                         slider (P::at_target), slider (P::leg_vel) }, { 1, 1, 1.6f, 1.4f, 1.4f, 1 });
}

void ModulationPage::resized()
{
    layoutRows ({ { fAlfo, fFlfo, fVib, fGlide }, { fPerf } }, getLocalBounds().reduced (10, 8));
}

//==============================================================================
//  FX
//==============================================================================
FxPage::FxPage (NaturaProcessor& p) : Page (p)
{
    fRot = addFrame ("ROTARY",  { column ({ toggle (P::rot_on, "ON"), toggle (P::rot_fast, "FAST") }),
                                  slider (P::rot_drive), slider (P::rot_balance), slider (P::rot_mix) }, { 1.1f, 1, 1, 1 });
    fPha = addFrame ("PHASER",  { column ({ toggle (P::pha_on, "ON") }), slider (P::pha_rate), slider (P::pha_depth),
                                  slider (P::pha_fb), slider (P::pha_mix) }, { 1.1f, 1, 1, 1, 1 });
    fCho = addFrame ("CHORUS",  { column ({ toggle (P::cho_on, "ON") }), slider (P::cho_rate), slider (P::cho_depth), slider (P::cho_mix) }, { 1.1f, 1, 1, 1 });
    fDly = addFrame ("DELAY",   { column ({ toggle (P::dly_on, "ON"), toggle (P::dly_pingpong, "PING PONG") }), slider (P::dly_time),
                                  slider (P::dly_fb), slider (P::dly_tone), slider (P::dly_mix) }, { 1.3f, 1, 1, 1, 1 });
    fRev = addFrame ("REVERB",  { column ({ toggle (P::rev_on, "ON") }), slider (P::rev_size), slider (P::rev_damp),
                                  slider (P::rev_predelay), slider (P::rev_mix) }, { 1.1f, 1, 1, 1, 1 });
    fOut = addFrame ("OUTPUT",  { slider (P::eq_low), slider (P::eq_high), slider (P::out_width), slider (P::out_volume) });
}

void FxPage::resized()
{
    layoutRows ({ { fRot, fPha, fCho }, { fDly, fRev, fOut } }, getLocalBounds().reduced (10, 8));
}

//==============================================================================
//  KEYBOARD
//==============================================================================
NaturaKeyboard::NaturaKeyboard (NaturaProcessor& p)
    : juce::MidiKeyboardComponent (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard), proc (p)
{
    setAvailableRange (21, 108);
    setOctaveForMiddleC (3);
    setScrollButtonsVisible (false);
    setColour (whiteNoteColourId, juce::Colour (0xFFE8E8EE));
    setColour (blackNoteColourId, juce::Colour (0xFF16161C));
    setColour (keyDownOverlayColourId, juce::Colour (Theme::Gold::kAccent).withAlpha (0.75f));
    setColour (mouseOverKeyOverlayColourId, juce::Colour (Theme::Gold::kAccentLight).withAlpha (0.25f));
}

void NaturaKeyboard::update()
{
    const bool on = proc.value (P::art_ks_on) >= 0.5f;
    const int base = (int) proc.value (P::art_ks_base);
    const int count = artsFor ((Archetype) (int) proc.value (P::archetype)).count;
    const int active = proc.engine.currentArt.load();
    if (on != ksOn || base != ksBase || count != ksCount || active != ksActive)
    {
        ksOn = on; ksBase = base; ksCount = count; ksActive = active;
        repaint();
    }
}

int NaturaKeyboard::ksState (int note) const
{
    if (! ksOn || note < ksBase || note >= ksBase + ksCount) return 0;
    return note - ksBase == ksActive ? 2 : 1;
}

void NaturaKeyboard::drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area,
                                    bool isDown, bool isOver, juce::Colour lineColour, juce::Colour textColour)
{
    juce::MidiKeyboardComponent::drawWhiteNote (note, g, area, isDown, isOver, lineColour, textColour);
    if (const int st = ksState (note))
    {
        g.setColour (juce::Colour (Theme::Gold::kAccent).withAlpha (st == 2 ? 0.85f : 0.35f));
        g.fillRect (area.reduced (1.0f, 0.0f).withTrimmedTop (area.getHeight() * 0.45f));
    }
}

void NaturaKeyboard::drawBlackNote (int note, juce::Graphics& g, juce::Rectangle<float> area,
                                    bool isDown, bool isOver, juce::Colour noteFillColour)
{
    juce::MidiKeyboardComponent::drawBlackNote (note, g, area, isDown, isOver, noteFillColour);
    if (const int st = ksState (note))
    {
        g.setColour (juce::Colour (Theme::Gold::kAccent).withAlpha (st == 2 ? 0.95f : 0.5f));
        g.fillRect (area.reduced (2.0f).withTrimmedTop (area.getHeight() * 0.5f));
    }
}

//==============================================================================
//  EDITOR
//==============================================================================
NaturaEditor::NaturaEditor (NaturaProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);

    presetName.onClick = [this] { showPresetMenu(); };
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    loadBtn.onClick = [this] { loadPreset(); };
    saveBtn.onClick = [this] { savePresetAs(); };
    quickBtn.onClick = [this] { quickSave(); };
    for (auto* b : { &presetName, &prevBtn, &nextBtn, &loadBtn, &saveBtn, &quickBtn }) addAndMakeVisible (b);

    status.setJustificationType (juce::Justification::centredRight);
    status.setColour (juce::Label::textColourId, juce::Colour (Theme::Gold::kAccentLight));
    status.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    addAndMakeVisible (status);

    const char* names[numTabs] = { "LIBRARY", "MODEL", "ARTIC", "SYNTHESIS", "MODULATION", "FX" };
    for (int i = 0; i < numTabs; ++i)
    {
        tabs[i].setButtonText (names[i]);
        tabs[i].onClick = [this, i] { showTab (i); };
        addAndMakeVisible (tabs[i]);
    }

    library    = std::make_unique<LibraryPage> (proc);
    library->onLoaded = [this] (int factory, const juce::File& file)
    {
        presetCursor = factory >= 0 ? factory : -1;
        if (factory < 0)
        {
            const auto user = PresetManager::userPresets();
            for (int i = 0; i < user.size(); ++i)
                if (user[i] == file) presetCursor = (int) factoryPresets().size() + i;
        }
        refreshPresetLabel();
    };
    model      = std::make_unique<ModelPage> (proc);
    artic      = std::make_unique<ArticPage> (proc);
    synthesis  = std::make_unique<SynthesisPage> (proc);
    modulation = std::make_unique<ModulationPage> (proc);
    fxPage     = std::make_unique<FxPage> (proc);
    for (juce::Component* c : { (juce::Component*) library.get(), (juce::Component*) model.get(), (juce::Component*) artic.get(),
                                (juce::Component*) synthesis.get(), (juce::Component*) modulation.get(), (juce::Component*) fxPage.get() })
        addChildComponent (c);

    keyboard = std::make_unique<NaturaKeyboard> (proc);
    addAndMakeVisible (*keyboard);

    setSize (Theme::Natura::width, Theme::Natura::height);
    showTab (0);
    refreshPresetLabel();
    startTimerHz (15);
}

NaturaEditor::~NaturaEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void NaturaEditor::showTab (int index)
{
    currentTab = index;
    juce::Component* pages[numTabs] = { library.get(), model.get(), artic.get(), synthesis.get(), modulation.get(), fxPage.get() };
    if (index == 0) library->rescan();
    for (int i = 0; i < numTabs; ++i)
    {
        pages[i]->setVisible (i == index);
        InstrEditStyle::styleSquareButton (tabs[i], i == index);
    }
}

void NaturaEditor::paint (juce::Graphics& g)
{
    using namespace Theme::Color;
    g.fillAll (bgPage);
    g.setColour (bgStrip);
    g.fillRect (0, 0, getWidth(), 56);
    g.setColour (border);
    g.drawHorizontalLine (56, 0.0f, (float) getWidth());

    g.setColour (juce::Colour (Theme::Gold::kAccent));
    g.setFont (juce::Font (juce::FontOptions (30.0f, juce::Font::bold)));
    g.drawText ("NATURA", 18, 6, 170, 44, juce::Justification::centredLeft);
    g.setColour (textSecondary);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText ("by FANAN", 152, 26, 80, 20, juce::Justification::centredLeft);
}

void NaturaEditor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (56).reduced (12, 10);
    top.removeFromLeft (240);
    prevBtn.setBounds (top.removeFromLeft (32));
    top.removeFromLeft (4);
    presetName.setBounds (top.removeFromLeft (320));
    top.removeFromLeft (4);
    nextBtn.setBounds (top.removeFromLeft (32));
    top.removeFromLeft (12);
    loadBtn.setBounds (top.removeFromLeft (70));
    top.removeFromLeft (6);
    saveBtn.setBounds (top.removeFromLeft (70));
    top.removeFromLeft (6);
    quickBtn.setBounds (top.removeFromLeft (110));
    status.setBounds (top.reduced (8, 0));

    auto tabRow = r.removeFromTop (40).reduced (12, 6);
    for (int i = 0; i < numTabs; ++i) { tabs[i].setBounds (tabRow.removeFromLeft (130)); tabRow.removeFromLeft (8); }

    keyboard->setBounds (r.removeFromBottom (84).reduced (12, 6));
    const float keyW = (float) keyboard->getWidth() / 52.0f;   // 52 white keys from A-1 to C7
    keyboard->setKeyWidth (keyW);

    for (juce::Component* c : { (juce::Component*) library.get(), (juce::Component*) model.get(), (juce::Component*) artic.get(),
                                (juce::Component*) synthesis.get(), (juce::Component*) modulation.get(), (juce::Component*) fxPage.get() })
        c->setBounds (r);
}

void NaturaEditor::timerCallback()
{
    const int a = (int) proc.value (P::archetype);
    if (a != shownArch)
    {
        shownArch = a;
        model->setArchetype (a);
        artic->setArchetype (a);
    }
    refreshPresetLabel();
    library->refresh();
    keyboard->update();

    const int art = proc.engine.currentArt.load();
    const juce::String txt = juce::String (artName (artAt ((Archetype) a, art))) + "   |   "
                           + juce::String (proc.engine.activeVoices.load()) + " VOICES";
    if (status.getText() != txt) status.setText (txt, juce::dontSendNotification);
}

void NaturaEditor::refreshPresetLabel()
{
    const auto name = proc.presetName + (proc.presetFile != juce::File() ? "" : "");
    if (name != shownPreset) { shownPreset = name; presetName.setButtonText (name); }
}

//==============================================================================
//  PRESETS
//==============================================================================
void NaturaEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto& fac = factoryPresets();
    const int arch = (int) proc.value (P::archetype);

    menu.addSectionHeader ("ROOT MODELS");
    juce::PopupMenu roots;
    for (int a = 0; a < kNumArchetypes; ++a) roots.addItem (1000 + a, archetypeName (a), true, a == arch);
    menu.addSubMenu ("Load a root model", roots);

    menu.addSectionHeader ("FACTORY");
    for (int a = 0; a < kNumArchetypes; ++a)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < (int) fac.size(); ++i)
            if ((int) fac[(size_t) i].arch == a)
                sub.addItem (1 + i, fac[(size_t) i].name, true, proc.presetName == fac[(size_t) i].name && proc.presetFile == juce::File());
        menu.addSubMenu (archetypeName (a), sub);
    }

    const auto user = PresetManager::userPresets();
    menu.addSectionHeader ("MY PRESETS");
    if (user.isEmpty()) menu.addItem (-1, "(none saved yet)", false);
    for (int i = 0; i < user.size(); ++i)
        menu.addItem (5000 + i, user[i].getFileNameWithoutExtension(), true, user[i] == proc.presetFile);
    menu.addSeparator();
    menu.addItem (9000, "Show presets folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
        [this, user] (int r)
        {
            if (r <= 0) return;
            if (r >= 1000 && r < 1000 + kNumArchetypes) { proc.loadArchetypeRoot (r - 1000); presetCursor = -1; }
            else if (r >= 1 && r <= (int) factoryPresets().size()) { proc.loadFactory (r - 1); presetCursor = r - 1; }
            else if (r >= 5000 && r < 5000 + user.size()) { proc.loadPresetFile (user[r - 5000]); presetCursor = (int) factoryPresets().size() + (r - 5000); }
            else if (r == 9000) { PresetManager::folder().createDirectory(); PresetManager::folder().startAsProcess(); }
            refreshPresetLabel();
        });
}

void NaturaEditor::stepPreset (int delta)
{
    const auto& fac = factoryPresets();
    const auto user = PresetManager::userPresets();
    const int total = (int) fac.size() + user.size();
    if (total == 0) return;
    if (presetCursor < 0)
    {
        presetCursor = 0;
        for (int i = 0; i < (int) fac.size(); ++i) if (proc.presetName == juce::String (fac[(size_t) i].name)) presetCursor = i;
        for (int i = 0; i < user.size(); ++i) if (user[i] == proc.presetFile) presetCursor = (int) fac.size() + i;
        if (delta > 0) presetCursor -= 1;
    }
    presetCursor = ((presetCursor + delta) % total + total) % total;
    if (presetCursor < (int) fac.size()) proc.loadFactory (presetCursor);
    else proc.loadPresetFile (user[presetCursor - (int) fac.size()]);
    refreshPresetLabel();
}

void NaturaEditor::loadPreset()
{
    PresetManager::folder().createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Load a Natura preset", PresetManager::folder(), juce::String ("*") + PresetManager::extension);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f.existsAsFile() && ! proc.loadPresetFile (f))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Natura", "That file is not a Natura preset.");
            presetCursor = -1;
            refreshPresetLabel();
        });
}

void NaturaEditor::savePresetAs()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name for this sound:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", proc.presetName.replace (" (root)", ""));
    w->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->setLookAndFeel (&laf);
    juce::Component::SafePointer<NaturaEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r)
    {
        if (safe != nullptr && r == 1)
        {
            const auto name = PresetManager::cleanName (w->getTextEditorContents ("name"));
            const auto file = PresetManager::folder().getChildFile (name + PresetManager::extension);
            const auto res = safe->proc.savePresetFile (file, name);
            if (res.failed())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Natura", res.getErrorMessage());
            safe->presetCursor = -1;
            safe->refreshPresetLabel();
            safe->library->rescan();
        }
    }), true);
}

void NaturaEditor::quickSave()
{
    if (proc.presetFile != juce::File())
    {
        const auto res = proc.savePresetFile (proc.presetFile, proc.presetName);
        if (res.failed())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Natura", res.getErrorMessage());
        library->rescan();
        return;
    }
    savePresetAs();   // factory and root sounds are never overwritten
}
