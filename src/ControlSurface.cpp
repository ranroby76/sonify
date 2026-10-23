// D:\workspace\Fanan_web_hub\src\ControlSurface.cpp

#include "ControlSurface.h"

#include <algorithm>
#include <cmath>

namespace
{
    // Studio One's fader scale, as the page's (hub-core.js): position, dB.
    constexpr std::array<std::pair<float, float>, 10> faderLaw {{
        { 0.0f, -144.0f }, { 0.05f, -72.0f }, { 0.14f, -48.0f }, { 0.24f, -36.0f }, { 0.36f, -24.0f },
        { 0.52f, -12.0f }, { 0.64f, -6.0f }, { 0.78f, 0.0f }, { 0.91f, 6.0f }, { 1.0f, 10.0f } }};

    constexpr int fullScale = 16383;          // a fader's fourteen bits
    constexpr int faderSlack = 8;             // closer than this, a motor fader isn't moved
    constexpr int timerMs = 20;
    constexpr double settleMs = 300.0;        // a fader moved by hand isn't moved back meanwhile
    constexpr double movedMs = 1500.0;        // the page follows a surface's moves this long
    constexpr double meterMs = 100.0;
    constexpr double clockMs = 50.0;          // the time display
    constexpr double pingMs = 1000.0;         // HUI goes offline without it
    constexpr double refreshMs = 10000.0;     // everything again, for a surface switched on since
    constexpr double locatedMs = 250.0;       // the transport catches up with a jump this soon
    constexpr double windFirstMs = 400.0, windMs = 120.0;
    constexpr float panStep = 0.02f;          // a knob's click: 1% of the way across
    constexpr float sendStep = 0.01f;         // a send knob's click, along the fader scale

    //==============================================================================
    // Mackie Control (and its Extender): notes are buttons in (127 down, 0 up) and lights out
    // (127 on, 0 off); pitch bend on channels 1-8 is the faders both ways, 9 the master fader.
    namespace Mackie
    {
        constexpr int record = 0x00, solo = 0x08, mute = 0x10, select = 0x18, knobPush = 0x20;
        constexpr int assignTrack = 0x28, assignSend = 0x29, assignPan = 0x2A, assignPlugin = 0x2B, assignEq = 0x2C, assignInstrument = 0x2D;
        constexpr int bankLeft = 0x2E, bankRight = 0x2F, channelLeft = 0x30, channelRight = 0x31;
        constexpr int flip = 0x32, globalView = 0x33, nameValue = 0x34, smpteBeats = 0x35;
        constexpr int viewMidi = 0x3E, viewInputs = 0x3F, viewAudio = 0x40, viewInstruments = 0x41,
                      viewAux = 0x42, viewBusses = 0x43, viewOutputs = 0x44, viewUser = 0x45;
        constexpr int shift = 0x46, option = 0x47, control = 0x48, alt = 0x49;
        constexpr int read = 0x4A, write = 0x4B, trim = 0x4C, touch = 0x4D, latch = 0x4E, group = 0x4F;
        constexpr int save = 0x50, undo = 0x51, cancel = 0x52, enter = 0x53;
        constexpr int marker = 0x54, nudge = 0x55, cycle = 0x56, drop = 0x57, replace = 0x58, click = 0x59, soloAll = 0x5A;
        constexpr int rewind = 0x5B, forward = 0x5C, stop = 0x5D, play = 0x5E, recordAll = 0x5F;
        constexpr int up = 0x60, down = 0x61, left = 0x62, right = 0x63, zoom = 0x64, scrub = 0x65;
        constexpr int userA = 0x66, userB = 0x67;
        constexpr int faderTouch = 0x68, masterTouch = 0x70;
        constexpr int smpteLight = 0x71, beatsLight = 0x72, rudeSolo = 0x73;

        constexpr int knob = 0x10;            // controllers in: 0x10-0x17, turned (relative)
        constexpr int jog = 0x3C;
        constexpr int ring = 0x30;            // controllers out: 0x30-0x37, the lights round the knobs
        constexpr int digits = 0x40;          // 0x40 the rightmost digit .. 0x49 the leftmost
        constexpr int assignment = 0x4A;      // 0x4A the right letter, 0x4B the left

        constexpr juce::uint8 mackieId = 0x14, extenderId = 0x15;

        // Buttons Sonify does nothing with, or whose lights it has nothing to show on: dark.
        constexpr int unused[] = { assignTrack, assignPlugin, assignEq, assignInstrument, flip, smpteBeats, viewInputs, viewOutputs,
                                   viewUser, shift, option, control, alt, write, trim, touch, latch, group, save, undo, cancel,
                                   enter, marker, nudge, drop, replace, up, down, left, right, userA, userB,
                                   0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D };   // F1-F8
    }

    //==============================================================================
    // HUI: a switch comes as its zone (controller 0x0F) and its port in that zone (controller
    // 0x2F: 0x40 + port down, the port alone up); lights go out the same way on 0x0C and 0x2C.
    // Faders are controller pairs both ways, and HUI wants a ping every second.
    namespace Hui
    {
        constexpr int stripTouch = 0, stripSelect = 1, stripMute = 2, stripSolo = 3, stripAuto = 4,
                      stripKnob = 5, stripInsert = 6, stripRecord = 7;
        constexpr int keysZone = 0x08;        // 0 ctrl, 1 shift, 2 edit mode, 3 undo, 4 alt, 5 option, 6 edit tool, 7 save
        constexpr int bankZone = 0x0A;        // 0 channel left, 1 bank left, 2 channel right, 3 bank right
        constexpr int assignZone = 0x0B;      // 0 output, 1 input, 2 pan, 3 send E .. 7 send A
        constexpr int cursorZone = 0x0D;      // 0 down, 1 left, 2 mode, 3 right, 4 up, 5 scrub, 6 shuttle
        constexpr int transportZone = 0x0E;   // 0 talkback, 1 rewind, 2 fast forward, 3 stop, 4 play, 5 record
        constexpr int transportZone2 = 0x0F;  // 0 return to zero, 1 end, 2 on line, 3 loop, 4 quick punch
        constexpr int timeZone = 0x16;        // lights: 0 timecode, 1 feet, 2 beats, 3 rude solo
        constexpr int autoZone = 0x18;        // 0 trim, 1 latch, 2 read, 3 off, 4 write, 5 touch

        constexpr int zoneIn = 0x0F, portIn = 0x2F, zoneOut = 0x0C, portOut = 0x2C;
        constexpr int faderHigh = 0x00, faderLow = 0x20;    // 0x00-0x07 and 0x20-0x27, both ways
        constexpr int knob = 0x40;                           // in: 0x40-0x47
        constexpr int jog = 0x0D;                            // in
        constexpr int ring = 0x10;                           // out: 0x10-0x17

        constexpr juce::uint8 huiId = 0x05;
    }

    //==============================================================================
    /** Mackie's system exclusive: its maker's id, the device, then what follows. */
    juce::MidiMessage sysex (juce::uint8 device, std::initializer_list<int> head, const juce::String& text = {})
    {
        std::vector<juce::uint8> data { 0x00, 0x00, 0x66, device };

        for (const auto byte : head)
            data.push_back (static_cast<juce::uint8> (byte & 0x7f));

        for (const auto c : text)
            data.push_back (static_cast<juce::uint8> (c & 0x7f));   // plain ASCII already

        return juce::MidiMessage::createSysExMessage (data.data(), static_cast<int> (data.size()));
    }

