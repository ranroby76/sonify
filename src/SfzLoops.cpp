// D:\workspace\Fanan_web_hub\src\SfzLoops.cpp

#include "SfzLoops.h"
#include "SfzFile.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace
{
    using Text = std::u32string;

    Text toText (const juce::String& s)
    {
        Text t;
        t.reserve (static_cast<size_t> (s.length()));

        for (auto p = s.getCharPointer(); ! p.isEmpty();)
            t.push_back (static_cast<char32_t> (p.getAndAdvance()));

        return t;
    }

    juce::String fromText (const Text& t, size_t start, size_t end)
    {
        juce::String s;
        s.preallocateBytes (end - start + 1);

        for (auto i = start; i < end && i < t.size(); ++i)
            s += static_cast<juce::juce_wchar> (t[i]);

        return s;
    }

    juce::String fromText (const Text& t)
    {
        return fromText (t, 0, t.size());
    }

    bool isSpace (char32_t c) noexcept    { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

    bool isNameChar (char32_t c) noexcept
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '$';
    }

    /** The text with its comments (// to the line's end, and block comments) made spaces. */
    Text masked (const Text& t)
    {
        auto m = t;

        for (size_t i = 0; i + 1 < m.size(); ++i)
        {
            if (m[i] == '/' && m[i + 1] == '/')
            {
                while (i < m.size() && m[i] != '\n')
                    m[i++] = ' ';
            }
            else if (m[i] == '/' && m[i + 1] == '*')
            {
                while (i < m.size() && ! (m[i] == '*' && i + 1 < m.size() && m[i + 1] == '/'))
                {
                    if (m[i] != '\n')
                        m[i] = ' ';

                    ++i;
                }

                if (i + 1 < m.size())
                {
                    m[i] = ' ';
                    m[i + 1] = ' ';
                    ++i;
                }
            }
        }

        return m;
    }

    /** Opcodes whose values may hold spaces: they run to the line's end (or the next opcode). */
    bool isPathOpcode (const juce::String& name)
    {
        return name == "sample" || name == "default_path" || name.startsWith ("label") || name.endsWith ("_label") || name == "image";
    }

    /** A key as the SFZ may write it: a number, or a note name (c4 is 60, c#4 and db4 61). */
    int keyValue (const juce::String& value, int fallback)
    {
        const auto v = value.trim().toLowerCase();

        if (v.isEmpty())
            return fallback;

        if (v[0] == '-' || juce::CharacterFunctions::isDigit (v[0]))
            return juce::jlimit (0, 127, v.getIntValue());

        static const int semitones[] { 9, 11, 0, 2, 4, 5, 7 };   // a b c d e f g

        if (v[0] < 'a' || v[0] > 'g')
            return fallback;

        auto note = semitones[v[0] - 'a'];
        int at = 1;

        if (at < v.length() && v[at] == '#')
        {
            ++note;
            ++at;
        }
        else if (at < v.length() && v[at] == 'b' && at + 1 < v.length() && (v[at + 1] == '-' || juce::CharacterFunctions::isDigit (v[at + 1])))
        {
            --note;
            ++at;
        }

        const auto octave = v.substring (at).getIntValue();
        return juce::jlimit (0, 127, (octave + 1) * 12 + note);
    }

    juce::File resolve (const juce::File& folder, const juce::String& path)
    {
        auto native = path.trim().replaceCharacter ('\\', '/');

        if (native.isEmpty())
            return folder;

        if (juce::File::isAbsolutePath (native.replaceCharacter ('/', juce::File::getSeparatorChar())))
            return juce::File (native.replaceCharacter ('/', juce::File::getSeparatorChar()));

        auto file = folder;

        for (const auto& part : juce::StringArray::fromTokens (native, "/", ""))
        {
            if (part.isEmpty() || part == ".")
                continue;

            file = part == ".." ? file.getParentDirectory() : file.getChildFile (part);
        }

        return file;
    }

    bool isLoopMode (const juce::String& mode)
    {
        return mode == "loop_sustain" || mode == "loop_continuous";
    }

    bool isLoopOpcode (const juce::String& name)
    {
        return name == "loop_mode" || name == "loopmode" || name == "loop_start" || name == "loopstart"
            || name == "loop_end" || name == "loopend" || name == "loop_crossfade";
    }

    double fileRate (const juce::File& file)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        return reader != nullptr ? reader->sampleRate : 44100.0;
    }

    juce::String number (double value, int decimals)
    {
        return juce::String (value, decimals);
    }
}

