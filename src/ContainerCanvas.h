// D:\workspace\Fanan_web_hub\src\ContainerCanvas.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Container.h"
#include "NodeTools.h"

#include <map>
#include <memory>
#include <vector>

//==============================================================================
/**
    The Node Rack's window, as Colosseum's: a bar with its pages (Rack, Mixer, Manual) and its
    Browse, Load Patch, Save Patch, Reset and PANIC; under it the page.

    The Rack is Colosseum's node canvas on the rack's graph: nodes with their pins (blue audio,
    green sidechain, red MIDI) and buttons (E, CH, M, P, T, X), cables dragged from pin to pin
    (a connected input pin picks its cable up again), the canvas panned and zoomed, with a
    minimap and scrollbars, and a header to rename it. A Container inside is entered with its E
    (or a double-click) and left with Back. Nodes come from the right-click menu (the system
    tools, Sonify's plugins, saved patches), from Browse (a plugin file), or dragged in: a
    plugin from Sonify's Browser, a plugin file from the computer's folders.

    The window stays in front of Sonify's own (it belongs to it), so the rack never hides
    behind Sonify while a plugin's screen or the Browser is used.

    ContainerCanvas::install() makes racks open this window (MainComponent calls it).
*/
namespace ContainerCanvas
{
    void install();

    /** Makes window belong to the window with ownerHandle (a native handle): it stays in front
        of it, and goes with it when it's minimised. Windows only; elsewhere nothing. */
    void keepInFrontOf (juce::Component& window, void* ownerHandle);
}

namespace CanvasStyle
{
    const auto colBackground        = juce::Colour (0xff1e1e1e);
    const auto colContainerBg       = juce::Colour (28, 14, 42);
    const auto colNodeBody          = juce::Colour (0xff2d2d2d);
    const auto colNodeBodyBypassed  = juce::Colour (0xff3a3a3a);
    const auto colNodeBorder        = juce::Colour (0xff555555);
    const auto colNodeTitle         = juce::Colour (0xff505050);
    const auto colNodeTitleBypassed = juce::Colour (0xff404040);
    const auto colPinAudio          = juce::Colour (0xff00aaff);   // blue
    const auto colPinSidechain      = juce::Colour (0xff00ff00);   // green
    const auto colPinMidi           = juce::Colour (0xffff0000);   // red

    constexpr float nodeHeight       = 60.0f;
    constexpr float nodeWidth        = 152.0f;
    constexpr float nodeRounding     = 5.0f;
    constexpr float nodeTitleHeight  = 24.0f;
    constexpr float pinSize          = 8.0f;
    constexpr float hookLength       = 10.0f;
    constexpr float minNodeWidth     = 156.0f;
    constexpr float minPinSpacing    = 18.0f;
    constexpr float bottomBtnWidth   = 20.0f;
    constexpr float bottomBtnHeight  = 20.0f;
    constexpr float bottomBtnSpacing = 4.0f;
    constexpr float bottomBtnMargin  = 4.0f;
}

//==============================================================================
/** Colosseum's little callout: what a pin, button or cable is, and whether it's live. */
class StatusToolTip final : public juce::Component, private juce::Timer
{
public:
    StatusToolTip (const juce::String& title, bool isStreaming, std::function<void()> onDelete = nullptr);
    ~StatusToolTip() override;
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    juce::Label titleLabel, statusLabel;
    juce::TextButton deleteButton { "Delete Wire" };
    bool streaming;
    float alpha = 0.0f, delta = 0.1f;
    std::function<void()> deleteCallback;
};

//==============================================================================
/** CH: which MIDI channel an instrument node hears (or all of them). */
class MidiChannelSelector final : public juce::Component, private juce::Button::Listener
{
public:
    MidiChannelSelector (PluginNode& node, std::function<void()> onUpdate);
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void buttonClicked (juce::Button* button) override;
    void showSelection();

    PluginNode& node;
    std::function<void()> updateCallback;
    juce::Label titleLabel;
    juce::TextButton closeButton { "X" }, allButton { "All Channels" };
    juce::OwnedArray<juce::TextButton> channelButtons;
};

