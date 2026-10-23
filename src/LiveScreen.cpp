// D:\workspace\Fanan_web_hub\src\LiveScreen.cpp

#include "LiveScreen.h"

#include <algorithm>
#include <cmath>

namespace LiveScreen
{
namespace
{
    /** A track's name, white on the black, in the middle. */
    void paintTitle (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& title, float alpha)
    {
        if (title.isEmpty() || alpha <= 0.0f || area.isEmpty())
            return;

        const auto size = juce::jlimit (11.0f, 120.0f, area.getHeight() * 0.075f);
        g.setColour (juce::Colours::white.withAlpha (0.92f * alpha));
        g.setFont (juce::FontOptions (size, juce::Font::bold));
        g.drawFittedText (title, area.reduced (area.getWidth() * 0.08f, area.getHeight() * 0.1f).toNearestInt(),
                          juce::Justification::centred, 3, 0.75f);
    }

    /** A picture fitted into the area (its shape kept). */
    void paintFrame (juce::Graphics& g, juce::Rectangle<float> area, const juce::Image& frame, float alpha,
                     juce::Graphics::ResamplingQuality quality)
    {
        if (! frame.isValid() || alpha <= 0.0f || area.isEmpty())
            return;

        const auto fitted = juce::RectanglePlacement (juce::RectanglePlacement::centred).appliedTo (frame.getBounds().toFloat(), area);
        g.setImageResamplingQuality (quality);
        g.setOpacity (alpha);
        g.drawImage (frame, fitted);
        g.setOpacity (1.0f);
    }

    /** The screen an area is most on (none: the nearest to it): its area for windows (its
        taskbar left out). */
    juce::Rectangle<int> userAreaFor (juce::Rectangle<int> area)
    {
        const auto& displays = juce::Desktop::getInstance().getDisplays();
        const juce::Displays::Display* best = nullptr;
        int most = 0;

        for (const auto& display : displays.displays)
        {
            const auto shared = display.logicalBounds.toNearestInt().getIntersection (area);

            if (shared.getWidth() * shared.getHeight() > most)
            {
                most = shared.getWidth() * shared.getHeight();
                best = &display;
            }
        }

        if (best == nullptr)
            best = displays.getDisplayForPoint (area.getCentre().toFloat());

        return best != nullptr ? best->userBounds.toNearestInt() : area;
    }

    juce::String idOf (juce::Rectangle<int> area)
    {
        return juce::String (area.getX()) + " " + juce::String (area.getY()) + " " + juce::String (area.getWidth()) + " " + juce::String (area.getHeight());
    }
}

//==============================================================================
/** A deck as the screen has it: its video's picture (VLC, following the deck), or its name. */
struct Screen::Layer
{
    explicit Layer (std::function<VideoScreen::Where()> where) : picture (std::move (where)) {}

    VideoScreen::Picture picture;
    juce::File video;                   // none: sound only
    bool open = false;                  // VLC has the video, and shows it
    juce::String title;
};

//==============================================================================
/** What the screen shows at a moment, with its own hold on the pictures (a picture held is never
    written again): the screens' windows paint it. */
struct Screen::Scene
{
    struct Deck
    {
        bool loaded = false;            // a track on it
        bool open = false;              // its video shows (black until its first picture)
        juce::Image picture;            // the video's picture now (none: black)
        juce::String title;             // a sound's name, on black
    };

    bool active = false;
    int front = 0;
    bool dissolving = false;
    float fade = 1.0f;                  // how far the front deck has dissolved in, while it does
    std::array<Deck, numDecks> decks;

    void paint (juce::Graphics& g, juce::Rectangle<float> area, juce::Graphics::ResamplingQuality quality) const
    {
        g.setColour (juce::Colours::black);
        g.fillRect (area);

        if (! active)
            return;

        // The next one dissolving in over the last one, its whole screen (black and all), as fast
        // as its sound comes in.
        if (dissolving)
        {
            paintDeck (g, area, 1 - front, 1.0f, false, quality);
            paintDeck (g, area, front, fade, true, quality);
            return;
        }

        paintDeck (g, area, front, 1.0f, false, quality);
    }

