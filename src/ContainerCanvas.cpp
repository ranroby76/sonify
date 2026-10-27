// D:\workspace\Fanan_web_hub\src\ContainerCanvas.cpp
//
// The Node Rack's window (Colosseum's): the popups, the window with its bar and pages, and the
// Rack's canvas upkeep and drawing. Mouse work and menus are in ContainerCanvasMouse.cpp; the
// Mixer, the Manual and the tools' editors in NodeRackPages.cpp.

#include "ContainerCanvas.h"
#include "NodeRackPages.h"
#include "PluginHeader.h"

#include <cmath>

using Graph = juce::AudioProcessorGraph;
using IoProcessor = juce::AudioProcessorGraph::AudioGraphIOProcessor;
namespace Style = CanvasStyle;

//==============================================================================
// StatusToolTip
StatusToolTip::StatusToolTip (const juce::String& title, bool isStreaming, std::function<void()> onDelete)
    : streaming (isStreaming), deleteCallback (std::move (onDelete))
{
    addAndMakeVisible (titleLabel);
    titleLabel.setText (title, juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setFont (juce::Font (juce::FontOptions (14.0f)));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible (statusLabel);
    statusLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    statusLabel.setJustificationType (juce::Justification::centred);

    if (streaming)
    {
        statusLabel.setText ("Active Streaming", juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
        startTimer (50);
    }
    else
    {
        statusLabel.setText ("Idle (Bypassed)", juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    }

    if (deleteCallback)
    {
        addAndMakeVisible (deleteButton);
        deleteButton.onClick = [this]
        {
            if (deleteCallback)
                deleteCallback();

            if (auto* callout = findParentComponentOfClass<juce::CallOutBox>())
                callout->dismiss();
        };
    }

    // Long names (a plugin's own) get room.
    const auto width = juce::jlimit (200, 420, juce::GlyphArrangement::getStringWidthInt (titleLabel.getFont(), title) + 30);
    setSize (width, deleteCallback ? 100 : 70);
}

StatusToolTip::~StatusToolTip()   { stopTimer(); }

void StatusToolTip::timerCallback()
{
    alpha += delta;

    if (alpha >= 1.0f || alpha <= 0.2f)
        delta = -delta;

    repaint();
}

void StatusToolTip::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.8f));
    g.setColour (juce::Colours::white);
    g.drawRect (getLocalBounds(), 1);

    if (streaming)
    {
        g.setColour (juce::Colours::green.withAlpha (juce::jlimit (0.0f, 1.0f, alpha)));
        const float r = 8.0f;
        g.fillEllipse (static_cast<float> (getWidth()) / 2.0f - r, 45.0f, r * 2.0f, r * 2.0f);
    }
    else
    {
        g.setColour (juce::Colours::red);
        const float r = 4.0f;
        g.drawEllipse (static_cast<float> (getWidth()) / 2.0f - r, 48.0f, r * 2.0f, r * 2.0f, 1.0f);
    }
}

void StatusToolTip::resized()
{
    titleLabel.setBounds (0, 5, getWidth(), 20);
    statusLabel.setBounds (0, 25, getWidth(), 20);

    if (deleteCallback)
        deleteButton.setBounds (10, 65, getWidth() - 20, 25);
}

//==============================================================================
// MidiChannelSelector
MidiChannelSelector::MidiChannelSelector (PluginNode& n, std::function<void()> onUpdate)
    : node (n), updateCallback (std::move (onUpdate))
{
    titleLabel.setText ("Select MIDI Input Channel", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLabel);

    closeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::red.darker());
    closeButton.addListener (this);
    addAndMakeVisible (closeButton);

    for (int i = 1; i <= 16; ++i)
    {
        auto* button = channelButtons.add (new juce::TextButton (juce::String (i)));
        button->addListener (this);
        addAndMakeVisible (button);
    }

    allButton.addListener (this);
    addAndMakeVisible (allButton);

    showSelection();
    setSize (260, 238);
}

void MidiChannelSelector::showSelection()
{
    const auto mask = node.getMidiChannelMask();
    const auto all = mask == 0xffff;

    for (int i = 0; i < 16; ++i)
    {
        const bool chosen = ! all && (mask & (1 << i)) != 0;
        channelButtons[i]->setColour (juce::TextButton::buttonColourId, chosen ? juce::Colours::orange : juce::Colours::grey.darker());
        channelButtons[i]->setColour (juce::TextButton::textColourOffId, chosen ? juce::Colours::black : juce::Colours::lightgrey);
    }

    allButton.setColour (juce::TextButton::buttonColourId, all ? juce::Colours::orange : juce::Colours::grey.darker());
    allButton.setColour (juce::TextButton::textColourOffId, all ? juce::Colours::black : juce::Colours::lightgrey);
}

void MidiChannelSelector::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.95f));
    g.setColour (juce::Colours::orange);
    g.drawRect (getLocalBounds(), 2);
}

void MidiChannelSelector::resized()
{
    auto area = getLocalBounds().reduced (8);
    auto header = area.removeFromTop (25);
    closeButton.setBounds (header.removeFromRight (25));
    titleLabel.setBounds (header);
    area.removeFromTop (8);

    allButton.setBounds (area.removeFromBottom (26).reduced (2));
    area.removeFromBottom (4);

    const int cols = 4, rows = 4;
    const int w = area.getWidth() / cols, h = area.getHeight() / rows;

    for (int i = 0; i < 16; ++i)
        channelButtons[i]->setBounds (area.getX() + (i % cols) * w + 2, area.getY() + (i / cols) * h + 2, w - 4, h - 4);
}

void MidiChannelSelector::buttonClicked (juce::Button* button)
{
    if (button == &closeButton)
    {
        if (auto* callout = findParentComponentOfClass<juce::CallOutBox>())
            callout->dismiss();

        return;
    }

    if (button == &allButton)
        node.setMidiChannelMask (0xffff);

    for (int i = 0; i < 16; ++i)
        if (channelButtons[i] == button)
            node.setMidiChannelMask (1 << i);   // one channel, as Colosseum's

    showSelection();

    if (updateCallback)
        updateCallback();
}

//==============================================================================
// TransportOverridePanel
TransportOverridePanel::TransportOverridePanel (Target t)
    : target (std::move (t))
{
    setSize (260, 160);

    titleLabel.setText (target.title, juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    syncLabel.setText ("Synced to master transport?", juce::dontSendNotification);
    syncLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    syncLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (syncLabel);

    for (auto* button : { &yesButton, &noButton })
    {
        button->setRadioGroupId (1001);
        button->setClickingTogglesState (true);
        button->addListener (this);
        button->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3a3a3a));
        button->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        button->setColour (juce::TextButton::textColourOffId, juce::Colours::grey);
        addAndMakeVisible (*button);
    }

    yesButton.setButtonText ("Yes");
    noButton.setButtonText ("No");
    yesButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::green.darker());
    noButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::orange.darker());

    timeSigLabel.setText ("Time Signature:", juce::dontSendNotification);
    timeSigLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    timeSigLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (timeSigLabel);

    const char* signatures[] = { "2/4", "3/4", "4/4", "5/4", "6/4", "7/4", "3/8", "6/8", "7/8", "9/8", "12/8" };

    for (int i = 0; i < 11; ++i)
        timeSigCombo.addItem (signatures[i], i + 1);

    timeSigCombo.addListener (this);
    addAndMakeVisible (timeSigCombo);

    tempoLabel.setText ("Tempo:", juce::dontSendNotification);
    tempoLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    tempoLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (tempoLabel);

    tempoSlider.setRange (20.0, 999.0, 0.1);
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 55, 20);
    tempoSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    tempoSlider.setColour (juce::Slider::trackColourId, juce::Colours::yellow.darker());
    tempoSlider.setColour (juce::Slider::thumbColourId, juce::Colours::yellow);
    tempoSlider.addListener (this);
    addAndMakeVisible (tempoSlider);

    const auto synced = target.isSynced();
    yesButton.setToggleState (synced, juce::dontSendNotification);
    noButton.setToggleState (! synced, juce::dontSendNotification);
    tempoSlider.setValue (target.getTempo(), juce::dontSendNotification);
    const auto [numerator, denominator] = target.getSignature();
    selectSignature (numerator, denominator);
    updateEnabledState (synced);
}

void TransportOverridePanel::resized()
{
    auto area = getLocalBounds().reduced (8);
    titleLabel.setBounds (area.removeFromTop (22));
    area.removeFromTop (4);
    auto syncRow = area.removeFromTop (24);
    syncLabel.setBounds (syncRow.removeFromLeft (170));
    syncRow.removeFromLeft (4);
    yesButton.setBounds (syncRow.removeFromLeft (36));
    syncRow.removeFromLeft (2);
    noButton.setBounds (syncRow.removeFromLeft (36));
    area.removeFromTop (8);
    auto tsRow = area.removeFromTop (24);
    timeSigLabel.setBounds (tsRow.removeFromLeft (100));
    tsRow.removeFromLeft (4);
    timeSigCombo.setBounds (tsRow);
    area.removeFromTop (6);
    auto tempoRow = area.removeFromTop (24);
    tempoLabel.setBounds (tempoRow.removeFromLeft (50));
    tempoRow.removeFromLeft (4);
    tempoSlider.setBounds (tempoRow);
}

void TransportOverridePanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2d2d2d));
    g.setColour (juce::Colour (0xff555555));
    g.drawRect (getLocalBounds(), 1);
}

void TransportOverridePanel::buttonClicked (juce::Button* button)
{
    if (button == &yesButton || button == &noButton)
    {
        const auto synced = yesButton.getToggleState();
        target.setSynced (synced);
        updateEnabledState (synced);
    }
}

void TransportOverridePanel::comboBoxChanged (juce::ComboBox*)
{
    const auto text = timeSigCombo.getText();
    target.setSignature (text.upToFirstOccurrenceOf ("/", false, false).getIntValue(), text.fromFirstOccurrenceOf ("/", false, false).getIntValue());
}

void TransportOverridePanel::sliderValueChanged (juce::Slider*)
{
    target.setTempo (tempoSlider.getValue());
}

void TransportOverridePanel::selectSignature (int numerator, int denominator)
{
    const auto wanted = juce::String (numerator) + "/" + juce::String (denominator);

    for (int i = 0; i < timeSigCombo.getNumItems(); ++i)
        if (timeSigCombo.getItemText (i) == wanted)
            timeSigCombo.setSelectedItemIndex (i, juce::dontSendNotification);

    if (timeSigCombo.getSelectedId() == 0)
        timeSigCombo.setSelectedId (3, juce::dontSendNotification);   // 4/4
}

void TransportOverridePanel::updateEnabledState (bool synced)
{
    timeSigCombo.setEnabled (! synced);
    tempoSlider.setEnabled (! synced);
    timeSigLabel.setAlpha (synced ? 0.4f : 1.0f);
    tempoLabel.setAlpha (synced ? 0.4f : 1.0f);
}

//==============================================================================
// The editor: the bar and the pages
ContainerEditor::ContainerEditor (ContainerPlugin& c)
    : container (c), canvas (c)
{
    macros = std::make_unique<NodeRackMacros> (container);
    addAndMakeVisible (*macros);
    addAndMakeVisible (canvas);
    setSize (1180, 760);
}

ContainerEditor::~ContainerEditor() = default;

void ContainerEditor::showPage (Page newPage)
{
    page = newPage;

    // The Mixer and the Manual are made when first shown.
    if (page == Page::mixer && mixer == nullptr)
    {
        mixer = std::make_unique<NodeRackMixer> ([this]() -> ContainerPlugin& { return canvas.shownRack(); });
        addChildComponent (*mixer);
    }

    if (page == Page::manual && manual == nullptr)
    {
        manual = std::make_unique<NodeRackManual>();
        addChildComponent (*manual);
    }

    canvas.setVisible (page == Page::rack);
    macros->setVisible (page == Page::rack);

    if (mixer != nullptr)  mixer->setVisible (page == Page::mixer);
    if (manual != nullptr) manual->setVisible (page == Page::manual);

    resized();
    repaint();
}

ContainerEditor::BarLayout ContainerEditor::barLayout() const
{
    BarLayout b;
    const float h = 28.0f, y = (static_cast<float> (barHeight) - h) / 2.0f;
    auto x = 10.0f;

    const auto tab = [&x, y, h] (float w) { juce::Rectangle<float> r (x, y, w, h); x += w + 4.0f; return r; };
    b.rack = tab (70.0f);
    b.mixer = tab (70.0f);
    b.manual = tab (76.0f);

    auto right = static_cast<float> (getWidth()) - 10.0f;
    const auto button = [&right, y, h] (float w) { right -= w; juce::Rectangle<float> r (right, y, w, h); right -= 6.0f; return r; };
    b.panic = button (70.0f);
    b.reset = button (62.0f);
    b.save = button (88.0f);
    b.load = button (88.0f);
    b.browse = button (82.0f);
    return b;
}

void ContainerEditor::paint (juce::Graphics& g)
{
    g.fillAll (Style::colBackground);

    const auto w = static_cast<float> (getWidth());
    g.setColour (juce::Colour (34, 14, 52));
    g.fillRect (0.0f, 0.0f, w, static_cast<float> (barHeight));
    g.setColour (juce::Colour (140, 70, 200));
    g.fillRect (0.0f, static_cast<float> (barHeight) - 1.0f, w, 1.0f);

    const auto b = barLayout();

    const auto tab = [&] (juce::Rectangle<float> r, const char* label, bool on)
    {
        const auto hot = r.contains (hover);
        g.setColour (on ? juce::Colour (120, 60, 180) : hot ? juce::Colour (70, 34, 104) : juce::Colour (52, 24, 78));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (on ? juce::Colour (210, 170, 255) : juce::Colour (110, 60, 160));
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        g.setColour (on ? juce::Colours::white : juce::Colour (200, 180, 225));
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText (label, r, juce::Justification::centred);
    };

    tab (b.rack, "Rack", page == Page::rack);
    tab (b.mixer, "Mixer", page == Page::mixer);
    tab (b.manual, "Manual", page == Page::manual);

    const auto button = [&] (juce::Rectangle<float> r, const char* label, juce::Colour fill, juce::Colour edge)
    {
        g.setColour (r.contains (hover) ? fill.brighter (0.25f) : fill);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (edge);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        g.drawText (label, r, juce::Justification::centred);
    };

    button (b.browse, "Browse...", juce::Colour (60, 40, 90), juce::Colour (170, 120, 230));
    button (b.load, "Load Patch", juce::Colour (35, 80, 120), juce::Colour (100, 180, 255));
    button (b.save, "Save Patch", juce::Colour (35, 100, 50), juce::Colour (100, 220, 130));
    button (b.reset, "Reset", juce::Colour (90, 70, 30), juce::Colour (220, 180, 90));
    button (b.panic, "PANIC", juce::Colour (150, 30, 30), juce::Colour (255, 110, 110));

    // The rack's name in the middle, when there's room for it.
    const auto middle = juce::Rectangle<float> (b.manual.getRight() + 16.0f, 0.0f, b.browse.getX() - b.manual.getRight() - 32.0f, static_cast<float> (barHeight));

    if (middle.getWidth() > 80.0f)
    {
        g.setColour (juce::Colour (190, 160, 230));
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText (container.getContainerName(), middle, juce::Justification::centred, true);
    }
}

void ContainerEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (barHeight);

    if (mixer != nullptr)  mixer->setBounds (area);
    if (manual != nullptr) manual->setBounds (area);

    // The Rack page: the macros over the canvas.
    macros->setBounds (area.removeFromTop (NodeRackMacros::height));
    canvas.setBounds (area);
}

void ContainerEditor::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isLeftButtonDown() || e.position.y >= static_cast<float> (barHeight))
        return;

    const auto b = barLayout();
    const auto pos = e.position;

    if (b.rack.contains (pos))          showPage (Page::rack);
    else if (b.mixer.contains (pos))    showPage (Page::mixer);
    else if (b.manual.contains (pos))   showPage (Page::manual);
    else if (b.browse.contains (pos))   { showPage (Page::rack); canvas.addPluginFile (canvas.centreOfView()); }
    else if (b.load.contains (pos))     canvas.loadPreset();
    else if (b.save.contains (pos))     canvas.savePreset();
    else if (b.reset.contains (pos))    canvas.resetRack();
    else if (b.panic.contains (pos))    canvas.panic();
}

void ContainerEditor::mouseMove (const juce::MouseEvent& e)
{
    hover = e.position;
    repaint (0, 0, getWidth(), barHeight);
}

void ContainerEditor::mouseExit (const juce::MouseEvent&)
{
    hover = { -1.0f, -1.0f };
    repaint (0, 0, getWidth(), barHeight);
}

namespace
{
    class ContainerWindow final : public juce::DocumentWindow
    {
    public:
        explicit ContainerWindow (ContainerPlugin& container)
            : juce::DocumentWindow (container.getContainerName(), Style::colBackground, juce::DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            PluginHeader::attach (*this, container);   // the rack's ON/OFF, over its canvas
            auto* editor = new ContainerEditor (container);
            editor->getCanvas().onTitleChanged = [this, &container, editor]
            {
                setName (container.getContainerName());
                editor->repaint();
            };
            setContentOwned (editor, true);
            setResizable (true, true);
            getConstrainer()->setSizeLimits (720, 460, 8000, 6000);
            centreWithSize (getWidth(), getHeight());
        }

        void closeButtonPressed() override
        {
            setVisible (false);   // kept for next time, where it was
        }
    };

    class WindowScreen final : public Containers::Screen
    {
    public:
        explicit WindowScreen (ContainerPlugin& c) : container (c) {}

        void show (const juce::String& title) override
        {
            if (window == nullptr)
                window = std::make_unique<ContainerWindow> (container);

            window->setName (title.isNotEmpty() ? title : container.getContainerName());
            window->setVisible (true);

            // It belongs to Sonify's window: Sonify coming forward (a plugin's screen opened on
            // its page, a click in the Browser) never puts the rack behind it.
            if (auto* host = Containers::getHost())
                ContainerCanvas::keepInFrontOf (*window, host->mainWindowHandle());

            window->toFront (true);
        }

        void hide() override
        {
            if (window != nullptr)
                window->setVisible (false);
        }

        bool isShowing() const override   { return window != nullptr && window->isVisible(); }

    private:
        ContainerPlugin& container;
        std::unique_ptr<ContainerWindow> window;
    };
}

