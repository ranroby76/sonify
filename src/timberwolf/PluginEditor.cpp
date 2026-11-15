// D:\workspace\Fanan_web_hub\src\timberwolf\PluginEditor.cpp
#include "PluginEditor.h"
#include "TwPresetFiles.h"

using namespace tw;

namespace
{
    constexpr int W = TimberWolfEditor::designWidth, H = TimberWolfEditor::designHeight;

    // The unit's regions (the 3D case is built to the same, Frames::bodyJson).
    constexpr int screenX = 236, screenTop = 16;
    const int screenH = (int) (H * 0.40f);
    const int deckTop = screenTop + screenH + 12;
    const int deckH = (int) (H * 0.33f);
    const int footTop = deckTop + deckH + 10;

    juce::Rectangle<int> screenArea()  { return { screenX + 8, screenTop + 8, W - screenX - 14 - 16, screenH - 16 }; }
    juce::Rectangle<int> deckArea()    { return { screenX, deckTop, W - screenX - 14, deckH }; }
    juce::Rectangle<int> footArea()    { return { 14, footTop, W - 28, H - footTop - 10 }; }
    juce::Rectangle<int> leftArea()    { return { 12, screenTop, 212, footTop - screenTop - 8 }; }
}

//==============================================================================
class TimberWolfEditor::Lnf final : public juce::LookAndFeel_V4
{
public:
    Lnf()
    {
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1c1e23));
        setColour (juce::TextButton::buttonOnColourId, look::accent.withAlpha (0.85f));
        setColour (juce::TextButton::textColourOffId, look::ink);
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff0d0f13));
        setColour (juce::ComboBox::textColourId, look::ink);
        setColour (juce::ComboBox::outlineColourId, juce::Colours::white.withAlpha (0.08f));
        setColour (juce::ComboBox::arrowColourId, look::accent);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff15171b));
        setColour (juce::PopupMenu::textColourId, look::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, look::accent.withAlpha (0.85f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
        setColour (juce::PopupMenu::headerTextColourId, look::accent);
        setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff0b0d10));
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff15171b));
        setColour (juce::TooltipWindow::textColourId, look::ink);
        setColour (juce::TooltipWindow::outlineColourId, look::accent.withAlpha (0.5f));
        setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff15171b));
        setColour (juce::AlertWindow::textColourId, look::ink);
        setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0b0d10));
        setColour (juce::TextEditor::textColourId, look::ink);
        setColour (juce::TextEditor::outlineColourId, look::accent.withAlpha (0.4f));
        setColour (juce::ScrollBar::thumbColourId, look::accent.withAlpha (0.5f));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();

        // A small raised key: lit amber when on.
        juce::ColourGradient face (on ? look::accent.brighter (0.2f) : juce::Colour (0xff2a2d33).brighter (over ? 0.15f : 0.0f),
                                   r.getCentreX(), r.getY(),
                                   on ? look::accent.darker (0.3f) : juce::Colour (0xff17191d), r.getCentreX(), r.getBottom(), false);
        juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 4, { 0, down ? 0 : 2 }).drawForRectangle (g, r.toNearestInt());
        g.setGradientFill (face);
        g.fillRoundedRectangle (down ? r.reduced (0.5f) : r, 5.0f);
        g.setColour (juce::Colours::white.withAlpha (on ? 0.35f : 0.09f));
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override   { return look::font (juce::jmin (13.0f, h * 0.55f), true); }
    juce::Font getComboBoxFont (juce::ComboBox&) override               { return look::font (12.5f); }
    juce::Font getPopupMenuFont() override                              { return look::font (14.0f); }
};

//==============================================================================
//  The rig list: factory rigs, then the user's
//==============================================================================
class TimberWolfEditor::PresetList final : public juce::Component,
                                           private juce::ListBoxModel
{
public:
    explicit PresetList (TimberWolfProcessor& p) : proc (p)
    {
        list.setModel (this);
        list.setRowHeight (21);
        list.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff0b0d10));
        addAndMakeVisible (list);
        refresh();
    }

    void refresh()
    {
        rows.clear();
        rows.push_back ({ Row::Kind::header, "FACTORY", -1, {} });

        for (int i = 0; i < proc.numFactoryPresets(); ++i)
            rows.push_back ({ Row::Kind::factory, proc.factoryPresetName (i), i, {} });

        rows.push_back ({ Row::Kind::header, "MY RIGS", -1, {} });

        const auto users = presetfiles::userPresets();

        for (const auto& f : users)
            rows.push_back ({ Row::Kind::user, f.getFileNameWithoutExtension(), -1, f });

        if (users.isEmpty())
            rows.push_back ({ Row::Kind::header, "  (none saved yet)", -1, {} });

        list.updateContent();
        list.repaint();
    }

    void resized() override   { list.setBounds (getLocalBounds()); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xff0b0d10));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    }

