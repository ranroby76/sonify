// D:\workspace\Fanan_web_hub\src\PluginCanvas_Mouse.cpp
// The canvas's view, after Colosseum's rack (GraphCanvas): zoom and pan, the minimap and the
// scrollbars; then the mouse, the keys, and what's dragged in (primitives and pictures).

#include "PluginCanvas.h"
#include "PluginCanvasMenu.h"
#include "PanelParts.h"
#include "StellaLookAndFeel.h"
#include "KnobStudio.h"   // the Gallery

namespace
{
    constexpr float scrollbarThickness = 12.0f;
    constexpr float minimapWidth = 200.0f, minimapMaxHeight = 140.0f, minimapMargin = 8.0f;
    constexpr float worldMargin = 1200.0f;     // how far past the window the view can go
    constexpr float scrollSpeed = 120.0f;      // wheel scrolling, in view pixels per wheel unit (as in Colosseum)
    const juce::Colour mapControl { 0xffd8d4ca };
}

//==============================================================================
// The view
juce::Rectangle<float> PluginCanvas::viewArea() const
{
    auto area = getLocalBounds().toFloat();

    if (hasPanel() && ! panelOnly)
        area = area.withTrimmedRight (scrollbarThickness).withTrimmedBottom (scrollbarThickness);

    return area;
}

juce::Rectangle<float> PluginCanvas::panelArea() const
{
    return toView ({ 0, 0, layout.width, layout.height });
}

juce::Point<float> PluginCanvas::toPlugin (juce::Point<float> view) const
{
    return view / zoom + pan;
}

juce::Rectangle<float> PluginCanvas::toView (juce::Rectangle<int> plugin) const
{
    return { ((float) plugin.getX() - pan.x) * zoom, ((float) plugin.getY() - pan.y) * zoom,
             (float) plugin.getWidth() * zoom, (float) plugin.getHeight() * zoom };
}

juce::Rectangle<float> PluginCanvas::worldBounds() const
{
    if (panelOnly)
        return { 0.0f, 0.0f, (float) layout.width, (float) layout.height };

    const auto margin = juce::jmax (worldMargin, (float) juce::jmax (layout.width, layout.height));
    return juce::Rectangle<float> (0.0f, 0.0f, (float) layout.width, (float) layout.height).expanded (margin);
}

void PluginCanvas::clampPan()
{
    const auto world = worldBounds();
    const auto view = viewArea();

    auto clampAxis = [] (float value, float start, float size, float visible)
    {
        if (visible >= size)
            return start + (size - visible) * 0.5f;   // all of it shows: keep it centred

        return juce::jlimit (start, start + size - visible, value);
    };

    pan.x = clampAxis (pan.x, world.getX(), world.getWidth(), view.getWidth() / zoom);
    pan.y = clampAxis (pan.y, world.getY(), world.getHeight(), view.getHeight() / zoom);
}

void PluginCanvas::centreOn (juce::Point<float> plugin)
{
    pan = plugin - viewArea().getCentre() / zoom;
    clampPan();
}

void PluginCanvas::setZoom (float newZoom, std::optional<juce::Point<float>> around)
{
    newZoom = juce::jlimit (minZoom, maxZoom, newZoom);

    if (std::abs (newZoom - zoom) < 0.0005f)
    {
        updateZoomControls();
        return;
    }

    // The plugin pixel under the point stays under it.
    const auto anchor = around.value_or (viewArea().getCentre());
    const auto under = toPlugin (anchor);

    zoom = newZoom;
    pan = under - anchor / zoom;
    clampPan();
    viewChanged();
}

void PluginCanvas::fitToView()
{
    // The panel alone fills the window (as big as it fits, its shape kept); otherwise there's
    // room around it for the caption above the window.
    const auto view = panelOnly ? viewArea() : viewArea().reduced (44.0f, 44.0f).withTrimmedTop (12.0f);

    if (view.isEmpty() || ! hasPanel())
    {
        autoFit = true;
        return;
    }

    zoom = juce::jlimit (minZoom, panelOnly ? maxZoom : 1.0f, juce::jmin (view.getWidth() / (float) layout.width, view.getHeight() / (float) layout.height));
    pan = juce::Point<float> ((float) layout.width, (float) layout.height) * 0.5f - view.getCentre() / zoom;
    clampPan();
    autoFit = true;
    viewChanged();
}

void PluginCanvas::viewChanged()
{
    clampPan();
    updateZoomControls();
    layOutOverlays();
    saveViewSoon();
    repaint();
}

void PluginCanvas::updateZoomControls()
{
    zoomSlider.setValue (zoom, juce::dontSendNotification);
    zoomLabel.setText (juce::String (juce::roundToInt (zoom * 100.0f)) + "%", juce::dontSendNotification);
    zoomSlider.setColour (juce::Slider::thumbColourId, std::abs (zoom - 1.0f) < 0.005f ? Theme::accent : Theme::text);
}

void PluginCanvas::loadView()
{
    if (! hasPanel() || panelOnly)
        return;

    const auto saved = juce::JSON::parse (guiFolder.getChildFile ("view.json"));

    if (! saved.isObject() || ! saved.hasProperty ("zoom"))
        return;

    zoom = juce::jlimit (minZoom, maxZoom, (float) (double) saved.getProperty ("zoom", 1.0));
    pan = { (float) (double) saved.getProperty ("x", 0.0), (float) (double) saved.getProperty ("y", 0.0) };
    autoFit = false;
}

void PluginCanvas::saveViewSoon()
{
    // Only a view the user chose is kept; the automatic one comes back by itself.
    if (autoFit || viewSaveScheduled || ! hasPanel() || panelOnly)
        return;

    viewSaveScheduled = true;

    juce::Timer::callAfterDelay (800, [safeThis = juce::Component::SafePointer<PluginCanvas> (this), folder = guiFolder]
    {
        if (safeThis == nullptr)
            return;

        safeThis->viewSaveScheduled = false;

        if (safeThis->guiFolder != folder || safeThis->autoFit || ! folder.isDirectory())
            return;

        auto* object = new juce::DynamicObject();
        const juce::var view (object);
        object->setProperty ("zoom", safeThis->zoom);
        object->setProperty ("x", safeThis->pan.x);
        object->setProperty ("y", safeThis->pan.y);
        folder.getChildFile ("view.json").replaceWithText (juce::JSON::toString (view));
    });
}

void PluginCanvas::followPosition()
{
    // The toolbox showing or hiding moves the canvas's left edge: the window stays where it
    // was on screen.
    if (getX() != lastX && ! autoFit)
        pan.x += (float) (getX() - lastX) / zoom;

    lastX = getX();
}

void PluginCanvas::moved()
{
    followPosition();

    if (! autoFit)
    {
        clampPan();
        viewChanged();
    }
}

void PluginCanvas::resized()
{
    followPosition();

    if (autoFit)
        fitToView();
    else
    {
        clampPan();
        viewChanged();
    }
}

//==============================================================================
// The minimap and the scrollbars, drawn over the canvas
juce::Rectangle<float> PluginCanvas::minimapArea() const
{
    const auto world = worldBounds();
    const auto height = juce::jmin (minimapMaxHeight, minimapWidth * world.getHeight() / world.getWidth());
    const auto width = height * world.getWidth() / world.getHeight();
    const auto view = viewArea();

    return { view.getRight() - width - minimapMargin, view.getBottom() - height - minimapMargin, width, height };
}

juce::Rectangle<float> PluginCanvas::hScrollArea() const
{
    const auto view = viewArea();
    return { view.getX(), view.getBottom(), view.getWidth(), scrollbarThickness };
}

juce::Rectangle<float> PluginCanvas::vScrollArea() const
{
    const auto view = viewArea();
    return { view.getRight(), view.getY(), scrollbarThickness, view.getHeight() };
}

juce::Rectangle<float> PluginCanvas::hThumb() const
{
    const auto world = worldBounds();
    const auto track = hScrollArea().reduced (2.0f);
    const auto visible = viewArea().getWidth() / zoom;
    const auto ratio = juce::jlimit (0.0f, 1.0f, visible / world.getWidth());
    const auto thumbWidth = juce::jmax (24.0f, track.getWidth() * ratio);
    const auto range = world.getWidth() - visible;
    const auto position = range > 0.0f ? juce::jlimit (0.0f, 1.0f, (pan.x - world.getX()) / range) : 0.0f;

    return { track.getX() + (track.getWidth() - thumbWidth) * position, track.getY(), thumbWidth, track.getHeight() };
}

juce::Rectangle<float> PluginCanvas::vThumb() const
{
    const auto world = worldBounds();
    const auto track = vScrollArea().reduced (2.0f);
    const auto visible = viewArea().getHeight() / zoom;
    const auto ratio = juce::jlimit (0.0f, 1.0f, visible / world.getHeight());
    const auto thumbHeight = juce::jmax (24.0f, track.getHeight() * ratio);
    const auto range = world.getHeight() - visible;
    const auto position = range > 0.0f ? juce::jlimit (0.0f, 1.0f, (pan.y - world.getY()) / range) : 0.0f;

    return { track.getX(), track.getY() + (track.getHeight() - thumbHeight) * position, track.getWidth(), thumbHeight };
}

juce::Rectangle<float> PluginCanvas::panelHandle() const
{
    const auto area = panelArea();
    return { area.getRight() - 12.0f, area.getBottom() - 12.0f, 12.0f, 12.0f };
}

//==============================================================================
// The crop tool
void PluginCanvas::startCrop()
{
    if (! design || ! hasPanel() || panelOnly || cropping)
        return;

    select (-1);
    releaseKey();
    cropping = true;
    cropBox = cropStart = { 0, 0, layout.width, layout.height };
    cropHandle = -1;
    cropButton.setToggleState (true, juce::dontSendNotification);
    cropApplyButton.setVisible (true);
    cropCancelButton.setVisible (true);
    grabKeyboardFocus();
    layOutOverlays();
    repaint();
}

