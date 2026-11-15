// D:\workspace\Fanan_web_hub\src\Showroom.h

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "UiHelpers.h"   // FineSlider

#include "ColourSwatch.h"
#include "Render3D.h"

#include <atomic>
#include <memory>

class PluginCanvas;
class Body3D;

//==============================================================================
/**
    The Showroom: where the window's 3D body (Body3D) is seen and edited in 3D, the finished
    instrument with the panel's controls (or its whole picture) printed on its upward faces,
    on a floor fading into the dark. It's the Body editor's view (BodyEditor holds it, its parts
    list and settings beside it).

    Seeing it, as 3D programs do:
        drag                    round it (round, above and below)
        right or middle drag,   moved across (pan); the wheel: closer
        Shift + drag
        Alt + drag              turned round the line of sight (roll)
        the view cube           (top right) a face, an edge or a corner clicked: seen from
                                there; dragged: round it; its arrows roll it, its house
                                brings the first view back
        the views below         front, back, left, right, top, bottom, three-quarter
        F, Home                 the whole body framed; the first view

    Editing the chosen part (a click on a part chooses it, a click beside the body none):
        Q select, W move, E rotate, R size   (the buttons at its top left)
        move: its arrows (along an axis) or its middle square (across the picture)
        rotate: its rings (round an axis; Shift: 15 degree steps)
        size: its handles (along its own axes), its middle box (all of it)
    Sizes and positions snap to the editor's grid when it snaps. Delete takes the part away.

    It's drawn in steps so it never waits: small while it's moved or edited, then half size,
    then sharp; a picture that's no longer wanted is stopped at once. Save picture: a PNG at
    twice the size, at full quality, on the backdrop or see-through (the body, and its shadow
    if it has one, alone).
*/
class Showroom final : public juce::Component,
                       private juce::Timer
{
public:
    explicit Showroom (PluginCanvas& canvas);
    ~Showroom() override;

    //==========================================================================
    // The Body editor's part in it.
    std::function<int()> chosenPart;                 // the JSON's index of the chosen part (-1: none)
    std::function<void (int)> onChoose;              // a part clicked (-1: none)
    std::function<void (bool final)> onPartEdited;   // the chosen part moved, turned or sized (final: let go)
    std::function<float()> gridPixels;               // what positions and sizes snap to (0: nothing)
    std::function<void()> onDelete;                  // Delete pressed

    enum class Tool { select, move, rotate, scale };
    void setTool (Tool);

    /** The first view; the whole body framed. */
    void resetView();
    void frameAll();

    //==========================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    PluginCanvas& canvas;

    //==========================================================================
    // The camera: round it, above it, turned; how close; where it looks.
    float yaw = 32.0f, pitch = 28.0f, roll = 0.0f, zoom = 1.0f, panX = 0.0f, panY = 0.0f;
    float toYaw = 0.0f, toPitch = 0.0f, toRoll = 0.0f;   // a view it's turning to (gliding there)
    bool gliding = false;

    Render3D::Showroom viewState (int w, int h) const;
    Render3D::Camera camera() const;   // the picture's camera, the view's size
    void glideTo (float newYaw, float newPitch, float newRoll);

    //==========================================================================
    // Its pictures: small while it moves, half size, then sharp; the newest wins.
    juce::Image shown;
    juce::String shownKey;
    int shownLevel = -1;            // 0 small, 1 half, 2 sharp
    juce::String askedKey;
    int askedLevel = -1;
    int quietTicks = 0;
    std::atomic<int> generation { 0 };
    juce::ThreadPool painter { juce::ThreadPoolOptions{}.withNumberOfThreads (1) };
    std::shared_ptr<const Body3D> pickBody;   // the body last drawn: what a click finds there
    juce::Image decal;                         // the panel's picture printed on it
    int decalPrint = -1;
    bool decalStale = true;

    juce::String stateKey() const;
    juce::Rectangle<int> viewArea() const;
    void request (int level);
    void timerCallback() override;
    void savePicture();
    void refreshDecal();

    //==========================================================================
    // The view cube.
    juce::Rectangle<float> cubeArea() const;
    struct CubeHit { int kind = 0; float dir[3] {}; };   // 0 none, 1 a view (dir), 2 roll left, 3 roll right, 4 home
    CubeHit cubeAt (juce::Point<float>) const;
    CubeHit cubeHover;
    void paintCube (juce::Graphics&);

    //==========================================================================
    // The chosen part's handles.
    Tool tool = Tool::move;
    enum class Handle { none, axisX, axisY, axisZ, plane, ringX, ringY, ringZ, sizeX, sizeY, sizeZ, sizeAll };
    Handle hovered = Handle::none;

    struct PartFrame
    {
        juce::var part;
        float centre[3] {}, axes[9] { 1, 0, 0, 0, 1, 0, 0, 0, 1 };   // its middle; its own axes (columns: x, y, z)
        float half[3] {};
        float mirrorAt = 0.0f;
        bool mirrored = false;
    };

    bool chosenFrame (PartFrame&) const;
    float handleLength (const Render3D::Camera&, const PartFrame&) const;
    Handle handleAt (juce::Point<float>) const;
    void paintHandles (juce::Graphics&, const Render3D::Camera&, const PartFrame&);

    // A handle being dragged: where it started.
    struct Grab
    {
        Handle handle = Handle::none;
        juce::Point<float> from;
        float x = 0, y = 0, z = 0, w = 0, h = 0, d = 0;
        float rot[9] { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
        juce::Point<float> axisOnScreen;   // a world unit along the axis, on the picture
        juce::Point<float> centreOnScreen;
        float startAngle = 0.0f, length = 1.0f, facing = 1.0f;
    } grab;

    void dragHandle (const juce::MouseEvent&);

    //==========================================================================
    // What the mouse is doing.
    enum class Drag { none, orbit, pan, roll, cube, handle };
    Drag drag = Drag::none;
    juce::Point<float> dragFrom;
    float yawFrom = 0, pitchFrom = 0, rollFrom = 0, panXFrom = 0, panYFrom = 0;
    bool moved = false;

    //==========================================================================
    juce::TextButton selectButton { "Select" }, moveButton { "Move" }, rotateButton { "Rotate" }, sizeButton { "Size" };
    juce::TextButton frontButton { "Front" }, backButton { "Back" }, leftButton { "Left" }, rightButton { "Right" },
                     topButton { "Top" }, bottomButton { "Bottom" }, threeQuarterButton { juce::String::fromUTF8 ("\xc2\xbe") },
                     frameButton { "Frame" }, saveButton { juce::String::fromUTF8 ("Save picture\xe2\x80\xa6") };
    juce::ToggleButton turnToggle { "Turn" }, shadowToggle { "Shadow" }, clearToggle { "See-through PNG" };
    juce::ComboBox printBox;
    FineSlider perspectiveSlider, lightSlider;
    juce::Label perspectiveLabel, lightLabel;
    ColourSwatchButton floorSwatch { "Floor" };
    std::unique_ptr<juce::FileChooser> chooser;

    static constexpr int barHeight = 76;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Showroom)
};
