// D:\workspace\Fanan_web_hub\src\KnobFile.cpp
// From KnobMaker (C:\workspace\knobmaker), unchanged: the same layer builder in both apps.

/*
    KnobFile.cpp
*/

#include "KnobFile.h"
#include "KnobManEngine.h"
#include "TextureLibrary.h"

#include <cmath>

#include <map>
#include <set>
#include <vector>

namespace
{
    //==========================================================================
    /** One [Section] of the INI body, as a key -> value map. */
    struct Section
    {
        std::map<juce::String, juce::String> values;

        const juce::String* find (const juce::String& key) const
        {
            auto it = values.find (key);
            return it == values.end() ? nullptr : &it->second;
        }

        juce::String getString (const juce::String& key, const juce::String& fallback) const
        {
            auto* v = find (key);
            return v != nullptr ? *v : fallback;
        }

        int getInt (const juce::String& key, int fallback) const
        {
            auto* v = find (key);
            return v != nullptr ? v->getIntValue() : fallback;
        }

        float getFloat (const juce::String& key, float fallback) const
        {
            auto* v = find (key);

            if (v == nullptr)
                return fallback;

            // KnobMan writes floats with the host locale's separator.
            return v->replaceCharacter (',', '.').getFloatValue();
        }

        bool has (const juce::String& key) const { return find (key) != nullptr; }
    };

    //==========================================================================
    class IniBody
    {
    public:
        explicit IniBody (const juce::String& text)
        {
            juce::StringArray lines;
            lines.addLines (text);

            juce::String current = "";

            for (auto raw : lines)
            {
                auto line = raw.trimEnd();

                if (line.startsWith ("[") && line.contains ("]"))
                {
                    current = line.fromFirstOccurrenceOf ("[", false, false)
                                  .upToFirstOccurrenceOf ("]", false, false);
                    sections[current];
                    continue;
                }

                const int eq = line.indexOfChar ('=');

                if (eq <= 0 || current.isEmpty())
                    continue;

                sections[current].values[line.substring (0, eq)] = line.substring (eq + 1);
            }
        }

        bool hasSection (const juce::String& name) const
        {
            return sections.find (name) != sections.end();
        }

        const Section& getSection (const juce::String& name) { return sections[name]; }

    private:
        std::map<juce::String, Section> sections;
    };

    //==========================================================================
    juce::uint32 readBigEndianInt (const juce::uint8* p) noexcept
    {
        return ((juce::uint32) p[0] << 24) | ((juce::uint32) p[1] << 16)
             | ((juce::uint32) p[2] << 8)  |  (juce::uint32) p[3];
    }

    /** Byte offset at which the INI text starts, or -1 if unrecognised. */
    int findBodyOffset (const juce::uint8* data, size_t size)
    {
        if (size < 4)
            return -1;

        const juce::uint16 id = (juce::uint16) (((juce::uint16) data[0] << 8) | data[1]);

        // 0xFFFE — UTF-16 byte order mark, body follows immediately.
        if (id == 0xFFFE)
            return 2;

        // 0x8950 — PNG signature with its first bytes rewritten. Walk chunks to
        // the tEXt payload; the keyword plus its null terminator is 8 bytes.
        if (id == 0x8950)
        {
            size_t offset = 8;

            while (offset + 8 <= size)
            {
                const juce::uint32 chunkLen = readBigEndianInt (data + offset);
                const juce::uint32 chunkId  = readBigEndianInt (data + offset + 4);

                if (chunkId == 0x74455874)          // 'tEXt'
                    return (int) (offset + 16);

                const size_t stride = (size_t) chunkLen + 12;

                if (stride == 0 || offset + stride <= offset)
                    break;

                offset += stride;
            }

            return -1;
        }

        // 0x424D — "BM": the BMP header's little-endian size field is where the
        // appended text begins.
        if (id == 0x424D && size >= 6)
        {
            const juce::uint32 len = (juce::uint32) data[2]
                                   | ((juce::uint32) data[3] << 8)
                                   | ((juce::uint32) data[4] << 16)
                                   | ((juce::uint32) data[5] << 24);

            return (len > 0 && len < size) ? (int) len : -1;
        }

        // 0x4B4D — "KM": KnobMan's own header, length stored byte-swapped.
        if (id == 0x4B4D && size >= 6)
        {
            const juce::uint32 raw = readBigEndianInt (data + 2);
            const juce::uint32 len = ((raw << 24) & 0xFF000000u) | ((raw << 8) & 0x00FF0000u)
                                   | ((raw >> 8) & 0x0000FF00u) | ((raw >> 24) & 0x000000FFu);

            return (len > 0 && len < size) ? (int) len : 0;
        }

        // Plain text.
        return 0;
    }

