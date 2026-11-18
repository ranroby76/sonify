// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakPattern.h
#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <limits>
#include <vector>
#include "ZodiakParams.h"

//==============================================================================
//  ZodiakPattern.h  —  the patterns and the sequencer that plays them.
//
//  Every channel has 32 patterns: banks A-D x variations 1-8.  A pattern:
//
//      BARS        1, 2 or 4
//      GRID        what the Roll snaps to: 1/8, 1/16 (default), 1/32, 1/8T, 1/16T
//      TIME SIG    SYNC DAW (default: the DAW's time signature, 4/4 without
//                  one) or its own (e.g. 7/8)
//      LOCATORS    the stretch of it that loops (on the 1/32 grid; the whole
//                  pattern by default)
//      SWING       50% (straight, the default) to 75%: the second 16th of each
//                  8th comes late, by up to half a 16th (ReBirth's SHUFFLE; an
//                  MPC's swing).  Played only: the Roll shows the notes straight
//      NOTES       start and length (ticks: 96 a quarter note), key, velocity
//
//  The notes go to the channel's VSTi as they are (a velocity of 115 or more
//  is drawn as an accent; what it does is the VSTi's).
//
//  PLAYING.  The sequencer turns the chosen patterns into notes for the
//  channels, ahead of the MIDI that comes in (they play the same way, so a
//  VSTi on the channel plays them too).  A pattern loops from where it began;
//  one chosen while the song plays begins at the end of the playing pattern's
//  bar (the window blinks it until then); chosen while stopped, it's there at
//  once.  Each channel loops its own pattern, so patterns of different lengths
//  and time signatures run side by side.
//
//  THREADS.  The windows read a pattern (a copy) and write it back whole
//  (get / set); the audio thread reads them under a try-lock, so a pattern
//  being written is skipped for one block, never waited for.
//==============================================================================

namespace zodiak::seq
{
    static constexpr int kTicksPerQuarter = 96;
    static constexpr int kNumPatterns     = 32;           // 4 banks x 8 variations
    static constexpr int kMaxNotes        = 2048;         // in one pattern
    static constexpr int kAccentVelocity  = 115;

    inline int patternIndex (int bank, int variation) noexcept   { return juce::jlimit (0, 3, bank) * 8 + juce::jlimit (0, 7, variation); }
    inline juce::String patternName (int index)                  { return juce::String::charToString ((juce::juce_wchar) ('A' + index / 8)) + juce::String (index % 8 + 1); }

    //==========================================================================
    //  The grids
    //==========================================================================
    static constexpr int kNumGrids = 7;
    inline int gridTicks (int g) noexcept
    {
        static const int t[kNumGrids] = { 48, 24, 12, 6, 32, 16, 4 };
        return t[juce::jlimit (0, kNumGrids - 1, g)];
    }
    inline const char* gridName (int g) noexcept
    {
        static const char* n[kNumGrids] = { "1/8", "1/16", "1/32", "1/64", "1/8T", "1/16T", "1/64T" };
        return n[juce::jlimit (0, kNumGrids - 1, g)];
    }

    //==========================================================================
    struct Note
    {
        int start  = 0;          // ticks from the pattern's start
        int length = 24;         // ticks
        int key    = 60;
        int velocity = 100;

        bool operator== (const Note& o) const noexcept
        { return start == o.start && length == o.length && key == o.key && velocity == o.velocity; }
    };

    struct Pattern
    {
        int  bars = 2;                   // 1, 2, 4 (a new pattern: 2 bars, 32 steps of 1/16)
        int  grid = 1;                   // gridTicks
        bool ownSignature = false;       // false: the DAW's
        int  numerator = 4, denominator = 4;
        int  loopStart = 0, loopEnd = kDefaultLoopEnd;   // the locators (ticks); loopEnd 0: the pattern's end
        int  swing = 50;                 // 50 (straight) .. 75
        std::vector<Note> notes;

        static constexpr int kLocatorStep = kTicksPerQuarter / 8;      // 1/32
        static constexpr int kDefaultLoopEnd = kTicksPerQuarter * 4;   // a new pattern's locators: its first 16 steps
        static constexpr int kSwingMin = 50, kSwingMax = 75;
        static constexpr int kEighth = kTicksPerQuarter / 2, kSixteenth = kTicksPerQuarter / 4;

