// D:\workspace\Fanan_web_hub\src\SipFiles.h

#pragma once

#include <juce_core/juce_core.h>

//==============================================================================
/**
    Sonify's own plugins on disk: one file each, "<name>.sip" (Sonify internal plugin), all in
    one folder, <user data>\Plugins (Documents\Sonify\Plugins unless the user moved their user
    data). Whatever it is, an instrument, an effect or a MIDI FX, a .sip is one file: share it,
    sell it in the Store, or drop it on Sonify, and it plays.

    A .sip is a zip. At its top:
        sip.json        what it is: { format: "Sonify internal plugin", formatVersion: 1,
                        kind: "instrument" | "fx" | "midi", tags: ["utility"...], id, name,
                        vendor, version }: Sonify knows where it goes from its kind alone,
                        without reading the rest
    and then what that kind holds:
        instrument, fx  a made plugin's files (plugin.json, plugin.wasm, gui\... : CreatorPlugin.h)
        midi            a MIDI device's files (device.json, source\..., module.wasm: DeviceFiles.h)
    A widget (the Plugins Creator Studio's "anything else": a tuner, a meter) is an fx with the
    tag "utility".

    A made plugin plays from its files unpacked: Sonify unpacks each .sip once into its own
    cache (AppData\Fanan\Sonify\Plugin Cache), again only when the .sip changes. Songs keep the
    .sip's path and the plugin's id, never the cache's.

    The formats before (a "<name>.sonifyplugin" folder in Plugins Creator\Plugins, a
    "<name>.sonidev" file in Devices) are turned into .sip files once (migrate); the old ones go
    into "<user data>\Plugins\Old format" and are never read again. A song naming an old one finds
    its .sip by the plugin's id.

    Nothing in a .sip is native code: made plugins and MIDI devices are WebAssembly, played in
    their sandbox. Unpacking refuses paths that leave the cache folder, links, and archives too
    big or with too many files.
*/
namespace Sip
{
    inline constexpr const char* extension = ".sip";
    inline constexpr const char* manifestName = "sip.json";
    inline constexpr const char* formatTag = "Sonify internal plugin";

    enum class Kind { unknown, instrument, fx, midi };

    juce::String kindName (Kind kind);                 // "instrument", "fx", "midi" ("" for unknown)
    Kind kindFromName (const juce::String& name);      // also takes the older words: "effect", "widget", "midifx"

    struct Manifest
    {
        Kind kind = Kind::unknown;
        juce::String id, name, vendor;
        int version = 0;
        juce::StringArray tags;

        juce::var toVar() const;
        static Manifest fromVar (const juce::var& value);
    };

    /** <user data>\Plugins, where every .sip is kept (made when first needed). */
    juce::File folder();

    /** By its name: "<something>.sip". */
    bool isSipFile (const juce::File& file);

    /** What a .sip says it is (sip.json; a .sip without one, from its plugin.json or device.json).
        False when it isn't a .sip Sonify can read. Quick: nothing is unpacked. */
    bool readManifest (const juce::File& sip, Manifest& manifest);

    /** One file inside a .sip, when it's there and at most limit bytes. */
    bool readEntry (const juce::File& sip, const juce::String& path, juce::MemoryBlock& into, size_t limit = 4 * 1024 * 1024);

    /** The .sip's files, unpacked into its own place in the cache (again only when the .sip has
        changed). Empty, with the reason in error, when it can't be. Message thread or any thread
        but the audio thread. */
    juce::File unpacked (const juce::File& sip, juce::String& error);

    /** Writes a .sip: the manifest, then every file in folder (its sub-folders too), safely (a
        whole new file or none). A sip.json in folder is left out (the manifest is the one). */
    juce::Result packFolder (const juce::File& folder, const Manifest& manifest, const juce::File& sip);

    /** Writes sip.json into a zip being built (DeviceFile::write's MIDI devices). */
    juce::String manifestText (const Manifest& manifest);

    /** The .sip files in the folder of that kind (unknown: every one), by name. */
    juce::Array<juce::File> list (Kind kind = Kind::unknown);

    /** A free file in the folder for one of that name ("Name.sip", "Name 2.sip"...); except: a
        file that may be taken (the one being saved over). */
    juce::File freeFile (const juce::String& name, const juce::File& except = {});

    /** The .sip of that kind with that id, or none. */
    juce::File findById (const juce::String& id, Kind kind = Kind::unknown);

    /** Once at start, after the user data folder is known: the old formats (made plugins'
        .sonifyplugin folders, MIDI devices' .sonidev files) become .sip files in the folder; the
        old ones are moved to "Old format". Returns how many were turned. */
    int migrate();
}
