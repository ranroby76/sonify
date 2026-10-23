// D:\workspace\Fanan_web_hub\src\sonify_device.h
//
// Sonify devices: the API a device is written against.
//
// A device is a small C++ program that sits in an instrument track's MIDI FX slot, in front of
// the instrument: it hears the notes (the clips' and the ones played live), plays notes of its
// own, and can move Sonify's own controls (the transport, faders, pan, mute, solo, any plugin
// knob it's mapped to). Sonify compiles it to WebAssembly on this computer and runs it in a
// sandbox: it can't reach files, the network or anything outside itself.
//
// A device in short:
//
//     #include "sonify_device.h"
//     using namespace sonify;
//
//     class Octaver : public Device
//     {
//     public:
//         Param shift { "shift", "Shift", -2, 2, 1, Unit::number };
//
//         void process (Block& block) override
//         {
//             for (const auto& e : block.events())
//             {
//                 block.play (e);                                           // the note itself
//                 if (e.isNote())
//                     block.play (e.withPitch (e.pitch() + 12 * shift.integer()));
//             }
//         }
//     };
//
//     SONIFY_DEVICE (Octaver)
//
// Time is in quarter notes on the device's clock: it runs with the song while it plays (and its
// grid is the song's), and at the song's tempo while it's stopped. A block covers
// [block.start(), block.end()). Events played at a later time than the block's end are kept by
// Sonify and played on time; every note a device starts is ended by Sonify when the device is
// switched off, removed or the song stops jumping around, so nothing hangs.
//
// Sonify includes this file in Sonify itself too, for the shared layout (sonify::abi) only.

#pragma once

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <limits>

namespace sonify
{
    // [internal]
    //==========================================================================
    // What Sonify and a device share, laid out the same on both sides. Not for devices to use.
    namespace abi
    {
        constexpr int version = 1;
        constexpr int maxParams = 12;
        constexpr int maxOutputs = 8;
        constexpr int maxEvents = 1024;
        constexpr int maxPieces = 34;
        constexpr int maxMarks = 96;
        constexpr int maxControls = 64;
        constexpr int targetSize = 56;
        constexpr int logSize = 2048;

        struct Event
        {
            double time;
            std::uint8_t data[3];
            std::uint8_t size;
            std::uint8_t pad[4];
        };

        struct Piece
        {
            double clock, perSample, song;
            std::int32_t offset, length, playing, segment;
        };

        struct Mark
        {
            double ppq, barLength, bpm;
            std::int32_t segment, barLine, bar, keyRoot, keyNotes, pad;
        };

        enum ControlKind : std::int32_t { controlTarget = 0, controlOutput = 1 };

        struct Control
        {
            char target[targetSize];     // controlTarget: what to move ("track.2.volume"); controlOutput: unused
            float value;                 // 0 .. 1
            std::int32_t kind;
            std::int32_t output;         // controlOutput: which output
            std::int32_t pad;
        };

        struct IO
        {
            std::int32_t abiVersion;
            std::int32_t reset;          // Sonify sets it: forget what you were doing, before this block
            double sampleRate;
            double start, end;           // the block on the clock, [start, end)
            double bpm;
            std::int32_t offline;        // run over a clip (Apply, or a test), not live
            std::int32_t numPieces;
            std::int32_t numMarks;
            std::int32_t numIn;
            std::int32_t numOut;
            std::int32_t numControls;
            std::int32_t logLength;
            std::int32_t pad;
            Piece pieces[maxPieces];
            Mark marks[maxMarks];
            float params[maxParams];
            Event in[maxEvents];
            Event out[maxEvents];
            Control controls[maxControls];
            char log[logSize];
        };

