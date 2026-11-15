// D:\workspace\Fanan_web_hub\src\stella_gui.cpp
//
// An exported plugin's window (see stella_gui.h): a small software renderer, and the same
// control behaviour as the Plugins Creator Studio's Play mode. A protected plugin's
// Registration window is drawn here too, with its text in Inter (stb_truetype).

#include "stella_gui.h"
#include "stella_keys.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>

#if defined (_WIN32)
 #include <direct.h>
 #include <io.h>
 #include <wchar.h>
#else
 #include <dirent.h>
 #include <sys/stat.h>
 #include <sys/types.h>
#endif

#if STELLA_ELEMENTS
 #include "stella_elements.h"

 #include <map>
#endif

// Registration's window, in every export (a plugin of its own that's protected, or one that
// plays a protected library): its text needs stb_truetype and Inter, which exports carry.
#if ! defined (STELLA_REGISTRATION) && defined (__has_include)
 #if __has_include ("stella_fonts.h") && __has_include ("stb_truetype.h")
  #define STELLA_REGISTRATION 1
 #endif
#endif

#ifndef STELLA_REGISTRATION
 #define STELLA_REGISTRATION 0
#endif

#if STELLA_REGISTRATION
 #include <unordered_map>
#endif

#if defined (__clang__)
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Weverything"
#elif defined (__GNUC__)
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wall"
 #pragma GCC diagnostic ignored "-Wextra"
 #pragma GCC diagnostic ignored "-Wunused-function"
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

// Registration's text: a copy of its own (static, so it never meets stella_elements.cpp's).
#if STELLA_REGISTRATION
 #define STB_TRUETYPE_IMPLEMENTATION
 #define STBTT_STATIC
 #include "stb_truetype.h"
#endif

#if defined (__clang__)
 #pragma clang diagnostic pop
#elif defined (__GNUC__)
 #pragma GCC diagnostic pop
#endif

#if STELLA_REGISTRATION
 #include "stella_fonts.h"   // generated: the Inter font, regular and bold, as byte arrays
#endif

namespace stella::gui
{
    namespace
    {
        //======================================================================
        // Pixels are 0xAARRGGBB, premultiplied.
        std::uint32_t premultiplied (std::uint32_t argb, float alpha)
        {
            const auto a = (float) (argb >> 24) * std::min (1.0f, std::max (0.0f, alpha)) / 255.0f;
            const auto r = (std::uint32_t) ((float) ((argb >> 16) & 0xff) * a + 0.5f);
            const auto g = (std::uint32_t) ((float) ((argb >> 8) & 0xff) * a + 0.5f);
            const auto b = (std::uint32_t) ((float) (argb & 0xff) * a + 0.5f);
            return ((std::uint32_t) (a * 255.0f + 0.5f) << 24) | (r << 16) | (g << 8) | b;
        }

        void blend (std::uint32_t& dst, std::uint32_t src)
        {
            const auto sa = src >> 24;

            if (sa == 0)
                return;

            if (sa == 255)
            {
                dst = src;
                return;
            }

            const auto inverse = 255 - sa;
            const auto rb = (((dst & 0x00ff00ffu) * inverse) >> 8) & 0x00ff00ffu;
            const auto ag = (((dst >> 8) & 0x00ff00ffu) * inverse) & 0xff00ff00u;
            const auto under = rb | ag;

            // Saturating add of the premultiplied source.
            std::uint32_t out = 0;

            for (int shift = 0; shift < 32; shift += 8)
            {
                const auto sum = ((under >> shift) & 0xff) + ((src >> shift) & 0xff);
                out |= (sum > 255 ? 255u : sum) << shift;
            }

            dst = out;
        }

        struct Canvas
        {
            std::uint32_t* pixels;
            int width, height;

            void plot (int x, int y, std::uint32_t argb, float alpha)
            {
                if (x < 0 || y < 0 || x >= width || y >= height || alpha <= 0.0f)
                    return;

                blend (pixels[(size_t) y * (size_t) width + (size_t) x], premultiplied (argb, alpha));
            }

            void fillRect (float x0, float y0, float x1, float y1, std::uint32_t argb, float alpha)
            {
                const auto left = std::max (0, (int) std::floor (x0)), right = std::min (width, (int) std::ceil (x1));
                const auto top = std::max (0, (int) std::floor (y0)), bottom = std::min (height, (int) std::ceil (y1));

                for (int y = top; y < bottom; ++y)
                {
                    const auto cy = std::min (1.0f, std::min ((float) y + 1.0f, y1) - std::max ((float) y, y0));

                    for (int x = left; x < right; ++x)
                    {
                        const auto cx = std::min (1.0f, std::min ((float) x + 1.0f, x1) - std::max ((float) x, x0));
                        plot (x, y, argb, alpha * cx * cy);
                    }
                }
            }

            void line (float x0, float y0, float x1, float y1, float thickness, std::uint32_t argb, float alpha)
            {
                const auto half = thickness * 0.5f;
                const auto left = std::max (0, (int) std::floor (std::min (x0, x1) - half - 1.0f));
                const auto right = std::min (width - 1, (int) std::ceil (std::max (x0, x1) + half + 1.0f));
                const auto top = std::max (0, (int) std::floor (std::min (y0, y1) - half - 1.0f));
                const auto bottom = std::min (height - 1, (int) std::ceil (std::max (y0, y1) + half + 1.0f));

                const auto dx = x1 - x0, dy = y1 - y0;
                const auto lengthSquared = dx * dx + dy * dy;

                for (int y = top; y <= bottom; ++y)
                {
                    for (int x = left; x <= right; ++x)
                    {
                        const auto px = (float) x + 0.5f - x0, py = (float) y + 0.5f - y0;
                        const auto t = lengthSquared > 0.0f ? std::min (1.0f, std::max (0.0f, (px * dx + py * dy) / lengthSquared)) : 0.0f;
                        const auto ex = px - t * dx, ey = py - t * dy;
                        const auto coverage = half + 0.5f - std::sqrt (ex * ex + ey * ey);

                        if (coverage > 0.0f)
                            plot (x, y, argb, alpha * std::min (1.0f, coverage));
                    }
                }
            }

            void circle (float cx, float cy, float radius, std::uint32_t argb, float alpha)
            {
                for (int y = (int) std::floor (cy - radius - 1.0f); y <= (int) std::ceil (cy + radius + 1.0f); ++y)
                {
                    for (int x = (int) std::floor (cx - radius - 1.0f); x <= (int) std::ceil (cx + radius + 1.0f); ++x)
                    {
                        const auto dx = (float) x + 0.5f - cx, dy = (float) y + 0.5f - cy;
                        const auto coverage = radius + 0.5f - std::sqrt (dx * dx + dy * dy);

                        if (coverage > 0.0f)
                            plot (x, y, argb, alpha * std::min (1.0f, coverage));
                    }
                }
            }

            /** A polyline, and optionally the area under it down to bottom. */
            void curve (const std::vector<float>& xs, const std::vector<float>& ys, float bottom, std::uint32_t argb, float fillAlpha)
            {
                if (xs.size() < 2)
                    return;

                if (fillAlpha > 0.0f)
                {
                    // One pass over the columns, so each column is filled exactly once.
                    size_t segment = 0;

                    for (int x = (int) std::floor (xs.front()); x < (int) std::ceil (xs.back()); ++x)
                    {
                        const auto centre = (float) x + 0.5f;

                        while (segment + 2 < xs.size() && xs[segment + 1] < centre)
                            ++segment;

                        const auto span = xs[segment + 1] - xs[segment];
                        const auto t = span > 0.0f ? std::min (1.0f, std::max (0.0f, (centre - xs[segment]) / span)) : 0.0f;
                        const auto top = ys[segment] + (ys[segment + 1] - ys[segment]) * t;
                        fillRect ((float) x, top, (float) x + 1.0f, bottom, argb, fillAlpha);
                    }
                }

                for (size_t i = 0; i + 1 < xs.size(); ++i)
                    line (xs[i], ys[i], xs[i + 1], ys[i + 1], 2.0f, argb, 1.0f);
            }
        };

        std::uint32_t mix (std::uint32_t a, std::uint32_t b, float t)
        {
            std::uint32_t out = 0xff000000u;

            for (int shift = 0; shift < 24; shift += 8)
            {
                const auto ca = (float) ((a >> shift) & 0xff), cb = (float) ((b >> shift) & 0xff);
                out |= (std::uint32_t) (ca + (cb - ca) * t + 0.5f) << shift;
            }

            return out;
        }

        constexpr std::uint32_t green = 0xff00c853, amber = 0xffffb300, red = 0xffd50000;

        /** A level as a meter shows it, 0..1: a signal in dB (-60..+6), a display as it is (as
            Sonify's meters show it). */
        float fractionOf (float level, bool isDisplay)
        {
            if (isDisplay)
                return std::min (1.0f, std::max (0.0f, level));

            return std::min (1.0f, std::max (0.0f, (20.0f * std::log10 (std::max (level, 1.0e-5f)) + 60.0f) / 66.0f));
        }

       #if STELLA_ELEMENTS
        /** A font's name as the elements' fonts are kept by (lower case, no spaces). */
        std::string normalisedName (const std::string& name)
        {
            std::string result;

            for (const auto c : name)
                if (c != ' ' && c != '-' && c != '_')
                    result += (char) (c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);

            return result;
        }
       #endif
    }

    //==========================================================================
    Watch watchOf (int slot)
    {
        Watch watch;

        if (slot >= 0 && slot < numWidgets)
        {
            const auto& w = widgets[slot];

            if (w.source == nullptr || w.source[0] == 0)
                return watch;

            watch.source = w.source;
            watch.isDisplay = w.isDisplay != 0;
            watch.level = w.kind == Kind::meter || w.kind == Kind::lamp || w.kind == Kind::custom || w.kind == Kind::filter;

            // A programmed element may read both: its level and its samples.
            watch.samples = ! watch.isDisplay && (w.kind == Kind::scope || w.kind == Kind::custom);
        }
        else if (slot >= numWidgets && slot < numWatchSlots())
        {
            const auto& r = roleSources[slot - numWidgets];

            if (r.source == nullptr || r.source[0] == 0)
                return watch;

            watch.source = r.source;
            watch.isDisplay = r.isDisplay != 0;
            watch.level = true;
            watch.samples = ! watch.isDisplay;
        }

        return watch;
    }

    //==========================================================================
   #if STELLA_ELEMENTS
    /** The programmed elements: the runtime that runs them, and what they ask of the plugin. */
    struct Editor::Elements final : ui::ElementHost
    {
        explicit Elements (Editor& e)
            : editor (e), started (std::chrono::steady_clock::now())
        {
            for (int i = 0; i < numWidgets; ++i)
            {
                const auto* c = custom (i);

                if (c == nullptr)
                    continue;

                const auto& w = widgets[i];
                auto& slots = params[i];
                slots.push_back (w.param != nullptr ? editor.host.findParam (w.param) : -1);

                for (int r = 0; r < c->numRoles; ++r)
                    slots.push_back (c->roleParams[r] != nullptr ? editor.host.findParam (c->roleParams[r]) : -1);

                runtime.create (i, c->type, w.w, w.h);
            }
        }

        const CustomElement* custom (int element) const
        {
            if (element < 0 || element >= numWidgets || widgets[element].kind != Kind::custom)
                return nullptr;

            const auto index = widgets[element].count;
            return index >= 0 && index < numCustomElements ? &customElements[index] : nullptr;
        }

        int paramOf (int element, int slot) const
        {
            const auto found = params.find (element);
            return found != params.end() && slot >= 0 && slot < (int) found->second.size() ? found->second[(size_t) slot] : -1;
        }

        /** A role source's watch slot; -1 if the element has none by that role. */
        int roleSlot (int element, const std::string& role) const
        {
            for (int r = 0; r < numRoleSources; ++r)
                if (roleSources[r].widget == element && roleSources[r].role != nullptr && role == roleSources[r].role)
                    return numWidgets + r;

            return -1;
        }

        //======================================================================
        bool paramInfo (int element, int slot, int info, float& result) override
        {
            const auto p = paramOf (element, slot);

            if (p < 0)
                return false;

            if (info == valueInfo)
            {
                result = editor.host.getValue (p);
                return true;
            }

            float min = 0.0f, max = 1.0f, def = 0.0f, skew = 1.0f;
            editor.host.getRange (p, min, max, def, skew);

            switch (info)
            {
                case minInfo:      result = min; return true;
                case maxInfo:      result = max; return true;
                case defaultInfo:  result = def; return true;
                case skewInfo:     result = skew; return true;
                default:           return false;
            }
        }

        int findSlot (int element, const std::string& role) override
        {
            const auto* c = custom (element);

            if (c == nullptr)
                return -1;

            if (role.empty() || role == "param")
                return widgets[element].param != nullptr ? 0 : -1;

            for (int r = 0; r < c->numRoles; ++r)
                if (c->roles[r] != nullptr && role == c->roles[r])
                    return r + 1;

            return -1;
        }

        void setValue (int element, int slot, float newValue) override
        {
            if (const auto p = paramOf (element, slot); p >= 0)
                editor.host.setValue (p, newValue);
        }

        void gesture (int element, int slot, bool starts) override
        {
            if (const auto p = paramOf (element, slot); p >= 0)
            {
                if (starts) editor.host.beginEdit (p);
                else        editor.host.endEdit (p);
            }
        }

        bool text (int element, int which, int index, const std::string& key, std::string& result) override
        {
            const auto* c = custom (element);

            if (c == nullptr)
                return false;

            const char* t = nullptr;

            switch (which)
            {
                case labelText:        t = c->label; break;
                case optionText:       t = index >= 0 && index < c->numOptions ? c->options[index] : nullptr; break;
                case paramNameText:    t = index >= 0 && index <= c->numRoles ? c->slotNames[index] : nullptr; break;
                case unitText:         t = index >= 0 && index <= c->numRoles ? c->slotUnits[index] : nullptr; break;
                case presetNameText:
                case presetGroupText:
                    if (index < 0 || index >= editor.numAllPresets())
                        return false;

                    result = which == presetNameText ? editor.presetNameAt (index) : editor.presetGroupAt (index);
                    return true;

                case settingText:
                    for (int s = 0; s < c->numSettings; ++s)
                        if (c->settingKeys[s] != nullptr && key == c->settingKeys[s])
                            t = c->settingValues[s];
                    break;

                case sourceText:
                    if (key.empty())
                        t = widgets[element].source;
                    else if (const auto slot = roleSlot (element, key); slot >= 0)
                        t = roleSources[slot - numWidgets].source;
                    break;

                default:
                    break;
            }

            if (t == nullptr)
                return false;

            result = t;
            return true;
        }

        int numOptions (int element) override
        {
            const auto* c = custom (element);
            return c != nullptr ? c->numOptions : 0;
        }

        std::uint32_t colour (int element) override                          { return custom (element) != nullptr ? widgets[element].colour : 0u; }
        float level (int element, bool rms) override                         { return editor.host.takeLevel (element, rms); }
        void readScope (int element, float* destination, int n) override     { editor.host.readScope (element, destination, n); }
        void playNote (int, int note, float velocity) override              { editor.host.playNote (note, velocity); }
        bool isNoteDown (int note) override                                  { return editor.host.isNoteDown (note); }

        float sourceLevel (int element, const std::string& role, bool rms) override
        {
            if (role.empty())
                return level (element, rms);

            const auto slot = roleSlot (element, role);
            return slot >= 0 ? editor.host.takeLevel (slot, rms) : 0.0f;
        }

        void readSourceScope (int element, const std::string& role, float* destination, int numSamples) override
        {
            const auto slot = role.empty() ? element : roleSlot (element, role);

            if (slot >= 0)
                editor.host.readScope (slot, destination, numSamples);
            else
                std::fill (destination, destination + std::max (0, numSamples), 0.0f);
        }

        //======================================================================
        // The presets: the plugin's own, then the user's (their files), as every preset manager
        // has them, with Save, Load, the banks...
        int presetInfo (int, int which, int index) override
        {
            switch (which)
            {
                case presetCountInfo:    return editor.numAllPresets();
                case presetCurrentInfo:  return editor.currentPreset >= 0 && editor.currentPreset < editor.numAllPresets() ? editor.currentPreset : -1;
                case presetVersionInfo:  return editor.presetVersion;
                case presetModifiedInfo: return editor.isPresetModified() ? 1 : 0;
                case presetCanDoInfo:    return index >= 0 && index <= (int) PresetCommand::showFolder && editor.canDoPreset ((PresetCommand) index) ? 1 : 0;
                default:                 return 0;
            }
        }

        void presetTask (int, int task, int index) override
        {
            if (task == choosePresetTask)
                editor.choosePreset (index);
            else if (task == stepPresetTask)
                editor.stepPreset (index < 0 ? -1 : 1);
            else if (task == presetCommandTask && index >= 0 && index <= (int) PresetCommand::showFolder)
                editor.doPreset ((PresetCommand) index);
        }