void PluginCanvas::cancelCrop()
{
    if (! cropping)
        return;

    cropping = false;

    if (drag == Drag::crop)
        drag = Drag::none;

    cropButton.setToggleState (false, juce::dontSendNotification);
    cropApplyButton.setVisible (false);
    cropCancelButton.setVisible (false);
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

void PluginCanvas::applyCrop()
{
    if (! cropping)
        return;

    const auto box = cropBox;
    cancelCrop();

    const auto newWidth = juce::jlimit (GuiLayout::minWidth, GuiLayout::maxWidth, box.getWidth());
    const auto newHeight = juce::jlimit (GuiLayout::minHeight, GuiLayout::maxHeight, box.getHeight());

    if (box.getPosition().isOrigin() && newWidth == layout.width && newHeight == layout.height)
        return;   // nothing to crop

    // Everything moves with the box's top-left corner; a background picture fills the new
    // window; the badge stays inside it; what lies wholly outside it is cropped away.
    const juce::Rectangle<int> newWindow (0, 0, newWidth, newHeight);
    std::vector<GuiWidget> kept;

    for (auto w : layout.widgets)
    {
        if (isBackdrop (w))
        {
            w.bounds = newWindow;
        }
        else
        {
            w.bounds.translate (-box.getX(), -box.getY());

            if (w.type == GuiWidget::Type::registration)
                w.bounds = w.bounds.constrainedWithin (newWindow);
            else if (! w.turnedArea().intersects (newWindow.toFloat()))
                continue;
        }

        kept.push_back (w);
    }

    releaseKey();
    layout.widgets = std::move (kept);
    layout.width = newWidth;
    layout.height = newHeight;
    indexesChanged();
    select (-1);

    fitPaintedBackdrops();
    edited();

    if (autoFit)
        fitToView();
    else
        viewChanged();
}

juce::Rectangle<float> PluginCanvas::cropHandleArea (int handle) const
{
    if (! cropping || ! juce::isPositiveAndBelow (handle, 8))
        return {};

    const auto box = toView (cropBox);
    const juce::Point<float> points[] { box.getTopLeft(), { box.getCentreX(), box.getY() }, box.getTopRight(),
                                        { box.getRight(), box.getCentreY() }, box.getBottomRight(), { box.getCentreX(), box.getBottom() },
                                        box.getBottomLeft(), { box.getX(), box.getCentreY() } };
    return juce::Rectangle<float> (10.0f, 10.0f).withCentre (points[handle]);
}

int PluginCanvas::cropHandleAt (juce::Point<float> view) const
{
    if (! cropping)
        return -1;

    for (int handle = 0; handle < 8; ++handle)
        if (cropHandleArea (handle).expanded (5.0f).contains (view))
            return handle;

    return toView (cropBox).contains (view) ? 8 : -1;
}

void PluginCanvas::dragCrop (juce::Point<float> delta, bool keepShape)
{
    const auto dx = juce::roundToInt (delta.x), dy = juce::roundToInt (delta.y);

    if (cropHandle == 8)
    {
        cropBox = cropStart.translated (dx, dy);
        return;
    }

    auto left = cropStart.getX(), top = cropStart.getY(), right = cropStart.getRight(), bottom = cropStart.getBottom();
    const bool moveLeft = cropHandle == 0 || cropHandle == 6 || cropHandle == 7;
    const bool moveRight = cropHandle == 2 || cropHandle == 3 || cropHandle == 4 || cropHandle == 9;
    const bool moveTop = cropHandle == 0 || cropHandle == 1 || cropHandle == 2;
    const bool moveBottom = cropHandle == 4 || cropHandle == 5 || cropHandle == 6 || cropHandle == 9;

    // An edge near the window's own edge sticks to it.
    const auto reach = 6.0f / zoom;
    auto stick = [reach] (int value, int edge) { return std::abs ((float) (value - edge)) <= reach ? edge : value; };

    if (moveLeft)   left = stick (stick (left + dx, 0), layout.width);
    if (moveRight)  right = stick (stick (right + dx, 0), layout.width);
    if (moveTop)    top = stick (stick (top + dy, 0), layout.height);
    if (moveBottom) bottom = stick (stick (bottom + dy, 0), layout.height);

    // Shift on a corner: the box keeps its shape.
    const bool corner = cropHandle == 0 || cropHandle == 2 || cropHandle == 4 || cropHandle == 6;

    if (keepShape && corner && cropStart.getWidth() > 0 && cropStart.getHeight() > 0)
    {
        const auto ratio = (float) cropStart.getWidth() / (float) cropStart.getHeight();
        const auto height = juce::roundToInt ((float) std::abs (right - left) / ratio);

        if (moveTop) top = bottom - height;
        else         bottom = top + height;
    }

    if (right < left)  std::swap (left, right);
    if (bottom < top)  std::swap (top, bottom);

    right = juce::jlimit (left + 8, left + GuiLayout::maxWidth, right);
    bottom = juce::jlimit (top + 8, top + GuiLayout::maxHeight, bottom);
    cropBox = juce::Rectangle<int>::leftTopRightBottom (left, top, right, bottom);
}

void PluginCanvas::drawCrop (juce::Graphics& g)
{
    const auto view = viewArea();
    const auto box = toView (cropBox);
    const auto window = panelArea();

    // Room it adds to the window: in the window's colours, as it will be.
    {
        juce::Path added;   // the box less the window (even-odd)
        added.addRectangle (box);
        added.addRectangle (window);
        added.setUsingNonZeroWinding (false);

        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (added);
        g.setGradientFill (juce::ColourGradient (layout.backgroundTop, 0.0f, window.getY(), layout.backgroundBottom, 0.0f, window.getBottom(), false));
        g.fillRect (box);
    }

    // What's cropped away: dimmed.
    {
        juce::RectangleList<float> outside (view);
        outside.subtract (box);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRectList (outside);
    }

    // Its thirds, its frame and its handles.
    g.setColour (juce::Colours::white.withAlpha (drag == Drag::crop ? 0.5f : 0.25f));

    for (int i = 1; i < 3; ++i)
    {
        g.drawVerticalLine (juce::roundToInt (box.getX() + box.getWidth() * (float) i / 3.0f), box.getY(), box.getBottom());
        g.drawHorizontalLine (juce::roundToInt (box.getY() + box.getHeight() * (float) i / 3.0f), box.getX(), box.getRight());
    }

    g.setColour (juce::Colours::white);
    g.drawRect (box, 1.5f);

    for (int handle = 0; handle < 8; ++handle)
    {
        const auto square = cropHandleArea (handle);
        g.setColour (juce::Colours::white);
        g.fillRect (square);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawRect (square, 1.0f);
    }

    // Its size above it, and what to do.
    const auto size = juce::String (cropBox.getWidth()) + juce::String::fromUTF8 (" \xc3\x97 ") + juce::String (cropBox.getHeight());
    const auto font = Theme::font (13.0f, true);
    const auto pill = juce::Rectangle<float> (box.getX(), box.getY() - 30.0f,
                                              (float) juce::GlyphArrangement::getStringWidthInt (font, size) + 18.0f, 22.0f);
    g.setColour (Theme::accent);
    g.fillRoundedRectangle (pill, 5.0f);
    g.setColour (juce::Colours::white);
    g.setFont (font);
    g.drawText (size, pill, juce::Justification::centred, false);

    const auto hint = juce::String::fromUTF8 ("Drag its sides and corners, in or out \xc2\xb7 Enter crops \xc2\xb7 Esc leaves it \xc2\xb7 "
                                             "what lies wholly outside is cropped away");
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont (Theme::font (12.5f));
    g.drawText (hint, juce::Rectangle<float> (view.getX(), cropApplyButton.getY() - 24.0f, view.getWidth(), 18.0f),
                juce::Justification::centred, true);
}

void PluginCanvas::fitPaintedBackdrops()
{
    for (const auto& w : layout.widgets)
    {
        if (! isBackdrop (w))
            continue;

        auto recipe = paintedRecipeOf (w);

        if (! recipe.isObject() || ((int) recipe.getProperty ("width", 0) == layout.width && (int) recipe.getProperty ("height", 0) == layout.height))
            continue;

        recipe = recipe.clone();
        recipe.getDynamicObject()->setProperty ("width", layout.width);
        recipe.getDynamicObject()->setProperty ("height", layout.height);
        repaintPainted (recipe);
    }
}

juce::AffineTransform PluginCanvas::viewTurn (const GuiWidget& w) const
{
    if (! w.isTurned())
        return {};

    const auto c = toView (w.bounds).getCentre();
    return juce::AffineTransform::rotation (juce::degreesToRadians (w.shownAngle()), c.x, c.y);
}

juce::Path PluginCanvas::viewOutline (const GuiWidget& w, float grow) const
{
    juce::Path box;
    box.addRectangle (toView (w.bounds).expanded (grow));
    box.applyTransform (viewTurn (w));
    return box;
}

juce::Rectangle<int> PluginCanvas::viewAreaOf (const GuiWidget& w) const
{
    return viewOutline (w, 0.0f).getBounds().getSmallestIntegerContainer().expanded (2);
}

juce::Point<float> PluginCanvas::handleCentre (int handle) const
{
    if (! juce::isPositiveAndBelow (selected, (int) layout.widgets.size()) || ! juce::isPositiveAndBelow (handle, numHandles) || ! manySelected.empty())
        return {};

    const auto& w = layout.widgets[(size_t) selected];
    const auto box = toView (w.bounds).expanded (2.0f);
    juce::Point<float> at;

    if (handle == turnHandle)
    {
        at = { box.getCentreX(), box.getY() - 26.0f };
    }
    else if (handle == lineStartHandle || handle == lineEndHandle)
    {
        // A line's ends, where they are in its box.
        const auto points = ObjectPaint::linePoints (w);
        at = toView (w.bounds).getPosition() + (handle == lineStartHandle ? points.start : points.end) * zoom;
    }
    else if (handle == shapeHandle)
    {
        // A shape's own, kept clear of the corner handles.
        const auto inside = toView (w.bounds);
        const auto point = ObjectPaint::adjustPoint (w) * zoom;
        at = inside.getPosition() + point;

        if (w.shapeKind() == "rectangle" && point.x < 16.0f)
            at = inside.getPosition() + juce::Point<float> (16.0f, 16.0f);
        else if (w.shapeKind() == "triangle")
            at.y = inside.getY() + 14.0f;
    }
    else
    {
        const juce::Point<float> points[] { box.getTopLeft(), { box.getCentreX(), box.getY() }, box.getTopRight(),
                                            { box.getRight(), box.getCentreY() }, box.getBottomRight(),
                                            { box.getCentreX(), box.getBottom() }, box.getBottomLeft(), { box.getX(), box.getCentreY() } };
        at = points[handle];
    }

    return at.transformedBy (viewTurn (w));
}

juce::Rectangle<float> PluginCanvas::handleArea (int handle) const
{
    if (! juce::isPositiveAndBelow (selected, (int) layout.widgets.size()) || ! juce::isPositiveAndBelow (handle, numHandles) || ! manySelected.empty())
        return {};

    const auto& w = layout.widgets[(size_t) selected];
    const auto box = toView (w.bounds).expanded (2.0f);
    const bool side = handle < 8 && (handle % 2) == 1;

    if (! isShown (w))
        return {};   // on another page

    // A line: its ends and its bend point shape it (and its turn); nothing else does.
    const bool line = w.type == GuiWidget::Type::shape && w.shapeKind() == "line";

    if (handle == lineStartHandle || handle == lineEndHandle)
        return line ? juce::Rectangle<float> (handleSize + 2.0f, handleSize + 2.0f).withCentre (handleCentre (handle)) : juce::Rectangle<float>();

    if (line)
        return handle < 8 ? juce::Rectangle<float>()
                          : juce::Rectangle<float> (handleSize, handleSize).withCentre (handleCentre (handle)).expanded (1.0f);

    // Sides: not on knobs (they stay round), nor where they'd crowd the corners.
    if (side && (w.type == GuiWidget::Type::knob
                 || ((handle == 1 || handle == 5) && box.getWidth() < handleSize * 4.0f)
                 || ((handle == 3 || handle == 7) && box.getHeight() < handleSize * 4.0f)))
        return {};

    // The turn: anything but the registration badge. A shape's own: shapes only.
    if ((handle == turnHandle && w.type == GuiWidget::Type::registration)
        || (handle == shapeHandle && (w.type != GuiWidget::Type::shape || box.getWidth() < handleSize * 3.0f || box.getHeight() < handleSize * 3.0f)))
        return {};

    return juce::Rectangle<float> (handleSize, handleSize).withCentre (handleCentre (handle)).expanded (handle >= 8 ? 1.0f : 0.0f);
}

int PluginCanvas::handleAt (juce::Point<float> view) const
{
    // A line's ends first, then the shape's own (it lies inside), then corners: they win where
    // they meet the sides.
    for (const auto handle : { lineStartHandle, lineEndHandle, shapeHandle, 0, 2, 4, 6, 1, 3, 5, 7, turnHandle })
        if (const auto area = handleArea (handle); ! area.isEmpty() && area.expanded (3.0f).contains (view))
            return handle;

    return -1;
}

bool PluginCanvas::isLine (int index) const
{
    return juce::isPositiveAndBelow (index, (int) layout.widgets.size()) && layout.widgets[(size_t) index].type == GuiWidget::Type::shape
        && layout.widgets[(size_t) index].shapeKind() == "line";
}

bool PluginCanvas::isBackdrop (const GuiWidget& w) const
{
    return w.type == GuiWidget::Type::image && w.bounds.contains (juce::Rectangle<int> (0, 0, layout.width, layout.height));
}

int PluginCanvas::backBackdrop() const
{
    // Up from the back while there are only backgrounds and frames (a frame may lie under one).
    int found = -1;

    for (int i = 0; i < (int) layout.widgets.size(); ++i)
    {
        const auto& w = layout.widgets[(size_t) i];

        if (isBackdrop (w))
            found = i;
        else if (w.type != GuiWidget::Type::group)
            break;
    }

    return found;
}

void PluginCanvas::reorder (int index, Order order)
{
    if (! juce::isPositiveAndBelow (index, (int) layout.widgets.size()))
        return;

    const auto last = (int) layout.widgets.size() - 1;
    const auto target = order == Order::front    ? last
                      : order == Order::back     ? 0
                      : order == Order::forward  ? juce::jmin (last, index + 1)
                                                 : juce::jmax (0, index - 1);

    if (target == index)
        return;

    releaseKey();

    const auto moving = layout.widgets[(size_t) index];
    layout.widgets.erase (layout.widgets.begin() + index);
    layout.widgets.insert (layout.widgets.begin() + target, moving);
    indexesChanged();

    select (target);
    edited();
}

//==============================================================================
// Copy and paste
namespace
{
    constexpr const char* clipboardKey = "stellaElements";   // what marks the clipboard's text as ours
}

void PluginCanvas::copyElements (const std::vector<int>& indexes)
{
    // Frames take what's inside them along; all in their layers' order.
    std::set<int> copied;

    for (const auto index : indexes)
    {
        if (! juce::isPositiveAndBelow (index, (int) layout.widgets.size()))
            continue;

        copied.insert (index);
        const auto& frame = layout.widgets[(size_t) index];

        if (frame.type == GuiWidget::Type::group)
            for (int i = 0; i < (int) layout.widgets.size(); ++i)
                if (i != index && frame.bounds.contains (layout.widgets[(size_t) i].bounds) && ! isBackdrop (layout.widgets[(size_t) i]))
                    copied.insert (i);
    }

    if (copied.empty())
        return;

    juce::Array<juce::var> list;

    for (const auto i : copied)
        list.add (GuiLayout::widgetToVar (layout.widgets[(size_t) i]));

    auto json = juce::var (new juce::DynamicObject());
    json.getDynamicObject()->setProperty (clipboardKey, list);
    juce::SystemClipboard::copyTextToClipboard (juce::JSON::toString (json));
    pasteCount = 0;
}

std::vector<GuiWidget> PluginCanvas::clipboardElements() const
{
    std::vector<GuiWidget> result;
    const auto text = juce::SystemClipboard::getTextFromClipboard();

    if (! text.containsIgnoreCase (clipboardKey))
        return result;

    const auto parsed = juce::JSON::parse (text);   // kept: the list lives inside it
    const auto copied = parsed.getProperty (clipboardKey, {});

    if (const auto* list = copied.getArray())
        for (const auto& item : *list)
            if (item.isObject())
                result.push_back (GuiLayout::widgetFromVar (item));

    return result;
}

void PluginCanvas::pasteElements (std::optional<juce::Point<float>> at)
{
    auto pasted = clipboardElements();

    if (pasted.empty() || ! design || ! hasPanel())
        return;

    juce::Rectangle<int> area = pasted.front().bounds;

    for (const auto& w : pasted)
        area = area.getUnion (w.bounds);

    // Where the menu was opened, or a step beside the copies (one more for each paste).
    juce::Point<int> target;

    if (at.has_value())
    {
        target = { snapped (at->x), snapped (at->y) };
    }
    else
    {
        ++pasteCount;
        target = area.getPosition() + juce::Point<int> (snapStep * 4 * pasteCount, snapStep * 4 * pasteCount);
    }

    // Inside the window, where it fits.
    target.x = juce::jlimit (0, juce::jmax (0, layout.width - area.getWidth()), target.x);
    target.y = juce::jlimit (0, juce::jmax (0, layout.height - area.getHeight()), target.y);
    const auto offset = target - area.getPosition();
    const auto first = (int) layout.widgets.size();
    std::set<int> added;

    for (auto& w : pasted)
    {
        w.bounds.translate (offset.x, offset.y);

        // On the page shown (what was on every page stays so; with no pages, nothing has one).
        if (layout.pages.isEmpty())
            w.page.clear();
        else if (w.page.isNotEmpty())
            w.page = shownPageName();

        added.insert ((int) layout.widgets.size());
        layout.widgets.push_back (w);
    }

    // All selected: they move together.
    selectMany (added, first);
    edited();
}

void PluginCanvas::reorderSelection (bool toFront)
{
    const auto all = selection();

    if (all.empty())
        return;

    releaseKey();

    std::vector<GuiWidget> moving;

    for (const auto i : all)
        moving.push_back (layout.widgets[(size_t) i]);

    for (auto it = all.rbegin(); it != all.rend(); ++it)
        layout.widgets.erase (layout.widgets.begin() + *it);

    const auto at = toFront ? (int) layout.widgets.size() : 0;
    layout.widgets.insert (layout.widgets.begin() + at, moving.begin(), moving.end());
    indexesChanged();

    std::set<int> moved;

    for (int i = 0; i < (int) moving.size(); ++i)
        moved.insert (at + i);

    selectMany (moved, at + (int) moving.size() - 1);
    edited();
}

void PluginCanvas::navigateMinimapTo (juce::Point<float> view)
{
    const auto map = minimapArea();
    const auto world = worldBounds();
    const auto x = juce::jlimit (0.0f, 1.0f, (view.x - map.getX()) / map.getWidth());
    const auto y = juce::jlimit (0.0f, 1.0f, (view.y - map.getY()) / map.getHeight());

    autoFit = false;
    centreOn ({ world.getX() + world.getWidth() * x, world.getY() + world.getHeight() * y });
    viewChanged();
}

void PluginCanvas::drawGrid (juce::Graphics& g)
{
    // Fine lines every 16 plugin pixels, stronger every 64, lined up with the window;
    // lines that would crowd together when zoomed out are left out.
    const auto scale = hasPanel() ? zoom : 1.0f;
    const auto origin = hasPanel() ? juce::Point<float> (-pan.x * zoom, -pan.y * zoom) : juce::Point<float>();

    for (int step : { 16, 64 })
    {
        auto spacing = (float) step * scale;

        if (step == 16 && spacing < 7.0f)
            continue;

        while (spacing < 14.0f)
            spacing *= 4.0f;

        g.setColour (juce::Colours::white.withAlpha (step == 16 ? 0.028f : 0.05f));

        auto x = std::fmod (origin.x, spacing);
        auto y = std::fmod (origin.y, spacing);

        if (x < 0.0f) x += spacing;
        if (y < 0.0f) y += spacing;

        for (; x < (float) getWidth(); x += spacing)
            g.drawVerticalLine (juce::roundToInt (x), 0.0f, (float) getHeight());

        for (; y < (float) getHeight(); y += spacing)
            g.drawHorizontalLine (juce::roundToInt (y), 0.0f, (float) getWidth());
    }
}

void PluginCanvas::drawMinimap (juce::Graphics& g)
{
    const auto map = minimapArea();
    const auto world = worldBounds();

    g.setColour (juce::Colour (0xdd101014));
    g.fillRoundedRectangle (map, 4.0f);
    g.setColour (Theme::outline.brighter (0.2f));
    g.drawRoundedRectangle (map, 4.0f, 1.0f);

    auto toMap = [&] (juce::Rectangle<float> plugin)
    {
        return juce::Rectangle<float> (map.getX() + (plugin.getX() - world.getX()) / world.getWidth() * map.getWidth(),
                                       map.getY() + (plugin.getY() - world.getY()) / world.getHeight() * map.getHeight(),
                                       plugin.getWidth() / world.getWidth() * map.getWidth(),
                                       plugin.getHeight() / world.getHeight() * map.getHeight());
    };

    // The window and what's on it.
    const auto window = toMap ({ 0.0f, 0.0f, (float) layout.width, (float) layout.height });
    g.setColour (layout.backgroundTop.interpolatedWith (layout.backgroundBottom, 0.5f).brighter (0.25f));
    g.fillRect (window);

    for (int i = 0; i < (int) layout.widgets.size(); ++i)
    {
        const auto& w = layout.widgets[(size_t) i];

        if (w.type == GuiWidget::Type::group)
            continue;

        g.setColour (i == selected ? Theme::accent : (isInteractive (w) ? mapControl : Theme::muted));
        g.fillRect (toMap (w.bounds.toFloat()).expanded (0.5f));
    }

    // What's showing.
    const auto view = viewArea();
    const auto visible = toMap ({ pan.x, pan.y, view.getWidth() / zoom, view.getHeight() / zoom }).getIntersection (map);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.fillRect (visible);
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.drawRect (visible, 1.2f);

    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.setFont (Theme::font (10.0f, true));
    g.drawText ("MAP " + juce::String (juce::roundToInt (zoom * 100.0f)) + "%", map.reduced (5.0f, 3.0f).removeFromTop (12.0f),
                juce::Justification::centredLeft, false);
}

void PluginCanvas::drawScrollbars (juce::Graphics& g)
{
    for (const bool horizontal : { true, false })
    {
        const auto track = horizontal ? hScrollArea() : vScrollArea();
        const auto thumb = horizontal ? hThumb() : vThumb();
        const bool dragging = drag == (horizontal ? Drag::hScroll : Drag::vScroll);

        g.setColour (juce::Colour (0xff141418));
        g.fillRect (track);
        g.setColour (dragging ? Theme::text.withAlpha (0.7f) : Theme::muted.withAlpha (0.55f));
        g.fillRoundedRectangle (thumb, 3.0f);
    }

    // The corner where they meet.
    const auto view = viewArea();
    g.setColour (juce::Colour (0xff141418));
    g.fillRect (juce::Rectangle<float> (view.getRight(), view.getBottom(), scrollbarThickness, scrollbarThickness));
}

void PluginCanvas::repaintDropGhost()
{
    if (dropGhost.has_value())
        repaint (toView (*dropGhost).getSmallestIntegerContainer().expanded (3));
}

void PluginCanvas::drawDropGhost (juce::Graphics& g)
{
    if (! dropGhost.has_value())
        return;

    const auto box = toView (*dropGhost);
    g.setColour (Theme::accent.withAlpha (0.18f));
    g.fillRect (box);
    g.setColour (Theme::accent);
    g.drawRect (box, 1.5f);
}

void PluginCanvas::showDropGhost (const juce::String& primitiveId, juce::Point<float> view)
{
    const auto* primitive = PrimitiveLibrary::get().find (primitiveId);
    repaintDropGhost();   // where it was

    if (primitive == nullptr)
        dropGhost.reset();
    else if (primitive->setsBackground())
        dropGhost = juce::Rectangle<int> (0, 0, layout.width, layout.height);
    else
        dropGhost = placed (primitive->makeWidget().bounds, toPlugin (view));

    repaintDropGhost();   // where it is now
}

//==============================================================================
// The mouse
void PluginCanvas::mouseDown (const juce::MouseEvent& e)
{
    // Words typed in the menu's boxes go to what it shows now, before this click picks something
    // else (a box hears it lost the keyboard later, when the menu may show something else).
    menu->commitTyped();

    grabKeyboardFocus();

    drag = Drag::none;
    changed = dragMoved = false;
    menuOnRelease = false;
    pressedPanel = false;
    pressedBackdrop = -1;
    reopenMenu = menuOpen;
    carried.clear();
    dragStart = e.position;
    panAtDragStart = pan;

    if (! hasPanel())
        return;

    // The minimap and the scrollbars come first (as in Colosseum).
    if (e.mods.isLeftButtonDown() && ! panelOnly)
    {
        if (minimapArea().contains (e.position))
        {
            drag = Drag::minimap;
            navigateMinimapTo (e.position);
            return;
        }

        if (hScrollArea().contains (e.position) || vScrollArea().contains (e.position))
        {
            const bool horizontal = hScrollArea().contains (e.position);
            const auto thumb = horizontal ? hThumb() : vThumb();

            // A click beside the thumb jumps there; then it drags.
            if (! thumb.contains (e.position))
            {
                const auto world = worldBounds();
                const auto track = horizontal ? hScrollArea() : vScrollArea();
                const auto t = horizontal ? (e.position.x - track.getX()) / track.getWidth() : (e.position.y - track.getY()) / track.getHeight();
                const auto target = horizontal ? juce::Point<float> (world.getX() + world.getWidth() * t, toPlugin (viewArea().getCentre()).y)
                                               : juce::Point<float> (toPlugin (viewArea().getCentre()).x, world.getY() + world.getHeight() * t);
                autoFit = false;
                centreOn (target);
                viewChanged();
                panAtDragStart = pan;
            }

            drag = horizontal ? Drag::hScroll : Drag::vScroll;
            return;
        }
    }

    // The middle button always moves the view (a made plugin's window has nothing to move).
    if (e.mods.isMiddleButtonDown() && ! panelOnly)
    {
        drag = Drag::pan;
        return;
    }

    const auto plugin = toPlugin (e.position);
    const bool insidePanel = panelArea().contains (e.position);

    // Cropping: a handle drags its side or corner, inside moves the box, outside draws a new
    // one; a double-click inside crops.
    if (cropping)
    {
        if (! e.mods.isLeftButtonDown())
            return;

        if (e.getNumberOfClicks() >= 2 && toView (cropBox).contains (e.position))
        {
            applyCrop();
            return;
        }

        cropHandle = cropHandleAt (e.position);
        cropStart = cropBox;

        if (cropHandle < 0)
        {
            cropHandle = 9;
            cropStart = { juce::roundToInt (plugin.x), juce::roundToInt (plugin.y), 0, 0 };
        }

        drag = Drag::crop;
        return;
    }

    // The page tabs above the window (Edit): a click shows a page, a right-click its menu.
    for (const auto& [item, box] : pageStrip())
    {
        if (box.contains (e.position))
        {
            pageStripClicked (item, e);
            return;
        }
    }

    if (design)
    {
        // A right-click opens the menu: the element's (one of several selected: theirs), or
        // the window's on its empty space. Its Paste lands where it was opened.
        if (e.mods.isPopupMenu())
        {
            const auto index = insidePanel ? widgetAt (plugin, false) : -1;

            // A line: a right-drag moves it; its menu opens on release when it wasn't dragged.
            if (isLine (index) && manySelected.count (index) == 0)
            {
                if (index != selected)
                    select (index);

                pastePoint = plugin;
                menuOnRelease = true;
                drag = Drag::element;
                startBounds = layout.widgets[(size_t) selected].bounds;
                return;
            }

            if (index >= 0)
            {
                if (manySelected.count (index) == 0)
                    select (index);

                pastePoint = plugin;
                openMenu();
            }
            else if (insidePanel)
            {
                selectPanel();
                pastePoint = plugin;
                openMenu();
            }

            return;
        }

        // The selected element's handles resize it; the window's own corner resizes the window.
        // The round one above it turns it; a shape's own reshapes it.
        if (const auto handle = selected >= 0 ? handleAt (e.position) : -1; handle >= 0)
        {
            drag = handle == turnHandle ? Drag::turnElement
                 : handle == shapeHandle ? Drag::adjustShape
                 : (handle == lineStartHandle || handle == lineEndHandle) ? Drag::lineEnd : Drag::resizeElement;
            resizeHandle = handle;
            startBounds = layout.widgets[(size_t) selected].bounds;
            startWidget = layout.widgets[(size_t) selected];
            return;   // its menu stays open, following it
        }

        if (panelHandle().expanded (4.0f).contains (e.position))
        {
            drag = Drag::resizePanel;
            startPanelSize = { layout.width, layout.height };
            selectPanel();
            return;
        }

        // Shift: a click adds an element to the selection or takes it away; a drag on empty
        // space (or the background picture) draws a box that adds what it touches.
        if (e.mods.isShiftDown())
        {
            const auto picked = insidePanel ? widgetAt (plugin, false) : -1;

            if (picked >= 0 && ! (isBackdrop (layout.widgets[(size_t) picked]) && ! isSelected (picked)))
            {
                toggleSelection (picked);
                return;
            }

            drag = Drag::marquee;
            marquee = {};
            return;
        }

        const auto index = insidePanel ? widgetAt (plugin, false) : -1;

        // The background picture isn't grabbed by accident: until it's selected, a drag on it
        // moves the view and a click selects it.
        if (index >= 0 && index != selected && isBackdrop (layout.widgets[(size_t) index]))
        {
            pressedBackdrop = index;
            drag = Drag::pan;
            return;
        }

        if (index >= 0)
        {
            // A tab of the page tabs shows its page here too (and the tabs are picked, to move).
            if (const auto& picked = layout.widgets[(size_t) index]; picked.type == GuiWidget::Type::tabs)
                if (const auto tab = tabAt (picked, intoWidget (picked, plugin)); tab >= 0)
                    showPage (tab);

            // An element: it's selected now (a menu open for another one closes: the menu is
            // the right-click's). One of several selected: they all move with it.
            if (menuOpen && ! isSelected (index))
                closeMenu();

            reopenMenu = menuOpen;
            const bool many = manySelected.count (index) > 0;

            if (many)
                selected = index;
            else
                select (index);

            // A line alone: a left-drag bows it where it was grabbed (a right-drag moves it).
            if (! many && isLine (index))
            {
                drag = Drag::lineBend;
                startBounds = layout.widgets[(size_t) selected].bounds;
                startWidget = layout.widgets[(size_t) selected];
                lineGrab = ObjectPaint::lineParameterAt (startWidget, intoWidget (startWidget, plugin) - startBounds.getPosition().toFloat());
                return;
            }

            drag = Drag::element;
            startBounds = layout.widgets[(size_t) selected].bounds;

            // What moves along: the others selected, and what lies inside a frame that moves.
            const auto movers = many ? manySelected : std::set<int> { selected };
            std::set<int> along (movers.begin(), movers.end());

            for (const auto mover : movers)
                if (layout.widgets[(size_t) mover].type == GuiWidget::Type::group)
                    for (int i = 0; i < (int) layout.widgets.size(); ++i)
                        if (layout.widgets[(size_t) mover].bounds.contains (layout.widgets[(size_t) i].bounds))
                            along.insert (i);

            along.erase (selected);

            for (const auto i : along)
                carried.push_back ({ i, layout.widgets[(size_t) i].bounds });

            return;
        }

        // Empty space: a click selects the window (or nothing, outside it); a drag moves the view.
        pressedPanel = insidePanel;
        drag = Drag::pan;
        return;
    }

    // Play mode. A programmed element's open popup takes the click; a click anywhere else
    // closes it, and goes no further.
    if (const auto owner = popupAt (plugin); owner >= 0)
    {
        active = owner;
        drag = Drag::control;
        pressedPopup = true;
        elementMouse (owner, true, 0, plugin, e.mods, e.getNumberOfClicks() > 1);
        return;
    }

    if (anyPopupOpen())
    {
        closeElementPopups();
        return;
    }

    // A protected plugin's registration badge opens its Registration window.
    if (onRegistrationClicked != nullptr && ! registered && insidePanel && e.mods.isLeftButtonDown())
    {
        if (const auto badge = widgetAt (plugin, false); badge >= 0 && layout.widgets[(size_t) badge].type == GuiWidget::Type::registration)
        {
            onRegistrationClicked();
            return;
        }
    }

    // The controls work; elsewhere a drag moves the view.
    active = insidePanel ? widgetAt (plugin, true) : -1;

    if (active < 0)
    {
        drag = Drag::pan;
        return;
    }

    drag = Drag::control;
    const auto& w = layout.widgets[(size_t) active];

    if (w.type == GuiWidget::Type::custom)
    {
        // It takes the drag and the release only if it asks for them.
        pressedPopup = false;

        if (! elementMouse (active, false, 0, plugin, e.mods, e.getNumberOfClicks() > 1))
        {
            active = -1;
            drag = Drag::none;
        }

        return;
    }

    // Played in its own frame: a turned element as it's turned.
    const auto local = intoWidget (w, plugin);

    if (w.type == GuiWidget::Type::tabs)
    {
        active = -1;
        drag = Drag::none;
        showPage (tabAt (w, local));
        return;
    }

    if (w.type == GuiWidget::Type::keyboard)
    {
        playKey (active, local);
        return;
    }

    if (w.type == GuiWidget::Type::xy)
    {
        setXy (w, local);
        return;
    }

    if (w.type == GuiWidget::Type::preset)
    {
        active = -1;
        drag = Drag::none;

        // The arrows step; the middle (or a right-click) opens the preset manager: the presets
        // under their headings, then Save, Save as, Load, the banks...
        const auto x = (local.x - (float) w.bounds.getX()) / (float) juce::jmax (1, w.bounds.getWidth());

        if (! e.mods.isPopupMenu() && (x < 0.22f || x > 0.78f))
        {
            if (const auto next = presetMenu.step (x < 0.22f ? -1 : 1); next >= 0 && onPresetChosen != nullptr)
                onPresetChosen (next);

            return;
        }

        if (presetMenu.names.isEmpty() && onPresetAction == nullptr)
            return;

        static constexpr int presetIds = 1, actionIds = 100000;
        juce::PopupMenu choices;
        PresetMenus::addPresets (choices, presetMenu, presetIds);

        if (onPresetAction != nullptr)
        {
            choices.addSeparator();
            PresetMenus::addActions (choices, presetMenu, actionIds);
        }

        choices.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (toView (w.bounds).toNearestInt()))
                                                      .withMinimumWidth (juce::jmax (180, (int) toView (w.bounds).getWidth())),
                            [safeThis = juce::Component::SafePointer<PluginCanvas> (this)] (int choice)
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
        return;
    }

    const auto* p = paramFor (w);

    if (p == nullptr)
    {
        active = -1;
        drag = Drag::pan;
        return;
    }

    startProportion = proportionOf (*p, currentValue (*p));

    if (w.type == GuiWidget::Type::toggle)
    {
        setValue (*p, startProportion >= 0.5f ? p->min : p->max);
    }
    else if (w.type == GuiWidget::Type::selector)
    {
        // The position under the mouse, as its display lays them out (a glowing display's
        // arrows step through them).
        const auto count = juce::jmax (1, w.options.size() > 0 ? w.options.size() : (int) (p->max - p->min) + 1);
        const auto chosen = juce::roundToInt (currentValue (*p) - p->min);
        const auto area = w.bounds.toFloat().withTrimmedBottom ((float) GuiLayout::captionHeight);

        if (const auto picked = PanelParts::selectorPick (w, area, count, chosen, local); picked >= 0)
            setValue (*p, p->min + (float) picked);
    }
    else if (w.type == GuiWidget::Type::slider)
    {
        mouseDrag (e);
    }

    repaint();
}

