// C:\workspace\SONIFY PLUGIN zax\src\PluginEditor.cpp
#include "PluginEditor.h"

namespace
{
    constexpr int kWidth  = 1560;
    constexpr int kHeight = 860;

    const char* kFxInfo =
        "The effect chain of the selected key's FAMILY: every key of that family plays through it. "
        "The stages run one after the other, left to right - no sends. A stage that is off is skipped.\n"
        "SWEET|The sweetener: soften or sharpen the attack, peak control, tame the harsh band, round warmth.\n"
        "EQ|10 bands, 31 Hz to 16 kHz - each slider is the volume of its own band. 0 = silent, 100 = flat, 200 = +24 dB.\n"
        "SAT|Saturation: DRIVE and the wet MIX.\n"
        "COMP|A compressor: threshold, ratio, attack, release, make-up gain.\n"
        "CHORUS|Grex's BBD chorus: rate, depth and how much of it you hear.\n"
        "DELAY|A ping-pong echo. NOTE locks it to the song's tempo (FREE uses TIME), then feedback, damping, low cut, mix.\n"
        "REVERB|Grex's hall: size, decay, damping, pre-delay, the reverb's own low and high cut, and its mix.\n"
        "OUT|PAN: where the family sits, 100 = centre. LEVEL: the family's level, 133 = 0 dB.\n"
        "ON|Switches the page's stage on or off. Moving any of its sliders switches it on.";

    const char* kKitInfo =
        "The whole kit.\n"
        "VOLUME|The kit's output level, from -24 to +12 dB. 133 = 0 dB.";

    juce::String soundLabel (const edm::Sound& s)   { return s.isEmpty() ? juce::String() : juce::String (s.name()); }
}

