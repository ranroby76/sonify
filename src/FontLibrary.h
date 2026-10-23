// D:\workspace\Fanan_web_hub\src\FontLibrary.h

#pragma once

#include <juce_graphics/juce_graphics.h>

#include <vector>

//==============================================================================
/**
    The fonts a plugin's panel can use for its text (a label, a group's title, captions, a
    selector's buttons, the page tabs, the preset manager, programmed elements' text): built
    into Sonify (the fonts folder beside src, SIL Open Font License), so a panel looks the same on every
    computer. An element's "font" names one; none (or one that isn't here) is the standard
    font.

    Clean: Inter, Roboto, Montserrat. Condensed: Oswald, Barlow Condensed, Bebas Neue.
    Technical: Rajdhani, Exo 2, Orbitron, Michroma, Audiowide, Russo One. Displays: Share Tech
    Mono, VT323, Roboto Mono, Press Start 2P. Classic: Playfair Display, Cinzel. Retro:
    Righteous, Monoton, Pacifico.
*/
namespace FontLibrary
{
    struct Family
    {
        juce::String name, group, character;    // "Orbitron", "Technical", "wide sci-fi capitals"
        const char* regular = nullptr;           // its font files (bold: none, the regular one stands in)
        int regularSize = 0;
        const char* bold = nullptr;
        int boldSize = 0;
    };

    /** Every font, group by group. */
    const std::vector<Family>& families();

    /** The groups, in order. */
    juce::StringArray groups();

    /** A font by its name, any case, spaces ignored ("bebasneue" finds Bebas Neue); nullptr: none. */
    const Family* find (const juce::String& name);

    /** Its name as the list has it; empty if there's no such font. */
    juce::String canonical (const juce::String& name);

    /** The font to draw with: the family's (bold if it has a bold), or the standard one. */
    juce::Font font (const juce::String& family, float height, bool bold = false);

    /** Its font file, for text drawn by programmed elements (bold: the regular one if it has
        none). False if there's no such font. */
    bool file (const juce::String& family, bool bold, const void*& data, int& size);

    /** For the AI: the fonts by group, with what each is like. */
    juce::String describe();
}
