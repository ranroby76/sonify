// D:\workspace\Fanan_web_hub\src\SoundCheck.h

#pragma once

#include <juce_core/juce_core.h>

#include "stella_blob.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

//==============================================================================
/**
    analyze_sound: Fanan AI listens to the plugin being made. A copy of the playing plugin
    renders a test offline (the user hears nothing and the live plugin plays on undisturbed):
    notes for an instrument, a test signal for an effect or a widget. SoundMeasure then turns
    the audio into the report the AI reads.

    Runs on a worker thread: it takes a moment.
*/
namespace SoundCheck
{
    struct Request
    {
        bool instrument = true;
        std::vector<int> notes { 48 };               // played together (MIDI numbers)
        bool pitched = true;                         // false: the notes are triggers (drums), not pitches
        float velocity = 0.8f;                       // 0..1
        double hold = 1.2, tail = 1.5;               // seconds: the notes (or the signal), then what's left
        std::string signal = "music";                // effects and widgets: SoundMeasure::signalNames
        std::vector<std::pair<int, float>> values;   // the plugin's own parameter numbers, and their values
        std::vector<int> bypassed;                   // modules bypassed in the studio (graph.json's order)
        std::vector<stella::blob::Entry> library;    // a sampler's sample library, as the studio plays it
    };

    /** Renders the test and measures it: the report, or why it couldn't (isError). */
    juce::String run (const std::vector<std::uint8_t>& wasm, const Request& request, bool& isError);
}
