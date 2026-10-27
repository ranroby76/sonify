// D:\workspace\Fanan_web_hub\src\tomahawk\GrexSoundsParts.h
//
// Pieces of Grex's SoundsTab.h (its SOUNDS tab), as they are in Grex, for the Tomahawk's window:
// the instrument picture with its smoke (InstrImageDisplay) and the PACK > CATEGORY > INSTRUMENT
// columns (SelectorColumn, SoundsTab's own member, here GrexSounds'). In namespace Tomahawk (see
// TomahawkJuce.h).

#pragma once

#include "TomahawkJuce.h"
#include "BalladaPalette.h"
#include <random>

namespace Tomahawk
{

class InstrImageDisplay : public juce::Component, private juce::Timer
{
public:
    InstrImageDisplay()
    {
        setOpaque(false);
        setInterceptsMouseClicks(false, false);
        startTimerHz(30);
    }
    ~InstrImageDisplay() override { stopTimer(); }

    void set(juce::Image img)
    {
        image = std::move(img);
        smoke.clear();
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        if (!image.isValid()) return;
        const float W = (float)getWidth(), H = (float)getHeight();

        const float cx = W * 0.5f, cy = H * 0.5f;
        const float iw = W * 0.75f, ih = H * 0.75f;
        for (int layer = 5; layer >= 1; --layer)
        {
            const float expand = (float)layer * 5.0f;
            const float alpha  = 0.04f * (float)(6 - layer) / 5.0f;
            g.setColour(juce::Colours::white.withAlpha(alpha));
            g.fillEllipse(cx - iw*0.5f - expand, cy - ih*0.5f - expand,
                          iw + expand*2.0f, ih + expand*2.0f);
        }

        for (auto& p : smoke)
        {
            juce::ColourGradient gr(juce::Colours::white.withAlpha(p.alpha),
                                    p.x, p.y,
                                    juce::Colours::white.withAlpha(0.0f),
                                    p.x + p.r, p.y, true);
            g.setGradientFill(gr);
            g.fillEllipse(p.x - p.r, p.y - p.r, p.r * 2.0f, p.r * 2.0f);
        }

        g.drawImageWithin(image, 0, 0, (int)W, (int)H, juce::RectanglePlacement::centred);
    }

private:
    struct SmokeParticle { float x, y, vy, r, alpha; };
    std::vector<SmokeParticle> smoke;
    juce::Image image;
    float spawnTimer = 0;
    std::mt19937 rng { std::random_device{}() };
    std::uniform_real_distribution<float> rnd { 0.f, 1.f };

    void timerCallback() override
    {
        const float W = (float)getWidth(), H = (float)getHeight();
        if (W <= 0 || !image.isValid()) return;

        spawnTimer += 1.0f;
        if (spawnTimer > 2.5f && (int)smoke.size() < 16)
        {
            smoke.push_back({ W * 0.15f + rnd(rng) * W * 0.7f,
                              H * 0.8f + rnd(rng) * H * 0.1f,
                              -(0.25f + rnd(rng) * 0.4f),
                              3.0f + rnd(rng) * 5.0f,
                              0.12f + rnd(rng) * 0.08f });
            spawnTimer = 0;
        }
        for (auto& p : smoke) { p.y += p.vy; p.r += 0.12f; p.alpha -= 0.0025f; }
        smoke.erase(std::remove_if(smoke.begin(), smoke.end(),
            [](const SmokeParticle& p){ return p.alpha <= 0 || p.y < -20; }), smoke.end());
        repaint();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InstrImageDisplay)
};

struct GrexSounds
{
    //==========================================================================
    // SelectorColumn  -  ONE VERTICAL BAND OF THE PACK > CATEGORY > INSTRUMENT
    // HIERARCHY.  Replaces the horizontal TimbreButton / InstrButton rows.
    //==========================================================================
    //
    // WHY IT IS NOT A COLUMN OF BUTTONS.  Rob's rule for this surface is that
    // there are NO horizontal separators between entries - they stay invisible.
    // A button draws its own outline by definition, so forty of them stacked is
    // forty boxes however faint the fill is.  Painting the rows here means the
    // only ink on the whole selector is the text plus one vertical rule per
    // column, which is the "table missing three of its borders" the feature
    // panel already uses.
    //
    // SELECTION IS AMBER TEXT, NOT A FILL.  A border is what normally says
    // "this one", and there are no borders here.  Gold is what selected
    // already means everywhere else in this tab, so it carries with no new
    // vocabulary.  Hover gets a faint wash - that is feedback, not state, and
    // it disappears the moment the mouse leaves.
    //
    // TOP-ALIGNED WITH A CAPPED ROW HEIGHT.  Dividing the height by the item
    // count would give the three-entry PACK column rows a third of the panel
    // tall, and a six-category ORIENTAL pack rows twice the height of GM's
    // sixteen.  Rows are a fixed size per column and stack from the top, so a
    // short list is a short list rather than a stretched one.
    //==========================================================================
    class SelectorColumn : public juce::Component
    {
    public:
        std::function<void (int)> onRowClicked;

