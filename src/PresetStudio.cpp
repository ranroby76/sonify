// D:\workspace\Fanan_web_hub\src\PresetStudio.cpp

#include "PresetStudio.h"
#include "GalleryHub.h"
#include "Render3D.h"

#include "FontLibrary.h"
#include "Model3D.h"
#include "PluginCanvas.h"
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

    /** What a preset manager shows, fitted into an area: the bar with its list or table open
        under it, a table, a side panel, the bar. size: the element's own (a side panel's: its
        width and the window's height). */
    void paintScene (juce::Graphics& g, const juce::var& d, juce::Point<float> size, const PresetPanel::View& view,
                     float lightAngle, float lightAltitude, juce::Rectangle<float> into, float most)
    {
        const auto style = PresetPanel::styleOf (d);
        const auto element = juce::Rectangle<float> (size.x, size.y);
        juce::Rectangle<float> popup, scene = element;

        if (PresetPanel::opensPopup (d))
        {
            popup = PresetPanel::popupArea (d, element, { -4000.0f, -4000.0f, 8000.0f, 8000.0f }, view);
            scene = scene.getUnion (popup);
        }

        const float scale = juce::jmin (into.getWidth() / juce::jmax (1.0f, scene.getWidth()), into.getHeight() / juce::jmax (1.0f, scene.getHeight()), most);
        const auto placed = juce::Rectangle<float> (scene.getWidth() * scale, scene.getHeight() * scale).withCentre (into.getCentre());

        juce::Graphics::ScopedSaveState state (g);
        g.addTransform (juce::AffineTransform::translation (-scene.getX(), -scene.getY()).scaled (scale).translated (placed.getX(), placed.getY()));
        const float resolution = juce::jlimit (1.0f, 3.0f, scale);

        if (style == "side" || (style == "table" && ! PresetPanel::opensPopup (d)))
        {
            PresetPanel::paintBody (g, d, element, view, lightAngle, lightAltitude, resolution);
            return;
        }

        PresetPanel::paintBar (g, d, element, view, lightAngle, lightAltitude, resolution);

        if (! popup.isEmpty())
            PresetPanel::paintBody (g, d, popup, view, lightAngle, lightAltitude, resolution);
    }

    juce::Point<float> sizeFor (const juce::var& d, juce::Rectangle<int> bounds, int windowHeight)
    {
        if (PresetPanel::isSide (d))
            return { (float) PresetPanel::sideWidth (d), (float) windowHeight };

        return { (float) bounds.getWidth(), (float) bounds.getHeight() };
    }

    struct StudioWindow final : public juce::DocumentWindow
    {
        explicit StudioWindow (juce::Component* content)
            : juce::DocumentWindow ("Preset Studio", Theme::panel, juce::DocumentWindow::closeButton)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (content, true);
            setResizable (true, false);
        }

        void closeButtonPressed() override    { setVisible (false); }

        bool keyPressed (const juce::KeyPress& key) override
        {
            if (key == juce::KeyPress::escapeKey)
            {
                closeButtonPressed();
                return true;
            }

            return juce::DocumentWindow::keyPressed (key);
        }
    };

    struct SliderLink { juce::String key; double fallback; };
    struct ComboLink { juce::String key; juce::StringArray values; juce::String fallback; };
    struct ToggleLink { juce::String key; bool fallback; };
    struct SwatchLink { juce::String key; juce::Colour fallback; };
}

struct PresetStudio::Links
{
    std::map<juce::Slider*, SliderLink> sliders;
    std::map<juce::ComboBox*, ComboLink> combos;
    std::map<juce::ToggleButton*, ToggleLink> toggles;
    std::map<ColourSwatchButton*, SwatchLink> swatches;
};

//==============================================================================
class PresetStudio::Rows final : public juce::Component
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
/** The preset manager as the window shows it: a list or a popup table open under its bar. */
class PresetStudio::Preview final : public juce::Component
{
public:
    explicit Preview (PresetStudio& s) : studio (s) {}

