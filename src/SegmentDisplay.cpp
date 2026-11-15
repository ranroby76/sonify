// D:\workspace\Fanan_web_hub\src\SegmentDisplay.cpp

#include "SegmentDisplay.h"

#include <cmath>

namespace
{
    // Every segment a bit: the top and the bottom in halves (16), the middle in halves, the
    // diagonals and the middle uprights (14 and 16).
    enum : int { a1 = 1, a2 = 2, sb = 4, sc = 8, d1 = 16, d2 = 32, se = 64, sf = 128, g1 = 256, g2 = 512,
                 sh = 1024, si = 2048, sj = 4096, sk = 8192, sl = 16384, sm = 32768 };
    constexpr int sa = a1 | a2, sd = d1 | d2, sg = g1 | g2;

    /** A character on 16 (or 14) segments. */
    int sixteenOf (juce::juce_wchar c)
    {
        switch (juce::CharacterFunctions::toUpperCase (c))
        {
            case '0': return sa | sb | sc | sd | se | sf | sj | sk;
            case '1': return sb | sc | sj;
            case '2': return sa | sb | sg | se | sd;
            case '3': return sa | sb | sc | sd | g2 | g1;
            case '4': return sf | sg | sb | sc;
            case '5': return sa | sf | sg | sc | sd;
            case '6': return sa | sf | se | sd | sc | sg;
            case '7': return sa | sb | sc;
            case '8': return sa | sb | sc | sd | se | sf | sg;
            case '9': return sa | sb | sc | sd | sf | sg;
            case 'A': return sa | sb | sc | se | sf | sg;
            case 'B': return sa | sb | sc | sd | g2 | si | sl;
            case 'C': return sa | sd | se | sf;
            case 'D': return sa | sb | sc | sd | si | sl;
            case 'E': return sa | sd | se | sf | sg;
            case 'F': return sa | se | sf | g1;
            case 'G': return sa | sc | sd | se | sf | g2;
            case 'H': return sb | sc | se | sf | sg;
            case 'I': return sa | sd | si | sl;
            case 'J': return sb | sc | sd | se;
            case 'K': return se | sf | g1 | sj | sm;
            case 'L': return sd | se | sf;
            case 'M': return sb | sc | se | sf | sh | sj;
            case 'N': return sb | sc | se | sf | sh | sm;
            case 'O': return sa | sb | sc | sd | se | sf;
            case 'P': return sa | sb | se | sf | sg;
            case 'Q': return sa | sb | sc | sd | se | sf | sm;
            case 'R': return sa | sb | se | sf | sg | sm;
            case 'S': return sa | sf | sg | sc | sd;
            case 'T': return sa | si | sl;
            case 'U': return sb | sc | sd | se | sf;
            case 'V': return se | sf | sk | sj;
            case 'W': return sb | sc | se | sf | sk | sm;
            case 'X': return sh | sj | sk | sm;
            case 'Y': return sh | sj | sl;
            case 'Z': return sa | sd | sj | sk;
            case '-': return sg;
            case '+': return sg | si | sl;
            case '*': return sg | sh | si | sj | sk | sl | sm;
            case '/': return sj | sk;
            case '\\': return sh | sm;
            case '%': return sj | sk | sf | sc | a1 | d2;
            case '=': return sg | sd;
            case '<': return sj | sm;
            case '>': return sh | sk;
            case '(': return sj | sm;
            case ')': return sh | sk;
            case '[': return a1 | d1 | si | sl;
            case ']': return a2 | d2 | si | sl;
            case '_': return sd;
            case '\'': return si;
            case '"': return sf | si;
            case '?': return sa | sb | g2 | sl;
            case '#': return sb | sc | sd | sg | si | sl;
            case '$': return sa | sf | sg | sc | sd | si | sl;
            case '&': return a1 | sh | sj | g1 | se | d1 | sm;
            case '@': return sa | sb | sc | d2 | si | g2 | se | sf | d1;
            case 0x00b0: return a2 | sb | g2 | si;   // the degree sign
            default:  return 0;
        }
    }