private:
    struct Row
    {
        enum class Kind { header, factory, user };
        Kind kind;
        juce::String name;
        int factory;
        juce::File file;
    };

    int getNumRows() override   { return (int) rows.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool) override
    {
        if (row < 0 || row >= (int) rows.size())
            return;

        const auto& r = rows[(size_t) row];

        if (r.kind == Row::Kind::header)
        {
            g.setColour (r.name.startsWith (" ") ? look::inkFaint : look::accent.withAlpha (0.9f));
            g.setFont (look::font (r.name.startsWith (" ") ? 11.5f : 10.5f, true));
            g.drawText (r.name, 8, 0, w - 10, h, juce::Justification::centredLeft);
            return;
        }

        const bool current = (r.kind == Row::Kind::factory && proc.getFactoryIndex() == r.factory)
                          || (r.kind == Row::Kind::user && proc.getPresetFile() == r.file);

        if (current)
        {
            g.setColour (look::accent.withAlpha (0.16f));
            g.fillRoundedRectangle (2.0f, 1.0f, (float) w - 4.0f, (float) h - 2.0f, 4.0f);
            g.setColour (look::accent);
            g.fillRoundedRectangle (2.0f, 4.0f, 3.0f, (float) h - 8.0f, 1.5f);
        }

        g.setColour (current ? look::ink : look::inkDim);
        g.setFont (look::font (13.0f, current));
        g.drawText (r.name + (current && proc.isModified() ? " *" : ""), 14, 0, w - 18, h, juce::Justification::centredLeft, true);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        if (row < 0 || row >= (int) rows.size())
            return;

        const auto& r = rows[(size_t) row];

        if (r.kind == Row::Kind::factory)    proc.loadFactoryPreset (r.factory);
        else if (r.kind == Row::Kind::user)  proc.loadPresetFile (r.file);

        list.repaint();
    }

    TimberWolfProcessor& proc;
    juce::ListBox list;
    std::vector<Row> rows;
};

//==============================================================================
//  The deck: the effect being edited
//==============================================================================
class TimberWolfEditor::Panel final : public juce::Component
{
public:
    explicit Panel (TimberWolfProcessor& p) : proc (p)
    {
        power.setClickingTogglesState (false);
        power.onClick = [this] { if (uid != 0 && proc.infoOf (uid) != nullptr) { proc.setOn (uid, ! proc.isOn (uid)); repaint(); } };
        addChildComponent (power);
    }

    void show (int newUid)
    {
        uid = newUid;
        controls.clear();
        removeAllChildren();
        addChildComponent (power);

        if (const auto* info = proc.infoOf (uid))
        {
            power.setVisible (true);

            for (int k = 0; k < (int) info->knobs.size() && k < maxKnobs; ++k)
            {
                const auto spec = info->knobs[(size_t) k];
                Binding b;
                const int u = uid;
                b.name = spec.name;
                b.steps = spec.isChoice() ? spec.choices.size() : 0;
                b.get = [this, u, k] { return proc.knobValue (u, k); };
                b.set = [this, u, k] (float v) { proc.setKnobValue (u, k, v); };
                b.begin = [this, u, k] { proc.beginKnobGesture (u, k); };
                b.end = [this, u, k] { proc.endKnobGesture (u, k); };
                b.text = [this, u, k] { return proc.knobText (u, k); };
                b.reset = [this, u, k] { proc.resetKnob (u, k); };
                addControl (std::move (b), spec.isChoice() && spec.choices.size() == 2);
            }
        }
        else if (const auto* node = proc.graph().node (uid); node != nullptr && node->kind == Node::Kind::parallel)
        {
            power.setVisible (false);
            const auto& split = registry::splitInfo();
            const auto& blend = registry::blendInfo();

            const auto spec = [&] (int i) -> const KnobSpec& { return i < 2 ? split.knobs[(size_t) i] : blend.knobs[(size_t) (i - 2)]; };

            for (int i = 0; i < 7; ++i)
            {
                const auto ks = spec (i);
                Binding b;
                const int u = uid;
                b.name = ks.name;
                b.steps = ks.isChoice() ? ks.choices.size() : 0;
                b.get = [this, u, i, ks] { return ks.toNorm (settingOf (u, i)); };
                b.set = [this, u, i, ks] (float v) { setSetting (u, i, ks.toPlain (v)); };
                b.text = [this, u, i, ks] { return ks.text (settingOf (u, i)); };
                b.reset = [this, u, i, ks] { setSetting (u, i, ks.def); };
                addControl (std::move (b), ks.isChoice() && ks.choices.size() == 2);
            }
        }

        resized();
        repaint();
    }

    int shown() const noexcept   { return uid; }

    void refreshValues()
    {
        for (auto& c : controls)
            c->repaint();

        power.setToggleState (uid != 0 && proc.isOn (uid), juce::dontSendNotification);
        repaint (header());
    }

    void resized() override
    {
        power.setBounds (getWidth() - 96, 12, 80, 26);

        auto area = getLocalBounds().reduced (14, 8);
        area.removeFromTop (60);
        const int n = (int) controls.size();

        if (n == 0)
            return;

        const int perRow = n <= 7 ? n : (n + 1) / 2;
        const int rowsN = n <= 7 ? 1 : 2;
        const int cw = juce::jmin (118, area.getWidth() / juce::jmax (1, perRow));
        const int ch = juce::jmin (rowsN == 1 ? 150 : 104, area.getHeight() / rowsN);
        const int totalW = cw * perRow;

        for (int i = 0; i < n; ++i)
        {
            const int row = i / perRow, col = i % perRow;
            const int inRow = juce::jmin (perRow, n - row * perRow);
            const int x0 = area.getX() + (area.getWidth() - (rowsN == 1 ? totalW : cw * inRow)) / 2;
            controls[(size_t) i]->setBounds (x0 + col * cw, area.getY() + row * ch, cw, ch);
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto h = header();

        if (const auto* info = proc.infoOf (uid))
        {
            const bool on = proc.isOn (uid);
            look::drawBadge (g, info->family, info->icon, juce::Rectangle<float> (38.0f, 38.0f).withPosition ((float) h.getX() + 6.0f, (float) h.getY() + 6.0f), on, false, 0.7f);

            g.setColour (look::ink);
            g.setFont (look::font (20.0f, true));
            g.drawText (info->name, h.getX() + 64, h.getY() + 2, 400, 24, juce::Justification::centredLeft);
            g.setColour (look::familyColour (info->family));
            g.setFont (look::font (11.0f, true));
            g.drawText (juce::String (familyName (info->family)).toUpperCase(), h.getX() + 64, h.getY() + 26, 200, 14, juce::Justification::centredLeft);
            g.setColour (look::inkDim);
            g.setFont (look::font (12.5f));
            g.drawFittedText (info->description, h.getX() + 64, h.getY() + 40, getWidth() - 280, 16, juce::Justification::centredLeft, 1);

            const float m = proc.meterOf (uid);

            if (m >= 0.0f)
            {
                auto bar = juce::Rectangle<float> (120.0f, 6.0f).withPosition ((float) getWidth() - 236.0f, 24.0f);
                g.setColour (juce::Colour (0xff0a0c10));
                g.fillRoundedRectangle (bar, 3.0f);
                g.setColour (look::accent);
                g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, m)), 3.0f);
                g.setColour (look::inkFaint);
                g.setFont (look::font (10.0f));
                g.drawText ("ACTIVITY", bar.translated (0.0f, -14.0f).toNearestInt(), juce::Justification::centredLeft);
            }

