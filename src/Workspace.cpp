// D:\workspace\Fanan_web_hub\src\Workspace.cpp

#include "Workspace.h"
#include "StellaLookAndFeel.h"

namespace
{
    constexpr int modeGroup = 4101;
}

//==============================================================================
Workspace::PluginView::PluginView()
{
    // Edit and Play: two tabs, one always on.
    for (auto* tab : { &editTab, &playTab })
    {
        tab->setClickingTogglesState (true);
        tab->setRadioGroupId (modeGroup);
        tab->setColour (juce::TextButton::buttonOnColourId, Theme::accent);
    }

    editTab.setTooltip ("Edit: reshape the plugin's panel. Add, move, resize and delete its parts");
    playTab.setTooltip ("Play: the panel is locked. Turn its knobs and play it like the finished plugin");
    editTab.setConnectedEdges (juce::Button::ConnectedOnRight);
    playTab.setConnectedEdges (juce::Button::ConnectedOnLeft);
    editTab.setToggleState (true, juce::dontSendNotification);
    editTab.onClick = [this] { if (editTab.getToggleState()) chooseMode (false); };
    playTab.onClick  = [this] { if (playTab.getToggleState()) chooseMode (true); };

    logButton.setTooltip ("The compiler's messages from the last build");
    logButton.onClick = [this] { if (onShowLog != nullptr) onShowLog(); };

    exportButton.setTooltip ("Build the plugin from its code as it is now and put it into Sonify's Browser (Made in Sonify), to use "
                             "on your tracks. Again later: it's updated");
    exportButton.onClick = [this] { if (onExport != nullptr) onExport(); };

    pluginButton.setTooltip ("Export the plugin as files your DAW loads, built on this computer: VST3 and CLAP"
                           #if JUCE_MAC
                             ", and Audio Unit"
                           #endif
                             ". They play anywhere you copy them");
    pluginButton.onClick = [this] { if (onExportPlugin != nullptr) onExportPlugin(); };

    panicButton.setTooltip ("Panic: stop every note that's sounding, stuck ones too");
    panicButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffff8a80));
    panicButton.onClick = [this] { if (onPanic != nullptr) onPanic(); };

    autoButton.setTooltip ("Replace the GUI with a plain automatic one: a group per module, a control per parameter");
    autoButton.onClick = [this] { if (onAutoLayout != nullptr) onAutoLayout(); };

    // The Gallery beside the panel: drag a knob, slider or switch onto it (onto a control: it
    // wears that look, keeping its size and settings), or double-click one to add it.
    galleryButton.setTooltip ("Knobs, sliders and switches from the Gallery: drag one onto the panel, or onto a control "
                              "to give it that look (it keeps its size and settings). Double-click one to add it");
    galleryButton.onClick = [this]
    {
        KnobStudio::showGallery ("Gallery: drag one onto your plugin",
                                 [safeThis = juce::Component::SafePointer<PluginView> (this)] (const juce::File& file)
                                 {
                                     if (safeThis != nullptr)
                                         safeThis->canvas.addGalleryItem (file);
                                 },
                                 false, true, this);
    };

    for (auto* c : std::initializer_list<juce::Component*> { &editTab, &playTab, &logButton, &exportButton, &pluginButton, &panicButton })
        addChildComponent (c);

    // Play's row: the preset manager, as the plugin's own (presets on the hard drive, banks).
    presetBox.setTextWhenNoChoicesAvailable ("No presets yet");
    presetBox.setTooltip ("Choose a preset: all its values at once (a * means its values changed since)");
    presetBox.onChange = [this]
    {
        // The chosen one again too: its own values back.
        if (presetBox.getSelectedId() > 0 && onPresetChosen != nullptr)
            onPresetChosen (presetBox.getSelectedId() - 1);
    };

    previousButton.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x80"));
    nextButton.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xb6"));
    previousButton.setTooltip ("The previous preset");
    nextButton.setTooltip ("The next preset");
    previousButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    nextButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    previousButton.onClick = [this] { if (const auto i = presets.step (-1); i >= 0 && onPresetChosen != nullptr) onPresetChosen (i); };
    nextButton.onClick = [this] { if (const auto i = presets.step (1); i >= 0 && onPresetChosen != nullptr) onPresetChosen (i); };

    saveButton.setTooltip ("Save the current values over the chosen preset");
    saveButton.onClick = [this] { if (onPresetAction != nullptr) onPresetAction (PresetAction::save); };

    saveAsButton.setButtonText (juce::String::fromUTF8 ("Save as\xe2\x80\xa6"));
    saveAsButton.setTooltip ("Save the current values as a new preset of your own (kept on this computer, for this plugin wherever it plays)");
    saveAsButton.onClick = [this] { if (onPresetAction != nullptr) onPresetAction (PresetAction::saveAs); };

    loadButton.setButtonText (juce::String::fromUTF8 ("Load\xe2\x80\xa6"));
    loadButton.setTooltip ("Load a preset file (.sonifypreset) into your presets");
    loadButton.onClick = [this] { if (onPresetAction != nullptr) onPresetAction (PresetAction::loadFile); };

    moreButton.setButtonText (juce::String::fromUTF8 ("More \xe2\x96\xbe"));
    moreButton.setTooltip ("Banks (every preset in one file), factory presets that come with the plugin, Delete, and the presets folder");
    moreButton.onClick = [this] { showMoreMenu(); };

    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &previousButton, &nextButton, &saveButton, &saveAsButton, &loadButton, &moreButton })
        addChildComponent (c);
    addChildComponent (autoButton);
    addChildComponent (galleryButton);
    addChildComponent (canvas);

    // The toolbox: drag a primitive onto the window, or double-click it.
    toolbox.onAdd = [this] (const juce::String& id)
    {
        if (const auto* primitive = PrimitiveLibrary::get().find (id))
            canvas.addPrimitive (*primitive);
    };

    addChildComponent (toolbox);
}

