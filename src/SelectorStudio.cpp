// D:\workspace\Fanan_web_hub\src\SelectorStudio.cpp

#include "SelectorStudio.h"
#include "GalleryHub.h"
#include "Render3D.h"

#include "FontLibrary.h"
#include "LayerRender.h"
#include "LayerStyleWindow.h"
#include "Model3D.h"
#include "PluginCanvas.h"
#include "SelectorSet.h"
#include "StellaLookAndFeel.h"

#include <map>

namespace
{
    juce::var object()    { return juce::var (new juce::DynamicObject()); }

    void put (const juce::var& v, const juce::Identifier& key, const juce::var& value)
    {
        if (auto* o = v.getDynamicObject())
            o->setProperty (key, value);
    }

    juce::Colour colourOf (const juce::var& v, juce::Colour fallback)
    {
        auto text = v.toString().trim().removeCharacters ("#");

        if (text.isEmpty())
            return fallback;

        if (text.length() == 6)
            text = "FF" + text;

        return juce::Colour ((juce::uint32) text.getHexValue64());
    }

    /** The box a set's picture is drawn in: wide for a line of keys, square-ish otherwise. */
    juce::Rectangle<float> shapeFor (const juce::var& d, juce::Rectangle<float> room, int keys)
    {
        const auto formation = d.getProperty ("formation", "row").toString();
        const bool vertical = (bool) d.getProperty ("vertical", false);
        float ratio = 1.0f;

        if (formation == "row" || ((formation == "segmented" || formation == "zipper" || formation == "rocker") && ! vertical))
            ratio = juce::jlimit (1.5f, 8.0f, (float) keys * (formation == "zipper" ? 0.6f : 1.1f));
        else if (formation == "column" || vertical)
            ratio = juce::jlimit (0.3f, 1.0f, 2.2f / (float) keys);
        else if (formation == "grid")
        {
            const int columns = juce::jmax (1, (int) d.getProperty ("columns", juce::roundToInt (std::ceil (std::sqrt ((float) keys)))));
            ratio = (float) columns / (float) juce::jmax (1, (keys + columns - 1) / columns);
        }
        else if (formation == "arc")
            ratio = 1.6f;

        auto r = room;

        if (r.getWidth() / r.getHeight() > ratio)
            return r.withSizeKeepingCentre (r.getHeight() * ratio, r.getHeight());

        return r.withSizeKeepingCentre (r.getWidth(), r.getWidth() / ratio);
    }

    struct StudioWindow final : public juce::DocumentWindow
    {
        StudioWindow (juce::Component* content, std::function<void()> closed)
            : juce::DocumentWindow ("Selector Studio", Theme::panel, juce::DocumentWindow::closeButton), onClosed (std::move (closed))
        {
            setUsingNativeTitleBar (true);
            setContentOwned (content, true);
            setResizable (true, false);
        }

        void closeButtonPressed() override
        {
            setVisible (false);

            if (onClosed != nullptr)
                onClosed();
        }

        bool keyPressed (const juce::KeyPress& key) override
        {
            if (key == juce::KeyPress::escapeKey)
            {
                closeButtonPressed();
                return true;
            }

            return juce::DocumentWindow::keyPressed (key);
        }

        std::function<void()> onClosed;
    };
}

//==============================================================================
/** The settings: a caption and its control (or two side by side), in groups. */
class SelectorStudio::Rows final : public juce::Component
{
public:
    struct Row
    {
        juce::String caption;
        juce::Component* first = nullptr;
        juce::Component* second = nullptr;
        bool header = false, shown = true;
        int top = 0;
    };

    std::vector<Row> rows;

    void add (const juce::String& caption, juce::Component& first, juce::Component* second = nullptr)
    {
        rows.push_back ({ caption, &first, second, false, true, 0 });
        addAndMakeVisible (first);

        if (second != nullptr)
            addAndMakeVisible (*second);
    }

    void header (const juce::String& caption)    { rows.push_back ({ caption, nullptr, nullptr, true, true, 0 }); }

    void show (juce::Component& c, bool shouldShow)
    {
        for (auto& row : rows)
            if (row.first == &c)
                row.shown = shouldShow;
    }

    int layOut (int width)
    {
        int y = 4;

        for (size_t i = 0; i < rows.size(); ++i)
        {
            auto& row = rows[i];

            if (row.header)
            {
                bool any = false;

                for (size_t j = i + 1; j < rows.size() && ! rows[j].header; ++j)
                    any = any || rows[j].shown;

                row.shown = any;
                row.top = y;
                y += any ? 26 : 0;
                continue;
            }

            for (auto* c : { row.first, row.second })
                if (c != nullptr)
                    c->setVisible (row.shown);

            if (! row.shown)
                continue;

            auto area = juce::Rectangle<int> (0, y, width, 24).reduced (8, 1);

            if (row.caption.isNotEmpty())
                area.removeFromLeft (118);

            if (row.second != nullptr)
            {
                row.first->setBounds (area.removeFromLeft (area.getWidth() / 2 - 2));
                area.removeFromLeft (4);
                row.second->setBounds (area);
            }
            else
            {
                row.first->setBounds (area);
            }

            row.top = y;
            y += 27;
        }

        return y + 8;
    }