        //======================================================================
        // Pictures, by file name: decoded once, then scaled to what's asked.
        const Image* picture (const std::string& name)
        {
            if (const auto found = decoded.find (name); found != decoded.end())
                return found->second.width > 0 ? &found->second : nullptr;

            auto& image = decoded[name];

            for (int i = 0; i < numPictures; ++i)
                if (pictures[i].name != nullptr && name == pictures[i].name)
                    image = Editor::decode (pictures[i].png, pictures[i].size);

            return image.width > 0 ? &image : nullptr;
        }

        bool pictureSize (int, const std::string& name, int& w, int& h) override
        {
            const auto* image = picture (name);

            if (image == nullptr)
                return false;

            w = image->width;
            h = image->height;
            return true;
        }

        bool readPicture (int, const std::string& name, int w, int h, std::uint32_t* destination) override
        {
            const auto* image = picture (name);

            if (image == nullptr || w <= 0 || h <= 0 || destination == nullptr)
                return false;

            // Each pixel the average of the source pixels it covers (or the nearest, growing).
            for (int y = 0; y < h; ++y)
            {
                const auto sy0 = (int) ((long long) y * image->height / h);
                const auto sy1 = std::max (sy0 + 1, (int) ((long long) (y + 1) * image->height / h));

                for (int x = 0; x < w; ++x)
                {
                    const auto sx0 = (int) ((long long) x * image->width / w);
                    const auto sx1 = std::max (sx0 + 1, (int) ((long long) (x + 1) * image->width / w));
                    std::uint32_t sum[4] {};
                    std::uint32_t n = 0;

                    for (int sy = sy0; sy < std::min (sy1, image->height); ++sy)
                    {
                        for (int sx = sx0; sx < std::min (sx1, image->width); ++sx)
                        {
                            const auto p = image->pixels[(size_t) sy * (size_t) image->width + (size_t) sx];

                            for (int c = 0; c < 4; ++c)
                                sum[c] += (p >> (c * 8)) & 0xff;

                            ++n;
                        }
                    }

                    std::uint32_t out = 0;

                    for (int c = 0; c < 4 && n > 0; ++c)
                        out |= ((sum[c] + n / 2) / n) << (c * 8);

                    destination[(size_t) y * (size_t) w + (size_t) x] = out;
                }
            }

            return true;
        }

        int fontData (const std::string& name, bool bold, std::uint8_t* destination, int capacity) override
        {
            const auto wanted = normalisedName (name);

            for (int i = 0; i < numFonts; ++i)
            {
                if (fonts[i].name == nullptr || wanted != fonts[i].name || (fonts[i].bold != 0) != bold)
                    continue;

                if (destination != nullptr && capacity >= fonts[i].size)
                    std::memcpy (destination, fonts[i].data, (size_t) fonts[i].size);

                return fonts[i].size;
            }

            return 0;
        }

        double seconds() override
        {
            return std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count();
        }

        //======================================================================
        /** Where an element's open popup lies in the window: kept inside it where it fits, as
            the studio keeps it. */
        bool popupArea (int element, int& x, int& y, int& w, int& h)
        {
            if (custom (element) == nullptr || ! editor.isShown (element) || ! runtime.popupArea (element, x, y, w, h))
                return false;

            x = std::min (std::max (0, width - w), std::max (0, widgets[element].x + x));
            y = std::min (std::max (0, height - h), std::max (0, widgets[element].y + y));
            return true;
        }

        /** Whose open popup is at a point (the frontmost); -1 if none. */
        int popupAt (int px, int py)
        {
            for (int i = numWidgets; --i >= 0;)
            {
                int x = 0, y = 0, w = 0, h = 0;

                if (popupArea (i, x, y, w, h) && px >= x && py >= y && px < x + w && py < y + h)
                    return i;
            }

            return -1;
        }

        bool closePopups()
        {
            bool closed = false;

            for (int i = 0; i < numWidgets; ++i)
            {
                int x = 0, y = 0, w = 0, h = 0;

                if (custom (i) != nullptr && runtime.popupArea (i, x, y, w, h))
                {
                    runtime.closePopup (i);
                    closed = true;
                }
            }

            return closed;
        }

        /** The mouse to an element, or its popup, in its own pixels. */
        bool mouse (int element, bool popup, int event, int x, int y, bool fine, bool doubleClick = false, float notches = 0.0f)
        {
            int ox = 0, oy = 0, w = 0, h = 0;

            if (popup)
            {
                if (! popupArea (element, ox, oy, w, h))
                    return false;
            }
            else
            {
                ox = widgets[element].x;
                oy = widgets[element].y;
            }

            ui::Mouse m;
            m.x = (float) (x - ox) + 0.5f;
            m.y = (float) (y - oy) + 0.5f;
            m.shift = fine;
            m.doubleClick = doubleClick;
            lastX = x;
            lastY = y;
            return runtime.mouse (element, popup, event, m, notches);
        }

        Editor& editor;
        std::map<int, std::vector<int>> params;   // per element: its parameters' indexes, by slot
        std::map<std::string, Image> decoded;     // pictures asked for (empty: there's none by that name)
        std::chrono::steady_clock::time_point started;
        int pressed = -1, hovered = -1, lastX = 0, lastY = 0;
        bool pressedPopup = false, hoveredPopup = false;
        ui::ElementRuntime runtime { *this };   // last: it goes first, while the rest is still there
    };
   #else
    struct Editor::Elements {};
   #endif

    //==========================================================================
   #if STELLA_REGISTRATION
    namespace
    {
        using Clock = std::chrono::steady_clock;

        //======================================================================
        // Registration's drawing: anti-aliased rounded shapes, and text in Inter.
        struct Rect
        {
            float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;

            bool contains (float px, float py) const noexcept    { return px >= x && py >= y && px < x + w && py < y + h; }
            float right() const noexcept                         { return x + w; }
            float bottom() const noexcept                        { return y + h; }
            float centreY() const noexcept                       { return y + h * 0.5f; }
            Rect expanded (float by) const noexcept              { return { x - by, y - by, w + by * 2.0f, h + by * 2.0f }; }
        };

        float clamp01 (float v) noexcept    { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

        /** Draws into the window's pixels, never outside them (or outside its clip). */
        struct Painter
        {
            Painter (std::uint32_t* p, int w, int h)
                : pixels (p), width (w), height (h), clipRight (w), clipBottom (h) {}

            void clipTo (const Rect& r)
            {
                clipLeft = std::max (0, (int) std::floor (r.x));
                clipTop = std::max (0, (int) std::floor (r.y));
                clipRight = std::min (width, (int) std::ceil (r.right()));
                clipBottom = std::min (height, (int) std::ceil (r.bottom()));
            }

            void unclip()
            {
                clipLeft = clipTop = 0;
                clipRight = width;
                clipBottom = height;
            }

            void plot (int x, int y, std::uint32_t argb, float alpha)
            {
                if (x < clipLeft || y < clipTop || x >= clipRight || y >= clipBottom || alpha <= 0.0f)
                    return;

                blend (pixels[(size_t) y * (size_t) width + (size_t) x], premultiplied (argb, alpha));
            }

            /** How far a point is outside a rounded box (negative: inside). */
            static float distance (const Rect& r, float radius, float px, float py)
            {
                const auto hw = r.w * 0.5f, hh = r.h * 0.5f;
                radius = std::max (0.0f, std::min (radius, std::min (hw, hh)));
                const auto qx = std::fabs (px - (r.x + hw)) - (hw - radius);
                const auto qy = std::fabs (py - (r.y + hh)) - (hh - radius);
                const auto ox = std::max (qx, 0.0f), oy = std::max (qy, 0.0f);
                return std::sqrt (ox * ox + oy * oy) + std::min (std::max (qx, qy), 0.0f) - radius;
            }

            /** Calls f (x, y, distance) for each pixel near a rounded box (within margin of it). */
            template <typename Function>
            void eachPixel (const Rect& r, float radius, float margin, Function&& f)
            {
                const auto x0 = std::max (clipLeft, (int) std::floor (r.x - margin)), x1 = std::min (clipRight, (int) std::ceil (r.right() + margin));
                const auto y0 = std::max (clipTop, (int) std::floor (r.y - margin)), y1 = std::min (clipBottom, (int) std::ceil (r.bottom() + margin));

                for (int y = y0; y < y1; ++y)
                    for (int x = x0; x < x1; ++x)
                        f (x, y, distance (r, radius, (float) x + 0.5f, (float) y + 0.5f));
            }

            /** A rounded box, its colour going from top to bottom. */
            void fill (const Rect& r, float radius, std::uint32_t top, std::uint32_t bottom, float alpha = 1.0f)
            {
                eachPixel (r, radius, 1.0f, [&] (int x, int y, float d)
                {
                    const auto coverage = clamp01 (0.5f - d);

                    if (coverage > 0.0f)
                        plot (x, y, top == bottom ? top : mix (top, bottom, clamp01 (((float) y + 0.5f - r.y) / std::max (1.0f, r.h))), alpha * coverage);
                });
            }

            void fill (const Rect& r, float radius, std::uint32_t colour, float alpha = 1.0f)    { fill (r, radius, colour, colour, alpha); }

            /** A line just inside a rounded box's edge. */
            void outline (const Rect& r, float radius, float thickness, std::uint32_t colour, float alpha = 1.0f)
            {
                eachPixel (r, radius, 1.0f, [&] (int x, int y, float d)
                {
                    const auto coverage = clamp01 (0.5f - d) - clamp01 (0.5f - (d + thickness));

                    if (coverage > 0.0f)
                        plot (x, y, colour, alpha * coverage);
                });
            }

            /** A soft shadow around a rounded box (not under it: what goes on top covers that). */
            void shadow (const Rect& r, float radius, float blur, float alpha)
            {
                eachPixel (r, radius, blur, [&] (int x, int y, float d)
                {
                    if (d > -1.0f && d < blur)
                    {
                        const auto t = 1.0f - clamp01 (d / blur);
                        plot (x, y, 0xff000000u, alpha * t * t * clamp01 (d + 1.0f));
                    }
                });
            }

            /** All of it, under a colour. */
            void wash (std::uint32_t colour, float alpha)
            {
                const auto over = premultiplied (colour, alpha);

                for (size_t i = 0, n = (size_t) width * (size_t) height; i < n; ++i)
                    blend (pixels[i], over);
            }

            std::uint32_t* pixels;
            int width, height;
            int clipLeft = 0, clipTop = 0, clipRight, clipBottom;
        };

        //======================================================================
        /** The next code point of UTF-8 text ('?' for a broken sequence). */
        std::uint32_t nextCodePoint (const std::string& text, size_t& i)
        {
            const auto c = (unsigned char) text[i++];

            if (c < 0x80)
                return c;

            const int extra = c >= 0xf0 ? 3 : (c >= 0xe0 ? 2 : (c >= 0xc0 ? 1 : -1));

            if (extra < 0)
                return '?';

            std::uint32_t code = c & (0x3fu >> extra);

            for (int k = 0; k < extra; ++k)
            {
                if (i >= text.size() || ((unsigned char) text[i] & 0xc0) != 0x80)
                    return '?';

                code = (code << 6) | ((unsigned char) text[i++] & 0x3fu);
            }

            return code;
        }

        /** Inter, regular and bold, read once. */
        struct InterFaces
        {
            InterFaces()
            {
                const unsigned char* data[2] { ui::fonts::regular, ui::fonts::bold };

                for (int i = 0; i < 2; ++i)
                    ok[i] = stbtt_InitFont (&faces[i], data[i], stbtt_GetFontOffsetForIndex (data[i], 0)) != 0;
            }

            stbtt_fontinfo faces[2];
            bool ok[2] {};
        };

        InterFaces& interFaces()
        {
            static InterFaces faces;
            return faces;
        }

        /** Text: measured, drawn (each glyph rendered once, at a quarter-pixel position). */
        class Text
        {
        public:
            /** A line's width; tracking: extra space after each character. */
            float width (const std::string& text, float size, bool bold, float tracking = 0.0f)
            {
                return run (nullptr, text, 0.0f, 0.0f, size, bold, tracking, 0u, 0.0f);
            }

            /** Draws a line from x on a baseline; returns where it ends. */
            float draw (Painter& p, const std::string& text, float x, float baseline, float size, bool bold, std::uint32_t colour,
                        float alpha = 1.0f, float tracking = 0.0f)
            {
                return run (&p, text, x, baseline, size, bold, tracking, colour, alpha);
            }

            /** The baseline that centres capitals and figures on y. */
            static float baselineFor (float y, float size)    { return y + size * 0.3635f; }   // Inter's cap height: 0.727 em

            /** The text cut to fit maxWidth, an ellipsis after what's left. */
            std::string fitted (const std::string& text, float size, bool bold, float maxWidth, float tracking = 0.0f)
            {
                if (width (text, size, bold, tracking) <= maxWidth)
                    return text;

                const std::string dots = "\xe2\x80\xa6";
                auto shortened = text;

                while (! shortened.empty())
                {
                    // One code point off the end.
                    while (! shortened.empty())
                    {
                        const auto c = (unsigned char) shortened.back();
                        shortened.pop_back();

                        if ((c & 0xc0) != 0x80)
                            break;
                    }

                    while (! shortened.empty() && shortened.back() == ' ')
                        shortened.pop_back();

                    if (width (shortened + dots, size, bold, tracking) <= maxWidth)
                        return shortened + dots;
                }

                return dots;
            }

            /** The text in lines of at most maxWidth (at most maxLines: the last one cut short). */
            std::vector<std::string> wrapped (const std::string& text, float size, bool bold, float maxWidth, int maxLines)
            {
                std::vector<std::string> lines;
                std::string line;
                size_t i = 0;

                while (i < text.size())
                {
                    const auto space = text.find (' ', i);
                    const auto word = text.substr (i, space == std::string::npos ? std::string::npos : space - i);
                    i = space == std::string::npos ? text.size() : space + 1;

                    if (word.empty())
                        continue;

                    const auto longer = line.empty() ? word : line + " " + word;

                    if (line.empty() || width (longer, size, bold) <= maxWidth)
                    {
                        line = longer;
                        continue;
                    }

                    lines.push_back (line);
                    line = word;

                    if ((int) lines.size() == maxLines - 1)
                    {
                        // The last line takes the rest.
                        if (i < text.size())
                            line += " " + text.substr (i);

                        break;
                    }
                }

                if (! line.empty())
                    lines.push_back (line);

                if (! lines.empty())
                    lines.back() = fitted (lines.back(), size, bold, maxWidth);

                return lines;
            }

        private:
            struct Glyph
            {
                int left = 0, top = 0, w = 0, h = 0;
                std::vector<std::uint8_t> coverage;
            };

            const Glyph& glyph (const stbtt_fontinfo* info, int face, float size, float scale, std::uint32_t code, int quarter)
            {
                const auto key = ((std::uint64_t) face << 40) | ((std::uint64_t) (std::min (4095L, std::lround (size * 8.0f))) << 26)
                               | ((std::uint64_t) quarter << 22) | (std::uint64_t) (code & 0x1fffffu);

                if (const auto found = glyphs.find (key); found != glyphs.end())
                    return found->second;

                if (glyphs.size() > 4096)
                    glyphs.clear();

                auto& g = glyphs[key];
                const auto shift = (float) quarter * 0.25f;
                int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
                stbtt_GetCodepointBitmapBoxSubpixel (info, (int) code, scale, scale, shift, 0.0f, &x0, &y0, &x1, &y1);

                if (x1 > x0 && y1 > y0 && x1 - x0 < 1024 && y1 - y0 < 1024)
                {
                    g.left = x0;
                    g.top = y0;
                    g.w = x1 - x0;
                    g.h = y1 - y0;
                    g.coverage.assign ((size_t) g.w * (size_t) g.h, 0);
                    stbtt_MakeCodepointBitmapSubpixel (info, g.coverage.data(), g.w, g.h, g.w, scale, scale, shift, 0.0f, (int) code);
                }

                return g;
            }

            float run (Painter* p, const std::string& text, float x, float baseline, float size, bool bold, float tracking,
                       std::uint32_t colour, float alpha)
            {
                auto& faces = interFaces();
                const int face = bold && faces.ok[1] ? 1 : 0;

                if (! faces.ok[face] || size <= 0.5f)
                    return x;

                const auto* info = &faces.faces[face];
                const auto scale = stbtt_ScaleForMappingEmToPixels (info, size);
                const auto by = (int) std::lround (baseline);
                std::uint32_t previous = 0;

                for (size_t i = 0; i < text.size();)
                {
                    const auto code = nextCodePoint (text, i);

                    if (previous != 0)
                        x += scale * (float) stbtt_GetCodepointKernAdvance (info, (int) previous, (int) code);

                    int advance = 0, bearing = 0;
                    stbtt_GetCodepointHMetrics (info, (int) code, &advance, &bearing);

                    if (p != nullptr)
                    {
                        auto whole = (int) std::floor (x);
                        auto quarter = (int) std::lround ((x - (float) whole) * 4.0f);

                        if (quarter >= 4)
                        {
                            ++whole;
                            quarter = 0;
                        }

                        const auto& g = glyph (info, face, size, scale, code, quarter);

                        for (int row = 0; row < g.h; ++row)
                            for (int col = 0; col < g.w; ++col)
                                if (const auto c = g.coverage[(size_t) row * (size_t) g.w + (size_t) col]; c != 0)
                                    p->plot (whole + g.left + col, by + g.top + row, colour, alpha * (float) c / 255.0f);
                    }

                    x += scale * (float) advance + tracking;
                    previous = code;
                }

                return x;
            }

            std::unordered_map<std::uint64_t, Glyph> glyphs;
        };

        //======================================================================
        // Registration's look.
        constexpr std::uint32_t panelTop = 0xff24272f, panelBottom = 0xff1b1d23, inset = 0xff131519, fieldBack = 0xff0f1114,
                                bright = 0xfff4f5f7, body = 0xffdfe2e7, muted = 0xff9198a5, faint = 0xff6c7280,
                                accent = 0xff5b8cff, accentDark = 0xff4570e6, good = 0xff3dd68c, warn = 0xfff5b544, bad = 0xffff6b6b,
                                button = 0xff2d313a, buttonHover = 0xff363b46, buttonDown = 0xff262930;

        /** Where everything of the Registration window lies, scaled to the plugin's window. */
        struct RegistrationLayout
        {
            enum Part { none = -1, closePart, submitPart, copyPart, pastePart, fieldPart, panelPart, outsidePart };

            float s = 1.0f;          // the scale
            bool compact = false;    // small: without the explanations
            int shown = 0, more = 0; // products listed, and those that didn't fit
            Rect panel, list, idBox, copy, field, paste, result, close, submit;
            float titleY = 0, subtitleY = 0, idLabelY = 0, hintY = 0, serialLabelY = 0;

            int partAt (float x, float y) const
            {
                if (close.contains (x, y))   return closePart;
                if (submit.contains (x, y))  return submitPart;
                if (copy.contains (x, y))    return copyPart;
                if (paste.contains (x, y))   return pastePart;
                if (field.contains (x, y))   return fieldPart;
                if (panel.contains (x, y))   return panelPart;
                return outsidePart;
            }
        };

        RegistrationLayout registrationLayout (int windowWidth, int windowHeight, int numItems)
        {
            RegistrationLayout l;
            constexpr int maxRows = 4;
            l.shown = std::min (numItems, maxRows);
            l.more = numItems - l.shown;
            const auto rows = (float) (l.shown + (l.more > 0 ? 1 : 0));
            constexpr float baseWidth = 460.0f;

            auto heightOf = [rows] (bool compact)
            {
                return 24.0f + 26.0f + (compact ? 0.0f : 20.0f) + 14.0f + (30.0f * rows + 8.0f) + 16.0f + 18.0f + 38.0f
                     + (compact ? 0.0f : 24.0f) + 14.0f + 18.0f + 38.0f + 8.0f + 36.0f + 10.0f + 38.0f + 24.0f;
            };

            auto fit = [&] (bool compact)
            {
                return std::min (1.25f, std::min (((float) windowWidth - 24.0f) / baseWidth, ((float) windowHeight - 24.0f) / heightOf (compact)));
            };

            l.s = fit (false);

            if (l.s < 0.85f)
            {
                l.compact = true;
                l.s = fit (true);
            }

            const auto s = l.s = std::max (0.3f, l.s);
            const auto pw = std::round (baseWidth * s), ph = std::round (heightOf (l.compact) * s);
            l.panel = { std::floor (((float) windowWidth - pw) * 0.5f), std::floor (((float) windowHeight - ph) * 0.5f), pw, ph };

            const auto x = l.panel.x + 24.0f * s, inner = pw - 48.0f * s, button = 92.0f * s, gap = 8.0f * s;
            auto y = l.panel.y + 24.0f * s;

            l.titleY = y + 19.0f * s;
            y += 26.0f * s;

            if (! l.compact)
            {
                l.subtitleY = y + 13.0f * s;
                y += 20.0f * s;
            }

            y += 14.0f * s;
            l.list = { x, y, inner, (30.0f * rows + 8.0f) * s };
            y += l.list.h + 16.0f * s;

            l.idLabelY = y + 11.0f * s;
            y += 18.0f * s;
            l.idBox = { x, y, inner - button - gap, 38.0f * s };
            l.copy = { x + inner - button, y, button, 38.0f * s };
            y += 38.0f * s;

            if (! l.compact)
            {
                l.hintY = y + 17.0f * s;
                y += 24.0f * s;
            }

            y += 14.0f * s;
            l.serialLabelY = y + 11.0f * s;
            y += 18.0f * s;
            l.field = { x, y, inner - button - gap, 38.0f * s };
            l.paste = { x + inner - button, y, button, 38.0f * s };
            y += 38.0f * s + 8.0f * s;

            l.result = { x, y, inner, 36.0f * s };
            y += 36.0f * s + 10.0f * s;

            l.submit = { x + inner - 124.0f * s, y, 124.0f * s, 38.0f * s };
            l.close = { l.submit.x - gap - button, y, button, 38.0f * s };
            return l;
        }
    }

