// D:\workspace\Fanan_web_hub\src\MainComponent.cpp

#include "MainComponent.h"
#include "BuiltinPlugins.h"
#include "CreatorPlugin.h"
#include "CreatorStudio.h"
#include "Edits.h"
#include "Licences.h"
#include "PluginHeader.h"
#include "Song.h"

#include "BinaryData.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <utility>

//==============================================================================
namespace
{
    juce::File internetBlockedFile()
    {
        return SettingsFolder::file ("network.json", "FananWebHub-network.json");
    }

    // The AI panel's own settings: Deep (every request to the strongest model), kept between runs.
    juce::File aiSettingsFile()
    {
        return SettingsFolder::file ("fanan-ai.json");
    }

    // Voice control turns speech into words through a paid service (Fanan's sonifyListen): it's
    // kept, but not offered for now. true brings it back (the AI panel shows its row again).
    constexpr bool voiceControlOffered = false;

    // The phone remote (Fanan AI from a phone, by text: in the studio on the local network, away
    // through the page /sonify on Fanan's site). false hides its row in the AI panel.
    constexpr bool phoneRemoteOffered = true;

    /** The input voice control listens to, by its name and channels (songs have their own inputs). */
    juce::File voiceInputFile()
    {
        return SettingsFolder::file ("voice.json", "FananWebHub-voice.json");
    }

    juce::var transportAction (const juce::String& action)
    {
        juce::DynamicObject::Ptr event (new juce::DynamicObject());
        event->setProperty ("action", action);
        return juce::var (event.get());
    }

    /** The account's connection, as the page names it. */
    const char* statusName (FananServer::Status status)
    {
        switch (status)
        {
            case FananServer::Status::checking:    return "checking";
            case FananServer::Status::online:      return "online";
            case FananServer::Status::notSetUp:    return "notSetUp";
            case FananServer::Status::noInternet:  return "noInternet";
            case FananServer::Status::serverDown:  return "serverDown";
            case FananServer::Status::blocked:     return "blocked";
        }

        return "checking";
    }
}

//==============================================================================
// Web browsing tabs
namespace
{
    juce::File webTabsFile()
    {
        return SettingsFolder::file ("tabs.json", "FananWebHub-tabs.json");
    }

    /** The tabs share the page's own browser profile (and so its running browser): a first tab
        opens in a moment. Cookies and sign-ins last between sessions. */
    juce::File browserProfileFolder()
    {
        return SettingsFolder::webViewProfile();
    }
}

namespace
{
    /** Where the user keeps plugins, for Plugin::isTrusted: native plugins load only from there. */
    void trustLibrary (const Library& library)
    {
        juce::StringArray files (library.getAddedFiles()), folders (library.getFolders());

        for (const auto& entry : library.getEntries())
            files.addIfNotAlreadyThere (entry.file);

        for (const auto& folder : Library::getDefaultFolders())
            folders.addIfNotAlreadyThere (folder.getFullPathName());

        for (const auto& format : Plugin::formatsForThisComputer())
            if (const auto folder = Plugin::defaultFolderFor (format); folder != juce::File())
                folders.addIfNotAlreadyThere (folder.getFullPathName());

        Plugin::setTrustedPlaces (files, folders);
    }
}

/** A web tab's page: plain web browsing, with nothing of the Hub's engine reachable from it. */
class MainComponent::WebTabView final : public juce::WebBrowserComponent
{
public:
    WebTabView (MainComponent& ownerIn, int tabId)
        : WebBrowserComponent (Options{}.withBackend (Options::Backend::webview2)
                                        .withKeepPageLoadedWhenBrowserIsHidden()   // a tab in the background keeps its page
                                        .withWinWebView2Options (Options::WinWebView2{}.withUserDataFolder (browserProfileFolder()))),
          owner (ownerIn), id (tabId)
    {
    }

    bool pageAboutToLoad (const juce::String& url) override         { owner.webTabLoading (id, url); return true; }
    void pageFinishedLoading (const juce::String& url) override      { owner.webTabLoaded (id, url); }
    bool pageLoadHadNetworkError (const juce::String& info) override { owner.webTabError (id, info); return true; }

    // A link that opens a new window opens a new tab.
    void newWindowAttemptingToLoad (const juce::String& url) override { owner.openWebTab (url, true); }

    // A page that closes itself closes its tab (after this call has returned).
    void windowCloseRequest() override
    {
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainComponent> (&owner), tab = id]
        {
            if (safe != nullptr)
                safe->closeWebTab (tab);
        });
    }

private:
    MainComponent& owner;
    const int id;
};

/** Sonify's own page: it loads only from Sonify itself. Anything else (a link or a web page
    dropped on the window, a page script sending it elsewhere) is stopped, because whatever
    page this view shows can drive the engine, the AI and the credits. A web plugin's screen
    (its frame) may load its own address; a new window opens as a web tab, which can't reach
    the engine. */
class MainComponent::HubPageView final : public juce::WebBrowserComponent
{
public:
    HubPageView (MainComponent& ownerIn, const Options& options) : WebBrowserComponent (options), owner (ownerIn) {}

    bool pageAboutToLoad (const juce::String& url) override
    {
        if (isOwn (url))
            return true;

        juce::Logger::writeToLog ("Sonify's page stayed put: it doesn't load " + url.substring (0, 200));
        return false;
    }

    void newWindowAttemptingToLoad (const juce::String& url) override
    {
        if (url.startsWithIgnoreCase ("https://") || url.startsWithIgnoreCase ("http://"))
            owner.openWebTab (url, true);
    }

    /** The scheme, host and port of an address ("https://example.com:8080"). */
    static juce::String originOf (const juce::String& url)
    {
        const auto scheme = url.upToFirstOccurrenceOf ("://", true, false);
        const auto rest = url.fromFirstOccurrenceOf ("://", false, false);
        return (scheme + rest.upToFirstOccurrenceOf ("/", false, false).upToFirstOccurrenceOf ("?", false, false)
                            .upToFirstOccurrenceOf ("#", false, false)).toLowerCase();
    }

private:
    bool isOwn (const juce::String& url) const
    {
        const auto root = getResourceProviderRoot();

        // Never data: (a link dropped on the window would load a stranger's page with the Hub's
        // bridge); blob: only for the page's own.
        if (url.startsWithIgnoreCase (root) || url.startsWithIgnoreCase ("about:")
            || url.startsWithIgnoreCase ("blob:" + root))
            return true;

        // macOS asks for a plugin screen's frame too (Windows only for the page itself).
        return url.contains ("://") && owner.screenOrigins.count (originOf (url)) > 0;
    }

    MainComponent& owner;
};


namespace
{
    const juce::Colour backgroundColour { 0xff232427 };

    std::vector<std::byte> toBytes (const char* data, int size)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, bytes.size());
        return bytes;
    }

    juce::String mimeTypeFor (const juce::String& path)
    {
        const auto extension = path.fromLastOccurrenceOf (".", false, false).toLowerCase();

        if (extension == "html")   return "text/html";
        if (extension == "css")    return "text/css";
        if (extension == "js")     return "text/javascript";
        if (extension == "svg")    return "image/svg+xml";
        if (extension == "png")    return "image/png";
        if (extension == "json")   return "application/json";
        if (extension == "md")     return "text/plain";

        return "application/octet-stream";
    }

    /** One of the page's files, compiled into the executable under its own name (nullptr: none). */
    const char* pageFile (const juce::String& name, int& size)
    {
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
            if (name == BinaryData::originalFilenames[i])
                return BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);

        return nullptr;
    }

    /** When anything in a made plugin's folder but its picture last changed. The folder is
        walked at most every few seconds (the library is pushed often; the walk reads the disk). */
    juce::Time newestChangeIn (const juce::File& folder)
    {
        // A .sip is one file: its own time says.
        if (! folder.isDirectory())
            return folder.getLastModificationTime();

        struct Checked { double atMs = 0.0; juce::Time newest; };
        static std::map<juce::String, Checked> checked;

        const auto now = juce::Time::getMillisecondCounterHiRes();
        auto& entry = checked[folder.getFullPathName()];

        if (entry.atMs > 0.0 && now - entry.atMs < 5000.0)
            return entry.newest;

        juce::Time newest;

        for (const auto& item : juce::RangedDirectoryIterator (folder, true, "*", juce::File::findFiles))
            if (item.getFile().getFileName() != MadePlugins::snapshotFileName)
                newest = juce::jmax (newest, item.getModificationTime());

        entry = { now, newest };
        return newest;
    }

    /** A made plugin's folder and when anything in it but its picture last changed. */
    juce::String snapshotKey (const juce::File& folder)
    {
        return folder.getFullPathName() + "|" + juce::String (newestChangeIn (folder).toMilliseconds());
    }

    /** Whether its picture is missing or older than what it shows. */
    bool snapshotDue (const juce::File& folder)
    {
        const auto picture = MadePlugins::snapshotOf (folder);
        return ! picture.existsAsFile() || newestChangeIn (folder) > picture.getLastModificationTime();
    }

    juce::Image pageImage (const juce::String& name)
    {
        int size = 0;

        if (const auto* data = pageFile (name, size))
            return juce::ImageCache::getFromMemory (data, size);

        return {};
    }

    juce::var notesToVar (juce::uint64 low, juce::uint64 high)
    {
        juce::Array<juce::var> notes;

        for (int note = 0; note < 128; ++note)
        {
            const auto word = note < 64 ? low : high;

            if (((word >> (note & 63)) & 1u) != 0)
                notes.add (note);
        }

        return notes;
    }

    const char* roleName (Plugin::Role role)
    {
        switch (role)
        {
            case Plugin::Role::instrument: return "instrument";
            case Plugin::Role::effect:     return "effect";
            case Plugin::Role::noAudio:    return "noteEffect";
        }

        return "instrument";
    }

    /** Main's fader, before and after: the Mastering Assistant's 0 dB, in the same undo step as
        its Sonify Mastering (faders are otherwise not part of the history). */
    class MainFaderChanged final : public Edits::Action
    {
    public:
        MainFaderChanged (AudioEngine& e, float beforeDb, float afterDb)
            : engine (e), before (beforeDb), after (afterDb) {}

        int getSizeInUnits() override   { return 1; }

    private:
        void redo() override         { engine.setMasterVolume (after); }
        void revert() override       { engine.setMasterVolume (before); }

        AudioEngine& engine;
        const float before, after;
    };

}

//==============================================================================
/** What the window shows while the page loads, the first seconds of WebView2 starting and the
    page's files coming in: Fanan's logo, Sonify's under it, and under them a spinner saying it's
    loading, on the startup question's own background.

    WebView2's window covers whatever the main window paints, so this is a native child window of
    its own laid over the page, as the web tabs' layer is. The page loads and draws under it; once
    the page has drawn itself (uiShown) this goes, and the question at startup is simply there. */
class MainComponent::Splash final : public juce::Component,
                                    private juce::Timer
{
public:
    explicit Splash (juce::Component& pageIn)
        : page (pageIn),
          fanan (pageImage ("fanan logo.png")),
          sonify (pageImage ("Sonify.png")),
          labelWidth (juce::GlyphArrangement::getStringWidth (font, label))
    {
        setOpaque (true);
        setTitle ("Sonify is loading");
        setWantsKeyboardFocus (false);
        setMouseClickGrabsKeyboardFocus (false);   // a click on it leaves the keyboard where it was
        startTimerHz (60);
    }

    /** Lies over the page, on top: a child window of the main window, once the main window is
        set up (its own window is made again while it is). */
    void cover()
    {
        auto* window = page.getTopLevelComponent();
        auto* peer = page.getPeer();

        if (peer == nullptr || ! window->isVisible())
            return;

        if (! isOnDesktop() || parentWindow != peer->getNativeHandle())
        {
            removeFromDesktop();
            addToDesktop (0, peer->getNativeHandle());
            parentWindow = peer->getNativeHandle();
        }

        setBounds (window->getLocalArea (&page, page.getLocalBounds()));
        setVisible (true);
        toFront (false);   // above the page's own window, which WebView2 makes later
    }

    /** The question's background (.scan-page: radial-gradient(ellipse at 50% 30%, #23262d 0%,
        #15161a 70%)), so the page takes over without a seam. That ellipse reaches the farthest
        corner: half the width and 70% of the height, each times the square root of two. */
    static void paintBackground (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const juce::Colour middle (0xff23262d), edge (0xff15161a);
        const auto cx = area.getCentreX(), cy = area.getY() + area.getHeight() * 0.3f;
        const auto rx = juce::jmax (1.0f, area.getWidth() * 0.5f * juce::MathConstants<float>::sqrt2);
        const auto ry = juce::jmax (1.0f, area.getHeight() * 0.7f * juce::MathConstants<float>::sqrt2);

        juce::ColourGradient gradient (middle, cx, cy, edge, cx + rx, cy, true);
        gradient.addColour (0.7, edge);

        juce::FillType fill (gradient);
        fill.transform = juce::AffineTransform::scale (1.0f, ry / rx, cx, cy);
        g.setFillType (fill);
        g.fillRect (area);
    }

    void paint (juce::Graphics& g) override
    {
        paintBackground (g, getLocalBounds().toFloat());

        const auto layout = getLayout();

        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

        if (fanan.isValid())
            g.drawImage (fanan, layout.fanan, juce::RectanglePlacement::centred);

        if (sonify.isValid())
            g.drawImage (sonify, layout.sonify, juce::RectanglePlacement::centred);

        // The page's own spinner: a faint ring with an arc going round it, a turn every 0.8 s.
        constexpr auto thickness = 2.5f;
        const auto ring = layout.spinner.reduced (thickness * 0.5f);
        const auto turn = static_cast<float> (std::fmod (juce::Time::getMillisecondCounterHiRes() / 800.0, 1.0))
                          * juce::MathConstants<float>::twoPi;

        g.setColour (juce::Colour (0xff3b3d42));
        g.drawEllipse (ring, thickness);

        juce::Path arc;
        arc.addCentredArc (ring.getCentreX(), ring.getCentreY(), ring.getWidth() * 0.5f, ring.getHeight() * 0.5f,
                           0.0f, turn, turn + juce::MathConstants<float>::halfPi, true);

        g.setColour (juce::Colour (0xff25984d));
        g.strokePath (arc, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (juce::Colour (0xffd6d7d9));
        g.setFont (font);
        g.drawText (label, layout.label, juce::Justification::centredLeft, false);
    }

private:
    struct Layout
    {
        juce::Rectangle<float> fanan, sonify, spinner, label;
    };

    static constexpr float logoWidth = 240.0f, logoGap = 14.0f, spinnerGap = 44.0f, spinnerSize = 24.0f, labelGap = 10.0f;

    static float heightOf (const juce::Image& image)
    {
        return image.isValid() ? logoWidth * static_cast<float> (image.getHeight()) / static_cast<float> (image.getWidth()) : 0.0f;
    }

    /** Both logos as wide as each other, Fanan's on top, then the spinner with its words: all
        of it in the middle of the window. */
    Layout getLayout() const
    {
        const auto area = getLocalBounds().toFloat();
        const auto fananHeight = heightOf (fanan), sonifyHeight = heightOf (sonify);
        const auto total = fananHeight + logoGap + sonifyHeight + spinnerGap + spinnerSize;
        const auto rowWidth = spinnerSize + labelGap + labelWidth;
        const auto x = area.getCentreX();
        auto y = area.getCentreY() - total * 0.5f;

        Layout layout;
        layout.fanan = { x - logoWidth * 0.5f, y, logoWidth, fananHeight };
        y += fananHeight + logoGap;
        layout.sonify = { x - logoWidth * 0.5f, y, logoWidth, sonifyHeight };
        y += sonifyHeight + spinnerGap;
        layout.spinner = { x - rowWidth * 0.5f, y, spinnerSize, spinnerSize };
        layout.label = { layout.spinner.getRight() + labelGap, y - 4.0f, labelWidth + 4.0f, spinnerSize + 8.0f };
        return layout;
    }

    void timerCallback() override
    {
        // WebView2 makes the page's window when it's ready, and nothing says where it puts it among
        // the main window's own: every quarter of a second this makes sure it's still on top.
        if (++ticks % 15 == 0)
            cover();

        repaint (getLayout().spinner.expanded (2.0f).getSmallestIntegerContainer());
    }

    juce::Component& page;
    const juce::Font font { juce::FontOptions().withName ("Segoe UI").withPointHeight (14.0f) };
    const juce::String label { "Loading..." };
    const juce::Image fanan, sonify;
    const float labelWidth;
    void* parentWindow = nullptr;      // the main window's own, which it's a child of
    int ticks = 0;
};

//==============================================================================
MainComponent::MainComponent()
{
    // The control surfaces first: the settings that follow set them up.
    surfaces = std::make_unique<ControlSurfaces> (static_cast<ControlSurfaces::Host&> (*this), [this] (std::vector<ControlSurfaces::Message>& into)
    {
        // The pad grids' messages go to them; the rest are the control surfaces'.
        drainSurfaceMessages();
        into.insert (into.end(), std::make_move_iterator (surfacePending.begin()), std::make_move_iterator (surfacePending.end()));
        surfacePending.clear();
    });

    // Pad grids (Launchpad, APC mini) playing Song Blocks' Live: their presses launch.
    padGrids = std::make_unique<PadGrids>();
    padGrids->onPress = [this] (const juce::String& action, int row, int column)
    {
        juce::StringArray parts { "launch" };

        if (action == "cell")         parts.addArray ({ "cell", juce::String (row), juce::String (column) });
        else if (action == "row")     parts.addArray ({ "row", juce::String (row) });
        else if (action == "stop")    parts.addArray ({ "stop", juce::String (column) });
        else if (action == "stopAll") parts.add ("stopall");
        else                          return;

        linkSetLauncher (parts, true);
    };
    surfaces->onProblemsChanged = [this] { pushSettings(); };   // a loopback found: MIDI Setup says so

    // MIDI Link (the toolbar's strip), with the map as it was left: the devices follow with the settings.
    setupMidiLink();

    // The Live Show's Controller (its pads play it from the start; its window opens when asked).
    setupLiveControl();

    // Containers ask Sonify for plugins and open their own windows.
    setupContainers();

    // The Sampling Studio, and the Sonify Sampler's window (MainSampling.cpp).
    setupSampling();
    VideoMedia::setCacheFolder (SettingsFolder::folder ("Video Cache", "Video Cache"));
    VideoMedia::tidyCache();

    // Sonify's settings first: the user data folder, the external MIDI devices and the rest.
    loadPreferences();
    engine.getDeviceManager().addChangeListener (this);   // MIDI ports coming and going
    loadRecentSongs();
    updateRecordingFolder();
    loadInstalledContent();
    loadInternetBlocked();
    cloud = std::make_unique<Cloud> (Cloud::loadSettings());
    cloud->setBlocked (internetBlocked);

    // The Fanan account: signed in by itself at every start once this computer is linked.
    server = std::make_unique<FananServer> (FananServer::loadSettings());
    server->setBlocked (internetBlocked);
    server->onStateChanged = [this]
    {
        if (server->isSignedIn())
            waitingForSignIn = false;   // back from the browser

        pushAccount();

        if (creator != nullptr)
            creator->connectionChanged();   // the Plugins Creator Studio's own account box
    };

    aiTools = std::make_unique<AiTools> (engine, library, AiTools::Hooks {
        [this] (int channel, int slot) { openPluginScreen (channel, slot); return true; },
        [this]
        {
            // The AI saves to the song's file; a song never saved asks the user, not the AI.
            if (songFile == juce::File())
                return juce::Result::fail ("This song hasn't been saved yet: the user picks where (File > Save).");

            return writeSong (songFile) ? juce::Result::ok() : juce::Result::fail ("The song couldn't be written.");
        },
        [this] (int trackId, int clipId, const juce::StringArray& stems, bool muteOriginal)
        {
            return splitStems (trackId, clipId, stems, muteOriginal);
        },
        [this] (const juce::String& title, const juce::String& message, const juce::String& yesText, std::function<void (bool)> answer)
        {
            // Asked by Sonify itself, not the AI: only the user's own click lets it go on.
            juce::NativeMessageBox::showAsync (juce::MessageBoxOptions()
                                                   .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                                   .withTitle (title)
                                                   .withMessage (message)
                                                   .withButton (yesText)
                                                   .withButton ("Keep it")
                                                   .withAssociatedComponent (this),
                                               [answer = std::move (answer)] (int button) { answer (button == 0); });
        },
        [this] { return pageCommands; },
        [this] (const juce::String& command, const juce::String& event, const juce::var& payload)
        {
            // Done by the page itself, exactly as its menus and buttons do it.
            if (webView == nullptr)
                return;

            juce::DynamicObject::Ptr item (new juce::DynamicObject());

            if (command.isNotEmpty())
                item->setProperty ("command", command);
            else
            {
                item->setProperty ("event", event);
                item->setProperty ("payload", payload);
            }

            webView->emitEventIfBrowserIsVisible ("aiRun", juce::var (item.get()));
        } });

    ai = std::make_unique<AiAgent> (*server, *aiTools, AiAgent::Hooks {
        [this] (const juce::String&) { aiSongBefore = songSnapshot(); },
        [this] (const juce::String& request, bool changed)
        {
            // Everything the request did: one undo step, named after it.
            if (changed)
            {
                const auto after = songSnapshot();

                if (after != aiSongBefore)
                    addToHistory ("AI: " + request.substring (0, 40) + (request.length() > 40 ? "..." : ""),
                                  new Edits::SongSnapshot ([this] (const juce::String& song) { restoreSnapshot (song); }, aiSongBefore, after));
            }

            aiSongBefore.clear();
            pushTracks();
            pushSong();

            // A spoken request: the musician hears how it ended.
            if (voiceRequest)
            {
                voiceRequest = false;
                const auto outcome = ai->getLastOutcome();

                engine.playCue (outcome == AiAgent::Outcome::done     ? AudioEngine::Cue::done
                              : outcome == AiAgent::Outcome::question ? AudioEngine::Cue::question
                                                                      : AudioEngine::Cue::problem);
            }
        },
        [this] { pushTracks(); pushAi(); } });

    // Deep, as it was left: every request to the strongest model.
    ai->setDeep (static_cast<bool> (juce::JSON::parse (aiSettingsFile()).getProperty ("deep", false)));

    // Native plugins load only from the user's own plugin places (Plugin::isTrusted), from the start.
    trustLibrary (library);

    // The Guide's "Ask anything about Sonify" (MainGuide.cpp).
    setupGuide();

    // Sonify devices, and the Plugins Creator Studio's own Sonify AI.
    setupDevices();

    // AI Mix and AI Master: Fanan AI on the Analog Board and the Mastering section (MainAiMix.cpp).
    setupAiMix();

    // The Plugins Creator Studio: Stella AI Studio inside Sonify (MainCreator.cpp).
    setupCreator();

    // Every plugin window's ON/OFF, for the plugins in the song.
    pluginSwitcher = PluginHeader::addSwitcher ({ [this] (const Plugin& plugin) { return engine.getPluginOn (plugin); },
                                                  [this] (const Plugin& plugin, bool on) { switchPlugin (plugin, on); } });

    // Voice control: always off at launch, listening to the input last chosen for it.
    loadVoiceInput();
    voice = std::make_unique<VoiceControl> (engine, *server);
    voice->onCommand = [this] (const juce::String& command) { runVoiceCommand (command); };
    voice->onChanged = [this] { pushVoice(); };
    followVoiceInput();

    // Sonify Remote: always off at launch.
    phoneRemote = std::make_unique<PhoneRemote> (*server);
    phoneRemote->getConversation = [this] { return ai->getState(); };
    phoneRemote->onAsk = [this] (const juce::String& text, bool spoken) { askFromPhone (text, spoken); };
    phoneRemote->onStop = [this] { ai->stop(); pushAi(); };
    phoneRemote->onChanged = [this] { pushRemote(); };
    phoneRemote->getLocalPage = []
    {
        int size = 0;
        const auto* data = pageFile ("phone-remote.html", size);
        return data != nullptr ? juce::MemoryBlock (data, (size_t) size) : juce::MemoryBlock();
    };
    phoneRemote->getLocalLogo = []
    {
        int size = 0;
        const auto* data = pageFile ("fanan logo.png", size);
        return data != nullptr ? juce::MemoryBlock (data, (size_t) size) : juce::MemoryBlock();
    };

    const auto options = createWebOptions();

    if (juce::WebBrowserComponent::areOptionsSupported (options))
    {
        webView = std::make_unique<HubPageView> (*this, options);
        addAndMakeVisible (*webView);
        webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

        SettingsFolder::removeOldWebViewProfiles();   // the tabs' and the store's own, from before they shared the page's

        // Until the page has drawn itself, the window shows Fanan's and Sonify's logos loading.
        splash = std::make_unique<Splash> (*this);
    }
    else
    {
        webViewError = "The Microsoft Edge WebView2 Runtime is missing.\n"
                       "Install it from Microsoft, then restart Sonify.";
    }

    // Most of the screen, like any DAW, without covering the taskbar.
    auto width = 1240, height = 760;

    if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto screen = display->userBounds.toNearestInt();
        width = juce::jlimit (820, 1680, screen.getWidth() * 88 / 100);
        height = juce::jlimit (560, 1000, screen.getHeight() * 88 / 100);
    }

    setSize (width, height);

    // The account's first check goes out now; the page hears of it once it's loaded.
    server->start();

    // The splash goes over the page once the window is set up (just after this), before it's
    // first painted. A page that never says it has drawn itself doesn't keep it there for good.
    placeSplash();
    dismissSplash (30000);
}

MainComponent::~MainComponent()
{
    Containers::setHost (nullptr);      // containers going from here on ask nothing of this
    PluginHeader::removeSwitcher (pluginSwitcher);   // nor do plugin windows' headers
    aiMixCancelled->store (true);       // AI Mix's listening stops
    aiMixWorker.removeAllJobs (true, 10000);
    splash = nullptr;                   // closed while the page loads: the splash's window goes first
    stopTimer();                        // no status (nor control surface feedback) while things go
    automationTimer.stopTimer();
    samplingTimer.stopTimer();
    sampling = nullptr;                 // a session stops: its track let go, its takes thrown away
    sampleBuild = nullptr;              // the Builder stops (its folder as far as it got)
    samplingDropZone = nullptr;
    samplingRackTimer.stopTimer();
    midiMonitorTimer.stopTimer();

    if (samplingRack != nullptr)
    {
        keepSampling();                 // the nodes' plugins' settings, for next time
        engine.setSamplingRack (nullptr);
        samplingRack = nullptr;         // their windows, then their plugins, while the plugin hosts still run
    }
    loopTimer.stopTimer();
    loopWorker.removeAllJobs (true, 10000);   // the Loop Editor's Batch Find All (or Fanan AI's listening) stops
    engine.stopPreview();
    auditionOutput = nullptr;
    liveClosing = true;                 // the Live Show's listening stops, and what it knows is kept
    deckLoader.removeAllJobs (true, 5000);   // a track (or a video's sound) being read for a deck stops
    liveListener.removeAllJobs (true, videoStarting.load() ? 30000 : 5000);   // VLC starting can't be stopped: it's given time
    podcastExport = nullptr;            // a podcast being made stops (its unfinished file goes)
    liveControl = nullptr;              // the Controller's window closes, its pads' lights go out, its setup is kept
    liveVideo = nullptr;                // a stream ends (its destinations closed), the cameras let go
    engine.closeLiveTap();
    liveDropZone = nullptr;
    liveScreen = nullptr;               // the show's screen: its windows on the other screens go, VLC lets go

    for (auto& preparation : deckPreparing)
        preparation = nullptr;          // a video being read out for a deck stops

    if (trackLibrary != nullptr && trackLibrary->needsSaving())
        trackLibrary->save();

    masteringAssistant = nullptr;       // a mastering being worked out stops (it reads its own file)
    stemSplitter = nullptr;             // stems being split stop, their unfinished files gone
    modelDownload = nullptr;
    audioRender = nullptr;              // an Audio menu job under way stops
    audioToMidi = nullptr;
    audioTempo = nullptr;
    masteringRender.deleteFile();
    videoImport = nullptr;              // a video being read, or exported, stops
    videoPreparing = nullptr;
    videoExport = nullptr;
    videoExportMix.deleteFile();
    videoWindow = nullptr;              // the Video window goes before its picture
    videoPicture = nullptr;
    engine.getDeviceManager().removeChangeListener (this);
    surfaces = nullptr;                 // the control surfaces go dark while Sonify is still here
    padGrids = nullptr;                 // and the pad grids
    midiLink = nullptr;                 // MIDI Link stops taking what the devices send
    voice = nullptr;                    // its listener stops before anything it reads goes
    phoneRemote = nullptr;              // the phone hears it's gone
    server->onStateChanged = nullptr;   // no account updates while the window goes
    guideAi = nullptr;                  // the Guide's question under way is dropped
    creator = nullptr;                  // the Plugins Creator Studio: its plugin stops playing, its last change is saved
    creatorLayer = nullptr;             // (before the devices it makes MIDI FX with)
    deviceTimer.stopTimer();            // devices' moves stop reaching Sonify
    deviceAgent = nullptr;              // a request under way is dropped
    deviceStudio = nullptr;             // a build or test under way finds it gone

    // Tracks and plugins kept for undo go first, while the WebAssembly engine still runs.
    undoManager.clearUndoHistory();
    engine.discardRenders (true);       // frozen tracks' renders no saved song names
    Devices::forgetAll();               // the devices' builds go with the last device playing them
}

//==============================================================================
//==============================================================================
namespace
{
    /** Runs a page event's handler just after WebView2's own callback has returned, in the order
        the events came. JUCE calls event listeners from inside WebView2's message callback, and
        WebView2 must not be re-entered from there: a window brought to the front, or Windows'
        device and COM calls pumping messages (the audio settings list every device), can crash it. */
    template <typename Handler>
    std::function<void (const juce::var&)> afterEvent (MainComponent* owner, Handler handler)
    {
        return [safe = juce::Component::SafePointer<MainComponent> (owner), handler = std::move (handler)] (const juce::var& event)
        {
            juce::MessageManager::callAsync ([safe, handler, event]
            {
                if (safe != nullptr)
                    handler (event);
            });
        };
    }
}

