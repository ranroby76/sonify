// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakLink.cpp
#include "ZodiakLink.h"
#include "ZodiakLinkApply.h"
#include "ZodiakEngine.h"

namespace zodiak::link
{
    namespace
    {
        juce::String chPrefix (int ch)              { return "ch" + juce::String (ch + 1); }
        juce::String chName (int ch)                { return "Ch " + juce::String (ch + 1); }
        juce::String rackPrefix (int ch)            { return ch == kMasterRack ? juce::String ("master") : chPrefix (ch); }

        int chFrom (const juce::String& head)       // "ch3" -> 2, "master" -> kMasterRack, else -1
        {
            if (head == "master") return kMasterRack;
            if (! head.startsWith ("ch")) return -1;
            const int n = head.substring (2).getIntValue();
            return (n >= 1 && n <= kNumChannels) ? n - 1 : -1;
        }

        int masterIndex (const juce::String& id)
        {
            for (int p = 0; p < kNumMasterParams; ++p)
                if (id == masterParamInfo (p).id) return p;
            return -1;
        }

        int chanIndex (const juce::String& id)
        {
            for (int p = 0; p < kNumChanParams; ++p)
                if (id == chanParamInfo (p).id) return p;
            return -1;
        }

        bool isNumber (const juce::String& s)       { return s.isNotEmpty() && s.containsOnly ("0123456789"); }
    }

    //==========================================================================
    //  Ids:  master.volume  ch3.pan  ch3.macro2  ch3.bank  ch3.var
    //        ch3.n7.cut (node 7's setting "cut")  ch3.n7.on (its bypass)
    //        ch3.n4.p12 (the plugin in node 4, its parameter 12)  master.fx2.p7
    //        transport.playstop
    //==========================================================================
    juce::String idOf (const Target& t)
    {
        switch (t.kind)
        {
            case kMaster:    return "master." + juce::String (masterParamInfo (t.a).id);
            case kChan:      return chPrefix (t.ch) + "." + juce::String (chanParamInfo (t.a).id);
            case kMacro:     return chPrefix (t.ch) + ".macro" + juce::String (t.a + 1);
            case kBank:      return chPrefix (t.ch) + ".bank";
            case kVar:       return chPrefix (t.ch) + ".var";
            case kTransport: return t.a == tRewind ? "transport.rewind" : t.a == tStop ? "transport.stop" : "transport.playstop";
            case kNode:      return t.text.isNotEmpty() ? chPrefix (t.ch) + ".n" + juce::String (t.a) + "." + t.text : juce::String();
            case kNodeOn:    return chPrefix (t.ch) + ".n" + juce::String (t.a) + ".on";
            case kPlugin:    return rackPrefix (t.ch) + (t.ch == kMasterRack ? ".fx" : ".n") + juce::String (t.a) + ".p" + juce::String (t.b);
            case kPluginOn:  return rackPrefix (t.ch) + (t.ch == kMasterRack ? ".fx" : ".n") + juce::String (t.a) + ".on";
            default:         return {};
        }
    }

    Target fromId (const juce::String& id)
    {
        Target t;
        const auto head = id.upToFirstOccurrenceOf (".", false, false);
        const auto rest = id.fromFirstOccurrenceOf (".", false, false);

        if (head == "transport")
        {
            if (rest == "playstop") return Target::transport (tPlayStop);
            if (rest == "rewind")   return Target::transport (tRewind);
            if (rest == "stop")     return Target::transport (tStop);
            return t;
        }

        const int ch = chFrom (head);
        if (ch < 0) return t;

        const auto first = rest.upToFirstOccurrenceOf (".", false, false);
        const auto what  = rest.fromFirstOccurrenceOf (".", false, false);

        // The master's n-th VST FX
        if (ch == kMasterRack && first.startsWith ("fx") && isNumber (first.substring (2)))
        {
            const int slot = first.substring (2).getIntValue();
            if (what == "on") return Target::pluginOn (ch, slot, {});
            if (what.startsWith ("p") && isNumber (what.substring (1))) return Target::plugin (ch, slot, what.substring (1).getIntValue(), {});
            return t;
        }

        // A graph's node
        if (ch != kMasterRack && first.startsWith ("n") && isNumber (first.substring (1)) && what.isNotEmpty())
        {
            const int uid = first.substring (1).getIntValue();
            if (what == "on") return Target::nodeOn (ch, uid);
            if (what.startsWith ("p") && isNumber (what.substring (1))) return Target::plugin (ch, uid, what.substring (1).getIntValue(), {});
            Target n = Target::node (ch, uid, -1, what);      // its setting's index found from its id (resolve)
            return n;
        }

        if (head == "master")
        {
            const int p = masterIndex (rest);
            return p >= 0 ? Target::master (p) : t;
        }

        if (rest == "bank") return Target::bank (ch);
        if (rest == "var")  return Target::variation (ch);
        if (rest.startsWith ("macro"))
        {
            const int k = rest.substring (5).getIntValue() - 1;
            return k >= 0 && k < kNumMacros ? Target::macro (ch, k) : t;
        }

        const int p = chanIndex (rest);
        return p >= 0 ? Target::chan (ch, p) : t;
    }