    void paint (juce::Graphics& g) override
    {
        for (const auto& row : rows)
        {
            if (! row.shown)
                continue;

            if (row.header)
            {
                g.setColour (Theme::accent);
                g.setFont (Theme::font (12.0f, true));
                g.drawText (row.caption.toUpperCase(), 8, row.top + 6, getWidth() - 16, 16, juce::Justification::centredLeft, true);
                g.setColour (Theme::outline);
                g.drawHorizontalLine (row.top + 24, 8.0f, (float) getWidth() - 8.0f);
            }
            else if (row.caption.isNotEmpty())
            {
                g.setColour (Theme::muted);
                g.setFont (Theme::font (12.5f));
                g.drawText (row.caption, 8, row.top, 112, 24, juce::Justification::centredLeft, true);
            }
        }
    }
};

//==============================================================================
/** The set as the selector shows it: its keys lit as they behave when they're clicked. */
class SelectorStudio::Preview final : public juce::Component
{
public:
    explicit Preview (SelectorStudio& s) : studio (s) {}

    std::vector<bool> lit;

    void paint (juce::Graphics& g) override
    {
        // On the panel's colours, the selector's own size (fitted).
        const auto& layout = studio.panel();
        g.setGradientFill (juce::ColourGradient (layout.backgroundTop, 0, 0, layout.backgroundBottom, 0, (float) getHeight(), false));
        g.fillAll();

        const auto d = studio.design();
        const auto keys = studio.keys();
        lit.resize ((size_t) keys, false);

        if (std::none_of (lit.begin(), lit.end(), [] (bool b) { return b; }) && studio.mode() == "single" && keys > 0)
            lit[0] = true;

        if (! d.isObject())
        {
            g.setColour (Theme::muted);
            g.setFont (Theme::font (14.0f));
            g.drawFittedText ("A plain selector. Pick a set in the Gallery window (the Gallery button above) to give it keys of their own.",
                              getLocalBounds().reduced (30), juce::Justification::centred, 4);
            return;
        }

        const Render3D::ScopedLater later (*this);   // made in the background: the preview never holds the window up
        SelectorSet::paint (g, d, area(), studio.names(), lit, layout.lightAngle, layout.lightAltitude, studio.dragDraft != nullptr ? 1.0f : 2.0f);

        g.setColour (Theme::muted);
        g.setFont (Theme::font (11.5f));
        g.drawText ("Click its keys to try them", getLocalBounds().reduced (10, 6), juce::Justification::bottomLeft, true);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto d = studio.design();

        if (! d.isObject())
            return;

        const auto a = area();
        const auto key = SelectorSet::keyAt (d, a, studio.keys(), e.position);

        if (! juce::isPositiveAndBelow (key, (int) lit.size()))
            return;

        if (studio.mode() == "single")
            std::fill (lit.begin(), lit.end(), false);

        lit[(size_t) key] = studio.mode() == "single" ? true : ! lit[(size_t) key];
        repaint();
    }

private:
    SelectorStudio& studio;

    /** The selector's own proportions, as big as fits. */
    juce::Rectangle<float> area() const
    {
        juce::Rectangle<float> box (120.0f, 48.0f);

        if (studio.alive())
        {
            const auto& w = studio.panel().widgets[(size_t) studio.widget];
            box = w.bounds.toFloat().withTrimmedBottom ((float) GuiLayout::captionHeight);
        }

        const auto room = getLocalBounds().reduced (30).toFloat();
        const float scale = juce::jmin (room.getWidth() / juce::jmax (1.0f, box.getWidth()), room.getHeight() / juce::jmax (1.0f, box.getHeight()), 3.0f);
        return juce::Rectangle<float> (box.getWidth() * scale, box.getHeight() * scale).withCentre (room.getCentre());
    }
};

//==============================================================================
void SelectorStudio::paintGalleryPicture (juce::Graphics& g, const juce::var& design, int keys, juce::Rectangle<float> area, float lightAngle, float lightAltitude)
{
    static const juce::StringArray sample { "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight",
                                            "9", "10", "11", "12", "13", "14", "15", "16" };
    keys = juce::jlimit (1, 32, keys);
    std::vector<bool> lit ((size_t) keys, false);
    lit[(size_t) (1 % keys)] = true;
    juce::StringArray names;

    for (int k = 0; k < keys; ++k)
        names.add (design.getProperty ("formation", {}).toString() == "rocker" ? juce::String (k == 0 ? "-" : "+") : sample[k]);

    SelectorSet::paint (g, design, shapeFor (design, area.reduced (4.0f), keys), names, lit, lightAngle, lightAltitude, 1.0f);
}