    void paintDeck (juce::Graphics& g, juce::Rectangle<float> area, int deck, float alpha, bool backdrop,
                    juce::Graphics::ResamplingQuality quality) const
    {
        const auto& d = decks[static_cast<size_t> (deck)];

        if (! d.loaded || alpha <= 0.0f)
            return;

        if (backdrop)
        {
            g.setColour (juce::Colours::black.withAlpha (alpha));
            g.fillRect (area);
        }

        if (d.open)
        {
            paintFrame (g, area, d.picture, alpha, quality);
            return;
        }

        paintTitle (g, area, d.title, alpha);
    }
};

//==============================================================================
/** The picture full screen on another screen (a projector, a TV): no frame, over the windows
    there, never taking the keys; the pointer shows only while it moves there. It watches both
    decks' pictures (they follow their decks while it shows), and draws what the screen draws. */
class Screen::Output final : public juce::Component,
                             private juce::Timer
{
public:
    Output (Screen& owner, const juce::String& displayId, juce::Rectangle<int> area)
        : screen (owner), id (displayId)
    {
        setOpaque (true);
        setWantsKeyboardFocus (false);
        setMouseCursor (juce::MouseCursor::NoCursor);
        setBounds (area);
        setAlwaysOnTop (true);
        addToDesktop (juce::ComponentPeer::windowIsTemporary | juce::ComponentPeer::windowIgnoresKeyPresses);
        setVisible (true);

        for (auto& layer : screen.layers)
            layer->picture.addView (this);

        startTimerHz (2);
    }

    ~Output() override
    {
        stopTimer();

        for (auto& layer : screen.layers)
            layer->picture.removeView (this);
    }

    void paint (juce::Graphics& g) override
    {
        screen.paintPicture (g, getLocalBounds().toFloat());
    }

    void mouseMove (const juce::MouseEvent&) override
    {
        movedAt = juce::Time::getMillisecondCounterHiRes();

        if (pointerHidden)
        {
            pointerHidden = false;
            setMouseCursor (juce::MouseCursor::NormalCursor);
        }
    }

    Screen& screen;
    const juce::String id;

private:
    void timerCallback() override
    {
        // The pointer keeps out of the picture's way once it rests.
        if (! pointerHidden && juce::Time::getMillisecondCounterHiRes() - movedAt > 2000.0)
        {
            pointerHidden = true;
            setMouseCursor (juce::MouseCursor::NoCursor);
        }
    }

    double movedAt = 0.0;
    bool pointerHidden = true;

    JUCE_DECLARE_NON_COPYABLE (Output)
};

//==============================================================================
/** The picture in picture: a small window of its own over every other window (Sonify's and the
    other apps'), shown while Sonify is minimised too. Its bar: "Screen", the track's name, HDMI,
    and minimise (just the bar: the picture rests), maximise, back to its own size, and close.
    Dragged by its bar or its picture it moves, by its corner it's sized (the picture's shape
    kept); double-click its bar: back to its own size (as it was: back to the top right). A click
    on it never takes the keys from Sonify's window. */
class Screen::Pip final : public juce::Component
{
public:
    static constexpr int barHeight = 28;
    static constexpr int narrowest = 240;           // its width at the least (its bar's buttons and a name)

    explicit Pip (Screen& owner) : screen (owner), picture (owner)
    {
        setOpaque (true);
        setWantsKeyboardFocus (false);
        setMouseClickGrabsKeyboardFocus (false);    // (Windows: clicked, it isn't made the active window)
        addAndMakeVisible (picture);
        setAlwaysOnTop (true);
        setBounds (boundsNow());
        picture.setVisible (screen.pipPlace.mode != "min");
        addToDesktop (juce::ComponentPeer::windowHasDropShadow | juce::ComponentPeer::windowIgnoresKeyPresses);
        setVisible (true);

        for (auto& layer : screen.layers)
            layer->picture.addView (&picture);

        refreshBar();
    }

    ~Pip() override
    {
        for (auto& layer : screen.layers)
            layer->picture.removeView (&picture);
    }

