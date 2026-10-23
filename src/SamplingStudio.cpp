// D:\workspace\Fanan_web_hub\src\SamplingStudio.cpp

#include "SamplingStudio.h"
#include "LoopFinder.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    // The takes go from the audio thread to the keeper in chunks: frames, or a marker (a take
    // begins, ends, or is thrown away). The last few chunks are kept for the markers.
    constexpr int chunkFrames = 1024;
    constexpr int queueChunks = 512;
    constexpr int reservedChunks = 8;

    enum ChunkType { framesChunk = 0, beginChunk, endChunk, dropChunk };

    struct Chunk
    {
        int type = framesChunk, shot = -1, value = 0, count = 0;
        float left[chunkFrames] {};
        float right[chunkFrames] {};
    };

    juce::int64 samplesFor (double ms, double rate)
    {
        return static_cast<juce::int64> (std::llround (juce::jmax (0.0, ms) * rate / 1000.0));
    }

    void storeMax (std::atomic<float>& peak, float value) noexcept
    {
        auto now = peak.load (std::memory_order_relaxed);

        while (value > now && ! peak.compare_exchange_weak (now, value, std::memory_order_relaxed))
        {
        }
    }

    float peakOf (const float* data, int numSamples) noexcept
    {
        if (data == nullptr || numSamples <= 0)
            return 0.0f;

        const auto range = juce::FloatVectorOperations::findMinAndMax (data, numSamples);
        return juce::jmax (-range.getStart(), range.getEnd());
    }

    /** A block's level as Sampler Arena measures it: the RMS of both channels. */
    float rmsOf (const float* left, const float* right, int numSamples) noexcept
    {
        if (numSamples <= 0)
            return 0.0f;

        double sum = 0.0;

        for (int i = 0; i < numSamples; ++i)
            sum += static_cast<double> (left[i]) * left[i] + static_cast<double> (right[i]) * right[i];

        return static_cast<float> (std::sqrt (sum / (2.0 * numSamples)));
    }

    juce::String stripWav (const juce::String& name)
    {
        return name.endsWithIgnoreCase (".wav") ? name.dropLastCharacters (4) : name;
    }

    /** "a,b,c," -> the numbers. */
    std::vector<int> numbersOf (const juce::String& text, const juce::String& separator = ",")
    {
        std::vector<int> found;

        for (const auto& token : juce::StringArray::fromTokens (text, separator, ""))
            if (token.trim().isNotEmpty())
                found.push_back (token.trim().getIntValue());

        return found;
    }

    std::vector<Sampling::VelocityLayer> layersOf (const juce::String& text)
    {
        std::vector<Sampling::VelocityLayer> layers;

        for (const auto& token : juce::StringArray::fromTokens (text, ",", ""))
        {
            const auto t = token.trim();

            if (t.contains ("-"))
            {
                const auto dash = t.indexOf ("-");
                layers.push_back ({ juce::jlimit (1, 127, t.substring (0, dash).getIntValue()), juce::jlimit (1, 127, t.substring (dash + 1).getIntValue()) });
            }
        }

        return layers;
    }

    juce::String layersText (const std::vector<Sampling::VelocityLayer>& layers)
    {
        juce::String text;

        for (const auto& layer : layers)
            text << layer.low << "-" << layer.high << ",";

        return text;
    }

    juce::var layersVar (const std::vector<Sampling::VelocityLayer>& layers)
    {
        juce::Array<juce::var> list;

        for (const auto& layer : layers)
            list.add (juce::Array<juce::var> { layer.low, layer.high });

        return list;
    }

    std::vector<Sampling::VelocityLayer> layersFromVar (const juce::var& value)
    {
        std::vector<Sampling::VelocityLayer> layers;

        if (const auto* list = value.getArray())
        {
            for (const auto& item : *list)
            {
                if (const auto* pair = item.getArray(); pair != nullptr && pair->size() == 2)
                    layers.push_back ({ juce::jlimit (1, 127, static_cast<int> ((*pair)[0])), juce::jlimit (1, 127, static_cast<int> ((*pair)[1])) });

                if (layers.size() >= 16)
                    break;
            }
        }

        return layers;
    }

    juce::String modeName (Sampling::Mode mode)
    {
        return mode == Sampling::Mode::chord ? "chord" : mode == Sampling::Mode::builder ? "builder" : "vsti";
    }

    juce::String speedName (Sampling::Speed speed)
    {
        return speed == Sampling::Speed::turbo ? "turbo" : speed == Sampling::Speed::unleashed ? "unleashed" : "realtime";
    }

    /** A take's channels as the WAVs have them: two (stereo), the left, or the two mixed. */
    void applyChannelMode (juce::AudioBuffer<float>& audio, int frames, int mode, int& channelsOut)
    {
        if (mode == 0)
        {
            if (audio.getNumChannels() == 1)
            {
                audio.setSize (2, audio.getNumSamples(), true, false, true);
                audio.copyFrom (1, 0, audio, 0, 0, frames);
            }

            channelsOut = 2;
            return;
        }

        if (mode == 2 && audio.getNumChannels() > 1)
        {
            audio.addFrom (0, 0, audio, 1, 0, frames);
            audio.applyGain (0, 0, frames, 0.5f);
        }

        channelsOut = 1;
    }

    /** Sampler Arena's fade: a straight line from full to nothing over the last ms. */
    void fadeTail (juce::AudioBuffer<float>& audio, int frames, double rate, int fadeMs)
    {
        const auto fade = static_cast<int> (juce::jmin<juce::int64> (frames, samplesFor (fadeMs, rate)));

        if (fade < 2)
            return;

        const auto start = frames - fade;

        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        {
            auto* data = audio.getWritePointer (channel);

            for (int i = 0; i < fade; ++i)
                data[start + i] *= 1.0f - static_cast<float> (i) / static_cast<float> (fade);
        }
    }
}

//==============================================================================
const std::vector<Sampling::ChordType>& Sampling::chordTypes()
{
    static const std::vector<ChordType> types {
        { "Major",      "Maj",  { 0, 4, 7 } },
        { "Minor",      "Min",  { 0, 3, 7 } },
        { "Diminished", "Dim",  { 0, 3, 6 } },
        { "Augmented",  "Aug",  { 0, 4, 8 } },
        { "Sus2",       "Sus2", { 0, 2, 7 } },
        { "Sus4",       "Sus4", { 0, 5, 7 } },
        { "Dom 7th",    "7",    { 0, 4, 7, 10 } },
        { "Maj 7th",    "Maj7", { 0, 4, 7, 11 } },
        { "Min 7th",    "Min7", { 0, 3, 7, 10 } },
        { "Dim 7th",    "Dim7", { 0, 3, 6, 9 } },
        { "Aug 7th",    "Aug7", { 0, 4, 8, 10 } },
        { "Min/Maj 7",  "mM7",  { 0, 3, 7, 11 } },
        { "Dom 9th",    "9",    { 0, 4, 7, 10, 14 } },
        { "Maj 9th",    "Maj9", { 0, 4, 7, 11, 14 } },
        { "Min 9th",    "Min9", { 0, 3, 7, 10, 14 } },
        { "Add 9",      "Add9", { 0, 4, 7, 14 } },
        { "6th",        "6",    { 0, 4, 7, 9 } },
        { "Min 6th",    "Min6", { 0, 3, 7, 9 } },
    };

    return types;
}

