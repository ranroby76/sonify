// D:\workspace\Fanan_web_hub\src\ConsolePanels.cpp
//
// The console-module faceplates (ConsolePanels.h), v2.
//
// The rules every panel keeps:
//   - One knob design: a dark grey knurled skirt with a machined metal cap. Two sizes per panel
//     (large = the main controls, small = the rest).
//   - What a knob does is marked by the printed disc behind it, not by the knob's own colour:
//     red = gain / level, blue = amount / shape / mix, slate = frequency / time / ratio / selector.
//   - Every knob sits on its panel's grid: the same row lines and the same column lines
//     (Grid, below). Nothing is placed by eye.
//   - One light (upper left) for every part: skirts, caps, gloss and shadows stay still, only a
//     knob's pointer turns. Shadows fall down and to the right.

#include "ConsolePanels.h"
#include "BinaryData.h"
#include "FontLibrary.h"
#include "PerformControl.h"

#include <array>
#include <cmath>
#include <tuple>
#include <vector>

namespace
{
using juce::Colour;
using juce::Graphics;
using juce::Justification;
using Rect = juce::Rectangle<float>;

const float pi = juce::MathConstants<float>::pi;

//==============================================================================
// Tokens
namespace tok
{
    const Colour rack      { 0xff141517 };
    const Colour enamelTop { 0xff6a7885 };
    const Colour enamelMid { 0xff5b6976 };
    const Colour enamelBot { 0xff4b5864 };
    const Colour silk      { 0xfff1eee6 };
    const Colour silkDim   = silk.withAlpha (0.72f);
    const Colour silkLine  = silk.withAlpha (0.55f);
    const Colour red       { 0xffc8352a };
    const Colour tape      { 0xff101113 };
    const Colour tapeText  { 0xfff2e9d0 };

    // the marker discs
    const Colour markRed   { 0xffb63a2f };
    const Colour markBlue  { 0xff3b6a9c };
    const Colour markSlate { 0xff2b3138 };
}

juce::Font barlow (float height, bool bold = true, float kerning = 0.0f)
{
    return FontLibrary::font ("Barlow Condensed", height, bold).withExtraKerningFactor (kerning);
}

juce::Font mono (float height)
{
    return FontLibrary::font ("Share Tech Mono", height, false);
}

juce::Point<float> polar (juce::Point<float> c, float r, float degrees)
{
    const auto a = juce::degreesToRadians (degrees);
    return { c.x + r * std::sin (a), c.y - r * std::cos (a) };
}

// The enamel's colour at a height on the plate (its gradient: top, middle at 42 %, base).
Colour plateColourAt (float y, float height)
{
    const auto t = juce::jlimit (0.0f, 1.0f, y / height);
    return t < 0.42f ? tok::enamelTop.interpolatedWith (tok::enamelMid, t / 0.42f)
                     : tok::enamelMid.interpolatedWith (tok::enamelBot, (t - 0.42f) / 0.58f);
}

// Machined metal: a conic gradient as thin wedges. Its highlights stay where the light is.
void fillConicMetal (Graphics& g, Rect r, float fromDegrees)
{
    static const struct { float pos; juce::uint32 argb; } stops[] = {
        { 0.00f, 0xff7d8086 }, { 0.10f, 0xfff2f3f5 }, { 0.24f, 0xff6a6d73 }, { 0.38f, 0xffcfd1d5 },
        { 0.52f, 0xff55585d }, { 0.68f, 0xffe6e7e9 }, { 0.84f, 0xff6f7277 }, { 1.00f, 0xff7d8086 } };
    const int n = 120;
    const auto from = juce::degreesToRadians (fromDegrees);

    for (int k = 0; k < n; ++k)
    {
        const float t = ((float) k + 0.5f) / (float) n;
        int s = 0; while (s < 6 && t > stops[s + 1].pos) ++s;
        const float u = (t - stops[s].pos) / (stops[s + 1].pos - stops[s].pos);
        g.setColour (Colour (stops[s].argb).interpolatedWith (Colour (stops[s + 1].argb), u));
        juce::Path p;
        p.addPieSegment (r, from + 2.0f * pi * (float) k / (float) n, from + 2.0f * pi * (float) (k + 1) / (float) n + 0.012f, 0.0f);
        g.fillPath (p);
    }
}

// A soft shadow under a round part: dark at its middle, gone at `reach`.
void roundShadow (Graphics& g, juce::Point<float> c, float reach, float alpha)
{
    juce::ColourGradient grad (juce::Colours::black.withAlpha (alpha), c.x, c.y, juce::Colours::transparentBlack, c.x + reach, c.y, true);
    grad.addColour (0.55, juce::Colours::black.withAlpha (alpha * 0.62f));
    g.setGradientFill (grad);
    g.fillEllipse (Rect (reach * 2.0f, reach * 2.0f).withCentre (c));
}

// A soft shadow under a box: layers spreading down and to the right.
void boxShadow (Graphics& g, Rect r, float corner, float distance, float alpha)
{
    const int layers = 7;
    for (int i = layers; i >= 1; --i)
    {
        const float t = (float) i / (float) layers;
        g.setColour (juce::Colours::black.withAlpha (alpha / (float) layers * 1.6f));
        g.fillRoundedRectangle (r.translated (distance * 0.45f, distance).expanded (distance * 0.9f * t), corner + distance * t);
    }
}

void silkText (Graphics& g, const juce::String& text, Rect area, float size, Colour colour, float kerning, Justification just = Justification::centred, bool bold = true)
{
    g.setFont (barlow (size, bold, kerning));
    g.setColour (colour);
    g.drawText (text, area, just, false);
}

//==============================================================================
// Layouts
enum class KnobSize { small, large, mini };
enum class Mark { slate, red, blue };

constexpr int smallD = 52, largeD = 72, miniD = 40, discRim = 9, miniRim = 7;
constexpr float defaultCaptionDrop = (float) (largeD / 2 + discRim) + 17.0f;   // every caption this far under its row line, whatever the knob's size

struct KnobDef    { const char* param; int x, y; KnobSize size; Mark mark; const char* label; std::vector<const char*> scale; float captionDrop = defaultCaptionDrop;
                    int page = -1;        // shown only while the panel's selector is on this page (-1: always)
                    int tab = -1;         // and only while the panel's tabs are on this one (-1: always)
                    bool named = false;   // its caption is what it chose (a wave's or a type's name), its own name printed above
                    const char* ring = nullptr;     // a second control on its disc (the cutoff's key shot): a yellow knob on the ring
                    const char* when = nullptr;     // shown only while this switch is on (whenOn) or off
                    bool whenOn = true; };
struct ButtonDef  { const char* param; int onValue; int x, y; const char* label; bool red = false; };   // onValue -1: toggles
struct SectionDef { int x, y, w, h; const char* title; };
struct ChipDef    { const char* param; int x, y, w; bool light = false; const char* label = ""; bool curve = false; };   // curve: an envelope SHAPE, dragged, its presets a click away   // a small choice button (a menu), or a light with its word that a click turns on and off
struct TextDef    { const char* text; int x, y; int tab = -1; };                                         // small centred caption (on a tab: shown with it)
enum class GrSource { compressor, perTwelve, perTwentyFour };
struct VuDef      { const char* source; GrSource kind; int x, y, w, h; const char* title; std::vector<const char*> marks; };
struct LedDef     { int x, y, segs; };
struct KeysDef    { int x = 0, y = 0, w = 0, h = 0, octaves = 0; };
struct WheelsDef  { int x = 0, y = 0, w = 0, h = 0; };                                                  // a pitch wheel and a mod wheel, in one housing
struct SelectorDef { int x = 0, y = 0; std::vector<const char*> items; };                             // a line of page buttons: its left end, its middle line
struct ChoiceDef   { const char* param; int cx, y; std::vector<const char*> items; int tab = -1; };      // a line of buttons, one for each of a parameter's values
struct SquareDef   { const char* param; int x, y, w; const char* label; int tab = -1; bool toggle = false; int page = -1; };   // a value in a square, dragged up and down (or a switch)
struct LaneDef     { int x = 0, y = 0, w = 0, h = 0; int page = -1;
                     const char* columns = nullptr;       // its columns' parameters, numbered from 1 after this ("Vowel Column ")
                     const char* steps = nullptr;         // its gate steps' parameters, the same way ("Vowel Gate ")
                     const char* marker = nullptr;        // the plugin's source for where its marker is, in columns ("vowel step")
                     std::vector<const char*> levels; };  // a column's choices, top to bottom
struct MatrixDef   { int x = 0, y = 0, w = 0, h = 0; int tab = -1;
                     std::vector<const char*> sources;                                 // the LFOs, as their routes' names begin ("LFO 1")
                     std::vector<std::pair<const char*, const char*>> destinations; };  // a destination's label and its parameter's name
struct SeedDef     { const char* param = nullptr; int x = 0, y = 0; int tab = -1; };                     // six digit squares on one number, and RANDOM

constexpr int squareHeight = 32, squareLabel = 14, seedWidth = 212, seedHeight = 66;
constexpr juce::uint32 glassYellow = 0xfff2c230;

// A selector button is as wide as its word.
inline int selectorItemWidth (const char* text)   { return juce::roundToInt ((float) juce::String (text).length() * 8.6f + 24.0f); }
constexpr int selectorItemGap = 4, selectorHeight = 28;

inline int selectorWidth (const std::vector<const char*>& items)
{
    int w = 0;
    for (auto* item : items) w += selectorItemWidth (item) + selectorItemGap;
    return w;
}

// A closed outline from its corner points, each rounded by its radius. `arc` makes that corner a
// true quarter circle (for a big fillet, where a quadratic curve would look pinched).
struct OutlinePoint { float x, y, radius; bool arc = false; };

inline juce::Path roundedOutline (const std::vector<OutlinePoint>& points)
{
    juce::Path path;
    const auto count = points.size();

    for (size_t i = 0; i < count; ++i)
    {
        const auto& p = points[i];
        const auto& before = points[(i + count - 1) % count];
        const auto& after = points[(i + 1) % count];
        const auto toward = [&p] (const OutlinePoint& q)
        {
            const juce::Point<float> d (q.x - p.x, q.y - p.y);
            return juce::Point<float> (p.x, p.y) + d * (p.radius / juce::jmax (0.001f, d.getDistanceFromOrigin()));
        };
        const auto in = toward (before), out = toward (after);

        if (i == 0) path.startNewSubPath (in);
        else        path.lineTo (in);

        if (p.arc)
        {
            // the circle that touches both edges: its middle is the corner pushed along both
            const juce::Point<float> middle (in.x + out.x - p.x, in.y + out.y - p.y);
            const auto a0 = std::atan2 (in.x - middle.x, middle.y - in.y), a1 = std::atan2 (out.x - middle.x, middle.y - out.y);
            auto sweep = a1 - a0;
            while (sweep > pi)  sweep -= 2.0f * pi;
            while (sweep < -pi) sweep += 2.0f * pi;
            path.addCentredArc (middle.x, middle.y, p.radius, p.radius, 0.0f, a0, a0 + sweep, false);
        }
        else
        {
            path.quadraticTo (p.x, p.y, out.x, out.y);
        }
    }

    path.closeSubPath();
    return path;
}

struct Layout
{
    int W = 0, H = 0;
    const char* model = "";
    const char* title = "";
    std::vector<SectionDef> sections;
    std::vector<KnobDef> knobs;
    std::vector<ButtonDef> buttons;
    std::vector<TextDef> texts;
    std::vector<VuDef> vus;
    std::vector<LedDef> leds;
    KeysDef keys;
    WheelsDef wheels;
    juce::Path glass;        // black glass set into the plate (Cayman's inverted L)
    SelectorDef selector;    // the page buttons on it
    SelectorDef tabs;        // the tabs at the top of its column
    juce::Line<float> rule;  // a thin line on the glass (under the tabs)
    std::vector<ChoiceDef> choices;
    std::vector<SquareDef> squares;
    SeedDef seed;
    MatrixDef matrix;
    std::vector<LaneDef> lanes;
    std::vector<ChipDef> chips;
    float captionDrop = defaultCaptionDrop;   // a panel can pull its knobs' names closer (Cayman does)
    bool presetWindow = false;                // its presets in a window of their own (Cayman), not a drop list
};

juce::Point<int> ledSize (int segs)   { return { 5 + 9 + 3 + 9 + 5, 10 + segs * 5 + (segs - 1) * 2 }; }

//==============================================================================
/** A panel's grid: columns (each its own width) and row lines. Every control is put at a column's
    middle on a row line, so knobs line up across and down the whole panel. A section's frame is
    the cells it covers, 86 px above its first row line and 84 px below its last. */
struct Grid
{
    static constexpr int above = 86, below = 84, firstRow = 182, rowPitch = 156, minWidth = 960;

    Grid (Layout& layout, std::vector<int> widths, std::vector<int> rows) : L (layout), colW (std::move (widths)), rowY (std::move (rows))
    {
        int sum = 0; for (auto w : colW) sum += w;
        L.W = juce::jmax ((int) minWidth, sum + 128);
        x0 = (L.W - sum) / 2;
    }

    int left (int c) const    { int x = x0; for (int i = 0; i < c; ++i) x += colW[(size_t) i]; return x; }
    int right (int c) const   { return left (c) + colW[(size_t) c]; }
    int cx (int c) const      { return left (c) + colW[(size_t) c] / 2; }
    int cy (int r) const      { return rowY[(size_t) r]; }

    void frame (int c0, int c1, int top, int bottom, const char* title)
    {
        L.sections.push_back ({ left (c0) + 6, top, right (c1) - left (c0) - 12, bottom - top, title });
        L.H = juce::jmax (L.H, bottom + 30);
    }

    void section (int c0, int c1, int r0, int r1, const char* title)   { frame (c0, c1, cy (r0) - above, cy (r1) + lastRowToFrame, title); }

    void knob (int c, int r, const char* param, KnobSize size, Mark mark, const char* label, std::vector<const char*> scale)
    {
        L.knobs.push_back ({ param, cx (c), cy (r), size, mark, label, std::move (scale), L.captionDrop });
    }

    void led (int c, int r, int segs)
    {
        const auto size = ledSize (segs);
        L.leds.push_back ({ cx (c) - size.x / 2, cy (r) - size.y / 2, segs });
    }

    void vu (int c0, int c1, int r, const char* source, GrSource kind, const char* title, std::vector<const char*> marks, int w = 240, int h = 140)
    {
        L.vus.push_back ({ source, kind, (left (c0) + right (c1)) / 2 - w / 2, cy (r) - h / 2, w, h, title, std::move (marks) });
    }

    void button (int c, int r, int dx, int dy, const char* param, int onValue, const char* label, bool red = false)
    {
        L.buttons.push_back ({ param, onValue, cx (c) + dx, cy (r) + dy, label, red });
    }

    void caption (int c, int r, int dx, int dy, const char* text)   { L.texts.push_back ({ text, cx (c) + dx, cy (r) + dy }); }

