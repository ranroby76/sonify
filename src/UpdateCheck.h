// D:\workspace\Fanan_web_hub\src\UpdateCheck.h

#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <functional>
#include <memory>

/**
    The update reminder: a few seconds after Sonify starts, and once a day while it stays open,
    Sonify asks Fanan's site (sonifyLatest, the release set in the product manager) for the
    newest version. One newer than this Sonify is handed to onNewer, on the message thread, so
    the page shows its floating reminder; a version the user chose to skip isn't, unless it's
    marked important. Offline, or with no answer, nothing happens.
*/
class UpdateCheck final : private juce::Thread
{
public:
    struct Release
    {
        juce::String version;      // "0.3.0"
        juce::String released;     // "2026-11-20"
        juce::StringArray notes;   // what's new, a point a line
        bool important = false;    // a security fix, say: shown even when skipped
        juce::String page;         // where to get it (always on fananteam.site)
    };

    explicit UpdateCheck (std::function<void (const Release&)> onNewer);
    ~UpdateCheck() override;

    /** This Sonify's version ("0.2.0"). */
    static juce::String currentVersion();

    /** Whether theirs is a later version than ours, number by number ("0.10.0" after "0.9.2"). */
    static bool isNewer (const juce::String& theirs, const juce::String& ours);

    /** "Skip this version": that version's reminder isn't shown again (an important one still is). */
    static void skip (const juce::String& version);

    /** Sonify's page on Fanan's site, where every version is: what "Get it" opens. */
    static juce::String sonifyPage();

    /** The page a release names, when it's on Fanan's site; else Sonify's page. */
    static juce::String safePage (const juce::String& page);

private:
    void run() override;
    void check();

    std::function<void (const Release&)> onNewer;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UpdateCheck)
};