            g.setColour (look::inkFaint);
            g.setFont (look::font (10.0f));
            g.drawText (info->credit, getLocalBounds().removeFromBottom (14).reduced (14, 0), juce::Justification::centredRight);
        }
        else if (const auto* node = proc.graph().node (uid); node != nullptr && node->kind == Node::Kind::parallel)
        {
            look::drawBadge (g, Family::utility, registry::splitInfo().icon, juce::Rectangle<float> (34.0f, 34.0f).withPosition ((float) h.getX() + 8.0f, (float) h.getY() + 8.0f), true);
            g.setColour (look::ink);
            g.setFont (look::font (20.0f, true));
            g.drawText ("Split / Blend", h.getX() + 64, h.getY() + 2, 400, 24, juce::Justification::centredLeft);
            g.setColour (look::inkDim);
            g.setFont (look::font (12.5f));
            g.drawText ("Where the signal splits into paths A and B, and where they meet again.", h.getX() + 64, h.getY() + 30, getWidth() - 200, 16,
                        juce::Justification::centredLeft);
        }
        else
        {
            g.setColour (look::inkDim);
            g.setFont (look::font (15.0f));
            g.drawText ("Double-click an effect on the screen to edit it here", getLocalBounds(), juce::Justification::centred);
        }
    }

    juce::TextButton power { "ON" };

private:
    juce::Rectangle<int> header() const   { return getLocalBounds().reduced (12, 6).removeFromTop (60); }

    void addControl (Binding b, bool lever)
    {
        std::unique_ptr<juce::Component> c;

        if (lever) c = std::make_unique<LeverView> (std::move (b));
        else       c = std::make_unique<KnobView> (std::move (b));

        addAndMakeVisible (*c);
        controls.push_back (std::move (c));
    }

    float settingOf (int u, int i) const
    {
        const auto* n = proc.graph().node (u);

        if (n == nullptr)
            return 0.0f;

        const auto& s = n->settings;

        switch (i)
        {
            case 0:  return (float) s.splitType;
            case 1:  return s.crossoverHz;
            case 2:  return s.levelA;
            case 3:  return s.levelB;
            case 4:  return s.panA;
            case 5:  return s.panB;
            default: return s.invertB ? 1.0f : 0.0f;
        }
    }

    void setSetting (int u, int i, float v)
    {
        const auto* n = proc.graph().node (u);

        if (n == nullptr)
            return;

        auto s = n->settings;

        switch (i)
        {
            case 0:  s.splitType = juce::roundToInt (v); break;
            case 1:  s.crossoverHz = v; break;
            case 2:  s.levelA = v; break;
            case 3:  s.levelB = v; break;
            case 4:  s.panA = v; break;
            case 5:  s.panB = v; break;
            default: s.invertB = v > 0.5f; break;
        }

        proc.setParallel (u, s);
        refreshValues();
    }

    TimberWolfProcessor& proc;
    int uid = 0;
    std::vector<std::unique_ptr<juce::Component>> controls;
};

