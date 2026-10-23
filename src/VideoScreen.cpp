// D:\workspace\Fanan_web_hub\src\VideoScreen.cpp

#include "VideoScreen.h"

#include <algorithm>
#include <cstring>

namespace
{
    /** The computer's biggest screen, its pixels each way (none: not known): a video's pictures
        are made no bigger than any screen could show them. */
    juce::Point<int> biggestScreen()
    {
        juce::Point<int> most;

        for (const auto& display : juce::Desktop::getInstance().getDisplays().displays)
        {
            const auto area = display.logicalBounds;
            const auto width = display.physicalBounds.getWidth() > 0 ? display.physicalBounds.getWidth()
                                                                     : juce::roundToInt (area.getWidth() * display.scale);
            const auto height = display.physicalBounds.getHeight() > 0 ? display.physicalBounds.getHeight()
                                                                       : juce::roundToInt (area.getHeight() * display.scale);
            most = { juce::jmax (most.x, width), juce::jmax (most.y, height) };
        }

        return most;
    }
}

//==============================================================================
VideoScreen::Picture::Picture (std::function<Where()> w) : where (std::move (w))
{
    // Each picture taken the moment VLC has it: the video's own frame rate, evenly.
    player.setOnFrame ([this] { triggerAsyncUpdate(); });
}

VideoScreen::Picture::~Picture()
{
    stopTimer();
    player.close();         // VLC's threads stop telling first
    cancelPendingUpdate();
}

bool VideoScreen::Picture::open (const juce::File& video, const VideoMedia::Info& info)
{
    close();

    if (! player.open (video, info, biggestScreen()))
        return false;

    message.clear();
    startTimerHz (60);
    update();
    return true;
}

void VideoScreen::Picture::close()
{
    stopTimer();
    player.close();
    cancelPendingUpdate();
    frame = juce::Image();
    drawable = juce::Image();
    drawnFrame = -1;
    showing = false;
    ++frameNumber;
    repaintViews();
}

void VideoScreen::Picture::setMessage (const juce::String& text)
{
    if (message != text)
    {
        message = text;
        repaintViews();
    }
}

void VideoScreen::Picture::update()
{
    if (! player.isOpen())
        return;

    // Nobody sees it (its window hidden or closed): VLC rests.
    const auto shown = [] (const juce::Component* c)
    {
        for (; c != nullptr; c = c->getParentComponent())
            if (! c->isVisible())
                return false;

        return true;
    };

    const auto seen = watchedElsewhere
                   || std::any_of (views.begin(), views.end(), [&shown] (const auto& view) { return view != nullptr && shown (view.getComponent()); });
    const auto now = seen && where ? where() : Where {};

    if (! now.shown)
    {
        if (showing)
        {
            showing = false;
            ++frameNumber;
            player.blank();
            repaintViews();
        }

        return;
    }

    if (! showing)
    {
        showing = true;
        ++frameNumber;
        repaintViews();
    }

    player.follow (now.seconds, now.moving, now.rate);
    takeFrame();
}

void VideoScreen::Picture::takeFrame()
{
    // (Not while nothing should show: the picture waiting comes once it should.)
    if (showing && player.takeFrame (frame))
    {
        ++frameNumber;
        repaintViews();
    }
}

const juce::Image& VideoScreen::Picture::getDrawable()
{
    if (! frame.isValid())
        return frame;

    // The graphics card keeps its own copy of an image of its own kind, sent again only when it
    // changes: the picture's copied into it once, and every window draws that.
    if (drawnFrame != frameNumber || ! drawable.isValid())
    {
        drawnFrame = frameNumber;

        if (! drawable.isValid() || drawable.getWidth() != frame.getWidth() || drawable.getHeight() != frame.getHeight())
            drawable = juce::Image (juce::Image::ARGB, frame.getWidth(), frame.getHeight(), false, juce::NativeImageType());

        const juce::Image::BitmapData from (frame, juce::Image::BitmapData::readOnly);
        const juce::Image::BitmapData to (drawable, juce::Image::BitmapData::writeOnly);
        const auto bytes = static_cast<size_t> (juce::jmin (from.width, to.width)) * 4;

        for (int y = 0; y < juce::jmin (from.height, to.height); ++y)
            std::memcpy (to.getLinePointer (y), from.getLinePointer (y), bytes);
    }

    return drawable;
}