    /** What the displays can show: plain ASCII, accents taken off, anything else a "?". */
    juce::String plainText (const juce::String& text)
    {
        static const juce::String accented (juce::CharPointer_UTF8 ("\xc3\x80\xc3\x81\xc3\x82\xc3\x83\xc3\x84\xc3\x85\xc3\xa0\xc3\xa1\xc3\xa2\xc3\xa3\xc3\xa4\xc3\xa5"
                                                                     "\xc3\x88\xc3\x89\xc3\x8a\xc3\x8b\xc3\xa8\xc3\xa9\xc3\xaa\xc3\xab"
                                                                     "\xc3\x8c\xc3\x8d\xc3\x8e\xc3\x8f\xc3\xac\xc3\xad\xc3\xae\xc3\xaf"
                                                                     "\xc3\x92\xc3\x93\xc3\x94\xc3\x95\xc3\x96\xc3\xb2\xc3\xb3\xc3\xb4\xc3\xb5\xc3\xb6"
                                                                     "\xc3\x99\xc3\x9a\xc3\x9b\xc3\x9c\xc3\xb9\xc3\xba\xc3\xbb\xc3\xbc"
                                                                     "\xc3\x87\xc3\xa7\xc3\x91\xc3\xb1"));
        static const juce::String plain ("AAAAAAaaaaaaEEEEeeeeIIIIiiiiOOOOOoooooUUUUuuuuCcNn");

        juce::String result;
        result.preallocateBytes (static_cast<size_t> (text.length()) + 1);

        for (const auto c : text)
        {
            if (c >= 0x20 && c < 0x7f)
                result << juce::String::charToString (c);
            else if (const auto index = accented.indexOfChar (c); index >= 0)
                result << juce::String::charToString (plain[index]);
            else
                result << "?";
        }

        return result;
    }

    /** Exactly width characters: cut, or filled out with spaces. */
    juce::String fit (const juce::String& text, int width)
    {
        return plainText (text).substring (0, width).paddedRight (' ', width);
    }

    /** A name in a few characters: "Audio 1" is "Audio1", "Track 12" is "Trac12". */
    juce::String shortName (const juce::String& name, int width)
    {
        auto text = plainText (name).trim();

        if (text.length() <= width)
            return text;

        text = text.removeCharacters (" ");

        if (text.length() <= width)
            return text;

        // Keep a number at its end: it's often what tells two tracks apart.
        int digits = 0;

        while (digits < text.length() && juce::CharacterFunctions::isDigit (text[text.length() - 1 - digits]))
            ++digits;

        digits = juce::jmin (digits, width - 1);
        return text.substring (0, width - digits) + text.substring (text.length() - digits);
    }

    juce::String dbText (float db)
    {
        if (db <= ControlSurfaces::silentDb + 0.05f)
            return "-inf";

        auto rounded = std::round (db * 10.0f) / 10.0f;

        if (std::abs (rounded) < 0.05f)
            rounded = 0.0f;

        return (rounded > 0.0f ? "+" : "") + juce::String (rounded, 1);
    }

    juce::String panText (float pan)
    {
        const auto percent = juce::roundToInt (juce::jlimit (-1.0f, 1.0f, pan) * 100.0f);
        return percent == 0 ? juce::String ("<C>") : (percent < 0 ? "L" : "R") + juce::String (std::abs (percent));
    }

    int toFader (float db)
    {
        return juce::roundToInt (ControlSurfaces::dbToFader (db) * static_cast<float> (fullScale));
    }

    /** A peak as one of the twelve steps of a surface's meter (0: nothing, 12: 0 dB). */
    int meterLevel (float peak)
    {
        if (! (peak > 0.0f))
            return 0;

        const auto db = 20.0f * std::log10 (peak);
        return juce::jlimit (0, 12, juce::roundToInt ((db + 60.0f) / 60.0f * 12.0f));
    }

    /** Mackie's knobs and jog wheel: bit 6 set turns left, the rest says how far. */
    int mackieTicks (int value)    { return (value & 0x40) != 0 ? -(value & 0x3f) : (value & 0x3f); }

    /** HUI's: bit 6 set turns right. */
    int huiTicks (int value)       { return (value & 0x40) != 0 ? (value & 0x3f) : -(value & 0x3f); }

    /** A character on Mackie's seven-segment displays (0x40 adds the dot after it). */
    int sevenSegment (juce::juce_wchar c, bool dot = false)
    {
        auto code = 0x20;   // blank

        if (c >= '0' && c <= '9')        code = static_cast<int> (c);
        else if (c >= 'A' && c <= 'Z')   code = static_cast<int> (c) - 0x40;
        else if (c >= 'a' && c <= 'z')   code = static_cast<int> (c) - 0x60;
        else if (c == '-')               code = 0x2d;

        return code | (dot ? 0x40 : 0);
    }

    /** The song position for the time displays: bars, beats, sixteenths and ticks (0-99) as the
        page shows them, or hours, minutes, seconds and frames. */
    std::array<int, 4> clockGroups (const Transport::State& transport, const TempoMap& map, bool smpte, double framesPerSecond)
    {
        const auto ppq = juce::jmax (0.0, transport.ppq);

        if (smpte)
        {
            const auto seconds = map.secondsAt (ppq);
            const auto whole = static_cast<juce::int64> (std::floor (seconds));
            const auto frames = static_cast<int> (std::floor ((seconds - static_cast<double> (whole)) * framesPerSecond));
            return { static_cast<int> (whole / 3600), static_cast<int> (whole / 60 % 60), static_cast<int> (whole % 60), frames };
        }

        // Bars as the song's time signatures count them.
        const auto at = map.barAt (ppq + 1.0e-9);
        const auto denominator = juce::jmax (1, at.denominator);
        const auto numerator = juce::jmax (1, at.numerator);
        const auto beatLength = at.beat();
        const auto bar = static_cast<double> (at.index);
        const auto inBar = juce::jmax (0.0, ppq + 1.0e-9 - at.start);
        const auto beat = juce::jmin (static_cast<double> (numerator - 1), std::floor (inBar / beatLength));
        const auto perBeat = juce::jmax (1, 16 / denominator);
        const auto sixteenthLength = beatLength / perBeat;
        const auto inBeat = inBar - beat * beatLength;
        const auto sixteenth = juce::jmin (static_cast<double> (perBeat - 1), std::floor (inBeat / sixteenthLength));
        const auto ticks = juce::jmin (99.0, std::floor ((inBeat - sixteenth * sixteenthLength) / sixteenthLength * 100.0));
        return { static_cast<int> (bar) + 1, static_cast<int> (beat) + 1, static_cast<int> (sixteenth) + 1, static_cast<int> (ticks) };
    }

    /** Logic Control's handshake: the answer to the surface's challenge. */
    std::array<int, 4> challengeResponse (const juce::uint8* challenge)
    {
        const int l1 = challenge[0], l2 = challenge[1], l3 = challenge[2], l4 = challenge[3];

        return { 0x7f & (l1 + (l2 ^ 0xa) - l4),
                 0x7f & ((l3 >> (l4 & 31)) ^ (l1 + l4)),
                 0x7f & ((l4 - (l3 << 2)) ^ (l1 | l2)),
                 0x7f & (l2 - l3 + (0xf0 ^ (l4 << 4))) };
    }
}

//==============================================================================
struct ControlSurfaces::Surface
{
    Config config;
    std::unique_ptr<juce::MidiOutput> output;
    bool outputFailed = false;
    juce::String problem;
    int firstStrip = 0;                         // where its strips are among all the surfaces'

    // What it shows now (-1 or empty: not known, so sent again). Index 8 is the master fader.
    std::array<int, 9> fader {};
    std::array<bool, 9> touched {};
    std::array<double, 9> movedAt {};
    std::array<int, 256> light {};              // Mackie: by note; HUI: zone * 8 + port
    std::array<int, 8> ring {};
    std::array<int, 8> meter {};
    std::array<int, 10> clock {};
    juce::String display, panel, assignment;
    int zone = -1;                              // HUI: the zone its switches are in now
    std::array<int, 8> faderHigh {};            // HUI: the upper seven bits of each fader
    double lastPing = -1.0e12, lastMeters = -1.0e12, lastClock = -1.0e12, lastRefresh = 0.0;
    bool meterModeDue = true;                   // Mackie: its meters are turned on again
    bool loopback = false;                      // what Sonify sends it comes back: its input is ignored

    bool isMackie() const noexcept              { return config.protocol == Protocol::mackie; }
    bool isHui() const noexcept                 { return config.protocol == Protocol::hui; }
    juce::uint8 deviceId() const noexcept       { return config.protocol == Protocol::extender ? Mackie::extenderId : Mackie::mackieId; }