    /** Where it belongs now: its own place and size (or minimised or maximised), on a screen
        that's there. Not while it's being dragged. */
    void place()
    {
        if (moving || sizing)
            return;

        const auto bounds = boundsNow();
        const bool pictureShown = screen.pipPlace.mode != "min";

        if (bounds == getBounds() && picture.isVisible() == pictureShown)
            return;

        setBounds (bounds);
        picture.setVisible (pictureShown);
        repaint();
    }

    void pictureChanged()                           { picture.repaint(); }

    /** The track's name or HDMI changed: its bar drawn again. */
    void refreshBar()
    {
        const auto title = screen.layers[static_cast<size_t> (screen.front)]->title;
        const auto outputs = screen.getOutputCount();

        if (title != shownTitle || outputs != shownOutputs)
        {
            shownTitle = title;
            shownOutputs = outputs;
            repaint (getLocalBounds().removeFromTop (barHeight));
        }
    }

    static int heightFor (int width)                { return barHeight + juce::roundToInt (width * 9.0 / 16.0); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black);
        auto bar = getLocalBounds().removeFromTop (barHeight);
        g.setColour (juce::Colour (0xff1b1d21));
        g.fillRect (bar);
        g.setColour (juce::Colour (0xff2c2e34));
        g.fillRect (bar.removeFromBottom (1));

        // "Screen", and the track's name.
        auto words = getLocalBounds().removeFromTop (barHeight).withTrimmedLeft (10).withRight (areaOf (Part::hdmi).getX() - 6);
        const juce::Font label (juce::FontOptions (12.5f, juce::Font::bold));
        g.setFont (label);
        g.setColour (juce::Colour (0xffeef0f3));
        const auto labelWidth = juce::GlyphArrangement::getStringWidthInt (label, "Screen") + 8;
        g.drawText ("Screen", words.removeFromLeft (labelWidth), juce::Justification::centredLeft, false);
        g.setFont (juce::FontOptions (12.0f));
        g.setColour (juce::Colour (0xff9aa0a8));
        g.drawText (shownTitle, words, juce::Justification::centredLeft, true);

        // HDMI: lit while the picture is full screen on another screen.
        const auto hdmi = areaOf (Part::hdmi).toFloat();
        const bool on = shownOutputs > 0;
        g.setColour (on ? juce::Colour (0xff1f6b3e) : juce::Colour (0xff26282d));
        g.fillRoundedRectangle (hdmi, 4.0f);
        g.setColour (on ? juce::Colour (0xff46c27a) : hover == Part::hdmi ? juce::Colour (0xff6d7179) : juce::Colour (0xff3a3d44));
        g.drawRoundedRectangle (hdmi.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (on ? juce::Colours::white : juce::Colour (0xffc9ccd1));
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText ("HDMI", hdmi, juce::Justification::centred, false);

        for (const auto part : { Part::minimise, Part::maximise, Part::restore, Part::close })
            paintButton (g, part);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        // Its edge, over any picture.
        g.setColour (juce::Colour (0xff3a3d44));
        g.drawRect (getLocalBounds(), 1);

        // The corner it's sized by (dark under light: seen on any picture).
        if (screen.pipPlace.mode == "normal")
        {
            const auto grip = gripArea().toFloat();
            const auto light = juce::Colours::white.withAlpha (hover == Part::grip || sizing ? 0.95f : 0.7f);

            for (int i = 1; i <= 3; ++i)
            {
                const auto d = 3.5f * static_cast<float> (i) + 1.0f;
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.drawLine (grip.getRight() - d + 1.0f, grip.getBottom() - 1.0f, grip.getRight() - 1.0f, grip.getBottom() - d + 1.0f, 1.6f);
                g.setColour (light);
                g.drawLine (grip.getRight() - d, grip.getBottom() - 2.0f, grip.getRight() - 2.0f, grip.getBottom() - d, 1.2f);
            }
        }
    }

