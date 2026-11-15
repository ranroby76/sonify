// D:\workspace\Fanan_web_hub\src\timberwolf\PluginProcessor.h
#pragma once
//==============================================================================
//  Timber Wolf  -  Sonify's guitar multi-effect.
//
//  Up to 16 effects in any order, with as many splits (two parallel paths and
//  their blend) as they leave room for; the chain shown as a graph of family
//  badges; eight footswitches and two expression pedals, all MIDI learnable.
//
//  What the host sees, per slot (one effect per slot, 16 slots):
//      S<n> On, S<n> K1 .. K12     the effect's on switch and its knobs (0..1)
//  plus Input, Output, EXP 1 and EXP 2.  A slot's knobs are whatever effect is
//  in it (an effect keeps its slot while it's in the chain, wherever it moves),
//  so automation and Fanan AI reach every knob.
//
//  The plugin's state, a song's and a preset's: the chain (TwGraph), every
//  slot's values, the four snapshots, the footswitches' jobs; and, Timber
//  Wolf-wide rather than per preset, what MIDI each footswitch and pedal learned.
//
//  Threads: the graph, presets, snapshots and footswitch jobs on the message
//  thread; processBlock on the audio thread (MIDI from a foot controller is
//  read there and handed to the message thread through a queue, except the
//  expression pedals, which move their knobs at once).
//==============================================================================
#include <juce_audio_processors/juce_audio_processors.h>

#include "TwEngine.h"
#include "TwTuner.h"

#include <array>
#include <atomic>
#include <functional>

namespace tw
{
    inline constexpr int numSwitches = 8;
    inline constexpr int numPedals = 2;
    inline constexpr int numSnapshots = 4;

    /** What a footswitch does. */
    struct Footswitch
    {
        enum class Job { none = 0, toggle, presetUp, presetDown, snapshot, tap, tuner };

        Job job = Job::none;
        std::vector<int> uids;      // toggle: the effects it switches (all together)
        int snapshot = 0;           // snapshot: which one (0..3)
        juce::String label;         // "" : made from its job
    };

    /** What an expression pedal moves: one knob of one effect, from min to max (0..1 travel). */
    struct Pedal
    {
        int uid = 0;                // 0: nothing
        int knob = 0;
        float from = 0.0f, to = 1.0f;
    };

    /** A MIDI message learned: a controller (CC) or a note, on a channel (0: any). */
    struct Learned
    {
        enum class Kind { none = 0, cc, note };

        Kind kind = Kind::none;
        int number = 0;
        int channel = 0;

        bool matches (const juce::MidiMessage& m) const noexcept
        {
            if (kind == Kind::none || (channel != 0 && m.getChannel() != channel))
                return false;

            if (kind == Kind::cc)
                return m.isController() && m.getControllerNumber() == number;

            return (m.isNoteOn() || m.isNoteOff()) && m.getNoteNumber() == number;
        }

        juce::String text() const
        {
            if (kind == Kind::none) return "-";
            return (kind == Kind::cc ? "CC " : "Note ") + juce::String (number)
                   + (channel != 0 ? " ch " + juce::String (channel) : juce::String());
        }
    };

    /** A snapshot: every slot's on switch and knobs, and every parallel's settings. */
    struct Snapshot
    {
        bool stored = false;
        std::array<float, maxBlocks> on {};
        std::array<std::array<float, maxKnobs>, maxBlocks> knobs {};   // 0..1
        std::map<int, ParallelSettings> parallels;
        juce::String name;
    };
}