void Workspace::PluginView::setProject (const juce::String& name, const juce::String& kindName)
{
    projectName = name;
    kind = kindName;

    const bool hasProject = projectName.isNotEmpty();

    for (auto* c : std::initializer_list<juce::Component*> { &editTab, &playTab, &logButton, &exportButton, &pluginButton, &panicButton })
        c->setVisible (hasProject);

    canvas.setVisible (true);   // the grid shows even with nothing on it
    setPlayMode (playMode);   // and the row that goes with the tab

    resized();
    repaint();
}

void Workspace::PluginView::setPlayMode (bool shouldPlay)
{
    playMode = shouldPlay;
    (playMode ? playTab : editTab).setToggleState (true, juce::dontSendNotification);

    // Each tab has a row of its own: Edit's tools, or Play's preset manager.
    const bool hasProject = projectName.isNotEmpty();

    autoButton.setVisible (hasProject && ! playMode);
    galleryButton.setVisible (hasProject && ! playMode);
    toolbox.setVisible (hasProject && ! playMode);

    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &previousButton, &nextButton, &saveButton, &saveAsButton, &loadButton, &moreButton })
        c->setVisible (hasProject && playMode);

    canvas.setDesignMode (! playMode);
    resized();   // the toolbox shows in Edit only
    repaint();
}

void Workspace::PluginView::chooseMode (bool play)
{
    if (play == playMode)
        return;

    setPlayMode (play);

    if (onMode != nullptr)
        onMode (play);
}

void Workspace::PluginView::setBuild (BuildState state, const juce::String& status)
{
    buildState = state;
    buildStatus = status;
    repaint (getLocalBounds().removeFromTop (barHeight));
}

