// D:\workspace\Fanan_web_hub\src\LayerMerge.h

#pragma once

#include "LayerDoc.h"

#include <vector>

//==============================================================================
/**
    The Knob Studio's Merge (Ctrl+M on layers picked together with Shift+click): the layers
    become one Shape layer, the area they cover between them, as Photoshop merges shapes. Any
    kind of layer merges (circles, rectangles, lines, text, pictures with see-through parts...):
    each as its shape shows, what its masks keep of it, its edge where it's half solid; where
    they overlap is one, holes stay holes unless another layer covers them.

    Layers that all move alike (the same zoom, offset and turn, animated or not) merge in their
    own frame and the merged one moves as they did: a pointer made of parts still turns with the
    knob. Layers that move differently merge as they lie at the frame shown, and the merged one
    stays still. The merged layer wears the top one's look (colour, material, bevel, shadows,
    outline, texture...) and lies in its place in the stack.
*/
namespace LayerMerge
{
    /** The layers (document indexes) merged into one; t: the frame they're taken at when they
        move differently. Returns the merged layer's index, or -1 (fewer than two, or nothing
        covered). */
    int merge (LayerDoc& doc, std::vector<int> indexes, float t);
}