//==============================================================================
Sampling::Settings Sampling::Settings::fromVar (const juce::var& value)
{
    Settings s;
    const auto mode = value.getProperty ("mode", "vsti").toString();
    s.mode = mode == "chord" ? Mode::chord : mode == "builder" ? Mode::builder : Mode::vsti;
    const auto source = value.getProperty ("source", "nodes").toString();   // "instrument" before the nodes: the nodes
    s.source = source == "hardware" ? Source::hardware : source == "voice" ? Source::voice : Source::nodes;
    s.trackId = static_cast<int> (value.getProperty ("trackId", 0));
    s.midiOutput = value.getProperty ("midiOutput", {}).toString();
    s.midiChannel = juce::jlimit (1, 16, static_cast<int> (value.getProperty ("midiChannel", 1)));

    s.family = value.getProperty ("family", "Sample").toString().trim();

    if (s.family.isEmpty())
        s.family = "Sample";

    if (const auto* keys = value.getProperty ("keys", {}).getArray())
        for (const auto& key : *keys)
            s.keys.insert (juce::jlimit (0, 127, static_cast<int> (key)));

    if (auto* boundaries = value.getProperty ("boundaries", {}).getDynamicObject())
        for (const auto& b : boundaries->getProperties())
            s.boundaries[b.name.toString().getIntValue()] = juce::jlimit (0, 127, static_cast<int> (b.value));

    s.noKeyRanges = static_cast<bool> (value.getProperty ("noKeyRanges", false));
    s.fillGap = static_cast<bool> (value.getProperty ("fillGap", false));

    if (auto layers = layersFromVar (value.getProperty ("layers", {})); ! layers.empty())
        s.layers = layers;

    s.samplingVelocity = juce::jlimit (1, 127, static_cast<int> (value.getProperty ("samplingVelocity", 127)));
    s.velocityOffset = juce::jlimit (0, 30, static_cast<int> (value.getProperty ("velocityOffset", 3)));
    s.keyShift = juce::jlimit (-48, 48, static_cast<int> (value.getProperty ("keyShift", 0)));

    s.durationMode = value.getProperty ("durationMode", "time").toString() == "bars" ? DurationMode::bars : DurationMode::time;
    s.durationMs = juce::jlimit (200.0f, 30000.0f, static_cast<float> (value.getProperty ("durationMs", 2000.0)));
    s.durationBars = juce::jlimit (0.25f, 16.0f, static_cast<float> (value.getProperty ("durationBars", 1.0)));
    s.timeSigNumerator = juce::jlimit (1, 16, static_cast<int> (value.getProperty ("timeSigNum", 4)));
    s.timeSigDenominator = juce::jlimit (1, 32, static_cast<int> (value.getProperty ("timeSigDen", 4)));

    if (const auto* bpms = value.getProperty ("bpmLayers", {}).getArray())
        for (const auto& bpm : *bpms)
            s.bpmLayers.push_back (juce::jlimit (20, 300, static_cast<int> (bpm)));

    std::sort (s.bpmLayers.begin(), s.bpmLayers.end());
    s.bpmLayers.erase (std::unique (s.bpmLayers.begin(), s.bpmLayers.end()), s.bpmLayers.end());

    s.minChordKey = juce::jlimit (0, 127, static_cast<int> (value.getProperty ("minChordKey", 48)));
    s.maxChordKey = juce::jlimit (0, 127, static_cast<int> (value.getProperty ("maxChordKey", 72)));

    if (const auto* chords = value.getProperty ("chords", {}).getArray())
        for (const auto& chord : *chords)
            if (juce::isPositiveAndBelow (static_cast<int> (chord), static_cast<int> (chordTypes().size())))
                s.chords.insert (static_cast<int> (chord));

    if (auto* overrides = value.getProperty ("overrides", {}).getDynamicObject())
    {
        for (const auto& item : overrides->getProperties())
        {
            const auto note = item.name.toString().getIntValue();

            if (! juce::isPositiveAndBelow (note, 128))
                continue;

            KeyOverride o;

            if (const auto layers = layersFromVar (item.value.getProperty ("vel", {})); ! layers.empty())
            {
                o.customVelocity = true;
                o.layers = layers;
            }

            if (const auto duration = item.value.getProperty ("dur", {}); duration.isObject())
            {
                o.customDuration = true;
                o.durationMode = duration.getProperty ("mode", "time").toString() == "bars" ? DurationMode::bars : DurationMode::time;
                o.durationMs = juce::jlimit (200.0f, 30000.0f, static_cast<float> (duration.getProperty ("ms", 2000.0)));
                o.durationBars = juce::jlimit (0.25f, 16.0f, static_cast<float> (duration.getProperty ("bars", 1.0)));
            }

            if (const auto shift = item.value.getProperty ("shift", {}); ! shift.isVoid() && ! shift.isUndefined())
            {
                o.customKeyShift = true;
                o.keyShift = juce::jlimit (-48, 48, static_cast<int> (shift));
            }

            if (o.customVelocity || o.customDuration || o.customKeyShift)
                s.overrides[note] = o;
        }
    }

    if (auto* names = value.getProperty ("customNames", {}).getDynamicObject())
        for (const auto& item : names->getProperties())
            if (const auto name = item.value.toString().trim(); name.isNotEmpty())
                s.customNames[juce::jlimit (0, 127, item.name.toString().getIntValue())] = name;

    if (auto* slots = value.getProperty ("slots", {}).getDynamicObject())
    {
        for (const auto& item : slots->getProperties())
        {
            const auto note = item.name.toString().getIntValue();

            if (! juce::isPositiveAndBelow (note, 128))
                continue;

            if (const auto* list = item.value.getArray())
            {
                for (const auto& slotVar : *list)
                {
                    BuilderSlot slot;
                    slot.file = juce::File (slotVar.getProperty ("path", {}).toString());

                    if (slot.file.getFullPathName().isEmpty() || ! juce::File::isAbsolutePath (slot.file.getFullPathName()))
                        continue;

                    slot.name = slotVar.getProperty ("name", slot.file.getFileNameWithoutExtension()).toString().trim();

                    if (slot.name.isEmpty())
                        slot.name = slot.file.getFileNameWithoutExtension();

                    slot.lovel = juce::jlimit (0, 127, static_cast<int> (slotVar.getProperty ("lovel", 0)));
                    slot.hivel = juce::jlimit (0, 127, static_cast<int> (slotVar.getProperty ("hivel", 127)));

                    if (slot.lovel > slot.hivel)
                        std::swap (slot.lovel, slot.hivel);

                    if (const auto clip = slotVar.getProperty ("clip", {}); clip.isObject())
                    {
                        slot.clip = true;
                        slot.offset = juce::jmax (0.0, static_cast<double> (clip.getProperty ("offset", 0.0)));
                        slot.length = juce::jmax (0.0, static_cast<double> (clip.getProperty ("length", 0.0)));
                        slot.gainDb = juce::jlimit (-72.0, 24.0, static_cast<double> (clip.getProperty ("gainDb", 0.0)));
                        slot.fadeIn = juce::jmax (0.0, static_cast<double> (clip.getProperty ("fadeIn", 0.0)));
                        slot.fadeOut = juce::jmax (0.0, static_cast<double> (clip.getProperty ("fadeOut", 0.0)));
                    }

                    s.slots[note].push_back (slot);
                }
            }
        }
    }

    s.silenceDb = juce::jlimit (-90.0f, -20.0f, static_cast<float> (value.getProperty ("silenceDb", -60.0)));
    s.silenceMs = juce::jlimit (100.0f, 5000.0f, static_cast<float> (value.getProperty ("silenceMs", 500.0)));
    s.fadeOut = static_cast<bool> (value.getProperty ("fadeOut", true));
    s.fadeMs = juce::jlimit (10, 2000, static_cast<int> (value.getProperty ("fadeMs", 200)));
    s.createSfz = static_cast<bool> (value.getProperty ("createSfz", false));
    s.autoLoop = static_cast<bool> (value.getProperty ("autoLoop", false));

    const auto bits = static_cast<int> (value.getProperty ("bitDepth", 24));
    s.bitDepth = bits == 16 || bits == 32 ? bits : 24;
    const auto rate = static_cast<int> (value.getProperty ("sampleRate", 0));
    s.sampleRateChoice = rate == 44100 || rate == 48000 || rate == 88200 || rate == 96000 || rate == 192000 ? rate : 0;
    s.channelMode = juce::jlimit (0, 2, static_cast<int> (value.getProperty ("channelMode", 0)));
    const auto speed = value.getProperty ("speed", "realtime").toString();
    s.speed = speed == "turbo" ? Speed::turbo : speed == "unleashed" ? Speed::unleashed : Speed::realTime;

    const auto takes = static_cast<int> (value.getProperty ("roundRobins", 0));
    s.roundRobins = takes < 2 ? 0 : juce::jmin (4, takes);
    s.trim = static_cast<bool> (value.getProperty ("trim", false));
    s.normalize = static_cast<bool> (value.getProperty ("normalize", false));
    s.triggerDb = juce::jlimit (-60.0f, -6.0f, static_cast<float> (value.getProperty ("triggerDb", -35.0)));
    return s;
}

juce::var Sampling::Settings::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("mode", modeName (mode));
    o->setProperty ("source", source == Source::hardware ? "hardware" : source == Source::voice ? "voice" : "nodes");
    o->setProperty ("trackId", trackId);
    o->setProperty ("midiOutput", midiOutput);
    o->setProperty ("midiChannel", midiChannel);
    o->setProperty ("family", family);

    juce::Array<juce::var> keyList;

    for (const auto key : keys)
        keyList.add (key);

    o->setProperty ("keys", keyList);

    auto* boundaryObject = new juce::DynamicObject();

    for (const auto& [zone, key] : boundaries)
        boundaryObject->setProperty (juce::String (zone), key);

    o->setProperty ("boundaries", juce::var (boundaryObject));
    o->setProperty ("noKeyRanges", noKeyRanges);
    o->setProperty ("fillGap", fillGap);
    o->setProperty ("layers", layersVar (layers));
    o->setProperty ("samplingVelocity", samplingVelocity);
    o->setProperty ("velocityOffset", velocityOffset);
    o->setProperty ("keyShift", keyShift);
    o->setProperty ("durationMode", durationMode == DurationMode::bars ? "bars" : "time");
    o->setProperty ("durationMs", durationMs);
    o->setProperty ("durationBars", durationBars);
    o->setProperty ("timeSigNum", timeSigNumerator);
    o->setProperty ("timeSigDen", timeSigDenominator);

    juce::Array<juce::var> bpmList;

    for (const auto bpm : bpmLayers)
        bpmList.add (bpm);

    o->setProperty ("bpmLayers", bpmList);
    o->setProperty ("minChordKey", minChordKey);
    o->setProperty ("maxChordKey", maxChordKey);

    juce::Array<juce::var> chordList;

    for (const auto chord : chords)
        chordList.add (chord);

    o->setProperty ("chords", chordList);

    auto* overrideObject = new juce::DynamicObject();

    for (const auto& [note, k] : overrides)
    {
        auto* item = new juce::DynamicObject();

        if (k.customVelocity)
            item->setProperty ("vel", layersVar (k.layers));

        if (k.customDuration)
        {
            auto* duration = new juce::DynamicObject();
            duration->setProperty ("mode", k.durationMode == DurationMode::bars ? "bars" : "time");
            duration->setProperty ("ms", k.durationMs);
            duration->setProperty ("bars", k.durationBars);
            item->setProperty ("dur", juce::var (duration));
        }

        if (k.customKeyShift)
            item->setProperty ("shift", k.keyShift);

        overrideObject->setProperty (juce::String (note), juce::var (item));
    }

    o->setProperty ("overrides", juce::var (overrideObject));

    auto* nameObject = new juce::DynamicObject();

    for (const auto& [note, name] : customNames)
        nameObject->setProperty (juce::String (note), name);

    o->setProperty ("customNames", juce::var (nameObject));

    auto* slotObject = new juce::DynamicObject();

    for (const auto& [note, list] : slots)
    {
        juce::Array<juce::var> items;

        for (const auto& slot : list)
        {
            auto* item = new juce::DynamicObject();
            item->setProperty ("path", slot.file.getFullPathName());
            item->setProperty ("name", slot.name);
            item->setProperty ("lovel", slot.lovel);
            item->setProperty ("hivel", slot.hivel);

            if (slot.clip)
            {
                auto* clip = new juce::DynamicObject();
                clip->setProperty ("offset", slot.offset);
                clip->setProperty ("length", slot.length);
                clip->setProperty ("gainDb", slot.gainDb);
                clip->setProperty ("fadeIn", slot.fadeIn);
                clip->setProperty ("fadeOut", slot.fadeOut);
                item->setProperty ("clip", juce::var (clip));
            }

            items.add (juce::var (item));
        }

        slotObject->setProperty (juce::String (note), items);
    }

    o->setProperty ("slots", juce::var (slotObject));
    o->setProperty ("silenceDb", silenceDb);
    o->setProperty ("silenceMs", silenceMs);
    o->setProperty ("fadeOut", fadeOut);
    o->setProperty ("fadeMs", fadeMs);
    o->setProperty ("createSfz", createSfz);
    o->setProperty ("autoLoop", autoLoop);
    o->setProperty ("bitDepth", bitDepth);
    o->setProperty ("sampleRate", sampleRateChoice);
    o->setProperty ("channelMode", channelMode);
    o->setProperty ("speed", speedName (speed));
    o->setProperty ("roundRobins", roundRobins);
    o->setProperty ("trim", trim);
    o->setProperty ("normalize", normalize);
    o->setProperty ("triggerDb", triggerDb);
    return juce::var (o);
}