//==============================================================================
class TimberWolfProcessor final : public juce::AudioProcessor,
                                  private juce::Timer
{
public:
    TimberWolfProcessor();
    ~TimberWolfProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                         { return true; }

    const juce::String getName() const override             { return "Timber Wolf"; }
    bool acceptsMidi() const override                       { return true; }
    bool producesMidi() const override                      { return false; }
    bool isMidiEffect() const override                      { return false; }
    double getTailLengthSeconds() const override            { return 8.0; }

    int getNumPrograms() override                           { return 1; }
    int getCurrentProgram() override                        { return 0; }
    void setCurrentProgram (int) override                   {}
    const juce::String getProgramName (int) override        { return presetName; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    //  The chain (message thread).
    //==========================================================================
    const tw::Graph& graph() const noexcept                 { return chain; }

    /** Edits: each rebuilds what plays and tells the editor (onChainChanged). */
    int addBlock (const juce::String& type, const tw::Path& path, int index);
    int addParallel (const tw::Path& path, int index);
    int splitAround (int uid);
    bool removeNode (int uid);
    bool unsplit (int parallelUid);
    bool moveNode (int uid, const tw::Path& path, int index);
    bool swapPaths (int parallelUid);
    bool replaceBlock (int uid, const juce::String& type);
    void setParallel (int uid, const tw::ParallelSettings& s);

    /** An effect's info and slot (nullptr / -1: not in the chain). */
    const tw::BlockInfo* infoOf (int uid) const;
    int slotOf (int uid) const;

    /** An effect's knob, 0..1 travel and as text; set from the panel (host told). */
    float knobValue (int uid, int knob) const;
    juce::String knobText (int uid, int knob) const;
    void setKnobValue (int uid, int knob, float norm);
    void beginKnobGesture (int uid, int knob);
    void endKnobGesture (int uid, int knob);
    void resetKnob (int uid, int knob);     // back to its default

    bool isOn (int uid) const;
    void setOn (int uid, bool on);

    float meterOf (int uid) const                           { return engine.meterOf (uid); }
    bool isSounding (int uid) const                         { return engine.isSounding (uid); }

    std::function<void()> onChainChanged;    // the editor's: the graph changed (structure or preset)
    std::function<void()> onValuesChanged;   // knobs or switches moved by something else

    //==========================================================================
    //  Global
    //==========================================================================
    juce::AudioParameterFloat& inputGain() noexcept         { return *input; }
    juce::AudioParameterFloat& outputGain() noexcept        { return *output; }
    float inputLevel() const noexcept                       { return meterIn.load(); }
    float outputLevel() const noexcept                      { return meterOut.load(); }

    /** The tempo the delays and modulations follow: the host's, or the tapped one. */
    double currentBpm() const noexcept                      { return bpmNow.load(); }
    bool usesTapTempo() const noexcept                      { return tapTempo.load(); }
    void setUseTapTempo (bool use)                          { tapTempo.store (use); }
    void tap();

    tw::Tuner& tuner() noexcept                             { return tunerAnalyzer; }
    bool isTuning() const noexcept                          { return tuning.load(); }
    void setTuning (bool on)                                { tuning.store (on); }
    bool muteWhileTuning = true;

    //==========================================================================
    //  Footswitches, pedals, snapshots
    //==========================================================================
    const tw::Footswitch& footswitch (int i) const          { return switches[(size_t) juce::jlimit (0, tw::numSwitches - 1, i)]; }
    void setFootswitch (int i, const tw::Footswitch& f);
    void pressFootswitch (int i);                           // from the panel or MIDI
    bool isFootswitchLit (int i) const;
    juce::String footswitchLabel (int i) const;

    const tw::Pedal& pedal (int i) const                    { return pedals[(size_t) juce::jlimit (0, tw::numPedals - 1, i)]; }
    void setPedal (int i, const tw::Pedal& p);
    float pedalPosition (int i) const;                      // 0..1
    void setPedalPosition (int i, float pos);               // from the panel

    /** MIDI learn: the next CC or note in is what the footswitch (0..7) or pedal (8..9) answers. */
    void learn (int target);
    int learning() const noexcept                           { return learnTarget.load(); }
    void forget (int target);
    tw::Learned learned (int target) const;

    const tw::Snapshot& snapshot (int i) const              { return snapshots[(size_t) juce::jlimit (0, tw::numSnapshots - 1, i)]; }
    void storeSnapshot (int i);
    void recallSnapshot (int i);
    void renameSnapshot (int i, const juce::String& name);
    void clearSnapshot (int i);
    int currentSnapshot() const noexcept                    { return snapshotNow; }

    //==========================================================================
    //  Presets
    //==========================================================================
    int numFactoryPresets() const;
    juce::String factoryPresetName (int index) const;
    void loadFactoryPreset (int index);

    juce::Result savePresetFile (const juce::File& file, const juce::String& name);
    bool loadPresetFile (const juce::File& file);
    juce::Result saveBankFile (const juce::File& file);
    int loadBankFile (const juce::File& file);              // presets written to the user folder; how many

    /** Steps through factory presets then the user's (footswitches). */
    void stepPreset (int delta);

    const juce::String& getPresetName() const noexcept      { return presetName; }
    const juce::File& getPresetFile() const noexcept        { return presetFile; }
    int getFactoryIndex() const noexcept                    { return factoryIndex; }
    bool isModified() const noexcept                        { return modified; }

    /** The whole rig as a preset (the chain, values, snapshots, footswitch jobs). */
    juce::var rigToVar() const;
    void rigFromVar (const juce::var& v);

    float getUiScale() const noexcept                       { return uiScale; }
    void setUiScale (float s);

private:
    class SlotParam;

    void timerCallback() override;
    void rebuild();
    void giveSlotDefaults (int uid);
    void markModified()                                     { modified = true; }
    void runFootswitch (int i);
    void loadGlobalLearned();
    void saveGlobalLearned() const;

    tw::Graph chain;
    tw::Engine engine;

    std::array<std::array<SlotParam*, tw::maxKnobs>, tw::maxBlocks> knobParams {};
    std::array<SlotParam*, tw::maxBlocks> onParams {};
    juce::AudioParameterFloat* input = nullptr;
    juce::AudioParameterFloat* output = nullptr;
    std::array<juce::AudioParameterFloat*, tw::numPedals> pedalParams {};

    std::array<tw::Footswitch, tw::numSwitches> switches {};
    std::array<tw::Pedal, tw::numPedals> pedals {};
    std::array<tw::Snapshot, tw::numSnapshots> snapshots {};
    int snapshotNow = -1;

    std::array<std::atomic<const tw::BlockInfo*>, tw::maxBlocks> slotInfo {};   // what's in each slot (any thread reads)

    std::array<std::atomic<int>, tw::numSwitches + tw::numPedals> learnedKind {}, learnedNumber {}, learnedChannel {};
    std::atomic<int> learnTarget { -1 };
    int learnedSeen = 0;
    std::array<std::atomic<int>, tw::numPedals> pedalCc {};

    // What each pedal moves, as the audio thread reads it.
    std::array<std::atomic<int>, tw::numPedals> pedalSlot {}, pedalKnob {};
    std::array<std::atomic<float>, tw::numPedals> pedalFrom {}, pedalTo {};
    std::array<float, tw::numPedals> pedalLast {};
    std::atomic<bool> pedalMoved { false };

    juce::AudioBuffer<float> monoRight;   // a mono output's second side, to run the chain in stereo

    // MIDI from the audio thread to the message thread: footswitch presses.
    juce::AbstractFifo pressFifo { 64 };
    std::array<int, 64> pressQueue {};
    std::atomic<int> learnedFromAudio { 0 };   // bumped when the audio thread learned something

    std::atomic<float> meterIn { 0.0f }, meterOut { 0.0f };
    std::atomic<double> bpmNow { 120.0 };
    std::atomic<double> tappedBpm { 120.0 };
    std::atomic<bool> tapTempo { false };
    std::vector<double> taps;

    tw::Tuner tunerAnalyzer;
    std::atomic<bool> tuning { false };

    juce::String presetName { "Init" };
    juce::File presetFile;
    int factoryIndex = -1;
    bool modified = false;
    float uiScale = 0.85f;

    double rate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimberWolfProcessor)
};