        bool isDefault() const noexcept
        {
            return notes.empty() && bars == 2 && grid == 1 && ! ownSignature && numerator == 4 && denominator == 4
                && loopStart == 0 && loopEnd == kDefaultLoopEnd && swing == kSwingMin;
        }

        /** How late the second 16th of an 8th plays (ticks): 0 straight, 12 at 75%. */
        double swingDelay() const noexcept
        {
            return (double) (juce::jlimit (kSwingMin, kSwingMax, swing) - kSwingMin) / (double) (kSwingMax - kSwingMin) * (kSixteenth / 2);
        }

        /** Where a tick of the pattern plays, swung (ticks).  Each 8th is warped
            on its own: its first 16th stretches by the delay, the second shrinks
            by it, so the 8ths stay where they are (and notes keep their order).
            An 8th that doesn't end by `limit` (the locators' end) stays straight. */
        double swung (double tick, int limit) const noexcept
        {
            const double d = swingDelay();
            if (d <= 0.0 || tick <= 0.0) return tick;
            const double eighth = std::floor (tick / kEighth) * kEighth;
            if (eighth + kEighth > limit) return tick;
            const double x = tick - eighth, half = kSixteenth;
            return eighth + (x < half ? x * (half + d) / half
                                      : half + d + (x - half) * (half - d) / half);
        }

        /** The stretch that loops, in a pattern this long: [start, end). */
        void loopRange (int lengthTicks, int& start, int& end) const noexcept
        {
            end   = loopEnd > 0 ? juce::jmin (loopEnd, lengthTicks) : lengthTicks;
            start = juce::jlimit (0, juce::jmax (0, end - kLocatorStep), loopStart);
            if (end - start < kLocatorStep) { start = 0; end = lengthTicks; }
        }

        /** Its time signature, given the DAW's. */
        void signature (int dawNum, int dawDen, int& num, int& den) const noexcept
        {
            num = ownSignature ? numerator : dawNum;
            den = ownSignature ? denominator : dawDen;
            if (num < 1 || num > 32) num = 4;
            if (den != 1 && den != 2 && den != 4 && den != 8 && den != 16) den = 4;
        }

        static double barQuarters (int num, int den) noexcept    { return (double) num * 4.0 / (double) den; }

        int barTicks (int dawNum, int dawDen) const noexcept
        {
            int n, d; signature (dawNum, dawDen, n, d);
            return juce::roundToInt (barQuarters (n, d) * kTicksPerQuarter);
        }

        int lengthTicks (int dawNum, int dawDen) const noexcept    { return bars * barTicks (dawNum, dawDen); }
    };

    //==========================================================================
    //  A pattern as text and back:
    //      <PATTERN slot="A1" bars="1" grid="1/16" sig="daw"|"own" own_sig="7/8"
    //               loop="start:end" swing="50..75" notes="start:length:key:velocity ...">
    //==========================================================================
    inline void writePattern (const Pattern& p, int index, juce::XmlElement& into)
    {
        auto* e = into.createNewChildElement ("PATTERN");
        e->setAttribute ("slot", patternName (index));
        e->setAttribute ("bars", p.bars);
        e->setAttribute ("grid", juce::String (gridName (p.grid)));
        e->setAttribute ("sig", p.ownSignature ? "own" : "daw");
        e->setAttribute ("own_sig", juce::String (p.numerator) + "/" + juce::String (p.denominator));
        e->setAttribute ("loop", juce::String (p.loopStart) + ":" + juce::String (p.loopEnd));
        if (p.swing != Pattern::kSwingMin)
            e->setAttribute ("swing", p.swing);

        juce::String text;
        for (const auto& n : p.notes)
            text << n.start << ":" << n.length << ":" << n.key << ":" << n.velocity << " ";
        e->setAttribute ("notes", text.trimEnd());
    }

