#include "PluginEditor.h"
#include "AxisWebUI.h"

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

    auto options = Options{}
        .withBackend (Options::Backend::webview2)
        .withWinWebView2Options (
            Options::WinWebView2{}
                .withUserDataFolder (juce::File::getSpecialLocation (
                    juce::File::tempDirectory)))
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
        .withNativeFunction ("axisLoadCapture",
            [this] (auto& args, auto complete) { complete (handleLoadCapture (args)); })
        .withNativeFunction ("axisPresetMenu",
            [this] (auto& args, auto complete) { complete (handlePresetMenu (args)); })
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

    auto& ampRef = const_cast<AxisAudioProcessor&> (proc).ampModule();
    obj->setProperty ("__capture", ampRef.currentCaptureName());
    obj->setProperty ("__captureLoaded", ampRef.isCaptureLoaded());

    obj->setProperty ("__preset", proc.getPresetName());
    obj->setProperty ("__presetSaved", proc.getPresetFile() != juce::File());

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

juce::var AxisAudioProcessorEditor::handleLoadCapture (const juce::Array<juce::var>&)
{
    chooser = std::make_unique<juce::FileChooser> (
        "Load a NAM capture", juce::File(), "*.nam");

    chooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();

            if (file.existsAsFile())
            {
                proc.ampModule().loadCapture (file);

                if (web != nullptr)
                    web->emitEventIfBrowserIsVisible (
                        "axisCapture", juce::var (proc.ampModule().currentCaptureName()));
            }
        });

    return juce::var (true);
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

juce::var AxisAudioProcessorEditor::handlePresetMenu (const juce::Array<juce::var>&)
{
    const auto rigs = axis::PresetManager::userPresets();

    juce::PopupMenu menu;
    menu.addSectionHeader ("RIGS");

    if (rigs.isEmpty())
        menu.addItem (-1, "(none saved yet)", false);

    for (int i = 0; i < rigs.size(); ++i)
        menu.addItem (1 + i, rigs[i].getFileNameWithoutExtension(), true, rigs[i] == proc.getPresetFile());

    menu.addSeparator();
    menu.addItem (9000, "Show rigs folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition(),
        [this, rigs] (int chosen)
        {
            if (chosen <= 0)
                return;

            if (chosen == 9000)
            {
                axis::PresetManager::showFolder();
                return;
            }

            const int index = chosen - 1;

            if (index < rigs.size() && proc.loadPresetFile (rigs[index]))
                presetCursor = index;
        });

    return juce::var (true);
}

juce::var AxisAudioProcessorEditor::handlePresetStep (const juce::Array<juce::var>& args)
{
    const auto rigs = axis::PresetManager::userPresets();

    if (rigs.isEmpty())
        return juce::var (false);

    const int delta = args.isEmpty() ? 1 : (static_cast<int> (args[0]) < 0 ? -1 : 1);

    if (presetCursor < 0)
    {
        presetCursor = 0;

        for (int i = 0; i < rigs.size(); ++i)
            if (rigs[i] == proc.getPresetFile())
                presetCursor = i;

        if (delta > 0)
            presetCursor -= 1;
    }

    presetCursor = ((presetCursor + delta) % rigs.size() + rigs.size()) % rigs.size();
    proc.loadPresetFile (rigs[presetCursor]);
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
