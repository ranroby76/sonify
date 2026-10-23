// D:\workspace\Fanan_web_hub\src\SonifySampler.cpp

#include "SonifySampler.h"
#include "BlobMaker.h"
#include "BuiltinPlugins.h"

#include <juce_data_structures/juce_data_structures.h>

#include <algorithm>
#include <cmath>
#include <map>

namespace
{
    std::function<juce::File()>& userFolder()
    {
        static std::function<juce::File()> folder;
        return folder;
    }

    Samplers::ScreenFactory& screenFactory()
    {
        static Samplers::ScreenFactory factory;
        return factory;
    }

    /** A parameter: its id, name, Sampler Arena's name for it (in .sfzp presets), range and default. */
    struct Spec
    {
        uint32_t id;
        const char* name;
        const char* arenaName;
        double minValue, maxValue, defaultValue;
        bool stepped = false;
    };

    using P = SonifySampler;

    const Spec specs[]
    {
        { P::volume,         "Volume",           "masterVolume",   0.0,  2.0,     1.0 },
        { P::octave,         "Octave",           "octaveOffset",  -3.0,  3.0,     0.0, true },
        { P::playMode,       "Play Mode",        "playMode",       0.0,  1.0,     0.0, true },
        { P::monoMode,       "Mono Mode",        nullptr,          0.0,  2.0,     0.0, true },
        { P::glide,          "Glide",            "portamentoTime", 0.0,  2000.0,  100.0 },
        { P::bendRange,      "Bend Range",       "pitchBendRange", 0.0,  12.0,    2.0, true },
        { P::ampAttack,      "Attack",           "ampAttack",      0.1,  5000.0,  1.0 },
        { P::ampHold,        "Hold",             "ampHold",        0.0,  2000.0,  0.0 },
        { P::ampDecay,       "Decay",            "ampDecay",       0.1,  5000.0,  200.0 },
        { P::ampSustain,     "Sustain",          "ampSustain",     0.0,  1.0,     1.0 },
        { P::ampRelease,     "Release",          "ampRelease",     0.1,  10000.0, 300.0 },
        { P::filterType,     "Filter Type",      "filterType",     0.0,  3.0,     0.0, true },
        { P::cutoff,         "Cutoff",           "filterCutoff",   20.0, 20000.0, 20000.0 },
        { P::resonance,      "Resonance",        "filterReso",     0.0,  0.99,    0.0 },
        { P::filterKeytrack, "Key Track",        "filterKeytrack", 0.0,  100.0,   0.0 },
        { P::filterAttack,   "Filter Attack",    "filtAttack",     0.1,  5000.0,  10.0 },
        { P::filterHold,     "Filter Hold",      "filtHold",       0.0,  2000.0,  0.0 },
        { P::filterDecay,    "Filter Decay",     "filtDecay",      0.1,  5000.0,  200.0 },
        { P::filterSustain,  "Filter Sustain",   "filtSustain",    0.0,  1.0,     1.0 },
        { P::filterRelease,  "Filter Release",   "filtRelease",    0.1,  10000.0, 300.0 },
        { P::filterAmount,   "Filter Env",       "filtEnvAmount", -1.0,  1.0,     0.0 },
        { P::pitchAttack,    "Pitch Attack",     "pitchAttack",    0.0,  5000.0,  0.0 },
        { P::pitchHold,      "Pitch Hold",       "pitchHold",      0.0,  2000.0,  0.0 },
        { P::pitchDecay,     "Pitch Decay",      "pitchDecay",     0.0,  5000.0,  0.0 },
        { P::pitchSustain,   "Pitch Sustain",    "pitchSustain",   0.0,  1.0,     0.0 },
        { P::pitchRelease,   "Pitch Release",    "pitchRelease",   0.0,  10000.0, 0.0 },
        { P::pitchDepth,     "Pitch Env",        "pitchEnvDepth", -24.0, 24.0,    0.0 },
        { P::pitchKeytrack,  "Pitch Key Track",  "pitchKeytrack",  0.0,  200.0,   100.0 },
        { P::pitchVeltrack,  "Pitch Vel Track",  "pitchVeltrack", -100.0, 100.0,  0.0 },
        { P::ampLfoRate,     "Tremolo Rate",     "ampLfoRate",     0.0,  20.0,    0.0 },
        { P::ampLfoDepth,    "Tremolo Depth",    "ampLfoDepth",    0.0,  1.0,     0.0 },
        { P::ampLfoDelay,    "Tremolo Delay",    "ampLfoDelay",    0.0,  5000.0,  0.0 },
        { P::filterLfoRate,  "Filter LFO Rate",  "filtLfoRate",    0.0,  20.0,    0.0 },
        { P::filterLfoDepth, "Filter LFO Depth", "filtLfoDepth",   0.0,  1.0,     0.0 },
        { P::filterLfoDelay, "Filter LFO Delay", "filtLfoDelay",   0.0,  5000.0,  0.0 },
        { P::pitchLfoRate,   "Vibrato Rate",     "pitchLfoRate",   0.0,  20.0,    0.0 },
        { P::pitchLfoDepth,  "Vibrato Depth",    "pitchLfoDepth",  0.0,  12.0,    0.0 },
        { P::pitchLfoDelay,  "Vibrato Delay",    "pitchLfoDelay",  0.0,  5000.0,  0.0 },
    };

    static_assert (std::size (specs) == static_cast<size_t> (P::numParams), "one spec per parameter");

    const Spec& specOf (uint32_t id) noexcept
    {
        return specs[juce::jlimit<size_t> (0, std::size (specs) - 1, static_cast<size_t> (id) - 1)];
    }

    //==============================================================================
    // Sampler Arena's envelope, filter and LFO.

    /** AHDSR: a straight attack (from where it is) and decay, an exponential release. */
    struct Envelope
    {
        enum Stage { idle, attack, hold, decay, sustain, release };
        Stage stage = idle;
        float level = 0.0f;
        float attackRate = 1.0f, holdSamples = 0.0f, decayRate = 1.0f, sustainLevel = 1.0f, releaseRate = 1.0f;
        int holdCounter = 0;

        void noteOn (float attackMs, float holdMs, float decayMs, float sustainAt, double rate) noexcept
        {
            sustainLevel = juce::jlimit (0.0f, 1.0f, sustainAt);
            attackRate = attackMs > 0.1f ? 1.0f / static_cast<float> (rate * attackMs / 1000.0) : 1.0f;
            holdSamples = static_cast<float> (rate * holdMs / 1000.0);
            decayRate = decayMs > 0.1f ? 1.0f / static_cast<float> (rate * decayMs / 1000.0) : 1.0f;
            holdCounter = 0;
            stage = attack;
        }

        void noteOff (float releaseMs, double rate) noexcept
        {
            releaseRate = releaseMs > 0.1f ? 1.0f / static_cast<float> (rate * releaseMs / 1000.0) : 1.0f;

            if (stage != idle)
                stage = release;
        }

        float process() noexcept
        {
            switch (stage)
            {
                case attack:
                    level += attackRate;

                    if (level >= 1.0f)
                    {
                        level = 1.0f;
                        stage = hold;
                        holdCounter = 0;
                    }

                    break;

                case hold:
                    if (static_cast<float> (++holdCounter) >= holdSamples)
                        stage = decay;
                    break;

                case decay:
                    level -= (1.0f - sustainLevel) * decayRate;

                    if (level <= sustainLevel)
                    {
                        level = sustainLevel;
                        stage = sustain;
                    }

                    break;

                case release:
                    level -= level * juce::jmin (1.0f, releaseRate);

                    if (level < 0.0001f)
                    {
                        level = 0.0f;
                        stage = idle;
                    }

                    break;

                case sustain:
                case idle:
                    break;
            }

            return level;
        }

        bool isActive() const noexcept     { return stage != idle; }
        bool isReleasing() const noexcept  { return stage == release || stage == idle; }
    };

    /** The state variable filter, its coefficients worked out a few times a millisecond. */
    struct Filter
    {
        float ic1 = 0.0f, ic2 = 0.0f;

        void reset() noexcept  { ic1 = ic2 = 0.0f; }

        float process (float input, float a1, float a2, float a3, float k, int type) noexcept
        {
            const auto v3 = input - ic2;
            const auto v1 = a1 * ic1 + a2 * v3;
            const auto v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;

            switch (type)
            {
                case 1:  return input - k * v1 - v2;   // high pass
                case 2:  return v1;                    // band pass
                case 3:  return input - k * v1;        // notch
                default: return v2;                    // low pass
            }
        }
    };