//==============================================================================
namespace
{
    // What each control edits, and how its value is read back.
    struct SliderLink { juce::Identifier key; double fallback; };
    struct ComboLink { juce::Identifier key; juce::StringArray values; juce::String fallback; };
    struct ToggleLink { juce::Identifier key; bool fallback; };
    struct SwatchLink { juce::Identifier key; juce::Colour fallback; };

}

struct SelectorStudio::Links
{
    std::map<const void*, SliderLink> sliders;
    std::map<const void*, ComboLink> combos;
    std::map<const void*, ToggleLink> toggles;
    std::map<const void*, SwatchLink> swatches;
};

//==============================================================================
SelectorStudio::SelectorStudio (PluginCanvas& c, int widgetIndex)
    : canvas (c), widget (widgetIndex), links (std::make_unique<Links>())
{
    behaviourLabel.setText ("Its keys", juce::dontSendNotification);
    behaviourLabel.setColour (juce::Label::textColourId, Theme::muted);
    behaviourLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (behaviourLabel);

    behaviourBox.addItem ("One at a time", 1);
    behaviourBox.addItem ("Each on its own (toggles)", 2);
    behaviourBox.addItem ("Several at once", 3);
    behaviourBox.setTooltip ("One at a time: the parameter is the key's number. Each on its own: every key its own on/off parameter "
                             "(the selector's parameter with the key's number after it). Several at once: one parameter, a key a bit");
    behaviourBox.onChange = [this]
    {
        if (! updating)
            setMode (juce::StringArray { "single", "toggles", "multi" }[behaviourBox.getSelectedItemIndex()]);
    };
    addAndMakeVisible (behaviourBox);

    for (auto* b : { &keepButton, &openButton, &saveButton, &plainButton })
        addAndMakeVisible (*b);

    keepButton.setTooltip ("This set in the Selector gallery, under a name of its own, for every plugin");
    keepButton.onClick = [this] { saveToGallery(); };
    openButton.setTooltip ("A set from a file (.fksel)");
    openButton.onClick = [this] { openFile(); };
    saveButton.setTooltip ("This set as a file (.fksel), to share");
    saveButton.onClick = [this] { saveFile(); };
    plainButton.setTooltip ("Back to a plain selector: its display (buttons in a row, a pill, console keys...) without a set of keys");
    plainButton.onClick = [this]
    {
        if (! alive())
            return;

        canvas.layout.widgets[(size_t) widget].settings.erase ("set");
        changed (true);
        refresh();
    };

    galleryButton.setTooltip ("The Gallery window on this one's page: the templates and the ones kept, to pick from");
    galleryButton.onClick = [this] { GalleryHub::show (canvas, GalleryHub::Page::selectorSets, this); };
    addAndMakeVisible (galleryButton);

    preview = std::make_unique<Preview> (*this);
    addAndMakeVisible (*preview);

    rows = std::make_unique<Rows>();
    setUpRows();
    rowsView.setViewedComponent (rows.get(), false);
    rowsView.setScrollBarsShown (true, false);
    addAndMakeVisible (rowsView);

    // Its own copy of its set (a duplicate, or one made from the toolbox, shares its first one's).
    if (alive())
        if (auto& settings = canvas.layout.widgets[(size_t) widget].settings; settings.count ("set") > 0 && settings["set"].isObject())
            settings["set"] = settings["set"].clone();

    refresh();
    setSize (952, 760);
    startTimer (40);
}

SelectorStudio::~SelectorStudio()
{
    stopTimer();
}

std::unique_ptr<juce::DocumentWindow> SelectorStudio::makeWindow (PluginCanvas& canvas, int widgetIndex)
{
    auto* studio = new SelectorStudio (canvas, widgetIndex);
    auto window = std::make_unique<StudioWindow> (studio, nullptr);
    window->centreAroundComponent (&canvas, studio->getWidth(), studio->getHeight());
    return window;
}

//==============================================================================
const GuiLayout& SelectorStudio::panel() const
{
    return canvas.layout;
}

bool SelectorStudio::alive() const
{
    return juce::isPositiveAndBelow (widget, (int) canvas.layout.widgets.size())
        && canvas.layout.widgets[(size_t) widget].type == GuiWidget::Type::selector;
}

juce::var SelectorStudio::design() const
{
    return alive() ? SelectorSet::designOf (canvas.layout.widgets[(size_t) widget]) : juce::var();
}

juce::var& SelectorStudio::ensureDesign()
{
    auto& settings = canvas.layout.widgets[(size_t) widget].settings;

    if (! settings["set"].isObject())
        settings["set"] = SelectorSet::makeTemplate (2);

    return settings["set"];
}

int SelectorStudio::keys() const
{
    return alive() ? canvas.selectorKeys (canvas.layout.widgets[(size_t) widget]) : 4;
}

juce::StringArray SelectorStudio::names() const
{
    juce::StringArray list;

    if (alive())
        list = canvas.layout.widgets[(size_t) widget].options;

    while (list.size() < keys())
        list.add (juce::String (list.size() + 1));

    return list;
}