//==============================================================================
//  Everything in the window, at its design size (the editor scales it)
//==============================================================================
class TimberWolfEditor::Content final : public juce::Component
{
public:
    Content (TimberWolfEditor& o, TimberWolfProcessor& p)
        : owner (o), proc (p), chain (p), panel (p), presets (p)
    {
        addAndMakeVisible (chain);
        addAndMakeVisible (panel);
        addAndMakeVisible (presets);

        chain.onOpen = [this] (int uid) { owner.open (uid); };

        for (auto* b : { &save, &saveAs, &load, &folder, &saveBankButton, &loadBankButton, &tapButton, &tuner, &prev, &next })
            addAndMakeVisible (*b);

        save.onClick = [this] { owner.savePreset (false); };
        saveAs.onClick = [this] { owner.savePreset (true); };
        load.onClick = [this] { owner.loadPresetFromFile(); };
        folder.onClick = [] { presetfiles::showFolder(); };
        saveBankButton.onClick = [this] { owner.saveBank(); };
        loadBankButton.onClick = [this] { owner.loadBank(); };
        prev.onClick = [this] { proc.stepPreset (-1); };
        next.onClick = [this] { proc.stepPreset (1); };
        tapButton.onClick = [this] { proc.tap(); repaint(); };
        tuner.setClickingTogglesState (true);
        tuner.onClick = [this] { proc.setTuning (tuner.getToggleState()); repaint(); };

        tapButton.setTooltip ("Tap the tempo (the delays and modulations that sync follow it)");
        tuner.setTooltip ("The tuner (the output is muted while it's on)");

        for (int i = 0; i < numSnapshots; ++i)
        {
            auto b = std::make_unique<juce::TextButton> (juce::String (i + 1));
            b->setClickingTogglesState (false);
            b->onClick = [this, i]
            {
                if (juce::ModifierKeys::currentModifiers.isPopupMenu())
                    return;

                if (proc.snapshot (i).stored) proc.recallSnapshot (i);
                else                          proc.storeSnapshot (i);

                owner.valuesChanged();
            };
            b->setTooltip ("Snapshot " + juce::String (i + 1) + ": click to recall (an empty one stores the rig as it is), right-click for more");
            b->addMouseListener (this, false);
            addAndMakeVisible (*b);
            snaps.push_back (std::move (b));
        }

        auto makeGlobal = [this] (juce::AudioParameterFloat& param, const juce::String& name)
        {
            Binding b;
            auto* prm = &param;
            b.name = name;
            b.get = [prm] { return prm->convertTo0to1 (prm->get()); };
            b.set = [prm] (float v) { prm->setValueNotifyingHost (v); };
            b.begin = [prm] { prm->beginChangeGesture(); };
            b.end = [prm] { prm->endChangeGesture(); };
            b.text = [prm] { return juce::String (prm->get(), 1) + " dB"; };
            b.reset = [prm] { prm->setValueNotifyingHost (prm->convertTo0to1 (0.0f)); };
            auto k = std::make_unique<KnobView> (std::move (b));
            addAndMakeVisible (*k);
            return k;
        };

        inputKnob = makeGlobal (proc.inputGain(), "Input");
        outputKnob = makeGlobal (proc.outputGain(), "Output");

        size.addItemList ({ "60 %", "65 %", "70 %", "75 %", "80 %", "85 %", "90 %", "95 %", "100 %", "110 %", "125 %" }, 1);
        size.onChange = [this]
        {
            static const float scales[] { 0.6f, 0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.0f, 1.1f, 1.25f };
            proc.setUiScale (scales[juce::jlimit (0, 10, size.getSelectedItemIndex())]);
            owner.applyScale();
        };
        size.setTooltip ("The window's size");
        addAndMakeVisible (size);

        for (int i = 0; i < numSwitches; ++i)
        {
            auto f = std::make_unique<FootswitchView>();
            f->number = juce::String::charToString ((juce::juce_wchar) ('A' + i));
            f->onPress = [this, i] { proc.pressFootswitch (i); owner.valuesChanged(); };
            f->onMenu = [this, i] { owner.footswitchMenu (i); };
            f->onLearn = [this, i] { proc.learn (i); };
            addAndMakeVisible (*f);
            switches.push_back (std::move (f));
        }

        for (int i = 0; i < numPedals; ++i)
        {
            auto p2 = std::make_unique<PedalView>();
            p2->label = "EXP " + juce::String (i + 1);
            p2->get = [this, i] { return proc.pedalPosition (i); };
            p2->set = [this, i] (float v) { proc.setPedalPosition (i, v); };
            p2->onMenu = [this, i] { owner.pedalMenu (i); };
            p2->onLearn = [this, i] { proc.learn (numSwitches + i); };
            addAndMakeVisible (*p2);
            pedals.push_back (std::move (p2));
        }

        setSize (W, H);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // The tempo display: a click goes between the song's tempo and the tapped one.
        if (e.eventComponent == this && tempoArea.contains (e.getPosition()))
        {
            proc.setUseTapTempo (! proc.usesTapTempo());
            repaint (tempoArea);
            return;
        }

        for (int i = 0; i < (int) snaps.size(); ++i)
            if (e.eventComponent == snaps[(size_t) i].get() && e.mods.isPopupMenu())
                owner.snapshotMenu (i);
    }

    void resized() override
    {
        chain.setBounds (screenArea());
        panel.setBounds (deckArea());

        auto left = leftArea().reduced (10, 8);
        left.removeFromTop (58);   // the name

        auto nav = left.removeFromTop (28);
        prev.setBounds (nav.removeFromLeft (28));
        next.setBounds (nav.removeFromRight (28));
        presetNameArea = nav.reduced (4, 0);
        left.removeFromTop (6);

        auto bottom = left.removeFromBottom (256);
        presets.setBounds (left);

        bottom.removeFromTop (6);
        auto row1 = bottom.removeFromTop (26);
        const int bw = row1.getWidth() / 3;
        save.setBounds (row1.removeFromLeft (bw).reduced (2, 0));
        saveAs.setBounds (row1.removeFromLeft (bw).reduced (2, 0));
        load.setBounds (row1.reduced (2, 0));
        bottom.removeFromTop (4);
        auto row2 = bottom.removeFromTop (26);
        folder.setBounds (row2.removeFromLeft (bw).reduced (2, 0));
        saveBankButton.setBounds (row2.removeFromLeft (bw).reduced (2, 0));
        loadBankButton.setBounds (row2.reduced (2, 0));

        bottom.removeFromTop (18);
        auto snapRow = bottom.removeFromTop (26);
        const int sw = snapRow.getWidth() / numSnapshots;
        for (auto& s : snaps)
            s->setBounds (snapRow.removeFromLeft (sw).reduced (2, 0));

        bottom.removeFromTop (8);
        auto tempoRow = bottom.removeFromTop (26);
        tempoArea = tempoRow.removeFromLeft (70);
        tapButton.setBounds (tempoRow.removeFromLeft (56).reduced (2, 0));
        tuner.setBounds (tempoRow.reduced (2, 0));

        bottom.removeFromTop (4);
        auto knobs = bottom.removeFromTop (88);
        inputKnob->setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
        outputKnob->setBounds (knobs);
        size.setBounds (bottom.removeFromTop (22).reduced (2, 0));

        // The foot row: eight switches, then the two pedals.
        auto foot = footArea().reduced (14, 6);
        auto pedalArea = foot.removeFromRight (190);
        const int fw = foot.getWidth() / numSwitches;

        for (auto& f : switches)
            f->setBounds (foot.removeFromLeft (fw).reduced (4, 0));

        const int pw = pedalArea.getWidth() / numPedals;
        for (auto& p2 : pedals)
            p2->setBounds (pedalArea.removeFromLeft (pw).reduced (6, 0));
    }