    //==========================================================================
    PrimType primTypeFromName (const juce::String& name)
    {
        // Note the hyphens: the file spells these "H-Lines" and "V-Lines",
        // which do not match our enum names.
        static const std::pair<const char*, PrimType> table[]
        {
            { "None",        PrimType::None },
            { "Image",       PrimType::Image },
            { "Circle",      PrimType::Circle },
            { "CircleFill",  PrimType::CircleFill },
            { "MetalCircle", PrimType::MetalCircle },
            { "WaveCircle",  PrimType::WaveCircle },
            { "Sphere",      PrimType::Sphere },
            { "Rect",        PrimType::Rect },
            { "RectFill",    PrimType::RectFill },
            { "Triangle",    PrimType::Triangle },
            { "Line",        PrimType::Line },
            { "RadiateLine", PrimType::RadiateLine },
            { "H-Lines",     PrimType::HLines },
            { "V-Lines",     PrimType::VLines },
            { "Text",        PrimType::Text },
            { "Shape",       PrimType::Shape },
        };

        for (const auto& entry : table)
            if (name == entry.first)
                return entry.second;

        return PrimType::None;
    }

    /** KnobMan stores "animate on/off" plus a custom curve index. We have five
        fixed curves, so anything animated becomes Linear. */
    AnimMode readAnimMode (const Section& s, const juce::String& enableKey)
    {
        return s.getInt (enableKey, 0) == 0 ? AnimMode::Fixed : AnimMode::Linear;
    }

    AnimVal readAnim (const Section& s,
                      const juce::String& fromKey, const juce::String& toKey,
                      const juce::String& enableKey, float fallback)
    {
        AnimVal v;
        v.from = s.getFloat (fromKey, fallback);
        v.to   = s.getFloat (toKey, v.from);
        v.mode = readAnimMode (s, enableKey);
        return v;
    }

    /** What a KnobMan layer's settings call for that Sonify's own drawing lacks. */
    struct Unmapped
    {
        bool texture = false, shadow = false, customCurve = false, intelliAlpha = false, frameMask = false;
    };

