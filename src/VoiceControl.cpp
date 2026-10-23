// D:\workspace\Fanan_web_hub\src\VoiceControl.cpp

#include "VoiceControl.h"

#include <algorithm>
#include <cmath>
#include <utility>

//==============================================================================
// Finding phrases
PhraseFinder::PhraseFinder()
{
    prepare (48000.0);
}

void PhraseFinder::prepare (double inputRate)
{
    const auto inRate = inputRate > 1000.0 ? inputRate : 48000.0;
    step = inRate / rate;
    resampling = std::abs (step - 1.0) > 1.0e-9;
    position = 0.0;
    previous = 0.0f;

    // Below 16 kHz's Nyquist (8 kHz) with room to spare: speech recognition needs little above 6 kHz.
    // Two sections make a fourth-order Butterworth low-pass.
    filtering = inRate >= 14000.0;

    if (filtering)
    {
        lowPass1.setCoefficients (juce::IIRCoefficients::makeLowPass (inRate, 6500.0, 0.5412));
        lowPass2.setCoefficients (juce::IIRCoefficients::makeLowPass (inRate, 6500.0, 1.3066));
    }

    highPass.setCoefficients (juce::IIRCoefficients::makeHighPass (static_cast<double> (rate), 100.0, 0.7071));
    lowPass1.reset();
    lowPass2.reset();
    highPass.reset();

    frame.clear();
    frame.reserve (static_cast<size_t> (frameLength));
    preRoll.clear();
    phrase.clear();
    levels.fill (0.0f);
    levelsCount = levelsAt = 0;
    noiseDb = -90.0f;
    inPhrase = dropped = headGiven = false;
    loudRun = quietRun = voicedFrames = phraseFrames = 0;
}

void PhraseFinder::process (const float* samples, int count, bool hushed, std::vector<Phrase>& found)
{
    hushedNow = hushed;

    for (int i = 0; i < count; ++i)
    {
        auto x = samples[i];

        if (! std::isfinite (x))
            x = 0.0f;

        if (filtering)
            x = lowPass2.processSingleSampleRaw (lowPass1.processSingleSampleRaw (x));

        if (! resampling)
        {
            addSample (x, found);
            continue;
        }

        // Linear interpolation between the input sample before (at 0) and this one (at 1).
        while (position < 1.0)
        {
            addSample (previous + (x - previous) * static_cast<float> (position), found);
            position += step;
        }

        position -= 1.0;
        previous = x;
    }
}

void PhraseFinder::addSample (float sample, std::vector<Phrase>& found)
{
    frame.push_back (highPass.processSingleSampleRaw (sample));

    if (static_cast<int> (frame.size()) >= frameLength)
    {
        endFrame (found);
        frame.clear();
    }
}

void PhraseFinder::endFrame (std::vector<Phrase>& found)
{
    double sum = 0.0;

    for (const auto sample : frame)
        sum += static_cast<double> (sample) * static_cast<double> (sample);

    const auto level = static_cast<float> (10.0 * std::log10 (sum / frameLength + 1.0e-12));

    // The room's noise: the quietest frame of the last three seconds (speech always has gaps).
    levels[static_cast<size_t> (levelsAt)] = level;
    levelsAt = (levelsAt + 1) % noiseFrames;
    levelsCount = juce::jmin (levelsCount + 1, noiseFrames);
    noiseDb = *std::min_element (levels.begin(), levels.begin() + levelsCount);

    const auto startLevel = juce::jmax (noiseDb + 12.0f, -50.0f);
    const auto endLevel = juce::jmax (noiseDb + 6.0f, -54.0f);

    if (! inPhrase)
    {
        preRoll.insert (preRoll.end(), frame.begin(), frame.end());

        if (static_cast<int> (preRoll.size()) > preRollFrames * frameLength)
            preRoll.erase (preRoll.begin(), preRoll.begin() + frameLength);

        loudRun = level > startLevel && ! hushedNow ? loudRun + 1 : 0;

        if (loudRun >= startFrames)
        {
            inPhrase = true;
            dropped = headGiven = false;
            phraseId = ++lastId;
            phrase = std::move (preRoll);   // the loud frames are already in it
            preRoll.clear();
            voicedFrames = phraseFrames = loudRun;
            quietRun = loudRun = 0;
        }

        return;
    }

    phrase.insert (phrase.end(), frame.begin(), frame.end());

    // Let go: only its end is kept (and the quiet after it, until it ends).
    if (dropped && static_cast<int> (phrase.size()) > (endKeptFrames + endFrames) * frameLength)
        phrase.erase (phrase.begin(), phrase.begin() + frameLength);

    ++phraseFrames;

    if (level > startLevel)
        ++voicedFrames;

    quietRun = level < endLevel ? quietRun + 1 : 0;

    if (quietRun >= endFrames)
    {
        endPhrase (found, true);
    }
    else if (phraseFrames >= maxFrames)
    {
        endPhrase (found, false);
    }
    else if (! headGiven && phraseFrames >= headFrames)
    {
        // Its head, as it passes three seconds (not later: heads switched on mid-phrase give none).
        headGiven = true;

        if (wantsHeads && ! dropped && voicedFrames >= minVoicedFrames)
            found.push_back ({ phraseId, Kind::head, phrase });
    }
}

