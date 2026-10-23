// D:\workspace\Fanan_web_hub\src\CodeReference.cpp

#include "CodeReference.h"

#include <map>
#include <mutex>

namespace
{
    // Code and notes, in the languages audio code comes in.
    const juce::StringArray codeTypes { "cpp", "cc", "cxx", "c", "h", "hpp", "hh", "hxx", "inl", "ipp", "mm", "m",
                                        "py", "js", "ts", "java", "cs", "rs", "go", "swift", "kt", "lua", "jsfx", "eel",
                                        "dsp", "lib",          // Faust
                                        "gendsp", "genexpr", "maxpat", "pd", "scd", "csd", "orc", "sco", "ck",
                                        "glsl", "frag", "vert", "hlsl",
                                        "txt", "md", "rst", "json", "xml", "jucer", "cmake", "ini", "cfg",
                                        "yaml", "yml", "toml", "csv" };

    // Folders that aren't the user's own code: builds, dependencies, generated code. (Hidden
    // folders, .git and the like, are left out too.)
    const juce::StringArray skippedFolders { "build", "builds", "out", "bin", "obj", "x64", "x86", "arm64", "debug", "release",
                                             "node_modules", "__pycache__", "__macosx", "cmake-build-debug", "cmake-build-release",
                                             "juce", "jucelibrarycode", "juce_modules", "third_party", "thirdparty", "external",
                                             "vendor", "deps" };

    /** A name that's safe in a folder: no slashes or characters a file system refuses. */
    juce::String safeName (const juce::String& name)
    {
        auto clean = juce::File::createLegalFileName (name.trim());
        return clean.isEmpty() || clean == "." || clean == ".." ? juce::String ("code") : clean;
    }

    /** A file's path inside what's brought, made safe ("Source/Filter.cpp"), or empty when it's
        left out: in a build, dependency or hidden folder, hidden itself, or not code. */
    juce::String keptPath (const juce::String& relative)
    {
        const auto parts = juce::StringArray::fromTokens (relative.replaceCharacter ('\\', '/'), "/", {});
        juce::StringArray clean;

        for (int i = 0; i < parts.size(); ++i)
        {
            const auto part = parts[i].trim();

            if (part.isEmpty() || part == ".")
                continue;

            if (part == ".." || part.startsWithChar ('.') || part.containsChar (':'))
                return {};

            if (i < parts.size() - 1 && skippedFolders.contains (part.toLowerCase()))
                return {};

            clean.add (safeName (part));
        }

        if (clean.isEmpty() || ! CodeReference::isCodeName (clean[clean.size() - 1]))
            return {};

        return clean.joinIntoString ("/");
    }

    /** A text's lines, without the empty one after a last line break. */
    juce::StringArray linesOf (const juce::String& text)
    {
        auto lines = juce::StringArray::fromLines (text);

        if (! lines.isEmpty() && lines[lines.size() - 1].isEmpty())
            lines.remove (lines.size() - 1);

        return lines;
    }

    /** A file's number of lines, remembered while it's unchanged (the list goes with every
        request, and a codebase can be big). */
    int lineCountOf (const juce::File& file)
    {
        struct Known
        {
            juce::Time modified;
            juce::int64 size = 0;
            int lines = 0;
        };

        static std::mutex lock;
        static std::map<juce::String, Known> known;

        const auto modified = file.getLastModificationTime();
        const auto size = file.getSize();
        const auto key = file.getFullPathName();

        {
            const std::lock_guard<std::mutex> held (lock);

            if (const auto found = known.find (key); found != known.end() && found->second.modified == modified && found->second.size == size)
                return found->second.lines;
        }

        const auto lines = linesOf (file.loadFileAsString()).size();
        const std::lock_guard<std::mutex> held (lock);

        if (known.size() > 5000)
            known.clear();

        known[key] = { modified, size, lines };
        return lines;
    }
}

//==============================================================================
CodeReference::CodeReference (juce::File referenceFolder)
    : folder (std::move (referenceFolder))
{
}

bool CodeReference::isCodeName (const juce::String& fileName)
{
    return fileName.containsChar ('.') && ! fileName.startsWithChar ('.')
        && codeTypes.contains (fileName.fromLastOccurrenceOf (".", false, false).toLowerCase());
}

bool CodeReference::isCodeFile (const juce::File& file)
{
    return isCodeName (file.getFileName());
}

bool CodeReference::canTake (const juce::File& file)
{
    return file.isDirectory() || (file.existsAsFile() && (isCodeFile (file) || file.hasFileExtension ("zip")));
}