    void resized() override
    {
        picture.setBounds (getLocalBounds().withTrimmedTop (barHeight).reduced (1, 0).withTrimmedBottom (1));
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto part = partAt (event.getPosition());
        setMouseCursor (part == Part::grip ? juce::MouseCursor::BottomRightCornerResizeCursor : juce::MouseCursor::NormalCursor);
        setHover (part);
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        setHover (Part::none);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        const auto part = partAt (event.getPosition());
        startBounds = getBounds();
        startMouse = event.getScreenPosition();
        moving = sizing = false;
        pressed = Part::none;

        if (part == Part::grip)
            sizing = true;
        else if (part == Part::bar || part == Part::picture)
            moving = screen.pipPlace.mode != "max";     // maximised, it stays put
        else if (enabled (part))
            pressed = part;                             // a button: it acts when let go over it

        repaint();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        const auto by = event.getScreenPosition() - startMouse;

        if (moving)
        {
            setTopLeftPosition (startBounds.getTopLeft() + by);
            return;
        }

        if (sizing)
        {
            // Along whichever way it's dragged more (the picture's shape kept), no bigger than its screen.
            const auto byWidth = std::abs (by.x) * 9 >= std::abs (by.y) * 16 ? by.x : juce::roundToInt (by.y * 16.0 / 9.0);
            const auto room = roomFor (startBounds);
            const auto widest = juce::jmax (narrowest, juce::jmin (room.getRight() - startBounds.getX(),
                                                                  juce::roundToInt ((room.getBottom() - startBounds.getY() - barHeight) * 16.0 / 9.0)));
            const auto width = juce::jlimit (narrowest, widest, startBounds.getWidth() + byWidth);
            setSize (width, heightFor (width));
            return;
        }

        if (pressed != Part::none)
            repaint (getLocalBounds().removeFromTop (barHeight));
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (moving || sizing)
        {
            const bool changed = getBounds() != startBounds;
            moving = sizing = false;

            if (changed)
                keep();
            else
                repaint();

            return;
        }

        const auto part = pressed;
        pressed = Part::none;
        repaint (getLocalBounds().removeFromTop (barHeight));

        if (part == Part::none || partAt (event.getPosition()) != part)
            return;

        if (part == Part::close)
        {
            // Hidden now; closed once this click is done with (closing it ends this window).
            setVisible (false);
            juce::MessageManager::callAsync ([self = juce::Component::SafePointer<Pip> (this)]
            {
                if (self != nullptr && self->screen.onPipClosed)
                    self->screen.onPipClosed();
            });

            return;
        }

        if (part == Part::hdmi)
        {
            // Its menu stays open only while Sonify is the app in front: this window comes in front
            // for it (a click on it alone never makes it so), Sonify's own window left as it is.
            toFront (true);

            if (screen.onPipOutputs)
                screen.onPipOutputs (areaOf (Part::hdmi) + getScreenPosition());

            return;
        }

        setMode (part == Part::minimise ? "min" : part == Part::maximise ? "max" : "normal");
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (partAt (event.getPosition()) != Part::bar)
            return;

        // Minimised or maximised: back to its own size. As it is: back to the top right.
        if (screen.pipPlace.mode != "normal")
        {
            setMode ("normal");
            return;
        }

        screen.pipPlace.area = defaultArea();
        place();

        if (screen.onPipMoved)
            screen.onPipMoved();
    }

private:
    enum class Part { none, bar, picture, grip, hdmi, minimise, maximise, restore, close };

    /** The picture under the bar: a View of both decks' pictures (they follow their decks while
        it shows). Clicks go through it to the window (dragged by its picture too). */
    struct Picture final : public juce::Component
    {
        explicit Picture (Screen& owner) : screen (owner)
        {
            setOpaque (true);
            setInterceptsMouseClicks (false, false);
        }

        void paint (juce::Graphics& g) override     { screen.paintPicture (g, getLocalBounds().toFloat()); }

        Screen& screen;
    };

    /** Its own size at first: at the top right of the screen Sonify is on. */
    juce::Rectangle<int> defaultArea() const
    {
        const auto room = screen.ownScreenArea();
        const auto width = juce::jlimit (juce::jmin (narrowest, room.getWidth()), juce::jmax (narrowest, room.getWidth()), 400);
        return { room.getRight() - width - 24, room.getY() + 110, width, heightFor (width) };
    }