//==============================================================================
ZaxEditor::ZaxEditor (ZaxProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    // Everything is laid out at the design size inside `content`; the window
    // shows it scaled by the UI SIZE menu.
    content.setInterceptsMouseClicks (false, true);
    addAndMakeVisible (content);

    sizeLbl.setText ("UI SIZE", juce::dontSendNotification);
    sizeLbl.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
    sizeLbl.setColour (juce::Label::textColourId, juce::Colour (0xFF8A8A8A));
    sizeLbl.setJustificationType (juce::Justification::centredRight);
    content.addAndMakeVisible (sizeLbl);
    for (int pct = 40, id = 1; pct <= 100; pct += 5, ++id)
        sizeBox.addItem (juce::String (pct) + "%", id);
    sizeBox.setSelectedId (juce::jlimit (1, 13, (int) std::lround ((proc.getUiScale() * 100.0f - 40.0f) / 5.0f) + 1),
                           juce::dontSendNotification);
    sizeBox.onChange = [this]
    {
        const float scale = (40.0f + 5.0f * (float) (sizeBox.getSelectedId() - 1)) / 100.0f;
        proc.setUiScale (scale);
        applyScale();
    };
    content.addAndMakeVisible (sizeBox);

    titleLbl.setText ("ZAX", juce::dontSendNotification);
    titleLbl.setFont (juce::Font (juce::FontOptions (30.0f).withStyle ("Bold")));
    titleLbl.setColour (juce::Label::textColourId, juce::Colour (Pal::kAccentLight));
    content.addAndMakeVisible (titleLbl);

    subLbl.setText ("GM DRUM SYNTH", juce::dontSendNotification);
    subLbl.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")));
    subLbl.setColour (juce::Label::textColourId, juce::Colour (0xFF8A8A8A));
    content.addAndMakeVisible (subLbl);

    kitBox.onChange = [this] { if (! updating) kitChosen(); };
    content.addAndMakeVisible (kitBox);

    loadBtn.setTooltip ("Load a kit file from anywhere: one someone shared, or a folder of your own");
    loadBtn.onClick = [this] { loadKitFile(); };
    saveBtn.setTooltip ("Save the kit over its file");
    saveBtn.onClick = [this] { saveKit(); };
    saveAsBtn.setTooltip ("Save the kit as a new file in My Kits");
    saveAsBtn.onClick = [this] { saveKitAs(); };
    reloadBtn.setTooltip ("Drop every change and load the kit's file again");
    reloadBtn.onClick = [this] { reloadKit(); };
    for (auto* b : { &loadBtn, &saveBtn, &saveAsBtn, &reloadBtn }) content.addAndMakeVisible (*b);

    statusLbl.setColour (juce::Label::textColourId, juce::Colour (0xFF9A9A9A));
    statusLbl.setJustificationType (juce::Justification::centredRight);
    content.addAndMakeVisible (statusLbl);

    keyboard.setKit (&proc.kit());
    keyboard.onPress   = [this] (int key, int vel) { proc.auditionNoteOn (key, vel); };
    keyboard.onRelease = [this] (int key)          { proc.auditionNoteOff (key); };
    keyboard.onSelect  = [this] (int key)          { selectKey (key); };
    content.addAndMakeVisible (keyboard);

    keyLbl.setFont (juce::Font (juce::FontOptions (20.0f).withStyle ("Bold")));
    keyLbl.setColour (juce::Label::textColourId, juce::Colour (0xFFEAEAEA));
    content.addAndMakeVisible (keyLbl);
    gmLbl.setFont (juce::Font (juce::FontOptions (15.0f).withStyle ("Bold")));
    gmLbl.setColour (juce::Label::textColourId, juce::Colour (Pal::kAccentLight));
    content.addAndMakeVisible (gmLbl);
    roleLbl.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")));
    roleLbl.setTooltip ("A key's role is fixed by the GM map: its sound menu lists only sounds of that role.");
    content.addAndMakeVisible (roleLbl);

    for (auto* l : { &soundLbl, &chokeLbl })
    {
        l->setColour (juce::Label::textColourId, juce::Colour (0xFF9A9A9A));
        l->setFont (juce::Font (juce::FontOptions (11.0f)));
        content.addAndMakeVisible (*l);
    }
    soundLbl.setText ("SOUND", juce::dontSendNotification);
    chokeLbl.setText ("CHOKE", juce::dontSendNotification);

    soundBox.onChange = [this] { if (! updating) soundChosen(); };
    content.addAndMakeVisible (soundBox);

    chokeBox.addItem ("No choke", 1);
    for (int i = 1; i <= 8; ++i) chokeBox.addItem ("Choke group " + juce::String (i), i + 1);
    chokeBox.onChange = [this]
    {
        if (updating) return;
        cell().chokeGroup = chokeBox.getSelectedId() - 1;
        edited();
    };
    content.addAndMakeVisible (chokeBox);

    resetBtn.setTooltip ("The key's sound as it ships: neutral macros, its own shape, open filter, 0 dB");
    resetBtn.onClick = [this]
    {
        auto& c = cell();
        if (const auto* lib = edm::params::librarySound (c.sound)) c.sound = *lib;
        c.macros = drum::Macros();
        c.gainDb = 0.0f;
        c.filterHpNorm = 0.0f;
        c.filterLpNorm = 1.0f;
        edited();
        refreshEditor();
    };
    content.addAndMakeVisible (resetBtn);
    content.addAndMakeVisible (midiLed);

    hintLbl.setText ("Click a key to play it and edit it. Lower on the key = harder. "
                     "Right double-click a slider to reset it.", juce::dontSendNotification);
    hintLbl.setColour (juce::Label::textColourId, juce::Colour (0xFF7A7A7A));
    hintLbl.setFont (juce::Font (juce::FontOptions (11.0f)));
    hintLbl.setJustificationType (juce::Justification::topLeft);
    content.addAndMakeVisible (hintLbl);

    tabBar.onSelect = [this] (int i) { currentTab = i; rebuildSliders(); };
    tabBar.onInfo   = [this] (int i) { showTabInfo (i); };
    content.addAndMakeVisible (tabBar);

    {
        juce::StringArray pages;
        for (int i = 0; i < zax::kNumFxPages; ++i) pages.add (zax::fxPageName (i));
        pageBar.setPages (pages);
    }
    pageBar.onSelect = [this] (int pg) { fxPage = pg; rebuildSliders(); };
    content.addChildComponent (pageBar);

    stageBtn.setClickingTogglesState (false);
    stageBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFF3CB85A));
    stageBtn.onClick = [this] { toggleFxStage(); };
    content.addChildComponent (stageBtn);

    fxFamilyLbl.setFont (juce::Font (juce::FontOptions (13.0f).withStyle ("Bold")));
    content.addChildComponent (fxFamilyLbl);

    content.addChildComponent (filterSlider);
    filterSlider.onChange = [this] (float lo, float hi)
    {
        if (updating) return;
        cell().filterHpNorm = lo;
        cell().filterLpNorm = hi;
        edited();
    };

    content.setBounds (0, 0, kWidth, kHeight);
    layoutContent();
    applyScale();
    refreshKitMenu();
    selectKey (36);
    startTimerHz (30);
}