//==============================================================================
bool SfzLoops::load (const juce::File& sfz, Patch& patch, juce::String& why)
{
    patch = {};

    if (! sfz.existsAsFile())
    {
        why = "File not found: " + sfz.getFileName();
        return false;
    }

    const auto raw = sfz.loadFileAsString();
    patch.file = sfz;
    patch.crlf = raw.contains ("\r\n");
    patch.text = raw.replace ("\r\n", "\n").replace ("\r", "\n");

    const auto t = toText (patch.text);
    const auto m = masked (t);
    const auto n = m.size();
    const auto sfzFolder = sfz.getParentDirectory();
    auto defaultFolder = sfzFolder;

    enum class Section { none, control, global, master, group, region, other };
    auto section = Section::none;
    std::map<juce::String, juce::String> global, master, group, own;
    bool inRegion = false;
    Region region;

    const auto close = [&] (size_t at)
    {
        if (! inRegion)
            return;

        inRegion = false;
        region.bodyEnd = static_cast<int> (at);

        auto ops = global;

        for (const auto& [k, v] : master) ops[k] = v;
        for (const auto& [k, v] : group)  ops[k] = v;

        const auto inheritedMode = ops.count ("loop_mode") != 0 ? ops["loop_mode"] : (ops.count ("loopmode") != 0 ? ops["loopmode"] : juce::String());
        region.inheritedLoop = isLoopMode (inheritedMode.trim().toLowerCase());

        for (const auto& [k, v] : own) ops[k] = v;

        region.sample = ops["sample"].trim();

        if (region.sample.isEmpty() || region.sample.startsWithChar ('*'))
            return;   // a generator (*sine...), or nothing to play

        region.file = resolve (defaultFolder, region.sample);

        if (ops.count ("key") != 0)
            region.lokey = region.hikey = region.keycenter = keyValue (ops["key"], 60);

        if (ops.count ("lokey") != 0)            region.lokey = keyValue (ops["lokey"], region.lokey);
        if (ops.count ("hikey") != 0)            region.hikey = keyValue (ops["hikey"], region.hikey);
        if (ops.count ("pitch_keycenter") != 0)  region.keycenter = keyValue (ops["pitch_keycenter"], region.keycenter);
        if (ops.count ("lovel") != 0)            region.lovel = juce::jlimit (0, 127, ops["lovel"].getIntValue());
        if (ops.count ("hivel") != 0)            region.hivel = juce::jlimit (0, 127, ops["hivel"].getIntValue());

        const auto pick = [&ops] (const char* a, const char* b) -> juce::String
        {
            if (ops.count (a) != 0) return ops[a];
            if (ops.count (b) != 0) return ops[b];
            return {};
        };

        region.loopMode = pick ("loop_mode", "loopmode").trim().toLowerCase();
        region.loopStart = juce::jmax<juce::int64> (0, pick ("loop_start", "loopstart").getLargeIntValue());
        region.loopEnd = juce::jmax<juce::int64> (0, pick ("loop_end", "loopend").getLargeIntValue());
        region.looped = region.loopEnd > region.loopStart && (isLoopMode (region.loopMode) || region.loopMode.isEmpty());

        if (ops.count ("loop_crossfade") != 0)
        {
            const auto rate = fileRate (region.file);
            region.crossfade = static_cast<int> (std::lround (Sfz::readCrossfade (ops["loop_crossfade"], rate) * rate));
        }

        patch.regions.push_back (region);
    };

    for (size_t i = 0; i < n;)
    {
        const auto c = m[i];

        if (isSpace (c))
        {
            ++i;
            continue;
        }

        if (c == '<')
        {
            auto j = i + 1;

            while (j < n && m[j] != '>' && m[j] != '\n')
                ++j;

            if (j >= n || m[j] != '>')
            {
                ++i;
                continue;
            }

            const auto name = fromText (t, i + 1, j).trim().toLowerCase();
            close (i);
            own.clear();

            if (name == "region")
            {
                section = Section::region;
                inRegion = true;
                region = {};
                region.headerStart = static_cast<int> (i);
                region.bodyStart = static_cast<int> (j + 1);
            }
            else if (name == "group")    { section = Section::group;  group.clear(); }
            else if (name == "master")   { section = Section::master; master.clear(); group.clear(); }
            else if (name == "global")   { section = Section::global; global.clear(); master.clear(); group.clear(); }
            else if (name == "control")  { section = Section::control; }
            else                         { section = Section::other; }

            i = j + 1;
            continue;
        }

        if (c == '#')
        {
            while (i < n && m[i] != '\n')
                ++i;

            continue;
        }

        const auto nameStart = i;

        while (i < n && isNameChar (m[i]))
            ++i;

        if (i == nameStart || i >= n || m[i] != '=')
        {
            while (i < n && ! isSpace (m[i]) && m[i] != '<')
                ++i;

            if (i == nameStart)
                ++i;

            continue;
        }

        const auto name = fromText (t, nameStart, i).toLowerCase();
        const auto valueStart = ++i;
        auto end = i;

        if (isPathOpcode (name))
        {
            // To the line's end, or before the next opcode on it.
            while (end < n && m[end] != '\n' && m[end] != '<')
            {
                if (isSpace (m[end]))
                {
                    auto k = end;

                    while (k < n && (m[k] == ' ' || m[k] == '\t'))
                        ++k;

                    auto q = k;

                    while (q < n && isNameChar (m[q]))
                        ++q;

                    if (q > k && q < n && m[q] == '=')
                        break;
                }

                ++end;
            }

            while (end > valueStart && isSpace (m[end - 1]))
                --end;
        }
        else
        {
            while (end < n && ! isSpace (m[end]) && m[end] != '<')
                ++end;
        }

        const auto value = fromText (t, valueStart, end);

        switch (section)
        {
            case Section::region:  own[name] = value; region.own.push_back ({ name, static_cast<int> (nameStart), static_cast<int> (valueStart), static_cast<int> (end) }); break;
            case Section::group:   group[name] = value; break;
            case Section::master:  master[name] = value; break;
            case Section::global:  global[name] = value; break;
            case Section::control:
            case Section::none:
            case Section::other:
            default:               break;
        }

        if (name == "default_path" && section == Section::control)
        {
            defaultFolder = resolve (sfzFolder, value);
            patch.paths.push_back ({ static_cast<int> (valueStart), static_cast<int> (end), defaultFolder, true, value.containsChar ('\\') });
        }
        else if (name == "sample" && ! value.trim().startsWithChar ('*'))
        {
            patch.paths.push_back ({ static_cast<int> (valueStart), static_cast<int> (end), resolve (defaultFolder, value), false, value.containsChar ('\\') });
        }

        i = juce::jmax (end, valueStart);
    }

    close (n);

    if (patch.regions.empty())
    {
        why = "No regions found in " + sfz.getFileName();
        return false;
    }

    return true;
}