juce::WebBrowserComponent::Options MainComponent::createWebOptions()
{
    // The page stays loaded while the window is minimised: by default JUCE unloads a hidden
    // browser's page and loads it afresh when shown, which brought back the Start page.
    auto options = juce::WebBrowserComponent::Options {}
                       .withKeepPageLoadedWhenBrowserIsHidden()
                       .withNativeIntegrationEnabled()
                       .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })
                       .withEventListener ("uiReady", afterEvent (this, [this] (const juce::var&) { handleUiReady(); }))
                       // The page has drawn itself: a moment more for that frame to reach the screen, and the splash goes.
                       .withEventListener ("uiShown", afterEvent (this, [this] (const juce::var&) { dismissSplash (100); }))
                       .withEventListener ("closeSong", afterEvent (this, [this] (const juce::var&) { closeSong(); }))
                       .withEventListener ("startPageClosed", afterEvent (this, [this] (const juce::var&) { noProject = false; }))
                       .withEventListener ("aiSend", afterEvent (this, [this] (const juce::var& event)
                       {
                           // The flag on the request: one of the studios the page offers, or none.
                           static const juce::StringArray studios { "Studio", "Analog Board", "Song Builder Blocks",
                                                                    "Plugins Creator Studio", "Live Show Studio", "Sampling Studio",
                                                                    "Web Browser" };
                           auto studio = event.getProperty ("studio", {}).toString().trim();

                           if (! studios.contains (studio))
                               studio = {};

                           ai->send (event.getProperty ("text", {}).toString(), AiAgent::Origin::typed, studio);
                       }))
                       .withEventListener ("aiStop", afterEvent (this, [this] (const juce::var&) { ai->stop(); pushAi(); }))
                       .withEventListener ("aiCommandList", afterEvent (this, [this] (const juce::var& event)
                       {
                           // The page's commands, for Fanan AI (AiTools::Hooks::pageCommands).
                           pageCommands.clear();

                           if (const auto* names = event.getProperty ("names", {}).getArray())
                               for (const auto& name : *names)
                                   pageCommands.addIfNotAlreadyThere (name.toString());
                       }))
                       .withEventListener ("aiClear", afterEvent (this, [this] (const juce::var&) { ai->clear(); }))
                       .withEventListener ("aiSettings", afterEvent (this, [this] (const juce::var& event)
                       {
                           // Deep: every request to the strongest model, kept for next time.
                           if (event.hasProperty ("deep"))
                           {
                               ai->setDeep (static_cast<bool> (event.getProperty ("deep", false)));

                               juce::DynamicObject::Ptr saved (new juce::DynamicObject());
                               saved->setProperty ("deep", ai->isDeep());
                               aiSettingsFile().replaceWithText (juce::JSON::toString (juce::var (saved.get())));
                           }

                           pushAi();
                       }))
                       .withEventListener ("guide", afterEvent (this, [this] (const juce::var& event) { handleGuide (event); }))
                       .withEventListener ("board", afterEvent (this, [this] (const juce::var& event) { handleBoard (event); }))
                       .withEventListener ("aiMix", afterEvent (this, [this] (const juce::var& event) { handleAiMix (event); }))
                       .withEventListener ("live", afterEvent (this, [this] (const juce::var& event) { handleLive (event); }))
                       .withEventListener ("deviceStudio", afterEvent (this, [this] (const juce::var& event) { handleDeviceStudio (event); }))
                       .withEventListener ("deviceMap", afterEvent (this, [this] (const juce::var& event) { handleDeviceMap (event); }))
                       // A protected MIDI FX's Register (in its demo): its Registration window.
                       .withEventListener ("deviceRegister", afterEvent (this, [this] (const juce::var& event) { handleDeviceRegister (event); }))
                       .withEventListener ("deviceAiSend", afterEvent (this, [this] (const juce::var& event)
                       {
                           // A request with no device open starts a new one.
                           if (! deviceStudio->isOpen() && ! deviceAgent->isBusy())
                           {
                               deviceStudio->newDevice();
                               deviceAgent->restoreConversation ({});
                           }

                           deviceAgent->send (event.getProperty ("text", {}).toString());
                           pushDeviceStudio();
                       }))
                       .withEventListener ("deviceAiStop", afterEvent (this, [this] (const juce::var&) { deviceAgent->stop(); pushDeviceAi(); }))
                       .withEventListener ("deviceAiClear", afterEvent (this, [this] (const juce::var&)
                       {
                           deviceAgent->clear();

                           if (deviceStudio->isOpen() && ! deviceAgent->isBusy())
                               deviceStudio->keepConversation (deviceAgent->saveConversation());
                       }))
                       .withEventListener ("voiceSettings", afterEvent (this, [this] (const juce::var& event)
                       {
                           if constexpr (voiceControlOffered)
                           {
                               if (event.hasProperty ("input"))
                                   chooseVoiceInput (static_cast<int> (event.getProperty ("input", -1)));

                               if (event.hasProperty ("enabled"))
                                   voice->setEnabled (static_cast<bool> (event.getProperty ("enabled", false)));
                           }

                           pushVoice();
                       }))
                       .withEventListener ("remoteSettings", afterEvent (this, [this] (const juce::var& event)
                       {
                           if constexpr (phoneRemoteOffered)
                           {
                               if (event.hasProperty ("enabled"))
                                   phoneRemote->setEnabled (static_cast<bool> (event.getProperty ("enabled", false)));

                               if (static_cast<bool> (event.getProperty ("newKey", false)))
                                   phoneRemote->newPairingKey();
                           }

                           pushRemote();
                       }))
                       .withEventListener ("setInternetBlocked", afterEvent (this, [this] (const juce::var& event)
                       {
                           setInternetBlocked (static_cast<bool> (event.getProperty ("blocked", false)));
                       }))
                       .withEventListener ("noteOn", afterEvent (this, [this] (const juce::var& event)
                       {
                           engine.noteOn (static_cast<int> (event.getProperty ("note", 60)),
                                          static_cast<float> (static_cast<double> (event.getProperty ("velocity", 0.8))));
                       }))
                       .withEventListener ("noteOff", afterEvent (this, [this] (const juce::var& event)
                       {
                           engine.noteOff (static_cast<int> (event.getProperty ("note", 60)));
                       }))
                       .withEventListener ("allNotesOff", afterEvent (this, [this] (const juce::var&) { engine.allNotesOff(); }))
                       .withEventListener ("studioShown", afterEvent (this, [this] (const juce::var& event)
                       {
                           studioShown (event.getProperty ("id", "studio").toString());
                       }))
                       .withEventListener ("transport", afterEvent (this, [this] (const juce::var& event) { handleTransport (event); }))
                       .withEventListener ("addFxChannel", afterEvent (this, [this] (const juce::var& event)
                       {
                           // Like Studio One's: an FX channel, and (from a send menu) a send to it straight away.
                           const auto id = engine.addTrack (Track::Kind::fx);
                           addToHistory ("Add FX Channel", new Edits::TrackAdded (engine, id, static_cast<int> (engine.getTracks().size()) - 1));
                           const auto from = static_cast<int> (event.getProperty ("from", 0));

                           if (from != 0)
                           {
                               const auto before = engine.getSends (from);
                               Track::Send send;
                               send.target = id;

                               if (engine.addSend (from, send) != 0)
                                   addToHistory ("Add FX Channel", new Edits::SendsChanged (engine, from, before, engine.getSends (from)), true);
                           }

                           pushTracks();
                       }))
                       .withEventListener ("addSend", afterEvent (this, [this] (const juce::var& event)
                       {
                           const auto trackId = static_cast<int> (event.getProperty ("trackId", 0));
                           const auto before = engine.getSends (trackId);
                           Track::Send send;
                           send.sidechain = static_cast<bool> (event.getProperty ("sidechain", false));
                           send.target = static_cast<int> (event.getProperty ("target", 0));
                           send.slot = static_cast<int> (event.getProperty ("slot", 0));

                           if (engine.addSend (trackId, send) != 0 && engine.getSends (trackId).size() != before.size())
                               addToHistory (send.sidechain ? "Add Sidechain Send" : "Add Send",
                                             new Edits::SendsChanged (engine, trackId, before, engine.getSends (trackId)));

                           pushTracks();
                       }))
                       .withEventListener ("setSend", afterEvent (this, [this] (const juce::var& event)
                       {
                           const auto trackId = static_cast<int> (event.getProperty ("trackId", 0));
                           const auto sendId = static_cast<int> (event.getProperty ("sendId", 0));
                           auto before = engine.getSends (trackId);

                           if (event.hasProperty ("on"))
                               engine.setSendOn (trackId, sendId, static_cast<bool> (event.getProperty ("on", true)));

                           if (event.hasProperty ("level"))
                               engine.setSendLevel (trackId, sendId, static_cast<float> (static_cast<double> (event.getProperty ("level", 0.0))));

                           // A drag is one step in the history, from where it started; its moves only reach the engine.
                           if (static_cast<bool> (event.getProperty ("commit", true)))
                           {
                               if (event.hasProperty ("from"))
                                   for (auto& send : before)
                                       if (send.id == sendId)
                                           send.levelDb = static_cast<float> (static_cast<double> (event.getProperty ("from", 0.0)));

                               // Nothing changed (a click without a move, a level it already had): no step.
                               const auto after = engine.getSends (trackId);
                               const auto same = before.size() == after.size()
                                              && std::equal (before.begin(), before.end(), after.begin(), [] (const Track::Send& a, const Track::Send& b)
                                                             { return a.id == b.id && juce::exactlyEqual (a.levelDb, b.levelDb) && a.on == b.on; });

                               if (! same)
                                   addToHistory ("Change Send", new Edits::SendsChanged (engine, trackId, before, after));

                               pushTracks();
                           }
                       }))
                       .withEventListener ("removeSend", afterEvent (this, [this] (const juce::var& event)
                       {
                           const auto trackId = static_cast<int> (event.getProperty ("trackId", 0));
                           const auto before = engine.getSends (trackId);
                           engine.removeSend (trackId, static_cast<int> (event.getProperty ("sendId", 0)));

                           // No such send (already gone): no step.
                           if (const auto after = engine.getSends (trackId); after.size() != before.size())
                               addToHistory ("Remove Send", new Edits::SendsChanged (engine, trackId, before, after));

                           pushTracks();
                       }))
                       .withEventListener ("addTrack", afterEvent (this, [this] (const juce::var& event)
                       {
                           const auto audio = event.getProperty ("kind", {}).toString() == "audio";
                           const auto id = engine.addTrack (audio ? Track::Kind::audio : Track::Kind::instrument);
                           addToHistory (audio ? "Add Audio Track" : "Add Track",
                                         new Edits::TrackAdded (engine, id, static_cast<int> (engine.getTracks().size()) - 1));
                           const auto file = event.getProperty ("file", {}).toString();

                           // A track made from a Browser instrument: its loading joins the same undo step.
                           if (file.isNotEmpty())
                               loadPluginInto (id, juce::File (file), event.getProperty ("pluginId", {}).toString(), -1, 0, true);
                           else
                               pushTracks();
                       }))
                       .withEventListener ("removeTrack", afterEvent (this, [this] (const juce::var& event)
                       {
                           const auto id = static_cast<int> (event.getProperty ("id", 0));
                           int index = 0;

                           // Kept, plugins and all, so undo brings it back exactly.
                           if (auto removed = engine.detachTrack (id, index))
                               addToHistory ("Remove Track", new Edits::TrackRemoved (engine, id, index, std::move (removed)));

                           loadingFiles.erase (id);
                           trackMessages.erase (id);
                           pushTracks();
                       }))
                       .withEventListener ("selectTrack", afterEvent (this, [this] (const juce::var& event)
                       {
                           const auto id = static_cast<int> (event.getProperty ("id", 0));
                           engine.selectTrack (id);
                           inputFollowsSelection (id);
                           pushTracks();
                       }))
                       // A choir track's Choir window: { id, settings } (Choir.h). As a plugin's knobs, the
                       // song changes but it isn't an undo step.
                       .withEventListener ("choirSettings", afterEvent (this, [this] (const juce::var& event)
                       {
                           const auto id = static_cast<int> (event.getProperty ("id", 0));

                           if (engine.setTrackChoir (id, ChoirSettings::fromVar (event.getProperty ("settings", {}))))
                               markChanged();
                       }))
                       .withEventListener ("trackSettings", afterEvent (this, [this] (const juce::var& event)
                       {
                           markChanged();
                           const auto id = static_cast<int> (event.getProperty ("id", 0));
                           const auto nameBefore = Edits::nameOf (engine, id);
                           handleTrackSettings (event);
                           const auto nameAfter = Edits::nameOf (engine, id);

                           // Renaming is an edit; faders, pan, mute and solo are mixing, as in Studio One.
                           if (event.hasProperty ("name") && (nameBefore.text != nameAfter.text || nameBefore.automatic != nameAfter.automatic))
                               addToHistory ("Rename Track", new Edits::TrackRenamed (engine, id, nameBefore, nameAfter));
                       }))
                       // A track's snowflake: freeze it, stop its freezing, or thaw it.
                       .withEventListener ("trackFreeze", afterEvent (this, [this] (const juce::var& event)
                       {
                           const auto id = static_cast<int> (event.getProperty ("id", 0));

                           if (static_cast<bool> (event.getProperty ("on", false)))
                               startFreezing (id);
                           else
                               stopFreezing (id);
                       }))
                       .withEventListener ("masterSettings", afterEvent (this, [this] (const juce::var& event)
                       {
                           markChanged();

                           if (event.hasProperty ("volume"))
                               engine.setMasterVolume (static_cast<float> (static_cast<double> (event.getProperty ("volume", 0.0))));

                           // Fader moves are not echoed back: the page already shows them.
                           if (event.hasProperty ("mute"))
                           {
                               engine.setMasterMute (static_cast<bool> (event.getProperty ("mute", false)));
                               pushTracks();
                           }
                       }))
                       .withEventListener ("loadPlugin", afterEvent (this, [this] (const juce::var& event)
                       {
                           markChanged();
                           loadPluginInto (static_cast<int> (event.getProperty ("trackId", 0)),
                                           juce::File (event.getProperty ("file", {}).toString()),
                                           event.getProperty ("pluginId", {}).toString(),
                                           static_cast<int> (event.getProperty ("position", -1)),
                                           static_cast<int> (event.getProperty ("replaceInsertId", 0)));
                       }))
                       .withEventListener ("clearInstrument", afterEvent (this, [this] (const juce::var& event)
                       {
                           const auto id = static_cast<int> (event.getProperty ("trackId", 0));
                           const auto name = Edits::nameOf (engine, id);

                           if (auto removed = engine.swapInstrument (id, nullptr))
                               addToHistory ("Remove Instrument", new Edits::InstrumentChanged (engine, id, std::move (removed), name, name));

                           trackMessages.erase (id);
                           pushTracks();
                       }))
                       .withEventListener ("removeInsert", afterEvent (this, [this] (const juce::var& event)
                       {
                           const auto channel = static_cast<int> (event.getProperty ("trackId", 0));
                           const auto insertId = static_cast<int> (event.getProperty ("insertId", 0));
                           int position = 0;

                           if (auto removed = engine.detachInsert (channel, insertId, position))
                               addToHistory ("Remove Insert", new Edits::InsertRemoved (engine, channel, insertId, position, std::move (removed)));

                           trackMessages.erase (channel);
                           pushTracks();
                       }))
                       .withEventListener ("moveInsert", afterEvent (this, [this] (const juce::var& event)
                       {
                           const auto channel = static_cast<int> (event.getProperty ("trackId", 0));
                           const auto insertId = static_cast<int> (event.getProperty ("insertId", 0));
                           const auto from = engine.getInsertPosition (channel, insertId);
                           engine.moveInsert (channel, insertId, static_cast<int> (event.getProperty ("position", 0)));
                           const auto to = engine.getInsertPosition (channel, insertId);

                           if (from >= 0 && from != to)
                               addToHistory ("Move Insert", new Edits::InsertMoved (engine, channel, insertId, from, to));

                           pushTracks();
                       }))
                       .withEventListener ("bypassInsert", afterEvent (this, [this] (const juce::var& event)
                       {
                           markChanged();
                           engine.setInsertBypass (static_cast<int> (event.getProperty ("trackId", 0)),
                                                   static_cast<int> (event.getProperty ("insertId", 0)),
                                                   static_cast<bool> (event.getProperty ("bypass", false)));
                           pushTracks();
                       }))
                       .withEventListener ("pluginOn", afterEvent (this, [this] (const juce::var& event)
                       {
                           // A page window's ON/OFF (hub-screens.js): the plugin in that slot.
                           if (auto* plugin = engine.getPlugin (static_cast<int> (event.getProperty ("channel", 0)),
                                                                static_cast<int> (event.getProperty ("slot", 0))))
                               switchPlugin (*plugin, static_cast<bool> (event.getProperty ("on", true)));
                       }))
                       .withEventListener ("setParam", afterEvent (this, [this] (const juce::var& event) { markChanged(); setPluginParameter (event); }))
                       .withEventListener ("setClipNotes", afterEvent (this, [this] (const juce::var& event)
                       {
                           // With a window when the edit moved the clip's edges.
                           AudioEngine::ClipWindow window;
                           const bool hasWindow = event.hasProperty ("start") && event.hasProperty ("length");

                           if (hasWindow)
                               window = { static_cast<double> (event.getProperty ("start", 0.0)),
                                          static_cast<double> (event.getProperty ("length", 0.0)),
                                          static_cast<double> (event.getProperty ("offset", 0.0)) };

                           const auto trackId = static_cast<int> (event.getProperty ("trackId", 0));
                           auto before = Edits::snapshot (engine, { trackId });

                           // The pitch bend and aftertouch lines, when the Editor sends them.
                           std::optional<std::vector<MidiControlPoint>> bend, pressure;

                           if (event.hasProperty ("pitchBend"))
                               bend = controlsFromVar (event.getProperty ("pitchBend", {}), MidiClip::pitchBendMin, MidiClip::pitchBendMax);

                           if (event.hasProperty ("aftertouch"))
                               pressure = controlsFromVar (event.getProperty ("aftertouch", {}), 0, MidiClip::aftertouchMax);

                           // The clip's seed, when the Editor's Seed changed it.
                           std::optional<juce::uint32> seed;

                           if (event.hasProperty ("seed"))
                               seed = static_cast<juce::uint32> (static_cast<juce::int64> (event.getProperty ("seed", 0)));

                           if (engine.setClipNotes (trackId, static_cast<int> (event.getProperty ("clipId", 0)),
                                                    notesFromVar (event.getProperty ("notes", {})),
                                                    hasWindow ? &window : nullptr,
                                                    bend ? &*bend : nullptr, pressure ? &*pressure : nullptr,
                                                    seed ? &*seed : nullptr))
                           {
                               // A MIDI tool's notes go into the history under the tool's name.
                               const auto label = event.getProperty ("label", {}).toString().substring (0, 60);
                               addToHistory (label.isNotEmpty() ? label : juce::String ("Edit Notes"),
                                             new Edits::ClipsChanged (engine, std::move (before), Edits::snapshot (engine, { trackId })));
                               pushTracks();
                           }
                       }))
                       .withEventListener ("clipEdits", afterEvent (this, [this] (const juce::var& event)
                       {
                           applyClipEdits (event.getProperty ("edits", {}), event.getProperty ("label", {}).toString().substring (0, 60));
                       }))
                       .withEventListener ("deleteClip", afterEvent (this, [this] (const juce::var& event)
                       {
                           const auto trackId = static_cast<int> (event.getProperty ("trackId", 0));
                           auto before = Edits::snapshot (engine, { trackId });

                           if (engine.deleteClip (trackId, static_cast<int> (event.getProperty ("clipId", 0))))
                           {
                               addToHistory ("Delete Clip", new Edits::ClipsChanged (engine, std::move (before), Edits::snapshot (engine, { trackId })));
                               pushTracks();
                           }
                       }))
                       .withEventListener ("listFolder", afterEvent (this, [this] (const juce::var& event) { listFolder (event.getProperty ("path", {}).toString()); }))
                       .withEventListener ("chooseFilesFolder", afterEvent (this, [this] (const juce::var&) { chooseFilesFolder(); }))
                       // The Fanan account: signing in happens in the browser (the site links this computer).
                       .withEventListener ("accountSignIn", afterEvent (this, [this] (const juce::var& event)
                       {
                           signIn (static_cast<bool> (event.getProperty ("buy", false)), event.getProperty ("email", {}).toString());
                       }))
                       .withEventListener ("accountSignInCancel", afterEvent (this, [this] (const juce::var&)
                       {
                           waitingForSignIn = false;
                           pushAccount();
                       }))
                       .withEventListener ("accountSignOut", afterEvent (this, [this] (const juce::var&) { signOut(); }))
                       .withEventListener ("accountBuy", afterEvent (this, [this] (const juce::var&) { buyCredits(); }))
                       .withEventListener ("accountCheck", afterEvent (this, [this] (const juce::var&) { server->checkNow(); }))
                       .withEventListener ("contentList", afterEvent (this, [this] (const juce::var&) { listContent(); }))
                       .withEventListener ("contentInstall", afterEvent (this, [this] (const juce::var& event) { installContent (event.getProperty ("id", {}).toString()); }))
                       .withEventListener ("contentPreview", afterEvent (this, [this] (const juce::var& event) { previewContent (event.getProperty ("id", {}).toString()); }))
                       .withEventListener ("contentOpen", afterEvent (this, [this] (const juce::var& event) { openContent (event.getProperty ("id", {}).toString()); }))
                       .withEventListener ("webTabNew", afterEvent (this, [this] (const juce::var& event) { openWebTab (event.getProperty ("url", {}).toString(), true); }))
                       .withEventListener ("webTabShow", afterEvent (this, [this] (const juce::var& event)
                       {
                           showWebTab (static_cast<int> (event.getProperty ("id", 0)),
                                       { static_cast<int> (event.getProperty ("x", 0)), static_cast<int> (event.getProperty ("y", 0)),
                                         static_cast<int> (event.getProperty ("width", 0)), static_cast<int> (event.getProperty ("height", 0)) });
                       }))
                       .withEventListener ("webTabHide", afterEvent (this, [this] (const juce::var&) { hideWebTabs(); }))
                       .withEventListener ("webTabNavigate", afterEvent (this, [this] (const juce::var& event)
                       {
                           navigateWebTab (static_cast<int> (event.getProperty ("id", 0)), event.getProperty ("url", {}).toString());
                       }))
                       .withEventListener ("webTabCommand", afterEvent (this, [this] (const juce::var& event)
                       {
                           auto* tab = findWebTab (static_cast<int> (event.getProperty ("id", 0)));

                           if (tab == nullptr || tab->view == nullptr)
                               return;

                           const auto command = event.getProperty ("command", {}).toString();

                           if (command == "back")         tab->view->goBack();
                           else if (command == "forward") tab->view->goForward();
                           else if (command == "reload")  tab->view->refresh();
                           else if (command == "stop")    tab->view->stop();
                       }))
                       .withEventListener ("webTabClose", afterEvent (this, [this] (const juce::var& event) { closeWebTab (static_cast<int> (event.getProperty ("id", 0))); }))
                       .withEventListener ("exportMixdown", afterEvent (this, [this] (const juce::var& event) { exportMixdown (event); }))
                       .withEventListener ("masterSong", afterEvent (this, [this] (const juce::var& event) { startMastering (event); }))
                       .withEventListener ("cancelMastering", afterEvent (this, [this] (const juce::var&) { cancelMastering(); }))
                       .withEventListener ("masteringState", afterEvent (this, [this] (const juce::var&) { emitMastering (masteringStateName(), masteringProgress()); }))
                       .withEventListener ("masteringListen", afterEvent (this, [this] (const juce::var& event)
                       {
                           setMasteringListen (static_cast<bool> (event.getProperty ("original", false)),
                                               static_cast<bool> (event.getProperty ("sameLoudness", false)));
                       }))
                       .withEventListener ("sessionSetup", afterEvent (this, [this] (const juce::var&) { sendSessionSetup(); }))
                       .withEventListener ("sessionDetect", afterEvent (this, [this] (const juce::var& event)
                       {
                           detectSessionChords (static_cast<int> (event.getProperty ("trackId", 0)),
                                                static_cast<double> (event.getProperty ("tempo", 0.0)));
                       }))
                       .withEventListener ("sessionCancelDetect", afterEvent (this, [this] (const juce::var&) { cancelRecordingAnalysis(); }))
                       .withEventListener ("addSessionPlayers", afterEvent (this, [this] (const juce::var& event) { addSessionPlayers (event); }))
                       .withEventListener ("recordQuantize", afterEvent (this, [this] (const juce::var& event)
                       {
                           // Record Quantize, as the Quantize panel has it.
                           AudioEngine::NoteQuantize q;
                           q.on = static_cast<bool> (event.getProperty ("on", false));
                           q.grid = juce::jlimit (1.0 / 64.0, 16.0, static_cast<double> (event.getProperty ("grid", 0.25)));
                           q.strength = juce::jlimit (0.0, 1.0, static_cast<double> (event.getProperty ("strength", 1.0)));
                           q.swing = juce::jlimit (0.0, 1.0, static_cast<double> (event.getProperty ("swing", 0.0)));
                           q.starts = static_cast<bool> (event.getProperty ("starts", true));
                           q.ends = static_cast<bool> (event.getProperty ("ends", false));

                           if (! std::isfinite (q.grid) || ! std::isfinite (q.strength) || ! std::isfinite (q.swing))
                               return;

                           engine.setRecordQuantize (q);
                       }))
                       .withEventListener ("stemsState", afterEvent (this, [this] (const juce::var&) { emitStems (stemStateName(), stemProgress()); }))
                       .withEventListener ("splitStems", afterEvent (this, [this] (const juce::var& event)
                       {
                           juce::StringArray stems;

                           if (const auto* list = event.getProperty ("stems", {}).getArray())
                               for (const auto& name : *list)
                                   stems.addIfNotAlreadyThere (name.toString());

                           const auto busy = stemStage != StemStage::idle;

                           if (const auto error = splitStems (static_cast<int> (event.getProperty ("trackId", 0)), static_cast<int> (event.getProperty ("clipId", 0)),
                                                              stems, static_cast<bool> (event.getProperty ("muteOriginal", true))); error.isNotEmpty())
                           {
                               if (busy)
                                   emitStems (stemStateName(), stemProgress());   // one under way already (the AI's): the page shows that one
                               else
                                   emitStems ("failed", 0.0, error);
                           }
                       }))
                       .withEventListener ("cancelStems", afterEvent (this, [this] (const juce::var&) { cancelStems(); }))
                       .withEventListener ("addBusChannel", afterEvent (this, [this] (const juce::var& event) { handleTrackGroup ("addBus", event); }))
                       .withEventListener ("tracksSettings", afterEvent (this, [this] (const juce::var& event) { handleTrackGroup ("settings", event); }))
                       .withEventListener ("removeTracks", afterEvent (this, [this] (const juce::var& event) { handleTrackGroup ("remove", event); }))
                       .withEventListener ("duplicateTracks", afterEvent (this, [this] (const juce::var& event) { handleTrackGroup ("duplicate", event); }))
                       .withEventListener ("trackMenu", afterEvent (this, [this] (const juce::var& event) { handleTrackMenu (event); }))
                       .withEventListener ("score", afterEvent (this, [this] (const juce::var& event) { handleScore (event); }))
                       .withEventListener ("importMedia", afterEvent (this, [this] (const juce::var& event) { handleImport (event); }))
                       .withEventListener ("moveTracks", afterEvent (this, [this] (const juce::var& event) { handleTrackGroup ("move", event); }))
                       .withEventListener ("renameTracks", afterEvent (this, [this] (const juce::var& event) { handleTrackGroup ("rename", event); }))
                       .withEventListener ("midiLinkState", afterEvent (this, [this] (const juce::var&) { pushMidiLink(); }))
                       .withEventListener ("midiLinkSet", afterEvent (this, [this] (const juce::var& event) { handleMidiLink ("set", event); }))
                       .withEventListener ("midiLinkMode", afterEvent (this, [this] (const juce::var& event) { handleMidiLink ("mode", event); }))
                       .withEventListener ("midiLinkClear", afterEvent (this, [this] (const juce::var& event) { handleMidiLink ("clear", event); }))
                       .withEventListener ("midiLinkCancel", afterEvent (this, [this] (const juce::var& event) { handleMidiLink ("cancel", event); }))
                       .withEventListener ("midiLinkLearn", afterEvent (this, [this] (const juce::var& event) { handleMidiLink ("learn", event); }))
                       .withEventListener ("midiLinkMenu", afterEvent (this, [this] (const juce::var& event) { handleMidiLink ("menu", event); }))
                       .withEventListener ("cancelMixdown", afterEvent (this, [this] (const juce::var&) { engine.cancelMixdown(); }))
                       .withEventListener ("chooseMixdownFolder", afterEvent (this, [this] (const juce::var& event) { chooseMixdownFolder (event.getProperty ("folder", {}).toString()); }))
                       .withEventListener ("setAutomation", afterEvent (this, [this] (const juce::var& event)
                       {
                           const auto trackId = static_cast<int> (event.getProperty ("trackId", -1));
                           const auto* lanes = engine.getAutomation (trackId);

                           if (lanes == nullptr)
                               return;

                           auto before = *lanes;
                           AutomationLane lane;
                           const auto target = event.getProperty ("target", {});
                           const auto kind = target.getProperty ("kind", {}).toString();
                           lane.target.kind = kind == "pan" ? AutomationTarget::Kind::pan
                                            : kind == "param" ? AutomationTarget::Kind::parameter : AutomationTarget::Kind::volume;
                           lane.target.slot = static_cast<int> (target.getProperty ("slot", 0));
                           lane.target.paramId = static_cast<uint32_t> (static_cast<juce::int64> (target.getProperty ("param", 0)));

                           if (const auto* points = event.getProperty ("points", {}).getArray())
                               for (const auto& point : *points)
                                   if (const auto* pair = point.getArray(); pair != nullptr && pair->size() == 2)
                                       lane.points.push_back ({ static_cast<double> ((*pair)[0]), static_cast<double> ((*pair)[1]) });

                           // Its shape regions (none sent: they stay as they are).
                           if (const auto shapes = event.getProperty ("shapes", {}); shapes.isArray())
                               lane.shapes = juce::JSON::parse (juce::JSON::toString (shapes, true));

                           engine.setAutomationLane (trackId, std::move (lane));
                           addToHistory ("Edit Automation", new Edits::AutomationChanged (engine, trackId, std::move (before), *engine.getAutomation (trackId)));
                           markChanged();
                           pushTracks();
                       }))
                       .withEventListener ("setAutomationMode", afterEvent (this, [this] (const juce::var& event)
                       {
                           const auto trackId = static_cast<int> (event.getProperty ("trackId", -1));
                           const auto mode = Automation::modeFromName (event.getProperty ("mode", {}).toString());

                           // Out of a recording mode: what it was recording is kept first.
                           if (! Automation::records (static_cast<int> (mode)))
                               finishAutomationWrites (trackId);

                           engine.setAutomationMode (trackId, mode);
                           markChanged();
                           pushTracks();
                       }))
                       .withEventListener ("audioOp", afterEvent (this, [this] (const juce::var& event) { handleAudioOp (event); }))
                       .withEventListener ("songBlocks", afterEvent (this, [this] (const juce::var& event) { handleSongBlocks (event); }))
                       .withEventListener ("songVideo", afterEvent (this, [this] (const juce::var& event) { handleSongVideo (event); }))
                       .withEventListener ("midiFxAdd", afterEvent (this, [this] (const juce::var& event) { handleMidiFx ("add", event); }))
                       .withEventListener ("midiFxRemove", afterEvent (this, [this] (const juce::var& event) { handleMidiFx ("remove", event); }))
                       .withEventListener ("midiFxMove", afterEvent (this, [this] (const juce::var& event) { handleMidiFx ("move", event); }))
                       .withEventListener ("midiFxOn", afterEvent (this, [this] (const juce::var& event) { handleMidiFx ("on", event); }))
                       .withEventListener ("midiFxParam", afterEvent (this, [this] (const juce::var& event) { handleMidiFx ("param", event); }))
                       .withEventListener ("midiToolRun", afterEvent (this, [this] (const juce::var& event) { runMidiTool (event); }))
                       .withEventListener ("midiToolTrack", afterEvent (this, [this] (const juce::var& event) { addMidiToolTrack (event); }))
                       .withEventListener ("magentaAnswer", afterEvent (this, [this] (const juce::var& event) { answerMagentaWindow (event); }))
                       .withEventListener ("midiToolsList", afterEvent (this, [this] (const juce::var&) { listMidiTools(); }))
                       .withEventListener ("midiToolInstall", afterEvent (this, [this] (const juce::var& event) { installMidiTool (event.getProperty ("tool", {})); }))
                       .withEventListener ("midiToolRemove", afterEvent (this, [this] (const juce::var& event) { removeMidiTool (event.getProperty ("id", {}).toString()); }))
                       .withEventListener ("audioCancel", afterEvent (this, [this] (const juce::var&) { cancelAudioJob (false); }))
                       .withEventListener ("setSongChannel", afterEvent (this, [this] (const juce::var& event) { setSongChannel (event); }))
                       .withEventListener ("setMainAutomation", afterEvent (this, [this] (const juce::var& event) { setMainAutomationFrom (event); }))
                       .withEventListener ("songChannelState", afterEvent (this, [this] (const juce::var&) { pushSongChannel (true); }))
                       .withEventListener ("openPluginScreen", afterEvent (this, [this] (const juce::var& event)
                       {
                           openPluginScreen (static_cast<int> (event.getProperty ("channel", -1)), static_cast<int> (event.getProperty ("slot", -1)));
                       }))
                       .withEventListener ("closePluginScreen", afterEvent (this, [this] (const juce::var& event)
                       {
                           const std::pair<int, int> key { static_cast<int> (event.getProperty ("channel", -1)), static_cast<int> (event.getProperty ("slot", -1)) };

                           if (auto* plugin = engine.getPlugin (key.first, key.second))
                               plugin->setScreenOpen (false);

                           openScreens.erase (key);
                       }))
                       .withEventListener ("screenMessage", afterEvent (this, [this] (const juce::var& event)
                       {
                           juce::MemoryBlock data;

                           if (auto* plugin = engine.getPlugin (static_cast<int> (event.getProperty ("channel", -1)), static_cast<int> (event.getProperty ("slot", -1))))
                               if (data.fromBase64Encoding (event.getProperty ("data", {}).toString()))
                                   plugin->sendToPlugin (data.getData(), data.getSize());
                       }))
                       .withEventListener ("audioFileInfo", afterEvent (this, [this] (const juce::var& event) { sendAudioFileInfo (event.getProperty ("path", {}).toString()); }))
                       .withEventListener ("previewAudio", afterEvent (this, [this] (const juce::var& event)
                       {
                           // Read on its own thread: a long file doesn't hold the app up. It plays as
                           // it's read (unless another was asked for, or the preview stopped, meanwhile).
                           engine.previewAudioWhenRead (juce::File (event.getProperty ("path", {}).toString()),
                                                        [safe = juce::Component::SafePointer<MainComponent> (this)] (bool)
                                                        {
                                                            if (safe != nullptr)
                                                                safe->pushStatus();
                                                        });
                           pushStatus();
                       }))
                       .withEventListener ("stopPreview", afterEvent (this, [this] (const juce::var&) { engine.stopPreview(); pushStatus(); }))
                       .withEventListener ("importAudio", afterEvent (this, [this] (const juce::var& event)
                       {
                           importAudioFile (juce::File (event.getProperty ("file", {}).toString()),
                                            static_cast<int> (event.getProperty ("trackId", 0)),
                                            static_cast<double> (event.getProperty ("start", 0.0)), false);
                           pushTracks();
                       }))
                       .withEventListener ("importAudioFiles", afterEvent (this, [this] (const juce::var&) { chooseAudioToImport(); }))
                       .withEventListener ("audioPeaksFine", afterEvent (this, [this] (const juce::var& event) { pushFinePeaks (event.getProperty ("file", {}).toString()); }))
                       .withEventListener ("songSetup", afterEvent (this, [this] (const juce::var& event)
                       {
                           if (event.getProperty ("inputs", {}).isArray())
                               engine.setInputs (Song::inputsFromVar (event.getProperty ("inputs", {})));

                           // Outputs: the page has no faders of its own for them, so a Sub's stay as they are.
                           if (event.getProperty ("outputs", {}).isArray())
                           {
                               auto outputs = Song::outputsFromVar (event.getProperty ("outputs", {}));

                               for (auto& output : outputs)
                                   for (const auto& current : engine.getOutputs())
                                       if (current.id == output.id && output.id > 0)
                                       {
                                           output.volumeDb = current.volumeDb;
                                           output.mute = current.mute;
                                       }

                               // New outputs come with ids below 0; the Audition may name one of them.
                               engine.setOutputs (std::move (outputs), static_cast<int> (event.getProperty ("audition", 0)));
                           }

                           engine.setRecordBits (static_cast<int> (event.getProperty ("recordBits", 24)));
                           markChanged();
                           pushTracks();
                       }))
                       .withEventListener ("songSetupDefault", afterEvent (this, [this] (const juce::var& event)
                       {
                           // Make Default: new songs start with these inputs and outputs.
                           setSongDefaults (event);
                           pushTracks();
                       }))
                       .withEventListener ("outputSettings", afterEvent (this, [this] (const juce::var& event)
                       {
                           // A Sub output's fader or mute (the Console). Fader moves aren't echoed back.
                           const auto id = static_cast<int> (event.getProperty ("id", 0));
                           markChanged();

                           if (event.hasProperty ("volume"))
                               engine.setOutputVolume (id, static_cast<float> (static_cast<double> (event.getProperty ("volume", 0.0))));

                           if (event.hasProperty ("mute"))
                           {
                               engine.setOutputMute (id, static_cast<bool> (event.getProperty ("mute", false)));
                               pushTracks();
                           }
                       }))
                       .withEventListener ("undo", afterEvent (this, [this] (const juce::var&) { undoOrRedo (true); }))
                       .withEventListener ("redo", afterEvent (this, [this] (const juce::var&) { undoOrRedo (false); }))
                       .withEventListener ("undoToggle", afterEvent (this, [this] (const juce::var&) { undoToggle(); }))
                       .withEventListener ("newSong", afterEvent (this, [this] (const juce::var& event) { whenSongIsSafe ([this, event] { newSong (event); }); }))
                       .withEventListener ("openSong", afterEvent (this, [this] (const juce::var& event)
                       {
                           const auto path = event.getProperty ("path", {}).toString();

                           whenSongIsSafe ([this, path]
                           {
                               if (path.isNotEmpty())
                                   openSong (juce::File (path));
                               else
                                   chooseSongToOpen();
                           });
                       }))
                       .withEventListener ("saveSong", afterEvent (this, [this] (const juce::var&) { saveSong ({}); }))
                       .withEventListener ("saveSongAs", afterEvent (this, [this] (const juce::var&) { chooseSongLocation ({}); }))
                       .withEventListener ("dawProject", afterEvent (this, [this] (const juce::var& event) { handleDawProject (event); }))
                       .withEventListener ("songView", afterEvent (this, [this] (const juce::var& event) { songView = event.getProperty ("view", {}); }))
                       .withEventListener ("chooseSongFolder", afterEvent (this, [this] (const juce::var& event)
                       {
                           juce::File start (event.getProperty ("folder", {}).toString());

                           if (! start.isDirectory())
                           {
                               start = Song::getDefaultFolder();
                               start.createDirectory();
                           }

                           songChooser = std::make_unique<juce::FileChooser> ("Where new songs go", start);
                           songChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                                     [safeThis = juce::Component::SafePointer<MainComponent> (this)] (const juce::FileChooser& chooser)
                           {
                               if (safeThis == nullptr || safeThis->webView == nullptr || chooser.getResult() == juce::File())
                                   return;

                               juce::DynamicObject::Ptr reply (new juce::DynamicObject());
                               reply->setProperty ("path", chooser.getResult().getFullPathName());
                               safeThis->webView->emitEventIfBrowserIsVisible ("songFolder", juce::var (reply.get()));
                           });
                       }))
                       .withEventListener ("addLibraryFiles", afterEvent (this, [this] (const juce::var&) { chooseLibraryFiles(); }))
                       .withEventListener ("addLibraryFolder", afterEvent (this, [this] (const juce::var&) { chooseLibraryFolder(); }))
                       .withEventListener ("removeLibraryFolder", afterEvent (this, [this] (const juce::var& event)
                       {
                           library.removeFolder (event.getProperty ("folder", {}).toString());
                           pushLibrary();
                       }))
                       .withEventListener ("choosePlugins", afterEvent (this, [this] (const juce::var& event)
                       {
                           choosePlugins (static_cast<bool> (event.getProperty ("thirdParty", true)));
                       }))
                       .withEventListener ("cancelScan", afterEvent (this, [this] (const juce::var&)
                       {
                           if (scanner != nullptr)
                               scanner->cancel();
                       }))
                       .withEventListener ("rescanLibrary", afterEvent (this, [this] (const juce::var& event)
                       {
                           startLibraryScan (static_cast<bool> (event.getProperty ("full", true)));
                       }))
                       .withEventListener ("setFavorite", afterEvent (this, [this] (const juce::var& event)
                       {
                           library.setFavorite (event.getProperty ("file", {}).toString(), event.getProperty ("id", {}).toString(),
                                                static_cast<bool> (event.getProperty ("favorite", false)));
                           pushLibrary();
                       }))
                       .withEventListener ("revealLibraryFile", afterEvent (this, [] (const juce::var& event)
                       {
                           juce::File (event.getProperty ("file", {}).toString()).revealToUser();
                       }))
                       .withEventListener ("removeLibraryFile", afterEvent (this, [this] (const juce::var& event)
                       {
                           library.removeFile (event.getProperty ("file", {}).toString());
                           pushLibrary();
                       }))
                       // The Plug-In Manager (hub-plugins.js).
                       .withEventListener ("pluginManager", afterEvent (this, [this] (const juce::var& event)
                       {
                           const auto action = event.getProperty ("action", {}).toString();
                           juce::String notice;

                           const auto pathsOf = [&event]
                           {
                               juce::StringArray paths;

                               if (const auto* list = event.getProperty ("files", {}).getArray())
                                   for (const auto& item : *list)
                                       paths.addIfNotAlreadyThere (item.toString());

                               paths.removeEmptyStrings();
                               return paths;
                           };

                           if (action == "show" || action == "hide")
                           {
                               std::vector<std::pair<juce::String, juce::String>> plugins;

                               if (const auto* list = event.getProperty ("plugins", {}).getArray())
                                   for (const auto& item : *list)
                                       plugins.emplace_back (item.getProperty ("file", {}).toString(), item.getProperty ("id", {}).toString());

                               library.setHidden (plugins, action == "hide");
                           }
                           else if (action == "hideDuplicates")
                           {
                               const auto hidden = library.hideDuplicates();
                               notice = hidden == 0 ? juce::String ("No duplicates to hide.")
                                                    : "Hid " + juce::String (hidden) + (hidden == 1 ? " duplicate." : " duplicates.");
                           }
                           else if (action == "priority")
                           {
                               juce::StringArray formats;

                               if (const auto* list = event.getProperty ("formats", {}).getArray())
                                   for (const auto& item : *list)
                                       formats.add (item.toString());

                               library.setFormatPriority (formats);
                           }
                           else if (action == "autoHide")
                           {
                               library.setHideDuplicatesAfterScans (static_cast<bool> (event.getProperty ("on", false)));

                               if (library.hidesDuplicatesAfterScans())
                                   library.hideDuplicates();
                           }
                           else if (action == "remove")
                           {
                               for (const auto& path : pathsOf())
                                   library.removeFile (path);
                           }
                           else if (action == "unblock" || action == "resetBlocklist")
                           {
                               if (action == "unblock")
                                   library.unblock (pathsOf());
                               else
                                   library.resetBlocklist();

                               startLibraryScan (false);   // read again now
                               return;
                           }

                           pushLibrary (notice);
                       }))
                       .withEventListener ("exportClip", afterEvent (this, [this] (const juce::var& event)
                       {
                           exportClip (static_cast<int> (event.getProperty ("trackId", 0)), static_cast<int> (event.getProperty ("clipId", 0)),
                                       static_cast<bool> (event.getProperty ("choose", false)));
                       }))
                       .withEventListener ("mergeClips", afterEvent (this, [this] (const juce::var& event) { mergeAudioClips (event); }))
                       .withEventListener ("bounceClips", afterEvent (this, [this] (const juce::var& event) { bounceClipsToNewTrack (event); }))
                       .withEventListener ("transients", afterEvent (this, [this] (const juce::var& event) { sendTransients (event); }))
                       // Silence Areas' window: the stretches that would go silent with these settings (not applied yet).
                       .withEventListener ("silencePreview", afterEvent (this, [this] (const juce::var& event)
                       {
                           const auto trackId = static_cast<int> (event.getProperty ("trackId", 0));
                           const auto clipId = static_cast<int> (event.getProperty ("clipId", 0));
                           const auto* clip = findAudioClip (trackId, clipId);

                           if (clip == nullptr || webView == nullptr)
                               return;

                           auto silence = Song::silenceFromVar (event.getProperty ("silence", {}));
                           double silenced = 0.0;
                           const auto areas = engine.silenceSeconds (clip->audioFile, silence, clip->audioOffset,
                                                                     clip->audioOffset + clip->audioLength, silenced);

                           juce::Array<juce::var> list;

                           for (const auto& [from, to] : areas)
                               list.add (juce::Array<juce::var> { std::round (from * 1000.0) / 1000.0, std::round (to * 1000.0) / 1000.0 });

                           juce::DynamicObject::Ptr reply (new juce::DynamicObject());
                           reply->setProperty ("trackId", trackId);
                           reply->setProperty ("clipId", clipId);
                           reply->setProperty ("key", event.getProperty ("key", {}));
                           reply->setProperty ("areas", list);
                           reply->setProperty ("silenced", silenced);
                           reply->setProperty ("length", clip->audioLength);
                           webView->emitEventIfBrowserIsVisible ("silenceAreas", juce::var (reply.get()));
                       }))
                       // The first source in a track's MIDI in list (keyboards first): a new instrument track hears it.
                       .withEventListener ("midiDefaultSource", afterEvent (this, [this] (const juce::var& event)
                       {
                           engine.setPreferredMidiSource (event.getProperty ("source", {}).toString());
                       }));

    // File > Settings: its events are in MainSettings.cpp.
    options = withSettingsEvents (std::move (options));

    // The Plugins Creator Studio: its events are in MainCreator.cpp.
    options = withCreatorEvents (std::move (options));

    // The Sampling Studio: its events are in MainSampling.cpp.
    options = withSamplingEvents (std::move (options));

   #if JUCE_WINDOWS
    // WebView2 keeps its profile next to the executable, inside the build folder: the one profile
    // every web view in Sonify shares (SettingsFolder::webViewProfile).
    const auto userDataFolder = SettingsFolder::webViewProfile();

    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2 {}
                                                  .withUserDataFolder (userDataFolder)
                                                  .withBackgroundColour (backgroundColour)
                                                  .withStatusBarDisabled());
   #endif

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> MainComponent::getResource (const juce::String& url)
{
    // Every file of the page is compiled into the executable under its own name.
    auto path = url.upToFirstOccurrenceOf ("?", false, false).fromFirstOccurrenceOf ("/", false, false);

    // A plugin's screen asks for its own files: plugin-screen/<channel>/<slot>/<its path>.
    if (path.startsWith ("plugin-screen/"))
        return getScreenResource (path.fromFirstOccurrenceOf ("plugin-screen/", false, false));

    // magenta.js and the MIDI tools' models (MainMidiTools.cpp).
    if (path.startsWith ("magenta/") || path.startsWith ("midi-tools/"))
        return getMidiToolResource (juce::URL::removeEscapeChars (path));

    // The song's video's small pictures, for the Video lane (MainVideo.cpp).
    if (path == "song-video-thumbnails.jpg")
        return getVideoResource();

    // A made plugin's picture, for the Browser: plugin-snapshot/<its folder's name>.
    if (path.startsWith ("plugin-snapshot/"))
    {
        const auto name = juce::URL::removeEscapeChars (path.fromFirstOccurrenceOf ("plugin-snapshot/", false, false));

        if (name.isEmpty() || name.containsAnyOf ("/\\:") || name.contains ("..") || ! MadePlugins::isMadeFile (name))
            return std::nullopt;

        juce::MemoryBlock bytes;

        if (! MadePlugins::snapshotOf (MadePlugins::folder().getChildFile (name)).loadFileAsData (bytes))
            return std::nullopt;

        std::vector<std::byte> data (bytes.getSize());
        std::memcpy (data.data(), bytes.getData(), bytes.getSize());
        return juce::WebBrowserComponent::Resource { std::move (data), "image/png" };
    }

    if (path.isEmpty())
        path = "index.html";

    // The page itself is asked for, so WebView2's window is there by now: the splash stays over it.
    if (path == "index.html" && splash != nullptr)
        placeSplash();

    // A name with a space arrives escaped ("fanan%20logo.png").
    path = juce::URL::removeEscapeChars (path);

    int size = 0;

    if (const auto* data = pageFile (path, size))
    {
        auto bytes = toBytes (data, size);

       #if JUCE_LINUX || JUCE_BSD
        // WebKitGTK reads the page's scripts and styles as Latin-1 (they don't say they're UTF-8),
        // which turns their symbols (arrows, stars, dashes) into rubbish: a UTF-8 byte order mark in
        // front tells it, before anything else is read.
        const auto mime = mimeTypeFor (path);
        const auto* first = reinterpret_cast<const unsigned char*> (data);

        if (mime.startsWith ("text/") && ! (size >= 3 && first[0] == 0xef && first[1] == 0xbb && first[2] == 0xbf))
            bytes.insert (bytes.begin(), { std::byte { 0xef }, std::byte { 0xbb }, std::byte { 0xbf } });
       #endif

        return juce::WebBrowserComponent::Resource { std::move (bytes), mimeTypeFor (path) };
    }

    return std::nullopt;
}