    Layout& L;
    std::vector<int> colW, rowY;
    int x0 = 0;
    int lastRowToFrame = below;   // a section's frame this far under its last row line
};

using S = KnobSize;
using M = Mark;
const std::vector<const char*> gr7 { "20", "10", "7", "5", "3", "1", "0" };
const std::vector<const char*> pct5 { "0", "25", "50", "75", "100" };
const std::vector<const char*> pct3 { "0", "", "100" };
const std::vector<const char*> dryWet { "DRY", "", "WET" };

bool layoutFor (const juce::String& name, Layout& L)
{
    const int r1 = Grid::firstRow, r2 = r1 + Grid::rowPitch, r3 = r2 + Grid::rowPitch;
    L = {};

    if (name == "eq")
    {
        // 5 columns x 3 rows: FREQ on row 1, Q on row 2 (the two bells), GAIN on row 3.
        L.model = "MODEL SX-84"; L.title = "FOUR BAND EQUALISER";
        Grid g (L, { 140, 140, 140, 140, 140, 70 }, { r1, r2, r3 });
        g.section (0, 0, 0, 2, "LOW"); g.section (1, 1, 0, 2, "LOW MID"); g.section (2, 2, 0, 2, "HIGH MID"); g.section (3, 3, 0, 2, "HIGH");
        g.section (4, 5, 0, 2, "FILTERS / OUTPUT");
        const std::vector<const char*> gain { "-15", "", "0", "", "+15" };
        g.knob (0, 0, "Low Shelf Freq", S::small, M::slate, "FREQ Hz", { "30", "60", "120", "250", "500" });
        g.knob (0, 2, "Low Shelf Gain", S::large, M::red, "GAIN dB", gain);
        g.knob (1, 0, "Low Mid Freq", S::small, M::slate, "FREQ Hz", { "40", "150", "500", "2K" });
        g.knob (1, 1, "Low Mid Q", S::small, M::blue, "Q", { "WIDE", "", "NARROW" });
        g.knob (1, 2, "Low Mid Gain", S::large, M::red, "GAIN dB", gain);
        g.knob (2, 0, "High Mid Freq", S::small, M::slate, "FREQ kHz", { ".3", "1", "3", "12" });
        g.knob (2, 1, "High Mid Q", S::small, M::blue, "Q", { "WIDE", "", "NARROW" });
        g.knob (2, 2, "High Mid Gain", S::large, M::red, "GAIN dB", gain);
        g.knob (3, 0, "High Shelf Freq", S::small, M::slate, "FREQ kHz", { "1.5", "3", "6", "12", "18" });
        g.knob (3, 2, "High Shelf Gain", S::large, M::red, "GAIN dB", gain);
        g.knob (4, 0, "Low Cut", S::small, M::slate, "LOW CUT Hz", { "OFF", "40", "100", "250", "500" });
        g.knob (4, 1, "High Cut", S::small, M::slate, "HIGH CUT kHz", { "2", "4", "8", "14", "OFF" });
        g.knob (4, 2, "Output", S::large, M::red, "OUTPUT dB", { "-24", "", "0", "", "+24" });
        g.led (5, 1, 16);
        return true;
    }

    if (name == "compressor")
    {
        // Row 1: the three main controls (large). Row 2: timing and shape (small).
        L.model = "MODEL SX-33"; L.title = "LIMITER / COMPRESSOR";
        Grid g (L, { 140, 140, 140, 140, 140, 140 }, { r1, r2 });
        g.section (0, 1, 0, 1, "GAIN REDUCTION"); g.section (2, 4, 0, 1, "DYNAMICS"); g.section (5, 5, 0, 1, "OUTPUT");
        g.vu (0, 1, 0, "comp.gr", GrSource::compressor, "GR", gr7);
        g.button (0, 1, -28, -8, "Detector", 0, "PEAK"); g.button (0, 1, 28, -8, "Detector", 1, "RMS"); g.button (1, 1, 0, -8, "Sidechain", -1, "S/C", true);
        g.caption (0, 1, 0, 42, "DETECTOR"); g.caption (1, 1, 0, 42, "KEY INPUT");
        g.knob (2, 0, "Threshold", S::large, M::slate, "THRESHOLD dB", { "-60", "-45", "-30", "-15", "0" });
        g.knob (3, 0, "Ratio", S::large, M::slate, "RATIO", { "1.5", "2", "4", "8", "20" });
        g.knob (4, 0, "Makeup", S::large, M::red, "MAKEUP dB", { "-12", "0", "+12", "+24" });
        g.knob (2, 1, "Attack", S::small, M::slate, "ATTACK ms", { ".1", "1", "10", "200" });
        g.knob (3, 1, "Release", S::small, M::slate, "RELEASE ms", { "5", "50", "500", "2K" });
        g.knob (4, 1, "Knee", S::small, M::blue, "KNEE dB", { "HARD", "", "12", "", "SOFT" });
        g.led (5, 0, 16);
        g.knob (5, 1, "Mix", S::small, M::blue, "MIX %", dryWet);
        return true;
    }

    if (name == "limiter")
    {
        L.model = "MODEL SX-10"; L.title = "PEAK LIMITER";
        Grid g (L, { 140, 140, 140, 140, 140, 90 }, { r1 });
        g.section (0, 1, 0, 0, "GAIN REDUCTION"); g.section (2, 4, 0, 0, "LIMITER"); g.section (5, 5, 0, 0, "OUT");
        g.vu (0, 1, 0, "limit.gr", GrSource::perTwelve, "GR", gr7);
        g.knob (2, 0, "Gain", S::large, M::red, "GAIN dB", { "0", "6", "12", "18", "24" });
        g.knob (3, 0, "Ceiling", S::large, M::red, "CEILING dBTP", { "-12", "-9", "-6", "-3", "0" });
        g.knob (4, 0, "Release", S::small, M::slate, "RELEASE ms", { "1", "10", "100", "1K" });
        g.led (5, 0, 16);
        return true;
    }

    if (name == "chorus")
    {
        L.model = "MODEL SX-60"; L.title = "ENSEMBLE CHORUS";
        Grid g (L, { 140, 140, 140, 140, 90 }, { r1 });
        g.section (0, 2, 0, 0, "MODULATION"); g.section (3, 4, 0, 0, "OUTPUT");
        g.knob (0, 0, "Rate", S::large, M::slate, "RATE Hz", { ".15", ".5", "1", "2", "3" });
        g.knob (1, 0, "Depth", S::large, M::blue, "DEPTH %", pct5);
        g.knob (2, 0, "Mix", S::small, M::blue, "MIX %", dryWet);
        g.knob (3, 0, "Output", S::small, M::red, "LEVEL dB", { "-12", "0", "+12" });
        g.led (4, 0, 16);
        return true;
    }

    if (name == "phaser")
    {
        L.model = "MODEL SX-41"; L.title = "FOUR STAGE PHASER";
        Grid g (L, { 140, 140, 140, 140, 90 }, { r1 });
        g.section (0, 3, 0, 0, "MODULATION"); g.section (4, 4, 0, 0, "OUT");
        g.knob (0, 0, "Rate", S::large, M::slate, "RATE Hz", { ".05", "1", "2", "3", "4" });
        g.knob (1, 0, "Depth", S::large, M::blue, "DEPTH %", pct5);
        g.knob (2, 0, "Feedback", S::small, M::blue, "FEEDBACK %", { "0", "30", "60", "90" });
        g.knob (3, 0, "Mix", S::small, M::blue, "MIX %", dryWet);
        g.led (4, 0, 16);
        return true;
    }

    if (name == "autowah")
    {
        L.model = "MODEL SX-12"; L.title = "ENVELOPE FILTER";
        Grid g (L, { 140, 140, 140, 140, 140, 140, 90 }, { r1 });
        g.section (0, 2, 0, 0, "ENVELOPE"); g.section (3, 4, 0, 0, "LFO"); g.section (5, 6, 0, 0, "OUTPUT");
        g.knob (0, 0, "Sensitivity", S::large, M::blue, "SENSITIVITY %", pct5);
        g.knob (1, 0, "Frequency", S::large, M::slate, "FREQUENCY Hz", { "80", "200", "500", "1.5K" });
        g.knob (2, 0, "Resonance", S::small, M::blue, "RESONANCE %", pct3);
        g.knob (3, 0, "LFO Rate", S::small, M::slate, "RATE Hz", { ".05", "", "4", "", "8" });
        g.knob (4, 0, "LFO Depth", S::small, M::blue, "DEPTH %", pct3);
        g.knob (5, 0, "Mix", S::small, M::blue, "MIX %", dryWet);
        g.led (6, 0, 16);
        return true;
    }

    if (name == "delay")
    {
        // One row. SONG / FREE sit beside the two time knobs they choose between.
        L.model = "MODEL SX-24"; L.title = "PING-PONG DELAY";
        Grid g (L, { 100, 140, 140, 140, 140, 140, 140, 140, 90 }, { r1 });
        g.section (0, 2, 0, 0, "TIME"); g.section (3, 5, 0, 0, "REPEATS"); g.section (6, 8, 0, 0, "MIX");
        g.button (0, 0, 0, -40, "Sync", 1, "SONG"); g.button (0, 0, 0, 22, "Sync", 0, "FREE");
        g.knob (1, 0, "Note", S::large, M::slate, "NOTE", { "1/1", "1/2", "1/4", "1/8", "1/16", "1/4.", "1/8.", "1/4T", "1/8T" });
        g.knob (2, 0, "Time", S::small, M::slate, "FREE TIME ms", { "10", "100", "1K", "2.4K" });
        g.knob (3, 0, "Feedback", S::large, M::blue, "FEEDBACK %", { "0", "25", "50", "75", "95" });
        g.knob (4, 0, "Damping", S::small, M::slate, "DAMPING Hz", { "500", "2K", "8K", "20K" });
        g.knob (5, 0, "Low Cut", S::small, M::slate, "LOW CUT Hz", { "OFF", "100", "300", "1K" });
        g.knob (6, 0, "Dry", S::small, M::red, "DRY %", pct3);
        g.knob (7, 0, "Wet", S::small, M::red, "WET %", pct3);
        g.led (8, 0, 16);
        return true;
    }

    if (name == "sweetener")
    {
        // Four stages as columns: its IN button on one line, its two knobs on the two row lines.
        L.model = "MODEL SX-4S"; L.title = "FOUR STAGE SWEETENER";
        const int buttons = 148, k1 = 262, k2 = k1 + Grid::rowPitch;
        Grid g (L, { 140, 140, 140, 140, 140 }, { buttons, k1, k2 });
        const char* const titles[4] = { "SOFTEN", "PEAK", "TAME", "ROUND" };
        const char* const on[4] = { "Soften", "Peak", "Tame", "Round" };
        for (int c = 0; c < 4; ++c)
        {
            g.frame (c, c, 96, k2 + Grid::below, titles[c]);
            g.button (c, 0, 0, 0, on[c], -1, "IN", true);
        }
        g.frame (4, 4, 96, k2 + Grid::below, "OUTPUT");
        g.knob (0, 1, "Soften Amount", S::small, M::blue, "AMOUNT %", { "SHARP", "", "0", "", "SOFT" });
        g.knob (0, 2, "Soften Time", S::small, M::slate, "TIME ms", { "1", "10", "150" });
        g.knob (1, 1, "Peak Ceiling", S::small, M::red, "CEILING dB", { "3", "12", "24" });
        g.knob (1, 2, "Peak Ratio", S::small, M::slate, "RATIO", { "1", "4", "20" });
        g.knob (2, 1, "Tame Depth", S::small, M::red, "DEPTH dB", { "0", "6", "12" });
        g.knob (2, 2, "Tame Freq", S::small, M::slate, "FREQ kHz", { "1.5", "4", "8" });
        g.knob (3, 1, "Round Drive", S::small, M::red, "DRIVE %", pct3);
        g.knob (3, 2, "Round Mix", S::small, M::blue, "ROUND MIX %", dryWet);
        g.knob (4, 1, "Mix", S::large, M::blue, "MIX %", { "DRY", "", "50", "", "WET" });
        g.led (4, 2, 14);
        return true;
    }

    if (name == "ducker")
    {
        // Two bands, one row each, the same columns.
        L.model = "MODEL SX-2D"; L.title = "TWO BAND DUCKER";
        const int rowB = r1 + Grid::below + 12 + Grid::above;
        Grid g (L, { 90, 130, 130, 130, 130, 130, 130, 130, 240 }, { r1, rowB });
        for (int band = 0; band < 2; ++band)
        {
            const bool low = band == 0;
            g.section (0, 8, band, band, low ? "LOW BAND" : "HIGH BAND");
            g.button (0, band, 0, -8, low ? "Low On" : "High On", -1, "ON", true);
            g.knob (1, band, low ? "Low Freq" : "High Freq", S::small, M::slate, "FREQ Hz", { "40", "400", "4K", "16K" });
            g.knob (2, band, low ? "Low Q" : "High Q", S::small, M::blue, "Q", { "WIDE", "", "NARROW" });
            g.knob (3, band, low ? "Low Threshold" : "High Threshold", S::large, M::slate, "THRESHOLD dB", { "-60", "-30", "0" });
            g.knob (4, band, low ? "Low Depth" : "High Depth", S::large, M::red, "DEPTH dB", { "0", "6", "12", "18", "24" });
            g.knob (5, band, low ? "Low Ratio" : "High Ratio", S::small, M::slate, "RATIO", { "1", "4", "20" });
            g.knob (6, band, low ? "Low Attack" : "High Attack", S::small, M::slate, "ATTACK ms", { ".5", "10", "200" });
            g.knob (7, band, low ? "Low Release" : "High Release", S::small, M::slate, "RELEASE ms", { "20", "150", "1K" });
            g.vu (8, 8, band, low ? "duck.gr1" : "duck.gr2", GrSource::perTwentyFour, "DUCK", gr7, 216, 140);
        }
        return true;
    }

    if (name == "mastering")
    {
        // 8 columns x 3 rows. Rows 1-2: the equaliser (FREQ over GAIN). Row 3: the level chain, in
        // the order the sound takes. The glue compressor down the right, on the same three lines.
        L.model = "MODEL SX-M1"; L.title = "MASTERING CHAIN";
        const int rowC = r2 + Grid::below + 12 + Grid::above;
        Grid g (L, { 140, 140, 140, 140, 140, 140, 140, 140, 80 }, { r1, r2, rowC });
        g.section (0, 5, 0, 1, "EQUALISER"); g.section (0, 5, 2, 2, "LEVELS / LIMITER"); g.section (6, 7, 0, 2, "GLUE"); g.section (8, 8, 0, 2, "OUT");
        g.knob (0, 0, "Low Cut", S::small, M::slate, "LOW CUT Hz", { "OFF", "40", "120" });
        g.knob (0, 1, "EQ Amount", S::large, M::blue, "EQ AMOUNT %", pct5);
        const struct { const char* freq; const char* gain; const char* fLabel; const char* gLabel; std::vector<const char*> scale; } bands[5] = {
            { "Low Freq", "Low", "LOW Hz", "LOW dB", { "40", "90", "200" } },
            { "Low Mid Freq", "Low Mid", "LOW MID Hz", "LOW MID dB", { "120", "250", "600" } },
            { "Mid Freq", "Mid", "MID kHz", "MID dB", { ".5", "1", "3" } },
            { "Presence Freq", "Presence", "PRESENCE kHz", "PRESENCE dB", { "2", "3.5", "8" } },
            { "Air Freq", "Air", "AIR kHz", "AIR dB", { "6", "10", "16" } } };
        for (int b = 0; b < 5; ++b)
        {
            g.knob (b + 1, 0, bands[b].freq, S::small, M::slate, bands[b].fLabel, bands[b].scale);
            g.knob (b + 1, 1, bands[b].gain, S::large, M::red, bands[b].gLabel, { "-6", "", "0", "", "+6" });
        }
        g.knob (0, 2, "Input", S::small, M::red, "INPUT dB", { "-12", "0", "+12" });
        g.knob (1, 2, "Drive", S::small, M::red, "DRIVE dB", { "-24", "0", "+24" });
        g.knob (2, 2, "Width", S::small, M::blue, "WIDTH %", { "0", "100", "200" });
        g.knob (3, 2, "Ceiling", S::small, M::red, "CEILING dBTP", { "-6", "-3", "0" });
        g.knob (4, 2, "Limiter Release", S::small, M::slate, "LIM RELEASE ms", { "10", "80", "500" });
        g.knob (5, 2, "Output", S::small, M::red, "OUTPUT dB", { "-12", "-6", "0" });
        g.vu (6, 7, 0, "master.glue", GrSource::perTwelve, "GLUE GR", { "10", "6", "4", "2", "1", "0" });
        g.knob (6, 1, "Glue Threshold", S::large, M::slate, "THRESHOLD dB", { "-40", "-20", "0" });
        g.knob (7, 1, "Glue Ratio", S::large, M::slate, "RATIO", { "1", "2", "8" });
        g.knob (6, 2, "Glue Attack", S::small, M::slate, "ATTACK ms", { "1", "10", "100" });
        g.knob (7, 2, "Glue Release", S::small, M::slate, "RELEASE ms", { "20", "200", "1K" });
        g.led (8, 1, 16);
        return true;
    }

    if (name == "testsynth")
    {
        // Cayman: 10 columns x 3 rows of knobs, packed tighter than the other panels (names 8 px
        // under their discs, rows 130 px apart). Black glass in the shape of an inverted L: a column
        // where the last two grid columns would be, joined to a band above the keyboard. The band
        // carries a line of page buttons (the LFO, the vibration, the six effects) in a narrow strip,
        // the plate taking a bay above and below it, then the chosen page's knobs. Output sits in
        // the corner the band leaves, on the plate, on the same line. Then the keyboard.
        L.model = "MODEL SX-CY"; L.title = "CAYMAN SYNTHESISER";
        L.presetWindow = true;
        L.captionDrop = (float) (largeD / 2 + discRim) + 3.0f;      // close under the knobs: room for a title over each envelope row
        const int pitch = 130, c2 = r1 + pitch, c3 = c2 + pitch;
        Grid g (L, { 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124 }, { r1, c2, c3 });
        g.lastRowToFrame = 75;
        g.section (0, 0, 0, 2, "OSC 1"); g.section (1, 2, 0, 2, "OSC 2"); g.section (3, 3, 0, 2, "LAYERS"); g.section (4, 5, 0, 2, "FILTER");
        g.section (6, 9, 0, 2, "ENVELOPES  /  VOICE");

        // each row of that section titled, in the gap the names' closeness leaves above it
        const int envelopeMiddle = (g.left (6) + g.right (9)) / 2;
        L.texts.push_back ({ "AMP ENV", envelopeMiddle, r1 - 61 });
        L.chips.push_back ({ "Amp Shape", g.right (9) - 6 - 12 - 112, r1 - 63, 112, false, "SHAPE", true });   // at the AMP ENV title's right
        L.texts.push_back ({ "FILTER ENV", envelopeMiddle, c2 - 61 });
        L.texts.push_back ({ "VOICE", envelopeMiddle, c3 - 61 });
        // The wave and filter type knobs have too many choices to print round them: their name
        // is printed above and what they chose is written below.
        const std::vector<const char*> oct { "-2", "", "0", "", "+2" };
        g.knob (0, 0, "Osc 1 Wave", S::small, M::slate, "WAVE", { "WAVE" });
        L.knobs.back().named = true;
        g.knob (0, 1, "Osc 1 Octave", S::small, M::slate, "OCTAVE", oct);
        g.knob (0, 2, "Osc 1 Level", S::small, M::red, "LEVEL", pct3);
        g.knob (1, 0, "Osc 2 Semi", S::small, M::slate, "SEMI", { "-12", "0", "+12" });
        g.knob (1, 1, "Osc 2 Detune", S::small, M::blue, "DETUNE ct", { "0", "", "50" });
        g.knob (1, 2, "Pulse Width", S::small, M::blue, "PULSE W", { "5", "50", "95" });
        g.knob (2, 0, "Osc 2 Wave", S::small, M::slate, "WAVE", { "WAVE" });
        L.knobs.back().named = true;
        g.knob (2, 1, "Osc 2 Octave", S::small, M::slate, "OCTAVE", oct);
        g.knob (2, 2, "Osc 2 Level", S::small, M::red, "LEVEL", pct3);
        g.knob (3, 0, "Sub", S::small, M::red, "SUB", pct3);
        g.knob (3, 1, "Noise", S::small, M::red, "NOISE", pct3);
        g.knob (3, 2, "Drive", S::small, M::red, "DRIVE", pct3);

        // at the drive's upper corners: its kind (a click lists them) and RAW (its aliasing left in)
        L.chips.push_back ({ "Drive Type", g.cx (3) - 52, c3 - 62, 38 });
        L.chips.push_back ({ "Drive Raw", g.cx (3) + 16, c3 - 62, 36, true, "RAW" });
        g.knob (4, 0, "Filter", S::small, M::slate, "TYPE", { "TYPE" });
        L.knobs.back().named = true;
        g.knob (4, 1, "Cutoff", S::large, M::slate, "CUTOFF Hz", { "20", "200", "2K", "20K" });
        L.knobs.back().ring = "Key Shot";
        g.knob (4, 2, "Resonance", S::small, M::blue, "RESONANCE", pct3);
        g.knob (5, 0, "Env Amount", S::small, M::blue, "ENV AMT", { "-", "0", "+" });
        g.knob (5, 1, "Key Track", S::small, M::slate, "KEY TRACK", pct3);
        g.knob (5, 2, "LFO Filter", S::small, M::blue, "LFO AMT", pct3);
        const char* const fenv[4] = { "F Attack", "F Decay", "F Sustain", "F Release" };
        const char* const aenv[4] = { "Attack", "Decay", "Sustain", "Release" };
        const char* const fl[4] = { "F ATTACK", "F DECAY", "F SUSTAIN", "F RELEASE" };
        const char* const al[4] = { "ATTACK", "DECAY", "SUSTAIN", "RELEASE" };
        for (int i = 0; i < 4; ++i)
        {
            const bool sustain = i == 2;
            g.knob (6 + i, 0, aenv[i], S::small, sustain ? M::red : M::slate, al[i], sustain ? pct3 : std::vector<const char*> { "1", "", "5K" });
            g.knob (6 + i, 1, fenv[i], S::small, sustain ? M::red : M::slate, fl[i], sustain ? pct3 : std::vector<const char*> { "1", "", "5K" });
        }
        g.knob (6, 2, "Unison", S::small, M::slate, "UNISON", { "1", "", "7" });
        g.knob (7, 2, "Unison Detune", S::small, M::blue, "DETUNE ct", pct3);
        g.knob (8, 2, "Width", S::small, M::blue, "WIDTH", pct3);
        g.knob (9, 2, "Velocity", S::small, M::blue, "VELOCITY", pct3);

        // the black glass: the column (grid columns 10 and 11), the band, the two bays
        const int top = r1 - Grid::above, frameBottom = c3 + g.lastRowToFrame;
        const int bandTop = frameBottom + 14, bandH = 110, bandBottom = bandTop + bandH, mid = bandTop + bandH / 2;
        const int bandLeft = g.left (0) + 6, outputX = g.cx (11), bandRight = outputX - 74;
        const int columnLeft = g.left (10) + 6, columnRight = g.right (11) - 6;

        L.selector = { bandLeft + 20, mid, { "LFO 1", "LFO 2", "VIBRATION", "CHORUS", "PHASER", "WAH", "VOWEL", "DELAY", "REVERB" } };
        const int selectorEnd = L.selector.x + selectorWidth (L.selector.items);
        const float stripTop = (float) mid - 22.0f, stripBottom = (float) mid + 22.0f;
        const float slopeIn = (float) selectorEnd + 9.0f, slopeOut = slopeIn + (stripTop - (float) bandTop);   // 45 degrees

        L.glass = roundedOutline ({ { (float) columnLeft, (float) top, 6.0f }, { (float) columnRight, (float) top, 6.0f },
                                    { (float) columnRight, (float) frameBottom, 6.0f },
                                    { (float) bandRight, (float) frameBottom, 70.0f, true },      // round Output's upper left
                                    { (float) bandRight, (float) bandBottom, 6.0f },
                                    { slopeOut, (float) bandBottom, 16.0f }, { slopeIn, stripBottom, 16.0f },
                                    { (float) bandLeft, stripBottom, 8.0f }, { (float) bandLeft, stripTop, 8.0f },
                                    { slopeIn, stripTop, 16.0f }, { slopeOut, (float) bandTop, 16.0f },
                                    { (float) columnLeft, (float) bandTop, 8.0f } });

        // the pages' knobs: small ones, in a line on the band's middle
        const int firstX = selectorEnd + 80, slotPitch = juce::jmin (112, (bandRight - 50 - firstX) / 5);   // six to a page at most
        const float miniDrop = (float) (miniD / 2 + miniRim) + 8.0f;
        const auto onPage = [&] (int page, int slot, const char* param, Mark mark, const char* label, std::vector<const char*> scale)
        {
            L.knobs.push_back ({ param, firstX + slotPitch * slot, mid, S::mini, mark, label, std::move (scale), miniDrop, page });
        };
        const std::vector<const char*> hz8 { ".05", "", "8" };

        // The two LFOs' pages: the wave (its name under it), the rate free or, while SYNC is on, the
        // note value in time with the song in its place, and how far the matrix moves the knobs it plays.
        for (int lfo = 0; lfo < 2; ++lfo)
        {
            const auto* sync = lfo == 0 ? "LFO 1 Sync" : "LFO 2 Sync";
            onPage (lfo, 0, lfo == 0 ? "LFO Wave" : "LFO 2 Wave", M::slate, "WAVE", { "WAVE" });
            L.knobs.back().named = true;
            onPage (lfo, 1, lfo == 0 ? "LFO Rate" : "LFO 2 Rate", M::slate, "RATE Hz", { ".05", "", "20" });
            L.knobs.back().when = sync; L.knobs.back().whenOn = false;
            onPage (lfo, 1, lfo == 0 ? "LFO 1 Note" : "LFO 2 Note", M::slate, "NOTE", { "NOTE" });
            L.knobs.back().named = true; L.knobs.back().when = sync;
            L.squares.push_back ({ sync, firstX + slotPitch * 2 - 25, mid - squareHeight / 2, 50, "SYNC", -1, true, lfo });
            onPage (lfo, 3, lfo == 0 ? "LFO 1 Depth" : "LFO 2 Depth", M::blue, "MATRIX AMT", pct3);
        }

        // VIBRATION: its own sine, at its RATE
        onPage (2, 0, "Vibration Rate", M::slate, "RATE Hz", { ".5", "", "12" });
        onPage (2, 1, "Tremolo", M::blue, "TREMOLO", pct3);
        onPage (2, 2, "Vibrato", M::blue, "VIBRATO", pct3);
        onPage (3, 0, "Chorus Rate", M::slate, "RATE", pct3);
        onPage (3, 1, "Chorus Depth", M::blue, "DEPTH", pct3);
        onPage (3, 2, "Chorus", M::blue, "MIX", dryWet);
        onPage (4, 0, "Phaser Rate", M::slate, "RATE Hz", { ".05", "", "4" });
        onPage (4, 1, "Phaser Depth", M::blue, "DEPTH", pct3);
        onPage (4, 2, "Phaser Feedback", M::blue, "FEEDBACK", { "0", "", "90" });
        onPage (4, 3, "Phaser Mix", M::blue, "MIX", dryWet);
        onPage (5, 0, "Wah Sensitivity", M::blue, "SENS", pct3);
        onPage (5, 1, "Wah Rate", M::slate, "RATE Hz", hz8);
        onPage (5, 2, "Wah Depth", M::blue, "DEPTH", pct3);
        onPage (5, 3, "Wah Frequency", M::slate, "FREQ Hz", { "80", "", "1.5K" });
        onPage (5, 4, "Wah Resonance", M::slate, "Q", pct3);
        onPage (5, 5, "Wah Mix", M::blue, "MIX", dryWet);
        // VOWEL: the lane (eight columns, a vowel each, the marker stepping across them; the
        // gate's 16 steps under them), then RATE (the note a column lasts), MORPH or JUMP, the
        // resonance, the mix and the gate's depth, closer together than the other pages' knobs.
        {
            const int laneX = firstX - 27, laneW = 240;
            L.lanes.push_back ({ laneX, bandTop + 10, laneW, bandH - 24, 6, "Vowel Column ", "Vowel Gate ", "vowel step", { "A", "E", "O", "U", "I" } });
            const int x0 = laneX + laneW + 40, knobPitch = 64;
            const auto vowelKnob = [&] (int slot, const char* param, Mark mark, const char* label, std::vector<const char*> scale, bool named = false)
            {
                L.knobs.push_back ({ param, x0 + knobPitch * slot, mid, S::mini, mark, label, std::move (scale), miniDrop, 6 });
                L.knobs.back().named = named;
            };
            vowelKnob (0, "Vowel Note", M::slate, "RATE", { "RATE" }, true);
            L.squares.push_back ({ "Vowel Mode", x0 + knobPitch - 25, mid - squareHeight / 2, 50, "MODE", -1, false, 6 });
            vowelKnob (2, "Vowel Resonance", M::slate, "RES", {});
            vowelKnob (3, "Vowel Mix", M::blue, "MIX", {});
            vowelKnob (4, "Vowel Gate", M::red, "GATE", {});
        }
        // DELAY: the time free or, while SYNC is on, a note value at the song's tempo in its place
        onPage (7, 0, "Delay Time", M::slate, "TIME ms", { "10", "", "2.4K" });
        L.knobs.back().when = "Delay Sync"; L.knobs.back().whenOn = false;
        onPage (7, 0, "Delay Note", M::slate, "NOTE", { "NOTE" });
        L.knobs.back().named = true; L.knobs.back().when = "Delay Sync";
        L.squares.push_back ({ "Delay Sync", firstX + slotPitch - 25, mid - squareHeight / 2, 50, "SYNC", -1, true, 7 });
        onPage (7, 2, "Delay Feedback", M::blue, "FEEDBACK", { "0", "", "95" });
        onPage (7, 3, "Delay Tone", M::slate, "TONE Hz", { "500", "", "20K" });
        onPage (7, 4, "Delay Mix", M::blue, "MIX", dryWet);
        onPage (8, 0, "Reverb Size", M::blue, "SIZE", pct3);
        onPage (8, 1, "Reverb Damping", M::blue, "DAMP", pct3);
        onPage (8, 2, "Reverb Pre-Delay", M::slate, "PRE ms", { "0", "", "200" });
        onPage (8, 3, "Reverb Mix", M::blue, "MIX", dryWet);

        // The column's three tabs. VOICES: how the keys are played (the mode's buttons, then two
        // knobs to a line). ARP: Domina's seeded arpeggiator, its seed as six digit squares and the
        // rest as values in squares, four to a line. MATRIX: which knobs each LFO plays.
        const int columnMiddle = (columnLeft + columnRight) / 2;
        L.tabs = { 0, top + 28, { "VOICES", "ARP", "MATRIX" } };
        L.tabs.x = columnMiddle - (selectorWidth (L.tabs.items) - selectorItemGap) / 2;
        L.rule = { (float) columnLeft + 16.0f, (float) top + 50.0f, (float) columnRight - 16.0f, (float) top + 50.0f };

        L.choices.push_back ({ "Mode", columnMiddle, top + 76, { "POLY", "MONO", "LEGATO", "ACID" }, 0 });
        // col -1: the middle of the line
        const auto onTab = [&] (int col, int row, const char* param, Mark mark, const char* label, std::vector<const char*> scale)
        {
            L.knobs.push_back ({ param, columnMiddle + (col < 0 ? 0 : col == 0 ? -56 : 56), top + 146 + row * 104, S::mini, mark, label, std::move (scale), miniDrop, -1, 0 });
        };
        onTab (0, 0, "Glide", M::slate, "GLIDE ms", { "0", "", "2K" });
        onTab (1, 0, "Slide", M::slate, "SLIDE ms", { "1", "", "500" });
        onTab (0, 1, "Accent", M::blue, "ACCENT", pct3);
        onTab (1, 1, "Bend Range", M::slate, "BEND", { "0", "", "24" });
        onTab (-1, 2, "Low Velocity", M::blue, "LOW VEL", pct3);      // Grex's LOW VELOCITY RESPONSE: soft notes lifted

        const int squaresLeft = columnLeft + 12, cell = 50, cellPitch = 54;
        L.seed = { "Arp Seed", squaresLeft, top + 56, 1 };
        const auto square = [&] (int col, int row, const char* param, const char* label, int span = 1, bool toggle = false)
        {
            L.squares.push_back ({ param, squaresLeft + col * cellPitch, top + 142 + row * 62, cell * span + (cellPitch - cell) * (span - 1), label, 1, toggle });
        };
        square (0, 0, "Arp", "ARP", 1, true);
        square (1, 0, "Arp Idiom", "IDIOM", 2);
        square (3, 0, "Arp Originality", "ORIGINAL");
        square (0, 1, "Arp Bars", "BARS");
        square (1, 1, "Arp Octaves", "OCTAVES");
        square (2, 1, "Arp Complexity", "COMPLEX");
        square (3, 1, "Arp Rests", "RESTS");
        square (0, 2, "Arp 1/16", "1/16");
        square (1, 2, "Arp 1/8", "1/8");
        square (2, 2, "Arp 1/4", "1/4");
        square (3, 2, "Arp Sort", "SORT");
        square (0, 3, "Arp Gate Min", "GATE MIN");
        square (1, 3, "Arp Gate Max", "GATE MAX");
        square (2, 3, "Arp Run", "RUN");
        square (3, 3, "Arp Hold", "HOLD");
        L.texts.push_back ({ "CHORDS DRIVEN ARP", columnMiddle, frameBottom - 30, 1 });

        // MATRIX: the LFOs on the left, the knobs either can play on the right (the ones marked
        // on the panel; a scroll bar if they ever outgrow the column)
        L.matrix = { columnLeft + 12, top + 62, columnRight - columnLeft - 24, frameBottom - 10 - (top + 62), 2, { "LFO 1", "LFO 2" },
                     { { "OSC 2 WAVE", "Osc 2 Wave" }, { "OSC 2 DETUNE", "Osc 2 Detune" }, { "PULSE W", "Pulse Width" }, { "OSC 2 LEVEL", "Osc 2 Level" },
                       { "SUB", "Sub" }, { "CUTOFF", "Cutoff" }, { "RESONANCE", "Resonance" }, { "ENV AMT", "Env Amount" }, { "KEY TRACK", "Key Track" },
                       { "AMP ATTACK", "Attack" }, { "UNI DETUNE", "Unison Detune" }, { "WIDTH", "Width" }, { "VELOCITY", "Velocity" } } };

        // Output: its own column, down on the band's line, on the plate
        L.knobs.push_back ({ "Output", outputX, mid, S::small, M::red, "OUTPUT dB", { "-24", "", "+6" }, (float) (smallD / 2 + discRim) + 10.0f });

        const int keysTop = bandBottom + 14, keysH = 130, ledW = ledSize (14).x;
        const int wheelsW = 104, wheelsX = g.left (0) + 6, keysX = wheelsX + wheelsW + 14;      // the wheels at the keyboard's left
        L.keys = { keysX, keysTop, g.right (11) - 6 - ledW - 14 - keysX, keysH, 5 };
        L.wheels = { wheelsX, keysTop, wheelsW, keysH };
        L.texts.push_back ({ "PITCH", wheelsX + wheelsW / 4 + 2, keysTop + keysH + 4 });
        L.texts.push_back ({ "MOD", wheelsX + wheelsW * 3 / 4 - 2, keysTop + keysH + 4 });
        L.leds.push_back ({ g.right (11) - 6 - ledW, keysTop + (keysH - ledSize (14).y) / 2, 14 });
        L.H = keysTop + keysH + 30;
        return true;
    }

    return false;
}

//==============================================================================
// A parameter of the plugin, by name: its spec, its value, and its travel (0..1, as heard).
struct ParamRef
{
    BuiltinPlugin* plugin = nullptr;
    int index = -1;