juce::String SelectorStudio::mode() const
{
    return alive() ? SelectorSet::modeOf (canvas.layout.widgets[(size_t) widget]) : juce::String ("single");
}

void SelectorStudio::setMode (const juce::String& newMode)
{
    if (! alive())
        return;

    canvas.layout.widgets[(size_t) widget].settings["mode"] = newMode;
    preview->lit.clear();
    changed (true);
}

void SelectorStudio::set (const juce::Identifier& key, const juce::var& value, bool final)
{
    if (updating || ! alive())
        return;

    put (ensureDesign(), key, value);
    changed (final);
}

void SelectorStudio::changed (bool final)
{
    if (! alive())
        return;

    // Dragged: drawn quickly (the panel too) until it's let go, then sharp.
    if (! final && dragDraft == nullptr)
        dragDraft = std::make_unique<Render3D::ScopedDraft>();
    else if (final)
        dragDraft.reset();

    lastSeen = juce::JSON::toString (design(), true) + SelectorSet::modeOf (canvas.layout.widgets[(size_t) widget])
             + canvas.layout.widgets[(size_t) widget].options.joinIntoString (",");
    preview->repaint();
    showRows();

    if (final)
        canvas.edited();
    else
        canvas.repaint();
}

void SelectorStudio::useDesign (const juce::var& newDesign)
{
    if (! alive())
        return;

    canvas.layout.widgets[(size_t) widget].settings["set"] = newDesign.clone();
    preview->lit.clear();
    changed (true);
    refresh();
}

void SelectorStudio::timerCallback()
{
    // A drag that ended without saying so: sharp again.
    if (dragDraft != nullptr && ! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        changed (true);


    // Changed elsewhere (an undo, Fanan AI, its menu): shown as it is now.
    if (alive())
    {
        const auto now = juce::JSON::toString (design(), true) + SelectorSet::modeOf (canvas.layout.widgets[(size_t) widget])
                       + canvas.layout.widgets[(size_t) widget].options.joinIntoString (",");

        if (now != lastSeen)
            refresh();
    }
}


void SelectorStudio::setUpSlider (juce::Slider& s, double from, double to, double step, const char* key, const juce::String& tip, const juce::String& suffix)
{
    s.setSliderStyle (juce::Slider::LinearBar);
    s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 70, 22);
    s.setColour (juce::Slider::trackColourId, Theme::accentDeep);
    s.setColour (juce::Slider::backgroundColourId, Theme::inset);
    s.setColour (juce::Slider::textBoxTextColourId, Theme::text);
    s.setColour (juce::Slider::textBoxOutlineColourId, Theme::outline);
    s.setRange (from, to, step);
    s.setTextValueSuffix (suffix);
    s.setTooltip (tip);
    s.onValueChange = [this, &s, key] { set (key, s.getValue(), ! s.isMouseButtonDown()); };
    s.onDragEnd = [this] { if (! updating) changed (true); };
    links->sliders[&s] = { key, from };
}

void SelectorStudio::setUpCombo (juce::ComboBox& box, const juce::StringArray& captions, const juce::StringArray& values, const char* key, const juce::String& tip)
{
    box.clear (juce::dontSendNotification);

    for (int i = 0; i < captions.size(); ++i)
        box.addItem (captions[i], i + 1);

    box.setTooltip (tip);
    box.onChange = [this, &box, values, key]
    {
        if (juce::isPositiveAndBelow (box.getSelectedItemIndex(), values.size()))
            set (key, values[box.getSelectedItemIndex()]);
    };
    links->combos[&box] = { key, values, values[0] };
}

void SelectorStudio::setUpToggle (juce::ToggleButton& t, const char* key, const juce::String& tip)
{
    t.setTooltip (tip);
    t.onClick = [this, &t, key] { set (key, t.getToggleState()); };
    links->toggles[&t] = { key, false };
}

void SelectorStudio::setUpSwatch (ColourSwatchButton& s, const char* key, const juce::String& tip)
{
    s.setTooltip (tip);
    s.onColourChange = [this, &s, key] { set (key, "#" + s.getColourValue().toDisplayString (true)); };
    links->swatches[&s] = { key, juce::Colours::grey };
}