void PhraseFinder::endPhrase (std::vector<Phrase>& found, bool quietAtEnd)
{
    if (voicedFrames >= minVoicedFrames)
    {
        // Most of the quiet after it goes: it's no use to speech recognition.
        if (quietAtEnd)
        {
            const auto extra = static_cast<size_t> ((endFrames - tailFrames) * frameLength);

            if (phrase.size() > extra)
                phrase.resize (phrase.size() - extra);
        }

        found.push_back ({ phraseId, dropped ? Kind::end : Kind::whole, std::move (phrase) });
    }

    phrase.clear();
    inPhrase = dropped = headGiven = false;
    loudRun = quietRun = voicedFrames = phraseFrames = 0;
}

void PhraseFinder::drop (int id)
{
    if (inPhrase && id == phraseId && ! dropped)
    {
        dropped = true;

        const auto keep = static_cast<size_t> ((endKeptFrames + endFrames) * frameLength);

        if (phrase.size() > keep)
            phrase.erase (phrase.begin(), phrase.end() - static_cast<std::ptrdiff_t> (keep));
    }
}

PhraseFinder::Phrase PhraseFinder::endOf (Phrase whole)
{
    // Its last seconds and the bit of quiet after them, as a phrase let go keeps them.
    const auto keep = static_cast<size_t> ((endKeptFrames + tailFrames) * frameLength);

    if (whole.samples.size() > keep)
        whole.samples.erase (whole.samples.begin(), whole.samples.end() - static_cast<std::ptrdiff_t> (keep));

    whole.kind = Kind::end;
    return whole;
}

//==============================================================================
// Reading what was said
namespace
{
    // The Hebrew words are written as their UTF-8 bytes, so this file stays plain ASCII.
    juce::StringArray utf8Words (std::initializer_list<const char*> items)
    {
        juce::StringArray words;

        for (const auto* item : items)
            words.add (juce::String::fromUTF8 (item));

        return words;
    }

    /** Kept for comparing: letters (any script) and digits, not punctuation, quotes or Hebrew vowel points. */
    bool keepsChar (juce::juce_wchar c) noexcept
    {
        if (c < 128)
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');

        if (c <= 0xbf || c == 0xd7 || c == 0xf7)        return false;   // Latin-1 signs
        if (c >= 0x0591 && c <= 0x05c7)                 return false;   // Hebrew points and marks
        if (c == 0x05f3 || c == 0x05f4)                 return false;   // geresh, gershayim
        if (c >= 0x2000 && c <= 0x206f)                 return false;   // dashes, quotes, ellipsis
        if (c >= 0x3000 && c <= 0x303f)                 return false;   // CJK punctuation

        return true;
    }

    bool isHebrew (juce::juce_wchar c) noexcept     { return c >= 0x05d0 && c <= 0x05ea; }

    /** Edits between two words: letters added, taken out or changed. */
    int distance (const juce::String& a, const juce::String& b)
    {
        std::vector<juce::juce_wchar> x, y;

        for (auto p = a.getCharPointer(); ! p.isEmpty();)  x.push_back (p.getAndAdvance());
        for (auto p = b.getCharPointer(); ! p.isEmpty();)  y.push_back (p.getAndAdvance());

        std::vector<int> row (y.size() + 1);

        for (size_t j = 0; j < row.size(); ++j)
            row[j] = static_cast<int> (j);

        for (size_t i = 1; i <= x.size(); ++i)
        {
            auto diagonal = row[0];
            row[0] = static_cast<int> (i);

            for (size_t j = 1; j <= y.size(); ++j)
            {
                const auto above = row[j];
                row[j] = std::min ({ row[j] + 1, row[j - 1] + 1, diagonal + (x[i - 1] == y[j - 1] ? 0 : 1) });
                diagonal = above;
            }
        }

        return row.back();
    }

    // "Sonify" in Hebrew letters (sonifai), as speech recognition writes it in a Hebrew sentence.
    const juce::String& hebrewSonify()
    {
        static const auto word = juce::String::fromUTF8 ("\xd7\xa1\xd7\x95\xd7\xa0\xd7\x99\xd7\xa4\xd7\x99\xd7\x99");
        return word;
    }