void Workspace::PluginView::setPresets (const PresetMenu& newPresets)
{
    presets = newPresets;

    presetBox.clear (juce::dontSendNotification);
    juce::String heading;

    for (int i = 0; i < presets.names.size(); ++i)
    {
        if (i == 0 || presets.groups[i] != heading)
        {
            heading = presets.groups[i];
            presetBox.addSectionHeading (heading);
        }

        presetBox.addItem (presets.names[i], i + 1);
    }

    // The chosen one by its name (a * when its values changed), or what to do.
    presetBox.setSelectedId (presets.current >= 0 && ! presets.modified ? presets.current + 1 : 0, juce::dontSendNotification);
    presetBox.setTextWhenNothingSelected (presets.currentText());

    const bool any = ! presets.names.isEmpty();
    previousButton.setEnabled (any);
    nextButton.setEnabled (any);
    saveButton.setEnabled (presets.canSave);

    canvas.setPresets (presets);
}

void Workspace::PluginView::showMoreMenu()
{
    constexpr int firstId = 1;
    juce::PopupMenu menu;
    PresetMenus::addActions (menu, presets, firstId);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&moreButton),
                        [safeThis = juce::Component::SafePointer<PluginView> (this)] (int choice)
                        {
                            if (safeThis != nullptr && choice >= firstId && safeThis->onPresetAction != nullptr)
                                safeThis->onPresetAction ((PresetAction) (choice - firstId));
                        });
}

void Workspace::PluginView::setExporting (bool exporting)
{
    exportButton.setEnabled (! exporting);
    exportButton.setButtonText (exporting ? juce::String::fromUTF8 ("Adding\xe2\x80\xa6") : juce::String ("Add to Sonify"));
}

void Workspace::PluginView::setExportingPlugin (bool exporting)
{
    pluginButton.setEnabled (! exporting);
    pluginButton.setButtonText (exporting ? juce::String::fromUTF8 ("Exporting\xe2\x80\xa6") : juce::String ("Export Plugin"));
}

void Workspace::PluginView::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);

    // No project: the canvas's grid fills the whole tab.
    if (projectName.isEmpty())
        return;

    // The build bar: a light for the state, and what's happening.
    auto bar = statusArea;

    const auto lightColour = buildState == BuildState::playing  ? Theme::safe
                           : buildState == BuildState::building ? Theme::hot
                           : buildState == BuildState::failed   ? Theme::clip
                                                                : Theme::muted;

    const auto light = bar.removeFromLeft (10).toFloat().withSizeKeepingCentre (8.0f, 8.0f);
    g.setColour (lightColour.withAlpha (0.35f));
    g.fillEllipse (light.expanded (2.5f));
    g.setColour (lightColour);
    g.fillEllipse (light);

    bar.removeFromLeft (8);
    g.setColour (buildState == BuildState::failed ? Theme::clip.brighter (0.4f) : Theme::text.withAlpha (0.85f));
    g.setFont (Theme::font (14.0f));
    g.drawText (buildStatus.isNotEmpty() ? buildStatus : juce::String ("Not built yet"), bar, juce::Justification::centredLeft, true);

    g.setColour (Theme::outline);
    g.fillRect (0, barHeight - 1, getWidth(), 1);
    g.fillRect (0, barHeight + rowHeight - 1, getWidth(), 1);

    // The tab's own row: Play names its presets; Edit says how editing works.
    g.setColour (Theme::muted);
    g.setFont (Theme::font (13.5f));

    if (playMode)
        g.drawText ("Preset", juce::Rectangle<int> (16, barHeight, 60, rowHeight), juce::Justification::centredLeft, false);
    else
        g.drawFittedText (juce::String::fromUTF8 ("Drag parts in from the toolbox \xc2\xb7 right-click one to edit it or ask Fanan AI \xc2\xb7 "
                                                  "drag empty space to move around \xc2\xb7 Ctrl + wheel zooms"),
                          hintArea, juce::Justification::centredLeft, 1, 0.85f);
}

