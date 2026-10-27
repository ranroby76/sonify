// C:\workspace\SONIFY PLUGIN zax\src\ZaxEngine.h
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>
#include "SynthVoice.h"
#include "ZaxKit.h"
#include "GrexFx.h"            // Sweetener, chorus, delay, reverb - Grex's own, as Sonify carries them

//==============================================================================
//  ZaxEngine.h  —  Zax's sound: live synth drum voices on the GM keys, each
//  family through its own effect chain, summed into the kit.
//
//      note-on  ->  the key's cell (ZaxKit.h) -> a free voice -> its filter and
//                   gain -> its FAMILY's buffer
//      family   ->  SWEET -> EQ -> SAT -> COMP -> CHORUS -> DELAY -> REVERB -> OUT
//      kit      ->  every family summed, the kit's volume
//
//  Drums are one-shots: a note-off ends nothing (it only drops the key's LED).
//  Choke groups cut every sounding voice of the same group in 4 ms; a key keeps
//  at most four hits of its own, the oldest let go in 30 ms.
//
//  Threads: setKit on the message thread; everything else on the audio thread.
//  The kit reaches the audio thread through a single slot under a SpinLock the
//  audio thread only ever TRIES; a retired kit is freed on the message thread.
//==============================================================================

namespace zax
{
    //==========================================================================
    //  A two-pole TPT filter, Butterworth (no peak at the corner): the key's
    //  own low cut and high cut.
    //==========================================================================
    struct KeyFilter
    {
        float ic1 = 0.0f, ic2 = 0.0f;
        void reset() noexcept   { ic1 = ic2 = 0.0f; }

        // hp: true = high-pass, false = low-pass
        float process (float x, float g, bool hp) noexcept
        {
            constexpr float k = 1.41421356f;
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float a2 = g * a1, a3 = g * a2;
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            return hp ? x - k * v1 - v2 : v2;
        }

        static float gFor (float norm, double sr) noexcept
        {
            const float hz = 20.0f * std::pow (1000.0f, std::min (1.0f, std::max (0.0f, norm)));
            return std::tan (juce::MathConstants<float>::pi * std::min (hz, (float) (sr * 0.45)) / (float) sr);
        }
    };

    //==========================================================================
    //  One family's chain.
    //==========================================================================
    class FamilyRack
    {
    public:
        void prepare (double sr)
        {
            sampleRate = sr;
            sweet.prepare (sr);
            eq.design (sr);
            eq.reset();
            chorus.prepare (sr);
            delay.prepare (sr);
            reverb.prepare (sr, true);
            compEnv = 0.0f;
        }

        void reset()
        {
            sweet.reset(); eq.reset(); chorus.reset(); delay.reset(); reverb.reset();
            compEnv = 0.0f;
        }

