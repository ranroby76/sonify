// C:\workspace\SONIFY PLUGIN natura\src\ModelBase.h
#pragma once
//==============================================================================
//  ModelBase.h  -  what every archetype model receives and how it is driven.
//
//  A model is the SOUND SOURCE of a voice.  The voice's own chain (body,
//  character, the synthesis filter and envelopes, the LFOs) shapes what comes
//  out of it, exactly like an ordinary synthesizer shapes an oscillator.
//
//  The voice hands a model a ModelIn once per control block (16 samples):
//  pitch, breath / bow pressure, the shared section values and the model's own
//  archetype-specific values, all normalised 0..1.  Pure C++, no JUCE.
//==============================================================================
#include "ModelDsp.h"
#include "OscEngine.h"

namespace natura
{
    /** How the voice turns dynamics (velocity / breath / expression) into the
        model's pressure input and its output level. */
    enum class Dynamics
    {
        Physical,   // pressure = env, dyn passed through: each model keeps its own
                    // physics inside the window where it speaks; level * (0.25 + 0.75 dyn)
        Direct,     // pressure = env * dyn (the model shapes its own response)
        Swell,      // pressure = env, level * dyn (organs: the swell pedal)
        Struck      // velocity strikes it; a continuous source rides the level
    };

    /** How a note is set going - articulations can change it. */
    enum class Excite { Normal = 0, Pluck, Strike };

    /** Brass mutes (Lip Reed archetype, also the Mute articulations). */
    enum class Mute { Open = 0, Straight, Cup, Harmon, Wah };

    constexpr int kMaxSpec = 16;

    struct ModelIn
    {
        // pitch and dynamics ---------------------------------------------------
        float freq     = 440.0f;   // Hz, everything included (bend, glide, vibrato)
        float pressure = 0.0f;     // sustained drive envelope 0..1 (breath, bow, bellows)
        float dyn      = 1.0f;     // 0..1 the player's dynamics (velocity, breath CC, expression)
        float velocity = 0.8f;     // 0..1 after the velocity curve
        int   note     = 60;
        std::uint32_t seed = 0;    // per note and unison unit: no two strikes alike

        // EXCITER --------------------------------------------------------------
        float strength  = 0.5f;
        float hardness  = 0.5f;
        float noise     = 0.2f;
        float transient = 0.5f;

        // RESONATOR ------------------------------------------------------------
        float decay    = 0.5f;
        float damping  = 0.5f;
        float inharm   = 0.2f;
        float oddEven  = 0.5f;

        // archetype-specific, in the model's own order --------------------------
        float spec[kMaxSpec] = {};

        // performance ------------------------------------------------------------
        float  bright   = 0.0f;    // -1..1 offset from mod wheel / aftertouch / VAR
        float  dampAdd  = 0.0f;    // 0..1 articulation damping (palm mute, dead stroke)
        float  decayMul = 1.0f;    // articulation decay scale
        float  pedal    = 0.0f;    // 0..1 damper lift (sustain pedal, half pedal)
        float  soft     = 0.0f;    // 0..1 una corda / soft pedal
        float  growl    = 0.0f;    // 0..1 growl / flutter roughness on the drive
        float  wah      = 0.5f;    // 0..1 wah mute position
        Excite excite   = Excite::Normal;
        Mute   mute     = Mute::Open;
        int    stateOffset = 0;    // free-reed register shift
        bool   percTrigger = true; // tonewheel percussion is single-trigger
        bool   harmonic = false;   // natural harmonic (plucked: excite at the node)
        bool   released = false;

        const oscengine::Params* osc = nullptr;   // SYNTH archetype settings
    };

    class ModelBase
    {
    public:
        virtual ~ModelBase() = default;

        virtual void prepare (double sampleRate) = 0;
        virtual void reset() = 0;

        /** A new note.  legato: the same voice carries on (mono slur) - keep the
            resonator ringing, only move the pitch and, where it makes sense,
            excite softly. */
        virtual void start (const ModelIn& in, bool legato) = 0;

        /** Key up (after the sustain pedal has been taken into account). */
        virtual void release (const ModelIn& in) = 0;

        /** n samples, n <= 64. */
        virtual void render (const ModelIn& in, float* out, int n) = 0;

        /** Sustained-drive models (bow, breath, bellows) need the voice's
            pressure envelope; decaying models (struck, plucked) do not. */
        virtual bool isSustained() const = 0;

        virtual Dynamics dynamics() const = 0;

    protected:
        double sr = 48000.0;
    };

    //==========================================================================
    //  Shared attack helpers: the tongue (or bow bite) and priming a self-oscillating
    //  model so a note starts from a real oscillation.
    //==========================================================================
    /** How much of a primed oscillation a tongued note starts from: a hard, fast tongue more. */
    inline float seedLevel (const ModelIn& in)
    {
        // a low note grows over fewer periods a second: it starts from more of its oscillation
        const float low = dsp::clampv (std::sqrt (262.0f / std::max (20.0f, in.freq)), 1.0f, 1.8f);
        return dsp::clampv ((0.06f + 0.22f * in.transient) * (0.55f + 0.45f * in.velocity) * low, 0.02f, 0.5f);
    }

