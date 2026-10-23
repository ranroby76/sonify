// D:\workspace\Fanan_web_hub\src\CreatorStudio.cpp

#include "CreatorStudio.h"
#include "AiAgent.h"
#include "CreatorPlugin.h"
#include "DeviceStudio.h"
#include "GuiLayout.h"
#include "PluginLibrary.h"
#include "StellaLookAndFeel.h"
#include "WasmCompiler.h"

#include <iterator>

namespace
{
    /** A new project is empty until Sonify AI writes its first modules and wires them. */
    bool hasNothingToBuild (const Project& project)
    {
        return ! project.isOpen() || ! project.getFolder().getChildFile (WasmCompiler::graphFileName).existsAsFile();
    }

    constexpr const char* emptyProjectStatus = "Empty so far: describe your plugin to Fanan AI";

    // The chosen preset and the values, kept with the project (not part of undo, nor of the plugin).
    constexpr const char* playStateFileName = "play.json";

    //==============================================================================
    /** Export Plugin's choices, in its dialog: the formats this computer makes (all ticked), the
        folder they go into, and whether they're copied into the user's own plugin folders too. */
    class ExportOptions final : public juce::Component
    {
    public:
        explicit ExportOptions (const juce::File& defaultFolder)
            : folder (defaultFolder)
        {
            for (const auto format : PluginExporter::formatsHere())
            {
                auto* toggle = toggles.add (new juce::ToggleButton (PluginExporter::formatName (format)));
                toggle->setToggleState (true, juce::dontSendNotification);
                toggle->onClick = [this] { if (onChanged != nullptr) onChanged(); };
                addAndMakeVisible (toggle);
                formats.add (format);
            }

            folderLabel.setFont (Theme::font (13.5f));
            folderLabel.setColour (juce::Label::textColourId, Theme::text);
            folderLabel.setMinimumHorizontalScale (0.6f);
            addAndMakeVisible (folderLabel);

            chooseButton.setTooltip ("Choose the folder the plugin files go into");
            chooseButton.onClick = [this] { chooseFolder(); };
            addAndMakeVisible (chooseButton);

            install.setToggleState (true, juce::dontSendNotification);
            install.setTooltip ("Copied into your own plugin folders too, where DAWs look for plugins (no admin rights needed):\n"
                                + PluginExporter::installFoldersText());
            addAndMakeVisible (install);

            showFolder();
            setSize (460, 112);
        }

        juce::Array<PluginExporter::Format> chosenFormats() const
        {
            juce::Array<PluginExporter::Format> chosen;

            for (int i = 0; i < toggles.size(); ++i)
                if (toggles[i]->getToggleState())
                    chosen.add (formats[i]);

            return chosen;
        }

        juce::File getFolder() const noexcept    { return folder; }
        bool shouldInstall() const               { return install.getToggleState(); }

        std::function<void()> onChanged;

        void paint (juce::Graphics& g) override
        {
            g.setColour (Theme::muted);
            g.setFont (Theme::font (13.5f));
            g.drawText ("Formats", 0, 0, 70, 26, juce::Justification::centredLeft, false);
            g.drawText ("Into", 0, 34, 70, 26, juce::Justification::centredLeft, false);
        }

        void resized() override
        {
            auto row = getLocalBounds().removeFromTop (26).withTrimmedLeft (74);

            for (auto* toggle : toggles)
                toggle->setBounds (row.removeFromLeft (juce::jmin (120, row.getWidth())));

            auto folderRow = getLocalBounds().withTrimmedTop (34).removeFromTop (26).withTrimmedLeft (70);
            chooseButton.setBounds (folderRow.removeFromRight (96));
            folderRow.removeFromRight (8);
            folderLabel.setBounds (folderRow);

            install.setBounds (getLocalBounds().withTrimmedTop (72).removeFromTop (26));
        }

    private:
        void showFolder()
        {
            folderLabel.setText (folder.getFullPathName(), juce::dontSendNotification);
            folderLabel.setTooltip (folder.getFullPathName());
        }

        void chooseFolder()
        {
            chooser = std::make_unique<juce::FileChooser> ("Export the plugin files into", folder.exists() ? folder : folder.getParentDirectory());
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                  [safeThis = juce::Component::SafePointer<ExportOptions> (this)] (const juce::FileChooser& c)
                                  {
                                      if (safeThis != nullptr && c.getResult() != juce::File())
                                      {
                                          safeThis->folder = c.getResult();
                                          safeThis->showFolder();
                                      }
                                  });
        }

        juce::OwnedArray<juce::ToggleButton> toggles;
        juce::Array<PluginExporter::Format> formats;
        juce::File folder;
        juce::Label folderLabel;
        juce::TextButton chooseButton { juce::String::fromUTF8 ("Choose\xe2\x80\xa6") };
        juce::ToggleButton install { "Copy them into my plugin folders too, where DAWs look" };
        std::unique_ptr<juce::FileChooser> chooser;
    };

    //==============================================================================
    /** The compiler's messages from the last build, in a window of their own. */
    class LogWindow final : public juce::DocumentWindow
    {
    public:
        LogWindow()
            : DocumentWindow ("Build log", Theme::panel, DocumentWindow::closeButton)
        {
            setUsingNativeTitleBar (true);

            text.setMultiLine (true, false);
            text.setReadOnly (true);
            text.setScrollbarsShown (true);
            text.setFont (Theme::monoFont (14.0f));
            text.setSize (760, 420);

            setContentNonOwned (&text, true);
            setResizable (true, false);
        }

        void setLog (const juce::String& log)
        {
            text.setText (log.isNotEmpty() ? log : juce::String ("No messages: the last build was clean."), false);
        }

        void setTitle (const juce::String& title)
        {
            setName (title);
        }

        ~LogWindow() override
        {
            clearContentComponent();   // the editor is a member: let go of it before it goes
        }

        void closeButtonPressed() override    { setVisible (false); }

    private:
        juce::TextEditor text;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LogWindow)
    };

    // Every plugin starts with its exact type (the New plugin dialog's, or the Type selector's
    // beside Send): an instrument, an effect, a MIDI FX (one of Sonify's MIDI devices, built here
    // by its own Sonify AI) or a widget, for anything else. In the selector's order.
    const juce::StringArray kindNames { "Instrument", "Effect", "MIDI FX", "Widget" };
    constexpr int midiFxIndex = 2;

    PluginKind kindAt (int index)
    {
        return index == 1 ? PluginKind::effect : index == 3 ? PluginKind::widget : PluginKind::instrument;
    }

    /** The open plugin's type in the selector beside Send. */
    ChatPanel::BuildType buildTypeOf (PluginKind kind)
    {
        switch (kind)
        {
            case PluginKind::effect:     return ChatPanel::BuildType::effect;
            case PluginKind::noteEffect: return ChatPanel::BuildType::midiFx;
            case PluginKind::widget:     return ChatPanel::BuildType::widget;
            case PluginKind::instrument: break;
        }

        return ChatPanel::BuildType::instrument;
    }

    // Ctrl+1 .. Ctrl+9: Sonify's studios, in its selector's order.
    const char* const studioCommands[] { "showStudio", "analogBoard", "sonifyAi", "songBlocks", "pluginsCreator", "liveShow",
                                         "samplingStudio", "webBrowser", "guide" };
}