ZaxEditor::~ZaxEditor()
{
    stopTimer();
}

//==============================================================================
void ZaxEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF161616));
    g.addTransform (juce::AffineTransform::scale (shownScale));
    g.setColour (juce::Colour (0xFF2A2A2A));
    g.drawHorizontalLine (headerBottom, 12.0f, (float) kWidth - 12.0f);
    g.drawHorizontalLine (keyboardBottom, 12.0f, (float) kWidth - 12.0f);
}

// The window at the chosen size: the design-size content, scaled
void ZaxEditor::applyScale()
{
    shownScale = juce::jlimit (0.4f, 1.0f, proc.getUiScale());
    content.setTransform (juce::AffineTransform::scale (shownScale));
    setSize (juce::roundToInt ((float) kWidth * shownScale), juce::roundToInt ((float) kHeight * shownScale));
    repaint();
}

void ZaxEditor::resized()
{
}

void ZaxEditor::layoutContent()
{
    auto area = juce::Rectangle<int> (0, 0, kWidth, kHeight).reduced (12);

    auto header = area.removeFromTop (40);
    titleLbl.setBounds (header.removeFromLeft (76));
    subLbl.setBounds (header.removeFromLeft (120));
    header.removeFromLeft (10);
    kitBox.setBounds (header.removeFromLeft (260).reduced (0, 6));
    header.removeFromLeft (10);
    loadBtn.setBounds (header.removeFromLeft (80).reduced (0, 6));
    header.removeFromLeft (6);
    saveBtn.setBounds (header.removeFromLeft (80).reduced (0, 6));
    header.removeFromLeft (6);
    saveAsBtn.setBounds (header.removeFromLeft (90).reduced (0, 6));
    header.removeFromLeft (6);
    reloadBtn.setBounds (header.removeFromLeft (90).reduced (0, 6));
    sizeBox.setBounds (header.removeFromRight (86).reduced (0, 6));
    sizeLbl.setBounds (header.removeFromRight (64));
    header.removeFromRight (16);
    statusLbl.setBounds (header);
    headerBottom = area.getY() + 2;
    area.removeFromTop (6);

    const int kbH = 2 * ZaxKeyboard::kLabelBand + 250;
    keyboard.setBounds (area.removeFromTop (kbH));
    keyboardBottom = area.getY() + 4;
    area.removeFromTop (12);

    auto left = area.removeFromLeft (270);
    keyLbl.setBounds (left.removeFromTop (28));
    gmLbl.setBounds (left.removeFromTop (22));
    {
        auto row = left.removeFromTop (20);
        midiLed.setBounds (row.removeFromRight (64));
        roleLbl.setBounds (row);
    }
    left.removeFromTop (8);
    soundLbl.setBounds (left.removeFromTop (14));
    soundBox.setBounds (left.removeFromTop (26));
    left.removeFromTop (6);
    chokeLbl.setBounds (left.removeFromTop (14));
    chokeBox.setBounds (left.removeFromTop (26));
    left.removeFromTop (8);
    resetBtn.setBounds (left.removeFromTop (26));
    left.removeFromTop (8);
    hintLbl.setBounds (left.removeFromTop (48));

    area.removeFromLeft (16);
    tabBar.setBounds (area.removeFromTop (26));
    area.removeFromTop (6);
    sliderArea = area;
    layoutSliders();
}

