// D:\workspace\Fanan_web_hub\src\KnobManEngine.cpp

/*
    A port of JKnobMan 1.3.3's rendering. JKnobMan: Copyright (c) 2012-2020 g200kg, MIT licence
    (see KnobManEngine.h). The arithmetic follows the original closely, integer truncations
    included, so a knob comes out as KnobMan draws it.
*/

#include "KnobManEngine.h"

#include <charconv>
#include <cmath>
#include <map>

namespace KnobManEngine
{
    //==========================================================================
    // A KnobMan bitmap: straight (not premultiplied) ARGB, as Java's TYPE_INT_ARGB.
    struct Bmp
    {
        int w = 0, h = 0;
        std::vector<juce::uint32> p;

        Bmp() = default;
        Bmp (int width, int height) : w (juce::jmax (0, width)), h (juce::jmax (0, height)), p ((size_t) w * (size_t) h, 0u) {}

        juce::uint32 get (int x, int y) const noexcept      { return p[(size_t) y * (size_t) w + (size_t) x]; }
        void set (int x, int y, juce::uint32 v) noexcept    { p[(size_t) y * (size_t) w + (size_t) x] = v; }
        int alpha (int x, int y) const noexcept             { return (int) (get (x, y) >> 24); }

        void set (int x, int y, int a, int r, int g, int b) noexcept
        {
            set (x, y, (juce::uint32) ((a << 24) + (r << 16) + (g << 8) + b));
        }

        void clear() noexcept { std::fill (p.begin(), p.end(), 0u); }
    };

    //==========================================================================
    // KnobMan's colour, with its HLS (0..240) maths.
    struct Col
    {
        int a = 255, r = 0, g = 0, b = 0, h = 0, l = 0, s = 0, cnt = 0;

        Col() = default;
        Col (int rr, int gg, int bb) : r (rr), g (gg), b (bb) {}
        Col (int rr, int gg, int bb, int aa) : a (aa), r (rr), g (gg), b (bb) {}

        static int htorgb (int n1, int n2, int hh)
        {
            while (hh < 0)   hh += 240;
            while (hh > 240) hh -= 240;

            if (hh < 40)  return n1 + ((n2 - n1) * hh + 20) / 40;
            if (hh < 120) return n2;
            if (hh < 160) return n1 + ((n2 - n1) * (160 - hh) + 20) / 40;
            return n1;
        }

        void setHls (int hh, int ll, int ss)
        {
            h = hh; l = ll; s = ss;
            ll = juce::jlimit (0, 240, ll);

            if (ss > 240)
                ss = 240;

            if (ss <= 0)
            {
                g = b = ll * 255 / 240;
                r = b;
            }
            else
            {
                const int tmp2 = ll <= 120 ? (ll * (240 + ss) + 120) / 240 : ll + ss - (ll * ss + 120) / 240;
                const int tmp1 = 2 * ll - tmp2;
                r = (htorgb (tmp1, tmp2, hh + 80) * 255 + 120) / 240;
                g = (htorgb (tmp1, tmp2, hh) * 255 + 120) / 240;
                b = (htorgb (tmp1, tmp2, hh - 80) * 255 + 120) / 240;
            }
        }

        void setRgb (int rr, int gg, int bb)
        {
            const int cmax = std::max (std::max (rr, gg), bb);
            const int cmin = std::min (std::min (rr, gg), bb);
            r = rr; g = gg; b = bb;
            l = ((cmax + cmin) * 240 + 255) / 510;

            if (cmax == cmin)
            {
                s = 0;
                h = 0;
            }
            else
            {
                s = l < 120 ? ((cmax - cmin) * 240 + (cmax + cmin) / 2) / (cmax + cmin)
                            : ((cmax - cmin) * 240 + (510 - cmax - cmin) / 2) / (510 - cmax - cmin);
                const int dr = ((cmax - r) * 40 + (cmax - cmin) / 2) / (cmax - cmin);
                const int dg = ((cmax - g) * 40 + (cmax - cmin) / 2) / (cmax - cmin);
                const int db = ((cmax - b) * 40 + (cmax - cmin) / 2) / (cmax - cmin);
                h = r == cmax ? db - dg : (g == cmax ? 80 + dr - db : 160 + dg - dr);

                if (h < 0)   h += 240;
                if (h > 240) h -= 240;
            }
        }

        void clearAll() noexcept { cnt = 0; a = r = g = b = 0; }

        void changeBrightness (int n) noexcept
        {
            r = juce::jlimit (0, 255, r + n);
            g = juce::jlimit (0, 255, g + n);
            b = juce::jlimit (0, 255, b + n);
        }

        void changeBrightness (double n) noexcept { changeBrightness ((int) n); }

        void blendTo (int rr, int gg, int bb, int aa) noexcept
        {
            r = (r * (256 - aa) + rr * aa) / 256;
            g = (g * (256 - aa) + gg * aa) / 256;
            b = (b * (256 - aa) + bb * aa) / 256;
        }

        void bright (int aa) noexcept
        {
            r = r * aa / 255;
            g = g * aa / 255;
            b = b * aa / 255;
        }

        void copy (const Col& c) noexcept { r = c.r; g = c.g; b = c.b; a = c.a; }

        void blend4 (const Col& c01, const Col& c10, const Col& c11, double x, double y) noexcept
        {
            const double x0 = 1.0 - x, y0 = 1.0 - y;
            double xy00 = x0 * y0 * a, xy01 = x * y0 * c01.a, xy10 = x0 * y * c10.a, xy11 = x * y * c11.a;
            const double at = xy00 + xy01 + xy10 + xy11;

            if (at != 0.0)
            {
                xy00 /= at; xy01 /= at; xy10 /= at; xy11 /= at;
            }
            else
            {
                xy00 = xy01 = xy10 = xy11 = 0.0;
            }

            const double rr = r * xy00 + c01.r * xy01 + c10.r * xy10 + c11.r * xy11;
            const double gg = g * xy00 + c01.g * xy01 + c10.g * xy10 + c11.g * xy11;
            const double bb = b * xy00 + c01.b * xy01 + c10.b * xy10 + c11.b * xy11;
            const double aa = a * x0 * y0 + c01.a * x * y0 + c10.a * x0 * y + c11.a * x * y;
            a = (int) aa; r = (int) rr; g = (int) gg; b = (int) bb;
        }

        void conv (double br, double c, double sat, double hue)
        {
            hue *= 0.6666666666666666;
            setRgb (r, g, b);
            l = (int) ((double) (l - 120) * (c + 100.0) / 100.0 + 120.0);
            l = (int) ((double) l + br * 240.0 / 100.0);

            if (l >= 240) l = 239;
            if (l < 0)    l = 0;

            s = (int) ((double) s * (sat + 100.0) / 100.0);
            h = (int) ((double) h + hue);
            setHls (h, l, s);
        }

        void getPix (const Bmp& img, int x, int y) noexcept
        {
            const auto v = img.get (x, y);
            a = (int) (v >> 24 & 0xff); r = (int) (v >> 16 & 0xff); g = (int) (v >> 8 & 0xff); b = (int) (v & 0xff);
        }

        void setPix (Bmp& img, int x, int y) const noexcept { img.set (x, y, a, r, g, b); }

        void addPix (Bmp& img, int x, int y) const noexcept
        {
            const auto vald = img.get (x, y);
            const int a0 = (int) (vald >> 24 & 0xff);

            if (a >= 255 || a0 == 0)
            {
                img.set (x, y, a, r, g, b);
                return;
            }

            const int r0 = (int) (vald >> 16 & 0xff), g0 = (int) (vald >> 8 & 0xff), b0 = (int) (vald & 0xff);
            const int na = 255 - (255 - a0) * (255 - a) / 255;
            const int aa = a0 * (255 - a) + a * 255;

            if (aa == 0)
                return;

            img.set (x, y, na, (r0 * a0 * (255 - a) + r * a * 255) / aa,
                               (g0 * a0 * (255 - a) + g * a * 255) / aa,
                               (b0 * a0 * (255 - a) + b * a * 255) / aa);
        }

        void mulAddPix (Bmp& img, const Bmp& mask, int dx, int dy, int mx, int my) noexcept
        {
            a = a * mask.alpha (mx, my) / 255;
            addPix (img, dx, dy);
        }

        void addVal (const Bmp& img, int x, int y) noexcept
        {
            const auto v = img.get (x, y);
            a += (int) (v >> 24 & 0xff); r += (int) (v >> 16 & 0xff); g += (int) (v >> 8 & 0xff); b += (int) (v & 0xff);
            ++cnt;
        }

        void setAve (Bmp& img, int x, int y) const noexcept
        {
            img.set (x, y, a / cnt, r / cnt, g / cnt, b / cnt);
        }

        void setAve() noexcept
        {
            if (cnt != 0)
            {
                a /= cnt; r /= cnt; g /= cnt; b /= cnt;
            }
        }
    };

    //==========================================================================
    // A bump texture: its luminance and alpha, ready at both zoom steps KnobMan uses.
    struct Tex
    {
        int width = 0, height = 0;
        std::vector<int> zp[2], za[2];
        int zw[2] {}, zh[2] {};

        explicit Tex (const Bmp& bmp)
        {
            width = bmp.w;
            height = bmp.h;

            if (width <= 0 || height <= 0)
                return;

            const int stride = width + 2;
            std::vector<int> pdata ((size_t) stride * (size_t) (height + 2)), adata (pdata.size());

            for (int yy = 0; yy < height + 2; ++yy)
                for (int xx = 0; xx < width + 2; ++xx)
                {
                    const auto c = bmp.get (xx % width, yy % height);
                    const int lum = ((int) (c >> 16 & 0xff) * 3 + (int) (c >> 8 & 0xff) * 6 + (int) (c & 0xff)) / 10;
                    pdata[(size_t) (yy * stride + xx)] = std::min (255, lum);
                    adata[(size_t) (yy * stride + xx)] = (int) (c >> 24 & 0xff);
                }

            for (int k = 0; k < 2; ++k)
            {
                const int r = k + 1;
                zw[k] = width / r;
                zh[k] = height / r;
                zp[k].assign ((size_t) (zw[k] + 1) * (size_t) (zh[k] + 1), 0);
                za[k].assign (zp[k].size(), 0);

                for (int yy = 0; yy < zh[k] + 1; ++yy)
                    for (int xx = 0; xx < zw[k] + 1; ++xx)
                    {
                        const auto ind = (size_t) (yy * (zw[k] + 1) + xx);
                        const int ind2 = yy * r * stride + xx * r;

                        if (r == 1)
                        {
                            zp[k][ind] = pdata[(size_t) ind2];
                            za[k][ind] = adata[(size_t) ind2];
                        }
                        else
                        {
                            zp[k][ind] = (pdata[(size_t) ind2] + pdata[(size_t) ind2 + 1] + pdata[(size_t) (ind2 + stride)] + pdata[(size_t) (ind2 + stride + 1)]) / 4;
                            za[k][ind] = (adata[(size_t) ind2] + adata[(size_t) ind2 + 1] + adata[(size_t) (ind2 + stride)] + adata[(size_t) (ind2 + stride + 1)]) / 4;
                        }
                    }
            }
        }

        juce::uint32 get (double x, double y, double z) const noexcept
        {
            const int r = z <= 50.0 ? 2 : 1;
            const int k = r - 1;

            if (z == 0.0 || width == 0 || zw[k] <= 0 || zh[k] <= 0)
                return 0xff0000ffu;

            z *= r;
            x = (x + 0.5) * 100.0 / z + zw[k] * 0.5;
            y = (y + 0.5) * 100.0 / z + zh[k] * 0.5;
            x -= std::floor (x / zw[k]) * zw[k];
            y -= std::floor (y / zh[k]) * zh[k];
            const int ix = juce::jlimit (0, zw[k] - 1, (int) x), iy = juce::jlimit (0, zh[k] - 1, (int) y);
            const double fx = x - ix, fy = y - iy;
            const int s = zw[k] + 1;
            const auto& P = zp[k];
            const auto& A = za[k];
            const double p0 = P[(size_t) (iy * s + ix)] * (1.0 - fx) + P[(size_t) (iy * s + ix + 1)] * fx;
            const double p1 = P[(size_t) ((iy + 1) * s + ix)] * (1.0 - fx) + P[(size_t) ((iy + 1) * s + ix + 1)] * fx;
            const double a0 = A[(size_t) (iy * s + ix)] * (1.0 - fx) + A[(size_t) (iy * s + ix + 1)] * fx;
            const double a1 = A[(size_t) ((iy + 1) * s + ix)] * (1.0 - fx) + A[(size_t) ((iy + 1) * s + ix + 1)] * fx;
            const int pp = (int) (p0 * (1.0 - fy) + p1 * fy);
            const int aa = (int) (a0 * (1.0 - fy) + a1 * fy);
            return (juce::uint32) ((aa << 24) + pp);
        }
    };

    //==========================================================================
    struct Prim
    {
        int type = 0;
        Col color { 255, 0, 0 };
        double aspect = 0, round = 0, width = 10, length = 50, step = 20, anglestep = 45, emboss = 0, embossdiffuse = 0,
               ambient = 50, lightdir = -50, specular = 0, specularwidth = 50, texturedepth = 0, texturezoom = 100,
               diffuse = 0, fontsize = 50;
        int transparent = 0, textalign = 0, framealign = 0, autofit = 1, bold = 0, italic = 0, fill = 1,
            intellialpha = 0, numframe = 1;
        juce::String text, shape, font, file, textureName;
        std::shared_ptr<const Bmp> image;
        std::shared_ptr<const Tex> tex;
        std::shared_ptr<const std::string> texHex, imgHex;   // the embedded texture / picture as the file has it (hex), to write it again

        bool animates() const noexcept { return (type == 1 && numframe > 1) || type == 14; }
    };

    struct Eff
    {
        int antialias = 1, unfold = 0, animstep = 0, zoomxysepa = 0, keepdir = 0;
        double zoomxf = 100, zoomxt = 100, zoomyf = 100, zoomyt = 100, offxf = 0, offxt = 0, offyf = 0, offyt = 0,
               centerx = 0, centery = 0, anglef = 0, anglet = 0, alphaf = 100, alphat = 100, brightf = 0, brightt = 0,
               contrastf = 0, contrastt = 0, saturationf = 0, saturationt = 0, huef = 0, huet = 0;
        int zoomxanim = 0, zoomyanim = 0, offxanim = 0, offyanim = 0, angleanim = 0, alphaanim = 0, brightanim = 0,
            contrastanim = 0, saturationanim = 0, hueanim = 0;
        int mask1ena = 0, mask1type = 0, mask1graddir = 0, mask1startanim = 0, mask1stopanim = 0;
        double mask1grad = 0, mask1startf = -140, mask1startt = -140, mask1stopf = 140, mask1stopt = 140;
        int mask2ena = 0, mask2op = 0, mask2type = 0, mask2graddir = 0, mask2startanim = 0, mask2stopanim = 0;
        double mask2grad = 0, mask2startf = -140, mask2startt = -140, mask2stopf = 140, mask2stopt = 140;
        int fmaskena = 0;
        double fmaskstart = 0, fmaskstop = 100;
        juce::String fmaskbits;
        double slightdirf = -45, slightdirt = -45, sdensityf = 0, sdensityt = 0;
        int slightdiranim = 0, sdensityanim = 0;
        int dlightdirena = 0, dlightdiranim = 0, doffsetanim = 0, ddensityanim = 0, ddiffuseanim = 0, dstype = 0;
        double dlightdirf = -45, dlightdirt = -45, doffsetf = 5, doffsett = 5, ddensityf = 0, ddensityt = 0,
               ddiffusef = 0, ddiffuset = 0, dsgrad = 100;
        int ilightdirena = 0, ilightdiranim = 0, ioffsetanim = 0, idensityanim = 0, idiffuseanim = 0;
        double ilightdirf = -45, ilightdirt = -45, ioffsetf = 5, ioffsett = 5, idensityf = 0, idensityt = 0,
               idiffusef = 20, idiffuset = 20;
        int elightdirena = 0, elightdiranim = 0, eoffsetanim = 0, edensityanim = 0;
        double elightdirf = -45, elightdirt = -45, eoffsetf = 0, eoffsett = 0, edensityf = 0, edensityt = 0;
    };

    struct Layer
    {
        bool visible = true;    // drawn (its own Visible, or Solo when any layer is soloed)
        int shown = 1, solo = 0;  // as the file has them (Visible1_n / Visible, VisibleSolo)
        juce::String name;
        Prim prim;
        Eff eff;
    };

    // The primitives as drawn at one canvas size (for those that don't change frame to frame).
    struct Made;

    struct Cache
    {
        struct Entry
        {
            std::vector<std::shared_ptr<const Bmp>> prims;    // primitives that don't change
            std::vector<std::shared_ptr<const Made>> made;    // whole layers that don't change (made when first needed)
            std::vector<bool> madeYet;
        };

        std::mutex lock;
        std::map<juce::int64, std::shared_ptr<Entry>> drawn;
    };

