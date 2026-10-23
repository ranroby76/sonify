// D:\workspace\Fanan_web_hub\src\VoiceControl.h

#pragma once

#include "AudioEngine.h"
#include "FananServer.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>

#include <array>
#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <set>
#include <vector>

//==============================================================================
/**
    Finds the phrases in what one mic hears: speech told apart from silence by its level
    against the room's own noise (the quietest moment of the last few seconds).

    The sound is filtered and resampled to 16 kHz mono, as speech recognition takes it. A
    phrase begins after 60 ms above the noise, with half a second from before it (so its first
    sound isn't lost), and ends after 700 ms of quiet. One too short to be a word is left out;
    one longer than 25 seconds is cut there. While Sonify waits for "Sonify", a long phrase also
    gives its head (its first three seconds) as it goes on: enough to hear how it begins. One
    let go after its head gives only its end (its last three seconds), to hear whether it ends
    with a command: "... yeah, that's it. Sonify, stop, over."

    One thread, and nothing else from Sonify: it can be tested on its own.
*/
class PhraseFinder final
{
public:
    static constexpr int rate = 16000;
    static constexpr int frameLength = 320;       // 20 ms
    static constexpr int startFrames = 3;         // 60 ms of sound begins a phrase
    static constexpr int endFrames = 35;          // 700 ms of quiet ends it
    static constexpr int preRollFrames = 25;      // 500 ms from before it is kept
    static constexpr int tailFrames = 15;         // and 300 ms of the quiet after it
    static constexpr int minVoicedFrames = 10;    // 200 ms of sound at least: a short word
    static constexpr int headFrames = 150;        // 3 s
    static constexpr int endKeptFrames = 150;     // 3 s, of a phrase let go
    static constexpr int maxFrames = 1250;        // 25 s
    static constexpr int noiseFrames = 150;       // the room's noise: the quietest 20 ms of the last 3 s

    enum class Kind { whole, head, end };

    struct Phrase
    {
        int id = 0;
        Kind kind = Kind::whole;        // all of it, its first seconds (while it goes on), or its last ones (once let go)
        std::vector<float> samples;     // 16 kHz mono
    };

    /** A whole phrase's end, as a phrase let go gives it. */
    static Phrase endOf (Phrase whole);

    PhraseFinder();

    /** Starts afresh, for sound at this rate. */
    void prepare (double inputRate);

    /** Sound in, phrases out (added to found). hushed: Sonify's own beep is sounding, or just
        was: what the mic hears of it begins no phrase. */
    void process (const float* samples, int count, bool hushed, std::vector<Phrase>& found);

    /** Gives the heads of long phrases (while waiting for "Sonify"). */
    void setWantsHeads (bool shouldGiveHeads) noexcept      { wantsHeads = shouldGiveHeads; }

    /** The phrase under way didn't begin with "Sonify": only its end is kept. */
    void drop (int phraseId);

    bool isInPhrase() const noexcept                        { return inPhrase; }
    float getNoiseLevel() const noexcept                    { return noiseDb; }   // dBFS

private:
    void addSample (float sample, std::vector<Phrase>& found);
    void endFrame (std::vector<Phrase>& found);
    void endPhrase (std::vector<Phrase>& found, bool quietAtEnd);

    // To 16 kHz: low-passed below its Nyquist, then linear interpolation; then the rumble goes.
    juce::IIRFilter lowPass1, lowPass2, highPass;
    bool filtering = true, resampling = true;
    double step = 3.0;          // input samples for each 16 kHz one
    double position = 0.0;      // where the next 16 kHz sample falls, after the previous input one
    float previous = 0.0f;

    std::vector<float> frame, preRoll, phrase;
    std::array<float, noiseFrames> levels {};
    int levelsCount = 0, levelsAt = 0;
    float noiseDb = -90.0f;

    bool hushedNow = false, wantsHeads = true;
    bool inPhrase = false, dropped = false, headGiven = false;
    int loudRun = 0, quietRun = 0, voicedFrames = 0, phraseFrames = 0;
    int phraseId = 0, lastId = 0;
};

