// D:\workspace\Fanan_web_hub\src\CreatorPlugin.cpp

#include "CreatorPlugin.h"

#include "GuiLayout.h"
#include "Looks.h"
#include "PluginCanvas.h"
#include "PresetBank.h"
#include "Settings.h"
#include "StellaLookAndFeel.h"
#include "WasmCompiler.h"
#include "WasmElements.h"

#include <algorithm>
#include <set>

namespace
{
    constexpr const char* infoFileName = "plugin.json";
    constexpr const char* wasmFileName = "plugin.wasm";
    constexpr const char* descriptionFileName = "description.json";
    constexpr const char* elementsFileName = "elements.wasm";
    constexpr const char* guiFolderName = "gui";
    constexpr const char* formatTag = "Sonify plugin";

    std::vector<std::uint8_t> readBytes (const juce::File& file)
    {
        juce::MemoryBlock data;

        if (! file.existsAsFile() || ! file.loadFileAsData (data))
            return {};

        const auto* start = static_cast<const std::uint8_t*> (data.getData());
        return std::vector<std::uint8_t> (start, start + data.getSize());
    }

    bool writeBytes (const juce::File& file, const std::vector<std::uint8_t>& bytes)
    {
        return file.replaceWithData (bytes.data(), bytes.size());
    }

    /** The Browser's words for a made plugin's type: an instrument, an effect, or a widget
        (anything else, on a track like an effect: the sound goes through it). */
    juce::StringArray featuresFor (const juce::String& kind)
    {
        if (kind == "effect")  return { "audio-effect" };
        if (kind == "widget")  return { "audio-effect", "utility" };
        return { "instrument", "synthesizer" };
    }

    /** A parameter's number for songs and automation, from its id: the same on every computer
        and in every version that keeps the id. */
    uint32_t stableId (const juce::String& text)
    {
        uint32_t hash = 2166136261u;

        for (const auto* p = text.toRawUTF8(); *p != 0; ++p)
            hash = (hash ^ (uint32_t) (std::uint8_t) *p) * 16777619u;

        return (hash & 0x7fffffffu) | 1u;
    }

    juce::String valueText (double value)
    {
        const auto size = std::abs (value);
        return juce::String (value, size >= 100.0 ? 0 : (size >= 10.0 ? 1 : 2));
    }

    /** The pictures in a made plugin's own looks (its knobs', sliders' and switches' Image
        layers), pinned to its own gui/images: in the studio a bare name is found in the open
        project's pictures, which a plugin on a track never is. Rewrites only what changed (a
        name, or a pinned path whose folder has moved since). */
    void pinLookPictures (const juce::File& gui)
    {
        const auto images = gui.getChildFile (GuiLayout::imagesFolder);
        const auto looksFolder = gui.getChildFile (Looks::folderName);

        if (! looksFolder.isDirectory())
            return;

        for (const auto& file : looksFolder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + Looks::fileExtension))
        {
            Looks::Look look;

            if (! Looks::readFile (file, look))
                continue;

            bool changed = false;

            for (auto& layer : look.doc.layers)
            {
                auto& picture = layer.prim.imageFile;

                if (layer.prim.type != PrimType::Image || picture.trim().isEmpty())
                    continue;

                const auto name = juce::File::isAbsolutePath (picture) ? juce::File (picture).getFileName() : picture.trim();
                const auto own = images.getChildFile (name);

                if (own.existsAsFile() && picture != own.getFullPathName())
                {
                    picture = own.getFullPathName();
                    changed = true;
                }
            }

            if (changed)
                Looks::writeFile (file, look);
        }
    }
}

//==============================================================================
juce::File MadePlugins::folder()
{
    return Settings::getDataFolder().getChildFile ("Plugins");
}

bool MadePlugins::isMadeFile (const juce::String& path)
{
    return path.trimCharactersAtEnd ("\\/").endsWithIgnoreCase (extension);
}

juce::Array<juce::File> MadePlugins::installed()
{
    juce::Array<juce::File> found;
    const auto home = folder();

    if (! home.isDirectory())
        return found;

    for (const auto& item : juce::RangedDirectoryIterator (home, false, juce::String ("*") + extension, juce::File::findDirectories))
        if (item.getFile().getChildFile (infoFileName).existsAsFile())
            found.add (item.getFile());

    found.sort();
    return found;
}

juce::File MadePlugins::findInstalled (const juce::String& id)
{
    for (const auto& existing : installed())
    {
        const auto info = juce::JSON::parse (existing.getChildFile (infoFileName));

        if (info.getProperty ("id", {}).toString() == id)
            return existing;
    }

    return {};
}

int MadePlugins::installedVersion (const juce::String& id)
{
    const auto existing = findInstalled (id);

    if (existing == juce::File())
        return 0;

    const auto info = juce::JSON::parse (existing.getChildFile (infoFileName));
    return juce::jmax (1, (int) info.getProperty ("version", 1));
}

juce::Result MadePlugins::installUnpacked (const juce::File& unpacked, const juce::String& id, juce::File& installedFolder, bool& replaced)
{
    replaced = false;
    const auto info = juce::JSON::parse (unpacked.getChildFile (infoFileName));

    if (info.getProperty ("format", {}).toString() != formatTag || info.getProperty ("id", {}).toString() != id
        || ! unpacked.getChildFile (wasmFileName).existsAsFile())
        return juce::Result::fail ("The package isn't a plugin made in Sonify.");

    auto target = findInstalled (id);
    replaced = target != juce::File();

    if (! replaced)
    {
        const auto name = info.getProperty ("name", "Store plugin").toString().trim();
        const auto base = juce::File::createLegalFileName (name.isNotEmpty() ? name : juce::String ("Store plugin"));
        target = folder().getChildFile (base + extension);

        for (int n = 2; target.exists(); ++n)
            target = folder().getChildFile (base + " " + juce::String (n) + extension);
    }

    if (! target.createDirectory())
        return juce::Result::fail ("Couldn't make the folder " + target.getFullPathName());

    // What the new version doesn't have goes; plugin.json comes last, so a folder that's only
    // half there is never taken for a plugin.
    for (const auto& old : target.findChildFiles (juce::File::findFilesAndDirectories, false))
        if (! unpacked.getChildFile (old.getFileName()).exists() || old.isDirectory())
            old.deleteRecursively();

    for (const auto& item : unpacked.findChildFiles (juce::File::findFilesAndDirectories, false))
    {
        if (item.getFileName() == infoFileName)
            continue;

        const auto copied = item.isDirectory() ? item.copyDirectoryTo (target.getChildFile (item.getFileName()))
                                               : item.copyFileTo (target.getChildFile (item.getFileName()));

        if (! copied)
            return juce::Result::fail ("Couldn't write " + item.getFileName() + " in " + target.getFullPathName());
    }

    if (! unpacked.getChildFile (infoFileName).copyFileTo (target.getChildFile (infoFileName)))
        return juce::Result::fail ("Couldn't write " + juce::String (infoFileName) + " in " + target.getFullPathName());

    installedFolder = target;
    return juce::Result::ok();
}