//==============================================================================
void ZaxEditor::timerCallback()
{
    proc.housekeeping();

    // A song restored with another window size
    if (std::abs (proc.getUiScale() - shownScale) > 0.001f)
    {
        sizeBox.setSelectedId (juce::jlimit (1, 13, (int) std::lround ((proc.getUiScale() * 100.0f - 40.0f) / 5.0f) + 1),
                               juce::dontSendNotification);
        applyScale();
    }

    std::array<uint32_t, 4> m {};
    proc.readActivity (m);
    keyboard.setActivity (m);
    midiLed.setOn (((m[(size_t) (selectedKey >> 5)] >> (selectedKey & 31)) & 1u) != 0);

    // A kit loaded from outside (a song's state, a preset): show it
    if (proc.kitGeneration() != shownGeneration)
    {
        refreshKitMenu();
        keyboard.setKit (&proc.kit());
        selectKey (selectedKey);
    }
}

//==============================================================================
//  The kit menu: factory kits, then My Kits.  Item id = entry index + 1.
void ZaxEditor::refreshKitMenu()
{
    shownGeneration = proc.kitGeneration();
    kitEntries = zax::files::listKits();
    const juce::ScopedValueSetter<bool> svs (updating, true);
    kitBox.clear (juce::dontSendNotification);
    bool mineStarted = false;
    kitBox.addSectionHeading ("FACTORY KITS");
    int current = 0;
    for (int i = 0; i < (int) kitEntries.size(); ++i)
    {
        const auto& e = kitEntries[(size_t) i];
        if (! e.factory && ! mineStarted)
        {
            kitBox.addSeparator();
            kitBox.addSectionHeading ("MY KITS");
            mineStarted = true;
        }
        kitBox.addItem (e.name, i + 1);
        if (e.file == proc.kitFile()) current = i + 1;
    }
    if (current > 0) kitBox.setSelectedId (current, juce::dontSendNotification);
    else             kitBox.setText (proc.kitName(), juce::dontSendNotification);
    updateKitStatus();
}

void ZaxEditor::updateKitStatus()
{
    const auto f = proc.kitFile();
    juce::String s = proc.isModified() ? "*  changed - not saved     " : "";
    s << (f != juce::File() ? f.getFileName() : juce::String ("(no file - use SAVE AS)"));
    statusLbl.setText (s, juce::dontSendNotification);
    saveBtn.setEnabled (f != juce::File());
    reloadBtn.setEnabled (f.existsAsFile() || zax::findFactoryKit (proc.kitName().toStdString()) != nullptr);
}

void ZaxEditor::kitChosen()
{
    const int i = kitBox.getSelectedId() - 1;
    if (i < 0 || i >= (int) kitEntries.size()) return;
    const auto r = proc.loadKit (kitEntries[(size_t) i]);
    if (r.failed())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Kit not loaded", r.getErrorMessage(), "OK", this);
    shownGeneration = proc.kitGeneration();
    keyboard.setKit (&proc.kit());
    updateKitStatus();
    selectKey (selectedKey);
}