juce::ValueTree Sampling::Settings::toPreset() const
{
    juce::ValueTree vt ("AutoSamplerState");
    vt.setProperty ("familyName", family, nullptr);
    vt.setProperty ("editorText", juce::String(), nullptr);
    vt.setProperty ("silenceThresholdDb", static_cast<double> (silenceDb), nullptr);
    vt.setProperty ("silenceDurationMs", static_cast<double> (silenceMs), nullptr);
    vt.setProperty ("noteHoldMs", static_cast<double> (durationMs), nullptr);
    vt.setProperty ("createSfz", createSfz, nullptr);
    vt.setProperty ("fillGap", fillGap, nullptr);
    vt.setProperty ("noKeyRanges", noKeyRanges, nullptr);
    vt.setProperty ("autoLoop", autoLoop, nullptr);
    vt.setProperty ("recordingBitDepth", bitDepth, nullptr);
    vt.setProperty ("recordingSampleRate", sampleRateChoice, nullptr);
    vt.setProperty ("fadeOutEnabled", fadeOut, nullptr);
    vt.setProperty ("fadeOutMs", fadeMs, nullptr);
    vt.setProperty ("channelMode", channelMode, nullptr);

    juce::String keyText;

    for (const auto key : keys)
        keyText << key << ",";

    vt.setProperty ("selectedKeys", keyText, nullptr);

    juce::String boundaryText;

    for (const auto& [zone, key] : boundaries)
        boundaryText << zone << ":" << key << ",";

    vt.setProperty ("zoneBoundaries", boundaryText, nullptr);
    vt.setProperty ("globalVelLayers", layersText (layers), nullptr);
    vt.setProperty ("globalDurationMs", static_cast<double> (durationMs), nullptr);
    vt.setProperty ("globalDurationBars", static_cast<double> (durationBars), nullptr);
    vt.setProperty ("globalDurationMode", durationMode == DurationMode::bars ? 1 : 0, nullptr);
    vt.setProperty ("globalKeyShift", keyShift, nullptr);
    vt.setProperty ("globalSamplingVelocity", samplingVelocity, nullptr);
    vt.setProperty ("samplingVelocityOffset", velocityOffset, nullptr);
    vt.setProperty ("minChordKey", minChordKey, nullptr);
    vt.setProperty ("maxChordKey", maxChordKey, nullptr);

    juce::StringArray bpmText;

    for (const auto bpm : bpmLayers)
        bpmText.add (juce::String (bpm));

    vt.setProperty ("bpmLayers", bpmText.joinIntoString (","), nullptr);
    vt.setProperty ("timeSigNumerator", timeSigNumerator, nullptr);
    vt.setProperty ("timeSigDenominator", timeSigDenominator, nullptr);
    vt.setProperty ("speedMode", static_cast<int> (speed), nullptr);

    juce::String nameText;

    for (const auto& [note, name] : customNames)
        if (name.isNotEmpty())
            nameText << note << "=" << name << ";";

    vt.setProperty ("keyCustomNames", nameText, nullptr);
    vt.setProperty ("chordMode", mode == Mode::chord, nullptr);
    vt.setProperty ("samplerMode", mode == Mode::chord ? 1 : mode == Mode::builder ? 2 : 0, nullptr);

    juce::ValueTree builder ("SampleBuilderSlots");

    for (const auto& [note, list] : slots)
    {
        for (size_t i = 0; i < list.size(); ++i)
        {
            const auto& slot = list[i];
            juce::ValueTree item ("Slot");
            item.setProperty ("note", note, nullptr);
            item.setProperty ("layer", static_cast<int> (i), nullptr);
            item.setProperty ("source", slot.file.getFullPathName(), nullptr);
            item.setProperty ("custom", slot.name, nullptr);
            item.setProperty ("lovel", slot.lovel, nullptr);
            item.setProperty ("hivel", slot.hivel, nullptr);

            if (slot.clip)
            {
                item.setProperty ("sonifyClip", true, nullptr);
                item.setProperty ("sonifyOffset", slot.offset, nullptr);
                item.setProperty ("sonifyLength", slot.length, nullptr);
                item.setProperty ("sonifyGainDb", slot.gainDb, nullptr);
                item.setProperty ("sonifyFadeIn", slot.fadeIn, nullptr);
                item.setProperty ("sonifyFadeOut", slot.fadeOut, nullptr);
            }

            builder.addChild (item, -1, nullptr);
        }
    }

    vt.addChild (builder, -1, nullptr);

    juce::String chordText;

    for (const auto chord : chords)
        chordText << chord << ",";

    vt.setProperty ("selectedChordTypes", chordText, nullptr);

    for (const auto& [note, k] : overrides)
    {
        juce::ValueTree item ("KeyOverride");
        item.setProperty ("note", note, nullptr);
        item.setProperty ("customVel", k.customVelocity, nullptr);
        item.setProperty ("customDur", k.customDuration, nullptr);
        item.setProperty ("durMs", static_cast<double> (k.durationMs), nullptr);
        item.setProperty ("durBars", static_cast<double> (k.durationBars), nullptr);
        item.setProperty ("durMode", k.durationMode == DurationMode::bars ? 1 : 0, nullptr);
        item.setProperty ("customKeyShift", k.customKeyShift, nullptr);
        item.setProperty ("keyShift", k.keyShift, nullptr);
        item.setProperty ("velLayers", layersText (k.layers), nullptr);
        vt.addChild (item, -1, nullptr);
    }

    // Sonify's own (Sampler Arena passes them by).
    vt.setProperty ("sonifySource", source == Source::hardware ? "hardware" : source == Source::voice ? "voice" : "nodes", nullptr);
    vt.setProperty ("sonifyMidiChannel", midiChannel, nullptr);
    vt.setProperty ("sonifyRoundRobins", roundRobins, nullptr);
    vt.setProperty ("sonifyTrim", trim, nullptr);
    vt.setProperty ("sonifyNormalize", normalize, nullptr);
    vt.setProperty ("sonifyTriggerDb", static_cast<double> (triggerDb), nullptr);
    return vt;
}

Sampling::Settings Sampling::Settings::fromPreset (const juce::ValueTree& vt, const Settings& base)
{
    auto s = base;

    if (! vt.isValid())
        return s;

    s.family = vt.getProperty ("familyName", "Sample").toString().trim();

    if (s.family.isEmpty())
        s.family = "Sample";

    s.silenceDb = juce::jlimit (-90.0f, -20.0f, static_cast<float> (static_cast<double> (vt.getProperty ("silenceThresholdDb", -60.0))));
    s.silenceMs = juce::jlimit (100.0f, 5000.0f, static_cast<float> (static_cast<double> (vt.getProperty ("silenceDurationMs", 500.0))));
    s.createSfz = static_cast<bool> (vt.getProperty ("createSfz", false));
    s.fillGap = static_cast<bool> (vt.getProperty ("fillGap", false));
    s.noKeyRanges = static_cast<bool> (vt.getProperty ("noKeyRanges", false));
    s.autoLoop = static_cast<bool> (vt.getProperty ("autoLoop", false));
    const auto bits = static_cast<int> (vt.getProperty ("recordingBitDepth", 24));
    s.bitDepth = bits == 16 || bits == 32 ? bits : 24;
    const auto rate = static_cast<int> (vt.getProperty ("recordingSampleRate", 0));
    s.sampleRateChoice = rate == 44100 || rate == 48000 || rate == 88200 || rate == 96000 || rate == 192000 ? rate : 0;
    s.fadeOut = static_cast<bool> (vt.getProperty ("fadeOutEnabled", true));
    s.fadeMs = juce::jlimit (10, 2000, static_cast<int> (vt.getProperty ("fadeOutMs", 200)));
    s.channelMode = juce::jlimit (0, 2, static_cast<int> (vt.getProperty ("channelMode", 0)));

    s.keys.clear();

    for (const auto key : numbersOf (vt.getProperty ("selectedKeys", {}).toString()))
        if (juce::isPositiveAndBelow (key, 128))
            s.keys.insert (key);

    s.boundaries.clear();

    for (const auto& token : juce::StringArray::fromTokens (vt.getProperty ("zoneBoundaries", {}).toString(), ",", ""))
        if (const auto t = token.trim(); t.contains (":"))
            s.boundaries[t.upToFirstOccurrenceOf (":", false, false).getIntValue()] = juce::jlimit (0, 127, t.fromFirstOccurrenceOf (":", false, false).getIntValue());

    if (auto layers = layersOf (vt.getProperty ("globalVelLayers", {}).toString()); ! layers.empty())
        s.layers = layers;
    else
        s.layers = { VelocityLayer {} };

    s.durationMs = juce::jlimit (200.0f, 30000.0f, static_cast<float> (static_cast<double> (vt.getProperty ("globalDurationMs", vt.getProperty ("noteHoldMs", 2000.0)))));
    s.durationBars = juce::jlimit (0.25f, 16.0f, static_cast<float> (static_cast<double> (vt.getProperty ("globalDurationBars", 1.0))));
    s.durationMode = static_cast<int> (vt.getProperty ("globalDurationMode", 0)) == 1 ? DurationMode::bars : DurationMode::time;
    s.keyShift = juce::jlimit (-48, 48, static_cast<int> (vt.getProperty ("globalKeyShift", 0)));
    s.samplingVelocity = juce::jlimit (1, 127, static_cast<int> (vt.getProperty ("globalSamplingVelocity", 127)));
    s.velocityOffset = juce::jlimit (0, 30, static_cast<int> (vt.getProperty ("samplingVelocityOffset", 3)));
    s.minChordKey = juce::jlimit (0, 127, static_cast<int> (vt.getProperty ("minChordKey", 48)));
    s.maxChordKey = juce::jlimit (0, 127, static_cast<int> (vt.getProperty ("maxChordKey", 72)));

    s.bpmLayers.clear();
    auto bpmText = vt.getProperty ("bpmLayers", {}).toString();

    if (bpmText.isEmpty())
        bpmText = vt.getProperty ("chordBpmLayers", {}).toString();

    for (const auto bpm : numbersOf (bpmText))
        if (bpm > 0)
            s.bpmLayers.push_back (juce::jlimit (20, 300, bpm));

    std::sort (s.bpmLayers.begin(), s.bpmLayers.end());
    s.bpmLayers.erase (std::unique (s.bpmLayers.begin(), s.bpmLayers.end()), s.bpmLayers.end());

    s.timeSigNumerator = juce::jlimit (1, 16, static_cast<int> (vt.getProperty ("timeSigNumerator", 4)));
    s.timeSigDenominator = juce::jlimit (1, 32, static_cast<int> (vt.getProperty ("timeSigDenominator", 4)));
    const auto speed = juce::jlimit (0, 2, static_cast<int> (vt.getProperty ("speedMode", 0)));
    s.speed = speed == 1 ? Speed::turbo : speed == 2 ? Speed::unleashed : Speed::realTime;

    s.customNames.clear();

    for (const auto& token : juce::StringArray::fromTokens (vt.getProperty ("keyCustomNames", {}).toString(), ";", ""))
    {
        const auto t = token.trim();
        const auto equals = t.indexOfChar ('=');

        if (equals > 0 && t.substring (equals + 1).isNotEmpty())
            s.customNames[juce::jlimit (0, 127, t.substring (0, equals).getIntValue())] = t.substring (equals + 1);
    }

    const auto samplerMode = static_cast<int> (vt.getProperty ("samplerMode", static_cast<bool> (vt.getProperty ("chordMode", false)) ? 1 : 0));
    s.mode = samplerMode == 1 ? Mode::chord : samplerMode == 2 ? Mode::builder : Mode::vsti;

    s.slots.clear();

    if (const auto builder = vt.getChildWithName ("SampleBuilderSlots"); builder.isValid())
    {
        std::map<int, std::map<int, BuilderSlot>> ordered;

        for (int i = 0; i < builder.getNumChildren(); ++i)
        {
            const auto item = builder.getChild (i);
            const auto note = static_cast<int> (item.getProperty ("note", -1));
            const auto layer = static_cast<int> (item.getProperty ("layer", -1));
            const auto source = item.getProperty ("source", {}).toString();

            if (! juce::isPositiveAndBelow (note, 128) || layer < 0 || source.isEmpty())
                continue;

            BuilderSlot slot;
            slot.file = juce::File (source);
            slot.name = item.getProperty ("custom", {}).toString().trim();

            if (slot.name.isEmpty())
                slot.name = slot.file.getFileNameWithoutExtension();

            slot.lovel = juce::jlimit (0, 127, static_cast<int> (item.getProperty ("lovel", 0)));
            slot.hivel = juce::jlimit (0, 127, static_cast<int> (item.getProperty ("hivel", 127)));
            slot.clip = static_cast<bool> (item.getProperty ("sonifyClip", false));
            slot.offset = static_cast<double> (item.getProperty ("sonifyOffset", 0.0));
            slot.length = static_cast<double> (item.getProperty ("sonifyLength", 0.0));
            slot.gainDb = static_cast<double> (item.getProperty ("sonifyGainDb", 0.0));
            slot.fadeIn = static_cast<double> (item.getProperty ("sonifyFadeIn", 0.0));
            slot.fadeOut = static_cast<double> (item.getProperty ("sonifyFadeOut", 0.0));
            ordered[note][layer] = slot;
        }

        for (const auto& [note, byLayer] : ordered)
            for (const auto& [layer, slot] : byLayer)
                s.slots[note].push_back (slot);
    }

    s.chords.clear();

    for (const auto chord : numbersOf (vt.getProperty ("selectedChordTypes", {}).toString()))
        if (juce::isPositiveAndBelow (chord, static_cast<int> (chordTypes().size())))
            s.chords.insert (chord);

    s.overrides.clear();

    for (int i = 0; i < vt.getNumChildren(); ++i)
    {
        const auto item = vt.getChild (i);

        if (! item.hasType ("KeyOverride"))
            continue;

        const auto note = static_cast<int> (item.getProperty ("note", -1));

        if (! juce::isPositiveAndBelow (note, 128))
            continue;

        KeyOverride k;
        k.customVelocity = static_cast<bool> (item.getProperty ("customVel", false));
        k.customDuration = static_cast<bool> (item.getProperty ("customDur", false));
        k.durationMs = juce::jlimit (200.0f, 30000.0f, static_cast<float> (static_cast<double> (item.getProperty ("durMs", 2000.0))));
        k.durationBars = juce::jlimit (0.25f, 16.0f, static_cast<float> (static_cast<double> (item.getProperty ("durBars", 1.0))));
        k.durationMode = static_cast<int> (item.getProperty ("durMode", 0)) == 1 ? DurationMode::bars : DurationMode::time;
        k.customKeyShift = static_cast<bool> (item.getProperty ("customKeyShift", false));
        k.keyShift = juce::jlimit (-48, 48, static_cast<int> (item.getProperty ("keyShift", 0)));
        k.layers = layersOf (item.getProperty ("velLayers", {}).toString());

        if (k.customVelocity && k.layers.empty())
            k.customVelocity = false;

        if (k.customVelocity || k.customDuration || k.customKeyShift)
            s.overrides[note] = k;
    }

    if (vt.hasProperty ("sonifySource"))
    {
        const auto source = vt.getProperty ("sonifySource").toString();
        s.source = source == "hardware" ? Source::hardware : source == "voice" ? Source::voice : Source::nodes;
        s.midiChannel = juce::jlimit (1, 16, static_cast<int> (vt.getProperty ("sonifyMidiChannel", s.midiChannel)));
        const auto takes = static_cast<int> (vt.getProperty ("sonifyRoundRobins", 0));
        s.roundRobins = takes < 2 ? 0 : juce::jmin (4, takes);
        s.trim = static_cast<bool> (vt.getProperty ("sonifyTrim", false));
        s.normalize = static_cast<bool> (vt.getProperty ("sonifyNormalize", false));
        s.triggerDb = juce::jlimit (-60.0f, -6.0f, static_cast<float> (static_cast<double> (vt.getProperty ("sonifyTriggerDb", -35.0))));
    }

    return s;
}

