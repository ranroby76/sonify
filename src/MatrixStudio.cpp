// D:\workspace\Fanan_web_hub\src\MatrixStudio.cpp

#include "MatrixStudio.h"
#include "GalleryHub.h"
#include "Render3D.h"

#include "FontLibrary.h"
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

    juce::StringArray listOf (const juce::String& text)
    {
        juce::StringArray list;
        list.addTokens (text, ",", "\"");
        list.trim();
        list.removeEmptyStrings();
        return list;
    }

    juce::String textOf (const juce::var& list)
    {
        juce::StringArray items;

        if (const auto* a = list.getArray())
            for (const auto& item : *a)
                items.add (item.toString());

        return items.joinIntoString (", ");
    }

    juce::Array<juce::var> arrayOf (const juce::StringArray& list)
    {
        juce::Array<juce::var> a;

        for (const auto& item : list)
            a.add (item);

        return a;
    }

    /** A matrix's picture for the gallery: some routes, made-up destinations. */
    ModMatrix::View sampleView (const juce::var& d)
    {
        ModMatrix::View view;
        view.sources = ModMatrix::sourcesOf (d);
        view.destinations = { "Cutoff", "Resonance", "Pitch", "Volume", "Pan", "Drive", "Attack", "Decay" };

        for (int i = 0; i < 8; ++i)
            view.columns.push_back (i + 1);

        const ModMatrix::Route sample[] { { 1, 1, 0.6f }, { 3, 4, -0.4f }, { 6, 2, 0.8f }, { 2, 7, 0.3f } };

        for (int i = 0; i < ModMatrix::routesOf (d); ++i)
            view.routes.push_back (i < 4 ? sample[i] : ModMatrix::Route {});

        return view;
    }

    struct StudioWindow final : public juce::DocumentWindow
    {
        explicit StudioWindow (juce::Component* content)
            : juce::DocumentWindow ("Matrix Studio", Theme::panel, juce::DocumentWindow::closeButton)
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

    struct SliderLink { juce::Identifier key; double fallback; double scale; };
    struct ComboLink { juce::Identifier key; juce::StringArray values; juce::String fallback; };
    struct SwatchLink { juce::Identifier key; juce::Colour fallback; };
}

struct MatrixStudio::Links
{
    std::map<juce::Slider*, SliderLink> sliders;
    std::map<juce::ComboBox*, ComboLink> combos;
    std::map<ColourSwatchButton*, SwatchLink> swatches;
};

//==============================================================================
class MatrixStudio::Rows final : public juce::Component
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
                row.first->setBounds (area.removeFromLeft (area.getWidth() * 3 / 4 - 2));
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
/** The matrix as the panel shows it, its routes as they are (they're played in Play). */
class MatrixStudio::Preview final : public juce::Component
{
public:
    explicit Preview (MatrixStudio& s) : studio (s) {}

    void paint (juce::Graphics& g) override
    {
        const auto& layout = studio.panel();
        g.setGradientFill (juce::ColourGradient (layout.backgroundTop, 0, 0, layout.backgroundBottom, 0, (float) getHeight(), false));
        g.fillAll();

        const auto* w = studio.matrix();

        if (w == nullptr)
            return;

        // Its own size, as big as fits.
        const auto box = w->bounds.toFloat();
        const auto room = getLocalBounds().reduced (24).toFloat();
        const float scale = juce::jmin (room.getWidth() / juce::jmax (1.0f, box.getWidth()), room.getHeight() / juce::jmax (1.0f, box.getHeight()), 2.5f);
        const auto area = juce::Rectangle<float> (box.getWidth() * scale, box.getHeight() * scale).withCentre (room.getCentre());

        {
            juce::Graphics::ScopedSaveState state (g);
            g.addTransform (juce::AffineTransform::scale (scale).translated (area.getX(), area.getY()));

            // A part lying on its own: as the window shows it.
            if (ModMatrix::isPart (*w))
            {
                g.addTransform (juce::AffineTransform::translation (-box.getX(), -box.getY()));
                studio.drawPart (g);
                return;
            }

            auto view = studio.view();

            // No routes yet: a few shown, so its look shows.
            bool any = false;

            for (const auto& r : view.routes)
                any = any || r.active();

            if (! any && ! view.destinations.isEmpty())
                for (size_t i = 0; i < view.routes.size() && i < 3; ++i)
                    view.routes[i] = { (int) i + 1, view.columns.empty() ? 1 : view.columns[i % view.columns.size()], 0.7f - 0.5f * (float) i };

            ModMatrix::paint (g, studio.design(), box.withZeroOrigin(), view);
        }

        g.setColour (Theme::muted);
        g.setFont (Theme::font (11.5f));
        g.drawText ("Its routes are played in Play: a source and a destination picked, an amount dragged, a pin clicked, a cable plugged",
                    getLocalBounds().reduced (10, 6), juce::Justification::bottomLeft, true);
    }

private:
    MatrixStudio& studio;
};

