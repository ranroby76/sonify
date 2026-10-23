// D:\workspace\Fanan_web_hub\src\MidiSync.h

#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include "Transport.h"

#include <array>
#include <atomic>
#include <memory>
#include <vector>

//==============================================================================
/**
    MIDI clock and MIDI timecode for external devices, as Studio One sends them (Settings >
    External Devices: Send MIDI Clock, Use MIDI Clock Start, Send MIDI Timecode). Sonify is
    the master: drum machines, arpeggiators and sequencers follow its tempo and position.

    Clock: 24 ticks a quarter note while Sonify plays. Playing from the top of the song sends
    Start; from anywhere else, where (Song Position Pointer) and then Continue, except to a
    device that takes Start only ("Use MIDI Clock Start"). Stopping sends Stop. A jump while
    playing (the loop going round) and a new position while stopped send Song Position.

    Timecode (MTC): quarter-frame messages while Sonify plays, and a full-frame message wherever
    playing starts, jumps, or the song position is moved. The song's start is 00:00:00:00.

    The audio thread works out, block by block, what's due and when it's heard (the block's
    time plus the device's output latency), and queues it. A thread of its own sends each
    message at its time, so the audio thread never waits for a MIDI port or allocates.
*/
class MidiSync final : private juce::Thread
{
public:
    /** MTC frame rates, numbered as the timecode messages number them. */
    enum class Rate { fps24 = 0, fps25 = 1, fps2997drop = 2, fps30 = 3 };

    struct Target
    {
        juce::String output;            // a MIDI output port, by name
        bool clock = false;             // MIDI clock
        bool clockStart = false;        // Start only, never Song Position + Continue
        bool timecode = false;          // MIDI timecode
    };

    /** One MIDI message, queued with the time it's sent at. */
    struct Message
    {
        double time = 0.0;              // Time::getMillisecondCounterHiRes()
        juce::uint32 targets = 0;       // which outputs, one bit each
        int size = 0;
        std::array<juce::uint8, 10> bytes {};
    };

    static constexpr int maxTargets = 16;

    //==============================================================================
    /** What each block sends: the audio thread's half, on its own so it can be tested. */
    class Planner
    {
    public:
        /** This block's messages, in time order, into out (at most maxOut); returns how many.
            clock: the outputs that get clock; startOnly: those of them that take Start only;
            timecode: the outputs that get MTC. blockTime: when the block began
            (Time::getMillisecondCounterHiRes()); delayMs: until it's heard. */
        int plan (const Transport& transport, double sampleRate, double blockTime, double delayMs,
                  juce::uint32 clock, juce::uint32 startOnly, juce::uint32 timecode, Rate rate,
                  Message* out, int maxOut) noexcept;

        /** Stop, for the clock outputs that are running. */
        int halt (double now, Message* out, int maxOut) noexcept;

        /** Forgets what the devices were told: the next block starts them afresh. */
        void reset() noexcept;

        static double framesPerSecond (Rate rate) noexcept;

        /** A frame count as hours, minutes, seconds and frames (drop-frame numbering at 29.97). */
        static std::array<int, 4> timecodeOf (juce::int64 frame, Rate rate) noexcept;

    private:
        struct Out;

        bool running = false;           // the clock outputs have been started
        juce::uint32 runningClock = 0;  // the ones that were
        double lastEnd = 0.0;           // where the last block played up to, quarter notes
        double lastEndSeconds = 0.0;    // and in the song's time
        double toldStopped = -1.0;      // the position last told while stopped (-1: none yet)
        double toldStoppedSeconds = -1.0;   // and its time (a tempo changed before it moves it)
        juce::int64 nextTick = 0;       // the next clock tick: tick k is at k/24 quarter notes
        juce::int64 nextQuarter = 0;    // the next MTC quarter frame: q is at q / (4 fps) seconds
        Rate lastRate = Rate::fps30;

        /** Counting again from here (quarter notes, and the song's time in seconds). */
        void resync (double ppq, double seconds, Rate rate) noexcept;
    };

    //==============================================================================
    MidiSync();
    ~MidiSync() override;

    /** Message thread: who gets what, and the timecode's frame rate. Opens and closes the MIDI
        outputs as needed (outputs no device sends to stay closed, for other apps to use). */
    void setTargets (const std::vector<Target>& targets, Rate rate);

    /** The outputs that couldn't be opened (another app may hold them). Message thread. */
    juce::StringArray getProblems() const;

    /** Audio thread, once per device block, after the transport has planned it. */
    void process (const Transport& transport, double sampleRate, double blockTime, double delayMs) noexcept;

    /** The device stops playing the song (a mixdown takes over, the device stops): devices
        running are told to stop. Audio thread, or once the device has stopped. */
    void halt (double now) noexcept;

    static Rate rateFromName (const juce::String& name) noexcept;

private:
    void run() override;
    void queueMessages (Message* messages, int count) noexcept;
    void send (const Message& message);

    Planner planner;                                // audio thread
    std::array<Message, 512> scratch {};            // audio thread: this block's messages

    static constexpr int queueSize = 4096;
    juce::AbstractFifo queue { queueSize };         // audio thread writes, the sender reads
    std::array<Message, queueSize> queued {};

    std::atomic<juce::uint32> clockOutputs { 0 }, startOnlyOutputs { 0 }, timecodeOutputs { 0 };
    std::atomic<int> rateSetting { static_cast<int> (Rate::fps30) };
    std::atomic<bool> restart { false };            // the outputs changed: the planner starts afresh

    juce::CriticalSection outputLock;               // the sender and the message thread
    std::array<std::unique_ptr<juce::MidiOutput>, maxTargets> outputs;
    std::array<juce::String, maxTargets> outputNames;
    juce::StringArray problems;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiSync)
};