int Sampling::Settings::keyShiftFor (int note) const
{
    if (const auto found = overrides.find (note); found != overrides.end() && found->second.customKeyShift)
        return found->second.keyShift;

    return keyShift;
}

const std::vector<Sampling::VelocityLayer>& Sampling::Settings::layersFor (int note) const
{
    if (const auto found = overrides.find (note); found != overrides.end() && found->second.customVelocity && ! found->second.layers.empty())
        return found->second.layers;

    return layers;
}

double Sampling::Settings::holdSecondsFor (int note, double bpm) const
{
    auto ms = durationMs;
    auto bars = durationBars;
    auto how = durationMode;

    if (const auto found = overrides.find (note); found != overrides.end() && found->second.customDuration)
    {
        ms = found->second.durationMs;
        bars = found->second.durationBars;
        how = found->second.durationMode;
    }

    if (how == DurationMode::bars)
    {
        const auto beat = 60.0 / juce::jmax (1.0, bpm);
        return static_cast<double> (bars) * beat * timeSigNumerator * (4.0 / juce::jmax (1, timeSigDenominator));
    }

    return static_cast<double> (ms) / 1000.0;
}

double Sampling::Settings::outputRate() const
{
    return sampleRateChoice > 0 ? static_cast<double> (sampleRateChoice) : (sampleRate > 0.0 ? sampleRate : 48000.0);
}

//==============================================================================
std::vector<Sampling::KeyZone> Sampling::zonesFor (const Settings& settings)
{
    std::vector<KeyZone> zones;

    if (settings.keys.empty())
        return zones;

    const std::vector<int> sorted (settings.keys.begin(), settings.keys.end());

    if (settings.noKeyRanges)
    {
        for (const auto key : sorted)
            zones.push_back ({ key, key, key });

        return zones;
    }

    // Sampler Arena's: from 3 below each key up to the next zone; the lowest from its own key
    // and the highest to its own key (or the keyboard's ends, filling the gap).
    for (size_t i = 0; i < sorted.size(); ++i)
    {
        KeyZone zone;
        zone.root = sorted[i];
        zone.low = i == 0 ? (settings.fillGap ? 0 : sorted.front()) : sorted[i] - 3;
        zone.high = i + 1 == sorted.size() ? (settings.fillGap ? 127 : sorted.back()) : sorted[i + 1] - 3 - 1;
        zone.low = juce::jmin (juce::jlimit (0, 127, zone.low), zone.root);
        zone.high = juce::jmax (juce::jlimit (0, 127, zone.high), zone.root);
        zones.push_back (zone);
    }

    // The yellow locators: 0 the first zone's low key, -1 the last's high key, n zone n's low
    // key (and so zone n-1's high key).
    for (const auto& [index, key] : settings.boundaries)
    {
        if (index > 0 && index < static_cast<int> (zones.size()))
        {
            zones[static_cast<size_t> (index)].low = key;
            zones[static_cast<size_t> (index) - 1].high = key - 1;
        }
        else if (index == 0)
        {
            zones.front().low = key;
        }
        else if (index == -1)
        {
            zones.back().high = key;
        }
    }

    for (auto& zone : zones)
    {
        zone.low = juce::jlimit (0, 127, zone.low);
        zone.high = juce::jlimit (zone.low, 127, zone.high);
    }

    return zones;
}

std::vector<Sampling::Shot> Sampling::plan (const Settings& settings)
{
    std::vector<Shot> shots;

    if (settings.mode == Mode::builder)
        return shots;

    const auto safe = Sfz::safeName (settings.family);
    const auto takes = settings.roundRobins >= 2 ? settings.roundRobins : 0;

    std::vector<std::pair<double, int>> passes;

    auto bpms = settings.bpmLayers;
    std::sort (bpms.begin(), bpms.end());
    bpms.erase (std::unique (bpms.begin(), bpms.end()), bpms.end());

    if (bpms.empty())
        passes.push_back ({ settings.songTempo, 0 });
    else
        for (const auto bpm : bpms)
            passes.push_back ({ static_cast<double> (bpm), bpm });

    const auto velocityOf = [&settings] (const std::vector<VelocityLayer>& layers, const VelocityLayer& layer)
    {
        const auto velocity = layers.size() == 1 && layer.low <= 1 && layer.high >= 127 ? settings.samplingVelocity
                                                                                         : layer.high - settings.velocityOffset;
        return juce::jlimit (1, 127, velocity);
    };

    const auto add = [&] (int note, const std::vector<int>& notes, const juce::String& suffix, int prefix)
    {
        const auto& layers = settings.layersFor (note);

        for (size_t l = 0; l < layers.size(); ++l)
        {
            for (const auto& [bpmForLength, bpm] : passes)
            {
                for (int take = takes > 0 ? 1 : 0; take <= takes; ++take)
                {
                    Shot shot;
                    shot.note = note;
                    shot.notes = notes;
                    shot.velocity = velocityOf (layers, layers[l]);
                    shot.layer = static_cast<int> (l);
                    shot.take = take;
                    shot.bpm = bpm;
                    shot.holdSeconds = settings.holdSecondsFor (note, bpmForLength);
                    shot.chordSuffix = suffix;
                    shot.prefix = prefix;
                    shots.push_back (shot);
                }
            }
        }
    };

    if (settings.mode == Mode::chord)
    {
        // No chord types picked: nothing to sample (the keys are VSTi Sampling's, not chords').
        auto low = settings.minChordKey, high = settings.maxChordKey;

        if (low > high)
            std::swap (low, high);

        for (const auto type : settings.chords)
        {
            const auto& chord = chordTypes()[static_cast<size_t> (type)];
            int prefix = 0;

            for (int root = low; root <= high; ++root, ++prefix)
            {
                std::vector<int> notes;
                bool fits = true;

                for (const auto interval : chord.intervals)
                {
                    const auto note = root + interval;

                    if (note < low || note > high)
                    {
                        fits = false;
                        break;
                    }

                    notes.push_back (note);
                }

                if (fits)
                    add (root, notes, chord.suffix, prefix);
            }
        }
    }
    else
    {
        for (const auto key : settings.keys)
            add (key, { key }, {}, -1);
    }

    // The names: Sampler Arena's, made unique (a BPM layer and a take say so; a custom name with
    // several samples gets their velocity).
    std::map<int, int> perKey;

    for (const auto& shot : shots)
        ++perKey[shot.note];

    std::set<juce::String> used;

    for (auto& shot : shots)
    {
        const auto custom = settings.customNames.count (shot.note) != 0 ? settings.customNames.at (shot.note) : juce::String();
        const auto bpmPart = shot.bpm > 0 ? "_B" + juce::String (shot.bpm) : juce::String();
        const auto takePart = shot.take > 0 ? "_RR" + juce::String (shot.take) : juce::String();
        juce::String base;

        if (shot.chordSuffix.isNotEmpty())
        {
            const auto letter = Sfz::keyName (shot.note).initialSectionContainingOnly ("ABCDEFGs");   // "Cs" (Sampler Arena's "C" for both)
            const auto familyPart = custom.isNotEmpty() ? Sfz::safeName (custom) : safe;
            base = familyPart + "_" + (shot.prefix >= 0 ? juce::String (shot.prefix) + "_" : juce::String()) + letter + "_" + shot.chordSuffix
                 + bpmPart + "_V" + juce::String (shot.velocity) + takePart;
        }
        else if (custom.isNotEmpty())
        {
            base = stripWav (Sfz::safeName (custom));

            if (perKey[shot.note] > 1)
                base << bpmPart << "_V" << shot.velocity << takePart;
        }
        else
        {
            base = safe + "_" + Sfz::keyName (shot.note) + bpmPart + "_V" + juce::String (shot.velocity) + takePart;
        }

        auto name = base + ".wav";

        for (int n = 2; used.count (name.toLowerCase()) != 0; ++n)
            name = base + "_" + juce::String (n) + ".wav";

        used.insert (name.toLowerCase());
        shot.file = name;
    }

    return shots;
}

juce::File Sampling::samplesFolderFor (const Settings& settings, const juce::File& folder)
{
    if (settings.createSfz || settings.mode == Mode::chord)
        return folder.getChildFile (Sfz::safeName (settings.family)).getChildFile ("samples");

    return folder;
}

juce::File Sampling::sfzFileFor (const Settings& settings, const juce::File& folder)
{
    if (! settings.createSfz || settings.mode == Mode::chord)
        return {};

    const auto safe = Sfz::safeName (settings.family);
    const auto base = settings.mode == Mode::vsti && ! settings.bpmLayers.empty() ? safe + "_B" + juce::String (settings.bpmLayers.front()) : safe;
    return folder.getChildFile (safe).getChildFile (base + ".sfz");
}