//==============================================================================
void MatrixStudio::paintGalleryPicture (juce::Graphics& g, const juce::var& design, juce::Rectangle<float> area)
{
    const float w = area.getWidth(), h = area.getHeight();
    juce::Graphics::ScopedSaveState state (g);
    g.addTransform (juce::AffineTransform::translation (area.getX(), area.getY()));

    // A part lying on its own: as big as reads well.
    if (ModMatrix::isPart (design))
    {
        const bool jack = design.getProperty ("style", {}).toString().trim().equalsIgnoreCase ("jack");
        const auto part = jack ? juce::Rectangle<float> (64.0f, 84.0f) : juce::Rectangle<float> (150.0f, 30.0f);
        const float size = juce::jmin (w / part.getWidth(), h / part.getHeight(), jack ? 1.3f : 1.5f);
        g.addTransform (juce::AffineTransform::scale (size).translated ((w - part.getWidth() * size) * 0.5f, (h - part.getHeight() * size) * 0.5f));
        ModMatrix::paintPart (g, design, part, jack && ModMatrix::roleOf (design) == "in" ? "Cutoff" : "LFO 1", true);
        return;
    }

    // Drawn at a panel's size, then shrunk: it reads as it will.
    const float scale = juce::jmin (w / 420.0f, h / 240.0f);
    g.addTransform (juce::AffineTransform::scale (scale).translated ((w - 420.0f * scale) * 0.5f, (h - 240.0f * scale) * 0.5f));
    ModMatrix::paint (g, design, { 420.0f, 240.0f }, sampleView (design));
}

//==============================================================================
MatrixStudio::MatrixStudio (PluginCanvas& c, int widgetIndex)
    : canvas (c), widget (widgetIndex), links (std::make_unique<Links>())
{
    keepButton.setTooltip ("This matrix's look in the Matrix gallery, under a name of its own, for every plugin");
    keepButton.onClick = [this] { saveToGallery(); };
    addAndMakeVisible (keepButton);

    galleryButton.setTooltip ("The Gallery window on this one's page: the templates and the ones kept, to pick from");
    galleryButton.onClick = [this] { GalleryHub::show (canvas, GalleryHub::Page::matrix, this); };
    addAndMakeVisible (galleryButton);

    preview = std::make_unique<Preview> (*this);
    addAndMakeVisible (*preview);

    rows = std::make_unique<Rows>();
    setUpRows();
    rowsView.setViewedComponent (rows.get(), false);
    rowsView.setScrollBarsShown (true, false);
    addAndMakeVisible (rowsView);

    // Its own copy of its design (a duplicate, or one made from the toolbox, shares its first one's).
    if (auto* w = matrix(); w != nullptr && w->settings.count ("matrix") > 0 && w->settings["matrix"].isObject())
        w->settings["matrix"] = w->settings["matrix"].clone();

    refresh();
    setSize (932, 760);
    startTimer (60);
}

MatrixStudio::~MatrixStudio()
{
    stopTimer();
}

std::unique_ptr<juce::DocumentWindow> MatrixStudio::makeWindow (PluginCanvas& canvas, int widgetIndex)
{
    auto* studio = new MatrixStudio (canvas, widgetIndex);
    auto window = std::make_unique<StudioWindow> (studio);
    window->centreAroundComponent (&canvas, studio->getWidth(), studio->getHeight());
    return window;
}

//==============================================================================
const GuiLayout& MatrixStudio::panel() const    { return canvas.layout; }

GuiWidget* MatrixStudio::matrix() const
{
    return juce::isPositiveAndBelow (widget, (int) canvas.layout.widgets.size()) && canvas.layout.widgets[(size_t) widget].type == GuiWidget::Type::matrix
               ? &canvas.layout.widgets[(size_t) widget] : nullptr;
}