void ContainerCanvas::install()
{
    Containers::setScreenFactory ([] (ContainerPlugin& container) -> std::unique_ptr<Containers::Screen>
    {
        return std::make_unique<WindowScreen> (container);
    });
}

//==============================================================================
// The canvas: upkeep
ContainerGraphCanvas::ContainerGraphCanvas (ContainerPlugin& rootContainer)
    : root (rootContainer)
{
    setOpaque (true);
    zoomLevel = juce::jlimit (0.10f, 2.0f, root.containerZoom);
    panOffsetX = root.containerPan.x;
    panOffsetY = root.containerPan.y;
    seenGeneration = ContainerPlugin::getStructureGeneration();
    rebuildNodeTypeCache();
    startTimer (mainTimer, 200);   // structure changes and general upkeep
    startTimer (meterTimer, 50);   // meters and monitors (only when there are any)
}

ContainerGraphCanvas::~ContainerGraphCanvas()
{
    stopTimer (mainTimer);
    stopTimer (meterTimer);
    stopTimer (mouseTimer);

    // Where the view was left is kept with the container.
    if (stack.empty())
    {
        root.containerZoom = zoomLevel;
        root.containerPan = { panOffsetX, panOffsetY };
    }
}

void ContainerGraphCanvas::resized()
{
    clampPanOffset();
    needsRepaint = true;
}

void ContainerGraphCanvas::setZoomLevel (float zoom)
{
    zoomLevel = juce::jlimit (0.10f, 2.0f, zoom);
    clampPanOffset();
    repaint();
}

void ContainerGraphCanvas::clampPanOffset()
{
    panOffsetX = juce::jlimit (0.0f, juce::jmax (0.0f, virtualCanvasWidth - getVisibleWidth()), panOffsetX);
    panOffsetY = juce::jlimit (0.0f, juce::jmax (0.0f, virtualCanvasHeight - getVisibleHeight()), panOffsetY);
}

juce::Point<int> ContainerGraphCanvas::virtualToScreen (float vx, float vy) const
{
    const auto sb = getScreenBounds();
    return { sb.getX() + static_cast<int> ((vx - panOffsetX) * zoomLevel), sb.getY() + static_cast<int> ((vy - panOffsetY) * zoomLevel) };
}

Containers::Spot ContainerGraphCanvas::centreOfView() const
{
    return { panOffsetX + getVisibleWidth() * 0.5f - Style::minNodeWidth * 0.5f,
             panOffsetY + getVisibleHeight() * 0.5f - Style::nodeHeight * 0.5f };
}

void ContainerGraphCanvas::rebuildNodeTypeCache()
{
    nodeTypeCache.clear();
    hasLiveNodes = false;
    auto& container = current();
    auto* graph = getActiveGraph();

    for (auto* node : graph->getNodes())
    {
        NodeTypeCache cache;
        auto* proc = node->getProcessor();
        cache.plugin = dynamic_cast<PluginNode*> (proc);
        cache.container = cache.plugin != nullptr ? cache.plugin->asContainer() : nullptr;
        cache.connector = dynamic_cast<ContainerTools::Connector*> (proc);
        cache.stereoMeter = dynamic_cast<ContainerTools::StereoMeter*> (proc);
        cache.midiMonitor = dynamic_cast<ContainerTools::MidiMonitor*> (proc);
        cache.missing = dynamic_cast<ContainerTools::Missing*> (proc);
        cache.latcher = dynamic_cast<NodeTools::Latcher*> (proc);
        cache.stepSeq = dynamic_cast<NodeTools::StepSeq*> (proc);
        cache.transientSplitter = dynamic_cast<NodeTools::TransientSplitter*> (proc);
        cache.isAudioInput = node == container.getAudioInputNode().get();
        cache.isAudioOutput = node == container.getAudioOutputNode().get();
        cache.isMidiInput = node == container.getMidiInputNode().get();
        cache.isMidiOutput = node == container.getMidiOutputNode().get();
        cache.isIO = cache.isAudioInput || cache.isAudioOutput || cache.isMidiInput || cache.isMidiOutput;
        cache.isInstrument = cache.plugin != nullptr && cache.plugin->isInstrument();
        cache.hasSidechain = cache.plugin != nullptr && cache.plugin->hasSidechain();

        if (cache.stereoMeter != nullptr || cache.midiMonitor != nullptr || cache.latcher != nullptr
            || cache.stepSeq != nullptr || cache.transientSplitter != nullptr)
            hasLiveNodes = true;

        if (cache.isAudioInput)        cache.pluginName = "Audio Input";
        else if (cache.isAudioOutput)  cache.pluginName = "Audio Output";
        else if (cache.isMidiInput)    cache.pluginName = "MIDI Input";
        else if (cache.isMidiOutput)   cache.pluginName = "MIDI Output";
        else if (proc != nullptr)      cache.pluginName = proc->getName();   // once, here: never while painting

        if (cache.pluginName.length() > 20)
            cache.pluginName = cache.pluginName.substring (0, 18) + "..";

        nodeTypeCache[node->nodeID] = cache;
    }

    lastNodeCount = static_cast<size_t> (graph->getNumNodes());
    cachedConnections = graph->getConnections();
    lastConnectionCount = cachedConnections.size();
    verifyPositions();
}

const ContainerGraphCanvas::NodeTypeCache* ContainerGraphCanvas::getCachedNodeType (NodeID id) const
{
    const auto found = nodeTypeCache.find (id);
    return found != nodeTypeCache.end() ? &found->second : nullptr;
}

void ContainerGraphCanvas::verifyPositions()
{
    for (auto* node : getActiveGraph()->getNodes())
    {
        if (! node->properties.contains ("x")) node->properties.set ("x", 100.0);
        if (! node->properties.contains ("y")) node->properties.set ("y", 100.0);
    }
}

void ContainerGraphCanvas::checkStack()
{
    // A container entered must still be a node of the one around it (a preset loaded over it
    // takes it away): otherwise back to the top.
    auto* parent = &root;

    for (auto* inner : stack)
    {
        bool found = false;
        parent->forEachPluginNode ([&] (PluginNode& node) { found = found || node.asContainer() == inner; });

        if (! found)
        {
            stack.clear();
            zoomLevel = savedRootZoom;
            panOffsetX = panOffsetY = 0.0f;

            // What was being dragged was in a graph no longer shown.
            dragCable.active = false;
            draggingNodeID = {};
            draggingVolumeNodeID = {};

            rebuildNodeTypeCache();
            needsRepaint = true;

            if (onTitleChanged)
                onTitleChanged();

            return;
        }

        parent = inner;
    }
}

void ContainerGraphCanvas::refreshIfStale()
{
    const auto now = ContainerPlugin::getStructureGeneration();

    if (now == seenGeneration)
        return;

    // The cache's pointers, and the containers entered, may be gone (a preset loaded over them,
    // a node deleted in another window): the stack is checked first (only by comparing pointers
    // from the window's own container down), then the cache made from what's there now.
    seenGeneration = now;
    checkStack();
    rebuildNodeTypeCache();
    needsRepaint = true;
}

bool ContainerGraphCanvas::holds (const ContainerPlugin* container) const
{
    if (container == &root)
        return true;

    bool found = false;
    root.forEachPluginNode ([&] (PluginNode& node) { found = found || node.asContainer() == container; });
    return found;
}

void ContainerGraphCanvas::timerCallback (int timerId)
{
    refreshIfStale();

    if (timerId == mainTimer)
    {
        checkStack();
        auto* graph = getActiveGraph();

        if (static_cast<size_t> (graph->getNumNodes()) != lastNodeCount || graph->getConnections().size() != lastConnectionCount)
        {
            rebuildNodeTypeCache();
            needsRepaint = true;
        }

        if (highlightPin != lastHighlightPin)
        {
            lastHighlightPin = highlightPin;
            needsRepaint = true;
        }

        if (! (hoveredConnection == lastHoveredConnection))
        {
            lastHoveredConnection = hoveredConnection;
            needsRepaint = true;
        }

        if (dragCable.active || draggingNodeID.uid != 0)
            needsRepaint = true;

        // Pins and cables light up with signal: a slow look is enough.
        for (const auto& [id, cache] : nodeTypeCache)
            if (cache.plugin != nullptr && (cache.plugin->isMidiInActive() || cache.plugin->isMidiOutActive()))
                needsRepaint = true;

        if (needsRepaint)
        {
            repaint();
            needsRepaint = false;
        }
    }
    else if (timerId == meterTimer)
    {
        if (hasLiveNodes)
            repaint();
    }
    else if (timerId == mouseTimer)
    {
        repaint();
    }
}

void ContainerGraphCanvas::diveInto (ContainerPlugin& container)
{
    if (stack.empty())
    {
        savedRootZoom = zoomLevel;
        root.containerZoom = zoomLevel;
        root.containerPan = { panOffsetX, panOffsetY };
    }

    stack.push_back (&container);
    zoomLevel = juce::jlimit (0.10f, 2.0f, container.containerZoom);
    panOffsetX = container.containerPan.x;
    panOffsetY = container.containerPan.y;
    clampPanOffset();
    rebuildNodeTypeCache();
    needsRepaint = true;
    repaint();

    if (onTitleChanged)
        onTitleChanged();
}