//==============================================================================
bool Sampling::loadSlot (juce::AudioFormatManager& formats, const BuilderSlot& slot, juce::AudioBuffer<float>& audio, double& rate)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (slot.file));

    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
        return false;

    rate = reader->sampleRate;
    const auto total = reader->lengthInSamples;
    const auto start = slot.clip ? juce::jlimit<juce::int64> (0, total, static_cast<juce::int64> (std::llround (slot.offset * rate))) : 0;
    auto length = slot.clip && slot.length > 0.0 ? static_cast<juce::int64> (std::llround (slot.length * rate)) : total;
    length = juce::jlimit<juce::int64> (0, juce::jmin<juce::int64> (total - start, static_cast<juce::int64> (rate * 600.0)), length);

    if (length <= 0)
        return false;

    const auto channels = static_cast<int> (juce::jlimit (1u, 2u, reader->numChannels));
    audio.setSize (channels, static_cast<int> (length), false, false, true);

    if (! reader->read (&audio, 0, static_cast<int> (length), start, true, channels > 1))
        return false;

    if (slot.clip)
    {
        audio.applyGain (juce::Decibels::decibelsToGain (static_cast<float> (slot.gainDb), -144.0f));
        const auto fadeIn = static_cast<int> (juce::jmin<juce::int64> (length, static_cast<juce::int64> (slot.fadeIn * rate)));
        const auto fadeOutLength = static_cast<int> (juce::jmin<juce::int64> (length, static_cast<juce::int64> (slot.fadeOut * rate)));

        if (fadeIn > 1)
            audio.applyGainRamp (0, fadeIn, 0.0f, 1.0f);

        if (fadeOutLength > 1)
            audio.applyGainRamp (static_cast<int> (length) - fadeOutLength, fadeOutLength, 1.0f, 0.0f);
    }

    return true;
}

void Sampling::convertRate (juce::AudioBuffer<float>& audio, int& numSamples, double fromRate, double toRate)
{
    if (fromRate <= 0.0 || toRate <= 0.0 || std::abs (fromRate - toRate) < 0.5 || numSamples <= 0)
        return;

    const auto ratio = fromRate / toRate;   // input samples per output sample
    const auto outSamples = static_cast<int> (std::floor (static_cast<double> (numSamples) / ratio));
    juce::AudioBuffer<float> converted (audio.getNumChannels(), juce::jmax (1, outSamples));

    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
    {
        // The input read a little past its end: silence, so the last outputs settle.
        std::vector<float> input (static_cast<size_t> (numSamples) + 256, 0.0f);
        std::copy (audio.getReadPointer (channel), audio.getReadPointer (channel) + numSamples, input.begin());
        juce::WindowedSincInterpolator interpolator;
        interpolator.process (ratio, input.data(), converted.getWritePointer (channel), outSamples);
    }

    audio = std::move (converted);
    numSamples = outSamples;
}

//==============================================================================
/** A synth's notes out of its MIDI port, a millisecond or so after the audio thread plays them. */
class Sampling::Session::MidiSender final : public juce::Thread
{
public:
    explicit MidiSender (juce::MidiOutput& out)
        : juce::Thread ("Sonify sampling MIDI"), output (out)
    {
    }

    ~MidiSender() override
    {
        stopThread (2000);
    }

    void push (juce::uint8 status, juce::uint8 data1, juce::uint8 data2) noexcept
    {
        const auto scope = fifo.write (1);

        if (scope.blockSize1 > 0)
            messages[static_cast<size_t> (scope.startIndex1)] = { status, data1, data2 };
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            send();
            wait (1);
        }

        send();
    }

private:
    void send()
    {
        while (fifo.getNumReady() > 0)
        {
            const auto scope = fifo.read (1);

            if (scope.blockSize1 > 0)
            {
                const auto& m = messages[static_cast<size_t> (scope.startIndex1)];
                output.sendMessageNow (juce::MidiMessage (m[0], m[1], m[2]));
            }
        }
    }

    juce::MidiOutput& output;
    juce::AbstractFifo fifo { 256 };
    std::array<std::array<juce::uint8, 3>, 256> messages {};
};

//==============================================================================
/** Turbo and Unleashed: the nodes rendered on a thread of their own, about 5
    times as fast as the music (Turbo, paced by the clock, so a sleep that runs long is made up)
    or as fast as it goes (Unleashed), never so far ahead that the keeper can't keep up. */
class Sampling::Session::Renderer final : public juce::Thread
{
public:
    Renderer (Session& owner)
        : juce::Thread ("Sonify sampling (fast)"), session (owner)
    {
    }

    ~Renderer() override
    {
        stopThread (5000);
    }

    void run() override;

private:
    Session& session;
};

//==============================================================================
/** Keeps each take as it comes (in a file of floats of its own), then makes the samples from
    them when asked. */
class Sampling::Session::Keeper final : public juce::Thread
{
public:
    explicit Keeper (Session& owner)
        : juce::Thread ("Sonify sampling"), session (owner)
    {
        chunks.resize (static_cast<size_t> (queueChunks));
        takes.resize (owner.shots.size());
        take.setSize (2, static_cast<int> (6.0 * owner.settings.sampleRate));
    }

    ~Keeper() override
    {
        stopThread (10000);
    }

    juce::AbstractFifo fifo { queueChunks };
    std::vector<Chunk> chunks;

    std::atomic<bool> finishAsked { false }, finished { false }, failed { false };
    std::atomic<int> wavsWritten { 0 };
    std::atomic<double> progress { 0.0 };
    juce::String failure;      // set before finished
    juce::String warning;      // set before finished: the takes recorded but lost (empty: none)
    juce::File sfz;            // set before finished (none without an SFZ)

    void run() override
    {
        while (! threadShouldExit())
        {
            const auto worked = drain();

            if (finishAsked.load() && ! finished.load() && fifo.getNumReady() == 0)
            {
                makeSamples();
                finished.store (true);
            }

            if (! worked)
                wait (3);
        }
    }

private:
    struct Take
    {
        juce::int64 frames = 0;
        int channels = 2;
        float peak = 0.0f;
    };

    bool drain()
    {
        bool worked = false;

        while (fifo.getNumReady() > 0 && ! threadShouldExit())
        {
            const auto scope = fifo.read (1);

            if (scope.blockSize1 > 0)
                handle (chunks[static_cast<size_t> (scope.startIndex1)]);

            worked = true;
        }

        return worked;
    }

    void handle (const Chunk& chunk)
    {
        switch (chunk.type)
        {
            case beginChunk:
                open = chunk.shot;
                frames = 0;
                break;

            case framesChunk:
                if (open >= 0 && open == chunk.shot)
                    append (chunk);
                break;

            case endChunk:
                if (open >= 0 && open == chunk.shot)
                    keep (chunk.shot);

                open = -1;
                break;

            case dropChunk:
            default:
                open = -1;
                break;
        }
    }

    void append (const Chunk& chunk)
    {
        const auto needed = frames + chunk.count;

        if (needed > take.getNumSamples())
            take.setSize (2, static_cast<int> (juce::jmax<juce::int64> (needed, take.getNumSamples() * 2)), true, false, true);

        take.copyFrom (0, static_cast<int> (frames), chunk.left, chunk.count);
        take.copyFrom (1, static_cast<int> (frames), chunk.right, chunk.count);
        frames = needed;
    }

    /** A take ended: kept as it came (its silence before the attack trimmed, with Trim), or
        left out when it made no sound. */
    void keep (int shot)
    {
        if (! juce::isPositiveAndBelow (shot, static_cast<int> (session.shots.size())))
            return;

        const auto& settings = session.settings;
        const auto rate = settings.sampleRate;
        auto* left = take.getWritePointer (0);
        auto* right = take.getWritePointer (1);
        const auto length = frames;
        const auto silence = juce::Decibels::decibelsToGain (settings.silenceDb);
        const auto loudest = juce::jmax (peakOf (left, static_cast<int> (length)), peakOf (right, static_cast<int> (length)));

        auto& kept = takes[static_cast<size_t> (shot)];
        const auto keptFile = session.takesFolder.getChildFile (juce::String (shot) + ".raw");

        const auto leaveOut = [&]
        {
            kept = {};
            keptFile.deleteFile();
            session.marks[static_cast<size_t> (shot)].fetch_or (Session::silent);
            session.marks[static_cast<size_t> (shot)].fetch_and (static_cast<std::uint8_t> (~Session::written));
        };

        if (loudest < silence || length <= 0)
        {
            leaveOut();
            return;
        }

        // Trim: from where it first gets louder than the silence (or, for a loud take, 60 dB
        // below its peak), with a millisecond kept before it, faded in.
        juce::int64 start = 0, onset = 0;

        if (settings.trim)
        {
            const auto level = juce::jmax (loudest * 0.001f, silence);

            while (onset < length && std::abs (left[onset]) <= level && std::abs (right[onset]) <= level)
                ++onset;

            start = juce::jmax<juce::int64> (0, onset - samplesFor (1.0, rate));
        }

        const auto count = length - start;

        if (count <= 0)
        {
            leaveOut();
            return;
        }

        left += start;
        right += start;

        for (juce::int64 i = 0; i < onset - start; ++i)
        {
            const auto gain = static_cast<float> (i) / static_cast<float> (onset - start);
            left[i] *= gain;
            right[i] *= gain;
        }

        // One channel stored when both are the same (a mono input heard on both sides).
        const bool mono = std::equal (left, left + count, right);
        kept.frames = count;
        kept.channels = mono ? 1 : 2;
        kept.peak = juce::jmax (peakOf (left, static_cast<int> (count)), mono ? 0.0f : peakOf (right, static_cast<int> (count)));

        juce::FileOutputStream out (keptFile);

        if (out.openedOk())
        {
            out.setPosition (0);
            out.truncate();
            out.write (left, static_cast<size_t> (count) * sizeof (float));

            if (! mono)
                out.write (right, static_cast<size_t> (count) * sizeof (float));

            out.flush();
        }

        if (! out.openedOk() || out.getStatus().failed())
        {
            kept = {};
            failure = "Sonify couldn't keep a take in " + session.takesFolder.getFullPathName() + " (is the disk full?).";
            return;
        }

        session.marks[static_cast<size_t> (shot)].fetch_and (static_cast<std::uint8_t> (~Session::silent));
        session.marks[static_cast<size_t> (shot)].fetch_or (Session::written);
    }

    struct Made
    {
        int shot = -1;
        juce::int64 loopStart = -1, loopEnd = -1;
        double crossfade = 0.0;     // seconds
    };

