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
}