    void forget()
    {
        fader.fill (-1);
        light.fill (-1);
        ring.fill (-1);
        meter.fill (-1);
        clock.fill (-1);
        display.clear();
        panel.clear();
        assignment.clear();
        meterModeDue = true;
    }

    juce::String loopbackProblem() const
    {
        return config.name + ": what Sonify sends to " + config.output + " comes straight back (a loopback cable, such as a loopMIDI port, "
               "used both ways), so Sonify doesn't listen to it. Use two ports: one for Receive From, another for Send To.";
    }
};

//==============================================================================
ControlSurfaces::Protocol ControlSurfaces::protocolFromName (const juce::String& name)
{
    if (name == "extender")  return Protocol::extender;
    if (name == "hui")       return Protocol::hui;
    return Protocol::mackie;
}

juce::String ControlSurfaces::protocolName (Protocol protocol)
{
    switch (protocol)
    {
        case Protocol::extender:  return "Mackie Control Extender";
        case Protocol::hui:       return "HUI";
        case Protocol::mackie:    break;
    }

    return "Mackie Control";
}

float ControlSurfaces::faderToDb (float position) noexcept
{
    const auto p = juce::jlimit (0.0f, 1.0f, position);

    for (size_t i = 1; i < faderLaw.size(); ++i)
    {
        if (p <= faderLaw[i].first)
        {
            const auto [p0, d0] = faderLaw[i - 1];
            const auto [p1, d1] = faderLaw[i];
            return d0 + (d1 - d0) * (p - p0) / (p1 - p0);
        }
    }

    return faderLaw.back().second;
}

float ControlSurfaces::dbToFader (float db) noexcept
{
    if (db <= silentDb)
        return 0.0f;   // closed: all the way down

    for (size_t i = 1; i < faderLaw.size(); ++i)
    {
        if (db <= faderLaw[i].second)
        {
            const auto [p0, d0] = faderLaw[i - 1];
            const auto [p1, d1] = faderLaw[i];
            return p0 + (p1 - p0) * (db - d0) / (d1 - d0);
        }
    }

    return 1.0f;
}

//==============================================================================
ControlSurfaces::ControlSurfaces (Host& hostIn, TakeIncoming takeIncomingIn)
    : host (hostIn), takeIncoming (std::move (takeIncomingIn))
{
}

ControlSurfaces::~ControlSurfaces()
{
    stopTimer();

    for (auto& surface : surfaces)
        goDark (*surface);
}

void ControlSurfaces::configure (const std::vector<Config>& list)
{
    // The ports here, by name (a test opens none).
    juce::StringArray inputsHere, outputsHere;
    std::map<juce::String, juce::String> outputIds;

    if (sendForTest == nullptr)
    {
        for (const auto& info : juce::MidiInput::getAvailableDevices())
            inputsHere.add (info.name);

        for (const auto& info : juce::MidiOutput::getAvailableDevices())
        {
            outputsHere.add (info.name);
            outputIds[info.name] = info.identifier;
        }
    }

    const auto layoutOf = [] (const Config& c) { return juce::String (static_cast<int> (c.protocol)) + "|" + c.input + "|" + c.output + "\n"; };
    juce::String before, after;

    for (const auto& surface : surfaces)
        before << layoutOf (surface->config);

    for (const auto& config : list)
        after << layoutOf (config);

    std::vector<std::unique_ptr<Surface>> next;

    for (const auto& config : list)
    {
        std::unique_ptr<Surface> surface;

        // The same surface as before keeps what it shows.
        for (auto& old : surfaces)
        {
            if (old != nullptr && old->config.protocol == config.protocol && old->config.input == config.input && old->config.output == config.output)
            {
                surface = std::move (old);
                break;
            }
        }

        const auto isNew = surface == nullptr;

        if (isNew)
            surface = std::make_unique<Surface>();

        surface->config = config;

        // Its output: opened now if it wasn't (it may have just come back). One that's gone (the
        // surface turned off or unplugged) is let go, so it's opened afresh, and shown everything,
        // when it's back.
        auto greetIt = isNew;

        if (sendForTest == nullptr && surface->output != nullptr && ! outputsHere.contains (config.output))
            surface->output.reset();

        if (sendForTest == nullptr && surface->output == nullptr)
        {
            surface->outputFailed = false;

            if (const auto id = outputIds.find (config.output); config.output.isNotEmpty() && id != outputIds.end())
            {
                surface->output = juce::MidiOutput::openDevice (id->second);
                surface->outputFailed = surface->output == nullptr;
                greetIt = greetIt || surface->output != nullptr;
            }
        }

        // What keeps it from working, said plainly.
        surface->problem.clear();

        if (config.input.isEmpty())
            surface->problem = config.name + ": choose the port it sends from (Receive From).";
        else if (sendForTest == nullptr && ! inputsHere.contains (config.input))
            surface->problem = config.name + ": its port " + config.input + " isn't connected.";
        else if (config.output.isEmpty())
            surface->problem = config.name + ": choose the port it listens to (Send To), so its faders, lights and display follow Sonify.";
        else if (sendForTest == nullptr && ! outputsHere.contains (config.output))
            surface->problem = config.name + ": its port " + config.output + " isn't connected.";
        else if (surface->outputFailed)
            surface->problem = config.name + ": couldn't open " + config.output + " (another app may be using it).";
        else if (surface->loopback)
            surface->problem = surface->loopbackProblem();

        if (greetIt)
            greet (*surface);

        next.push_back (std::move (surface));
    }

    // Surfaces no longer listed go dark.
    for (auto& old : surfaces)
        if (old != nullptr)
            goDark (*old);

    surfaces = std::move (next);

    // They line up in the list's order; a new layout shows everything afresh.
    for (size_t i = 0; i < surfaces.size(); ++i)
    {
        surfaces[i]->firstStrip = static_cast<int> (i) * stripsPerSurface;

        if (before != after)
            surfaces[i]->forget();
    }

    if (surfaces.empty())
    {
        stopTimer();
        winding = 0;
        moved.clear();   // the page follows its own faders again
    }
    else if (sendForTest == nullptr)
    {
        startTimer (timerMs);
    }
}

juce::StringArray ControlSurfaces::getProblems() const
{
    juce::StringArray problems;

    for (const auto& surface : surfaces)
        if (surface->problem.isNotEmpty())
            problems.add (surface->problem);

    return problems;
}

void ControlSurfaces::setMeters (const std::map<int, float>& peaks)
{
    if (surfaces.empty())
        return;

    meters = peaks;
    metersNew = true;
}

bool ControlSurfaces::movedLately (int channelId) const
{
    const auto found = moved.find (channelId);
    return found != moved.end() && now - found->second < movedMs;
}

//==============================================================================
void ControlSurfaces::timerCallback()
{
    update (juce::Time::getMillisecondCounterHiRes());
}

void ControlSurfaces::update (double nowMs)
{
    now = nowMs;
    readSong();

    std::vector<Message> incoming;

    if (takeIncoming != nullptr)
        takeIncoming (incoming);

    for (const auto& [port, message] : incoming)
        receive (port, message, nowMs);

    applyMoves();
    wind();

    if (jogTicks != 0)
        jog (std::exchange (jogTicks, 0));

    if (songChanged)
        readSong();

    for (auto& surface : surfaces)
        feedback (*surface);

    metersNew = false;

    // The page has followed moves made long enough ago.
    for (auto it = moved.begin(); it != moved.end();)
        it = now - it->second > movedMs ? moved.erase (it) : std::next (it);
}

void ControlSurfaces::readSong()
{
    channels = host.getSurfaceChannels (sendKnobs);
    transportNow = host.getSurfaceTransport();
    mainVolume = host.getMainVolume();

    shown.clear();

    for (size_t i = 0; i < channels.size(); ++i)
    {
        const auto kind = channels[i].kind;

        if (view == View::all || (view == View::instruments && kind == Channel::Kind::instrument)
            || (view == View::audio && kind == Channel::Kind::audio) || (view == View::fx && kind == Channel::Kind::fx))
            shown.push_back (static_cast<int> (i));
    }

    bank = juce::jlimit (0, lastBank(), bank);
    songChanged = false;
}

