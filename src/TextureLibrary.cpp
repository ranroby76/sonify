// D:\workspace\Fanan_web_hub\src\TextureLibrary.cpp
// From KnobMaker (C:\workspace\knobmaker), as Sonify has it; KnobMaster 2 adds four more patterns
// made in code: Water, Veins, Spun and Stones.

/*
    TextureLibrary.cpp
*/

#include "TextureLibrary.h"
#include "BmpImage.h"
#include "Settings.h"

#include <cmath>

namespace
{
    constexpr float kPi = juce::MathConstants<float>::pi;

    //==========================================================================
    /** Deterministic per-lattice-point value. Not juce::Random: the same cell
        has to give the same number every time or the pattern would crawl as the
        preview re-renders at a different scale. */
    inline float hashCell (int x, int y) noexcept
    {
        auto h = (juce::uint32) (x * 374761393) + (juce::uint32) (y * 668265263);
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= (h >> 16);

        return (float) (h & 0xffffffu) / (float) 0xffffffu;
    }

    inline float smoothStep (float t) noexcept { return t * t * (3.0f - 2.0f * t); }

    /** Value noise that wraps at `period` cells, so the pattern tiles cleanly
        no matter where the rotation lands it. */
    float valueNoise (float u, float v, int period) noexcept
    {
        period = juce::jmax (1, period);

        const float x = u * (float) period;
        const float y = v * (float) period;

        const int x0 = (int) std::floor (x);
        const int y0 = (int) std::floor (y);

        const float fx = smoothStep (x - (float) x0);
        const float fy = smoothStep (y - (float) y0);

        auto wrap = [period] (int i) noexcept { return ((i % period) + period) % period; };

        const float a = hashCell (wrap (x0),     wrap (y0));
        const float b = hashCell (wrap (x0 + 1), wrap (y0));
        const float c = hashCell (wrap (x0),     wrap (y0 + 1));
        const float d = hashCell (wrap (x0 + 1), wrap (y0 + 1));

        return juce::jmap (fy, juce::jmap (fx, a, b), juce::jmap (fx, c, d));
    }

    //==========================================================================
    float patternCheckers (float u, float v, int n) noexcept
    {
        const int cx = (int) std::floor (u * (float) n);
        const int cy = (int) std::floor (v * (float) n);

        return ((cx + cy) & 1) ? 0.18f : 0.82f;
    }

    /** Streaks that run along one axis and vary across it — brushed metal.
        The x lattice coordinate is pinned so every row of the streak is
        identical; only the offsets change between octaves. */
    float patternHairline (float u, float v, int n) noexcept
    {
        juce::ignoreUnused (u);

        float value = 0.58f * valueNoise (0.00f, v, n * 8)
                    + 0.30f * valueNoise (0.37f, v, n * 23)
                    + 0.12f * valueNoise (0.71f, v, n * 61);

        return juce::jlimit (0.0f, 1.0f, value);
    }

    float patternSand (float u, float v, int n) noexcept
    {
        float value = 0.55f * valueNoise (u, v, n * 6)
                    + 0.30f * valueNoise (u, v, n * 13)
                    + 0.15f * valueNoise (u, v, n * 29);

        return juce::jlimit (0.0f, 1.0f, value);
    }

    /** A weave: alternate cells show the warp thread, the rest the weft, each
        shaded across its own width so the crossing reads as over-and-under. */
    float patternFabric (float u, float v, int n) noexcept
    {
        const float x = u * (float) n;
        const float y = v * (float) n;

        const int cx = (int) std::floor (x);
        const int cy = (int) std::floor (y);

        const float across = ((cx + cy) & 1) ? (y - (float) cy) : (x - (float) cx);

        return juce::jlimit (0.0f, 1.0f, 0.22f + 0.68f * std::sin (across * kPi));
    }


    //==========================================================================
    // KnobMaster 2's four: water caustics in a swirl, marble veins, spun metal, and stones.

