// C:\workspace\SONIFY PLUGIN natura\src\NaturaEngine.h
#pragma once
//==============================================================================
//  NaturaEngine.h  -  voices, MIDI, articulations and the effects.
//
//  POLY          free voice, else the oldest released, else the oldest
//  MONO LEGATO   one voice; overlapping notes move pitch without
//                re-articulating (a real slur); HOLD STOLEN returns to a
//                still-held note on release, RETRIG STOLEN re-articulates it
//  MONO RETRIG   one voice, re-articulated on every note
//
//  Pedals: CC 64 is continuous (half pedal reaches the dampers), CC 66
//  sostenuto, CC 67 soft (una corda).
//==============================================================================
#include "FxChain.h"
#include <atomic>

namespace natura
{
    class NaturaEngine
    {
    public:
        static constexpr int kMaxVoices = 32;

        NaturaEngine();

        void prepare (double sampleRate, int maxBlock);
        void reset();

        /** values: every parameter's plain value for this block. */
        void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, const float* values);

        // read by the editor ------------------------------------------------------
        std::atomic<int> currentArt { 0 };
        std::atomic<int> activeVoices { 0 };
        std::atomic<int> lastNoteOn { -1 };

        // written by the editor ----------------------------------------------------
        std::atomic<int> forcedArt { -1 };

    private:
        void buildSnapshot (const float* values);
        void handleMidi (const juce::MidiMessage& m);
        void noteOn (int note, int velocity);
        void noteOff (int note);
        void selectArt (int index, bool fromPanel);
        void applyStateArt (Art a);
        void allNotesOff (bool hard);
        void renderVoices (float* L, float* R, int n);
        void pedalChanged();
        int  findVoice();
        int  maxVoices() const;
        Art  noteArt (int velocity, bool overlapping, double now);
        NoteStart makeStart (int soundingNote, int velocity, Art art, bool overlapping);
        bool decideAttackGlide (int rawVelocity);

        double sr = 48000.0;
        Snapshot snap;
        Live live;
        NaturaVoice voices[kMaxVoices];
        FxChain fx;
        std::vector<float> bufL, bufR;
        long ageCounter = 0;
        double nowSec = 0.0;

        // held keys (sounding notes, for mono and auto legato)
        int  held[16] = {};
        int  heldVel[16] = {};
        int  heldCount = 0;
        int  monoNote = -1;
        double lastNoteTime = -10.0;
        float lastPitchSemis = 60.0f;
        bool  havePitch = false;

        // articulation state
        int  artIndex = 0;
        int  lastPanelArt = -1;
        int  lastArchetype = -1;
        bool rotorFast = false;
        int  lastRotParam = -1;
        int  motorState = 0, vibState = 0, percState = 0;   // 0 as set, 1 on, 2 off
        int  regOffset = 0;
        bool sostenuto = false, ccSostenuto = false, artSostenuto = false;

        // attack glide
        int  glideCounter = 0;
        std::uint32_t glideRng = 0x2545F491u;
        std::uint32_t noteSeed = 1;

        // smoothed controllers
        float mwT = 0.0f, atT = 0.0f, breathT = 1.0f, exprT = 1.0f, bendT = 0.0f;
        int   bendRaw = 8192;

        // piano pedal noise
        dsp::Burst pedalNoise;
        dsp::OnePoleLP pedalLp;
        dsp::Noise pnz;
    };
}
