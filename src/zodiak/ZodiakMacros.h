// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakMacros.h
#pragma once

#include <juce_core/juce_core.h>
#include "ZodiakParams.h"
#include "ZodiakGraph.h"

//==============================================================================
//  ZodiakMacros.h  —  where a channel's nine macros go.
//
//  A macro IS its DESTINATION's control: the knob stands where the control
//  stands and shows its value.  Turn the macro and the control turns (the
//  node's window, the plugin's own window, the mixer); turn the control and
//  the macro follows.  There is one value, the control's.
//
//  A destination is any setting in the channel's graph - a built-in node's
//  (vowel, filter, gate, delay, saturator, reverb, Dyn EQ, phaser, wah,
//  feedback, sidechain in) or any parameter of a plugin there (a VSTi's or a
//  VST FX's) - or the channel's own Transpose, Octave, Volume or Pan.
//
//  The defaults, on the line a new channel has:
//      Octave, Cutoff, Resonance, Vowel, Drive, Trance Gate, Delay, Delay
//      Feedback, Reverb
//
//  A macro whose node goes (removed, or a set without it) has no destination:
//  it keeps a value of its own and moves nothing.
//
//  A destination is a code:  chan  (1 << 29) | the channel parameter
//                            node  (2 << 29) | uid << 13 | the node's setting
//  The set files keep it as text: "chan.octave", "node.<uid>.<setting id>"
//  (a plugin's setting id: its parameter's index).
//==============================================================================

namespace zodiak::macros
{
    enum DestKind : int { dkNone = 0, dkChan = 1, dkNode = 2 };

    inline int chanCode (int p) noexcept             { return (dkChan << 29) | (p & 0xFFFF); }
    inline int nodeCode (int uid, int p) noexcept    { return (dkNode << 29) | ((uid & 0xFFFF) << 13) | (p & 8191); }
    inline int kindOf (int c) noexcept               { return (c >> 29) & 3; }
    inline int chanOf (int c) noexcept               { return c & 0xFFFF; }
    inline int uidOf (int c) noexcept                { return (c >> 13) & 0xFFFF; }
    inline int paramOf (int c) noexcept              { return c & 8191; }

    /** The node a code points to (nullptr: none, or gone). */
    inline graph::Node* nodeFor (const graph::Graph& g, int c) noexcept
    {
        if (kindOf (c) != dkNode) return nullptr;
        auto* n = g.find (uidOf (c));
        return n != nullptr && paramOf (c) < n->numParams() ? n : nullptr;
    }

    inline bool isLinked (const graph::Graph& g, int c) noexcept
    {
        if (kindOf (c) == dkChan) return chanOf (c) < kNumChanParams;
        return nodeFor (g, c) != nullptr;
    }

    /** The steps a stepped destination has (0: smooth). */
    inline int stepsOf (const graph::Graph& g, int c) noexcept
    {
        if (kindOf (c) == dkChan)
        {
            const auto& info = chanParamInfo (chanOf (c));
            return info.kind == pkStepped ? (int) std::lround (info.max - info.min) : (info.kind == pkToggle ? 1 : 0);
        }
        if (auto* n = nodeFor (g, c)) { const int p = n->places (paramOf (c)); return p > 1 ? p - 1 : 0; }
        return 0;
    }

    /** Short, for the knob's name: the setting's own (the node's when it is ambiguous). */
    inline juce::String shortName (const graph::Graph& g, int c)
    {
        if (kindOf (c) == dkChan) return chanParamInfo (chanOf (c)).name;
        auto* n = nodeFor (g, c);
        if (n == nullptr) return "Macro";
        const int p = paramOf (c);
        if (graph::isPluginType (n->type)) return n->paramName (p);
        const juce::String id = n->paramId (p), name = n->paramName (p);
        switch (n->type)
        {
            case graph::ntVowel:    if (id == "mix") return "Vowel"; break;
            case graph::ntGate:     if (id == "depth") return "Trance Gate"; break;
            case graph::ntDelay:    if (id == "mix") return "Delay"; if (id == "fb") return "Delay FB"; break;
            case graph::ntReverb:   if (id == "mix") return "Reverb"; break;
            case graph::ntSaturator: if (id == "drive") return "Drive"; break;
            case graph::ntPhaser:   if (id == "mix") return "Phaser"; break;
            case graph::ntWah:      if (id == "mix") return "Wah"; break;
            default: break;
        }
        return name;
    }

