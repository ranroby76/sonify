// D:\workspace\Fanan_web_hub\src\timberwolf\TwGraph.cpp
#include "TwGraph.h"
#include "TwRegistry.h"

#include <algorithm>
#include <functional>

namespace tw
{
    namespace
    {
        bool findIn (const std::vector<Node>& chain, int uid, Path& path, Location& out)
        {
            for (int i = 0; i < (int) chain.size(); ++i)
            {
                const auto& n = chain[(size_t) i];

                if (n.uid == uid)
                {
                    out.chain = path;
                    out.index = i;
                    return true;
                }

                if (n.kind == Node::Kind::parallel)
                {
                    path.push_back ({ i, false });
                    if (findIn (n.a, uid, path, out))
                        return true;
                    path.back().pathB = true;
                    if (findIn (n.b, uid, path, out))
                        return true;
                    path.pop_back();
                }
            }

            return false;
        }

        int countParallels (const std::vector<Node>& chain)
        {
            int count = 0;

            for (const auto& n : chain)
                if (n.kind == Node::Kind::parallel)
                    count += 1 + countParallels (n.a) + countParallels (n.b);

            return count;
        }

        juce::var chainToVar (const std::vector<Node>& chain)
        {
            juce::Array<juce::var> items;

            for (const auto& n : chain)
            {
                auto* o = new juce::DynamicObject();

                if (n.kind == Node::Kind::block)
                {
                    o->setProperty ("block", n.uid);
                }
                else
                {
                    o->setProperty ("parallel", n.uid);
                    o->setProperty ("a", chainToVar (n.a));
                    o->setProperty ("b", chainToVar (n.b));
                    o->setProperty ("split", n.settings.splitType);
                    o->setProperty ("xover", n.settings.crossoverHz);
                    o->setProperty ("levelA", n.settings.levelA);
                    o->setProperty ("levelB", n.settings.levelB);
                    o->setProperty ("panA", n.settings.panA);
                    o->setProperty ("panB", n.settings.panB);
                    o->setProperty ("invertB", n.settings.invertB);
                }

                items.add (juce::var (o));
            }

            return items;
        }
    }

    //==========================================================================
    int Graph::numParallels() const   { return countParallels (main); }

    std::vector<Node>* Graph::chainAt (const Path& path)
    {
        auto* chain = &main;

        for (const auto& step : path)
        {
            if (step.index < 0 || step.index >= (int) chain->size())
                return nullptr;

            auto& n = (*chain)[(size_t) step.index];

            if (n.kind != Node::Kind::parallel)
                return nullptr;

            chain = step.pathB ? &n.b : &n.a;
        }

        return chain;
    }

    const std::vector<Node>* Graph::chainAt (const Path& path) const
    {
        return const_cast<Graph*> (this)->chainAt (path);
    }

    std::optional<Location> Graph::find (int uid) const
    {
        Path path;
        Location out;

        if (findIn (main, uid, path, out))
            return out;

        return std::nullopt;
    }

    Node* Graph::node (int uid)
    {
        const auto at = find (uid);

        if (! at)
            return nullptr;

        auto* chain = chainAt (at->chain);
        return chain != nullptr ? &(*chain)[(size_t) at->index] : nullptr;
    }

    const Node* Graph::node (int uid) const   { return const_cast<Graph*> (this)->node (uid); }

    int Graph::uidInSlot (int slot) const
    {
        for (const auto& [uid, entry] : blocks)
            if (entry.slot == slot)
                return uid;

        return 0;
    }

    int Graph::freeSlot() const
    {
        for (int s = 0; s < maxBlocks; ++s)
            if (uidInSlot (s) == 0)
                return s;

        return -1;
    }

    //==========================================================================
    int Graph::addBlock (const juce::String& type, const Path& path, int index)
    {
        auto* chain = chainAt (path);
        const int slot = freeSlot();

        if (chain == nullptr || slot < 0 || registry::find (type) == nullptr)
            return 0;

        Node n;
        n.kind = Node::Kind::block;
        n.uid = takeUid();

        blocks[n.uid] = { type, slot };
        chain->insert (chain->begin() + juce::jlimit (0, (int) chain->size(), index), n);
        return n.uid;
    }

    int Graph::addParallel (const Path& path, int index)
    {
        auto* chain = chainAt (path);

        if (chain == nullptr || numParallels() >= maxBlocks - 1)
            return 0;

        Node n;
        n.kind = Node::Kind::parallel;
        n.uid = takeUid();
        chain->insert (chain->begin() + juce::jlimit (0, (int) chain->size(), index), n);
        return n.uid;
    }