    void paint (juce::Graphics& g) override
    {
        const float scale = juce::jmax (1.0f, (float) g.getInternalContext().getPhysicalPixelScaleFactor());
        const auto* body = Frames::get().find (Frames::Kind::body, W, H, juce::jmin (2.0f, scale), this);

        if (body != nullptr && ! body->empty())
        {
            g.setOpacity (1.0f);
            g.drawImage (body->front(), getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
        }
        else
        {
            // Until the case is rendered: its regions, flat.
            g.fillAll (juce::Colour (0xff18191d));
            g.setColour (juce::Colour (0xff202227));
            g.fillRoundedRectangle (leftArea().toFloat(), 10.0f);
            g.setColour (look::screenBg);
            g.fillRoundedRectangle (screenArea().expanded (8).toFloat(), 12.0f);
            g.setColour (juce::Colour (0xff33363d));
            g.fillRoundedRectangle (deckArea().toFloat(), 10.0f);
            g.setColour (juce::Colour (0xff121316));
            g.fillRoundedRectangle (footArea().toFloat(), 10.0f);
        }

        // The name.
        auto name = leftArea().reduced (14, 10).removeFromTop (52);
        g.setColour (look::ink);
        g.setFont (look::font (25.0f, true).withExtraKerningFactor (0.12f));
        g.drawText ("TIMBER", name.removeFromTop (24), juce::Justification::centredLeft);
        g.setColour (look::accent);
        g.drawText ("WOLF", name.removeFromTop (24), juce::Justification::centredLeft);
        g.setColour (look::inkFaint);
        g.setFont (look::font (9.0f, true));
        auto tag = leftArea().reduced (14, 10).removeFromTop (52).removeFromBottom (24).withTrimmedLeft (84);
        g.drawFittedText ("GUITAR\nMULTI-FX", tag, juce::Justification::centredRight, 2);

        // The rig's name between the arrows.
        g.setColour (juce::Colour (0xff0b0d10));
        g.fillRoundedRectangle (presetNameArea.toFloat(), 5.0f);
        g.setColour (look::ink);
        g.setFont (look::font (14.0f, true));
        g.drawFittedText (proc.getPresetName() + (proc.isModified() ? " *" : ""), presetNameArea.reduced (6, 0), juce::Justification::centred, 1);

        // Section labels.
        g.setColour (look::inkFaint);
        g.setFont (look::font (10.0f, true));
        if (! snaps.empty())
            g.drawText ("SNAPSHOTS", snaps.front()->getBounds().translated (0, -16).withHeight (14).withWidth (180), juce::Justification::centredLeft);

        // The tempo.
        g.setColour (juce::Colour (0xff0b0d10));
        g.fillRoundedRectangle (tempoArea.toFloat().reduced (1.0f), 4.0f);
        g.setColour (look::accent);
        g.setFont (look::font (13.0f, true));
        g.drawText (juce::String (proc.currentBpm(), 1), tempoArea.withTrimmedBottom (9), juce::Justification::centred);
        g.setColour (look::inkFaint);
        g.setFont (look::font (8.5f, true));
        g.drawText (proc.usesTapTempo() ? "TAP BPM" : "SONG BPM", tempoArea.withTrimmedTop (15), juce::Justification::centred);

        if (tempoArea.contains (getMouseXYRelative()))
        {
            g.setColour (look::accent.withAlpha (0.5f));
            g.drawRoundedRectangle (tempoArea.toFloat().reduced (1.0f), 4.0f, 1.0f);
        }

        // The meters, by the screen's edge.
        paintMeter (g, juce::Rectangle<int> (screenX - 8, screenTop + 12, 4, screenH - 24), proc.inputLevel());
        paintMeter (g, juce::Rectangle<int> (W - 12, screenTop + 12, 4, screenH - 24), proc.outputLevel());
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        if (! proc.isTuning())
            return;

        // The tuner, over the screen.
        const auto area = screenArea().toFloat();
        g.setColour (look::screenBg.withAlpha (0.94f));
        g.fillRoundedRectangle (area, 8.0f);

        juce::String note;
        float cents = 0.0f;
        const float hz = proc.tuner().lastHz();
        Tuner::note (hz, 440.0f, note, cents);
        const bool inTune = hz > 0.0f && std::abs (cents) < 3.0f;

        g.setColour (hz > 0.0f ? (inTune ? look::ledOn : look::ink) : look::inkFaint);
        g.setFont (look::font (88.0f, true));
        g.drawText (note, area.withTrimmedBottom (70).toNearestInt(), juce::Justification::centred);

        auto scaleBar = area.withSizeKeepingCentre (area.getWidth() * 0.7f, 10.0f).translated (0.0f, area.getHeight() * 0.26f);
        g.setColour (juce::Colour (0xff1a2230));
        g.fillRoundedRectangle (scaleBar, 5.0f);

        for (int c = -50; c <= 50; c += 10)
        {
            const float x = scaleBar.getX() + scaleBar.getWidth() * (float) (c + 50) / 100.0f;
            g.setColour (c == 0 ? look::ledOn : look::inkFaint);
            g.fillRect (x - 1.0f, scaleBar.getY() - (c == 0 ? 10.0f : 5.0f), 2.0f, c == 0 ? 30.0f : 20.0f);
        }

        if (hz > 0.0f)
        {
            const float x = scaleBar.getX() + scaleBar.getWidth() * (juce::jlimit (-50.0f, 50.0f, cents) + 50.0f) / 100.0f;
            g.setColour (inTune ? look::ledOn : look::accent);
            g.fillRoundedRectangle (x - 4.0f, scaleBar.getY() - 14.0f, 8.0f, 38.0f, 3.0f);
            g.setColour (look::inkDim);
            g.setFont (look::font (14.0f));
            g.drawText (juce::String (hz, 1) + " Hz   " + (cents >= 0.0f ? "+" : "") + juce::String (cents, 1) + " cents",
                        scaleBar.translated (0.0f, 30.0f).withHeight (20.0f).toNearestInt(), juce::Justification::centred);
        }
        else
        {
            g.setColour (look::inkFaint);
            g.setFont (look::font (14.0f));
            g.drawText ("Play a string", scaleBar.translated (0.0f, 30.0f).withHeight (20.0f).toNearestInt(), juce::Justification::centred);
        }

        g.setColour (look::inkFaint);
        g.setFont (look::font (11.0f, true));
        g.drawText ("TUNER  A = 440 Hz  (output muted)", area.reduced (14.0f, 10.0f).toNearestInt(), juce::Justification::topLeft);
    }

    void paintMeter (juce::Graphics& g, juce::Rectangle<int> r, float level) const
    {
        g.setColour (juce::Colour (0xff0a0c10));
        g.fillRoundedRectangle (r.toFloat(), 2.0f);
        const float db = juce::jlimit (-60.0f, 6.0f, gainToDb (level));
        const float f = (db + 60.0f) / 66.0f;
        const auto lit = r.toFloat().withTop ((float) r.getBottom() - (float) r.getHeight() * f);
        g.setColour (db > -1.0f ? juce::Colour (0xffff4a3a) : (db > -12.0f ? look::accent : look::ledOn.withMultipliedBrightness (0.85f)));
        g.fillRoundedRectangle (lit, 2.0f);
    }

    void refreshSwitches()
    {
        for (int i = 0; i < numSwitches; ++i)
        {
            auto& f = *switches[(size_t) i];
            const bool lit = proc.isFootswitchLit (i);
            const auto label = proc.footswitchLabel (i);
            const auto midi = proc.learned (i).text();
            const bool learning = proc.learning() == i;

            if (lit != f.lit || label != f.label || midi != f.midi || learning != f.learning)
            {
                f.lit = lit; f.label = label; f.midi = midi; f.learning = learning;

                const auto& sw = proc.footswitch (i);
                f.light = sw.job == Footswitch::Job::snapshot ? juce::Colour (0xff4fa8ff)
                        : (sw.job == Footswitch::Job::tuner || sw.job == Footswitch::Job::tap ? look::accent : look::ledOn);
                f.repaint();
            }
        }

        for (int i = 0; i < numPedals; ++i)
        {
            auto& p2 = *pedals[(size_t) i];
            const auto& pd = proc.pedal (i);
            juce::String target = "(nothing)";

            if (const auto* info = proc.infoOf (pd.uid); info != nullptr && pd.knob < (int) info->knobs.size())
                target = info->name + " " + info->knobs[(size_t) pd.knob].name;

            p2.target = target;
            p2.midi = proc.learned (numSwitches + i).text();
            p2.learning = proc.learning() == numSwitches + i;
            p2.repaint();
        }

        for (int i = 0; i < (int) snaps.size(); ++i)
        {
            snaps[(size_t) i]->setToggleState (proc.currentSnapshot() == i, juce::dontSendNotification);
            snaps[(size_t) i]->setButtonText (proc.snapshot (i).stored ? (proc.snapshot (i).name.isNotEmpty() ? proc.snapshot (i).name.substring (0, 5)
                                                                                                                 : juce::String (i + 1))
                                                                       : "+");
        }

        tuner.setToggleState (proc.isTuning(), juce::dontSendNotification);
    }

    TimberWolfEditor& owner;
    TimberWolfProcessor& proc;
    ChainView chain;
    Panel panel;
    PresetList presets;

    juce::TextButton save { "Save" }, saveAs { "Save As" }, load { "Load" }, folder { "Folder" },
                     saveBankButton { "Bank >" }, loadBankButton { "< Bank" }, tapButton { "TAP" },
                     tuner { "TUNER" }, prev { "<" }, next { ">" };
    std::vector<std::unique_ptr<juce::TextButton>> snaps;
    std::unique_ptr<KnobView> inputKnob, outputKnob;
    juce::ComboBox size;
    std::vector<std::unique_ptr<FootswitchView>> switches;
    std::vector<std::unique_ptr<PedalView>> pedals;
    juce::Rectangle<int> presetNameArea, tempoArea;
};

//==============================================================================
TimberWolfEditor::TimberWolfEditor (TimberWolfProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    lnf = std::make_unique<Lnf>();
    setLookAndFeel (lnf.get());

    content = std::make_unique<Content> (*this, p);
    addAndMakeVisible (*content);

    content->saveBankButton.setTooltip ("Save all your rigs into one bank file");
    content->loadBankButton.setTooltip ("Load a bank: its rigs join yours");

    proc.onChainChanged = [this] { chainChanged(); };
    proc.onValuesChanged = [this] { valuesChanged(); };

    static const float scales[] { 0.6f, 0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.0f, 1.1f, 1.25f };
    int nearest = 0;
    for (int i = 0; i < 11; ++i)
        if (std::abs (scales[i] - proc.getUiScale()) < std::abs (scales[nearest] - proc.getUiScale()))
            nearest = i;
    content->size.setSelectedItemIndex (nearest, juce::dontSendNotification);

    // The first effect of the rig on the deck.
    const auto order = proc.graph().blockOrder();
    open (order.empty() ? 0 : order.front());

    applyScale();
    startTimerHz (20);
}

TimberWolfEditor::~TimberWolfEditor()
{
    proc.onChainChanged = nullptr;
    proc.onValuesChanged = nullptr;
    stopTimer();
    setLookAndFeel (nullptr);
}

void TimberWolfEditor::applyScale()
{
    const float s = proc.getUiScale();
    content->setTransform (juce::AffineTransform::scale (s));
    setSize (juce::roundToInt (W * s), juce::roundToInt (H * s));
}

void TimberWolfEditor::paint (juce::Graphics& g)   { g.fillAll (juce::Colour (0xff0b0c0e)); }
void TimberWolfEditor::resized()                   { content->setBounds (0, 0, W, H); }

void TimberWolfEditor::open (int uid)
{
    content->chain.setSelected (uid);
    content->panel.show (uid);
}

void TimberWolfEditor::chainChanged()
{
    content->chain.refresh();
    content->presets.refresh();

    // The effect on the deck may be gone (another rig, or removed).
    const int shown = content->panel.shown();
    const bool still = proc.infoOf (shown) != nullptr
                    || (proc.graph().node (shown) != nullptr && proc.graph().node (shown)->kind == Node::Kind::parallel);

    if (! still)
    {
        const auto order = proc.graph().blockOrder();
        open (order.empty() ? 0 : order.front());
    }
    else
    {
        content->panel.show (shown);
    }

    content->refreshSwitches();
    content->repaint();
}

void TimberWolfEditor::valuesChanged()
{
    content->panel.refreshValues();
    content->chain.repaint();
    content->refreshSwitches();
    content->repaint (leftArea());
}

void TimberWolfEditor::timerCallback()
{
    content->refreshSwitches();
    content->panel.refreshValues();
    content->repaint (screenX - 10, screenTop, 8, screenH);
    content->repaint (W - 14, screenTop, 8, screenH);

    static int ticks = 0;
    if (++ticks % 10 == 0)
    {
        content->chain.repaint();        // effects switched by automation or a foot controller
        content->repaint (leftArea());   // the tempo, the rig's name
    }

    if (proc.isTuning())
        content->repaint (screenArea());
}

//==============================================================================
//  Menus
//==============================================================================
void TimberWolfEditor::footswitchMenu (int i)
{
    juce::PopupMenu menu;
    const auto f = proc.footswitch (i);
    auto* self = this;

    const auto setJob = [self, i] (Footswitch::Job job, std::vector<int> uids = {}, int snapshot = 0)
    {
        auto now = self->proc.footswitch (i);
        now.job = job;
        now.uids = std::move (uids);
        now.snapshot = snapshot;
        self->proc.setFootswitch (i, now);
        self->valuesChanged();
    };

    menu.addSectionHeader ("Footswitch " + juce::String::charToString ((juce::juce_wchar) ('A' + i)));

    juce::PopupMenu toggle;
    for (const int uid : proc.graph().blockOrder())
    {
        const auto* info = proc.infoOf (uid);
        if (info == nullptr) continue;

        const bool in = f.job == Footswitch::Job::toggle && std::find (f.uids.begin(), f.uids.end(), uid) != f.uids.end();
        toggle.addItem (info->name, true, in, [self, i, uid, setJob]
        {
            auto now = self->proc.footswitch (i);
            auto uids = now.job == Footswitch::Job::toggle ? now.uids : std::vector<int>();
            auto it = std::find (uids.begin(), uids.end(), uid);
            if (it != uids.end()) uids.erase (it); else uids.push_back (uid);
            setJob (uids.empty() ? Footswitch::Job::none : Footswitch::Job::toggle, uids);
        });
    }
    menu.addSubMenu ("Switch effects on / off (tick several for one switch)", toggle);

    juce::PopupMenu snap;
    for (int s = 0; s < numSnapshots; ++s)
        snap.addItem ("Snapshot " + juce::String (s + 1) + (proc.snapshot (s).name.isNotEmpty() ? " (" + proc.snapshot (s).name + ")" : juce::String()),
                      true, f.job == Footswitch::Job::snapshot && f.snapshot == s, [setJob, s] { setJob (Footswitch::Job::snapshot, {}, s); });
    menu.addSubMenu ("Recall a snapshot", snap);

    menu.addItem ("Next rig", true, f.job == Footswitch::Job::presetUp, [setJob] { setJob (Footswitch::Job::presetUp); });
    menu.addItem ("Previous rig", true, f.job == Footswitch::Job::presetDown, [setJob] { setJob (Footswitch::Job::presetDown); });
    menu.addItem ("Tap tempo", true, f.job == Footswitch::Job::tap, [setJob] { setJob (Footswitch::Job::tap); });
    menu.addItem ("Tuner", true, f.job == Footswitch::Job::tuner, [setJob] { setJob (Footswitch::Job::tuner); });
    menu.addItem ("Nothing", true, f.job == Footswitch::Job::none, [setJob] { setJob (Footswitch::Job::none); });
    menu.addSeparator();

    menu.addItem ("Rename...", [self, i]
    {
        auto* w = new juce::AlertWindow ("Footswitch label", "What its strip says (empty: made from its job).", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("label", self->proc.footswitch (i).label);
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([self, i, w] (int r)
        {
            if (r == 1)
            {
                auto now = self->proc.footswitch (i);
                now.label = w->getTextEditorContents ("label").trim().substring (0, 24);
                self->proc.setFootswitch (i, now);
                self->valuesChanged();
            }
        }), true);
    });

    const auto learnedNow = proc.learned (i);
    menu.addItem ("MIDI learn (or right double-click it)", [self, i] { self->proc.learn (i); });
    menu.addItem ("Forget its MIDI (" + learnedNow.text() + ")", learnedNow.kind != Learned::Kind::none, false, [self, i] { self->proc.forget (i); self->valuesChanged(); });

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (content->switches[(size_t) i].get()));
}

