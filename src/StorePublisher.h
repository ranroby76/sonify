// D:\workspace\Fanan_web_hub\src\StorePublisher.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "CreatorPlugin.h"
#include "DeviceFiles.h"
#include "FananServer.h"

#include <atomic>
#include <functional>
#include <memory>

//==============================================================================
/**
    The Store tab's posting: an ad for a plugin or MIDI FX made here, put on Sonify's Store (a
    notice board: nothing is sold through it).

    What the poster writes is kept on this computer, one ad per plugin project (by its id) or
    MIDI FX (by its id), in <user data>\Plugins Creator\Store: the title, section, tagline, story
    and features, a YouTube link, the website where it's sold, the formats it comes in, a price
    or terms in the poster's own words, whether it offers the Sonify version for download, and
    its picture. Once posted, the ad's id stays for good: every later post updates the same ad.

    Posting:
        1. "begin": the store gives the ad's id (the first time), the post's number, and where
           to send the files: its picture, the developer's logo and (when offered) the package;
        2. when the Sonify version is offered from the open project, it's packed: a plugin as
           Add to Sonify writes it (a protected one built again with its protection, and its
           license.soni beside it), or a MIDI FX's file as its last build, without its code or
           its conversation. An ad edited while its project isn't open keeps the package it had;
        3. the files are sent;
        4. "publish": the ad is on the board (open to every user).

    Fanan AI's story is what's charged (credits), when the poster asks for it.

    Message thread; the build and the packing run on a worker of its own.
*/
class StorePublisher final
{
public:
    //==========================================================================
    struct Listing
    {
        juce::String key;            // what it's for: the project's id, or the MIDI FX's
        juce::String kind;           // "instrument", "effect", "widget" or "midifx"
        juce::String ad;             // the store's id for the ad, from the first post on (empty: never posted)
        juce::String title, tagline, story;
        juce::String section;        // one of its kind's sections (StoreClient::sectionsFor)
        juce::String video;          // a YouTube link: its demo, played on its ad (empty: none)
        juce::String website;        // where it's sold and registered (http or https): required
        juce::String priceText;      // the poster's own words on price or terms, shown as written (optional)
        juce::StringArray features;
        juce::StringArray formats;   // of StoreClient::allFormats()
        bool offersDownload = false; // the Sonify version can be added straight from the ad
        int postedVersion = 0;       // 0: never posted
        juce::String postedAt;       // ISO time of the last post

        bool wasPosted() const noexcept              { return ad.isNotEmpty() && postedVersion > 0; }

        juce::var toVar() const;
        static Listing fromVar (const juce::var& value);
    };

    static constexpr int maxTitle = 80, maxTagline = 140, maxStory = 6000, maxFeatures = 10, maxFeature = 120;
    static constexpr int maxWebsite = 300, maxPriceText = 120;

    /** Where ads and their pictures are kept: <user data>\Plugins Creator\Store. */
    static juce::File folder();

    /** An ad as it was left (a new one, with key and kind, when there's none). */
    static Listing load (const juce::String& key, const juce::String& kind);
    static juce::Result save (const Listing& listing);

    /** Its picture, kept beside it. */
    static juce::File pictureFile (const juce::String& key);
    static juce::Image loadPicture (const juce::String& key);
    static juce::Result savePicture (const juce::String& key, const juce::Image& picture);

    //==========================================================================
    /** The developer's logo, on every ad they post: one for all, kept beside the ads
        (developer-logo.png). */
    static juce::File logoFile();
    static juce::Image loadLogo();
    static void removeLogo();

    /** A logo file (PNG, JPEG, GIF or SVG) made ready and kept: its empty margins trimmed off
        (transparent, or the plain colour around it), then fitted to logoLongest pixels on its
        longer side. report says what was measured and done. */
    static juce::Result importLogo (const juce::File& file, juce::String& report);

    /** The same for a picture already loaded: the logo as it's kept (invalid when it's empty). */
    static juce::Image fitLogo (const juce::Image& source, juce::String& report);

    static constexpr int logoLongest = 512, logoSmall = 128;

    /** What's missing before it can be posted (empty: nothing). */
    static juce::String problemWith (const Listing& listing, const juce::Image& picture);

    /** A website typed without http:// or https:// gets https:// in front. */
    static juce::String completeWebsite (const juce::String& typed);

    /** Whether a plugin project is protected (its protection.json is on and whole): its Sonify
        version is built with its protection and carries its license.soni. */
    static bool isProtected (const juce::File& projectFolder);

    //==========================================================================
    /** What's posted. sendPackage: the Sonify version goes with it (a plugin: its package as Add
        to Sonify makes it, and its project's folder, built again from it when it's protected;
        a MIDI FX: its file as it is). Without it, an ad that offers the download keeps the
        package it had. */
    struct Item
    {
        Listing listing;
        juce::Image picture;
        juce::String maker;                // the name the ad shows

        bool sendPackage = false;
        bool isMidiFx = false;
        MadePlugins::Package package;      // a plugin
        juce::File projectFolder;          // ...built from here when it's protected
        Devices::DeviceFile device;        // a MIDI FX
    };

    explicit StorePublisher (FananServer& server);
    ~StorePublisher();

    using Progress = std::function<void (const juce::String& step)>;
    using Posted = std::function<void (const juce::Result& result, const Listing& listing)>;
    using Written = std::function<void (const juce::Result& result, const Listing& story)>;

    /** Posts it (see above). done gets the listing as the store has it now (its id, post). */
    void post (Item item, Progress progress, Posted done);

    /** Fanan AI writes the ad's tagline, story, features and section from facts (what the
        plugin is, from its code) and its picture: charged in credits. done gets them in a copy
        of current. */
    void writeStory (const juce::String& facts, const juce::Image& picture, const Listing& current, Written done);

    bool isPosting() const noexcept     { return posting; }
    bool isWriting() const noexcept     { return writing; }

private:
    struct Prepared
    {
        juce::String error;
        juce::MemoryBlock package;
    };

    static Prepared prepare (const Item& item, const juce::String& adId, int version);
    static juce::Result zipFolder (const juce::File& source, juce::MemoryBlock& out);
    static juce::MemoryBlock pngOf (const juce::Image& image, int longest);
    static juce::String storyGuide();
    static bool readStory (const juce::var& answer, Listing& story, juce::String& why);

    void finishPost (const Posted& done, const juce::Result& result, const Listing& listing);

    /** A post under way, once its files are made: they go one after the other, then publish. */
    struct Sending
    {
        Listing listing;
        juce::String maker, adId;
        int version = 0;
        Progress say;
        Posted done;
        juce::String packageAddress, pictureAddress, logoAddress;
        juce::MemoryBlock package, picture, logo;
        bool packageSent = false, logoSent = false;
    };

    void sendPackage (std::shared_ptr<Sending> sending);
    void sendPicture (std::shared_ptr<Sending> sending);
    void sendLogo (std::shared_ptr<Sending> sending);
    void publish (std::shared_ptr<Sending> sending);

    FananServer& server;
    bool posting = false, writing = false;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    // Last, so it's the first to go: a build that's under way finishes while the rest exists.
    juce::ThreadPool worker { juce::ThreadPoolOptions{}.withThreadName ("Store").withNumberOfThreads (1) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StorePublisher)
};