    //==========================================================================
    // The document's text, read the way JKnobMan's ProfileReader reads it: a key is looked for
    // from its section's start to the end of the file (so a key a section lacks may come from a
    // later one), the first match wins.
    class Reader
    {
    public:
        explicit Reader (const juce::String& text)
        {
            auto* start = text.getCharPointer().getAddress();
            const auto* end = start + text.getNumBytesAsUTF8();
            const char* lineStart = start;

            for (const char* c = start; ; ++c)
            {
                if (c == end || *c == '\n')
                {
                    const char* lineEnd = c;

                    if (lineEnd > lineStart && lineEnd[-1] == '\r')
                        --lineEnd;

                    lines.emplace_back (lineStart, (size_t) (lineEnd - lineStart));

                    if (c == end)
                        break;

                    lineStart = c + 1;
                }
            }
        }

        void setSection (const std::string& name)
        {
            const auto head = "[" + name + "]";

            for (size_t i = 0; i < lines.size(); ++i)
                if (lines[i].compare (0, head.size(), head) == 0)
                {
                    section = i + 1;
                    return;
                }
        }

        bool find (const std::string& key, std::string& value, size_t* at = nullptr) const
        {
            const auto prefix = key + "=";

            for (size_t i = section; i < lines.size(); ++i)
                if (lines[i].compare (0, prefix.size(), prefix) == 0)
                {
                    value = lines[i].substr (prefix.size());

                    if (at != nullptr)
                        *at = i;

                    return true;
                }

            return false;
        }

        juce::String readString (const std::string& key, const juce::String& fallback) const
        {
            std::string v;
            return find (key, v) ? juce::String::fromUTF8 (v.c_str(), (int) v.size()) : fallback;
        }

        int readInt (const std::string& key, int fallback) const
        {
            std::string v;

            if (! find (key, v))
                return fallback;

            // Integer.parseInt: a sign and digits only; anything else reads as 1.
            size_t i = 0;
            bool negative = false;

            if (i < v.size() && (v[i] == '-' || v[i] == '+'))
                negative = v[i++] == '-';

            if (i >= v.size())
                return 1;

            long long n = 0;

            for (; i < v.size(); ++i)
            {
                if (v[i] < '0' || v[i] > '9')
                    return 1;

                n = n * 10 + (v[i] - '0');

                if (n > 2147483647LL)
                    return 1;
            }

            return (int) (negative ? -n : n);
        }

        double readFloat (const std::string& key, double fallback) const
        {
            std::string v;

            if (! find (key, v))
                return fallback;

            for (auto& ch : v)
                if (ch == ',')
                    ch = '.';

            const auto text = juce::String (v).trim();

            if (text.isEmpty() || ! (text.containsOnly ("0123456789+-.eE") ))
                return fallback;   // (Java would stop the whole load here)

            return text.getDoubleValue();
        }

        /** ExtractFile: hex lines from key0 on, until a section or a short line. */
        std::vector<juce::uint8> extractFile (const std::string& base, std::string* hexOut = nullptr) const
        {
            std::vector<juce::uint8> data;
            std::string value;
            size_t at = 0;

            if (! find (base + "0", value, &at) || value.empty())
                return data;

            for (size_t line = at; ; )
            {
                if (value.empty() || value[0] == '[')
                    break;

                const auto n = value.size();

                if (hexOut != nullptr)
                    hexOut->append (value);

                for (size_t j = 0; j + 1 < n; j += 2)
                {
                    const auto hex = [] (char ch) { return ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10
                                                          : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1; };
                    const int hi = hex (value[j]), lo = hex (value[j + 1]);
                    data.push_back ((juce::uint8) (hi < 0 || lo < 0 ? 0 : (hi << 4 | lo)));
                }

                if (n < 512)
                    break;

                if (++line >= lines.size())
                    break;

                const auto& next = lines[line];
                const auto eq = next.find ('=');
                value = eq == std::string::npos ? next : next.substr (eq + 1);
            }

            return data;
        }

    private:
        std::vector<std::string> lines;
        size_t section = 0;
    };

    //==========================================================================
    std::shared_ptr<const Bmp> decodeImage (const std::vector<juce::uint8>& bytes)
    {
        if (bytes.empty())
            return nullptr;

        auto image = juce::ImageFileFormat::loadFrom (bytes.data(), bytes.size());

        if (! image.isValid())
            return nullptr;

        image = image.convertedToFormat (juce::Image::ARGB);
        auto bmp = std::make_shared<Bmp> (image.getWidth(), image.getHeight());
        const juce::Image::BitmapData data (image, juce::Image::BitmapData::readOnly);

        for (int y = 0; y < bmp->h; ++y)
            for (int x = 0; x < bmp->w; ++x)
            {
                const auto c = data.getPixelColour (x, y);   // unpremultiplied
                bmp->set (x, y, c.getAlpha(), c.getRed(), c.getGreen(), c.getBlue());
            }

        return bmp;
    }

    //==========================================================================
    // Java's getScaledInstance (SCALE_SMOOTH): an area average of each channel.
    void areaScale (const Bmp& src, int sx, int sy, int sw, int sh, Bmp& dst, int dx, int dy, int dw, int dh)
    {
        if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
            return;

        for (int y = 0; y < dh; ++y)
        {
            const double y0 = (double) y * sh / dh, y1 = (double) (y + 1) * sh / dh;

            for (int x = 0; x < dw; ++x)
            {
                const double x0 = (double) x * sw / dw, x1 = (double) (x + 1) * sw / dw;
                double sa = 0, sr = 0, sg = 0, sb = 0, total = 0;

                for (int yy = (int) std::floor (y0); yy < (int) std::ceil (y1); ++yy)
                {
                    const double wy = std::min (y1, (double) yy + 1) - std::max (y0, (double) yy);

                    if (wy <= 0 || sy + yy < 0 || sy + yy >= src.h)
                        continue;

                    for (int xx = (int) std::floor (x0); xx < (int) std::ceil (x1); ++xx)
                    {
                        const double wx = std::min (x1, (double) xx + 1) - std::max (x0, (double) xx);

                        if (wx <= 0 || sx + xx < 0 || sx + xx >= src.w)
                            continue;

                        const auto c = src.get (sx + xx, sy + yy);
                        const double wgt = wx * wy;
                        sa += (c >> 24 & 0xff) * wgt; sr += (c >> 16 & 0xff) * wgt; sg += (c >> 8 & 0xff) * wgt; sb += (c & 0xff) * wgt;
                        total += wgt;
                    }
                }

                if (dx + x < 0 || dx + x >= dst.w || dy + y < 0 || dy + y >= dst.h)
                    continue;

                if (total <= 0)
                {
                    dst.set (dx + x, dy + y, 0u);
                    continue;
                }

                const int a = juce::jlimit (0, 255, (int) (sa / total + 0.5));
                dst.set (dx + x, dy + y, a == 0 ? 0u : (juce::uint32) ((a << 24) + (juce::jlimit (0, 255, (int) (sr / total + 0.5)) << 16)
                                                                     + (juce::jlimit (0, 255, (int) (sg / total + 0.5)) << 8)
                                                                     + juce::jlimit (0, 255, (int) (sb / total + 0.5))));
            }
        }
    }

    // A JUCE drawing (premultiplied) back into a straight bitmap.
    void fromJuceImage (const juce::Image& image, Bmp& out)
    {
        const juce::Image::BitmapData data (image, juce::Image::BitmapData::readOnly);

        for (int y = 0; y < out.h && y < image.getHeight(); ++y)
            for (int x = 0; x < out.w && x < image.getWidth(); ++x)
            {
                const auto c = data.getPixelColour (x, y);
                out.set (x, y, c.getAlpha(), c.getRed(), c.getGreen(), c.getBlue());
            }
    }

    //==========================================================================
    // KnobMan's text with counters: "(1:99)", "a,b,c", "(0:10:0.5:%.1f)" and the like.
    class DynamicText
    {
    public:
        explicit DynamicText (const juce::String& s) : str (s.toStdString()) { total = count(); }

        juce::String get (int frame, int maxframe)
        {
            return juce::String (maxframe <= 1 ? getItem (0) : getItem ((int) ((long long) total * frame / (maxframe - 1))));
        }

    private:
        std::string str, fmt = "%d";
        int p = 0, pi = 0, total = 0, fx = 0;
        double pn1 = 0, pn2 = 0, pn3 = 0;

        char at (int i) const noexcept { return i >= 0 && i < (int) str.size() ? str[(size_t) i] : '\0'; }
        int skip (int i) const noexcept { while (at (i) == ' ') ++i; return i; }
        static bool digit (char c) noexcept { return c >= '0' && c <= '9'; }
        bool startsAt (const char* w, int i) const { return str.compare ((size_t) i, std::strlen (w), w) == 0; }

        double eval0 (int i, double v)
        {
            switch (at (i))
            {
                case 'x': p = i + 1; return v;
                case '(': { const double d = eval (i + 1, v); if (at (p) == ')') ++p; return d; }
                case '-': return -eval0 (i + 1, v);
                case '+': return eval0 (i + 1, v);
                default:  break;
            }

            return getNum (i);
        }

        double eval1 (int i, double v)
        {
            const auto func = [&] (const char* name, int len, auto f)
            {
                const double d = eval (i + len, v);
                if (at (p) == ')') ++p;
                return f (d);
            };

            if (startsAt ("pow(", i))
            {
                double d = eval (i + 4, v);
                if (at (p) == ',') ++p;
                d = std::pow (d, eval (p, v));
                if (at (p) == ')') ++p;
                return d;
            }

            if (startsAt ("exp(", i))   return func ("exp(", 4, [] (double d) { return std::exp (d); });
            if (startsAt ("log(", i))   return func ("log(", 4, [] (double d) { return std::log (d); });
            if (startsAt ("log10(", i)) return func ("log10(", 6, [] (double d) { return 0.4342944819032518 * std::log (d); });
            if (startsAt ("sqrt(", i))  return func ("sqrt(", 5, [] (double d) { return std::sqrt (d); });
            return eval0 (i, v);
        }

        double eval2 (int i, double v)
        {
            double d = eval1 (i, v);

            for (;;)
            {
                if (at (p) == '*')      d *= eval1 (p + 1, v);
                else if (at (p) == '/') d /= eval1 (p + 1, v);
                else break;
            }

            return d;
        }

        double eval (int i, double v)
        {
            double d = eval2 (i, v);

            for (;;)
            {
                if (at (p) == '+')      d += eval2 (p + 1, v);
                else if (at (p) == '-') d -= eval2 (p + 1, v);
                else break;
            }

            return d;
        }

        std::string sprintfValue (double v)
        {
            if (fx != 0)
                v = eval (fx, v);

            std::string out;
            int w = 0;
            bool zero = false, plus = false;

            for (size_t i = 0; i < fmt.size(); ++i)
            {
                if (fmt[i] != '%')
                {
                    out += fmt[i];
                    continue;
                }

                std::string sv;

                if (++i >= fmt.size()) break;
                if (fmt[i] == '+') { if (++i >= fmt.size()) break; if (v >= 0.0) plus = true; }
                if (fmt[i] == '0') { zero = true; if (++i >= fmt.size()) break; }
                if (digit (fmt[i])) { w = fmt[i] - '0'; if (++i >= fmt.size()) break; }
                int precision = 6;

                if (fmt[i] == '.') { if (++i >= fmt.size()) break; if (digit (fmt[i])) precision = fmt[i++] - '0'; }
                if (i >= fmt.size()) break;

                char buffer[64] {};

                switch (fmt[i])
                {
                    case 'd': sv = std::to_string ((int) v); break;
                    case 'f': std::snprintf (buffer, sizeof (buffer), "%.*f", precision, v); sv = buffer; break;   // (never the text's own format: it may hold more than one value)
                    case 'x': std::snprintf (buffer, sizeof (buffer), "%x", (unsigned) (int) v); sv = buffer; break;
                    case 'X': std::snprintf (buffer, sizeof (buffer), "%X", (unsigned) (int) v); sv = buffer; break;
                    default: break;
                }

                if (w != 0)
                {
                    const auto padded = std::string (zero ? "00000000" : "        ") + sv;
                    sv = padded.substr (padded.size() - (size_t) std::min ((int) padded.size(), w));
                }

                if (plus)
                    sv = "+" + sv;

                out += sv;
            }

            return out;
        }

        double getNum (int i)
        {
            double sign = 1.0, frac = 0.0;
            int n1 = 0;

            if (at (i = skip (i)) == '-') { sign = -1.0; ++i; }

            i = skip (i);

            while (digit (at (i))) { n1 = n1 * 10 + at (i) - '0'; ++i; }

            if (at (i) == '.')
            {
                double dp = 0.1;

                while (digit (at (++i))) { frac += dp * (at (i) - '0'); dp *= 0.1; }
            }

            p = skip (i);
            return sign * (n1 + frac);
        }

        int checkNum (int i)
        {
            fx = 0;
            fmt = "%d";
            const double r1 = getNum (++i);

            if (at (p) != ':')
                return -1;

            const double r2 = getNum (p + 1);
            double r3 = 1.0;

            if (at (p) == ':')
            {
                if ((digit (at (p + 1)) || at (p + 1) == '.') && (r3 = getNum (p + 1)) == 0.0)
                    r3 = 1.0;

                if (at (p) == ':')
                {
                    ++p;
                    fmt.clear();

                    while (at (p) != ')' && at (p) != ':' && p < (int) str.size())
                        fmt += at (p++);
                }

                if (at (p) == ':')
                {
                    ++p;
                    fx = p;
                    int paren = 0;

                    while (p < (int) str.size() && at (p) != ':')
                    {
                        if (at (p) == '(') ++paren;
                        if (at (p) == ')' && --paren < 0) break;
                        ++p;
                    }
                }
            }

            if (at (p) != ')')
                return -1;

            pn1 = r1; pn2 = r2; pn3 = r3;
            return (int) (std::abs (r1 - r2) / r3);
        }

        int count()
        {
            int n = 1;

            for (int i = 0; i < (int) str.size(); ++i)
            {
                if (at (i) == ',') { ++n; continue; }

                int j;

                if (at (i) != '(' || (j = checkNum (i)) <= 0)
                    continue;

                i = p;
                n += j;
            }

            return n;
        }

        std::string getNext (int i)
        {
            std::string temp;
            const int start = i;

            for (;;)
            {
                int n;

                if (at (i) == '(' && (n = checkNum (i)) >= 0)
                {
                    temp += sprintfValue (pn2 >= pn1 ? pn1 + pi * pn3 : pn1 - pi * pn3);
                    ++pi;
                    ++p;

                    while (p < (int) str.size() && str[(size_t) p] != ',')
                        temp += str[(size_t) p++];

                    if (pi > n)
                    {
                        pi = 0;
                        p = skip (p);

                        if (p < (int) str.size() && str[(size_t) p] == ',')
                            ++p;
                    }
                    else
                    {
                        p = start;
                    }

                    return temp;
                }

                if (i >= (int) str.size())
                    break;

                const char c = str[(size_t) i++];

                if (c == ',')
                    break;

                temp += c;
            }

            p = i;
            return temp;
        }

        std::string getItem (int n)
        {
            int i = 0;
            pi = 0;
            std::string item;

            if (n >= total)
                n = total - 1;

            while (n >= 0)
            {
                item = getNext (i);
                i = p;
                --n;
            }

            return item;
        }
    };

    //==========================================================================
    // The primitives: each fills a bitmap the canvas's size (an image not fitted: its own size).
    struct Painter
    {
        const Prim& pr;
        int W, H;
        double imageScale;   // a bigger render: an image not fitted grows with it
        Bmp& bmp;

        Col base() const { return pr.color; }

        int texLumi (double px, double py, double txd, int& alpha) const
        {
            alpha = 255;

            if (pr.tex == nullptr)
                return 0;

            const auto pix = pr.tex->get (px, py, pr.texturezoom);
            alpha = 255 - (int) ((double) (255 - (int) (pix >> 24 & 0xff)) * txd);
            return (int) ((double) ((int) (pix & 0xff) - 128) * txd);
        }

        void none() { bmp.clear(); }

        void image (int frame, int total)
        {
            bmp.clear();
            const int n = pr.numframe;

            if (n == 0 || pr.image == nullptr || pr.image->w <= 0 || pr.image->h <= 0)
                return;

            int offX = 0, offY = 0, isw = pr.image->w, ish = pr.image->h;

            if (total > 1)
            {
                const int which = std::min (n - 1, n * frame / (total - 1));

                if (pr.framealign == 1) { isw /= n; offX = isw * which; }
                else                    { ish /= n; offY = ish * which; }
            }

            Bmp b2 (*pr.image);
            imageAlpha (b2, (int) b2.get (0, 0));

            if (pr.autofit != 0)
            {
                areaScale (b2, offX, offY, isw, ish, bmp, 0, 0, W, H);
            }
            else if (std::abs (imageScale - 1.0) < 1.0e-6)
            {
                for (int y = 0; y < ish && y < bmp.h; ++y)
                    for (int x = 0; x < isw && x < bmp.w; ++x)
                        if (offX + x < b2.w && offY + y < b2.h)
                            bmp.set (x, y, b2.get (offX + x, offY + y));
            }
            else
            {
                areaScale (b2, offX, offY, isw, ish, bmp, 0, 0, bmp.w, bmp.h);
            }
        }

