// D:\workspace\Fanan_web_hub\src\zodiak\PluginProcessor.h
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <memory>
#include "ZodiakEngine.h"
#include "ZodiakSet.h"

//==============================================================================
//  Zodiak — Fanan's groove box (VST3 and CLAP).
//
//  Eight channels alike: each one a VSTi (or several) loaded from file and its
//  graph of effects (ZodiakGraph.h), its macros, its patterns, a mixer strip;
//  the master's VST FX and the master (ZodiakEngine.h).
//
//  PRESETS are SETS (ZodiakSet.h), files in %APPDATA%\Fanan\Sonify\Zodiak\Sets.  The DAW's
//  preset list holds a single entry, the set playing now: the DAW's project
//  keeps that whole set inside it and plays it back exactly as it was saved.
//
//  Zodiak always comes up playable: a new instance (or a project that saved
//  nothing it can read) opens Default.zset, else the set built in.
//==============================================================================
class ZodiakProcessor final : public juce::AudioProcessor
{
public:
    ZodiakProcessor();
    ~ZodiakProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                          { return true; }

    const juce::String getName() const override              { return "Zodiak"; }
    bool acceptsMidi() const override                        { return true; }
    bool producesMidi() const override                       { return false; }
    bool isMidiEffect() const override                       { return false; }
    double getTailLengthSeconds() const override             { return 8.0; }

    // The DAW's preset list: the set playing now, and nothing else
    int getNumPrograms() override                            { return 1; }
    int getCurrentProgram() override                         { return 0; }
    void setCurrentProgram (int) override                    {}
    const juce::String getProgramName (int) override         { return info.name; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    //  The windows' side (message thread)
    //==========================================================================
    zodiak::Engine&        engine() noexcept                 { return *eng; }

    juce::String setName() const                             { return info.name; }
    const juce::File& setFile() const noexcept               { return currentSetFile; }

    /** What a channel plays, for the banner: its first VSTi's name (empty: none loaded). */
    juce::String instrumentName (int ch) const;

    /** The pattern a channel has chosen: bank 0..3, variation 0..7. */
    // The pattern chosen is the sequencer's (a MIDI controller learnt to a BANK or VAR knob
    // chooses there, from the audio thread): the bank and variation are read from it
    int  bank (int ch) const                                 { return eng->patterns().requested (juce::jlimit (0, zodiak::kNumChannels - 1, ch)) / 8; }
    int  variation (int ch) const                            { return eng->patterns().requested (juce::jlimit (0, zodiak::kNumChannels - 1, ch)) % 8; }
    void choosePattern (int ch, int bank, int variation);
    void syncPatternInfo();

    /** Whether the master's strip shows its VST FX. */
    bool isMasterOpen() const                                { return info.masterOpen; }
    void setMasterOpen (bool open)                           { info.masterOpen = open; }

    /** Plugins the last set held that weren't found (taking them clears them). */
    juce::StringArray takeMissingPlugins()                   { auto m = missing; missing.clear(); return m; }
    bool isRestoringPlugins() const noexcept                 { return restoreIndex < (int) toRestore.size(); }

    /** Bumped when a plugin comes in or goes (a graph's or the master's): the windows look again. */
    int  rackGeneration() const noexcept                     { return rackGen; }
    void rackChanged()                                       { ++rackGen; }

    /** Bumped whenever a set loads (the windows refresh everything). */
    int setGeneration() const noexcept                       { return generation; }

    /** A plugin from file into a graph's VSTi / VST FX node (the one there goes). */
    void loadNodePlugin (int ch, int uid, const zodiak::rack::Source& source, std::function<void (const juce::String& error)> done);

    juce::Result loadSet (const juce::File& file);
    juce::Result saveSet();                                  // over its file
    juce::Result saveSetAs (const juce::File& file);
    void         newSet();                                   // the built-in set

    /** The window's size, 0.3 .. 1.0 of the design size. */
    float getUiScale() const noexcept                        { return uiScale; }
    void  setUiScale (float scale);

    /** The Roll's look: bright (the default) or dark.  Kept for every Zodiak. */
    bool isRollDark() const noexcept                         { return rollDark; }
    void setRollDark (bool dark);

    /** The window's page: PLAY (the banner) or MIXER, kept while Zodiak is loaded. */
    bool mixerShown = false;

    //==========================================================================
    //  MIDI LINK (ZodiakLink.h).  Message thread.
    //==========================================================================
    zodiak::link::Map& links() noexcept                      { return eng->links(); }

    /** Links a source to a target, as a drop does (the map is saved straight after).
        The link's slot, or -1. */
    int  linkSource (const zodiak::link::Source& s, const zodiak::link::Target& t);

    /** MIDI learn onto a target: the next source moved is linked to it. */
    void startLinkLearn (const zodiak::link::Target& t);
    void cancelLinkLearn();
    bool isLinkLearning (const zodiak::link::Target& t) const;

    /** A named map: the map in force saved as it, or replaced by it. */
    juce::Result saveLinkMapAs (const juce::File& file);
    juce::Result loadLinkMap (const juce::File& file);

private:
    // MIDI LINK: the moves that wait for the message thread, MIDI learn, the map's file
    class LinkPump final : public juce::Timer
    {
    public:
        explicit LinkPump (ZodiakProcessor& p) : owner (p) {}
        void timerCallback() override                        { owner.pumpLinks(); }
    private:
        ZodiakProcessor& owner;
    };
    void pumpLinks();
    void watchGraphs();             // a graph changed: its links' settings found, the latency lined up

    void startUp();
    void setApplied();
    bool applySet (const juce::XmlElement& xml);
    void restoreNext();

    std::unique_ptr<zodiak::Engine>      eng = std::make_unique<zodiak::Engine>();
    std::array<int, zodiak::kNumChannels> seenGraphGen {};
    zodiak::sets::Info info;
    juce::File         currentSetFile;
    float              uiScale    = 0.55f;
    bool               rollDark   = false;
    int                generation = 0;
    int                rackGen    = 0;

    zodiak::link::Target learnTarget;
    int                  savedLinkGen = -1;
    LinkPump             linkPump { *this };

    // The plugins a set holds, loaded one after another in its order
    std::vector<zodiak::sets::PluginToLoad> toRestore;
    int                restoreIndex = 0, restoreRun = 0;
    juce::StringArray  missing;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZodiakProcessor)
};