    /** A sine LFO after its delay. */
    struct Lfo
    {
        double phase = 0.0, increment = 0.0;
        juce::int64 delayLeft = 0;

        void reset (double rateHz, double delayMs, double sampleRate) noexcept
        {
            phase = 0.0;
            increment = rateHz > 0.001 ? rateHz / sampleRate : 0.0;
            delayLeft = static_cast<juce::int64> (sampleRate * delayMs / 1000.0);
        }

        /** Its value now, then on by that many samples. */
        float next (int samples) noexcept
        {
            if (delayLeft > 0)
            {
                delayLeft -= samples;
                return 0.0f;
            }

            const auto value = static_cast<float> (std::sin (phase * juce::MathConstants<double>::twoPi));
            phase += increment * samples;
            phase -= std::floor (phase);
            return value;
        }
    };

    /** The sample between two of its frames: four-point Hermite. */
    inline float sampleAt (const float* data, juce::int64 length, double position) noexcept
    {
        const auto i = static_cast<juce::int64> (position);
        const auto f = static_cast<float> (position - static_cast<double> (i));
        const auto at = [data, length] (juce::int64 k) noexcept { return data[juce::jlimit<juce::int64> (0, length - 1, k)]; };
        const auto xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
        const auto c1 = 0.5f * (x1 - xm1);
        const auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    constexpr int controlInterval = 16;          // samples between the pitch, filter and LFO updates

    //==============================================================================
    /** A key by its number or its name, as SFZ writes them ("c4" or "c#4" or "db4": middle C, 60, is c4),
        moved by offset (<control>'s note_offset and octave_offset; the fallback isn't moved). */
    int keyFrom (const juce::String& text, int fallback, int offset = 0)
    {
        const auto t = text.trim().toLowerCase();

        if (t.isEmpty())
            return fallback;

        if (t.containsOnly ("-0123456789"))
            return juce::jlimit (0, 127, t.getIntValue() + offset);

        const auto letter = juce::String ("c d ef g a b").indexOfChar (t[0]);

        if (letter < 0)
            return fallback;

        auto rest = t.substring (1);
        auto semitone = letter;

        if (rest.startsWithChar ('#'))       { ++semitone; rest = rest.substring (1); }
        else if (rest.startsWithChar ('b'))  { --semitone; rest = rest.substring (1); }

        return juce::jlimit (0, 127, (rest.getIntValue() + 1) * 12 + semitone + offset);
    }
}

//==============================================================================
struct SonifySampler::Instrument
{
    struct Region
    {
        int sample = -1;
        int lokey = 0, hikey = 127, keycenter = 60;
        int lovel = 0, hivel = 127;
        float gain = 1.0f;                       // volume (dB) as a gain
        float tune = 0.0f;                       // cents (tune, and transpose's semitones)
        float veltrack = 1.0f;                   // amp_veltrack, 0..1
        juce::int64 offset = 0;
        bool loop = false;
        bool loopSustain = false;                // loop_sustain: the loop is left once the note is let go
        juce::int64 loopStart = 0, loopEnd = 0;
        juce::String crossfadeText;              // loop_crossfade as the SFZ has it (seconds; Sampler Arena's: samples)
        juce::int64 loopFade = 0;                // it, in samples
        int seqLength = 1, seqPosition = 1;
        int group = -1;                          // round robins sharing a count (-1: none)
        float volumeDb = 0.0f;
        int tuneCents = 0, transpose = 0;
        juce::String path;                       // as the SFZ named its sample
    };

    struct Sample
    {
        juce::AudioBuffer<float> audio;
        double rate = 44100.0;
        juce::File file;
    };

    juce::File file;
    juce::String name;
    std::vector<Region> regions;
    std::vector<Sample> samples;
    std::vector<int> sequence;                   // the audio thread's: each round robin group's count
    int keys = 0;                                // keys with a sample of their own
    std::vector<std::pair<uint32_t, double>> settings;   // the SFZ's own settings for the sampler
    juce::String note;                           // samples that couldn't be read

    /** Reads an SFZ and its samples (any thread). shouldStop: the reading is called off. */
    static std::unique_ptr<Instrument> read (const juce::File& file, juce::String& problem, const std::function<bool()>& shouldStop);
};

struct SonifySampler::Voice
{
    bool active = false, sustained = false;
    int note = -1, velocity = 0, region = -1;
    int key = -1;                                // the key as it came in (before Octave): its note-off finds it by it
    double position = 0.0;
    double pitch = 60.0, target = 60.0, glideStep = 0.0;     // the note it plays (semitones), gliding to target
    bool gliding = false;
    float velocityGain = 1.0f;
    Envelope amp, filter, pitchEnvelope;
    Filter left, right;
    Lfo ampLfo, filterLfo, pitchLfo;
    juce::uint32 age = 0;

    int countdown = 0;                           // samples to the next control update
    double step = 1.0;                           // sample frames per output sample
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 2.0f;
    bool filtering = false;
    float tremolo = 1.0f;

