// D:\workspace\Fanan_web_hub\src\PluginCanvas.cpp
// The canvas's elements: adding them, drawing them, baking them for Export. The view and
// the mouse are in PluginCanvas_Mouse.cpp, the edit menu in PluginCanvas_Menu.cpp.

#include "PluginCanvas.h"

#include <algorithm>
#include <cmath>
#include <set>
#include "PluginCanvasMenu.h"
#include "LayerStyleWindow.h"
#include "FontLibrary.h"
#include "MeterPaint.h"
#include "PanelParts.h"
#include "StellaLookAndFeel.h"
#include "WasmElements.h"
#include "KnobStudio.h"   // the Gallery
#include "stella_keys.h"

namespace
{
    constexpr int scopeSamples = 2048;
    const juce::Colour defaultAccent { 0xffe5484d };
    const juce::Colour captionColour { 0xffd8d4ca };

    /** A value as a caption or readout shows it: whole numbers on a wide range and on a
        percentage, never more than two decimals, its unit after it. */
    juce::String valueText (float value, const PluginCanvas::Param& p)
    {
        const auto range = p.max - p.min;
        const auto unit = p.unit.trim();
        // a log range (a frequency, a time): by the value's size, not the range's
        const auto logRange = (unit == "Hz" || unit == "ms") && p.min > 0.0f && p.max / p.min >= 8.0f;
        const auto size = std::abs (value);
        const auto decimals = p.decimals >= 0 ? juce::jmin (2, p.decimals)
                            : logRange ? (size < 10.0f ? 2 : (size < 100.0f ? 1 : 0))
                            : (unit == "%" || range >= 100.0f ? 0 : (range >= 10.0f ? 1 : 2));
        return juce::String (value, decimals) + (p.unit.isNotEmpty() ? " " + p.unit : juce::String());
    }
}

//==============================================================================
bool PluginCanvas::isInteractive (const GuiWidget& w)
{
    return w.isControl() || w.type == GuiWidget::Type::xy || w.type == GuiWidget::Type::preset || w.type == GuiWidget::Type::keyboard
        || w.type == GuiWidget::Type::custom || w.type == GuiWidget::Type::tabs;
}

juce::String PluginCanvas::typeDisplayName (GuiWidget::Type type)
{
    switch (type)
    {
        case GuiWidget::Type::knob:      return "Knob";
        case GuiWidget::Type::slider:    return "Slider";
        case GuiWidget::Type::toggle:    return "Switch";
        case GuiWidget::Type::selector:  return "Selector";
        case GuiWidget::Type::label:     return "Text";
        case GuiWidget::Type::group:     return "Frame";
        case GuiWidget::Type::meter:     return "Level meter";
        case GuiWidget::Type::scope:     return "Scope";
        case GuiWidget::Type::lamp:      return "Lamp";
        case GuiWidget::Type::envelope:  return "Envelope curve";
        case GuiWidget::Type::filter:    return "Filter curve";
        case GuiWidget::Type::xy:        return "XY pad";
        case GuiWidget::Type::shape:     return "Shape";
        case GuiWidget::Type::preset:    return "Preset manager";
        case GuiWidget::Type::image:     return "Picture";
        case GuiWidget::Type::keyboard:  return "Keyboard";
        case GuiWidget::Type::custom:    return "Programmed element";
        case GuiWidget::Type::registration: return "Registration";
        case GuiWidget::Type::tabs:      return "Page tabs";
    }

    return "Element";
}

juce::Colour PluginCanvas::standardColour (const GuiWidget& w)
{
    if (w.type == GuiWidget::Type::label || w.type == GuiWidget::Type::group || w.type == GuiWidget::Type::shape)
        return captionColour;

    return w.type == GuiWidget::Type::lamp || w.type == GuiWidget::Type::registration ? juce::Colour (0xffffb020) : defaultAccent;
}

juce::Rectangle<int> PluginCanvas::defaultBounds (GuiWidget::Type type, int x, int y)
{
    switch (type)
    {
        case GuiWidget::Type::knob:      return { x, y, 52, 52 + GuiLayout::captionHeight };
        case GuiWidget::Type::slider:    return { x, y, 32, 140 };
        case GuiWidget::Type::toggle:    return { x, y, 48, 66 };
        case GuiWidget::Type::selector:  return { x, y, 180, 48 };
        case GuiWidget::Type::label:     return { x, y, 200, 28 };
        case GuiWidget::Type::group:     return { x, y, 240, 160 };
        case GuiWidget::Type::meter:     return { x, y, 18, 140 };
        case GuiWidget::Type::scope:     return { x, y, 200, 110 };
        case GuiWidget::Type::lamp:      return { x, y, 30, 44 };
        case GuiWidget::Type::envelope:  return { x, y, 200, 110 };
        case GuiWidget::Type::filter:    return { x, y, 200, 110 };
        case GuiWidget::Type::xy:        return { x, y, 160, 180 };
        case GuiWidget::Type::shape:     return { x, y, 60, 60 };
        case GuiWidget::Type::preset:    return { x, y, 220, 28 };
        case GuiWidget::Type::image:     return { x, y, 160, 120 };
        case GuiWidget::Type::keyboard:  return { x, y, 600, 90 };
        case GuiWidget::Type::custom:    return { x, y, 160, 32 };
        case GuiWidget::Type::registration: return { x, y, 236, 26 };
        case GuiWidget::Type::tabs:      return { x, y, 300, 30 };
    }

    return { x, y, 60, 60 };
}

const char* PluginCanvas::defaultStarPath()
{
    return "M50,5 L61,39 L97,39 L68,61 L79,95 L50,74 L21,95 L32,61 L3,39 L39,39 Z";
}

//==============================================================================
PluginCanvas::PluginCanvas()
{
    setWantsKeyboardFocus (true);
    setOpaque (true);   // paint() always fills it: nothing behind needs painting first
    scopeBuffer.resize ((size_t) scopeSamples, 0.0f);

    menu = std::make_unique<ElementMenu> (*this);
    addChildComponent (*menu);

    banner = std::make_unique<AiBanner> (*this);
    addChildComponent (*banner);

    // The zoom, bottom left: Fit, a slider (double-click: 100%) and the percentage.
    fitButton.setTooltip ("Show the whole window (Ctrl+0)");
    fitButton.onClick = [this] { fitToView(); };

    zoomSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    zoomSlider.setRange (minZoom, maxZoom, 0.01);
    zoomSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    zoomSlider.setDoubleClickReturnValue (true, 1.0);
    zoomSlider.setValue (1.0, juce::dontSendNotification);
    zoomSlider.setTooltip ("Zoom (also Ctrl + mouse wheel). Double-click: 100%");
    zoomSlider.onValueChange = [this]
    {
        autoFit = false;
        setZoom ((float) zoomSlider.getValue());
    };

    zoomLabel.setFont (Theme::font (12.0f, true));
    zoomLabel.setJustificationType (juce::Justification::centredLeft);
    zoomLabel.setColour (juce::Label::textColourId, Theme::muted);
    zoomLabel.setInterceptsMouseClicks (false, false);

    for (auto* c : std::initializer_list<juce::Component*> { &fitButton, &zoomSlider, &zoomLabel })
        addChildComponent (c);

    // The crop tool, beside Fit; its Apply and Cancel while it's out.
    cropButton.setTooltip ("Crop the window, as in Photoshop (C): drag the box's sides and corners in or out, then Enter");
    cropButton.setClickingTogglesState (false);
    cropButton.onClick = [this] { if (cropping) cancelCrop(); else startCrop(); };
    cropApplyButton.setTooltip ("Crop the window to the box (Enter, or double-click inside it)");
    cropApplyButton.onClick = [this] { applyCrop(); };
    cropCancelButton.setTooltip ("Leave the window as it was (Esc)");
    cropCancelButton.onClick = [this] { cancelCrop(); };
    cropApplyButton.setColour (juce::TextButton::buttonColourId, Theme::accent);

    for (auto* c : std::initializer_list<juce::Component*> { &cropButton, &cropApplyButton, &cropCancelButton })
        addChildComponent (c);

    shapePainter = std::make_unique<juce::ThreadPool> (1);
    startTimerHz (30);
}

PluginCanvas::~PluginCanvas()
{
    *alive = false;
    stopTimer();
    shapePainter = nullptr;   // a shape still being painted is let go (its picture finds no canvas)

    if (elements != nullptr)
        elements->setHost (nullptr);

    pictureChooser = nullptr;
}

void PluginCanvas::setGuiFolder (const juce::File& folder)
{
    if (folder == guiFolder)
        return;

    cancelCrop();
    guiFolder = folder;
    lightKnown = false;
    pictures.clear();

    // Paints asked for in the project before are let go (they'd be painted into this one), and
    // so are words typed in its menu.
    ++paintGeneration;
    paintingPicture = false;
    pendingRecipes.clear();
    menu->forgetTyped();

    dropGhost.reset();
    select (-1);

    // Each project keeps its own view; one never moved fits the window.
    autoFit = true;
    zoom = 1.0f;
    pan = {};
    loadView();

    for (auto* c : std::initializer_list<juce::Component*> { &fitButton, &zoomSlider, &zoomLabel })
        c->setVisible (hasPanel() && ! panelOnly);

    cropButton.setVisible (design && hasPanel() && ! panelOnly);
    banner->setVisible (false);

    if (autoFit)
        fitToView();
    else
        viewChanged();
}

void PluginCanvas::setLayout (const GuiLayout& newLayout)
{
    const bool sizeChanged = newLayout.width != layout.width || newLayout.height != layout.height;

    // The global light moved (Sonify AI, an undo): painted backgrounds are painted again in it.
    const bool lightMoved = lightKnown && (newLayout.lightAngle != layout.lightAngle || newLayout.lightAltitude != layout.lightAltitude);
    lightKnown = true;

    releaseKey();   // its keyboard may be gone, or elsewhere now
    menu->forgetTyped();   // words typed in the menu were for the layout before
    layout = newLayout;
    indexesChanged();
    manySelected.clear();   // the elements may be other ones now
    shownPage = juce::jlimit (0, juce::jmax (0, layout.pages.size() - 1), shownPage);
    syncElements();

    // Pictures that weren't there may be now, and a picture painted again is a new file.
    for (auto it = pictures.begin(); it != pictures.end();)
    {
        const auto known = pictureTimes.find (it->first);
        const bool repainted = known == pictureTimes.end()
                          || imagesFolder().getChildFile (it->first).getLastModificationTime() != known->second;
        it = it->second.isValid() && ! repainted ? std::next (it) : pictures.erase (it);
    }

    if (! juce::isPositiveAndBelow (selected, (int) layout.widgets.size()))
        selected = -1;

    if (selected < 0 && ! panelSelected)
        closeMenu();
    else if (menuOpen)
        openMenu();   // shows the element's new state

    if (autoFit && sizeChanged)
        fitToView();
    else
        viewChanged();

    reportSources();

    if (lightMoved)
        repaintBackgrounds();
}

void PluginCanvas::setDesignMode (bool shouldDesign)
{
    releaseKey();
    closeElementPopups();
    hoveredElement = -1;
    design = shouldDesign;

    if (! design)
    {
        cancelCrop();
        select (-1);
    }

    cropButton.setVisible (design && hasPanel() && ! panelOnly);

    repaint();
}

void PluginCanvas::setPanelOnly (bool shouldBePanelOnly)
{
    panelOnly = shouldBePanelOnly;

    if (panelOnly)
        setDesignMode (false);

    for (auto* c : std::initializer_list<juce::Component*> { &fitButton, &zoomSlider, &zoomLabel })
        c->setVisible (hasPanel() && ! panelOnly);

    cropButton.setVisible (design && hasPanel() && ! panelOnly);
    autoFit = true;
    fitToView();
}

void PluginCanvas::setParameters (const juce::Array<Param>& newParams)
{
    // The same parameters with some values moved (automation, the Hub, a preset): only what
    // shows those is painted again, not the whole panel.
    auto sameList = newParams.size() == params.size();

    for (int i = 0; sameList && i < params.size(); ++i)
    {
        const auto& a = params.getReference (i);
        const auto& b = newParams.getReference (i);
        sameList = a.index == b.index && a.id == b.id && a.name == b.name && a.unit == b.unit && a.decimals == b.decimals
                && juce::exactlyEqual (a.min, b.min) && juce::exactlyEqual (a.max, b.max)
                && juce::exactlyEqual (a.def, b.def) && juce::exactlyEqual (a.skew, b.skew);
    }

    std::vector<int> moved;

    if (sameList)
        for (const auto& p : newParams)
            if (const auto found = values.find (p.index); found == values.end() || ! juce::exactlyEqual (found->second, p.value))
                moved.push_back (p.index);

    params = newParams;
    values.clear();

    for (const auto& p : params)
        values[p.index] = p.value;

    if (menuOpen)
        openMenu();   // the parameter lists

    if (sameList)
        repaintParameters (moved);
    else
        repaint();
}

void PluginCanvas::repaintParameters (const std::vector<int>& indices)
{
    if (indices.empty())
        return;

    if (design)
    {
        repaint();
        return;
    }

    std::set<juce::String> ids;

    for (const auto& p : params)
        if (std::find (indices.begin(), indices.end(), p.index) != indices.end())
            ids.insert (p.id);

    juce::RectangleList<int> areas;

    for (const auto& w : layout.widgets)
    {
        if (! isShown (w))
            continue;

        // A programmed element may show any value: it's painted again too.
        auto shows = w.type == GuiWidget::Type::custom || ids.count (w.param) > 0;

        for (const auto& [role, id] : w.roles)
            shows = shows || ids.count (id) > 0;

        if (! shows)
            continue;

        // Its look may reach past its box (a shadow, an LED's glow): a margin round it.
        const auto area = viewAreaOf (w);
        areas.add (area.expanded (area.getWidth() / 4 + 8, area.getHeight() / 4 + 8));
    }

    if (areas.getNumRectangles() > 24)
        repaint (areas.getBounds());
    else
        for (const auto& area : areas)
            repaint (area);
}

