// D:\workspace\Fanan_web_hub\src\ModMatrix.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "GuiLayout.h"

#include <vector>

//==============================================================================
/**
    The modulation matrix: an element (GuiWidget::Type::matrix) that routes the plugin's
    modulation sources to its parameters, as many routes as it's given.

    Every route is three of the plugin's parameters, so it's automated and kept in presets:
        <param>_src<n>   the source: 0 none, 1.. the "sources" in order
        <param>_dst<n>   the destination: 0 none, 1.. the plugin's own parameters in order
                         (every parameter but the matrices' own)
        <param>_amt<n>   the amount, -1..1
    (<param> is the element's parameter, "mod" when it has none; n from 1.)

    Its "settings" "matrix" (the design), every key optional:
        "style"         slots (a list of routes: source, destination, amount), grid (sources
                        down, destinations across: a pin or a knob where they meet), patch (a
                        patch bay: the sources' output jacks, the destinations' input jacks,
                        cables between them)
        "routes"        how many (1..64; the standard 8)
        "sources"       the sources' names, in order ("LFO 1", "Env 2", "Velocity", "Mod wheel"...)
        "matchInputs"   patch: as many inputs as outputs (the parameters first, then inputs
                        of their own, In 1, In 2..., for what's still to be wired)
        "targets"       grid and patch: the destinations they show (parameter ids; the
                        standard: the plugin's first 8)
        "cell"          grid: "pin" (on or off, the amount its ring), "knob", "bar"
        "amount"        a new route's amount (the standard 0.5)
        "colour", "panelColour", "textColour", "lineColour", "font", "textSize" %
        "cables"        patch: the cables' colours in turn; "sag" (0..100)
        "jack"          patch: "eurorack" (the standard: a nut and a hole), "studio" (a round
                        socket), "minimal"

    One matrix's parts can also lie anywhere on the panel, each an element of its own sharing
    the matrix's parameters (the same <param>): the cables run across the whole window, and a
    source dragged onto any knob or slider routes to it (its amount a ring round it):
        "style": "jack"     a jack: "role" "out" (its "source": 1..) or "in" (its "target":
                            a parameter id)
        "style": "chip"     a source to drag onto any control, or any input ("source": 1..)
        "style": "assign"   a source's assign button ("source": 1..): on, the next control
                            touched is routed from it (Compact assign)
*/
namespace ModMatrix
{
    struct Route
    {
        int source = 0, destination = 0;   // 0: none
        float amount = 0.0f;               // -1..1
        bool active() const noexcept    { return source > 0 && destination > 0; }
    };

    /** What a point of it is. */
    struct Hit
    {
        enum class Kind { none, clear, source, destination, amount, cell, output, input };
        Kind kind = Kind::none;
        int route = -1;     // a slot's route
        int row = -1;       // a grid's source row, a patch's output (0-based)
        int column = -1;    // a grid's destination column, a patch's input (0-based)
    };

    /** What's drawn: the routes, the sources' and destinations' names, the destinations a
        grid or a patch bay shows (1-based destination numbers), and a cable being plugged
        (from an output to a point; -1: none). */
    struct View
    {
        std::vector<Route> routes;
        juce::StringArray sources, destinations;
        std::vector<int> columns;
        int draggingFrom = -1;
        juce::Point<float> draggingTo;
        int hoverRoute = -1;
    };

    juce::var designOf (const GuiWidget& matrix);

    /** A part of a matrix lying on its own (a jack, a chip, an assign button). */
    bool isPart (const juce::var& design);
    bool isPart (const GuiWidget& matrix);
    juce::String roleOf (const juce::var& design);      // a jack's "out" or "in"; a chip's and an assign button's "out"
    int sourceOf (const juce::var& design);             // 1..; 0 none
    juce::String targetOf (const juce::var& design);    // an input's parameter id

    /** Its picture: label its source's or target's name, lit while it's in use (an assign
        button: while it's on). */
    void paintPart (juce::Graphics& g, const juce::var& design, juce::Rectangle<float> area, const juce::String& label, bool lit);

    /** The middle of a part's jack (or chip) in its area. */
    juce::Point<float> partPoint (const juce::var& design, juce::Rectangle<float> area);

    /** A patch bay's jacks: where an output (0-based source) or an input (0-based column) is. */
    juce::Point<float> outputPoint (const juce::var& design, juce::Rectangle<float> area, const View& view, int source);
    juce::Point<float> inputPoint (const juce::var& design, juce::Rectangle<float> area, const View& view, int column);
    float jackSize (const juce::var& design, juce::Rectangle<float> area, const View& view);

    /** A patch bay's jack (as its design draws them) at a point, size across, lit or not. */
    void paintJackAt (juce::Graphics& g, const juce::var& design, juce::Point<float> centre, float size, bool lit);

    /** A cable between two points (it hangs by sag 0..1), a route's colour. */
    void paintCable (juce::Graphics& g, juce::Point<float> from, juce::Point<float> to, float thickness, float sag, juce::Colour colour);
    juce::Colour cableColour (const juce::var& design, int route);

    /** A modulation's ring round a control: from its value, by the amount (a knob: an arc in
        its turn; a slider: a bar beside its track). */
    void paintRing (juce::Graphics& g, juce::Rectangle<float> box, bool knob, bool vertical, float value, float amount, juce::Colour colour, int ringIndex);

    juce::String styleOf (const juce::var& design);
    int routesOf (const juce::var& design);
    juce::StringArray sourcesOf (const juce::var& design);

    /** A route's parameter: part "src", "dst" or "amt", route from 0. */
    juce::String paramId (const GuiWidget& matrix, int route, const char* part);

    /** Whether a parameter id is one of a matrix's own (not a destination). */
    bool isMatrixParam (const GuiLayout& layout, const juce::String& id);

    void paint (juce::Graphics& g, const juce::var& design, juce::Rectangle<float> area, const View& view);
    Hit hitTest (const juce::var& design, juce::Rectangle<float> area, const View& view, juce::Point<float> at);

    /** A patch bay: the input a cable let go at a point is plugged into (the nearest, if it's
        near enough: a jack and a half); -1 none. */
    int inputNear (const juce::var& design, juce::Rectangle<float> area, const View& view, juce::Point<float> at);

    /** The gallery's templates. */
    juce::StringArray templateNames();
    juce::var makeTemplate (int index);

    juce::File galleryFolder();
    constexpr const char* fileExtension = ".fkmatrix";
}