//==============================================================================
CreatorStudio::CreatorStudio (AudioEngine& sonifyEngine, FananServer& fananServer)
    : server (fananServer),
      audio (sonifyEngine)
{
    project.onChanged = [this] { projectChanged(); };

    topBar.onNew = [this] { newProject(); };
    topBar.onOpen = [this] { openProject(); };
    topBar.onSave = [this] { saveProject(); };
    topBar.onSaveAs = [this] { saveProjectAs(); };
    topBar.takePeak = [this] (int channel) { return audio.takePeak (channel); };
    topBar.audioLoad = [this] { return audio.getCpuLoad(); };
    workspace.onModeChanged = [this] (bool play) { setPlayMode (play); };
    topBar.onOpenRecent = [this] (const juce::File& file) { openProjectFile (file); };
    topBar.onRevealProject = [this]
    {
        if (isMakingMidiFx())
        {
            const auto file = devices->getFile();

            if (file.existsAsFile())
                file.revealToUser();
            else
                showError ("Not saved yet", "The MIDI FX is saved in My Devices as soon as Fanan AI starts on it.");
        }
        else if (project.isOpen())
        {
            project.getProjectFile().revealToUser();
        }
    };
    topBar.onCloseProject = [this]
    {
        if (refuseWhileBusy())
            return;

        if (isMakingMidiFx())
        {
            closeMidiFx();
            return;
        }

        flushLayoutSave();
        project.close();
    };
    topBar.isFinishSoundOn = [this] { return settings.getFinishSound(); };
    topBar.onToggleFinishSound = [this]
    {
        settings.setFinishSound (! settings.getFinishSound());

        if (settings.getFinishSound())
            playFinishSound();   // so it's heard what it sounds like
    };

    topBar.recentProjects = [this] { return settings.getRecentProjects(); };
    topBar.onSampleLibrary = [this] { showLibrary(); };

    // MIDI In: the ports the studio hears while it's in front (kept), and its light.
    sonifyEngine.setMidiInInputs (AudioEngine::MidiIn::creator, settings.getMidiInputs());
    topBar.midiInPorts = [&engine = sonifyEngine]
    {
        std::vector<TopBar::MidiPort> ports;

        for (const auto& port : engine.getMidiInPorts())
            ports.push_back ({ port.name, port.device });

        return ports;
    };
    topBar.midiInChosen = [&engine = sonifyEngine] { return engine.getMidiInInputs (AudioEngine::MidiIn::creator); };
    topBar.onMidiInChosen = [this, &engine = sonifyEngine] (const juce::StringArray& ports)
    {
        engine.setMidiInInputs (AudioEngine::MidiIn::creator, ports);
        settings.setMidiInputs (ports);
    };
    topBar.midiInActivity = [&engine = sonifyEngine] { return engine.getMidiInActivity (AudioEngine::MidiIn::creator); };
    topBar.refreshMidiIn();

    chat.onNewPlugin = [this] { newProject(); };
    chat.onOpenPlugin = [this] { openProject(); };

    audio.onDeviceChanged = [this] { preview.deviceChanged(); };

    preview.onChanged = [this] { previewChanged(); };
    preview.onElements = [this] (std::shared_ptr<WasmElements> elements) { workspace.setElements (std::move (elements)); };
    workspace.onElementsFailed = [this] (const juce::String& why) { preview.setElementsProblem (why); };
    workspace.onShowLogRequested = [this] { showBuildLog(); };
    workspace.onExportRequested = [this] { addToSonify(); };
    workspace.onExportPluginRequested = [this] { exportPlugin(); };
    workspace.onPanicRequested = [this] { audio.allNotesOff(); };
    workspace.onParameterChanged = [this] (int index, float value) { parameterMoved (index, value); };

    // A keyboard element in the GUI plays the plugin like the studio's own keyboard state
    // (a MIDI keyboard's notes light its keys up too).
    workspace.onNote = [this] (int note, float velocity)
    {
        if (velocity > 0.0f)
            audio.getKeyboardState().noteOn (1, note, velocity);
        else
            audio.getKeyboardState().noteOff (1, note, 0.0f);
    };
    workspace.isNoteDown = [this] (int note) { return audio.getKeyboardState().isNoteOnForChannels (0xffff, note); };
    workspace.onLayoutEdited = [this] { saveLayoutSoon(); };

    // The GUI's meters, lamps and scopes read the plugin live.
    workspace.readLevel = [this] (const juce::String& source, bool rms) { return preview.takeLevel (source, rms); };
    workspace.readScope = [this] (const juce::String& source, float* destination, int numSamples) { preview.readScope (source, destination, numSamples); };
    workspace.onSourcesChanged = [this] (const juce::StringArray& levels, const juce::StringArray& scopes) { preview.setGuiSources (levels, scopes); };
    workspace.onAutoLayoutRequested = [this] { autoLayout(); };
    projectTools.onLayoutChanged = [this] { loadLayout (false); };
    projectTools.onPresetsChanged = [this] { presets.refresh(); pushPresets(); };
    projectTools.onCreateProject = [this] (const juce::String& name, PluginKind kind)
    {
        ProjectInfo info;
        info.name = name;
        info.vendor = settings.getVendor().isNotEmpty() ? settings.getVendor() : juce::String ("Made in Sonify");
        info.kind = kind;

        flushLayoutSave();   // the open project's last GUI change, before it goes
        const auto created = project.create (settings.getProjectsFolder(), info);

        if (created.wasOk())
            settings.addRecentProject (project.getProjectFile());

        return created;
    };

    // Undo: snapshots around each request to Sonify AI, and after each edit (Ctrl+Z / Ctrl+Y).
    ai.onTurnStarted = [this] (const juce::String&)
    {
        flushLayoutSave();   // Sonify AI sees the GUI as the user left it, and undo starts from there
        recordHistory ("Changes");
    };
    ai.onTurnFinished = [this] (const juce::String& request)
    {
        if (settings.getFinishSound())
            playFinishSound();

        recordHistory ("Fanan AI: " + (request.length() > 40 ? request.substring (0, 40) + juce::String::fromUTF8 ("\xe2\x80\xa6") : request));

        // A request from an Edit UI menu: its answer shows there too. A plan waiting for
        // approval is in Build with AI, where its price and Start are.
        if (guiRequestPending)
        {
            guiRequestPending = false;

            if (ai.getWaiting() == StellaAi::Waiting::approval)
            {
                workspace.showAiReply ("The plan and its price are in Build with AI: start it there.");
                workspace.showTab (Workspace::aiTab);
            }
            else
            {
                workspace.showAiReply (lastAnswer());
            }
        }
    };

    // An edit menu's instruction for Sonify AI, about one element of the GUI or the window.
    workspace.onAskAiAboutGui = [this] (int widgetIndex, const juce::String& instruction) { askAiAboutGui (widgetIndex, instruction); };

    // The Edit tab repaints a painted background when its material, colours or pattern change.
    workspace.onPaintPicture = [this] (const juce::var& recipe, std::function<void (const juce::String&)> done)
    {
        projectTools.run ("paint_picture", recipe, [done] (const juce::String& result, bool isError)
        {
            if (done != nullptr)
                done (isError ? result : juce::String());
        });
    };

    // The preset manager: Play's row and the panel's preset managers.
    presetUi.getValues = [this] { return currentValues(); };
    presetUi.apply = [this] (const Preset& preset)
    {
        applyValues (preset.values);
        playStateDirty = true;
    };
    presetUi.changed = [this]
    {
        pushPresets();
        playStateDirty = true;
        recordHistory ("Changed the presets");   // factory presets are part of the plugin
    };
    workspace.onPresetChosen = [this] (int index) { presetUi.choose (index); };
    workspace.onPresetAction = [this] (PresetAction action)
    {
        const bool saving = action == PresetAction::save || action == PresetAction::saveAs || action == PresetAction::saveFactory;

        if (saving && preview.getParameters().isEmpty())
        {
            showError ("Nothing to save yet", "Build the plugin first: a preset holds its parameter values.");
            return;
        }

        presetUi.perform (action);
    };

    setWantsKeyboardFocus (true);

    ai.onChanged = [this] { refreshChat(); };

    // Sonify AI's estimates learn what plan steps really cost.
    ai.averageStepCost = [this] (const juce::String& size) { return settings.getStepCost (size); };
    ai.onStepCost = [this] (const juce::String& size, int credits) { settings.addStepCost (size, credits); };

    chat.onPlanStart = [this] { ai.approvePlan(); };
    chat.onPlanCancel = [this] { ai.cancelPlan(); };
    chat.onPlanGoOn = [this] { ai.continueStep(); };
    chat.onPlanPause = [this] { ai.pausePlan(); };

    chat.onSend = [this] (const juce::String& request, const juce::Array<juce::File>& files) { sendFromChat (request, files); };
    chat.onLookPicked = [this] (const juce::String& look) { ai.setPickedLook (look); };
    chat.onStop = [this]
    {
        if (isMakingMidiFx())
            deviceAi->stop();
        else
            ai.stop();
    };
    chat.onRetry = [this] { server.checkNow(); };
    chat.onBuyCredits = [this] { buyCredits(); };
    chat.onSignIn = [this] (const juce::String& email) { signIn (false, email); };
    chat.onSignOut = [this] { signOut(); };

    // The tabs, picked in the top bar: Build with AI (the chat), Edit UI, Knob Studio and
    // Protection. The workspace shows one at a time; the top bar follows it.
    workspace.setAiPage (chat);
    workspace.setProtectionPage (protection);

    // A MIDI FX's protection changed: its file is saved again, so it carries its license.soni
    // (DeviceFile writes it from the protection on its maker's computer).
    protection.onChanged = [this]
    {
        if (isMakingMidiFx())
            devices->setVendor (devices->getDevice().vendor);
    };

    // The Store: what's added from it goes into Sonify like Add to Sonify's plugins (or My
    // Devices).
    workspace.setStorePage (storePage);
    store.onPluginInstalled = [this] (const juce::File& folder, const juce::String& name, bool replaced)
    {
        if (onPluginAdded != nullptr)
            onPluginAdded (folder, name, replaced);
    };
    store.onDeviceInstalled = [this] (const juce::String&) { topBar.showNotice ("Added to My Devices"); };

    // Its ad form posts for what's open here: its picture, its story's facts, its Sonify version.
    storePage.getSubject = [this] { return storeSubject(); };
    storePage.takePicture = [this] (juce::String& problem) { return storePicture (problem); };
    storePage.storyFacts = [this] { return storeFacts(); };
    storePage.makeItem = [this] (StorePublisher::Item& item, juce::String& problem) { return makeStoreItem (item, problem); };
    storePage.onNotice = [this] (const juce::String& text) { topBar.showNotice (text); };

    topBar.onTabChosen = [this] (int index) { showTab (index); };
    workspace.onTabChanged = [this] (int index)
    {
        currentTab = index;
        topBar.setCurrentTab (index);

        if (index == Workspace::aiTab)
            chat.focusInput();

        if (index == Workspace::storeTab)
            storePage.studioChanged();   // the ad form follows what's open here

        if (index == Workspace::storeTab && ! storeLoaded)
        {
            storeLoaded = true;
            storePage.refresh();
        }
    };

    addAndMakeVisible (topBar);
    addAndMakeVisible (workspace);

    workspace.showTab (Workspace::aiTab);

    // The studio starts with no project open: the user opens one (recent projects are in the
    // Options menu) or asks Sonify AI for a plugin.
    projectChanged();
    connectionChanged();
}

CreatorStudio::~CreatorStudio()
{
    flushLayoutSave();   // a GUI change made just before quitting isn't lost

    presetUi.getValues = nullptr;
    presetUi.apply = nullptr;
    presetUi.changed = nullptr;
    project.onChanged = nullptr;
    audio.onDeviceChanged = nullptr;
    preview.onChanged = nullptr;
    preview.onElements = nullptr;
    logWindow = nullptr;
    libraryWindow = nullptr;
    exportDialog = nullptr;
    exportOptions = nullptr;
    ai.onChanged = nullptr;
    ai.averageStepCost = nullptr;
    ai.onStepCost = nullptr;
    ai.onTurnStarted = nullptr;
    ai.onTurnFinished = nullptr;
    newProjectDialog = nullptr;
    saveAsDialog = nullptr;
    signOutDialog = nullptr;
    autoLayoutDialog = nullptr;
    protection.onChanged = nullptr;
    store.onPluginInstalled = nullptr;
    store.onDeviceInstalled = nullptr;
    storePage.getSubject = nullptr;
    storePage.takePicture = nullptr;
    storePage.storyFacts = nullptr;
    storePage.makeItem = nullptr;
    storePage.onNotice = nullptr;
}

//==============================================================================
void CreatorStudio::paint (juce::Graphics& g)
{
    g.fillAll (Theme::window);
}

void CreatorStudio::resized()
{
    auto area = getLocalBounds();

    topBar.setBounds (area.removeFromTop (TopBar::height));
    workspace.setBounds (area);
}