    inline float quintic (float t) noexcept { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

    /** Gradient noise (smooth, with no grid showing) that wraps at `period` cells, 0..1. */
    float gradientNoise (float u, float v, int period) noexcept
    {
        period = juce::jmax (1, period);

        const float x = u * (float) period;
        const float y = v * (float) period;

        const int x0 = (int) std::floor (x);
        const int y0 = (int) std::floor (y);
        const float fx = x - (float) x0;
        const float fy = y - (float) y0;

        auto wrap = [period] (int i) noexcept { return ((i % period) + period) % period; };

        auto corner = [&] (int ix, int iy, float dx, float dy) noexcept
        {
            const float a = hashCell (wrap (ix), wrap (iy)) * 2.0f * kPi;
            return std::cos (a) * dx + std::sin (a) * dy;
        };

        const float n00 = corner (x0,     y0,     fx,        fy);
        const float n10 = corner (x0 + 1, y0,     fx - 1.0f, fy);
        const float n01 = corner (x0,     y0 + 1, fx,        fy - 1.0f);
        const float n11 = corner (x0 + 1, y0 + 1, fx - 1.0f, fy - 1.0f);

        const float sx = quintic (fx);
        const float sy = quintic (fy);
        const float n = juce::jmap (sy, juce::jmap (sx, n00, n10), juce::jmap (sx, n01, n11));

        return juce::jlimit (0.0f, 1.0f, 0.5f + n * 0.7071f);
    }

    float fractalNoise (float u, float v, int period, int octaves) noexcept
    {
        float total = 0.0f, amplitude = 0.5f, norm = 0.0f;

        for (int o = 0; o < octaves; ++o)
        {
            total += amplitude * gradientNoise (u + 0.17f * (float) o, v + 0.31f * (float) o, period);
            norm += amplitude;
            amplitude *= 0.5f;
            period *= 2;
        }

        return total / norm;
    }

    /** Thin bright lines on near black, rippling in a zig-zag and turning in a swirl round the
        middle: light through moving water. */
    float patternWater (float u, float v, int n) noexcept
    {
        n *= 2;   // finer than the others at the same scale: KnobMan's water has many lines

        const float cx = u - 0.5f, cy = v - 0.5f;
        const float r = std::sqrt (cx * cx + cy * cy);
        const float a = std::atan2 (cy, cx) + 2.6f * std::exp (-r * 3.0f);   // the swirl, tightest in the middle

        float sx = 0.5f + r * std::cos (a);
        const float sy = 0.5f + r * std::sin (a);
        sx += 0.03f * std::sin (sy * 2.0f * kPi * 7.0f * (float) n);          // the zig-zag

        const float wu = sx + 0.10f * fractalNoise (sx, sy, 3 * n, 2);
        const float wv = sy + 0.10f * fractalNoise (sx + 0.5f, sy + 0.5f, 3 * n, 2);
        const float f = fractalNoise (wu, wv, 2 * n, 3);
        const float c = std::abs (std::sin (f * 2.0f * kPi * 4.0f));

        const float lines = std::pow (1.0f - c, 7.0f);
        const float glow = std::pow (1.0f - c, 3.0f);

        return juce::jlimit (0.0f, 1.0f, 0.03f + 0.62f * lines + 0.10f * glow);
    }

    /** Glowing veins in a dark grey, as in marble or smoke: ridged noise, bent a little. */
    float patternVeins (float u, float v, int n) noexcept
    {
        const float wu = u + 0.06f * gradientNoise (u, v, 3 * n);
        const float wv = v + 0.06f * gradientNoise (u + 0.5f, v + 0.5f, 3 * n);

        float total = 0.0f, amplitude = 0.6f, norm = 0.0f;
        int period = 4 * n;

        for (int o = 0; o < 4; ++o)
        {
            const float noise = gradientNoise (wu + 0.13f * (float) o, wv + 0.29f * (float) o, period);
            const float ridge = 1.0f - std::abs (noise * 2.0f - 1.0f);
            total += amplitude * std::pow (ridge, 6.0f);
            norm += amplitude;
            amplitude *= 0.5f;
            period *= 2;
        }

        return juce::jlimit (0.0f, 1.0f, 0.16f + 0.85f * std::pow (total / norm, 1.1f));
    }

    /** Spun metal: fine rings round the middle, and the light catching them along two
        crossing lines (four bright rays), dark between them. */
    float patternSpun (float u, float v, int n) noexcept
    {
        const float cx = u - 0.5f, cy = v - 0.5f;
        const float r = std::sqrt (cx * cx + cy * cy);
        const float a = std::atan2 (cy, cx);

        const float rays = 0.5f + 0.5f * std::cos (4.0f * (a - 0.35f));
        const float sheen = 0.06f + 0.94f * std::pow (rays, 2.2f);

        const float rings = 0.55f * valueNoise (0.00f, r * 2.0f, 220 * n)
                          + 0.30f * valueNoise (0.37f, r * 2.0f, 610 * n)
                          + 0.15f * valueNoise (0.71f, r * 2.0f, 1500);

        return juce::jlimit (0.0f, 1.0f, sheen * (0.72f + 0.5f * (rings - 0.5f)));
    }

    /** Stones (cobbles, crazy paving): cells in light greys, each its own shade, with dark gaps
        between them and a lit rim on the side that faces the light. Wraps at its edges. */
    float patternStones (float u, float v, int n) noexcept
    {
        const int cells = juce::jmax (1, n) * 7;
        const float x = u * (float) cells;
        const float y = v * (float) cells;
        const int xi = (int) std::floor (x);
        const int yi = (int) std::floor (y);

        auto wrap = [cells] (int i) noexcept { return ((i % cells) + cells) % cells; };
        auto hash = [] (int cx, int cy, int salt) noexcept { return hashCell (cx * 7 + salt * 131, cy * 13 + salt * 97); };

        float f1 = 10.0f, f2 = 10.0f, shade = 0.0f, gx = 0.0f, gy = 0.0f;

        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                const int cx = xi + dx, cy = yi + dy;
                const int wx = wrap (cx), wy = wrap (cy);
                const float px = (float) cx + 0.15f + 0.7f * hash (wx, wy, 1);
                const float py = (float) cy + 0.15f + 0.7f * hash (wx, wy, 2);
                const float d = std::sqrt ((x - px) * (x - px) + (y - py) * (y - py));

                if (d < f1)
                {
                    f2 = f1;
                    f1 = d;
                    shade = hash (wx, wy, 3);
                    gx = x - px;
                    gy = y - py;
                }
                else if (d < f2)
                {
                    f2 = d;
                }
            }
        }