std::vector<Plugin::Descriptor> MadePlugins::describe (const juce::File& pluginFolder)
{
    const auto info = juce::JSON::parse (pluginFolder.getChildFile (infoFileName));
    const auto id = info.getProperty ("id", {}).toString();

    if (info.getProperty ("format", {}).toString() != formatTag || ! id.startsWith (idPrefix))
        return {};

    Plugin::Descriptor descriptor;
    descriptor.id = id;
    descriptor.name = info.getProperty ("name", pluginFolder.getFileNameWithoutExtension()).toString();
    descriptor.vendor = info.getProperty ("vendor", "Made in Sonify").toString();
    descriptor.version = juce::String ((int) info.getProperty ("version", 1));
    descriptor.features = featuresFor (info.getProperty ("kind", "instrument").toString());
    return { descriptor };
}

juce::Result MadePlugins::install (const Package& package, juce::File& installedFolder, bool& replaced)
{
    replaced = false;

    if (package.wasm.empty() || package.projectId.isEmpty())
        return juce::Result::fail ("There's nothing to add: the plugin hasn't been built.");

    const auto id = juce::String (idPrefix) + package.projectId;
    const auto name = package.name.trim().isNotEmpty() ? package.name.trim() : juce::String ("My Plugin");
    juce::File target;
    int version = 1;

    // The folder it went into before (by its id: a new name doesn't make it another plugin),
    // or a new one named after it.
    for (const auto& existing : installed())
    {
        const auto info = juce::JSON::parse (existing.getChildFile (infoFileName));

        if (info.getProperty ("id", {}).toString() == id)
        {
            target = existing;
            version = (int) info.getProperty ("version", 0) + 1;
            replaced = true;
            break;
        }
    }

    if (target == juce::File())
    {
        const auto base = juce::File::createLegalFileName (name);
        target = folder().getChildFile (base + extension);

        for (int n = 2; target.exists(); ++n)
            target = folder().getChildFile (base + " " + juce::String (n) + extension);
    }

    if (const auto written = writeFolder (package, target, version); written.failed())
        return written;

    installedFolder = target;
    return juce::Result::ok();
}

juce::Result MadePlugins::writeFolder (const Package& package, const juce::File& target, int version)
{
    if (package.wasm.empty() || package.projectId.isEmpty())
        return juce::Result::fail ("There's nothing to write: the plugin hasn't been built.");

    const auto id = juce::String (idPrefix) + package.projectId;
    const auto name = package.name.trim().isNotEmpty() ? package.name.trim() : juce::String ("My Plugin");

    if (! target.createDirectory())
        return juce::Result::fail ("Couldn't make the folder " + target.getFullPathName());

    const auto cantWrite = [&target] (const char* file)
    {
        return juce::Result::fail ("Couldn't write " + juce::String (file) + " in " + target.getFullPathName());
    };

    if (! writeBytes (target.getChildFile (wasmFileName), package.wasm))
        return cantWrite (wasmFileName);

    if (! package.elementsWasm.empty())
    {
        if (! writeBytes (target.getChildFile (elementsFileName), package.elementsWasm))
            return cantWrite (elementsFileName);
    }
    else
    {
        target.getChildFile (elementsFileName).deleteFile();
    }

    if (! target.getChildFile (descriptionFileName).replaceWithText (juce::String (juce::CharPointer_UTF8 (package.description.c_str()))))
        return cantWrite (descriptionFileName);

    const auto presets = target.getChildFile (PresetBank::fileName);

    if (package.presetsFile.existsAsFile())
    {
        if (! package.presetsFile.copyFileTo (presets))
            return cantWrite (PresetBank::fileName);
    }
    else
    {
        presets.deleteFile();
    }

    // The panel: its layout, pictures and looks (the studio's view of it stays behind).
    const auto gui = target.getChildFile (guiFolderName);
    gui.deleteRecursively();

    if (package.guiFolder.isDirectory() && ! package.guiFolder.copyDirectoryTo (gui))
        return cantWrite (guiFolderName);

    gui.getChildFile ("view.json").deleteFile();
    pinLookPictures (gui);

    // A sampler's library: its blobs by name (and, for this computer, where they are; never for others).
    if (! PluginLibrary::writeMade (target, package.library, ! package.forOthers))
        return cantWrite (PluginLibrary::fileName);

    // A protected plugin's license.soni, in its folder (one left from before goes when it isn't).
    const auto soniFile = target.getChildFile (stella::licence::fileName);

    if (! package.licence.isEmpty())
    {
        if (! soniFile.replaceWithData (package.licence.getData(), package.licence.getSize()))
            return cantWrite (stella::licence::fileName);
    }
    else
    {
        soniFile.deleteFile();
    }

    // Last, so a folder that's only half there is never taken for a plugin.
    juce::DynamicObject::Ptr info (new juce::DynamicObject());
    info->setProperty ("format", formatTag);
    info->setProperty ("formatVersion", 1);
    info->setProperty ("id", id);
    info->setProperty ("name", name);
    info->setProperty ("vendor", package.vendor.trim().isNotEmpty() ? package.vendor.trim() : juce::String ("Made in Sonify"));
    info->setProperty ("kind", package.kind == "effect" || package.kind == "widget" ? package.kind : juce::String ("instrument"));
    info->setProperty ("version", version);
    info->setProperty ("added", juce::Time::getCurrentTime().toISO8601 (true));
    info->setProperty ("project", package.projectId);

    if (! target.getChildFile (infoFileName).replaceWithText (juce::JSON::toString (juce::var (info.get()))))
        return cantWrite (infoFileName);

    return juce::Result::ok();
}

