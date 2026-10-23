// D:\workspace\Fanan_web_hub\src\stella_builtin_paint.cpp
//
// PanelPainter: a built-in painter for paint_picture (part of the elements' runtime, in every
// build): a whole panel background, lit by one light the way the knobs' looks are, from its
// settings alone (stella_paint.h does the painting):
//
//   material   powder, aluminium, steel, silver, chrome, gold, rosegold, brass, copper, bronze,
//              titanium (liquid), gunmetal, iron, hammertone, anodized, carbon, marble,
//              blackmarble, granite, slate, concrete, terrazzo, sandstone, plastic,
//              matteplastic, rubber, bakelite, ceramic, wood (walnut, oak, rosewood, ebony,
//              whitewood, orangewood, bordowood, greywood), burl, guitarwood (flamed maple), leather, tolex, felt, paper, glass, frosted, pearl, smooth
//   color, color2         its colours (paint_picture's "color" is the main one; color2: marble's
//                         veins, granite's flecks, wood's dark grain...)
//   finish                natural, polished, satin, brushed, hammered, sandblasted, matte, liquid
//   scale, angle, grain   the texture's size, direction (degrees) and strength (0..2)
//   shine, flow           0 matte .. 1 mirror; liquid swirls 0..1 (titanium has them)
//   seed                  another grain
//   pattern               a pattern layer over it: a name with patternColor, patternOpacity,
//                         patternBlend, patternSize, patternAngle; or "patterns": a list of
//                         { "pattern", "color", "opacity", "blend", "size", "angle", "width",
//                         "depth", "x", "y", "count", "invert" } layers, bottom first
//   light, altitude       where the light comes from (degrees, 120: the upper left, as the
//                         looks), and how high (10..80, default 35: low is dramatic)
//   key, keyX, keyY       the key light's pool (0..1, default 0.6) and where it falls (0..1)
//   vignette              darker edges (0..1, default 0.45)
//   rim, corner           the panel's own bevelled edge (px), and its corner radius
//   screws                1: a screw in each corner of the window
//   parts     [{ "kind": recess|raise|plate|screen|inlay|light|well, "x", "y", "w", "h",
//                "shape": "circle", "radius", "depth", "strength", "screws", "glow",
//                and a surface of its own: "material", "color", "finish", "pattern"... }]
//              (the older "sections": "x,y,w,h;..." are recesses)
//   lines     [{ "x1", "y1", "x2", "y2", "width", "kind": groove|ridge }]
//   vents     [{ "x", "y", "w", "h", "slots", "across" }]
//   grilles   [{ "x", "y", "w", "h", "spacing", "size" }]
//   texts     [{ "x", "y", "w", "h", "text", "size", "bold", "style": engraved|embossed|printed,
//                "color", "align": left|centre|right }]