    void reset() noexcept
    {
        *this = Voice();
    }
};

//==============================================================================
std::unique_ptr<SonifySampler::Instrument> SonifySampler::Instrument::read (const juce::File& file, juce::String& problem,
                                                                           const std::function<bool()>& shouldStop)
{
    if (! file.existsAsFile())
    {
        problem = file.getFileName() + " isn't there any more.";
        return nullptr;
    }

    const auto text = file.loadFileAsString();

    if (text.isEmpty())
    {
        problem = file.getFileName() + " is empty, or can't be read.";
        return nullptr;
    }

    auto made = std::make_unique<Instrument>();
    made->file = file;
    made->name = file.getFileNameWithoutExtension();

    // Each header starts from the one above it: <global>, <master>, <group>, then <region>.
    enum class Section { none, control, global, master, group, region, other };
    auto section = Section::none;
    Region global, master, group, region;
    juce::String defaultPath;
    int noteOffset = 0, octaveOffset = 0;        // <control>'s: every key after them moved
    std::map<juce::String, juce::String> defines;
    std::vector<Region> regions;
    std::map<uint32_t, double> settings;
    bool releaseTrigger = false, groupRelease = false;

    const auto flush = [&]
    {
        if (section == Section::region && region.path.isNotEmpty() && ! releaseTrigger)
            regions.push_back (region);
    };

    const auto setting = [&settings] (uint32_t id, double value)
    {
        const auto& spec = specOf (id);
        settings[id] = juce::jlimit (spec.minValue, spec.maxValue, value);
    };

    const auto apply = [&] (const juce::String& key, const juce::String& value)
    {
        const auto number = value.getDoubleValue();

        // The sampler's own settings: wherever they're written (Sampler Arena's units).
        if (key == "ampeg_attack")         setting (P::ampAttack, number * 1000.0);
        else if (key == "ampeg_hold")      setting (P::ampHold, number * 1000.0);
        else if (key == "ampeg_decay")     setting (P::ampDecay, number * 1000.0);
        else if (key == "ampeg_sustain")   setting (P::ampSustain, number / 100.0);
        else if (key == "ampeg_release")   setting (P::ampRelease, number * 1000.0);
        else if (key == "pitcheg_attack")  setting (P::pitchAttack, number * 1000.0);
        else if (key == "pitcheg_hold")    setting (P::pitchHold, number * 1000.0);
        else if (key == "pitcheg_decay")   setting (P::pitchDecay, number * 1000.0);
        else if (key == "pitcheg_sustain") setting (P::pitchSustain, number / 100.0);
        else if (key == "pitcheg_release") setting (P::pitchRelease, number * 1000.0);
        else if (key == "pitcheg_depth")   setting (P::pitchDepth, number / 100.0);
        else if (key == "amplfo_freq")     setting (P::ampLfoRate, number);
        else if (key == "amplfo_depth")    setting (P::ampLfoDepth, number / 100.0);
        else if (key == "amplfo_delay")    setting (P::ampLfoDelay, number * 1000.0);
        else if (key == "fillfo_freq")     setting (P::filterLfoRate, number);
        else if (key == "fillfo_depth")    setting (P::filterLfoDepth, number / 100.0);
        else if (key == "fillfo_delay")    setting (P::filterLfoDelay, number * 1000.0);
        else if (key == "pitchlfo_freq")   setting (P::pitchLfoRate, number);
        else if (key == "pitchlfo_depth")  setting (P::pitchLfoDepth, number / 100.0);
        else if (key == "pitchlfo_delay")  setting (P::pitchLfoDelay, number * 1000.0);
        else if (key == "fil_keytrack")    setting (P::filterKeytrack, number);
        else if (key == "cutoff")          setting (P::cutoff, number);
        else if (key == "resonance")       setting (P::resonance, number / 24.0);
        else if (key == "bend_up")         setting (P::bendRange, std::round (number / 100.0));
        else if (key == "fil_type")
        {
            const auto type = value.trim().toLowerCase();
            setting (P::filterType, type.startsWith ("hpf") ? 1.0 : type.startsWith ("bpf") ? 2.0 : type.startsWith ("brf") ? 3.0 : 0.0);
        }

        if (section == Section::control)
        {
            if (key == "default_path")
                defaultPath = value.trim().replaceCharacter ('\\', '/');
            else if (key == "note_offset")
                noteOffset = value.getIntValue();
            else if (key == "octave_offset")
                octaveOffset = value.getIntValue();

            return;
        }

        auto* target = section == Section::global ? &global
                     : section == Section::master ? &master
                     : section == Section::group  ? &group
                     : section == Section::region ? &region : nullptr;

        if (target == nullptr)
            return;

        auto& r = *target;
        const auto shift = noteOffset + 12 * octaveOffset;

        if (key == "sample")                         r.path = defaultPath + value.trim();
        else if (key == "key")                       r.lokey = r.hikey = r.keycenter = keyFrom (value, r.keycenter, shift);
        else if (key == "lokey")                     r.lokey = keyFrom (value, r.lokey, shift);
        else if (key == "hikey")                     r.hikey = keyFrom (value, r.hikey, shift);
        else if (key == "pitch_keycenter")           r.keycenter = keyFrom (value, r.keycenter, shift);
        else if (key == "lovel")                     r.lovel = juce::jlimit (0, 127, value.getIntValue());
        else if (key == "hivel")                     r.hivel = juce::jlimit (0, 127, value.getIntValue());
        else if (key == "volume")                    r.volumeDb = static_cast<float> (number);
        else if (key == "tune")                      r.tuneCents = value.getIntValue();
        else if (key == "transpose")                 r.transpose = value.getIntValue();
        else if (key == "offset")                    r.offset = juce::jmax<juce::int64> (0, value.getLargeIntValue());
        else if (key == "amp_veltrack")              r.veltrack = static_cast<float> (juce::jlimit (-100.0, 100.0, number) / 100.0);
        else if (key == "loop_mode" || key == "loopmode")
        {
            r.loop = value.trim() == "loop_continuous" || value.trim() == "loop_sustain";
            r.loopSustain = value.trim() == "loop_sustain";
        }
        else if (key == "loop_start" || key == "loopstart")   r.loopStart = juce::jmax<juce::int64> (0, value.getLargeIntValue());
        else if (key == "loop_end" || key == "loopend")       r.loopEnd = juce::jmax<juce::int64> (0, value.getLargeIntValue());
        else if (key == "loop_crossfade")            r.crossfadeText = value.trim();
        else if (key == "seq_length")                r.seqLength = juce::jlimit (1, 100, value.getIntValue());
        else if (key == "seq_position")              r.seqPosition = juce::jlimit (1, 100, value.getIntValue());
        else if (key == "trigger")
        {
            const auto release = value.trim().startsWith ("release");

            if (section == Section::region)    releaseTrigger = release;
            else if (section == Section::group) groupRelease = release;
        }
    };

    // An opcode's value runs to the next opcode (name=) or the line's end: a sample's name may
    // have spaces in it.
    const auto opcodes = [&] (const juce::String& line)
    {
        auto rest = line.trim();

        while (rest.isNotEmpty())
        {
            const auto equals = rest.indexOfChar ('=');

            if (equals <= 0)
                break;

            const auto key = rest.substring (0, equals).trim();
            auto valueAndMore = rest.substring (equals + 1);
            auto end = valueAndMore.length();

            // The next " name=" ends this value.
            for (int i = 0; i < valueAndMore.length(); ++i)
            {
                if (! juce::CharacterFunctions::isWhitespace (valueAndMore[i]))
                    continue;

                int j = i;

                while (j < valueAndMore.length() && juce::CharacterFunctions::isWhitespace (valueAndMore[j]))
                    ++j;

                int w = j;

                while (w < valueAndMore.length() && (juce::CharacterFunctions::isLetterOrDigit (valueAndMore[w]) || valueAndMore[w] == '_'))
                    ++w;

                if (w > j && w < valueAndMore.length() && valueAndMore[w] == '=')
                {
                    end = i;
                    break;
                }
            }

            apply (key.toLowerCase(), valueAndMore.substring (0, end).trim());
            rest = valueAndMore.substring (end).trim();
        }
    };

    // The instrument's own name, as the Sampling Studio and Sampler Arena write it.
    for (const auto& line : juce::StringArray::fromLines (text))
    {
        if (line.trimStart().startsWith ("// Family:"))
        {
            if (const auto family = line.fromFirstOccurrenceOf ("// Family:", false, false).trim(); family.isNotEmpty())
            {
                made->name = family;
                break;
            }
        }
    }

    // Read as the SFZ to Blob reads it (its #include and comments, then #define and <control>'s
    // offsets here), so an instrument maps the same here and in its blob.
    juce::StringArray includeNotes;

    for (auto line : juce::StringArray::fromLines (BlobMaker::sfzText (file, includeNotes)))
    {
        line = line.trim();

        if (line.startsWith ("#define"))
        {
            const auto rest = line.substring (7).trim();
            const auto name = rest.upToFirstOccurrenceOf (" ", false, false).trim();

            if (name.startsWithChar ('$'))
                defines[name] = rest.fromFirstOccurrenceOf (" ", false, false).trim();

            continue;
        }

        // $NAME: what its #define says.
        if (line.containsChar ('$') && ! defines.empty())
        {
            juce::String replaced;

            for (int i = 0; i < line.length();)
            {
                if (line[i] == '$')
                {
                    int j = i + 1;

                    while (j < line.length() && (juce::CharacterFunctions::isLetterOrDigit (line[j]) || line[j] == '_'))
                        ++j;

                    const auto name = line.substring (i, j);

                    if (const auto found = defines.find (name); found != defines.end())
                    {
                        replaced << found->second;
                        i = j;
                        continue;
                    }
                }

                replaced << juce::String::charToString (line[i]);
                ++i;
            }

            line = replaced;
        }

        while (line.isNotEmpty())
        {
            const auto open = line.indexOfChar ('<');

            if (open != 0)
            {
                // Opcodes before the next header (or to the line's end).
                opcodes (open < 0 ? line : line.substring (0, open));

                if (open < 0)
                    break;

                line = line.substring (open);
            }

            const auto close = line.indexOfChar ('>');

            if (close < 0)
                break;

            const auto header = line.substring (1, close).trim().toLowerCase();
            line = line.substring (close + 1).trim();
            flush();

            if (header == "control")       { section = Section::control; }
            else if (header == "global")   { section = Section::global; global = Region(); master = global; group = global; }
            else if (header == "master")   { section = Section::master; master = global; group = master; }
            else if (header == "group")    { section = Section::group; group = master; groupRelease = false; }
            else if (header == "region")   { section = Section::region; region = group; releaseTrigger = groupRelease; }
            else                           { section = Section::other; }
        }

        if (shouldStop())
            return nullptr;
    }

    flush();

    if (regions.empty())
    {
        problem = file.getFileName() + " has no samples in it (no <region> with a sample=).";
        return nullptr;
    }

    // The samples, each read once (several regions may share one).
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::map<juce::String, int> sampleIndex;
    juce::StringArray missing;
    juce::int64 totalBytes = 0;
    const auto base = file.getParentDirectory();

    for (auto& r : regions)
    {
        if (shouldStop())
            return nullptr;

        const auto native = r.path.replaceCharacter ('\\', '/');
        const auto sampleFile = juce::File::isAbsolutePath (native) ? juce::File (native) : base.getChildFile (native);
        const auto key = sampleFile.getFullPathName();

        if (const auto found = sampleIndex.find (key); found != sampleIndex.end())
        {
            r.sample = found->second;
            continue;
        }

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (sampleFile));

        if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
        {
            sampleIndex[key] = -1;
            missing.addIfNotAlreadyThere (sampleFile.getFileName());
            continue;
        }

        const auto channels = static_cast<int> (juce::jlimit (1u, 2u, reader->numChannels));
        const auto frames = juce::jmin<juce::int64> (reader->lengthInSamples, std::numeric_limits<int>::max() - 4);
        totalBytes += frames * channels * static_cast<juce::int64> (sizeof (float));

        if (totalBytes > (static_cast<juce::int64> (3) << 30))
        {
            problem = file.getFileName() + "'s samples are too big to read into memory (over 3 GB).";
            return nullptr;
        }

        Sample sample;
        sample.file = sampleFile;
        sample.rate = reader->sampleRate;
        sample.audio.setSize (channels, static_cast<int> (frames));
        reader->read (&sample.audio, 0, static_cast<int> (frames), 0, true, channels > 1);
        sampleIndex[key] = static_cast<int> (made->samples.size());
        r.sample = static_cast<int> (made->samples.size());
        made->samples.push_back (std::move (sample));
    }

