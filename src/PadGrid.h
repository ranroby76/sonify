// D:\workspace\Fanan_web_hub\src\PadGrid.h

#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_graphics/juce_graphics.h>

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <vector>

//==============================================================================
/**
    Pad-grid controllers playing Song Blocks' Live (the clip launcher): Novation's Launchpad
    (Mini MK3, X, Pro MK3, in their Programmer mode) and Akai's APC mini (the first one and the
    MK2). They're control surfaces in Settings > MIDI Setup (their protocol: Launchpad, APC mini,
    APC mini MK2), so their ports play Sonify and no track hears them.

    The 8 x 8 pads are the launcher's grid as the page draws it: a row for each block (the first
    at the top), a column for each track the blocks have clips on (the first at the left); the
    first eight of each. A pad launches its block on its track; a row's side button launches the
    whole block; a column's button (the Launchpad's top row, the APC mini's row under the pads)
    stops its track; Shift on the APC mini, and the Launchpad's lowest side button (its Stop Solo
    Mute, as in Ableton Live), is Stop All (so the Launchpad's eighth row launches from its pads). The lights follow the grid: a cell with a clip in its block's colour (the
    first APC mini: yellow), playing pulsing (green), waiting to start flashing; a track's stop
    button lit while it plays, flashing while it waits to stop. With Live off, they're dark.

    Message thread. Their messages come from the engine's queue for surfaces (receive); the
    lights go out through each one's own output port.
*/
class PadGrids final
{
public:
    enum class Model { launchpad, apcMini, apcMini2 };

    /** A control surface's protocol that's a pad grid: "launchpad", "apcmini", "apcmini2". */
    static bool isPadProtocol (const juce::String& protocol);
    static Model modelFromName (const juce::String& protocol);

    struct Config
    {
        juce::String name;                   // the device's
        Model model = Model::launchpad;
        juce::String input, output;          // MIDI ports by name
    };

    /** What the grid shows. Rows top to bottom, columns left to right. */
    enum class Light { off, clip, playing, queued };

    struct View
    {
        bool on = false;                                            // Live is on
        std::array<std::array<Light, 8>, 8> cells {};               // [row][column]
        std::array<juce::Colour, 8> rowColours {};                  // each block's colour
        std::array<bool, 8> rows {}, columns {};                    // a block, a track there
        std::array<Light, 8> tracks {};                             // a track's state: playing, stopping (queued), off
    };

    /** A press: "cell" (row, column), "row" (row), "stop" (column) or "stopAll"; 1-based. */
    std::function<void (const juce::String& action, int row, int column)> onPress;

    PadGrids() = default;
    ~PadGrids();                             // dark, and a Launchpad back in its Live mode

    /** The pad grids now (the device list or the ports changed). New ones are greeted. */
    void configure (const std::vector<Config>& list);
    bool isEmpty() const noexcept            { return grids.empty(); }

    /** A port one of them sends from (its messages are this's). */
    bool ownsPort (const juce::String& port) const;

    void receive (const juce::String& port, const juce::MidiMessage& message);

    /** The lights for the grid as it is (only what changed goes out). */
    void show (const View& view);

private:
    struct Grid
    {
        Config config;
        std::unique_ptr<juce::MidiOutput> output;
        std::map<int, int> sent;             // light key -> what it was sent
    };

    void greet (Grid& grid);
    void goDark (Grid& grid);
    void showOn (Grid& grid, const View& view);
    void light (Grid& grid, int key, int value);   // key: a pad's or button's own, as below
    void press (Grid& grid, int number, bool isController);

    std::vector<std::unique_ptr<Grid>> grids;
};
