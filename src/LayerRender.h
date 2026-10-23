// D:\workspace\Fanan_web_hub\src\LayerRender.h
// From KnobMaker (C:\workspace\knobmaker): the same layer builder in both apps (LayerRender.cpp
// finds Image layers' pictures through LayerImages). Stella adds the room around the canvas
// (renderFrame's pad and overhangOf): shadows and glows reach past the canvas instead of being
// cut at its edge; materials (applyMaterial); and a light that stays put on screen: a layer
// that turns is lit as if the light turned back by as much, so a turning knob's highlights,
// sheen and bevels stay where the light is while its grain and marks turn with it.

/*
    LayerRender.h

    Rasterises a LayerDoc. Each layer draws its primitive into its own ARGB
    buffer, gets embossed from its own alpha, then has colour adjustments and
    masks applied before being composited onto the accumulator under its
    transform.

    Emboss is the piece that does the heavy lifting: it derives a height field
    from the primitive's alpha, takes the gradient of that field as a surface
    normal, and shades it against lightDir. That is how an arbitrary shape gets
    a bevel without anyone writing shape-specific code.
*/

#pragma once

#include "LayerDoc.h"

//==============================================================================
/** What the renderer noticed while drawing, for the UI to report. Optional
    everywhere — pass nullptr and nothing is collected. */
struct RenderNotes
{
    juce::StringArray messages;
};

class LayerRender
{
public:
    /** Renders the document at normalised time t (0..1).

        scale > 1 renders above the document's own pixel size. The engine is
        resolution independent — every primitive is described in fractions of
        the canvas — so this yields a genuinely sharper image rather than an
        upscaled one. Use it for on-screen preview; export stays at 1.0. */
    static juce::Image renderFrame (const LayerDoc&, float t, float scale = 1.0f,
                                    RenderNotes* notes = nullptr);

    /** Stella: the same, with pad pixels of room on every side of the canvas (pixels of the
        result, already scaled): drop shadows, glows and outside outlines go on into it
        instead of stopping at the canvas's edge. The canvas lies at (pad, pad). */
    static juce::Image renderFrame (const LayerDoc&, float t, float scale, float pad,
                                    RenderNotes* notes = nullptr);

    /** Stella: how far past the canvas the document's drop shadows, glows and outside
        outlines can reach, in canvas pixels at 1x (0: none of them is on). */
    static float overhangOf (const LayerDoc&);

    /** Renders every frame into one strip. */
    static juce::Image renderFilmstrip (const LayerDoc&, bool vertical, float scale = 1.0f);

    /** Renders a single layer in isolation. */
    static juce::Image renderLayer (const LayerDoc&, int layerIndex, float t, float scale = 1.0f);

    /** Stella: Photoshop's layer style on a picture of a shape (with room round it for what
        reaches past it, LayerStyle::reach): Color Overlay, Inner Shadow, Stroke and Bevel &
        Emboss (with its Contour) over the shape, Drop Shadow under it, stacked as Photoshop
        stacks them. Angles that use the global light take globalAngle and globalAltitude.
        scale: the picture's pixels per pixel of the style's sizes. */
    static void applyLayerStyle (juce::Image& picture, const LayerStyle&, float scale, float globalAngle, float globalAltitude);

    /** Stella: just a stroke round a picture's shape (its colour and opacity in its alpha). */
    static juce::Image strokeBand (const juce::Image& silhouette, const StrokeSpec&, float scale);

private:
    /** `t` is the frame position, 0..1. Only the LED reads it — everything else
        is shaped by the effect chain rather than by where it sits in the strip. */
    static juce::Image rasterisePrimitive (const PrimitiveSpec&, int w, int h, float scale,
                                           float t = 0.0f);

    /** CircleFill, MetalCircle and Sphere are one analytic renderer with three
        specular models, exactly as KnobMan does it: a per-pixel distance field
        with its own antialiasing, its own rim bevel driven by Emboss, and an
        edge fade driven by Diffuse. spectype 0 = flat tint, 1 = brushed metal
        (anisotropic sin(2t) sheen), 2 = true Phong sphere. */
    static juce::Image renderCircleFamily (const PrimitiveSpec&, int w, int h, int spectype);

    /** Circle: an analytic ring whose thickness is Width as a percentage of the
        radius, with a tube highlight running across the ring. */
    static juce::Image renderRing (const PrimitiveSpec&, int w, int h);

    /** True when the primitive shades itself and must skip the generic emboss
        pass, which would otherwise double-shade it. */
    static bool hasOwnShading (PrimType) noexcept;

