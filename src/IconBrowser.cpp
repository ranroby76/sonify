// D:\workspace\Fanan_web_hub\src\IconBrowser.cpp

#include "IconBrowser.h"

#include "IconLibrary.h"
#include "PanelParts.h"
#include "StellaLookAndFeel.h"

namespace
{
    constexpr int plainCell = 44, badgeCell = 64;

    const juce::StringArray shapeIds { "rect", "circle", "rhombus", "hexagon", "pentagon", "octagon",
                                      "triangle", "wide", "small", "none" };
}

void IconBrowser::familyColours (const juce::String& name, juce::Colour& badge, juce::Colour& fore)
{
    // Its family: the first of its tags.
    const auto index = IconLibrary::indexOf (name);
    const auto family = index >= 0 ? IconLibrary::all()[(size_t) index].tags.upToFirstOccurrenceOf (" ", false, false) : juce::String();

    struct Family { const char* name; juce::uint32 badge, fore; };
    static const Family families[] {
        { "drive",      0xffe0622a, 0xffffffff }, { "dynamics", 0xff2f7bff, 0xffffffff }, { "filter",   0xffe8b52a, 0xff1a1a1c },
        { "modulation", 0xff22a99b, 0xffffffff }, { "pitch",    0xff8a4fd8, 0xffffffff }, { "time",     0xff3fa34d, 0xffffffff },
        { "ambience",   0xff4f5fd8, 0xffffffff }, { "amp",      0xff7a5236, 0xfff2e6d0 }, { "utility",  0xff5e6672, 0xffffffff } };

    badge = juce::Colour (PanelParts::standardBadgeColour);
    fore = juce::Colours::white;

    for (const auto& f : families)
        if (family == f.name)
        {
            badge = juce::Colour (f.badge);
            fore = juce::Colour (f.fore);
        }
}

//==============================================================================
struct IconBrowser::Grid final : public juce::Component
{
    IconBrowser& owner;
    std::vector<int> icons;
    int hovered = -1;

    explicit Grid (IconBrowser& o) : owner (o), cell (o.badges ? badgeCell : plainCell) {}

    const int cell;

    juce::String shape() const
    {
        return shapeIds[juce::jlimit (0, shapeIds.size() - 1, owner.shapeBox.getSelectedId() - 1)];
    }

    /** An icon on its badge (or plain) in a box. */
    void paintIcon (juce::Graphics& g, int index, juce::Rectangle<float> box, bool lit) const
    {
        const auto path = IconLibrary::pathOf (index);

        if (! owner.badges)
        {
            g.setColour (lit ? Theme::text : Theme::text.withAlpha (0.82f));
            g.fillPath (path, juce::AffineTransform::translation (box.getCentreX() - 12.0f, box.getCentreY() - 12.0f));
            return;
        }

        juce::Colour badge, fore;
        familyColours (IconLibrary::all()[(size_t) index].name, badge, fore);

        if (shape() == "none")
        {
            g.setColour (badge.brighter (0.3f));
            g.fillPath (path, path.getTransformToScaleToFit (box.reduced (box.getWidth() * 0.18f), true));
            return;
        }

        const auto inner = PanelParts::paintBadge (g, box.reduced (box.getWidth() * 0.06f), shape(), lit ? badge.brighter (0.15f) : badge);
        g.setColour (fore);
        g.fillPath (path, path.getTransformToScaleToFit (inner, true));
    }

    int columns() const   { return juce::jmax (1, getWidth() / cell); }

    void fit (int width)
    {
        const auto across = juce::jmax (1, width / cell);
        setSize (width, juce::jmax (cell, ((int) icons.size() + across - 1) / across * cell));
        repaint();
    }

    int at (juce::Point<int> p) const
    {
        const auto i = (p.y / cell) * columns() + p.x / cell;
        return p.x >= 0 && p.x < columns() * cell && juce::isPositiveAndBelow (i, (int) icons.size()) ? i : -1;
    }

