#include "PluginEditor.h"
#include "AxisWebUI.h"

#include "../Preferences.h"   // SettingsFolder: the web view profile Sonify shares

namespace
{
    constexpr int defaultWidth  = 1120;
    constexpr int defaultHeight = 680;

    /** Resolves a requested path such as "/js/index.js" to the embedded
        resource whose original filename matches its basename.

        Binary data is flat — it keeps filenames, not folders — so matching is
        by basename. That makes collisions possible, which is why anything
        under /juce/ is refused here: those requests belong to JUCE's own
        frontend library, and answering them with a file of ours would hand the
        page the wrong script. */
    const char* findResource (const juce::String& path, int& sizeOut)
    {
        if (path.startsWithIgnoreCase ("/juce/"))
            return nullptr;

        const auto wanted = path.fromLastOccurrenceOf ("/", false, false);

        for (int i = 0; i < AxisWebUI::namedResourceListSize; ++i)
        {
            const auto* name = AxisWebUI::namedResourceList[i];

            if (juce::String (AxisWebUI::getNamedResourceOriginalFilename (name))
                    .equalsIgnoreCase (wanted))
                return AxisWebUI::getNamedResource (name, sizeOut);
        }

        return nullptr;
    }

    juce::String mimeFor (const juce::String& path)
    {
        if (path.endsWithIgnoreCase (".html")) return "text/html";
        if (path.endsWithIgnoreCase (".js"))   return "text/javascript";
        if (path.endsWithIgnoreCase (".css"))  return "text/css";
        if (path.endsWithIgnoreCase (".svg"))  return "image/svg+xml";
        if (path.endsWithIgnoreCase (".png"))  return "image/png";
        if (path.endsWithIgnoreCase (".woff2"))return "font/woff2";
        return "application/octet-stream";
    }
}

// ---------------------------------------------------------------------------

AxisAudioProcessorEditor::AxisAudioProcessorEditor (AxisAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    using Options = juce::WebBrowserComponent::Options;

    // The one WebView2 profile every web view in Sonify uses (SettingsFolder::webViewProfile):
    // one profile is one browser process for them all, so this window opens in a moment. A
    // profile of its own would start a browser of its own first and freeze Sonify for seconds.
    auto options = Options{}
        .withBackend (Options::Backend::webview2)
        .withWinWebView2Options (
            Options::WinWebView2{}
                .withUserDataFolder (SettingsFolder::webViewProfile()))
        .withNativeIntegrationEnabled()
        .withResourceProvider ([this] (const auto& url) { return provide (url); })
        .withNativeFunction ("axisSetParam",
            [this] (auto& args, auto complete) { complete (handleSetParam (args)); })
        .withNativeFunction ("axisBeginGesture",
            [this] (auto& args, auto complete) { complete (handleGesture (args, true)); })
        .withNativeFunction ("axisEndGesture",
            [this] (auto& args, auto complete) { complete (handleGesture (args, false)); })
        .withNativeFunction ("axisGetState",
            [this] (auto& args, auto complete) { complete (handleGetState (args)); })
        .withNativeFunction ("axisSetChainOrder",
            [this] (auto& args, auto complete) { complete (handleChainOrder (args)); })
        .withNativeFunction ("axisCabChoose",
            [this] (auto& args, auto complete) { complete (handleCabChoose (args)); })
        .withNativeFunction ("axisPresetChoose",
            [this] (auto& args, auto complete) { complete (handlePresetChoose (args)); })
        .withNativeFunction ("axisPresetStep",
            [this] (auto& args, auto complete) { complete (handlePresetStep (args)); })
        .withNativeFunction ("axisPresetLoad",
            [this] (auto& args, auto complete) { complete (handlePresetLoad (args)); })
        .withNativeFunction ("axisPresetSave",
            [this] (auto& args, auto complete) { complete (handlePresetSave (args)); })
        .withNativeFunction ("axisPresetQuickSave",
            [this] (auto& args, auto complete) { complete (handlePresetQuick (args)); });

    web = std::make_unique<juce::WebBrowserComponent> (options);
    addAndMakeVisible (*web);

    web->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) defaultWidth / (double) defaultHeight);
    setResizeLimits (defaultWidth / 2, defaultHeight / 2,
                     defaultWidth * 2, defaultHeight * 2);
    setSize (defaultWidth, defaultHeight);

    startTimerHz (24);
}

AxisAudioProcessorEditor::~AxisAudioProcessorEditor()
{
    stopTimer();
}

void AxisAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0a0c0f));
}

void AxisAudioProcessorEditor::resized()
{
    if (web != nullptr)
        web->setBounds (getLocalBounds());
}

// ---------------------------------------------------------------------------