//==============================================================================
void CreatorStudio::setShown (bool shown)
{
    if (shown == shownInSonify)
        return;

    shownInSonify = shown;
    updateListening();

    // Typing goes to Sonify AI here once the studio comes to the front.
    if (shown && currentTab == Workspace::aiTab)
        juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<CreatorStudio> (this)]
        {
            if (safeThis != nullptr && safeThis->shownInSonify && safeThis->currentTab == Workspace::aiTab)
                safeThis->chat.focusInput();
        });
}

void CreatorStudio::showTab (int index)
{
    workspace.showTab ((Workspace::TabIndex) juce::jlimit (0, TopBar::numTabs - 1, index));   // onTabChanged follows
}

void CreatorStudio::updateListening()
{
    // While a MIDI FX is open, the live notes go to the tracks: it plays on its instrument track.
    audio.setListening (shownInSonify && ! isMakingMidiFx());
}

//==============================================================================
void CreatorStudio::projectChanged()
{
    if (project.isOpen())
    {
        // One thing is open at a time: a MIDI FX that was open steps aside.
        closeMidiFx();

        const auto& info = project.getInfo();

        workspace.setProject (info.name, Project::kindDisplayName (info.kind));
        projectTools.setChosenKind (info.kind);
        settings.addRecentProject (project.getProjectFile());

        // A newly opened project starts playing at once; a new one stays empty until Sonify AI
        // builds it.
        if (info.uuid != builtProjectId)
        {
            builtProjectId = info.uuid;

            libraryWindow = nullptr;   // another plugin's library
            applyLibrary();

            workspace.setGuiFolder (project.getGuiFolder());
            loadLayout (false);

            history.reset (project.getFolder());

            // Its presets (factory ones in the project, the user's on the hard drive), and the
            // preset and values it was left with, once it plays.
            presets.setPlugin (info.name, project.getFolder().getChildFile (PresetBank::fileName), true);
            playStatePending = true;
            playStateDirty = false;
            pushPresets();
            buildPlugin();
        }
        else if (! presets.getFolder().getFileName().equalsIgnoreCase (juce::File::createLegalFileName (info.name).trim()))
        {
            // Renamed (Save as): the user's presets are the new name's.
            presets.setPlugin (info.name, project.getFolder().getChildFile (PresetBank::fileName), true);
            pushPresets();
        }
    }
    else
    {
        workspace.setProject ({}, {});
        projectTools.setChosenKind (std::nullopt);

        builtProjectId.clear();
        preview.unload();
        libraryWindow = nullptr;
        preview.setLibrary ({});
        workspace.setGuiFolder ({});
        workspace.setLayout ({});

        history.reset ({});
        presets.setPlugin ({}, {}, false);
        playStatePending = false;
        playStateDirty = false;
        addWhenPlaying = false;
        pushPresets();
    }

    refreshIdentity();
    updateListening();
}

//==============================================================================
bool CreatorStudio::isMakingMidiFx() const noexcept
{
    return devices != nullptr && deviceAi != nullptr && devices->isOpen();
}

bool CreatorStudio::midiFxBusy() const
{
    return deviceAi != nullptr && deviceAi->isBusy();
}

bool CreatorStudio::refuseWhileBusy()
{
    if (ai.isBusy())
    {
        showError ("Fanan AI is still working", "It's still working on this plugin: once it's done (or stopped), go on.");
        return true;
    }

    if (midiFxBusy())
    {
        showError ("Fanan AI is still working", "It's still working on the MIDI FX: once it's done (or stopped), go on.");
        return true;
    }

    return false;
}

void CreatorStudio::setMidiDevices (DeviceStudio& studio, AiAgent& builder)
{
    devices = &studio;
    deviceAi = &builder;
    store.setDevices (&studio);   // a MIDI FX from the store goes into My Devices
    midiDeviceChanged();
}

void CreatorStudio::midiDeviceChanged()
{
    // A MIDI FX that opened (from a track, or a new one) takes the place of the plugin project.
    if (isMakingMidiFx() && project.isOpen() && ! ai.isBusy())
    {
        flushLayoutSave();
        project.close();   // projectChanged follows
    }

    refreshIdentity();
    refreshChat();
    updateListening();
}

void CreatorStudio::showMidiDevice()
{
    midiDeviceChanged();

    if (isMakingMidiFx())
        showTab (Workspace::aiTab);
}

juce::Result CreatorStudio::startMidiFx (const juce::String& name, const juce::String& developer, const juce::String& request,
                                         const juce::Array<juce::File>& files)
{
    if (devices == nullptr || deviceAi == nullptr)
        return juce::Result::fail ("MIDI FX can't be made in this Sonify.");

    if (ai.isBusy())
        return juce::Result::fail ("Fanan AI is still working on the plugin that's open: once it's done (or stopped), start the MIDI FX.");

    if (midiFxBusy())
        return juce::Result::fail ("Fanan AI is still working on a MIDI FX: once it's done (or stopped), start the next one.");

    if (project.isOpen())
    {
        flushLayoutSave();
        project.close();
    }

    // A new device, its conversation fresh; its file is made at its first change.
    devices->newDevice();
    deviceAi->restoreConversation ({});

    if (const auto clean = name.trim().replace ("My Plugin", {}).trim(); clean.isNotEmpty())
        devices->setInfo (clean, {});

    devices->setVendor (developer.trim().isNotEmpty() ? developer.trim() : settings.getVendor());

    if (request.isNotEmpty() || ! files.isEmpty())
        sendToMidiFx (request, files);

    showMidiDevice();
    return juce::Result::ok();
}

void CreatorStudio::sendToMidiFx (const juce::String& request, const juce::Array<juce::File>& files)
{
    // The MIDI FX's Fanan AI works from words and code: the user's own code (code files, a zip,
    // a folder) is kept with the device, read only, and the AI is told what's there and reads
    // what it needs. Pictures and PDFs aren't for a MIDI FX.
    juce::Array<juce::File> code;
    juce::StringArray notSent, attached;

    for (const auto& file : files)
    {
        if (CodeReference::canTake (file))
            code.add (file);
        else
            notSent.add (file.getFileName());
    }

    juce::String context;

    if (! code.isEmpty())
    {
        const auto added = devices->addReference (code);

        if (! added.problems.isEmpty())
            deviceAi->addNotice ("Not kept: " + added.problems.joinIntoString (", ") + ".");

        if (! added.paths.isEmpty())
        {
            for (const auto& file : code)
                attached.add (file.getFileName() + (file.isDirectory() ? "/" : ""));

            context = "\n\n(The user brought their own code: " + juce::String (added.paths.size())
                    + (added.paths.size() == 1 ? " file" : " files") + ", saved read only:\n"
                    + devices->getReference().listing (40, added.paths)
                    + "Read what you need with read_reference (a long file a part at a time) and search_reference, and port it as the "
                      "guide's \"The user's own code\" says.)";
        }
    }

    if (! notSent.isEmpty())
        deviceAi->addNotice ("Not sent: " + notSent.joinIntoString (", ") + " (a MIDI FX takes your code, not pictures or PDFs).");

    auto text = request.trim();

    if (text.isEmpty() && context.isEmpty())
        return;

    if (text.isEmpty())
        text = "Have a look at my code.";

    if (! attached.isEmpty())
        text << juce::String::fromUTF8 ("\n\xf0\x9f\x93\x8e ") << attached.joinIntoString (", ");

    deviceAi->send (text, AiAgent::Origin::typed, {}, context);
}

juce::Result CreatorStudio::openMidiFxFile (const juce::File& file)
{
    if (devices == nullptr || deviceAi == nullptr)
        return juce::Result::fail ("MIDI FX can't be opened in this Sonify.");

    if (const auto opened = devices->open (file); opened.failed())
        return opened;

    if (project.isOpen())
    {
        flushLayoutSave();
        project.close();
    }

    deviceAi->restoreConversation (devices->getDevice().chat);
    showMidiDevice();
    return juce::Result::ok();
}

void CreatorStudio::closeMidiFx()
{
    if (! isMakingMidiFx() || midiFxBusy())
        return;

    devices->close();
    deviceAi->restoreConversation ({});
    refreshIdentity();
    refreshChat();
    updateListening();
}

void CreatorStudio::saveMidiFxCopy()
{
    if (! isMakingMidiFx())
        return;

    const auto suggested = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                               .getChildFile (juce::File::createLegalFileName (devices->getDevice().name) + Devices::fileExtension);

    chooser = std::make_unique<juce::FileChooser> ("Save a copy of the MIDI FX", suggested, juce::String ("*") + Devices::fileExtension);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safeThis = juce::Component::SafePointer<CreatorStudio> (this)] (const juce::FileChooser& fc)
                          {
                              if (safeThis == nullptr || fc.getResult() == juce::File() || ! safeThis->isMakingMidiFx())
                                  return;

                              const auto target = fc.getResult().withFileExtension (Devices::fileExtension);

                              if (const auto written = safeThis->devices->getDevice().write (target); written.failed())
                                  safeThis->showError ("Couldn't save a copy", written.getErrorMessage());
                              else
                                  safeThis->topBar.showNotice ("Saved a copy");
                          });
}

void CreatorStudio::refreshIdentity()
{
    // A MIDI FX: Sonify's MIDI device, built in Build with AI. It has no panel to design.
    if (isMakingMidiFx())
    {
        const auto& device = devices->getDevice();
        const auto developer = device.vendor.isNotEmpty() ? device.vendor : settings.getVendor();

        topBar.setProjectName (device.name);
        chat.setBuildType (ChatPanel::BuildType::midiFx, true);
        chat.setPluginInfo (device.name, "MIDI FX", developer);
        workspace.setNotice ("A MIDI FX has no panel to design",
                             "Fanan AI builds it in Build with AI, and every build plays at once on your instrument track (it's in the "
                             "tracks' MIDI FX menu too, under My Devices). Its settings show in its window there.");
        refreshProtection();
        return;
    }

    workspace.setNotice ({}, {});

    if (project.isOpen())
    {
        const auto& info = project.getInfo();

        // Its type, locked beside Send: what's built here is that.
        topBar.setProjectName (info.name);
        chat.setBuildType (buildTypeOf (info.kind), true);
        chat.setPluginInfo (info.name, Project::kindDisplayName (info.kind), info.vendor);
        refreshProtection();
        return;
    }

    // Nothing open: the next plugin's type is chosen afresh, before anything is built.
    topBar.setProjectName ({});
    chat.setBuildType (ChatPanel::BuildType::none, false);
    chat.setPluginInfo ({}, {}, {});
    refreshProtection();
}

