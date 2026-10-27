// D:\workspace\Fanan_web_hub\src\ContainerCanvasMouse.cpp
//
// The container canvas's mouse work, menus and actions (Colosseum's GraphCanvas_Mouse,
// _Connections, _PluginMenu, _Tooltips and _DragDrop, on a container's graph).

#include "ContainerCanvas.h"
#include "NodeRackPages.h"

#include <cmath>

using Graph = juce::AudioProcessorGraph;
namespace Style = CanvasStyle;

//==============================================================================
// Actions
void ContainerGraphCanvas::openPluginWindow (Node* node)
{
    auto* cache = node != nullptr ? getCachedNodeType (node->nodeID) : nullptr;

    if (cache == nullptr || cache->plugin == nullptr)
        return;

    if (cache->container != nullptr)
    {
        diveInto (*cache->container);
        return;
    }

    auto& plugin = cache->plugin->getPlugin();

    // Its own window (a VST3 editor, the Playlist Player, the Sonify Sampler), or Sonify's page shows it.
    const auto title = current().getContainerName() + " - " + plugin.getDescriptor().name;

    if (plugin.getScreenKind() == Plugin::ScreenKind::native && plugin.showNativeScreen (title))
    {
        // It belongs to the rack's window: clicking the rack (which belongs to Sonify's) never
        // brings Sonify over the plugin.
        auto* rackPeer = getPeer();

        for (int i = juce::Desktop::getInstance().getNumComponents(); --i >= 0;)
            if (auto* window = dynamic_cast<juce::TopLevelWindow*> (juce::Desktop::getInstance().getComponent (i)))
                if (window != getTopLevelComponent() && window->getName() == title && window->isVisible() && rackPeer != nullptr)
                    ContainerCanvas::keepInFrontOf (*window, rackPeer->getNativeHandle());

        return;
    }

    if (auto* host = Containers::getHost())
        host->openScreen (plugin);
}

void ContainerGraphCanvas::deleteNode (NodeID id)
{
    // A Latcher going with pads latched: the notes it held are let go everywhere in the rack.
    auto* node = getActiveGraph()->getNodeForId (id);
    auto* latcher = node != nullptr ? dynamic_cast<NodeTools::Latcher*> (node->getProcessor()) : nullptr;
    const auto heldNotes = latcher != nullptr && latcher->countLatched() > 0;

    if (auto* seq = node != nullptr ? dynamic_cast<NodeTools::StepSeq*> (node->getProcessor()) : nullptr)
        seq->stop();

    current().removeNode (id);

    if (heldNotes)
        current().panic();

    rebuildNodeTypeCache();
    needsRepaint = true;
    repaint();
}

void ContainerGraphCanvas::toggleMute (Node* node)
{
    if (node != nullptr)
        current().setNodeMuted (node->nodeID, ! node->isBypassed());

    needsRepaint = true;
}

void ContainerGraphCanvas::showTip (const juce::String& text, juce::Point<int> screenPos, bool live)
{
    juce::CallOutBox::launchAsynchronously (std::make_unique<StatusToolTip> (text, live),
                                            { screenPos.x + 10, screenPos.y + 10, 1, 1 }, nullptr);
}

void ContainerGraphCanvas::showPinInfo (const PinID& pin, juce::Point<int> screenPos)
{
    auto* node = getActiveGraph()->getNodeForId (pin.nodeID);

    if (node == nullptr)
        return;

    auto* cache = getCachedNodeType (pin.nodeID);
    juce::String text;

    if (cache != nullptr && cache->container != nullptr)
    {
        text = cache->container->getContainerName()
             + (pin.isMidi ? (pin.isInput ? " MIDI In" : " MIDI Out")
                           : (pin.isInput && pin.pinIndex >= 2 ? " Sidechain Ch " + juce::String (pin.pinIndex - 1)
                                                               : (pin.isInput ? " In" : " Out") + juce::String (" Ch ") + juce::String (pin.pinIndex + 1)));
    }
    else if (pin.isMidi)
    {
        const auto ioName = node->properties["ioNodeName"].toString();
        text = ioName.isNotEmpty() ? ioName : pin.isInput ? "MIDI In" : "MIDI Out";
    }
    else if (cache != nullptr && cache->isAudioInput)
    {
        text = pin.pinIndex >= 2 ? "Rack Sidechain In Ch " + juce::String (pin.pinIndex - 1)
                                 : "Rack Audio In Ch " + juce::String (pin.pinIndex + 1);
    }
    else if (cache != nullptr && cache->isAudioOutput)
    {
        text = "Rack Audio Out Ch " + juce::String (pin.pinIndex + 1);
    }
    else if (cache != nullptr && cache->transientSplitter != nullptr && ! pin.isInput)
    {
        text = juce::String (pin.pinIndex < 2 ? "Transient " : "Sustain ") + (pin.pinIndex % 2 == 0 ? "L" : "R");
    }
    else if (pin.isInput && isSidechainPin (pin, node))
    {
        text = "Sidechain Ch " + juce::String (pin.pinIndex - 1);
    }
    else
    {
        text = (pin.isInput ? (cache != nullptr && cache->plugin != nullptr ? "Main Input" : "Input") : "Output") + juce::String (" Ch ") + juce::String (pin.pinIndex + 1);
    }

    showTip (text, screenPos, ! current().isOff (node));
}

void ContainerGraphCanvas::showWireMenu (const Connection& connection, juce::Point<int> screenPos)
{
    auto* graph = getActiveGraph();
    auto* src = graph->getNodeForId (connection.source.nodeID);
    auto* dst = graph->getNodeForId (connection.destination.nodeID);
    juce::String text = connection.source.isMIDI() ? "MIDI Connection" : "Audio Connection";

    if (dst != nullptr && ! connection.source.isMIDI() && isSidechainPin ({ dst->nodeID, connection.destination.channelIndex, true, false }, dst))
        text = "Sidechain Connection";

    const bool live = src != nullptr && dst != nullptr && ! current().isOff (src) && ! current().isOff (dst);
    juce::Component::SafePointer<ContainerGraphCanvas> safeThis (this);
    auto* container = &current();

    juce::CallOutBox::launchAsynchronously (std::make_unique<StatusToolTip> (text, live, [safeThis, container, connection]
                                            {
                                                if (safeThis == nullptr)
                                                    return;

                                                safeThis->refreshIfStale();

                                                if (&safeThis->current() != container)
                                                    return;

                                                container->disconnect (connection);
                                                safeThis->rebuildNodeTypeCache();
                                                safeThis->repaint();
                                            }),
                                            { screenPos.x + 10, screenPos.y + 10, 1, 1 }, nullptr);
}

