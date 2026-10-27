// D:\workspace\Fanan_web_hub\src\AiMix.h

#pragma once

#include "AiAgent.h"
#include "AiTools.h"
#include "MixAnalysis.h"

#include <functional>

//==============================================================================
/**
    Fanan AI's mixing and mastering (AI Mix on the Analog Board, AI Master in the Mastering
    window): an AiAgent toolbox of its own. Sonify does the listening (measure_mix renders the
    song and measures it: MixAnalysis), Fanan AI reads the numbers and moves the board's own
    knobs and the Mastering section's (set_board), then measures again, two or three times.
    Its targets are a genre's profile (the mix-genres skill) or a reference track's numbers.

    Its guide is sonify_mix_guide.md (with the mixing skills' list): it works for a suggestion,
    never "the best", and says what it did and why as choices. In Master only it may touch the
    Mastering section alone.
*/
class AiMix final : public AiAgent::Toolbox
{
public:
    struct Hooks
    {
        /** Renders the song and measures it; done (on the message thread) with the report, or
            { error }. Called with the next one only after done. */
        std::function<void (std::function<void (const juce::var&)> done)> measure;

        /** Stops a measuring under way (its done comes, with { error: "Cancelled" }). */
        std::function<void()> cancelMeasure;

        /** Master only: the Mastering section alone may change. */
        std::function<bool()> masterOnly;
    };

    AiMix (AiTools& tools, Hooks hooks);

    juce::String guide() override;
    juce::var definitions() override;
    void run (const juce::String& name, const juce::var& input, std::function<void (const juce::var&)> done) override;
    juce::String describe (const juce::String& name, const juce::var& input, const juce::var& result) override;
    bool looksOnly (const juce::String& name) override;
    void cancel() override;

    /** Its guide alone (sonify_mix_guide.md), for the request's own text. */
    static juce::String mixGuide();

private:
    AiTools& tools;
    Hooks hooks;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AiMix)
};