    /** A protected plugin's Registration window: its state, its look and what it does. */
    struct Editor::Registration
    {
        using Part = RegistrationLayout::Part;

        explicit Registration (Editor& e) : editor (e)
        {
            for (int i = 0; i < numWidgets; ++i)
                hasBadge = hasBadge || widgets[i].kind == Kind::registration;

            refresh();
        }

        //======================================================================
        void refresh()
        {
            items = editor.host.licenceItems();
            checked = Clock::now();
        }

        bool anyUnregistered() const
        {
            for (const auto& item : items)
                if (! item.registered)
                    return true;

            return false;
        }

        RegistrationLayout layout() const    { return registrationLayout (width, height, (int) items.size()); }

        /** The small tab that opens it when the panel has no badge. */
        Rect tab()
        {
            const auto ts = std::min (1.15f, std::max (0.8f, (float) std::min (width, height) / 520.0f));
            const auto w = text.width (tabLabel, 11.5f * ts, true) + 32.0f * ts, h = 24.0f * ts;
            return { std::floor ((float) width - w - 8.0f * ts), std::floor ((float) height - h - 8.0f * ts), std::round (w), std::round (h) };
        }

        bool showsTab()    { return ! open && ! hasBadge && anyUnregistered(); }

        //======================================================================
        // The serial field: ASCII only, so a character is a byte.
        int selectionStart() const noexcept    { return std::min (caret, anchor); }
        int selectionEnd() const noexcept      { return std::max (caret, anchor); }

        void moveCaret (int to, bool extend)
        {
            caret = std::min ((int) serial.size(), std::max (0, to));

            if (! extend)
                anchor = caret;

            caretMoved = Clock::now();
        }

        bool eraseSelection()
        {
            if (caret == anchor)
                return false;

            serial.erase ((size_t) selectionStart(), (size_t) (selectionEnd() - selectionStart()));
            moveCaret (selectionStart(), false);
            return true;
        }

        /** Typed or pasted text, as a serial has it: its digits, and dashes; anything else
            (letters, spaces, line breaks and the like) left out. */
        void insert (const std::string& typed)
        {
            std::string clean;

            for (size_t i = 0; i < typed.size();)
            {
                auto c = nextCodePoint (typed, i);

                if ((c >= 0x2010 && c <= 0x2015) || c == 0x2212)
                    c = '-';   // a dash from a mail or a web page

                if ((c >= '0' && c <= '9') || c == '-')
                    clean += (char) c;
            }

            eraseSelection();
            clean = clean.substr (0, (size_t) std::max (0, maxSerial - (int) serial.size()));
            serial.insert ((size_t) caret, clean);
            moveCaret (caret + (int) clean.size(), false);
            say ({}, 0);
        }

        /** The boundary nearest to x in the field. */
        int caretAt (const RegistrationLayout& l, float x)
        {
            const auto size = 14.5f * l.s, tracking = 0.4f * l.s;
            const auto local = x - (l.field.x + 12.0f * l.s) + scroll;
            int best = 0;
            float bestDistance = std::fabs (local);

            for (int i = 1; i <= (int) serial.size(); ++i)
            {
                const auto d = std::fabs (local - text.width (serial.substr (0, (size_t) i), size, true, tracking));

                if (d < bestDistance)
                {
                    best = i;
                    bestDistance = d;
                }
            }

            return best;
        }

        void say (const std::string& message, int newTone)
        {
            result = message;
            tone = newTone;
        }

        //======================================================================
        // What it does.
        void submit()
        {
            bool any = false;

            for (const auto c : serial)
                any = any || (c != '-');

            if (! any)
            {
                say ("Type or paste your serial first.", -1);
                return;
            }

            const auto why = editor.host.licenceRegister (serial);
            refresh();

            if (why.empty())
                say ("Registered \xe2\x80\x94 thank you!", 1);
            else
                say (why, -1);
        }

        void copyId()
        {
            if (computerId.empty())
                say ("This computer's ID couldn't be read.", -1);
            else if (editor.clipboard == nullptr)
                say ("The clipboard can't be reached here.", -1);
            else
            {
                editor.clipboard->setText (computerId);
                copiedAt = Clock::now();
                say ("Your computer ID is copied: send it to the seller.", 0);
            }
        }

        void copySelection (bool cut)
        {
            if (editor.clipboard == nullptr || serial.empty())
                return;

            const bool all = caret == anchor;
            editor.clipboard->setText (all ? serial : serial.substr ((size_t) selectionStart(), (size_t) (selectionEnd() - selectionStart())));

            if (cut && ! all)
                eraseSelection();
        }

        void paste()
        {
            std::string pasted;

            if (editor.clipboard == nullptr)
                say ("The clipboard can't be reached here: type the serial.", -1);
            else if (editor.clipboard->getText (pasted))
                insert (pasted);
            // Otherwise it comes later (Editor::pasteText).
        }

        void activate (int part)
        {
            switch (part)
            {
                case Part::closePart:   editor.closeRegistration(); break;
                case Part::submitPart:  submit(); break;
                case Part::copyPart:    copyId(); break;
                case Part::pastePart:   paste(); break;
                default:                break;
            }
        }

        bool key (Key k, std::uint32_t character, int modifiers, bool fromHost)
        {
            // The same key from the host and from the window at once (some hosts pass it on
            // both ways): once.
            const auto now = Clock::now();

            if (fromHost != lastFromHost && k == lastKey && character == lastCharacter && modifiers == lastModifiers
                 && now - lastKeyAt < std::chrono::milliseconds (40))
            {
                lastKeyAt = {};
                return true;
            }

            lastKey = k;
            lastCharacter = character;
            lastModifiers = modifiers;
            lastFromHost = fromHost;
            lastKeyAt = now;

            const bool extend = (modifiers & shiftKey) != 0, command = (modifiers & commandKey) != 0;

            switch (k)
            {
                case Key::character:
                    if (command)
                    {
                        switch (character >= 'A' && character <= 'Z' ? character - 'A' + 'a' : character)
                        {
                            case 'a':  anchor = 0; moveCaret ((int) serial.size(), true); return true;
                            case 'c':  copySelection (false); return true;
                            case 'x':  copySelection (true); return true;
                            case 'v':  paste(); return true;
                            default:   return false;   // the host's shortcuts stay the host's
                        }
                    }

                    if (character >= 0x20 && character != 0x7f)
                    {
                        std::string typed;

                        // As UTF-8 (insert keeps what a serial has).
                        if (character < 0x80)
                            typed += (char) character;
                        else if (character < 0x800)
                            typed += { (char) (0xc0 | (character >> 6)), (char) (0x80 | (character & 0x3f)) };
                        else if (character < 0x10000)
                            typed += { (char) (0xe0 | (character >> 12)), (char) (0x80 | ((character >> 6) & 0x3f)), (char) (0x80 | (character & 0x3f)) };

                        insert (typed);
                    }

                    return true;

                case Key::backspace:
                    if (! eraseSelection() && caret > 0)
                    {
                        const auto from = command ? 0 : caret - 1;
                        serial.erase ((size_t) from, (size_t) (caret - from));
                        moveCaret (from, false);
                    }

                    return true;

                case Key::forwardDelete:
                    if (! eraseSelection() && caret < (int) serial.size())
                    {
                        serial.erase ((size_t) caret, command ? std::string::npos : (size_t) 1);
                        moveCaret (caret, false);
                    }

                    return true;

                case Key::left:
                    if (caret != anchor && ! extend)
                        moveCaret (selectionStart(), false);
                    else
                        moveCaret (command ? 0 : caret - 1, extend);

                    return true;

                case Key::right:
                    if (caret != anchor && ! extend)
                        moveCaret (selectionEnd(), false);
                    else
                        moveCaret (command ? (int) serial.size() : caret + 1, extend);

                    return true;

                case Key::home:    moveCaret (0, extend); return true;
                case Key::end:     moveCaret ((int) serial.size(), extend); return true;
                case Key::enter:   submit(); return true;
                case Key::escape:  editor.closeRegistration(); return true;
                case Key::tab:     return true;
            }

            return false;
        }

        //======================================================================
        // Its look.
        void drawButton (Painter& p, const Rect& r, const std::string& label, int part, bool primary, std::uint32_t labelColour = bright)
        {
            const auto s = r.h / 38.0f;
            const bool over = hovered == part, down = pressed == part && over;
            const auto radius = 8.0f * s;

            if (primary)
            {
                const auto top = down ? accentDark : (over ? mix (accent, 0xffffffffu, 0.1f) : accent);
                const auto bottom = down ? mix (accentDark, 0xff000000u, 0.1f) : (over ? accent : accentDark);
                p.fill (r, radius, top, bottom);
                p.outline (r, radius, 1.0f, 0xffffffffu, 0.14f);
            }
            else
            {
                const auto face = down ? buttonDown : (over ? buttonHover : button);
                p.fill (r, radius, mix (face, 0xffffffffu, 0.03f), face);
                p.outline (r, radius, 1.0f, 0xffffffffu, over ? 0.13f : 0.08f);
            }

            const auto size = 13.5f * s;
            const auto shown = text.fitted (label, size, true, r.w - 12.0f * s);
            const auto w = text.width (shown, size, true);
            text.draw (p, shown, std::round (r.x + (r.w - w) * 0.5f), Text::baselineFor (r.centreY() + (down ? 0.5f : 0.0f), size), size, true, labelColour);
        }

        void drawBox (Painter& p, const Rect& r, float radius, std::uint32_t back, float edge)
        {
            p.fill (r, radius, back);
            p.outline (r, radius, 1.0f, 0xffffffffu, edge);
        }

        void drawTab (Painter& p)
        {
            const auto r = tab();
            const auto ts = r.h / 24.0f, size = 11.5f * ts;
            p.shadow (r.expanded (-1.0f), r.h * 0.5f, 6.0f * ts, 0.35f);
            p.fill (r, r.h * 0.5f, tabHovered ? 0xff2a2d36 : 0xff1d1f25, tabHovered ? 0xff23262e : 0xff17191e, 0.96f);
            p.outline (r, r.h * 0.5f, 1.0f, warn, tabHovered ? 0.75f : 0.5f);

            Canvas canvas { p.pixels, p.width, p.height };
            canvas.circle (r.x + 13.0f * ts, r.centreY(), 3.2f * ts, warn, 1.0f);
            text.draw (p, tabLabel, r.x + 22.0f * ts, Text::baselineFor (r.centreY(), size), size, true, bright);
        }

        void drawStatus (Painter& p, const Rect& row, bool registered, float s)
        {
            const std::string label = registered ? "Registered" : "Demo";
            const auto size = 11.5f * s;
            const auto colour = registered ? good : warn;
            const auto w = text.width (label, size, true) + 26.0f * s, h = 22.0f * s;
            const Rect pill { std::round (row.right() - w - 10.0f * s), std::round (row.centreY() - h * 0.5f), std::round (w), std::round (h) };
            p.fill (pill, h * 0.5f, colour, 0.13f);
            p.outline (pill, h * 0.5f, 1.0f, colour, 0.35f);

            Canvas canvas { p.pixels, p.width, p.height };
            canvas.circle (pill.x + 11.0f * s, pill.centreY(), 2.8f * s, colour, 1.0f);
            text.draw (p, label, pill.x + 18.0f * s, Text::baselineFor (pill.centreY(), size), size, true, colour);
        }

        /** A small round mark before the result: a tick, or an exclamation mark. */
        void drawMark (Painter& p, float cx, float cy, float s, bool fine)
        {
            Canvas canvas { p.pixels, p.width, p.height };
            const auto r = 8.0f * s;
            canvas.circle (cx, cy, r, fine ? good : bad, 1.0f);

            if (fine)
            {
                canvas.line (cx - 3.6f * s, cy + 0.2f * s, cx - 1.0f * s, cy + 2.8f * s, 1.9f * s, panelBottom, 1.0f);
                canvas.line (cx - 1.0f * s, cy + 2.8f * s, cx + 3.9f * s, cy - 2.6f * s, 1.9f * s, panelBottom, 1.0f);
            }
            else
            {
                canvas.line (cx, cy - 4.0f * s, cx, cy + 0.9f * s, 2.0f * s, panelBottom, 1.0f);
                canvas.circle (cx, cy + 3.7f * s, 1.15f * s, panelBottom, 1.0f);
            }
        }