    void paint (juce::Graphics& g) override
    {
        const Render3D::ScopedLater later (*this);   // 3D keys made in the background: the preview never holds the window up
        const auto& layout = studio.panel();
        g.setGradientFill (juce::ColourGradient (layout.backgroundTop, 0, 0, layout.backgroundBottom, 0, (float) getHeight(), false));
        g.fillAll();

        const auto* w = studio.element();

        if (w == nullptr)
            return;

        const auto d = studio.design();
        paintScene (g, d, sizeFor (d, w->bounds, layout.height), studio.view(), layout.lightAngle, layout.lightAltitude,
                    getLocalBounds().reduced (24).withTrimmedBottom (14).toFloat(), studio.dragDraft != nullptr ? 1.0f : 2.0f);

        g.setColour (Theme::muted);
        g.setFont (Theme::font (11.5f));
        g.drawText ("It works in Play: the arrows step, the name opens its list or table, a side panel's handle opens and closes it",
                    getLocalBounds().reduced (10, 6), juce::Justification::bottomLeft, true);
    }

private:
    PresetStudio& studio;
};

//==============================================================================
void PresetStudio::paintGalleryPicture (juce::Graphics& g, const juce::var& design, juce::Rectangle<float> area, float lightAngle, float lightAltitude)
{
    // At a typical size: a bar 240 wide, a table 360 x 220, a side panel 300 high.
    const auto style = PresetPanel::styleOf (design);
    const bool table = style == "table" && ! PresetPanel::opensPopup (design);
    const auto size = PresetPanel::isSide (design) ? juce::Point<float> ((float) PresetPanel::sideWidth (design), 300.0f)
                                                    : table ? juce::Point<float> (360.0f, 220.0f) : juce::Point<float> (240.0f, 30.0f);
    paintScene (g, design, size, PresetPanel::sampleView(), lightAngle, lightAltitude, area, 1.0f);
}

//==============================================================================
PresetStudio::PresetStudio (PluginCanvas& c, int widgetIndex)
    : canvas (c), widget (widgetIndex), links (std::make_unique<Links>())
{
    keepButton.setTooltip ("This preset manager's design in the Preset gallery, under a name of its own, for every plugin");
    keepButton.onClick = [this] { saveToGallery(); };
    addAndMakeVisible (keepButton);

    galleryButton.setTooltip ("The Gallery window on this one's page: the templates and the ones kept, to pick from");
    galleryButton.onClick = [this] { GalleryHub::show (canvas, GalleryHub::Page::presets, this); };
    addAndMakeVisible (galleryButton);

    preview = std::make_unique<Preview> (*this);
    addAndMakeVisible (*preview);

    rows = std::make_unique<Rows>();
    setUpRows();
    rowsView.setViewedComponent (rows.get(), false);
    rowsView.setScrollBarsShown (true, false);
    addAndMakeVisible (rowsView);

    // Its own copy of its design (a duplicate, or one made from the toolbox, shares its first one's).
    if (auto* w = element(); w != nullptr && w->settings.count ("presets") > 0 && w->settings["presets"].isObject())
        w->settings["presets"] = w->settings["presets"].clone();

    refresh();
    setSize (932, 760);
    startTimer (60);
}

PresetStudio::~PresetStudio()
{
    stopTimer();
}

std::unique_ptr<juce::DocumentWindow> PresetStudio::makeWindow (PluginCanvas& canvas, int widgetIndex)
{
    auto* studio = new PresetStudio (canvas, widgetIndex);
    auto window = std::make_unique<StudioWindow> (studio);
    window->centreAroundComponent (&canvas, studio->getWidth(), studio->getHeight());
    return window;
}

//==============================================================================
const GuiLayout& PresetStudio::panel() const    { return canvas.layout; }

GuiWidget* PresetStudio::element() const
{
    return juce::isPositiveAndBelow (widget, (int) canvas.layout.widgets.size()) && canvas.layout.widgets[(size_t) widget].type == GuiWidget::Type::preset
               ? &canvas.layout.widgets[(size_t) widget] : nullptr;
}

