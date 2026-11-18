// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakGraph.cpp
#include "ZodiakGraph.h"
#include <map>
#include <set>

namespace zodiak::graph
{
    //==========================================================================
    //  Node
    //==========================================================================
    Node::Node (int u, int t, bool byHand) : uid (u), type (t)
    {
        int n = 0;
        const auto* ps = paramsOf (type, n);
        for (int p = 0; p < kMaxNodeParams; ++p)
            values[(size_t) p].store (p < n ? (byHand ? ps[p].add : ps[p].def) : 0.0f);

        dsp = dsp::make (type);
        if (ducker() != nullptr)
            for (int p = 0; p < n; ++p) set (p, byHand ? ps[p].add : ps[p].def);
        midi.ensureSize (4096);
    }

    Node::~Node() = default;

    ZBetel::StyleDucker* Node::ducker() noexcept
    {
        auto* d = type == ntDynEq ? dynamic_cast<dsp::DynEq*> (dsp.get()) : nullptr;
        return d != nullptr ? &d->ducker : nullptr;
    }

    void Node::prepare (double sampleRate, int maxBlock)
    {
        in.setSize (2, maxBlock, false, true, false);
        sc.setSize (2, maxBlock, false, true, false);
        out.setSize (2, maxBlock, false, true, false);
        hold.setSize (2, maxBlock, false, true, false);
        in.clear(); sc.clear(); out.clear(); hold.clear();
        holdLength = 0;
        if (dsp != nullptr) dsp->prepare (sampleRate, maxBlock);
        if (plugin != nullptr) plugin->prepare (sampleRate, maxBlock);
    }

    juce::String Node::title() const
    {
        if (plugin != nullptr) return plugin->name;
        if (isPluginType (type) && wantedName.isNotEmpty()) return wantedName;
        return typeName (type);
    }

    namespace
    {
        juce::AudioProcessorParameter* pluginParam (const Node& n, int p) noexcept
        {
            if (n.plugin == nullptr || n.plugin->instance == nullptr) return nullptr;
            const auto& ps = n.plugin->instance->getParameters();
            return p >= 0 && p < ps.size() ? ps[p] : nullptr;
        }

        int bandOf (int p) noexcept    { return p / 8; }
        int fieldOf (int p) noexcept   { return p % 8; }
    }

    int Node::numParams() const noexcept
    {
        if (isPluginType (type))
            return plugin != nullptr && plugin->instance != nullptr ? plugin->instance->getParameters().size() : 0;
        return numBuiltInParams (type);
    }

    juce::String Node::paramName (int p) const
    {
        if (isPluginType (type))
            if (auto* pp = pluginParam (*this, p)) return pp->getName (40);
        if (const auto* info = builtInParam (type, p)) return info->name;
        return {};
    }

    juce::String Node::paramId (int p) const
    {
        if (isPluginType (type)) return juce::String (p);
        if (const auto* info = builtInParam (type, p)) return info->id;
        return {};
    }

    int Node::paramFromId (const juce::String& id) const
    {
        if (isPluginType (type)) return id.containsOnly ("0123456789") && id.isNotEmpty() ? id.getIntValue() : -1;
        return builtInParamIndex (type, id);
    }

    float Node::get (int p) const noexcept
    {
        if (isPluginType (type))
        {
            auto* pp = pluginParam (*this, p);
            return pp != nullptr ? pp->getValue() : 0.0f;
        }
        if (p < 0 || p >= numBuiltInParams (type)) return 0.0f;
        if (type == ntDynEq)
        {
            auto* d = const_cast<Node*> (this)->ducker();
            const int b = bandOf (p);
            switch (fieldOf (p))
            {
                case 0: return d->isBandEnabled (b) ? 1.0f : 0.0f;
                case 1: return d->getFreq (b);
                case 2: return d->getQ (b);
                case 3: return d->getDepthDb (b);
                case 4: return d->getThresholdDb (b);
                case 5: return d->getRatio (b);
                case 6: return d->getAttackMs (b);
                default: return d->getReleaseMs (b);
            }
        }
        return values[(size_t) p].load (std::memory_order_relaxed);
    }