        const float edge = f2 - f1;                                            // 0 in the gap
        const float gap = juce::jlimit (0.0f, 1.0f, (edge - 0.05f) / 0.06f);
        const float rim = juce::jlimit (0.0f, 1.0f, 1.0f - std::abs (edge - 0.14f) / 0.08f);
        const float facing = (-gx - gy) / (std::sqrt (gx * gx + gy * gy) + 1.0e-6f);   // lit from the upper left

        const float value = (0.62f + 0.22f * shade) * gap + rim * 0.25f * (0.6f + 0.4f * facing) + (1.0f - gap) * 0.22f
                          + 0.06f * (valueNoise (u, v, cells * 8) - 0.5f);

        return juce::jlimit (0.0f, 1.0f, value);
    }

    //==========================================================================
    /** Scale is "percent of the canvas one repeat covers", so a smaller number
        means a finer pattern. Turned into a whole repeat count because the
        tileable noise needs an integer period. */
    int repeatsFor (float scalePercent) noexcept
    {
        const float s = juce::jlimit (2.0f, 200.0f, scalePercent);

        return juce::jlimit (1, 64, (int) std::lround (100.0f / s));
    }

    juce::Image generatePattern (const juce::String& name, int w, int h,
                                 float scalePercent, float angleDegrees)
    {
        juce::Image out (juce::Image::ARGB, w, h, false);
        juce::Image::BitmapData data (out, juce::Image::BitmapData::writeOnly);

        const int   n  = repeatsFor (scalePercent);
        const float a  = angleDegrees * kPi / 180.0f;
        const float ca = std::cos (a);
        const float sa = std::sin (a);

        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                // Rotate about the middle, then let the pattern wrap.
                const float u = (float) x / (float) w - 0.5f;
                const float v = (float) y / (float) h - 0.5f;

                const float ru = u * ca - v * sa + 0.5f;
                const float rv = u * sa + v * ca + 0.5f;

                float value = 0.5f;

                if      (name == "Checkers") value = patternCheckers (ru, rv, n);
                else if (name == "Fabric")   value = patternFabric   (ru, rv, n);
                else if (name == "Hairline") value = patternHairline (ru, rv, n);
                else if (name == "Sand")     value = patternSand     (ru, rv, n);
                else if (name == "Water")    value = patternWater    (ru, rv, n);
                else if (name == "Veins")    value = patternVeins    (ru, rv, n);
                else if (name == "Spun")     value = patternSpun     (ru, rv, n);
                else if (name == "Stones")   value = patternStones   (ru, rv, n);

                const auto g = (juce::uint8) juce::jlimit (0, 255,
                                                           (int) std::lround (value * 255.0f));

                auto* px = reinterpret_cast<juce::PixelARGB*> (data.getLinePointer (y)
                                                                 + x * data.pixelStride);
                px->setARGB (255, g, g, g);
            }
        }

        return out;
    }

    //==========================================================================
    juce::File findUserFile (const juce::String& name)
    {
        auto folder = TextureLibrary::userFolder();

        if (! folder.isDirectory())
            return {};

        auto direct = folder.getChildFile (name);

        if (direct.existsAsFile())
            return direct;

        // A document may have stored the name without its extension — which is
        // exactly what an imported .knob does.
        juce::Array<juce::File> files;
        folder.findChildFiles (files, juce::File::findFiles, false,
                               "*.png;*.jpg;*.jpeg;*.bmp;*.gif");

        for (const auto& f : files)
            if (f.getFileNameWithoutExtension().equalsIgnoreCase (name))
                return f;

        return {};
    }

    juce::Image tileUserImage (const juce::String& name, int w, int h,
                               float scalePercent, float angleDegrees)
    {
        auto file = findUserFile (name);

        if (file == juce::File())
            return {};

        // Not ImageFileFormat::loadFrom: JUCE has no BMP decoder, and KnobMan's
        // own stock textures are bitmaps.
        auto source = loadImageFile (file);

        if (! source.isValid() || source.getWidth() < 1 || source.getHeight() < 1)
            return {};

        juce::Image out (juce::Image::ARGB, w, h, true);
        juce::Graphics g (out);

        // Neutral grey wherever the tile is transparent: no bump, no tint.
        g.fillAll (juce::Colour (0xff808080));

        const float tile = juce::jmax (4.0f, (float) juce::jmin (w, h)
                                               * juce::jlimit (2.0f, 200.0f, scalePercent) * 0.01f);

        auto transform = juce::AffineTransform::scale (tile / (float) source.getWidth(),
                                                       tile / (float) source.getHeight())
                            .rotated (angleDegrees * kPi / 180.0f,
                                      (float) w * 0.5f, (float) h * 0.5f);

        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.setFillType (juce::FillType (source, transform));
        g.fillAll();

        return out;
    }

    //==========================================================================
    juce::CriticalSection cacheLock;
    juce::HashMap<juce::String, juce::Image> cache;

    //==========================================================================
    juce::CriticalSection folderLock;
    juce::String chosenFolder;        // empty means "use the default"
    bool         folderLoaded = false;

    // The Plugins Creator Studio's own places, beside its primitives and looks.
    juce::File appDataRoot()
    {
        return Settings::getCacheFolder();
    }
}

