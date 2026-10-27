// D:\workspace\Fanan_web_hub\src\AiMix.cpp

#include "AiMix.h"
#include "Skills.h"

#include "SonifyAiData.h"

namespace
{
    juce::var failure (const juce::String& text)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty ("error", text);
        return juce::var (object);
    }
}

AiMix::AiMix (AiTools& t, Hooks h) : tools (t), hooks (std::move (h)) {}

juce::String AiMix::mixGuide()
{
    static const juce::String guide = []
    {
        auto text = juce::String::fromUTF8 (SonifyAiData::sonify_mix_guide_md, SonifyAiData::sonify_mix_guide_mdSize);

        // The first line names the file's place in the project, for people: the AI doesn't need it.
        if (text.startsWith ("<!--"))
            text = text.fromFirstOccurrenceOf ("-->", false, false).trimStart();

        return text;
    }();

    return guide;
}

juce::String AiMix::guide()
{
    // The same with every request (cached): the guide, then the mixing skills' list.
    static const juce::String full = [] { return mixGuide() + "\n\n" + Skills::index (Skills::For::mixing); }();
    return full;
}

juce::var AiMix::definitions()
{
    static const juce::var list = []
    {
        juce::Array<juce::var> definitionList;

        definitionList.add (juce::JSON::parse (
            "{\"name\":\"measure_mix\",\"description\":\"Renders the whole song as it's mixed now (faster than real time; the user hears silence meanwhile) "
            "and measures it: each Analog Board strip as it leaves its fader (stems, with track_id), Main's mix before the Mastering section (mix) and "
            "after it (master, while it's in), and the reference track when there is one: integrated loudness (LUFS), the loudest short-term "
            "loudness, peak and true peak, crest (peak over loudness), how much of the song it plays in, ten octave bands (dBFS where it plays), "
            "stereo correlation and the side's level against the middle's; and masking: where two stems are loud in the same octave within 6 dB of "
            "each other, the worst first. Also the board's and the Mastering section's settings now. Changes nothing. Takes a few seconds; "
            "measure at most three times in a request.\",\"input_schema\":{\"type\":\"object\",\"properties\":{}}}"));

        // The board and the Mastering section, as Sonify AI sets them.
        if (const auto* all = AiTools::definitions().getArray())
            for (const auto& definition : *all)
                if (const auto name = definition.getProperty ("name", {}).toString(); name == "get_board" || name == "set_board")
                    definitionList.add (definition);

        definitionList.add (Skills::toolDefinition (Skills::For::mixing));
        return juce::var (definitionList);
    }();

    return list;
}

void AiMix::run (const juce::String& name, const juce::var& input, std::function<void (const juce::var&)> done)
{
    if (name == "measure_mix")
    {
        if (hooks.measure == nullptr)
            return done (failure ("Measuring isn't available."));

        // The numbers as compact text (two decimals): far fewer tokens than the full JSON.
        hooks.measure ([done = std::move (done)] (const juce::var& report)
        {
            done (report.hasProperty ("error") ? report : juce::var (MixAnalysis::toText (report)));
        });
        return;
    }

    if (name == "read_skill")
    {
        juce::String text;

        if (const auto result = Skills::read (Skills::For::mixing, input.getProperty ("name", {}).toString(), text); result.failed())
            return done (failure (result.getErrorMessage()));

        auto* object = new juce::DynamicObject();
        object->setProperty ("skill", text);
        return done (juce::var (object));
    }

    if (name == "get_board")
        return done (tools.run (name, input));

    if (name == "set_board")
    {
        // Master only: the Mastering section, and nothing of the mix.
        if (hooks.masterOnly != nullptr && hooks.masterOnly())
            for (const auto* key : { "strips", "master", "mixing", "reset", "copy_console" })
                if (input.hasProperty (key))
                    return done (failure (juce::String ("Master only: change only \"mastering\" (") + key + " was sent). The mix stays as the user made it."));

        return done (tools.run (name, input));
    }

    done (failure ("There's no tool called " + name + "."));
}

juce::String AiMix::describe (const juce::String& name, const juce::var& input, const juce::var& result)
{
    if (name == "set_board")
    {
        if (input.hasProperty ("mastering") && ! input.hasProperty ("strips") && ! input.hasProperty ("master"))
            return "Set the Mastering section";

        return AiTools::describe (name, input, result);
    }

    return {};
}

bool AiMix::looksOnly (const juce::String& name)
{
    return name == "measure_mix" || name == "get_board" || name == "read_skill";
}

void AiMix::cancel()
{
    if (hooks.cancelMeasure != nullptr)
        hooks.cancelMeasure();
}
