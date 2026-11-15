// D:\workspace\Fanan_web_hub\src\IconLibrary.cpp

#include "IconLibrary.h"

#include "SonifyIcons.h"

#include <algorithm>
#include <string>
#include <unordered_map>

namespace
{
    /** The library, read once: each icon's name, category and tags, and its outline as written
        ("S<path>" a stroke 2 wide with round ends, "F<path>" a fill, joined by "|"). */
    struct Library
    {
        std::vector<IconLibrary::Icon> icons;
        std::vector<juce::String> data;
        std::unordered_map<std::string, int> byName;
        juce::StringArray categories;

        juce::CriticalSection lock;
        std::unordered_map<int, juce::Path> paths;

        Library()
        {
            juce::MemoryInputStream packed (SonifyIcons::icon_library_txt_gz, (size_t) SonifyIcons::icon_library_txt_gzSize, false);
            juce::GZIPDecompressorInputStream unpacked (&packed, false, juce::GZIPDecompressorInputStream::gzipFormat);
            const auto text = unpacked.readEntireStreamAsString();

            juce::StringArray lines;
            lines.addLines (text);
            icons.reserve ((size_t) lines.size());
            data.reserve ((size_t) lines.size());

            for (const auto& line : lines)
            {
                const auto a = line.indexOfChar ('\t');
                const auto b = a < 0 ? -1 : line.indexOfChar (a + 1, '\t');
                const auto c = b < 0 ? -1 : line.indexOfChar (b + 1, '\t');

                if (c < 0)
                    continue;

                IconLibrary::Icon icon;
                icon.name = line.substring (0, a);
                icon.category = line.substring (a + 1, b);
                icon.tags = line.substring (b + 1, c);

                byName[icon.name.toStdString()] = (int) icons.size();

                if (! categories.contains (icon.category))
                    categories.add (icon.category);

                icons.push_back (icon);
                data.push_back (line.substring (c + 1));
            }
        }

        juce::Path path (int index)
        {
            if (! juce::isPositiveAndBelow (index, (int) icons.size()))
                return {};

            const juce::ScopedLock sl (lock);

            if (const auto found = paths.find (index); found != paths.end())
                return found->second;

            juce::Path outline;
            const juce::PathStrokeType stroke (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

            for (const auto& part : juce::StringArray::fromTokens (data[(size_t) index], "|", {}))
            {
                if (part.length() < 2)
                    continue;

                const auto shape = juce::Drawable::parseSVGPath (part.substring (1));

                if (part[0] == 'S')
                {
                    juce::Path solid;
                    stroke.createStrokedPath (solid, shape);
                    outline.addPath (solid);
                }
                else
                {
                    outline.addPath (shape);
                }
            }

            paths[index] = outline;
            return outline;
        }
    };

    Library& library()
    {
        static Library lib;
        return lib;
    }

    bool isNameCharacter (juce::juce_wchar c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
    }

    /** Where the icons are in some text: [start, end) of each "{name}" that's an icon, its index. */
    struct Found { int start, end, icon; };