void PluginCanvas::followMenu()
{
    // A menu open for what's dragged stays open: its position, size and angle follow.
    if (menuOpen && menu != nullptr && menu->isVisible())
        menu->followDrag();
}

void PluginCanvas::mouseDrag (const juce::MouseEvent& e)
{
    const auto offset = e.position - dragStart;
    const auto delta = offset / zoom;   // in plugin pixels

    if (e.getDistanceFromDragStart() > 3)
        dragMoved = true;

    switch (drag)
    {
        case Drag::minimap:
            navigateMinimapTo (e.position);
            return;

        case Drag::hScroll:
        case Drag::vScroll:
        {
            const bool horizontal = drag == Drag::hScroll;
            const auto world = worldBounds();
            const auto track = horizontal ? hScrollArea() : vScrollArea();
            const auto thumb = horizontal ? hThumb() : vThumb();
            const auto travel = (horizontal ? track.getWidth() - thumb.getWidth() : track.getHeight() - thumb.getHeight());
            const auto range = horizontal ? world.getWidth() - viewArea().getWidth() / zoom : world.getHeight() - viewArea().getHeight() / zoom;

            if (travel > 0.0f && range > 0.0f)
            {
                autoFit = false;

                if (horizontal) pan.x = panAtDragStart.x + offset.x / travel * range;
                else            pan.y = panAtDragStart.y + offset.y / travel * range;

                clampPan();
                viewChanged();
            }
            return;
        }

        case Drag::pan:
        {
            if (! dragMoved || panelOnly)
                return;

            autoFit = false;
            pan = panAtDragStart - delta;
            clampPan();
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            viewChanged();
            return;
        }

        case Drag::crop:
        {
            if (dragMoved || cropHandle != 9)
            {
                dragCrop (delta, e.mods.isShiftDown());
                repaint();
            }
            return;
        }

        case Drag::resizePanel:
        {
            // Never smaller than what's on it; a picture filling the window keeps filling it.
            const juce::Rectangle<int> oldWindow (0, 0, layout.width, layout.height);
            int needW = GuiLayout::minWidth, needH = GuiLayout::minHeight;

            for (const auto& w : layout.widgets)
            {
                if (w.type == GuiWidget::Type::image && w.bounds == oldWindow)
                    continue;

                needW = juce::jmax (needW, w.bounds.getRight());
                needH = juce::jmax (needH, w.bounds.getBottom());
            }

            layout.width = juce::jlimit (juce::jmin (needW, GuiLayout::maxWidth), GuiLayout::maxWidth, snapped ((float) startPanelSize.x + delta.x));
            layout.height = juce::jlimit (juce::jmin (needH, GuiLayout::maxHeight), GuiLayout::maxHeight, snapped ((float) startPanelSize.y + delta.y));

            for (auto& w : layout.widgets)
                if (w.type == GuiWidget::Type::image && w.bounds == oldWindow)
                    w.bounds = { 0, 0, layout.width, layout.height };
            changed = true;
            followMenu();
            autoFit = false;
            clampPan();
            layOutOverlays();
            repaint();
            return;
        }

        case Drag::turnElement:
        {
            if (selected < 0 || ! dragMoved)
                return;

            // Round its middle, as far as the mouse goes round it; Shift: steps of 15 degrees,
            // and it sticks to upright and the quarters on its way past.
            auto& w = layout.widgets[(size_t) selected];
            const auto centre = toView (startBounds).getCentre();
            const auto from = dragStart - centre, to = e.position - centre;
            auto angle = startWidget.angle + juce::radiansToDegrees (std::atan2 (to.y, to.x) - std::atan2 (from.y, from.x));

            if (e.mods.isShiftDown())
                angle = 15.0f * (float) juce::roundToInt (angle / 15.0f);
            else if (const auto quarter = 90.0f * (float) juce::roundToInt (angle / 90.0f); std::abs (angle - quarter) < 3.0f)
                angle = quarter;

            angle = std::fmod (angle, 360.0f);

            if (angle > 180.0f)        angle -= 360.0f;
            else if (angle <= -180.0f) angle += 360.0f;

            w.angle = (float) juce::roundToInt (angle * 10.0f) / 10.0f;
            changed = true;
            followMenu();
            repaint();
            return;
        }

        case Drag::adjustShape:
        {
            if (selected < 0)
                return;

            // The mouse's move, in the shape's own upright box.
            auto& w = layout.widgets[(size_t) selected];
            const auto moved = delta.rotatedAboutOrigin (juce::degreesToRadians (-startWidget.shownAngle()));
            ObjectPaint::adjust (w, startWidget, moved);
            changed = true;
            followMenu();
            repaint();
            return;
        }

        case Drag::resizeElement:
        {
            if (selected < 0)
                return;

            auto& w = layout.widgets[(size_t) selected];

            if (startWidget.isTurned())
            {
                // Turned: its sides move along its own axes, and the side across from the handle
                // stays where it is.
                const float turnRadians = juce::degreesToRadians (startWidget.shownAngle());
                const auto along = delta.rotatedAboutOrigin (-turnRadians);
                const bool turnedKnob = w.type == GuiWidget::Type::knob;
                const auto turnedCaption = turnedKnob ? GuiLayout::captionHeight : 0;
                const float minimum = 12.0f;
                const bool l = resizeHandle == 0 || resizeHandle == 6 || resizeHandle == 7;
                const bool r = resizeHandle == 2 || resizeHandle == 3 || resizeHandle == 4;
                const bool u = resizeHandle == 0 || resizeHandle == 1 || resizeHandle == 2;
                const bool d = resizeHandle == 4 || resizeHandle == 5 || resizeHandle == 6;

                auto box = startBounds.toFloat();
                float x0 = box.getX(), y0 = box.getY(), x1 = box.getRight(), y1 = box.getBottom();

                if (l) x0 = juce::jmin (x1 - minimum, x0 + along.x);
                if (r) x1 = juce::jmax (x0 + minimum, x1 + along.x);
                if (u) y0 = juce::jmin (y1 - minimum - (float) turnedCaption, y0 + along.y);
                if (d) y1 = juce::jmax (y0 + minimum + (float) turnedCaption, y1 + along.y);

                if (turnedKnob || ((l || r) && (u || d) && e.mods.isShiftDown()))
                {
                    // Its shape kept: the bigger change wins (a knob stays round over its caption).
                    const float startW = box.getWidth(), startH = box.getHeight() - (float) turnedCaption;
                    const float scale = std::abs ((x1 - x0) / startW - 1.0f) >= std::abs ((y1 - y0 - (float) turnedCaption) / startH - 1.0f)
                                            ? (x1 - x0) / startW : (y1 - y0 - (float) turnedCaption) / startH;
                    const float newW = juce::jmax (minimum, startW * scale), newH = turnedKnob ? newW : juce::jmax (minimum, startH * scale);

                    if (l) x0 = x1 - newW; else x1 = x0 + newW;
                    if (u) y0 = y1 - (newH + (float) turnedCaption); else y1 = y0 + newH + (float) turnedCaption;
                }

                const juce::Point<float> middle ((x0 + x1) * 0.5f, (y0 + y1) * 0.5f);
                const auto centre = box.getCentre() + (middle - box.getCentre()).rotatedAboutOrigin (turnRadians);
                w.bounds = juce::Rectangle<float> (x1 - x0, y1 - y0).withCentre (centre).toNearestInt();
                changed = true;
                followMenu();
                repaint();
                return;
            }

            const bool knob = w.type == GuiWidget::Type::knob;
            const auto caption = knob ? GuiLayout::captionHeight : 0;
            const auto minW = 12, minH = 12 + caption;

            // The sides the handle moves; the others stay where they were.
            const bool left = resizeHandle == 0 || resizeHandle == 6 || resizeHandle == 7;
            const bool right = resizeHandle == 2 || resizeHandle == 3 || resizeHandle == 4;
            const bool top = resizeHandle == 0 || resizeHandle == 1 || resizeHandle == 2;
            const bool bottom = resizeHandle == 4 || resizeHandle == 5 || resizeHandle == 6;

            auto x0 = startBounds.getX(), y0 = startBounds.getY(), x1 = startBounds.getRight(), y1 = startBounds.getBottom();

            // A side near the window's edge sticks to it, so a picture is easily made to fill it.
            auto toEdge = [] (int v, int edge) { return std::abs (v - edge) <= edgeSnap ? edge : v; };

            if (left)   x0 = juce::jmin (x1 - minW, toEdge (snapped ((float) x0 + delta.x), 0));
            if (right)  x1 = juce::jmax (x0 + minW, toEdge (snapped ((float) x1 + delta.x), layout.width));
            if (top)    y0 = juce::jmin (y1 - minH, toEdge (snapped ((float) y0 + delta.y), 0));
            if (bottom) y1 = juce::jmax (y0 + minH, toEdge (snapped ((float) y1 + delta.y), layout.height));

            auto bounds = juce::Rectangle<int>::leftTopRightBottom (x0, y0, x1, y1);

            // A knob stays round (its caption stays under it); Shift on a corner keeps the shape.
            if (knob || ((left || right) && (top || bottom) && e.mods.isShiftDown()))
            {
                const auto startW = (float) startBounds.getWidth(), startH = (float) (startBounds.getHeight() - caption);
                const auto scaleW = (float) bounds.getWidth() / juce::jmax (1.0f, startW);
                const auto scaleH = (float) (bounds.getHeight() - caption) / juce::jmax (1.0f, startH);
                const auto scale = std::abs (scaleW - 1.0f) >= std::abs (scaleH - 1.0f) ? scaleW : scaleH;
                const auto newW = juce::jmax (minW, juce::roundToInt (startW * scale));
                const auto newH = knob ? newW : juce::jmax (minH - caption, juce::roundToInt (startH * scale));

                // Anchored at the corner opposite the handle.
                const auto anchorX = left ? startBounds.getRight() : startBounds.getX();
                const auto anchorY = top ? startBounds.getBottom() : startBounds.getY();
                bounds = { left ? anchorX - newW : anchorX, top ? anchorY - (newH + caption) : anchorY, newW, newH + caption };
            }

            w.bounds = bounds;
            changed = true;
            followMenu();
            repaint();
            return;
        }

        case Drag::element:
        {
            if (selected < 0 || ! dragMoved)
                return;

            auto& w = layout.widgets[(size_t) selected];
            const auto dx = snapped (delta.x), dy = snapped (delta.y);
            w.bounds = startBounds.translated (dx, dy);

            for (const auto& [index, bounds] : carried)
                layout.widgets[(size_t) index].bounds = bounds.translated (dx, dy);

            changed = true;

            followMenu();
            repaint();
            return;
        }

        case Drag::lineEnd:
        case Drag::lineBend:
        {
            if (selected < 0 || ! dragMoved)
                return;

            // In the line's own frame (where its points are kept), from its box as it was; the
            // box is fitted round it again when the drag ends. Its ends stick to the design grid
            // (Alt: free).
            auto& w = layout.widgets[(size_t) selected];
            auto plugin = toPlugin (e.position);

            if (drag == Drag::lineEnd && ! e.mods.isAltDown())
                plugin = { (float) snapped (plugin.x), (float) snapped (plugin.y) };

            const auto local = intoWidget (startWidget, plugin) - startBounds.getPosition().toFloat();

            if (drag == Drag::lineEnd)
                ObjectPaint::moveLineEnd (w, startWidget, resizeHandle == lineStartHandle, local);
            else
                ObjectPaint::pullLine (w, startWidget, lineGrab, local);

            changed = true;
            followMenu();
            repaint();
            return;
        }

        case Drag::marquee:
        {
            marquee = juce::Rectangle<float> (toPlugin (dragStart), toPlugin (e.position)).toNearestInt();
            repaint();
            return;
        }

        case Drag::control:
        {
            if (active < 0)
                return;

            const auto& w = layout.widgets[(size_t) active];

            if (w.type == GuiWidget::Type::custom)
            {
                elementMouse (active, pressedPopup, 1, toPlugin (e.position), e.mods);
                return;
            }

            // Sliding across the keys plays each in turn; off the keyboard, nothing plays.
            if (w.type == GuiWidget::Type::keyboard)
            {
                playKey (active, intoWidget (w, toPlugin (e.position)));
                return;
            }

            if (w.type == GuiWidget::Type::xy)
            {
                setXy (w, intoWidget (w, toPlugin (e.position)));
                return;
            }

            const auto* p = paramFor (w);

            if (p == nullptr)
                return;

            if (w.type == GuiWidget::Type::knob)
            {
                // Up or right turns it up; Shift moves it finely.
                const auto travel = e.mods.isShiftDown() ? 900.0f : 200.0f;
                setValue (*p, valueAt (*p, startProportion + (delta.x - delta.y) / travel));
            }
            else if (w.type == GuiWidget::Type::slider)
            {
                const auto plugin = intoWidget (w, toPlugin (e.position));
                const auto area = w.bounds.toFloat().withTrimmedBottom ((float) GuiLayout::captionHeight).reduced (4.0f);
                const auto t = w.vertical ? (area.getBottom() - plugin.y) / area.getHeight() : (plugin.x - area.getX()) / area.getWidth();
                setValue (*p, valueAt (*p, t));
            }
            return;
        }

        case Drag::none:
            return;
    }
}

