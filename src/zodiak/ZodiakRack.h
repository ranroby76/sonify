// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakRack.h
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>
#include "ZodiakInserts.h"

//==============================================================================
//  ZodiakRack.h  —  plugins loaded from file (Hosted), and the MASTER's VST FX
//  (its rack: the built-in Wah and Phaser, and any VST loaded from file, in
//  order).  A channel's plugins live in its graph (ZodiakGraph.h).
//
//  THREADS.  The message thread changes a rack under its lock, briefly; the
//  audio thread holds the lock while it plays the rack for one block, and
//  only TRIES for it: a rack being changed plays silence for that block.  A
//  plugin is made, prepared and destroyed on the message thread, never under
//  the lock.
//
//  Loading is VST3 (and VST 2 when Sonify is built with the VST 2 SDK; AU on
//  a Mac), straight from the file: no scanning, at the user's own risk, as in
//  Colosseum VSTi - or one of Sonify's own plugins (Zax, Natura...), picked by
//  name.  Zodiak carries only what the user loads: nothing is built in.
//==============================================================================

namespace zodiak::rack
{
    //==========================================================================
    //  A plugin loaded from file, with the buffer it plays in
    //==========================================================================
    struct Hosted
    {
        Hosted() = default;
        ~Hosted();      // its window (if open) closes first

        std::unique_ptr<juce::AudioProcessor> instance;   // a VST3/VST/AU, or one of Sonify's own
        juce::File   file;                                // a plugin from file: its file
        juce::String sonifyId;                            // one of Sonify's own: its id (Internals)
        juce::String name;
        juce::AudioBuffer<float> io;          // its channels (at least 2), the rack's largest part long
        juce::MidiBuffer         noMidi;

        void prepare (double sampleRate, int maxBlock);

        /** Plays n samples: L/R in and out (an instrument: out only), with these notes. */
        void play (float* L, float* R, int n, juce::MidiBuffer& midi, bool isInstrument) noexcept;
    };

    //==========================================================================
    //  One VST FX item
    //==========================================================================
    struct Insert
    {
        enum Type : int { builtInType = 0, pluginType = 1 };

        Type  type = builtInType;
        std::atomic<bool> on { true };
        std::unique_ptr<inserts::BuiltIn> builtIn;     // Wah or Phaser
        std::unique_ptr<Hosted>           plugin;
        int   uid = 0;                                 // the window's handle on it

        juce::String name() const   { return type == builtInType ? juce::String (inserts::kindName (builtIn->getKind())) : plugin->name; }
    };

    //==========================================================================
    class Rack
    {
    public:
        static constexpr int kMaxInserts = 8;

        Rack();

        juce::SpinLock lock;

        // VST FX
        std::vector<std::unique_ptr<Insert>> fx;

        //======================================================================
        //  Message thread
        //======================================================================
        void prepare (double sampleRate, int maxBlock);

        /** A built-in insert at the end; its uid, or 0 when the list is full. */
        int addBuiltIn (int kind);

        /** A plugin insert at the end (already prepared); its uid, or 0 when full
            (the plugin is then destroyed). */
        int addPlugin (std::unique_ptr<Hosted> plugin);

        /** Puts a plugin in place of an insert (the old one destroyed here). */
        bool replaceWithPlugin (int uid, std::unique_ptr<Hosted> plugin);

        bool removeInsert (int uid);
        void clearInserts();

        Insert* findInsert (int uid);
        int     numInserts() const noexcept    { return (int) fx.size(); }

        /** Everything back as a new set has it: no inserts. */
        void clearAll();

    private:
        int nextUid = 1;
        double sr = 48000.0;
        int    block = 512;
    };

    //==========================================================================
    //  A plugin's own window, wherever it's open: closed (its editor deleted)
    //  before the plugin goes.  Message thread.
    //==========================================================================
    class EditorCloser
    {
    public:
        EditorCloser();
        virtual ~EditorCloser();
        virtual void closeEditorOf (juce::AudioProcessor* plugin) = 0;
    };

    void closeEditorsOf (juce::AudioProcessor* plugin);

    //==========================================================================
    //  Loading plugins from file (message thread)
    //==========================================================================
    /** "Load any VST3 or VST" (Windows), "Load any VST3, VST or AU" (Mac). */
    juce::String loadTitle();

    /** The file chooser's patterns. */
    juce::String loadPatterns();

    /** Where the file chooser opens: the folder last loaded from (kept in
        %APPDATA%\Fanan\Sonify\Zodiak\Plugin Folder.xml), else the system's VST3 folder. */
    juce::File pluginFolder();

    /** A plugin was chosen from `file`: its folder is the one the chooser opens next
        time (written silently; nothing changes when it's the same folder). */
    void rememberPluginFolder (const juce::File& file);

    /** The plugin a chosen file holds: a file inside a .vst3 bundle is taken as
        the bundle. */
    juce::File normalisePluginFile (const juce::File& chosen);

    /** Where a plugin comes from: a file, or one of Sonify's own by its id. */
    struct Source
    {
        Source() = default;
        Source (const juce::File& f) : file (f) {}
        static Source sonify (const juce::String& id)    { Source s; s.sonifyId = id; return s; }

        juce::File   file;
        juce::String sonifyId;

        bool isSonify() const noexcept     { return sonifyId.isNotEmpty(); }
        bool isEmpty() const noexcept      { return sonifyId.isEmpty() && file == juce::File(); }
    };

    /** Sonify's own plugins Zodiak can load, of one kind (instruments, or effects): every one
        but Zodiak itself.  id and name each. */
    std::vector<std::pair<juce::String, juce::String>> sonifyPlugins (bool instruments);

    /** Is this one of Sonify's own plugins (still in this Sonify)? */
    bool isSonifyPlugin (const juce::String& id);

    /** Which plugin: a menu of Sonify's own of the kind (when there are any) with
        "Load from file..." under them, or straight to the file chooser.  `picked` gets
        the choice (a file chosen also becomes the chooser's folder next time); nothing
        when the user cancels.  Message thread. */
    void pickSource (bool wantInstrument, juce::Component* near, std::function<void (const Source&)> picked);

    /** Makes the plugin from its source, prepared, and hands it over (nullptr and why
        when it can't). */
    void loadPlugin (const Source& source, double sampleRate, int maxBlock, bool wantInstrument,
                     std::function<void (std::unique_ptr<Hosted>, const juce::String& error)> done);

    /** A plugin's state, for a set file, and back. */
    juce::String saveState (const Hosted& h);
    void         restoreState (Hosted& h, const juce::String& base64);
}
