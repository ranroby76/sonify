// D:\workspace\Fanan_web_hub\src\KnobFile.h
// From KnobMaker (C:\workspace\knobmaker), unchanged: the same layer builder in both apps.

/*
    KnobFile.h

    Reads KnobMan / JKnobMan .knob documents.

    The format is not the binary blob it looks like. A .knob is a PNG whose
    first eight bytes have been overwritten with a different signature; the
    document itself lives in a tEXt chunk as INI-style text with [Section]
    blocks and Key=Value lines. Restore the signature and it is an ordinary
    PNG whose image is the preview thumbnail.

    Older files may instead be BMP-wrapped, "KM"-headered, or plain text; all
    four containers are handled.

    Mapping to our LayerDoc is close but not exact — KnobMan has parameters we
    do not implement (textures, drop/inner shadow, per-layer light animation,
    custom animation curves). Anything unmapped is reported in
    ImportResult::warnings rather than silently dropped.
*/

#pragma once

#include "LayerDoc.h"

//==============================================================================
namespace KnobFile
{
    struct ImportResult
    {
        bool ok = false;
        juce::String error;
        juce::StringArray warnings;
        LayerDoc document;
        juce::Image preview;      ///< the embedded PNG, when the container was PNG
    };

    /** Parses a .knob file from raw bytes. */
    ImportResult parse (const juce::MemoryBlock& raw);

    /** Parses a .knob file from disk. */
    ImportResult parseFile (const juce::File&);

    /** Strips the container and returns the INI body, or an empty string. */
    juce::String extractBody (const juce::MemoryBlock& raw);

    /** Restores the PNG signature so the preview can be decoded. Returns an
        invalid image for non-PNG containers. */
    juce::Image extractPreview (const juce::MemoryBlock& raw);

    /** A look from KnobMan's layers as Sonify's editor shows them: one for each layer of its
        KnobMan document, in order, each linked to it (DocLayer::knobmanLayer). */
    std::vector<DocLayer> layersOf (const KnobManDoc& knobman);

    /** A look from KnobMan, edited in Knob Studio (its layers changed, added, removed or
        moved; its size or frames): its KnobMan document made to match, so it's still drawn as
        KnobMan draws it, and its layers as they read back from that. A change KnobMan's drawing
        has no setting for doesn't stick: undone says what (empty: everything stuck). */
    LayerDoc applyEdits (const LayerDoc& edited, juce::StringArray& undone);
}