juce::String CodeReference::filePatterns()
{
    juce::StringArray patterns;

    for (const auto& type : codeTypes)
        patterns.add ("*." + type);

    patterns.add ("*.zip");
    return patterns.joinIntoString (";");
}

//==============================================================================
juce::Array<juce::File> CodeReference::files() const
{
    juce::Array<juce::File> found;

    if (folder.isDirectory())
        for (const auto& entry : juce::RangedDirectoryIterator (folder, true, "*", juce::File::findFiles))
            found.add (entry.getFile());

    found.sort();
    return found;
}

juce::String CodeReference::pathOf (const juce::File& file) const
{
    return juce::String (prefix) + file.getRelativePathFrom (folder).replaceCharacter ('\\', '/');
}

bool CodeReference::resolve (const juce::String& path, juce::File& file) const
{
    auto clean = path.trim().replaceCharacter ('\\', '/');

    if (clean.startsWith (prefix))
        clean = clean.substring ((int) std::strlen (prefix));

    if (clean.isEmpty() || clean.startsWithChar ('/') || clean.containsChar (':'))
        return false;

    // No climbing out: a ".." segment (or one of only dots and spaces, which Windows may read
    // as one) is refused. ".." inside a name ("v1..2.cpp") is fine: keptPath keeps such files.
    for (const auto& part : juce::StringArray::fromTokens (clean, "/", {}))
        if (part.contains ("..") && part.containsOnly (". "))
            return false;

    file = folder.getChildFile (clean);
    return file.existsAsFile() && file.isAChildOf (folder);
}

juce::int64 CodeReference::totalBytes() const
{
    juce::int64 total = 0;

    for (const auto& file : files())
        total += file.getSize();

    return total;
}

bool CodeReference::isEmpty() const
{
    return files().isEmpty();
}

//==============================================================================
CodeReference::Added CodeReference::add (const juce::Array<juce::File>& sources)
{
    Added added;
    auto count = files().size();
    auto total = totalBytes();
    bool full = false;
    int moreProblems = 0;

    const auto problem = [&] (const juce::String& text)
    {
        if (added.problems.size() < 6)
            added.problems.add (text);
        else
            ++moreProblems;
    };

    // One file in (relative: its path in the reference folder), under the limits.
    const auto keep = [&] (const juce::String& relative, juce::int64 size, const std::function<bool (const juce::File&)>& write)
    {
        if (size > maxFileBytes)
        {
            problem (relative + " (over " + juce::String (maxFileBytes / 1024) + " KB)");
            return;
        }

        if (count >= maxFiles || total + size > maxTotalBytes)
        {
            full = true;
            return;
        }

        const auto target = folder.getChildFile (relative);

        if (! target.isAChildOf (folder) || ! target.getParentDirectory().createDirectory()
            || (target.exists() && ! target.deleteFile()) || ! write (target))
        {
            problem (relative + " (couldn't be kept)");
            return;
        }

        ++count;
        total += size;
        added.paths.add (pathOf (target));
    };

    for (const auto& source : sources)
    {
        const auto before = added.paths.size();

        if (source.isDirectory())
        {
            // A folder: its code, in a folder of its name, without builds and libraries.
            const auto base = safeName (source.getFileName());

            for (const auto& entry : juce::RangedDirectoryIterator (source, true, "*", juce::File::findFiles))
            {
                const auto file = entry.getFile();
                const auto relative = keptPath (file.getRelativePathFrom (source));

                if (relative.isEmpty())
                {
                    ++added.skipped;
                    continue;
                }

                keep (base + "/" + relative, file.getSize(), [&file] (const juce::File& target) { return file.copyFileTo (target); });
            }
        }
        else if (source.hasFileExtension ("zip"))
        {
            // A zip: the same, from inside it.
            juce::ZipFile zip (source);
            const auto base = safeName (source.getFileNameWithoutExtension());

            if (zip.getNumEntries() == 0)
            {
                problem (source.getFileName() + " (it isn't a zip that opens)");
                continue;
            }

            for (int i = 0; i < zip.getNumEntries(); ++i)
            {
                const auto* entry = zip.getEntry (i);

                if (entry == nullptr || entry->isSymbolicLink || entry->filename.endsWithChar ('/') || entry->filename.endsWithChar ('\\'))
                    continue;

                const auto relative = keptPath (entry->filename);

                if (relative.isEmpty())
                {
                    ++added.skipped;
                    continue;
                }

                keep (base + "/" + relative, entry->uncompressedSize,
                      [&zip, i] (const juce::File& target)
                      {
                          std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (i));

                          if (in == nullptr)
                              return false;

                          juce::FileOutputStream out (target);
                          return out.openedOk() && out.writeFromInputStream (*in, -1) >= 0 && (out.flush(), out.getStatus().wasOk());
                      });
            }
        }
        else if (isCodeFile (source))
        {
            keep (safeName (source.getFileName()), source.getSize(), [&source] (const juce::File& target) { return source.copyFileTo (target); });
            continue;
        }
        else
        {
            problem (source.getFileName() + " (not code or text)");
            continue;
        }

        if (added.paths.size() == before && ! full)
            problem (source.getFileName() + " (no code in it)");
    }

    if (full)
        problem ("some files didn't fit (the code kept can be up to " + juce::String (maxFiles) + " files, "
                 + juce::String (maxTotalBytes / (1024 * 1024)) + " MB in all)");

    if (moreProblems > 0)
        added.problems.add ("and " + juce::String (moreProblems) + " more");

    return added;
}

