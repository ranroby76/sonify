// D:\workspace\Fanan_web_hub\src\Library.h

#pragma once

#include "Plugin.h"

#include <juce_core/juce_core.h>

#include <map>
#include <vector>

//==============================================================================
/**
    The library database: every plugin the Browser offers, and what's known about it.

    Plugins come from the Plugin Folders (searched, sub-folders too, for VST3 plugins,
    for native CLAP plugins: .clap libraries, and .clap bundle folders on a Mac, and for
    WCLAPs: .wasm files, .wclap bundle folders and .wclap.tar.gz archives) and
    from files added one at a time. The system's VST3 and CLAP folders are added the first
    time (Settings > Locations > Plug-ins lists them all).
    A plugin that crashes the Hub while it's being read is left out after the restart
    (until its file changes or a full rescan), as Studio One's scanner does. Each file is read once: a scan reads only files that are new or changed since
    (every file when full), and a plugin whose file disappears stays listed as missing
    until the file is back or the plugin is removed from the Browser.

    For each plugin: its name, vendor, version and CLAP features, the kind and type
    (category) the Browser sorts by, both read from the features, whether it's a
    favorite, and when it was last used.

    Saved as library.json with Sonify's other settings (SettingsFolder). Message thread only.
*/
class Library final
{
public:
    struct Entry
    {
        juce::String file, id, name, vendor, version;
        juce::StringArray features;
        juce::String kind;             // instrument, effect or noteEffect
        juce::String category;         // the Browser's Type: Synth, Reverb, Note FX...
        bool favorite = false;
        bool hidden = false;           // hidden in the Plug-In Manager: no list offers it
        bool missing = false;          // its file isn't there any more
        juce::int64 lastUsed = 0;      // ms since 1970; 0: never
        int uses = 0;

        juce::String format() const;   // "VST3", "CLAP", "WCLAP", "Built-in" or "Sonify" (made in the Plugins Creator Studio)
    };

    explicit Library (const juce::File& databaseFile = getDefaultFile());

    const std::vector<Entry>& getEntries() const noexcept      { return entries; }
    const juce::StringArray& getFolders() const noexcept       { return folders; }
    const juce::StringArray& getAddedFiles() const noexcept    { return addedFiles; }

    //==============================================================================
    /** For this run only, from the question at startup: whether third-party plugins are
        offered. Without them, only Sonify's own plugins are (those installed from Fanan's
        content, in ownFolder), and scans read only those. Everything stays listed for the
        next run. */
    void setThirdPartyPlugins (bool offered, const juce::File& ownFolder);
    bool offersThirdParty() const noexcept                     { return thirdPartyOffered; }

    /** Whether a plugin file is offered this run: always, unless third-party plugins are off
        and it isn't one of Sonify's own. */
    bool isOffered (const juce::String& file) const;

    //==============================================================================
    /** A plugin file picked by hand: read now, and kept even outside the Plugin Folders. */
    /** Lists a plugin file added by hand; the next scan reads it. */
    void addFile (const juce::File& file);

    /** Out of the Browser. A file inside a Plugin Folder stays out on later scans too. */
    void removeFile (const juce::String& path);

    void addFolder (const juce::File& folder);

    /** Another folder where one was, in the same place in the list (Settings > Locations >
        Plug-ins: Change). The old folder's plugins go, as with removeFolder. */
    void replaceFolder (const juce::String& oldPath, const juce::File& newFolder);

    /** The usual places for plugins: the system's VST3 and CLAP folders
        (C:\Program Files\Common Files\VST3 and ...\CLAP on Windows). */
    static juce::Array<juce::File> getDefaultFolders();

    /** Lists the usual folders that exist and aren't listed yet; returns how many. */
    int addDefaultFolders();

    /** The folder, and the plugins that came from it (not ones also added by hand), go. */
    void removeFolder (const juce::String& path);

    /** The files a scan should read: new and changed ones in the Plugin Folders and among
        the files added by hand, or every one of them when full. Plugins whose files have
        gone are marked missing (and ones whose files are back, not) on the way. */
    std::vector<juce::File> prepareScan (bool full);

