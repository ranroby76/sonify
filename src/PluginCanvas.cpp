// D:\workspace\Fanan_web_hub\src\PluginCanvas.cpp
// The canvas's elements: adding them, drawing them, baking them for Export. The view and
// the mouse are in PluginCanvas_Mouse.cpp, the edit menu in PluginCanvas_Menu.cpp.

#include "PluginCanvas.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <set>
#include "PluginCanvasMenu.h"
#include "LayerStyleWindow.h"
#include "FontLibrary.h"
#include "MeterPaint.h"
#include "PanelParts.h"
#include "StellaLookAndFeel.h"
#include "WasmElements.h"
#include "KnobStudio.h"   // the Gallery
#include "ShapeMerge.h"   // Ctrl+M
#include "Body3D.h"       // the window's 3D body
#include "Render3D.h"
#include "BodyEditor.h"     // ...and its editor
#include "SelectorSet.h"    // selectors as sets of keys
#include "SelectorStudio.h" // ...and their studio
#include "PerformControl.h" // wheels, levers, joysticks on an X-Y pad
#include "PerformStudio.h"  // ...and their studio
#include "ModMatrix.h"      // the modulation matrix
#include "MatrixStudio.h"   // ...and its studio
#include "PresetStudio.h"   // preset managers' designs (PresetPanel)
#include "SegmentDisplay.h"  // LED digits
#include "IconLibrary.h"     // icons from the Gallery
#include "LayerRender.h"     // kept shapes as pictures
#include "Showroom.h"       // the finished instrument, seen from round it
#include "ColourSwatch.h"   // the pickers' dropper takes colours from the panel
#include "stella_keys.h"
#include "MaterialArchetypes.h"   // a material look's painted panel

namespace
{
    const char* rulerTickName (int item)
    {
        return item == -2 ? "Allow X rulers" : (item == -3 ? "Allow Y rulers" : "Allow body distances");
    }

    /** Where the rulers' ticks are remembered (Edit's, above the window). */
    juce::File rulersFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Fanan").getChildFile ("canvas_rulers.txt");
    }

    constexpr int scopeSamples = 2048;
    const juce::Colour defaultAccent { 0xffe5484d };
    const juce::Colour captionColour = PanelParts::standardCaptionColour();

    /** A caption as its element has it: its own colour (else the standard one, as faint as given),
        bold, italic, underlined; centred under (or over) it, moved up or down as far as it's shifted. */
    void drawCaption (juce::Graphics& g, const GuiWidget& w, const juce::String& text, juce::Rectangle<float> area,
                      float standardAlpha, bool strong = false, bool muted = false)
    {
        // Its own size: as big (or small) as it's set, whatever the control's size; wider than
        // its strip, it spreads both ways from its middle rather than ending in dots.
        const auto font = FontLibrary::font (w.font, w.fontSize > 0.0f ? w.fontSize : 12.5f, strong || w.bold);

        if (w.fontSize > 0.0f)
        {
            const auto wide = juce::GlyphArrangement::getStringWidth (font, text) + 6.0f;
            area = area.withSizeKeepingCentre (juce::jmax (area.getWidth(), wide), juce::jmax (area.getHeight(), font.getHeight() + 2.0f));
        }

        area = area.translated (0.0f, (float) w.textShift);
        PanelParts::drawStyledText (g, w, text, area, juce::Justification::centred, font,
                                    muted ? Theme::muted : (w.textColour.isTransparent() ? captionColour.withAlpha (standardAlpha) : w.textColour));
    }

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
        || w.type == GuiWidget::Type::custom || w.type == GuiWidget::Type::tabs || w.type == GuiWidget::Type::matrix;
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
        case GuiWidget::Type::matrix:    return "Mod matrix";
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
        case GuiWidget::Type::matrix:    return { x, y, 420, 260 };
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
    loadRulers();
    setOpaque (true);   // paint() always fills it: nothing behind needs painting first
    ColourDropper::markAsSource (*this);   // a colour picker open: a click on the panel takes the colour there
    scopeBuffer.resize ((size_t) scopeSamples, 0.0f);

    menu = std::make_unique<ElementMenu> (*this);   // in a window of its own, once opened

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
    bodyPainter = std::make_unique<juce::ThreadPool> (juce::ThreadPoolOptions{}.withThreadName ("3D body").withNumberOfThreads (1));
    startTimerHz (30);
}

PluginCanvas::~PluginCanvas()
{
    *alive = false;
    stopTimer();
    bodyEditorWindow = nullptr;   // the Body editor goes with the panel it edits
    menuWindow = nullptr;             // ...and the edit window (its studio with it)
    shapePainter = nullptr;   // a shape still being painted is let go (its picture finds no canvas)
    bodyPainter = nullptr;    // ...and the body (it renders to its end first)

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
    presetPopup = -1;   // ...and a preset manager's open list
    menu->forgetTyped();   // words typed in the menu were for the layout before
    layout = newLayout;
    nameUnboundControls();   // (no plugin behind it: each control its own parameter)
    PanelParts::setTextLight (layout.lightAngle, layout.lightAltitude);   // text effects are lit by it
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

    if (! Body3D::hasParts (layout.body))
        bodySelected = false;

    if (selected < 0 && ! panelSelected && ! bodySelected)
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

    // A route changed: its cables and rings lie anywhere.
    for (const auto& id : ids)
        if (ModMatrix::isMatrixParam (layout, id))
        {
            repaint();
            return;
        }

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
    const auto reach = std::hypot (box.getWidth(), box.getHeight()) * 0.5f * 1.4f + 24.0f + (float) std::abs (w.textShift)   // a shifted caption too
                     + (w.fontSize > 0.0f ? w.fontSize * (0.4f * (float) w.label.length() + 1.0f) : 0.0f);                     // ...and a big one
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
    ++editStamp;
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

    // Nothing opens by itself: a picture without its file says to double-click it.
    addElement (primitive.makeWidget(), centre);
}

juce::Rectangle<int> PluginCanvas::placed (juce::Rectangle<int> bounds, juce::Point<float> centre) const
{
    auto b = bounds.withPosition (snapped (centre.x - (float) bounds.getWidth() * 0.5f),
                                  snapped (centre.y - (float) bounds.getHeight() * 0.5f));

    b.setX (juce::jlimit (0, juce::jmax (0, layout.width - b.getWidth()), b.getX()));
    b.setY (juce::jlimit (0, juce::jmax (0, layout.height - b.getHeight()), b.getY()));
    return b;
}

//==============================================================================
juce::String PluginCanvas::freshParamId (const juce::String& base, const juce::StringArray& avoid) const
{
    auto taken = [&] (const juce::String& id)
    {
        if (avoid.contains (id))
            return true;

        for (const auto& w : layout.widgets)
        {
            if (w.param == id || id.startsWith (w.param + "_") || (w.param.isNotEmpty() && id.startsWith (w.param) && id.substring (w.param.length()).containsOnly ("0123456789")))
                return true;

            for (const auto& [role, other] : w.roles)
                if (other == id)
                    return true;
        }

        return false;
    };

    for (int n = 1;; ++n)
        if (const auto id = base + juce::String (n); ! taken (id))
            return id;
}

bool PluginCanvas::nameUnboundControls()
{
    if (! inventsParams)
        return false;

    bool named = false;

    for (auto& w : layout.widgets)
    {
        const auto base = w.type == GuiWidget::Type::knob ? "knob" : w.type == GuiWidget::Type::slider ? "slider"
                        : w.type == GuiWidget::Type::toggle ? "switch" : w.type == GuiWidget::Type::selector ? "selector" : "";

        if (*base != 0 && w.param.isEmpty())
        {
            w.param = freshParamId (base, {});
            named = true;
        }

        if (w.type == GuiWidget::Type::xy)
        {
            for (const auto* axis : { "x", "y" })
            {
                if (const auto found = w.roles.find (axis); found == w.roles.end() || found->second.isEmpty())
                {
                    w.roles[axis] = freshParamId (juce::String ("pad_") + axis, {});
                    named = true;
                }
            }
        }
    }

    return named;
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

            // No plugin behind it: a parameter of its own (never one another control has).
            if (w.param.isEmpty() && inventsParams)
                w.param = freshParamId (w.type == GuiWidget::Type::knob ? "knob" : w.type == GuiWidget::Type::slider ? "slider"
                                        : w.type == GuiWidget::Type::toggle ? "switch" : "selector", {});

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
                auto x = unusedParam ({});
                auto y = unusedParam ({ x });

                if (inventsParams && x.isEmpty())    x = freshParamId ("pad_x", {});
                if (inventsParams && y.isEmpty())    y = freshParamId ("pad_y", { x });

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

        case GuiWidget::Type::matrix:
            if (w.settings.count ("matrix") == 0)
                w.settings["matrix"] = ModMatrix::makeTemplate (0);

            if (w.param.isEmpty())
                w.param = "mod";
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

    // Selected, its edit window not opened (the E under it, a right-click or a double-click
    // opens it; already open, it follows).
    select (index);
    edited();
    return index;
}

void PluginCanvas::removeSelected()
{
    // The 3D body, selected: taken off the window (an undo brings it back).
    if (bodySelected && Body3D::hasParts (layout.body))
    {
        layout.body = juce::var();
        bodySelected = false;
        bodyImageMap.reset();
        bodyHighlight.clear();
        closeMenu();
        edited();
        repaint();
        return;
    }

    const auto all = selection();

    if (all.empty())
        return;

    for (auto it = all.rbegin(); it != all.rend(); ++it)
        layout.widgets.erase (layout.widgets.begin() + *it);

    indexesChanged();
    select (-1);
    edited();
}

void PluginCanvas::mergeShapes (const std::vector<int>& indexes)
{
    const auto shapes = ShapeMerge::mergeable (layout, indexes);

    if (shapes.size() < 2)
        return;

    std::vector<const GuiWidget*> parts;

    for (const auto i : shapes)
        parts.push_back (&layout.widgets[(size_t) i]);

    auto merged = ShapeMerge::merge (parts);

    if (! merged.has_value())
        return;

    // In the top one's place; the rest go (from the back, so the indexes before stay put).
    const auto top = shapes.back();
    layout.widgets[(size_t) top] = std::move (*merged);

    for (auto it = std::next (shapes.rbegin()); it != shapes.rend(); ++it)
        layout.widgets.erase (layout.widgets.begin() + *it);

    const auto at = top - (int) (shapes.size() - 1);
    indexesChanged();
    select (at);
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

        c.setBehindBody (index, Body3D::hasParts (c.layout.body));   // a background picture: behind the 3D body
        c.select (index);
        c.edited();
    });
}