void TimberWolfEditor::pedalMenu (int i)
{
    juce::PopupMenu menu;
    const auto pd = proc.pedal (i);
    auto* self = this;

    menu.addSectionHeader ("EXP " + juce::String (i + 1));

    juce::PopupMenu target;
    for (const int uid : proc.graph().blockOrder())
    {
        const auto* info = proc.infoOf (uid);
        if (info == nullptr) continue;

        juce::PopupMenu knobs;
        for (int k = 0; k < (int) info->knobs.size(); ++k)
            knobs.addItem (info->knobs[(size_t) k].name, true, pd.uid == uid && pd.knob == k, [self, i, uid, k]
            {
                auto now = self->proc.pedal (i);
                now.uid = uid;
                now.knob = k;
                self->proc.setPedal (i, now);
                self->valuesChanged();
            });

        target.addSubMenu (info->name, knobs, true, nullptr, pd.uid == uid);
    }
    menu.addSubMenu ("Moves", target);

    juce::PopupMenu range;
    const auto setRange = [self, i] (float from, float to)
    {
        auto now = self->proc.pedal (i);
        now.from = from;
        now.to = to;
        self->proc.setPedal (i, now);
    };
    range.addItem ("All the way (0 - 100 %)", [setRange] { setRange (0.0f, 1.0f); });
    range.addItem ("Reversed (100 - 0 %)", [setRange] { setRange (1.0f, 0.0f); });
    range.addItem ("Upper half (50 - 100 %)", [setRange] { setRange (0.5f, 1.0f); });
    range.addItem ("Lower half (0 - 50 %)", [setRange] { setRange (0.0f, 0.5f); });
    range.addItem ("From where the knob is now, up", [self, i, setRange]
    {
        const auto now = self->proc.pedal (i);
        setRange (self->proc.knobValue (now.uid, now.knob), 1.0f);
    });
    menu.addSubMenu ("Range", range, pd.uid != 0);

    menu.addItem ("Moves nothing", pd.uid != 0, false, [self, i] { self->proc.setPedal (i, {}); self->valuesChanged(); });
    menu.addSeparator();
    const auto learnedNow = proc.learned (numSwitches + i);
    menu.addItem ("MIDI learn (or right double-click it)", [self, i] { self->proc.learn (numSwitches + i); });
    menu.addItem ("Forget its MIDI (" + learnedNow.text() + ")", learnedNow.kind != Learned::Kind::none, false,
                  [self, i] { self->proc.forget (numSwitches + i); self->valuesChanged(); });

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (content->pedals[(size_t) i].get()));
}