//==============================================================================
juce::String SfzLoops::textFor (const Patch& patch, const juce::File& target)
{
    struct Edit
    {
        int start = 0, end = 0;
        juce::String with;
    };

    std::vector<Edit> edits;
    const auto t = toText (patch.text);
    const auto m = masked (t);

    // The changed regions: their own loop opcodes out (a line left empty goes too), the loop as
    // set in after their last opcode.
    for (const auto& region : patch.regions)
    {
        if (! region.changed)
            continue;

        int insertAt = region.bodyStart;
        const auto firstRemoval = edits.size();

        for (const auto& op : region.own)
        {
            if (isLoopOpcode (op.name))
            {
                auto start = op.start, end = op.end;

                // The spaces before it on its line, and the line itself when nothing else is on it
                // (a comment after it stays).
                while (start > 0 && (t[static_cast<size_t> (start - 1)] == ' ' || t[static_cast<size_t> (start - 1)] == '\t'))
                    --start;

                // The one just before it going out too, up to it (or over its spaces): one edit for
                // both, so neither is lost to the other's overlap.
                const bool joins = edits.size() > firstRemoval && edits.back().end >= start;

                if (joins)
                    start = edits.back().start;

                auto after = end;

                while (after < static_cast<int> (t.size()) && (t[static_cast<size_t> (after)] == ' ' || t[static_cast<size_t> (after)] == '\t'))
                    ++after;

                const bool lineStarts = start == 0 || t[static_cast<size_t> (start - 1)] == '\n';
                const bool lineEnds = after >= static_cast<int> (t.size()) || t[static_cast<size_t> (after)] == '\n';

                if (lineStarts && lineEnds && after < static_cast<int> (m.size()))
                    end = after + 1;
                else if (lineStarts)
                    end = after;

                if (joins)
                    edits.back().end = juce::jmax (edits.back().end, end);
                else
                    edits.push_back ({ start, end, {} });
            }
            else
            {
                insertAt = juce::jmax (insertAt, op.end);
            }
        }

        juce::String loop;

        if (region.looped && region.loopEnd > region.loopStart)
        {
            const auto mode = isLoopMode (region.loopMode) ? region.loopMode : juce::String ("loop_sustain");
            loop << "\nloop_mode=" << mode << "\nloop_start=" << juce::String (region.loopStart) << "\nloop_end=" << juce::String (region.loopEnd);

            if (region.crossfade > 0)
            {
                const auto rate = fileRate (region.file);
                auto seconds = number (region.crossfade / juce::jmax (1.0, rate), 3);

                while (seconds.endsWithChar ('0'))
                    seconds = seconds.dropLastCharacters (1);

                if (seconds.endsWithChar ('.'))
                    seconds = seconds.dropLastCharacters (1);

                loop << "\nloop_crossfade=" << seconds;
            }
        }
        else if (region.inheritedLoop)
        {
            loop << "\nloop_mode=no_loop";
        }

        if (loop.isNotEmpty())
            edits.push_back ({ insertAt, insertAt, loop });
    }

    // Saved into another folder: the paths made relative to it (default_path, when there's one,
    // else each sample's).
    const auto from = patch.file.getParentDirectory();
    const auto to = target.getParentDirectory();

    if (from != to)
    {
        const bool hasDefault = std::any_of (patch.paths.begin(), patch.paths.end(), [] (const Patch::PathOpcode& p) { return p.isDefaultPath; });

        for (const auto& path : patch.paths)
        {
            if (hasDefault != path.isDefaultPath)
                continue;

            const auto original = fromText (t, static_cast<size_t> (path.valueStart), static_cast<size_t> (path.end));

            if (juce::File::isAbsolutePath (original.trim().replaceCharacter ('\\', juce::File::getSeparatorChar()).replaceCharacter ('/', juce::File::getSeparatorChar())))
                continue;

            auto relative = path.points.getRelativePathFrom (to).replaceCharacter ('\\', '/');

            if (path.isDefaultPath && relative.isNotEmpty() && ! relative.endsWithChar ('/'))
                relative << "/";

            if (path.backslash)
                relative = relative.replaceCharacter ('/', '\\');

            edits.push_back ({ path.valueStart, path.end, relative });
        }
    }

    std::sort (edits.begin(), edits.end(), [] (const Edit& a, const Edit& b) { return a.start != b.start ? a.start > b.start : a.end > b.end; });

    auto out = t;
    int lastStart = std::numeric_limits<int>::max();

    for (const auto& edit : edits)
    {
        if (edit.end > lastStart)
            continue;   // overlaps one already made

        out.replace (static_cast<size_t> (edit.start), static_cast<size_t> (edit.end - edit.start), toText (edit.with));
        lastStart = edit.start;
    }

    return fromText (out);
}

