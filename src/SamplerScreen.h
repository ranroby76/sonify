// D:\workspace\Fanan_web_hub\src\SamplerScreen.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "SonifySampler.h"

//==============================================================================
/**
    The Sonify Sampler's window (SonifySampler.h), Sampler Arena's SFZ Player in Sonify's colours:
    the instrument (Load SFZ, Latest: the one sampled last, Clear, Save SFZ, Save As), presets
    (Load Preset, Save Preset: .sfzp), and the sampler's settings as sliders in groups: the amp
    envelope, the filter and its envelope, how it plays (Poly or Mono and its mode, octave,
    glide), the volume, the pitch envelope, pitch bend and tracking, and the three LFOs. A
    slider's double-click puts it back where it starts.

    SamplerScreen::install() makes Sonify Samplers open this window (MainSampling.cpp calls it).
*/
namespace SamplerScreen
{
    void install();

    /** The window's page, on its own (the tests draw it). */
    std::unique_ptr<juce::Component> createPage (SonifySampler& sampler);
}
