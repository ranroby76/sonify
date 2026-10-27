// D:\workspace\Fanan_web_hub\src\Library.cpp

#include "Library.h"
#include "Preferences.h"   // SettingsFolder

#include "BuiltinPlugins.h"
#include "CreatorPlugin.h"
#include "Plugin.h"
#include "Vst3Plugin.h"

#include <algorithm>
#include <limits>
#include <set>

namespace
{
    // What changes when a plugin file does. A .wclap bundle folder is judged by its module.
    juce::File contentOf (const juce::File& file)
    {
        return file.isDirectory() ? file.getChildFile ("module.wasm") : file;
    }

    juce::int64 sizeOf (const juce::File& file)       { return contentOf (file).getSize(); }
    juce::int64 modifiedOf (const juce::File& file)   { return contentOf (file).getLastModificationTime().toMilliseconds(); }

    /** Every plugin file in a folder and its sub-folders; a .wclap bundle counts as one. */
    void findPluginFiles (const juce::File& folder, std::vector<juce::File>& found, int depth)
    {
        if (depth > 8 || ! folder.isDirectory())
            return;

        for (const auto& item : juce::RangedDirectoryIterator (folder, false, "*",
                                                              juce::File::findFilesAndDirectories | juce::File::ignoreHiddenFiles))
        {
            const auto& file = item.getFile();

            if (Library::isPluginFile (file))
                found.push_back (file);
            else if (item.isDirectory())
                findPluginFiles (file, found, depth + 1);
        }
    }

    juce::var stringsToVar (const juce::StringArray& strings)
    {
        juce::Array<juce::var> list;

        for (const auto& text : strings)
            list.add (text);

        return list;
    }

    juce::StringArray stringsFromVar (const juce::var& value)
    {
        juce::StringArray strings;

        if (const auto* list = value.getArray())
            for (const auto& item : *list)
                strings.add (item.toString());

        return strings;
    }
}

//==============================================================================
juce::File Library::getDefaultFile()
{
    // With Sonify's other settings (a new build keeps it).
    return SettingsFolder::file ("library.json", "FananWebHub-library.json");
}

Library::Library (const juce::File& file)
    : databaseFile (file)
{
    load();
    addBuiltins();   // the made plugins are refreshed once Sonify knows the user data folder (MainCreator.cpp)

    // A plugin that crashed the Hub while it was being read last time is left out.
    const auto scanning = getScanningFile();

    if (scanning.existsAsFile())
    {
        const juce::File crashed (scanning.loadFileAsString().trim());

        if (crashed.exists())
            remember (crashed, "It crashed Sonify while being read, so it's left out. A full rescan tries it again.");

        scanning.deleteFile();
        save();
    }

    // The first time, the system's VST3 folder joins the Plugin Folders, and so does its CLAP
    // folder (once, for libraries made before it was one of the usual places).
    if (! systemFoldersAdded || ! clapFolderAdded)
    {
        const auto defaults = getDefaultFolders();

        for (const auto& folder : defaults)
        {
            const auto isClap = folder.getFileName().equalsIgnoreCase ("CLAP");

            if (folder.isDirectory() && (isClap ? ! clapFolderAdded : ! systemFoldersAdded))
                folders.addIfNotAlreadyThere (folder.getFullPathName());
        }

        systemFoldersAdded = true;
        clapFolderAdded = true;
        save();
    }
}

juce::File Library::getScanningFile() const
{
    return databaseFile.getSiblingFile (databaseFile.getFileNameWithoutExtension() + "-scanning.txt");
}

juce::String Library::Entry::format() const
{
    if (Builtins::isBuiltinId (id))
        return "Built-in";

    if (MadePlugins::isMadeFile (file))
        return MadePlugins::formatName;

    if (Plugin::isVst3 (juce::File (file)))
        return "VST3";

    return Plugin::isClap (juce::File (file)) ? "CLAP" : "WCLAP";
}

