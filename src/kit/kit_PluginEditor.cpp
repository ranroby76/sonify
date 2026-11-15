// D:\workspace\Fanan_web_hub\src\kit\kit_PluginEditor.cpp

#include "PluginEditor.h"
#include "LayerDoc.h"
#include "PluginAssets.h"
#include "PluginDesign.h"
#include "WasmElements.h"

//==============================================================================
SonifyPluginEditor::SonifyPluginEditor (SonifyPluginProcessor& owner)
    : AudioProcessorEditor (owner), processor (owner)
{
    setLookAndFeel (&lookAndFeel);

    // Its pictures and looks, unpacked where the window's code reads them (a look's pictures
    // by name, as in the studio).
    const auto gui = PluginAssets::guiFolder();
    LayerImages::setFolder (gui.getChildFile (GuiLayout::imagesFolder));
    looks.setGuiFolder (gui);

    canvas.setLooks (looks);
    canvas.setPanelOnly (true);
    canvas.setGuiFolder (gui);

    canvas.onParameterChanged = [this] (int index, float value)
    {
        processor.setValueFromWindow (index, value);

        // The chosen preset's values aren't its own any more: it shows a *.
        if (processor.presets.getCurrent() >= 0 && ! processor.presets.isModified())
        {
            processor.presets.setModified (true);
            canvas.setPresets (processor.presets.getMenu());
        }
    };

    canvas.onNote = [this] (int note, float velocity)
    {
        if (velocity > 0.0f)
            processor.keyboard.noteOn (1, note, velocity);
        else
            processor.keyboard.noteOff (1, note, 0.0f);
    };

    canvas.isNoteDown = [this] (int note) { return processor.keyboard.isNoteOnForChannels (0xffff, note); };

    // The preset manager: the plugin's presets, the user's own (Save, Save as, Load), banks.
    presetUi.getValues = [this] { return processor.currentValues(); };
    presetUi.apply = [this] (const Preset& preset)
    {
        processor.applyPreset (preset);
        pushParameters (true);
    };
    presetUi.changed = [this] { canvas.setPresets (processor.presets.getMenu()); };
    canvas.onPresetChosen = [this] (int index) { presetUi.choose (index); };
    canvas.onPresetAction = [this] (PresetAction action) { presetUi.perform (action); };

    // What its meters, lamps and scopes watch.
    canvas.readLevel = [this] (const juce::String& source, bool rms) { return processor.takeLevel (source, rms); };
    canvas.readScope = [this] (const juce::String& source, float* destination, int numSamples) { processor.readScope (source, destination, numSamples); };
    canvas.onSourcesChanged = [this] (const juce::StringArray& levels, const juce::StringArray& scopes) { processor.setWindowSources (levels, scopes); };

    // The programmed elements (the toolbox's and the plugin's own), compiled in.
    {
        std::string error;
        std::shared_ptr<WasmElements> elements (WasmElements::load ({}, error));
        canvas.setElements (elements);
    }

    canvas.setLayout (PluginDesign::window());
    pushParameters (true);
    canvas.setPresets (processor.presets.getMenu());

    // A protected plugin: its registration badge opens the Registration window, until it's registered.
    if (processor.isProtected())
    {
        canvas.onRegistrationClicked = [this] { showRegistration(); };
        wasRegistered = processor.isRegistered();
        canvas.setRegistered (wasRegistered);
    }

    // A sampler whose blobs aren't here: which, and where they're looked for.
    if (const auto missing = processor.getMissingInstruments(); ! missing.isEmpty())
    {
        libraryBar.setText ("Not on this computer: " + missing.joinIntoString (", ") + ". Put the blobs in "
                                + processor.libraryFolder().getFullPathName(), juce::dontSendNotification);
        libraryBar.setColour (juce::Label::backgroundColourId, juce::Colour (0xff3a2a14));
        libraryBar.setColour (juce::Label::textColourId, juce::Colour (0xffffd28a));
        libraryBar.setFont (juce::FontOptions (13.0f));
        addAndMakeVisible (libraryBar);
    }

    addAndMakeVisible (canvas);

    // Its window: the panel and the side panels joined to it (a preset manager's).
    shownWindow = canvas.windowBounds();
    const auto bars = libraryBar.isVisible() ? barHeight : 0;
    setResizable (true, true);
    setResizeLimits (juce::jmax (120, shownWindow.getWidth() / 4), juce::jmax (90, shownWindow.getHeight() / 4),
                     shownWindow.getWidth() * 3, shownWindow.getHeight() * 3 + bars);

    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio ((double) shownWindow.getWidth() / (double) juce::jmax (1, shownWindow.getHeight() + bars));

    setSize (shownWindow.getWidth(), shownWindow.getHeight() + bars);
    canvas.onWindowChanged = [this] { followWindow(); };

    startTimerHz (15);
}