    /** The parameter by its name, or an invalid one (no complaint: for controls that are optional). */
    static ParamRef lookup (BuiltinPlugin& p, const juce::String& name)
    {
        const auto& specs = p.getSpecs();
        for (size_t i = 0; i < specs.size(); ++i)
            if (name == specs[i].name)
                return { &p, static_cast<int> (i) };
        return {};
    }

    static ParamRef find (BuiltinPlugin& p, const char* name)
    {
        const auto& specs = p.getSpecs();
        for (size_t i = 0; i < specs.size(); ++i)
            if (juce::String (specs[i].name) == name)
                return { &p, static_cast<int> (i) };
        jassertfalse;   // a control on a parameter the plugin hasn't got
        return {};
    }

    bool valid() const { return plugin != nullptr && index >= 0; }
    const BuiltinPlugin::Spec& spec() const { return plugin->getSpecs()[(size_t) index]; }
    double value() const { return valid() ? plugin->getParameterValue (index) : 0.0; }

    double toTravel (double v) const
    {
        if (! valid()) return 0.0;
        const auto& s = spec();
        if (BuiltinPlugin::isLogRange (s))
            return std::log (juce::jmax (s.minValue, v) / s.minValue) / std::log (s.maxValue / s.minValue);
        return (v - s.minValue) / (s.maxValue - s.minValue);
    }

    double fromTravel (double t) const
    {
        if (! valid()) return 0.0;
        const auto& s = spec();
        t = juce::jlimit (0.0, 1.0, t);
        const auto v = BuiltinPlugin::isLogRange (s) ? s.minValue * std::pow (s.maxValue / s.minValue, t) : s.minValue + t * (s.maxValue - s.minValue);
        return BuiltinPlugin::quantised (s, v);
    }

    void set (double v) const
    {
        if (! valid())
            return;
        plugin->queueParameter (index, v);
        plugin->panelTouched (index);
    }

    juce::String text (double v) const { return valid() ? plugin->getParameterText (index, v) : juce::String(); }
};

//==============================================================================
// A knob on its marker disc. Still layers (disc, scale, shadow, skirt, cap, gloss) are cached;
// only the pointer is drawn each time.
class Knob final : public juce::Component
{
public:
    Knob (ParamRef p, const KnobDef& d)
        : param (p), def (d), r ((float) (d.size == KnobSize::large ? largeD : d.size == KnobSize::mini ? miniD : smallD) * 0.5f),
          discR (r + (float) (d.size == KnobSize::mini ? miniRim : discRim)), margin (discR + 40.0f)
    {
        for (auto* s : d.scale) scale.add (s);
        setRepaintsOnMouseActivity (true);
        setTitle (d.label);
        shown = param.value();

        if (d.ring != nullptr && param.valid())
        {
            ring = ParamRef::lookup (*param.plugin, d.ring);
            ringShown = ring.value();
        }
    }

    /** How far the matrix's LFOs move it now, in its travel (set by the plate for a knob they can play). */
    std::function<double()> modulation;

    juce::Point<float> centre() const { return { margin, margin }; }
    juce::Rectangle<int> boundsFor() const
    {
        return juce::Rectangle<float> ((float) def.x - margin, (float) def.y - margin, margin * 2.0f, margin + def.captionDrop + 17.0f).toNearestInt();
    }

    bool hitTest (int x, int y) override
    {
        return centre().getDistanceFrom ({ (float) x, (float) y }) <= (ring.valid() ? discR + 4.0f : r + 5.0f);
    }

    void sync()
    {
        const auto v = param.value();
        if (std::abs (v - shown) > 1.0e-9) { shown = v; repaint(); }

        if (ring.valid())
        {
            const auto rv = ring.value();
            if (std::abs (rv - ringShown) > 1.0e-9) { ringShown = rv; repaint(); }

            // how far the newest note's shot has thrown the cutoff now
            const auto sv = (double) ring.plugin->readSource ("key shot");
            if (std::abs (sv - shotShown) > 0.004) { shotShown = sv; repaint(); }
        }

        const auto m = modulation != nullptr ? modulation() : 0.0;
        if (std::abs (m - modShown) > 0.002) { modShown = m; repaint(); }
    }

    void paint (Graphics& g) override
    {
        const auto scaleFactor = juce::jmax (1.0f, juce::Component::getApproximateScaleFactorForComponent (this)) * 2.0f;
        if (base.isNull() || cachedScale != scaleFactor)
        {
            cachedScale = scaleFactor;
            base = render (true, scaleFactor);
            gloss = render (false, scaleFactor);
        }

        const auto area = getLocalBounds().toFloat();
        g.drawImage (base, area, juce::RectanglePlacement::stretchToFit);
        drawRing (g);
        drawPointer (g);
        g.drawImage (gloss, area, juce::RectanglePlacement::stretchToFit);

        // its caption: the value while it is being turned or pointed at
        const bool live = isMouseOverOrDragging();
        const bool onRing = ring.valid() && (ringDrag || (isMouseOver() && overRing (getMouseXYRelative().toFloat())));
        const auto text = onRing ? "KEY SHOT " + ring.text (ringShown)
                                 : def.named ? param.text (shown).toUpperCase() : (live ? param.text (shown) : juce::String (def.label));
        silkText (g, text, Rect (0.0f, margin + def.captionDrop, (float) getWidth(), 15.0f), 12.0f, live ? Colour (0xffffe4b0) : tok::silk, 0.14f);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        ringDrag = overRing (e.position);
        dragTravel = ringDrag ? ring.toTravel (ringShown) : param.toTravel (shown);
        dragY = e.position.y;
    }

    void mouseUp (const juce::MouseEvent&) override   { ringDrag = false; repaint(); }
    void mouseMove (const juce::MouseEvent&) override { if (ring.valid()) repaint(); }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (ringDrag)
        {
            const double span = e.mods.isShiftDown() || e.mods.isCtrlDown() ? 900.0 : 220.0;
            ring.set (ring.fromTravel (dragTravel + (dragY - e.position.y) / span));
            ringShown = ring.value();
            repaint();
            return;
        }

        if (! param.valid()) return;
        const auto& s = param.spec();
        const double span = e.mods.isShiftDown() || e.mods.isCtrlDown() ? 900.0 : 220.0;
        auto t = dragTravel + (dragY - e.position.y) / span;
        if (s.stepped)
        {
            const auto steps = juce::jmax (1.0, s.maxValue - s.minValue);
            t = std::round (t * steps) / steps;
        }
        apply (param.fromTravel (t));
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (overRing (e.position)) { ring.set (ring.spec().defaultValue); ringShown = ring.value(); repaint(); return; }
        if (param.valid()) apply (param.spec().defaultValue);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (overRing (e.position))
        {
            ring.set (ring.fromTravel (ring.toTravel (ringShown) + (double) w.deltaY * 0.05));
            ringShown = ring.value();
            repaint();
            return;
        }