ControlSurfaces::Channel* ControlSurfaces::channelAt (int strip)
{
    const auto index = bank + strip;
    return juce::isPositiveAndBelow (index, static_cast<int> (shown.size())) ? &channels[static_cast<size_t> (shown[static_cast<size_t> (index)])] : nullptr;
}

int ControlSurfaces::totalStrips() const noexcept
{
    return static_cast<int> (surfaces.size()) * stripsPerSurface;
}

int ControlSurfaces::lastBank() const noexcept
{
    return juce::jmax (0, static_cast<int> (shown.size()) - totalStrips());
}

//==============================================================================
// In
void ControlSurfaces::receive (const juce::String& port, const juce::MidiMessage& message, double nowMs)
{
    now = nowMs;

    for (auto& surface : surfaces)
    {
        if (surface->config.input != port)
            continue;

        if (surface->loopback)
            return;   // only Sonify's own messages, and whatever else shares the cable

        // A loopback cable used both ways: Sonify hears what it sent (its displays' text, or its
        // question to a Mackie Control), which no surface sends. Listening on would have its
        // lights come back as button presses.
        if (isOwnMessage (*surface, message))
        {
            surface->loopback = true;
            surface->problem = surface->loopbackProblem();

            if (onProblemsChanged != nullptr)
                onProblemsChanged();

            return;
        }

        // Buttons, knobs and the HUI's faders are on channel 1 (a keyboard's notes on another
        // channel are not presses).
        if ((message.isNoteOnOrOff() || message.isController()) && message.getChannel() != 1)
            return;

        if (surface->isHui())
            receiveHui (*surface, message);
        else
            receiveMackie (*surface, message);

        return;   // a port is one surface's
    }
}

bool ControlSurfaces::isOwnMessage (const Surface& surface, const juce::MidiMessage& message)
{
    if (! message.isSysEx())
        return false;

    const auto* data = message.getSysExData();
    const auto size = message.getSysExDataSize();

    if (size < 5 || data[0] != 0 || data[1] != 0 || data[2] != 0x66)
        return false;

    // HUI: a display's text. Mackie: the question who's there, the handshake's answer, the
    // display's text, the meters turned on.
    if (surface.isHui())
        return data[3] == Hui::huiId && size >= 6 && data[4] == 0x00 && (data[5] == 0x10 || data[5] == 0x11 || data[5] == 0x12);

    return data[3] == surface.deviceId() && (data[4] == 0x00 || data[4] == 0x02 || data[4] == 0x12 || data[4] == 0x20);
}

void ControlSurfaces::receiveMackie (Surface& surface, const juce::MidiMessage& message)
{
    if (message.isNoteOnOrOff())
    {
        mackieButton (surface, message.getNoteNumber(), message.isNoteOn());
        return;
    }

    if (message.isPitchWheel())
    {
        const auto strip = message.getChannel() - 1;   // 0-7 the strips, 8 the master fader

        if (juce::isPositiveAndBelow (strip, stripsPerSurface) || (strip == 8 && surface.isMackie()))
            moveFader (surface, strip, message.getPitchWheelValue());

        return;
    }

    if (message.isController())
    {
        const auto number = message.getControllerNumber();
        const auto value = message.getControllerValue();

        if (number >= Mackie::knob && number < Mackie::knob + stripsPerSurface)
            turnKnob (surface, number - Mackie::knob, mackieTicks (value));
        else if (number == Mackie::jog)
            jogTicks += mackieTicks (value);

        return;
    }

    if (message.isSysEx())
    {
        // The handshake some surfaces want: they ask with their serial number and a challenge,
        // and Sonify answers; once they're satisfied they're shown everything.
        const auto* data = message.getSysExData();
        const auto size = message.getSysExDataSize();

        if (size < 5 || data[0] != 0 || data[1] != 0 || data[2] != 0x66 || data[3] != surface.deviceId())
            return;

        if (data[4] == 0x01 && size >= 16)
        {
            std::vector<juce::uint8> reply { 0x00, 0x00, 0x66, surface.deviceId(), 0x02 };
            reply.insert (reply.end(), data + 5, data + 12);

            for (const auto byte : challengeResponse (data + 12))
                reply.push_back (static_cast<juce::uint8> (byte));

            send (surface, juce::MidiMessage::createSysExMessage (reply.data(), static_cast<int> (reply.size())));
        }
        else if (data[4] == 0x03)
        {
            surface.forget();
        }
    }
}

void ControlSurfaces::mackieButton (Surface& surface, int note, bool down)
{
    if (note >= Mackie::faderTouch && note < Mackie::faderTouch + stripsPerSurface)
    {
        faderTouched (surface, note - Mackie::faderTouch, down);
        return;
    }

    if (note == Mackie::masterTouch)
    {
        faderTouched (surface, 8, down);
        return;
    }

    if (note == Mackie::shift)    { shift = down; return; }
    if (note == Mackie::option)   { option = down; return; }

    if (note == Mackie::rewind || note == Mackie::forward)
    {
        const auto direction = note == Mackie::rewind ? -1 : 1;

        if (down)
            startWinding (direction);
        else if (winding == direction)
            winding = 0;

        return;
    }

    if (! down)
        return;

    // The strips' buttons: record, solo, mute, select, and the knobs pushed.
    if (note < Mackie::assignTrack)
    {
        static constexpr StripButton kinds[] = { StripButton::record, StripButton::solo, StripButton::mute, StripButton::select, StripButton::knob };
        stripButton (surface.firstStrip + note % stripsPerSurface, kinds[note / stripsPerSurface]);
        return;
    }

    switch (note)
    {
        case Mackie::assignPan:         knobsForPan(); break;
        case Mackie::assignSend:        knobsForSend (sendKnobs ? sendIndex + 1 : 0); break;   // again: the next send
        case Mackie::bankLeft:          if (shift) bankTo (false); else bankBy (-totalStrips()); break;
        case Mackie::bankRight:         if (shift) bankTo (true); else bankBy (totalStrips()); break;
        case Mackie::channelLeft:       bankBy (-1); break;
        case Mackie::channelRight:      bankBy (1); break;
        case Mackie::globalView:        showView (View::all); break;
        case Mackie::viewMidi:
        case Mackie::viewInstruments:   showView (View::instruments); break;
        case Mackie::viewAudio:         showView (View::audio); break;
        case Mackie::viewAux:
        case Mackie::viewBusses:        showView (View::fx); break;
        case Mackie::nameValue:         showVolume = ! showVolume; break;
        case Mackie::smpteBeats:        showSmpte = ! showSmpte; break;
        case Mackie::read:              toggleReadMode(); break;
        case Mackie::save:              host.surfaceSave(); break;
        case Mackie::undo:              host.surfaceUndo (! shift); songChanged = true; break;
        case Mackie::cycle:             host.surfaceLoop (! transportNow.loopEnabled); songChanged = true; break;
        case Mackie::click:             host.surfaceMetronome (! transportNow.metronomeEnabled); songChanged = true; break;
        case Mackie::soloAll:           clearAll (true); break;
        case Mackie::stop:              stop(); break;
        case Mackie::play:              host.surfaceTransport ("play"); songChanged = true; break;
        case Mackie::recordAll:         host.surfaceTransport ("record"); songChanged = true; break;
        case Mackie::up:                cursor (0, -1); break;
        case Mackie::down:              cursor (0, 1); break;
        case Mackie::left:              cursor (-1, 0); break;
        case Mackie::right:             cursor (1, 0); break;
        case Mackie::zoom:              zoom = ! zoom; break;
        case Mackie::scrub:             scrub = ! scrub; break;
        default:                        break;
    }
}

