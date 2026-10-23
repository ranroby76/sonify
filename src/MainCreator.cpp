// D:\workspace\Fanan_web_hub\src\MainCreator.cpp
//
// The Plugins Creator Studio (CreatorStudio): Stella AI Studio inside Sonify, with Sonify AI
// building. It's native, so it lives in a window of its own laid over the page's studio area
// while the studio is in front (as the web tabs do), and steps aside for the page's dialogs and
// menus. Its tabs: Build with AI, Edit UI and Knob Studio.
//
// Build with AI makes everything, MIDI FX included: a MIDI FX is one of Sonify's devices, made by
// the Device Studio (MainDevices.cpp) and its own Sonify AI, shown in the studio's conversation. A
// device opened from a track's MIDI FX (or its menu's New Device) shows there too.
//
// Page events: creatorShow {x, y, width, height} (the studio area, in the page's pixels) |
// creatorHide {overlay (the studio is still in front, under a dialog or menu of the page)} |
// creatorTab {index} | creatorState. Sonify sends: creator {tab} whenever its tab changes;
// runCommand {command} for Sonify's keys pressed in it (Ctrl+1..8, Ctrl+Tab).

#include "MainComponent.h"
#include "CreatorPlugin.h"
#include "CreatorStudio.h"
#include "Settings.h"
#include "StellaLookAndFeel.h"

namespace
{
    /** The studio's own window, over Sonify's page (as WebTabLayer: a native window above the
        page's WebView2). */
    class CreatorLayer final : public juce::Component
    {
    public:
        CreatorLayer()
        {
            setOpaque (true);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (Theme::window);
        }

        void resized() override
        {
            for (auto* child : getChildren())
                child->setBounds (getLocalBounds());
        }
    };

    template <typename Handler>
    std::function<void (const juce::var&)> afterCreatorEvent (MainComponent* owner, Handler handler)
    {
        // As every page event: after WebView2's own callback has returned (see afterEvent).
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


//==============================================================================
void MainComponent::setupCreator()
{
    // Sonify's settings are read by now: the user data folder (where made plugins are) and
    // Block internet.
    Settings::setInternetBlocked (internetBlocked);
    library.refreshMadePlugins();

    creator = std::make_unique<CreatorStudio> (engine, *server);

    creator->onPageCommand = [this] (const juce::String& command) { surfaceCommand (command); };
    creator->onPluginAdded = [this] (const juce::File& folder, const juce::String& name, bool replaced)
    {
        creatorPluginAdded (folder, name, replaced);
    };

    // MIDI FX are Sonify's devices: the studio makes them with the Device Studio and its Sonify AI.
    if (deviceStudio != nullptr && deviceAgent != nullptr)
        creator->setMidiDevices (*deviceStudio, *deviceAgent);
}

juce::WebBrowserComponent::Options MainComponent::withCreatorEvents (juce::WebBrowserComponent::Options options)
{
    return options
        .withEventListener ("creatorShow", afterCreatorEvent (this, [this] (const juce::var& event)
        {
            creatorArea = { static_cast<int> (event.getProperty ("x", 0)), static_cast<int> (event.getProperty ("y", 0)),
                            static_cast<int> (event.getProperty ("width", 0)), static_cast<int> (event.getProperty ("height", 0)) };
            creatorShown = true;
            placeCreator();

            if (creator != nullptr)
                creator->setShown (true);
        }))
        .withEventListener ("creatorHide", afterCreatorEvent (this, [this] (const juce::var& event)
        {
            creatorShown = false;
            hideCreator();

            // Only stepping aside for the page's dialog or menu: the studio is still in front,
            // and keeps the live notes.
            if (creator != nullptr && ! static_cast<bool> (event.getProperty ("overlay", false)))
                creator->setShown (false);
        }))
        .withEventListener ("creatorTab", afterCreatorEvent (this, [this] (const juce::var& event)
        {
            if (creator != nullptr)
                creator->showTab (static_cast<int> (event.getProperty ("index", 0)));

            emitCreatorState();
        }))
        .withEventListener ("creatorState", afterCreatorEvent (this, [this] (const juce::var&) { emitCreatorState(); }));
}

void MainComponent::placeCreator()
{
    auto* peer = getPeer();

    if (creator == nullptr || ! creatorShown || creatorArea.isEmpty() || peer == nullptr)
    {
        hideCreator();
        return;
    }

    if (creatorLayer == nullptr)
    {
        creatorLayer = std::make_unique<CreatorLayer>();
        creatorLayer->addAndMakeVisible (*creator);
    }

    if (! creatorLayer->isOnDesktop())
        creatorLayer->addToDesktop (0, peer->getNativeHandle());

    // The page's pixels are this component's own (the page fills it); the layer sits in the main
    // window, so the area is carried over to the window's coordinates.
    creatorLayer->setBounds (getTopLevelComponent()->getLocalArea (this, creatorArea));
    creatorLayer->setVisible (true);
    creatorLayer->toFront (false);   // above the page, every time
}

void MainComponent::hideCreator()
{
    if (creatorLayer == nullptr || ! creatorLayer->isVisible())
        return;

    // The keyboard goes back to the page, which is in front again.
    const bool hadFocus = creatorLayer->hasKeyboardFocus (true);
    creatorLayer->setVisible (false);

    if (hadFocus && webView != nullptr)
        webView->grabKeyboardFocus();
}

void MainComponent::emitCreatorState()
{
    if (webView == nullptr || creator == nullptr)
        return;

    juce::DynamicObject::Ptr state (new juce::DynamicObject());
    state->setProperty ("tab", creator->getCurrentTab());
    webView->emitEventIfBrowserIsVisible ("creator", juce::var (state.get()));
}

void MainComponent::creatorPluginAdded (const juce::File& folder, const juce::String& name, bool replaced)
{
    library.refreshMadePlugins (folder);

    pushLibrary (replaced ? name + " is updated in the Browser (Made in Sonify). Tracks that already play it get the new "
                                   "version when their song opens again."
                          : name + " is in the Browser now, under Made in Sonify: drag it onto a track.");
}
