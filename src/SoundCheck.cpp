// D:\workspace\Fanan_web_hub\src\SoundCheck.cpp

#include "SoundCheck.h"
#include "SoundMeasure.h"
#include "WasmPlugin.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr double rate = 48000.0;
    constexpr int blockSize = 256;
    constexpr double preRoll = 0.3;   // seconds of quiet first: smoothed parameters settle on their values

    juce::String noteName (int note)
    {
        static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[((note % 12) + 12) % 12]) + juce::String (note / 12 - 1);
    }

    /** One render of the test into take; false (with why) if the plugin couldn't play it. */
    bool render (const std::vector<std::uint8_t>& wasm, const SoundCheck::Request& request, float velocity,
                 SoundMeasure::Take& take, juce::String& why)
    {
        std::string error;
        auto plugin = WasmPlugin::load (wasm, rate, blockSize, error);

        if (plugin == nullptr)
        {
            why = "The plugin couldn't start for the test: " + juce::String (error);
            return false;
        }

        for (const auto& [index, value] : request.values)
            plugin->setParameter (index, value);

        for (const auto module : request.bypassed)
            plugin->setBypass (module, true);

        // A sampler: its instrument loaded before the test (blocks of quiet, not kept, meanwhile).
        if (! request.library.empty())
        {
            plugin->setLibrary (request.library);
            std::vector<float> quietLeft ((std::size_t) blockSize), quietRight ((std::size_t) blockSize);
            const auto started = juce::Time::getMillisecondCounter();

            for (;;)
            {
                plugin->process (nullptr, nullptr, quietLeft.data(), quietRight.data(), blockSize, nullptr, 0);
                const auto elapsed = juce::Time::getMillisecondCounter() - started;
                bool loading = false;

                for (int i = 0; i < (int) request.library.size(); ++i)
                    loading = loading || plugin->getLibraryState (i) == stella::blob::loading;

                if ((elapsed >= 120 && ! loading) || elapsed > 15000)
                    break;

                juce::Thread::sleep (5);
            }
        }

        const auto total = (std::size_t) ((preRoll + request.hold + request.tail) * rate);
        const auto startAt = (std::size_t) (preRoll * rate);
        const auto releaseAt = (std::size_t) ((preRoll + request.hold) * rate);

        take = {};
        take.sampleRate = rate;
        take.start = preRoll;
        take.release = preRoll + request.hold;
        take.notes = request.notes;
        take.pitched = request.pitched;
        take.velocity = velocity;
        take.left.assign (total, 0.0f);
        take.right.assign (total, 0.0f);

        std::vector<float> inLeft, inRight;

        if (! request.instrument)
        {
            std::vector<float> signalLeft, signalRight;
            SoundMeasure::makeSignal (request.signal, rate, request.hold, signalLeft, signalRight);

            inLeft.assign (total, 0.0f);
            inRight.assign (total, 0.0f);

            for (std::size_t i = 0; i < signalLeft.size() && startAt + i < total; ++i)
            {
                inLeft[startAt + i] = signalLeft[i];
                inRight[startAt + i] = i < signalRight.size() ? signalRight[i] : signalLeft[i];
            }

            take.signal = request.signal;
            take.inLeft = inLeft;
            take.inRight = inRight;
        }

        const auto velocityByte = (std::uint8_t) juce::jlimit (1, 127, (int) std::lround (velocity * 127.0f));
        std::vector<WasmPlugin::MidiEvent> events;

        for (std::size_t position = 0; position < total; position += blockSize)
        {
            const auto frames = (int) std::min<std::size_t> (blockSize, total - position);
            events.clear();

            if (request.instrument)
            {
                if (startAt >= position && startAt < position + (std::size_t) frames)
                    for (const auto note : request.notes)
                        events.push_back ({ (std::uint32_t) (startAt - position), 0x90, (std::uint8_t) note, velocityByte });

                if (releaseAt >= position && releaseAt < position + (std::size_t) frames)
                    for (const auto note : request.notes)
                        events.push_back ({ (std::uint32_t) (releaseAt - position), 0x80, (std::uint8_t) note, 0 });
            }

            const auto* inL = request.instrument ? nullptr : inLeft.data() + position;
            const auto* inR = request.instrument ? nullptr : inRight.data() + position;

            if (! plugin->process (inL, inR, take.left.data() + position, take.right.data() + position, frames,
                                   events.empty() ? nullptr : events.data(), (int) events.size()))
            {
                why = "The plugin stopped during the test: " + juce::String (plugin->getFailure())
                    + ". It would stop the same way in a song: fix it first.";
                return false;
            }
        }

        return true;
    }
}

juce::String SoundCheck::run (const std::vector<std::uint8_t>& wasm, const Request& request, bool& isError)
{
    isError = false;

    if (wasm.empty())
    {
        isError = true;
        return "Nothing is playing yet: build the plugin first.";
    }

    SoundMeasure::Take main, soft;
    juce::String why;

    if (! render (wasm, request, request.velocity, main, why))
    {
        isError = true;
        return why;
    }

    // An instrument: the same notes played softly too, for how it answers to velocity.
    const bool withSoft = request.instrument && request.velocity > 0.45f;

    if (withSoft && ! render (wasm, request, 0.3f, soft, why))
    {
        isError = true;
        return why;
    }

    juce::String heading;

    if (request.instrument)
    {
        juce::StringArray names;

        for (const auto note : request.notes)
            names.add (noteName (note));

        heading << "Test: " << names.joinIntoString (" + ") << " held " << juce::String (request.hold, 2) << " s at velocity "
                << juce::roundToInt (request.velocity * 100.0f) << "%, then " << juce::String (request.tail, 2) << " s after the note"
                << (request.notes.size() > 1 ? "s end" : " ends") << ".";
    }
    else
    {
        heading << "Test: the \"" << juce::String (request.signal) << "\" signal for " << juce::String (request.hold, 2)
                << " s, then " << juce::String (request.tail, 2) << " s of silence.";
    }

    if (! request.values.empty())
        heading << " With the values given for this test only (the plugin itself didn't change).";

    return heading + "\n" + juce::String (SoundMeasure::describe (main, withSoft ? &soft : nullptr)).trimEnd();
}