void ContainerGraphCanvas::diveOut()
{
    if (stack.empty())
        return;

    stack.back()->containerZoom = zoomLevel;
    stack.back()->containerPan = { panOffsetX, panOffsetY };
    stack.pop_back();

    auto& now = current();
    zoomLevel = stack.empty() ? savedRootZoom : now.containerZoom;
    panOffsetX = now.containerPan.x;
    panOffsetY = now.containerPan.y;
    clampPanOffset();
    rebuildNodeTypeCache();
    needsRepaint = true;
    repaint();

    if (onTitleChanged)
        onTitleChanged();
}

void ContainerGraphCanvas::diveToTop()
{
    while (! stack.empty())
        diveOut();
}

//==============================================================================
// Geometry
int ContainerGraphCanvas::inputPinCount (Node* node) const
{
    auto* proc = node->getProcessor();
    auto* cache = getCachedNodeType (node->nodeID);

    // Instruments have no audio in, only MIDI.
    if (cache != nullptr && cache->plugin != nullptr && cache->isInstrument)
        return 0;

    return proc->getTotalNumInputChannels();
}

juce::Rectangle<float> ContainerGraphCanvas::getNodeBounds (Node* node) const
{
    const auto x = static_cast<float> (static_cast<double> (node->properties.getWithDefault ("x", 0.0)));
    const auto y = static_cast<float> (static_cast<double> (node->properties.getWithDefault ("y", 0.0)));
    auto* proc = node->getProcessor();
    auto numIn = proc->getTotalNumInputChannels();
    auto numOut = proc->getTotalNumOutputChannels();

    if (proc->acceptsMidi())  ++numIn;
    if (proc->producesMidi()) ++numOut;

    auto width = Style::minNodeWidth;
    const auto maxPins = juce::jmax (numIn, numOut);

    if (maxPins > 1)
        width = juce::jmax (width, static_cast<float> (maxPins + 1) * Style::minPinSpacing);

    auto height = Style::nodeHeight;
    auto* cache = getCachedNodeType (node->nodeID);

    if (cache != nullptr && cache->stereoMeter != nullptr)
    {
        height = Style::nodeHeight * 4.0f;
    }
    else if (cache != nullptr && cache->midiMonitor != nullptr)
    {
        height = Style::nodeHeight * 6.0f;
        width = Style::minNodeWidth * 2.0f;
    }
    else if (cache != nullptr && cache->connector != nullptr)
    {
        height = Style::nodeHeight * 1.5f;
    }
    else if (cache != nullptr && cache->latcher != nullptr)
    {
        height = Style::nodeHeight * 4.0f;   // the 4x4 pads
        width = Style::minNodeWidth * 2.0f;
    }
    else if (cache != nullptr && (cache->stepSeq != nullptr || cache->transientSplitter != nullptr))
    {
        height = Style::nodeHeight * 2.0f;
        width = Style::minNodeWidth * 2.0f;
    }

    return { x, y, width, height };
}

juce::Rectangle<float> ContainerGraphCanvas::getPinBounds (const PinID& pin, Node* node) const
{
    if (node == nullptr || node->getProcessor() == nullptr)
        return {};

    auto* proc = node->getProcessor();
    const auto nodeBounds = getNodeBounds (node);
    const auto numIn = inputPinCount (node);
    const auto numOut = proc->getTotalNumOutputChannels();
    const auto totalInputs = numIn + (proc->acceptsMidi() ? 1 : 0);
    const auto totalOutputs = numOut + (proc->producesMidi() ? 1 : 0);
    const auto pinIndex = pin.isMidi ? (pin.isInput ? numIn : numOut) : pin.pinIndex;
    const auto totalPins = pin.isInput ? totalInputs : totalOutputs;

    if (totalPins == 0)
        return {};

    const auto spacing = Style::minPinSpacing;
    const auto totalWidth = spacing * static_cast<float> (totalPins + 1);
    const auto startX = nodeBounds.getCentreX() - totalWidth / 2.0f;
    const auto x = startX + spacing * static_cast<float> (pinIndex + 1);
    const auto y = pin.isInput ? nodeBounds.getY() - Style::hookLength : nodeBounds.getBottom() + Style::hookLength;
    return { x - Style::pinSize / 2.0f, y - Style::pinSize / 2.0f, Style::pinSize, Style::pinSize };
}

juce::Point<float> ContainerGraphCanvas::pinPosition (NodeID node, int pin, bool isInput, bool isMidi) const
{
    return getPinCenterPos ({ node, pin, isInput, isMidi });
}

juce::Point<float> ContainerGraphCanvas::getPinCenterPos (const PinID& pin) const
{
    auto* node = getActiveGraph()->getNodeForId (pin.nodeID);
    return node != nullptr ? getPinPos (node, pin) : juce::Point<float>();
}

bool ContainerGraphCanvas::isSidechainPin (const PinID& pin, Node* node) const
{
    if (pin.isMidi || ! pin.isInput || pin.pinIndex < 2 || node == nullptr)
        return false;

    auto* cache = getCachedNodeType (node->nodeID);

    // A plugin's (or a container's) sidechain input: its third and fourth input pins.
    return cache != nullptr && ((cache->plugin != nullptr && cache->hasSidechain) || (cache->missing != nullptr));
}

juce::Colour ContainerGraphCanvas::getPinColour (const PinID& pin, Node* node) const
{
    if (pin.isMidi)
        return Style::colPinMidi;

    // The container's own sidechain in: the Audio Input's third and fourth pins.
    if (auto* cache = node != nullptr ? getCachedNodeType (node->nodeID) : nullptr; cache != nullptr && cache->isAudioInput && pin.pinIndex >= 2)
        return Style::colPinSidechain;

    // The Transient Splitter's outputs: its attacks (1-2) and what sustains (3-4).
    if (auto* cache = node != nullptr ? getCachedNodeType (node->nodeID) : nullptr; cache != nullptr && cache->transientSplitter != nullptr && ! pin.isInput)
        return pin.pinIndex < 2 ? juce::Colour (0xff00c8ff) : juce::Colour (0xffffb432);

    return isSidechainPin (pin, node) ? Style::colPinSidechain : Style::colPinAudio;
}

ContainerGraphCanvas::PinID ContainerGraphCanvas::findPinAt (juce::Point<float> pos) const
{
    for (auto* node : getActiveGraph()->getNodes())
    {
        auto* proc = node->getProcessor();

        if (proc == nullptr)
            continue;

        const auto numIn = inputPinCount (node);

        for (int i = 0; i < numIn; ++i)
        {
            const PinID p { node->nodeID, i, true, false };

            if (pos.getDistanceFrom (getPinPos (node, p)) <= Style::pinSize)
                return p;
        }

        if (proc->acceptsMidi())
        {
            const PinID p { node->nodeID, 0, true, true };

            if (pos.getDistanceFrom (getPinPos (node, p)) <= Style::pinSize)
                return p;
        }

        for (int i = 0; i < proc->getTotalNumOutputChannels(); ++i)
        {
            const PinID p { node->nodeID, i, false, false };

            if (pos.getDistanceFrom (getPinPos (node, p)) <= Style::pinSize)
                return p;
        }

        if (proc->producesMidi())
        {
            const PinID p { node->nodeID, 0, false, true };

            if (pos.getDistanceFrom (getPinPos (node, p)) <= Style::pinSize)
                return p;
        }
    }

    return {};
}

ContainerGraphCanvas::Node* ContainerGraphCanvas::findNodeAt (juce::Point<float> pos) const
{
    // The topmost (last drawn) first.
    const auto nodes = getActiveGraph()->getNodes();

    for (int i = nodes.size(); --i >= 0;)
        if (getNodeBounds (nodes[i]).contains (pos))
            return nodes[i];

    return nullptr;
}

ContainerGraphCanvas::Connection ContainerGraphCanvas::getConnectionAt (juce::Point<float> pos) const
{
    constexpr float hitTolerance = 3.0f;
    auto* graph = getActiveGraph();
    Connection closest { { NodeID(), 0 }, { NodeID(), 0 } };
    auto closestDistance = std::numeric_limits<float>::max();

    for (const auto& connection : cachedConnections)
    {
        auto* src = graph->getNodeForId (connection.source.nodeID);
        auto* dst = graph->getNodeForId (connection.destination.nodeID);

        if (src == nullptr || dst == nullptr)
            continue;

        const auto isMidi = connection.source.isMIDI();
        const auto start = getPinPos (src, { src->nodeID, isMidi ? 0 : connection.source.channelIndex, false, isMidi });
        const auto end = getPinPos (dst, { dst->nodeID, isMidi ? 0 : connection.destination.channelIndex, true, isMidi });

        juce::Path path;
        path.startNewSubPath (start);
        path.cubicTo (start.x, start.y + 50.0f, end.x, end.y - 50.0f, end.x, end.y);

        juce::Path stroked;
        juce::PathStrokeType (hitTolerance * 2.0f).createStrokedPath (stroked, path);

        if (! stroked.contains (pos))
            continue;

        // The nearest of overlapping cables: by sampled distance to each curve.
        auto minDistance = std::numeric_limits<float>::max();

        for (int i = 0; i <= 20; ++i)
        {
            const auto t = static_cast<float> (i) / 20.0f, u = 1.0f - t;
            const auto px = u * u * u * start.x + 3.0f * u * u * t * start.x + 3.0f * u * t * t * end.x + t * t * t * end.x;
            const auto py = u * u * u * start.y + 3.0f * u * u * t * (start.y + 50.0f) + 3.0f * u * t * t * (end.y - 50.0f) + t * t * t * end.y;
            minDistance = juce::jmin (minDistance, pos.getDistanceFrom ({ px, py }));
        }

        if (minDistance < closestDistance)
        {
            closestDistance = minDistance;
            closest = connection;
        }
    }

    return closest;
}