void PluginCanvas::mouseUp (const juce::MouseEvent& e)
{
    const auto was = drag;
    drag = Drag::none;

    if (was == Drag::crop)
    {
        repaint();
        return;
    }

    if (was == Drag::resizePanel && changed)
        fitPaintedBackdrops();

    // A line reshaped: its box made snug round it again.
    if (design && changed && (was == Drag::lineEnd || was == Drag::lineBend || was == Drag::adjustShape) && isLine (selected))
    {
        auto& w = layout.widgets[(size_t) selected];
        ObjectPaint::setLinePoints (w, ObjectPaint::linePoints (w), true);
    }

    if (was == Drag::control && juce::isPositiveAndBelow (active, (int) layout.widgets.size())
        && layout.widgets[(size_t) active].type == GuiWidget::Type::custom)
    {
        elementMouse (active, pressedPopup, 2, toPlugin (e.position), e.mods);
        pressedPopup = false;
    }

    releaseKey();
    setMouseCursor (juce::MouseCursor::NormalCursor);

    if (design && (was == Drag::element || was == Drag::resizeElement || was == Drag::turnElement || was == Drag::adjustShape
                   || was == Drag::resizePanel || was == Drag::lineEnd || was == Drag::lineBend))
        repaint();   // the alignment grid goes with the drag

    if (design && changed)
        edited();

    if (was == Drag::marquee)
    {
        // What the box touches joins the selection (a frame only when it's all inside).
        if (dragMoved && ! marquee.isEmpty())
        {
            const auto current = selection();
            std::set<int> indexes (current.begin(), current.end());
            int last = selected;

            for (int i = 0; i < (int) layout.widgets.size(); ++i)
            {
                const auto& w = layout.widgets[(size_t) i];
                const auto covers = w.turnedArea().getSmallestIntegerContainer();
                const bool touched = w.type == GuiWidget::Type::group ? marquee.contains (covers)
                                                                      : ! isBackdrop (w) && marquee.intersects (covers);

                if (touched)
                {
                    indexes.insert (i);
                    last = i;
                }
            }

            selectMany (indexes, last);
        }

        marquee = {};
        repaint();
    }

    if (design)
    {
        if (was == Drag::element && ! dragMoved)
        {
            if (menuOnRelease)
                openMenu();                     // a right-click on a line that didn't move it
            else if (! manySelected.empty())
                select (selected);              // a click on one of several: just that one
        }
        else if (was == Drag::element || was == Drag::resizeElement || was == Drag::turnElement || was == Drag::adjustShape
                 || was == Drag::resizePanel || was == Drag::lineEnd || was == Drag::lineBend)
        {
            if (reopenMenu)
                openMenu();                     // back where it was before the move
        }
        else if (was == Drag::pan && ! dragMoved)
        {
            // A click on the background picture selects it; on the window's empty space, the
            // window; outside it: nothing. A menu open for something else closes.
            if (juce::isPositiveAndBelow (pressedBackdrop, (int) layout.widgets.size()))
            {
                if (menuOpen && ! isSelected (pressedBackdrop))
                    closeMenu();

                select (pressedBackdrop);
            }
            else if (pressedPanel)
            {
                if (menuOpen && ! panelSelected)
                    closeMenu();

                selectPanel();
            }
            else
            {
                select (-1);
            }
        }
    }

    changed = false;
    menuOnRelease = false;
    active = -1;
    carried.clear();
    repaint();
}