void MainComponent::placeSplash()
{
    // Just after this message: the window may be in the middle of being set up.
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainComponent> (this)]
    {
        if (safe != nullptr && safe->splash != nullptr)
            safe->splash->cover();
    });
}

void MainComponent::dismissSplash (int milliseconds)
{
    juce::Timer::callAfterDelay (milliseconds, [safe = juce::Component::SafePointer<MainComponent> (this)]
    {
        if (safe != nullptr)
            safe->splash = nullptr;   // its window goes, and the page is simply there
    });
}

//==============================================================================
// Plugin screens
std::optional<juce::WebBrowserComponent::Resource> MainComponent::getScreenResource (const juce::String& rest)
{
    if (internetBlocked)
        return {};   // a plugin's web screen is a web page: blocked with everything else

    const auto channel = rest.upToFirstOccurrenceOf ("/", false, false).getIntValue();
    const auto afterChannel = rest.fromFirstOccurrenceOf ("/", false, false);
    const auto slot = afterChannel.upToFirstOccurrenceOf ("/", false, false).getIntValue();

    // The plugin sees its own absolute path, with the Hub's prefix taken off (as CLAP asks).
    const auto path = "/" + juce::URL::removeEscapeChars (afterChannel.fromFirstOccurrenceOf ("/", false, false));
    auto* plugin = engine.getPlugin (channel, slot);

    if (plugin == nullptr || plugin->getScreenKind() != Plugin::ScreenKind::web)
        return std::nullopt;

    juce::String mime;
    juce::MemoryBlock data;

    if (! plugin->getScreenResource (path, mime, data))
        return std::nullopt;

    return juce::WebBrowserComponent::Resource { toBytes (static_cast<const char*> (data.getData()), static_cast<int> (data.getSize())),
                                                 mime.isNotEmpty() ? mime : mimeTypeFor (path) };
}

/** Web tabs live in a native child window of their own, laid over the Hub's page.

    JUCE makes every WebView2 a child of the main window, with no stacking order between them,
    so a tab's page made after the Hub's own full-window page can end up hidden behind it (a
    black content area). This window is raised above the Hub's page whenever a tab shows. */
class MainComponent::WebTabLayer final : public juce::Component
{
public:
    WebTabLayer()
    {
        setOpaque (true);
        setWantsKeyboardFocus (false);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1b1c20));   // until the page has drawn
    }

    void resized() override
    {
        for (auto* child : getChildren())
            child->setBounds (getLocalBounds());
    }
};

void MainComponent::loadInternetBlocked()
{
    internetBlocked = static_cast<bool> (juce::JSON::parse (internetBlockedFile()).getProperty ("blocked", false));
}

void MainComponent::pushInternetBlocked()
{
    if (webView == nullptr)
        return;

    juce::DynamicObject::Ptr item (new juce::DynamicObject());
    item->setProperty ("blocked", internetBlocked);
    webView->emitEventIfBrowserIsVisible ("internetBlocked", juce::var (item.get()));
}

juce::String MainComponent::songSnapshot()
{
    // With the ids of its tracks, inserts and clips: undoing to it keeps the older steps working.
    return juce::JSON::toString (Song::capture (engine, songTitle(), songView, songFile, true));
}

void MainComponent::restoreSnapshot (const juce::String& song)
{
    juce::StringArray problems;
    Song::restore (engine, juce::JSON::parse (song), problems, songFile);

    if (aiTools != nullptr)
        aiTools->forgetWhatItMade();   // undo and redo: the song comes back as it was

    pushTracks();
    pushSong();
}

void MainComponent::addAiAccount (juce::var& state) const
{
    // What an AI panel needs from the account: whether it can work now, and the credits.
    auto* fields = state.getDynamicObject();

    if (fields == nullptr)
        return;

    fields->setProperty ("configured", server->isConfigured());
    fields->setProperty ("blocked", internetBlocked);
    fields->setProperty ("status", statusName (server->getStatus()));
    fields->setProperty ("signedIn", server->isSignedIn());
    fields->setProperty ("credits", server->getCredits());
    fields->setProperty ("siteAvailable", server->hasSite());
}

void MainComponent::pushAi()
{
    if (webView == nullptr || ai == nullptr)
        return;

    auto state = ai->getState();
    addAiAccount (state);
    webView->emitEventIfBrowserIsVisible ("aiState", state);

    // The Plugins Creator Studio's AI panel and the Guide's follow the account too.
    pushDeviceAi();
    pushGuide();
}

//==============================================================================
// Voice control
void MainComponent::pushVoice()
{
    if (webView == nullptr || voice == nullptr)
        return;

    juce::Array<juce::var> inputs;

    for (const auto& input : engine.getInputs())
        inputs.add (input.name);

    const auto stage = voice->getStage();

    juce::DynamicObject::Ptr item (new juce::DynamicObject());
    item->setProperty ("offered", voiceControlOffered);
    item->setProperty ("enabled", voice->isEnabled());
    item->setProperty ("why", voice->isEnabled() ? voice->whyNot() : juce::String());
    item->setProperty ("stage", stage == VoiceControl::Stage::command   ? "command"
                              : stage == VoiceControl::Stage::listening ? "listening"
                                                                        : "off");
    item->setProperty ("heard", voice->getHeard());
    item->setProperty ("message", voice->getMessage());
    item->setProperty ("input", engine.getVoiceInput());
    item->setProperty ("inputs", inputs);
    webView->emitEventIfBrowserIsVisible ("voiceState", juce::var (item.get()));
}

//==============================================================================
// Sonify Remote
void MainComponent::pushRemote()
{
    if (webView == nullptr || phoneRemote == nullptr)
        return;

    juce::DynamicObject::Ptr item (new juce::DynamicObject());
    item->setProperty ("offered", phoneRemoteOffered);
    item->setProperty ("enabled", phoneRemote->isEnabled());
    item->setProperty ("why", phoneRemote->isEnabled() ? phoneRemote->whyNot() : juce::String());
    item->setProperty ("connected", phoneRemote->isConnected());
    item->setProperty ("problem", phoneRemote->getProblem());
    item->setProperty ("url", server->getRemotePageUrl());
    item->setProperty ("email", server->getEmail());
    item->setProperty ("localUrl", phoneRemote->getLocalUrl());
    item->setProperty ("localProblem", phoneRemote->isEnabled() ? phoneRemote->getLocalProblem() : juce::String());
    item->setProperty ("localSeen", phoneRemote->isLocalPhoneSeen());
    webView->emitEventIfBrowserIsVisible ("remoteState", juce::var (item.get()));
}

void MainComponent::askFromPhone (const juce::String& text, bool spoken)
{
    // One request at a time: the phone waits while Sonify AI works, but someone at the computer may not.
    if (ai->isBusy())
    {
        ai->addNotice ("Fanan AI was still working, so this from the phone wasn't sent: \"" + text + "\"");
        return;
    }

    ai->send (text, spoken ? AiAgent::Origin::phoneVoice : AiAgent::Origin::typed);
}

void MainComponent::loadVoiceInput()
{
    voiceInputChoice = juce::JSON::parse (voiceInputFile());
}

void MainComponent::chooseVoiceInput (int index)
{
    // Kept by name and channels: the next song's inputs are its own.
    const auto& inputs = engine.getInputs();
    juce::DynamicObject::Ptr choice (new juce::DynamicObject());

    if (juce::isPositiveAndBelow (index, static_cast<int> (inputs.size())))
    {
        const auto& input = inputs[static_cast<size_t> (index)];
        choice->setProperty ("name", input.name);
        choice->setProperty ("left", input.left);
        choice->setProperty ("right", input.right);
    }
    else
    {
        index = -1;
    }

    voiceInputChoice = juce::var (choice.get());
    voiceInputFile().replaceWithText (juce::JSON::toString (voiceInputChoice));

    engine.setVoiceInput (index);
    voice->inputChanged();
}

void MainComponent::followVoiceInput()
{
    if (voice == nullptr)
        return;

    // The mic's input is found again in the song's inputs: by its name, or by its channels.
    const auto& inputs = engine.getInputs();
    const auto name = voiceInputChoice.getProperty ("name", {}).toString();
    const auto left = static_cast<int> (voiceInputChoice.getProperty ("left", -1));
    const auto right = static_cast<int> (voiceInputChoice.getProperty ("right", -1));
    int index = -1;

    if (name.isNotEmpty())
    {
        for (size_t i = 0; i < inputs.size() && index < 0; ++i)
            if (inputs[i].name == name)
                index = static_cast<int> (i);

        for (size_t i = 0; i < inputs.size() && index < 0; ++i)
            if (inputs[i].left == left && inputs[i].right == right)
                index = static_cast<int> (i);
    }

    juce::String shown;

    for (const auto& input : inputs)
        shown << input.name << "\n";

    shown << index;

    if (index != engine.getVoiceInput())
    {
        engine.setVoiceInput (index);
        voice->inputChanged();
    }

    if (shown != voiceInputsShown)
    {
        voiceInputsShown = shown;
        pushVoice();
    }
}

void MainComponent::followTransport()
{
    // Where playing (or recording) last began, for voice control's "again". A voice command
    // knows it exactly; any other start is seen here, a moment late.
    const auto now = engine.getTransport().getState();
    const auto recordingNow = now.playing && now.recording;

    if (! now.playing)
        stoppedAt = now.ppq;
    else if ((! wasPlaying || (recordingNow && ! wasRecording)) && ! startedByVoice)
    {
        againFrom = wasPlaying ? now.ppq : stoppedAt;   // punched in, or started
        againRecords = recordingNow;
    }

    // A voice command's own start, seen now or missed between two looks: either way it's past.
    if (now.playing)
        startedByVoice = false;

    wasPlaying = now.playing;
    wasRecording = recordingNow;
}

void MainComponent::forgetWhereItBegan()
{
    againFrom = -1.0;
    againRecords = false;
    startedByVoice = false;
}

void MainComponent::voiceProblem (const juce::String& message)
{
    engine.playCue (AudioEngine::Cue::problem);
    voice->setMessage (message);
}

void MainComponent::runVoiceCommand (const juce::String& command)
{
    using Quick = VoiceControl::Quick;
    const auto quick = VoiceControl::quickCommand (command);

    if (quick == Quick::none)
    {
        // Anything else is for Sonify AI, as it was said.
        if (ai->isBusy())
            return voiceProblem ("Fanan AI is still working on the last request.");

        ai->send (command, AiAgent::Origin::voice);

        if (! ai->isBusy())
            return voiceProblem ("Fanan AI couldn't take that: the AI panel says why.");

        voiceRequest = true;
        engine.playCue (AudioEngine::Cue::heard);
        return;
    }

    // The transport, at once.
    if (engine.isMixdownRunning())
        return voiceProblem ("Not while a mixdown is being made.");

    auto& transport = engine.getTransport();
    const auto state = transport.getState();

    switch (quick)
    {
        case Quick::record:
            if (! engine.isRecording())
                return voiceRecord (0);   // it beeps first
            break;

        case Quick::play:
            if (! state.playing)
            {
                againFrom = state.ppq;
                againRecords = false;
                startedByVoice = true;
                transport.setPlaying (true);
            }
            break;

        case Quick::stop:
            transport.setPlaying (false);   // and recording stops
            break;

        case Quick::undo:
            if (ai->isBusy())
                return voiceProblem ("Not while Fanan AI is working.");

            if (engine.isRecording() || engine.isTakeActive())
                return voiceProblem ("Not while recording.");

            if (! undoManager.canUndo())
                return voiceProblem ("There's nothing to undo.");

            undoOrRedo (true);
            break;

        case Quick::again:
        case Quick::recordAgain:
        case Quick::playAgain:
            voiceAgain (quick, 0);
            return;

        case Quick::none:
            break;
    }

    engine.playCue (AudioEngine::Cue::done);
}

void MainComponent::voiceAgain (VoiceControl::Quick quick, int attempt)
{
    auto& transport = engine.getTransport();

    // It stops first, and a take is filed into its clips (its own undo step) before the next begins.
    if (attempt == 0)
        transport.setPlaying (false);

    if (transport.getState().playing || engine.isTakeActive() || takeRunning)
    {
        if (attempt >= 60)   // three seconds
            return voiceProblem ("Sonify couldn't stop to go again.");

        juce::Timer::callAfterDelay (50, [safeThis = juce::Component::SafePointer<MainComponent> (this), quick, attempt]
        {
            if (safeThis != nullptr)
                safeThis->voiceAgain (quick, attempt + 1);
        });

        return;
    }

    if (engine.isMixdownRunning())
        return voiceProblem ("Not while a mixdown is being made.");

    // From where it last began (or from here, when nothing has played yet), as it did then.
    const auto record = quick == VoiceControl::Quick::recordAgain || (quick == VoiceControl::Quick::again && againRecords);

    if (againFrom < 0.0)
        againFrom = transport.getState().ppq;

    transport.locate (againFrom);

    if (record)
        return voiceRecord (0);

    againRecords = false;
    startedByVoice = true;
    transport.setPlaying (true);
    engine.playCue (AudioEngine::Cue::done);
}

void MainComponent::voiceRecord (int attempt)
{
    // The beep first, then the take: what the headphones leak into the mic stays out of it.
    if (attempt == 0)
    {
        engine.playCue (AudioEngine::Cue::done);

        juce::Timer::callAfterDelay (400, [safeThis = juce::Component::SafePointer<MainComponent> (this)]
        {
            if (safeThis != nullptr)
                safeThis->voiceRecord (1);
        });

        return;
    }

    if (engine.isMixdownRunning() || engine.isRecording())
        return;

    againFrom = engine.getTransport().getState().ppq;   // where the take begins, or punches in
    againRecords = true;
    startedByVoice = true;
    handleTransport (transportAction ("record"));
}

void MainComponent::setInternetBlocked (bool blocked)
{
    internetBlocked = blocked;

    juce::DynamicObject::Ptr saved (new juce::DynamicObject());
    saved->setProperty ("blocked", blocked);
    internetBlockedFile().replaceWithText (juce::JSON::toString (juce::var (saved.get())));

    cloud->setBlocked (blocked);
    server->setBlocked (blocked);   // tells the page (the account and the AI panel)
    Settings::setInternetBlocked (blocked);   // the Plugins Creator Studio's gallery
    pushAi();

    // Plugins' web screens, and web browsing tabs, are sent to a blank page while blocked; they
    // reload where they were when it's lifted.
    if (blocked)
    {
        hideWebTabs();

        for (auto& tab : webTabs)
            if (tab.view != nullptr)
                tab.view->goToURL ("about:blank");
    }
    else
    {
        for (auto& tab : webTabs)
            if (tab.view != nullptr && tab.url.isNotEmpty())
                tab.view->goToURL (tab.url);
    }

    pushInternetBlocked();
}

MainComponent::WebTab* MainComponent::findWebTab (int id)
{
    for (auto& tab : webTabs)
        if (tab.id == id)
            return &tab;

    return nullptr;
}

int MainComponent::openWebTab (const juce::String& url, bool activate)
{
    WebTab tab;
    tab.id = nextWebTabId++;
    tab.url = url;
    webTabs.push_back (std::move (tab));
    saveWebTabs();

    if (webView != nullptr)
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("id", webTabs.back().id);
        item->setProperty ("url", url);
        item->setProperty ("title", juce::String());
        item->setProperty ("activate", activate);
        webView->emitEventIfBrowserIsVisible ("webTabOpened", juce::var (item.get()));
    }

    return webTabs.back().id;
}

void MainComponent::showWebTab (int id, juce::Rectangle<int> bounds)
{
    for (auto& tab : webTabs)
        if (tab.id != id && tab.view != nullptr)
            tab.view->setVisible (false);

    auto* tab = findWebTab (id);
    auto* peer = getPeer();

    if (tab == nullptr || tab->url.isEmpty() || bounds.isEmpty() || peer == nullptr)
    {
        hideWebTabs();   // an empty tab shows the page's own new-tab screen
        return;
    }

    if (webTabLayer == nullptr)
        webTabLayer = std::make_unique<WebTabLayer>();

    if (! webTabLayer->isOnDesktop())
        webTabLayer->addToDesktop (0, peer->getNativeHandle());

    // The page's CSS pixels are this component's own (the Hub's page fills it); the layer sits in
    // the main window, so the area is carried over to the window's coordinates.
    webTabLayer->setBounds (getTopLevelComponent()->getLocalArea (this, bounds));
    webTabLayer->setVisible (true);
    webTabLayer->toFront (false);   // above the Hub's own page, every time

    if (tab->view == nullptr)
    {
        tab->view = std::make_unique<WebTabView> (*this, id);
        webTabLayer->addChildComponent (*tab->view);
        tab->view->goToURL (internetBlocked ? juce::String ("about:blank") : tab->url);
    }

    tab->view->setBounds (webTabLayer->getLocalBounds());
    tab->view->setVisible (true);
}

void MainComponent::hideWebTabs()
{
    for (auto& tab : webTabs)
        if (tab.view != nullptr)
            tab.view->setVisible (false);

    if (webTabLayer != nullptr)
        webTabLayer->setVisible (false);
}

void MainComponent::navigateWebTab (int id, const juce::String& url)
{
    auto* tab = findWebTab (id);

    if (tab == nullptr || url.isEmpty() || internetBlocked)
        return;

    tab->url = url;
    saveWebTabs();

    if (tab->view != nullptr)
        tab->view->goToURL (url);
}

void MainComponent::closeWebTab (int id)
{
    const auto found = std::find_if (webTabs.begin(), webTabs.end(), [id] (const WebTab& tab) { return tab.id == id; });

    if (found == webTabs.end())
        return;

    if (found->view != nullptr)
        found->view->setVisible (false);

    webTabs.erase (found);
    saveWebTabs();

    if (webView != nullptr)
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("id", id);
        webView->emitEventIfBrowserIsVisible ("webTabClosed", juce::var (item.get()));
    }
}

void MainComponent::emitWebTabState (const WebTab& tab, bool loading, const juce::String& error)
{
    if (webView == nullptr)
        return;

    juce::DynamicObject::Ptr item (new juce::DynamicObject());
    item->setProperty ("id", tab.id);
    item->setProperty ("url", tab.url);
    item->setProperty ("title", tab.title);
    item->setProperty ("loading", loading);
    item->setProperty ("error", error);
    webView->emitEventIfBrowserIsVisible ("webTabState", juce::var (item.get()));
}

void MainComponent::webTabLoading (int id, const juce::String& url)
{
    // The blank page a tab is sent to while the internet is blocked: the tab keeps where it was.
    if (internetBlocked && url.startsWithIgnoreCase ("about:blank"))
        return;

    if (auto* tab = findWebTab (id))
    {
        tab->url = url;
        emitWebTabState (*tab, true);
    }
}

void MainComponent::webTabLoaded (int id, const juce::String& url)
{
    auto* tab = findWebTab (id);

    // As above: the blank page of a blocked internet isn't the tab's page, nor its title.
    if (tab == nullptr || (internetBlocked && url.startsWithIgnoreCase ("about:blank")))
        return;

    tab->url = url;
    saveWebTabs();
    emitWebTabState (*tab, false);

    // The page's title, for its tab.
    if (tab->view != nullptr)
    {
        tab->view->evaluateJavascript ("document.title", [safe = juce::Component::SafePointer<MainComponent> (this), id] (juce::WebBrowserComponent::EvaluationResult result)
        {
            // Blocked meanwhile: the title read would be the blank page's.
            if (safe == nullptr || safe->internetBlocked)
                return;

            if (auto* found = safe->findWebTab (id))
            {
                if (const auto* value = result.getResult())
                    found->title = value->toString().substring (0, 200);

                safe->saveWebTabs();
                safe->emitWebTabState (*found, false);
            }
        });
    }
}

void MainComponent::webTabError (int id, const juce::String& error)
{
    if (auto* tab = findWebTab (id))
        emitWebTabState (*tab, false, error);
}

void MainComponent::saveWebTabs() const
{
    juce::Array<juce::var> list;

    for (const auto& tab : webTabs)
    {
        if (tab.url.isEmpty())
            continue;

        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("url", tab.url);
        item->setProperty ("title", tab.title);
        list.add (juce::var (item.get()));
    }

    webTabsFile().replaceWithText (juce::JSON::toString (list));
}

