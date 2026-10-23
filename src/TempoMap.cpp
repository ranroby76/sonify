// D:\workspace\Fanan_web_hub\src\TempoMap.cpp

#include "TempoMap.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr double tiny = 1.0e-9;

    double number (const juce::var& value, double fallback)
    {
        const auto result = value.isDouble() || value.isInt() || value.isInt64() || value.isBool() ? static_cast<double> (value) : fallback;
        return std::isfinite (result) ? result : fallback;
    }

    /** A whole number from a song or the page, in a range an int holds. */
    int whole (const juce::var& value, double fallback)
    {
        return static_cast<int> (juce::jlimit (-1.0e6, 1.0e6, number (value, fallback)));
    }
}

//==============================================================================
bool TempoMap::Tempo::operator== (const Tempo& other) const noexcept
{
    return juce::exactlyEqual (ppq, other.ppq) && juce::exactlyEqual (bpm, other.bpm) && ramp == other.ramp;
}

bool TempoMap::Signature::operator== (const Signature& other) const noexcept
{
    return bar == other.bar && numerator == other.numerator && denominator == other.denominator;
}

bool TempoMap::Key::operator== (const Key& other) const noexcept
{
    return bar == other.bar && root == other.root && scale == other.scale;
}

bool TempoMap::Chord::operator== (const Chord& other) const noexcept
{
    return juce::exactlyEqual (ppq, other.ppq) && root == other.root && quality == other.quality && bass == other.bass;
}

bool TempoMap::operator== (const TempoMap& other) const noexcept
{
    return tempos == other.tempos && signatures == other.signatures && keys == other.keys && chords == other.chords;
}

//==============================================================================
TempoMap::TempoMap()
{
    tempos = { Tempo {} };
    signatures = { Signature {} };
    keys = { Key {} };
    rebuild();
}

bool TempoMap::isValidDenominator (int value) noexcept
{
    return value == 1 || value == 2 || value == 4 || value == 8 || value == 16 || value == 32;
}

const juce::StringArray& TempoMap::getScales()
{
    static const juce::StringArray scales { "major", "minor", "dorian", "phrygian", "lydian", "mixolydian", "locrian",
                                            "harmonic minor", "melodic minor", "major pentatonic", "minor pentatonic",
                                            "blues", "chromatic" };
    return scales;
}

juce::uint16 TempoMap::scaleNotes (const juce::String& scale) noexcept
{
    // In getScales()'s order.
    static constexpr juce::uint16 notes[] = {
        0x0ab5,   // major              0 2 4 5 7 9 11
        0x05ad,   // minor              0 2 3 5 7 8 10
        0x06ad,   // dorian             0 2 3 5 7 9 10
        0x05ab,   // phrygian           0 1 3 5 7 8 10
        0x0ad5,   // lydian             0 2 4 6 7 9 11
        0x06b5,   // mixolydian         0 2 4 5 7 9 10
        0x056b,   // locrian            0 1 3 5 6 8 10
        0x09ad,   // harmonic minor     0 2 3 5 7 8 11
        0x0aad,   // melodic minor      0 2 3 5 7 9 11
        0x0295,   // major pentatonic   0 2 4 7 9
        0x04a9,   // minor pentatonic   0 3 5 7 10
        0x04e9,   // blues              0 3 5 6 7 10
        0x0fff    // chromatic
    };

    const auto index = getScales().indexOf (scale);
    return index >= 0 && index < static_cast<int> (std::size (notes)) ? notes[index] : notes[0];
}

juce::String TempoMap::keyName (const Key& key)
{
    static const char* names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return juce::String (names[juce::jlimit (0, 11, key.root)]) + " " + key.scale;
}

const juce::StringArray& TempoMap::getChordQualities()
{
    static const juce::StringArray qualities { "", "m", "7", "maj7", "m7", "6", "m6", "9", "maj9", "m9", "add9",
                                               "sus2", "sus4", "7sus4", "dim", "dim7", "m7b5", "aug", "5" };
    return qualities;
}