void ContainerGraphCanvas::showMidiChannelFilter (Node* node)
{
    auto* cache = node != nullptr ? getCachedNodeType (node->nodeID) : nullptr;

    if (cache == nullptr || cache->plugin == nullptr || ! cache->isInstrument)
        return;

    const auto at = buttonsFor (node).channel.getCentre();
    const auto screen = virtualToScreen (at.x, at.y);
    juce::Component::SafePointer<ContainerGraphCanvas> safeThis (this);
    auto* container = &current();

    juce::CallOutBox::launchAsynchronously (std::make_unique<MidiChannelSelector> (*cache->plugin, [safeThis, container]
                                            {
                                                if (safeThis == nullptr || ! safeThis->holds (container))
                                                    return;

                                                safeThis->needsRepaint = true;
                                                container->changed();
                                            }),
                                            { screen.x, screen.y, 1, 1 }, nullptr);
}

void ContainerGraphCanvas::showTransportOverride (Node* node)
{
    auto* cache = node != nullptr ? getCachedNodeType (node->nodeID) : nullptr;

    if (cache == nullptr || cache->plugin == nullptr)
        return;

    TransportOverridePanel::Target target;
    auto* container = &current();

    if (auto* inner = cache->container)
    {
        target.title = "Container's Transport";
        target.isSynced = [inner] { return inner->isTransportSynced(); };
        target.setSynced = [inner, container] (bool s) { inner->setTransportSynced (s); container->changed(); };
        target.getTempo = [inner] { return inner->getCustomTempo(); };
        target.setTempo = [inner, container] (double t) { inner->setCustomTempo (t); container->changed(); };
        target.getSignature = [inner] { return std::make_pair (inner->getCustomTimeSigNumerator(), inner->getCustomTimeSigDenominator()); };
        target.setSignature = [inner, container] (int n, int d) { inner->setCustomTimeSignature (n, d); container->changed(); };
    }
    else
    {
        auto* pluginNode = cache->plugin;
        target.title = "Plugin's Transport";
        target.isSynced = [pluginNode] { return pluginNode->isTransportSynced(); };
        target.setSynced = [pluginNode, container] (bool s) { pluginNode->setTransportSynced (s); container->changed(); };
        target.getTempo = [pluginNode] { return pluginNode->getCustomTempo(); };
        target.setTempo = [pluginNode, container] (double t) { pluginNode->setCustomTempo (t); container->changed(); };
        target.getSignature = [pluginNode] { return std::make_pair (pluginNode->getCustomTimeSigNumerator(), pluginNode->getCustomTimeSigDenominator()); };
        target.setSignature = [pluginNode, container] (int n, int d) { pluginNode->setCustomTimeSignature (n, d); container->changed(); };
    }

    const auto bounds = getNodeBounds (node);
    const auto screen = virtualToScreen (bounds.getCentreX(), bounds.getY());
    juce::CallOutBox::launchAsynchronously (std::make_unique<TransportOverridePanel> (std::move (target)), { screen.x, screen.y, 1, 1 }, nullptr);
}

void ContainerGraphCanvas::renameContainer()
{
    auto* container = &current();
    juce::Component::SafePointer<ContainerGraphCanvas> safeThis (this);
    auto* alert = new juce::AlertWindow ("Rename Container", "Enter a new name for this container:", juce::MessageBoxIconType::NoIcon);
    alert->addTextEditor ("name", container->getContainerName(), "Name:");
    alert->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    alert->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, alert, container] (int result)
    {
        if (result == 1 && safeThis != nullptr && safeThis->holds (container))
        {
            container->setContainerName (alert->getTextEditorContents ("name"));
            safeThis->refreshIfStale();
            safeThis->rebuildNodeTypeCache();
            safeThis->repaint();

            if (safeThis->onTitleChanged)
                safeThis->onTitleChanged();
        }
    }), true);
}

void ContainerGraphCanvas::showToolEditor (Node* node)
{
    auto* cache = node != nullptr ? getCachedNodeType (node->nodeID) : nullptr;

    if (cache == nullptr)
        return;

    // The editor knows its tool by the rack and the processor: it closes when either goes.
    auto* container = &current();
    auto* proc = node->getProcessor();
    juce::Component::SafePointer<ContainerGraphCanvas> safeThis (this);

    NodeRackPages::ToolLink link;
    link.isAlive = [safeThis, container, proc]
    {
        if (safeThis == nullptr || ! safeThis->holds (container))
            return false;

        for (auto* n : container->getGraph().getNodes())
            if (n->getProcessor() == proc)
                return true;

        return false;
    };
    link.edited = [safeThis, container]
    {
        if (safeThis != nullptr && safeThis->holds (container))
            container->toolEdited();
    };

    std::unique_ptr<juce::Component> editor;

    if (cache->latcher != nullptr)                 editor = NodeRackPages::makeLatcherEditor (*cache->latcher, std::move (link));
    else if (cache->stepSeq != nullptr)            editor = NodeRackPages::makeStepSeqEditor (*cache->stepSeq, std::move (link));
    else if (cache->transientSplitter != nullptr)  editor = NodeRackPages::makeTransientSplitterEditor (*cache->transientSplitter, std::move (link));

    if (editor == nullptr)
        return;

    const auto bounds = getNodeBounds (node);
    const auto at = virtualToScreen (bounds.getCentreX(), bounds.getCentreY());
    juce::CallOutBox::launchAsynchronously (std::move (editor), { at.x, at.y, 1, 1 }, nullptr);
}