    /** Every take kept made into a WAV (its channels, its fade, its rate, its loop), then the
        SFZ when one's asked for. */
    void makeSamples()
    {
        const auto& settings = session.settings;
        const auto& shots = session.shots;
        std::vector<int> kept;
        float loudest = 0.0f;
        juce::StringArray lost;
        int lostCount = 0;

        for (size_t i = 0; i < shots.size(); ++i)
        {
            const auto mark = session.marks[i].load();

            if ((mark & Session::written) != 0 && (mark & (Session::skipped | Session::silent)) == 0 && takes[i].frames > 0)
            {
                kept.push_back (static_cast<int> (i));
                loudest = juce::jmax (loudest, takes[i].peak);
            }
            else if ((mark & Session::taken) != 0 && (mark & (Session::written | Session::skipped | Session::silent)) == 0)
            {
                // Recorded, but its take couldn't be kept (the disk): not recorded after all, and said.
                session.marks[i].fetch_and (static_cast<std::uint8_t> (~Session::taken));

                if (lostCount++ < 6)
                    lost.add (shots[i].file.upToLastOccurrenceOf (".", false, false));
            }
        }

        if (kept.empty())
        {
            fail (failure.isNotEmpty() ? failure : juce::String ("Nothing was recorded: no note made a sound."));
            return;
        }

        if (lostCount > 0)
            warning = juce::String (lostCount) + (lostCount == 1 ? " take couldn't be kept (is the disk full?) and is left out: "
                                                                 : " takes couldn't be kept (is the disk full?) and are left out: ")
                    + lost.joinIntoString (", ") + (lostCount > lost.size() ? "..." : ".");

        // Normalize: the loudest at -1 dB, the rest as much louder. Otherwise as recorded, only
        // kept from clipping (PCM).
        auto gain = 1.0f;

        if (loudest > 0.0f)
            gain = settings.normalize ? juce::Decibels::decibelsToGain (-1.0f) / loudest
                                      : (settings.bitDepth == 32 ? 1.0f : juce::jmin (1.0f, 0.999f / loudest));

        const auto samples = samplesFolderFor (settings, session.folder);

        if (! samples.createDirectory())
        {
            fail ("Sonify can't make the folder " + samples.getFullPathName() + ".");
            return;
        }

        const auto outRate = settings.outputRate();
        juce::AudioBuffer<float> audio;
        std::vector<Made> made;

        for (size_t k = 0; k < kept.size() && ! threadShouldExit(); ++k)
        {
            const auto index = kept[k];
            const auto& info = takes[static_cast<size_t> (index)];
            const auto& shot = shots[static_cast<size_t> (index)];
            audio.setSize (info.channels, static_cast<int> (info.frames), false, false, true);

            juce::FileInputStream in (session.takesFolder.getChildFile (juce::String (index) + ".raw"));
            bool read = in.openedOk();

            for (int channel = 0; read && channel < info.channels; ++channel)
                read = in.read (audio.getWritePointer (channel), static_cast<int> (info.frames * static_cast<juce::int64> (sizeof (float))))
                         == static_cast<int> (info.frames * static_cast<juce::int64> (sizeof (float)));

            if (! read)
            {
                fail ("Sonify couldn't read back a take from " + session.takesFolder.getFullPathName() + ".");
                return;
            }

            auto frameCount = static_cast<int> (info.frames);
            int channelsOut = 2;
            applyChannelMode (audio, frameCount, settings.channelMode, channelsOut);
            convertRate (audio, frameCount, settings.sampleRate, outRate);

            if (settings.fadeOut)
                fadeTail (audio, frameCount, outRate, settings.fadeMs);

            Made m;
            m.shot = index;
            const auto file = samples.getChildFile (shot.file);

            // Create Legato Loop: the finder's best, its .loop file beside the sample.
            if (settings.autoLoop)
            {
                juce::AudioBuffer<float> view (audio.getArrayOfWritePointers(), channelsOut, frameCount);
                const auto found = LoopFinder::analyze (view, outRate, 1, 100);

                if (found.success && ! found.candidates.empty())
                {
                    const auto& c = found.candidates.front();
                    m.loopStart = c.loopStart;
                    m.loopEnd = c.loopEnd;
                    m.crossfade = c.crossfadeSamples / outRate;
                    Sfz::writeLoopFile (file, c.loopStart, c.loopEnd, c.crossfadeSamples, c.score);
                }
                else
                {
                    Sfz::removeLoopFile (file);
                }
            }

            if (! Sfz::writeWav (file, audio, frameCount, outRate, shot.note, gain, settings.bitDepth, channelsOut, m.loopStart, m.loopEnd))
            {
                fail ("Sonify couldn't write " + shot.file + " in " + samples.getFullPathName() + " (is the disk full?).");
                return;
            }

            made.push_back (m);
            wavsWritten.store (static_cast<int> (k) + 1);
            progress.store (static_cast<double> (k + 1) / static_cast<double> (kept.size() + 1));
        }

        if (threadShouldExit())
            return;

        if (settings.createSfz && settings.mode != Mode::chord && ! writeSfz (made))
            return;

        session.takesFolder.deleteRecursively();
        progress.store (1.0);
    }

    /** Sampler Arena's SFZ: key by key over its zone (the key shift applied), its velocities from
        midpoint to midpoint, each with its loop; a velocity's takes in turn. One for each BPM
        layer when there are some. */
    bool writeSfz (const std::vector<Made>& made)
    {
        const auto& settings = session.settings;
        const auto& shots = session.shots;
        const auto safe = Sfz::safeName (settings.family);
        const auto folder = session.folder.getChildFile (safe);
        const auto zones = zonesFor (settings);
        std::map<int, KeyZone> zoneOf;

        for (const auto& zone : zones)
            zoneOf[zone.root] = zone;

        std::vector<int> bpms;

        for (const auto& m : made)
            bpms.push_back (shots[static_cast<size_t> (m.shot)].bpm);

        std::sort (bpms.begin(), bpms.end());
        bpms.erase (std::unique (bpms.begin(), bpms.end()), bpms.end());

        for (const auto bpm : bpms)
        {
            std::map<int, std::map<int, std::vector<const Made*>>> byKey;   // key -> velocity -> takes

            for (const auto& m : made)
                if (const auto& shot = shots[static_cast<size_t> (m.shot)]; shot.bpm == bpm)
                    byKey[shot.note][shot.velocity].push_back (&m);

            std::vector<Sfz::Region> regions;

            for (auto& [note, byVelocity] : byKey)
            {
                const auto found = zoneOf.find (note);
                const auto zone = found != zoneOf.end() ? found->second : KeyZone { note, note, note };
                const auto shift = settings.keyShiftFor (note);
                std::vector<int> velocities;

                for (const auto& [velocity, list] : byVelocity)
                    velocities.push_back (velocity);

                const auto ranges = Sfz::velocityRanges (velocities);

                for (size_t v = 0; v < velocities.size(); ++v)
                {
                    auto list = byVelocity[velocities[v]];
                    std::sort (list.begin(), list.end(), [&shots] (const Made* a, const Made* b)
                    {
                        return shots[static_cast<size_t> (a->shot)].take < shots[static_cast<size_t> (b->shot)].take;
                    });

                    for (size_t t = 0; t < list.size(); ++t)
                    {
                        const auto& shot = shots[static_cast<size_t> (list[t]->shot)];
                        Sfz::Region region;
                        region.sample = shot.file;
                        region.lokey = juce::jlimit (0, 127, zone.low + shift);
                        region.hikey = juce::jlimit (0, 127, zone.high + shift);
                        region.keycenter = juce::jlimit (0, 127, note + shift);
                        region.lovel = velocities.size() == 1 ? 0 : ranges[v].first;
                        region.hivel = velocities.size() == 1 ? 127 : ranges[v].second;
                        region.loopStart = list[t]->loopStart;
                        region.loopEnd = list[t]->loopEnd;
                        region.loopCrossfade = list[t]->crossfade;

                        if (list.size() > 1)
                        {
                            region.seqLength = static_cast<int> (list.size());
                            region.seqPosition = static_cast<int> (t) + 1;
                        }

                        regions.push_back (region);
                    }
                }
            }

            const auto file = folder.getChildFile ((bpm > 0 ? safe + "_B" + juce::String (bpm) : safe) + ".sfz");

            if (! Sfz::write (file, Sfz::text (settings.family, static_cast<int> (byKey.size()), regions)))
            {
                fail ("Sonify couldn't write " + file.getFullPathName() + ".");
                return false;
            }

            if (sfz == juce::File())
                sfz = file;
        }

        return true;
    }

    void fail (const juce::String& why)
    {
        failure = why;
        failed.store (true);
    }

    Session& session;
    juce::AudioBuffer<float> take;
    juce::int64 frames = 0;
    int open = -1;
    std::vector<Take> takes;
};

//==============================================================================
void Sampling::Session::Renderer::run()
{
    const auto block = juce::jmax (32, session.engine.getBlockSize());
    const auto rate = session.settings.sampleRate > 0.0 ? session.settings.sampleRate : 48000.0;
    const auto turbo = session.settings.speed == Speed::turbo;
    const auto started = juce::Time::getMillisecondCounterHiRes();
    juce::int64 rendered = 0;

    while (! threadShouldExit())
    {
        // Nothing to do (stopped, or every shot in): resting.
        if (session.audioStopped.load() || session.audioDone.load())
        {
            wait (5);
            continue;
        }

        if (session.keeper != nullptr && session.keeper->fifo.getFreeSpace() < queueChunks / 4)
        {
            wait (2);
            continue;
        }

        if (! session.engine.renderSampling (block))
        {
            wait (5);
            continue;
        }

        rendered += block;

        if (turbo)
        {
            const auto due = static_cast<double> (rendered) / rate * 1000.0 / 5.0;

            while (! threadShouldExit() && juce::Time::getMillisecondCounterHiRes() - started < due)
                wait (1);
        }
        else
        {
            // A moment between blocks (about 50 microseconds) for the device's callback, which
            // waits on the same engine lock: straight back after a yield, it would rarely get in.
            const auto rested = juce::Time::getMillisecondCounterHiRes();
            juce::Thread::yield();

            while (! threadShouldExit() && juce::Time::getMillisecondCounterHiRes() - rested < 0.05)
                juce::Thread::yield();
        }
    }
}

//==============================================================================
Sampling::Session::Session (AudioEngine& audioEngine, const Settings& s, const juce::File& outputFolder)
    : engine (audioEngine),
      settings (s),
      shots (plan (s)),
      folder (outputFolder),
      takesFolder (outputFolder.getChildFile ("." + Sfz::safeName (s.family) + " takes")),
      offline (s.speed != Speed::realTime && s.source == Source::nodes),
      marks (std::make_unique<std::atomic<std::uint8_t>[]> (juce::jmax<size_t> (1, shots.size())))
{
    for (size_t i = 0; i < juce::jmax<size_t> (1, shots.size()); ++i)
        marks[i].store (0);

    const auto rate = settings.sampleRate > 0.0 ? settings.sampleRate : 48000.0;
    silenceSamples = juce::jmax<juce::int64> (1, samplesFor (settings.silenceMs, rate));
    settleSamples = samplesFor (100.0, rate);
    maxSettleSamples = samplesFor (3000.0, rate);
    maxTailSamples = samplesFor (Settings::maxTailSeconds * 1000.0, rate);
    armSamples = samplesFor (250.0, rate);
    minRecordSamples = samplesFor (50.0, rate);
    silenceGain = juce::Decibels::decibelsToGain (settings.silenceDb);
    triggerGain = juce::Decibels::decibelsToGain (juce::jmax (settings.triggerDb, settings.silenceDb + 6.0f));

    preRollFrames = static_cast<int> (juce::jmin<juce::int64> (preRollCapacity, samplesFor (100.0, rate)));
    preRollLeft.assign (static_cast<size_t> (preRollCapacity), 0.0f);
    preRollRight.assign (static_cast<size_t> (preRollCapacity), 0.0f);
}