bool PluginCanvas::liesOutside (const GuiWidget& w, juce::Rectangle<float> painted) const
{
    // Only controls and displays with a bounded picture: shapes, lines, labels, pictures and
    // programmed elements can reach far past their boxes (shadows, glows, effects), so they're
    // always painted.
    switch (w.type)
    {
        case GuiWidget::Type::knob:
        case GuiWidget::Type::slider:
        case GuiWidget::Type::toggle:
        case GuiWidget::Type::selector:
        case GuiWidget::Type::meter:
        case GuiWidget::Type::lamp:
        case GuiWidget::Type::scope:
        case GuiWidget::Type::preset:
        case GuiWidget::Type::keyboard:
            break;

        default:
            return false;
    }

    // Turned about its middle, its look reaching past its box: a generous circle round it.
    const auto box = w.bounds.toFloat();
    const auto reach = std::hypot (box.getWidth(), box.getHeight()) * 0.5f * 1.4f + 24.0f;
    return ! juce::Rectangle<float> (box.getCentreX() - reach, box.getCentreY() - reach, reach * 2.0f, reach * 2.0f).intersects (painted);
}

void PluginCanvas::setSources (const juce::StringArray& signals, const juce::StringArray& displays)
{
    signalSources = signals;
    displaySources = displays;

    if (menuOpen)
        openMenu();
}

void PluginCanvas::edited()
{
    syncElements();
    reportSources();

    if (onLayoutEdited != nullptr)
        onLayoutEdited();

    layOutOverlays();
    repaint();
}

void PluginCanvas::reportSources()
{
    // Tell the live preview which signals the GUI watches, when that changes.
    juce::StringArray levels, scopes;

    for (const auto& w : layout.widgets)
    {
        if ((w.type == GuiWidget::Type::meter || w.type == GuiWidget::Type::lamp) && w.source.isNotEmpty())
            levels.addIfNotAlreadyThere (w.source);
        else if (w.type == GuiWidget::Type::scope && w.source.isNotEmpty() && scopes.size() < 3)
            scopes.addIfNotAlreadyThere (w.source);
    }

    // A programmed element's sources (its own and those by role): their levels, and their
    // samples while a scope slot is free. A live filter's cutoff display.
    for (const auto& w : layout.widgets)
    {
        if (w.type == GuiWidget::Type::filter && w.source.isNotEmpty())
            levels.addIfNotAlreadyThere (w.source);

        if (w.type != GuiWidget::Type::custom)
            continue;

        juce::StringArray watched;

        if (w.source.isNotEmpty())
            watched.add (w.source);

        for (const auto& [role, source] : w.sources)
            watched.addIfNotAlreadyThere (source);

        for (const auto& source : watched)
        {
            levels.addIfNotAlreadyThere (source);

            if (scopes.size() < 3 && ! displaySources.contains (source))
                scopes.addIfNotAlreadyThere (source);
        }
    }

    if (levels != reportedLevels || scopes != reportedScopes)
    {
        reportedLevels = levels;
        reportedScopes = scopes;

        if (onSourcesChanged != nullptr)
            onSourcesChanged (levels, scopes);
    }
}

//==============================================================================
namespace
{
    /** What a 3D display (stella_builtin_3d.cpp) binds by role, and the word its parameter's or
        display's name has: a knob, or (live) a module's display. */
    struct Binding3d
    {
        const char* role;
        const char* word;
        bool live;
    };

    std::vector<Binding3d> displayBindings3d (const juce::String& element)
    {
        if (element == "WaveTerrain")
            return { { "timbre", "timbre", false }, { "morph", "morph", false }, { "position", "morph", true }, { "level", "env", true } };

        if (element == "MorphSurface")
            return { { "mix", "mix", false }, { "timbreA", "timbre", false }, { "morphA", "morph", false },
                     { "mix", "mix", true }, { "level", "env", true } };

        if (element == "FilterSweep")
            return { { "cutoff", "cutoff", false }, { "resonance", "reso", false }, { "cutoff", "cutoff", true }, { "level", "env", true } };

        if (element == "SoundWaterfall")
            return { { "", "", false } };   // a signal: its "source"

        return {};
    }
}

void PluginCanvas::addPrimitive (const Primitive& primitive, std::optional<juce::Point<float>> centre)
{
    if (! design || ! hasPanel())
        return;

    // The background picture: a picture element filling the window, behind everything.
    if (primitive.setsBackground())
    {
        choosePicture (-1);
        return;
    }

    const auto index = addElement (primitive.makeWidget(), centre);

    // A picture needs its file: ask for it at once.
    const auto& w = layout.widgets[(size_t) index];

    if (w.type == GuiWidget::Type::image && w.image.isEmpty())
        choosePicture (index);
}

juce::Rectangle<int> PluginCanvas::placed (juce::Rectangle<int> bounds, juce::Point<float> centre) const
{
    auto b = bounds.withPosition (snapped (centre.x - (float) bounds.getWidth() * 0.5f),
                                  snapped (centre.y - (float) bounds.getHeight() * 0.5f));

    b.setX (juce::jlimit (0, juce::jmax (0, layout.width - b.getWidth()), b.getX()));
    b.setY (juce::jlimit (0, juce::jmax (0, layout.height - b.getHeight()), b.getY()));
    return b;
}

int PluginCanvas::addElement (GuiWidget w, std::optional<juce::Point<float>> centre)
{
    auto unusedParam = [this] (const juce::StringArray& avoid)
    {
        for (const auto& p : params)
        {
            bool shown = avoid.contains (p.id);

            for (const auto& other : layout.widgets)
                shown = shown || other.param == p.id;

            if (! shown)
                return p.id;
        }

        return juce::String();
    };

    auto paramNamed = [this] (const juce::String& word)
    {
        for (const auto& p : params)
            if (p.id.fromFirstOccurrenceOf (".", false, false).containsIgnoreCase (word) || p.name.containsIgnoreCase (word))
                return p.id;

        return juce::String();
    };

    // What the primitive leaves open: the first parameter not shown yet, its name as the
    // caption, the look its kind mostly wears in this window, a source to watch.
    switch (w.type)
    {
        case GuiWidget::Type::knob:
        case GuiWidget::Type::slider:
        case GuiWidget::Type::toggle:
        case GuiWidget::Type::selector:
            if (w.param.isEmpty())
                w.param = unusedParam ({});

            if (w.label.isEmpty())
                for (const auto& p : params)
                    if (p.id == w.param)
                        w.label = p.name;

            if (Looks::takesLook (w) && w.style.isEmpty())
                w.style = commonLook (w);

            if (w.type == GuiWidget::Type::selector && w.options.isEmpty())
                w.options = { "One", "Two", "Three" };
            break;

        case GuiWidget::Type::label:     if (w.label.isEmpty()) w.label = "Text"; break;
        case GuiWidget::Type::group:     if (w.label.isEmpty()) w.label = "SECTION"; break;

        case GuiWidget::Type::meter:
            if (w.source.isEmpty()) w.source = "plugin.out L";
            if (w.mode.isEmpty())   w.mode = "peak";
            break;

        case GuiWidget::Type::scope:
            if (w.source.isEmpty()) w.source = "plugin.out L";
            break;

        case GuiWidget::Type::lamp:
            if (w.source.isEmpty()) w.source = displaySources.isEmpty() ? juce::String ("plugin.out L") : displaySources[0];
            break;

        case GuiWidget::Type::envelope:
            if (w.roles.empty())
                for (const auto& role : GuiWidget::rolesOf (w.type))
                    if (const auto id = paramNamed (role); id.isNotEmpty())
                        w.roles[role] = id;
            break;

        case GuiWidget::Type::filter:
            if (w.roles.empty())
            {
                if (const auto id = paramNamed ("cutoff"); id.isNotEmpty())    w.roles["cutoff"] = id;
                if (const auto id = paramNamed ("reso"); id.isNotEmpty())      w.roles["resonance"] = id;
            }

            if (w.mode.isEmpty())
                w.mode = "lowpass";
            break;

        case GuiWidget::Type::xy:
            if (w.roles.empty())
            {
                const auto x = unusedParam ({});
                const auto y = unusedParam ({ x });
                if (x.isNotEmpty()) w.roles["x"] = x;
                if (y.isNotEmpty()) w.roles["y"] = y;
            }
            break;

        case GuiWidget::Type::shape:
            if (w.shapeKind() == "drawn" && w.path.isEmpty())
                w.path = defaultStarPath();
            break;

        case GuiWidget::Type::keyboard:
            stella::keys::normalise (w.lowNote, w.highNote);
            break;

        case GuiWidget::Type::custom:
            // A 3D display (stella_builtin_3d.cpp): its knobs and live values by role, found by name.
            if (const auto bindings = displayBindings3d (w.element); ! bindings.empty())
            {
                for (const auto& b : bindings)
                {
                    if (*b.role == 0)
                        continue;

                    if (b.live && w.sources.count (b.role) == 0)
                    {
                        for (const auto& shown : displaySources)
                            if (shown.fromFirstOccurrenceOf (".", false, false).containsIgnoreCase (b.word))
                            {
                                w.sources[b.role] = shown;
                                break;
                            }
                    }
                    else if (! b.live && w.roles.count (b.role) == 0)
                    {
                        if (const auto id = paramNamed (b.word); id.isNotEmpty())
                            w.roles[b.role] = id;
                    }
                }

                if (w.element == "SoundWaterfall" && w.source.isEmpty())
                    w.source = "plugin.out L";

                break;
            }

            // A built-in control with roles (a range slider's low and high, a dual knob's outer
            // and inner): the first parameters not shown yet.
            if (const auto roles = w.rolesFor(); ! roles.isEmpty() && w.roles.empty())
            {
                juce::StringArray taken;

                for (const auto& role : roles)
                    if (const auto id = unusedParam (taken); id.isNotEmpty())
                    {
                        w.roles[role] = id;
                        taken.add (id);
                    }

                if (const auto first = w.roles.find (roles[0]); w.label.isEmpty() && first != w.roles.end())
                    if (const auto* p = paramById (first->second))
                        w.label = p->name;

                break;
            }

            // Like a selector: the first parameter not shown yet, named after it.
            if (w.param.isEmpty())
                w.param = unusedParam ({});

            if (w.label.isEmpty())
                if (const auto* p = paramById (w.param))
                    w.label = p->name;
            break;

        case GuiWidget::Type::registration:
            if (w.label.isEmpty())
                w.label = registrationText();
            break;

        case GuiWidget::Type::tabs:
            // The tabs need pages to show: the first ones are made with them.
            if (layout.pages.size() < 2)
            {
                addPage (false);
                w.bounds.setWidth (juce::jlimit (120, layout.width, 110 * layout.pages.size()));
            }
            break;

        case GuiWidget::Type::preset:
        case GuiWidget::Type::image:     break;
    }

    // Pages: what's added lies on the page shown (the tabs, the badge and the preset manager on every page).
    if (! layout.pages.isEmpty() && w.page.isEmpty() && w.type != GuiWidget::Type::tabs && w.type != GuiWidget::Type::registration
        && w.type != GuiWidget::Type::preset)
        w.page = shownPageName();

    // Centred where it was dropped, or in the middle of what's showing; always inside the window.
    w.bounds = placed (w.bounds, centre.value_or (toPlugin (viewArea().getCentre())));

    // Frames go underneath (just above the background picture), everything else on top.
    int index = 0;

    if (w.type == GuiWidget::Type::group)
    {
        index = backBackdrop() + 1;
        layout.widgets.insert (layout.widgets.begin() + index, w);
        indexesChanged();
    }
    else
    {
        layout.widgets.push_back (w);
        index = (int) layout.widgets.size() - 1;
    }

    select (index);
    openMenu();
    edited();
    return index;
}

void PluginCanvas::removeSelected()
{
    const auto all = selection();

    if (all.empty())
        return;

    for (auto it = all.rbegin(); it != all.rend(); ++it)
        layout.widgets.erase (layout.widgets.begin() + *it);

    indexesChanged();
    select (-1);
    edited();
}

//==============================================================================
juce::File PluginCanvas::imagesFolder() const
{
    return hasPanel() ? guiFolder.getChildFile (GuiLayout::imagesFolder) : juce::File();
}

const juce::Image& PluginCanvas::picture (const juce::String& name)
{
    static const juce::Image none;

    if (name.isEmpty() || ! hasPanel())
        return none;

    auto found = pictures.find (name);

    if (found == pictures.end())
    {
        const auto file = imagesFolder().getChildFile (name);
        found = pictures.emplace (name, file.existsAsFile() ? juce::ImageFileFormat::loadFrom (file) : juce::Image()).first;
        pictureTimes[name] = file.getLastModificationTime();
    }

    return found->second;
}

//==============================================================================
juce::var PluginCanvas::paintedRecipeOf (const GuiWidget& w) const
{
    if (w.type != GuiWidget::Type::image || w.image.isEmpty() || ! hasPanel())
        return {};

    const auto recipe = GuiLayout::paintedRecipe (guiFolder, w.image);
    return recipe.getProperty ("element", {}).toString() == "PanelPainter" ? recipe : juce::var();
}

void PluginCanvas::pictureRepainted (const juce::String& fileName)
{
    const auto name = fileName.endsWithIgnoreCase (".png") ? fileName : fileName + ".png";
    pictures.erase (name);
    pictureTimes.erase (name);
    repaint();
}

void PluginCanvas::repaintPainted (const juce::var& recipe)
{
    if (onPaintPicture == nullptr || ! recipe.isObject())
        return;

    // One at a time: while one paints, the newest change to each waits (a dragged slider asks often).
    if (paintingPicture)
    {
        pendingRecipes[recipe.getProperty ("file", {}).toString()] = recipe;
        return;
    }

    paintingPicture = true;
    auto asked = recipe.clone();
    asked.getDynamicObject()->setProperty ("quiet", true);
    const auto file = asked.getProperty ("file", {}).toString();

    // Lit by the panel's global light.
    auto settings = asked.getProperty ("settings", {}).isObject() ? asked.getProperty ("settings", {}) : juce::var (new juce::DynamicObject());
    settings.getDynamicObject()->setProperty ("light", std::round (layout.lightAngle * 10.0f) / 10.0f);
    settings.getDynamicObject()->setProperty ("altitude", std::round (layout.lightAltitude * 10.0f) / 10.0f);
    asked.getDynamicObject()->setProperty ("settings", settings);

    onPaintPicture (asked, [safeThis = juce::Component::SafePointer<PluginCanvas> (this), file, generation = paintGeneration] (const juce::String& problem)
    {
        if (safeThis == nullptr || safeThis->paintGeneration != generation)
            return;   // (another project's: let go)

        safeThis->paintingPicture = false;

        if (problem.isNotEmpty())
            safeThis->showAiReply (problem);
        else
            safeThis->pictureRepainted (file);

        if (! safeThis->pendingRecipes.empty())
        {
            const auto next = safeThis->pendingRecipes.begin()->second;
            safeThis->pendingRecipes.erase (safeThis->pendingRecipes.begin());
            safeThis->repaintPainted (next);
        }
    });
}