//==============================================================================
/**
    Voice control: the musician talks to Sonify from the recording room, into their mic.

    "Sonify" (among the first words) begins a command and "over" (the last word) ends it; a
    command can take a few breaths in between. Sonify answers with beeps in the headphones,
    never speech: "go on" when it heard "Sonify", then done, a question waiting on the screen,
    or a problem. Hebrew works too. Transport commands (record, stop, play, again, undo) are
    done at once; anything else goes to Sonify AI, as it was said.

    It listens to one song input (the mic the musician talks into) on a thread of its own. Each
    phrase it hears goes to Fanan's server to be written down, and nothing is kept. While
    waiting for "Sonify", a long phrase sends only its first and last seconds, and all of it
    only when it began with "Sonify". It listens while Fanan's server, the account and the audio
    device can work, and goes on by itself when they can again.

    Message thread, apart from the listener.
*/
class VoiceControl final : private juce::Thread,
                           private juce::Timer
{
public:
    enum class Stage { off, listening, command };

    /** What Sonify does at once, without Sonify AI: the transport, while the musician plays. */
    enum class Quick { none, record, stop, play, again, recordAgain, playAgain, undo };

    VoiceControl (AudioEngine& engine, FananServer& server);
    ~VoiceControl() override;

    /** On or off (it starts off). */
    void setEnabled (bool shouldListen);
    bool isEnabled() const noexcept                     { return enabled; }

    /** The mic's input changed: it listens afresh. */
    void inputChanged();

    Stage getStage() const noexcept                     { return stage; }

    /** Why it can't listen right now, in a few words for the page; empty when it can. */
    juce::String whyNot() const;

    /** The words of the command under way, or of the last one. */
    const juce::String& getHeard() const noexcept       { return heard; }

    /** What went wrong last, for the page; empty when nothing did. */
    const juce::String& getMessage() const noexcept     { return message; }
    void setMessage (const juce::String& text);

    /** A command: the words between "Sonify" and "over". */
    std::function<void (const juce::String& command)> onCommand;

    /** The stage, the words heard, the message or the reason it can't listen changed. */
    std::function<void()> onChanged;

    //==============================================================================
    // Reading what was said (public for tests).
    struct Words
    {
        juce::StringArray spoken;   // as written down, for Sonify AI
        juce::StringArray plain;    // the same words to compare: lower case, letters and digits only
    };

    static Words read (const juce::String& text);

    /** Where the command begins: after "Sonify" among the first words. -1 when it isn't there. */
    static int afterWake (const Words& words);

    /** After the last "Sonify", anywhere: for a phrase that ends with "over". -1 when it isn't there. */
    static int afterLastWake (const Words& words);

    /** How many of the last words say "over" (1, or 3 for "over and out"; 0: it doesn't end with it),
        counting from where the command begins. */
    static int stopLength (const Words& words, int from);

    /** The spoken words from..to, without the commas around them. */
    static juce::String join (const Words& words, int from, int to);

    static Quick quickCommand (const juce::String& command);

    /** 16 kHz 16-bit mono WAV, its loudest sound brought up near full scale. */
    static juce::MemoryBlock toWav (const std::vector<float>& samples);

    static constexpr int wakeWithin = 3;                        // "Sonify" among the first three words
    static constexpr juce::uint32 hushMs = 150;                 // after a beep, for its echo in the room
    static constexpr juce::uint32 commandTimeoutMs = 12000;     // quiet after the last words, without "over"
    static constexpr juce::uint32 commandLimitMs = 90000;       // one command at most
    static constexpr size_t maxWaiting = 3;                     // phrases waiting to be written down, while waiting for "Sonify"

private:
    void run() override;               // the listener
    void timerCallback() override;

    void updateListening();
    void startListening();
    void stopListening();
    void accept (PhraseFinder::Phrase phrase);
    void sendNext();
    void onWords (int phraseId, PhraseFinder::Kind kind, const juce::Result& result, const juce::var& answer);
    void forget (int phraseId);
    void addWords (const Words& words, int from, bool woke);
    void backToListening();
    void giveUp (const juce::String& why);
    void changed();

    AudioEngine& engine;
    FananServer& server;

    // Message thread
    bool enabled = false, listening = false;
    Stage stage = Stage::off;
    juce::String heard, message, lastWhy;
    juce::StringArray parts;                     // the command so far
    juce::uint32 commandStarted = 0, lastWordsAt = 0;
    std::deque<PhraseFinder::Phrase> waiting;    // to be written down, in order
    bool writing = false;                        // one at a time
    int session = 0;                             // moves on when listening stops: answers from before are dropped
    std::set<int> unwanted;                      // phrases whose head didn't begin with "Sonify": only their end is wanted
    bool failing = false;                        // the last phrase couldn't be written down (beeped once)
    juce::Time pausedUntil;                      // the account's listening for the day is used up
    juce::String pausedWhy;

    // Shared with the listener
    std::atomic<double> inputRate { 0.0 };
    std::atomic<bool> wantHeads { true }, hearing { false }, restart { false };
    std::atomic<int> dropRequest { 0 };
    juce::CriticalSection foundLock;
    std::vector<PhraseFinder::Phrase> found;

    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoiceControl)
};
