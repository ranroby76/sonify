// D:\workspace\Fanan_web_hub\src\VideoScreen.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "VideoMedia.h"

#include <functional>
#include <memory>
#include <vector>

//==============================================================================
/**
    Where video is shown: the song's Video window, the Playlist Player's screen and its own
    Video window, and the Live Show's screen (LiveScreen.h). A Picture is a video player
    following where it's told the picture should be (about 60 times a second) while one of its
    Views shows, or something else watches it (setWatched); each of the video's pictures is taken
    the moment VLC has it (the video's own size, up to the computer's biggest screen, and its own
    frame rate), and its Views draw it, fitted (black around it). A Window is a View of its own on the desktop: drag it to another screen,
    double-click (or F) for full screen there, Escape to come back.
*/
namespace VideoScreen
{
    /** Where the picture should be, asked for each moment. */
    struct Where
    {
        bool shown = false;             // false: no picture (black)
        double seconds = 0.0;           // into the video
        bool moving = false;
        double rate = 1.0;
    };

    class Picture final : private juce::Timer,
                          private juce::AsyncUpdater
    {
    public:
        explicit Picture (std::function<Where()> where);
        ~Picture() override;

        /** A video to show (prepared: its Info). False: VLC can't (see VideoMedia::problem()). */
        bool open (const juce::File& video, const VideoMedia::Info& info);
        void close();
        const juce::File& getFile() const noexcept       { return player.getFile(); }
        bool isOpen() const noexcept                     { return player.isOpen(); }

        /** Said where there's no picture ("Preparing the video…", "...isn't there"). */
        void setMessage (const juce::String& text);
        const juce::String& getMessage() const noexcept  { return message; }

        /** The picture now (none: black), and whether there is one to show. It's never written
            again once it's here: a copy of it can be kept and drawn on another thread. */
        const juce::Image& getFrame() const noexcept     { return frame; }
        bool hasPicture() const noexcept                 { return showing && frame.isValid(); }

        /** The picture now as a window draws it best (the graphics card's own kind of image, made
            once for each picture; none: black). Message thread. */
        const juce::Image& getDrawable();

        /** Goes up each time what it shows changes (a new picture, or shown or not): whoever draws
            it elsewhere knows when to draw again. */
        int getFrameNumber() const noexcept              { return frameNumber; }

        /** Something other than its Views looks at it (the Live Show's screen on the page): it
            follows where it's told while this is on, as while a View shows. */
        void setWatched (bool watched) noexcept          { watchedElsewhere = watched; }

        /** Asks where the picture should be now (the timer does it; the tests call it). */
        void update();

        void addView (juce::Component* view);
        void removeView (juce::Component* view);

    private:
        void timerCallback() override                    { update(); }
        void handleAsyncUpdate() override                { takeFrame(); }
        void takeFrame();
        void repaintViews();

        std::function<Where()> where;
        VideoMedia::Player player;
        juce::Image frame;
        juce::Image drawable;                            // frame, as a window draws it
        int drawnFrame = -1;                             // the frame number drawable has
        juce::String message;
        bool showing = false;
        bool watchedElsewhere = false;
        int frameNumber = 0;
        std::vector<juce::Component::SafePointer<juce::Component>> views;

        JUCE_DECLARE_NON_COPYABLE (Picture)
    };

    /** A picture fitted into the component, black around it; its message when there's none. */
    class View final : public juce::Component
    {
    public:
        explicit View (Picture& picture);
        ~View() override;

        void paint (juce::Graphics& g) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;

        /** A double-click on it: its window goes full screen and back (none: nothing). */
        std::function<void()> onDoubleClick;

    private:
        Picture& picture;
    };

    /** A View in a window of its own. Closing it hides it. */
    class Window final : public juce::DocumentWindow,
                         private juce::Timer
    {
    public:
        Window (Picture& picture, const juce::String& title);
        ~Window() override;

        void closeButtonPressed() override;
        bool keyPressed (const juce::KeyPress& key) override;
        void mouseMove (const juce::MouseEvent&) override;

        void setFullScreenHere (bool on);
        bool isFullScreenHere() const noexcept           { return fullScreen; }

        /** It closed (hidden): whoever shows it can say so. */
        std::function<void()> onClose;

    private:
        void timerCallback() override;

        View* view = nullptr;
        bool fullScreen = false;
        juce::Rectangle<int> windowedBounds;
        double lastMoved = 0.0;
        bool cursorHidden = false;
    };
}