        if (! param.valid()) return;
        const auto& s = param.spec();
        if (s.stepped)
            apply (BuiltinPlugin::quantised (s, shown + (w.deltaY > 0 ? 1.0 : -1.0)));
        else
            apply (param.fromTravel (param.toTravel (shown) + (double) w.deltaY * 0.05));
    }

private:
    void apply (double v)
    {
        param.set (v);
        shown = param.value();
        repaint();
    }

    // where it points: its value, moved by the LFOs routed to it
    float angleNow() const  { return angleAt (param.toTravel (shown) + modShown); }
    static float angleAt (double travel) { return -135.0f + 270.0f * (float) juce::jlimit (0.0, 1.0, travel); }

    // the ring: between the skirt and the disc's edge
    bool overRing (juce::Point<float> p) const
    {
        const auto d = centre().getDistanceFrom (p);
        return ring.valid() && d > r + 1.0f && d <= discR + 4.0f;
    }

    /** While an LFO moves it, where the knob was set: a white notch across the disc's rim.
        The cutoff's key shot: a dark track round the skirt, a yellow knob on it where a key throws
        the cutoff (it follows the cutoff knob, either side of it), a faint yellow path from the
        knob to it, and a red marker running along that path as the newest note's shot opens the
        filter and closes it again (inside the yellow knob while it's all the way open). */
    void drawRing (Graphics& g) const
    {
        const auto c = centre();

        if (std::abs (modShown) > 0.005)
        {
            const auto a = angleAt (param.toTravel (shown));
            g.setColour (juce::Colours::white.withAlpha (0.92f));
            g.drawLine (juce::Line<float> (polar (c, r + 1.0f, a), polar (c, discR - 0.5f, a)), 2.2f);
        }

        if (! ring.valid())
            return;

        const float radius = (r + discR) * 0.5f, thickness = discR - r - 3.0f;
        const auto arc = [&] (double from, double to)
        {
            juce::Path p;
            p.addCentredArc (c.x, c.y, radius, radius, 0.0f, juce::degreesToRadians (angleAt (from)), juce::degreesToRadians (angleAt (to)), true);
            return p;
        };
        const juce::PathStrokeType stroke (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.strokePath (arc (0.0, 1.0), stroke);

        const auto from = juce::jlimit (0.0, 1.0, param.toTravel (shown)), amount = ringShown / 100.0;
        const auto target = juce::jlimit (0.0, 1.0, from + amount);

        if (std::abs (target - from) > 1.0e-3)
        {
            g.setColour (Colour (glassYellow).withAlpha (0.30f));
            g.strokePath (arc (juce::jmin (from, target), juce::jmax (from, target)), stroke);

            // the shot now: red, from the cutoff knob toward the yellow one
            const auto now = from + (target - from) * juce::jlimit (0.0, 1.0, shotShown);
            if (std::abs (now - from) > 1.0e-3)
            {
                g.setColour (Colour (0xffe0352b).withAlpha (0.85f));
                g.strokePath (arc (juce::jmin (from, now), juce::jmax (from, now)), juce::PathStrokeType (thickness * 0.45f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }

        // the yellow knob: a small round cap, lit from the upper left
        const auto at = polar (c, radius, angleAt (target));
        const float kr = thickness * 0.5f + 2.6f;
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (Rect (2 * kr, 2 * kr).withCentre (at.translated (0.6f, 1.1f)));
        juce::ColourGradient cap (Colour (glassYellow).brighter (0.45f), at.x - kr * 0.6f, at.y - kr * 0.6f,
                                  Colour (glassYellow).darker (0.35f), at.x + kr * 0.7f, at.y + kr * 0.7f, true);
        g.setGradientFill (cap);
        g.fillEllipse (Rect (2 * kr, 2 * kr).withCentre (at));
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawEllipse (Rect (2 * kr, 2 * kr).withCentre (at), 0.9f);

        // the red marker inside it: lit as the shot arrives there
        const auto lit = (float) juce::jlimit (0.0, 1.0, shotShown);
        if (std::abs (target - from) > 1.0e-3 && lit > 0.02f)
        {
            g.setColour (Colour (0xffe0352b).withAlpha (0.25f + 0.75f * lit));
            g.fillEllipse (Rect (kr * 1.0f, kr * 1.0f).withCentre (at));
        }
    }
    float capRadius() const { return r * 0.66f; }

    Colour markColour() const
    {
        switch (def.mark)
        {
            case Mark::red:   return tok::markRed;
            case Mark::blue:  return tok::markBlue;
            case Mark::slate: break;
        }
        return tok::markSlate;
    }

    // Where the scale's ticks go: one per position on a stepped knob, else eleven round the sweep.
    int tickCount() const
    {
        if (param.valid() && param.spec().stepped)
        {
            const auto steps = (int) std::lround (param.spec().maxValue - param.spec().minValue);
            if (steps >= 1 && steps <= 24) return steps + 1;   // a wave or a filter type: one each
        }
        return 11;
    }

    juce::Image render (bool under, float scaleFactor) const
    {
        const auto w = juce::jmax (1, juce::roundToInt ((float) getWidth() * scaleFactor)), h = juce::jmax (1, juce::roundToInt ((float) getHeight() * scaleFactor));
        juce::Image img (juce::Image::ARGB, w, h, true);
        Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (scaleFactor));
        const auto c = centre();
        const float rc = capRadius();
        const auto capRect = Rect (2 * rc, 2 * rc).withCentre (c);

        if (! under)
        {
            // gloss: the air between the eye and the cap (above the pointer)
            juce::Path cap; cap.addEllipse (capRect);
            g.reduceClipRegion (cap);
            const auto lp = c + juce::Point<float> (-0.30f * rc, -0.44f * rc);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.34f), lp.x, lp.y, juce::Colours::white.withAlpha (0.0f), lp.x + rc * 0.9f, lp.y, true));
            g.fillEllipse (capRect);
            return img;
        }

        // the marker disc: printed on the plate (flat), a thin dark edge
        const auto disc = Rect (2 * discR, 2 * discR).withCentre (c);
        g.setColour (markColour()); g.fillEllipse (disc);
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.drawEllipse (disc.reduced (0.5f), 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f)); g.drawEllipse (disc.reduced (1.6f), 0.8f);

        // scale: ticks and numbers round the disc
        g.setColour (tok::silk.withAlpha (0.85f));
        const int ticks = tickCount();
        for (int k = 0; k < ticks; ++k)
        {
            const float a = -135.0f + 270.0f * (float) k / (float) (ticks - 1);
            g.drawLine (juce::Line<float> (polar (c, discR + 3.0f, a), polar (c, discR + 8.0f, a)), 1.5f);
        }
        g.setFont (barlow (9.5f, true, 0.02f));
        g.setColour (tok::silkDim);
        for (int i = 0; i < scale.size(); ++i)
        {
            if (scale[i].isEmpty()) continue;
            const float a = scale.size() == 1 ? 0.0f : -135.0f + 270.0f * (float) i / (float) (scale.size() - 1);
            g.drawText (scale[i], Rect (60.0f, 12.0f).withCentre (polar (c, discR + (def.size == KnobSize::mini ? 15.0f : 17.0f), a)), Justification::centred, false);
        }

        // cast shadow: long and soft, down and to the right, then a tight dark one where it sits
        roundShadow (g, c + juce::Point<float> (r * 0.26f, r * 0.46f), r * 1.42f, 0.62f);
        roundShadow (g, c + juce::Point<float> (r * 0.08f, r * 0.16f), r * 1.16f, 0.60f);

        // skirt: dark grey, knurled; rim light above, seat below
        const auto skirt = Rect (2 * r, 2 * r).withCentre (c);
        g.setColour (Colour (0xff3d4045)); g.fillEllipse (skirt);
        {
            juce::Path ridges;
            for (float a = 0.0f; a < 360.0f; a += 6.0f)
                ridges.addPieSegment (skirt, juce::degreesToRadians (a), juce::degreesToRadians (a + 3.0f), 0.0f);
            g.setColour (Colour (0xff62666c)); g.fillPath (ridges);
        }
        {
            // the skirt's own shading: lit upper left, falling away lower right
            juce::ColourGradient lit (juce::Colours::white.withAlpha (0.22f), c.x - r * 0.7f, c.y - r * 0.7f, juce::Colours::black.withAlpha (0.42f), c.x + r * 0.7f, c.y + r * 0.7f, false);
            lit.addColour (0.5, juce::Colours::transparentBlack);
            g.setGradientFill (lit); g.fillEllipse (skirt);
            g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawEllipse (skirt.reduced (0.4f), 0.9f);
        }

        // the cap sits in a shallow well: a dark ring, then machined metal
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillEllipse (capRect.expanded (2.2f).translated (0.6f, 1.2f));
        fillConicMetal (g, capRect, 20.0f);
        {
            juce::ColourGradient bevel (juce::Colours::white.withAlpha (0.55f), c.x - rc * 0.7f, c.y - rc * 0.7f, juce::Colours::black.withAlpha (0.45f), c.x + rc * 0.7f, c.y + rc * 0.7f, false);
            bevel.addColour (0.5, juce::Colours::transparentWhite);
            g.setGradientFill (bevel);
            g.drawEllipse (capRect.reduced (0.8f), 1.5f);
        }
        return img;
    }

    void drawPointer (Graphics& g) const
    {
        const auto c = centre();
        const float a = angleNow(), rc = capRadius();
        // a dark line across the cap, and a white index on the skirt so it reads from a distance
        juce::Path line; line.startNewSubPath (polar (c, rc * 0.18f, a)); line.lineTo (polar (c, rc * 0.94f, a));
        g.setColour (Colour (0xff18191b));
        g.strokePath (line, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::Path index; index.startNewSubPath (polar (c, rc + 3.0f, a)); index.lineTo (polar (c, r - 1.5f, a));
        g.setColour (Colour (0xfffbf8f0));
        g.strokePath (index, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    ParamRef param, ring;
    double ringShown = 0.0, modShown = 0.0, shotShown = 0.0;
    bool ringDrag = false;
    KnobDef def;
    juce::StringArray scale;
    float r, discR, margin;
    double shown = 0.0, dragTravel = 0.0;
    float dragY = 0.0f;
    juce::Image base, gloss;
    float cachedScale = 0.0f;
};

//==============================================================================
// A cream push button that lights (amber, or red for keys and stage switches).
class PushButton final : public juce::Component
{
public:
    PushButton (juce::String text, bool redLamp) : label (std::move (text)), red (redLamp)
    {
        setRepaintsOnMouseActivity (true);
        setTitle (label);
    }

    std::function<bool()> isOn;
    std::function<void()> onPress;
    bool momentary = false;

    juce::Rectangle<int> boundsAt (int x, int y) const { return { x - 35, y - 32, 70, 70 }; }
    Rect cap() const { return Rect (30.0f, 26.0f).withCentre ({ 35.0f, 32.0f }); }

    bool hitTest (int x, int y) override { return cap().expanded (4.0f).contains ((float) x, (float) y); }

    void sync()
    {
        const bool on = isOn != nullptr && isOn();
        if (on != shownOn) { shownOn = on; repaint(); }
    }

    void paint (Graphics& g) override
    {
        const bool down = isMouseButtonDown();
        const bool on = shownOn || (momentary && down);
        const auto b = cap().translated (0.0f, down ? 1.0f : 0.0f);
        const Colour glow = red ? Colour (0xffff5032) : Colour (0xffffaa32);
        boxShadow (g, cap().expanded (2.0f), 4.0f, down ? 2.0f : 4.5f, 0.55f);
        if (on)
        {
            g.setGradientFill (juce::ColourGradient (glow.withAlpha (0.55f), b.getCentreX(), b.getCentreY(), glow.withAlpha (0.0f), b.getCentreX() + 30.0f, b.getCentreY(), true));
            g.fillEllipse (b.expanded (16.0f));
        }
        g.setColour (Colour (0xff24262a)); g.fillRoundedRectangle (cap().expanded (2.5f), 4.0f);
        if (on)
        {
            juce::ColourGradient lit (red ? Colour (0xffffe2dc) : Colour (0xfffff7e0), b.getCentreX(), b.getCentreY() - 1.0f,
                                      red ? Colour (0xffd0281a) : Colour (0xffff9a2a), b.getRight(), b.getBottom(), true);
            lit.addColour (0.42, red ? Colour (0xffff6a50) : Colour (0xffffcf6e));
            g.setGradientFill (lit);
        }
        else
        {
            g.setGradientFill (juce::ColourGradient (Colour (0xfff6efdc), 0.0f, b.getY(), Colour (0xffd3c7aa), 0.0f, b.getBottom(), false));
        }
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (on ? 0.5f : 0.9f)); g.fillRect (b.withHeight (1.0f).reduced (2.0f, 0.0f));
        g.setColour (juce::Colours::black.withAlpha (0.18f)); g.fillRect (b.withTop (b.getBottom() - 2.0f).reduced (2.0f, 0.0f));
        if (isMouseOver() && ! on) { g.setColour (juce::Colours::white.withAlpha (0.12f)); g.fillRoundedRectangle (b, 3.0f); }
        silkText (g, label, Rect (0.0f, 52.0f, 70.0f, 14.0f), 11.0f, tok::silk, 0.14f);
    }

    void mouseDown (const juce::MouseEvent&) override { repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        repaint();
        if (e.mouseWasClicked() && hitTest (e.x, e.y) && onPress != nullptr)
            onPress();
    }

private:
    juce::String label;
    bool red, shownOn = false;
};

//==============================================================================
// Cream VU meter reading gain reduction: needle right at rest, left as it works.
class VuMeter final : public juce::Component
{
public:
    VuMeter (const VuDef& d, std::function<float()> readDb) : def (d), read (std::move (readDb))
    {
        for (auto* m : d.marks) { marks.add (m); markDb.add (juce::String (m).getFloatValue()); }
        setInterceptsMouseClicks (false, false);
    }

    static constexpr int shadowRoom = 14;   // the component is this much bigger than the meter, for its shadow
    juce::Rectangle<int> boundsFor() const { return juce::Rectangle<int> (def.x, def.y, def.w, def.h).expanded (shadowRoom); }

    void sync()
    {
        const auto target = travelFor (read != nullptr ? read() : 0.0f);
        const auto next = needle + (target - needle) * 0.28f;      // VU ballistics, about 300 ms
        if (std::abs (next - needle) > 0.0005f) { needle = next; repaint(); }
    }

    void paint (Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced ((float) shadowRoom);
        boxShadow (g, b, 8.0f, 7.0f, 0.6f);
        g.setGradientFill (juce::ColourGradient (Colour (0xff33363b), 0.0f, b.getY(), Colour (0xff141518), 0.0f, b.getBottom(), false));
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (juce::Colours::white.withAlpha (0.18f)); g.fillRect (Rect (b.getX() + 8.0f, b.getY() + 1.0f, b.getWidth() - 16.0f, 1.0f));

        const auto face = b.reduced (9.0f);
        {
            Graphics::ScopedSaveState s (g);
            juce::Path clip; clip.addRoundedRectangle (face, 4.0f); g.reduceClipRegion (clip);
            juce::ColourGradient cream (Colour (0xfffff6dd), face.getCentreX(), face.getY() + face.getHeight() * 1.2f, Colour (0xffd9c48f), face.getCentreX(), face.getY() - face.getHeight() * 0.1f, true);
            cream.addColour (0.55, Colour (0xfff1e2b8));
            g.setGradientFill (cream); g.fillRect (face);

            const float fw = face.getWidth(), fh = face.getHeight(), R = fw * 0.5f, cx = face.getX() + fw * 0.5f, cy = face.getY() + fh * 0.34f + R;
            const juce::Point<float> pivot (cx, cy);
            const float a0 = -48.0f, a1 = 48.0f;
            juce::Path arc; arc.addCentredArc (cx, cy, R, R, 0.0f, juce::degreesToRadians (a0), juce::degreesToRadians (a1), true);
            g.setColour (Colour (0xff1b1b1b)); g.strokePath (arc, juce::PathStrokeType (1.6f));
            g.setFont (barlow (12.0f, true));
            for (int i = 0; i < marks.size(); ++i)
            {
                const float a = a0 + (a1 - a0) * (float) i / (float) juce::jmax (1, marks.size() - 1);
                g.drawLine (juce::Line<float> (polar (pivot, R, a), polar (pivot, R + 9.0f, a)), 1.6f);
                g.drawText (marks[i], Rect (40.0f, 14.0f).withCentre (polar (pivot, R + 19.0f, a)), Justification::centred, false);
            }
            silkText (g, def.title, Rect (face.getX(), face.getY() + fh * 0.66f - 10.0f, fw, 16.0f), 13.0f, Colour (0xff1b1b1b), 0.25f);

            // needle, with its shadow on the card
            const float an = a0 + (a1 - a0) * needle;
            const auto n0 = polar (pivot, cy - face.getBottom() + 2.0f, an), n1 = polar (pivot, R + 6.0f, an);
            g.setColour (juce::Colours::black.withAlpha (0.22f)); g.drawLine (juce::Line<float> (n0.translated (2.0f, 3.0f), n1.translated (2.0f, 3.0f)), 2.2f);
            g.setColour (Colour (0xff111111)); g.drawLine (juce::Line<float> (n0, n1), 1.6f);
            juce::Path capPath; capPath.addCentredArc (cx, face.getBottom(), fh * 0.2f, fh * 0.2f, 0.0f, -pi * 0.5f, pi * 0.5f, true); capPath.closeSubPath();
            g.setColour (Colour (0xff2a2a2a)); g.fillPath (capPath);

            // the bezel's shadow on the card (the light is upper left)
            g.setGradientFill (juce::ColourGradient (Colour (0x80402800), face.getX(), face.getY(), juce::Colours::transparentBlack, face.getX() + 16.0f, face.getY() + 16.0f, false));
            g.fillRect (face);
        }
        juce::Path glare;
        glare.startNewSubPath (face.getX(), face.getY()); glare.lineTo (face.getX() + face.getWidth() * 0.62f, face.getY());
        glare.lineTo (face.getX(), face.getY() + face.getHeight() * 0.58f); glare.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.30f), face.getX(), face.getY(), juce::Colours::white.withAlpha (0.04f), face.getX() + 70.0f, face.getY() + 55.0f, false));
        g.fillPath (glare);
    }

private:
    // dB of reduction to the needle's travel across the printed scale (its marks are not evenly spaced in dB)
    float travelFor (float db) const
    {
        const int n = markDb.size();
        if (n < 2) return 1.0f;
        if (db >= markDb[0]) return 0.0f;
        for (int i = 1; i < n; ++i)
            if (db >= markDb[i])
                return ((float) (i - 1) + (markDb[i - 1] - db) / (markDb[i - 1] - markDb[i])) / (float) (n - 1);
        return 1.0f;
    }

    VuDef def;
    std::function<float()> read;
    juce::StringArray marks;
    juce::Array<float> markDb;
    float needle = 1.0f;
};