SonifyPluginEditor::~SonifyPluginEditor()
{
    stopTimer();
    presetUi.getValues = nullptr;
    presetUi.apply = nullptr;
    presetUi.changed = nullptr;
    registration = nullptr;
    setLookAndFeel (nullptr);
}

void SonifyPluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void SonifyPluginEditor::resized()
{
    auto area = getLocalBounds();

    if (libraryBar.isVisible())
        libraryBar.setBounds (area.removeFromBottom (barHeight));

    canvas.setBounds (area);
}

//==============================================================================
void SonifyPluginEditor::timerCallback()
{
    // Automation, the host's own controls and presets move its knobs too.
    pushParameters (false);
    canvas.setPresets (processor.presets.getMenu());

    // Registered meanwhile (here, or in another copy of it): the badge goes.
    if (processor.isProtected() && processor.isRegistered() != wasRegistered)
    {
        wasRegistered = processor.isRegistered();
        canvas.setRegistered (wasRegistered);
    }
}

void SonifyPluginEditor::pushParameters (bool always)
{
    const auto count = (size_t) processor.getNumPluginParameters();
    bool changed = always || shown.size() != count;

    for (size_t i = 0; i < count && ! changed; ++i)
        changed = std::abs (shown[i] - processor.getValue ((int) i)) > 1.0e-6f;

    if (! changed)
        return;

    shown.resize (count);
    juce::Array<PluginCanvas::Param> params;

    for (size_t i = 0; i < count; ++i)
    {
        const auto& info = processor.getInfo ((int) i);
        shown[i] = processor.getValue ((int) i);

        PluginCanvas::Param p;
        p.index = (int) i;
        p.id = juce::String::fromUTF8 (info.id.c_str());
        p.name = juce::String::fromUTF8 (info.name.c_str());
        p.unit = juce::String::fromUTF8 (info.unit.c_str());
        p.min = info.min;
        p.max = info.max;
        p.def = info.def;
        p.skew = info.skew;
        p.value = shown[i];
        params.add (p);
    }

    canvas.setParameters (params);
}

// A side panel opened or closed: the window as much wider or narrower, at the size it's shown.
void SonifyPluginEditor::followWindow()
{
    const auto window = canvas.windowBounds();
    const double scale = shownWindow.getWidth() > 0 ? (double) canvas.getWidth() / shownWindow.getWidth() : 1.0;
    const auto bars = getHeight() - canvas.getHeight();
    shownWindow = window;

    setResizeLimits (1, 1, 1 << 16, 1 << 16);

    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio (0.0);

    setSize (juce::roundToInt (window.getWidth() * scale), juce::roundToInt (window.getHeight() * scale) + bars);

    setResizeLimits (juce::jmax (120, window.getWidth() / 4), juce::jmax (90, window.getHeight() / 4),
                     window.getWidth() * 3, window.getHeight() * 3 + bars);

    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio ((double) window.getWidth() / (double) juce::jmax (1, window.getHeight() + bars));
}

//==============================================================================
// The Registration window: this computer's ID (sent to the maker), and the serial they send back.
void SonifyPluginEditor::showRegistration()
{
    if (registration != nullptr || processor.isRegistered())
        return;

    registration = std::make_unique<juce::AlertWindow> ("Register " + processor.getRegistrationName(),
                                                        "Send this computer's ID to the plugin's maker, then type or paste the serial "
                                                        "they send you.",
                                                        juce::MessageBoxIconType::NoIcon, this);

    registration->addTextEditor ("id", processor.getComputerId(), "This computer's ID");
    registration->getTextEditor ("id")->setReadOnly (true);
    registration->addTextEditor ("serial", {}, "Serial");
    registration->addButton ("Register", 1, juce::KeyPress (juce::KeyPress::returnKey));
    registration->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    registration->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis = juce::Component::SafePointer<SonifyPluginEditor> (this)] (int result)
        {
            if (safeThis == nullptr || safeThis->registration == nullptr)
                return;

            const auto serial = safeThis->registration->getTextEditorContents ("serial").trim();
            safeThis->registration->setVisible (false);

            if (result == 1)
            {
                if (const auto why = safeThis->processor.registerSerial (serial); why.isNotEmpty())
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Not registered", why, "OK", safeThis.getComponent());
                else
                    safeThis->timerCallback();
            }

            juce::MessageManager::callAsync ([safeThis]
            {
                if (safeThis != nullptr)
                    safeThis->registration = nullptr;
            });
        }), false);
}
