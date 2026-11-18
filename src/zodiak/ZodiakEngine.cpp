// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakEngine.cpp
#include "ZodiakEngine.h"

namespace zodiak
{
    namespace
    {
        float dbGain (float db) noexcept   { return db <= -59.5f ? 0.0f : std::pow (10.0f, db * 0.05f); }

        // MIDI channel (0-based) -> Zodiak channel, or -1: MIDI 1..8 play channels 1..8
        int channelForMidi (int midiCh) noexcept   { return midiCh >= 0 && midiCh < kNumChannels ? midiCh : -1; }
    }

    //==========================================================================
    juce::Optional<juce::AudioPlayHead::PositionInfo> Engine::PlayHead::getPosition() const
    {
        PositionInfo info;
        info.setBpm (now.bpm);
        info.setTimeSignature (TimeSignature { now.numerator, now.denominator });
        info.setPpqPosition (now.ppq);
        info.setPpqPositionOfLastBarStart (now.barStart);
        info.setIsPlaying (now.playing);
        info.setTimeInSeconds (now.ppq * 60.0 / (now.bpm > 1.0 ? now.bpm : 120.0));
        info.setTimeInSamples ((juce::int64) (now.ppq * 60.0 / (now.bpm > 1.0 ? now.bpm : 120.0) * now.sampleRate));
        return info;
    }

    //==========================================================================
    Engine::Engine()
    {
        for (int p = 0; p < kNumMasterParams; ++p)
            masterParams[(size_t) p].store (masterParamInfo (p).def);

        for (int ch = 0; ch < kNumChannels; ++ch)
        {
            graphs[(size_t) ch].makeDefault();
            resetMacros (ch);
            for (auto& s : sentNote[(size_t) ch]) s = -1;
            hostMidi[(size_t) ch].ensureSize (4096);
        }
    }

    Engine::~Engine()
    {
        // The plugins go first, while the rest they may still call is here
        for (auto& g : graphs) g.clear();
        masterFx.clearAll();
    }

    void Engine::prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        capacity = juce::jmax (64, maxBlockSize);