Sampling::Session::~Session()
{
    detach();

    // A synth left with its notes held.
    if (sender != nullptr)
    {
        sender->stopThread (2000);

        if (midiOut != nullptr)
        {
            for (int i = 0; i < heldCount; ++i)
                midiOut->sendMessageNow (juce::MidiMessage::noteOff (settings.midiChannel, heldNotes[static_cast<size_t> (i)]));

            midiOut->sendMessageNow (juce::MidiMessage::allNotesOff (settings.midiChannel));
        }
    }

    sender = nullptr;
    keeper = nullptr;
    midiOut = nullptr;

    if (state.load() != State::done)
        takesFolder.deleteRecursively();
}

juce::File Sampling::Session::getSfzFile() const
{
    if (keeper != nullptr && keeper->finished.load() && keeper->sfz != juce::File())
        return keeper->sfz;

    return sfzFileFor (settings, folder);
}

//==============================================================================
juce::Result Sampling::Session::start()
{
    if (state.load() != State::ready)
        return juce::Result::fail ("It's already started.");

    if (shots.empty())
        return juce::Result::fail (settings.mode == Mode::chord ? "No chord fits: pick chord types (Chord Types...) and a key range wide enough for them (Settings)."
                                                                : "Select keys on the keyboard first.");

    takesFolder.deleteRecursively();

    if (! takesFolder.createDirectory())
        return juce::Result::fail ("Sonify can't make a folder for the takes in " + folder.getFullPathName() + ".");

    if (settings.source == Source::hardware)
    {
        midiOut = juce::MidiOutput::openDevice (settings.midiOutput);

        if (midiOut == nullptr)
        {
            takesFolder.deleteRecursively();
            return juce::Result::fail ("The MIDI output to the synth can't be opened: another app (or Sonify's MIDI clock) may be using it.");
        }

        sender = std::make_unique<MidiSender> (*midiOut);
        sender->startThread (juce::Thread::Priority::high);
    }

    keeper = std::make_unique<Keeper> (*this);
    keeper->startThread();

    resetAudioState (firstOpenShot (0));
    attach();
    state.store (State::running);
    return juce::Result::ok();
}

void Sampling::Session::stop()
{
    if (state.load() != State::running)
        return;

    command.store (stopCommand);
    stopAskedMs = juce::Time::getMillisecondCounterHiRes();
    state.store (State::stopping);
}

void Sampling::Session::resume()
{
    if (state.load() != State::stopped)
        return;

    const auto from = firstOpenShot (0);

    if (from < 0)
    {
        finish();
        return;
    }

    resetAudioState (from);
    attach();
    state.store (State::running);
}

void Sampling::Session::finish()
{
    const auto now = state.load();

    if (now == State::running)
    {
        stop();
        finishWanted = true;
        return;
    }

    if (now == State::stopping)
    {
        finishWanted = true;
        return;
    }

    if (now == State::stopped && keeper != nullptr)
    {
        keeper->finishAsked.store (true);
        state.store (State::finishing);
    }
}

void Sampling::Session::skip()
{
    if (state.load() == State::running && settings.source == Source::voice)
        command.store (skipCommand);
}

void Sampling::Session::redo()
{
    if (state.load() == State::running && settings.source == Source::voice)
        command.store (redoCommand);
}

void Sampling::Session::update()
{
    switch (state.load())
    {
        case State::running:
        {
            // The nodes are the rack's (no track): only a synth's or the user's track can go.
            if (settings.source != Source::nodes && engine.findTrack (settings.trackId) == nullptr)
            {
                problem = "The track being sampled is gone: sampling stopped.";
                stop();
                break;
            }

            if (audioDone.load())
            {
                detach();

                if (keeper != nullptr)
                    keeper->finishAsked.store (true);

                state.store (State::finishing);
            }

            break;
        }

        case State::stopping:
        {
            const auto waited = juce::Time::getMillisecondCounterHiRes() - stopAskedMs;

            if (audioStopped.load() || (! offline && ! engine.isAudioRunning()) || waited > 1000.0)
            {
                detach();

                // The audio thread never heard (no audio): its take and its notes are let go here.
                if (! audioStopped.load())
                {
                    if (takeOpen)
                        pushMarker (dropChunk, current, 0);

                    if (sender != nullptr)
                        for (int i = 0; i < heldCount; ++i)
                            sender->push (static_cast<juce::uint8> (0x80 | (settings.midiChannel - 1)), static_cast<juce::uint8> (heldNotes[static_cast<size_t> (i)]), 0);

                    takeOpen = false;
                    heldCount = 0;
                    step = Step::idle;
                }

                state.store (State::stopped);

                if (std::exchange (finishWanted, false))
                    finish();
            }

            break;
        }

        case State::finishing:
        {
            if (keeper != nullptr && keeper->finished.load())
            {
                if (keeper->failed.load())
                {
                    problem = keeper->failure;
                    state.store (State::failed);
                }
                else
                {
                    problem = keeper->warning;   // the takes lost on the way, when there were some
                    state.store (State::done);
                }
            }

            break;
        }

        case State::ready:
        case State::stopped:
        case State::done:
        case State::failed:
            break;
    }
}

Sampling::Session::Status Sampling::Session::getStatus()
{
    Status status;
    status.state = state.load();
    status.step = static_cast<Step> (shownStep.load());
    status.shot = shownShot.load();
    status.total = static_cast<int> (shots.size());
    status.left = peakLeft.exchange (0.0f);
    status.right = peakRight.exchange (0.0f);
    status.message = problem;

    for (size_t i = 0; i < shots.size(); ++i)
    {
        const auto mark = marks[i].load();

        if ((mark & (taken | skipped)) != 0)
            ++status.recorded;

        if ((mark & silent) != 0)
            ++status.silent;
    }

    if (keeper != nullptr)
    {
        status.written = keeper->wavsWritten.load();
        status.progress = keeper->progress.load();
    }

    if (status.state != State::running && status.state != State::stopping)
        status.step = Step::idle;

    return status;
}

bool Sampling::Session::isRecorded (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, static_cast<int> (shots.size()))
        && (marks[static_cast<size_t> (index)].load() & taken) != 0;
}

bool Sampling::Session::isLeftOut (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, static_cast<int> (shots.size()))
        && (marks[static_cast<size_t> (index)].load() & (skipped | silent)) != 0;
}

//==============================================================================
void Sampling::Session::attach()
{
    if (attached)
        return;

    audioStopped.store (false);
    engine.setSamplingTap (this, settings.source == Source::nodes ? AudioEngine::rackTrackId : settings.trackId, offline);
    attached = true;

    if (offline)
    {
        renderer = std::make_unique<Renderer> (*this);
        renderer->startThread (juce::Thread::Priority::high);
    }
}

void Sampling::Session::detach()
{
    if (! attached)
        return;

    renderer = nullptr;   // its thread ends before the tap goes
    engine.setSamplingTap (nullptr, 0);
    attached = false;
}

int Sampling::Session::firstOpenShot (int from) const noexcept
{
    const auto count = static_cast<int> (shots.size());

    for (int n = 0; n < count; ++n)
    {
        const auto index = (juce::jmax (0, from) + n) % count;

        if ((marks[static_cast<size_t> (index)].load() & (taken | skipped)) == 0)
            return index;
    }

    return -1;
}

void Sampling::Session::resetAudioState (int from)
{
    command.store (noCommand);
    current = from;
    lastEnded = -1;
    step = from < 0 ? Step::idle : settings.source == Source::voice ? Step::waiting : Step::settling;
    stepSamples = quietSamples = tailSamples = takeFrames = 0;
    heldCount = 0;
    takeOpen = takeBroken = armed = heardSound = false;
    preRollAt = preRollFilled = 0;
    tempoNow.store (0.0);
    audioStopped.store (false);
    audioDone.store (from < 0);
    shownShot.store (from);
    shownStep.store (static_cast<int> (step));
}

//==============================================================================
// The audio thread (or Turbo's).

double Sampling::Session::tapTempo() const noexcept
{
    return tempoNow.load (std::memory_order_relaxed);
}

void Sampling::Session::tapNotes (juce::MidiBuffer& into, int numSamples) noexcept
{
    switch (command.exchange (noCommand))
    {
        case stopCommand:
            notesOff (into, 0);
            dropTake();
            step = Step::idle;
            audioStopped.store (true);
            shownStep.store (static_cast<int> (step));
            return;

        case skipCommand:
            if (current >= 0 && step != Step::idle)
            {
                dropTake();
                marks[static_cast<size_t> (current)].fetch_or (skipped);
                nextShot (current + 1);
            }
            break;

        case redoCommand:
            if (lastEnded >= 0 && step != Step::idle)
            {
                dropTake();
                marks[static_cast<size_t> (lastEnded)].fetch_and (static_cast<std::uint8_t> (~(taken | written | silent)));
                const auto again = std::exchange (lastEnded, -1);
                nextShot (again);
            }
            break;

        default:
            break;
    }

    if (step == Step::settling)
    {
        if (quietSamples >= settleSamples || stepSamples >= maxSettleSamples)
            beginShot (into);
    }
    else if (step == Step::playing)
    {
        // Sampler Arena's: let go once held long enough, and at least 50 ms are recorded.
        if (stepSamples + numSamples >= holdSamples && takeFrames >= minRecordSamples)
        {
            const auto offset = static_cast<int> (juce::jlimit<juce::int64> (0, juce::jmax (0, numSamples - 1), holdSamples - stepSamples));
            notesOff (into, offset);
            tailSamples = -offset;
            quietSamples = 0;
            step = Step::releasing;
        }
    }

    shownShot.store (current);
    shownStep.store (static_cast<int> (step));
}

void Sampling::Session::tapSound (const float* left, const float* right, int numSamples) noexcept
{
    if (numSamples <= 0 || left == nullptr)
        return;

    if (right == nullptr)
        right = left;

    const auto peakLeftNow = peakOf (left, numSamples);
    const auto peakRightNow = right == left ? peakLeftNow : peakOf (right, numSamples);
    storeMax (peakLeft, peakLeftNow);
    storeMax (peakRight, peakRightNow);

    const auto blockPeak = juce::jmax (peakLeftNow, peakRightNow);
    const bool quiet = rmsOf (left, right, numSamples) < silenceGain;   // Sampler Arena: the block's RMS under the threshold

    switch (step)
    {
        case Step::settling:
            quietSamples = quiet ? quietSamples + numSamples : 0;
            stepSamples += numSamples;
            break;

        case Step::playing:
            pushFrames (left, right, numSamples);
            stepSamples += numSamples;
            heardSound = heardSound || ! quiet;
            break;

        case Step::releasing:
        {
            pushFrames (left, right, numSamples);
            tailSamples += numSamples;
            heardSound = heardSound || ! quiet;
            quietSamples = quiet ? quietSamples + numSamples : 0;

            // Until it's been quiet for the silence duration (once it made a sound; at most 30 s).
            if ((quietSamples >= silenceSamples && (heardSound || tailSamples >= silenceSamples * 4)) || tailSamples >= maxTailSamples)
            {
                const auto from = current;
                endTake();
                nextShot (takeBroken ? from : from + 1);
            }

            break;
        }

        case Step::waiting:
        {
            if (armed && blockPeak >= triggerGain)
            {
                // The note starts: the moments before it first, then this block.
                pushMarker (beginChunk, current, 0);
                takeOpen = true;
                takeBroken = false;
                takeFrames = 0;

                const auto kept = juce::jmin (preRollFilled, preRollFrames);
                auto at = (preRollAt - kept + preRollCapacity) % preRollCapacity;

                for (int remaining = kept; remaining > 0;)
                {
                    const auto run = juce::jmin (remaining, preRollCapacity - at);
                    pushFrames (preRollLeft.data() + at, preRollRight.data() + at, run);
                    at = (at + run) % preRollCapacity;
                    remaining -= run;
                }

                pushFrames (left, right, numSamples);
                step = Step::recording;
                stepSamples = takeFrames;
                quietSamples = 0;
                break;
            }

            if (! armed)
            {
                quietSamples = quiet ? quietSamples + numSamples : 0;
                armed = quietSamples >= armSamples;
            }

            for (int i = 0; i < numSamples; ++i)
            {
                preRollLeft[static_cast<size_t> (preRollAt)] = left[i];
                preRollRight[static_cast<size_t> (preRollAt)] = right[i];
                preRollAt = (preRollAt + 1) % preRollCapacity;
            }

            preRollFilled = juce::jmin (preRollCapacity, preRollFilled + numSamples);
            break;
        }

        case Step::recording:
        {
            pushFrames (left, right, numSamples);
            stepSamples += numSamples;
            quietSamples = quiet ? quietSamples + numSamples : 0;

            if (quietSamples >= silenceSamples || stepSamples >= maxTailSamples)
            {
                const auto from = current;
                endTake();
                nextShot (takeBroken ? from : from + 1);
            }

            break;
        }

        case Step::idle:
            break;
    }

    shownShot.store (current);
    shownStep.store (static_cast<int> (step));
}