void CreatorStudio::refreshChat()
{
    if (isMakingMidiFx())
    {
        chat.setConversation (midiFxConversation(), deviceAi->isBusy());
        chat.setPlanBar (StellaAi::Waiting::nothing, {}, false);
        return;
    }

    chat.setConversation (ai.getEntries(), ai.isBusy());
    chat.setPlanBar (ai.getWaiting(), ai.getWaitingText(), ai.isWaitingWarning());
}

std::vector<StellaAi::Entry> CreatorStudio::midiFxConversation() const
{
    // The device's Sonify AI, in the chat's own terms: requests, its steps, answers and notes.
    std::vector<StellaAi::Entry> shown;
    const auto state = deviceAi->getState();

    if (const auto* list = state.getProperty ("entries", {}).getArray())
    {
        for (const auto& entry : *list)
        {
            const auto role = entry.getProperty ("role", {}).toString();
            const auto text = entry.getProperty ("text", {}).toString();

            if (role == "user")
            {
                shown.push_back ({ StellaAi::Entry::Kind::user, text });
                continue;
            }

            if (role == "notice")
            {
                shown.push_back ({ StellaAi::Entry::Kind::notice, text });
                continue;
            }

            if (const auto* steps = entry.getProperty ("steps", {}).getArray())
                for (const auto& step : *steps)
                    if (const auto line = step.getProperty ("text", {}).toString(); line.isNotEmpty())
                        shown.push_back ({ StellaAi::Entry::Kind::activity,
                                           (static_cast<bool> (step.getProperty ("ok", true)) ? juce::String() : juce::String ("Didn't work: ")) + line });

            if (text.isNotEmpty())
                shown.push_back ({ StellaAi::Entry::Kind::ai, text });
        }
    }

    return shown;
}

void CreatorStudio::buildPlugin()
{
    if (! project.isOpen())
        return;

    // Nothing to build yet: stop whatever was playing before.
    if (hasNothingToBuild (project))
    {
        preview.unload();
        return;
    }

    preview.build (project.getFolder(), project.getInfo().uuid, [this, folder = project.getFolder()] (bool ok)
    {
        // Built and playing: from now on Fanan AI's usual builder works on it.
        if (ok && project.isOpen() && project.getFolder() == folder)
            project.markBuilt();
    });
}

void CreatorStudio::previewChanged()
{
    const auto state = preview.getState();
    const bool empty = state == LivePreview::State::idle && project.isOpen() && hasNothingToBuild (project);

    workspace.setBuild (state == LivePreview::State::building ? Workspace::BuildState::building
                      : state == LivePreview::State::playing  ? Workspace::BuildState::playing
                      : state == LivePreview::State::failed   ? Workspace::BuildState::failed
                                                              : Workspace::BuildState::idle,
                        empty ? juce::String (emptyProjectStatus) : preview.getStatus());

    // The knobs are rebuilt only when the plugin's parameter list changed.
    if (preview.getParametersVersion() != shownParamsVersion)
    {
        shownParamsVersion = preview.getParametersVersion();

        pushParameters();

        // What the GUI can watch: every module output, the plugin's own audio, and displays.
        juce::StringArray signals { "plugin.out L", "plugin.out R" }, displays;

        if (project.isOpen() && (project.getInfo().kind == PluginKind::effect || project.getInfo().kind == PluginKind::widget))
            signals.addArray ({ "plugin.in L", "plugin.in R" });

        for (const auto& m : preview.getModules())
            for (const auto& out : m.outputs)
                signals.add (m.id + "." + out);

        for (const auto& d : preview.getDisplays())
            displays.add (d.id);

        workspace.setSources (signals, displays);

        // A plugin's first build gets a plain automatic GUI, for Sonify AI or the user to reshape.
        loadLayout (state == LivePreview::State::playing);
    }

    // The project just opened plays: the preset and values it was left with come back.
    if (playStatePending && state == LivePreview::State::playing && ! preview.getParameters().isEmpty())
        restorePlayState();

    // Add to Sonify, pressed while it built: once it plays (or why not, when it didn't build).
    if (addWhenPlaying && state != LivePreview::State::building)
    {
        addWhenPlaying = false;

        if (state == LivePreview::State::playing)
            installInSonify();
        else if (state == LivePreview::State::failed)
            showError ("It didn't build", "So it can't go into Sonify yet. Ask Fanan AI to fix it (Log shows what the compiler says).");
    }

    // Only while it shows the build's log (an export's stays as it is).
    if (logWindow != nullptr && logWindow->getName() == "Build log")
        static_cast<LogWindow*> (logWindow.get())->setLog (preview.getLog());

    refreshProtection();   // its panel can be taken once it plays
}

void CreatorStudio::loadLayout (bool makeOneIfMissing)
{
    if (! project.isOpen())
        return;

    const auto file = project.getGuiFolder().getChildFile (GuiLayout::fileName);
    GuiLayout layout;

    if (GuiLayout::load (file, layout).wasOk())
    {
        if (syncLibraryLists (layout))
            layout.save (file);

        workspace.setLayout (layout);
        return;
    }

    if (! makeOneIfMissing || preview.getParameters().isEmpty())
    {
        workspace.setLayout ({});
        return;
    }

    juce::Array<GuiLayout::ParamInfo> infos;

    for (const auto& p : preview.getParameters())
        infos.add ({ p.id, p.module, p.name, p.unit, p.min, p.max, p.def });

    layout = GuiLayout::makeDefault (infos, project.getInfo().name);
    layout.save (file);
    workspace.setLayout (layout);
    history.absorb();   // the studio made it, not the user: nothing to undo
}

void CreatorStudio::saveLayout()
{
    if (! project.isOpen())
        return;

    // An Instrument list just dropped shows the library's names.
    if (auto layout = workspace.getLayout(); syncLibraryLists (layout))
        workspace.setLayout (layout);

    if (const auto saved = workspace.getLayout().save (project.getGuiFolder().getChildFile (GuiLayout::fileName)); saved.failed())
    {
        showError ("Couldn't save the GUI", saved.getErrorMessage());
    }
    else
    {
        recordHistory ("Changed the GUI");
    }
}

//==============================================================================
void CreatorStudio::pushParameters()
{
    juce::Array<Workspace::ParamControl> controls;

    for (const auto& p : preview.getParameters())
    {
        Workspace::ParamControl control;
        control.index = p.index;
        control.id = p.id;
        control.name = p.name;
        control.unit = p.unit;
        control.min = p.min;
        control.max = p.max;
        control.def = p.def;
        control.skew = p.skew;
        control.value = preview.getParameterValue (p.index);
        controls.add (control);
    }

    workspace.setParameters (controls);
}

std::map<juce::String, float> CreatorStudio::currentValues() const
{
    std::map<juce::String, float> values;

    for (const auto& p : preview.getParameters())
        values[p.id] = preview.getParameterValue (p.index);

    return values;
}

void CreatorStudio::applyValues (const std::map<juce::String, float>& values)
{
    for (const auto& p : preview.getParameters())
        if (const auto found = values.find (p.id); found != values.end())
            preview.setParameterValue (p.index, juce::jlimit (p.min, p.max, found->second));

    pushParameters();
}

void CreatorStudio::pushPresets()
{
    workspace.setPresets (presets.getMenu());
}

void CreatorStudio::parameterMoved (int index, float value)
{
    preview.setParameterValue (index, value);
    playStateDirty = true;

    // The chosen preset's values aren't its own any more: it shows a *.
    if (presets.getCurrent() >= 0 && ! presets.isModified())
    {
        presets.setModified (true);
        pushPresets();
    }
}

void CreatorStudio::restorePlayState()
{
    playStatePending = false;

    if (! project.isOpen())
        return;

    const auto state = juce::JSON::parse (project.getFolder().getChildFile (playStateFileName));

    if (const auto* values = state.getProperty ("values", {}).getDynamicObject())
    {
        // As it was left: its values, and the preset that was chosen (marked when they had changed).
        std::map<juce::String, float> saved;

        for (const auto& entry : values->getProperties())
            saved[entry.name.toString()] = (float) (double) entry.value;

        applyValues (saved);
        presets.restore (state.getProperty ("preset", {}).toString(), state.getProperty ("group", {}).toString(),
                         static_cast<bool> (state.getProperty ("modified", false)));
        pushPresets();
    }
    else if (! presets.isEmpty())
    {
        // The first time: the first preset.
        presetUi.choose (0);
    }

    playStateDirty = false;
}

void CreatorStudio::savePlayState()
{
    if (! playStateDirty || ! project.isOpen() || preview.getParameters().isEmpty())
        return;

    playStateDirty = false;

    juce::DynamicObject::Ptr values (new juce::DynamicObject());

    for (const auto& [id, value] : currentValues())
        values->setProperty (id, (double) value);

    juce::DynamicObject::Ptr state (new juce::DynamicObject());
    const auto* chosen = presets.getCurrentEntry();
    state->setProperty ("preset", chosen != nullptr ? chosen->preset.name : juce::String());
    state->setProperty ("group", chosen != nullptr ? chosen->group : juce::String());
    state->setProperty ("modified", presets.isModified());
    state->setProperty ("values", juce::var (values.get()));

    project.getFolder().getChildFile (playStateFileName).replaceWithText (juce::JSON::toString (juce::var (state.get())));
}

//==============================================================================
void CreatorStudio::recordHistory (const juce::String& label)
{
    if (project.isOpen())
        history.record (label);
}

void CreatorStudio::undoOrRedo (bool redo)
{
    if (! project.isOpen() || ai.isBusy())
        return;   // never under Sonify AI's feet

    // A GUI edit still waiting to be written is a step of its own first (and isn't written over
    // what's put back later).
    flushLayoutSave();

    const auto changed = redo ? history.redo() : history.undo();

    if (! changed.any())
        return;

    if (changed.layout)
        loadLayout (false);

    if (changed.presets)
    {
        presets.refresh();
        pushPresets();
    }

    if (changed.code)
        buildPlugin();
}

