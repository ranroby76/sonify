// D:\workspace\Fanan_web_hub\src\PlaylistScreen.cpp

#include "PlaylistScreen.h"
#include "VideoScreen.h"

#include <cmath>

namespace
{
    namespace Look
    {
        const juce::Colour gold (0xffd4af37);
        const juce::Colour page (0xff000000);
        const juce::Colour panel (0xff222222);
        const juce::Colour banner (0xff1a1a1a);
        const juce::Colour bannerPlaying (0xff152215);
        const juce::Colour playing (0xff00ff00);
        const juce::Colour outline (0xff404040);
        const juce::Colour missing (0xffe05050);
        const juce::Colour quiet (0xff9a9a9a);

        juce::Font bold (float size)    { return juce::Font (juce::FontOptions (size, juce::Font::bold)); }
        juce::Font plain (float size)   { return juce::Font (juce::FontOptions (size)); }

        constexpr int bannerHeight = 44;
        constexpr int rowHeight = 30;
        constexpr int rows = 5;          // Volume, Pitch, Speed, Wait, Fade
    }

    juce::String formatTime (double seconds)
    {
        const auto total = static_cast<int> (juce::jmax (0.0, seconds));
        return juce::String::formatted ("%02d:%02d", total / 60, total % 60);
    }

    //==============================================================================
    /** OnStage's glassy PLAY (green) and STOP (red) buttons. */
    class GlassyLook final : public juce::LookAndFeel_V4
    {
    public:
        explicit GlassyLook (bool isGreen) : green (isGreen) {}