    /** Records what a plugin file holds, as the scanner read it (in a copy of the Hub, never
        in the Hub itself): its plugins replace what was known about it, keeping each plugin's
        favorite and use history. With no plugins, error says why. added counts plugins not
        listed before. */
    juce::Result recordScan (const juce::File& file, const std::vector<Plugin::Descriptor>& found,
                             const juce::String& error, int& added);

    void setFavorite (const juce::String& file, const juce::String& id, bool favorite);
    void markUsed (const juce::String& file, const juce::String& id);

    //==============================================================================
    // The Plug-In Manager.

    /** Hidden plugins stay listed (here and in the Plug-In Manager), but no list in Sonify offers
        them. plugins: {file, id} pairs. */
    void setHidden (const std::vector<std::pair<juce::String, juce::String>>& plugins, bool hidden);

    /** A plugin found in more than one format (the same name from the same vendor) shows in the
        first of the formats in this order only; the others are hidden. Returns how many were. */
    int hideDuplicates();

    const juce::StringArray& getFormatPriority() const noexcept   { return formatPriority; }
    void setFormatPriority (const juce::StringArray& formats);   // known formats, in order; the rest keep theirs after
    static juce::StringArray defaultFormatPriority()              { return { "VST3", "CLAP", "WCLAP", "Sonify" }; }

    /** Whether duplicates are hidden after every scan, by the format order. */
    bool hidesDuplicatesAfterScans() const noexcept             { return hideDuplicatesAfterScans; }
    void setHideDuplicatesAfterScans (bool on);

    /** The Blocklist: files left out because they couldn't be read (or crashed Sonify), with why,
        and files taken out of the Browser (removed). */
    struct Blocked
    {
        juce::String file, reason;
        bool removed = false;
    };

    std::vector<Blocked> getBlocked() const;

    /** Off the Blocklist: the next scan reads those files again. */
    void unblock (const juce::StringArray& paths);
    void resetBlocklist();

    /** When a plugin file last changed (ms since 1970), as the last scan saw it. */
    juce::int64 fileTimeOf (const juce::String& path) const;

    /** The group a plugin file belongs to in the Browser's Folder view. */
    juce::String folderLabelFor (const juce::String& file) const;

    void save() const;

    //==============================================================================
    static bool isPluginFile (const juce::File& file);
    static juce::String kindFromFeatures (const juce::StringArray& features);
    static juce::String categoryFromFeatures (const juce::StringArray& features);
    static juce::File getDefaultFile();

    static constexpr const char* formatName = "Fanan Web Hub Library";
    static constexpr int formatVersion = 2;

private:
    struct FileRecord
    {
        juce::int64 size = 0, modified = 0;
        juce::String error;            // why it couldn't be read, when it couldn't
    };

    void load();

    /** Sonify's own effects, always listed (under the executable), keeping their favorites and use. */
    void addBuiltins();

public:
    /** The plugins made in the Plugins Creator Studio, listed as they are in their folder
        (MadePlugins::folder, in the user data folder: call it once that's known, and when it
        changes): new ones join, updated ones change, deleted ones go; each keeps its favorite and
        use. justAdded comes back to the Browser if it was taken out. Saves. */
    void refreshMadePlugins (const juce::File& justAdded = {});

private:
    void remember (const juce::File& file, const juce::String& error);
    void setFormatPriorityQuietly (const juce::StringArray& formats);
    bool isInFolders (const juce::String& path) const;
    Entry* find (const juce::String& file, const juce::String& id);

    juce::File databaseFile;
    std::vector<Entry> entries;
    std::map<juce::String, FileRecord> files;   // every file read, by path
    juce::StringArray folders;                  // the Plugin Folders
    juce::StringArray addedFiles;               // files added by hand, outside the folders
    juce::StringArray ignoredFiles;             // removed from the Browser though in a folder
    bool systemFoldersAdded = false;            // the VST3 folder has been offered once
    bool clapFolderAdded = false;               // so has the CLAP folder
    bool thirdPartyOffered = true;              // this run's answer (not saved)
    juce::StringArray formatPriority = defaultFormatPriority();
    bool hideDuplicatesAfterScans = false;
    juce::File ownPluginsFolder;                // where Sonify's own plugins are installed

    juce::File getScanningFile() const;         // names the file being read, in case it crashes

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Library)
};