    /** The slot (0..31) a PATTERN element is for, or -1. */
    inline int readPattern (const juce::XmlElement& e, Pattern& p)
    {
        const auto slot = e.getStringAttribute ("slot").toUpperCase();
        if (slot.length() < 2) return -1;
        const int bank = slot[0] - 'A', variation = slot.substring (1).getIntValue() - 1;
        if (bank < 0 || bank > 3 || variation < 0 || variation > 7) return -1;

        p = Pattern();
        const int bars = e.getIntAttribute ("bars", 1);
        p.bars = bars >= 4 ? 4 : bars >= 2 ? 2 : 1;
        const auto grid = e.getStringAttribute ("grid", "1/16");
        for (int g = 0; g < kNumGrids; ++g)
            if (grid == gridName (g)) p.grid = g;

        p.ownSignature = e.getStringAttribute ("sig", "daw") == "own";
        const auto ownSig = e.getStringAttribute ("own_sig", "4/4");
        p.numerator   = juce::jlimit (1, 32, ownSig.upToFirstOccurrenceOf ("/", false, false).getIntValue());
        p.denominator = ownSig.fromFirstOccurrenceOf ("/", false, false).getIntValue();
        if (p.denominator != 2 && p.denominator != 4 && p.denominator != 8 && p.denominator != 16) p.denominator = 4;

        p.loopStart = 0;
        p.loopEnd = 0;                  // no "loop": an older file's, the whole pattern
        if (const auto loop = e.getStringAttribute ("loop"); loop.containsChar (':'))
        {
            p.loopStart = juce::jmax (0, loop.upToFirstOccurrenceOf (":", false, false).getIntValue());
            p.loopEnd   = juce::jmax (0, loop.fromFirstOccurrenceOf (":", false, false).getIntValue());
        }

        p.swing = juce::jlimit (Pattern::kSwingMin, Pattern::kSwingMax, e.getIntAttribute ("swing", Pattern::kSwingMin));

        for (const auto& t : juce::StringArray::fromTokens (e.getStringAttribute ("notes"), " ", {}))
        {
            const auto f = juce::StringArray::fromTokens (t, ":", {});
            if (f.size() != 4 || (int) p.notes.size() >= kMaxNotes) continue;
            Note n;
            n.start    = juce::jmax (0, f[0].getIntValue());
            n.length   = juce::jmax (1, f[1].getIntValue());
            n.key      = juce::jlimit (0, 127, f[2].getIntValue());
            n.velocity = juce::jlimit (1, 127, f[3].getIntValue());
            p.notes.push_back (n);
        }
        return bank * 8 + variation;
    }

    //==========================================================================
    //  A pattern as a standard MIDI file and back (the Roll's EXPORT / IMPORT
    //  MIDI): plain MIDI, quantized -
    //      the locators' stretch, from 0 (the whole pattern by default); the
    //      file's end is the stretch's end, so a 15-step loop stays 15 steps
    //      SWING isn't played into the notes: it rides along as a text event,
    //      "ZODIAK SWING=61", which other programs ignore
    //      its tempo and time signature, its name
    //      one track: the drums on MIDI channel 10 (GM), a synth on channel 1
    //==========================================================================
    static constexpr const char* kSwingText = "ZODIAK SWING=";