bool ContainerGraphCanvas::canConnect (const PinID& start, const PinID& end) const
{
    return start.nodeID != end.nodeID && start.isInput != end.isInput && start.isMidi == end.isMidi;
}

void ContainerGraphCanvas::createConnection (const PinID& start, const PinID& end)
{
    if (! canConnect (start, end))
        return;

    const auto& source = start.isInput ? end : start;
    const auto& destination = start.isInput ? start : end;

    if (source.isMidi)
        current().connectMidi (source.nodeID, destination.nodeID);
    else
        current().connect (source.nodeID, source.pinIndex, destination.nodeID, destination.pinIndex);

    rebuildNodeTypeCache();
    needsRepaint = true;
}

ContainerGraphCanvas::Buttons ContainerGraphCanvas::buttonsFor (Node* node) const
{
    Buttons b;
    auto bounds = getNodeBounds (node);
    auto* cache = getCachedNodeType (node->nodeID);
    bounds.removeFromTop (Style::nodeTitleHeight);
    const auto y = bounds.getBottom() - Style::bottomBtnMargin - Style::bottomBtnHeight;
    auto x = bounds.getX() + Style::bottomBtnMargin;
    const auto next = [&x, y]
    {
        juce::Rectangle<float> r (x, y, Style::bottomBtnWidth, Style::bottomBtnHeight);
        x += Style::bottomBtnWidth + Style::bottomBtnSpacing;
        return r;
    };

    if (cache == nullptr)
        return b;

    if (cache->isAudioInput || cache->isAudioOutput)
    {
        b.toggle = { x, y, Style::bottomBtnWidth * 1.5f, Style::bottomBtnHeight };
        return b;
    }

    if (cache->isIO)
        return b;

    if (cache->container != nullptr)
    {
        b.edit = next();        // E: in
        b.mute = next();        // M: the container's mute
        b.transport = next();   // T
        b.remove = next();      // X
        return b;
    }

    if (cache->plugin != nullptr)
    {
        b.edit = next();

        if (cache->isInstrument)
            b.channel = next();

        b.mute = next();

        if (! cache->isInstrument)
            b.pass = next();

        b.transport = next();
        b.remove = next();
        return b;
    }

    if (cache->connector != nullptr)
    {
        b.mute = next();
        b.remove = next();
        auto content = getNodeBounds (node);
        content.removeFromTop (Style::nodeTitleHeight);
        b.volume = content.withTrimmedBottom (Style::bottomBtnHeight + Style::bottomBtnMargin * 2.0f).reduced (15.0f, 4.0f);
        return b;
    }

    if (cache->latcher != nullptr || cache->stepSeq != nullptr || cache->transientSplitter != nullptr)
    {
        b.edit = next();
        b.remove = next();
        auto content = getNodeBounds (node);
        content.removeFromTop (Style::nodeTitleHeight);
        auto area = content.reduced (8.0f, 6.0f);

        if (cache->latcher != nullptr)
        {
            // ALL OFF at the top right, the pads below it, above the buttons.
            b.allOff = { area.getRight() - 42.0f, area.getY(), 42.0f, 16.0f };
            auto grid = area;
            grid.removeFromTop (18.0f);
            grid.removeFromBottom (Style::bottomBtnHeight + Style::bottomBtnMargin + 2.0f);
            b.pads = grid;
        }
        else if (cache->stepSeq != nullptr)
        {
            // Play, the tempo and SYN along the top.
            auto row = area.removeFromTop (28.0f);
            b.play = row.removeFromLeft (50.0f).reduced (2.0f);
            row.removeFromLeft (6.0f);
            b.bpm = row.removeFromLeft (70.0f);
            row.removeFromLeft (4.0f);
            b.sync = row.removeFromLeft (32.0f).reduced (1.0f, 3.0f);
        }

        return b;
    }

    b.remove = next();   // meters, monitors and missing plugins: delete only
    return b;
}