void ContainerGraphCanvas::resetRack()
{
    auto* container = &current();
    juce::Component::SafePointer<ContainerGraphCanvas> safeThis (this);
    const auto options = juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::QuestionIcon, "Reset",
                                                                       "Empty \"" + container->getContainerName() + "\"? Every node and cable in it goes.",
                                                                       "Reset", "Cancel", this);

    juce::AlertWindow::showAsync (options, [safeThis, container] (int result)
    {
        if (result != 1 || safeThis == nullptr || ! safeThis->holds (container))
            return;

        safeThis->refreshIfStale();

        while (safeThis->isNested() && &safeThis->current() != container)
            safeThis->diveOut();

        container->reset();
        safeThis->rebuildNodeTypeCache();
        safeThis->repaint();

        if (safeThis->onTitleChanged)
            safeThis->onTitleChanged();
    });
}

void ContainerGraphCanvas::panic()
{
    // The whole rack, whichever Container is shown.
    root.panic();
    needsRepaint = true;
    repaint();
}

void ContainerGraphCanvas::savePreset()
{
    auto* container = &current();
    auto folder = Containers::defaultFolder();
    folder.createDirectory();
    chooser = std::make_shared<juce::FileChooser> ("Save Patch",
                                                   folder.getChildFile (juce::File::createLegalFileName (container->getContainerName()) + ".container"),
                                                   "*.container");
    juce::Component::SafePointer<ContainerGraphCanvas> safeThis (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safeThis, container] (const juce::FileChooser& fc)
    {
        if (safeThis == nullptr || ! safeThis->holds (container))   // gone while the chooser was open
            return;

        auto file = fc.getResult();

        if (file == juce::File())
            return;

        if (! file.hasFileExtension ("container"))
            file = file.withFileExtension ("container");

        if (! container->savePreset (file))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save Patch", "The patch couldn't be saved to " + file.getFullPathName() + ".");
    });
}

void ContainerGraphCanvas::loadPreset()
{
    auto* container = &current();
    chooser = std::make_shared<juce::FileChooser> ("Load Patch", Containers::defaultFolder(), "*.container");
    juce::Component::SafePointer<ContainerGraphCanvas> safeThis (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safeThis, container] (const juce::FileChooser& fc)
    {
        if (safeThis == nullptr || ! fc.getResult().existsAsFile() || ! safeThis->holds (container))   // gone while the chooser was open
            return;

        // What's shown is replaced: anything entered below it goes first.
        safeThis->refreshIfStale();

        while (safeThis->isNested() && &safeThis->current() != container)
            safeThis->diveOut();

        if (! container->loadPreset (fc.getResult()))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Load Patch", "That file isn't a Node Rack patch.");
            return;
        }

        if (! container->getProblems().isEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Load Patch",
                                                    "Not everything in it could be brought back (what isn't here is kept as it was, for when it is):\n\n" + container->getProblems().joinIntoString ("\n"));

        safeThis->rebuildNodeTypeCache();
        safeThis->repaint();

        if (safeThis->onTitleChanged)
            safeThis->onTitleChanged();
    });
}

void ContainerGraphCanvas::addPluginFile (Containers::Spot at)
{
    // The folder last browsed; the first time, the VST3 folder.
    auto* host = Containers::getHost();
    auto folder = host != nullptr ? host->pluginBrowseFolder() : juce::File();

    if (! folder.isDirectory())
    {
       #if JUCE_WINDOWS
        folder = juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory).getChildFile ("Common Files").getChildFile ("VST3");
       #elif JUCE_MAC
        folder = juce::File ("/Library/Audio/Plug-Ins/VST3");
       #else
        folder = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".vst3");
       #endif
    }

    chooser = std::make_shared<juce::FileChooser> ("Browse for a Plugin", folder, "*.vst3;*.clap;*.sip;*.sonifyplugin");
    juce::Component::SafePointer<ContainerGraphCanvas> safeThis (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectDirectories,
                          [safeThis, at] (const juce::FileChooser& fc)
    {
        const auto result = fc.getResult();

        if (safeThis == nullptr || ! result.exists())
            return;

        // A VST3 bundle is a folder: the folder it's in is the one to come back to.
        if (auto* h = Containers::getHost())
            h->setPluginBrowseFolder (result.getParentDirectory());

        Plugin::approve (result);   // the user's own pick: it may load, wherever it is
        safeThis->addFromTools ("PLUGIN:" + result.getFullPathName() + "|", at);
    });
}

void ContainerGraphCanvas::addFromTools (const juce::String& dragId, Containers::Spot at)
{
    refreshIfStale();
    auto& container = current();
    juce::String error;

    if (dragId.startsWith ("TOOL:"))
    {
        container.addTool (dragId.fromFirstOccurrenceOf ("TOOL:", false, false), at);
    }
    else if (dragId == "FILE:")
    {
        addPluginFile (at);
        return;
    }
    else if (dragId.startsWith ("PLUGIN:"))
    {
        const auto rest = dragId.fromFirstOccurrenceOf ("PLUGIN:", false, false);
        const auto path = rest.upToLastOccurrenceOf ("|", false, false);
        const auto pluginId = rest.fromLastOccurrenceOf ("|", false, false);
        const auto file = juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();
        auto node = container.addPlugin (file, pluginId, at, error);

        if (node == nullptr)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Add Plugin", error);
        else if (auto* pluginNode = dynamic_cast<PluginNode*> (node->getProcessor()))
            if (auto* inner = pluginNode->asContainer())
                inner->setContainerName (ContainerPlugin::defaultName (inner->isEffect(), true));   // a rack inside: a Container
    }
    else if (dragId.startsWith ("CONTAINERPRESET:"))
    {
        // A saved container comes in as a container node of the kind it was saved as.
        const juce::File file (dragId.fromFirstOccurrenceOf ("CONTAINERPRESET:", false, false));
        const auto text = file.loadFileAsString();
        const auto xml = juce::parseXML (text);
        const bool effect = xml != nullptr && xml->getStringAttribute ("kind", "instrument") == "effect";
        auto inner = std::make_unique<ContainerPlugin> (effect);

        if (! inner->fromXml (text))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Add Patch", "That file isn't a Node Rack patch.");
            return;
        }

        const auto problems = inner->getProblems();
        container.addPluginNode (std::move (inner), at);

        if (! problems.isEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Add Patch",
                                                    "Not everything in it could be brought back (what isn't here is kept as it was, for when it is):\n\n" + problems.joinIntoString ("\n"));
    }

    rebuildNodeTypeCache();
    needsRepaint = true;
    repaint();
}

void ContainerGraphCanvas::positionsMoved()
{
    // Where nodes sit is kept with the song.
    current().changed();
}

