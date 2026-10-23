// D:\workspace\Fanan_web_hub\src\MainContainers.cpp
//
// Containers (Container.h) and Sonify: what a container asks of it (plugins from the
// Browser's library, a node's screen opened the way Sonify opens any plugin's, ids the page
// can name a node's plugin by), and the page's view of what's inside one. And the Playlist
// Player's window and folders (PlaylistPlayer.h).

#include "MainComponent.h"
#include "BuiltinPlugins.h"
#include "ContainerCanvas.h"
#include "PlaylistScreen.h"
#include "BuiltinScreen.h"

//==============================================================================
void MainComponent::setupContainers()
{
    Containers::setHost (this);
    ContainerCanvas::install();   // containers open Colosseum's node canvas
    BuiltinScreen::install();     // the built-in effects and the Test Synth open their panels

    // The Playlist Player opens OnStage's Media page, and keeps its playlists with the user's things.
    PlaylistScreen::install();
    Playlists::setUserFolder ([] { return UserData::getFolder(); });
}

std::unique_ptr<Plugin> MainComponent::loadPlugin (const juce::File& file, const juce::String& pluginId, juce::String& error)
{
    auto plugin = Plugin::load (file, pluginId, error);

    if (plugin != nullptr)
        library.markUsed (Builtins::isBuiltinId (pluginId) ? Builtins::file().getFullPathName() : file.getFullPathName(), plugin->getDescriptor().id);

    return plugin;
}

std::vector<Containers::Choice> MainComponent::listPlugins()
{
    std::vector<Containers::Choice> choices;

    for (const auto& entry : library.getEntries())
    {
        if (entry.missing)
            continue;

        Containers::Choice choice;
        choice.file = entry.file;
        choice.id = entry.id;
        choice.name = entry.name;
        choice.vendor = entry.vendor;
        choice.kind = entry.kind;
        choice.category = entry.category;
        choice.format = entry.format();
        choice.favorite = entry.favorite;
        choices.push_back (choice);
    }

    return choices;
}

void MainComponent::openScreen (Plugin& plugin)
{
    int channel = 0, slot = 0;

    if (! engine.locatePlugin (plugin, channel, slot))
        return;

    // The page has to know the node's plugin before it can show it.
    if (std::exchange (containersChanged, false))
        pushTracks();

    openPluginScreen (channel, slot);

    // Its window is on Sonify's page: bring that forward, over the container's window.
    if (plugin.getScreenKind() != Plugin::ScreenKind::native)
        if (auto* top = getTopLevelComponent())
            top->toFront (true);
}

int MainComponent::allocateSlotId()
{
    return engine.allocateInsertId();
}

void MainComponent::containerChanged (ContainerPlugin&)
{
    markChanged();
    containersChanged = true;
}

juce::File MainComponent::containersFolder()
{
    return UserData::getFolder().getChildFile ("Containers");
}

double MainComponent::sampleRate() const
{
    return engine.getPlaybackRate();
}

juce::File MainComponent::pluginBrowseFolder()
{
    return prefs.nodeRackFolder.isNotEmpty() ? juce::File (prefs.nodeRackFolder) : juce::File();
}

void MainComponent::setPluginBrowseFolder (const juce::File& folder)
{
    if (! folder.isDirectory() || folder.getFullPathName() == prefs.nodeRackFolder)
        return;

    prefs.nodeRackFolder = folder.getFullPathName();   // Browse opens there next time
    savePreferences();
}

void* MainComponent::mainWindowHandle()
{
    // Rack windows belong to Sonify's window, so they stay in front of it.
    if (auto* peer = getPeer())
        return peer->getNativeHandle();

    return nullptr;
}

//==============================================================================
void MainComponent::addContainerInfo (Plugin& plugin, juce::DynamicObject& state)
{
    auto* container = dynamic_cast<ContainerPlugin*> (&plugin);

    if (container == nullptr)
        return;

    // What's inside: each node's plugin (named by its slot id), so its screen can be shown and
    // its controls moved like any plugin's. A container inside lists its own.
    juce::Array<juce::var> nodes;

    for (auto* node : container->getGraph().getNodes())
    {
        if (auto* pluginNode = dynamic_cast<PluginNode*> (node->getProcessor()))
        {
            auto item = pluginToVar (pluginNode->getPlugin());
            item.getDynamicObject()->setProperty ("insertId", pluginNode->slotId);
            item.getDynamicObject()->setProperty ("bypass", node->isBypassed());
            nodes.add (item);
        }
    }

    state.setProperty ("container", true);
    state.setProperty ("containerName", container->getContainerName());
    state.setProperty ("nodes", nodes);
}
