// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakLink.h
#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <functional>
#include <vector>
#include "ZodiakParams.h"
#include "../Preferences.h"     // SettingsFolder: Sonify's own folder

//==============================================================================
//  ZodiakLink.h  —  MIDI LINK: which knob, pad or key of a MIDI controller
//  moves which control of Zodiak (Grex's MIDI link system).
//
//  HOW IT IS USED.  Move a control on the hardware: its number shows in the
//  MIDI LINK window on the mode bar.  Drag it from there onto any knob, slider,
//  button or switch of Zodiak (any window: the banner, the mixer, a graph's
//  node windows, a node's bypass, a plugin's header) and drop it.  That is the link, made and saved.
//  A small window then offers how the control is driven (below), CLEAR and
//  CANCEL.  Tapping the MIDI LINK window arms its source instead: the next
//  click on a control takes it.  A right double-click on a linked control
//  opens the same small window, to change or clear it.
//
//  A plugin's own window can't be dropped on (it isn't Zodiak's): drop on its
//  header's LINK, then move the control in the plugin - that one is linked.
//
//  THE MAP is the rig's, not a set's: one file, <Sonify's folder>\Zodiak\MIDI Map.zmap,
//  written the moment a link is made, changed or cleared.  The MIDI LINK
//  window's right-click saves it as a named map, loads one, or clears it.
//
//  ONE SOURCE, ONE CONTROL: a source linked to a control leaves whatever it
//  moved before, and a control takes one source.
//
//  A SOURCE is a controller (CC) or a key / pad (NOTE) on its MIDI channel.  A
//  linked source is Zodiak's: it plays nothing and reaches no instrument.
//
//  HOW A SOURCE DRIVES ITS CONTROL (Mode):
//      KNOB    the source's 0..127 across the control's range.  A control with
//              places (a selector, a bank, a switch) gets them as equal
//              STATIONS: 128 / places values each (4 banks: 0-31 A, 32-63 B,
//              64-95 C, 96-127 D), so it clicks over at the same spots.
//      TOGGLE  each press flips the control (a switch: on / off; PLAY: plays
//              or stops; a knob: its top or its bottom).  Releases do nothing.
//      PUSH    on while held: press = on (top), release = off (bottom).
//  A controller's press is its value crossing 64 upward, its release crossing
//  back.  A new link takes KNOB on a control with a range, TOGGLE on a switch
//  or a button (and on anything a key or pad is dropped on).
//
//  THREADS.  The map is atomics: the audio thread finds a source's control
//  without a lock.  The settings that are atomics (channel, master, macros,
//  the graphs' nodes, the patterns, the transport) are moved right there (a
//  graph being changed at that moment misses the move).  The plugins are the
//  message thread's: their moves wait (in `pending`) for it, a few ms.
//==============================================================================

namespace zodiak::link
{
    //==========================================================================
    //  WHAT a source moves
    //==========================================================================
    enum Kind : int
    {
        kNone = 0,
        kMaster,        // a = the master parameter
        kChan,          // ch, a = the channel parameter
        kMacro,         // ch, a = the macro (0..8)
        kBank,          // ch
        kVar,           // ch
        kTransport,     // a = tPlayStop / tRewind / tStop
        kNode,          // ch, a = the graph node's uid, b = its setting; text = the setting's id
        kNodeOn,        // ch, a = the node's uid: on / bypass
        kPlugin,        // ch (0..7: a = the node's uid; kMasterRack: a = the n-th VST FX), b = the parameter; text = the plugin's name
        kPluginOn       // ch, a as kPlugin: its on / bypass
    };

    /** The kinds the message thread moves (their moves wait in `pending`). */
    inline bool isDeferred (int kind) noexcept      { return kind >= kPlugin; }

    enum TransportWhat : int { tPlayStop = 0, tRewind = 1, tStop = 2 };

    static constexpr int kMasterRack = 8;            // kPlugin's ch: the master's VST FX

    struct Target
    {
        int kind = kNone, ch = 0, a = 0, b = 0, c = 0;
        juce::String text;          // message thread only (see Kind)

        bool valid() const noexcept                 { return kind != kNone; }
        bool sameAs (const Target& o) const         { return kind == o.kind && ch == o.ch && a == o.a && b == o.b && c == o.c && text == o.text; }