void ControlSurfaces::receiveHui (Surface& surface, const juce::MidiMessage& message)
{
    if (! message.isController())
        return;   // the answers to its pings, and the rest

    const auto number = message.getControllerNumber();
    const auto value = message.getControllerValue();

    if (number == Hui::zoneIn)
    {
        surface.zone = value;
    }
    else if (number == Hui::portIn)
    {
        if (surface.zone >= 0)
            huiSwitch (surface, surface.zone, value & 0x0f, (value & 0x40) != 0);
    }
    else if (number == Hui::jog)
    {
        jogTicks += huiTicks (value);
    }
    else if (number >= Hui::faderHigh && number < Hui::faderHigh + stripsPerSurface)
    {
        surface.faderHigh[static_cast<size_t> (number - Hui::faderHigh)] = value;
    }
    else if (number >= Hui::faderLow && number < Hui::faderLow + stripsPerSurface)
    {
        const auto strip = number - Hui::faderLow;
        moveFader (surface, strip, (surface.faderHigh[static_cast<size_t> (strip)] << 7) | value);
    }
    else if (number >= Hui::knob && number < Hui::knob + stripsPerSurface)
    {
        turnKnob (surface, number - Hui::knob, huiTicks (value));
    }
}

void ControlSurfaces::huiSwitch (Surface& surface, int zone, int port, bool down)
{
    // A strip: its fader's touch, and its buttons.
    if (zone < stripsPerSurface)
    {
        if (port == Hui::stripTouch)
        {
            faderTouched (surface, zone, down);
            return;
        }

        if (! down)
            return;

        const auto strip = surface.firstStrip + zone;

        switch (port)
        {
            case Hui::stripSelect:   stripButton (strip, StripButton::select); break;
            case Hui::stripMute:     stripButton (strip, StripButton::mute); break;
            case Hui::stripSolo:     stripButton (strip, StripButton::solo); break;
            case Hui::stripKnob:     stripButton (strip, StripButton::knob); break;
            case Hui::stripRecord:   stripButton (strip, StripButton::record); break;
            default:                 break;
        }

        return;
    }

    if (zone == Hui::keysZone && port == 1)   { shift = down; return; }
    if (zone == Hui::keysZone && port == 5)   { option = down; return; }

    if (zone == Hui::transportZone && (port == 1 || port == 2))
    {
        const auto direction = port == 1 ? -1 : 1;

        if (down)
            startWinding (direction);
        else if (winding == direction)
            winding = 0;

        return;
    }

    if (! down)
        return;

    switch (zone)
    {
        case Hui::keysZone:
            if (port == 3)        { host.surfaceUndo (! shift); songChanged = true; }
            else if (port == 7)   host.surfaceSave();
            break;

        case Hui::bankZone:
            if (port == 0)        bankBy (-1);
            else if (port == 1)   bankBy (-totalStrips());
            else if (port == 2)   bankBy (1);
            else if (port == 3)   bankBy (totalStrips());
            break;

        case Hui::assignZone:
            if (port == 2)        knobsForPan();
            else if (port >= 3)   knobsForSend (7 - port);   // send A is the first
            break;

        case Hui::cursorZone:
            if (port == 0)        cursor (0, 1);
            else if (port == 1)   cursor (-1, 0);
            else if (port == 2)   zoom = ! zoom;
            else if (port == 3)   cursor (1, 0);
            else if (port == 4)   cursor (0, -1);
            else if (port == 5)   scrub = ! scrub;
            break;

        case Hui::transportZone:
            if (port == 3)        stop();
            else if (port == 4)   { host.surfaceTransport ("play"); songChanged = true; }
            else if (port == 5)   { host.surfaceTransport ("record"); songChanged = true; }
            break;

        case Hui::transportZone2:
            if (port == 0)        locate (0.0);
            else if (port == 1)   locate (host.getSurfaceSongEnd());
            else if (port == 3)   { host.surfaceLoop (! transportNow.loopEnabled); songChanged = true; }
            break;

        case Hui::autoZone:
            if (port == 2 || port == 3)
                for (const auto& channel : channels)
                    if (channel.selected)
                    {
                        host.setChannelAutomationRead (channel.id, port == 2);
                        songChanged = true;
                    }
            break;

        default:
            break;
    }
}

void ControlSurfaces::stripButton (int strip, StripButton button)
{
    auto* channel = channelAt (strip);

    if (channel == nullptr)
        return;

    const auto id = channel->id;

    switch (button)
    {
        case StripButton::record:
            if (channel->kind != Channel::Kind::fx)
                host.setChannelRecordArm (id, ! channel->recordArm);
            break;

        case StripButton::solo:
            if (option)
                clearAll (true);   // Option and a solo button: every solo off
            else
                host.setChannelSolo (id, ! channel->solo);
            break;

        case StripButton::mute:
            if (option)
                clearAll (false);
            else
                host.setChannelMute (id, ! channel->mute);
            break;

        case StripButton::select:
            host.selectChannel (id);
            break;

        case StripButton::knob:
            // A knob pushed: the pan back to the middle, or the send on or off.
            if (! sendKnobs)
            {
                host.setChannelPan (id, 0.0f);
                channel->pan = 0.0f;
                moved[id] = now;
            }
            else if (juce::isPositiveAndBelow (sendIndex, static_cast<int> (channel->sends.size())))
            {
                const auto& chosen = channel->sends[static_cast<size_t> (sendIndex)];
                host.setChannelSendOn (id, chosen.id, ! chosen.on);
            }
            break;
    }

    songChanged = true;
}

void ControlSurfaces::faderTouched (Surface& surface, int strip, bool down)
{
    surface.touched[static_cast<size_t> (strip)] = down;

    if (! down)
    {
        // Let go: its last move counts now, and the motor puts it exactly where the level is.
        applyMoves();
        surface.fader[static_cast<size_t> (strip)] = -1;
        surface.movedAt[static_cast<size_t> (strip)] = 0.0;
    }
}

void ControlSurfaces::moveFader (Surface& surface, int strip, int value)
{
    const auto position = juce::jlimit (0, fullScale, value);
    faderMoves[{ &surface, strip }] = position;
    surface.fader[static_cast<size_t> (strip)] = position;   // it shows that already
    surface.movedAt[static_cast<size_t> (strip)] = now;
}

void ControlSurfaces::turnKnob (Surface& surface, int strip, int ticks)
{
    knobTurns[{ &surface, strip }] += ticks;
}

void ControlSurfaces::applyMoves()
{
    // The last place each fader went this update, and how far each knob turned.
    for (const auto& [key, value] : faderMoves)
    {
        const auto db = juce::jmax (silentDb, faderToDb (static_cast<float> (value) / static_cast<float> (fullScale)));

        if (key.second == 8)
        {
            host.setChannelVolume (mainChannel, db);
            mainVolume = db;
            moved[mainChannel] = now;
        }
        else if (auto* channel = channelAt (key.first->firstStrip + key.second))
        {
            host.setChannelVolume (channel->id, db);
            channel->volumeDb = db;
            moved[channel->id] = now;
        }
    }

    for (const auto& [key, ticks] : knobTurns)
    {
        auto* channel = channelAt (key.first->firstStrip + key.second);

        if (channel == nullptr || ticks == 0)
            continue;

        if (! sendKnobs)
        {
            channel->pan = juce::jlimit (-1.0f, 1.0f, channel->pan + static_cast<float> (ticks) * panStep);
            host.setChannelPan (channel->id, channel->pan);
            moved[channel->id] = now;
        }
        else if (juce::isPositiveAndBelow (sendIndex, static_cast<int> (channel->sends.size())))
        {
            auto& chosen = channel->sends[static_cast<size_t> (sendIndex)];
            const auto along = juce::jlimit (0.0f, 1.0f, dbToFader (chosen.levelDb) + static_cast<float> (ticks) * sendStep);
            chosen.levelDb = juce::jmax (silentDb, faderToDb (along));
            host.setChannelSendLevel (channel->id, chosen.id, chosen.levelDb);
        }
    }

    if (! faderMoves.empty() || ! knobTurns.empty())
        songChanged = true;

    faderMoves.clear();
    knobTurns.clear();
}

//==============================================================================
// Doing
void ControlSurfaces::bankBy (int strips)
{
    bank = juce::jlimit (0, lastBank(), bank + strips);
}

void ControlSurfaces::bankTo (bool end)
{
    bank = end ? lastBank() : 0;
}

void ControlSurfaces::showView (View newView)
{
    view = newView;
    bank = 0;
    songChanged = true;
}