    std::map<std::tuple<int, int, int, int>, int> groups;
    std::vector<int> ownKeys;

    for (auto& r : regions)
    {
        if (r.sample < 0)
            continue;

        if (r.lokey > r.hikey)
            std::swap (r.lokey, r.hikey);

        if (r.lovel > r.hivel)
            std::swap (r.lovel, r.hivel);

        r.gain = juce::Decibels::decibelsToGain (r.volumeDb);
        r.tune = static_cast<float> (r.tuneCents + 100 * r.transpose);

        const auto length = made->samples[static_cast<size_t> (r.sample)].audio.getNumSamples();
        r.offset = juce::jlimit<juce::int64> (0, juce::jmax (0, length - 1), r.offset);

        // Its loop ends at the last frame at the most (where the voice would end): an end past it,
        // as some tools write (the frame count), loops there.
        r.loopEnd = juce::jmin<juce::int64> (r.loopEnd, length - 1);
        r.loop = r.loop && r.loopEnd > r.loopStart;

        // The loop's crossfade: seconds as the standard has it, or (a whole number of 10 or more)
        // samples as Sampler Arena writes it; never longer than the loop, nor than what's before it.
        if (r.loop && r.crossfadeText.isNotEmpty())
        {
            const auto rate = made->samples[static_cast<size_t> (r.sample)].rate;
            const auto number = r.crossfadeText.getDoubleValue();
            const auto fade = ! r.crossfadeText.containsChar ('.') && number >= 10.0 ? number : number * rate;
            r.loopFade = juce::jlimit<juce::int64> (0, juce::jmin (r.loopEnd - r.loopStart, r.loopStart), static_cast<juce::int64> (fade));
        }

        if (r.seqLength > 1)
        {
            const auto groupKey = std::make_tuple (r.lokey, r.hikey, r.lovel, r.hivel);
            auto found = groups.find (groupKey);

            if (found == groups.end())
                found = groups.emplace (groupKey, static_cast<int> (groups.size())).first;

            r.group = found->second;
        }

        ownKeys.push_back (r.keycenter);
        made->regions.push_back (r);
    }

    if (made->regions.empty())
    {
        problem = "None of " + file.getFileName() + "'s samples can be read (" + missing.joinIntoString (", ").substring (0, 120) + ").";
        return nullptr;
    }

    std::sort (ownKeys.begin(), ownKeys.end());
    made->keys = static_cast<int> (std::unique (ownKeys.begin(), ownKeys.end()) - ownKeys.begin());
    made->sequence.assign (groups.size(), 0);

    for (const auto& [id, value] : settings)
        made->settings.emplace_back (id, value);

    if (! missing.isEmpty())
        made->note = juce::String (missing.size()) + (missing.size() == 1 ? " sample is" : " samples are") + " missing";

    return made;
}

//==============================================================================
/** Reads an instrument on a thread of its own: a new one asked for calls off the last. */
class SonifySampler::Loader final : public juce::Thread
{
public:
    explicit Loader (SonifySampler& owner)
        : juce::Thread ("Sonify Sampler loading"), sampler (owner)
    {
    }

    ~Loader() override
    {
        stopThread (10000);
    }

    void load (const juce::File& sfz, bool withSettings, int generation)
    {
        stopThread (10000);
        file = sfz;
        takeSettings = withSettings;
        asked = generation;
        startThread();
    }

    void run() override
    {
        const auto mine = asked;
        juce::String why;
        auto made = Instrument::read (file, why, [this] { return threadShouldExit(); });

        if (threadShouldExit())
            return;

        // Only the one asked for last is installed, or says it failed: one asked for since (or a
        // Clear) leaves this one out, under the lock it's asked for under, so nothing stale is
        // played nor said over it.
        const juce::ScopedLock lock (sampler.fileLock);

        if (sampler.loadGeneration != mine)
            return;

        if (made == nullptr)
        {
            sampler.problem = why;
            sampler.loading.store (Loading::failed);
            return;
        }

        if (takeSettings)
            sampler.applySettings (made->settings);

        sampler.install (std::move (made));
    }

private:
    SonifySampler& sampler;
    juce::File file;
    bool takeSettings = true;
    int asked = 0;
};

//==============================================================================
Plugin::Descriptor Samplers::describe()
{
    Plugin::Descriptor descriptor;
    descriptor.id = playerId;
    descriptor.name = "Sonify Sampler";
    descriptor.vendor = "Sonify";
    descriptor.version = "1.0";
    descriptor.features = { "instrument", "sampler", "stereo" };
    return descriptor;
}

void Samplers::setUserFolder (std::function<juce::File()> userData)
{
    userFolder() = std::move (userData);
}

juce::File Samplers::folder()
{
    const auto base = userFolder() != nullptr ? userFolder()()
                                             : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Sonify");
    return base.getChildFile ("Samples");
}

namespace
{
    juce::File& madeLatest()
    {
        static juce::File file;
        return file;
    }
}

void Samplers::setLatest (const juce::File& sfz)
{
    madeLatest() = sfz;
}

juce::File Samplers::latest()
{
    juce::File newest;
    juce::Time when;

    if (const auto made = madeLatest(); made.existsAsFile())
    {
        newest = made;
        when = made.getLastModificationTime();
    }

    for (const auto& entry : juce::RangedDirectoryIterator (folder(), false, "*", juce::File::findDirectories))
    {
        for (const auto& sfz : entry.getFile().findChildFiles (juce::File::findFiles, false, "*.sfz"))
        {
            if (const auto modified = sfz.getLastModificationTime(); newest == juce::File() || modified > when)
            {
                newest = sfz;
                when = modified;
            }
        }
    }

    return newest;
}

void Samplers::setScreenFactory (ScreenFactory factory)
{
    screenFactory() = std::move (factory);
}

//==============================================================================
SonifySampler::SonifySampler()
    : descriptor (Samplers::describe()),
      voices (std::make_unique<std::array<Voice, maxVoices>>())
{
    inFile.push_back (descriptor);

    for (const auto& spec : specs)
    {
        Parameter parameter;
        parameter.id = spec.id;
        parameter.name = spec.name;
        parameter.minValue = spec.minValue;
        parameter.maxValue = spec.maxValue;
        parameter.defaultValue = spec.defaultValue;
        parameter.stepped = spec.stepped;
        parameters.push_back (parameter);
        values[spec.id].store (spec.defaultValue);
    }

    heldNotes.reserve (128);
    loader = std::make_unique<Loader> (*this);
}

SonifySampler::~SonifySampler()
{
    screen = nullptr;
    loader = nullptr;   // a reading under way is called off
}

const juce::File& SonifySampler::getSourceFile() const noexcept
{
    return Builtins::file();
}

//==============================================================================
double SonifySampler::get (Param id) const noexcept
{
    return id >= 1 && id <= static_cast<uint32_t> (numParams) ? values[id].load (std::memory_order_relaxed) : 0.0;
}

double SonifySampler::getDefault (Param id) const noexcept
{
    return specOf (id).defaultValue;
}

void SonifySampler::set (Param id, double value)
{
    queueParameter (static_cast<int> (id) - 1, value);
}

int SonifySampler::findParameter (uint32_t id) const noexcept
{
    return id >= 1 && id <= static_cast<uint32_t> (numParams) ? static_cast<int> (id) - 1 : -1;
}

double SonifySampler::getParameterValue (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, numParams) ? values[static_cast<size_t> (index) + 1].load (std::memory_order_relaxed) : 0.0;
}