juce::var PresetStudio::design() const
{
    const auto* w = element();

    if (w == nullptr)
        return {};

    // None yet: it shows as a drop list would.
    return PresetPanel::has (*w) ? PresetPanel::designOf (*w) : PresetPanel::makeTemplate (0);
}

juce::var& PresetStudio::ensureDesign()
{
    auto& settings = element()->settings;

    if (! settings["presets"].isObject())
        settings["presets"] = PresetPanel::makeTemplate (0);

    return settings["presets"];
}

juce::String PresetStudio::stateKey() const
{
    const auto* w = element();
    return w != nullptr ? juce::JSON::toString (design(), true) + "|" + w->bounds.toString() : juce::String();
}

PresetPanel::View PresetStudio::view() const
{
    return canvas.presetView (widget);
}

juce::var PresetStudio::valueOf (const juce::String& key) const
{
    const auto d = design();

    if (key.startsWith ("keys."))
        return d.getProperty ("keys", {}).getProperty (key.fromFirstOccurrenceOf (".", false, false), {});

    return d.getProperty (juce::Identifier (key), {});
}

void PresetStudio::set (const juce::String& key, const juce::var& value, bool final)
{
    if (updating || element() == nullptr)
        return;

    auto& d = ensureDesign();

    if (key.startsWith ("keys."))
    {
        if (! d.getProperty ("keys", {}).isObject())
            put (d, "keys", object());

        put (d.getProperty ("keys", {}), juce::Identifier (key.fromFirstOccurrenceOf (".", false, false)), value);
    }
    else
    {
        put (d, juce::Identifier (key), value);
    }

    if (key == "style" || key == "open")
        fitBounds();

    changed (final);
}

void PresetStudio::fitBounds()
{
    auto* w = element();

    if (w == nullptr)
        return;

    // A side panel is placed where it joins the window; anything else comes back onto the
    // panel, as big as its style needs.
    const auto d = design();
    const auto style = PresetPanel::styleOf (d);
    auto& b = w->bounds;

    if (PresetPanel::isSide (d))
        return;

    const bool outside = b.getX() < 0 || b.getX() >= canvas.layout.width;
    const bool table = style == "table" && ! PresetPanel::opensPopup (d);

    if (outside)
        b.setPosition (20, 20);

    if (table && b.getHeight() < 100)
        b.setSize (juce::jmax (b.getWidth(), 360), 220);
    else if (! table && (b.getHeight() > 60 || outside))
        b.setSize (juce::jlimit (160, 320, b.getWidth()), 30);

    b.setX (juce::jlimit (0, juce::jmax (0, canvas.layout.width - b.getWidth()), b.getX()));
    b.setY (juce::jlimit (0, juce::jmax (0, canvas.layout.height - b.getHeight()), b.getY()));
}

void PresetStudio::changed (bool final)
{
    if (element() == nullptr)
        return;

    // Dragged: drawn quickly (the panel too) until it's let go, then sharp.
    if (! final && dragDraft == nullptr)
        dragDraft = std::make_unique<Render3D::ScopedDraft>();
    else if (final)
        dragDraft.reset();

    lastSeen = stateKey();
    preview->repaint();
    showRows();

    if (final)
        canvas.edited();
    else
        canvas.repaint();
}

void PresetStudio::useDesign (const juce::var& newDesign)
{
    if (auto* w = element())
    {
        w->settings["presets"] = newDesign.clone();
        fitBounds();
        changed (true);
        refresh();
    }
}

void PresetStudio::timerCallback()
{
    // A drag that ended without saying so: sharp again.
    if (dragDraft != nullptr && ! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        changed (true);


    if (element() != nullptr && stateKey() != lastSeen)
        refresh();
}

//==============================================================================
void PresetStudio::setUpSlider (juce::Slider& s, double from, double to, double step, double fallback, const juce::String& key, const juce::String& tip, const juce::String& suffix)
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
    links->sliders[&s] = { key, fallback };
}

