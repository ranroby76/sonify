// D:\workspace\Fanan_web_hub\src\SonifySampler.h

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include "Plugin.h"

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

class SonifySampler;

//==============================================================================
/**
    The Sonify Sampler: Sonify's system tool for playing sampled instruments (SFZ), Sampler Arena's
    SFZ Player brought over. It goes on a track's instrument slot (or into a container) and plays
    an SFZ (the Sampling Studio's instruments, Sampler Arena's, any other), set up in its own
    window.

    A note plays one region: the one whose keys and velocities take it (the narrowest when
    several do), its sample pitched from its key, louder the harder it's played up to the top of
    its velocity range (amp_veltrack). Regions with seq_length take turns (round robins). Poly, or
    Mono: Legato (a new note glides from the one held, its sound going on: Sampler Arena's Hold
    Stolen), Retrigger New (the sound starts again, gliding from the last note) or Retrigger
    Stolen (the envelopes start again, the sound going on); letting go goes back to the note still
    held. Glide (portamento), in semitones. The sustain pedal holds notes (Poly).

    Its sound: an amp envelope (AHDSR), a filter (low pass, high pass, band pass or notch) with
    its own envelope, key tracking and resonance, a pitch envelope, pitch bend, pitch key and
    velocity tracking, and three LFOs (tremolo, filter, vibrato), as in Sampler Arena; an SFZ's
    own settings (ampeg_*, the LFOs, cutoff, resonance, fil_type, fil_keytrack, bend_up) are taken
    when it's loaded from the window. They're parameters, so they're automated and MIDI Link
    reaches them. Its settings are kept with the song (with the SFZ's file), and as presets
    (.sfzp, Sampler Arena's own format: either app opens the other's). Sampler Arena's Loop Sync
    isn't here.

    The SFZ and its samples are read on a thread of the sampler's own, all of them into memory;
    the last instrument plays until the new one is read. A mixdown waits for it.

    Threads: the instrument and the window on the message thread; process() on the audio thread.
*/
namespace Samplers
{
    inline constexpr const char* playerId = "sonify.builtin.sampler";
    inline constexpr const char* presetExtension = ".sfzp";

    Plugin::Descriptor describe();

    /** Where Sonify keeps the user's things: sampled instruments are in its Samples folder. */
    void setUserFolder (std::function<juce::File()> userData);
    juce::File folder();

    /** The instrument sampled last (the newest SFZ in the Samples folder, or the one the Sampling
        Studio made last wherever it went, when that's newer), or none. */
    juce::File latest();

    /** The Sampling Studio made this one (in any folder): latest() knows it. */
    void setLatest (const juce::File& sfz);

    /** The sampler's window: made by SamplerScreen.cpp in the app (none in the engine tests). */
    struct Screen
    {
        virtual ~Screen() = default;
        virtual void show (const juce::String& title) = 0;
        virtual void hide() = 0;
        virtual bool isShowing() const = 0;
    };

    using ScreenFactory = std::function<std::unique_ptr<Screen> (SonifySampler&)>;
    void setScreenFactory (ScreenFactory factory);
}

//==============================================================================
class SonifySampler final : public Plugin
{
public:
    SonifySampler();
    ~SonifySampler() override;

    //==============================================================================
    // Its parameters: ids kept with songs, values in Sampler Arena's units.
    enum Param : uint32_t
    {
        volume = 1,                 // gain 0..2
        octave,                     // -3..3
        playMode,                   // 0 Poly, 1 Mono
        monoMode,                   // 0 Legato, 1 Retrigger New, 2 Retrigger Stolen
        glide,                      // ms
        bendRange,                  // semitones
        ampAttack, ampHold, ampDecay, ampSustain, ampRelease,                         // ms, sustain 0..1
        filterType,                 // 0 low pass, 1 high pass, 2 band pass, 3 notch
        cutoff,                     // Hz
        resonance,                  // 0..0.99
        filterKeytrack,             // cents per key from C4, 0..100
        filterAttack, filterHold, filterDecay, filterSustain, filterRelease,           // ms, sustain 0..1
        filterAmount,               // -1..1 (the envelope's reach: up to 10 kHz)
        pitchAttack, pitchHold, pitchDecay, pitchSustain, pitchRelease,               // ms, sustain 0..1
        pitchDepth,                 // semitones, -24..24
        pitchKeytrack,              // %, 0..200 (100: a semitone a key)
        pitchVeltrack,              // cents at full velocity, -100..100
        ampLfoRate, ampLfoDepth, ampLfoDelay,                                         // Hz, 0..1, ms
        filterLfoRate, filterLfoDepth, filterLfoDelay,                                // Hz, 0..1, ms
        pitchLfoRate, pitchLfoDepth, pitchLfoDelay                                    // Hz, semitones, ms
    };

    static constexpr int numParams = static_cast<int> (pitchLfoDelay);

    double get (Param id) const noexcept;
    void set (Param id, double value);                  // as the page, a song or the window sets it
    double getDefault (Param id) const noexcept;

