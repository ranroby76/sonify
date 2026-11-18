// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakMixer.h
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <functional>
#include <map>
#include <memory>
#include "ZodiakWidgets.h"
#include "ZodiakPanels.h"
#include "PluginProcessor.h"

//==============================================================================
//  ZodiakMixer.h  —  the mixer at the bottom of Zodiak's window: one strip per
//  channel, then the MASTER.
//
//  A channel's strip: pan, M, S, its level, the fader and its meters, GRAPH
//  (its graph: the VSTi and its effects, ZodiakGraphUi.h), what it plays, its
//  name in its colour.  Everything else a channel does lives in its graph.
//
//  The MASTER: balance, mute, the fader, and the arrow that opens its VST FX
//  (in order: on/off, load, e; + : Wah, Phaser, or a VST from file;
//  right-click an item: remove it).
//
//  WindowHub keeps the small windows the e buttons open, and the plugins' own
//  windows (closed before a plugin goes): every one a window of its own, kept
//  on the screen.  Message thread only.
//==============================================================================

namespace zodiak::ui
{
    //==========================================================================
    class WindowHub final : public rack::EditorCloser
    {
    public:
        WindowHub();
        ~WindowHub() override;

        /** A window holding a component (an e's), by key: shown again if open. */
        void showPanel (const juce::String& key, const juce::String& title, std::unique_ptr<juce::Component> content,
                        juce::Component* near);

        /** A window whose content keeps its shape: its content at the size
            it was designed at, made smaller only to fit the screen; resizable,
            keeping its shape.  The content is owned, or borrowed (left alone
            when the window closes).  Already open: retitled and brought up. */
        void showScaled (const juce::String& key, const juce::String& title, juce::Component* borrowed,
                         std::unique_ptr<juce::Component> owned, juce::Rectangle<int> design, juce::Component* near);

        /** A plugin's own window: a window of its own (moved by its title bar or its
            header, kept on the screen), its header on top - its name, ON / OFF (the
            plugin bypassed), and its MIDI LINK: drop a source there, then move a
            control in the plugin.  rackCh: the channel whose graph holds it (0..7), or
            link::kMasterRack. */
        void showPluginEditor (const juce::String& key, juce::AudioProcessor* plugin, const juce::String& title, juce::Component* near,
                               ZodiakProcessor& proc, int rackCh);

        void close (const juce::String& key);
        void closeAll();
        bool isOpen (const juce::String& key) const;

        void closeEditorOf (juce::AudioProcessor* plugin) override;

    private:
        class Window;
        void closeLater (const juce::String& key);

        std::map<juce::String, std::unique_ptr<Window>> windows;
        std::shared_ptr<bool> token = std::make_shared<bool> (true);
    };

    //==========================================================================
    class MixerPanel final : public juce::Component,
                             private juce::Timer
    {
    public:
        MixerPanel (ZodiakProcessor& p, WindowHub& hub, std::function<void (int ch, juce::Component* near)> openGraph);
        ~MixerPanel() override;

        /** A plugin came or went (a set loaded): the strips look again. */
        void rebuild();

        void paint (juce::Graphics&) override;
        void resized() override;

        class Strip;

    private:
        void timerCallback() override;
        void layoutStrips();

        std::function<void (int, juce::Component*)> openGraph;
        ZodiakProcessor& proc;
        WindowHub&       hub;

        juce::Label    title;
        juce::Viewport view;
        juce::Component holder;
        std::array<std::unique_ptr<Strip>, kNumChannels> strips;
        std::unique_ptr<Strip> masterStrip;
        int shownRackGen = -1;

        friend class Strip;
    };
}
