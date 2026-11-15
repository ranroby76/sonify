// D:\workspace\Fanan_web_hub\src\MeshField.h

#pragma once

#include <juce_core/juce_core.h>

#include <memory>
#include <vector>

//==============================================================================
/**
    A 3D model from a file (Wavefront OBJ, STL binary or text) as a signed distance field the
    body's renderer marches (Body3D's "mesh" parts): read once, measured on a grid, kept for
    as long as the file stays as it is.

    The model is centred and scaled so its longest side is 1: extent[] is its size each way
    (the longest 1). Its distance there is in those units; below 0 inside.
*/
namespace MeshField
{
    struct Grid
    {
        int nx = 0, ny = 0, nz = 0;
        float lo[3] { 0, 0, 0 };        // the grid's first corner (model units)
        float cell = 1.0f;
        float extent[3] { 1, 1, 1 };    // the model's size each way (the longest 1)
        int triangles = 0;
        std::vector<float> d;           // nx * ny * nz, x fastest

        /** Its distance at a point (model units): between the grid's points, and past its edge. */
        float at (float x, float y, float z) const;
    };

    /** The model in a file, its distance measured (upZ: a CAD or STL model standing on z).
        Kept: the same file, unchanged, comes back at once. Null (and why) when it can't be read. */
    std::shared_ptr<const Grid> load (const juce::File& file, bool upZ, juce::String* error = nullptr);

    /** The files it reads. */
    constexpr const char* wildcard = "*.obj;*.stl";
}
