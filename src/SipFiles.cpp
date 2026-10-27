// D:\workspace\Fanan_web_hub\src\SipFiles.cpp

#include "SipFiles.h"

#include "Preferences.h"

#include <algorithm>

namespace Sip
{
namespace
{
    constexpr int maxEntries = 4000;
    constexpr juce::int64 maxUnpacked = (juce::int64) 1024 * 1024 * 1024;   // a sampler's panel pictures and all: 1 GB at most
    constexpr const char* stampName = ".sip-stamp";
    constexpr const char* oldFormatFolder = "Old format";

    juce::CriticalSection& cacheLock()
    {
        static juce::CriticalSection lock;
        return lock;
    }

    juce::File cacheFolder()
    {
        return SettingsFolder::get().getChildFile ("Plugin Cache");
    }

    /** A zip entry's path that stays inside the folder it's unpacked into. */
    bool isSafePath (const juce::String& path)
    {
        return path.isNotEmpty() && path.length() < 400 && ! path.startsWithChar ('/') && ! path.startsWithChar ('\\')
            && ! path.containsChar (':') && ! path.contains ("..") && ! path.containsChar ('\\');
    }

    juce::String asText (const juce::MemoryBlock& data)
    {
        if (! juce::CharPointer_UTF8::isValidString (static_cast<const char*> (data.getData()), (int) data.getSize()))
            return {};

        return juce::String::fromUTF8 (static_cast<const char*> (data.getData()), (int) data.getSize());
    }

    bool readZipEntry (juce::ZipFile& zip, int index, size_t limit, juce::MemoryBlock& into)
    {
        const auto* entry = zip.getEntry (index);

        if (entry == nullptr || entry->uncompressedSize < 0 || (size_t) entry->uncompressedSize > limit)
            return false;

        std::unique_ptr<juce::InputStream> stream (zip.createStreamForEntry (index));

        if (stream == nullptr)
            return false;

        into.reset();
        juce::MemoryOutputStream out (into, false);
        const auto copied = out.writeFromInputStream (*stream, (juce::int64) limit + 1);
        out.flush();
        return copied >= 0 && (size_t) copied <= limit;
    }

    /** The manifest of a made plugin, from its plugin.json. */
    bool manifestFromPlugin (const juce::var& info, Manifest& manifest)
    {
        if (info.getProperty ("format", {}).toString() != "Sonify plugin")
            return false;

        const auto kind = info.getProperty ("kind", "instrument").toString();
        manifest.kind = kind == "instrument" ? Kind::instrument : Kind::fx;
        manifest.tags.clear();

        if (kind == "widget")
            manifest.tags.add ("utility");

        manifest.id = info.getProperty ("id", {}).toString();
        manifest.name = info.getProperty ("name", {}).toString();
        manifest.vendor = info.getProperty ("vendor", {}).toString();
        manifest.version = juce::jmax (0, (int) info.getProperty ("version", 0));
        return manifest.id.isNotEmpty();
    }

    /** The manifest of a MIDI device, from its device.json. */
    bool manifestFromDevice (const juce::var& info, Manifest& manifest)
    {
        if (! info.isObject() || (int) info.getProperty ("format", 0) != 1)
            return false;

        manifest.kind = Kind::midi;
        manifest.tags.clear();
        manifest.id = info.getProperty ("id", {}).toString();
        manifest.name = info.getProperty ("name", {}).toString();
        manifest.vendor = info.getProperty ("vendor", {}).toString();
        manifest.version = juce::jmax (0, (int) info.getProperty ("version", 0));
        return manifest.id.isNotEmpty();
    }

    juce::String stampOf (const juce::File& sip)
    {
        return juce::String (sip.getSize()) + "/" + juce::String (sip.getLastModificationTime().toMilliseconds());
    }

    /** The start of its places' names: one for each .sip (by its path). */
    juce::String prefixFor (const juce::File& sip)
    {
        const auto path = sip.getFullPathName();
        const auto key = juce::String::toHexString ((juce::int64) path.toLowerCase().hashCode64()).paddedLeft ('0', 16);
        const auto name = juce::File::createLegalFileName (sip.getFileNameWithoutExtension()).substring (0, 40).trim();
        return (name.isNotEmpty() ? name : juce::String ("plugin")) + " " + key + " ";
    }

    /** Its place for this version of it: a new .sip gets a new place, so a track still playing the
        one before keeps its files (its old place goes once nothing holds it). */
    juce::File placeFor (const juce::File& sip, const juce::String& stamp)
    {
        const auto version = juce::String::toHexString ((juce::int64) stamp.hashCode64()).paddedLeft ('0', 16).substring (0, 8);
        return cacheFolder().getChildFile (prefixFor (sip) + version);
    }