bool CreatorStudio::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();

    // Ctrl+S saves, Ctrl+Shift+S saves a copy: from anywhere.
    if (mods.isCommandDown() && (key.getKeyCode() == 'S' || key.getKeyCode() == 's'))
    {
        if (mods.isShiftDown())
            saveProjectAs();
        else
            saveProject();

        return true;
    }

    if (sonifyKey (key))
        return true;

    // Text boxes keep their own undo.
    if (dynamic_cast<juce::TextEditor*> (juce::Component::getCurrentlyFocusedComponent()) != nullptr)
        return false;

    if (mods.isCommandDown() && (key.getKeyCode() == 'Z' || key.getKeyCode() == 'z'))
    {
        undoOrRedo (mods.isShiftDown());
        return true;
    }

    if (mods.isCommandDown() && (key.getKeyCode() == 'Y' || key.getKeyCode() == 'y'))
    {
        undoOrRedo (true);
        return true;
    }

    return false;
}

bool CreatorStudio::sonifyKey (const juce::KeyPress& key)
{
    // Sonify's own keys work here too: Ctrl+1 .. Ctrl+8 a studio, Ctrl+Tab / Ctrl+Shift+Tab the
    // next / previous one (the page's selector does it).
    const auto mods = key.getModifiers();

    if (onPageCommand == nullptr || ! mods.isCtrlDown() || mods.isAltDown())
        return false;

    if (key.getKeyCode() == juce::KeyPress::tabKey)
    {
        onPageCommand (mods.isShiftDown() ? "previousStudio" : "nextStudio");
        return true;
    }

    const auto code = key.getKeyCode();

    if (! mods.isShiftDown() && code >= '1' && code < '1' + (int) std::size (studioCommands))
    {
        onPageCommand (studioCommands[code - '1']);
        return true;
    }

    return false;
}

void CreatorStudio::saveLayoutSoon()
{
    // Knob Studio edits arrive many times a second: the file is written once they pause.
    if (layoutSaveScheduled)
        return;

    layoutSaveScheduled = true;

    juce::Timer::callAfterDelay (400, [safeThis = juce::Component::SafePointer<CreatorStudio> (this)]
    {
        if (safeThis != nullptr)
            safeThis->flushLayoutSave();
    });
}

void CreatorStudio::flushLayoutSave()
{
    savePlayState();   // the chosen preset and the values too, when they changed

    if (! layoutSaveScheduled)
        return;

    layoutSaveScheduled = false;
    saveLayout();
}

void CreatorStudio::askAiAboutGui (int widgetIndex, const juce::String& instruction)
{
    if (! project.isOpen() || instruction.trim().isEmpty())
        return;

    if (ai.isBusy())
    {
        workspace.showAiReply ("I'm still working on the last request. Send this again when I'm done.");
        return;
    }

    // Sonify AI gets the element as it is in gui/layout.json; the conversation shows the
    // request in plain words.
    const auto& layout = workspace.getLayout();
    juce::String context, shown, what;

    if (juce::isPositiveAndBelow (widgetIndex, (int) layout.widgets.size()))
    {
        const auto& w = layout.widgets[(size_t) widgetIndex];
        const auto kind = GuiLayout::typeName (w.type);
        const auto name = w.type == GuiWidget::Type::image ? w.image : w.label;

        context = "About one element of the plugin's GUI: index " + juce::String (widgetIndex) + " in the project's GUI elements list (the "
                + kind + (name.isNotEmpty() ? " \"" + name + "\"" : juce::String()) + "):\n"
                + juce::JSON::toString (GuiLayout::widgetToVar (w), true)
                + "\nChange that element (and only what's needed around it) with edit_layout, keeping everything else as it is. "
                  "The user's words about it:\n" + instruction.trim();

        shown = "The " + kind + (name.isNotEmpty() ? " \"" + name + "\"" : juce::String()) + ": " + instruction.trim();
        what = "the " + kind + (name.isNotEmpty() ? " \"" + name + "\"" : juce::String());
    }
    else
    {
        context = "About the plugin's whole window (gui/layout.json: " + juce::String (layout.width) + " x " + juce::String (layout.height)
                + (layout.backgroundPicture().isNotEmpty() ? ", background picture " + layout.backgroundPicture() : juce::String())
                + "). Change it with edit_layout, keeping what the user arranged (set_layout only if they ask for a new design). The user's words about it:\n"
                + instruction.trim();

        shown = "The plugin window: " + instruction.trim();
        what = "the plugin window";
    }

    ai.send (context, {}, shown);

    if (ai.isBusy())
    {
        guiRequestPending = true;
        workspace.showAiWorking (what);
    }
    else
    {
        workspace.showAiReply (lastAnswer());   // it couldn't go (no connection, no credits...)
    }
}

void CreatorStudio::sendFromChat (const juce::String& request, const juce::Array<juce::File>& files)
{
    // A MIDI FX that's open: to its own Sonify AI, which reads words and the user's own code.
    if (isMakingMidiFx())
    {
        sendToMidiFx (request, files);
        refreshChat();
        return;
    }

    // A new plugin: its type, chosen beside Send (the panel won't send without one), is what's
    // built. A MIDI FX is one of Sonify's MIDI devices, built here by its own Sonify AI.
    if (! project.isOpen())
    {
        const auto type = chat.getBuildType();

        if (type == ChatPanel::BuildType::midiFx)
        {
            if (const auto made = startMidiFx ({}, {}, request, files); made.failed())
                showError ("Couldn't start the MIDI FX", made.getErrorMessage());

            return;
        }

        projectTools.setChosenKind (type == ChatPanel::BuildType::effect   ? std::optional<PluginKind> (PluginKind::effect)
                                  : type == ChatPanel::BuildType::widget   ? std::optional<PluginKind> (PluginKind::widget)
                                  : type == ChatPanel::BuildType::instrument ? std::optional<PluginKind> (PluginKind::instrument)
                                                                             : std::nullopt);
    }

    // Pictures sent with a request also go into the project, so Sonify AI can use them in
    // the GUI (a background, a logo...).
    juce::StringArray saved;

    if (project.isOpen())
        for (const auto& file : files)
            if (GuiLayout::isPicture (file))
                if (const auto name = GuiLayout::importPicture (file, project.getGuiFolder().getChildFile (GuiLayout::imagesFolder)); name.isNotEmpty())
                    saved.add (name);

    if (saved.isEmpty())
    {
        ai.send (request, files);
        return;
    }

    ai.send (request + "\n\n(The attached pictures are also saved in the project as gui/images/" + saved.joinIntoString (", gui/images/")
                 + ": the GUI can use them by those names.)",
             files, request);
}

juce::String CreatorStudio::lastAnswer() const
{
    // The newest answer or note since the user's last request.
    const auto& entries = ai.getEntries();

    for (auto it = entries.rbegin(); it != entries.rend(); ++it)
    {
        if (it->kind == StellaAi::Entry::Kind::user)
            break;

        if (it->kind == StellaAi::Entry::Kind::ai || it->kind == StellaAi::Entry::Kind::notice)
            return it->text;
    }

    return "Done.";
}

void CreatorStudio::autoLayout()
{
    if (! project.isOpen() || preview.getParameters().isEmpty())
    {
        showError ("Nothing to lay out yet", "Build the plugin first: the automatic GUI is made from its parameters.");
        return;
    }

    autoLayoutDialog = std::make_unique<juce::AlertWindow> ("Replace the GUI?",
                                                            "This replaces the current GUI with a plain automatic one: a group per module, "
                                                            "a control per parameter.",
                                                            juce::MessageBoxIconType::NoIcon, this);
    autoLayoutDialog->addButton ("Replace", 1, juce::KeyPress (juce::KeyPress::returnKey));
    autoLayoutDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    autoLayoutDialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis = juce::Component::SafePointer<CreatorStudio> (this)] (int result)
        {
            if (safeThis == nullptr || safeThis->autoLayoutDialog == nullptr)
                return;

            safeThis->autoLayoutDialog->setVisible (false);

            if (result == 1)
            {
                safeThis->project.getGuiFolder().getChildFile (GuiLayout::fileName).deleteFile();
                safeThis->loadLayout (true);
            }

            juce::MessageManager::callAsync ([safeThis]
            {
                if (safeThis != nullptr)
                    safeThis->autoLayoutDialog = nullptr;
            });
        }), false);
}

void CreatorStudio::showBuildLog()
{
    showLog ("Build log", preview.getLog());
}

void CreatorStudio::showLog (const juce::String& title, const juce::String& text)
{
    if (logWindow == nullptr)
        logWindow = std::make_unique<LogWindow>();

    auto* window = static_cast<LogWindow*> (logWindow.get());
    window->setTitle (title);
    window->setLog (text);
    logWindow->centreAroundComponent (this, logWindow->getWidth(), logWindow->getHeight());
    logWindow->setVisible (true);
    logWindow->toFront (true);
}

//==============================================================================
void CreatorStudio::addToSonify()
{
    if (isMakingMidiFx())
    {
        showError ("It's in Sonify already", "A MIDI FX plays on your instrument track as soon as Fanan AI builds it, and it's in the "
                                             "tracks' MIDI FX menu, under My Devices.");
        return;
    }

    if (! project.isOpen() || adding)
        return;

    if (hasNothingToBuild (project))
    {
        showError ("Nothing to add yet", "Describe your plugin to Fanan AI first. Once it plays, Add to Sonify puts it in Sonify's Browser.");
        return;
    }

    if (project.getInfo().kind == PluginKind::noteEffect)
    {
        showError ("MIDI effects go elsewhere", "This old MIDI effect project can't play on a track: plugins made here don't send MIDI. "
                                                "Start a new plugin of type MIDI FX: Fanan AI builds it as one of Sonify's MIDI devices.");
        return;
    }

    if (ai.isBusy())
    {
        showError ("Fanan AI is still working", "Add it to Sonify once it's finished: the plugin may still change.");
        return;
    }

    // Built from its code as it is now, then added once it plays (it keeps playing meanwhile).
    addWhenPlaying = true;
    topBar.showNotice (juce::String::fromUTF8 ("Building, then adding\xe2\x80\xa6"));
    buildPlugin();
}