    int Graph::splitAround (int uid)
    {
        const auto at = find (uid);

        if (! at || numParallels() >= maxBlocks - 1)
            return 0;

        auto* chain = chainAt (at->chain);
        Node inner = std::move ((*chain)[(size_t) at->index]);

        Node par;
        par.kind = Node::Kind::parallel;
        par.uid = takeUid();
        par.a.push_back (std::move (inner));

        (*chain)[(size_t) at->index] = std::move (par);
        return (*chain)[(size_t) at->index].uid;
    }

    bool Graph::removeNode (int uid, Node* taken)
    {
        const auto at = find (uid);

        if (! at)
            return false;

        auto* chain = chainAt (at->chain);

        if (taken != nullptr)
            *taken = std::move ((*chain)[(size_t) at->index]);

        chain->erase (chain->begin() + at->index);
        return true;
    }

    void Graph::dropBlocksIn (const Node& n)
    {
        if (n.kind == Node::Kind::block)
        {
            blocks.erase (n.uid);
            return;
        }

        for (const auto& x : n.a) dropBlocksIn (x);
        for (const auto& x : n.b) dropBlocksIn (x);
    }

    bool Graph::remove (int uid)
    {
        Node taken;

        if (! removeNode (uid, &taken))
            return false;

        dropBlocksIn (taken);
        return true;
    }

    bool Graph::unsplit (int parallelUid)
    {
        const auto at = find (parallelUid);

        if (! at)
            return false;

        auto* chain = chainAt (at->chain);
        auto& par = (*chain)[(size_t) at->index];

        if (par.kind != Node::Kind::parallel)
            return false;

        std::vector<Node> flat;
        for (auto& x : par.a) flat.push_back (std::move (x));
        for (auto& x : par.b) flat.push_back (std::move (x));

        chain->erase (chain->begin() + at->index);
        chain->insert (chain->begin() + at->index, std::make_move_iterator (flat.begin()), std::make_move_iterator (flat.end()));
        return true;
    }

    bool Graph::contains (const std::vector<Node>& chain, int uid)
    {
        for (const auto& n : chain)
        {
            if (n.uid == uid)
                return true;

            if (n.kind == Node::Kind::parallel && (contains (n.a, uid) || contains (n.b, uid)))
                return true;
        }

        return false;
    }

    bool Graph::move (int uid, const Path& path, int index)
    {
        const auto from = find (uid);

        if (! from || chainAt (path) == nullptr)
            return false;

        // A parallel can't go inside itself.
        if (const auto* n = node (uid); n != nullptr && n->kind == Node::Kind::parallel)
        {
            Path walk;
            for (const auto& step : path)
            {
                const auto* c = chainAt (walk);
                if (c != nullptr && step.index < (int) c->size() && (*c)[(size_t) step.index].uid == uid)
                    return false;
                walk.push_back (step);
            }
        }

        // Taking it out moves what comes after it in the same chain one place up, and the
        // indices of a path running through that chain after it, too.
        Path target = path;
        int targetIndex = index;

        const auto sameChainPrefix = [&] (const Path& p)
        {
            if (p.size() < from->chain.size())
                return false;

            for (size_t i = 0; i < from->chain.size(); ++i)
                if (! (p[i] == from->chain[i]))
                    return false;

            return true;
        };

        if (sameChainPrefix (target))
        {
            if (target.size() == from->chain.size())
            {
                if (targetIndex > from->index)
                    --targetIndex;
            }
            else
            {
                auto& step = target[from->chain.size()];
                if (step.index > from->index)
                    --step.index;
            }
        }

        Node taken;
        removeNode (uid, &taken);

        auto* chain = chainAt (target);

        if (chain == nullptr)   // never: but put it back rather than lose it
        {
            chain = chainAt (from->chain);
            chain->insert (chain->begin() + juce::jlimit (0, (int) chain->size(), from->index), std::move (taken));
            return false;
        }

        chain->insert (chain->begin() + juce::jlimit (0, (int) chain->size(), targetIndex), std::move (taken));
        return true;
    }

    bool Graph::swapPaths (int parallelUid)
    {
        auto* n = node (parallelUid);

        if (n == nullptr || n->kind != Node::Kind::parallel)
            return false;

        std::swap (n->a, n->b);
        std::swap (n->settings.levelA, n->settings.levelB);
        std::swap (n->settings.panA, n->settings.panB);
        return true;
    }

    bool Graph::replace (int uid, const juce::String& type)
    {
        auto it = blocks.find (uid);

        if (it == blocks.end() || registry::find (type) == nullptr)
            return false;

        it->second.type = type;
        return true;
    }

    void Graph::clear()
    {
        main.clear();
        blocks.clear();
        nextUid = 1;
    }

    //==========================================================================
    void Graph::collectUids (const std::vector<Node>& chain, std::vector<int>& into, bool blocksOnly)
    {
        for (const auto& n : chain)
        {
            if (n.kind == Node::Kind::block || ! blocksOnly)
                into.push_back (n.uid);

            if (n.kind == Node::Kind::parallel)
            {
                collectUids (n.a, into, blocksOnly);
                collectUids (n.b, into, blocksOnly);
            }
        }
    }