//==============================================================================
// Two LED columns: the output, 3 dB a segment, the top one at 0 dBFS.
class LedMeter final : public juce::Component
{
public:
    // The top light is the clip light: it lights only when a sample went past 0 dBFS (the plugin
    // counts every over, so none falls between two looks) and stays lit 1.5 s. The lights under
    // it are 3 dB apart, the highest at -3 dBFS.
    LedMeter (int segments, std::function<float (int)> peak, std::function<float (int)> overCount)
        : segs (segments), read (std::move (peak)), overs (std::move (overCount)) { setInterceptsMouseClicks (false, false); }

    void sync()
    {
        const auto now = juce::Time::getMillisecondCounter();
        int l[2];
        for (int ch = 0; ch < 2; ++ch)
        {
            const float p = read != nullptr ? read (ch) : 0.0f;
            const float db = p > 1.0e-5f ? 20.0f * std::log10 (p) : -100.0f;
            l[ch] = juce::jlimit (0, segs - 1, (int) std::floor ((db + 3.0f * (float) (segs - 1)) / 3.0f) + 1);

            const float count = overs != nullptr ? overs (ch) : 0.0f;
            if (count != lastOvers[ch]) { if (seenOvers) clipUntil[ch] = now + 1500; lastOvers[ch] = count; }
            const bool c = now < clipUntil[ch];
            if (c != clip[ch]) { clip[ch] = c; repaint(); }
        }
        seenOvers = true;   // (overs from before the panel opened don't count)
        if (l[0] != lit[0] || l[1] != lit[1]) { lit[0] = l[0]; lit[1] = l[1]; repaint(); }
    }

    void paint (Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colours::white.withAlpha (0.20f)); g.fillRoundedRectangle (b.translated (0.0f, 1.0f), 4.0f);
        g.setColour (Colour (0xff0b0c0d)); g.fillRoundedRectangle (b, 4.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black, 0.0f, 0.0f, juce::Colours::transparentBlack, 0.0f, 7.0f, false));
        g.fillRoundedRectangle (b, 4.0f);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < segs; ++i)
            {
                const bool on = i == segs - 1 ? clip[ch] : i < lit[ch];
                const Colour c = i == segs - 1 ? Colour (0xffff4a32) : (i >= segs - 3 ? Colour (0xffffd34a) : Colour (0xff5ee36b));
                const Rect seg (5.0f + (float) ch * 12.0f, b.getHeight() - 5.0f - 5.0f - (float) i * 7.0f, 9.0f, 5.0f);
                if (on) { g.setColour (c.withAlpha (0.35f)); g.fillRoundedRectangle (seg.expanded (1.5f), 2.0f); }
                g.setColour (on ? c : c.withMultipliedSaturation (0.5f).withMultipliedBrightness (0.22f)); g.fillRoundedRectangle (seg, 1.0f);
            }
    }

private:
    int segs, lit[2] = { 0, 0 };
    bool clip[2] = { false, false }, seenOvers = false;
    float lastOvers[2] = { 0.0f, 0.0f };
    juce::uint32 clipUntil[2] = { 0, 0 };
    std::function<float (int)> read, overs;
};

//==============================================================================
// The preset strip's window: arrows step, a click on the name opens the list.
class PresetTape final : public juce::Component
{
public:
    PresetTape() { setRepaintsOnMouseActivity (true); setTitle ("Presets"); }

    std::function<void (int)> choose;
    std::function<void (PresetAction)> act;
    std::function<void()> open;      // the preset window, in place of the drop list

    void setMenu (const PresetMenu& m) { if (! (m == menu)) { menu = m; repaint(); } }

    void paint (Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colours::white.withAlpha (0.20f)); g.fillRoundedRectangle (b.translated (0.0f, 1.0f), 4.0f);
        g.setColour (tok::tape); g.fillRoundedRectangle (b, 4.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black, 0.0f, 0.0f, juce::Colours::transparentBlack, 0.0f, 8.0f, false));
        g.fillRoundedRectangle (b, 4.0f);
        const auto mouse = getMouseXYRelative();
        const bool over = isMouseOver();
        g.setFont (barlow (13.0f, true));
        g.setColour (over && mouse.x < 30 ? Colour (0xfffff3d6) : Colour (0xffc9bfa3)); g.drawText (juce::String::fromUTF8 ("\xe2\x97\x80"), b.withWidth (30.0f), Justification::centred, false);
        g.setColour (over && mouse.x > getWidth() - 30 ? Colour (0xfffff3d6) : Colour (0xffc9bfa3)); g.drawText (juce::String::fromUTF8 ("\xe2\x96\xb6"), b.withTrimmedLeft (b.getWidth() - 30.0f), Justification::centred, false);
        g.setFont (mono (15.0f)); g.setColour (tok::tapeText);
        g.drawFittedText (menu.currentText(), b.reduced (32.0f, 0.0f).toNearestInt(), Justification::centred, 1, 0.8f);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mouseWasClicked()) return;
        if (e.x < 30 || e.x > getWidth() - 30)
        {
            const auto next = menu.step (e.x < 30 ? -1 : 1);
            if (next >= 0 && choose != nullptr) choose (next);
            return;
        }
        if (open != nullptr) { open(); return; }
        juce::PopupMenu m;
        PresetMenus::addPresets (m, menu, 1);
        m.addSeparator();
        PresetMenus::addActions (m, menu, 10000);
        juce::Component::SafePointer<PresetTape> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int id)
        {
            if (safe == nullptr || id <= 0) return;
            if (id >= 10000) { if (safe->act != nullptr) safe->act (static_cast<PresetAction> (id - 10000)); }
            else if (safe->choose != nullptr) safe->choose (id - 1);
        });
    }

private:
    PresetMenu menu;
};

//==============================================================================
// The preset window: over the whole plate, the plate dimmed round it. Categories on the left (a
// factory preset's own, then My presets and each bank), the chosen category's presets on the right
// as a grid of buttons (it scrolls when there are more than fit), and under the grid what can be
// done with presets. A click on a preset puts it on (the window stays, to try the next); a
// double-click puts it on and closes the window, as do Esc, CLOSE and a click outside it.
class PresetWindow final : public juce::Component
{
public:
    std::function<void (int)> choose;
    std::function<void (PresetAction)> act;

    PresetWindow()
    {
        setTitle ("Presets");
        setWantsKeyboardFocus (true);
        grid.owner = this;
        viewport.setViewedComponent (&grid, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (8);
        viewport.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, Colour (glassYellow).withAlpha (0.45f));
        addAndMakeVisible (viewport);
        setVisible (false);
    }

    void setMenu (const PresetMenu& m)
    {
        if (m == menu)
            return;

        menu = m;
        categories.clear();
        for (const auto& c : menu.categories)
            categories.addIfNotAlreadyThere (c);

        if (! categories.contains (shownCategory))
            shownCategory = currentCategory();

        fill();
        repaint();
    }

    void open()
    {
        shownCategory = currentCategory();
        fill();
        setVisible (true);
        toFront (true);
        grabKeyboardFocus();
        scrollToCurrent();
    }

    void close() { setVisible (false); }

    /** Shows a category's presets (as a click on it would). */
    void showCategory (const juce::String& c) { shownCategory = c; fill(); viewport.setViewPosition (0, 0); repaint(); }

    //==========================================================================
    void resized() override
    {
        const auto b = box();
        window = b;
        const int top = b.getY() + headerH, bottom = b.getBottom() - footerH;
        categoryArea = { b.getX() + pad, top, categoryW, bottom - top };
        const int gx = categoryArea.getRight() + pad * 2;
        viewport.setBounds (gx, top, b.getRight() - pad - gx, bottom - top - 12);
        actionsArea = { gx, bottom, b.getRight() - pad - gx, footerH - pad };
        fill();
    }

    void paint (Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.55f));

        const auto w = window.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (w.translated (0.0f, 8.0f).expanded (6.0f), 14.0f);
        g.setGradientFill (juce::ColourGradient (Colour (0xff0a0a0b), 0.0f, w.getY(), Colour (0xff141518), 0.0f, w.getBottom(), false));
        g.fillRoundedRectangle (w, 10.0f);
        g.setColour (juce::Colours::white.withAlpha (0.16f)); g.drawRoundedRectangle (w.reduced (0.5f), 10.0f, 1.2f);
        g.setColour (Colour (0xff0b0c0d)); g.drawRoundedRectangle (w.expanded (2.0f), 12.0f, 4.0f);

        const Colour yellow (glassYellow);

        // the header: PRESETS, the one that's on, CLOSE
        const auto header = Rect (w.getX() + (float) pad, w.getY(), w.getWidth() - (float) pad * 2.0f, (float) headerH - 14.0f);
        silkText (g, "PRESETS", header, 20.0f, tok::silk, 0.32f, Justification::centredLeft);
        g.setFont (mono (17.0f)); g.setColour (tok::tapeText);
        g.drawFittedText (menu.currentText(), header.reduced (220.0f, 0.0f).toNearestInt(), Justification::centred, 1, 0.8f);
        const auto closeArea = closeButton().toFloat();
        g.setColour (hoverClose ? yellow.withAlpha (0.18f) : Colour (0xff1a1c1f)); g.fillRoundedRectangle (closeArea, 4.0f);
        g.setColour (yellow.withAlpha (hoverClose ? 1.0f : 0.6f)); g.drawRoundedRectangle (closeArea.reduced (0.5f), 4.0f, 1.0f);
        g.setFont (barlow (14.0f, true, 0.12f)); g.setColour (yellow);
        g.drawText (juce::String::fromUTF8 ("CLOSE  \xc3\x97"), closeArea, Justification::centred, false);
        g.setColour (yellow.withAlpha (0.35f));
        g.fillRect (Rect (w.getX() + (float) pad, (float) (window.getY() + headerH) - 12.0f, w.getWidth() - (float) pad * 2.0f, 1.0f));

        // the categories
        silkText (g, "CATEGORY", Rect ((float) categoryArea.getX(), (float) categoryArea.getY() - 2.0f, (float) categoryW, 14.0f), 11.0f, tok::silk.withAlpha (0.5f), 0.3f, Justification::centredLeft);
        for (int i = 0; i < categories.size(); ++i)
        {
            const auto area = categoryButton (i).toFloat();
            const bool on = categories[i] == shownCategory;
            if (on)                      { g.setColour (yellow); g.fillRoundedRectangle (area, 4.0f); }
            else if (i == hoverCategory) { g.setColour (yellow.withAlpha (0.16f)); g.fillRoundedRectangle (area, 4.0f); }
            g.setColour (on ? Colour (0xff111111) : yellow);
            g.setFont (barlow (15.0f, true, 0.12f));
            g.drawText (categories[i].toUpperCase(), area.reduced (12.0f, 0.0f), Justification::centredLeft, false);
            g.setFont (barlow (13.0f, false));
            g.setColour (on ? Colour (0xff111111).withAlpha (0.7f) : yellow.withAlpha (0.55f));
            g.drawText (juce::String (menu.categories.strings.isEmpty() ? 0 : countOf (categories[i])), area.reduced (12.0f, 0.0f), Justification::centredRight, false);
        }

        // the line between the columns
        g.setColour (yellow.withAlpha (0.22f));
        g.fillRect (Rect ((float) (categoryArea.getRight() + pad), (float) categoryArea.getY(), 1.0f, (float) categoryArea.getHeight() + (float) footerH - (float) pad));

        // the actions, under the grid
        g.setColour (yellow.withAlpha (0.22f));
        g.fillRect (Rect ((float) actionsArea.getX(), (float) actionsArea.getY() - 1.0f, (float) actionsArea.getWidth(), 1.0f));
        for (int i = 0; i < numActions; ++i)
        {
            const auto area = actionButton (i).toFloat();
            const bool enabled = actionEnabled (i);
            g.setColour (i == hoverAction && enabled ? yellow.withAlpha (0.18f) : Colour (0xff1a1c1f)); g.fillRoundedRectangle (area, 4.0f);
            g.setColour (yellow.withAlpha (enabled ? (i == hoverAction ? 1.0f : 0.6f) : 0.18f)); g.drawRoundedRectangle (area.reduced (0.5f), 4.0f, 1.0f);
            g.setFont (barlow (14.0f, true, 0.12f)); g.setColour (yellow.withAlpha (enabled ? 1.0f : 0.3f));
            g.drawText (actions[i].label, area, Justification::centred, false);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto p = e.getPosition();
        int c = -1, a = -1;
        for (int i = 0; i < categories.size(); ++i) if (categoryButton (i).contains (p)) c = i;
        for (int i = 0; i < numActions; ++i) if (actionButton (i).contains (p)) a = i;
        const bool x = closeButton().contains (p);
        if (c != hoverCategory || a != hoverAction || x != hoverClose) { hoverCategory = c; hoverAction = a; hoverClose = x; repaint(); }
        setMouseCursor (c >= 0 || (a >= 0 && actionEnabled (a)) || x ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }

    void mouseExit (const juce::MouseEvent&) override { hoverCategory = hoverAction = -1; hoverClose = false; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = e.getPosition();

        if (! window.contains (p) || closeButton().contains (p)) { close(); return; }

        for (int i = 0; i < categories.size(); ++i)
            if (categoryButton (i).contains (p)) { shownCategory = categories[i]; fill(); viewport.setViewPosition (0, 0); repaint(); return; }

        for (int i = 0; i < numActions; ++i)
            if (actionButton (i).contains (p) && actionEnabled (i))
            {
                if (actions[i].closes) close();
                if (act != nullptr) act (actions[i].action);
                return;
            }
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey) { close(); return true; }
        return false;
    }

private:
    //==========================================================================
    // The grid of presets: buttons in columns, the one that's on lit.
    struct Grid final : public juce::Component
    {
        PresetWindow* owner = nullptr;
        std::vector<int> indices;      // the presets shown, by their index in the menu
        int hovered = -1, columns = 4;
        static constexpr int buttonH = 40, gap = 8;

        juce::Rectangle<int> button (int i) const
        {
            const int w = (getWidth() - gap * (columns - 1)) / juce::jmax (1, columns);
            return { (i % columns) * (w + gap), (i / columns) * (buttonH + gap), w, buttonH };
        }

        int at (juce::Point<int> p) const
        {
            for (int i = 0; i < (int) indices.size(); ++i) if (button (i).contains (p)) return i;
            return -1;
        }

        void paint (Graphics& g) override
        {
            const Colour yellow (glassYellow);
            for (int i = 0; i < (int) indices.size(); ++i)
            {
                const auto area = button (i).toFloat();
                const int index = indices[(size_t) i];
                const bool on = index == owner->menu.current;
                if (on) { g.setColour (yellow); g.fillRoundedRectangle (area, 4.0f); }
                else
                {
                    g.setColour (i == hovered ? Colour (0xff24262a) : Colour (0xff1a1c1f)); g.fillRoundedRectangle (area, 4.0f);
                    g.setColour (yellow.withAlpha (i == hovered ? 0.95f : 0.4f)); g.drawRoundedRectangle (area.reduced (0.5f), 4.0f, 1.0f);
                }
                g.setFont (barlow (15.0f, true, 0.06f));
                g.setColour (on ? Colour (0xff111111) : (i == hovered ? yellow.brighter (0.3f) : yellow));
                g.drawFittedText (owner->menu.names[index].toUpperCase(), area.reduced (10.0f, 0.0f).toNearestInt(), Justification::centred, 1, 0.75f);
            }
        }

        void mouseMove (const juce::MouseEvent& e) override { const int i = at (e.getPosition()); if (i != hovered) { hovered = i; repaint(); } }
        void mouseExit (const juce::MouseEvent&) override   { hovered = -1; repaint(); }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (const int i = at (e.getPosition()); i >= 0 && owner->choose != nullptr)
                owner->choose (indices[(size_t) i]);
        }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (at (e.getPosition()) >= 0)
                owner->close();
        }
    };

    struct Action { const char* label; PresetAction action; bool closes; };
    static constexpr int numActions = 7;
    const Action actions[numActions] { { "SAVE", PresetAction::save, false }, { "SAVE AS", PresetAction::saveAs, false },
                                       { "LOAD", PresetAction::loadFile, false }, { "SAVE BANK", PresetAction::saveBank, false },
                                       { "LOAD BANK", PresetAction::loadBank, false }, { "DELETE", PresetAction::remove, false },
                                       { "SHOW FOLDER", PresetAction::showFolder, false } };

    bool actionEnabled (int i) const
    {
        switch (actions[i].action)
        {
            case PresetAction::save:   return menu.canSave;
            case PresetAction::remove: return menu.canDelete;
            default:                   return true;
        }
    }

    static constexpr int pad = 24, headerH = 70, footerH = 78, categoryW = 250;

    juce::Rectangle<int> box() const
    {
        return getLocalBounds().reduced (juce::jmax (40, getWidth() / 14), juce::jmax (30, getHeight() / 14));
    }

    juce::Rectangle<int> closeButton() const  { return { window.getRight() - pad - 110, window.getY() + 14, 110, 30 }; }
    juce::Rectangle<int> categoryButton (int i) const { return { categoryArea.getX(), categoryArea.getY() + 18 + i * 44, categoryW, 38 }; }

    juce::Rectangle<int> actionButton (int i) const
    {
        const int gap = 8, w = (actionsArea.getWidth() - gap * (numActions - 1)) / numActions;
        return { actionsArea.getX() + i * (w + gap), actionsArea.getY() + 16, w, 38 };
    }

    juce::String currentCategory() const
    {
        if (menu.current >= 0 && menu.current < menu.categories.size())
            return menu.categories[menu.current];
        return categories.isEmpty() ? juce::String() : categories[0];
    }

    int countOf (const juce::String& category) const
    {
        int n = 0;
        for (const auto& c : menu.categories) if (c == category) ++n;
        return n;
    }

    void fill()
    {
        grid.indices.clear();
        for (int i = 0; i < menu.names.size(); ++i)
            if (i < menu.categories.size() && menu.categories[i] == shownCategory)
                grid.indices.push_back (i);

        const int width = juce::jmax (100, viewport.getWidth() - viewport.getScrollBarThickness() - 4);
        grid.columns = juce::jmax (1, (width + Grid::gap) / (200 + Grid::gap));
        const int rows = ((int) grid.indices.size() + grid.columns - 1) / grid.columns;
        grid.setSize (width, juce::jmax (1, rows * (Grid::buttonH + Grid::gap) - Grid::gap));
        grid.repaint();
    }

    void scrollToCurrent()
    {
        for (int i = 0; i < (int) grid.indices.size(); ++i)
            if (grid.indices[(size_t) i] == menu.current)
            {
                const auto b = grid.button (i);
                if (b.getBottom() > viewport.getViewHeight())
                    viewport.setViewPosition (0, b.getBottom() - viewport.getViewHeight());
            }
    }

    PresetMenu menu;
    juce::StringArray categories;
    juce::String shownCategory;
    juce::Viewport viewport;
    Grid grid;
    juce::Rectangle<int> window, categoryArea, actionsArea;
    int hoverCategory = -1, hoverAction = -1;
    bool hoverClose = false;
};

//==============================================================================
// Cayman's keyboard: plays the plugin's panel notes and shows what sounds.
class Keys final : public juce::Component
{
public:
    Keys (BuiltinPlugin& p, int oct) : plugin (p), octaves (oct) { setTitle ("Keyboard"); }

    static constexpr int lowest = 36;   // C2