        static Target master (int p)                { Target t; t.kind = kMaster; t.a = p; return t; }
        static Target chan (int ch, int p)          { Target t; t.kind = kChan; t.ch = ch; t.a = p; return t; }
        static Target macro (int ch, int k)         { Target t; t.kind = kMacro; t.ch = ch; t.a = k; return t; }
        static Target bank (int ch)                 { Target t; t.kind = kBank; t.ch = ch; return t; }
        static Target variation (int ch)            { Target t; t.kind = kVar; t.ch = ch; return t; }
        static Target node (int ch, int uid, int p, const juce::String& id = {})
        {
            Target t; t.kind = kNode; t.ch = ch; t.a = uid; t.b = p; t.text = id; return t;
        }
        static Target nodeOn (int ch, int uid)      { Target t; t.kind = kNodeOn; t.ch = ch; t.a = uid; return t; }
        static Target transport (int what)          { Target t; t.kind = kTransport; t.a = what; return t; }
        static Target plugin (int ch, int slot, int param, const juce::String& name)
        {
            Target t; t.kind = kPlugin; t.ch = ch; t.a = slot; t.b = param; t.text = name; return t;
        }
        static Target pluginOn (int ch, int slot, const juce::String& name)
        {
            Target t; t.kind = kPluginOn; t.ch = ch; t.a = slot; t.text = name; return t;
        }
    };

    /** The target's file name ("ch3.i3q.synth.f1_cutoff"), and back (an invalid one: not understood). */
    juce::String idOf (const Target& t);
    Target       fromId (const juce::String& id);

    /** The target as a person reads it ("Ch 3  Cutoff"). */
    juce::String labelOf (const Target& t);

    //==========================================================================
    //  WHERE from
    //==========================================================================
    enum SourceKind : int { srcNone = 0, srcNote = 1, srcCc = 2 };

    struct Source
    {
        int kind = srcNone, midiCh = 0, number = -1;

        bool valid() const noexcept                 { return kind != srcNone && number >= 0; }
        int  key() const noexcept                   { return valid() ? (kind << 12) | ((midiCh & 15) << 8) | (number & 127) : -1; }

        static Source fromKey (int k) noexcept
        {
            Source s;
            if (k < 0) return s;
            s.kind = (k >> 12) & 3;
            s.midiCh = (k >> 8) & 15;
            s.number = k & 127;
            return s;
        }

        /** "CC 21" / "NOTE 36" (and its MIDI channel: "CC 21 / 1"). */
        juce::String shortText() const              { return valid() ? (kind == srcCc ? "CC " : "NOTE ") + juce::String (number) : juce::String ("--"); }
        juce::String text() const                   { return valid() ? shortText() + "  (MIDI ch " + juce::String (midiCh + 1) + ")" : juce::String ("--"); }
    };

    enum Mode : int { kKnob = 0, kToggle = 1, kPush = 2 };

    inline const char* modeName (int m) noexcept   { return m == kToggle ? "TOGGLE" : m == kPush ? "PUSH" : "KNOB"; }

    // A move waiting for the message thread (Map::Slot::pending): 0..127 a KNOB value, or one of these
    static constexpr int kEvFlip = 1000, kEvPress = 1001, kEvRelease = 1002, kEvNone = -1;

    // The last source seen, for the MIDI LINK window: count (to tell a new one) | kind | MIDI channel | number
    inline int packLast (int count, int kind, int midiCh, int number) noexcept
    {
        return ((count & 0x3FFFF) << 13) | ((kind & 3) << 11) | ((midiCh & 15) << 7) | (number & 127);
    }
    inline Source unpackLast (int word) noexcept
    {
        Source s;
        if (word == 0) return s;
        s.kind = (word >> 11) & 3;
        s.midiCh = (word >> 7) & 15;
        s.number = word & 127;
        return s;
    }

    //==========================================================================
    //  THE MAP
    //==========================================================================
    class Map
    {
    public:
        static constexpr int kMaxLinks = 256;

        struct Slot
        {
            std::atomic<int> source { -1 };                 // Source::key, -1 = free
            std::atomic<int> kind { kNone }, ch { 0 }, a { 0 }, b { 0 }, c { 0 };
            std::atomic<int> mode { kKnob };
            std::atomic<int> last { 0 };                    // the audio thread's: the controller's previous value
            std::atomic<int> pending { kEvNone };           // a deferred move, for the message thread
            juce::String     text;                          // message thread
        };

        //======================================================================
        //  Message thread
        //======================================================================
        /** Links a source to a target: whatever had either before lets go. Its slot, or -1 (full). */
        int assign (const Source& s, const Target& t, int mode)
        {
            if (! s.valid() || ! t.valid()) return -1;
            const int key = s.key();
            for (int i = 0; i < kMaxLinks; ++i)
            {
                auto& sl = slots[(size_t) i];
                const int src = sl.source.load();
                if (src < 0) continue;
                if (src == key || targetOf (i).sameAs (t)) release (i);
            }

            // A free slot, looked for from after the last one used (a slot just let go of
            // is the last to be taken again: the audio thread may still be reading it)
            for (int n = 0; n < kMaxLinks; ++n)
            {
                const int i = (nextFree + n) % kMaxLinks;
                auto& sl = slots[(size_t) i];
                if (sl.source.load() >= 0) continue;
                sl.kind.store (t.kind); sl.ch.store (t.ch); sl.a.store (t.a); sl.b.store (t.b); sl.c.store (t.c);
                sl.text = t.text;
                sl.mode.store (juce::jlimit (0, 2, mode));
                sl.last.store (0);
                sl.pending.store (kEvNone);
                sl.source.store (key, std::memory_order_release);
                nextFree = (i + 1) % kMaxLinks;
                changed();
                return i;
            }
            return -1;
        }