        void process (float* L, float* R, int n, const FamilyFx& fx, double bpm) noexcept
        {
            if (fx.sweetOn)
            {
                GrexFx::SweetenerFx::Params p;
                p.enabled     = true;
                p.mix         = juce::jlimit (0.0f, 1.0f, fx.sweetMix);
                p.softenOn    = true;  p.softenDepth = juce::jlimit (-1.0f, 1.0f, fx.softenDepth);
                p.softenMs    = juce::jlimit (1.0f, 150.0f, fx.softenMs);
                p.peakOn      = true;  p.peakCeilDb  = juce::jlimit (3.0f, 24.0f, fx.peakCeilDb);
                p.peakRatio   = juce::jlimit (1.0f, 20.0f, fx.peakRatio);
                p.tameOn      = true;  p.tameDepthDb = juce::jlimit (0.0f, 12.0f, fx.tameDepthDb);
                p.tameFreqHz  = juce::jlimit (1500.0f, 8000.0f, fx.tameFreqHz);
                p.roundOn     = true;  p.roundDrive  = juce::jlimit (0.0f, 1.0f, fx.roundDrive);
                p.roundMix    = juce::jlimit (0.0f, 1.0f, fx.roundMix);
                sweet.process (L, R, n, p);
            }

            // The EQ glides in and out on its own switch, so it always runs
            // (an untouched EQ returns at once).
            eq.process (L, R, n, fx.eqGainDb, fx.eqOn);

            if (fx.satOn && fx.satDrive > 1.0e-4f && fx.satMix > 1.0e-4f)
            {
                const float pre  = 1.0f + fx.satDrive * 19.0f;
                const float post = 1.0f / std::max (1.0f, std::tanh (pre) / 0.762f);
                const float wet  = juce::jlimit (0.0f, 1.0f, fx.satMix), dry = 1.0f - wet;
                for (int i = 0; i < n; ++i)
                {
                    L[i] = L[i] * dry + std::tanh (L[i] * pre) * post * wet;
                    R[i] = R[i] * dry + std::tanh (R[i] * pre) * post * wet;
                }
            }

            if (fx.compOn && fx.compRatio > 1.001f)
            {
                const float thresh  = juce::Decibels::decibelsToGain (fx.compThreshDb);
                const float makeup  = juce::Decibels::decibelsToGain (fx.compMakeupDb);
                const float attCoef = std::exp (-1.0f / ((float) sampleRate * std::max (0.1f, fx.compAttackMs) * 0.001f));
                const float relCoef = std::exp (-1.0f / ((float) sampleRate * std::max (1.0f, fx.compReleaseMs) * 0.001f));
                const float slope   = 1.0f - 1.0f / fx.compRatio;
                for (int i = 0; i < n; ++i)
                {
                    const float lin = std::max (std::abs (L[i]), std::abs (R[i]));
                    compEnv = lin + (lin > compEnv ? attCoef : relCoef) * (compEnv - lin);
                    float g = makeup;
                    if (compEnv > thresh && compEnv > 1.0e-6f)
                        g *= juce::Decibels::decibelsToGain (-juce::Decibels::gainToDecibels (compEnv / thresh) * slope);
                    L[i] *= g;
                    R[i] *= g;
                }
            }

            if (fx.chorusOn)
                chorus.process (L, R, n, fx.chorusRate, fx.chorusDepth, fx.chorusMix, sampleRate);

            if (fx.delayOn)
            {
                double seconds = fx.delayMs / 1000.0;
                const int note = (int) std::lround (fx.delayNote);
                if (note > 0)
                    seconds = delayDivision (note) * 60.0 / (bpm > 1.0 ? bpm : 120.0);
                delay.process (L, R, n, (float) (juce::jlimit (0.001, 2.4, seconds) * sampleRate),
                               fx.delayFeedback, fx.delayMix, sampleRate, 1.0f,
                               fx.delayDampHz, fx.delayLowCutHz);
            }

            if (fx.reverbOn)
                reverb.process (L, R, n, fx.revSize, fx.revDamp, fx.revMix,
                                (fx.revPreMs - 25.0f) / 175.0f, sampleRate, 1.0f, fx.revDecay,
                                hzToNorm (fx.revLowCutHz), hzToNorm (fx.revHighCutHz), 0.0f, 0.5f);

            const float lvl = juce::Decibels::decibelsToGain (fx.levelDb);
            const float p   = juce::jlimit (-1.0f, 1.0f, fx.pan);
            const float gl  = lvl * std::min (1.0f, 1.0f - p), gr = lvl * std::min (1.0f, 1.0f + p);
            if (gl != 1.0f) juce::FloatVectorOperations::multiply (L, gl, n);
            if (gr != 1.0f) juce::FloatVectorOperations::multiply (R, gr, n);
        }

    private:
        static float hzToNorm (float hz) noexcept   { return std::log (std::max (20.0f, hz) / 20.0f) / std::log (1000.0f); }

        double                 sampleRate = 48000.0;
        GrexFx::SweetenerFx    sweet;
        Betel::DrumSplitEq     eq;
        GrexFx::ChorusFx       chorus;
        GrexFx::StereoDelayFx  delay;
        GrexFx::ReverbFx       reverb;
        float                  compEnv = 0.0f;
    };

    //==========================================================================
    class Engine
    {
    public:
        static constexpr int kMaxVoices       = 48;
        static constexpr int kMaxHitsPerKey   = 4;
        static constexpr int kChunk           = 256;

