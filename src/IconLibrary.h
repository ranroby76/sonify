// D:\workspace\Fanan_web_hub\src\IconLibrary.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

//==============================================================================
/**
    The icon library: some 13,000 icons and symbols to show in place of words (or among them)
    anywhere a panel has text: captions, labels, a selector's positions and keys, tabs, preset
    buttons. Fanan's own audio set (waves, filters, envelopes, arpeggios, routing, effects,
    note values...), Tabler's (MIT) and the Pictogrammers' Material Design Icons (Apache 2.0),
    free in any plugin with no credit in it (their licences: icons_license.txt, with the app).

    In text, an icon is its name in braces: "{wave-sine}", "{filter-lowpass} Cutoff". A name
    that isn't an icon stays as it's written. Icons take the text's colour, size, italic and
    its text effects, like letters.

    They're built in (icon_library.txt.gz, SonifyIcons): read the first time one's needed.
*/
namespace IconLibrary
{
    struct Icon
    {
        juce::String name, category, tags;
    };

    /** Every icon, in the library's order (Fanan's audio set first). */
    const std::vector<Icon>& all();

    /** The categories, in order (Audio first). */
    juce::StringArray categories();

    /** The icon called this (its index); -1: none. */
    int indexOf (const juce::String& name);

    /** Icons whose names or tags have every word (in a category, or all of them: empty), best
        matches first, at most limit. */
    std::vector<int> search (const juce::String& words, const juce::String& category, int limit = 2000);

    /** Its outline in a 24 x 24 box, filled (its strokes made solid). Empty if there's no such icon. */
    juce::Path pathOf (int index);
    juce::Path pathOf (const juce::String& name);

    /** An icon as it's written in text. */
    juce::String token (const juce::String& name);

    /** Whether some text has an icon in it. */
    bool hasIcons (const juce::String& text);
}

//==============================================================================
/** Text with icons in it as one outline, laid out as JUCE lays out words. */
namespace IconText
{
    /** An icon's size, of the font's height: among words (as tall as their capitals), and
        alone (a key's, a tab's, an icon element's: it reads as a symbol). */
    constexpr float besideWords = 0.82f, aloneScale = 1.3f;

    /** On one line from x 0, its baseline at y 0: and how wide it is. */
    juce::Path line (const juce::String& text, const juce::Font& font, float& width);

    /** In a box, as addFittedText puts one line: squeezed down to minScale (then smaller) to
        fit, placed by the justification. Its baseline (in the box's coordinates) out. */
    juce::Path fitted (const juce::String& text, const juce::Font& font, juce::Rectangle<float> box, juce::Justification justification,
                       float minScale, float* baseline = nullptr);
}
