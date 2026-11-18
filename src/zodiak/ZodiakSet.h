// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakSet.h
#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <memory>
#include <vector>
#include "ZodiakEngine.h"
#include "../Preferences.h"     // SettingsFolder: Sonify's own folder

//==============================================================================
//  ZodiakSet.h  —  a SET: everything Zodiak plays, in one file
//  (<Sonify's folder>\Zodiak\Sets\<name>.zset; Sonify's folder is
//  %APPDATA%\Fanan\Sonify), as Grex's .bset is for Grex.
//
//      <ZODIAKSET version="5" name>
//        <MASTER volume pan mute tempo follow_daw low_velocity open>
//          <FX type="wah"|"phaser"|"plugin" on ...> (plugin state) </FX>   the master's VST FX
//        </MASTER>
//        <CHANNEL index bank variation transpose octave volume pan mute solo>
//          <MACRO slot dest value />                     x 9: dest "chan.octave",
//                                                        "node.<uid>.<setting>", or "none"
//          <GRAPH>                                       the channel's graph, whole
//            <NODE uid type x y on [p_<setting>=value ...]>   a built-in node's settings
//            <NODE uid type="vsti"|"vstfx" x y on file name> plugin state </NODE>
//            <WIRE src dst pin />                        a cable (pin 1: the Dyn EQ's SC)
//          </GRAPH>
//          <PATTERN slot="A1" bars grid sig own_sig notes />   its patterns (ZodiakPattern.h),
//        </CHANNEL>                                            the ones not empty
//      </ZODIAKSET>
//
//  A plugin is found again by its file: one that isn't there any more stays
//  in its node, empty (its name kept: the node says it's missing), and is
//  named, so the window can say so.  Every macro, MIDI link and cable to it
//  stays.
//
//  The DAW's project keeps the whole set inside it.  Numbers keep 2 decimals
//  (a node's settings 4: a cutoff's place matters).
//
//  VERSIONS: 5 is the graph's.  An older set (Zax ZE and I3Q, the chain, the
//  master reverb, songs) gives its mixer, its transport, its master VST FX and
//  its patterns; every channel gets the default graph.
//
//  Message thread only.
//==============================================================================

namespace zodiak::sets
{
    inline constexpr const char* kExtension = ".zset";
    inline constexpr const char* kTag       = "ZODIAKSET";
    inline constexpr int         kVersion   = 5;

    inline juce::File zodiakFolder()  { return SettingsFolder::get().getChildFile ("Zodiak"); }
    inline juce::File folder()        { return zodiakFolder().getChildFile ("Sets"); }
    inline juce::File defaultFile()   { return folder().getChildFile ("Default.zset"); }

    /** A plugin the set holds, to be loaded again (after the set is applied). */
    struct PluginToLoad
    {
        int  channel = 0;              // -1: the master
        int  node = 0;                 // a channel's: its graph node's uid
        int  builtInKind = -1;         // the master's built-in VST FX (Wah, Phaser) waiting its turn, else a plugin
        bool on = true;
        juce::File   file;
        juce::String sonifyId;         // one of Sonify's own plugins, in place of a file
        juce::String name, state;

        bool isSonify() const noexcept     { return sonifyId.isNotEmpty(); }
    };

    /** What the set says that lives on the message thread. */
    struct Info
    {
        juce::String name { "Default" };
        std::array<int, kNumChannels> bank {}, variation {};
        bool masterOpen = false;        // the master's strip shows its VST FX
    };

    // At most `places` decimals, no trailing zeros
    inline juce::String num (float v, int places = 2)
    {
        const double k = std::pow (10.0, places);
        auto s = juce::String (std::round ((double) v * k) / k, places);
        if (s.containsChar ('.'))
        {
            s = s.trimCharactersAtEnd ("0");
            if (s.endsWithChar ('.')) s = s.dropLastCharacters (1);
        }
        return s == "-0" ? juce::String ("0") : s;
    }

    //==========================================================================
    //  The master's VST FX
    //==========================================================================
    inline void captureInserts (rack::Rack& r, juce::XmlElement& into)
    {
        for (auto& ins : r.fx)
        {
            auto* e = into.createNewChildElement ("FX");
            e->setAttribute ("on", ins->on.load() ? 1 : 0);
            if (ins->builtIn != nullptr)
            {
                const int k = ins->builtIn->getKind();
                e->setAttribute ("type", inserts::kindId (k));
                for (int i = 0; i < inserts::numParams (k); ++i)
                    e->setAttribute (inserts::param (k, i).id, num (ins->builtIn->value (i).load()));
            }
            else if (ins->plugin != nullptr)
            {
                e->setAttribute ("type", "plugin");
                if (ins->plugin->sonifyId.isNotEmpty()) e->setAttribute ("sonify", ins->plugin->sonifyId);
                else                                     e->setAttribute ("file", ins->plugin->file.getFullPathName());
                e->setAttribute ("name", ins->plugin->name);
                e->addTextElement (rack::saveState (*ins->plugin));
            }
        }
    }