//==============================================================================
// Menus
void ContainerGraphCanvas::showPluginMenu (juce::Point<float> virtualPos)
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Add Node");
    menu.addSeparator();

    juce::PopupMenu tools;
    tools.addItem (1, "Connector");
    tools.addItem (2, "Container");
    tools.addItem (3, "FX Container");
    tools.addItem (9, "Latcher");
    tools.addItem (10, "Step Seq");
    tools.addItem (11, "Transient Splitter");
    tools.addSeparator();
    tools.addItem (4, "MIDI Monitor");
    tools.addItem (6, "Stereo Meter");
    tools.addItem (5, "Playlist Player");
    tools.addItem (8, "Sonify Sampler");
    tools.addItem (12, "Tomahawk");
    menu.addSubMenu ("System Tools", tools);
    menu.addItem (7, "Browse for a Plugin...");
    menu.addSeparator();

    // The plugins Sonify has, by kind and vendor.
    std::vector<juce::String> ids;
    auto* host = Containers::getHost();
    auto choices = host != nullptr ? host->listPlugins() : std::vector<Containers::Choice> {};
    std::sort (choices.begin(), choices.end(), [] (const auto& a, const auto& b)
    {
        return a.vendor != b.vendor ? a.vendor.compareNatural (b.vendor) < 0 : a.name.compareNatural (b.name) < 0;
    });

    for (const auto* kind : { "instrument", "effect", "noteEffect" })
    {
        std::map<juce::String, juce::PopupMenu> byVendor;

        for (const auto& choice : choices)
        {
            if (choice.kind != kind || Containers::isContainerId (choice.id))
                continue;

            ids.push_back ("PLUGIN:" + choice.file + "|" + choice.id);
            byVendor[choice.vendor.isNotEmpty() ? choice.vendor : juce::String ("Other")].addItem (100 + static_cast<int> (ids.size()) - 1, choice.name);
        }

        if (byVendor.empty())
            continue;

        juce::PopupMenu sub;

        for (auto& [vendor, items] : byVendor)
            sub.addSubMenu (vendor, items);

        menu.addSubMenu (juce::String (kind) == "instrument" ? "Instruments" : juce::String (kind) == "effect" ? "Effects" : "Note FX", sub);
    }

    // Saved containers.
    juce::PopupMenu saved;

    for (const auto& file : Containers::defaultFolder().findChildFiles (juce::File::findFiles, false, "*.container"))
    {
        ids.push_back ("CONTAINERPRESET:" + file.getFullPathName());
        saved.addItem (100 + static_cast<int> (ids.size()) - 1, file.getFileNameWithoutExtension());
    }

    if (saved.getNumItems() > 0)
        menu.addSubMenu ("Saved Patches", saved);

    const Containers::Spot at { virtualPos.x, virtualPos.y };
    juce::Component::SafePointer<ContainerGraphCanvas> safeThis (this);

    menu.showMenuAsync (juce::PopupMenu::Options(), [safeThis, ids, at] (int result)
    {
        if (safeThis == nullptr || result == 0)
            return;

        switch (result)
        {
            case 1: safeThis->addFromTools ("TOOL:Connector", at); break;
            case 2: safeThis->addFromTools ("PLUGIN:|" + juce::String (Containers::instrumentId), at); break;
            case 3: safeThis->addFromTools ("PLUGIN:|" + juce::String (Containers::effectId), at); break;
            case 4: safeThis->addFromTools ("TOOL:MidiMonitor", at); break;
            case 5: safeThis->addFromTools ("PLUGIN:|sonify.builtin.playlist", at); break;
            case 6: safeThis->addFromTools ("TOOL:StereoMeter", at); break;
            case 7: safeThis->addFromTools ("FILE:", at); break;
            case 8: safeThis->addFromTools ("PLUGIN:|sonify.builtin.sampler", at); break;
            case 9: safeThis->addFromTools ("TOOL:Latcher", at); break;
            case 10: safeThis->addFromTools ("TOOL:StepSeq", at); break;
            case 11: safeThis->addFromTools ("TOOL:TransientSplitter", at); break;
            case 12: safeThis->addFromTools ("PLUGIN:|sonify.builtin.tomahawk", at); break;
            default:
                if (result >= 100 && result - 100 < static_cast<int> (ids.size()))
                    safeThis->addFromTools (ids[static_cast<size_t> (result - 100)], at);
                break;
        }
    });
}

void ContainerGraphCanvas::showNodeTitleMenu (Node* node)
{
    juce::PopupMenu menu;
    menu.addItem (1, "Disconnect All Wires");
    menu.addSeparator();
    menu.addItem (2, "Delete");

    const auto id = node->nodeID;
    auto* container = &current();
    juce::Component::SafePointer<ContainerGraphCanvas> safeThis (this);

    menu.showMenuAsync (juce::PopupMenu::Options(), [safeThis, id, container] (int result)
    {
        if (safeThis == nullptr)
            return;

        safeThis->refreshIfStale();

        if (&safeThis->current() != container)
            return;

        if (result == 1)
            container->disconnectNode (id);
        else if (result == 2)
            safeThis->deleteNode (id);

        safeThis->rebuildNodeTypeCache();
        safeThis->repaint();
    });
}

//==============================================================================
// The header
bool ContainerGraphCanvas::headerClick (const juce::MouseEvent& e)
{
    if (e.position.y >= headerHeight)
        return false;

    if (! e.mods.isLeftButtonDown())
        return true;

    const auto h = headerLayout();

    if (h.back.contains (e.position))           diveOut();
    else if (h.zoomMinus.contains (e.position)) setZoomLevel (zoomLevel - 0.1f);
    else if (h.zoomPct.contains (e.position))   setZoomLevel (1.0f);
    else if (h.zoomPlus.contains (e.position))  setZoomLevel (zoomLevel + 0.1f);
    else if (h.name.contains (e.position))      renameContainer();

    return true;   // anything else in the header does nothing
}