        Engine()
        {
            voices = std::make_unique<std::array<Voice, kMaxVoices>>();
            racks  = std::make_unique<std::array<FamilyRack, kNumFamilies>>();
        }

        //======================================================================
        // Message thread
        void setKit (std::shared_ptr<const Kit> kit)
        {
            std::shared_ptr<const Kit> old;
            {
                const juce::SpinLock::ScopedLockType sl (kitLock);
                old = std::move (pending);
                pending = std::move (kit);
            }
            // old: a kit the audio thread never took - freed here
            freeRetired();
        }

        // Kits the audio thread let go of, freed on the message thread
        void freeRetired()
        {
            std::shared_ptr<const Kit> r;
            {
                const juce::SpinLock::ScopedLockType sl (kitLock);
                r = std::move (retired);
            }
        }

        //======================================================================
        // Audio thread
        void prepare (double sr, int maxBlock)
        {
            sampleRate = sr;
            juce::ignoreUnused (maxBlock);
            for (int i = 0; i < kMaxVoices; ++i)
            {
                auto& v = (*voices)[(size_t) i];
                v.synth.prepare (sr, 0x9E3779B9u ^ (uint32_t) ((i + 1) * 7919));
                v.active = false;
            }
            for (auto& r : *racks) r.prepare (sr);
            famL.assign ((size_t) kNumFamilies * kChunk, 0.0f);
            famR.assign ((size_t) kNumFamilies * kChunk, 0.0f);
            scratch.assign ((size_t) kChunk, 0.0f);
        }

        void allSoundOff() noexcept
        {
            for (auto& v : *voices) { v.active = false; v.synth.stop(); }
            for (auto& r : *racks) r.reset();
        }

        void noteOn (int key, int velocity) noexcept
        {
            takeKit();
            if (live == nullptr || ! isZaxKey (key) || velocity <= 0) return;
            const auto& cell = live->cells[(size_t) key];
            if (cell.sound.isEmpty()) return;

            // Choke: every sounding voice of the same group lets go in 4 ms
            if (cell.chokeGroup > 0)
                for (auto& v : *voices)
                    if (v.active && v.choke == cell.chokeGroup)
                        v.fadeOut (4.0f, sampleRate);

            // A key keeps four hits at most: the oldest lets go
            {
                int sounding = 0;
                Voice* oldest = nullptr;
                for (auto& v : *voices)
                    if (v.active && v.key == key && ! v.fading)
                    {
                        ++sounding;
                        if (oldest == nullptr || v.serial < oldest->serial) oldest = &v;
                    }
                if (sounding >= kMaxHitsPerKey && oldest != nullptr)
                    oldest->fadeOut (30.0f, sampleRate);
            }

            Voice* v = nullptr;
            for (auto& c : *voices)
                if (! c.active) { v = &c; break; }
            if (v == nullptr)                                // every voice busy: the oldest
                for (auto& c : *voices)
                    if (v == nullptr || c.serial < v->serial) v = &c;

            v->active  = true;
            v->fading  = false;
            v->fade    = 1.0f;
            v->fadeInc = 0.0f;
            v->key     = key;
            v->family  = roleFamily (key);
            v->choke   = cell.chokeGroup;
            v->gain    = juce::Decibels::decibelsToGain (cell.gainDb);
            v->hpOn    = cell.filterHpNorm > 0.001f;
            v->lpOn    = cell.filterLpNorm < 0.999f;
            v->gHp     = KeyFilter::gFor (cell.filterHpNorm, sampleRate);
            v->gLp     = KeyFilter::gFor (cell.filterLpNorm, sampleRate);
            v->hp.reset();
            v->lp.reset();
            v->serial  = ++serialCounter;
            v->synth.trigger (cell.sound, cell.macros, velocity, 0.0f);
        }

        void render (float* outL, float* outR, int numSamples, double bpm) noexcept
        {
            takeKit();
            int done = 0;
            while (done < numSamples)
            {
                const int n = std::min (kChunk, numSamples - done);
                renderChunk (outL + done, outR + done, n, bpm);
                done += n;
            }
        }