        void imageAlpha (Bmp& b, int defColour) const
        {
            if (pr.transparent == 0 || b.w == 0 || b.h == 0)
                return;

            if (pr.transparent == 1)
            {
                for (auto& v : b.p) v |= 0xff000000u;
                return;
            }

            int step = 0;

            if (pr.intellialpha != 0 && (step = 255 - pr.intellialpha * 254 / 100) < 16)
                step = 16;

            if (step != 0)
            {
                const auto pix = [step] (int dwDefault, int dwTarget) -> juce::uint32
                {
                    dwDefault &= 0xffffff;
                    dwTarget &= 0xffffff;

                    if (dwDefault == dwTarget)
                        return (juce::uint32) dwDefault;

                    const int rd = dwDefault >> 16 & 0xff, gd = dwDefault >> 8 & 0xff, bd = dwDefault & 0xff;
                    const int rt = dwTarget >> 16 & 0xff, gt = dwTarget >> 8 & 0xff, bt = dwTarget & 0xff;
                    int alpha = 0, r, g, bb;
                    double a;

                    do
                    {
                        if ((alpha += step) >= 255) { alpha = 255; a = 1.0; }
                        else a = alpha / 255.0;

                        r = (int) ((rt - rd * (1.0 - a)) / a);
                        g = (int) ((gt - gd * (1.0 - a)) / a);
                        bb = (int) ((bt - bd * (1.0 - a)) / a);
                    }
                    while (! (a >= 1.0) && (r < 0 || r > 255 || g < 0 || g > 255 || bb < 0 || bb > 255));

                    return (juce::uint32) (alpha << 24 | (r & 0xff) << 16 | (g & 0xff) << 8 | (bb & 0xff));
                };

                for (int y = 0; y < b.h; ++y)
                    for (int x = 0; x < b.w; ++x)
                        b.set (x, y, pix (defColour, (int) b.get (x, y)));
            }
            else
            {
                for (auto& v : b.p)
                    v = (int) v == defColour ? 0u : (v | 0xff000000u);
            }
        }

        void rect()
        {
            const double cx = W * 0.5, cy = H * 0.5;
            double xro = cx + 0.5, yro = cy + 0.5;

            if (pr.aspect > 0.0) xro = xro * (100.0 - std::min (pr.aspect, 99.0)) / 100.0;
            if (pr.aspect < 0.0) yro = yro * (100.0 + std::max (pr.aspect, -99.0)) / 100.0;

            const double width = std::min (cx, cy) * pr.width / 100.0 + 1.0;
            const double width2 = width * 0.5;
            const double d = width2 * pr.diffuse / 100.0;
            const double xrc = xro - width2, yrc = yro - width2, xri = xro - width, yri = yro - width;
            const double roundO = pr.round * std::min (cx, cy) / 100.0, roundI = roundO - width;
            const double xcc = xro - roundO, ycc = yro - roundO;

            for (int y = 0; y < H; ++y)
            {
                const double ry = -((double) y + 0.5 - cy), ya = std::abs (ry);

                for (int x = 0; x < W; ++x)
                {
                    const double rx = -((double) x + 0.5 - cx), xa = std::abs (rx);
                    double alpha = 1.0, spec = width, r;

                    if (xa > xro) alpha = 0.0;
                    else if (xa > xro - 1.0 - d) alpha = (xro - xa) / (1.0 + d);

                    if (ya > yro) alpha = 0.0;
                    else if (ya > yro - 1.0 - d) alpha = alpha * (yro - ya) / (1.0 + d);

                    if (xa >= xcc && ya >= ycc)
                    {
                        const double dx = xa - xcc, dy = ya - ycc;
                        r = std::sqrt (dx * dx + dy * dy);
                        if (r > roundO) alpha = 0.0;
                        if (r > roundO - 1.0 - d) alpha = alpha * (roundO - r) / (1.0 + d);
                        if (r < roundI) alpha = 0.0;
                        if (r < roundI + 1.0) alpha *= r - roundI;
                    }
                    else if (xa < xri && ya < yri)
                    {
                        alpha = 0.0;
                    }
                    else if (xa < xri + 1.0 + d && ya < yri + 1.0 + d)
                    {
                        alpha *= std::max (ya - yri, xa - xri) / (1.0 + d);
                    }

                    if (ya < std::min (yri, ycc))       spec = std::max (0.0, std::abs (xa - xrc));
                    else if (xa < std::min (xri, xcc))  spec = std::max (0.0, std::abs (ya - yrc));
                    else if (xa >= xcc && ya >= ycc)
                    {
                        r = std::sqrt ((xa - xcc) * (xa - xcc) + (ya - ycc) * (ya - ycc));
                        spec = std::abs (r + (width2 - roundO));
                    }
                    else spec = ya - yri > xa - xri ? std::abs (ya - yrc) : std::abs (xa - xrc);

                    const int iSpec = (int) ((1.0 - spec / width2) * 255.0 * pr.specular / 100.0);
                    Col col = base();
                    col.a = (int) (alpha * 255.0);
                    col.changeBrightness (iSpec);
                    col.setPix (bmp, x, y);
                }
            }
        }

        void rectFill()
        {
            const double lz = std::sqrt (1.0 / 3.0), lx = lz, ly = lz;
            const double root2 = std::sqrt (2.0);
            const double mn = std::min (W, H);
            const double rnd = pr.round * mn / 200.0;
            const int em = (int) std::abs (pr.emboss);
            const double edge = (100.0 - pr.embossdiffuse) / 100.0;
            const double cx = W * 0.5, cy = H * 0.5;
            double width = cx, height = cy;

            if (pr.aspect > 0.0) width = width * (100.0 - std::min (pr.aspect, 99.0)) / 100.0;
            if (pr.aspect < 0.0) height = height * (100.0 + std::max (pr.aspect, -99.0)) / 100.0;

            const double xd = width * (100.0 - pr.diffuse) / 100.0, yd = height * (100.0 - pr.diffuse) / 100.0;
            const double xe = width + 1.0 - mn * em / 200.0 * edge, xem = width - mn * em / 200.0;
            const double ye = height + 1.0 - mn * em / 200.0 * edge, yem = height - mn * em / 200.0;
            const double txd = pr.texturedepth * 0.01;

            for (int y = 0; y < H; ++y)
            {
                const double ry = -((double) y + 0.5 - cy), yn = ry / cy, ya = std::abs (ry), py = (double) y - cy + 0.5;

                for (int x = 0; x < W; ++x)
                {
                    const double px = (double) x - cx + 0.5;
                    int alpha;
                    const int lumi = texLumi (px, py, txd, alpha);
                    const double rx = -((double) x + 0.5 - cx), xn = rx / cx, xa = std::abs (rx);

                    if (ya > height || xa > width) alpha = 0;
                    if (xa > xd) alpha = (int) std::max (0.0, (double) alpha * (1.0 - (xa - xd) / (width - xd)));
                    if (ya > yd) alpha = (int) std::max (0.0, (double) alpha * (1.0 - (ya - yd) / (height - yd)));

                    double xr, yr;

                    if (xa > width - rnd && ya > height - rnd)
                    {
                        xr = xa - width + rnd;
                        yr = ya - height + rnd;
                        const double xrm = xr + 1.0, yrm = yr + 1.0, r2 = rnd * rnd;

                        if (xrm * xrm + yrm * yrm >= r2)
                        {
                            if (xr * xr + yr * yr >= r2)
                            {
                                alpha = 0;
                            }
                            else
                            {
                                const double bq = (xr + yr) * 2.0, cq = xr * xr + yr * yr - r2;
                                const double v = (std::sqrt (bq * bq - 8.0 * cq) - bq) * 0.25;
                                alpha = (int) ((double) alpha * v);
                            }
                        }
                    }

                    const double round2 = rnd * std::min (xe, ye) / std::min (width, height);
                    xr = 0.0;
                    yr = 0.0;
                    int mode = 0;
                    double xyr = 0.0;

                    if (xa >= xem) { xr = rx / (xa * root2); yr = 0.0; mode = 1; }
                    if (ya >= yem) { xr = 0.0; yr = ry / (ya * root2); mode = 2; }

                    if (xa >= xem - round2 && ya >= yem - round2)
                    {
                        xr = rx > 0.0 ? xa - xem + round2 : -(xa - xem + round2);
                        yr = ry > 0.0 ? ya - yem + round2 : -(ya - yem + round2);
                        xyr = xr * xr + yr * yr;

                        if (xyr >= round2 * round2)
                        {
                            const double r = std::sqrt (2.0 * xyr);
                            xr /= r;
                            yr /= r;
                            mode = 3;
                        }
                    }

                    Col col = base();
                    col.changeBrightness ((xn + yn) * 128.0 * pr.specular / 100.0 + (double) lumi);

                    if (mode != 0)
                    {
                        const double tz = 1.0 / root2;
                        double r = pr.emboss > 0.0 ? xr * lx + yr * ly + tz * lz : -xr * lx - yr * ly + tz * lz;

                        if (r < 0.0)
                            r = 0.0;

                        Col ce = base();
                        ce.changeBrightness (r * 255.0 - 128.0);
                        double a = 0.0;

                        if (mode == 1)      a = 255.0 * (xa - xem) / (xe - xem);
                        else if (mode == 2) a = 255.0 * (ya - yem) / (ye - yem);
                        else                a = 255.0 * (std::sqrt (xyr) - round2) / (xe - xem);

                        a = std::min (255.0, std::max (0.0, std::abs (a)));
                        col.blendTo (ce.r, ce.g, ce.b, (int) a);
                    }

                    col.a = alpha;
                    col.setPix (bmp, x, y);
                }
            }
        }

        void waveCircle()
        {
            const double stepA = 180.0 / pr.anglestep, depth = pr.width * 0.01;
            const double cx = W * 0.5, cy = H * 0.5;
            double ax = 1.0, ay = 1.0;

            if (pr.aspect > 0.0) ax = (100.0 - std::min (pr.aspect, 99.0)) * 0.01;
            if (pr.aspect < 0.0) ay = (100.0 + std::max (pr.aspect, -99.0)) * 0.01;

            const double mx = std::sqrt (std::min (cx, cy) * std::min (cx, cy));
            const double d = 1.0 - pr.diffuse * 0.01, txd = pr.texturedepth * 0.01;

            for (int y = 0; y < H; ++y)
            {
                const double py = (double) y - cy + 0.5, ry = py / ay;

                for (int x = 0; x < W; ++x)
                {
                    const double px = (double) x - cx + 0.5;
                    int alpha;
                    const double lumi = texLumi (px, py, txd, alpha);
                    const double rx = px / ax;
                    const double th = std::abs (std::atan2 (rx, ry));
                    const double c = std::abs (std::sin (th * stepA));
                    const double rr = std::sqrt (rx * rx + ry * ry);
                    const double mx2 = mx * (1.0 - c * depth);
                    Col col = base();
                    col.changeBrightness ((int) ((-rx / cx - ry / cy) * 128.0 * pr.specular * 0.01 + lumi));

                    if (rr < mx2)
                    {
                        const double m2 = (mx2 - 1.0) * d;

                        if (rr >= m2)
                            alpha = (int) ((double) alpha - (rr - m2) * 255.0 / (mx2 - m2));
                    }
                    else
                    {
                        alpha = 0;
                    }

                    col.a = alpha;
                    col.setPix (bmp, x, y);
                }
            }
        }

        void circle()
        {
            const double cx = W * 0.5, cy = H * 0.5;
            const double minSize = std::min (cx, cy) * pr.width * 0.01;
            double ax = 1.0, ay = 1.0;

            if (pr.aspect > 0.0) ax = 100.0 / (100.0 - std::min (pr.aspect, 99.0));
            if (pr.aspect < 0.0) ay = 100.0 / (100.0 + std::max (pr.aspect, -99.0));

            for (int y = 0; y < H; ++y)
            {
                const double py = -((double) y + 0.5 - cy) * ay;
                const double ry = py / cy, ry2 = ry * ry;
                const double ym = py / (cy - ay), ym2 = ym * ym;
                const double yem = py / std::max (0.1, cy - minSize * ay + ay), yem2 = yem * yem;
                const double yd1 = py / std::max (0.1, cy - minSize * 0.5 * pr.diffuse / 100.0 * ay), yd12 = yd1 * yd1;
                const double yd2 = py / std::max (0.1, cy - minSize * ay + minSize * 0.5 * pr.diffuse / 100.0 * ay), yd22 = yd2 * yd2;
                const double ye = py / std::max (0.1, cy - minSize * ay), ye2 = ye * ye;

                for (int x = 0; x < W; ++x)
                {
                    const double px = -((double) x + 0.5 - cx) * ax;
                    const double rx = px / cx;
                    const double xm = px / (cx - ax);
                    const double xem = px / std::max (0.1, cx - minSize * ax + ax);
                    const double xe = px / std::max (0.1, cx - minSize * ax);
                    const double xd1 = px / std::max (0.1, cx - minSize * 0.5 * pr.diffuse * 0.01 * ax);
                    const double xd2 = px / std::max (0.1, cx - minSize * ax + minSize * 0.5 * pr.diffuse * 0.01 * ax);
                    const double r = rx * rx + ry2, rm = xm * xm + ym2, rem = xem * xem + yem2, re = xe * xe + ye2;
                    const double rd1 = xd1 * xd1 + yd12, rd2 = xd2 * xd2 + yd22;
                    Col col = base();
                    double alpha = 255.0;

                    if (re < 1.0 || r > 1.0)
                    {
                        alpha = 0.0;
                    }
                    else
                    {
                        if (re >= 1.0 && rem <= 1.0) alpha *= (1.0 - re) / (rem - re);
                        if (r < 1.0 && rm >= 1.0)    alpha *= (1.0 - r) / (rm - r);

                        if (rd1 >= 1.0 && r < 1.0)        alpha *= (1.0 - r) / (rd1 - r);
                        else if (re >= 1.0 && rd2 < 1.0)  alpha *= (re - 1.0) / (re - rd2) * (re - 1.0) / (re - rd2);

                        if (pr.specular != 0.0)
                        {
                            const double v1 = 1.0 / std::sqrt (r), v2 = 1.0 / std::sqrt (re);
                            double v = 2.0 * (1.0 - v2) / (v1 - v2);

                            if (v > 1.0)
                                v = 2.0 - v;

                            col.changeBrightness ((int) (v * pr.specular * 2.55));
                        }
                    }

                    bmp.set (x, y, (int) alpha, col.r, col.g, col.b);
                }
            }
        }