//==============================================================================
// Mouse
void ContainerGraphCanvas::mouseDown (const juce::MouseEvent& e)
{
    refreshIfStale();

    if (headerClick (e))
        return;

    if (e.mods.isLeftButtonDown())
    {
        if (getMinimapRect().contains (e.position))
        {
            isDraggingMinimap = true;
            navigateMinimapTo (e.position);
            return;
        }

        if (getHScrollbarRect().contains (e.position))
        {
            isDraggingHScrollbar = true;
            scrollbarDragStartOffset = e.position.x;
            scrollbarDragStartPan = panOffsetX;
            return;
        }

        if (getVScrollbarRect().contains (e.position))
        {
            isDraggingVScrollbar = true;
            scrollbarDragStartOffset = e.position.y;
            scrollbarDragStartPan = panOffsetY;
            return;
        }
    }

    const auto pos = toVirtual (e.position);
    const auto screenClick = e.getScreenPosition();
    auto* graph = getActiveGraph();

    // Pins: right-click says what it is; dragging starts a cable. A connected input pin picks
    // its (last) cable up again, to move it elsewhere; outputs fan out, so they always start a new one.
    const auto pin = findPinAt (pos);

    if (pin.isValid())
    {
        if (e.mods.isRightButtonDown())
        {
            showPinInfo (pin, screenClick);
            return;
        }

        auto source = pin;

        if (pin.isInput)
        {
            const Connection* last = nullptr;

            for (const auto& connection : cachedConnections)
                if (connection.destination.nodeID == pin.nodeID
                    && connection.destination.isMIDI() == pin.isMidi
                    && (pin.isMidi || connection.destination.channelIndex == pin.pinIndex))
                    last = &connection;

            if (last != nullptr)
            {
                const auto taken = *last;
                current().disconnect (taken);
                source = { taken.source.nodeID, pin.isMidi ? 0 : taken.source.channelIndex, false, pin.isMidi };
                rebuildNodeTypeCache();
            }
        }

        dragCable.active = true;
        dragCable.sourcePin = source;
        dragCable.currentDragPos = pos;
        dragCable.dragColour = getPinColour (source, graph->getNodeForId (source.nodeID));
        startTimer (mouseTimer, 16);
        return;
    }

    const auto connection = getConnectionAt (pos);

    if (connection.source.nodeID.uid != 0)
    {
        if (e.mods.isRightButtonDown())
        {
            showWireMenu (connection, screenClick);
            return;
        }

        if (e.getNumberOfClicks() == 2)
        {
            current().disconnect (connection);
            rebuildNodeTypeCache();
            needsRepaint = true;
            return;
        }
    }

    if (auto* node = findNodeAt (pos))
    {
        auto bounds = getNodeBounds (node);
        auto* cache = getCachedNodeType (node->nodeID);

        if (cache == nullptr)
            return;

        // The title bar's right-click menu (every node but the I/O).
        if (e.mods.isRightButtonDown() && bounds.withHeight (Style::nodeTitleHeight).contains (pos) && ! cache->isIO)
        {
            showNodeTitleMenu (node);
            return;
        }

        const auto b = buttonsFor (node);
        const auto hit = [&pos] (juce::Rectangle<float> r) { return ! r.isEmpty() && r.contains (pos); };

        // Right-click on a button: what it does (Colosseum: tooltips only, no action).
        if (e.mods.isRightButtonDown())
        {
            juce::String tip;

            if (cache->container != nullptr)
            {
                if (hit (b.edit))           tip = "E - Dive into container (open internal graph)";
                else if (hit (b.mute))      tip = "M - Mute/Unmute container audio output";
                else if (hit (b.transport)) tip = "T - Per-container transport override";
                else if (hit (b.remove))    tip = "X - Delete container";
            }
            else if (cache->plugin != nullptr)
            {
                if (hit (b.edit))           tip = "E - Open plugin editor window";
                else if (hit (b.channel))   tip = "CH - MIDI channel filter (select which channels this instrument receives)";
                else if (hit (b.mute))      tip = "M - Mute/Bypass plugin";
                else if (hit (b.pass))      tip = "P - Pass-through mode (bypass processing, pass audio through)";
                else if (hit (b.transport)) tip = "T - Per-plugin transport override (custom tempo/time signature)";
                else if (hit (b.remove))    tip = "X - Delete plugin from the container";
            }
            else if (cache->connector != nullptr)
            {
                if (hit (b.mute))           tip = "M - Mute/Unmute audio output";
                else if (hit (b.remove))    tip = "X - Delete this connector module";
                else if (hit (b.volume))    tip = "Volume - Drag up/down to adjust gain (-inf to +25dB)";
            }
            else if (cache->isAudioInput || cache->isAudioOutput)
            {
                if (hit (b.toggle))         tip = cache->isAudioInput ? "ON/OFF - Enable/Disable the track's sound into the container"
                                                                      : "ON/OFF - Enable/Disable the container's sound out";
            }
            else if (cache->latcher != nullptr)
            {
                if (hit (b.allOff))         tip = "All Off - Release all latched pads";
                else if (hit (b.pads))      tip = "Click pads to toggle latch on/off";
                else if (hit (b.edit))      tip = "E - Open Latcher pad editor";
                else if (hit (b.remove))    tip = "X - Delete this Latcher module";
            }
            else if (cache->stepSeq != nullptr)
            {
                if (hit (b.play))           tip = "Play/Stop - Toggle sequencer playback";
                else if (hit (b.bpm))       tip = cache->stepSeq->isSyncedToSongTempo() ? "BPM - The song's tempo (turn SYN off to set its own)"
                                                                                        : "BPM - Drag up/down to set its own tempo";
                else if (hit (b.sync))      tip = "SYN - Follow the song's tempo";
                else if (hit (b.edit))      tip = "E - Open CC Step Sequencer editor";
                else if (hit (b.remove))    tip = "X - Delete this Step Sequencer module";
            }
            else if (cache->transientSplitter != nullptr)
            {
                if (hit (b.edit))           tip = "E - Open Transient Splitter editor";
                else if (hit (b.remove))    tip = "X - Delete this Transient Splitter";
            }
            else if (hit (b.remove))
            {
                tip = cache->missing != nullptr ? "X - Delete the missing plugin" : "X - Delete this module";
            }

            if (cache->missing != nullptr && tip.isEmpty())
                tip = "Not found: " + cache->missing->getSaved().getStringAttribute ("file", cache->pluginName)
                    + (cache->missing->getWhy().isNotEmpty() ? " (" + cache->missing->getWhy() + ")" : juce::String());

            if (cache->plugin != nullptr && tip.isEmpty() && cache->plugin->getFailure().isNotEmpty())
                tip = cache->plugin->getFailure();

            if (tip.isNotEmpty())
                showTip (tip, screenClick);

            return;
        }

        // Left-click: the buttons.
        if (cache->container != nullptr)
        {
            if (hit (b.edit))      { diveInto (*cache->container); return; }
            if (hit (b.mute))      { cache->container->setMuted (! cache->container->isMuted()); current().changed(); needsRepaint = true; return; }
            if (hit (b.transport)) { showTransportOverride (node); return; }
            if (hit (b.remove))    { deleteNode (node->nodeID); return; }
        }
        else if (cache->plugin != nullptr)
        {
            if (hit (b.edit))      { openPluginWindow (node); return; }
            if (hit (b.channel))   { showMidiChannelFilter (node); return; }
            if (hit (b.mute))      { toggleMute (node); return; }
            if (hit (b.pass))      { cache->plugin->setPassThrough (! cache->plugin->isPassThrough()); current().changed(); needsRepaint = true; return; }
            if (hit (b.transport)) { showTransportOverride (node); return; }
            if (hit (b.remove))    { deleteNode (node->nodeID); return; }
        }
        else if (cache->connector != nullptr)
        {
            if (hit (b.mute))      { cache->connector->toggleMute(); current().changed(); needsRepaint = true; return; }
            if (hit (b.remove))    { deleteNode (node->nodeID); return; }

            if (hit (b.volume))
            {
                draggingVolumeNodeID = node->nodeID;
                volumeDragStartY = e.position.y;
                volumeDragStartValue = cache->connector->getVolume();
                startTimer (mouseTimer, 16);
                return;
            }
        }
        else if (cache->latcher != nullptr)
        {
            if (hit (b.allOff))    { cache->latcher->allNotesOff(); needsRepaint = true; repaint(); return; }
            if (hit (b.edit))      { showToolEditor (node); return; }
            if (hit (b.remove))    { deleteNode (node->nodeID); return; }

            if (hit (b.pads))
            {
                const auto col = juce::jlimit (0, 3, static_cast<int> ((pos.x - b.pads.getX()) / (b.pads.getWidth() / 4.0f)));
                const auto row = juce::jlimit (0, 3, static_cast<int> ((pos.y - b.pads.getY()) / (b.pads.getHeight() / 4.0f)));
                cache->latcher->togglePadFromUI (row * 4 + col);
                needsRepaint = true;
                repaint();
                return;
            }
        }
        else if (cache->stepSeq != nullptr)
        {
            auto* seq = cache->stepSeq;

            if (hit (b.edit))      { showToolEditor (node); return; }
            if (hit (b.remove))    { deleteNode (node->nodeID); return; }
            if (hit (b.play))      { seq->togglePlayStop(); current().toolEdited(); needsRepaint = true; repaint(); return; }
            if (hit (b.sync))      { seq->setSyncToSongTempo (! seq->isSyncedToSongTempo()); current().toolEdited(); needsRepaint = true; repaint(); return; }

            if (hit (b.bpm) && ! seq->isSyncedToSongTempo())
            {
                draggingBpmNodeID = node->nodeID;
                bpmDragStartY = e.position.y;
                bpmDragStartValue = seq->getBpm();
                startTimer (mouseTimer, 16);
                return;
            }
        }
        else if (cache->transientSplitter != nullptr)
        {
            if (hit (b.edit))      { showToolEditor (node); return; }
            if (hit (b.remove))    { deleteNode (node->nodeID); return; }
        }
        else if (cache->isAudioInput || cache->isAudioOutput)
        {
            if (hit (b.toggle))
            {
                // Audio In by its own switch (its buffers would go stale bypassed); Audio Out by the graph's.
                if (cache->isAudioInput)
                {
                    current().setAudioInputOn (! current().isAudioInputOn());
                }
                else
                {
                    node->setBypassed (! node->isBypassed());
                    current().changed();
                }

                needsRepaint = true;
                return;
            }
        }
        else if (hit (b.remove))
        {
            deleteNode (node->nodeID);
            return;
        }

        // Anywhere else on a node: drag it.
        if (! e.mods.isRightButtonDown())
        {
            draggingNodeID = node->nodeID;
            nodeMoved = false;
            nodeDragOffset = pos - juce::Point<float> (static_cast<float> (static_cast<double> (node->properties["x"])),
                                                       static_cast<float> (static_cast<double> (node->properties["y"])));
            startTimer (mouseTimer, 16);
        }

        return;
    }

    if (e.mods.isRightButtonDown())
    {
        showPluginMenu (pos);   // the empty canvas: add a node where the mouse is
        return;
    }

    if (e.mods.isLeftButtonDown())
    {
        // The empty canvas: drag to pan.
        isPanning = true;
        panMouseScreenStart = e.getScreenPosition();
        panStartOffsetX = panOffsetX;
        panStartOffsetY = panOffsetY;
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }
}

