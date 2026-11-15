// D:\workspace\Fanan_web_hub\src\timberwolf\TwEngine.cpp
#include "TwEngine.h"

#include <thread>

namespace tw
{
    namespace
    {
        constexpr int maxCompensation = 8192;   // samples a blend can delay a path by

        /** A 2nd-order Butterworth section (two make a Linkwitz-Riley crossover). */
        struct Biquad
        {
            float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
            float z1 = 0, z2 = 0;

            void lowpass (double sr, double hz)
            {
                const double w = juce::MathConstants<double>::twoPi * hz / sr;
                const double c = std::cos (w), s = std::sin (w), alpha = s / (2.0 * 0.70710678);
                const double a0 = 1.0 + alpha;
                b0 = (float) ((1.0 - c) * 0.5 / a0); b1 = (float) ((1.0 - c) / a0); b2 = b0;
                a1 = (float) (-2.0 * c / a0); a2 = (float) ((1.0 - alpha) / a0);
            }

            void highpass (double sr, double hz)
            {
                const double w = juce::MathConstants<double>::twoPi * hz / sr;
                const double c = std::cos (w), s = std::sin (w), alpha = s / (2.0 * 0.70710678);
                const double a0 = 1.0 + alpha;
                b0 = (float) ((1.0 + c) * 0.5 / a0); b1 = (float) (-(1.0 + c) / a0); b2 = b0;
                a1 = (float) (-2.0 * c / a0); a2 = (float) ((1.0 - alpha) / a0);
            }

            float run (float x) noexcept
            {
                const float y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;
                return y;
            }

            void clear() noexcept   { z1 = z2 = 0.0f; }
        };

        struct Ring
        {
            std::vector<float> data;
            int pos = 0;

            void prepare (int size)     { data.assign ((size_t) size, 0.0f); pos = 0; }
            void clear()                { std::fill (data.begin(), data.end(), 0.0f); pos = 0; }

            /** Writes x, returns what was written delay samples ago. */
            float push (float x, int delay) noexcept
            {
                const int size = (int) data.size();
                data[(size_t) pos] = x;
                int read = pos - delay;
                if (read < 0) read += size;
                const float y = data[(size_t) read];
                if (++pos >= size) pos = 0;
                return y;
            }
        };

        void balance (float pan, float& gl, float& gr) noexcept
        {
            const float p = juce::jlimit (-1.0f, 1.0f, pan * 0.01f);
            gl = p > 0.0f ? std::cos (p * juce::MathConstants<float>::halfPi) : 1.0f;
            gr = p < 0.0f ? std::cos (-p * juce::MathConstants<float>::halfPi) : 1.0f;
        }
    }

    //==========================================================================
    struct Engine::BlockUnit
    {
        int uid = 0;
        juce::String type;
        const BlockInfo* info = nullptr;
        std::unique_ptr<Block> block;
        int slot = 0;

        // Audio thread only.
        float gain = 1.0f;          // the on/off fade
        bool ringing = false;       // off, but its tail still sounding (trails)
        int quietFor = 0;           // samples its output has been silent while off
        std::atomic<bool> sounding { true };
    };

    struct Engine::ParUnit
    {
        int uid = 0;
        std::atomic<int> splitType { 0 };
        std::atomic<float> crossoverHz { 700.0f }, levelA { 0.0f }, levelB { 0.0f }, panA { 0.0f }, panB { 0.0f };
        std::atomic<bool> invertB { false };

        // Audio thread only.
        Biquad lowA[2][2], highB[2][2];   // [channel][stage]
        float designedHz = -1.0f;
        Ring delay[2][2];                 // [path][channel]
        Smooth gain[2][2];                // [path][channel]

        void prepare (double sr)
        {
            for (auto& path : delay)
                for (auto& ring : path)
                    ring.prepare (maxCompensation + 1);

            for (auto& path : gain)
                for (auto& g : path)
                    g.prepare (sr, 15.0f, 1.0f);

            designedHz = -1.0f;
            clear();
        }