juce::Result MadePlugins::protect (Package& package, const juce::File& projectFolder, const Licences::ProjectProtection& protection,
                                  const std::atomic<bool>* cancelled)
{
    if (! protection.isValid())
        return juce::Result::fail ("Its protection isn't turned on.");

    // Built again with its product's id and its demo inside.
    const auto compiled = WasmCompiler::compile (projectFolder, package.projectId,
                                                 WasmCompiler::Protection (protection.product, protection.playSeconds, protection.silentSeconds),
                                                 cancelled);

    if (! compiled.ok)
        return juce::Result::fail ("The protected build didn't work. Ask Fanan AI to look at the build log, then try again.\n\n"
                                   + compiled.log.trim().substring (0, 1500));

    const auto wasm = readBytes (compiled.wasm);

    // It has to start, and say it's this product.
    std::string error;
    const auto plugin = WasmPlugin::load (wasm, 48000.0, 512, error);

    if (plugin == nullptr)
        return juce::Result::fail ("The protected build doesn't start: " + juce::String (error));

    const auto description = plugin->getDescription();

    if (juce::JSON::parse (juce::String (juce::CharPointer_UTF8 (description.c_str()))).getProperty ("product", {}).toString() != protection.product)
        return juce::Result::fail ("The protected build didn't take its product id. Please try again.");

    package.wasm = wasm;
    package.description = description;
    package.licence = Licences::soniData (Licences::soniFor (protection, package.name, package.vendor));
    return juce::Result::ok();
}

//==============================================================================
namespace
{
    /** Under a protected plugin's panel while it plays its demo: what the demo does, and Register. */
    class DemoBar final : public juce::Component
    {
    public:
        DemoBar()
        {
            registerButton.setColour (juce::TextButton::buttonColourId, Theme::accent);
            registerButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
            registerButton.setTooltip ("Register it on this computer with the serial you got from its maker");
            addAndMakeVisible (registerButton);
        }

        void setDemo (double playSeconds, double silentSeconds)
        {
            auto seconds = [] (double value)
            {
                const auto tenths = juce::roundToInt (value * 10.0);
                return tenths % 10 == 0 ? juce::String (tenths / 10) : juce::String (tenths / 10.0, 1);
            };

            text = seconds (silentSeconds) + " seconds of silence every " + seconds (playSeconds) + " seconds, until it's registered";
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (0xff221d0e));
            g.setColour (juce::Colour (0xffffb300).withAlpha (0.35f));
            g.fillRect (0, 0, getWidth(), 1);

            auto area = getLocalBounds().reduced (12, 0).withTrimmedRight (registerButton.getWidth() + 12);
            g.setColour (juce::Colour (0xffffc940));
            g.setFont (Theme::font (13.0f, true));
            g.drawText ("DEMO", area.removeFromLeft (48), juce::Justification::centredLeft, false);
            g.setColour (juce::Colour (0xffe9e4d6));
            g.setFont (Theme::font (12.5f));
            g.drawFittedText (text, area, juce::Justification::centredLeft, 1, 0.8f);
        }

        void resized() override
        {
            registerButton.setBounds (getLocalBounds().removeFromRight (juce::jmin (130, getWidth() / 3)).reduced (6, 5));
        }

        juce::TextButton registerButton { juce::String::fromUTF8 ("Register\xe2\x80\xa6") };

    private:
        juce::String text { "Silent now and then, until it's registered" };
    };

    /** Over a sampler's panel while blobs of its sample library aren't on this computer: which,
        where they go, and Locate... for the folder they're in. */
    class LibraryBar final : public juce::Component
    {
    public:
        LibraryBar()
        {
            locate.setColour (juce::TextButton::buttonColourId, Theme::raised);
            locate.setTooltip ("The folder its blobs are in: found there from now on");
            addAndMakeVisible (locate);
        }

        void setMissing (const juce::StringArray& names, const juce::File& home)
        {
            text = (names.size() == 1 ? names[0] + " isn't" : names.joinIntoString (", ") + " aren't") + " on this computer: put "
                 + (names.size() == 1 ? "its blob" : "their blobs") + " in " + home.getFullPathName() + ", or Locate their folder.";
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (0xff10202a));
            g.setColour (juce::Colour (0xff4fc3f7).withAlpha (0.35f));
            g.fillRect (0, getHeight() - 1, getWidth(), 1);

            auto area = getLocalBounds().reduced (12, 0).withTrimmedRight (locate.getWidth() + 12);
            g.setColour (juce::Colour (0xff81d4fa));
            g.setFont (Theme::font (13.0f, true));
            g.drawText ("SAMPLES", area.removeFromLeft (72), juce::Justification::centredLeft, false);
            g.setColour (juce::Colour (0xffe3eef3));
            g.setFont (Theme::font (12.5f));
            g.drawFittedText (text, area, juce::Justification::centredLeft, 2, 0.75f);
        }

        void resized() override
        {
            locate.setBounds (getLocalBounds().removeFromRight (juce::jmin (110, getWidth() / 4)).reduced (6, 5));
        }

        juce::TextButton locate { juce::String::fromUTF8 ("Locate\xe2\x80\xa6") };

    private:
        juce::String text;
    };

    /** A plugin's window content: its panel, the demo's bar under it while it's in its demo, and
        the sample library's over it while blobs are missing. */
    class PanelHolder final : public juce::Component
    {
    public:
        PanelHolder (juce::Component& panelToShow, juce::Component& barToShow, juce::Component& topBarToShow)
            : panel (panelToShow), bar (barToShow), topBar (topBarToShow)
        {
            addAndMakeVisible (panel);
            addChildComponent (bar);
            addChildComponent (topBar);
        }

        void resized() override
        {
            auto area = getLocalBounds();

            if (topBar.isVisible())
                topBar.setBounds (area.removeFromTop (barHeight));

            if (bar.isVisible())
                bar.setBounds (area.removeFromBottom (barHeight));

            panel.setBounds (area);
        }

        static constexpr int barHeight = 34;

    private:
        juce::Component& panel;
        juce::Component& bar;
        juce::Component& topBar;
    };
}

//==============================================================================
/** The plugin's own window: its panel, as designed in Edit UI, playing (with the demo's bar
    and Register under it while a protected plugin plays its demo; its registration badge opens
    the Registration window too). Hides when closed, and goes with the plugin. */