    juce::String labelOf (const Target& t)
    {
        switch (t.kind)
        {
            case kMaster:    return juce::String (masterParamInfo (t.a).name);
            case kChan:      return chName (t.ch) + "  " + chanParamInfo (t.a).name;
            case kMacro:     return chName (t.ch) + "  Macro " + juce::String (t.a + 1);
            case kBank:      return chName (t.ch) + "  Bank";
            case kVar:       return chName (t.ch) + "  Variation";
            case kTransport: return t.a == tRewind ? "Rewind" : t.a == tStop ? "Stop" : "Play / Stop";
            case kNode:      return chName (t.ch) + "  Node " + juce::String (t.a) + "  " + t.text;
            case kNodeOn:    return chName (t.ch) + "  Node " + juce::String (t.a) + "  On";
            case kPlugin:
            case kPluginOn:
            {
                const auto where = t.ch == kMasterRack ? juce::String ("Master") : chName (t.ch);
                const auto name = t.text.isNotEmpty() ? t.text : juce::String (t.ch == kMasterRack ? "VST FX " + juce::String (t.a) : "Plugin");
                return where + "  " + name + (t.kind == kPluginOn ? juce::String ("  On") : "  #" + juce::String (t.b + 1));
            }
            default:         return {};
        }
    }

    juce::String labelOf (Engine& engine, const Target& t)
    {
        if (t.kind == kNode || t.kind == kNodeOn)
            if (auto* n = engine.graph (t.ch).find (t.a))
                return "Ch " + juce::String (t.ch + 1) + "  " + n->title() + "  "
                     + (t.kind == kNodeOn ? juce::String ("On") : n->paramName (t.b >= 0 ? t.b : n->paramFromId (t.text)));
        if (t.kind == kPlugin && t.ch != kMasterRack)
            if (const auto name = pluginParamName (engine, t); name.isNotEmpty())
                return "Ch " + juce::String (t.ch + 1) + "  " + (t.text.isNotEmpty() ? t.text : juce::String ("Plugin")) + "  " + name;
        return labelOf (t);
    }

    Target resolve (Engine& engine, Target t)
    {
        if (t.kind == kNode && t.b < 0)
            if (auto* n = engine.graph (t.ch).find (t.a)) t.b = n->paramFromId (t.text);
        return t;
    }