        static_assert (sizeof (Event) == 16, "Event layout");
        static_assert (sizeof (Piece) == 40, "Piece layout");
        static_assert (sizeof (Mark) == 48, "Mark layout");
        static_assert (sizeof (Control) == 72, "Control layout");
        static_assert (offsetof (IO, pieces) == 72, "IO layout");
        static_assert (sizeof (IO) == 72 + 40 * maxPieces + 48 * maxMarks + 4 * maxParams + 2 * 16 * maxEvents
                                         + 72 * maxControls + logSize, "IO layout");
    }
    // [/internal]

#if defined (__wasm__)
    //==========================================================================
    /** How a parameter's value reads on the device's panel. */
    enum class Unit
    {
        number,       // a whole number
        percent,      // 0 .. 100
        semitones,
        steps,        // steps of the song's key
        pitch,        // a MIDI note (60 is middle C, shown as C4)
        rate,         // a note value: 0 1/1, 1 1/2, 2 1/2 dotted, 3 1/2 triplet, 4 1/4, 5 1/4 dotted, 6 1/4 triplet,
                      // 7 1/8, 8 1/8 dotted, 9 1/8 triplet, 10 1/16, 11 1/16 dotted, 12 1/16 triplet, 13 1/32,
                      // 14 1/32 triplet, 15 1/64 (Param::length() gives it in quarter notes)
        choice,       // one of the names given as "First|Second|Third" (0, 1, 2...)
        toggle,       // 0 off, 1 on
        ms,           // milliseconds
        bars,
        decimal       // any value in the range
    };

    //==========================================================================
    /** One MIDI message, at a time on the clock (quarter notes). Channels are 0..15 (0 is MIDI
        channel 1), pitches and values 0..127. */
    struct Event
    {
        double time = 0.0;
        std::uint8_t data[3] {};
        std::uint8_t size = 0;

        bool isNoteOn() const noexcept         { return size == 3 && (data[0] & 0xf0) == 0x90 && data[2] > 0; }
        bool isNoteOff() const noexcept        { return size == 3 && ((data[0] & 0xf0) == 0x80 || ((data[0] & 0xf0) == 0x90 && data[2] == 0)); }
        bool isNote() const noexcept           { return isNoteOn() || isNoteOff(); }
        bool isController() const noexcept     { return size == 3 && (data[0] & 0xf0) == 0xb0; }
        bool isPitchBend() const noexcept      { return size == 3 && (data[0] & 0xf0) == 0xe0; }
        bool isAftertouch() const noexcept     { return (size == 2 && (data[0] & 0xf0) == 0xd0) || (size == 3 && (data[0] & 0xf0) == 0xa0); }
        bool isProgramChange() const noexcept  { return size == 2 && (data[0] & 0xf0) == 0xc0; }

        int channel() const noexcept           { return data[0] & 0x0f; }
        int pitch() const noexcept             { return data[1] & 0x7f; }       // notes, poly aftertouch
        int velocity() const noexcept          { return data[2] & 0x7f; }       // notes (0 for a note-off)
        int controller() const noexcept        { return data[1] & 0x7f; }       // controllers
        int value() const noexcept             { return data[2] & 0x7f; }       // controllers
        int bend() const noexcept              { return ((data[2] & 0x7f) << 7 | (data[1] & 0x7f)) - 8192; }   // -8192 .. 8191

        static Event noteOn (double time, int channel, int pitch, int velocity) noexcept
        {
            return make (time, 0x90 | (channel & 15), clamp (pitch, 0, 127), clamp (velocity, 1, 127), 3);
        }

        static Event noteOff (double time, int channel, int pitch) noexcept
        {
            return make (time, 0x80 | (channel & 15), clamp (pitch, 0, 127), 0, 3);
        }

        static Event controllerChange (double time, int channel, int number, int value) noexcept
        {
            return make (time, 0xb0 | (channel & 15), clamp (number, 0, 127), clamp (value, 0, 127), 3);
        }

        static Event pitchBend (double time, int channel, int bend) noexcept   // -8192 .. 8191
        {
            const auto v = clamp (bend + 8192, 0, 16383);
            return make (time, 0xe0 | (channel & 15), v & 0x7f, (v >> 7) & 0x7f, 3);
        }

