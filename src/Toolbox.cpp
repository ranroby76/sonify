// D:\workspace\Fanan_web_hub\src\Toolbox.cpp

#include "Toolbox.h"
#include "Primitives.h"
#include "StellaLookAndFeel.h"

namespace
{
    constexpr int headerHeight = 26, itemHeight = 34, iconSize = 26;
    const juce::Colour iconLine { 0xffd8d4ca };
}

//==============================================================================
/** One primitive: its icon and name. Drag it out, or double-click it. */
class Toolbox::Item final : public juce::Component,
                            public juce::SettableTooltipClient
{
public:
    Item (Toolbox& owner, const Primitive& p)
        : toolbox (owner), id (p.id), name (p.name), icon (p.icon), category (p.category), description (p.description)
    {
        setTooltip (p.description.isNotEmpty() ? p.description + "\nDrag it onto the window, or double-click it."
                                               : juce::String ("Drag it onto the window, or double-click it."));
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

    bool matches (const juce::String& text) const
    {
        return text.isEmpty() || name.containsIgnoreCase (text) || description.containsIgnoreCase (text) || category.containsIgnoreCase (text);
    }

    const juce::String& getCategory() const noexcept    { return category; }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced (6.0f, 2.0f);

        if (isMouseOverOrDragging())
        {
            g.setColour (Theme::raised);
            g.fillRoundedRectangle (area, 5.0f);
        }

        auto iconArea = area.removeFromLeft ((float) iconSize + 8.0f).withSizeKeepingCentre ((float) iconSize, (float) iconSize);
        drawIcon (g, iconArea, icon);

        area.removeFromLeft (6.0f);
        g.setColour (Theme::text.withAlpha (0.9f));
        g.setFont (Theme::font (13.5f));
        g.drawText (name, area, juce::Justification::centredLeft, true);
    }

    void mouseEnter (const juce::MouseEvent&) override    { repaint(); }
    void mouseExit (const juce::MouseEvent&) override     { repaint(); }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() < 4)
            return;

        auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);

        if (container == nullptr || container->isDragAndDropActive())
            return;

        // The drag shows the icon under the mouse.
        juce::Image image (juce::Image::ARGB, iconSize + 12, iconSize + 12, true);
        {
            juce::Graphics g (image);
            g.setColour (Theme::raised.withAlpha (0.92f));
            g.fillRoundedRectangle (image.getBounds().toFloat().reduced (0.5f), 6.0f);
            g.setColour (Theme::accent.withAlpha (0.8f));
            g.drawRoundedRectangle (image.getBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
            drawIcon (g, image.getBounds().toFloat().reduced (6.0f), icon);
        }

        const juce::Point<int> offset (-(iconSize + 12) / 2, -(iconSize + 12) / 2);
        container->startDragging (PrimitiveLibrary::dragDescription (id), this, juce::ScaledImage (image), false, &offset);
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (toolbox.onAdd != nullptr)
            toolbox.onAdd (id);
    }

private:
    Toolbox& toolbox;
    juce::String id, name, icon, category, description;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Item)
};

//==============================================================================
/** The categories and their items, filtered by the search text. */
class Toolbox::List final : public juce::Component
{
public:
    explicit List (Toolbox& owner)
    {
        const auto& library = PrimitiveLibrary::get();
        categories = library.getCategories();

        for (const auto& p : library.getAll())
            addAndMakeVisible (items.add (new Item (owner, p)));
    }

    void setFilter (const juce::String& text)
    {
        filter = text.trim();
        layOut();
    }