void MainComponent::restoreWebTabs()
{
    // Last session's tabs come back; their pages load when each is first shown.
    if (! webTabs.empty() || webView == nullptr)
        return;

    const auto saved = juce::JSON::parse (webTabsFile());
    juce::Array<juce::var> restored;

    if (const auto* list = saved.getArray())
    {
        for (const auto& item : *list)
        {
            const auto url = item.getProperty ("url", {}).toString();

            if (! (url.startsWithIgnoreCase ("https://") || url.startsWithIgnoreCase ("http://")))
                continue;

            WebTab tab;
            tab.id = nextWebTabId++;
            tab.url = url;
            tab.title = item.getProperty ("title", {}).toString();

            juce::DynamicObject::Ptr entry (new juce::DynamicObject());
            entry->setProperty ("id", tab.id);
            entry->setProperty ("url", tab.url);
            entry->setProperty ("title", tab.title);
            restored.add (juce::var (entry.get()));
            webTabs.push_back (std::move (tab));
        }
    }

    juce::DynamicObject::Ptr item (new juce::DynamicObject());
    item->setProperty ("tabs", restored);
    webView->emitEventIfBrowserIsVisible ("webTabsRestore", juce::var (item.get()));
}

//==============================================================================
// The Fanan account: Fanan's server, with the same accounts and credits as Stella AI Studio.
void MainComponent::emitCloud (const juce::String& event, const juce::var& data)
{
    if (webView != nullptr)
        webView->emitEventIfBrowserIsVisible (event, data);
}

void MainComponent::pushAccount (const juce::String& message, const juce::String& error)
{
    juce::DynamicObject::Ptr item (new juce::DynamicObject());
    item->setProperty ("configured", server->isConfigured());
    item->setProperty ("siteAvailable", server->hasSite());
    item->setProperty ("status", statusName (server->getStatus()));
    item->setProperty ("statusText", server->getStatusText());
    item->setProperty ("signedIn", server->isSignedIn());
    item->setProperty ("email", server->getEmail());
    item->setProperty ("lastEmail", server->getLastEmail());   // suggested when signing in again
    item->setProperty ("credits", server->getCredits());
    item->setProperty ("computer", server->getComputerCode());
    item->setProperty ("busy", signingIn);
    item->setProperty ("waiting", waitingForSignIn && ! server->isSignedIn());
    item->setProperty ("message", message);
    item->setProperty ("error", error);
    emitCloud ("account", juce::var (item.get()));
    pushAi();       // the AI panel follows the account
    pushRemote();   // and so does the phone remote
}

void MainComponent::signIn (bool thenBuy, const juce::String& emailHint)
{
    if (! server->isConfigured() || ! server->hasSite())
        return pushAccount ({}, "Signing in isn't set up in this build yet.");

    if (server->isBlocked())
        return pushAccount ({}, "The internet is blocked (the switch at the top left). Turn it off to sign in.");

    if (! server->isReady())
    {
        server->checkNow();
        return pushAccount ({}, "Signing in needs Fanan's server. Check the internet connection and try again.");
    }

    if (signingIn)
        return;

    signingIn = true;
    pushAccount();

    // A one-time link opens in the browser; the site signs the user in and links this computer.
    server->signIn (thenBuy, emailHint, [safeThis = juce::Component::SafePointer<MainComponent> (this)] (const juce::Result& result, const juce::var&)
    {
        if (safeThis == nullptr)
            return;

        safeThis->signingIn = false;

        if (result.failed())
        {
            safeThis->pushAccount ({}, result.getErrorMessage());
            return;
        }

        safeThis->waitingForSignIn = true;
        safeThis->pushAccount();

        // If the browser is abandoned, the wait ends by itself after ten minutes.
        const auto attempt = ++safeThis->signInAttempt;

        juce::Timer::callAfterDelay (10 * 60 * 1000, [safeThis, attempt]
        {
            if (safeThis != nullptr && safeThis->signInAttempt == attempt && safeThis->waitingForSignIn)
            {
                safeThis->waitingForSignIn = false;
                safeThis->pushAccount();
            }
        });
    });
}

void MainComponent::signOut()
{
    // The page has asked first. This computer stops using the account; the credits stay with it.
    server->signOut ([safeThis = juce::Component::SafePointer<MainComponent> (this)] (const juce::Result& result, const juce::var&)
    {
        if (safeThis != nullptr)
            safeThis->pushAccount (result.wasOk() ? "Signed out. Your credits stay with your account." : juce::String(), result.getErrorMessage());
    });
}

void MainComponent::buyCredits()
{
    if (! server->isConfigured() || ! server->hasSite())
        return pushAccount ({}, "Buying credits isn't set up in this build yet.");

    // Credits belong to the account, so they work on every computer signed in to it: a
    // computer that isn't signed in signs in first, and the site goes on to the store.
    if (! server->isSignedIn())
        return signIn (true, server->getLastEmail());

    if (server->isBlocked())
        return pushAccount ({}, "The internet is blocked (the switch at the top left). Turn it off to buy credits.");

    if (server->getStoreUrl().launchInDefaultBrowser())
        server->watchForChanges();   // the new credits show up here within seconds of paying
    else
        pushAccount ({}, "The web browser couldn't be opened.");
}

//==============================================================================
// Fanan's content
juce::File MainComponent::contentFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Sonify").getChildFile ("Fanan Content");
}

void MainComponent::loadInstalledContent()
{
    installedContent = juce::JSON::parse (contentFolder().getChildFile ("installed.json"));

    if (! installedContent.isObject())
        installedContent = juce::var (new juce::DynamicObject());

    // What was deleted by hand isn't installed any more.
    auto* fields = installedContent.getDynamicObject();

    for (const auto& id : [fields] { juce::StringArray ids; for (const auto& p : fields->getProperties()) ids.add (p.name.toString()); return ids; }())
        if (! juce::File (fields->getProperty (id).getProperty ("path", {}).toString()).exists())
            fields->removeProperty (id);
}

void MainComponent::saveInstalledContent() const
{
    contentFolder().createDirectory();
    contentFolder().getChildFile ("installed.json").replaceWithText (juce::JSON::toString (installedContent));
}

void MainComponent::emitContent (const juce::String& error)
{
    juce::Array<juce::var> items;

    for (const auto& [id, original] : contentItems)
    {
        // Each item as listed, plus whether it's here, and whether Fanan has a newer one.
        juce::var item (new juce::DynamicObject());

        for (const auto& property : original.getDynamicObject()->getProperties())
            item.getDynamicObject()->setProperty (property.name, property.value);

        const auto installed = installedContent.getProperty (id, {});
        item.getDynamicObject()->setProperty ("installed", installed.isObject());
        item.getDynamicObject()->setProperty ("update", installed.isObject()
                                                        && installed.getProperty ("version", {}).toString() != original.getProperty ("version", {}).toString());
        item.getDynamicObject()->setProperty ("path", installed.getProperty ("path", {}).toString());
        items.add (item);
    }

    juce::DynamicObject::Ptr data (new juce::DynamicObject());
    data->setProperty ("items", items);
    data->setProperty ("error", error);
    data->setProperty ("configured", cloud->isConfigured());
    emitCloud ("fananContent", juce::var (data.get()));
}

void MainComponent::listContent()
{
    if (! cloud->isConfigured())
        return emitContent ("Fanan's content isn't set up in this build yet.");

    cloud->listContent ([this] (const juce::Result& result, const juce::var& items)
    {
        if (result.wasOk())
        {
            contentItems.clear();

            if (const auto* list = items.getArray())
                for (const auto& item : *list)
                    contentItems[item.getProperty ("id", {}).toString()] = item;
        }

        emitContent (result.getErrorMessage());
    });
}

void MainComponent::installContent (const juce::String& id)
{
    const auto found = contentItems.find (id);

    if (found == contentItems.end())
        return;

    const auto item = found->second;

    cloud->installContent (item, contentFolder(),
                           [this, id, item] (const juce::Result& result, const juce::var& value)
                           {
                               const auto kind = item.getProperty ("kind", {}).toString();

                               if (result.wasOk())
                               {
                                   juce::DynamicObject::Ptr record (new juce::DynamicObject());
                                   record->setProperty ("version", item.getProperty ("version", {}).toString());
                                   record->setProperty ("path", value.getProperty ("path", {}).toString());
                                   record->setProperty ("kind", kind);
                                   record->setProperty ("title", item.getProperty ("title", {}).toString());
                                   installedContent.getDynamicObject()->setProperty (id, juce::var (record.get()));
                                   saveInstalledContent();

                                   // A plugin joins the Browser: its folder becomes a Plugin Folder.
                                   if (kind == "plugin")
                                   {
                                       const auto plugins = contentFolder().getChildFile ("Plugins");

                                       if (! library.getFolders().contains (plugins.getFullPathName()))
                                           library.addFolder (plugins);

                                       startLibraryScan (false);
                                   }

                                   // A MIDI tool joins the MIDI tab's installed ones.
                                   if (kind == "miditool")
                                       listMidiTools();
                               }

                               juce::DynamicObject::Ptr data (new juce::DynamicObject());
                               data->setProperty ("id", id);
                               data->setProperty ("ok", result.wasOk());
                               data->setProperty ("error", result.getErrorMessage());
                               data->setProperty ("kind", kind);
                               data->setProperty ("title", item.getProperty ("title", {}).toString());
                               emitCloud ("contentResult", juce::var (data.get()));
                               emitContent();
                           },
                           [this, id] (double fraction, const juce::String& text)
                           {
                               juce::DynamicObject::Ptr data (new juce::DynamicObject());
                               data->setProperty ("id", id);
                               data->setProperty ("fraction", fraction);
                               data->setProperty ("text", text);
                               emitCloud ("contentProgress", juce::var (data.get()));
                           });
}

void MainComponent::previewContent (const juce::String& id)
{
    const auto found = contentItems.find (id);

    if (found == contentItems.end())
        return;

    const auto cache = SettingsFolder::folder ("Previews", "Previews");

    cloud->fetchPreview (found->second, cache, [this, id] (const juce::Result& result, const juce::var& value)
    {
        if (result.wasOk() && engine.previewAudio (juce::File (value.getProperty ("file", {}).toString())))
        {
            juce::DynamicObject::Ptr data (new juce::DynamicObject());
            data->setProperty ("id", id);
            data->setProperty ("file", value.getProperty ("file", {}).toString());
            emitCloud ("contentPreviewing", juce::var (data.get()));
            pushStatus();
            return;
        }

        juce::DynamicObject::Ptr data (new juce::DynamicObject());
        data->setProperty ("id", id);
        data->setProperty ("ok", false);
        data->setProperty ("error", result.failed() ? result.getErrorMessage() : juce::String ("Couldn't play the preview."));
        emitCloud ("contentResult", juce::var (data.get()));
    });
}

void MainComponent::openContent (const juce::String& id)
{
    const auto installed = installedContent.getProperty (id, {});
    const juce::File path (installed.getProperty ("path", {}).toString());

    if (! path.exists())
        return;

    // A song opens; a loop or a sample shows in the Files tab, ready to drag.
    if (installed.getProperty ("kind", {}).toString() == "song")
        whenSongIsSafe ([this, path] { openSong (path); });
    else
        listFolder (path.getParentDirectory().getFullPathName());
}

//==============================================================================
// Mixdown (Song > Export Mixdown)
juce::File MainComponent::defaultMixdownFolder() const
{
    // Like Studio One: the song's Mixdown folder.
    if (songFile != juce::File())
        return songFile.getParentDirectory().getChildFile ("Mixdown");

    return UserData::mixdown();   // Settings > Locations > User Data
}

void MainComponent::emitMixdown (const juce::String& state, const juce::File& file, double progress, const juce::String& error)
{
    if (webView == nullptr)
        return;

    juce::DynamicObject::Ptr item (new juce::DynamicObject());
    item->setProperty ("state", state);
    item->setProperty ("file", file.getFullPathName());
    item->setProperty ("name", file.getFileName());
    item->setProperty ("progress", progress);
    item->setProperty ("error", error);
    webView->emitEventIfBrowserIsVisible ("mixdown", juce::var (item.get()));
}

void MainComponent::exportMixdown (const juce::var& event)
{
    auto name = juce::File::createLegalFileName (event.getProperty ("name", {}).toString().trim());

    if (name.isEmpty())
        name = songTitle().isNotEmpty() ? juce::File::createLegalFileName (songTitle()) : juce::String ("Mixdown");

    juce::File folder = defaultMixdownFolder();
    const auto chosen = event.getProperty ("folder", {}).toString();

    if (chosen.isNotEmpty() && juce::File::isAbsolutePath (chosen))
        folder = juce::File (chosen);

    using Format = AudioEngine::MixdownSettings::Format;
    const auto formatName = event.getProperty ("format", "wav").toString();

    AudioEngine::MixdownSettings settings;
    settings.format = formatName == "mp3" ? Format::mp3 : formatName == "flac" ? Format::flac : formatName == "ogg" ? Format::ogg : Format::wav;
    settings.bits = static_cast<int> (event.getProperty ("bits", 24));
    settings.kbps = static_cast<int> (event.getProperty ("kbps", 320));
    settings.outputRate = static_cast<double> (event.getProperty ("rate", 0.0));
    settings.tailSeconds = static_cast<double> (event.getProperty ("tail", 2.0));
    const auto extension = AudioEngine::extensionOf (settings.format);

    // An earlier export is kept: the new one gets the next free name.
    const auto freeFile = [&extension] (const juce::File& in, const juce::String& base)
    {
        const auto file = in.getChildFile (base + extension);
        return file.exists() ? in.getNonexistentChildFile (base, extension, true) : file;
    };

    // Stems: each chosen channel to its own file, in a folder of their own; Main's mix with them when asked.
    const auto* stemIds = event.getProperty ("stems", {}).getArray();
    const bool stems = stemIds != nullptr;
    const bool withMain = ! stems || static_cast<bool> (event.getProperty ("main", false));
    const auto stemFolder = folder.getChildFile (name + " Stems");

    if (withMain)
        settings.file = freeFile (stems ? stemFolder : folder, name);

    // The Mastering section, when it's in: the mix with it (as set), without it, or both, the
    // one without beside the other as "<name> (unmastered)".
    if (engine.getMastering().settings.isOn (MasterSection::on) && withMain)
    {
        const auto mastering = event.getProperty ("mastering", "on").toString();
        settings.mastered = mastering != "off";

        if (mastering == "both")
            settings.unmasteredFile = freeFile (stems ? stemFolder : folder, name + " (unmastered)");
    }

    if (stems)
    {
        for (const auto& id : *stemIds)
        {
            if (const auto* track = engine.findTrack (static_cast<int> (id)))
            {
                auto channel = juce::File::createLegalFileName (track->name.trim());

                if (channel.isEmpty())
                    channel = "Track " + juce::String (track->id);

                // Two channels of the same name get their own files.
                auto file = freeFile (stemFolder, name + " - " + channel);

                for (const auto& taken : settings.stems)
                    if (taken.file == file)
                        file = stemFolder.getNonexistentChildFile (name + " - " + channel + " " + juce::String (track->id), extension, true);

                settings.stems.push_back ({ track->id, file });
            }
        }

        if (settings.stems.empty() && ! withMain)
        {
            emitMixdown ("failed", {}, 0.0, "Choose at least one channel to export.");
            return;
        }
    }

    const auto shown = settings.file != juce::File() && ! stems ? settings.file : stemFolder;
    const auto state = engine.getTransport().getState();

    if (event.getProperty ("range", "song").toString() == "loop")
    {
        settings.startPpq = state.loopStart;
        settings.endPpq = state.loopEnd;
    }
    else
    {
        settings.startPpq = 0.0;
        settings.endPpq = engine.getSongEnd();
    }

    revealMixdown = static_cast<bool> (event.getProperty ("reveal", false));

    if (masteringStage != MasteringStage::idle)
    {
        emitMixdown ("failed", shown, 0.0, "Not while the Mastering Assistant is working.");
        return;
    }

    if (videoExportStage != VideoExportStage::idle)
    {
        emitMixdown ("failed", shown, 0.0, "Not while a video is being exported.");
        return;
    }

    if (aiMixRender.running)
    {
        emitMixdown ("failed", shown, 0.0, "Not while Fanan AI is listening to the song (AI Mix): try again in a moment.");
        return;
    }

    if (freezingTrackId != 0)
    {
        emitMixdown ("failed", shown, 0.0, "Not while a track is freezing.");
        return;
    }

    // The mixdown is the master, never the original being compared with it.
    setMasteringListen (false, false);

    const auto result = engine.startMixdown (settings);
    emitMixdown (result.wasOk() ? "running" : "failed", shown, 0.0, result.getErrorMessage());
}

void MainComponent::updateMixdown()
{
    // AI Mix's measuring renders the song as a mixdown too (MainAiMix.cpp).
    if (aiMixRender.running)
    {
        updateAiMixRender();
        return;
    }

    // The Mastering Assistant renders the song as a mixdown too: its own way through.
    if (masteringStage != MasteringStage::idle)
    {
        updateMastering();
        return;
    }

    // So does freezing a track.
    if (freezingTrackId != 0)
    {
        updateFreeze();
        return;
    }

    // So does Export Clip to Audio.
    if (clipExport.trackId != 0)
    {
        updateClipExport();
        return;
    }

    // So do Merge Clips and Bounce to New Track (MainAudio.cpp).
    if (clipBounce.trackId != 0)
    {
        updateClipBounce();
        return;
    }

    // So does Export Video (MainVideo.cpp).
    if (videoExportStage != VideoExportStage::idle)
    {
        updateVideoExport();
        return;
    }

    if (! engine.isMixdownRunning())
        return;

    juce::File written;

    if (const auto result = engine.collectMixdown (written))
    {
        const auto cancelled = result->getErrorMessage() == "Cancelled";
        emitMixdown (result->wasOk() ? "done" : cancelled ? "cancelled" : "failed", written, result->wasOk() ? 1.0 : 0.0,
                     cancelled ? juce::String() : result->getErrorMessage());

        if (result->wasOk() && revealMixdown)
            written.revealToUser();

        pushStatus();
        return;
    }

    emitMixdown ("running", {}, engine.getMixdownProgress(), {});
}

//==============================================================================
void MainComponent::startFreezing (int trackId)
{
    const auto* track = engine.findTrack (trackId);

    if (track == nullptr || track->isMixChannel() || track->isFrozen())
        return pushTracks();

    // One render at a time; the page shows the one working.
    const auto refuse = [this] (const juce::String& why) { pushTracks (why); };

    if (freezingTrackId != 0)
        return refuse ("Another track is freezing: one at a time.");

    if (masteringStage != MasteringStage::idle)
        return refuse ("Not while the Mastering Assistant is working.");

    if (videoExportStage != VideoExportStage::idle)
        return refuse ("Not while a video is being exported.");

    if (engine.isMixdownRunning())
        return refuse ("Not while a mixdown is being made.");

    if (engine.isRecording() || engine.isTakeActive())
        return refuse ("Not while recording.");

    // With the song's recordings (Media), in a folder of their own; each render a new file.
    auto folder = engine.getRecordingFolder() != juce::File() ? engine.getRecordingFolder() : UserData::recordings();
    folder = folder.getChildFile ("Frozen");

    if (! folder.createDirectory())
        return refuse ("Couldn't make the folder " + folder.getFullPathName());

    auto name = juce::File::createLegalFileName (track->name.trim());

    if (name.isEmpty())
        name = "Track " + juce::String (trackId);

    const auto file = folder.getNonexistentChildFile (name + " (frozen " + juce::Uuid().toString().substring (0, 8) + ")", ".wav", false);

    if (const auto result = engine.startFreeze (trackId, file); result.failed())
        return refuse (result.getErrorMessage());

    freezingTrackId = trackId;
    freezeGeneration = songGeneration;
    pushTracks ("Freezing \"" + track->name + "\": the song is quiet until it's done. Click its snowflake again to stop.");
    pushStatus();
}

void MainComponent::stopFreezing (int trackId)
{
    // Still rendering: it stops (updateFreeze tidies up).
    if (freezingTrackId != 0 && freezingTrackId == trackId)
    {
        engine.cancelMixdown();
        return;
    }

    if (const auto* track = engine.findTrack (trackId); track != nullptr && track->isFrozen())
    {
        engine.unfreezeTrack (trackId);
        markChanged();
    }

    pushTracks();
}

void MainComponent::updateFreeze()
{
    juce::File written;
    const auto result = engine.collectMixdown (written);

    if (! result.has_value())
        return;   // still rendering: the status shows how far

    const auto id = std::exchange (freezingTrackId, 0);
    const auto* track = engine.findTrack (id);
    juce::String notice;
    bool frozen = false;

    if (result->failed())
    {
        // Cancelled or failed: the render is gone already.
        if (result->getErrorMessage() != "Cancelled")
            notice = "Couldn't freeze: " + result->getErrorMessage();
    }
    else if (track == nullptr || freezeGeneration != songGeneration)
    {
        // The track (or the song) went meanwhile.
    }
    else if (const auto applied = engine.applyFreeze (written); applied.failed())
    {
        notice = applied.getErrorMessage();   // the song changed meanwhile: it plays itself
    }
    else
    {
        frozen = true;
        markChanged();
    }

    if (result->wasOk() && ! frozen)
        written.deleteFile();

    pushTracks (notice);
    pushStatus();
}

//==============================================================================
void MainComponent::exportClip (int trackId, int clipId, bool chooseFolder)
{
    // Into the folder chosen last (Export Clip to Audio In... chooses it), else the song's Exports.
    const juce::File kept (prefs.clipExportFolder);
    const auto usual = kept.isDirectory() ? kept : (songFile != juce::File() ? songFile.getParentDirectory().getChildFile ("Exports")
                                                                              : UserData::mixdown().getChildFile ("Clips"));

    if (! chooseFolder)
        return startClipExport (trackId, clipId, usual);

    if (choosingFile)
        return;

    choosingFile = true;
    usual.createDirectory();
    clipExportChooser = std::make_unique<juce::FileChooser> ("Where the clip's audio goes", usual);
    clipExportChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                    [safeThis = juce::Component::SafePointer<MainComponent> (this), trackId, clipId] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        safeThis->choosingFile = false;
        const auto folder = chooser.getResult();

        if (folder == juce::File())
            return;

        safeThis->prefs.clipExportFolder = folder.getFullPathName();   // the next export goes there too
        safeThis->savePreferences();
        safeThis->startClipExport (trackId, clipId, folder);
    });
}

void MainComponent::startClipExport (int trackId, int clipId, const juce::File& folder)
{
    const auto refuse = [this] (const juce::String& why) { pushTracks (why); };

    if (clipExport.trackId != 0 || clipBounce.trackId != 0)
        return refuse ("Clips are being rendered already: one at a time.");

    if (freezingTrackId != 0 || masteringStage != MasteringStage::idle || videoExportStage != VideoExportStage::idle || engine.isMixdownRunning())
        return refuse ("Not while Sonify is rendering something else.");

    if (engine.isRecording() || engine.isTakeActive())
        return refuse ("Not while recording.");

    const auto* track = engine.findTrack (trackId);
    const MidiClip* clip = nullptr;

    if (track != nullptr)
        for (const auto& each : track->clips)
            if (each.id == clipId)
                clip = &each;

    if (clip == nullptr)
        return refuse ("That clip isn't in the song any more.");

    if (! folder.createDirectory())
        return refuse ("Couldn't make the folder " + folder.getFullPathName());

    // Its name: the track's and the clip's, with its tempo ("Vocal - take 2 120bpm.wav").
    const auto& map = engine.getTempoMap();
    const auto bpm = map.bpmAt (clip->start);
    const auto bar = map.barAt (clip->start);
    juce::String name = track->name.trim();

    if (clip->isAudio())
        name << " - " << juce::File (clip->audioFile).getFileNameWithoutExtension();

    name = juce::File::createLegalFileName (name.trim().isNotEmpty() ? name.trim() : juce::String ("Clip"));
    name << " " << juce::String (bpm, std::abs (bpm - std::round (bpm)) < 0.005 ? 0 : 2) << "bpm";

    ClipExport job;
    job.trackId = trackId;
    job.clipId = clipId;
    job.file = folder.getNonexistentChildFile (name, ".wav", false);
    job.render = job.file.getSiblingFile (job.file.getFileNameWithoutExtension() + ".rendering.wav");
    job.bpm = bpm;
    job.beats = clip->length * bar.denominator / 4.0;
    job.numerator = bar.numerator;
    job.denominator = bar.denominator;

    if (const auto result = engine.startClipRender (trackId, { clipId }, job.render); result.failed())
        return refuse (result.getErrorMessage());

    clipExport = job;
    pushTracks ("Exporting the clip to " + job.file.getFileName() + ": the song is quiet until it's done.");
    pushStatus();
}

void MainComponent::updateClipExport()
{
    juce::File written;
    const auto result = engine.collectMixdown (written);

    if (! result.has_value())
        return;   // still rendering

    const auto job = std::exchange (clipExport, {});
    juce::String notice;

    if (result->failed())
    {
        if (result->getErrorMessage() != "Cancelled")
            notice = "Couldn't export the clip: " + result->getErrorMessage();
    }
    else
    {
        // A 24-bit WAV with its tempo, beats and meter (an ACID chunk): loop players stretch it to theirs.
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (job.render));
        std::unique_ptr<juce::OutputStream> stream = job.file.createOutputStream();
        bool written24 = false;

        if (reader != nullptr && stream != nullptr)
        {
            const std::unordered_map<juce::String, juce::String> acid {
                { juce::WavAudioFormat::acidTempo, juce::String (job.bpm, 3) },
                { juce::WavAudioFormat::acidBeats, juce::String (juce::jmax (1, juce::roundToInt (job.beats))) },
                { juce::WavAudioFormat::acidNumerator, juce::String (job.numerator) },
                { juce::WavAudioFormat::acidDenominator, juce::String (job.denominator) },
                { juce::WavAudioFormat::acidStretch, "1" },
                { juce::WavAudioFormat::acidDiskBased, "1" }
            };

            if (auto writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                                                  .withSampleRate (reader->sampleRate)
                                                                                  .withNumChannels (static_cast<int> (reader->numChannels))
                                                                                  .withBitsPerSample (24)
                                                                                  .withMetadataValues (acid)))
            {
                written24 = writer->writeFromAudioReader (*reader, 0, reader->lengthInSamples);
            }
        }

        reader.reset();
        job.render.deleteFile();

        if (written24)
        {
            notice = "Exported: " + job.file.getFullPathName();
            prefs.browserFolder = job.file.getParentDirectory().getFullPathName();   // the Files tab shows it next
            savePreferences();
        }
        else
        {
            job.file.deleteFile();
            notice = "Couldn't write " + job.file.getFullPathName();
        }
    }

    job.render.deleteFile();
    pushTracks (notice);
    pushStatus();
}

void MainComponent::chooseMixdownFolder (const juce::String& current)
{
    if (choosingFile)
        return;

    choosingFile = true;
    const juce::File start (juce::File::isAbsolutePath (current) ? current : defaultMixdownFolder().getFullPathName());
    songChooser = std::make_unique<juce::FileChooser> ("Where the mixdown goes", start.isDirectory() ? start : start.getParentDirectory());
    songChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [safeThis = juce::Component::SafePointer<MainComponent> (this)] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        safeThis->choosingFile = false;
        const auto folder = chooser.getResult();

        if (folder != juce::File() && safeThis->webView != nullptr)
        {
            juce::DynamicObject::Ptr item (new juce::DynamicObject());
            item->setProperty ("path", folder.getFullPathName());
            safeThis->webView->emitEventIfBrowserIsVisible ("mixdownFolder", juce::var (item.get()));
        }
    });
}

juce::var MainComponent::automationToVar (const std::vector<AutomationLane>& lanes)
{
    juce::Array<juce::var> list;

    for (const auto& lane : lanes)
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        const auto& target = lane.target;
        item->setProperty ("kind", target.kind == AutomationTarget::Kind::volume ? "volume"
                                   : target.kind == AutomationTarget::Kind::pan ? "pan" : "param");
        item->setProperty ("slot", target.slot);
        item->setProperty ("param", static_cast<juce::int64> (target.paramId));

        juce::Array<juce::var> points;

        for (const auto& point : lane.points)
            points.add (juce::Array<juce::var> { point.ppq, point.value });

        item->setProperty ("points", points);

        if (lane.shapes.isArray() && lane.shapes.size() > 0)
            item->setProperty ("shapes", lane.shapes);

        list.add (juce::var (item.get()));
    }

    return list;
}

juce::String MainComponent::channelName (int channel) const
{
    if (channel == AudioEngine::mainChannel)
        return "Main";

    const auto* track = engine.findTrack (channel);
    return track != nullptr ? track->name : juce::String();
}

void MainComponent::switchPlugin (const Plugin& plugin, bool on)
{
    if (engine.getPluginOn (plugin) == (on ? 1 : 0))
        return;

    markChanged();
    engine.setPluginOn (plugin, on);
    pushTracks();
}

void MainComponent::openPluginScreen (int channel, int slot)
{
    auto* plugin = engine.getPlugin (channel, slot);

    if (plugin == nullptr || webView == nullptr)
        return;

    // A frozen track's plugins rest: what's changed in their windows wouldn't be heard. Nor
    // would it be in a freeze rendering now, from the track or one it needs.
    if (const auto* track = engine.findTrack (channel); track != nullptr && track->isFrozen())
    {
        pushTracks ("\"" + track->name + "\" is frozen: its snowflake unfreezes it, then its plugins open.");
        return;
    }

    if (const auto* track = engine.findTrack (channel); track != nullptr && engine.isFreezeRendering (channel))
    {
        pushTracks (channel == freezingTrackId ? "\"" + track->name + "\" is freezing: its plugins open once that's done."
                                               : "\"" + track->name + "\" plays a part in the track freezing now: its plugins open once that's done.");
        return;
    }

    const auto title = channelName (channel) + " - " + plugin->getDescriptor().name;

    juce::DynamicObject::Ptr reply (new juce::DynamicObject());
    reply->setProperty ("channel", channel);
    reply->setProperty ("slot", slot);
    reply->setProperty ("title", title);

    switch (plugin->getScreenKind())
    {
        case Plugin::ScreenKind::native:
            reply->setProperty ("kind", "native");
            reply->setProperty ("shown", plugin->showNativeScreen (title));
            break;

        case Plugin::ScreenKind::web:
        {
            // A full URL opens as it is; a path is served by the plugin, under the Hub's prefix.
            auto uri = plugin->getScreenUri();
            const auto absolute = uri.startsWithIgnoreCase ("http:") || uri.startsWithIgnoreCase ("https:") || uri.startsWithIgnoreCase ("data:");

            if (! absolute && ! uri.startsWith ("/"))
                uri = "/" + uri;

            int width = 0, height = 0;

            if (plugin->getScreenSize (width, height))
            {
                reply->setProperty ("width", width);
                reply->setProperty ("height", height);
            }

            reply->setProperty ("kind", "web");
            reply->setProperty ("url", absolute ? uri : "plugin-screen/" + juce::String (channel) + "/" + juce::String (slot) + uri);

            if (absolute && ! uri.startsWithIgnoreCase ("data:"))
                screenOrigins.insert (HubPageView::originOf (uri));
            plugin->setScreenOpen (true);
            openScreens.insert ({ channel, slot });
            break;
        }

        case Plugin::ScreenKind::none:
        default:
            reply->setProperty ("kind", "generic");
            break;
    }

    webView->emitEventIfBrowserIsVisible ("pluginScreen", juce::var (reply.get()));
}

void MainComponent::forwardScreenMessages()
{
    for (auto it = openScreens.begin(); it != openScreens.end();)
    {
        auto* plugin = engine.getPlugin (it->first, it->second);

        if (plugin == nullptr)
        {
            it = openScreens.erase (it);   // the plugin has gone; the page closes its window
            continue;
        }

        const auto messages = plugin->takeScreenMessages();

        if (! messages.empty() && webView != nullptr)
        {
            juce::Array<juce::var> encoded;

            for (const auto& message : messages)
                encoded.add (message.toBase64Encoding());

            juce::DynamicObject::Ptr item (new juce::DynamicObject());
            item->setProperty ("channel", it->first);
            item->setProperty ("slot", it->second);
            item->setProperty ("messages", encoded);
            webView->emitEventIfBrowserIsVisible ("screenMessages", juce::var (item.get()));
        }

        ++it;
    }
}

//==============================================================================
//==============================================================================
// The song as a document
namespace
{
    constexpr int maxRecentSongs = 12;
    const char* const appName = "Sonify";

    juce::File getRecentSongsFile()
    {
        return SettingsFolder::file ("recent.json", "FananWebHub-recent.json");
    }
}

juce::String MainComponent::songTitle() const
{
    return songFile == juce::File() ? juce::String ("Untitled") : songFile.getFileNameWithoutExtension();
}

void MainComponent::updateTitle()
{
    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
        window->setName (songTitle() + (songChanged ? " *" : "") + " - " + appName);
}

void MainComponent::parentHierarchyChanged()
{
    updateTitle();
}

void MainComponent::markChanged()
{
    if (songChanged)
        return;

    songChanged = true;
    updateTitle();
    pushSong();
}

void MainComponent::loadRecentSongs()
{
    recentSongs.clear();

    const auto parsed = juce::JSON::parse (getRecentSongsFile());   // kept: the list lives in it

    if (const auto* list = parsed.getArray())
        for (const auto& path : *list)
            if (juce::File::isAbsolutePath (path.toString()))
                recentSongs.addIfNotAlreadyThere (path.toString());
}