class CreatorPlugin::Screen final : public juce::DocumentWindow,
                                    private juce::Timer
{
public:
    explicit Screen (CreatorPlugin& owner)
        : DocumentWindow (owner.descriptor.name, juce::Colours::black, DocumentWindow::closeButton),
          plugin (owner)
    {
        setUsingNativeTitleBar (true);

        const auto gui = plugin.sourceFolder.getChildFile (guiFolderName);
        pinLookPictures (gui);   // the folder may have moved since it was added
        looks.setGuiFolder (gui);

        canvas.setLooks (looks);
        canvas.setPanelOnly (true);
        canvas.setGuiFolder (gui);

        // Its panel; a plain automatic one if it never had one.
        GuiLayout layout;

        if (GuiLayout::load (gui.getChildFile (GuiLayout::fileName), layout).failed() || layout.width <= 0 || layout.height <= 0)
        {
            juce::Array<GuiLayout::ParamInfo> paramInfos;

            for (const auto& p : plugin.infos)
                paramInfos.add ({ p.id, p.module, p.name, p.unit, p.min, p.max, p.def });

            layout = GuiLayout::makeDefault (paramInfos, plugin.descriptor.name);
        }

        canvas.onParameterChanged = [this] (int pluginIndex, float value)
        {
            if (const auto index = plugin.indexOfPluginParameter (pluginIndex); index >= 0)
                plugin.setValue (index, value, true);

            // The chosen preset's values aren't its own any more: it shows a *.
            if (plugin.presets.getCurrent() >= 0 && ! plugin.presets.isModified())
            {
                plugin.presets.setModified (true);
                canvas.setPresets (plugin.presets.getMenu());
            }
        };

        canvas.onNote = [this] (int note, float velocity)
        {
            if (velocity > 0.0f)
                plugin.keyboardState.noteOn (1, note, velocity);
            else
                plugin.keyboardState.noteOff (1, note, 0.0f);
        };

        canvas.isNoteDown = [this] (int note) { return plugin.keyboardState.isNoteOnForChannels (0xffff, note); };

        // The preset manager: the plugin's presets, the user's own (Save, Save as, Load), banks.
        presetUi.getValues = [this] { return plugin.currentValues(); };
        presetUi.apply = [this] (const Preset& preset)
        {
            plugin.applyPreset (preset, true);
            pushParameters (true);
        };
        presetUi.changed = [this]
        {
            canvas.setPresets (plugin.presets.getMenu());
            plugin.stateChanged.store (true);
        };
        canvas.onPresetChosen = [this] (int index) { presetUi.choose (index); };
        canvas.onPresetAction = [this] (PresetAction action) { presetUi.perform (action); };
        canvas.readLevel = [this] (const juce::String& source, bool rms) { return plugin.takeLevel (source, rms); };
        canvas.readScope = [this] (const juce::String& source, float* destination, int numSamples) { plugin.readScope (source, destination, numSamples); };
        canvas.onSourcesChanged = [this] (const juce::StringArray& levels, const juce::StringArray& scopes) { plugin.setScreenSources (levels, scopes); };

        // The panel's programmed elements, in a sandbox of their own.
        if (const auto bytes = readBytes (plugin.sourceFolder.getChildFile (elementsFileName)); ! bytes.empty())
        {
            std::string error;
            std::shared_ptr<WasmElements> elements (WasmElements::load (bytes, error));
            canvas.setElements (elements);
        }

        canvas.setLayout (layout);
        pushParameters (true);

        canvas.setPresets (plugin.presets.getMenu());

        // A protected plugin, not registered here: the demo's bar, with Register (and its
        // registration badge opens the Registration window too, until it's registered).
        demoBar.setDemo (plugin.soni.isValid() ? plugin.soni.playSeconds : stella::licence::playSeconds,
                         plugin.soni.isValid() ? plugin.soni.silentSeconds : stella::licence::silentSeconds);
        demoBar.registerButton.onClick = [this] { plugin.showRegistration(); };
        demoBar.setVisible (plugin.isInDemo());

        if (plugin.isProtected())
        {
            canvas.onRegistrationClicked = [this] { plugin.showRegistration(); };
            canvas.setRegistered (! plugin.isInDemo());
        }

        // A sampler whose blobs aren't here: which, and Locate...
        libraryBar.locate.onClick = [this] { locateLibrary(); };
        libraryBar.setVisible (! plugin.getMissingInstruments().isEmpty());
        libraryBar.setMissing (plugin.getMissingInstruments(), PluginLibrary::homeFolder (plugin.descriptor.vendor, plugin.descriptor.name));

        holder.setSize (layout.width, layout.height + (demoBar.isVisible() ? PanelHolder::barHeight : 0)
                                                    + (libraryBar.isVisible() ? PanelHolder::barHeight : 0));
        setContentNonOwned (&holder, true);
        setResizable (true, false);
        setResizeLimits (juce::jmax (120, layout.width / 4), juce::jmax (90, layout.height / 4),
                         layout.width * 3, layout.height * 3 + 2 * PanelHolder::barHeight);
        centreWithSize (getWidth(), getHeight());

        startTimerHz (15);
    }

    ~Screen() override
    {
        stopTimer();
        presetUi.getValues = nullptr;
        presetUi.apply = nullptr;
        presetUi.changed = nullptr;
        clearContentComponent();   // the canvas is a member: let go of it before it goes
    }

    void show (const juce::String& title)
    {
        setName (title);
        pushParameters (true);

        // Presets saved elsewhere since (the studio, another track) show up.
        plugin.presets.refresh();
        canvas.setPresets (plugin.presets.getMenu());

        setVisible (true);
        toFront (true);
    }

    void closeButtonPressed() override
    {
        setVisible (false);
    }

private:
    void timerCallback() override
    {
        // Automation, the Hub's controls and presets move its knobs too (and an undo, its preset).
        if (isVisible())
        {
            pushParameters (false);
            canvas.setPresets (plugin.presets.getMenu());
        }

        // Registered meanwhile: the demo's bar and the badge go.
        if (const bool demo = plugin.isInDemo(); demo != demoBar.isVisible())
        {
            canvas.setRegistered (! demo);
            demoBar.setVisible (demo);
            setSize (getWidth(), getHeight() + (demo ? PanelHolder::barHeight : -PanelHolder::barHeight));
            holder.resized();
        }

        // Its blobs found meanwhile (Locate...): the library's bar goes.
        if (const bool missing = ! plugin.getMissingInstruments().isEmpty(); missing != libraryBar.isVisible())
        {
            libraryBar.setVisible (missing);
            setSize (getWidth(), getHeight() + (missing ? PanelHolder::barHeight : -PanelHolder::barHeight));
            holder.resized();
        }
    }

    void locateLibrary()
    {
        chooser = std::make_unique<juce::FileChooser> ("The folder " + plugin.descriptor.name + "'s blobs are in",
                                                       PluginLibrary::homeFolder (plugin.descriptor.vendor, plugin.descriptor.name).getParentDirectory());

        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [safe = juce::Component::SafePointer<Screen> (this)] (const juce::FileChooser& fc)
                              {
                                  if (safe == nullptr || fc.getResult() == juce::File())
                                      return;

                                  safe->plugin.locateLibrary (fc.getResult());

                                  if (const auto missing = safe->plugin.getMissingInstruments(); ! missing.isEmpty())
                                  {
                                      safe->libraryBar.setMissing (missing, PluginLibrary::homeFolder (safe->plugin.descriptor.vendor,
                                                                                                       safe->plugin.descriptor.name));
                                      juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Not there",
                                                                              missing.joinIntoString (", ") + (missing.size() == 1 ? " isn't" : " aren't")
                                                                              + " in " + fc.getResult().getFullPathName() + " either.",
                                                                              "OK", safe.getComponent());
                                  }
                              });
    }

    void pushParameters (bool always)
    {
        const auto count = plugin.infos.size();
        bool changed = always || shown.size() != count;

        for (size_t i = 0; i < count && ! changed; ++i)
            changed = std::abs (shown[i] - plugin.values[i].load (std::memory_order_relaxed)) > 1.0e-6f;

        if (! changed)
            return;

        shown.resize (count);
        juce::Array<PluginCanvas::Param> params;

        for (size_t i = 0; i < count; ++i)
        {
            const auto& info = plugin.infos[i];
            shown[i] = plugin.values[i].load (std::memory_order_relaxed);

            PluginCanvas::Param p;
            p.index = info.index;
            p.id = info.id;
            p.name = info.name;
            p.unit = info.unit;
            p.min = info.min;
            p.max = info.max;
            p.def = info.def;
            p.skew = info.skew;
            p.value = shown[i];
            params.add (p);
        }

        canvas.setParameters (params);
    }

    CreatorPlugin& plugin;
    Looks looks;               // before the canvas, which draws with it
    PluginCanvas canvas;
    DemoBar demoBar;
    LibraryBar libraryBar;
    PanelHolder holder { canvas, demoBar, libraryBar };
    PresetUi presetUi { *this, plugin.presets };
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<float> shown;  // the values the canvas has

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Screen)
};