    void layOut()
    {
        headers.clear();
        int y = 4;

        for (const auto& category : categories)
        {
            bool any = false;

            for (auto* item : items)
                any = any || (item->getCategory() == category && item->matches (filter));

            if (! any)
                continue;

            headers.add ({ category, y });
            y += headerHeight;

            for (auto* item : items)
            {
                if (item->getCategory() != category)
                    continue;

                const bool shown = item->matches (filter);
                item->setVisible (shown);

                if (shown)
                {
                    item->setBounds (0, y, getWidth(), itemHeight);
                    y += itemHeight;
                }
            }

            y += 6;
        }

        for (auto* item : items)
            if (! item->matches (filter))
                item->setVisible (false);

        empty = headers.isEmpty();
        setSize (getWidth(), juce::jmax (y + 8, 60));
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        for (const auto& [category, y] : headers)
        {
            g.setColour (Theme::muted);
            g.setFont (Theme::font (11.0f, true));
            g.drawText (category.toUpperCase(), juce::Rectangle<int> (12, y, getWidth() - 24, headerHeight).withTrimmedTop (6),
                        juce::Justification::centredLeft, true);
        }

        if (empty)
        {
            g.setColour (Theme::muted);
            g.setFont (Theme::font (13.0f));
            g.drawText ("Nothing matches", getLocalBounds().withHeight (40), juce::Justification::centred, false);
        }
    }

    void resized() override
    {
        for (auto* item : items)
            item->setSize (getWidth(), itemHeight);
    }

private:
    juce::StringArray categories;
    juce::OwnedArray<Item> items;
    juce::Array<std::pair<juce::String, int>> headers;
    juce::String filter;
    bool empty = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (List)
};