    juce::File freeIn (const juce::File& where, const juce::String& base, const juce::String& extensionToUse)
    {
        auto file = where.getChildFile (base + extensionToUse);

        for (int n = 2; file.exists() && n < 1000; ++n)
            file = where.getChildFile (base + " " + juce::String (n) + extensionToUse);

        return file;
    }

    juce::String cleanName (const juce::String& name)
    {
        auto clean = juce::File::createLegalFileName (name.trim()).trim().substring (0, 60);

        // Never only dots ("." or ".."): a name, not a way out of the folder.
        if (clean.containsOnly ("."))
            clean.clear();

        return clean.isNotEmpty() ? clean : juce::String ("Plugin");
    }

    /** A zip's files unpacked into a folder, checked first: none leaves it, no links, not too many, not too big. */
    juce::Result unzipInto (const juce::File& zipFile, const juce::File& target)
    {
        juce::ZipFile zip (zipFile);

        if (zip.getNumEntries() == 0 || zip.getNumEntries() > maxEntries)
            return juce::Result::fail (zipFile.getFileName() + " isn't one Sonify can read.");

        juce::int64 total = 0;

        for (int i = 0; i < zip.getNumEntries(); ++i)
        {
            const auto* entry = zip.getEntry (i);
            total += juce::jmax ((juce::int64) 0, entry->uncompressedSize);

            if (! isSafePath (entry->filename) || entry->isSymbolicLink || entry->uncompressedSize < 0 || total > maxUnpacked)
                return juce::Result::fail (zipFile.getFileName() + " isn't one Sonify can read.");
        }

        if (! target.createDirectory())
            return juce::Result::fail ("Couldn't make the folder " + target.getFullPathName());

        for (int i = 0; i < zip.getNumEntries(); ++i)
        {
            const auto* entry = zip.getEntry (i);

            if (entry->filename.endsWithChar ('/'))
                continue;   // a folder: made with the files in it

            if (const auto result = zip.uncompressEntry (i, target, true); result.failed())
                return juce::Result::fail ("Couldn't unpack " + zipFile.getFileName() + ": " + result.getErrorMessage());
        }

        return juce::Result::ok();
    }
}

//==============================================================================
juce::String kindName (Kind kind)
{
    switch (kind)
    {
        case Kind::instrument:  return "instrument";
        case Kind::fx:          return "fx";
        case Kind::midi:        return "midi";
        case Kind::unknown:     break;
    }

    return {};
}

Kind kindFromName (const juce::String& name)
{
    const auto lower = name.trim().toLowerCase();

    if (lower == "instrument")                                          return Kind::instrument;
    if (lower == "fx" || lower == "effect" || lower == "widget")       return Kind::fx;
    if (lower == "midi" || lower == "midifx" || lower == "midi fx")    return Kind::midi;
    return Kind::unknown;
}

juce::var Manifest::toVar() const
{
    juce::DynamicObject::Ptr o (new juce::DynamicObject());
    o->setProperty ("format", formatTag);
    o->setProperty ("formatVersion", 1);
    o->setProperty ("kind", kindName (kind));

    juce::Array<juce::var> tagList;

    for (const auto& tag : tags)
        tagList.add (tag);

    o->setProperty ("tags", tagList);
    o->setProperty ("id", id);
    o->setProperty ("name", name);
    o->setProperty ("vendor", vendor);
    o->setProperty ("version", version);
    return juce::var (o.get());
}

Manifest Manifest::fromVar (const juce::var& value)
{
    Manifest manifest;

    if (value.getProperty ("format", {}).toString() != formatTag)
        return manifest;

    manifest.kind = kindFromName (value.getProperty ("kind", {}).toString());
    manifest.id = value.getProperty ("id", {}).toString().substring (0, 120);
    manifest.name = value.getProperty ("name", {}).toString().substring (0, 80);
    manifest.vendor = value.getProperty ("vendor", {}).toString().substring (0, 80);
    manifest.version = juce::jmax (0, (int) value.getProperty ("version", 0));

    if (const auto* list = value.getProperty ("tags", {}).getArray())
        for (const auto& tag : *list)
            if (const auto text = tag.toString().trim().toLowerCase().substring (0, 30); text.isNotEmpty())
                manifest.tags.addIfNotAlreadyThere (text);

    return manifest;
}

juce::String manifestText (const Manifest& manifest)
{
    return juce::JSON::toString (manifest.toVar());
}

//==============================================================================
juce::File folder()
{
    return UserData::getFolder().getChildFile ("Plugins");
}

bool isSipFile (const juce::File& file)
{
    return file.getFileName().endsWithIgnoreCase (extension);
}

bool readEntry (const juce::File& sip, const juce::String& path, juce::MemoryBlock& into, size_t limit)
{
    if (! sip.existsAsFile())
        return false;

    juce::ZipFile zip (sip);
    const auto index = zip.getIndexOfFileName (path);
    return index >= 0 && readZipEntry (zip, index, limit, into);
}

bool readManifest (const juce::File& sip, Manifest& manifest)
{
    if (! sip.existsAsFile())
        return false;

    juce::ZipFile zip (sip);

    if (zip.getNumEntries() == 0 || zip.getNumEntries() > maxEntries)
        return false;

    juce::MemoryBlock data;

    if (const auto index = zip.getIndexOfFileName (manifestName); index >= 0 && readZipEntry (zip, index, 65536, data))
    {
        manifest = Manifest::fromVar (juce::JSON::parse (asText (data)));

        if (manifest.kind != Kind::unknown && manifest.id.isNotEmpty())
            return true;
    }

    // Without its sip.json: what it holds says what it is.
    if (const auto index = zip.getIndexOfFileName ("plugin.json"); index >= 0 && readZipEntry (zip, index, 65536, data))
        return manifestFromPlugin (juce::JSON::parse (asText (data)), manifest);

    if (const auto index = zip.getIndexOfFileName ("device.json"); index >= 0 && readZipEntry (zip, index, 65536, data))
        return manifestFromDevice (juce::JSON::parse (asText (data)), manifest);

    return false;
}

juce::File unpacked (const juce::File& sip, juce::String& error)
{
    if (! sip.existsAsFile())
    {
        error = sip.getFullPathName() + " isn't there.";
        return {};
    }

    const juce::ScopedLock sl (cacheLock());
    const auto stamp = stampOf (sip);
    const auto place = placeFor (sip, stamp);

    if (place.getChildFile (stampName).loadFileAsString() == stamp)
        return place;

    // Unpacked afresh: into a folder beside it, then put in its place, so a half-unpacked
    // plugin is never played.
    const auto fresh = place.getSiblingFile (place.getFileName() + " new");
    fresh.deleteRecursively();

    if (const auto result = unzipInto (sip, fresh); result.failed())
    {
        fresh.deleteRecursively();
        error = result.getErrorMessage();
        return {};
    }

    place.deleteRecursively();

    if (! fresh.moveFileTo (place))
    {
        fresh.deleteRecursively();
        error = "Couldn't unpack " + sip.getFileName() + " into " + place.getFullPathName();
        return {};
    }

    place.getChildFile (stampName).replaceWithText (stamp);

    // Its places from before go, when nothing holds them any more (else next time).
    const auto prefix = prefixFor (sip);

    for (const auto& item : juce::RangedDirectoryIterator (cacheFolder(), false, "*", juce::File::findDirectories))
        if (const auto other = item.getFile(); other != place && other.getFileName().startsWith (prefix))
            other.deleteRecursively();

    return place;
}

juce::Result packFolder (const juce::File& source, const Manifest& manifest, const juce::File& sip)
{
    if (! source.isDirectory())
        return juce::Result::fail ("There's nothing to pack: " + source.getFullPathName() + " isn't there.");

    juce::ZipFile::Builder zip;
    const auto text = manifestText (manifest).toUTF8();
    zip.addEntry (std::make_unique<juce::MemoryInputStream> (text.getAddress(), text.sizeInBytes() - 1, true), 9, manifestName,
                  juce::Time::getCurrentTime());

    int count = 0;

    for (const auto& item : juce::RangedDirectoryIterator (source, true, "*", juce::File::findFiles))
    {
        const auto file = item.getFile();
        const auto path = file.getRelativePathFrom (source).replaceCharacter ('\\', '/');

        if (path == manifestName || file.getFileName() == stampName || file.isSymbolicLink() || ! isSafePath (path))
            continue;

        if (++count > maxEntries)
            return juce::Result::fail (source.getFileName() + " has too many files to pack.");

        // Pictures and WebAssembly are compressed already, or nearly: stored as they are, quicker.
        const auto stored = file.hasFileExtension ("png;jpg;jpeg;wasm;blob");
        zip.addFile (file, stored ? 0 : 9, path);
    }

    if (! sip.getParentDirectory().createDirectory())
        return juce::Result::fail ("Couldn't make the folder " + sip.getParentDirectory().getFullPathName());

    juce::TemporaryFile temp (sip);

    {
        juce::FileOutputStream out (temp.getFile());

        if (! out.openedOk() || ! zip.writeToStream (out, nullptr))
            return juce::Result::fail ("Couldn't write " + sip.getFileName());

        out.flush();

        if (out.getStatus().failed())
            return juce::Result::fail ("Couldn't write " + sip.getFileName() + ": " + out.getStatus().getErrorMessage());
    }

    if (! temp.overwriteTargetFileWithTemporary())
        return juce::Result::fail ("Couldn't write " + sip.getFullPathName());

    return juce::Result::ok();
}

juce::Array<juce::File> list (Kind kind)
{
    juce::Array<juce::File> found;
    const auto home = folder();

    if (! home.isDirectory())
        return found;

    for (const auto& item : juce::RangedDirectoryIterator (home, false, juce::String ("*") + extension, juce::File::findFiles))
    {
        Manifest manifest;

        if (readManifest (item.getFile(), manifest) && (kind == Kind::unknown || manifest.kind == kind))
            found.add (item.getFile());
    }

    std::sort (found.begin(), found.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0;
    });