juce::uint16 TempoMap::chordNotes (const juce::String& quality) noexcept
{
    // In getChordQualities()'s order.
    static constexpr juce::uint16 notes[] = {
        0x0091,   // major      0 4 7
        0x0089,   // m          0 3 7
        0x0491,   // 7          0 4 7 10
        0x0891,   // maj7       0 4 7 11
        0x0489,   // m7         0 3 7 10
        0x0291,   // 6          0 4 7 9
        0x0289,   // m6         0 3 7 9
        0x0495,   // 9          0 2 4 7 10
        0x0895,   // maj9       0 2 4 7 11
        0x048d,   // m9         0 2 3 7 10
        0x0095,   // add9       0 2 4 7
        0x0085,   // sus2       0 2 7
        0x00a1,   // sus4       0 5 7
        0x04a1,   // 7sus4      0 5 7 10
        0x0049,   // dim        0 3 6
        0x0249,   // dim7       0 3 6 9
        0x0449,   // m7b5       0 3 6 10
        0x0111,   // aug        0 4 8
        0x0081    // 5          0 7
    };

    const auto index = getChordQualities().indexOf (quality);
    return index >= 0 && index < static_cast<int> (std::size (notes)) ? notes[index] : notes[0];
}

juce::String TempoMap::chordName (const Chord& chord)
{
    static const char* names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    auto name = juce::String (names[juce::jlimit (0, 11, chord.root)]) + chord.quality;

    if (chord.bass >= 0 && chord.bass != chord.root)
        name << "/" << names[juce::jlimit (0, 11, chord.bass)];

    return name;
}

//==============================================================================
void TempoMap::setTempos (std::vector<Tempo> list)
{
    list.erase (std::remove_if (list.begin(), list.end(), [] (const Tempo& t) { return ! std::isfinite (t.ppq) || ! std::isfinite (t.bpm); }),
                list.end());

    for (auto& tempo : list)
    {
        tempo.ppq = juce::jmax (0.0, tempo.ppq);
        tempo.bpm = juce::jlimit (minTempo, maxTempo, tempo.bpm);
    }

    std::stable_sort (list.begin(), list.end(), [] (const Tempo& a, const Tempo& b) { return a.ppq < b.ppq; });

    // One per position: the last one given there.
    std::vector<Tempo> tidy;

    for (const auto& tempo : list)
    {
        if (! tidy.empty() && std::abs (tidy.back().ppq - tempo.ppq) < tiny)
            tidy.back() = tempo;
        else
            tidy.push_back (tempo);
    }

    if (tidy.empty())
        tidy.push_back ({});

    tidy.front().ppq = 0.0;
    tidy.back().ramp = false;   // nothing after it to glide to
    tempos = std::move (tidy);
    rebuild();
}

void TempoMap::setSignatures (std::vector<Signature> list)
{
    list.erase (std::remove_if (list.begin(), list.end(), [] (const Signature& s) { return ! isValidDenominator (s.denominator); }),
                list.end());

    for (auto& signature : list)
    {
        signature.bar = juce::jlimit (0, maxBar, signature.bar);
        signature.numerator = juce::jlimit (1, 32, signature.numerator);
    }

    std::stable_sort (list.begin(), list.end(), [] (const Signature& a, const Signature& b) { return a.bar < b.bar; });

    std::vector<Signature> tidy;

    for (const auto& signature : list)
    {
        if (! tidy.empty() && tidy.back().bar == signature.bar)
            tidy.back() = signature;
        else
            tidy.push_back (signature);
    }

    if (tidy.empty())
        tidy.push_back ({});

    tidy.front().bar = 0;
    signatures = std::move (tidy);
    rebuild();
}

void TempoMap::setKeys (std::vector<Key> list)
{
    for (auto& key : list)
    {
        key.bar = juce::jlimit (0, maxBar, key.bar);
        key.root = ((key.root % 12) + 12) % 12;

        if (! getScales().contains (key.scale))
            key.scale = "major";
    }

    std::stable_sort (list.begin(), list.end(), [] (const Key& a, const Key& b) { return a.bar < b.bar; });

    std::vector<Key> tidy;

    for (const auto& key : list)
    {
        if (! tidy.empty() && tidy.back().bar == key.bar)
            tidy.back() = key;
        else
            tidy.push_back (key);
    }

    if (tidy.empty())
        tidy.push_back ({});

    tidy.front().bar = 0;
    keys = std::move (tidy);
}

