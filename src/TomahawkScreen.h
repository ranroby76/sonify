// D:\workspace\Fanan_web_hub\src\TomahawkScreen.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "TomahawkSampler.h"

//==============================================================================
/**
    The Tomahawk's window (TomahawkSampler.h), its only one: Grex's instrument editor. The
    header: the sound, SAVE PRESET (its settings become its own), and LOCATE LIBRARY and GET THE
    LIBRARY at the right. Its tabs: SOUND ENGINE (PACK, CATEGORY and INSTRUMENT columns, eight
    to a page with a button per page, the pack's picture, and Grex's LOW VELOCITY RESPONSE and
    PITCH BEND RANGE under the columns), SYNTHESIS (a double-click on GAIN sets the sound's base
    gain, on OCTAVE its base octave, both saved in its preset), MODULATION and EFFECTS (every
    effect a button in one row, its settings under it).
    Grex's keyboard along the bottom. Shown at 80 % of Grex's size, resizable.

    TomahawkScreen::install() makes Tomahawks open this window (MainSampling.cpp calls it).
*/
namespace TomahawkScreen
{
    void install();
}