    /** Where it should be: its own place and size (empty: the top right), its bar alone
        (minimised), or as big as its screen lets the picture be (maximised), on a screen that's
        there. */
    juce::Rectangle<int> boundsNow() const
    {
        const auto& kept = screen.pipPlace;
        const auto own = kept.area.isEmpty() ? defaultArea() : kept.area;
        const auto room = roomFor (kept.mode == "min" ? own.withHeight (barHeight) : own);

        if (kept.mode == "max")
        {
            const auto inside = room.reduced (8);
            const auto width = juce::jmax (narrowest, juce::jmin (inside.getWidth(), juce::roundToInt ((inside.getHeight() - barHeight) * 16.0 / 9.0)));
            return juce::Rectangle<int> (width, heightFor (width)).withCentre (inside.getCentre());
        }

        const auto widest = juce::jmax (narrowest, juce::jmin (room.getWidth(), juce::roundToInt ((room.getHeight() - barHeight) * 16.0 / 9.0)));
        const auto width = juce::jlimit (juce::jmin (narrowest, room.getWidth()), widest, own.getWidth());
        const auto height = kept.mode == "min" ? barHeight : heightFor (width);
        return juce::Rectangle<int> (own.getX(), own.getY(), width, height).constrainedWithin (room);
    }

    /** The screen it goes on (the one most of it is on): never one showing the picture full
        screen (the audience would see it there), Sonify's own instead. */
    juce::Rectangle<int> roomFor (juce::Rectangle<int> area) const
    {
        const auto room = userAreaFor (area);

        for (const auto& output : screen.outputs)
            if (output->getBounds().intersects (room))
                return screen.ownScreenArea();

        return room;
    }

    /** Moved or sized by hand: its own place (and size) from now on, kept. */
    void keep()
    {
        auto& kept = screen.pipPlace;
        const auto now = getBounds();

        // (Kept as it ended up: on its screen, no bigger than it.)
        if (kept.mode == "min")
        {
            kept.area = juce::Rectangle<int> (now.getX(), now.getY(), now.getWidth(), heightFor (now.getWidth()));
            place();
            kept.area.setPosition (getPosition());
        }
        else
        {
            kept.area = now;
            place();
            kept.area = getBounds();
        }

        if (screen.onPipMoved)
            screen.onPipMoved();
    }

    /** Minimised, maximised, or back to its own place and size (kept apart from the other two). */
    void setMode (const juce::String& mode)
    {
        auto& kept = screen.pipPlace;

        if (kept.area.isEmpty())
            kept.area = defaultArea();

        kept.mode = mode;
        place();
        repaint();      // its buttons: the one for the way it is now does nothing

        if (screen.onPipMoved)
            screen.onPipMoved();
    }

    /** The button for the way it already is does nothing. */
    bool enabled (Part part) const
    {
        const auto& mode = screen.pipPlace.mode;
        return ! ((part == Part::minimise && mode == "min") || (part == Part::maximise && mode == "max")
                  || (part == Part::restore && mode == "normal"));
    }

    juce::Rectangle<int> areaOf (Part part) const
    {
        auto bar = getLocalBounds().removeFromTop (barHeight).reduced (4, 3);
        const auto close = bar.removeFromRight (24);
        const auto restore = bar.removeFromRight (24);
        const auto maximise = bar.removeFromRight (24);
        const auto minimise = bar.removeFromRight (24);
        bar.removeFromRight (6);
        const auto hdmi = bar.removeFromRight (44).reduced (0, 1);

        switch (part)
        {
            case Part::close:       return close;
            case Part::restore:     return restore;
            case Part::maximise:    return maximise;
            case Part::minimise:    return minimise;
            case Part::hdmi:        return hdmi;
            case Part::none:
            case Part::bar:
            case Part::picture:
            case Part::grip:        break;
        }

        return {};
    }

    juce::Rectangle<int> gripArea() const           { return getLocalBounds().removeFromBottom (16).removeFromRight (16); }