//==============================================================================
void PluginCanvas::paintMaterialPanel (const juce::String& lookName)
{
    if (onPaintPicture == nullptr || ! hasPanel())
        return;

    if (paintingPicture)
    {
        showAiReply ("A picture is still being painted: apply the look again in a moment for its painted panel.");
        return;
    }

    // The window's painted background, painted again in the look; none: a new one of its own
    // (the user's own pictures are never painted over).
    const auto painted = GuiLayout::paintedPictures (guiFolder);
    juce::String file;

    for (const auto& w : layout.widgets)
        if (file.isEmpty() && w.type == GuiWidget::Type::image && w.bounds == juce::Rectangle<int> (0, 0, layout.width, layout.height)
            && paintedRecipeOf (w).isObject())
            file = w.image;

    if (file.isEmpty())
    {
        file = "background.png";

        for (int n = 2; imagesFolder().getChildFile (file).existsAsFile() && ! painted.contains (file); ++n)
            file = "background-" + juce::String (n) + ".png";
    }

    auto recipe = MaterialArchetypes::painterRecipe (layout, lookName, file);

    if (! recipe.isObject())
        return;

    recipe.getDynamicObject()->setProperty ("quiet", true);
    paintingPicture = true;
    showAiWorking ("Painting the panel");

    onPaintPicture (recipe, [safeThis = juce::Component::SafePointer<PluginCanvas> (this), file, generation = paintGeneration] (const juce::String& problem)
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

        c.showAiReply ("Painted. Its material, colours and pattern are in its menu.");
        c.pictureRepainted (file);

        // Behind everything, filling the window (unless it's there already).
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

        c.setBehindBody (index, Body3D::hasParts (c.layout.body));   // a background picture: behind the 3D body
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
    PanelParts::setTextLight (layout.lightAngle, layout.lightAltitude);
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

void PluginCanvas::editTextEffects (int widgetIndex)
{
    if (! hasPanel() || ! juce::isPositiveAndBelow (widgetIndex, (int) layout.widgets.size())
          || ! PanelParts::hasText (layout.widgets[(size_t) widgetIndex]))
        return;

    const auto before = layout.widgets[(size_t) widgetIndex];
    const juce::Point<float> lightBefore (layout.lightAngle, layout.lightAltitude);
    const juce::Component::SafePointer<PluginCanvas> safeThis (this);

    auto elementAt = [safeThis, widgetIndex]() -> GuiWidget*
    {
        if (safeThis == nullptr || ! juce::isPositiveAndBelow (widgetIndex, (int) safeThis->layout.widgets.size()))
            return nullptr;

        auto& w = safeThis->layout.widgets[(size_t) widgetIndex];
        return PanelParts::hasText (w) ? &w : nullptr;
    };

    LayerStyleWindow::Host host;
    host.alive = [elementAt] { return elementAt() != nullptr; };

    host.apply = [safeThis, elementAt, before, lightBefore] (const LayerStyle& style, bool final)
    {
        auto* w = elementAt();

        if (w == nullptr)
            return;

        w->settings = before.settings;
        PanelParts::setTextEffects (*w, style);

        if (final)
        {
            safeThis->edited();

            if (juce::Point<float> (safeThis->layout.lightAngle, safeThis->layout.lightAltitude) != lightBefore)
                safeThis->repaintBackgrounds();
        }

        safeThis->repaint();
    };

    host.cancel = [safeThis, elementAt, before, lightBefore]
    {
        if (auto* w = elementAt())
            w->settings = before.settings;

        if (safeThis != nullptr)
            safeThis->setGlobalLight (lightBefore.x, lightBefore.y);
    };

    host.sample = [before] (const LayerStyle& style)
    {
        // Its words, big, wearing the style.
        auto sample = before;
        PanelParts::setTextEffects (sample, style);
        const auto words = sample.label.isNotEmpty() && sample.type != GuiWidget::Type::tabs ? sample.label.substring (0, 18) : juce::String ("Text");
        juce::Image picture (juce::Image::ARGB, 360, 120, true);
        juce::Graphics g (picture);
        const auto ink = ! sample.textColour.isTransparent() ? sample.textColour
                       : (sample.type == GuiWidget::Type::label || sample.type == GuiWidget::Type::group) && ! sample.colour.isTransparent() ? sample.colour
                       : captionColour;
        PanelParts::drawStyledText (g, sample, words, { 0.0f, 0.0f, 360.0f, 120.0f }, juce::Justification::centred,
                                    FontLibrary::font (sample.font, 44.0f, sample.bold), ink, true);
        return picture;
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
    const auto name = w.label.isNotEmpty() ? w.label : GuiLayout::typeName (w.type);
    LayerStyleWindow::launch (PanelParts::textEffectsOf (w), std::move (host), "Text Effects: " + name, this);
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
        setBehindBody (index, Body3D::hasParts (layout.body));   // a background picture: behind the 3D body

        select (index);
        edited();
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

//==============================================================================
// "Apply this knob on similars"
int PluginCanvas::similarsToChange (int widgetIndex) const
{
    if (looks == nullptr || ! juce::isPositiveAndBelow (widgetIndex, (int) layout.widgets.size()))
        return 0;

    const auto& w = layout.widgets[(size_t) widgetIndex];

    if (! Looks::takesLook (w))
        return 0;

    const auto kind = Looks::kindOf (w);
    const auto look = looks->lookFor (w).name;
    int count = 0;

    for (size_t i = 0; i < layout.widgets.size(); ++i)
    {
        const auto& other = layout.widgets[i];

        if ((int) i != widgetIndex && Looks::takesLook (other) && Looks::kindOf (other) == kind && looks->lookFor (other).name != look)
            ++count;
    }

    return count;
}

void PluginCanvas::putSimilarsBack()
{
    if (similarsTry.widget < 0)
        return;

    // Only the ones still wearing the tried look (one changed since keeps its change).
    for (const auto& [index, style] : similarsTry.before)
        if (juce::isPositiveAndBelow (index, (int) layout.widgets.size()) && layout.widgets[(size_t) index].style == similarsTry.look)
            layout.widgets[(size_t) index].style = style;

    similarsTry = {};
}

void PluginCanvas::trySimilars (int widgetIndex, bool on)
{
    if (! on)
    {
        if (isTryingSimilars (widgetIndex))
        {
            putSimilarsBack();
            edited();
        }

        return;
    }

    if (looks == nullptr || ! juce::isPositiveAndBelow (widgetIndex, (int) layout.widgets.size()) || isTryingSimilars (widgetIndex))
        return;

    putSimilarsBack();   // another control's try, undone first

    const auto& w = layout.widgets[(size_t) widgetIndex];

    if (! Looks::takesLook (w))
        return;

    const auto kind = Looks::kindOf (w);
    const auto look = looks->lookFor (w).name;
    SimilarsTry tried;
    tried.widget = widgetIndex;
    tried.look = look;

    for (size_t i = 0; i < layout.widgets.size(); ++i)
    {
        auto& other = layout.widgets[i];

        if ((int) i == widgetIndex || ! Looks::takesLook (other) || Looks::kindOf (other) != kind || looks->lookFor (other).name == look)
            continue;

        tried.before.emplace_back ((int) i, other.style);
        rememberOriginalLook (other);
        other.style = look;
    }

    similarsTry = std::move (tried);
    edited();
}

void PluginCanvas::applySimilars (int widgetIndex)
{
    if (! isTryingSimilars (widgetIndex))
        trySimilars (widgetIndex, true);

    if (isTryingSimilars (widgetIndex))
        similarsTry = {};   // for good: nothing kept to put back
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

bool PluginCanvas::wearsGalleryDesign (const GuiWidget& w, const juce::String& kind, const juce::var& design)
{
    if (kind == "set")      return w.type == GuiWidget::Type::selector;
    if (kind == "perform")  return w.type == GuiWidget::Type::xy;
    if (kind == "presets")  return w.type == GuiWidget::Type::preset;
    return kind == "matrix" && w.type == GuiWidget::Type::matrix && ModMatrix::isPart (w) == ModMatrix::isPart (design);
}

void PluginCanvas::useGalleryDesign (const juce::String& kind, const juce::var& design, const juce::var& settings, int keys, bool asNew,
                                     int target, std::optional<juce::Point<float>> at)
{
    if (! this->design || ! hasPanel() || ! design.isObject())
        return;

    const bool part = kind == "matrix" && ModMatrix::isPart (design);
    auto matches = [&] (const GuiWidget& w) { return wearsGalleryDesign (w, kind, design); };

    auto wear = [&] (GuiWidget& w)
    {
        w.settings[kind] = design.clone();

        if (const auto* own = settings.getDynamicObject())
            for (const auto& entry : own->getProperties())
                w.settings[entry.name.toString()] = entry.value;
    };

    // Dropped on one of its kind: that one; else the selected one of its kind (unless a new one's asked for).
    int index = juce::isPositiveAndBelow (target, (int) layout.widgets.size()) && matches (layout.widgets[(size_t) target]) ? target : -1;

    if (index < 0 && ! asNew && ! at.has_value() && juce::isPositiveAndBelow (selected, (int) layout.widgets.size())
        && matches (layout.widgets[(size_t) selected]))
        index = selected;

    if (index >= 0)
    {
        wear (layout.widgets[(size_t) index]);
    }
    else
    {
        // A new one: the toolbox's element of that kind, wearing it.
        juce::String id = kind == "set" ? "selector_keys" : kind == "perform" ? "perform_ball" : kind == "presets" ? "presets" : "matrix_slots";

        if (part)
        {
            const auto style = design.getProperty ("style", {}).toString().trim().toLowerCase();
            id = style == "jack" ? (ModMatrix::roleOf (design) == "in" ? "matrix_jack_in" : "matrix_jack_out")
                                 : style == "chip" ? "matrix_chip" : "matrix_assign";
        }

        const auto* primitive = PrimitiveLibrary::get().find (id);

        if (primitive == nullptr)
            return;

        auto w = primitive->makeWidget();
        wear (w);

        // A set's keys: as many positions.
        if (kind == "set" && keys > 0)
        {
            while (w.options.size() < keys)
                w.options.add (juce::String (w.options.size() + 1));

            while (w.options.size() > keys)
                w.options.remove (w.options.size() - 1);
        }

        index = addElement (w, at);
    }

    syncSideModules();
    select (index);
    edited();

    if (onWindowChanged != nullptr)
        onWindowChanged();

    repaint();
}

void PluginCanvas::useGalleryBody (const juce::var& body, int fromWidth, int fromHeight)
{
    if (! design || ! hasPanel() || ! body.isObject())
        return;

    auto apply = [safe = juce::Component::SafePointer<PluginCanvas> (this), body, fromWidth, fromHeight]
    {
        if (safe == nullptr)
            return;

        // Three quarters of the window, in its middle: room round it for a table, a backdrop,
        // a stand (its handles, or a frame round it, make it bigger).
        const auto W = (float) safe->layout.width, H = (float) safe->layout.height;
        const auto fromW = fromWidth > 0 ? fromWidth : safe->layout.width, fromH = fromHeight > 0 ? fromHeight : safe->layout.height;
        const auto from = juce::Rectangle<float> ((float) fromW, (float) fromH);
        const auto to = juce::Rectangle<float> (W * 0.75f, H * 0.75f).withCentre ({ W * 0.5f, H * 0.5f });
        safe->layout.body = bodyInto (body, fromW, fromH, from, to);
        safe->bodyImageMap.reset();

        // The background pictures go behind it (the room it stands in).
        for (int i = 0; i < (int) safe->layout.widgets.size(); ++i)
            if (safe->isBackdrop (safe->layout.widgets[(size_t) i]))
                safe->setBehindBody (i, true);
        safe->edited();
        safe->repaint();
    };

    // Over a body: asked first.
    if (Body3D::hasParts (layout.body))
    {
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Use this body",
                                            "It takes the place of the window's body. (Undo brings that one back.)", "Use it", "Cancel", nullptr,
                                            juce::ModalCallbackFunction::create ([apply] (int result)
                                            {
                                                if (result == 1)
                                                    apply();
                                            }));
        return;
    }

    apply();
}

void PluginCanvas::bodyFollowsWindow (int fromWidth, int fromHeight, juce::Point<int> shift)
{
    if (! Body3D::hasParts (layout.body))
        return;

    // Moved with a crop: every part by the crop's corner.
    if (! shift.isOrigin())
    {
        layout.body = layout.body.clone();

        if (auto* o = layout.body.getDynamicObject(); o != nullptr && o->hasProperty ("mirrorAt"))
            o->setProperty ("mirrorAt", (double) o->getProperty ("mirrorAt") - shift.x);

        if (auto* parts = layout.body.getProperty ("parts", {}).getArray())
        {
            for (auto& part : *parts)
            {
                if (auto* o = part.getDynamicObject())
                {
                    o->setProperty ("x", (double) part.getProperty ("x", 0.0) - shift.x);
                    o->setProperty ("z", (double) part.getProperty ("z", 0.0) - shift.y);
                }
            }
        }
    }

    if (fromWidth > 0 && fromHeight > 0 && (fromWidth != layout.width || fromHeight != layout.height))
        layout.body = BodyEditor::scaledBody (layout.body, fromWidth, fromHeight, layout.width, layout.height);
}

void PluginCanvas::useGalleryIcon (const juce::String& name, int target, std::optional<juce::Point<float>> at, const juce::var& badge)
{
    if (! design || ! hasPanel() || IconLibrary::indexOf (name) < 0)
        return;

    // A Guitar FX icon comes on its badge, in its colours.
    auto wear = [&badge] (GuiWidget& w)
    {
        if (! badge.hasProperty ("badge"))
            return;

        const auto shape = badge.getProperty ("badge", "none").toString();

        if (shape == "none") w.settings.erase ("badge");
        else                 w.settings["badge"] = shape;

        if (badge.hasProperty ("badgeColour"))
            w.settings["badgeColour"] = badge.getProperty ("badgeColour", {}).toString();

        if (badge.hasProperty ("iconColour"))
            w.colour = juce::Colour::fromString (badge.getProperty ("iconColour", {}).toString());
    };

    // Dropped on an Icon element (or, not dropped, the selected one): its icon.
    const auto onto = target >= 0 ? target : (at.has_value() ? -1 : selected);

    if (juce::isPositiveAndBelow (onto, (int) layout.widgets.size()))
    {
        auto& w = layout.widgets[(size_t) onto];

        if (w.type == GuiWidget::Type::label && PanelParts::labelDisplayOf (w) == "icon")
        {
            w.label = IconLibrary::token (name);
            wear (w);
            edited();
            repaint();
            return;
        }
    }

    if (const auto* primitive = PrimitiveLibrary::get().find ("icon"))
    {
        auto w = primitive->makeWidget();
        w.label = IconLibrary::token (name);
        wear (w);

        if (badge.hasProperty ("badge") && badge.getProperty ("badge", "none").toString() != "none")
            w.bounds.setSize (56, 56);   // a badge reads at a size of its own

        select (addElement (w, at));
        edited();
        repaint();
    }
}

void PluginCanvas::addGalleryShape (const juce::File& file, std::optional<juce::Point<float>> at)
{
    if (! design || ! hasPanel())
        return;

    Looks::Look look;

    if (! Looks::readFile (file, look))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Gallery", "Couldn't read " + file.getFileName() + ".");
        return;
    }

    // Its picture, shadows and glows included, among the design's pictures.
    const auto picture = LayerRender::renderFrame (look.doc, 0.0f, 1.0f, std::ceil (LayerRender::overhangOf (look.doc)));
    const auto folder = imagesFolder();

    if (! picture.isValid() || ! folder.createDirectory())
        return;

    const auto target = folder.getNonexistentChildFile (juce::File::createLegalFileName (file.getFileNameWithoutExtension()), ".png", false);

    {
        juce::FileOutputStream out (target);

        if (! out.openedOk() || ! juce::PNGImageFormat().writeImageToStream (picture, out))
            return;
    }

    GuiWidget w;
    w.type = GuiWidget::Type::image;
    w.image = target.getFileName();
    w.mode = "fit";
    const auto longest = juce::jmax (picture.getWidth(), picture.getHeight());
    const auto scale = longest > 160 ? 160.0f / (float) longest : 1.0f;
    w.bounds = { 0, 0, juce::jmax (8, juce::roundToInt ((float) picture.getWidth() * scale)), juce::jmax (8, juce::roundToInt ((float) picture.getHeight() * scale)) };
    select (addElement (w, at));
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
    // Front to back: what's over the 3D body, the body (where it is: nothing picked, the body
    // is), what's behind it.
    std::vector<int> order;
    const bool body = Body3D::hasParts (layout.body);

    for (int i = (int) layout.widgets.size(); --i >= 0;)
        if (! isBehindBody (i))
            order.push_back (i);

    if (body)
    {
        order.push_back (-2);

        for (int i = (int) layout.widgets.size(); --i >= 0;)
            if (isBehindBody (i))
                order.push_back (i);
    }

    int insideFrame = -1;   // a frame the point is inside: picked when nothing else is there

    for (const auto i : order)
    {
        if (i == -2)
        {
            if (bodyBounds().contains (plugin))
                return insideFrame >= 0 ? insideFrame : -1;

            continue;
        }

        const auto& w = layout.widgets[(size_t) i];

        if ((interactiveOnly && ! isInteractive (w)) || ! isShown (w))
            continue;

        // Picked in its own frame: a turned element where it is turned to.
        const auto at = intoWidget (w, plugin);

        if (w.type == GuiWidget::Type::group && ! interactiveOnly)
        {
            // A frame, as an unfilled shape: picked by its border and title, and inside it where
            // nothing else is (what's inside stays reachable, the 3D body under it too).
            const auto outer = w.bounds.toFloat();

            if (outer.contains (at) && ! outer.reduced (8.0f).withTrimmedTop (16.0f).contains (at))
                return i;

            if (outer.contains (at) && insideFrame < 0)
                insideFrame = i;

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

    return insideFrame;
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
    x += (layout.pages.isEmpty() ? 64.0f : 22.0f) + 16.0f;

    // The rulers' ticks.
    for (const int item : { -2, -3, -4 })
    {
        const auto width = (float) juce::GlyphArrangement::getStringWidthInt (font, rulerTickName (item)) + 26.0f;
        items.push_back ({ item, { x, y, width, 20.0f } });
        x += width + 8.0f;
    }

    return items;
}

void PluginCanvas::loadRulers()
{
    const auto text = rulersFile().loadFileAsString();
    xRulers = text.contains ("x");
    yRulers = text.contains ("y");
    distances = text.contains ("d");
}

void PluginCanvas::saveRulers() const
{
    rulersFile().getParentDirectory().createDirectory();
    rulersFile().replaceWithText (juce::String (xRulers ? "x" : "") + (yRulers ? "y" : "") + (distances ? "d" : "") + "-");
}

void PluginCanvas::drawRulers (juce::Graphics& g)
{
    if (! design || cropping)
        return;

    const auto window = windowArea();

    // The 3D body, moved: pink where its middle meets the window's (both: a cross).
    if (drag == Drag::moveBody && dragMoved && (bodyCentredX || bodyCentredY))
    {
        const juce::Colour pink (0xffff3ea5);
        const float cx = toView (juce::Rectangle<int> (0, 0, layout.width, layout.height)).getCentreX();
        const float cy = toView (juce::Rectangle<int> (0, 0, layout.width, layout.height)).getCentreY();
        g.setColour (pink);

        if (bodyCentredX)
            g.drawLine (cx, window.getY(), cx, window.getBottom(), 1.5f);

        if (bodyCentredY)
            g.drawLine (window.getX(), cy, window.getRight(), cy, 1.5f);

        if (bodyCentredX && bodyCentredY)
        {
            g.drawEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ cx, cy }), 2.0f);
            g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ cx, cy }));
        }
    }

    if (! xRulers && ! yRulers && ! distances)
        return;

    // What they follow: the chosen element (several: all of them together).
    juce::Rectangle<int> box;
    std::vector<int> chosen (manySelected.begin(), manySelected.end());

    if (chosen.empty() && juce::isPositiveAndBelow (selected, (int) layout.widgets.size()))
        chosen.push_back (selected);

    for (const auto index : chosen)
        if (juce::isPositiveAndBelow (index, (int) layout.widgets.size()) && isShown (layout.widgets[(size_t) index]))
            box = box.isEmpty() ? layout.widgets[(size_t) index].bounds : box.getUnion (layout.widgets[(size_t) index].bounds);

    if (box.isEmpty())
        return;

    const auto view = toView (box);
    const juce::Colour colour (0xff35c8ff);
    const float dashes[] { 5.0f, 4.0f };

    // Another element's edge on one of its lines: lit, and that element marked there.
    auto edgeMatches = [&] (int value, bool horizontal)
    {
        for (int i = 0; i < (int) layout.widgets.size(); ++i)
        {
            if (std::find (chosen.begin(), chosen.end(), i) != chosen.end() || ! isShown (layout.widgets[(size_t) i]))
                continue;

            const auto& b = layout.widgets[(size_t) i].bounds;

            if (horizontal ? (b.getY() == value || b.getBottom() == value) : (b.getX() == value || b.getRight() == value))
                return true;
        }

        return false;
    };

    auto rule = [&] (juce::Line<float> line, bool lit, const juce::String& label, juce::Point<float> labelAt)
    {
        if (lit)   // another element lines up with it: solid
        {
            g.setColour (colour);
            g.drawLine (line, 1.4f);
        }
        else
        {
            juce::Path path, dashed;
            path.startNewSubPath (line.getStart());
            path.lineTo (line.getEnd());
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, path, dashes, 2);
            g.setColour (colour.withAlpha (0.75f));
            g.fillPath (dashed);
        }

        g.setFont (Theme::font (10.5f, true));
        const auto tag = juce::Rectangle<float> ((float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), label) + 8.0f, 14.0f)
                             .withCentre (labelAt);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillRoundedRectangle (tag, 3.0f);
        g.setColour (lit ? colour : colour.withAlpha (0.85f));
        g.drawText (label, tag, juce::Justification::centred, false);
    };

    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (window.expanded (40.0f).toNearestInt());

    // How far it is from the 3D body's nearest edges (no body: the window's): red arrows across
    // the gap, left or right and above or below, whichever's nearer.
    if (distances)
    {
        const auto body = Body3D::hasParts (layout.body) ? bodyBounds() : juce::Rectangle<float> (0.0f, 0.0f, (float) layout.width, (float) layout.height);
        const auto edges = toView (body.getSmallestIntegerContainer());
        const juce::Colour red (0xffff3b3b);
        const auto b = box.toFloat();
        const float toLeft = b.getX() - body.getX(), toRight = body.getRight() - b.getRight();
        const float toTop = b.getY() - body.getY(), toBottom = body.getBottom() - b.getBottom();

        auto arrow = [&] (juce::Point<float> from, juce::Point<float> to, float pixels)
        {
            if (from.getDistanceFrom (to) < 1.0f)
                return;

            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawLine ({ from, to }, 3.0f);
            juce::Path line;
            line.addArrow ({ from, to }, 1.4f, 8.0f, 7.0f);
            line.addArrow ({ to, from }, 1.4f, 8.0f, 7.0f);
            g.setColour (red);
            g.fillPath (line);

            const auto label = juce::String (juce::roundToInt (pixels)) + " px";
            g.setFont (Theme::font (10.5f, true));
            const auto tag = juce::Rectangle<float> ((float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), label) + 8.0f, 14.0f)
                                 .withCentre ((from + to) * 0.5f + (std::abs (from.x - to.x) > std::abs (from.y - to.y) ? juce::Point<float> (0.0f, -10.0f)
                                                                                                                      : juce::Point<float> (24.0f, 0.0f)));
            g.setColour (juce::Colours::black.withAlpha (0.75f));
            g.fillRoundedRectangle (tag, 3.0f);
            g.setColour (red);
            g.drawText (label, tag, juce::Justification::centred, false);
        };

        // The edges it's measured from, faintly.
        {
            juce::Path outline, dotted;
            outline.addRectangle (edges);
            const float dots[] { 2.0f, 4.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (dotted, outline, dots, 2);
            g.setColour (red.withAlpha (0.45f));
            g.fillPath (dotted);
        }

        const float midY = view.getCentreY(), midX = view.getCentreX();

        if (std::abs (toLeft) <= std::abs (toRight))
            arrow ({ edges.getX(), midY }, { view.getX(), midY }, toLeft);
        else
            arrow ({ view.getRight(), midY }, { edges.getRight(), midY }, toRight);

        if (std::abs (toTop) <= std::abs (toBottom))
            arrow ({ midX, edges.getY() }, { midX, view.getY() }, toTop);
        else
            arrow ({ midX, view.getBottom() }, { midX, edges.getBottom() }, toBottom);
    }

    if (xRulers)
    {
        // Above it and under it, the window's width.
        for (const auto [y, value] : { std::pair<float, int> { view.getY(), box.getY() }, { view.getBottom(), box.getBottom() } })
            rule ({ window.getX() - 14.0f, y, window.getRight() + 14.0f, y }, edgeMatches (value, true), juce::String (value),
                  { window.getX() - 26.0f, y });
    }

    if (yRulers)
    {
        // At its left and its right, the window's height.
        for (const auto [x, value] : { std::pair<float, int> { view.getX(), box.getX() }, { view.getRight(), box.getRight() } })
            rule ({ x, window.getY() - 6.0f, x, window.getBottom() + 14.0f }, edgeMatches (value, false), juce::String (value),
                  { x, window.getBottom() + 24.0f });
    }
}