#include "stella_elements.h"
#include "stella_paint.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace stella::ui::builtin
{
    class PanelPainter final : public Element
    {
    public:
        void resized() override
        {
            namespace p = stella::paint;

            // The panel's own surface, from its settings.
            p::Json look;
            look.type = p::Json::Type::object;

            for (const auto* key : { "material", "color", "colour", "color2", "colour2", "finish", "scale", "angle", "grain", "shine", "flow",
                                     "seed", "pattern", "patterns", "patternColor", "patternColour", "patternOpacity", "patternBlend",
                                     "patternSize", "patternScale", "patternAngle", "patternWidth", "patternDepth" })
            {
                const auto text = settingText (key, "");

                if (text.empty())
                    continue;

                look.fields.push_back ({ key, text[0] == '[' || text[0] == '{' ? p::Json::parse (text) : textValue (text) });
            }

            panel = {};
            panel.surfacing = p::surfacingFrom (look, p::Surfacing::of (p::Material::powder, (int) setting ("seed", 1.0f)));

            if (const auto given = colour (0u); given != 0u)
                panel.surfacing.colour = p::rgbOf (given);

            panel.light = p::Light::from (setting ("light", 120.0f), setting ("altitude", 35.0f));
            panel.key = setting ("key", 0.6f);
            panel.keyX = setting ("keyX", -1.0f);
            panel.keyY = setting ("keyY", -1.0f);
            panel.vignette = setting ("vignette", 0.45f);
            rim = setting ("rim", 0.0f);
            corner = setting ("corner", 0.0f);
            screws = setting ("screws", 0.0f) > 0.5f;

            parts.clear();
            lines = p::Json::parse (settingText ("lines", "[]"));
            vents = p::Json::parse (settingText ("vents", "[]"));
            grilles = p::Json::parse (settingText ("grilles", "[]"));
            texts = p::Json::parse (settingText ("texts", "[]"));

            for (const auto* name : { "parts", "sections" })
            {
                const auto text = settingText (name, "");

                if (text.empty())
                    continue;

                if (text[0] == '[' || text[0] == '{')
                {
                    const auto list = p::Json::parse (text);

                    if (list.type == p::Json::Type::object)
                        addPart (list);

                    for (const auto& item : list.items)
                        addPart (item);
                }
                else
                {
                    addOldSections (text);
                }
            }
        }

        void paint (Canvas& g) override
        {
            namespace p = stella::paint;

            surface.resize (width(), height());
            panel.setSize (width(), height());
            p::paintBase (surface, panel);

            // Lights first (they fall on the panel), then the parts in their order.
            for (const auto& part : parts)
                if (part.kind == p::PartKind::light)
                    p::paintPart (surface, part, panel);

            for (const auto& part : parts)
                if (part.kind != p::PartKind::light)
                    p::paintPart (surface, part, panel);

            for (const auto& l : lines.items)
                p::paintLine (surface, l.num ("x1", 0.0f), l.num ("y1", 0.0f), l.num ("x2", 0.0f), l.num ("y2", 0.0f),
                              l.num ("width", 2.0f), l.str ("kind", "groove") == "ridge", panel.light);

            for (const auto& v : vents.items)
                p::paintSlots (surface, v.num ("x", 0.0f), v.num ("y", 0.0f), v.num ("w", 60.0f), v.num ("h", 30.0f),
                               (int) v.num ("slots", 6.0f), v.flag ("across"), panel);

            for (const auto& h : grilles.items)
                p::paintHoles (surface, h.num ("x", 0.0f), h.num ("y", 0.0f), h.num ("w", 80.0f), h.num ("h", 80.0f),
                               h.num ("spacing", 7.0f), h.num ("size", 3.5f), panel.light);

            p::paintRim (surface, rim, corner, panel.light);

            if (screws)
            {
                const float r = 5.0f, m = 14.0f, w = (float) width(), h = (float) height();
                p::paintScrew (surface, m, m, r, 0.6f, panel.light);
                p::paintScrew (surface, w - m, m, r, 2.1f, panel.light);
                p::paintScrew (surface, m, h - m, r, 1.2f, panel.light);
                p::paintScrew (surface, w - m, h - m, r, 2.7f, panel.light);
            }

            surface.write (g);
            paintTexts (g);
        }

    private:
        /** A setting's text as a value: a number when it reads as one, otherwise words. */
        static stella::paint::Json textValue (const std::string& text)
        {
            stella::paint::Json j;
            const char* end = nullptr;
            const double d = stella::paint::numberIn (text.c_str(), &end);   // not strtod: it follows the host's locale

            if (end != text.c_str() && *end == 0)
            {
                j.type = stella::paint::Json::Type::number;
                j.number = d;
            }
            else
            {
                j.type = stella::paint::Json::Type::text;
                j.text = text;
            }

            return j;
        }

        void addPart (const stella::paint::Json& j)
        {
            namespace p = stella::paint;

            if (j.type != p::Json::Type::object || parts.size() >= 64)
                return;

            p::Part part;
            part.kind = p::partKindNamed (j.str ("kind", "recess"));
            const float x = j.num ("x", 0.0f), y = j.num ("y", 0.0f), w = j.num ("w", 40.0f), h = j.num ("h", 40.0f);
            const bool round = j.str ("shape") == "circle" || part.kind == p::PartKind::well;

            part.shape = round ? p::Shape::circle (x + w * 0.5f, y + h * 0.5f, std::fmin (w, h) * 0.5f)
                               : p::Shape::box (x, y, w, h, j.num ("radius", part.kind == p::PartKind::screen ? 6.0f : 8.0f));

            const float defaultDepth = part.kind == p::PartKind::raise ? 5.0f : (part.kind == p::PartKind::plate || part.kind == p::PartKind::well ? 3.0f : 4.0f);
            part.depth = j.num ("depth", defaultDepth);
            part.strength = j.num ("strength", part.kind == p::PartKind::light ? 0.6f : 1.0f);
            part.screws = j.flag ("screws");

            const auto colourText = j.str ("color", j.str ("colour"));

            if (part.kind == p::PartKind::light || part.kind == p::PartKind::screen)
            {
                // Its colour is its light's, or its glass's.
                if (! colourText.empty())
                {
                    part.colour = p::colourNamed (colourText, { 1.0f, 1.0f, 1.0f });
                    part.hasColour = true;
                }
            }
            else
            {
                // Its own surface when it says anything about one (a plate always has its own).
                bool own = part.kind == p::PartKind::plate;

                for (const auto* key : { "material", "color", "colour", "color2", "finish", "pattern", "patterns", "shine", "grain", "scale", "angle", "flow" })
                    own = own || j.has (key);

                if (own)
                {
                    const auto fallback = part.kind == p::PartKind::plate && ! j.has ("material") ? p::Surfacing::of (p::Material::aluminium, panel.surfacing.seed + 7)
                                                                                                  : panel.surfacing;
                    part.surfacing = p::surfacingFrom (j, fallback);
                    part.ownSurface = true;
                }
            }

            if (const auto glow = j.str ("glow"); ! glow.empty())
            {
                part.glow = p::colourNamed (glow, { 0.3f, 1.0f, 0.6f });
                part.glowAmount = 1.0f;
            }

            parts.push_back (part);
        }

        /** "x,y,w,h;x,y,w,h": recessed sections, as the first painter took them. */
        void addOldSections (const std::string& text)
        {
            namespace p = stella::paint;
            std::size_t start = 0;

            while (start < text.size() && parts.size() < 64)
            {
                const auto end = text.find (';', start);
                const auto piece = text.substr (start, end == std::string::npos ? std::string::npos : end - start);
                float v[4] {};
                int n = 0;
                const char* at = piece.c_str();

                while (n < 4 && *at != 0)
                {
                    const char* next = nullptr;
                    v[n] = (float) stella::paint::numberIn (at, &next);   // not strtof: it follows the host's locale

                    if (next == at)
                        break;

                    ++n;
                    at = *next == ',' ? next + 1 : next;
                }

                if (n == 4)
                {
                    p::Part part;
                    part.shape = p::Shape::box (v[0], v[1], v[2], v[3], 8.0f);
                    parts.push_back (part);
                }

                if (end == std::string::npos)
                    break;

                start = end + 1;
            }
        }

        /** Words on the panel, cut in (engraved), standing out (embossed) or printed, lit from
            the same place as everything else. */
        void paintTexts (Canvas& g)
        {
            namespace p = stella::paint;

            for (const auto& t : texts.items)
            {
                const auto words = t.str ("text");

                if (words.empty())
                    continue;

                const float x = t.num ("x", 0.0f), y = t.num ("y", 0.0f), w = t.num ("w", 200.0f), h = t.num ("h", 24.0f);
                const float size = t.num ("size", 14.0f);
                const bool bold = t.flag ("bold");
                const auto alignText = t.str ("align", "left");
                const auto align = alignText == "right" ? Align::right : (alignText == "centre" || alignText == "center" ? Align::centre : Align::left);
                const auto style = t.str ("style", "engraved");
                const auto ink = p::colourOf (p::colourNamed (t.str ("color", t.str ("colour")), { 0.10f, 0.11f, 0.12f }));
                const float ax = -panel.light.tx, ay = -panel.light.ty;   // away from the light

                if (style == "embossed")
                {
                    g.drawText (words, x + ax, y + ay, w, h, size, 0x66000000, align, bold);
                    g.drawText (words, x - ax * 0.7f, y - ay * 0.7f, w, h, size, 0x40ffffff, align, bold);
                }
                else if (style != "printed")
                {
                    g.drawText (words, x + ax, y + ay, w, h, size, 0x38ffffff, align, bold);   // the far edge of the cut catches the light
                }

                g.drawText (words, x, y, w, h, size, ink, align, bold);
            }
        }

        stella::paint::Panel panel;
        float rim = 0.0f, corner = 0.0f;
        bool screws = false;
        std::vector<stella::paint::Part> parts;
        stella::paint::Json lines, vents, grilles, texts;
        stella::paint::Surface surface;
    };

    STELLA_BUILTIN_ELEMENT (PanelPainter, "Painter for paint_picture: a whole panel background lit by one light, from settings (material in any "
                                          "colour and finish, pattern layers, light, key, vignette, rim, screws, parts: recess, raise, plate, "
                                          "screen, inlay, light, well; lines, vents, grilles, texts). Nothing to write or build.")
}