//==============================================================================
// Drawing
void ContainerGraphCanvas::drawWire (juce::Graphics& g, juce::Point<float> start, juce::Point<float> end, juce::Colour colour, float thickness) const
{
    juce::Path path;
    path.startNewSubPath (start);
    path.cubicTo (start.x, start.y + 50.0f, end.x, end.y - 50.0f, end.x, end.y);
    g.setColour (colour);
    g.strokePath (path, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void ContainerGraphCanvas::drawButton (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill, juce::Colour text,
                                       const juce::String& label, float fontSize) const
{
    if (r.isEmpty())
        return;

    g.setColour (fill);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (text);
    g.setFont (juce::Font (juce::FontOptions (fontSize, juce::Font::bold)));
    g.drawText (label, r, juce::Justification::centred);
}

void ContainerGraphCanvas::paint (juce::Graphics& g)
{
    refreshIfStale();
    g.fillAll (Style::colContainerBg);   // inside a container: Colosseum's dark purple
    g.saveState();
    g.addTransform (juce::AffineTransform::scale (zoomLevel).translated (-panOffsetX * zoomLevel, -panOffsetY * zoomLevel));

    // The grid, thinned out when it would be too dense.
    g.setColour (juce::Colours::white.withAlpha (0.03f));
    int gridSize = 40;

    while (static_cast<float> (gridSize) * zoomLevel < 8.0f)
        gridSize *= 2;

    const auto firstX = static_cast<int> (panOffsetX) / gridSize * gridSize;
    const auto firstY = static_cast<int> (panOffsetY) / gridSize * gridSize;
    const auto lastX = juce::jmin (virtualCanvasWidth, panOffsetX + getVisibleWidth() + static_cast<float> (gridSize));
    const auto lastY = juce::jmin (virtualCanvasHeight, panOffsetY + getVisibleHeight() + static_cast<float> (gridSize));

    for (int x = firstX; static_cast<float> (x) < lastX; x += gridSize)
        g.drawVerticalLine (x, panOffsetY, lastY);

    for (int y = firstY; static_cast<float> (y) < lastY; y += gridSize)
        g.drawHorizontalLine (y, panOffsetX, lastX);

    auto* graph = getActiveGraph();

    if (nodeTypeCache.empty() && graph->getNumNodes() > 0)
        rebuildNodeTypeCache();

    // Cables
    for (const auto& connection : cachedConnections)
    {
        auto* src = graph->getNodeForId (connection.source.nodeID);
        auto* dst = graph->getNodeForId (connection.destination.nodeID);

        if (src == nullptr || dst == nullptr)
            continue;

        const auto isMidi = connection.source.isMIDI();
        const PinID srcPin { src->nodeID, isMidi ? 0 : connection.source.channelIndex, false, isMidi };
        const PinID dstPin { dst->nodeID, isMidi ? 0 : connection.destination.channelIndex, true, isMidi };
        const auto start = getPinPos (src, srcPin);
        const auto end = getPinPos (dst, dstPin);

        auto colour = juce::Colours::darkgrey;
        auto thickness = 2.0f;

        if (! current().isOff (src) && ! current().isOff (dst))
        {
            auto* srcCache = getCachedNodeType (src->nodeID);
            bool hasSignal = false;

            if (isMidi)
                hasSignal = srcCache != nullptr && srcCache->plugin != nullptr && srcCache->plugin->isMidiOutActive();
            else
                hasSignal = srcCache != nullptr && srcCache->plugin != nullptr;   // plugin to plugin: shown live

            colour = isMidi ? Style::colPinMidi : (isSidechainPin (dstPin, dst) ? Style::colPinSidechain : Style::colPinAudio);
            thickness = 2.5f;

            if (hasSignal)
            {
                colour = colour.brighter (0.5f);
                thickness = 3.5f;
            }
        }

        if (connection == hoveredConnection)
        {
            colour = juce::Colour (0xffcc88ff);   // light purple: the cable under the mouse
            thickness = 3.5f;
        }

        drawWire (g, start, end, colour, thickness);
    }

    if (dragCable.active)
        drawWire (g, getPinCenterPos (dragCable.sourcePin), dragCable.currentDragPos, dragCable.dragColour, 2.5f);

    for (auto* node : graph->getNodes())
        drawNode (g, node);

    // Where a tool dropped from the panel will land.
    if (isDragHovering)
    {
        const auto at = toVirtual (dragHoverPosition.toFloat());
        g.setColour (juce::Colour (200, 160, 255).withAlpha (0.5f));
        g.drawRoundedRectangle ({ at.x, at.y, Style::minNodeWidth, Style::nodeHeight }, Style::nodeRounding, 2.0f);
    }

    g.restoreState();
    drawScrollbars (g);
    drawMinimap (g);
    drawHeader (g);
}

void ContainerGraphCanvas::drawNode (juce::Graphics& g, Node* node)
{
    auto bounds = getNodeBounds (node);
    auto* cache = getCachedNodeType (node->nodeID);
    const bool bypassed = current().isOff (node);
    const bool isContainer = cache != nullptr && cache->container != nullptr;
    const bool failed = cache != nullptr && ((cache->plugin != nullptr && cache->plugin->getFailure().isNotEmpty()) || cache->missing != nullptr);

    auto body = bypassed ? Style::colNodeBodyBypassed : Style::colNodeBody;

    if (isContainer)
        body = bypassed ? juce::Colour (55, 55, 60) : juce::Colour (75, 80, 90);

    g.setColour (body);
    g.fillRoundedRectangle (bounds, Style::nodeRounding);
    g.setColour (failed ? juce::Colours::red : bypassed ? juce::Colours::grey.darker() : Style::colNodeBorder);
    g.drawRoundedRectangle (bounds, Style::nodeRounding, 2.0f);

    auto titleBounds = bounds.removeFromTop (Style::nodeTitleHeight);
    auto titleColour = bypassed ? Style::colNodeTitleBypassed : Style::colNodeTitle;

    if (isContainer)
        titleColour = bypassed ? juce::Colour (60, 65, 75) : juce::Colour (50, 90, 130);
    else if (failed)
        titleColour = juce::Colour (120, 30, 30);

    g.setColour (titleColour);
    g.fillRoundedRectangle (titleBounds, Style::nodeRounding);
    g.fillRect (titleBounds.removeFromBottom (Style::nodeRounding));

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    g.drawText (cache != nullptr ? cache->pluginName : juce::String ("Unknown"), titleBounds.reduced (5.0f, 0.0f), juce::Justification::centredLeft, true);

    // Stereo Meter: the bars, peaks and clip lights.
    if (cache != nullptr && cache->stereoMeter != nullptr)
    {
        auto* meter = cache->stereoMeter;
        const auto area = bounds.reduced (8.0f, 6.0f).withTrimmedBottom (Style::bottomBtnHeight + Style::bottomBtnMargin);
        const auto barWidth = (area.getWidth() - 6.0f) / 2.0f;

        const auto drawBar = [&] (juce::Rectangle<float> bar, float level, float peak, bool clipping, const char* label)
        {
            level = juce::jlimit (0.0f, 1.0f, level);
            peak = juce::jlimit (0.0f, 1.0f, peak);
            g.setColour (juce::Colour (30, 30, 30));
            g.fillRect (bar);

            if (level > 0.0f)
            {
                const auto fill = level * bar.getHeight();
                g.setColour (level < 0.75f ? juce::Colours::green : level < 0.90f ? juce::Colours::yellow : juce::Colours::red);
                g.fillRect (bar.getX(), bar.getBottom() - fill, bar.getWidth(), fill);
            }

            if (peak > 0.0f)
            {
                const auto py = bar.getBottom() - peak * bar.getHeight();
                g.setColour (juce::Colours::white);
                g.drawLine (bar.getX(), py, bar.getRight(), py, 2.0f);
            }

            g.setColour (juce::Colours::grey);
            g.drawRect (bar, 1.0f);
            g.setColour (juce::Colours::white);
            g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
            g.drawText (label, bar.withTop (bar.getBottom() - 12.0f), juce::Justification::centred);

            if (clipping)
            {
                g.setColour (juce::Colours::red.brighter());
                g.fillEllipse (bar.getX() + 2.0f, bar.getY() + 2.0f, bar.getWidth() - 4.0f, 8.0f);
            }
        };

        drawBar ({ area.getX(), area.getY(), barWidth, area.getHeight() }, meter->getLeftLevel(), meter->getLeftPeak(), meter->isLeftClipping(), "L");
        drawBar ({ area.getX() + barWidth + 6.0f, area.getY(), barWidth, area.getHeight() }, meter->getRightLevel(), meter->getRightPeak(), meter->isRightClipping(), "R");
    }

    // MIDI Monitor: the last message on each channel.
    if (cache != nullptr && cache->midiMonitor != nullptr)
    {
        const auto events = cache->midiMonitor->getEvents();
        const auto area = bounds.reduced (6.0f, 4.0f).withTrimmedBottom (Style::bottomBtnHeight + Style::bottomBtnMargin);
        g.setColour (juce::Colour (25, 25, 25));
        g.fillRect (area);
        g.setColour (juce::Colours::grey.darker());
        g.drawRect (area, 1.0f);
        g.setFont (juce::Font (juce::FontOptions (11.0f).withName (juce::Font::getDefaultMonospacedFontName())));
        auto y = area.getY();
        bool any = false;

        for (const auto& event : events)
        {
            if (! event.isActive)
                continue;

            any = true;
            g.setColour (event.isCC ? juce::Colour (80, 200, 255) : event.isNoteOn ? juce::Colour (0, 255, 100) : juce::Colour (255, 150, 0));
            g.drawText (event.toString(), juce::Rectangle<float> (area.getX() + 4.0f, y, area.getWidth() - 8.0f, 16.0f), juce::Justification::centredLeft, true);
            y += 16.0f;

            if (y + 16.0f > area.getBottom())
                break;
        }

        if (! any)
        {
            g.setColour (juce::Colours::grey);
            g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::italic)));
            g.drawText ("Waiting for MIDI...", area, juce::Justification::centred);
        }
    }

    // Connector: its volume, dragged up and down.
    if (cache != nullptr && cache->connector != nullptr)
    {
        const auto area = buttonsFor (node).volume;
        const auto db = cache->connector->getVolumeDb();
        g.setColour (juce::Colour (35, 25, 45));
        g.fillRoundedRectangle (area, 4.0f);
        const auto fill = area.withWidth (area.getWidth() * cache->connector->getVolume());
        g.setColour (juce::Colour (140, 80, 190).withAlpha (cache->connector->isMuted() ? 0.3f : 0.8f));
        g.fillRoundedRectangle (fill, 4.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        g.drawText (db <= -99.0f ? juce::String ("-inf dB") : juce::String (db, 1) + " dB", area, juce::Justification::centred);
    }

    // The Latcher: its pads (click one to latch it) and ALL OFF.
    if (cache != nullptr && cache->latcher != nullptr)
    {
        const auto b = buttonsFor (node);
        const auto latchedCount = cache->latcher->countLatched();
        g.setColour (latchedCount > 0 ? juce::Colour (0xffcc3333) : juce::Colour (60, 60, 60));
        g.fillRoundedRectangle (b.allOff, 3.0f);
        g.setColour (latchedCount > 0 ? juce::Colours::white : juce::Colours::grey);
        g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
        g.drawText ("ALL OFF", b.allOff, juce::Justification::centred);

        const auto padW = b.pads.getWidth() / 4.0f, padH = b.pads.getHeight() / 4.0f;

        for (int index = 0; index < NodeTools::Latcher::numPads; ++index)
        {
            const auto r = juce::Rectangle<float> (b.pads.getX() + static_cast<float> (index % 4) * padW + 1.0f,
                                                   b.pads.getY() + static_cast<float> (index / 4) * padH + 1.0f, padW - 2.0f, padH - 2.0f);

            if (cache->latcher->isPadLatched (index))
            {
                g.setColour (juce::Colour (0xffb8860b));
                g.fillRoundedRectangle (r, 3.0f);
                g.setColour (juce::Colour (0xffffd700).withAlpha (0.3f));
                g.fillRoundedRectangle (r.expanded (1.0f), 4.0f);
                g.setColour (juce::Colour (0xffffd700));
                g.drawRoundedRectangle (r, 3.0f, 1.0f);
            }
            else
            {
                g.setColour (juce::Colour (180, 180, 185));
                g.fillRoundedRectangle (r, 3.0f);
                g.setColour (juce::Colour (140, 140, 145));
                g.drawRoundedRectangle (r, 3.0f, 0.8f);
                g.setColour (juce::Colours::black);
            }

            g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
            g.drawText (juce::String (index + 1), r, juce::Justification::centred);
        }
    }

    // The Step Seq: play and stop, its tempo (the song's with SYN) and how many lanes play.
    if (cache != nullptr && cache->stepSeq != nullptr)
    {
        auto* seq = cache->stepSeq;
        const auto b = buttonsFor (node);
        const auto playing = seq->isPlaying();

        g.setColour (playing ? juce::Colour (20, 70, 20) : juce::Colour (45, 45, 50));
        g.fillRoundedRectangle (b.play, 6.0f);
        g.setColour (playing ? juce::Colours::green.darker() : juce::Colour (80, 80, 90));
        g.drawRoundedRectangle (b.play, 6.0f, 1.0f);

        if (playing)
        {
            const auto size = b.play.getHeight() * 0.38f;
            g.setColour (juce::Colour (60, 140, 220));
            g.fillRect (b.play.withSizeKeepingCentre (size, size));
        }
        else
        {
            const auto h = b.play.getHeight() * 0.5f;
            const auto c = b.play.getCentre();
            juce::Path triangle;
            triangle.addTriangle (c.x - h * 0.35f, c.y - h * 0.5f, c.x - h * 0.35f, c.y + h * 0.5f, c.x + h * 0.55f, c.y);
            g.setColour (juce::Colour (80, 200, 80));
            g.fillPath (triangle);
        }

        const auto synced = seq->isSyncedToSongTempo();
        g.setColour (juce::Colour (30, 30, 35));
        g.fillRoundedRectangle (b.bpm, 4.0f);
        g.setColour (juce::Colour (60, 60, 70));
        g.drawRoundedRectangle (b.bpm, 4.0f, 1.0f);
        g.setColour (synced ? juce::Colour (150, 150, 170) : juce::Colour (200, 200, 220));
        g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
        g.drawText (juce::String (synced ? seq->getEffectiveBpm() : seq->getBpm(), 1), b.bpm.reduced (4.0f, 0.0f), juce::Justification::centredLeft);
        g.setColour (juce::Colour (120, 120, 140));
        g.setFont (juce::Font (juce::FontOptions (8.0f)));
        g.drawText ("BPM", b.bpm.reduced (4.0f, 2.0f), juce::Justification::bottomRight);

        g.setColour (synced ? juce::Colour (30, 90, 30) : juce::Colour (45, 45, 50));
        g.fillRoundedRectangle (b.sync, 4.0f);
        g.setColour (synced ? juce::Colours::limegreen.darker() : juce::Colour (80, 80, 90));
        g.drawRoundedRectangle (b.sync, 4.0f, 1.0f);
        g.setColour (synced ? juce::Colours::limegreen : juce::Colour (120, 120, 140));
        g.setFont (juce::Font (juce::FontOptions (8.0f, juce::Font::bold)));
        g.drawText ("SYN", b.sync, juce::Justification::centred);

        g.setColour (juce::Colour (140, 140, 160));
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (juce::String (seq->countEnabled()) + "/16",
                    juce::Rectangle<float> (b.sync.getRight() + 6.0f, b.play.getY(), bounds.getRight() - b.sync.getRight() - 14.0f, b.play.getHeight()),
                    juce::Justification::centredRight);
    }

    // The Transient Splitter: what goes to each pair of outputs.
    if (cache != nullptr && cache->transientSplitter != nullptr)
    {
        auto* splitter = cache->transientSplitter;
        auto area = bounds.reduced (10.0f, 8.0f).withTrimmedBottom (Style::bottomBtnHeight + Style::bottomBtnMargin + 2.0f);
        const auto rowH = juce::jmin (16.0f, (area.getHeight() - 4.0f) / 2.0f);

        const auto bar = [&g] (juce::Rectangle<float> r, float rms, juce::Colour colour, const char* label)
        {
            g.setColour (juce::Colour (25, 25, 30));
            g.fillRoundedRectangle (r, 2.0f);
            const auto fill = juce::jlimit (0.0f, 1.0f, juce::jmap (juce::Decibels::gainToDecibels (rms, -60.0f), -60.0f, 0.0f, 0.0f, 1.0f));

            if (fill > 0.001f)
            {
                g.setColour (colour.withAlpha (0.8f));
                g.fillRoundedRectangle (r.reduced (1.0f).withWidth ((r.getWidth() - 2.0f) * fill), 1.5f);
            }

            g.setColour (juce::Colour (200, 200, 210));
            g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
            g.drawText (label, r.withTrimmedLeft (4.0f), juce::Justification::centredLeft);
        };

        bar (area.removeFromTop (rowH), juce::jmax (splitter->transientRmsL.load(), splitter->transientRmsR.load()), juce::Colour (0, 200, 255), "TRANSIENT  1-2");
        area.removeFromTop (4.0f);
        bar (area.removeFromTop (rowH), juce::jmax (splitter->sustainRmsL.load(), splitter->sustainRmsR.load()), juce::Colour (255, 180, 50), "SUSTAIN  3-4");
    }

    // What went wrong, across the body.
    if (failed)
    {
        const auto why = cache->missing != nullptr ? juce::String ("Not found") : juce::String ("Stopped");
        g.setColour (juce::Colours::red.withAlpha (0.85f));
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        g.drawText (why, bounds.withTrimmedBottom (Style::bottomBtnHeight + Style::bottomBtnMargin * 2.0f), juce::Justification::centred);
    }

    drawNodePins (g, node);

    if (cache != nullptr && (cache->isAudioInput || cache->isAudioOutput))
        drawAudioIOToggle (g, node);
    else if (cache != nullptr && ! cache->isIO)
        drawNodeButtons (g, node);
}