void TimberWolfEditor::snapshotMenu (int i)
{
    juce::PopupMenu menu;
    auto* self = this;
    const auto& s = proc.snapshot (i);

    menu.addSectionHeader ("Snapshot " + juce::String (i + 1));
    menu.addItem ("Recall", s.stored, false, [self, i] { self->proc.recallSnapshot (i); self->valuesChanged(); });
    menu.addItem ("Store the rig as it is now", [self, i] { self->proc.storeSnapshot (i); self->valuesChanged(); });
    menu.addItem ("Rename...", s.stored, false, [self, i]
    {
        auto* w = new juce::AlertWindow ("Snapshot name", "A short name (up to 12 letters).", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", self->proc.snapshot (i).name);
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([self, i, w] (int r)
        {
            if (r == 1)
            {
                self->proc.renameSnapshot (i, w->getTextEditorContents ("name").trim().substring (0, 12));
                self->valuesChanged();
            }
        }), true);
    });
    menu.addItem ("Clear", s.stored, false, [self, i] { self->proc.clearSnapshot (i); self->valuesChanged(); });

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (content->snaps[(size_t) i].get()));
}

//==============================================================================
//  Rigs
//==============================================================================
void TimberWolfEditor::savePreset (bool askName)
{
    if (! askName && proc.getPresetFile().existsAsFile())
    {
        proc.savePresetFile (proc.getPresetFile(), proc.getPresetName());
        chainChanged();
        return;
    }

    auto* self = this;
    auto* w = new juce::AlertWindow ("Save the rig", "Its name:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", proc.getPresetName());
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([self, w] (int r)
    {
        if (r != 1)
            return;

        const auto name = sonify::presets::cleanName (w->getTextEditorContents ("name"));

        if (name.isEmpty())
            return;

        const auto result = self->proc.savePresetFile (presetfiles::fileFor (name), name);

        if (result.failed())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't save the rig", result.getErrorMessage());

        self->chainChanged();
    }), true);
}