        void clear()
        {
            for (auto& ch : lowA)  for (auto& s : ch) s.clear();
            for (auto& ch : highB) for (auto& s : ch) s.clear();
            for (auto& path : delay) for (auto& ring : path) ring.clear();
        }
    };

    struct Engine::Op
    {
        enum class Type { block, fork, blend };

        Type type = Type::block;
        int level = 0, levelB = 0;
        BlockUnit* unit = nullptr;
        ParUnit* par = nullptr;
        int delayA = 0, delayB = 0;
    };

    struct Engine::Plan
    {
        std::vector<Op> ops;
        int levels = 1;
        int latency = 0;
    };

    //==========================================================================
    Engine::Engine()
    {
        handOver (std::make_unique<Plan>());
    }

    Engine::~Engine()
    {
        current.store (nullptr);
    }

    void Engine::setSlotSources (const std::array<SlotSource, maxBlocks>& sources)
    {
        slots = sources;
    }

    void Engine::prepare (double sampleRate, int maxBlock)
    {
        ctx.sampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
        ctx.maxBlock = juce::jmax (16, maxBlock);

        buffers.setSize (2 * maxLevels, ctx.maxBlock, false, true, false);
        scratch.setSize (3, ctx.maxBlock, false, true, false);

        for (auto& [uid, u] : units)
        {
            u->block->context = &ctx;
            u->block->prepare (ctx.sampleRate, ctx.maxBlock);
        }

        for (auto& [uid, p] : parallels)
            p->prepare (ctx.sampleRate);

        prepared = true;
    }

    void Engine::reset()
    {
        for (auto& [uid, u] : units)
            u->block->reset();

        for (auto& [uid, p] : parallels)
            p->clear();
    }

    //==========================================================================
    int Engine::compile (const std::vector<Node>& chain, int level, int& nextLevel, Plan& plan, const Graph& graph)
    {
        int lat = 0;

        for (const auto& n : chain)
        {
            if (n.kind == Node::Kind::block)
            {
                auto it = units.find (n.uid);

                if (it == units.end())
                    continue;

                Op op;
                op.type = Op::Type::block;
                op.level = level;
                op.unit = it->second.get();
                plan.ops.push_back (op);
                lat += it->second->block->latency();
                continue;
            }

            auto pit = parallels.find (n.uid);

            if (pit == parallels.end() || nextLevel >= maxLevels)
                continue;

            const int levelB = nextLevel++;
            plan.levels = juce::jmax (plan.levels, nextLevel);

            Op fork;
            fork.type = Op::Type::fork;
            fork.level = level;
            fork.levelB = levelB;
            fork.par = pit->second.get();
            plan.ops.push_back (fork);

            const int latA = compile (n.a, level, nextLevel, plan, graph);
            const int latB = compile (n.b, levelB, nextLevel, plan, graph);

            Op blend;
            blend.type = Op::Type::blend;
            blend.level = level;
            blend.levelB = levelB;
            blend.par = pit->second.get();
            blend.delayA = juce::jlimit (0, maxCompensation, latB - latA);
            blend.delayB = juce::jlimit (0, maxCompensation, latA - latB);
            plan.ops.push_back (blend);

            lat += juce::jmax (latA, latB);
        }

        return lat;
    }