void ContainerGraphCanvas::drawNodePins (juce::Graphics& g, Node* node)
{
    auto* proc = node->getProcessor();
    auto* cache = getCachedNodeType (node->nodeID);
    auto* plugin = cache != nullptr ? cache->plugin : nullptr;
    const bool live = ! current().isOff (node);

    const auto drawOne = [&] (const PinID& pin, juce::Colour colour, bool active)
    {
        const auto r = getPinBounds (pin, node);

        if (r.isEmpty())
            return;

        if (active)
            colour = colour.brighter (0.5f);

        // A pin the cable being dragged can land on glows yellow.
        if (dragCable.active && highlightPin == pin && canConnect (dragCable.sourcePin, pin))
        {
            g.setColour (juce::Colours::yellow.withAlpha (0.6f));
            g.fillEllipse (r.expanded (3.0f));
            g.setColour (juce::Colours::yellow);
            g.fillEllipse (r);
            g.setColour (juce::Colours::white);
            g.drawEllipse (r, 2.0f);
            return;
        }

        if (highlightPin == pin)
        {
            g.setColour (colour.brighter (0.5f));
            g.fillEllipse (r.expanded (2.0f));
        }

        g.setColour (colour);
        g.fillEllipse (r);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (r, 1.0f);
    };

    for (int i = 0; i < inputPinCount (node); ++i)
    {
        const PinID pin { node->nodeID, i, true, false };
        drawOne (pin, getPinColour (pin, node), false);
    }

    if (proc->acceptsMidi())
        drawOne ({ node->nodeID, 0, true, true }, Style::colPinMidi, live && plugin != nullptr && plugin->isMidiInActive());

    for (int i = 0; i < proc->getTotalNumOutputChannels(); ++i)
    {
        const PinID pin { node->nodeID, i, false, false };
        drawOne (pin, getPinColour (pin, node), false);
    }

    if (proc->producesMidi())
        drawOne ({ node->nodeID, 0, false, true }, Style::colPinMidi, live && plugin != nullptr && plugin->isMidiOutActive());
}

void ContainerGraphCanvas::drawNodeButtons (juce::Graphics& g, Node* node)
{
    auto* cache = getCachedNodeType (node->nodeID);

    if (cache == nullptr)
        return;

    const auto b = buttonsFor (node);
    const auto black = juce::Colours::black;

    if (cache->container != nullptr)
    {
        drawButton (g, b.edit, juce::Colours::cyan.darker(), black, "E");
        drawButton (g, b.mute, cache->container->isMuted() ? juce::Colours::red : juce::Colours::lightgreen, black, "M");
        drawButton (g, b.transport, cache->container->isTransportSynced() ? juce::Colours::yellow.darker (0.4f) : juce::Colours::yellow, black, "T");
        drawButton (g, b.remove, juce::Colours::darkred, juce::Colours::white, "X");
        return;
    }

    if (cache->plugin != nullptr)
    {
        drawButton (g, b.edit, juce::Colours::cyan.darker(), black, "E");

        if (! b.channel.isEmpty())
        {
            const auto mask = cache->plugin->getMidiChannelMask();
            juce::String label ("CH");

            if (mask != 0xffff)
                for (int i = 0; i < 16; ++i)
                    if ((mask >> i) & 1)
                    {
                        label = juce::String (i + 1);
                        break;
                    }

            drawButton (g, b.channel, juce::Colours::orange.darker(), black, label, 10.0f);
        }

        drawButton (g, b.mute, node->isBypassed() ? juce::Colours::red : juce::Colours::lightgreen, black, "M");
        drawButton (g, b.pass, cache->plugin->isPassThrough() ? juce::Colours::yellow : juce::Colours::grey.darker(), black, "P");
        drawButton (g, b.transport, cache->plugin->isTransportSynced() ? juce::Colours::yellow.darker (0.4f) : juce::Colours::yellow, black, "T");
        drawButton (g, b.remove, juce::Colours::darkred, juce::Colours::white, "X");
        return;
    }

    if (cache->connector != nullptr)
        drawButton (g, b.mute, cache->connector->isMuted() ? juce::Colours::red : juce::Colours::lightgreen, black, "M");

    if (! b.edit.isEmpty())
        drawButton (g, b.edit, juce::Colours::cyan.darker(), black, "E");   // a tool's editor

    drawButton (g, b.remove, juce::Colours::darkred, juce::Colours::white, "X");
}

void ContainerGraphCanvas::drawAudioIOToggle (juce::Graphics& g, Node* node)
{
    const auto b = buttonsFor (node);
    const auto off = current().isOff (node);
    drawButton (g, b.toggle, off ? juce::Colours::grey.darker() : juce::Colours::green, juce::Colours::white, off ? "OFF" : "ON", 9.0f);
}

ContainerGraphCanvas::HeaderLayout ContainerGraphCanvas::headerLayout() const
{
    HeaderLayout h;
    const auto w = static_cast<float> (getWidth());
    const float btnH = 28.0f, btnY = (headerHeight - btnH) / 2.0f, margin = 8.0f;
    const float zoomBtnW = 26.0f, zoomPctW = 46.0f;
    const float zoomX = w - margin - scrollbarThickness - (zoomBtnW + zoomPctW + zoomBtnW);

    h.back = isNested() ? juce::Rectangle<float> (margin, btnY, 80.0f, btnH) : juce::Rectangle<float>();
    const auto nameX = isNested() ? margin + 80.0f + 12.0f : margin;
    h.name = { nameX, btnY, juce::jmax (60.0f, zoomX - nameX - 12.0f), btnH };
    h.zoomMinus = { zoomX, btnY, zoomBtnW, btnH };
    h.zoomPct = { zoomX + zoomBtnW, btnY, zoomPctW, btnH };
    h.zoomPlus = { zoomX + zoomBtnW + zoomPctW, btnY, zoomBtnW, btnH };
    return h;
}