//==============================================================================
juce::File TextureLibrary::defaultFolder()
{
    return Settings::getDataFolder().getChildFile ("Textures");
}

juce::File TextureLibrary::settingsFile()
{
    return appDataRoot().getChildFile ("textures.xml");
}

juce::File TextureLibrary::userFolder()
{
    const juce::ScopedLock sl (folderLock);

    if (! folderLoaded)
    {
        folderLoaded = true;

        if (auto xml = juce::XmlDocument::parse (settingsFile()))
            chosenFolder = xml->getStringAttribute ("textureFolder");
    }

    if (chosenFolder.isNotEmpty())
    {
        const juce::File chosen (chosenFolder);

        // If the folder has gone — renamed, or on a drive that is not plugged
        // in — fall back rather than showing an empty list with no explanation.
        if (chosen.isDirectory())
            return chosen;
    }

    return defaultFolder();
}

bool TextureLibrary::isUsingCustomFolder()
{
    return userFolder() != defaultFolder();
}

void TextureLibrary::setUserFolder (const juce::File& folder)
{
    {
        const juce::ScopedLock sl (folderLock);

        chosenFolder = folder.isDirectory() ? folder.getFullPathName() : juce::String();
        folderLoaded = true;

        auto file = settingsFile();
        file.getParentDirectory().createDirectory();

        // Merge into whatever is already in there — this file is shared with
        // anything else that wants to remember something later.
        auto xml = juce::XmlDocument::parse (file);

        if (xml == nullptr)
            xml = std::make_unique<juce::XmlElement> ("KnobMakerSettings");

        if (chosenFolder.isEmpty())
            xml->removeAttribute ("textureFolder");
        else
            xml->setAttribute ("textureFolder", chosenFolder);

        xml->writeTo (file);
    }

    rescan();
}