void PluginCanvas::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (cropping || ! hasPanel() || ! panelArea().contains (e.position))
        return;

    const auto index = widgetAt (toPlugin (e.position), ! design);

    if (index < 0)
        return;

    const auto& w = layout.widgets[(size_t) index];

    if (design)
    {
        // A knob, slider or switch: its menu, where "Design it in the Knob Studio" is.
        if (Looks::takesLook (w))
        {
            select (index);
            openMenu();
        }
        else if (w.type == GuiWidget::Type::image)
        {
            choosePicture (index);
        }
        else if (isLine (index) && index == selected && handleAt (e.position) == shapeHandle)
        {
            // A line's bend point: straight again.
            ObjectPaint::straighten (layout.widgets[(size_t) index]);
            edited();
            repaint();
        }
        else if (w.type == GuiWidget::Type::shape)
        {
            // A shape: Photoshop's Layer Style, as a double-click on a layer opens it.
            select (index);
            editLayerStyle (index);
        }

        return;
    }

    if (const auto* p = paramFor (w); p != nullptr && (w.type == GuiWidget::Type::knob || w.type == GuiWidget::Type::slider))
        setValue (*p, p->def);
}

void PluginCanvas::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (! hasPanel())
        return;

    // Ctrl + wheel zooms around the mouse (Colosseum's step).
    if ((e.mods.isCtrlDown() || e.mods.isCommandDown()) && ! panelOnly)
    {
        autoFit = false;
        setZoom (zoom + wheel.deltaY * 0.15f * juce::jmax (0.5f, zoom), e.position);
        return;
    }

    // Play mode: the wheel turns the control under the mouse (a popup first).
    if (! design)
    {
        if (const auto owner = popupAt (toPlugin (e.position)); owner >= 0)
        {
            elementMouse (owner, true, 5, toPlugin (e.position), e.mods, false, wheel.deltaY * 4.0f);
            return;
        }

        const auto index = panelArea().contains (e.position) ? widgetAt (toPlugin (e.position), true) : -1;

        if (index >= 0)
        {
            const auto& w = layout.widgets[(size_t) index];

            if (w.type == GuiWidget::Type::custom)
            {
                elementMouse (index, false, 5, toPlugin (e.position), e.mods, false, wheel.deltaY * 4.0f);
                return;
            }

            if (const auto* p = paramFor (w); p != nullptr && (w.type == GuiWidget::Type::knob || w.type == GuiWidget::Type::slider))
            {
                setValue (*p, valueAt (*p, proportionOf (*p, currentValue (*p)) + wheel.deltaY * 0.08f));
                return;
            }
        }
    }

    // Otherwise it scrolls: down and up, Shift for sideways (a trackpad's sideways moves too).
    if (panelOnly)
        return;

    autoFit = false;

    if (e.mods.isShiftDown())
        pan.x -= wheel.deltaY * scrollSpeed / zoom;
    else
    {
        pan.y -= wheel.deltaY * scrollSpeed / zoom;
        pan.x -= wheel.deltaX * scrollSpeed / zoom;
    }

    clampPan();
    viewChanged();
}