        static Event programChange (double time, int channel, int program) noexcept
        {
            return make (time, 0xc0 | (channel & 15), clamp (program, 0, 127), 0, 2);
        }

        /** The same message at another time, on another channel, or (notes) another pitch or velocity. */
        Event at (double newTime) const noexcept           { auto e = *this; e.time = newTime; return e; }
        Event onChannel (int newChannel) const noexcept    { auto e = *this; e.data[0] = static_cast<std::uint8_t> ((data[0] & 0xf0) | (newChannel & 15)); return e; }
        Event withPitch (int newPitch) const noexcept      { auto e = *this; e.data[1] = static_cast<std::uint8_t> (clamp (newPitch, 0, 127)); return e; }

        Event withVelocity (int newVelocity) const noexcept
        {
            auto e = *this;

            if (isNoteOn())
                e.data[2] = static_cast<std::uint8_t> (clamp (newVelocity, 1, 127));

            return e;
        }

        static int clamp (int v, int lo, int hi) noexcept  { return v < lo ? lo : (v > hi ? hi : v); }

    private:
        static Event make (double time, int status, int d1, int d2, int size) noexcept
        {
            Event e;
            e.time = time;
            e.data[0] = static_cast<std::uint8_t> (status);
            e.data[1] = static_cast<std::uint8_t> (d1);
            e.data[2] = static_cast<std::uint8_t> (d2);
            e.size = static_cast<std::uint8_t> (size);
            return e;
        }
    };

    static_assert (sizeof (Event) == sizeof (abi::Event) && offsetof (Event, data) == offsetof (abi::Event, data)
                    && offsetof (Event, size) == offsetof (abi::Event, size), "Event layout");

    //==========================================================================
    /** A key: its root (0 C .. 11 B) and its scale's notes above the root, one bit a semitone
        (bit 0 the root). The song channel sets it; with no key set it's C major. */
    struct Key
    {
        int root = 0;
        std::uint16_t notes = 0x0ab5;   // major

        static constexpr std::uint16_t major = 0x0ab5, minor = 0x05ad, chromatic = 0x0fff;

        bool contains (int pitch) const noexcept
        {
            if (notes == 0)
                return true;

            const auto step = ((pitch - root) % 12 + 12) % 12;
            return ((notes >> step) & 1) != 0;
        }

        /** The key's note nearest a pitch: direction 0 the nearer (the lower on a tie), 1 up, -1 down. */
        int snap (int pitch, int direction = 0) const noexcept
        {
            if (contains (pitch))
                return pitch;

            for (int distance = 1; distance < 12; ++distance)
            {
                if (direction <= 0 && contains (pitch - distance))
                    return pitch - distance;

                if (direction >= 0 && contains (pitch + distance))
                    return pitch + distance;
            }

            return pitch;
        }

        /** steps notes of the key up (down when negative) from a pitch: 2 steps up from C in
            C major is E. A pitch outside the key moves with the key's note below it. */
        int shift (int pitch, int steps) const noexcept
        {
            if (steps == 0)
                return pitch;

            auto base = snap (pitch, -1);
            const auto offset = pitch - base;
            const int direction = steps > 0 ? 1 : -1;

            for (int i = 0; i < (steps > 0 ? steps : -steps); ++i)
            {
                auto next = base + direction;

                for (int guard = 0; guard < 12 && ! contains (next); ++guard)
                    next += direction;

                base = next;
            }

            return base + offset;
        }

        /** A chord's notes in this key on a pitch: thirds stacked from the scale (count 3 a
            triad, 4 a seventh...); a chromatic scale gives the pitch's own major. Returns how
            many went into out. */
        int chord (int pitch, int count, int* out) const noexcept
        {
            Key k = *this;

            if (notes == 0 || notes == chromatic)
                k = { ((pitch % 12) + 12) % 12, major };

            for (int i = 0; i < count; ++i)
                out[i] = k.shift (pitch, 2 * i);

            return count;
        }
    };

    //==========================================================================
    class Device;

    // [internal]
    namespace detail
    {
        constexpr double eps = 1.0e-9;

