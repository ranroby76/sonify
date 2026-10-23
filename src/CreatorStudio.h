// D:\workspace\Fanan_web_hub\src\CreatorStudio.h

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "ChatPanel.h"
#include "CreatorAudio.h"
#include "FananServer.h"
#include "History.h"
#include "LivePreview.h"
#include "PluginExporter.h"
#include "PresetLibrary.h"
#include "Project.h"
#include "ProjectTools.h"
#include "ProtectionPage.h"
#include "Settings.h"
#include "StellaAi.h"
#include "StoreClient.h"
#include "StorePage.h"
#include "StorePublisher.h"
#include "TopBar.h"
#include "Workspace.h"

#include <map>

class AiAgent;
class DeviceStudio;

//==============================================================================
/**
    The Plugins Creator Studio: Stella AI Studio inside Sonify, with Sonify AI as its builder.

        its top bar (Options menu, the five tabs, its meter and the load)
        the open tab: Build with AI / Edit UI / Knob Studio / Protection / Store

    Build with AI makes everything: instruments, effects and widgets (plugin projects), and MIDI
    FX, which are Sonify's MIDI devices (DeviceStudio, built by its own Sonify AI, in the same
    conversation panel). One thing is open at a time: a plugin project, or a MIDI FX. The chat
    shows the open one's card (name, type, developer).

    Add to Sonify puts the plugin that plays into the Browser, to use on tracks like Sonify's
    system tools (it's built first, from its code as it is). Export Plugin makes plugin files any
    DAW loads, built on this computer (PluginExporter): VST3 and CLAP, and on a Mac an Audio Unit
    too. A MIDI FX plays on its instrument track as soon as it's built, and is kept in
    My Devices. Sonify's engine owns the audio device and the MIDI inputs; the plugin being made
    plays in its callback (CreatorAudio), fed by the live notes while the studio is in front
    (while a MIDI FX is open, the live notes go to the tracks, where it plays). Sonify's server
    is shared: the same Fanan account and credits as Sonify AI everywhere else.

    Protection protects the open plugin or MIDI FX with a serial formula of its own (Licences):
    the creator sells it on their own site, with serials the formula makes there. Protected, Add
    to Sonify builds it with its product's id and demo inside and puts its license.soni in its
    folder; a MIDI FX's file carries its license.soni. The Store is Sonify's notice board
    (StoreClient), open to every user: makers post ads there (StorePublisher) with a link to their
    own site, and may offer the Sonify version for instant download.

    The plugin's presets are a modern preset manager's (PresetLibrary): factory presets in the
    project, the user's own on the hard drive, banks; the chosen preset and the values are kept
    with the project, and come back when it opens (the first preset, the first time).

    Message thread only. Lives in a window of its own laid over Sonify's page (MainCreator.cpp).
*/
class CreatorStudio final : public juce::Component
{
public:
    CreatorStudio (AudioEngine& sonifyEngine, FananServer& fananServer);
    ~CreatorStudio() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Whether the studio is in front (Sonify's selector): it takes the live notes then, unless
        a MIDI FX is open (they go to the tracks, where it plays). */
    void setShown (bool shown);
    bool isShown() const noexcept                  { return shownInSonify; }

    /** One of the tabs, in TopBar::tabNames' order. */
    void showTab (int index);
    int getCurrentTab() const noexcept             { return currentTab; }

    /** Sonify's server: the connection or the account changed. */
    void connectionChanged();

    /** Sonify AI here is working on a plugin (or has a plan waiting): Sonify keeps it in mind on
        quitting, and nothing else opens here meanwhile. */
    bool isBusy() const                            { return ai.isBusy(); }

    /** What's waiting is written now (Sonify is quitting, or saving). */
    void flush()                                   { flushLayoutSave(); }

    /** Sonify's MIDI devices and the Sonify AI that builds them (Sonify's own, set once): a MIDI
        FX is made here as one. */
    void setMidiDevices (DeviceStudio& studio, AiAgent& builder);

    /** The open MIDI device, or its Sonify AI, changed (Sonify says so). A device that opens (from
        a track's MIDI FX, or a new one) takes the place of the plugin project open here. */
    void midiDeviceChanged();

    /** A MIDI device opened elsewhere (a track's MIDI FX, its menu's New Device): Build with AI
        shows it, in front. */
    void showMidiDevice();

    /** Whether a MIDI FX is what's open here. */
    bool isMakingMidiFx() const noexcept;

    /** One of Sonify's own keys pressed here: a page command for the studios' selector
        ("showStudio", "pluginsCreator", "nextStudio"...). */
    std::function<void (const juce::String& command)> onPageCommand;

    /** Add to Sonify: the plugin's folder in Sonify's library of made plugins (CreatorPlugin). */
    std::function<void (const juce::File& pluginFolder, const juce::String& name, bool replaced)> onPluginAdded;

private:
    void newProject (bool typeMissing = false);   // typeMissing: asked again, its type wasn't chosen
    void openProject();
    void openProjectFile (const juce::File& file);
    void playFinishSound();   // Sonify AI has finished
    void saveProject();       // what's waiting is written now
    void saveProjectAs();     // a copy in a new folder, under a new name, and work goes on there

    void projectChanged();
    void previewChanged();
    void loadLayout (bool makeOneIfMissing);
    void saveLayout();
    void saveLayoutSoon();
    void flushLayoutSave();   // a save that's waiting happens now
    void autoLayout();
    void askAiAboutGui (int widgetIndex, const juce::String& instruction);
    void sendFromChat (const juce::String& request, const juce::Array<juce::File>& files);
    juce::String lastAnswer() const;
    void buildPlugin();
    void showBuildLog();
    void showLog (const juce::String& title, const juce::String& text);
    void addToSonify();
    void installInSonify();   // Add to Sonify, once it plays (protected: built again with its protection first)
    void finishAdding (const MadePlugins::Package& package, const juce::String& name);

    // Export Plugin: the formats and the folder asked for, then built in the background.
    void exportPlugin();
    void startExport (const juce::Array<PluginExporter::Format>& formats, const juce::File& destination, bool install);
    void exportFinished (const PluginExporter::Result& result);
    void updateListening();

    /** The playing plugin as Add to Sonify (and the store) take it; problem says why not. */
    bool makePackage (MadePlugins::Package& package, juce::String& problem);

    // The sample library a sampler plays (PluginLibrary): Options > Sample library...
    void showLibrary();
    void applyLibrary();                              // the project's, to the plugin playing
    bool syncLibraryLists (GuiLayout& layout) const;  // Instrument lists show its names: true when one changed

    // Protection, and the Store's story.
    void refreshProtection();      // what's open, for the Protection tab
    juce::String storeFacts() const;   // what Fanan AI writes a store story from

    // The Store's ad form: what's open here, as it posts it.
    StorePage::Subject storeSubject() const;
    juce::Image storePicture (juce::String& problem);                          // the panel as it plays
    bool makeStoreItem (StorePublisher::Item& item, juce::String& problem);    // the Sonify version an ad offers

    // MIDI FX: Sonify's MIDI devices, made here.
    juce::Result startMidiFx (const juce::String& name, const juce::String& developer, const juce::String& request,
                              const juce::Array<juce::File>& files = {});
    void sendToMidiFx (const juce::String& request, const juce::Array<juce::File>& files);   // the user's code kept with it
    juce::Result openMidiFxFile (const juce::File& file);
    void closeMidiFx();
    bool midiFxBusy() const;       // its Sonify AI is at work: nothing else opens meanwhile
    bool refuseWhileBusy();        // says so, and true, when either Sonify AI is at work
    void saveMidiFxCopy();
    void refreshIdentity();        // the top bar's name, the chat's card and type, the tabs' notice
    void refreshChat();            // the open one's conversation
    std::vector<StellaAi::Entry> midiFxConversation() const;

    // Undo, presets
    bool keyPressed (const juce::KeyPress& key) override;
    bool sonifyKey (const juce::KeyPress& key);
    void undoOrRedo (bool redo);
    void recordHistory (const juce::String& label);
    void pushParameters();
    void pushPresets();
    std::map<juce::String, float> currentValues() const;
    void applyValues (const std::map<juce::String, float>& values);
    void parameterMoved (int index, float value);   // by the user: the chosen preset is changed now
    void restorePlayState();   // the project's chosen preset and values, once it plays
    void savePlayState();      // ...kept when they changed
    void buyCredits();
    void signIn (bool thenBuy, const juce::String& emailHint);
    void signOut();
    void setPlayMode (bool shouldPlay);
    void showError (const juce::String& title, const juce::String& message);

    Settings settings;
    FananServer& server;
    CreatorAudio audio;
    LivePreview preview { audio };
    Project project;
    ProjectTools projectTools { project, preview };
    StellaAi ai { server, projectTools };

    TopBar topBar;
    ChatPanel chat;
    ProtectionPage protection;
    Workspace workspace;
    StorePublisher publisher { server };
    StoreClient store { server };
    StorePage storePage { store, publisher };   // after the store and the publisher it uses
    bool storeLoaded = false;          // the Store tab asks the store the first time it opens

    std::unique_ptr<juce::DocumentWindow> logWindow, libraryWindow;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::AlertWindow> newProjectDialog, signOutDialog, autoLayoutDialog, saveAsDialog, exportDialog;
    std::unique_ptr<juce::Component> exportOptions;   // the export dialog's formats and folder (it goes first)

    History history;
    PresetLibrary presets;
    PresetUi presetUi { *this, presets };
    bool playStatePending = false;   // the project opened: its preset and values come back once it plays
    bool playStateDirty = false;     // ...and are kept when they change
    juce::TooltipWindow tooltips { this, 600 };

    DeviceStudio* devices = nullptr;   // Sonify's, for MIDI FX
    AiAgent* deviceAi = nullptr;

    int currentTab = Workspace::aiTab;
    bool shownInSonify = false;
    bool playMode = false;
    bool waitingForSignIn = false;   // sent to the browser to sign in, not back yet
    int signInAttempt = 0;           // a wait ends by itself after a while
    juce::String builtProjectId;     // the project the preview was built for
    int shownParamsVersion = -1;
    bool layoutSaveScheduled = false;
    bool guiRequestPending = false;  // Sonify AI is answering an Edit UI menu's request
    bool adding = false;             // Add to Sonify is writing the plugin
    bool addWhenPlaying = false;     // Add to Sonify pressed while it built: it's added once it plays
    bool exportingPlugin = false;    // Export Plugin is building plugin files

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CreatorStudio)
};