void TimberWolfEditor::loadPresetFromFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a Timber Wolf rig", presetfiles::folder(), "*" + juce::String (sonify::presets::extension));
    auto* self = this;

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [self] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();

        if (f.existsAsFile() && ! self->proc.loadPresetFile (f))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load the rig",
                                                    f.getFileName() + " isn't a Timber Wolf rig.");
    });
}

void TimberWolfEditor::saveBank()
{
    chooser = std::make_unique<juce::FileChooser> ("Save all your rigs as a bank", presetfiles::folder().getChildFile ("Timber Wolf rigs" + juce::String (presetfiles::bankExtension)),
                                                   "*" + juce::String (presetfiles::bankExtension));
    auto* self = this;

    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                          | juce::FileBrowserComponent::warnAboutOverwriting, [self] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();

        if (f == juce::File())
            return;

        f = f.withFileExtension (presetfiles::bankExtension);
        const auto result = self->proc.saveBankFile (f);

        if (result.failed())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't save the bank", result.getErrorMessage());
    });
}

void TimberWolfEditor::loadBank()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a bank of rigs", presetfiles::folder(), "*" + juce::String (presetfiles::bankExtension));
    auto* self = this;

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [self] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();

        if (! f.existsAsFile())
            return;

        const int count = self->proc.loadBankFile (f);
        self->chainChanged();
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Bank loaded",
                                                count > 0 ? juce::String (count) + " rigs were added to yours."
                                                          : f.getFileName() + " has no Timber Wolf rigs in it.");
    });
}
