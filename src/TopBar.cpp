// D:\workspace\Fanan_web_hub\src\TopBar.cpp

#include "TopBar.h"
#include "StellaLookAndFeel.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace
{
    enum MenuIds
    {
        revealId = 1,
        closeId  = 2,
        newId = 5,
        openId = 6,
        saveId = 7,
        saveAsId = 8,
        finishSoundId = 9,
        libraryId = 10,
        recentBaseId = 100
    };

    juce::Font tabFont()     { return Theme::font (14.5f, true); }
    juce::Font menuFont()    { return Theme::font (14.5f); }

    int textWidth (const juce::Font& font, const juce::String& text)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (font, text, 0.0f, 0.0f);
        return (int) std::ceil (glyphs.getBoundingBox (0, glyphs.getNumGlyphs(), true).getWidth());
    }

    constexpr int minNameWidth = 120;
    constexpr int wordmarkWidth = 118, meterWidth = 16, loadWidth = 72;
    constexpr int midiInWidth = 168, midiInCompactWidth = 90;

    //==========================================================================
    /** MIDI In's list, dropped from its button: All inputs, or the ports ticked one by one. */
    class MidiInList final : public juce::Component
    {
    public:
        MidiInList (const std::vector<TopBar::MidiPort>& available, const juce::StringArray& chosen,
                    std::function<void (const juce::StringArray&)> changed)
            : onChange (std::move (changed))
        {
            every.setButtonText ("All inputs");
            every.setToggleState (chosen.contains ("*"), juce::dontSendNotification);
            every.onClick = [this] { everyClicked(); };
            addAndMakeVisible (every);

            // The ports here, then any picked before that isn't here now.
            for (const auto& port : available)
                addPort (port.name, port.device.isNotEmpty() ? port.name + "  (" + port.device + ")" : port.name, chosen.contains (port.name));

            for (const auto& name : chosen)
                if (name != "*" && std::none_of (available.begin(), available.end(), [&name] (const TopBar::MidiPort& p) { return p.name == name; }))
                    addPort (name, name + "  (not connected)", true);

            note.setText (ports.isEmpty() ? "No MIDI input is enabled: Sonify's Audio Settings enable them."
                                          : "Heard only while the studio is in front; its keyboards always.",
                          juce::dontSendNotification);
            note.setFont (Theme::font (12.0f));
            note.setColour (juce::Label::textColourId, Theme::muted);
            note.setJustificationType (juce::Justification::topLeft);
            addAndMakeVisible (note);

            showStates();
            setSize (320, 12 + rowHeight * (1 + ports.size()) + 44);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (10, 6);
            every.setBounds (area.removeFromTop (rowHeight));

            for (auto* port : ports)
                port->setBounds (area.removeFromTop (rowHeight).withTrimmedLeft (18));

            note.setBounds (area.withTrimmedTop (6));
        }

    private:
        static constexpr int rowHeight = 26;

        void addPort (const juce::String& name, const juce::String& label, bool ticked)
        {
            auto* button = ports.add (new juce::ToggleButton (label));
            button->setComponentID (name);
            button->setToggleState (ticked, juce::dontSendNotification);
            button->onClick = [this] { report(); };
            addAndMakeVisible (button);
        }

        void everyClicked()
        {
            // All off: every port here ticked to begin with, so nothing goes quiet until one is unticked.
            if (! every.getToggleState())
                for (auto* port : ports)
                    if (! port->getButtonText().endsWith ("(not connected)"))
                        port->setToggleState (true, juce::dontSendNotification);

            showStates();
            report();
        }

        void showStates()
        {
            for (auto* port : ports)
                port->setEnabled (! every.getToggleState());
        }

        void report()
        {
            juce::StringArray chosen;

            if (every.getToggleState())
                chosen.add ("*");
            else
                for (auto* port : ports)
                    if (port->getToggleState())
                        chosen.add (port->getComponentID());

            if (onChange != nullptr)
                onChange (chosen);
        }

        std::function<void (const juce::StringArray&)> onChange;
        juce::ToggleButton every;
        juce::OwnedArray<juce::ToggleButton> ports;
        juce::Label note;
    };
}