//==============================================================================
Toolbox::Toolbox()
{
    search.setTextToShowWhenEmpty (juce::String::fromUTF8 ("Search\xe2\x80\xa6"), Theme::muted);
    search.setFont (Theme::font (13.5f));
    search.setIndents (8, 5);
    search.onTextChange = [this] { list->setFilter (search.getText()); };
    search.onEscapeKey = [this] { search.clear(); list->setFilter ({}); };
    addAndMakeVisible (search);

    list = std::make_unique<List> (*this);
    viewport.setViewedComponent (list.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
}

Toolbox::~Toolbox() = default;

void Toolbox::paint (juce::Graphics& g)
{
    g.fillAll (Theme::window);

    g.setColour (Theme::outline);
    g.fillRect (getWidth() - 1, 0, 1, getHeight());

    g.setColour (Theme::text);
    g.setFont (Theme::font (12.0f, true));
    g.drawText ("TOOLBOX", getLocalBounds().removeFromTop (34).withTrimmedLeft (14), juce::Justification::centredLeft, false);
}

void Toolbox::resized()
{
    auto area = getLocalBounds().withTrimmedRight (1);
    area.removeFromTop (34);
    search.setBounds (area.removeFromTop (28).reduced (10, 0));
    area.removeFromTop (6);
    viewport.setBounds (area);

    list->setSize (viewport.getMaximumVisibleWidth(), list->getHeight());
    list->layOut();
}

//==============================================================================
void Toolbox::drawIcon (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& icon)
{
    const auto line = iconLine;
    const auto accent = Theme::accent;
    const auto c = area.getCentre();
    const auto s = juce::jmin (area.getWidth(), area.getHeight());
    const auto box = area.withSizeKeepingCentre (s, s);

    auto knob = [&] (float diameter, juce::Colour body)
    {
        const auto r = juce::Rectangle<float> (diameter, diameter).withCentre (c);
        g.setColour (body);
        g.fillEllipse (r);
        g.setColour (line.withAlpha (0.55f));
        g.drawEllipse (r, 1.0f);

        const auto angle = juce::MathConstants<float>::pi * 0.25f;
        const juce::Point<float> tip (c.x + std::sin (angle) * diameter * 0.42f, c.y - std::cos (angle) * diameter * 0.42f);
        g.setColour (accent);
        g.drawLine ({ c, tip }, juce::jmax (1.5f, diameter * 0.09f));
    };

    if (icon == "knob")            { knob (s * 0.78f, juce::Colour (0xff2a2a2f)); return; }
    if (icon == "knob_small")      { knob (s * 0.5f, juce::Colour (0xff2a2a2f)); return; }
    if (icon == "knob_big")        { knob (s * 0.96f, juce::Colour (0xff6b6b72)); return; }

    if (icon == "slider_vertical" || icon == "slider_horizontal")
    {
        const bool vertical = icon == "slider_vertical";
        const auto track = vertical ? box.withSizeKeepingCentre (4.0f, s * 0.9f) : box.withSizeKeepingCentre (s * 0.9f, 4.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (track, 2.0f);
        g.setColour (accent);
        g.fillRoundedRectangle (vertical ? track.withTop (track.getCentreY()) : track.withWidth (track.getWidth() * 0.5f), 2.0f);
        const auto thumb = vertical ? juce::Rectangle<float> (s * 0.5f, 5.0f).withCentre (track.getCentre())
                                    : juce::Rectangle<float> (5.0f, s * 0.5f).withCentre (track.getCentre());
        g.setColour (juce::Colour (0xffe9e6df));
        g.fillRoundedRectangle (thumb, 1.5f);
        return;
    }

    if (icon == "switch")
    {
        g.setColour (juce::Colour (0xffffb020));
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ c.x, box.getY() + 4.0f }));
        const auto button = box.withTrimmedTop (s * 0.35f).reduced (s * 0.18f, s * 0.08f);
        g.setColour (juce::Colour (0xff4a4a50));
        g.fillRoundedRectangle (button, 3.0f);
        g.setColour (line.withAlpha (0.5f));
        g.drawRoundedRectangle (button, 3.0f, 1.0f);
        return;
    }

    if (icon == "selector")
    {
        const auto strip = box.withSizeKeepingCentre (s, s * 0.42f);
        const auto cell = strip.getWidth() / 3.0f;

        for (int i = 0; i < 3; ++i)
        {
            g.setColour (i == 1 ? accent : juce::Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (juce::Rectangle<float> (strip.getX() + (float) i * cell, strip.getY(), cell, strip.getHeight()).reduced (1.0f), 2.0f);
        }
        return;
    }

    if (icon == "dropdown")
    {
        // A box with the chosen option and the arrow that opens the list.
        const auto field = box.withSizeKeepingCentre (s, s * 0.42f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (field, 2.5f);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawRoundedRectangle (field, 2.5f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.fillRect (field.withTrimmedLeft (field.getWidth() * 0.14f).withWidth (field.getWidth() * 0.42f).withSizeKeepingCentre (field.getWidth() * 0.42f, 1.5f));

        juce::Path arrow;
        const auto ax = field.getRight() - field.getHeight() * 0.55f, ay = field.getCentreY(), r = field.getHeight() * 0.2f;
        arrow.addTriangle (ax - r, ay - r * 0.5f, ax + r, ay - r * 0.5f, ax, ay + r * 0.7f);
        g.setColour (accent);
        g.fillPath (arrow);
        return;
    }

    if (icon == "samples")
    {
        // A sampler's instruments: the list's box, a sample's waveform in it.
        const auto field = box.withSizeKeepingCentre (s, s * 0.46f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (field, 2.5f);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawRoundedRectangle (field, 2.5f, 1.0f);

        juce::Path wave;
        const auto left = field.getX() + field.getWidth() * 0.1f, width = field.getWidth() * 0.52f;
        const auto mid = field.getCentreY(), height = field.getHeight() * 0.36f;

        for (int i = 0; i <= 24; ++i)
        {
            const auto t = (float) i / 24.0f;
            const auto y = mid - std::sin (t * 18.0f) * height * (1.0f - t * 0.85f);
            if (i == 0) wave.startNewSubPath (left, y); else wave.lineTo (left + t * width, y);
        }

        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.strokePath (wave, juce::PathStrokeType (1.2f));

        juce::Path arrow;
        const auto ax = field.getRight() - field.getHeight() * 0.5f, ay = field.getCentreY(), r = field.getHeight() * 0.18f;
        arrow.addTriangle (ax - r, ay - r * 0.5f, ax + r, ay - r * 0.5f, ax, ay + r * 0.7f);
        g.setColour (accent);
        g.fillPath (arrow);
        return;
    }

    if (icon == "xy")
    {
        const auto pad = box.reduced (2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (pad, 3.0f);
        const juce::Point<float> dot (pad.getX() + pad.getWidth() * 0.65f, pad.getY() + pad.getHeight() * 0.35f);
        g.setColour (accent.withAlpha (0.5f));
        g.drawVerticalLine (juce::roundToInt (dot.x), pad.getY() + 1.0f, pad.getBottom() - 1.0f);
        g.drawHorizontalLine (juce::roundToInt (dot.y), pad.getX() + 1.0f, pad.getRight() - 1.0f);
        g.setColour (accent);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (dot));
        return;
    }

    if (icon == "presets")
    {
        const auto bar = box.withSizeKeepingCentre (s, s * 0.45f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (bar, 3.0f);
        juce::Path arrows;
        arrows.addTriangle (bar.getX() + 3.0f, bar.getCentreY(), bar.getX() + 7.0f, bar.getCentreY() - 3.5f, bar.getX() + 7.0f, bar.getCentreY() + 3.5f);
        arrows.addTriangle (bar.getRight() - 3.0f, bar.getCentreY(), bar.getRight() - 7.0f, bar.getCentreY() - 3.5f, bar.getRight() - 7.0f, bar.getCentreY() + 3.5f);
        g.setColour (accent);
        g.fillPath (arrows);
        g.setColour (line.withAlpha (0.7f));
        g.fillRect (bar.withSizeKeepingCentre (bar.getWidth() * 0.4f, 2.0f));
        return;
    }

    if (icon == "meter")
    {
        const auto w = s * 0.2f;

        for (int i = 0; i < 2; ++i)
        {
            const auto bar = juce::Rectangle<float> (c.x - w - 1.5f + (float) i * (w + 3.0f), box.getY() + 1.0f, w, s - 2.0f);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.fillRect (bar);
            const auto level = i == 0 ? 0.7f : 0.55f;
            g.setGradientFill (juce::ColourGradient (Theme::safe, 0.0f, bar.getBottom(), Theme::clip, 0.0f, bar.getY(), false));
            g.fillRect (bar.withTop (bar.getBottom() - bar.getHeight() * level));
        }
        return;
    }

    if (icon == "scope" || icon == "envelope" || icon == "filter")
    {
        const auto frame = box.reduced (1.0f, s * 0.12f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (frame, 3.0f);

        juce::Path path;
        const auto inner = frame.reduced (3.0f);

        if (icon == "scope")
        {
            for (int i = 0; i <= 24; ++i)
            {
                const auto t = (float) i / 24.0f;
                const juce::Point<float> p (inner.getX() + inner.getWidth() * t,
                                            inner.getCentreY() - std::sin (t * juce::MathConstants<float>::twoPi * 1.5f) * inner.getHeight() * 0.42f);
                if (i == 0) path.startNewSubPath (p); else path.lineTo (p);
            }
        }
        else if (icon == "envelope")
        {
            path.startNewSubPath (inner.getBottomLeft());
            path.lineTo (inner.getX() + inner.getWidth() * 0.2f, inner.getY());
            path.lineTo (inner.getX() + inner.getWidth() * 0.4f, inner.getY() + inner.getHeight() * 0.4f);
            path.lineTo (inner.getX() + inner.getWidth() * 0.75f, inner.getY() + inner.getHeight() * 0.4f);
            path.lineTo (inner.getBottomRight());
        }
        else
        {
            path.startNewSubPath (inner.getX(), inner.getY() + inner.getHeight() * 0.45f);
            path.lineTo (inner.getX() + inner.getWidth() * 0.5f, inner.getY() + inner.getHeight() * 0.45f);
            path.quadraticTo (inner.getX() + inner.getWidth() * 0.66f, inner.getY() - 1.0f,
                              inner.getX() + inner.getWidth() * 0.75f, inner.getY() + inner.getHeight() * 0.5f);
            path.lineTo (inner.getX() + inner.getWidth() * 0.9f, inner.getBottom());
        }

        g.setColour (accent);
        g.strokePath (path, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        return;
    }

    // The 3D displays: a few rows stacked in depth, the front one lit.
    if (icon == "wave3d" || icon == "morph3d" || icon == "filter3d" || icon == "waterfall3d")
    {
        const auto frame = box.reduced (1.0f, s * 0.12f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (frame, 3.0f);

        const auto inner = frame.reduced (3.0f);
        constexpr int rows = 4;

        auto heightAt = [&icon] (float x, float depth)   // 0..1
        {
            const auto pi = juce::MathConstants<float>::pi;

            if (icon == "filter3d")
            {
                const auto cutoff = 0.35f + 0.3f * depth;
                return x < cutoff ? 0.55f + 0.35f * std::exp (-(x - cutoff) * (x - cutoff) * 120.0f)
                                  : juce::jmax (0.0f, 0.55f + 0.35f * std::exp (-(x - cutoff) * (x - cutoff) * 120.0f) - (x - cutoff) * 2.2f);
            }

            if (icon == "waterfall3d")
                return 0.15f + 0.7f * std::exp (-(x - 0.25f) * (x - 0.25f) * 40.0f) * (1.0f - 0.4f * depth)
                     + 0.35f * std::exp (-(x - 0.6f - 0.1f * depth) * (x - 0.6f - 0.1f * depth) * 90.0f);

            const auto sine = 0.5f + 0.45f * std::sin (x * 2.0f * pi);
            const auto saw = 0.95f - 0.9f * std::fmod (x + 0.5f, 1.0f);
            return icon == "morph3d" ? sine + (saw - sine) * depth : sine;
        };

        for (int r = rows - 1; r >= 0; --r)   // back to front: nearer rows cover the ones behind
        {
            const auto depth = (float) r / (float) (rows - 1);
            const auto x0 = inner.getX() + inner.getWidth() * 0.24f * depth, span = inner.getWidth() * 0.76f;
            const auto base = inner.getBottom() - inner.getHeight() * 0.42f * depth, tall = inner.getHeight() * 0.5f;

            juce::Path curve;

            for (int i = 0; i <= 20; ++i)
            {
                const auto x = (float) i / 20.0f;
                const juce::Point<float> p (x0 + span * x, base - tall * heightAt (x, depth));
                if (i == 0) curve.startNewSubPath (p); else curve.lineTo (p);
            }

            auto under = curve;
            under.lineTo (x0 + span, base);
            under.lineTo (x0, base);
            under.closeSubPath();
            g.setColour (juce::Colour (0xff101014));
            g.fillPath (under);

            g.setColour (r == 0 ? accent : accent.withAlpha (0.25f + 0.15f * (1.0f - depth)));
            g.strokePath (curve, juce::PathStrokeType (r == 0 ? 1.6f : 1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        return;
    }

    if (icon == "lamp")
    {
        const auto lamp = juce::Rectangle<float> (s * 0.42f, s * 0.42f).withCentre (c);
        g.setColour (Theme::safe.withAlpha (0.3f));
        g.fillEllipse (lamp.expanded (4.0f));
        g.setColour (Theme::safe);
        g.fillEllipse (lamp);
        return;
    }

    if (icon == "picture" || icon == "background")
    {
        const bool background = icon == "background";
        const auto frame = background ? box.reduced (1.0f, s * 0.14f) : box.reduced (s * 0.12f, s * 0.2f);

        g.setColour (juce::Colour (0xff2f4a66));
        g.fillRoundedRectangle (frame, 2.0f);

        juce::Path hills;
        hills.startNewSubPath (frame.getBottomLeft());
        hills.lineTo (frame.getX() + frame.getWidth() * 0.35f, frame.getY() + frame.getHeight() * 0.45f);
        hills.lineTo (frame.getX() + frame.getWidth() * 0.55f, frame.getY() + frame.getHeight() * 0.7f);
        hills.lineTo (frame.getX() + frame.getWidth() * 0.75f, frame.getY() + frame.getHeight() * 0.5f);
        hills.lineTo (frame.getBottomRight());
        hills.closeSubPath();
        g.setColour (juce::Colour (0xff5f8f5a));
        g.fillPath (hills);

        g.setColour (juce::Colour (0xffffd166));
        g.fillEllipse (juce::Rectangle<float> (s * 0.16f, s * 0.16f).withCentre ({ frame.getX() + frame.getWidth() * 0.75f, frame.getY() + frame.getHeight() * 0.28f }));

        g.setColour (line.withAlpha (background ? 0.9f : 0.5f));
        g.drawRoundedRectangle (frame, 2.0f, background ? 1.5f : 1.0f);

        if (background)
        {
            // Small controls sitting on it: it goes behind everything.
            g.setColour (juce::Colour (0xff2a2a2f));
            g.fillEllipse (juce::Rectangle<float> (s * 0.22f, s * 0.22f).withCentre ({ frame.getX() + frame.getWidth() * 0.25f, frame.getCentreY() + 2.0f }));
            g.setColour (accent);
            g.drawEllipse (juce::Rectangle<float> (s * 0.22f, s * 0.22f).withCentre ({ frame.getX() + frame.getWidth() * 0.25f, frame.getCentreY() + 2.0f }), 1.0f);
        }
        return;
    }

    if (icon == "title" || icon == "label")
    {
        const bool title = icon == "title";
        g.setColour (line);
        g.setFont (Theme::font (title ? s * 0.62f : s * 0.48f, title));
        g.drawText (title ? "T" : "Aa", box, juce::Justification::centred, false);
        return;
    }

    if (icon == "group")
    {
        const auto frame = box.reduced (1.0f, s * 0.12f);
        g.setColour (line.withAlpha (0.55f));
        g.drawRoundedRectangle (frame, 3.0f, 1.0f);
        g.setColour (line);
        g.fillRect (juce::Rectangle<float> (frame.getX() + 3.0f, frame.getY() + 3.0f, frame.getWidth() * 0.45f, 2.5f));
        return;
    }

    if (icon == "keyboard")
    {
        // An octave, one key down.
        const auto bed = box.withSizeKeepingCentre (s, s * 0.62f);
        const auto whiteW = bed.getWidth() / 7.0f;

        g.setColour (juce::Colour (0xff111113));
        g.fillRoundedRectangle (bed, 2.0f);

        for (int i = 0; i < 7; ++i)
        {
            g.setColour (i == 2 ? accent : juce::Colour (0xffefece5));
            g.fillRect (juce::Rectangle<float> (bed.getX() + (float) i * whiteW + 0.5f, bed.getY(), whiteW - 1.0f, bed.getHeight()));
        }

        g.setColour (juce::Colour (0xff141416));

        for (const auto i : { 1, 2, 4, 5, 6 })
            g.fillRect (juce::Rectangle<float> (bed.getX() + (float) i * whiteW - whiteW * 0.3f, bed.getY(), whiteW * 0.6f, bed.getHeight() * 0.6f));

        return;
    }

    if (icon == "registration")
    {
        // A padlock in the badge's amber.
        const auto amber = juce::Colour (0xffffb020);
        const auto body = juce::Rectangle<float> (s * 0.58f, s * 0.42f).withCentre (c.translated (0.0f, s * 0.14f));
        const auto r = s * 0.17f, top = body.getY() - s * 0.06f;

        juce::Path shackle;
        shackle.startNewSubPath (c.x - r, body.getY());
        shackle.lineTo (c.x - r, top);
        shackle.addCentredArc (c.x, top, r, r, 0.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi);
        shackle.lineTo (c.x + r, body.getY());

        g.setColour (amber);
        g.strokePath (shackle, juce::PathStrokeType (juce::jmax (1.5f, s * 0.08f)));
        g.fillRoundedRectangle (body, 2.0f);
        g.setColour (juce::Colour (0xff141418));
        g.fillEllipse (juce::Rectangle<float> (s * 0.1f, s * 0.1f).withCentre (body.getCentre()));
        return;
    }

    if (icon == "tabs")
    {
        // Three tabs, the first lit.
        const auto row = box.reduced (2.0f).withSizeKeepingCentre (box.getWidth() - 4.0f, box.getHeight() * 0.42f);
        const auto cellW = row.getWidth() / 3.0f;

        for (int i = 0; i < 3; ++i)
        {
            const auto cell = juce::Rectangle<float> (row.getX() + (float) i * cellW, row.getY(), cellW, row.getHeight()).reduced (0.8f, 0.0f);
            g.setColour (i == 0 ? accent : line.withAlpha (0.35f));
            g.fillRoundedRectangle (cell, 1.5f);
        }

        return;
    }

    if (icon == "range")
    {
        // A slot, the band between two half-moon handles lit.
        const auto track = box.withSizeKeepingCentre (s * 0.92f, 4.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (track, 2.0f);
        const auto x0 = track.getX() + track.getWidth() * 0.28f, x1 = track.getX() + track.getWidth() * 0.72f;
        g.setColour (accent);
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (x0, track.getY(), x1, track.getBottom()));

        for (const auto& [x, left] : { std::pair<float, bool> { x0, true }, { x1, false } })
        {
            juce::Path moon;
            moon.addPieSegment (juce::Rectangle<float> (s * 0.36f, s * 0.36f).withCentre ({ x, c.y }), left ? juce::MathConstants<float>::pi : 0.0f,
                                left ? juce::MathConstants<float>::twoPi : juce::MathConstants<float>::pi, 0.0f);
            g.setColour (line);
            g.fillPath (moon);
        }

        return;
    }

    if (icon == "meterknob")
    {
        // A dial plate, its meter's window above, a knob in the middle.
        const auto plate = box.reduced (1.0f);
        g.setColour (juce::Colour (0xff2a2a2f));
        g.fillEllipse (plate);
        juce::Path window;
        window.addPieSegment (plate.reduced (s * 0.06f), -0.95f, 0.95f, 0.62f);
        g.setColour (juce::Colour (0xfff1e4c3));
        g.fillPath (window);
        g.setColour (accent);
        g.drawLine ({ c, c.translated (std::sin (0.4f) * s * 0.42f, -std::cos (0.4f) * s * 0.42f) }, 1.2f);
        knob (s * 0.46f, juce::Colour (0xff6b6b72));
        return;
    }

    if (icon == "dualknob")
    {
        knob (s * 0.92f, juce::Colour (0xff2a2a2f));
        knob (s * 0.46f, juce::Colour (0xff8a8a92));
        return;
    }

    if (icon == "line")
    {
        // A line bowed through its bend point, its two ends as dots.
        const auto inner = box.reduced (3.0f);
        const juce::Point<float> a (inner.getX() + 1.5f, inner.getBottom() - inner.getHeight() * 0.22f),
                                 b (inner.getRight() - 1.5f, inner.getBottom() - inner.getHeight() * 0.22f),
                                 bend (inner.getCentreX(), inner.getY() + inner.getHeight() * 0.2f);
        juce::Path curve;
        curve.startNewSubPath (a);
        curve.quadraticTo (bend * 2.0f - (a + b) * 0.5f, b);
        g.setColour (line);
        g.strokePath (curve, juce::PathStrokeType (juce::jmax (1.5f, s * 0.09f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        for (const auto& end : { a, b })
            g.fillEllipse (juce::Rectangle<float> (s * 0.2f, s * 0.2f).withCentre (end));

        juce::Path diamond;
        const auto r = s * 0.11f;
        diamond.addQuadrilateral (bend.x, bend.y - r, bend.x + r, bend.y, bend.x, bend.y + r, bend.x - r, bend.y);
        g.setColour (accent);
        g.fillPath (diamond);
        return;
    }

    if (icon == "shape" || icon == "star")
    {
        auto star = juce::Drawable::parseSVGPath ("M50,5 L61,39 L97,39 L68,61 L79,95 L50,74 L21,95 L32,61 L3,39 L39,39 Z");
        star.applyTransform (star.getTransformToScaleToFit (box.reduced (2.0f), true));
        g.setColour (line);
        g.fillPath (star);
        return;
    }

    if (icon == "rectangle" || icon == "circle" || icon == "triangle")
    {
        const auto inner = box.reduced (3.0f);
        juce::Path shape;

        if (icon == "rectangle")
            shape.addRoundedRectangle (inner.withSizeKeepingCentre (inner.getWidth(), inner.getHeight() * 0.68f), 2.5f);
        else if (icon == "circle")
            shape.addEllipse (inner.withSizeKeepingCentre (juce::jmin (inner.getWidth(), inner.getHeight()), juce::jmin (inner.getWidth(), inner.getHeight())));
        else
            shape.addTriangle (inner.getCentreX(), inner.getY(), inner.getRight(), inner.getBottom() - inner.getHeight() * 0.08f,
                               inner.getX(), inner.getBottom() - inner.getHeight() * 0.08f);

        g.setColour (line);
        g.fillPath (shape);
        return;
    }

    g.setColour (line.withAlpha (0.6f));
    g.drawRoundedRectangle (box.reduced (3.0f), 3.0f, 1.0f);
}