double SonifySampler::queueParameter (int index, double value)
{
    if (! juce::isPositiveAndBelow (index, numParams))
        return 0.0;

    const auto& spec = specs[static_cast<size_t> (index)];
    value = juce::jlimit (spec.minValue, spec.maxValue, std::isfinite (value) ? value : spec.defaultValue);

    if (spec.stepped)
        value = std::round (value);

    values[spec.id].store (value);
    stateChanged.store (true);
    return value;
}

void SonifySampler::setParameterFromAudio (uint32_t paramId, int& indexHint, double value) noexcept
{
    indexHint = findParameter (paramId);

    if (indexHint < 0)
        return;

    const auto& spec = specs[static_cast<size_t> (indexHint)];
    value = juce::jlimit (spec.minValue, spec.maxValue, std::isfinite (value) ? value : spec.defaultValue);
    values[spec.id].store (spec.stepped ? std::round (value) : value, std::memory_order_relaxed);
}

juce::String SonifySampler::getParameterText (int index, double value)
{
    if (! juce::isPositiveAndBelow (index, numParams))
        return {};

    const auto id = specs[static_cast<size_t> (index)].id;

    const auto ms = [] (double v) -> juce::String
    {
        if (v < 1.0)     return juce::String (v, 1) + " ms";
        if (v < 1000.0)  return juce::String (juce::roundToInt (v)) + " ms";
        return juce::String (v / 1000.0, 2) + " s";
    };

    const auto percent = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };

    switch (id)
    {
        case volume:
            return value <= 0.0001 ? juce::String ("-inf dB") : juce::String (juce::Decibels::gainToDecibels (value), 1) + " dB";

        case octave:            return (value > 0 ? "+" : "") + juce::String (juce::roundToInt (value));
        case playMode:          return value < 0.5 ? "Poly" : "Mono";
        case monoMode:          return value < 0.5 ? "Legato" : value < 1.5 ? "Retrigger New" : "Retrigger Stolen";
        case glide:             return value < 0.1 ? juce::String ("Off") : ms (value);
        case bendRange:         return juce::String (juce::roundToInt (value)) + " st";
        case filterType:        return value < 0.5 ? "Low Pass" : value < 1.5 ? "High Pass" : value < 2.5 ? "Band Pass" : "Notch";
        case cutoff:            return value >= 1000.0 ? juce::String (value / 1000.0, value >= 10000.0 ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (value)) + " Hz";
        case filterKeytrack:    return juce::String (juce::roundToInt (value)) + "%";
        case pitchKeytrack:     return juce::String (juce::roundToInt (value)) + "%";
        case pitchVeltrack:     return juce::String (juce::roundToInt (value)) + " ct";
        case pitchDepth:
        case pitchLfoDepth:     return (value > 0 ? "+" : "") + juce::String (value, 1) + " st";
        case ampLfoRate:
        case filterLfoRate:
        case pitchLfoRate:      return value < 0.01 ? juce::String ("Off") : juce::String (value, 2) + " Hz";

        case ampSustain:
        case filterSustain:
        case pitchSustain:
        case resonance:
        case filterAmount:
        case ampLfoDepth:
        case filterLfoDepth:    return percent (value);

        default:                return ms (value);
    }
}

//==============================================================================
void SonifySampler::loadInstrument (const juce::File& sfz, bool takeSettings)
{
    int generation = 0;

    {
        // With the generation: a reading for the last one asked for can't install it, nor say
        // ready or failed, after this.
        const juce::ScopedLock lock (fileLock);
        loadingFile = sfz;
        problem.clear();
        generation = ++loadGeneration;
        loading.store (Loading::loading);
    }

    stateChanged.store (true);
    loader->load (sfz, takeSettings, generation);
}

void SonifySampler::clearInstrument()
{
    loader->stopThread (10000);

    {
        const juce::ScopedLock lock (fileLock);
        loadingFile = juce::File();
        problem.clear();
        ++loadGeneration;   // a reading that didn't stop installs nothing over the cleared one
    }

    install (nullptr);
    loading.store (Loading::none);
    stateChanged.store (true);
}

juce::File SonifySampler::getInstrumentFile() const
{
    const juce::ScopedLock lock (fileLock);
    return loadingFile;
}

juce::String SonifySampler::getProblem() const
{
    const juce::ScopedLock lock (fileLock);
    return problem;
}

juce::String SonifySampler::describeInstrument() const
{
    const juce::ScopedLock lock (fileLock);
    return description;
}

void SonifySampler::install (std::unique_ptr<Instrument> made)
{
    const auto loaded = made != nullptr;
    juce::String text;

    if (made != nullptr)
    {
        text = made->name + ": " + juce::String (made->keys) + (made->keys == 1 ? " key, " : " keys, ")
             + juce::String (made->samples.size()) + (made->samples.size() == 1 ? " sample" : " samples");

        if (made->note.isNotEmpty())
            text << " (" << made->note << ")";
    }

    {
        const juce::ScopedLock lock (fileLock);
        description = text;
    }

    {
        const juce::SpinLock::ScopedLockType lock (instrumentLock);
        std::swap (instrument, made);
        instrumentChanged.store (true);
    }

    made.reset();   // the last one, outside the lock

    if (loaded)
        loading.store (Loading::ready);
}

void SonifySampler::applySettings (const std::vector<std::pair<uint32_t, double>>& settings)
{
    for (const auto& [id, value] : settings)
        set (static_cast<Param> (id), value);
}

//==============================================================================
bool SonifySampler::saveState (juce::MemoryBlock& destination)
{
    auto* params = new juce::DynamicObject();

    for (const auto& spec : specs)
        params->setProperty (juce::String (spec.id), values[spec.id].load());

    auto* root = new juce::DynamicObject();
    root->setProperty ("plugin", descriptor.id);
    root->setProperty ("version", 1);
    root->setProperty ("params", juce::var (params));
    root->setProperty ("sfz", getInstrumentFile().getFullPathName());

    const auto text = juce::JSON::toString (juce::var (root), true);
    destination.reset();
    destination.append (text.toRawUTF8(), text.getNumBytesAsUTF8());
    return true;
}

bool SonifySampler::loadState (const juce::MemoryBlock& source)
{
    const auto parsed = juce::JSON::parse (source.toString());

    if (! parsed.isObject() || parsed.getProperty ("plugin", {}).toString() != descriptor.id)
        return false;

    if (const auto* params = parsed.getProperty ("params", {}).getDynamicObject())
        for (const auto& [name, value] : params->getProperties())
            queueParameter (findParameter (static_cast<uint32_t> (name.toString().getIntValue())), static_cast<double> (value));

    // The song's settings win over the SFZ's own.
    if (const auto path = parsed.getProperty ("sfz", {}).toString(); path.isNotEmpty())
        loadInstrument (juce::File (path), false);
    else
        clearInstrument();

    stateChanged.store (false);
    return true;
}

bool SonifySampler::savePreset (const juce::File& file)
{
    juce::ValueTree tree ("SfzPlayerState");

    if (const auto sfz = getInstrumentFile(); sfz.existsAsFile())
        tree.setProperty ("sfzPath", sfz.getFullPathName(), nullptr);

    for (const auto& spec : specs)
    {
        if (spec.arenaName == nullptr)
            continue;

        const auto value = values[spec.id].load();

        if (spec.stepped)
            tree.setProperty (spec.arenaName, juce::roundToInt (value), nullptr);
        else
            tree.setProperty (spec.arenaName, value, nullptr);
    }

    const auto mono = juce::roundToInt (get (monoMode));
    tree.setProperty ("monoHoldStolen", mono == 0, nullptr);
    tree.setProperty ("monoRetrigNew", mono == 1, nullptr);
    tree.setProperty ("monoRetrigStolen", mono == 2, nullptr);
    tree.setProperty ("loopSyncEnabled", false, nullptr);
    tree.setProperty ("loopSyncOrigBPM", 120.0, nullptr);
    tree.setProperty ("loopSyncBars", 1.0, nullptr);
    tree.setProperty ("loopSyncTargetBPM", 120.0, nullptr);
    tree.setProperty ("loopSyncToHost", true, nullptr);

    juce::MemoryOutputStream stream;
    tree.writeToStream (stream);
    const auto target = file.hasFileExtension (Samplers::presetExtension) ? file : file.withFileExtension (Samplers::presetExtension);
    return target.replaceWithData (stream.getData(), stream.getDataSize());
}