    // "Sonify" as speech recognition may write it; in Hebrew: sonifi, sonifai with alef, sonfai, sonifaii.
    const juce::StringArray& wakeWords()
    {
        static const auto words = []
        {
            auto list = utf8Words ({
                "sonify", "sonifi", "sonifie", "sonifye", "sonifai", "sonifay", "sonnify", "sonofy", "sonafy", "sonefy",
                "sunify", "sanify", "sonyfi", "sonyfy", "spotify",
                "\xd7\xa1\xd7\x95\xd7\xa0\xd7\x99\xd7\xa4\xd7\x99",
                "\xd7\xa1\xd7\x95\xd7\xa0\xd7\x99\xd7\xa4\xd7\x90\xd7\x99",
                "\xd7\xa1\xd7\x95\xd7\xa0\xd7\xa4\xd7\x99\xd7\x99",
                "\xd7\xa1\xd7\x95\xd7\xa0\xd7\x99\xd7\xa4\xd7\x90\xd7\x99\xd7\x99" });
            list.add (hebrewSonify());
            return list;
        }();

        return words;
    }

    bool isWake (const juce::String& word)
    {
        if (word.isEmpty())
            return false;

        if (wakeWords().contains (word))
            return true;

        // One letter off: "sonifu", "zonify" and the like (no common word is that close).
        const auto length = word.length();

        if (word[0] < 128 && length >= 5 && length <= 8)
            return distance (word, "sonify") <= 1;

        if (isHebrew (word[0]) && length >= 5 && length <= 9)
            return distance (word, hebrewSonify()) <= 1;

        return false;
    }

    // "Over" as speech recognition may write it: over, and in Hebrew over (alef), over (ayin, "passing"), avur.
    const juce::StringArray& stopWords()
    {
        static const auto words = utf8Words ({
            "over", "ova", "ovah", "overr",
            "\xd7\x90\xd7\x95\xd7\x91\xd7\xa8",
            "\xd7\xa2\xd7\x95\xd7\x91\xd7\xa8",
            "\xd7\xa2\xd7\x91\xd7\x95\xd7\xa8" });

        return words;
    }

    // Words that don't change a short command: please, now, it, that... In Hebrew: please, now,
    // the object marker, this, to me, come on, can you, possible.
    const juce::StringArray& fillers()
    {
        static const auto words = utf8Words ({
            "please", "now", "ok", "okay", "hey", "just", "the", "it", "that", "this", "can", "could", "would", "will",
            "you", "for", "me", "us", "sonify",
            "\xd7\x91\xd7\x91\xd7\xa7\xd7\xa9\xd7\x94",
            "\xd7\xa2\xd7\x9b\xd7\xa9\xd7\x99\xd7\x95",
            "\xd7\x90\xd7\xaa",
            "\xd7\x96\xd7\x94",
            "\xd7\x9c\xd7\x99",
            "\xd7\x99\xd7\x90\xd7\x9c\xd7\x9c\xd7\x94",
            "\xd7\xaa\xd7\x95\xd7\x9b\xd7\x9c",
            "\xd7\x90\xd7\xa4\xd7\xa9\xd7\xa8" });

        return words;
    }

    struct QuickPhrases
    {
        juce::StringArray phrases;
        std::vector<VoiceControl::Quick> commands;

        void add (VoiceControl::Quick command, std::initializer_list<const char*> items)
        {
            for (const auto* item : items)
            {
                phrases.add (juce::String::fromUTF8 (item));
                commands.push_back (command);
            }
        }
    };