bool Library::isPluginFile (const juce::File& file)
{
    const auto name = file.getFileName().toLowerCase();

    // A .vst3 can be a bundle folder or a single file; so can a WCLAP, and so can a native CLAP
    // (a library on Windows and Linux, a bundle folder on a Mac). ".wclap" doesn't end in
    // ".clap", so a WCLAP is never taken for a native one.
    if (name.endsWith (".vst3"))
        return true;

    if (file.isDirectory())
        return name.endsWith (".wclap") || name.endsWith (".clap");

    return name.endsWith (".wasm") || name.endsWith (".wclap.tar.gz") || name.endsWith (".wclap.tgz")
        || name.endsWith (".clap");
}

juce::String Library::kindFromFeatures (const juce::StringArray& features)
{
    if (features.contains ("instrument"))
        return "instrument";

    if (features.contains ("audio-effect") || features.contains ("analyzer"))
        return "effect";

    if (features.contains ("note-effect"))
        return "noteEffect";

    return "instrument";
}

juce::String Library::categoryFromFeatures (const juce::StringArray& features)
{
    const auto has = [&features] (std::initializer_list<const char*> names)
    {
        for (const auto* name : names)
            if (features.contains (name))
                return true;

        return false;
    };

    const auto kind = kindFromFeatures (features);

    if (kind == "noteEffect")
        return "Note FX";

    if (kind == "instrument")
    {
        if (has ({ "drum", "drum-machine" }))    return "Drums";
        if (has ({ "sampler", "granular" }))     return "Sampler";
        if (has ({ "synthesizer" }))             return "Synth";
        return "Instrument";
    }

    // Effects: the most telling feature first.
    if (has ({ "analyzer" }))                    return "Analyzer";
    if (has ({ "reverb" }))                      return "Reverb";
    if (has ({ "delay" }))                       return "Delay";
    if (has ({ "equalizer" }))                   return "EQ";
    if (has ({ "compressor", "limiter", "expander", "gate", "transient-shaper", "de-esser" }))  return "Dynamics";
    if (has ({ "distortion" }))                  return "Distortion";
    if (has ({ "filter" }))                      return "Filter";
    if (has ({ "chorus", "flanger", "phaser", "tremolo", "rotary-speaker", "frequency-shifter", "pitch-shifter", "vibrato" }))
        return "Modulation";
    if (has ({ "utility", "mixing", "mastering", "restoration", "pitch-correction" }))
        return "Mixing";
    return "Effect";
}