void TempoMap::rebuild()
{
    tempoSeconds.assign (tempos.size(), 0.0);

    for (size_t i = 1; i < tempos.size(); ++i)
        tempoSeconds[i] = tempoSeconds[i - 1] + secondsIn (i - 1, tempos[i].ppq);

    signaturePpq.assign (signatures.size(), 0.0);

    for (size_t i = 1; i < signatures.size(); ++i)
    {
        const auto& before = signatures[i - 1];
        signaturePpq[i] = signaturePpq[i - 1] + static_cast<double> (signatures[i].bar - before.bar) * before.numerator * 4.0 / before.denominator;
    }
}

//==============================================================================
double TempoMap::secondsIn (size_t index, double ppq) const noexcept
{
    // From tempos[index] to ppq (at most to the next change).
    const auto& from = tempos[index];
    const auto distance = ppq - from.ppq;

    if (from.ramp && index + 1 < tempos.size())
    {
        const auto& to = tempos[index + 1];
        const auto span = to.ppq - from.ppq;
        const auto slope = span > tiny ? (to.bpm - from.bpm) / span : 0.0;

        // The tempo rises (or falls) in a straight line over the quarter notes: time is its log.
        if (std::abs (slope) > 1.0e-12)
            return 60.0 / slope * std::log ((from.bpm + slope * distance) / from.bpm);
    }

    return distance * 60.0 / from.bpm;
}

double TempoMap::secondsAt (double ppq) const noexcept
{
    if (! std::isfinite (ppq) || ppq <= 0.0)
        return 0.0;

    const auto after = std::upper_bound (tempos.begin(), tempos.end(), ppq, [] (double p, const Tempo& t) { return p < t.ppq; });
    const auto index = static_cast<size_t> (std::distance (tempos.begin(), after) - 1);
    return tempoSeconds[index] + secondsIn (index, ppq);
}

double TempoMap::ppqAt (double seconds) const noexcept
{
    if (! std::isfinite (seconds) || seconds <= 0.0)
        return 0.0;

    const auto after = std::upper_bound (tempoSeconds.begin(), tempoSeconds.end(), seconds);
    const auto index = static_cast<size_t> (std::distance (tempoSeconds.begin(), after) - 1);
    const auto& from = tempos[index];
    const auto into = seconds - tempoSeconds[index];

    if (from.ramp && index + 1 < tempos.size())
    {
        const auto& to = tempos[index + 1];
        const auto span = to.ppq - from.ppq;
        const auto slope = span > tiny ? (to.bpm - from.bpm) / span : 0.0;

        if (std::abs (slope) > 1.0e-12)
            return from.ppq + from.bpm / slope * (std::exp (slope * into / 60.0) - 1.0);
    }

    return from.ppq + into * from.bpm / 60.0;
}

double TempoMap::bpmAt (double ppq) const noexcept
{
    ppq = std::isfinite (ppq) ? juce::jmax (0.0, ppq) : 0.0;
    const auto after = std::upper_bound (tempos.begin(), tempos.end(), ppq, [] (double p, const Tempo& t) { return p < t.ppq; });
    const auto index = static_cast<size_t> (juce::jmax<std::ptrdiff_t> (0, std::distance (tempos.begin(), after) - 1));
    const auto& from = tempos[index];

    if (from.ramp && index + 1 < tempos.size())
    {
        const auto& to = tempos[index + 1];
        const auto span = to.ppq - from.ppq;

        if (span > tiny)
            return from.bpm + (to.bpm - from.bpm) * juce::jlimit (0.0, 1.0, (ppq - from.ppq) / span);
    }

    return from.bpm;
}

//==============================================================================
TempoMap::Bar TempoMap::barAt (double ppq) const noexcept
{
    ppq = std::isfinite (ppq) ? juce::jmax (0.0, ppq) : 0.0;
    const auto after = std::upper_bound (signaturePpq.begin(), signaturePpq.end(), ppq + tiny);
    const auto index = static_cast<size_t> (juce::jmax<std::ptrdiff_t> (0, std::distance (signaturePpq.begin(), after) - 1));
    const auto& signature = signatures[index];
    const auto length = signature.numerator * 4.0 / signature.denominator;
    const auto bars = juce::jlimit (0.0, static_cast<double> (maxBar), std::floor ((ppq - signaturePpq[index]) / length + tiny));

    Bar bar;
    bar.index = juce::jmin (maxBar, signature.bar + static_cast<int> (bars));
    bar.start = signaturePpq[index] + static_cast<double> (bar.index - signature.bar) * length;
    bar.numerator = signature.numerator;
    bar.denominator = signature.denominator;
    return bar;
}

