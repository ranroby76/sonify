// D:\workspace\Fanan_web_hub\src\PadGrid.cpp

#include "PadGrid.h"

#include <algorithm>
#include <cmath>

namespace
{
    //==============================================================================
    // Novation Launchpad (Mini MK3, X, Pro MK3), Programmer mode: pads are notes 11 .. 88 (a
    // row's tens, a column's units: 11 the lower left), the right column CC 19 .. 89 (89 at the
    // top), the top row CC 91 .. 98. A light: its note or CC on channel 1 (lit), 2 (flashing) or 3
    // (pulsing), the value a colour of its palette.
    namespace Launchpad
    {
        constexpr int off = 0, white = 3, red = 5, orange = 9, yellow = 13, green = 21, blue = 45, pink = 53;
        constexpr int lit = 1, flashing = 2, pulsing = 3;

        int pad (int row, int column)        { return 10 * (8 - row) + column + 1; }   // row 0: the top
        int side (int row)                   { return 10 * (8 - row) + 9; }
        int top (int column)                 { return 91 + column; }
        constexpr int stopAll = 19;          // the right column's bottom button (Ableton's Stop Solo Mute)

        /** Programmer mode (on) or Live mode, for each of them (each ignores the others'). */
        std::vector<juce::MidiMessage> mode (bool programmer)
        {
            std::vector<juce::MidiMessage> list;

            for (const juce::uint8 device : { 0x0d, 0x0c, 0x0e })   // Mini MK3, X, Pro MK3
            {
                const juce::uint8 data[] = { 0x00, 0x20, 0x29, 0x02, device, 0x0e, static_cast<juce::uint8> (programmer ? 1 : 0) };
                list.push_back (juce::MidiMessage::createSysExMessage (data, static_cast<int> (sizeof (data))));
            }

            return list;
        }
    }

    //==============================================================================
    // Akai APC mini: pads notes 0 .. 63 (0 the lower left, rows of eight), the buttons under them
    // notes 64 .. 71, the side buttons 82 .. 89 (82 at the top), Shift 98. A pad's light: its note's
    // velocity, 1 green, 2 green flashing, 3 red, 5 yellow; a button's: 1 lit, 2 flashing.
    namespace ApcMini
    {
        constexpr int green = 1, greenFlashing = 2, yellow = 5;
        constexpr int track0 = 64, side0 = 82, shift = 98;
    }

    // Akai APC mini MK2: pads notes 0 .. 63 as the first one's, lit in colour (the velocity a
    // colour of its palette, the channel how: 7 full brightness, 11 pulsing, 16 flashing); the
    // buttons under them notes 100 .. 107, the side buttons 112 .. 119 (112 at the top), Shift
    // 122, their lights 1 lit, 2 flashing (channel 1).
    namespace ApcMini2
    {
        constexpr int red = 72, orange = 61, yellow = 74, green = 87, cyan = 78, blue = 41, purple = 67, pink = 95;
        constexpr int solid = 7, pulsing = 11, flashing = 16;
        constexpr int track0 = 100, side0 = 112, shift = 122;
    }

    int apcPad (int row, int column)         { return (7 - row) * 8 + column; }

    /** A block's colour as the nearest one the controller has (by hue; grey: white). */
    int paletteOf (juce::Colour colour, PadGrids::Model model)
    {
        const auto launchpad = model == PadGrids::Model::launchpad;

        if (colour.getSaturation() < 0.18f)
            return launchpad ? Launchpad::white : ApcMini2::yellow;

        struct Hue { float degrees; int launchpad, apc; };
        static const Hue hues[] = {
            { 0.0f,   Launchpad::red,    ApcMini2::red },    { 30.0f,  Launchpad::orange, ApcMini2::orange },
            { 55.0f,  Launchpad::yellow, ApcMini2::yellow }, { 125.0f, Launchpad::green,  ApcMini2::green },
            { 180.0f, Launchpad::green,  ApcMini2::cyan },   { 220.0f, Launchpad::blue,   ApcMini2::blue },
            { 270.0f, Launchpad::blue,   ApcMini2::purple }, { 320.0f, Launchpad::pink,   ApcMini2::pink },
            { 360.0f, Launchpad::red,    ApcMini2::red } };

        const auto hue = colour.getHue() * 360.0f;
        const Hue* best = &hues[0];

        for (const auto& h : hues)
            if (std::abs (h.degrees - hue) < std::abs (best->degrees - hue))
                best = &h;

        return launchpad ? best->launchpad : best->apc;
    }