void PresetStudio::setUpCombo (juce::ComboBox& box, const juce::StringArray& captions, const juce::StringArray& values, const juce::String& fallback,
                               const juce::String& key, const juce::String& tip)
{
    for (int i = 0; i < captions.size(); ++i)
        box.addItem (captions[i], i + 1);

    box.setTooltip (tip);
    box.onChange = [this, &box, values, key]
    {
        if (juce::isPositiveAndBelow (box.getSelectedItemIndex(), values.size()))
            set (key, values[box.getSelectedItemIndex()]);
    };
    links->combos[&box] = { key, values, fallback };
}

void PresetStudio::setUpToggle (juce::ToggleButton& t, bool fallback, const juce::String& key, const juce::String& tip)
{
    t.setTooltip (tip);
    t.onClick = [this, &t, key] { set (key, t.getToggleState()); };
    links->toggles[&t] = { key, fallback };
}

void PresetStudio::setUpSwatch (ColourSwatchButton& s, juce::Colour fallback, const juce::String& key, const juce::String& tip)
{
    s.setTooltip (tip);
    s.onColourChange = [this, &s, key] { set (key, "#" + s.getColourValue().toDisplayString (true)); };
    links->swatches[&s] = { key, fallback };
}

void PresetStudio::setUpRows()
{
    auto& r = *rows;

    setUpCombo (styleBox, { "The classic bar", "Drop list", "Table of buttons", "Side panel" }, { "bar", "list", "table", "side" }, "list", "style",
                "The classic bar (< name >, a menu), a drop list under its bar, a table of preset buttons, or a side panel joined to the "
                "window's left or right (the window wider by it)");
    setUpCombo (openBox, { "On the panel", "In a popup (its bar opens it)" }, { "panel", "popup" }, "panel", "open", "Where the table is");
    setUpCombo (sideBox, { "Left", "Right" }, { "left", "right" }, "left", "side", "The window's side it joins");
    setUpSlider (widthSlider, 120, 480, 1, 220, "width", "How wide it is: the window is wider by it", " px");
    setUpToggle (toggleToggle, false, "toggle", "A handle on the window's edge opens and closes it: the window wider only while it's open");
    setUpToggle (startOpenToggle, false, "startOpen", "Open from the start (otherwise its handle opens it)");
    titleBox.setTooltip ("Its title, at its top");
    titleBox.onReturnKey = titleBox.onFocusLost = [this]
    {
        if (! updating && element() != nullptr && titleBox.getText() != valueOf ("title").toString())
            set ("title", titleBox.getText());
    };

    r.header ("Preset manager");
    r.add ("Style", styleBox);
    r.add ("Table", openBox);
    r.add ("Joins", sideBox);
    r.add ("Width", widthSlider);
    r.add ({}, toggleToggle);
    r.add ({}, startOpenToggle);
    r.add ("Title", titleBox);

    setUpSlider (columnsSlider, 1, 8, 1, 3, "columns", "Its buttons across");
    setUpSlider (rowsSlider, 1, 12, 1, 4, "rows", "Its buttons down: a page (more presets: more pages)");
    setUpSlider (rowHeightSlider, 14, 60, 1, 26, "rowHeight", "A row's height", " px");
    setUpSlider (listRowsSlider, 3, 40, 1, 10, "listRows", "How many rows the list shows (more: pages)");
    setUpCombo (categoriesBox, { "Tabs (All and each category)", "Headings over each category", "None" }, { "tabs", "headings", "none" }, "tabs", "categories",
                "The presets' categories: a tab strip to pick one, each one's name over its presets, or none");
    setUpToggle (searchToggle, true, "search", "A search field: type and only the presets whose names have it show");
    setUpToggle (actionsToggle, true, "actions", "Save, Save as, Load and More (the banks, Delete, the folder); a table: a ... beside its pages");
    setUpToggle (plateToggle, true, "plate", "A plate under the table");

    r.header ("Its presets");
    r.add ("Across", columnsSlider);
    r.add ("Down", rowsSlider);
    r.add ("Row height", rowHeightSlider);
    r.add ("List rows", listRowsSlider);
    r.add ("Categories", categoriesBox);
    r.add ({}, searchToggle);
    r.add ({}, actionsToggle);
    r.add ({}, plateToggle);

    setUpCombo (renderBox, { "Flat (2D)", "3D keys" }, { "2d", "3d" }, "2d", "keys.render", "Its buttons flat, or 3D keys lit by the panel's light");
    setUpCombo (keyStyleBox, { "Soft", "Flat", "Glossy" }, { "soft", "flat", "glossy" }, "soft", "keys.style", "Flat buttons: soft (a gradient and a bevel), flat, or glossy");
    setUpCombo (shapeBox, { "Rounded", "Square", "Pill" }, { "round", "rect", "pill" }, "rect", "keys.shape", "Its buttons' shape");
    setUpSlider (roundingSlider, 0, 100, 1, 22, "keys.rounding", "Its buttons' corners", " %");
    setUpSlider (gapSlider, 0, 60, 1, 12, "keys.gap", "Between its buttons", " %");

    juce::StringArray materials;

    for (const auto& m : Model3D::materialNames())
        materials.add (m);

    setUpCombo (materialBox, materials, materials, "Satin plastic", "keys.material", "What the 3D keys are made of");
    setUpSlider (heightSlider, 4, 40, 1, 14, "keys.height", "How tall the 3D keys stand", " %");
    setUpSwatch (capSwatch, juce::Colour (0xff24262b), "keys.capColour", "A button");
    setUpSwatch (capOnSwatch, juce::Colour (0xff27392f), "keys.capOnColour", "The chosen preset's button");
    setUpSwatch (glowSwatch, juce::Colour (0xff2fb36b), "keys.glowColour", "The chosen one's light");
    setUpSlider (glowSlider, 0, 100, 1, 35, "keys.glow", "How much the chosen one glows", " %");
    setUpSwatch (labelSwatch, juce::Colour (0xffc8cbd0), "keys.labelColour", "The presets' names");
    setUpSwatch (labelOnSwatch, juce::Colour (0xffffffff), "keys.labelOnColour", "The chosen one's name");
    setUpSlider (keyTextSlider, 20, 100, 1, 60, "keys.textSize", "How big the names are in the buttons", " %");

    r.header ("Its buttons");
    r.add ("Drawn", renderBox);
    r.add ("Flat style", keyStyleBox);
    r.add ("Shape", shapeBox);
    r.add ("Corners", roundingSlider);
    r.add ("Gap", gapSlider);
    r.add ("Material", materialBox);
    r.add ("Height", heightSlider);
    r.add ("Key, chosen", capSwatch, &capOnSwatch);
    r.add ("Glow", glowSwatch, &glowSlider);
    r.add ("Names", labelSwatch, &labelOnSwatch);
    r.add ("Name size", keyTextSlider);

    setUpSwatch (accentSwatch, juce::Colour (0xff2fb36b), "colour", "Its accent: arrows, the chosen tab, headings, the title");
    setUpSwatch (panelSwatch, juce::Colour (0xff17191c), "panelColour", "Its panel (a list's, a popup's, a side panel's)");
    setUpSwatch (textSwatch, juce::Colour (0xffd8dadf), "textColour", "Its text");
    setUpSwatch (lineSwatch, juce::Colour (0xff2c2f35), "lineColour", "Its lines");

    fontBox.addItem ("Standard", 1);

    for (int i = 0; i < (int) FontLibrary::families().size(); ++i)
        fontBox.addItem (FontLibrary::families()[(size_t) i].name, i + 2);

    fontBox.setTooltip ("Its font");
    fontBox.onChange = [this] { set ("font", fontBox.getSelectedId() <= 1 ? juce::String() : fontBox.getText()); };
    setUpSlider (textSizeSlider, 40, 200, 1, 100, "textSize", "How big its text is", " %");

    setUpCombo (barDisplayBox, { "Its name on the bar", "LED digits (number and name)" }, { "keys", "led" }, "keys", "barDisplay",
                "The bar: the preset's name on it, or its number and name in glowing LED digits between the arrows, as 80s synths had them");
    setUpCombo (ledSegmentsBox, { "Auto (16 with letters)", "7 segments", "14 segments", "16 segments" }, { "auto", "7", "14", "16" }, "auto",
                "ledSegments", "The bars each character has: 7 for numbers, 14 or 16 for letters too");
    setUpSlider (ledNumberSlider, 0, 4, 1, 2, "ledNumber", "The preset's number: how many digits (0: no number)");
    setUpSwatch (ledSwatch, juce::Colour (0xffff3b2a), "ledColour", "The digits' colour (one for all of them)");
    setUpSwatch (ledBackSwatch, juce::Colour (0xff0c0a0a), "ledBackground", "The window's colour");
    setUpSlider (ledGhostSlider, 0, 60, 1, 10, "ledGhost", "How much the bars that aren't lit show", " %");
    setUpSlider (ledGlowSlider, 0, 100, 1, 40, "ledGlow", "The light round the lit bars", " %");
    setUpSlider (ledSlantSlider, 0, 15, 0.5, 6, "ledSlant", "Leaning forward, as printed LED digits do", juce::String::fromUTF8 ("\xc2\xb0"));
    setUpSlider (ledThicknessSlider, 6, 30, 1, 16, "ledThickness", "The bars' thickness, of a character's width", " %");
    setUpCombo (ledWindowBox, { "No window", "A dark window", "A window in a bezel (3D)" }, { "none", "flat", "3d" }, "flat", "ledWindow",
                "What the digits sit in");

    r.header ("Its bar");
    r.add ("Shows", barDisplayBox);
    r.add ("Segments", ledSegmentsBox);
    r.add ("Number digits", ledNumberSlider);
    r.add ("Digits, window", ledSwatch, &ledBackSwatch);
    r.add ("Dark bars", ledGhostSlider);
    r.add ("Glow", ledGlowSlider);
    r.add ("Slant", ledSlantSlider);
    r.add ("Bars' thickness", ledThicknessSlider);
    r.add ("Window", ledWindowBox);

    r.header ("Look");
    r.add ("Accent, panel", accentSwatch, &panelSwatch);
    r.add ("Text, lines", textSwatch, &lineSwatch);
    r.add ("Font", fontBox);
    r.add ("Text size", textSizeSlider);
}