//==============================================================================
void Library::load()
{
    entries.clear();
    files.clear();
    folders.clear();
    addedFiles.clear();
    ignoredFiles.clear();

    const auto parsed = juce::JSON::parse (databaseFile);
    const auto* plugins = parsed.getArray();

    if (plugins != nullptr)
    {
        // The first library was a plain list of plugins, every one added by hand. With no
        // file records, the next scan reads them again and learns their features.
        for (const auto& item : *plugins)
            addedFiles.addIfNotAlreadyThere (item.getProperty ("file", {}).toString());

        addedFiles.removeEmptyStrings();
    }
    else if (parsed.getProperty ("format", {}).toString() == formatName)
    {
        plugins = parsed.getProperty ("plugins", {}).getArray();
        folders = stringsFromVar (parsed.getProperty ("folders", {}));
        addedFiles = stringsFromVar (parsed.getProperty ("addedFiles", {}));
        ignoredFiles = stringsFromVar (parsed.getProperty ("ignoredFiles", {}));
        systemFoldersAdded = static_cast<bool> (parsed.getProperty ("systemFoldersAdded", false));
        clapFolderAdded = static_cast<bool> (parsed.getProperty ("clapFolderAdded", false));
        hideDuplicatesAfterScans = static_cast<bool> (parsed.getProperty ("hideDuplicatesAfterScans", false));

        if (parsed.hasProperty ("formatPriority"))
            setFormatPriorityQuietly (stringsFromVar (parsed.getProperty ("formatPriority", {})));

        if (const auto* records = parsed.getProperty ("files", {}).getArray())
        {
            for (const auto& item : *records)
            {
                const auto path = item.getProperty ("path", {}).toString();

                if (path.isNotEmpty())
                    files[path] = { static_cast<juce::int64> (item.getProperty ("size", 0)),
                                    static_cast<juce::int64> (item.getProperty ("modified", 0)),
                                    item.getProperty ("error", {}).toString() };
            }
        }
    }

    if (plugins == nullptr)
        return;

    for (const auto& item : *plugins)
    {
        Entry entry;
        entry.file = item.getProperty ("file", {}).toString();
        entry.id = item.getProperty ("id", {}).toString();
        entry.name = item.getProperty ("name", {}).toString();
        entry.vendor = item.getProperty ("vendor", {}).toString();
        entry.version = item.getProperty ("version", {}).toString();
        entry.features = stringsFromVar (item.getProperty ("features", {}));
        entry.kind = item.getProperty ("kind", "instrument").toString();
        entry.category = item.getProperty ("category", {}).toString();
        entry.favorite = static_cast<bool> (item.getProperty ("favorite", false));
        entry.hidden = static_cast<bool> (item.getProperty ("hidden", false));
        entry.missing = static_cast<bool> (item.getProperty ("missing", false));
        entry.lastUsed = static_cast<juce::int64> (item.getProperty ("lastUsed", 0));
        entry.uses = static_cast<int> (item.getProperty ("uses", 0));

        if (entry.category.isEmpty() && ! entry.features.isEmpty())
            entry.category = categoryFromFeatures (entry.features);

        if (entry.file.isNotEmpty() && entry.id.isNotEmpty())
            entries.push_back (entry);
    }
}

void Library::addBuiltins()
{
    // What the user made of them is kept; where they're listed follows the executable.
    std::map<juce::String, Entry> known;

    for (const auto& entry : entries)
        if (Builtins::isBuiltinId (entry.id))
            known[entry.id] = entry;

    entries.erase (std::remove_if (entries.begin(), entries.end(), [] (const Entry& entry) { return Builtins::isBuiltinId (entry.id); }),
                   entries.end());

    for (const auto& descriptor : Builtins::describe())
    {
        Entry entry;
        entry.file = Builtins::file().getFullPathName();
        entry.id = descriptor.id;
        entry.name = descriptor.name;
        entry.vendor = descriptor.vendor;
        entry.version = descriptor.version;
        entry.features = descriptor.features;
        entry.kind = kindFromFeatures (descriptor.features);   // the system tools include instruments
        entry.category = Builtins::categoryOf (descriptor.id);

        if (const auto found = known.find (descriptor.id); found != known.end())
        {
            entry.favorite = found->second.favorite;
            entry.hidden = found->second.hidden;
            entry.lastUsed = found->second.lastUsed;
            entry.uses = found->second.uses;
        }

        entries.push_back (entry);
    }
}

void Library::refreshMadePlugins (const juce::File& justAdded)
{
    if (justAdded != juce::File())
        ignoredFiles.removeString (justAdded.getFullPathName());

    // What the user made of them is kept, by id: a plugin added again under a new name is
    // still the same plugin.
    std::map<juce::String, Entry> known;

    for (const auto& entry : entries)
        if (MadePlugins::isMadeFile (entry.file))
            known[entry.id] = entry;

    entries.erase (std::remove_if (entries.begin(), entries.end(), [] (const Entry& entry) { return MadePlugins::isMadeFile (entry.file); }),
                   entries.end());

    for (const auto& folder : MadePlugins::installed())
    {
        const auto path = folder.getFullPathName();

        if (ignoredFiles.contains (path))   // taken out of the Browser
            continue;

        for (const auto& descriptor : MadePlugins::describe (folder))
        {
            Entry entry;
            entry.file = path;
            entry.id = descriptor.id;
            entry.name = descriptor.name;
            entry.vendor = descriptor.vendor;
            entry.version = descriptor.version;
            entry.features = descriptor.features;
            entry.kind = kindFromFeatures (descriptor.features);
            entry.category = MadePlugins::category;

            if (const auto found = known.find (descriptor.id); found != known.end())
            {
                entry.favorite = found->second.favorite;
                entry.hidden = found->second.hidden;
                entry.lastUsed = found->second.lastUsed;
                entry.uses = found->second.uses;
            }

            entries.push_back (entry);
        }
    }

    save();
}