    void Node::set (int p, float v) noexcept
    {
        if (isPluginType (type))
        {
            if (auto* pp = pluginParam (*this, p)) pp->setValue (juce::jlimit (0.0f, 1.0f, v));
            return;
        }
        const auto* info = builtInParam (type, p);
        if (info == nullptr) return;
        v = juce::jlimit (juce::jmin (info->min, info->max), juce::jmax (info->min, info->max), v);
        if (info->kind == pkStepped || info->kind == pkMask) v = std::round (v);
        if (info->kind == pkToggle) v = v >= 0.5f ? 1.0f : 0.0f;

        if (type == ntDynEq)
        {
            auto* d = ducker();
            const int b = bandOf (p);
            switch (fieldOf (p))
            {
                case 0: d->setBandEnabled (b, v >= 0.5f); break;
                case 1: d->setFreq (b, v); break;
                case 2: d->setQ (b, v); break;
                case 3: d->setDepthDb (b, v); break;
                case 4: d->setThresholdDb (b, v); break;
                case 5: d->setRatio (b, v); break;
                case 6: d->setAttackMs (b, v); break;
                default: d->setReleaseMs (b, v); break;
            }
        }
        values[(size_t) p].store (v, std::memory_order_relaxed);
    }

    void Node::range (int p, float& min, float& max, float& skew) const noexcept
    {
        min = 0.0f; max = 1.0f; skew = 1.0f;
        if (const auto* info = builtInParam (type, p)) { min = info->min; max = info->max; skew = info->skew; }
    }

    int Node::places (int p) const noexcept
    {
        if (isPluginType (type))
        {
            auto* pp = pluginParam (*this, p);
            if (pp == nullptr || ! pp->isDiscrete()) return 0;
            const int steps = pp->getNumSteps();
            return steps >= 2 && steps <= 128 ? steps : 0;
        }
        const auto* info = builtInParam (type, p);
        if (info == nullptr) return 0;
        if (info->kind == pkToggle) return 2;
        if (info->kind == pkStepped) return (int) std::lround (info->max - info->min) + 1;
        return 0;
    }

    bool Node::isMask (int p) const noexcept
    {
        const auto* info = builtInParam (type, p);
        return info != nullptr && info->kind == pkMask;
    }

    float Node::restValue (int p) const noexcept
    {
        if (isPluginType (type))
        {
            auto* pp = pluginParam (*this, p);
            return pp != nullptr ? pp->getDefaultValue() : 0.0f;
        }
        const auto* info = builtInParam (type, p);
        return info != nullptr ? info->add : 0.0f;
    }

    float Node::getNorm (int p) const noexcept
    {
        float lo, hi, sk;
        range (p, lo, hi, sk);
        return toNorm (get (p), lo, hi, sk);
    }

    void Node::setNorm (int p, float n) noexcept
    {
        float lo, hi, sk;
        range (p, lo, hi, sk);
        set (p, fromNorm (juce::jlimit (0.0f, 1.0f, n), lo, hi, sk));
    }

    juce::String Node::text (int p, float v) const
    {
        if (isPluginType (type))
        {
            if (auto* pp = pluginParam (*this, p))
            {
                auto s = pp->getText (v, 24);
                const auto label = pp->getLabel();
                return label.isNotEmpty() && ! s.endsWith (label) ? s + " " + label : s;
            }
            return {};
        }
        return builtInText (type, p, v);
    }

    //==========================================================================
    //  The delays that keep branches in time
    //==========================================================================
    void Graph::Delay::setDelay (int samples)
    {
        delay = juce::jmax (0, samples);
        ring.setSize (2, juce::jmax (1, delay), false, true, false);
        ring.clear();
        pos = 0;
    }