//==============================================================================
juce::String CodeReference::listing (int maxEntries, const juce::StringArray& only) const
{
    juce::Array<juce::File> shown;

    if (only.isEmpty())
    {
        shown = files();
    }
    else
    {
        for (const auto& path : only)
            if (juce::File file; resolve (path, file))
                shown.add (file);
    }

    if (shown.isEmpty())
        return {};

    juce::String text;

    for (int i = 0; i < shown.size() && i < maxEntries; ++i)
        text << "- " << pathOf (shown[i]) << " (" << lineCountOf (shown[i]) << " lines)\n";

    if (shown.size() > maxEntries)
        text << "- ... and " << (shown.size() - maxEntries) << " more files (search_reference finds things in all of them)\n";

    return text;
}

juce::Result CodeReference::read (const juce::String& path, int from, int count, juce::String& text) const
{
    juce::File file;

    if (! resolve (path, file))
        return juce::Result::fail ("There's no file " + path + " in the user's code (reference/).");

    const auto lines = linesOf (file.loadFileAsString());
    const auto total = lines.size();

    text.clear();

    if (total == 0)
    {
        text = pathOf (file) + " is empty.";
        return juce::Result::ok();
    }

    if (from > total)
        return juce::Result::fail (pathOf (file) + " has " + juce::String (total) + " lines.");

    const auto first = juce::jmax (1, from);
    const auto length = juce::jlimit (1, maxReadLines, count <= 0 ? maxReadLines : count);
    const auto last = juce::jmin (total, first + length - 1);

    text << pathOf (file) << ", lines " << first << " to " << last << " of " << total;

    if (last < total)
        text << " (read on with from: " << (last + 1) << ")";

    text << ":\n";

    for (int i = first - 1; i < last; ++i)
        text << lines[i] << "\n";

    return juce::Result::ok();
}

juce::String CodeReference::search (const juce::String& wanted, int maxResults) const
{
    const auto needle = wanted.trim();

    if (needle.isEmpty())
        return "Give some text to look for.";

    juce::String found;
    const auto most = juce::jlimit (1, maxSearchResults, maxResults);
    int results = 0, more = 0;

    for (const auto& file : files())
    {
        // A file whose name it's in, then the lines it's on.
        if (file.getRelativePathFrom (folder).replaceCharacter ('\\', '/').containsIgnoreCase (needle))
        {
            if (results < most)
            {
                found << pathOf (file) << " (a file of " << lineCountOf (file) << " lines)\n";
                ++results;
            }
            else
            {
                ++more;
            }
        }

        const auto lines = linesOf (file.loadFileAsString());

        for (int i = 0; i < lines.size(); ++i)
        {
            if (! lines[i].containsIgnoreCase (needle))
                continue;

            if (results < most)
            {
                found << pathOf (file) << ":" << (i + 1) << ": " << lines[i].trim().substring (0, 200) << "\n";
                ++results;
            }
            else
            {
                ++more;
            }
        }
    }

    if (results == 0)
        return "\"" + needle + "\" isn't in the user's code.";

    if (more > 0)
        found << "... and " << more << " more: look for something more exact.\n";

    return found;
}

void CodeReference::moveInto (const juce::File& otherFolder)
{
    if (! folder.isDirectory() || otherFolder == folder)
        return;

    otherFolder.createDirectory();

    for (const auto& item : folder.findChildFiles (juce::File::findFilesAndDirectories, false))
    {
        const auto target = otherFolder.getChildFile (item.getFileName());
        target.deleteRecursively();

        if (! item.moveFileTo (target))
            item.isDirectory() ? item.copyDirectoryTo (target) : item.copyFileTo (target);
    }

    folder.deleteRecursively();
}

void CodeReference::clear()
{
    folder.deleteRecursively();
}