juce::StringArray TextureLibrary::proceduralNames()
{
    return { "Checkers", "Fabric", "Hairline", "Sand", "Water", "Veins", "Spun", "Stones" };
}

bool TextureLibrary::isProcedural (const juce::String& name)
{
    return proceduralNames().contains (name);
}

juce::StringArray TextureLibrary::availableNames()
{
    auto names = proceduralNames();

    auto folder = userFolder();

    if (folder.isDirectory())
    {
        juce::Array<juce::File> files;
        folder.findChildFiles (files, juce::File::findFiles, false,
                               "*.png;*.jpg;*.jpeg;*.bmp;*.gif");

        juce::StringArray fromDisk;

        for (const auto& f : files)
            fromDisk.add (f.getFileName());

        fromDisk.sort (true);
        names.addArray (fromDisk);
    }

    return names;
}

void TextureLibrary::rescan()
{
    const juce::ScopedLock sl (cacheLock);
    cache.clear();
}

juce::Image TextureLibrary::image (const juce::String& name, int w, int h,
                                   float scalePercent, float angleDegrees)
{
    if (name.isEmpty() || w < 1 || h < 1)
        return {};

    const auto key = name + "|" + juce::String (w) + "x" + juce::String (h)
                       + "|" + juce::String (juce::roundToInt (scalePercent))
                       + "|" + juce::String (juce::roundToInt (angleDegrees));

    {
        const juce::ScopedLock sl (cacheLock);

        if (cache.contains (key))
            return cache[key];
    }

    auto result = isProcedural (name)
                    ? generatePattern (name, w, h, scalePercent, angleDegrees)
                    : tileUserImage   (name, w, h, scalePercent, angleDegrees);

    if (result.isValid())
    {
        const juce::ScopedLock sl (cacheLock);

        // The key carries the render size, and that changes every time the
        // preview is resized. Left alone this would grow without bound.
        if (cache.size() > 24)
            cache.clear();

        cache.set (key, result);
    }

    return result;
}