        void circleFill (int spectype)
        {
            const double cx = W * 0.5, cy = H * 0.5;
            double ax = cx, ay = cy;

            if (pr.aspect > 0.0) ax = (100.0 - std::min (pr.aspect, 99.0)) / 100.0 * cx;
            if (pr.aspect < 0.0) ay = (100.0 + std::max (pr.aspect, -99.0)) / 100.0 * cy;

            double thick = std::abs (pr.emboss) / 100.0;

            if (thick == 1.0)
                thick = (double) 0.99f;

            const double edge = (100.0 - pr.embossdiffuse) / 100.0;
            const double d = 1.0 - pr.diffuse / 100.0, d2 = d * d;
            const double mn = std::min (ax, ay);
            const double lz = std::sqrt (1.0 / 3.0), lx = lz, ly = lz;
            const double tz = 1.0 / std::sqrt (2.0);
            const int metalAmbient = (int) (pr.ambient * 255.0 / 100.0);
            int metalSpecular = (int) (pr.specular * 255.0 / 100.0);
            double metalWidth = 0.0;

            if (pr.specularwidth == 0.0) metalSpecular = 0;
            else metalWidth = std::pow (1.0 / (pr.specularwidth * 0.01), 3.0);

            double sx = -pr.lightdir, sy = -pr.lightdir, sz = 50.0;
            const double len = std::sqrt (sx * sx + sy * sy + sz * sz);
            sx /= len; sy /= len; sz /= len;
            const double txd = pr.texturedepth * 0.01;

            for (int y = 0; y < H; ++y)
            {
                const double py = (double) y - cy + 0.5;
                const double ry = py / (ay + 0.5), rym = py / (ay - 0.5);
                const double rye = py / (ay + 0.5 - mn * thick * edge), ryem = py / (ay - mn * thick);

                for (int x = 0; x < W; ++x)
                {
                    const double px = (double) x - cx + 0.5;
                    const double rx = px / (ax + 0.5), rxy = rx * rx + ry * ry;
                    const double rxm = px / (ax - 0.5), rxym = rxm * rxm + rym * rym;
                    const double rxe = px / (ax + 0.5 - mn * thick * edge), rxem = px / (ax - mn * thick);
                    const double rxye = rxe * rxe + rye * rye, rxyem = rxem * rxem + ryem * ryem;
                    Col col = base();
                    int alpha;
                    const int lumi = texLumi (px, py, txd, alpha);

                    if (rxye < 1.0)
                    {
                        if (spectype == 0)
                        {
                            col.changeBrightness ((int) ((-rx - ry) * 128.0 * pr.specular / 100.0 + (double) lumi));
                        }
                        else if (spectype == 1)
                        {
                            const double th = std::atan2 (ry, rx);
                            const double dd = std::sin (th * 2.0);
                            const double dw = std::pow ((dd + 1.0) * 0.5, metalWidth);
                            const double a = (dd + 1.0) * 0.5 * (double) (255 - metalAmbient) + (double) metalAmbient
                                           + dw * (double) metalSpecular + (double) lumi;
                            col.changeBrightness ((int) (a - 256.0));
                        }
                        else
                        {
                            const double z = std::sqrt (1.0 - rxy);
                            double dd = -rx * sx - ry * sy + z * sz;
                            double a = (double) metalAmbient + (double) (255 - metalAmbient) * dd;
                            dd = 2.0 * z * dd - sz;
                            dd = dd <= 0.0 ? 0.0 : std::exp (std::log (dd) * (110.0 - pr.specularwidth) / 10.0);
                            a += (double) lumi;
                            double bb = (double) metalSpecular * dd;

                            if (bb < 0.0)
                                bb = 0.0;

                            col.bright ((int) a);
                            col.changeBrightness ((int) bb);
                        }
                    }

                    if (rxyem >= 1.0)
                    {
                        const double r2 = std::sqrt (2.0 * rxy);
                        const double tx = -rx / r2, ty = -ry / r2;
                        Col ce = base();

                        if (pr.emboss > 0.0)      ce.changeBrightness ((int) ((tx * lx + ty * ly + tz * lz) * 255.0 - 128.0));
                        else if (pr.emboss < 0.0) ce.changeBrightness ((int) ((-tx * lx - ty * ly + tz * lz) * 255.0 - 128.0));

                        const double a = std::min (255.0, std::max (0.0, 255.0 * std::abs ((1.0 - rxyem) / (rxye - rxyem))));

                        if (rxye < 1.0) col.blendTo (ce.r, ce.g, ce.b, (int) a);
                        else            col.copy (ce);
                    }

                    if (rxy > 1.0) alpha = 0;
                    if (d2 < 1.0 && rxy > d2) alpha = (int) ((double) alpha * (1.0 - (rxy - d2) / (1.0 - d2)));
                    if (rxym >= 1.0) alpha = (int) ((1.0 - rxy) / (rxym - rxy) * (double) alpha);

                    col.a = alpha;
                    col.setPix (bmp, x, y);
                }
            }
        }

        void triangle()
        {
            const double cx = W * 0.5, cy = H * 0.5;
            const double ylen = H * pr.length * 0.01, width = W * pr.width * 0.005;
            const double d = 1.0 - pr.diffuse * 0.01, txd = pr.texturedepth * 0.01;

            for (int y = 0; y < H; ++y)
            {
                const double py = (double) y - cy + 0.5;

                for (int x = 0; x < W; ++x)
                {
                    const double px = (double) x - cx + 0.5;
                    int alpha;
                    const int lumi = texLumi (px, py, txd, alpha);
                    Col col = base();

                    if ((double) y > ylen)
                    {
                        alpha = 0;
                    }
                    else
                    {
                        const double rx = std::abs ((double) x + 0.5 - cx);
                        const double line = width * (double) y / ylen + 1.0;
                        const double line2 = (line - 1.0) * d;

                        if (! (rx < line2))
                            alpha = rx < line ? (int) ((1.0 - (rx - line2) / (line - line2)) * (double) alpha) : 0;

                        int ia = line == 0.0 ? 255 : (int) ((255.0 - rx / line * 255.0) * pr.specular * 0.01);
                        ia += lumi;
                        ia = std::min (254, std::max (0, ia));
                        col.changeBrightness (ia);
                    }

                    col.a = alpha;
                    col.setPix (bmp, x, y);
                }
            }
        }

        void line()
        {
            const double width = W * pr.width / 400.0;
            double leny = H * pr.length / 100.0 - width;

            if (leny < width)
                leny = width;

            const double xc = W * 0.5;
            const double d = 1.0 - pr.diffuse / 100.0;
            const double width2 = (width - 1.0) * d;

            for (int y = 0; y < H; ++y)
            {
                const double ry = (double) y + 0.5;
                const double yp = ry < width ? width : (ry < leny ? ry : leny);
                const double dy = ry - yp;

                for (int x = 0; x < W; ++x)
                {
                    const double dx = (double) x + 0.5 - xc;
                    const double dist = std::sqrt (dx * dx + dy * dy);
                    Col col = base();
                    int alpha;

                    if (dist >= width)
                    {
                        alpha = 0;
                    }
                    else
                    {
                        alpha = 255;
                        const int ia = std::max (0, std::min (255, (int) ((255.0 - dist / width * 255.0) * pr.specular / 100.0)));
                        col.changeBrightness (ia);

                        if (dist >= width2)
                            alpha = (int) (255.0 - 255.0 * (dist - width2) / (width - width2));
                    }

                    col.a = alpha;
                    col.setPix (bmp, x, y);
                }
            }
        }

        static double linePoint (double x0, double y0, double x1, double y1, double px, double py)
        {
            const double dx = x1 - x0, dy = y1 - y0, a = dx * dx + dy * dy;

            if (a == 0.0)
                return std::sqrt ((x0 - px) * (x0 - px) + (y0 - py) * (y0 - py));

            double t = -((dx * (x0 - px) + dy * (y0 - py)) / a);
            t = juce::jlimit (0.0, 1.0, t);
            const double x = t * dx + x0, y = t * dy + y0;
            return std::sqrt ((x - px) * (x - px) + (y - py) * (y - py));
        }

        static void rotate (double& x, double& y, double t)
        {
            const double s = std::sin (t * juce::MathConstants<double>::pi / 180.0);
            const double c = std::cos (t * juce::MathConstants<double>::pi / 180.0);
            const double xx = c * x + s * y;
            y = c * y - s * x;
            x = xx;
        }

        void radiateLine()
        {
            if (W == 0 || H == 0 || pr.anglestep == 0.0 || ! std::isfinite (pr.anglestep))
                return;

            // KnobMan steps from anglestep to 180 by anglestep: below zero it would never get
            // there (KnobMan's own panel stops at 1). Its spokes are mirrored, so a negative step
            // draws as its positive; and never closer than 0.05 degrees (3600 spokes a side).
            const double angleStep = juce::jmax (0.05, std::abs (pr.anglestep));

            const double cx = W * 0.5, cy = H * 0.5;
            const double width = cx * pr.width / 200.0 + 1.0;
            const double d = 1.0 - pr.diffuse / 100.0;
            const double width2 = (width - 1.0) * d;
            double leny = H * pr.length / 200.0;

            if (leny < width)
                leny = width;

            double aspX = 1.0, aspY = 1.0;

            if (pr.aspect > 0.0) aspX = (100.0 - std::min (pr.aspect, 99.0)) / 100.0;
            if (pr.aspect < 0.0) aspY = (100.0 + std::max (pr.aspect, -99.0)) / 100.0;

            aspX *= (double) W / (double) H;

            // The spokes' ends (they're the same for every pixel).
            struct Seg { double x1, y1, x2, y2; };
            std::vector<Seg> segs;
            const double p1y = -cy + width, p2y = -cy + leny;
            segs.push_back ({ 0.0 * aspX + cx, p1y * aspY + cy, 0.0 * aspX + cx, p2y * aspY + cy });

            for (int n = 1; ; ++n)
            {
                const double th = angleStep * n;

                if (th > 180.0)
                    break;

                for (const double sign : { 1.0, -1.0 })
                {
                    double ax = 0.0, ay = p1y, bx = 0.0, by = p2y;
                    rotate (ax, ay, th * sign);
                    rotate (bx, by, th * sign);
                    segs.push_back ({ ax * aspX + cx, ay * aspY + cy, bx * aspX + cx, by * aspY + cy });
                }
            }

            // Each pixel's distance to the nearest spoke: a spoke only reaches the pixels within
            // its width of it, so each is measured over its own box (the same result as measuring
            // every spoke at every pixel, much sooner).
            std::vector<double> nearest ((size_t) W * (size_t) H, 1.0e30);

            for (const auto& sg : segs)
            {
                const int x0 = juce::jmax (0, (int) std::floor (std::min (sg.x1, sg.x2) - width - 1.0));
                const int x1 = juce::jmin (W - 1, (int) std::ceil (std::max (sg.x1, sg.x2) + width + 1.0));
                const int y0 = juce::jmax (0, (int) std::floor (std::min (sg.y1, sg.y2) - width - 1.0));
                const int y1 = juce::jmin (H - 1, (int) std::ceil (std::max (sg.y1, sg.y2) + width + 1.0));

                for (int y = y0; y <= y1; ++y)
                    for (int x = x0; x <= x1; ++x)
                    {
                        auto& d0 = nearest[(size_t) y * (size_t) W + (size_t) x];
                        d0 = std::min (d0, linePoint (sg.x1, sg.y1, sg.x2, sg.y2, x + 0.5, y + 0.5));
                    }
            }

            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                {
                    const double dist = nearest[(size_t) y * (size_t) W + (size_t) x];

                    Col col = base();
                    int alpha;

                    if (dist < width)
                    {
                        col.changeBrightness ((width - dist) / width * 255.0 * pr.specular / 100.0);
                        alpha = dist < width2 ? 255 : (int) (255.0 - (dist - width2) / (width - width2) * 255.0);
                    }
                    else
                    {
                        alpha = 0;
                    }

                    col.a = alpha;
                    col.setPix (bmp, x, y);
                }
        }

        void lines (bool horizontal)
        {
            const double cx = W * 0.5, cy = H * 0.5;
            double ax = 1.0, ay = 1.0;

            if (pr.aspect > 0.0) ax = 100.0 / (100.0 - std::min (pr.aspect, 99.0));
            if (pr.aspect < 0.0) ay = 100.0 / (100.0 + std::max (pr.aspect, -99.0));

            const double len = horizontal ? cx / ax * pr.length / 100.0 : cy / ay * pr.length / 100.0;
            const double area = horizontal ? cy / ay : cx / ax;
            const double width = area * pr.width / 200.0 + 1.0;
            const double width2 = (width - 1.0) * (1.0 - pr.diffuse / 100.0);

            if (W == 0 || H == 0)
                return;

            for (int y = 0; y < H; ++y)
            {
                const double ya = std::abs (-((double) y + 0.5 - cy));

                for (int x = 0; x < W; ++x)
                {
                    const double xa = std::abs ((double) x + 0.5 - cx);
                    const double across = horizontal ? ya : xa;   // distance across the lines
                    const double along = horizontal ? xa : ya;    // along them
                    double mn = 1.0, alpha;

                    if (across < area)
                    {
                        mn = 100000.0;

                        for (int k = 0; k <= 100; )
                        {
                            const double at = (double) k * (area - width) / 100.0;
                            const double r = along < len ? std::abs (at - across)
                                                         : std::sqrt ((along - len) * (along - len) + (across - at) * (across - at));
                            mn = std::min (mn, r);

                            // (A step below 1 would never move on: KnobMan's own panel stops at 1.)
                            if (pr.step <= 0.0 || ! std::isfinite (pr.step))
                                break;

                            k = (int) ((double) k + juce::jmax (1.0, pr.step));
                        }

                        alpha = mn < width ? (mn < width2 ? 255.0 : 255.0 - (mn - width2) / (width - width2) * 255.0) : 0.0;
                    }
                    else
                    {
                        alpha = 0.0;
                    }

                    Col col = base();
                    col.changeBrightness ((1.0 - mn / width) * 255.0 * pr.specular / 100.0);
                    col.a = (int) alpha;
                    col.setPix (bmp, x, y);
                }
            }
        }

        static juce::String typefaceFor (const juce::String& name)
        {
            const auto trimmed = name.trim();

            if (trimmed.isEmpty() || trimmed == "undefined")
                return juce::Font::getDefaultSansSerifFontName();

            return trimmed;
        }

        void text (int frame, int total)
        {
            bmp.clear();
            DynamicText dt (pr.text);
            const auto s = dt.get (frame, total);
            const int size = (int) ((double) H * pr.fontsize * 0.01);

            if (size <= 0 || s.isEmpty())
                return;

            int style = juce::Font::plain;
            if (pr.bold != 0)   style |= juce::Font::bold;
            if (pr.italic != 0) style |= juce::Font::italic;

            juce::Font font (juce::FontOptions (typefaceFor (pr.font), 10.0f, style));
            font = font.withPointHeight ((float) size);   // Java's size is the em size
            juce::Image image (juce::Image::ARGB, W, H, true);
            {
                juce::Graphics g (image);
                g.setFont (font);
                g.setColour (juce::Colour ((juce::uint8) pr.color.r, (juce::uint8) pr.color.g, (juce::uint8) pr.color.b));
                const int w = (int) std::ceil (juce::GlyphArrangement::getStringWidth (font, s));
                const int space = (int) std::ceil (juce::GlyphArrangement::getStringWidth (font, " "));
                const float baseline = (float) ((H + (int) font.getAscent() - (int) font.getDescent()) / 2);
                const float x = pr.textalign == 1 ? (float) (space / 2)
                              : pr.textalign == 2 ? (float) (W - w - space / 2)
                                                  : (float) ((W - w) / 2);
                g.drawSingleLineText (s, juce::roundToInt (x), juce::roundToInt (baseline));
            }

            fromJuceImage (image, bmp);
        }

        void shape()
        {
            bmp.clear();

            // "/x,y,x,y,x,y:..." per path: bezier handles and points in KnobMan's 0..512 space,
            // the canvas being 128..384.
            juce::Path path;
            path.setUsingNonZeroWinding (false);
            const auto sx = [this] (double v) { return (float) ((v - 128.0) / 256.0 * W); };
            const auto sy = [this] (double v) { return (float) ((v - 128.0) / 256.0 * H); };
            auto paths = juce::StringArray::fromTokens (pr.shape, "/", {});

            for (int pi = 1; pi < paths.size(); ++pi)
            {
                std::vector<double> v;

                for (const auto& point : juce::StringArray::fromTokens (paths[pi], ":", {}))
                {
                    auto nums = juce::StringArray::fromTokens (point, ",", {});

                    for (int k = 0; k < 6; ++k)
                        v.push_back (nums[k].getDoubleValue());
                }

                size_t j = 0;

                for (; j + 5 < v.size(); j += 6)
                {
                    if (j == 0)
                        path.startNewSubPath (sx (v[j + 2]), sy (v[j + 3]));
                    else
                        path.cubicTo (sx (v[j - 2]), sy (v[j - 1]), sx (v[j]), sy (v[j + 1]), sx (v[j + 2]), sy (v[j + 3]));
                }

                if (pr.fill != 0 && j >= 2 && v.size() >= 4)
                    path.cubicTo (sx (v[j - 2]), sy (v[j - 1]), sx (v[0]), sy (v[1]), sx (v[2]), sy (v[3]));
            }

            juce::Image image (juce::Image::ARGB, W, H, true);
            {
                juce::Graphics g (image);
                g.setColour (juce::Colour (0, 0, 255));

                if (pr.fill != 0)
                {
                    const int dw = (int) std::floor ((double) std::min (W, H) * pr.diffuse * 0.005);

                    if (dw != 0)
                    {
                        g.reduceClipRegion (path);
                        g.fillPath (path);
                        int old = -1;

                        for (int i = dw; i > 0; --i)
                        {
                            const int c = i * 255 / (dw + 1);

                            if (c == old)
                                continue;

                            g.setColour (juce::Colour ((juce::uint8) 0, (juce::uint8) 0, (juce::uint8) c));
                            g.strokePath (path, juce::PathStrokeType ((float) i));
                            old = c;
                        }
                    }
                    else
                    {
                        g.fillPath (path);
                    }
                }
                else
                {
                    g.strokePath (path, juce::PathStrokeType ((float) (pr.width * W * 0.005)));
                }
            }

            Bmp drawn (W, H);
            fromJuceImage (image, drawn);
            const double cx = W * 0.5, cy = H * 0.5, txd = pr.texturedepth * 0.01;

            for (int y = 0; y < H; ++y)
            {
                const double yn = -((double) y + 0.5 - cy) / cy, py = (double) y - cy + 0.5;

                for (int x = 0; x < W; ++x)
                {
                    const double px = (double) x - cx + 0.5, xn = -((double) x + 0.5 - cx) / cx;
                    int alpha;
                    const int lumi = texLumi (px, py, txd, alpha);
                    Col col = base();
                    col.changeBrightness ((xn + yn) * 128.0 * pr.specular * 0.01 + (double) lumi);
                    const auto v = drawn.get (x, y);
                    col.a = (int) (v & 0xff) * (int) (v >> 24 & 0xff) * alpha / 255 / 255;
                    col.setPix (bmp, x, y);
                }
            }
        }