std::optional<juce::WebBrowserComponent::Resource>
AxisAudioProcessorEditor::provide (const juce::String& url)
{
    const auto path = (url == "/" || url.isEmpty()) ? juce::String ("/index.html") : url;

    int size = 0;
    if (const auto* data = findResource (path, size))
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));
        return juce::WebBrowserComponent::Resource { std::move (bytes), mimeFor (path) };
    }

    return std::nullopt;
}

// ---------------------------------------------------------------------------

juce::var AxisAudioProcessorEditor::handleSetParam (const juce::Array<juce::var>& args)
{
    if (args.size() < 2)
        return juce::var (false);

    const auto id  = args[0].toString();
    const auto val = static_cast<float> (args[1]);

    if (auto* param = proc.state().getParameter (id))
    {
        const auto norm = param->getNormalisableRange().convertTo0to1 (val);
        param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
        return juce::var (true);
    }

    return juce::var (false);
}

juce::var AxisAudioProcessorEditor::handleGesture (const juce::Array<juce::var>& args,
                                                   bool begin)
{
    if (args.isEmpty())
        return juce::var (false);

    if (auto* param = proc.state().getParameter (args[0].toString()))
    {
        if (begin) param->beginChangeGesture();
        else       param->endChangeGesture();
        return juce::var (true);
    }

    return juce::var (false);
}

juce::String AxisAudioProcessorEditor::buildStateJson() const
{
    auto* obj = new juce::DynamicObject();

    for (auto* p : proc.getParameters())
    {
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
        {
            const auto id = withId->paramID;

            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                obj->setProperty (id, ranged->convertFrom0to1 (ranged->getValue()));
        }
    }

    auto* meters = new juce::DynamicObject();
    meters->setProperty ("in",  proc.inputLevel());
    meters->setProperty ("out", proc.outputLevel());
    obj->setProperty ("__meters", juce::var (meters));

    juce::StringArray orderParts;
    for (auto v : proc.chainOrder())
        orderParts.add (juce::String (v));

    obj->setProperty ("__chainOrder", orderParts.joinIntoString (","));

    // From the parameters, not from what the audio has caught up with: with the audio stopped
    // (or the pedal switched out) the page still shows what was picked.
    auto& procRef = const_cast<AxisAudioProcessor&> (proc);

    const auto choiceOf = [&procRef] (const char* id)
    {
        const auto* v = procRef.state().getRawParameterValue (id);
        return v != nullptr ? juce::roundToInt (v->load()) : 0;
    };

    // The AMP: what the amp that's on is called under the pedal's name.
    const auto voice = choiceOf (axis::pid::ampVoice);
    auto* ampInfo = new juce::DynamicObject();
    ampInfo->setProperty ("voice", voice);
    ampInfo->setProperty ("sub", axis::dsp::AmpModel::voiceDescription (voice));
    obj->setProperty ("__amp", juce::var (ampInfo));

    // The cabinet: the modelled ones, the IR loaded (and where from), the pack's, what plays,
    // and where the mic is, for the rail's plate and its menu.
    const auto& cabinet = procRef.ampModule().getCabinet();
    auto* cab = new juce::DynamicObject();
    juce::Array<juce::var> models, packNames;

    for (int i = 0; i < axis::dsp::cab::numSpeakers; ++i)
        models.add (juce::String (axis::dsp::cab::speakerSpecs()[(size_t) i].name));

    for (const auto& name : procRef.packCabinetNames())
        packNames.add (name);

    const auto source = proc.getCabIr();
    const auto type = choiceOf (axis::pid::cabType);
    const auto irType = axis::dsp::Cabinet::irType;

    cab->setProperty ("models", models);
    cab->setProperty ("playing", type == irType ? (cabinet.hasIr() ? irType : axis::dsp::Cabinet::fallbackType) : type);
    cab->setProperty ("irType", axis::dsp::Cabinet::irType);
    cab->setProperty ("ir", cabinet.hasIr() ? proc.getCabIrName() : juce::String());
    cab->setProperty ("irFrom", source.startsWith ("pack:") ? "pack" : source.isNotEmpty() ? "file" : "");
    cab->setProperty ("irSource", source.startsWith ("pack:") ? source.fromFirstOccurrenceOf ("pack:", false, false) : source);
    cab->setProperty ("problem", proc.getCabIrProblem());
    cab->setProperty ("pack", packNames);
    cab->setProperty ("packProblem", procRef.getPackProblem());

    const auto* micX = procRef.state().getRawParameterValue (axis::pid::cabMicX);
    const auto* micY = procRef.state().getRawParameterValue (axis::pid::cabMicY);
    cab->setProperty ("mic", juce::String (axis::dsp::cab::Mic::describe (micX != nullptr ? micX->load() : 36.0f,
                                                                          micY != nullptr ? micY->load() : 50.0f)));
    obj->setProperty ("__cab", juce::var (cab));

    obj->setProperty ("__preset", proc.getPresetName());
    obj->setProperty ("__presetSaved", proc.getPresetFile() != juce::File());

    // The rigs the LCD's menu lists: the five AXIS comes with, then the user's own, and which one
    // is loaded now (kind "factory" or "mine" with its place in that list, or nothing). The menu is
    // the page's own, so the page needs the names.
    const auto& factory = axis::PresetManager::factoryRigs();
    const auto mine = axis::PresetManager::userPresets();

    juce::Array<juce::var> factoryNames, mineNames;

    for (const auto& rig : factory)
        factoryNames.add (rig.name);

    for (const auto& file : mine)
        mineNames.add (file.getFileNameWithoutExtension());

    auto* rigs = new juce::DynamicObject();
    rigs->setProperty ("factory", factoryNames);
    rigs->setProperty ("mine", mineNames);

    juce::String loadedKind;
    int loadedIndex = -1;

    if (proc.getPresetFile() != juce::File())
    {
        loadedKind = "mine";
        loadedIndex = mine.indexOf (proc.getPresetFile());
    }
    else
    {
        for (int i = 0; i < (int) factory.size(); ++i)
            if (factory[(size_t) i].name == proc.getPresetName())
            {
                loadedKind = "factory";
                loadedIndex = i;
                break;
            }
    }

    rigs->setProperty ("kind", loadedKind);
    rigs->setProperty ("index", loadedIndex);
    obj->setProperty ("__rigs", juce::var (rigs));

    return juce::JSON::toString (juce::var (obj), true);
}