    void paint (juce::Graphics& g) override
    {
        // The rows showing only: there are thousands.
        const auto clip = g.getClipBounds();
        const auto across = columns();
        const auto first = juce::jmax (0, clip.getY() / cell), last = clip.getBottom() / cell;

        for (int row = first; row <= last; ++row)
        {
            for (int c = 0; c < across; ++c)
            {
                const auto i = row * across + c;

                if (! juce::isPositiveAndBelow (i, (int) icons.size()))
                    return;

                const juce::Rectangle<float> box ((float) (c * cell), (float) (row * cell), (float) cell, (float) cell);

                if (i == hovered)
                {
                    g.setColour (Theme::accent.withAlpha (0.22f));
                    g.fillRoundedRectangle (box.reduced (2.0f), 5.0f);
                }

                paintIcon (g, icons[(size_t) i], owner.badges ? box.reduced (6.0f) : box, i == hovered);
            }
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        if (const auto i = at (e.getPosition()); i != hovered)
        {
            hovered = i;
            owner.nameLabel.setText (i >= 0 ? IconLibrary::token (IconLibrary::all()[(size_t) icons[(size_t) i]].name)
                                            : juce::String (owner.dragOut ? "Drag an icon onto the window" : "Click an icon to put it in"),
                                     juce::dontSendNotification);
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hovered = -1;
        repaint();
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        dragging = false;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! owner.dragOut || dragging || e.getDistanceFromDragStart() < 5)
            return;

        const auto i = at (e.getMouseDownPosition());
        auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);

        if (i < 0 || container == nullptr)
            return;

        dragging = true;
        const auto name = IconLibrary::all()[(size_t) icons[(size_t) i]].name;
        auto* description = new juce::DynamicObject();
        description->setProperty ("galleryIcon", name);

        // On its badge: the badge and its colours go with it.
        if (owner.badges)
        {
            juce::Colour badge, fore;
            familyColours (name, badge, fore);
            description->setProperty ("badge", shape());
            description->setProperty ("badgeColour", badge.toDisplayString (true));
            description->setProperty ("iconColour", (shape() == "none" ? badge.brighter (0.3f) : fore).toDisplayString (true));
        }

        juce::Image image (juce::Image::ARGB, 40, 40, true);
        {
            juce::Graphics g (image);

            if (owner.badges)
            {
                paintIcon (g, icons[(size_t) i], image.getBounds().toFloat(), false);
            }
            else
            {
                const auto path = IconLibrary::pathOf (icons[(size_t) i]);
                g.setColour (Theme::accent);
                g.fillPath (path, path.getTransformToScaleToFit (image.getBounds().toFloat().reduced (4.0f), true));
            }
        }

        const juce::Point<int> offset (-20, -20);
        container->startDragging (juce::var (description), this, juce::ScaledImage (image), true, &offset);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (const auto i = at (e.getPosition()); i >= 0 && ! dragging && owner.onPick != nullptr)
            owner.onPick (IconLibrary::all()[(size_t) icons[(size_t) i]].name);

        dragging = false;
    }

    bool dragging = false;
};

//==============================================================================
IconBrowser::IconBrowser (std::function<void (const juce::String&)> pick, bool canDragOut, bool onBadges)
    : onPick (std::move (pick)), dragOut (canDragOut), badges (onBadges)
{
    shapeBox.addItemList ({ "Rectangle badges", "Circle badges", "Rhombus badges", "Hexagon badges", "Pentagon badges",
                            "Octagon badges", "Triangle badges", "Wide badges", "Small square badges", "No badge" }, 1);
    shapeBox.setSelectedId (1, juce::dontSendNotification);
    shapeBox.setTooltip ("The badges' shape (dropped, an icon keeps it: its menu changes it, and its colours)");
    shapeBox.onChange = [this] { grid->repaint(); };

    if (badges)
        addAndMakeVisible (shapeBox);

    grid = std::make_unique<Grid> (*this);

    searchBox.setTextToShowWhenEmpty ("Search: sine, filter, play, mic, arrow...", Theme::muted);
    searchBox.setFont (Theme::font (14.0f));
    searchBox.onTextChange = [this] { startTimer (200); };
    searchBox.onReturnKey = [this] { stopTimer(); refill(); };
    addAndMakeVisible (searchBox);

    categoryBox.addItem ("Every category", 1);
    categoryBox.addItemList (IconLibrary::categories(), 2);
    categoryBox.setSelectedId (juce::jmax (1, IconLibrary::categories().indexOf (badges ? fxCategory : "Audio") + 2), juce::dontSendNotification);
    categoryBox.onChange = [this] { refill(); };

    if (! badges)
        addAndMakeVisible (categoryBox);   // badges: the Guitar FX icons only

    nameLabel.setFont (Theme::font (13.0f));
    nameLabel.setColour (juce::Label::textColourId, Theme::muted);
    nameLabel.setText (dragOut ? "Drag an icon onto the window" : "Click an icon to put it in", juce::dontSendNotification);
    addAndMakeVisible (nameLabel);

    view.setViewedComponent (grid.get(), false);
    view.setScrollBarsShown (true, false);
    addAndMakeVisible (view);

    setSize (560, 480);
    refill();
}

IconBrowser::~IconBrowser() = default;

void IconBrowser::show (juce::Component& near, std::function<void (const juce::String&)> pick)
{
    juce::CallOutBox::launchAsynchronously (std::make_unique<IconBrowser> (std::move (pick)), near.getScreenBounds(), nullptr);
}

void IconBrowser::refill()
{
    const auto category = categoryBox.getSelectedId() <= 1 ? juce::String() : categoryBox.getText();
    grid->icons = IconLibrary::search (searchBox.getText().trim(), category, 4000);
    grid->hovered = -1;
    grid->fit (view.getMaximumVisibleWidth());
    view.setViewPosition (0, 0);
    nameLabel.setText (juce::String ((int) grid->icons.size()) + (grid->icons.size() == 1 ? " icon" : " icons")
                           + (grid->icons.size() >= 4000 ? juce::String (" (the first 4000: search for fewer)") : juce::String()),
                       juce::dontSendNotification);
}

void IconBrowser::timerCallback()
{
    stopTimer();
    refill();
}

void IconBrowser::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
}

void IconBrowser::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto top = r.removeFromTop (28);
    (badges ? static_cast<juce::Component&> (shapeBox) : categoryBox).setBounds (top.removeFromRight (190));
    top.removeFromRight (8);
    searchBox.setBounds (top);
    r.removeFromTop (6);
    nameLabel.setBounds (r.removeFromBottom (22));
    r.removeFromBottom (4);
    view.setBounds (r);
    grid->fit (view.getMaximumVisibleWidth());
}