    // The VST FX in a set: the built-in ones go in now, the plugins after
    // (they load from their files), all in the set's order
    inline void applyInserts (const juce::XmlElement& from, std::vector<PluginToLoad>& plugins)
    {
        for (const auto* e : from.getChildWithTagNameIterator ("FX"))
        {
            const auto type = e->getStringAttribute ("type");
            PluginToLoad p;
            p.channel = -1;
            p.on = e->getBoolAttribute ("on", true);
            if (type == "plugin")
            {
                const auto path = e->getStringAttribute ("file");
                p.file = juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();
                p.sonifyId = e->getStringAttribute ("sonify");
                p.name = e->getStringAttribute ("name", p.isSonify() ? p.sonifyId : p.file.getFileNameWithoutExtension());
                p.state = e->getAllSubText().trim();
                plugins.push_back (p);
                continue;
            }

            const int kind = type == "phaser" ? inserts::kPhaser : type == "wah" ? inserts::kWah : -1;
            if (kind < 0) continue;

            // A built-in one keeps its place among the plugins: it waits its turn
            p.builtInKind = kind;
            p.name = inserts::kindName (kind);
            p.state = e->toString (juce::XmlElement::TextFormat().singleLine().withoutHeader());
            plugins.push_back (p);
        }
    }

    /** A built-in insert from what applyInserts kept of it. */
    inline void addBuiltInInsert (rack::Rack& r, const PluginToLoad& p)
    {
        const auto xml = juce::XmlDocument::parse (p.state);
        if (xml == nullptr) return;
        const int kind = p.builtInKind;
        if (kind < 0) return;
        const int uid = r.addBuiltIn (kind);
        if (auto* ins = r.findInsert (uid))
        {
            ins->on.store (p.on);
            for (int i = 0; i < inserts::numParams (kind); ++i)
            {
                const auto& ps = inserts::param (kind, i);
                ins->builtIn->value (i).store (juce::jlimit (ps.min, ps.max, (float) xml->getDoubleAttribute (ps.id, ps.def)));
            }
        }
    }

    //==========================================================================
    //  A channel's graph
    //==========================================================================
    inline void captureGraph (const graph::Graph& g, juce::XmlElement& into)
    {
        auto* e = into.createNewChildElement ("GRAPH");
        for (auto& n : g.nodeList())
        {
            auto* x = e->createNewChildElement ("NODE");
            x->setAttribute ("uid", n->uid);
            x->setAttribute ("type", graph::typeId (n->type));
            x->setAttribute ("x", num (n->pos.x, 0));
            x->setAttribute ("y", num (n->pos.y, 0));
            x->setAttribute ("on", n->on.load() ? 1 : 0);
            if (graph::isPluginType (n->type))
            {
                if (n->plugin != nullptr)
                {
                    if (n->plugin->sonifyId.isNotEmpty()) x->setAttribute ("sonify", n->plugin->sonifyId);
                    else                                   x->setAttribute ("file", n->plugin->file.getFullPathName());
                    x->setAttribute ("name", n->plugin->name);
                    x->addTextElement (rack::saveState (*n->plugin));
                }
                else if (n->wantedSonify.isNotEmpty())
                {
                    x->setAttribute ("sonify", n->wantedSonify);
                    x->setAttribute ("name", n->wantedName);
                }
                else if (n->wantedFile != juce::File())
                {
                    x->setAttribute ("file", n->wantedFile.getFullPathName());
                    x->setAttribute ("name", n->wantedName);
                }
                continue;
            }
            for (int p = 0; p < n->numParams(); ++p)
                x->setAttribute ("p_" + n->paramId (p), num (n->get (p), 4));
        }
        for (auto& w : g.wireList())
        {
            auto* x = e->createNewChildElement ("WIRE");
            x->setAttribute ("src", w.src);
            x->setAttribute ("dst", w.dst);
            if (w.pin != graph::kPinMain) x->setAttribute ("pin", w.pin);
        }
    }