void Workspace::PluginView::resized()
{
    // The bar: Panic and the build's state on the left, the Edit and Play tabs in the middle,
    // Export Plugin, Add to Sonify and Log on the right.
    const auto full = getLocalBounds().removeFromTop (barHeight).reduced (16, 9);
    auto bar = full;

    auto tabs = juce::Rectangle<int> (modeTabWidth * 2, bar.getHeight()).withCentre (full.getCentre());
    editTab.setBounds (tabs.removeFromLeft (modeTabWidth));
    playTab.setBounds (tabs);

    panicButton.setBounds (bar.removeFromLeft (76));
    bar.removeFromLeft (14);

    logButton.setBounds (bar.removeFromRight (64));
    bar.removeFromRight (8);
    exportButton.setBounds (bar.removeFromRight (124));
    bar.removeFromRight (8);
    pluginButton.setBounds (bar.removeFromRight (120));

    statusArea = juce::Rectangle<int>::leftTopRightBottom (bar.getX(), bar.getY() + 1, juce::jmax (bar.getX(), editTab.getX() - 14), bar.getBottom() - 1);

    const auto row = getLocalBounds().withTrimmedTop (barHeight).removeFromTop (rowHeight).reduced (16, 6);

    // Edit's row: Auto layout, then how editing works.
    auto tools = row;
    autoButton.setBounds (tools.removeFromLeft (104));
    tools.removeFromLeft (8);
    galleryButton.setBounds (tools.removeFromLeft (96));
    tools.removeFromLeft (18);
    hintArea = tools;

    // Play's row: the preset manager.
    auto line = row;
    line.removeFromLeft (56);
    presetBox.setBounds (line.removeFromLeft (240));
    line.removeFromLeft (6);
    previousButton.setBounds (line.removeFromLeft (30));
    nextButton.setBounds (line.removeFromLeft (30));
    line.removeFromLeft (12);
    saveButton.setBounds (line.removeFromLeft (64));
    line.removeFromLeft (6);
    saveAsButton.setBounds (line.removeFromLeft (88));
    line.removeFromLeft (6);
    loadButton.setBounds (line.removeFromLeft (72));
    line.removeFromLeft (6);
    moreButton.setBounds (line.removeFromLeft (84));

    // The grid: under the bars with a project, the whole tab without one. In Edit, the
    // toolbox runs down its left side.
    auto rest = projectName.isNotEmpty() ? getLocalBounds().withTrimmedTop (barHeight + rowHeight) : getLocalBounds();

    if (toolbox.isVisible())
        toolbox.setBounds (rest.removeFromLeft (Toolbox::width));

    canvas.setBounds (rest);
}

//==============================================================================
void Workspace::HintView::setText (const juce::String& newTitle, const juce::String& newDetail)
{
    title = newTitle;
    detail = newDetail;
    repaint();
}

void Workspace::HintView::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);

    auto area = getLocalBounds().withSizeKeepingCentre (juce::jmin (560, getWidth() - 40), 110);

    g.setColour (Theme::text);
    g.setFont (Theme::font (20.0f, true));
    g.drawText (title, area.removeFromTop (32), juce::Justification::centred, false);

    // A scale of 1 wraps long text onto more lines instead of squashing it onto one.
    g.setColour (Theme::muted);
    g.setFont (Theme::font (14.5f));
    g.drawFittedText (detail, area, juce::Justification::centredTop, 4, 1.0f);
}