    int Engine::rebuild (const Graph& graph)
    {
        // Effects for every uid in the graph (a new one, or one whose type changed).
        std::map<int, std::unique_ptr<BlockUnit>> gone;

        for (const auto& [uid, entry] : graph.blocks)
        {
            auto it = units.find (uid);

            if (it != units.end() && it->second->type == entry.type)
            {
                it->second->slot = entry.slot;
                continue;
            }

            const auto* info = registry::find (entry.type);

            if (info == nullptr || ! info->make)
                continue;

            auto u = std::make_unique<BlockUnit>();
            u->uid = uid;
            u->type = entry.type;
            u->info = info;
            u->slot = entry.slot;
            u->block = info->make();
            u->block->context = &ctx;

            for (size_t k = 0; k < info->knobs.size() && k < (size_t) maxKnobs; ++k)
                u->block->knob[k] = info->knobs[k].def;

            if (prepared)
                u->block->prepare (ctx.sampleRate, ctx.maxBlock);

            if (it != units.end())
            {
                gone[uid] = std::move (it->second);
                it->second = std::move (u);
            }
            else
            {
                units[uid] = std::move (u);
            }
        }

        for (auto it = units.begin(); it != units.end();)
        {
            if (graph.blocks.count (it->first) == 0)
            {
                gone[it->first] = std::move (it->second);
                it = units.erase (it);
            }
            else
            {
                ++it;
            }
        }

        // Parallels: one per parallel node.
        std::vector<int> parUids;
        std::function<void (const std::vector<Node>&)> collect = [&] (const std::vector<Node>& chain)
        {
            for (const auto& n : chain)
            {
                if (n.kind != Node::Kind::parallel)
                    continue;

                parUids.push_back (n.uid);

                auto pit = parallels.find (n.uid);

                if (pit == parallels.end())
                {
                    auto p = std::make_unique<ParUnit>();
                    p->uid = n.uid;

                    if (prepared)
                        p->prepare (ctx.sampleRate);

                    pit = parallels.emplace (n.uid, std::move (p)).first;
                }

                updateParallel (n.uid, n.settings);
                collect (n.a);
                collect (n.b);
            }
        };

        collect (graph.main);

        std::vector<std::unique_ptr<ParUnit>> goneParallels;

        for (auto it = parallels.begin(); it != parallels.end();)
        {
            if (std::find (parUids.begin(), parUids.end(), it->first) == parUids.end())
            {
                goneParallels.push_back (std::move (it->second));
                it = parallels.erase (it);
            }
            else
            {
                ++it;
            }
        }

        auto plan = std::make_unique<Plan>();
        int nextLevel = 1;
        plan->latency = compile (graph.main, 0, nextLevel, *plan, graph);
        plan->levels = juce::jmax (plan->levels, nextLevel);

        const int lat = plan->latency;
        handOver (std::move (plan));
        latency.store (lat);

        // The audio thread has left the old plan: what only it played can go now.
        gone.clear();
        goneParallels.clear();
        return lat;
    }

    void Engine::handOver (std::unique_ptr<Plan> next)
    {
        Plan* old = current.exchange (next.get());

        // Wait for the audio thread to leave the plan it's in (a buffer's time at most; it never
        // waits on us, so this can't lock up - and a host that stopped calling never holds one).
        const auto until = juce::Time::getMillisecondCounter() + 250;

        while (old != nullptr && inUse.load() == old && juce::Time::getMillisecondCounter() < until)
            std::this_thread::sleep_for (std::chrono::microseconds (200));

        live = std::move (next);
    }

    void Engine::updateParallel (int uid, const ParallelSettings& s)
    {
        auto it = parallels.find (uid);

        if (it == parallels.end())
            return;

        auto& p = *it->second;
        p.splitType.store (s.splitType);
        p.crossoverHz.store (s.crossoverHz);
        p.levelA.store (s.levelA);
        p.levelB.store (s.levelB);
        p.panA.store (s.panA);
        p.panB.store (s.panB);
        p.invertB.store (s.invertB);
    }

