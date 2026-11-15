// D:\workspace\Fanan_web_hub\src\timberwolf\TwFactory.cpp
#include "TwFactory.h"
#include "TwGraph.h"
#include "TwRegistry.h"

#include <functional>

namespace tw::factory
{
    namespace
    {
        /** A knob's value as the JSON gives it: a number in its own units, or a selector's
            position by its name. */
        float normOf (const KnobSpec& spec, const juce::var& v)
        {
            if (v.isString())
            {
                const int i = spec.choices.indexOf (v.toString(), true);
                return spec.toNorm (i >= 0 ? (float) i : spec.def);
            }

            if (v.isBool())
                return spec.toNorm (v ? spec.max : spec.min);

            return spec.toNorm ((float) (double) v);
        }

        struct Values
        {
            bool on = true;
            std::array<float, maxKnobs> knobs {};
        };

        void applyKnobs (const BlockInfo& info, const juce::var& k, Values& into)
        {
            if (const auto* o = k.getDynamicObject())
                for (const auto& p : o->getProperties())
                    for (size_t i = 0; i < info.knobs.size() && i < (size_t) maxKnobs; ++i)
                        if (info.knobs[i].name.equalsIgnoreCase (p.name.toString()))
                            into.knobs[i] = normOf (info.knobs[i], p.value);
        }

        Values defaultsOf (const BlockInfo& info)
        {
            Values v;
            for (size_t i = 0; i < (size_t) maxKnobs; ++i)
                v.knobs[i] = i < info.knobs.size() ? info.knobs[i].toNorm (info.knobs[i].def) : 0.5f;
            return v;
        }

        juce::var valuesToVar (const Values& v)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("on", v.on);
            juce::Array<juce::var> k;
            for (float x : v.knobs) k.add (x);
            o->setProperty ("knobs", k);
            return juce::var (o);
        }