void ControlSurfaces::knobsForPan()
{
    sendKnobs = false;
    songChanged = true;
}

void ControlSurfaces::knobsForSend (int index)
{
    // The sends read (the knobs may have been on pan): as many as the channel with the most,
    // and past the last, back to the first.
    sendKnobs = true;
    readSong();

    size_t most = 0;

    for (const auto& channel : channels)
        most = juce::jmax (most, channel.sends.size());

    sendIndex = index >= juce::jmax (1, static_cast<int> (most)) ? 0 : juce::jmax (0, index);
    songChanged = true;
}

void ControlSurfaces::startWinding (int direction)
{
    // With Shift: to the start or the end at once.
    if (shift)
    {
        locate (direction < 0 ? 0.0 : host.getSurfaceSongEnd());
        winding = 0;
        return;
    }

    winding = direction;
    stepBars (direction);
    windAgain = now + windFirstMs;
}

void ControlSurfaces::wind()
{
    if (winding != 0 && now >= windAgain)
    {
        stepBars (winding);
        windAgain = now + windMs;
    }
}

double ControlSurfaces::position() const
{
    // Just after a jump the transport may not be there yet: from where it's going.
    return now - locatedAt < locatedMs ? locatedTo : juce::jmax (0.0, transportNow.ppq);
}

void ControlSurfaces::locate (double ppq)
{
    locatedTo = juce::jmax (0.0, ppq);
    locatedAt = now;
    host.surfaceLocate (locatedTo);
    songChanged = true;
}

void ControlSurfaces::stepBars (int direction)
{
    // To the start of the bar (or the one before, from its start), or of the next one.
    const auto& map = host.getSurfaceTempoMap();
    const auto from = position();
    const auto bar = map.barAt (from + 1.0e-6);

    if (direction < 0)
        locate (from - bar.start > 0.01 * bar.length() ? bar.start : map.ppqOfBar (bar.index - 1));
    else
        locate (bar.start + bar.length());
}

void ControlSurfaces::stop()
{
    // Stop; stopped already, back to the start, as Studio One's.
    if (transportNow.playing || transportNow.recording)
        host.surfaceTransport ("stop");
    else
        locate (0.0);

    songChanged = true;
}

void ControlSurfaces::cursor (int across, int upDown)
{
    // With Zoom on, left and right zoom out and in; otherwise they go a bar back or on, and
    // up and down select the track before or after.
    if (zoom)
    {
        if (across != 0)
            host.surfaceCommand (across < 0 ? "zoomOut" : "zoomIn");

        return;
    }

    if (across != 0)
        stepBars (across);

    if (upDown != 0)
        selectNext (upDown);
}

void ControlSurfaces::selectNext (int direction)
{
    // Among the tracks: FX channels aren't selected.
    std::vector<int> ids;
    int at = -1;

    for (const auto& channel : channels)
    {
        if (channel.kind == Channel::Kind::fx)
            continue;

        if (channel.selected)
            at = static_cast<int> (ids.size());

        ids.push_back (channel.id);
    }

    if (ids.empty())
        return;

    const auto next = at < 0 ? 0 : juce::jlimit (0, static_cast<int> (ids.size()) - 1, at + direction);
    host.selectChannel (ids[static_cast<size_t> (next)]);
    songChanged = true;
}

void ControlSurfaces::toggleReadMode()
{
    for (const auto& channel : channels)
    {
        if (channel.selected)
        {
            host.setChannelAutomationRead (channel.id, ! channel.automationRead);
            songChanged = true;
        }
    }
}

void ControlSurfaces::clearAll (bool solos)
{
    for (const auto& channel : channels)
    {
        if (solos && channel.solo)
            host.setChannelSolo (channel.id, false);
        else if (! solos && channel.mute)
            host.setChannelMute (channel.id, false);
    }

    songChanged = true;
}

void ControlSurfaces::jog (int ticks)
{
    // With Zoom on, the wheel zooms; otherwise it moves the song position a sixteenth a click
    // (a 64th with Scrub on).
    if (zoom)
    {
        host.surfaceCommand (ticks > 0 ? "zoomIn" : "zoomOut");
        return;
    }

    const auto beat = host.getSurfaceTempoMap().barAt (position()).beat();
    locate (position() + ticks * (scrub ? beat / 16.0 : beat / 4.0));
}

//==============================================================================
// Out
void ControlSurfaces::feedback (Surface& surface)
{
    // Now and then everything again, for a surface switched on (or plugged in) since.
    if (now - surface.lastRefresh > refreshMs)
    {
        surface.forget();
        surface.lastRefresh = now;
    }

    if (surface.isHui())
        feedbackHui (surface);
    else
        feedbackMackie (surface);

    if (metersNew && now - surface.lastMeters >= meterMs)
    {
        sendMeters (surface);
        surface.lastMeters = now;
    }
}

int ControlSurfaces::ringFor (const Channel* channel) const
{
    if (channel == nullptr)
        return 0;

    // The pan as one light, 6 the middle; a send's level as lights filling from the left.
    if (! sendKnobs)
        return 1 + juce::roundToInt ((juce::jlimit (-1.0f, 1.0f, channel->pan) + 1.0f) * 5.0f);

    if (! juce::isPositiveAndBelow (sendIndex, static_cast<int> (channel->sends.size())))
        return 0;

    const auto& chosen = channel->sends[static_cast<size_t> (sendIndex)];
    return chosen.on ? 0x20 | juce::roundToInt (dbToFader (chosen.levelDb) * 11.0f) : 0;
}

juce::String ControlSurfaces::valueText (const Channel& channel) const
{
    if (showVolume)
        return dbText (channel.volumeDb);

    if (! sendKnobs)
        return panText (channel.pan);

    if (! juce::isPositiveAndBelow (sendIndex, static_cast<int> (channel.sends.size())))
        return "--";

    const auto& chosen = channel.sends[static_cast<size_t> (sendIndex)];
    return chosen.on ? dbText (chosen.levelDb) : juce::String ("off");
}