juce::Result SfzLoops::save (Patch& patch, const juce::File& target)
{
    if (patch.regions.empty())
        return juce::Result::fail ("Load an SFZ first.");

    const auto text = textFor (patch, target);

    target.getParentDirectory().createDirectory();

    if (! target.replaceWithText (text, false, false, patch.crlf ? "\r\n" : "\n"))
        return juce::Result::fail ("Sonify couldn't write " + target.getFullPathName() + ".");

    // The loops into the samples (a WAV only; other formats keep theirs in the SFZ alone).
    juce::StringArray notWritten;

    for (const auto& region : patch.regions)
    {
        if (! region.file.existsAsFile() || ! region.file.hasFileExtension ("wav"))
            continue;

        if (region.looped && region.loopEnd > region.loopStart)
        {
            if (! Sfz::setWavLoop (region.file, region.keycenter, region.loopStart, region.loopEnd))
                notWritten.addIfNotAlreadyThere (region.file.getFileName());
        }
        else if (region.changed)
        {
            if (! Sfz::setWavLoop (region.file, region.keycenter, -1, -1))
                notWritten.addIfNotAlreadyThere (region.file.getFileName());
        }
    }

    juce::String why;
    const auto keep = patch;

    if (! load (target, patch, why))
    {
        patch = keep;
        return juce::Result::fail (why);
    }

    if (! notWritten.isEmpty())
        return juce::Result::fail ("Saved " + target.getFileName() + ", but the loop couldn't be put in "
                                   + notWritten.joinIntoString (", ") + " (in use by another app?).");

    return juce::Result::ok();
}