void Library::save() const
{
    juce::Array<juce::var> records;

    for (const auto& [path, record] : files)
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("path", path);
        item->setProperty ("size", record.size);
        item->setProperty ("modified", record.modified);

        if (record.error.isNotEmpty())
            item->setProperty ("error", record.error);

        records.add (juce::var (item.get()));
    }

    juce::Array<juce::var> plugins;

    for (const auto& entry : entries)
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("file", entry.file);
        item->setProperty ("id", entry.id);
        item->setProperty ("name", entry.name);
        item->setProperty ("vendor", entry.vendor);
        item->setProperty ("version", entry.version);
        item->setProperty ("features", stringsToVar (entry.features));
        item->setProperty ("kind", entry.kind);
        item->setProperty ("category", entry.category);
        item->setProperty ("favorite", entry.favorite);

        if (entry.hidden)
            item->setProperty ("hidden", true);

        item->setProperty ("missing", entry.missing);
        item->setProperty ("lastUsed", entry.lastUsed);
        item->setProperty ("uses", entry.uses);
        plugins.add (juce::var (item.get()));
    }

    juce::DynamicObject::Ptr root (new juce::DynamicObject());
    root->setProperty ("format", formatName);
    root->setProperty ("version", formatVersion);
    root->setProperty ("folders", stringsToVar (folders));
    root->setProperty ("addedFiles", stringsToVar (addedFiles));
    root->setProperty ("ignoredFiles", stringsToVar (ignoredFiles));
    root->setProperty ("systemFoldersAdded", systemFoldersAdded);
    root->setProperty ("clapFolderAdded", clapFolderAdded);
    root->setProperty ("formatPriority", stringsToVar (formatPriority));
    root->setProperty ("hideDuplicatesAfterScans", hideDuplicatesAfterScans);
    root->setProperty ("files", records);
    root->setProperty ("plugins", plugins);

    databaseFile.replaceWithText (juce::JSON::toString (juce::var (root.get())));
}

//==============================================================================
void Library::remember (const juce::File& file, const juce::String& error)
{
    files[file.getFullPathName()] = { sizeOf (file), modifiedOf (file), error };
}

bool Library::isInFolders (const juce::String& path) const
{
    const juce::File file (path);

    for (const auto& folder : folders)
        if (file.isAChildOf (juce::File (folder)))
            return true;

    return false;
}

void Library::setThirdPartyPlugins (bool offered, const juce::File& ownFolder)
{
    thirdPartyOffered = offered;
    ownPluginsFolder = ownFolder;
}

bool Library::isOffered (const juce::String& file) const
{
    return thirdPartyOffered || Builtins::isBuiltinFile (file) || MadePlugins::isMadeFile (file)
        || juce::File (file).isAChildOf (ownPluginsFolder);
}

Library::Entry* Library::find (const juce::String& file, const juce::String& id)
{
    for (auto& entry : entries)
        if (entry.file == file && entry.id == id)
            return &entry;

    return nullptr;
}