juce::var AxisAudioProcessorEditor::handleGetState (const juce::Array<juce::var>&)
{
    uiReady = true;
    return juce::var (buildStateJson());
}

juce::var AxisAudioProcessorEditor::handleChainOrder (const juce::Array<juce::var>& args)
{
    if (args.isEmpty())
        return juce::var (false);

    juce::StringArray parts;
    parts.addTokens (args[0].toString(), ",", "");

    if (parts.size() != axis::numModules)
        return juce::var (false);

    std::array<int, axis::numModules> newOrder {};
    for (int i = 0; i < axis::numModules; ++i)
        newOrder[(size_t) i] = juce::jlimit (0, axis::numModules - 1, parts[i].getIntValue());

    proc.setChainOrder (newOrder);
    return juce::var (true);
}

juce::var AxisAudioProcessorEditor::handleCabChoose (const juce::Array<juce::var>& args)
{
    if (args.isEmpty())
        return juce::var (false);

    const auto kind = args[0].toString();
    const auto index = args.size() > 1 ? static_cast<int> (args[1]) : -1;

    const auto setType = [this] (int type)
    {
        if (auto* p = proc.state().getParameter (axis::pid::cabType))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 ((float) type));
            p->endChangeGesture();
        }
    };

    // A modelled cabinet.
    if (kind == "model")
    {
        if (! juce::isPositiveAndBelow (index, axis::dsp::cab::numSpeakers))
            return juce::var (false);

        setType (index);
        proc.forgetCabIrProblem();
        return juce::var (true);
    }

    // Back to the IR that's loaded.
    if (kind == "ir")
    {
        if (! proc.ampModule().getCabinet().hasIr())
            return juce::var (false);

        setType (axis::dsp::Cabinet::irType);
        return juce::var (true);
    }

    // One of the pack's.
    if (kind == "pack")
    {
        const auto names = proc.packCabinetNames();

        if (! juce::isPositiveAndBelow (index, names.size()))
            return juce::var (false);

        if (const auto problem = proc.setCabIr ("pack:" + names[index]); problem.isNotEmpty())
            report (juce::Result::fail (problem));

        return juce::var (true);
    }

    // A file of the user's own.
    if (kind == "file")
    {
        const auto source = proc.getCabIr();
        const auto start = source.isNotEmpty() && ! source.startsWith ("pack:") ? juce::File (source)
                                                                                 : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

        chooser = std::make_unique<juce::FileChooser> ("Load a cabinet impulse response", start, AxisPack::irWildcards());

        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto file = fc.getResult();

                if (file.existsAsFile())
                    if (const auto problem = proc.setCabIr (file.getFullPathName()); problem.isNotEmpty())
                        report (juce::Result::fail (problem));
            });

        return juce::var (true);
    }

    // No IR.
    if (kind == "clear")
    {
        proc.clearCabIr();
        return juce::var (true);
    }

    // Where the pack goes.
    if (kind == "folder")
    {
        const auto file = AxisAudioProcessor::packFile();

        if (file.existsAsFile())
            file.revealToUser();
        else if (file.getParentDirectory().createDirectory())
            file.getParentDirectory().revealToUser();

        return juce::var (true);
    }

    return juce::var (false);
}