juce::var MatrixStudio::design() const
{
    const auto* w = matrix();
    return w != nullptr ? ModMatrix::designOf (*w) : juce::var();
}

juce::var& MatrixStudio::ensureDesign()
{
    auto& settings = matrix()->settings;

    if (! settings["matrix"].isObject())
        settings["matrix"] = ModMatrix::makeTemplate (0);

    return settings["matrix"];
}

ModMatrix::View MatrixStudio::view() const
{
    const auto* w = matrix();
    return w != nullptr ? canvas.matrixView (*w) : ModMatrix::View();
}

juce::String MatrixStudio::stateKey() const
{
    const auto* w = matrix();
    return w != nullptr ? juce::JSON::toString (design(), true) + "|" + w->param : juce::String();
}

void MatrixStudio::set (const juce::Identifier& key, const juce::var& value, bool final)
{
    if (updating || matrix() == nullptr)
        return;

    put (ensureDesign(), key, value);
    changed (final);
}

void MatrixStudio::changed (bool final)
{
    if (matrix() == nullptr)
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

void MatrixStudio::useDesign (const juce::var& newDesign)
{
    auto* w = matrix();

    if (w == nullptr)
        return;

    // Its look from the gallery; its routes, sources and destinations as they were.
    const auto before = design();
    auto d = newDesign.clone();

    for (const auto* key : { "routes", "sources", "targets", "source", "target" })
        if (before.hasProperty (key))
            put (d, key, before.getProperty (key, {}));

    w->settings["matrix"] = d;
    changed (true);
    refresh();
}

void MatrixStudio::timerCallback()
{
    // A drag that ended without saying so: sharp again.
    if (dragDraft != nullptr && ! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        changed (true);

    preview->repaint();   // its routes, as they're played

    if (matrix() != nullptr && stateKey() != lastSeen)
        refresh();
}

//==============================================================================
void MatrixStudio::setUpSlider (juce::Slider& s, double from, double to, double step, double fallback, const char* key, const juce::String& tip, const juce::String& suffix)
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
    const double scale = juce::String (key) == "amount" ? 0.01 : 1.0;
    s.onValueChange = [this, &s, key, scale] { set (key, s.getValue() * scale, ! s.isMouseButtonDown()); };
    s.onDragEnd = [this] { if (! updating) changed (true); };
    links->sliders[&s] = { key, fallback, scale };
}

void MatrixStudio::setUpCombo (juce::ComboBox& box, const juce::StringArray& captions, const juce::StringArray& values, const juce::String& fallback,
                               const char* key, const juce::String& tip)
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

void MatrixStudio::setUpSwatch (ColourSwatchButton& s, juce::Colour fallback, const char* key, const juce::String& tip)
{
    s.setTooltip (tip);
    s.onColourChange = [this, &s, key] { set (key, "#" + s.getColourValue().toDisplayString (true)); };
    links->swatches[&s] = { key, fallback };
}

void MatrixStudio::setUpRows()
{
    auto& r = *rows;

    paramBox.setTooltip ("Its parameters' first part: each route is <this>_src<n>, <this>_dst<n> and <this>_amt<n> (mod_src1...)");
    paramBox.onReturnKey = paramBox.onFocusLost = [this]
    {
        auto* w = matrix();

        if (updating || w == nullptr || paramBox.getText().trim().isEmpty() || paramBox.getText().trim() == w->param)
            return;

        w->param = paramBox.getText().trim().removeCharacters (" ");
        changed (true);
    };

    setUpCombo (styleBox, { "Slots (a list of routes)", "Grid (pins or knobs)", "Patch bay (cables)", "A jack on its own", "Source chip (drag onto a knob)", "Assign button" },
                { "slots", "grid", "patch", "jack", "chip", "assign" }, "slots", "style",
                "Slots: a route a row (source, destination, amount). Grid: sources down, destinations across, a pin or a knob where they meet. "
                "Patch bay: output jacks, input jacks, cables between them. A jack, a chip, an assign button: one part lying anywhere on the "
                "window, sharing the matrix of the same Parameters (its cables run across the window; a chip dragged onto a knob routes to it)");
    setUpSlider (routesSlider, 1, 64, 1, 8, "routes", "How many routes it has: each is a source, a destination and an amount (three of the plugin's parameters)");

    r.header ("Matrix");
    r.add ("Style", styleBox);
    r.add ("Routes", routesSlider);
    r.add ("Parameters", paramBox);

    // A part's: a jack's way, its source (the matrix's sources), an input's parameter.
    setUpCombo (roleBox, { "Output (a source)", "Input (a parameter)" }, { "out", "in" }, "out", "role", "An output jack sends its source; an input jack takes a parameter's modulation");
    sourceBox.setTooltip ("Its source: one of the matrix's sources");
    sourceBox.onChange = [this] { if (sourceBox.getSelectedId() > 0) set ("source", sourceBox.getSelectedId()); };
    targetButton.setTooltip ("The parameter it modulates");
    targetButton.onClick = [this] { pickTarget(); };

    r.header ("This part");
    r.add ("Jack", roleBox);
    r.add ("Source", sourceBox);
    r.add ("Parameter", targetButton);

    sourcesBox.setTooltip ("Its modulation sources, in order, comma separated (LFO 1, Env 2, Velocity, Mod wheel...)");
    sourcesBox.onReturnKey = sourcesBox.onFocusLost = [this]
    {
        if (! updating && matrix() != nullptr && listOf (sourcesBox.getText()) != ModMatrix::sourcesOf (design()))
            set ("sources", arrayOf (listOf (sourcesBox.getText())));
    };

    targetsBox.setTooltip ("The destinations a grid's columns or a patch bay's inputs show: the plugin's parameters' ids, comma separated "
                           "(none: its first eight). A slot's destination can be any of its parameters");
    targetsBox.onReturnKey = targetsBox.onFocusLost = [this]
    {
        if (updating || matrix() == nullptr || targetsBox.getText().trim() == textOf (design().getProperty ("targets", {})))
            return;

        if (listOf (targetsBox.getText()).isEmpty())
        {
            if (auto* o = ensureDesign().getDynamicObject())
                o->removeProperty ("targets");

            changed (true);
        }
        else
        {
            set ("targets", arrayOf (listOf (targetsBox.getText())));
        }
    };

    targetsButton.setTooltip ("The plugin's parameters, ticked on and off");
    targetsButton.onClick = [this] { pickTargets(); };

    r.header ("Sources and destinations");
    r.add ("Sources", sourcesBox);
    r.add ("Shown", targetsBox, &targetsButton);

    setUpCombo (cellBox, { "Pins (the amount a ring)", "Knobs", "Bars" }, { "pin", "knob", "bar" }, "pin", "cell", "What's where a source meets a destination");
    setUpCombo (jackBox, { "Eurorack (a nut)", "Studio (a round socket)", "Minimal" }, { "eurorack", "studio", "minimal" }, "eurorack", "jack", "The jacks");
    setUpSlider (sagSlider, 0, 100, 1, 45, "sag", "How the cables hang", " %");
    setUpSlider (amountSlider, -100, 100, 1, 50, "amount", "A new route's amount", " %");
    headerToggle.onClick = [this] { set ("header", headerToggle.getToggleState()); };
    matchToggle.setTooltip ("A patch bay: as many inputs as it has outputs (its sources): the plugin's parameters first, "
                            "then inputs of their own (In 1, In 2...) for what's still to be wired");
    matchToggle.onClick = [this] { set ("matchInputs", matchToggle.getToggleState()); };

    cablesBox.setTooltip ("The cables' colours in turn, comma separated (#FFE8473A, #FF3A8EF2...); none: a rainbow");
    cablesBox.onReturnKey = cablesBox.onFocusLost = [this]
    {
        if (! updating && matrix() != nullptr && cablesBox.getText().trim() != textOf (design().getProperty ("cables", {})))
            set ("cables", arrayOf (listOf (cablesBox.getText())));
    };

    r.header ("Its routes");
    r.add ({}, headerToggle);
    r.add ("Where they meet", cellBox);
    r.add ("Jacks", jackBox);
    r.add ({}, matchToggle);
    r.add ("Cables hang", sagSlider);
    r.add ("Cable colours", cablesBox);
    r.add ("A new one", amountSlider);

    setUpSwatch (accentSwatch, juce::Colour (0xff2fb36b), "colour", "Its accent: amounts, pins, lit jacks (a negative amount: its opposite)");
    setUpSwatch (panelSwatch, juce::Colour (0xff141518), "panelColour", "Its panel");
    setUpSwatch (textSwatch, juce::Colour (0xffd8dadf), "textColour", "Its text");
    setUpSwatch (lineSwatch, juce::Colour (0xff2c2e33), "lineColour", "Its lines");

    fontBox.addItem ("Standard", 1);

    for (int i = 0; i < (int) FontLibrary::families().size(); ++i)
        fontBox.addItem (FontLibrary::families()[(size_t) i].name, i + 2);

    fontBox.setTooltip ("Its font");
    fontBox.onChange = [this] { set ("font", fontBox.getSelectedId() <= 1 ? juce::String() : fontBox.getText()); };
    setUpSlider (textSizeSlider, 40, 200, 1, 100, "textSize", "How big its text is", " %");

    r.header ("Look");
    r.add ("Accent, panel", accentSwatch, &panelSwatch);
    r.add ("Text, lines", textSwatch, &lineSwatch);
    r.add ("Font", fontBox);
    r.add ("Text size", textSizeSlider);
}

void MatrixStudio::pickTargets()
{
    const auto v = view();
    juce::StringArray ids;

    for (const auto& p : canvas.params)
        if (! ModMatrix::isMatrixParam (canvas.layout, p.id))
            ids.add (p.id);

    const auto chosen = listOf (targetsBox.getText());
    juce::PopupMenu menu;

    for (int i = 0; i < ids.size(); ++i)
        menu.addItem (i + 1, v.destinations[i] + (v.destinations[i] != ids[i] ? "  (" + ids[i] + ")" : juce::String()), true, chosen.contains (ids[i]));

    if (ids.isEmpty())
        menu.addItem (-1, "The plugin has no parameters yet", false);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&targetsButton),
                        [safe = juce::Component::SafePointer<MatrixStudio> (this), ids] (int result)
                        {
                            if (safe == nullptr || result <= 0)
                                return;

                            auto list = listOf (safe->targetsBox.getText());
                            const auto id = ids[result - 1];

                            if (list.contains (id)) list.removeString (id);
                            else                    list.add (id);

                            safe->targetsBox.setText (list.joinIntoString (", "), false);
                            safe->set ("targets", arrayOf (list));
                        });
}