        struct ParamInfo
        {
            const char* id;
            const char* name;
            float min, max, defaultValue;
            Unit unit;
            const char* choices;
        };

        struct OutputInfo
        {
            const char* id;
            const char* name;
        };

        inline abi::IO io {};
        inline ParamInfo params[abi::maxParams] {};
        inline int numParams = 0;
        inline OutputInfo outputs[abi::maxOutputs] {};
        inline int numOutputs = 0;
        inline Device* device = nullptr;
        inline char description[8192] {};
        inline float lastOutput[abi::maxOutputs] {};
        inline bool outputSent[abi::maxOutputs] {};

        inline int paramsWanted = 0, outputsWanted = 0;

        inline int registerParam (const ParamInfo& info) noexcept
        {
            ++paramsWanted;

            if (numParams >= abi::maxParams)
                return -1;

            params[numParams] = info;
            return numParams++;
        }

        inline int registerOutput (const OutputInfo& info) noexcept
        {
            ++outputsWanted;

            if (numOutputs >= abi::maxOutputs)
                return -1;

            outputs[numOutputs] = info;
            return numOutputs++;
        }

        inline void addControl (int kind, const char* target, int output, float value) noexcept
        {
            if (! (value == value))   // NaN
                return;

            value = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);

            // One move a block for each control: the last one counts.
            for (int i = 0; i < io.numControls; ++i)
            {
                auto& c = io.controls[i];

                if (c.kind == kind && (kind == abi::controlOutput ? c.output == output : std::strncmp (c.target, target, abi::targetSize) == 0))
                {
                    c.value = value;
                    return;
                }
            }

            if (io.numControls >= abi::maxControls)
                return;

            auto& c = io.controls[io.numControls++];
            std::memset (c.target, 0, sizeof (c.target));

            if (target != nullptr)
                std::strncpy (c.target, target, abi::targetSize - 1);