bool SonifySampler::loadPreset (const juce::File& file)
{
    juce::MemoryBlock data;

    if (! file.loadFileAsData (data) || data.getSize() == 0)
        return false;

    const auto tree = juce::ValueTree::readFromData (data.getData(), data.getSize());

    if (! tree.isValid())
        return false;

    for (const auto& spec : specs)
        if (spec.arenaName != nullptr && tree.hasProperty (spec.arenaName))
            set (static_cast<Param> (spec.id), static_cast<double> (tree.getProperty (spec.arenaName)));

    // Sampler Arena's three mono boxes: the first ticked wins (none: Hold Stolen, its default).
    if (tree.hasProperty ("monoHoldStolen"))
    {
        const auto mode = static_cast<bool> (tree.getProperty ("monoHoldStolen")) ? 0
                        : static_cast<bool> (tree.getProperty ("monoRetrigNew", false)) ? 1
                        : static_cast<bool> (tree.getProperty ("monoRetrigStolen", false)) ? 2 : 0;
        set (monoMode, mode);
    }

    if (const auto path = tree.getProperty ("sfzPath", {}).toString(); path.isNotEmpty() && juce::File (path).existsAsFile())
        loadInstrument (juce::File (path), false);

    return true;
}

bool SonifySampler::saveInstrument (const juce::File& file)
{
    const auto target = file.hasFileExtension ("sfz") ? file : file.withFileExtension ("sfz");
    juce::String sfz;
    sfz << "// Generated by the Sonify Sampler\n\n<global>\n";

    const auto seconds = [] (double ms) { return juce::String (ms / 1000.0, 4); };
    sfz << "ampeg_attack=" << seconds (get (ampAttack)) << "\n";

    if (get (ampHold) > 0.01)
        sfz << "ampeg_hold=" << seconds (get (ampHold)) << "\n";

    sfz << "ampeg_decay=" << seconds (get (ampDecay)) << "\n";
    sfz << "ampeg_sustain=" << juce::String (get (ampSustain) * 100.0, 1) << "\n";
    sfz << "ampeg_release=" << seconds (get (ampRelease)) << "\n";

    const auto type = juce::roundToInt (get (filterType));

    if (get (cutoff) < 19999.0 || get (resonance) > 0.01)
    {
        sfz << "fil_type=" << (type == 1 ? "hpf_2p" : type == 2 ? "bpf_2p" : type == 3 ? "brf_2p" : "lpf_2p") << "\n";
        sfz << "cutoff=" << juce::roundToInt (get (cutoff)) << "\n";

        if (get (resonance) > 0.01)
            sfz << "resonance=" << juce::String (get (resonance) * 24.0, 1) << "\n";
    }

    if (get (filterKeytrack) > 0.1)
        sfz << "fil_keytrack=" << juce::roundToInt (get (filterKeytrack)) << "\n";

    const auto lfo = [&] (const char* prefix, Param rate, Param depth, Param delay, double depthScale)
    {
        if (get (rate) <= 0.01)
            return;

        sfz << prefix << "_freq=" << juce::String (get (rate), 2) << "\n";
        sfz << prefix << "_depth=" << juce::String (get (depth) * depthScale, 1) << "\n";

        if (get (delay) > 0.01)
            sfz << prefix << "_delay=" << juce::String (get (delay) / 1000.0, 3) << "\n";
    };

    lfo ("amplfo", ampLfoRate, ampLfoDepth, ampLfoDelay, 100.0);
    lfo ("fillfo", filterLfoRate, filterLfoDepth, filterLfoDelay, 100.0);
    lfo ("pitchlfo", pitchLfoRate, pitchLfoDepth, pitchLfoDelay, 100.0);

    if (const auto bendCents = juce::roundToInt (get (bendRange) * 100.0); bendCents > 0)
        sfz << "bend_up=" << bendCents << "\nbend_down=" << -bendCents << "\n";

    sfz << "\n";

    // The regions and their samples' files, taken quickly (the audio thread waits meanwhile).
    std::vector<Instrument::Region> regions;
    std::vector<juce::File> sampleFiles;

    {
        const juce::SpinLock::ScopedLockType lock (instrumentLock);

        if (instrument == nullptr)
            return false;

        regions = instrument->regions;

        for (const auto& sample : instrument->samples)
            sampleFiles.push_back (sample.file);
    }

    {
        for (const auto& r : regions)
        {
            const auto& sampleFile = sampleFiles[static_cast<size_t> (r.sample)];
            const auto relative = sampleFile.getRelativePathFrom (target.getParentDirectory()).replaceCharacter ('/', '\\');
            sfz << "<region>\nsample=" << relative << "\n";
            sfz << "lokey=" << r.lokey << "\nhikey=" << r.hikey << "\npitch_keycenter=" << r.keycenter << "\n";
            sfz << "lovel=" << r.lovel << " hivel=" << r.hivel << "\n";
            sfz << "amp_veltrack=" << juce::roundToInt (r.veltrack * 100.0f) << "\n";

            if (r.seqLength > 1)
                sfz << "seq_length=" << r.seqLength << "\nseq_position=" << r.seqPosition << "\n";

            if (r.tuneCents != 0)
                sfz << "tune=" << r.tuneCents << "\n";

            if (r.transpose != 0)
                sfz << "transpose=" << r.transpose << "\n";

            if (r.offset > 0)
                sfz << "offset=" << r.offset << "\n";

            if (std::abs (r.volumeDb) > 0.01f)
                sfz << "volume=" << juce::String (r.volumeDb, 1) << "\n";

            if (r.loop)
            {
                sfz << "loop_mode=" << (r.loopSustain ? "loop_sustain" : "loop_continuous")
                    << "\nloop_start=" << r.loopStart << "\nloop_end=" << r.loopEnd << "\n";

                if (r.crossfadeText.isNotEmpty())
                    sfz << "loop_crossfade=" << r.crossfadeText << "\n";
            }

            sfz << "\n";
        }
    }

    if (! target.replaceWithText (sfz, false, false, "\r\n"))
        return false;

    if (target != getInstrumentFile())
    {
        const juce::ScopedLock lock (fileLock);
        loadingFile = target;
    }

    stateChanged.store (true);
    return true;
}

//==============================================================================
bool SonifySampler::activate (double newRate, int)
{
    sampleRate = newRate > 0.0 ? newRate : 48000.0;

    for (auto& voice : *voices)
        voice.reset();

    heldNotes.clear();
    bend = 0.0f;
    pedalDown = false;
    active.store (true);
    return true;
}

bool SonifySampler::showNativeScreen (const juce::String& title)
{
    if (screen == nullptr)
        if (auto& factory = screenFactory())
            screen = factory (*this);

    if (screen == nullptr)
        return false;

    screen->show (title);
    return true;
}

void SonifySampler::hideNativeScreen()
{
    if (screen != nullptr)
        screen->hide();
}

//==============================================================================
// Playing, audio thread.

void SonifySampler::process (const float*, const float*, float* outLeft, float* outRight,
                             int numSamples, const juce::MidiBuffer& midi, const clap_event_transport_t*) noexcept
{
    if (outLeft == nullptr || outRight == nullptr || numSamples <= 0)
        return;

    juce::FloatVectorOperations::clear (outLeft, numSamples);
    juce::FloatVectorOperations::clear (outRight, numSamples);

    // A mixdown waits for an instrument being read (the device never does).
    if (Plugin::offlineRendering().load (std::memory_order_relaxed))
        for (int waited = 0; loading.load() == Loading::loading && waited < 60000; ++waited)
            juce::Thread::sleep (1);

    const juce::SpinLock::ScopedTryLockType lock (instrumentLock);

    if (! lock.isLocked())
        return;

    // A new instrument: what played on the last one stops.
    if (instrumentChanged.exchange (false))
    {
        for (auto& voice : *voices)
            voice.reset();

        heldNotes.clear();
    }

    int at = 0;

    const auto renderTo = [&] (int to)
    {
        if (to <= at || instrument == nullptr)
            return;

        for (auto& voice : *voices)
            if (voice.active)
                render (voice, outLeft + at, outRight + at, to - at);

        at = to;
    };

    for (const auto metadata : midi)
    {
        renderTo (juce::jlimit (0, numSamples, metadata.samplePosition));
        const auto message = metadata.getMessage();

        if (message.isNoteOn())
            startNote (message.getNoteNumber(), message.getVelocity());
        else if (message.isNoteOff())
            stopNote (message.getNoteNumber());
        else if (message.isSustainPedalOn())
            pedal (true);
        else if (message.isSustainPedalOff())
            pedal (false);
        else if (message.isAllSoundOff())
        {
            for (auto& voice : *voices)
                voice.reset();

            heldNotes.clear();
        }
        else if (message.isAllNotesOff())
        {
            releaseAll (5.0f);
            heldNotes.clear();
            pedalDown = false;
        }
        else if (message.isPitchWheel())
        {
            bend = static_cast<float> (message.getPitchWheelValue() - 8192) / 8192.0f;
        }
    }

    renderTo (numSamples);

    const auto gain = static_cast<float> (get (volume));

    if (std::abs (gain - 1.0f) > 1.0e-6f)
    {
        juce::FloatVectorOperations::multiply (outLeft, gain, numSamples);
        juce::FloatVectorOperations::multiply (outRight, gain, numSamples);
    }
}