void SelectorStudio::setUpRows()
{
    auto& r = *rows;

    namesBox.setTooltip ("Its positions' names, comma separated: as many keys as names");
    namesBox.onReturnKey = namesBox.onFocusLost = [this]
    {
        if (updating || ! alive())
            return;

        juce::StringArray list;
        list.addTokens (namesBox.getText(), ",", "\"");
        list.trim();
        list.removeEmptyStrings();

        if (list != canvas.layout.widgets[(size_t) widget].options && ! list.isEmpty())
        {
            canvas.layout.widgets[(size_t) widget].options = list;
            preview->lit.clear();
            changed (true);
        }
    };

    r.header ("Keys");
    r.add ("Names", namesBox);

    setUpCombo (renderBox, { "3D keys", "Flat (2D)" }, { "3d", "2d" }, "render", "Drawn as 3D keys, lit by the panel's light, or flat");
    setUpCombo (styleBox, { "Soft", "Flat", "Glossy" }, { "soft", "flat", "glossy" }, "style", "Flat keys: soft (a gradient and a bevel), flat, or glossy");
    setUpCombo (formationBox, { "Row", "Column", "Zipper (triangles)", "Grid", "Ring", "Arc", "Joined strip", "D-pad (cross)", "Rocker" },
                { "row", "column", "zipper", "grid", "ring", "arc", "segmented", "cross", "rocker" }, "formation",
                "How the keys are set out: in a row or a column, triangles zipped tip up and tip down, a grid, round a ring or an arc, "
                "a joined strip, a D-pad (4 keys, or 5 with the middle), a rocker (2 keys)");
    setUpCombo (shapeBox, { "Rounded", "Square", "Circle", "Pill", "Triangle", "Curved (ring, arc)", "Lamp and name" },
                { "round", "rect", "circle", "pill", "triangle", "segment", "led" }, "shape", "The keys' shape");
    setUpToggle (verticalToggle, "vertical", "A zipper, a joined strip or a rocker up and down");
    setUpSlider (columnsSlider, 1, 16, 1, "columns", "A grid: how many across");
    setUpSlider (gapSlider, 0, 100, 1, "gap", "Between the keys", " %");
    setUpSlider (roundingSlider, 0, 100, 1, "rounding", "Their corners", " %");
    setUpSlider (arcFromSlider, -360, 720, 1, "arcFrom", "Where the keys start round the ring: 0 at the top, clockwise",
                 juce::String::fromUTF8 ("\xc2\xb0"));
    setUpSlider (arcToSlider, -360, 720, 1, "arcTo", "Where they end", juce::String::fromUTF8 ("\xc2\xb0"));
    setUpSlider (innerSlider, 5, 95, 1, "inner", "The keys' inner edge, of the ring's outer", " %");

    r.header ("Set out");
    r.add ("Drawn", renderBox);
    r.add ("Style", styleBox);
    r.add ("Formation", formationBox);
    r.add ("Shape", shapeBox);
    r.add ({}, verticalToggle);
    r.add ("Columns", columnsSlider);
    r.add ("Gap, corners", gapSlider, &roundingSlider);
    r.add ("From, to", arcFromSlider, &arcToSlider);
    r.add ("Inner edge", innerSlider);

    // Its text.
    setUpCombo (labelsBox, { "In the control", "Out of the control", "None" }, { "caps", "beside", "none" }, "labels",
                "The names in the keys, out of them (under or above a row, beside a column, round a ring...), or none");
    setUpCombo (labelSideBox, { "Where it fits best", "Above", "Below", "Left", "Right", "Inside the ring" },
                { "auto", "above", "below", "left", "right", "inside" }, "labelSide",
                "Out of the keys: which side. Where it fits best is under a row, a grid or a strip, beside a column, outside a ring "
                "or an arc, past a D-pad's arms");
    setUpCombo (labelAngleBox, { "Stays horizontal", "Follows the shape" }, { "horizontal", "follow" }, "labelAngle",
                "Stays horizontal: read straight whatever the key's shape. Follows the shape: up a tall key, along a curved key, "
                "round a ring's edge, up a column's edge");
    setUpSlider (textSizeSlider, 5, 100, 1, "textSize", "How big the names are", " %");
    setUpSlider (spacingSlider, -20, 100, 1, "spacing", "Between the letters", " %");

    fontBox.addItem ("Standard", 1);

    for (int i = 0; i < (int) FontLibrary::families().size(); ++i)
        fontBox.addItem (FontLibrary::families()[(size_t) i].name, i + 2);

    fontBox.setTooltip ("Their font: one of the panel's fonts");
    fontBox.onChange = [this] { set ("font", fontBox.getSelectedId() <= 1 ? juce::String() : fontBox.getText()); };
    setUpToggle (boldToggle, "bold", "Bold names");
    links->toggles[&boldToggle].fallback = true;
    setUpToggle (italicToggle, "italic", "Leaning names");
    setUpToggle (upperToggle, "upper", "In capitals");
    setUpToggle (labelGlowToggle, "labelGlow", "A lit key's name glows in its light");
    links->toggles[&labelGlowToggle].fallback = true;
    setUpSwatch (labelSwatch, "labelColour", "The names' colour");
    setUpSwatch (labelOnSwatch, "labelOnColour", "A name's colour when its key is on");
    textEffectsButton.setTooltip ("Photoshop's layer style on the names: a stroke, a bevel, an inner or a drop shadow, a colour overlay...");
    textEffectsButton.onClick = [this] { editTextEffects(); };

    r.header ("Text");
    r.add ("Placement", labelsBox);
    r.add ("Side", labelSideBox);
    r.add ("Direction", labelAngleBox);
    r.add ("Font", fontBox, &boldToggle);
    r.add ({}, italicToggle, &upperToggle);
    r.add ("Size, spacing", textSizeSlider, &spacingSlider);
    r.add ("Off, on", labelSwatch, &labelOnSwatch);
    r.add ({}, labelGlowToggle);
    r.add ({}, textEffectsButton);

    setUpSwatch (capSwatch, "capColour", "The keys' colour");
    setUpSwatch (capOnSwatch, "capOnColour", "A key's colour when it's on");
    setUpSwatch (glowSwatch, "glowColour", "The light of a key that's on (its glow, its rim)");
    setUpSlider (glowSlider, 0, 100, 1, "glow", "How bright it is", " %");
    setUpToggle (edgeToggle, "edge", "Its rim lights when it's on (dimly when it's off)");
    setUpSlider (edgeOffSlider, 0, 100, 1, "edgeOff", "Its rim when it's off", " %");
    setUpToggle (capGlowToggle, "capGlow", "The key itself glows when it's on");
    setUpCombo (ledBox, { "No lamp", "A lamp dot", "A lamp bar" }, { "none", "dot", "bar" }, "led", "A lamp on each key, lit when it's on");
    setUpSwatch (ledSwatch, "ledColour", "The lamps' colour");
    setUpToggle (plateToggle, "plate", "The keys sit in a plate");
    setUpSwatch (plateSwatch, "plateColour", "The plate's colour");

    r.header ("Colours and light");
    r.add ("Key off, on", capSwatch, &capOnSwatch);
    r.add ("Light", glowSwatch, &glowSlider);
    r.add ({}, edgeToggle, &capGlowToggle);
    r.add ("Rim when off", edgeOffSlider);
    r.add ("Lamp", ledBox, &ledSwatch);
    r.add ({}, plateToggle, &plateSwatch);

    juce::StringArray materials;

    for (const auto& m : Model3D::materialNames())
        materials.add (m);

    setUpCombo (materialBox, materials, materials, "material", "What the keys are made of");
    setUpSlider (heightSlider, 3, 80, 1, "height", "How tall the keys are, of their size", " %");
    setUpSlider (travelSlider, 0, 90, 1, "travel", "How far a key goes in when it's on, of its height", " %");

    r.header ("3D");
    r.add ("Material", materialBox);
    r.add ("Height", heightSlider);
    r.add ("Travel", travelSlider);
}