        void draw (Painter& p)
        {
            const auto l = layout();
            const auto s = l.s;
            const auto now = Clock::now();

            // The panel behind, dimmed; the window over it.
            p.wash (0xff000000u, 0.58f);
            p.shadow (Rect { l.panel.x, l.panel.y + 8.0f * s, l.panel.w, l.panel.h }, 14.0f * s, 28.0f * s, 0.5f);
            p.fill (l.panel, 14.0f * s, panelTop, panelBottom);
            p.outline (l.panel, 14.0f * s, 1.0f, 0xffffffffu, 0.09f);

            const auto x = l.list.x, inner = l.list.w;

            // The title and what it's about.
            const auto title = items.empty() ? std::string ("Registration") : "Register " + items.front().name;
            text.draw (p, text.fitted (title, 19.0f * s, true, inner), x, l.titleY, 19.0f * s, true, bright);

            if (! l.compact)
            {
                const std::string about = anyUnregistered() ? "Unregistered, it runs as a demo with short silent pauses."
                                                            : "Everything here is registered. Thank you!";
                text.draw (p, text.fitted (about, 12.5f * s, false, inner), x, l.subtitleY, 12.5f * s, false, muted);
            }

            // Each product and its state.
            drawBox (p, l.list, 10.0f * s, inset, 0.06f);
            const auto rowHeight = 30.0f * s;

            for (int i = 0; i < l.shown + (l.more > 0 ? 1 : 0); ++i)
            {
                const Rect row { l.list.x, l.list.y + 4.0f * s + rowHeight * (float) i, l.list.w, rowHeight };

                if (i > 0)
                    p.fill (Rect { row.x + 12.0f * s, std::round (row.y), row.w - 24.0f * s, 1.0f }, 0.0f, 0xffffffffu, 0.05f);

                if (i >= l.shown)
                {
                    text.draw (p, "and " + std::to_string (l.more) + " more", row.x + 14.0f * s, Text::baselineFor (row.centreY(), 12.5f * s),
                               12.5f * s, false, muted);
                    continue;
                }

                const auto& item = items[(size_t) i];
                const auto name = text.fitted (item.name.empty() ? std::string ("This plugin") : item.name, 13.5f * s, false, row.w - 130.0f * s);
                text.draw (p, name, row.x + 14.0f * s, Text::baselineFor (row.centreY(), 13.5f * s), 13.5f * s, false, body);
                drawStatus (p, row, item.registered, s);
            }

            // The computer's ID.
            text.draw (p, "YOUR COMPUTER ID", x, l.idLabelY, 10.5f * s, true, muted, 1.0f, 0.9f * s);
            drawBox (p, l.idBox, 8.0f * s, fieldBack, 0.08f);

            {
                const auto size = 14.5f * s;
                const auto shown = computerId.empty() ? std::string ("Unavailable") : computerId;
                p.clipTo (l.idBox.expanded (-2.0f));
                text.draw (p, text.fitted (shown, size, true, l.idBox.w - 24.0f * s, 0.4f * s), l.idBox.x + 12.0f * s,
                           Text::baselineFor (l.idBox.centreY(), size), size, true, computerId.empty() ? faint : bright, 1.0f, 0.4f * s);
                p.unclip();
            }

            const bool justCopied = copiedAt != Clock::time_point() && now - copiedAt < std::chrono::milliseconds (1600);
            drawButton (p, l.copy, justCopied ? "Copied" : "Copy ID", Part::copyPart, false, justCopied ? good : bright);

            if (! l.compact)
                text.draw (p, text.fitted ("Send this ID to the seller to get your serial.", 12.0f * s, false, inner), x, l.hintY, 12.0f * s, false, faint);

            // The serial.
            text.draw (p, "SERIAL", x, l.serialLabelY, 10.5f * s, true, muted, 1.0f, 0.9f * s);
            drawField (p, l, now);
            drawButton (p, l.paste, "Paste", Part::pastePart, false);

            // What came of it.
            if (! result.empty())
            {
                const auto size = 13.0f * s;
                const auto colour = tone > 0 ? good : (tone < 0 ? bad : body);
                const auto indent = tone != 0 ? 26.0f * s : 0.0f;
                const auto lines = text.wrapped (result, size, tone > 0, inner - indent, 2);
                const auto lineHeight = 17.0f * s;
                const auto top = l.result.centreY() - lineHeight * (float) lines.size() * 0.5f;

                if (tone != 0)
                    drawMark (p, x + 9.0f * s, top + lineHeight * 0.5f, s, tone > 0);

                for (size_t i = 0; i < lines.size(); ++i)
                    text.draw (p, lines[i], x + indent, Text::baselineFor (top + lineHeight * ((float) i + 0.5f), size), size, tone > 0, colour);
            }

            drawButton (p, l.close, "Close", Part::closePart, false);
            drawButton (p, l.submit, "Register", Part::submitPart, true, 0xffffffffu);
        }

        void drawField (Painter& p, const RegistrationLayout& l, Clock::time_point now)
        {
            const auto s = l.s;
            const auto& f = l.field;
            p.outline (f.expanded (3.0f * s), 10.0f * s, 3.0f * s, accent, 0.16f);
            p.fill (f, 8.0f * s, fieldBack);
            p.outline (f, 8.0f * s, 1.5f, accent, 0.9f);

            const auto size = 14.5f * s, tracking = 0.4f * s;
            const auto left = f.x + 12.0f * s, room = f.w - 24.0f * s;

            // Scrolled so the caret is in view.
            const auto caretX = text.width (serial.substr (0, (size_t) caret), size, true, tracking);
            const auto whole = text.width (serial, size, true, tracking);

            if (whole <= room)                 scroll = 0.0f;
            else if (caretX - scroll > room)   scroll = caretX - room;
            else if (caretX < scroll)          scroll = caretX;

            scroll = std::max (0.0f, std::min (scroll, std::max (0.0f, whole - room)));

            const auto baseline = Text::baselineFor (f.centreY(), size);
            p.clipTo (Rect { left - 2.0f * s, f.y + 2.0f, room + 4.0f * s, f.h - 4.0f });

            if (serial.empty())
            {
                text.draw (p, text.fitted ("00000-00000", size, true, room, tracking), left, baseline, size, true, faint, 0.45f, tracking);
            }
            else
            {
                if (caret != anchor)
                {
                    const auto a = text.width (serial.substr (0, (size_t) selectionStart()), size, true, tracking);
                    const auto b = text.width (serial.substr (0, (size_t) selectionEnd()), size, true, tracking);
                    p.fill (Rect { left + a - scroll - 1.0f, f.y + 8.0f * s, b - a + 2.0f, f.h - 16.0f * s }, 3.0f * s, accent, 0.38f);
                }

                text.draw (p, serial, left - scroll, baseline, size, true, bright, 1.0f, tracking);
            }

            // The caret, blinking (steady while it moves).
            const auto since = std::chrono::duration_cast<std::chrono::milliseconds> (now - caretMoved).count();

            if (since % 1060 < 600)
            {
                const auto cx = std::round (left + caretX - scroll - (caret > 0 ? tracking * 0.5f : 1.0f));
                p.fill (Rect { cx, f.y + 9.0f * s, std::max (1.5f, 1.5f * s), f.h - 18.0f * s }, 0.0f, 0xffffffffu, 0.95f);
            }

            p.unclip();
        }

        //======================================================================
        Editor& editor;
        bool open = false, hasBadge = false, tabHovered = false;
        std::vector<LicenceItem> items;
        std::string computerId;
        Clock::time_point checked {};

        static constexpr int maxSerial = 16;   // 10 digits, with room for dashes
        const std::string tabLabel = "Demo \xc2\xb7 Register";
        std::string serial;
        int caret = 0, anchor = 0;
        float scroll = 0.0f;
        bool selecting = false;
        float mouseX = 0.0f, mouseY = 0.0f;

        std::string result;
        int tone = 0;   // the result: 1 good, -1 not, 0 news
        int hovered = Part::none, pressed = Part::none;
        Clock::time_point caretMoved {}, copiedAt {};   // copiedAt: when Copy ID was last clicked (none yet: its epoch)

        Key lastKey = Key::tab;
        std::uint32_t lastCharacter = 0;
        int lastModifiers = 0;
        bool lastFromHost = false;
        Clock::time_point lastKeyAt {};

        Text text;
    };
   #else
    struct Editor::Registration {};
   #endif

    //==========================================================================
    Editor::Image Editor::decode (const unsigned char* png, int size)
    {
        Image image;

        if (png == nullptr || size <= 0)
            return image;

        int w = 0, h = 0, channels = 0;
        auto* rgba = stbi_load_from_memory (png, size, &w, &h, &channels, 4);

        if (rgba == nullptr)
            return image;

        image.width = w;
        image.height = h;
        image.pixels.resize ((size_t) w * (size_t) h);

        for (size_t i = 0; i < image.pixels.size(); ++i)
        {
            const auto* p = rgba + i * 4;
            const auto a = (std::uint32_t) p[3];
            const auto r = (std::uint32_t) p[0] * a / 255, g = (std::uint32_t) p[1] * a / 255, b = (std::uint32_t) p[2] * a / 255;
            image.pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
        }

        stbi_image_free (rgba);
        return image;
    }

    Editor::Editor (Host& h)
        : host (h)
    {
        for (int i = 0; i < numPages; ++i)
            backgrounds.push_back (decode (pages[i].data, pages[i].size));

        for (int i = 0; i < numStrips; ++i)
            frames.push_back (decode (strips[i].png, strips[i].size));

        for (int i = 0; i < numWidgets; ++i)
        {
            const auto& w = widgets[i];
            Bound b;
            b.param = w.param != nullptr ? host.findParam (w.param) : -1;
            b.param2 = w.param2 != nullptr ? host.findParam (w.param2) : -1;
            b.param3 = w.param3 != nullptr ? host.findParam (w.param3) : -1;
            b.param4 = w.param4 != nullptr ? host.findParam (w.param4) : -1;
            bound.push_back (b);
        }

        meterLevels.assign ((size_t) numWidgets, 0.0f);
        meterNeedles.assign ((size_t) numWidgets, 0.0f);
        scopeBuffer.assign (2048, 0.0f);

       #if STELLA_ELEMENTS
        if (numCustomElements > 0)
            elements = std::make_unique<Elements> (*this);
       #endif

       #if STELLA_REGISTRATION
        registration = std::make_unique<Registration> (*this);
       #endif

        scale = snapScale (host.windowScale());   // where the last window was left
    }

    Editor::~Editor()
    {
        releaseKey();
    }

    //==========================================================================
    float Editor::proportionOf (int param) const
    {
        if (param < 0)
            return 0.0f;

        float min = 0.0f, max = 1.0f, def = 0.0f, skew = 1.0f;
        host.getRange (param, min, max, def, skew);
        const auto t = std::min (1.0f, std::max (0.0f, (host.getValue (param) - min) / std::max (1.0e-9f, max - min)));
        return std::abs (skew - 1.0f) > 1.0e-4f && skew > 0.0f ? std::pow (t, skew) : t;
    }

    float Editor::valueAt (int param, float proportion) const
    {
        float min = 0.0f, max = 1.0f, def = 0.0f, skew = 1.0f;
        host.getRange (param, min, max, def, skew);
        auto t = std::min (1.0f, std::max (0.0f, proportion));

        if (std::abs (skew - 1.0f) > 1.0e-4f && skew > 0.0f)
            t = std::pow (t, 1.0f / skew);

        return min + (max - min) * t;
    }

    bool Editor::isShown (int index) const noexcept
    {
        return index >= 0 && index < numWidgets && (widgets[index].page < 0 || widgets[index].page == page);
    }

    void Editor::boxPoint (int index, int x, int y, float& ux, float& uy) const
    {
        // Played in its own frame: a turned element as it's turned.
        const auto& w = widgets[index];
        const auto px = (float) x + 0.5f, py = (float) y + 0.5f;

        if (w.angle == 0.0f)
        {
            ux = px;
            uy = py;
            return;
        }

        const auto cx = (float) w.bx + (float) w.bw * 0.5f, cy = (float) w.by + (float) w.bh * 0.5f;
        const auto radians = -w.angle * 3.14159265f / 180.0f;
        const auto c = std::cos (radians), s = std::sin (radians);
        ux = cx + (px - cx) * c - (py - cy) * s;
        uy = cy + (px - cx) * s + (py - cy) * c;
    }

    int Editor::widgetAt (int x, int y) const
    {
        for (int i = numWidgets; --i >= 0;)
        {
            const auto& w = widgets[i];
            const bool interactive = w.kind == Kind::knob || w.kind == Kind::slider || w.kind == Kind::toggle
                                  || w.kind == Kind::selector || w.kind == Kind::xy || w.kind == Kind::preset
                                  || w.kind == Kind::keyboard || w.kind == Kind::custom || w.kind == Kind::tabs
                                  || w.kind == Kind::registration || w.kind == Kind::key || w.kind == Kind::perform;

            if (! interactive || ! isShown (i))
                continue;

            float ux = 0.0f, uy = 0.0f;
            boxPoint (i, x, y, ux, uy);

            if (ux >= (float) w.bx && uy >= (float) w.by && ux < (float) (w.bx + w.bw) && uy < (float) (w.by + w.bh))
                return i;
        }

        return -1;
    }

    void Editor::setFromPointer (int index, int x, int y)
    {
        const auto& w = widgets[index];
        const auto& b = bound[(size_t) index];
        float ux = 0.0f, uy = 0.0f;
        boxPoint (index, x, y, ux, uy);

        if (w.kind == Kind::slider && b.param >= 0)
        {
            const auto t = w.vertical ? ((float) (w.by + w.bh - 4) - uy) / (float) std::max (1, w.bh - 8)
                                      : (ux - (float) (w.bx + 4)) / (float) std::max (1, w.bw - 8);
            host.setValue (b.param, valueAt (b.param, t));
        }
        else if (w.kind == Kind::xy)
        {
            if (b.param >= 0)  host.setValue (b.param, valueAt (b.param, (ux - (float) (w.bx + 2)) / (float) std::max (1, w.bw - 4)));
            if (b.param2 >= 0) host.setValue (b.param2, valueAt (b.param2, ((float) (w.by + w.bh - 2) - uy) / (float) std::max (1, w.bh - 4)));
        }
        else if (w.kind == Kind::perform)
        {
            // A pair of wheels: the one held, up and down; one way: along it; else both.
            const auto across = (ux - (float) w.bx) / (float) std::max (1, w.bw), up = ((float) (w.by + w.bh) - uy) / (float) std::max (1, w.bh);
            const bool pair = (w.vertical & 1) != 0;

            if (pair)
            {
                const int param = performPart == 0 ? b.param : b.param2;

                if (param >= 0)
                    host.setValue (param, valueAt (param, up));
            }
            else
            {
                if (b.param >= 0 && w.count > 1)  host.setValue (b.param, valueAt (b.param, across));
                if (b.param2 >= 0 && w.mode > 1)  host.setValue (b.param2, valueAt (b.param2, up));
            }
        }
    }

    void Editor::showPage (int index)
    {
        if (index < 0 || index >= std::max (1, numPages) || index == page)
            return;

        releaseKey();

       #if STELLA_ELEMENTS
        // What's open on the page that goes closes, and the mouse leaves it.
        if (elements != nullptr)
        {
            elements->closePopups();

            if (elements->hovered >= 0)
                elements->mouse (elements->hovered, elements->hoveredPopup, ui::ElementRuntime::exit, elements->lastX, elements->lastY, false);

            elements->hovered = elements->pressed = -1;
        }
       #endif

        page = index;
    }

