// D:\workspace\Fanan_web_hub\src\PerformStudio.cpp

#include "PerformStudio.h"
#include "GalleryHub.h"
#include "Render3D.h"

#include "Model3D.h"
#include "PerformControl.h"
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

    /** The box a control's picture fits: tall for an upright wheel, wide for a ribbon... */
    juce::Rectangle<float> shapeFor (const juce::var& d, juce::Rectangle<float> room)
    {
        const auto kind = d.getProperty ("kind", "joystick").toString();
        const bool vertical = (bool) d.getProperty ("vertical", kind != "ribbon");
        float ratio = 1.0f;

        if (kind == "wheel")        ratio = vertical ? 0.42f : 2.4f;
        else if (kind == "wheels")  ratio = 0.8f;
        else if (kind == "ribbon")  ratio = vertical ? 0.3f : 3.4f;
        else if (kind == "lever")   ratio = 1.45f;

        if (room.getWidth() / room.getHeight() > ratio)
            return room.withSizeKeepingCentre (room.getHeight() * ratio, room.getHeight());

        return room.withSizeKeepingCentre (room.getWidth(), room.getWidth() / ratio);
    }

    struct StudioWindow final : public juce::DocumentWindow
    {
        explicit StudioWindow (juce::Component* content)
            : juce::DocumentWindow ("Performance Studio", Theme::panel, juce::DocumentWindow::closeButton)
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

    struct SliderLink { juce::Identifier key; double fallback; };
    struct ComboLink { juce::Identifier key; juce::StringArray values; juce::String fallback; };
    struct ToggleLink { juce::Identifier key; bool fallback; };
    struct SwatchLink { juce::Identifier key; juce::Colour fallback; };
}

struct PerformStudio::Links
{
    std::map<juce::Slider*, SliderLink> sliders;
    std::map<juce::ComboBox*, ComboLink> combos;
    std::map<juce::ToggleButton*, ToggleLink> toggles;
    std::map<ColourSwatchButton*, SwatchLink> swatches;
};

//==============================================================================
class PerformStudio::Rows final : public juce::Component
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
/** It as it looks: drag it, and let it go (a pitch bend springs back). */
class PerformStudio::Preview final : public juce::Component
{
public:
    explicit Preview (PerformStudio& s) : studio (s) {}

    float x = 0.5f, y = 0.5f;

    void paint (juce::Graphics& g) override
    {
        const auto& layout = studio.panel();
        g.setGradientFill (juce::ColourGradient (layout.backgroundTop, 0, 0, layout.backgroundBottom, 0, (float) getHeight(), false));
        g.fillAll();

        const auto d = studio.design();

        if (! d.isObject())
        {
            g.setColour (Theme::muted);
            g.setFont (Theme::font (14.0f));
            g.drawFittedText ("A plain X-Y pad. Pick a control in the Gallery window (the Gallery button above): wheels, a bender, a joystick, a pad, a ribbon.",
                              getLocalBounds().reduced (30), juce::Justification::centred, 4);
            return;
        }

        const Render3D::ScopedLater later (*this);   // made in the background: the preview never holds the window up
        PerformControl::paint (g, d, area(), x, y, layout.lightAngle, layout.lightAltitude, studio.dragDraft != nullptr ? 1.0f : 2.0f);
        g.setColour (Theme::muted);
        g.setFont (Theme::font (11.5f));
        g.drawText ("Drag it to try it", getLocalBounds().reduced (10, 6), juce::Justification::bottomLeft, true);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        part = PerformControl::partAt (studio.design(), area(), e.position);
        move (e.position);
    }

    void mouseDrag (const juce::MouseEvent& e) override    { move (e.position); }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (auto* w = studio.pad(); w != nullptr && PerformControl::modeOf (*w) == "bendmod")
        {
            if (PerformControl::springsBoth (studio.design())) { x = 0.5f; y = 0.5f; }   // a lever: both
            else if (PerformControl::bendAxisOf (*w) == "y")    y = 0.5f;
            else                                                x = 0.5f;
        }

        repaint();
    }