void SelectorStudio::refresh()
{
    const juce::ScopedValueSetter<bool> busy (updating, true);
    const auto d = design();
    const bool rings = d.getProperty ("formation", {}).toString() == "ring" || d.getProperty ("formation", {}).toString() == "arc";

    // The set's own defaults, as SelectorSet reads them.
    const std::map<juce::String, double> numbers { { "columns", std::ceil (std::sqrt ((double) keys())) }, { "gap", 8 }, { "rounding", 30 },
                                                   { "arcFrom", d.getProperty ("formation", {}).toString() == "ring" ? 0 : 100 },
                                                   { "arcTo", d.getProperty ("formation", {}).toString() == "ring" ? 360 : 260 }, { "inner", 60 },
                                                   { "textSize", 34 }, { "spacing", 0 }, { "glow", 60 }, { "edgeOff", 25 }, { "height", 18 }, { "travel", 30 } };
    const std::map<juce::String, juce::Colour> colours { { "capColour", juce::Colour (0xff26282d) }, { "capOnColour", juce::Colour (0xff26282d) },
                                                         { "labelColour", juce::Colour (0xffd8dadf) }, { "labelOnColour", juce::Colours::white },
                                                         { "glowColour", juce::Colour (0xff5fe0e8) }, { "ledColour", juce::Colour (0xffff3b30) },
                                                         { "plateColour", juce::Colour (0xff141518) } };

    for (auto& [control, link] : links->sliders)
    {
        auto* s = static_cast<juce::Slider*> (const_cast<void*> (control));
        const auto value = d.getProperty (link.key, {});
        const auto found = numbers.find (link.key.toString());
        s->setValue (value.isVoid() ? (found != numbers.end() ? found->second : link.fallback) : (double) value, juce::dontSendNotification);
    }

    for (auto& [control, link] : links->combos)
    {
        auto* box = static_cast<juce::ComboBox*> (const_cast<void*> (control));
        auto value = d.getProperty (link.key, {}).toString().trim();

        if (value.isEmpty())
            value = link.key == juce::Identifier ("shape") ? (rings ? "segment" : "round") : link.key == juce::Identifier ("material") ? "Satin plastic" : link.fallback;

        int index = 0;

        for (int i = 0; i < link.values.size(); ++i)
            if (link.values[i].equalsIgnoreCase (value))
                index = i;

        box->setSelectedItemIndex (index, juce::dontSendNotification);
    }

    for (auto& [control, link] : links->toggles)
        static_cast<juce::ToggleButton*> (const_cast<void*> (control))->setToggleState ((bool) d.getProperty (link.key, link.fallback), juce::dontSendNotification);

    for (auto& [control, link] : links->swatches)
    {
        const auto found = colours.find (link.key.toString());
        static_cast<ColourSwatchButton*> (const_cast<void*> (control))->setColourValue (colourOf (d.getProperty (link.key, {}), found != colours.end() ? found->second : link.fallback),
                                                                                         juce::dontSendNotification);
    }

    fontBox.setSelectedId (1, juce::dontSendNotification);

    for (int i = 0; i < fontBox.getNumItems(); ++i)
        if (fontBox.getItemText (i).equalsIgnoreCase (d.getProperty ("font", {}).toString()))
            fontBox.setSelectedItemIndex (i, juce::dontSendNotification);

    behaviourBox.setSelectedItemIndex (juce::jmax (0, juce::StringArray { "single", "toggles", "multi" }.indexOf (mode())), juce::dontSendNotification);
    namesBox.setText (alive() ? canvas.layout.widgets[(size_t) widget].options.joinIntoString (", ") : juce::String(), false);

    if (alive())
        lastSeen = juce::JSON::toString (design(), true) + SelectorSet::modeOf (canvas.layout.widgets[(size_t) widget])
                 + canvas.layout.widgets[(size_t) widget].options.joinIntoString (",");

    showRows();
    preview->repaint();
}