    /** The Sonify layer closest to a KnobMan layer (its section's keys). */
    DocLayer mapLayer (const Section& s, int index, Unmapped& flags)
    {
        DocLayer layer;
        layer.name    = s.getString ("Name", "Layer " + juce::String (index + 1));
        layer.visible = s.getInt ("Visible", 1) != 0;
        layer.solo    = s.getInt ("VisibleSolo", 0) != 0;

        // ---- primitive -----------------------------------------------------
        auto& p = layer.prim;

        p.type = primTypeFromName (s.getString ("Primitive", "None"));

        p.colour = juce::Colour ((juce::uint8) juce::jlimit (0, 255, s.getInt ("ColR", 255)),
                                 (juce::uint8) juce::jlimit (0, 255, s.getInt ("ColG", 0)),
                                 (juce::uint8) juce::jlimit (0, 255, s.getInt ("ColB", 0)));

        p.aspect        = s.getFloat ("PrimAspect", 0.0f);
        p.round         = s.getFloat ("PrimRound", 0.0f);
        p.width         = s.getFloat ("PrimWidth", 10.0f);
        p.length        = s.getFloat ("PrimLength", 50.0f);
        p.step          = juce::jlimit (1, 512, (int) s.getFloat ("PrimStep", 20.0f));
        p.angleStep     = s.getFloat ("PrimAngleStep", 45.0f);
        p.emboss        = s.getFloat ("PrimEmboss", 0.0f);
        p.embossDiffuse = s.getFloat ("PrimEmbossDiffuse", 0.0f);
        p.ambient       = s.getFloat ("PrimAmbient", 50.0f);
        p.specular      = s.getFloat ("PrimSpecular", 0.0f);
        p.specularWidth = s.getFloat ("PrimSpecWidth", 50.0f);
        p.diffuse       = s.getFloat ("PrimDiffuse", 0.0f);

        // KnobMan measures light anticlockwise from 3 o'clock and defaults to
        // -45; ours is clockwise from 12. Convert rather than copy.
        p.lightDir = 90.0f - s.getFloat ("PrimLightDir", -45.0f);

        p.text      = s.getString ("PrimText", "");
        p.fontSize  = s.getFloat ("PrimSize", 50.0f);
        p.bold      = s.getInt ("PrimBold", 0) != 0;
        p.italic    = s.getInt ("PrimItalic", 0) != 0;
        p.textAlign = s.getInt ("PrimTextAlign", 0);
        p.fill      = s.getInt ("PrimFill", 1) != 0;
        p.imageFile = s.getString ("PrimFile", "");

        // Shape points arrive as a flat list of numbers.
        {
            auto shapeText = s.getString ("PrimShape", "");

            if (shapeText.isNotEmpty())
            {
                juce::StringArray tokens;
                tokens.addTokens (shapeText.replaceCharacter (',', ' '), " ", {});
                tokens.removeEmptyStrings();

                for (int t = 0; t + 1 < tokens.size(); t += 2)
                    p.shapePoints.push_back ({ tokens[t].getFloatValue(),
                                               tokens[t + 1].getFloatValue() });
            }
        }

        // KnobMan's Emboss is a generic bevel for everything except the circle
        // family, which builds its rim analytically. Map it onto BevelSpec for
        // the former and leave the rim fields alone for the latter.
        {
            const bool circleFamily = p.type == PrimType::Circle
                                   || p.type == PrimType::CircleFill
                                   || p.type == PrimType::MetalCircle
                                   || p.type == PrimType::Sphere;

            // RectFill embosses itself too now, as KnobMan does (LayerRender's rasterisePrimitive).
            if (! circleFamily && p.type != PrimType::RectFill && std::abs (p.emboss) > 0.5f)
            {
                auto& b = p.bevel;
                b.enabled   = true;
                b.style     = BevelStyle::InnerBevel;
                b.technique = BevelTechnique::Smooth;
                b.up        = p.emboss >= 0.0f;
                b.size      = juce::jlimit (0.5f, 100.0f, std::abs (p.emboss) * 0.12f);
                b.depth     = juce::jlimit (1.0f, 1000.0f, std::abs (p.emboss) * 4.0f);
                b.useGlobalLight = false;
                b.angle     = p.lightDir;
                b.altitude  = 30.0f;
                b.highlightOpacity = juce::jlimit (0.0f, 100.0f, p.embossDiffuse);
                b.shadowOpacity    = juce::jlimit (0.0f, 100.0f, p.embossDiffuse);
            }
        }

        // KnobMan's texture is a bump map named by file. Map the four stock
        // names onto our generated equivalents; anything else keeps its name
        // and is looked for in the user's texture folder at render time.
        {
            const auto textureFile = s.getString ("PrimTextureFile", "");
            const float depth = s.getFloat ("PrimTexture", 0.0f);

            if (textureFile.isNotEmpty() && std::abs (depth) > 0.01f)
            {
                // Split the path FIRST. createLegalFileName strips separators,
                // so legalising before splitting would fuse the whole path into
                // one name and the lookup would never match.
                auto leaf = textureFile.replaceCharacter ('\\', '/')
                                       .fromLastOccurrenceOf ("/", false, false)
                                       .trim();

                auto bare = leaf.contains (".") ? leaf.upToLastOccurrenceOf (".", false, false)
                                                : leaf;

                for (const auto& known : TextureLibrary::proceduralNames())
                    if (bare.equalsIgnoreCase (known))
                        bare = known;

                p.textureName  = bare.isNotEmpty() ? bare : leaf;
                p.textureDepth = juce::jlimit (0.0f, 100.0f, std::abs (depth));
                p.textureMode  = TextureMode::Bump;

                if (! TextureLibrary::isProcedural (p.textureName))
                    flags.texture = true;
            }
        }

        if (s.getInt ("IntelliAlpha", 0) != 0)
            flags.intelliAlpha = true;

        // ---- effect --------------------------------------------------------
        auto& e = layer.eff;

        e.antialias    = s.getInt ("Antialias", 1) != 0;
        e.zoomSeparate = s.getInt ("ZoomXYSepa", 0) != 0;
        e.keepDir      = s.getInt ("KeepDir", 0) != 0;

        e.zoomX   = readAnim (s, "Zoom1", "Zoom2", "AnimateZoom", 100.0f);
        e.zoomY   = readAnim (s, "ZoomY1", "ZoomY2", "AnimateZoomY", 100.0f);
        e.offsetX = readAnim (s, "LayerOffsetX1", "LayerOffsetX2", "AnimateLayerOffsetX", 0.0f);
        e.offsetY = readAnim (s, "LayerOffsetY1", "LayerOffsetY2", "AnimateLayerOffsetY", 0.0f);
        e.angle   = readAnim (s, "Angle1", "Angle2", "AnimateAngle", 0.0f);
        e.alpha   = readAnim (s, "Alpha1", "Alpha2", "AnimateAlpha", 100.0f);

        e.brightness = readAnim (s, "Brightness1", "Brightness2", "AnimateBrightness", 0.0f);
        e.contrast   = readAnim (s, "Contrast1", "Contrast2", "AnimateContrast", 0.0f);
        e.saturation = readAnim (s, "Saturation1", "Saturation2", "AnimateSaturation", 0.0f);
        e.hue        = readAnim (s, "Hue1", "Hue2", "AnimateHue", 0.0f);

        // KnobMan's rotation centre is an offset from the middle of the canvas;
        // ours is a percentage across it.
        e.centreX = juce::jlimit (0.0f, 100.0f, 50.0f + s.getFloat ("RotCenterX1", 0.0f) * 0.5f);
        e.centreY = juce::jlimit (0.0f, 100.0f, 50.0f + s.getFloat ("RotCenterY1", 0.0f) * 0.5f);

        auto readMask = [&s] (MaskSpec& m,
                              const juce::String& enable, const juce::String& type,
                              const juce::String& start1, const juce::String& start2,
                              const juce::String& startAnim,
                              const juce::String& stop1, const juce::String& stop2,
                              const juce::String& stopAnim)
        {
            m.enabled = s.getInt (enable, 0) != 0;
            m.type    = (MaskType) juce::jlimit (0, (int) MaskType::numTypes - 1,
                                                 s.getInt (type, 0));
            m.start   = readAnim (s, start1, start2, startAnim, -140.0f);
            m.stop    = readAnim (s, stop1,  stop2,  stopAnim,   140.0f);
        };

        readMask (e.mask1, "UseMask", "MaskType",
                  "MaskStart1", "MaskStart2", "AnimateMaskStart",
                  "MaskStop1",  "MaskStop2",  "AnimateMaskStop");

        readMask (e.mask2, "UseMask2", "Mask2Type",
                  "Mask2Start1", "Mask2Start2", "AnimateMask2Start",
                  "Mask2Stop1",  "Mask2Stop2",  "AnimateMask2Stop");

        e.mask2.orOp  = s.getInt ("Mask2Operation", 0) != 0;
        e.mask1.biDir = s.getInt ("MaskGradDir", 0) != 0;
        e.mask2.biDir = s.getInt ("Mask2GradDir", 0) != 0;

        if (s.getFloat ("ShadowDensity1", 0.0f) != 0.0f
            || s.getFloat ("IShadowDensity1", 0.0f) != 0.0f
            || s.getFloat ("HilightDensity1", 0.0f) != 0.0f)
            flags.shadow = true;

        if (s.getInt ("UseFMask", 0) != 0)
            flags.frameMask = true;

        if (s.getInt ("AnimateZoom", 0) != 0 && s.has ("ZoomCurve"))
            flags.customCurve = true;

        return layer;
    }
}