//==============================================================================
juce::var SfzLoops::peaks (const juce::AudioBuffer<float>& audio, double sampleRate, int buckets)
{
    auto* o = new juce::DynamicObject();
    const auto frames = audio.getNumSamples();
    const auto channels = juce::jlimit (0, 2, audio.getNumChannels());
    buckets = juce::jlimit (1, 8000, juce::jmin (buckets, juce::jmax (1, frames)));

    o->setProperty ("channels", channels);
    o->setProperty ("frames", frames);
    o->setProperty ("rate", sampleRate);

    juce::Array<juce::var> all;

    for (int channel = 0; channel < channels; ++channel)
    {
        juce::Array<juce::var> list;
        list.ensureStorageAllocated (buckets * 2);
        const auto* data = audio.getReadPointer (channel);

        for (int b = 0; b < buckets; ++b)
        {
            const auto start = static_cast<int> (static_cast<juce::int64> (frames) * b / buckets);
            const auto end = juce::jmax (start + 1, static_cast<int> (static_cast<juce::int64> (frames) * (b + 1) / buckets));
            const auto range = juce::FloatVectorOperations::findMinAndMax (data + start, juce::jmin (end, frames) - start);
            list.add (juce::jlimit (-127, 127, juce::roundToInt (range.getStart() * 127.0f)));
            list.add (juce::jlimit (-127, 127, juce::roundToInt (range.getEnd() * 127.0f)));
        }

        all.add (list);
    }

    o->setProperty ("peaks", all);
    return juce::var (o);
}

double SfzLoops::keyHz (int note)
{
    return 440.0 * std::pow (2.0, (note - 69) / 12.0);
}

//==============================================================================
juce::String SfzLoops::aiGuide()
{
    return
        "You are Fanan AI inside Sonify's Sampling Studio, in its Loop Editor. You choose sustain loop points for "
        "sampled instrument notes: while a key is held, the sampler plays the attack, then repeats the loop until the "
        "key is let go. A good loop is one the listener can't hear: no click, no thump, no pitch jump, no level pump, "
        "no tone change at the seam, and long enough not to sound like a buzzing cycle.\n\n"
        "For each sample you get what it is (its key, its expected pitch, its length, the detected pitch) and a list "
        "of candidate loops found by Sonify's loop finder (each starts and ends on a rising zero crossing and is a "
        "whole number of pitch periods long), with these numbers:\n"
        "- score: the finder's own 0..1 rating (the two ends' waveform likeness, level, value and slope matches, how late it sits).\n"
        "- correlation: how alike the waveform is at the two ends over four periods (-1..1; above 0.95 is good, above 0.98 very good).\n"
        "- levelMatch: the quieter end's level over the louder's (1 = equal; under 0.85 the level will pump each cycle).\n"
        "- valueMatch, slopeMatch: how well the sample value and slope meet at the seam (1 = perfect; under 0.9 risks a click).\n"
        "- seconds, periods: the loop's length (very short loops, under about 0.1 s or 20 periods, can sound static or buzzy; "
        "longer loops keep the natural movement of chorus, vibrato, bowing or breath, but need the sound steady across them).\n"
        "- startSeconds, endSeconds: where it sits (a loop starting inside the attack sounds wrong; one ending past the strong body loops the fade).\n"
        "- startPitchHz, endPitchHz: the pitch at each end (they should match each other, and the expected pitch; a gap of more than "
        "about 0.5% is an audible jump).\n"
        "- levelWobbleDb: how much the level moves inside the loop (a few dB is natural movement; much more will be heard as pumping).\n"
        "- brightnessMatch: how alike the tone (the share of high frequencies) is at the two ends (1 = same; under 0.85 the tone jumps).\n"
        "Candidate 0, when listed, is the loop the sample has now.\n\n"
        "Pick for each sample the candidate that should sound smoothest held, weighing all of them (not only the score): "
        "prefer matched pitch, level and brightness at the seam, then length (longer is better when the sound is steady), "
        "then correlation. Choose the crossfade in milliseconds (20..500): short (20..60) for pure, steady tones (organ, "
        "flute, sine-like synths); medium (60..150) for most instruments; long (150..400) for noisy, chorused, ensemble, "
        "breathy or evolving sounds and for loops whose ends match less well. Answer -1 when no candidate is usable "
        "(a sound that dies away without sustaining, such as a pluck or a drum: it should play without a loop).\n\n"
        "Answer with JSON only, no other text:\n"
        "{\"samples\":[{\"index\":<the sample's index>,\"candidate\":<its number, 0 to keep its loop, -1 for none>,"
        "\"crossfadeMs\":<20..500>,\"why\":\"<one short sentence>\"}]}\n"
        "Give one entry for every sample, in any order.";
}

