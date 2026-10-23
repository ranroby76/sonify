// D:\workspace\Fanan_web_hub\src\Cloud.h

#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <functional>
#include <memory>

//==============================================================================
/**
    Fanan's content, on Base44 (the platform Fanan's sites run on): the loops, samples,
    songs and plugins Fanan publishes for everyone. No account is needed for any of it.

    Speaks Base44's web API directly, as its JavaScript SDK does. The Fanan account, its
    credits and Sonify AI live on Fanan's server instead (FananServer).

    Everything that waits on the network runs on a thread of its own; every answer comes
    back on the message thread.
*/
class Cloud final
{
public:
    struct Settings
    {
        juce::String serverUrl { "https://base44.app" };
        juce::String appId;
    };

    /** Settings from FananWebHub-cloud.json beside the program, or the built-in ones. */
    static Settings loadSettings();

    using Done = std::function<void (const juce::Result& result, const juce::var& value)>;
    using Progress = std::function<void (double fraction, const juce::String& text)>;

    explicit Cloud (Settings settings);
    ~Cloud();

    bool isConfigured() const noexcept              { return settings.appId.isNotEmpty(); }

    /** When blocked (Sonify's "Block internet"), every request fails at once, reaching nothing. */
    void setBlocked (bool shouldBlock) noexcept     { blocked.store (shouldBlock); }
    bool isBlocked() const noexcept                 { return blocked.load(); }
    bool isBusy() const noexcept                    { return pending.load() > 0; }

    //==============================================================================
    /** Published items, newest first: [{ id, title, kind, description, tags, bpm, key, size,
        version, file_url, preview_url, tool_id }]. kind: loop, sample, song, plugin or miditool
        (a MIDI tool's definition, .json, or its package, .zip; tool_id names the tool). */
    void listContent (Done done);

    /** Downloads an item into its place under root: Loops, Samples, Songs (a song's package
        unpacked into a folder of its own), Plugins or MIDI Tools (as installMidiTool). Answers
        { path }: the audio file, the song file, the plugin file or the tool's folder. */
    void installContent (const juce::var& item, const juce::File& root, Done done, Progress progress);

    /** An item's short preview, downloaded once into the cache folder. Answers { file }. */
    void fetchPreview (const juce::var& item, const juce::File& cache, Done done);

    /** A MIDI tool (a Magenta model, MidiTools.h) into its own folder under root, root\<id>: its
        definition as tool.json and the model's files in model\. The definition names the model's
        checkpoint, a web folder of config.json, weights_manifest.json and the weight files the
        manifest lists. Answers { id, path }: the tool's folder. */
    void installMidiTool (const juce::var& tool, const juce::File& root, Done done, Progress progress);

    static constexpr const char* contentEntityName = "FananContent";

    /** How answers reach the message thread (tests without a message loop set their own). */
    void setDeliverer (std::function<void (std::function<void()>)> newDeliverer)   { deliverer = std::move (newDeliverer); }

private:
    struct Response
    {
        int status = 0;
        juce::var body;
        juce::String error;
    };

    Response request (const juce::String& method, const juce::String& path, const juce::var& json = {},
                      const juce::StringPairArray& query = {}, int timeoutMs = 20000);
    bool download (const juce::String& url, const juce::File& destination, juce::String& error);
    juce::Result installToolNow (juce::var tool, const juce::File& root, const juce::File& package, const Progress& progress);

    void run (std::function<void()> job);
    void answer (const Done& done, juce::Result result, juce::var value = {});
    void report (const Progress& progress, double fraction, const juce::String& text);

    const Settings settings;

    std::function<void (std::function<void()>)> deliverer;
    std::atomic<bool> blocked { false };
    std::atomic<int> pending { 0 };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    juce::ThreadPool worker { juce::ThreadPoolOptions{}.withThreadName ("Fanan cloud").withNumberOfThreads (1) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Cloud)
};