        void setTitle (const juce::String& t) { title = t; repaint(); }
        void setMaxRowH (int h)               { maxRowH = juce::jmax (12, h); }

        /** THE RULE IS BETWEEN COLUMNS, so the last one does not get it: a rule
            after INSTRUMENT would box that column against the animation panel
            rather than separate it from a next column that does not exist. */
        void setDrawRule (bool r) { drawRule = r; repaint(); }

        /** The names (and their numbers) this much bigger, the rows as they are (the Tomahawk's
            are 1.5 times Grex's). */
        void setTextScale (float s) { textScale = juce::jlimit (0.5f, 3.0f, s); repaint(); }

        /** Dimmed = present but not offering anything, which is the style-mode
            PACK column.  Kept on screen rather than removed so the tab does not
            change shape when the mode buttons are used. */
        void setDimmed (bool d) { dimmed = d; repaint(); }

        void setItems (const juce::StringArray& names)
        {
            items = names;
            // A PLAIN LOOP, not insertMultiple: every other container fill in
            // this project uses resize() or add(), and a one-line convenience
            // that nothing else here uses is exactly what turned into a build
            // break two edits ago.
            numbers.clear();
            for (int k = 0; k < items.size(); ++k) numbers.add (-1);

            // A SHORTER LIST MUST NOT KEEP A LONGER LIST'S INDEX.  Every caller
            // does set the selection immediately afterwards, but setSelected
            // early-returns when the value is unchanged - so a stale index that
            // happens to equal the new one would survive as a highlight on a
            // different sound.  Clamped here, where the list changes.
            if (selected >= items.size()) selected = -1;
            hovered = -1;
            repaint();
        }

        /** Right-aligned trailing number per row - the GM program change, or a
            pack flag.  -1 hides it. */
        void setNumber (int row, int n)
        {
            if (row >= 0 && row < numbers.size() && numbers[row] != n)
            { numbers.set (row, n); repaint(); }
        }

        void setSelected (int idx)
        {
            if (selected == idx) return;
            selected = idx; repaint();
        }

        int  getSelected()   const noexcept { return selected; }
        bool hasSelection()  const noexcept { return selected >= 0 && selected < items.size(); }
        int  getNumItems()   const noexcept { return items.size(); }

        /** Height one row wants, so the caller can size the column to its
            content instead of guessing. */
        int rowHeightFor (int availableH) const noexcept
        {
            const int n = juce::jmax (1, items.size());
            return juce::jlimit (13, maxRowH, availableH / n);
        }