    //==============================================================================
    // The instrument, message thread.
    /** Reads an SFZ and its samples on the sampler's own thread; it plays once they're read.
        takeSettings: the SFZ's own settings (envelopes, LFOs, filter) become the sampler's. */
    void loadInstrument (const juce::File& sfz, bool takeSettings = true);
    void clearInstrument();

    enum class Loading { none, loading, ready, failed };
    Loading getLoading() const noexcept                  { return loading.load(); }
    juce::File getInstrumentFile() const;                // the one playing, or being read
    juce::String getProblem() const;                     // why the last one couldn't be read

    /** The instrument playing, in a few words ("Grand Piano: 61 keys, 244 samples"). */
    juce::String describeInstrument() const;

    /** The SFZ again with the sampler's settings in its <global> (Sampler Arena's Save SFZ). */
    bool saveInstrument (const juce::File& file);

    /** Presets: Sampler Arena's .sfzp (its settings and the SFZ's file). */
    bool savePreset (const juce::File& file);
    bool loadPreset (const juce::File& file);

    //==============================================================================
    // Plugin
    juce::String getFormatName() const override                          { return "Built-in"; }
    const Descriptor& getDescriptor() const noexcept override            { return descriptor; }
    const std::vector<Descriptor>& getPluginsInFile() const noexcept override { return inFile; }
    const juce::File& getSourceFile() const noexcept override;
    Role getRole() const noexcept override                               { return Role::instrument; }
    int getMainInputChannels() const noexcept override                   { return 0; }
    int getMainOutputChannels() const noexcept override                  { return 2; }

    bool canSaveState() const noexcept override                          { return true; }
    bool saveState (juce::MemoryBlock& destination) override;
    bool loadState (const juce::MemoryBlock& source) override;
    bool consumeStateChanged() noexcept override                         { return stateChanged.exchange (false); }

    const std::vector<Parameter>& getParameters() const noexcept override { return parameters; }
    int findParameter (uint32_t id) const noexcept override;
    double getParameterValue (int index) const noexcept override;
    double readParameterValue (int index) override                       { return getParameterValue (index); }
    juce::String getParameterText (int index, double value) override;
    double queueParameter (int index, double value) override;
    void setParameterFromAudio (uint32_t paramId, int& indexHint, double value) noexcept override;

    bool activate (double sampleRate, int maxBlockSize) override;
    void deactivate() override                                           { active.store (false); }
    bool isActive() const noexcept override                              { return active.load(); }

    void process (const float* inLeft, const float* inRight, float* outLeft, float* outRight,
                  int numSamples, const juce::MidiBuffer& midi, const clap_event_transport_t* transport) noexcept override;

    ScreenKind getScreenKind() const noexcept override                   { return ScreenKind::native; }
    bool showNativeScreen (const juce::String& title) override;
    void hideNativeScreen() override;

    struct Instrument;
    struct Voice;

private:
    class Loader;

    void install (std::unique_ptr<Instrument> instrument);   // the loader's thread, or the message thread
    void applySettings (const std::vector<std::pair<uint32_t, double>>& settings);

    // Audio thread
    void startNote (int note, int velocity) noexcept;
    void stopNote (int note) noexcept;
    void releaseAll (float releaseMs) noexcept;
    void pedal (bool down) noexcept;
    int findRegion (int note, int velocity) noexcept;
    Voice* freeVoice() noexcept;
    Voice* soundingVoice() noexcept;
    void startVoice (Voice& voice, int note, int velocity, int region, double glideFrom) noexcept;
    void glideTo (Voice& voice, int note, int region) noexcept;
    void retrigger (Voice& voice) noexcept;
    void render (Voice& voice, float* left, float* right, int numSamples) noexcept;

    Descriptor descriptor;
    std::vector<Descriptor> inFile;
    std::vector<Parameter> parameters;
    std::array<std::atomic<double>, numParams + 1> values {};
    std::atomic<bool> stateChanged { false }, active { false };

    // The instrument: swapped under the lock (the audio thread tries for it, and plays silence
    // for a block when it can't have it).
    juce::SpinLock instrumentLock;
    std::unique_ptr<Instrument> instrument;
    std::atomic<bool> instrumentChanged { false };
    std::atomic<Loading> loading { Loading::none };
    juce::File loadingFile;
    juce::String problem, description;
    int loadGeneration = 0;                      // one more for each instrument asked for (or cleared): a reading for an older one installs nothing
    mutable juce::CriticalSection fileLock;      // loadingFile, problem, description and loadGeneration, between the loader and the window
    std::unique_ptr<Loader> loader;

    // The audio thread's own
    double sampleRate = 48000.0;
    static constexpr int maxVoices = 128;
    std::unique_ptr<std::array<Voice, maxVoices>> voices;
    std::vector<int> heldNotes;                  // Mono: the keys held (as they came in, before Octave), last pressed last
    float bend = 0.0f;                           // -1..1
    bool pedalDown = false;
    juce::uint32 voiceAge = 0;

    std::unique_ptr<Samplers::Screen> screen;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SonifySampler)
};