    std::vector<Found> iconsIn (const juce::String& text)
    {
        std::vector<Found> found;

        for (int i = text.indexOfChar ('{'); i >= 0; i = text.indexOfChar (i + 1, '{'))
        {
            const auto close = text.indexOfChar (i + 1, '}');

            if (close < 0)
                break;

            if (close - i < 2 || close - i > 60)
                continue;

            const auto name = text.substring (i + 1, close).trim();
            bool plain = name.isNotEmpty();

            for (auto p = name.getCharPointer(); plain && ! p.isEmpty(); ++p)
                plain = isNameCharacter (*p);

            if (! plain)
                continue;

            if (const auto index = IconLibrary::indexOf (name); index >= 0)
            {
                found.push_back ({ i, close + 1, index });
                i = close;
            }
        }

        return found;
    }
}

//==============================================================================
const std::vector<IconLibrary::Icon>& IconLibrary::all()
{
    return library().icons;
}

juce::StringArray IconLibrary::categories()
{
    return library().categories;
}

int IconLibrary::indexOf (const juce::String& name)
{
    auto& lib = library();
    const auto found = lib.byName.find (name.trim().toLowerCase().toStdString());
    return found != lib.byName.end() ? found->second : -1;
}

std::vector<int> IconLibrary::search (const juce::String& words, const juce::String& category, int limit)
{
    auto& lib = library();
    const auto wanted = juce::StringArray::fromTokens (words.toLowerCase(), " ,", {});
    std::vector<std::pair<int, int>> scored;   // score, index

    for (int i = 0; i < (int) lib.icons.size(); ++i)
    {
        const auto& icon = lib.icons[(size_t) i];

        if (category.isNotEmpty() && icon.category != category)
            continue;

        int score = 0;
        bool all = true;

        for (const auto& word : wanted)
        {
            if (icon.name == word)                                 score += 100;
            else if (icon.name.startsWith (word))                  score += 40;
            else if (icon.name.contains (word))                    score += 20;
            else if ((" " + icon.tags + " ").contains (" " + word)) score += 8;
            else if (icon.tags.contains (word))                     score += 3;
            else                                                    all = false;
        }

        if (! all)
            continue;

        // Fanan's audio icons first among equals, then the outlined, then the solid.
        if (icon.category == "Audio")
            score += 2;

        scored.push_back ({ score, i });
    }

    std::stable_sort (scored.begin(), scored.end(), [] (const auto& a, const auto& b) { return a.first > b.first; });

    std::vector<int> result;

    for (const auto& [score, index] : scored)
    {
        if ((int) result.size() >= limit)
            break;

        result.push_back (index);
    }

    return result;
}

juce::Path IconLibrary::pathOf (int index)
{
    return library().path (index);
}

juce::Path IconLibrary::pathOf (const juce::String& name)
{
    return pathOf (indexOf (name));
}

juce::String IconLibrary::token (const juce::String& name)
{
    return "{" + name + "}";
}

bool IconLibrary::hasIcons (const juce::String& text)
{
    return text.containsChar ('{') && ! iconsIn (text).empty();
}

//==============================================================================
juce::Path IconText::line (const juce::String& text, const juce::Font& font, float& width)
{
    juce::Path result;
    const auto found = iconsIn (text);

    // Among words, an icon is as tall as their capitals; alone (a key's, a tab's, an icon
    // element's), bigger: it reads as a symbol.
    juce::String others;
    int from = 0;

    for (const auto& f : found)
    {
        others << text.substring (from, f.start);
        from = f.end;
    }

    others << text.substring (from);
    const bool onlyIcons = ! found.empty() && others.trim().isEmpty();
    const float size = font.getHeight() * (onlyIcons ? IconText::aloneScale : IconText::besideWords);
    const float top = (font.getDescent() - font.getAscent()) * 0.5f - size * 0.5f;
    const float margin = size * 0.12f;
    float x = 0.0f;
    int at = 0;

    auto addText = [&] (const juce::String& run)
    {
        if (run.isEmpty())
            return;

        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (font, run, x, 0.0f);
        juce::Path words;
        glyphs.createPath (words);
        result.addPath (words);
        x += juce::GlyphArrangement::getStringWidth (font, run);
    };

    for (size_t k = 0; k < found.size(); ++k)
    {
        const auto& f = found[k];
        addText (text.substring (at, f.start));

        const bool before = f.start > 0, after = f.end < text.length();
        x += before ? margin : 0.0f;
        result.addPath (IconLibrary::pathOf (f.icon), juce::AffineTransform::scale (size / 24.0f).translated (x, top));
        x += size + (after ? margin : 0.0f);
        at = f.end;
    }

    addText (text.substring (at));
    width = x;
    return result;
}

juce::Path IconText::fitted (const juce::String& text, const juce::Font& font, juce::Rectangle<float> box, juce::Justification justification,
                             float minScale, float* baseline)
{
    float width = 0.0f;
    auto path = line (text, font, width);

    if (path.isEmpty() || box.isEmpty())
        return {};

    // Its height: the words' (or a lone icon's, which may be taller).
    const auto drawn = path.getBounds();
    const float top = juce::jmin (-font.getAscent(), drawn.getY()), bottom = juce::jmax (font.getDescent(), drawn.getBottom());

    // Squeezed across first (as far as minScale), then smaller all over.
    float across = 1.0f, all = 1.0f;

    if (width > box.getWidth() && width > 0.0f)
    {
        across = juce::jmax (juce::jlimit (0.1f, 1.0f, minScale), box.getWidth() / width);

        if (width * across > box.getWidth())
            all = box.getWidth() / (width * across);
    }

    if ((bottom - top) * all > box.getHeight() && bottom > top)
        all = box.getHeight() / (bottom - top);

    const float shownWidth = width * across * all;
    const float ascent = -top * all, descent = bottom * all;

    float x = box.getX();

    if (justification.testFlags (juce::Justification::right))
        x = box.getRight() - shownWidth;
    else if (justification.testFlags (juce::Justification::horizontallyCentred))
        x = box.getCentreX() - shownWidth * 0.5f;

    float y = box.getY() + (box.getHeight() - (ascent + descent)) * 0.5f + ascent;

    if (justification.testFlags (juce::Justification::top))
        y = box.getY() + ascent;
    else if (justification.testFlags (juce::Justification::bottom))
        y = box.getBottom() - descent;

    if (baseline != nullptr)
        *baseline = y;

    path.applyTransform (juce::AffineTransform::scale (across * all, all).translated (x, y));
    return path;
}