    // Each command as musicians say it, after the filler words are taken out.
    const QuickPhrases& quickPhrases()
    {
        static const QuickPhrases table = []
        {
            using Quick = VoiceControl::Quick;
            QuickPhrases t;

            t.add (Quick::record, { "record", "recording", "rec", "start recording", "start record", "begin recording",
                                    "lets record", "let record", "hit record", "punch in",
                                    "\xd7\x94\xd7\xa7\xd7\x9c\xd7\x98",                                     // haklet
                                    "\xd7\x94\xd7\xa7\xd7\x9c\xd7\x98\xd7\x94",                             // haklata
                                    "\xd7\xaa\xd7\xa7\xd7\x9c\xd7\x99\xd7\x98",                             // takLIT
                                    "\xd7\xaa\xd7\xa7\xd7\x9c\xd7\x99\xd7\x98\xd7\x99",                     // takLIti
                                    "\xd7\x9c\xd7\x94\xd7\xa7\xd7\x9c\xd7\x99\xd7\x98",                     // lehakLIT
                                    "\xd7\xaa\xd7\xaa\xd7\x97\xd7\x99\xd7\x9c\x20\xd7\x9c\xd7\x94\xd7\xa7\xd7\x9c\xd7\x99\xd7\x98",
                                    "\xd7\xaa\xd7\xaa\xd7\x97\xd7\x99\xd7\x9c\xd7\x99\x20\xd7\x9c\xd7\x94\xd7\xa7\xd7\x9c\xd7\x99\xd7\x98",
                                    "\xd7\x94\xd7\xaa\xd7\x97\xd7\x9c\x20\xd7\x94\xd7\xa7\xd7\x9c\xd7\x98\xd7\x94",
                                    "\xd7\xaa\xd7\xaa\xd7\x97\xd7\x99\xd7\x9c\x20\xd7\x94\xd7\xa7\xd7\x9c\xd7\x98\xd7\x94",
                                    "\xd7\xa8\xd7\xa7\xd7\x95\xd7\xa8\xd7\x93" });                         // "record"

            t.add (Quick::stop, { "stop", "stop stop", "stop recording", "stop record", "stop playing", "stop playback",
                                  "stop everything", "halt", "cut", "enough", "pause",
                                  "\xd7\xa2\xd7\xa6\xd7\x95\xd7\xa8",                                       // atsor
                                  "\xd7\xa2\xd7\xa6\xd7\xa8\xd7\x99",
                                  "\xd7\xaa\xd7\xa2\xd7\xa6\xd7\x95\xd7\xa8",
                                  "\xd7\xaa\xd7\xa2\xd7\xa6\xd7\xa8\xd7\x99",
                                  "\xd7\xa1\xd7\x98\xd7\x95\xd7\xa4",                                       // "stop"
                                  "\xd7\x93\xd7\x99",                                                       // dai (enough)
                                  "\xd7\xa2\xd7\xa6\xd7\x95\xd7\xa8\x20\xd7\x94\xd7\xa7\xd7\x9c\xd7\x98\xd7\x94",
                                  "\xd7\xaa\xd7\xa2\xd7\xa6\xd7\x95\xd7\xa8\x20\xd7\x94\xd7\xa7\xd7\x9c\xd7\x98\xd7\x94",
                                  "\xd7\xa2\xd7\xa6\xd7\x95\xd7\xa8\x20\xd7\x94\xd7\x94\xd7\xa7\xd7\x9c\xd7\x98\xd7\x94",
                                  "\xd7\xaa\xd7\xa2\xd7\xa6\xd7\x95\xd7\xa8\x20\xd7\x94\xd7\x94\xd7\xa7\xd7\x9c\xd7\x98\xd7\x94",
                                  "\xd7\xa2\xd7\xa6\xd7\x95\xd7\xa8\x20\xd7\xa0\xd7\x92\xd7\x99\xd7\xa0\xd7\x94",
                                  "\xd7\xaa\xd7\xa2\xd7\xa6\xd7\x95\xd7\xa8\x20\xd7\xa0\xd7\x92\xd7\x99\xd7\xa0\xd7\x94" });

            t.add (Quick::play, { "play", "playback", "play back", "start playback", "start playing", "start play",
                                  "play song", "let me hear", "lets hear", "let hear", "listen", "resume", "roll", "go",
                                  "\xd7\xa0\xd7\x92\xd7\x9f",                                               // nagen
                                  "\xd7\xaa\xd7\xa0\xd7\x92\xd7\x9f",
                                  "\xd7\xa0\xd7\x92\xd7\xa0\xd7\x99",
                                  "\xd7\xaa\xd7\xa0\xd7\x92\xd7\xa0\xd7\x99",
                                  "\xd7\x94\xd7\xa4\xd7\xa2\xd7\x9c",                                       // hafel
                                  "\xd7\xaa\xd7\xa4\xd7\xa2\xd7\x99\xd7\x9c",
                                  "\xd7\xa4\xd7\x9c\xd7\x99\xd7\x99",                                       // "play"
                                  "\xd7\x94\xd7\xa9\xd7\x9e\xd7\xa2",                                       // hashma
                                  "\xd7\xaa\xd7\xa9\xd7\x9e\xd7\x99\xd7\xa2",
                                  "\xd7\xaa\xd7\xa0\xd7\x92\xd7\x9f\x20\xd7\xa9\xd7\x99\xd7\xa8" });

            t.add (Quick::again, { "again", "one more", "one more time", "once more", "once again", "do again", "go again",
                                   "lets go again", "let go again", "take again", "another take", "another one", "repeat",
                                   "same again",
                                   "\xd7\xa9\xd7\x95\xd7\x91",                                              // shuv
                                   "\xd7\xa2\xd7\x95\xd7\x93\x20\xd7\xa4\xd7\xa2\xd7\x9d",                  // od pa'am
                                   "\xd7\xa2\xd7\x95\xd7\x93\x20\xd7\xa4\xd7\xa2\xd7\x9d\x20\xd7\x90\xd7\x97\xd7\xaa",
                                   "\xd7\xa4\xd7\xa2\xd7\x9d\x20\xd7\xa0\xd7\x95\xd7\xa1\xd7\xa4\xd7\xaa",
                                   "\xd7\xa2\xd7\x95\xd7\x93\x20\xd7\x98\xd7\x99\xd7\x99\xd7\xa7",          // od take
                                   "\xd7\xa9\xd7\x95\xd7\x91\x20\xd7\xa4\xd7\xa2\xd7\x9d",
                                   "\xd7\x9e\xd7\x97\xd7\x93\xd7\xa9",                                      // mehadash
                                   "\xd7\xa2\xd7\x95\xd7\x93\x20\xd7\x90\xd7\x97\xd7\x93",
                                   "\xd7\xa2\xd7\x95\xd7\x93\x20\xd7\x90\xd7\x97\xd7\xaa" });

            t.add (Quick::recordAgain, { "record again", "rerecord", "re record", "record one more", "record another take",
                                         "record once more", "new take",
                                         "\xd7\x94\xd7\xa7\xd7\x9c\xd7\x98\x20\xd7\xa9\xd7\x95\xd7\x91",
                                         "\xd7\xaa\xd7\xa7\xd7\x9c\xd7\x99\xd7\x98\x20\xd7\xa9\xd7\x95\xd7\x91",
                                         "\xd7\xaa\xd7\xa7\xd7\x9c\xd7\x99\xd7\x98\x20\xd7\xa2\xd7\x95\xd7\x93\x20\xd7\xa4\xd7\xa2\xd7\x9d",
                                         "\xd7\x94\xd7\xa7\xd7\x9c\xd7\x98\x20\xd7\xa2\xd7\x95\xd7\x93\x20\xd7\xa4\xd7\xa2\xd7\x9d",
                                         "\xd7\xa2\xd7\x95\xd7\x93\x20\xd7\x94\xd7\xa7\xd7\x9c\xd7\x98\xd7\x94",
                                         "\xd7\x94\xd7\xa7\xd7\x9c\xd7\x98\xd7\x94\x20\xd7\xa0\xd7\x95\xd7\xa1\xd7\xa4\xd7\xaa" });

            t.add (Quick::playAgain, { "play again", "replay", "play one more time", "play once more",
                                       "\xd7\xa0\xd7\x92\xd7\x9f\x20\xd7\xa9\xd7\x95\xd7\x91",
                                       "\xd7\xaa\xd7\xa0\xd7\x92\xd7\x9f\x20\xd7\xa9\xd7\x95\xd7\x91",
                                       "\xd7\xaa\xd7\xa0\xd7\x92\xd7\x9f\x20\xd7\xa2\xd7\x95\xd7\x93\x20\xd7\xa4\xd7\xa2\xd7\x9d",
                                       "\xd7\xa0\xd7\x92\xd7\x9f\x20\xd7\xa2\xd7\x95\xd7\x93\x20\xd7\xa4\xd7\xa2\xd7\x9d" });

            t.add (Quick::undo, { "undo", "undo last", "undo last thing", "take back", "scratch",
                                  "\xd7\x91\xd7\x98\xd7\x9c",                                               // batel
                                  "\xd7\x91\xd7\x99\xd7\x98\xd7\x95\xd7\x9c",
                                  "\xd7\xaa\xd7\x91\xd7\x98\xd7\x9c",
                                  "\xd7\xaa\xd7\x91\xd7\x98\xd7\x9c\xd7\x99",
                                  "\xd7\x90\xd7\xa0\xd7\x93\xd7\x95",                                       // "undo"
                                  "\xd7\x91\xd7\x98\xd7\x9c\x20\xd7\xa4\xd7\xa2\xd7\x95\xd7\x9c\xd7\x94",
                                  "\xd7\xaa\xd7\x91\xd7\x98\xd7\x9c\x20\xd7\xa4\xd7\xa2\xd7\x95\xd7\x9c\xd7\x94",
                                  "\xd7\x91\xd7\x98\xd7\x9c\x20\xd7\x90\xd7\x97\xd7\xa8\xd7\x95\xd7\x9f" });
            return t;
        }();

        return table;
    }
}