    void Editor::mouseDown (int x, int y, bool fine, bool doubleClick)
    {
        // The size menu, open, takes the click (outside it: it closes).
        sizeMenuTookPress = sizeMenuMouse (x, y, true);

        if (sizeMenuTookPress)
            return;

        x = toBaked (x);
        y = toBaked (y);

        // Registration, while it's open, takes every click; its tab, when it shows one.
        if (registrationMouseDown (x, y, fine, doubleClick))
            return;

       #if STELLA_ELEMENTS
        // A programmed element's open popup takes the click; a click anywhere else closes it,
        // and goes no further.
        if (elements != nullptr)
        {
            elements->pressed = -1;

            if (const auto owner = elements->popupAt (x, y); owner >= 0)
            {
                elements->pressed = owner;
                elements->pressedPopup = true;
                elements->mouse (owner, true, ui::ElementRuntime::down, x, y, fine, doubleClick);
                return;
            }

            if (elements->closePopups())
                return;
        }
       #endif

        const auto index = widgetAt (x, y);

        if (index < 0)
            return;

        const auto& w = widgets[index];
        const auto& b = bound[(size_t) index];
        float ux = 0.0f, uy = 0.0f;
        boxPoint (index, x, y, ux, uy);

        if (doubleClick && (w.kind == Kind::knob || w.kind == Kind::slider) && b.param >= 0)
        {
            float min = 0.0f, max = 1.0f, def = 0.0f, skew = 1.0f;
            host.getRange (b.param, min, max, def, skew);
            host.beginEdit (b.param);
            host.setValue (b.param, def);
            host.endEdit (b.param);
            return;
        }

        switch (w.kind)
        {
            case Kind::knob:
                if (b.param < 0) return;
                active = index;
                dragX = x;
                dragY = y;
                startProportion = proportionOf (b.param);
                host.beginEdit (b.param);
                break;

            case Kind::slider:
                if (b.param < 0) return;
                active = index;
                host.beginEdit (b.param);
                setFromPointer (index, x, y);
                break;

            case Kind::xy:
                active = index;
                if (b.param >= 0)  host.beginEdit (b.param);
                if (b.param2 >= 0) host.beginEdit (b.param2);
                setFromPointer (index, x, y);
                break;

            case Kind::toggle:
            {
                if (b.param < 0) return;
                float min = 0.0f, max = 1.0f, def = 0.0f, skew = 1.0f;
                host.getRange (b.param, min, max, def, skew);
                host.beginEdit (b.param);
                host.setValue (b.param, proportionOf (b.param) >= 0.5f ? min : max);
                host.endEdit (b.param);
                break;
            }

            case Kind::selector:
            {
                if (b.param < 0) return;
                float min = 0.0f, max = 1.0f, def = 0.0f, skew = 1.0f;
                host.getRange (b.param, min, max, def, skew);
                const auto count = std::max (1, w.count);
                const auto chosen = std::min (count - 1, std::max (0, (int) std::lround (host.getValue (b.param) - min)));
                int cell = 0;

                if (w.mode == 2)
                {
                    // Arrows: the left one steps back, anywhere else steps on (round past the ends).
                    const auto along = (ux - (float) w.bx) / (float) std::max (1, w.bw);
                    cell = along < 0.35f ? (chosen + count - 1) % count : (chosen + 1) % count;
                }
                else
                {
                    const auto along = w.mode == 1 ? (uy - (float) w.by) / (float) std::max (1, w.bh) : (ux - (float) w.bx) / (float) std::max (1, w.bw);
                    cell = std::min (count - 1, std::max (0, (int) std::floor (along * (float) count)));
                }

                host.beginEdit (b.param);
                host.setValue (b.param, min + (float) cell);
                host.endEdit (b.param);
                break;
            }

            case Kind::preset:
            {
                // The arrows step back and forth; the middle steps on.
                stepPreset ((ux - (float) w.bx) < (float) w.bw * 0.22f ? -1 : 1);
                break;
            }

            case Kind::tabs:
            {
                const auto count = std::max (1, w.count);
                showPage (std::min (count - 1, std::max (0, (int) ((ux - (float) w.bx) / ((float) std::max (1, w.bw) / (float) count)))));
                break;
            }

            case Kind::keyboard:
                active = index;
                playKey (index, x, y);
                break;

            case Kind::custom:
               #if STELLA_ELEMENTS
                // It takes the drag and the release only if it asks for them.
                if (elements != nullptr && elements->mouse (index, false, ui::ElementRuntime::down, x, y, fine, doubleClick))
                {
                    elements->pressed = index;
                    elements->pressedPopup = false;
                }
               #else
                (void) fine;
               #endif
                break;

            case Kind::registration:
                openRegistration();
                break;

            case Kind::key:
            {
                // One at a time: this one; by itself: on or off; several: its bit.
                if (b.param < 0) return;
                float min = 0.0f, max = 1.0f, def = 0.0f, skew = 1.0f;
                host.getRange (b.param, min, max, def, skew);
                host.beginEdit (b.param);

                if (w.mode == 1)
                {
                    host.setValue (b.param, proportionOf (b.param) >= 0.5f ? min : max);
                }
                else if (w.mode == 2)
                {
                    const auto bits = (int) std::lround (host.getValue (b.param) - min) ^ (1 << std::max (0, std::min (30, w.count)));
                    host.setValue (b.param, std::min (max, min + (float) bits));
                }
                else
                {
                    host.setValue (b.param, std::min (max, min + (float) w.count));
                }

                host.endEdit (b.param);
                break;
            }

            case Kind::perform:
                active = index;
                performPart = (w.vertical & 1) != 0 && ux >= (float) w.bx + (float) w.bw * 0.5f ? 1 : 0;
                if (b.param >= 0)  host.beginEdit (b.param);
                if (b.param2 >= 0) host.beginEdit (b.param2);
                setFromPointer (index, x, y);
                break;

            case Kind::meter: case Kind::lamp: case Kind::scope: case Kind::envelope: case Kind::filter: case Kind::picture:
            case Kind::readout:
                break;
        }
    }

    void Editor::playKey (int index, int x, int y)
    {
        const auto& w = widgets[index];
        float px = 0.0f, py = 0.0f;
        boxPoint (index, x, y, px, py);
        const auto note = keys::noteAt (px, py, w.mode, w.count, (float) w.bx, (float) w.by, (float) w.bw, (float) w.bh);

        if (note == heldNote && index == heldKeyboard)
            return;

        releaseKey();

        if (note < 0)
            return;

        heldNote = note;
        heldKeyboard = index;
        host.playNote (note, keys::velocityAt (py, keys::keyOf (note, w.mode, w.count, (float) w.bx, (float) w.by, (float) w.bw, (float) w.bh)));
    }

    void Editor::releaseKey()
    {
        if (heldNote < 0)
            return;

        const auto note = heldNote;
        heldNote = heldKeyboard = -1;
        host.playNote (note, 0.0f);
    }

    void Editor::applyPreset (int index)
    {
        if (index < 0 || index >= numAllPresets())
            return;

        currentPreset = index;
        ++presetVersion;

        auto put = [this] (const char* id, float value)
        {
            const auto param = id != nullptr ? host.findParam (id) : -1;

            if (param >= 0)
            {
                host.beginEdit (param);
                host.setValue (param, value);
                host.endEdit (param);
            }
        };

        if (index < numPresets)
        {
            const auto& preset = presets[index];

            for (int i = 0; i < preset.numValues; ++i)
                put (preset.values[i].param, preset.values[i].value);
        }
        else
        {
            for (const auto& [id, value] : userPresets[(size_t) (index - numPresets)].values)
                put (id.c_str(), value);
        }

        keepChosenValues();
    }

    //==========================================================================
    // The user's presets: files in Documents/<vendor>/<name> Presets ("My presets"), and a
    // folder for each bank they loaded (a group of its own).
    namespace
    {
        namespace files
        {
           #if defined (_WIN32)
            std::wstring wide (const std::string& s)
            {
                std::wstring w;
                size_t i = 0;

                while (i < s.size())
                {
                    const auto c = (unsigned char) s[i];
                    std::uint32_t code = c;
                    int more = 0;

                    if (c >= 0xf0)       { code = c & 0x07u; more = 3; }
                    else if (c >= 0xe0)  { code = c & 0x0fu; more = 2; }
                    else if (c >= 0xc0)  { code = c & 0x1fu; more = 1; }

                    ++i;

                    for (; more > 0 && i < s.size(); --more, ++i)
                        code = (code << 6) | ((unsigned char) s[i] & 0x3fu);

                    if (code >= 0x10000u)
                    {
                        code -= 0x10000u;
                        w += (wchar_t) (0xd800u + (code >> 10));
                        w += (wchar_t) (0xdc00u + (code & 0x3ffu));
                    }
                    else
                    {
                        w += (wchar_t) (code == '/' ? '\\' : code);
                    }
                }

                return w;
            }

            std::string narrow (const wchar_t* w)
            {
                std::string s;

                for (; *w != 0; ++w)
                {
                    std::uint32_t code = (std::uint32_t) *w;

                    if (code >= 0xd800u && code < 0xdc00u && w[1] >= 0xdc00 && w[1] < 0xe000)
                        code = 0x10000u + ((code - 0xd800u) << 10) + ((std::uint32_t) *++w - 0xdc00u);

                    if (code < 0x80u)          s += (char) code;
                    else if (code < 0x800u)    { s += (char) (0xc0u | (code >> 6)); s += (char) (0x80u | (code & 0x3fu)); }
                    else if (code < 0x10000u)  { s += (char) (0xe0u | (code >> 12)); s += (char) (0x80u | ((code >> 6) & 0x3fu)); s += (char) (0x80u | (code & 0x3fu)); }
                    else                       { s += (char) (0xf0u | (code >> 18)); s += (char) (0x80u | ((code >> 12) & 0x3fu));
                                                 s += (char) (0x80u | ((code >> 6) & 0x3fu)); s += (char) (0x80u | (code & 0x3fu)); }
                }

                return s;
            }

            FILE* open (const std::string& path, bool writing)    { return _wfopen (wide (path).c_str(), writing ? L"wb" : L"rb"); }
            bool makeOne (const std::string& path)                 { return _wmkdir (wide (path).c_str()) == 0; }
            bool remove (const std::string& path)                  { return _wremove (wide (path).c_str()) == 0; }

            /** What's in a folder: its files (or its folders). */
            std::vector<std::string> list (const std::string& folder, bool folders)
            {
                std::vector<std::string> names;
                _wfinddata_t found {};
                const auto handle = _wfindfirst (wide (folder + "/*").c_str(), &found);

                if (handle == -1)
                    return names;

                do
                {
                    const auto name = narrow (found.name);

                    if (name != "." && name != ".." && ((found.attrib & _A_SUBDIR) != 0) == folders)
                        names.push_back (name);
                }
                while (_wfindnext (handle, &found) == 0);

                _findclose (handle);
                return names;
            }
           #else
            FILE* open (const std::string& path, bool writing)    { return std::fopen (path.c_str(), writing ? "wb" : "rb"); }
            bool makeOne (const std::string& path)                 { return ::mkdir (path.c_str(), 0755) == 0; }
            bool remove (const std::string& path)                  { return std::remove (path.c_str()) == 0; }

            std::vector<std::string> list (const std::string& folder, bool folders)
            {
                std::vector<std::string> names;

                if (auto* dir = ::opendir (folder.c_str()))
                {
                    while (const auto* entry = ::readdir (dir))
                    {
                        const std::string name = entry->d_name;
                        struct stat info {};

                        if (name != "." && name != ".." && ::stat ((folder + "/" + name).c_str(), &info) == 0
                            && S_ISDIR (info.st_mode) == folders)
                            names.push_back (name);
                    }

                    ::closedir (dir);
                }

                return names;
            }
           #endif

            /** A folder and those it's in, made where they're missing. */
            void make (const std::string& path)
            {
                for (size_t at = 1; at <= path.size(); ++at)
                    if (at == path.size() || path[at] == '/' || path[at] == '\\')
                        if (at > 0 && path[at - 1] != ':')
                            makeOne (path.substr (0, at));
            }

            bool read (const std::string& path, std::string& text)
            {
                auto* f = open (path, false);

                if (f == nullptr)
                    return false;

                text.clear();
                char buffer[4096];

                for (size_t n; (n = std::fread (buffer, 1, sizeof (buffer), f)) > 0;)
                    text.append (buffer, n);

                std::fclose (f);
                return true;
            }

            bool write (const std::string& path, const std::string& text)
            {
                auto* f = open (path, true);

                if (f == nullptr)
                    return false;

                const bool ok = std::fwrite (text.data(), 1, text.size(), f) == text.size();
                return std::fclose (f) == 0 && ok;
            }

            bool endsWith (const std::string& s, const std::string& end)
            {
                if (s.size() < end.size())
                    return false;

                for (size_t i = 0; i < end.size(); ++i)
                    if (std::tolower ((unsigned char) s[s.size() - end.size() + i]) != std::tolower ((unsigned char) end[i]))
                        return false;

                return true;
            }

            /** A file's name without its folder or its extension. */
            std::string stem (const std::string& path)
            {
                const auto slash = path.find_last_of ("/\\");
                auto name = slash == std::string::npos ? path : path.substr (slash + 1);
                const auto dot = name.find_last_of ('.');
                return dot == std::string::npos || dot == 0 ? name : name.substr (0, dot);
            }

            /** A name that can be a file's (no slashes, colons...). */
            std::string safe (const std::string& name)
            {
                std::string s;

                for (const auto c : name)
                    s += std::strchr ("/\\:*?\"<>|", c) != nullptr || (unsigned char) c < 32 ? '_' : c;

                while (! s.empty() && (s.back() == ' ' || s.back() == '.'))
                    s.pop_back();

                return s.empty() ? std::string ("Untitled") : s;
            }

            bool exists (const std::string& path)
            {
                auto* f = open (path, false);

                if (f != nullptr)
                    std::fclose (f);

                return f != nullptr;
            }
        }

        constexpr const char* presetExtension = "preset";
        constexpr const char* bankExtension = "presetbank";

        std::string trimmed (std::string s)
        {
            while (! s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t'))
                s.pop_back();

            size_t start = 0;

            while (start < s.size() && (s[start] == ' ' || s[start] == '\t'))
                ++start;

            return s.substr (start);
        }

        /** A preset file's (or a bank's) presets: their names and values, by parameter id. */
        struct ReadPreset
        {
            std::string name;
            std::vector<std::pair<std::string, float>> values;
        };

        std::vector<ReadPreset> readPresets (const std::string& text, const std::string& fallbackName)
        {
            std::vector<ReadPreset> found;
            size_t at = 0;
            bool first = true, bank = false;

            while (at <= text.size())
            {
                auto end = text.find ('\n', at);

                if (end == std::string::npos)
                    end = text.size();

                const auto line = trimmed (text.substr (at, end - at));
                at = end + 1;

                if (first)
                {
                    first = false;

                    if (line.rfind ("stella-bank", 0) == 0)
                    {
                        bank = true;
                        continue;
                    }

                    if (line.rfind ("stella-preset", 0) == 0)
                    {
                        found.push_back ({ fallbackName, {} });
                        continue;
                    }

                    return found;   // not one of ours
                }

                const auto equals = line.find ('=');

                if (line.empty() || line[0] == '#' || equals == std::string::npos)
                    continue;

                const auto key = trimmed (line.substr (0, equals)), value = trimmed (line.substr (equals + 1));

                if (key == "preset" && bank)
                {
                    found.push_back ({ value.empty() ? fallbackName : value, {} });
                    continue;
                }

                if (found.empty())
                    continue;

                if (key == "name")
                {
                    if (! value.empty())
                        found.back().name = value;
                    continue;
                }

                char* stop = nullptr;
                const auto number = std::strtof (value.c_str(), &stop);

                if (stop != value.c_str() && std::isfinite (number))
                    found.back().values.push_back ({ key, number });
            }

            return found;
        }

        std::string numberText (float value)
        {
            char buffer[48];
            std::snprintf (buffer, sizeof (buffer), "%.9g", (double) value);
            return buffer;
        }
    }

    std::string Editor::presetFolder()
    {
        if (desktop == nullptr)
            return {};

        auto documents = desktop->documentsFolder();

        if (documents.empty())
            return {};

        while (documents.size() > 1 && (documents.back() == '/' || documents.back() == '\\'))
            documents.pop_back();

        return documents + "/" + files::safe (host.vendorName()) + "/" + files::safe (host.pluginName()) + " Presets";
    }

    void Editor::findUserPresets()
    {
        presetsFound = true;
        const std::string chosenFile = currentPreset >= numPresets && currentPreset - numPresets < (int) userPresets.size()
                                         ? userPresets[(size_t) (currentPreset - numPresets)].file : std::string();
        userPresets.clear();
        const auto folder = presetFolder();

        if (folder.empty())
            return;

        auto readFolder = [this] (const std::string& path, const std::string& group)
        {
            auto names = files::list (path, false);
            std::sort (names.begin(), names.end(), [] (const std::string& a, const std::string& b)
            {
                for (size_t i = 0; i < a.size() && i < b.size(); ++i)
                    if (std::tolower ((unsigned char) a[i]) != std::tolower ((unsigned char) b[i]))
                        return std::tolower ((unsigned char) a[i]) < std::tolower ((unsigned char) b[i]);

                return a.size() < b.size();
            });

            for (const auto& name : names)
            {
                std::string text;

                if (! files::endsWith (name, std::string (".") + presetExtension) || ! files::read (path + "/" + name, text))
                    continue;

                const auto read = readPresets (text, files::stem (name));

                if (! read.empty())
                    userPresets.push_back ({ read.front().name, group, path + "/" + name, read.front().values });
            }
        };

        readFolder (folder, "My presets");
        auto banks = files::list (folder, true);
        std::sort (banks.begin(), banks.end());

        for (const auto& bank : banks)
            readFolder (folder + "/" + bank, bank);

        // The chosen preset where it is now (or none, when its file went).
        if (! chosenFile.empty())
        {
            currentPreset = -1;

            for (size_t i = 0; i < userPresets.size(); ++i)
                if (userPresets[i].file == chosenFile)
                    currentPreset = numPresets + (int) i;
        }

        ++presetVersion;
    }

    int Editor::numAllPresets()
    {
        if (! presetsFound)
            findUserPresets();

        return numPresets + (int) userPresets.size();
    }

