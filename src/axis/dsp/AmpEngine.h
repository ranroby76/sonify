#pragma once

#include "AmpVoices.h"

#include <array>
#include <atomic>

// ============================================================================
//  AXIS AMP — one amp, one channel, at the oversampled rate.
//
//      in → TIGHT → V1 → [bright network] → GAIN (bright cap) → V2 ... Vn
//         → cathode follower → tone stack → MASTER → presence / resonance
//         → supply sag → phase inverter → power tubes → output transformer
//
//  Plain C++17 like the rest of the amp's core (AmpModel.h is the part that
//  knows JUCE). Nothing here allocates once the tubes' curves are built: they
//  are built once, for every amp and every instance (curves()), and the first
//  call to it must come from the message thread (AmpModel's constructor makes
//  sure of it).
// ============================================================================
namespace axis::dsp::amp
{
    /** The knobs, 0..1 each, and the two switches. */
    struct AmpControls
    {
        double gain = 0.5, bass = 0.5, mid = 0.5, treble = 0.5, presence = 0.5, master = 0.5;
        bool bright = false, tight = false;
    };

    class AmpEngine
    {
    public:
        static constexpr int numVoices = 5;
        static constexpr int maxStages = 4;

        /** Every amp's tube curves, built the first time it's asked for. */
        static const std::array<std::array<TriodeCurve, maxStages>, numVoices>& curves()
        {
            static const auto built = []
            {
                std::array<std::array<TriodeCurve, maxStages>, numVoices> c;

                for (int v = 0; v < numVoices; ++v)
                    for (int s = 0; s < voiceSpecs()[(size_t) v].stages; ++s)
                        c[(size_t) v][(size_t) s].build (voiceSpecs()[(size_t) v].stage[(size_t) s]);

                return c;
            }();

            return built;
        }

        void prepare (double sampleRate) noexcept
        {
            fs = sampleRate;
            voice = voiceWanted.load();      // the amp asked for before it plays: no fade into it
            fade = 1.0;
            fadeDirection = 0;
            applyVoice();
            controlsNow = controlsWanted;
            applyControls (controlsNow);
            reset();
        }

        /** From clean, and as it's set now: the amp asked for and the knobs where they are, no
            fade or glide into them. */
        void restart() noexcept                 { prepare (fs); }

        void reset() noexcept
        {
            tight.reset();
            tightMix = tightTarget;
            network.reset();
            gainPot.reset();
            for (int s = 0; s < spec->stages; ++s)
                stage[(size_t) s].reset();
            follower.x1 = 0.0; follower.f1 = spec->follower.integralAt (0.0);
            stack.reset();
            midBand.reset();
            presence.reset();
            resonance.reset();
            pi.x1 = 0.0; pi.f1 = TanhCurve().integralAt (0.0);
            tubes.x1 = 0.0; tubes.f1 = PowerCurve().integralAt (0.0);
            transformerLow.reset();
            transformerHigh.reset();
            sagEnv = 0.0;
        }

        /** Any thread, any time: the amp changes at the next quiet moment (a short fade). */
        void setVoice (int index) noexcept      { voiceWanted.store (std::clamp (index, 0, numVoices - 1)); }

        int getVoice() const noexcept           { return voice; }

        /** The audio thread, each block: the knobs glide to these over the block. */
        void setControls (const AmpControls& c) noexcept   { controlsWanted = c; }

        void process (double* x, int n) noexcept
        {
            // A new amp: down to silence, the change, up again (a few milliseconds each way).
            const int wanted = voiceWanted.load (std::memory_order_relaxed);

            if (wanted != voice && fadeDirection == 0)
                fadeDirection = -1;

            const double fadeStep = 1.0 / (0.004 * fs);

            int done = 0;

            while (done < n)
            {
                // Knobs move in short steps: new coefficients every 32 samples while they glide.
                const int chunk = std::min (32, n - done);
                glideControls (chunk);

                for (int i = 0; i < chunk; ++i)
                {
                    double y = tick (x[done + i]);

                    if (fadeDirection != 0)
                    {
                        fade += fadeDirection * fadeStep;

                        if (fade <= 0.0)
                        {
                            fade = 0.0;
                            voice = voiceWanted.load (std::memory_order_relaxed);
                            applyVoice();
                            applyControls (controlsNow);
                            reset();
                            fadeDirection = +1;
                        }
                        else if (fade >= 1.0)
                        {
                            fade = 1.0;
                            fadeDirection = 0;
                        }
                    }

                    x[done + i] = y * fade;
                }

                // Anything that ever went non-finite in here (it shouldn't: every curve refuses a
                // NaN) would otherwise stay in the filters for good. Start clean instead.
                if (! std::isfinite (x[done + chunk - 1]))
                {
                    reset();

                    for (int i = 0; i < chunk; ++i)
                        x[done + i] = 0.0;
                }

                done += chunk;
            }
        }

        /** For the tests: the controls as they are applied now. */
        const AmpControls& controlsApplied() const noexcept   { return controlsNow; }