VoiceControl::Words VoiceControl::read (const juce::String& text)
{
    Words words;

    for (const auto& token : juce::StringArray::fromTokens (text, " \t\r\n", {}))
    {
        const auto lower = token.toLowerCase();
        juce::String plain;

        for (auto p = lower.getCharPointer(); ! p.isEmpty();)
            if (const auto c = p.getAndAdvance(); keepsChar (c))
                plain += c;

        if (plain.isEmpty())
            continue;   // a dash or a dot on its own

        words.spoken.add (token.trim());
        words.plain.add (plain);
    }

    return words;
}

int VoiceControl::afterWake (const Words& words)
{
    const auto& plain = words.plain;
    const auto count = plain.size();

    for (int i = 0; i < juce::jmin (wakeWithin, count); ++i)
    {
        int next = -1;

        if (isWake (plain[i]))
            next = i + 1;
        else if (i + 1 < count && isWake (plain[i] + plain[i + 1]))   // "so nify"
            next = i + 2;

        if (next < 0)
            continue;

        while (next < count && isWake (plain[next]))   // "Sonify, Sonify..."
            ++next;

        return next;
    }

    return -1;
}

int VoiceControl::afterLastWake (const Words& words)
{
    const auto& plain = words.plain;

    for (int i = plain.size(); --i >= 0;)
    {
        if (isWake (plain[i]))
            return i + 1;

        if (i + 1 < plain.size() && isWake (plain[i] + plain[i + 1]))
            return i + 2;
    }

    return -1;
}

