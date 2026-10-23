// D:\workspace\Fanan_web_hub\src\PageDropZone.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

//==============================================================================
/**
    Over part of the page while files are dragged in from a folder on the computer (Explorer):
    the page can't learn where such files are, Sonify's own window can. It's laid over the part of
    the page that takes them (the Live Show's playlist, the Sampling Studio's keyboard, the
    Studio's arrangement), shows where they go, and hands them over with where they landed on the
    page; it goes once they're dropped, once the drag leaves it, or once no mouse button is held
    any more (the drag ended elsewhere). Message thread.

    Quiet (no words): only a light tint and its edge, for a page that shows where the files would
    go itself as they move (onMove), as the arrangement does.
*/
class PageDropZone final : public juce::Component,
                           public juce::FileDragAndDropTarget,
                           private juce::Timer
{
public:
    std::function<bool (const juce::File&)> accepts;
    std::function<void (const juce::StringArray& files, juce::Point<int> onPage)> onDrop;
    std::function<void (const juce::StringArray& files, juce::Point<int> onPage)> onMove;   // the files over it, where
    std::function<void()> onHidden;                                                          // gone, dropped or not

    /** What it says over the area ("Drop to add to the playlist"); none: quiet. */
    explicit PageDropZone (const juce::String& words) : label (words)
    {
        setOpaque (false);
        setWantsKeyboardFocus (false);
        setAlwaysOnTop (true);
    }

    /** Over that part of the screen, whose top left corner is there on the page. */
    void showOver (juce::Rectangle<int> screenArea, juce::Point<int> pageOrigin)
    {
        origin = pageOrigin;
        setBounds (screenArea);

        if (! isOnDesktop())
            addToDesktop (juce::ComponentPeer::windowIsTemporary | juce::ComponentPeer::windowIsSemiTransparent
                          | juce::ComponentPeer::windowIgnoresKeyPresses);

        setVisible (true);
        toFront (false);
        shownAt = juce::Time::getMillisecondCounterHiRes();
        startTimerHz (10);
    }

    void hide()
    {
        stopTimer();
        over = false;
        const auto wasVisible = isVisible();
        setVisible (false);

        if (wasVisible && onHidden != nullptr)
            onHidden();
    }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (const auto& path : files)
            if (accepts != nullptr && accepts (juce::File (path)))
                return true;

        return false;
    }

    void fileDragEnter (const juce::StringArray& files, int x, int y) override
    {
        over = true;
        repaint();
        moved (files, x, y);
    }

    void fileDragMove (const juce::StringArray& files, int x, int y) override   { moved (files, x, y); }
    void fileDragExit (const juce::StringArray&) override                       { hide(); }

    void filesDropped (const juce::StringArray& files, int x, int y) override
    {
        hide();

        if (onDrop != nullptr)
            onDrop (files, origin + juce::Point<int> (x, y));
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced (2.0f);
        const juce::Colour green (0xff46c27a);

        // Quiet: the page under it shows through (a tint all over, so it takes the files everywhere).
        if (label.isEmpty())
        {
            g.setColour (green.withAlpha (0.07f));
            g.fillRect (getLocalBounds());
            g.setColour (green.withAlpha (0.85f));
            g.drawRect (getLocalBounds(), 2);
            return;
        }

        g.setColour (green.withAlpha (over ? 0.24f : 0.16f));
        g.fillRoundedRectangle (area, 6.0f);

        juce::Path outline, dashed;
        outline.addRoundedRectangle (area, 6.0f);
        const float dashes[] = { 7.0f, 5.0f };
        juce::PathStrokeType (2.0f).createDashedStroke (dashed, outline, dashes, 2);
        g.setColour (green);
        g.fillPath (dashed);

        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawFittedText (label, getLocalBounds().reduced (12), juce::Justification::centred, 2);
    }

private:
    void moved (const juce::StringArray& files, int x, int y)
    {
        if (onMove != nullptr)
            onMove (files, origin + juce::Point<int> (x, y));
    }

    void timerCallback() override
    {
        // The drag ended somewhere else (dropped there, or Escape): no button held any more.
        if (juce::Time::getMillisecondCounterHiRes() - shownAt > 300.0
            && ! juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown())
            hide();
    }

    const juce::String label;
    juce::Point<int> origin;
    bool over = false;
    double shownAt = 0.0;
};
