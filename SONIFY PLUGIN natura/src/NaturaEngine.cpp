// C:\workspace\SONIFY PLUGIN natura\src\NaturaEngine.cpp
#include "NaturaEngine.h"

namespace natura
{
    namespace
    {
        // Grex's velocity curve: one bipolar number, -1 soft .. 0 linear .. +1 hard,
        // symmetric in log space.  0 and 127 stay fixed points.
        int applyVelCurve (int velocity, float curve)
        {
            if (velocity <= 0 || std::abs (curve) < 1.0e-3f) return velocity;
            const float gamma = std::pow (2.5f, -juce::jlimit (-1.0f, 1.0f, curve));
            const float out = std::pow ((float) velocity / 127.0f, gamma) * 127.0f;
            return juce::jlimit (1, 127, (int) std::lround (out));
        }

        Dynamics dynamicsFor (Archetype a)
        {
            switch (a)
            {
                case Archetype::Tonewheel: case Archetype::PipeOrgan:      return Dynamics::Swell;
                case Archetype::FreeReed:                                  return Dynamics::Direct;
                case Archetype::BowedString: case Archetype::LipReed:
                case Archetype::SingleReed:  case Archetype::DoubleReed:
                case Archetype::AirJet:                                    return Dynamics::Physical;
                default:                                                   return Dynamics::Struck;
            }
        }
    }

    NaturaEngine::NaturaEngine() = default;

