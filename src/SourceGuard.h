// D:\workspace\Fanan_web_hub\src\SourceGuard.h

#pragma once

#include <juce_core/juce_core.h>

#include <cctype>
#include <cstring>
#include <string>

//==============================================================================
/**
    A check on code Sonify is about to compile that Fanan AI (or anyone) wrote: the Plugins
    Creator's modules and elements, and the Device Studio's devices.

    The compiler would read any file a source names, and its messages go back to the AI (and
    so to the server). So a source may only bring in files from its own project (quoted
    includes, relative, staying inside it) and the compiler's own headers (<...> with a plain
    name). Turned away: an absolute path, a drive or ".." that leaves the project, an include
    whose name comes from a macro, #embed outside the project, #pragma ... dependency,
    _Pragma, and inline assembly (whose .incbin reads files too).

    It reads the code as the compiler does: lines joined at a backslash, comments as spaces,
    string literals (raw ones too) skipped, %: as #.
*/
namespace SourceGuard
{
    namespace detail
    {
        /** Whether a file a source names stays where it may. */
        inline juce::String checkPath (juce::String path, bool quoted, const juce::File& folderOfFile, const juce::File& root)
        {
            path = path.replaceCharacter ('\\', '/');

            if (path.trim().isEmpty())
                return "an empty file name";

            if (path.containsChar (':') || path.startsWithChar ('/') || path.startsWithChar ('~'))
                return "\"" + path + "\" (only files of the project itself can be included)";

            const auto parts = juce::StringArray::fromTokens (path, "/", "");

            if (! parts.contains (".."))
                return {};

            if (! quoted)
                return "<" + path + "> (\"..\" isn't allowed in a <...> include)";

            const auto target = folderOfFile.getChildFile (path);

            if (root == juce::File() || ! (target == root || target.isAChildOf (root)))
                return "\"" + path + "\" (it leaves the project)";

            return {};
        }
    }