    inline juce::MidiFile patternToMidi (const Pattern& p, int dawNum, int dawDen, double bpm, bool drums, const juce::String& name)
    {
        constexpr int kFileTicks = 960;
        constexpr int scale = kFileTicks / kTicksPerQuarter;

        int num, den;
        p.signature (dawNum, dawDen, num, den);
        int ls, le;
        p.loopRange (p.lengthTicks (dawNum, dawDen), ls, le);

        juce::MidiMessageSequence track;
        auto meta = [&] (juce::MidiMessage m) { m.setTimeStamp (0.0); track.addEvent (m); };
        if (name.isNotEmpty()) meta (juce::MidiMessage::textMetaEvent (3, name));
        meta (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / juce::jlimit (20.0, 999.0, bpm > 1.0 ? bpm : 120.0))));
        meta (juce::MidiMessage::timeSignatureMetaEvent (num, den));
        if (p.swing > Pattern::kSwingMin)
            meta (juce::MidiMessage::textMetaEvent (1, kSwingText + juce::String (juce::jlimit (Pattern::kSwingMin, Pattern::kSwingMax, p.swing))));

        const int midiChannel = drums ? 10 : 1;
        for (const auto& n : p.notes)
        {
            if (n.start < ls || n.start >= le) continue;
            const int on  = (n.start - ls) * scale;
            const int off = juce::jmax (on + 1, (juce::jmin (n.start + n.length, le) - ls) * scale);
            track.addEvent (juce::MidiMessage::noteOn  (midiChannel, n.key, (juce::uint8) juce::jlimit (1, 127, n.velocity)).withTimeStamp (on));
            track.addEvent (juce::MidiMessage::noteOff (midiChannel, n.key).withTimeStamp (off));
        }
        track.updateMatchedPairs();

        auto endOfTrack = juce::MidiMessage::endOfTrack();
        endOfTrack.setTimeStamp ((le - ls) * scale);
        track.addEvent (endOfTrack);
        track.sort();

        juce::MidiFile file;
        file.setTicksPerQuarterNote (kFileTicks);
        file.addTrack (track);
        return file;
    }

    /** What reading a MIDI file into a pattern found. */
    struct MidiImport
    {
        bool ok = false;
        juce::String error;                 // not ok: why
        int  notes = 0, dropped = 0;        // dropped: keys the channel can't play, notes past 4 bars
        bool cut = false;                   // longer than 4 bars: the first 4 kept
    };

    /** A standard MIDI file into a pattern (it replaces `p`, keeping its GRID).
        Every track is read; the notes of one MIDI channel are taken - on a drum
        channel channel 10 when the file has it, else the lowest channel with
        notes (a synth channel: the lowest but 10).  BARS: the smallest of 1, 2, 4
        that holds the file (its end, else its last note's bar); a file that ends
        inside its last bar sets the LOCATORS' end there.  Its time signature
        (4/4: SYNC DAW) and swing ("ZODIAK SWING=") come with it.  `mapKey`: a
        key to the key it plays (-1: dropped). */
    inline MidiImport patternFromMidi (const juce::MidiFile& file, bool drums, int dawNum, int dawDen,
                                       const std::function<int (int)>& mapKey, Pattern& p)
    {
        MidiImport r;
        const int format = file.getTimeFormat();
        if (format <= 0) { r.error = "The file counts time in SMPTE frames, not beats"; return r; }
        auto toTicks = [format] (double t) { return (int) std::llround (t * kTicksPerQuarter / (double) format); };

        struct Found { int start, end, key, velocity, channel; };
        std::vector<Found> found;
        std::array<int, 17> perChannel {};
        double eot = 0.0, sigAt = std::numeric_limits<double>::max();
        int sigNum = 0, sigDen = 0, swing = Pattern::kSwingMin;

        for (int t = 0; t < file.getNumTracks(); ++t)
        {
            juce::MidiMessageSequence seq (*file.getTrack (t));
            seq.updateMatchedPairs();
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                const auto* ev = seq.getEventPointer (i);
                const auto& m = ev->message;
                if (m.isEndOfTrackMetaEvent()) eot = juce::jmax (eot, m.getTimeStamp());
                else if (m.isTimeSignatureMetaEvent() && m.getTimeStamp() < sigAt) { sigAt = m.getTimeStamp(); m.getTimeSignatureInfo (sigNum, sigDen); }
                else if (m.isTextMetaEvent() && m.getTextFromTextMetaEvent().startsWith (kSwingText))
                    swing = juce::jlimit (Pattern::kSwingMin, Pattern::kSwingMax, m.getTextFromTextMetaEvent().fromFirstOccurrenceOf ("=", false, false).getIntValue());
                else if (m.isNoteOn())
                {
                    const int start = toTicks (m.getTimeStamp());
                    const int end = ev->noteOffObject != nullptr ? toTicks (ev->noteOffObject->message.getTimeStamp()) : start + kTicksPerQuarter / 4;
                    found.push_back ({ start, juce::jmax (start + 1, end), m.getNoteNumber(), m.getVelocity(), m.getChannel() });
                    ++perChannel[(size_t) juce::jlimit (1, 16, m.getChannel())];
                }
            }
        }
        if (found.empty()) { r.error = "The file has no notes"; return r; }

        int channel = 0;
        if (drums && perChannel[10] > 0) channel = 10;
        for (int c = 1; c <= 16 && channel == 0; ++c)
            if (perChannel[(size_t) c] > 0 && (drums || c != 10)) channel = c;
        if (channel == 0) channel = 10;

        const int grid = p.grid;
        p = Pattern();
        p.grid = grid;
        p.swing = swing;
        if (sigNum >= 1 && sigNum <= 16 && (sigDen == 2 || sigDen == 4 || sigDen == 8 || sigDen == 16) && ! (sigNum == 4 && sigDen == 4))
        {
            p.ownSignature = true;
            p.numerator = sigNum;
            p.denominator = sigDen;
        }
        const int bar = p.barTicks (dawNum, dawDen);

        int lastEnd = 0;
        for (const auto& f : found) if (f.channel == channel) lastEnd = juce::jmax (lastEnd, f.end);
        int length = toTicks (eot);
        if (length <= 0) length = (lastEnd + bar - 1) / bar * bar;
        length = juce::jmax (Pattern::kLocatorStep, length);

        p.bars = length <= bar ? 1 : length <= 2 * bar ? 2 : 4;
        const int total = p.bars * bar;
        if (length > total) { r.cut = true; length = total; }
        const int loopEnd = juce::jlimit (Pattern::kLocatorStep, total, (length + Pattern::kLocatorStep / 2) / Pattern::kLocatorStep * Pattern::kLocatorStep);
        p.loopEnd = loopEnd < total ? loopEnd : 0;

        for (const auto& f : found)
        {
            if (f.channel != channel) continue;
            const int key = mapKey ? mapKey (f.key) : f.key;
            if (key < 0 || key > 127 || f.start >= total || (int) p.notes.size() >= kMaxNotes) { ++r.dropped; continue; }
            p.notes.push_back ({ juce::jmax (0, f.start), juce::jmax (1, juce::jmin (f.end, total) - juce::jmax (0, f.start)),
                                 key, juce::jlimit (1, 127, f.velocity) });
        }
        std::sort (p.notes.begin(), p.notes.end(), [] (const Note& a, const Note& b) { return a.start != b.start ? a.start < b.start : a.key < b.key; });
        r.notes = (int) p.notes.size();
        r.ok = true;
        return r;
    }

    //==========================================================================
    //  The sequencer
    //==========================================================================
    class Sequencer
    {
    public:
        Sequencer()
        {
            for (auto& c : chans)
            {
                c.requested.store (0);
                for (auto& n : c.counts) n.store (0);
            }
        }

        //======================================================================
        //  Message thread
        //======================================================================
        Pattern get (int ch, int index) const
        {
            const auto& c = chan (ch);
            const juce::SpinLock::ScopedLockType sl (c.lock);
            return c.patterns[(size_t) juce::jlimit (0, kNumPatterns - 1, index)];
        }

        void set (int ch, int index, Pattern p)
        {
            auto& c = chan (ch);
            index = juce::jlimit (0, kNumPatterns - 1, index);
            if ((int) p.notes.size() > kMaxNotes) p.notes.resize ((size_t) kMaxNotes);
            const int count = (int) p.notes.size();
            {
                const juce::SpinLock::ScopedLockType sl (c.lock);
                std::swap (c.patterns[(size_t) index], p);       // the old one goes here, outside the lock
            }
            c.counts[(size_t) index].store (count);
        }

        void clearAll()
        {
            for (int ch = 0; ch < kNumChannels; ++ch)
                for (int i = 0; i < kNumPatterns; ++i)
                    set (ch, i, Pattern());
        }

        /** Its notes (the variation buttons show which patterns have some). */
        int noteCount (int ch, int index) const noexcept   { return chan (ch).counts[(size_t) juce::jlimit (0, kNumPatterns - 1, index)].load(); }

        /** The pattern chosen (it plays at once when stopped, else at the end of the bar). */
        void select (int ch, int index) noexcept            { chan (ch).requested.store (juce::jlimit (0, kNumPatterns - 1, index)); }

        /** Chosen and playing at once (a set loading). */
        void selectNow (int ch, int index) noexcept
        {
            select (ch, index);
            chan (ch).jumpRequested.store (true);
        }

        int  requested (int ch) const noexcept              { return chan (ch).requested.load(); }
        int  playing (int ch) const noexcept                { return chan (ch).playingIndex.load (std::memory_order_relaxed); }

        /** Where the channel's pattern is (ticks from its start), -1 when stopped. */
        int  position (int ch) const noexcept               { return chan (ch).shownTick.load (std::memory_order_relaxed); }

        /** The DAW's time signature as the sequencer last saw it. */
        int  dawNumerator() const noexcept                  { return dawNum.load (std::memory_order_relaxed); }
        int  dawDenominator() const noexcept                { return dawDen.load (std::memory_order_relaxed); }

        //======================================================================
        //  Audio thread
        //======================================================================
        void prepare (double sampleRate) noexcept
        {
            sr = sampleRate;
            for (auto& c : chans) { c.numActive = 0; c.playingIndex.store (c.requested.load()); c.anchor = 0.0; }
            wasPlaying = false;
        }

        /** The notes for this block, into `out` (the incoming MIDI is already there). */
        void render (juce::MidiBuffer& out, const Transport& tr, int numSamples) noexcept
        {
            const int tsNum = juce::jlimit (1, 32, tr.numerator);
            const int tsDen = (tr.denominator == 2 || tr.denominator == 8 || tr.denominator == 16) ? tr.denominator : 4;
            dawNum.store (tsNum, std::memory_order_relaxed);
            dawDen.store (tsDen, std::memory_order_relaxed);

            numEvents = 0;
            const double qps = tr.beatsPerSample();
            const double p0 = tr.ppq, p1 = p0 + (tr.playing ? qps * numSamples : 0.0);

            const bool started = tr.playing && ! wasPlaying;
            const bool jumped  = tr.playing && wasPlaying && std::abs (p0 - expected) > qps * 2.0 + 1.0e-6;
            const bool stopped = ! tr.playing && wasPlaying;
            wasPlaying = tr.playing;
            expected = p1;

            for (int ch = 0; ch < kNumChannels; ++ch)
            {
                auto& c = chans[(size_t) ch];

                if (stopped || jumped) { allOff (c, ch, 0); c.resumeFrom = -1.0; }
                if (started) c.anchor = 0.0;

                // Chosen while stopped (or a set loading): at once
                const int want = c.requested.load (std::memory_order_relaxed);
                if (want != c.playingIndex.load (std::memory_order_relaxed) && (! tr.playing || c.jumpRequested.load()))
                {
                    allOff (c, ch, 0);
                    c.playingIndex.store (want, std::memory_order_relaxed);
                    c.anchor = tr.playing ? p0 : 0.0;
                }
                c.jumpRequested.store (false, std::memory_order_relaxed);

                if (! tr.playing)
                {
                    c.shownTick.store (-1, std::memory_order_relaxed);
                    continue;
                }

                // A pattern being written: its notes come in the next block (late, not lost)
                const juce::SpinLock::ScopedTryLockType sl (c.lock);
                if (! sl.isLocked())
                {
                    if (c.resumeFrom < 0.0) c.resumeFrom = p0;
                    offsDue (c, ch, p0, p1, qps);
                    continue;
                }

                const auto* pat = &c.patterns[(size_t) c.playingIndex.load (std::memory_order_relaxed)];
                double from = p0;
                if (c.resumeFrom >= 0.0)
                {
                    if (c.resumeFrom < p0 && ! jumped) from = c.resumeFrom;
                    c.resumeFrom = -1.0;
                }

                // Chosen while playing: at the end of the playing pattern's bar
                if (want != c.playingIndex.load (std::memory_order_relaxed))
                {
                    const double barQ = pat->barTicks (tsNum, tsDen) / (double) kTicksPerQuarter;
                    const double into = from - c.anchor;
                    const double k = std::ceil (into / barQ - 1.0e-9);
                    const double boundary = c.anchor + juce::jmax (0.0, k) * barQ;
                    if (boundary < p1)
                    {
                        notesIn (c, ch, *pat, from, boundary, p0, qps, tsNum, tsDen);
                        offsDue (c, ch, p0, boundary, qps);
                        allOff (c, ch, frameAt (boundary, p0, qps));
                        c.playingIndex.store (want, std::memory_order_relaxed);
                        c.anchor = boundary;
                        pat = &c.patterns[(size_t) want];
                        from = boundary;
                    }
                }

                notesIn (c, ch, *pat, from, p1, p0, qps, tsNum, tsDen);
                offsDue (c, ch, p0, p1, qps);

                int ls, le;
                pat->loopRange (pat->lengthTicks (tsNum, tsDen), ls, le);
                c.shownTick.store (le > ls ? ls + (int) posMod ((p0 - c.anchor) * kTicksPerQuarter, (double) (le - ls)) : 0, std::memory_order_relaxed);
            }

            // Into the block: by frame, a note's end before a note's start
            std::sort (events.begin(), events.begin() + numEvents, [] (const Event& a, const Event& b)
            {
                return a.frame != b.frame ? a.frame < b.frame : a.velocity < b.velocity;
            });
            for (int i = 0; i < numEvents; ++i)
            {
                const auto& e = events[(size_t) i];
                const int frame = juce::jlimit (0, juce::jmax (0, numSamples - 1), e.frame);
                out.addEvent (e.velocity > 0 ? juce::MidiMessage::noteOn (e.midiChannel, e.key, (juce::uint8) e.velocity)
                                             : juce::MidiMessage::noteOff (e.midiChannel, e.key),
                              frame);
            }
        }

    private:
        struct Active { int key; double offAt; };
        struct Event  { int frame; int midiChannel; int key; int velocity; };     // velocity 0: off

        struct Chan
        {
            mutable juce::SpinLock lock;
            std::array<Pattern, kNumPatterns> patterns;
            std::array<std::atomic<int>, kNumPatterns> counts;
            std::atomic<int>  requested { 0 }, playingIndex { 0 }, shownTick { -1 };
            std::atomic<bool> jumpRequested { false };
            double anchor = 0.0;                         // the song's place the pattern began at (quarters)
            double resumeFrom = -1.0;                    // a block skipped: where its notes begin
            std::array<Active, 128> active {};
            int numActive = 0;
        };

        Chan&       chan (int ch) noexcept        { return chans[(size_t) juce::jlimit (0, kNumChannels - 1, ch)]; }
        const Chan& chan (int ch) const noexcept  { return chans[(size_t) juce::jlimit (0, kNumChannels - 1, ch)]; }

        static int midiChannelOf (int ch) noexcept          { return ch + 1; }     // MIDI 1..8: channels 1..8
        static double posMod (double a, double b) noexcept  { const double r = std::fmod (a, b); return r < 0.0 ? r + b : r; }
        static int frameAt (double ppq, double p0, double qps) noexcept   { return (int) std::floor ((ppq - p0) / qps + 1.0e-6); }

        void push (int frame, int ch, int key, int velocity) noexcept
        {
            if (numEvents >= (int) events.size()) return;
            events[(size_t) numEvents++] = { frame, midiChannelOf (ch), key, velocity };
        }

        void allOff (Chan& c, int ch, int frame) noexcept
        {
            for (int i = 0; i < c.numActive; ++i) push (frame, ch, c.active[(size_t) i].key, 0);
            c.numActive = 0;
        }

        // The notes that end before `to`
        void offsDue (Chan& c, int ch, double p0, double to, double qps) noexcept
        {
            for (int i = 0; i < c.numActive;)
            {
                if (c.active[(size_t) i].offAt < to)
                {
                    push (juce::jmax (0, frameAt (c.active[(size_t) i].offAt, p0, qps)), ch, c.active[(size_t) i].key, 0);
                    c.active[(size_t) i] = c.active[(size_t) --c.numActive];
                }
                else ++i;
            }
        }

        // The notes that start in [from, to) (the song's quarters), the locators' stretch looping
        void notesIn (Chan& c, int ch, const Pattern& p, double from, double to, double p0, double qps, int tsNum, int tsDen) noexcept
        {
            if (p.notes.empty() || to <= from) return;
            int ls, le;
            p.loopRange (p.lengthTicks (tsNum, tsDen), ls, le);
            const double startQ = ls / (double) kTicksPerQuarter, endQ = le / (double) kTicksPerQuarter;
            const double spanQ = endQ - startQ;
            if (spanQ <= 0.0) return;

            double local = startQ + posMod (from - c.anchor, spanQ);
            double at = from;
            while (at < to)
            {
                const double span = std::min (to - at, endQ - local);
                for (const auto& n : p.notes)
                {
                    if (n.start < ls || n.start >= le) continue;     // outside the locators
                    const double s = p.swung (n.start, le) / (double) kTicksPerQuarter;
                    if (s >= local && s < local + span)
                    {
                        const double onAt = at + (s - local);
                        const int frame = juce::jmax (0, frameAt (onAt, p0, qps));

                        // The same key still sounding ends first
                        for (int i = 0; i < c.numActive; ++i)
                            if (c.active[(size_t) i].key == n.key)
                            {
                                push (frame, ch, n.key, 0);
                                c.active[(size_t) i] = c.active[(size_t) --c.numActive];
                                break;
                            }

                        push (frame, ch, n.key, juce::jlimit (1, 127, n.velocity));
                        if (c.numActive < (int) c.active.size())
                        {
                            const double end = p.swung (n.start + n.length, le) / (double) kTicksPerQuarter;
                            c.active[(size_t) c.numActive++] = { n.key, onAt + juce::jmax (1.0 / kTicksPerQuarter, end - s) };
                        }
                    }
                }
                at += span;
                local = startQ;
            }
        }

        std::array<Chan, kNumChannels> chans;
        std::array<Event, 4096> events {};
        int numEvents = 0;
        double sr = 48000.0, expected = 0.0;
        bool wasPlaying = false;
        std::atomic<int> dawNum { 4 }, dawDen { 4 };
    };
}