    //==========================================================================
    //  The map's file
    //==========================================================================
    std::unique_ptr<juce::XmlElement> Map::toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement> ("ZODIAK_MIDI_MAP");
        xml->setAttribute ("version", 1);
        for (int i = 0; i < kMaxLinks; ++i)
        {
            const auto s = sourceOf (i);
            if (! s.valid()) continue;
            const auto t = targetOf (i);
            const auto id = idOf (t);
            if (id.isEmpty()) continue;
            auto* e = xml->createNewChildElement ("LINK");
            e->setAttribute ("target", id);
            e->setAttribute ("source", s.kind == srcCc ? "cc" : "note");
            e->setAttribute ("channel", s.midiCh + 1);
            e->setAttribute ("number", s.number);
            e->setAttribute ("mode", juce::String (modeName (modeOf (i))).toLowerCase());
            if ((t.kind == kPlugin || t.kind == kPluginOn) && t.text.isNotEmpty())
                e->setAttribute ("plugin", t.text);
        }
        return xml;
    }

    void Map::fromXml (const juce::XmlElement& xml)
    {
        clearAll();
        for (const auto* e : xml.getChildWithTagNameIterator ("LINK"))
        {
            auto t = fromId (e->getStringAttribute ("target"));
            if (! t.valid()) continue;              // a map from a newer Zodiak: that one link is skipped
            if (t.kind == kPlugin || t.kind == kPluginOn) t.text = e->getStringAttribute ("plugin");

            Source s;
            s.kind = e->getStringAttribute ("source") == "note" ? srcNote : srcCc;
            s.midiCh = juce::jlimit (0, 15, e->getIntAttribute ("channel", 1) - 1);
            s.number = juce::jlimit (0, 127, e->getIntAttribute ("number", 0));

            const auto m = e->getStringAttribute ("mode");
            const int mode = m == "toggle" ? kToggle : m == "push" ? kPush : kKnob;
            assign (s, t, mode);
        }
    }

    //==========================================================================
    //  The message thread's moves: the plugins (ZodiakLinkApply.h)
    //==========================================================================
    namespace
    {
        float eventNorm (int ev, float current, int places) noexcept
        {
            if (ev >= 0 && ev <= 127) return knobNorm (ev, places);
            if (ev == kEvFlip)        return current >= 0.5f ? 0.0f : 1.0f;
            if (ev == kEvPress)       return 1.0f;
            if (ev == kEvRelease)     return 0.0f;
            return -1.0f;
        }

        std::atomic<bool>* onOf (Engine& engine, int ch, int slot)
        {
            if (ch == kMasterRack)
            {
                auto& r = engine.masterRack();
                return slot >= 1 && slot <= r.numInserts() ? &r.fx[(size_t) (slot - 1)]->on : nullptr;
            }
            auto* n = engine.graph (ch).find (slot);
            return n != nullptr ? &n->on : nullptr;
        }
    }

    rack::Hosted* findPlugin (Engine& engine, int ch, int slot, const juce::String& name)
    {
        rack::Hosted* h = nullptr;
        if (ch == kMasterRack)
        {
            auto& r = engine.masterRack();
            if (slot >= 1 && slot <= r.numInserts())
            {
                auto& ins = *r.fx[(size_t) (slot - 1)];
                if (ins.type == rack::Insert::pluginType) h = ins.plugin.get();
            }
        }
        else if (ch >= 0 && ch < kNumChannels)
        {
            if (auto* n = engine.graph (ch).find (slot)) h = n->plugin.get();
        }
        if (h == nullptr || h->instance == nullptr) return nullptr;
        if (name.isNotEmpty() && h->name != name) return nullptr;      // another plugin there now: not this link's
        return h;
    }

    int slotOf (Engine& engine, int ch, const juce::AudioProcessor* plugin)
    {
        if (ch == kMasterRack)
        {
            auto& r = engine.masterRack();
            for (int i = 0; i < r.numInserts(); ++i)
            {
                const auto& ins = *r.fx[(size_t) i];
                if (ins.plugin != nullptr && ins.plugin->instance.get() == plugin) return i + 1;
            }
            return -1;
        }
        for (auto& n : engine.graph (ch).nodeList())
            if (n->plugin != nullptr && n->plugin->instance.get() == plugin) return n->uid;
        return -1;
    }

    void applyDeferred (Map& map, Engine& engine)
    {
        for (int i = 0; i < Map::kMaxLinks; ++i)
        {
            auto& sl = map.slot (i);
            if (sl.source.load() < 0 || ! isDeferred (sl.kind.load())) continue;
            const int ev = sl.pending.exchange (kEvNone);
            if (ev == kEvNone) continue;
            const auto t = map.targetOf (i);

            if (t.kind == kPlugin)
            {
                auto* h = findPlugin (engine, t.ch, t.a, t.text);
                if (h == nullptr) continue;
                const auto& params = h->instance->getParameters();
                if (t.b < 0 || t.b >= params.size()) continue;
                auto* p = params[t.b];
                const int steps = p->getNumSteps();
                const int places = p->isBoolean() ? 2 : (p->isDiscrete() && steps >= 2 && steps <= 128 ? steps : 0);
                const float n = eventNorm (ev, p->getValue(), places);
                if (n < 0.0f) continue;
                p->beginChangeGesture();
                p->setValueNotifyingHost (n);
                p->endChangeGesture();
            }
            else if (t.kind == kPluginOn)
            {
                if (findPlugin (engine, t.ch, t.a, t.text) == nullptr) continue;
                auto* on = onOf (engine, t.ch, t.a);
                if (on == nullptr) continue;
                const float n = eventNorm (ev, on->load() ? 1.0f : 0.0f, 2);
                if (n >= 0.0f) on->store (n >= 0.5f);
            }
        }
    }

    int defaultMode (Engine& engine, const Target& t, const Source& s)
    {
        if (s.kind == srcNote) return kToggle;
        switch (t.kind)
        {
            case kTransport:
            case kPluginOn:
            case kNodeOn:    return kToggle;
            case kMaster:    return masterParamInfo (t.a).kind == pkToggle ? kToggle : kKnob;
            case kChan:      return chanParamInfo (t.a).kind == pkToggle ? kToggle : kKnob;
            case kNode:
                if (auto* n = engine.graph (t.ch).find (t.a)) return n->places (t.b) == 2 ? kToggle : kKnob;
                return kKnob;
            case kPlugin:
                if (auto* h = findPlugin (engine, t.ch, t.a, t.text))
                {
                    const auto& params = h->instance->getParameters();
                    if (t.b >= 0 && t.b < params.size() && params[t.b]->isBoolean()) return kToggle;
                }
                return kKnob;
            default:         return kKnob;
        }
    }

    juce::String pluginParamName (Engine& engine, const Target& t)
    {
        if (t.kind != kPlugin) return {};
        if (auto* h = findPlugin (engine, t.ch, t.a, t.text))
        {
            const auto& params = h->instance->getParameters();
            if (t.b >= 0 && t.b < params.size()) return params[t.b]->getName (40);
        }
        return {};
    }
}