void ZaxEditor::loadKitFile()
{
    // A kit from anywhere, not only the two folders the menu lists: a kit someone shared, or one
    // kept in a folder of the user's own. It becomes the kit that's loaded, with its file, so
    // SAVE writes straight back over it.
    const auto folder = zax::files::userFolder();
    folder.createDirectory();

    chooser = std::make_unique<juce::FileChooser> ("Load a Zax kit", folder,
                                                   juce::String ("*") + zax::files::kExtension);

    juce::Component::SafePointer<ZaxEditor> safe (this);

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe] (const juce::FileChooser& fc)
        {
            if (safe == nullptr)
                return;

            const auto file = fc.getResult();

            if (! file.existsAsFile())
                return;

            const auto r = safe->proc.loadKit ({ file.getFileNameWithoutExtension(), file, false });

            if (r.failed())
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Kit not loaded",
                                                        r.getErrorMessage(), "OK", safe.getComponent());
                return;
            }

            safe->shownGeneration = safe->proc.kitGeneration();
            safe->keyboard.setKit (&safe->proc.kit());
            safe->refreshKitMenu();
            safe->updateKitStatus();
            safe->selectKey (safe->selectedKey);
        });
}

void ZaxEditor::saveKit()
{
    const auto r = proc.saveKit();
    if (r.failed())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Kit NOT saved", r.getErrorMessage(), "OK", this);
    refreshKitMenu();
}

void ZaxEditor::saveKitAs()
{
    const auto folder = zax::files::userFolder();
    folder.createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Save Zax kit as",
                                                   folder.getChildFile (zax::files::fileNameFor (proc.kitName())),
                                                   juce::String ("*") + zax::files::kExtension);
    juce::Component::SafePointer<ZaxEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr) return;
        auto file = fc.getResult();
        if (file == juce::File()) return;
        file = file.withFileExtension (zax::files::kExtension);
        const auto r = safe->proc.saveKitAs (file, file.getFileNameWithoutExtension());
        if (r.failed())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Kit NOT saved", r.getErrorMessage(), "OK", safe.getComponent());
        safe->refreshKitMenu();
    });
}

void ZaxEditor::reloadKit()
{
    for (const auto& e : kitEntries)
        if (e.file == proc.kitFile() || (proc.kitFile() == juce::File() && e.name == proc.kitName()))
        {
            proc.loadKit (e);
            break;
        }
    shownGeneration = proc.kitGeneration();
    keyboard.setKit (&proc.kit());
    refreshKitMenu();
    selectKey (selectedKey);
}

//==============================================================================
void ZaxEditor::selectKey (int key)
{
    selectedKey = juce::jlimit (zax::kFirstKey, zax::kLastKey, key);
    keyboard.setSelected (selectedKey);
    const juce::ScopedValueSetter<bool> svs (updating, true);

    const auto* g = zax::gm (selectedKey);
    keyLbl.setText ("KEY " + juce::String (selectedKey) + "   " + juce::String (zax::noteName (selectedKey)),
                    juce::dontSendNotification);
    gmLbl.setText (g->name, juce::dontSendNotification);
    roleLbl.setText (juce::String (juce::CharPointer_UTF8 ("ROLE: ")) + juce::String (zax::roleName (selectedKey)) + "  (locked)",
                     juce::dontSendNotification);
    roleLbl.setColour (juce::Label::textColourId, juce::Colour (zax::familyArgb (g->family)).brighter (0.4f));

    // SOUND: only this key's role
    soundBox.clear (juce::dontSendNotification);
    soundIndices = zax::soundsForKey (selectedKey);
    const auto& c = cell();
    const int cur = zax::libraryIndexOf (c);
    int selId = 0;
    for (int i = 0; i < (int) soundIndices.size(); ++i)
    {
        soundBox.addItem (soundLabel (edm::library()[(size_t) soundIndices[(size_t) i]].sound), i + 1);
        if (soundIndices[(size_t) i] == cur) selId = i + 1;
    }
    soundBox.setSelectedId (selId, juce::dontSendNotification);
    chokeBox.setSelectedId (juce::jlimit (0, 8, c.chokeGroup) + 1, juce::dontSendNotification);
    refreshEditor();
}