//==============================================================================
std::unique_ptr<Plugin> CreatorPlugin::load (const juce::File& pluginFolder, juce::String& error)
{
    const auto descriptors = MadePlugins::describe (pluginFolder);

    if (descriptors.empty())
    {
        error = "It isn't a plugin made in Sonify, or its plugin.json is damaged: " + pluginFolder.getFullPathName();
        return nullptr;
    }

    auto bytes = readBytes (pluginFolder.getChildFile (wasmFileName));

    if (bytes.empty())
    {
        error = descriptors.front().name + " is missing its plugin.wasm: add it to Sonify again from the Plugins Creator Studio.";
        return nullptr;
    }

    std::unique_ptr<CreatorPlugin> plugin (new CreatorPlugin());
    plugin->descriptor = descriptors.front();
    plugin->inFile = descriptors;
    plugin->sourceFolder = pluginFolder;
    plugin->instrument = juce::JSON::parse (pluginFolder.getChildFile (infoFileName)).getProperty ("kind", "instrument").toString() == "instrument";
    plugin->wasmBytes = std::move (bytes);
    plugin->library = PluginLibrary::load (pluginFolder);
    plugin->findLibrary();

    // What the plugin said of itself when it was added: its parameters, modules and displays.
    const auto description = juce::JSON::parse (pluginFolder.getChildFile (descriptionFileName));
    std::set<uint32_t> usedIds;

    // Protected: it plays its demo until it's registered on this computer. The id its own
    // WebAssembly carries comes first (its files can be changed, its build can't); its
    // license.soni (in its folder) is what registers it.
    stella::licence::Soni soni;
    const bool hasSoni = Licences::readSoni (pluginFolder.getChildFile (stella::licence::fileName), soni);
    const auto builtProduct = description.getProperty ("product", {}).toString().trim();
    plugin->protectedBuild = builtProduct.isNotEmpty();
    plugin->productId = plugin->protectedBuild ? builtProduct : (hasSoni ? juce::String (soni.product) : juce::String());

    if (hasSoni && juce::String (soni.product) == plugin->productId)
        plugin->soni = soni;

    if (plugin->productId.isNotEmpty())
    {
        Licences::isRegistered (plugin->soni);   // its flag follows what's found now
        plugin->licensed = Licences::flagFor (plugin->productId);
        plugin->licenceToken = Licences::runtimeToken (plugin->productId);

        if (plugin->soni.isValid())
            plugin->demoGate.setTimes (plugin->soni.playSeconds, plugin->soni.silentSeconds);
    }

    if (const auto* list = description.getProperty ("params", {}).getArray())
    {
        for (const auto& item : *list)
        {
            Info info;
            info.index = (int) item.getProperty ("index", 0);
            info.id = item.getProperty ("id", {}).toString();
            info.name = item.getProperty ("name", {}).toString();
            info.module = item.getProperty ("module", {}).toString();
            info.unit = item.getProperty ("unit", {}).toString();
            info.min = (float) (double) item.getProperty ("min", 0.0);
            info.max = (float) (double) item.getProperty ("max", 1.0);
            info.def = (float) (double) item.getProperty ("def", 0.0);
            info.skew = (float) (double) item.getProperty ("skew", 1.0);

            if (! (info.max > info.min) || info.id.isEmpty())
                continue;

            Parameter parameter;
            parameter.id = stableId (info.id);

            while (! usedIds.insert (parameter.id).second)   // two ids that read the same: the next number
                parameter.id = (parameter.id + 1) & 0x7fffffffu;

            parameter.name = info.name.isNotEmpty() ? info.name : info.id;
            parameter.minValue = info.min;
            parameter.maxValue = info.max;
            parameter.defaultValue = juce::jlimit (info.min, info.max, info.def);

            plugin->infos.push_back (info);
            plugin->parameters.push_back (parameter);
        }
    }

    plugin->values = std::make_unique<std::atomic<float>[]> (juce::jmax ((size_t) 1, plugin->infos.size()));

    for (size_t i = 0; i < plugin->infos.size(); ++i)
        plugin->values[i].store (juce::jlimit (plugin->infos[i].min, plugin->infos[i].max, plugin->infos[i].def));

    // Its presets: the factory ones it came with, and the user's own by its name. A new one
    // starts on the first (a song's state, loaded next, brings back what it was left on).
    plugin->presets.setPlugin (plugin->descriptor.name, pluginFolder.getChildFile (PresetBank::fileName), false);

    if (! plugin->presets.isEmpty())
    {
        plugin->presets.choose (0);
        plugin->applyPreset (plugin->presets.getEntries().front().preset, false);
    }

    if (const auto* list = description.getProperty ("modules", {}).getArray())
    {
        for (int i = 0; i < list->size(); ++i)
        {
            const auto& item = list->getReference (i);
            ModuleInfo module;
            module.index = i;
            module.id = item.getProperty ("id", {}).toString();

            if (const auto* outs = item.getProperty ("outputs", {}).getArray())
                for (const auto& port : *outs)
                    module.outputs.add (port.toString());

            plugin->modules.push_back (module);
        }
    }

    if (const auto* list = description.getProperty ("displays", {}).getArray())
        for (const auto& item : *list)
            plugin->displays.push_back ({ (int) item.getProperty ("index", 0), item.getProperty ("id", {}).toString() });

    return plugin;
}