//==============================================================================
juce::String KnobFile::extractBody (const juce::MemoryBlock& raw)
{
    const auto* data = static_cast<const juce::uint8*> (raw.getData());

    // UTF-16 text after its byte order mark (FF FE: little-endian, FE FF: big-endian): two
    // bytes a character, so it can't be read as UTF-8 (its first zero byte would end it).
    if (raw.getSize() >= 2 && ((data[0] == 0xFF && data[1] == 0xFE) || (data[0] == 0xFE && data[1] == 0xFF)))
    {
        const bool littleEndian = data[0] == 0xFF;
        const size_t units = (raw.getSize() - 2) / 2;

        // Two zeros after it: a lone surrogate at the end can't read past them.
        std::vector<juce::CharPointer_UTF16::CharType> text (units + 2, 0);

        for (size_t i = 0; i < units; ++i)
        {
            const auto* p = data + 2 + i * 2;
            text[i] = (juce::CharPointer_UTF16::CharType) (littleEndian ? (p[0] | (p[1] << 8)) : ((p[0] << 8) | p[1]));
        }

        return juce::String (juce::CharPointer_UTF16 (text.data()));
    }

    const int offset = findBodyOffset (data, raw.getSize());

    if (offset < 0 || (size_t) offset >= raw.getSize())
        return {};

    // KnobMan's own "KM" header: the text after the preview is UTF-16 (little-endian, no byte
    // order mark) when its second byte is zero, as KnobMan reads it; otherwise UTF-8.
    const auto* body = data + offset;
    const auto length = raw.getSize() - (size_t) offset;

    if (length >= 4 && body[0] != 0 && body[1] == 0)
    {
        const size_t units = length / 2;
        std::vector<juce::CharPointer_UTF16::CharType> text (units + 2, 0);

        for (size_t i = 0; i < units; ++i)
            text[i] = (juce::CharPointer_UTF16::CharType) (body[i * 2] | (body[i * 2 + 1] << 8));

        return juce::String (juce::CharPointer_UTF16 (text.data()));
    }

    return juce::String::fromUTF8 (reinterpret_cast<const char*> (body), (int) length);
}