    std::string Editor::presetNameAt (int index)
    {
        if (index >= 0 && index < numPresets)
            return presets[index].name != nullptr ? presets[index].name : "";

        const auto user = index - numPresets;
        return index >= 0 && user < (int) userPresets.size() ? userPresets[(size_t) user].name : std::string();
    }

    std::string Editor::presetGroupAt (int index)
    {
        if (index >= 0 && index < numPresets)
            return "Factory";

        const auto user = index - numPresets;
        return index >= 0 && user < (int) userPresets.size() ? userPresets[(size_t) user].group : std::string();
    }

    void Editor::keepChosenValues()
    {
        chosenValues.clear();

        for (int p = 0, n = host.numParams(); p < n; ++p)
            chosenValues.push_back (host.getValue (p));
    }

    bool Editor::isPresetModified()
    {
        if (currentPreset < 0 || chosenValues.empty() || (int) chosenValues.size() != host.numParams())
            return false;

        for (size_t p = 0; p < chosenValues.size(); ++p)
        {
            const auto now = host.getValue ((int) p), then = chosenValues[p];

            if (std::abs (now - then) > 1.0e-5f * std::max (1.0f, std::abs (then)))
                return true;
        }

        return false;
    }

    void Editor::choosePreset (int index)
    {
        applyPreset (index);
    }

    void Editor::stepPreset (int delta)
    {
        const auto count = numAllPresets();

        if (count <= 0)
            return;

        if (currentPreset < 0 || currentPreset >= count)
            applyPreset (delta < 0 ? count - 1 : 0);
        else
            applyPreset ((currentPreset + (delta < 0 ? count - 1 : 1)) % count);
    }

    std::string Editor::presetText (const std::string& name) const
    {
        std::string text = "stella-preset 1\nname=" + name + "\n";

        for (int p = 0, n = host.numParams(); p < n; ++p)
            if (const auto* id = host.paramId (p); id != nullptr && *id != 0)
                text += std::string (id) + "=" + numberText (host.getValue (p)) + "\n";

        return text;
    }

    bool Editor::canDoPreset (PresetCommand command)
    {
        if (desktop == nullptr || host.numParams() <= 0)
            return command == PresetCommand::showFolder ? desktop != nullptr : false;

        const bool users = currentPreset >= numPresets && currentPreset < numAllPresets();

        switch (command)
        {
            case PresetCommand::saveFactory:  return false;   // the plugin's own: made in the studio
            case PresetCommand::remove:       return users;
            case PresetCommand::saveBank:     return numAllPresets() > 0;
            default:                          return true;
        }
    }

    void Editor::doPreset (PresetCommand command)
    {
        // Run from the window's timer, not in the middle of a click: its dialogs run their own
        // event loops.
        if (canDoPreset (command))
        {
            queuedCommand = command;
            hasQueuedCommand = true;
        }
    }

    void Editor::runQueuedPresetCommand()
    {
        if (! hasQueuedCommand)
            return;

        hasQueuedCommand = false;
        const auto command = queuedCommand;

        if (! canDoPreset (command))
            return;

        const auto folder = presetFolder();
        const auto count = numAllPresets();
        const bool users = currentPreset >= numPresets && currentPreset < count;

        auto chooseFile = [this] (const std::string& path)
        {
            findUserPresets();

            for (size_t i = 0; i < userPresets.size(); ++i)
                if (userPresets[i].file == path)
                {
                    currentPreset = numPresets + (int) i;
                    keepChosenValues();
                    ++presetVersion;
                    return;
                }
        };

        switch (command)
        {
            case PresetCommand::save:
            {
                if (users)
                {
                    // Quick save: over the chosen preset of the user's.
                    const auto& chosen = userPresets[(size_t) (currentPreset - numPresets)];

                    if (files::write (chosen.file, presetText (chosen.name)))
                        chooseFile (chosen.file);

                    return;
                }

                [[fallthrough]];   // a factory preset (or none): a new one of the user's
            }

            case PresetCommand::saveAs:
            {
                files::make (folder);
                std::string path;
                const auto suggested = currentPreset >= 0 && currentPreset < count ? presetNameAt (currentPreset) : std::string ("New preset");

                if (! desktop->chooseFile (true, "Save preset", folder, files::safe (suggested), presetExtension, path) || path.empty())
                    return;

                if (! files::endsWith (path, std::string (".") + presetExtension))
                    path += std::string (".") + presetExtension;

                if (files::write (path, presetText (files::stem (path))))
                    chooseFile (path);

                return;
            }

            case PresetCommand::load:
            {
                // A preset file from anywhere: kept among the user's, then chosen.
                std::string path, text;

                if (! desktop->chooseFile (false, "Load preset", folder, {}, presetExtension, path) || ! files::read (path, text))
                    return;

                const auto read = readPresets (text, files::stem (path));

                if (read.empty() || read.front().values.empty())
                {
                    desktop->confirm ("That file has no preset of this plugin's in it.");
                    return;
                }

                files::make (folder);
                const auto copy = folder + "/" + files::stem (path) + "." + presetExtension;

                if (copy != path)
                    files::write (copy, text);

                chooseFile (copy);

                if (currentPreset >= numPresets)
                    applyPreset (currentPreset);

                return;
            }

            case PresetCommand::saveBank:
            {
                // Every preset, the plugin's and the user's, into one file.
                files::make (folder);
                std::string path;

                if (! desktop->chooseFile (true, "Save bank", folder, files::safe (std::string (host.pluginName()) + " bank"), bankExtension, path)
                    || path.empty())
                    return;

                if (! files::endsWith (path, std::string (".") + bankExtension))
                    path += std::string (".") + bankExtension;

                std::string text = "stella-bank 1\n";

                for (int i = 0; i < numPresets; ++i)
                {
                    text += "preset=" + presetNameAt (i) + "\n";

                    for (int v = 0; v < presets[i].numValues; ++v)
                        if (presets[i].values[v].param != nullptr)
                            text += std::string (presets[i].values[v].param) + "=" + numberText (presets[i].values[v].value) + "\n";
                }

                for (const auto& user : userPresets)
                {
                    text += "preset=" + user.name + "\n";

                    for (const auto& [id, value] : user.values)
                        text += id + "=" + numberText (value) + "\n";
                }

                files::write (path, text);
                return;
            }

            case PresetCommand::loadBank:
            {
                // A bank's presets: a folder of their own (a group in the list), the first chosen.
                std::string path, text;

                if (! desktop->chooseFile (false, "Load bank", folder, {}, bankExtension, path) || ! files::read (path, text))
                    return;

                const auto read = readPresets (text, files::stem (path));

                if (read.empty())
                {
                    desktop->confirm ("That file has no presets of this plugin's in it.");
                    return;
                }

                auto bankFolder = folder + "/" + files::safe (files::stem (path));

                for (int n = 2; ! files::list (bankFolder, false).empty() && n < 1000; ++n)
                    bankFolder = folder + "/" + files::safe (files::stem (path)) + " " + std::to_string (n);

                files::make (bankFolder);
                std::string firstFile;

                for (size_t i = 0; i < read.size(); ++i)
                {
                    std::string body = "stella-preset 1\nname=" + read[i].name + "\n";

                    for (const auto& [id, value] : read[i].values)
                        body += id + "=" + numberText (value) + "\n";

                    char number[16];
                    std::snprintf (number, sizeof (number), "%03d ", (int) i + 1);
                    const auto file = bankFolder + "/" + number + files::safe (read[i].name) + "." + presetExtension;

                    if (files::write (file, body) && firstFile.empty())
                        firstFile = file;
                }

                if (! firstFile.empty())
                {
                    chooseFile (firstFile);

                    if (currentPreset >= numPresets)
                        applyPreset (currentPreset);
                }

                return;
            }

            case PresetCommand::remove:
            {
                const auto chosen = userPresets[(size_t) (currentPreset - numPresets)];

                if (! desktop->confirm ("Delete the preset \"" + chosen.name + "\"?"))
                    return;

                if (files::remove (chosen.file))
                {
                    currentPreset = -1;
                    chosenValues.clear();
                    findUserPresets();
                }

                return;
            }

            case PresetCommand::showFolder:
                files::make (folder);
                desktop->showFolder (folder);
                return;

            case PresetCommand::saveFactory:
                return;
        }
    }

    void Editor::mouseDrag (int x, int y, bool fine)
    {
        if (sizeMenuTookPress || sizeMenuOpen)
            return;

        x = toBaked (x);
        y = toBaked (y);

       #if STELLA_REGISTRATION
        if (registration != nullptr && registration->open)
        {
            auto& r = *registration;
            r.mouseX = (float) x + 0.5f;
            r.mouseY = (float) y + 0.5f;

            if (r.selecting)
                r.moveCaret (r.caretAt (r.layout(), r.mouseX), true);

            return;
        }
       #endif

       #if STELLA_ELEMENTS
        if (elements != nullptr && elements->pressed >= 0)
        {
            elements->mouse (elements->pressed, elements->pressedPopup, ui::ElementRuntime::drag, x, y, fine);
            return;
        }
       #endif

        if (active < 0)
            return;

        const auto& w = widgets[active];
        const auto& b = bound[(size_t) active];

        // Sliding across the keys plays each in turn; off the keyboard, nothing plays.
        if (w.kind == Kind::keyboard)
        {
            playKey (active, x, y);
            return;
        }

        if (w.kind == Kind::knob)
        {
            // Up or right turns it up; fine moves go slower.
            const auto travel = fine ? 900.0f : 200.0f;
            host.setValue (b.param, valueAt (b.param, startProportion + (float) ((x - dragX) - (y - dragY)) / travel));
        }
        else
        {
            setFromPointer (active, x, y);
        }
    }

    void Editor::mouseUp()
    {
        if (sizeMenuTookPress)
        {
            sizeMenuTookPress = false;
            return;
        }

       #if STELLA_REGISTRATION
        if (registration != nullptr && registration->open)
        {
            // A button acts when it's let go over it.
            auto& r = *registration;
            const auto part = r.pressed;
            r.pressed = RegistrationLayout::none;
            r.selecting = false;

            if (part != RegistrationLayout::none && r.layout().partAt (r.mouseX, r.mouseY) == part)
                r.activate (part);

            return;
        }
       #endif

       #if STELLA_ELEMENTS
        if (elements != nullptr && elements->pressed >= 0)
        {
            const auto element = elements->pressed;
            elements->pressed = -1;
            elements->mouse (element, elements->pressedPopup, ui::ElementRuntime::up, elements->lastX, elements->lastY, false);
        }
       #endif

        releaseKey();

        if (active < 0)
            return;

        if (widgets[active].kind == Kind::keyboard)
        {
            active = -1;
            return;
        }

        const auto& b = bound[(size_t) active];

        // A performance control let go: a pitch bend goes back to its middle.
        if (widgets[active].kind == Kind::perform)
        {
            const auto flags = widgets[active].vertical;

            if ((flags & 2) != 0 && b.param >= 0)   host.setValue (b.param, valueAt (b.param, 0.5f));
            if ((flags & 4) != 0 && b.param2 >= 0)  host.setValue (b.param2, valueAt (b.param2, 0.5f));
        }

        if (b.param >= 0) host.endEdit (b.param);
        if ((widgets[active].kind == Kind::xy || widgets[active].kind == Kind::perform) && b.param2 >= 0) host.endEdit (b.param2);

        active = -1;
    }

    void Editor::mouseWheel (int x, int y, float notches)
    {
        if (wantsKeys() || sizeMenuOpen)
            return;   // Registration (or the size menu) is over the panel

        x = toBaked (x);
        y = toBaked (y);

       #if STELLA_ELEMENTS
        if (elements != nullptr)
        {
            if (const auto owner = elements->popupAt (x, y); owner >= 0)
            {
                elements->mouse (owner, true, ui::ElementRuntime::wheel, x, y, false, false, notches);
                return;
            }

            if (const auto index = widgetAt (x, y); index >= 0 && widgets[index].kind == Kind::custom)
            {
                elements->mouse (index, false, ui::ElementRuntime::wheel, x, y, false, false, notches);
                return;
            }
        }
       #endif

        const auto index = widgetAt (x, y);

        if (index < 0 || (widgets[index].kind != Kind::knob && widgets[index].kind != Kind::slider))
            return;

        const auto param = bound[(size_t) index].param;

        if (param < 0)
            return;

        host.beginEdit (param);
        host.setValue (param, valueAt (param, proportionOf (param) + notches * 0.04f));
        host.endEdit (param);
    }

    void Editor::mouseMove (int x, int y)
    {
        if (sizeMenuMouse (x, y, false))
            return;

        x = toBaked (x);
        y = toBaked (y);

       #if STELLA_REGISTRATION
        if (registration != nullptr)
        {
            auto& r = *registration;
            r.mouseX = (float) x + 0.5f;
            r.mouseY = (float) y + 0.5f;

            if (r.open)
            {
                r.hovered = r.layout().partAt (r.mouseX, r.mouseY);
                return;
            }

            r.tabHovered = r.showsTab() && r.tab().contains (r.mouseX, r.mouseY);
        }
       #endif

       #if STELLA_ELEMENTS
        if (elements == nullptr)
            return;

        // The element (or popup) under the mouse hears it move; the one it left, that it left.
        auto index = elements->popupAt (x, y);
        const bool popup = index >= 0;

        if (! popup)
        {
            index = widgetAt (x, y);

            if (index >= 0 && widgets[index].kind != Kind::custom)
                index = -1;
        }

        if (index != elements->hovered || popup != elements->hoveredPopup)
        {
            if (elements->hovered >= 0)
                elements->mouse (elements->hovered, elements->hoveredPopup, ui::ElementRuntime::exit, x, y, false);

            elements->hovered = index;
            elements->hoveredPopup = popup;
        }

        if (index >= 0)
            elements->mouse (index, popup, ui::ElementRuntime::move, x, y, false);
       #else
        (void) x;
        (void) y;
       #endif
    }

    void Editor::mouseExit()
    {
        sizeMenuHovered = -1;

       #if STELLA_REGISTRATION
        if (registration != nullptr)
        {
            registration->hovered = RegistrationLayout::none;
            registration->tabHovered = false;
        }
       #endif

       #if STELLA_ELEMENTS
        if (elements != nullptr && elements->hovered >= 0)
        {
            elements->mouse (elements->hovered, elements->hoveredPopup, ui::ElementRuntime::exit, elements->lastX, elements->lastY, false);
            elements->hovered = -1;
        }
       #endif
    }

    //==========================================================================
    void Editor::blitPixels (std::uint32_t* pixels, const std::uint32_t* source, int x, int y, int w, int h) const
    {
        if (source == nullptr)
            return;

        for (int row = 0; row < h; ++row)
        {
            const auto ty = y + row;

            if (ty < 0 || ty >= height)
                continue;

            for (int col = 0; col < w; ++col)
            {
                const auto tx = x + col;

                if (tx >= 0 && tx < width)
                    blend (pixels[(size_t) ty * (size_t) width + (size_t) tx], source[(size_t) row * (size_t) w + (size_t) col]);
            }
        }
    }

    void Editor::drawElement (std::uint32_t* pixels, int index)
    {
       #if STELLA_ELEMENTS
        if (elements == nullptr)
            return;

        if (elements->runtime.needsPaint (index))
            elements->runtime.render (index);

        // Only what its buffer holds: the runtime keeps an element within 4096 x 4096.
        const auto& w = widgets[index];
        blitPixels (pixels, elements->runtime.pixels (index, false), w.x, w.y, std::max (0, std::min (4096, w.w)), std::max (0, std::min (4096, w.h)));
       #else
        (void) pixels;
        (void) index;
       #endif
    }

    void Editor::drawPopups (std::uint32_t* pixels)
    {
       #if STELLA_ELEMENTS
        if (elements == nullptr)
            return;

        for (int i = 0; i < numWidgets; ++i)
        {
            int x = 0, y = 0, w = 0, h = 0;

            if (elements->popupArea (i, x, y, w, h))
            {
                // A soft shadow under it, as menus have.
                Canvas canvas { pixels, width, height };

                for (int s = 1; s <= 6; ++s)
                    canvas.fillRect ((float) (x - s + 2), (float) (y - s + 4), (float) (x + w + s - 2), (float) (y + h + s), 0xff000000u, 0.045f);

                blitPixels (pixels, elements->runtime.pixels (i, true), x, y, w, h);
            }
        }
       #else
        (void) pixels;
       #endif
    }

    //==========================================================================
    void Editor::blitFrame (std::uint32_t* pixels, const Image& image, int frameHeight, int frame, int x, int y) const
    {
        if (image.pixels.empty() || frameHeight <= 0)
            return;

        for (int row = 0; row < frameHeight; ++row)
        {
            const auto ty = y + row;
            const auto sy = frame * frameHeight + row;

            if (ty < 0 || ty >= height || sy < 0 || sy >= image.height)
                continue;

            for (int col = 0; col < image.width; ++col)
            {
                const auto tx = x + col;

                if (tx >= 0 && tx < width)
                    blend (pixels[(size_t) ty * (size_t) width + (size_t) tx], image.pixels[(size_t) sy * (size_t) image.width + (size_t) col]);
            }
        }
    }