void CreatorStudio::installInSonify()
{
    if (! project.isOpen() || adding)
        return;

    const auto info = project.getInfo();
    auto* playing = audio.getPlugin();

    if (preview.getState() != LivePreview::State::playing || playing == nullptr || preview.getPluginWasm().empty())
    {
        showError ("It isn't playing", "It has to play to go into Sonify: ask Fanan AI to fix it, then Add to Sonify again.");
        return;
    }

    if (ai.isBusy())
    {
        showError ("Fanan AI is still working", "Add it to Sonify once it's finished: the plugin may still change.");
        return;
    }

    MadePlugins::Package package;
    juce::String problem;

    if (! makePackage (package, problem))
    {
        showError ("Couldn't add it to Sonify", problem);
        return;
    }

    adding = true;
    workspace.setExporting (true);

    // Protected: built again with its product's id and demo inside, with its license.soni (in
    // the background: it takes a few seconds), then added.
    if (const auto protectionNow = Licences::loadProtection (project.getFolder()); protectionNow.isValid())
    {
        topBar.showNotice (juce::String::fromUTF8 ("Building it protected\xe2\x80\xa6"));

        juce::Thread::launch ([safeThis = juce::Component::SafePointer<CreatorStudio> (this), package, protectionNow,
                               folder = project.getFolder(), name = info.name] () mutable
        {
            const auto protectedNow = MadePlugins::protect (package, folder, protectionNow);

            juce::MessageManager::callAsync ([safeThis, package = std::move (package), protectedNow, name]
            {
                if (safeThis == nullptr)
                    return;

                if (protectedNow.failed())
                {
                    safeThis->adding = false;
                    safeThis->workspace.setExporting (false);
                    safeThis->showError ("Couldn't add it to Sonify", protectedNow.getErrorMessage());
                    return;
                }

                safeThis->finishAdding (package, name);
            });
        });

        return;
    }

    finishAdding (package, info.name);
}

void CreatorStudio::finishAdding (const MadePlugins::Package& package, const juce::String& name)
{
    juce::File folder;
    bool replaced = false;
    const auto installed = MadePlugins::install (package, folder, replaced);

    adding = false;
    workspace.setExporting (false);

    if (installed.failed())
    {
        showError ("Couldn't add it to Sonify", installed.getErrorMessage());
        return;
    }

    topBar.showNotice (replaced ? "Updated in Sonify" : "Added to Sonify");

    if (onPluginAdded != nullptr)
        onPluginAdded (folder, name, replaced);
}

//==============================================================================
void CreatorStudio::exportPlugin()
{
    if (isMakingMidiFx())
    {
        showError ("MIDI FX stay in Sonify", "A MIDI FX is one of Sonify's MIDI devices: it plays on your tracks here. Export Plugin makes "
                                             "plugin files from plugins (instruments, effects and widgets).");
        return;
    }

    if (! project.isOpen() || exportingPlugin)
        return;

    if (hasNothingToBuild (project))
    {
        showError ("Nothing to export yet", "Describe your plugin to Fanan AI first. Once it plays, Export Plugin turns it into files your DAW loads.");
        return;
    }

    if (project.getInfo().kind == PluginKind::noteEffect)
    {
        showError ("MIDI effects can't be exported", "This old MIDI effect project can't become a plugin file: plugins made here don't send "
                                                     "MIDI. Start a new plugin of type MIDI FX to make one of Sonify's MIDI devices.");
        return;
    }

    if (ai.isBusy())
    {
        showError ("Fanan AI is still working", "Export it once it's finished: the plugin may still change.");
        return;
    }

    if (preview.getState() != LivePreview::State::playing)
    {
        showError ("It isn't playing", "It has to play to be exported: ask Fanan AI to fix it, then export again.");
        return;
    }

    if (const auto problem = PluginExporter::compilerProblem(); problem.isNotEmpty())
    {
        showError ("Can't export on this computer yet", problem);
        return;
    }

    const auto info = project.getInfo();
   #if JUCE_MAC
    const juce::String formats ("VST3, Audio Unit and CLAP files for macOS (Apple silicon and Intel)");
   #elif JUCE_WINDOWS
    const juce::String formats ("VST3 and CLAP files for Windows (64-bit)");
   #else
    const juce::String formats ("VST3 and CLAP files for Linux");
   #endif

    auto* options = new ExportOptions (PluginExporter::defaultExportFolder (info.name));
    exportOptions.reset (options);
    exportDialog = std::make_unique<juce::AlertWindow> ("Export " + info.name,
                                                        formats + ", built on this computer. They play in any DAW, wherever you copy "
                                                        "them, without Sonify.",
                                                        juce::MessageBoxIconType::NoIcon, this);
    exportDialog->addCustomComponent (options);
    exportDialog->addButton ("Export", 1, juce::KeyPress (juce::KeyPress::returnKey));
    exportDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    // Nothing ticked: nothing to export.
    options->onChanged = [safeThis = juce::Component::SafePointer<CreatorStudio> (this), options]
    {
        if (safeThis != nullptr && safeThis->exportDialog != nullptr)
            if (auto* button = safeThis->exportDialog->getButton (0))
                button->setEnabled (! options->chosenFormats().isEmpty());
    };

    exportDialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis = juce::Component::SafePointer<CreatorStudio> (this), options] (int result)
        {
            if (safeThis == nullptr || safeThis->exportDialog == nullptr)
                return;

            safeThis->exportDialog->setVisible (false);

            if (result == 1 && ! options->chosenFormats().isEmpty())
                safeThis->startExport (options->chosenFormats(), options->getFolder(), options->shouldInstall());

            juce::MessageManager::callAsync ([safeThis]
            {
                if (safeThis != nullptr)
                {
                    safeThis->exportDialog = nullptr;
                    safeThis->exportOptions = nullptr;
                }
            });
        }), false);
}

void CreatorStudio::startExport (const juce::Array<PluginExporter::Format>& formats, const juce::File& destination, bool install)
{
    if (! project.isOpen() || exportingPlugin || preview.getState() != LivePreview::State::playing)
        return;

    // The panel as it is now, with every look it wears; baked here, compiled in the background.
    flushLayoutSave();
    workspace.getLooks().adoptMine (workspace.getLayout());

    std::map<juce::String, juce::String> units, names;

    for (const auto& p : preview.getParameters())
    {
        units[p.id] = p.unit;
        names[p.id] = p.name;
    }

    juce::StringArray displays;

    for (const auto& d : preview.getDisplays())
        displays.add (d.id);

    // The presets that come with the plugin (its factory presets): the window's preset managers
    // step through them, and an Audio Unit lists them.
    std::vector<Preset> factory;
    juce::StringArray presetNames;

    for (const auto& entry : presets.getEntries())
    {
        if (entry.isFactory())
        {
            factory.push_back (entry.preset);
            presetNames.add (entry.preset.name);
        }
    }

    auto request = std::make_shared<PluginExporter::Request>();
    request->projectFolder = project.getFolder();
    request->info = project.getInfo();
    request->destination = destination;
    request->formats = formats;
    request->install = install;
    request->gui = PluginExporter::prepareGui (workspace.getLayout(), workspace.bakeForExport (presetNames), units, names, displays,
                                               factory, project.getGuiFolder());
    request->library = PluginLibrary::load (project.getFolder());

    exportingPlugin = true;
    workspace.setExportingPlugin (true);
    topBar.showNotice (juce::String::fromUTF8 ("Exporting the plugin\xe2\x80\xa6"));

    juce::Thread::launch ([safeThis = juce::Component::SafePointer<CreatorStudio> (this), request]
    {
        const auto result = PluginExporter::exportPlugins (*request);

        juce::MessageManager::callAsync ([safeThis, result]
        {
            if (safeThis == nullptr)
                return;

            safeThis->exportingPlugin = false;
            safeThis->workspace.setExportingPlugin (false);
            safeThis->exportFinished (result);
        });
    });
}

void CreatorStudio::exportFinished (const PluginExporter::Result& result)
{
    if (! result.ok)
    {
        topBar.showNotice ("Export didn't work");
        showLog ("Export log", "The export didn't work.\n\n" + result.log);
        return;
    }

    topBar.showNotice ("Exported");

    juce::StringArray names;

    for (const auto& file : result.files)
        names.add (file.getFileName());

    const auto folder = result.folder != juce::File() ? result.folder
                                                      : (result.files.isEmpty() ? juce::File() : result.files.getFirst().getParentDirectory());

    auto message = names.joinIntoString (", ") + (names.size() == 1 ? " is" : " are") + " ready, built in " + juce::String (result.seconds, 1)
                 + " s:\n" + folder.getFullPathName();

    message << "\n\nEach plugin is in a folder of its own there (VST3" << juce::File::getSeparatorString() << "<name>, CLAP"
            << juce::File::getSeparatorString() << "<name>)" << (result.isProtected ? ", with its license.soni" : "") << ".";

    if (! result.installed.isEmpty())
        message << "\n\nAnd copied into your plugin folders, where DAWs look:\n" << result.installed.joinIntoString ("\n")
                << "\n\nRescan plugins in your DAW (or start it again) to see " << (names.size() == 1 ? "it." : "them.");
    else
        message << "\n\nFor your DAW, copy each plugin's whole folder into the folders it scans, then rescan plugins.";

    if (result.isProtected)
        message << "\n\nIt's protected: keep license.soni next to the plugin (copy the whole folder, and zip it that way for your "
                   "buyers). Without it, the plugin can't be registered and only plays its demo.";

    // A sampler: its blobs go with it, and where they're found.
    if (project.isOpen())
    {
        if (const auto library = PluginLibrary::load (project.getFolder()); ! library.isEmpty())
        {
            const auto& info = project.getInfo();
            const auto home = PluginLibrary::homeFolder (info.vendor, info.name);
            message << "\n\nIts sample library (" << library.size() << (library.size() == 1 ? " blob" : " blobs")
                    << ") plays here as it is. On another computer, the blobs go in the Documents folder's "
                    << home.getParentDirectory().getFileName() << juce::File::getSeparatorString() << home.getFileName()
                    << " folder (here: " << home.getFullPathName() << ").";
        }
    }

    auto options = juce::MessageBoxOptions()
                       .withTitle ("Exported")
                       .withMessage (message)
                       .withButton ("Show in folder")
                       .withButton ("OK")
                       .withAssociatedComponent (this);

    juce::AlertWindow::showAsync (options, [file = result.files.isEmpty() ? folder : result.files.getFirst()] (int button)
    {
        if (button == 1 && file != juce::File())
            file.revealToUser();
    });
}