//==============================================================================
void TopBar::MidiInButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto area = getLocalBounds().toFloat();

    g.setColour (down ? Theme::raised.brighter (0.12f) : highlighted ? Theme::raised : Theme::raised.withAlpha (0.55f));
    g.fillRoundedRectangle (area.reduced (0.0f, 4.0f), 5.0f);

    // The light: lit as MIDI comes through.
    const auto led = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ area.getX() + 12.0f, area.getCentreY() });
    g.setColour (lit ? Theme::safe : Theme::inset);
    g.fillEllipse (led);
    g.setColour (Theme::outline);
    g.drawEllipse (led, 1.0f);

    const auto words = area.withTrimmedLeft (22.0f).withTrimmedRight (16.0f);
    g.setColour (Theme::text);
    g.setFont (Theme::font (12.5f, true));
    g.drawText ("MIDI In", words, juce::Justification::centredLeft, false);

    if (! compact)
    {
        const auto label = Theme::font (12.5f, true);
        const auto after = words.withTrimmedLeft ((float) textWidth (label, "MIDI In") + 7.0f);
        g.setColour (Theme::muted);
        g.setFont (Theme::font (12.5f));
        g.drawFittedText (summary, after.toNearestInt(), juce::Justification::centredLeft, 1, 0.85f);
    }

    g.setColour (Theme::muted);
    g.setFont (Theme::font (11.0f));
    g.drawText (juce::String::fromUTF8 ("\xe2\x96\xbe"), area.removeFromRight (16.0f), juce::Justification::centredLeft, false);
}

//==============================================================================
void TopBar::MenuTitle::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    if (open || highlighted || down)
    {
        g.setColour (open || down ? Theme::raised.brighter (0.12f) : Theme::raised);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
    }

    g.setColour (Theme::text);
    g.setFont (menuFont());
    g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred, false);
}

void TopBar::TabButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto area = getLocalBounds().toFloat();
    const bool on = getToggleState();

    if (! on && (highlighted || down))
    {
        g.setColour (Theme::raised.withAlpha (0.7f));
        g.fillRoundedRectangle (area.reduced (2.0f, 8.0f), 5.0f);
    }

    g.setColour (on ? Theme::text : (highlighted ? Theme::text.withAlpha (0.85f) : Theme::muted));
    g.setFont (Theme::font (14.5f, on));
    g.drawText (getButtonText(), area, juce::Justification::centred, false);

    // The tab that's showing carries the accent as an underline, on the bar's bottom edge.
    if (on)
    {
        g.setColour (Theme::accent);
        g.fillRect (area.withTop (area.getBottom() - 2.0f).reduced (10.0f, 0.0f));
    }
}

//==============================================================================
float TopBar::Meter::toPosition (float gain) noexcept
{
    // -60 dB at the bottom, +6 dB at the top.
    const auto db = juce::Decibels::gainToDecibels (gain, -60.0f);
    return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
}

void TopBar::Meter::setLevels (float left, float right)
{
    const float incoming[2] { left, right };

    for (int i = 0; i < 2; ++i)
    {
        // Fast up, slow down; the peak line holds for about a second.
        levels[i] = incoming[i] > levels[i] ? incoming[i] : levels[i] * 0.86f;

        if (incoming[i] >= holds[i])
        {
            holds[i] = incoming[i];
            holdFrames[i] = 30;
        }
        else if (--holdFrames[i] <= 0)
        {
            holds[i] *= 0.9f;
        }
    }

    repaint();
}

void TopBar::Meter::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto barWidth = (bounds.getWidth() - 2.0f) * 0.5f;

    for (int i = 0; i < 2; ++i)
    {
        const auto frame = juce::Rectangle<float> (bounds.getX() + (float) i * (barWidth + 2.0f), bounds.getY(),
                                                   barWidth, bounds.getHeight());

        // A thin frame, so the meter shows even in silence.
        g.setColour (Theme::outline);
        g.fillRect (frame);
        const auto bar = frame.reduced (1.0f);
        g.setColour (Theme::inset);
        g.fillRect (bar);

        const auto top = bar.getBottom() - bar.getHeight() * toPosition (levels[i]);
        const auto zeroDb = bar.getBottom() - bar.getHeight() * toPosition (1.0f);
        const auto minus6 = bar.getBottom() - bar.getHeight() * toPosition (0.5f);

        g.setColour (Theme::safe);
        g.fillRect (bar.withTop (juce::jmax (top, minus6)));

        if (top < minus6)
        {
            g.setColour (Theme::hot);
            g.fillRect (bar.withTop (juce::jmax (top, zeroDb)).withBottom (minus6));
        }

        if (top < zeroDb)
        {
            g.setColour (Theme::clip);
            g.fillRect (bar.withTop (top).withBottom (zeroDb));
        }

        if (holds[i] > 0.001f)
        {
            const auto holdY = bar.getBottom() - bar.getHeight() * toPosition (holds[i]);
            g.setColour (holds[i] >= 1.0f ? Theme::clip : Theme::text.withAlpha (0.7f));
            g.fillRect (bar.withTop (holdY).withHeight (1.5f));
        }
    }
}