void MainComponent::rememberRecent (const juce::File& file)
{
    recentSongs.removeString (file.getFullPathName());
    recentSongs.insert (0, file.getFullPathName());

    while (recentSongs.size() > maxRecentSongs)
        recentSongs.remove (recentSongs.size() - 1);

    juce::Array<juce::var> list;

    for (const auto& path : recentSongs)
        list.add (path);

    getRecentSongsFile().replaceWithText (juce::JSON::toString (juce::var (list)));
}

void MainComponent::pushSong (bool opened, const juce::String& busy)
{
    if (webView == nullptr)
        return;

    juce::Array<juce::var> recent;

    for (const auto& path : recentSongs)
    {
        const juce::File file (path);
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("name", file.getFileNameWithoutExtension());
        item->setProperty ("path", path);
        item->setProperty ("exists", file.existsAsFile());
        item->setProperty ("time", file.existsAsFile() ? static_cast<double> (file.getLastModificationTime().toMilliseconds()) : 0.0);
        recent.add (juce::var (item.get()));
    }

    juce::DynamicObject::Ptr song (new juce::DynamicObject());
    song->setProperty ("name", songTitle());
    song->setProperty ("path", songFile.getFullPathName());
    song->setProperty ("mixdownFolder", defaultMixdownFolder().getFullPathName());
    song->setProperty ("changed", songChanged);
    song->setProperty ("welcome", noProject);   // no project open: the Start page is the place
    song->setProperty ("busy", busy);
    song->setProperty ("folder", Song::getDefaultFolder().getFullPathName());
    song->setProperty ("user", juce::SystemStats::getLogonName());
    song->setProperty ("recent", recent);
    song->setProperty ("undo", undoManager.canUndo() ? undoManager.getUndoDescription() : juce::String());
    song->setProperty ("redo", undoManager.canRedo() ? undoManager.getRedoDescription() : juce::String());

    // A song that was just opened or made: the page takes its view settings (none: the defaults).
    if (opened)
    {
        song->setProperty ("opened", true);
        song->setProperty ("view", songView);
    }

    webView->emitEventIfBrowserIsVisible ("song", juce::var (song.get()));
}

void MainComponent::showSongMessage (const juce::String& title, const juce::String& message)
{
    juce::NativeMessageBox::showAsync (juce::MessageBoxOptions()
                                           .withIconType (juce::MessageBoxIconType::WarningIcon)
                                           .withTitle (title)
                                           .withMessage (message)
                                           .withButton ("OK")
                                           .withAssociatedComponent (this),
                                       nullptr);
}

void MainComponent::whenSongIsSafe (std::function<void()> next)
{
    if (! songChanged)
    {
        next();
        return;
    }

    if (askingToSave)
        return;   // one question at a time

    askingToSave = true;
    const auto options = juce::MessageBoxOptions::makeOptionsYesNoCancel (juce::MessageBoxIconType::QuestionIcon,
                                                                          "Save changes?",
                                                                          "Do you want to save the changes to \"" + songTitle() + "\"?",
                                                                          "Save", "Don't Save", "Cancel", this);

    juce::NativeMessageBox::showAsync (options, [safeThis = juce::Component::SafePointer<MainComponent> (this), next] (int button)
    {
        if (safeThis == nullptr)
            return;

        safeThis->askingToSave = false;

        if (button == 0)
            safeThis->saveSong (next);   // goes on only once it's saved
        else if (button == 1)
            next();
    });
}

bool MainComponent::writeSong (const juce::File& file)
{
    const auto text = juce::JSON::toString (withFilesFolder (Song::capture (engine, file.getFileNameWithoutExtension(), songView, file)));

    if (! file.getParentDirectory().createDirectory() || ! file.replaceWithText (text))
    {
        showSongMessage ("Couldn't save", "The song couldn't be written to " + file.getFullPathName() + ".");
        return false;
    }

    songFile = file;
    songChanged = false;
    engine.markFrozenSaved();   // the song names its frozen tracks' renders now
    rememberRecent (file);
    updateRecordingFolder();
    updateTitle();
    pushSong();
    return true;
}

void MainComponent::saveSong (std::function<void()> afterSaving)
{
    if (songFile == juce::File())
    {
        chooseSongLocation (std::move (afterSaving));
        return;
    }

    if (writeSong (songFile) && afterSaving)
        afterSaving();
}

void MainComponent::chooseSongLocation (std::function<void()> afterSaving)
{
    auto folder = Song::getDefaultFolder();
    folder.createDirectory();
    const auto suggested = songFile != juce::File() ? songFile : folder.getChildFile (songTitle() + Song::fileExtension);

    songChooser = std::make_unique<juce::FileChooser> ("Save the song as", suggested, juce::String ("*") + Song::fileExtension);
    songChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safeThis = juce::Component::SafePointer<MainComponent> (this), afterSaving] (const juce::FileChooser& chooser)
    {
        const auto chosen = chooser.getResult();

        // Cancelled: whatever was waiting for the save (a New, an Open, quitting) doesn't happen.
        if (safeThis == nullptr || chosen == juce::File())
            return;

        if (safeThis->writeSong (chosen.withFileExtension (Song::fileExtension)) && afterSaving)
            afterSaving();
    });
}

void MainComponent::chooseSongToOpen()
{
    auto folder = Song::getDefaultFolder();
    folder.createDirectory();

    songChooser = std::make_unique<juce::FileChooser> ("Open a song", folder, juce::String ("*") + Song::fileExtension);
    songChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis = juce::Component::SafePointer<MainComponent> (this)] (const juce::FileChooser& chooser)
    {
        if (safeThis != nullptr && chooser.getResult() != juce::File())
            safeThis->openSong (chooser.getResult());
    });
}

void MainComponent::openSong (const juce::File& file)
{
    if (! file.existsAsFile())
    {
        showSongMessage ("Song not found", file.getFullPathName() + " isn't there any more.");
        return;
    }

    // Loading plugins can take a moment: the page shows "Opening" first.
    pushSong (false, "Opening " + file.getFileNameWithoutExtension() + "...");

    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<MainComponent> (this), file]
    {
        if (safeThis == nullptr)
            return;

        const auto song = juce::JSON::parse (file.loadFileAsString());
        juce::StringArray problems;

        // A song just imported (File > Import DAWproject...): what didn't come across, said first.
        const auto messageTitle = std::exchange (safeThis->importTitle, {});
        problems.addArray (std::exchange (safeThis->importNotes, {}));

        const auto result = Song::restore (safeThis->engine, song, problems, file);

        if (safeThis->aiTools != nullptr)
            safeThis->aiTools->forgetWhatItMade();

        if (result.wasOk())
        {
            safeThis->undoManager.clearUndoHistory();   // the old song's steps don't apply to this one
            safeThis->engine.discardRenders();          // nor its frozen tracks' renders, unless saved
            safeThis->forgetWhereItBegan();
            safeThis->takeRunning = false;              // nor a take it was recording (clearSong ended it)
            safeThis->takeBefore.clear();
            ++safeThis->songGeneration;
        }

        if (result.failed())
        {
            safeThis->pushSong();
            safeThis->showSongMessage ("Couldn't open " + file.getFileNameWithoutExtension(), result.getErrorMessage());
            return;
        }

        safeThis->songFile = file;
        safeThis->noProject = false;
        safeThis->songChanged = false;
        safeThis->updateRecordingFolder();
        safeThis->songView = song.getProperty ("view", {});
        safeThis->songFilesFolder = song.getProperty ("filesFolder", {}).toString();
        safeThis->loadingFiles.clear();
        safeThis->trackMessages.clear();
        safeThis->rememberRecent (file);
        safeThis->updateTitle();
        safeThis->pushTracks();
        safeThis->pushSong (true);
        safeThis->showSongFilesFolder();   // its own files folder, else the one for new songs

        if (! problems.isEmpty())
        {
            // A long list is cut short: the first ones say what kind of things they are.
            auto shown = problems;

            if (shown.size() > 24)
            {
                shown.removeRange (24, shown.size());
                shown.add ("... and " + juce::String (problems.size() - 24) + " more.");
            }

            safeThis->showSongMessage (messageTitle.isNotEmpty() ? messageTitle : juce::String ("Some of the song couldn't be loaded"),
                                       shown.joinIntoString ("\n"));
        }
    });
}

void MainComponent::newSong (const juce::var& event)
{
    auto name = juce::File::createLegalFileName (event.getProperty ("name", {}).toString().trim());

    if (name.isEmpty())
        name = "Untitled";

    juce::File folder (event.getProperty ("folder", {}).toString());

    if (! juce::File::isAbsolutePath (folder.getFullPathName()))
        folder = Song::getDefaultFolder();

    // Like Studio One, the song gets a folder of its own; a name that's taken gets " (2)".
    auto songFolder = folder.getChildFile (name);

    if (songFolder.exists())
        songFolder = songFolder.getNonexistentSibling (true);

    if (! songFolder.createDirectory())
    {
        showSongMessage ("Couldn't make the song", "The folder " + songFolder.getFullPathName() + " couldn't be made.");
        return;
    }

    undoManager.clearUndoHistory();
    engine.clearSong();
    engine.discardRenders();   // the old song's frozen tracks' renders, unless saved

    if (aiTools != nullptr)
        aiTools->forgetWhatItMade();

    engine.addTrack();
    forgetWhereItBegan();
    takeRunning = false;       // a take the old song was recording isn't this one's undo step
    takeBefore.clear();
    ++songGeneration;
    songFile = juce::File();   // the new song's Media folder applies once it's written below

    TempoMap map;
    map.setTempos ({ { 0.0, juce::jlimit (TempoMap::minTempo, TempoMap::maxTempo, static_cast<double> (event.getProperty ("bpm", 120.0))), false } });
    map.setSignatures ({ { 0, static_cast<int> (event.getProperty ("numerator", 4)), static_cast<int> (event.getProperty ("denominator", 4)) } });
    engine.setTempoMap (map);
    pushSongChannel();

    loadingFiles.clear();
    trackMessages.clear();
    songView = juce::var();
    songFilesFolder = {};      // it shows the folder for new songs until one is chosen for it

    // On disk straight away, as Studio One does.
    writeSong (songFolder.getChildFile (songFolder.getFileName() + Song::fileExtension));
    noProject = false;
    pushTracks();
    pushSong (true);
    showSongFilesFolder();
}

void MainComponent::closeSong()
{
    whenSongIsSafe ([this]
    {
        undoManager.clearUndoHistory();
        engine.clearSong();
        engine.discardRenders();   // its frozen tracks' renders, unless saved

        if (aiTools != nullptr)
            aiTools->forgetWhatItMade();

        forgetWhereItBegan();
        takeRunning = false;       // nor a take it was recording
        takeBefore.clear();
        ++songGeneration;
        songFile = juce::File();
        loadingFiles.clear();
        trackMessages.clear();
        songView = juce::var();
        songFilesFolder = {};
        songChanged = false;
        noProject = true;
        updateRecordingFolder();
        updateTitle();
        pushTracks();
        pushSong (true);   // the page drops the song's view, and the Start page returns
    });
}

//==============================================================================
// Song > Mastering Assistant
int MainComponent::findMasteringInsert() const
{
    const auto& chain = engine.getMainInserts();

    for (int i = chain.size(); --i >= 0;)
        if (const auto* insert = chain.at (i); insert != nullptr && insert->plugin != nullptr
                                               && insert->plugin->getDescriptor().id == Builtins::masteringId)
            return insert->id;

    return 0;
}

MasteringPlugin* MainComponent::findMasteringPlugin()
{
    const auto insertId = findMasteringInsert();
    return insertId != 0 ? dynamic_cast<MasteringPlugin*> (engine.getPlugin (AudioEngine::mainChannel, insertId)) : nullptr;
}

juce::String MainComponent::masteringStateName() const
{
    return masteringStage == MasteringStage::rendering ? "rendering"
         : masteringStage == MasteringStage::analysing ? "analysing" : "idle";
}

double MainComponent::masteringProgress() const
{
    if (masteringStage == MasteringStage::rendering)
        return engine.getMixdownProgress();

    return masteringAssistant != nullptr ? masteringAssistant->getProgress() : 0.0;
}

void MainComponent::emitMastering (const juce::String& state, double progress, const juce::String& error)
{
    if (webView == nullptr)
        return;

    juce::DynamicObject::Ptr item (new juce::DynamicObject());
    item->setProperty ("state", state);
    item->setProperty ("progress", progress);
    item->setProperty ("error", error);
    item->setProperty ("insertId", findMasteringInsert());

    // What the assistant found last time, kept with the song in Sonify Mastering.
    if (auto* plugin = findMasteringPlugin())
    {
        item->setProperty ("result", plugin->getInfo());
        item->setProperty ("original", plugin->isListeningToOriginal());
        item->setProperty ("sameLoudness", plugin->isSameLoudness());
    }

    webView->emitEventIfBrowserIsVisible ("mastering", juce::var (item.get()));
}

void MainComponent::startMastering (const juce::var& event)
{
    if (masteringStage != MasteringStage::idle)
        return;   // one at a time: the page shows the one working

    const auto refuse = [this] (const juce::String& why) { emitMastering ("failed", 0.0, why); };

    if (engine.isMixdownRunning())
        return refuse ("Not while a mixdown is being made.");

    if (videoExportStage != VideoExportStage::idle)
        return refuse ("Not while a video is being exported.");

    if (engine.isRecording())
        return refuse ("Not while recording.");

    // Sonify Mastering sits on Main, which the board leaves out while it mixes.
    if (engine.isBoardMixing())
        return refuse ("The Analog Board mixes this song, and Main's mastering isn't heard then: mix on the Console to master it here.");

    if (engine.isMasterMuted() || engine.getMasterVolume() <= Track::silentDb)
        return refuse ("Main is muted: unmute it, so the song can be heard.");

    const auto end = engine.getSongEnd();

    if (end <= 0.0)
        return refuse ("The song is empty: there's nothing to master.");

    masteringOptions.loudness = MasteringAssistant::loudnessFrom (event.getProperty ("loudness", "streaming").toString());
    masteringOptions.character = MasteringAssistant::characterFrom (event.getProperty ("character", "clean").toString());

    // The render is after Main's fader; the mastering sits before it, so the fader is undone.
    masteringOptions.inputGain = 1.0 / juce::Decibels::decibelsToGain (static_cast<double> (engine.getMasterVolume()));

    // The song as Main plays it, without an earlier mastering, rendered faster than real time.
    if (const auto existing = findMasteringInsert(); existing != 0)
    {
        if (const auto* insert = engine.getMainInserts().find (existing); insert != nullptr && ! insert->bypassed.load())
        {
            engine.setInsertBypass (AudioEngine::mainChannel, existing, true);
            masteringBypassed = existing;
        }
    }

    setMasteringListen (false, false);
    masteringRender = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("Sonify mastering", ".wav", false);

    AudioEngine::MixdownSettings settings;
    settings.file = masteringRender;
    settings.bits = 32;
    settings.startPpq = 0.0;
    settings.endPpq = end;
    settings.tailSeconds = 1.0;
    settings.mastered = false;   // the assistant listens to the song before the Mastering section too

    const auto result = engine.startMixdown (settings);

    if (result.failed())
    {
        restoreMasteringBypass();
        return refuse (result.getErrorMessage());
    }

    masteringGeneration = songGeneration;
    masteringStage = MasteringStage::rendering;
    emitMastering ("rendering", 0.0);
}

void MainComponent::cancelMastering()
{
    if (masteringStage == MasteringStage::rendering)
        engine.cancelMixdown();
    else if (masteringStage == MasteringStage::analysing && masteringAssistant != nullptr)
        masteringAssistant->cancel();
}

void MainComponent::restoreMasteringBypass()
{
    if (masteringBypassed != 0)
        engine.setInsertBypass (AudioEngine::mainChannel, masteringBypassed, false);

    masteringBypassed = 0;
}

void MainComponent::updateMastering()
{
    if (masteringStage == MasteringStage::rendering)
    {
        juce::File written;

        if (const auto result = engine.collectMixdown (written))
        {
            restoreMasteringBypass();
            pushStatus();

            if (result->failed())
            {
                masteringRender.deleteFile();
                masteringStage = MasteringStage::idle;
                const auto cancelled = result->getErrorMessage() == "Cancelled";
                emitMastering (cancelled ? "cancelled" : "failed", 0.0, cancelled ? juce::String() : result->getErrorMessage());
                return;
            }

            // Listening and deciding take a few seconds more, on a thread of their own.
            masteringStage = MasteringStage::analysing;
            masteringAssistant = std::make_unique<MasteringAssistant> (masteringRender, masteringOptions);
            masteringAssistant->start();
            emitMastering ("analysing", 0.0);
            return;
        }

        emitMastering ("rendering", engine.getMixdownProgress());
        return;
    }

    if (masteringAssistant == nullptr)
    {
        masteringStage = MasteringStage::idle;
        return;
    }

    if (! masteringAssistant->isDone())
    {
        emitMastering ("analysing", masteringAssistant->getProgress());
        return;
    }

    const auto result = masteringAssistant->getResult();
    masteringAssistant.reset();
    masteringRender.deleteFile();
    masteringStage = MasteringStage::idle;

    // Another song opened meanwhile: this master was for the one before.
    if (masteringGeneration != songGeneration)
        return emitMastering ("cancelled");

    if (! result.ok)
    {
        const auto cancelled = result.error == "Cancelled";
        return emitMastering (cancelled ? "cancelled" : "failed", 0.0, cancelled ? juce::String() : result.error);
    }

    applyMastering (result);
}

void MainComponent::applyMastering (const MasteringAssistant::Result& result)
{
    const auto rounded = [] (double value) { return std::round (value * 10.0) / 10.0; };

    juce::Array<juce::var> eq;

    for (const auto band : result.eqDb)
        eq.add (rounded (band));

    juce::DynamicObject::Ptr info (new juce::DynamicObject());
    info->setProperty ("loudness", MasteringAssistant::nameOf (masteringOptions.loudness));
    info->setProperty ("character", MasteringAssistant::nameOf (masteringOptions.character));
    info->setProperty ("target", result.targetLufs);
    info->setProperty ("ceiling", MasteringAssistant::ceilingDb);
    info->setProperty ("beforeLufs", rounded (result.beforeLufs));
    info->setProperty ("beforePeak", rounded (result.beforePeakDb));
    info->setProperty ("afterLufs", rounded (result.afterLufs));
    info->setProperty ("afterPeak", rounded (result.afterPeakDb));
    info->setProperty ("eq", eq);
    info->setProperty ("compression", rounded (result.compressionDb));
    info->setProperty ("limiting", rounded (result.limitingDb));
    info->setProperty ("when", static_cast<double> (juce::Time::currentTimeMillis()));

    // A fresh Sonify Mastering as Main's last insert, or in place of the one there: one undo step.
    const auto existing = findMasteringInsert();
    int insertId = 0;
    AudioEngine::Displaced displaced;
    const auto nameBefore = Edits::nameOf (engine, AudioEngine::mainChannel);
    const auto loaded = engine.loadPlugin (AudioEngine::mainChannel, Builtins::file(), Builtins::masteringId,
                                           -1, existing, insertId, &displaced);

    if (loaded.failed())
        return emitMastering ("failed", 0.0, loaded.getErrorMessage());

    if (auto* plugin = dynamic_cast<MasteringPlugin*> (engine.getPlugin (AudioEngine::mainChannel, insertId)))
        plugin->applyAssistant (result.settings, juce::var (info.get()), result.afterLufs - result.beforeLufs);

    recordPluginLoad (AudioEngine::mainChannel, insertId, displaced, nameBefore, false);

    // The mastering sets the song's level, so Main's fader goes to 0 dB (in the same undo step).
    const auto faderBefore = engine.getMasterVolume();
    engine.setMasterVolume (0.0f);

    if (! juce::exactlyEqual (faderBefore, engine.getMasterVolume()))
        addToHistory ("Mastering", new MainFaderChanged (engine, faderBefore, engine.getMasterVolume()), true);

    library.markUsed (Builtins::file().getFullPathName(), Builtins::masteringId);

    markChanged();
    pushTracks();
    pushLibrary();
    emitMastering ("done", 1.0);
}

void MainComponent::setMasteringListen (bool original, bool sameLoudness)
{
    if (auto* plugin = findMasteringPlugin())
        plugin->setListen (original, sameLoudness);
}

//==============================================================================
// Track > Add Session Players
void MainComponent::sendSessionSetup()
{
    if (webView == nullptr)
        return;

    // Each player's best fit among the installed instruments (none: the Test Synth plays it).
    juce::DynamicObject::Ptr best (new juce::DynamicObject());

    for (const auto player : { SessionPlayers::Player::drums, SessionPlayers::Player::bass, SessionPlayers::Player::keys })
    {
        juce::var choice;

        if (const auto* entry = SessionPlayers::bestInstrument (library, player))
        {
            juce::DynamicObject::Ptr item (new juce::DynamicObject());
            item->setProperty ("file", entry->file);
            item->setProperty ("id", entry->id);
            item->setProperty ("name", entry->name);
            choice = juce::var (item.get());
        }

        best->setProperty (SessionPlayers::nameOf (player), choice);
    }

    juce::DynamicObject::Ptr reply (new juce::DynamicObject());
    reply->setProperty ("best", juce::var (best.get()));
    webView->emitEventIfBrowserIsVisible ("sessionSetup", juce::var (reply.get()));
}

void MainComponent::detectSessionChords (int trackId, double tempo)
{
    if (webView == nullptr)
        return;

    // A recording: listened to on a thread of its own, the page kept up to date as it goes.
    if (const auto* track = engine.findTrack (trackId); track != nullptr && track->kind == Track::Kind::audio)
    {
        cancelRecordingAnalysis();
        heardTrack = trackId;
        heard = {};
        auto clips = RecordingAnalysis::clipsOf (engine, trackId);

        if (clips.empty())
        {
            heard.error = "That track has no recording to listen to.";
            emitHeard (false, 0.0);
            return;
        }

        // Heard against the song's beat where the recording starts.
        auto first = clips.front().startSeconds;
        for (const auto& c : clips) first = juce::jmin (first, c.startSeconds);
        const auto& map = engine.getTempoMap();
        heardLayout = RecordingAnalysis::layoutOf (engine);
        recordingAnalysis = std::make_unique<RecordingAnalysis> (std::move (clips), RecordingAnalysis::gridAt (map, map.ppqAt (first)),
                                                                 juce::jlimit (0.0, 400.0, tempo));
        recordingAnalysis->start();
        heardProgressSent = 0.0;
        emitHeard (true, 0.0);
        return;
    }

    int firstBar = 1, bars = 0;
    juce::String chords;

    if (const auto* track = engine.findTrack (trackId); track != nullptr && track->kind == Track::Kind::instrument)
    {
        chords = SessionPlayers::detectChords (engine.getClips (trackId), engine.getTempoMap(), firstBar, bars);
    }

    juce::DynamicObject::Ptr reply (new juce::DynamicObject());
    reply->setProperty ("trackId", trackId);
    reply->setProperty ("chords", chords);
    reply->setProperty ("startBar", firstBar);
    reply->setProperty ("bars", bars);
    webView->emitEventIfBrowserIsVisible ("sessionChords", juce::var (reply.get()));
}

void MainComponent::addSessionPlayers (const juce::var& event)
{
    std::vector<int> trackIds;
    juce::StringArray notes;

    const auto answer = [this, &trackIds, &notes] (const juce::Result& result)
    {
        if (webView == nullptr)
            return;

        juce::Array<juce::var> ids, told;

        for (const auto id : trackIds)
            ids.add (id);

        for (const auto& note : notes)
            told.add (note);

        juce::DynamicObject::Ptr reply (new juce::DynamicObject());
        reply->setProperty ("ok", result.wasOk());
        reply->setProperty ("error", result.getErrorMessage());
        reply->setProperty ("trackIds", ids);
        reply->setProperty ("notes", told);
        webView->emitEventIfBrowserIsVisible ("sessionPlayers", juce::var (reply.get()));
    };

    if (engine.isRecording() || engine.isTakeActive())
        return answer (juce::Result::fail ("Not while recording."));

    if (engine.isMixdownRunning())
        return answer (juce::Result::fail ("Not while a mixdown is being made."));

    if (recordingAnalysis != nullptr)
        return answer (juce::Result::fail ("Sonify is still listening to the recording."));

    SessionPlayers::Request request;
    request.style = SessionPlayers::styleFrom (event.getProperty ("style", "pop").toString());
    request.intensity = juce::jlimit (0.0, 1.0, static_cast<double> (event.getProperty ("intensity", 0.5)));
    request.chords = event.getProperty ("chords", {}).toString();
    request.startBar = juce::jlimit (1, 9999, static_cast<int> (event.getProperty ("startBar", 1)));
    request.times = juce::jlimit (1, 64, static_cast<int> (event.getProperty ("times", 1)));

    if (const auto* players = event.getProperty ("players", {}).getArray())
    {
        request.players.clear();

        for (const auto& name : *players)
        {
            SessionPlayers::Player player;

            if (SessionPlayers::playerFrom (name.toString(), player)
                && std::find (request.players.begin(), request.players.end(), player) == request.players.end())
                request.players.push_back (player);
        }
    }

    // Each player's instrument as chosen: {file, id}, or "none" for the Test Synth; not given: the best fit.
    const auto instruments = event.getProperty ("instruments", {});

    for (const auto player : request.players)
    {
        const auto chosen = instruments.getProperty (SessionPlayers::nameOf (player), {});

        if (chosen.toString() == "none")
            request.instruments[player] = { "none", {} };
        else if (const auto file = chosen.getProperty ("file", {}).toString(); file.isNotEmpty())
            request.instruments[player] = { file, chosen.getProperty ("id", {}).toString() };
    }

    // Backing a recording with a tempo of its own: the song takes its tempo and its audio moves
    // onto the bars, in the same undo step as the players. The chords must read first.
    bool joined = false;

    if (const auto alignTrack = static_cast<int> (event.getProperty ("alignTrack", 0)); alignTrack != 0)
    {
        if (alignTrack != heardTrack || ! heard.ok || heard.followsSong || RecordingAnalysis::layoutOf (engine) != heardLayout)
            return answer (juce::Result::fail ("The recording or the song changed since Sonify listened to it: choose the track in From Track again."));

        if (request.players.empty())
            return answer (juce::Result::fail ("Choose at least one player."));

        const auto state = engine.getTransport().getState();
        std::vector<SessionPlayers::Chord> readable;

        if (const auto parsed = SessionPlayers::parseProgression (request.chords, 0.0, std::max (1, state.numerator) * 4.0 / std::max (1, state.denominator),
                                                                  std::max (1, state.numerator), request.times, readable); parsed.failed())
            return answer (parsed);

        std::vector<int> audioTracks;

        for (const auto& track : engine.getTracks())
            if (track->kind == Track::Kind::audio)
                audioTracks.push_back (track->id);

        auto before = Edits::snapshot (engine, audioTracks);
        const auto mapBefore = engine.getTempoMap();
        RecordingAnalysis::align (engine, heardAlignment);
        const auto now = engine.getTransport().getState();
        notes.add ("The song is at " + juce::String (now.bpm, 2) + " BPM now, the recording's tempo, with the recordings on its bars.");

        addToHistory ("Add Session Players", new Edits::TempoChanged (engine, mapBefore, engine.getTempoMap()));
        pushSongChannel();
        addToHistory ("Add Session Players", new Edits::ClipsChanged (engine, std::move (before), Edits::snapshot (engine, audioTracks)), true);
        joined = true;

        // Heard against the song as it was.
        heardTrack = 0;
        heard = {};
    }

    const auto result = SessionPlayers::create (engine, library, request, trackIds, notes);

    // One undo step for them all; undo keeps each track whole (instrument, part and all) for redo.
    for (const auto id : trackIds)
    {
        const auto& tracks = engine.getTracks();
        const auto found = std::find_if (tracks.begin(), tracks.end(), [id] (const auto& track) { return track->id == id; });

        if (found == tracks.end())
            continue;

        addToHistory ("Add Session Players", new Edits::TrackAdded (engine, id, static_cast<int> (found - tracks.begin())), joined);
        joined = true;
    }

    if (! trackIds.empty())
        pushLibrary();   // the instruments just used, for the Browser's Recent folder

    pushTracks();
    pushStatus();        // the tempo, when the song took the recording's
    answer (result);
}

void MainComponent::updateRecordingAnalysis()
{
    if (recordingAnalysis == nullptr)
        return;

    if (! recordingAnalysis->isDone())
    {
        // Often enough for its progress bar.
        if (const auto progress = recordingAnalysis->getProgress(); progress >= heardProgressSent + 0.02)
        {
            heardProgressSent = progress;
            emitHeard (true, progress);
        }

        return;
    }

    heard = recordingAnalysis->getResult();
    recordingAnalysis.reset();

    // The song changed while it listened: its audio moved, another tempo, another song.
    if (heard.ok && RecordingAnalysis::layoutOf (engine) != heardLayout)
    {
        heard = {};
        heard.error = "The song changed while Sonify listened: choose the track again.";
    }

    if (heard.ok && ! heard.followsSong)
        heardAlignment = RecordingAnalysis::alignmentFor (heard, engine);

    emitHeard (false, 1.0);
}

void MainComponent::cancelRecordingAnalysis()
{
    if (recordingAnalysis != nullptr)
    {
        recordingAnalysis->cancel();
        recordingAnalysis.reset();   // a moment, while it stops
    }
}

void MainComponent::emitHeard (bool listening, double progress)
{
    if (webView == nullptr)
        return;

    juce::DynamicObject::Ptr reply (new juce::DynamicObject());
    reply->setProperty ("trackId", heardTrack);
    reply->setProperty ("audio", true);
    reply->setProperty ("listening", listening);
    reply->setProperty ("progress", progress);
    reply->setProperty ("songTempo", engine.getTransport().getState().bpm);

    if (! listening)
    {
        reply->setProperty ("error", heard.ok ? juce::String() : heard.error);
        reply->setProperty ("chords", heard.ok ? heard.chords : juce::String());
        reply->setProperty ("bars", heard.ok ? heard.bars : 0);
        reply->setProperty ("tempo", heard.ok ? (heard.followsSong ? heard.bpm : heardAlignment.bpm) : 0.0);
        reply->setProperty ("follows", heard.ok && heard.followsSong);
        reply->setProperty ("steadiness", heard.steadiness);
        reply->setProperty ("startBar", ! heard.ok ? 1 : heard.followsSong ? heard.firstBar : heardAlignment.startBar);
    }

    webView->emitEventIfBrowserIsVisible ("sessionChords", juce::var (reply.get()));
}

//==============================================================================
// Plugins' parameters for the page. A plugin's list (with each value's text, which asks the
// plugin) is made again only when its parameters or values changed; pushTracks sends each list
// once, in a "pluginParams" event, and the track state names it by key, so a click or an edit
// doesn't send every plugin's parameters again. Message thread.
namespace
{
    struct ParamsEntry
    {
        std::uint64_t listSignature = 0, fullSignature = 0;
        int id = 0, listVersion = 0, version = 0;
        juce::var params;
        bool used = false;

        juce::String key() const        { return "p" + juce::String (id) + "." + juce::String (version); }
        juce::String listKey() const    { return "p" + juce::String (id) + "l" + juce::String (listVersion); }
    };

    struct PluginParams
    {
        std::map<const Plugin*, ParamsEntry> entries;
        std::set<juce::String> sent;                 // the lists the page has
        juce::DynamicObject::Ptr outgoing;           // the lists this push sends (while pushing)
        int nextId = 1;
        bool byKey = false;                          // pushTracks: lists by key, not inline
    };

    PluginParams& pluginParams()
    {
        static PluginParams params;
        return params;
    }

    std::uint64_t mixIn (std::uint64_t hash, std::uint64_t value) noexcept
    {
        hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
        return hash;
    }

    std::uint64_t bitsOf (double value) noexcept
    {
        std::uint64_t bits = 0;
        std::memcpy (&bits, &value, sizeof (bits));
        return bits;
    }

    /** The plugin's parameter list, made again only when it changed. */
    ParamsEntry& paramsEntryFor (Plugin& plugin)
    {
        auto& all = pluginParams();
        const auto& parameters = plugin.getParameters();

        // What the list is (its parameters, by id and name), and what it shows (with the values).
        auto list = mixIn (static_cast<std::uint64_t> (plugin.getDescriptor().id.hashCode64()), parameters.size());

        for (const auto& parameter : parameters)
            list = mixIn (mixIn (list, parameter.id), static_cast<std::uint64_t> (parameter.name.hashCode64()) ^ (parameter.hidden ? 1u : 0u));

        auto full = list;

        for (size_t i = 0; i < parameters.size(); ++i)
            if (! parameters[i].hidden)
                full = mixIn (full, bitsOf (plugin.getParameterValue (static_cast<int> (i))));

        auto& entry = all.entries[&plugin];
        entry.used = true;

        if (entry.id != 0 && entry.fullSignature == full && ! entry.params.isVoid())
            return entry;

        if (entry.id == 0)
            entry.id = all.nextId++;

        if (entry.listSignature != list || entry.listVersion == 0)
            ++entry.listVersion;

        ++entry.version;
        entry.listSignature = list;
        entry.fullSignature = full;

        juce::Array<juce::var> params;
        params.ensureStorageAllocated (static_cast<int> (parameters.size()));

        for (size_t i = 0; i < parameters.size(); ++i)
        {
            const auto& parameter = parameters[i];

            if (parameter.hidden)
                continue;

            const auto index = static_cast<int> (i);
            const auto value = plugin.getParameterValue (index);

            juce::DynamicObject::Ptr item (new juce::DynamicObject());
            item->setProperty ("index", index);
            item->setProperty ("id", static_cast<juce::int64> (parameter.id));
            item->setProperty ("name", parameter.name);
            item->setProperty ("min", parameter.minValue);
            item->setProperty ("max", parameter.maxValue);
            item->setProperty ("value", value);
            item->setProperty ("text", plugin.getParameterText (index, value));
            item->setProperty ("stepped", parameter.stepped);
            item->setProperty ("readOnly", parameter.readOnly);
            params.add (juce::var (item.get()));
        }

        entry.params = params;
        return entry;
    }