    //==========================================================================
    void Engine::runBlock (BlockUnit& u, float* l, float* r, int n) noexcept
    {
        const auto& src = slots[(size_t) juce::jlimit (0, maxBlocks - 1, u.slot)];
        const auto& specs = u.info->knobs;

        for (size_t k = 0; k < specs.size() && k < (size_t) maxKnobs; ++k)
            if (src.knobs[k] != nullptr)
                u.block->knob[k] = specs[k].toPlain (src.knobs[k]->load (std::memory_order_relaxed));

        const bool on = src.on == nullptr || src.on->load (std::memory_order_relaxed) > 0.5f;
        const float target = on ? 1.0f : 0.0f;
        const bool trails = u.info->trails;

        if (on)
        {
            u.ringing = trails;
            u.quietFor = 0;
        }

        // Off, faded out, and nothing ringing: the signal passes.
        if (! on && u.gain <= 0.0f && ! u.ringing)
        {
            u.sounding.store (false, std::memory_order_relaxed);
            return;
        }

        u.sounding.store (true, std::memory_order_relaxed);

        // Steady on: straight through.
        if (on && u.gain >= 1.0f)
        {
            u.block->process (l, r, n);
            guard (u, l, r, n);
            return;
        }

        float* dryL = scratch.getWritePointer (0);
        float* dryR = scratch.getWritePointer (1);
        float* ramp = scratch.getWritePointer (2);

        juce::FloatVectorOperations::copy (dryL, l, n);
        juce::FloatVectorOperations::copy (dryR, r, n);

        const float step = (float) (1.0 / (0.010 * ctx.sampleRate));
        float g = u.gain;

        for (int i = 0; i < n; ++i)
        {
            g = target > g ? juce::jmin (target, g + step) : juce::jmax (target, g - step);
            ramp[i] = g;
        }

        u.gain = g;

        if (trails)
        {
            // Its input fades; what it puts out is kept whole (its own dry and its tail), and the
            // dry fills in as it fades: switched off, the tail rings on over the dry.
            for (int i = 0; i < n; ++i) { l[i] *= ramp[i]; r[i] *= ramp[i]; }

            u.block->process (l, r, n);
            guard (u, l, r, n);

            float peak = 0.0f;

            for (int i = 0; i < n; ++i)
            {
                const float d = 1.0f - ramp[i];
                peak = juce::jmax (peak, std::abs (l[i]), std::abs (r[i]));
                l[i] += dryL[i] * d;
                r[i] += dryR[i] * d;
            }

            if (! on && g <= 0.0f)
            {
                u.quietFor = peak < 1.0e-5f ? u.quietFor + n : 0;

                if (u.quietFor > (int) (ctx.sampleRate * 1.5))
                {
                    u.ringing = false;
                    u.block->reset();
                }
            }

            return;
        }

        u.block->process (l, r, n);
        guard (u, l, r, n);

        for (int i = 0; i < n; ++i)
        {
            const float w = ramp[i], d = 1.0f - w;
            l[i] = l[i] * w + dryL[i] * d;
            r[i] = r[i] * w + dryR[i] * d;
        }

        if (g <= 0.0f)
            u.block->reset();   // off: it starts clean when it's switched on again
    }

    void Engine::guard (BlockUnit& u, float* l, float* r, int n) noexcept
    {
        // An effect that blew up (a filter pushed past its limits by an extreme setting): its
        // buffer silenced and its memory cleared, so the rest of the chain - and the ears - are safe.
        if (std::isfinite (l[n - 1]) && std::isfinite (r[n - 1]) && std::abs (l[n - 1]) < 1.0e4f && std::abs (r[n - 1]) < 1.0e4f)
            return;

        juce::FloatVectorOperations::clear (l, n);
        juce::FloatVectorOperations::clear (r, n);
        u.block->reset();
    }

