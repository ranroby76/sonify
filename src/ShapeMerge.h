// D:\workspace\Fanan_web_hub\src\ShapeMerge.h

#pragma once

#include "GuiLayout.h"

#include <optional>
#include <vector>

//==============================================================================
/**
    Photoshop's Merge Shapes (Ctrl+M in the Edit UI): shapes selected together become one
    drawn shape, the area they cover between them (their union: where they overlap is one,
    holes stay holes unless another shape covers them). Each is taken as it shows on the
    panel: turned, lopsided, per corner, a line as its stroke; one that isn't filled as the
    form its outline runs along.

    The merged shape wears the top one's look (its colour or material, patterns, layer style,
    outline, filled or not, opacity, page) and lies in its layer; it's upright, its box snug
    round it, and filled or not like any other.
*/
namespace ShapeMerge
{
    /** The shapes among these elements (by index), back to front: two or more merge. */
    std::vector<int> mergeable (const GuiLayout& layout, const std::vector<int>& indexes);

    /** The shapes (back to front) as one, or nothing when they don't cover anything. */
    std::optional<GuiWidget> merge (const std::vector<const GuiWidget*>& shapes);

    /** A path as SVG path data (M, L, C, Q and Z, absolute), for a drawn shape's "path". */
    juce::String toSvgPath (const juce::Path& path);
}
