// D:\workspace\Fanan_web_hub\src\PanelParts.h

#pragma once

#include "GuiLayout.h"

#include <functional>

//==============================================================================
/**
    How lamps, selectors, keyboards and labels look on a plugin's panel: the canvas draws
    them with these, in Edit, in Play and in a made plugin's window. Each takes a "display"
    (the layout's settings, see GuiWidget::partKeys); without one it's drawn as it always was.

    Lamps (lampDisplays):
        dot        a round light with a glow (the standard)
        dome       a domed lens in a dark bezel: a console's LED
        jewel      a faceted jewel in a chrome bezel: a pilot light
        square     a flat square lens, as wide as its box: an output or clip LED
        tube       a vacuum tube whose filament glows with the level
        eye        a magic eye: a green ring whose dark wedge closes as the level rises
      "follow": true makes the light follow the level (a tube and an eye always do); otherwise
      it's on from its "threshold". "bezel": the bezel's colour. The lamp's colour is its light.

    Selectors (selectorDisplays):
        cells      a row of buttons (the standard)
        pill       a rounded track with a pill sliding to the chosen position
        keys       console key caps, the chosen one lit (stacked when the box is tall; "columns"
                   lays them out as a table of that many columns; "icons" draws waveforms and
                   filter shapes in place of their names: Sine, Saw, Square, Pulse, Triangle,
                   Noise, Random, Classic, Low Pass, High Pass, Band Pass, Notch, Off)
        lamps      the names, each with a small lamp: the chosen one lights
        vfd        a glowing display window showing the chosen name, arrows to step
        tiles      coloured tiles along a colour ramp, the chosen one raised
      Its colour is the chosen position's (the light, the pill, the window's glow);
      "background" the track's or window's.

    Keyboards (keyboardDisplays):
        classic    ivory naturals and black sharps (the standard)
        reverse    black naturals and ivory sharps, as on old harpsichords and some synths
        modern     flat rounded slate keys with gaps; the keys that are down glow
        graphite   dark naturals under light grey sharps (the sharps carry the contrast)
      Its colour: the keys that are down.

    Labels (labelDisplays):
        plain      words on the panel (the standard)
        readout    a framed window showing its parameter's value and unit ("param")
        vfd        a dark window with glowing words (vacuum fluorescent)
        lcd        a backlit window with segment characters
      "arrows" (left, right, both, up, down) puts small arrowheads beside its words;
      "background" the window's colour; its colour the words'.
*/
namespace PanelParts
{
    //==========================================================================
    const juce::StringArray& lampDisplays();
    const juce::StringArray& lampDisplayNames();
    juce::String lampDisplayOf (const GuiWidget& lamp);

    /** Whether it follows the level (a tube and an eye do, unless "follow" is false). */
    bool lampFollows (const GuiWidget& lamp);

    /** lit: 0 off .. 1 fully on (in between while it follows the level). */
    void paintLamp (juce::Graphics&, juce::Rectangle<float> area, const GuiWidget& lamp, float lit);

    //==========================================================================
    const juce::StringArray& selectorDisplays();
    const juce::StringArray& selectorDisplayNames();
    juce::String selectorDisplayOf (const GuiWidget& selector);

    /** Its positions, chosen: -1 none. accent: the chosen one's colour; ink: the names'. */
    void paintSelector (juce::Graphics&, juce::Rectangle<float> area, const GuiWidget& selector, const juce::StringArray& names,
                        int chosen, juce::Colour accent, juce::Colour ink);

    /** The position a click at `at` picks (a vfd's arrows step from the chosen one); -1: none. */
    int selectorPick (const GuiWidget& selector, juce::Rectangle<float> area, int count, int chosen, juce::Point<float> at);

    //==========================================================================
    const juce::StringArray& keyboardDisplays();
    const juce::StringArray& keyboardDisplayNames();
    juce::String keyboardDisplayOf (const GuiWidget& keyboard);

    /** Its keys at rest (what never moves). */
    void paintKeyboard (juce::Graphics&, const GuiWidget& keyboard);

    /** The keys that are down, over the keys at rest (the black keys stay on top). */
    void paintKeysDown (juce::Graphics&, const GuiWidget& keyboard, const std::function<bool (int)>& isDown, juce::Colour accent);

    //==========================================================================
    const juce::StringArray& labelDisplays();
    const juce::StringArray& labelDisplayNames();
    juce::String labelDisplayOf (const GuiWidget& label);

    /** Whether it shows a live value (a readout of its parameter): it moves. */
    bool labelMoves (const GuiWidget& label);

    /** Its words (a readout's: its value) in its box. ink: the words' colour; size 0: the standard. */
    void paintLabel (juce::Graphics&, const GuiWidget& label, const juce::String& text, juce::Colour ink, float size);

    /** The displays one kind takes, by id and by name (empty for kinds without them). */
    const juce::StringArray& displaysFor (GuiWidget::Type type);
    const juce::StringArray& displayNamesFor (GuiWidget::Type type);
}
