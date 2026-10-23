// D:\workspace\Fanan_web_hub\src\LiveScreen.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "VideoScreen.h"

#include <array>
#include <functional>
#include <memory>
#include <vector>

//==============================================================================
/**
    The Live Show's screen (Live & podcasts, Live streaming): what the playlist's player plays,
    as the audience sees it. A video's picture (VLC follows its deck: VideoScreen), or, for a
    track that's only sound, black with its name in the middle. The player is two decks (the
    second only crossfades the next track in): while the next one comes in, its whole screen
    dissolves in over the last one's, as fast as its sound fades in (its first picture made ready
    while it waited, so it never comes in from black).

    It's shown as the picture in picture (a small window of its own, over every other window:
    Sonify's and the other apps', Sonify minimised too; moved by its bar, sized by its corner) and
    full screen on the computer's other screens (a projector, a TV on HDMI), each one chosen: no
    frame, no pointer, over the windows there. Each of the video's pictures is drawn as it comes,
    at the window's size, scaled its best way. The screen Sonify itself is on is never one of the
    full screen ones.

    Message thread.
*/
namespace LiveScreen
{
    /** The playlist's player: decks 1 and 2. */
    constexpr int numDecks = 2;

    /** A deck, as the screen needs it each moment. */
    struct DeckState
    {
        bool loaded = false;            // a track on it
        double seconds = 0.0;           // where it is in its track (a video's picture: there in it)
        bool playing = false;
        double rate = 1.0;              // its speed (1: as recorded)
        float fade = 1.0f;              // how far it's faded in (0..1): the dissolve follows it
        bool coming = false;            // the next to play, waiting behind the player: its picture made ready
    };

    /** One of the computer's other screens. */
    struct Display
    {
        juce::String id;                // where it is on the desktop: "x y width height"
        juce::String name;              // "Screen 2 (1920 x 1080)"
        juce::Rectangle<int> area;      // on the desktop (logical pixels)
        bool on = false;                // the picture full screen on it
    };

    class Screen final : private juce::Timer
    {
    public:
        /** window: Sonify's own (its screen is never an output). where: deck 0 or 1, now. */
        Screen (juce::Component& window, std::function<DeckState (int deck)> where);
        ~Screen() override;

        /** The track on a deck: a video (prepared: its Info) or none (sound only), and its name
            (shown on black for sound only, or when a video can't be shown). */
        void setTrack (int deck, const juce::File& video, const VideoMedia::Info& info, const juce::String& title);
        void clearDeck (int deck);

        /** The deck in front: the player. The other shows only while it plays (fading out under it). */
        void setFront (int deck);

        /** The playlist's tabs: on. Off (DJ show): no pictures, the other screens let go, the
            picture in picture hidden. */
        void setActive (bool active);
        bool isActive() const noexcept                  { return active; }

        /** The picture in picture: shown while asked for (and the screen is active). Its bar: the
            track's name, HDMI (the other screens' menu), minimise (just its bar: the picture rests),
            maximise (as big as its screen lets the picture be), back to its own size, and close.
            It never takes the keys (Sonify keeps them). */
        void setPipShown (bool shown);
        bool isPipShown() const noexcept                { return pip != nullptr; }

        /** Its own place and size (empty: the top right of Sonify's screen, as it starts), and how
            it shows: "normal", "min" (its bar) or "max". Sonify keeps them. */
        struct PipPlace
        {
            juce::Rectangle<int> area;
            juce::String mode { "normal" };
        };

        const PipPlace& getPipPlace() const noexcept    { return pipPlace; }
        void setPipPlace (const PipPlace& place);

        std::function<void()> onPipClosed;              // its close button
        std::function<void()> onPipMoved;               // moved, sized, minimised, maximised or back
        std::function<void (juce::Rectangle<int>)> onPipOutputs;   // HDMI pressed (its place on the desktop): the screens' menu

        /** The computer's other screens, and the picture full screen on the ones chosen (by id;
            a screen chosen and not there, or Sonify moved onto it, waits until it is). */
        std::vector<Display> getDisplays() const;
        void setOutput (const juce::String& id, bool on);
        const juce::StringArray& getChosenOutputs() const noexcept   { return chosen; }
        int getOutputCount() const noexcept             { return static_cast<int> (outputs.size()); }

        /** The picture as it is now, into an area (black around it): the screens' windows draw
            it (and the tests look at it). */
        void paintPicture (juce::Graphics& g, juce::Rectangle<float> area);

        /** Asks where the decks are now (the timer does it, 60 times a second; the tests call it). */
        void update();

    private:
        struct Layer;
        struct Scene;
        class Output;
        class Pip;

        /** What's on the screen now, to know when to draw it again. */
        struct Look
        {
            bool active = false;
            int front = 0, fade = 0;
            std::array<int, numDecks> frames {};
            std::array<bool, numDecks> loaded {}, playing {}, open {};
            juce::String titles;

            bool operator== (const Look& other) const noexcept
            {
                return active == other.active && front == other.front && fade == other.fade && frames == other.frames
                    && loaded == other.loaded && playing == other.playing && open == other.open && titles == other.titles;
            }

            bool operator!= (const Look& other) const noexcept   { return ! (*this == other); }
        };

        void timerCallback() override                   { update(); }
        VideoScreen::Where pictureWhere (int deck) const;
        Scene sceneNow();
        bool dissolving() const noexcept;
        Look lookNow() const;
        void updateOutputs();
        void updatePip();
        juce::Rectangle<int> ownScreenArea() const;     // the desktop's area of the screen Sonify is on (its taskbar left out)

        juce::Component& ownWindow;
        std::function<DeckState (int)> where;
        std::array<std::unique_ptr<Layer>, numDecks> layers;          // before the windows (they watch the layers' pictures)
        std::vector<std::unique_ptr<Output>> outputs;
        std::unique_ptr<Pip> pip;

        std::array<DeckState, numDecks> states {};
        int front = 0;
        bool active = false;
        bool pipWanted = false;
        PipPlace pipPlace;
        juce::StringArray chosen;
        mutable juce::Rectangle<int> ownArea;   // the screen Sonify's window was last seen on
        Look look;
        int ticks = 0;

        JUCE_DECLARE_NON_COPYABLE (Screen)
    };
}