private:
    PerformStudio& studio;
    int part = 0;

    juce::Rectangle<float> area() const
    {
        juce::Rectangle<float> box (160.0f, 160.0f);

        if (const auto* w = studio.pad())
            box = w->bounds.toFloat().withTrimmedBottom ((float) GuiLayout::captionHeight);

        const auto room = getLocalBounds().reduced (30).toFloat();
        const float scale = juce::jmin (room.getWidth() / juce::jmax (1.0f, box.getWidth()), room.getHeight() / juce::jmax (1.0f, box.getHeight()), 3.0f);
        return juce::Rectangle<float> (box.getWidth() * scale, box.getHeight() * scale).withCentre (room.getCentre());
    }

    void move (juce::Point<float> at)
    {
        const auto a = area();
        const auto across = juce::jlimit (0.0f, 1.0f, (at.x - a.getX()) / a.getWidth());
        const auto up = juce::jlimit (0.0f, 1.0f, (a.getBottom() - at.y) / a.getHeight());
        const auto axes = PerformControl::axesOf (studio.design());

        if (axes == "pair")    (part == 1 ? y : x) = up;
        else if (axes == "y")  y = up;
        else if (axes == "x")  x = across;
        else                 { x = across; y = up; }

        repaint();
    }
};

//==============================================================================
void PerformStudio::paintGalleryPicture (juce::Graphics& g, const juce::var& design, juce::Rectangle<float> area, float lightAngle, float lightAltitude)
{
    PerformControl::paint (g, design, shapeFor (design, area.reduced (4.0f)), 0.5f, 0.5f, lightAngle, lightAltitude, 1.0f);
}

//==============================================================================
PerformStudio::PerformStudio (PluginCanvas& c, int widgetIndex)
    : canvas (c), widget (widgetIndex), links (std::make_unique<Links>())
{
    moveLabel.setText ("It moves", juce::dontSendNotification);
    moveLabel.setColour (juce::Label::textColourId, Theme::muted);
    moveLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (moveLabel);

    modeBox.addItem ("Freely (it stays where it's left)", 1);
    modeBox.addItem ("Pitch bend and modulation", 2);
    modeBox.setTooltip ("Freely: it stays where it's left. Pitch bend and modulation: the bend goes back to its middle when it's let go, "
                        "the modulation stays");
    modeBox.onChange = [this] { if (! updating) setSetting ("mode", modeBox.getSelectedId() == 2 ? "bendmod" : "free"); };
    addAndMakeVisible (modeBox);

    bendBox.addItem ("Across (X) bends", 1);
    bendBox.addItem ("Up and down (Y) bends", 2);
    bendBox.setTooltip ("Which of its two ways is the pitch bend (the pair of wheels: X is the left wheel); the other modulates");
    bendBox.onChange = [this] { if (! updating) setSetting ("bendAxis", bendBox.getSelectedId() == 2 ? "y" : "x"); };
    addAndMakeVisible (bendBox);

    for (auto* b : { &keepButton, &plainButton })
        addAndMakeVisible (*b);

    keepButton.setTooltip ("This control in the Performance gallery, under a name of its own, for every plugin");
    keepButton.onClick = [this] { saveToGallery(); };
    plainButton.setTooltip ("Back to a plain X-Y pad");
    plainButton.onClick = [this]
    {
        if (auto* w = pad())
        {
            w->settings.erase ("perform");
            changed (true);
            refresh();
        }
    };

    galleryButton.setTooltip ("The Gallery window on this one's page: the templates and the ones kept, to pick from");
    galleryButton.onClick = [this] { GalleryHub::show (canvas, GalleryHub::Page::perform, this); };
    addAndMakeVisible (galleryButton);

    preview = std::make_unique<Preview> (*this);
    addAndMakeVisible (*preview);

    rows = std::make_unique<Rows>();
    setUpRows();
    rowsView.setViewedComponent (rows.get(), false);
    rowsView.setScrollBarsShown (true, false);
    addAndMakeVisible (rowsView);

    // Its own copy of its design (a duplicate, or one made from the toolbox, shares its first one's).
    if (auto* w = pad(); w != nullptr && w->settings.count ("perform") > 0 && w->settings["perform"].isObject())
        w->settings["perform"] = w->settings["perform"].clone();

    refresh();
    setSize (932, 760);
    startTimer (40);
}

PerformStudio::~PerformStudio()
{
    stopTimer();
}

std::unique_ptr<juce::DocumentWindow> PerformStudio::makeWindow (PluginCanvas& canvas, int widgetIndex)
{
    auto* studio = new PerformStudio (canvas, widgetIndex);
    auto window = std::make_unique<StudioWindow> (studio);
    window->centreAroundComponent (&canvas, studio->getWidth(), studio->getHeight());
    return window;
}

//==============================================================================
const GuiLayout& PerformStudio::panel() const    { return canvas.layout; }

bool PerformStudio::alive() const
{
    return juce::isPositiveAndBelow (widget, (int) canvas.layout.widgets.size()) && canvas.layout.widgets[(size_t) widget].type == GuiWidget::Type::xy;
}