    void Editor::drawMeter (std::uint32_t* pixels, const Widget& w, int index, const Image* image, const Strip* strip)
    {
        // On a signal it falls back smoothly; a needle swings after it, as a real one does.
        auto& shown = meterLevels[(size_t) index];
        const auto level = host.takeLevel (index, (w.mode & 1) != 0);
        shown = w.isDisplay ? level : std::max (level, shown * 0.86f);

        const auto fraction = fractionOf (shown, w.isDisplay != 0);
        auto& needle = meterNeedles[(size_t) index];
        needle += (fraction - needle) * 0.22f;
        const auto position = (w.mode & 2) != 0 ? needle : fraction;

        // Its look at that level, as Sonify drew it.
        if (image != nullptr && strip->frames > 0)
        {
            const auto frame = std::min (strip->frames - 1, std::max (0, (int) std::lround (position * (float) (strip->frames - 1))));
            blitFrame (pixels, *image, strip->frameHeight, frame, w.x, w.y);
            return;
        }

        const auto t1 = w.isDisplay ? 0.75f : 48.0f / 66.0f, t2 = w.isDisplay ? 0.9f : 57.0f / 66.0f;

        auto colourAt = [&] (float t)
        {
            if (t < t1) return green;
            if (t < t2) return mix (green, amber, (t - t1) / (t2 - t1));
            return mix (amber, red, (t - t2) / (1.0f - t2));
        };

        Canvas canvas { pixels, width, height };
        const auto x0 = (float) w.x + 2.0f, y0 = (float) w.y + 2.0f, x1 = (float) (w.x + w.w) - 2.0f, y1 = (float) (w.y + w.h) - 2.0f;

        if (w.h >= w.w)
        {
            const auto top = y1 - (y1 - y0) * position;

            for (int y = (int) std::floor (top); y < (int) std::ceil (y1); ++y)
                canvas.fillRect (x0, std::max ((float) y, top), x1, (float) y + 1.0f, colourAt ((y1 - (float) y) / (y1 - y0)), 1.0f);
        }
        else
        {
            const auto right = x0 + (x1 - x0) * position;

            for (int x = (int) std::floor (x0); x < (int) std::ceil (right); ++x)
                canvas.fillRect ((float) x, y0, std::min ((float) x + 1.0f, right), y1, colourAt (((float) x - x0) / (x1 - x0)), 1.0f);
        }
    }

    void Editor::drawLamp (std::uint32_t* pixels, const Widget& w, int index, const Image* image, const Strip* strip)
    {
        if (image == nullptr || strip->frames <= 0)
            return;

        // On from its threshold, or following the level (a signal's in dB, as a meter shows it).
        const auto level = host.takeLevel (index, false);

        if (w.mode == 1)
        {
            auto& shown = meterLevels[(size_t) index];
            shown = w.isDisplay ? level : std::max (level, shown * 0.86f);
            const auto lit = fractionOf (shown, w.isDisplay != 0);
            blitFrame (pixels, *image, strip->frameHeight, std::min (strip->frames - 1, std::max (0, (int) std::lround (lit * (float) (strip->frames - 1)))), w.x, w.y);
        }
        else
        {
            blitFrame (pixels, *image, strip->frameHeight, level >= w.threshold ? std::min (1, strip->frames - 1) : 0, w.x, w.y);
        }
    }

    void Editor::drawScope (std::uint32_t* pixels, const Widget& w, int index)
    {
        host.readScope (index, scopeBuffer.data(), (int) scopeBuffer.size());

        constexpr int shown = 1024;
        int start = 0;

        for (int i = 1; i < (int) scopeBuffer.size() - shown; ++i)
        {
            if (scopeBuffer[(size_t) i - 1] < 0.0f && scopeBuffer[(size_t) i] >= 0.0f)
            {
                start = i;
                break;
            }
        }

        Canvas canvas { pixels, width, height };
        const auto x0 = (float) w.x + 3.0f, y0 = (float) w.y + 3.0f, x1 = (float) (w.x + w.w) - 3.0f, y1 = (float) (w.y + w.h) - 3.0f;
        const auto centre = (y0 + y1) * 0.5f, half = (y1 - y0) * 0.5f;

        float px = x0, py = centre - std::min (1.0f, std::max (-1.0f, scopeBuffer[(size_t) start])) * half;

        for (int i = 2; i < shown; i += 2)
        {
            const auto x = x0 + (x1 - x0) * (float) i / (float) (shown - 1);
            const auto y = centre - std::min (1.0f, std::max (-1.0f, scopeBuffer[(size_t) (start + i)])) * half;
            canvas.line (px, py, x, y, 1.5f, w.colour, 1.0f);
            px = x;
            py = y;
        }
    }

    void Editor::drawCurve (std::uint32_t* pixels, const Widget& w, const Bound& b, int index)
    {
        Canvas canvas { pixels, width, height };
        const auto x0 = (float) w.x + 6.0f, y0 = (float) w.y + 6.0f, x1 = (float) (w.x + w.w) - 6.0f, y1 = (float) (w.y + w.h) - 6.0f;
        std::vector<float> xs, ys;

        if (w.kind == Kind::envelope)
        {
            // Each stage is as wide as its knob shows; the sustain plateau keeps a fixed width.
            const auto a = b.param >= 0 ? proportionOf (b.param) : 0.1f;
            const auto d = b.param2 >= 0 ? proportionOf (b.param2) : 0.3f;
            const auto s = b.param3 >= 0 ? proportionOf (b.param3) : 0.7f;
            const auto r = b.param4 >= 0 ? proportionOf (b.param4) : 0.4f;

            const float stageWidths[] { 0.08f + 0.92f * a, 0.08f + 0.92f * d, 0.6f, 0.08f + 0.92f * r };
            const auto unit = (x1 - x0) / (stageWidths[0] + stageWidths[1] + stageWidths[2] + stageWidths[3]);
            const auto sustainY = y1 - (y1 - y0) * s;

            xs = { x0, x0 + stageWidths[0] * unit, x0 + (stageWidths[0] + stageWidths[1]) * unit,
                   x0 + (stageWidths[0] + stageWidths[1] + stageWidths[2]) * unit, x1 };
            ys = { y1, y0, sustainY, sustainY, y1 };

            canvas.curve (xs, ys, y1, w.colour, 0.22f);

            for (int i = 1; i < 4; ++i)
                canvas.circle (xs[(size_t) i], ys[(size_t) i], 3.0f, w.colour, 1.0f);

            return;
        }

        // A filter's response over 20 Hz..20 kHz (log), -30..+24 dB.
        float cutoff = 1000.0f;

        if (b.param >= 0)
            cutoff = w.cutoffScale > 0.0f ? host.getValue (b.param) * w.cutoffScale : 20.0f * std::pow (1000.0f, proportionOf (b.param));

        cutoff = std::min (20000.0f, std::max (20.0f, cutoff));

        const auto resonance = b.param2 >= 0 ? proportionOf (b.param2) : 0.2f;
        const auto q = 0.707f + resonance * 9.0f;
        const auto steep = w.mode == 1 || w.mode == 3;
        const auto kind = w.mode == 2 || w.mode == 3 ? 1 : (w.mode == 4 ? 2 : 0);

        // The response at a cutoff, as points along the box.
        auto responseAt = [&] (float hz)
        {
            xs.clear();
            ys.clear();

            for (int i = 0; i <= 120; ++i)
            {
                const auto t = (float) i / 120.0f;
                const auto f = 20.0f * std::pow (1000.0f, t);
                const auto x = f / hz;
                const auto denominator = std::sqrt ((1.0f - x * x) * (1.0f - x * x) + (x / q) * (x / q));
                auto magnitude = kind == 1 ? (x * x) / denominator : (kind == 2 ? (x / q) / denominator : 1.0f / denominator);

                if (steep)
                    magnitude *= kind == 2 ? 1.0f : magnitude / std::max (1.0f, q * 0.5f);

                const auto db = std::min (24.0f, std::max (-30.0f, 20.0f * std::log10 (std::max (magnitude, 1.0e-6f))));
                xs.push_back (x0 + (x1 - x0) * t);
                ys.push_back (y0 + (y1 - y0) * ((24.0f - db) / 54.0f));
            }
        };

        // Following the plugin's live cutoff (its source: a display, in Hz or 0..1 along 20 Hz to
        // 20 kHz), it opens and closes as it plays, as the studio's canvas shows it; the knob's own
        // setting stays as a faint line.
        float live = 0.0f;

        if (w.source != nullptr && w.source[0] != 0)
        {
            const auto value = host.takeLevel (index, false);

            if (value > 1.5f)
                live = std::min (20000.0f, std::max (20.0f, value));
            else if (value > 0.0f)
                live = 20.0f * std::pow (1000.0f, std::min (1.0f, value));
        }

        if (live <= 0.0f)
        {
            responseAt (cutoff);
            canvas.curve (xs, ys, y1, w.colour, 0.22f);
            return;
        }

        responseAt (cutoff);

        for (size_t i = 0; i + 1 < xs.size(); ++i)
            canvas.line (xs[i], ys[i], xs[i + 1], ys[i + 1], 1.0f, w.colour, 0.35f);

        // How far it's opened from the knob: the further, the brighter.
        const auto opened = std::min (1.0f, std::fabs (std::log2 (live / cutoff)) / 3.0f);
        const auto marker = x0 + (x1 - x0) * std::log10 (live / 20.0f) / 3.0f;
        canvas.fillRect (marker - 0.75f, y0, marker + 0.75f, y1, w.colour, 0.18f + 0.30f * opened);

        responseAt (live);
        canvas.curve (xs, ys, y1, mix (w.colour, 0xffffffffu, 0.15f + 0.35f * opened), 0.17f + 0.20f * opened);
    }

    void Editor::drawXy (std::uint32_t* pixels, const Widget& w, const Bound& b)
    {
        Canvas canvas { pixels, width, height };
        const auto x = b.param >= 0 ? proportionOf (b.param) : 0.5f;
        const auto y = b.param2 >= 0 ? proportionOf (b.param2) : 0.5f;
        const auto dx = (float) w.x + 2.0f + ((float) w.w - 4.0f) * x;
        const auto dy = (float) (w.y + w.h) - 2.0f - ((float) w.h - 4.0f) * y;

        canvas.fillRect (std::floor (dx), (float) w.y + 1.0f, std::floor (dx) + 1.0f, (float) (w.y + w.h) - 1.0f, w.colour, 0.4f);
        canvas.fillRect ((float) w.x + 1.0f, std::floor (dy), (float) (w.x + w.w) - 1.0f, std::floor (dy) + 1.0f, w.colour, 0.4f);
        canvas.circle (dx, dy, 11.0f, w.colour, 0.35f);
        canvas.circle (dx, dy, 5.5f, w.colour, 1.0f);
    }

    void Editor::drawKeyboard (std::uint32_t* pixels, const Widget& w, int index)
    {
        // The keys at rest are in the background; the ones that are down are drawn here.
        const auto low = w.mode, high = w.count;
        const auto x0 = (float) w.x, y0 = (float) w.y, kw = (float) w.w, kh = (float) w.h;
        Canvas canvas { pixels, width, height };
        const auto* background = page < (int) backgrounds.size() ? &backgrounds[(size_t) page] : nullptr;

        auto isDown = [&] (int note)
        {
            return (note == heldNote && index == heldKeyboard) || host.isNoteDown (note);
        };

        bool whiteDown = false;

        for (int note = low; note <= high; ++note)
        {
            if (keys::isBlack (note) || ! isDown (note))
                continue;

            const auto k = keys::keyOf (note, low, high, x0, y0, kw, kh);
            canvas.fillRect (k.x + 0.5f, k.y, k.x + k.w - 0.5f, k.y + k.h, w.colour, 0.55f);
            whiteDown = true;
        }

        // The black keys lie on top: as the background has them, over a white key that went down.
        if (whiteDown && background != nullptr && (int) background->pixels.size() == width * height)
        {
            for (int note = low; note <= high; ++note)
            {
                if (! keys::isBlack (note))
                    continue;

                const auto k = keys::keyOf (note, low, high, x0, y0, kw, kh);
                const auto left = std::max (0, (int) std::floor (k.x)), right = std::min (width, (int) std::ceil (k.x + k.w));
                const auto top = std::max (0, (int) std::floor (k.y)), bottom = std::min (height, (int) std::ceil (k.y + k.h));

                for (int y = top; y < bottom && right > left; ++y)
                    std::memcpy (pixels + (size_t) y * (size_t) width + (size_t) left,
                                 background->pixels.data() + (size_t) y * (size_t) width + (size_t) left,
                                 sizeof (std::uint32_t) * (size_t) (right - left));
            }
        }

        for (int note = low; note <= high; ++note)
        {
            if (! keys::isBlack (note) || ! isDown (note))
                continue;

            const auto k = keys::keyOf (note, low, high, x0, y0, kw, kh);
            canvas.fillRect (k.x, k.y, k.x + k.w, k.y + k.h, w.colour, 0.8f);
        }
    }

    //==========================================================================
    //==========================================================================
    // Scaled: the baked window resampled (bilinear up, averaged down), on premultiplied pixels.
    struct Editor::Resampler
    {
        struct Taps
        {
            std::vector<int> first, count, index, weight;   // per output pixel: its source pixels and weights (out of 256)
        };

        int srcW = 0, srcH = 0, dstW = 0, dstH = 0;
        Taps across, down;
        std::vector<std::uint32_t> middle;   // across done, down not yet

        static Taps tapsFor (int src, int dst)
        {
            Taps t;
            t.first.resize ((size_t) dst);
            t.count.resize ((size_t) dst);

            for (int d = 0; d < dst; ++d)
            {
                const auto start = (int) t.index.size();

                if (dst >= src)
                {
                    // Bigger: between the two nearest.
                    const float c = ((float) d + 0.5f) * (float) src / (float) dst - 0.5f;
                    auto i0 = (int) std::floor (c);
                    auto f = c - (float) i0;

                    if (i0 < 0)            { i0 = 0; f = 0.0f; }
                    if (i0 >= src - 1)     { i0 = src - 1; f = 0.0f; }

                    const auto w1 = (int) std::lround (f * 256.0f);
                    t.index.push_back (i0);                        t.weight.push_back (256 - w1);
                    t.index.push_back (std::min (i0 + 1, src - 1)); t.weight.push_back (w1);
                }
                else
                {
                    // Smaller: every source pixel it covers, by how much.
                    const float a = (float) d * (float) src / (float) dst, b = (float) (d + 1) * (float) src / (float) dst;
                    const float span = b - a;
                    int sum = 0;

                    for (auto i = (int) std::floor (a); i < (int) std::ceil (b) && i < src; ++i)
                    {
                        const float overlap = std::min (b, (float) (i + 1)) - std::max (a, (float) i);
                        const auto w = (int) std::lround (overlap / span * 256.0f);
                        t.index.push_back (i);
                        t.weight.push_back (w);
                        sum += w;
                    }

                    if (t.weight.size() > (size_t) start)
                        t.weight.back() += 256 - sum;   // exactly 256 together
                }

                t.first[(size_t) d] = start;
                t.count[(size_t) d] = (int) t.index.size() - start;
            }

            return t;
        }

        static std::uint32_t mix (const std::uint32_t* source, int stride, const int* index, const int* weight, int count) noexcept
        {
            std::uint32_t rb = 0, ag = 0;

            for (int k = 0; k < count; ++k)
            {
                const auto p = source[(size_t) index[k] * (size_t) stride];
                const auto w = (std::uint32_t) weight[k];
                rb += (p & 0x00ff00ffu) * w;
                ag += ((p >> 8) & 0x00ff00ffu) * w;
            }

            return ((rb >> 8) & 0x00ff00ffu) | (ag & 0xff00ff00u);
        }

        void run (const std::uint32_t* src, int sw, int sh, std::uint32_t* dst, int dw, int dh)
        {
            if (sw != srcW || dw != dstW)    { across = tapsFor (sw, dw); srcW = sw; dstW = dw; }
            if (sh != srcH || dh != dstH)    { down = tapsFor (sh, dh); srcH = sh; dstH = dh; }

            middle.resize ((size_t) dw * (size_t) sh);

            for (int y = 0; y < sh; ++y)
            {
                const auto* row = src + (size_t) y * (size_t) sw;
                auto* out = middle.data() + (size_t) y * (size_t) dw;

                for (int x = 0; x < dw; ++x)
                    out[x] = mix (row, 1, across.index.data() + across.first[(size_t) x], across.weight.data() + across.first[(size_t) x],
                                  across.count[(size_t) x]);
            }

            for (int y = 0; y < dh; ++y)
            {
                const auto* index = down.index.data() + down.first[(size_t) y];
                const auto* weight = down.weight.data() + down.first[(size_t) y];
                const auto count = down.count[(size_t) y];
                auto* out = dst + (size_t) y * (size_t) dw;

                for (int x = 0; x < dw; ++x)
                    out[x] = mix (middle.data() + x, dw, index, weight, count);
            }
        }
    };