void PluginCanvas::mouseMove (const juce::MouseEvent& e)
{
    if (hasPanel())
        hoverElements (toPlugin (e.position), e.mods);

    const auto index = hasPanel() && panelArea().contains (e.position) ? widgetAt (toPlugin (e.position), ! design) : -1;

    if (index != hovered)
    {
        hovered = index;
        repaint();
    }

    // The crop box's handles pull its sides and corners; inside, it moves.
    if (cropping)
    {
        static const juce::MouseCursor::StandardCursorType cropCursors[] {
            juce::MouseCursor::TopLeftCornerResizeCursor, juce::MouseCursor::UpDownResizeCursor, juce::MouseCursor::TopRightCornerResizeCursor,
            juce::MouseCursor::LeftRightResizeCursor, juce::MouseCursor::BottomRightCornerResizeCursor, juce::MouseCursor::UpDownResizeCursor,
            juce::MouseCursor::BottomLeftCornerResizeCursor, juce::MouseCursor::LeftRightResizeCursor, juce::MouseCursor::DraggingHandCursor };
        const auto cropAt = cropHandleAt (e.position);
        setMouseCursor (cropAt >= 0 ? cropCursors[cropAt] : juce::MouseCursor::CrosshairCursor);
        return;
    }

    // The handles show what they do: a resize handle points the way it pulls (turned with its
    // element), the turn and the shape's own take the hand.
    const auto handle = design && hasPanel() && selected >= 0 ? handleAt (e.position) : -1;

    if (handle >= 8)
    {
        setMouseCursor (handle == turnHandle ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::PointingHandCursor);
        return;
    }

    if (handle >= 0)
    {
        // Its direction from the middle, turned, to the nearest of the four resize cursors.
        static const float directions[] { 225.0f, 270.0f, 315.0f, 0.0f, 45.0f, 90.0f, 135.0f, 180.0f };
        const auto turned = std::fmod (directions[handle] + layout.widgets[(size_t) selected].shownAngle() + 720.0f, 180.0f);
        const auto step = juce::roundToInt (turned / 45.0f) % 4;
        static const juce::MouseCursor::StandardCursorType cursors[] {
            juce::MouseCursor::LeftRightResizeCursor, juce::MouseCursor::TopLeftCornerResizeCursor,
            juce::MouseCursor::UpDownResizeCursor, juce::MouseCursor::TopRightCornerResizeCursor };

        setMouseCursor (cursors[step]);
        return;
    }

    const bool onPanelHandle = design && hasPanel() && panelHandle().expanded (4.0f).contains (e.position);
    const bool onBadge = ! design && onRegistrationClicked != nullptr && ! registered && hasPanel() && panelArea().contains (e.position)
                         && [this, &e] { const auto i = widgetAt (toPlugin (e.position), false);
                                         return i >= 0 && layout.widgets[(size_t) i].type == GuiWidget::Type::registration; }();
    setMouseCursor (onPanelHandle ? juce::MouseCursor::BottomRightCornerResizeCursor
                                  : onBadge ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void PluginCanvas::mouseExit (const juce::MouseEvent& e)
{
    if (hoveredElement >= 0)
        elementMouse (hoveredElement, hoveredPopup, 4, toPlugin (e.position), e.mods);

    hoveredElement = -1;
    hovered = -1;
    repaint();
}

bool PluginCanvas::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();

    // The view: Ctrl+0 fits, Ctrl+1 is 100%, Ctrl + / - zoom.
    if (hasPanel() && ! panelOnly && (mods.isCtrlDown() || mods.isCommandDown()))
    {
        if (key.isKeyCode ('0'))                          { fitToView(); return true; }
        if (key.isKeyCode ('1'))                          { autoFit = false; setZoom (1.0f); return true; }
        if (key.isKeyCode ('=') || key.isKeyCode ('+'))   { autoFit = false; setZoom (zoom * 1.25f); return true; }
        if (key.isKeyCode ('-'))                          { autoFit = false; setZoom (zoom / 1.25f); return true; }
    }

    if (! design)
        return false;

    // The crop tool: C takes it, Enter crops, Esc leaves it; the arrows nudge the box (Shift:
    // further).
    if (cropping)
    {
        if (key == juce::KeyPress::returnKey)  { applyCrop(); return true; }
        if (key == juce::KeyPress::escapeKey)  { cancelCrop(); return true; }

        const auto nudge = mods.isShiftDown() ? 8 : 1;
        juce::Point<int> by;

        if (key.isKeyCode (juce::KeyPress::leftKey))  by = { -nudge, 0 };
        if (key.isKeyCode (juce::KeyPress::rightKey)) by = { nudge, 0 };
        if (key.isKeyCode (juce::KeyPress::upKey))    by = { 0, -nudge };
        if (key.isKeyCode (juce::KeyPress::downKey))  by = { 0, nudge };

        cropBox.translate (by.x, by.y);
        repaint();
        return true;
    }

    if (hasPanel() && ! panelOnly && ! mods.isAnyModifierKeyDown()
        && juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) key.getKeyCode()) == 'C')
    {
        startCrop();
        return true;
    }

    if (key == juce::KeyPress::escapeKey && (menuOpen || selected >= 0 || panelSelected))
    {
        select (-1);
        return true;
    }

    // Ctrl + a letter (some systems give the key's code in lower case, some in upper).
    auto ctrlAnd = [&] (char letter)
    {
        return (mods.isCtrlDown() || mods.isCommandDown())
            && juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) key.getKeyCode()) == (juce::juce_wchar) letter;
    };

    // Ctrl+V pastes under the mouse (or a step beside the copies, with the mouse elsewhere).
    if (ctrlAnd ('V') && hasPanel())
    {
        const auto mouse = getMouseXYRelative().toFloat();
        pasteElements (panelArea().contains (mouse) ? std::optional<juce::Point<float>> (toPlugin (mouse)) : std::nullopt);
        return true;
    }

    if (selected < 0)
        return false;

    if (ctrlAnd ('C'))
    {
        copyElements (selection());
        return true;
    }

    if (ctrlAnd ('D'))
    {
        copyElements (selection());
        pasteElements ({});
        return true;
    }

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        removeSelected();
        return true;
    }

    // Layers: Ctrl+] forward, Ctrl+[ backward; with Shift, all the way.
    if (mods.isCtrlDown() || mods.isCommandDown())
    {
        const auto code = key.getKeyCode();

        // Several: all to the front or the back, keeping their order.
        if (! manySelected.empty() && (code == ']' || code == '}' || code == '[' || code == '{'))
        {
            reorderSelection (code == ']' || code == '}');
            return true;
        }

        if (code == ']' || code == '}')
        {
            reorder (selected, mods.isShiftDown() ? Order::front : Order::forward);
            return true;
        }

        if (code == '[' || code == '{')
        {
            reorder (selected, mods.isShiftDown() ? Order::back : Order::backward);
            return true;
        }
    }

    const auto step = mods.isShiftDown() ? 8 : 1;
    juce::Point<int> move;

    if (key.isKeyCode (juce::KeyPress::leftKey))  move = { -step, 0 };
    if (key.isKeyCode (juce::KeyPress::rightKey)) move = { step, 0 };
    if (key.isKeyCode (juce::KeyPress::upKey))    move = { 0, -step };
    if (key.isKeyCode (juce::KeyPress::downKey))  move = { 0, step };

    if (move.isOrigin())
        return false;

    for (const auto index : selection())
        layout.widgets[(size_t) index].bounds.translate (move.x, move.y);

    edited();
    return true;
}