// Tabs for the key's voice; another kind of voice starts on MAIN.
void ZaxEditor::refreshEditor()
{
    const auto& snd = cell().sound;
    juce::StringArray names { "MAIN" };
    if (const auto* t = edm::params::tableFor (snd))
        for (const char* n : t->tabs) names.add (n);
    names.add ("FX");
    fxTab = names.size() - 1;
    names.add ("KIT");
    kitTab = names.size() - 1;

    // FX and KIT stay open from key to key; a voice tab stays open while the
    // next key is the same kind of voice; anything else starts on MAIN.
    const juce::String was = tabBar.getTabName (currentTab);
    const int kind = (int) snd.type * 8 + edm::params::kickModel (snd) + edm::params::snareModel (snd);
    if (was == "FX")        currentTab = fxTab;
    else if (was == "KIT")  currentTab = kitTab;
    else if (kind != tabKey) currentTab = 0;
    tabKey = kind;
    currentTab = juce::jlimit (0, names.size() - 1, currentTab);
    tabBar.setTabs (names, currentTab);
    rebuildSliders();
}

float ZaxEditor::toUi (int k, float v)
{
    if (k == kTune) return (v + 25.0f) * 4.0f;
    if (k == kGain) return (v + 20.0f) * 5.0f;
    return v * 200.0f;
}

float ZaxEditor::fromUi (int k, float u)
{
    if (k == kTune) return u / 4.0f - 25.0f;
    if (k == kGain) return u / 5.0f - 20.0f;
    return u / 200.0f;
}

float ZaxEditor::defaultFor (int k)
{
    const drum::Macros m;
    switch (k)
    {
        case kDecay:   return m.decay;
        case kDamp:    return m.damp;
        case kSnap:    return m.snap;
        case kColor:   return m.color;
        case kDrive:   return m.drive;
        case kHuman:   return m.humanize;
        case kVelSens: return m.velSens;
        default:       return 0.0f;
    }
}

void ZaxEditor::rebuildSliders()
{
    const juce::ScopedValueSetter<bool> svs (updating, true);
    sliders.clear();
    sliderSpecs.clear();
    fxSliderSpec.clear();

    const auto& c = cell();
    filterSlider.setVisible (currentTab == 0);
    filterSlider.setValues (c.filterHpNorm, c.filterLpNorm, false);

    const bool fxShown = currentTab == fxTab;
    pageBar.setVisible (fxShown);
    fxFamilyLbl.setVisible (fxShown);
    stageBtn.setVisible (fxShown && zax::fxStage (proc.kit().fx[0], fxPage) != nullptr);

    auto addSlider = [this] (const juce::String& label, float def, float value) -> zax::GoldSlider&
    {
        auto s = std::make_unique<zax::GoldSlider> (label, 0.0f, 200.0f, def);
        s->setStep (1.0f);
        s->displayFn = [] (float v) { return juce::String ((int) std::lround (v)); };
        s->setValue (value, false);
        content.addAndMakeVisible (*s);
        sliders.push_back (std::move (s));
        return *sliders.back();
    };

    if (fxShown)
    {
        buildFxPage();
    }
    else if (currentTab == kitTab)
    {
        // VOLUME: -24 .. +12 dB on 0..200
        auto toU = [] (float db) { return (db + 24.0f) / 36.0f * 200.0f; };
        auto& s = addSlider ("VOLUME", toU (0.0f), toU (proc.kit().volumeDb));
        s.displayFn = [] (float v) { return juce::String (v / 200.0f * 36.0f - 24.0f, 1) + "dB"; };
        s.onChange = [this] (float v)
        {
            if (updating) return;
            proc.kit().volumeDb = v / 200.0f * 36.0f - 24.0f;
            edited();
        };
    }
    else if (currentTab == 0)
    {
        static const char* names[kNumKnobs] = { "TUNE", "DECAY", "DAMP", "SNAP", "COLOR", "DRIVE", "HUMAN", "VEL SENS", "GAIN" };
        const auto& m = c.macros;
        const float vals[kNumKnobs] = { m.tuneSemis, m.decay, m.damp, m.snap, m.color, m.drive, m.humanize, m.velSens, c.gainDb };
        for (int k = 0; k < kNumKnobs; ++k)
        {
            auto& s = addSlider (names[k], toUi (k, defaultFor (k)), toUi (k, vals[k]));
            s.onChange = [this, k] (float) { if (! updating) knobMoved (k); };
        }
    }
    else if (const auto* t = edm::params::tableFor (c.sound))
    {
        const edm::Sound* lib = edm::params::librarySound (c.sound);
        edm::Sound ref = lib != nullptr ? *lib : c.sound;
        edm::Sound cur = c.sound;
        for (const auto& sp : t->specs)
        {
            if (sp.tab != currentTab - 1) continue;
            const int idx = (int) sliderSpecs.size();
            auto& s = addSlider (sp.label, edm::params::toUi (sp, *sp.field (ref)), edm::params::toUi (sp, *sp.field (cur)));
            s.onChange = [this, idx] (float) { if (! updating) paramMoved (idx); };
            sliderSpecs.push_back (&sp);
        }
    }
    layoutSliders();
}