    /** Photoshop's Bevel & Emboss. Every style is a band on the signed distance
        to the layer's own edge, turned into a height field, shaded against a
        light with both an azimuth and an altitude, then split into a highlight
        pass and a shadow pass with their own blend modes.

        This replaced the old applyEmboss. The circle family still builds its
        rim inside renderCircleFamily from PrimitiveSpec::emboss — that one is
        analytic and has nothing to do with this. */
    static void applyBevel (juce::Image&, const PrimitiveSpec&, float scale,
                            float lightAngleDeg, float lightAltitudeDeg, const juce::Image* silhouette = nullptr);
    static void applyGloss         (juce::Image&, const PrimitiveSpec&);

    /** Stella: the layer as a material (stella_paint's metals, stones, plastics, woods...), in
        its colour with a finish and a pattern over it, lit by the document's light. The light is
        turned back by the layer's angle at t, so on screen it stays where it is however the
        layer turns (a gold knob's highlight doesn't spin with it); the primitive's own shading
        (the circle family's rim, a sphere) is kept over it. Runs first, before the bevel. */
    static void applyMaterial (juce::Image&, const PrimitiveSpec&, const EffectSpec&, float t, float scale,
                               float lightAngleDeg, float lightAltitudeDeg);

    /** Bump or Fill, from TextureLibrary. Runs after emboss on purpose: in
        KnobMan the embossed bevel deliberately ignores the texture, and doing
        it in this order reproduces that. scale: the render's, so a bump is as
        deep at 4x as at 1x. */
    static void applyTexture       (juce::Image&, const PrimitiveSpec&, float scale);

    /** Photoshop's Stroke: an outline hugging the shape's silhouette, placed
        outside it, centred on it, or inside it. Driven by a signed distance
        field built from the layer's own alpha, so it follows any shape without
        knowing what that shape is. */
    /** Photoshop's Stroke, applied to the layer AS PLACED ON THE CANVAS —
        same reason as the shadows. A CircleFill or a Sphere is rasterised
        inscribed in its own layer image, so measured in layer space an Outside
        stroke has nowhere to go but the four corners, and the layer's zoom then
        shrinks those into a square frame.

        Working in canvas space also matches Photoshop, where a layer style is
        applied after the transform: an outline keeps its width in screen pixels
        instead of thinning out as the layer is scaled down. */
    static void applyStroke (juce::Image& placed, const PrimitiveSpec&, float scale,
                             bool* wasClipped = nullptr);

    /** The drop shadow, the inner shadow and the outer glow, which are one
        construction with two switches.

        Outward (`inward` false) is a shadow or a glow: the coverage starts at
        the silhouette and fades outward over `size`. Inward is an inner shadow:
        the same ramp read from inside, clipped to the layer's own alpha, so it
        darkens the inside of the edge instead of the outside. An outer glow is
        just an outward pass with no offset.

        Pass the layer AS PLACED ON THE CANVAS — pass the image
        compositeLayer has already drawn into, not the raw layer image.

        That distinction is the whole feature. A Sphere is rasterised inscribed
        in its own layer image, touching all four edges, so measured in layer
        space it has no room around it for a shadow and the only empty space is
        the four corners. Scale that down by the layer's zoom and those corners
        arrive as a square block behind the knob.

        Working from the placed image also makes the offset plain screen pixels,
        with no zoom to divide out and no rotation to undo.

        fullAlpha: how opaque the layer itself is at its most solid, when the
        picture has an outline painted over it since (0: measured from it). The
        shape's edge is where it is half that. */
    static juce::Image makeEdgeEffect (juce::Image& placed, const ShadowSpec&,
                                       float scale, bool inward, bool* wasClipped = nullptr,
                                       int fullAlpha = 0);

    /** Composites one image onto another through a blend mode. Normal takes the
        fast path through juce::Graphics; the rest go per-pixel, which is what
        makes a Screen-blended glow add light rather than lay a coloured disc
        over the top. */
    static void blendOnto (juce::Image& canvas, const juce::Image& src, BlendMode);

    /** Places the layer, then stacks its edge effects around it. Takes the
        canvas image rather than a Graphics, because a blend mode other than
        Normal has to reach the pixels directly. */
    static void compositeWithEdgeEffects (juce::Image& canvas, const DocLayer&, juce::Image&,
                                          int canvasW, int canvasH, float t, float scale,
                                          RenderNotes* notes, float pad = 0.0f);
    static void applyColourAdjust  (juce::Image&, const EffectSpec&, float t);
    static void applyMasks         (juce::Image&, const EffectSpec&, float t);
    /** pad: where the canvas lies in the image drawn into (Stella's room around it).
        withAlpha false: placed at full strength, its Alpha left to the caller (the
        edge effects measure the shape, not how see-through it is). */
    static void compositeLayer     (juce::Graphics&, const DocLayer&, juce::Image&,
                                    int canvasW, int canvasH, float t, float pad = 0.0f,
                                    bool withAlpha = true);
};