//==============================================================================
Workspace::Workspace()
{
    pluginView.onShowLog    = [this] { if (onShowLogRequested != nullptr) onShowLogRequested(); };
    pluginView.onExport     = [this] { if (onExportRequested != nullptr) onExportRequested(); };
    pluginView.onExportPlugin = [this] { if (onExportPluginRequested != nullptr) onExportPluginRequested(); };
    pluginView.onPanic      = [this] { if (onPanicRequested != nullptr) onPanicRequested(); };
    pluginView.onPresetChosen = [this] (int i) { if (onPresetChosen != nullptr) onPresetChosen (i); };
    pluginView.onPresetAction = [this] (PresetAction action) { if (onPresetAction != nullptr) onPresetAction (action); };
    pluginView.canvas.onPresetChosen = [this] (int i) { if (onPresetChosen != nullptr) onPresetChosen (i); };
    pluginView.canvas.onPresetAction = [this] (PresetAction action) { if (onPresetAction != nullptr) onPresetAction (action); };
    pluginView.canvas.onElementsFailed = [this] (const juce::String& why) { if (onElementsFailed != nullptr) onElementsFailed (why); };
    pluginView.canvas.onPaintPicture = [this] (const juce::var& recipe, std::function<void (const juce::String&)> done)
    {
        if (onPaintPicture != nullptr)
            onPaintPicture (recipe, std::move (done));
        else if (done != nullptr)
            done ("Painting isn't available here.");
    };
    pluginView.onAutoLayout = [this] { if (onAutoLayoutRequested != nullptr) onAutoLayoutRequested(); };
    pluginView.onMode       = [this] (bool play) { if (onModeChanged != nullptr) onModeChanged (play); };

    pluginView.canvas.onLayoutEdited = [this]
    {
        // A look of mine put on a control comes into the plugin, so it opens anywhere.
        looks.adoptMine (pluginView.canvas.getLayout());

        if (onLayoutEdited != nullptr)
            onLayoutEdited();
    };
    pluginView.canvas.onAskAi = [this] (int widgetIndex, const juce::String& instruction)
    {
        if (onAskAiAboutGui != nullptr)
            onAskAiAboutGui (widgetIndex, instruction);
    };
    pluginView.canvas.onOpenChat = [this] { showTab (aiTab); };

    // The looks: the canvas draws from them and the Knob Studio edits them. A change shows
    // on both at once.
    pluginView.canvas.setLooks (looks);
    looks.onChanged = [this]
    {
        pluginView.canvas.looksChanged();
        studio.looksChanged();
    };

    // A picture chosen for a look's Image layer is copied into the project's pictures.
    LayerImages::setAdopter ([] (const juce::File& picture)
    {
        const auto folder = LayerImages::getFolder();
        return folder != juce::File() ? GuiLayout::importPicture (picture, folder) : juce::String();
    });

    // A knob, slider or switch's menu in the Edit tab, "Design it in the Knob Studio": its look
    // opens there, for it alone or with every control like it.
    pluginView.canvas.onEditLook = [this] (int widgetIndex, bool includeSimilars)
    {
        studio.setLayout (pluginView.canvas.getLayout());
        studio.editControl (widgetIndex, includeSimilars);

        const juce::ScopedValueSetter<bool> forControl (openingStudioForControl, true);
        showTab (studioTab);
    };

    pluginView.canvas.readLevel = [this] (const juce::String& source, bool rms) { return readLevel != nullptr ? readLevel (source, rms) : 0.0f; };
    pluginView.canvas.readScope = [this] (const juce::String& source, float* destination, int numSamples)
    {
        if (readScope != nullptr)
            readScope (source, destination, numSamples);
        else
            std::fill (destination, destination + numSamples, 0.0f);
    };
    pluginView.canvas.onSourcesChanged = [this] (const juce::StringArray& levels, const juce::StringArray& scopes)
    {
        if (onSourcesChanged != nullptr)
            onSourcesChanged (levels, scopes);
    };
    studio.onUseLook = [this] (int widgetIndex, const juce::String& lookName)
    {
        pluginView.canvas.useLook (widgetIndex, lookName);
        studio.setLayout (pluginView.canvas.getLayout());
    };
    studio.onGiveOwnLook = [this] (int widgetIndex)
    {
        const auto name = pluginView.canvas.giveOwnLook (widgetIndex);
        studio.setLayout (pluginView.canvas.getLayout());
        return name;
    };
    studio.onBack = [this] { showTab (pluginTab); };
    pluginView.canvas.onParameterChanged = [this] (int index, float value)
    {
        if (onParameterChanged != nullptr)
            onParameterChanged (index, value);
    };
    pluginView.canvas.onNote = [this] (int note, float velocity)
    {
        if (onNote != nullptr)
            onNote (note, velocity);
    };
    pluginView.canvas.isNoteDown = [this] (int note) { return isNoteDown != nullptr && isNoteDown (note); };

    // The tabs are picked in the top bar; one page shows at a time.
    addChildComponent (pluginView);
    addChildComponent (studio);
    addChildComponent (notice);
    showPages();
}