juce::String SfzLoops::aiRequest (const std::vector<AiSample>& samples)
{
    juce::String text;
    text << "Choose the loop (and crossfade) for each of these " << static_cast<int> (samples.size()) << " samples.\n";

    const auto describe = [] (juce::String& out, int numberShown, const LoopFinder::Candidate& c, double rate)
    {
        out << "  candidate " << numberShown << ": start " << juce::String (c.loopStart) << ", end " << juce::String (c.loopEnd)
            << ", score " << number (c.score, 3) << ", correlation " << number (c.correlation, 3)
            << ", levelMatch " << number (c.levelMatch, 3) << ", valueMatch " << number (c.valueMatch, 3)
            << ", slopeMatch " << number (c.slopeMatch, 3)
            << ", seconds " << number (c.seconds > 0.0 ? c.seconds : static_cast<double> (c.loopEnd - c.loopStart) / rate, 3)
            << ", periods " << number (c.periods, 1)
            << ", startSeconds " << number (c.startSeconds, 3) << ", endSeconds " << number (c.endSeconds, 3)
            << ", startPitchHz " << number (c.startPitchHz, 2) << ", endPitchHz " << number (c.endPitchHz, 2)
            << ", levelWobbleDb " << number (c.levelWobbleDb, 2) << ", brightnessMatch " << number (c.brightnessMatch, 3) << "\n";
    };

    for (const auto& s : samples)
    {
        const auto& f = s.found;
        text << "\nSample index " << s.index << ": \"" << s.name << "\", key " << Sfz::keyName (s.keycenter) << " (" << s.keycenter
             << ", expected " << number (keyHz (s.keycenter), 2) << " Hz), velocities " << s.lovel << "-" << s.hivel
             << ", " << number (s.seconds, 2) << " s, " << s.channels << (s.channels == 1 ? " channel" : " channels")
             << ", rate " << juce::String (juce::roundToInt (f.sampleRate)) << " Hz";

        if (f.success)
            text << ", detected pitch " << number (f.pitchHz, 2) << " Hz, peak " << number (f.peakLevelDb, 1) << " dB"
                 << ", sustain " << number (static_cast<double> (f.sustainStart) / f.sampleRate, 3) << "-"
                 << number (static_cast<double> (f.sustainEnd) / f.sampleRate, 3) << " s, strong body ends "
                 << number (static_cast<double> (f.bodyEnd) / f.sampleRate, 3) << " s";
        else if (f.error.isNotEmpty())
            text << ", the loop finder says: " << f.error;

        text << "\n";

        if (s.hasCurrent)
            describe (text, 0, s.current, f.sampleRate);

        for (size_t i = 0; i < f.candidates.size(); ++i)
            describe (text, static_cast<int> (i) + 1, f.candidates[i], f.sampleRate);

        if (! s.hasCurrent && f.candidates.empty())
            text << "  (no candidates)\n";
    }

    return text;
}

std::vector<SfzLoops::AiPick> SfzLoops::aiPicks (const juce::var& answer, juce::String& why)
{
    std::vector<AiPick> picks;
    juce::String text = answer.getProperty ("reply", {}).toString();

    if (const auto* blocks = answer.getProperty ("content", {}).getArray())
        for (const auto& block : *blocks)
            if (block.getProperty ("type", {}).toString() == "text")
                text << block.getProperty ("text", {}).toString() << "\n";

    const auto start = text.indexOfChar ('{');
    const auto end = text.lastIndexOfChar ('}');

    if (start < 0 || end <= start)
    {
        why = "Fanan AI didn't answer with loop picks this time. Please try again.";
        return picks;
    }

    const auto parsed = juce::JSON::parse (text.substring (start, end + 1));

    if (const auto* list = parsed.getProperty ("samples", {}).getArray())
    {
        for (const auto& item : *list)
        {
            AiPick pick;
            pick.index = static_cast<int> (item.getProperty ("index", -1));
            pick.candidate = static_cast<int> (item.getProperty ("candidate", -1));
            pick.crossfadeMs = juce::jlimit (20, 500, static_cast<int> (item.getProperty ("crossfadeMs", 100)));
            pick.why = item.getProperty ("why", {}).toString().substring (0, 240);

            if (pick.index >= 0)
                picks.push_back (pick);
        }
    }

    if (picks.empty())
        why = "Fanan AI didn't answer with loop picks this time. Please try again.";

    return picks;
}