    return found;
}

juce::File freeFile (const juce::String& name, const juce::File& except)
{
    const auto home = folder();
    const auto base = cleanName (name);
    auto file = home.getChildFile (base + extension);

    for (int n = 2; file.exists() && file != except && n < 1000; ++n)
        file = home.getChildFile (base + " " + juce::String (n) + extension);

    return file;
}

juce::File findById (const juce::String& id, Kind kind)
{
    if (id.isEmpty())
        return {};

    for (const auto& file : list (kind))
    {
        Manifest manifest;

        if (readManifest (file, manifest) && manifest.id == id)
            return file;
    }

    return {};
}

//==============================================================================
int migrate()
{
    const auto home = folder();
    const auto old = home.getChildFile (oldFormatFolder);
    const auto userData = UserData::getFolder();
    int turned = 0;

    const auto keepOld = [&old] (const juce::File& item)
    {
        old.createDirectory();
        const auto target = freeIn (old, item.getFileNameWithoutExtension(), item.getFileExtension());
        return item.moveFileTo (target);
    };

    // Made plugins: "<name>.sonifyplugin" folders.
    const auto madeFolder = userData.getChildFile ("Plugins Creator").getChildFile ("Plugins");

    if (madeFolder.isDirectory())
    {
        for (const auto& item : juce::RangedDirectoryIterator (madeFolder, false, "*.sonifyplugin", juce::File::findDirectories))
        {
            const auto pluginFolder = item.getFile();
            Manifest manifest;

            if (! manifestFromPlugin (juce::JSON::parse (pluginFolder.getChildFile ("plugin.json")), manifest))
                continue;

            // Already turned (its .sip there, with its id): only the old one moves.
            auto target = findById (manifest.id);

            if (target == juce::File())
            {
                target = freeFile (manifest.name.isNotEmpty() ? manifest.name : pluginFolder.getFileNameWithoutExtension());

                if (packFolder (pluginFolder, manifest, target).failed())
                    continue;

                ++turned;
            }

            keepOld (pluginFolder);
        }
    }

    // MIDI devices: "<name>.sonidev" files (zips: their files go into the .sip as they are).
    const auto devicesFolder = userData.getChildFile ("Devices");

    if (devicesFolder.isDirectory())
    {
        for (const auto& item : juce::RangedDirectoryIterator (devicesFolder, false, "*.sonidev", juce::File::findFiles))
        {
            const auto device = item.getFile();
            Manifest manifest;
            juce::MemoryBlock data;

            if (! readEntry (device, "device.json", data, 65536) || ! manifestFromDevice (juce::JSON::parse (asText (data)), manifest))
                continue;

            auto target = findById (manifest.id, Kind::midi);

            if (target == juce::File())
            {
                const auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                      .getChildFile ("Sonify sip " + juce::Uuid().toString().substring (0, 8));
                target = freeFile (manifest.name.isNotEmpty() ? manifest.name : device.getFileNameWithoutExtension());
                const auto ok = unzipInto (device, temp).wasOk() && packFolder (temp, manifest, target).wasOk();
                temp.deleteRecursively();

                if (! ok)
                    continue;

                ++turned;
            }

            keepOld (device);
        }
    }

    return turned;
}
}