juce::Result Library::recordScan (const juce::File& file, const std::vector<Plugin::Descriptor>& found,
                                  const juce::String& error, int& added)
{
    added = 0;
    const auto path = file.getFullPathName();

    if (found.empty())
    {
        remember (file, error);
        return juce::Result::fail (error);
    }

    std::vector<Entry> fresh;

    for (const auto& descriptor : found)
    {
        Entry entry;
        entry.file = path;
        entry.id = descriptor.id;
        entry.name = descriptor.name.isNotEmpty() ? descriptor.name : descriptor.id;
        entry.vendor = descriptor.vendor.isNotEmpty() ? descriptor.vendor : juce::String ("Unknown vendor");
        entry.version = descriptor.version;
        entry.features = descriptor.features;
        entry.kind = kindFromFeatures (descriptor.features);
        entry.category = categoryFromFeatures (descriptor.features);

        // What the user made of it survives a rescan.
        if (const auto* known = find (path, entry.id))
        {
            entry.favorite = known->favorite;
            entry.hidden = known->hidden;
            entry.lastUsed = known->lastUsed;
            entry.uses = known->uses;
        }
        else
        {
            ++added;
        }

        fresh.push_back (entry);
    }

    // The file's plugins are exactly what it holds now.
    entries.erase (std::remove_if (entries.begin(), entries.end(), [&path] (const Entry& entry) { return entry.file == path; }),
                   entries.end());
    entries.insert (entries.end(), fresh.begin(), fresh.end());
    remember (file, {});
    return juce::Result::ok();
}

void Library::addFile (const juce::File& file)
{
    const auto path = file.getFullPathName();
    ignoredFiles.removeString (path);

    if (! isInFolders (path))
        addedFiles.addIfNotAlreadyThere (path);

    // Read on the next scan, like a new file in a Plugin Folder.
    files.erase (path);
    save();
}

void Library::removeFile (const juce::String& file)
{
    // A copy: the caller's string may belong to an entry this is about to erase.
    const auto path = file;

    entries.erase (std::remove_if (entries.begin(), entries.end(), [&path] (const Entry& entry) { return entry.file == path; }),
                   entries.end());
    addedFiles.removeString (path);
    files.erase (path);

    // A scan would find it in its folder again (a made plugin: in its own folder): remember
    // that it was taken out.
    if (isInFolders (path) || MadePlugins::isMadeFile (path))
        ignoredFiles.addIfNotAlreadyThere (path);

    save();
}

void Library::addFolder (const juce::File& folder)
{
    if (! folder.isDirectory())
        return;

    folders.addIfNotAlreadyThere (folder.getFullPathName());
    save();
}

void Library::replaceFolder (const juce::String& oldPath, const juce::File& newFolder)
{
    const auto at = folders.indexOf (oldPath);

    if (at < 0 || ! newFolder.isDirectory() || newFolder.getFullPathName() == oldPath)
        return;

    removeFolder (oldPath);   // its plugins go, as when it's removed

    if (! folders.contains (newFolder.getFullPathName()))
        folders.insert (juce::jmin (at, folders.size()), newFolder.getFullPathName());

    save();
}

juce::Array<juce::File> Library::getDefaultFolders()
{
    juce::Array<juce::File> list;
    const auto vst3 = Vst3Plugin::getDefaultFolders();

    for (int i = 0; i < vst3.getNumPaths(); ++i)
        list.addIfNotAlreadyThere (vst3[i]);

   #if JUCE_WINDOWS
    // Where CLAP plugins are installed: C:\Program Files\Common Files\CLAP.
    list.addIfNotAlreadyThere (juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory)
                                   .getChildFile ("Common Files").getChildFile ("CLAP"));
   #elif JUCE_MAC
    list.addIfNotAlreadyThere (juce::File ("/Library/Audio/Plug-Ins/CLAP"));
    list.addIfNotAlreadyThere (juce::File ("~/Library/Audio/Plug-Ins/CLAP"));
   #else
    list.addIfNotAlreadyThere (juce::File ("~/.clap"));
    list.addIfNotAlreadyThere (juce::File ("/usr/lib/clap"));
   #endif

    return list;
}

int Library::addDefaultFolders()
{
    int added = 0;

    for (const auto& folder : getDefaultFolders())
    {
        if (folder.isDirectory() && ! folders.contains (folder.getFullPathName()))
        {
            folders.add (folder.getFullPathName());
            ++added;
        }
    }

    if (added > 0)
        save();

    return added;
}