        void paint (juce::Graphics& g) override
        {
            const int w = getWidth(), h = getHeight();
            int y = 0;

            if (title.isNotEmpty())
            {
                g.setColour (juce::Colours::white.withAlpha (dimmed ? 0.18f : 0.38f));
                g.setFont (juce::Font (9.5f, juce::Font::bold));
                g.drawFittedText (title, juce::Rectangle<int> (2, 0, w - 8, kTitleH),
                                  juce::Justification::centredLeft, 1);
                y = kTitleH;
            }

            const int rowH = rowHeightFor (juce::jmax (1, h - y));
            const float fh = juce::jlimit (8.5f, 12.0f, rowH * 0.58f) * textScale;

            for (int i = 0; i < items.size(); ++i)
            {
                const int ry = y + i * rowH;
                if (ry + rowH > h) break;                 // never draw past the band

                const juce::Rectangle<int> row (0, ry, w - 4, rowH);

                // HOVER IS THE ONLY FILL, and only when the column is live.
                if (i == hovered && ! dimmed)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.07f));
                    g.fillRect (row);
                }

                const bool sel = (i == selected);
                g.setColour (dimmed  ? juce::Colours::white.withAlpha (0.16f)
                           : sel     ? juce::Colour (Betel::Pal::kAccent).brighter (0.35f)
                                     : juce::Colours::white.withAlpha (0.72f));
                // THE PROVEN IDIOM IN THIS FILE, deliberately.  My first attempt
                // was `withStyle ("Bold")`, which does not compile: Font::withStyle
                // takes the int FontStyleFlags and the string overload is called
                // withTypefaceStyle.  Rather than reach for a third spelling,
                // this is the exact two-argument form every other font in
                // SoundsTab.h already uses and that therefore certainly builds.
                g.setFont (juce::Font (fh, sel ? juce::Font::bold : juce::Font::plain));

                // The trailing number needs its own lane or a long name runs
                // straight through it.
                const int numW = numbers[i] >= 0 ? juce::jmin (juce::roundToInt (30.0f * textScale), w / 4) : 0;
                g.drawFittedText (items[i], row.withTrimmedLeft (6).withTrimmedRight (numW + 2),
                                  juce::Justification::centredLeft, 1);

                if (numbers[i] >= 0)
                {
                    g.setColour (dimmed ? juce::Colours::white.withAlpha (0.12f)
                                        : sel ? juce::Colour (Betel::Pal::kAccent).withAlpha (0.85f)
                                              : juce::Colours::white.withAlpha (0.30f));
                    g.setFont (juce::Font (juce::jmax (7.5f * textScale, fh - 2.0f * textScale), juce::Font::plain));
                    g.drawFittedText (juce::String (numbers[i]),
                                      row.withTrimmedRight (4), juce::Justification::centredRight, 1);
                }
            }

            // NO HORIZONTAL SEPARATORS ANYWHERE - see the header comment.  The
            // vertical rule is the only long line this control ever draws.
            if (drawRule)
            {
                g.setColour (juce::Colours::white.withAlpha (dimmed ? 0.10f : 0.22f));
                g.drawVerticalLine (w - 1, 0.0f, (float) h);
            }
        }

        void mouseMove (const juce::MouseEvent& e) override  { setHover (rowAt (e.y)); }
        void mouseExit (const juce::MouseEvent&)   override  { setHover (-1); }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (dimmed) return;
            const int r = rowAt (e.y);
            if (r >= 0 && r < items.size() && onRowClicked) onRowClicked (r);
        }

    private:
        static constexpr int kTitleH = 13;

        int rowAt (int py) const noexcept
        {
            const int top = title.isNotEmpty() ? kTitleH : 0;
            if (py < top) return -1;
            const int rowH = rowHeightFor (juce::jmax (1, getHeight() - top));
            const int r = (py - top) / juce::jmax (1, rowH);
            return r < items.size() ? r : -1;
        }

        void setHover (int r) { if (hovered != r) { hovered = r; repaint(); } }

        juce::StringArray items;
        juce::Array<int>  numbers;
        juce::String      title;
        int  selected = -1, hovered = -1, maxRowH = 26;
        float textScale = 1.0f;
        bool drawRule = true, dimmed = false;
    };

};

} // namespace Tomahawk