void PluginCanvas::paintNewBackground()
{
    if (onPaintPicture == nullptr || ! hasPanel() || paintingPicture)
        return;

    // A name of its own: the user's own pictures are never painted over.
    const auto painted = GuiLayout::paintedPictures (guiFolder);
    juce::String file = "background.png";

    for (int n = 2; imagesFolder().getChildFile (file).existsAsFile() && ! painted.contains (file); ++n)
        file = "background-" + juce::String (n) + ".png";

    auto* settings = new juce::DynamicObject();
    settings->setProperty ("material", "powder");
    settings->setProperty ("key", 0.6);
    settings->setProperty ("light", std::round (layout.lightAngle * 10.0f) / 10.0f);   // the panel's global light
    settings->setProperty ("altitude", std::round (layout.lightAltitude * 10.0f) / 10.0f);
    settings->setProperty ("vignette", 0.45);

    auto* recipe = new juce::DynamicObject();
    recipe->setProperty ("element", "PanelPainter");
    recipe->setProperty ("file", file);
    recipe->setProperty ("width", layout.width);
    recipe->setProperty ("height", layout.height);
    recipe->setProperty ("color", "#FF2B2F36");
    recipe->setProperty ("settings", juce::var (settings));
    recipe->setProperty ("quiet", true);

    paintingPicture = true;
    showAiWorking ("Painting the background");

    onPaintPicture (juce::var (recipe), [safeThis = juce::Component::SafePointer<PluginCanvas> (this), file, generation = paintGeneration] (const juce::String& problem)
    {
        if (safeThis == nullptr || safeThis->paintGeneration != generation)
            return;   // (another project's: let go)

        auto& c = *safeThis;
        c.paintingPicture = false;

        if (! c.pendingRecipes.empty())
        {
            const auto next = c.pendingRecipes.begin()->second;
            c.pendingRecipes.erase (c.pendingRecipes.begin());
            c.repaintPainted (next);
        }

        if (problem.isNotEmpty())
        {
            c.showAiReply (problem);
            return;
        }

        c.showAiReply ("Painted. Choose its material, colours and a pattern in its menu.");
        c.pictureRepainted (file);

        // Behind everything, filling the window (unless it's there already; a background picture
        // there takes it instead), then its menu.
        int index = -1;

        for (int i = 0; i < (int) c.layout.widgets.size() && index < 0; ++i)
            if (c.layout.widgets[(size_t) i].type == GuiWidget::Type::image && c.layout.widgets[(size_t) i].image == file)
                index = i;

        if (index < 0 && c.backBackdrop() >= 0)
        {
            index = c.backBackdrop();
            c.layout.widgets[(size_t) index].image = file;
        }

        if (index < 0)
        {
            c.releaseKey();

            GuiWidget w;
            w.type = GuiWidget::Type::image;
            w.image = file;
            w.mode = "stretch";
            w.bounds = { 0, 0, c.layout.width, c.layout.height };
            c.layout.widgets.insert (c.layout.widgets.begin(), w);
            c.indexesChanged();
            index = 0;
        }

        c.select (index);
        c.openMenu();
        c.edited();
    });
}

//==============================================================================
void PluginCanvas::setGlobalLight (float angle, float altitude)
{
    auto a = std::fmod (angle, 360.0f);

    if (a > 180.0f)        a -= 360.0f;
    else if (a <= -180.0f) a += 360.0f;

    layout.lightAngle = std::round (a * 10.0f) / 10.0f;
    layout.lightAltitude = std::round (juce::jlimit (0.0f, 90.0f, altitude) * 10.0f) / 10.0f;
    repaint();   // the looks and shapes are drawn in the new light
}

void PluginCanvas::repaintBackgrounds()
{
    for (const auto& w : layout.widgets)
        if (const auto recipe = paintedRecipeOf (w); recipe.isObject())
            repaintPainted (recipe);
}

void PluginCanvas::editGlobalLight()
{
    if (! hasPanel())
        return;

    const juce::Point<float> before (layout.lightAngle, layout.lightAltitude);

    GlobalLightWindow::launch (before.x, before.y,
                               [safeThis = juce::Component::SafePointer<PluginCanvas> (this), before] (float angle, float altitude, bool final)
                               {
                                   if (safeThis == nullptr)
                                       return;

                                   safeThis->setGlobalLight (angle, altitude);

                                   // Kept: saved, and the painted backgrounds painted again in it.
                                   if (final && juce::Point<float> (safeThis->layout.lightAngle, safeThis->layout.lightAltitude) != before)
                                   {
                                       safeThis->edited();
                                       safeThis->repaintBackgrounds();
                                   }
                               },
                               this);
}

void PluginCanvas::editLayerStyle (int widgetIndex)
{
    if (! hasPanel() || ! juce::isPositiveAndBelow (widgetIndex, (int) layout.widgets.size())
          || layout.widgets[(size_t) widgetIndex].type != GuiWidget::Type::shape)
        return;

    // As it was, for Cancel (and the start of every change: its older relief, shadow and
    // outline become the style's).
    const auto before = layout.widgets[(size_t) widgetIndex];
    const juce::Point<float> lightBefore (layout.lightAngle, layout.lightAltitude);
    const juce::Component::SafePointer<PluginCanvas> safeThis (this);

    auto shapeAt = [safeThis, widgetIndex]() -> GuiWidget*
    {
        if (safeThis == nullptr || ! juce::isPositiveAndBelow (widgetIndex, (int) safeThis->layout.widgets.size()))
            return nullptr;

        auto& w = safeThis->layout.widgets[(size_t) widgetIndex];
        return w.type == GuiWidget::Type::shape ? &w : nullptr;
    };

    auto restore = [before] (GuiWidget& w)
    {
        w.settings = before.settings;
        w.stroke = before.stroke;
        w.strokeWidth = before.strokeWidth;
    };

    LayerStyleWindow::Host host;
    host.alive = [shapeAt] { return shapeAt() != nullptr; };

    host.apply = [safeThis, shapeAt, restore, lightBefore] (const LayerStyle& style, bool final)
    {
        auto* w = shapeAt();

        if (w == nullptr)
            return;

        restore (*w);
        ObjectPaint::setStyle (*w, style);

        if (final)
        {
            safeThis->edited();

            if (juce::Point<float> (safeThis->layout.lightAngle, safeThis->layout.lightAltitude) != lightBefore)
                safeThis->repaintBackgrounds();
        }

        safeThis->repaint();
    };

    host.cancel = [safeThis, shapeAt, restore, lightBefore]
    {
        if (auto* w = shapeAt())
            restore (*w);

        if (safeThis != nullptr)
            safeThis->setGlobalLight (lightBefore.x, lightBefore.y);
    };

    host.sample = [safeThis, before] (const LayerStyle& style)
    {
        if (safeThis == nullptr)
            return juce::Image();

        // Upright, so the style reads.
        auto shape = before;
        shape.angle = 0.0f;
        ObjectPaint::setStyle (shape, style);
        const auto job = ObjectPaint::jobFor (shape, standardColour (shape).withAlpha (0.9f), safeThis->layout.lightAngle, safeThis->layout.lightAltitude);
        return ObjectPaint::paint (job).image;
    };

    host.globalLight = [safeThis]
    {
        return safeThis != nullptr ? juce::Point<float> (safeThis->layout.lightAngle, safeThis->layout.lightAltitude) : juce::Point<float> (120.0f, 30.0f);
    };

    host.setGlobalLight = [safeThis] (float angle, float altitude)
    {
        if (safeThis != nullptr)
            safeThis->setGlobalLight (angle, altitude);
    };

    const auto& w = layout.widgets[(size_t) widgetIndex];
    const auto kind = w.shapeKind();
    const auto name = w.label.isNotEmpty() ? w.label : kind.substring (0, 1).toUpperCase() + kind.substring (1);
    LayerStyleWindow::launch (ObjectPaint::styleOf (w), std::move (host), "Layer Style: " + name, this);
}

void PluginCanvas::choosePicture (int widgetIndex)
{
    if (! hasPanel())
        return;

    pictureChooser = std::make_unique<juce::FileChooser> (widgetIndex < 0 ? "Choose the background picture" : "Choose a picture",
                                                          juce::File(), GuiLayout::pictureFiles());

    pictureChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                 [safeThis = juce::Component::SafePointer<PluginCanvas> (this), widgetIndex] (const juce::FileChooser& chooser)
                                 {
                                     if (safeThis == nullptr)
                                         return;

                                     const auto file = chooser.getResult();

                                     if (file == juce::File())
                                         return;

                                     const auto name = GuiLayout::importPicture (file, safeThis->imagesFolder());

                                     if (name.isEmpty())
                                     {
                                         juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't use that picture",
                                                                                 "Pictures can be PNG, JPEG or GIF files.");
                                         return;
                                     }

                                     safeThis->usePicture (widgetIndex, name);
                                 });
}

void PluginCanvas::usePicture (int widgetIndex, const juce::String& name)
{
    pictures.erase (name);   // read afresh

    if (widgetIndex < 0)
    {
        // A background picture: a picture element covering the window, at the back (one there
        // already shows the new picture instead). Its corners and sides stretch it; its menu and
        // right-click change its layer.
        auto index = backBackdrop();

        if (index >= 0)
        {
            layout.widgets[(size_t) index].image = name;
        }
        else
        {
            releaseKey();

            GuiWidget backdrop;
            backdrop.type = GuiWidget::Type::image;
            backdrop.image = name;
            backdrop.mode = "stretch";
            backdrop.bounds = { 0, 0, layout.width, layout.height };
            layout.widgets.insert (layout.widgets.begin(), backdrop);
            indexesChanged();
            index = 0;
        }

        layout.backgroundImage.clear();

        select (index);
        edited();
        openMenu();
        return;
    }

    if (juce::isPositiveAndBelow (widgetIndex, (int) layout.widgets.size()))
    {
        auto& w = layout.widgets[(size_t) widgetIndex];
        const bool first = w.image.isEmpty();
        w.image = name;

        // A new picture element takes the picture's own size (made to fit in the window).
        if (const auto& image = picture (name); first && image.isValid())
        {
            const auto scale = juce::jmin (1.0f, (float) layout.width / (float) image.getWidth(), (float) layout.height / (float) image.getHeight());
            const auto size = juce::Rectangle<int> (juce::jmax (8, juce::roundToInt ((float) image.getWidth() * scale)),
                                                    juce::jmax (8, juce::roundToInt ((float) image.getHeight() * scale)));
            w.bounds = placed (size, w.bounds.getCentre().toFloat());
        }
    }

    edited();

    if (menuOpen)
        openMenu();
}

//==============================================================================
void PluginCanvas::setPresets (const PresetMenu& presets)
{
    if (presets == presetMenu)
        return;

    presetMenu = presets;
    presetVersion = (presetVersion + 1) & 0xffffff;
    repaint();
}

void PluginCanvas::setLooks (Looks& looksToUse)
{
    looks = &looksToUse;
    repaint();
}

void PluginCanvas::looksChanged()
{
    // The edit menu lists the looks: it shows the new ones, once whatever changed them is done.
    if (menuOpen)
        juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<PluginCanvas> (this)]
                                         {
                                             if (safeThis != nullptr && safeThis->menuOpen)
                                                 safeThis->openMenu();
                                         });

    repaint();
}

void PluginCanvas::useLook (int widgetIndex, const juce::String& lookName)
{
    if (! juce::isPositiveAndBelow (widgetIndex, (int) layout.widgets.size()))
        return;

    auto& w = layout.widgets[(size_t) widgetIndex];

    if (! Looks::takesLook (w) || w.style == lookName)
        return;

    rememberOriginalLook (w);
    w.style = lookName;
    edited();
}

juce::String PluginCanvas::giveOwnLook (int widgetIndex)
{
    if (looks == nullptr || ! juce::isPositiveAndBelow (widgetIndex, (int) layout.widgets.size()))
        return {};

    auto& w = layout.widgets[(size_t) widgetIndex];

    if (! Looks::takesLook (w))
        return {};

    // A copy of what it wears now, named after it ("Cutoff"), saved as the plugin's own: never
    // the name of a look other controls wear.
    juce::StringArray worn;

    for (const auto& other : layout.widgets)
        if (Looks::takesLook (other))
            worn.addIfNotAlreadyThere (looks->lookFor (other).name);

    const auto& current = looks->lookFor (w);
    const auto doc = current.doc;
    const auto name = looks->freeName (w.label.isNotEmpty() ? w.label : current.name, worn);

    looks->setProjectLook (name, Looks::kindOf (w), doc);
    rememberOriginalLook (w);
    w.style = name;
    edited();
    return name;
}

//==============================================================================
// The Gallery's knobs, sliders and switches
juce::String PluginCanvas::lookFromGallery (const juce::File& file, Looks::Kind& kind)
{
    if (looks == nullptr)
        return {};

    LayerDoc doc;
    juce::String credits, error;
    bool commercialSafe = true;

    if (! KnobStudio::readGalleryItem (file, doc, kind, credits, commercialSafe, error))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Gallery",
                                                "Couldn't use " + file.getFileName() + ": " + error + ".");
        return {};
    }

    // Named after the item, never the name of a look the panel's controls wear already.
    juce::StringArray worn;

    for (const auto& other : layout.widgets)
        if (Looks::takesLook (other))
            worn.addIfNotAlreadyThere (looks->lookFor (other).name);

    auto base = file.getFileNameWithoutExtension().trim();

    if (credits.startsWith ("From the KnobMan gallery"))
        base = Looks::kindDisplayName (kind).substring (0, 1).toUpperCase() + Looks::kindDisplayName (kind).substring (1)
             + " #" + base;   // its number: the gallery's own name for it is in the credits

    const auto name = looks->freeName (base.isNotEmpty() ? base : juce::String ("Gallery look"), worn);
    looks->setProjectLook (name, kind, doc, credits);

    // A licence that doesn't allow selling: said, once, as it goes onto the panel.
    if (! commercialSafe)
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Its licence",
                                                credits + "\n\nThis licence doesn't allow it in a plugin that's sold (or changed, for ND). "
                                                "It's fine in a free plugin; for one with a price, pick an item without the NC mark.");
    return name;
}