void PluginCanvas::drawPageStrip (juce::Graphics& g)
{
    for (const auto& [item, box] : pageStrip())
    {
        if (item <= -2)
        {
            // A ruler's tick: a box and its words.
            const bool on = item == -2 ? xRulers : (item == -3 ? yRulers : distances);
            const auto tick = box.withWidth (14.0f).withSizeKeepingCentre (14.0f, 14.0f);
            g.setColour (on ? Theme::accent : Theme::inset);
            g.fillRoundedRectangle (tick, 3.0f);
            g.setColour (on ? Theme::accent.brighter (0.3f) : Theme::outline.brighter (0.3f));
            g.drawRoundedRectangle (tick.reduced (0.5f), 3.0f, 1.0f);

            if (on)
            {
                juce::Path check;
                check.startNewSubPath (tick.getX() + 3.0f, tick.getCentreY());
                check.lineTo (tick.getX() + 6.0f, tick.getBottom() - 3.5f);
                check.lineTo (tick.getRight() - 3.0f, tick.getY() + 3.5f);
                g.setColour (juce::Colours::white);
                g.strokePath (check, juce::PathStrokeType (1.8f));
            }

            g.setColour (on ? Theme::text : Theme::muted);
            g.setFont (Theme::font (12.0f, true));
            g.drawText (rulerTickName (item), box.withTrimmedLeft (20.0f), juce::Justification::centredLeft, false);
            continue;
        }

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
    if (item <= -2)
    {
        // A ruler's tick: on or off (remembered for next time).
        if (item == -2)      xRulers = ! xRulers;
        else if (item == -3) yRulers = ! yRulers;
        else                 distances = ! distances;

        saveRulers();
        repaint();
        return;
    }

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

    // A performance control moves what it moves: a wheel or a ribbon one way, the pair of
    // wheels the one grabbed (up and down), a lever, a stick or a pad both.
    if (PerformControl::has (w))
    {
        const auto axes = PerformControl::axesOf (PerformControl::designOf (w));
        const auto across = (plugin.x - pad.getX()) / pad.getWidth(), up = (pad.getBottom() - plugin.y) / pad.getHeight();

        auto move = [&] (const char* role, float proportion)
        {
            if (const auto found = w.roles.find (role); found != w.roles.end())
                if (const auto* p = paramById (found->second))
                    setValue (*p, valueAt (*p, proportion));
        };

        if (axes == "pair")      move (performPart == 1 ? "y" : "x", up);
        else if (axes == "y")    move ("y", up);
        else if (axes == "x")    move ("x", across);
        else                   { move ("x", across); move ("y", up); }

        return;
    }

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
    // A 3D look: as the panel camera sees it where it is on the panel.
    // ...standing on the 3D body's face there (a sloped one turns it).
    g.drawImage (looks->frame (look, kind, area.getWidth(), area.getHeight(), value, resolution, layout.cameraLean (area),
                               look.doc.model3d != nullptr ? faceSlope (area) : juce::Point<float>()),
                 area.expanded (room).toFloat());
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

    // A preset search being typed: its caret blinks (only the field is painted again).
    for (const auto& [index, state] : presetStates)
        if (state.searching && ! design && juce::isPositiveAndBelow (index, (int) layout.widgets.size()))
        {
            const auto area = presetPopup == index ? presetPopupArea (index) : layout.widgets[(size_t) index].bounds.toFloat();
            const auto field = PresetPanel::searchArea (PresetPanel::designOf (layout.widgets[(size_t) index]), area, presetView (index));
            repaint (toView (field.getSmallestIntegerContainer()).expanded (2.0f).getSmallestIntegerContainer());
        }

    // Assigning: the controls it may route to pulse.
    if (assigning.widget >= 0 && ! design)
    {
        const auto ids = matrixDestinationIds();

        for (const auto& w : layout.widgets)
            if (isShown (w) && w.type != GuiWidget::Type::matrix && w.param.isNotEmpty() && ids.contains (w.param))
                repaint (viewAreaOf (w).expanded (6));
    }

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

        drawCaption (g, w, w.label, area, 0.85f);
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
        drawCaption (g, w, w.label, b.withTop (b.getBottom() - (float) GuiLayout::captionHeight), 0.85f);
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
    // Faded: a control whose parameter the built plugin hasn't got (with nothing built yet, the
    // draft GUI say, none is missing: it's being designed)
    const bool missing = w.isControl() && p == nullptr && ! params.isEmpty() && w.command.isEmpty();
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

        drawCaption (g, w, text, area, showValue ? 1.0f : 0.85f, showValue, missing);
    };

    switch (w.type)
    {
        case GuiWidget::Type::group:
        {
            if (! still)
                break;

            g.setColour (juce::Colours::white.withAlpha (0.13f));
            g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
            PanelParts::drawStyledText (g, w, w.label, b.reduced (10.0f, 4.0f).withHeight (16.0f), juce::Justification::centredLeft,
                                        FontLibrary::font (w.font, w.fontSize > 0.0f ? w.fontSize : 11.5f, true),
                                        w.colour.isTransparent() ? captionColour.withAlpha (0.75f) : w.colour);
            break;
        }

        case GuiWidget::Type::label:
        {
            // Words are still; a readout shows its parameter's value, so it moves.
            if (PanelParts::labelMoves (w) ? ! moving : ! still)
                break;

            auto text = w.label;
            int cells = 0;

            if (p != nullptr && PanelParts::labelShowsValue (w))
            {
                text = valueText (value, *p);

                // LED digits keep their cells as the value changes: as many as its longest.
                if (PanelParts::labelDisplayOf (w) == "led")
                    for (int step = 0; step <= 8; ++step)
                        cells = juce::jmax (cells, SegmentDisplay::cellsOf (valueText (valueAt (*p, (float) step / 8.0f), *p)));
            }

            PanelParts::paintLabel (g, w, text, w.colour.isTransparent() ? captionColour : w.colour, w.fontSize, cells);
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
                PanelParts::drawStyledText (g, w, names[i], cell.reduced (6.0f, 0.0f), juce::Justification::centred,
                                            FontLibrary::font (w.font, size, i == shown || w.bold),
                                            // The shown tab's name reads on its colour: dark on a light one, white on a dark one.
                                            i == shown ? (accent.withAlpha (1.0f).getPerceivedBrightness() > 0.62f ? juce::Colour (0xff141416) : juce::Colours::white)
                                                       : (w.textColour.isTransparent() ? captionColour.withAlpha (0.8f) : w.textColour),
                                            true);
            }
            break;
        }

        case GuiWidget::Type::preset:
        {
            // A design of its own (PresetPanel): a drop list, a table, a side panel.
            if (PresetPanel::has (w))
            {
                if (moving)
                    drawPresetWidget (g, index);

                break;
            }

            // "<  Preset name  >": the whole widget moves (one frame per preset).
            if (! moving)
                break;

            const auto chosen = override >= 0.0f ? juce::roundToInt (override) : -1;
            const auto name = bareBars ? juce::String()
                            : juce::isPositiveAndBelow (chosen, presetMenu.names.size()) ? presetMenu.names[chosen] : presetMenu.currentText();

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

            PanelParts::drawStyledText (g, w, name, b.reduced (14.0f + arrow * 2.0f, 0.0f), juce::Justification::centred,
                                        FontLibrary::font (w.font, w.fontSize > 0.0f ? w.fontSize : juce::jmin (14.0f, b.getHeight() * 0.5f), true),
                                        w.textColour.isTransparent() ? captionColour : w.textColour, true);
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

            // A performance control (PerformControl): its picture for where it is.
            if (PerformControl::has (w))
            {
                if (moving)
                {
                    const auto resolution = baking ? 1.0f : (g.getInternalContext().getPhysicalPixelScaleFactor() > 1.3f ? 2.0f : 1.0f);
                    PerformControl::paint (g, PerformControl::designOf (w), pad, x, y, layout.lightAngle, layout.lightAltitude, resolution);
                }

                if (still)
                    drawCaption (g, w, w.label, b.withTop (pad.getBottom()), 0.85f);

                break;
            }
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

                drawCaption (g, w, w.label, b.withTop (pad.getBottom()), 0.85f);
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
            // A command button (Save, Load...): a key in its colour with its name on it, dark on
            // a light colour and white on a dark one, brighter while it's held.
            if (w.type == GuiWidget::Type::toggle && w.command.isNotEmpty())
            {
                if (moving)
                {
                    const auto held = commandHeld == index;
                    const auto key = b.reduced (1.0f).translated (0.0f, held ? 1.0f : 0.0f);
                    const auto face = held ? accent.brighter (0.25f) : accent;
                    const auto corner = w.rounding >= 0.0f ? juce::jlimit (0.0f, 1.0f, w.rounding) * juce::jmin (key.getWidth(), key.getHeight()) * 0.5f : 5.0f;

                    g.setColour (juce::Colours::black.withAlpha (0.45f));
                    g.fillRoundedRectangle (key.translated (0.0f, held ? 0.5f : 1.5f), corner);
                    g.setGradientFill (juce::ColourGradient (face.brighter (0.2f), key.getX(), key.getY(), face.darker (0.15f), key.getX(), key.getBottom(), false));
                    g.fillRoundedRectangle (key, corner);
                    g.setColour (juce::Colours::white.withAlpha (0.25f));
                    g.drawRoundedRectangle (key.reduced (0.5f), corner, 1.0f);

                    const auto ink = w.textColour.isTransparent() ? (face.withAlpha (1.0f).getPerceivedBrightness() > 0.62f ? juce::Colour (0xff141416) : juce::Colours::white)
                                                                  : w.textColour;
                    PanelParts::drawStyledText (g, w, w.label, key.reduced (4.0f, 0.0f), juce::Justification::centred,
                                                FontLibrary::font (w.font, w.fontSize > 0.0f ? w.fontSize : juce::jmin (13.0f, key.getHeight() * 0.48f), true), ink, true);
                }

                break;
            }

            // Its look, at the frame for its value (a switch: off or on). Unbound, it's faded.
            if (moving)
            {
                const auto position = w.type != GuiWidget::Type::toggle ? proportion
                                    : w.command.isNotEmpty() ? (commandHeld == index ? 1.0f : 0.0f)
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

            // A set of keys (SelectorSet): one lit, each by itself, or several at once.
            if (SelectorSet::has (w))
            {
                const auto keys = selectorKeys (w);
                const auto mode = SelectorSet::modeOf (w);
                std::vector<bool> lit ((size_t) keys, false);

                while (names.size() < keys)
                    names.add (juce::String (names.size() + 1));

                for (int i = 0; i < keys; ++i)
                {
                    if (mode == "toggles")
                    {
                        if (const auto* key = paramById (SelectorSet::keyParam (w, i)))
                            lit[(size_t) i] = proportionOf (*key, currentValue (*key)) >= 0.5f;
                    }
                    else if (mode == "multi")
                    {
                        const auto bits = override >= 0.0f ? juce::roundToInt (override) : (p != nullptr ? juce::roundToInt (value - p->min) : 0);
                        lit[(size_t) i] = ((bits >> i) & 1) != 0;
                    }
                    else
                    {
                        lit[(size_t) i] = i == chosen;
                    }
                }

                const auto resolution = baking ? 1.0f : (g.getInternalContext().getPhysicalPixelScaleFactor() > 1.3f ? 2.0f : 1.0f);
                SelectorSet::paint (g, SelectorSet::designOf (w), area, names, lit, layout.lightAngle, layout.lightAltitude, resolution);
                caption (b.withTop (b.getBottom() - (float) GuiLayout::captionHeight));
                break;
            }

            PanelParts::paintSelector (g, area, w, names, chosen, accent, w.textColour.isTransparent() ? captionColour : w.textColour);
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

        case GuiWidget::Type::matrix:
        {
            // A part lying on its own: a jack, a source chip, an assign button.
            if (ModMatrix::isPart (w))
            {
                if (moving)
                    drawMatrixPart (g, index);

                break;
            }

            // Its routes, as its parameters have them (a cable being plugged, while it is: one
            // taken past its edge is drawn over the whole window).
            if (moving)
            {
                auto view = matrixView (w);

                if (matrixDrag.widget == index && matrixDrag.from >= 0 && b.contains (matrixDrag.to))
                {
                    view.draggingFrom = matrixDrag.from;
                    view.draggingTo = matrixDrag.to - b.getPosition();
                }

                juce::Graphics::ScopedSaveState state (g);
                g.addTransform (juce::AffineTransform::translation (b.getX(), b.getY()));
                ModMatrix::paint (g, ModMatrix::designOf (w), b.withZeroOrigin(), view);
            }

            break;
        }
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
    float velocity = 0.0f;
    const auto note = PanelParts::keyboardNoteAt (w, plugin, velocity);   // flat or in 3D

    if (note == heldNote && widgetIndex == heldKeyboard)
        return;

    releaseKey();

    if (note < 0)
        return;

    heldNote = note;
    heldKeyboard = widgetIndex;

    if (onNote != nullptr)
        onNote (note, velocity);

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
        case GuiWidget::Type::matrix:    return w.bounds;
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

void PluginCanvas::drawPanel (juce::Graphics& g, const std::function<void (int)>& behind)
{
    const auto plugin = juce::Rectangle<float> (0.0f, 0.0f, (float) layout.width, (float) layout.height);
    g.setGradientFill (juce::ColourGradient (layout.backgroundTop, 0.0f, 0.0f, layout.backgroundBottom, 0.0f, plugin.getBottom(), false));
    g.fillRect (plugin);

    if (layout.backgroundImage.isNotEmpty())
        drawPicture (g, layout.backgroundImage, plugin, layout.backgroundMode);

    // What's been sent behind the 3D body (a table, a backdrop), then the body.
    if (behind != nullptr)
        for (int i = 0; i < (int) layout.widgets.size(); ++i)
            if (isBehindBody (i))
                behind (i);

    drawBody (g);
}

bool PluginCanvas::isBehindBody (int index) const
{
    if (! juce::isPositiveAndBelow (index, (int) layout.widgets.size()) || ! Body3D::hasParts (layout.body))
        return false;

    const auto& settings = layout.widgets[(size_t) index].settings;
    const auto found = settings.find ("behindBody");
    return found != settings.end() && (bool) found->second;
}

//==============================================================================
juce::String PluginCanvas::bodyShapeKeyNow() const
{
    if (! Body3D::hasParts (layout.body))
        return {};

    // The editor's own settings (its units and grid) don't change how it looks.
    auto shown = layout.body.clone();

    if (auto* object = shown.getDynamicObject())
        object->removeProperty ("editor");

    return juce::JSON::toString (shown, true) + "|" + juce::String (layout.width) + "x" + juce::String (layout.height);
}

juce::String PluginCanvas::bodyKeyNow() const
{
    const auto shape = bodyShapeKeyNow();

    if (shape.isEmpty())
        return {};

    return shape + "|" + juce::String (layout.lightAngle, 1) + "/" + juce::String (layout.lightAltitude, 1) + "|" + juce::String (layout.cameraDistance(), 1);
}

juce::Point<float> PluginCanvas::faceSlope (juce::Rectangle<int> area)
{
    if (! Body3D::hasParts (layout.body) || ! (bool) layout.body.getProperty ("follow", true))
        return {};

    // Export and pictures of the panel: the body as it is now. Otherwise the shape its
    // picture was last rendered from stands in until the new one's ready.
    if (const auto key = bodyShapeKeyNow(); key != bodyShapeKey && (baking || bodyShape == nullptr))
    {
        if (! baking && bodyPending.isNotEmpty())
            return {};

        bodyShape = Body3D::fromVar (layout.body, layout.width, layout.height, imagesFolder());
        bodyShapeKey = key;
        faceSlopes.clear();
    }

    if (bodyShape == nullptr)
        return {};

    const auto middle = area.getCentre();
    const auto where = ((juce::int64) middle.x << 32) | (juce::int64) (juce::uint32) middle.y;

    if (const auto found = faceSlopes.find (where); found != faceSlopes.end())
        return found->second;

    Render3D::Face face;
    juce::Point<float> slope;

    if (Render3D::bodyFace (*bodyShape, layout.cameraDistance(), (float) middle.x + 0.5f, (float) middle.y + 0.5f, face))
        slope = { face.slopeX, face.slopeZ };

    if (faceSlopes.size() > 4000)
        faceSlopes.clear();

    faceSlopes[where] = slope;
    return slope;
}

int PluginCanvas::selectorKeys (const GuiWidget& w) const
{
    if (w.options.size() > 0)
        return w.options.size();

    const auto* p = paramFor (w);

    if (p == nullptr)
        return 3;

    // Several at once: a key a bit of its value.
    if (SelectorSet::has (w) && SelectorSet::modeOf (w) == "multi")
        return juce::jlimit (1, 24, (int) std::ceil (std::log2 (juce::jmax (1.0f, p->max - p->min) + 1.0f)));

    return juce::jmax (1, (int) (p->max - p->min) + 1);
}

void PluginCanvas::editAdvanced (int widgetIndex)
{
    if (! design || ! hasPanel() || ! juce::isPositiveAndBelow (widgetIndex, (int) layout.widgets.size()))
        return;

    // The edit window, on its Advanced edit tab, for this one.
    menu->preferTab (ElementMenu::advancedTab);

    if (selected != widgetIndex || ! manySelected.empty() || panelSelected)
        select (widgetIndex);

    if (! menuOpen || menu->getWidgetIndex() != widgetIndex || ! menu->isAdvanced())
        openMenu();

    if (menuWindow != nullptr)
        menuWindow->toFront (true);
}

void PluginCanvas::releasePerform (int widgetIndex)
{
    if (! juce::isPositiveAndBelow (widgetIndex, (int) layout.widgets.size()))
        return;

    const auto& w = layout.widgets[(size_t) widgetIndex];

    // Pitch bend and modulation: the bend goes back to its middle, the modulation stays (a
    // lever: both go back, as a bender does).
    if (! PerformControl::has (w) || PerformControl::modeOf (w) != "bendmod")
        return;

    const bool both = PerformControl::springsBoth (PerformControl::designOf (w));

    for (const auto* axis : { "x", "y" })
        if (both || PerformControl::bendAxisOf (w) == axis)
            if (const auto found = w.roles.find (axis); found != w.roles.end())
                if (const auto* p = paramById (found->second))
                    setValue (*p, valueAt (*p, 0.5f));

    repaint();
}

ModMatrix::View PluginCanvas::matrixView (const GuiWidget& w) const
{
    ModMatrix::View view;
    const auto d = ModMatrix::designOf (w);
    view.sources = ModMatrix::sourcesOf (d);

    // Its destinations: the plugin's own parameters (the matrices' own aside).
    juce::StringArray ids;

    for (const auto& p : params)
        if (! ModMatrix::isMatrixParam (layout, p.id))
        {
            ids.add (p.id);
            view.destinations.add (p.name.isNotEmpty() ? p.name : p.id);
        }

    for (int i = 0; i < ModMatrix::routesOf (d); ++i)
    {
        ModMatrix::Route route;
        auto valueOf = [this, &w, i] (const char* part, float fallback)
        {
            const auto* p = paramById (ModMatrix::paramId (w, i, part));
            return p != nullptr ? currentValue (*p) : fallback;
        };

        route.source = juce::roundToInt (valueOf ("src", 0.0f));
        route.destination = juce::roundToInt (valueOf ("dst", 0.0f));
        route.amount = juce::jlimit (-1.0f, 1.0f, valueOf ("amt", 0.0f));
        view.routes.push_back (route);
    }

    // A grid's columns, a patch bay's inputs: the ones it names, else the first eight.
    if (const auto* targets = d.getProperty ("targets", {}).getArray(); targets != nullptr && ! targets->isEmpty())
    {
        for (const auto& t : *targets)
            if (const auto at = ids.indexOf (t.toString().trim()); at >= 0)
                view.columns.push_back (at + 1);
    }
    else
    {
        for (int i = 0; i < juce::jmin (8, ids.size()); ++i)
            view.columns.push_back (i + 1);
    }

    // A patch bay with matching inputs: one for every output, the parameters' first, then
    // inputs of their own (In 1, In 2...) for what's still to be wired.
    if (ModMatrix::styleOf (d) == "patch" && (bool) d.getProperty ("matchInputs", false))
    {
        const auto wanted = juce::jmax (1, view.sources.size());

        if ((int) view.columns.size() > wanted)
            view.columns.resize ((size_t) wanted);

        for (int n = 1; (int) view.columns.size() < wanted; ++n)
        {
            view.destinations.add ("In " + juce::String (n));
            view.columns.push_back (view.destinations.size());
        }
    }

    return view;
}

void PluginCanvas::setMatrixRoute (const GuiWidget& w, int route, int source, int destination, float amount)
{
    auto put = [this, &w, route] (const char* part, float value)
    {
        if (const auto* p = paramById (ModMatrix::paramId (w, route, part)))
            setValue (*p, juce::jlimit (p->min, p->max, value));
    };

    if (source >= 0)       put ("src", (float) source);
    if (destination >= 0)  put ("dst", (float) destination);
    if (amount > -2.0f)    put ("amt", amount);
    repaint();
}

void PluginCanvas::matrixPress (int index, juce::Point<float> plugin, bool popupMenu)
{
    const auto& w = layout.widgets[(size_t) index];
    const auto d = ModMatrix::designOf (w);
    const auto view = matrixView (w);
    const auto box = w.bounds.toFloat();
    const auto hit = ModMatrix::hitTest (d, box.withZeroOrigin(), view, plugin - box.getPosition());
    const auto style = ModMatrix::styleOf (d);
    const float newAmount = juce::jlimit (-1.0f, 1.0f, (float) (double) d.getProperty ("amount", style == "patch" ? 1.0 : 0.5));

    matrixDrag = {};
    matrixDrag.widget = index;
    matrixDrag.hit = hit;
    matrixDrag.start = plugin;
    matrixDrag.to = plugin;

    if (ModMatrix::isPart (d))
    {
        partPress (index, plugin, popupMenu);
        return;
    }

    auto freeRoute = [&view]
    {
        for (size_t i = 0; i < view.routes.size(); ++i)
            if (! view.routes[i].active())
                return (int) i;

        return -1;
    };

    using Kind = ModMatrix::Hit::Kind;

    switch (hit.kind)
    {
        case Kind::clear:
            setMatrixRoute (w, hit.route, 0, 0, -3.0f);
            break;

        case Kind::source:
        case Kind::destination:
        {
            // Picked from a list: the sources, or the plugin's parameters.
            const bool source = hit.kind == Kind::source;
            const auto& names = source ? view.sources : view.destinations;
            const auto current = source ? view.routes[(size_t) hit.route].source : view.routes[(size_t) hit.route].destination;
            juce::PopupMenu choices;
            choices.addItem (1, "None", true, current == 0);
            choices.addSeparator();

            for (int i = 0; i < names.size(); ++i)
                choices.addItem (i + 2, names[i], true, current == i + 1);

            choices.showMenuAsync (juce::PopupMenu::Options().withMousePosition(),
                                [safeThis = juce::Component::SafePointer<PluginCanvas> (this), index, route = hit.route, source] (int result)
                                {
                                    if (safeThis == nullptr || result == 0 || ! juce::isPositiveAndBelow (index, (int) safeThis->layout.widgets.size()))
                                        return;

                                    const auto& matrix = safeThis->layout.widgets[(size_t) index];
                                    safeThis->setMatrixRoute (matrix, route, source ? result - 1 : -1, source ? -1 : result - 1, -3.0f);
                                });
            matrixDrag.widget = -1;
            break;
        }

        case Kind::amount:
            matrixDrag.startAmount = view.routes[(size_t) hit.route].amount;
            break;

        case Kind::cell:
        {
            if (hit.route >= 0)
            {
                matrixDrag.startAmount = view.routes[(size_t) hit.route].amount;
                matrixDrag.remove = ! popupMenu;
            }
            else if (const int route = freeRoute(); route >= 0)
            {
                setMatrixRoute (w, route, hit.row + 1, view.columns[(size_t) hit.column], newAmount);
                matrixDrag.hit.route = route;
                matrixDrag.startAmount = newAmount;
            }
            break;
        }

        case Kind::output:
            matrixDrag.from = hit.row;
            matrixDrag.source = hit.row + 1;
            matrixDrag.anchor = ModMatrix::outputPoint (d, box.withZeroOrigin(), view, hit.row) + box.getPosition();
            matrixDrag.amount = newAmount;
            break;

        case Kind::input:
            // A cable pulled out of its input: plugged again elsewhere, or let go.
            if (hit.route >= 0)
            {
                matrixDrag.from = view.routes[(size_t) hit.route].source - 1;
                matrixDrag.startAmount = view.routes[(size_t) hit.route].amount;
                matrixDrag.source = matrixDrag.from + 1;
                matrixDrag.amount = matrixDrag.startAmount;

                if (juce::isPositiveAndBelow (matrixDrag.from, view.sources.size()))
                    matrixDrag.anchor = ModMatrix::outputPoint (d, box.withZeroOrigin(), view, matrixDrag.from) + box.getPosition();

                setMatrixRoute (w, hit.route, 0, 0, -3.0f);
            }
            break;

        case Kind::none:
        default:
            matrixDrag.widget = -1;
            break;
    }
}

void PluginCanvas::matrixDragTo (juce::Point<float> plugin)
{
    if (! juce::isPositiveAndBelow (matrixDrag.widget, (int) layout.widgets.size()))
        return;

    const auto& w = layout.widgets[(size_t) matrixDrag.widget];
    matrixDrag.to = plugin;
    matrixDrag.moved = matrixDrag.moved || plugin.getDistanceFrom (matrixDrag.start) > 3.0f;

    using Kind = ModMatrix::Hit::Kind;
    const auto& hit = matrixDrag.hit;

    // An amount: dragged across a slot's bar, up and down a grid's cell.
    if ((hit.kind == Kind::amount || hit.kind == Kind::cell) && hit.route >= 0 && matrixDrag.moved)
    {
        const float delta = hit.kind == Kind::amount ? (plugin.x - matrixDrag.start.x) / juce::jmax (40.0f, (float) w.bounds.getWidth() * 0.15f)
                                                     : (matrixDrag.start.y - plugin.y) / 80.0f;
        setMatrixRoute (w, hit.route, -1, -1, juce::jlimit (-1.0f, 1.0f, matrixDrag.startAmount + delta));
        matrixDrag.remove = false;
    }

    repaint();
}

void PluginCanvas::matrixRelease()
{
    if (! juce::isPositiveAndBelow (matrixDrag.widget, (int) layout.widgets.size()))
    {
        matrixDrag = {};
        return;
    }

    const auto& w = layout.widgets[(size_t) matrixDrag.widget];
    using Kind = ModMatrix::Hit::Kind;

    if (ModMatrix::isPart (w))
    {
        partRelease();
        return;
    }

    // A pin clicked (not dragged): taken out.
    if (matrixDrag.hit.kind == Kind::cell && matrixDrag.remove && ! matrixDrag.moved && matrixDrag.hit.route >= 0)
        setMatrixRoute (w, matrixDrag.hit.route, 0, 0, -3.0f);

    // A cable let go over an input: plugged in, on a free route.
    if (matrixDrag.from >= 0)
    {
        const auto d = ModMatrix::designOf (w);
        const auto view = matrixView (w);
        const auto box = w.bounds.toFloat();
        const auto column = ModMatrix::inputNear (d, box.withZeroOrigin(), view, matrixDrag.to - box.getPosition());

        if (column >= 0)
        {
            int route = -1;

            for (size_t i = 0; i < view.routes.size() && route < 0; ++i)
                if (! view.routes[i].active())
                    route = (int) i;

            if (route >= 0)
            {
                const float amount = matrixDrag.hit.kind == Kind::input ? matrixDrag.startAmount
                                                                        : juce::jlimit (-1.0f, 1.0f, (float) (double) d.getProperty ("amount", 1.0));
                setMatrixRoute (w, route, matrixDrag.from + 1, view.columns[(size_t) column], amount);
            }
        }
        else if (! box.contains (matrixDrag.to))
        {
            // Past its edge: into a jack lying on its own, another patch bay, any control.
            if (const auto destination = modTargetAt (matrixDrag.to, matrixBase (w)); destination > 0)
                routeSource (matrixDrag.widget, matrixDrag.from + 1, destination, matrixDrag.amount);
        }
    }

    matrixDrag = {};
    repaint();
}

void PluginCanvas::matrixWheel (int index, juce::Point<float> plugin, float delta)
{
    const auto& w = layout.widgets[(size_t) index];
    const auto view = matrixView (w);
    const auto box = w.bounds.toFloat();
    const auto hit = ModMatrix::hitTest (ModMatrix::designOf (w), box.withZeroOrigin(), view, plugin - box.getPosition());

    if (juce::isPositiveAndBelow (hit.route, (int) view.routes.size()) && view.routes[(size_t) hit.route].active())
        setMatrixRoute (w, hit.route, -1, -1, juce::jlimit (-1.0f, 1.0f, view.routes[(size_t) hit.route].amount + delta * 0.25f));
}

//==============================================================================
juce::String PluginCanvas::matrixBase (const GuiWidget& w)
{
    return w.param.isNotEmpty() ? w.param : juce::String ("mod");
}

std::vector<int> PluginCanvas::matrixHomes() const
{
    std::vector<int> homes;
    juce::StringArray bases;

    for (int i = 0; i < (int) layout.widgets.size(); ++i)
        if (const auto& w = layout.widgets[(size_t) i]; w.type == GuiWidget::Type::matrix && ! bases.contains (matrixBase (w)))
        {
            bases.add (matrixBase (w));
            homes.push_back (matrixHomeOf (w));
        }

    return homes;
}

int PluginCanvas::matrixHomeOf (const GuiWidget& matrix) const
{
    // The whole matrix of its <param>, else the first of its parts.
    const auto base = matrixBase (matrix);
    int first = -1;

    for (int i = 0; i < (int) layout.widgets.size(); ++i)
    {
        const auto& w = layout.widgets[(size_t) i];

        if (w.type != GuiWidget::Type::matrix || matrixBase (w) != base)
            continue;

        if (! ModMatrix::isPart (w))
            return i;

        if (first < 0)
            first = i;
    }

    return first;
}

juce::StringArray PluginCanvas::matrixDestinationIds() const
{
    juce::StringArray ids;

    for (const auto& p : params)
        if (! ModMatrix::isMatrixParam (layout, p.id))
            ids.add (p.id);

    return ids;
}

int PluginCanvas::destinationOf (const GuiWidget& w) const
{
    if (w.type == GuiWidget::Type::matrix || w.param.isEmpty() || paramFor (w) == nullptr)
        return 0;

    return matrixDestinationIds().indexOf (w.param) + 1;
}

std::map<int, std::vector<PluginCanvas::Modulation>> PluginCanvas::allModulations() const
{
    std::map<int, std::vector<Modulation>> found;

    for (const int home : matrixHomes())
    {
        const auto& w = layout.widgets[(size_t) home];
        const auto d = ModMatrix::designOf (w);
        const auto view = matrixView (w);

        for (size_t r = 0; r < view.routes.size(); ++r)
            if (view.routes[r].active())
                found[view.routes[r].destination].push_back ({ home, (int) r, view.routes[r].source, view.routes[r].amount, ModMatrix::cableColour (d, (int) r) });
    }

    return found;
}

std::vector<PluginCanvas::Modulation> PluginCanvas::modulationsOf (int destination) const
{
    if (destination <= 0)
        return {};

    auto all = allModulations();
    return all.count (destination) > 0 ? all[destination] : std::vector<Modulation>();
}

float PluginCanvas::newAmountOf (const GuiWidget& w) const
{
    const auto d = ModMatrix::designOf (w);
    return juce::jlimit (-1.0f, 1.0f, (float) (double) d.getProperty ("amount", ModMatrix::styleOf (d) == "patch" ? 1.0 : 0.5));
}

bool PluginCanvas::routeSource (int home, int source, int destination, float amount)
{
    if (! juce::isPositiveAndBelow (home, (int) layout.widgets.size()) || source <= 0 || destination <= 0)
        return false;

    const auto& w = layout.widgets[(size_t) home];
    const auto view = matrixView (w);
    int route = -1;

    // The same source and destination again: that route (its amount now this).
    for (size_t i = 0; i < view.routes.size() && route < 0; ++i)
        if (view.routes[i].source == source && view.routes[i].destination == destination)
            route = (int) i;

    for (size_t i = 0; i < view.routes.size() && route < 0; ++i)
        if (! view.routes[i].active())
            route = (int) i;

    if (route < 0)
        return false;   // every route taken

    setMatrixRoute (w, route, source, destination, amount);
    return true;
}

int PluginCanvas::modTargetAt (juce::Point<float> plugin, const juce::String& base, int* control) const
{
    if (control != nullptr)
        *control = -1;

    const auto ids = matrixDestinationIds();

    // An input jack of this matrix's (lying on its own), a patch bay's input...
    for (int i = (int) layout.widgets.size(); --i >= 0;)
    {
        const auto& w = layout.widgets[(size_t) i];

        if (w.type != GuiWidget::Type::matrix || matrixBase (w) != base || ! isShown (w))
            continue;

        const auto d = ModMatrix::designOf (w);
        const auto box = w.bounds.toFloat();

        if (ModMatrix::isPart (d))
        {
            if (d.getProperty ("style", {}).toString().trim().equalsIgnoreCase ("jack") && ModMatrix::roleOf (d) == "in"
                && box.expanded (6.0f).contains (plugin))
                return ids.indexOf (ModMatrix::targetOf (d)) + 1;

            continue;
        }

        if (ModMatrix::styleOf (d) == "patch" && box.expanded (12.0f).contains (plugin))
        {
            const auto view = matrixView (w);

            if (const auto column = ModMatrix::inputNear (d, box.withZeroOrigin(), view, plugin - box.getPosition()); column >= 0)
                return view.columns[(size_t) column];
        }
    }

    // ...else any control of the plugin's own parameters.
    const auto index = widgetAt (plugin, true);

    if (index < 0)
        return 0;

    const auto destination = destinationOf (layout.widgets[(size_t) index]);

    if (destination > 0 && control != nullptr)
        *control = index;

    return destination;
}

//==============================================================================
void PluginCanvas::drawMatrixPart (juce::Graphics& g, int index)
{
    const auto& w = layout.widgets[(size_t) index];
    const auto d = ModMatrix::designOf (w);
    const auto home = matrixHomeOf (w);
    const auto view = matrixView (layout.widgets[(size_t) juce::jmax (0, home)]);
    const auto style = d.getProperty ("style", {}).toString().trim().toLowerCase();
    const auto source = ModMatrix::sourceOf (d);
    juce::String label;
    bool lit = false;

    if (style == "jack" && ModMatrix::roleOf (d) == "in")
    {
        const auto target = ModMatrix::targetOf (d);
        const auto* p = paramById (target);
        label = w.label.isNotEmpty() ? w.label : p != nullptr ? (p->name.isNotEmpty() ? p->name : p->id) : juce::String ("In");
        const auto destination = matrixDestinationIds().indexOf (target) + 1;

        for (const auto& r : view.routes)
            lit = lit || (r.active() && r.destination == destination);
    }
    else
    {
        label = w.label.isNotEmpty() ? w.label : juce::isPositiveAndBelow (source - 1, view.sources.size()) ? view.sources[source - 1] : "Source " + juce::String (source);

        if (style == "assign")
            lit = assigning.widget == index;
        else if (style == "chip")
            lit = (matrixDrag.widget == index && matrixDrag.moved) || (assigning.widget == index);
        else
            for (const auto& r : view.routes)
                lit = lit || (r.active() && r.source == source);
    }

    ModMatrix::paintPart (g, d, w.bounds.toFloat(), label, lit);
}

void PluginCanvas::drawMatrixOverlay (juce::Graphics& g)
{
    const auto homes = matrixHomes();

    if (homes.empty())
        return;

    const auto ids = matrixDestinationIds();

    for (const int home : homes)
    {
        const auto& hw = layout.widgets[(size_t) home];
        const auto base = matrixBase (hw);
        const auto d = ModMatrix::designOf (hw);
        const auto view = matrixView (hw);
        const float sag = juce::jlimit (0.0f, 100.0f, (float) (double) d.getProperty ("sag", 40.0)) * 0.01f;
        float thickness = 4.0f;

        // Where each source's cable starts and each destination's ends: a jack lying on its
        // own first, else a patch bay's.
        struct End { int bay = -1; juce::Point<float> at; };
        std::map<int, End> outs, ins;
        std::set<std::pair<int, int>> bayOuts, bayIns;   // (bay, source), (bay, destination)

        for (int i = 0; i < (int) layout.widgets.size(); ++i)
        {
            const auto& w = layout.widgets[(size_t) i];

            if (w.type != GuiWidget::Type::matrix || matrixBase (w) != base || ! isShown (w))
                continue;

            const auto wd = ModMatrix::designOf (w);
            const auto box = w.bounds.toFloat();

            if (ModMatrix::isPart (wd))
            {
                if (! wd.getProperty ("style", {}).toString().trim().equalsIgnoreCase ("jack"))
                    continue;

                const auto at = ModMatrix::partPoint (wd, box);
                thickness = juce::jmax (thickness, juce::jmin (box.getWidth(), box.getHeight() * 0.62f) * 0.27f);

                if (ModMatrix::roleOf (wd) == "out")
                    outs[ModMatrix::sourceOf (wd)] = { -1, at };
                else if (const auto destination = ids.indexOf (ModMatrix::targetOf (wd)) + 1; destination > 0)
                    ins[destination] = { -1, at };
            }
            else if (ModMatrix::styleOf (wd) == "patch")
            {
                const auto v = matrixView (w);
                const auto zero = box.withZeroOrigin();

                for (int s = 0; s < v.sources.size(); ++s)
                {
                    bayOuts.insert ({ i, s + 1 });

                    if (outs.count (s + 1) == 0)
                        outs[s + 1] = { i, ModMatrix::outputPoint (wd, zero, v, s) + box.getPosition() };
                }

                for (size_t c = 0; c < v.columns.size(); ++c)
                {
                    bayIns.insert ({ i, v.columns[c] });

                    if (ins.count (v.columns[c]) == 0)
                        ins[v.columns[c]] = { i, ModMatrix::inputPoint (wd, zero, v, (int) c) + box.getPosition() };
                }
            }
        }

        // The cables across the window (one a patch bay shows both ends of is its own).
        for (size_t r = 0; r < view.routes.size(); ++r)
        {
            const auto& route = view.routes[r];

            if (! route.active() || outs.count (route.source) == 0 || ins.count (route.destination) == 0)
                continue;

            bool bays = false;

            for (const auto& [bay, source] : bayOuts)
                bays = bays || (source == route.source && bayIns.count ({ bay, route.destination }) > 0);

            if (bays)
                continue;

            const auto& from = outs[route.source];
            const auto& to = ins[route.destination];

            ModMatrix::paintCable (g, from.at, to.at, thickness, sag, ModMatrix::cableColour (d, (int) r));
        }

        // A cable being plugged from a part (or out of a patch bay, past its edge).
        if (juce::isPositiveAndBelow (matrixDrag.widget, (int) layout.widgets.size()) && matrixDrag.moved
            && matrixBase (layout.widgets[(size_t) matrixDrag.widget]) == base && matrixDrag.source > 0)
        {
            int free = 0;

            while (free < (int) view.routes.size() - 1 && view.routes[(size_t) free].active())
                ++free;

            const auto colour = ModMatrix::cableColour (d, free);
            int control = -1;

            if (modTargetAt (matrixDrag.to, base, &control) > 0 && control >= 0)
            {
                const auto area = layout.widgets[(size_t) control].bounds.toFloat().expanded (3.0f);
                g.setColour (colour.withAlpha (0.25f));
                g.fillRoundedRectangle (area, 6.0f);
                g.setColour (colour);
                g.drawRoundedRectangle (area, 6.0f, 2.0f);
            }

            if (ModMatrix::styleOf (ModMatrix::designOf (layout.widgets[(size_t) matrixDrag.widget])) != "patch"
                || ! layout.widgets[(size_t) matrixDrag.widget].bounds.toFloat().contains (matrixDrag.to))
                ModMatrix::paintCable (g, matrixDrag.anchor, matrixDrag.to, thickness, sag, colour.withAlpha (0.85f));
        }
    }

    // Each control modulated: a ring a route, its amount from its value.
    const auto modulations = allModulations();

    for (const auto& w : layout.widgets)
    {
        if ((w.type != GuiWidget::Type::knob && w.type != GuiWidget::Type::slider) || ! isShown (w))
            continue;

        const auto* p = paramFor (w);
        const auto destination = p != nullptr ? ids.indexOf (w.param) + 1 : 0;

        if (destination <= 0)
            continue;

        const auto found = modulations.find (destination);

        if (found == modulations.end())
            continue;

        const auto& mods = found->second;
        const auto value = proportionOf (*p, currentValue (*p));
        const bool knob = w.type == GuiWidget::Type::knob;
        const auto box = knob ? knobSquare (w).toFloat() : movingAreaOf (w).toFloat().reduced (4.0f);

        for (size_t i = 0; i < mods.size(); ++i)
            ModMatrix::paintRing (g, box, knob, w.vertical, value, mods[i].amount, mods[i].colour, (int) i);
    }

    // Assigning: the controls it may route to, pulsing.
    if (! design && juce::isPositiveAndBelow (assigning.widget, (int) layout.widgets.size()))
    {
        const auto colour = ModMatrix::cableColour (ModMatrix::designOf (layout.widgets[(size_t) assigning.widget]), 0);
        const auto accent = ModMatrix::designOf (layout.widgets[(size_t) assigning.widget]).getProperty ("colour", {}).toString();
        const auto glow = accent.isNotEmpty() ? juce::Colour::fromString (accent) : colour;
        const auto pulse = 0.5f + 0.5f * std::sin ((float) juce::Time::getMillisecondCounterHiRes() * 0.006f);

        for (const auto& w : layout.widgets)
            if (isShown (w) && w.type != GuiWidget::Type::matrix && w.param.isNotEmpty() && ids.contains (w.param))
            {
                const auto area = w.bounds.toFloat().expanded (3.0f);
                g.setColour (glow.withAlpha (0.08f + 0.12f * pulse));
                g.fillRoundedRectangle (area, 6.0f);
                g.setColour (glow.withAlpha (0.45f + 0.5f * pulse));
                g.drawRoundedRectangle (area, 6.0f, 1.5f);
            }
    }
}

//==============================================================================
juce::Point<float> PluginCanvas::sourceEnd (const juce::String& base, int source, juce::Point<float> fallback) const
{
    // A source's output jack lying on its own, else a patch bay's.
    std::optional<juce::Point<float>> bay;

    for (const auto& w : layout.widgets)
    {
        if (w.type != GuiWidget::Type::matrix || matrixBase (w) != base || ! isShown (w))
            continue;

        const auto d = ModMatrix::designOf (w);

        if (ModMatrix::isPart (d))
        {
            if (d.getProperty ("style", {}).toString().trim().equalsIgnoreCase ("jack") && ModMatrix::roleOf (d) == "out" && ModMatrix::sourceOf (d) == source)
                return ModMatrix::partPoint (d, w.bounds.toFloat());
        }
        else if (! bay.has_value() && ModMatrix::styleOf (d) == "patch")
        {
            const auto view = matrixView (w);

            if (juce::isPositiveAndBelow (source - 1, view.sources.size()))
                bay = ModMatrix::outputPoint (d, w.bounds.toFloat().withZeroOrigin(), view, source - 1) + w.bounds.toFloat().getPosition();
        }
    }

    return bay.value_or (fallback);
}

void PluginCanvas::partPress (int index, juce::Point<float> plugin, bool popupMenu)
{
    const auto& w = layout.widgets[(size_t) index];
    const auto d = ModMatrix::designOf (w);
    const auto style = d.getProperty ("style", {}).toString().trim().toLowerCase();
    const auto homeIndex = matrixHomeOf (w);
    const auto& home = layout.widgets[(size_t) juce::jmax (0, homeIndex)];
    const auto view = matrixView (home);
    const auto source = ModMatrix::sourceOf (d);
    juce::ignoreUnused (plugin);

    // Taken out at once (a right-click): every cable from this source, or into this input.
    auto removeWhere = [this, &home, &view] (auto&& matches)
    {
        for (size_t r = 0; r < view.routes.size(); ++r)
            if (view.routes[r].active() && matches (view.routes[r]))
                setMatrixRoute (home, (int) r, 0, 0, 0.0f);
    };

    // An assign button: on and off.
    if (style == "assign")
    {
        matrixDrag = {};

        if (popupMenu)
            removeWhere ([source] (const ModMatrix::Route& r) { return r.source == source; });
        else if (assigning.widget == index)
            endAssigning();
        else
        {
            assigning = { index, source };
            repaint();
        }

        return;
    }

    // An input jack: its last cable pulled out (still hanging from its source).
    if (style == "jack" && ModMatrix::roleOf (d) == "in")
    {
        const auto destination = matrixDestinationIds().indexOf (ModMatrix::targetOf (d)) + 1;

        if (popupMenu)
        {
            removeWhere ([destination] (const ModMatrix::Route& r) { return r.destination == destination; });
            matrixDrag = {};
            return;
        }

        int last = -1;

        for (size_t r = 0; r < view.routes.size(); ++r)
            if (view.routes[r].active() && view.routes[r].destination == destination)
                last = (int) r;

        if (destination <= 0 || last < 0)
        {
            matrixDrag = {};
            return;
        }

        const auto& route = view.routes[(size_t) last];
        matrixDrag.source = route.source;
        matrixDrag.amount = route.amount;
        matrixDrag.hit.column = destination;    // a click (no drag) plugs it back
        matrixDrag.anchor = sourceEnd (matrixBase (w), route.source, ModMatrix::partPoint (d, w.bounds.toFloat()));
        setMatrixRoute (home, last, 0, 0, -3.0f);
        return;
    }

    // An output jack, a source chip: a cable from its source.
    if (popupMenu)
    {
        removeWhere ([source] (const ModMatrix::Route& r) { return r.source == source; });
        matrixDrag = {};
        return;
    }

    matrixDrag.source = source;
    matrixDrag.amount = d.hasProperty ("amount") ? newAmountOf (w) : newAmountOf (home);
    matrixDrag.anchor = ModMatrix::partPoint (d, w.bounds.toFloat());
}

void PluginCanvas::partRelease()
{
    const auto index = matrixDrag.widget;
    const auto& w = layout.widgets[(size_t) index];
    const auto d = ModMatrix::designOf (w);
    const auto style = d.getProperty ("style", {}).toString().trim().toLowerCase();
    const auto home = matrixHomeOf (w);

    if (matrixDrag.source > 0)
    {
        if (! matrixDrag.moved)
        {
            // A chip clicked: assigning from its source (again: no more); an input jack
            // clicked: its cable back in.
            if (style == "chip")
            {
                if (assigning.widget == index)
                    endAssigning();
                else
                    assigning = { index, matrixDrag.source };
            }
            else if (matrixDrag.hit.column > 0)
                routeSource (home, matrixDrag.source, matrixDrag.hit.column, matrixDrag.amount);
        }
        else if (const auto destination = modTargetAt (matrixDrag.to, matrixBase (w)); destination > 0)
        {
            routeSource (home, matrixDrag.source, destination, matrixDrag.amount);
        }
    }

    matrixDrag = {};
    repaint();
}

bool PluginCanvas::modulationPress (int index, juce::Point<float> plugin, const juce::ModifierKeys& mods)
{
    juce::ignoreUnused (plugin);
    amountDrag = {};

    if (! juce::isPositiveAndBelow (index, (int) layout.widgets.size()))
        return false;

    const auto& w = layout.widgets[(size_t) index];
    const auto destination = destinationOf (w);

    if (destination <= 0)
        return false;

    // Assigning: the control clicked routed from its source (a drag sets how much).
    if (juce::isPositiveAndBelow (assigning.widget, (int) layout.widgets.size()))
    {
        const auto& part = layout.widgets[(size_t) assigning.widget];
        const auto home = matrixHomeOf (part);

        if (home >= 0 && routeSource (home, assigning.source, destination, newAmountOf (part)))
            for (const auto& m : modulationsOf (destination))
                if (m.home == home && m.source == assigning.source)
                    amountDrag = { home, m.route, m.amount, plugin.y };

        repaint();
        return true;
    }

    const auto found = modulationsOf (destination);

    if (found.empty())
        return false;

    // Its modulation's menu: each route's amount and a way out.
    if (mods.isPopupMenu())
    {
        juce::PopupMenu choices;
        choices.addSectionHeader ("Modulation");

        for (size_t i = 0; i < found.size(); ++i)
        {
            const auto view = matrixView (layout.widgets[(size_t) found[i].home]);
            const auto name = juce::isPositiveAndBelow (found[i].source - 1, view.sources.size()) ? view.sources[found[i].source - 1] : "Source " + juce::String (found[i].source);
            choices.addColouredItem ((int) i + 1, "Remove " + name + " (" + (found[i].amount >= 0 ? "+" : "") + juce::String (juce::roundToInt (found[i].amount * 100.0f)) + "%)",
                                  found[i].colour, true, false);
        }

        if (found.size() > 1)
        {
            choices.addSeparator();
            choices.addItem (1000, "Remove all its modulation");
        }

        choices.showMenuAsync (juce::PopupMenu::Options().withMousePosition(),
                            [safeThis = juce::Component::SafePointer<PluginCanvas> (this), found] (int result)
                            {
                                if (safeThis == nullptr || result <= 0)
                                    return;

                                for (size_t i = 0; i < found.size(); ++i)
                                    if ((result == 1000 || result == (int) i + 1) && juce::isPositiveAndBelow (found[i].home, (int) safeThis->layout.widgets.size()))
                                        safeThis->setMatrixRoute (safeThis->layout.widgets[(size_t) found[i].home], found[i].route, 0, 0, 0.0f);
                            });
        return true;
    }

    // Alt + drag: its (last) modulation's amount.
    if (mods.isAltDown())
    {
        amountDrag = { found.back().home, found.back().route, found.back().amount, plugin.y };
        return true;
    }

    return false;
}

bool PluginCanvas::modulationWheel (int index, float delta)
{
    if (! juce::isPositiveAndBelow (index, (int) layout.widgets.size()))
        return false;

    const auto found = modulationsOf (destinationOf (layout.widgets[(size_t) index]));

    if (found.empty())
        return false;

    setMatrixRoute (layout.widgets[(size_t) found.back().home], found.back().route, -1, -1, juce::jlimit (-1.0f, 1.0f, found.back().amount + delta * 0.1f));
    return true;
}

void PluginCanvas::endAssigning()
{
    if (assigning.widget >= 0)
    {
        assigning = {};
        repaint();
    }
}

//==============================================================================
PluginCanvas::PresetState& PluginCanvas::presetState (int index)
{
    if (const auto found = presetStates.find (index); found != presetStates.end())
        return found->second;

    auto& state = presetStates[index];

    if (juce::isPositiveAndBelow (index, (int) layout.widgets.size()))
        state.sideOpen = PresetPanel::startsOpen (PresetPanel::designOf (layout.widgets[(size_t) index]));

    return state;
}

PresetPanel::View PluginCanvas::presetView (int index) const
{
    // The plugin's presets; a panel with none yet (KnobMaster, a plugin not built yet): made-up ones.
    auto view = presetMenu.names.isEmpty() ? PresetPanel::sampleView() : PresetPanel::View();

    if (presetMenu.names.isEmpty())
    {
        view.current = juce::jlimit (0, view.names.size() - 1, demoPreset);
    }
    else
    {
        view.names = presetMenu.names;
        view.groups = presetMenu.groups;

        while (view.groups.size() < view.names.size())
            view.groups.add ({});

        view.current = presetMenu.current;
        view.modified = presetMenu.modified;
    }

    if (const auto found = presetStates.find (index); found != presetStates.end())
    {
        view.page = found->second.page;
        view.tab = found->second.tab;
        view.search = found->second.search;
        view.searching = found->second.searching && ! design;
    }

    return view;
}

bool PluginCanvas::sideOpen (int index) const
{
    if (! juce::isPositiveAndBelow (index, (int) layout.widgets.size()))
        return false;

    const auto d = PresetPanel::designOf (layout.widgets[(size_t) index]);

    if (design || ! PresetPanel::toggles (d))
        return true;

    const auto found = presetStates.find (index);
    return found != presetStates.end() ? found->second.sideOpen : PresetPanel::startsOpen (d);
}

void PluginCanvas::syncSideModules()
{
    for (auto& w : layout.widgets)
        if (PresetPanel::has (w))
            if (const auto d = PresetPanel::designOf (w); PresetPanel::isSide (d))
                w.bounds = PresetPanel::moduleArea (d, layout.width, layout.height);
}

juce::Rectangle<int> PluginCanvas::windowBounds() const
{
    juce::Rectangle<int> window (0, 0, layout.width, layout.height);

    for (int i = 0; i < (int) layout.widgets.size(); ++i)
    {
        const auto& w = layout.widgets[(size_t) i];

        if (PresetPanel::has (w) && PresetPanel::isSide (PresetPanel::designOf (w)) && isShown (w) && sideOpen (i))
            window = window.getUnion (PresetPanel::moduleArea (PresetPanel::designOf (w), layout.width, layout.height));
    }

    return window;
}

juce::Rectangle<float> PluginCanvas::windowArea() const
{
    return toView (windowBounds());
}

juce::Rectangle<float> PluginCanvas::presetPopupArea (int index) const
{
    const auto& w = layout.widgets[(size_t) index];
    return PresetPanel::popupArea (PresetPanel::designOf (w), w.bounds.toFloat(), windowBounds().toFloat(), presetView (index));
}

void PluginCanvas::drawPresetWidget (juce::Graphics& g, int index)
{
    const auto& w = layout.widgets[(size_t) index];
    const auto d = PresetPanel::designOf (w);
    const auto style = PresetPanel::styleOf (d);
    const auto view = presetView (index);
    const float resolution = juce::jlimit (1.0f, 3.0f, zoom);

    if (style == "side")
    {
        if (sideOpen (index))
            PresetPanel::paintBody (g, d, w.bounds.toFloat(), view, layout.lightAngle, layout.lightAltitude, resolution);
    }
    else if (style == "table" && ! PresetPanel::opensPopup (d))
    {
        PresetPanel::paintBody (g, d, w.bounds.toFloat(), view, layout.lightAngle, layout.lightAltitude, resolution);
    }
    else
    {
        PresetPanel::paintBar (g, d, w.bounds.toFloat(), view, layout.lightAngle, layout.lightAltitude, resolution);
    }
}

void PluginCanvas::drawPresetOverlay (juce::Graphics& g)
{
    // The handles that open and close side panels, on the window's edges.
    for (int i = 0; i < (int) layout.widgets.size(); ++i)
    {
        const auto& w = layout.widgets[(size_t) i];

        if (! PresetPanel::has (w) || ! isShown (w))
            continue;

        if (const auto d = PresetPanel::designOf (w); PresetPanel::isSide (d) && PresetPanel::toggles (d))
            PresetPanel::paintHandle (g, d, PresetPanel::handleArea (d, layout.width, layout.height), sideOpen (i));
    }

    // The list or the table that's open, over everything.
    if (! design && juce::isPositiveAndBelow (presetPopup, (int) layout.widgets.size()) && PresetPanel::has (layout.widgets[(size_t) presetPopup]))
        PresetPanel::paintBody (g, PresetPanel::designOf (layout.widgets[(size_t) presetPopup]), presetPopupArea (presetPopup), presetView (presetPopup),
                                layout.lightAngle, layout.lightAltitude, juce::jlimit (1.0f, 3.0f, zoom));
}

bool PluginCanvas::presetPress (juce::Point<float> plugin, bool popupMenu)
{
    juce::ignoreUnused (popupMenu);

    // The list or the table that's open: what's clicked in it; a click elsewhere closes it.
    if (juce::isPositiveAndBelow (presetPopup, (int) layout.widgets.size()))
    {
        const auto index = presetPopup;
        const auto area = presetPopupArea (index);

        if (area.contains (plugin))
        {
            presetBodyHit (index, PresetPanel::hitBody (PresetPanel::designOf (layout.widgets[(size_t) index]), area, presetView (index), plugin), area);
            return true;
        }

        presetPopup = -1;
        endPresetSearch();
        repaint();
        return true;
    }

    presetPopup = -1;

    // A side panel's handle: it opens or closes, the window wider or narrower.
    for (int i = 0; i < (int) layout.widgets.size(); ++i)
    {
        const auto& w = layout.widgets[(size_t) i];

        if (! PresetPanel::has (w) || ! isShown (w))
            continue;

        if (const auto d = PresetPanel::designOf (w); PresetPanel::isSide (d) && PresetPanel::toggles (d)
                                                      && PresetPanel::handleArea (d, layout.width, layout.height).contains (plugin))
        {
            auto& state = presetState (i);
            state.sideOpen = ! state.sideOpen;
            endPresetSearch();

            if (onWindowChanged != nullptr)
                onWindowChanged();

            if (autoFit)
                fitToView();

            repaint();
            return true;
        }
    }

    return false;
}

void PluginCanvas::presetWidgetPress (int index, juce::Point<float> plugin, bool popupMenu)
{
    const auto& w = layout.widgets[(size_t) index];
    const auto d = PresetPanel::designOf (w);
    const auto style = PresetPanel::styleOf (d);
    const auto box = w.bounds.toFloat();

    // A table on the panel, a side panel: what's clicked in it.
    if (style == "side" || (style == "table" && ! PresetPanel::opensPopup (d)))
    {
        presetBodyHit (index, PresetPanel::hitBody (d, box, presetView (index), plugin), box);
        return;
    }

    // The bar: the arrows step; the name opens the list, the table (the classic: its menu).
    endPresetSearch();
    const auto hit = PresetPanel::hitBar (d, box, plugin);

    if (hit.kind == PresetPanel::Hit::Kind::prev || hit.kind == PresetPanel::Hit::Kind::next)
    {
        if (! popupMenu)
        {
            stepPreset (hit.kind == PresetPanel::Hit::Kind::prev ? -1 : 1);
            return;
        }
    }

    if (style == "bar" || popupMenu)
    {
        showPresetMenu (box);
        return;
    }

    presetPopup = index;
    repaint();
}

void PluginCanvas::presetBodyHit (int index, const PresetPanel::Hit& hit, juce::Rectangle<float> area)
{
    using Kind = PresetPanel::Hit::Kind;
    auto& state = presetState (index);
    const auto d = PresetPanel::designOf (layout.widgets[(size_t) index]);
    const bool inPopup = presetPopup == index;

    if (hit.kind != Kind::search)
        endPresetSearch();

    switch (hit.kind)
    {
        case Kind::preset:
            choosePreset (hit.index);

            if (inPopup)
                presetPopup = -1;
            break;

        case Kind::tab:
            state.tab = hit.index;
            state.page = 0;
            break;

        case Kind::search:
            state.searching = true;
            grabKeyboardFocus();
            break;

        case Kind::pagePrev:
        case Kind::pageNext:
        {
            const int pages = PresetPanel::pagesOf (d, area, presetView (index));
            state.page = juce::jlimit (0, pages - 1, state.page + (hit.kind == Kind::pageNext ? 1 : -1));
            break;
        }

        case Kind::action:
            if (inPopup)
                presetPopup = -1;

            presetAction (hit.index);
            break;

        case Kind::more:
            if (inPopup)
                presetPopup = -1;

            presetAction (3);
            break;

        case Kind::none:
        case Kind::body:
        case Kind::prev:
        case Kind::next:
        case Kind::open:
        default:
            break;
    }

    repaint();
}

bool PluginCanvas::presetWheel (juce::Point<float> plugin, float delta)
{
    auto turnPage = [this, delta] (int index, juce::Rectangle<float> area)
    {
        auto& state = presetState (index);
        const int pages = PresetPanel::pagesOf (PresetPanel::designOf (layout.widgets[(size_t) index]), area, presetView (index));
        state.page = juce::jlimit (0, pages - 1, state.page + (delta < 0.0f ? 1 : -1));
        repaint();
    };

    if (juce::isPositiveAndBelow (presetPopup, (int) layout.widgets.size()))
    {
        if (const auto area = presetPopupArea (presetPopup); area.contains (plugin))
        {
            turnPage (presetPopup, area);
            return true;
        }
    }

    const auto index = widgetAt (plugin, true);

    if (index < 0 || ! PresetPanel::has (layout.widgets[(size_t) index]))
        return false;

    const auto& w = layout.widgets[(size_t) index];
    const auto d = PresetPanel::designOf (w);

    if (PresetPanel::isSide (d) || (PresetPanel::styleOf (d) == "table" && ! PresetPanel::opensPopup (d)))
        turnPage (index, w.bounds.toFloat());
    else
        stepPreset (delta < 0.0f ? 1 : -1);

    return true;
}

bool PluginCanvas::presetKey (const juce::KeyPress& key)
{
    for (auto& [index, state] : presetStates)
    {
        if (! state.searching)
            continue;

        if (key == juce::KeyPress::escapeKey || key == juce::KeyPress::returnKey)
        {
            if (key == juce::KeyPress::escapeKey)
                state.search.clear();

            state.searching = false;
        }
        else if (key == juce::KeyPress::backspaceKey)
        {
            state.search = state.search.dropLastCharacters (1);
        }
        else if (const auto c = key.getTextCharacter(); c >= ' ' && ! key.getModifiers().isCommandDown() && ! key.getModifiers().isCtrlDown())
        {
            state.search << juce::String::charToString (c);
        }
        else
        {
            return false;
        }

        state.page = 0;
        repaint();
        return true;
    }

    return false;
}

void PluginCanvas::endPresetSearch()
{
    for (auto& [index, state] : presetStates)
        state.searching = false;
}

void PluginCanvas::stepPreset (int delta)
{
    if (presetMenu.names.isEmpty())
    {
        const int count = PresetPanel::sampleView().names.size();
        demoPreset = ((demoPreset + delta) % count + count) % count;
        repaint();
        return;
    }

    if (const auto next = presetMenu.step (delta); next >= 0 && onPresetChosen != nullptr)
        onPresetChosen (next);
}

void PluginCanvas::choosePreset (int index)
{
    if (presetMenu.names.isEmpty())
        demoPreset = index;
    else if (onPresetChosen != nullptr)
        onPresetChosen (index);

    repaint();
}

void PluginCanvas::presetAction (int action)
{
    if (onPresetAction == nullptr || presetMenu.names.isEmpty())
    {
        if (action == 3 && onPresetAction != nullptr)
            showPresetMenu ({});

        return;
    }

    if (action == 0)        onPresetAction (presetMenu.canSave ? PresetAction::save : PresetAction::saveAs);
    else if (action == 1)   onPresetAction (PresetAction::saveAs);
    else if (action == 2)   onPresetAction (PresetAction::loadFile);
    else                    showPresetMenu ({});
}

void PluginCanvas::showPresetMenu (juce::Rectangle<float> pluginArea)
{
    if (presetMenu.names.isEmpty() && onPresetAction == nullptr)
        return;

    static constexpr int presetIds = 1, actionIds = 100000;
    juce::PopupMenu choices;

    // From a bar: the presets, then what to do; from a More key: what to do.
    if (! pluginArea.isEmpty())
        PresetMenus::addPresets (choices, presetMenu, presetIds);

    if (onPresetAction != nullptr)
    {
        if (! pluginArea.isEmpty())
            choices.addSeparator();

        PresetMenus::addActions (choices, presetMenu, actionIds);
    }

    auto options = juce::PopupMenu::Options();
    options = pluginArea.isEmpty() ? options.withMousePosition()
                                   : options.withTargetScreenArea (localAreaToGlobal (toView (pluginArea.toNearestInt()).toNearestInt()))
                                            .withMinimumWidth (juce::jmax (180, (int) toView (pluginArea.toNearestInt()).getWidth()));

    choices.showMenuAsync (options, [safeThis = juce::Component::SafePointer<PluginCanvas> (this)] (int choice)
                           {
                               if (safeThis == nullptr || choice <= 0)
                                   return;

                               if (choice >= actionIds)
                               {
                                   if (safeThis->onPresetAction != nullptr)
                                       safeThis->onPresetAction ((PresetAction) (choice - actionIds));
                               }
                               else if (safeThis->onPresetChosen != nullptr)
                               {
                                   safeThis->onPresetChosen (choice - presetIds);
                               }
                           });
}

void PluginCanvas::editBody()
{
    if (! hasPanel())
        return;

    if (bodyEditorWindow == nullptr)
        bodyEditorWindow = BodyEditor::makeWindow (*this);

    bodyEditorWindow->setVisible (true);
    bodyEditorWindow->toFront (true);
}

void PluginCanvas::showShowroom()
{
    editBody();   // the Showroom is where the body's seen and edited
}

juce::Image PluginCanvas::controlsPicture (float scale, bool withBackground)
{
    if (! hasPanel() || layout.width <= 0 || layout.height <= 0)
        return {};

    const juce::ScopedValueSetter<int> noHover (hovered, -1), noActive (active, -1);
    const juce::ScopedValueSetter<bool> asPlayed (design, false), sharp (baking, true);

    scale = juce::jlimit (0.25f, 4.0f, scale);
    juce::Image image (juce::Image::ARGB, juce::jmax (1, juce::roundToInt ((float) layout.width * scale)),
                       juce::jmax (1, juce::roundToInt ((float) layout.height * scale)), true);

    {
        juce::Graphics g (image);
        g.addTransform (juce::AffineTransform::scale (scale));

        // The panel's own picture (not its body): its colours and its background image.
        if (withBackground)
        {
            const auto plugin = juce::Rectangle<float> (0.0f, 0.0f, (float) layout.width, (float) layout.height);
            g.setGradientFill (juce::ColourGradient (layout.backgroundTop, 0.0f, 0.0f, layout.backgroundBottom, 0.0f, plugin.getBottom(), false));
            g.fillRect (plugin);

            if (layout.backgroundImage.isNotEmpty())
                drawPicture (g, layout.backgroundImage, plugin, layout.backgroundMode);
        }

        for (int i = 0; i < (int) layout.widgets.size(); ++i)
            if (isShown (layout.widgets[(size_t) i]))
                drawWidget (g, i);
    }

    return image;
}

void PluginCanvas::drawBody (juce::Graphics& g)
{
    const auto key = bodyKeyNow();

    if (key.isEmpty())
        return;

    const auto lightAngle = layout.lightAngle, lightAltitude = layout.lightAltitude, distance = layout.cameraDistance();
    const auto images = imagesFolder();

    // Export, and a picture of the panel: the sharp one, now.
    if (baking && (key != bodyKey || ! bodySharp))
    {
        if (std::shared_ptr<const Body3D> body = Body3D::fromVar (layout.body, layout.width, layout.height, images))
        {
            bodyImage = Render3D::renderBody (*body, 1.0f, lightAngle, lightAltitude, distance);
            bodyKey = key;
            bodySharp = true;

            if (const auto shapeKey = bodyShapeKeyNow(); shapeKey != bodyShapeKey)
            {
                bodyShape = body;
                bodyShapeKey = shapeKey;
                faceSlopes.clear();
            }
        }
    }

    // Changed: rendered on its thread, quickly first and then sharp; the last one stands in meanwhile.
    if (! baking && key != bodyKey && key != bodyPending && bodyPainter != nullptr)
    {
        bodyPending = key;
        bodyPainter->removeAllJobs (false, 0);
        const auto generation = ++*bodyGeneration;

        bodyPainter->addJob ([safeThis = juce::Component::SafePointer<PluginCanvas> (this), json = layout.body.clone(), key,
                              shapeKey = bodyShapeKeyNow(), w = layout.width, h = layout.height, lightAngle, lightAltitude, distance, images,
                              latest = bodyGeneration, generation]
        {
            // A newer body asked for since (an edit being dragged): this one's not wanted.
            if (latest->load() != generation)
                return;

            const std::shared_ptr<const Body3D> body = Body3D::fromVar (json, w, h, images);

            // Its shape, at once: what stands on its faces leans with them.
            juce::MessageManager::callAsync ([safeThis, body, shapeKey]
            {
                if (safeThis == nullptr || safeThis->bodyShapeKey == shapeKey)
                    return;

                safeThis->bodyShape = body;
                safeThis->bodyShapeKey = shapeKey;
                safeThis->faceSlopes.clear();
                safeThis->repaint();
            });

            for (int pass = 0; pass < 2; ++pass)
            {
                juce::Image image;

                // The sharp one only for the newest: a newer one's quick picture first.
                if (latest->load() != generation)
                    return;

                if (body != nullptr)
                {
                    if (pass == 0)
                    {
                        const Render3D::ScopedDraft quick;
                        image = Render3D::renderBody (*body, 0.35f, lightAngle, lightAltitude, distance);
                    }
                    else
                    {
                        image = Render3D::renderBody (*body, 1.0f, lightAngle, lightAltitude, distance);
                    }
                }

                const bool sharp = pass == 1;
                juce::MessageManager::callAsync ([safeThis, image, key, sharp]
                {
                    if (safeThis == nullptr || safeThis->bodyPending != key)
                        return;

                    safeThis->bodyImage = image;
                    safeThis->bodyImageMap.reset();   // its own picture now: nothing stretched
                    safeThis->bodyKey = sharp ? key : juce::String();   // (the quick one: the sharp one's still to come)
                    safeThis->bodySharp = sharp;

                    if (sharp)
                        safeThis->bodyPending.clear();

                    safeThis->repaint();
                });

                if (safeThis == nullptr)
                    return;
            }
        });
    }

    if (bodyImage.isValid())
    {
        juce::Graphics::ScopedSaveState state (g);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.setOpacity (1.0f);

        // Being moved or resized (or just let go, its new picture still coming): the last
        // picture stretched from where it was to where it is.
        std::optional<std::pair<juce::Rectangle<float>, juce::Rectangle<float>>> map;

        if (! baking && (drag == Drag::moveBody || drag == Drag::resizeBody || bodyInFrame))
            map = std::make_pair (bodyImageMap.has_value() ? bodyImageMap->first : bodyStartRect, bodyDragRect);
        else if (! baking && bodyImageMap.has_value())
            map = bodyImageMap;

        if (map.has_value() && ! map->first.isEmpty() && ! map->second.isEmpty())
            g.addTransform (juce::AffineTransform::translation (-map->first.getX(), -map->first.getY())
                                .scaled (map->second.getWidth() / map->first.getWidth(), map->second.getHeight() / map->first.getHeight())
                                .translated (map->second.getX(), map->second.getY()));

        g.drawImage (bodyImage, juce::Rectangle<float> (0.0f, 0.0f, (float) layout.width, (float) layout.height));
    }
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
    const juce::ScopedValueSetter<bool> asPlayed (design, false), sharpBody (baking, true);   // (the 3D body: the sharp one, now)

    scale = juce::jlimit (0.25f, 3.0f, scale);
    juce::Image image (juce::Image::ARGB, juce::jmax (1, juce::roundToInt ((float) layout.width * scale)),
                       juce::jmax (1, juce::roundToInt ((float) layout.height * scale)), true);

    {
        juce::Graphics g (image);
        g.addTransform (juce::AffineTransform::scale (scale));
        drawPanel (g, [&] (int i) { drawWidget (g, i); });

        for (int i = 0; i < (int) layout.widgets.size(); ++i)
            if (! isBehindBody (i))
                drawWidget (g, i);
    }

    return image;
}

std::vector<juce::Image> PluginCanvas::snapshotPages (float scale)
{
    std::vector<juce::Image> pictures;
    const juce::ScopedValueSetter<int> pageNow (shownPage, shownPage);

    for (int page = 0; page < juce::jmax (1, layout.pages.size()); ++page)
    {
        shownPage = page;
        pictures.push_back (snapshot (scale));
    }

    return pictures;
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

        for (size_t below = 0; still && ! isBehindBody ((int) i) && below < i && ! layered[i]; ++below)
            layered[i] = movingAreaOf (layout.widgets[below]).intersects (w.bounds)
                      || (layered[below] && layout.widgets[below].bounds.intersects (w.bounds));
    }

    {
        juce::Graphics g (result.background);
        drawPanel (g, [&] (int i) { drawWidget (g, i); });   // (behind the body: all of it, there)

        for (int i = 0; i < (int) layout.widgets.size(); ++i)
            if (! layered[(size_t) i] && ! isBehindBody (i))
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
        const auto area = isBehindBody (i) ? juce::Rectangle<int>()   // behind the 3D body: in the background, under it
                        : layered[(size_t) i] ? w.bounds.getIntersection ({ 0, 0, layout.width, layout.height }) : movingAreaOf (w);
        result.stripOf.push_back (-1);
        result.movingArea.push_back (area);

        if (isBehindBody (i))
            continue;

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
            const auto lean = Looks::leanFor (look, layout.cameraLean (area));   // a 3D look: where the panel camera sees it from
            const auto slope = Looks::slopeFor (look, look.doc.model3d != nullptr ? faceSlope (area) : juce::Point<float>());   // ...on the body's face
            const auto key = juce::String (look.revision) + "|" + Looks::kindName (kind) + "|"
                           + juce::String (area.getWidth()) + "x" + juce::String (area.getHeight()) + Looks::leanKey (lean) + Looks::slopeKey (slope);

            // Its frames reach past its box by the look's shadows: drawn over the box expanded.
            const auto room = Looks::overhang (look, area.getWidth(), area.getHeight());
            result.movingArea.back() = area.expanded (room);

            if (const auto found = lookStrips.find (key); found != lookStrips.end())
            {
                result.stripOf.back() = found->second;
                continue;
            }

            Bake::Strip strip;
            strip.image = looks->strip (look, kind, area.getWidth(), area.getHeight(), lean, slope);
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
            case GuiWidget::Type::tabs:
            case GuiWidget::Type::matrix: break;
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
    // An element being resized: its 3D pictures stretched as they are (made again, sharp, once
    // it's let go), so the drag keeps up.
    std::optional<Render3D::ScopedStretch> stretch;

    if (drag == Drag::resizeElement)
        stretch.emplace();

    // 3D pictures not made yet: made in the background (the last ones standing in), so a
    // change never holds the window up; painted again once they're made.
    std::optional<Render3D::ScopedLater> later;

    if (! baking)
        later.emplace (*this);

    // A made plugin's window in Sonify: the panel and what's on it, nothing around it.
    if (panelOnly)
    {
        g.fillAll (juce::Colours::black);

        if (! hasPanel())
            return;

        syncSideModules();
        const auto area = panelArea();
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (windowArea().toNearestInt());
        g.addTransform (juce::AffineTransform::scale (zoom).translated (area.getX(), area.getY()));

        const auto painted = g.getClipBounds().toFloat();
        drawPanel (g, [&] (int i) { if (! liesOutside (layout.widgets[(size_t) i], painted)) drawWidget (g, i); });

        // Only what lies in the area being painted (a knob turning paints its own square).
        for (int i = 0; i < (int) layout.widgets.size(); ++i)
            if (! liesOutside (layout.widgets[(size_t) i], painted) && ! isBehindBody (i))
                drawWidget (g, i);

        drawMatrixOverlay (g);
        drawPresetOverlay (g);
        drawElementPopups (g);
        return;
    }

    // A grid over the whole tab, FlowStone-style; it moves and zooms with the view.
    g.fillAll (Theme::panel);
    drawGrid (g);

    if (! hasPanel())
        return;

    syncSideModules();
    const auto area = panelArea();

    juce::DropShadow (juce::Colours::black.withAlpha (0.45f), 18, { 0, 6 }).drawForRectangle (g, windowArea().toNearestInt());

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (windowArea().toNearestInt());
        g.addTransform (juce::AffineTransform::scale (zoom).translated (area.getX(), area.getY()));

        const auto painted = g.getClipBounds().toFloat();
        drawPanel (g, [&] (int i) { if (! liesOutside (layout.widgets[(size_t) i], painted)) drawWidget (g, i); });

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

        for (int i = 0; i < (int) layout.widgets.size(); ++i)
            if (! liesOutside (layout.widgets[(size_t) i], painted) && ! isBehindBody (i))
                drawWidget (g, i);

        // The cables between a matrix's jacks across the window, the modulated controls' rings.
        drawMatrixOverlay (g);

        // The side panels' handles; a preset manager's open list or table, over everything.
        drawPresetOverlay (g);

        // Programmed elements' popups lie over everything.
        if (! design)
            drawElementPopups (g);

        // The Body editor's part, outlined where it lies.
        if (design && ! bodyHighlight.isEmpty() && bodyEditorWindow != nullptr && bodyEditorWindow->isVisible())
        {
            // Corner marks just outside it, so the part itself stays in plain sight (a small
            // or narrow one too).
            const float px = 1.0f / juce::jmax (0.1f, zoom);
            const auto box = bodyHighlight.getBounds().expanded (5.0f * px);
            const auto arm = juce::jmin (12.0f * px, juce::jmax (4.0f * px, juce::jmin (box.getWidth(), box.getHeight()) * 0.35f));
            juce::Path marks;

            for (const auto& [corner, dx, dy] : { std::tuple<juce::Point<float>, float, float> { box.getTopLeft(), 1.0f, 1.0f },
                                                  { box.getTopRight(), -1.0f, 1.0f },
                                                  { box.getBottomRight(), -1.0f, -1.0f },
                                                  { box.getBottomLeft(), 1.0f, -1.0f } })
            {
                marks.startNewSubPath (corner.translated (dx * arm, 0.0f));
                marks.lineTo (corner);
                marks.lineTo (corner.translated (0.0f, dy * arm));
            }

            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.strokePath (marks, juce::PathStrokeType (3.5f * px, juce::PathStrokeType::mitered, juce::PathStrokeType::square));
            g.setColour (Theme::accent);
            g.strokePath (marks, juce::PathStrokeType (1.6f * px, juce::PathStrokeType::mitered, juce::PathStrokeType::square));
        }
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
    drawRulers (g);

    if (design && ! cropping)
    {
        // The window's own corner: drag it to resize the window.
        const auto grip = panelHandle();
        g.setColour (panelSelected || drag == Drag::resizePanel ? Theme::accent : Theme::accent.withAlpha (0.7f));
        juce::Path corner;
        corner.addTriangle (grip.getTopRight(), grip.getBottomRight(), grip.getBottomLeft());
        g.fillPath (corner);

        // The 3D body, selected: its box and its eight handles.
        if (bodySelected && Body3D::hasParts (layout.body))
        {
            const auto box = toView (bodyBounds().getSmallestIntegerContainer());
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawRect (box.expanded (1.0f), 3.0f);
            g.setColour (Theme::accent);
            g.drawRect (box, 1.5f);

            for (int handle = 0; handle < 8; ++handle)
            {
                if (const auto area = bodyHandleArea (handle); ! area.isEmpty())
                {
                    g.setColour (juce::Colours::white);
                    g.fillRect (area);
                    g.setColour (Theme::accent.darker (0.3f));
                    g.drawRect (area, 1.5f);
                }
            }

            g.setColour (Theme::accent);
            g.setFont (Theme::font (12.0f, true));
            g.drawText ("3D BODY  " + juce::String (juce::roundToInt (bodyBounds().getWidth())) + juce::String::fromUTF8 (" \xc3\x97 ")
                            + juce::String (juce::roundToInt (bodyBounds().getHeight())),
                        juce::Rectangle<float> (box.getX() + 8.0f, box.getY() + 6.0f, 260.0f, 16.0f), juce::Justification::centredLeft, false);
        }

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

            // The "E" under it: its studio.
            if (const auto tab = editTabArea(); ! tab.isEmpty())
            {
                g.setColour (Theme::accent.withAlpha (0.8f));
                g.drawLine (tab.getCentreX(), handleCentre (5).y + handleSize * 0.5f, tab.getCentreX(), tab.getY(), 1.2f);
                g.setColour (Theme::accent);
                g.fillRoundedRectangle (tab, 4.0f);
                g.setColour (juce::Colours::white.withAlpha (0.9f));
                g.drawRoundedRectangle (tab.reduced (0.5f), 4.0f, 1.0f);
                g.setColour (juce::Colour (0xff0b1a12));
                g.setFont (Theme::font (12.0f, true));
                g.drawText ("E", tab, juce::Justification::centred, false);
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

            // Per corner: its corners, round handles (nothing drawn over the shape itself).
            if (const auto first = handleArea (cornerHandle); ! first.isEmpty())
            {

                for (int k = 0; k < 4; ++k)
                {
                    const auto knob = handleArea (cornerHandle + k);
                    g.setColour (juce::Colours::white);
                    g.fillEllipse (knob);
                    g.setColour (Theme::accent);
                    g.drawEllipse (knob.reduced (0.75f), 1.5f);
                }
            }

            // A line's bend points: a yellow diamond for an arc, a yellow square for a corner; the
            // one the menu shows, ringed.
            for (int k = 0; k < maxBendHandles; ++k)
            {
                const auto own = handleArea (bendHandle + k);

                if (own.isEmpty())
                    break;

                const auto c = own.getCentre();
                const auto r = own.getWidth() * 0.62f;
                juce::Path mark;

                if (ObjectPaint::isCornerBend (w, k))
                    mark.addRectangle (own.reduced (own.getWidth() * 0.12f));
                else
                    mark.addQuadrilateral (c.x, c.y - r, c.x + r, c.y, c.x, c.y + r, c.x - r, c.y);

                g.setColour (juce::Colour (0xffffcc00));
                g.fillPath (mark);
                g.setColour (k == activeBend ? juce::Colours::white : juce::Colours::black.withAlpha (0.6f));
                g.strokePath (mark, juce::PathStrokeType (k == activeBend ? 1.6f : 1.0f));
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