    void sync() { int now = 0; for (int n = lowest; n <= lowest + octaves * 12; ++n) if (plugin.isPanelNoteDown (n)) now = now * 31 + n; if (now != held) { held = now; repaint(); } }

    void paint (Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setColour (Colour (0xff0c0c0d)); g.fillRoundedRectangle (b, 4.0f);
        const int whites = octaves * 7 + 1;
        const float kw = b.getWidth() / (float) whites;
        static const int whiteSemis[7] = { 0, 2, 4, 5, 7, 9, 11 };
        for (int i = 0; i < whites; ++i)
        {
            const int note = lowest + (i / 7) * 12 + whiteSemis[i % 7];
            const Rect key ((float) i * kw + 1.0f, 4.0f, kw - 2.0f, b.getHeight() - 8.0f);
            juce::ColourGradient gw (Colour (0xffe9e6dd), 0.0f, key.getY(), Colour (0xffd9d5ca), 0.0f, key.getBottom(), false);
            gw.addColour (0.7, Colour (0xfffbfaf6));
            g.setGradientFill (gw); g.fillRoundedRectangle (key, 2.0f);
            if (plugin.isPanelNoteDown (note)) { g.setColour (Colour (0x66ff9a2a)); g.fillRoundedRectangle (key, 2.0f); }
        }
        static const bool hasBlack[7] = { true, true, false, true, true, true, false };
        for (int i = 0; i < whites - 1; ++i)
        {
            if (! hasBlack[i % 7]) continue;
            const int note = lowest + (i / 7) * 12 + whiteSemis[i % 7] + 1;
            const Rect key ((float) (i + 1) * kw - kw * 0.3f, 4.0f, kw * 0.6f, b.getHeight() * 0.6f);
            g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (key.translated (2.0f, 3.0f), 2.0f);
            juce::ColourGradient gb (Colour (0xff2a2b2e), 0.0f, key.getY(), Colour (0xff3a3b3f), 0.0f, key.getBottom(), false);
            gb.addColour (0.85, Colour (0xff0f1011));
            g.setGradientFill (gb); g.fillRoundedRectangle (key, 2.0f);
            if (plugin.isPanelNoteDown (note)) { g.setColour (Colour (0x88ff9a2a)); g.fillRoundedRectangle (key, 2.0f); }
        }
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), 0.0f, 4.0f, juce::Colours::transparentBlack, 0.0f, 16.0f, false));
        g.fillRect (b.reduced (1.0f, 4.0f).withHeight (12.0f));
    }

    void mouseDown (const juce::MouseEvent& e) override { play (noteAt (e.position)); }
    void mouseDrag (const juce::MouseEvent& e) override { const auto n = noteAt (e.position); if (n != playing) play (n); }
    void mouseUp (const juce::MouseEvent&) override { play (-1); }

private:
    int noteAt (juce::Point<float> p) const
    {
        const auto b = getLocalBounds().toFloat();
        const int whites = octaves * 7 + 1;
        const float kw = b.getWidth() / (float) whites;
        static const int whiteSemis[7] = { 0, 2, 4, 5, 7, 9, 11 };
        static const bool hasBlack[7] = { true, true, false, true, true, true, false };
        if (p.y < b.getHeight() * 0.6f + 4.0f)
            for (int i = 0; i < whites - 1; ++i)
                if (hasBlack[i % 7] && std::abs (p.x - (float) (i + 1) * kw) < kw * 0.3f)
                    return lowest + (i / 7) * 12 + whiteSemis[i % 7] + 1;
        const int i = juce::jlimit (0, whites - 1, (int) (p.x / kw));
        return lowest + (i / 7) * 12 + whiteSemis[i % 7];
    }

    void play (int note)
    {
        if (playing >= 0) plugin.panelNote (playing, 0.0f);
        playing = note;
        if (playing >= 0) plugin.panelNote (playing, 0.8f);
        repaint();
    }

    BuiltinPlugin& plugin;
    int octaves, playing = -1, held = 0;
};

//==============================================================================
// Cayman's wheels: the Plugins Creator gallery's "Pitch and mod wheels", in the synth's colours (the
// keyboard's black housing, slate wheels, the plate's red mark). The left one bends and goes back to
// the middle when it's let go; the right one is the mod wheel and stays. They follow the synth's own
// (a MIDI keyboard's wheels move them too).
class Wheels final : public juce::Component
{
public:
    explicit Wheels (BuiltinPlugin& p) : plugin (p)
    {
        setTitle ("Pitch and mod wheels");
        design = juce::JSON::parse (R"({ "kind": "wheels", "housingColour": "#FF0C0C0D", "partColour": "#FF2B3138", "accentColour": "#FFC8352A", "material": "Rubber" })");
    }

    void sync()
    {
        if (dragging >= 0)
            return;

        const auto b = 0.5f + 0.5f * plugin.readSource ("bend"), m = plugin.readSource ("mod wheel");
        if (std::abs (b - bend) > 0.004f || std::abs (m - mod) > 0.004f) { bend = b; mod = m; repaint(); }
    }

    void paint (Graphics& g) override
    {
        PerformControl::paint (g, design, getLocalBounds().toFloat(), bend, mod, 120.0f, 30.0f, 2.0f);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = PerformControl::partAt (design, getLocalBounds().toFloat(), e.position);
        from = dragging == 0 ? bend : mod;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging < 0)
            return;

        const auto v = juce::jlimit (0.0f, 1.0f, from - (float) e.getDistanceFromDragStartY() / ((float) getHeight() * 0.8f));
        if (dragging == 0) { bend = v; plugin.panelWheel (0, bend * 2.0f - 1.0f); }
        else               { mod = v;  plugin.panelWheel (1, mod); }
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging == 0) { bend = 0.5f; plugin.panelWheel (0, 0.0f); repaint(); }     // the bend springs back
        dragging = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (PerformControl::partAt (design, getLocalBounds().toFloat(), e.position) == 1) { mod = 0.0f; plugin.panelWheel (1, 0.0f); repaint(); }
    }

private:
    BuiltinPlugin& plugin;
    juce::var design;
    float bend = 0.5f, mod = 0.0f, from = 0.0f;
    int dragging = -1;
};

// A caption that belongs to one of the column's tabs (on the black glass).
class TabCaption final : public juce::Component
{
public:
    explicit TabCaption (const juce::String& t) : text (t) { setInterceptsMouseClicks (false, false); }
    void paint (Graphics& g) override { silkText (g, text, getLocalBounds().toFloat(), 11.0f, tok::silk.withAlpha (0.55f), 0.24f); }

private:
    juce::String text;
};

//==============================================================================
// The still artwork: rack ears, enamel plate, screws, brand, section frames and printed words.
class Backdrop final : public juce::Component
{
public:
    explicit Backdrop (const Layout& l) : L (l) { setInterceptsMouseClicks (false, false); setBufferedToImage (true); }

    static juce::Image& noise()
    {
        static juce::Image img = []
        {
            juce::Image n (juce::Image::ARGB, 96, 96, true);
            juce::Random rng (1234);
            for (int i = 0; i < 1100; ++i)
                n.setPixelAt (rng.nextInt (96), rng.nextInt (96), (i & 1) ? Colour (0x12ffffff) : Colour (0x17000000));
            return n;
        }();
        return img;
    }

    // The Sonify logo (assets/Sonify.png), cropped to its letters and halved down to twice the size
    // it is drawn at: one big downscale in drawImage would alias its thin serifs.
    static constexpr float logoW = 120.0f, logoH = 31.0f;

    static const juce::Image& logo()
    {
        static const juce::Image img = []
        {
            auto src = juce::ImageCache::getFromMemory (BinaryData::Sonify_png, BinaryData::Sonify_pngSize);
            if (! src.isValid())
                return juce::Image();
            src = src.getClippedImage (juce::Rectangle<int> (39, 31, 568, 146).getIntersection (src.getBounds()));
            const int targetW = juce::roundToInt (logoW * 2.0f);
            while (src.getWidth() >= targetW * 2)
                src = src.rescaled (src.getWidth() / 2, src.getHeight() / 2, Graphics::highResamplingQuality);
            return src.rescaled (targetW, juce::roundToInt ((float) targetW * (float) src.getHeight() / (float) src.getWidth()),
                                 Graphics::highResamplingQuality);
        }();
        return img;
    }

    void paint (Graphics& g) override
    {
        const float W = (float) L.W, H = (float) L.H, P0 = 30.0f, P1 = W - 30.0f;
        g.fillAll (tok::rack);

        for (int side = 0; side < 2; ++side)
        {
            const Rect ear (side == 0 ? 0.0f : P1, 0.0f, 30.0f, H);
            juce::ColourGradient eg (Colour (0xff26282c), ear.getX(), 0.0f, Colour (0xff222428), ear.getRight(), 0.0f, false);
            eg.addColour (0.55, Colour (0xff35383d));
            g.setGradientFill (eg); g.fillRect (ear);
            for (float sy : { 40.0f, H - 64.0f })
            {
                const Rect slot (ear.getX() + 9.0f, sy, 12.0f, 24.0f);
                g.setColour (juce::Colours::white.withAlpha (0.07f)); g.fillRoundedRectangle (slot.translated (0.0f, 1.0f), 6.0f);
                g.setColour (Colour (0xff0a0a0b)); g.fillRoundedRectangle (slot, 6.0f);
            }
        }

        const Rect plate (P0, 0.0f, P1 - P0, H);
        juce::ColourGradient pg (tok::enamelTop, 0.0f, 0.0f, tok::enamelBot, 0.0f, H, false);
        pg.addColour (0.42, tok::enamelMid);
        g.setGradientFill (pg); g.fillRect (plate);
        g.setFillType (juce::FillType (noise(), juce::AffineTransform()));
        g.fillRect (plate);
        g.setColour (juce::Colours::white.withAlpha (0.22f)); g.fillRect (plate.withHeight (1.0f));
        g.setColour (juce::Colours::white.withAlpha (0.08f)); g.fillRect (plate.withWidth (1.0f));
        g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRect (plate.withX (P1 - 1.0f).withWidth (1.0f));

        for (auto [sx, sy] : { std::pair<float, float> { P0 + 14.0f, 14.0f }, { P1 - 28.0f, 14.0f }, { P0 + 14.0f, H - 28.0f }, { P1 - 28.0f, H - 28.0f } })
        {
            const Rect s (sx, sy, 14.0f, 14.0f);
            roundShadow (g, s.getCentre().translated (1.5f, 2.5f), 11.0f, 0.55f);
            fillConicMetal (g, s, 40.0f);
            g.setColour (Colour (0xff2a2c2f));
            g.drawLine (juce::Line<float> (polar (s.getCentre(), 5.0f, 58.0f), polar (s.getCentre(), 5.0f, 238.0f)), 2.0f);
        }

        // brand plate: the Sonify logo, a red bar under it, the model beside it
        const Rect logoArea (P0 + 36.0f, 18.0f, logoW, logoH);
        if (const auto& img = logo(); img.isValid())
        {
            Graphics::ScopedSaveState s (g);
            g.setImageResamplingQuality (Graphics::highResamplingQuality);
            g.drawImage (img, logoArea, juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid);
        }
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRect (Rect (logoArea.getX() + 2.0f, 53.0f, logoW - 4.0f, 3.0f));
        g.setColour (tok::red); g.fillRect (Rect (logoArea.getX() + 2.0f, 52.0f, logoW - 4.0f, 3.0f));
        const float textX = logoArea.getRight() + 14.0f;
        silkText (g, L.model, Rect (textX, 22.0f, 400.0f, 14.0f), 12.0f, tok::silkDim, 0.32f, Justification::centredLeft);
        silkText (g, L.title, Rect (textX, 38.0f, 400.0f, 18.0f), 15.0f, tok::silk, 0.22f, Justification::centredLeft);

        // power lamp (the plugin's on: the header's ON switch is its bypass)
        {
            const juce::Point<float> c (P1 - 64.0f, 35.0f);
            roundShadow (g, c.translated (1.5f, 2.5f), 13.0f, 0.5f);
            g.setColour (Colour (0xff2a2c30)); g.fillEllipse (Rect (16.0f, 16.0f).withCentre (c));
            g.setGradientFill (juce::ColourGradient (Colour (0x99ff4a32), c.x, c.y, Colour (0x00ff4a32), c.x + 16.0f, c.y, true));
            g.fillEllipse (Rect (32.0f, 32.0f).withCentre (c));
            juce::ColourGradient lamp (Colour (0xffffe1d8), c.x - 1.5f, c.y - 2.0f, Colour (0xffa3170a), c.x + 6.0f, c.y + 6.0f, true);
            lamp.addColour (0.45, Colour (0xffff4a32));
            g.setGradientFill (lamp); g.fillEllipse (Rect (12.0f, 12.0f).withCentre (c));
            silkText (g, "POWER", Rect (c.x - 40.0f, 49.0f, 80.0f, 12.0f), 10.0f, tok::silk, 0.16f);
        }
        g.setColour (tok::silkLine); g.fillRect (Rect (P0 + 22.0f, 76.0f, W - 52.0f - (P0 + 22.0f), 1.5f));

        // sections: printed outlines with their titles
        for (const auto& s : L.sections)
        {
            const Rect r ((float) s.x, (float) s.y, (float) s.w, (float) s.h);
            g.setColour (tok::silkLine); g.drawRoundedRectangle (r, 6.0f, 1.5f);
            const auto f = barlow (12.0f, true, 0.3f);
            const float tw = juce::GlyphArrangement::getStringWidth (f, s.title) + 16.0f;
            g.setColour (plateColourAt (r.getY(), H)); g.fillRect (Rect (tw, 18.0f).withCentre ({ r.getCentreX(), r.getY() }));
            silkText (g, s.title, Rect (tw, 18.0f).withCentre ({ r.getCentreX(), r.getY() }), 12.0f, tok::silk, 0.3f);
        }

        for (const auto& t : L.texts)
            if (t.tab < 0)
                silkText (g, t.text, Rect ((float) t.x - 80.0f, (float) t.y, 160.0f, 12.0f), 10.0f, tok::silkDim, 0.24f);

        // the black glass: a dark bezel with a light edge under it, the glass darkest at its top
        if (! L.glass.isEmpty())
        {
            const juce::PathStrokeType bezel (4.0f, juce::PathStrokeType::curved);
            g.setColour (juce::Colours::white.withAlpha (0.18f)); g.strokePath (L.glass, bezel, juce::AffineTransform::translation (0.0f, 1.2f));
            g.setColour (Colour (0xff0b0c0d)); g.strokePath (L.glass, bezel);
            const auto area = L.glass.getBounds();
            g.setGradientFill (juce::ColourGradient (Colour (0xff0a0a0b), 0.0f, area.getY(), Colour (0xff131416), 0.0f, area.getBottom(), false));
            g.fillPath (L.glass);

            if (L.rule.getLength() > 0.0f)
            {
                g.setColour (Colour (glassYellow).withAlpha (0.25f));
                g.drawLine (L.rule, 1.0f);
            }
        }
    }

private:
    const Layout& L;
};

//==============================================================================
// A line of page buttons on the black glass: the chosen one yellow with black letters, the others
// yellow letters on the glass.
class PageSelector final : public juce::Component
{
public:
    explicit PageSelector (const std::vector<const char*>& names, const juce::String& title = "Page", bool small = false)
        : fontHeight (small ? 12.0f : 13.0f)
    {
        const int height = small ? 24 : selectorHeight;
        int x = 0;
        for (auto* item : names)
        {
            items.add (item);
            const int w = small ? juce::roundToInt ((float) juce::String (item).length() * 7.4f + 16.0f) : selectorItemWidth (item);
            areas.add (juce::Rectangle<int> (x, 0, w, height));
            x += w + selectorItemGap;
        }
        setSize (juce::jmax (1, x - selectorItemGap), height);
        setTitle (title);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    std::function<void (int)> onChange;

    /** Lights a button without telling anyone (the value came from the parameter). */
    void show (int index)
    {
        index = juce::jlimit (0, items.size() - 1, index);
        if (index != selected) { selected = index; repaint(); }
    }

    void select (int index)
    {
        index = juce::jlimit (0, items.size() - 1, index);
        if (index != selected) { selected = index; repaint(); }
        if (onChange != nullptr) onChange (selected);
    }

    void paint (Graphics& g) override
    {
        const Colour yellow (glassYellow);
        g.setFont (barlow (fontHeight, true, 0.12f));

        for (int i = 0; i < items.size(); ++i)
        {
            const auto area = areas[i].toFloat();
            if (i == selected)
            {
                g.setColour (yellow); g.fillRoundedRectangle (area, 4.0f);
                g.setColour (Colour (0xff111111));
            }
            else
            {
                if (i == hovered) { g.setColour (yellow.withAlpha (0.16f)); g.fillRoundedRectangle (area, 4.0f); }
                g.setColour (i == hovered ? yellow.brighter (0.3f) : yellow);
            }
            g.drawText (items[i], area, Justification::centred, false);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override   { hover (itemAt (e.getPosition())); }
    void mouseExit (const juce::MouseEvent&) override     { hover (-1); }
    void mouseDown (const juce::MouseEvent& e) override   { if (const int i = itemAt (e.getPosition()); i >= 0) select (i); }

private:
    int itemAt (juce::Point<int> p) const
    {
        for (int i = 0; i < areas.size(); ++i)
            if (areas[i].contains (p)) return i;
        return -1;
    }

    void hover (int i) { if (i != hovered) { hovered = i; repaint(); } }

    juce::StringArray items;
    juce::Array<juce::Rectangle<int>> areas;
    int selected = 0, hovered = -1;
    float fontHeight = 13.0f;
};

//==============================================================================
// A value in a square on the black glass, its name under it. Dragged up and down to change it
// (Shift or Ctrl for fine), the wheel steps it, a double-click puts it back; one with two values
// flips on a click. A `toggle` one is drawn as a switch instead.
class TextSquare final : public juce::Component
{
public:
    TextSquare (ParamRef p, const SquareDef& d) : param (p), def (d)
    {
        setTitle (d.label);
        setRepaintsOnMouseActivity (true);
        setMouseCursor (isSwitch() ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::UpDownResizeCursor);
        shown = param.value();
    }

    juce::Rectangle<int> boundsFor() const { return { def.x, def.y, def.w, squareHeight + squareLabel }; }

    void sync()
    {
        const auto v = param.value();
        if (std::abs (v - shown) > 1.0e-9) { shown = v; repaint(); }
    }

    void paint (Graphics& g) override
    {
        const Colour yellow (glassYellow);
        const bool live = isMouseOverOrDragging();
        const Rect box (0.0f, 0.0f, (float) getWidth(), (float) squareHeight);

        if (def.toggle)
        {
            // a switch: the pill yellow and its handle to the right while it's on
            const bool on = shown > 0.5;
            const auto pill = Rect (44.0f, 22.0f).withCentre (box.getCentre());
            if (on) { g.setColour (live ? yellow.brighter (0.25f) : yellow); g.fillRoundedRectangle (pill, 11.0f); }
            else    { g.setColour (Colour (0xff1a1c1f)); g.fillRoundedRectangle (pill, 11.0f);
                      g.setColour (yellow.withAlpha (live ? 0.9f : 0.55f)); g.drawRoundedRectangle (pill.reduced (0.5f), 10.5f, 1.0f); }
            g.setColour (on ? Colour (0xff111214) : yellow);
            g.fillEllipse (Rect (17.0f, 17.0f).withCentre ({ on ? pill.getRight() - 11.0f : pill.getX() + 11.0f, pill.getCentreY() }));
            g.setFont (barlow (9.5f, true, 0.06f));
            g.setColour (on ? Colour (0xff111111) : yellow);
            g.drawText (on ? "ON" : "OFF", on ? pill.withTrimmedRight (20.0f) : pill.withTrimmedLeft (20.0f), Justification::centred, false);
        }
        else
        {
            g.setColour (Colour (0xff1a1c1f)); g.fillRoundedRectangle (box, 4.0f);
            g.setColour (yellow.withAlpha (live ? 0.95f : 0.45f)); g.drawRoundedRectangle (box.reduced (0.5f), 4.0f, 1.0f);
            g.setFont (barlow (14.0f, true, 0.06f));
            g.setColour (live ? yellow.brighter (0.3f) : yellow);
            g.drawFittedText (valueText(), box.reduced (3.0f, 0.0f).toNearestInt(), Justification::centred, 1, 0.75f);
        }

        silkText (g, def.label, Rect (-10.0f, (float) squareHeight + 3.0f, (float) getWidth() + 20.0f, 12.0f), 10.0f, tok::silkDim, 0.14f);
    }

    void mouseDown (const juce::MouseEvent& e) override { dragValue = shown; dragTravel = param.toTravel (shown); dragY = e.position.y; dragged = false; }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! param.valid() || isSwitch()) return;
        if (std::abs (e.position.y - dragY) > 3.0f) dragged = true;
        const auto& s = param.spec();
        const bool fine = e.mods.isShiftDown() || e.mods.isCtrlDown();

        if (s.stepped)
        {
            // each step a fixed distance of the hand: closer together the more steps there are
            const auto steps = juce::jmax (1.0, s.maxValue - s.minValue);
            const auto pixels = juce::jlimit (3.0, 14.0, 160.0 / steps) * (fine ? 3.0 : 1.0);
            apply (BuiltinPlugin::quantised (s, dragValue + std::round ((dragY - e.position.y) / pixels)));
        }
        else
        {
            apply (param.fromTravel (dragTravel + (dragY - e.position.y) / (fine ? 700.0 : 170.0)));
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (param.valid() && isSwitch() && ! dragged && e.mouseWasClicked())
            apply (shown > param.spec().minValue + 0.5 ? param.spec().minValue : param.spec().maxValue);
    }

    void mouseDoubleClick (const juce::MouseEvent&) override { if (param.valid() && ! isSwitch()) apply (param.spec().defaultValue); }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (! param.valid()) return;
        const auto& s = param.spec();
        if (s.stepped) apply (BuiltinPlugin::quantised (s, shown + (w.deltaY > 0 ? 1.0 : -1.0)));
        else           apply (param.fromTravel (param.toTravel (shown) + (double) w.deltaY * 0.05));
    }

private:
    // two values: a click flips it, there's nothing to drag
    bool isSwitch() const { return param.valid() && param.spec().stepped && std::abs (param.spec().maxValue - param.spec().minValue - 1.0) < 1.0e-6; }

    void apply (double v)
    {
        param.set (v);
        shown = param.value();
        repaint();
    }

    // a choice by its name, a number without its unit (the name under the square says what it is)
    juce::String valueText() const
    {
        if (! param.valid()) return {};
        const auto& s = param.spec();
        if (! s.choices.empty())
            return juce::String (s.choices[(size_t) juce::jlimit (0, (int) s.choices.size() - 1, (int) std::lround (shown - s.minValue))]).toUpperCase();
        const int decimals = BuiltinPlugin::decimalsFor (s, shown);
        return decimals <= 0 ? juce::String (juce::roundToInt (shown)) : juce::String (shown, decimals);
    }

    ParamRef param;
    SquareDef def;
    double shown = 0.0, dragValue = 0.0, dragTravel = 0.0;
    float dragY = 0.0f;
    bool dragged = false;
};

//==============================================================================
// The seed: six digit squares that are one number (as Domina's six knobs are), read most
// significant first, each dragged up and down from 0 to 9; and RANDOM, which rolls all six.
class SeedRow final : public juce::Component
{
public:
    static constexpr int digits = 6;