    private:
        double tick (double in) noexcept
        {
            const auto& v = *spec;

            double x = in * v.inputVolts;

            // TIGHT: its filter always runs, and the switch fades between it and the plain input.
            const double trimmed = tight.process (x);

            if (tightMix != tightTarget)
                tightMix = tightMix < tightTarget ? std::min (tightTarget, tightMix + switchStep)
                                                  : std::max (tightTarget, tightMix - switchStep);

            x += tightMix * (trimmed - x);

            for (int s = 0; s < v.stages; ++s)
            {
                x = stage[(size_t) s].process (x);

                if (s == 0 && v.networkLevel > 0.0)
                    x = network.process (x);

                if (s == v.gainAfter)
                    x = gainPot.process (x);

                x *= v.after[(size_t) s];
            }

            // The follower, then the tone stack it drives.
            x = follower.process (v.follower, x);
            x = stack.process (x);

            if (v.midBandHz > 0.0)
                x = midBand.process (x);

            // The power section, in units of the phase inverter's swing.
            double u = x * powerDrive;
            u = resonance.process (presence.process (u));
            u /= 1.0 + v.power.sag * sagEnv;

            const double split = pi.process (TanhCurve(), u);
            const double w = tubes.process (PowerCurve(), split * v.power.tubeDrive + v.power.classA * sagEnv);

            // The supply gives under load, and recovers.
            const double level = std::abs (w);
            sagEnv += (level - sagEnv) * (level > sagEnv ? sagAttack : sagRelease);

            double out = transformerHigh.process (transformerLow.process (w));
            return out * outputGain;
        }

        void applyVoice() noexcept
        {
            spec = &voiceSpecs()[(size_t) voice];
            const auto& v = *spec;
            const auto& c = curves()[(size_t) voice];

            tight.setHighpass (v.tightHz, fs);

            for (int s = 0; s < maxStages; ++s)
            {
                stage[(size_t) s].setCurve (&c[(size_t) s], v.stage[(size_t) s]);

                if (s < v.stages)
                    stage[(size_t) s].prepare (fs);
            }

            if (v.networkLevel > 0.0)
            {
                const double wz = 1.0 / (v.networkR * v.networkC);
                network.setZeroPole (wz, wz / v.networkLevel, fs);
            }

            stack.setParts (v.stack);
            stack.prepare (fs);

            transformerLow.setHighpass (v.power.transformerLowHz, 0.7, fs);
            transformerHigh.setLowpass (v.power.transformerHighHz, fs);

            sagAttack = 1.0 - std::exp (-1.0 / (v.power.sagAttackMs * 0.001 * fs));
            sagRelease = 1.0 - std::exp (-1.0 / (v.power.sagReleaseMs * 0.001 * fs));
        }

        void applyControls (const AmpControls& c) noexcept
        {
            const auto& v = *spec;

            tightTarget = c.tight ? 1.0 : 0.0;
            switchStep = 1.0 / (0.010 * fs);
            gainPot.set (audioTaper (c.gain), v.gainPotR, v.brightC, c.bright, fs);
            stack.setKnobs (c.bass, c.mid, c.treble);
            stack.update();

            if (v.midBandHz > 0.0)
                midBand.setPeak (v.midBandHz, 0.7, (c.mid - 0.5) * 12.0, fs);

            presence.setHighShelf (v.power.presenceHz, 0.7,
                                   v.power.presenceMinDb + (v.power.presenceMaxDb - v.power.presenceMinDb) * c.presence, fs);

            if (v.power.resonanceDb != 0.0)
                resonance.setLowShelf (v.power.resonanceHz, 0.7, v.power.resonanceDb, fs);
            else
                resonance.setThrough();

            // MASTER drives the power section; the output follows it at half its dB, so it
            // still gets louder as it's turned up, without leaping.
            const double master = audioTaper (c.master, 0.2);
            powerDrive = v.power.drive * master;
            outputGain = v.outputTrim / std::sqrt (std::max (master, 1.0e-3)) * (master > 1.0e-4 ? 1.0 : 0.0);

            // GAIN too gets louder as it goes up, as an amp does, but at a little under a third of
            // its dB less than the pot itself (from the middle of the knob), so a clean amp turned
            // up doesn't leap and a high gain one, already as loud as it gets, doesn't sink.
            const double pot = std::max (audioTaper (c.gain), 1.0e-3);
            outputGain *= std::pow (pot / audioTaper (0.5), -0.3);
        }

        void glideControls (int samples) noexcept
        {
            auto& a = controlsNow;
            const auto& b = controlsWanted;
            const double k = std::min (1.0, samples / (0.02 * fs));   // about 20 ms to get there

            auto glide = [k] (double& from, double to) { from += (to - from) * k; if (std::abs (to - from) < 1.0e-5) from = to; };

            const bool moving = a.gain != b.gain || a.bass != b.bass || a.mid != b.mid || a.treble != b.treble
                             || a.presence != b.presence || a.master != b.master || a.bright != b.bright || a.tight != b.tight;

            if (! moving)
                return;

            glide (a.gain, b.gain); glide (a.bass, b.bass); glide (a.mid, b.mid);
            glide (a.treble, b.treble); glide (a.presence, b.presence); glide (a.master, b.master);
            a.bright = b.bright;
            a.tight = b.tight;
            applyControls (a);
        }

        double fs = 192000.0;
        int voice = 0;
        std::atomic<int> voiceWanted { 0 };
        const VoiceSpec* spec = &voiceSpecs()[0];

        AmpControls controlsNow, controlsWanted;

        FirstOrder tight, network;
        double tightMix = 0.0, tightTarget = 0.0, switchStep = 0.001;
        VolumePot gainPot;
        std::array<TriodeStage, maxStages> stage;
        Adaa follower;
        ToneStack stack;
        Biquad midBand, presence, resonance, transformerLow;
        FirstOrder transformerHigh;
        Adaa pi, tubes;

        double powerDrive = 0.0, outputGain = 1.0;
        double sagEnv = 0.0, sagAttack = 0.0, sagRelease = 0.0;
        double fade = 1.0;
        int fadeDirection = 0;
    };
}
