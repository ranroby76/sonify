// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakGraphUi.h
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <memory>
#include "PluginProcessor.h"
#include "ZodiakWidgets.h"
#include "ZodiakPanels.h"
#include "ZodiakMixer.h"

//==============================================================================
//  ZodiakGraphUi.h  —  a channel's GRAPH on the screen, Colosseum's way.
//
//  THE CANVAS: the nodes as boxes, the cables between them.
//      drag a node                 moves it
//      drag from an output (right) to an input (left)    a cable; dropped on
//                                  an input that already has cables, it adds
//                                  one (an input sums what comes in)
//      drag from an input          picks up its last cable, to move it or drop
//                                  it on nothing (gone)
//      click a cable               chooses it; Delete (or its right-click) removes it
//      right-click the canvas      adds a node there
//      right-click a node          its menu: settings, the plugin (load, open,
//                                  remove), bypass, cut its cables, remove it
//      the dot on a node           ON / bypassed
//      e on a VSTi / VST FX        the plugin's own window (empty: load one)
//      double-click a node         its settings (a plugin's: its window)
//      drag the canvas             moves the view; the wheel scrolls it, with
//                                  Ctrl (Cmd) it zooms
//  A cable that would close a loop is refused (the canvas says why) unless it
//  goes into a Feedback node.  A Feedback node's cables are dashed.  A Dyn EQ
//  has two inputs: IN and SC (its key, orange).
//
//  UNDER IT: the settings of the node chosen - Zodiak's own nodes their
//  sliders and switches (each one takes a MIDI LINK source), a plugin's its
//  file, ON, OPEN, LOAD / REPLACE, REMOVE.
//
//  Message thread only.
//==============================================================================

namespace zodiak::ui
{
    /** The key a node's plugin window is kept by. */
    juce::String nodeWindowKey (int ch, int uid);

    /** A VSTi / VST FX node's plugin window; an empty node: the file chooser first. */
    void openNodePlugin (ZodiakProcessor& p, WindowHub& hub, int ch, int uid, juce::Component* near);

    /** The file chooser for a VSTi / VST FX node: the plugin chosen goes in (the one
        there goes) and its window opens, this once. */
    void chooseNodePlugin (ZodiakProcessor& p, WindowHub& hub, int ch, int uid, juce::Component* near);

    class GraphCanvas;

    //==========================================================================
    class GraphEditor final : public juce::Component,
                              private juce::Timer
    {
    public:
        GraphEditor (ZodiakProcessor& p, WindowHub& hub, int channel);
        ~GraphEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        /** A node's settings under the canvas (0: none). */
        void select (int uid);

    private:
        void timerCallback() override;

        ZodiakProcessor& proc;
        WindowHub& hub;
        const int ch;
        std::unique_ptr<GraphCanvas> canvas;
        std::unique_ptr<juce::Component> panel;
        int shownUid = -1;
        int shownRackGen = -1;
    };
}
