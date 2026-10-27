// D:\workspace\Fanan_web_hub\src\StoreClient.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "FananServer.h"

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <vector>

class DeviceStudio;

//==============================================================================
/**
    Sonify's Store, as the Store tab uses it: a notice board. Sonify users post ads for the
    plugins and MIDI FX they made (StorePublisher), open to every user: what it is, its story,
    features, video and pictures, the formats it comes in, a price or terms written by its maker
    (shown as written: nothing is sold here), and a link to the maker's own website, where they
    sell it and register their buyers.

        browse     the ads (by kind, by section, by words), and this account's own (My ads:
                   hidden ones too), which their poster can hide, show again or delete
        add        an ad may offer the Sonify version for instant download (the poster's choice):
                   its package is downloaded from the store's files, checked (its size and
                   SHA-256), and installed: a plugin into Made in Sonify (an update replaces the
                   older version, songs keep finding it), a MIDI FX into My Devices. A protected
                   one carries its license.soni and plays its demo until the buyer registers it
                   with the serial they get from the maker's site (Licences, CreatorPlugin)

    A package can only install as its ad's own: its id in Sonify comes from the ad's id
    ("store-<ad>"), whatever its files say.

    Message thread; downloads and unpacking run on workers of its own.
*/
class StoreClient final
{
public:
    //==========================================================================
    /** The Sonify version an ad offers: where its package is, and what it must be. */
    struct Package
    {
        juce::String url, sha256;
        juce::int64 size = 0;
        int version = 0;

        bool isValid() const noexcept         { return url.startsWithIgnoreCase ("https://") || url.startsWithIgnoreCase ("http://"); }
    };

    struct Ad
    {
        juce::String id, kind, title, tagline, story, maker, section;
        juce::String pictureUrl;
        juce::String logoUrl;                 // its maker's logo (none: no logo)
        juce::String video;                   // its demo video on YouTube (none: no video)
        juce::String website;                 // the maker's site, where it's sold and registered
        juce::String priceText;               // the maker's own words on price or terms (shown only)
        juce::StringArray features, formats;  // formats: of allFormats()
        bool offersDownload = false;          // the Sonify version can be added from here
        Package package;
        int version = 0;                      // counts the ad's posts (its pictures' cache follows it)
        juce::int64 created = 0, updated = 0;

        // The poster's own (My ads).
        bool mine = false, hidden = false;
        juce::String key;                     // the project's or MIDI FX's id it was posted from

        bool isValid() const noexcept         { return id.isNotEmpty(); }
        bool isMidiFx() const noexcept        { return kind == "midifx"; }
        bool canDownload() const noexcept     { return offersDownload && package.isValid(); }
        juce::String videoId() const          { return youtubeId (video); }
        juce::String kindName() const;        // "Instrument", "Effect", "Widget", "MIDI FX"

        static Ad fromVar (const juce::var& value);
    };

    struct Page
    {
        std::vector<Ad> ads;
        int page = 1, pages = 1, total = 0;
    };

    /** The store's sections, by kind ("instrument", "effect", "midifx", "widget"): an ad is in
        one of its kind's (its poster picks it). The store keeps the same list (sonifyStore's
        SECTIONS). */
    static const juce::StringArray& sectionsFor (const juce::String& kind);

    /** The kind a section belongs to (empty: none). */
    static juce::String kindOfSection (const juce::String& section);

    /** The formats an ad can say it comes in: VST3, CLAP, AU and Sonify (sonifyStore's FORMATS). */
    static const juce::StringArray& allFormats();

    /** A YouTube video's id (11 letters, digits, '-' and '_') from a link to it (watch?v=,
        youtu.be/, shorts/, embed/, live/) or the id itself; empty when it isn't one. */
    static juce::String youtubeId (const juce::String& link);

    /** Whether a link is a website's address the store takes: http:// or https://, a real host
        name, nothing odd in it. */
    static bool isWebsite (const juce::String& link);