        void render (int frame, int total)
        {
            switch (pr.type)
            {
                case 1:  image (frame, total); break;
                case 2:  circle(); break;
                case 3:  circleFill (0); break;
                case 4:  circleFill (1); break;
                case 5:  waveCircle(); break;
                case 6:  circleFill (2); break;
                case 7:  rect(); break;
                case 8:  rectFill(); break;
                case 9:  triangle(); break;
                case 10: line(); break;
                case 11: radiateLine(); break;
                case 12: lines (true); break;
                case 13: lines (false); break;
                case 14: text (frame, total); break;
                case 15: shape(); break;
                default: none(); break;
            }
        }
    };

    // The size a primitive's bitmap has: the canvas, or an image not fitted at its frame's size.
    void primSize (const Prim& pr, int W, int H, double imageScale, int& w, int& h)
    {
        w = W;
        h = H;

        if (pr.type == 1 && pr.autofit == 0 && pr.image != nullptr)
        {
            int isw = pr.image->w, ish = pr.image->h;

            if (pr.numframe > 1)
            {
                if (pr.framealign == 1) isw /= pr.numframe;
                else                    ish /= pr.numframe;
            }

            w = juce::jmax (1, juce::roundToInt (isw * imageScale));
            h = juce::jmax (1, juce::roundToInt (ish * imageScale));
        }
    }

    //==========================================================================
    // What a layer puts on the canvas: itself and its shadows, before they're blended on.
    struct Made
    {
        Bmp temp, shadow, shadow2;
        bool drop = false, inner = false;
    };

    //==========================================================================
    // Eff: the layer's transform, colour, masks, lighting and shadows, onto the canvas.
    struct Applier
    {
        const Eff& e;
        const KnobManDoc& doc;
        int W, H;   // the canvas (KnobMan's prefs.width / height)

        double anim (double from, double to, double ratio, int curve) const
        {
            if (curve == 0) return from;
            if (curve == 1) return from + (to - from) * ratio;

            const int c = juce::jlimit (0, 7, curve - 2);
            return from + (to - from) * doc.curves[c].get (ratio);
        }

        void diffuse (Bmp& img, double diff) const
        {
            // A box blur across, then down (running sums: the same averages as KnobMan's loops).
            const int wx = (int) ((double) (W / 8) * diff / 100.0);

            if (wx <= 0)
                return;   // (each pixel is its own average)

            Bmp temp (W, H);

            const auto pass = [] (const Bmp& from, Bmp& to, int count, int along, int radius, auto at)
            {
                for (int line = 0; line < count; ++line)
                {
                    long sa = 0, sr = 0, sg = 0, sb = 0;
                    int n = 0;
                    const auto add = [&] (int i, int sign)
                    {
                        const auto v = at (from, line, i);
                        sa += sign * (long) (v >> 24 & 0xff); sr += sign * (long) (v >> 16 & 0xff);
                        sg += sign * (long) (v >> 8 & 0xff);  sb += sign * (long) (v & 0xff);
                        n += sign;
                    };

                    for (int i = 0; i <= std::min (along - 1, radius); ++i)
                        add (i, 1);

                    for (int i = 0; i < along; ++i)
                    {
                        if (i > 0)
                        {
                            if (i + radius < along)   add (i + radius, 1);
                            if (i - radius - 1 >= 0)  add (i - radius - 1, -1);
                        }

                        const auto value = (juce::uint32) (((sa / n) << 24) + ((sr / n) << 16) + ((sg / n) << 8) + (sb / n));
                        at (to, line, i) = value;
                    }
                }
            };

            pass (img, temp, H, W, wx, [] (auto& b, int y, int x) -> auto& { return b.p[(size_t) y * (size_t) b.w + (size_t) x]; });
            pass (temp, img, W, H, wx, [] (auto& b, int x, int y) -> auto& { return b.p[(size_t) y * (size_t) b.w + (size_t) x]; });
        }

        void makeShadow (Bmp& dst, const Bmp& src, int inside, int type, double grad, double offset,
                         double offX, double offY, int density, Col colDef) const
        {
            offX *= offset;
            offY *= offset;
            int loops = type == 0 ? 1 : (int) offset;

            if (loops == 0)
                loops = 1;

            const int sizeX = src.w, sizeY = H;

            for (int y = 0; y < sizeY; ++y)
                for (int x = 0; x < sizeX; ++x)
                {
                    double mx = 0.0;

                    for (int i = 1; i <= loops; ++i)
                    {
                        double fy = (double) y - offY * i / loops;
                        int yy = (int) fy;
                        fy -= yy;
                        double fx = (double) x - offX * i / loops;
                        int xx = (int) fx;
                        fx -= xx;
                        int v00 = 0, v01 = 0, v10 = 0, v11 = 0;

                        if (xx >= 0 && xx < sizeX && yy >= 0 && yy < sizeY) v00 = src.alpha (xx, yy);
                        if (++xx >= 0 && xx < sizeX && yy >= 0 && yy < sizeY) v01 = src.alpha (xx, yy);
                        --xx; ++yy;
                        if (xx >= 0 && xx < sizeX && yy >= 0 && yy < sizeY) v10 = src.alpha (xx, yy);
                        if (++xx >= 0 && xx < sizeX && yy >= 0 && yy < sizeY) v11 = src.alpha (xx, yy);

                        const double f0 = v00 + (v01 - v00) * fx;
                        const double f1 = v10 + (v11 - v10) * fx;
                        double val = f0 + (f1 - f0) * fy;
                        val = val * (grad * (double) (loops - i + 1) / (double) loops + (100.0 - grad)) * 0.01;

                        if (val > mx)
                            mx = val;
                    }

                    if (inside != 0)
                        mx = 255.0 - mx;

                    colDef.a = (int) mx * density / 100;
                    colDef.setPix (dst, x, y);
                }
        }

        static double maskWipe (int type, double x, double y, int xd, int yd, double start, double stop, double grad, int gradDir)
        {
            if (start == stop)
                return 0.0;

            bool reversed = false;

            if (start > stop)
            {
                std::swap (start, stop);
                reversed = true;
            }

            const auto gradient = [&] (double v, double lo, double hi, bool flip) -> double
            {
                double g;

                if (gradDir != 0)
                    g = std::min ((v - lo) / ((hi - lo) * grad / 100.0), (v - hi) / ((lo - hi) * grad / 100.0));
                else
                    g = ! flip ? (v - lo) / ((hi - lo) * grad / 100.0) : (v - hi) / ((lo - hi) * grad / 100.0);

                return juce::jlimit (0.0, 1.0, g);
            };

            switch (type)
            {
                case 0:
                {
                    if (stop - start >= 360.0)
                        return 255.0;

                    const double xx = x - xd * 0.5, yy = y - yd * 0.5;
                    const double c = std::sqrt (xx * xx + yy * yy);

                    if (c == 0.0)
                        return 255.0;

                    const double rc = 1.0 / c;
                    const double twoPi = juce::MathConstants<double>::twoPi;
                    const double aMin = start * juce::MathConstants<double>::pi / 180.0;
                    const double aMax = stop * juce::MathConstants<double>::pi / 180.0;
                    double a = std::atan2 (xx, -yy) - twoPi;

                    // (A whole number of turns at once when the range is far off: never a long loop.)
                    if (a < aMin - rc) a += std::ceil ((aMin - rc - a) / twoPi) * twoPi;
                    if (a > aMax + rc) a -= std::ceil ((a - aMax - rc) / twoPi) * twoPi;

                    while (a < aMin - rc) a += twoPi;
                    while (a > aMax + rc) a -= twoPi;

                    double alpha = 255.0;

                    if (a >= aMin - rc && a <= aMax + rc)
                    {
                        if (a < aMin) alpha = alpha * (a - (aMin - rc)) / rc;
                        if (a > aMax) alpha = alpha * (aMax + rc - a) / rc;
                    }
                    else
                    {
                        return 0.0;
                    }

                    // (KnobMan's angle gradient spans the edges' extra rc too.)
                    double g;

                    if (gradDir != 0)
                        g = std::min ((a - aMin + rc) / ((aMax - aMin + 2.0 * rc) * grad / 100.0), (a - aMax - rc) / ((aMin - aMax) * grad / 100.0));
                    else
                        g = ! reversed ? (a - aMin + rc) / ((aMax - aMin + 2.0 * rc) * grad / 100.0)
                                       : (a - aMax - rc) / ((aMin - aMax) * grad / 100.0);

                    return alpha * juce::jlimit (0.0, 1.0, g);
                }

                case 1:
                {
                    const double rStart = start * std::min (yd, xd) / 200.0, rStop = stop * std::min (yd, xd) / 200.0;
                    const double xx = x - (double) (xd / 2), yy = y - (double) (yd / 2);
                    const double c = std::sqrt (xx * xx + yy * yy);
                    double alpha = 255.0;

                    if (c < rStart) alpha = 0.0;
                    if (c < rStart + 1.0) alpha = (c - rStart) * alpha;
                    if (c > rStop) alpha = 0.0;
                    if (c > rStop - 1.0) alpha = (rStop - c) * alpha;

                    if (grad == 0.0)
                        return alpha;

                    return alpha * gradient (c, rStart, rStop, reversed);
                }

                case 2:
                {
                    const double rStart = (start + 100.0) * xd / 200.0, rStop = (stop + 100.0) * xd / 200.0;
                    double alpha = 255.0;

                    if (x < rStart - 1.0) return 0.0;
                    if (x < rStart) alpha *= x - (rStart - 1.0);
                    if (x > rStop + 1.0) return 0.0;
                    if (x > rStop) alpha *= rStop + 1.0 - x;

                    if (grad == 0.0)
                        return alpha;

                    return alpha * gradient (x, rStart, rStop, reversed);
                }

                case 3:
                {
                    const double rStop = (-start + 100.0) * yd / 200.0, rStart = (-stop + 100.0) * yd / 200.0;
                    double alpha = 255.0;

                    if (y < rStart - 1.0) return 0.0;
                    if (y < rStart) alpha *= y - (rStart - 1.0);
                    if (y > rStop + 1.0) return 0.0;
                    if (y > rStop) alpha *= rStop + 1.0 - y;

                    if (grad == 0.0)
                        return alpha;

                    return alpha * gradient (y, rStart, rStop, ! reversed);
                }

                default:
                    return 255.0;
            }
        }

        static void hilight (Bmp& img, double sdir, double sden, double edir, double offset, double eden)
        {
            // Nothing to light and no edge to light up: KnobMan's loops would change nothing.
            if (sden == 0.0 && eden == 0.0)
                return;

            const double pi = juce::MathConstants<double>::pi;
            const double lx = -std::sin (sdir * pi / 180.0), ly = std::cos (sdir * pi / 180.0);
            const double ex = -std::sin (edir * pi / 180.0), ey = std::cos (edir * pi / 180.0);
            const int w = img.w, h = img.h;
            std::vector<int> t1 ((size_t) w * (size_t) h, 0), t2 (t1.size(), 0);
            double table[64] {};
            const double sigma = offset;
            const int xMax = (int) std::min (32.0, sigma * 3.0 + 1.0);

            for (int x = 0; x < xMax; ++x)
                table[x] = std::exp ((double) (-x * x) / (2.0 * sigma * sigma));

            for (int y = 0; y < h && eden != 0.0; ++y)
                for (int x = 0; x < w; ++x)
                {
                    const int a = img.alpha (x, y);
                    const int ax0 = x >= 1 ? img.alpha (x - 1, y) : 0, ax1 = x < w - 1 ? img.alpha (x + 1, y) : 0;
                    const int ay0 = y >= 1 ? img.alpha (x, y - 1) : 0, ay1 = y < h - 1 ? img.alpha (x, y + 1) : 0;
                    t1[(size_t) (y * w + x)] = (int) (((double) (ay1 - ay0) * ey + (double) (ax1 - ax0) * ex) * (double) a);
                }

            for (int y = 0; y < h && eden != 0.0; ++y)
                for (int x = 0; x < w; ++x)
                {
                    auto& acc = t2[(size_t) (y * w + x)];

                    for (int i = -xMax; i <= xMax; ++i)
                    {
                        const int xx = x + i;

                        if (xx >= 0 && xx < w)
                            acc = (int) ((double) acc + table[std::abs (i)] * (double) t1[(size_t) (y * w + xx)]);
                    }
                }

            std::fill (t1.begin(), t1.end(), 0);

            for (int x = 0; x < w && eden != 0.0; ++x)
                for (int y = 0; y < h; ++y)
                {
                    auto& acc = t1[(size_t) (y * w + x)];

                    for (int i = -xMax; i <= xMax; ++i)
                    {
                        const int yy = y + i;

                        if (yy >= 0 && yy < h)
                            acc = (int) ((double) acc + table[std::abs (i)] * (double) t2[(size_t) (yy * w + x)]);
                    }
                }

            Col c;

            for (int y = 0; y < h; ++y)
            {
                const double ry = ((double) y + 0.5) * 2.0 / (double) h - 1.0;

                for (int x = 0; x < w; ++x)
                {
                    const double rx = ((double) x + 0.5) * 2.0 / (double) w - 1.0;
                    const double br = -(lx * rx + ly * ry) * sden * 2.55;
                    c.getPix (img, x, y);
                    c.changeBrightness (br + (double) t1[(size_t) (y * w + x)] * eden);
                    c.setPix (img, x, y);
                }
            }
        }

        // Layer onto the canvas (add: blended over what's there).
        static void composite (const Made& m, Bmp& dest)
        {
            Col c;

            for (int y = 0; y < m.temp.h; ++y)
            {
                if (m.drop)
                    for (int x = 0; x < m.temp.w; ++x) { c.getPix (m.shadow, x, y); c.addPix (dest, x, y); }

                for (int x = 0; x < m.temp.w; ++x) { c.getPix (m.temp, x, y); c.addPix (dest, x, y); }

                if (m.inner)
                    for (int x = 0; x < m.temp.w; ++x) { c.getPix (m.shadow2, x, y); c.mulAddPix (dest, m.temp, x, y, x, y); }
            }
        }

        // Nothing in the layer's effect changes from frame to frame.
        bool isStill() const noexcept
        {
            return e.zoomxanim == 0 && e.zoomyanim == 0 && e.offxanim == 0 && e.offyanim == 0 && e.angleanim == 0
                && e.alphaanim == 0 && e.brightanim == 0 && e.contrastanim == 0 && e.saturationanim == 0 && e.hueanim == 0
                && e.mask1startanim == 0 && e.mask1stopanim == 0 && e.mask2startanim == 0 && e.mask2stopanim == 0
                && e.fmaskena == 0 && e.slightdiranim == 0 && e.sdensityanim == 0 && e.dlightdiranim == 0 && e.doffsetanim == 0
                && e.ddensityanim == 0 && e.ddiffuseanim == 0 && e.ilightdiranim == 0 && e.ioffsetanim == 0 && e.idensityanim == 0
                && e.idiffuseanim == 0 && e.elightdiranim == 0 && e.eoffsetanim == 0 && e.edensityanim == 0;
        }

