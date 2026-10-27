// D:\workspace\Fanan_web_hub\src\tomahawk\TomahawkVoice.h
//
// The Tomahawk sampler's sound: Grex's right hand, one instrument of it. Grex's own Channel plays
// it (its voices, envelopes, filters, LFOs, glide, mono modes and insert effects: EQ, wah,
// phaser, sweetener), with Grex's chorus, reverb and delay behind it as the instrument's own
// effects (Grex's section rack, set from the sound's settings rather than shared), and a sound
// is voiced the way Grex voices a right-hand pick: from its own .ins. Nothing here changes how
// Grex sounds; what's Sonify's is around it: the library on disk (found anywhere, scanned once for
// every Tomahawk), the instruments read on a thread of their own, the presets beside Sonify.exe
// (Save Preset writes there too), a base octave beside Grex's base gain, and the state kept with
// the song.
//
// The library (Tomahawk_Sampler): classic\<category>\NNNN-Name.frb, oriental\<category>\NNNN-Name.frb
// (Grex's own folder names, sounds_world and sounds_oriental, are found too). Drum kits
// (*_kit.frb, NNN_Name.frb) aren't the Tomahawk's.
//
// The presets: Tomahawk_classic_instruments_presets and Tomahawk_oriental_instruments_presets,
// beside Sonify.exe (the build puts them there), each sorted into categories (the flag decides,
// not the folder); a pack's own instruments_presets when they aren't there. They're the one place
// a sound's preset lives: Save Preset, the base gain and the base octave are written into the
// sound's file there (and, while developing, into the build's source copy too). Where Sonify.exe's
// folder can't be written (installed in Program Files), they go into the same folders in Sonify's
// AppData folder (AppData\Roaming\Fanan\Sonify\Tomahawk) instead, and a preset there wins over
// the one beside Sonify.exe. A preset someone shares is dropped into either; both are read again
// whenever a Tomahawk's window opens.
//
// Threads: the library and a Voice's settings on the message thread; Voice::render on the audio
// thread; an instrument read on the Voice's own thread, which puts it in place.

#pragma once

#include "TomahawkJuce.h"
#include "Channel.h"
#include "InstrumentPreset.h"
#include "SlotParamConvert.h"

