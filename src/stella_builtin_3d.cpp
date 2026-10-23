// D:\workspace\Fanan_web_hub\src\stella_builtin_3d.cpp
//
// The 3D displays: built-in elements (like the drop-down list, stella_builtin_elements.cpp)
// compiled with the elements' runtime into the studio's module and every exported plugin. Each
// is placed by its name as the type, bound to knobs and live values by role, and turned by
// dragging (the wheel zooms, a double-click puts it back). A project's element of the same name
// takes one's place.
//
//   WaveTerrain     an oscillator's waves along a knob, the playing row lit
//   MorphSurface    two oscillators and the blend between them, the mix row lit
//   FilterSweep     the filter opening and closing as it plays, rolling back
//   SoundWaterfall  the sound live (spectrum or wave), rolling back

#include "stella_elements.h"
#include "stella_view3d.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstdio>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace stella::ui::builtin
{
    //==========================================================================
    /** One cycle of each engine's wave as pure maths, for the pictures: the synth-engines
        skill's engines (Engines.h) without their anti-aliasing, which a picture doesn't need. */
    namespace waveshapes
    {
        constexpr float twoPi = 6.28318530718f;

        inline float clamp01 (float v) noexcept       { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
        inline float wrap (float p) noexcept          { return p - std::floor (p); }
        inline float saw (float p) noexcept           { return 2.0f * wrap (p) - 1.0f; }
        inline float pulse (float p, float w) noexcept { return wrap (p) < w ? 1.0f : -1.0f; }
        inline float triangle (float p) noexcept      { return 1.0f - 4.0f * std::fabs (wrap (p) - 0.5f); }

        enum class Engine { analog, supersaw, morph, fold, fm, phaseDistortion, additive, formant, sine };

        /** An engine by the name its struct has in Engines.h ("Analog", "Fm"...), any case. */
        inline Engine engineNamed (std::string name)
        {
            for (auto& c : name) c = (char) std::tolower ((unsigned char) c);
            if (name == "analog")          return Engine::analog;
            if (name == "supersaw")        return Engine::supersaw;
            if (name == "morph")           return Engine::morph;
            if (name == "fold")            return Engine::fold;
            if (name == "fm")              return Engine::fm;
            if (name == "phasedistortion" || name == "pd" || name == "cz") return Engine::phaseDistortion;
            if (name == "additive")        return Engine::additive;
            if (name == "formant")         return Engine::formant;
            return Engine::sine;
        }

        /** Morph's shape: sine, triangle, saw, square, narrow pulse as morph goes 0 to 1. */
        inline float morphShape (float p, float morph) noexcept
        {
            const float position = clamp01 (morph) * 3.999f;
            const int i = (int) position;
            const float f = position - (float) i;

            auto at = [p] (int k) -> float
            {
                switch (k)
                {
                    case 0:  return std::sin (twoPi * p);
                    case 1:  return triangle (p);
                    case 2:  return saw (p);
                    case 3:  return pulse (p, 0.5f);
                    default: return pulse (p, 0.12f);
                }
            };

            return at (i) + (at (i + 1) - at (i)) * f;
        }

        /** One cycle of an engine's wave at phase p (0..1), for these timbre and morph (0..1). */
        inline float wave (Engine engine, float p, float timbre, float morph) noexcept
        {
            p = wrap (p);
            timbre = clamp01 (timbre);
            morph = clamp01 (morph);

            switch (engine)
            {
                case Engine::analog:
                {
                    const float pw = 0.5f - 0.45f * morph;
                    return saw (p) + (pulse (p, pw) - saw (p)) * timbre;
                }

                case Engine::supersaw:
                {
                    // Seven saws, caught at one moment: the further apart, the more jagged.
                    static const float offsets[7] { -0.110f, -0.063f, -0.019f, 0.0f, 0.020f, 0.064f, 0.108f };
                    static const float starts[7] { 0.13f, 0.71f, 0.42f, 0.0f, 0.57f, 0.29f, 0.86f };
                    const float detune = timbre * timbre * 0.9f + 0.01f;
                    const float side = morph * 0.8f, centre = 1.0f - morph * 0.45f;
                    float sum = 0.0f;

                    for (int i = 0; i < 7; ++i)
                        sum += saw (p * (1.0f + offsets[i] * detune * 8.0f) + starts[i] * detune) * (i == 3 ? centre : side);

                    return sum / (centre + 6.0f * side) * 1.6f;
                }

                case Engine::morph:
                    return morphShape (p, morph);

                case Engine::fold:
                {
                    const float gain = 1.0f + timbre * 9.0f, offset = morph * 0.8f;
                    return std::sin (0.5f * 3.14159265f * gain * (std::sin (twoPi * p) + offset));
                }

                case Engine::fm:
                {
                    static const float ratios[] { 0.5f, 1.0f, 2.0f, 3.0f, 3.5f, 4.0f, 5.0f, 7.0f, 14.0f };
                    const float ratio = ratios[(int) (morph * 8.999f)];
                    return std::sin (twoPi * p + timbre * 5.0f * std::sin (twoPi * p * ratio));
                }

                case Engine::phaseDistortion:
                {
                    const float d = 0.5f - 0.48f * timbre;
                    const float bent = p < d ? 0.5f * p / d : 0.5f + 0.5f * (p - d) / (1.0f - d);
                    const float warp = -std::cos (twoPi * bent);
                    const float res = (1.0f - p) * std::cos (twoPi * p * (2.0f + 11.0f * timbre)) * 1.4f - 0.2f;
                    return warp + (res - warp) * morph;
                }

                case Engine::additive:
                {
                    const float slope = 2.4f - 1.7f * timbre, centre = 1.0f + morph * 24.0f;
                    float sum = 0.0f, total = 0.0f;

                    for (int k = 1; k <= 32; ++k)
                    {
                        const float n = (float) k;
                        const float a = std::pow (n, -slope) * (1.0f + 5.0f * std::exp (-(n - centre) * (n - centre) / 6.0f));
                        sum += a * std::sin (twoPi * n * p);
                        total += a;
                    }

                    return sum / (total * 0.55f);
                }

                case Engine::formant:
                {
                    // One period of the voice at C3: three formants under the grain's window.
                    static const float table[5][6] { { 730, 1090, 2440, 1.0f, 0.50f, 0.25f }, { 530, 1840, 2480, 1.0f, 0.40f, 0.30f },
                                                     { 270, 2290, 3010, 1.0f, 0.25f, 0.25f }, { 570, 840, 2410, 1.0f, 0.45f, 0.15f },
                                                     { 300, 870, 2240, 1.0f, 0.25f, 0.10f } };
                    const float position = morph * 3.999f;
                    const int v = (int) position;
                    const float t = position - (float) v, scale = 0.8f + 0.5f * timbre;
                    const float window = 0.5f - 0.5f * std::cos (twoPi * p);
                    float out = 0.0f;

                    for (int k = 0; k < 3; ++k)
                    {
                        const float f = (table[v][k] + (table[v + 1][k] - table[v][k]) * t) * scale;
                        const float level = table[v][k + 3] + (table[v + 1][k + 3] - table[v][k + 3]) * t;
                        out += std::sin (twoPi * p * f / 130.8f) * level;
                    }

                    return out * window;
                }

                case Engine::sine:
                    break;
            }

            return std::sin (twoPi * p);
        }
    }

    //==========================================================================
    /** An oscillator's waves in 3D, one row for each step of the knob that sweeps it (the morph,
        the wavetable position...). The row that plays is lit, glides as the knob or its modulation
        moves it, and glows brighter while the instrument sounds, with light rippling out from it.

        { "type": "WaveTerrain", "x", "y", "w": 400, "h": 200, "label": "Osc 1", "color": "#38E0D0",
          "params": { "timbre": "synth.timbre", "morph": "synth.morph" },
          "sources": { "position": "synth.morph", "level": "synth.env" },
          "settings": { "engine": "Morph", "sweep": "morph", "rows": 40, "points": 96 } }
        "engine": the Engines.h struct the oscillator uses; "sweep": the knob along the depth ("morph"
        or "timbre"), the other one shapes every row. "position" (a display, 0..1): where it plays
        now, modulation included (without it: the knob); "level" (0..1) makes it glow. */
    class WaveTerrain final : public Element
    {
    public:
        void resized() override
        {
            cam.width = (float) width();
            cam.height = (float) height();
        }

        void paint (Canvas& g) override
        {
            const Colour accent = colour (0xff38e0d0);
            const int rows = (int) view3d::clamp (setting ("rows", 40.0f), 4.0f, 96.0f);
            const int points = (int) view3d::clamp (setting ("points", 96.0f), 16.0f, 256.0f);
            const auto engine = waveshapes::engineNamed (settingText ("engine", "Morph"));
            const bool sweepTimbre = settingText ("sweep", "morph") == "timbre";
            const char* sweepRole = sweepTimbre ? "timbre" : "morph";
            const char* otherRole = sweepTimbre ? "morph" : "timbre";
            const float other = hasParam (otherRole) ? normalised (otherRole) : 0.3f;

            // The rows: the waves along the sweep, worked out again only when they change.
            if (rows != shownRows || points != shownPoints || std::fabs (other - shownOther) > 1.0e-4f || engine != shownEngine)
            {
                heights.assign ((size_t) rows * (size_t) points, 0.0f);

                for (int r = 0; r < rows; ++r)
                {
                    const float sweep = (float) r / (float) (rows - 1);
                    float peak = 1.0e-3f;

                    for (int i = 0; i < points; ++i)
                    {
                        const float phase = (float) i / (float) (points - 1);
                        const float v = waveshapes::wave (engine, phase, sweepTimbre ? sweep : other, sweepTimbre ? other : sweep);
                        heights[(size_t) r * points + i] = v;
                        peak = std::fmax (peak, std::fabs (v));
                    }

                    if (peak > 1.0f)   // never over the top of its row
                        for (int i = 0; i < points; ++i)
                            heights[(size_t) r * points + i] /= peak;
                }

                shownRows = rows; shownPoints = points; shownOther = other; shownEngine = engine;
            }

            // Where it plays: the live position when it's watched, otherwise the knob.
            const float knob = hasParam (sweepRole) ? normalised (sweepRole) : 0.5f;
            const float target = hasSource ("position") ? view3d::clamp (level ("position"), 0.0f, 1.0f) : knob;
            position = view3d::glide (position < 0.0f ? target : position, target);

            // How much it sounds: the envelope when it's watched, otherwise any key that's down.
            float sounding = 0.0f;

            if (hasSource ("level"))
                sounding = view3d::clamp (level ("level"), 0.0f, 1.0f);
            else
                for (int note = 21; note <= 108 && sounding == 0.0f; ++note)
                    if (isNoteDown (note)) sounding = 1.0f;

            glow = sounding > glow ? view3d::glide (glow, sounding, 0.6f) : glow * 0.92f;
            const float now = (float) seconds();

            cam.fit (-0.62f, 0.10f);
            view3d::stage (g, cam, 0xff0d1117, 0xff050608, withAlpha (accent, 0.08f), -0.62f);

            const float playRow = position * (float) (rows - 1);
            const Colour dim = mix (0xff1b3a40, accent, 0.30f), hot = brighter (accent, 0.55f);

            view3d::terrain (g, cam, heights.data(), rows, points, 0.36f, -0.62f, [&] (int r)
            {
                const float d = (float) r - playRow;
                const float lit = std::exp (-d * d / 0.8f);                                   // the row that plays
                const float area = std::exp (-std::fabs (d) / 4.0f) * glow;                    // its neighbourhood, while it sounds
                const float ripple = 0.5f + 0.5f * std::cos (6.2832f * (std::fabs (d) / 6.0f - now * 1.4f));
                const float depth = 1.0f - 0.45f * (float) r / (float) rows;                   // far rows fade
                const float bright = view3d::clamp (lit * (0.6f + 0.4f * glow) + area * (0.35f + 0.35f * ripple), 0.0f, 1.0f);

                view3d::RowLook look;
                look.line = withAlpha (mix (dim, lit > 0.5f ? 0xfff2fffd : hot, bright), 0.55f + 0.45f * std::fmax (depth, bright));
                look.thickness = 1.0f + 1.4f * lit;
                look.glow = lit > 0.2f ? withAlpha (accent, (0.12f + 0.30f * glow) * lit) : 0u;
                look.glowWidth = 6.0f + 8.0f * glow;
                look.xray = lit > 0.5f ? 0.35f + 0.25f * glow : 0.0f;   // the playing row shows through what's in front of it
                return look;
            });

            // The caption and the knob's value, under it.
            const float h = (float) height(), w = (float) width();
            g.drawText (label(), 8.0f, h - 18.0f, w * 0.5f, 16.0f, 12.0f, withAlpha (0xffe6edf3, 0.85f), Align::left, true);

            if (hasParam (sweepRole))
                g.drawText (paramName (sweepRole) + "  " + valueText (sweepRole), w * 0.5f, h - 18.0f, w * 0.5f - 8.0f, 16.0f, 12.0f,
                            withAlpha (0xffe6edf3, 0.70f), Align::right);
        }

        bool mouseDown (const Mouse& m) override       { orbit.down (cam, m); return true; }
        void mouseDrag (const Mouse& m) override       { orbit.drag (cam, m); }
        void mouseWheel (const Mouse&, float n) override { orbit.wheel (cam, n); }

    private:
        view3d::Camera cam;
        view3d::Orbit orbit;
        std::vector<float> heights;
        int shownRows = 0, shownPoints = 0;
        float shownOther = -1.0f;
        waveshapes::Engine shownEngine = waveshapes::Engine::sine;
        float position = -1.0f, glow = 0.0f;
    };

    STELLA_BUILTIN_ELEMENT (WaveTerrain, "An oscillator's waves in 3D along a knob (morph or timbre), the playing row lit and glowing as it sounds; drag to turn it")

    //==========================================================================
    /** Two oscillators and the blend between them as one 3D surface: the front edge is oscillator
        A's wave, the back edge B's, the rows between are the mix (with B tuned against A, the shapes
        twist as they blend). The row at the mix plays: lit, gliding as the mix moves, glowing while
        the instrument sounds. Colours run from A's to B's.

        { "type": "MorphSurface", "x", "y", "w": 440, "h": 220, "label": "A / B", "color": "#38E0D0",
          "params": { "mix": "synth.mix", "timbreA": "synth.timbre", "morphA": "synth.morph",
                      "timbreB": "synth.timbre2", "morphB": "synth.morph2", "tune": "synth.tune2" },
          "sources": { "mix": "synth.mix", "level": "synth.env" },
          "settings": { "engineA": "Analog", "engineB": "Fm", "colorB": "#FF4FD8", "rows": 32, "points": 96 } }
        Every parameter is optional (a missing knob counts as half way; "tune" is B's tuning in
        semitones). "mix" in sources is the live blend (0..1), modulation included. */
    class MorphSurface final : public Element
    {
    public:
        void resized() override
        {
            cam.width = (float) width();
            cam.height = (float) height();
        }

        void paint (Canvas& g) override
        {
            const Colour colourA = colour (0xff38e0d0);
            const Colour colourB = view3d::colourFrom (settingText ("colorB"), 0xffff4fd8);
            const int rows = (int) view3d::clamp (setting ("rows", 32.0f), 4.0f, 80.0f);
            const int points = (int) view3d::clamp (setting ("points", 96.0f), 16.0f, 256.0f);
            const auto engineA = waveshapes::engineNamed (settingText ("engineA", "Analog"));
            const auto engineB = waveshapes::engineNamed (settingText ("engineB", "Morph"));
            const auto knob = [this] (const char* role, float fallback) { return hasParam (role) ? normalised (role) : fallback; };
            const float timbreA = knob ("timbreA", 0.5f), morphA = knob ("morphA", 0.5f);
            const float timbreB = knob ("timbreB", 0.5f), morphB = knob ("morphB", 0.5f);
            const float ratio = hasParam ("tune") ? std::pow (2.0f, value ("tune") / 12.0f) : 1.0f;
            const float key[] { (float) rows, (float) points, timbreA, morphA, timbreB, morphB, ratio, (float) engineA, (float) engineB };

            // The surface, worked out again only when a knob that shapes it moves.
            if (! std::equal (std::begin (key), std::end (key), std::begin (shownKey)))
            {
                std::copy (std::begin (key), std::end (key), std::begin (shownKey));
                std::vector<float> a ((size_t) points), b ((size_t) points);
                float peak = 1.0e-3f;

                for (int i = 0; i < points; ++i)
                {
                    const float phase = (float) i / (float) (points - 1);
                    a[(size_t) i] = waveshapes::wave (engineA, phase, timbreA, morphA);
                    b[(size_t) i] = waveshapes::wave (engineB, phase * ratio, timbreB, morphB);
                    peak = std::fmax (peak, std::fmax (std::fabs (a[(size_t) i]), std::fabs (b[(size_t) i])));
                }

                heights.assign ((size_t) rows * (size_t) points, 0.0f);

                for (int r = 0; r < rows; ++r)
                {
                    const float t = (float) r / (float) (rows - 1);

                    for (int i = 0; i < points; ++i)
                        heights[(size_t) r * points + i] = ((1.0f - t) * a[(size_t) i] + t * b[(size_t) i]) / std::fmax (1.0f, peak);
                }
            }

            // The blend that plays, and how much it sounds.
            const float target = hasSource ("mix") ? view3d::clamp (level ("mix"), 0.0f, 1.0f) : knob ("mix", 0.5f);
            blend = view3d::glide (blend < 0.0f ? target : blend, target);
            const float sounding = hasSource ("level") ? view3d::clamp (level ("level"), 0.0f, 1.0f) : anyNoteDown();
            glow = sounding > glow ? view3d::glide (glow, sounding, 0.6f) : glow * 0.92f;
            const float now = (float) seconds();

            cam.fit (-0.5f, 0.3f);
            view3d::stage (g, cam, 0xff0f0d18, 0xff060508, withAlpha (mix (colourA, colourB, 0.5f), 0.08f), -0.5f);

            const float playRow = blend * (float) (rows - 1);

            view3d::terrain (g, cam, heights.data(), rows, points, 0.40f, -0.5f, [&] (int r)
            {
                const float t = (float) r / (float) (rows - 1);
                const float d = (float) r - playRow;
                const float lit = std::exp (-d * d / 0.8f);
                const float pulse = 0.5f + 0.5f * std::sin (now * 5.0f);
                const Colour tint = mix (colourA, colourB, t);

                view3d::RowLook look;
                look.line = withAlpha (mix (mix (0xff10161c, tint, 0.55f), 0xfff8f4ff, lit * (0.55f + 0.35f * glow)), 0.55f + 0.45f * lit);
                look.thickness = 1.0f + 1.5f * lit;
                look.glow = lit > 0.2f ? withAlpha (tint, (0.15f + 0.30f * glow * (0.7f + 0.3f * pulse)) * lit) : 0u;
                look.glowWidth = 6.0f + 9.0f * glow;
                look.cross = withAlpha (tint, 0.20f + 0.25f * std::exp (-std::fabs (d) / 3.0f) * glow);
                look.xray = lit > 0.5f ? 0.35f + 0.25f * glow : 0.0f;
                return look;
            }, 8);

            const float h = (float) height(), w = (float) width();
            g.drawText (label(), 8.0f, h - 18.0f, w * 0.5f, 16.0f, 12.0f, withAlpha (0xffece8f6, 0.85f), Align::left, true);
            // A at the front edge, B at the back.
            const auto front = cam.project (-1.0f, -0.5f, -1.0f), back = cam.project (1.0f, 0.3f, 1.0f);
            g.drawText ("A", front.x - 22.0f, front.y - 14.0f, 16.0f, 14.0f, 12.0f, colourA, Align::right, true);
            g.drawText ("B", back.x + 6.0f, back.y - 7.0f, 16.0f, 14.0f, 12.0f, colourB, Align::left, true);

            if (hasParam ("mix"))
                g.drawText (paramName ("mix") + "  " + valueText ("mix"), w * 0.5f, h - 18.0f, w * 0.5f - 8.0f, 16.0f, 12.0f,
                            withAlpha (0xffece8f6, 0.70f), Align::right);
        }

        bool mouseDown (const Mouse& m) override        { orbit.down (cam, m); return true; }
        void mouseDrag (const Mouse& m) override        { orbit.drag (cam, m); }
        void mouseWheel (const Mouse&, float n) override { orbit.wheel (cam, n); }

    private:
        float anyNoteDown() const
        {
            for (int note = 21; note <= 108; ++note)
                if (isNoteDown (note))
                    return 1.0f;

            return 0.0f;
        }

        view3d::Camera cam;
        view3d::Orbit orbit;
        std::vector<float> heights;
        float shownKey[9] { -1.0f };
        float blend = -1.0f, glow = 0.0f;
    };

    STELLA_BUILTIN_ELEMENT (MorphSurface, "Two oscillators and the blend between them as a 3D surface, the mix row lit and glowing as it sounds; drag to turn it")

    //==========================================================================
    /** The filter in 3D: its response now stands at the front and the ones before it roll away
        into the back, so you see it open and close as you play (an envelope's sweep rises and falls
        like a wave rolling back). The cutoff is a bright point on the front curve; the further the
        filter opens from its knob, the brighter it glows.

        { "type": "FilterSweep", "x", "y", "w": 400, "h": 200, "label": "Filter", "color": "#FFB347",
          "params": { "cutoff": "synth.cutoff", "resonance": "synth.reso" },
          "sources": { "cutoff": "synth.cutoff", "level": "synth.env" },
          "settings": { "mode": "lowpass24", "rows": 36, "speed": 24 } }
        "cutoff" in sources: the live cutoff (Hz, or 0..1 along 20 Hz to 20 kHz), modulation included;
        without it, the knob. "mode": lowpass, lowpass24, highpass, highpass24, bandpass. "speed":
        rows a second rolling back. */
    class FilterSweep final : public Element
    {
    public:
        void resized() override
        {
            cam.width = (float) width();
            cam.height = (float) height();
        }

        void paint (Canvas& g) override
        {
            const Colour accent = colour (0xffffb347);
            const int rows = (int) view3d::clamp (setting ("rows", 36.0f), 4.0f, 80.0f);
            const int points = (int) view3d::clamp (setting ("points", 96.0f), 16.0f, 256.0f);
            const float speed = view3d::clamp (setting ("speed", 24.0f), 2.0f, 60.0f);
            const std::string mode = settingText ("mode", "lowpass");

            // The cutoff: live when it's watched, otherwise the knob.
            const float knobHz = knobCutoff();
            float liveHz = knobHz;

            if (hasSource ("cutoff"))
            {
                const float v = level ("cutoff");
                liveHz = v > 1.5f ? view3d::clamp (v, 20.0f, 20000.0f) : (v > 0.0f ? 20.0f * std::pow (1000.0f, view3d::clamp (v, 0.0f, 1.0f)) : knobHz);
            }

            const float q = 0.707f + (hasParam ("resonance") ? normalised ("resonance") : 0.2f) * 9.0f;

            // A new row for each moment that passed (the oldest roll off the back).
            const double now = seconds();

            if ((int) heights.size() != rows * points)
            {
                heights.assign ((size_t) rows * (size_t) points, 0.0f);
                std::vector<float> first;
                response (first, points, liveHz, q, mode);

                for (int r = 0; r < rows; ++r)
                    std::copy (first.begin(), first.end(), heights.begin() + (std::ptrdiff_t) r * points);

                last = now;
            }

            const int steps = std::min (rows, (int) ((now - last) * speed));

            if (steps > 0 || now < last)
            {
                last = now < last ? now : last + steps / (double) speed;
                std::vector<float> row;
                response (row, points, liveHz, q, mode);

                for (int s = 0; s < std::max (1, steps); ++s)
                {
                    std::copy_backward (heights.begin(), heights.end() - points, heights.end());
                    std::copy (row.begin(), row.end(), heights.begin());
                }
            }

            const float opened = view3d::clamp (std::fabs (std::log2 (liveHz / knobHz)) / 3.0f, 0.0f, 1.0f);
            const float sounding = hasSource ("level") ? view3d::clamp (level ("level"), 0.0f, 1.0f) : opened;
            glow = sounding > glow ? view3d::glide (glow, sounding, 0.6f) : glow * 0.92f;

            cam.fit (-0.5f, 0.34f, 8.0f, 30.0f);   // room under it for the frequencies and the caption
            view3d::stage (g, cam, 0xff12100c, 0xff070605, withAlpha (accent, 0.07f), -0.5f);

            view3d::terrain (g, cam, heights.data(), rows, points, 0.42f, -0.5f, [&] (int r)
            {
                const float age = (float) r / (float) (rows - 1);
                const float fresh = std::exp (-age * 6.0f);

                view3d::RowLook look;
                look.line = withAlpha (mix (mix (0xff241a10, accent, 0.45f), 0xfffff4e0, fresh * (0.5f + 0.5f * glow)), 0.85f - 0.55f * age);
                look.thickness = r == 0 ? 2.2f : 1.0f;
                look.glow = r == 0 ? withAlpha (accent, 0.18f + 0.30f * glow) : 0u;
                look.glowWidth = 7.0f + 8.0f * glow;
                return look;
            });

            // The cutoff on the front curve.
            const float t = view3d::clamp (std::log10 (liveHz / 20.0f) / 3.0f, 0.0f, 1.0f);
            const int at = std::min (points - 1, (int) std::lround (t * (float) (points - 1)));
            const auto point = cam.project (-1.0f + 2.0f * t, -0.5f + (heights[(size_t) at] + 1.0f) * 0.42f, -1.0f);
            g.fillCircle (point.x, point.y, 7.0f + 6.0f * glow, withAlpha (accent, 0.20f + 0.25f * glow));
            g.fillCircle (point.x, point.y, 3.0f, 0xfffff8ec);

            // Where 100 Hz, 1 kHz and 10 kHz are, along the front.
            const float h = (float) height(), w = (float) width();

            for (const auto& [hz, text] : { std::pair<float, const char*> { 100.0f, "100" }, { 1000.0f, "1k" }, { 10000.0f, "10k" } })
            {
                const auto p = cam.project (-1.0f + 2.0f * std::log10 (hz / 20.0f) / 3.0f, -0.5f, -1.0f);
                g.drawText (text, p.x - 15.0f, p.y + 3.0f, 30.0f, 12.0f, 10.0f, withAlpha (0xfff3eadf, 0.45f));
            }

            g.drawText (label(), 8.0f, h - 18.0f, w * 0.5f, 16.0f, 12.0f, withAlpha (0xfff3eadf, 0.85f), Align::left, true);
            g.drawText (hzText (liveHz), w * 0.5f, h - 18.0f, w * 0.5f - 8.0f, 16.0f, 12.0f, withAlpha (0xfff3eadf, 0.75f), Align::right);
        }

        bool mouseDown (const Mouse& m) override        { orbit.down (cam, m); return true; }
        void mouseDrag (const Mouse& m) override        { orbit.drag (cam, m); }
        void mouseWheel (const Mouse&, float n) override { orbit.wheel (cam, n); }

    private:
        float knobCutoff() const
        {
            if (! hasParam ("cutoff"))
                return 1000.0f;

            const auto u = unit ("cutoff");

            if (u == "Hz" || u == "hz")   return view3d::clamp (value ("cutoff"), 20.0f, 20000.0f);
            if (u == "kHz" || u == "khz") return view3d::clamp (value ("cutoff") * 1000.0f, 20.0f, 20000.0f);
            return 20.0f * std::pow (1000.0f, normalised ("cutoff"));
        }

        /** The filter's response over 20 Hz to 20 kHz (log), as heights: -30 dB at -1, +24 dB at 1. */
        static void response (std::vector<float>& out, int points, float cutoff, float q, const std::string& mode)
        {
            out.resize ((size_t) points);
            const bool steep = mode.size() >= 2 && mode.compare (mode.size() - 2, 2, "24") == 0;
            const int kind = mode.rfind ("high", 0) == 0 ? 1 : (mode.rfind ("band", 0) == 0 ? 2 : 0);

            for (int i = 0; i < points; ++i)
            {
                const float f = 20.0f * std::pow (1000.0f, (float) i / (float) (points - 1));
                const float x = f / cutoff;
                const float denominator = std::sqrt ((1.0f - x * x) * (1.0f - x * x) + (x / q) * (x / q));
                float magnitude = kind == 1 ? (x * x) / denominator : (kind == 2 ? (x / q) / denominator : 1.0f / denominator);

                if (steep && kind != 2)
                    magnitude *= magnitude / std::fmax (1.0f, q * 0.5f);

                const float db = view3d::clamp (20.0f * std::log10 (std::fmax (magnitude, 1.0e-6f)), -30.0f, 24.0f);
                out[(size_t) i] = (db + 30.0f) / 27.0f - 1.0f;
            }
        }

        static std::string hzText (float hz)
        {
            char text[32];

            if (hz >= 1000.0f)
                std::snprintf (text, sizeof (text), "%.1f kHz", hz / 1000.0f);
            else
                std::snprintf (text, sizeof (text), "%.0f Hz", hz);

            return text;
        }

        view3d::Camera cam;
        view3d::Orbit orbit;
        std::vector<float> heights;
        double last = 0.0;
        float glow = 0.0f;
    };

    STELLA_BUILTIN_ELEMENT (FilterSweep, "The filter in 3D: its response now at the front, the moments before rolling back, so it opens and closes as you play; drag to turn it")

    //==========================================================================
    /** The sound itself, live, as a 3D waterfall: the newest moment at the front, the ones before
        rolling back into the distance. "spectrum": its frequencies, low on the left; "wave": its
        waveform, held steady on the note's cycle. The front row glows with the sound's level.

        { "type": "SoundWaterfall", "x", "y", "w": 420, "h": 210, "label": "Output", "color": "#7C9CFF",
          "source": "plugin.out L", "settings": { "mode": "spectrum", "rows": 40, "speed": 30 } }
        "source" is a signal (the plugin's output, or a module's), not a display. */
    class SoundWaterfall final : public Element
    {
    public:
        void resized() override
        {
            cam.width = (float) width();
            cam.height = (float) height();
        }

        void paint (Canvas& g) override
        {
            const Colour accent = colour (0xff7c9cff);
            const int rows = (int) view3d::clamp (setting ("rows", 40.0f), 4.0f, 80.0f);
            const int points = (int) view3d::clamp (setting ("points", 96.0f), 16.0f, 192.0f);
            const float speed = view3d::clamp (setting ("speed", 30.0f), 2.0f, 60.0f);
            const bool spectrum = settingText ("mode", "spectrum") != "wave";

            if ((int) heights.size() != rows * points)
            {
                heights.assign ((size_t) rows * (size_t) points, spectrum ? -1.0f : 0.0f);
                last = seconds();
            }

            // Each moment that passed: one new row, read from the sound now.
            const double now = seconds();
            const int steps = std::min (rows, (int) ((now - last) * speed));

            if (steps > 0 || now < last)
            {
                last = now < last ? now : last + steps / (double) speed;
                readScope (samples, size);
                std::vector<float> row ((size_t) points);
                loudness = spectrum ? spectrumRow (row) : waveRow (row);

                for (int s = 0; s < std::max (1, steps); ++s)
                {
                    std::copy_backward (heights.begin(), heights.end() - points, heights.end());
                    std::copy (row.begin(), row.end(), heights.begin());
                }
            }

            glow = loudness > glow ? view3d::glide (glow, loudness, 0.6f) : glow * 0.90f;

            cam.fit (-0.5f, spectrum ? 0.42f : 0.18f);
            view3d::stage (g, cam, 0xff0c0e16, 0xff05060a, withAlpha (accent, 0.07f), -0.5f);

            view3d::terrain (g, cam, heights.data(), rows, points, spectrum ? 0.46f : 0.34f, -0.5f, [&] (int r)
            {
                const float age = (float) r / (float) (rows - 1);
                const float fresh = std::exp (-age * 5.0f);

                view3d::RowLook look;
                look.line = withAlpha (mix (mix (0xff141a2a, accent, 0.5f), 0xfff0f4ff, fresh * glow), 0.9f - 0.6f * age);
                look.thickness = r == 0 ? 1.8f : 1.0f;
                look.glow = r == 0 && glow > 0.02f ? withAlpha (accent, 0.10f + 0.35f * glow) : 0u;
                look.glowWidth = 6.0f + 8.0f * glow;
                return look;
            });

            const float h = (float) height(), w = (float) width();
            g.drawText (label(), 8.0f, h - 18.0f, w * 0.5f, 16.0f, 12.0f, withAlpha (0xffe8ecff, 0.85f), Align::left, true);
            g.drawText (spectrum ? "Spectrum" : "Wave", w * 0.5f, h - 18.0f, w * 0.5f - 8.0f, 16.0f, 12.0f, withAlpha (0xffe8ecff, 0.6f), Align::right);
        }

        bool mouseDown (const Mouse& m) override
        {
            orbit.down (cam, m);
            return true;
        }

        void mouseDrag (const Mouse& m) override        { orbit.drag (cam, m); }
        void mouseWheel (const Mouse&, float n) override { orbit.wheel (cam, n); }

    private:
        static constexpr int size = 1024;

        /** Frequencies, 30 Hz to 16 kHz on a log scale: -1 (silence, -72 dB) to 1 (full scale).
            Returns the loudness (0..1). */
        float spectrumRow (std::vector<float>& row)
        {
            std::complex<float> bins[size];
            float rms = 0.0f;

            for (int i = 0; i < size; ++i)
            {
                const float window = 0.5f - 0.5f * std::cos (6.2831853f * (float) i / (float) (size - 1));
                bins[i] = samples[i] * window;
                rms += samples[i] * samples[i];
            }

            fft (bins);
            const int points = (int) row.size();
            const float rate = 48000.0f;

            for (int p = 0; p < points; ++p)
            {
                // A band's loudest bin; where bands are narrower than a bin (the lows), the level
                // between the two nearest bins, so the low end is smooth rather than steps.
                const float lowBin = 30.0f * std::pow (16000.0f / 30.0f, (float) p / (float) points) * size / rate;
                const float highBin = 30.0f * std::pow (16000.0f / 30.0f, (float) (p + 1) / (float) points) * size / rate;
                float peak = 0.0f;

                if (highBin - lowBin < 1.0f)
                {
                    const float centre = 0.5f * (lowBin + highBin);
                    const int k = std::max (1, std::min (size / 2 - 2, (int) centre));
                    const float t = view3d::clamp (centre - (float) k, 0.0f, 1.0f);
                    peak = std::abs (bins[k]) * (1.0f - t) + std::abs (bins[k + 1]) * t;
                }
                else
                {
                    for (int k = std::max (1, (int) lowBin); k <= std::min (size / 2 - 1, (int) highBin); ++k)
                        peak = std::fmax (peak, std::abs (bins[k]));
                }

                const float db = 20.0f * std::log10 (std::fmax (peak / (size * 0.25f), 1.0e-6f));
                row[(size_t) p] = view3d::clamp ((db + 72.0f) / 36.0f - 1.0f, -1.0f, 1.0f);
            }

            return view3d::clamp (std::sqrt (rms / size) * 3.0f, 0.0f, 1.0f);
        }

        /** The waveform from a rising zero crossing, so a held note stands still. */
        float waveRow (std::vector<float>& row)
        {
            // From a rising zero crossing, two cycles (to the crossing two cycles on), so a held note
            // stands still and fills the width.
            int start = 0, span = size / 2;

            for (int i = 1; i < size / 2; ++i)
                if (samples[i - 1] <= 0.0f && samples[i] > 0.0f) { start = i; break; }

            for (int i = start + 16, cycles = 0; i < size; ++i)
                if (samples[i - 1] <= 0.0f && samples[i] > 0.0f && ++cycles == 2) { span = i - start; break; }

            float peak = 0.0f, rms = 0.0f;

            for (int i = 0; i < size; ++i)
            {
                peak = std::fmax (peak, std::fabs (samples[i]));
                rms += samples[i] * samples[i];
            }

            gain = peak > gain ? peak : std::fmax (gain * 0.995f, 0.05f);   // follows the level up at once, down slowly
            const int points = (int) row.size();
            span = std::max (32, std::min (span, size - start));

            for (int p = 0; p < points; ++p)
                row[(size_t) p] = view3d::clamp (samples[start + p * (span - 1) / (points - 1)] / gain, -1.0f, 1.0f);

            return view3d::clamp (std::sqrt (rms / size) * 3.0f, 0.0f, 1.0f);
        }

        /** In place, radix 2. */
        static void fft (std::complex<float>* x)
        {
            for (int i = 1, j = 0; i < size; ++i)
            {
                int bit = size >> 1;

                for (; j & bit; bit >>= 1)
                    j ^= bit;

                j ^= bit;

                if (i < j)
                    std::swap (x[i], x[j]);
            }

            for (int length = 2; length <= size; length <<= 1)
            {
                const float angle = -6.2831853f / (float) length;
                const std::complex<float> step (std::cos (angle), std::sin (angle));

                for (int i = 0; i < size; i += length)
                {
                    std::complex<float> w (1.0f, 0.0f);

                    for (int k = 0; k < length / 2; ++k)
                    {
                        const auto u = x[i + k], v = x[i + k + length / 2] * w;
                        x[i + k] = u + v;
                        x[i + k + length / 2] = u - v;
                        w *= step;
                    }
                }
            }
        }

        view3d::Camera cam;
        view3d::Orbit orbit;
        std::vector<float> heights;
        float samples[size] {};
        double last = 0.0;
        float glow = 0.0f, loudness = 0.0f, gain = 0.1f;
    };

    STELLA_BUILTIN_ELEMENT (SoundWaterfall, "The sound live as a 3D waterfall (spectrum or wave), the newest moment at the front rolling back; drag to turn it")
}