    /** A character on 7 segments: numbers, and the letters a 7-segment display can show. */
    int sevenOf (juce::juce_wchar c)
    {
        switch (c)
        {
            case '0': case 'O': case 'D': return sa | sb | sc | sd | se | sf;
            case '1': return sb | sc;
            case '2': case 'Z': case 'z': return sa | sb | sg | se | sd;
            case '3': return sa | sb | sc | sd | sg;
            case '4': return sf | sg | sb | sc;
            case '5': case 'S': case 's': return sa | sf | sg | sc | sd;
            case '6': return sa | sf | se | sd | sc | sg;
            case '7': return sa | sb | sc;
            case '8': case 'B': return sa | sb | sc | sd | se | sf | sg;
            case '9': return sa | sb | sc | sd | sf | sg;
            case 'A': case 'a': return sa | sb | sc | se | sf | sg;
            case 'b': return sf | se | sd | sc | sg;
            case 'C': return sa | sf | se | sd;
            case 'c': return sg | se | sd;
            case 'd': return sb | sc | sd | se | sg;
            case 'E': case 'e': return sa | sf | sg | se | sd;
            case 'F': case 'f': return sa | sf | sg | se;
            case 'G': case 'g': return sa | sf | se | sd | sc;
            case 'H': return sf | se | sb | sc | sg;
            case 'h': return sf | se | sg | sc;
            case 'I': case 'l': return sf | se;
            case 'i': return sc;
            case 'J': case 'j': return sb | sc | sd | se;
            case 'L': return sf | se | sd;
            case 'N': case 'n': case 'M': case 'm': return se | sg | sc;
            case 'o': return se | sg | sc | sd;
            case 'P': case 'p': return sa | sb | sf | sg | se;
            case 'Q': case 'q': return sa | sb | sf | sg | sc;
            case 'R': case 'r': return se | sg;
            case 'T': case 't': case 'K': case 'k': return sf | se | sd | sg;
            case 'U': case 'V': case 'W': return sf | se | sd | sc | sb;
            case 'u': case 'v': case 'w': return se | sd | sc;
            case 'Y': case 'y': return sf | sg | sb | sc | sd;
            case 'X': case 'x': return sf | se | sb | sc | sg;
            case '-': return sg;
            case '_': return sd;
            case '=': return sg | sd;
            case '[': return sa | sf | se | sd;
            case ']': return sa | sb | sc | sd;
            case '\'': return sf;
            case '"': return sf | sb;
            case '?': return sa | sb | sg | se;
            case 0x00b0: return sa | sb | sf | sg;
            default:  return 0;
        }
    }

    bool isDot (juce::juce_wchar c)    { return c == '.' || c == ':' || c == ','; }

    /** What 7 segments show as they are: numbers and their marks. */
    bool sevenAlone (const juce::String& text)
    {
        for (auto p = text.getCharPointer(); ! p.isEmpty(); ++p)
            if (! (juce::CharacterFunctions::isDigit (*p) || isDot (*p) || juce::String (" -_='\"").containsChar (*p) || *p == 0x00b0))
                return false;

        return true;
    }

    /** One bar between two points of a cell, its ends pointed (an LED's hexagonal bar). */
    void addBar (juce::Path& path, juce::Point<float> from, juce::Point<float> to, float thickness, float trimFrom, float trimTo,
                 const std::function<juce::Point<float> (juce::Point<float>)>& place)
    {
        auto along = to - from;
        const auto length = along.getDistanceFromOrigin();

        if (length <= trimFrom + trimTo + thickness * 0.3f)
            return;

        const auto u = along / length;
        const juce::Point<float> n (-u.y, u.x);
        const auto a = from + u * trimFrom, b = to - u * trimTo;
        const auto half = thickness * 0.5f;
        const auto tip = juce::jmin (half, (b - a).getDistanceFromOrigin() * 0.5f);

        path.startNewSubPath (place (a));
        path.lineTo (place (a + u * tip + n * half));
        path.lineTo (place (b - u * tip + n * half));
        path.lineTo (place (b));
        path.lineTo (place (b - u * tip - n * half));
        path.lineTo (place (a + u * tip - n * half));
        path.closeSubPath();
    }