void ControlSurfaces::feedbackMackie (Surface& surface)
{
    // Its meters on: the signal lights and peak hold, not over the display.
    if (std::exchange (surface.meterModeDue, false))
        for (int strip = 0; strip < stripsPerSurface; ++strip)
            send (surface, sysex (surface.deviceId(), { 0x20, strip, 0x03 }));

    for (int strip = 0; strip < stripsPerSurface; ++strip)
    {
        const auto* channel = channelAt (surface.firstStrip + strip);
        const auto s = static_cast<size_t> (strip);

        // The motor fader, unless a hand is on it (or just was).
        const auto target = channel != nullptr ? toFader (channel->volumeDb) : 0;

        if (! surface.touched[s] && now - surface.movedAt[s] > settleMs
            && (surface.fader[s] < 0 || std::abs (target - surface.fader[s]) > faderSlack))
        {
            send (surface, juce::MidiMessage::pitchWheel (strip + 1, target));
            surface.fader[s] = target;
        }

        lightMackie (surface, Mackie::record + strip, channel != nullptr && channel->kind != Channel::Kind::fx && channel->recordArm);
        lightMackie (surface, Mackie::solo + strip, channel != nullptr && channel->solo);
        lightMackie (surface, Mackie::mute + strip, channel != nullptr && channel->mute);
        lightMackie (surface, Mackie::select + strip, channel != nullptr && channel->selected);
        lightMackie (surface, Mackie::knobPush + strip, false);

        if (const auto ring = ringFor (channel); surface.ring[s] != ring)
        {
            send (surface, juce::MidiMessage::controllerEvent (1, Mackie::ring + strip, ring));
            surface.ring[s] = ring;
        }
    }

    showDisplayMackie (surface);

    if (! surface.isMackie())
        return;   // an Extender has strips only

    // The master fader: Main's.
    const auto master = toFader (mainVolume);

    if (! surface.touched[8] && now - surface.movedAt[8] > settleMs
        && (surface.fader[8] < 0 || std::abs (master - surface.fader[8]) > faderSlack))
    {
        send (surface, juce::MidiMessage::pitchWheel (9, master));
        surface.fader[8] = master;
    }

    const auto anySolo = std::any_of (channels.begin(), channels.end(), [] (const Channel& c) { return c.solo; });
    const auto selected = std::find_if (channels.begin(), channels.end(), [] (const Channel& c) { return c.selected; });
    const auto& transport = transportNow;

    lightMackie (surface, Mackie::play, transport.playing);
    lightMackie (surface, Mackie::stop, ! transport.playing);
    lightMackie (surface, Mackie::recordAll, transport.recording);
    lightMackie (surface, Mackie::rewind, winding < 0);
    lightMackie (surface, Mackie::forward, winding > 0);
    lightMackie (surface, Mackie::cycle, transport.loopEnabled);
    lightMackie (surface, Mackie::click, transport.metronomeEnabled);
    lightMackie (surface, Mackie::soloAll, anySolo);
    lightMackie (surface, Mackie::rudeSolo, anySolo);
    lightMackie (surface, Mackie::assignPan, ! sendKnobs);
    lightMackie (surface, Mackie::assignSend, sendKnobs);
    lightMackie (surface, Mackie::globalView, view == View::all);
    lightMackie (surface, Mackie::viewMidi, view == View::instruments);
    lightMackie (surface, Mackie::viewInstruments, view == View::instruments);
    lightMackie (surface, Mackie::viewAudio, view == View::audio);
    lightMackie (surface, Mackie::viewAux, view == View::fx);
    lightMackie (surface, Mackie::viewBusses, view == View::fx);
    lightMackie (surface, Mackie::nameValue, showVolume);
    lightMackie (surface, Mackie::smpteLight, showSmpte);
    lightMackie (surface, Mackie::beatsLight, ! showSmpte);
    lightMackie (surface, Mackie::zoom, zoom);
    lightMackie (surface, Mackie::scrub, scrub);
    lightMackie (surface, Mackie::read, selected != channels.end() && selected->automationRead);

    for (const auto note : Mackie::unused)
        lightMackie (surface, note, false);

    // The two letters over the knobs say what they do: PN the pan, S1 the first send.
    const auto letters = sendKnobs ? "S" + juce::String (juce::jmin (9, sendIndex + 1)) : juce::String ("PN");

    if (surface.assignment != letters)
    {
        send (surface, juce::MidiMessage::controllerEvent (1, Mackie::assignment + 1, sevenSegment (letters[0])));
        send (surface, juce::MidiMessage::controllerEvent (1, Mackie::assignment, sevenSegment (letters[1])));
        surface.assignment = letters;
    }

    if (now - surface.lastClock >= clockMs)
    {
        showTimeMackie (surface);
        surface.lastClock = now;
    }
}

void ControlSurfaces::showDisplayMackie (Surface& surface)
{
    // Each strip's seven characters: its channel's name above, and the knob's value (or the
    // volume, with NAME/VALUE) below.
    juce::String top, bottom;

    for (int strip = 0; strip < stripsPerSurface; ++strip)
    {
        const auto* channel = channelAt (surface.firstStrip + strip);
        top << fit (channel != nullptr ? shortName (channel->name, 6) : juce::String(), 6) << " ";
        bottom << fit (channel != nullptr ? valueText (*channel) : juce::String(), 6) << " ";
    }

    const auto text = top + bottom;   // 112 characters

    if (text == surface.display)
        return;

    // From the first change to the last.
    int first = 0, last = text.length() - 1;

    if (surface.display.length() == text.length())
    {
        while (first < text.length() && text[first] == surface.display[first])
            ++first;

        while (last > first && text[last] == surface.display[last])
            --last;
    }

    send (surface, sysex (surface.deviceId(), { 0x12, first }, text.substring (first, last + 1)));
    surface.display = text;
}

void ControlSurfaces::showTimeMackie (Surface& surface)
{
    // Ten digits: bars, beats, sixteenths and ticks; or hours, minutes, seconds and frames.
    const auto groups = clockGroups (transportNow, host.getSurfaceTempoMap(), showSmpte, host.getSurfaceFrameRate());
    const auto text = juce::String (groups[0] % 1000).paddedLeft (' ', 3)
                    + juce::String (groups[1] % 100).paddedLeft ('0', 2)
                    + juce::String (groups[2] % 100).paddedLeft ('0', 2)
                    + juce::String (groups[3] % 1000).paddedLeft ('0', 3);

    for (int i = 0; i < 10; ++i)
    {
        const auto code = sevenSegment (text[i], i == 2 || i == 4 || i == 6);   // a dot after each group

        if (surface.clock[static_cast<size_t> (i)] != code)
        {
            surface.clock[static_cast<size_t> (i)] = code;
            send (surface, juce::MidiMessage::controllerEvent (1, Mackie::digits + 9 - i, code));
        }
    }
}

void ControlSurfaces::feedbackHui (Surface& surface)
{
    // HUI goes offline unless it hears from the host every second.
    if (now - surface.lastPing >= pingMs)
    {
        send (surface, juce::MidiMessage::noteOn (1, 0, static_cast<juce::uint8> (0)));
        surface.lastPing = now;
    }

    for (int strip = 0; strip < stripsPerSurface; ++strip)
    {
        const auto* channel = channelAt (surface.firstStrip + strip);
        const auto s = static_cast<size_t> (strip);
        const auto target = channel != nullptr ? toFader (channel->volumeDb) : 0;

        if (! surface.touched[s] && now - surface.movedAt[s] > settleMs
            && (surface.fader[s] < 0 || std::abs (target - surface.fader[s]) > faderSlack))
        {
            send (surface, juce::MidiMessage::controllerEvent (1, Hui::faderHigh + strip, target >> 7));
            send (surface, juce::MidiMessage::controllerEvent (1, Hui::faderLow + strip, target & 0x7f));
            surface.fader[s] = target;
        }

        lightHui (surface, strip, Hui::stripSelect, channel != nullptr && channel->selected);
        lightHui (surface, strip, Hui::stripMute, channel != nullptr && channel->mute);
        lightHui (surface, strip, Hui::stripSolo, channel != nullptr && channel->solo);
        lightHui (surface, strip, Hui::stripRecord, channel != nullptr && channel->kind != Channel::Kind::fx && channel->recordArm);
        lightHui (surface, strip, Hui::stripAuto, false);
        lightHui (surface, strip, Hui::stripInsert, false);

        if (const auto ring = ringFor (channel); surface.ring[s] != ring)
        {
            send (surface, juce::MidiMessage::controllerEvent (1, Hui::ring + strip, ring));
            surface.ring[s] = ring;
        }
    }

    showHuiText (surface);

    const auto anySolo = std::any_of (channels.begin(), channels.end(), [] (const Channel& c) { return c.solo; });
    const auto selected = std::find_if (channels.begin(), channels.end(), [] (const Channel& c) { return c.selected; });
    const auto& transport = transportNow;

    lightHui (surface, Hui::transportZone, 1, winding < 0);
    lightHui (surface, Hui::transportZone, 2, winding > 0);
    lightHui (surface, Hui::transportZone, 3, ! transport.playing);
    lightHui (surface, Hui::transportZone, 4, transport.playing);
    lightHui (surface, Hui::transportZone, 5, transport.recording);
    lightHui (surface, Hui::transportZone2, 3, transport.loopEnabled);
    lightHui (surface, Hui::assignZone, 2, ! sendKnobs);

    for (int index = 0; index < 5; ++index)
        lightHui (surface, Hui::assignZone, 7 - index, sendKnobs && sendIndex == index);

    lightHui (surface, Hui::timeZone, 0, showSmpte);
    lightHui (surface, Hui::timeZone, 2, ! showSmpte);
    lightHui (surface, Hui::timeZone, 3, anySolo);
    lightHui (surface, Hui::autoZone, 2, selected != channels.end() && selected->automationRead);
    lightHui (surface, Hui::autoZone, 3, selected != channels.end() && ! selected->automationRead);
    lightHui (surface, Hui::cursorZone, 2, zoom);
    lightHui (surface, Hui::cursorZone, 5, scrub);

    if (now - surface.lastClock >= clockMs * 2.0)
    {
        showTimeHui (surface);
        surface.lastClock = now;
    }
}

