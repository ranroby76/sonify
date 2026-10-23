// D:\workspace\Fanan_web_hub\src\stella_paint.h
//
// Painting with light, for the pictures paint_picture makes (part of the elements' runtime):
// a catalogue of materials (metals that reflect the room: aluminium, steel, silver, chrome,
// gold, rose gold, brass, copper, bronze, liquid titanium, gunmetal, cast iron, hammertone,
// anodized; stones: marble, black marble, granite, slate, concrete, terrazzo, sandstone;
// plastics: glossy, matte, rubber, bakelite, ceramic; wood, burl, leather, tolex, felt,
// carbon, paper, dark screen glass, frosted glass, clear and tinted see-through glass (Glass:
// for shapes and knob layers over something), pearl, powder coat), each in any colour and finish
// (polished, satin, brushed, hammered, sandblasted, matte, liquid), with pattern layers over
// them the way Photoshop stacks them (stripes, checkers, dots, honeycomb, diamond plate,
// knurling, guilloche, sunburst, carbon weave, camouflage, damascus... each with its colour,
// opacity and blend mode, or embossed into the surface).
//
// One light shades everything the way the Knob Studio's looks are lit: an angle (120: from the
// upper left, where the looks' light comes from) and a height. Metals mirror a soft studio
// around them; raised plates catch the light on one side and cast a shadow on the other,
// recesses take the shadow of their rim, so the panel and its knobs agree.
//
// PanelPainter (stella_builtin_paint.cpp) paints whole panels with it from settings. A
// painter of a project's own can use it too: #include "stella_paint.h", then stella::paint.
//
// Coordinates are pixels from the picture's top-left corner; colours are 0..1 floats (Rgb)
// while painting, written to the canvas once at the end (Surface::write).

#pragma once