GuiWidget* PerformStudio::pad() const
{
    return alive() ? &canvas.layout.widgets[(size_t) widget] : nullptr;
}

juce::var PerformStudio::design() const
{
    return alive() ? PerformControl::designOf (canvas.layout.widgets[(size_t) widget]) : juce::var();
}

juce::var& PerformStudio::ensureDesign()
{
    auto& settings = pad()->settings;

    if (! settings["perform"].isObject())
        settings["perform"] = PerformControl::makeTemplate (0);

    return settings["perform"];
}

juce::String PerformStudio::stateKey() const
{
    const auto* w = pad();

    if (w == nullptr)
        return {};

    juce::String roles;

    for (const auto& [role, id] : w->roles)
        roles << role << "=" << id << ";";

    return juce::JSON::toString (design(), true) + PerformControl::modeOf (*w) + PerformControl::bendAxisOf (*w) + roles;
}

void PerformStudio::set (const juce::Identifier& key, const juce::var& value, bool final)
{
    if (updating || ! alive())
        return;

    put (ensureDesign(), key, value);
    changed (final);
}

void PerformStudio::setSetting (const juce::String& key, const juce::var& value)
{
    if (auto* w = pad())
    {
        w->settings[key] = value;
        changed (true);
    }
}

void PerformStudio::changed (bool final)
{
    if (! alive())
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

void PerformStudio::useTemplate (const juce::var& newDesign, const juce::var& settings)
{
    auto* w = pad();

    if (w == nullptr)
        return;

    w->settings["perform"] = newDesign.clone();

    if (auto* o = settings.getDynamicObject())
        for (const auto& property : o->getProperties())
            w->settings[property.name.toString()] = property.value;

    // Its parameters, when it has none yet.
    if (w->roles.count ("x") == 0 && w->roles.count ("y") == 0)
    {
        const auto kind = newDesign.getProperty ("kind", {}).toString();
        const bool bending = settings.getProperty ("mode", {}).toString() == "bendmod";
        const auto bendAxis = settings.getProperty ("bendAxis", "x").toString();
        w->roles["x"] = bending ? (bendAxis == "x" ? "pitchbend" : "modwheel") : (kind == "pad" || kind == "joystick" ? "padx" : "control1");
        w->roles["y"] = bending ? (bendAxis == "y" ? "pitchbend" : "modwheel") : (kind == "pad" || kind == "joystick" ? "pady" : "control2");
    }

    preview->x = preview->y = 0.5f;
    changed (true);
    refresh();
}

void PerformStudio::timerCallback()
{
    // A drag that ended without saying so: sharp again.
    if (dragDraft != nullptr && ! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        changed (true);


    if (alive() && stateKey() != lastSeen)
        refresh();
}

//==============================================================================
void PerformStudio::setUpSlider (juce::Slider& s, double from, double to, double step, double fallback, const char* key, const juce::String& tip, const juce::String& suffix)
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

void PerformStudio::setUpCombo (juce::ComboBox& box, const juce::StringArray& captions, const juce::StringArray& values, const juce::String& fallback,
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

void PerformStudio::setUpToggle (juce::ToggleButton& t, bool fallback, const char* key, const juce::String& tip)
{
    t.setTooltip (tip);
    t.onClick = [this, &t, key] { set (key, t.getToggleState()); };
    links->toggles[&t] = { key, fallback };
}

void PerformStudio::setUpSwatch (ColourSwatchButton& s, juce::Colour fallback, const char* key, const juce::String& tip)
{
    s.setTooltip (tip);
    s.onColourChange = [this, &s, key] { set (key, "#" + s.getColourValue().toDisplayString (true)); };
    links->swatches[&s] = { key, fallback };
}

void PerformStudio::setUpRows()
{
    auto& r = *rows;

    for (auto* box : { &xParamBox, &yParamBox })
    {
        box->onReturnKey = box->onFocusLost = [this, box]
        {
            auto* w = pad();

            if (updating || w == nullptr)
                return;

            const auto role = box == &xParamBox ? juce::String ("x") : juce::String ("y");
            const auto text = box->getText().trim();

            if (text == (w->roles.count (role) > 0 ? w->roles[role] : juce::String()))
                return;

            if (text.isEmpty()) w->roles.erase (role);
            else                w->roles[role] = text;

            changed (true);
        };
    }

    xParamBox.setTooltip ("The parameter it moves across (X); the pair of wheels: the left wheel's");
    yParamBox.setTooltip ("The parameter it moves up and down (Y); the pair of wheels: the right wheel's");

    r.header ("Parameters");
    r.add ("Across (X)", xParamBox);
    r.add ("Up and down (Y)", yParamBox);

    setUpCombo (kindBox, { "Wheel", "Pitch and mod wheels", "Bender lever", "Joystick", "X-Y pad", "Ribbon" },
                { "wheel", "wheels", "lever", "joystick", "pad", "ribbon" }, "joystick", "kind", "What it is");
    setUpCombo (styleBox, { "Ball on a cross", "Vector (cross slot)", "Arcade stick", "Gimbal in a boot", "Thumbstick" },
                { "ball", "vector", "arcade", "gimbal", "thumb" }, "ball", "style", "The joystick's kind");
    setUpToggle (verticalToggle, true, "vertical", "A wheel or a ribbon up and down (else across)");
    setUpCombo (renderBox, { "3D", "Flat (2D)" }, { "3d", "2d" }, "3d", "render", "A pad or a ribbon: 3D, or flat");

    r.header ("It");
    r.add ("Kind", kindBox);
    r.add ("Joystick", styleBox);
    r.add ({}, verticalToggle);
    r.add ("Drawn", renderBox);

    setUpSwatch (housingSwatch, juce::Colour (0xff1a1b1e), "housingColour", "Its housing");
    setUpSwatch (partSwatch, juce::Colour (0xff202124), "partColour", "The wheel, the ball, the stick, the strip");
    setUpSwatch (accentSwatch, juce::Colour (0xff5fe0e8), "accentColour", "Its mark, the puck, its lamps, its light");

    juce::StringArray materials;

    for (const auto& m : Model3D::materialNames())
        materials.add (m);

    setUpCombo (materialBox, materials, materials, "Satin plastic", "material", "What the moving part is made of");
    setUpSlider (glowSlider, 0, 100, 1, 60, "glow", "How bright its light is", " %");
    setUpToggle (litToggle, false, "lit", "A light in its slot (a wheel) or under its pad");
    setUpToggle (ledsToggle, false, "leds", "A ribbon: lamps along it, lit up to where it's touched");
    setUpToggle (middleToggle, false, "fromMiddle", "Its lamps lit from the middle (a bend)");
    setUpToggle (cleanToggle, false, "clean", "A wheel: plain, no ribs and no mark");

    r.header ("Look");
    r.add ("Housing", housingSwatch);
    r.add ("Moving part", partSwatch, &materialBox);
    r.add ("Light", accentSwatch, &glowSlider);
    r.add ({}, litToggle);
    r.add ({}, ledsToggle, &middleToggle);
    r.add ({}, cleanToggle);

    setUpSlider (travelSlider, 10, 100, 1, 60, "travel", "How far it moves or leans", " %");
    setUpSlider (rangeSlider, 10, 170, 1, 160, "range", "Push and pull angle: how far it leans, its whole sweep edge to edge (160: 80 degrees each way)",
                 juce::String::fromUTF8 ("\xc2\xb0"));
    setUpSlider (tiltSlider, 0, 45, 1, 0, "tilt", "Seen from the front: 0 straight down", juce::String::fromUTF8 ("\xc2\xb0"));
    setUpSlider (framesSlider, 3, 65, 1, 17, "frames", "Pictures each way (the exported plugin plays them as frames)");

    r.header ("Movement and pictures");
    r.add ("Travel", travelSlider);
    r.add ("Push / pull angle", rangeSlider);
    r.add ("Seen from", tiltSlider);
    r.add ("Pictures each way", framesSlider);
}

void PerformStudio::refresh()
{
    const juce::ScopedValueSetter<bool> busy (updating, true);
    const auto d = design();

    for (auto& [s, link] : links->sliders)
    {
        auto fallback = link.fallback;

        if (link.key == juce::Identifier ("frames"))
            fallback = PerformControl::framesOf (d);

        if (link.key == juce::Identifier ("range"))
            fallback = PerformControl::rangeOf (d);

        s->setValue (d.getProperty (link.key, {}).isVoid() ? fallback : (double) d.getProperty (link.key, 0), juce::dontSendNotification);
    }

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

    for (auto& [t, link] : links->toggles)
    {
        auto fallback = link.fallback;

        if (link.key == juce::Identifier ("vertical"))
            fallback = d.getProperty ("kind", {}).toString() != "ribbon";

        t->setToggleState ((bool) d.getProperty (link.key, fallback), juce::dontSendNotification);
    }

    for (auto& [s, link] : links->swatches)
        s->setColourValue (colourOf (d.getProperty (link.key, {}), link.fallback), juce::dontSendNotification);

    if (const auto* w = pad())
    {
        modeBox.setSelectedId (PerformControl::modeOf (*w) == "bendmod" ? 2 : 1, juce::dontSendNotification);
        bendBox.setSelectedId (PerformControl::bendAxisOf (*w) == "y" ? 2 : 1, juce::dontSendNotification);
        xParamBox.setText (w->roles.count ("x") > 0 ? w->roles.at ("x") : juce::String(), false);
        yParamBox.setText (w->roles.count ("y") > 0 ? w->roles.at ("y") : juce::String(), false);
        lastSeen = stateKey();
    }

    showRows();
    preview->repaint();
}

void PerformStudio::showRows()
{
    const auto d = design();
    const auto kind = d.getProperty ("kind", "joystick").toString();
    const bool any = d.isObject();
    auto& r = *rows;

    for (auto& row : r.rows)
        if (! row.header)
            row.shown = any || row.first == &xParamBox || row.first == &yParamBox;

    if (any)
    {
        r.show (styleBox, kind == "joystick");
        r.show (verticalToggle, kind == "wheel" || kind == "ribbon");
        r.show (renderBox, kind == "pad" || kind == "ribbon");

        // A joystick and the lever lean through their angle; the rest have no travel to set.
        const bool leaning = PerformControl::leans (d);
        r.show (rangeSlider, leaning);
        r.show (travelSlider, false);
        r.show (cleanToggle, kind == "wheel" || kind == "wheels");
        r.show (litToggle, kind == "wheel" || kind == "wheels" || kind == "pad");
        r.show (ledsToggle, kind == "ribbon");
    }

    bendBox.setVisible (pad() != nullptr && PerformControl::modeOf (*pad()) == "bendmod");
    keepButton.setEnabled (any);
    plainButton.setEnabled (any);

    rows->setSize (rowsView.getMaximumVisibleWidth(), rows->layOut (rowsView.getMaximumVisibleWidth()));
    rows->repaint();
}

void PerformStudio::saveToGallery()
{
    const auto* w = pad();

    if (w == nullptr || ! design().isObject())
        return;

    auto file = object();
    put (file, "perform", design().clone());
    auto settings = object();
    put (settings, "mode", PerformControl::modeOf (*w));
    put (settings, "bendAxis", PerformControl::bendAxisOf (*w));
    put (file, "settings", settings);

    auto* ask = new juce::AlertWindow ("Keep in the Performance gallery", "Its name in the gallery:", juce::MessageBoxIconType::NoIcon, this);
    ask->addTextEditor ("name", "My control");
    ask->addButton ("Keep", 1, juce::KeyPress (juce::KeyPress::returnKey));
    ask->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    ask->enterModalState (true, juce::ModalCallbackFunction::create ([ask, file, safe = juce::Component::SafePointer<PerformStudio> (this)] (int result)
    {
        const auto name = ask->getTextEditorContents ("name").trim();

        if (result != 1 || name.isEmpty())
            return;

        put (file, "name", name);
        const auto target = PerformControl::galleryFolder().getChildFile (juce::File::createLegalFileName (name) + PerformControl::fileExtension);

        if (! target.replaceWithText (juce::JSON::toString (file)) && safe != nullptr)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Keep in the gallery",
                                                    "It couldn't be written to " + target.getFullPathName());
    }), true);
}

//==============================================================================
void PerformStudio::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::outline);
    g.drawHorizontalLine (44, 0.0f, (float) getWidth());
}

void PerformStudio::resized()
{
    auto area = getLocalBounds();
    auto top = area.removeFromTop (44).reduced (8, 8);

    moveLabel.setBounds (top.removeFromLeft (70));
    top.removeFromLeft (6);
    modeBox.setBounds (top.removeFromLeft (250));
    top.removeFromLeft (6);
    bendBox.setBounds (top.removeFromLeft (190));
    top.removeFromLeft (16);
    keepButton.setBounds (top.removeFromLeft (128));
    top.removeFromLeft (4);
    galleryButton.setBounds (top.removeFromLeft (84));
    plainButton.setBounds (top.removeFromRight (120));

    area.reduce (8, 8);
    rowsView.setBounds (area.removeFromRight (380));
    rows->setSize (rowsView.getMaximumVisibleWidth(), rows->layOut (rowsView.getMaximumVisibleWidth()));
    area.removeFromRight (8);
    preview->setBounds (area);
}