//==============================================================================
TopBar::TopBar()
{
    optionsMenu.setTooltip ("New, open, save and recent projects");
    optionsMenu.onClick = [this] { showOptionsMenu(); };

    meter.setTooltip ("Output level");

    midiInButton.setTooltip ("MIDI In: the inputs the studio hears while it's in front");
    midiInButton.onClick = [this] { showMidiInList(); };

    for (auto* c : std::initializer_list<juce::Component*> { &optionsMenu, &midiInButton, &meter })
        addAndMakeVisible (c);

    for (int i = 0; i < numTabs; ++i)
    {
        auto* tab = tabs.add (new TabButton (tabNames[i]));
        tab->onClick = [this, i] { if (onTabChosen != nullptr) onTabChosen (i); };
        addAndMakeVisible (tab);
    }

    setCurrentTab (0);

    startTimerHz (30);
}

TopBar::~TopBar()
{
    stopTimer();
}

//==============================================================================
void TopBar::setProjectName (const juce::String& name)
{
    projectName = name;
    repaint (noticeArea);
}

void TopBar::showNotice (const juce::String& text)
{
    notice = text;
    noticeUntil = juce::Time::getMillisecondCounter() + 2500;
    repaint (noticeArea);
}

void TopBar::setCurrentTab (int index)
{
    for (int i = 0; i < tabs.size(); ++i)
        tabs[i]->setToggleState (i == index, juce::dontSendNotification);
}

void TopBar::refreshMidiIn()
{
    const auto chosen = midiInChosen != nullptr ? midiInChosen() : juce::StringArray ("*");

    midiInButton.setSummary (chosen.contains ("*") ? juce::String ("All")
                           : chosen.isEmpty() ? juce::String ("None")
                           : chosen.size() == 1 ? chosen[0]
                           : juce::String (chosen.size()) + " inputs");
    midiInButton.setTooltip ("MIDI In: " + (chosen.contains ("*") ? juce::String ("every input")
                                          : chosen.isEmpty() ? juce::String ("no input, the keyboards only")
                                          : chosen.joinIntoString (", "))
                             + ". Heard only while the studio is in front.");
}

void TopBar::showMidiInList()
{
    auto* top = getTopLevelComponent();

    if (top == nullptr)
        return;

    const auto ports = midiInPorts != nullptr ? midiInPorts() : std::vector<MidiPort>();
    const auto chosen = midiInChosen != nullptr ? midiInChosen() : juce::StringArray ("*");

    auto list = std::make_unique<MidiInList> (ports, chosen, [safeThis = juce::Component::SafePointer<TopBar> (this)] (const juce::StringArray& picked)
    {
        if (safeThis == nullptr)
            return;

        if (safeThis->onMidiInChosen != nullptr)
            safeThis->onMidiInChosen (picked);

        safeThis->refreshMidiIn();
    });

    // Inside the studio's own window (over the page), dropped from the button.
    juce::CallOutBox::launchAsynchronously (std::move (list), top->getLocalArea (this, midiInButton.getBounds()), top);
}