void VideoScreen::Picture::addView (juce::Component* view)
{
    views.push_back (view);
}

void VideoScreen::Picture::removeView (juce::Component* view)
{
    views.erase (std::remove_if (views.begin(), views.end(), [view] (const auto& v) { return v == nullptr || v.getComponent() == view; }),
                 views.end());
}

void VideoScreen::Picture::repaintViews()
{
    for (auto& view : views)
        if (view != nullptr)
            view->repaint();
}

//==============================================================================
VideoScreen::View::View (Picture& p) : picture (p)
{
    setOpaque (true);
    setWantsKeyboardFocus (false);
    picture.addView (this);
}

VideoScreen::View::~View()
{
    picture.removeView (this);
}

void VideoScreen::View::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    if (picture.hasPicture())
    {
        // Scaled its best way (smaller: every pixel counted; bigger: smooth).
        const auto& frame = picture.getDrawable();
        const auto area = juce::RectanglePlacement (juce::RectanglePlacement::centred)
                              .appliedTo (frame.getBounds().toFloat(), getLocalBounds().toFloat());
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (frame, area);
        return;
    }

    if (picture.getMessage().isNotEmpty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.setFont (juce::FontOptions (juce::jlimit (13.0f, 22.0f, static_cast<float> (getHeight()) * 0.05f)));
        g.drawFittedText (picture.getMessage(), getLocalBounds().reduced (16), juce::Justification::centred, 3);
    }
}

void VideoScreen::View::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onDoubleClick)
        onDoubleClick();
}

//==============================================================================
VideoScreen::Window::Window (Picture& picture, const juce::String& title)
    : juce::DocumentWindow (title, juce::Colours::black, juce::DocumentWindow::allButtons)
{
    setUsingNativeTitleBar (true);
    auto* content = new View (picture);
    content->onDoubleClick = [this] { setFullScreenHere (! fullScreen); };
    content->addMouseListener (this, true);
    view = content;
    setContentOwned (content, false);
    setResizable (true, false);
    setResizeLimits (160, 90, 10000, 10000);
    centreWithSize (640, 360);
    windowedBounds = getBounds();
    startTimerHz (2);
}

VideoScreen::Window::~Window()
{
    stopTimer();
}

void VideoScreen::Window::closeButtonPressed()
{
    setFullScreenHere (false);
    setVisible (false);

    if (onClose)
        onClose();
}

bool VideoScreen::Window::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && fullScreen)
    {
        setFullScreenHere (false);
        return true;
    }

    if (key.getTextCharacter() == 'f' || key.getTextCharacter() == 'F')
    {
        setFullScreenHere (! fullScreen);
        return true;
    }

    return juce::DocumentWindow::keyPressed (key);
}

void VideoScreen::Window::setFullScreenHere (bool on)
{
    if (on == fullScreen)
        return;

    fullScreen = on;

    if (on)
    {
        // The whole of the screen it's on, over the taskbar, without its frame.
        windowedBounds = getBounds();
        const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds());
        const auto area = display != nullptr ? display->logicalBounds.toNearestInt() : getScreenBounds();
        setUsingNativeTitleBar (false);
        setTitleBarHeight (0);
        setResizable (false, false);
        setBounds (area);
        setAlwaysOnTop (true);
        lastMoved = juce::Time::getMillisecondCounterHiRes();
    }
    else
    {
        setAlwaysOnTop (false);
        setUsingNativeTitleBar (true);
        setResizable (true, false);
        setBounds (windowedBounds);

        if (view != nullptr)
            view->setMouseCursor (juce::MouseCursor::NormalCursor);

        cursorHidden = false;
    }

    toFront (true);
    grabKeyboardFocus();
}

void VideoScreen::Window::mouseMove (const juce::MouseEvent&)
{
    lastMoved = juce::Time::getMillisecondCounterHiRes();

    if (cursorHidden && view != nullptr)
    {
        view->setMouseCursor (juce::MouseCursor::NormalCursor);
        cursorHidden = false;
    }
}

void VideoScreen::Window::timerCallback()
{
    // Full screen, the pointer keeps out of the picture's way.
    if (fullScreen && ! cursorHidden && view != nullptr && juce::Time::getMillisecondCounterHiRes() - lastMoved > 2000.0)
    {
        view->setMouseCursor (juce::MouseCursor::NoCursor);
        cursorHidden = true;
    }
}