void PluginCanvas::chooseGalleryItem (int widgetIndex)
{
    if (! juce::isPositiveAndBelow (widgetIndex, (int) layout.widgets.size()) || ! Looks::takesLook (layout.widgets[(size_t) widgetIndex]))
        return;

    const auto kind = Looks::kindDisplayName (Looks::kindOf (layout.widgets[(size_t) widgetIndex]));

    KnobStudio::showGallery ("Replace this " + kind + " with a gallery " + kind,
                             [safeThis = juce::Component::SafePointer<PluginCanvas> (this), widgetIndex] (const juce::File& file)
                             {
                                 if (safeThis != nullptr)
                                     safeThis->useGalleryItem (widgetIndex, file);
                             },
                             true, false, this);
}

bool PluginCanvas::useGalleryItem (int widgetIndex, const juce::File& file)
{
    if (! design || ! juce::isPositiveAndBelow (widgetIndex, (int) layout.widgets.size()))
        return false;

    const auto wanted = Looks::kindOf (layout.widgets[(size_t) widgetIndex]);

    if (! Looks::takesLook (layout.widgets[(size_t) widgetIndex]))
        return false;

    // Only the same kind: the control stays what it is (its size, place and settings too).
    LayerDoc doc;
    juce::String credits, error;
    bool commercialSafe = true;
    Looks::Kind kind = wanted;

    if (KnobStudio::readGalleryItem (file, doc, kind, credits, commercialSafe, error) && kind != wanted)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Gallery",
                                                file.getFileNameWithoutExtension() + " is a " + Looks::kindDisplayName (kind)
                                                + ", and this is a " + Looks::kindDisplayName (wanted)
                                                + ": pick a " + Looks::kindDisplayName (wanted) + " to replace it with.");
        return false;
    }

    const auto name = lookFromGallery (file, kind);

    if (name.isEmpty())
        return false;

    useLook (widgetIndex, name);
    repaint();
    return true;
}

void PluginCanvas::addGalleryItem (const juce::File& file, std::optional<juce::Point<float>> centre)
{
    if (! design || ! hasPanel())
        return;

    Looks::Kind kind = Looks::Kind::knob;
    const auto name = lookFromGallery (file, kind);

    if (name.isEmpty())
        return;

    GuiWidget w;
    w.type = kind == Looks::Kind::button ? GuiWidget::Type::toggle
           : kind == Looks::Kind::knob   ? GuiWidget::Type::knob
                                         : GuiWidget::Type::slider;
    w.vertical = kind != Looks::Kind::sliderAcross;
    w.bounds = defaultBounds (w.type, 0, 0);

    if (kind == Looks::Kind::sliderAcross)
        w.bounds = { 0, 0, w.bounds.getHeight(), w.bounds.getWidth() };   // lying down

    w.style = name;
    addElement (w, centre);
    edited();
    repaint();
}

juce::String PluginCanvas::commonLook (const GuiWidget& widget) const
{
    std::map<juce::String, int> counts;   // "" counts too: the default look
    const auto kind = Looks::kindOf (widget);

    for (const auto& other : layout.widgets)
        if (Looks::takesLook (other) && Looks::kindOf (other) == kind)
            ++counts[other.style];

    juce::String best;
    int most = 0;

    for (const auto& [name, count] : counts)
    {
        if (count > most)
        {
            best = name;
            most = count;
        }
    }

    return best;
}

//==============================================================================
int PluginCanvas::widgetAt (juce::Point<float> plugin, bool interactiveOnly) const
{
    for (int i = (int) layout.widgets.size(); --i >= 0;)
    {
        const auto& w = layout.widgets[(size_t) i];

        if ((interactiveOnly && ! isInteractive (w)) || ! isShown (w))
            continue;

        // Picked in its own frame: a turned element where it is turned to.
        const auto at = intoWidget (w, plugin);

        if (w.type == GuiWidget::Type::group && ! interactiveOnly)
        {
            // A group is picked by its border and title, so what's inside stays reachable.
            const auto outer = w.bounds.toFloat();

            if (outer.contains (at) && ! outer.reduced (8.0f).withTrimmedTop (16.0f).contains (at))
                return i;

            continue;
        }

        if (! w.bounds.toFloat().contains (at))
            continue;

        if (w.type == GuiWidget::Type::shape)
        {
            // A shape by itself (or a few pixels round it), not its box's empty corners.
            const auto box = w.bounds.toFloat();
            const auto outline = ObjectPaint::outline (w, box.getWidth(), box.getHeight());
            const auto inBox = at - box.getPosition();

            if (box.getWidth() * box.getHeight() < 400.0f || outline.contains (inBox))
                return i;

            juce::Path edge;
            juce::PathStrokeType (juce::jmax (8.0f, w.strokeWidth + 6.0f)).createStrokedPath (edge, outline);

            if (edge.contains (inBox))
                return i;

            continue;
        }

        return i;
    }

    return -1;
}

void PluginCanvas::indexesChanged()
{
    meterLevels.clear();
    meterNeedles.clear();
    meterPeaks.clear();
    shapePaintings.clear();
}

//==============================================================================
void PluginCanvas::showPage (int index)
{
    const auto count = juce::jmax (1, layout.pages.size());
    index = juce::jlimit (0, count - 1, index);

    if (index == shownPage)
        return;

    shownPage = index;
    releaseKey();

    // What's selected on the page that went stays behind.
    std::set<int> still;

    for (const auto i : selection())
        if (isShown (layout.widgets[(size_t) i]))
            still.insert (i);

    if (still.empty() && (selected >= 0 || ! manySelected.empty()))
        select (-1);
    else if (still.size() != selection().size())
        selectMany (still, *still.begin());

    closeElementPopups();
    repaint();
}

int PluginCanvas::tabAt (const GuiWidget& tabs, juce::Point<float> local) const
{
    const auto b = tabs.bounds.toFloat();
    const auto count = juce::jmax (1, layout.pages.size());

    if (! b.contains (local) || layout.pages.isEmpty())
        return -1;

    return juce::jlimit (0, count - 1, (int) ((local.x - b.getX()) / (b.getWidth() / (float) count)));
}

void PluginCanvas::addPage (bool withTabs)
{
    if (layout.pages.isEmpty())
    {
        // The window becomes its first page: its controls, displays and frames lie on "Main";
        // the background, words, pictures, shapes and preset manager stay on every page.
        layout.pages.add ("Main");

        for (auto& w : layout.widgets)
            if (w.page.isEmpty() && (w.isControl() || w.isLive() || w.type == GuiWidget::Type::envelope || w.type == GuiWidget::Type::filter
                                     || w.type == GuiWidget::Type::xy || w.type == GuiWidget::Type::keyboard || w.type == GuiWidget::Type::custom
                                     || w.type == GuiWidget::Type::group))
                w.page = "Main";
    }

    juce::String name;

    for (int n = layout.pages.size() + 1; name.isEmpty() || layout.pages.contains (name); ++n)
        name = "Page " + juce::String (n);

    layout.pages.add (name);
    shownPage = layout.pages.size() - 1;

    bool hasTabs = false;

    for (const auto& w : layout.widgets)
        hasTabs = hasTabs || w.type == GuiWidget::Type::tabs;

    if (withTabs && ! hasTabs)
    {
        // Tabs to switch them, at the top right (move them anywhere).
        GuiWidget tabs;
        tabs.type = GuiWidget::Type::tabs;
        const auto width = juce::jlimit (120, layout.width - 20, 110 * layout.pages.size());
        tabs.bounds = { juce::jmax (0, layout.width - width - 16), 12, width, 30 };
        layout.widgets.push_back (tabs);
    }

    select (-1);
    edited();
    repaint();
}

void PluginCanvas::renamePage (int index, const juce::String& newName)
{
    const auto name = newName.trim();

    if (! juce::isPositiveAndBelow (index, layout.pages.size()) || name.isEmpty() || layout.pages.contains (name))
        return;

    const auto old = layout.pages[index];
    layout.pages.set (index, name);

    for (auto& w : layout.widgets)
        if (w.page == old)
            w.page = name;

    edited();
    repaint();
}

void PluginCanvas::removePage (int index)
{
    if (! juce::isPositiveAndBelow (index, layout.pages.size()))
        return;

    const auto old = layout.pages[index];
    layout.pages.remove (index);

    // With one page left there are no pages: the window is one again.
    if (layout.pages.size() < 2)
        layout.pages.clear();

    const auto heir = layout.pages.isEmpty() ? juce::String() : layout.pages[juce::jmax (0, index - 1)];

    for (auto& w : layout.widgets)
        if (w.page == old || (layout.pages.isEmpty() && w.page.isNotEmpty()))
            w.page = heir;

    // No pages: no tabs.
    if (layout.pages.isEmpty())
    {
        for (int i = (int) layout.widgets.size(); --i >= 0;)
            if (layout.widgets[(size_t) i].type == GuiWidget::Type::tabs)
                layout.widgets.erase (layout.widgets.begin() + i);

        indexesChanged();
    }

    shownPage = juce::jlimit (0, juce::jmax (0, layout.pages.size() - 1), index - 1);
    select (-1);
    edited();
    repaint();
}

void PluginCanvas::movePage (int index, int delta)
{
    const auto target = index + delta;

    if (! juce::isPositiveAndBelow (index, layout.pages.size()) || ! juce::isPositiveAndBelow (target, layout.pages.size()))
        return;

    layout.pages.move (index, target);

    if (shownPage == index)
        shownPage = target;
    else if (shownPage == target)
        shownPage = index;

    edited();
    repaint();
}

void PluginCanvas::setPages (const juce::StringArray& wanted)
{
    juce::StringArray names;

    for (const auto& n : wanted)
        if (const auto name = n.trim(); name.isNotEmpty())
            names.addIfNotAlreadyThere (name);

    if (names == layout.pages)
        return;

    // The window's menu, whose Pages box this is, stays open (adding and removing pages closes it).
    const bool windowMenu = menuOpen && panelSelected;

    if (names.size() < 2)
    {
        // One page or none: the window is one page again.
        while (! layout.pages.isEmpty())
            removePage (layout.pages.size() - 1);
    }
    else
    {
        if (layout.pages.isEmpty())
            addPage (true);   // "Main" and its elements, and the tabs

        // By name: a page whose name is still there is the same page. One whose name went is
        // renamed to a new name in its place (after the same page that stays), or else deleted
        // (its elements go where removePage sends them); new names left over are new pages.
        const auto old = layout.pages;

        auto gapOf = [] (const juce::StringArray& list, int index, const juce::StringArray& other)
        {
            for (int i = index; --i >= 0;)
                if (other.contains (list[i]))
                    return list[i];   // the nearest page before it that stays

            return juce::String();
        };

        juce::StringArray fresh, gone;

        for (int j = 0; j < names.size(); ++j)
            if (! old.contains (names[j]))
                fresh.add (names[j]);

        for (int i = 0; i < old.size(); ++i)
        {
            if (names.contains (old[i]))
                continue;

            juce::String newName;

            for (int j = 0; j < names.size() && newName.isEmpty(); ++j)
                if (fresh.contains (names[j]) && gapOf (names, j, old) == gapOf (old, i, names))
                    newName = names[j];

            if (newName.isNotEmpty())
            {
                renamePage (layout.pages.indexOf (old[i]), newName);
                fresh.removeString (newName);
            }
            else
            {
                gone.add (old[i]);
            }
        }

        // The new pages first, so there are always two or more while the others go.
        layout.pages.addArray (fresh);

        for (const auto& name : gone)
            removePage (layout.pages.indexOf (name));

        // In the box's order, the page shown staying shown.
        const auto shownName = shownPageName();
        layout.pages = names;
        shownPage = juce::jmax (0, layout.pages.indexOf (shownName));
        edited();
        repaint();
    }

    if (windowMenu)
    {
        selectPanel();
        openMenu();
    }
}

void PluginCanvas::askToRenamePage (int index)
{
    if (! juce::isPositiveAndBelow (index, layout.pages.size()))
        return;

    auto* window = new juce::AlertWindow ("Rename the page", {}, juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor ("name", layout.pages[index]);
    window->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    window->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis = juce::Component::SafePointer<PluginCanvas> (this), window, index] (int result)
    {
        if (safeThis != nullptr && result == 1)
            safeThis->renamePage (index, window->getTextEditorContents ("name"));
    }), true);
}

std::vector<std::pair<int, juce::Rectangle<float>>> PluginCanvas::pageStrip() const
{
    std::vector<std::pair<int, juce::Rectangle<float>>> items;

    if (! design || panelOnly || ! hasPanel())
        return items;

    // Right of the "EDIT  760 x 420" caption, above the window.
    const auto area = panelArea();
    auto x = area.getX() + 150.0f;
    const auto y = area.getY() - 24.0f;
    const auto font = Theme::font (12.0f, true);

    for (int i = 0; i < layout.pages.size(); ++i)
    {
        const auto width = (float) juce::GlyphArrangement::getStringWidthInt (font, layout.pages[i]) + 20.0f;
        items.push_back ({ i, { x, y, width, 20.0f } });
        x += width + 4.0f;
    }

    items.push_back ({ -1, { x, y, layout.pages.isEmpty() ? 64.0f : 22.0f, 20.0f } });
    return items;
}