        void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                   bool isMouseOverButton, bool isButtonDown) override
        {
            auto bounds = button.getLocalBounds().toFloat();
            auto base = green ? juce::Colour (0xff228b22) : juce::Colour (0xffb22222);
            auto light = green ? juce::Colour (0xff32cd32) : juce::Colour (0xffdc143c);
            const auto dark = green ? juce::Colour (0xff006400) : juce::Colour (0xff8b0000);

            if (isButtonDown)
            {
                light = light.brighter (0.2f);
                base = base.brighter (0.1f);
            }
            else if (isMouseOverButton)
            {
                light = light.brighter (0.1f);
            }

            juce::ColourGradient main (light, bounds.getX(), bounds.getY(), dark, bounds.getX(), bounds.getBottom(), false);
            main.addColour (0.5, base);
            g.setGradientFill (main);
            g.fillRoundedRectangle (bounds, 6.0f);

            // The glass: a highlight over the top half.
            const auto highlight = bounds.withHeight (bounds.getHeight() * 0.45f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.4f), highlight.getX(), highlight.getY(),
                                                     juce::Colours::white.withAlpha (0.05f), highlight.getX(), highlight.getBottom(), false));
            g.fillRoundedRectangle (highlight.reduced (1.0f), 5.0f);

            g.setColour (dark.darker (0.3f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.5f);
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool) override
        {
            const auto bounds = button.getLocalBounds();
            g.setFont (Look::bold (14.0f));
            g.setColour (juce::Colours::black.withAlpha (0.3f));
            g.drawText (button.getButtonText(), bounds.translated (1, 1), juce::Justification::centred);
            g.setColour (juce::Colours::black);
            g.drawText (button.getButtonText(), bounds, juce::Justification::centred);
        }

    private:
        const bool green;
    };

    //==============================================================================
    /** OnStage's gold: sliders, scrollbars, menus. */
    class GoldenLook final : public juce::LookAndFeel_V4
    {
    public:
        GoldenLook()
        {
            setColour (juce::Slider::thumbColourId, Look::gold);
            setColour (juce::Slider::trackColourId, juce::Colour (0xff202020));
            setColour (juce::Slider::backgroundColourId, juce::Colour (0xff202020));
            setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
            setColour (juce::Slider::textBoxOutlineColourId, Look::outline);
            setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff151515));
            setColour (juce::ScrollBar::thumbColourId, Look::gold);
            setColour (juce::ScrollBar::trackColourId, juce::Colour (0xff1a1a1a));
            setColour (juce::ScrollBar::backgroundColourId, juce::Colour (0xff1a1a1a));
            setColour (juce::PopupMenu::backgroundColourId, Look::gold);
            setColour (juce::PopupMenu::textColourId, juce::Colours::black);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colours::black);
            setColour (juce::PopupMenu::highlightedTextColourId, Look::gold);
            setColour (juce::ToggleButton::textColourId, juce::Colours::white);
            setColour (juce::ToggleButton::tickColourId, Look::gold);
            setColour (juce::ToggleButton::tickDisabledColourId, Look::outline);
            setColour (juce::TextButton::buttonColourId, juce::Colour (0xff333333));
            setColour (juce::TextButton::textColourOffId, juce::Colours::white);
            setColour (juce::Label::textColourId, juce::Colours::white);
            setColour (juce::AlertWindow::backgroundColourId, Look::panel);
        }

        void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float, float,
                               const juce::Slider::SliderStyle style, juce::Slider&) override
        {
            const auto vertical = style == juce::Slider::LinearVertical;
            const auto trackWidth = juce::jlimit (4.0f, 8.0f, vertical ? static_cast<float> (width) * 0.2f : static_cast<float> (height) * 0.25f);
            juce::Rectangle<float> track;

            if (vertical)
                track = { static_cast<float> (x) + static_cast<float> (width) * 0.5f - trackWidth * 0.5f, static_cast<float> (y) + 5.0f,
                          trackWidth, static_cast<float> (height) - 10.0f };
            else
                track = { static_cast<float> (x) + 5.0f, static_cast<float> (y) + static_cast<float> (height) * 0.5f - trackWidth * 0.5f,
                          static_cast<float> (width) - 10.0f, trackWidth };

            g.setColour (juce::Colour (0xff151515));
            g.fillRoundedRectangle (track, trackWidth * 0.5f);
            g.setColour (juce::Colour (0xff333333));
            g.drawRoundedRectangle (track, trackWidth * 0.5f, 1.0f);

            auto fill = track;

            if (vertical)
                fill.setTop (sliderPos);
            else
                fill.setRight (sliderPos);

            if (! fill.isEmpty())
            {
                g.setColour (Look::gold);
                g.fillRoundedRectangle (fill, trackWidth * 0.5f);
            }

            const auto thumbSize = juce::jlimit (14.0f, 20.0f, vertical ? static_cast<float> (width) * 0.7f : static_cast<float> (height) * 0.7f);
            const auto thumbX = vertical ? static_cast<float> (x) + static_cast<float> (width) * 0.5f - thumbSize * 0.5f : sliderPos - thumbSize * 0.5f;
            const auto thumbY = vertical ? sliderPos - thumbSize * 0.5f : static_cast<float> (y) + static_cast<float> (height) * 0.5f - thumbSize * 0.5f;
            const juce::Rectangle<float> thumb (thumbX, thumbY, thumbSize, thumbSize);
            g.setColour (Look::gold);
            g.fillEllipse (thumb);
            g.setColour (juce::Colours::black);
            g.fillEllipse (thumb.reduced (3.0f));
        }

        void drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height, bool vertical,
                            int thumbStart, int thumbSize, bool, bool) override
        {
            g.fillAll (findColour (juce::ScrollBar::backgroundColourId));
            const auto thumb = vertical ? juce::Rectangle<int> (x + 2, thumbStart, width - 4, thumbSize)
                                        : juce::Rectangle<int> (thumbStart, y + 2, thumbSize, height - 4);
            g.setColour (findColour (juce::ScrollBar::thumbColourId));
            g.fillRoundedRectangle (thumb.toFloat(), 4.0f);
        }
    };

    //==============================================================================
    /** OnStage's note on a control (a right-click): what it does, in a bubble that goes by itself. */
    class TipBubble final : public juce::BubbleComponent,
                            private juce::Timer
    {
    public:
        explicit TipBubble (const juce::String& message)
        {
            text.append (message, Look::bold (15.0f), juce::Colours::white);
            layout.createLayoutWithBalancedLineLengths (text, 300.0f);
        }

        void getContentSize (int& width, int& height) override
        {
            width = static_cast<int> (std::ceil (layout.getWidth())) + 16;
            height = static_cast<int> (std::ceil (layout.getHeight())) + 12;
        }

        void paintContent (juce::Graphics& g, int width, int height) override
        {
            layout.draw (g, juce::Rectangle<float> (8.0f, 6.0f, static_cast<float> (width) - 16.0f, static_cast<float> (height) - 12.0f));
        }

        void showFor (juce::Component& owner)
        {
            setAlwaysOnTop (true);
            addToDesktop (juce::ComponentPeer::windowIsTemporary | juce::ComponentPeer::windowIgnoresMouseClicks);
            setPosition (owner.getScreenBounds());
            setVisible (true);
            startTimer (2000);
        }

    private:
        void timerCallback() override
        {
            stopTimer();
            delete this;
        }

        juce::AttributedString text;
        juce::TextLayout layout;
    };

    /** The one tip showing, if any: a new one replaces it, and it goes with the window. */
    juce::Component::SafePointer<juce::Component>& shownTip()
    {
        static juce::Component::SafePointer<juce::Component> tip;
        return tip;
    }

    void showTip (juce::Component& owner, const juce::String& text)
    {
        if (text.isEmpty() || ! owner.isShowing())
            return;

        delete shownTip().getComponent();
        auto* bubble = new TipBubble (text);
        shownTip() = bubble;
        bubble->showFor (owner);
    }

    /** A button that says what it does when right-clicked. */
    class TipButton final : public juce::TextButton
    {
    public:
        using juce::TextButton::TextButton;
        juce::String tip;

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
                showTip (*this, tip);
            else
                juce::TextButton::mouseDown (e);
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (! e.mods.isPopupMenu())
                juce::TextButton::mouseUp (e);
        }
    };

    /** OnStage's StyledSlider: gold, a right-click says what it does, a double-click resets it. */
    class GoldSlider final : public juce::Slider
    {
    public:
        GoldSlider (SliderStyle style, TextEntryBoxPosition box)
            : juce::Slider (style, box)
        {
            setTextBoxStyle (box, false, 70, 18);
            setMouseDragSensitivity (250);
            setVelocityBasedMode (false);
            setSliderSnapsToMousePosition (false);
        }

        juce::String tip;
        std::function<void()> onDoubleClick;

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
                showTip (*this, tip);
            else
                juce::Slider::mouseDown (e);
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! e.mods.isPopupMenu())
                juce::Slider::mouseDrag (e);
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (! e.mods.isPopupMenu())
                juce::Slider::mouseUp (e);
        }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
                return;

            if (onDoubleClick != nullptr)
                onDoubleClick();
            else
                juce::Slider::mouseDoubleClick (e);
        }
    };

    //==============================================================================
    /** What a song's banner asks of the playlist. */
    struct BannerOwner
    {
        virtual ~BannerOwner() = default;
        virtual void bannerClicked (int index) = 0;
        virtual void bannerPlay (int index) = 0;
        virtual void bannerMenu (int index) = 0;
        virtual void bannerDragged (int index, const juce::MouseEvent& e) = 0;
        virtual void bannerDropped (int index) = 0;
    };

    /** One song in the playlist (OnStage's TrackBanner): its play circle, title, X and +, and
        when it's opened, its controls. */
    class SongBanner final : public juce::Component
    {
    public:
        SongBanner (PlaylistPlayer& p, int songIndex, BannerOwner& bannerOwner)
            : player (p), index (songIndex), owner (bannerOwner)
        {
            const auto& song = player.getSongs()[(size_t) index];
            setComponentID ("banner" + juce::String (index));

            addAndMakeVisible (removeButton);
            removeButton.tip = "Remove Track from Playlist";
            removeButton.setComponentID ("remove");
            removeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            removeButton.setColour (juce::TextButton::textColourOffId, juce::Colours::red);
            removeButton.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
            removeButton.onClick = [this] { player.removeSong (index); };

            addAndMakeVisible (expandButton);
            expandButton.setButtonText (song.expanded ? "-" : "+");
            expandButton.tip = "Show/Hide Controls (Volume, Pitch, Speed, Wait, Fade)";
            expandButton.setComponentID ("expand");
            expandButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            expandButton.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
            expandButton.onClick = [this]
            {
                if (index < static_cast<int> (player.getSongs().size()))
                    player.setExpanded (index, ! player.getSongs()[(size_t) index].expanded);
            };

            if (song.expanded)
            {
                addRow ("Vol", "volume", Playlists::minVolume, Playlists::maxVolume, 0.01, song.volume, "x", 1.0,
                        "Track Volume (0.0x to 2.0x) - Double-click to reset to 1.0x",
                        [this] (double v) { player.setVolume (index, static_cast<float> (v)); });
                addRow ("Pitch", "pitch", Playlists::minPitch, Playlists::maxPitch, 1.0, song.pitch, " st", 0.0,
                        "Pitch in semitones (-12 to +12), the speed kept - Double-click to reset to 0",
                        [this] (double v) { player.setPitch (index, static_cast<float> (v)); });
                addRow ("Speed", "speed", Playlists::minSpeed, Playlists::maxSpeed, 0.01, song.speed, "x", 1.0,
                        "Playback Speed (0.5x - 1.5x), the pitch kept - Double-click to reset to 1.0x",
                        [this] (double v) { player.setSpeed (index, static_cast<float> (v)); });
                addRow ("Wait", "wait", 0.0, Playlists::maxWaitMs, 100.0, song.waitMs, " ms", 0.0,
                        "Wait after this track before the next one starts (0 - 30000 ms, Auto-Play)",
                        [this] (double v) { player.setWait (index, juce::roundToInt (v)); });
                addRow ("Fade", "fade", 0.0, Playlists::maxCrossfadeMs, 100.0, song.crossfadeMs, " ms", 0.0,
                        "Crossfade into the next track (0 - 10000 ms, Auto-Play) - the next one starts this long before this one ends",
                        [this] (double v) { player.setCrossfade (index, juce::roundToInt (v)); });
            }
        }

        static int heightFor (const PlaylistSong& song)
        {
            return song.expanded ? Look::bannerHeight + Look::rows * Look::rowHeight + 6 : Look::bannerHeight;
        }

        void setLook (bool isPlaying, bool isSelected)
        {
            if (isPlaying != playing || isSelected != selected)
            {
                playing = isPlaying;
                selected = isSelected;
                repaint();
            }
        }

        void paint (juce::Graphics& g) override
        {
            // (The playlist just changed: it's built again in a moment.)
            if (index >= static_cast<int> (player.getSongs().size()))
                return;

            const auto& song = player.getSongs()[(size_t) index];
            const auto bounds = getLocalBounds().toFloat();

            g.setColour (playing ? Look::bannerPlaying : Look::banner);
            g.fillRoundedRectangle (bounds, 5.0f);

            if (playing)
            {
                g.setColour (Look::playing);
                g.drawRoundedRectangle (bounds.reduced (1.0f), 5.0f, 2.0f);
            }
            else if (selected)
            {
                g.setColour (Look::gold);
                g.drawRoundedRectangle (bounds.reduced (1.0f), 5.0f, 1.5f);
            }
            else
            {
                g.setColour (Look::outline);
                g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);
            }

            // The play circle.
            const auto circle = circleArea().toFloat();
            g.setColour (playing ? Look::playing : Look::gold);
            g.fillEllipse (circle);
            g.setColour (juce::Colours::black);
            juce::Path triangle;
            const auto centre = circle.getCentre();
            constexpr float size = 8.0f;
            triangle.addTriangle (centre.x - size / 2.0f, centre.y - size / 2.0f, centre.x - size / 2.0f, centre.y + size / 2.0f,
                                  centre.x + size / 2.0f, centre.y);
            g.fillPath (triangle);

            // The title (a video's says so; one being read says how far it's got).
            auto text = getLocalBounds().reduced (5).withTrimmedLeft (40).withTrimmedRight (70).withHeight (34);

            if (song.video)
            {
                const auto badge = text.removeFromRight (46).withSizeKeepingCentre (42, 16).toFloat();
                g.setColour (song.missing && ! song.preparing ? Look::missing : Look::quiet);
                g.drawRoundedRectangle (badge, 3.0f, 1.0f);
                g.setFont (Look::bold (10.0f));
                g.drawText ("VIDEO", badge, juce::Justification::centred);
                text.removeFromRight (4);
            }

            juce::String title = song.title;

            if (song.preparing)
                title << "  (reading it: " << juce::String (juce::roundToInt (100.0 * juce::jmax (0.0, player.getPreparation (index)))) << "%)";
            else if (song.missing)
                title << (song.problem.isNotEmpty() ? "  (" + song.problem + ")" : juce::String ("  (missing)"));

            g.setColour (song.preparing ? Look::quiet : song.missing ? Look::missing : Look::gold);
            g.setFont (Look::bold (15.0f));
            g.drawFittedText (title, text, juce::Justification::centredLeft, 1);
        }

        bool isPreparing() const
        {
            return index < static_cast<int> (player.getSongs().size()) && player.getSongs()[(size_t) index].preparing;
        }

        void resized() override
        {
            const auto width = getWidth();
            expandButton.setBounds (width - 30, 10, 20, 20);
            removeButton.setBounds (width - 60, 10, 20, 20);

            for (int row = 0; row < static_cast<int> (sliders.size()); ++row)
            {
                const auto y = Look::bannerHeight + row * Look::rowHeight;
                labels[(size_t) row]->setBounds (10, y, 48, Look::rowHeight);
                sliders[(size_t) row]->setBounds (58, y, width - 68, Look::rowHeight);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            dragging = false;

            if (e.mods.isPopupMenu())
            {
                owner.bannerMenu (index);
                return;
            }

            pressedOnCircle = circleArea().expanded (2).contains (e.getPosition());
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
                return;

            if (! dragging && e.getDistanceFromDragStart() > 6)
                dragging = true;

            if (dragging)
                owner.bannerDragged (index, e);
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
                return;

            if (std::exchange (dragging, false))
            {
                owner.bannerDropped (index);
                return;
            }

            // The circle plays it now (once: a double-click on it doesn't start it again).
            if (pressedOnCircle && circleArea().expanded (2).contains (e.getPosition()))
            {
                if (e.getNumberOfClicks() < 2)
                    owner.bannerPlay (index);
            }
            else
            {
                owner.bannerClicked (index);   // select it
            }
        }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (! e.mods.isPopupMenu() && ! circleArea().expanded (2).contains (e.getPosition()))
                owner.bannerPlay (index);
        }

        int getIndex() const noexcept    { return index; }

    private:
        static juce::Rectangle<int> circleArea()    { return { 10, 10, 24, 24 }; }

        void addRow (const juce::String& name, const juce::String& id, double low, double high, double step, double value,
                     const juce::String& suffix, double reset, const juce::String& tip, std::function<void (double)> set)
        {
            auto label = std::make_unique<juce::Label> (id + "Label", name);
            label->setFont (Look::plain (14.0f));
            addAndMakeVisible (*label);

            auto slider = std::make_unique<GoldSlider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            slider->setComponentID (id);
            slider->tip = tip;
            slider->setRange (low, high, step);
            slider->setValue (value, juce::dontSendNotification);
            slider->setTextValueSuffix (suffix);
            auto* raw = slider.get();
            slider->onValueChange = [raw, set] { set (raw->getValue()); };
            slider->onDoubleClick = [raw, reset] { raw->setValue (reset, juce::sendNotificationSync); };
            addAndMakeVisible (*slider);

            labels.push_back (std::move (label));
            sliders.push_back (std::move (slider));
        }

        PlaylistPlayer& player;
        const int index;
        BannerOwner& owner;
        TipButton removeButton { "X" }, expandButton;
        std::vector<std::unique_ptr<juce::Label>> labels;
        std::vector<std::unique_ptr<GoldSlider>> sliders;
        bool playing = false, selected = false, dragging = false, pressedOnCircle = false;
    };

    //==============================================================================
    /** The banners' background (solid, so scrolling doesn't smear), with where a dragged song
        would go. */
    class ListArea final : public juce::Component
    {
    public:
        ListArea()                                  { setOpaque (true); }
        void paint (juce::Graphics& g) override     { g.fillAll (Look::panel); }

        void paintOverChildren (juce::Graphics& g) override
        {
            if (dropLine >= 0)
            {
                g.setColour (Look::gold);
                g.fillRect (0, dropLine - 1, getWidth(), 3);
            }
        }

        void setDropLine (int y)
        {
            if (y != dropLine)
            {
                dropLine = y;
                repaint();
            }
        }

    private:
        int dropLine = -1;
    };

    //==============================================================================
    /** OnStage's playlist panel. */
    class PlaylistPanel final : public juce::Component,
                                private BannerOwner,
                                private juce::AsyncUpdater
    {
    public:
        explicit PlaylistPanel (PlaylistPlayer& p) : player (p)
        {
            addAndMakeVisible (header);
            header.setText ("PLAYLIST", juce::dontSendNotification);
            header.setFont (Look::bold (20.0f));
            header.setColour (juce::Label::textColourId, Look::gold);

            addAndMakeVisible (autoPlay);
            autoPlay.setComponentID ("autoPlay");
            autoPlay.setButtonText ("Auto-Play");
            autoPlay.setToggleState (player.isAutoPlay(), juce::dontSendNotification);
            autoPlay.onClick = [this] { player.setAutoPlay (autoPlay.getToggleState()); };

            addAndMakeVisible (folderButton);
            folderButton.setComponentID ("folder");
            folderButton.tip = "Where Add Tracks starts";
            folderButton.onClick = [this] { chooseFolder(); };

            addAndMakeVisible (addButton);
            addButton.setComponentID ("add");
            addButton.tip = "Add audio files and videos to the playlist";
            addButton.onClick = [this] { addTracks(); };

            addAndMakeVisible (clearButton);
            clearButton.setComponentID ("clear");
            clearButton.tip = "Remove every track from the playlist";
            clearButton.onClick = [this] { player.clearSongs(); };

            addAndMakeVisible (saveButton);
            saveButton.setComponentID ("save");
            saveButton.tip = "Save the playlist as a file";
            saveButton.onClick = [this] { savePlaylist(); };

            addAndMakeVisible (loadButton);
            loadButton.setComponentID ("load");
            loadButton.tip = "Open a saved playlist";
            loadButton.onClick = [this] { loadPlaylist(); };

            addAndMakeVisible (viewport);
            viewport.setComponentID ("list");
            viewport.setViewedComponent (&list, false);
            viewport.setScrollBarsShown (true, false);

            player.onSongsChanged = [this] { triggerAsyncUpdate(); };
            rebuild();
        }

        ~PlaylistPanel() override
        {
            player.onSongsChanged = nullptr;
            cancelPendingUpdate();
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (Look::panel);
        }

        void resized() override
        {
            auto bounds = getLocalBounds();

            auto row1 = bounds.removeFromTop (35);
            header.setBounds (row1.removeFromLeft (108).reduced (5));
            autoPlay.setBounds (row1.removeFromLeft (96));
            folderButton.setBounds (row1.reduced (2));

            auto row2 = bounds.removeFromTop (35);
            const auto buttonWidth = row2.getWidth() / 4;
            addButton.setBounds (row2.removeFromLeft (buttonWidth).reduced (2));
            clearButton.setBounds (row2.removeFromLeft (buttonWidth).reduced (2));
            saveButton.setBounds (row2.removeFromLeft (buttonWidth).reduced (2));
            loadButton.setBounds (row2.reduced (2));

            viewport.setBounds (bounds);
            layoutBanners();
        }

        /** The banners again, from the playlist as it is now. */
        void rebuild()
        {
            const auto scroll = viewport.getViewPosition();
            list.setDropLine (-1);   // a drag in progress ends with the banner it was on
            dragFrom = dropAt = -1;
            banners.clear();

            for (int i = 0; i < static_cast<int> (player.getSongs().size()); ++i)
                list.addAndMakeVisible (banners.add (new SongBanner (player, i, *this)));

            layoutBanners();
            viewport.setViewPosition (scroll);
            shownPlaying = shownSelected = -2;
            refresh (player.getStatus());
        }

        /** Which banner plays and which is selected (the page's timer). */
        void refresh (const PlaylistPlayer::Status& status)
        {
            const auto playingIndex = status.state == PlaylistPlayer::State::stopped ? -1 : status.song;
            const auto selectedIndex = player.getSelected();

            for (auto* banner : banners)
                if (banner->isPreparing())
                    banner->repaint();

            if (playingIndex == shownPlaying && selectedIndex == shownSelected && autoPlay.getToggleState() == player.isAutoPlay())
                return;

            shownPlaying = playingIndex;
            shownSelected = selectedIndex;
            autoPlay.setToggleState (player.isAutoPlay(), juce::dontSendNotification);

            for (auto* banner : banners)
                banner->setLook (banner->getIndex() == playingIndex, banner->getIndex() == selectedIndex);
        }

    private:
        void handleAsyncUpdate() override   { rebuild(); }

        void layoutBanners()
        {
            const auto width = viewport.getWidth() - viewport.getScrollBarThickness();
            int y = 0;

            for (auto* banner : banners)
            {
                if (banner->getIndex() >= static_cast<int> (player.getSongs().size()))
                    break;   // the playlist just changed: it's built again in a moment

                const auto height = SongBanner::heightFor (player.getSongs()[(size_t) banner->getIndex()]);
                banner->setBounds (0, y, juce::jmax (0, width), height);
                y += height;
            }

            list.setSize (juce::jmax (0, width), y);
        }

        // BannerOwner
        void bannerClicked (int index) override
        {
            player.select (index);
            refresh (player.getStatus());
        }

        void bannerPlay (int index) override
        {
            player.playSong (index);
            refresh (player.getStatus());
        }

        void bannerMenu (int index) override
        {
            const auto count = static_cast<int> (player.getSongs().size());

            if (index < 0 || index >= count)
                return;

            const auto expanded = player.getSongs()[(size_t) index].expanded;
            juce::PopupMenu menu;
            menu.setLookAndFeel (&getLookAndFeel());   // OnStage's gold menu
            menu.addItem (1, "Play");
            menu.addItem (2, "Move Up", index > 0);
            menu.addItem (3, "Move Down", index < count - 1);
            menu.addItem (4, expanded ? "Hide Controls" : "Show Controls");
            menu.addSeparator();
            menu.addItem (5, "Remove");

            juce::Component::SafePointer<PlaylistPanel> safeThis (this);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (banners[index]),
                                [safeThis, index, expanded] (int chosen)
                                {
                                    if (safeThis == nullptr)
                                        return;

                                    auto& p = safeThis->player;

                                    switch (chosen)
                                    {
                                        case 1: p.playSong (index); break;
                                        case 2: p.moveSong (index, index - 1); break;
                                        case 3: p.moveSong (index, index + 1); break;
                                        case 4: p.setExpanded (index, ! expanded); break;
                                        case 5: p.removeSong (index); break;
                                        default: break;
                                    }
                                });
        }

        void bannerDragged (int index, const juce::MouseEvent& e) override
        {
            dragFrom = index;
            const auto y = e.getEventRelativeTo (&list).getPosition().y;
            dropAt = static_cast<int> (banners.size());
            auto lineY = list.getHeight();

            for (auto* banner : banners)
            {
                if (y < banner->getBounds().getCentreY())
                {
                    dropAt = banner->getIndex();
                    lineY = banner->getY();
                    break;
                }
            }

            list.setDropLine (lineY);

            // Near the top or bottom of the view, it scrolls.
            viewport.autoScroll (e.getEventRelativeTo (&viewport).x, e.getEventRelativeTo (&viewport).y, 20, 10);
        }

        void bannerDropped (int index) override
        {
            list.setDropLine (-1);

            if (dragFrom == index && dropAt >= 0)
            {
                // Dropped before a song: where that song is now, less one if it came from above.
                const auto to = dropAt > index ? dropAt - 1 : dropAt;

                if (to != index)
                    player.moveSong (index, to);
            }

            dragFrom = dropAt = -1;
        }

        void addTracks()
        {
            chooser = std::make_shared<juce::FileChooser> ("Select Files", Playlists::mediaFolder(), player.getWildcards());
            juce::Component::SafePointer<PlaylistPanel> safeThis (this);
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::canSelectMultipleItems,
                                  [safeThis] (const juce::FileChooser& fc)
                                  {
                                      if (safeThis != nullptr)
                                          safeThis->player.addSongs (fc.getResults());
                                  });
        }

        void chooseFolder()
        {
            chooser = std::make_shared<juce::FileChooser> ("Select Default Media Folder", Playlists::mediaFolder());
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                  [] (const juce::FileChooser& fc)
                                  {
                                      const auto folder = fc.getResult();

                                      if (folder.isDirectory() && Playlists::setMediaFolder (folder))
                                          juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Default Folder Set",
                                                                                  "Default media folder set to:\n" + folder.getFullPathName(), "OK");
                                  });
        }

        void savePlaylist()
        {
            Playlists::folder().createDirectory();
            chooser = std::make_shared<juce::FileChooser> ("Save Playlist", Playlists::folder().getChildFile ("Playlist").withFileExtension (Playlists::fileExtension),
                                                           juce::String ("*") + Playlists::fileExtension);
            juce::Component::SafePointer<PlaylistPanel> safeThis (this);
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [safeThis] (const juce::FileChooser& fc)
                                  {
                                      const auto file = fc.getResult();

                                      if (safeThis != nullptr && file != juce::File()
                                          && ! safeThis->player.savePlaylist (file.withFileExtension (Playlists::fileExtension)))
                                          juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Playlist Player",
                                                                                  "Couldn't save " + file.getFullPathName(), "OK");
                                  });
        }

        void loadPlaylist()
        {
            chooser = std::make_shared<juce::FileChooser> ("Load Playlist", Playlists::folder(), juce::String ("*") + Playlists::fileExtension);
            juce::Component::SafePointer<PlaylistPanel> safeThis (this);
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [safeThis] (const juce::FileChooser& fc)
                                  {
                                      const auto file = fc.getResult();

                                      if (safeThis != nullptr && file.existsAsFile() && ! safeThis->player.loadPlaylist (file))
                                          juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Playlist Player",
                                                                                  file.getFileName() + " isn't a playlist Sonify can read.", "OK");
                                  });
        }

        PlaylistPlayer& player;
        juce::Label header;
        juce::ToggleButton autoPlay;
        TipButton folderButton { "Set Default Folder" }, addButton { "Add Tracks" }, clearButton { "Clear All" },
                  saveButton { "Save" }, loadButton { "Load" };
        juce::Viewport viewport;
        ListArea list;
        juce::OwnedArray<SongBanner> banners;
        std::shared_ptr<juce::FileChooser> chooser;
        int dragFrom = -1, dropAt = -1, shownPlaying = -2, shownSelected = -2;
    };

    //==============================================================================
    /** Where the song playing is shown (OnStage's video surface): a video's picture, or an audio
        song's name. Its Video Window button shows the picture in a window of its own (for the
        other screen, full screen there). */
    class NowShowing final : public juce::Component
    {
    public:
        NowShowing (PlaylistPlayer& p, VideoScreen::Picture& picture)
            : player (p), view (picture)
        {
            setOpaque (true);
            addChildComponent (view);
            addAndMakeVisible (windowButton);
            windowButton.setComponentID ("videoWindow");
            windowButton.setButtonText ("Video Window");
            windowButton.tip = "The picture in a window of its own: drag it to another screen, double-click it for full screen";
            windowButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xcc111111));
            windowButton.setColour (juce::TextButton::textColourOffId, Look::gold);
            windowButton.onClick = [this] { if (onVideoWindow) onVideoWindow(); };
        }

        std::function<void()> onVideoWindow;

        /** Whether a video's picture shows (over the song's name). */
        void showPicture (bool on)
        {
            if (view.isVisible() != on)
                view.setVisible (on);
        }

        void resized() override
        {
            view.setBounds (getLocalBounds());
            windowButton.setBounds (getWidth() - 112, 8, 104, 22);
        }

        void show (const PlaylistPlayer::Status& status)
        {
            const auto& songs = player.getSongs();
            const auto index = status.song >= 0 ? status.song : player.getSelected();
            juce::String newTitle, newLine, newCount;

            if (songs.empty())
            {
                newLine = "Add Tracks to start a playlist";
            }
            else if (index >= 0 && index < static_cast<int> (songs.size()))
            {
                newTitle = songs[(size_t) index].title;
                newCount = "Track " + juce::String (index + 1) + " of " + juce::String (songs.size());

                switch (status.state)
                {
                    case PlaylistPlayer::State::playing:   newLine = "Playing"; break;
                    case PlaylistPlayer::State::paused:    newLine = "Paused"; break;
                    case PlaylistPlayer::State::waiting:   newLine = "Starts in " + juce::String (status.waitLeft, 1) + " s"; break;
                    case PlaylistPlayer::State::stopped:
                    default:                               newLine = "Ready"; break;
                }
            }

            if (newTitle != title || newLine != line || newCount != count)
            {
                title = newTitle;
                line = newLine;
                count = newCount;
                repaint();
            }
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colours::black);
            auto area = getLocalBounds().reduced (20);
            const auto middle = area.getCentreY();

            if (title.isNotEmpty())
            {
                g.setColour (Look::gold);
                g.setFont (Look::bold (juce::jlimit (18.0f, 34.0f, static_cast<float> (getHeight()) * 0.08f)));
                g.drawFittedText (title, area.withHeight (60).withCentre ({ area.getCentreX(), middle - 24 }), juce::Justification::centred, 2);
            }

            g.setColour (juce::Colours::white);
            g.setFont (Look::plain (17.0f));
            g.drawText (line, area.withHeight (24).withCentre ({ area.getCentreX(), middle + 22 }), juce::Justification::centred);

            g.setColour (Look::quiet);
            g.setFont (Look::plain (14.0f));
            g.drawText (count, area.withHeight (20).withCentre ({ area.getCentreX(), middle + 48 }), juce::Justification::centred);
        }

    private:
        PlaylistPlayer& player;
        VideoScreen::View view;
        TipButton windowButton;
        juce::String title, line, count;
    };

    //==============================================================================
    /** OnStage's Media page. */
    class PlayerPage final : public juce::Component,
                             private juce::Timer
    {
    public:
        explicit PlayerPage (PlaylistPlayer& p)
            : player (p), picture ([this] { return pictureWhere(); }), screen (p, picture), playlist (p)
        {
            setLookAndFeel (&golden);
            setComponentID ("playlistPlayer");
            screen.onVideoWindow = [this] { showVideoWindow(); };

            addAndMakeVisible (screen);
            addAndMakeVisible (playlist);

            addAndMakeVisible (playPause);
            playPause.setComponentID ("playPause");
            playPause.setButtonText ("PLAY");
            playPause.tip = "MIDI: Note 15";
            playPause.setLookAndFeel (&greenLook);
            playPause.onClick = [this] { player.playPause(); };

            addAndMakeVisible (stop);
            stop.setComponentID ("stop");
            stop.setButtonText ("STOP");
            stop.tip = "MIDI: Note 16";
            stop.setLookAndFeel (&redLook);
            stop.onClick = [this]
            {
                player.stop();
                playPause.setButtonText ("PLAY");
                progress.setValue (0.0, juce::dontSendNotification);
            };

            addAndMakeVisible (progress);
            progress.setComponentID ("progress");
            progress.tip = "Click or drag to move in the track";
            progress.setRange (0.0, 1.0, 0.001);
            progress.setSliderSnapsToMousePosition (true);
            progress.onDragStart = [this] { dragging = true; };
            progress.onDragEnd = [this]
            {
                dragging = false;
                player.seek (progress.getValue());
            };

            for (auto* label : { &totalTime, &currentTime })
            {
                addAndMakeVisible (*label);
                label->setText ("00:00", juce::dontSendNotification);
                label->setColour (juce::Label::textColourId, juce::Colours::white);
                label->setFont (Look::bold (14.0f));
            }

            totalTime.setJustificationType (juce::Justification::centredLeft);
            currentTime.setJustificationType (juce::Justification::centredRight);

            setSize (1200, 680);
            startTimerHz (30);
        }

        ~PlayerPage() override
        {
            stopTimer();
            videoWindow = nullptr;
            picture.close();
            delete shownTip().getComponent();
            playPause.setLookAndFeel (nullptr);
            stop.setLookAndFeel (nullptr);
            setLookAndFeel (nullptr);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (Look::page);
        }

        void resized() override
        {
            auto area = getLocalBounds();

            // The playlist takes 26% (OnStage), wide enough for its buttons; the player the rest.
            const auto playlistWidth = juce::jmax (340, static_cast<int> (static_cast<float> (area.getWidth()) * 0.26f));
            auto playerArea = area.removeFromLeft (area.getWidth() - playlistWidth);
            playlist.setBounds (area);

            auto transport = playerArea.removeFromBottom (30);
            playPause.setBounds (transport.removeFromLeft (transport.getWidth() / 2).reduced (2));
            stop.setBounds (transport.reduced (2));

            auto strip = playerArea.removeFromBottom (20 + 15 + 5);
            auto labels = strip.removeFromTop (15);
            totalTime.setBounds (labels.removeFromLeft (60));
            currentTime.setBounds (labels.removeFromRight (60));
            progress.setBounds (strip.reduced (2));

            // The screen keeps 16:9.
            auto width = playerArea.getWidth();
            auto height = static_cast<int> (static_cast<float> (width) * 9.0f / 16.0f);

            if (height > playerArea.getHeight())
            {
                height = playerArea.getHeight();
                width = static_cast<int> (static_cast<float> (height) * 16.0f / 9.0f);
            }

            screen.setBounds (playerArea.withSizeKeepingCentre (width, height));
        }

        /** The player as it is now: also the timer's work. */
        void update()
        {
            const auto status = player.getStatus();
            const auto isPlaying = status.state == PlaylistPlayer::State::playing;
            playPause.setButtonText (isPlaying ? "PAUSE" : "PLAY");

            if (! dragging && ! progress.isMouseButtonDown())
                progress.setValue (status.length > 0.0 ? status.position / status.length : 0.0, juce::dontSendNotification);

            totalTime.setText (formatTime (status.length), juce::dontSendNotification);
            currentTime.setText (formatTime (dragging ? progress.getValue() * status.length : status.position), juce::dontSendNotification);
            screen.show (status);
            playlist.refresh (status);
            followVideo (status);
        }

        /** The video song playing (or paused) opened for its picture; none: black. */
        void followVideo (const PlaylistPlayer::Status& status)
        {
            const auto& songs = player.getSongs();
            const auto* song = status.state != PlaylistPlayer::State::stopped && status.song >= 0 && status.song < static_cast<int> (songs.size())
                                   ? &songs[(size_t) status.song] : nullptr;
            const juce::File wanted = song != nullptr && song->video && ! song->missing ? juce::File (song->file) : juce::File();

            if (wanted != picture.getFile())
            {
                picture.close();

                if (wanted != juce::File())
                {
                    VideoMedia::Wanted parts;
                    parts.thumbnails = false;
                    parts.silenceWhenNoSound = true;

                    if (const auto ready = VideoMedia::cached (wanted, parts); ! ready || ! picture.open (wanted, ready->info))
                        picture.setMessage ("VLC couldn't play " + wanted.getFileName() + ".");
                }
            }

            screen.showPicture (picture.isOpen());
        }

        /** Where the picture should be: the song's own time, at its speed. */
        VideoScreen::Where pictureWhere() const
        {
            const auto status = player.getStatus();
            const auto& songs = player.getSongs();

            // (A mixdown races through the songs: the picture rests meanwhile.)
            if (status.song < 0 || status.song >= static_cast<int> (songs.size()) || ! songs[(size_t) status.song].video
                || status.state == PlaylistPlayer::State::stopped || status.state == PlaylistPlayer::State::waiting
                || Plugin::offlineRendering().load())
                return {};

            return { true, status.position, status.state == PlaylistPlayer::State::playing,
                     static_cast<double> (songs[(size_t) status.song].speed) };
        }

        void showVideoWindow()
        {
            if (videoWindow == nullptr)
                videoWindow = std::make_unique<VideoScreen::Window> (picture, "Playlist Player - Video");

            videoWindow->setVisible (true);
            videoWindow->toFront (true);
        }

    private:
        void timerCallback() override   { update(); }

        PlaylistPlayer& player;
        VideoScreen::Picture picture;
        std::unique_ptr<VideoScreen::Window> videoWindow;

        // The looks first: they outlast the controls that use them.
        GoldenLook golden;
        GlassyLook greenLook { true }, redLook { false };

        NowShowing screen;
        PlaylistPanel playlist;
        TipButton playPause, stop;
        GoldSlider progress { juce::Slider::LinearBar, juce::Slider::NoTextBox };
        juce::Label totalTime, currentTime;
        bool dragging = false;
    };

    //==============================================================================
    class PlayerWindow final : public juce::DocumentWindow
    {
    public:
        explicit PlayerWindow (PlaylistPlayer& player)
            : juce::DocumentWindow ("Playlist Player", Look::page, juce::DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (PlaylistScreen::createPage (player).release(), true);
            setResizable (true, true);
            setResizeLimits (760, 420, 8000, 6000);
            centreWithSize (getWidth(), getHeight());
        }

        void closeButtonPressed() override
        {
            setVisible (false);   // kept for next time, where it was
        }
    };

    class WindowScreen final : public Playlists::Screen
    {
    public:
        explicit WindowScreen (PlaylistPlayer& p) : player (p) {}

        void show (const juce::String& title) override
        {
            if (window == nullptr)
                window = std::make_unique<PlayerWindow> (player);

            window->setName (title.isNotEmpty() ? title : juce::String ("Playlist Player"));
            window->setVisible (true);
            window->toFront (true);
        }

        void hide() override
        {
            if (window != nullptr)
                window->setVisible (false);
        }

        bool isShowing() const override   { return window != nullptr && window->isVisible(); }

    private:
        PlaylistPlayer& player;
        std::unique_ptr<PlayerWindow> window;
    };
}

//==============================================================================
std::unique_ptr<juce::Component> PlaylistScreen::createPage (PlaylistPlayer& player)
{
    return std::make_unique<PlayerPage> (player);
}

void PlaylistScreen::install()
{
    Playlists::setScreenFactory ([] (PlaylistPlayer& player) -> std::unique_ptr<Playlists::Screen>
    {
        return std::make_unique<WindowScreen> (player);
    });
}