    std::vector<int> Graph::blockOrder() const
    {
        std::vector<int> order;
        collectUids (main, order, true);
        return order;
    }

    juce::var Graph::toVar() const
    {
        auto* root = new juce::DynamicObject();
        root->setProperty ("chain", chainToVar (main));

        auto* list = new juce::DynamicObject();

        for (const auto& [uid, entry] : blocks)
        {
            auto* b = new juce::DynamicObject();
            b->setProperty ("type", entry.type);
            b->setProperty ("slot", entry.slot);
            list->setProperty (juce::String (uid), juce::var (b));
        }

        root->setProperty ("blocks", juce::var (list));
        return juce::var (root);
    }

    bool Graph::fromVar (const juce::var& v)
    {
        const auto* chainList = v.getProperty ("chain", {}).getArray();
        const auto* blockList = v.getProperty ("blocks", {}).getDynamicObject();

        if (chainList == nullptr)
            return false;

        clear();

        // The effects first: their types and slots.
        std::map<int, BlockEntry> found;
        std::array<bool, maxBlocks> taken {};

        if (blockList != nullptr)
        {
            for (const auto& p : blockList->getProperties())
            {
                const int uid = p.name.toString().getIntValue();
                const auto type = p.value.getProperty ("type", {}).toString();
                const int slot = (int) p.value.getProperty ("slot", -1);

                if (uid <= 0 || registry::find (type) == nullptr || (int) found.size() >= maxBlocks)
                    continue;

                BlockEntry e { type, slot };

                if (slot < 0 || slot >= maxBlocks || taken[(size_t) slot])
                    e.slot = -1;
                else
                    taken[(size_t) slot] = true;

                found[uid] = e;
            }
        }

        // Effects whose slot clashed or was missing get the lowest free one.
        for (auto& [uid, e] : found)
        {
            if (e.slot >= 0)
                continue;

            for (int s = 0; s < maxBlocks; ++s)
                if (! taken[(size_t) s]) { e.slot = s; taken[(size_t) s] = true; break; }
        }

        int highest = 0;

        std::function<void (const juce::var&, std::vector<Node>&)> read = [&] (const juce::var& list, std::vector<Node>& into)
        {
            if (const auto* items = list.getArray())
            {
                for (const auto& item : *items)
                {
                    if (item.hasProperty ("block"))
                    {
                        const int uid = (int) item.getProperty ("block", 0);
                        auto it = found.find (uid);

                        if (it == found.end() || blocks.count (uid) != 0)
                            continue;

                        blocks[uid] = it->second;
                        Node n;
                        n.kind = Node::Kind::block;
                        n.uid = uid;
                        into.push_back (n);
                        highest = juce::jmax (highest, uid);
                    }
                    else if (item.hasProperty ("parallel"))
                    {
                        Node n;
                        n.kind = Node::Kind::parallel;
                        n.uid = (int) item.getProperty ("parallel", 0);
                        n.settings.splitType   = juce::jlimit (0, 1, (int) item.getProperty ("split", 0));
                        n.settings.crossoverHz = juce::jlimit (80.0f, 8000.0f, (float) (double) item.getProperty ("xover", 700.0));
                        n.settings.levelA      = juce::jlimit (-60.0f, 12.0f, (float) (double) item.getProperty ("levelA", 0.0));
                        n.settings.levelB      = juce::jlimit (-60.0f, 12.0f, (float) (double) item.getProperty ("levelB", 0.0));
                        n.settings.panA        = juce::jlimit (-100.0f, 100.0f, (float) (double) item.getProperty ("panA", 0.0));
                        n.settings.panB        = juce::jlimit (-100.0f, 100.0f, (float) (double) item.getProperty ("panB", 0.0));
                        n.settings.invertB     = (bool) item.getProperty ("invertB", false);
                        read (item.getProperty ("a", {}), n.a);
                        read (item.getProperty ("b", {}), n.b);
                        highest = juce::jmax (highest, n.uid);
                        into.push_back (std::move (n));
                    }
                }
            }
        };

        read (v.getProperty ("chain", {}), main);

        // Parallels with a uid that clashes or is missing get new ones.
        std::vector<int> seen;
        std::function<void (std::vector<Node>&)> fix = [&] (std::vector<Node>& chain)
        {
            for (auto& n : chain)
            {
                if (n.kind != Node::Kind::parallel)
                    continue;

                if (n.uid <= 0 || std::find (seen.begin(), seen.end(), n.uid) != seen.end() || blocks.count (n.uid) != 0)
                    n.uid = ++highest;

                seen.push_back (n.uid);
                fix (n.a);
                fix (n.b);
            }
        };

        fix (main);
        nextUid = highest + 1;
        return true;
    }
}