void PluginCanvas::drawPageStrip (juce::Graphics& g)
{
    for (const auto& [item, box] : pageStrip())
    {
        const bool shown = item == juce::jlimit (0, juce::jmax (0, layout.pages.size() - 1), shownPage) && item >= 0;
        g.setColour (shown ? Theme::accent : Theme::raised);
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (shown ? Theme::accent.brighter (0.3f) : Theme::outline.brighter (0.2f));
        g.drawRoundedRectangle (box.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (shown ? juce::Colours::white : Theme::text);
        g.setFont (Theme::font (12.0f, true));
        g.drawText (item >= 0 ? layout.pages[item] : (layout.pages.isEmpty() ? juce::String ("+ Pages") : juce::String ("+")),
                    box, juce::Justification::centred, false);
    }
}

void PluginCanvas::pageStripClicked (int item, const juce::MouseEvent& e)
{
    if (item < 0)
    {
        addPage (true);
        return;
    }

    showPage (item);

    if (! e.mods.isPopupMenu())
        return;

    juce::PopupMenu choices;
    choices.addItem (1, "Rename...");
    choices.addItem (2, "Move left", item > 0);
    choices.addItem (3, "Move right", item < layout.pages.size() - 1);
    choices.addSeparator();
    choices.addItem (4, "Delete the page (its elements go to the page before)");

    choices.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                        [safeThis = juce::Component::SafePointer<PluginCanvas> (this), item] (int choice)
                        {
                            if (safeThis == nullptr)
                                return;

                            if (choice == 1)      safeThis->askToRenamePage (item);
                            else if (choice == 2) safeThis->movePage (item, -1);
                            else if (choice == 3) safeThis->movePage (item, 1);
                            else if (choice == 4) safeThis->removePage (item);
                        });
}

const PluginCanvas::Param* PluginCanvas::paramById (const juce::String& id) const
{
    if (id.isEmpty())
        return nullptr;

    for (const auto& p : params)
        if (p.id == id)
            return &p;

    return nullptr;
}

float PluginCanvas::proportionOf (const Param& p, float value) const
{
    const auto t = juce::jlimit (0.0f, 1.0f, (value - p.min) / juce::jmax (1.0e-9f, p.max - p.min));
    return std::abs (p.skew - 1.0f) > 1.0e-4f && p.skew > 0.0f ? std::pow (t, p.skew) : t;
}

float PluginCanvas::valueAt (const Param& p, float proportion) const
{
    auto t = juce::jlimit (0.0f, 1.0f, proportion);

    if (std::abs (p.skew - 1.0f) > 1.0e-4f && p.skew > 0.0f)
        t = std::pow (t, 1.0f / p.skew);

    return p.min + (p.max - p.min) * t;
}

float PluginCanvas::currentValue (const Param& p) const
{
    const auto found = values.find (p.index);
    return found != values.end() ? found->second : p.def;
}

float PluginCanvas::roleProportion (const GuiWidget& w, const juce::String& role, float fallback) const
{
    const auto found = w.roles.find (role);

    if (found == w.roles.end())
        return fallback;

    const auto* p = paramById (found->second);
    return p != nullptr ? proportionOf (*p, currentValue (*p)) : fallback;
}

void PluginCanvas::setValue (const Param& p, float value)
{
    value = juce::jlimit (p.min, p.max, value);
    values[p.index] = value;

    if (onParameterChanged != nullptr)
        onParameterChanged (p.index, value);

    repaintParameters ({ p.index });   // what shows it, not the whole panel
}

void PluginCanvas::setXy (const GuiWidget& w, juce::Point<float> plugin)
{
    const auto pad = w.bounds.toFloat().withTrimmedBottom ((float) GuiLayout::captionHeight).reduced (2.0f);

    if (const auto found = w.roles.find ("x"); found != w.roles.end())
        if (const auto* p = paramById (found->second))
            setValue (*p, valueAt (*p, (plugin.x - pad.getX()) / pad.getWidth()));

    if (const auto found = w.roles.find ("y"); found != w.roles.end())
        if (const auto* p = paramById (found->second))
            setValue (*p, valueAt (*p, (pad.getBottom() - plugin.y) / pad.getHeight()));
}

void PluginCanvas::drawLook (juce::Graphics& g, const GuiWidget& w, juce::Rectangle<int> area, float value)
{
    if (looks == nullptr || area.isEmpty())
        return;

    looks->setLight (layout.lightAngle, layout.lightAltitude);   // lit by the panel's global light
    const auto kind = Looks::kindOf (w);
    const auto& look = looks->lookFor (w);

    // Zoomed in, it's drawn with more pixels so it stays sharp; Export bakes plugin pixels.
    const auto resolution = baking ? 1.0f
                                   : (g.getInternalContext().getPhysicalPixelScaleFactor() > 1.3f ? 2.0f : 1.0f);

    // Its shadows and glows fall on the panel around its box, never cut at the box's edge.
    const auto room = Looks::overhang (look, area.getWidth(), area.getHeight());
    g.drawImage (looks->frame (look, kind, area.getWidth(), area.getHeight(), value, resolution), area.expanded (room).toFloat());
}

juce::Rectangle<int> PluginCanvas::knobSquare (const GuiWidget& w) const
{
    const auto size = juce::jmax (8, juce::jmin (w.bounds.getWidth(), w.bounds.getHeight() - GuiLayout::captionHeight));
    return { w.bounds.getX() + (w.bounds.getWidth() - size) / 2, w.bounds.getY(), size, size };
}

//==============================================================================
void PluginCanvas::timerCallback()
{
    if (! isShowing())
        return;

    // Each source is read once per frame: a peak is "the highest since the last read".
    frameLevels.clear();

    // Programmed elements that look different now are drawn again.
    refreshElements (false);

    // Keyboards light up for the notes that sound: the mouse's, a MIDI keyboard's, the host's.
    if (isNoteDown != nullptr)
    {
        std::bitset<128> down;

        for (const auto& w : layout.widgets)
            if (w.type == GuiWidget::Type::keyboard)
                for (int note = juce::jmax (0, w.lowNote); note <= juce::jmin (127, w.highNote); ++note)
                    if (isNoteDown (note))
                        down.set ((size_t) note);

        if (down != notesShown)
        {
            notesShown = down;

            for (const auto& w : layout.widgets)
                if (w.type == GuiWidget::Type::keyboard)
                    repaint (viewAreaOf (w));
        }
    }

    bool live = false;

    for (const auto& w : layout.widgets)
        live = live || w.isLive() || (w.type == GuiWidget::Type::filter && w.source.isNotEmpty());

    if (! live || readLevel == nullptr)
        return;

    for (int i = 0; i < (int) layout.widgets.size(); ++i)
    {
        const auto& w = layout.widgets[(size_t) i];

        if (w.type != GuiWidget::Type::meter && w.type != GuiWidget::Type::lamp)
            continue;

        const bool rms = w.type == GuiWidget::Type::meter && w.mode == "rms";
        const auto level = frameLevel (w.source, rms);
        auto& shown = meterLevels[i];

        // Meters (and lamps following the level) on a signal fall back smoothly; displays and
        // other lamps show the value as it is.
        const bool falls = w.type == GuiWidget::Type::meter || PanelParts::lampFollows (w);
        shown = (falls && ! displaySources.contains (w.source)) ? juce::jmax (level, shown * 0.86f) : level;

        if (w.type != GuiWidget::Type::meter)
            continue;

        // A VU's needle swings after the level, as a real one does; the peak is held a
        // moment, then falls.
        const auto fraction = MeterPaint::fractionOf (shown, displaySources.contains (w.source));
        auto& needle = meterNeedles[i];
        needle += (fraction - needle) * 0.22f;

        auto& [peak, holdUntil] = meterPeaks[i];
        const auto now = juce::Time::getMillisecondCounterHiRes();

        if (fraction >= peak)
        {
            peak = fraction;
            holdUntil = now + 1500.0;
        }
        else if (now > holdUntil)
        {
            peak = juce::jmax (fraction, peak - 0.015f);
        }
    }

    // Only what moves is drawn again: the rest of the window (a big background picture,
    // say) stays as it is. A filter following the live cutoff moves too.
    for (const auto& w : layout.widgets)
        if (w.isLive() || (w.type == GuiWidget::Type::filter && w.source.isNotEmpty()))
            repaint (viewAreaOf (w));
}

void PluginCanvas::drawLive (juce::Graphics& g, const GuiWidget& w, int index, Part part, float override)
{
    const auto b = w.bounds.toFloat();
    const auto accent = w.colour.isTransparent() ? defaultAccent : w.colour;
    const auto found = meterLevels.find (index);
    const auto level = found != meterLevels.end() ? found->second : 0.0f;
    const bool isDisplay = displaySources.contains (w.source);
    const bool still = part != Part::moving, moving = part != Part::still;

    auto caption = [&] (juce::Rectangle<float> area)
    {
        if (! still)
            return;

        g.setColour (captionColour.withAlpha (0.85f));
        g.setFont (FontLibrary::font (w.font, 12.5f));
        g.drawText (w.label, area, juce::Justification::centred, true);
    };

    if (w.type == GuiWidget::Type::meter)
    {
        // How it shows the level (MeterPaint): signals in dB (-60..+6), displays as they are.
        const auto area = b.withTrimmedBottom (w.label.isNotEmpty() ? (float) GuiLayout::captionHeight : 0.0f);
        const auto style = MeterPaint::styleOf (w);

        if (still)
            MeterPaint::paintStill (g, area, style, isDisplay);

        if (moving)
        {
            auto fraction = MeterPaint::fractionOf (level, isDisplay);
            const auto needle = meterNeedles.find (index);
            const auto held = meterPeaks.find (index);
            const bool needled = MeterPaint::hasNeedle (style);
            auto shown = needled && needle != meterNeedles.end() ? needle->second : fraction;
            auto peak = held != meterPeaks.end() ? held->second.first : -1.0f;

            // Silent in Edit: shown part-way up, so its colours and zones can be seen.
            if (design && ! baking && fraction <= 0.0001f && (needle == meterNeedles.end() || needle->second <= 0.001f))
            {
                fraction = shown = isDisplay ? 0.8f : (needled ? 44.0f : 52.0f) / 66.0f;   // a needle near 0 VU
                peak = isDisplay ? 0.93f : 58.5f / 66.0f;
            }

            MeterPaint::paintLevel (g, area, style, shown, peak, isDisplay);
        }

        if (w.label.isNotEmpty())
            caption (b.withTop (b.getBottom() - (float) GuiLayout::captionHeight).expanded (14.0f, 0.0f));

        return;
    }

    if (w.type == GuiWidget::Type::lamp)
    {
        const auto area = b.withTrimmedBottom (w.label.isNotEmpty() ? (float) GuiLayout::captionHeight : 0.0f);

        if (moving)
        {
            // On from its threshold, or following the level (a signal's in dB, as a meter shows it).
            float lit = 0.0f;

            if (override >= 0.0f)
                lit = juce::jlimit (0.0f, 1.0f, override);
            else if (PanelParts::lampFollows (w))
                lit = design && ! baking && level <= 0.0001f ? 0.7f : MeterPaint::fractionOf (level, isDisplay);   // silent in Edit: lit, to be seen
            else
                lit = level >= w.threshold ? 1.0f : 0.0f;

            PanelParts::paintLamp (g, area, w, lit);
        }

        if (w.label.isNotEmpty())
            caption (b.withTop (b.getBottom() - (float) GuiLayout::captionHeight).expanded (14.0f, 0.0f));

        return;
    }

    // Scope: the newest samples, steadied on a rising zero crossing.
    auto area = b.withTrimmedBottom (w.label.isNotEmpty() ? (float) GuiLayout::captionHeight : 0.0f);

    if (still)
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (area, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawHorizontalLine (juce::roundToInt (area.getCentreY()), area.getX() + 2.0f, area.getRight() - 2.0f);
    }

    if (moving && readScope != nullptr && w.source.isNotEmpty())
    {
        readScope (w.source, scopeBuffer.data(), scopeSamples);

        constexpr int shown = 1024;
        int start = 0;

        for (int i = 1; i < scopeSamples - shown; ++i)
        {
            if (scopeBuffer[(size_t) i - 1] < 0.0f && scopeBuffer[(size_t) i] >= 0.0f)
            {
                start = i;
                break;
            }
        }

        juce::Path trace;
        const auto inner = area.reduced (3.0f);

        for (int i = 0; i < shown; i += 2)
        {
            const auto x = inner.getX() + inner.getWidth() * (float) i / (float) (shown - 1);
            const auto y = inner.getCentreY() - juce::jlimit (-1.0f, 1.0f, scopeBuffer[(size_t) (start + i)]) * inner.getHeight() * 0.5f;

            if (i == 0) trace.startNewSubPath (x, y);
            else        trace.lineTo (x, y);
        }

        g.setColour (accent);
        g.strokePath (trace, juce::PathStrokeType (1.5f));
    }

    if (w.label.isNotEmpty())
        caption (b.withTop (b.getBottom() - (float) GuiLayout::captionHeight));
}