// ---------------------------------------------------------------------------
//  Rigs (presets)
// ---------------------------------------------------------------------------

void AxisAudioProcessorEditor::report (const juce::Result& result)
{
    if (result.failed())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "AXIS", result.getErrorMessage());
}

juce::var AxisAudioProcessorEditor::handlePresetChoose (const juce::Array<juce::var>& args)
{
    if (args.isEmpty())
        return juce::var (false);

    const auto kind = args[0].toString();

    if (kind == "folder")
    {
        axis::PresetManager::showFolder();
        return juce::var (true);
    }

    const auto index = args.size() > 1 ? static_cast<int> (args[1]) : -1;

    if (index < 0)
        return juce::var (false);

    const auto factoryCount = (int) axis::PresetManager::factoryRigs().size();

    if (kind == "factory")
    {
        if (index >= factoryCount)
            return juce::var (false);

        proc.loadFactory (index);
        presetCursor = index;
        return juce::var (true);
    }

    if (kind == "mine")
    {
        const auto rigs = axis::PresetManager::userPresets();

        if (index >= rigs.size() || ! proc.loadPresetFile (rigs[index]))
            return juce::var (false);

        presetCursor = factoryCount + index;
        return juce::var (true);
    }

    return juce::var (false);
}

juce::var AxisAudioProcessorEditor::handlePresetStep (const juce::Array<juce::var>& args)
{
    // The five AXIS comes with, then the user's own: the arrows walk the whole list, round.
    const auto& factory = axis::PresetManager::factoryRigs();
    const auto rigs = axis::PresetManager::userPresets();
    const auto factoryCount = (int) factory.size();
    const auto total = factoryCount + rigs.size();

    if (total == 0)
        return juce::var (false);

    const int delta = args.isEmpty() ? 1 : (static_cast<int> (args[0]) < 0 ? -1 : 1);

    if (presetCursor < 0)
    {
        presetCursor = 0;

        for (int i = 0; i < factoryCount; ++i)
            if (proc.getPresetFile() == juce::File() && proc.getPresetName() == factory[(size_t) i].name)
                presetCursor = i;

        for (int i = 0; i < rigs.size(); ++i)
            if (rigs[i] == proc.getPresetFile())
                presetCursor = factoryCount + i;

        if (delta > 0)
            presetCursor -= 1;
    }

    presetCursor = ((presetCursor + delta) % total + total) % total;

    if (presetCursor < factoryCount)
        proc.loadFactory (presetCursor);
    else
        proc.loadPresetFile (rigs[presetCursor - factoryCount]);

    return juce::var (true);
}

juce::var AxisAudioProcessorEditor::handlePresetLoad (const juce::Array<juce::var>&)
{
    const auto folder = axis::PresetManager::folder();
    folder.createDirectory();

    chooser = std::make_unique<juce::FileChooser> ("Load an AXIS rig", folder,
                                                   juce::String ("*") + axis::PresetManager::extension);

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();

            if (file.existsAsFile() && ! proc.loadPresetFile (file))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                        "AXIS", "That file is not an AXIS rig.");

            presetCursor = -1;
        });

    return juce::var (true);
}

void AxisAudioProcessorEditor::savePresetAs()
{
    auto* window = new juce::AlertWindow ("Save rig", "Name for this rig:", juce::MessageBoxIconType::NoIcon);
    window->addTextEditor ("name", proc.getPresetName());
    window->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<AxisAudioProcessorEditor> safe (this);

    window->enterModalState (true, juce::ModalCallbackFunction::create ([safe, window] (int result)
    {
        if (safe == nullptr || result != 1)
            return;

        const auto name = axis::PresetManager::cleanName (window->getTextEditorContents ("name"));
        safe->report (safe->proc.savePresetFile (axis::PresetManager::fileFor (name), name));
        safe->presetCursor = -1;
    }), true);
}

juce::var AxisAudioProcessorEditor::handlePresetSave (const juce::Array<juce::var>&)
{
    savePresetAs();
    return juce::var (true);
}

juce::var AxisAudioProcessorEditor::handlePresetQuick (const juce::Array<juce::var>&)
{
    // Over the rig that's loaded; a rig that was never saved asks for a name instead.
    if (proc.getPresetFile() != juce::File())
    {
        report (proc.savePresetFile (proc.getPresetFile(), proc.getPresetName()));
        return juce::var (true);
    }

    savePresetAs();
    return juce::var (true);
}

// ---------------------------------------------------------------------------

void AxisAudioProcessorEditor::timerCallback()
{
    if (! uiReady || web == nullptr)
        return;

    // one push per frame carries both the meters and any parameter the host
    // moved behind the UI's back (automation, a control surface, undo)
    web->emitEventIfBrowserIsVisible ("axisUpdate", juce::var (buildStateJson()));
}