void ContainerGraphCanvas::mouseMove (const juce::MouseEvent& e)
{
    refreshIfStale();
    const auto pos = toVirtual (e.position);
    highlightPin = findPinAt (pos);
    hoveredConnection = getConnectionAt (pos);
}

void ContainerGraphCanvas::applyMagneticSnap (Node* dragged, float& x, float& y)
{
    if (dragged == nullptr)
        return;

    const auto draggedBounds = getNodeBounds (dragged);
    const auto dragW = draggedBounds.getWidth(), dragH = draggedBounds.getHeight();
    auto bestX = magneticSnapThreshold + 1.0f, bestY = magneticSnapThreshold + 1.0f;
    auto snapX = x, snapY = y;
    bool snappedX = false, snappedY = false;
    auto* graph = getActiveGraph();

    const auto tryX = [&] (float distance, float value) { if (distance < bestX && distance < magneticSnapThreshold) { bestX = distance; snapX = value; snappedX = true; } };
    const auto tryY = [&] (float distance, float value) { if (distance < bestY && distance < magneticSnapThreshold) { bestY = distance; snapY = value; snappedY = true; } };

    for (const auto& connection : cachedConnections)
    {
        NodeID other;

        if (connection.source.nodeID == dragged->nodeID)            other = connection.destination.nodeID;
        else if (connection.destination.nodeID == dragged->nodeID)  other = connection.source.nodeID;
        else continue;

        auto* otherNode = graph->getNodeForId (other);

        if (otherNode == nullptr)
            continue;

        const auto o = getNodeBounds (otherNode);
        tryY (std::abs (y - o.getY()), o.getY());                                                       // top to top
        tryY (std::abs (y + dragH * 0.5f - o.getCentreY()), o.getCentreY() - dragH * 0.5f);            // centre to centre
        tryX (std::abs (x + dragW - o.getX()), o.getX() - dragW);                                        // snug on its left
        tryX (std::abs (x + dragW + 10.0f - o.getX()), o.getX() - dragW - 10.0f);                        // 10 px to its left
        tryX (std::abs (x - o.getRight()), o.getRight());                                                // snug on its right
        tryX (std::abs (x - (o.getRight() + 10.0f)), o.getRight() + 10.0f);                              // 10 px to its right
        tryX (std::abs (x - o.getX()), o.getX());                                                        // stacked, left edges in line
    }

    if (snappedX) x = snapX;
    if (snappedY) y = snapY;
}