    // A light's key: a note or a controller, and its number; its value: the channel and the value.
    int noteKey (int number)                 { return number; }
    int controllerKey (int number)           { return 0x10000 | number; }
    int value (int channel, int velocity)    { return (channel << 8) | velocity; }
}

//==============================================================================
bool PadGrids::isPadProtocol (const juce::String& protocol)
{
    return protocol == "launchpad" || protocol == "apcmini" || protocol == "apcmini2";
}

PadGrids::Model PadGrids::modelFromName (const juce::String& protocol)
{
    return protocol == "apcmini" ? Model::apcMini : protocol == "apcmini2" ? Model::apcMini2 : Model::launchpad;
}

PadGrids::~PadGrids()
{
    for (auto& grid : grids)
        goDark (*grid);
}

void PadGrids::configure (const std::vector<Config>& list)
{
    std::vector<std::unique_ptr<Grid>> next;

    for (const auto& config : list)
    {
        // The same one as before keeps its port and what it shows.
        const auto same = std::find_if (grids.begin(), grids.end(), [&config] (const std::unique_ptr<Grid>& g)
        {
            return g != nullptr && g->config.model == config.model && g->config.input == config.input && g->config.output == config.output;
        });

        if (same != grids.end())
        {
            (*same)->config.name = config.name;
            next.push_back (std::move (*same));
            continue;
        }

        auto grid = std::make_unique<Grid>();
        grid->config = config;

        if (config.output.isNotEmpty())
            for (const auto& device : juce::MidiOutput::getAvailableDevices())
                if (device.name == config.output)
                {
                    grid->output = juce::MidiOutput::openDevice (device.identifier);
                    break;
                }

        greet (*grid);
        next.push_back (std::move (grid));
    }

    // The ones that went: dark.
    for (auto& old : grids)
        if (old != nullptr)
            goDark (*old);

    grids = std::move (next);
}

bool PadGrids::ownsPort (const juce::String& port) const
{
    return port.isNotEmpty() && std::any_of (grids.begin(), grids.end(), [&port] (const std::unique_ptr<Grid>& g) { return g->config.input == port; });
}

void PadGrids::greet (Grid& grid)
{
    grid.sent.clear();

    if (grid.output == nullptr)
        return;

    if (grid.config.model == Model::launchpad)
        for (const auto& message : Launchpad::mode (true))
            grid.output->sendMessageNow (message);
}

void PadGrids::goDark (Grid& grid)
{
    showOn (grid, View {});   // nothing lit

    if (grid.output != nullptr && grid.config.model == Model::launchpad)
        for (const auto& message : Launchpad::mode (false))
            grid.output->sendMessageNow (message);
}

void PadGrids::light (Grid& grid, int key, int wanted)
{
    if (grid.output == nullptr)
        return;

    const auto found = grid.sent.find (key);

    if (found != grid.sent.end() && found->second == wanted)
        return;

    grid.sent[key] = wanted;
    const auto channel = juce::jlimit (1, 16, wanted >> 8);
    const auto velocity = static_cast<juce::uint8> (wanted & 0x7f);
    const auto number = key & 0x7f;

    grid.output->sendMessageNow ((key & 0x10000) != 0 ? juce::MidiMessage::controllerEvent (channel, number, velocity)
                                                       : juce::MidiMessage::noteOn (channel, number, velocity));
}

//==============================================================================
void PadGrids::show (const View& view)
{
    for (auto& grid : grids)
        if (grid != nullptr)
            showOn (*grid, view);
}