void ControlSurfaces::showHuiText (Surface& surface)
{
    // Four letters over each strip.
    juce::String names;

    for (int strip = 0; strip < stripsPerSurface; ++strip)
    {
        const auto* channel = channelAt (surface.firstStrip + strip);
        names << fit (channel != nullptr ? shortName (channel->name, 4) : juce::String(), 4);
    }

    for (int strip = 0; strip < stripsPerSurface; ++strip)
    {
        const auto part = names.substring (strip * 4, strip * 4 + 4);

        if (surface.display.length() != names.length() || surface.display.substring (strip * 4, strip * 4 + 4) != part)
            send (surface, sysex (Hui::huiId, { 0x00, 0x10, strip }, part));
    }

    surface.display = names;

    // Its big display: the selected track, what the knobs do, and the channels on the strips.
    const auto selected = std::find_if (channels.begin(), channels.end(), [] (const Channel& c) { return c.selected; });
    const auto first = bank + 1;
    const auto last = juce::jmin (static_cast<int> (shown.size()), bank + totalStrips());
    const auto line1 = "Sonify  " + (selected != channels.end() ? selected->name : juce::String());
    const auto line2 = (sendKnobs ? "Knobs: Send " + juce::String (sendIndex + 1) : juce::String ("Knobs: Pan"))
                     + "   " + (shown.empty() ? juce::String ("No channels") : "Channels " + juce::String (first) + "-" + juce::String (last));
    const auto panel = fit (line1, 40) + fit (line2, 40);

    for (int zone = 0; zone < 8; ++zone)
    {
        const auto part = panel.substring (zone * 10, zone * 10 + 10);

        if (surface.panel.length() != panel.length() || surface.panel.substring (zone * 10, zone * 10 + 10) != part)
            send (surface, sysex (Hui::huiId, { 0x00, 0x12, zone }, part));
    }

    surface.panel = panel;
}

void ControlSurfaces::showTimeHui (Surface& surface)
{
    // Eight digits: bars (3), beats, sixteenths and tens of ticks; or two each of hours,
    // minutes, seconds and frames. Sent right to left, 0x10 adding the dot after a digit.
    const auto groups = clockGroups (transportNow, host.getSurfaceTempoMap(), showSmpte, host.getSurfaceFrameRate());
    const auto text = showSmpte ? juce::String (groups[0] % 100).paddedLeft ('0', 2) + juce::String (groups[1]).paddedLeft ('0', 2)
                                  + juce::String (groups[2]).paddedLeft ('0', 2) + juce::String (groups[3] % 100).paddedLeft ('0', 2)
                                : juce::String (groups[0] % 1000).paddedLeft ('0', 3) + juce::String (groups[1] % 100).paddedLeft ('0', 2)
                                  + juce::String (groups[2] % 100).paddedLeft ('0', 2) + juce::String (groups[3] / 10);
    const std::array<int, 3> dots = showSmpte ? std::array<int, 3> { 1, 3, 5 } : std::array<int, 3> { 2, 4, 6 };

    std::vector<juce::uint8> data { 0x00, 0x00, 0x66, Hui::huiId, 0x00, 0x11 };
    bool changed = false;

    for (int i = 7; i >= 0; --i)
    {
        const auto dot = std::find (dots.begin(), dots.end(), i) != dots.end();
        const auto digit = juce::jlimit (0, 9, static_cast<int> (text[i] - '0')) | (dot ? 0x10 : 0);
        data.push_back (static_cast<juce::uint8> (digit));
        changed = changed || surface.clock[static_cast<size_t> (i)] != digit;
        surface.clock[static_cast<size_t> (i)] = digit;
    }

    if (changed)
        send (surface, juce::MidiMessage::createSysExMessage (data.data(), static_cast<int> (data.size())));
}

void ControlSurfaces::sendMeters (Surface& surface)
{
    for (int strip = 0; strip < stripsPerSurface; ++strip)
    {
        const auto* channel = channelAt (surface.firstStrip + strip);
        const auto found = channel != nullptr ? meters.find (channel->id) : meters.end();
        const auto level = found != meters.end() ? meterLevel (found->second) : 0;
        auto& lit = surface.meter[static_cast<size_t> (strip)];

        if (level == 0 && lit == 0)
            continue;   // dark already

        lit = level;

        if (surface.isHui())
        {
            for (int side = 0; side < 2; ++side)
                send (surface, juce::MidiMessage::aftertouchChange (1, strip, (side << 4) | level));
        }
        else
        {
            send (surface, juce::MidiMessage::channelPressureChange (1, (strip << 4) | level));
        }
    }
}

void ControlSurfaces::lightMackie (Surface& surface, int note, bool on)
{
    const auto value = on ? 127 : 0;
    auto& lit = surface.light[static_cast<size_t> (note & 0x7f)];

    if (lit != value)
    {
        lit = value;
        send (surface, juce::MidiMessage::noteOn (1, note, static_cast<juce::uint8> (value)));
    }
}

void ControlSurfaces::lightHui (Surface& surface, int zone, int port, bool on)
{
    const auto key = static_cast<size_t> (zone * 8 + port);
    const auto value = on ? 1 : 0;

    if (key >= surface.light.size() || surface.light[key] == value)
        return;

    surface.light[key] = value;
    send (surface, juce::MidiMessage::controllerEvent (1, Hui::zoneOut, zone));
    send (surface, juce::MidiMessage::controllerEvent (1, Hui::portOut, (on ? 0x40 : 0x00) | port));
}

void ControlSurfaces::send (Surface& surface, const juce::MidiMessage& message)
{
    if (sendForTest != nullptr)
        sendForTest (surface.config.output, message);
    else if (surface.output != nullptr)
        surface.output->sendMessageNow (message);
}

void ControlSurfaces::greet (Surface& surface)
{
    surface.forget();
    surface.lastPing = -1.0e12;
    surface.lastRefresh = now;

    // Mackie Control is asked who's there: one that wants the handshake answers (receiveMackie).
    if (! surface.isHui())
        send (surface, sysex (surface.deviceId(), { 0x00 }));
}

void ControlSurfaces::goDark (Surface& surface)
{
    // Sonify has gone: a blank display, the lights out, the faders down.
    if (surface.isHui())
    {
        for (int strip = 0; strip < stripsPerSurface; ++strip)
        {
            send (surface, sysex (Hui::huiId, { 0x00, 0x10, strip }, "    "));
            send (surface, juce::MidiMessage::controllerEvent (1, Hui::faderHigh + strip, 0));
            send (surface, juce::MidiMessage::controllerEvent (1, Hui::faderLow + strip, 0));
            send (surface, juce::MidiMessage::controllerEvent (1, Hui::ring + strip, 0));
        }

        for (int zone = 0; zone < 8; ++zone)
            send (surface, sysex (Hui::huiId, { 0x00, 0x12, zone }, juce::String::repeatedString (" ", 10)));

        for (size_t key = 0; key < surface.light.size(); ++key)
            if (surface.light[key] > 0)
                lightHui (surface, static_cast<int> (key / 8), static_cast<int> (key % 8), false);

        return;
    }

    send (surface, sysex (surface.deviceId(), { 0x12, 0 }, juce::String::repeatedString (" ", 112)));

    for (int strip = 0; strip < stripsPerSurface; ++strip)
    {
        send (surface, juce::MidiMessage::pitchWheel (strip + 1, 0));
        send (surface, juce::MidiMessage::controllerEvent (1, Mackie::ring + strip, 0));
        send (surface, juce::MidiMessage::channelPressureChange (1, strip << 4));
    }

    if (surface.isMackie())
    {
        send (surface, juce::MidiMessage::pitchWheel (9, 0));

        for (int digit = 0; digit < 12; ++digit)   // the time, and the two letters
            send (surface, juce::MidiMessage::controllerEvent (1, Mackie::digits + digit, 0x20));
    }

    for (int note = 0; note < 128; ++note)
        if (surface.light[static_cast<size_t> (note)] > 0)
            lightMackie (surface, note, false);
}