void SelectorStudio::showRows()
{
    const auto d = design();
    const auto formation = d.getProperty ("formation", "row").toString();
    const bool threeD = d.getProperty ("render", "3d").toString() != "2d";
    const bool any = d.isObject();
    auto& r = *rows;

    for (auto& row : r.rows)
        if (! row.header)
            row.shown = any || row.first == &namesBox;

    if (any)
    {
        r.show (styleBox, ! threeD);
        r.show (verticalToggle, formation == "zipper" || formation == "segmented" || formation == "rocker");
        r.show (columnsSlider, formation == "grid");
        r.show (arcFromSlider, formation == "ring" || formation == "arc");
        r.show (innerSlider, formation == "ring" || formation == "arc");
        r.show (shapeBox, formation != "zipper" && formation != "segmented" && formation != "cross" && formation != "rocker");
        r.show (edgeOffSlider, (bool) d.getProperty ("edge", false));

        const auto labels = d.getProperty ("labels", "caps").toString();
        r.show (labelSideBox, labels == "beside");

        for (auto* c : std::initializer_list<juce::Component*> { &labelAngleBox, &fontBox, &italicToggle, &textSizeSlider, &labelSwatch,
                                                                  &labelGlowToggle, &textEffectsButton })
            r.show (*c, labels != "none");
        r.show (materialBox, threeD);
        r.show (heightSlider, threeD);
        r.show (travelSlider, threeD);
    }

    for (auto* c : std::initializer_list<juce::Component*> { &keepButton, &saveButton, &plainButton })
        c->setEnabled (any);

    rows->setSize (rowsView.getMaximumVisibleWidth(), rows->layOut (rowsView.getMaximumVisibleWidth()));
    rows->repaint();
}

//==============================================================================
void SelectorStudio::editTextEffects()
{
    if (! alive() || ! design().isObject())
        return;

    const auto before = design().getProperty ("textEffects", {}).clone();
    const juce::Point<float> lightBefore (canvas.layout.lightAngle, canvas.layout.lightAltitude);
    const juce::Component::SafePointer<SelectorStudio> safe (this);

    LayerStyleWindow::Host host;
    host.alive = [safe] { return safe != nullptr && safe->alive() && safe->design().isObject(); };

    host.apply = [safe] (const LayerStyle& style, bool final)
    {
        if (safe == nullptr || ! safe->alive())
            return;

        const auto effects = style.toVar();
        auto& d = safe->ensureDesign();

        if (const auto* o = effects.getDynamicObject(); o != nullptr && ! o->getProperties().isEmpty())
            put (d, "textEffects", effects);
        else if (auto* object = d.getDynamicObject())
            object->removeProperty ("textEffects");

        safe->changed (final);
    };

    host.cancel = [safe, before, lightBefore]
    {
        if (safe == nullptr || ! safe->alive())
            return;

        auto& d = safe->ensureDesign();

        if (before.isObject())
            put (d, "textEffects", before);
        else if (auto* object = d.getDynamicObject())
            object->removeProperty ("textEffects");

        safe->canvas.setGlobalLight (lightBefore.x, lightBefore.y);
        safe->changed (true);
    };

    host.sample = [safe] (const LayerStyle& style)
    {
        // A name, big, wearing the style.
        juce::Image picture (juce::Image::ARGB, 360, 120, true);

        if (safe == nullptr || ! safe->alive())
            return picture;

        const auto d = safe->design();
        auto font = FontLibrary::font (d.getProperty ("font", {}).toString(), 64.0f, (bool) d.getProperty ("bold", true));
        auto text = safe->names()[juce::jmax (0, juce::jmin (1, safe->names().size() - 1))].substring (0, 14);

        if ((bool) d.getProperty ("upper", false))
            text = text.toUpperCase();

        juce::GlyphArrangement glyphs;
        glyphs.addFittedText (font, text, 10.0f, 10.0f, 340.0f, 100.0f, juce::Justification::centred, 1, 0.6f);
        juce::Path words;
        glyphs.createPath (words);

        {
            juce::Graphics g (picture);
            g.setColour (colourOf (d.getProperty ("labelOnColour", {}), juce::Colours::white));
            g.fillPath (words);
        }

        LayerRender::applyLayerStyle (picture, style, 1.0f, safe->panel().lightAngle, safe->panel().lightAltitude);
        return picture;
    };

    host.globalLight = [safe]
    {
        return safe != nullptr ? juce::Point<float> (safe->panel().lightAngle, safe->panel().lightAltitude) : juce::Point<float> (120.0f, 30.0f);
    };

    host.setGlobalLight = [safe] (float angle, float altitude)
    {
        if (safe != nullptr)
            safe->canvas.setGlobalLight (angle, altitude);
    };

    LayerStyleWindow::launch (LayerStyle::fromVar (before), std::move (host), "Text effects", this);
}