void TopBar::timerCallback()
{
    meter.setLevels (takePeak != nullptr ? takePeak (0) : 0.0f, takePeak != nullptr ? takePeak (1) : 0.0f);

    // MIDI In's light: lit a moment each time something comes through.
    if (midiInActivity != nullptr)
    {
        if (const auto now = midiInActivity(); now != midiInSeen)
        {
            midiInSeen = now;
            midiInLitFrames = 4;
        }
    }

    midiInButton.setLit (midiInLitFrames > 0);

    if (midiInLitFrames > 0)
        --midiInLitFrames;

    if (notice.isNotEmpty() && juce::Time::getMillisecondCounter() > noticeUntil)
    {
        notice.clear();
        repaint (noticeArea);
    }

    const auto load = juce::String (juce::roundToInt ((audioLoad != nullptr ? audioLoad() : 0.0) * 100.0)) + "%";

    if (load != loadText)
    {
        loadText = load;
        repaint (loadArea);
    }
}

//==============================================================================
void TopBar::showOptionsMenu()
{
    // A plain Windows-style menu: text items, separators, and a submenu for recent projects.
    // Projects live on this computer and save themselves as they change; Save writes what's
    // waiting at once, Save as copies the plugin into a new folder.
    const auto recent = recentProjects != nullptr ? recentProjects() : juce::StringArray();
    const bool hasProject = projectName.isNotEmpty();

    juce::PopupMenu menu;
    menu.addItem (newId, juce::String::fromUTF8 ("New plugin\xe2\x80\xa6"));
    menu.addItem (openId, juce::String::fromUTF8 ("Open\xe2\x80\xa6"));

    juce::PopupMenu recentMenu;

    for (int i = 0; i < recent.size(); ++i)
        recentMenu.addItem (recentBaseId + i, juce::File (recent[i]).getParentDirectory().getFileName());

    menu.addSubMenu ("Open recent", recentMenu, ! recent.isEmpty());
    menu.addSeparator();

    auto item = [hasProject] (int id, const juce::String& text, const char* keys)
    {
        juce::PopupMenu::Item i (text);
        i.itemID = id;
        i.isEnabled = hasProject;
        i.shortcutKeyDescription = keys;
        return i;
    };

    menu.addItem (item (saveId, "Save", "Ctrl+S"));
    menu.addItem (item (saveAsId, juce::String::fromUTF8 ("Save as\xe2\x80\xa6"), "Ctrl+Shift+S"));
    menu.addSeparator();
    menu.addItem (revealId, "Show project folder", hasProject);
    menu.addItem (closeId, "Close project", hasProject);
    menu.addSeparator();
    menu.addItem (libraryId, juce::String::fromUTF8 ("Sample library\xe2\x80\xa6"), hasProject);
    menu.addSeparator();
    menu.addItem (finishSoundId, "Sound when Fanan AI finishes", true, isFinishSoundOn != nullptr && isFinishSoundOn());

    optionsMenu.setOpen (true);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&optionsMenu).withMinimumWidth (220),
                        [safeThis = juce::Component::SafePointer<TopBar> (this), recent] (int result)
                        {
                            if (safeThis == nullptr)
                                return;

                            safeThis->optionsMenu.setOpen (false);

                            if (result == newId && safeThis->onNew != nullptr)
                                safeThis->onNew();
                            else if (result == openId && safeThis->onOpen != nullptr)
                                safeThis->onOpen();
                            else if (result == saveId && safeThis->onSave != nullptr)
                                safeThis->onSave();
                            else if (result == saveAsId && safeThis->onSaveAs != nullptr)
                                safeThis->onSaveAs();
                            else if (result == revealId && safeThis->onRevealProject != nullptr)
                                safeThis->onRevealProject();
                            else if (result == closeId && safeThis->onCloseProject != nullptr)
                                safeThis->onCloseProject();
                            else if (result == finishSoundId && safeThis->onToggleFinishSound != nullptr)
                                safeThis->onToggleFinishSound();
                            else if (result == libraryId && safeThis->onSampleLibrary != nullptr)
                                safeThis->onSampleLibrary();
                            else if (result >= recentBaseId && result - recentBaseId < recent.size() && safeThis->onOpenRecent != nullptr)
                                safeThis->onOpenRecent (juce::File (recent[result - recentBaseId]));
                        });
}