bool CreatorStudio::makePackage (MadePlugins::Package& package, juce::String& problem)
{
    auto* playing = audio.getPlugin();

    if (! project.isOpen() || preview.getState() != LivePreview::State::playing || playing == nullptr || preview.getPluginWasm().empty())
    {
        problem = "It has to play first: ask Fanan AI to fix it, then try again.";
        return false;
    }

    const auto info = project.getInfo();

    // The panel as it is now, with every look it wears (mine too), so it opens the same on a track.
    flushLayoutSave();
    workspace.getLooks().adoptMine (workspace.getLayout());

    package.projectId = info.uuid;
    package.name = info.name;
    package.vendor = info.vendor;
    package.kind = Project::kindToString (info.kind);   // instrument, effect or widget
    package.description = playing->getDescription();
    package.wasm = preview.getPluginWasm();
    package.elementsWasm = preview.getElementsWasm();
    package.guiFolder = project.getGuiFolder();
    package.presetsFile = project.getFolder().getChildFile (PresetBank::fileName);
    package.library = PluginLibrary::load (project.getFolder());
    return true;
}

//==============================================================================
void CreatorStudio::applyLibrary()
{
    if (! project.isOpen())
    {
        preview.setLibrary ({});
        return;
    }

    const auto& info = project.getInfo();
    preview.setLibrary (PluginLibrary::entriesFor (PluginLibrary::load (project.getFolder()), info.vendor, info.name, {}, true));
}

bool CreatorStudio::syncLibraryLists (GuiLayout& layout) const
{
    if (! project.isOpen())
        return false;

    juce::StringArray names;

    for (const auto& instrument : PluginLibrary::load (project.getFolder()))
        names.add (instrument.name);

    if (names.isEmpty())
        names.add ("(no instruments yet)");

    bool changed = false;

    for (auto& w : layout.widgets)
    {
        const auto found = w.settings.find ("library");

        if (found == w.settings.end() || ! (bool) found->second || w.options == names)
            continue;

        w.options = names;
        changed = true;
    }

    return changed;
}

void CreatorStudio::showLibrary()
{
    if (! project.isOpen() || isMakingMidiFx())
        return;

    if (libraryWindow == nullptr)
    {
        PluginLibrary::WindowHooks hooks;
        hooks.get = [this] { return project.isOpen() ? PluginLibrary::load (project.getFolder()) : juce::Array<PluginLibrary::Instrument>(); };
        hooks.set = [this] (const juce::Array<PluginLibrary::Instrument>& instruments)
        {
            if (! project.isOpen())
                return;

            if (! PluginLibrary::save (project.getFolder(), instruments))
            {
                showError ("Couldn't save the library", "library.json couldn't be written in " + project.getFolder().getFullPathName());
                return;
            }

            applyLibrary();

            // The Instrument lists show the new names.
            if (auto layout = workspace.getLayout(); syncLibraryLists (layout))
            {
                workspace.setLayout (layout);
                saveLayout();
            }
        };
        hooks.stateOf = [this] (int instrument) { return preview.getLibraryState (instrument); };
        hooks.problemOf = [this] (int instrument) { return preview.getLibraryProblem (instrument); };
        hooks.startFolder = [this]
        {
            for (const auto& instrument : PluginLibrary::load (project.getFolder()))
                if (instrument.file.getParentDirectory().isDirectory())
                    return instrument.file.getParentDirectory();

            return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        };

        libraryWindow = PluginLibrary::makeWindow (project.getInfo().name, std::move (hooks));
        libraryWindow->centreAroundComponent (this, libraryWindow->getWidth(), libraryWindow->getHeight());
    }

    libraryWindow->setVisible (true);
    libraryWindow->toFront (true);
}

//==============================================================================
void CreatorStudio::refreshProtection()
{
    ProtectionPage::Subject subject;

    if (isMakingMidiFx())
    {
        // A MIDI FX's protection is kept in My Devices, by its id (it's a file, not a project folder).
        const auto& device = devices->getDevice();
        subject.key = device.id;
        subject.kind = "midifx";
        subject.name = device.name;
        subject.vendor = device.vendor.isNotEmpty() ? device.vendor : settings.getVendor();
        subject.folder = Devices::protectionFolderOf (device.id);
    }
    else if (project.isOpen())
    {
        const auto& info = project.getInfo();
        subject.key = info.uuid;
        subject.kind = Project::kindToString (info.kind);
        subject.name = info.name;
        subject.vendor = info.vendor;
        subject.folder = project.getFolder();
    }

    protection.setSubject (subject);
}

juce::String CreatorStudio::storeFacts() const
{
    juce::String facts;

    if (isMakingMidiFx())
    {
        // A MIDI FX: what it says of itself, and its code.
        const auto& device = devices->getDevice();
        facts << "A MIDI FX (a MIDI effect on an instrument track in Sonify: it changes, makes or filters the notes).\n"
              << "Name: " << device.name << "\n";

        if (device.description.isNotEmpty())
            facts << "Its description: " << device.description << "\n";

        for (const auto& [fileName, text] : device.sources)
            facts << "\n// " << fileName << "\n" << text << "\n";

        return facts;
    }

    if (project.isOpen())
    {
        const auto& info = project.getInfo();
        facts << "A plugin made in Sonify, a " << Project::kindDisplayName (info.kind).toLowerCase() << ".\n"
              << "Name: " << info.name << "\n\n"
              << projectTools.describeProject();
    }

    return facts;
}

//==============================================================================
StorePage::Subject CreatorStudio::storeSubject() const
{
    StorePage::Subject subject;

    if (isMakingMidiFx())
    {
        // A MIDI FX's protection is kept in My Devices, by its id.
        const auto& device = devices->getDevice();
        subject.key = device.id;
        subject.kind = "midifx";
        subject.name = device.name;
        subject.maker = device.vendor.isNotEmpty() ? device.vendor : settings.getVendor();
        subject.isProtected = Licences::loadProtection (Devices::protectionFolderOf (device.id)).isValid();
    }
    else if (project.isOpen())
    {
        const auto& info = project.getInfo();
        subject.key = info.uuid;
        subject.kind = Project::kindToString (info.kind);
        subject.name = info.name;
        subject.maker = info.vendor;
        subject.hasPanel = preview.getState() == LivePreview::State::playing && project.getGuiFolder().isDirectory();
        subject.isProtected = Licences::loadProtection (project.getFolder()).isValid();
    }

    return subject;
}

juce::Image CreatorStudio::storePicture (juce::String& problem)
{
    if (isMakingMidiFx() || ! project.isOpen())
    {
        problem = "A MIDI FX has no panel: choose a picture.";
        return {};
    }

    if (preview.getState() != LivePreview::State::playing)
    {
        problem = "It has to play first: its panel is taken as it plays.";
        return {};
    }

    flushLayoutSave();
    const auto picture = workspace.snapshotPanel (1200);

    if (! picture.isValid())
        problem = "It has no panel yet: design it in Edit UI, then take it.";

    return picture;
}

bool CreatorStudio::makeStoreItem (StorePublisher::Item& item, juce::String& problem)
{
    if (isMakingMidiFx())
    {
        const auto& device = devices->getDevice();

        if (midiFxBusy())
        {
            problem = "Fanan AI is still working on it: post the ad once it's finished.";
            return false;
        }

        if (device.wasm.isEmpty())
        {
            problem = "It hasn't been built yet: ask Fanan AI to build it first (or untick the Sonify version).";
            return false;
        }

        item.isMidiFx = true;
        item.device = device;

        if (item.device.vendor.trim().isEmpty())
            item.device.vendor = settings.getVendor();

        if (item.maker.trim().isEmpty())
            item.maker = item.device.vendor;

        return true;
    }

    if (! project.isOpen())
    {
        problem = "Open its project in the studio first.";
        return false;
    }

    if (project.getInfo().kind == PluginKind::noteEffect)
    {
        problem = "This old MIDI effect project can't play on a track, so its Sonify version can't be offered. "
                  "Make it again as a MIDI FX (or untick the Sonify version).";
        return false;
    }

    if (ai.isBusy())
    {
        problem = "Fanan AI is still working on it: post the ad once it's finished.";
        return false;
    }

    if (! makePackage (item.package, problem))
        return false;

    item.projectFolder = project.getFolder();
    return true;
}

void CreatorStudio::connectionChanged()
{
    if (server.isSignedIn())
        waitingForSignIn = false;

    ChatPanel::Account account;
    account.status = server.getStatus();
    account.siteAvailable = server.hasSite();
    account.signedIn = server.isSignedIn();
    account.waitingForBrowser = waitingForSignIn;
    account.credits = server.getCredits();
    account.email = server.getEmail();
    account.computerCode = server.getComputerCode();
    account.lastEmail = server.getLastEmail();

    chat.setAccount (account);
    refreshChat();   // the plan bar's "you have" follows the credits
    storePage.connectionChanged();
}

void CreatorStudio::buyCredits()
{
    if (! server.hasSite())
    {
        showError ("The store isn't open yet", "Buying Fanan AI credits isn't set up in this build yet.");
        return;
    }

    // Credits belong to an account, so they work on every computer signed in to it:
    // a computer that isn't signed in signs in first, and the site goes on to the store.
    if (! server.isSignedIn())
    {
        signIn (true, server.getLastEmail());
        return;
    }

    server.getStoreUrl().launchInDefaultBrowser();
    server.watchForChanges();   // the new credits show up here within seconds of paying
}

void CreatorStudio::signIn (bool thenBuy, const juce::String& emailHint)
{
    if (! server.isReady())
    {
        showError ("Not connected", "Signing in needs Fanan's server. Check the internet connection and try again.");
        server.checkNow();
        return;
    }

    server.signIn (thenBuy, emailHint, [safeThis = juce::Component::SafePointer<CreatorStudio> (this)] (const juce::Result& result, const juce::var&)
    {
        if (safeThis == nullptr)
            return;

        if (result.failed())
        {
            safeThis->showError ("Couldn't start signing in", result.getErrorMessage());
            return;
        }

        safeThis->waitingForSignIn = true;
        safeThis->connectionChanged();

        // If the browser is abandoned, the box goes back to normal after ten minutes.
        const auto attempt = ++safeThis->signInAttempt;

        juce::Timer::callAfterDelay (10 * 60 * 1000, [safeThis, attempt]
        {
            if (safeThis != nullptr && safeThis->signInAttempt == attempt && safeThis->waitingForSignIn)
            {
                safeThis->waitingForSignIn = false;
                safeThis->connectionChanged();
            }
        });
    });
}