//==============================================================================
// Primitives from the toolbox
bool PluginCanvas::isInterestedInDragSource (const SourceDetails& details)
{
    return design && hasPanel()
        && (PrimitiveLibrary::idFromDrag (details.description).isNotEmpty()
            || KnobStudio::galleryDragFile (details.description) != juce::File());
}

void PluginCanvas::itemDragEnter (const SourceDetails& details)
{
    itemDragMove (details);
}

void PluginCanvas::itemDragMove (const SourceDetails& details)
{
    // A Gallery item: over a knob, slider or switch, that control (it takes the item's look);
    // elsewhere, a new control's place.
    if (KnobStudio::galleryDragFile (details.description) != juce::File())
    {
        repaintDropGhost();
        const auto at = toPlugin (details.localPosition.toFloat());
        const auto over = widgetAt (at, false);

        if (over >= 0 && Looks::takesLook (layout.widgets[(size_t) over]))
            dropGhost = layout.widgets[(size_t) over].bounds;
        else
            dropGhost = placed (defaultBounds (GuiWidget::Type::knob, 0, 0), at);

        repaintDropGhost();
        return;
    }

    showDropGhost (PrimitiveLibrary::idFromDrag (details.description), details.localPosition.toFloat());
}

void PluginCanvas::itemDragExit (const SourceDetails&)
{
    repaintDropGhost();
    dropGhost.reset();
}