    void Editor::setScale (float newScale)
    {
        scale = snapScale (newScale);
        sizeMenuOpen = false;
    }

    int Editor::toBaked (int v) const noexcept
    {
        return scale == 1.0f ? v : (int) std::floor ((float) v / scale);
    }

    void Editor::render (std::uint32_t* pixels)
    {
        // The preset managers' "*": drawn again when the values leave the chosen preset's (or return).
        if (currentPreset >= 0 && ++modifiedCheck >= 8)
        {
            modifiedCheck = 0;

            if (const bool modified = isPresetModified(); modified != wasModified)
            {
                wasModified = modified;
                ++presetVersion;
            }
        }

        const auto w = getWidth(), h = getHeight();

        if (w == width && h == height)
        {
            renderBaked (pixels);
        }
        else
        {
            base.resize ((size_t) width * (size_t) height);
            renderBaked (base.data());

            if (resampler == nullptr)
                resampler = std::make_unique<Resampler>();

            resampler->run (base.data(), width, height, pixels, w, h);
        }

        drawSizeMenu (pixels);
    }

    //==========================================================================
    // The size menu: a grid of the steps, 50% to 200%, the one chosen lit.
    void Editor::contextClick (int x, int y)
    {
        if (wantsKeys())
            return;   // Registration is open

        sizeMenuOpen = true;
        sizeMenuHovered = -1;
        sizeMenuX = x;
        sizeMenuY = y;
    }

    void Editor::layOutSizeMenu (int& columns, int& cellW, int& cellH, int& left, int& top) const
    {
        const auto W = getWidth(), H = getHeight();
        const int pad = 6, margin = 4;
        float best = -1.0f;

        // As many across as fits it best into the window (a small window: smaller cells).
        for (int c = 4; c <= 8; ++c)
        {
            const auto rows = (numScales + c - 1) / c;
            const auto cw = std::max (8, std::min (58, (W - 2 * (pad + margin)) / c));
            const auto ch = std::max (6, std::min (24, (H - 2 * (pad + margin)) / rows));
            const auto fit = std::min ((float) cw / 58.0f, (float) ch / 24.0f);

            if (fit > best + 0.001f)
            {
                best = fit;
                columns = c;
                cellW = cw;
                cellH = ch;
            }
        }

        const auto rows = (numScales + columns - 1) / columns;
        const auto panelW = columns * cellW + 2 * pad, panelH = rows * cellH + 2 * pad;
        left = std::max (margin, std::min (sizeMenuX, W - margin - panelW)) + pad;
        top = std::max (margin, std::min (sizeMenuY, H - margin - panelH)) + pad;
    }

    int Editor::sizeMenuItemAt (int x, int y) const
    {
        int columns = 4, cellW = 58, cellH = 24, left = 0, top = 0;
        layOutSizeMenu (columns, cellW, cellH, left, top);

        if (x < left || y < top)
            return -1;

        const auto c = (x - left) / cellW, r = (y - top) / cellH;
        const auto item = r * columns + c;
        return c < columns && item < numScales ? item : -1;
    }

    bool Editor::sizeMenuMouse (int x, int y, bool down)
    {
        if (! sizeMenuOpen)
            return false;

        const auto item = sizeMenuItemAt (x, y);

        if (! down)
        {
            sizeMenuHovered = item;
            return true;
        }

        sizeMenuOpen = false;
        sizeMenuHovered = -1;

        if (item < 0)
            return true;   // outside it: closed, nothing else

        const auto chosen = snapScale (minScale + scaleStep * (float) item);

        if (chosen != scale)
        {
            scale = chosen;
            host.setWindowScale (scale);

            if (onScaleChosen)
                onScaleChosen();
        }

        return true;
    }

   #if ! STELLA_REGISTRATION
    namespace
    {
        /** Figures and % as blocks (no fonts in this plugin): 3 x 5 cells each. */
        void drawBlockText (std::uint32_t* pixels, int W, int H, const std::string& text, int centreX, int centreY, int cell, std::uint32_t colour)
        {
            static const char* const glyphs[11] { "111101101101111", "010110010010111", "111001111100111", "111001111001111", "101101111001001",
                                                  "111100111001111", "111100111101111", "111001001001001", "111101111101111", "111101111001111",
                                                  "101001010100101" };
            const auto width = (int) text.size() * 4 * cell - cell;
            auto x = centreX - width / 2;
            const auto y = centreY - (5 * cell) / 2;

            for (const auto ch : text)
            {
                const auto g = ch == '%' ? 10 : (ch >= '0' && ch <= '9' ? ch - '0' : -1);

                if (g >= 0)
                    for (int r = 0; r < 5; ++r)
                        for (int c = 0; c < 3; ++c)
                            if (glyphs[g][r * 3 + c] == '1')
                                for (int py = y + r * cell; py < y + (r + 1) * cell; ++py)
                                    for (int px = x + c * cell; px < x + (c + 1) * cell; ++px)
                                        if (px >= 0 && py >= 0 && px < W && py < H)
                                            pixels[(size_t) py * (size_t) W + (size_t) px] = colour;

                x += 4 * cell;
            }
        }

        void fillBox (std::uint32_t* pixels, int W, int H, int x, int y, int w, int h, std::uint32_t colour)
        {
            for (int py = std::max (0, y); py < std::min (H, y + h); ++py)
                for (int px = std::max (0, x); px < std::min (W, x + w); ++px)
                    blend (pixels[(size_t) py * (size_t) W + (size_t) px], colour);
        }
    }
   #endif

    void Editor::drawSizeMenu (std::uint32_t* pixels)
    {
        if (! sizeMenuOpen)
            return;

        const auto W = getWidth(), H = getHeight();
        int columns = 4, cellW = 58, cellH = 24, left = 0, top = 0;
        layOutSizeMenu (columns, cellW, cellH, left, top);
        const auto rows = (numScales + columns - 1) / columns;
        const auto current = (int) std::lround ((scale - minScale) / scaleStep);
        const int pad = 6;

       #if STELLA_REGISTRATION
        Painter p (pixels, W, H);
        const Rect panel { (float) (left - pad), (float) (top - pad), (float) (columns * cellW + 2 * pad), (float) (rows * cellH + 2 * pad) };
        p.shadow (panel, 8.0f, 10.0f, 0.5f);
        p.fill (panel, 8.0f, 0xff1e1f24u);
        p.outline (panel, 8.0f, 1.0f, 0xff3a3c44u);
        static Text text;
       #else
        fillBox (pixels, W, H, left - pad, top - pad, columns * cellW + 2 * pad, rows * cellH + 2 * pad, 0xf01e1f24u);
       #endif

        for (int i = 0; i < numScales; ++i)
        {
            const auto x = left + (i % columns) * cellW, y = top + (i / columns) * cellH;
            const auto label = std::to_string ((int) std::lround ((minScale + scaleStep * (float) i) * 100.0f)) + "%";
            const bool chosen = i == current, hovered = i == sizeMenuHovered;
            const std::uint32_t ink = chosen ? 0xff101114u : (hovered ? 0xffffffffu : 0xffc9ccd4u);

           #if STELLA_REGISTRATION
            const Rect cell { (float) x + 1.0f, (float) y + 1.0f, (float) cellW - 2.0f, (float) cellH - 2.0f };

            if (chosen)
                p.fill (cell, 5.0f, 0xff2fb36bu);
            else if (hovered)
                p.fill (cell, 5.0f, 0xff34363eu);

            const auto size = std::max (6.0f, std::min (13.0f, (float) cellH * 0.52f));
            const auto tw = text.width (label, size, chosen);
            text.draw (p, label, std::round (cell.x + (cell.w - tw) * 0.5f), Text::baselineFor (cell.centreY(), size), size, chosen, ink);
           #else
            if (chosen)
                fillBox (pixels, W, H, x + 1, y + 1, cellW - 2, cellH - 2, 0xff2fb36bu);
            else if (hovered)
                fillBox (pixels, W, H, x + 1, y + 1, cellW - 2, cellH - 2, 0xff34363eu);

            drawBlockText (pixels, W, H, label, x + cellW / 2, y + cellH / 2, std::max (1, cellH / 10), ink);
           #endif
        }
    }

    void Editor::renderBaked (std::uint32_t* pixels)
    {
        const auto* background = page < (int) backgrounds.size() ? &backgrounds[(size_t) page] : nullptr;

        if (background != nullptr && (int) background->pixels.size() == width * height)
            std::memcpy (pixels, background->pixels.data(), background->pixels.size() * sizeof (std::uint32_t));
        else
            std::fill (pixels, pixels + (size_t) width * (size_t) height, 0xff202024u);

        for (int i = 0; i < numWidgets; ++i)
        {
            if (! isShown (i))
                continue;

            const auto& w = widgets[i];
            const auto& b = bound[(size_t) i];
            const auto* strip = w.strip >= 0 && w.strip < numStrips ? &strips[w.strip] : nullptr;
            const auto* image = strip != nullptr ? &frames[(size_t) w.strip] : nullptr;

            auto frameOf = [&] (float proportion)
            {
                return std::min (strip->frames - 1, std::max (0, (int) std::lround (proportion * (float) (strip->frames - 1))));
            };

            switch (w.kind)
            {
                case Kind::knob:
                case Kind::slider:
                case Kind::readout:
                    if (image != nullptr)
                        blitFrame (pixels, *image, strip->frameHeight, frameOf (proportionOf (b.param)), w.x, w.y);
                    break;

                case Kind::toggle:
                    if (image != nullptr)
                        blitFrame (pixels, *image, strip->frameHeight, proportionOf (b.param) >= 0.5f ? std::min (1, strip->frames - 1) : 0, w.x, w.y);
                    break;

                case Kind::selector:
                    if (image != nullptr && b.param >= 0)
                    {
                        float min = 0.0f, max = 1.0f, def = 0.0f, skew = 1.0f;
                        host.getRange (b.param, min, max, def, skew);
                        const auto chosen = (int) std::lround (host.getValue (b.param) - min);
                        blitFrame (pixels, *image, strip->frameHeight, std::min (strip->frames - 1, std::max (0, chosen)), w.x, w.y);
                    }
                    break;

                case Kind::preset:
                    // One frame per preset, then one for none chosen.
                    if (image != nullptr)
                        blitFrame (pixels, *image, strip->frameHeight,
                                   currentPreset >= 0 && currentPreset < strip->frames - 1 ? currentPreset : strip->frames - 1, w.x, w.y);
                    break;

                case Kind::tabs:
                    if (image != nullptr)
                        blitFrame (pixels, *image, strip->frameHeight, std::min (strip->frames - 1, std::max (0, page)), w.x, w.y);
                    break;

                case Kind::picture:   // in front of what's under it
                    if (image != nullptr)
                        blitFrame (pixels, *image, strip->frameHeight, 0, w.x, w.y);
                    break;

                case Kind::registration:   // the badge: frame 0 while anything is unregistered, 1 when all is
                    if (image != nullptr)
                        blitFrame (pixels, *image, strip->frameHeight, isRegistered (i) ? std::min (1, strip->frames - 1) : 0, w.x, w.y);
                    break;

                case Kind::key:            // lit over the set in the background
                    if (image != nullptr && b.param >= 0)
                    {
                        float min = 0.0f, max = 1.0f, def = 0.0f, skew = 1.0f;
                        host.getRange (b.param, min, max, def, skew);
                        const auto at = (int) std::lround (host.getValue (b.param) - min);
                        const bool on = w.mode == 1 ? proportionOf (b.param) >= 0.5f
                                      : w.mode == 2 ? ((at >> std::max (0, std::min (30, w.count))) & 1) != 0
                                                    : at == w.count;

                        if (on)
                            blitFrame (pixels, *image, strip->frameHeight, std::min (1, strip->frames - 1), w.x, w.y);
                    }
                    break;

                case Kind::perform:        // its picture for where it is: across, then up
                    if (image != nullptr)
                    {
                        const int across = std::max (1, w.count), up = std::max (1, w.mode);
                        const bool pair = (w.vertical & 1) != 0;
                        const float px = b.param >= 0 ? proportionOf (b.param) : 0.5f, py = b.param2 >= 0 ? proportionOf (b.param2) : 0.5f;
                        const int ix = across > 1 ? (int) std::lround (px * (float) (across - 1)) : 0;
                        const int iy = up > 1 ? (int) std::lround (py * (float) (up - 1)) : 0;
                        (void) pair;
                        blitFrame (pixels, *image, strip->frameHeight, std::min (strip->frames - 1, std::max (0, iy * across + ix)), w.x, w.y);
                    }
                    break;

                case Kind::meter:     drawMeter (pixels, w, i, image, strip); break;
                case Kind::lamp:      drawLamp (pixels, w, i, image, strip); break;
                case Kind::scope:     drawScope (pixels, w, i); break;
                case Kind::envelope:
                case Kind::filter:    drawCurve (pixels, w, b, i); break;
                case Kind::xy:        drawXy (pixels, w, b); break;
                case Kind::keyboard:  drawKeyboard (pixels, w, i); break;
                case Kind::custom:    drawElement (pixels, i); break;
            }
        }

        // Programmed elements' popups lie over everything, and Registration over them.
        drawPopups (pixels);
        drawRegistration (pixels);
    }

    //==========================================================================
    // Registration (the plugin, or a library it plays, protected).
   #if STELLA_REGISTRATION
    void Editor::openRegistration()
    {
        if (registration == nullptr)
            return;

        auto& r = *registration;
        r.refresh();

        if (r.items.empty())
            return;   // nothing protected

        r.computerId = host.licenceComputerId();

        // What the panel was doing stops: it's behind now.
        if (active >= 0)
            mouseUp();

        releaseKey();

       #if STELLA_ELEMENTS
        if (elements != nullptr)
        {
            elements->closePopups();

            if (elements->hovered >= 0)
                elements->mouse (elements->hovered, elements->hoveredPopup, ui::ElementRuntime::exit, elements->lastX, elements->lastY, false);

            elements->hovered = elements->pressed = -1;
        }
       #endif

        r.open = true;
        r.say ({}, 0);
        r.hovered = r.pressed = RegistrationLayout::none;
        r.selecting = r.tabHovered = false;
        r.anchor = 0;
        r.moveCaret ((int) r.serial.size(), false);
    }

    void Editor::closeRegistration()
    {
        if (registration == nullptr)
            return;

        auto& r = *registration;
        r.open = r.selecting = false;
        r.hovered = r.pressed = RegistrationLayout::none;
    }

    bool Editor::wantsKeys() const noexcept
    {
        return registration != nullptr && registration->open;
    }

    bool Editor::keyDown (Key key, std::uint32_t character, int modifiers, bool fromHost)
    {
        return wantsKeys() && registration->key (key, character, modifiers, fromHost);
    }

    void Editor::pasteText (const std::string& text)
    {
        if (wantsKeys())
            registration->insert (text);
    }

    bool Editor::registrationMouseDown (int x, int y, bool fine, bool doubleClick)
    {
        if (registration == nullptr)
            return false;

        auto& r = *registration;
        r.mouseX = (float) x + 0.5f;
        r.mouseY = (float) y + 0.5f;

        if (! r.open)
        {
            if (! r.showsTab() || ! r.tab().contains (r.mouseX, r.mouseY))
                return false;

            openRegistration();
            return true;
        }

        const auto l = r.layout();
        const auto part = l.partAt (r.mouseX, r.mouseY);
        r.pressed = part;
        r.hovered = part;

        if (part == RegistrationLayout::outsidePart)
        {
            // A click beside it closes it (what was typed stays for next time).
            r.pressed = RegistrationLayout::none;
            closeRegistration();
        }
        else if (part == RegistrationLayout::fieldPart)
        {
            if (doubleClick)
            {
                r.anchor = 0;
                r.moveCaret ((int) r.serial.size(), true);
            }
            else
            {
                r.moveCaret (r.caretAt (l, r.mouseX), fine);
                r.selecting = true;
            }
        }

        return true;
    }

    void Editor::drawRegistration (std::uint32_t* pixels)
    {
        if (registration == nullptr)
            return;

        auto& r = *registration;

        // Registered elsewhere (another copy of the plugin, Sonify) shows here soon after.
        if (Clock::now() - r.checked > std::chrono::seconds (2))
            r.refresh();

        Painter painter (pixels, width, height);

        if (r.open && r.items.empty())
            closeRegistration();

        if (r.open)
            r.draw (painter);
        else if (r.showsTab())
            r.drawTab (painter);
    }

    bool Editor::isRegistered (int)
    {
        return registration == nullptr || ! registration->anyUnregistered();
    }
   #else
    void Editor::openRegistration() {}
    void Editor::closeRegistration() {}
    bool Editor::wantsKeys() const noexcept                         { return false; }
    bool Editor::keyDown (Key, std::uint32_t, int, bool)            { return false; }
    void Editor::pasteText (const std::string&) {}
    bool Editor::registrationMouseDown (int, int, bool, bool)       { return false; }
    void Editor::drawRegistration (std::uint32_t*) {}
    bool Editor::isRegistered (int)                                 { return true; }
   #endif
}