void ContainerGraphCanvas::drawHeader (juce::Graphics& g)
{
    const auto w = static_cast<float> (getWidth());
    g.setColour (juce::Colour (45, 18, 68));
    g.fillRect (0.0f, 0.0f, w, headerHeight);
    g.setColour (juce::Colour (140, 70, 200));
    g.fillRect (0.0f, headerHeight - 1.5f, w, 1.5f);

    const auto h = headerLayout();

    if (! h.back.isEmpty())
    {
        g.setColour (juce::Colour (80, 40, 110));
        g.fillRoundedRectangle (h.back, 5.0f);
        g.setColour (juce::Colour (200, 150, 255));
        g.drawRoundedRectangle (h.back, 5.0f, 1.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        g.drawText (juce::CharPointer_UTF8 ("\xe2\x86\x90 Back"), h.back, juce::Justification::centred);
    }

    // The name (click to rename), and the way in when inside another container.
    g.setColour (juce::Colour (70, 30, 100));
    g.fillRoundedRectangle (h.name, 4.0f);
    g.setColour (juce::Colour (140, 80, 190));
    g.drawRoundedRectangle (h.name, 4.0f, 1.0f);
    g.setColour (juce::Colour (220, 180, 255));
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    juce::String path = root.getContainerName();

    for (auto* inner : stack)
        path << "  >  " << inner->getContainerName();

    g.drawText (path + juce::String (juce::CharPointer_UTF8 ("  \xe2\x9c\x8e")), h.name.reduced (6.0f, 0.0f), juce::Justification::centredLeft, true);

    const auto zoomButton = [&g] (juce::Rectangle<float> r, const char* label)
    {
        g.setColour (juce::Colour (60, 30, 90));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colour (140, 80, 190));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
        g.drawText (label, r, juce::Justification::centred);
    };

    zoomButton (h.zoomMinus, "-");
    g.setColour (juce::Colour (50, 20, 75));
    g.fillRect (h.zoomPct);
    g.setColour (juce::Colour (200, 170, 255));
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.drawText (juce::String (juce::roundToInt (zoomLevel * 100.0f)) + "%", h.zoomPct, juce::Justification::centred);
    zoomButton (h.zoomPlus, "+");
}

juce::Rectangle<float> ContainerGraphCanvas::getMinimapRect() const
{
    const auto w = static_cast<float> (getWidth()), h = static_cast<float> (getHeight());
    return { w - minimapWidth - minimapMargin - scrollbarThickness, h - minimapHeight - minimapMargin - scrollbarThickness, minimapWidth, minimapHeight };
}

juce::Rectangle<float> ContainerGraphCanvas::getHScrollbarRect() const
{
    return { 0.0f, static_cast<float> (getHeight()) - scrollbarThickness, static_cast<float> (getWidth()) - scrollbarThickness, scrollbarThickness };
}

juce::Rectangle<float> ContainerGraphCanvas::getVScrollbarRect() const
{
    return { static_cast<float> (getWidth()) - scrollbarThickness, headerHeight, scrollbarThickness, static_cast<float> (getHeight()) - scrollbarThickness - headerHeight };
}

void ContainerGraphCanvas::drawMinimap (juce::Graphics& g)
{
    const auto mm = getMinimapRect();
    g.setColour (juce::Colour (0xdd101018));
    g.fillRoundedRectangle (mm, 4.0f);
    g.setColour (juce::Colour (0xff505060));
    g.drawRoundedRectangle (mm, 4.0f, 1.0f);

    const auto scaleX = mm.getWidth() / virtualCanvasWidth, scaleY = mm.getHeight() / virtualCanvasHeight;
    g.setColour (juce::Colours::white.withAlpha (0.06f));

    for (int i = 0; i <= 4; ++i)
    {
        g.drawVerticalLine (static_cast<int> (mm.getX() + static_cast<float> (i) * 4800.0f * scaleX), mm.getY(), mm.getBottom());
        g.drawHorizontalLine (static_cast<int> (mm.getY() + static_cast<float> (i) * 3200.0f * scaleY), mm.getX(), mm.getRight());
    }

    for (auto* node : getActiveGraph()->getNodes())
    {
        const auto bounds = getNodeBounds (node);
        auto* cache = getCachedNodeType (node->nodeID);
        auto colour = juce::Colour (0xff00aaff);

        if (cache != nullptr)
        {
            if (cache->isIO)                       colour = juce::Colour (0xffffd700);   // gold: I/O
            else if (cache->isInstrument)          colour = juce::Colour (0xff00ff88);   // green: instruments
            else if (cache->plugin == nullptr)     colour = juce::Colour (0xffff8800);   // orange: system tools
            else if (current().isOff (node))       colour = juce::Colour (0xff666666);
        }

        g.setColour (colour);
        g.fillEllipse (mm.getX() + bounds.getCentreX() * scaleX - 2.0f, mm.getY() + bounds.getCentreY() * scaleY - 2.0f, 4.0f, 4.0f);
    }

    const auto view = juce::Rectangle<float> (mm.getX() + panOffsetX * scaleX, mm.getY() + panOffsetY * scaleY,
                                              juce::jmin (getVisibleWidth() * scaleX, mm.getWidth()),
                                              juce::jmin (getVisibleHeight() * scaleY, mm.getHeight()));
    g.setColour (juce::Colour (0x20ffffff));
    g.fillRect (view);
    g.setColour (juce::Colour (0xccffffff));
    g.drawRect (view, 1.5f);
    g.setColour (juce::Colour (0x99ffffff));
    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
    g.drawText ("MAP " + juce::String (juce::roundToInt (zoomLevel * 100.0f)) + "%",
                juce::Rectangle<float> (mm.getX() + 4.0f, mm.getY() + 2.0f, 80.0f, 12.0f), juce::Justification::centredLeft);
}

void ContainerGraphCanvas::drawScrollbars (juce::Graphics& g)
{
    const auto visW = getVisibleWidth(), visH = getVisibleHeight();

    {
        const auto bar = getHScrollbarRect();
        g.setColour (juce::Colour (0xaa181820));
        g.fillRect (bar);
        g.setColour (juce::Colour (0xff383840));
        g.drawRect (bar, 1.0f);
        const auto ratio = visW / virtualCanvasWidth;

        if (ratio < 1.0f)
        {
            const auto thumbW = juce::jmax (20.0f, bar.getWidth() * ratio);
            const auto maxPan = virtualCanvasWidth - visW;
            const auto thumbX = bar.getX() + (maxPan > 0.0f ? panOffsetX / maxPan * (bar.getWidth() - thumbW) : 0.0f);
            g.setColour (isDraggingHScrollbar ? juce::Colour (0xffaabbcc) : juce::Colour (0xff667788));
            g.fillRoundedRectangle ({ thumbX, bar.getY() + 2.0f, thumbW, bar.getHeight() - 4.0f }, 3.0f);
        }
    }

    {
        const auto bar = getVScrollbarRect();
        g.setColour (juce::Colour (0xaa181820));
        g.fillRect (bar);
        g.setColour (juce::Colour (0xff383840));
        g.drawRect (bar, 1.0f);
        const auto ratio = visH / virtualCanvasHeight;

        if (ratio < 1.0f)
        {
            const auto thumbH = juce::jmax (20.0f, bar.getHeight() * ratio);
            const auto maxPan = virtualCanvasHeight - visH;
            const auto thumbY = bar.getY() + (maxPan > 0.0f ? panOffsetY / maxPan * (bar.getHeight() - thumbH) : 0.0f);
            g.setColour (isDraggingVScrollbar ? juce::Colour (0xffaabbcc) : juce::Colour (0xff667788));
            g.fillRoundedRectangle ({ bar.getX() + 2.0f, thumbY, bar.getWidth() - 4.0f, thumbH }, 3.0f);
        }
    }

    g.setColour (juce::Colour (0xff181820));
    g.fillRect (juce::Rectangle<float> (static_cast<float> (getWidth()) - scrollbarThickness, static_cast<float> (getHeight()) - scrollbarThickness,
                                        scrollbarThickness, scrollbarThickness));
}

//==============================================================================
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#endif

void ContainerCanvas::keepInFrontOf (juce::Component& window, void* ownerHandle)
{
   #if JUCE_WINDOWS
    auto* peer = window.getPeer();

    if (peer == nullptr || ownerHandle == nullptr)
        return;

    const auto handle = static_cast<HWND> (peer->getNativeHandle());
    const auto owner = static_cast<HWND> (ownerHandle);

    // An owned window: always above its owner, minimised and restored with it.
    if (handle != owner && reinterpret_cast<HWND> (GetWindowLongPtrW (handle, GWLP_HWNDPARENT)) != owner)
        SetWindowLongPtrW (handle, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR> (owner));
   #else
    juce::ignoreUnused (window, ownerHandle);
   #endif
}