int VoiceControl::stopLength (const Words& words, int from)
{
    const auto& plain = words.plain;
    const auto end = plain.size();

    if (end - from >= 3 && plain[end - 3] == "over" && plain[end - 2] == "and" && plain[end - 1] == "out")
        return 3;

    // Just the last one: "Sonify, start over, over" is "start over".
    return end - 1 >= from && stopWords().contains (plain[end - 1]) ? 1 : 0;
}

juce::String VoiceControl::join (const Words& words, int from, int to)
{
    juce::StringArray chosen;

    for (int i = juce::jmax (0, from); i < juce::jmin (to, words.spoken.size()); ++i)
        chosen.add (words.spoken[i]);

    // "Sonify, record, over.": the commas are the pauses around the command, not part of it.
    static const auto around = juce::String::fromUTF8 (",;:-\xe2\x80\x93\xe2\x80\x94 ");
    static const auto before = around + ".!?";

    return chosen.joinIntoString (" ").trim().trimCharactersAtStart (before).trimCharactersAtEnd (around).trim();
}

VoiceControl::Quick VoiceControl::quickCommand (const juce::String& command)
{
    const auto words = read (command);
    juce::StringArray kept;

    for (const auto& word : words.plain)
        if (! fillers().contains (word))
            kept.add (word);

    const auto& table = quickPhrases();
    const auto index = table.phrases.indexOf (kept.joinIntoString (" "));

    return index >= 0 ? table.commands[static_cast<size_t> (index)] : Quick::none;
}

juce::MemoryBlock VoiceControl::toWav (const std::vector<float>& samples)
{
    float peak = 0.0f;

    for (const auto sample : samples)
        peak = juce::jmax (peak, std::abs (sample));

    // Up near full scale (a mic set for singing hears speech quietly), but by 30 dB at most.
    const auto gain = peak > 0.0f ? juce::jmin (0.7f / peak, 31.6f) : 1.0f;
    const auto dataBytes = static_cast<int> (samples.size() * 2);

    juce::MemoryOutputStream out;
    out.write ("RIFF", 4);
    out.writeInt (36 + dataBytes);
    out.write ("WAVE", 4);
    out.write ("fmt ", 4);
    out.writeInt (16);
    out.writeShort (1);                              // PCM
    out.writeShort (1);                              // mono
    out.writeInt (PhraseFinder::rate);
    out.writeInt (PhraseFinder::rate * 2);           // bytes a second
    out.writeShort (2);                              // bytes a sample
    out.writeShort (16);
    out.write ("data", 4);
    out.writeInt (dataBytes);

    for (const auto sample : samples)
        out.writeShort (static_cast<short> (juce::jlimit (-32767, 32767, juce::roundToInt (sample * gain * 32767.0f))));

    return out.getMemoryBlock();
}

//==============================================================================
VoiceControl::VoiceControl (AudioEngine& e, FananServer& s)
    : juce::Thread ("Sonify voice control"), engine (e), server (s)
{
}

VoiceControl::~VoiceControl()
{
    *alive = false;   // an answer arriving later finds no one
    stopTimer();
    engine.setVoiceListening (false);
    stopThread (3000);
}

void VoiceControl::changed()
{
    if (onChanged != nullptr)
        onChanged();
}

void VoiceControl::setEnabled (bool shouldListen)
{
    if (enabled == shouldListen)
        return;

    enabled = shouldListen;
    heard.clear();
    message.clear();

    if (enabled)
        startTimer (50);
    else
        stopTimer();

    updateListening();
    changed();
}

void VoiceControl::inputChanged()
{
    restart.store (true);   // the listener starts afresh

    if (enabled)
        updateListening();
}

void VoiceControl::setMessage (const juce::String& text)
{
    message = text;
    changed();
}

juce::String VoiceControl::whyNot() const
{
    // It works through Fanan's server, like Sonify AI.
    if (! server.isConfigured())
        return "Voice control isn't set up in this build yet.";

    if (server.isBlocked())
        return "The internet is blocked (the switch at the top left): voice control works through it.";

    if (! server.isReady())
    {
        const auto status = server.getStatus();

        return status == FananServer::Status::checking   ? juce::String::fromUTF8 ("Connecting to Fanan's server\xe2\x80\xa6")
             : status == FananServer::Status::noInternet ? juce::String ("No internet connection: voice control works through it.")
                                                         : juce::String ("Fanan's server isn't responding right now.");
    }

    if (! server.isSignedIn())
        return "Sign in to use voice control. It's part of your Fanan account.";

    if (server.getCredits() == 0)
        return "You're out of credits.";

    if (juce::Time::getCurrentTime() < pausedUntil)
        return pausedWhy;

    if (! engine.isAudioRunning())
        return "The audio device isn't running.";

    if (engine.isMixdownRunning())
        return "Not while a mixdown is being made.";

    const auto& inputs = engine.getInputs();
    const auto index = engine.getVoiceInput();

    if (! juce::isPositiveAndBelow (index, static_cast<int> (inputs.size())))
        return "Choose the input the musician's mic is on.";

    const auto& input = inputs[static_cast<size_t> (index)];
    const auto channels = engine.getInputChannelNames().size();

    if (input.left >= channels || input.right >= channels)
        return "That input isn't on in the audio settings.";

    return {};
}

