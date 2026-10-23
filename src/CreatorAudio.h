// D:\workspace\Fanan_web_hub\src\CreatorAudio.h

#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "AudioEngine.h"
#include "WasmPlugin.h"

#include <array>
#include <atomic>
#include <functional>

//==============================================================================
/**
    A small sine synth, so the keyboard can be tried before a plugin exists. Allocation-free
    and lock-free: everything happens on the audio thread.
*/
class TestTone final
{
public:
    void prepare (double newSampleRate) noexcept;
    void reset() noexcept;

    /** Adds into left/right (right may be null). Notes are applied at their sample positions. */
    void render (float* left, float* right, int numSamples, const juce::MidiBuffer& midi) noexcept;

private:
    struct Voice
    {
        int note = -1;
        double phase = 0.0, step = 0.0;
        float level = 0.0f, target = 0.0f, velocity = 0.0f;
        std::uint32_t started = 0;
    };

    void handle (const juce::MidiMessage& message) noexcept;
    void renderVoices (float* left, float* right, int start, int end) noexcept;

    static constexpr int numVoices = 12;
    std::array<Voice, numVoices> voices {};
    double sampleRate = 48000.0;
    float attackCoeff = 0.0f, releaseCoeff = 0.0f;
    std::uint32_t counter = 0;
};

//==============================================================================
/**
    The Plugins Creator Studio's audio side (Stella AI Studio's engine, inside Sonify). Sonify's
    engine owns the audio device and the MIDI inputs: this plays the plugin being made (live, in
    its sandbox) in Sonify's audio callback, on the Audition output after everything else, fed by
    Sonify's live notes while the studio listens (it's in front), by its GUI's keyboard, and for
    an effect by the device's first two inputs, in time with the song (its tempo and position,
    for tempo sync). With no plugin it plays the test tone.

    It keeps the on-screen keyboard's state and the output meter (its own sound alone), and plays
    a chime when Sonify AI has finished (heard wherever the user is).
*/
class CreatorAudio final : public AudioEngine::CreatorPreview,
                           private juce::Timer
{
public:
    explicit CreatorAudio (AudioEngine& sonifyEngine);
    ~CreatorAudio() override;

    juce::AudioDeviceManager& getDeviceManager() noexcept   { return sonify.getDeviceManager(); }
    juce::MidiKeyboardState& getKeyboardState() noexcept    { return keyboardState; }

    /** Whether the studio is in front: it takes the live notes then, and plays. Message thread. */
    void setListening (bool shouldListen);
    bool isListening() const noexcept                       { return listening.load(); }

    /** The highest peak on an output channel since the last call (0..1, can exceed 1). */
    float takePeak (int channel) noexcept;

    /** 0..1: how much of each audio block's time Sonify's callback used. */
    double getCpuLoad() const;

    /** The device in three short lines: its name, the sample rate, the buffer
        ("Focusrite USB ASIO", "48 kHz", "128 samples · 2.7 ms"). */
    juce::StringArray describeDeviceLines() const;

    /** e.g. "ASIO · Focusrite USB ASIO · 48 kHz · 128 samples", or "No audio device". */
    juce::String describeDevice() const;

    /** A short two-note chime over whatever is playing (Sonify AI has finished). False if no
        audio device is running, so nothing can be heard. */
    bool playChime();

    void setTestToneEnabled (bool shouldBeOn) noexcept    { testToneOn.store (shouldBeOn); }
    bool isTestToneEnabled() const noexcept               { return testToneOn.load(); }

    /** Panic: silences everything that's sounding. The plugin gets "all notes off" (every
        voice is let go, stuck ones too), the keyboards' held keys go up, the test tone stops. */
    void allNotesOff();

    /** Message thread. Swaps the playing plugin (null: none); the old one is deleted here. */
    void setPlugin (std::unique_ptr<WasmPlugin> newPlugin);

    /** Message thread only: the plugin is only ever replaced there. */
    WasmPlugin* getPlugin() const noexcept    { return plugin.get(); }

    double getCurrentSampleRate() const noexcept  { return currentRate.load(); }
    int getCurrentBlockSize() const noexcept      { return currentBlockSize.load(); }

    /** Called on the message thread when the device changes: another one, or a new sample rate
        or block size. */
    std::function<void()> onDeviceChanged;

    // AudioEngine::CreatorPreview
    void renderPreview (const juce::MidiBuffer& live, const float* inLeft, const float* inRight,
                        float* left, float* right, int numSamples, const clap_event_transport_t& song) noexcept override;

private:
    void timerCallback() override;   // follows the device's rate and block size
    void prepare (double rate, int blockSize);
    static void raisePeak (std::atomic<float>& peak, float value) noexcept;

    AudioEngine& sonify;
    juce::MidiKeyboardState keyboardState;
    juce::MidiBuffer midiBuffer;
    TestTone testTone;
    std::atomic<bool> testToneOn { true };
    std::atomic<bool> listening { false };

    /** The chime, on the audio thread: two soft bell notes, a fifth apart. */
    struct Chime
    {
        void start (double rate) noexcept    { sampleRate = rate; position = 0; active = true; }
        void render (float* left, float* right, int numSamples) noexcept;

        double sampleRate = 48000.0;
        int position = 0;
        bool active = false;
    };

    Chime chime;
    std::atomic<bool> chimeRequested { false };

    // The plugin's own block (it's added to Sonify's output after), sized as the device's
    // blocks are: the audio thread only uses it while it holds the plugin's lock.
    juce::AudioBuffer<float> scratch;

    juce::SpinLock pluginLock;
    std::unique_ptr<WasmPlugin> plugin;
    std::array<WasmPlugin::MidiEvent, 512> pluginEvents {};

    std::atomic<double> currentRate { 48000.0 };
    std::atomic<int> currentBlockSize { 512 };
    double preparedRate = 0.0;            // message thread
    int preparedBlockSize = 0;
    juce::String shownDevice;             // the device as the top bar last showed it

    std::atomic<bool> silenceRequested { false };
    std::atomic<bool> panicRequested { false };
    std::array<std::atomic<float>, 2> peaks { 0.0f, 0.0f };

    static constexpr int midiBufferBytes = 16384;
    static constexpr int largestBlock = 8192;   // samples: the scratch always has room for a device block

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CreatorAudio)
};