void PluginCanvas::drawCurve (juce::Graphics& g, const GuiWidget& w, Part part)
{
    const auto b = w.bounds.toFloat();
    const auto accent = w.colour.isTransparent() ? defaultAccent : w.colour;
    auto area = b.withTrimmedBottom (w.label.isNotEmpty() ? (float) GuiLayout::captionHeight : 0.0f);
    const bool still = part != Part::moving, moving = part != Part::still;

    if (still)
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (area, 4.0f);
    }

    const auto inner = area.reduced (6.0f);
    juce::Path curve;

    if (w.type == GuiWidget::Type::envelope && moving)
    {
        // Each stage's width follows its knob (as the knob shows it), so short and long
        // settings both read clearly. The sustain plateau keeps a fixed width.
        const auto a = roleProportion (w, "attack", 0.1f);
        const auto d = roleProportion (w, "decay", 0.3f);
        const auto s = roleProportion (w, "sustain", 0.7f);
        const auto r = roleProportion (w, "release", 0.4f);

        const float widths[] { 0.08f + 0.92f * a, 0.08f + 0.92f * d, 0.6f, 0.08f + 0.92f * r };
        const auto unit = inner.getWidth() / (widths[0] + widths[1] + widths[2] + widths[3]);
        const auto top = inner.getY(), bottom = inner.getBottom();
        const auto sustainY = bottom - (bottom - top) * s;

        juce::Point<float> points[] { { inner.getX(), bottom },
                                      { inner.getX() + widths[0] * unit, top },
                                      { inner.getX() + (widths[0] + widths[1]) * unit, sustainY },
                                      { inner.getX() + (widths[0] + widths[1] + widths[2]) * unit, sustainY },
                                      { inner.getRight(), bottom } };

        curve.startNewSubPath (points[0]);

        for (int i = 1; i < 5; ++i)
            curve.lineTo (points[i]);

        auto filled = curve;
        filled.closeSubPath();
        g.setColour (accent.withAlpha (0.22f));
        g.fillPath (filled);
        g.setColour (accent);
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        for (int i = 1; i < 4; ++i)
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (points[i]));
    }
    else if (w.type == GuiWidget::Type::filter)
    {
        // A filter's response over 20 Hz to 20 kHz (log), -30..+24 dB.
        const auto* cutoffParam = [&] () -> const Param* { const auto f = w.roles.find ("cutoff"); return f != w.roles.end() ? paramById (f->second) : nullptr; }();
        float cutoff = 1000.0f;

        if (cutoffParam != nullptr)
            cutoff = cutoffParam->unit.equalsIgnoreCase ("Hz") || cutoffParam->unit.equalsIgnoreCase ("kHz")
                         ? currentValue (*cutoffParam) * (cutoffParam->unit.equalsIgnoreCase ("kHz") ? 1000.0f : 1.0f)
                         : 20.0f * std::pow (1000.0f, proportionOf (*cutoffParam, currentValue (*cutoffParam)));

        cutoff = juce::jlimit (20.0f, 20000.0f, cutoff);

        const auto resonance = roleProportion (w, "resonance", 0.2f);
        const auto q = 0.707f + resonance * 9.0f;
        const auto steep = w.mode.endsWith ("24");
        const auto kind = w.mode.startsWith ("high") ? 1 : (w.mode.startsWith ("band") ? 2 : 0);

        if (still)
        {
            g.setColour (juce::Colours::white.withAlpha (0.08f));

            for (const auto f : { 100.0f, 1000.0f, 10000.0f })
                g.drawVerticalLine (juce::roundToInt (inner.getX() + inner.getWidth() * std::log10 (f / 20.0f) / 3.0f), inner.getY(), inner.getBottom());

            const auto zeroY = inner.getY() + inner.getHeight() * (24.0f / 54.0f);
            g.drawHorizontalLine (juce::roundToInt (zeroY), inner.getX(), inner.getRight());
        }

        // The response at a cutoff, as a path along the box.
        const auto responseAt = [&] (float hz)
        {
            juce::Path path;

            for (int i = 0; i <= 120; ++i)
            {
                const auto t = (float) i / 120.0f;
                const auto f = 20.0f * std::pow (1000.0f, t);
                const auto x = f / hz;
                const auto denominator = std::sqrt ((1.0f - x * x) * (1.0f - x * x) + (x / q) * (x / q));
                auto magnitude = kind == 1 ? (x * x) / denominator : (kind == 2 ? (x / q) / denominator : 1.0f / denominator);

                if (steep)
                    magnitude *= kind == 2 ? 1.0f : magnitude / juce::jmax (1.0f, q * 0.5f);

                const auto db = juce::jlimit (-30.0f, 24.0f, 20.0f * std::log10 (juce::jmax (magnitude, 1.0e-6f)));
                const auto px = inner.getX() + inner.getWidth() * t;
                const auto py = inner.getY() + inner.getHeight() * ((24.0f - db) / 54.0f);

                if (i == 0) path.startNewSubPath (px, py);
                else        path.lineTo (px, py);
            }

            return path;
        };

        // Following the plugin's live cutoff (a display: Hz, or 0..1 along 20 Hz to 20 kHz), it
        // opens and closes as it plays; the knob's own setting stays as a faint line.
        float live = 0.0f;

        if (moving && ! baking && w.source.isNotEmpty() && readLevel != nullptr)
        {
            const auto value = frameLevel (w.source, false);

            if (value > 1.5f)
                live = juce::jlimit (20.0f, 20000.0f, value);
            else if (value > 0.0f)
                live = 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, value));
        }

        if (moving)
        {
            curve = responseAt (live > 0.0f ? live : cutoff);

            auto filled = curve;
            filled.lineTo (inner.getRight(), inner.getBottom());
            filled.lineTo (inner.getX(), inner.getBottom());
            filled.closeSubPath();

            if (live > 0.0f)
            {
                // How far it's opened from the knob: the further, the brighter.
                const auto opened = juce::jlimit (0.0f, 1.0f, std::abs (std::log2 (live / cutoff)) / 3.0f);

                g.setColour (accent.withAlpha (0.35f));
                g.strokePath (responseAt (cutoff), juce::PathStrokeType (1.0f));

                g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.30f + 0.20f * opened), inner.getX(), inner.getY(),
                                                         accent.withAlpha (0.04f), inner.getX(), inner.getBottom(), false));
                g.fillPath (filled);

                const auto x = inner.getX() + inner.getWidth() * std::log10 (live / 20.0f) / 3.0f;
                g.setColour (accent.withAlpha (0.18f + 0.30f * opened));
                g.fillRect (juce::Rectangle<float> (x - 0.75f, inner.getY(), 1.5f, inner.getHeight()));

                g.setColour (accent.brighter (0.3f).withAlpha (0.20f + 0.25f * opened));
                g.strokePath (curve, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (accent.brighter (0.15f + 0.35f * opened));
                g.strokePath (curve, juce::PathStrokeType (2.0f));
            }
            else
            {
                g.setColour (accent.withAlpha (0.22f));
                g.fillPath (filled);
                g.setColour (accent);
                g.strokePath (curve, juce::PathStrokeType (2.0f));
            }
        }
    }

    if (w.label.isNotEmpty() && still)
    {
        g.setColour (captionColour.withAlpha (0.85f));
        g.setFont (FontLibrary::font (w.font, 12.5f));
        g.drawText (w.label, b.withTop (b.getBottom() - (float) GuiLayout::captionHeight), juce::Justification::centred, true);
    }
}

void PluginCanvas::drawWidget (juce::Graphics& g, int index, Part part, float override)
{
    const auto& w = layout.widgets[(size_t) index];
    const auto opacity = w.shownOpacity();

    // Another page's: not shown.
    if (! isShown (w))
        return;

    // Every element as an object: turned about its middle, as see-through as it's asked. A
    // shape turns itself (its light stays where the light is).
    juce::Graphics::ScopedSaveState state (g);

    if (w.isTurned() && w.type != GuiWidget::Type::shape)
        g.addTransform (w.turn());

    if (opacity >= 0.999f)
    {
        drawUpright (g, index, part, override);
        return;
    }

    if (opacity > 0.0f)
    {
        g.beginTransparencyLayer (opacity);
        drawUpright (g, index, part, override);
        g.endTransparencyLayer();
    }

    // Quite see-through in Edit: a dashed frame says where it is.
    if (design && ! baking && opacity < 0.2f && part != Part::moving)
    {
        const auto b = w.bounds.toFloat();
        const auto turn = w.type == GuiWidget::Type::shape ? w.turn() : juce::AffineTransform();   // a shape isn't turned yet here
        const juce::Point<float> corners[] { b.getTopLeft(), b.getTopRight(), b.getBottomRight(), b.getBottomLeft() };
        const float dashes[] { 4.0f, 4.0f };
        g.setColour (Theme::muted.withAlpha (0.7f));

        for (int i = 0; i < 4; ++i)
            g.drawDashedLine ({ corners[i].transformedBy (turn), corners[(i + 1) % 4].transformedBy (turn) }, dashes, 2, 1.0f);
    }
}

void PluginCanvas::drawShape (juce::Graphics& g, const GuiWidget& w, int index)
{
    const auto fallback = standardColour (w).withAlpha (0.9f);

    // A flat colour and its outline: a vector, sharp at any zoom.
    if (! ObjectPaint::isPainted (w))
    {
        shapePaintings.erase (index);

        if (! w.bounds.isEmpty())
            ObjectPaint::drawFlat (g, w, fallback);
        else if (design)
            g.drawRect (w.bounds.toFloat(), 1.0f);

        return;
    }

    // A material, patterns, relief, a shadow or a layer style: painted, and kept until it changes.
    // A layer style is painted here and now (its pictures are the renderer's own kind).
    const auto job = ObjectPaint::jobFor (w, fallback, layout.lightAngle, layout.lightAltitude);
    auto& entry = shapePaintings[index];

    if (entry.painting.key != job.key)
    {
        if (ObjectPaint::pixelsOf (job) <= shapePixelsNow || job.style.any() || baking || shapePainter == nullptr)
        {
            entry.painting = ObjectPaint::paint (job);
            entry.pending.clear();
        }
        else if (entry.pending != job.key)
        {
            entry.pending = job.key;

            shapePainter->addJob ([job, index, safeThis = juce::Component::SafePointer<PluginCanvas> (this)]
            {
                auto painting = ObjectPaint::paint (job);

                juce::MessageManager::callAsync ([safeThis, index, painting]
                {
                    if (safeThis == nullptr)
                        return;

                    if (auto found = safeThis->shapePaintings.find (index); found != safeThis->shapePaintings.end() && found->second.pending == painting.key)
                    {
                        found->second.painting = painting;
                        found->second.pending.clear();
                        safeThis->repaint();
                    }
                });
            });
        }
    }

    const auto& painting = entry.painting;
    const auto centre = w.bounds.toFloat().getCentre();

    if (! painting.image.isValid())
    {
        ObjectPaint::drawFlat (g, w, fallback);   // until its picture is ready
        return;
    }

    if (painting.key == job.key)
    {
        g.drawImageAt (painting.image, juce::roundToInt (centre.x + painting.offset.x), juce::roundToInt (centre.y + painting.offset.y));
        return;
    }

    // The one before, stretched and turned to where it is now, until the new one's ready.
    const float sx = (float) w.bounds.getWidth() / (float) juce::jmax (1, painting.width);
    const float sy = (float) w.bounds.getHeight() / (float) juce::jmax (1, painting.height);
    const auto standIn = juce::AffineTransform::translation (painting.offset)
                             .rotated (juce::degreesToRadians (-painting.angle))
                             .scaled (sx, sy)
                             .rotated (juce::degreesToRadians (w.shownAngle()))
                             .translated (centre);
    g.drawImageTransformed (painting.image, standIn);
}