    /** A cell's bars: the lit ones in one path, the rest in the other. */
    void cellPaths (juce::Rectangle<float> cell, int segments, int lit, float thickness, float slant, juce::Path& on, juce::Path& off)
    {
        const float H = cell.getHeight(), lean = std::tan (juce::degreesToRadians (slant));
        const float W = juce::jmax (2.0f, cell.getWidth() - H * lean);
        const float t = thickness;
        const float x0 = t * 0.5f, x1 = W - t * 0.5f, xm = W * 0.5f, y0 = t * 0.5f, y1 = H - t * 0.5f, ym = H * 0.5f;
        const float gap = t * 0.22f, cross = t * 1.15f, centre = t * 0.75f;

        auto place = [&] (juce::Point<float> p) { return juce::Point<float> (cell.getX() + p.x + (H - p.y) * lean, cell.getY() + p.y); };

        struct Bar { int bits; float ax, ay, bx, by, trimA, trimB; };
        std::vector<Bar> bars;

        if (segments == 16)
            bars.insert (bars.end(), { { a1, x0, y0, xm, y0, gap, gap }, { a2, xm, y0, x1, y0, gap, gap },
                                       { d1, x0, y1, xm, y1, gap, gap }, { d2, xm, y1, x1, y1, gap, gap } });
        else
            bars.insert (bars.end(), { { sa, x0, y0, x1, y0, gap, gap }, { sd, x0, y1, x1, y1, gap, gap } });

        bars.insert (bars.end(), { { sb, x1, y0, x1, ym, gap, gap }, { sc, x1, ym, x1, y1, gap, gap },
                                   { se, x0, ym, x0, y1, gap, gap }, { sf, x0, y0, x0, ym, gap, gap } });

        if (segments == 7)
        {
            bars.push_back ({ sg, x0, ym, x1, ym, gap, gap });
        }
        else
        {
            bars.insert (bars.end(), { { g1, x0, ym, xm, ym, gap, gap }, { g2, xm, ym, x1, ym, gap, gap },
                                       { sh, x0, y0, xm, ym, cross, centre }, { si, xm, y0, xm, ym, centre, gap },
                                       { sj, x1, y0, xm, ym, cross, centre }, { sk, x0, y1, xm, ym, cross, centre },
                                       { sl, xm, ym, xm, y1, gap, centre }, { sm, x1, y1, xm, ym, cross, centre } });
        }

        for (const auto& bar : bars)
            addBar ((lit & bar.bits) == bar.bits ? on : off, { bar.ax, bar.ay }, { bar.bx, bar.by }, t, bar.trimA, bar.trimB, place);
    }

    float numberOf (const juce::var& v, const juce::Identifier& key, float fallback)
    {
        const auto value = v.getProperty (key, {});
        return value.isVoid() || value.toString().trim().isEmpty() ? fallback : (float) (double) value;
    }

    juce::Colour colourFrom (const juce::String& textIn, juce::Colour fallback)
    {
        auto text = textIn.trim().removeCharacters ("#");

        if (text.isEmpty())
            return fallback;

        if (text.length() == 6)
            text = "FF" + text;

        return juce::Colour ((juce::uint32) text.getHexValue64());
    }

    int segmentsFrom (const juce::String& text)
    {
        const auto t = text.trim().toLowerCase();
        return t == "7" ? 7 : t == "14" ? 14 : t == "16" ? 16 : 0;
    }

    int windowFrom (const juce::String& text)
    {
        const auto t = text.trim().toLowerCase();
        return t == "none" ? 0 : t == "3d" ? 2 : 1;
    }

    void finish (SegmentDisplay::Style& s)
    {
        s.cells = juce::jlimit (0, 64, s.cells);
        s.ghost = juce::jlimit (0.0f, 0.6f, s.ghost);
        s.glow = juce::jlimit (0.0f, 1.0f, s.glow);
        s.slant = juce::jlimit (0.0f, 15.0f, s.slant);
        s.thickness = juce::jlimit (0.06f, 0.30f, s.thickness);
    }
}

//==============================================================================
SegmentDisplay::Style SegmentDisplay::styleOf (const GuiWidget& w)
{
    Style s;
    s.segments = segmentsFrom (w.settingText ("segments"));
    s.cells = juce::roundToInt (w.number ("cells", 0.0f));
    s.lit = w.colour.isTransparent() ? Style().lit : w.colour;
    s.background = colourFrom (w.settingText ("background"), Style().background);
    s.ghost = w.number ("ghost", 10.0f) * 0.01f;
    s.glow = w.number ("glow", 40.0f) * 0.01f;
    s.slant = w.number ("slant", 6.0f);
    s.thickness = w.number ("thickness", 16.0f) * 0.01f;
    s.window = windowFrom (w.settingText ("window"));
    finish (s);
    return s;
}

