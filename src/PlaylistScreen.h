// D:\workspace\Fanan_web_hub\src\PlaylistScreen.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PlaylistPlayer.h"

#include <memory>

//==============================================================================
/**
    The Playlist Player's window: OnStage's Media page, as OnStage draws and works it. On the
    left, the screen (where the song playing is shown), the progress bar with the song's
    length and time, and the glassy PLAY / PAUSE (green) and STOP (red) buttons; on the right,
    the playlist: Auto-Play, Set Default Folder, Add Tracks, Clear All, Save and Load, and a
    banner for each song.

    A banner's circle plays the song now (so does a double-click); a click selects it, a drag
    moves it in the list, and right-clicking gives its menu. Its + opens its controls: Volume,
    Pitch, Speed, Wait and Fade. Right-clicking a button or a slider says what it does (and
    which MIDI note works it), as in OnStage.

    PlaylistScreen::install() makes Playlist Players open this window (MainComponent calls it).
*/
namespace PlaylistScreen
{
    void install();

    /** The window's content, for the window (and for a look at it off screen in the tests). */
    std::unique_ptr<juce::Component> createPage (PlaylistPlayer& player);
}