double TempoMap::ppqOfBar (int index) const noexcept
{
    index = juce::jlimit (0, maxBar, index);
    const auto after = std::upper_bound (signatures.begin(), signatures.end(), index, [] (int i, const Signature& s) { return i < s.bar; });
    const auto which = static_cast<size_t> (std::distance (signatures.begin(), after) - 1);
    const auto& signature = signatures[which];
    return signaturePpq[which] + static_cast<double> (index - signature.bar) * signature.numerator * 4.0 / signature.denominator;
}

TempoMap::Signature TempoMap::signatureAt (double ppq) const noexcept
{
    const auto bar = barAt (ppq);
    return { bar.index, bar.numerator, bar.denominator };
}

void TempoMap::setChords (std::vector<Chord> list)
{
    std::vector<Chord> tidy;

    for (auto chord : list)
    {
        if (! std::isfinite (chord.ppq) || chord.ppq < 0.0)
            continue;

        chord.ppq = juce::jmin (chord.ppq, 1.0e7);
        chord.root = ((chord.root % 12) + 12) % 12;
        chord.bass = chord.bass < 0 ? -1 : chord.bass % 12;

        if (chord.bass == chord.root)
            chord.bass = -1;

        if (! getChordQualities().contains (chord.quality))
            chord.quality = {};

        tidy.push_back (chord);
    }

    std::stable_sort (tidy.begin(), tidy.end(), [] (const Chord& a, const Chord& b) { return a.ppq < b.ppq; });

    // One chord to a position: the last given there.
    std::vector<Chord> one;

    for (const auto& chord : tidy)
    {
        if (! one.empty() && std::abs (one.back().ppq - chord.ppq) < tiny)
            one.back() = chord;
        else
            one.push_back (chord);
    }

    chords = std::move (one);
}

const TempoMap::Chord* TempoMap::chordAt (double ppq) const noexcept
{
    if (chords.empty() || ! (ppq + tiny >= chords.front().ppq))
        return nullptr;

    const auto after = std::upper_bound (chords.begin(), chords.end(), ppq + tiny, [] (double p, const Chord& c) { return p < c.ppq; });
    return &*(after - 1);
}

TempoMap::Key TempoMap::keyAt (double ppq) const noexcept
{
    const auto bar = barAt (ppq).index;
    const auto after = std::upper_bound (keys.begin(), keys.end(), bar, [] (int i, const Key& k) { return i < k.bar; });
    return *(after - 1);
}

double TempoMap::nearestBar (double ppq) const noexcept
{
    const auto bar = barAt (ppq);
    return ppq - bar.start < bar.length() / 2.0 ? bar.start : bar.start + bar.length();
}

double TempoMap::quantized (double ppq, double grid, double swing, double strength) const noexcept
{
    const auto t = juce::jmax (0.0, ppq);
    const auto bar = barAt (t);
    const auto barEnd = bar.start + bar.length();
    const auto step = grid > 1.0e-6 ? grid : 0.25;
    auto target = t;

    if (step >= bar.length() - 1.0e-9)
    {
        target = t - bar.start <= barEnd - t ? bar.start : barEnd;
    }
    else
    {
        // The grid from the bar's start, every second point swung later; the bar's end is one too.
        const auto shift = juce::jlimit (0.0, 1.0, swing) * step / 3.0;
        const auto k = static_cast<int> (std::floor ((t - bar.start) / step));
        auto distance = std::abs (barEnd - t);
        target = barEnd;

        for (int j = juce::jmax (0, k - 1); j <= k + 2; ++j)
        {
            const auto at = juce::jmin (barEnd, bar.start + j * step + ((j % 2) != 0 ? shift : 0.0));

            if (std::abs (at - t) < distance)
            {
                target = at;
                distance = std::abs (at - t);
            }
        }
    }

    return t + (target - t) * juce::jlimit (0.0, 1.0, strength);
}

//==============================================================================
TempoMap TempoMap::withTempoAt (double ppq, double bpm) const
{
    auto copy = *this;
    auto list = tempos;
    const auto at = std::isfinite (ppq) ? juce::jmax (0.0, ppq) : 0.0;
    const auto after = std::upper_bound (list.begin(), list.end(), at + tiny, [] (double p, const Tempo& t) { return p < t.ppq; });
    (after - 1)->bpm = bpm;
    copy.setTempos (std::move (list));
    return copy;
}