CreatorPlugin::~CreatorPlugin()
{
    screen.reset();   // the window goes before the plugin it shows
    deactivate();
}

//==============================================================================
bool CreatorPlugin::saveState (juce::MemoryBlock& destination)
{
    juce::DynamicObject::Ptr saved (new juce::DynamicObject());

    for (size_t i = 0; i < infos.size(); ++i)
        saved->setProperty (infos[i].id, (double) this->values[i].load());

    juce::DynamicObject::Ptr state (new juce::DynamicObject());
    state->setProperty ("version", descriptor.version);
    state->setProperty ("values", juce::var (saved.get()));

    // The preset it's on (and whether its values changed since), as a DAW keeps it.
    if (const auto* chosen = presets.getCurrentEntry())
    {
        state->setProperty ("preset", chosen->preset.name);
        state->setProperty ("presetGroup", chosen->group);
        state->setProperty ("presetModified", presets.isModified());
    }

    const auto text = juce::JSON::toString (juce::var (state.get()), true);
    destination.replaceAll (text.toRawUTF8(), text.getNumBytesAsUTF8());
    return true;
}

bool CreatorPlugin::loadState (const juce::MemoryBlock& source)
{
    const auto state = juce::JSON::parse (source.toString());
    const auto saved = state.getProperty ("values", {});

    if (! saved.isObject())
        return false;

    // Matched by id: parameters a newer version added keep their defaults.
    for (size_t i = 0; i < infos.size(); ++i)
    {
        const auto value = saved.hasProperty (infos[i].id) ? (float) (double) saved.getProperty (infos[i].id, infos[i].def) : infos[i].def;
        setValue ((int) i, juce::jlimit (infos[i].min, infos[i].max, value), false);
    }

    // The preset it was on, if it's still there.
    presets.restore (state.getProperty ("preset", {}).toString(), state.getProperty ("presetGroup", {}).toString(),
                     static_cast<bool> (state.getProperty ("presetModified", false)));
    return true;
}

void CreatorPlugin::applyPreset (const Preset& preset, bool byUser)
{
    // Its values by id; parameters it doesn't name go back to their defaults.
    for (size_t i = 0; i < infos.size(); ++i)
    {
        const auto found = preset.values.find (infos[i].id);
        const auto value = found != preset.values.end() ? found->second : infos[i].def;
        setValue ((int) i, juce::jlimit (infos[i].min, infos[i].max, value), false);
    }

    if (byUser)
        stateChanged.store (true);
}

std::map<juce::String, float> CreatorPlugin::currentValues() const
{
    std::map<juce::String, float> current;

    for (size_t i = 0; i < infos.size(); ++i)
        current[infos[i].id] = values[i].load (std::memory_order_relaxed);

    return current;
}

//==============================================================================
int CreatorPlugin::findParameter (uint32_t id) const noexcept
{
    for (size_t i = 0; i < parameters.size(); ++i)
        if (parameters[i].id == id)
            return (int) i;

    return -1;
}

int CreatorPlugin::indexOfPluginParameter (int pluginIndex) const noexcept
{
    for (size_t i = 0; i < infos.size(); ++i)
        if (infos[i].index == pluginIndex)
            return (int) i;

    return -1;
}

double CreatorPlugin::getParameterValue (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, (int) infos.size()) ? (double) values[(size_t) index].load (std::memory_order_relaxed) : 0.0;
}

juce::String CreatorPlugin::getParameterText (int index, double value)
{
    if (! juce::isPositiveAndBelow (index, (int) infos.size()))
        return {};

    const auto& unit = infos[(size_t) index].unit;
    return unit.isNotEmpty() ? valueText (value) + " " + unit : valueText (value);
}

double CreatorPlugin::queueParameter (int index, double value)
{
    if (! juce::isPositiveAndBelow (index, (int) infos.size()))
        return 0.0;

    const auto& info = infos[(size_t) index];
    const auto clamped = juce::jlimit (info.min, info.max, (float) value);
    setValue (index, clamped, false);
    stateChanged.store (true);
    return clamped;
}

void CreatorPlugin::setValue (int index, float value, bool fromScreen)
{
    // Message thread: the instance is only ever replaced here.
    values[(size_t) index].store (value, std::memory_order_relaxed);

    if (wasm != nullptr)
        wasm->setParameter (infos[(size_t) index].index, value);

    if (fromScreen)
    {
        touched.store (index);
        stateChanged.store (true);
    }
}

void CreatorPlugin::setParameterFromAudio (uint32_t paramId, int& indexHint, double value) noexcept
{
    if (! (juce::isPositiveAndBelow (indexHint, (int) parameters.size()) && parameters[(size_t) indexHint].id == paramId))
        indexHint = findParameter (paramId);

    if (indexHint < 0)
        return;

    const auto& info = infos[(size_t) indexHint];
    const auto clamped = juce::jlimit (info.min, info.max, (float) value);
    values[(size_t) indexHint].store (clamped, std::memory_order_relaxed);

    // The audio thread: the instance can't be swapped while this holds the lock.
    const juce::SpinLock::ScopedTryLockType lock (wasmLock);

    if (lock.isLocked() && wasm != nullptr)
        wasm->setParameter (info.index, clamped);
}

//==============================================================================
bool CreatorPlugin::activate (double sampleRate, int maxBlockSize)
{
    // Longer blocks are split up as they play: only a new sample rate needs a new instance.
    if (active && std::abs (sampleRate - activeRate.load()) < 0.5)
        return true;

    // A new instance at this rate (compiled once a run, so starting again is quick), with the
    // values the host holds.
    std::string error;
    auto fresh = WasmPlugin::load (wasmBytes, sampleRate, juce::jmax (32, maxBlockSize), error);

    if (fresh == nullptr)
    {
        failure = descriptor.name + " didn't start: " + juce::String (error);
        deactivate();
        return false;
    }

    for (size_t i = 0; i < infos.size(); ++i)
        fresh->setParameter (infos[i].index, values[i].load());

    // Its sample library, with its blobs as they're found now (put there since, they're found).
    findLibrary();

    {
        const juce::ScopedLock hold (libraryLock);
        fresh->setLibrary (libraryEntries);
    }

    playedMidi.ensureSize (16384);

    {
        const juce::SpinLock::ScopedLockType lock (wasmLock);
        std::swap (wasm, fresh);
    }

    fresh.reset();   // the one it replaced, if any: here, never on the audio thread

    active = true;
    activeRate.store (sampleRate);
    activeBlockSize = maxBlockSize;
    failure.clear();
    failureReported = false;
    applyScreenSources();
    return true;
}