    /** A graph from its set: its nodes (the plugins listed, to be loaded), its cables. */
    inline void applyGraph (graph::Graph& g, const juce::XmlElement& e, int ch, std::vector<PluginToLoad>& plugins)
    {
        g.clear();
        for (const auto* x : e.getChildWithTagNameIterator ("NODE"))
        {
            const int type = graph::typeFromId (x->getStringAttribute ("type"));
            if (type < 0) continue;
            const juce::Point<float> pos ((float) x->getDoubleAttribute ("x"), (float) x->getDoubleAttribute ("y"));
            auto* n = g.addNode (type, pos, false, x->getIntAttribute ("uid"));
            if (n == nullptr) continue;
            n->on.store (x->getBoolAttribute ("on", true));

            if (graph::isPluginType (type))
            {
                const auto path = x->getStringAttribute ("file");
                const auto sonifyId = x->getStringAttribute ("sonify");
                if (path.isEmpty() && sonifyId.isEmpty()) continue;
                PluginToLoad p;
                p.channel = ch;
                p.node = n->uid;
                p.file = juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();
                p.sonifyId = sonifyId;
                p.name = x->getStringAttribute ("name", p.isSonify() ? sonifyId : p.file.getFileNameWithoutExtension());
                p.state = x->getAllSubText().trim();
                n->wantedFile = p.file;
                n->wantedSonify = p.sonifyId;
                n->wantedName = p.name;
                plugins.push_back (p);
                continue;
            }
            for (int p = 0; p < n->numParams(); ++p)
                if (const auto a = "p_" + n->paramId (p); x->hasAttribute (a))
                    n->set (p, (float) x->getDoubleAttribute (a));
        }
        for (const auto* x : e.getChildWithTagNameIterator ("WIRE"))
            g.connect (x->getIntAttribute ("src"), x->getIntAttribute ("dst"), x->getIntAttribute ("pin", graph::kPinMain));
    }

    //==========================================================================
    inline std::unique_ptr<juce::XmlElement> capture (Engine& engine, const Info& info)
    {
        auto xml = std::make_unique<juce::XmlElement> (kTag);
        xml->setAttribute ("version", kVersion);
        xml->setAttribute ("name", info.name);

        auto* m = xml->createNewChildElement ("MASTER");
        for (int p = 0; p < kNumMasterParams; ++p)
            m->setAttribute (masterParamInfo (p).id, num (engine.master (p).load()));
        m->setAttribute ("open", info.masterOpen ? 1 : 0);
        captureInserts (engine.masterRack(), *m);

        for (int ch = 0; ch < kNumChannels; ++ch)
        {
            auto& st = engine.channel (ch);
            auto& g = engine.graph (ch);
            auto* c = xml->createNewChildElement ("CHANNEL");
            c->setAttribute ("index", ch + 1);
            c->setAttribute ("bank", info.bank[(size_t) ch] + 1);
            c->setAttribute ("variation", info.variation[(size_t) ch] + 1);
            for (int p = 0; p < kNumChanParams; ++p)
                c->setAttribute (chanParamInfo (p).id, num (st.chan[(size_t) p].load()));

            for (int k = 0; k < kNumMacros; ++k)
            {
                const auto& mac = st.macros[(size_t) k];
                auto* e = c->createNewChildElement ("MACRO");
                e->setAttribute ("slot", k + 1);
                e->setAttribute ("dest", macros::idOf (g, mac.dest.load()));
                e->setAttribute ("value", num (mac.value.load()));
            }

            captureGraph (g, *c);

            for (int i = 0; i < seq::kNumPatterns; ++i)
            {
                const auto pat = engine.patterns().get (ch, i);
                if (! pat.isDefault()) seq::writePattern (pat, i, *c);
            }
        }
        return xml;
    }

    /** A channel as a new set has it: the default graph, its macros, the mixer at rest. */
    inline void resetChannel (Engine& engine, int ch)
    {
        auto& st = engine.channel (ch);
        for (int p = 0; p < kNumChanParams; ++p) st.chan[(size_t) p].store (chanParamDefault (p));
        engine.graph (ch).makeDefault();
        engine.resetMacros (ch);
    }

    /** The set built in: every channel's default graph, no plugins, no notes. */
    inline void applyBuiltIn (Engine& engine, Info& info)
    {
        for (int p = 0; p < kNumMasterParams; ++p) engine.master (p).store (masterParamInfo (p).def);
        engine.masterRack().clearAll();
        engine.patterns().clearAll();
        for (int ch = 0; ch < kNumChannels; ++ch)
        {
            resetChannel (engine, ch);
            engine.patterns().selectNow (ch, 0);
        }
        info = {};
        info.name = "Default";
    }