SegmentDisplay::Style SegmentDisplay::styleOf (const juce::var& d, const juce::String& prefix, juce::Colour litFallback)
{
    auto key = [&prefix] (const char* name)
    {
        auto k = juce::String (name);
        return juce::Identifier (prefix.isEmpty() ? k : prefix + k.substring (0, 1).toUpperCase() + k.substring (1));
    };

    Style s;
    s.segments = segmentsFrom (d.getProperty (key ("segments"), {}).toString());
    s.cells = juce::roundToInt (numberOf (d, key ("cells"), 0.0f));
    s.lit = colourFrom (d.getProperty (key ("colour"), {}).toString(), litFallback);
    s.background = colourFrom (d.getProperty (key ("background"), {}).toString(), Style().background);
    s.ghost = numberOf (d, key ("ghost"), 10.0f) * 0.01f;
    s.glow = numberOf (d, key ("glow"), 40.0f) * 0.01f;
    s.slant = numberOf (d, key ("slant"), 6.0f);
    s.thickness = numberOf (d, key ("thickness"), 16.0f) * 0.01f;
    s.window = windowFrom (d.getProperty (key ("window"), {}).toString());
    finish (s);
    return s;
}

const juce::StringArray& SegmentDisplay::colourNames()
{
    static const juce::StringArray names { "Red", "Green", "Amber", "Blue", "White", "Cyan" };
    return names;
}

juce::Colour SegmentDisplay::colourNamed (int index)
{
    static const juce::uint32 colours[] { 0xffff3b2a, 0xff3dff6e, 0xffffa21f, 0xff3f8cff, 0xfff2f4ff, 0xff3ff2ff };
    return juce::Colour (colours[juce::jlimit (0, 5, index)]);
}

int SegmentDisplay::cellsOf (const juce::String& text)
{
    int cells = 0;
    bool any = false;

    for (auto p = text.getCharPointer(); ! p.isEmpty(); ++p)
    {
        // A dot, colon or comma sits in the gap after a character; one at the start takes a cell.
        if (isDot (*p) && any)
            continue;

        ++cells;
        any = true;
    }

    return cells;
}

int SegmentDisplay::segmentsFor (const Style& s, const juce::String& text)
{
    return s.segments != 0 ? s.segments : (sevenAlone (text) ? 7 : 16);
}

void SegmentDisplay::paint (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, const Style& s, int cells)
{
    if (area.getWidth() < 2.0f || area.getHeight() < 2.0f)
        return;

    auto inside = area;

    if (s.window > 0)
    {
        const auto corner = juce::jmin (5.0f, area.getHeight() * 0.14f);

        if (s.window == 2)
        {
            // A bezel round a window set into it, its glass catching the light.
            const auto bezel = juce::jlimit (2.0f, 10.0f, area.getHeight() * 0.09f);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (area.translated (0.0f, 1.5f), corner + 1.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4c51), 0.0f, area.getY(), juce::Colour (0xff121315), 0.0f, area.getBottom(), false));
            g.fillRoundedRectangle (area, corner + 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.drawRoundedRectangle (area.reduced (0.5f), corner + 0.5f, 1.0f);
            inside = area.reduced (bezel);
        }

        const auto innerCorner = juce::jmax (1.0f, corner - 1.0f);
        g.setColour (s.background);
        g.fillRoundedRectangle (inside, innerCorner);
        g.setGradientFill (juce::ColourGradient (s.lit.withAlpha (0.06f), inside.getCentreX(), inside.getCentreY(),
                                                 juce::Colours::black.withAlpha (0.35f), inside.getX(), inside.getY(), true));
        g.fillRoundedRectangle (inside, innerCorner);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), 0.0f, inside.getY(),
                                                 juce::Colours::black.withAlpha (0.0f), 0.0f, inside.getY() + juce::jmin (8.0f, inside.getHeight() * 0.3f), false));
        g.fillRoundedRectangle (inside, innerCorner);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawRoundedRectangle (inside.reduced (0.5f), innerCorner, 1.0f);

        const auto padY = inside.getHeight() * 0.17f, padX = juce::jmax (padY, inside.getHeight() * 0.2f);
        paintCharacters (g, inside.reduced (padX, padY), text, s, cells);

        // The glass: a soft sheen across its upper part.
        juce::Path sheen;
        sheen.addRoundedRectangle (inside.withHeight (inside.getHeight() * 0.45f).reduced (1.0f, 1.0f), innerCorner);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (s.window == 2 ? 0.09f : 0.05f), 0.0f, inside.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), 0.0f, inside.getY() + inside.getHeight() * 0.45f, false));
        g.fillPath (sheen);
        return;
    }

    paintCharacters (g, inside, text, s, cells);
}