    explicit StoreClient (FananServer& server);
    ~StoreClient();

    /** Sonify's MIDI devices: a MIDI FX installs into My Devices (none: it can't install). */
    void setDevices (DeviceStudio* devices) noexcept    { deviceStudio = devices; }

    bool isSignedIn() const                             { return server.isSignedIn(); }

    //==========================================================================
    using Listed = std::function<void (const juce::Result& result, const Page& page)>;
    using Done = std::function<void (const juce::Result& result)>;
    using Progress = std::function<void (const juce::String& step)>;

    /** The board: kind and section empty for all, words to look for, a page (from 1). */
    void list (const juce::String& kind, const juce::String& section, const juce::String& words, int page, Listed done);

    /** This account's own ads, hidden ones too. */
    void mine (Listed done);

    /** One ad. */
    void get (const juce::String& adId, std::function<void (const juce::Result&, const Ad&)> done);

    /** Its picture: from the cache, or fetched (invalid when it can't be had). */
    void picture (const Ad& ad, std::function<void (const juce::Image&)> done);

    /** Its maker's logo, the same way (invalid when it has none). */
    void logo (const Ad& ad, std::function<void (const juce::Image&)> done);

    //==========================================================================
    /** The version of its Sonify download installed here (0: not installed). */
    int installedVersion (const Ad& ad) const;

    /** Downloads, checks and installs its Sonify version (an update replaces the older one). */
    void install (const Ad& ad, Progress progress, Done done);
    bool isInstalling (const juce::String& adId) const;

    /** The poster takes one of theirs off the board, or puts it back. */
    void hide (const Ad& ad, bool hidden, Done done);

    /** The poster deletes one of theirs for good. */
    void remove (const Ad& ad, Done done);

    /** Anyone signed in tells Fanan an ad breaks the Store's rules (reason: one of reportReasons,
        note: their own words). The store keeps one report per account and ad; when enough
        people report the same ad, Fanan AI reads it again against the rules, and one that
        breaks them comes off the board (a strike for its poster). */
    void report (const Ad& ad, const juce::String& reason, const juce::String& note, Done done);
    static const juce::StringArray& reportReasons();

    /** A plugin was installed (or updated): Sonify's Browser shows it. */
    std::function<void (const juce::File& folder, const juce::String& name, bool replaced)> onPluginInstalled;

    /** A MIDI FX was installed into My Devices. */
    std::function<void (const juce::String& name)> onDeviceInstalled;

    /** The id a downloaded MIDI FX has in My Devices. */
    static juce::String deviceIdFor (const juce::String& adId)   { return "store-" + adId; }

private:
    struct Unpacked
    {
        juce::String error;
        juce::File folder;      // the work folder (deleted once installed)
        juce::File plugin;      // a plugin's folder, ready
        juce::File device;      // a MIDI FX's file, ready
        juce::String name;
    };

    static Unpacked unpack (const juce::MemoryBlock& package, const Ad& ad);
    void download (const Ad& ad, Progress say, Done finish);
    void finishInstall (const Ad& ad, Unpacked unpacked, Done done);
    void call (const juce::String& action, juce::var fields, FananServer::Done done);
    void listFrom (const juce::String& action, juce::var fields, bool mineToo, Listed done);
    static juce::File pictureCacheFile (const Ad& ad, const juce::String& part);
    void fetchImage (const juce::String& address, const juce::String& key, const juce::File& cacheFile,
                     std::function<void (const juce::Image&)> done);

    FananServer& server;
    DeviceStudio* deviceStudio = nullptr;

    std::map<juce::String, juce::Image> pictures;                                          // by ad and version
    std::map<juce::String, std::vector<std::function<void (const juce::Image&)>>> waitingPictures;
    std::vector<juce::String> installing;

    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    // Last, so they're the first to go: a download under way finishes while the rest exists.
    juce::ThreadPool workers { juce::ThreadPoolOptions{}.withThreadName ("Store").withNumberOfThreads (3) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StoreClient)
};