    explicit SeedRow (ParamRef p) : param (p)
    {
        setTitle ("Seed");
        shown = juce::jlimit (0, 999999, (int) std::lround (param.value()));
    }

    void sync()
    {
        const int v = juce::jlimit (0, 999999, (int) std::lround (param.value()));
        if (v != shown) { shown = v; repaint(); }
    }

    void paint (Graphics& g) override
    {
        const Colour yellow (glassYellow);
        silkText (g, "SEED", Rect (0.0f, 2.0f, 60.0f, 16.0f), 10.0f, tok::silkDim, 0.3f, Justification::centredLeft);

        const auto button = randomArea().toFloat();
        if (overRandom) { g.setColour (yellow.withAlpha (pressedRandom ? 0.45f : 0.18f)); g.fillRoundedRectangle (button, 4.0f); }
        g.setColour (yellow); g.drawRoundedRectangle (button.reduced (0.5f), 4.0f, 1.0f);
        g.setFont (barlow (11.0f, true, 0.12f));
        g.drawText ("RANDOM", button, Justification::centred, false);

        g.setFont (barlow (20.0f, true, 0.0f));
        for (int slot = 0; slot < digits; ++slot)
        {
            const auto box = digitArea (slot).toFloat();
            const bool live = slot == overDigit || slot == dragDigit;
            g.setColour (Colour (0xff1a1c1f)); g.fillRoundedRectangle (box, 4.0f);
            g.setColour (yellow.withAlpha (live ? 0.95f : 0.45f)); g.drawRoundedRectangle (box.reduced (0.5f), 4.0f, 1.0f);
            g.setColour (live ? yellow.brighter (0.3f) : yellow);
            g.drawText (juce::String (digitAt (slot)), box, Justification::centred, false);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override   { hover (e.getPosition()); }
    void mouseExit (const juce::MouseEvent&) override     { overDigit = -1; overRandom = false; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        hover (e.getPosition());
        dragDigit = overDigit;
        dragFrom = dragDigit >= 0 ? digitAt (dragDigit) : 0;
        dragY = e.position.y;
        pressedRandom = overRandom;
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragDigit >= 0)
            setDigit (dragDigit, dragFrom + juce::roundToInt ((dragY - e.position.y) / 12.0f));
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (pressedRandom && randomArea().contains (e.getPosition()))
        {
            auto& random = juce::Random::getSystemRandom();
            int value = 0;
            for (int i = 0; i < digits; ++i) value = value * 10 + random.nextInt (10);     // each digit its own draw
            set (value);
        }
        dragDigit = -1;
        pressedRandom = false;
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        for (int slot = 0; slot < digits; ++slot)
            if (digitArea (slot).contains (e.getPosition())) setDigit (slot, 0);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        for (int slot = 0; slot < digits; ++slot)
            if (digitArea (slot).contains (e.getPosition())) setDigit (slot, digitAt (slot) + (w.deltaY > 0 ? 1 : -1));
    }

private:
    juce::Rectangle<int> randomArea() const            { return { getWidth() - 70, 0, 70, 20 }; }
    juce::Rectangle<int> digitArea (int slot) const    { return { slot * 36, 28, 32, 38 }; }

    static int placeOf (int slot)                      { int p = 1; for (int i = slot; i < digits - 1; ++i) p *= 10; return p; }   // slot 0 is the hundred thousands
    int digitAt (int slot) const                       { return shown / placeOf (slot) % 10; }
    void setDigit (int slot, int digit)                { set (shown + (juce::jlimit (0, 9, digit) - digitAt (slot)) * placeOf (slot)); }

    void set (int value)
    {
        param.set ((double) juce::jlimit (0, 999999, value));
        shown = juce::jlimit (0, 999999, (int) std::lround (param.value()));
        repaint();
    }

    void hover (juce::Point<int> p)
    {
        int digit = -1;
        for (int slot = 0; slot < digits; ++slot)
            if (digitArea (slot).contains (p)) digit = slot;
        const bool random = randomArea().contains (p);
        if (digit != overDigit || random != overRandom) { overDigit = digit; overRandom = random; repaint(); }
        setMouseCursor (digit >= 0 ? juce::MouseCursor::UpDownResizeCursor : random ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }

    ParamRef param;
    int shown = 0, overDigit = -1, dragDigit = -1, dragFrom = 0;
    float dragY = 0.0f;
    bool overRandom = false, pressedRandom = false;
};

//==============================================================================
// A small control on the plate at a knob's corner. A choice: a dark window with the choice's name,
// which a click opens as a menu (the wheel steps through it). A light: a lamp and its word, which
// a click turns on and off.
class Chip final : public juce::Component
{
public:
    Chip (ParamRef p, const ChipDef& d) : param (p), def (d)
    {
        setTitle (d.light || d.curve ? juce::String (d.label) : juce::String (d.param));
        setRepaintsOnMouseActivity (true);
        setMouseCursor (d.curve ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::PointingHandCursor);
        shown = param.value();
        setBounds (d.x, d.y, d.w, 16);
    }

    void sync()
    {
        const auto v = param.value();
        if (std::abs (v - shown) > 1.0e-9) { shown = v; repaint(); }
    }

    void paint (Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const bool over = isMouseOver();

        if (def.light)
        {
            const bool on = shown > 0.5;
            const juce::Point<float> c (6.0f, b.getCentreY());
            g.setColour (Colour (0xff1c1d20)); g.fillEllipse (Rect (10.0f, 10.0f).withCentre (c));
            if (on)
            {
                g.setGradientFill (juce::ColourGradient (Colour (0x88ff4a32), c.x, c.y, Colour (0x00ff4a32), c.x + 9.0f, c.y, true));
                g.fillEllipse (Rect (18.0f, 18.0f).withCentre (c));
            }
            juce::ColourGradient lamp (on ? Colour (0xffffe1d8) : Colour (0xff6a3a34), c.x - 1.0f, c.y - 1.5f, on ? Colour (0xffc22a18) : Colour (0xff2c1614), c.x + 3.5f, c.y + 3.5f, true);
            g.setGradientFill (lamp); g.fillEllipse (Rect (7.0f, 7.0f).withCentre (c));
            silkText (g, def.label, b.withTrimmedLeft (13.0f), 10.0f, over ? Colour (0xffffe4b0) : tok::silk, 0.14f, Justification::centredLeft);
            return;
        }

        g.setColour (juce::Colours::white.withAlpha (0.20f)); g.fillRoundedRectangle (b.translated (0.0f, 1.0f), 3.0f);
        g.setColour (tok::tape); g.fillRoundedRectangle (b, 3.0f);
        g.setFont (barlow (10.0f, true, 0.1f));
        g.setColour (over ? Colour (0xfffff3d6) : tok::tapeText);
        g.drawText (def.curve ? juce::String (def.label) + "  " + curveName (shown) : choiceName ((int) std::lround (shown)), b, Justification::centred, false);
    }

    // a SHAPE: dragged up and down (Shift or Ctrl for fine), double-click for the straight line
    void mouseDown (const juce::MouseEvent& e) override { dragFrom = shown; dragY = e.position.y; dragged = false; }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! def.curve || ! param.valid()) return;
        if (std::abs (e.position.y - dragY) > 2.0f) dragged = true;
        const auto& s = param.spec();
        const auto span = (e.mods.isShiftDown() || e.mods.isCtrlDown() ? 600.0 : 200.0);
        apply (juce::jlimit (s.minValue, s.maxValue, std::round ((dragFrom + (dragY - e.position.y) / span * (s.maxValue - s.minValue)) * 10.0) / 10.0));
    }

    void mouseDoubleClick (const juce::MouseEvent&) override { if (def.curve && param.valid()) apply (param.spec().defaultValue); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mouseWasClicked() || ! param.valid()) return;
        const auto& s = param.spec();

        if (def.light)
        {
            apply (shown > 0.5 ? s.minValue : s.maxValue);
            return;
        }

        if (def.curve)
        {
            // a click (not a drag): Grex's three shapes, and the first My Plugin's
            if (dragged) return;
            juce::PopupMenu m;
            const std::pair<const char*, double> shapes[] { { "LOG (holds, then falls)", -4.0 }, { "LIN (a straight line)", 0.0 },
                                                            { "EXP (drops fast, then trails off)", 6.0 }, { "MY PLUGIN (EXP 6.9)", 6.9 } };
            for (int i = 0; i < 4; ++i)
                m.addItem (i + 1, shapes[i].first, true, std::abs (shown - shapes[i].second) < 0.05);
            juce::Component::SafePointer<Chip> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, shapes] (int id)
            {
                if (safe != nullptr && id > 0) safe->apply (shapes[id - 1].second);
            });
            return;
        }

        juce::PopupMenu m;
        for (int i = 0; i < (int) s.choices.size(); ++i)
            m.addItem (i + 1, s.choices[(size_t) i], true, i == (int) std::lround (shown - s.minValue));
        juce::Component::SafePointer<Chip> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int id)
        {
            if (safe != nullptr && id > 0) safe->apply (safe->param.spec().minValue + (id - 1));
        });
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (param.valid() && def.curve)
            apply (juce::jlimit (param.spec().minValue, param.spec().maxValue, shown + (w.deltaY > 0 ? 0.5 : -0.5)));
        else if (param.valid() && ! def.light)
            apply (BuiltinPlugin::quantised (param.spec(), shown + (w.deltaY > 0 ? 1.0 : -1.0)));
    }

private:
    void apply (double v)
    {
        param.set (v);
        shown = param.value();
        repaint();
    }

    juce::String choiceName (int index) const
    {
        if (! param.valid() || param.spec().choices.empty()) return {};
        const auto& choices = param.spec().choices;
        return juce::String (choices[(size_t) juce::jlimit (0, (int) choices.size() - 1, index - (int) std::lround (param.spec().minValue))]).toUpperCase();
    }

    static juce::String curveName (double k)
    {
        if (k < -0.05) return "LOG " + juce::String (-k, 1);
        if (k > 0.05)  return "EXP " + juce::String (k, 1);
        return "LIN";
    }

    ParamRef param;
    ChipDef def;
    double shown = 0.0, dragFrom = 0.0;
    float dragY = 0.0f;
    bool dragged = false;
};

//==============================================================================
// A step lane on the black glass: eight columns, each a vertical line with a handle on the choice
// it's set to (dragged up and down; a drag across sets the columns it passes), the marker moving
// over them as the plugin plays them, and under them a row of LED buttons (two a column) that a
// click turns on and off, the one playing lit brighter.
class StepLane final : public juce::Component
{
public:
    StepLane (BuiltinPlugin& p, const LaneDef& d) : plugin (p), def (d)
    {
        setTitle ("Steps");
        for (int i = 0; i < columnCount; ++i) columns.push_back (ParamRef::lookup (p, juce::String (d.columns) + juce::String (i + 1)));
        for (int i = 0; i < stepCount; ++i)   steps.push_back (ParamRef::lookup (p, juce::String (d.steps) + juce::String (i + 1)));
        setSize (d.w, d.h);
        sync();
    }

    void sync()
    {
        bool changed = false;
        for (int i = 0; i < columnCount; ++i)
        {
            const int v = columns[(size_t) i].valid() ? (int) std::lround (columns[(size_t) i].value()) : 0;
            if (v != shownColumns[(size_t) i]) { shownColumns[(size_t) i] = v; changed = true; }
        }
        int lit = 0;
        for (int i = 0; i < stepCount; ++i)
            if (steps[(size_t) i].valid() && steps[(size_t) i].value() > 0.5) lit |= 1 << i;
        const auto at = def.marker != nullptr ? (double) plugin.readSource (def.marker) : 0.0;
        if (lit != shownSteps || std::abs (at - marker) > 0.01 || changed) { shownSteps = lit; marker = at; repaint(); }
    }