            c.value = value;
            c.kind = kind;
            c.output = output;
        }
    }
    // [/internal]

    //==========================================================================
    /**
        A setting of the device: a knob, a choice or a switch on its panel, saved with the song.
        Declare them as members of the device, in the order the panel shows them (12 at most):

            Param rate   { "rate", "Rate", 0, 15, 10, Unit::rate };           // 1/16
            Param mode   { "mode", "Mode", 0, 2, 0, Unit::choice, "Up|Down|Random" };
            Param gate   { "gate", "Gate", 5, 100, 80, Unit::percent };
            Param latch  { "latch", "Latch", 0, 1, 0, Unit::toggle };

        The id stays the same for good (songs keep values by it); the name is what the panel says.
        Values of every unit but percent, decimal and ms are whole numbers.
    */
    class Param
    {
    public:
        Param (const char* id, const char* name, float min, float max, float defaultValue,
               Unit unit = Unit::number, const char* choices = nullptr) noexcept
            : index (detail::registerParam ({ id, name, min, max, defaultValue, unit, choices })),
              fallback (defaultValue)
        {
        }

        /** Its value now (the panel, automation and MIDI Link move it any time). */
        float value() const noexcept
        {
            return index >= 0 ? detail::io.params[index] : fallback;
        }

        operator float() const noexcept           { return value(); }
        int integer() const noexcept              { return static_cast<int> (std::lround (value())); }
        bool on() const noexcept                  { return value() >= 0.5f; }
        int choice() const noexcept               { return integer(); }

        /** A rate param's note value, in quarter notes (1/16 is 0.25). */
        double length() const noexcept
        {
            static constexpr double lengths[] = { 4.0, 2.0, 3.0, 4.0 / 3.0, 1.0, 1.5, 2.0 / 3.0, 0.5, 0.75, 1.0 / 3.0,
                                                  0.25, 0.375, 1.0 / 6.0, 0.125, 1.0 / 12.0, 0.0625 };
            auto i = integer();
            i = i < 0 ? 0 : (i > 15 ? 15 : i);
            return lengths[i];
        }

    private:
        int index;
        float fallback;
    };

    //==========================================================================
    /**
        One of the device's outputs: a Map button on its panel. The user maps it to any of
        Sonify's controls (a fader, a pan knob, mute, a plugin's knob, play...), and set() moves
        whatever it's mapped to: 0 .. 1 is the control's whole range (a button is on from 0.5).
        Up to 8, declared as members like params:

            Output lfo { "lfo", "LFO" };
            ...
            lfo.set (0.5f + 0.5f * std::sin (phase));

        Moves reach Sonify's controls up to 60 times a second (the last of each block counts), and
        only when the value changes.
    */
    class Output
    {
    public:
        Output (const char* id, const char* name) noexcept
            : index (detail::registerOutput ({ id, name }))
        {
        }

        void set (float value) noexcept
        {
            if (index < 0)
                return;

            // Only changes are sent.
            if (detail::outputSent[index] && std::fabs (detail::lastOutput[index] - value) < 1.0e-4f)
                return;

            detail::lastOutput[index] = value;
            detail::outputSent[index] = true;
            detail::addControl (abi::controlOutput, nullptr, index, value);
        }

    private:
        int index;
    };

    /**
        Moves one of Sonify's own controls, named the way MIDI Link names them, to value (0 .. 1,
        the control's whole range; a button is on from 0.5):

            track.<n>.volume | pan | mute | solo | arm | monitor | send.<k>   the n-th track (from 1)
            fx.<n>.volume | pan | mute | solo | send.<k>                      the n-th FX channel
            bus.<n>.volume | pan | mute | solo | send.<k>                     the n-th bus
            main.volume | mute
            transport.play | stop | record | loop | metronome | start         start: back to the song's start

        A fader's 0 .. 1 is its travel (about 0.8 is 0 dB); pan 0 is left, 0.5 the middle, 1 right.
        Sonify acts when a control's value changes: a fader moves, a button switches (mute 1 mutes,
        0 unmutes; transport.play 1 plays, 0 stops; stop and start act when they go to 1).
    */
    inline void control (const char* target, float value) noexcept
    {
        detail::addControl (abi::controlTarget, target, 0, value);
    }

    /** A line for the device's log (Device Studio shows it; tests return it). printf-style. */
    inline void log (const char* format, ...) noexcept
    {
        auto& io = detail::io;

        if (io.logLength >= abi::logSize - 2)
            return;

        va_list args;
        va_start (args, format);
        const auto room = abi::logSize - 1 - io.logLength;
        const auto written = std::vsnprintf (io.log + io.logLength, static_cast<size_t> (room), format, args);
        va_end (args);

        if (written < 0)
            return;

        io.logLength += written < room ? written : room - 1;

        if (io.logLength < abi::logSize - 1 && (io.logLength == 0 || io.log[io.logLength - 1] != '\n'))
            io.log[io.logLength++] = '\n';

        io.log[io.logLength] = 0;
    }

    //==========================================================================
    /** A line of a grid in the block: its time on the clock, and its number counted from the
        song's start (while it plays) or from the origin given (while it's stopped). */
    struct GridLine
    {
        double time = 0.0;
        long long number = 0;
    };

    /** A bar line in the block (only while the song plays): its time, the bar (from 0) and that
        bar's length in quarter notes. */
    struct BarLine
    {
        double time = 0.0;
        int bar = 0;
        double length = 4.0;
    };

    /**
        One block: the events that came in (events(), in time order), the time it covers, and the
        music there. play() what the device plays.
    */
    class Block
    {
    public:
        //======================================================================
        // What came in, in time order: for (const auto& e : block.events()) ...
        struct Events
        {
            const Event* first;
            int count;

            const Event* begin() const noexcept         { return first; }
            const Event* end() const noexcept           { return first + count; }
            int size() const noexcept                   { return count; }
            const Event& operator[] (int i) const noexcept   { return first[i]; }
        };

        Events events() const noexcept                  { return { reinterpret_cast<const Event*> (io().in), io().numIn }; }

        //======================================================================
        /** Plays an event at its time: now (a time before the block's start plays at its start)
            or later (Sonify keeps it and plays it on time). False when the block is full
            (1024 events). */
        bool play (const Event& e) noexcept
        {
            auto& o = io();

            if (o.numOut >= abi::maxEvents || e.size == 0)
                return false;

            auto& out = o.out[o.numOut++];
            out.time = e.time < o.start ? o.start : e.time;
            out.data[0] = e.data[0];
            out.data[1] = e.data[1];
            out.data[2] = e.data[2];
            out.size = e.size;
            return true;
        }

        /** A note from start for length quarter notes (its end is played later on its own). */
        bool playNote (double start, double length, int channel, int pitch, int velocity) noexcept
        {
            if (pitch < 0 || pitch > 127)
                return false;

            return play (Event::noteOn (start, channel, pitch, velocity))
                && play (Event::noteOff (start + (length > 1.0e-4 ? length : 1.0e-4), channel, pitch));
        }

        //======================================================================
        // Time.
        double start() const noexcept                   { return io().start; }
        double end() const noexcept                     { return io().end; }
        double bpm() const noexcept                     { return io().bpm; }
        bool isOffline() const noexcept                 { return io().offline != 0; }
        double sampleRate() const noexcept              { return io().sampleRate; }

        /** Milliseconds as quarter notes at the block's tempo. */
        double ms (double milliseconds) const noexcept  { return milliseconds / 60000.0 * bpm(); }

        bool isPlaying() const noexcept
        {
            for (int i = 0; i < io().numPieces; ++i)
                if (io().pieces[i].playing)
                    return true;

            return false;
        }

        bool isPlayingAt (double time) const noexcept   { return pieceAt (time).playing != 0; }

        /** The song position (quarter notes from the song's start) at a time on the clock while
            the song plays; NaN while it's stopped. */
        double songAt (double time) const noexcept
        {
            const auto& piece = pieceAt (time);
            return piece.playing ? piece.song + (time - piece.clock) : std::numeric_limits<double>::quiet_NaN();
        }

        /** The song's key where a time is (the song channel's). */
        Key keyAt (double time) const noexcept
        {
            const auto& mark = markAt (time);
            return { mark.keyRoot, static_cast<std::uint16_t> (mark.keyNotes) };
        }

        Key key() const noexcept                        { return keyAt (start()); }

        /** The length of the bar a time is in, in quarter notes (4 in 4/4, 3 in 3/4). */
        double barLengthAt (double time) const noexcept { return markAt (time).barLength; }

        /** The tempo where a time is. */
        double bpmAt (double time) const noexcept       { return markAt (time).bpm; }

        /** The lines of a grid of step quarter notes (0.25 is 1/16) in the block, in order, into
            out (at most max); returns how many. While the song plays they're the song's own grid;
            while it's stopped, steps counted from origin, a time on the clock the device chose
            (the first note it heard, say): none while origin is NaN. */
        int grid (double step, double origin, GridLine* out, int max) const noexcept
        {
            if (! (step > 0.0) || max <= 0)
                return 0;

            int count = 0;

            for (int i = 0; i < io().numPieces && count < max; ++i)
            {
                const auto& piece = io().pieces[i];
                const auto span = piece.perSample * piece.length;

                if (span <= 0.0)
                    continue;

                const auto base = piece.playing ? piece.song : origin;

                if (std::isnan (base))
                    continue;

                const auto from = piece.playing ? piece.song : piece.clock - origin;
                const auto to = from + span;
                auto n = static_cast<long long> (std::ceil ((from - detail::eps) / step));

                for (; count < max; ++n)
                {
                    const auto line = static_cast<double> (n) * step;

                    if (line >= to - detail::eps)
                        break;

                    const auto offset = line - from;
                    out[count++] = { piece.clock + (offset > 0.0 ? offset : 0.0), n };
                }
            }

            return count;
        }

        /** The bar lines in the block while the song plays (none while it's stopped). */
        int bars (BarLine* out, int max) const noexcept
        {
            int count = 0;

            for (int i = 0; i < io().numMarks && count < max; ++i)
            {
                const auto& mark = io().marks[i];

                if (! mark.barLine || mark.segment < 0)
                    continue;

                for (int p = 0; p < io().numPieces; ++p)
                {
                    const auto& piece = io().pieces[p];

                    if (piece.playing && piece.segment == mark.segment)
                    {
                        const auto offset = mark.ppq - piece.song;
                        out[count++] = { piece.clock + (offset > 0.0 ? offset : 0.0), mark.bar, mark.barLength };
                        break;
                    }
                }
            }

            return count;
        }

    private:
        static abi::IO& io() noexcept                   { return detail::io; }

        const abi::Piece& pieceAt (double time) const noexcept
        {
            int index = 0;

            while (index + 1 < io().numPieces && io().pieces[index + 1].clock <= time)
                ++index;

            return io().pieces[index];
        }

        const abi::Mark& markAt (double time) const noexcept
        {
            static const abi::Mark none { 0.0, 4.0, 120.0, -1, 0, 0, 0, Key::major, 0 };
            const auto count = io().numMarks;

            if (count == 0)
                return none;

            const auto& piece = pieceAt (time);

            if (! piece.playing)
            {
                for (int i = 0; i < count; ++i)
                    if (io().marks[i].segment < 0)
                        return io().marks[i];

                return io().marks[count - 1];
            }

            const auto song = piece.song + (time - piece.clock);
            const abi::Mark* found = nullptr;

            for (int i = 0; i < count; ++i)
            {
                const auto& mark = io().marks[i];

                if (mark.segment != piece.segment)
                    continue;

                if (found == nullptr || mark.ppq <= song + 1.0e-7)
                    found = &mark;
            }

            return found != nullptr ? *found : io().marks[0];
        }
    };

    //==========================================================================
    /**
        The device. Its members keep what it needs between blocks. Sonify makes one for each
        place it's in (each track's slot), and calls process() for every block of audio
        (about every 10 ms) and, for Apply on a clip or a test, faster than real time.
    */
    class Device
    {
    public:
        virtual ~Device() = default;

        /** Once, before the first block. */
        virtual void prepare (double /*sampleRate*/) {}

        /** Hears the block's events and plays what it plays (block.play). Events it doesn't play
            don't reach the instrument: a device that only adds notes plays what came in too. */
        virtual void process (Block& block) = 0;

        /** Forget what you were doing (held notes, steps waiting, phases): the song jumped, the
            device was switched off, or a clip's Apply starts. Sonify already ends every note the
            device played. */
        virtual void reset() {}
    };

    // [internal]
    //==========================================================================
    namespace detail
    {
        inline void appendText (char*& at, const char* end, const char* text) noexcept
        {
            while (*text != 0 && at < end - 1)
                *at++ = *text++;

            *at = 0;
        }

        inline void appendQuoted (char*& at, const char* end, const char* text) noexcept
        {
            appendText (at, end, "\"");

            for (; text != nullptr && *text != 0 && at < end - 3; ++text)
            {
                const auto c = static_cast<unsigned char> (*text);

                if (c == '"' || c == '\\')
                {
                    *at++ = '\\';
                    *at++ = static_cast<char> (c);
                }
                else if (c >= 0x20)
                {
                    *at++ = static_cast<char> (c);
                }
            }

            *at = 0;
            appendText (at, end, "\"");
        }

        inline void appendNumber (char*& at, const char* end, double value) noexcept
        {
            char text[48];
            std::snprintf (text, sizeof (text), "%.9g", std::isfinite (value) ? value : 0.0);
            appendText (at, end, text);
        }

        inline const char* unitName (Unit unit) noexcept
        {
            switch (unit)
            {
                case Unit::number:    return "number";
                case Unit::percent:   return "percent";
                case Unit::semitones: return "semitones";
                case Unit::steps:     return "steps";
                case Unit::pitch:     return "pitch";
                case Unit::rate:      return "rate";
                case Unit::choice:    return "choice";
                case Unit::toggle:    return "toggle";
                case Unit::ms:        return "ms";
                case Unit::bars:      return "bars";
                case Unit::decimal:   return "decimal";
            }

            return "number";
        }

        /** { "abi": 1, "params": [{ id, name, min, max, default, unit, choices }], "outputs": [{ id, name }],
              "paramsWanted": n, "outputsWanted": n } (wanted: how many it declared, kept or not) */
        inline void describe() noexcept
        {
            char* at = description;
            const char* end = description + sizeof (description);
            appendText (at, end, "{\"abi\":1,\"params\":[");

            for (int i = 0; i < numParams; ++i)
            {
                const auto& p = params[i];

                if (i > 0)
                    appendText (at, end, ",");

                appendText (at, end, "{\"id\":");
                appendQuoted (at, end, p.id);
                appendText (at, end, ",\"name\":");
                appendQuoted (at, end, p.name);
                appendText (at, end, ",\"min\":");
                appendNumber (at, end, p.min);
                appendText (at, end, ",\"max\":");
                appendNumber (at, end, p.max);
                appendText (at, end, ",\"default\":");
                appendNumber (at, end, p.defaultValue);
                appendText (at, end, ",\"unit\":");
                appendQuoted (at, end, unitName (p.unit));
                appendText (at, end, ",\"choices\":");
                appendQuoted (at, end, p.choices != nullptr ? p.choices : "");
                appendText (at, end, "}");
            }

            appendText (at, end, "],\"outputs\":[");

            for (int i = 0; i < numOutputs; ++i)
            {
                if (i > 0)
                    appendText (at, end, ",");

                appendText (at, end, "{\"id\":");
                appendQuoted (at, end, outputs[i].id);
                appendText (at, end, ",\"name\":");
                appendQuoted (at, end, outputs[i].name);
                appendText (at, end, "}");
            }

            appendText (at, end, "],\"paramsWanted\":");
            appendNumber (at, end, paramsWanted);
            appendText (at, end, ",\"outputsWanted\":");
            appendNumber (at, end, outputsWanted);
            appendText (at, end, "}");
        }

        template <typename DeviceClass>
        int create (double sampleRate) noexcept
        {
            if (device != nullptr)
                return 1;

            numParams = 0;
            numOutputs = 0;
            paramsWanted = 0;
            outputsWanted = 0;
            io.abiVersion = abi::version;
            io.sampleRate = sampleRate;
            device = new DeviceClass();

            for (int i = 0; i < numParams; ++i)
                io.params[i] = params[i].defaultValue;

            describe();
            device->prepare (sampleRate);
            return 1;
        }

        inline void process() noexcept
        {
            io.numOut = 0;
            io.numControls = 0;

            if (io.reset != 0)
            {
                io.reset = 0;

                for (auto& sent : outputSent)
                    sent = false;

                if (device != nullptr)
                    device->reset();
            }

            if (device == nullptr)
                return;

            Block block;
            device->process (block);
        }
    }
    // [/internal]
#endif
}

#if defined (__wasm__)
/** Makes the class the device: write it once, after the class, at the end of the file. */
 #define SONIFY_DEVICE(DeviceClass) \
    extern "C" __attribute__ ((export_name ("sonify_abi"))) int sonify_abi() { return ::sonify::abi::version; } \
    extern "C" __attribute__ ((export_name ("sonify_create"))) int sonify_create (double sampleRate) { return ::sonify::detail::create<DeviceClass> (sampleRate); } \
    extern "C" __attribute__ ((export_name ("sonify_io"))) void* sonify_io() { return &::sonify::detail::io; } \
    extern "C" __attribute__ ((export_name ("sonify_describe"))) const char* sonify_describe() { return ::sonify::detail::description; } \
    extern "C" __attribute__ ((export_name ("sonify_process"))) void sonify_process() { ::sonify::detail::process(); }
#endif