//==============================================================================
juce::Image KnobFile::extractPreview (const juce::MemoryBlock& raw)
{
    if (raw.getSize() < 8)
        return {};

    const auto* data = static_cast<const juce::uint8*> (raw.getData());
    static const juce::uint8 signature[] { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };

    // KnobMan's own "KM" header: the preview PNG with its first eight bytes replaced by "KM",
    // its length (little-endian) and two more. The document text follows it.
    if (data[0] == 'K' && data[1] == 'M')
    {
        const auto length = (size_t) data[2] | ((size_t) data[3] << 8) | ((size_t) data[4] << 16) | ((size_t) data[5] << 24);

        if (length <= 8 || length > raw.getSize())
            return {};

        juce::MemoryBlock png (data, length);
        png.copyFrom (signature, 0, sizeof (signature));
        return juce::ImageFileFormat::loadFrom (png.getData(), png.getSize());
    }

    if (! (data[0] == 0x89 && data[1] == 0x50))
        return {};

    // Put the real PNG signature back and decode normally.
    juce::MemoryBlock png (raw);
    png.copyFrom (signature, 0, sizeof (signature));

    return juce::ImageFileFormat::loadFrom (png.getData(), png.getSize());
}

//==============================================================================
KnobFile::ImportResult KnobFile::parse (const juce::MemoryBlock& raw)
{
    ImportResult result;

    if (raw.getSize() < 16)
    {
        result.error = "File is too small to be a knob document.";
        return result;
    }

    auto body = extractBody (raw);

    if (body.isEmpty())
    {
        result.error = "Could not find the document text inside the file.";
        return result;
    }

    IniBody ini (body);

    if (! ini.hasSection ("Prefs"))
    {
        result.error = "No [Prefs] section — this does not look like a knob document.";
        return result;
    }

    const auto& prefs = ini.getSection ("Prefs");

    LayerDoc doc;
    doc.layers.clear();

    doc.canvasWidth  = juce::jlimit (8, 2048, prefs.getInt ("OutputSizeX", 32));
    doc.canvasHeight = juce::jlimit (8, 2048, prefs.getInt ("OutputSizeY", 32));

    int frames = prefs.getInt ("NumOfImage", 0);

    if (frames <= 0)
        frames = prefs.getInt ("RenderFrames", 31);

    doc.frames = juce::jlimit (1, 512, frames);

    if (prefs.getInt ("OverSampling", 0) != 0)
        result.warnings.add ("Oversampling is set in the source file; we always render 1:1.");

    if (prefs.getInt ("AlignHorizontal", 0) != 0)
        result.warnings.add ("Source strip is horizontal; export defaults to vertical.");

    const int layerCount = juce::jlimit (0, 256, prefs.getInt ("Layers", 1));

    Unmapped unmapped;

    for (int i = 0; i < layerCount; ++i)
    {
        const juce::String name = "Layer" + juce::String (i + 1);

        if (! ini.hasSection (name))
            continue;

        const auto& s = ini.getSection (name);
        doc.layers.push_back (mapLayer (s, i, unmapped));
    }

    if (doc.layers.empty())
    {
        result.error = "The document contains no layers.";
        return result;
    }

    // Drawn exactly as KnobMan draws it; its layers, for editing in Knob Studio, read from the
    // KnobMan document itself (each key as KnobMan resolves it).
    juce::String knobManError;
    doc.knobman = KnobManDoc::parse (body, knobManError);

    if (doc.knobman != nullptr)
    {
        doc.layers = layersOf (*doc.knobman);
        doc.canvasWidth  = juce::jlimit (8, 2048, doc.knobman->width);
        doc.canvasHeight = juce::jlimit (8, 2048, doc.knobman->height);
    }

    if (doc.knobman == nullptr)
    {
        if (unmapped.texture)
            result.warnings.add ("Uses a texture image we do not ship. Drop a matching file into "
                                 "the texture folder, or pick one of the built-in patterns.");

        if (unmapped.shadow)
            result.warnings.add ("Drop, inner and highlight shadows are not implemented; those layers lose depth.");

        if (unmapped.customCurve)
            result.warnings.add ("Custom animation curves became Linear.");

        if (unmapped.intelliAlpha)
            result.warnings.add ("IntelliAlpha edge handling is not implemented.");

        if (unmapped.frameMask)
            result.warnings.add ("Per-frame masks (FMask) are not implemented.");
    }
    else
    {
        for (const auto& note : doc.knobman->notes)
            result.warnings.add (note);
    }

    result.document = doc;
    result.preview  = extractPreview (raw);
    result.ok       = true;
    return result;
}