    void paint (Graphics& g) override
    {
        const Colour yellow (glassYellow);
        const float laneBottom = laneHeight();

        // the choices' names down the left
        g.setFont (barlow (9.5f, true, 0.06f));
        g.setColour (tok::silkDim);
        for (int v = 0; v < levelCount(); ++v)
            g.drawText (def.levels[(size_t) v], Rect (0.0f, levelY (v) - 6.0f, (float) labelWidth - 3.0f, 12.0f), Justification::centredRight, false);

        // the column the marker is in, and the marker
        const auto lap = marker - 8.0 * std::floor (marker / 8.0);
        const int playing = juce::jlimit (0, columnCount - 1, (int) std::floor (lap));
        g.setColour (yellow.withAlpha (0.08f));
        g.fillRect (Rect ((float) labelWidth + (float) playing * columnWidth(), 0.0f, columnWidth(), laneBottom));

        // the lines, the path through the handles, the handles
        juce::Path path;
        for (int i = 0; i < columnCount; ++i)
        {
            const auto x = columnX (i);
            g.setColour (yellow.withAlpha (0.22f));
            g.fillRect (Rect (x - 0.5f, 2.0f, 1.0f, laneBottom - 4.0f));
            const juce::Point<float> handle (x, levelY (shownColumns[(size_t) i]));
            if (i == 0) path.startNewSubPath (handle); else path.lineTo (handle);
        }
        g.setColour (yellow.withAlpha (0.45f));
        g.strokePath (path, juce::PathStrokeType (1.2f));

        g.setFont (barlow (9.5f, true, 0.0f));
        for (int i = 0; i < columnCount; ++i)
        {
            const auto box = Rect (13.0f, 13.0f).withCentre ({ columnX (i), levelY (shownColumns[(size_t) i]) });
            g.setColour (i == playing ? yellow.brighter (0.35f) : yellow);
            g.fillEllipse (box);
            g.setColour (Colour (0xff111111));
            g.drawText (def.levels[(size_t) juce::jlimit (0, levelCount() - 1, shownColumns[(size_t) i])], box, Justification::centred, false);
        }

        const auto mx = (float) labelWidth + (float) lap * columnWidth();
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.fillRect (Rect (mx - 1.0f, 0.0f, 2.0f, laneBottom));

        // the gate's steps
        const int now = (int) std::floor (lap * 2.0) % stepCount;
        for (int i = 0; i < stepCount; ++i)
        {
            const auto led = stepArea (i);
            const bool on = (shownSteps >> i & 1) != 0;
            g.setColour (on ? (i == now ? yellow.brighter (0.45f) : yellow) : Colour (0xff1a1c1f));
            g.fillRoundedRectangle (led, 2.0f);
            g.setColour (i == now ? juce::Colours::white.withAlpha (0.9f) : yellow.withAlpha (on ? 0.0f : 0.45f));
            g.drawRoundedRectangle (led.reduced (0.5f), 2.0f, 1.0f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if ((float) e.y > laneHeight())
        {
            for (int i = 0; i < stepCount; ++i)
                if (stepArea (i).expanded (1.5f, 3.0f).contains (e.position) && steps[(size_t) i].valid())
                {
                    steps[(size_t) i].set (steps[(size_t) i].value() > 0.5 ? 0.0 : 1.0);
                    sync();
                }
            return;
        }
        drawing = true;
        setColumnAt (e.position);
    }

    void mouseDrag (const juce::MouseEvent& e) override { if (drawing) setColumnAt (e.position); }
    void mouseUp (const juce::MouseEvent&) override      { drawing = false; }

private:
    static constexpr int columnCount = 8, stepCount = 16, labelWidth = 14, ledHeight = 10;

    int levelCount() const        { return juce::jmax (1, (int) def.levels.size()); }
    float laneHeight() const      { return (float) getHeight() - (float) ledHeight - 7.0f; }
    float columnWidth() const     { return ((float) getWidth() - (float) labelWidth) / (float) columnCount; }
    float columnX (int i) const   { return (float) labelWidth + ((float) i + 0.5f) * columnWidth(); }
    float levelY (int v) const    { return 8.0f + (float) juce::jlimit (0, levelCount() - 1, v) * (laneHeight() - 16.0f) / (float) juce::jmax (1, levelCount() - 1); }

    Rect stepArea (int i) const
    {
        const auto w = columnWidth() * 0.5f;
        return Rect ((float) labelWidth + (float) i * w + 1.5f, (float) getHeight() - (float) ledHeight, w - 3.0f, (float) ledHeight);
    }

    void setColumnAt (juce::Point<float> p)
    {
        const int i = (int) std::floor ((p.x - (float) labelWidth) / columnWidth());
        if (i < 0 || i >= columnCount || ! columns[(size_t) i].valid())
            return;
        const auto spacing = (laneHeight() - 16.0f) / (float) juce::jmax (1, levelCount() - 1);
        const int v = juce::jlimit (0, levelCount() - 1, juce::roundToInt ((p.y - 8.0f) / spacing));
        if (v != shownColumns[(size_t) i]) { columns[(size_t) i].set ((double) v); sync(); }
    }

    BuiltinPlugin& plugin;
    LaneDef def;
    std::vector<ParamRef> columns, steps;
    std::array<int, columnCount> shownColumns { -1, -1, -1, -1, -1, -1, -1, -1 };
    int shownSteps = -1;
    double marker = 0.0;
    bool drawing = false;
};

//==============================================================================
// The matrix: the LFOs as buttons down its left side, a line, and down its right side every knob
// they can play, each lit (yellow, black letters) while the chosen LFO plays it. Several at once;
// a click turns one on or off. The list scrolls if it's ever taller than the room.
class MatrixPanel final : public juce::Component
{
public:
    MatrixPanel (BuiltinPlugin& p, const MatrixDef& d) : plugin (p), def (d)
    {
        setTitle ("Matrix");
        list.panel = this;
        list.setSize (d.w - listLeft, juce::jmax (1, (int) d.destinations.size() * rowPitch - rowGap));
        view.setViewedComponent (&list, false);
        view.setScrollBarsShown (true, false, true, false);
        view.setScrollBarThickness (6);
        view.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, Colour (glassYellow).withAlpha (0.6f));
        addAndMakeVisible (view);
        setSize (d.w, d.h);
        refresh();
    }

    void resized() override { view.setBounds (listLeft, 0, getWidth() - listLeft, getHeight()); }

    void sync()
    {
        int lit = 0;
        for (size_t i = 0; i < routes.size(); ++i)
            if (routes[i].valid() && routes[i].value() > 0.5) lit |= 1 << (int) i;
        if (lit != shown) { shown = lit; list.repaint(); }
    }

    void paint (Graphics& g) override
    {
        const Colour yellow (glassYellow);
        g.setFont (barlow (13.0f, true, 0.12f));

        for (int i = 0; i < (int) def.sources.size(); ++i)
        {
            const auto area = sourceArea (i).toFloat();
            if (i == source) { g.setColour (yellow); g.fillRoundedRectangle (area, 4.0f); g.setColour (Colour (0xff111111)); }
            else             { g.setColour (yellow); }
            g.drawText (def.sources[(size_t) i], area, Justification::centred, false);
        }

        g.setColour (yellow.withAlpha (0.25f));
        g.fillRect (Rect ((float) listLeft - 9.0f, 0.0f, 1.0f, (float) getHeight()));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < (int) def.sources.size(); ++i)
            if (sourceArea (i).contains (e.getPosition()) && i != source) { source = i; refresh(); repaint(); }
    }

private:
    static constexpr int listLeft = 82, rowHeight = 22, rowGap = 4, rowPitch = rowHeight + rowGap;

    juce::Rectangle<int> sourceArea (int i) const { return { 0, i * 32, listLeft - 18, 26 }; }

    /** The routes of the LFO chosen: "LFO 1 > Cutoff" and so on. */
    void refresh()
    {
        routes.clear();
        for (const auto& d : def.destinations)
            routes.push_back (ParamRef::lookup (plugin, juce::String (def.sources[(size_t) source]) + " > " + d.second));
        shown = -1;
        sync();
    }

    struct List final : public juce::Component
    {
        MatrixPanel* panel = nullptr;

        void paint (Graphics& g) override
        {
            const Colour yellow (glassYellow);
            g.setFont (barlow (12.0f, true, 0.12f));

            for (int i = 0; i < (int) panel->def.destinations.size(); ++i)
            {
                const auto area = Rect (0.0f, (float) (i * rowPitch), (float) getWidth() - 10.0f, (float) rowHeight);
                const bool on = (panel->shown >> i & 1) != 0;
                if (on)              { g.setColour (yellow); g.fillRoundedRectangle (area, 4.0f); }
                else if (i == over)  { g.setColour (yellow.withAlpha (0.16f)); g.fillRoundedRectangle (area, 4.0f); }
                g.setColour (on ? Colour (0xff111111) : yellow);
                g.drawText (panel->def.destinations[(size_t) i].first, area, Justification::centred, false);
            }
        }

        int rowAt (int y) const { const int i = y / rowPitch; return y % rowPitch < rowHeight && i < (int) panel->def.destinations.size() ? i : -1; }

        void mouseMove (const juce::MouseEvent& e) override { const int i = rowAt (e.y); if (i != over) { over = i; repaint(); } }
        void mouseExit (const juce::MouseEvent&) override   { over = -1; repaint(); }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (! e.mouseWasClicked()) return;
            const int i = rowAt (e.y);
            if (i < 0 || ! panel->routes[(size_t) i].valid()) return;
            const auto& ref = panel->routes[(size_t) i];
            ref.set (ref.value() > 0.5 ? 0.0 : 1.0);
            panel->sync();
        }

        int over = -1;
    };

    BuiltinPlugin& plugin;
    MatrixDef def;
    std::vector<ParamRef> routes;
    int source = 0, shown = -1;
    List list;
    juce::Viewport view;
};

//==============================================================================
class Plate final : public juce::Component
{
public:
    Plate (BuiltinPlugin& p, const Layout& layout) : plugin (p), L (layout), backdrop (L)
    {
        addAndMakeVisible (backdrop);

        // meters first (under the knobs' shadow margins), then buttons, then knobs
        for (const auto& v : L.vus)
        {
            const juce::String source (v.source);
            const auto kind = v.kind;
            auto* p2 = &plugin;
            auto* vu = vus.add (new VuMeter (v, [p2, source, kind]
            {
                const float x = p2->readSource (source);
                switch (kind)
                {
                    case GrSource::compressor:    return -20.0f * std::log10 (juce::jmax (1.0e-4f, x * (1.4125375f - 0.1f) + 0.1f));   // vuReduction, undone
                    case GrSource::perTwelve:     return x * 12.0f;
                    case GrSource::perTwentyFour: return x * 24.0f;
                }
                return 0.0f;
            }));
            addAndMakeVisible (vu);
            vu->setBounds (vu->boundsFor());
        }

        for (const auto& l : L.leds)
        {
            auto* p2 = &plugin;
            auto* led = leds.add (new LedMeter (l.segs, [p2] (int ch) { return p2->readSource (ch == 0 ? "out L" : "out R"); },
                                                [p2] (int ch) { return p2->readSource (ch == 0 ? "over L" : "over R"); }));
            addAndMakeVisible (led);
            const auto size = ledSize (l.segs);
            led->setBounds (l.x, l.y, size.x, size.y);
        }

        for (const auto& b : L.buttons)
        {
            auto* button = buttons.add (new PushButton (b.label, b.red));
            const auto ref = ParamRef::find (plugin, b.param);
            const auto onValue = b.onValue;
            button->isOn = [ref, onValue] { return onValue < 0 ? ref.value() > 0.5 : std::abs (ref.value() - onValue) < 0.5; };
            button->onPress = [ref, onValue] { ref.set (onValue < 0 ? (ref.value() > 0.5 ? 0.0 : 1.0) : (double) onValue); };
            addAndMakeVisible (button);
            button->setBounds (button->boundsAt (b.x, b.y));
        }

        for (const auto& k : L.knobs)
        {
            auto* knob = knobs.add (new Knob (ParamRef::find (plugin, k.param), k));
            addAndMakeVisible (knob);
            knob->setBounds (knob->boundsFor());
        }

        for (int i = 0; i < knobs.size(); ++i)
        {
            const auto& k = L.knobs[(size_t) i];
            paged.push_back ({ knobs[i], k.page, k.tab, k.when != nullptr ? ParamRef::find (plugin, k.when) : ParamRef(), k.whenOn });
        }

        // The knobs the matrix can play move with the LFOs routed to them, as far as each LFO's
        // depth reaches (at 100 %, half the knob's travel either way), as the synth moves them.
        for (const auto& d : L.matrix.destinations)
        {
            for (int i = 0; i < knobs.size(); ++i)
            {
                if (juce::String (L.knobs[(size_t) i].param) != d.second)
                    continue;

                std::vector<std::tuple<ParamRef, ParamRef, juce::String>> from;
                for (auto* source : L.matrix.sources)
                    from.push_back ({ ParamRef::lookup (plugin, juce::String (source) + " > " + d.second),
                                      ParamRef::lookup (plugin, juce::String (source) + " Depth"), juce::String (source).toLowerCase() });

                auto* p2 = &plugin;
                knobs[i]->modulation = [from, p2]
                {
                    double offset = 0.0;
                    for (const auto& [routed, depth, source] : from)
                        if (routed.valid() && depth.valid() && routed.value() > 0.5)
                            offset += depth.value() / 100.0 * 0.5 * (double) p2->readSource (source);
                    return offset;
                };
            }
        }

        for (const auto& c : L.chips)
            addAndMakeVisible (chips.add (new Chip (ParamRef::find (plugin, c.param), c)));

        for (const auto& lane : L.lanes)
        {
            auto* l = lanes.add (new StepLane (plugin, lane));
            addAndMakeVisible (l);
            l->setTopLeftPosition (lane.x, lane.y);
            paged.push_back ({ l, lane.page, -1 });
        }

        if (L.matrix.w > 0)
        {
            matrix = std::make_unique<MatrixPanel> (plugin, L.matrix);
            addAndMakeVisible (*matrix);
            matrix->setTopLeftPosition (L.matrix.x, L.matrix.y);
            paged.push_back ({ matrix.get(), -1, L.matrix.tab });
        }

        // a parameter's values as a line of buttons
        for (const auto& c : L.choices)
        {
            auto* buttonsLine = choices.add (new PageSelector (c.items, c.param, true));
            const auto ref = ParamRef::find (plugin, c.param);
            choiceParams.push_back (ref);
            buttonsLine->onChange = [ref] (int index) { if (ref.valid()) ref.set (ref.spec().minValue + index); };
            addAndMakeVisible (buttonsLine);
            buttonsLine->setTopLeftPosition (c.cx - buttonsLine->getWidth() / 2, c.y - buttonsLine->getHeight() / 2);
            paged.push_back ({ buttonsLine, -1, c.tab });
        }

        for (const auto& q : L.squares)
        {
            auto* square = squares.add (new TextSquare (ParamRef::find (plugin, q.param), q));
            addAndMakeVisible (square);
            square->setBounds (square->boundsFor());
            paged.push_back ({ square, q.page, q.tab });
        }

        if (L.seed.param != nullptr)
        {
            seed = std::make_unique<SeedRow> (ParamRef::find (plugin, L.seed.param));
            addAndMakeVisible (*seed);
            seed->setBounds (L.seed.x, L.seed.y, seedWidth, seedHeight);
            paged.push_back ({ seed.get(), -1, L.seed.tab });
        }

        if (! L.selector.items.empty())
        {
            selector = std::make_unique<PageSelector> (L.selector.items);
            addAndMakeVisible (*selector);
            selector->setTopLeftPosition (L.selector.x, L.selector.y - selectorHeight / 2);
            selector->onChange = [this] (int page) { shownPage = page; showPaged(); };
            selector->show (shownPage);
        }

        if (! L.tabs.items.empty())
        {
            tabs = std::make_unique<PageSelector> (L.tabs.items, "Tab");
            addAndMakeVisible (*tabs);
            tabs->setTopLeftPosition (L.tabs.x, L.tabs.y - selectorHeight / 2);
            tabs->onChange = [this] (int tab) { shownTab = tab; showPaged(); };
            tabs->show (shownTab);
        }

        showPaged();

        if (L.keys.octaves > 0)
        {
            keys = std::make_unique<Keys> (plugin, L.keys.octaves);
            addAndMakeVisible (*keys);
            keys->setBounds (L.keys.x, L.keys.y, L.keys.w, L.keys.h);
        }

        if (L.wheels.w > 0)
        {
            wheels = std::make_unique<Wheels> (plugin);
            addAndMakeVisible (*wheels);
            wheels->setBounds (L.wheels.x, L.wheels.y, L.wheels.w, L.wheels.h);
        }

        for (const auto& t : L.texts)
        {
            if (t.tab < 0)
                continue;

            auto* caption = captions.add (new TabCaption (t.text));
            addAndMakeVisible (caption);
            caption->setBounds (t.x - 110, t.y, 220, 14);
            paged.push_back ({ caption, -1, t.tab });
        }

        showPaged();

        // the preset strip: tape window and its three push buttons
        const int P1 = L.W - 30, tx = juce::jmin (L.W / 2 - 40, P1 - 110 - 400);
        addAndMakeVisible (tape);
        tape.setBounds (tx, 20, 250, 30);
        const std::pair<const char*, PresetAction> actions[] = { { "SAVE", PresetAction::saveAs }, { "LOAD", PresetAction::loadFile }, { "QUICK", PresetAction::save } };
        for (int i = 0; i < (L.presetWindow ? 0 : 3); ++i)      // a preset window has them
        {
            auto* button = buttons.add (new PushButton (actions[i].first, false));
            button->momentary = true;
            const auto action = actions[i].second;
            button->onPress = [this, action] { if (onAction != nullptr) onAction (action); };
            addAndMakeVisible (button);
            button->setBounds (button->boundsAt (tx + 282 + i * 48, 35));
        }

        if (L.presetWindow)
        {
            tape.setBounds (tape.getBounds().withWidth (330));
            addChildComponent (presetWindow);
            presetWindow.setBounds (0, 0, L.W, L.H);
            tape.open = [this] { presetWindow.open(); };
        }

        setSize (L.W, L.H);
    }

    std::function<void (PresetAction)> onAction;
    PresetTape tape;
    PresetWindow presetWindow;

    void resized() override { backdrop.setBounds (getLocalBounds()); }

    void sync()
    {
        showPaged();     // a SYNC switched (by a preset too) swaps the knob in its place
        for (auto* k : knobs) k->sync();
        for (auto* q : squares) q->sync();
        if (seed != nullptr) seed->sync();
        if (matrix != nullptr) matrix->sync();
        for (auto* l : lanes) l->sync();
        for (auto* c : chips) c->sync();
        for (int i = 0; i < choices.size(); ++i)
            if (choiceParams[(size_t) i].valid())
                choices[i]->show ((int) std::lround (choiceParams[(size_t) i].value() - choiceParams[(size_t) i].spec().minValue));
        for (auto* b : buttons) b->sync();
        for (auto* v : vus) v->sync();
        for (auto* l : leds) l->sync();
        if (keys != nullptr) keys->sync();
        if (wheels != nullptr) wheels->sync();
    }

private:
    BuiltinPlugin& plugin;
    const Layout& L;
    Backdrop backdrop;
    juce::OwnedArray<VuMeter> vus;
    juce::OwnedArray<LedMeter> leds;
    juce::OwnedArray<PushButton> buttons;
    juce::OwnedArray<Knob> knobs;
    // What a selector's page or a tab shows: each control on the page and the tab it belongs to.
    struct Paged { juce::Component* component; int page, tab; ParamRef when {}; bool whenOn = true; };

    void showPaged()
    {
        for (const auto& p : paged)
            p.component->setVisible ((p.page < 0 || p.page == shownPage) && (p.tab < 0 || p.tab == shownTab)
                                     && (! p.when.valid() || (p.when.value() > 0.5) == p.whenOn));
    }

    juce::OwnedArray<PageSelector> choices;
    std::vector<ParamRef> choiceParams;
    juce::OwnedArray<TextSquare> squares;
    std::unique_ptr<SeedRow> seed;
    std::unique_ptr<MatrixPanel> matrix;
    juce::OwnedArray<StepLane> lanes;
    juce::OwnedArray<Chip> chips;
    std::unique_ptr<PageSelector> selector, tabs;
    std::vector<Paged> paged;
    std::unique_ptr<Keys> keys;
    std::unique_ptr<Wheels> wheels;
    juce::OwnedArray<TabCaption> captions;
    static inline int shownPage = 0, shownTab = 0;   // the page and tab last looked at, kept while Sonify runs
};

//==============================================================================
class ConsolePanelImpl final : public ConsolePanel, private juce::Timer
{
public:
    ConsolePanelImpl (BuiltinPlugin& p, Layout l) : layout (std::move (l)), plate (p, layout)
    {
        addAndMakeVisible (plate);
        plate.tape.choose = [this] (int i) { if (onPresetChosen != nullptr) onPresetChosen (i); };
        plate.tape.act = [this] (PresetAction a) { if (onPresetAction != nullptr) onPresetAction (a); };
        plate.onAction = [this] (PresetAction a) { if (onPresetAction != nullptr) onPresetAction (a); };
        plate.presetWindow.choose = [this] (int i) { if (onPresetChosen != nullptr) onPresetChosen (i); };
        plate.presetWindow.act = [this] (PresetAction a) { if (onPresetAction != nullptr) onPresetAction (a); };
        setSize (layout.W, layout.H);
        startTimerHz (30);
    }

    ~ConsolePanelImpl() override { stopTimer(); }

    juce::Rectangle<int> designBounds() const override   { return { 0, 0, layout.W, layout.H }; }
    void setPresets (const PresetMenu& menu) override     { plate.tape.setMenu (menu); plate.presetWindow.setMenu (menu); }

    void paint (Graphics& g) override { g.fillAll (tok::rack); }

    void resized() override
    {
        plate.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) layout.W, (float) getHeight() / (float) layout.H));
        plate.setBounds (0, 0, layout.W, layout.H);
    }

private:
    void timerCallback() override { if (isShowing()) plate.sync(); }

    Layout layout;     // before the plate, which keeps a reference to it
    Plate plate;
};
} // namespace

//==============================================================================
std::unique_ptr<ConsolePanel> ConsolePanel::create (BuiltinPlugin& plugin)
{
    Layout layout;

    if (plugin.getPanelName() == nullptr || ! layoutFor (plugin.getPanelName(), layout))
        return nullptr;

    return std::make_unique<ConsolePanelImpl> (plugin, std::move (layout));
}