void PresetStudio::refresh()
{
    const juce::ScopedValueSetter<bool> busy (updating, true);
    const auto d = design();

    for (auto& [s, link] : links->sliders)
    {
        const auto value = valueOf (link.key);
        s->setValue (value.isVoid() ? link.fallback : (double) value, juce::dontSendNotification);
    }

    for (auto& [box, link] : links->combos)
    {
        auto value = valueOf (link.key).toString().trim();

        if (value.isEmpty())
            value = link.key == "style" ? PresetPanel::styleOf (d) : link.fallback;

        int index = 0;

        for (int i = 0; i < link.values.size(); ++i)
            if (link.values[i].equalsIgnoreCase (value))
                index = i;

        box->setSelectedItemIndex (index, juce::dontSendNotification);
    }

    for (auto& [t, link] : links->toggles)
    {
        const auto value = valueOf (link.key);
        t->setToggleState (value.isVoid() ? link.fallback : (bool) value, juce::dontSendNotification);
    }

    for (auto& [s, link] : links->swatches)
        s->setColourValue (colourOf (valueOf (link.key), link.fallback), juce::dontSendNotification);

    titleBox.setText (d.getProperty ("title", "Presets").toString(), false);
    fontBox.setSelectedId (1, juce::dontSendNotification);

    for (int i = 0; i < fontBox.getNumItems(); ++i)
        if (fontBox.getItemText (i).equalsIgnoreCase (d.getProperty ("font", {}).toString()))
            fontBox.setSelectedItemIndex (i, juce::dontSendNotification);

    lastSeen = stateKey();
    showRows();
    preview->repaint();
}