TempoMap TempoMap::withSignatureAt (double ppq, int numerator, int denominator) const
{
    if (! isValidDenominator (denominator))
        return *this;

    auto copy = *this;
    auto list = signatures;
    const auto bar = barAt (ppq).index;
    const auto after = std::upper_bound (list.begin(), list.end(), bar, [] (int i, const Signature& s) { return i < s.bar; });
    (after - 1)->numerator = numerator;
    (after - 1)->denominator = denominator;
    copy.setSignatures (std::move (list));
    return copy;
}

TempoMap TempoMap::withOneTempo (double bpm) const
{
    auto copy = *this;
    copy.setTempos ({ { 0.0, bpm, false } });
    return copy;
}

//==============================================================================
juce::var TempoMap::toVar() const
{
    juce::Array<juce::var> tempoList, signatureList, keyList;

    for (const auto& tempo : tempos)
        tempoList.add (juce::Array<juce::var> { tempo.ppq, tempo.bpm, tempo.ramp });

    for (const auto& signature : signatures)
        signatureList.add (juce::Array<juce::var> { signature.bar, signature.numerator, signature.denominator });

    for (const auto& key : keys)
        keyList.add (juce::Array<juce::var> { key.bar, key.root, key.scale });

    juce::DynamicObject::Ptr object (new juce::DynamicObject());
    object->setProperty ("tempos", tempoList);
    object->setProperty ("signatures", signatureList);
    object->setProperty ("keys", keyList);

    if (! chords.empty())
    {
        juce::Array<juce::var> chordList;

        for (const auto& chord : chords)
            chordList.add (juce::Array<juce::var> { chord.ppq, chord.root, chord.quality, chord.bass });

        object->setProperty ("chords", chordList);
    }

    return juce::var (object.get());
}

TempoMap TempoMap::fromVar (const juce::var& value)
{
    TempoMap map;

    if (const auto* list = value.getProperty ("tempos", {}).getArray(); list != nullptr && ! list->isEmpty())
    {
        std::vector<Tempo> read;

        for (const auto& item : *list)
            if (const auto* fields = item.getArray(); fields != nullptr && fields->size() >= 2)
                read.push_back ({ number ((*fields)[0], -1.0), number ((*fields)[1], 120.0), fields->size() > 2 && static_cast<bool> ((*fields)[2]) });

        read.erase (std::remove_if (read.begin(), read.end(), [] (const Tempo& t) { return t.ppq < 0.0; }), read.end());

        if (! read.empty())
            map.setTempos (std::move (read));
    }

    if (const auto* list = value.getProperty ("signatures", {}).getArray(); list != nullptr && ! list->isEmpty())
    {
        std::vector<Signature> read;

        for (const auto& item : *list)
            if (const auto* fields = item.getArray(); fields != nullptr && fields->size() >= 3)
                read.push_back ({ whole ((*fields)[0], -1.0), whole ((*fields)[1], 4.0), whole ((*fields)[2], 4.0) });

        read.erase (std::remove_if (read.begin(), read.end(), [] (const Signature& s) { return s.bar < 0; }), read.end());

        if (! read.empty())
            map.setSignatures (std::move (read));
    }

    if (const auto* list = value.getProperty ("keys", {}).getArray(); list != nullptr && ! list->isEmpty())
    {
        std::vector<Key> read;

        for (const auto& item : *list)
            if (const auto* fields = item.getArray(); fields != nullptr && fields->size() >= 3)
                read.push_back ({ whole ((*fields)[0], -1.0), whole ((*fields)[1], 0.0), (*fields)[2].toString() });

        read.erase (std::remove_if (read.begin(), read.end(), [] (const Key& k) { return k.bar < 0; }), read.end());

        if (! read.empty())
            map.setKeys (std::move (read));
    }

    if (const auto* list = value.getProperty ("chords", {}).getArray(); list != nullptr && ! list->isEmpty())
    {
        std::vector<Chord> read;

        for (const auto& item : *list)
            if (const auto* fields = item.getArray(); fields != nullptr && fields->size() >= 3)
                read.push_back ({ number ((*fields)[0], -1.0), whole ((*fields)[1], 0.0), (*fields)[2].toString(),
                                  fields->size() > 3 ? whole ((*fields)[3], -1.0) : -1 });

        map.setChords (std::move (read));
    }

    return map;
}