int SonifySampler::findRegion (int note, int velocity) noexcept
{
    if (instrument == nullptr)
        return -1;

    const auto& regions = instrument->regions;
    int best = -1, bestRange = 128;

    // The narrowest region that takes the key and velocity (Sampler Arena's rule).
    for (int i = 0; i < static_cast<int> (regions.size()); ++i)
    {
        const auto& r = regions[static_cast<size_t> (i)];

        if (note >= r.lokey && note <= r.hikey && velocity >= r.lovel && velocity <= r.hivel && r.hikey - r.lokey < bestRange)
        {
            best = i;
            bestRange = r.hikey - r.lokey;
        }
    }

    // Round robins: its group's next take.
    if (best >= 0)
    {
        const auto group = regions[static_cast<size_t> (best)].group;

        if (juce::isPositiveAndBelow (group, static_cast<int> (instrument->sequence.size())))
        {
            const auto length = regions[static_cast<size_t> (best)].seqLength;
            const auto wanted = instrument->sequence[static_cast<size_t> (group)]++ % juce::jmax (1, length) + 1;

            for (int i = 0; i < static_cast<int> (regions.size()); ++i)
                if (regions[static_cast<size_t> (i)].group == group && regions[static_cast<size_t> (i)].seqPosition == wanted)
                    return i;
        }
    }

    return best;
}

SonifySampler::Voice* SonifySampler::freeVoice() noexcept
{
    Voice* oldest = nullptr;

    for (auto& voice : *voices)
    {
        if (! voice.active)
            return &voice;

        if (oldest == nullptr || voice.age < oldest->age)
            oldest = &voice;
    }

    if (oldest != nullptr)
        oldest->reset();

    return oldest;
}

SonifySampler::Voice* SonifySampler::soundingVoice() noexcept
{
    Voice* newest = nullptr;

    for (auto& voice : *voices)
        if (voice.active && ! voice.amp.isReleasing() && (newest == nullptr || voice.age > newest->age))
            newest = &voice;

    return newest;
}

void SonifySampler::startVoice (Voice& voice, int note, int velocity, int region, double glideFrom) noexcept
{
    const auto& r = instrument->regions[static_cast<size_t> (region)];
    voice.reset();
    voice.active = true;
    voice.note = note;
    voice.velocity = velocity;
    voice.region = region;
    voice.position = static_cast<double> (r.offset);

    // Louder the harder it's played, full at the top of its velocity range.
    const auto taper = juce::jlimit (0.0f, 1.0f, static_cast<float> (velocity) / static_cast<float> (juce::jmax (1, r.hivel)));
    voice.velocityGain = 1.0f - r.veltrack * (1.0f - taper);

    voice.pitch = voice.target = note;

    if (glideFrom >= 0.0)
    {
        voice.pitch = glideFrom;
        glideTo (voice, note, region);
    }

    retrigger (voice);
    voice.age = ++voiceAge;
}

void SonifySampler::glideTo (Voice& voice, int note, int region) noexcept
{
    voice.note = note;
    voice.region = region;
    voice.target = note;
    const auto glideMs = get (glide);

    if (glideMs > 0.1)
    {
        voice.glideStep = std::abs (voice.target - voice.pitch) / juce::jmax (1.0, sampleRate * glideMs / 1000.0);
        voice.gliding = voice.glideStep > 0.0;
    }
    else
    {
        voice.pitch = voice.target;
        voice.gliding = false;
    }
}

void SonifySampler::retrigger (Voice& voice) noexcept
{
    voice.amp.noteOn (static_cast<float> (get (ampAttack)), static_cast<float> (get (ampHold)), static_cast<float> (get (ampDecay)),
                      static_cast<float> (get (ampSustain)), sampleRate);
    voice.filter.noteOn (static_cast<float> (get (filterAttack)), static_cast<float> (get (filterHold)), static_cast<float> (get (filterDecay)),
                         static_cast<float> (get (filterSustain)), sampleRate);
    voice.pitchEnvelope.noteOn (static_cast<float> (get (pitchAttack)), static_cast<float> (get (pitchHold)), static_cast<float> (get (pitchDecay)),
                                static_cast<float> (get (pitchSustain)), sampleRate);
    voice.ampLfo.reset (get (ampLfoRate), get (ampLfoDelay), sampleRate);
    voice.filterLfo.reset (get (filterLfoRate), get (filterLfoDelay), sampleRate);
    voice.pitchLfo.reset (get (pitchLfoRate), get (pitchLfoDelay), sampleRate);
    voice.countdown = 0;
}

void SonifySampler::startNote (int note, int velocity) noexcept
{
    const auto played = juce::jlimit (0, 127, note + 12 * juce::roundToInt (get (octave)));
    const auto region = findRegion (played, velocity);

    if (region < 0)
        return;

    if (juce::roundToInt (get (playMode)) == 0)
    {
        if (auto* voice = freeVoice())
        {
            startVoice (*voice, played, velocity, region, -1.0);
            voice->key = note;
        }

        return;
    }

    // Mono: the keys held (as they came in), the newest last.
    heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());

    if (heldNotes.size() < heldNotes.capacity())
        heldNotes.push_back (note);

    auto* existing = soundingVoice();
    const auto mode = juce::roundToInt (get (monoMode));

    if (existing != nullptr && mode == 0)
    {
        // Legato: the sound goes on, gliding to the new note.
        glideTo (*existing, played, region);
        existing->key = note;
        return;
    }

    if (mode == 1)
    {
        // Retrigger New: the last sound let go fast, a new one starts, gliding from the last note.
        const auto from = existing != nullptr ? existing->pitch : -1.0;

        for (auto& voice : *voices)
            if (voice.active)
                voice.amp.noteOff (5.0f, sampleRate);

        if (auto* voice = freeVoice())
        {
            startVoice (*voice, played, velocity, region, from);
            voice->key = note;
        }

        return;
    }

    if (existing != nullptr)
    {
        // Retrigger Stolen: the sound goes on, its envelopes start again, gliding to the new note.
        const auto& r = instrument->regions[static_cast<size_t> (region)];
        glideTo (*existing, played, region);
        existing->key = note;
        const auto taper = juce::jlimit (0.0f, 1.0f, static_cast<float> (velocity) / static_cast<float> (juce::jmax (1, r.hivel)));
        existing->velocity = velocity;
        existing->velocityGain = 1.0f - r.veltrack * (1.0f - taper);
        retrigger (*existing);
        return;
    }

    if (auto* voice = freeVoice())
    {
        startVoice (*voice, played, velocity, region, -1.0);
        voice->key = note;
    }
}

void SonifySampler::stopNote (int note) noexcept
{
    // Found by the key as it came in: Octave may have changed since it was pressed.
    const auto releaseVoice = [this] (Voice& voice)
    {
        voice.amp.noteOff (static_cast<float> (get (ampRelease)), sampleRate);
        voice.filter.noteOff (static_cast<float> (get (filterRelease)), sampleRate);
        voice.pitchEnvelope.noteOff (static_cast<float> (get (pitchRelease)), sampleRate);
    };

    if (juce::roundToInt (get (playMode)) == 0)
    {
        for (auto& voice : *voices)
        {
            if (voice.active && voice.key == note && ! voice.amp.isReleasing() && ! voice.sustained)
            {
                if (pedalDown)
                    voice.sustained = true;
                else
                    releaseVoice (voice);
            }
        }

        return;
    }

    heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());

    if (heldNotes.empty())
    {
        for (auto& voice : *voices)
            if (voice.active)
                releaseVoice (voice);

        return;
    }

    // Back to the note still held (in the octave set now).
    const auto backKey = heldNotes.back();
    const auto back = juce::jlimit (0, 127, backKey + 12 * juce::roundToInt (get (octave)));
    const auto region = findRegion (back, 64);
    auto* existing = soundingVoice();

    if (region < 0 || existing == nullptr)
        return;

    glideTo (*existing, back, region);
    existing->key = backKey;
    const auto mode = juce::roundToInt (get (monoMode));

    if (mode != 0)
    {
        retrigger (*existing);

        if (mode == 1)
            existing->position = static_cast<double> (instrument->regions[static_cast<size_t> (region)].offset);
    }
}