    Part partAt (juce::Point<int> point) const
    {
        if (! getLocalBounds().contains (point))
            return Part::none;

        for (const auto part : { Part::close, Part::restore, Part::maximise, Part::minimise, Part::hdmi })
            if (areaOf (part).contains (point))
                return part;

        if (screen.pipPlace.mode == "normal" && gripArea().contains (point))
            return Part::grip;

        return point.y < barHeight ? Part::bar : Part::picture;
    }

    void setHover (Part part)
    {
        if (part == hover)
            return;

        hover = part;
        repaint (getLocalBounds().removeFromTop (barHeight));
        repaint (gripArea());
    }

    void paintButton (juce::Graphics& g, Part part) const
    {
        const auto area = areaOf (part).toFloat();
        const bool usable = enabled (part);

        if (usable && (hover == part || pressed == part))
        {
            g.setColour (part == Part::close ? juce::Colour (0xffc42b37) : juce::Colour (pressed == part ? 0xff3a3d44 : 0xff2e3036));
            g.fillRoundedRectangle (area, 4.0f);
        }

        g.setColour (juce::Colour (0xffd5d8dc).withAlpha (usable ? 1.0f : 0.3f));
        const auto c = area.getCentre();

        switch (part)
        {
            case Part::minimise:
                g.drawLine (c.x - 4.5f, c.y + 3.5f, c.x + 4.5f, c.y + 3.5f, 1.6f);
                break;

            case Part::maximise:
                g.drawRoundedRectangle ({ c.x - 4.5f, c.y - 4.5f, 9.0f, 9.0f }, 1.0f, 1.4f);
                break;

            case Part::restore:
            {
                g.drawRoundedRectangle ({ c.x - 5.0f, c.y - 2.0f, 7.0f, 7.0f }, 0.8f, 1.3f);
                juce::Path behind;
                behind.startNewSubPath (c.x - 2.0f, c.y - 2.0f);
                behind.lineTo (c.x - 2.0f, c.y - 5.0f);
                behind.lineTo (c.x + 5.0f, c.y - 5.0f);
                behind.lineTo (c.x + 5.0f, c.y + 2.0f);
                behind.lineTo (c.x + 2.0f, c.y + 2.0f);
                g.strokePath (behind, juce::PathStrokeType (1.3f));
                break;
            }

            case Part::close:
                g.drawLine (c.x - 4.0f, c.y - 4.0f, c.x + 4.0f, c.y + 4.0f, 1.6f);
                g.drawLine (c.x + 4.0f, c.y - 4.0f, c.x - 4.0f, c.y + 4.0f, 1.6f);
                break;

            case Part::none:
            case Part::bar:
            case Part::picture:
            case Part::grip:
            case Part::hdmi:
                break;
        }
    }

    Screen& screen;
    Picture picture;
    Part hover = Part::none, pressed = Part::none;
    bool moving = false, sizing = false;
    juce::Rectangle<int> startBounds;
    juce::Point<int> startMouse;
    juce::String shownTitle;
    int shownOutputs = -1;

