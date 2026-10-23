// D:\workspace\Fanan_web_hub\src\VideoSound.h

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include "VideoMedia.h"

#include <functional>
#include <memory>

//==============================================================================
/**
    A video's sound, read straight from its file by FFmpeg for the Live Show's player and its
    listening (tempo and key). Only the sound is decoded (the picture is passed over, most kinds
    of file not even read for it), so a video comes onto a deck about as fast as an audio file
    does: no pass of VLC's over the whole video first, no WAV written and read again. VLC still
    shows its picture.

    Lined up with the picture: sample 0 is the video's time 0, where its picture is from. A sound
    that starts later has silence in front of it; what an encoder put before the start (its
    priming, which the file says to leave out) is left out. A video without sound reads as silence
    as long as it is.

    MP4, MOV, M4V, 3GP, MKV, WebM, AVI, FLV and WMV are read this way. MPEG transport and program
    streams (TS, MTS, M2TS, MPG, VOB) and Ogg keep times VLC reads differently: those, and any video
    FFmpeg can't open, are prepared by VLC as before (VideoMedia::Preparation), as is every video in a
    Sonify built without FFmpeg.

    Any thread; a reader is used by one thread at a time.
*/
namespace VideoSound
{
    /** Whether this Sonify reads videos' sound itself (it has FFmpeg, and FFmpeg's DLLs load). */
    bool available();

    struct Opened
    {
        std::unique_ptr<juce::AudioFormatReader> reader;   // floats, one or two channels, the sound's own rate
        VideoMedia::Info info;                              // the picture's size and rate, how long, its sound
        juce::String error;                                 // why there's no reader ("": VLC prepares this one)
    };

    /** Opens a video's sound (reading only its start). No reader with no error: VLC's preparation
        reads this one instead; with an error: it can't be read at all. */
    Opened open (const juce::File& video);

    /** An audio file of a kind JUCE doesn't read (M4A, AAC, Opus, WMA, ALAC, AC-3...), any file
        FFmpeg reads a sound from, written whole into a WAV: 32-bit float, at its own rate, one or
        two channels. Media import (MainImport.cpp) brings such files into a song this way. False:
        it couldn't (error says why, "Cancelled" when keepGoing said to stop). Any thread. */
    bool soundToWav (const juce::File& source, const juce::File& wav, juce::String& error,
                     const std::function<bool()>& keepGoing = nullptr);
}