    /** A plugin's parameters onto its state: inline, or (pushTracks) by key, the list queued to go. */
    void putParams (Plugin& plugin, juce::DynamicObject& state)
    {
        auto& all = pluginParams();
        const auto& entry = paramsEntryFor (plugin);

        if (! all.byKey || all.outgoing == nullptr)
        {
            state.setProperty ("params", entry.params);
            return;
        }

        const auto key = entry.key();
        state.setProperty ("paramsKey", key);
        state.setProperty ("paramsList", entry.listKey());   // changes only when the list itself does

        if (all.sent.count (key) == 0)
            all.outgoing->setProperty (key, entry.params);
    }
}

//==============================================================================
// Event > Split into Stems
namespace
{
    /** A track muted or unmuted along with an edit, in its undo step (muting alone is mixing). */
    class TrackMuted final : public Edits::Action
    {
    public:
        TrackMuted (AudioEngine& e, int trackId, bool beforeChange, bool afterChange)
            : engine (e), id (trackId), before (beforeChange), after (afterChange) {}

    private:
        void redo() override     { set (after); }
        void revert() override   { set (before); }

        void set (bool mute)
        {
            if (auto* track = engine.findTrack (id))
                track->mute.store (mute);
        }

        AudioEngine& engine;
        int id;
        bool before, after;
    };
}

juce::String MainComponent::splitStems (int trackId, int clipId, const juce::StringArray& names, bool muteOriginal)
{
    if (stemStage != StemStage::idle)
        return "Stems are being split already: wait for those, or cancel them.";

    std::vector<StemSplitter::Stem> stems;

    for (const auto& name : names)
        if (StemSplitter::Stem stem; StemSplitter::stemFrom (name, stem) && std::find (stems.begin(), stems.end(), stem) == stems.end())
            stems.push_back (stem);

    if (stems.empty())
        return "Choose at least one stem: vocals, drums, bass, guitar, piano or other.";

    if (engine.isRecording() || engine.isTakeActive())
        return "Not while recording.";

    if (const auto problem = StemSplitter::runtimeProblem(); problem.isNotEmpty())
        return problem;

    const auto* track = engine.findTrack (trackId);
    const MidiClip* clip = nullptr;

    if (track != nullptr && track->kind == Track::Kind::audio)
        for (const auto& c : track->clips)
            if (c.id == clipId && c.isAudio())
                clip = &c;

    if (clip == nullptr)
        return "Select an audio clip to split into stems.";

    if (! juce::File (clip->audioFile).existsAsFile())
        return "The clip's recording is missing: " + juce::File (clip->audioFile).getFileName();

    if (clip->audioLength < 0.5)
        return "The clip is too short to split.";

    stemJob = {};
    stemJob.source = juce::File (clip->audioFile);
    stemJob.offsetSeconds = clip->audioOffset;
    stemJob.lengthSeconds = clip->audioLength;
    stemJob.stems = stems;
    stemJob.model = StemSplitter::modelFor (stems);   // the six-stem one for guitar or piano
    stemJob.folder = engine.getRecordingFolder() != juce::File() ? engine.getRecordingFolder() : UserData::recordings();
    stemJob.name = track->name;
    stemTrack = trackId;
    stemClip = clipId;
    stemStart = clip->start;
    stemGain = clip->audioGainDb;
    stemFadeIn = clip->fadeIn;
    stemFadeOut = clip->fadeOut;
    stemMute = muteOriginal;
    stemGeneration = songGeneration;

    // The first time: the model, downloaded once.
    if (! StemSplitter::hasModel (stemJob.model))
    {
        if (internetBlocked)
            return "Splitting " + juce::String (stemJob.model == StemSplitter::Model::sixStems ? "guitar and piano" : "stems") + " needs its model, downloaded once ("
                   + juce::String (StemSplitter::downloadMegabytes (stemJob.model)) + " MB), and the internet is blocked in Settings.";

        modelDownload = std::make_unique<StemSplitter::Download> (stemJob.model);
        modelDownload->start();
        stemStage = StemStage::downloading;
        stemProgressSent = 0.0;
        emitStems ("downloading", 0.0);
        return {};
    }

    startStemSplitter();
    return {};
}

void MainComponent::startStemSplitter()
{
    stemSplitter = std::make_unique<StemSplitter> (stemJob);
    stemSplitter->start();
    stemStage = StemStage::splitting;
    stemProgressSent = 0.0;
    emitStems ("splitting", 0.0);
}

void MainComponent::cancelStems()
{
    if (modelDownload != nullptr)
        modelDownload->cancel();

    if (stemSplitter != nullptr)
        stemSplitter->cancel();
}

juce::String MainComponent::stemStateName() const
{
    return stemStage == StemStage::downloading ? "downloading" : stemStage == StemStage::splitting ? "splitting" : "idle";
}

double MainComponent::stemProgress() const
{
    if (stemStage == StemStage::downloading && modelDownload != nullptr)
        return modelDownload->getProgress();

    if (stemStage == StemStage::splitting && stemSplitter != nullptr)
        return stemSplitter->getProgress();

    return 0.0;
}

void MainComponent::updateStems()
{
    if (stemStage == StemStage::idle)
        return;

    const auto progress = stemProgress();
    const auto working = stemStage == StemStage::downloading ? modelDownload != nullptr && ! modelDownload->isDone()
                                                             : stemSplitter != nullptr && ! stemSplitter->isDone();

    // Another song opened meanwhile: these stems were for the one before (the model still comes).
    const auto stale = stemGeneration != songGeneration;

    if (working)
    {
        if (stale && stemStage == StemStage::splitting)
            cancelStems();

        if (progress >= stemProgressSent + 0.01)
        {
            stemProgressSent = progress;
            emitStems (stemStateName(), progress);
        }

        return;
    }

    // An AI request makes one undo step of what it does: the stems wait for it to end.
    if (stemStage == StemStage::splitting && aiSongBefore.isNotEmpty() && ! stale)
        return;

    if (stemStage == StemStage::downloading)
    {
        const auto result = modelDownload != nullptr ? modelDownload->getResult() : juce::Result::fail ("Cancelled");
        modelDownload.reset();
        stemStage = StemStage::idle;

        if (result.failed())
        {
            const auto cancelled = result.getErrorMessage() == "Cancelled";
            return emitStems (cancelled ? "cancelled" : "failed", 0.0, cancelled ? juce::String() : result.getErrorMessage());
        }

        if (stale)
            return emitStems ("cancelled");

        return startStemSplitter();
    }

    const auto result = stemSplitter->getResult();
    stemSplitter.reset();
    stemStage = StemStage::idle;

    if (! result.ok)
    {
        const auto cancelled = result.error == "Cancelled";
        return emitStems (cancelled ? "cancelled" : "failed", 0.0, cancelled ? juce::String() : result.error);
    }

    if (stale)
    {
        for (const auto& [stem, file] : result.files)
            file.deleteFile();

        return emitStems ("cancelled");
    }

    finishStems (result);
}

void MainComponent::finishStems (const StemSplitter::Result& result)
{
    // Under the clip's track (where it is now), each on a track of its own, where the clip starts now.
    int index = static_cast<int> (engine.getTracks().size());

    for (size_t i = 0; i < engine.getTracks().size(); ++i)
        if (engine.getTracks()[i]->id == stemTrack)
            index = static_cast<int> (i) + 1;

    if (const auto* source = engine.findTrack (stemTrack))
        for (const auto& clip : source->clips)
            if (clip.id == stemClip)
                stemStart = clip.start;

    juce::Array<juce::var> ids;
    bool joined = false;

    for (const auto stem : stemJob.stems)
    {
        const auto file = result.files.find (stem);

        if (file == result.files.end())
            continue;

        const auto id = engine.addTrack (Track::Kind::audio);
        engine.setTrackName (id, stemJob.name + " " + StemSplitter::nameOf (stem));
        int at = 0;

        if (auto moved = engine.detachTrack (id, at))
            engine.attachTrack (std::move (moved), index);

        MidiClip clip;
        clip.start = stemStart;
        clip.audioFile = file->second.getFullPathName();
        clip.audioOffset = 0.0;
        clip.audioLength = result.lengthSeconds;
        clip.audioGainDb = stemGain;
        clip.fadeIn = stemFadeIn;
        clip.fadeOut = stemFadeOut;
        engine.addClip (id, clip);

        addToHistory ("Split into Stems", new Edits::TrackAdded (engine, id, index), joined);
        joined = true;
        ids.add (id);
        ++index;
    }

    // The original, muted in the same step: undo brings it back as it was.
    if (stemMute && joined)
    {
        if (auto* track = engine.findTrack (stemTrack); track != nullptr && ! track->mute.load())
        {
            track->mute.store (true);
            addToHistory ("Split into Stems", new TrackMuted (engine, stemTrack, false, true), true);
        }
    }

    pushTracks();
    emitStems ("done", 1.0, {}, ids);
}

void MainComponent::emitStems (const juce::String& state, double progress, const juce::String& error, const juce::Array<juce::var>& trackIds)
{
    if (webView == nullptr)
        return;

    juce::DynamicObject::Ptr item (new juce::DynamicObject());
    item->setProperty ("state", state);
    item->setProperty ("progress", progress);
    item->setProperty ("error", error);
    item->setProperty ("modelReady", StemSplitter::hasModel (StemSplitter::Model::fourStems));
    item->setProperty ("sixReady", StemSplitter::hasModel (StemSplitter::Model::sixStems));
    item->setProperty ("downloadMb", StemSplitter::downloadMegabytes (stemJob.model));
    item->setProperty ("problem", StemSplitter::runtimeProblem());
    item->setProperty ("trackIds", trackIds);
    item->setProperty ("trackId", stemTrack);
    item->setProperty ("clipId", stemClip);

    juce::Array<juce::var> stems;

    for (const auto stem : stemJob.stems)
        stems.add (StemSplitter::nameOf (stem).toLowerCase());

    item->setProperty ("stems", stems);
    webView->emitEventIfBrowserIsVisible ("stems", juce::var (item.get()));
}

//==============================================================================
// The Browser's Files tab, previews and importing audio
juce::var MainComponent::withFilesFolder (juce::var song) const
{
    if (auto* object = song.getDynamicObject(); object != nullptr && songFilesFolder.isNotEmpty())
        object->setProperty ("filesFolder", songFilesFolder);

    return song;
}

void MainComponent::showSongFilesFolder()
{
    // The song's own; a song without one (or whose folder is gone), and a new song: the one for
    // new songs; neither: the tab stays where it is.
    for (const auto& candidate : { songFilesFolder, prefs.filesFolder })
    {
        if (candidate.isNotEmpty() && juce::File::isAbsolutePath (candidate) && juce::File (candidate).isDirectory())
        {
            listFolder (candidate);
            return;
        }
    }
}

void MainComponent::chooseFilesFolder()
{
    juce::File start (songFilesFolder.isNotEmpty() ? songFilesFolder : prefs.filesFolder.isNotEmpty() ? prefs.filesFolder : prefs.browserFolder);

    if (! juce::File::isAbsolutePath (start.getFullPathName()) || ! start.isDirectory())
        start = juce::File::getSpecialLocation (juce::File::userMusicDirectory);

    filesFolderChooser = std::make_unique<juce::FileChooser> ("Choose your files folder", start);
    filesFolderChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                     [safeThis = juce::Component::SafePointer<MainComponent> (this)] (const juce::FileChooser& chooser)
    {
        const auto chosen = chooser.getResult();

        if (safeThis == nullptr || ! chosen.isDirectory())
            return;

        const auto path = chosen.getFullPathName();

        // The folder for new songs from now on...
        safeThis->prefs.filesFolder = path;
        safeThis->savePreferences();

        // ...and this song's own, kept with it (a song is open: it's a change to save).
        if (! safeThis->noProject && safeThis->songFilesFolder != path)
        {
            safeThis->songFilesFolder = path;
            safeThis->markChanged();
        }

        safeThis->listFolder (path);
    });
}

void MainComponent::listFolder (const juce::String& path)
{
    // The Files tab: where it was last (also from the last time Sonify ran) when it asks for none.
    auto listing = folderListing (path.isNotEmpty() ? path : prefs.browserFolder, false);
    const auto shown = listing.getProperty ("path", {}).toString();

    if (shown.isNotEmpty() && shown != prefs.browserFolder)
    {
        prefs.browserFolder = shown;
        savePreferences();
    }

    if (webView != nullptr)
        webView->emitEventIfBrowserIsVisible ("folderListing", listing);
}

juce::var MainComponent::folderListing (const juce::String& path, bool audioOnly)
{
    juce::File folder (path);

    if (! folder.isDirectory())
        folder = juce::File::getSpecialLocation (juce::File::userMusicDirectory);

    if (! folder.isDirectory())
        folder = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

    std::vector<juce::File> folders, files;

    for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "*",
                                                           juce::File::findFilesAndDirectories | juce::File::ignoreHiddenFiles))
    {
        if (entry.isDirectory())
            folders.push_back (entry.getFile());
        else if (engine.getMedia().isAudioFile (entry.getFile())
                 || (! audioOnly && (VideoMedia::isVideoFile (entry.getFile()) || entry.getFile().hasFileExtension ("mid;midi;smf"))))
            files.push_back (entry.getFile());
    }

    const auto byName = [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; };
    std::sort (folders.begin(), folders.end(), byName);
    std::sort (files.begin(), files.end(), byName);

    const auto describe = [] (const juce::File& file, bool withSize)
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("name", file.getFileName());
        item->setProperty ("path", file.getFullPathName());

        if (withSize)
        {
            item->setProperty ("size", static_cast<double> (file.getSize()));

            if (VideoMedia::isVideoFile (file))
                item->setProperty ("video", true);

            if (file.hasFileExtension ("mid;midi;smf"))
                item->setProperty ("midi", true);
        }

        return juce::var (item.get());
    };

    juce::Array<juce::var> folderList, fileList, places;

    for (const auto& f : folders)
        folderList.add (describe (f, false));

    for (const auto& f : files)
        fileList.add (describe (f, true));

    // Places, as in Studio One's file browser: the usual folders, the song's media, the drives.
    const auto addPlace = [&places] (const juce::String& name, const juce::File& place)
    {
        if (! place.isDirectory())
            return;

        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("name", name);
        item->setProperty ("path", place.getFullPathName());
        places.add (juce::var (item.get()));
    };

    const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    addPlace ("Home", home);
    addPlace ("Desktop", juce::File::getSpecialLocation (juce::File::userDesktopDirectory));
    addPlace ("Documents", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));
    addPlace ("Music", juce::File::getSpecialLocation (juce::File::userMusicDirectory));
    addPlace ("Downloads", home.getChildFile ("Downloads"));

    if (songFile != juce::File())
        addPlace ("Song Media", songFile.getParentDirectory().getChildFile ("Media"));

    addPlace ("Songs", UserData::songs());
    addPlace ("Recordings", UserData::recordings());
    addPlace ("Fanan Content", contentFolder());

    juce::Array<juce::File> roots;
    juce::File::findFileSystemRoots (roots);

    for (const auto& root : roots)
        addPlace (root.getFullPathName(), root);

    const auto parent = folder.getParentDirectory();

    juce::DynamicObject::Ptr listing (new juce::DynamicObject());
    listing->setProperty ("path", folder.getFullPathName());
    listing->setProperty ("parent", parent != folder ? parent.getFullPathName() : juce::String());
    listing->setProperty ("folders", folderList);
    listing->setProperty ("files", fileList);
    listing->setProperty ("places", places);
    return juce::var (listing.get());
}

void MainComponent::sendAudioFileInfo (const juce::String& path)
{
    if (webView == nullptr)
        return;

    std::unique_ptr<juce::AudioFormatReader> reader (engine.getMedia().getFormats().createReaderFor (juce::File (path)));

    juce::DynamicObject::Ptr info (new juce::DynamicObject());
    info->setProperty ("path", path);

    if (reader != nullptr && reader->sampleRate > 0.0)
    {
        info->setProperty ("rate", reader->sampleRate);
        info->setProperty ("channels", static_cast<int> (reader->numChannels));
        info->setProperty ("bits", static_cast<int> (reader->bitsPerSample));
        info->setProperty ("float", reader->usesFloatingPointData);
        info->setProperty ("length", static_cast<double> (reader->lengthInSamples) / reader->sampleRate);
        info->setProperty ("format", reader->getFormatName());
    }

    webView->emitEventIfBrowserIsVisible ("audioFileInfo", juce::var (info.get()));
}

void MainComponent::pushFinePeaks (const juce::String& path)
{
    if (webView == nullptr)
        return;

    juce::String error;
    const auto data = engine.getMedia().get (juce::File (path), engine.getPlaybackRate(), error);

    if (data == nullptr)
        return;

    constexpr int fineBucket = 64;
    const auto fine = AudioMedia::peaksOf (*data, fineBucket);
    juce::Array<juce::var> peaks;
    peaks.ensureStorageAllocated (static_cast<int> (fine.size()));

    for (const auto value : fine)
        peaks.add (static_cast<int> (value));

    juce::DynamicObject::Ptr item (new juce::DynamicObject());
    item->setProperty ("file", path);
    item->setProperty ("rate", data->playbackRate);
    item->setProperty ("bucket", fineBucket);
    item->setProperty ("peaks", peaks);
    webView->emitEventIfBrowserIsVisible ("audioPeaksFine", juce::var (item.get()));
}

bool MainComponent::importMidiFile (const juce::File& file, int trackId, double start, bool joinPreviousStep)
{
    juce::FileInputStream input (file);
    juce::MidiFile midi;

    if (! input.openedOk() || ! midi.readFrom (input) || midi.getTimeFormat() <= 0)
    {
        pushTracks ("Couldn't read " + file.getFileName() + " as a MIDI file.");
        return false;
    }

    // Every track's notes, in quarter notes from the file's start.
    const auto ticks = static_cast<double> (midi.getTimeFormat());
    MidiClip clip;

    for (int t = 0; t < midi.getNumTracks(); ++t)
    {
        juce::MidiMessageSequence sequence (*midi.getTrack (t));
        sequence.updateMatchedPairs();

        for (int i = 0; i < sequence.getNumEvents(); ++i)
        {
            const auto* event = sequence.getEventPointer (i);

            if (! event->message.isNoteOn())
                continue;

            const auto on = event->message.getTimeStamp() / ticks;
            const auto off = event->noteOffObject != nullptr ? event->noteOffObject->message.getTimeStamp() / ticks : on + 0.25;
            MidiNote note;
            note.pitch = event->message.getNoteNumber();
            note.start = on;
            note.length = juce::jmax (1.0 / 64.0, off - on);
            note.velocity = juce::jlimit (1.0f / 127.0f, 1.0f, event->message.getFloatVelocity());
            clip.notes.push_back (note);
        }
    }

    if (clip.notes.empty())
    {
        pushTracks (file.getFileName() + " has no notes.");
        return false;
    }

    std::sort (clip.notes.begin(), clip.notes.end(), [] (const MidiNote& a, const MidiNote& b) { return a.start < b.start; });

    // Whole bars long, from where it's dropped.
    double end = 0.0;

    for (const auto& note : clip.notes)
        end = juce::jmax (end, note.start + note.length);

    const auto& map = engine.getTempoMap();
    const auto lastBar = map.barAt (start + end - 1.0e-9);
    clip.start = start;
    clip.offset = 0.0;
    clip.length = juce::jmax (end, lastBar.start + lastBar.length() - start);

    // Onto an instrument track, or a new one named after the file.
    auto* target = engine.findTrack (trackId);

    if (target == nullptr || target->kind != Track::Kind::instrument)
    {
        trackId = engine.addTrack (Track::Kind::instrument);
        engine.setTrackName (trackId, file.getFileNameWithoutExtension());
        addToHistory ("Import MIDI", new Edits::TrackAdded (engine, trackId, static_cast<int> (engine.getTracks().size()) - 1), joinPreviousStep);
        joinPreviousStep = true;
    }

    auto before = Edits::snapshot (engine, { trackId });

    if (engine.addClip (trackId, std::move (clip)) == 0)
        return false;

    addToHistory ("Import MIDI", new Edits::ClipsChanged (engine, std::move (before), Edits::snapshot (engine, { trackId })), joinPreviousStep);
    return true;
}

void MainComponent::importAudioFile (const juce::File& file, int trackId, double start, bool joinPreviousStep)
{
    // A MIDI file: its notes, as a clip on an instrument track.
    if (file.hasFileExtension ("mid;midi;smf"))
    {
        importMidiFile (file, trackId, start, joinPreviousStep);
        return;
    }

    // A video dropped in the arrangement (or onto a track) becomes the song's video there.
    if (VideoMedia::isVideoFile (file))
    {
        importVideo (file, start);
        return;
    }

    juce::String error;

    if (! engine.getMedia().isAudioFile (file) || engine.getMedia().get (file, engine.getPlaybackRate(), error) == nullptr)
    {
        pushTracks ("Couldn't read " + file.getFileName() + " as audio.");
        return;
    }

    // Onto an audio track, or onto a new one named after the file, as in Studio One.
    auto* target = engine.findTrack (trackId);

    if (target == nullptr || target->kind != Track::Kind::audio)
    {
        trackId = engine.addTrack (Track::Kind::audio);
        engine.setTrackName (trackId, file.getFileNameWithoutExtension());
        addToHistory ("Import Audio", new Edits::TrackAdded (engine, trackId, static_cast<int> (engine.getTracks().size()) - 1), joinPreviousStep);
        joinPreviousStep = true;
    }

    auto before = Edits::snapshot (engine, { trackId });

    if (engine.importAudio (trackId, file, start) > 0)
        addToHistory ("Import Audio", new Edits::ClipsChanged (engine, std::move (before), Edits::snapshot (engine, { trackId })), joinPreviousStep);
}

void MainComponent::chooseAudioToImport()
{
    // File > Import Media Files...: at the playhead, the first file onto the selected audio track,
    // the others onto new tracks under it (MainImport.cpp).
    const auto* selected = engine.findTrack (engine.getSelectedTrackId());
    MediaPlace place;
    place.trackId = selected != nullptr && selected->kind == Track::Kind::audio ? selected->id : 0;
    place.after = selected != nullptr ? selected->id : 0;
    place.start = engine.getTransport().getState().ppq;
    chooseMedia (place);
}

void MainComponent::updateRecordingFolder()
{
    // Like Studio One: recordings go into the song's Media folder. An unsaved song records
    // into the user data folder's Recordings (the engine's default).
    engine.setRecordingFolder (songFile != juce::File() ? songFile.getParentDirectory().getChildFile ("Media") : juce::File());
}

void MainComponent::pushAudioPeaks (const juce::String& path)
{
    if (webView == nullptr)
        return;

    juce::String error;
    const auto data = engine.getMedia().get (juce::File (path), engine.getPlaybackRate(), error);

    if (data == nullptr)
        return;

    juce::Array<juce::var> peaks;
    peaks.ensureStorageAllocated (static_cast<int> (data->peaks.size()));

    for (const auto value : data->peaks)
        peaks.add (static_cast<int> (value));

    juce::DynamicObject::Ptr item (new juce::DynamicObject());
    item->setProperty ("file", path);
    item->setProperty ("rate", data->fileRate);
    item->setProperty ("bucket", AudioData::peakBucket);
    item->setProperty ("length", data->lengthSeconds);
    item->setProperty ("peaks", peaks);
    webView->emitEventIfBrowserIsVisible ("audioPeaks", juce::var (item.get()));
}

void MainComponent::handleUiReady()
{
    // The page says when it has drawn itself (uiShown), and the splash goes; if it never does, the
    // splash goes anyway in a moment.
    dismissSplash (3000);

    // A page loaded afresh has none of the plugins' parameter lists.
    pluginParams().sent.clear();

    // The page is loaded and listening, so updates will no longer be dropped.
    pushInternetBlocked();
    pushAi();
    pushDeviceStudio();
    pushVoice();
    pushRemote();
    restoreWebTabs();
    pushAccount();
    pushLibrary();
    pushSettings();
    emitMidiFxTypes();
    listMidiTools();

    // At startup the page asks first whether to load third-party plugins, and the scan waits
    // for the answer (choosePlugins). A page loading again later keeps the answer: it isn't
    // asked again, and gets only what changed, as any rescan.
    if (pluginChoiceMade)
    {
        juce::DynamicObject::Ptr choice (new juce::DynamicObject());
        choice->setProperty ("thirdParty", library.offersThirdParty());

        if (webView != nullptr)
            webView->emitEventIfBrowserIsVisible ("pluginChoice", juce::var (choice.get()));

        if (library.offersThirdParty())
            startLibraryScan (false);
        else
            emitScan ("done");
    }

    pushSongChannel (true);
    pushSongBlocks (true);
    pushSongVideo (true);
    pushTracks();

    // Waveforms for the audio already in the song (a reloaded page has none).
    std::set<juce::String> audioFiles;

    for (const auto& track : engine.getTracks())
        for (const auto& clip : track->clips)
            if (clip.isAudio())
                audioFiles.insert (clip.audioFile);

    for (const auto& path : audioFiles)
        pushAudioPeaks (path);
    pushStatus();
    pushSong();
    startTimerHz (20);
    automationTimer.startTimerHz (60);   // recording automation (MainAutomation.cpp)
}

void MainComponent::updateSnapshots()
{
    if (snapshotQueue.isEmpty())
        return;

    // Drawing one loads and draws a whole plugin on this thread: one every couple of seconds at
    // most, never while a mouse button is down (a drag, a click being made) or a take records.
    const auto now = juce::Time::getMillisecondCounterHiRes();

    if (now - snapshotDrawnMs < 2000.0 || juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown() || engine.isRecording())
        return;

    snapshotDrawnMs = now;

    const juce::File folder (snapshotQueue[0]);
    snapshotQueue.remove (0);

    // Tried once for each time it changes: one that can't be drawn isn't tried again and again.
    if (folder.exists() && snapshotTried.insert (snapshotKey (folder)).second)
        MadePlugins::makeSnapshot (folder);

    // All drawn: the Browser shows them (pushLibrary queues only what's due and not tried yet).
    if (snapshotQueue.isEmpty())
        pushLibrary();
}

void MainComponent::timerCallback()
{
    updateScan();
    updateSnapshots();

    juce::String message;
    const auto pluginsChanged = engine.updatePlugins (message) != AudioEngine::PluginChange::none;

    // A recording take becomes one undo step: the armed tracks' clips before it and after.
    if (! takeRunning && ! engine.isTakeActive() && engine.getTransport().getState().recording)
    {
        std::vector<int> armed;

        for (const auto& track : engine.getTracks())
            if (track->recordArm.load())
                armed.push_back (track->id);

        takeBefore = Edits::snapshot (engine, armed);
    }

    const auto clipsChanged = engine.updateRecording();
    forwardScreenMessages();
    updateMixdown();
    updateVideo();
    updateRecordingAnalysis();
    updateStems();
    updateAudioJobs();
    updateMagenta();
    updateLauncher();
    updatePadGrids();

    // Audio loaded since (a finished recording, an opened song): the page needs its waveform
    // (a frozen track's render is drawn nowhere).
    for (const auto& path : engine.getMedia().takeNewlyLoaded())
        if (! engine.isFrozenRender (path))
            pushAudioPeaks (path);

    // Frozen tracks a change thawed (a sidechain key's fader too): they play themselves again.
    engine.checkFrozenKeys();

    if (const auto thawed = engine.takeThawed(); ! thawed.empty())
    {
        juce::StringArray names;

        for (const auto id : thawed)
            if (const auto* track = engine.findTrack (id))
                names.addIfNotAlreadyThere ("\"" + track->name + "\"");

        if (names.isEmpty())
        {
            pushTracks();
        }
        else
        {
            markChanged();
            pushTracks (names.size() == 1 ? names[0] + " isn't frozen any more: what it plays, or how, changed. Its snowflake freezes it again."
                                          : names.joinIntoString (", ") + " aren't frozen any more: what they play, or how, changed. Their snowflakes freeze them again.");
        }
    }

    if (engine.isTakeActive())
    {
        takeRunning = true;
    }
    else if (takeRunning && ! engine.isRecording())
    {
        takeRunning = false;
        takeEnded = true;      // the whole song goes to the page now, as the take is done
        takeClipsDue = false;
        std::vector<int> ids;

        for (const auto& [id, clips] : takeBefore)
            ids.push_back (id);

        auto after = Edits::snapshot (engine, ids);

        if (! Edits::same (takeBefore, after))
            addToHistory ("Record", new Edits::ClipsChanged (engine, std::move (takeBefore), std::move (after)));

        takeBefore.clear();
    }

    if (clipsChanged || engine.consumePluginStateChanges())
        markChanged();

    // A control surface's mutes, solos, arms, selections and sends: the page hears them a few
    // times a second at most (a send's knob turning would otherwise send the whole song each
    // tick). A send's knob at rest for a moment: its turn is one undo step.
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();

    if (surfaceSendEditing && nowMs - surfaceSendChangedMs > 700.0)
        commitSurfaceSendEdit();

    const auto surfaceChanges = surfaceTracksDue && nowMs - surfaceTracksPushedMs >= 250.0;

    // A container's name or nodes changed: the page shows what's in it.
    const auto containerChanges = std::exchange (containersChanged, false);

    // Only a take's clips growing: they alone go, a few times a second.
    const auto recordingTake = engine.isRecording() && engine.isTakeActive();
    auto clipsToPush = clipsChanged;

    if (clipsChanged && recordingTake && ! (pluginsChanged || surfaceChanges || containerChanges || takeEnded))
    {
        takeClipsDue = true;
        clipsToPush = false;
    }

    if (pluginsChanged || clipsToPush || surfaceChanges || containerChanges || std::exchange (takeEnded, false))
    {
        pushTracks (message);
        surfaceTracksDue = false;
        surfaceTracksPushedMs = nowMs;
        takeClipsDue = false;
    }
    else if (takeClipsDue && nowMs - takeClipsPushedMs >= 150.0)
    {
        pushTakeClips();
        takeClipsPushedMs = nowMs;
    }

    pushTakeWaves (nowMs);

    followTransport();
    followForeground();
    watchAudioDevice();
    autosaveIfDue();
    updateMidiLink();
    pushStatus();
}