void PresetStudio::showRows()
{
    const auto d = design();
    const auto style = PresetPanel::styleOf (d);
    const bool side = style == "side", table = style == "table", list = style == "list", bar = style == "bar";
    const bool threeD = valueOf ("keys.render").toString() == "3d";
    auto& r = *rows;

    r.show (openBox, table);
    r.show (sideBox, side);
    r.show (widthSlider, side);
    r.show (toggleToggle, side);
    r.show (startOpenToggle, side && PresetPanel::toggles (d));
    r.show (titleBox, side);
    r.show (columnsSlider, table);
    r.show (rowsSlider, table);
    r.show (rowHeightSlider, list || side);
    r.show (listRowsSlider, list);
    r.show (categoriesBox, ! bar);
    r.show (searchToggle, list || side);
    r.show (actionsToggle, false);   // every preset manager has them
    r.show (plateToggle, table && ! PresetPanel::opensPopup (d));
    r.show (keyStyleBox, ! threeD);
    r.show (materialBox, threeD);
    r.show (heightSlider, threeD);
    r.show (capOnSwatch, ! bar);
    r.show (glowSwatch, ! bar);
    r.show (labelSwatch, true);
    r.show (keyTextSlider, ! bar);
    r.show (gapSlider, ! bar);

    // Its bar's LED digits: the bar, a list's and a popup table's.
    const bool hasBar = ! side && ! (table && ! PresetPanel::opensPopup (d));
    const bool led = hasBar && PresetPanel::barIsLed (d);
    r.show (barDisplayBox, hasBar);

    for (auto* c : std::initializer_list<juce::Component*> { &ledSegmentsBox, &ledNumberSlider, &ledSwatch, &ledGhostSlider, &ledGlowSlider,
                                                            &ledSlantSlider, &ledThicknessSlider, &ledWindowBox })
        r.show (*c, led);

    rows->setSize (rowsView.getMaximumVisibleWidth(), rows->layOut (rowsView.getMaximumVisibleWidth()));
    rows->repaint();
}

