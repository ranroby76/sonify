// D:\workspace\Fanan_web_hub\src\FontLibrary.cpp

#include "FontLibrary.h"
#include "SonifyFonts.h"         // fonts/*.ttf (the folder beside src)
#include "StellaLookAndFeel.h"   // Theme::font, the standard font
#include "StellaRuntimeData.h"   // Inter, the programmed elements' own font

#include <map>

namespace
{
    /** Which font is which: its name, its group, its files' names in fonts and what it's like. */
    struct Entry
    {
        const char* name;
        const char* group;
        const char* character;
    };

    const Entry entries[]
    {
        { "Inter",            "Clean",     "neutral modern sans, very readable at small sizes" },
        { "Roboto",           "Clean",     "friendly modern sans" },
        { "Montserrat",       "Clean",     "geometric sans, wide and confident" },
        { "Oswald",           "Condensed", "tall condensed sans, strong headings" },
        { "Barlow Condensed", "Condensed", "narrow technical sans, fits long captions" },
        { "Bebas Neue",       "Condensed", "tall all-capitals display, posters and titles" },
        { "Rajdhani",         "Technical", "squared technical sans, hardware labels" },
        { "Exo 2",            "Technical", "futuristic sans, synths and electronics" },
        { "Orbitron",         "Technical", "wide geometric sci-fi capitals" },
        { "Michroma",         "Technical", "wide squared capitals, studio-hardware nameplates" },
        { "Audiowide",        "Technical", "rounded retro-tech display, 80s gear" },
        { "Russo One",        "Technical", "heavy blocky display, bold titles" },
        { "Share Tech Mono",  "Displays",  "monospaced technical, readouts and screens" },
        { "VT323",            "Displays",  "pixel terminal, LCD and old computer screens" },
        { "Roboto Mono",      "Displays",  "clean monospaced, values and numbers" },
        { "Press Start 2P",   "Displays",  "8-bit game pixels, chiptune" },
        { "Playfair Display", "Classic",   "elegant high-contrast serif, vintage and luxury" },
        { "Cinzel",           "Classic",   "engraved Roman capitals, classic and orchestral" },
        { "Righteous",        "Retro",     "rounded 70s display" },
        { "Monoton",          "Retro",     "neon tube outlines, disco and synthwave titles" },
        { "Pacifico",         "Retro",     "brush script, surf and vintage logos" },
    };

    juce::String keyOf (const juce::String& name)
    {
        return name.toLowerCase().removeCharacters (" -_");
    }

    /** A font file by its name in the fonts folder ("Bebas_Neue-Regular.ttf"). */
    const char* fileNamed (const juce::String& fileName, int& size)
    {
        for (int i = 0; i < SonifyFonts::namedResourceListSize; ++i)
        {
            if (fileName.equalsIgnoreCase (SonifyFonts::getNamedResourceOriginalFilename (SonifyFonts::namedResourceList[i])))
                return SonifyFonts::getNamedResource (SonifyFonts::namedResourceList[i], size);
        }

        size = 0;
        return nullptr;
    }

    std::vector<FontLibrary::Family> makeFamilies()
    {
        std::vector<FontLibrary::Family> list;

        for (const auto& e : entries)
        {
            FontLibrary::Family f;
            f.name = e.name;
            f.group = e.group;
            f.character = e.character;

            if (f.name == "Inter")
            {
                f.regular = StellaRuntimeData::inter_regular_ttf;
                f.regularSize = StellaRuntimeData::inter_regular_ttfSize;
                f.bold = StellaRuntimeData::inter_bold_ttf;
                f.boldSize = StellaRuntimeData::inter_bold_ttfSize;
            }
            else
            {
                const auto stem = f.name.replaceCharacter (' ', '_');
                f.regular = fileNamed (stem + "-Regular.ttf", f.regularSize);
                f.bold = fileNamed (stem + "-Bold.ttf", f.boldSize);
            }

            if (f.regular != nullptr && f.regularSize > 0)
                list.push_back (f);
        }

        return list;
    }

    juce::CriticalSection typefaceLock;

    juce::Typeface::Ptr typefaceFor (const FontLibrary::Family& f, bool bold)
    {
        static std::map<juce::String, juce::Typeface::Ptr> made;
        const bool hasBold = f.bold != nullptr && f.boldSize > 0;
        const auto key = f.name + (bold && hasBold ? "|bold" : "|regular");

        const juce::ScopedLock lock (typefaceLock);

        if (const auto found = made.find (key); found != made.end())
            return found->second;

        auto typeface = bold && hasBold ? juce::Typeface::createSystemTypefaceFor (f.bold, (size_t) f.boldSize)
                                        : juce::Typeface::createSystemTypefaceFor (f.regular, (size_t) f.regularSize);
        made[key] = typeface;
        return typeface;
    }
}

//==============================================================================
const std::vector<FontLibrary::Family>& FontLibrary::families()
{
    static const auto list = makeFamilies();
    return list;
}

juce::StringArray FontLibrary::groups()
{
    juce::StringArray names;

    for (const auto& f : families())
        names.addIfNotAlreadyThere (f.group);

    return names;
}

const FontLibrary::Family* FontLibrary::find (const juce::String& name)
{
    const auto wanted = keyOf (name);

    if (wanted.isEmpty())
        return nullptr;

    for (const auto& f : families())
        if (keyOf (f.name) == wanted)
            return &f;

    return nullptr;
}

juce::String FontLibrary::canonical (const juce::String& name)
{
    const auto* f = find (name);
    return f != nullptr ? f->name : juce::String();
}

juce::Font FontLibrary::font (const juce::String& family, float height, bool bold)
{
    const auto* f = find (family);

    if (f == nullptr)
        return Theme::font (height, bold);

    if (auto typeface = typefaceFor (*f, bold))
        return juce::Font (juce::FontOptions (typeface).withHeight (juce::jmax (1.0f, height)));

    return Theme::font (height, bold);
}

bool FontLibrary::file (const juce::String& family, bool bold, const void*& data, int& size)
{
    const auto* f = find (family);

    if (f == nullptr)
        return false;

    const bool hasBold = f->bold != nullptr && f->boldSize > 0;
    data = bold && hasBold ? f->bold : f->regular;
    size = bold && hasBold ? f->boldSize : f->regularSize;
    return data != nullptr && size > 0;
}

juce::String FontLibrary::describe()
{
    juce::StringArray lines;

    for (const auto& group : groups())
    {
        juce::StringArray names;

        for (const auto& f : families())
            if (f.group == group)
                names.add (f.name + " (" + f.character + ")");

        lines.add (group + ": " + names.joinIntoString ("; "));
    }

    return lines.joinIntoString ("\n");
}