    /** Long, for the menus: "Filter  Cutoff". */
    inline juce::String longName (const graph::Graph& g, int c)
    {
        if (kindOf (c) == dkChan) return juce::String ("Channel  ") + chanParamInfo (chanOf (c)).name;
        auto* n = nodeFor (g, c);
        return n != nullptr ? n->title() + "  " + n->paramName (paramOf (c)) : juce::String ("(none)");
    }

    /** The destination's value as text. */
    inline juce::String valueText (const graph::Graph& g, int c, float real)
    {
        if (kindOf (c) == dkChan)
        {
            const auto& info = chanParamInfo (chanOf (c));
            if (chanOf (c) == cpOctave || chanOf (c) == cpTranspose) return (real > 0 ? "+" : "") + juce::String ((int) std::lround (real)) + " " + info.unit;
            if (chanOf (c) == cpVolume) return juce::String (real, 1) + " dB";
            if (chanOf (c) == cpPan) return juce::String ((int) std::lround (real * 100.0f));
            return juce::String (real, 2);
        }
        if (auto* n = nodeFor (g, c)) return n->text (paramOf (c), real);
        return {};
    }

    /** The code as the set file keeps it, and back (0: not understood). */
    inline juce::String idOf (const graph::Graph& g, int c)
    {
        if (kindOf (c) == dkChan) return juce::String ("chan.") + chanParamInfo (chanOf (c)).id;
        if (kindOf (c) == dkNode)
        {
            auto* n = g.find (uidOf (c));
            if (n == nullptr) return "none";
            // A plugin waiting to load keeps its parameter's index; a built-in setting its id
            const juce::String pid = graph::isPluginType (n->type) ? juce::String (paramOf (c)) : n->paramId (paramOf (c));
            return pid.isNotEmpty() ? "node." + juce::String (uidOf (c)) + "." + pid : juce::String ("none");
        }
        return "none";
    }

    /** Reads an id back.  A node's setting by id needs the node: a plugin's
        parameter (an index) is taken as it is, before its plugin is loaded. */
    inline int fromId (const graph::Graph& g, const juce::String& id)
    {
        if (id.startsWith ("chan."))
        {
            const int p = chanParamIndex (id.substring (5).toRawUTF8());
            return p >= 0 ? chanCode (p) : 0;
        }
        if (id.startsWith ("node."))
        {
            const auto rest = id.substring (5);
            const int uid = rest.upToFirstOccurrenceOf (".", false, false).getIntValue();
            const auto pid = rest.fromFirstOccurrenceOf (".", false, false);
            auto* n = g.find (uid);
            if (n == nullptr) return 0;
            int p = -1;
            if (graph::isPluginType (n->type)) p = pid.containsOnly ("0123456789") && pid.isNotEmpty() ? pid.getIntValue() : -1;
            else                               p = n->paramFromId (pid);
            return p >= 0 ? nodeCode (uid, p) : 0;
        }
        return 0;
    }

    /** The nine defaults, on a graph (its first node of each kind). */
    inline std::array<int, kNumMacros> defaultsFor (const graph::Graph& g)
    {
        auto first = [&g] (int type) -> graph::Node*
        {
            graph::Node* best = nullptr;
            for (auto& n : g.nodeList())
                if (n->type == type && (best == nullptr || n->pos.x < best->pos.x)) best = n.get();
            return best;
        };
        auto at = [&] (int type, const char* id) -> int
        {
            auto* n = first (type);
            if (n == nullptr) return 0;
            const int p = n->paramFromId (id);
            return p >= 0 ? nodeCode (n->uid, p) : 0;
        };
        return { chanCode (cpOctave), at (graph::ntFilter, "cut"), at (graph::ntFilter, "res"), at (graph::ntVowel, "mix"),
                 at (graph::ntSaturator, "drive"), at (graph::ntGate, "depth"), at (graph::ntDelay, "mix"),
                 at (graph::ntDelay, "fb"), at (graph::ntReverb, "mix") };
    }
}