void CreatorStudio::signOut()
{
    signOutDialog = std::make_unique<juce::AlertWindow> ("Sign out?",
                                                         "This computer stops using " + server.getEmail()
                                                             + ". Your credits stay with your account, and signing in again brings them back.",
                                                         juce::MessageBoxIconType::NoIcon, this);

    signOutDialog->addButton ("Sign out", 1, juce::KeyPress (juce::KeyPress::returnKey));
    signOutDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    signOutDialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis = juce::Component::SafePointer<CreatorStudio> (this)] (int result)
        {
            if (safeThis == nullptr || safeThis->signOutDialog == nullptr)
                return;

            safeThis->signOutDialog->setVisible (false);

            if (result == 1)
            {
                safeThis->server.signOut ([safeThis] (const juce::Result& outcome, const juce::var&)
                {
                    if (safeThis != nullptr && outcome.failed())
                        safeThis->showError ("Couldn't sign out", outcome.getErrorMessage());
                });
            }

            // Let the modal machinery finish with the dialog before it goes.
            juce::MessageManager::callAsync ([safeThis]
            {
                if (safeThis != nullptr)
                    safeThis->signOutDialog = nullptr;
            });
        }), false);
}

void CreatorStudio::setPlayMode (bool shouldPlay)
{
    // The Edit UI tab's Edit and Play tabs.
    playMode = shouldPlay;
    workspace.setPlayMode (playMode);
}

//==============================================================================
void CreatorStudio::newProject (bool typeMissing)
{
    if (refuseWhileBusy())
        return;

    // The type first, chosen by the user (never a default): it's what gets built, and it stays.
    newProjectDialog = std::make_unique<juce::AlertWindow> ("New plugin",
                                                            typeMissing ? "Choose its type first: an Instrument, an Effect, a MIDI FX, or a "
                                                                          "Widget for anything else."
                                                                        : "What are you building? Its type is set now, and it's built exactly "
                                                                          "as that. Widget: anything that isn't an instrument, an effect or a "
                                                                          "MIDI FX.",
                                                            typeMissing ? juce::MessageBoxIconType::WarningIcon : juce::MessageBoxIconType::NoIcon,
                                                            this);

    newProjectDialog->addTextEditor ("name", "My Plugin", "Name");
    newProjectDialog->addComboBox ("kind", kindNames, "Type");
    newProjectDialog->addTextEditor ("vendor", settings.getVendor(), "Company");
    newProjectDialog->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
    newProjectDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    // The type already chosen beside Send, if any; otherwise none until the user picks one.
    if (auto* combo = newProjectDialog->getComboBoxComponent ("kind"))
    {
        combo->setTextWhenNothingSelected (juce::String::fromUTF8 ("Choose\xe2\x80\xa6"));
        const auto chosen = project.isOpen() ? ChatPanel::BuildType::none : chat.getBuildType();
        combo->setSelectedItemIndex (chosen == ChatPanel::BuildType::none ? -1 : (int) chosen - 1, juce::dontSendNotification);
    }

    newProjectDialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis = juce::Component::SafePointer<CreatorStudio> (this)] (int result)
        {
            if (safeThis == nullptr || safeThis->newProjectDialog == nullptr)
                return;

            auto& dialog = *safeThis->newProjectDialog;
            dialog.setVisible (false);

            const auto* kindBox = dialog.getComboBoxComponent ("kind");
            const auto kindIndex = kindBox != nullptr ? kindBox->getSelectedItemIndex() : 0;

            // No type chosen: nothing is made without one, so it's asked again.
            const bool askAgain = result == 1 && kindIndex < 0;

            if (result == 1 && ! askAgain && kindIndex == midiFxIndex)
            {
                // A new MIDI FX: one of Sonify's MIDI devices, described to Sonify AI in Build with AI.
                const auto vendor = dialog.getTextEditorContents ("vendor").trim();

                if (const auto made = safeThis->startMidiFx (dialog.getTextEditorContents ("name"), vendor, {}); made.failed())
                    safeThis->showError ("Couldn't start the MIDI FX", made.getErrorMessage());
                else if (vendor.isNotEmpty())
                    safeThis->settings.setVendor (vendor);
            }
            else if (result == 1 && ! askAgain)
            {
                ProjectInfo info;
                info.name = dialog.getTextEditorContents ("name").trim();
                info.vendor = dialog.getTextEditorContents ("vendor").trim();
                info.kind = kindAt (kindIndex);

                safeThis->flushLayoutSave();
                const auto created = safeThis->project.create (safeThis->settings.getProjectsFolder(), info);

                if (created.failed())
                    safeThis->showError ("Couldn't create the plugin", created.getErrorMessage());
                else
                    safeThis->settings.setVendor (info.vendor);
            }

            // Let the modal machinery finish with the dialog before it goes.
            juce::MessageManager::callAsync ([safeThis, askAgain]
            {
                if (safeThis == nullptr)
                    return;

                safeThis->newProjectDialog = nullptr;

                if (askAgain)
                    safeThis->newProject (true);
            });
        }), false);

    // Typing goes straight into the name, ready to be replaced. Done once the window is
    // on screen: a window that isn't showing yet can't take the keyboard.
    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<CreatorStudio> (this)]
    {
        if (safeThis == nullptr || safeThis->newProjectDialog == nullptr)
            return;

        if (auto* nameEditor = safeThis->newProjectDialog->getTextEditor ("name"))
        {
            nameEditor->grabKeyboardFocus();
            nameEditor->selectAll();
        }
    });
}

void CreatorStudio::playFinishSound()
{
    // Through Sonify's output (over whatever plays), wherever the user is; with no audio
    // device running, the system's alert sound instead.
    if (! audio.playChime())
        juce::LookAndFeel::getDefaultLookAndFeel().playAlertSound();
}

void CreatorStudio::saveProject()
{
    // A MIDI FX is kept in My Devices as it changes.
    if (isMakingMidiFx())
    {
        topBar.showNotice ("Saved");
        return;
    }

    if (! project.isOpen())
        return;

    // Everything else is written as it changes.
    flushLayoutSave();
    topBar.showNotice ("Saved");
}

void CreatorStudio::saveProjectAs()
{
    if (isMakingMidiFx())
    {
        saveMidiFxCopy();
        return;
    }

    if (! project.isOpen() || saveAsDialog != nullptr)
        return;

    // Sonify AI writes into the project as it works: the copy waits until it's done.
    if (ai.isBusy())
    {
        showError ("Fanan AI is still working", "Save a copy once it's finished.");
        return;
    }

    flushLayoutSave();

    saveAsDialog = std::make_unique<juce::AlertWindow> (juce::String::fromUTF8 ("Save as\xe2\x80\xa6"),
                                                        "A copy of this plugin in a new folder, to go on with under a new name. "
                                                        "This one stays as it is.",
                                                        juce::MessageBoxIconType::NoIcon, this);

    saveAsDialog->addTextEditor ("name", project.getInfo().name + " 2", "Name");
    saveAsDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveAsDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    saveAsDialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis = juce::Component::SafePointer<CreatorStudio> (this)] (int result)
        {
            if (safeThis == nullptr || safeThis->saveAsDialog == nullptr)
                return;

            const auto name = safeThis->saveAsDialog->getTextEditorContents ("name").trim();
            safeThis->saveAsDialog->setVisible (false);

            if (result == 1)
            {
                const auto saved = safeThis->project.saveAs (name);

                if (saved.failed())
                {
                    safeThis->showError ("Couldn't save a copy", saved.getErrorMessage());
                }
                else
                {
                    // The copy is a plugin of its own: Add to Sonify adds it beside the first one.
                    safeThis->topBar.showNotice ("Saved as " + safeThis->project.getInfo().name);
                }
            }

            // Let the modal machinery finish with the dialog before it goes.
            juce::MessageManager::callAsync ([safeThis]
            {
                if (safeThis != nullptr)
                    safeThis->saveAsDialog = nullptr;
            });
        }), false);

    // The name is ready to be typed over.
    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<CreatorStudio> (this)]
    {
        if (safeThis == nullptr || safeThis->saveAsDialog == nullptr)
            return;

        if (auto* nameEditor = safeThis->saveAsDialog->getTextEditor ("name"))
        {
            nameEditor->grabKeyboardFocus();
            nameEditor->selectAll();
        }
    });
}

void CreatorStudio::openProject()
{
    if (refuseWhileBusy())
        return;

    const auto start = project.isOpen() ? project.getFolder().getParentDirectory() : settings.getProjectsFolder();

    // A plugin project, or a MIDI FX (a Sonify device, from My Devices or anywhere).
    chooser = std::make_unique<juce::FileChooser> ("Open a plugin project or a MIDI FX", start.isDirectory() ? start : juce::File(),
                                                   juce::String ("*.stella;*") + Devices::fileExtension);

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safeThis = juce::Component::SafePointer<CreatorStudio> (this)] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();

                              if (safeThis != nullptr && file != juce::File())
                                  safeThis->openProjectFile (file);
                          });
}

void CreatorStudio::openProjectFile (const juce::File& file)
{
    if (refuseWhileBusy())
        return;

    if (file.hasFileExtension (Devices::fileExtension))
    {
        if (const auto opened = openMidiFxFile (file); opened.failed())
            showError ("Couldn't open the MIDI FX", opened.getErrorMessage());

        return;
    }

    flushLayoutSave();   // the open project's last GUI change, before another one opens
    const auto result = project.open (file);

    if (result.failed())
    {
        settings.removeRecentProject (file);
        showError ("Couldn't open the project", result.getErrorMessage());
    }
}

//==============================================================================
void CreatorStudio::showError (const juce::String& title, const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, title, message, "OK", this);
}