        std::shared_ptr<Made> make (const Bmp& src, Col coldef, double r) const
        {
            auto made = std::make_shared<Made>();
            const int dw = W, dh = H;
            double zoomx = anim (e.zoomxf, e.zoomxt, r, e.zoomxanim);
            double zoomy = anim (e.zoomyf, e.zoomyt, r, e.zoomyanim);

            if (e.zoomxysepa == 0)
                zoomy = zoomx;

            const double offx = anim (e.offxf, e.offxt, r, e.offxanim);
            const double offy = anim (e.offyf, e.offyt, r, e.offyanim);
            const double angle = anim (e.anglef, e.anglet, r, e.angleanim);
            const double alpha = anim (e.alphaf, e.alphat, r, e.alphaanim);
            const double bright = anim (e.brightf, e.brightt, r, e.brightanim);
            const double cont = anim (e.contrastf, e.contrastt, r, e.contrastanim);
            const double sat = anim (e.saturationf, e.saturationt, r, e.saturationanim);
            const double hue = anim (e.huef, e.huet, r, e.hueanim);
            const double m1start = anim (e.mask1startf, e.mask1startt, r, e.mask1startanim);
            const double m1stop = anim (e.mask1stopf, e.mask1stopt, r, e.mask1stopanim);
            const double m2start = anim (e.mask2startf, e.mask2startt, r, e.mask2startanim);
            const double m2stop = anim (e.mask2stopf, e.mask2stopt, r, e.mask2stopanim);
            const double slightdir = anim (e.slightdirf, e.slightdirt, r, e.slightdiranim);
            const double sden = anim (e.sdensityf, e.sdensityt, r, e.sdensityanim);
            const double dlightdir = anim (e.dlightdirf, e.dlightdirt, r, e.dlightdiranim);
            double doff = anim (e.doffsetf, e.doffsett, r, e.doffsetanim);
            const double dden = anim (e.ddensityf, e.ddensityt, r, e.ddensityanim);
            const double ddiff = anim (e.ddiffusef, e.ddiffuset, r, e.ddiffuseanim);
            const double ilightdir = anim (e.ilightdirf, e.ilightdirt, r, e.ilightdiranim);
            double ioff = anim (e.ioffsetf, e.ioffsett, r, e.ioffsetanim);
            const double iden = anim (e.idensityf, e.idensityt, r, e.idensityanim);
            const double idiff = anim (e.idiffusef, e.idiffuset, r, e.idiffuseanim);
            const double elightdir = anim (e.elightdirf, e.elightdirt, r, e.elightdiranim);
            const double eoff = anim (e.eoffsetf, e.eoffsett, r, e.eoffsetanim);
            const double eden = anim (e.edensityf, e.edensityt, r, e.edensityanim);

            if (zoomx == 0.0 || zoomy == 0.0)
                return nullptr;

            const double dzx = 1.0 / zoomx, dzy = 1.0 / zoomy;
            auto& temp = made->temp;
            auto& shadow = made->shadow;
            auto& shadow2 = made->shadow2;
            temp = Bmp (dw, dh);

            // Conv is the same for the same colour: the last one is remembered.
            int convIn[3] { -1, -1, -1 }, convOut[3] {};
            const auto convert = [&] (Col& c)
            {
                if (c.r == convIn[0] && c.g == convIn[1] && c.b == convIn[2])
                {
                    c.r = convOut[0]; c.g = convOut[1]; c.b = convOut[2];
                    return;
                }

                convIn[0] = c.r; convIn[1] = c.g; convIn[2] = c.b;
                c.conv (bright, cont, sat, hue);
                convOut[0] = c.r; convOut[1] = c.g; convOut[2] = c.b;
            };

            // The matrix that takes a canvas pixel back into the primitive.
            struct M { double a[6] { 1, 0, 0, 1, 0, 0 }; };
            const auto mul = [] (const M& l, const M& rr)
            {
                M t;
                t.a[0] = l.a[0] * rr.a[0] + l.a[2] * rr.a[1];
                t.a[1] = l.a[1] * rr.a[0] + l.a[3] * rr.a[1];
                t.a[2] = l.a[0] * rr.a[2] + l.a[2] * rr.a[3];
                t.a[3] = l.a[1] * rr.a[2] + l.a[3] * rr.a[3];
                t.a[4] = l.a[0] * rr.a[4] + l.a[2] * rr.a[5] + l.a[4];
                t.a[5] = l.a[1] * rr.a[4] + l.a[3] * rr.a[5] + l.a[5];
                return t;
            };
            const auto translate = [&] (M& m, double x, double y) { M t; t.a[4] = x; t.a[5] = y; m = mul (t, m); };
            const auto rotateM = [&] (M& m, double deg)
            {
                const double rad = deg * juce::MathConstants<double>::pi / 180.0;
                M t;
                t.a[0] = t.a[3] = std::cos (rad);
                t.a[2] = std::sin (rad);
                t.a[1] = -t.a[2];
                m = mul (t, m);
            };
            const auto scaleM = [&] (M& m, double x, double y) { M t; t.a[0] = x; t.a[3] = y; m = mul (t, m); };
            const auto rotateAt = [&] (M& m, double deg, double x, double y)
            {
                M t;
                translate (t, -x, -y);
                rotateM (t, deg);
                translate (t, x, y);
                m = mul (t, m);
            };
            const auto scaleAt = [&] (M& m, double sx, double sy, double x, double y)
            {
                M t;
                translate (t, -x, -y);
                scaleM (t, sx, sy);
                translate (t, x, y);
                m = mul (t, m);
            };

            M mx;
            translate (mx, (double) ((src.w - dw) / 2), (double) ((src.h - dh) / 2));
            rotateAt (mx, angle, (e.centerx + 50.0) * 0.01 * src.w, (50.0 - e.centery) * 0.01 * src.h);
            translate (mx, -offx * 0.01 * src.w, offy * 0.01 * src.h);

            if (e.keepdir != 0)
                rotateAt (mx, -angle, src.w * 0.5, src.h * 0.5);

            scaleAt (mx, dzx * 100.0, dzy * 100.0, src.w * 0.5, src.h * 0.5);

            coldef.a = 0;
            Col c00, c01, c10, c11;
            const int w = (int) std::min ((double) (src.w * 100 / dw) * dzx / 2.0, (double) (src.h * 100 / dh) * dzx / 2.0);

            for (int y = 0; y < dh; ++y)
                for (int x = 0; x < dw; ++x)
                {
                    double px0 = x + 0.5, py0 = y + 0.5;
                    double pos0 = px0 * mx.a[0] + py0 * mx.a[2] + mx.a[4];
                    double pos1 = px0 * mx.a[1] + py0 * mx.a[3] + mx.a[5];
                    int px = (int) std::floor (pos0), py = (int) std::floor (pos1);

                    if (px < -1 || px > src.w || py < -1 || py > src.h)
                    {
                        c00.clearAll();
                        convert (c00);
                        c00.setPix (temp, x, y);
                        continue;
                    }

                    if (w >= 2)
                    {
                        const int step = w > 10 ? (int) std::sqrt ((double) w) : 1;
                        c00.clearAll();

                        for (int yy = -w; yy <= w; yy += step)
                        {
                            const int yyy = py + yy;

                            if (yyy < 0 || yyy >= src.h)
                                continue;

                            for (int xx = -w; xx <= w; xx += step)
                            {
                                const int xxx = px + xx;

                                if (xxx >= 0 && xxx < src.w)
                                    c00.addVal (src, xxx, yyy);
                            }
                        }

                        c00.setAve();
                    }
                    else
                    {
                        pos0 -= 0.5;
                        pos1 -= 0.5;
                        px = (int) std::floor (pos0);
                        py = (int) std::floor (pos1);
                        const auto fetch = [&] (Col& c, int xx, int yy)
                        {
                            if (xx < 0 || yy < 0 || xx >= src.w || yy >= src.h) c.copy (coldef);
                            else c.getPix (src, xx, yy);
                        };
                        fetch (c00, px, py);
                        fetch (c01, px + 1, py);
                        fetch (c10, px, py + 1);
                        fetch (c11, px + 1, py + 1);
                        c00.blend4 (c01, c10, c11, pos0 - px, pos1 - py);
                    }

                    convert (c00);
                    const double dX = (double) x + 0.5 - e.centerx * dw / 100.0;
                    const double dY = (double) y + 0.5 + e.centery * dh / 100.0;

                    if (e.antialias == 0)
                        c00.a = c00.a >= 128 ? 255 : 0;

                    double m1 = 255.0;

                    if (e.mask1ena != 0)
                        m1 = maskWipe (e.mask1type, dX, dY, dw, dh, m1start, m1stop, e.mask1grad, e.mask1graddir);

                    if (e.mask2ena != 0)
                    {
                        const double m2 = maskWipe (e.mask2type, dX, dY, dw, dh, m2start, m2stop, e.mask2grad, e.mask2graddir);
                        m1 = e.mask2op == 0 ? m1 * m2 / 255.0 : std::max (m1, m2);
                    }

                    c00.a = (int) ((double) c00.a * m1 / 255.0 * (alpha * 0.01));
                    c00.setPix (temp, x, y);
                }

            const double pi = juce::MathConstants<double>::pi;
            const double few = (double) std::min (dw, dh) * (eoff + 1.0) / 400.0;
            const double lx = -std::sin (slightdir * pi / 180.0), ly = std::cos (slightdir * pi / 180.0);

            if (few != 0.0)
                hilight (temp, slightdir, sden, elightdir, few, eden / (40000.0 * few));

            if (dden != 0.0)
            {
                Col def (0, 0, 0);

                if (dden < 0.0)
                    def.r = def.g = def.b = 255;

                double dx = lx, dy = ly;

                if (e.dlightdirena != 0)
                {
                    dx = -std::sin (dlightdir * pi / 180.0);
                    dy = std::cos (dlightdir * pi / 180.0);
                }

                shadow = Bmp (dw, dh);
                doff *= (double) std::min (dw, dh) / 100.0;
                makeShadow (shadow, temp, 0, e.dstype, e.dsgrad, doff, dx, dy, (int) std::abs (dden), def);
                diffuse (shadow, ddiff);
            }

            if (iden != 0.0)
            {
                Col def (0, 0, 0);

                if (iden < 0.0)
                    def.r = def.g = def.b = 255;

                double dx = lx, dy = ly;

                if (e.ilightdirena != 0)
                {
                    dx = -std::sin (ilightdir * pi / 180.0);
                    dy = std::cos (ilightdir * pi / 180.0);
                }

                shadow2 = Bmp (dw, dh);
                ioff *= (double) std::min (dw, dh) / 100.0;
                makeShadow (shadow2, temp, 1, 0, 0.0, ioff, dx, dy, (int) std::abs (iden), def);
                diffuse (shadow2, idiff);
            }

            made->drop = dden != 0.0;
            made->inner = iden != 0.0;
            return made;
        }
    };

    //==========================================================================
    std::shared_ptr<const Bmp> drawPrim (const Prim& pr, int W, int H, double imageScale, int frame, int total)
    {
        int w, h;
        primSize (pr, W, H, imageScale, w, h);
        auto bmp = std::make_shared<Bmp> (w, h);
        Painter painter { pr, w, h, imageScale, *bmp };
        painter.render (frame, total);
        return bmp;
    }

    // Layer.RenderFrame: frame masks, unfold and animation steps, then the layer onto the canvas.
    void renderLayer (const KnobManDoc& doc, Cache::Entry& entry, size_t index, const Layer& ly, const std::shared_ptr<const Bmp>& still,
                      Bmp& dest, int W, int H, double imageScale, int frame, int total)
    {
        const auto& e = ly.eff;

        if (e.fmaskena == 1)
        {
            const double f = total > 0 ? (double) (float) ((float) (frame * 100) / (float) total) : 0.0;

            if (f < e.fmaskstart || f > e.fmaskstop)
                return;
        }

        if (e.fmaskena == 2)
        {
            const int l = e.fmaskbits.length();
            int p = total > 0 ? (int) ((double) frame / (double) total * (double) l) : 0;

            if (p >= l)
                p = l - 1;

            if (p >= 0 && e.fmaskbits[p] == '0')
                return;
        }

        int totalFrame, startFrame, endFrame;

        if (e.unfold != 0)
        {
            totalFrame = e.animstep == 0 ? total + 1 : e.animstep;
            startFrame = 0;
            endFrame = e.animstep == 0 ? total : e.animstep - 1;
        }
        else if (e.animstep == 0)
        {
            totalFrame = total + 1;
            startFrame = endFrame = frame;
        }
        else
        {
            totalFrame = e.animstep;
            startFrame = endFrame = total > 0 ? frame * (e.animstep - 1) / total : 0;
        }

        const Applier applier { e, doc, W, H };

        // A layer that's the same every frame: made once at this size.
        if (! ly.prim.animates() && applier.isStill())
        {
            std::shared_ptr<const Made> made;
            {
                std::lock_guard<std::mutex> hold (doc.cache->lock);

                if (! entry.madeYet[index])
                {
                    if (still != nullptr && still->w > 0 && still->h > 0)
                        entry.made[index] = applier.make (*still, ly.prim.color, 0.0);

                    entry.madeYet[index] = true;
                }

                made = entry.made[index];
            }

            if (made != nullptr)
                for (int f = startFrame; f <= endFrame; ++f)
                    Applier::composite (*made, dest);

            return;
        }

        for (int f = startFrame; f <= endFrame; ++f)
        {
            const auto bmp = ly.prim.animates() ? drawPrim (ly.prim, W, H, imageScale, f, totalFrame) : still;
            const double r = totalFrame <= 1 ? 0.0 : (double) f / (double) (totalFrame - 1);

            if (bmp != nullptr && bmp->w > 0 && bmp->h > 0)
                if (const auto made = applier.make (*bmp, ly.prim.color, r))
                    Applier::composite (*made, dest);
        }
    }

    //==========================================================================
    juce::Image toJuce (const Bmp& b)
    {
        juce::Image image (juce::Image::ARGB, juce::jmax (1, b.w), juce::jmax (1, b.h), true);
        juce::Image::BitmapData data (image, juce::Image::BitmapData::writeOnly);

        for (int y = 0; y < b.h; ++y)
            for (int x = 0; x < b.w; ++x)
            {
                const auto v = b.get (x, y);
                data.setPixelColour (x, y, juce::Colour ((juce::uint8) (v >> 16), (juce::uint8) (v >> 8), (juce::uint8) v, (juce::uint8) (v >> 24)));
            }

        return image;
    }

    juce::Image renderFrame (const KnobManDoc& doc, int frame, float scale)
    {
        return renderFrame (doc, frame, scale, ShapeDrawer());
    }

    juce::Image renderFrame (const KnobManDoc& doc, int frame, float scale, const ShapeDrawer& drawer)
    {
        if (! std::isfinite (scale) || scale <= 0.0f)
            scale = 1.0f;

        // Below 1: drawn at 1 (a big document at no less than 256 pixels across), then made
        // smaller. Never drawn past 1600 pixels across: anything bigger is that, enlarged (a
        // huge canvas would take seconds a frame, and nothing shows it that big).
        const float maxDim = (float) juce::jmax (1, doc.width, doc.height);
        float drawScale = scale >= 1.0f ? scale : std::max (scale, std::min (1.0f, 256.0f / maxDim));
        drawScale = std::min (drawScale, std::max (0.01f, 1600.0f / maxDim));
        const int outW = juce::jmax (1, juce::roundToInt (doc.width * scale));
        const int outH = juce::jmax (1, juce::roundToInt (doc.height * scale));
        const int baseW = juce::jmax (1, juce::roundToInt (doc.width * drawScale));
        const int baseH = juce::jmax (1, juce::roundToInt (doc.height * drawScale));

        // KnobMan's oversampling: drawn 2, 4 or 8 times bigger and decimated. (Not past a size
        // that would take long: a big render is sharp already.)
        int os = juce::jlimit (0, 3, doc.oversampling);

        while (os > 0 && (std::max (baseW, baseH) << os) > 768)
            --os;

        const int W = baseW << os, H = baseH << os;
        const double imageScale = (double) W / (double) juce::jmax (1, doc.width);
        const int total = juce::jmax (1, doc.frames) - 1;
        frame = juce::jlimit (0, juce::jmax (0, total), frame);

        // The primitives that don't change from frame to frame, drawn once a size.
        std::shared_ptr<Cache::Entry> entry;
        {
            // (Drawn another way: cached apart.)
            const auto key = (((juce::int64) W << 32) | (juce::int64) H) ^ (drawer != nullptr ? ((juce::int64) 1 << 62) : 0);
            std::lock_guard<std::mutex> hold (doc.cache->lock);
            auto found = doc.cache->drawn.find (key);

            if (found == doc.cache->drawn.end())
            {
                if (doc.cache->drawn.size() >= 4)
                    doc.cache->drawn.erase (doc.cache->drawn.begin());

                auto made = std::make_shared<Cache::Entry>();

                for (size_t i = 0; i < doc.layers.size(); ++i)
                {
                    const auto& ly = doc.layers[i];
                    std::shared_ptr<const Bmp> drawn;

                    if (ly->visible && ! ly->prim.animates())
                    {
                        // A Sphere or Metal Circle drawn another way, at the canvas's size as KnobMan's is.
                        if (drawer != nullptr && (ly->prim.type == 4 || ly->prim.type == 6))
                        {
                            const auto image = drawer ((int) i, ly->prim.type, W, H);

                            if (image.isValid() && image.getWidth() == W && image.getHeight() == H)
                            {
                                auto bmp = std::make_shared<Bmp> (W, H);
                                fromJuceImage (image, *bmp);
                                drawn = bmp;
                            }
                        }

                        if (drawn == nullptr)
                            drawn = drawPrim (ly->prim, W, H, imageScale, 0, 2);
                    }

                    made->prims.push_back (drawn);
                }

                made->made.resize (doc.layers.size());
                made->madeYet.assign (doc.layers.size(), false);
                found = doc.cache->drawn.emplace (key, made).first;
            }

            entry = found->second;
        }

        Bmp canvas (W, H);

        for (size_t i = 0; i < doc.layers.size(); ++i)
            if (doc.layers[i]->visible)
                renderLayer (doc, *entry, i, *doc.layers[i], entry->prims[i], canvas, W, H, imageScale, frame, total);

        if (W == outW && H == outH)
            return toJuce (canvas);

        // (Drawn smaller than asked, past the size limit: enlarged.)
        if (W < outW || H < outH)
            return toJuce (canvas).rescaled (outW, outH, juce::Graphics::highResamplingQuality);

        Bmp out (outW, outH);
        areaScale (canvas, 0, 0, W, H, out, 0, 0, outW, outH);
        return toJuce (out);
    }
}

//==============================================================================
double KnobManDoc::Curve::get (double ratio) const noexcept
{
    int t0 = 0, l0 = lv[0];

    for (int i = 1; i < 12; ++i)
    {
        if (tm[i] < 0)
            continue;

        const int t1 = tm[i], l1 = lv[i];

        if (ratio * 100.0 <= (double) tm[i])
        {
            ratio = t1 == t0 ? (double) l1 / 100.0
                             : ((double) l0 + (double) (l1 - l0) * (ratio * 100.0 - (double) t0) / (double) (t1 - t0)) / 100.0;
            break;
        }

        t0 = t1;
        l0 = l1;
    }

    if (stepReso == 1)
    {
        ratio = 0.0;
    }
    else if (stepReso >= 2)
    {
        int i = (int) std::floor (ratio * stepReso);

        if (i >= stepReso)
            --i;

        ratio = (double) i / (double) (stepReso - 1);
    }

    return ratio;
}