void Library::removeFolder (const juce::String& folderPath)
{
    // A copy: the caller's string may be the entry in folders this is about to remove.
    const auto path = folderPath;

    folders.removeString (path);
    const juce::File folder (path);

    // Plugins from that folder go, unless they were also added by hand or another folder holds them.
    const auto goes = [this, &folder] (const juce::String& file)
    {
        return juce::File (file).isAChildOf (folder) && ! addedFiles.contains (file) && ! isInFolders (file)
               && ! Builtins::isBuiltinFile (file);   // Sonify's own stay, wherever Sonify is
    };

    entries.erase (std::remove_if (entries.begin(), entries.end(), [&goes] (const Entry& entry) { return goes (entry.file); }),
                   entries.end());

    for (auto record = files.begin(); record != files.end();)
        record = goes (record->first) ? files.erase (record) : std::next (record);

    for (int i = ignoredFiles.size(); --i >= 0;)
        if (juce::File (ignoredFiles[i]).isAChildOf (folder))
            ignoredFiles.remove (i);

    save();
}

std::vector<juce::File> Library::prepareScan (bool full)
{
    std::vector<juce::File> candidates;

    for (const auto& folder : folders)
        findPluginFiles (juce::File (folder), candidates, 0);

    for (const auto& path : addedFiles)
        candidates.push_back (juce::File (path));

    std::vector<juce::File> toRead;
    std::set<juce::String> seen;

    for (const auto& file : candidates)
    {
        const auto path = file.getFullPathName();

        // Third-party plugins off for this run: they wait, still new or changed, for the next.
        if (! file.exists() || ignoredFiles.contains (path) || ! isOffered (path) || ! seen.insert (path).second)
            continue;

        // A file that failed before is only tried again once it changes (or on a full scan).
        const auto known = files.find (path);
        const bool changed = known == files.end() || known->second.size != sizeOf (file) || known->second.modified != modifiedOf (file);

        if (full || changed)
            toRead.push_back (file);
    }

    // Plugins whose files have gone are missing; when the files are back, they're not.
    bool updated = false;

    for (auto& entry : entries)
    {
        const bool gone = ! juce::File (entry.file).exists();

        if (entry.missing != gone)
        {
            entry.missing = gone;
            updated = true;
        }
    }

    if (updated)
        save();

    return toRead;
}

void Library::setFavorite (const juce::String& file, const juce::String& id, bool favorite)
{
    if (auto* entry = find (file, id); entry != nullptr && entry->favorite != favorite)
    {
        entry->favorite = favorite;
        save();
    }
}

void Library::markUsed (const juce::String& file, const juce::String& id)
{
    if (auto* entry = find (file, id))
    {
        entry->lastUsed = juce::Time::currentTimeMillis();
        ++entry->uses;
        save();
    }
}

//==============================================================================
void Library::setHidden (const std::vector<std::pair<juce::String, juce::String>>& plugins, bool hidden)
{
    bool changed = false;

    for (const auto& [file, id] : plugins)
    {
        if (auto* entry = find (file, id); entry != nullptr && entry->hidden != hidden)
        {
            entry->hidden = hidden;
            changed = true;
        }
    }

    if (changed)
        save();
}

