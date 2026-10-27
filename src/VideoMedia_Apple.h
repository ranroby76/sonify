// D:\workspace\Fanan_web_hub\src\VideoMedia_Apple.h

#pragma once

#include "VideoMedia.h"

#include <atomic>
#include <functional>
#include <optional>
#include <vector>

#if JUCE_MAC

//==============================================================================
/**
    The Mac's side of VideoMedia (VideoMedia_Apple.mm): AVFoundation, which every Mac has, in
    place of VLC. So a Mac ships no video library at all, there is nothing to sign or notarize
    for it, and video works the moment Sonify starts.

    Everything round these - the video cache, the sound lined up with the picture, the strip of
    small pictures, the preparation thread - is VideoMedia.cpp's and is the same on every
    platform. Only the four things here, and the Player, are the platform's own:

        probe            what the picture is (its shape, how long, its frame rate, its sound)
        readSound        the sound as a WAV, as the file has it (VideoMedia lines it up after)
        readThumbnails   one small picture every so often, as images
        transcodePicture the picture written again as H.264, for a video whose picture can't go
                         into an MP4 as it is (Song > Export Video)

    The Player itself (the picture following the song) is defined in the .mm as well: AVPlayer
    seeks and plays at a rate exactly, so the drift-chasing and restarting VLC needs isn't
    wanted here.

    Each of these runs on the thread that calls it, and watches `cancelled` as it goes.
*/
namespace AppleVideo
{
    /** "" always: AVFoundation is part of the system. */
    juce::String problem();

    std::optional<VideoMedia::Info> probe (const juce::File& video, juce::String& error);

    /** The sound of the video into `wav` (float 32, as many channels as it has, at its own rate),
        from the start of its sound track. VideoMedia puts it where the picture starts afterwards
        (lineUpSound with Mp4::soundCorrection), exactly as it does with VLC's.
        reached: how long the sound turned out to be (seconds), when it is known. */
    juce::Result readSound (const juce::File& video, const juce::File& wav,
                            const std::function<void (double)>& progress,
                            std::atomic<bool>* cancelled, double* reached);

    /** One picture every `every` seconds from 0, each cellWidth x cellHeight, oldest first.
        At most VideoMedia's own limit: it stops when `into` has that many. */
    juce::Result readThumbnails (const juce::File& video, int cellWidth, int cellHeight, double every,
                                 int most, std::vector<juce::Image>& into,
                                 const std::function<void (double)>& progress,
                                 std::atomic<bool>* cancelled, double* reached);

    juce::Result transcodePicture (const juce::File& video, const juce::File& target,
                                   const std::function<void (double)>& progress, std::atomic<bool>* cancelled);
}

#endif