        for (int ch = 0; ch < kNumChannels; ++ch)
        {
            graphs[(size_t) ch].prepare (sr, capacity, &playHead);
            gainL[(size_t) ch] = gainR[(size_t) ch] = 0.0f;
            for (auto& s : sentNote[(size_t) ch]) s = -1;
            chOut[(size_t) ch].setSize (2, capacity, false, true, false);
            chOut[(size_t) ch].clear();
            lastLength[(size_t) ch] = 0;
        }
        masterFx.prepare (sr, capacity);
        mixBuf.setSize (2, capacity, false, true, false);
        sequencer.prepare (sr);
        playMidi.ensureSize (32768);
        alignLatency();
    }

    //==========================================================================
    void Engine::setNodePlugin (int ch, int uid, std::unique_ptr<rack::Hosted> plugin)
    {
        graph (ch).setPlugin (uid, std::move (plugin));
    }

    int Engine::addMasterPlugin (std::unique_ptr<rack::Hosted> plugin)
    {
        if (plugin == nullptr || plugin->instance == nullptr) return 0;
        plugin->instance->setPlayHead (&playHead);
        plugin->prepare (sr, capacity);
        return masterFx.addPlugin (std::move (plugin));
    }

    bool Engine::replaceMasterPlugin (int uid, std::unique_ptr<rack::Hosted> plugin)
    {
        if (plugin == nullptr || plugin->instance == nullptr) return false;
        plugin->instance->setPlayHead (&playHead);
        plugin->prepare (sr, capacity);
        return masterFx.replaceWithPlugin (uid, std::move (plugin));
    }

    int Engine::alignLatency()
    {
        int most = 0;
        for (auto& g : graphs) most = juce::jmax (most, g.latency());
        for (auto& g : graphs) g.setTailDelay (most - g.latency());
        return most;
    }

    //==========================================================================
    //  MACROS
    //==========================================================================
    void Engine::resetMacros (int ch)
    {
        auto& c = channel (ch);
        const auto d = macros::defaultsFor (graph (ch));
        for (int m = 0; m < kNumMacros; ++m)
        {
            c.macros[(size_t) m].dest.store (d[(size_t) m]);
            c.macros[(size_t) m].value.store (0.5f);
        }
    }

    float Engine::chanNorm (int ch, int p) const noexcept
    {
        const auto& info = chanParamInfo (p);
        return toNorm (channels[(size_t) juce::jlimit (0, kNumChannels - 1, ch)].chan[(size_t) p].load (std::memory_order_relaxed), info.min, info.max, info.skew);
    }

    void Engine::setChanNorm (int ch, int p, float n) noexcept
    {
        const auto& info = chanParamInfo (p);
        float x = fromNorm (n, info.min, info.max, info.skew);
        if (info.kind == pkStepped) x = std::round (x);
        if (info.kind == pkToggle)  x = n >= 0.5f ? 1.0f : 0.0f;
        channel (ch).chan[(size_t) p].store (x, std::memory_order_relaxed);
    }

    float Engine::nodeNormLocked (int ch, int dest) noexcept
    {
        auto* n = macros::nodeFor (graph (ch), dest);
        return n != nullptr ? n->getNorm (macros::paramOf (dest)) : 0.0f;
    }

    void Engine::setMacroLocked (int ch, int dest, float norm) noexcept
    {
        if (auto* n = macros::nodeFor (graph (ch), dest))
        {
            const int p = macros::paramOf (dest);
            const int places = n->places (p);
            if (places > 1) norm = std::round (norm * (float) (places - 1)) / (float) (places - 1);
            n->setNorm (p, norm);
        }
    }

    float Engine::macroNorm (int ch, int slot) noexcept
    {
        auto& mac = channel (ch).macros[(size_t) juce::jlimit (0, kNumMacros - 1, slot)];
        const int dest = mac.dest.load (std::memory_order_relaxed);
        if (macros::kindOf (dest) == macros::dkChan) return chanNorm (ch, macros::chanOf (dest));
        if (macros::kindOf (dest) == macros::dkNode && macros::nodeFor (graph (ch), dest) != nullptr) return nodeNormLocked (ch, dest);
        return mac.value.load (std::memory_order_relaxed);
    }

    void Engine::setMacroNorm (int ch, int slot, float norm) noexcept
    {
        norm = juce::jlimit (0.0f, 1.0f, norm);
        auto& mac = channel (ch).macros[(size_t) juce::jlimit (0, kNumMacros - 1, slot)];
        const int dest = mac.dest.load (std::memory_order_relaxed);
        if (macros::kindOf (dest) == macros::dkChan) { setChanNorm (ch, macros::chanOf (dest), norm); return; }

        if (macros::kindOf (dest) == macros::dkNode)
        {
            auto& g = graph (ch);
            if (juce::MessageManager::existsAndIsCurrentThread())
            {
                if (macros::nodeFor (g, dest) != nullptr) { setMacroLocked (ch, dest, norm); return; }
            }
            else if (g.lock.tryEnter())
            {
                const bool there = macros::nodeFor (g, dest) != nullptr;
                if (there) setMacroLocked (ch, dest, norm);
                g.lock.exit();
                if (there) return;
            }
            else
                return;     // the graph is being changed: this move is missed
        }
        mac.value.store (norm, std::memory_order_relaxed);
    }

    juce::String Engine::macroText (int ch, int slot)
    {
        auto& mac = channel (ch).macros[(size_t) juce::jlimit (0, kNumMacros - 1, slot)];
        const int dest = mac.dest.load();
        auto& g = graph (ch);
        if (macros::kindOf (dest) == macros::dkChan)
            return macros::valueText (g, dest, channel (ch).chan[(size_t) macros::chanOf (dest)].load());
        if (auto* n = macros::nodeFor (g, dest))
            return n->text (macros::paramOf (dest), n->get (macros::paramOf (dest)));
        return juce::String ((int) std::lround (mac.value.load() * 100.0f)) + "%";
    }

    //==========================================================================
    void Engine::audition (int ch, int note, int velocity) noexcept
    {
        const int w = qWrite.load (std::memory_order_relaxed), next = (w + 1) % kQueue;
        if (next == qRead.load (std::memory_order_acquire)) return;
        queue[(size_t) w].store (((ch & 0xFF) << 16) | ((note & 0x7F) << 8) | juce::jlimit (0, 127, velocity), std::memory_order_relaxed);
        qWrite.store (next, std::memory_order_release);
    }

    //==========================================================================
    //  PITCH: transpose and octave, kept per held key so its note-off finds it
    //==========================================================================
    int Engine::sendNote (int ch, int note, int velocity) noexcept
    {
        if (velocity > 0)
        {
            const auto& st = channels[(size_t) ch];
            const int tr = (int) std::lround (st.chan[cpTranspose].load (std::memory_order_relaxed))
                         + 12 * (int) std::lround (juce::jlimit (-3.0f, 3.0f, st.chan[cpOctave].load (std::memory_order_relaxed)));
            const int sent = juce::jlimit (0, 127, note + tr);
            sentNote[(size_t) ch][(size_t) note] = (int8_t) sent;
            return sent;
        }

        int sent = sentNote[(size_t) ch][(size_t) note];
        if (sent < 0) sent = note;
        sentNote[(size_t) ch][(size_t) note] = -1;
        return sent;
    }

    // LOW VELOCITY RESPONSE (Grex's VelCurve): out = 127 x (in / 127) ^ gamma, gamma =
    // 2.5 ^ -(amount / 100).  0 is off; 100 lifts velocity 20 to 60 and 64 to 96.
    int Engine::liftVelocity (int velocity) const noexcept
    {
        const float amount = juce::jlimit (0.0f, 100.0f, masterParams[mpLowVel].load (std::memory_order_relaxed)) / 100.0f;
        if (velocity <= 0 || amount < 1.0e-3f) return velocity;
        const float gamma = std::pow (2.5f, -amount);
        return juce::jlimit (1, 127, (int) std::lround (std::pow ((float) velocity / 127.0f, gamma) * 127.0f));
    }

    // A message for a channel: on its MIDI channel 1, the notes moved by its PITCH
    void Engine::hostedMidi (int ch, const juce::MidiMessage& m, int frame) noexcept
    {
        auto& out = hostMidi[(size_t) ch];
        auto& st = channels[(size_t) ch];

        if (m.isNoteOn())
        {
            st.notes.fetch_add (1, std::memory_order_relaxed);
            graphs[(size_t) ch].noteOn();
            const auto vel = (juce::uint8) liftVelocity (m.getVelocity());
            out.addEvent (juce::MidiMessage::noteOn (1, sendNote (ch, m.getNoteNumber(), vel), vel), frame);
        }
        else if (m.isNoteOff())
            out.addEvent (juce::MidiMessage::noteOff (1, sendNote (ch, m.getNoteNumber(), 0), m.getVelocity()), frame);
        else if (m.isController() || m.isPitchWheel() || m.isChannelPressure() || m.isAftertouch() || m.isProgramChange())
        {
            auto copy = m;
            copy.setChannel (1);
            out.addEvent (copy, frame);
        }
    }

    //==========================================================================
    //  MIDI LINK (ZodiakLink.h): a source's move onto its control
    //==========================================================================
    bool Engine::linkMidi (const juce::MidiMessage& m) noexcept
    {
        int kind = link::srcNone, number = 0, value = 0;
        bool press = false, release = false;
        if (m.isController())
        {
            number = m.getControllerNumber();
            if (number >= 120) return false;                     // the channel mode messages: never a source
            kind = link::srcCc;
            value = m.getControllerValue();
        }
        else if (m.isNoteOn())  { kind = link::srcNote; number = m.getNoteNumber(); value = 127; press = true; }
        else if (m.isNoteOff()) { kind = link::srcNote; number = m.getNoteNumber(); release = true; }
        else return false;

        const int midiCh = juce::jlimit (0, 15, m.getChannel() - 1);
        if (kind == link::srcCc || press)
            lastMidiWord.store (link::packLast (++lastMidiCount, kind, midiCh, number), std::memory_order_relaxed);

        const int key = (kind << 12) | (midiCh << 8) | number;

        if ((kind == link::srcCc || press) && learnArmed.load (std::memory_order_relaxed))
        {
            learnArmed.store (false);
            learnHit.store (key);
            return true;
        }

        bool linked = false;
        for (int i = 0; i < link::Map::kMaxLinks; ++i)
        {
            auto& sl = linkMap.slot (i);
            if (sl.source.load (std::memory_order_acquire) != key) continue;
            linked = true;

            const int mode = sl.mode.load (std::memory_order_relaxed);
            int ev = link::kEvNone;
            if (kind == link::srcCc)
            {
                const int prev = sl.last.exchange (value, std::memory_order_relaxed);
                const bool up = value >= 64 && prev < 64, down = value < 64 && prev >= 64;
                if (mode == link::kKnob)        ev = value;
                else if (mode == link::kToggle) ev = up ? link::kEvFlip : link::kEvNone;
                else                            ev = up ? link::kEvPress : down ? link::kEvRelease : link::kEvNone;
            }
            else
            {
                if (mode == link::kKnob)        ev = press ? 127 : link::kEvNone;
                else if (mode == link::kToggle) ev = press ? link::kEvFlip : link::kEvNone;
                else                            ev = press ? link::kEvPress : (release ? link::kEvRelease : link::kEvNone);
            }
            if (ev == link::kEvNone) continue;

            const int targetKind = sl.kind.load (std::memory_order_relaxed);
            if (link::isDeferred (targetKind))
                sl.pending.store (ev, std::memory_order_relaxed);
            else
                applyLinkNow (targetKind, sl.ch.load (std::memory_order_relaxed), sl.a.load (std::memory_order_relaxed),
                              sl.b.load (std::memory_order_relaxed), ev);
        }
        return linked;
    }

    void Engine::applyLinkNow (int kind, int ch, int a, int b, int ev) noexcept
    {
        ch = juce::jlimit (0, kNumChannels - 1, ch);
        auto& st = channels[(size_t) ch];

        float cur = 0.0f;
        int places = 0;
        const ParamInfo* info = nullptr;
        std::atomic<float>* value = nullptr;
        graph::Node* node = nullptr;
        auto& g = graphs[(size_t) ch];
        bool locked = false;
        int macroDest = 0;

        switch (kind)
        {
            case link::kMaster:
                if (a < 0 || a >= kNumMasterParams) return;
                info = &masterParamInfo (a);
                value = &masterParams[(size_t) a];
                break;
            case link::kChan:
                if (a < 0 || a >= kNumChanParams) return;
                info = &chanParamInfo (a);
                value = &st.chan[(size_t) a];
                break;
            case link::kMacro:
            {
                if (a < 0 || a >= kNumMacros || ! g.lock.tryEnter()) return;
                locked = true;
                macroDest = st.macros[(size_t) a].dest.load (std::memory_order_relaxed);
                const int steps = macros::stepsOf (g, macroDest);
                places = steps > 0 ? steps + 1 : 0;
                if (macros::kindOf (macroDest) == macros::dkChan)   cur = chanNorm (ch, macros::chanOf (macroDest));
                else if (macros::nodeFor (g, macroDest) != nullptr) cur = nodeNormLocked (ch, macroDest);
                else                                                cur = st.macros[(size_t) a].value.load (std::memory_order_relaxed);
                break;
            }
            case link::kNode:
            case link::kNodeOn:
            {
                if (! g.lock.tryEnter()) return;
                locked = true;
                node = g.find (a);
                if (node == nullptr || (kind == link::kNode && (b < 0 || b >= node->numParams() || node->isMask (b))))
                {
                    g.lock.exit();
                    return;
                }
                if (kind == link::kNode) { places = node->places (b); cur = node->getNorm (b); }
                else                     { places = 2; cur = node->on.load() ? 1.0f : 0.0f; }
                break;
            }
            case link::kBank:
                places = 4;
                cur = (float) (sequencer.requested (ch) / 8) / 3.0f;
                break;
            case link::kVar:
                places = 8;
                cur = (float) (sequencer.requested (ch) % 8) / 7.0f;
                break;
            case link::kTransport:
                places = 2;
                cur = (a == link::tPlayStop && ownPlaying.load (std::memory_order_relaxed)) ? 1.0f : 0.0f;
                break;
            default:
                return;
        }

        float lo = 0.0f, hi = 1.0f, skew = 1.0f;
        if (info != nullptr)
        {
            lo = info->min; hi = info->max; skew = info->skew;
            places = link::paramPlaces (*info);
            cur = toNorm (value->load (std::memory_order_relaxed), lo, hi, skew);
        }

        float n = -1.0f;
        if (ev >= 0 && ev <= 127)        n = link::knobNorm (ev, places);
        else if (ev == link::kEvFlip)    n = cur >= 0.5f ? 0.0f : 1.0f;
        else if (ev == link::kEvPress)   n = 1.0f;
        else if (ev == link::kEvRelease) n = 0.0f;

        if (n >= 0.0f)
            switch (kind)
            {
                case link::kMaster:
                case link::kChan:
                {
                    float x = fromNorm (n, lo, hi, skew);
                    if (info->kind == pkToggle) x = n >= 0.5f ? hi : lo;
                    else if (places > 0)        x = std::round (x);
                    value->store (juce::jlimit (juce::jmin (lo, hi), juce::jmax (lo, hi), x), std::memory_order_relaxed);
                    break;
                }
                case link::kMacro:
                    if (macros::kindOf (macroDest) == macros::dkChan)   setChanNorm (ch, macros::chanOf (macroDest), n);
                    else if (macros::nodeFor (g, macroDest) != nullptr) setMacroLocked (ch, macroDest, n);
                    else                                                st.macros[(size_t) a].value.store (n, std::memory_order_relaxed);
                    break;
                case link::kNode:
                    if (places > 1) n = std::round (n * (float) (places - 1)) / (float) (places - 1);
                    node->setNorm (b, n);
                    break;
                case link::kNodeOn:
                    node->on.store (n >= 0.5f);
                    break;
                case link::kBank:
                case link::kVar:
                {
                    const int now = sequencer.requested (ch);
                    int bk = now / 8, v = now % 8;
                    if (kind == link::kBank) bk = juce::jlimit (0, 3, (int) std::lround (n * 3.0f));
                    else                     v = juce::jlimit (0, 7, (int) std::lround (n * 7.0f));
                    if (bk * 8 + v != now) sequencer.select (ch, bk * 8 + v);
                    break;
                }
                case link::kTransport:
                    if (a == link::tRewind)    { if (n >= 0.5f) rewind(); }
                    else if (a == link::tStop) { if (n >= 0.5f) stop(); }
                    else if (n >= 0.5f)        play();
                    else                       stop();
                    break;
                default:
                    break;
            }

        if (locked) g.lock.exit();
    }

    //==========================================================================
    void Engine::silenceAll() noexcept
    {
        for (int ch = 0; ch < kNumChannels; ++ch)
        {
            auto& g = graphs[(size_t) ch];
            if (g.lock.tryEnter()) { g.silence(); g.lock.exit(); }
            for (auto& s : sentNote[(size_t) ch]) s = -1;
            chOut[(size_t) ch].clear();
        }
    }

    //==========================================================================
    void Engine::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, const Transport& transport) noexcept
    {
        const int numSamples = buffer.getNumSamples();
        if (numSamples <= 0 || buffer.getNumChannels() < 2 || chOut[0].getNumSamples() < capacity) { buffer.clear(); return; }

        if (silenceRequested.exchange (false)) silenceAll();

        // The transport Zodiak plays to: the DAW's, or its own clock
        Transport tr = transport;
        if (transport.bpm > 1.0) dawBpm = transport.bpm;
        const bool followDaw = followsDaw();
        if (rewindRequested.exchange (false)) ownPpq = 0.0;

        if (followDaw && transport.playing)
            ownPpq = transport.ppq;
        else
        {
            tr.bpm = followDaw ? dawBpm : (double) juce::jlimit (40.0f, 240.0f, master (mpTempo).load (std::memory_order_relaxed));
            tr.playing = ownPlaying.load (std::memory_order_relaxed);
            tr.ppq = ownPpq;
            const double bar = (double) tr.numerator * 4.0 / (double) juce::jmax (1, tr.denominator);
            tr.barStart = std::floor (ownPpq / bar) * bar;
            if (tr.playing) ownPpq += tr.beatsPerSample() * numSamples;
        }

        shownPlaying.store (tr.playing, std::memory_order_relaxed);
        shownPpq.store (tr.ppq, std::memory_order_relaxed);
        shownBpm.store (tr.bpm, std::memory_order_relaxed);
        shownBeatsPerBar.store (juce::jmax (1, tr.numerator), std::memory_order_relaxed);
        shownStep.store (tr.playing ? (int) (std::floor (std::max (0.0, tr.ppq) * 4.0)) % 16 : -1, std::memory_order_relaxed);

        float* outL = buffer.getWritePointer (0);
        float* outR = buffer.getWritePointer (1);

        // The MIDI that came in (a source in the MIDI LINK map moves its control and
        // goes no further), and the patterns' notes
        playMidi.clear();
        for (const auto meta : midi)
        {
            if (meta.samplePosition >= numSamples) break;
            const auto msg = meta.getMessage();
            if (! linkMidi (msg)) playMidi.addEvent (msg, juce::jmax (0, meta.samplePosition));
        }
        sequencer.render (playMidi, tr, numSamples);

        for (int start = 0; start < numSamples; start += capacity)
        {
            const int n = std::min (capacity, numSamples - start);
            processSegment (outL + start, outR + start, n, playMidi, start, tr.advancedBy (start));
        }

        for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
            buffer.clear (ch, 0, numSamples);
    }

    void Engine::processSegment (float* outL, float* outR, int n, const juce::MidiBuffer& midi, int midiOffset, const Transport& tr) noexcept
    {
        playHead.now = tr;

        //----------------------------------------------------------------------
        //  1. Every channel's notes for this part: the windows' auditions, then
        //     the MIDI (the patterns' and the DAW's)
        //----------------------------------------------------------------------
        for (auto& m : hostMidi) m.clear();

        for (int r = qRead.load (std::memory_order_relaxed); r != qWrite.load (std::memory_order_acquire); r = (r + 1) % kQueue)
        {
            const int ev = queue[(size_t) r].load (std::memory_order_relaxed);
            qRead.store ((r + 1) % kQueue, std::memory_order_release);
            const int ch = (ev >> 16) & 0xFF;
            if (ch >= kNumChannels) continue;
            const int note = (ev >> 8) & 0x7F, vel = ev & 0x7F;
            hostedMidi (ch, vel > 0 ? juce::MidiMessage::noteOn (1, note, (juce::uint8) vel) : juce::MidiMessage::noteOff (1, note), 0);
        }

        for (const auto meta : midi)
        {
            if (meta.samplePosition < midiOffset) continue;
            if (meta.samplePosition >= midiOffset + n) break;
            const auto m = meta.getMessage();
            const int ch = channelForMidi (m.getChannel() - 1);
            if (ch >= 0) hostedMidi (ch, m, meta.samplePosition - midiOffset);
        }

        //----------------------------------------------------------------------
        //  2. The graphs, held for this part (one being changed plays silence),
        //     in the order the Sidechain In nodes need
        //----------------------------------------------------------------------
        std::array<int, kNumChannels> keys {};
        for (int ch = 0; ch < kNumChannels; ++ch)
        {
            held[(size_t) ch] = graphs[(size_t) ch].lock.tryEnter();
            keys[(size_t) ch] = held[(size_t) ch] ? graphs[(size_t) ch].keySources() & ~(1 << ch) : 0;
        }

        std::array<int, kNumChannels> order {};
        int done = 0, doneMask = 0;
        while (done < kNumChannels)
        {
            int pick = -1;
            for (int ch = 0; ch < kNumChannels && pick < 0; ++ch)
                if ((doneMask >> ch & 1) == 0 && (keys[(size_t) ch] & ~doneMask) == 0) pick = ch;
            if (pick < 0)          // a ring: the first left plays, and hears the last block of the rest
                for (int ch = 0; ch < kNumChannels && pick < 0; ++ch)
                    if ((doneMask >> ch & 1) == 0) pick = ch;
            order[(size_t) done++] = pick;
            doneMask |= 1 << pick;
        }

        graph::Taps taps;
        for (int ch = 0; ch < kNumChannels; ++ch)
        {
            taps.L[(size_t) ch] = chOut[(size_t) ch].getReadPointer (0);
            taps.R[(size_t) ch] = chOut[(size_t) ch].getReadPointer (1);
            taps.length[(size_t) ch] = lastLength[(size_t) ch];
        }

        for (int ch : order)
        {
            float* L = chOut[(size_t) ch].getWritePointer (0);
            float* R = chOut[(size_t) ch].getWritePointer (1);
            if (held[(size_t) ch]) graphs[(size_t) ch].render (n, hostMidi[(size_t) ch], tr, taps, L, R);
            else { std::fill_n (L, n, 0.0f); std::fill_n (R, n, 0.0f); }
            taps.length[(size_t) ch] = n;
            lastLength[(size_t) ch] = n;
        }

        for (int ch = 0; ch < kNumChannels; ++ch)
            if (held[(size_t) ch]) graphs[(size_t) ch].lock.exit();

        //----------------------------------------------------------------------
        //  3. The faders into the mix
        //----------------------------------------------------------------------
        bool anySolo = false;
        for (auto& c : channels) anySolo = anySolo || c.chan[cpSolo].load (std::memory_order_relaxed) > 0.5f;

        float* mixL = mixBuf.getWritePointer (0);
        float* mixR = mixBuf.getWritePointer (1);
        std::fill_n (mixL, n, 0.0f);
        std::fill_n (mixR, n, 0.0f);

        for (int ch = 0; ch < kNumChannels; ++ch)
        {
            auto& st = channels[(size_t) ch];
            const bool audible = st.chan[cpMute].load (std::memory_order_relaxed) < 0.5f && (! anySolo || st.chan[cpSolo].load (std::memory_order_relaxed) > 0.5f);
            const float vol = audible ? dbGain (st.chan[cpVolume].load (std::memory_order_relaxed)) : 0.0f;
            const float pan = juce::jlimit (-1.0f, 1.0f, st.chan[cpPan].load (std::memory_order_relaxed));
            const float tL = vol * std::cos ((pan + 1.0f) * 0.25f * fx::kPi) * 1.41421356f;
            const float tR = vol * std::sin ((pan + 1.0f) * 0.25f * fx::kPi) * 1.41421356f;

            const float* L = chOut[(size_t) ch].getReadPointer (0);
            const float* R = chOut[(size_t) ch].getReadPointer (1);
            float& gl = gainL[(size_t) ch];
            float& gr = gainR[(size_t) ch];
            const float dl = (tL - gl) / (float) n, dr = (tR - gr) / (float) n;
            float pkL = 0.0f, pkR = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                gl += dl;
                gr += dr;
                const float l = L[i] * gl, r = R[i] * gr;
                mixL[i] += l;
                mixR[i] += r;
                pkL = std::max (pkL, std::abs (l));
                pkR = std::max (pkR, std::abs (r));
            }
            gl = tL;
            gr = tR;

            if (pkL > st.peakL.load (std::memory_order_relaxed)) st.peakL.store (pkL, std::memory_order_relaxed);
            if (pkR > st.peakR.load (std::memory_order_relaxed)) st.peakR.store (pkR, std::memory_order_relaxed);
            const float pk = std::max (pkL, pkR);
            if (pk > st.peak.load (std::memory_order_relaxed)) st.peak.store (pk, std::memory_order_relaxed);
        }

        //----------------------------------------------------------------------
        //  4. The master's VST FX, the master
        //----------------------------------------------------------------------
        if (masterFx.lock.tryEnter())
        {
            for (auto& ins : masterFx.fx)
            {
                if (! ins->on.load (std::memory_order_relaxed)) continue;
                if (ins->builtIn != nullptr)     ins->builtIn->process (mixL, mixR, n);
                else if (ins->plugin != nullptr) ins->plugin->play (mixL, mixR, n, ins->plugin->noMidi, false);
            }
            masterFx.lock.exit();
        }

        const bool masterMuted = master (mpMute).load (std::memory_order_relaxed) > 0.5f;
        const float mv = masterMuted ? 0.0f : dbGain (master (mpVolume).load (std::memory_order_relaxed));
        const float bal = juce::jlimit (-1.0f, 1.0f, master (mpPan).load (std::memory_order_relaxed));
        const float mlT = mv * (bal > 0.0f ? 1.0f - bal : 1.0f);
        const float mrT = mv * (bal < 0.0f ? 1.0f + bal : 1.0f);
        const float dml = (mlT - masterL) / (float) n, dmr = (mrT - masterR) / (float) n;
        float pkL = 0.0f, pkR = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            masterL += dml;
            masterR += dmr;
            outL[i] = mixL[i] * masterL;
            outR[i] = mixR[i] * masterR;
            pkL = std::max (pkL, std::abs (outL[i]));
            pkR = std::max (pkR, std::abs (outR[i]));
        }
        masterL = mlT;
        masterR = mrT;
        if (pkL > masterPeakL.load (std::memory_order_relaxed)) masterPeakL.store (pkL, std::memory_order_relaxed);
        if (pkR > masterPeakR.load (std::memory_order_relaxed)) masterPeakR.store (pkR, std::memory_order_relaxed);
    }
}