    void Graph::Delay::add (const float* srcL, const float* srcR, float* dstL, float* dstR, int n) noexcept
    {
        if (delay <= 0)
        {
            juce::FloatVectorOperations::add (dstL, srcL, n);
            juce::FloatVectorOperations::add (dstR, srcR, n);
            return;
        }
        float* rl = ring.getWritePointer (0);
        float* rr = ring.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            dstL[i] += rl[pos];
            dstR[i] += rr[pos];
            rl[pos] = srcL[i];
            rr[pos] = srcR[i];
            if (++pos >= delay) pos = 0;
        }
    }

    void Graph::Delay::apply (float* L, float* R, int n) noexcept
    {
        if (delay <= 0) return;
        float* rl = ring.getWritePointer (0);
        float* rr = ring.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            std::swap (L[i], rl[pos]);
            std::swap (R[i], rr[pos]);
            if (++pos >= delay) pos = 0;
        }
    }

    //==========================================================================
    //  Graph
    //==========================================================================
    Graph::Graph()
    {
        clear();
    }

    Graph::~Graph()
    {
        // Plugins go before the rest of the node (their windows first)
        for (auto& n : nodes) n->plugin.reset();
    }

    void Graph::prepare (double sampleRate, int maxBlock, juce::AudioPlayHead* playHead)
    {
        sr = sampleRate;
        block = juce::jmax (32, maxBlock);
        head = playHead;
        const juce::SpinLock::ScopedLockType sl (lock);
        for (auto& n : nodes)
        {
            if (n->plugin != nullptr && n->plugin->instance != nullptr) n->plugin->instance->setPlayHead (head);
            n->prepare (sr, block);
        }
        tail.setDelay (tailSamples);
        plan = makePlan (nodePointers(), wires, 0);
        totalLatency.store (plan.latency);
        gen.fetch_add (1);
    }

    std::vector<Node*> Graph::nodePointers() const
    {
        std::vector<Node*> v;
        v.reserve (nodes.size());
        for (auto& n : nodes) v.push_back (n.get());
        return v;
    }

    Node* Graph::find (int uid) const noexcept
    {
        for (auto& n : nodes)
            if (n->uid == uid) return n.get();
        return nullptr;
    }

    Node* Graph::output() const noexcept
    {
        for (auto& n : nodes)
            if (n->type == ntOutput) return n.get();
        return nullptr;
    }

    Node* Graph::firstInstrument() const noexcept
    {
        Node* best = nullptr;
        for (auto& n : nodes)
            if (n->type == ntInstrument && (best == nullptr || n->pos.x < best->pos.x)) best = n.get();
        return best;
    }

    void Graph::clear()
    {
        // A uid is never given twice (a macro or a MIDI link left on a node that went
        // must not find another one in its place)
        std::vector<std::unique_ptr<Node>> old;
        {
            auto fresh = std::make_unique<Node> (nextUid++, ntOutput, false);
            fresh->pos = { 2120.0f, 220.0f };
            fresh->prepare (sr, block);
            Plan empty;
            const juce::SpinLock::ScopedLockType sl (lock);
            old.swap (nodes);
            wires.clear();
            nodes.push_back (std::move (fresh));
            std::swap (plan, empty);
        }
        for (auto& n : old) n->plugin.reset();
        old.clear();
        replan();
    }

    void Graph::makeDefault()
    {
        clear();
        static const int line[] = { ntInstrument, ntVowel, ntFilter, ntSaturator, ntGate, ntDynEq, ntDelay, ntReverb };
        int prev = 0;
        float x = 40.0f;
        for (int t : line)
        {
            auto* n = addNode (t, { x, 220.0f }, false);
            if (prev != 0) connect (prev, n->uid, kPinMain);
            prev = n->uid;
            x += 260.0f;
        }
        if (auto* o = output()) { o->pos = { x, 220.0f }; connect (prev, o->uid, kPinMain); }
    }

    Node* Graph::addNode (int type, juce::Point<float> pos, bool byHand, int uid)
    {
        if (type < 0 || type >= kNumNodeTypes || (int) nodes.size() >= kMaxNodes) return nullptr;
        if (type == ntOutput)
        {
            // There is one Output: a set's takes the place of the one there is
            if (auto* o = output())
            {
                if (uid != 0 && uid != o->uid && find (uid) == nullptr)
                {
                    auto moved = std::make_unique<Node> (uid, ntOutput, false);
                    moved->pos = pos;
                    moved->prepare (sr, block);
                    std::unique_ptr<Node> old;
                    {
                        const juce::SpinLock::ScopedLockType sl (lock);
                        for (auto& n : nodes)
                            if (n.get() == o) { old = std::move (n); n = std::move (moved); break; }
                        wires.erase (std::remove_if (wires.begin(), wires.end(), [o] (const Wire& w) { return w.src == o->uid || w.dst == o->uid; }), wires.end());
                        plan = {};
                    }
                    nextUid = juce::jmax (nextUid, uid + 1);
                    old.reset();
                    replan();
                    return output();
                }
                o->pos = pos;
                return o;
            }
        }
        if (uid != 0 && find (uid) != nullptr) return nullptr;
        if (uid == 0) uid = nextUid;
        nextUid = juce::jmax (nextUid, uid + 1);

        auto n = std::make_unique<Node> (uid, type, byHand);
        n->pos = pos;
        n->prepare (sr, block);
        auto* raw = n.get();
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            nodes.push_back (std::move (n));
        }
        gen.fetch_add (1);
        return raw;
    }

    bool Graph::removeNode (int uid)
    {
        auto* victim = find (uid);
        if (victim == nullptr || victim->type == ntOutput) return false;

        std::vector<Wire> left;
        for (auto& w : wires)
            if (w.src != uid && w.dst != uid) left.push_back (w);

        std::vector<Node*> list;
        for (auto& n : nodes)
            if (n->uid != uid) list.push_back (n.get());
        auto next = makePlan (list, left, uid);

        std::unique_ptr<Node> gone;
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            wires.swap (left);
            std::swap (plan, next);
            for (auto it = nodes.begin(); it != nodes.end(); ++it)
                if ((*it)->uid == uid) { gone = std::move (*it); nodes.erase (it); break; }
        }
        totalLatency.store (plan.latency);
        gen.fetch_add (1);
        if (gone != nullptr) gone->plugin.reset();
        gone.reset();
        return true;
    }

    bool Graph::wouldLoop (int src, int dst) const
    {
        if (src == dst) return true;
        const auto* d = find (dst);
        if (d != nullptr && d->type == ntFeedback) return false;

        // Can dst reach src, along cables that don't go into a Feedback node?
        std::vector<int> todo { dst };
        std::set<int> seen;
        while (! todo.empty())
        {
            const int at = todo.back();
            todo.pop_back();
            if (at == src) return true;
            if (! seen.insert (at).second) continue;
            for (auto& w : wires)
            {
                if (w.src != at) continue;
                const auto* to = find (w.dst);
                if (to != nullptr && to->type == ntFeedback) continue;
                todo.push_back (w.dst);
            }
        }
        return false;
    }

    Graph::Connect Graph::connect (int src, int dst, int pin)
    {
        const auto* s = find (src);
        const auto* d = find (dst);
        if (s == nullptr || d == nullptr || ! s->hasOutput() || pin < 0 || pin >= d->numInputs()) return Connect::invalid;
        const Wire w { src, dst, pin };
        if (std::find (wires.begin(), wires.end(), w) != wires.end()) return Connect::already;
        if (wouldLoop (src, dst)) return Connect::loop;

        auto cables = wires;
        cables.push_back (w);
        auto next = makePlan (nodePointers(), cables, 0);
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            wires.swap (cables);
            std::swap (plan, next);
        }
        totalLatency.store (plan.latency);
        gen.fetch_add (1);
        return Connect::ok;
    }

    bool Graph::disconnect (const Wire& w)
    {
        auto cables = wires;
        const auto it = std::find (cables.begin(), cables.end(), w);
        if (it == cables.end()) return false;
        cables.erase (it);
        auto next = makePlan (nodePointers(), cables, 0);
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            wires.swap (cables);
            std::swap (plan, next);
        }
        totalLatency.store (plan.latency);
        gen.fetch_add (1);
        return true;
    }

    void Graph::disconnectAll (int uid)
    {
        std::vector<Wire> left;
        for (auto& w : wires)
            if (w.src != uid && w.dst != uid) left.push_back (w);
        if (left.size() == wires.size()) return;
        auto next = makePlan (nodePointers(), left, 0);
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            wires.swap (left);
            std::swap (plan, next);
        }
        totalLatency.store (plan.latency);
        gen.fetch_add (1);
    }

    void Graph::setPlugin (int uid, std::unique_ptr<rack::Hosted> p)
    {
        auto* n = find (uid);
        if (n == nullptr || ! isPluginType (n->type)) return;
        if (p != nullptr && p->instance != nullptr)
        {
            p->instance->setPlayHead (head);
            p->prepare (sr, block);
            n->wantedFile = p->file;
            n->wantedSonify = p->sonifyId;
            n->wantedName = p->name;
        }
        std::unique_ptr<rack::Hosted> old;
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            old = std::move (n->plugin);
            n->plugin = std::move (p);
        }
        old.reset();
        replan();          // its latency
    }

    std::unique_ptr<rack::Hosted> Graph::takePlugin (int uid)
    {
        auto* n = find (uid);
        if (n == nullptr) return {};
        std::unique_ptr<rack::Hosted> p;
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            p = std::move (n->plugin);
        }
        replan();
        return p;
    }

    bool Graph::latencyMoved() const
    {
        for (auto& n : nodes)
        {
            const int now = n->plugin != nullptr && n->plugin->instance != nullptr ? n->plugin->instance->getLatencySamples() : 0;
            if (now != n->ownLatency) return true;
        }
        return false;
    }

    void Graph::rebuild()
    {
        replan();
    }

    void Graph::setTailDelay (int samples)
    {
        samples = juce::jlimit (0, (int) sr * 2, samples);
        if (samples == tailSamples) return;
        Delay next;
        next.setDelay (samples);
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            std::swap (tail, next);
            tailSamples = samples;
        }
    }

    void Graph::install (Plan&& p)
    {
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            std::swap (plan, p);
        }
        totalLatency.store (plan.latency);
        gen.fetch_add (1);
    }

    //==========================================================================
    //  The order: only what the Output hears, every node after what it hears
    //  (a Feedback node's input aside), and the delays that keep it in time
    //==========================================================================
    Graph::Plan Graph::makePlan (const std::vector<Node*>& list, const std::vector<Wire>& cables, int skipUid) const
    {
        Plan p;
        std::map<int, Node*> byUid;
        Node* out = nullptr;
        for (auto* n : list)
        {
            if (n->uid == skipUid) continue;
            byUid[n->uid] = n;
            if (n->type == ntOutput) out = n;
            n->ownLatency = n->plugin != nullptr && n->plugin->instance != nullptr ? juce::jmax (0, n->plugin->instance->getLatencySamples()) : 0;
        }
        if (out == nullptr) return p;

        // What the Output hears (back along every cable)
        std::set<int> live;
        std::vector<int> todo { out->uid };
        while (! todo.empty())
        {
            const int at = todo.back();
            todo.pop_back();
            if (! live.insert (at).second) continue;
            for (auto& w : cables)
                if (w.dst == at && byUid.count (w.src) != 0) todo.push_back (w.src);
        }

        auto isFb = [&] (int uid) { auto it = byUid.find (uid); return it != byUid.end() && it->second->type == ntFeedback; };

        // In order (Kahn's), a Feedback node's input not followed
        std::map<int, int> waiting;
        for (int u : live) waiting[u] = 0;
        for (auto& w : cables)
            if (live.count (w.src) != 0 && live.count (w.dst) != 0 && ! isFb (w.dst)) ++waiting[w.dst];

        std::vector<int> ready, order;
        for (auto& [u, c] : waiting)
            if (c == 0) ready.push_back (u);
        while (! ready.empty())
        {
            // Left to right on the canvas among those ready: the order a person reads
            std::sort (ready.begin(), ready.end(), [&] (int a, int b) { return byUid[a]->pos.x > byUid[b]->pos.x; });
            const int u = ready.back();
            ready.pop_back();
            order.push_back (u);
            for (auto& w : cables)
                if (w.src == u && live.count (w.dst) != 0 && ! isFb (w.dst))
                    if (--waiting[w.dst] == 0) ready.push_back (w.dst);
        }

        // Latency: each node's output, from the latest of its inputs
        std::map<int, int> latOut;
        for (int u : order)
        {
            int arrival = 0;
            if (! isFb (u))
                for (auto& w : cables)
                    if (w.dst == u && live.count (w.src) != 0) arrival = juce::jmax (arrival, latOut[w.src]);
            auto* n = byUid[u];
            latOut[u] = isFb (u) ? 0 : arrival + n->ownLatency;
        }

        for (int u : order)
        {
            Step st { byUid[u], {} };
            const bool fb = isFb (u);
            int arrival = 0;
            for (auto& w : cables)
                if (w.dst == u && live.count (w.src) != 0) arrival = juce::jmax (arrival, latOut[w.src]);

            for (auto& w : cables)
            {
                if (w.dst != u || live.count (w.src) == 0) continue;
                Feed f { byUid[w.src], w.pin, -1 };
                const int need = fb ? 0 : arrival - latOut[w.src];
                if (need > 0)
                {
                    p.delays.emplace_back();
                    p.delays.back().setDelay (need);
                    f.delay = (int) p.delays.size() - 1;
                }
                st.feeds.push_back (f);
            }
            if (fb)
            {
                p.feedback.push_back (st);
                p.steps.push_back ({ byUid[u], {} });
            }
            else
                p.steps.push_back (std::move (st));
        }

        p.latency = latOut[out->uid];
        return p;
    }

    //==========================================================================
    //  The audio thread
    //==========================================================================
    void Graph::gather (const std::vector<Feed>& feeds, Node& into, int n) noexcept
    {
        into.in.clear (0, n);
        if (into.numInputs() > 1) into.sc.clear (0, n);
        for (auto& f : feeds)
        {
            auto& dst = f.pin == kPinSc ? into.sc : into.in;
            const float* sL = f.src->out.getReadPointer (0);
            const float* sR = f.src->out.getReadPointer (1);
            float* dL = dst.getWritePointer (0);
            float* dR = dst.getWritePointer (1);
            if (f.delay >= 0) plan.delays[(size_t) f.delay].add (sL, sR, dL, dR, n);
            else
            {
                juce::FloatVectorOperations::add (dL, sL, n);
                juce::FloatVectorOperations::add (dR, sR, n);
            }
        }
    }

    int Graph::keySources() const noexcept
    {
        int mask = 0;
        for (auto& st : plan.steps)
            if (st.node->type == ntKey && st.node->on.load (std::memory_order_relaxed))
                mask |= 1 << juce::jlimit (0, kNumChannels - 1, (int) std::lround (st.node->values[0].load (std::memory_order_relaxed)));
        return mask;
    }

    void Graph::silence() noexcept
    {
        for (auto& nd : nodes)
        {
            if (nd->dsp != nullptr) nd->dsp->reset();
            nd->hold.clear();
            nd->holdLength = 0;
            nd->out.clear();
        }
        for (auto& d : plan.delays) { d.ring.clear(); d.pos = 0; }
        tail.ring.clear();
    }

    void Graph::render (int n, const juce::MidiBuffer& midiIn, const Transport& t, const Taps& taps, float* L, float* R) noexcept
    {
        std::fill_n (L, n, 0.0f);
        std::fill_n (R, n, 0.0f);
        const bool noteNow = notePending.exchange (false, std::memory_order_relaxed);

        std::array<float, kMaxNodeParams> v {};
        for (auto& st : plan.steps)
        {
            auto& nd = *st.node;
            float* oL = nd.out.getWritePointer (0);
            float* oR = nd.out.getWritePointer (1);
            const bool on = nd.on.load (std::memory_order_relaxed);

            switch (nd.type)
            {
                case ntInstrument:
                {
                    std::fill_n (oL, n, 0.0f);
                    std::fill_n (oR, n, 0.0f);
                    if (on && nd.plugin != nullptr && nd.plugin->instance != nullptr)
                    {
                        nd.midi.clear();
                        nd.midi.addEvents (midiIn, 0, n, 0);
                        nd.plugin->play (oL, oR, n, nd.midi, true);
                    }
                    break;
                }

                case ntKey:
                {
                    std::fill_n (oL, n, 0.0f);
                    std::fill_n (oR, n, 0.0f);
                    const int srcCh = juce::jlimit (0, kNumChannels - 1, (int) std::lround (nd.values[0].load (std::memory_order_relaxed)));
                    const int len = juce::jmin (n, taps.length[(size_t) srcCh]);
                    if (on && len > 0 && taps.L[(size_t) srcCh] != nullptr)
                    {
                        const float g = std::pow (10.0f, nd.values[1].load (std::memory_order_relaxed) * 0.05f);
                        juce::FloatVectorOperations::copyWithMultiply (oL, taps.L[(size_t) srcCh], g, len);
                        juce::FloatVectorOperations::copyWithMultiply (oR, taps.R[(size_t) srcCh], g, len);
                    }
                    break;
                }

                case ntFeedback:
                {
                    std::fill_n (oL, n, 0.0f);
                    std::fill_n (oR, n, 0.0f);
                    const int len = juce::jmin (n, nd.holdLength);
                    if (on && len > 0)
                    {
                        float g = juce::jlimit (0.0f, 1.0f, nd.values[0].load (std::memory_order_relaxed));
                        if (nd.values[1].load (std::memory_order_relaxed) > 0.5f) g = -g;
                        juce::FloatVectorOperations::copyWithMultiply (oL, nd.hold.getReadPointer (0), g, len);
                        juce::FloatVectorOperations::copyWithMultiply (oR, nd.hold.getReadPointer (1), g, len);
                    }
                    break;
                }

                case ntOutput:
                {
                    gather (st.feeds, nd, n);
                    juce::FloatVectorOperations::copy (L, nd.in.getReadPointer (0), n);
                    juce::FloatVectorOperations::copy (R, nd.in.getReadPointer (1), n);
                    break;
                }

                default:
                {
                    gather (st.feeds, nd, n);
                    juce::FloatVectorOperations::copy (oL, nd.in.getReadPointer (0), n);
                    juce::FloatVectorOperations::copy (oR, nd.in.getReadPointer (1), n);
                    if (! on) break;

                    if (nd.type == ntPlugin)
                    {
                        if (nd.plugin != nullptr && nd.plugin->instance != nullptr)
                            nd.plugin->play (oL, oR, n, nd.plugin->noMidi, false);
                    }
                    else if (nd.dsp != nullptr)
                    {
                        const int np = numBuiltInParams (nd.type);
                        for (int p = 0; p < np; ++p) v[(size_t) p] = nd.values[(size_t) p].load (std::memory_order_relaxed);
                        if (noteNow) nd.dsp->noteOn();
                        nd.dsp->notes = &midiIn;
                        const bool sc = nd.numInputs() > 1;
                        nd.dsp->process (oL, oR, n, sc ? nd.sc.getReadPointer (0) : nullptr, sc ? nd.sc.getReadPointer (1) : nullptr, t, v.data());
                    }
                    break;
                }
            }

            if (nd.type != ntOutput)
            {
                const float pk = juce::jmax (nd.out.getMagnitude (0, 0, n), nd.out.getMagnitude (1, 0, n));
                if (pk > nd.peak.load (std::memory_order_relaxed)) nd.peak.store (pk, std::memory_order_relaxed);
            }
        }

        // The Feedback nodes take in what came to them, for the next block
        for (auto& st : plan.feedback)
        {
            auto& nd = *st.node;
            nd.hold.clear (0, n);
            for (auto& f : st.feeds)
            {
                nd.hold.addFrom (0, 0, f.src->out, 0, 0, n);
                nd.hold.addFrom (1, 0, f.src->out, 1, 0, n);
            }
            nd.holdLength = n;
        }

        tail.apply (L, R, n);
    }
}