        void clear (const Target& t)
        {
            bool any = false;
            for (int i = 0; i < kMaxLinks; ++i)
                if (slots[(size_t) i].source.load() >= 0 && targetOf (i).sameAs (t)) { release (i); any = true; }
            if (any) changed();
        }

        void clearAll()
        {
            for (int i = 0; i < kMaxLinks; ++i)
                if (slots[(size_t) i].source.load() >= 0) release (i);
            changed();
        }

        /** The slot linked to a target, or -1. */
        int find (const Target& t) const
        {
            for (int i = 0; i < kMaxLinks; ++i)
                if (slots[(size_t) i].source.load() >= 0 && targetOf (i).sameAs (t)) return i;
            return -1;
        }

        /** The slot a source is linked in, or -1. */
        int findSource (const Source& s) const
        {
            const int key = s.key();
            if (key < 0) return -1;
            for (int i = 0; i < kMaxLinks; ++i)
                if (slots[(size_t) i].source.load() == key) return i;
            return -1;
        }

        Source sourceOf (int slot) const            { return slot >= 0 && slot < kMaxLinks ? Source::fromKey (slots[(size_t) slot].source.load()) : Source(); }
        int    modeOf (int slot) const              { return slot >= 0 && slot < kMaxLinks ? slots[(size_t) slot].mode.load() : kKnob; }
        void   setMode (int slot, int mode)         { if (slot >= 0 && slot < kMaxLinks) { slots[(size_t) slot].mode.store (juce::jlimit (0, 2, mode)); changed(); } }

        Target targetOf (int slot) const
        {
            Target t;
            if (slot < 0 || slot >= kMaxLinks) return t;
            const auto& sl = slots[(size_t) slot];
            t.kind = sl.kind.load(); t.ch = sl.ch.load(); t.a = sl.a.load(); t.b = sl.b.load(); t.c = sl.c.load();
            t.text = sl.text;
            return t;
        }

        int count() const
        {
            int n = 0;
            for (const auto& s : slots) if (s.source.load() >= 0) ++n;
            return n;
        }

        /** Counts every change: the windows redraw their badges when it moves. */
        int generation() const noexcept             { return gen.load(); }

        /** Every link, as file entries (and back: the map as it is replaced). */
        std::unique_ptr<juce::XmlElement> toXml() const;
        void fromXml (const juce::XmlElement& xml);

        bool save (const juce::File& f) const
        {
            if (auto x = toXml()) { f.getParentDirectory().createDirectory(); return x->writeTo (f); }
            return false;
        }

        bool load (const juce::File& f)
        {
            if (! f.existsAsFile()) return false;
            if (auto x = juce::XmlDocument::parse (f))
                if (x->hasTagName ("ZODIAK_MIDI_MAP")) { fromXml (*x); return true; }
            return false;
        }

        //======================================================================
        //  Audio thread
        //======================================================================
        Slot&       slot (int i) noexcept           { return slots[(size_t) i]; }

    private:
        void release (int i)
        {
            auto& sl = slots[(size_t) i];
            sl.source.store (-1, std::memory_order_release);
            sl.pending.store (kEvNone);
            sl.kind.store (kNone);
        }

        void changed()                              { gen.fetch_add (1); }

        std::array<Slot, kMaxLinks> slots;
        int nextFree = 0;
        std::atomic<int> gen { 0 };
    };

    //==========================================================================
    //  Files
    //==========================================================================
    static constexpr const char* kMapExtension = ".zmap";

    /** The map in force, written at every change. */
    inline juce::File mapFile()
    {
        return SettingsFolder::get().getChildFile ("Zodiak").getChildFile ("MIDI Map.zmap");
    }

    /** Where named maps are saved and loaded from. */
    inline juce::File mapsFolder()
    {
        return SettingsFolder::get().getChildFile ("Zodiak").getChildFile ("MIDI Maps");
    }

    //==========================================================================
    //  Places: 0 = a range, else how many stations a KNOB source gets
    //==========================================================================
    inline int paramPlaces (const ParamInfo& p) noexcept
    {
        if (p.kind == pkToggle)  return 2;
        if (p.kind == pkStepped) return juce::jmax (2, (int) std::lround (p.max - p.min) + 1);
        return 0;
    }

    /** A KNOB value 0..127 as the control's 0..1: its station's, when it has places. */
    inline float knobNorm (int value, int places) noexcept
    {
        value = juce::jlimit (0, 127, value);
        if (places > 1)
        {
            const int place = juce::jmin (places - 1, (value * places) >> 7);
            return (float) place / (float) (places - 1);
        }
        return (float) value / 127.0f;
    }
}