void PluginCanvas::itemDropped (const SourceDetails& details)
{
    dropGhost.reset();
    repaint();

    if (const auto file = KnobStudio::galleryDragFile (details.description); file != juce::File())
    {
        const auto at = toPlugin (details.localPosition.toFloat());
        const auto over = widgetAt (at, false);

        if (over >= 0 && Looks::takesLook (layout.widgets[(size_t) over]))
            useGalleryItem (over, file);   // dropped on a control: it wears the item
        else
            addGalleryItem (file, at);     // on the panel: a new control wearing it

        return;
    }

    if (const auto* primitive = PrimitiveLibrary::get().find (PrimitiveLibrary::idFromDrag (details.description)))
        addPrimitive (*primitive, toPlugin (details.localPosition.toFloat()));
}

//==============================================================================
// Pictures from the desktop
bool PluginCanvas::isInterestedInFileDrag (const juce::StringArray& files)
{
    if (! design || ! hasPanel())
        return false;

    for (const auto& path : files)
        if (GuiLayout::isPicture (juce::File (path)))
            return true;

    return false;
}

void PluginCanvas::fileDragEnter (const juce::StringArray& files, int x, int y)
{
    fileDragMove (files, x, y);
}

void PluginCanvas::fileDragMove (const juce::StringArray&, int x, int y)
{
    repaintDropGhost();
    dropGhost = placed ({ 0, 0, 160, 120 }, toPlugin ({ (float) x, (float) y }));
    repaintDropGhost();
}

void PluginCanvas::fileDragExit (const juce::StringArray&)
{
    repaintDropGhost();
    dropGhost.reset();
}

void PluginCanvas::filesDropped (const juce::StringArray& files, int x, int y)
{
    dropGhost.reset();
    auto centre = toPlugin ({ (float) x, (float) y });

    for (const auto& path : files)
    {
        const juce::File file (path);
        const auto name = GuiLayout::isPicture (file) ? GuiLayout::importPicture (file, imagesFolder()) : juce::String();

        if (name.isEmpty())
            continue;

        GuiWidget w;
        w.type = GuiWidget::Type::image;
        w.mode = "fit";
        w.bounds = { 0, 0, 160, 120 };

        const auto index = addElement (w, centre);
        usePicture (index, name);
        centre += { 24.0f, 24.0f };   // several pictures: a little apart
    }

    repaint();
}
