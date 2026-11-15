// D:\workspace\Fanan_web_hub\src\Render3D.h

#pragma once

#include "Model3D.h"

#include <atomic>
#include <functional>

class Body3D;

//==============================================================================
/**
    The 3D Studio's renderer: a model (Model3D) as an object on the panel, ray-marched
    (signed distance fields: a turned body with its fillets, flutes and knurling, rounded caps,
    a fader cap with its grooves, a lever on its nut), shaded as its materials are (plastics
    with their clear coat, rubber, spun and brushed metals, chrome, gold...) under the panel's
    global light and a soft studio around it, with the shadow it casts and the light it gives
    (a knob's LED ring, a button's lamp) falling on the panel; the panel itself see-through.

    Units are the look's canvas pixels; the picture is drawn at scale, with pad pixels of room
    round the canvas for its shadow. All the computer's cores draw it.
*/
namespace Render3D
{
    /** A frame (t: 0..1; a button or a toggle: 0 off, 1 on). samples: per pixel, each way (0:
        as fine as its size allows). */
    juce::Image renderFrame (const Model3D& model, int canvasWidth, int canvasHeight, float t, float scale, float pad,
                             float lightAngle, float lightAltitude, int samples = 0);

    /** While one lives, frames are drawn quickly (one sample a pixel): a preview being changed
        or played. */
    struct ScopedDraft
    {
        ScopedDraft();
        ~ScopedDraft();
    };

    /** Whether frames are being drawn quickly now (a ScopedDraft lives): pictures kept are
        kept apart from the sharp ones. */
    bool drafting();

    /** While one lives (an element being resized), a picture already made at another size may
        be stretched to stand in for one not made yet: made sharp once it's let go. */
    struct ScopedStretch
    {
        ScopedStretch();
        ~ScopedStretch();
    };

    bool stretching();

    /** While one lives on this thread (a window or a preview being painted), a picture asked
        for that isn't made yet is made in the background (later()): a stand-in, or nothing, is
        drawn meanwhile, and who is painted again once it's made. Nothing lives: made now. */
    struct ScopedLater
    {
        explicit ScopedLater (juce::Component& who);
        ~ScopedLater();

    private:
        juce::Component* old = nullptr;
    };

    /** Who's painting under a ScopedLater on this thread, or nullptr: make pictures now. */
    juce::Component* later();

    /** job made in the background, with this thread's camera view (ScopedView) and quickness
        (ScopedDraft); key: one job at a time for it. who (and everyone else waiting for the
        same key) is painted again once it's made. place: where it goes (its size and what's
        on it, not its look): a job waiting for the same place, only who waiting for it, is let
        go (a look being changed: only the newest is wanted). The newest jobs go first; the
        oldest are let go when too many wait (who is painted again, to ask once more if it's
        still wanted). */
    void makeLater (const juce::String& key, const juce::String& place, std::function<void()> job, juce::Component& who);

    /** The panel camera: while one lives (on this thread), a frame is seen from where the
        panel's one camera sees it, not from straight in front of it. leanX, leanY: the
        control's offset from the panel's middle over the camera's distance (GuiLayout::
        cameraLean), so a control on the left shows a little of its right side, one low on the
        panel a little more of its top, as in a photograph of real hardware. */
    struct ScopedView
    {
        /** slopeX, slopeZ: the slope (degrees) of the body's face the control stands on (Face):
            rising towards the back, rising to the left. */
        ScopedView (float leanX, float leanY, float slopeX = 0.0f, float slopeZ = 0.0f);
        ~ScopedView();

    private:
        float oldX = 0.0f, oldY = 0.0f, oldSlopeX = 0.0f, oldSlopeZ = 0.0f;
    };

    /** The window's 3D body (Body3D): the whole window, scale pixels a window pixel, lit by
        the panel's global light, seen by the panel camera from cameraDistance window pixels
        (GuiLayout::cameraDistance; 0: from infinitely far). Where there's no body: its floor
        (see-through unless it has a colour), with the body's shadow on it. */
    juce::Image renderBody (const Body3D& body, float scale, float lightAngle, float lightAltitude, float cameraDistance,
                            int samples = 0);

    /** The body's face at a point of the window (x, y: window pixels), as the body's picture
        shows it: which part it is, how high it is, and its slope (degrees: rising towards the
        back, rising to the left), which a control standing there is seen with. False: no body there. */
    struct Face
    {
        int part = -1;
        float height = 0, slopeX = 0, slopeZ = 0;
    };

    bool bodyFace (const Body3D& body, float cameraDistance, float x, float y, Face& face);

    /** The Showroom: the body seen from anywhere round it (yaw: degrees round it, 0 from the
        front; pitch: degrees above it, 90 straight down), on a floor fading into the dark, the
        panel's picture (its controls, and its background if asked) printed on its upward
        faces. zoom 1: the whole body fills the picture. */
    struct Showroom
    {
        float yaw = 30.0f, pitch = 32.0f, zoom = 1.0f, perspective = 45.0f;
        float roll = 0.0f;               // degrees: the camera turned round its own line of sight
        float panX = 0.0f, panY = 0.0f;  // where it looks, moved across the picture (world pixels: right, up)
        juce::Colour floor { 0xff2c2d32 };
        juce::Image decal;               // the window's picture: window pixels x decalScale
        float decalScale = 1.0f;
        bool shadow = true;              // its shadow on the floor (a see-through floor: the shadow alone)

        // What it's framed round: the body's middle (from the window's middle, y up) and its
        // reach. radius 0: the body's own box (Body3D's). frameOf() works it out from the JSON,
        // so a picture and the editor's handles over it agree.
        float centre[3] { 0, 0, 0 };
        float radius = 0.0f;

        // Asked to stop (a newer picture wanted): the picture's left unmade (invalid).
        const std::atomic<int>* stop = nullptr;
        int stopUnless = 0;
    };

    /** A body's middle and reach, from its JSON (its parts' boxes, their copies and mirrors;
        a turned part: as far as it could reach), for a window W x H. */
    void frameOf (const juce::var& bodyJson, int W, int H, Showroom& view);

    juce::Image renderShowroom (const Body3D& body, int width, int height, const Showroom& view,
                                float lightAngle, float lightAltitude, int samples = 0);

    /** The Showroom's camera for a picture w x h: what draws the picture, and the editor's
        handles over it (world: pixels from the window's middle, y up, z towards the front). */
    struct Camera
    {
        float c[3] {}, r[3] {}, u[3] {};   // towards the camera, its right, its up
        float target[3] {};                // where it looks
        float camL = 0, fit = 1;           // its distance (0: no perspective); picture pixels a world pixel
        int w = 0, h = 0;

        /** A world point on the picture; false: behind the camera. */
        bool project (const float p[3], juce::Point<float>& at) const;

        /** The ray through a point of the picture. */
        void ray (juce::Point<float> at, float o[3], float d[3]) const;
    };

    Camera showroomCamera (int width, int height, const Showroom& view);

    /** The part a point of the Showroom's picture shows (Body3D's index; -1: none). */
    int partAt (const Body3D& body, const Camera& camera, juce::Point<float> at);

    /** How far its shadow and glow reach past the canvas, in canvas pixels. */
    float overhang (const Model3D& model, int canvasWidth, int canvasHeight, float lightAltitude);
}