    JUCE_DECLARE_NON_COPYABLE (Pip)
};

//==============================================================================
Screen::Screen (juce::Component& window, std::function<DeckState (int)> w)
    : ownWindow (window), where (std::move (w))
{
    for (int d = 0; d < numDecks; ++d)
        layers[static_cast<size_t> (d)] = std::make_unique<Layer> ([this, d] { return pictureWhere (d); });

    startTimerHz (60);
}

Screen::~Screen()
{
    stopTimer();
    pip = nullptr;          // the windows watch the pictures: they go first
    outputs.clear();

    for (auto& layer : layers)
        layer->picture.close();
}

//==============================================================================
void Screen::setTrack (int deck, const juce::File& video, const VideoMedia::Info& info, const juce::String& title)
{
    if (! juce::isPositiveAndBelow (deck, numDecks))
        return;

    auto& layer = *layers[static_cast<size_t> (deck)];
    layer.title = title;

    if (video == juce::File())
    {
        if (layer.open || layer.video != juce::File())
            layer.picture.close();

        layer.video = juce::File();
        layer.open = false;
    }
    else if (layer.video != video || ! layer.open)
    {
        // A video VLC can't show: its name, as for a sound.
        layer.video = video;
        layer.open = layer.picture.open (video, info);
    }
}

void Screen::clearDeck (int deck)
{
    if (! juce::isPositiveAndBelow (deck, numDecks))
        return;

    auto& layer = *layers[static_cast<size_t> (deck)];

    if (layer.open || layer.video != juce::File())
        layer.picture.close();

    layer.video = juce::File();
    layer.open = false;
    layer.title.clear();
}

void Screen::setFront (int deck)
{
    if (! juce::isPositiveAndBelow (deck, numDecks) || deck == front)
        return;

    front = deck;
}

void Screen::setActive (bool shouldBeActive)
{
    if (shouldBeActive == active)
        return;

    active = shouldBeActive;
    updateOutputs();
    updatePip();
}

void Screen::setPipShown (bool shown)
{
    pipWanted = shown;
    updatePip();
}

void Screen::setPipPlace (const PipPlace& place)
{
    pipPlace = place;

    if (pipPlace.mode != "min" && pipPlace.mode != "max")
        pipPlace.mode = "normal";

    if (pip != nullptr)
        pip->place();
}

void Screen::updatePip()
{
    const bool show = pipWanted && active;

    if (show && pip == nullptr)
        pip = std::make_unique<Pip> (*this);
    else if (! show)
        pip = nullptr;
}

juce::Rectangle<int> Screen::ownScreenArea() const
{
    const auto& displays = juce::Desktop::getInstance().getDisplays();
    const auto* display = ownWindow.getPeer() != nullptr ? displays.getDisplayForRect (ownWindow.getScreenBounds()) : nullptr;

    if (display == nullptr)
        display = displays.getPrimaryDisplay();

    return display != nullptr ? display->userBounds.toNearestInt() : juce::Rectangle<int> (0, 0, 1280, 720);
}

//==============================================================================
std::vector<Display> Screen::getDisplays() const
{
    std::vector<Display> list;
    const auto& displays = juce::Desktop::getInstance().getDisplays();

    // Sonify's own screen is never one: the picture would cover it. Where its window is while it
    // shows (minimised it's nowhere: the screen it was last on).
    const auto* peer = ownWindow.getPeer();

    if (peer != nullptr && ! peer->isMinimised() && ownWindow.isShowing())
    {
        const auto bounds = ownWindow.getScreenBounds();
        int most = 0;

        for (const auto& display : displays.displays)
        {
            const auto area = display.logicalBounds.toNearestInt();
            const auto shared = area.getIntersection (bounds);

            if (shared.getWidth() * shared.getHeight() > most)
            {
                most = shared.getWidth() * shared.getHeight();
                ownArea = area;
            }
        }
    }

    int number = 0;

    for (const auto& display : displays.displays)
    {
        ++number;
        const auto area = display.logicalBounds.toNearestInt();

        if (! ownArea.isEmpty() ? area == ownArea : display.isMain)
            continue;

        const auto id = idOf (area);
        const auto width = display.physicalBounds.getWidth() > 0 ? display.physicalBounds.getWidth() : juce::roundToInt (area.getWidth() * display.scale);
        const auto height = display.physicalBounds.getHeight() > 0 ? display.physicalBounds.getHeight() : juce::roundToInt (area.getHeight() * display.scale);
        list.push_back ({ id, "Screen " + juce::String (number) + " (" + juce::String (width) + " x " + juce::String (height) + ")",
                          area, chosen.contains (id) });
    }

    return list;
}

void Screen::setOutput (const juce::String& id, bool on)
{
    if (on)
        chosen.addIfNotAlreadyThere (id);
    else
        chosen.removeString (id);

    updateOutputs();
}

void Screen::updateOutputs()
{
    const auto displays = getDisplays();
    const auto there = [&displays] (const juce::String& id)
    {
        return std::any_of (displays.begin(), displays.end(), [&id] (const Display& display) { return display.id == id; });
    };

    // Gone: not chosen any more, its screen unplugged (or Sonify moved onto it), or the screen not
    // in use (DJ show).
    for (auto it = outputs.begin(); it != outputs.end();)
    {
        if (active && chosen.contains ((*it)->id) && there ((*it)->id))
            ++it;
        else
            it = outputs.erase (it);
    }

    if (! active)
        return;

    for (const auto& display : displays)
    {
        if (! display.on)
            continue;

        const auto shown = std::any_of (outputs.begin(), outputs.end(), [&display] (const auto& output) { return output->id == display.id; });

        if (! shown)
            outputs.push_back (std::make_unique<Output> (*this, display.id, display.area));
    }

    // The picture in picture keeps off a screen showing the picture full screen.
    if (pip != nullptr)
        pip->place();
}

//==============================================================================
VideoScreen::Where Screen::pictureWhere (int deck) const
{
    const auto& layer = *layers[static_cast<size_t> (deck)];

    if (! active || ! layer.open)
        return {};

    const auto state = where (deck);

    if (! state.loaded)
        return {};

    // The player's picture, and the last one's while the next dissolves in over it. The next one,
    // waiting behind: its picture where it starts made ready (it isn't shown), so it dissolves in
    // from its own first picture, not from black while VLC gets going. Any other rests.
    if (deck != front && ! state.playing)
        return state.coming ? VideoScreen::Where { true, state.seconds, false, state.rate } : VideoScreen::Where {};

    return { true, state.seconds, state.playing, state.rate };
}

bool Screen::dissolving() const noexcept
{
    const auto& inFront = states[static_cast<size_t> (front)];
    const auto& behind = states[static_cast<size_t> (1 - front)];
    return inFront.loaded && behind.loaded && behind.playing && inFront.fade < 0.999f;
}

void Screen::paintPicture (juce::Graphics& g, juce::Rectangle<float> area)
{
    // Scaled its best way: a 4K video on an HD screen has every pixel counted, a small one
    // on a big screen comes out smooth.
    sceneNow().paint (g, area, juce::Graphics::highResamplingQuality);
}

Screen::Scene Screen::sceneNow()
{
    Scene scene;
    scene.active = active;
    scene.front = front;
    scene.dissolving = dissolving();
    scene.fade = juce::jlimit (0.0f, 1.0f, states[static_cast<size_t> (front)].fade);

    for (int d = 0; d < numDecks; ++d)
    {
        const auto place = static_cast<size_t> (d);
        auto& layer = *layers[place];
        auto& deck = scene.decks[place];
        deck.loaded = states[place].loaded;
        deck.open = layer.open;
        deck.title = layer.title;

        // (Black until VLC's first picture; only the decks painted.)
        const auto painted = d == front || scene.dissolving;

        if (painted && layer.open && layer.picture.hasPicture())
            deck.picture = layer.picture.getDrawable();
    }

    return scene;
}

Screen::Look Screen::lookNow() const
{
    Look now;
    now.active = active;
    now.front = front;
    now.fade = dissolving() ? juce::roundToInt (states[static_cast<size_t> (front)].fade * 64.0f) : 64;

    for (int d = 0; d < numDecks; ++d)
    {
        const auto place = static_cast<size_t> (d);
        const auto& layer = *layers[place];
        now.frames[place] = layer.open ? layer.picture.getFrameNumber() : 0;
        now.loaded[place] = states[place].loaded;
        now.playing[place] = states[place].playing;
        now.open[place] = layer.open;
        now.titles += layer.title + "|";
    }

    return now;
}

//==============================================================================
void Screen::update()
{
    if (! active)
        return;

    for (int d = 0; d < numDecks; ++d)
        states[static_cast<size_t> (d)] = where (d);

    const auto now = lookNow();

    if (now != look)
    {
        look = now;

        for (auto& output : outputs)
            output->repaint();

        if (pip != nullptr)
            pip->pictureChanged();
    }

    if (pip != nullptr)
        pip->refreshBar();

    // Every two seconds: the screens as they are (one plugged in or out, Sonify moved onto one),
    // and the picture in picture kept on one that's there (updateOutputs places it).
    if (++ticks % 120 == 0)
        updateOutputs();
}
}