void MatrixStudio::drawPart (juce::Graphics& g)
{
    canvas.drawMatrixPart (g, widget);
}

void MatrixStudio::pickTarget()
{
    const auto v = view();
    juce::StringArray ids;

    for (const auto& p : canvas.params)
        if (! ModMatrix::isMatrixParam (canvas.layout, p.id))
            ids.add (p.id);

    const auto chosen = ModMatrix::targetOf (design());
    juce::PopupMenu menu;

    for (int i = 0; i < ids.size(); ++i)
        menu.addItem (i + 1, v.destinations[i] + (v.destinations[i] != ids[i] ? "  (" + ids[i] + ")" : juce::String()), true, chosen == ids[i]);

    if (ids.isEmpty())
        menu.addItem (-1, "The plugin has no parameters yet", false);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&targetButton),
                        [safe = juce::Component::SafePointer<MatrixStudio> (this), ids] (int result)
                        {
                            if (safe != nullptr && result > 0)
                            {
                                safe->set ("target", ids[result - 1]);
                                safe->refresh();
                            }
                        });
}

void MatrixStudio::refresh()
{
    const juce::ScopedValueSetter<bool> busy (updating, true);
    const auto d = design();

    for (auto& [s, link] : links->sliders)
        s->setValue (d.getProperty (link.key, {}).isVoid() ? link.fallback : (double) d.getProperty (link.key, 0) / link.scale, juce::dontSendNotification);

    for (auto& [box, link] : links->combos)
    {
        auto value = d.getProperty (link.key, {}).toString().trim();

        if (value.isEmpty())
            value = link.fallback;

        int index = 0;

        for (int i = 0; i < link.values.size(); ++i)
            if (link.values[i].equalsIgnoreCase (value))
                index = i;

        box->setSelectedItemIndex (index, juce::dontSendNotification);
    }

    for (auto& [s, link] : links->swatches)
        s->setColourValue (colourOf (d.getProperty (link.key, {}), link.fallback), juce::dontSendNotification);

    headerToggle.setToggleState ((bool) d.getProperty ("header", true), juce::dontSendNotification);
    matchToggle.setToggleState ((bool) d.getProperty ("matchInputs", false), juce::dontSendNotification);
    sourcesBox.setText (ModMatrix::sourcesOf (d).joinIntoString (", "), false);
    targetsBox.setText (textOf (d.getProperty ("targets", {})), false);
    cablesBox.setText (textOf (d.getProperty ("cables", {})), false);

    fontBox.setSelectedId (1, juce::dontSendNotification);

    for (int i = 0; i < fontBox.getNumItems(); ++i)
        if (fontBox.getItemText (i).equalsIgnoreCase (d.getProperty ("font", {}).toString()))
            fontBox.setSelectedItemIndex (i, juce::dontSendNotification);

    if (const auto* w = matrix())
        paramBox.setText (w->param.isNotEmpty() ? w->param : juce::String ("mod"), false);

    // A part's source, from its matrix's; its parameter, by name.
    sourceBox.clear (juce::dontSendNotification);
    const auto v = view();

    for (int i = 0; i < v.sources.size(); ++i)
        sourceBox.addItem (v.sources[i], i + 1);

    sourceBox.setSelectedId (ModMatrix::sourceOf (d), juce::dontSendNotification);

    const auto target = ModMatrix::targetOf (d);
    const auto* p = target.isNotEmpty() ? canvas.paramById (target) : nullptr;
    targetButton.setButtonText (p != nullptr ? (p->name.isNotEmpty() ? p->name : p->id) : target.isNotEmpty() ? target : juce::String::fromUTF8 ("Pick\xe2\x80\xa6"));

    lastSeen = stateKey();
    showRows();
    preview->repaint();
}