    void Engine::process (float* l, float* r, int n, double bpm, bool playing) noexcept
    {
        Plan* plan = nullptr;

        // Take the plan, and say so before the message thread can let it go.
        do
        {
            plan = current.load();
            inUse.store (plan);
        }
        while (plan != current.load());

        if (plan == nullptr || plan->ops.empty() || n <= 0)
        {
            inUse.store (nullptr);
            return;
        }

        ctx.bpm = bpm > 0.0 ? bpm : 120.0;
        ctx.playing = playing;

        for (int start = 0; start < n; start += ctx.maxBlock)
        {
            const int count = juce::jmin (ctx.maxBlock, n - start);

            float* level0[2] { buffers.getWritePointer (0), buffers.getWritePointer (1) };
            juce::FloatVectorOperations::copy (level0[0], l + start, count);
            juce::FloatVectorOperations::copy (level0[1], r + start, count);

            for (const auto& op : plan->ops)
            {
                float* al = buffers.getWritePointer (2 * op.level);
                float* ar = buffers.getWritePointer (2 * op.level + 1);

                if (op.type == Op::Type::block)
                {
                    runBlock (*op.unit, al, ar, count);
                    continue;
                }

                float* bl = buffers.getWritePointer (2 * op.levelB);
                float* br = buffers.getWritePointer (2 * op.levelB + 1);
                auto& p = *op.par;

                if (op.type == Op::Type::fork)
                {
                    if (p.splitType.load (std::memory_order_relaxed) == 1)
                    {
                        const float hz = p.crossoverHz.load (std::memory_order_relaxed);

                        if (std::abs (hz - p.designedHz) > 0.5f)
                        {
                            for (int ch = 0; ch < 2; ++ch)
                                for (int s = 0; s < 2; ++s)
                                {
                                    p.lowA[ch][s].lowpass (ctx.sampleRate, hz);
                                    p.highB[ch][s].highpass (ctx.sampleRate, hz);
                                }

                            p.designedHz = hz;
                        }

                        for (int i = 0; i < count; ++i)
                        {
                            const float xl = al[i], xr = ar[i];
                            al[i] = p.lowA[0][1].run (p.lowA[0][0].run (xl));
                            ar[i] = p.lowA[1][1].run (p.lowA[1][0].run (xr));
                            bl[i] = p.highB[0][1].run (p.highB[0][0].run (xl));
                            br[i] = p.highB[1][1].run (p.highB[1][0].run (xr));
                        }
                    }
                    else
                    {
                        juce::FloatVectorOperations::copy (bl, al, count);
                        juce::FloatVectorOperations::copy (br, ar, count);
                    }

                    continue;
                }

                // Blend.
                float gAl, gAr, gBl, gBr;
                balance (p.panA.load (std::memory_order_relaxed), gAl, gAr);
                balance (p.panB.load (std::memory_order_relaxed), gBl, gBr);

                const float la = dbToGain (p.levelA.load (std::memory_order_relaxed));
                const float lb = dbToGain (p.levelB.load (std::memory_order_relaxed))
                                 * (p.invertB.load (std::memory_order_relaxed) ? -1.0f : 1.0f);
                gAl *= la; gAr *= la; gBl *= lb; gBr *= lb;

                for (int i = 0; i < count; ++i)
                {
                    const float a0 = op.delayA > 0 ? p.delay[0][0].push (al[i], op.delayA) : al[i];
                    const float a1 = op.delayA > 0 ? p.delay[0][1].push (ar[i], op.delayA) : ar[i];
                    const float b0 = op.delayB > 0 ? p.delay[1][0].push (bl[i], op.delayB) : bl[i];
                    const float b1 = op.delayB > 0 ? p.delay[1][1].push (br[i], op.delayB) : br[i];

                    al[i] = a0 * p.gain[0][0].next (gAl) + b0 * p.gain[1][0].next (gBl);
                    ar[i] = a1 * p.gain[0][1].next (gAr) + b1 * p.gain[1][1].next (gBr);
                }
            }

            juce::FloatVectorOperations::copy (l + start, level0[0], count);
            juce::FloatVectorOperations::copy (r + start, level0[1], count);
        }

        inUse.store (nullptr);
    }

    //==========================================================================
    void Engine::tick()
    {
        for (auto& [uid, u] : units)
        {
            const auto& src = slots[(size_t) juce::jlimit (0, maxBlocks - 1, u->slot)];
            const auto& specs = u->info->knobs;

            for (size_t k = 0; k < specs.size() && k < (size_t) maxKnobs; ++k)
                if (src.knobs[k] != nullptr)
                    u->block->uiKnob[k] = specs[k].toPlain (src.knobs[k]->load (std::memory_order_relaxed));

            u->block->messageTick();
        }
    }

    float Engine::meterOf (int uid) const
    {
        auto it = units.find (uid);
        return it != units.end() ? it->second->block->meter() : -1.0f;
    }

    bool Engine::isSounding (int uid) const
    {
        auto it = units.find (uid);
        return it != units.end() && it->second->sounding.load();
    }
}