void PluginCanvas::drawUpright (juce::Graphics& g, int index, Part part, float override)
{
    const auto& w = layout.widgets[(size_t) index];

    if (w.isLive())
    {
        drawLive (g, w, index, part, override);
        return;
    }

    if (w.type == GuiWidget::Type::envelope || w.type == GuiWidget::Type::filter)
    {
        drawCurve (g, w, part);
        return;
    }

    if (w.type == GuiWidget::Type::keyboard)
    {
        drawKeyboard (g, w, index, part);
        return;
    }

    // The registration badge: shown here as the plugin shows it until it's registered. Export
    // keeps it out of the panel's images (the plugin draws it, and hides it once registered).
    if (w.type == GuiWidget::Type::registration)
    {
        if (part == Part::all && ! (panelOnly && registered))
            drawRegistration (g, w);

        return;
    }

    // A programmed element draws itself, all of it moving (Export leaves it to the plugin).
    if (w.type == GuiWidget::Type::custom)
    {
        if (part != Part::still)
            drawElement (g, w, index);

        return;
    }

    const bool still = part != Part::moving, moving = part != Part::still;
    const auto* p = paramFor (w);
    const auto value = p != nullptr ? currentValue (*p) : 0.0f;
    const auto proportion = override >= 0.0f ? override : (p != nullptr ? proportionOf (*p, value) : 0.0f);
    const auto accent = w.colour.isTransparent() ? defaultAccent : w.colour;
    const bool missing = w.isControl() && p == nullptr;
    const auto b = w.bounds.toFloat();

    auto caption = [&] (juce::Rectangle<float> area)
    {
        if (! still)
            return;

        // The caption shows the value while it's being set or pointed at.
        const bool showValue = p != nullptr && (index == active || (index == hovered && ! design));
        juce::String text = w.label;

        if (showValue)
        {
            text = valueText (value, *p);
        }

        g.setColour (missing ? Theme::muted : captionColour.withAlpha (showValue ? 1.0f : 0.85f));
        g.setFont (FontLibrary::font (w.font, 12.5f, showValue));
        g.drawText (text, area, juce::Justification::centred, true);
    };

    switch (w.type)
    {
        case GuiWidget::Type::group:
        {
            if (! still)
                break;

            g.setColour (juce::Colours::white.withAlpha (0.13f));
            g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
            g.setColour (w.colour.isTransparent() ? captionColour.withAlpha (0.75f) : w.colour);
            g.setFont (FontLibrary::font (w.font, w.fontSize > 0.0f ? w.fontSize : 11.5f, true));
            g.drawText (w.label, b.reduced (10.0f, 4.0f).withHeight (16.0f), juce::Justification::centredLeft, true);
            break;
        }

        case GuiWidget::Type::label:
        {
            // Words are still; a readout shows its parameter's value, so it moves.
            if (PanelParts::labelMoves (w) ? ! moving : ! still)
                break;

            auto text = w.label;

            if (p != nullptr && PanelParts::labelDisplayOf (w) == "readout")
                text = valueText (value, *p);

            PanelParts::paintLabel (g, w, text, w.colour.isTransparent() ? captionColour : w.colour, w.fontSize);
            break;
        }

        case GuiWidget::Type::tabs:
        {
            // A tab per page, the one shown lit (it moves: the page changes).
            if (! moving)
                break;

            const auto names = layout.pages.isEmpty() ? juce::StringArray { "Page 1" } : layout.pages;
            const auto cellW = b.getWidth() / (float) names.size();
            const auto shown = juce::jlimit (0, names.size() - 1, shownPage);
            const auto size = w.fontSize > 0.0f ? w.fontSize : juce::jmin (13.5f, b.getHeight() * 0.48f);

            for (int i = 0; i < names.size(); ++i)
            {
                const auto cell = juce::Rectangle<float> (b.getX() + (float) i * cellW, b.getY(), cellW, b.getHeight()).reduced (1.5f, 1.0f);
                g.setColour (i == shown ? accent : juce::Colours::black.withAlpha (0.45f));
                g.fillRoundedRectangle (cell, 4.0f);
                g.setColour (i == shown ? juce::Colours::white.withAlpha (0.25f) : juce::Colours::white.withAlpha (0.08f));
                g.drawRoundedRectangle (cell.reduced (0.5f), 4.0f, 1.0f);
                g.setColour (i == shown ? juce::Colours::white : captionColour.withAlpha (0.8f));
                g.setFont (FontLibrary::font (w.font, size, i == shown || w.bold));
                g.drawFittedText (names[i], cell.reduced (6.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.75f);
            }
            break;
        }

        case GuiWidget::Type::preset:
        {
            // "<  Preset name  >": the whole widget moves (one frame per preset).
            if (! moving)
                break;

            const auto chosen = override >= 0.0f ? juce::roundToInt (override) : -1;
            const auto name = juce::isPositiveAndBelow (chosen, presetMenu.names.size()) ? presetMenu.names[chosen] : presetMenu.currentText();

            // Its corners: as standard, or rounded from square (0) to round ends (1).
            const auto corner = w.rounding >= 0.0f ? juce::jlimit (0.0f, 1.0f, w.rounding) * juce::jmin (b.getWidth(), b.getHeight()) * 0.5f : 4.0f;
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (b, corner);
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.drawRoundedRectangle (b.reduced (0.5f), juce::jmax (0.0f, corner - 0.5f), 1.0f);

            const auto arrow = juce::jmin (b.getHeight() * 0.32f, 8.0f);
            juce::Path left, right;
            left.addTriangle (b.getX() + 10.0f, b.getCentreY(), b.getX() + 10.0f + arrow, b.getCentreY() - arrow, b.getX() + 10.0f + arrow, b.getCentreY() + arrow);
            right.addTriangle (b.getRight() - 10.0f, b.getCentreY(), b.getRight() - 10.0f - arrow, b.getCentreY() - arrow, b.getRight() - 10.0f - arrow, b.getCentreY() + arrow);
            g.setColour (accent);
            g.fillPath (left);
            g.fillPath (right);

            g.setColour (captionColour);
            g.setFont (FontLibrary::font (w.font, juce::jmin (14.0f, b.getHeight() * 0.5f), true));
            g.drawFittedText (name, b.reduced (14.0f + arrow * 2.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.8f);
            break;
        }

        case GuiWidget::Type::shape:
        {
            if (still)
                drawShape (g, w, index);
            break;
        }

        case GuiWidget::Type::xy:
        {
            const auto pad = b.withTrimmedBottom ((float) GuiLayout::captionHeight);
            const auto x = roleProportion (w, "x", 0.5f), y = roleProportion (w, "y", 0.5f);
            const auto dot = juce::Point<float> (pad.getX() + 2.0f + (pad.getWidth() - 4.0f) * x, pad.getBottom() - 2.0f - (pad.getHeight() - 4.0f) * y);

            if (still)
            {
                g.setColour (juce::Colours::black.withAlpha (0.55f));
                g.fillRoundedRectangle (pad, 4.0f);
                g.setColour (juce::Colours::white.withAlpha (0.07f));

                for (int i = 1; i < 4; ++i)
                {
                    g.drawVerticalLine (juce::roundToInt (pad.getX() + pad.getWidth() * (float) i / 4.0f), pad.getY(), pad.getBottom());
                    g.drawHorizontalLine (juce::roundToInt (pad.getY() + pad.getHeight() * (float) i / 4.0f), pad.getX(), pad.getRight());
                }

                g.setColour (captionColour.withAlpha (0.85f));
                g.setFont (FontLibrary::font (w.font, 12.5f));
                g.drawText (w.label, b.withTop (pad.getBottom()), juce::Justification::centred, true);
            }

            if (moving)
            {
                g.setColour (accent.withAlpha (0.4f));
                g.drawVerticalLine (juce::roundToInt (dot.x), pad.getY() + 1.0f, pad.getBottom() - 1.0f);
                g.drawHorizontalLine (juce::roundToInt (dot.y), pad.getX() + 1.0f, pad.getRight() - 1.0f);

                g.setColour (accent.withAlpha (0.35f));
                g.fillEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre (dot));
                g.setColour (accent);
                g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (dot));
            }
            break;
        }

        case GuiWidget::Type::knob:
        case GuiWidget::Type::slider:
        case GuiWidget::Type::toggle:
        {
            // Its look, at the frame for its value (a switch: off or on). Unbound, it's faded.
            if (moving)
            {
                const auto position = w.type != GuiWidget::Type::toggle ? proportion
                                    : (override >= 0.0f ? (override >= 0.5f ? 1.0f : 0.0f) : (p != nullptr && proportion >= 0.5f ? 1.0f : 0.0f));

                juce::Graphics::ScopedSaveState state (g);

                if (missing && ! baking)
                    g.setOpacity (0.45f);

                drawLook (g, w, movingAreaOf (w), position);
            }

            // The caption may be wider than the control, so names like "Resonance" fit.
            caption (b.withTop (b.getBottom() - (float) GuiLayout::captionHeight).expanded (14.0f, 0.0f));
            break;
        }

        case GuiWidget::Type::selector:
        {
            const auto area = b.withTrimmedBottom ((float) GuiLayout::captionHeight);
            const auto count = juce::jmax (1, w.options.size() > 0 ? w.options.size() : (p != nullptr ? (int) (p->max - p->min) + 1 : 1));
            const auto chosen = override >= 0.0f ? juce::roundToInt (override) : (p != nullptr ? juce::roundToInt (value - p->min) : -1);

            if (! moving)
            {
                caption (b.withTop (b.getBottom() - (float) GuiLayout::captionHeight));
                break;
            }

            // Its positions, as its display shows them (a row of buttons, a pill, keys...).
            juce::StringArray names;

            for (int i = 0; i < count; ++i)
                names.add (i < w.options.size() ? w.options[i] : juce::String (i + 1));

            PanelParts::paintSelector (g, area, w, names, chosen, accent, captionColour);
            caption (b.withTop (b.getBottom() - (float) GuiLayout::captionHeight));
            break;
        }

        case GuiWidget::Type::image:
        {
            if (! still)
                break;

            if (picture (w.image).isValid())
            {
                drawPicture (g, w.image, b, w.mode.isNotEmpty() ? w.mode : juce::String ("fit"));
            }
            else if (design && ! baking)
            {
                // No picture yet: a dashed frame that says how to get one.
                g.setColour (juce::Colours::black.withAlpha (0.25f));
                g.fillRect (b);
                g.setColour (captionColour.withAlpha (0.6f));
                const float dashes[] { 5.0f, 4.0f };

                for (const auto& edge : { juce::Line<float> (b.getTopLeft(), b.getTopRight()), juce::Line<float> (b.getTopRight(), b.getBottomRight()),
                                          juce::Line<float> (b.getBottomRight(), b.getBottomLeft()), juce::Line<float> (b.getBottomLeft(), b.getTopLeft()) })
                    g.drawDashedLine (edge, dashes, 2, 1.0f);

                g.setFont (Theme::font (12.5f));
                g.drawFittedText (w.image.isNotEmpty() ? "Picture missing: " + w.image : juce::String ("Double-click to choose a picture"),
                                  b.reduced (6.0f).toNearestInt(), juce::Justification::centred, 3, 0.9f);
            }
            break;
        }

        case GuiWidget::Type::meter:
        case GuiWidget::Type::scope:
        case GuiWidget::Type::lamp:
        case GuiWidget::Type::envelope:
        case GuiWidget::Type::filter:
        case GuiWidget::Type::keyboard:
        case GuiWidget::Type::custom:
        case GuiWidget::Type::registration:
            break;   // drawn above
    }
}

void PluginCanvas::drawKeyboardAtRest (juce::Graphics& g, const GuiWidget& w)
{
    // Its keys as its display has them (classic ivory, reverse colours, modern slate).
    PanelParts::paintKeyboard (g, w);
}

void PluginCanvas::drawKeyboard (juce::Graphics& g, const GuiWidget& w, int index, Part part)
{
    if (part != Part::moving)
        drawKeyboardAtRest (g, w);

    // The keys that are down (never baked: the plugin draws them live, the same way).
    if (part == Part::still || baking)
        return;

    const auto accent = w.colour.isTransparent() ? defaultAccent : w.colour;

    PanelParts::paintKeysDown (g, w, [this, index] (int note)
    {
        return (note == heldNote && index == heldKeyboard) || (note >= 0 && note < 128 && notesShown[(size_t) note]);
    }, accent);
}

void PluginCanvas::playKey (int widgetIndex, juce::Point<float> plugin)
{
    if (! juce::isPositiveAndBelow (widgetIndex, (int) layout.widgets.size()))
        return;

    const auto& w = layout.widgets[(size_t) widgetIndex];
    const auto b = w.bounds.toFloat();
    const auto note = stella::keys::noteAt (plugin.x, plugin.y, w.lowNote, w.highNote, b.getX(), b.getY(), b.getWidth(), b.getHeight());

    if (note == heldNote && widgetIndex == heldKeyboard)
        return;

    releaseKey();

    if (note < 0)
        return;

    heldNote = note;
    heldKeyboard = widgetIndex;

    if (onNote != nullptr)
        onNote (note, stella::keys::velocityAt (plugin.y, stella::keys::keyOf (note, w.lowNote, w.highNote, b.getX(), b.getY(), b.getWidth(), b.getHeight())));

    repaint (viewAreaOf (w));
}

void PluginCanvas::releaseKey()
{
    if (heldNote < 0)
        return;

    const auto note = heldNote;
    const auto keyboard = heldKeyboard;
    heldNote = heldKeyboard = -1;

    if (onNote != nullptr)
        onNote (note, 0.0f);

    if (juce::isPositiveAndBelow (keyboard, (int) layout.widgets.size()))
        repaint (viewAreaOf (layout.widgets[(size_t) keyboard]));
}

juce::Rectangle<int> PluginCanvas::movingAreaOf (const GuiWidget& w) const
{
    switch (w.type)
    {
        case GuiWidget::Type::knob:      return knobSquare (w);
        case GuiWidget::Type::slider:
        case GuiWidget::Type::toggle:
        case GuiWidget::Type::selector:  return w.bounds.withTrimmedBottom (GuiLayout::captionHeight);
        case GuiWidget::Type::lamp:
        case GuiWidget::Type::meter:
        case GuiWidget::Type::scope:
        case GuiWidget::Type::envelope:
        case GuiWidget::Type::filter:    return w.bounds.withTrimmedBottom (w.label.isNotEmpty() ? GuiLayout::captionHeight : 0);
        case GuiWidget::Type::xy:        return w.bounds.withTrimmedBottom (GuiLayout::captionHeight);
        case GuiWidget::Type::preset:
        case GuiWidget::Type::keyboard:
        case GuiWidget::Type::custom:    return w.bounds;
        case GuiWidget::Type::label:     return PanelParts::labelMoves (w) ? w.bounds : juce::Rectangle<int>();   // a readout's value
        case GuiWidget::Type::group:
        case GuiWidget::Type::shape:
        case GuiWidget::Type::image:
        case GuiWidget::Type::registration:
        case GuiWidget::Type::tabs:      break;   // the plugin draws it itself
    }

    return {};
}

void PluginCanvas::setRegistered (bool isRegistered)
{
    if (registered != isRegistered)
    {
        registered = isRegistered;
        repaint();
    }
}

juce::String PluginCanvas::registrationText()
{
    return juce::String::fromUTF8 ("UNREGISTERED \xc2\xb7 click to register");
}

void PluginCanvas::drawRegistration (juce::Graphics& g, const GuiWidget& w)
{
    // A dark pill with an outline and words in the badge's colour (amber unless chosen).
    const auto box = w.bounds.toFloat().reduced (0.5f);
    const auto colour = w.colour.isTransparent() ? standardColour (w) : w.colour;
    const auto radius = juce::jmin (box.getHeight() * 0.5f, 14.0f);

    g.setColour (juce::Colour (0xe8141418));
    g.fillRoundedRectangle (box, radius);
    g.setColour (colour.withAlpha (0.85f));
    g.drawRoundedRectangle (box, radius, 1.0f);
    g.setColour (colour.brighter (0.15f));
    g.setFont (Theme::font (juce::jlimit (9.0f, 18.0f, box.getHeight() * 0.48f), true));
    g.drawFittedText (w.label.isNotEmpty() ? w.label : registrationText(), w.bounds.reduced (juce::roundToInt (radius * 0.6f), 0),
                      juce::Justification::centred, 1, 0.8f);
}

void PluginCanvas::drawPanel (juce::Graphics& g)
{
    const auto plugin = juce::Rectangle<float> (0.0f, 0.0f, (float) layout.width, (float) layout.height);
    g.setGradientFill (juce::ColourGradient (layout.backgroundTop, 0.0f, 0.0f, layout.backgroundBottom, 0.0f, plugin.getBottom(), false));
    g.fillRect (plugin);

    if (layout.backgroundImage.isNotEmpty())
        drawPicture (g, layout.backgroundImage, plugin, layout.backgroundMode);
}