void PadGrids::showOn (Grid& grid, const View& view)
{
    {
        const auto model = grid.config.model;

        for (int row = 0; row < 8; ++row)
        {
            for (int column = 0; column < 8; ++column)
            {
                const auto state = view.on ? view.cells[static_cast<size_t> (row)][static_cast<size_t> (column)] : Light::off;
                const auto colour = paletteOf (view.rowColours[static_cast<size_t> (row)], model);

                if (model == Model::launchpad)
                    light (grid, noteKey (Launchpad::pad (row, column)),
                           state == Light::off     ? value (Launchpad::lit, Launchpad::off)
                         : state == Light::playing ? value (Launchpad::pulsing, colour)
                         : state == Light::queued  ? value (Launchpad::flashing, colour)
                                                   : value (Launchpad::lit, colour));
                else if (model == Model::apcMini2)
                    light (grid, noteKey (apcPad (row, column)),
                           state == Light::off     ? value (ApcMini2::solid, 0)
                         : state == Light::playing ? value (ApcMini2::pulsing, colour)
                         : state == Light::queued  ? value (ApcMini2::flashing, colour)
                                                   : value (ApcMini2::solid, colour));
                else
                    light (grid, noteKey (apcPad (row, column)),
                           value (1, state == Light::off ? 0 : state == Light::playing ? ApcMini::green
                                   : state == Light::queued ? ApcMini::greenFlashing : ApcMini::yellow));
            }

            // A block's side button: lit when there's a block there.
            const auto rowLit = view.on && view.rows[static_cast<size_t> (row)];

            if (model == Model::launchpad)
            {
                if (row < 7)   // the bottom one is Stop All
                    light (grid, controllerKey (Launchpad::side (row)), value (Launchpad::lit, rowLit ? Launchpad::white : Launchpad::off));
            }
            else
            {
                light (grid, noteKey ((model == Model::apcMini2 ? ApcMini2::side0 : ApcMini::side0) + row), value (1, rowLit ? 1 : 0));
            }
        }

        // A track's button: lit while it plays, flashing while it waits to stop.
        for (int column = 0; column < 8; ++column)
        {
            const auto state = view.on ? view.tracks[static_cast<size_t> (column)] : Light::off;

            if (model == Model::launchpad)
                light (grid, controllerKey (Launchpad::top (column)),
                       state == Light::playing ? value (Launchpad::lit, Launchpad::green)
                     : state == Light::queued  ? value (Launchpad::flashing, Launchpad::red)
                     : value (Launchpad::lit, view.on && view.columns[static_cast<size_t> (column)] ? Launchpad::white : Launchpad::off));
            else
                light (grid, noteKey ((model == Model::apcMini2 ? ApcMini2::track0 : ApcMini::track0) + column),
                       value (1, state == Light::playing ? 1 : state == Light::queued ? 2 : 0));
        }

        if (model == Model::launchpad)
            light (grid, controllerKey (Launchpad::stopAll), value (Launchpad::lit, view.on ? Launchpad::red : Launchpad::off));
    }
}

//==============================================================================
void PadGrids::receive (const juce::String& port, const juce::MidiMessage& message)
{
    for (auto& grid : grids)
    {
        if (grid->config.input != port)
            continue;

        if (message.isNoteOn())
            press (*grid, message.getNoteNumber(), false);
        else if (message.isController() && message.getControllerValue() > 0)
            press (*grid, message.getControllerNumber(), true);
    }
}

void PadGrids::press (Grid& grid, int number, bool isController)
{
    if (onPress == nullptr)
        return;

    if (grid.config.model == Model::launchpad)
    {
        const auto tens = number / 10, units = number % 10;

        if (! isController && tens >= 1 && tens <= 8 && units >= 1 && units <= 8)
            onPress ("cell", 9 - tens, units);
        else if (isController && number >= 91 && number <= 98)
            onPress ("stop", 0, number - 90);
        else if (isController && number == Launchpad::stopAll)
            onPress ("stopAll", 0, 0);
        else if (isController && units == 9 && tens >= 2 && tens <= 8)
            onPress ("row", 9 - tens, 0);

        return;
    }

    if (isController)
        return;   // the faders: not the launcher's

    const auto mk2 = grid.config.model == Model::apcMini2;
    const auto track0 = mk2 ? ApcMini2::track0 : ApcMini::track0;
    const auto side0 = mk2 ? ApcMini2::side0 : ApcMini::side0;
    const auto shift = mk2 ? ApcMini2::shift : ApcMini::shift;

    if (number >= 0 && number < 64)
        onPress ("cell", 8 - number / 8, number % 8 + 1);
    else if (number >= track0 && number < track0 + 8)
        onPress ("stop", 0, number - track0 + 1);
    else if (number >= side0 && number < side0 + 8)
        onPress ("row", number - side0 + 1, 0);
    else if (number == shift)
        onPress ("stopAll", 0, 0);
}
