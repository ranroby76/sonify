// C:\workspace\Fanan_web_hub\src\Automation.h

#pragma once

#include <juce_core/juce_core.h>

#include <algorithm>
#include <vector>

//==============================================================================
/**
    Automation, as in Studio One: an envelope of points (song position in quarter
    notes, value) for something on a track, joined by straight lines. Before the first
    point the envelope holds the first value; after the last, the last.

    Values are in the target's own units: dB for volume, -1..+1 for pan, and the
    parameter's own range for a plugin parameter.
*/
struct AutomationPoint
{
    double ppq = 0.0;
    double value = 0.0;
};

struct AutomationTarget
{
    enum class Kind { volume, pan, parameter };

    Kind kind = Kind::volume;
    int slot = 0;             // parameter: 0 the instrument, otherwise an insert's id
    uint32_t paramId = 0;     // parameter: its stable id

    bool operator== (const AutomationTarget& other) const noexcept
    {
        return kind == other.kind && (kind != Kind::parameter || (slot == other.slot && paramId == other.paramId));
    }
};

struct AutomationLane
{
    AutomationTarget target;
    std::vector<AutomationPoint> points;   // by position

    /** The Automation view's shape regions on it (a repeating shape drawn over a stretch, kept to
        edit again): the page's, kept with the song as they are. What plays is the points, which
        the page makes from them. */
    juce::var shapes;
};

/** A track's automation mode, as Studio One's:
        off     its envelopes rest
        read    its envelopes move its controls as the song plays
        touch   as read, but a control taken hold of while the song plays is recorded, until it's
                let go; then its envelope takes over again
        latch   as touch, but once taken hold of it goes on being recorded (at its last value)
                until the song stops
        write   every envelope it has is recorded from the start, at its control's value (and any
                control taken hold of), until the song stops; then it becomes touch */
enum class AutomationMode { off, read, touch, latch, write };

namespace Automation
{
    inline bool reads (int mode) noexcept        { return mode != static_cast<int> (AutomationMode::off); }
    inline bool records (int mode) noexcept      { return mode == static_cast<int> (AutomationMode::touch) || mode == static_cast<int> (AutomationMode::latch)
                                                          || mode == static_cast<int> (AutomationMode::write); }

    inline juce::String modeName (int mode)
    {
        switch (static_cast<AutomationMode> (mode))
        {
            case AutomationMode::off:    return "off";
            case AutomationMode::touch:  return "touch";
            case AutomationMode::latch:  return "latch";
            case AutomationMode::write:  return "write";
            case AutomationMode::read:
            default:                     return "read";
        }
    }

    inline AutomationMode modeFromName (const juce::String& name)
    {
        return name == "off" ? AutomationMode::off : name == "touch" ? AutomationMode::touch : name == "latch" ? AutomationMode::latch
             : name == "write" ? AutomationMode::write : AutomationMode::read;
    }

    /** A target as one number (the held targets a track keeps for the audio thread); never 0. */
    inline juce::uint64 keyOf (const AutomationTarget& target) noexcept
    {
        return (static_cast<juce::uint64> (static_cast<int> (target.kind) + 1) << 62)
             | (target.kind == AutomationTarget::Kind::parameter ? (static_cast<juce::uint64> (static_cast<juce::uint32> (target.slot) & 0x3fffffffu) << 32)
                                                                     | static_cast<juce::uint64> (target.paramId)
                                                                 : 0u);
    }
}

namespace Automation
{
    /** The envelope's value at a song position. */
    inline double valueAt (const std::vector<AutomationPoint>& points, double ppq) noexcept
    {
        if (points.empty())
            return 0.0;

        if (ppq <= points.front().ppq)
            return points.front().value;

        if (ppq >= points.back().ppq)
            return points.back().value;

        const auto after = std::upper_bound (points.begin(), points.end(), ppq,
                                             [] (double position, const AutomationPoint& point) { return position < point.ppq; });
        const auto& b = *after;
        const auto& a = *(after - 1);
        const auto span = b.ppq - a.ppq;
        return span > 0.0 ? a.value + (b.value - a.value) * (ppq - a.ppq) / span : b.value;
    }

    /** Points sorted by position, with nonsense (non-finite) values dropped. */
    inline std::vector<AutomationPoint> tidy (std::vector<AutomationPoint> points)
    {
        points.erase (std::remove_if (points.begin(), points.end(), [] (const AutomationPoint& p)
                                      { return ! std::isfinite (p.ppq) || ! std::isfinite (p.value) || p.ppq < 0.0; }),
                      points.end());
        std::stable_sort (points.begin(), points.end(), [] (const AutomationPoint& a, const AutomationPoint& b) { return a.ppq < b.ppq; });
        return points;
    }
}

//==============================================================================
/**
    A track's automation for the audio thread: built on the message thread when the
    envelopes change, handed over whole, then only used by the audio thread (which
    keeps its own notes in it: the last value it sent, and where a parameter is).
*/
struct AutomationPlayback
{
    struct Entry
    {
        AutomationTarget target;
        std::vector<AutomationPoint> points;
        int parameterIndex = -1;    // audio thread: where the parameter was last found
        double lastValue = std::numeric_limits<double>::quiet_NaN();
    };

    std::vector<Entry> entries;
};