void SonifySampler::releaseAll (float releaseMs) noexcept
{
    for (auto& voice : *voices)
    {
        if (voice.active)
        {
            voice.sustained = false;
            voice.amp.noteOff (releaseMs, sampleRate);
        }
    }
}

void SonifySampler::pedal (bool down) noexcept
{
    pedalDown = down;

    if (down)
        return;

    for (auto& voice : *voices)
    {
        if (voice.active && voice.sustained)
        {
            voice.sustained = false;
            voice.amp.noteOff (static_cast<float> (get (ampRelease)), sampleRate);
            voice.filter.noteOff (static_cast<float> (get (filterRelease)), sampleRate);
            voice.pitchEnvelope.noteOff (static_cast<float> (get (pitchRelease)), sampleRate);
        }
    }
}

void SonifySampler::render (Voice& voice, float* left, float* right, int numSamples) noexcept
{
    if (! juce::isPositiveAndBelow (voice.region, static_cast<int> (instrument->regions.size())))
    {
        voice.active = false;
        return;
    }

    const auto& r = instrument->regions[static_cast<size_t> (voice.region)];

    if (! juce::isPositiveAndBelow (r.sample, static_cast<int> (instrument->samples.size())))
    {
        voice.active = false;
        return;
    }

    const auto& sample = instrument->samples[static_cast<size_t> (r.sample)];
    const auto length = static_cast<juce::int64> (sample.audio.getNumSamples());
    const auto* dataLeft = sample.audio.getReadPointer (0);
    const auto* dataRight = sample.audio.getNumChannels() > 1 ? sample.audio.getReadPointer (1) : dataLeft;

    if (length < 2)
    {
        voice.active = false;
        return;
    }

    // This block's settings.
    const auto keytrack = static_cast<float> (get (pitchKeytrack)) / 100.0f;
    const auto veltrack = static_cast<float> (get (pitchVeltrack)) / 100.0f;
    const auto bendSemitones = bend * static_cast<float> (get (bendRange));
    const auto envDepth = static_cast<float> (get (pitchDepth));
    const auto vibrato = static_cast<float> (get (pitchLfoDepth));
    const auto tremoloDepth = static_cast<float> (get (ampLfoDepth));
    const auto type = juce::roundToInt (get (filterType));
    const auto baseCutoff = static_cast<float> (get (cutoff));
    const auto reso = static_cast<float> (get (resonance));
    const auto filterKeys = static_cast<float> (get (filterKeytrack));
    const auto envAmount = static_cast<float> (get (filterAmount));
    const auto filterLfoDepthNow = static_cast<float> (get (filterLfoDepth));
    const bool filterOn = type != 0 || baseCutoff < 19999.0f || reso > 0.001f || std::abs (envAmount) > 0.001f || filterLfoDepthNow > 0.001f;
    const auto nyquistish = static_cast<float> (sampleRate * 0.49);
    const auto fullScale = static_cast<float> (sampleRate);

    // loop_sustain: round the loop while the note is held, on past it once it's let go (as the
    // standard and the blob player have it); loop_continuous: round it to the end.
    const bool looping = r.loop && ! (r.loopSustain && voice.amp.isReleasing());
    const auto loopLength = static_cast<double> (r.loopEnd - r.loopStart);

    for (int i = 0; i < numSamples; ++i)
    {
        // A few times a millisecond: the pitch (glide, bend, envelope, vibrato), the tremolo, the filter.
        if (--voice.countdown < 0)
        {
            voice.countdown = controlInterval - 1;

            if (voice.gliding)
            {
                const auto difference = voice.target - voice.pitch;
                const auto stepNow = voice.glideStep * controlInterval;

                if (std::abs (difference) <= stepNow)
                {
                    voice.pitch = voice.target;
                    voice.gliding = false;
                }
                else
                {
                    voice.pitch += difference > 0.0 ? stepNow : -stepNow;
                }
            }

            auto semitones = static_cast<float> (voice.pitch - r.keycenter) * keytrack + r.tune / 100.0f
                           + veltrack * static_cast<float> (voice.velocity) / 127.0f + bendSemitones
                           + voice.pitchEnvelope.level * envDepth;

            if (vibrato > 0.001f)
                semitones += voice.pitchLfo.next (controlInterval) * vibrato;

            voice.step = std::pow (2.0, static_cast<double> (semitones) / 12.0) * sample.rate / sampleRate;

            voice.tremolo = tremoloDepth > 0.001f
                          ? 1.0f - tremoloDepth * 0.5f + tremoloDepth * 0.5f * voice.ampLfo.next (controlInterval)
                          : 1.0f;

            if (filterOn)
            {
                auto frequency = baseCutoff;

                if (filterKeys > 0.1f)
                    frequency *= std::pow (2.0f, static_cast<float> (voice.note - 60) * filterKeys / 1200.0f);

                if (std::abs (envAmount) > 0.001f)
                    frequency += voice.filter.level * envAmount * 10000.0f;

                if (filterLfoDepthNow > 0.001f)
                    frequency *= 1.0f + voice.filterLfo.next (controlInterval) * filterLfoDepthNow;

                frequency = juce::jlimit (20.0f, juce::jmin (20000.0f, nyquistish), frequency);
                const auto g = std::tan (juce::MathConstants<float>::pi * frequency / fullScale);
                voice.k = 2.0f - 2.0f * juce::jlimit (0.0f, 0.99f, reso);
                voice.a1 = 1.0f / (1.0f + g * (g + voice.k));
                voice.a2 = g * voice.a1;
                voice.a3 = g * voice.a2;

                if (! voice.filtering)
                {
                    voice.left.reset();
                    voice.right.reset();
                }
            }

            voice.filtering = filterOn;
        }

        const auto ampLevel = voice.amp.process();
        voice.filter.process();
        voice.pitchEnvelope.process();

        if (! voice.amp.isActive())
        {
            voice.active = false;
            return;
        }

        // Its end: round its loop (before the sample's end is looked at: the loop ends at its last
        // frame at the most), or done.
        if (looping && voice.position >= static_cast<double> (r.loopEnd))
        {
            voice.position -= loopLength;

            if (voice.position >= static_cast<double> (r.loopEnd))   // a step longer than the loop
                voice.position = static_cast<double> (r.loopStart) + std::fmod (voice.position - static_cast<double> (r.loopStart), loopLength);
        }

        if (voice.position >= static_cast<double> (length - 1))
        {
            voice.active = false;
            return;
        }

        auto sampleLeft = sampleAt (dataLeft, length, voice.position);
        auto sampleRight = dataRight == dataLeft ? sampleLeft : sampleAt (dataRight, length, voice.position);

        // The loop's crossfade: its end fades into what comes before its start, so the turn is
        // smooth (equal power).
        if (looping && r.loopFade > 0 && voice.position >= static_cast<double> (r.loopEnd - r.loopFade))
        {
            const auto t = static_cast<float> ((voice.position - static_cast<double> (r.loopEnd - r.loopFade)) / static_cast<double> (r.loopFade));
            const auto back = voice.position - static_cast<double> (r.loopEnd - r.loopStart);
            const auto fadeOut = std::cos (t * juce::MathConstants<float>::halfPi);
            const auto fadeIn = std::sin (t * juce::MathConstants<float>::halfPi);
            sampleLeft = sampleLeft * fadeOut + sampleAt (dataLeft, length, back) * fadeIn;
            sampleRight = dataRight == dataLeft ? sampleLeft : sampleRight * fadeOut + sampleAt (dataRight, length, back) * fadeIn;
        }

        voice.position += voice.step;

        if (voice.filtering)
        {
            sampleLeft = voice.left.process (sampleLeft, voice.a1, voice.a2, voice.a3, voice.k, type);
            sampleRight = voice.right.process (sampleRight, voice.a1, voice.a2, voice.a3, voice.k, type);
        }

        const auto gain = voice.velocityGain * r.gain * ampLevel * voice.tremolo;
        left[i] += sampleLeft * gain;
        right[i] += sampleRight * gain;
    }
}