void ContainerGraphCanvas::mouseDrag (const juce::MouseEvent& e)
{
    refreshIfStale();

    if (isDraggingMinimap)
    {
        navigateMinimapTo (e.position);
        return;
    }

    if (isDraggingHScrollbar || isDraggingVScrollbar)
    {
        const bool horizontal = isDraggingHScrollbar;
        const auto visible = horizontal ? getVisibleWidth() : getVisibleHeight();
        const auto total = horizontal ? virtualCanvasWidth : virtualCanvasHeight;
        const auto maxPan = total - visible;

        if (maxPan > 0.0f)
        {
            const auto bar = horizontal ? getHScrollbarRect() : getVScrollbarRect();
            const auto length = horizontal ? bar.getWidth() : bar.getHeight();
            const auto thumb = juce::jmax (20.0f, length * visible / total);
            const auto delta = (horizontal ? e.position.x : e.position.y) - scrollbarDragStartOffset;
            const auto pan = juce::jlimit (0.0f, maxPan, scrollbarDragStartPan + delta / (length - thumb) * maxPan);
            (horizontal ? panOffsetX : panOffsetY) = pan;
            repaint();
        }

        return;
    }

    if (isPanning)
    {
        const auto delta = e.getScreenPosition() - panMouseScreenStart;
        panOffsetX = panStartOffsetX - static_cast<float> (delta.x) / zoomLevel;
        panOffsetY = panStartOffsetY - static_cast<float> (delta.y) / zoomLevel;
        clampPanOffset();
        repaint();
        return;
    }

    if (draggingBpmNodeID.uid != 0)
    {
        if (auto* cache = getCachedNodeType (draggingBpmNodeID); cache != nullptr && cache->stepSeq != nullptr)
            cache->stepSeq->setBpm (bpmDragStartValue + static_cast<double> (bpmDragStartY - e.position.y) * 0.5);

        needsRepaint = true;
        return;
    }

    if (draggingVolumeNodeID.uid != 0)
    {
        if (auto* cache = getCachedNodeType (draggingVolumeNodeID); cache != nullptr && cache->connector != nullptr)
            cache->connector->setVolume (volumeDragStartValue + (volumeDragStartY - e.position.y) * 0.005f);

        needsRepaint = true;
        return;
    }

    if (dragCable.active)
    {
        dragCable.currentDragPos = toVirtual (e.position);
        highlightPin = findPinAt (dragCable.currentDragPos);
        needsRepaint = true;
        return;
    }

    if (draggingNodeID.uid != 0)
    {
        if (auto* node = getActiveGraph()->getNodeForId (draggingNodeID))
        {
            const auto p = toVirtual (e.position) - nodeDragOffset;
            auto x = juce::jlimit (10.0f, virtualCanvasWidth - Style::nodeWidth - 10.0f, p.x);
            auto y = juce::jlimit (10.0f, virtualCanvasHeight - Style::nodeHeight - 10.0f, p.y);

            if (! e.mods.isShiftDown())   // hold Shift to place it freely
                applyMagneticSnap (node, x, y);

            node->properties.set ("x", static_cast<double> (x));
            node->properties.set ("y", static_cast<double> (y));
            nodeMoved = true;
            needsRepaint = true;
        }
    }
}

void ContainerGraphCanvas::mouseUp (const juce::MouseEvent& e)
{
    refreshIfStale();

    if (isDraggingMinimap)                        { isDraggingMinimap = false; return; }
    if (isDraggingHScrollbar)                     { isDraggingHScrollbar = false; repaint(); return; }
    if (isDraggingVScrollbar)                     { isDraggingVScrollbar = false; repaint(); return; }

    if (isPanning)
    {
        isPanning = false;
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    if (draggingVolumeNodeID.uid != 0)
    {
        draggingVolumeNodeID = {};
        current().changed();
    }

    if (draggingBpmNodeID.uid != 0)
    {
        draggingBpmNodeID = {};
        current().toolEdited();
    }

    if (dragCable.active)
    {
        const auto hovered = findPinAt (toVirtual (e.position));

        if (hovered.isValid() && canConnect (dragCable.sourcePin, hovered))
            createConnection (dragCable.sourcePin, hovered);

        dragCable.active = false;
        needsRepaint = true;
    }

    if (draggingNodeID.uid != 0 && nodeMoved)
        positionsMoved();

    draggingNodeID = {};
    nodeMoved = false;
    stopTimer (mouseTimer);
    repaint();
}

void ContainerGraphCanvas::mouseDoubleClick (const juce::MouseEvent& e)
{
    refreshIfStale();

    if (e.position.y < headerHeight || dragCable.active || draggingNodeID.uid != 0)
        return;

    const auto pos = toVirtual (e.position);

    if (findPinAt (pos).isValid() || getConnectionAt (pos).source.nodeID.uid != 0)
        return;

    if (auto* node = findNodeAt (pos))
    {
        auto* cache = getCachedNodeType (node->nodeID);

        // A container is entered; the Latcher, Step Seq and Transient Splitter open their editors;
        // the I/O and the other tools have no window of their own.
        if (cache != nullptr && cache->plugin != nullptr)
            openPluginWindow (node);
        else if (cache != nullptr && (cache->latcher != nullptr || cache->stepSeq != nullptr || cache->transientSplitter != nullptr))
            showToolEditor (node);
    }
}

void ContainerGraphCanvas::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCtrlDown())
    {
        // Zoom about the mouse: the point under it stays put.
        const auto newZoom = juce::jlimit (0.10f, 2.0f, zoomLevel + wheel.deltaY * 0.15f);

        if (std::abs (newZoom - zoomLevel) > 0.001f)
        {
            const auto under = toVirtual (e.position);
            zoomLevel = newZoom;
            panOffsetX = under.x - e.position.x / zoomLevel;
            panOffsetY = under.y - e.position.y / zoomLevel;
            clampPanOffset();
            repaint();
        }

        return;
    }

    constexpr float scrollSpeed = 120.0f;

    if (e.mods.isShiftDown())
    {
        panOffsetX -= wheel.deltaY * scrollSpeed;
    }
    else
    {
        panOffsetY -= wheel.deltaY * scrollSpeed;
        panOffsetX -= wheel.deltaX * scrollSpeed;
    }

    clampPanOffset();
    repaint();
}