#include <array>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace Tomahawk
{
    /** Grex's sound packs are all signed with this code (Grex's kFixedAccessCode). */
    inline constexpr const char* kAccessCode = "111222";

    enum Pack { packClassic = 0, packOriental, numPacks };

    //==============================================================================
    /** The library on disk, one for every Tomahawk in Sonify. Message thread, except
        contents(), which any thread may take. */
    class Library
    {
    public:
        struct Sound
        {
            int flag = -1;
            int pack = packClassic;
            juce::String name, category;   // as Grex shows them (underscores off)
            juce::File file;
        };

        /** One scan: what it found, never changed after (a rescan makes another). */
        struct Contents
        {
            juce::File root;
            std::map<int, Sound> sounds;                          // by flag
            std::array<juce::StringArray, numPacks> categories;   // in the order shown
            std::map<int, Betel::InstrumentPreset> factory;       // the presets (.ins), by flag
            std::map<int, juce::File> factoryFiles;               // the file each came from
            juce::String report;                                  // what didn't load, and why

            bool isInstalled() const noexcept                       { return ! sounds.empty(); }
            const Sound* find (int flag) const;
            const Betel::InstrumentPreset* presetFor (int flag) const;
            juce::File presetFileFor (int flag) const;                    // its file
            std::vector<const Sound*> soundsIn (int pack, const juce::String& category) const;
            bool hasPack (int pack) const;
        };

        static Library& get();

        /** Where Sonify keeps the user's things (the Tomahawk's settings go in its Tomahawk
            folder). Set by the app. */
        static void setUserFolder (std::function<juce::File()> folder);
        static juce::File userFolder();          // <user>\Tomahawk

        /** A pack's presets: Tomahawk_<pack>_instruments_presets beside Sonify.exe (on a Mac in
            the app's Resources), and the same folder in Sonify's AppData folder, for presets
            saved where Sonify.exe's folder can't be written (read after, so they win). */
        static juce::File factoryPresetsFolder (int pack);
        static juce::File savedPresetsFolder (int pack);

        /** The library's own download (the whole of it, one zip). */
        static constexpr const char* downloadLink = "https://pub-69afac88e4554ad1a83a725f618e71e7.r2.dev/full/Tomahawk_Library_v1.zip";

        static const char* packName (int pack) noexcept;           // "CLASSIC", "ORIENTAL"

        /** The folder that holds classic and oriental, from the one chosen (it, the
            Tomahawk_Sampler in it, or the one above a pack's folder); none when it isn't one. */
        static juce::File findRoot (const juce::File& chosen);

        /** What was scanned last (scanned first from the saved place, else the usual ones). */
        std::shared_ptr<const Contents> contents();

        /** Sets the library's folder, keeps it for next time and scans it. */
        bool locate (const juce::File& chosen, juce::String& problem);
        void rescan();

        /** The presets read again (one saved, or dropped into their folders). */
        void reloadPresets();

        struct Listener
        {
            virtual ~Listener() = default;
            virtual void libraryChanged() = 0;   // message thread
        };

        void addListener (Listener* l)        { listeners.add (l); }
        void removeListener (Listener* l)     { listeners.remove (l); }

    private:
        Library() = default;
        std::shared_ptr<Contents> scan (const juce::File& root) const;
        void publish (std::shared_ptr<Contents> made);
        juce::File savedRoot() const;
        void saveRoot (const juce::File& root) const;

        std::shared_ptr<const Contents> current;
        bool tried = false;
        juce::CriticalSection lock;
        juce::ListenerList<Listener> listeners;
    };

    //==============================================================================
    /** One Tomahawk's instrument: what plays and how it's voiced. */
    class Voice final : private Library::Listener
    {
    public:
        Voice();
        ~Voice() override;

        //==============================================================================
        // Audio thread
        void prepare (double sampleRate, int maxBlockSize);

        /** Writes the instrument's sound (the block's notes played where they fall). */
        void render (float* left, float* right, int numSamples, const juce::MidiBuffer& midi, double bpm) noexcept;

        /** Grex's global PITCH BEND RANGE (1..12 semitones) and LOW VELOCITY RESPONSE (0..100). */
        void setBendRange (float semitones) noexcept;
        void setLowVelocityResponse (float amount) noexcept;

        //==============================================================================
        // Message thread
        enum class Loading { none, loading, ready, failed };

        /** The user picked a sound: it plays with its saved settings (its .ins), as in Grex. */
        void selectInstrument (int flag);

        int getFlag() const;
        juce::String getInstrumentName() const;
        Loading getLoading() const noexcept           { return loading.load(); }
        bool isLoading() const noexcept               { return loading.load() == Loading::loading; }
        juce::String getProblem() const;

        /** The sound's settings: what the editor shows and changes. */
        SlotParams getParams() const;
        void setParams (const SlotParams& params);

        /** Save Preset: the sound's settings become its own, written into its preset in the
            presets' folders. The file written, or none (problem says why). */
        juce::File savePreset (juce::String& problem);

        /** The sound's base gain (dB, what GAIN's 100 means; GAIN goes back to 100) and base
            octave (where OCTAVE's 0 plays, -3..+3), written into its preset as well. The file
            written, or none (problem says why). */
        juce::File setBaseGain (float db, juce::String& problem);
        juce::File setBaseOctave (int octave, juce::String& problem);

        // The editor's keyboard, and what it shows.
        void editorNoteOn (int note, int velocity);
        void editorNoteOff (int note);
        void getSoundingNotes (uint32_t out[4]) const noexcept;
        void getSweetenerGr (float gr[4]) const noexcept;

        juce::ValueTree saveState() const;
        void loadState (const juce::ValueTree& state);

        /** Something the window shows changed (a sound read or failed, the library). */
        std::function<void()> onChanged;

    private:
        class Loader;
        struct Job;

        void libraryChanged() override;
        void start (int flag);                                       // stateLock held
        void install (const Job& job, std::shared_ptr<Betel::Channel::PresetVoice> made, const juce::String& error);
        void commit (const SlotParams& params);                      // stateLock held
        SlotParams voicedFor (int flag) const;
        static void takeCalibration (SlotParams& params, int flag);
        juce::File writePreset (std::function<void (SlotParams&)> change, bool whole, juce::String& problem);
        void notify();

        // The audio thread's
        std::unique_ptr<Betel::Channel> channel;
        Betel::Channel::PresetCache presetCache;
        std::unique_ptr<Betel::SectionSendFx> rack;
        juce::AudioBuffer<float> bus;
        int maxBlock = 512;
        bool pedalDown = false;
        std::array<bool, 128> sustained {}, held {};

        struct EditorNote { int note, velocity; };
        juce::AbstractFifo editorFifo { 256 };
        std::array<EditorNote, 256> editorNotes {};

        // The instrument swaps under this (the audio thread tries for it and plays silence
        // for the block when it can't have it); settings are atomics and need no lock.
        juce::SpinLock audioLock;

        // A Voice's state, between the message thread and its loader.
        mutable juce::CriticalSection stateLock;
        int flag = -1, generation = 0;
        SlotParams params;
        juce::String problem, instrumentName;
        std::shared_ptr<Betel::Channel::PresetVoice> heldBlob;   // freed outside the locks
        std::atomic<Loading> loading { Loading::none };

        std::unique_ptr<Loader> loader;

        JUCE_DECLARE_WEAK_REFERENCEABLE (Voice)
        JUCE_DECLARE_NON_COPYABLE (Voice)
    };
}