//==============================================================================
std::vector<DocLayer> KnobFile::layersOf (const KnobManDoc& knobman)
{
    std::vector<DocLayer> layers;
    Unmapped unmapped;

    for (int i = 0; i < (int) knobman.layers.size(); ++i)
    {
        IniBody ini (knobman.layerText (i));
        auto layer = mapLayer (ini.getSection ("Layer"), i, unmapped);
        layer.knobmanLayer = i;
        layers.push_back (layer);
    }

    return layers;
}

namespace
{
    juce::String numberText (double v)
    {
        return juce::String (v, 6).trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
    }

    /** The KnobMan keys for what changed from before to now (Sonify's settings that KnobMan has). */
    void keysFor (const DocLayer& now, const DocLayer& before, std::vector<std::pair<juce::String, juce::String>>& keys)
    {
        const auto set = [&keys] (const juce::String& key, const juce::String& value) { keys.emplace_back (key, value); };
        const auto real = [&set] (const juce::String& key, float a, float b) { if (! juce::exactlyEqual (a, b)) set (key, numberText (a)); };
        const auto flag = [&set] (const juce::String& key, bool a, bool b) { if (a != b) set (key, a ? "1" : "0"); };
        const auto whole = [&set] (const juce::String& key, int a, int b) { if (a != b) set (key, juce::String (a)); };

        // An animated value: its two ends, and on or off (a KnobMan curve stays as it was while it's on).
        const auto anim = [&] (const juce::String& from, const juce::String& to, const juce::String& animate, const juce::String& curve,
                               const AnimVal& a, const AnimVal& b)
        {
            real (from, a.from, b.from);
            real (to, a.to, b.to);

            if (a.mode != b.mode)
            {
                if (a.mode == AnimMode::Fixed)
                {
                    set (animate, "0");
                }
                else if (a.mode == AnimMode::Linear)
                {
                    set (animate, "1");
                    set (curve, "0");
                }
            }
        };

        if (now.name != before.name)
            set ("Name", now.name);

        flag ("Visible", now.visible, before.visible);
        flag ("VisibleSolo", now.solo, before.solo);

        const auto& p = now.prim;
        const auto& q = before.prim;

        if (p.type != q.type && (int) p.type <= (int) PrimType::Shape)
        {
            static const char* const names[] { "None", "Image", "Circle", "CircleFill", "MetalCircle", "WaveCircle", "Sphere", "Rect",
                                               "RectFill", "Triangle", "Line", "RadiateLine", "H-Lines", "V-Lines", "Text", "Shape" };
            set ("Primitive", names[(int) p.type]);
        }

        if (p.colour.getRed() != q.colour.getRed() || p.colour.getGreen() != q.colour.getGreen() || p.colour.getBlue() != q.colour.getBlue())
        {
            set ("ColR", juce::String (p.colour.getRed()));
            set ("ColG", juce::String (p.colour.getGreen()));
            set ("ColB", juce::String (p.colour.getBlue()));
        }

        real ("PrimAspect", p.aspect, q.aspect);
        real ("PrimRound", p.round, q.round);
        real ("PrimWidth", p.width, q.width);
        real ("PrimLength", p.length, q.length);
        whole ("PrimStep", p.step, q.step);
        real ("PrimAngleStep", p.angleStep, q.angleStep);
        real ("PrimEmboss", p.emboss, q.emboss);
        real ("PrimEmbossDiffuse", p.embossDiffuse, q.embossDiffuse);
        real ("PrimAmbient", p.ambient, q.ambient);
        real ("PrimSpecular", p.specular, q.specular);
        real ("PrimSpecWidth", p.specularWidth, q.specularWidth);
        real ("PrimDiffuse", p.diffuse, q.diffuse);

        if (! juce::exactlyEqual (p.lightDir, q.lightDir))
            set ("PrimLightDir", numberText (90.0 - (double) p.lightDir));   // Sonify's 90 - KnobMan's

        if (p.text != q.text)
            set ("PrimText", p.text);

        real ("PrimSize", p.fontSize, q.fontSize);
        flag ("PrimBold", p.bold, q.bold);
        flag ("PrimItalic", p.italic, q.italic);
        whole ("PrimTextAlign", p.textAlign, q.textAlign);
        flag ("PrimFill", p.fill, q.fill);

        if (p.textureMode == TextureMode::Bump && q.textureMode == TextureMode::Bump)
            real ("PrimTexture", p.textureDepth, q.textureDepth);

        const auto& e = now.eff;
        const auto& f = before.eff;
        flag ("Antialias", e.antialias, f.antialias);
        flag ("ZoomXYSepa", e.zoomSeparate, f.zoomSeparate);
        flag ("KeepDir", e.keepDir, f.keepDir);
        anim ("Zoom1", "Zoom2", "AnimateZoom", "ZoomCurve", e.zoomX, f.zoomX);
        anim ("ZoomY1", "ZoomY2", "AnimateZoomY", "ZoomYCurve", e.zoomY, f.zoomY);
        anim ("LayerOffsetX1", "LayerOffsetX2", "AnimateLayerOffsetX", "LayerOffsetXCurve", e.offsetX, f.offsetX);
        anim ("LayerOffsetY1", "LayerOffsetY2", "AnimateLayerOffsetY", "LayerOffsetYCurve", e.offsetY, f.offsetY);
        anim ("Angle1", "Angle2", "AnimateAngle", "AngleCurve", e.angle, f.angle);
        anim ("Alpha1", "Alpha2", "AnimateAlpha", "AlphaCurve", e.alpha, f.alpha);
        anim ("Brightness1", "Brightness2", "AnimateBrightness", "BrightnessCurve", e.brightness, f.brightness);
        anim ("Contrast1", "Contrast2", "AnimateContrast", "ContrastCurve", e.contrast, f.contrast);
        anim ("Saturation1", "Saturation2", "AnimateSaturation", "SaturationCurve", e.saturation, f.saturation);
        anim ("Hue1", "Hue2", "AnimateHue", "HueCurve", e.hue, f.hue);

        // Sonify's centre is a percentage across; KnobMan's an offset from the middle.
        if (! juce::exactlyEqual (e.centreX, f.centreX))
            set ("RotCenterX1", numberText (((double) e.centreX - 50.0) * 2.0));

        if (! juce::exactlyEqual (e.centreY, f.centreY))
            set ("RotCenterY1", numberText (((double) e.centreY - 50.0) * 2.0));

        flag ("UseMask", e.mask1.enabled, f.mask1.enabled);
        whole ("MaskType", (int) e.mask1.type, (int) f.mask1.type);
        anim ("MaskStart1", "MaskStart2", "AnimateMaskStart", "MaskStartCurve", e.mask1.start, f.mask1.start);
        anim ("MaskStop1", "MaskStop2", "AnimateMaskStop", "MaskStopCurve", e.mask1.stop, f.mask1.stop);
        flag ("MaskGradDir", e.mask1.biDir, f.mask1.biDir);
        flag ("UseMask2", e.mask2.enabled, f.mask2.enabled);
        whole ("Mask2Type", (int) e.mask2.type, (int) f.mask2.type);
        anim ("Mask2Start1", "Mask2Start2", "AnimateMask2Start", "Mask2StartCurve", e.mask2.start, f.mask2.start);
        anim ("Mask2Stop1", "Mask2Stop2", "AnimateMask2Stop", "Mask2StopCurve", e.mask2.stop, f.mask2.stop);
        flag ("Mask2GradDir", e.mask2.biDir, f.mask2.biDir);
        flag ("Mask2Operation", e.mask2.orOp, f.mask2.orOp);
    }