KnobManDoc::~KnobManDoc() = default;

namespace KnobManEngine
{
    static const char* const primNames[] { "None", "Image", "Circle", "CircleFill", "MetalCircle", "WaveCircle", "Sphere",
                                            "Rect", "RectFill", "Triangle", "Line", "RadiateLine", "H-Lines", "V-Lines", "Text", "Shape" };

    /** Control.LoadLayer: a layer's name, primitive and effect from its section (not its pictures). */
    void readLayer (const Reader& in, Layer& layer)
    {
        auto& p = layer.prim;
        auto& e = layer.eff;
        layer.name = in.readString ("Name", "");

        const auto primName = in.readString ("Primitive", "None");
        p.type = 0;

        for (int t = 0; t < 16; ++t)
            if (primName == primNames[t])
                p.type = t;

        p.color = Col (in.readInt ("ColR", 255), in.readInt ("ColG", 0), in.readInt ("ColB", 0));
        p.aspect = in.readFloat ("PrimAspect", 0.0);
        p.round = in.readFloat ("PrimRound", 0.0);
        p.width = in.readFloat ("PrimWidth", 10.0);
        p.length = in.readFloat ("PrimLength", 50.0);
        p.step = in.readFloat ("PrimStep", 20.0);
        p.anglestep = in.readFloat ("PrimAngleStep", 45.0);
        p.emboss = in.readFloat ("PrimEmboss", 0.0);
        p.embossdiffuse = in.readFloat ("PrimEmbossDiffuse", 0.0);
        p.ambient = in.readFloat ("PrimAmbient", 50.0);
        p.lightdir = in.readFloat ("PrimLightDir", -50.0);
        p.specular = in.readFloat ("PrimSpecular", 0.0);
        p.specularwidth = in.readFloat ("PrimSpecWidth", 50.0);
        p.textureName = in.readString ("PrimTextureFile", "");
        p.texturedepth = in.readFloat ("PrimTexture", 0.0);
        p.texturezoom = in.readFloat ("PrimTexZoom", 100.0);
        p.diffuse = in.readFloat ("PrimDiffuse", 0.0);
        p.text = in.readString ("PrimText", "");
        p.font = in.readString ("PrimFont", "SansSerif");
        p.fontsize = in.readFloat ("PrimSize", 50.0);
        p.bold = in.readInt ("PrimBold", 0);
        p.italic = in.readInt ("PrimItalic", 0);
        p.textalign = in.readInt ("PrimTextAlign", 0);
        p.transparent = in.readInt ("Transparent", 0);
        p.autofit = in.readInt ("AutoFit", 1);
        p.intellialpha = in.readInt ("IntelliAlpha", 0);
        p.numframe = in.readInt ("PrimFrames", 1);
        p.framealign = in.readInt ("PrimAlign", 0);
        p.file = in.readString ("PrimFile", "");
        p.shape = in.readString ("PrimShape", "");
        p.fill = in.readInt ("PrimFill", 1);

        const auto anim = [&in] (const std::string& a, const std::string& c) { return in.readInt (a, 0) == 0 ? 0 : in.readInt (c, 1) + 1; };

        e.antialias = in.readInt ("Antialias", 1);
        e.unfold = in.readInt ("Unfold", 0);
        e.animstep = in.readInt ("AnimStep", 0);
        e.zoomxysepa = in.readInt ("ZoomXYSepa", 0);
        e.zoomxf = in.readFloat ("Zoom1", 100.0);
        e.zoomxt = in.readFloat ("Zoom2", 100.0);
        e.zoomxanim = anim ("AnimateZoom", "ZoomCurve");
        e.zoomyf = in.readFloat ("ZoomY1", 100.0);
        e.zoomyt = in.readFloat ("ZoomY2", 100.0);
        e.zoomyanim = anim ("AnimateZoomY", "ZoomYCurve");
        e.offxf = in.readFloat ("LayerOffsetX1", 0.0);
        e.offxt = in.readFloat ("LayerOffsetX2", 0.0);
        e.offxanim = anim ("AnimateLayerOffsetX", "LayerOffsetXCurve");
        e.offyf = in.readFloat ("LayerOffsetY1", 0.0);
        e.offyt = in.readFloat ("LayerOffsetY2", 0.0);
        e.offyanim = anim ("AnimateLayerOffsetY", "LayerOffsetYCurve");
        e.keepdir = in.readInt ("KeepDir", 0);
        e.centerx = in.readFloat ("RotCenterX1", 0.0);
        e.centery = in.readFloat ("RotCenterY1", 0.0);
        e.anglef = in.readFloat ("Angle1", 0.0);
        e.anglet = in.readFloat ("Angle2", 0.0);
        e.angleanim = anim ("AnimateAngle", "AngleCurve");
        e.alphaf = in.readFloat ("Alpha1", 100.0);
        e.alphat = in.readFloat ("Alpha2", 100.0);
        e.alphaanim = anim ("AnimateAlpha", "AlphaCurve");
        e.brightf = in.readFloat ("Brightness1", 0.0);
        e.brightt = in.readFloat ("Brightness2", 0.0);
        e.brightanim = anim ("AnimateBrightness", "BrightnessCurve");
        e.contrastf = in.readFloat ("Contrast1", 0.0);
        e.contrastt = in.readFloat ("Contrast2", 0.0);
        e.contrastanim = anim ("AnimateContrast", "ContrastCurve");
        e.saturationf = in.readFloat ("Saturation1", 0.0);
        e.saturationt = in.readFloat ("Saturation2", 0.0);
        e.saturationanim = anim ("AnimateSaturation", "SaturationCurve");
        e.huef = in.readFloat ("Hue1", 0.0);
        e.huet = in.readFloat ("Hue2", 0.0);
        e.hueanim = anim ("AnimateHue", "HueCurve");
        e.mask1ena = in.readInt ("UseMask", 0);
        e.mask1type = in.readInt ("MaskType", 0);
        e.mask1grad = in.readFloat ("MaskGradation", 0.0);
        e.mask1graddir = in.readInt ("MaskGradDir", 0);
        e.mask1startf = in.readFloat ("MaskStart1", -140.0);
        e.mask1startt = in.readFloat ("MaskStart2", -140.0);
        e.mask1startanim = anim ("AnimateMaskStart", "MaskStartCurve");
        e.mask1stopf = in.readFloat ("MaskStop1", 140.0);
        e.mask1stopt = in.readFloat ("MaskStop2", 140.0);
        e.mask1stopanim = anim ("AnimateMaskStop", "MaskStopCurve");
        e.mask2ena = in.readInt ("UseMask2", 0);
        e.mask2op = in.readInt ("Mask2Operation", 0);
        e.mask2type = in.readInt ("Mask2Type", 0);
        e.mask2grad = in.readFloat ("Mask2Gradation", 0.0);
        e.mask2graddir = in.readInt ("Mask2GradDir", 0);
        e.mask2startf = in.readFloat ("Mask2Start1", -140.0);
        e.mask2startt = in.readFloat ("Mask2Start2", -140.0);
        e.mask2startanim = anim ("AnimateMask2Start", "Mask2StartCurve");
        e.mask2stopf = in.readFloat ("Mask2Stop1", 140.0);
        e.mask2stopt = in.readFloat ("Mask2Stop2", 140.0);
        e.mask2stopanim = anim ("AnimateMask2Stop", "Mask2StopCurve");
        e.fmaskena = in.readInt ("UseFMask", 0);
        e.fmaskstart = in.readFloat ("FMaskStart", 0.0);
        e.fmaskstop = in.readFloat ("FMaskStop", 0.0);
        e.fmaskbits = in.readString ("FMaskBits", "");
        e.slightdirf = in.readFloat ("LightDir", -45.0);
        e.slightdirt = in.readFloat ("LightDir2", -45.0);
        e.slightdiranim = anim ("AnimateLightDir", "LightDirCurve");
        e.sdensityf = in.readFloat ("Lighting", 0.0);
        e.sdensityt = in.readFloat ("Lighting2", 0.0);
        e.sdensityanim = anim ("AnimateLighting", "LightingCurve");
        e.dlightdirena = in.readInt ("LightDirDEn", 0);
        e.dlightdirf = in.readFloat ("LightDirD", -45.0);
        e.dlightdirt = in.readFloat ("LightDirD2", -45.0);
        e.dlightdiranim = anim ("AnimateLightDirD", "LightDirDCurve");
        e.doffsetf = in.readFloat ("ShadowOffset1", 5.0);
        e.doffsett = in.readFloat ("ShadowOffset2", 5.0);
        e.doffsetanim = anim ("AnimateShadowOffset", "ShadowOffsetCurve");
        e.ddensityf = in.readFloat ("ShadowDensity1", 0.0);
        e.ddensityt = in.readFloat ("ShadowDensity2", 0.0);
        e.ddensityanim = anim ("AnimateShadowDensity", "ShadowDensityCurve");
        e.ddiffusef = in.readFloat ("ShadowDiffuse1", 0.0);
        e.ddiffuset = in.readFloat ("ShadowDiffuse2", 0.0);
        e.ddiffuseanim = anim ("AnimateShadowDiffuse", "ShadowDiffuseCurve");
        e.dstype = in.readInt ("ShadowType", 0);
        e.dsgrad = in.readFloat ("ShadowGradate", 100.0);
        e.ilightdirena = in.readInt ("LightDirIEn", 0);
        e.ilightdirf = in.readFloat ("LightDirI", -45.0);
        e.ilightdirt = in.readFloat ("LightDirI2", -45.0);
        e.ilightdiranim = anim ("AnimateLightDirI", "LightDirICurve");
        e.ioffsetf = in.readFloat ("IShadowOffset1", 5.0);
        e.ioffsett = in.readFloat ("IShadowOffset2", 5.0);
        e.ioffsetanim = anim ("AnimateIShadowOffset", "IShadowOffsetCurve");
        e.idensityf = in.readFloat ("IShadowDensity1", 0.0);
        e.idensityt = in.readFloat ("IShadowDensity2", 0.0);
        e.idensityanim = anim ("AnimateIShadowDensity", "IShadowDensityCurve");
        e.idiffusef = in.readFloat ("IShadowDiffuse1", 20.0);
        e.idiffuset = in.readFloat ("IShadowDiffuse2", 20.0);
        e.idiffuseanim = anim ("AnimateIShadowDiffuse", "IShadowDiffuseCurve");
        e.elightdirena = in.readInt ("LightDirEEn", 0);
        e.elightdirf = in.readFloat ("LightDirE", -45.0);
        e.elightdirt = in.readFloat ("LightDirE2", -45.0);
        e.elightdiranim = anim ("AnimateLightDirE", "LightDirECurve");
        e.eoffsetf = in.readFloat ("HilightOffset1", 0.0);
        e.eoffsett = in.readFloat ("HilightOffset2", 0.0);
        e.eoffsetanim = anim ("AnimateHilightOffset", "HilightOffsetCurve");
        e.edensityf = in.readFloat ("HilightDensity1", 0.0);
        e.edensityt = in.readFloat ("HilightDensity2", 0.0);
        e.edensityanim = anim ("AnimateHilightDensity", "HilightDensityCurve");
    }

    //==========================================================================
    // Control.SaveLayer / SaveExec: the document written as KnobMan writes it.
    std::string num (double v)
    {
        char buffer[64];
        const auto result = std::to_chars (buffer, buffer + sizeof (buffer), v);
        std::string text (buffer, result.ptr);

        if (text.find_first_of (".eEn") == std::string::npos)
            text += ".0";

        return text;
    }

    struct Writer
    {
        std::string text;

        void section (const std::string& name)                    { text += "[" + name + "]\n"; }
        void str (const std::string& key, const juce::String& v)  { text += key + "=" + v.toStdString() + "\n"; }
        void integer (const std::string& key, int v)              { text += key + "=" + std::to_string (v) + "\n"; }
        void real (const std::string& key, double v)              { text += key + "=" + num (v) + "\n"; }

        void anim (const std::string& s1, const std::string& s2, const std::string& s3, const std::string& s4, double from, double to, int a)
        {
            real (s1, from);
            real (s2, to);
            integer (s3, a != 0 ? 1 : 0);
            integer (s4, std::max (0, a - 1));
        }

        /** Hex in lines of 512 (ExtractFile reads on while a line is full, so a last full line
            gets an empty one after it, where the reading stops). */
        void embed (const std::string& base, const std::string& hex)
        {
            size_t n = 0;

            for (size_t i = 0; i < hex.size(); i += 512, ++n)
                text += base + std::to_string (n) + "=" + hex.substr (i, 512) + "\n";

            if (! hex.empty() && hex.size() % 512 == 0)
                text += base + std::to_string (n) + "=\n";
        }
    };

    /** A layer's keys (its pictures aside), its name and visibility included. */
    void writeLayer (Writer& w, const Layer& ly)
    {
        const auto& p = ly.prim;
        const auto& e = ly.eff;
        w.str ("Name", ly.name);
        w.integer ("Visible", ly.shown);
        w.integer ("VisibleSolo", ly.solo);
        w.integer ("ColR", p.color.r);
        w.integer ("ColG", p.color.g);
        w.integer ("ColB", p.color.b);
        w.str ("Primitive", primNames[juce::jlimit (0, 15, p.type)]);
        w.integer ("PrimFill", p.fill);
        w.real ("PrimAspect", p.aspect);
        w.real ("PrimWidth", p.width);
        w.real ("PrimRound", p.round);
        w.real ("PrimLength", p.length);
        w.real ("PrimStep", p.step);
        w.real ("PrimAngleStep", p.anglestep);
        w.real ("PrimEmboss", p.emboss);
        w.real ("PrimEmbossDiffuse", p.embossdiffuse);
        w.str ("PrimTextureFile", p.textureName);
        w.real ("PrimTexture", p.texturedepth);
        w.real ("PrimTexZoom", p.texturezoom);
        w.real ("PrimDiffuse", p.diffuse);
        w.real ("PrimAmbient", p.ambient);
        w.real ("PrimSpecWidth", p.specularwidth);
        w.real ("PrimSpecular", p.specular);
        w.real ("PrimLightDir", p.lightdir);
        w.real ("PrimSize", p.fontsize);
        w.integer ("PrimBold", p.bold);
        w.integer ("PrimItalic", p.italic);
        w.integer ("PrimTextAlign", p.textalign);
        w.str ("PrimText", p.text);
        w.str ("PrimFont", p.font);
        w.str ("PrimFile", p.file);
        w.str ("PrimShape", p.shape);
        w.integer ("Transparent", p.transparent);
        w.integer ("AutoFit", p.autofit);
        w.integer ("IntelliAlpha", p.intellialpha);
        w.integer ("PrimFrames", p.numframe);
        w.integer ("PrimAlign", p.framealign);
        w.integer ("Antialias", e.antialias);
        w.integer ("Unfold", e.unfold);
        w.integer ("KeepDir", e.keepdir);
        w.integer ("AnimStep", e.animstep);
        w.anim ("Angle1", "Angle2", "AnimateAngle", "AngleCurve", e.anglef, e.anglet, e.angleanim);
        w.integer ("ZoomXYSepa", e.zoomxysepa);
        w.anim ("Zoom1", "Zoom2", "AnimateZoom", "ZoomCurve", e.zoomxf, e.zoomxt, e.zoomxanim);
        w.anim ("ZoomY1", "ZoomY2", "AnimateZoomY", "ZoomYCurve", e.zoomyf, e.zoomyt, e.zoomyanim);
        w.real ("RotCenterX1", e.centerx);
        w.real ("RotCenterY1", e.centery);
        w.anim ("LayerOffsetX1", "LayerOffsetX2", "AnimateLayerOffsetX", "LayerOffsetXCurve", e.offxf, e.offxt, e.offxanim);
        w.anim ("LayerOffsetY1", "LayerOffsetY2", "AnimateLayerOffsetY", "LayerOffsetYCurve", e.offyf, e.offyt, e.offyanim);
        w.anim ("Alpha1", "Alpha2", "AnimateAlpha", "AlphaCurve", e.alphaf, e.alphat, e.alphaanim);
        w.anim ("Brightness1", "Brightness2", "AnimateBrightness", "BrightnessCurve", e.brightf, e.brightt, e.brightanim);
        w.anim ("Contrast1", "Contrast2", "AnimateContrast", "ContrastCurve", e.contrastf, e.contrastt, e.contrastanim);
        w.anim ("Saturation1", "Saturation2", "AnimateSaturation", "SaturationCurve", e.saturationf, e.saturationt, e.saturationanim);
        w.anim ("Hue1", "Hue2", "AnimateHue", "HueCurve", e.huef, e.huet, e.hueanim);
        w.integer ("UseMask", e.mask1ena);
        w.integer ("MaskType", e.mask1type);
        w.real ("MaskGradation", e.mask1grad);
        w.integer ("MaskGradDir", e.mask1graddir);
        w.anim ("MaskStart1", "MaskStart2", "AnimateMaskStart", "MaskStartCurve", e.mask1startf, e.mask1startt, e.mask1startanim);
        w.anim ("MaskStop1", "MaskStop2", "AnimateMaskStop", "MaskStopCurve", e.mask1stopf, e.mask1stopt, e.mask1stopanim);
        w.integer ("UseMask2", e.mask2ena);
        w.integer ("Mask2Operation", e.mask2op);
        w.integer ("Mask2Type", e.mask2type);
        w.real ("Mask2Gradation", e.mask2grad);
        w.integer ("Mask2GradDir", e.mask2graddir);
        w.anim ("Mask2Start1", "Mask2Start2", "AnimateMask2Start", "Mask2StartCurve", e.mask2startf, e.mask2startt, e.mask2startanim);
        w.anim ("Mask2Stop1", "Mask2Stop2", "AnimateMask2Stop", "Mask2StopCurve", e.mask2stopf, e.mask2stopt, e.mask2stopanim);
        w.integer ("UseFMask", e.fmaskena);
        w.real ("FMaskStart", e.fmaskstart);
        w.real ("FMaskStop", e.fmaskstop);
        w.str ("FMaskBits", e.fmaskbits);
        w.anim ("LightDir", "LightDir2", "AnimateLightDir", "LightDirCurve", e.slightdirf, e.slightdirt, e.slightdiranim);
        w.anim ("Lighting", "Lighting2", "AnimateLighting", "LightingCurve", e.sdensityf, e.sdensityt, e.sdensityanim);
        w.integer ("LightDirDEn", e.dlightdirena);
        w.anim ("LightDirD", "LightDirD2", "AnimateLightDirD", "LightDirDCurve", e.dlightdirf, e.dlightdirt, e.dlightdiranim);
        w.anim ("ShadowOffset1", "ShadowOffset2", "AnimateShadowOffset", "ShadowOffsetCurve", e.doffsetf, e.doffsett, e.doffsetanim);
        w.anim ("ShadowDensity1", "ShadowDensity2", "AnimateShadowDensity", "ShadowDensityCurve", e.ddensityf, e.ddensityt, e.ddensityanim);
        w.anim ("ShadowDiffuse1", "ShadowDiffuse2", "AnimateShadowDiffuse", "ShadowDiffuseCurve", e.ddiffusef, e.ddiffuset, e.ddiffuseanim);
        w.integer ("ShadowType", e.dstype);
        w.real ("ShadowGradate", e.dsgrad);
        w.integer ("LightDirIEn", e.ilightdirena);
        w.anim ("LightDirI", "LightDirI2", "AnimateLightDirI", "LightDirICurve", e.ilightdirf, e.ilightdirt, e.ilightdiranim);
        w.anim ("IShadowOffset1", "IShadowOffset2", "AnimateIShadowOffset", "IShadowOffsetCurve", e.ioffsetf, e.ioffsett, e.ioffsetanim);
        w.anim ("IShadowDensity1", "IShadowDensity2", "AnimateIShadowDensity", "IShadowDensityCurve", e.idensityf, e.idensityt, e.idensityanim);
        w.anim ("IShadowDiffuse1", "IShadowDiffuse2", "AnimateIShadowDiffuse", "IShadowDiffuseCurve", e.idiffusef, e.idiffuset, e.idiffuseanim);
        w.integer ("LightDirEEn", e.elightdirena);
        w.anim ("LightDirE", "LightDirE2", "AnimateLightDirE", "LightDirECurve", e.elightdirf, e.elightdirt, e.elightdiranim);
        w.anim ("HilightOffset1", "HilightOffset2", "AnimateHilightOffset", "HilightOffsetCurve", e.eoffsetf, e.eoffsett, e.eoffsetanim);
        w.anim ("HilightDensity1", "HilightDensity2", "AnimateHilightDensity", "HilightDensityCurve", e.edensityf, e.edensityt, e.edensityanim);
    }