// One row of vertical sliders; columns never narrower than MAIN's ten
void ZaxEditor::layoutSliders()
{
    if (sliderArea.isEmpty()) return;
    auto area = sliderArea;

    if (currentTab == fxTab)
    {
        auto row = area.removeFromTop (26);
        fxFamilyLbl.setBounds (row.removeFromLeft (150));
        stageBtn.setBounds (row.removeFromRight (64).reduced (2, 1));
        row.removeFromRight (10);
        pageBar.setBounds (row);
        area.removeFromTop (6);
    }

    const bool withFilter = currentTab == 0;
    const int  nSliders   = (int) sliders.size();
    const int  n          = nSliders + (withFilter ? 1 : 0);
    if (n == 0) return;
    const int cols  = juce::jmax (n, (int) kNumKnobs + 1);
    const int cellW = area.getWidth() / cols;
    const int x0    = area.getX() + (area.getWidth() - cellW * n) / 2;
    for (int i = 0; i < nSliders; ++i)
        sliders[(size_t) i]->setBounds (x0 + i * cellW, area.getY(), cellW, area.getHeight());
    if (withFilter)
        filterSlider.setBounds (x0 + nSliders * cellW, area.getY(), cellW, area.getHeight());
}

void ZaxEditor::soundChosen()
{
    const int i = soundBox.getSelectedId() - 1;
    if (i < 0 || i >= (int) soundIndices.size()) return;
    cell().sound = edm::library()[(size_t) soundIndices[(size_t) i]].sound;
    edited();
    refreshEditor();
}

void ZaxEditor::knobMoved (int k)
{
    if (k < 0 || k >= (int) sliders.size()) return;
    auto& c = cell();
    const float v = fromUi (k, sliders[(size_t) k]->getValue());
    switch (k)
    {
        case kTune:    c.macros.tuneSemis = v; break;
        case kDecay:   c.macros.decay     = v; break;
        case kDamp:    c.macros.damp      = v; break;
        case kSnap:    c.macros.snap      = v; break;
        case kColor:   c.macros.color     = v; break;
        case kDrive:   c.macros.drive     = v; break;
        case kHuman:   c.macros.humanize  = v; break;
        case kVelSens: c.macros.velSens   = v; break;
        case kGain:    c.gainDb           = v; break;
        default: break;
    }
    edited();
}

void ZaxEditor::paramMoved (int idx)
{
    if (idx < 0 || idx >= (int) sliderSpecs.size()) return;
    const auto& sp = *sliderSpecs[(size_t) idx];
    *sp.field (cell().sound) = edm::params::fromUi (sp, sliders[(size_t) idx]->getValue());
    edited();
}

void ZaxEditor::showTabInfo (int i)
{
    const auto& snd = cell().sound;
    std::string text;
    if (i == fxTab)       text = kFxInfo;
    else if (i == kitTab) text = kKitInfo;
    else                  text = edm::params::tabInfo (snd, i);
    if (text.empty()) return;
    const auto title = tabBar.getTabName (i) + "  -  " + juce::String (edm::params::voiceKindName (snd.type));
    juce::CallOutBox::launchAsynchronously (std::make_unique<ZaxInfoPanel> (title, juce::String (text)),
                                            getLocalArea (&tabBar, tabBar.getTabArea (i)), this);
}