        ParallelSettings settingsOf (const juce::var& s)
        {
            ParallelSettings p;
            p.splitType = s.getProperty ("type", "y").toString().equalsIgnoreCase ("crossover") ? 1 : 0;
            p.crossoverHz = (float) (double) s.getProperty ("xover", 700.0);
            p.levelA = (float) (double) s.getProperty ("levelA", 0.0);
            p.levelB = (float) (double) s.getProperty ("levelB", 0.0);
            p.panA = (float) (double) s.getProperty ("panA", 0.0);
            p.panB = (float) (double) s.getProperty ("panB", 0.0);
            p.invertB = (bool) s.getProperty ("invertB", false);
            return p;
        }
    }

    juce::StringArray problemsOf (const Preset& preset)
    {
        juce::StringArray problems;
        const auto spec = juce::JSON::parse (preset.json);

        if (! spec.isObject())
            return { "doesn't parse" };

        std::function<void (const juce::var&)> walk = [&] (const juce::var& list)
        {
            if (const auto* items = list.getArray())
                for (const auto& item : *items)
                {
                    if (item.hasProperty ("split"))
                    {
                        walk (item.getProperty ("a", {}));
                        walk (item.getProperty ("b", {}));
                        continue;
                    }

                    const auto type = item.getProperty ("fx", {}).toString();
                    const auto* info = registry::find (type);

                    if (info == nullptr) { problems.add ("no effect " + type); continue; }

                    if (const auto* k = item.getProperty ("k", {}).getDynamicObject())
                        for (const auto& p : k->getProperties())
                        {
                            const KnobSpec* spec = nullptr;
                            for (const auto& ks : info->knobs)
                                if (ks.name.equalsIgnoreCase (p.name.toString())) spec = &ks;

                            if (spec == nullptr)
                                problems.add (type + ": no knob " + p.name.toString());
                            else if (p.value.isString() && spec->choices.indexOf (p.value.toString(), true) < 0)
                                problems.add (type + "." + spec->name + ": no position " + p.value.toString());
                            else if (! p.value.isString() && ! spec->isChoice()
                                     && ((float) (double) p.value < spec->min || (float) (double) p.value > spec->max))
                                problems.add (type + "." + spec->name + ": out of range " + p.value.toString());
                        }
                }
        };

        walk (spec.getProperty ("chain", {}));
        return problems;
    }

    juce::var rigOf (const Preset& preset)
    {
        const auto spec = juce::JSON::parse (preset.json);
        jassert (spec.isObject());   // a factory rig that doesn't parse is a bug here

        Graph g;
        std::map<int, Values> values;
        std::vector<int> order;      // uids in the order the JSON writes them (for "fx" numbers)

        std::function<void (const juce::var&, const Path&)> build = [&] (const juce::var& list, const Path& path)
        {
            const auto* items = list.getArray();

            if (items == nullptr)
                return;

            for (const auto& item : *items)
            {
                auto* chain = g.chainAt (path);

                if (chain == nullptr)
                    return;

                const int index = (int) chain->size();

                if (item.hasProperty ("fx"))
                {
                    const auto type = item.getProperty ("fx", {}).toString();
                    const auto* info = registry::find (type);
                    jassert (info != nullptr);   // a factory rig naming an effect that isn't there

                    const int uid = info != nullptr ? g.addBlock (type, path, index) : 0;

                    if (uid == 0)
                        continue;

                    auto v = defaultsOf (*info);
                    v.on = ! (bool) item.getProperty ("off", false);
                    applyKnobs (*info, item.getProperty ("k", {}), v);
                    values[uid] = v;
                    order.push_back (uid);
                }
                else if (item.hasProperty ("split"))
                {
                    const int par = g.addParallel (path, index);

                    if (par == 0)
                        continue;

                    if (auto* n = g.node (par))
                        n->settings = settingsOf (item.getProperty ("split", {}));

                    auto pathA = path;
                    pathA.push_back ({ index, false });
                    build (item.getProperty ("a", {}), pathA);

                    auto pathB = path;
                    pathB.push_back ({ index, true });
                    build (item.getProperty ("b", {}), pathB);
                }
            }
        };

        build (spec.getProperty ("chain", {}), {});

        auto* root = new juce::DynamicObject();
        root->setProperty ("format", "Timber Wolf rig");
        root->setProperty ("version", 1);
        root->setProperty ("chain", g.toVar());

        auto* vals = new juce::DynamicObject();
        for (const auto& [uid, v] : values)
            vals->setProperty (juce::String (uid), valuesToVar (v));
        root->setProperty ("values", juce::var (vals));

        const auto uidOf = [&] (const juce::var& number) -> int
        {
            const int i = (int) number - 1;
            return i >= 0 && i < (int) order.size() ? order[(size_t) i] : 0;
        };

        // Snapshots: each from the rig's own values, with what it changes.
        juce::Array<juce::var> snaps;

        if (const auto* list = spec.getProperty ("snapshots", {}).getArray())
        {
            for (const auto& s : *list)
            {
                auto snapValues = values;

                if (const auto* offs = s.getProperty ("off", {}).getArray())
                    for (const auto& n : *offs)
                        if (const int uid = uidOf (n); uid != 0) snapValues[uid].on = false;

                if (const auto* ons = s.getProperty ("on", {}).getArray())
                    for (const auto& n : *ons)
                        if (const int uid = uidOf (n); uid != 0) snapValues[uid].on = true;

                if (const auto* ks = s.getProperty ("k", {}).getDynamicObject())
                    for (const auto& p : ks->getProperties())
                        if (const int uid = uidOf (p.name.toString().getIntValue()); uid != 0)
                            if (const auto* info = registry::find (g.blocks[uid].type))
                                applyKnobs (*info, p.value, snapValues[uid]);

                auto* o = new juce::DynamicObject();
                o->setProperty ("stored", true);
                o->setProperty ("name", s.getProperty ("name", {}).toString());

                auto* sv = new juce::DynamicObject();
                for (const auto& [uid, v] : snapValues)
                    sv->setProperty (juce::String (uid), valuesToVar (v));
                o->setProperty ("values", juce::var (sv));
                snaps.add (juce::var (o));
            }
        }

        root->setProperty ("snapshots", snaps);
        root->setProperty ("snapshotNow", snaps.isEmpty() ? -1 : 0);

        // Footswitches.
        juce::Array<juce::var> sw;

        if (const auto* list = spec.getProperty ("switches", {}).getArray())
        {
            for (const auto& s : *list)
            {
                static const juce::StringArray jobs { "none", "toggle", "presetUp", "presetDown", "snapshot", "tap", "tuner" };
                auto* o = new juce::DynamicObject();
                o->setProperty ("job", juce::jmax (0, jobs.indexOf (s.getProperty ("job", "none").toString(), true)));

                juce::Array<juce::var> uids;
                if (const auto* fx = s.getProperty ("fx", {}).getArray())
                    for (const auto& n : *fx)
                        if (const int uid = uidOf (n); uid != 0) uids.add (uid);

                o->setProperty ("uids", uids);
                o->setProperty ("snapshot", (int) s.getProperty ("snapshot", 1) - 1);
                o->setProperty ("label", s.getProperty ("label", {}).toString());
                sw.add (juce::var (o));
            }
        }

        root->setProperty ("switches", sw);

        // Pedals: { "fx": 2, "knob": "Pedal", "from": 0, "to": 100 } in the knob's own units.
        juce::Array<juce::var> pd;

        if (const auto* list = spec.getProperty ("pedals", {}).getArray())
        {
            for (const auto& p : *list)
            {
                auto* o = new juce::DynamicObject();
                const int uid = uidOf (p.getProperty ("fx", 0));
                const auto* info = uid != 0 ? registry::find (g.blocks[uid].type) : nullptr;
                int knob = 0;

                if (info != nullptr)
                    for (size_t i = 0; i < info->knobs.size(); ++i)
                        if (info->knobs[i].name.equalsIgnoreCase (p.getProperty ("knob", {}).toString()))
                            knob = (int) i;

                o->setProperty ("uid", info != nullptr ? uid : 0);
                o->setProperty ("knob", knob);

                if (info != nullptr)
                {
                    const auto& ks = info->knobs[(size_t) knob];
                    o->setProperty ("from", ks.toNorm ((float) (double) p.getProperty ("from", ks.min)));
                    o->setProperty ("to", ks.toNorm ((float) (double) p.getProperty ("to", ks.max)));
                }

                pd.add (juce::var (o));
            }
        }

        root->setProperty ("pedals", pd);
        return juce::var (root);
    }
}