//==============================================================================
void MainComponent::pushStatus()
{
    if (webView == nullptr)
        return;

    const auto status = engine.getStatus();
    pushSongChannel();   // the AI, a recording's tempo or an undo may have changed it

    juce::DynamicObject::Ptr object (new juce::DynamicObject());
    object->setProperty ("deviceOpen", status.deviceOpen);
    object->setProperty ("running", status.running);
    object->setProperty ("typeName", status.typeName);
    object->setProperty ("deviceName", status.deviceName);
    object->setProperty ("sampleRate", status.sampleRate);
    object->setProperty ("bufferSize", status.bufferSize);
    object->setProperty ("bufferMs", status.bufferMs);
    object->setProperty ("outputLatencyMs", status.outputLatencyMs);
    object->setProperty ("cpu", status.cpu);
    object->setProperty ("xruns", status.xruns);
    object->setProperty ("peakLeft", static_cast<double> (status.peakLeft));
    object->setProperty ("peakRight", static_cast<double> (status.peakRight));
    object->setProperty ("hardwareNoteOns", status.hardwareNoteOns);
    object->setProperty ("notes", notesToVar (status.notesLow, status.notesHigh));
    object->setProperty ("midiInputs", status.midiInputs);
    object->setProperty ("released", status.released);

    // A track freezing: which, and how far.
    if (freezingTrackId != 0)
    {
        object->setProperty ("freezing", freezingTrackId);
        object->setProperty ("freezeProgress", engine.getMixdownProgress());
    }

    const auto& transportState = status.transport;
    juce::DynamicObject::Ptr transportObject (new juce::DynamicObject());
    transportObject->setProperty ("playing", transportState.playing);
    transportObject->setProperty ("ppq", transportState.ppq);
    transportObject->setProperty ("seconds", transportState.seconds);
    transportObject->setProperty ("bpm", transportState.bpm);
    transportObject->setProperty ("numerator", transportState.numerator);
    transportObject->setProperty ("denominator", transportState.denominator);
    transportObject->setProperty ("loop", transportState.loopEnabled);
    transportObject->setProperty ("loopStart", transportState.loopStart);          // quarter notes
    transportObject->setProperty ("loopEnd", transportState.loopEnd);
    transportObject->setProperty ("metronome", transportState.metronomeEnabled);
    transportObject->setProperty ("metronomeVolume", static_cast<double> (transportState.metronomeVolume));
    transportObject->setProperty ("recording", transportState.recording);
    transportObject->setProperty ("capturing", transportState.capturing);
    transportObject->setProperty ("counting", transportState.counting);
    transportObject->setProperty ("countBeats", transportState.countBeatsLeft);
    transportObject->setProperty ("recordOptions", Transport::recordOptionsToVar (engine.getTransport().getRecordOptions()));
    object->setProperty ("transport", juce::var (transportObject.get()));

    juce::Array<juce::var> meters;
    std::map<int, float> surfaceMeters;   // the control surfaces' meters show the same

    for (const auto& meter : status.trackMeters)
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("id", meter.id);
        item->setProperty ("left", static_cast<double> (meter.left));
        item->setProperty ("right", static_cast<double> (meter.right));
        item->setProperty ("inputLeft", static_cast<double> (meter.inputLeft));
        item->setProperty ("inputRight", static_cast<double> (meter.inputRight));

        // Automation, a control surface or MIDI Link moving the fader and pan: the page's follow.
        if (meter.automated || (surfaces != nullptr && surfaces->movedLately (meter.id)) || linkMovedLately (meter.id))
        {
            item->setProperty ("volume", static_cast<double> (meter.volume));
            item->setProperty ("pan", static_cast<double> (meter.pan));
        }

        meters.add (juce::var (item.get()));
        surfaceMeters[meter.id] = juce::jmax (meter.left, meter.right);
    }

    object->setProperty ("trackMeters", meters);
    surfaceMeters[ControlSurfaces::mainChannel] = juce::jmax (status.peakLeft, status.peakRight);
    if (surfaces != nullptr)
        surfaces->setMeters (surfaceMeters);

    // Main's fader moved on a control surface, by MIDI Link, or by the song channel's Main Volume lane.
    if ((surfaces != nullptr && surfaces->movedLately (ControlSurfaces::mainChannel)) || linkMovedLately (ControlSurfaces::mainChannel)
        || (transportState.playing && ! engine.getMainAutomation().empty()))
        object->setProperty ("masterVolume", static_cast<double> (engine.getMasterVolume()));

    // The Sub outputs' meters (the Console's output strips).
    juce::Array<juce::var> outputMeters;

    for (const auto& meter : status.outputMeters)
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("id", meter.id);
        item->setProperty ("left", static_cast<double> (meter.left));
        item->setProperty ("right", static_cast<double> (meter.right));
        outputMeters.add (juce::var (item.get()));
    }

    object->setProperty ("outputMeters", outputMeters);
    object->setProperty ("previewing", engine.getPreviewFile());

    // The Analog Board's meters, while its tab shows; its knobs as MIDI Link moved them, now and then.
    if (boardShown)
        object->setProperty ("board", boardMeters());

    // The Mastering section's meters, while its window shows.
    if (masteringShown)
        object->setProperty ("mastering", masteringMeters());

    // The Live Show's meters and its decks' positions, while its tab shows.
    if (liveShown)
        object->setProperty ("live", liveMeters());

    if (boardDue && juce::Time::getMillisecondCounterHiRes() - boardPushedMs > 100.0)
    {
        boardPushedMs = juce::Time::getMillisecondCounterHiRes();
        pushBoard();
    }

    // The live stream: what's playing on its picture, its preview, and a stream that has ended.
    updateLiveStream();

    // Main is the Live Show's while its tab shows or it plays; synced decks follow the song's tempo.
    updateLiveOwnership();
    followSongTempo();
    updatePlaylist();

    // The playlist's player: the deck it shows, videos being read out for the decks, the screen.
    updateLiveScreen();

    // A deck's control changed what its playlist banner shows: the whole state, once.
    if (liveStateDue)
        pushLive();

    // The Live Show's settings as MIDI Link moved them, now and then (a whole state on showing).
    if (liveValuesDue && liveShown && juce::Time::getMillisecondCounterHiRes() - liveValuesPushedMs > 60.0)
    {
        liveValuesPushedMs = juce::Time::getMillisecondCounterHiRes();
        pushLiveValues();
    }

    webView->emitEventIfBrowserIsVisible ("status", juce::var (object.get()));
}

juce::var MainComponent::pluginToVar (Plugin& plugin)
{
    const auto& descriptor = plugin.getDescriptor();

    juce::DynamicObject::Ptr state (new juce::DynamicObject());
    state->setProperty ("name", descriptor.name);
    state->setProperty ("vendor", descriptor.vendor);
    state->setProperty ("version", descriptor.version);
    state->setProperty ("id", descriptor.id);
    state->setProperty ("file", plugin.getSourceFile().getFullPathName());
    state->setProperty ("fileName", plugin.getSourceFile().getFileName());
    state->setProperty ("role", roleName (plugin.getRole()));
    state->setProperty ("format", plugin.getFormatName());
    state->setProperty ("sidechain", plugin.hasSidechain());
    state->setProperty ("midiOut", plugin.producesMidi());
    state->setProperty ("screen", plugin.getScreenKind() == Plugin::ScreenKind::web ? "web"
                                  : plugin.getScreenKind() == Plugin::ScreenKind::native ? "native" : "none");
    state->setProperty ("outputs", plugin.getMainOutputChannels());

    juce::Array<juce::var> plugins;

    for (const auto& entry : plugin.getPluginsInFile())
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("id", entry.id);
        item->setProperty ("name", entry.name);
        plugins.add (juce::var (item.get()));
    }

    state->setProperty ("plugins", plugins);

    putParams (plugin, *state);   // its parameters (made again only when they changed)
    addContainerInfo (plugin, *state);
    return juce::var (state.get());
}

juce::var MainComponent::clipsToVar (const Track& track) const
{
    juce::Array<juce::var> list;

    for (const auto& clip : track.clips)
    {
        juce::Array<juce::var> notes;

        // Each note as [pitch, start, length, velocity 1-127], then 1 for a muted note, then its
        // operators (MidiNote::toVar): compact, since a clip can hold many.
        for (const auto& note : clip.notes)
            notes.add (note.toVar());

        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("id", clip.id);
        item->setProperty ("start", clip.start);
        item->setProperty ("length", clip.length);
        item->setProperty ("offset", clip.offset);
        item->setProperty ("recording", engine.isRecordingInto (track.id, clip.id));
        item->setProperty ("notes", notes);

        if (clip.muted)
            item->setProperty ("muted", true);

        if (clip.seed != 0)
            item->setProperty ("seed", static_cast<juce::int64> (clip.seed));

        // Pitch bend and aftertouch as [time, value] points (content positions), when there are any.
        if (! clip.pitchBend.empty())
            item->setProperty ("pitchBend", controlsToVar (clip.pitchBend));

        if (! clip.aftertouch.empty())
            item->setProperty ("aftertouch", controlsToVar (clip.aftertouch));

        // Its own automation lines (the clip's Automation), in content times.
        if (! clip.envelopes.empty())
            item->setProperty ("envelopes", Song::envelopesToVar (clip.envelopes));

        if (clip.isAudio())
        {
            item->setProperty ("audioFile", clip.audioFile);
            item->setProperty ("fileName", juce::File (clip.audioFile).getFileNameWithoutExtension());
            item->setProperty ("audioOffset", clip.audioOffset);
            item->setProperty ("audioLength", clip.audioLength);
            item->setProperty ("gain", clip.audioGainDb);
            item->setProperty ("fadeIn", clip.fadeIn);
            item->setProperty ("fadeOut", clip.fadeOut);
            item->setProperty ("missing", ! juce::File (clip.audioFile).existsAsFile());

            // The Stretcher's time map (the Audio Editor marks where it bends), and Silence Areas.
            if (! clip.warp.empty())
                item->setProperty ("warp", Song::warpToVar (clip.warp));

            if (clip.silence.on)
            {
                juce::DynamicObject::Ptr silence (new juce::DynamicObject());
                silence->setProperty ("threshold", clip.silence.thresholdDb);
                silence->setProperty ("shortest", clip.silence.shortest);
                silence->setProperty ("fade", clip.silence.fade);
                item->setProperty ("silence", juce::var (silence.get()));
            }

            // Its own effect: the plugin's name, the slot its window opens by (Hub.openScreen with
            // the track's id), why it isn't playing (when it isn't), and its uid (a paste brings it).
            if (clip.effect != nullptr)
            {
                juce::String error;
                juce::DynamicObject::Ptr effect (new juce::DynamicObject());
                effect->setProperty ("name", clip.effect->name);
                effect->setProperty ("format", clip.effect->format);
                effect->setProperty ("uid", clip.effect->uid);
                const auto slot = engine.clipEffectSlot (track.id, clip.id, error);
                effect->setProperty ("slot", slot);

                // Its parameters, for the clip's Automation lanes.
                if (const auto* found = slot > 0 ? track.findClipEffect (slot) : nullptr; found != nullptr && found->plugin != nullptr)
                    putParams (*found->plugin, *effect);

                if (error.isNotEmpty())
                    effect->setProperty ("error", error);

                item->setProperty ("effect", juce::var (effect.get()));
            }

            // Made from an original (Audio > Sync Tempo, Pitch, Reverse): what was done to it.
            if (clip.isRender())
            {
                item->setProperty ("sourceFile", clip.sourceFile);
                item->setProperty ("sourceName", juce::File (clip.sourceFile).getFileNameWithoutExtension());
                item->setProperty ("sourceOffset", clip.sourceOffset);   // the original's window (a paste keeps it)
                item->setProperty ("sourceLength", clip.sourceLength);
                item->setProperty ("sourceTempo", clip.sourceTempo);
                item->setProperty ("pitch", clip.pitch);
                item->setProperty ("reversed", clip.reversed);
                item->setProperty ("followTempo", clip.followTempo);
            }
        }
        list.add (juce::var (item.get()));
    }

    return list;
}

juce::var MainComponent::controlsToVar (const std::vector<MidiControlPoint>& points)
{
    juce::Array<juce::var> list;
    list.ensureStorageAllocated (static_cast<int> (points.size()));

    for (const auto& point : points)
        list.add (juce::Array<juce::var> { point.time, point.value });

    return list;
}

std::vector<MidiControlPoint> MainComponent::controlsFromVar (const juce::var& list, int minimum, int maximum)
{
    std::vector<MidiControlPoint> points;

    if (const auto* items = list.getArray())
    {
        points.reserve (static_cast<size_t> (items->size()));

        // Each as [time, value], the same shape the page receives.
        for (const auto& item : *items)
            if (const auto* values = item.getArray(); values != nullptr && values->size() >= 2)
                points.push_back ({ static_cast<double> ((*values)[0]), juce::roundToInt (static_cast<double> ((*values)[1])) });
    }

    MidiClip::tidy (points, minimum, maximum);
    return points;
}

std::vector<MidiNote> MainComponent::notesFromVar (const juce::var& list)
{
    std::vector<MidiNote> notes;

    if (const auto* items = list.getArray())
    {
        notes.reserve (static_cast<size_t> (items->size()));

        // Each as [pitch, start, length, velocity 1-127, muted?, operators?], the same shape the page receives.
        for (const auto& item : *items)
            if (MidiNote note; MidiNote::fromVar (item, note))
                notes.push_back (note);
    }

    return notes;
}

double MainComponent::zeroCrossingNear (int trackId, int clipId, double ppq)
{
    // Within 10 ms either side, the nearest place the clip's sound (its channels together) crosses zero.
    const auto* clip = findAudioClip (trackId, clipId);

    if (clip == nullptr || ! std::isfinite (ppq))
        return ppq;

    juce::String error;
    const auto data = engine.getMedia().get (juce::File (clip->audioFile), engine.getPlaybackRate(), error);

    if (data == nullptr || data->playbackRate <= 0.0)
        return ppq;

    const auto& map = engine.getTempoMap();
    const auto clipSeconds = map.secondsAt (clip->start);
    const auto rate = data->playbackRate;
    const auto& samples = data->samples;
    const auto total = static_cast<juce::int64> (samples.getNumSamples());
    const auto at = static_cast<juce::int64> (std::llround ((clip->audioOffset + map.secondsAt (ppq) - clipSeconds) * rate));
    const auto reach = static_cast<juce::int64> (rate * 0.01);

    const auto value = [&samples] (juce::int64 i)
    {
        float sum = 0.0f;

        for (int c = 0; c < samples.getNumChannels(); ++c)
            sum += samples.getSample (c, static_cast<int> (i));

        return sum;
    };

    for (juce::int64 distance = 0; distance <= reach; ++distance)
    {
        for (const auto i : { at + distance, at - distance })
        {
            if (i < 1 || i >= total)
                continue;

            if ((value (i - 1) <= 0.0f) != (value (i) <= 0.0f))
                return map.ppqAt (clipSeconds + static_cast<double> (i) / rate - clip->audioOffset);
        }
    }

    return ppq;
}

void MainComponent::applyClipEdits (const juce::var& edits, const juce::String& label)
{
    bool changed = false;
    juce::String problem;   // why one couldn't be done, said when nothing was

    // The tracks the gesture touches (from and to), and what to call it in Edit > Undo.
    std::vector<int> touched;
    juce::String firstOp;
    int count = 0;

    if (const auto* list = edits.getArray())
    {
        for (const auto& edit : *list)
        {
            touched.push_back (static_cast<int> (edit.getProperty ("trackId", 0)));
            touched.push_back (static_cast<int> (edit.getProperty ("toTrackId", edit.getProperty ("trackId", 0))));

            if (firstOp.isEmpty())
                firstOp = edit.getProperty ("op", {}).toString();

            ++count;
        }
    }

    auto before = Edits::snapshot (engine, touched);

    // The clips the gesture put somewhere (track, clip), and those moved or copied (Track Options:
    // No overlap, Automation follows events).
    std::vector<std::pair<int, int>> placed;
    std::vector<CarriedClip> carried;
    const auto carries = prefs.trackOption ("automationFollowsEvents");

    if (const auto* list = edits.getArray())
    {
        for (const auto& edit : *list)
        {
            const auto op = edit.getProperty ("op", {}).toString();
            const auto trackId = static_cast<int> (edit.getProperty ("trackId", 0));
            const auto clipId = static_cast<int> (edit.getProperty ("clipId", 0));
            const auto toTrackId = static_cast<int> (edit.getProperty ("toTrackId", trackId));
            const auto number = [&edit] (const char* name, double fallback) { return static_cast<double> (edit.getProperty (name, fallback)); };
            bool done = false;

            // Snap To Zero Crossings: an audio clip's cut, or the edge dragged, where its sound crosses zero.
            const auto zeroCross = static_cast<bool> (edit.getProperty ("zeroCross", false));
            const auto zeroEdge = edit.getProperty ("zeroEdge", {}).toString();

            // Where a clip moved or copied was (its envelopes may go with it).
            CarriedClip carry;

            if (carries && (op == "move" || op == "copy"))
                for (const auto& clip : engine.getClips (trackId))
                    if (clip.id == clipId)
                        carry = { trackId, toTrackId, clip.start, clip.start + clip.length, 0.0, op == "copy" };

            if (op == "move")
            {
                done = engine.moveClip (trackId, clipId, toTrackId, number ("start", -1.0));

                if (done)
                    placed.push_back ({ toTrackId, clipId });
            }
            else if (op == "copy")
            {
                const auto copyId = engine.copyClip (trackId, clipId, toTrackId, number ("start", -1.0));
                done = copyId != 0;

                if (done)
                    placed.push_back ({ toTrackId, copyId });
            }
            else if (op == "range")
            {
                auto start = number ("start", -1.0), length = number ("length", 0.0);

                if (zeroEdge == "start")
                {
                    const auto moved = zeroCrossingNear (trackId, clipId, start);
                    length += start - moved;
                    start = moved;
                }
                else if (zeroEdge == "end")
                {
                    length = zeroCrossingNear (trackId, clipId, start + length) - start;
                }

                done = length > 0.0 && engine.setClipRange (trackId, clipId, start, length);

                if (done)
                    placed.push_back ({ trackId, clipId });
            }
            else if (op == "split")  done = engine.splitClip (trackId, clipId, zeroCross ? zeroCrossingNear (trackId, clipId, number ("at", -1.0))
                                                                                         : number ("at", -1.0)) != 0;
            else if (op == "delete") done = engine.deleteClip (trackId, clipId);
            else if (op == "audio")  done = engine.setClipAudio (trackId, clipId, number ("gain", 0.0), number ("fadeIn", 0.0), number ("fadeOut", 0.0));
            else if (op == "mute")   done = engine.setClipMuted (trackId, clipId, static_cast<bool> (edit.getProperty ("muted", true)));
            else if (op == "stripSilence")
            {
                // Strip Silence: the clip cut into the parts that sound, the silent stretches gone.
                done = stripSilence (trackId, clipId, Song::silenceFromVar (edit.getProperty ("silence", {})), problem);
            }
            else if (op == "envelope")
            {
                // One of the clip's own automation lines: its points (content times), or none.
                std::vector<ClipEnvelopePoint> points;

                if (const auto* list = edit.getProperty ("points", {}).getArray())
                    for (const auto& point : *list)
                        if (const auto* pair = point.getArray(); pair != nullptr && pair->size() >= 2)
                            points.push_back ({ static_cast<double> ((*pair)[0]), static_cast<double> ((*pair)[1]) });

                done = engine.setClipEnvelope (trackId, clipId, edit.getProperty ("key", {}).toString(), std::move (points));
            }
            else if (op == "silence")
            {
                // Silence Areas: on with these settings, or off.
                auto silence = Song::silenceFromVar (edit.getProperty ("silence", {}));
                done = engine.setClipSilence (trackId, clipId, silence);
            }
            else if (op == "effect")
            {
                // An audio clip's own effect: a plugin from the Browser, or none (remove).
                if (static_cast<bool> (edit.getProperty ("remove", false)))
                {
                    done = engine.removeClipEffect (trackId, clipId);
                }
                else
                {
                    const auto result = engine.setClipEffect (trackId, clipId, juce::File (edit.getProperty ("file", {}).toString()),
                                                              edit.getProperty ("pluginId", {}).toString());
                    done = result.wasOk();

                    if (result.failed())
                        problem = result.getErrorMessage();
                }
            }
            else if (op == "notes")
            {
                // A clip's notes replaced (a MIDI tool on the selected clips), with its window when given.
                AudioEngine::ClipWindow window;
                const bool hasWindow = edit.hasProperty ("start") && edit.hasProperty ("length");

                if (hasWindow)
                    window = { number ("start", 0.0), number ("length", 0.0), number ("offset", 0.0) };

                done = engine.setClipNotes (trackId, clipId, notesFromVar (edit.getProperty ("notes", {})), hasWindow ? &window : nullptr,
                                            nullptr, nullptr);
            }
            else if (op == "add")
            {
                MidiClip clip;
                clip.start = number ("start", 0.0);
                clip.length = number ("length", 0.0);
                clip.offset = number ("offset", 0.0);
                clip.notes = notesFromVar (edit.getProperty ("notes", {}));
                clip.pitchBend = controlsFromVar (edit.getProperty ("pitchBend", {}), MidiClip::pitchBendMin, MidiClip::pitchBendMax);
                clip.aftertouch = controlsFromVar (edit.getProperty ("aftertouch", {}), 0, MidiClip::aftertouchMax);
                clip.audioFile = edit.getProperty ("audioFile", {}).toString();
                clip.audioOffset = number ("audioOffset", 0.0);
                clip.audioLength = number ("audioLength", 0.0);
                clip.muted = static_cast<bool> (edit.getProperty ("muted", false));
                clip.seed = static_cast<juce::uint32> (static_cast<juce::int64> (number ("seed", 0.0)));
                clip.envelopes = Song::envelopesFromVar (edit.getProperty ("envelopes", {}), clip.audioFile.isNotEmpty());

                // An audio clip's own volume and fades, and what it was rendered from (as clipsToVar
                // names them; kept as a duplicate keeps them, within the limits a song's are read with).
                if (clip.isAudio())
                {
                    clip.audioGainDb = juce::jlimit (-60.0, 12.0, number ("gain", 0.0));
                    clip.fadeIn = juce::jmax (0.0, number ("fadeIn", 0.0));
                    clip.fadeOut = juce::jmax (0.0, number ("fadeOut", 0.0));

                    // Without the original's window a render couldn't be made again: it plays as it is.
                    const auto source = edit.getProperty ("sourceFile", {}).toString();
                    const auto sourceLength = number ("sourceLength", 0.0);

                    // Its own effect, as it is now (a copy of a clip that has one), and Silence Areas.
                    if (const auto uid = static_cast<int> (edit.getProperty ("effectUid", 0)); uid > 0)
                        clip.effect = engine.clipEffectNow (uid);

                    clip.silence = Song::silenceFromVar (edit.getProperty ("silence", {}));

                    if (source.isNotEmpty() && sourceLength > 0.0)
                    {
                        clip.sourceFile = source;
                        clip.sourceOffset = juce::jmax (0.0, number ("sourceOffset", 0.0));
                        clip.sourceLength = sourceLength;
                        clip.sourceTempo = juce::jlimit (0.0, 400.0, number ("sourceTempo", 0.0));
                        clip.pitch = juce::jlimit (-24.0, 24.0, number ("pitch", 0.0));
                        clip.reversed = static_cast<bool> (edit.getProperty ("reversed", false));
                        clip.followTempo = static_cast<bool> (edit.getProperty ("followTempo", false)) && clip.sourceTempo > 0.0;
                        clip.warp = Song::warpFromVar (edit.getProperty ("warp", {}));   // its Stretcher's (its render was made with it)
                    }
                }

                const auto addedId = std::isfinite (clip.offset) ? engine.addClip (trackId, std::move (clip)) : 0;
                done = addedId != 0;

                if (done)
                    placed.push_back ({ trackId, addedId });
            }

            // Moved or copied: where it is now, against where it was.
            if (done && carry.fromTrack != 0)
                for (const auto& clip : engine.getClips (toTrackId))
                    if (clip.id == placed.back().second)
                    {
                        carry.shift = clip.start - carry.start;
                        carried.push_back (carry);
                    }

            changed = done || changed;
        }
    }

    // One undo step and one update for the whole gesture.
    if (changed)
    {
        // No overlap: what the clips were put over is trimmed (or goes), in the same step.
        if (prefs.trackOption ("noOverlap"))
            clearOverlaps (placed);

        const std::map<juce::String, juce::String> verbs { { "move", "Move" }, { "copy", "Copy" }, { "range", "Resize" },
                                                           { "split", "Split" }, { "delete", "Delete" }, { "add", "Add" },
                                                           { "audio", "Adjust" }, { "mute", "Mute" } };
        const auto verb = verbs.count (firstOp) > 0 ? verbs.at (firstOp) : juce::String ("Edit");
        addToHistory (label.isNotEmpty() ? label : verb + (count == 1 ? " Clip" : " Clips"),
                      new Edits::ClipsChanged (engine, std::move (before), Edits::snapshot (engine, touched)));

        // Automation follows events: the envelopes under the clips go with them (the same undo step).
        if (carries)
            carryAutomation (carried);

        pushTracks();
    }
    else if (problem.isNotEmpty())
    {
        pushTracks (problem);
    }
}

juce::var MainComponent::sendsToVar (const Track& track)
{
    juce::Array<juce::var> list;

    for (const auto& send : track.sends)
    {
        juce::String name;

        if (send.sidechain)
        {
            if (auto* plugin = engine.getPlugin (send.target, send.slot); plugin != nullptr && plugin->hasSidechain())
            {
                const auto* owner = send.target == AudioEngine::mainChannel ? nullptr : engine.findTrack (send.target);
                name = plugin->getDescriptor().name + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 "))   // " · "
                     + (owner != nullptr ? owner->name : juce::String ("Main"));
            }
        }
        else if (const auto* fx = engine.findTrack (send.target); fx != nullptr && fx->kind == Track::Kind::fx)
        {
            name = fx->name;
        }

        if (name.isEmpty())
            continue;   // where it went is gone (an undo brings it back)

        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("id", send.id);
        item->setProperty ("sidechain", send.sidechain);
        item->setProperty ("target", send.target);
        item->setProperty ("slot", send.slot);
        item->setProperty ("level", static_cast<double> (send.levelDb));
        item->setProperty ("on", send.on);
        item->setProperty ("name", name);
        list.add (juce::var (item.get()));
    }

    return list;
}

juce::var MainComponent::insertsToVar (const EffectChain& chain)
{
    juce::Array<juce::var> list;

    for (int i = 0; i < chain.size(); ++i)
    {
        auto* insert = chain.at (i);

        if (insert == nullptr || insert->plugin == nullptr)
            continue;

        auto state = pluginToVar (*insert->plugin);
        state.getDynamicObject()->setProperty ("insertId", insert->id);
        state.getDynamicObject()->setProperty ("bypass", insert->bypassed.load());
        list.add (state);
    }

    return list;
}

void MainComponent::pushSongChannel (bool force)
{
    if (webView == nullptr)
        return;

    const auto generation = engine.getTransport().getMapGeneration();
    const auto& points = engine.getMainAutomation();

    const auto samePoints = points.size() == pushedMainAutomation.size()
                         && std::equal (points.begin(), points.end(), pushedMainAutomation.begin(), [] (const AutomationPoint& a, const AutomationPoint& b)
                                        { return juce::exactlyEqual (a.ppq, b.ppq) && juce::exactlyEqual (a.value, b.value); });

    if (! force && songChannelPushed && generation == pushedMapGeneration && samePoints)
        return;

    songChannelPushed = true;
    pushedMapGeneration = generation;
    pushedMainAutomation = points;

    juce::Array<juce::var> mainPoints;

    for (const auto& point : points)
        mainPoints.add (juce::Array<juce::var> { point.ppq, point.value });

    juce::DynamicObject::Ptr channel (new juce::DynamicObject());
    channel->setProperty ("map", engine.getTempoMap().toVar());
    channel->setProperty ("mainAutomation", mainPoints);
    channel->setProperty ("scales", TempoMap::getScales());
    webView->emitEventIfBrowserIsVisible ("songChannel", juce::var (channel.get()));
}

void MainComponent::setSongChannel (const juce::var& event)
{
    const auto before = engine.getTempoMap();
    const auto after = TempoMap::fromVar (event.getProperty ("map", {}));

    // Not while recording (the take is being laid on the bars as they are), nor while the song
    // is being exported (its length was measured on them).
    if (after == before || engine.isRecording() || engine.isTakeActive() || engine.isMixdownRunning())
    {
        pushSongChannel (true);
        return;
    }

    engine.setTempoMap (after);
    addToHistory (event.getProperty ("name", "Edit Master Song Channel").toString(), new Edits::TempoChanged (engine, before, after));
    pushTracks();   // audio clips cover other lengths now; the song channel goes first
}

void MainComponent::setMainAutomationFrom (const juce::var& event)
{
    if (engine.isMixdownRunning())
    {
        pushSongChannel (true);
        return;
    }

    auto before = engine.getMainAutomation();
    std::vector<AutomationPoint> points;

    if (const auto* list = event.getProperty ("points", {}).getArray())
        for (const auto& point : *list)
            if (const auto* pair = point.getArray(); pair != nullptr && pair->size() == 2)
                points.push_back ({ static_cast<double> ((*pair)[0]),
                                    juce::jlimit (static_cast<double> (Track::silentDb), static_cast<double> (Track::maxDb), static_cast<double> ((*pair)[1])) });

    engine.setMainAutomation (std::move (points));

    const auto& after = engine.getMainAutomation();
    const auto same = before.size() == after.size()
                   && std::equal (before.begin(), before.end(), after.begin(), [] (const AutomationPoint& a, const AutomationPoint& b)
                                  { return juce::exactlyEqual (a.ppq, b.ppq) && juce::exactlyEqual (a.value, b.value); });

    // Without points, Main's fader stays where the lane left it.
    if (! same)
        addToHistory ("Edit Main Volume", new Edits::MainAutomationChanged (engine, std::move (before), after));
    pushSongChannel();
    pushTracks();
}

void MainComponent::pushTracks (const juce::String& notice)
{
    if (webView == nullptr)
        return;

    pushSongChannel();   // the page lays tracks out on the song's bars
    pushSongBlocks();    // the blocks name the tracks: a song opened, an undo

    // The board and the show draw a strip or a channel for each track: when they're shown (the
    // board is sent whole as it's shown; the show, as it's shown next when tracks changed).
    if (boardShown || masteringShown) pushBoard();
    if (liveShown)  pushLive();  else liveStale = true;

    loadSongDecks();     // a song just opened: the tracks its decks had

    // Plugins' parameters go by key: only the lists the page hasn't got are sent, before the state.
    auto& params = pluginParams();
    params.byKey = true;
    params.outgoing = new juce::DynamicObject();

    for (auto& [plugin, entry] : params.entries)
        entry.used = false;

    juce::Array<juce::var> list, fxList, busList;   // FX channels and buses live in the Console only
    int number = 1;

    for (const auto& track : engine.getTracks())
    {
        const auto isFx = track->isMixChannel();
        const auto loading = loadingFiles.find (track->id);
        const auto message = trackMessages.find (track->id);

        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("id", track->id);
        item->setProperty ("number", isFx ? 0 : number++);
        item->setProperty ("name", track->name);
        item->setProperty ("colour", "#" + juce::Colour (track->colour).toDisplayString (false));
        item->setProperty ("volume", static_cast<double> (track->volumeDb.load()));
        item->setProperty ("pan", static_cast<double> (track->pan.load()));
        item->setProperty ("mute", track->mute.load());
        item->setProperty ("solo", track->solo.load());
        item->setProperty ("polarity", track->polarity.load());

        if (track->drumMap.isObject())
            item->setProperty ("drumMap", track->drumMap);

        item->setProperty ("monitor", track->monitor.load());
        item->setProperty ("recordArm", track->recordArm.load());
        item->setProperty ("kind", track->kind == Track::Kind::audio ? "audio" : track->kind == Track::Kind::bus ? "bus" : isFx ? "fx" : "instrument");
        item->setProperty ("input", track->input);
        item->setProperty ("output", track->output);

        if (track->kind == Track::Kind::audio)
            item->setProperty ("mono", track->monoMode.load());   // its channel: mono or stereo

        if (track->choir != nullptr)
            item->setProperty ("choir", track->choir->getSettings().toVar());   // a choir track: its voices (Choir.h)

        item->setProperty ("frozen", track->isFrozen());

        if (engine.isFreezeRendering (track->id))
            item->setProperty ("rendering", true);   // in the freeze under way: its plugins' windows close

        item->setProperty ("bus", engine.getTrackBus (track->id));   // the bus it plays into (0: its output)
        item->setProperty ("sends", sendsToVar (*track));
        item->setProperty ("height", track->height);

        juce::Array<juce::var> midiInputs;

        for (const auto& input : track->midiInputs)
            midiInputs.add (input);

        item->setProperty ("midiInputs", midiInputs);
        item->setProperty ("autoMode", Automation::modeName (track->automationMode.load()));
        item->setProperty ("automation", automationToVar (track->automation));
        item->setProperty ("clips", clipsToVar (*track));
        item->setProperty ("loading", loading != loadingFiles.end() ? loading->second : juce::String());
        item->setProperty ("message", message != trackMessages.end() ? message->second : juce::String());
        if (track->instrument != nullptr)
        {
            auto instrument = pluginToVar (*track->instrument);
            instrument.getDynamicObject()->setProperty ("bypass", track->instrumentOff.load());   // its ON/OFF
            item->setProperty ("instrument", instrument);
        }
        else
        {
            item->setProperty ("instrument", juce::var());
        }

        item->setProperty ("inserts", insertsToVar (track->inserts));

        if (track->kept.isArray())
            item->setProperty ("kept", keptToPage (track->kept));   // a DAWproject's placeholders

        if (track->kind == Track::Kind::instrument)
            item->setProperty ("midiFx", engine.getMidiFx (track->id));

        (track->kind == Track::Kind::bus ? busList : isFx ? fxList : list).add (juce::var (item.get()));
    }

    // Every plugin with a sidechain input, for the sends' menus.
    juce::Array<juce::var> sidechainTargets;

    for (const auto& target : engine.getSidechainTargets())
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("channel", target.channel);
        item->setProperty ("slot", target.slot);
        item->setProperty ("name", target.pluginName);
        item->setProperty ("channelName", target.channelName);
        sidechainTargets.add (juce::var (item.get()));
    }

    const auto mainLoading = loadingFiles.find (AudioEngine::mainChannel);
    const auto mainMessage = trackMessages.find (AudioEngine::mainChannel);

    juce::DynamicObject::Ptr master (new juce::DynamicObject());
    master->setProperty ("volume", static_cast<double> (engine.getMasterVolume()));
    master->setProperty ("mute", engine.isMasterMuted());
    master->setProperty ("inserts", insertsToVar (engine.getMainInserts()));

    if (engine.getMainKept().isArray())
        master->setProperty ("kept", keptToPage (engine.getMainKept()));
    master->setProperty ("loading", mainLoading != loadingFiles.end() ? mainLoading->second : juce::String());
    master->setProperty ("message", mainMessage != trackMessages.end() ? mainMessage->second : juce::String());

    juce::DynamicObject::Ptr state (new juce::DynamicObject());
    juce::Array<juce::var> channelNames, outputChannelNames;

    for (const auto& name : engine.getInputChannelNames())
        channelNames.add (name);

    for (const auto& name : engine.getOutputChannelNames())
        outputChannelNames.add (name);

    // What Song Setup's Reset to Default goes back to.
    juce::DynamicObject::Ptr defaults (new juce::DynamicObject());
    defaults->setProperty ("inputs", Song::inputsToVar (engine.getSongDefaultInputs()));
    defaults->setProperty ("outputs", Song::outputsToVar (engine.getSongDefaultOutputs()));
    defaults->setProperty ("audition", engine.getSongDefaultAudition());

    state->setProperty ("selected", engine.getSelectedTrackId());
    state->setProperty ("tracks", list);
    state->setProperty ("fxChannels", fxList);
    state->setProperty ("busChannels", busList);
    state->setProperty ("sidechainTargets", sidechainTargets);
    state->setProperty ("inputs", Song::inputsToVar (engine.getInputs()));
    state->setProperty ("inputChannels", channelNames);
    state->setProperty ("outputs", Song::outputsToVar (engine.getOutputs()));
    state->setProperty ("outputChannels", outputChannelNames);
    state->setProperty ("audition", engine.getAuditionOutput());
    state->setProperty ("ioDefaults", juce::var (defaults.get()));
    state->setProperty ("recordBits", engine.getRecordBits());
    state->setProperty ("master", juce::var (master.get()));
    state->setProperty ("boardMixing", engine.isBoardMixing());
    state->setProperty ("masteringOn", engine.getMastering().settings.isOn (MasterSection::on));   // the Mastering section is in
    state->setProperty ("notice", notice);

    // The parameter lists: the new ones go first; the page keeps those this state names.
    juce::StringArray named;

    for (auto it = params.entries.begin(); it != params.entries.end();)
    {
        if (it->second.used)
        {
            named.add (it->second.key());
            ++it;
        }
        else
        {
            it = params.entries.erase (it);   // its plugin is gone, or wasn't in the song
        }
    }

    if (! params.outgoing->getProperties().isEmpty())
    {
        juce::DynamicObject::Ptr message (new juce::DynamicObject());
        message->setProperty ("params", juce::var (params.outgoing.get()));
        webView->emitEventIfBrowserIsVisible ("pluginParams", juce::var (message.get()));
    }

    // What the page has now (none of them, if it couldn't hear this: they all go next time).
    params.sent.clear();

    if (webView->isVisible())
        params.sent.insert (named.begin(), named.end());
    params.byKey = false;
    params.outgoing = nullptr;

    webView->emitEventIfBrowserIsVisible ("tracks", juce::var (state.get()));

    followVoiceInput();   // the song's inputs may have changed
}