    /** A layer's settings as a tree (as a look file keeps them). */
    juce::ValueTree treeOf (const DocLayer& layer)
    {
        LayerDoc one;
        one.layers = { layer };
        one.layers[0].knobmanLayer = -1;
        return one.toValueTree().getChildWithName ("Layer");
    }

    bool sameValue (const juce::var& a, const juce::var& b)
    {
        if (a.isDouble() || b.isDouble() || a.isInt() || b.isInt() || a.isInt64() || b.isInt64())
        {
            const auto x = (double) a, y = (double) b;
            return std::abs (x - y) <= 1.0e-3 * juce::jmax (1.0, std::abs (x), std::abs (y));
        }

        return a == b;
    }

    /** Where two layers' trees differ ("Primitive/bevel/size", say). */
    void differences (const juce::ValueTree& a, const juce::ValueTree& b, const juce::String& path, std::set<juce::String>& out)
    {
        for (int i = 0; i < a.getNumProperties(); ++i)
        {
            const auto name = a.getPropertyName (i);

            if (! b.hasProperty (name) || ! sameValue (a[name], b[name]))
                out.insert (path + name.toString());
        }

        for (int i = 0; i < b.getNumProperties(); ++i)
            if (! a.hasProperty (b.getPropertyName (i)))
                out.insert (path + b.getPropertyName (i).toString());

        for (int i = 0; i < juce::jmax (a.getNumChildren(), b.getNumChildren()); ++i)
            differences (a.getChild (i), b.getChild (i), path + a.getChild (i).getType().toString() + b.getChild (i).getType().toString() + "/", out);
    }