//==============================================================================
void CreatorPlugin::findLibrary()
{
    auto found = PluginLibrary::entriesFor (library, descriptor.vendor, descriptor.name,
                                            { sourceFolder.getChildFile ("library"), sourceFolder });
    const juce::ScopedLock hold (libraryLock);
    libraryEntries = std::move (found);
}

juce::StringArray CreatorPlugin::getMissingInstruments() const
{
    juce::StringArray missing;
    const juce::ScopedLock hold (libraryLock);

    for (const auto& entry : libraryEntries)
        if (entry.path.empty())
            missing.add (juce::String (juce::CharPointer_UTF8 (entry.name.c_str())));

    return missing;
}

void CreatorPlugin::locateLibrary (const juce::File& folder)
{
    PluginLibrary::addToList (descriptor.vendor, descriptor.name, { folder.getFullPathName() });
    findLibrary();

    const juce::ScopedLock hold (libraryLock);
    const juce::SpinLock::ScopedLockType lock (wasmLock);

    if (wasm != nullptr)
        wasm->setLibrary (libraryEntries);
}

void CreatorPlugin::deactivate()
{
    std::unique_ptr<WasmPlugin> old;

    {
        const juce::SpinLock::ScopedLockType lock (wasmLock);
        old = std::move (wasm);
    }

    active = false;
    activeRate.store (0.0);
    activeBlockSize = 0;
}

WasmPlugin::Transport MadePlugins::transportOf (const clap_event_transport_t* song) noexcept
{
    WasmPlugin::Transport t;

    if (song == nullptr)
        return t;

    if ((song->flags & CLAP_TRANSPORT_HAS_TEMPO) != 0 && song->tempo > 1.0)
        t.tempo = song->tempo;

    if ((song->flags & CLAP_TRANSPORT_HAS_BEATS_TIMELINE) != 0)
    {
        t.beat = (double) song->song_pos_beats / (double) CLAP_BEATTIME_FACTOR;
        t.barStart = (double) song->bar_start / (double) CLAP_BEATTIME_FACTOR;
    }

    if ((song->flags & CLAP_TRANSPORT_HAS_TIME_SIGNATURE) != 0 && song->tsig_num > 0 && song->tsig_denom > 0)
    {
        t.numerator = song->tsig_num;
        t.denominator = song->tsig_denom;
    }

    t.playing = (song->flags & CLAP_TRANSPORT_IS_PLAYING) != 0;
    return t;
}

void CreatorPlugin::process (const float* inLeft, const float* inRight, float* outLeft, float* outRight,
                             int numSamples, const juce::MidiBuffer& midi, const clap_event_transport_t* song) noexcept
{
    const juce::SpinLock::ScopedTryLockType lock (wasmLock);

    if (! lock.isLocked() || wasm == nullptr || numSamples <= 0)
    {
        // Not playing: an instrument is silent, an effect lets the sound through.
        for (int channel = 0; channel < 2; ++channel)
        {
            auto* out = channel == 0 ? outLeft : outRight;
            const auto* in = channel == 0 ? inLeft : (inRight != nullptr ? inRight : inLeft);

            if (out == nullptr)
                continue;

            if (instrument || in == nullptr)
                juce::FloatVectorOperations::clear (out, juce::jmax (0, numSamples));
            else if (in != out)
                juce::FloatVectorOperations::copy (out, in, numSamples);
        }

        return;
    }

    // The track's notes, and the panel's keyboard (which lights up with every note played).
    playedMidi.clear();
    playedMidi.addEvents (midi, 0, numSamples, 0);
    keyboardState.processNextMidiBuffer (playedMidi, 0, numSamples, true);

    int count = 0;

    for (const auto metadata : playedMidi)
    {
        const auto* raw = metadata.data;

        if (metadata.numBytes < 1 || raw[0] >= 0xf0)
            continue;

        if (count >= maxEvents)
            break;

        events[(size_t) count++] = { (std::uint32_t) juce::jlimit (0, numSamples - 1, metadata.samplePosition),
                                     raw[0],
                                     metadata.numBytes > 1 ? raw[1] : (std::uint8_t) 0,
                                     metadata.numBytes > 2 ? raw[2] : (std::uint8_t) 0 };
    }

    wasm->setTransport (MadePlugins::transportOf (song));   // the song's tempo and position: tempo sync

    // Its registration: the plugin's own token while it's registered on this computer.
    const bool licensedHere = licensed == nullptr || licensed->load (std::memory_order_relaxed);

    if (protectedBuild)
        wasm->setLicenceToken (licensedHere ? licenceToken : 0);

    if (instrument)
        wasm->process (nullptr, nullptr, outLeft, outRight, numSamples, events.data(), count);
    else
        wasm->process (inLeft, inRight != nullptr ? inRight : inLeft, outLeft, outRight, numSamples, events.data(), count);

    // A protected plugin whose build doesn't carry the demo: Sonify plays it.
    if (licensed != nullptr && ! protectedBuild)
        demoGate.process (outLeft, outRight, numSamples, activeRate.load (std::memory_order_relaxed), licensedHere);
}

bool CreatorPlugin::isInDemo() const noexcept
{
    return licensed != nullptr && ! licensed->load (std::memory_order_relaxed);
}

void CreatorPlugin::showRegistration()
{
    auto shown = soni;

    // No license.soni with it (or another product's): the window says so, by the plugin's name.
    if (! shown.isValid())
    {
        shown = {};
        shown.product = productId.toStdString();
        shown.name = descriptor.name.toStdString();
        shown.vendor = descriptor.vendor.toStdString();
    }

    Licences::showRegistration (shown, nullptr);
}

//==============================================================================
Plugin::Requests CreatorPlugin::takeRequests() noexcept
{
    Requests requests;

    // It stopped (it crashed, or got stuck): Sonify says so once.
    if (wasm != nullptr && wasm->hasFailed() && ! failureReported)
    {
        failureReported = true;
        requests.processError = true;
    }

    return requests;
}

bool CreatorPlugin::getError (juce::String& error) const
{
    if (wasm != nullptr && wasm->hasFailed())
    {
        error = juce::String (wasm->getFailure());
        return true;
    }

    if (failure.isNotEmpty())
    {
        error = failure;
        return true;
    }

    return false;
}