void MainComponent::pushTakeClips()
{
    takeClipsDue = false;

    if (webView == nullptr)
        return;

    // The tracks a take is recording into: their clips as they are now.
    juce::Array<juce::var> list;

    for (const auto& track : engine.getTracks())
    {
        const auto recording = std::any_of (track->clips.begin(), track->clips.end(),
                                            [&] (const auto& clip) { return engine.isRecordingInto (track->id, clip.id); });

        if (! recording)
            continue;

        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("id", track->id);
        item->setProperty ("clips", clipsToVar (*track));
        list.add (juce::var (item.get()));
    }

    if (list.isEmpty())
        return;

    juce::DynamicObject::Ptr state (new juce::DynamicObject());
    state->setProperty ("tracks", list);
    webView->emitEventIfBrowserIsVisible ("takeClips", juce::var (state.get()));
}

void MainComponent::pushTakeWaves (double nowMs)
{
    const auto waves = engine.getLiveWaves();

    if (waves.empty())
    {
        takeWaveSent.clear();
        return;
    }

    if (webView == nullptr || nowMs - takeWavesPushedMs < 100.0)
        return;

    takeWavesPushedMs = nowMs;
    juce::Array<juce::var> list;

    for (const auto& wave : waves)
    {
        // A new pass (the loop came round): its waveform starts again.
        auto& sent = takeWaveSent[wave.trackId];
        const bool reset = sent.first != wave.firstBucket || sent.second < wave.firstBucket;

        if (reset)
            sent = { wave.firstBucket, wave.firstBucket };

        const auto upTo = juce::jmin<juce::int64> (wave.buckets, sent.second + 20000);   // no more than about 3.5 minutes at once

        if (upTo <= sent.second && ! reset)
            continue;

        juce::Array<juce::var> peaks;
        peaks.ensureStorageAllocated (static_cast<int> (2 * juce::jmax<juce::int64> (0, upTo - sent.second)));

        for (auto b = sent.second; b < upTo; ++b)
        {
            peaks.add (static_cast<int> (wave.data[2 * b]));
            peaks.add (static_cast<int> (wave.data[2 * b + 1]));
        }

        sent.second = juce::jmax (sent.second, upTo);

        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("id", wave.trackId);
        item->setProperty ("ppq", wave.ppq);
        item->setProperty ("rate", wave.rate);
        item->setProperty ("bucket", Track::LivePeaks::peakBucket);
        item->setProperty ("reset", reset);
        item->setProperty ("peaks", peaks);
        list.add (juce::var (item.get()));
    }

    if (list.isEmpty())
        return;

    juce::DynamicObject::Ptr state (new juce::DynamicObject());
    state->setProperty ("tracks", list);
    webView->emitEventIfBrowserIsVisible ("takeWave", juce::var (state.get()));
}

void MainComponent::pushLibrary (const juce::String& notice)
{
    trustLibrary (library);

    if (webView == nullptr)
        return;

    juce::Array<juce::var> list;

    for (const auto& entry : library.getEntries())
    {
        // Third-party plugins off for this run: only Sonify's own are offered.
        if (! library.isOffered (entry.file))
            continue;

        juce::Array<juce::var> features;

        for (const auto& feature : entry.features)
            features.add (feature);

        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("file", entry.file);
        item->setProperty ("fileName", juce::File (entry.file).getFileName());
        item->setProperty ("folder", library.folderLabelFor (entry.file));
        item->setProperty ("id", entry.id);
        item->setProperty ("name", entry.name);
        item->setProperty ("vendor", entry.vendor);
        item->setProperty ("version", entry.version);
        item->setProperty ("kind", entry.kind);
        item->setProperty ("category", entry.category);
        item->setProperty ("format", entry.format());
        item->setProperty ("features", features);
        item->setProperty ("favorite", entry.favorite);
        item->setProperty ("hidden", entry.hidden);
        item->setProperty ("missing", entry.missing);
        item->setProperty ("modified", static_cast<double> (library.fileTimeOf (entry.file)));

        // A made plugin's picture of its panel, for the Browser's pop-up: when it was taken (its
        // address changes with it); one missing or out of date is drawn soon (updateSnapshots).
        if (MadePlugins::isMadeFile (entry.file))
        {
            const juce::File folder (entry.file);
            const auto picture = MadePlugins::snapshotOf (folder);

            if (picture.existsAsFile())
            {
                item->setProperty ("snapshot", folder.getFileName());
                item->setProperty ("snapshotTime", static_cast<double> (picture.getLastModificationTime().toMilliseconds()));
            }

            if (snapshotDue (folder) && snapshotTried.count (snapshotKey (folder)) == 0)
                snapshotQueue.addIfNotAlreadyThere (folder.getFullPathName());
        }
        item->setProperty ("lastUsed", static_cast<double> (entry.lastUsed));
        item->setProperty ("uses", entry.uses);
        list.add (juce::var (item.get()));
    }

    juce::Array<juce::var> folders;

    for (const auto& folder : library.getFolders())
        folders.add (folder);

    juce::DynamicObject::Ptr scan (new juce::DynamicObject());
    scan->setProperty ("active", scanner != nullptr);
    scan->setProperty ("done", scanner != nullptr ? scanner->getDone() : 0);
    scan->setProperty ("total", scanner != nullptr ? scanner->getTotal() : 0);
    scan->setProperty ("current", scanner != nullptr ? scanner->getCurrent() : juce::String());

    juce::DynamicObject::Ptr state (new juce::DynamicObject());
    state->setProperty ("entries", list);
    state->setProperty ("folders", folders);
    state->setProperty ("scan", juce::var (scan.get()));
    state->setProperty ("notice", notice);
    state->setProperty ("thirdParty", library.offersThirdParty());

    // The Plug-In Manager's: the Blocklist, the format order for duplicates.
    juce::Array<juce::var> blocked, priority;

    for (const auto& item : library.getBlocked())
    {
        juce::DynamicObject::Ptr entry (new juce::DynamicObject());
        entry->setProperty ("file", item.file);
        entry->setProperty ("name", juce::File (item.file).getFileName());
        entry->setProperty ("reason", item.reason);
        entry->setProperty ("removed", item.removed);
        blocked.add (juce::var (entry.get()));
    }

    for (const auto& format : library.getFormatPriority())
        priority.add (format);

    state->setProperty ("blocked", blocked);
    state->setProperty ("formatPriority", priority);
    state->setProperty ("hideDuplicatesAfterScans", library.hidesDuplicatesAfterScans());

    webView->emitEventIfBrowserIsVisible ("library", juce::var (state.get()));
}

//==============================================================================
bool MainComponent::handleTrackSettings (const juce::var& event, bool push)
{
    const auto id = static_cast<int> (event.getProperty ("id", 0));
    auto* track = engine.findTrack (id);

    if (track == nullptr)
        return false;

    bool changedShape = false;

    if (event.hasProperty ("volume"))
    {
        track->volumeDb.store (juce::jlimit (Track::silentDb, Track::maxDb,
                                             static_cast<float> (static_cast<double> (event.getProperty ("volume", 0.0)))));
        automationTouched (id, { AutomationTarget::Kind::volume, 0, 0 });
    }

    if (event.hasProperty ("pan"))
    {
        track->pan.store (juce::jlimit (-1.0f, 1.0f, static_cast<float> (static_cast<double> (event.getProperty ("pan", 0.0)))));
        automationTouched (id, { AutomationTarget::Kind::pan, 0, 0 });
    }

    if (event.hasProperty ("mute"))
    {
        track->mute.store (static_cast<bool> (event.getProperty ("mute", false)));
        changedShape = true;
    }

    if (event.hasProperty ("solo"))
    {
        track->solo.store (static_cast<bool> (event.getProperty ("solo", false)));
        changedShape = true;
    }

    // Polarity (Ø): mixing, as the fader is.
    if (event.hasProperty ("polarity"))
    {
        track->polarity.store (static_cast<bool> (event.getProperty ("polarity", false)));
        changedShape = true;
    }

    if (event.hasProperty ("monitor"))
    {
        engine.setTrackMonitor (id, static_cast<bool> (event.getProperty ("monitor", false)), true);
        changedShape = true;
    }

    if (event.hasProperty ("recordArm"))
    {
        armTrack (id, static_cast<bool> (event.getProperty ("recordArm", false)));
        changedShape = true;
    }

    // MIDI in: the sources the track's instrument hears (MIDI devices, other tracks' plugins).
    if (const auto* sources = event.getProperty ("midiInputs", {}).getArray())
    {
        juce::StringArray list;

        for (const auto& source : *sources)
            list.add (source.toString());

        engine.setTrackMidiInputs (id, list);
        pushTracks();
    }

    // Its height in the arrangement (0: the view's size), set by dragging its bottom edge.
    if (event.hasProperty ("height"))
        if (auto* resized = engine.findTrack (id))
            resized->height = juce::jlimit (0, 1000, static_cast<int> (event.getProperty ("height", 0)));

    if (event.hasProperty ("input"))
    {
        engine.setTrackInput (id, static_cast<int> (event.getProperty ("input", 0)));
        changedShape = true;
    }

    // An audio track's channel: mono or stereo.
    if (event.hasProperty ("mono"))
    {
        engine.setTrackMono (id, static_cast<bool> (event.getProperty ("mono", false)));
        changedShape = true;
    }

    // Where it plays: Main (0) or a Sub output.
    if (event.hasProperty ("output"))
    {
        engine.setTrackOutput (id, static_cast<int> (event.getProperty ("output", 0)));
        changedShape = true;
    }

    // Or into a bus (its id; 0: its output again).
    if (event.hasProperty ("bus"))
    {
        engine.setTrackBus (id, static_cast<int> (event.getProperty ("bus", 0)));
        changedShape = true;
    }

    if (event.hasProperty ("name"))
    {
        engine.setTrackName (id, event.getProperty ("name", {}).toString());
        changedShape = true;
    }

    // The Editor's Drums view: its rows' names and the rows added (the track's drum map).
    if (event.hasProperty ("drumMap"))
    {
        track->drumMap = Track::sanitiseDrumMap (event.getProperty ("drumMap", {}));
        changedShape = true;
    }

    // Its colour, "#rrggbb".
    if (event.hasProperty ("colour"))
    {
        const auto text = event.getProperty ("colour", {}).toString().trimCharactersAtStart ("#");

        if (text.length() == 6 && text.containsOnly ("0123456789abcdefABCDEF"))
        {
            track->colour = 0xff000000u | static_cast<juce::uint32> (text.getHexValue32());
            changedShape = true;
        }
    }

    // Fader moves are not echoed back: the page already shows them.
    if (changedShape && push)
        pushTracks();

    return changedShape;
}

void MainComponent::handleTrackGroup (const juce::String& what, const juce::var& event)
{
    // The tracks named, each once, that are still there.
    std::vector<int> ids;

    if (const auto* list = event.getProperty ("ids", {}).getArray())
        for (const auto& value : *list)
            if (const auto id = static_cast<int> (value); engine.findTrack (id) != nullptr && std::find (ids.begin(), ids.end(), id) == ids.end())
                ids.push_back (id);

    if (what == "settings")
    {
        // The same change to every selected channel (Mute on all of them, say): mixing, no undo step.
        bool changed = false;

        for (const auto id : ids)
        {
            juce::DynamicObject::Ptr one (new juce::DynamicObject());

            if (const auto* fields = event.getDynamicObject())
                for (const auto& [key, value] : fields->getProperties())
                    if (key.toString() != "ids")
                        one->setProperty (key, value);

            one->setProperty ("id", id);
            changed = handleTrackSettings (juce::var (one.get()), false) || changed;
        }

        markChanged();

        if (changed)
            pushTracks();

        return;
    }

    if (what == "remove")
    {
        // Kept, plugins and all, so one undo brings them all back exactly.
        bool first = true;

        for (const auto id : ids)
        {
            int index = 0;

            if (auto removed = engine.detachTrack (id, index))
            {
                addToHistory (ids.size() > 1 ? "Remove Tracks" : "Remove Track", new Edits::TrackRemoved (engine, id, index, std::move (removed)), ! first);
                first = false;
            }

            loadingFiles.erase (id);
            trackMessages.erase (id);
        }

        pushTracks();
        return;
    }

    if (what == "duplicate")
    {
        // Each copy right under its original, plugins and all; one undo step. The copies are selected.
        std::vector<int> copies;
        juce::StringArray problems;
        bool first = true;

        for (const auto id : ids)
        {
            const auto copy = Song::duplicateTrack (engine, id, problems);

            if (copy == 0)
                continue;

            auto order = Edits::allTrackIds (engine);
            order.erase (std::remove (order.begin(), order.end(), copy), order.end());
            const auto original = std::find (order.begin(), order.end(), id);
            order.insert (original == order.end() ? order.end() : original + 1, copy);
            engine.setTrackOrder (order);

            const auto placed = Edits::allTrackIds (engine);
            const auto index = static_cast<int> (std::find (placed.begin(), placed.end(), copy) - placed.begin());
            addToHistory (ids.size() > 1 ? "Duplicate Tracks" : "Duplicate Track", new Edits::TrackAdded (engine, copy, index), ! first);
            first = false;
            copies.push_back (copy);
        }

        // The keyboard plays the first copy that's a track (FX channels aren't played).
        for (const auto copy : copies)
        {
            if (const auto* track = engine.findTrack (copy); track != nullptr && ! track->isMixChannel())
            {
                engine.selectTrack (copy);
                break;
            }
        }

        pushTracks (problems.joinIntoString ("\n"));

        if (webView != nullptr && ! copies.empty())
        {
            juce::Array<juce::var> list;

            for (const auto copy : copies)
                list.add (copy);

            juce::DynamicObject::Ptr selection (new juce::DynamicObject());
            selection->setProperty ("ids", list);
            webView->emitEventIfBrowserIsVisible ("selectTracks", juce::var (selection.get()));
        }

        return;
    }

    if (what == "move")
    {
        // Moved together in the song's order, before the track named (0: to the end).
        const auto before = static_cast<int> (event.getProperty ("before", 0));
        const auto old = Edits::allTrackIds (engine);
        std::vector<int> moving;

        for (const auto id : old)
            if (std::find (ids.begin(), ids.end(), id) != ids.end())
                moving.push_back (id);

        if (moving.empty() || std::find (moving.begin(), moving.end(), before) != moving.end())
            return;

        auto order = old;
        order.erase (std::remove_if (order.begin(), order.end(), [&moving] (int id) { return std::find (moving.begin(), moving.end(), id) != moving.end(); }),
                     order.end());
        const auto at = before == 0 ? order.end() : std::find (order.begin(), order.end(), before);
        order.insert (at, moving.begin(), moving.end());

        if (order == old)
            return;

        engine.setTrackOrder (order);
        addToHistory (moving.size() > 1 ? "Move Tracks" : "Move Track", new Edits::TracksReordered (engine, old, order));
        pushTracks();
        return;
    }

    if (what == "addBus")
    {
        // A bus, and (Add Bus for Selected Channels) the tracks chosen playing into it: one undo step.
        // Buses don't play into buses, nor FX channels into either.
        const auto bus = engine.addTrack (Track::Kind::bus);
        addToHistory (ids.empty() ? "Add Bus Channel" : "Add Bus for Selected Channels",
                      new Edits::TrackAdded (engine, bus, static_cast<int> (engine.getTracks().size()) - 1));

        std::map<int, int> before, after;

        for (const auto id : ids)
        {
            if (const auto* track = engine.findTrack (id); track != nullptr && ! track->isMixChannel())
            {
                before[id] = track->outputBus.load();
                engine.setTrackBus (id, bus);
                after[id] = bus;
            }
        }

        if (! after.empty())
            addToHistory ("Add Bus for Selected Channels", new Edits::BusesChanged (engine, std::move (before), std::move (after)), true);

        pushTracks();
        return;
    }

    if (what == "rename")
    {
        // Each its own name ("Vocals 1", "Vocals 2"...), as one undo step.
        bool first = true;

        if (const auto* names = event.getProperty ("names", {}).getArray())
        {
            for (const auto& item : *names)
            {
                const auto id = static_cast<int> (item.getProperty ("id", 0));
                const auto name = item.getProperty ("name", {}).toString().trim();

                if (engine.findTrack (id) == nullptr || name.isEmpty())
                    continue;

                const auto nameBefore = Edits::nameOf (engine, id);
                engine.setTrackName (id, name);
                const auto nameAfter = Edits::nameOf (engine, id);

                if (nameBefore.text != nameAfter.text || nameBefore.automatic != nameAfter.automatic)
                {
                    addToHistory ("Rename Tracks", new Edits::TrackRenamed (engine, id, nameBefore, nameAfter), ! first);
                    first = false;
                }
            }
        }

        pushTracks();
    }
}

void MainComponent::loadPluginInto (int channel, const juce::File& file, const juce::String& pluginId,
                                    int position, int replaceInsertId, bool joinUndoStep)
{
    // Third-party plugins off for this run (the question at startup): only Sonify's own load.
    if (! library.isOffered (file.getFullPathName()))
    {
        trackMessages[channel] = "Third-party plugins are off this time. Restart Sonify and choose Yes to use them.";
        pushTracks();
        return;
    }

    loadingFiles[channel] = file.getFileName();
    trackMessages.erase (channel);
    pushTracks();

    // Loading compiles the WebAssembly and can take a moment, so the page gets to show
    // "Loading" before the work starts. The step to join (the track's Add Track, just made) is the
    // newest now: it's joined only if it still is then (no step begun, undone, redone or cleared
    // meanwhile), else the load is a step of its own.
    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<MainComponent> (this),
                                      channel, file, pluginId, position, replaceInsertId, joinUndoStep, step = historySteps]
    {
        if (safeThis == nullptr)
            return;

        int insertId = 0;
        AudioEngine::Displaced displaced;
        const auto nameBefore = Edits::nameOf (safeThis->engine, channel);
        const auto result = safeThis->engine.loadPlugin (channel, file, pluginId, position, replaceInsertId, insertId, &displaced);

        safeThis->loadingFiles.erase (channel);

        if (result.failed())
        {
            safeThis->trackMessages[channel] = result.getErrorMessage();
        }
        else
        {
            const auto join = joinUndoStep && safeThis->historySteps == step && safeThis->undoManager.canUndo();
            safeThis->recordPluginLoad (channel, insertId, displaced, nameBefore, join);

            // An instrument plugin on an instrument track: Monitor on, the first time only.
            if (insertId == AudioEngine::instrumentSlot)
                safeThis->engine.monitorOnce (channel);

            // For the Browser's Recent folder.
            if (auto* plugin = safeThis->engine.getPlugin (channel, insertId))
            {
                safeThis->library.markUsed (file.getFullPathName(), plugin->getDescriptor().id);
                safeThis->pushLibrary();
            }
        }

        safeThis->pushTracks();

        // A plugin the user just put on a channel opens its window at once (from then on it opens
        // when they open it).
        if (result.wasOk())
            safeThis->openPluginScreen (channel, insertId);
    });
}

void MainComponent::recordPluginLoad (int channel, int insertId, AudioEngine::Displaced& displaced,
                                      const Edits::TrackName& nameBefore, bool joinUndoStep)
{
    // What the load took out is kept in the step, so undo puts it back exactly.
    if (insertId == AudioEngine::instrumentSlot)
        addToHistory ("Load Instrument", new Edits::InstrumentChanged (engine, channel, std::move (displaced.instrument),
                                                                       nameBefore, Edits::nameOf (engine, channel)), joinUndoStep);
    else if (displaced.insert != nullptr)
        addToHistory ("Replace Insert", new Edits::InsertReplaced (engine, channel, insertId, std::move (displaced.insert),
                                                                   displaced.insertPosition), joinUndoStep);
    else
        addToHistory ("Add Insert", new Edits::InsertAdded (engine, channel, insertId, engine.getInsertPosition (channel, insertId)), joinUndoStep);
}

//==============================================================================
void MainComponent::addToHistory (const juce::String& name, juce::UndoableAction* action, bool joinPreviousStep)
{
    // A control surface's send turn still going is its own step, before this one.
    if (! joinPreviousStep)
        commitSurfaceSendEdit();

    if (! joinPreviousStep)
    {
        undoManager.beginNewTransaction (name);
        ++historySteps;
    }

    undoManager.perform (action);
    lastHistoryWasUndo = false;
    markChanged();
    pushSong();
}

void MainComponent::undoOrRedo (bool undo)
{
    // Not while recording: the take is still being written into its clips.
    if (engine.isRecording() || engine.isTakeActive())
        return;

    commitSurfaceSendEdit();   // a control surface's send turn just made is what's undone

    if (undo ? undoManager.undo() : undoManager.redo())
    {
        ++historySteps;
        lastHistoryWasUndo = undo;
        markChanged();
        pushTracks();
        pushSong();
    }
}

void MainComponent::setPluginParameter (const juce::var& event)
{
    const auto channel = static_cast<int> (event.getProperty ("trackId", 0));
    const auto insertId = static_cast<int> (event.getProperty ("insertId", 0));
    const auto index = static_cast<int> (event.getProperty ("index", -1));
    const auto value = engine.setPluginParameter (channel, insertId, index,
                                                  static_cast<double> (event.getProperty ("value", 0.0)));
    automationParamTouched (channel, insertId, index);

    if (webView == nullptr)
        return;

    juce::DynamicObject::Ptr reply (new juce::DynamicObject());
    reply->setProperty ("trackId", channel);
    reply->setProperty ("insertId", insertId);
    reply->setProperty ("index", index);
    reply->setProperty ("value", value);
    reply->setProperty ("text", engine.getPluginParameterText (channel, insertId, index));

    webView->emitEventIfBrowserIsVisible ("param", juce::var (reply.get()));
}

void MainComponent::choosePlugins (bool thirdParty)
{
    if (pluginChoiceMade)
        return;   // asked once a run

    pluginChoiceMade = true;
    library.setThirdPartyPlugins (thirdParty, contentFolder().getChildFile ("Plugins"));

    // Yes: the plugins are checked now, with the scanning page before the welcome page.
    // No: no scan; the page goes straight to the welcome page, and the Browser and the AI
    // offer only Sonify's own plugins until the next start.
    if (thirdParty)
        startLibraryScan (false, true);
    else
        pushLibrary();
}

void MainComponent::startLibraryScan (bool full, bool atStartup)
{
    if (scanner != nullptr)
        return;   // one scan at a time; this one shows already

    scanAtStartup = atStartup;
    scanFound = 0;
    scanProblems.clear();

    auto toRead = library.prepareScan (full);

    if (toRead.empty())
    {
        // Nothing new or changed: the page goes straight on.
        library.save();
        pushLibrary();
        emitScan ("done");
        return;
    }

    // Read by copies of the Hub, one plugin each, on a thread of its own; this timer collects.
    scanner = std::make_unique<PluginScanner> (std::move (toRead));
    scanner->start();
    emitScan ("running");
    pushLibrary();
}

void MainComponent::updateScan()
{
    if (scanner == nullptr)
        return;

    auto outcomes = scanner->takeOutcomes();

    for (auto& outcome : outcomes)
    {
        int added = 0;

        if (library.recordScan (outcome.file, outcome.plugins, outcome.error, added).failed())
        {
            juce::DynamicObject::Ptr problem (new juce::DynamicObject());
            problem->setProperty ("name", outcome.file.getFileName());
            problem->setProperty ("error", outcome.error);
            scanProblems.add (juce::var (problem.get()));
        }

        scanFound += added;
    }

    if (! scanner->isFinished())
    {
        emitScan ("running");
        return;
    }

    // The last results, then the verdict.
    for (auto& outcome : scanner->takeOutcomes())
    {
        int added = 0;

        if (library.recordScan (outcome.file, outcome.plugins, outcome.error, added).failed())
        {
            juce::DynamicObject::Ptr problem (new juce::DynamicObject());
            problem->setProperty ("name", outcome.file.getFileName());
            problem->setProperty ("error", outcome.error);
            scanProblems.add (juce::var (problem.get()));
        }

        scanFound += added;
    }

    const auto cancelled = scanner->wasCancelled();
    emitScan (cancelled ? "cancelled" : "done");
    scanner.reset();

    // Plugins found in a second format: shown in the first only, when that's asked for.
    if (library.hidesDuplicatesAfterScans())
        library.hideDuplicates();

    library.save();

    juce::String notice;

    if (scanFound > 0)
        notice << "Found " << scanFound << (scanFound == 1 ? " new plugin." : " new plugins.");

    if (! scanProblems.isEmpty())
        notice << (notice.isEmpty() ? "" : " ") << scanProblems.size() << (scanProblems.size() == 1 ? " file couldn't be read." : " files couldn't be read.");

    pushLibrary (notice);
}

void MainComponent::emitScan (const juce::String& state)
{
    if (webView == nullptr)
        return;

    juce::DynamicObject::Ptr item (new juce::DynamicObject());
    item->setProperty ("state", state);
    item->setProperty ("startup", scanAtStartup);
    item->setProperty ("total", scanner != nullptr ? scanner->getTotal() : 0);
    item->setProperty ("done", scanner != nullptr ? scanner->getDone() : 0);
    item->setProperty ("current", scanner != nullptr ? scanner->getCurrent() : juce::String());
    item->setProperty ("found", scanFound);
    item->setProperty ("plugins", static_cast<int> (std::count_if (library.getEntries().begin(), library.getEntries().end(),
                                                                   [this] (const Library::Entry& entry) { return library.isOffered (entry.file); })));
    item->setProperty ("problems", scanProblems);
    webView->emitEventIfBrowserIsVisible ("pluginScan", juce::var (item.get()));
}

void MainComponent::chooseLibraryFolder()
{
    if (choosingFile)
        return;

    auto startFolder = lastPluginFolder;

    if (! startFolder.isDirectory())
        startFolder = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

    choosingFile = true;
    fileChooser = std::make_unique<juce::FileChooser> ("Add a Plugin Folder: the Browser finds every WCLAP in it", startFolder);

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [safeThis = juce::Component::SafePointer<MainComponent> (this)] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        safeThis->choosingFile = false;
        const auto folder = chooser.getResult();

        if (! folder.isDirectory())
            return;

        safeThis->lastPluginFolder = folder;
        safeThis->library.addFolder (folder);
        safeThis->startLibraryScan (false);
    });
}

void MainComponent::chooseLibraryFiles()
{
    if (choosingFile)
        return;

    auto startFolder = lastPluginFolder;

    if (! startFolder.isDirectory())
    {
        const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
        startFolder = home.getChildFile ("Downloads").isDirectory() ? home.getChildFile ("Downloads") : home;
    }

    choosingFile = true;
    fileChooser = std::make_unique<juce::FileChooser> ("Add plugins to the Browser: VST3, CLAP, or WCLAP (.wasm, .wclap.tar.gz)",
                                                       startFolder, "*.wasm;*.gz;*.tgz;*.vst3;*.clap");

    const auto chooserFlags = juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::canSelectMultipleItems;

    fileChooser->launchAsync (chooserFlags, [safeThis = juce::Component::SafePointer<MainComponent> (this)] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        safeThis->choosingFile = false;
        const auto files = chooser.getResults();

        if (files.isEmpty())
            return;

        safeThis->lastPluginFolder = files.getFirst().getParentDirectory();

        // Listed, then read by the scan (with its page showing), never inside the Hub.
        for (const auto& file : files)
            safeThis->library.addFile (file);

        safeThis->startLibraryScan (false);
    });
}

//==============================================================================
void MainComponent::handleTransport (const juce::var& event)
{
    // A mixdown owns the song while it renders.
    if (engine.isMixdownRunning())
        return;

    auto& transport = engine.getTransport();
    const auto mapBefore = engine.getTempoMap();
    const auto action = event.getProperty ("action", {}).toString();

    // The Live Show has Main (its tab shows, or it plays): the song waits.
    if (engine.isLiveShowOn() && (action == "play" || action == "toggle" || action == "record"))
    {
        pushTracks ("The Live Show has the outputs while it plays: stop its decks, pads, recording and stream to play the song.");
        return;
    }

    if (action == "record")
    {
        // Record toggles: on starts playback too; off punches out and playback goes on.
        if (engine.isRecording())
        {
            engine.stopRecording();
        }
        else
        {
            engine.startRecording();

            // The undo step's "before", taken now: an audio take has already begun.
            std::vector<int> armed;

            for (const auto& track : engine.getTracks())
                if (track->recordArm.load())
                    armed.push_back (track->id);

            takeBefore = Edits::snapshot (engine, armed);
            takeRunning = true;
        }

        pushTracks();   // the selected track may have just been armed
    }

    if (action == "toggle")      transport.togglePlaying();
    else if (action == "play")   transport.setPlaying (true);
    else if (action == "stop")   transport.setPlaying (false);
    else if (action == "start")  transport.locate (0.0);
    else if (action == "locate") transport.locate (static_cast<double> (event.getProperty ("ppq", 0.0)));

    for (const auto* setting : { "bpm", "numerator", "denominator", "loop", "loopStart", "loopEnd", "metronome", "metronomeVolume" })
        if (event.hasProperty (setting))
            markChanged();

    // The tempo field sets the tempo that holds where the song is (one tempo: the song's), the
    // signature fields the time signature of the bar it's in.
    const auto state = transport.getState();

    if (event.hasProperty ("bpm"))
        engine.setTempoMap (engine.getTempoMap().withTempoAt (state.ppq, static_cast<double> (event.getProperty ("bpm", 120.0))));

    if (event.hasProperty ("numerator") || event.hasProperty ("denominator"))
        engine.setTempoMap (engine.getTempoMap().withSignatureAt (state.ppq, static_cast<int> (event.getProperty ("numerator", state.numerator)),
                                                                  static_cast<int> (event.getProperty ("denominator", state.denominator))));

    // Loop start and end are song positions in quarter notes, as the ruler draws them.
    if (event.hasProperty ("loop") || event.hasProperty ("loopStart") || event.hasProperty ("loopEnd"))
        transport.setLoop (static_cast<bool> (event.getProperty ("loop", state.loopEnabled)),
                           static_cast<double> (event.getProperty ("loopStart", state.loopStart)),
                           static_cast<double> (event.getProperty ("loopEnd", state.loopEnd)));

    // Precount, Preroll, Postroll, Autopunch (and their bars): kept with the song, not edits.
    if (const auto options = event.getProperty ("recordOptions", {}); options.isObject())
    {
        transport.setRecordOptions (Transport::recordOptionsFromVar (options, transport.getRecordOptions()));
        markChanged();
    }

    if (event.hasProperty ("metronome") || event.hasProperty ("metronomeVolume"))
        transport.setMetronome (static_cast<bool> (event.getProperty ("metronome", state.metronomeEnabled)),
                                static_cast<float> (static_cast<double> (event.getProperty ("metronomeVolume", state.metronomeVolume))));

    // Tempo and time signature are edits; the loop and the metronome are not.
    if (engine.getTempoMap() != mapBefore)
    {
        addToHistory (event.hasProperty ("bpm") ? "Change Tempo" : "Change Time Signature",
                      new Edits::TempoChanged (engine, mapBefore, engine.getTempoMap()));
        pushSongChannel();
        pushTracks();   // audio clips cover other lengths now
    }
}

//==============================================================================
//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (backgroundColour);

    if (webViewError.isNotEmpty())
    {
        g.setColour (juce::Colour (0xffd9dadc));
        g.setFont (juce::FontOptions (16.0f));
        g.drawFittedText (webViewError, getLocalBounds().reduced (40), juce::Justification::centred, 4);
    }
}

void MainComponent::paintOverChildren (juce::Graphics& g)
{
    // What the window paints itself before WebView2 shows anything (the browser's own stand-in is
    // white) is the splash's background, in case it's painted before the splash's window is there.
    if (splash != nullptr)
        Splash::paintBackground (g, getLocalBounds().toFloat());
}

void MainComponent::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());

    // The window resized while the page loads: the splash still covers it.
    if (splash != nullptr && splash->isOnDesktop())
        splash->cover();
}