void ContainerGraphCanvas::navigateMinimapTo (juce::Point<float> local)
{
    const auto mm = getMinimapRect();
    const auto rx = juce::jlimit (0.0f, 1.0f, (local.x - mm.getX()) / mm.getWidth());
    const auto ry = juce::jlimit (0.0f, 1.0f, (local.y - mm.getY()) / mm.getHeight());
    panOffsetX = rx * virtualCanvasWidth - getVisibleWidth() * 0.5f;
    panOffsetY = ry * virtualCanvasHeight - getVisibleHeight() * 0.5f;
    clampPanOffset();
    repaint();
}

//==============================================================================
// Dropped from the Tools panel
bool ContainerGraphCanvas::isInterestedInDragSource (const SourceDetails& details)
{
    const auto id = details.description.toString();
    return id.startsWith ("TOOL:") || id.startsWith ("PLUGIN:") || id.startsWith ("CONTAINERPRESET:") || id == "FILE:";
}

void ContainerGraphCanvas::itemDragEnter (const SourceDetails& details)
{
    isDragHovering = true;
    dragHoverPosition = details.localPosition;
    repaint();
}

void ContainerGraphCanvas::itemDragMove (const SourceDetails& details)
{
    dragHoverPosition = details.localPosition;
    repaint();
}

void ContainerGraphCanvas::itemDragExit (const SourceDetails&)
{
    isDragHovering = false;
    repaint();
}

void ContainerGraphCanvas::itemDropped (const SourceDetails& details)
{
    isDragHovering = false;
    const auto at = toVirtual (details.localPosition.toFloat());
    addFromTools (details.description.toString(), { at.x, at.y });
}

void ContainerGraphCanvas::dropAt (const juce::String& dragId, int x, int y)
{
    isDragHovering = false;
    const auto at = toVirtual ({ static_cast<float> (x), static_cast<float> (y) });

    // The window comes forward to show what was dropped.
    if (auto* top = getTopLevelComponent())
        top->toFront (true);

    addFromTools (dragId, { at.x, at.y });
}

//==============================================================================
// Plugin files dragged in from the computer's folders
namespace
{
    bool isPluginFile (const juce::String& path)
    {
        const juce::File file (path);
        return file.hasFileExtension ("vst3") || file.hasFileExtension ("clap") || file.hasFileExtension ("sip") || file.hasFileExtension ("sonifyplugin");
    }
}

bool ContainerGraphCanvas::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& path : files)
        if (isPluginFile (path))
            return true;

    return false;
}

void ContainerGraphCanvas::fileDragEnter (const juce::StringArray&, int x, int y)
{
    isDragHovering = true;
    dragHoverPosition = { x, y };
    repaint();
}

void ContainerGraphCanvas::fileDragMove (const juce::StringArray&, int x, int y)
{
    dragHoverPosition = { x, y };
    repaint();
}

void ContainerGraphCanvas::fileDragExit (const juce::StringArray&)
{
    isDragHovering = false;
    repaint();
}

void ContainerGraphCanvas::filesDropped (const juce::StringArray& files, int x, int y)
{
    // Each plugin file a step lower than the one before.
    int placed = 0;

    for (const auto& path : files)
    {
        if (isPluginFile (path))
        {
            Plugin::approve (juce::File (path));   // dropped by the user: it may load, wherever it is
            dropAt ("PLUGIN:" + path + "|", x, y + 80 * placed++);
        }
    }
}

//==============================================================================
// A plugin dragged in from Sonify's Browser: "sonify-plugin|<id>|<file>"
namespace
{
    bool parseBrowserDrag (const juce::String& text, juce::String& pluginId, juce::String& path)
    {
        const auto trimmed = text.trim();

        if (! trimmed.startsWith (ContainerGraphCanvas::browserDragPrefix))
            return false;

        const auto rest = trimmed.fromFirstOccurrenceOf (ContainerGraphCanvas::browserDragPrefix, false, false);
        pluginId = rest.upToFirstOccurrenceOf ("|", false, false);
        path = rest.fromFirstOccurrenceOf ("|", false, false);
        return pluginId.isNotEmpty();
    }
}

bool ContainerGraphCanvas::isInterestedInTextDrag (const juce::String& text)
{
    juce::String pluginId, path;
    return parseBrowserDrag (text, pluginId, path);
}

void ContainerGraphCanvas::textDragEnter (const juce::String&, int x, int y)
{
    isDragHovering = true;
    dragHoverPosition = { x, y };
    repaint();
}

void ContainerGraphCanvas::textDragMove (const juce::String&, int x, int y)
{
    dragHoverPosition = { x, y };
    repaint();
}

void ContainerGraphCanvas::textDragExit (const juce::String&)
{
    isDragHovering = false;
    repaint();
}

void ContainerGraphCanvas::textDropped (const juce::String& text, int x, int y)
{
    juce::String pluginId, path;

    if (! parseBrowserDrag (text, pluginId, path))
    {
        isDragHovering = false;
        repaint();
        return;
    }

    // Sonify's own plugins (the built-in ones) have no file of their own.
    dropAt ("PLUGIN:" + (juce::File::isAbsolutePath (path) ? path : juce::String()) + "|" + pluginId, x, y);
}