void PresetStudio::saveToGallery()
{
    if (element() == nullptr)
        return;

    auto file = object();
    put (file, "presets", design().clone());

    auto* ask = new juce::AlertWindow ("Keep in the Preset gallery", "Its name in the gallery:", juce::MessageBoxIconType::NoIcon, this);
    ask->addTextEditor ("name", "My preset manager");
    ask->addButton ("Keep", 1, juce::KeyPress (juce::KeyPress::returnKey));
    ask->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    ask->enterModalState (true, juce::ModalCallbackFunction::create ([ask, file, safe = juce::Component::SafePointer<PresetStudio> (this)] (int result)
    {
        const auto name = ask->getTextEditorContents ("name").trim();

        if (result != 1 || name.isEmpty())
            return;

        put (file, "name", name);
        const auto target = PresetPanel::galleryFolder().getChildFile (juce::File::createLegalFileName (name) + PresetPanel::fileExtension);

        if (! target.replaceWithText (juce::JSON::toString (file)) && safe != nullptr)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Keep in the gallery",
                                                    "It couldn't be written to " + target.getFullPathName());
    }), true);
}

//==============================================================================
void PresetStudio::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::outline);
    g.drawHorizontalLine (44, 0.0f, (float) getWidth());
}

void PresetStudio::resized()
{
    auto area = getLocalBounds();
    auto top = area.removeFromTop (44).reduced (8, 8);
    keepButton.setBounds (top.removeFromLeft (140));
    top.removeFromLeft (4);
    galleryButton.setBounds (top.removeFromLeft (84));

    area.reduce (8, 8);
    rowsView.setBounds (area.removeFromRight (390));
    rows->setSize (rowsView.getMaximumVisibleWidth(), rows->layOut (rowsView.getMaximumVisibleWidth()));
    area.removeFromRight (8);
    preview->setBounds (area);
}
