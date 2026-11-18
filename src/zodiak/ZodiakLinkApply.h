// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakLinkApply.h
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ZodiakLink.h"

//==============================================================================
//  ZodiakLinkApply.h  —  MIDI LINK's message-thread side (ZodiakLink.h):
//  the moves that wait for it (Zax ZE's kit, the plugins), and what the
//  windows ask about a link's plugin.  Message thread only.
//==============================================================================

namespace zodiak
{
    class Engine;
    namespace rack { struct Hosted; }
}

namespace zodiak::link
{
    /** The waiting moves, done (the processor's timer, about 60 times a second). */
    void applyDeferred (Map& map, Engine& engine);

    /** A node target read from a file holds its setting's id: its index, found on the graph. */
    Target resolve (Engine& engine, Target t);

    /** The target as a person reads it, with the graph's names. */
    juce::String labelOf (Engine& engine, const Target& t);

    /** A new link's mode: TOGGLE on a switch, a button, or from a key / pad; else KNOB. */
    int defaultMode (Engine& engine, const Target& t, const Source& s);

    /** A plugin: a channel's graph's node's (slot = the node's uid), or the master's
        n-th VST FX (ch kMasterRack, slot n), when it is still the plugin named (an
        empty name: any). */
    rack::Hosted* findPlugin (Engine& engine, int ch, int slot, const juce::String& name);

    /** Where a plugin sits: its node's uid (a channel), its place in the master's VST FX, or -1. */
    int slotOf (Engine& engine, int ch, const juce::AudioProcessor* plugin);

    /** A plugin link's parameter's name (empty when the plugin isn't there). */
    juce::String pluginParamName (Engine& engine, const Target& t);
}