void Sampling::Session::notesOn (juce::MidiBuffer& into, const Shot& shot, int offset) noexcept
{
    heldCount = 0;

    for (const auto note : shot.notes)
    {
        if (heldCount >= static_cast<int> (heldNotes.size()))
            break;

        heldNotes[static_cast<size_t> (heldCount++)] = note;
        const auto velocity = static_cast<juce::uint8> (juce::jlimit (1, 127, shot.velocity));

        if (settings.source == Source::hardware)
        {
            if (sender != nullptr)
                sender->push (static_cast<juce::uint8> (0x90 | (settings.midiChannel - 1)), static_cast<juce::uint8> (note), velocity);
        }
        else if (settings.source == Source::nodes)
        {
            into.addEvent (juce::MidiMessage::noteOn (1, note, velocity), offset);
        }
    }
}

void Sampling::Session::notesOff (juce::MidiBuffer& into, int offset) noexcept
{
    for (int i = 0; i < heldCount; ++i)
    {
        const auto note = heldNotes[static_cast<size_t> (i)];

        if (settings.source == Source::hardware)
        {
            if (sender != nullptr)
                sender->push (static_cast<juce::uint8> (0x80 | (settings.midiChannel - 1)), static_cast<juce::uint8> (note), 0);
        }
        else if (settings.source == Source::nodes)
        {
            into.addEvent (juce::MidiMessage::noteOff (1, note), offset);
        }
    }

    heldCount = 0;
}

void Sampling::Session::beginShot (juce::MidiBuffer& into) noexcept
{
    if (! juce::isPositiveAndBelow (current, static_cast<int> (shots.size())))
    {
        step = Step::idle;
        return;
    }

    const auto& shot = shots[static_cast<size_t> (current)];

    if (! pushMarker (beginChunk, current, 0))
        return;   // no room yet: the next block tries again

    // A BPM layer: the track hears its tempo while it plays.
    tempoNow.store (static_cast<double> (shot.bpm));
    takeOpen = true;
    takeBroken = false;
    takeFrames = 0;
    heardSound = false;
    holdSamples = juce::jmax<juce::int64> (1, static_cast<juce::int64> (std::llround (shot.holdSeconds * settings.sampleRate)));
    notesOn (into, shot, 0);
    step = Step::playing;
    stepSamples = 0;
    quietSamples = 0;
}

void Sampling::Session::endTake() noexcept
{
    if (! takeOpen)
        return;

    takeOpen = false;

    if (! takeBroken && pushMarker (endChunk, current, 0))
    {
        marks[static_cast<size_t> (current)].fetch_or (taken);
        lastEnded = current;
        return;
    }

    // Some of it was lost (the keeper fell far behind): it's played again.
    takeBroken = true;
    pushMarker (dropChunk, current, 0);
}

void Sampling::Session::dropTake() noexcept
{
    if (takeOpen)
        pushMarker (dropChunk, current, 0);

    takeOpen = false;
}

void Sampling::Session::nextShot (int from) noexcept
{
    current = firstOpenShot (from);
    stepSamples = quietSamples = tailSamples = 0;
    armed = false;
    takeBroken = false;

    if (current < 0)
    {
        step = Step::idle;
        tempoNow.store (0.0);
        audioDone.store (true);
        return;
    }

    step = settings.source == Source::voice ? Step::waiting : Step::settling;
}

void Sampling::Session::pushFrames (const float* left, const float* right, int numSamples) noexcept
{
    if (! takeOpen || takeBroken || keeper == nullptr)
        return;

    for (int done = 0; done < numSamples;)
    {
        if (keeper->fifo.getFreeSpace() <= reservedChunks)
        {
            takeBroken = true;
            return;
        }

        const auto count = juce::jmin (chunkFrames, numSamples - done);
        const auto scope = keeper->fifo.write (1);

        if (scope.blockSize1 <= 0)
        {
            takeBroken = true;
            return;
        }

        auto& chunk = keeper->chunks[static_cast<size_t> (scope.startIndex1)];
        chunk.type = framesChunk;
        chunk.shot = current;
        chunk.value = 0;
        chunk.count = count;
        std::copy (left + done, left + done + count, chunk.left);
        std::copy (right + done, right + done + count, chunk.right);
        done += count;
    }

    takeFrames += numSamples;
}

bool Sampling::Session::pushMarker (int type, int shot, int value) noexcept
{
    if (keeper == nullptr)
        return false;

    const auto scope = keeper->fifo.write (1);

    if (scope.blockSize1 <= 0)
        return false;

    auto& chunk = keeper->chunks[static_cast<size_t> (scope.startIndex1)];
    chunk.type = type;
    chunk.shot = shot;
    chunk.value = value;
    chunk.count = 0;
    return true;
}

//==============================================================================
// The Sample Builder.

Sampling::Build::Build (const Settings& s, const juce::File& outputFolder)
    : juce::Thread ("Sonify sample builder"), settings (s), folder (outputFolder)
{
}

Sampling::Build::~Build()
{
    stopThread (10000);
}

juce::String Sampling::Build::getProblem() const
{
    return done.load() ? problem : juce::String();
}

juce::File Sampling::Build::getSfzFile() const
{
    return sfzFileFor (settings, folder);
}

void Sampling::Build::run()
{
    const auto finishWith = [this] (const juce::String& why)
    {
        problem = why;
        done.store (true);
    };

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    struct Item
    {
        const BuilderSlot* slot = nullptr;
        int note = 60, take = 0, takes = 0;
        juce::String file;
    };

    // Each sample under its own name (its file's, as Sampler Arena); slots of a key with the same
    // range take turns (round robins).
    std::vector<Item> items;
    std::set<juce::String> used;

    for (const auto& [note, list] : settings.slots)
    {
        // A key that isn't picked any more keeps its slots, but isn't built (Sampler Arena built
        // every key's slots, picked or not).
        if (settings.keys.count (note) == 0)
            continue;

        std::map<std::pair<int, int>, int> counts, seen;

        for (const auto& slot : list)
            ++counts[{ slot.lovel, slot.hivel }];

        for (const auto& slot : list)
        {
            Item item;
            item.slot = &slot;
            item.note = note;
            item.takes = counts[{ slot.lovel, slot.hivel }];
            item.take = item.takes > 1 ? ++seen[{ slot.lovel, slot.hivel }] : 0;

            const auto base = stripWav (Sfz::safeName (slot.name.isNotEmpty() ? slot.name : slot.file.getFileNameWithoutExtension()));
            auto name = base + ".wav";

            for (int n = 2; used.count (name.toLowerCase()) != 0; ++n)
                name = base + "_" + juce::String (n) + ".wav";

            used.insert (name.toLowerCase());
            item.file = name;
            items.push_back (item);
        }
    }

    if (items.empty())
    {
        finishWith ("No sample slots loaded. Use the E button on each key to load files.");
        return;
    }

    auto gain = 1.0f;
    juce::AudioBuffer<float> audio;
    double rate = 0.0;

    if (settings.normalize)
    {
        float loudest = 0.0f;

        for (const auto& item : items)
        {
            if (threadShouldExit())
                return;

            if (loadSlot (formats, *item.slot, audio, rate))
                loudest = juce::jmax (loudest, audio.getMagnitude (0, audio.getNumSamples()));
        }

        if (loudest > 0.0f)
            gain = juce::Decibels::decibelsToGain (-1.0f) / loudest;
    }

    const auto samples = samplesFolderFor (settings, folder);

    if (! samples.createDirectory())
    {
        finishWith ("Sonify can't make the folder " + samples.getFullPathName() + ".");
        return;
    }

    std::vector<Sfz::Region> regions;

    for (size_t i = 0; i < items.size(); ++i)
    {
        if (threadShouldExit())
            return;

        const auto& item = items[i];

        if (! loadSlot (formats, *item.slot, audio, rate))
        {
            finishWith (item.slot->file.getFileName() + " can't be read as audio.");
            return;
        }

        auto frames = audio.getNumSamples();
        const auto outRate = settings.sampleRateChoice > 0 ? static_cast<double> (settings.sampleRateChoice) : rate;
        convertRate (audio, frames, rate, outRate);

        const auto file = samples.getChildFile (item.file);
        Sfz::Region region;
        region.sample = item.file;
        const auto shift = settings.keyShiftFor (item.note);
        region.lokey = region.hikey = region.keycenter = juce::jlimit (0, 127, item.note + shift);
        region.lovel = item.slot->lovel;
        region.hivel = item.slot->hivel;

        if (item.takes > 1)
        {
            region.seqLength = item.takes;
            region.seqPosition = item.take;
        }

        if (settings.autoLoop)
        {
            juce::AudioBuffer<float> view (audio.getArrayOfWritePointers(), audio.getNumChannels(), frames);
            const auto found = LoopFinder::analyze (view, outRate, 1, 100);

            if (found.success && ! found.candidates.empty())
            {
                const auto& c = found.candidates.front();
                region.loopStart = c.loopStart;
                region.loopEnd = c.loopEnd;
                region.loopCrossfade = c.crossfadeSamples / outRate;
                Sfz::writeLoopFile (file, c.loopStart, c.loopEnd, c.crossfadeSamples, c.score);
            }
        }

        if (! Sfz::writeWav (file, audio, frames, outRate, item.note, gain, settings.bitDepth, audio.getNumChannels(), region.loopStart, region.loopEnd))
        {
            finishWith ("Failed to copy " + item.slot->file.getFileName() + " to output folder.");
            return;
        }

        regions.push_back (region);
        written.store (static_cast<int> (i) + 1);
        progress.store (static_cast<double> (i + 1) / static_cast<double> (items.size() + 1));
    }

    if (settings.createSfz)
    {
        const auto file = getSfzFile();

        if (! Sfz::write (file, Sfz::builderText (settings.family, "samples\\", regions)))
        {
            finishWith ("Sonify couldn't write " + file.getFullPathName() + ".");
            return;
        }
    }

    progress.store (1.0);
    finishWith ({});
}