void MatrixStudio::showRows()
{
    const auto d = design();
    const auto style = ModMatrix::styleOf (d);
    const bool part = ModMatrix::isPart (d);
    const auto partStyle = d.getProperty ("style", {}).toString().trim().toLowerCase();
    const bool jack = part && partStyle == "jack", input = jack && ModMatrix::roleOf (d) == "in";
    const auto* w = matrix();
    const bool home = ! part || (w != nullptr && canvas.matrixHomeOf (*w) == widget);   // a part with no whole matrix: its sources its own
    auto& r = *rows;

    r.show (headerToggle, ! part && style == "slots");
    r.show (cellBox, ! part && style == "grid");
    r.show (jackBox, jack || (! part && style == "patch"));
    r.show (sagSlider, ! part && style == "patch");      // a part's cables: its matrix's
    r.show (matchToggle, ! part && style == "patch");
    r.show (cablesBox, ! part && style == "patch");
    r.show (amountSlider, ! input);
    r.show (targetsBox, ! part && style != "slots");
    r.show (routesSlider, home);
    r.show (sourcesBox, home);
    r.show (roleBox, jack);
    r.show (sourceBox, part && ! input);
    r.show (targetButton, input);

    rows->setSize (rowsView.getMaximumVisibleWidth(), rows->layOut (rowsView.getMaximumVisibleWidth()));
    rows->repaint();
}