//==============================================================================
juce::Image CreatorPlugin::drawPanel (int maxWidth)
{
    // As its window sets it up (Screen), without the window.
    const auto gui = sourceFolder.getChildFile (guiFolderName);
    pinLookPictures (gui);

    Looks looks;               // before the canvas, which draws with it
    looks.setGuiFolder (gui);

    PluginCanvas canvas;
    canvas.setLooks (looks);
    canvas.setPanelOnly (true);
    canvas.setGuiFolder (gui);

    GuiLayout layout;

    if (GuiLayout::load (gui.getChildFile (GuiLayout::fileName), layout).failed() || layout.width <= 0 || layout.height <= 0)
    {
        juce::Array<GuiLayout::ParamInfo> paramInfos;

        for (const auto& p : infos)
            paramInfos.add ({ p.id, p.module, p.name, p.unit, p.min, p.max, p.def });

        layout = GuiLayout::makeDefault (paramInfos, descriptor.name);
    }

    if (layout.width <= 0 || layout.height <= 0)
        return {};

    if (const auto bytes = readBytes (sourceFolder.getChildFile (elementsFileName)); ! bytes.empty())
    {
        std::string error;
        std::shared_ptr<WasmElements> elements (WasmElements::load (bytes, error));
        canvas.setElements (elements);
    }

    canvas.setLayout (layout);

    juce::Array<PluginCanvas::Param> params;

    for (size_t i = 0; i < infos.size(); ++i)
    {
        const auto& info = infos[i];
        PluginCanvas::Param p;
        p.index = info.index;
        p.id = info.id;
        p.name = info.name;
        p.unit = info.unit;
        p.min = info.min;
        p.max = info.max;
        p.def = info.def;
        p.skew = info.skew;
        p.value = values[i].load (std::memory_order_relaxed);
        params.add (p);
    }

    canvas.setParameters (params);
    canvas.setPresets (presets.getMenu());

    if (isProtected())
        canvas.setRegistered (! isInDemo());

    canvas.setSize (layout.width, layout.height);
    return canvas.snapshot (juce::jmin (1.0f, (float) juce::jmax (64, maxWidth) / (float) layout.width));
}

//==============================================================================
juce::File MadePlugins::snapshotOf (const juce::File& pluginFolder)
{
    return pluginFolder.getChildFile (snapshotFileName);
}

bool MadePlugins::snapshotDue (const juce::File& pluginFolder)
{
    const auto picture = snapshotOf (pluginFolder);

    if (! picture.existsAsFile())
        return true;

    const auto taken = picture.getLastModificationTime();

    for (const auto& item : juce::RangedDirectoryIterator (pluginFolder, true, "*", juce::File::findFiles))
        if (item.getFile() != picture && item.getModificationTime() > taken)
            return true;

    return false;
}

juce::Result MadePlugins::makeSnapshot (const juce::File& pluginFolder)
{
    juce::String error;
    auto plugin = CreatorPlugin::load (pluginFolder, error);

    if (plugin == nullptr)
        return juce::Result::fail (error);

    const auto image = static_cast<CreatorPlugin&> (*plugin).drawPanel (snapshotWidth);

    if (! image.isValid())
        return juce::Result::fail ("It has no panel to show.");

    const auto picture = snapshotOf (pluginFolder);
    juce::MemoryOutputStream bytes;
    juce::PNGImageFormat png;

    if (! png.writeImageToStream (image, bytes) || ! picture.replaceWithData (bytes.getData(), bytes.getDataSize()))
        return juce::Result::fail ("Couldn't write " + picture.getFullPathName());

    return juce::Result::ok();
}

//==============================================================================
bool CreatorPlugin::showNativeScreen (const juce::String& title)
{
    if (screen == nullptr)
        screen = std::make_unique<Screen> (*this);

    screen->show (title);
    return true;
}

void CreatorPlugin::hideNativeScreen()
{
    if (screen != nullptr)
        screen->setVisible (false);
}

//==============================================================================
bool CreatorPlugin::resolve (const juce::String& source, int& moduleIndex, int& port) const
{
    const auto module = source.upToFirstOccurrenceOf (".", false, false);
    const auto portName = source.fromFirstOccurrenceOf (".", false, false);

    if (module == "plugin")
    {
        // "plugin.out L" is what the plugin plays; "plugin.in L" what it gets.
        moduleIndex = portName.startsWith ("in") ? WasmPlugin::pluginInput : WasmPlugin::pluginOutput;
        port = portName.endsWithChar ('R') ? 1 : 0;
        return true;
    }

    for (const auto& m : modules)
    {
        if (m.id == module && m.outputs.contains (portName))
        {
            moduleIndex = m.index;
            port = m.outputs.indexOf (portName);
            return true;
        }
    }

    return false;
}

void CreatorPlugin::setScreenSources (const juce::StringArray& levels, const juce::StringArray& scopes)
{
    levelSources = levels;
    scopeSources = scopes;
    applyScreenSources();
}

void CreatorPlugin::applyScreenSources()
{
    tappedSources.clear();

    if (wasm == nullptr)
        return;

    std::vector<std::pair<int, int>> taps;

    for (const auto& source : levelSources)
    {
        int moduleIndex = 0, port = 0;

        if (taps.size() < (size_t) WasmPlugin::maxTaps && resolve (source, moduleIndex, port))
        {
            taps.push_back ({ moduleIndex, port });
            tappedSources.add (source);
        }
    }

    wasm->setTaps (taps);

    // As in the studio: the panel's scopes use slots 1 to 3.
    for (int slot = 1; slot < WasmPlugin::maxScopes; ++slot)
    {
        int moduleIndex = WasmPlugin::off, port = 0;

        if (slot - 1 < scopeSources.size())
            resolve (scopeSources[slot - 1], moduleIndex, port);

        wasm->setScope (slot, moduleIndex, port);
    }
}

float CreatorPlugin::takeLevel (const juce::String& source, bool rms)
{
    if (wasm == nullptr)
        return 0.0f;

    for (const auto& d : displays)
        if (d.id == source)
            return wasm->getDisplay (d.index);

    const auto tap = tappedSources.indexOf (source);

    if (tap < 0)
        return 0.0f;

    return rms ? wasm->getTapRms (tap) : wasm->takeTapPeak (tap);
}

void CreatorPlugin::readScope (const juce::String& source, float* destination, int numSamples) const
{
    const auto slot = scopeSources.indexOf (source) + 1;

    if (wasm != nullptr && slot >= 1 && slot < WasmPlugin::maxScopes)
        wasm->readScope (slot, destination, numSamples);
    else
        juce::FloatVectorOperations::clear (destination, numSamples);
}