    /** "" when the source is fine; otherwise what's wrong with it, in one line. */
    inline juce::String problemIn (const juce::String& sourceText, const juce::String& fileName,
                                   const juce::File& folderOfFile, const juce::File& root)
    {
        using namespace detail;

        // Lines joined where a backslash ends them, as the compiler's first step does. Read as
        // bytes: what matters here is plain ASCII, and UTF-8's other bytes count as letters.
        const auto joined = sourceText.replace ("\\\r\n", {}).replace ("\\\n", {}).toStdString();
        const auto n = joined.size();
        const auto at = [&] (size_t i) -> unsigned char { return i < n ? (unsigned char) joined[i] : 0; };
        const auto wordChar = [] (unsigned char ch) { return ch == '_' || ch >= 0x80 || std::isalnum (ch) != 0; };

        // Comments become spaces; literals stay out of the words looked at.
        std::string clean;
        clean.reserve (n + 16);
        juce::StringArray words;    // identifiers outside literals and comments, in order
        std::string word;

        const auto endWord = [&]
        {
            if (! word.empty() && ! std::isdigit ((unsigned char) word[0]))
                words.add (juce::String (word));

            word.clear();
        };

        size_t i = 0;

        while (i < n)
        {
            const auto ch = at (i), next = at (i + 1);

            if (ch == '/' && next == '/')
            {
                endWord();
                while (i < n && at (i) != '\n') ++i;
                clean += ' ';
                continue;
            }

            if (ch == '/' && next == '*')
            {
                endWord();
                i += 2;
                while (i < n && ! (at (i) == '*' && at (i + 1) == '/')) ++i;
                i = juce::jmin (n, i + 2);
                clean += ' ';
                continue;
            }

            // A digit separator (1'000, inside a number) isn't a character literal.
            const bool inNumber = ! word.empty() && std::isdigit ((unsigned char) word[0]) != 0;

            if (ch == '"' || (ch == '\'' && ! inNumber))
            {
                // A raw string: R"delim( ... )delim", after R, u8R, uR, UR or LR.
                const bool raw = ch == '"' && (word == "R" || word == "u8R" || word == "uR" || word == "UR" || word == "LR");
                word.clear();   // a literal's prefix isn't a word
                clean += (char) ch;
                ++i;

                if (raw)
                {
                    std::string delimiter;
                    while (i < n && at (i) != '(' && delimiter.size() < 17) delimiter += (char) at (i++);
                    const auto close = ")" + delimiter + "\"";
                    const auto found = joined.find (close, i);
                    const auto stop = found == std::string::npos ? n : found + close.size();

                    for (; i < stop; ++i)
                        if (at (i) == '\n')
                            clean += '\n';   // its lines stay lines

                    clean += '"';
                    continue;
                }

                while (i < n && at (i) != ch && at (i) != '\n')
                {
                    if (at (i) == '\\' && i + 1 < n && at (i + 1) != '\n') clean += (char) at (i++);
                    clean += (char) at (i++);
                }

                if (i < n && at (i) == ch) clean += (char) at (i++);
                continue;
            }

            if (wordChar (ch))
                word += (char) ch;
            else
                endWord();

            clean += (char) ch;
            ++i;
        }

        endWord();

        // Words that reach outside the code itself.
        for (const auto& w : words)
        {
            if (w == "asm" || w == "__asm" || w == "__asm__")
                return fileName + ": inline assembly isn't allowed.";

            if (w == "_Pragma" || w == "__pragma")
                return fileName + ": _Pragma isn't allowed (use #pragma).";
        }

        // The directives, a line at a time.
        int lineNumber = 0;

        for (auto line : juce::StringArray::fromLines (juce::String::fromUTF8 (clean.data(), (int) clean.size())))
        {
            ++lineNumber;
            line = line.trimStart();
            const auto where = fileName + " line " + juce::String (lineNumber) + ": ";

            // __has_include and its kind: probing for a file, in #if or a macro.
            for (const auto* probe : { "__has_include_next", "__has_include", "__has_embed" })
            {
                for (int found = line.indexOf (probe); found >= 0; found = line.indexOf (found + 1, probe))
                {
                    const auto after = line.substring (found + (int) std::strlen (probe)).trimStart();

                    if (! after.startsWithChar ('('))
                        continue;

                    const auto inside = after.substring (1).trimStart();
                    const bool quoted = inside.startsWithChar ('"');

                    if (! quoted && ! inside.startsWithChar ('<'))
                        return where + juce::String (probe) + " needs a file name written out, not a macro.";

                    const auto name = inside.substring (1).upToFirstOccurrenceOf (quoted ? "\"" : ">", false, false);

                    if (const auto problem = checkPath (name, quoted, folderOfFile, root); problem.isNotEmpty())
                        return where + "it can't look for " + problem + ".";
                }
            }

            if (line.startsWith ("%:"))
                line = "#" + line.substring (2);

            if (! line.startsWithChar ('#'))
                continue;

            auto rest = line.substring (1).trimStart();
            const auto name = rest.initialSectionContainingOnly ("abcdefghijklmnopqrstuvwxyz_ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
            rest = rest.substring (name.length()).trimStart();

            if (name == "pragma" && rest.contains ("dependency"))
                return where + "#pragma ... dependency isn't allowed.";

            if (name != "include" && name != "include_next" && name != "import" && name != "embed")
                continue;

            const bool quoted = rest.startsWithChar ('"');

            if (! quoted && ! rest.startsWithChar ('<'))
                return where + "#" + name + " needs a file name written out (\"name.h\" or <name>), not a macro.";

            const auto file = rest.substring (1).upToFirstOccurrenceOf (quoted ? "\"" : ">", false, false);

            if (name == "embed" && ! quoted)
                return where + "#embed can only take a file of the project (in quotes).";

            if (const auto problem = checkPath (file, quoted, folderOfFile, root); problem.isNotEmpty())
                return where + "#" + name + " " + problem + ".";
        }

        return {};
    }

    /** Every source file in a folder (not below it): "" when all are fine. */
    inline juce::String problemInFolder (const juce::File& folder, const juce::File& root)
    {
        if (! folder.isDirectory())
            return {};

        for (const auto& file : folder.findChildFiles (juce::File::findFiles, false, "*.cpp;*.cc;*.cxx;*.c;*.h;*.hpp;*.hh;*.hxx;*.inc;*.ipp;*.inl"))
            if (const auto problem = problemIn (file.loadFileAsString(), folder.getFileName() + "/" + file.getFileName(), folder, root);
                problem.isNotEmpty())
                return problem;

        return {};
    }
}