int Library::hideDuplicates()
{
    // The rank of a format: its place in the order (formats not in it after, as they are).
    const auto rankOf = [this] (const Entry& entry)
    {
        const auto at = formatPriority.indexOf (entry.format());
        return at >= 0 ? at : formatPriority.size();
    };

    // The same plugin: its name and vendor, whatever their case.
    std::map<juce::String, std::vector<Entry*>> groups;

    for (auto& entry : entries)
        if (! Builtins::isBuiltinId (entry.id) && ! entry.missing && isOffered (entry.file))
            groups[entry.name.trim().toLowerCase() + "\n" + entry.vendor.trim().toLowerCase()].push_back (&entry);

    int hidden = 0;

    for (auto& [key, group] : groups)
    {
        // One format only: nothing to choose between (two plugins of one format stay as they are).
        std::set<juce::String> formats;

        for (const auto* entry : group)
            formats.insert (entry->format());

        if (formats.size() < 2)
            continue;

        int best = std::numeric_limits<int>::max();

        for (const auto* entry : group)
            best = juce::jmin (best, rankOf (*entry));

        for (auto* entry : group)
        {
            // The others go; what was hidden by hand stays so.
            if (rankOf (*entry) != best && ! entry->hidden)
            {
                entry->hidden = true;
                ++hidden;
            }
        }
    }

    save();
    return hidden;
}

void Library::setFormatPriorityQuietly (const juce::StringArray& formats)
{
    juce::StringArray order;

    for (const auto& format : formats)
        if (defaultFormatPriority().contains (format))
            order.addIfNotAlreadyThere (format);

    for (const auto& format : defaultFormatPriority())
        order.addIfNotAlreadyThere (format);

    formatPriority = order;
}

void Library::setFormatPriority (const juce::StringArray& formats)
{
    setFormatPriorityQuietly (formats);
    save();
}

void Library::setHideDuplicatesAfterScans (bool on)
{
    if (hideDuplicatesAfterScans == on)
        return;

    hideDuplicatesAfterScans = on;
    save();
}

std::vector<Library::Blocked> Library::getBlocked() const
{
    std::vector<Blocked> list;

    for (const auto& [path, record] : files)
        if (record.error.isNotEmpty() && isOffered (path))
            list.push_back ({ path, record.error, false });

    for (const auto& path : ignoredFiles)
        list.push_back ({ path, "Removed from the Browser.", true });

    std::sort (list.begin(), list.end(), [] (const Blocked& a, const Blocked& b)
    {
        return juce::File (a.file).getFileName().compareIgnoreCase (juce::File (b.file).getFileName()) < 0;
    });

    return list;
}

void Library::unblock (const juce::StringArray& paths)
{
    for (const auto& path : paths)
    {
        // A failed file is forgotten, so the next scan reads it; a removed one may be found again.
        if (const auto record = files.find (path); record != files.end() && record->second.error.isNotEmpty())
            files.erase (record);

        if (ignoredFiles.contains (path))
        {
            ignoredFiles.removeString (path);

            // Added by hand before: listed again, to be read.
            if (! isInFolders (path) && ! MadePlugins::isMadeFile (path) && juce::File (path).exists())
                addedFiles.addIfNotAlreadyThere (path);
        }
    }

    save();
}

void Library::resetBlocklist()
{
    juce::StringArray all;

    for (const auto& blocked : getBlocked())
        all.add (blocked.file);

    unblock (all);
}

juce::int64 Library::fileTimeOf (const juce::String& path) const
{
    if (const auto record = files.find (path); record != files.end() && record->second.modified != 0)
        return record->second.modified;

    return modifiedOf (juce::File (path));
}

juce::String Library::folderLabelFor (const juce::String& path) const
{
    if (Builtins::isBuiltinFile (path))
        return "Built-in";

    if (MadePlugins::isMadeFile (path))
        return MadePlugins::folderLabel;

    const juce::File file (path);
    juce::File deepest;

    // The deepest Plugin Folder holding it, then the sub-folders below that.
    for (const auto& folder : folders)
    {
        const juce::File candidate (folder);

        if (file.isAChildOf (candidate) && (deepest == juce::File() || candidate.isAChildOf (deepest)))
            deepest = candidate;
    }

    if (deepest == juce::File())
        return "Added files";

    const auto relative = file.getParentDirectory().getRelativePathFrom (deepest).replaceCharacter ('\\', '/');

    if (relative.isEmpty() || relative == ".")
        return deepest.getFileName();

    return deepest.getFileName() + " / " + relative.replace ("/", " / ");
}
