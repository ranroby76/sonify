// D:\workspace\Fanan_web_hub\src\CodeReference.h

#pragma once

#include <juce_core/juce_core.h>

//==============================================================================
/**
    The user's own code, brought to Fanan AI: files, a zip or a whole folder (a JUCE project, a
    Faust or Max gen~ patch, a Python sketch...), kept in a folder of its own beside what's
    being built (a plugin project's reference folder, or a MIDI FX's), read only.

    Fanan AI isn't sent the code with every step: it sees the list (paths and line counts) and
    reads what it needs, a part at a time (read), or looks for something across it all
    (search). So a big codebase fits, and costs only what's read.

    Only text that can be code or notes is kept (by extension); build folders, version control
    and compiled files are left out. Limits: maxFiles files, maxFileBytes each, maxTotalBytes
    in all.
*/
class CodeReference final
{
public:
    explicit CodeReference (juce::File folder);

    const juce::File& getFolder() const noexcept    { return folder; }

    /** Whether something can be brought: a code or text file, a zip, or a folder. */
    static bool canTake (const juce::File& file);

    /** Whether a file's extension is code or text that's kept. */
    static bool isCodeFile (const juce::File& file);
    static bool isCodeName (const juce::String& fileName);

    /** The extensions kept, for a file chooser ("*.cpp;*.h;..."), with "*.zip". */
    static juce::String filePatterns();

    struct Added
    {
        juce::StringArray paths;      // as Fanan AI names them: "reference/..."
        juce::StringArray problems;   // what couldn't come, and why (for the user)
        int skipped = 0;              // files left out on purpose (not code: pictures, builds, libraries)
    };

    /** Brings files, zips and folders in (a zip or folder keeps its own folder inside). A file
        of the same name is replaced. */
    Added add (const juce::Array<juce::File>& sources);

    bool isEmpty() const;

    /** The files, as Fanan AI sees them, a line each ("- reference/x.cpp (120 lines)"), at most
        maxEntries, then how many more; only: just these paths (as add gave them). Empty when
        there are none. */
    juce::String listing (int maxEntries, const juce::StringArray& only = {}) const;

    /** A part of a file: lines from (1-based), at most count (0: up to maxReadLines). path is
        "reference/..." (or without it). The text comes with a line saying what part it is. */
    juce::Result read (const juce::String& path, int from, int count, juce::String& text) const;

    /** The files whose path, and the lines that, contain text (any case): "reference/x.cpp (a file
        of 120 lines)", "reference/x.cpp:12: the line", at most maxResults in all. */
    juce::String search (const juce::String& text, int maxResults) const;

    /** Everything in it moved into another reference folder (a project that now exists). */
    void moveInto (const juce::File& otherFolder);

    /** Everything in it gone. */
    void clear();

    static constexpr const char* prefix = "reference/";
    static constexpr int maxFiles = 600, maxReadLines = 400, maxSearchResults = 80;
    static constexpr juce::int64 maxFileBytes = 512 * 1024, maxTotalBytes = 12 * 1024 * 1024;

private:
    juce::Array<juce::File> files() const;
    juce::String pathOf (const juce::File& file) const;
    bool resolve (const juce::String& path, juce::File& file) const;
    juce::int64 totalBytes() const;

    juce::File folder;
};