//==============================================================================
/** T: a tempo and time signature of a node's (or a container's) own. */
class TransportOverridePanel final : public juce::Component,
                                     private juce::Slider::Listener,
                                     private juce::ComboBox::Listener,
                                     private juce::Button::Listener
{
public:
    struct Target
    {
        juce::String title;
        std::function<bool()> isSynced;
        std::function<void (bool)> setSynced;
        std::function<double()> getTempo;
        std::function<void (double)> setTempo;
        std::function<std::pair<int, int>()> getSignature;
        std::function<void (int, int)> setSignature;
    };

    explicit TransportOverridePanel (Target target);
    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    void buttonClicked (juce::Button* button) override;
    void comboBoxChanged (juce::ComboBox* combo) override;
    void sliderValueChanged (juce::Slider* slider) override;
    void selectSignature (int numerator, int denominator);
    void updateEnabledState (bool synced);

    Target target;
    juce::Label titleLabel, syncLabel, timeSigLabel, tempoLabel;
    juce::TextButton yesButton, noButton;
    juce::ComboBox timeSigCombo;
    juce::Slider tempoSlider;
};

//==============================================================================
/** The node canvas (Colosseum's GraphCanvas) on a container. */
class ContainerGraphCanvas final : public juce::Component,
                                   public juce::MultiTimer,
                                   public juce::DragAndDropTarget,
                                   public juce::FileDragAndDropTarget,
                                   public juce::TextDragAndDropTarget
{
public:
    explicit ContainerGraphCanvas (ContainerPlugin& root);
    ~ContainerGraphCanvas() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void timerCallback (int timerId) override;

    bool isInterestedInDragSource (const SourceDetails& details) override;
    void itemDragEnter (const SourceDetails& details) override;
    void itemDragMove (const SourceDetails& details) override;
    void itemDragExit (const SourceDetails& details) override;
    void itemDropped (const SourceDetails& details) override;

    // Plugin files dragged in from the computer's folders (.vst3, .clap, .sip).
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragMove (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    // A plugin dragged in from Sonify's Browser (it carries "sonify-plugin|<id>|<file>").
    bool isInterestedInTextDrag (const juce::String& text) override;
    void textDragEnter (const juce::String& text, int x, int y) override;
    void textDragMove (const juce::String& text, int x, int y) override;
    void textDragExit (const juce::String& text) override;
    void textDropped (const juce::String& text, int x, int y) override;

    /** What Sonify's Browser puts on a plugin it drags out. */
    static constexpr const char* browserDragPrefix = "sonify-plugin|";

    /** Adds what a menu or a drop names ("TOOL:Connector", "PLUGIN:<file>|<id>",
        "CONTAINERPRESET:<path>", "FILE:" to browse) at a place on the canvas (virtual coordinates). */
    void addFromTools (const juce::String& dragId, Containers::Spot at);
    /** The middle of what's in view. */
    Containers::Spot centreOfView() const;

    // The window's bar: what's shown (the rack, or the Container entered in it).
    void savePreset();
    void loadPreset();
    void resetRack();
    void panic();
    void addPluginFile (Containers::Spot at);

    /** The container shown: the window's, or one inside it that was entered. */
    ContainerPlugin& current() const noexcept          { return stack.empty() ? root : *stack.back(); }

    /** current(), once what was entered is known to be still there (for the Mixer). */
    ContainerPlugin& shownRack()                        { refreshIfStale(); return current(); }
    juce::AudioProcessorGraph* getActiveGraph() const   { return &current().getGraph(); }
    bool isNested() const noexcept                      { return ! stack.empty(); }

    void diveInto (ContainerPlugin& container);
    void diveOut();
    void diveToTop();

    std::function<void()> onTitleChanged;              // the window's title follows the name

    enum TimerIds { mainTimer = 1, meterTimer = 2, mouseTimer = 3 };

    static constexpr float virtualCanvasWidth  = 19200.0f;
    static constexpr float virtualCanvasHeight = 12800.0f;
    static constexpr float headerHeight = 44.0f;

    float getZoomLevel() const noexcept                 { return zoomLevel; }
    void setZoomLevel (float zoom);
    void markDirty() noexcept                           { needsRepaint = true; }

    using NodeID = juce::AudioProcessorGraph::NodeID;
    using Node = juce::AudioProcessorGraph::Node;
    using Connection = juce::AudioProcessorGraph::Connection;

    // Where things are on the canvas (virtual coordinates), and where that is in the component.
    juce::Rectangle<float> getNodeBounds (Node* node) const;
    juce::Point<float> pinPosition (NodeID node, int pin, bool isInput, bool isMidi) const;
    juce::Point<float> toLocal (juce::Point<float> virtualPos) const noexcept
    {
        return { (virtualPos.x - panOffsetX) * zoomLevel, (virtualPos.y - panOffsetY) * zoomLevel };
    }

    struct Buttons
    {
        juce::Rectangle<float> edit, channel, mute, pass, transport, remove, toggle, volume;
        juce::Rectangle<float> play, bpm, sync, allOff, pads;   // the Step Seq's and the Latcher's faces
    };
    Buttons buttonsFor (Node* node) const;

private:
    struct PinID
    {
        NodeID nodeID;
        int pinIndex = 0;
        bool isInput = false, isMidi = false;
        bool isValid() const noexcept { return nodeID.uid != 0; }
        bool operator== (const PinID& o) const noexcept { return nodeID == o.nodeID && pinIndex == o.pinIndex && isInput == o.isInput && isMidi == o.isMidi; }
        bool operator!= (const PinID& o) const noexcept { return ! (*this == o); }
    };

    struct DraggingCable
    {
        PinID sourcePin;
        juce::Point<float> currentDragPos;
        bool active = false;
        juce::Colour dragColour;
    };

    struct NodeTypeCache
    {
        PluginNode* plugin = nullptr;
        ContainerPlugin* container = nullptr;         // a container node's container
        ContainerTools::Connector* connector = nullptr;
        ContainerTools::StereoMeter* stereoMeter = nullptr;
        ContainerTools::MidiMonitor* midiMonitor = nullptr;
        ContainerTools::Missing* missing = nullptr;
        NodeTools::Latcher* latcher = nullptr;
        NodeTools::StepSeq* stepSeq = nullptr;
        NodeTools::TransientSplitter* transientSplitter = nullptr;
        bool isAudioInput = false, isAudioOutput = false, isMidiInput = false, isMidiOutput = false, isIO = false;
        bool isInstrument = false, hasSidechain = false;
        juce::String pluginName;
    };

    // Coordinates: virtual canvas <-> component pixels.
    juce::Point<float> toVirtual (juce::Point<float> local) const noexcept
    {
        return { local.x / zoomLevel + panOffsetX, local.y / zoomLevel + panOffsetY };
    }
    juce::Point<int> virtualToScreen (float vx, float vy) const;
    void clampPanOffset();
    float getVisibleWidth() const noexcept              { return static_cast<float> (getWidth()) / zoomLevel; }
    float getVisibleHeight() const noexcept             { return static_cast<float> (getHeight()) / zoomLevel; }

    // Cache of what each node is (names asked once: some plugins freeze if asked often).
    void rebuildNodeTypeCache();
    const NodeTypeCache* getCachedNodeType (NodeID id) const;
    void verifyPositions();
    void checkStack();

    /** Any container changed (or went) since the last look: the stack entered is checked and
        the cache made again, before anything uses either. */
    void refreshIfStale();

    /** Whether a container is still this window's or one inside it (it may have gone since). */
    bool holds (const ContainerPlugin* container) const;

    // Geometry
    int inputPinCount (Node* node) const;
    juce::Rectangle<float> getPinBounds (const PinID& pin, Node* node) const;
    juce::Point<float> getPinPos (Node* node, const PinID& pin) const   { return getPinBounds (pin, node).getCentre(); }
    juce::Point<float> getPinCenterPos (const PinID& pin) const;
    juce::Colour getPinColour (const PinID& pin, Node* node) const;
    bool isSidechainPin (const PinID& pin, Node* node) const;
    PinID findPinAt (juce::Point<float> pos) const;
    Node* findNodeAt (juce::Point<float> pos) const;
    Connection getConnectionAt (juce::Point<float> pos) const;
    bool canConnect (const PinID& start, const PinID& end) const;
    void createConnection (const PinID& start, const PinID& end);

    // Drawing
    void drawWire (juce::Graphics& g, juce::Point<float> start, juce::Point<float> end, juce::Colour colour, float thickness) const;
    void drawNode (juce::Graphics& g, Node* node);
    void drawNodePins (juce::Graphics& g, Node* node);
    void drawNodeButtons (juce::Graphics& g, Node* node);
    void drawAudioIOToggle (juce::Graphics& g, Node* node);
    void drawButton (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill, juce::Colour text, const juce::String& label, float fontSize = 11.0f) const;
    void drawHeader (juce::Graphics& g);
    void drawMinimap (juce::Graphics& g);
    void drawScrollbars (juce::Graphics& g);

    // Header
    struct HeaderLayout
    {
        juce::Rectangle<float> back, name, zoomMinus, zoomPct, zoomPlus;
    };
    HeaderLayout headerLayout() const;
    bool headerClick (const juce::MouseEvent& e);

    // Overlays
    juce::Rectangle<float> getMinimapRect() const;
    juce::Rectangle<float> getHScrollbarRect() const;
    juce::Rectangle<float> getVScrollbarRect() const;
    void navigateMinimapTo (juce::Point<float> local);

    // Actions
    void openPluginWindow (Node* node);
    void deleteNode (NodeID id);
    void toggleMute (Node* node);
    void showPluginMenu (juce::Point<float> virtualPos);
    void showNodeTitleMenu (Node* node);
    void showPinInfo (const PinID& pin, juce::Point<int> screenPos);
    void showWireMenu (const Connection& connection, juce::Point<int> screenPos);
    void showTip (const juce::String& text, juce::Point<int> screenPos, bool live = true);
    void showMidiChannelFilter (Node* node);
    void showTransportOverride (Node* node);
    void renameContainer();
    void showToolEditor (Node* node);
    void dropAt (const juce::String& dragId, int x, int y);
    void applyMagneticSnap (Node* dragged, float& x, float& y);
    void positionsMoved();

    ContainerPlugin& root;
    std::vector<ContainerPlugin*> stack;

    float zoomLevel = 0.7f, panOffsetX = 0.0f, panOffsetY = 0.0f;
    float savedRootZoom = 0.7f;
    bool isPanning = false;
    juce::Point<int> panMouseScreenStart;
    float panStartOffsetX = 0.0f, panStartOffsetY = 0.0f;

    std::map<NodeID, NodeTypeCache> nodeTypeCache;
    std::vector<Connection> cachedConnections;
    size_t lastNodeCount = 0, lastConnectionCount = 0;
    juce::uint32 seenGeneration = 0;                  // ContainerPlugin::getStructureGeneration() last looked at
    bool needsRepaint = true, hasLiveNodes = false;

    PinID highlightPin, lastHighlightPin;
    Connection hoveredConnection { { NodeID(), 0 }, { NodeID(), 0 } };
    Connection lastHoveredConnection { { NodeID(), 0 }, { NodeID(), 0 } };
    DraggingCable dragCable;
    NodeID draggingNodeID;
    juce::Point<float> nodeDragOffset;
    bool nodeMoved = false;

    // The Connector's volume, dragged up and down.
    NodeID draggingVolumeNodeID;
    float volumeDragStartY = 0.0f, volumeDragStartValue = 0.5f;

    // The Step Seq's own tempo, dragged up and down.
    NodeID draggingBpmNodeID;
    float bpmDragStartY = 0.0f;
    double bpmDragStartValue = 120.0;

    static constexpr float magneticSnapThreshold = 20.0f;

    // Drop target
    bool isDragHovering = false;
    juce::Point<int> dragHoverPosition;

    // Minimap and scrollbars
    static constexpr float minimapWidth = 154.0f, minimapHeight = 102.67f, minimapMargin = 8.0f, scrollbarThickness = 14.0f;
    bool isDraggingMinimap = false, isDraggingHScrollbar = false, isDraggingVScrollbar = false;
    float scrollbarDragStartOffset = 0.0f, scrollbarDragStartPan = 0.0f;

    std::shared_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ContainerGraphCanvas)
};

//==============================================================================
class NodeRackMixer;
class NodeRackManual;
class NodeRackMacros;

/** The window's content: the bar with the pages and the rack's buttons, and the page shown. */
class ContainerEditor final : public juce::Component, public juce::DragAndDropContainer
{
public:
    explicit ContainerEditor (ContainerPlugin& container);
    ~ContainerEditor() override;

    void resized() override;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

    ContainerGraphCanvas& getCanvas() noexcept          { return canvas; }

    enum class Page { rack, mixer, manual };
    void showPage (Page page);

    static constexpr int barHeight = 40;

private:
    struct BarLayout
    {
        juce::Rectangle<float> rack, mixer, manual, browse, load, save, reset, panic;
    };
    BarLayout barLayout() const;

    ContainerPlugin& container;
    ContainerGraphCanvas canvas;
    std::unique_ptr<NodeRackMacros> macros;     // the Rack page's macro knobs (the window's rack's)
    std::unique_ptr<NodeRackMixer> mixer;
    std::unique_ptr<NodeRackManual> manual;
    Page page = Page::rack;
    juce::Point<float> hover { -1.0f, -1.0f };
};