    /** SaveExec: the whole document, its pictures embedded (a texture with the first layer drawing
        with it, a picture with each image layer). */
    juce::String writeBody (const KnobManDoc& doc)
    {
        Writer w;
        w.section ("Prefs");
        w.str ("Version", "1490");
        w.integer ("Layers", (int) doc.layers.size());
        w.integer ("CurrentLayer", 1);
        w.integer ("Styles", 1);
        w.integer ("CurrentStyle", 0);
        w.integer ("OutputSizeX", doc.width);
        w.integer ("OutputSizeX1", doc.width);
        w.integer ("OutputSizeY", doc.height);
        w.integer ("OutputSizeY1", doc.height);
        w.integer ("OverSampling", doc.oversampling);
        w.integer ("RenderFrames", doc.frames);
        w.integer ("PreviewFrames", doc.previewFrames);
        w.integer ("AlignHorizontal", doc.alignHorizontal);
        w.integer ("BkColorR", doc.background.getRed());
        w.integer ("BkColorG", doc.background.getGreen());
        w.integer ("BkColorB", doc.background.getBlue());

        for (size_t i = 0; i < doc.layers.size(); ++i)
            w.integer ("Visible1_" + std::to_string (i), doc.layers[i]->shown);

        static const char* const names[] { "0", "1", "2", "3", "4", "a", "b", "c", "d", "e", "f", "5" };

        for (int i = 0; i < 8; ++i)
        {
            const auto& c = doc.curves[i];
            const auto k = "Curve" + std::to_string (i + 1);
            w.integer (k + "L0", c.lv[0]);

            for (int j = 1; j < 11; ++j)
            {
                w.integer (k + "T" + names[j], c.tm[j]);
                w.integer (k + "L" + names[j], c.lv[j]);
            }

            w.integer (k + "L5", c.lv[11]);
            w.integer (k + "StepReso", c.stepReso);
        }

        // A texture's or a picture's hex: from whichever layer read it.
        const auto hexOf = [&doc] (bool texture, const juce::String& key) -> std::string
        {
            for (const auto& ly : doc.layers)
            {
                const auto& p = ly->prim;
                const auto& hex = texture ? p.texHex : p.imgHex;

                if (hex != nullptr && ! hex->empty() && (texture ? p.textureName : p.file) == key)
                    return *hex;
            }

            return {};
        };

        for (size_t i = 0; i < doc.layers.size(); ++i)
        {
            const auto& ly = *doc.layers[i];
            const auto& p = ly.prim;
            w.section ("Layer" + std::to_string (i + 1));
            writeLayer (w, ly);

            // A texture with the first layer of its name (a later one shares it as it's read). A
            // picture with every image layer: a layer without one would take the next one's.
            bool firstTexture = p.texturedepth != 0.0;
            const bool firstImage = p.type == 1;

            for (size_t j = 0; j < i; ++j)
            {
                const auto& q = doc.layers[j]->prim;

                if (q.texturedepth != 0.0 && q.textureName == p.textureName)
                    firstTexture = false;
            }

            if (firstTexture)
                w.embed ("TexBmp", p.texHex != nullptr && ! p.texHex->empty() ? *p.texHex : hexOf (true, p.textureName));

            if (firstImage)
                w.embed ("ImgBmp", p.imgHex != nullptr && ! p.imgHex->empty() ? *p.imgHex : hexOf (false, p.file));
        }

        return juce::String::fromUTF8 (w.text.c_str(), (int) w.text.size());
    }

    /** Which layers are drawn: the soloed ones when any is, else the shown ones. */
    void settleVisibility (KnobManDoc& doc)
    {
        const bool anySolo = std::any_of (doc.layers.begin(), doc.layers.end(), [] (const auto& ly) { return ly->solo != 0; });

        for (auto& ly : doc.layers)
            ly->visible = anySolo ? ly->solo != 0 : ly->shown != 0;
    }
}

juce::String KnobManDoc::layerText (int index) const
{
    if (! juce::isPositiveAndBelow (index, (int) layers.size()))
        return {};

    KnobManEngine::Writer w;
    w.section ("Layer");
    KnobManEngine::writeLayer (w, *layers[(size_t) index]);
    return juce::String::fromUTF8 (w.text.c_str(), (int) w.text.size());
}

std::shared_ptr<const KnobManDoc> KnobManDoc::edited (const std::vector<LayerEdit>& edits, int newWidth, int newHeight, int newFrames) const
{
    using namespace KnobManEngine;

    auto doc = std::make_shared<KnobManDoc>();
    doc->width = juce::jlimit (1, 2048, newWidth);
    doc->height = juce::jlimit (1, 2048, newHeight);
    doc->frames = juce::jlimit (1, 1024, newFrames);
    doc->oversampling = oversampling;
    doc->previewFrames = previewFrames;
    doc->alignHorizontal = alignHorizontal;
    doc->background = background;
    doc->notes = notes;
    doc->cache = std::make_unique<Cache>();

    for (int i = 0; i < 8; ++i)
        doc->curves[i] = curves[i];

    for (const auto& edit : edits)
    {
        const Layer* source = juce::isPositiveAndBelow (edit.source, (int) layers.size()) ? layers[(size_t) edit.source].get() : nullptr;

        // The layer's keys, the changed ones first (the first of a key is the one read).
        Writer w;
        w.section ("Layer");

        for (const auto& [key, value] : edit.values)
            w.str (key.toStdString(), value);

        if (source != nullptr)
            writeLayer (w, *source);

        Reader in (juce::String::fromUTF8 (w.text.c_str(), (int) w.text.size()));
        auto ly = std::make_shared<Layer>();
        in.setSection ("Layer");
        readLayer (in, *ly);
        ly->shown = in.readInt ("Visible", 1);
        ly->solo = in.readInt ("VisibleSolo", 0);

        // Its texture and picture as they were (the same file, the same name).
        if (source != nullptr)
        {
            const auto& from = source->prim;

            if (ly->prim.textureName == from.textureName)
            {
                ly->prim.tex = from.tex;
                ly->prim.texHex = from.texHex;
            }

            if (ly->prim.file == from.file)
            {
                ly->prim.image = from.image;
                ly->prim.imgHex = from.imgHex;
            }
        }

        // A texture another layer of the document draws with, by its name.
        if (ly->prim.tex == nullptr && ly->prim.textureName.isNotEmpty())
            for (const auto& other : layers)
                if (other->prim.textureName == ly->prim.textureName && other->prim.tex != nullptr)
                {
                    ly->prim.tex = other->prim.tex;
                    break;
                }

        doc->layers.push_back (ly);
    }

    settleVisibility (*doc);
    doc->body = writeBody (*doc);
    return doc;
}

std::shared_ptr<const KnobManDoc> KnobManDoc::parse (const juce::String& iniBody, juce::String& error)
{
    using namespace KnobManEngine;

    if (! iniBody.contains ("[Prefs]"))
    {
        error = "No [Prefs] section: this isn't a KnobMan document.";
        return nullptr;
    }

    auto doc = std::make_shared<KnobManDoc>();
    doc->body = iniBody;
    doc->cache = std::make_unique<Cache>();

    Reader in (iniBody);
    in.setSection ("Prefs");

    // A picture's hex, kept once however many layers read it.
    std::vector<std::shared_ptr<const std::string>> hexes;
    const auto keepHex = [&hexes] (std::string hex) -> std::shared_ptr<const std::string>
    {
        for (const auto& h : hexes)
            if (*h == hex)
                return h;

        hexes.push_back (std::make_shared<const std::string> (std::move (hex)));
        return hexes.back();
    };

    // The same picture embedded in several layers is decoded once.
    std::vector<std::pair<std::vector<juce::uint8>, std::shared_ptr<const Bmp>>> decoded;
    const auto decodeOnce = [&decoded] (std::vector<juce::uint8> bytes) -> std::shared_ptr<const Bmp>
    {
        if (bytes.empty())
            return nullptr;

        for (const auto& [data, bmp] : decoded)
            if (data == bytes)
                return bmp;

        auto bmp = decodeImage (bytes);
        decoded.emplace_back (std::move (bytes), bmp);
        return bmp;
    };

    doc->width = juce::jlimit (1, 2048, in.readInt ("OutputSizeX", 32));
    doc->height = juce::jlimit (1, 2048, in.readInt ("OutputSizeY", 32));
    doc->oversampling = juce::jlimit (0, 3, in.readInt ("OverSampling", 0));
    int frames = in.readInt ("NumOfImage", 0);

    if (frames <= 0)
        frames = in.readInt ("RenderFrames", 31);

    doc->frames = juce::jlimit (1, 1024, frames);
    doc->previewFrames = in.readInt ("PreviewFrames", 5);
    doc->alignHorizontal = in.readInt ("AlignHorizontal", 0);
    const int layerCount = juce::jlimit (0, 256, in.readInt ("Layers", 1));
    doc->background = juce::Colour ((juce::uint8) juce::jlimit (0, 255, in.readInt ("BkColorR", 255)),
                                    (juce::uint8) juce::jlimit (0, 255, in.readInt ("BkColorG", 255)),
                                    (juce::uint8) juce::jlimit (0, 255, in.readInt ("BkColorB", 255)));

    static const char* const names[] { "0", "1", "2", "3", "4", "a", "b", "c", "d", "e", "f", "5" };

    for (int i = 0; i < 8; ++i)
    {
        auto& c = doc->curves[i];
        const auto k = "Curve" + std::to_string (i + 1);
        c.lv[0] = in.readInt (k + "L0", 0);

        for (int j = 1; j < 11; ++j)
        {
            c.tm[j] = in.readInt (k + "T" + names[j], -1);
            c.lv[j] = in.readInt (k + "L" + names[j], -1);
        }

        c.lv[11] = in.readInt (k + "L5", 100);
        c.tm[0] = 0;
        c.tm[11] = 100;
        c.stepReso = in.readInt (k + "StepReso", 0);
    }

    std::vector<int> visibleAll ((size_t) layerCount, -1);

    for (int i = 0; i < layerCount; ++i)
        visibleAll[(size_t) i] = in.readInt ("Visible1_" + std::to_string (i), -1);

    static const char* const primNames[] { "None", "Image", "Circle", "CircleFill", "MetalCircle", "WaveCircle", "Sphere",
                                            "Rect", "RectFill", "Triangle", "Line", "RadiateLine", "H-Lines", "V-Lines", "Text", "Shape" };

    std::vector<int> solo;

    for (int i = 0; i < layerCount; ++i)
    {
        in.setSection ("Layer" + std::to_string (i + 1));
        auto ly = std::make_shared<KnobManEngine::Layer>();
        auto& p = ly->prim;
        auto& e = ly->eff;

        int visible = visibleAll[(size_t) i];

        if (visible < 0)
            visible = in.readInt ("Visible", 1);

        solo.push_back (in.readInt ("VisibleSolo", 0));
        ly->visible = visible != 0;
        ly->shown = visible;
        ly->solo = solo.back();

        readLayer (in, *ly);

        // A texture: one an earlier layer of the same name has, or the one in the file.
        for (const auto& earlier : doc->layers)
            if (p.textureName.isNotEmpty() && earlier->prim.textureName == p.textureName)
            {
                p.tex = earlier->prim.tex;
                break;
            }

        if (p.tex == nullptr)
        {
            std::string hex;

            if (const auto bmp = decodeOnce (in.extractFile ("TexBmp", &hex)))
            {
                p.tex = std::make_shared<const Tex> (*bmp);
                p.texHex = keepHex (std::move (hex));
            }
        }

        // An image: one an earlier layer of the same file has; the one in the file wins.
        for (const auto& earlier : doc->layers)
            if (p.file.isNotEmpty() && earlier->prim.file == p.file)
            {
                p.image = earlier->prim.image;
                break;
            }

        {
            std::string hex;

            if (const auto bmp = decodeOnce (in.extractFile ("ImgBmp", &hex)))
            {
                p.image = bmp;
                p.imgHex = keepHex (std::move (hex));
            }
        }

        // What can't be drawn as KnobMan would, said once.
        if (ly->visible)
        {
            const auto name = "Layer " + juce::String (i + 1);

            if (p.texturedepth != 0.0 && p.tex == nullptr && p.textureName.isNotEmpty())
                doc->notes.add (name + ": the texture \"" + p.textureName + "\" isn't in the file, so it's drawn without it.");

            if (p.type == 1 && p.image == nullptr)
                doc->notes.add (name + ": its picture isn't in the file, so it's empty.");

            if (p.type == 14)
            {
                const auto font = p.font.trim();

                if (font.isNotEmpty() && font != "undefined"
                    && ! juce::Font::findAllTypefaceNames().contains (font, true))
                    doc->notes.add (name + ": the font \"" + font + "\" isn't on this computer; another is used.");
            }
        }

        doc->layers.push_back (ly);
    }

    // A soloed layer: only those show.
    if (std::any_of (solo.begin(), solo.end(), [] (int s) { return s != 0; }))
        for (size_t i = 0; i < doc->layers.size(); ++i)
            doc->layers[i]->visible = solo[i] != 0;

    if (doc->layers.empty())
    {
        error = "The document has no layers.";
        return nullptr;
    }

    return doc;
}
