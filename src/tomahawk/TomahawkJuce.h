// D:\workspace\Fanan_web_hub\src\tomahawk\TomahawkJuce.h
//
// What Grex's files had from <JuceHeader.h> and <BinaryData.h>, for the Tomahawk sampler's copy
// of them: the JUCE modules they use, JUCE's names without juce:: (as the generated JuceHeader.h
// gives them), and the instrument pictures (TomahawkImages, built from "assets/instrumet
// images") under Grex's name for them, BinaryData. All of it inside namespace Tomahawk, so none
// of it reaches the rest of Sonify.

#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include <TomahawkImages.h>

namespace Tomahawk
{
    using namespace juce;
    namespace BinaryData = ::TomahawkImages;
}
