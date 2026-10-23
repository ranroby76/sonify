// D:\workspace\Fanan_web_hub\src\PluginLibrary.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "stella_blob.h"

#include <functional>
#include <memory>
#include <vector>

//==============================================================================
/**
    A plugin's sample library: the blobs (sampled instruments, made by the Sampling Studio's SFZ to
    Blob) a sampler made in the Plugins Creator Studio plays, numbered 0, 1, 2... in its list
    (stella::blob in stella_api.h; the engine is stella_blob.h, compiled into Sonify here).

        A project       library.json: [{ "name", "file" }], the blobs where they were added from.
                        Options > Sample library... edits it; the Edit UI toolbox's Instrument
                        list shows its names (a list whose settings say "library": true).
        Add to Sonify   the made plugin's library.json: [{ "name", "file" (its name), "origin"
                        (where it was) }]; a store package's has no origin.
        Export          the plugin knows its blobs by their files' names.

    Where a plugin finds a blob on a computer, in order (the same in Sonify and in an exported
    plugin, so one copy serves both):
        1. the folders (or blobs) listed in <app data>\<vendor>\<plugin>\library.txt, a line each:
           Locate... adds a folder, an export adds the creator's own blobs;
        2. <Documents>\<vendor>\<plugin>, where a buyer puts them;
        3. beside the plugin (a made plugin's folder, its "library" folder);
        4. where it was added from (Sonify only).
*/
namespace PluginLibrary
{
    inline constexpr const char* fileName = "library.json";

    struct Instrument
    {
        juce::String name;           // as the plugin lists it
        juce::File file;             // the blob (a made plugin's: where it was added from)
    };

    /** The library in a project's folder, or a made plugin's (in order). */
    juce::Array<Instrument> load (const juce::File& folder);
    bool save (const juce::File& folder, const juce::Array<Instrument>& instruments);

    /** A made plugin's library.json: each blob by its file's name, and (origins: true) where it was. */
    bool writeMade (const juce::File& pluginFolder, const juce::Array<Instrument>& instruments, bool origins);

    /** A name fit for a folder, as an exported plugin makes it (stella_blob.h). */
    juce::String folderName (const juce::String& text);

    /** <Documents>\<vendor>\<plugin>. */
    juce::File homeFolder (const juce::String& vendor, const juce::String& plugin);

    /** <app data>\<vendor>\<plugin>\library.txt. */
    juce::File listFile (const juce::String& vendor, const juce::String& plugin);

    /** Adds lines to the plugin's list (each once): folders, or blobs. */
    void addToList (const juce::String& vendor, const juce::String& plugin, const juce::StringArray& paths);

    /** A blob, by its file's name, as the plugin finds it (see above): besides are the plugin's own
        folders, origin where it was added from. None: it isn't on this computer. */
    juce::File find (const juce::String& blobName, const juce::String& vendor, const juce::String& plugin,
                     const juce::Array<juce::File>& besides, const juce::File& origin);

    /** The library as the engine takes it: each instrument with the blob found for it. ownFirst:
        each blob where it was added from comes first (the studio's own project). */
    std::vector<stella::blob::Entry> entriesFor (const juce::Array<Instrument>& instruments, const juce::String& vendor,
                                                 const juce::String& plugin, const juce::Array<juce::File>& besides,
                                                 bool ownFirst = false);

    /** A blob's summary, said in a few words: "Grand Piano: 244 regions, 1.2 GB", or its problem. */
    juce::String describe (const juce::File& blob);

    //==========================================================================
    // Protected blob libraries (made with a product's protection: stella_blob.h). Each opens with
    // the license.soni beside it; one this computer isn't registered for plays its demo. For the
    // Registration window (Licences::showRegistration). Message thread.

    /** The protected libraries Sonify's plugins have loaded now (a product each), registered or not. */
    std::vector<stella::blob::LicenceInfo> licencesInUse();

    /** One of them: its license.soni (complete the type with stella_licence.h). False: not in use. */
    bool licenceOf (const juce::String& product, stella::licence::Soni& soni);

    /** A serial tried on every one in use that isn't registered: "" when one took it (kept, its
        demo over at once), else why not. */
    juce::String registerSerial (const juce::String& serial);

    /** After registering one some other way (Licences::registerSerial: the same key files): their
        demos end at once instead of within 2 seconds. */
    void recheckLicences();

    //==========================================================================
    /** The Plugins Creator Studio's Sample library window (Options > Sample library...). */
    struct WindowHooks
    {
        std::function<juce::Array<Instrument>()> get;
        std::function<void (const juce::Array<Instrument>&)> set;   // kept, and the plugin plays it
        std::function<int (int)> stateOf;                           // stella::blob's states, as the plugin has them
        std::function<juce::String (int)> problemOf;
        std::function<juce::File()> startFolder;                    // where Add... opens
    };

    std::unique_ptr<juce::DocumentWindow> makeWindow (const juce::String& pluginName, WindowHooks hooks);
}