void SelectorStudio::saveToGallery()
{
    if (! design().isObject())
        return;

    auto* ask = new juce::AlertWindow ("Keep in the Selector gallery", "Its name in the gallery:", juce::MessageBoxIconType::NoIcon, this);
    ask->addTextEditor ("name", "My keys");
    ask->addButton ("Keep", 1, juce::KeyPress (juce::KeyPress::returnKey));
    ask->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    auto file = object();
    put (file, "keys", keys());
    put (file, "set", design().clone());

    ask->enterModalState (true, juce::ModalCallbackFunction::create ([ask, file, safe = juce::Component::SafePointer<SelectorStudio> (this)] (int result)
    {
        const auto name = ask->getTextEditorContents ("name").trim();

        if (result != 1 || name.isEmpty())
            return;

        put (file, "name", name);
        const auto target = SelectorSet::galleryFolder().getChildFile (juce::File::createLegalFileName (name) + SelectorSet::fileExtension);

        if (! target.replaceWithText (juce::JSON::toString (file)))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Keep in the Selector gallery",
                                                    "It couldn't be written to " + target.getFullPathName());

        juce::ignoreUnused (safe);
    }), true);
}

void SelectorStudio::openFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Open a set of keys", SelectorSet::galleryFolder(), juce::String ("*") + SelectorSet::fileExtension);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<SelectorStudio> (this)] (const juce::FileChooser& fc)
                          {
                              if (safe == nullptr || fc.getResult() == juce::File())
                                  return;

                              const auto json = juce::JSON::parse (fc.getResult());

                              if (json.getProperty ("set", {}).isObject())
                                  safe->useDesign (json.getProperty ("set", {}));
                              else
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Open a set of keys",
                                                                          fc.getResult().getFileName() + " isn't a set of keys.");
                          });
}

void SelectorStudio::saveFile()
{
    if (! design().isObject())
        return;

    auto file = object();
    put (file, "name", "Keys");
    put (file, "keys", keys());
    put (file, "set", design().clone());
    const auto text = juce::JSON::toString (file);

    chooser = std::make_unique<juce::FileChooser> ("Save the set of keys", SelectorSet::galleryFolder().getChildFile (juce::String ("Keys") + SelectorSet::fileExtension),
                                                   juce::String ("*") + SelectorSet::fileExtension);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [text] (const juce::FileChooser& fc)
                          {
                              if (fc.getResult() != juce::File())
                                  fc.getResult().withFileExtension (SelectorSet::fileExtension).replaceWithText (text);
                          });
}

//==============================================================================
void SelectorStudio::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::outline);
    g.drawHorizontalLine (44, 0.0f, (float) getWidth());
}

void SelectorStudio::resized()
{
    auto area = getLocalBounds();
    auto top = area.removeFromTop (44).reduced (8, 8);

    behaviourLabel.setBounds (top.removeFromLeft (64));
    top.removeFromLeft (6);
    behaviourBox.setBounds (top.removeFromLeft (220));
    top.removeFromLeft (16);
    keepButton.setBounds (top.removeFromLeft (128));
    top.removeFromLeft (4);
    galleryButton.setBounds (top.removeFromLeft (84));
    top.removeFromLeft (4);
    openButton.setBounds (top.removeFromLeft (70));
    top.removeFromLeft (4);
    saveButton.setBounds (top.removeFromLeft (84));
    plainButton.setBounds (top.removeFromRight (120));

    area.reduce (8, 8);
    rowsView.setBounds (area.removeFromRight (400));
    rows->setSize (rowsView.getMaximumVisibleWidth(), rows->layOut (rowsView.getMaximumVisibleWidth()));
    area.removeFromRight (8);
    preview->setBounds (area);
}
