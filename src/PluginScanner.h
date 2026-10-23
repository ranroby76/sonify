// C:\workspace\Fanan_web_hub\src\PluginScanner.h

#pragma once

#include "Plugin.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <vector>

//==============================================================================
/**
    Reads plugin files the safe way, as Colosseum does: never inside the Hub.

    Each plugin is read by a copy of the Hub started just for it
    (FananWebHub.exe --scan-plugin <plugin> <result.json>): it loads that one plugin,
    writes what it holds, and exits. A plugin that crashes or hangs takes only that copy
    down (after 30 seconds it's stopped), and is reported; the Hub itself never loads a
    plugin while scanning.

    The reading runs on a thread of its own; the Hub asks for progress and results from
    its own timer (no calls from this thread into the Hub).
*/
class PluginScanner final : private juce::Thread
{
public:
    struct Outcome
    {
        juce::File file;
        std::vector<Plugin::Descriptor> plugins;
        juce::String error;            // why it couldn't be read, when it couldn't
    };

    /** worker: the program that reads a plugin (the Hub itself, unless a test says otherwise). */
    explicit PluginScanner (std::vector<juce::File> files,
                            int timeoutSeconds = 30,
                            juce::File worker = juce::File::getSpecialLocation (juce::File::currentExecutableFile));
    ~PluginScanner() override;

    void start();
    void cancel();

    bool isFinished() const noexcept        { return finished.load(); }
    bool wasCancelled() const noexcept      { return cancelled.load(); }
    int getTotal() const noexcept           { return static_cast<int> (files.size()); }
    int getDone() const noexcept            { return done.load(); }
    juce::String getCurrent() const;

    /** What has been read since the last call. */
    std::vector<Outcome> takeOutcomes();

    //==============================================================================
    /** The reading copy's side: true when the command line asks for it. */
    static bool isWorkerCommandLine (const juce::String& commandLine);

    /** Reads the plugin named on the command line and writes the result; returns the exit code. */
    static int runWorker (const juce::StringArray& arguments);

    static juce::var toVar (const Plugin::Descriptor& descriptor);
    static Plugin::Descriptor fromVar (const juce::var& value);

    static constexpr const char* workerFlag = "--scan-plugin";

private:
    void run() override;
    Outcome read (const juce::File& file);

    const std::vector<juce::File> files;
    const int timeoutSeconds;
    const juce::File worker;

    std::atomic<bool> finished { false }, cancelled { false };
    std::atomic<int> done { 0 };

    mutable juce::CriticalSection lock;
    juce::String current;
    std::vector<Outcome> outcomes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginScanner)
};