        // A family's rack is running (for nothing but tests and the editor)
        const Kit* liveKit() const noexcept   { return live.get(); }

    private:
        struct Voice
        {
            edm::SynthVoice synth;
            KeyFilter hp, lp;
            bool   active = false, fading = false, hpOn = false, lpOn = false;
            int    key = -1, family = -1, choke = 0;
            float  gain = 1.0f, fade = 1.0f, fadeInc = 0.0f, gHp = 0.0f, gLp = 1.0f;
            uint32_t serial = 0;

            void fadeOut (float ms, double sr) noexcept
            {
                if (fading && fadeInc >= 1.0f / (float) (sr * ms * 0.001)) return;
                fading  = true;
                fadeInc = 1.0f / std::max (1.0f, (float) (sr * ms * 0.001));
            }
        };

        void takeKit() noexcept
        {
            const juce::SpinLock::ScopedTryLockType sl (kitLock);
            if (! sl.isLocked() || pending == nullptr) return;
            if (retired == nullptr)                       // the message thread frees it
            {
                retired = std::move (live);
                live    = std::move (pending);
            }
        }

        void renderChunk (float* outL, float* outR, int n, double bpm) noexcept
        {
            std::array<bool, kNumFamilies> used {};
            for (int f = 0; f < kNumFamilies; ++f)
            {
                std::fill_n (famL.data() + (size_t) f * kChunk, n, 0.0f);
                std::fill_n (famR.data() + (size_t) f * kChunk, n, 0.0f);
            }

            for (auto& v : *voices)
            {
                if (! v.active) continue;
                if (! v.synth.isActive()) { v.active = false; continue; }
                float* s = scratch.data();
                v.synth.renderMono (s, n);

                const int f = (v.family >= 0 && v.family < kNumFamilies) ? v.family : 0;
                float* L = famL.data() + (size_t) f * kChunk;
                float* R = famR.data() + (size_t) f * kChunk;
                used[(size_t) f] = true;
                for (int i = 0; i < n; ++i)
                {
                    float x = s[i];
                    if (v.hpOn) x = v.hp.process (x, v.gHp, true);
                    if (v.lpOn) x = v.lp.process (x, v.gLp, false);
                    x *= v.gain * v.fade;
                    if (v.fading)
                    {
                        v.fade -= v.fadeInc;
                        if (v.fade <= 0.0f) { v.fade = 0.0f; v.active = false; v.synth.stop(); }
                    }
                    L[i] += x;
                    R[i] += x;
                    if (! v.active) break;
                }
            }

            std::fill_n (outL, n, 0.0f);
            std::fill_n (outR, n, 0.0f);
            for (int f = 0; f < kNumFamilies; ++f)
            {
                float* L = famL.data() + (size_t) f * kChunk;
                float* R = famR.data() + (size_t) f * kChunk;
                const FamilyFx* fx = live != nullptr ? &live->fx[(size_t) f] : nullptr;
                const bool rack = fx != nullptr && fx->runs();
                if (! rack && ! used[(size_t) f]) continue;
                if (rack) (*racks)[(size_t) f].process (L, R, n, *fx, bpm);
                juce::FloatVectorOperations::add (outL, L, n);
                juce::FloatVectorOperations::add (outR, R, n);
            }

            const float vol = live != nullptr ? juce::Decibels::decibelsToGain (live->volumeDb) : 1.0f;
            if (vol != 1.0f)
            {
                juce::FloatVectorOperations::multiply (outL, vol, n);
                juce::FloatVectorOperations::multiply (outR, vol, n);
            }
        }

        double sampleRate = 48000.0;
        std::unique_ptr<std::array<Voice, kMaxVoices>>        voices;
        std::unique_ptr<std::array<FamilyRack, kNumFamilies>> racks;
        std::vector<float> famL, famR, scratch;
        uint32_t serialCounter = 0;

        juce::SpinLock              kitLock;
        std::shared_ptr<const Kit>  pending, retired;   // under kitLock
        std::shared_ptr<const Kit>  live;               // audio thread's own
    };
}