void SegmentDisplay::paintCharacters (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, const Style& s, int cellsIn)
{
    const auto segments = segmentsFor (s, text);
    const auto needed = cellsOf (text);
    const auto cells = juce::jmax (1, needed, cellsIn > 0 ? cellsIn : s.cells);

    // The cells stretched across it: a gap between them a third of one.
    const float spacing = 0.34f;
    const float cellW = area.getWidth() / ((float) cells + (float) (cells - 1) * spacing);
    const float cellH = area.getHeight();
    const float step = cellW * (1.0f + spacing);
    const float t = juce::jlimit (1.0f, cellH * 0.2f, cellW * s.thickness);
    const float lean = std::tan (juce::degreesToRadians (s.slant));

    juce::Path on, off, dots;

    // The text from the right, as LED counters show it (cells to spare stay dark on the left).
    int cell = cells - needed;
    bool any = false;

    for (auto p = text.getCharPointer(); ! p.isEmpty(); ++p)
    {
        const auto c = *p;

        if (isDot (c) && any)
        {
            // In the gap after the last character, at its foot (a colon: two, a comma: a tail).
            const auto x = area.getX() + (float) cell * step - step + cellW + cellW * spacing * 0.5f;
            const auto dot = juce::jmin (t * 1.1f, cellW * spacing * 0.9f);
            const auto foot = area.getBottom() - dot * 0.5f;

            if (c == ':')
            {
                for (const auto at : { 0.3f, 0.7f })
                {
                    const auto y = area.getY() + cellH * at;
                    dots.addEllipse (x - dot * 0.5f + (area.getBottom() - y) * lean, y - dot * 0.5f, dot, dot);
                }
            }
            else
            {
                dots.addEllipse (x - dot * 0.5f, foot - dot * 0.5f, dot, dot);

                if (c == ',')
                    dots.addTriangle (x + dot * 0.1f, foot, x + dot * 0.5f, foot, x - dot * 0.3f, foot + dot * 1.2f);
            }

            continue;
        }

        if (juce::isPositiveAndBelow (cell, cells))
        {
            const auto lit = segments == 7 ? sevenOf (c) : sixteenOf (c);
            cellPaths ({ area.getX() + (float) cell * step, area.getY(), cellW, cellH }, segments, lit, t, s.slant, on, off);
        }

        ++cell;
        any = true;
    }

    // The cells to spare on the left: all their bars dark.
    for (int i = 0; i < cells - needed; ++i)
        cellPaths ({ area.getX() + (float) i * step, area.getY(), cellW, cellH }, segments, 0, t, s.slant, on, off);

    if (s.ghost > 0.0f)
    {
        g.setColour (s.lit.withMultipliedAlpha (s.ghost));
        g.fillPath (off);
    }

    on.addPath (dots);

    // Their light: a bloom round the lit bars, then the bars.
    if (s.glow > 0.0f)
    {
        for (const auto& [width, alpha] : { std::pair<float, float> { 3.2f, 0.05f }, { 2.0f, 0.09f }, { 1.0f, 0.16f } })
        {
            g.setColour (s.lit.withMultipliedAlpha (alpha * s.glow * 1.6f));
            g.strokePath (on, juce::PathStrokeType (t * width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    g.setColour (s.lit.interpolatedWith (juce::Colours::white, 0.14f * s.glow));
    g.fillPath (on);
}