//==============================================================================
void VoiceControl::updateListening()
{
    const auto why = enabled ? whyNot() : juce::String();
    const auto can = enabled && why.isEmpty();
    auto tell = why != lastWhy;
    lastWhy = why;

    if (can != listening)
    {
        listening = can;
        tell = true;

        if (can)
            startListening();
        else
            stopListening();
    }

    if (tell)
        changed();
}

void VoiceControl::startListening()
{
    ++session;
    stage = Stage::listening;
    parts.clear();
    waiting.clear();
    unwanted.clear();
    writing = failing = false;

    wantHeads.store (true);
    dropRequest.store (0);
    hearing.store (false);
    inputRate.store (engine.getPlaybackRate());
    restart.store (true);

    {
        const juce::ScopedLock lock (foundLock);
        found.clear();
    }

    engine.setVoiceListening (true);
    startThread();
}

void VoiceControl::stopListening()
{
    engine.setVoiceListening (false);
    stopThread (3000);

    ++session;   // an answer on its way is dropped
    stage = Stage::off;
    parts.clear();
    waiting.clear();
    unwanted.clear();
    writing = false;
    hearing.store (false);

    const juce::ScopedLock lock (foundLock);
    found.clear();
}

void VoiceControl::run()
{
    const juce::ScopedNoDenormals noDenormals;

    PhraseFinder finder;
    std::vector<float> buffer (8192);
    std::vector<PhraseFinder::Phrase> phrases;
    double preparedRate = 0.0;
    juce::uint32 lastBeep = 0;
    bool beeped = false;

    while (! threadShouldExit())
    {
        const auto rateNow = inputRate.load();

        if (restart.exchange (false) || ! juce::exactlyEqual (rateNow, preparedRate))
        {
            finder.prepare (rateNow);
            preparedRate = rateNow;

            // What the mic heard before is no use now.
            while (engine.readVoice (buffer.data(), static_cast<int> (buffer.size())) > 0) {}

            hearing.store (false);
        }

        const auto count = engine.readVoice (buffer.data(), static_cast<int> (buffer.size()));

        if (count <= 0)
        {
            wait (15.0);
            continue;
        }

        // Sonify's own beeps, and their echo in the room, begin no phrase.
        const auto now = juce::Time::getMillisecondCounter();

        if (engine.isCuePlaying())
        {
            beeped = true;
            lastBeep = now;
        }

        const auto hushed = beeped && now - lastBeep < hushMs;
        beeped = hushed;

        finder.setWantsHeads (wantHeads.load());

        if (const auto id = dropRequest.exchange (0); id != 0)
            finder.drop (id);

        finder.process (buffer.data(), count, hushed, phrases);
        hearing.store (finder.isInPhrase());

        if (! phrases.empty())
        {
            const juce::ScopedLock lock (foundLock);

            for (auto& phrase : phrases)
                found.push_back (std::move (phrase));

            phrases.clear();
        }
    }
}

//==============================================================================
void VoiceControl::timerCallback()
{
    inputRate.store (engine.getPlaybackRate());   // the listener starts afresh when the device's rate changes
    updateListening();

    if (! listening)
        return;

    std::vector<PhraseFinder::Phrase> arrived;

    {
        const juce::ScopedLock lock (foundLock);
        arrived.swap (found);
    }

    for (auto& phrase : arrived)
        accept (std::move (phrase));

    // A command that never got its "over" ends by itself, with a beep.
    if (stage == Stage::command)
    {
        const auto now = juce::Time::getMillisecondCounter();
        const auto goingOn = hearing.load() || writing || ! waiting.empty();

        if (now - commandStarted > commandLimitMs || (! goingOn && now - lastWordsAt > commandTimeoutMs))
            giveUp ("Didn't hear \"over\". Say \"Sonify\" to begin again.");
    }

    sendNext();
}

void VoiceControl::accept (PhraseFinder::Phrase phrase)
{
    using Kind = PhraseFinder::Kind;

    // A head is only wanted while waiting for "Sonify"; during a command the whole phrase comes.
    if (phrase.kind == Kind::head && stage != Stage::listening)
        return;

    // A phrase whose head didn't begin with "Sonify": only its end is heard.
    if (phrase.kind == Kind::whole && unwanted.erase (phrase.id) > 0)
        phrase = PhraseFinder::endOf (std::move (phrase));

    waiting.push_back (std::move (phrase));

    // While waiting for "Sonify", only the newest few: Sonify never falls behind what's said.
    if (stage == Stage::listening)
    {
        while (waiting.size() > maxWaiting)
            waiting.pop_front();   // a head that goes leaves its phrase to be heard whole
    }
}