    /** An older set's learnt controller ("midi channel:controller") into the MIDI LINK map,
        unless that controller is linked already. */
    inline void importLearn (Engine& engine, const juce::String& text, const link::Target& t)
    {
        if (! text.containsChar (':')) return;
        link::Source s;
        s.kind   = link::srcCc;
        s.midiCh = text.upToFirstOccurrenceOf (":", false, false).getIntValue() - 1;
        s.number = text.fromFirstOccurrenceOf (":", false, false).getIntValue();
        if (s.midiCh < 0 || s.midiCh > 15 || s.number < 0 || s.number > 127) return;
        if (engine.links().findSource (s) >= 0 || engine.links().find (t) >= 0) return;
        engine.links().assign (s, t, link::kKnob);
    }

    /** A set onto the engine. False (and nothing changed) when it isn't one.
        The plugins it holds are listed in `plugins`, in order, to be loaded. */
    inline bool apply (const juce::XmlElement& xml, Engine& engine, Info& info, std::vector<PluginToLoad>& plugins)
    {
        if (! xml.hasTagName (kTag)) return false;
        const int version = xml.getIntAttribute ("version", 1);

        Info next;
        next.name = xml.getStringAttribute ("name", "Set");
        plugins.clear();
        engine.masterRack().clearAll();

        if (const auto* m = xml.getChildByName ("MASTER"))
        {
            for (int p = 0; p < kNumMasterParams; ++p)
            {
                const auto& pi = masterParamInfo (p);
                engine.master (p).store (juce::jlimit (pi.min, pi.max, (float) m->getDoubleAttribute (pi.id, pi.def)));
            }
            next.masterOpen = m->getBoolAttribute ("open", false);
            applyInserts (*m, plugins);
        }

        engine.patterns().clearAll();
        for (int ch = 0; ch < kNumChannels; ++ch) resetChannel (engine, ch);

        for (const auto* c : xml.getChildWithTagNameIterator ("CHANNEL"))
        {
            int ch = c->getIntAttribute ("index", 0) - 1;
            // A version 1 set had one drum channel and the synths on 2..8: they were moved up by 3's time
            if (version < 3 && ch >= 1) ++ch;
            if (ch < 0 || ch >= kNumChannels) continue;

            auto& st = engine.channel (ch);
            next.bank[(size_t) ch] = juce::jlimit (0, 3, c->getIntAttribute ("bank", 1) - 1);
            next.variation[(size_t) ch] = juce::jlimit (0, 7, c->getIntAttribute ("variation", 1) - 1);
            for (int p = 0; p < kNumChanParams; ++p)
            {
                const auto& pi = chanParamInfo (p);
                st.chan[(size_t) p].store (juce::jlimit (pi.min, pi.max, (float) c->getDoubleAttribute (pi.id, pi.def)));
            }

            importLearn (engine, c->getStringAttribute ("bank_learn"), link::Target::bank (ch));
            importLearn (engine, c->getStringAttribute ("var_learn"), link::Target::variation (ch));

            auto& g = engine.graph (ch);
            if (const auto* ge = c->getChildByName ("GRAPH"))
                applyGraph (g, *ge, ch, plugins);

            // The macros: onto the graph as it now is (an older set's: the defaults)
            if (version >= kVersion)
                for (const auto* e : c->getChildWithTagNameIterator ("MACRO"))
                {
                    const int k = e->getIntAttribute ("slot", 0) - 1;
                    if (k < 0 || k >= kNumMacros) continue;
                    auto& mac = st.macros[(size_t) k];
                    mac.dest.store (macros::fromId (g, e->getStringAttribute ("dest")));
                    mac.value.store (juce::jlimit (0.0f, 1.0f, (float) e->getDoubleAttribute ("value", 0.5)));
                }
            else
                engine.resetMacros (ch);

            for (const auto* e : c->getChildWithTagNameIterator ("PATTERN"))
            {
                seq::Pattern pat;
                const int slot = seq::readPattern (*e, pat);
                if (slot >= 0) engine.patterns().set (ch, slot, std::move (pat));
            }
        }

        for (int ch = 0; ch < kNumChannels; ++ch)
            engine.patterns().selectNow (ch, seq::patternIndex (next.bank[(size_t) ch], next.variation[(size_t) ch]));
        info = next;
        return true;
    }

    inline juce::Result writeFile (const juce::XmlElement& xml, const juce::File& file)
    {
        if (const auto r = file.getParentDirectory().createDirectory(); r.failed()) return r;
        if (! xml.writeTo (file)) return juce::Result::fail ("Could not write " + file.getFullPathName());
        return juce::Result::ok();
    }

    inline std::unique_ptr<juce::XmlElement> readFile (const juce::File& file)
    {
        auto xml = juce::XmlDocument::parse (file);
        return (xml != nullptr && xml->hasTagName (kTag)) ? std::move (xml) : nullptr;
    }

    inline juce::Array<juce::File> list()
    {
        auto files = folder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + kExtension);
        files.sort();
        return files;
    }
}