void MatrixStudio::saveToGallery()
{
    if (matrix() == nullptr)
        return;

    auto file = object();
    put (file, "matrix", design().clone());

    auto* ask = new juce::AlertWindow ("Keep in the Matrix gallery", "Its name in the gallery:", juce::MessageBoxIconType::NoIcon, this);
    ask->addTextEditor ("name", "My matrix");
    ask->addButton ("Keep", 1, juce::KeyPress (juce::KeyPress::returnKey));
    ask->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    ask->enterModalState (true, juce::ModalCallbackFunction::create ([ask, file, safe = juce::Component::SafePointer<MatrixStudio> (this)] (int result)
    {
        const auto name = ask->getTextEditorContents ("name").trim();

        if (result != 1 || name.isEmpty())
            return;

        put (file, "name", name);
        const auto target = ModMatrix::galleryFolder().getChildFile (juce::File::createLegalFileName (name) + ModMatrix::fileExtension);

        if (! target.replaceWithText (juce::JSON::toString (file)) && safe != nullptr)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Keep in the gallery",
                                                    "It couldn't be written to " + target.getFullPathName());
    }), true);
}

//==============================================================================
void MatrixStudio::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::outline);
    g.drawHorizontalLine (44, 0.0f, (float) getWidth());
}

void MatrixStudio::resized()
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