void PluginCanvas::drawPicture (juce::Graphics& g, const juce::String& name, juce::Rectangle<float> area, const juce::String& mode)
{
    const auto& image = picture (name);

    if (! image.isValid() || area.isEmpty())
        return;

    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (area.getSmallestIntegerContainer());
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.setOpacity (1.0f);

    if (mode == "tile")
    {
        g.setTiledImageFill (image, juce::roundToInt (area.getX()), juce::roundToInt (area.getY()), 1.0f);
        g.fillRect (area);
        return;
    }

    const auto placement = mode == "stretch" ? juce::RectanglePlacement (juce::RectanglePlacement::stretchToFit)
                         : mode == "fit"     ? juce::RectanglePlacement (juce::RectanglePlacement::centred)
                         : mode == "fitLeft" ? juce::RectanglePlacement (juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid)
                         : mode == "centre"  ? juce::RectanglePlacement (juce::RectanglePlacement::centred | juce::RectanglePlacement::doNotResize)
                                             : juce::RectanglePlacement (juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
    g.drawImage (image, area, placement);
}

juce::Image PluginCanvas::snapshot (float scale)
{
    if (! hasPanel() || layout.width <= 0 || layout.height <= 0)
        return {};

    const juce::ScopedValueSetter<int> noHover (hovered, -1), noActive (active, -1);
    const juce::ScopedValueSetter<bool> asPlayed (design, false);

    scale = juce::jlimit (0.25f, 3.0f, scale);
    juce::Image image (juce::Image::ARGB, juce::jmax (1, juce::roundToInt ((float) layout.width * scale)),
                       juce::jmax (1, juce::roundToInt ((float) layout.height * scale)), true);

    {
        juce::Graphics g (image);
        g.addTransform (juce::AffineTransform::scale (scale));
        drawPanel (g);

        for (int i = 0; i < (int) layout.widgets.size(); ++i)
            drawWidget (g, i);
    }

    return image;
}

PluginCanvas::Bake PluginCanvas::bake()
{
    const juce::ScopedValueSetter<int> noHover (hovered, -1), noActive (active, -1);
    const juce::ScopedValueSetter<bool> bakingNow (baking, true);

    Bake result;
    result.background = juce::Image (juce::Image::ARGB, layout.width, layout.height, true);

    // Still things lying over something that moves (a frame over a meter, glass over a knob,
    // a caption on that glass) stay in front of it in the plugin too: layers of their own,
    // not part of the background.
    std::vector<bool> layered (layout.widgets.size(), false);

    for (size_t i = 0; i < layout.widgets.size(); ++i)
    {
        const auto& w = layout.widgets[i];
        const bool still = w.type == GuiWidget::Type::label || w.type == GuiWidget::Type::group || w.type == GuiWidget::Type::shape
                        || (w.type == GuiWidget::Type::image && w.image.isNotEmpty());

        for (size_t below = 0; still && below < i && ! layered[i]; ++below)
            layered[i] = movingAreaOf (layout.widgets[below]).intersects (w.bounds)
                      || (layered[below] && layout.widgets[below].bounds.intersects (w.bounds));
    }

    {
        juce::Graphics g (result.background);
        drawPanel (g);

        for (int i = 0; i < (int) layout.widgets.size(); ++i)
            if (! layered[(size_t) i])
                drawWidget (g, i, Part::still);
    }

    // The registration badge (the first one), on its own: a protected plugin draws it until
    // it's registered.
    for (const auto& w : layout.widgets)
    {
        if (w.type != GuiWidget::Type::registration)
            continue;

        result.registrationArea = w.bounds.getIntersection ({ 0, 0, layout.width, layout.height });

        if (! result.registrationArea.isEmpty())
        {
            result.registration = juce::Image (juce::Image::ARGB, result.registrationArea.getWidth(), result.registrationArea.getHeight(), true);
            juce::Graphics g (result.registration);
            g.setOrigin ({ -result.registrationArea.getX(), -result.registrationArea.getY() });
            drawRegistration (g, w);
        }

        break;
    }

    std::map<juce::String, int> lookStrips;   // controls with the same look and size share frames

    for (int i = 0; i < (int) layout.widgets.size(); ++i)
    {
        const auto& w = layout.widgets[(size_t) i];
        const auto area = layered[(size_t) i] ? w.bounds.getIntersection ({ 0, 0, layout.width, layout.height }) : movingAreaOf (w);
        result.stripOf.push_back (-1);
        result.movingArea.push_back (area);

        if (area.isEmpty())
            continue;

        // A layer in front of something that moves: one frame, drawn over it by the plugin.
        if (layered[(size_t) i])
        {
            Bake::Strip strip;
            strip.frameWidth = area.getWidth();
            strip.frameHeight = area.getHeight();
            strip.frames = 1;
            strip.image = juce::Image (juce::Image::ARGB, area.getWidth(), area.getHeight(), true);

            {
                juce::Graphics g (strip.image);
                g.setOrigin ({ -area.getX(), -area.getY() });
                drawWidget (g, i, Part::still);
            }

            result.stripOf.back() = (int) result.strips.size();
            result.strips.push_back (strip);
            continue;
        }

        // Knobs, sliders and switches: their look's frames (a switch: off, on).
        if (Looks::takesLook (w) && looks != nullptr)
        {
            looks->setLight (layout.lightAngle, layout.lightAltitude);   // lit by the panel's global light
            const auto kind = Looks::kindOf (w);
            const auto& look = looks->lookFor (w);
            const auto key = juce::String (look.revision) + "|" + Looks::kindName (kind) + "|"
                           + juce::String (area.getWidth()) + "x" + juce::String (area.getHeight());

            // Its frames reach past its box by the look's shadows: drawn over the box expanded.
            const auto room = Looks::overhang (look, area.getWidth(), area.getHeight());
            result.movingArea.back() = area.expanded (room);

            if (const auto found = lookStrips.find (key); found != lookStrips.end())
            {
                result.stripOf.back() = found->second;
                continue;
            }

            Bake::Strip strip;
            strip.image = looks->strip (look, kind, area.getWidth(), area.getHeight());
            strip.frameWidth = area.getWidth() + 2 * room;
            strip.frameHeight = area.getHeight() + 2 * room;
            strip.frames = Looks::framesFor (look, kind);

            lookStrips[key] = (int) result.strips.size();
            result.stripOf.back() = (int) result.strips.size();
            result.strips.push_back (strip);
            continue;
        }

        int frames = 0;

        switch (w.type)
        {
            case GuiWidget::Type::preset:    frames = juce::jmax (1, presetMenu.names.size()); break;
            case GuiWidget::Type::lamp:      frames = 2; break;
            case GuiWidget::Type::selector:
            {
                const auto* p = paramFor (w);
                frames = juce::jmax (1, w.options.size() > 0 ? w.options.size() : (p != nullptr ? (int) (p->max - p->min) + 1 : 1));
                break;
            }
            case GuiWidget::Type::knob:
            case GuiWidget::Type::slider:
            case GuiWidget::Type::toggle:
            case GuiWidget::Type::label:
            case GuiWidget::Type::group:
            case GuiWidget::Type::meter:
            case GuiWidget::Type::scope:
            case GuiWidget::Type::envelope:
            case GuiWidget::Type::filter:
            case GuiWidget::Type::xy:
            case GuiWidget::Type::shape:
            case GuiWidget::Type::image:
            case GuiWidget::Type::keyboard:           // its keys are in the background; the plugin draws the ones that are down
            case GuiWidget::Type::custom:             // the plugin draws it itself
            case GuiWidget::Type::registration:
            case GuiWidget::Type::tabs: break;
        }

        if (frames == 0)
            continue;

        Bake::Strip strip;
        strip.frameWidth = area.getWidth();
        strip.frameHeight = area.getHeight();
        strip.frames = frames;
        strip.image = juce::Image (juce::Image::ARGB, area.getWidth(), area.getHeight() * frames, true);

        for (int f = 0; f < frames; ++f)
        {
            juce::Graphics g (strip.image);
            g.setOrigin ({ -area.getX(), f * area.getHeight() - area.getY() });
            g.reduceClipRegion (area);

            drawWidget (g, i, Part::moving, (float) f);
        }

        result.stripOf.back() = (int) result.strips.size();
        result.strips.push_back (strip);
    }

    return result;
}

void PluginCanvas::paint (juce::Graphics& g)
{
    // A made plugin's window in Sonify: the panel and what's on it, nothing around it.
    if (panelOnly)
    {
        g.fillAll (juce::Colours::black);

        if (! hasPanel())
            return;

        const auto area = panelArea();
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (area.toNearestInt());
        g.addTransform (juce::AffineTransform::scale (zoom).translated (area.getX(), area.getY()));

        drawPanel (g);

        // Only what lies in the area being painted (a knob turning paints its own square).
        const auto painted = g.getClipBounds().toFloat();

        for (int i = 0; i < (int) layout.widgets.size(); ++i)
            if (! liesOutside (layout.widgets[(size_t) i], painted))
                drawWidget (g, i);

        drawElementPopups (g);
        return;
    }

    // A grid over the whole tab, FlowStone-style; it moves and zooms with the view.
    g.fillAll (Theme::panel);
    drawGrid (g);

    if (! hasPanel())
        return;

    const auto area = panelArea();

    juce::DropShadow (juce::Colours::black.withAlpha (0.45f), 18, { 0, 6 }).drawForRectangle (g, area.toNearestInt());

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (area.toNearestInt());
        g.addTransform (juce::AffineTransform::scale (zoom).translated (area.getX(), area.getY()));

        drawPanel (g);

        // The alignment grid shows only while something is being moved or resized: the rest of
        // the time the window shows exactly as the plugin will (a grid over it looked like part
        // of its background).
        if (design && dragMoved && (drag == Drag::element || drag == Drag::resizeElement || drag == Drag::resizePanel))
        {
            const auto plugin = juce::Rectangle<float> (0.0f, 0.0f, (float) layout.width, (float) layout.height);
            g.setColour (juce::Colours::white.withAlpha (0.035f));

            for (int x = 16; x < layout.width; x += 16)
                g.drawVerticalLine (x, 0.0f, plugin.getBottom());

            for (int y = 16; y < layout.height; y += 16)
                g.drawHorizontalLine (y, 0.0f, plugin.getRight());
        }

        const auto painted = g.getClipBounds().toFloat();

        for (int i = 0; i < (int) layout.widgets.size(); ++i)
            if (! liesOutside (layout.widgets[(size_t) i], painted))
                drawWidget (g, i);

        // Programmed elements' popups lie over everything.
        if (! design)
            drawElementPopups (g);
    }

    g.setColour (design ? (panelSelected ? Theme::accent : Theme::accent.withAlpha (0.55f)) : Theme::outline);
    g.drawRect (area, design ? (panelSelected ? 2.0f : 1.5f) : 1.0f);

    // Which tab, and the window's size, above its top-left corner.
    {
        const auto tab = juce::String (design ? "EDIT" : "PLAY");
        const auto font = Theme::font (12.0f, true);
        const auto caption = juce::Rectangle<float> (area.getX(), area.getY() - 20.0f, 320.0f, 16.0f);

        g.setColour (design ? Theme::accent : Theme::safe);
        g.setFont (font);
        g.drawText (tab, caption, juce::Justification::centredLeft, false);

        g.setColour (Theme::muted);
        g.setFont (Theme::font (12.0f));
        g.drawText (juce::String (layout.width) + juce::String::fromUTF8 (" \xc3\x97 ") + juce::String (layout.height),
                    caption.withTrimmedLeft ((float) juce::GlyphArrangement::getStringWidthInt (font, tab) + 10.0f),
                    juce::Justification::centredLeft, false);
    }

    drawPageStrip (g);

    if (design && ! cropping)
    {
        // The window's own corner: drag it to resize the window.
        const auto grip = panelHandle();
        g.setColour (panelSelected || drag == Drag::resizePanel ? Theme::accent : Theme::accent.withAlpha (0.7f));
        juce::Path corner;
        corner.addTriangle (grip.getTopRight(), grip.getBottomRight(), grip.getBottomLeft());
        g.fillPath (corner);

        // Several selected: a frame round each (handles only for one alone).
        for (const auto index : manySelected)
        {
            if (! juce::isPositiveAndBelow (index, (int) layout.widgets.size()) || ! isShown (layout.widgets[(size_t) index]))
                continue;

            g.setColour (index == selected ? Theme::accent : Theme::accent.withAlpha (0.75f));
            g.strokePath (viewOutline (layout.widgets[(size_t) index], 2.0f), juce::PathStrokeType (1.5f));
        }

        // Shift+drag's box.
        if (drag == Drag::marquee && ! marquee.isEmpty())
        {
            const auto box = toView (marquee);
            g.setColour (Theme::accent.withAlpha (0.12f));
            g.fillRect (box);
            g.setColour (Theme::accent.withAlpha (0.8f));
            g.drawRect (box, 1.0f);
        }

        if (juce::isPositiveAndBelow (selected, (int) layout.widgets.size()) && manySelected.empty() && isShown (layout.widgets[(size_t) selected]))
        {
            const auto& w = layout.widgets[(size_t) selected];
            g.setColour (isLine (selected) ? Theme::accent.withAlpha (0.35f) : Theme::accent);   // a line's box only faintly: its handles shape it
            g.strokePath (viewOutline (w, 2.0f), juce::PathStrokeType (1.5f));

            // The turn: a round handle on a stem above its top, with its angle while it turns.
            if (const auto knob = handleArea (turnHandle); ! knob.isEmpty())
            {
                g.setColour (Theme::accent.withAlpha (0.8f));
                g.drawLine ({ handleCentre (1), knob.getCentre() }, 1.2f);
                g.setColour (Theme::accent);
                g.fillEllipse (knob);
                g.setColour (juce::Colours::white.withAlpha (0.9f));
                g.drawEllipse (knob.reduced (0.5f), 1.0f);

                // A little turning arrow inside.
                juce::Path arrow;
                const auto c = knob.getCentre();
                const auto r = knob.getWidth() * 0.26f;
                arrow.addCentredArc (c.x, c.y, r, r, 0.0f, 0.4f, 4.9f, true);
                g.strokePath (arrow, juce::PathStrokeType (1.1f));

                if (drag == Drag::turnElement)
                {
                    g.setFont (Theme::font (12.0f, true));
                    g.drawText (juce::String (w.angle, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + juce::String::fromUTF8 ("\xc2\xb0"),
                                knob.translated (knob.getWidth() + 6.0f, 0.0f).withWidth (60.0f), juce::Justification::centredLeft, false);
                }
            }

            // Its handles: corners and sides.
            for (int handle = 0; handle < 8; ++handle)
            {
                if (const auto square = handleArea (handle); ! square.isEmpty())
                {
                    g.setColour (Theme::accent);
                    g.fillRect (square);
                    g.setColour (juce::Colours::white.withAlpha (0.85f));
                    g.drawRect (square, 1.0f);
                }
            }

            // A line's ends: round handles; the way between them, faint, while it's bent.
            if (const auto start = handleArea (lineStartHandle), end = handleArea (lineEndHandle); ! start.isEmpty() && ! end.isEmpty())
            {
                if (ObjectPaint::isBent (w))
                {
                    const float dashes[] { 4.0f, 4.0f };
                    g.setColour (Theme::accent.withAlpha (0.45f));
                    g.drawDashedLine ({ start.getCentre(), end.getCentre() }, dashes, 2, 1.0f);
                }

                for (const auto& knob : { start, end })
                {
                    g.setColour (juce::Colours::white);
                    g.fillEllipse (knob);
                    g.setColour (Theme::accent);
                    g.drawEllipse (knob.reduced (0.75f), 1.5f);
                }
            }

            // A shape's own: a yellow diamond.
            if (const auto own = handleArea (shapeHandle); ! own.isEmpty())
            {
                juce::Path diamond;
                const auto c = own.getCentre();
                const auto r = own.getWidth() * 0.62f;
                diamond.addQuadrilateral (c.x, c.y - r, c.x + r, c.y, c.x, c.y + r, c.x - r, c.y);
                g.setColour (juce::Colour (0xffffcc00));
                g.fillPath (diamond);
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.strokePath (diamond, juce::PathStrokeType (1.0f));
            }
        }
    }

    if (cropping)
        drawCrop (g);

    drawDropGhost (g);
    drawScrollbars (g);
    drawMinimap (g);
}