void VoiceControl::forget (int phraseId)
{
    using Kind = PhraseFinder::Kind;

    // Only its end is wanted now: the listener keeps just that, and here a whole phrase waiting
    // becomes its end.
    dropRequest.store (phraseId);
    bool restHere = false;

    for (auto it = waiting.begin(); it != waiting.end();)
    {
        if (it->id != phraseId)
        {
            ++it;
        }
        else if (it->kind == Kind::head)
        {
            it = waiting.erase (it);
        }
        else
        {
            if (it->kind == Kind::whole)
                *it = PhraseFinder::endOf (std::move (*it));

            restHere = true;
            ++it;
        }
    }

    if (! restHere)
    {
        unwanted.insert (phraseId);   // it may come whole yet

        while (unwanted.size() > 32)
            unwanted.erase (unwanted.begin());
    }
}

void VoiceControl::sendNext()
{
    while (listening && ! writing && ! waiting.empty())
    {
        auto phrase = std::move (waiting.front());
        waiting.pop_front();

        if (phrase.kind == PhraseFinder::Kind::head && stage != Stage::listening)
            continue;

        writing = true;

        server.transcribe (toWav (phrase.samples),
                           [this, flag = alive, id = phrase.id, kind = phrase.kind, asked = session] (const juce::Result& result, const juce::var& answer)
        {
            if (! *flag || asked != session)
                return;

            writing = false;
            onWords (id, kind, result, answer);
            sendNext();
        });
    }
}

void VoiceControl::onWords (int phraseId, PhraseFinder::Kind kind, const juce::Result& result, const juce::var& answer)
{
    if (result.failed())
    {
        if (kind == PhraseFinder::Kind::head)
            forget (phraseId);

        // One beep for a problem, not one for every phrase while it lasts.
        const auto first = ! failing;
        failing = true;
        message = result.getErrorMessage();

        if (stage == Stage::command)
            giveUp (message);
        else if (first)
            engine.playCue (AudioEngine::Cue::problem);

        // The account's listening for the day is used up: it waits for the next day (UTC, as the server counts).
        if (answer.getProperty ("error", {}).toString() == "listen_limit")
        {
            constexpr juce::int64 day = 24 * 60 * 60 * 1000;
            pausedUntil = juce::Time ((juce::Time::currentTimeMillis() / day + 1) * day);
            pausedWhy = message;
            updateListening();
        }

        changed();
        return;
    }

    const auto text = answer.getProperty ("text", {}).toString();

    if (failing)
    {
        failing = false;
        message.clear();
        changed();
    }

    const auto words = read (text);

    if (kind == PhraseFinder::Kind::head)
    {
        // The head of a long phrase: all of it is wanted only when it began with "Sonify".
        if (afterWake (words) < 0)
            forget (phraseId);

        return;
    }

    if (words.plain.isEmpty())
        return;

    auto from = afterWake (words);

    if (stage == Stage::listening)
    {
        // "Sonify" further on counts too when "over" ends it: "... yeah. Sonify, stop, over."
        if (from < 0)
            if (const auto last = afterLastWake (words); last >= 0 && stopLength (words, last) > 0)
                from = last;

        if (from < 0)
            return;   // not for Sonify

        stage = Stage::command;
        wantHeads.store (false);
        parts.clear();
        commandStarted = juce::Time::getMillisecondCounter();
        addWords (words, from, true);
        return;
    }

    // "Sonify" again during a command begins it again.
    if (from >= 0)
    {
        parts.clear();
        commandStarted = juce::Time::getMillisecondCounter();
        addWords (words, from, true);
    }
    else
    {
        addWords (words, 0, false);
    }
}

void VoiceControl::addWords (const Words& words, int from, bool woke)
{
    const auto stop = stopLength (words, from);

    if (const auto text = join (words, from, words.spoken.size() - stop); text.isNotEmpty())
        parts.add (text);

    lastWordsAt = juce::Time::getMillisecondCounter();
    heard = parts.joinIntoString (" ");

    if (stop == 0)
    {
        if (woke)
            engine.playCue (AudioEngine::Cue::listening);   // go on, then say "over"

        changed();
        return;
    }

    const auto command = heard.trim();
    backToListening();

    if (command.isEmpty())
    {
        engine.playCue (AudioEngine::Cue::problem);
        message = "Heard \"Sonify\" and \"over\", with nothing between.";
        changed();
        return;
    }

    heard = command;
    message.clear();
    changed();

    if (onCommand != nullptr)
        onCommand (command);
}

void VoiceControl::backToListening()
{
    stage = Stage::listening;
    parts.clear();
    wantHeads.store (true);
}

void VoiceControl::giveUp (const juce::String& why)
{
    engine.playCue (AudioEngine::Cue::problem);
    message = why;
    backToListening();
    changed();
}