    void NaturaEngine::prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        for (auto& v : voices) v.prepare (sr);
        fx.prepare (sr);
        bufL.assign ((size_t) std::max (16, maxBlock), 0.0f);
        bufR.assign ((size_t) std::max (16, maxBlock), 0.0f);
        pedalLp.setCutoff (380.0, sr);
        pnz.seed (0x77u);
        reset();
    }

    void NaturaEngine::reset()
    {
        for (auto& v : voices) v.kill();
        fx.reset();
        heldCount = 0;
        monoNote = -1;
        live = {};
        mwT = atT = 0.0f; breathT = exprT = 1.0f; bendT = 0.0f; bendRaw = 8192;
        sostenuto = ccSostenuto = artSostenuto = false;
        motorState = vibState = percState = 0;
        regOffset = 0;
        lastArchetype = -1;
        lastPanelArt = -1;
        lastRotParam = -1;
        havePitch = false;
    }

    int NaturaEngine::maxVoices() const
    {
        return juce::jlimit (1, kMaxVoices, (int) std::lround (snap[P::voices]));
    }

    //==========================================================================
    void NaturaEngine::buildSnapshot (const float* values)
    {
        std::copy (values, values + kNumParams, snap.v);
        const int a = juce::jlimit (0, kNumArchetypes - 1, (int) std::lround (values[(int) P::archetype]));

        if (a != lastArchetype)
        {
            // a new instrument: nothing of the old one may keep sounding through it
            for (auto& v : voices) v.kill();
            heldCount = 0;
            monoNote = -1;
            motorState = vibState = percState = 0;
            regOffset = 0;
            artSostenuto = false;
            sostenuto = ccSostenuto;
            lastArchetype = a;
            lastPanelArt = -1;
            fx.reset();
        }
        snap.arch = (Archetype) a;
        snap.dynMode = dynamicsFor (snap.arch);
        snap.sustained = snap.dynMode != Dynamics::Struck;

        const auto& sl = specsFor (snap.arch);
        for (int i = 0; i < kMaxSpec; ++i)
            snap.spec[i] = i < sl.count ? toSpec (sl.ids[i], values[(int) sl.ids[i]]) : 0.0f;
        if (snap.arch == Archetype::Tonewheel)
        {
            if (percState == 1 && snap.spec[11] < 0.25f) snap.spec[11] = 0.5f;
            if (percState == 2) snap.spec[11] = 0.0f;
        }
        snap.regOffset = regOffset;

        auto& o = snap.osc;
        auto V = [values] (P p) { return values[(int) p]; };
        o.wave[0]  = (int) std::lround (V (P::s1_wave));  o.wave[1]  = (int) std::lround (V (P::s2_wave));
        o.oct[0]   = (int) std::lround (V (P::s1_oct));   o.oct[1]   = (int) std::lround (V (P::s2_oct));
        o.semi[0]  = (int) std::lround (V (P::s1_semi));  o.semi[1]  = (int) std::lround (V (P::s2_semi));
        o.fine[0]  = V (P::s1_fine);                      o.fine[1]  = V (P::s2_fine);
        o.pw[0]    = V (P::s1_pw) / 100.0f;               o.pw[1]    = V (P::s2_pw) / 100.0f;
        o.level[0] = V (P::s1_level) / 100.0f;            o.level[1] = V (P::s2_level) / 100.0f;
        o.fm = V (P::sy_fm) / 100.0f;  o.ring = V (P::sy_ring) / 100.0f;  o.sync = V (P::sy_sync) >= 0.5f;
        o.sub = V (P::sy_sub) / 100.0f;  o.noise = V (P::sy_noise) / 100.0f;  o.drive = V (P::sy_drive) / 100.0f;
        o.cutoff = V (P::sy_cutoff) / 100.0f;  o.reso = V (P::sy_reso) / 100.0f;  o.keyTrack = V (P::sy_key) / 100.0f;
        o.envAmt = V (P::sy_env) / 100.0f;  o.velAmt = V (P::sy_vel) / 100.0f;
        o.envA = V (P::sy_a) / 100.0f;  o.envD = V (P::sy_d) / 100.0f;  o.envS = V (P::sy_s) / 100.0f;  o.envR = V (P::sy_r) / 100.0f;
        o.outLevel = V (P::sy_level) / 100.0f;

        // the panel chose an articulation (or automation moved it)
        const int panelArt = (int) std::lround (V (P::art_select));
        if (panelArt != lastPanelArt)
        {
            lastPanelArt = panelArt;
            selectArt (panelArt, true);
        }
        const int forced = forcedArt.exchange (-1);
        if (forced >= 0) selectArt (forced, true);

        const int rotParam = V (P::rot_fast) >= 0.5f ? 1 : 0;
        if (rotParam != lastRotParam) { lastRotParam = rotParam; rotorFast = rotParam == 1; }
    }

    //==========================================================================
    void NaturaEngine::selectArt (int index, bool)
    {
        const auto& list = artsFor (snap.arch);
        index = juce::jlimit (0, list.count - 1, index);
        const Art a = list.arts[index];

        if (artSostenuto && a != Art::Sostenuto)
        {
            artSostenuto = false;
            sostenuto = ccSostenuto;
            if (! sostenuto)
            {
                for (auto& v : voices) v.sostenutoHeld = false;
                pedalChanged();
            }
        }
        if (isStateArt (a)) applyStateArt (a);
        else
        {
            artIndex = index;
            if (a == Art::Normal && snap.arch == Archetype::FreeReed) regOffset = 0;
        }
        currentArt.store (index);
    }

    void NaturaEngine::applyStateArt (Art a)
    {
        switch (a)
        {
            case Art::MotorOn:      motorState = 1; break;
            case Art::MotorOff:     motorState = 2; break;
            case Art::RotorSlow:    rotorFast = false; break;
            case Art::RotorFast:    rotorFast = true; break;
            case Art::PercOn:       percState = 1; break;
            case Art::PercOff:      percState = 2; break;
            case Art::VibOn:        vibState = 1; break;
            case Art::VibOff:       vibState = 2; break;
            case Art::RegisterUp:   regOffset = 1; break;
            case Art::RegisterDown: regOffset = -1; break;
            case Art::Sostenuto:
                artSostenuto = true;
                sostenuto = true;
                for (auto& v : voices) if (v.isKeyDown()) v.sostenutoHeld = true;
                break;
            default: break;
        }
    }

    //==========================================================================
    bool NaturaEngine::decideAttackGlide (int rawVelocity)
    {
        const int mode = (int) std::lround (snap[P::ag_mode]);
        switch (mode)
        {
            case 1: return true;
            case 2:
            {
                const int n = juce::jlimit (2, 16, (int) std::lround (snap[P::ag_every]));
                if (++glideCounter >= n) { glideCounter = 0; return true; }
                return false;
            }
            case 3:
            {
                glideRng ^= glideRng << 13; glideRng ^= glideRng >> 17; glideRng ^= glideRng << 5;
                const float u = (float) (glideRng & 0xFFFFFF) / 16777216.0f;
                return u * (float) juce::jlimit (2, 16, (int) std::lround (snap[P::ag_odds])) < 1.0f;
            }
            case 4: return rawVelocity >= (int) std::lround (snap[P::ag_vel]);
            default: glideCounter = 0; return false;
        }
    }

    Art NaturaEngine::noteArt (int velocity, bool overlapping, double now)
    {
        const auto arch = snap.arch;
        Art a = artAt (arch, artIndex);
        if (isStateArt (a)) a = Art::Normal;

        if (snap[P::art_vel_on] >= 0.5f)
        {
            const int idx = velocity < (int) snap[P::art_vel_split] ? (int) snap[P::art_vel_lo] : (int) snap[P::art_vel_hi];
            const Art z = artAt (arch, idx);
            if (! isStateArt (z)) a = z;
        }
        if (snap[P::art_auto_on] >= 0.5f)
        {
            if (overlapping)
            {
                for (Art want : { Art::Legato, Art::HammerOn, Art::GlideLegato })
                    if (findArt (arch, want) >= 0) { a = want; break; }
            }
            else if ((now - lastNoteTime) * 1000.0 < snap[P::art_auto_ms])
            {
                for (Art want : { Art::Staccato, Art::Spiccato })
                    if (findArt (arch, want) >= 0) { a = want; break; }
            }
        }
        return a;
    }

    NoteStart NaturaEngine::makeStart (int soundingNote, int velocity, Art art, bool)
    {
        NoteStart ns;
        ns.note = soundingNote;
        ns.rawVelocity = velocity;
        const int curved = applyVelCurve (velocity, snap[P::vel_curve] / 100.0f);
        ns.velocity = (float) curved / 127.0f;
        ns.art = art;
        ns.attackGlide = decideAttackGlide (velocity);
        ns.percTrigger = heldCount <= 1;
        noteSeed = noteSeed * 1664525u + 1013904223u;
        ns.seed = noteSeed;
        return ns;
    }

    int NaturaEngine::findVoice()
    {
        const int maxV = maxVoices();
        for (int i = 0; i < maxV; ++i) if (! voices[i].isActive()) return i;
        int best = -1; long bestAge = 0;
        for (int i = 0; i < maxV; ++i)
            if (voices[i].isLetGo() && (best < 0 || voices[i].getAge() < bestAge)) { best = i; bestAge = voices[i].getAge(); }
        if (best >= 0) return best;
        for (int i = 0; i < maxV; ++i)
            if (best < 0 || voices[i].getAge() < bestAge) { best = i; bestAge = voices[i].getAge(); }
        return std::max (0, best);
    }

    //==========================================================================
    void NaturaEngine::noteOn (int note, int velocity)
    {
        const auto& list = artsFor (snap.arch);

        // keyswitches never sound
        const int ksBase = (int) std::lround (snap[P::art_ks_base]);
        if (snap[P::art_ks_on] >= 0.5f && note >= ksBase && note < ksBase + list.count)
        {
            selectArt (note - ksBase, false);
            return;
        }
        if (snap[P::range_on] >= 0.5f && (note < (int) snap[P::range_lo] || note > (int) snap[P::range_hi])) return;

        const int sounding = note + (int) std::lround (snap[P::octave]) * 12;
        if (sounding < 0 || sounding > 127) return;

        const bool overlapping = heldCount > 0;
        const Art art = noteArt (velocity, overlapping, nowSec);
        const ArtFx afx = artFx (art, snap.arch);

        // remember the key
        if (heldCount < 16) { held[heldCount] = note; heldVel[heldCount] = velocity; ++heldCount; }

        NoteStart ns = makeStart (sounding, velocity, art, overlapping);
        const int mode = (int) std::lround (snap[P::voice_mode]);

        if (mode != 0)
        {
            auto& v = voices[0];
            for (int i = 1; i < kMaxVoices; ++i) if (voices[i].isActive()) voices[i].fadeOut();
            if (v.isActive() && monoNote >= 0 && overlapping)
            {
                const bool legato = (mode == 1 || afx.forceLegato) && ! afx.forceRetrig;
                v.moveTo (ns, snap, legato, ++ageCounter);
            }
            else
            {
                if (afx.forceLegato && havePitch)
                {
                    ns.glideFromSemis = lastPitchSemis - (float) sounding;
                    ns.glideMs = law::portaMs (snap[P::port]);
                }
                v.start (ns, snap, ++ageCounter);
            }
            monoNote = sounding;
        }
        else
        {
            if (afx.forceLegato && havePitch && overlapping)
            {
                ns.glideFromSemis = lastPitchSemis - (float) sounding;
                ns.glideMs = std::max (40.0f, law::portaMs (snap[P::port]));
            }
            int idx = -1;
            for (int i = 0; i < maxVoices(); ++i)
                if (voices[i].isActive() && voices[i].getNote() == sounding) { idx = i; break; }
            if (idx < 0) idx = findVoice();
            voices[idx].start (ns, snap, ++ageCounter);
        }

        lastPitchSemis = (float) sounding;
        havePitch = true;
        lastNoteTime = nowSec;
        lastNoteOn.store (note);
    }

    void NaturaEngine::noteOff (int note)
    {
        const auto& list = artsFor (snap.arch);
        const int ksBase = (int) std::lround (snap[P::art_ks_base]);
        if (snap[P::art_ks_on] >= 0.5f && note >= ksBase && note < ksBase + list.count) return;

        for (int i = 0; i < heldCount; ++i)
            if (held[i] == note)
            {
                for (int j = i; j < heldCount - 1; ++j) { held[j] = held[j + 1]; heldVel[j] = heldVel[j + 1]; }
                --heldCount;
                break;
            }

        const int sounding = note + (int) std::lround (snap[P::octave]) * 12;
        const int mode = (int) std::lround (snap[P::voice_mode]);

        if (mode != 0)
        {
            if (sounding != monoNote) return;
            auto& v = voices[0];
            if (heldCount > 0 && snap[P::mono_hold] >= 0.5f)
            {
                const int back = held[heldCount - 1];
                const int backSounding = back + (int) std::lround (snap[P::octave]) * 12;
                NoteStart ns = makeStart (backSounding, heldVel[heldCount - 1], noteArt (heldVel[heldCount - 1], true, nowSec), true);
                ns.attackGlide = false;
                const bool legato = snap[P::mono_retrig] < 0.5f && mode == 1;
                v.moveTo (ns, snap, legato, ++ageCounter);
                monoNote = backSounding;
                lastPitchSemis = (float) backSounding;
            }
            else
            {
                v.keyUp (snap, live);
                monoNote = -1;
            }
            return;
        }

        for (auto& v : voices)
            if (v.isKeyDown() && v.getNote() == sounding) v.keyUp (snap, live);
    }

    void NaturaEngine::pedalChanged()
    {
        for (auto& v : voices) v.updateLetGo (snap, live);
    }

    void NaturaEngine::allNotesOff (bool hard)
    {
        for (auto& v : voices)
        {
            if (hard) v.fadeOut();
            else v.keyUp (snap, live);
        }
        heldCount = 0;
        monoNote = -1;
    }

    //==========================================================================
    void NaturaEngine::handleMidi (const juce::MidiMessage& m)
    {
        if (m.isNoteOn())             noteOn (m.getNoteNumber(), m.getVelocity());
        else if (m.isNoteOff())       noteOff (m.getNoteNumber());
        else if (m.isPitchWheel())    bendRaw = m.getPitchWheelValue();
        else if (m.isChannelPressure()) atT = (float) m.getChannelPressureValue() / 127.0f;
        else if (m.isAftertouch())    atT = std::max (atT, (float) m.getAfterTouchValue() / 127.0f);
        else if (m.isController())
        {
            const int cc = m.getControllerNumber();
            const float v = (float) m.getControllerValue() / 127.0f;
            switch (cc)
            {
                case 1:  mwT = v; break;
                case 2:  breathT = v; break;
                case 11: exprT = v; break;
                case 64:
                {
                    const bool wasDown = live.pedal >= 0.5f, isDown = v >= 0.5f;
                    live.pedal = v;
                    if (wasDown != isDown && snap.arch == Archetype::StruckString && snap[P::ss_pedalnoise] > 0.5f)
                        pedalNoise.trigger (snap[P::ss_pedalnoise] / 100.0f * (isDown ? 0.05f : 0.035f), 0.3f, sr);
                    pedalChanged();
                    break;
                }
                case 66:
                {
                    const bool on = v >= 0.5f;
                    if (on && ! ccSostenuto)
                        for (auto& vc : voices) if (vc.isKeyDown()) vc.sostenutoHeld = true;
                    ccSostenuto = on;
                    sostenuto = ccSostenuto || artSostenuto;
                    if (! sostenuto)
                    {
                        for (auto& vc : voices) vc.sostenutoHeld = false;
                        pedalChanged();
                    }
                    break;
                }
                case 67: live.soft = v >= 0.5f ? 1.0f : 0.0f; break;
                case 120: allNotesOff (true); break;
                case 123: allNotesOff (false); break;
                default: break;
            }
            if (snap[P::art_cc_on] >= 0.5f && cc == (int) std::lround (snap[P::art_cc_num]))
            {
                const int count = artsFor (snap.arch).count;
                selectArt (juce::jlimit (0, count - 1, (int) (v * (float) count * 0.999f)), false);
            }
        }
        else if (m.isAllNotesOff())   allNotesOff (false);
        else if (m.isAllSoundOff())   allNotesOff (true);
    }

    //==========================================================================
    void NaturaEngine::renderVoices (float* L, float* R, int n)
    {
        int done = 0;
        while (done < n)
        {
            const int todo = std::min (kCtl, n - done);
            // controllers glide a little so they never zipper
            const float k = 0.25f;
            live.mw     += (mwT - live.mw) * k;
            live.at     += (atT - live.at) * k;
            live.breath += (breathT - live.breath) * k;
            live.expr   += (exprT - live.expr) * k;
            bendT = (float) (bendRaw - 8192) / 8192.0f * snap[P::pb_range];
            live.bend   += (bendT - live.bend) * 0.5f;

            for (auto& v : voices)
                if (v.isActive()) v.render (snap, live, L + done, R + done, todo);

            if (pedalNoise.running())
                for (int i = 0; i < todo; ++i)
                {
                    const float x = pedalLp.process (pedalNoise.next() * pnz.next());
                    L[done + i] += x; R[done + i] += x;
                }
            done += todo;
        }
    }

    void NaturaEngine::process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, const float* values)
    {
        const int n = buffer.getNumSamples();
        if ((int) bufL.size() < n) { bufL.resize ((size_t) n); bufR.resize ((size_t) n); }
        std::fill (bufL.begin(), bufL.begin() + n, 0.0f);
        std::fill (bufR.begin(), bufR.begin() + n, 0.0f);

        buildSnapshot (values);

        int pos = 0;
        for (const auto meta : midi)
        {
            const int at = juce::jlimit (0, n, meta.samplePosition);
            if (at > pos) { renderVoices (bufL.data() + pos, bufR.data() + pos, at - pos); pos = at; }
            handleMidi (meta.getMessage());
        }
        if (pos < n) renderVoices (bufL.data() + pos, bufR.data() + pos, n - pos);

        fx.process (snap, rotorFast, motorState, vibState, bufL.data(), bufR.data(), n);

        const int chans = buffer.getNumChannels();
        if (chans >= 2)
        {
            buffer.copyFrom (0, 0, bufL.data(), n);
            buffer.copyFrom (1, 0, bufR.data(), n);
            for (int c = 2; c < chans; ++c) buffer.clear (c, 0, n);
        }
        else if (chans == 1)
        {
            float* d = buffer.getWritePointer (0);
            for (int i = 0; i < n; ++i) d[i] = 0.5f * (bufL[(size_t) i] + bufR[(size_t) i]);
        }

        int count = 0;
        for (auto& v : voices) if (v.isActive()) ++count;
        activeVoices.store (count);
        nowSec += (double) n / sr;
    }
}