void Workspace::setAiPage (juce::Component& page)
{
    aiPage = &page;
    addChildComponent (page);
    page.setBounds (getLocalBounds());
    showPages();
}

void Workspace::setProtectionPage (juce::Component& page)
{
    protectionPage = &page;
    addChildComponent (page);
    page.setBounds (getLocalBounds());
    showPages();
}

void Workspace::setStorePage (juce::Component& page)
{
    storePage = &page;
    addChildComponent (page);
    page.setBounds (getLocalBounds());
    showPages();
}

juce::Image Workspace::snapshotPanel (int longest)
{
    const auto& layout = pluginView.canvas.getLayout();
    const auto side = juce::jmax (1, juce::jmax (layout.width, layout.height));
    return pluginView.canvas.snapshot (juce::jlimit (0.5f, 2.0f, (float) longest / (float) side));
}

void Workspace::showTab (TabIndex index)
{
    // The Knob Studio opened from the top bar: empty, with the plugin's looks as they are now in
    // its list. A control's look opens in it only from that control's menu.
    if (index == studioTab && currentTab != studioTab && ! openingStudioForControl)
    {
        studio.setLayout (pluginView.canvas.getLayout());
        studio.clear();
    }

    currentTab = index;
    showPages();

    if (onTabChanged != nullptr)
        onTabChanged ((int) index);
}

void Workspace::showPages()
{
    // Edit UI and the Knob Studio give way to the notice while it has something to say (a MIDI
    // FX has no panel); Build with AI, Protection and the Store never do.
    const bool noted = notice.hasText() && (currentTab == pluginTab || currentTab == studioTab);
    juce::Component* pages[numPages] { aiPage, &pluginView, &studio, protectionPage, storePage };

    for (int i = 0; i < numPages; ++i)
        if (pages[i] != nullptr)
            pages[i]->setVisible (i == (int) currentTab && ! noted);

    notice.setVisible (noted);
}

void Workspace::setNotice (const juce::String& title, const juce::String& detail)
{
    notice.setText (title, detail);
    showPages();
}

void Workspace::setProject (const juce::String& name, const juce::String& kindName)
{
    pluginView.setProject (name, kindName);
}

void Workspace::setGuiFolder (const juce::File& folder)
{
    LayerImages::setFolder (folder != juce::File() ? folder.getChildFile (GuiLayout::imagesFolder) : juce::File());
    looks.setGuiFolder (folder);
    pluginView.canvas.setGuiFolder (folder);
}

void Workspace::showAiWorking (const juce::String& what)                  { pluginView.canvas.showAiWorking (what); }
void Workspace::showAiReply (const juce::String& reply)                   { pluginView.canvas.showAiReply (reply); }
void Workspace::setPlayMode (bool playMode)                               { pluginView.setPlayMode (playMode); }
void Workspace::setBuild (BuildState state, const juce::String& status)   { pluginView.setBuild (state, status); }
void Workspace::setExporting (bool exporting)                             { pluginView.setExporting (exporting); }
void Workspace::setExportingPlugin (bool exporting)                       { pluginView.setExportingPlugin (exporting); }
void Workspace::setParameters (const juce::Array<ParamControl>& params)   { pluginView.canvas.setParameters (params); }
void Workspace::setLayout (const GuiLayout& layout)
{
    // A new layout may come with changed looks (an undo, Sonify AI): read them again.
    looks.reload();
    looks.adoptMine (layout);

    pluginView.canvas.setLayout (layout);
    studio.setLayout (layout);
}

void Workspace::setPresets (const PresetMenu& presets)                       { pluginView.setPresets (presets); }

void Workspace::setSources (const juce::StringArray& signals, const juce::StringArray& displays)
{
    pluginView.canvas.setSources (signals, displays);
}

void Workspace::resized()
{
    for (auto* page : { aiPage, static_cast<juce::Component*> (&pluginView), static_cast<juce::Component*> (&studio),
                        protectionPage, storePage, static_cast<juce::Component*> (&notice) })
        if (page != nullptr)
            page->setBounds (getLocalBounds());
}