#include "stella_element_api.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace stella::paint
{
    using stella::ui::Canvas;
    using stella::ui::Colour;

    //==========================================================================
    struct Rgb { float r = 0.0f, g = 0.0f, b = 0.0f; };

    inline float clamp01 (float v) noexcept               { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
    inline float mix (float a, float b, float t) noexcept  { return a + (b - a) * t; }
    inline float smooth (float t) noexcept                 { t = clamp01 (t); return t * t * (3.0f - 2.0f * t); }
    inline float smoothstep (float e0, float e1, float v) noexcept { return smooth ((v - e0) / (e1 - e0)); }
    inline float fract (float v) noexcept                  { return v - std::floor (v); }

    /** A number from text, as JSON writes it: '.' for its point, whatever the locale (strtod
        follows the locale, and stops at the '.' under a German one). *end: where it stopped
        (text itself if there's no number there). */
    inline double numberIn (const char* text, const char** end) noexcept
    {
        const char* p = text;

        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
            ++p;

        const bool negative = *p == '-';

        if (*p == '-' || *p == '+')
            ++p;

        std::uint64_t mantissa = 0;
        int exponent = 0, digits = 0;

        for (; *p >= '0' && *p <= '9'; ++p, ++digits)
        {
            if (mantissa < 1000000000000000000ull)
                mantissa = mantissa * 10u + (std::uint64_t) (*p - '0');
            else
                ++exponent;   // past 19 digits: they can't change a double
        }

        if (*p == '.')
        {
            const char* q = p + 1;

            for (; *q >= '0' && *q <= '9'; ++q, ++digits)
            {
                if (mantissa < 1000000000000000000ull)
                {
                    mantissa = mantissa * 10u + (std::uint64_t) (*q - '0');
                    --exponent;
                }
            }

            if (digits > 0)
                p = q;
        }

        if (digits == 0)
        {
            if (end != nullptr)
                *end = text;

            return 0.0;
        }

        if (*p == 'e' || *p == 'E')
        {
            const char* q = p + 1;
            const bool down = *q == '-';

            if (*q == '-' || *q == '+')
                ++q;

            if (*q >= '0' && *q <= '9')
            {
                int power = 0;

                for (; *q >= '0' && *q <= '9'; ++q)
                    power = power < 100000 ? power * 10 + (*q - '0') : power;

                exponent += down ? -power : power;
                p = q;
            }
        }

        if (end != nullptr)
            *end = p;

        // Scaled by an exact power of ten (exact up to 1e22): "0.1" is the double it was.
        auto value = (double) mantissa;

        if (mantissa != 0 && exponent != 0)
        {
            double tens = 1.0;
            int left = exponent < 0 ? -exponent : exponent;

            for (; left > 0 && tens < 1.0e300; --left)
                tens *= 10.0;

            if (left > 0)
                value = exponent < 0 ? 0.0 : HUGE_VAL;
            else
                value = exponent < 0 ? value / tens : value * tens;
        }

        return negative ? -value : value;
    }

    inline Rgb rgbOf (Colour c) noexcept
    {
        return { (float) ((c >> 16) & 255u) / 255.0f, (float) ((c >> 8) & 255u) / 255.0f, (float) (c & 255u) / 255.0f };
    }

    inline Colour colourOf (Rgb c, float alpha = 1.0f) noexcept
    {
        auto byte = [] (float v) { return (std::uint32_t) (clamp01 (v) * 255.0f + 0.5f); };
        return (byte (alpha) << 24) | (byte (c.r) << 16) | (byte (c.g) << 8) | byte (c.b);
    }

    inline Rgb mix (Rgb a, Rgb b, float t) noexcept        { return { mix (a.r, b.r, t), mix (a.g, b.g, t), mix (a.b, b.b, t) }; }
    inline Rgb scale (Rgb a, float k) noexcept             { return { a.r * k, a.g * k, a.b * k }; }
    inline Rgb times (Rgb a, Rgb b) noexcept               { return { a.r * b.r, a.g * b.g, a.b * b.b }; }
    inline Rgb add (Rgb a, float k) noexcept               { return { a.r + k, a.g + k, a.b + k }; }
    inline Rgb add (Rgb a, Rgb b, float k = 1.0f) noexcept { return { a.r + b.r * k, a.g + b.g * k, a.b + b.b * k }; }
    inline float luma (Rgb c) noexcept                     { return c.r * 0.299f + c.g * 0.587f + c.b * 0.114f; }

    /** Light added the way light adds up: brightens, never past white (a glow, a lamp's pool). */
    inline Rgb screen (Rgb a, Rgb light, float amount) noexcept
    {
        auto one = [amount] (float x, float l) { l = clamp01 (l * amount); return 1.0f - (1.0f - clamp01 (x)) * (1.0f - l); };
        return { one (a.r, light.r), one (a.g, light.g), one (a.b, light.b) };
    }

    /** A rainbow around t (0..1), for colours that shift with the angle (titanium, pearl). */
    inline Rgb rainbow (float t) noexcept
    {
        const float a = 6.2831853f * t;
        return { 0.5f + 0.5f * std::cos (a), 0.5f + 0.5f * std::cos (a - 2.094f), 0.5f + 0.5f * std::cos (a - 4.189f) };
    }

    /** "#RRGGBB", "#AARRGGBB" or "0xAARRGGBB"; fallback when it isn't one. */
    inline Rgb colourNamed (const std::string& text, Rgb fallback) noexcept
    {
        std::string hex;

        for (const char c : text)
            if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
                hex += c;

        if (text.find ('x') != std::string::npos || text.find ('X') != std::string::npos)
            hex = hex.size() > 1 ? hex.substr (1) : hex;   // the 0 of 0x

        if (hex.size() == 8)
            hex = hex.substr (2);

        if (hex.size() != 6)
            return fallback;

        return rgbOf ((Colour) std::strtoul (hex.c_str(), nullptr, 16));
    }

    inline std::string lowered (std::string s)
    {
        for (auto& c : s)
            if (c >= 'A' && c <= 'Z')
                c = (char) (c - 'A' + 'a');

        std::string out;

        for (const char c : s)
            if (c != ' ' && c != '-' && c != '_')
                out += c;

        return out;
    }

    //==========================================================================
    // Noise: the same seed paints the same grain again.
    inline float hash (int x, int y, int seed) noexcept
    {
        std::uint32_t h = (std::uint32_t) x * 374761393u + (std::uint32_t) y * 668265263u + (std::uint32_t) seed * 2147483647u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return (float) ((h ^ (h >> 16)) & 0xffffffu) / 16777215.0f;   // 0..1
    }

    /** Smooth value noise, 0..1. */
    inline float noise (float x, float y, int seed) noexcept
    {
        const int xi = (int) std::floor (x), yi = (int) std::floor (y);
        const float fx = smooth (x - (float) xi), fy = smooth (y - (float) yi);
        const float a = hash (xi, yi, seed), b = hash (xi + 1, yi, seed), c = hash (xi, yi + 1, seed), d = hash (xi + 1, yi + 1, seed);
        return mix (mix (a, b, fx), mix (c, d, fx), fy);
    }

    /** Fractal noise: octaves of noise, each twice as fine and half as strong. 0..1. */
    inline float fbm (float x, float y, int seed, int octaves = 5) noexcept
    {
        float sum = 0.0f, amp = 0.5f, total = 0.0f;

        for (int o = 0; o < octaves; ++o)
        {
            sum += noise (x, y, seed + o * 17) * amp;
            total += amp;
            x *= 2.03f; y *= 2.03f; amp *= 0.5f;
        }

        return sum / total;
    }

    /** Cells (Voronoi): the distance to the nearest scattered point (0 at a point .. ~1), and
        which cell it is (a number of its own, 0..1). */
    inline float cells (float x, float y, int seed, float* which = nullptr) noexcept
    {
        const int xi = (int) std::floor (x), yi = (int) std::floor (y);
        float best = 9.0f, id = 0.0f;

        for (int j = -1; j <= 1; ++j)
            for (int i = -1; i <= 1; ++i)
            {
                const float px = (float) (xi + i) + hash (xi + i, yi + j, seed), py = (float) (yi + j) + hash (xi + i, yi + j, seed + 1);
                const float dx = px - x, dy = py - y, d = dx * dx + dy * dy;

                if (d < best)
                {
                    best = d;
                    id = hash (xi + i, yi + j, seed + 2);
                }
            }

        if (which != nullptr)
            *which = id;

        return std::sqrt (best);
    }

    //==========================================================================
    /** The light: where it comes from, as the looks have it. angle in degrees around the
        picture (0 from the right, 90 from the top, 120 the upper left, 180 the left);
        altitude in degrees above the panel (low: long shadows and strong edges). */
    struct Light
    {
        float x = 0.0f, y = 0.0f, z = 1.0f;   // towards the light (unit)
        float tx = 0.0f, ty = -1.0f;          // towards the light across the picture (unit)

        static Light from (float angleDegrees, float altitudeDegrees) noexcept
        {
            const float a = angleDegrees * 0.0174533f;
            const float e = (altitudeDegrees < 10.0f ? 10.0f : (altitudeDegrees > 80.0f ? 80.0f : altitudeDegrees)) * 0.0174533f;
            Light l;
            l.tx = std::cos (a);
            l.ty = -std::sin (a);
            l.x = l.tx * std::cos (e);
            l.y = l.ty * std::cos (e);
            l.z = std::sin (e);
            return l;
        }
    };

    //==========================================================================
    // Materials

    /** How a material answers the light. */
    struct Optics
    {
        float metal = 0.0f;         // 0 paint, stone, plastic .. 1 bare metal: it mirrors the room, tinted by its colour
        float spec = 0.15f;         // its highlight's strength
        float rough = 0.6f;         // 0 a mirror .. 1 matte
        float coat = 0.0f;          // a clear glossy coat over it (polished stone, glossy plastic, lacquer, enamel)
        float flow = 0.0f;          // liquid: the surface swirls as if molten
        float iridescence = 0.0f;   // colours shifting with the angle (titanium, pearl)
        float bump = 1.0f;          // how strongly its texture's relief catches the light
    };

    enum class Material
    {
        powder, aluminium, steel, silver, chrome, gold, roseGold, brass, copper, bronze, titanium, gunmetal, iron, hammertone, anodized,
        carbon, marble, blackMarble, granite, slate, concrete, terrazzo, sandstone,
        plastic, mattePlastic, rubber, bakelite, ceramic, wood, burl, flamewood, leather, tolex, felt, paper, glass, frosted, pearl, smooth,
        clearGlass, tintedGlass,   // see-through (isSeeThrough): what's under them shows through
        count
    };

    /** How a material's surface is finished (natural: as the material comes). */
    enum class Finish { natural, polished, satin, brushed, hammered, sandblasted, matte, liquid };

    struct MaterialInfo
    {
        const char* name;          // its name in settings
        std::uint32_t colour, colour2;
        Optics optics;
        Finish finish;
    };

    inline const MaterialInfo& infoOf (Material m) noexcept
    {
        //                         name             colour      colour2     metal spec  rough coat  flow  irid  bump    finish
        static const MaterialInfo table[] {
            { "powder",        0xff2b2f36, 0xff2b2f36, { 0.00f, 0.15f, 0.70f, 0.00f, 0.0f, 0.00f, 1.0f }, Finish::natural },
            { "aluminium",     0xffa9adb4, 0xffa9adb4, { 0.85f, 0.35f, 0.35f, 0.00f, 0.0f, 0.00f, 1.0f }, Finish::brushed },
            { "steel",         0xff8e949b, 0xff8e949b, { 0.85f, 0.35f, 0.35f, 0.00f, 0.0f, 0.00f, 1.0f }, Finish::brushed },
            { "silver",        0xffcdd0d5, 0xffcdd0d5, { 1.00f, 0.55f, 0.12f, 0.00f, 0.0f, 0.00f, 1.0f }, Finish::polished },
            { "chrome",        0xffdde1e6, 0xffdde1e6, { 1.00f, 0.70f, 0.04f, 0.00f, 0.0f, 0.00f, 1.0f }, Finish::polished },
            { "gold",          0xffd9b048, 0xffd9b048, { 1.00f, 0.55f, 0.14f, 0.00f, 0.0f, 0.00f, 1.0f }, Finish::polished },
            { "rosegold",      0xffe2a48e, 0xffe2a48e, { 1.00f, 0.55f, 0.14f, 0.00f, 0.0f, 0.00f, 1.0f }, Finish::polished },
            { "brass",         0xffc9a54a, 0xffc9a54a, { 0.95f, 0.45f, 0.26f, 0.00f, 0.0f, 0.00f, 1.0f }, Finish::satin },
            { "copper",        0xffc77b4a, 0xffc77b4a, { 1.00f, 0.50f, 0.18f, 0.00f, 0.0f, 0.00f, 1.0f }, Finish::polished },
            { "bronze",        0xffa8743b, 0xffa8743b, { 0.90f, 0.40f, 0.34f, 0.00f, 0.0f, 0.00f, 1.0f }, Finish::satin },
            { "titanium",      0xff9a9fa6, 0xff9a9fa6, { 1.00f, 0.60f, 0.06f, 0.00f, 1.0f, 0.12f, 1.0f }, Finish::liquid },
            { "gunmetal",      0xff4a4f57, 0xff4a4f57, { 0.90f, 0.40f, 0.30f, 0.00f, 0.0f, 0.00f, 1.0f }, Finish::satin },
            { "iron",          0xff3c3d40, 0xff3c3d40, { 0.60f, 0.15f, 0.75f, 0.00f, 0.0f, 0.00f, 1.0f }, Finish::sandblasted },
            { "hammertone",    0xff5a6670, 0xff5a6670, { 0.45f, 0.30f, 0.45f, 0.20f, 0.0f, 0.00f, 1.0f }, Finish::hammered },
            { "anodized",      0xff3a4250, 0xff3a4250, { 0.55f, 0.35f, 0.40f, 0.00f, 0.0f, 0.00f, 1.0f }, Finish::satin },
            { "carbon",        0xff2a2c30, 0xff2a2c30, { 0.00f, 0.40f, 0.30f, 0.60f, 0.0f, 0.00f, 1.0f }, Finish::natural },
            { "marble",        0xffefede8, 0xff7a746e, { 0.00f, 0.35f, 0.15f, 0.80f, 0.0f, 0.00f, 0.3f }, Finish::natural },
            { "blackmarble",   0xff1a1a1d, 0xffd8c08a, { 0.00f, 0.40f, 0.12f, 0.85f, 0.0f, 0.00f, 0.3f }, Finish::natural },
            { "granite",       0xff6b6b6e, 0xff2a2a2c, { 0.00f, 0.35f, 0.18f, 0.65f, 0.0f, 0.00f, 0.3f }, Finish::natural },
            { "slate",         0xff3c4044, 0xff2c2f33, { 0.00f, 0.10f, 0.80f, 0.00f, 0.0f, 0.00f, 1.0f }, Finish::natural },
            { "concrete",      0xff9a9893, 0xff6e6c68, { 0.00f, 0.05f, 0.90f, 0.00f, 0.0f, 0.00f, 1.0f }, Finish::natural },
            { "terrazzo",      0xffe8e2d6, 0xffb5543c, { 0.00f, 0.30f, 0.20f, 0.55f, 0.0f, 0.00f, 0.3f }, Finish::natural },
            { "sandstone",     0xffc9a97a, 0xffa9875a, { 0.00f, 0.05f, 0.85f, 0.00f, 0.0f, 0.00f, 1.0f }, Finish::natural },
            { "plastic",       0xff1e2024, 0xff1e2024, { 0.00f, 0.50f, 0.20f, 0.70f, 0.0f, 0.00f, 1.0f }, Finish::natural },
            { "matteplastic",  0xff2a2c30, 0xff2a2c30, { 0.00f, 0.10f, 0.80f, 0.00f, 0.0f, 0.00f, 1.0f }, Finish::natural },
            { "rubber",        0xff1a1b1d, 0xff1a1b1d, { 0.00f, 0.05f, 0.95f, 0.00f, 0.0f, 0.00f, 1.0f }, Finish::natural },
            { "bakelite",      0xff4a2418, 0xff2a120b, { 0.00f, 0.45f, 0.20f, 0.60f, 0.0f, 0.00f, 1.0f }, Finish::natural },
            { "ceramic",       0xfff1efea, 0xfff1efea, { 0.00f, 0.45f, 0.10f, 0.90f, 0.0f, 0.00f, 1.0f }, Finish::natural },
            { "wood",          0xff7a4a2a, 0xff3e2412, { 0.00f, 0.25f, 0.45f, 0.25f, 0.0f, 0.00f, 1.0f }, Finish::natural },
            { "burl",          0xff8a5a32, 0xff3a2210, { 0.00f, 0.30f, 0.35f, 0.40f, 0.0f, 0.00f, 1.0f }, Finish::natural },
            { "flamewood",     0xffe8b62e, 0xffb06a14, { 0.00f, 0.45f, 0.22f, 0.80f, 0.0f, 0.00f, 1.0f }, Finish::natural },
            { "leather",       0xff3a2a22, 0xff3a2a22, { 0.00f, 0.15f, 0.65f, 0.00f, 0.0f, 0.00f, 1.0f }, Finish::natural },
            { "tolex",         0xff1e1e20, 0xff1e1e20, { 0.00f, 0.15f, 0.75f, 0.00f, 0.0f, 0.00f, 1.0f }, Finish::natural },
            { "felt",          0xff3a3d44, 0xff3a3d44, { 0.00f, 0.00f, 1.00f, 0.00f, 0.0f, 0.00f, 1.0f }, Finish::natural },
            { "paper",         0xffe8dfc8, 0xffe8dfc8, { 0.00f, 0.05f, 0.90f, 0.00f, 0.0f, 0.00f, 1.0f }, Finish::natural },
            { "glass",         0xff16324a, 0xff16324a, { 0.00f, 0.60f, 0.08f, 1.00f, 0.0f, 0.00f, 1.0f }, Finish::natural },
            { "frosted",       0xffc9d2d8, 0xffc9d2d8, { 0.00f, 0.20f, 0.60f, 0.20f, 0.0f, 0.00f, 1.0f }, Finish::natural },
            { "pearl",         0xffede6da, 0xffede6da, { 0.00f, 0.45f, 0.20f, 0.70f, 0.0f, 0.25f, 0.5f }, Finish::natural },
            { "smooth",        0xff2b2f36, 0xff2b2f36, { 0.00f, 0.20f, 0.50f, 0.00f, 0.0f, 0.00f, 1.0f }, Finish::natural },
            { "clearglass",    0xffdcebf2, 0xffdcebf2, { 0.00f, 0.90f, 0.03f, 1.00f, 0.0f, 0.00f, 1.0f }, Finish::polished },
            { "tintedglass",   0xff2f4b5c, 0xff2f4b5c, { 0.00f, 0.90f, 0.03f, 1.00f, 0.0f, 0.00f, 1.0f }, Finish::polished },
        };

        const auto i = (int) m;
        return table[i >= 0 && i < (int) Material::count ? i : 0];
    }

    /** A material by name (any case, spaces and dashes ignored); powder coat if it's none. */
    inline Material materialNamed (const std::string& name) noexcept
    {
        const auto n = lowered (name);

        if (n == "metal" || n == "brushed" || n == "aluminum" || n == "brushedmetal" || n == "brushedaluminium") return Material::aluminium;
        if (n == "stainless" || n == "stainlesssteel" || n == "brushedsteel")  return Material::steel;
        if (n == "mirror" || n == "polishedchrome")                            return Material::chrome;
        if (n == "polishedsilver" || n == "platinum" || n == "nickel")         return Material::silver;
        if (n == "rose" || n == "pinkgold")                                    return Material::roseGold;
        if (n == "liquidtitanium" || n == "liquidmetal" || n == "mercury")     return Material::titanium;
        if (n == "castiron")                                                   return Material::iron;
        if (n == "hammered")                                                   return Material::hammertone;
        if (n == "anodised")                                                   return Material::anodized;
        if (n == "carbonfibre" || n == "carbonfiber")                          return Material::carbon;
        if (n == "stone")                                                      return Material::granite;
        if (n == "glossyplastic" || n == "gloss" || n == "lacquer")            return Material::plastic;
        if (n == "matte" || n == "softtouch")                                  return Material::mattePlastic;
        if (n == "enamel" || n == "porcelain")                                 return Material::ceramic;
        if (n == "walnut" || n == "oak" || n == "rosewood" || n == "ebony"
            || n == "whitewood" || n == "maple" || n == "bleachedwood" || n == "orangewood" || n == "lightorangewood"
            || n == "bordowood" || n == "bordeauxwood" || n == "burgundywood" || n == "bordo"
            || n == "greywood" || n == "graywood" || n == "darkgreywood" || n == "darkgraywood")  return Material::wood;
        if (n == "guitarwood" || n == "flamedmaple" || n == "flamemaple" || n == "guitartop" || n == "flamedwood")  return Material::flamewood;
        if (n == "fabric" || n == "cloth")                                     return Material::felt;
        if (n == "screen" || n == "darkglass" || n == "screenglass")           return Material::glass;
        if (n == "clear" || n == "transparent" || n == "transparentglass" || n == "seethrough" || n == "seethroughglass"
            || n == "acrylic" || n == "perspex" || n == "plexiglass" || n == "crystal")  return Material::clearGlass;
        if (n == "tinted" || n == "smokedglass" || n == "smoke" || n == "smokyglass" || n == "colouredglass" || n == "coloredglass"
            || n == "stainedglass" || n == "tintglass")                         return Material::tintedGlass;
        if (n == "frostedglass")                                               return Material::frosted;
        if (n == "motherofpearl")                                              return Material::pearl;
        if (n == "flat" || n == "gradient" || n == "plain")                    return Material::smooth;

        for (int i = 0; i < (int) Material::count; ++i)
            if (n == infoOf ((Material) i).name)
                return (Material) i;

        return Material::powder;
    }

    /** Wood colours by name, for "walnut", "oak" and the like: its own colour and its grain's. */
    inline bool woodColours (const std::string& name, Rgb& colour, Rgb& grain) noexcept
    {
        const auto n = lowered (name);
        const auto set = [&] (std::uint32_t main, std::uint32_t second) { colour = rgbOf (main); grain = rgbOf (second); return true; };

        if (n == "walnut")   return set (0xff6a4024, 0xff3e2412);
        if (n == "oak")      return set (0xff9a6a3a, 0xff3e2412);
        if (n == "rosewood") return set (0xff5a2a1a, 0xff3e2412);
        if (n == "ebony")    return set (0xff2a1d16, 0xff3e2412);

        // Bleached maple: near white, a pale tan grain.
        if (n == "whitewood" || n == "maple" || n == "bleachedwood")              return set (0xffe9dfcb, 0xffbfa982);
        // Light orange: cherry or teak, honey-orange with a deeper grain.
        if (n == "orangewood" || n == "lightorangewood")                          return set (0xffe0954e, 0xff9c5222);
        // Bordeaux: a deep wine stain over the grain.
        if (n == "bordowood" || n == "bordeauxwood" || n == "burgundywood" || n == "bordo") return set (0xff74202c, 0xff330a12);
        // Dark grey: weathered or ash-stained, a charcoal grain.
        if (n == "greywood" || n == "graywood" || n == "darkgreywood" || n == "darkgraywood") return set (0xff5f5b56, 0xff2a2724);
        // The yellow flamed maple of a guitar's top, under lacquer.
        if (n == "guitarwood" || n == "flamedmaple" || n == "flamemaple" || n == "guitartop" || n == "flamedwood") return set (0xffe8b62e, 0xffb06a14);
        return false;
    }

    inline bool woodColour (const std::string& name, Rgb& colour) noexcept
    {
        Rgb grain;
        return woodColours (name, colour, grain);
    }

    inline Finish finishNamed (const std::string& name, Finish fallback) noexcept
    {
        const auto n = lowered (name);
        if (n == "polished" || n == "mirror" || n == "gloss" || n == "glossy") return Finish::polished;
        if (n == "satin" || n == "silk")                                      return Finish::satin;
        if (n == "brushed")                                                   return Finish::brushed;
        if (n == "hammered" || n == "hammertone")                             return Finish::hammered;
        if (n == "sandblasted" || n == "bead" || n == "beadblasted" || n == "cast") return Finish::sandblasted;
        if (n == "matte" || n == "matt")                                      return Finish::matte;
        if (n == "liquid" || n == "molten")                                   return Finish::liquid;
        if (n == "natural")                                                   return Finish::natural;
        return fallback;
    }

    //==========================================================================
    // Patterns: a second layer over the material, as in Photoshop: a design drawn in a colour
    // with an opacity and a blend mode, or pressed into the surface (emboss, deboss, engrave).

    enum class Pattern
    {
        none, stripes, pinstripes, checkers, dots, grid, hexagons, diamondPlate, knurl, crosshatch, waves, chevron, scales, rings,
        guilloche, sunburst, perforated, weave, noise, camo, damascus, bricks, triangles, count
    };

    inline const char* patternName (Pattern p) noexcept
    {
        static const char* const names[] { "none", "stripes", "pinstripes", "checkers", "dots", "grid", "hexagons", "diamondplate", "knurl",
                                           "crosshatch", "waves", "chevron", "scales", "rings", "guilloche", "sunburst", "perforated",
                                           "weave", "noise", "camo", "damascus", "bricks", "triangles" };
        const auto i = (int) p;
        return names[i >= 0 && i < (int) Pattern::count ? i : 0];
    }

    inline Pattern patternNamed (const std::string& name) noexcept
    {
        const auto n = lowered (name);
        if (n == "honeycomb" || n == "hex")                      return Pattern::hexagons;
        if (n == "treadplate" || n == "checkerplate" || n == "diamond") return Pattern::diamondPlate;
        if (n == "knurled" || n == "knurling")                   return Pattern::knurl;
        if (n == "polkadots" || n == "polka")                    return Pattern::dots;
        if (n == "checkerboard" || n == "checks")                return Pattern::checkers;
        if (n == "pinstripe")                                    return Pattern::pinstripes;
        if (n == "stripe")                                       return Pattern::stripes;
        if (n == "fishscales")                                   return Pattern::scales;
        if (n == "concentric" || n == "vinyl" || n == "circles") return Pattern::rings;
        if (n == "engineturned" || n == "guillochee")           return Pattern::guilloche;
        if (n == "radial" || n == "rays")                        return Pattern::sunburst;
        if (n == "holes" || n == "perforation" || n == "mesh")   return Pattern::perforated;
        if (n == "carbonweave" || n == "twill")                  return Pattern::weave;
        if (n == "camouflage")                                   return Pattern::camo;
        if (n == "zigzag" || n == "herringbone")                 return Pattern::chevron;
        if (n == "brick")                                        return Pattern::bricks;

        for (int i = 0; i < (int) Pattern::count; ++i)
            if (n == patternName ((Pattern) i))
                return (Pattern) i;

        return Pattern::none;
    }

    enum class Blend { normal, multiply, screen, overlay, softLight, add, darken, lighten, emboss, deboss, engrave };

    inline Blend blendNamed (const std::string& name, Blend fallback) noexcept
    {
        const auto n = lowered (name);
        if (n == "normal")                     return Blend::normal;
        if (n == "multiply")                   return Blend::multiply;
        if (n == "screen")                     return Blend::screen;
        if (n == "overlay")                    return Blend::overlay;
        if (n == "softlight" || n == "soft")   return Blend::softLight;
        if (n == "add" || n == "lineardodge")  return Blend::add;
        if (n == "darken")                     return Blend::darken;
        if (n == "lighten")                    return Blend::lighten;
        if (n == "emboss" || n == "raise" || n == "raised") return Blend::emboss;
        if (n == "deboss" || n == "press" || n == "pressed") return Blend::deboss;
        if (n == "engrave" || n == "engraved" || n == "etch") return Blend::engrave;
        return fallback;
    }

    struct PatternLayer
    {
        Pattern pattern = Pattern::none;
        Rgb colour { 0.0f, 0.0f, 0.0f };
        bool hasColour = false;
        float opacity = 0.5f;     // 0..1
        Blend blend = Blend::normal;
        float size = 24.0f;       // the pattern's cell, in pixels
        float angle = 0.0f;       // degrees
        float width = -1.0f;      // its lines' or shapes' share of the cell (0..1; -1: the pattern's own)
        float depth = 1.5f;       // emboss, deboss and engrave: how deep, in pixels
        float centreX = 0.5f, centreY = 0.5f;   // rings, sunburst, guilloche: their centre, 0..1 of the area
        int count = 48;           // sunburst: its rays
        bool invert = false;
        int seed = 1;
    };

    /** Where a texture lies: the area it's measured against (the panel, or a part's box). */
    struct Frame { float x = 0.0f, y = 0.0f, w = 1.0f, h = 1.0f; };

    /** A pattern's value at a point, 0..1 (where it is: 1). */
    inline float patternAt (const PatternLayer& p, float x, float y, const Frame& frame) noexcept
    {
        const float cell = p.size < 2.0f ? 2.0f : p.size;
        const float a = p.angle * 0.0174533f, ca = std::cos (a), sa = std::sin (a);
        const float ox = frame.x + frame.w * p.centreX, oy = frame.y + frame.h * p.centreY;
        const float rx = x - ox, ry = y - oy;
        const float u = (rx * ca + ry * sa) / cell, v = (-rx * sa + ry * ca) / cell;
        const float fw = 1.0f / cell;   // a pixel, in cells: the edges' smoothing
        auto band = [fw] (float d, float half) { return 1.0f - smoothstep (half - fw, half + fw, d); };
        auto width = [&p] (float fallback) { return p.width >= 0.0f ? p.width : fallback; };
        float value = 0.0f;

        switch (p.pattern)
        {
            case Pattern::none:
            case Pattern::count:      return 0.0f;

            case Pattern::stripes:    value = band (std::fabs (fract (u) - 0.5f), width (0.5f) * 0.5f); break;
            case Pattern::pinstripes: value = band (std::fabs (fract (u) - 0.5f), width (0.08f) * 0.5f); break;

            case Pattern::checkers:
            {
                const float s = std::sin (3.14159265f * u) * std::sin (3.14159265f * v);
                value = smoothstep (-fw * 3.0f, fw * 3.0f, s);
                break;
            }

            case Pattern::dots:
            case Pattern::perforated:
            {
                const float du = fract (u) - 0.5f, dv = fract (v) - 0.5f;
                value = band (std::sqrt (du * du + dv * dv), width (p.pattern == Pattern::dots ? 0.5f : 0.6f) * 0.5f);
                break;
            }

            case Pattern::grid:
            {
                const float du = std::fmin (fract (u), 1.0f - fract (u)), dv = std::fmin (fract (v), 1.0f - fract (v));
                value = band (std::fmin (du, dv), width (0.08f) * 0.5f);
                break;
            }

            case Pattern::hexagons:
            {
                auto modulo = [] (float t, float m) { return t - m * std::floor (t / m); };
                const float ax = modulo (u, 1.0f) - 0.5f, ay = modulo (v, 1.732f) - 0.866f;
                const float bx = modulo (u - 0.5f, 1.0f) - 0.5f, by = modulo (v - 0.866f, 1.732f) - 0.866f;
                const bool first = ax * ax + ay * ay < bx * bx + by * by;
                const float gx = std::fabs (first ? ax : bx), gy = std::fabs (first ? ay : by);
                const float hexDist = std::fmax (gx * 0.5f + gy * 0.866f, gx);
                value = band (0.5f - hexDist, width (0.08f) * 0.5f);
                break;
            }

            case Pattern::diamondPlate:
            {
                // Raised lozenges, turned one way then the other, cell by cell (tread plate).
                const bool turn = (((int) std::floor (u) + (int) std::floor (v)) & 1) != 0;
                const float du = fract (u) - 0.5f, dv = fract (v) - 0.5f, r = 0.7071f;
                const float pu = turn ? (du + dv) * r : (du - dv) * r, pv = turn ? (dv - du) * r : (du + dv) * r;
                const float d = std::fabs (pu) / 0.42f + std::fabs (pv) / 0.13f - 1.0f;
                value = smooth (-d * 3.0f);
                break;
            }

            case Pattern::knurl:
            {
                const float du = std::fabs (fract (u) - 0.5f), dv = std::fabs (fract (v) - 0.5f);
                value = clamp01 (1.0f - (du + dv) * 1.9f);
                break;
            }

            case Pattern::crosshatch:
            {
                const float d1 = std::fabs (fract (u + v) - 0.5f), d2 = std::fabs (fract (u - v) - 0.5f);
                value = std::fmax (band (d1, width (0.1f) * 0.5f), band (d2, width (0.1f) * 0.5f));
                break;
            }

            case Pattern::waves:
                value = band (std::fabs (fract (v + 0.18f * std::sin (6.2831853f * u * 0.5f)) - 0.5f), width (0.35f) * 0.5f);
                break;

            case Pattern::chevron:
                value = band (std::fabs (fract (v + std::fabs (fract (u) - 0.5f)) - 0.5f), width (0.4f) * 0.5f);
                break;

            case Pattern::scales:
            {
                const float row = std::floor (v), off = ((int) row & 1) ? 0.5f : 0.0f;
                const float du = fract (u + off) - 0.5f, dv = fract (v);
                const float d = std::sqrt (du * du + dv * dv);
                value = band (std::fabs (d - 0.5f), width (0.08f) * 0.5f) * (dv < 0.9f ? 1.0f : 0.0f);
                break;
            }

            case Pattern::rings:
            {
                const float r = std::sqrt (u * u + v * v);
                value = band (std::fabs (fract (r) - 0.5f), width (0.4f) * 0.5f);
                break;
            }

            case Pattern::guilloche:
            {
                // Engine turning: waves from centres on a circle, overlapping as on a watch's dial.
                float sum = 0.0f;

                for (int k = 0; k < 6; ++k)
                {
                    const float t = 6.2831853f * (float) k / 6.0f;
                    const float dx = u - 0.35f * std::cos (t), dy = v - 0.35f * std::sin (t);
                    sum += std::sin (6.2831853f * std::sqrt (dx * dx + dy * dy) * 1.5f);
                }

                value = 0.5f + sum / 12.0f;
                break;
            }

            case Pattern::sunburst:
            {
                const float t = std::atan2 (v, u) / 6.2831853f + 0.5f;
                const float r = std::sqrt (u * u + v * v);
                const float local = fw / std::fmax (0.02f, r * 6.2831853f / (float) (p.count < 4 ? 4 : p.count));
                value = 1.0f - smoothstep (width (0.5f) * 0.5f - local, width (0.5f) * 0.5f + local, std::fabs (fract (t * (float) p.count) - 0.5f));
                break;
            }

            case Pattern::weave:
            {
                const int cx = (int) std::floor (u * 2.0f), cy = (int) std::floor (v * 2.0f);
                const bool across = ((cx + cy) & 1) == 0;
                const float s = (across ? v : u) * 2.0f, t = (across ? u : v) * 2.0f;
                const float thread = 0.5f + 0.5f * std::cos ((fract (s) - 0.5f) * 3.14159f);
                const float over = 0.65f + 0.35f * std::sin (fract (t) * 3.14159f);
                value = (across ? 0.75f : 1.0f) * thread * over;
                break;
            }

            case Pattern::noise:  value = fbm (u * 2.0f, v * 2.0f, p.seed, 4); break;

            case Pattern::camo:
            {
                const float n = fbm (u * 0.6f, v * 0.6f, p.seed, 4);
                value = n < 0.42f ? 0.0f : (n < 0.52f ? 0.5f : 1.0f);
                break;
            }

            case Pattern::damascus:
            {
                const float warp = fbm (u * 0.35f, v * 0.35f, p.seed, 4) * 3.0f;
                value = smooth (0.5f + 0.5f * std::sin ((v + warp) * 6.2831853f) * 1.6f);
                break;
            }

            case Pattern::bricks:
            {
                const float row = std::floor (v * 2.0f), off = ((int) row & 1) ? 0.5f : 0.0f;
                const float du = std::fmin (fract (u + off), 1.0f - fract (u + off)), dv = std::fmin (fract (v * 2.0f), 1.0f - fract (v * 2.0f)) * 0.5f;
                value = band (std::fmin (du, dv), width (0.06f) * 0.5f);
                break;
            }

            case Pattern::triangles:
            {
                const float d1 = std::fabs (fract (v * 1.1547f) - 0.5f);
                const float d2 = std::fabs (fract ((u * 0.866f + v * 0.5f) * 1.1547f) - 0.5f);
                const float d3 = std::fabs (fract ((-u * 0.866f + v * 0.5f) * 1.1547f) - 0.5f);
                value = std::fmax (band (0.5f - d1, width (0.08f) * 0.5f), std::fmax (band (0.5f - d2, width (0.08f) * 0.5f), band (0.5f - d3, width (0.08f) * 0.5f)));
                break;
            }
        }

        value = clamp01 (value);
        return p.invert ? 1.0f - value : value;
    }

    //==========================================================================
    /** A surface: a material in a colour and finish, with pattern layers over it. */
    struct Surfacing
    {
        Material material = Material::powder;
        Rgb colour, colour2;
        Finish finish = Finish::natural;
        float scale = 1.0f;       // the texture's size (2: twice as big)
        float angle = 0.0f;       // the direction of brushing, grain, veins (degrees)
        float grain = 1.0f;       // how strong the texture is (0 smooth .. 2 coarse)
        float shine = -1.0f;      // 0 matte .. 1 mirror (-1: the material's own)
        float flow = -1.0f;       // liquid swirls 0..1 (-1: the material's own)
        int seed = 1;
        std::vector<PatternLayer> patterns;

        /** A material with its own colours and finish. */
        static Surfacing of (Material m, int seed = 1)
        {
            Surfacing s;
            s.material = m;
            s.colour = rgbOf (infoOf (m).colour);
            s.colour2 = rgbOf (infoOf (m).colour2);
            s.finish = infoOf (m).finish;
            s.seed = seed;
            return s;
        }

        /** How it answers the light: the material's, as its finish changes it. */
        Optics optics() const noexcept
        {
            auto o = infoOf (material).optics;
            const bool metallic = o.metal > 0.3f;

            switch (finish)
            {
                case Finish::polished:    o.rough = std::fmin (o.rough, metallic ? 0.10f : 0.15f); o.coat = metallic ? o.coat : std::fmax (o.coat, 0.7f); o.spec = std::fmax (o.spec, 0.45f); break;
                case Finish::satin:       o.rough = std::fmax (o.rough, 0.32f); break;
                case Finish::brushed:     o.rough = std::fmax (o.rough, 0.30f); break;
                case Finish::hammered:    o.rough = std::fmin (std::fmax (o.rough, 0.22f), 0.4f); o.bump = 1.0f; break;
                case Finish::sandblasted: o.rough = std::fmax (o.rough, 0.65f); o.coat = 0.0f; break;
                case Finish::matte:       o.rough = std::fmax (o.rough, 0.85f); o.coat = 0.0f; o.spec *= 0.4f; break;
                case Finish::liquid:      o.rough = std::fmin (o.rough, 0.08f); o.flow = std::fmax (o.flow, 1.0f); break;
                case Finish::natural:     break;
            }

            if (shine >= 0.0f)
            {
                o.rough = mix (0.95f, 0.04f, clamp01 (shine));
                o.coat = metallic ? o.coat : clamp01 (shine) * 0.9f;
            }

            if (flow >= 0.0f)
                o.flow = clamp01 (flow);

            return o;
        }
    };

    /** A point of a surface before it's lit: its colour, and its relief (pixels up). */
    struct Texel
    {
        Rgb albedo;
        float height = 0.0f;
    };

    inline Texel texelAt (const Surfacing& s, float x, float y, const Frame& frame) noexcept
    {
        const float a = s.angle * 0.0174533f, ca = std::cos (a), sa = std::sin (a);
        const float k = 1.0f / (s.scale <= 0.05f ? 0.05f : s.scale);
        const float u = (x * ca + y * sa) * k, v = (-x * sa + y * ca) * k;   // along the grain, across it
        const float g = s.grain;
        const int seed = s.seed;
        Texel t { s.colour, 0.0f };

        switch (s.material)
        {
            case Material::marble:
            case Material::blackMarble:
            {
                const float turbulence = fbm (u * 0.006f, v * 0.006f, seed, 6);
                const float vein = std::fabs (std::sin ((u * 0.004f + v * 0.0015f + turbulence * 5.0f) * 3.14159f));
                const float fine = std::fabs (std::sin ((u * 0.011f - v * 0.004f + fbm (u * 0.02f, v * 0.02f, seed + 5, 4) * 4.0f) * 3.14159f));
                const float veins = (1.0f - smoothstep (0.0f, 0.06f, vein)) * 0.9f + (1.0f - smoothstep (0.0f, 0.03f, fine)) * 0.35f;
                const float cloud = fbm (u * 0.01f, v * 0.01f, seed + 9, 4);
                t.albedo = mix (scale (s.colour, 0.94f + 0.1f * cloud), s.colour2, clamp01 (veins * g));
                break;
            }

            case Material::granite:
            case Material::terrazzo:
            {
                const bool terrazzo = s.material == Material::terrazzo;
                float which = 0.0f;
                const float d = cells (u * (terrazzo ? 0.06f : 0.35f), v * (terrazzo ? 0.06f : 0.35f), seed, &which);
                const float speck = hash ((int) (u * 0.9f), (int) (v * 0.9f), seed + 3);
                Rgb chip = which < 0.33f ? s.colour2 : (which < 0.6f ? Rgb { 0.95f, 0.94f, 0.92f } : (which < 0.8f ? Rgb { 0.12f, 0.12f, 0.13f } : scale (s.colour, 0.7f)));

                if (terrazzo)
                    t.albedo = d < 0.32f + which * 0.1f && which > 0.3f ? mix (s.colour, chip, clamp01 (g)) : add (s.colour, (speck - 0.5f) * 0.04f * g);
                else
                    t.albedo = mix (s.colour, chip, clamp01 ((0.55f - d) * 2.0f) * 0.8f * clamp01 (g) + (speck > 0.92f ? 0.3f : 0.0f));

                break;
            }

            case Material::slate:
            {
                const float layers = fbm (u * 0.004f, v * 0.06f, seed, 5);
                const float cleave = 1.0f - smoothstep (0.0f, 0.025f, std::fabs (fract (v * 0.03f + layers * 2.0f) - 0.5f));
                t.albedo = add (mix (s.colour, s.colour2, layers), (fbm (u * 0.3f, v * 0.3f, seed + 2, 2) - 0.5f) * 0.06f * g);
                t.height = (layers - 0.5f) * 1.2f * g - cleave * 0.6f * g;
                break;
            }

            case Material::concrete:
            case Material::sandstone:
            {
                const bool sand = s.material == Material::sandstone;
                const float mottle = fbm (u * 0.012f, v * (sand ? 0.05f : 0.012f), seed, 5);
                const float pit = hash ((int) (u * 0.5f), (int) (v * 0.5f), seed + 4);
                t.albedo = add (mix (s.colour, s.colour2, mottle * (sand ? 0.9f : 0.6f)), (hash ((int) u, (int) v, seed + 1) - 0.5f) * 0.06f * g);
                t.height = (fbm (u * 0.25f, v * 0.25f, seed + 6, 3) - 0.5f) * 0.8f * g;

                if (! sand && pit > 0.985f)
                {
                    t.albedo = scale (t.albedo, 0.55f);
                    t.height -= 1.0f * g;
                }

                break;
            }

            case Material::wood:
            case Material::burl:
            {
                if (s.material == Material::burl)
                {
                    const float swirl = fbm (u * 0.01f, v * 0.01f, seed, 5) * 12.0f;
                    float which = 0.0f;
                    const float eye = cells (u * 0.03f, v * 0.03f, seed + 3, &which);
                    const float rings = 0.5f + 0.5f * std::sin ((swirl + eye * 9.0f) * 3.14159f);
                    t.albedo = mix (scale (s.colour, 1.15f), s.colour2, clamp01 ((std::pow (rings, 2.5f) * 0.7f + (eye < 0.08f ? 0.6f : 0.0f)) * g));
                }
                else
                {
                    const float warp = fbm (u * 0.004f, v * 0.03f, seed, 4) * 6.0f;
                    const float rings = 0.5f + 0.5f * std::sin ((v * 0.06f + warp) * 3.14159f);
                    const float fibre = fbm (u * 0.05f, v * 0.8f, seed + 3, 3);
                    const float tt = (std::pow (rings, 3.0f) * 0.55f + fibre * 0.45f) * clamp01 (g);
                    t.albedo = mix (scale (s.colour, 1.12f), s.colour2, tt * 0.85f);
                    t.height = (fibre - 0.5f) * 0.35f * g;
                }

                break;
            }

            case Material::flamewood:
            {
                // Figured maple, a guitar's top: a fine straight grain, and the flame across it,
                // rippling bands that catch the light as it turns.
                const float warp = fbm (u * 0.004f, v * 0.03f, seed, 4) * 3.0f;
                const float rings = 0.5f + 0.5f * std::sin ((v * 0.05f + warp) * 3.14159f);
                const float fibre = fbm (u * 0.05f, v * 0.8f, seed + 3, 3);
                const float ripple = (fbm (u * 0.006f, v * 0.012f, seed + 7, 3) - 0.5f) * 2.4f + std::sin (v * 0.02f + u * 0.003f) * 0.6f;
                const float flame = std::pow (0.5f + 0.5f * std::sin ((u * 0.2f + ripple) * 3.14159f), 3.0f);
                const float tt = (std::pow (rings, 4.0f) * 0.2f + fibre * 0.3f + flame * 0.4f) * clamp01 (g);
                t.albedo = mix (scale (s.colour, 1.08f), s.colour2, tt * 0.7f);
                t.height = (flame - 0.5f) * 0.45f * g + (fibre - 0.5f) * 0.15f * g;
                break;
            }

            case Material::leather:
            case Material::tolex:
            {
                const float c = cells (u * (s.material == Material::tolex ? 0.18f : 0.09f), v * (s.material == Material::tolex ? 0.18f : 0.09f), seed);
                const float bump = smooth (c * 1.6f);
                t.albedo = add (scale (s.colour, 1.0f + (bump - 0.7f) * 0.2f * g), (fbm (u * 0.3f, v * 0.3f, seed + 9, 2) - 0.5f) * 0.05f * g);
                t.height = bump * 1.4f * g;
                break;
            }

            case Material::felt:
            {
                const float fibres = fbm (u * 0.6f, v * 0.6f, seed, 3) * 0.6f + hash ((int) u, (int) v, seed) * 0.4f;
                t.albedo = scale (s.colour, 1.0f + (fibres - 0.5f) * 0.18f * g);
                t.height = (fibres - 0.5f) * 0.4f * g;
                break;
            }

            case Material::carbon:
            {
                const float cell = 6.0f;
                const int cx = (int) std::floor (u / cell), cy = (int) std::floor (v / cell);
                const bool across = ((cx + cy) & 1) == 0;
                const float pu = (across ? u : v) / cell, pv = (across ? v : u) / cell;
                const float thread = 0.5f + 0.5f * std::cos ((fract (pv) - 0.5f) * 3.14159f);
                const float weave = 0.65f + 0.35f * std::sin (fract (pu) * 3.14159f);
                t.albedo = scale (s.colour, mix (1.0f, (across ? 0.8f : 1.2f) * (0.55f + 0.45f * thread * weave), clamp01 (g)));
                t.height = thread * weave * 0.6f * g;
                break;
            }

            case Material::paper:
            {
                const float fibres = fbm (u * 0.08f, v * 0.08f, seed, 5);
                const float blotch = fbm (u * 0.006f, v * 0.006f, seed + 4, 3);
                t.albedo = add (scale (s.colour, 1.0f + (blotch - 0.5f) * 0.12f * g), (fibres - 0.5f) * 0.08f * g);
                t.height = (fibres - 0.5f) * 0.3f * g;
                break;
            }

            case Material::glass:
            {
                const float h = frame.h < 1.0f ? 1.0f : frame.h;
                t.albedo = scale (s.colour, 0.65f + 0.35f * (1.0f - (y - frame.y) / h));
                break;
            }

            case Material::frosted:
                t.albedo = add (s.colour, (hash ((int) x, (int) y, seed) - 0.5f) * 0.05f * g);
                t.height = (hash ((int) x, (int) y, seed + 1) - 0.5f) * 0.25f * g;
                break;

            case Material::pearl:
            {
                float which = 0.0f;
                const float d = cells (u * 0.02f, v * 0.02f, seed, &which);
                t.albedo = mix (s.colour, times (s.colour, add (scale (rainbow (which * 0.5f + d * 0.4f), 0.16f), 0.92f)), clamp01 (g));
                t.height = (1.0f - d) * 0.4f * g;
                break;
            }

            case Material::bakelite:
            {
                const float mottle = fbm (u * 0.015f, v * 0.015f, seed, 5);
                t.albedo = mix (s.colour, s.colour2, smooth (mottle * 1.4f - 0.2f) * clamp01 (g));
                break;
            }

            case Material::ceramic:
            case Material::plastic:
                t.albedo = add (s.colour, (fbm (u * 0.9f, v * 0.9f, seed, 2) - 0.5f) * 0.015f * g);
                break;

            case Material::mattePlastic:
            case Material::rubber:
                t.albedo = add (s.colour, (hash ((int) x, (int) y, seed) - 0.5f) * 0.03f * g);
                t.height = (fbm (u * 0.8f, v * 0.8f, seed, 2) - 0.5f) * 0.3f * g;
                break;

            case Material::smooth:
            case Material::clearGlass:
            case Material::tintedGlass:
                break;

            case Material::powder:
                t.albedo = add (s.colour, (fbm (u * 0.6f, v * 0.6f, seed, 2) - 0.5f) * 0.05f * g + (hash ((int) x, (int) y, seed + 2) - 0.5f) * 0.03f * g);
                t.height = (fbm (u * 0.35f, v * 0.35f, seed + 3, 2) - 0.5f) * 0.25f * g;   // a fine orange peel
                break;

            case Material::aluminium: case Material::steel: case Material::silver: case Material::chrome: case Material::gold:
            case Material::roseGold: case Material::brass: case Material::copper: case Material::bronze: case Material::titanium:
            case Material::gunmetal: case Material::iron: case Material::hammertone: case Material::anodized: case Material::count:
                break;
        }

        // A second colour on a material whose texture doesn't use one: a two-tone shade across
        // it, its own colour towards the upper left (where the light comes from), the second
        // towards the lower right.
        switch (s.material)
        {
            case Material::marble: case Material::blackMarble: case Material::granite: case Material::terrazzo: case Material::slate:
            case Material::concrete: case Material::sandstone: case Material::wood: case Material::burl: case Material::flamewood:
            case Material::bakelite:
                break;

            default:
            {
                const Rgb shift { s.colour2.r - s.colour.r, s.colour2.g - s.colour.g, s.colour2.b - s.colour.b };

                if (std::fabs (shift.r) + std::fabs (shift.g) + std::fabs (shift.b) > 0.004f && frame.w > 0.0f && frame.h > 0.0f)
                {
                    const float dx = (x - frame.x) / frame.w * 2.0f - 1.0f, dy = (y - frame.y) / frame.h * 2.0f - 1.0f;
                    t.albedo = add (t.albedo, shift, clamp01 (0.5f + 0.5f * (0.5f * dx + 0.866f * dy) / 1.366f));
                }

                break;
            }
        }

        // The finish's own texture (metals mostly; any material can take one).
        switch (s.finish)
        {
            case Finish::brushed:
            {
                // Fine streaks along the grain, a few longer ones, a hair of grit.
                const float streak = noise (u * 0.03f, v * 2.2f, seed) * 0.55f + noise (u * 0.008f, v * 0.7f, seed + 5) * 0.45f;
                t.albedo = add (t.albedo, (streak - 0.5f) * 0.07f * g + (hash ((int) x, (int) y, seed) - 0.5f) * 0.015f * g);
                t.height += (streak - 0.5f) * 0.12f * g;
                break;
            }

            case Finish::hammered:
            {
                const float c = cells (u * 0.075f, v * 0.075f, seed + 11);
                t.height += -(1.0f - smooth (c * 1.3f)) * 2.2f * g;
                break;
            }

            case Finish::satin:
                t.albedo = add (t.albedo, (hash ((int) x, (int) y, seed + 13) - 0.5f) * 0.02f * g);
                t.height += (fbm (u * 0.15f, v * 0.15f, seed + 13, 2) - 0.5f) * 0.05f * g;
                break;

            case Finish::sandblasted:
                t.albedo = add (t.albedo, (hash ((int) x, (int) y, seed + 7) - 0.5f) * 0.05f * g);
                t.height += (hash ((int) x, (int) y, seed + 8) - 0.5f) * 0.35f * g;
                break;

            case Finish::polished:
            case Finish::liquid:
                t.height += (fbm (u * 0.004f, v * 0.004f, seed + 15, 3) - 0.5f) * 1.2f;   // a polished sheet is never quite flat
                break;

            case Finish::matte:
            case Finish::natural:
                break;
        }

        // The pattern layers, bottom to top.
        for (const auto& layer : s.patterns)
        {
            if (layer.pattern == Pattern::none || layer.opacity <= 0.0f)
                continue;

            const float value = patternAt (layer, x, y, frame);
            const float amount = clamp01 (value * layer.opacity);
            const bool raise = layer.blend == Blend::emboss || layer.blend == Blend::deboss || layer.blend == Blend::engrave;
            const Rgb ink = layer.hasColour ? layer.colour
                          : (layer.blend == Blend::screen || layer.blend == Blend::add || layer.blend == Blend::lighten) ? Rgb { 1.0f, 1.0f, 1.0f }
                                                                                                                         : Rgb { 0.0f, 0.0f, 0.0f };
            auto& c = t.albedo;

            if (raise)
            {
                t.height += value * layer.depth * (layer.blend == Blend::emboss ? 1.0f : -1.0f) * clamp01 (layer.opacity * 2.0f);

                if (layer.blend == Blend::engrave)
                    c = mix (c, layer.hasColour ? ink : scale (c, 0.55f), amount);
                else if (layer.hasColour)
                    c = mix (c, ink, amount);

                continue;
            }

            auto channel = [&layer] (float base, float top) -> float
            {
                switch (layer.blend)
                {
                    case Blend::multiply:  return base * top;
                    case Blend::screen:    return 1.0f - (1.0f - base) * (1.0f - top);
                    case Blend::overlay:   return base < 0.5f ? 2.0f * base * top : 1.0f - 2.0f * (1.0f - base) * (1.0f - top);
                    case Blend::softLight: return (1.0f - 2.0f * top) * base * base + 2.0f * top * base;
                    case Blend::add:       return base + top;
                    case Blend::darken:    return std::fmin (base, top);
                    case Blend::lighten:   return std::fmax (base, top);
                    default:               return top;
                }
            };

            c = { mix (c.r, channel (c.r, ink.r), amount), mix (c.g, channel (c.g, ink.g), amount), mix (c.b, channel (c.b, ink.b), amount) };
        }

        return t;
    }

    //==========================================================================
    /** The room the panel sits in, as a shiny surface mirrors it: a bright sky above a dark
        floor with a soft horizon, the key light's softbox where the light comes from and a
        strip light opposite. r: the reflected direction (x right, y down, z towards the
        viewer). rough blurs it. */
    inline float studio (float rx, float ry, float rz, float rough, const struct Light& light) noexcept
    {
        const float up = -ry;
        const float blur = 0.08f + rough * 0.6f;
        float e = mix (0.28f, 1.05f, smoothstep (-0.12f - blur, 0.10f + blur, up + 0.02f));

        const float dx = rx - light.x, dy = ry - light.y, dz = rz - light.z;
        const float sigma = 0.16f + rough * 0.45f;
        e += 0.9f * std::exp (-(dx * dx + dy * dy + dz * dz) / (2.0f * sigma * sigma)) * (1.0f - rough * 0.4f);

        const float sx = rx + light.tx * 0.55f, sw = 0.05f + rough * 0.3f;
        e += 0.35f * std::exp (-(sx * sx) / (2.0f * sw * sw)) * smoothstep (-0.4f, 0.3f, up);
        return e;
    }

    /** The view: an eye above the middle of the picture, far enough for a gentle perspective. */
    struct View
    {
        float x = 0.0f, y = 0.0f, z = 1000.0f;

        static View over (float width, float height) noexcept
        {
            const float far = 2.2f * (width > height ? width : height);
            return { width * 0.5f, height * 0.45f, far < 200.0f ? 200.0f : far };
        }
    };

    /** A surface lit: its albedo with its optics, at a point, its normal (nx, ny, nz: unit,
        z towards the viewer). Flat matte paint comes out its own colour; slopes facing the
        light are brighter; metals show the room, tinted; coats and shiny things catch the
        light's highlight. */
    inline Rgb lightAt (Rgb albedo, const Optics& o, float nx, float ny, float nz, float px, float py, const Light& light, const View& view) noexcept
    {
        float vx = view.x - px, vy = view.y - py, vz = view.z;
        const float vl = std::sqrt (vx * vx + vy * vy + vz * vz);
        vx /= vl; vy /= vl; vz /= vl;

        const float nv = nx * vx + ny * vy + nz * vz;
        const float rx = 2.0f * nv * nx - vx, ry = 2.0f * nv * ny - vy, rz = 2.0f * nv * nz - vz;

        // Colours that shift with the angle.
        if (o.iridescence > 0.0f)
            albedo = mix (albedo, times (albedo, add (scale (rainbow (nv * 2.2f + px * 0.0007f), 0.55f), 0.75f)), o.iridescence);

        // Paint and stone: the light on its slopes, against a flat surface.
        const float facing = std::fmax (0.0f, nx * light.x + ny * light.y + nz * light.z) / std::fmax (0.05f, light.z);
        const float lit = facing < 0.15f ? 0.15f : (facing > 2.4f ? 2.4f : facing);
        Rgb diffuse = scale (albedo, 1.0f + (lit - 1.0f) * 0.85f);

        // The room, mirrored: what metals are, and a glint on a coat.
        const float env = studio (rx, ry, rz, o.rough, light);
        Rgb colour = mix (diffuse, scale (albedo, env * 0.95f), o.metal);

        if (o.coat > 0.0f)
        {
            const float fresnel = 0.06f + 0.94f * std::pow (1.0f - clamp01 (nv), 5.0f);
            colour = add (colour, (env - 0.6f) * o.coat * (0.10f + fresnel * 0.5f) * (1.0f - o.rough * 0.8f));
        }

        // The highlight.
        float hx = light.x + vx, hy = light.y + vy, hz = light.z + vz;
        const float hl = std::sqrt (hx * hx + hy * hy + hz * hz);
        hx /= hl; hy /= hl; hz /= hl;
        const float tightness = 2.0f / (o.rough * o.rough + 0.0025f);
        const float highlight = std::pow (std::fmax (0.0f, nx * hx + ny * hy + nz * hz), tightness < 4.0f ? 4.0f : (tightness > 800.0f ? 800.0f : tightness));
        const Rgb glint = mix (Rgb { 1.0f, 1.0f, 1.0f }, add (scale (albedo, 0.7f), 0.3f), o.metal);
        return add (colour, glint, highlight * o.spec * (0.6f + (1.0f - o.rough) * 1.2f));
    }

    //==========================================================================
    /** Clear and tinted glass let what's under them show: only their body's tint, their rim
        and the light's reflections are solid. */
    inline bool isSeeThrough (Material m) noexcept    { return m == Material::clearGlass || m == Material::tintedGlass; }

    /** A piece of see-through glass, lit: what it lays over the panel at each point. */
    struct Glass
    {
        Rgb tint { 0.86f, 0.92f, 0.95f };   // its colour
        bool tinted = false;                // tinted glass: a stronger body colour
        float cx = 0.0f, cy = 0.0f;         // its middle, in pixels
        float halfW = 1.0f, halfH = 1.0f;   // half its size
        float rim = 4.0f;                   // how wide its rim is (pixels)
        Light light;

        /** At (px, py), inside its edge by `inside` pixels (0 at the edge), the edge's outward
            direction (gx, gy: unit): its colour times how solid it is (premultiplied), and how
            solid it is. Its body is a faint tint; the rim (where the glass is seen at a slant)
            is denser, bright on the side facing the light and dark on the other; a sheen and a
            streak lie across it on the light's side, and the light's hotspot near that corner. */
        void at (float px, float py, float inside, float gx, float gy, Rgb& premultiplied, float& alpha) const noexcept
        {
            const float e = 1.0f - smoothstep (0.0f, rim < 1.0f ? 1.0f : rim, inside);
            const float tilt = e * 1.6f;
            const float nx = gx * tilt, ny = gy * tilt;
            const float facing = (nx * light.x + ny * light.y + light.z) / std::sqrt (nx * nx + ny * ny + 1.0f);
            const float rimLight = clamp01 ((facing - light.z) * 2.4f) * e;
            const float rimShade = clamp01 ((light.z - facing) * 2.4f) * e;

            // Across it, along the light: -1 the far side .. 1 the side the light comes from.
            const float extent = halfW > halfH ? halfW : halfH;
            const float u = ((px - cx) * light.tx + (py - cy) * light.ty) / (extent < 1.0f ? 1.0f : extent);
            const float sheen = smoothstep (-0.15f, 0.95f, u) * 0.19f;
            const float streak = (1.0f - smoothstep (0.0f, 0.1f, std::fabs (u - 0.45f))) * 0.2f;

            // The light's own reflection, towards its corner.
            const float hx = cx + light.tx * halfW * 0.52f, hy = cy + light.ty * halfH * 0.52f;
            const float spot = std::fmax (1.5f, std::fmin (halfW, halfH) * 0.22f);
            const float dx = (px - hx) / spot, dy = (py - hy) / spot;
            const float hot = std::exp (-(dx * dx + dy * dy) * 1.5f) * 0.6f;

            // Layered as Photoshop would: the tinted body, the rim's shade over it, the light over all.
            float a = clamp01 ((tinted ? 0.32f : 0.07f) + e * (tinted ? 0.34f : 0.2f));
            Rgb c = scale (tint, a);

            const float dark = clamp01 (rimShade * 0.6f);
            c = add (scale (c, 1.0f - dark), scale (tint, 0.22f * dark));
            a = dark + a * (1.0f - dark);

            const float white = clamp01 (rimLight * 0.8f + sheen + streak + hot);
            c = add (scale (c, 1.0f - white), Rgb { white, white, white });
            a = white + a * (1.0f - white);

            premultiplied = c;
            alpha = a;
        }
    };

    //==========================================================================
    /** A shape, measured by its signed distance: negative inside, 0 on the edge, positive
        outside, in pixels. */
    struct Shape
    {
        enum class Kind { box, circle, line };

        Kind kind = Kind::box;
        float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;   // box and circle: the box (a circle fills it); line: from (x, y) to (w, h)
        float radius = 8.0f;                            // box: its corners; line: half its width

        static Shape box (float left, float top, float width, float height, float cornerRadius) noexcept { return { Kind::box, left, top, width, height, cornerRadius }; }
        static Shape circle (float cx, float cy, float r) noexcept       { return { Kind::circle, cx - r, cy - r, r * 2.0f, r * 2.0f, r }; }
        static Shape line (float x1, float y1, float x2, float y2, float halfWidth) noexcept { return { Kind::line, x1, y1, x2, y2, halfWidth }; }

        float distance (float px, float py) const noexcept
        {
            switch (kind)
            {
                case Kind::circle:
                {
                    const float r = std::fmin (w, h) * 0.5f, dx = px - (x + w * 0.5f), dy = py - (y + h * 0.5f);
                    return std::sqrt (dx * dx + dy * dy) - r;
                }

                case Kind::line:
                {
                    const float vx = w - x, vy = h - y, ux = px - x, uy = py - y;
                    const float len = vx * vx + vy * vy;
                    const float t = len > 0.0f ? clamp01 ((ux * vx + uy * vy) / len) : 0.0f;
                    const float dx = ux - vx * t, dy = uy - vy * t;
                    return std::sqrt (dx * dx + dy * dy) - radius;
                }

                case Kind::box:
                    break;
            }

            const float r = std::fmin (radius, std::fmin (w, h) * 0.5f);
            const float qx = std::fabs (px - (x + w * 0.5f)) - (w * 0.5f - r);
            const float qy = std::fabs (py - (y + h * 0.5f)) - (h * 0.5f - r);
            const float ox = std::fmax (qx, 0.0f), oy = std::fmax (qy, 0.0f);
            return std::sqrt (ox * ox + oy * oy) + std::fmin (std::fmax (qx, qy), 0.0f) - r;
        }

        /** Which way is out at a point: the distance's slope (a unit vector). */
        void outward (float px, float py, float& gx, float& gy) const noexcept
        {
            gx = distance (px + 0.5f, py) - distance (px - 0.5f, py);
            gy = distance (px, py + 0.5f) - distance (px, py - 0.5f);
            const float len = std::sqrt (gx * gx + gy * gy);

            if (len > 1.0e-5f) { gx /= len; gy /= len; }
            else               { gx = 0.0f; gy = 0.0f; }
        }

        /** Its bounds, grown by margin. */
        void bounds (float margin, float& x0, float& y0, float& x1, float& y1) const noexcept
        {
            if (kind == Kind::line)
            {
                x0 = std::fmin (x, w) - radius - margin; x1 = std::fmax (x, w) + radius + margin;
                y0 = std::fmin (y, h) - radius - margin; y1 = std::fmax (y, h) + radius + margin;
                return;
            }

            x0 = x - margin; y0 = y - margin; x1 = x + w + margin; y1 = y + h + margin;
        }

        Frame frame() const noexcept   { return kind == Kind::line ? Frame { std::fmin (x, w), std::fmin (y, h), std::fabs (w - x) + 1.0f, std::fabs (h - y) + 1.0f }
                                                                   : Frame { x, y, w, h }; }
    };

    //==========================================================================
    /** The picture while it's painted: a colour per pixel, written to the canvas once. */
    struct Surface
    {
        int width = 0, height = 0;
        std::vector<Rgb> pixels;

        void resize (int w, int h)
        {
            width = w < 1 ? 1 : w;
            height = h < 1 ? 1 : h;
            pixels.assign ((std::size_t) width * (std::size_t) height, Rgb {});
        }

        Rgb& at (int x, int y) noexcept   { return pixels[(std::size_t) y * (std::size_t) width + (std::size_t) x]; }

        /** A shape's bounds grown by margin, as this surface's pixels (held inside it before they
            become ints, so a far-off shape can't overflow them); false if they aren't numbers. */
        bool pixelBounds (const Shape& s, float margin, int& ax, int& ay, int& bx, int& by) const noexcept
        {
            float x0, y0, x1, y1;
            s.bounds (margin, x0, y0, x1, y1);

            if (! std::isfinite (x0) || ! std::isfinite (y0) || ! std::isfinite (x1) || ! std::isfinite (y1))
                return false;

            ax = (int) std::fmax (0.0f, std::fmin ((float) width, x0));
            ay = (int) std::fmax (0.0f, std::fmin ((float) height, y0));
            bx = (int) std::fmin ((float) (width - 1), std::fmax (-1.0f, x1));
            by = (int) std::fmin ((float) (height - 1), std::fmax (-1.0f, y1));
            return true;
        }

        /** Calls f (x, y, colour&) for every pixel of a shape's bounds grown by margin. */
        template <typename F>
        void within (const Shape& s, float margin, F&& f)
        {
            int ax, ay, bx, by;

            if (! pixelBounds (s, margin, ax, ay, bx, by))
                return;

            for (int y = ay; y <= by; ++y)
                for (int x = ax; x <= bx; ++x)
                    f (x, y, at (x, y));
        }

        void write (Canvas& g) const
        {
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x)
                    g.drawPixel (x, y, colourOf (pixels[(std::size_t) y * (std::size_t) width + (std::size_t) x]));
        }
    };

    //==========================================================================
    /** The panel: its surface, the light, the view, and the key light falling on it. */
    struct Panel
    {
        Surfacing surfacing;
        Light light = Light::from (120.0f, 35.0f);
        View view;
        float key = 0.6f, keyX = -1.0f, keyY = -1.0f, vignette = 0.45f;
        float width = 1.0f, height = 1.0f;

        void setSize (int w, int h) noexcept
        {
            width = (float) (w < 1 ? 1 : w);
            height = (float) (h < 1 ? 1 : h);
            view = View::over (width, height);
        }

        /** The key light's pool and slope and the vignette at a point: a brightness factor. */
        float toneAt (float x, float y) const noexcept
        {
            const float u = x / width - 0.5f, v = y / height - 0.5f;
            const float towards = u * light.tx + v * light.ty;   // + on the light's side
            const float kx = keyX >= 0.0f ? keyX : 0.5f + light.tx * 0.32f;
            const float ky = keyY >= 0.0f ? keyY : 0.5f + light.ty * 0.40f;
            const float big = width > height ? width : height;
            const float dx = (x / width - kx) * (width / big), dy = (y / height - ky) * (height / big);
            const float pool = std::exp (-(dx * dx + dy * dy) / 0.16f);
            const float vx = u * 1.1f, vy = v * 1.3f;
            return (1.0f + key * (0.30f * towards + 0.22f * pool))
                 * (1.0f - vignette * 0.55f * smooth (std::sqrt (vx * vx + vy * vy) * 1.35f - 0.15f));
        }
    };

    /** A surface's normal at a point: its relief (the heights around it) tilted by a bevel
        (slope along gx, gy: + rising outwards), and a liquid metal's swirl. */
    inline void normalAt (float hLeft, float hRight, float hUp, float hDown, float bump, float bevelSlope, float gx, float gy,
                          float flow, float px, float py, int seed, float& nx, float& ny, float& nz) noexcept
    {
        nx = -(hRight - hLeft) * 0.5f * bump + bevelSlope * gx;
        ny = -(hDown - hUp) * 0.5f * bump + bevelSlope * gy;

        if (flow > 0.0f)
        {
            // Molten: the surface bends in slow swirls, so the room it mirrors flows.
            const float q = fbm (px * 0.0035f, py * 0.0035f, seed + 21, 4);
            const float r = fbm (px * 0.0035f + q * 2.2f, py * 0.0035f - q * 1.7f, seed + 23, 4);
            const float s = fbm (px * 0.0035f - q * 1.9f, py * 0.0035f + q * 2.4f, seed + 27, 4);
            nx += flow * (r - 0.5f) * 1.1f;
            ny += flow * (s - 0.5f) * 1.1f;
        }

        nz = 1.0f;
        const float len = std::sqrt (nx * nx + ny * ny + nz * nz);
        nx /= len; ny /= len; nz /= len;
    }

    /** A bevel's slope at d from a shape's edge (negative inside): rise 1 rising inwards over
        depth pixels (a raised plate's edge), -1 falling (a recess's wall), 0 flat. */
    inline float bevelSlope (float d, float depth, float rise) noexcept
    {
        if (rise == 0.0f || depth <= 0.0f)
            return 0.0f;

        const float s = -d;

        if (s < -1.0f || s > depth)
            return 0.0f;

        const float t = clamp01 (s / depth);
        return rise * 1.6f * 6.0f * t * (1.0f - t) + (s < 0.0f ? rise * 0.4f : 0.0f);
    }

    /** A colour already lit, shaded again by a slope (lines, the rim): brighter facing the
        light, darker away. */
    inline Rgb shadeEdge (Rgb c, float d, float gx, float gy, float depth, float rise, const Light& light,
                          float spec = 0.2f, float tightness = 16.0f) noexcept
    {
        const float slope = bevelSlope (d, depth, rise);

        if (slope == 0.0f)
            return c;

        float nx = slope * gx, ny = slope * gy, nz = 1.0f;
        const float len = std::sqrt (nx * nx + ny * ny + nz * nz);
        nx /= len; ny /= len; nz /= len;

        const float facing = std::fmax (0.0f, nx * light.x + ny * light.y + nz * light.z) / std::fmax (0.05f, light.z);
        const float lit = facing < 0.2f ? 0.2f : (facing > 2.2f ? 2.2f : facing);
        auto out = scale (c, 1.0f + (lit - 1.0f) * 0.85f);

        float hx = light.x, hy = light.y, hz = light.z + 1.0f;
        const float hl = std::sqrt (hx * hx + hy * hy + hz * hz);
        hx /= hl; hy /= hl; hz /= hl;
        const float onSlope = std::pow (std::fmax (0.0f, nx * hx + ny * hy + nz * hz), tightness);
        const float onFlat = std::pow (hz, tightness);
        return add (out, std::fmax (0.0f, onSlope - onFlat) * spec * 1.4f);
    }

    /** How much a raised shape's shadow covers a point outside it (0..1): the shadow falls
        away from the light by distance pixels, softened over softness pixels. */
    inline float dropShadow (const Shape& s, float px, float py, const Light& light, float distance, float softness) noexcept
    {
        const float q = s.distance (px + light.tx * distance, py + light.ty * distance);
        return 1.0f - smoothstep (-softness, softness, q);
    }

    /** How much a recess's rim shades a point inside it (0..1): the rim on the light's side
        casts its shadow in. */
    inline float innerShadow (const Shape& s, float px, float py, const Light& light, float distance, float softness) noexcept
    {
        const float q = s.distance (px + light.tx * distance, py + light.ty * distance);
        return smoothstep (-softness, softness, q);
    }

    /** A surface over a box of pixels, worked out once (its colours and relief), then lit
        point by point: the panel, or a part of it. */
    struct Patch
    {
        int x0 = 0, y0 = 0, w = 0, h = 0;
        std::vector<Texel> texels;

        void fill (const Surfacing& s, int left, int top, int right, int bottom, const Frame& frame)
        {
            x0 = left; y0 = top;
            w = right - left + 1; h = bottom - top + 1;

            if (w < 1 || h < 1) { w = h = 0; texels.clear(); return; }

            texels.resize ((std::size_t) w * (std::size_t) h);

            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    texels[(std::size_t) y * (std::size_t) w + (std::size_t) x] = texelAt (s, (float) (x + x0) + 0.5f, (float) (y + y0) + 0.5f, frame);
        }

        const Texel& at (int x, int y) const noexcept
        {
            x = x < x0 ? x0 : (x >= x0 + w ? x0 + w - 1 : x);
            y = y < y0 ? y0 : (y >= y0 + h ? y0 + h - 1 : y);
            return texels[(std::size_t) (y - y0) * (std::size_t) w + (std::size_t) (x - x0)];
        }

        /** Lit at a pixel, its relief tilted by a bevel slope along gx, gy. */
        Rgb lit (int x, int y, const Optics& o, float slope, float gx, float gy, int seed, const Light& light, const View& view) const noexcept
        {
            float nx, ny, nz;
            normalAt (at (x - 1, y).height, at (x + 1, y).height, at (x, y - 1).height, at (x, y + 1).height, o.bump, slope, gx, gy,
                      o.flow, (float) x, (float) y, seed, nx, ny, nz);
            return lightAt (at (x, y).albedo, o, nx, ny, nz, (float) x + 0.5f, (float) y + 0.5f, light, view);
        }
    };

    //==========================================================================
    /** The panel's own surface, lit, over the whole picture. */
    inline void paintBase (Surface& s, const Panel& panel)
    {
        const auto o = panel.surfacing.optics();
        const Frame frame { 0.0f, 0.0f, (float) s.width, (float) s.height };

        // A few rows at a time, so a big picture needs no big buffer.
        constexpr int band = 64;

        for (int top = 0; top < s.height; top += band)
        {
            const int bottom = top + band - 1 < s.height - 1 ? top + band - 1 : s.height - 1;
            Patch patch;
            patch.fill (panel.surfacing, 0, top > 0 ? top - 1 : 0, s.width - 1, bottom + 1 < s.height ? bottom + 1 : bottom, frame);

            for (int y = top; y <= bottom; ++y)
                for (int x = 0; x < s.width; ++x)
                    s.at (x, y) = scale (patch.lit (x, y, o, 0.0f, 0.0f, 0.0f, panel.surfacing.seed, panel.light, panel.view),
                                         panel.toneAt ((float) x + 0.5f, (float) y + 0.5f));
        }
    }

    //==========================================================================
    // Panel parts

    /** What a part of the panel is:
          recess   sunk into the panel: its rim's shadow falls in, its far wall catches the light
          raise    a raised block: lit edge, a soft shadow beside it
          plate    a raised plate of its own material (brushed aluminium by default), with screws
          screen   a sunk window of dark glass with a reflection (displays, scopes)
          inlay    flush, in its own surface, with a fine engraved outline (a stripe, a badge)
          light    light falling on the panel: a pool of colour from a lamp, an LED, a neon tube
          well     a round recess, for a knob to sit in */
    enum class PartKind { recess, raise, plate, screen, inlay, light, well };

    inline PartKind partKindNamed (const std::string& name) noexcept
    {
        const auto n = lowered (name);
        if (n == "raise" || n == "raised" || n == "block")     return PartKind::raise;
        if (n == "plate" || n == "nameplate")                  return PartKind::plate;
        if (n == "screen" || n == "display" || n == "window")  return PartKind::screen;
        if (n == "inlay" || n == "stripe" || n == "badge")     return PartKind::inlay;
        if (n == "light" || n == "glow" || n == "lamp")        return PartKind::light;
        if (n == "well" || n == "seat")                        return PartKind::well;
        return PartKind::recess;
    }

    struct Part
    {
        PartKind kind = PartKind::recess;
        Shape shape;
        float depth = 4.0f;          // the slope's width at its edge, in pixels
        float strength = 1.0f;       // light: how bright; raise and plate: their shadow; recess and screen: how deep its shadow
        bool ownSurface = false;     // its own material (otherwise the panel's)
        Surfacing surfacing;
        Rgb colour;                  // light: its colour; screen: its glass
        bool hasColour = false;
        bool screws = false;         // plate and raise: a screw in each corner
        Rgb glow;                    // screen: the colour of what it shows, lighting it from inside
        float glowAmount = 0.0f;
    };

    inline void paintScrew (Surface& s, float cx, float cy, float r, float angle, const Light& light);

    /** One part of the panel, painted over what's there. */
    inline void paintPart (Surface& s, const Part& p, const Panel& panel)
    {
        const auto& light = panel.light;
        const float depth = p.depth < 0.5f ? 0.5f : p.depth;
        const bool raised = p.kind == PartKind::raise || p.kind == PartKind::plate;
        const float shadowDistance = depth * 1.1f + 2.0f, shadowSoftness = depth * 1.3f + 3.0f;
        const float margin = p.kind == PartKind::light ? std::fmax (8.0f, std::fmin (p.shape.w, p.shape.h) * 0.5f) * 2.0f
                           : raised ? shadowDistance + shadowSoftness + 2.0f : 2.0f;

        // Its own surface (or the panel's, for a raised block of it), worked out over its box.
        const bool surfaced = raised || p.kind == PartKind::inlay || p.ownSurface;
        const auto& surfacing = p.ownSurface ? p.surfacing : panel.surfacing;
        const auto optics = surfacing.optics();
        Patch patch;

        if (surfaced)
        {
            int left = 0, top = 0, right = -1, bottom = -1;

            if (s.pixelBounds (p.shape, 2.0f, left, top, right, bottom))
                patch.fill (surfacing, left, top, right, bottom, p.ownSurface ? p.shape.frame() : Frame { 0.0f, 0.0f, (float) s.width, (float) s.height });
        }

        s.within (p.shape, margin, [&] (int ix, int iy, Rgb& c)
        {
            const float px = (float) ix + 0.5f, py = (float) iy + 0.5f;
            const float d = p.shape.distance (px, py);
            const float inside = clamp01 (0.5f - d);   // the edge, smoothed
            float gx = 0.0f, gy = 0.0f;

            if (d < depth + 1.0f && d > -depth - 1.0f)
                p.shape.outward (px, py, gx, gy);

            switch (p.kind)
            {
                case PartKind::light:
                {
                    const float soft = margin * 0.5f;
                    const float amount = p.strength * (1.0f - smoothstep (-soft, soft, d));
                    c = screen (c, p.hasColour ? p.colour : Rgb { 1.0f, 0.95f, 0.85f }, amount * 0.6f);
                    return;
                }

                case PartKind::inlay:
                {
                    if (inside > 0.0f && patch.w > 0)
                        c = mix (c, scale (patch.lit (ix, iy, optics, 0.0f, 0.0f, 0.0f, surfacing.seed, light, panel.view), panel.toneAt (px, py)), inside);

                    c = scale (c, 1.0f - 0.45f * clamp01 (1.0f - std::fabs (d) / 0.9f));   // the engraved line round it
                    return;
                }

                case PartKind::raise:
                case PartKind::plate:
                {
                    // Its shadow on the panel, and the panel darkening where they meet.
                    if (inside < 1.0f)
                    {
                        const float shade = dropShadow (p.shape, px, py, light, shadowDistance, shadowSoftness) * 0.5f * p.strength
                                          + std::exp (-std::fmax (d, 0.0f) / 1.5f) * 0.18f;
                        c = scale (c, 1.0f - clamp01 (shade) * (1.0f - inside));
                    }

                    if (inside <= 0.0f || patch.w == 0)
                        return;

                    const auto top = scale (patch.lit (ix, iy, optics, bevelSlope (d, depth, 1.0f), gx, gy, surfacing.seed, light, panel.view),
                                            panel.toneAt (px, py) * 1.02f);
                    c = mix (c, top, inside);
                    return;
                }

                case PartKind::recess:
                case PartKind::well:
                case PartKind::screen:
                {
                    if (inside <= 0.0f)
                        return;

                    const bool glass = p.kind == PartKind::screen;
                    Rgb floor;

                    if (glass)
                    {
                        const auto base = p.hasColour ? p.colour : Rgb { 0.04f, 0.07f, 0.09f };
                        const float gy01 = (py - p.shape.y) / std::fmax (1.0f, p.shape.h);
                        floor = scale (base, (0.65f + 0.35f * (1.0f - gy01)) * (((int) py % 3 == 0) ? 0.94f : 1.0f));

                        if (p.glowAmount > 0.0f)   // what it shows lights it from inside
                        {
                            const float cx = (px - p.shape.x) / std::fmax (1.0f, p.shape.w) - 0.5f, cy = gy01 - 0.5f;
                            floor = screen (floor, p.glow, p.glowAmount * 0.35f * std::exp (-(cx * cx + cy * cy) * 4.0f));
                        }

                        // The reflection: a soft band across the top, slanted with the light.
                        const float along = (px - p.shape.x) / std::fmax (1.0f, p.shape.w);
                        const float band = gy01 + (along - 0.5f) * 0.25f * light.tx;
                        floor = add (floor, 0.07f * smoothstep (0.42f, 0.08f, band));
                        floor = shadeEdge (floor, d, gx, gy, depth, -1.0f, light, 0.5f, 30.0f);
                    }
                    else if (p.ownSurface && patch.w > 0)
                    {
                        floor = scale (patch.lit (ix, iy, optics, bevelSlope (d, depth, -1.0f), gx, gy, surfacing.seed, light, panel.view),
                                       panel.toneAt (px, py) * 0.92f);
                    }
                    else
                    {
                        floor = shadeEdge (scale (c, 0.86f), d, gx, gy, depth, -1.0f, light, optics.spec, 16.0f);
                    }

                    const float ish = innerShadow (p.shape, px, py, light, depth * (glass ? 1.4f : 1.1f) + 1.5f, depth + 2.0f);
                    floor = scale (floor, 1.0f - ish * (glass ? 0.65f : 0.5f) * p.strength);
                    c = mix (c, floor, inside);
                    return;
                }
            }
        });

        // A plate's (or a raised block's) screws, one in each corner.
        if (p.screws && raised && p.shape.kind == Shape::Kind::box)
        {
            const float r = std::fmax (2.5f, std::fmin (6.0f, std::fmin (p.shape.w, p.shape.h) * 0.09f));
            const float inset = std::fmax (r * 2.2f, std::fmin (p.shape.radius, p.shape.w * 0.25f));

            paintScrew (s, p.shape.x + inset, p.shape.y + inset, r, 0.6f, light);
            paintScrew (s, p.shape.x + p.shape.w - inset, p.shape.y + inset, r, 2.1f, light);
            paintScrew (s, p.shape.x + inset, p.shape.y + p.shape.h - inset, r, 1.2f, light);
            paintScrew (s, p.shape.x + p.shape.w - inset, p.shape.y + p.shape.h - inset, r, 2.7f, light);
        }
    }

    /** A line cut into the panel (a groove: a divider, an engraved border) or standing out of
        it (a ridge). */
    inline void paintLine (Surface& s, float x1, float y1, float x2, float y2, float width, bool ridge, const Light& light)
    {
        const auto line = Shape::line (x1, y1, x2, y2, std::fmax (0.5f, width * 0.5f));
        const float depth = std::fmax (0.8f, width * 0.5f);

        s.within (line, ridge ? depth + 3.0f : 1.5f, [&] (int ix, int iy, Rgb& c)
        {
            const float px = (float) ix + 0.5f, py = (float) iy + 0.5f;
            const float d = line.distance (px, py);
            const float inside = clamp01 (0.5f - d);
            float gx, gy;
            line.outward (px, py, gx, gy);

            if (ridge && inside < 1.0f)
                c = scale (c, 1.0f - dropShadow (line, px, py, light, depth, depth + 1.0f) * 0.35f * (1.0f - inside));

            if (inside > 0.0f)
                c = mix (c, shadeEdge (ridge ? c : scale (c, 0.8f), d, gx, gy, depth, ridge ? 1.0f : -1.0f, light), inside);
        });
    }

    /** Slots cut through the panel (vents, a speaker grille): count of them across a box,
        lengthwise along it when across is false. */
    inline void paintSlots (Surface& s, float x, float y, float w, float h, int count, bool across, const Panel& panel)
    {
        count = count < 1 ? 1 : (count > 64 ? 64 : count);
        const float step = (across ? w : h) / (float) count;
        const float half = std::fmax (1.0f, step * 0.28f);

        for (int i = 0; i < count; ++i)
        {
            const float at = (across ? x : y) + step * ((float) i + 0.5f);
            Part p;
            p.kind = PartKind::recess;
            p.shape = across ? Shape::line (at, y + half, at, y + h - half, half) : Shape::line (x + half, at, x + w - half, at, half);
            p.depth = half;
            p.ownSurface = true;
            p.surfacing = Surfacing::of (Material::smooth);
            p.surfacing.colour = { 0.02f, 0.02f, 0.025f };
            paintPart (s, p, panel);
        }
    }

    /** Holes in a grid (a perforated grille): spacing between centres, size their diameter. */
    inline void paintHoles (Surface& s, float x, float y, float w, float h, float spacing, float size, const Light& light)
    {
        spacing = std::fmax (3.0f, spacing);
        const float r = std::fmax (0.6f, std::fmin (size, spacing * 0.9f) * 0.5f);
        const auto area = Shape::box (x, y, w, h, 0.0f);

        s.within (area, 0.0f, [&] (int ix, int iy, Rgb& c)
        {
            const float px = (float) ix + 0.5f, py = (float) iy + 0.5f;
            const float row = std::floor ((py - y) / spacing);
            const float offset = ((int) row & 1) ? spacing * 0.5f : 0.0f;   // staggered rows
            const float cx = x + offset + spacing * (std::floor ((px - x - offset) / spacing) + 0.5f), cy = y + spacing * (row + 0.5f);
            const auto hole = Shape::circle (cx, cy, r);
            const float d = hole.distance (px, py);
            const float inside = clamp01 (0.5f - d);

            if (inside <= 0.0f)
                return;

            const float shade = innerShadow (hole, px, py, light, r * 0.8f, r * 0.6f);
            c = mix (c, scale (Rgb { 0.03f, 0.03f, 0.035f }, 1.0f - shade * 0.5f), inside);
        });
    }

    /** A screw head: a dome lit by the light, its slot at an angle, a little shadow. */
    inline void paintScrew (Surface& s, float cx, float cy, float r, float angle, const Light& light)
    {
        const auto head = Shape::circle (cx, cy, r);
        const float ca = std::cos (angle), sa = std::sin (angle);

        s.within (head, r * 0.6f + 2.0f, [&] (int ix, int iy, Rgb& c)
        {
            const float px = (float) ix + 0.5f, py = (float) iy + 0.5f;
            const float d = head.distance (px, py);
            const float inside = clamp01 (0.5f - d);

            if (inside < 1.0f)
                c = scale (c, 1.0f - dropShadow (head, px, py, light, r * 0.25f + 0.8f, r * 0.35f + 0.8f) * 0.45f * (1.0f - inside));

            if (inside <= 0.0f)
                return;

            // The dome: its slope from the centre out, lit.
            const float dx = (px - cx) / r, dy = (py - cy) / r;
            const float rr = std::fmin (1.0f, dx * dx + dy * dy);
            float nx = dx * 0.75f, ny = dy * 0.75f, nz = std::sqrt (std::fmax (0.05f, 1.0f - rr * 0.5625f));
            const float len = std::sqrt (nx * nx + ny * ny + nz * nz);
            nx /= len; ny /= len; nz /= len;
            const float facing = std::fmax (0.0f, nx * light.x + ny * light.y + nz * light.z);
            Rgb metal = scale (Rgb { 0.62f, 0.64f, 0.67f }, 0.45f + 0.75f * facing);
            metal = add (metal, std::pow (facing, 24.0f) * 0.5f);

            // The slot across it, sunk: its wall on the light's side in shadow.
            const float u = dx * ca + dy * sa, v = -dx * sa + dy * ca;

            if (std::fabs (v) < 0.16f && std::fabs (u) < 0.8f)
                metal = scale (metal, (v * (-light.tx * sa + light.ty * ca) > 0.0f) ? 0.55f : 0.32f);

            c = mix (c, metal, inside);
        });
    }

    /** The panel's own rim: its outer edge rising over width pixels (a chassis's bevel). */
    inline void paintRim (Surface& s, float width, float radius, const Light& light)
    {
        if (width <= 0.0f)
            return;

        const auto panel = Shape::box (0.0f, 0.0f, (float) s.width, (float) s.height, radius);

        for (int iy = 0; iy < s.height; ++iy)
        {
            for (int ix = 0; ix < s.width; ++ix)
            {
                const float px = (float) ix + 0.5f, py = (float) iy + 0.5f;

                if (px > width + 2.0f && py > width + 2.0f && px < (float) s.width - width - 2.0f && py < (float) s.height - width - 2.0f)
                    continue;

                const float d = panel.distance (px, py);
                float gx, gy;
                panel.outward (px, py, gx, gy);
                auto& c = s.at (ix, iy);
                c = shadeEdge (c, d, gx, gy, width, 1.0f, light, 0.3f, 20.0f);
                c = scale (c, 1.0f - 0.35f * clamp01 (1.0f - std::fabs (d + 0.5f)));   // its very edge
            }
        }
    }

    //==========================================================================
    /** Settings arrive as text: numbers, words, and lists or objects as JSON (paint_picture
        passes them that way). A small reader for them. */
    struct Json
    {
        enum class Type { none, number, text, list, object, boolean };

        Type type = Type::none;
        double number = 0.0;
        std::string text;
        std::vector<Json> items;
        std::vector<std::pair<std::string, Json>> fields;

        static Json parse (const std::string& source)
        {
            std::size_t at = 0;
            return value (source, at, 0);
        }

        const Json* find (const std::string& key) const noexcept
        {
            for (const auto& [name, v] : fields)
                if (name == key)
                    return &v;

            return nullptr;
        }

        bool has (const std::string& key) const noexcept    { return find (key) != nullptr; }

        float num (const std::string& key, float fallback) const noexcept
        {
            const auto* v = find (key);

            if (v == nullptr) return fallback;
            if (v->type == Type::number || v->type == Type::boolean) return (float) v->number;
            if (v->type == Type::text) { const char* end = nullptr; const double d = numberIn (v->text.c_str(), &end); return end != v->text.c_str() ? (float) d : fallback; }
            return fallback;
        }

        std::string str (const std::string& key, const std::string& fallback = {}) const
        {
            const auto* v = find (key);
            return v != nullptr && v->type == Type::text ? v->text : fallback;
        }

        bool flag (const std::string& key) const noexcept
        {
            const auto* v = find (key);

            if (v == nullptr) return false;
            if (v->type == Type::text) return v->text == "true" || v->text == "yes" || v->text == "1";
            return v->number != 0.0;
        }

    private:
        static void space (const std::string& s, std::size_t& at) noexcept
        {
            while (at < s.size() && (s[at] == ' ' || s[at] == '\n' || s[at] == '\r' || s[at] == '\t'))
                ++at;
        }

        static Json value (const std::string& s, std::size_t& at, int level)
        {
            Json j;
            space (s, at);

            if (at >= s.size() || level > 16)
                return j;

            const char c = s[at];

            if (c == '{' || c == '[')
            {
                const bool object = c == '{';
                j.type = object ? Type::object : Type::list;
                ++at;

                for (;;)
                {
                    space (s, at);

                    if (at >= s.size()) break;
                    if (s[at] == (object ? '}' : ']')) { ++at; break; }
                    if (s[at] == ',') { ++at; continue; }

                    const auto before = at;

                    if (object)
                    {
                        auto key = value (s, at, level + 1);
                        space (s, at);

                        if (at < s.size() && s[at] == ':')
                            ++at;

                        j.fields.push_back ({ key.text, value (s, at, level + 1) });
                    }
                    else
                    {
                        j.items.push_back (value (s, at, level + 1));
                    }

                    if (at == before)
                        ++at;   // never stuck on something it can't read
                }

                return j;
            }

            if (c == '"')
            {
                j.type = Type::text;
                ++at;

                while (at < s.size() && s[at] != '"')
                {
                    if (s[at] == '\\' && at + 1 < s.size())
                        ++at;

                    j.text += s[at++];
                }

                ++at;
                return j;
            }

            if (s.compare (at, 4, "true") == 0)  { j.type = Type::boolean; j.number = 1.0; at += 4; return j; }
            if (s.compare (at, 5, "false") == 0) { j.type = Type::boolean; j.number = 0.0; at += 5; return j; }
            if (s.compare (at, 4, "null") == 0)  { at += 4; return j; }

            const char* end = nullptr;
            j.number = numberIn (s.c_str() + at, &end);

            if (end != s.c_str() + at)
            {
                j.type = Type::number;
                at = (std::size_t) (end - s.c_str());
                return j;
            }

            // A bare word: taken as text up to the next separator.
            j.type = Type::text;

            while (at < s.size() && s[at] != ',' && s[at] != '}' && s[at] != ']' && s[at] != ':')
                j.text += s[at++];

            return j;
        }
    };

    //==========================================================================
    /** A pattern layer from its settings: { "pattern", "color", "opacity", "blend", "size",
        "angle", "width", "depth", "x", "y", "count", "invert", "seed" }. */
    inline PatternLayer patternFrom (const Json& j)
    {
        PatternLayer p;
        p.pattern = patternNamed (j.str ("pattern", j.str ("name")));
        p.blend = blendNamed (j.str ("blend", j.str ("mode")), Blend::normal);

        if (const auto c = j.str ("color", j.str ("colour")); ! c.empty())
        {
            p.colour = colourNamed (c, p.colour);
            p.hasColour = true;
        }

        p.opacity = clamp01 (j.num ("opacity", 0.5f));
        p.size = j.num ("size", j.num ("scale", 24.0f));
        p.angle = j.num ("angle", p.pattern == Pattern::knurl ? 45.0f : 0.0f);
        p.width = j.num ("width", -1.0f);
        p.depth = j.num ("depth", 1.5f);
        p.centreX = j.num ("x", 0.5f);
        p.centreY = j.num ("y", 0.5f);
        p.count = (int) j.num ("count", 48.0f);
        p.invert = j.flag ("invert");
        p.seed = (int) j.num ("seed", 1.0f);
        return p;
    }

    /** A surface from its settings: { "material", "color", "color2", "finish", "scale",
        "angle", "grain", "shine", "flow", "seed", "pattern" (a name, an object, or a list of
        layers: "patterns") } (a missing one: the material's own, or fallback's). */
    inline Surfacing surfacingFrom (const Json& j, const Surfacing& fallback, bool fromFallbackMaterial = false)
    {
        auto s = fallback;

        if (j.has ("material") && ! fromFallbackMaterial)
        {
            const auto name = j.str ("material");
            s = Surfacing::of (materialNamed (name), fallback.seed);
            s.patterns.clear();

            if (Rgb wood, grain; woodColours (name, wood, grain))
            {
                s.colour = wood;
                s.colour2 = grain;
            }
        }

        if (const auto c = j.str ("color", j.str ("colour")); ! c.empty())  s.colour = colourNamed (c, s.colour);
        if (const auto c = j.str ("color2", j.str ("colour2")); ! c.empty()) s.colour2 = colourNamed (c, s.colour2);
        if (j.has ("finish"))   s.finish = finishNamed (j.str ("finish"), s.finish);
        if (j.has ("scale"))    s.scale = j.num ("scale", 1.0f);
        if (j.has ("angle"))    s.angle = j.num ("angle", 0.0f);
        if (j.has ("grain"))    s.grain = j.num ("grain", 1.0f);
        if (j.has ("shine"))    s.shine = j.num ("shine", -1.0f);
        if (j.has ("flow"))     s.flow = j.num ("flow", -1.0f);
        if (j.has ("seed"))     s.seed = (int) j.num ("seed", 1.0f);

        for (const auto* key : { "pattern", "patterns" })
        {
            const auto* v = j.find (key);

            if (v == nullptr)
                continue;

            if (v->type == Json::Type::text && patternNamed (v->text) != Pattern::none)
            {
                // A pattern by name, with the layer's settings beside it ("patternColor"...).
                Json layer;
                layer.type = Json::Type::object;
                layer.fields.push_back ({ "pattern", *v });

                for (const auto* field : { "Color", "Colour", "Opacity", "Blend", "Size", "Scale", "Angle", "Width", "Depth" })
                    if (const auto* f = j.find (std::string ("pattern") + field))
                    {
                        std::string name (field);
                        name[0] = (char) (name[0] - 'A' + 'a');
                        layer.fields.push_back ({ name, *f });
                    }

                s.patterns.push_back (patternFrom (layer));
            }
            else if (v->type == Json::Type::object)
            {
                s.patterns.push_back (patternFrom (*v));
            }
            else if (v->type == Json::Type::list)
            {
                for (const auto& item : v->items)
                    if (item.type == Json::Type::object && s.patterns.size() < 8)
                        s.patterns.push_back (patternFrom (item));
            }
        }

        return s;
    }
}
