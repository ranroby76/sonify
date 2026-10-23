// D:\workspace\Fanan_web_hub\src\KnobManEngine.h

/*
    KnobMan's own renderer, so a .knob file looks in Sonify the way KnobMan draws it.

    A C++ port of JKnobMan 1.3.3's rendering (Primitive, Eff, Layer.RenderFrame, Col, Tex,
    AnimCurve, IntelliAlpha, DynamicText, and the document reader).
    JKnobMan: Copyright (c) 2012-2020 g200kg, under the MIT License:

      Permission is hereby granted, free of charge, to any person obtaining a copy
      of this software and associated documentation files (the "Software"), to deal
      in the Software without restriction, including without limitation the rights
      to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
      copies of the Software, and to permit persons to whom the Software is
      furnished to do so, subject to the following conditions:

      The above copyright notice and this permission notice shall be included in all
      copies or substantial portions of the Software.

      THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
      IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
      FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
      AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
      LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
      OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
      SOFTWARE.

    The document is read once (parse) and never changes; rendering keeps a small cache of each
    layer's drawn primitive inside it, behind a lock, so different threads may render the same
    document or different ones at once.
*/

#pragma once

#include <juce_graphics/juce_graphics.h>

#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace KnobManEngine { struct Layer; struct Cache; }

//==============================================================================
struct KnobManDoc
{
    juce::String body;                 // the INI text it was read from (kept so a look can save and re-read it)
    int width = 64, height = 64, frames = 31, oversampling = 0;
    int previewFrames = 5, alignHorizontal = 0;   // kept as the file has them
    juce::Colour background;
    juce::StringArray notes;           // what couldn't be drawn exactly as KnobMan would (empty: exact)

    std::vector<std::shared_ptr<KnobManEngine::Layer>> layers;

    /** The 8 user animation curves: 12 points each (time and level, 0..100; -1 unused). */
    struct Curve { int tm[12] {}; int lv[12] {}; int stepReso = 0; double get (double ratio) const noexcept; };
    Curve curves[8];

    static std::shared_ptr<const KnobManDoc> parse (const juce::String& iniBody, juce::String& error);

    /** One layer's settings as KnobMan writes them ([Layer], then Name=..., Visible=...,
        ColR=..., Primitive=...): every key, its pictures aside. */
    juce::String layerText (int index) const;

    /** A layer of an edited document: an existing one (source, an index into layers; -1: a
        new one, KnobMan's defaults) with these keys set (KnobMan's names and text, as in the
        file: "ColR" -> "200"). */
    struct LayerEdit
    {
        int source = -1;
        std::vector<std::pair<juce::String, juce::String>> values;
    };

    /** The document with its layers made from these (in this order), at this size and frame
        count; pictures and textures, curves and the rest kept. Its body is written afresh. */
    std::shared_ptr<const KnobManDoc> edited (const std::vector<LayerEdit>& layers, int width, int height, int frames) const;

    ~KnobManDoc();

    // Rendering's cache (the primitives as drawn at a size): not part of the document.
    std::unique_ptr<KnobManEngine::Cache> cache;
};

namespace KnobManEngine
{
    /** One frame (0 .. doc.frames-1), round(width*scale) x round(height*scale) ARGB (premultiplied),
        transparent background. scale > 1 renders at that bigger size, sharper (as KnobMan's
        oversampling does); the document's own oversampling is rendered and decimated as KnobMan's
        export does. */
    juce::Image renderFrame (const KnobManDoc& doc, int frame, float scale = 1.0f);

    /** Draws a Sphere or Metal Circle layer's shape another way (Sonify's lighting): the layer's
        index, KnobMan's primitive type (4 Metal Circle, 6 Sphere) and the canvas size it's drawn
        at. An invalid image (or one of another size): KnobMan's own. Everything else on the layer
        (its zoom, turn, shadows, masks) stays KnobMan's. */
    using ShapeDrawer = std::function<juce::Image (int layerIndex, int primType, int width, int height)>;

    /** The same, with Sphere and Metal Circle layers drawn by drawer (none: as KnobMan does). */
    juce::Image renderFrame (const KnobManDoc& doc, int frame, float scale, const ShapeDrawer& drawer);
}