void ZaxEditor::edited()
{
    keyboard.repaint();
    proc.kitEdited();
    updateKitStatus();
}

//==============================================================================
//  FX: the selected key's family chain
//==============================================================================
void ZaxEditor::refreshPageBar()
{
    auto& fx = proc.kit().fx[(size_t) zax::roleFamily (selectedKey)];
    for (int pg = 0; pg < zax::kNumFxPages; ++pg)
    {
        bool on = false;
        if (bool* st = zax::fxStage (fx, pg)) on = *st;
        else on = fx.pan != 0.0f || fx.levelDb != 0.0f;           // OUT: lit when it moves anything
        pageBar.setStageOn (pg, on);
    }
    pageBar.setSelected (fxPage);
}

void ZaxEditor::buildFxPage()
{
    const int fam = zax::roleFamily (selectedKey);
    auto& fx = proc.kit().fx[(size_t) fam];
    fxFamilyLbl.setText (juce::String (zax::familyName (fam)).toUpperCase() + " CHAIN", juce::dontSendNotification);
    fxFamilyLbl.setColour (juce::Label::textColourId, juce::Colour (zax::familyArgb (fam)).brighter (0.4f));
    refreshPageBar();

    if (bool* on = zax::fxStage (fx, fxPage))
    {
        stageBtn.setButtonText (*on ? "ON" : "OFF");
        stageBtn.setToggleState (*on, juce::dontSendNotification);
    }

    zax::FamilyFx def;
    const auto& specs = zax::fxSpecs();
    for (int i = 0; i < (int) specs.size(); ++i)
    {
        const auto& sp = specs[(size_t) i];
        if (sp.page != fxPage) continue;
        auto s = std::make_unique<zax::GoldSlider> (sp.label, 0.0f, 200.0f, zax::fxToUi (sp, zax::fxRef (def, sp)));
        s->setStep (1.0f);
        if (sp.map == zax::FxMap::Step)                    // the delay's NOTE shows the note value
        {
            const auto spec = sp;
            s->displayFn = [spec] (float v) { return juce::String (zax::delayNoteName ((int) zax::fxFromUi (spec, v))); };
        }
        else
            s->displayFn = [] (float v) { return juce::String ((int) std::lround (v)); };
        s->setValue (zax::fxToUi (sp, zax::fxRef (fx, sp)), false);
        const int idx = (int) fxSliderSpec.size();
        s->onChange = [this, idx] (float) { if (! updating) fxMoved (idx); };
        content.addAndMakeVisible (*s);
        sliders.push_back (std::move (s));
        fxSliderSpec.push_back (i);
    }
}

void ZaxEditor::fxMoved (int idx)
{
    if (idx < 0 || idx >= (int) fxSliderSpec.size() || idx >= (int) sliders.size()) return;
    const auto& sp = zax::fxSpecs()[(size_t) fxSliderSpec[(size_t) idx]];
    auto& fx = proc.kit().fx[(size_t) zax::roleFamily (selectedKey)];
    zax::fxRef (fx, sp) = zax::fxFromUi (sp, sliders[(size_t) idx]->getValue());
    if (bool* on = zax::fxStage (fx, sp.page); on != nullptr && ! *on)
    {
        *on = true;                                      // moving a stage's slider switches it on
        stageBtn.setButtonText ("ON");
        stageBtn.setToggleState (true, juce::dontSendNotification);
    }
    refreshPageBar();
    edited();
}

void ZaxEditor::toggleFxStage()
{
    auto& fx = proc.kit().fx[(size_t) zax::roleFamily (selectedKey)];
    if (bool* on = zax::fxStage (fx, fxPage))
    {
        *on = ! *on;
        stageBtn.setButtonText (*on ? "ON" : "OFF");
        stageBtn.setToggleState (*on, juce::dontSendNotification);
        refreshPageBar();
        edited();
    }
}