    /** A setting's name, for saying what didn't stick. */
    juce::String settingName (const juce::String& path)
    {
        const auto p = path.toLowerCase();

        if (p.contains ("bev"))                           return "Bevel & Emboss";
        if (p.contains ("orop"))                          return "first mask's Or";
        if (p.contains ("inner"))                         return "inner shadow";
        if (p.contains ("glow"))                          return "outer glow";
        if (p.contains ("shad"))                          return "drop shadow";
        if (p.contains ("stroke"))                        return "outline";
        if (p.contains ("material") || p.contains ("pattern")) return "material";
        if (p.contains ("tex"))                           return "texture";
        if (p.contains ("led"))                           return "LED";
        if (p.contains ("meter"))                         return "scale";
        if (p.contains ("ray"))                           return "ray shape";
        if (p.contains ("gloss"))                         return "gloss";
        if (p.contains ("mode"))                          return "animation curve";
        if (p.contains ("image"))                         return "picture";
        if (p.contains ("shape"))                         return "shape";
        if (p.contains ("type"))                          return "shape type";
        if (p.contains ("blend"))                         return "blend mode";
        return path.fromLastOccurrenceOf ("/", false, false);
    }
}

LayerDoc KnobFile::applyEdits (const LayerDoc& edited, juce::StringArray& undone)
{
    if (edited.knobman == nullptr)
        return edited;

    const auto& knobman = *edited.knobman;
    const auto before = layersOf (knobman);

    // A new layer starts from KnobMan's defaults (an empty section, as KnobMan reads one).
    Unmapped unmapped;
    IniBody empty ("[Layer]\n");
    const auto fresh = mapLayer (empty.getSection ("Layer"), 0, unmapped);

    std::vector<KnobManDoc::LayerEdit> edits;
    std::vector<const DocLayer*> baselines;

    for (const auto& layer : edited.layers)
    {
        KnobManDoc::LayerEdit edit;
        const bool known = juce::isPositiveAndBelow (layer.knobmanLayer, (int) before.size());
        edit.source = known ? layer.knobmanLayer : -1;
        const auto& base = known ? before[(size_t) layer.knobmanLayer] : fresh;
        keysFor (layer, base, edit.values);
        edits.push_back (std::move (edit));
        baselines.push_back (&base);
    }

    auto result = edited;
    result.knobman = knobman.edited (edits, edited.canvasWidth, edited.canvasHeight, edited.frames);
    result.layers = layersOf (*result.knobman);
    result.canvasWidth  = juce::jlimit (8, 2048, result.knobman->width);
    result.canvasHeight = juce::jlimit (8, 2048, result.knobman->height);

    // What was changed but didn't come back as it was changed: KnobMan has no setting for it.
    std::set<juce::String> missed;

    for (size_t i = 0; i < edited.layers.size() && i < result.layers.size(); ++i)
    {
        const auto was = treeOf (*baselines[i]), asked = treeOf (edited.layers[i]), got = treeOf (result.layers[i]);
        std::set<juce::String> changed, kept;
        differences (was, asked, {}, changed);
        differences (asked, got, {}, kept);

        for (const auto& path : changed)
            if (kept.count (path) > 0)
                missed.insert (settingName (path));
    }

    for (const auto& name : missed)
        undone.addIfNotAlreadyThere (name);

    return result;
}

//==============================================================================
KnobFile::ImportResult KnobFile::parseFile (const juce::File& file)
{
    ImportResult result;

    if (! file.existsAsFile())
    {
        result.error = "File not found.";
        return result;
    }

    juce::MemoryBlock raw;

    if (! file.loadFileAsData (raw))
    {
        result.error = "Could not read the file.";
        return result;
    }

    return parse (raw);
}