    /** The tongue: as it leaves the reed (or the lips, or the jet's edge) the air already
        pushing behind it lets go at once.  A short sharp pressure pulse into the bore - so the
        oscillation starts from a real disturbance instead of growing out of the noise floor
        (which is what made blown notes swell in over a tenth of a second) - and the "t" of
        the consonant, a short band of noise. */
    struct Tongue
    {
        dsp::Burst kick, consonant;
        dsp::Biquad bp;
        void reset() { kick = {}; consonant = {}; bp.reset(); }
        void trigger (float kickAmp, float noiseAmp, double centreHz, double sr)
        {
            kick.trigger (kickAmp, 0.025f, sr);
            if (noiseAmp > 0.0005f) { consonant.trigger (noiseAmp, 0.018f, sr); bp.bandpass (centreHz, 0.9, sr); }
        }
        float pulse()                     { return kick.running() ? kick.next() : 0.0f; }
        float noise (dsp::Noise& n)       { return consonant.running() ? bp.process (consonant.next() * n.next()) : 0.0f; }
    };

    /** A player's air column is primed before the tongue lets go: the pressure is already
        there, so the note starts from a real oscillation, not from the noise floor.  Priming
        runs the model silently at playing pressure until its oscillation stops growing (at
        most maxSeconds), and the caller then scales its bore back to `seed` of that - the
        note grows from there in a few periods, as a tongued note does. */
    /** Priming costs a burst of work at a note-on.  A chord of unison voices would ask for a
        lot of it in one audio block, so the engine hands each block a budget (in model
        samples, NaturaEngine::process) and a note that finds it spent starts unprimed - the
        tongue's pulse still sets it going, only a little later. */
    struct PrimeBudget
    {
        static int& left() { static thread_local int v = 1 << 30; return v; }
    };

    template <typename Model>
    inline void primeOscillation (Model& m, const ModelIn& in, double sr, double maxSeconds = 0.15)
    {
        ModelIn p = in;
        p.pressure = 0.9f;
        p.dyn = std::max (p.dyn, 0.7f);
        float tmp[64];
        maxSeconds *= dsp::clampv (262.0 / std::max (20.0, (double) in.freq), 1.0, 2.5);   // low notes settle slower
        const int maxBlocks = std::min ((int) (maxSeconds * sr / 64.0), PrimeBudget::left() / 64);
        float last = 0.0f, floorE = 1.0e30f;
        int steady = 0, b = 0;
        for (; b < maxBlocks && steady < 3; ++b)
        {
            m.render (p, tmp, 64);
            float e = 0.0f;
            for (float v : tmp) e += v * v;
            if (e > 1.0e-12f) floorE = std::min (floorE, e);
            // grown well out of the noise and no longer growing fast: the shape is there
            // (what's kept is scaled down anyway), so stop
            const bool grown = e > floorE * 100.0f && e < last * 1.1f;
            if (grown || (e > 1.0e-10f && e < last * 1.04f && e > last * 0.96f)) ++steady; else steady = 0;
            last = e;
        }
        PrimeBudget::left() -= b * 64;
    }

    /** A tongued reed note's level over its first 140 ms, measured from a real clarinet
        (a tongued middle C, level against the held note every 10 ms): a fast climb out of
        the tongue's release, an overshoot of about 2 dB at 50 ms - the breath behind the
        tongue - and a settle.  The model starts its oscillation nearly grown (primed) and
        this shapes how it is heard, so the attack has the punch of the real one however
        long the model itself would take.  Its upper harmonics also run a few dB hot through
        the climb (the reed beats hard as it starts): bright() is that extra, 0 when held.
        speed: 1 as measured, more is faster; overshoot: 1 as measured. */
    struct TonguedAttack
    {
        static constexpr int kPoints = 15;   // 0, 10, ... 140 ms
        float gain[kPoints] = {}, brightG[kPoints] = {};
        float t = 0.0f, msPerSample = 0.0f, lastBright = 0.0f;
        bool  on = false;

        void start (float speed, float overshoot, double sr)
        {
            static const float db[kPoints] = { -25.0f, -21.0f, -12.8f, -4.5f, 0.5f, 2.0f, 1.6f, 1.4f, 1.2f,
                                               1.0f, 0.9f, 0.6f, 0.3f, 0.15f, 0.0f };
            static const float bdb[kPoints] = { 2.0f, 3.0f, 4.5f, 6.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.2f,
                                                1.5f, 1.0f, 0.6f, 0.3f, 0.1f, 0.0f };
            for (int i = 0; i < kPoints; ++i)
            {
                gain[i] = dsp::dbToGain (db[i] > 0.0f ? db[i] * overshoot : db[i]);
                brightG[i] = dsp::dbToGain (bdb[i] * overshoot) - 1.0f;
            }
            t = 0.0f;
            msPerSample = (float) (1000.0 / sr) * std::max (0.2f, speed);
            on = true;
        }
        void stop() { on = false; }

        float next()
        {
            if (! on) return 1.0f;
            const float x = t * 0.1f;
            const int   i = (int) x;
            if (i >= kPoints - 1) { on = false; lastBright = 0.0f; return 1.0f; }
            t += msPerSample;
            const float f = x - (float) i;
            lastBright = brightG[i] + f * (brightG[i + 1] - brightG[i]);
            return gain[i] + f * (gain[i + 1] - gain[i]);
        }

        /** The upper harmonics' extra (linear, 0 = none) at the sample next() just gave. */
        float bright() const { return on ? lastBright : 0.0f; }
    };
}