//==============================================================================
void TopBar::paint (juce::Graphics& g)
{
    g.fillAll (Theme::window);

    g.setColour (Theme::outline);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    // FANAN in the accent, AI quieter: Fanan AI, the studio's builder.
    if (! wordmarkArea.isEmpty())
    {
        const auto bold = Theme::font (17.0f, true);
        const auto area = wordmarkArea.toFloat();

        g.setFont (bold);
        g.setColour (Theme::accent);
        g.drawText ("FANAN", area, juce::Justification::centredLeft, false);

        g.setFont (Theme::font (17.0f));
        g.setColour (Theme::muted);
        g.drawText ("AI", area.withTrimmedLeft ((float) textWidth (bold, "FANAN") + 6.0f), juce::Justification::centredLeft, false);
    }

    // A notice ("Saved") for a moment; otherwise the open plugin's name.
    if (noticeArea.getWidth() > 30)
    {
        if (notice.isNotEmpty())
        {
            g.setColour (Theme::safe);
            g.setFont (Theme::font (13.5f, true));
            g.drawFittedText (notice, noticeArea, juce::Justification::centredLeft, 1, 0.8f);
        }
        else if (projectName.isNotEmpty())
        {
            g.setColour (Theme::text.withAlpha (0.8f));
            g.setFont (Theme::font (13.5f, true));
            g.drawFittedText (projectName, noticeArea, juce::Justification::centredLeft, 1, 0.8f);
        }
    }

    // The audio load, beside the meter.
    if (! loadArea.isEmpty())
    {
        auto load = loadArea;
        g.setColour (Theme::muted);
        g.setFont (Theme::font (10.5f));
        g.drawText ("AUDIO LOAD", load.removeFromTop (load.getHeight() / 2), juce::Justification::bottomLeft, false);
        g.setColour (Theme::text);
        g.setFont (Theme::monoFont (13.0f));
        g.drawText (loadText, load, juce::Justification::topLeft, false);
    }
}

void TopBar::resized()
{
    auto area = getLocalBounds().reduced (12, 8);
    area.removeFromBottom (1);

    const auto optionsWidth = textWidth (menuFont(), optionsMenu.getButtonText()) + 22;

    juce::Array<int> tabWidths;
    int tabsWidth = 0;

    for (auto* tab : tabs)
    {
        tabWidths.add (textWidth (tabFont(), tab->getButtonText()) + 40);
        tabsWidth += tabWidths.getLast();
    }

    // Always there: Options, the tabs and the meter, with room for the plugin's name.
    // When the bar is too narrow for the rest, the wordmark goes first, then the audio load.
    bool showWordmark = true, showLoad = true;

    bool compactMidi = false;

    const auto needed = [&]
    {
        return (showWordmark ? wordmarkWidth : 0) + optionsWidth + 12 + minNameWidth + 24 + tabsWidth + 24
             + (compactMidi ? midiInCompactWidth : midiInWidth) + 12 + meterWidth + (showLoad ? 8 + loadWidth : 0);
    };

    if (needed() > area.getWidth())   showWordmark = false;
    if (needed() > area.getWidth())   showLoad = false;
    if (needed() > area.getWidth())   compactMidi = true;

    wordmarkArea = showWordmark ? area.removeFromLeft (wordmarkWidth) : juce::Rectangle<int>();

    // Right: the load and the meter.
    loadArea = showLoad ? area.removeFromRight (loadWidth) : juce::Rectangle<int>();

    if (showLoad)
        area.removeFromRight (8);

    meter.setBounds (area.removeFromRight (meterWidth).reduced (0, 1));
    area.removeFromRight (12);

    midiInButton.setCompact (compactMidi);
    midiInButton.setBounds (area.removeFromRight (compactMidi ? midiInCompactWidth : midiInWidth));
    area.removeFromRight (24);

    // Left: Options, then the plugin's name.
    optionsMenu.setBounds (area.removeFromLeft (optionsWidth));
    area.removeFromLeft (12);

    // The tabs: centred in the window, moving right only as far as a short name needs.
    const auto earliest = area.getX() + minNameWidth + 24;
    const auto latest = juce::jmax (earliest, area.getRight() - tabsWidth);
    auto x = juce::jlimit (earliest, latest, (getWidth() - tabsWidth) / 2);

    noticeArea = juce::Rectangle<int>::leftTopRightBottom (area.getX() + 2, 0, x - 12, getHeight());

    for (int i = 0; i < tabs.size(); ++i)
    {
        tabs[i]->setBounds (x, 0, tabWidths[i], getHeight());
        x += tabWidths[i];
    }
}
