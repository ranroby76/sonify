// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakEditor.cpp
#include "ZodiakEditor.h"
#include "ZodiakRoll.h"
#include "ZodiakGraphUi.h"
#include "ZodiakLinkUi.h"
#include <initializer_list>

using namespace zodiak;
using namespace zodiak::ui;

namespace
{
    // The design size, and where things sit on it
    constexpr int kWidth    = 2180;
    constexpr int kHeaderH  = 72;
    constexpr int kModeY    = 72, kModeH = 54;                            // PLAY | MIXER, MIDI LINK, the set
    constexpr int kStepsY   = 130, kStepsH = 40;                          // the page starts here
    constexpr int kRowsY    = 176, kRowPitch = 114, kRowH = 106;
    constexpr int kBannerEnd = kRowsY + kNumChannels * kRowPitch - 8;     // 1080: the page ends here
    constexpr int kTransY   = kBannerEnd + 8, kTransH = 100;             // 1088
    constexpr int kHeight   = kTransY + kTransH + 12;                     // 1200

    // Within a row
    constexpr int kChX = 16, kChW = 100;
    constexpr int kPatX = 150, kPatW = 320;         // the BANK and VAR knobs' space
    constexpr int kNameX = 486, kNameW = 170;
    constexpr int kMuteX = 664, kMuteW = 42;                              // M over S: mute and solo
    constexpr int kInstX = 714, kInstW = 44;                              // its e: the instrument's window
    constexpr int kMacroX0 = 828, kMacroPitch = 136, kMacroW = 116;
    constexpr int kRollX = 1972, kEditX = 2062, kBtnW = 80;

    juce::Label& styleLabel (juce::Label& l, float h, juce::Colour c, bool bold = true)
    {
        l.setFont (font (h, bold));
        l.setColour (juce::Label::textColourId, c);
        l.setInterceptsMouseClicks (false, false);
        return l;
    }

    void styleHeaderButton (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2E34));
        b.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFFD5D8DC));
    }

    juce::String instrumentName (ZodiakProcessor& proc, int ch)
    {
        const auto n = proc.instrumentName (ch);
        if (n.isNotEmpty()) return n;
        if (auto* node = proc.engine().graph (ch).firstInstrument())
            if (node->wantedName.isNotEmpty()) return node->wantedName + " (missing)";
        return "Load a VSTi";
    }

    juce::String instrumentKind (ZodiakProcessor& proc, int ch)
    {
        return proc.instrumentName (ch).isNotEmpty() ? "VSTi" : "no instrument";
    }
}

namespace zodiak::ui
{
    //==========================================================================
    //  The mode bar, under the header: the page (PLAY: the banner; MIXER: the
    //  mixer, on the same room), MIDI LINK, and the SET playing, in a pill
    //==========================================================================
    class ModeBar final : public juce::Component
    {
    public:
        std::function<void (bool mixer)> onPage;

        explicit ModeBar (ZodiakProcessor& p) : linkCanvas (p)
        {
            addAndMakeVisible (linkCanvas);
        }

        void setPage (bool mixer)                    { if (mixer != mixerShown) { mixerShown = mixer; repaint(); } }
        void setSet (const juce::String& name)       { if (name != setName) { setName = name; repaint(); } }

        void paint (juce::Graphics& g) override
        {
            g.setColour (win::row);
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);

            // The pill: PLAY | MIXER
            const auto p = pill.toFloat();
            g.setColour (win::well);
            g.fillRoundedRectangle (p, p.getHeight() * 0.5f);
            auto half = p.reduced (4.0f);
            const auto lit = mixerShown ? half.removeFromRight (half.getWidth() * 0.5f) : half.removeFromLeft (half.getWidth() * 0.5f);
            g.setColour (win::arrow);
            g.fillRoundedRectangle (lit, lit.getHeight() * 0.5f);

            g.setFont (font (18.0f, true));
            auto text = pill;
            const auto left = text.removeFromLeft (text.getWidth() / 2);
            g.setColour (mixerShown ? win::label : win::bg);
            g.drawText ("PLAY", left, juce::Justification::centred, false);
            g.setColour (mixerShown ? win::bg : win::label);
            g.drawText ("MIXER", text, juce::Justification::centred, false);

            // The white line between the pill and MIDI LINK
            g.setColour (juce::Colours::white);
            g.fillRect (separator);

            // The set: a bright grey pill in the middle, its name in black
            const auto sp = setPill.toFloat();
            g.setColour (juce::Colour (0xFFD4D7DC));
            g.fillRoundedRectangle (sp, sp.getHeight() * 0.5f);
            auto inner = setPill.reduced (20, 0);
            g.setColour (juce::Colour (0xFF5A5F66));
            g.setFont (font (14.0f, true));
            g.drawText ("SET", inner.removeFromLeft (40), juce::Justification::centredLeft, false);
            g.setColour (juce::Colours::black);
            g.setFont (font (19.0f, true));
            g.drawFittedText (setName, inner.withTrimmedRight (40), juce::Justification::centred, 1, 0.8f);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (12, 7);
            pill = r.removeFromLeft (320);
            r.removeFromLeft (16);
            separator = r.removeFromLeft (2).withSizeKeepingCentre (2, getHeight() - 14);
            r.removeFromLeft (16);
            linkCanvas.setBounds (r.removeFromLeft (330));

            // The set's pill: centred on the bar
            setPill = juce::Rectangle<int> (620, r.getHeight()).withCentre ({ getWidth() / 2, getHeight() / 2 });
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (! pill.contains (e.getPosition())) return;
            const bool mixer = e.x >= pill.getCentreX();
            if (mixer != mixerShown && onPage) onPage (mixer);
        }

    private:
        bool mixerShown = false;
        juce::String setName;
        juce::Rectangle<int> pill, separator, setPill;
        LinkCanvas linkCanvas;
    };

    //==========================================================================
    //  The 16 step LEDs
    //==========================================================================
    class StepLeds final : public juce::Component
    {
    public:
        void setStep (int s)   { if (s != step) { step = s; repaint(); } }

        void paint (juce::Graphics& g) override
        {
            g.setColour (win::dim);
            g.setFont (font (15.0f));
            g.drawText ("Steps", juce::Rectangle<int> (kChX + 8, 0, 100, getHeight()), juce::Justification::centredLeft, false);

            const float y = (float) getHeight() * 0.5f;
            for (int i = 0; i < 16; ++i)
            {
                const float x = (float) kPatX + 10.0f + (float) i * 20.0f;
                const juce::Colour c = i == step ? win::led : (i % 4 == 0 ? juce::Colour (0xFF5A5F66) : juce::Colour (0xFF3A3F45));
                g.setColour (c);
                g.fillEllipse (x - 7.0f, y - 7.0f, 14.0f, 14.0f);
                if (i == step)
                {
                    g.setColour (win::led.withAlpha (0.35f));
                    g.fillEllipse (x - 10.0f, y - 10.0f, 20.0f, 20.0f);
                }
            }
        }

    private:
        int step = -1;
    };

    //==========================================================================
    //  A pattern knob: the channel's BANK (A-D, 4 places) or its VARIATION
    //  (1-8, 8 places).  It clicks from place to place: drag up or down, turn
    //  the wheel, or click a place's dot.  The place it is on is written under
    //  it.  Around it, one dot per place: bright = that pattern (a bank: any of
    //  its eight) has notes.  While the pattern chosen waits for the bar's end
    //  the knob blinks.  Right-click: onMenu.  Double-click: the first place.
    //==========================================================================
    class PatternKnob final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        PatternKnob (int places, const juce::String& caption, const ChannelColours& c) : count (places), title (caption), colours (c) {}

        std::function<int()>               getPlace;
        std::function<void (int)>          setPlace;
        std::function<juce::String (int)>  placeName;
        std::function<void()>              onMenu;

        /** Its look, each timer tick: where it is, which places have notes (a bit each), blinking off. */
        void setLook (int place, unsigned notes, bool blinkOff)
        {
            if (place != shown || notes != noteBits || blinkOff != dimmed) { shown = place; noteBits = notes; dimmed = blinkOff; repaint(); }
        }

        /** Its MIDI controller ("CC 21", or nothing), and whether it waits to learn one. */
        void setLearn (const juce::String& controller, bool waiting)
        {
            if (controller != badge || waiting != learning) { badge = controller; learning = waiting; repaint(); }
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            auto nameArea = b.removeFromBottom (kNameH);
            const auto c = b.getCentre();
            const float r = radius();

            for (int i = 0; i < count; ++i)
            {
                const auto p = dotAt (i);
                const bool here = i == shown, notes = (noteBits >> i) & 1u;
                const float d = here ? 9.0f : 7.0f;
                g.setColour (notes ? colours.edge : juce::Colour (0xFF4A4F56));
                g.fillEllipse (p.x - d * 0.5f, p.y - d * 0.5f, d, d);
                if (here)
                {
                    g.setColour (win::text);
                    g.drawEllipse (p.x - d * 0.5f - 2.0f, p.y - d * 0.5f - 2.0f, d + 4.0f, d + 4.0f, 1.4f);
                }
            }

            g.setColour (learning ? col::learn : dimmed ? colours.dark : colours.light);
            g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
            g.setColour (colours.edge);
            g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 2.2f);

            const float a = angleOf (shown);
            g.setColour (dimmed ? colours.lightText : colours.dark);
            g.drawLine ({ { c.x + std::sin (a) * r * 0.25f, c.y - std::cos (a) * r * 0.25f },
                          { c.x + std::sin (a) * (r - 3.0f), c.y - std::cos (a) * (r - 3.0f) } }, 3.4f);

            if (badge.isNotEmpty() || learning)
            {
                g.setFont (font (9.0f, true));
                g.setColour (col::learn.withAlpha (0.9f));
                g.drawText (learning ? juce::String ("LEARN") : badge, getLocalBounds().removeFromTop (11).toFloat(), juce::Justification::centredRight, false);
            }

            // BANK A / VAR 3: the caption quiet, the place loud
            const auto place = placeName ? placeName (shown) : juce::String (shown + 1);
            const auto quiet = font (16.0f), loud = font (21.0f, true);
            const float wq = juce::GlyphArrangement::getStringWidth (quiet, title + " "), wl = juce::GlyphArrangement::getStringWidth (loud, place);
            float x = nameArea.getCentreX() - (wq + wl) * 0.5f;
            g.setColour (win::label);
            g.setFont (quiet);
            g.drawText (title, juce::Rectangle<float> (x, nameArea.getY(), wq, nameArea.getHeight()), juce::Justification::centredLeft, false);
            g.setColour (win::text);
            g.setFont (loud);
            g.drawText (place, juce::Rectangle<float> (x + wq, nameArea.getY(), wl + 2.0f, nameArea.getHeight()), juce::Justification::centredLeft, false);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu()) { if (onMenu) onMenu(); return; }
            dragFrom = shown;
            // A click on a place's dot goes there
            for (int i = 0; i < count; ++i)
                if (dotAt (i).getDistanceFrom (e.position) < 9.0f) { set (i); dragFrom = i; return; }
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu()) return;
            set (dragFrom - (int) std::lround ((float) e.getDistanceFromDragStartY() / kPixelsPerPlace));
        }

        void mouseDoubleClick (const juce::MouseEvent& e) override   { if (! e.mods.isPopupMenu()) set (0); }

        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
        {
            // One place per notch (a touchpad's small moves add up)
            wheel += w.deltaY * 6.0f;
            const int moved = (int) wheel;
            if (moved != 0) { wheel -= (float) moved; set (shown + moved); }
        }

    private:
        void set (int place)
        {
            place = juce::jlimit (0, count - 1, place);
            if (place == shown) return;
            shown = place;
            if (setPlace) setPlace (place);
            repaint();
        }

        float radius() const   { return (std::min ((float) getWidth(), (float) getHeight() - kNameH) - 26.0f) * 0.5f; }

        float angleOf (int place) const
        {
            const float a0 = -juce::MathConstants<float>::pi * 0.75f, a1 = juce::MathConstants<float>::pi * 0.75f;
            return a0 + (a1 - a0) * (float) place / (float) (count - 1);
        }

        juce::Point<float> dotAt (int place) const
        {
            const auto c = getLocalBounds().toFloat().withTrimmedBottom (kNameH).getCentre();
            const float a = angleOf (place), r = radius() + 9.0f;
            return { c.x + std::sin (a) * r, c.y - std::cos (a) * r };
        }

        static constexpr float kNameH = 26.0f, kPixelsPerPlace = 16.0f;
        const int count;
        const juce::String title;
        const ChannelColours& colours;
        int shown = 0, dragFrom = 0;
        unsigned noteBits = 0;
        bool dimmed = false, learning = false;
        juce::String badge;
        float wheel = 0.0f;
    };

    // The patterns' clipboard (Copy and Paste on the VAR knob's right-click)
    std::unique_ptr<seq::Pattern>& patternClipboard()
    {
        static std::unique_ptr<seq::Pattern> clip;
        return clip;
    }

    //==========================================================================
    //  M and S on the banner: the mixer strip's mute and solo
    //==========================================================================
    class MuteSoloButton final : public juce::Button
    {
    public:
        MuteSoloButton (const juce::String& text, const ChannelColours& c, juce::Colour litFill, juce::Colour litText)
            : juce::Button (text), colours (c), fill (litFill), ink (litText)
        {
            setClickingTogglesState (true);
        }

        void paintButton (juce::Graphics& g, bool over, bool) override
        {
            auto b = getLocalBounds().toFloat().reduced (2.0f);
            const bool lit = getToggleState();
            g.setColour (lit ? fill : (over ? colours.light.brighter (0.08f) : colours.light));
            g.fillRoundedRectangle (b, 5.0f);
            g.setColour (lit ? fill.darker (0.4f) : colours.edge);
            g.drawRoundedRectangle (b, 5.0f, 1.6f);
            g.setColour (lit ? ink : colours.darkText);
            g.setFont (font (17.0f, true));
            g.drawText (getButtonText(), b, juce::Justification::centred, false);
        }

    private:
        const ChannelColours& colours;
        const juce::Colour fill, ink;
    };

    //==========================================================================
    //  ROLL / EDIT, in the channel's colours
    //==========================================================================
    class ChannelButton final : public juce::Button
    {
    public:
        ChannelButton (const juce::String& text, const ChannelColours& c, float size = 17.0f)
            : juce::Button (text), colours (c), textSize (size) {}

        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            auto b = getLocalBounds().toFloat().reduced (2.0f);
            g.setColour (down ? colours.dark : over ? colours.light.brighter (0.08f) : colours.light);
            g.fillRoundedRectangle (b, 6.0f);
            g.setColour (colours.edge);
            g.drawRoundedRectangle (b, 6.0f, 1.8f);
            g.setColour (down ? colours.lightText : colours.darkText);
            g.setFont (font (textSize, true));
            g.drawText (getButtonText(), b, juce::Justification::centred, false);
        }

    private:
        const ChannelColours& colours;
        const float textSize;
    };

    //==========================================================================
    //  One channel's row on the banner
    //==========================================================================
    class BannerRow final : public juce::Component
    {
    public:
        BannerRow (ZodiakProcessor& p, ZodiakEditor& e, int channel)
            : proc (p), editor (e), ch (channel), colours (channelColours (channel)),
              rollBtn ("Roll", colours), editBtn ("Graph", colours), instBtn ("e", colours, 24.0f),
              muteBtn ("M", colours, juce::Colour (0xFFD05050), juce::Colours::white),
              soloBtn ("S", colours, juce::Colour (0xFFE0C040), juce::Colours::black)
        {
            bankKnob.getPlace  = [this] { return proc.bank (ch); };
            bankKnob.setPlace  = [this] (int b) { proc.choosePattern (ch, b, proc.variation (ch)); refresh(); };
            bankKnob.placeName = [] (int b) { return juce::String::charToString ((juce::juce_wchar) ('A' + b)); };
            bankKnob.onMenu    = [this] { showLearnMenu (0); };
            bankKnob.setTooltip ("The bank, A to D: each has eight variations. Drag, turn the wheel, or click a dot. A bright dot: that bank has notes. "
                                 "Right-click: MIDI learn (the controller's travel in four equal parts: 0-31 A, 32-63 B, 64-95 C, 96-127 D).");
            addAndMakeVisible (bankKnob);

            varKnob.getPlace  = [this] { return proc.variation (ch); };
            varKnob.setPlace  = [this] (int v) { proc.choosePattern (ch, proc.bank (ch), v); refresh(); };
            varKnob.placeName = [] (int v) { return juce::String (v + 1); };
            varKnob.onMenu    = [this] { showPatternMenu (proc.variation (ch)); };
            varKnob.setTooltip ("The variation, 1 to 8: the pattern that plays (while playing, from the bar's end). Drag, turn the wheel, or click a dot. "
                                "A bright dot: that pattern has notes. Right-click: copy, paste, clear, MIDI learn (the controller's travel in eight equal "
                                "parts of 16).");
            addAndMakeVisible (varKnob);
            linkTarget (bankKnob, proc, link::Target::bank (ch), false);
            linkTarget (varKnob, proc, link::Target::variation (ch), false);

            auto& st = proc.engine().channel (ch);
            for (int m = 0; m < kNumMacros; ++m)
            {
                auto& k = macroKnobs[(size_t) m];
                k.colours = &colours;
                k.nameHeight = 26.0f;
                k.nameFont   = 19.5f;
                // A macro is its destination's own control: it stands where the control
                // stands, turns it, and follows it (ZodiakMacros.h)
                k.getNorm = [this, m] { return proc.engine().macroNorm (ch, m); };
                k.setNorm = [this, m] (float v) { proc.engine().setMacroNorm (ch, m, v); };
                k.textFor = [this, m] (float v) { return macroText (m, v); };
                k.onMenu  = [this, m] { showMacroMenu (m); };
                addAndMakeVisible (k);
                linkTarget (k, proc, link::Target::macro (ch, m), false);       // the knob shows its link itself
            }

            rollBtn.setTooltip ("The channel's piano roll");
            rollBtn.onClick = [this] { editor.openRoll (ch); };
            editBtn.setTooltip ("The channel's graph: its VSTi, its effects, and how they're wired");
            editBtn.onClick = [this] { editor.openGraph (ch); };
            instBtn.setTooltip ("The VSTi's own window (none loaded yet: load one)");
            instBtn.onClick = [this] { editor.openInstrument (ch, &instBtn); };
            addAndMakeVisible (rollBtn);
            addAndMakeVisible (editBtn);
            addAndMakeVisible (instBtn);

            // Mute and solo: the same as on its mixer strip
            muteBtn.setTooltip ("Mute the channel");
            soloBtn.setTooltip ("Solo the channel");
            muteBtn.onClick = [this, &st] { st.chan[cpMute].store (muteBtn.getToggleState() ? 1.0f : 0.0f); };
            soloBtn.onClick = [this, &st] { st.chan[cpSolo].store (soloBtn.getToggleState() ? 1.0f : 0.0f); };
            addAndMakeVisible (muteBtn);
            addAndMakeVisible (soloBtn);
            linkTarget (muteBtn, proc, link::Target::chan (ch, cpMute));
            linkTarget (soloBtn, proc, link::Target::chan (ch, cpSolo));

            refresh();
        }

        void refresh()
        {
            // The knobs: where they are, which places have notes, and the blink while a pattern
            // waits for the bar's end
            {
                auto& s = proc.engine().patterns();
                const int want = s.requested (ch), now = s.playing (ch);
                const bool waiting = proc.engine().isPlaying() && want != now;
                const bool off = waiting && ((juce::Time::getMillisecondCounter() / 220) & 1) != 0;
                unsigned bankNotes = 0, varNotes = 0;
                for (int b = 0; b < 4; ++b)
                    for (int v = 0; v < 8; ++v)
                        if (s.noteCount (ch, b * 8 + v) > 0)
                        {
                            bankNotes |= 1u << b;
                            if (b == proc.bank (ch)) varNotes |= 1u << v;
                        }
                bankKnob.setLook (proc.bank (ch), bankNotes, off && want / 8 != now / 8);
                varKnob.setLook (proc.variation (ch), varNotes, off);

                bankKnob.setLearn (linkBadge (link::Target::bank (ch)), proc.isLinkLearning (link::Target::bank (ch)));
                varKnob.setLearn (linkBadge (link::Target::variation (ch)), proc.isLinkLearning (link::Target::variation (ch)));
            }

            auto& eng = proc.engine();
            auto& st = eng.channel (ch);
            muteBtn.setToggleState (st.chan[cpMute].load() > 0.5f, juce::dontSendNotification);
            soloBtn.setToggleState (st.chan[cpSolo].load() > 0.5f, juce::dontSendNotification);
            auto& gr = eng.graph (ch);
            for (int m = 0; m < kNumMacros; ++m)
            {
                auto& k = macroKnobs[(size_t) m];
                const auto& mac = st.macros[(size_t) m];
                const int d = mac.dest.load();
                const bool linked = macros::isLinked (gr, d);
                const auto name = linked ? macros::shortName (gr, d) : juce::String ("Macro ") + juce::String (m + 1);
                const bool bip = isBipolar (d);
                const auto badge = linkBadge (link::Target::macro (ch, m));
                const bool learning = proc.isLinkLearning (link::Target::macro (ch, m));
                const int steps = linked ? macros::stepsOf (gr, d) : 0;
                if (name != k.name || bip != k.bipolar || badge != k.badge || learning != k.learning || steps != k.steps)
                {
                    k.name = name;
                    k.bipolar = bip;
                    k.rest = restOf (d);
                    k.badge = badge;
                    k.learning = learning;
                    k.steps = steps;
                    k.setTooltip ("Macro " + juce::String (m + 1) + ": " + (linked ? macros::longName (gr, d) : juce::String ("no destination"))
                                  + ". Right-click: MIDI learn, destination.  MIDI LINK: drop a source on it.");
                    k.repaint();
                }
                k.refresh();
            }

            const auto n = st.notes.load();
            const bool active = n != lastNotes;
            lastNotes = n;
            const auto name = instrumentName (proc, ch), kind = instrumentKind (proc, ch);
            if (name != shownName || kind != shownKind || active != shownActive)
            {
                shownName = name;
                shownKind = kind;
                shownActive = active;
                repaint (nameArea());
            }
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (win::row);
            g.fillRoundedRectangle (b, 12.0f);

            // Ch N
            const auto chArea = juce::Rectangle<float> ((float) kChX, b.getCentreY() - 26.0f, (float) kChW, 52.0f);
            g.setColour (colours.dark);
            g.fillRoundedRectangle (chArea, 10.0f);
            g.setColour (colours.lightText);
            g.setFont (font (20.0f, true));
            g.drawText ("Ch " + juce::String (ch + 1), chArea, juce::Justification::centred, false);

            // The instrument's name
            const auto na = nameArea().toFloat();
            g.setColour (win::well);
            g.fillRoundedRectangle (na, 6.0f);
            g.setColour (colours.edge);
            g.drawRoundedRectangle (na.reduced (0.5f), 6.0f, 1.5f);
            g.setColour (win::text);
            g.setFont (font (19.0f, true));
            g.drawFittedText (shownName, na.withTrimmedBottom (na.getHeight() * 0.42f).reduced (10.0f, 0.0f).toNearestInt(),
                              juce::Justification::centredBottom, 1, 0.75f);
            g.setColour (win::dim);
            g.setFont (font (15.0f));
            g.drawText (shownKind, na.withTrimmedTop (na.getHeight() * 0.58f), juce::Justification::centredTop, false);

            // The activity lamp
            g.setColour (shownActive ? colours.edge : juce::Colour (0xFF3A3F45));
            g.fillEllipse (na.getRight() - 16.0f, na.getY() + 8.0f, 8.0f, 8.0f);
        }

        void resized() override
        {
            const int cy = getHeight() / 2;
            bankKnob.setBounds (kPatX + kPatW / 4 - kMacroW / 2, cy - 50, kMacroW, 102);
            varKnob.setBounds (kPatX + 3 * kPatW / 4 - kMacroW / 2, cy - 50, kMacroW, 102);

            for (int m = 0; m < kNumMacros; ++m)
                macroKnobs[(size_t) m].setBounds (kMacroX0 + m * kMacroPitch - kMacroW / 2, cy - 50, kMacroW, 102);

            instBtn.setBounds (kInstX, cy - 22, kInstW, 44);
            muteBtn.setBounds (kMuteX, cy - 40, kMuteW, 38);
            soloBtn.setBounds (kMuteX, cy + 2, kMuteW, 38);

            rollBtn.setBounds (kRollX, cy - 26, kBtnW, 52);
            editBtn.setBounds (kEditX, cy - 26, kBtnW, 52);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (nameArea().contains (e.getPosition())) showSoundMenu();
        }

    private:
        juce::Rectangle<int> nameArea() const   { return { kNameX, getHeight() / 2 - 36, kNameW, 72 }; }

        // The name's click: the VSTi (open its window, load another, remove it), the graph
        void showSoundMenu()
        {
            auto& gr = proc.engine().graph (ch);
            auto* inst = gr.firstInstrument();
            juce::PopupMenu menu;
            const bool loaded = inst != nullptr && inst->plugin != nullptr;
            menu.addItem (1, "Open its window", loaded);
            menu.addItem (2, loaded ? "Load another VSTi..." : "Load a VSTi...", inst != nullptr);
            menu.addItem (3, "Remove the VSTi", loaded);
            menu.addSeparator();
            menu.addItem (4, "The channel's graph...");
            if (inst == nullptr) menu.addItem (5, "Add a VSTi node, and load one...");

            menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (nameArea())),
                [safe = juce::Component::SafePointer<BannerRow> (this)] (int res)
                {
                    if (safe == nullptr || res == 0) return;
                    auto& g = safe->proc.engine().graph (safe->ch);
                    auto* node = g.firstInstrument();
                    auto& hub = safe->editor.windows();
                    if (res == 4) { safe->editor.openGraph (safe->ch); return; }
                    if (res == 5)
                    {
                        if (auto* n = g.addNode (graph::ntInstrument, { 40.0f, 420.0f }, true))
                        {
                            if (auto* o = g.output()) g.connect (n->uid, o->uid, graph::kPinMain);
                            chooseNodePlugin (safe->proc, hub, safe->ch, n->uid, safe.getComponent());
                        }
                        return;
                    }
                    if (node == nullptr) return;
                    if (res == 1) openNodePlugin (safe->proc, hub, safe->ch, node->uid, safe.getComponent());
                    if (res == 2) chooseNodePlugin (safe->proc, hub, safe->ch, node->uid, safe.getComponent());
                    if (res == 3)
                    {
                        hub.close (nodeWindowKey (safe->ch, node->uid));
                        node->wantedFile = juce::File();
                        node->wantedSonify = {};
                        node->wantedName = {};
                        g.setPlugin (node->uid, nullptr);
                        safe->proc.rackChanged();
                    }
                    safe->refresh();
                });
        }

        // A macro's value as text (v: its knob's 0..1)
        juce::String macroText (int m, float v)
        {
            auto& gr = proc.engine().graph (ch);
            const int d = proc.engine().channel (ch).macros[(size_t) m].dest.load();
            if (macros::kindOf (d) == macros::dkChan)
            {
                const auto& info = chanParamInfo (macros::chanOf (d));
                float x = fromNorm (v, info.min, info.max, info.skew);
                if (info.kind == pkStepped) x = std::round (x);
                return ui::chanText (macros::chanOf (d), x);
            }
            if (auto* n = macros::nodeFor (gr, d))
            {
                const int p = macros::paramOf (d);
                float lo, hi, sk;
                n->range (p, lo, hi, sk);
                return n->text (p, fromNorm (v, lo, hi, sk));
            }
            return juce::String ((int) std::lround (v * 100.0f)) + "%";
        }

        bool isBipolar (int d)
        {
            if (macros::kindOf (d) == macros::dkChan)
            {
                const int p = macros::chanOf (d);
                return p == cpTranspose || p == cpOctave || p == cpPan;
            }
            float lo = 0.0f, hi = 1.0f, sk = 1.0f;
            if (auto* n = macros::nodeFor (proc.engine().graph (ch), d)) n->range (macros::paramOf (d), lo, hi, sk);
            return lo < 0.0f && hi > 0.0f;
        }

        float restOf (int d)
        {
            if (macros::kindOf (d) == macros::dkChan)
            {
                const auto& info = chanParamInfo (macros::chanOf (d));
                return toNorm (info.def, info.min, info.max, info.skew);
            }
            if (auto* n = macros::nodeFor (proc.engine().graph (ch), d))
            {
                const int p = macros::paramOf (d);
                float lo, hi, sk;
                n->range (p, lo, hi, sk);
                return graph::isPluginType (n->type) ? n->restValue (p) : toNorm (n->restValue (p), lo, hi, sk);
            }
            return 0.5f;
        }

        // A variation's right-click: its pattern (in the bank chosen) copied, pasted, cleared
        void showPatternMenu (int v)
        {
            const int index = seq::patternIndex (proc.bank (ch), v);
            const auto name = seq::patternName (index);
            auto& s = proc.engine().patterns();
            juce::PopupMenu menu;
            menu.addSectionHeader ("Pattern " + name);
            menu.addItem (1, "Copy");
            menu.addItem (2, "Paste", patternClipboard() != nullptr);
            menu.addItem (3, "Clear", s.noteCount (ch, index) > 0);
            menu.addSeparator();
            addLinkItems (menu, link::Target::variation (ch));
            juce::Component::SafePointer<BannerRow> safe (this);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&varKnob),
                                [safe, index] (int r)
            {
                if (safe == nullptr || r == 0) return;
                if (r >= 100) { safe->linkChosen (link::Target::variation (safe->ch), r, safe->varKnob); return; }
                auto& seqr = safe->proc.engine().patterns();
                if (r == 1)      patternClipboard() = std::make_unique<seq::Pattern> (seqr.get (safe->ch, index));
                else if (r == 2) { if (patternClipboard() != nullptr) seqr.set (safe->ch, index, *patternClipboard()); }
                else if (r == 3) { auto p = seqr.get (safe->ch, index); p.notes.clear(); seqr.set (safe->ch, index, std::move (p)); }
                safe->refresh();
            });
        }

        // A control's source in the MIDI LINK map, as its badge shows it ("CC 21"), or nothing
        juce::String linkBadge (const link::Target& t)
        {
            const int slot = proc.links().find (t);
            return slot >= 0 ? proc.links().sourceOf (slot).shortText() : juce::String();
        }

        // MIDI LINK in a control's menu: learn, its link's window, forget (items 100..102)
        void addLinkItems (juce::PopupMenu& menu, const link::Target& t)
        {
            const int slot = proc.links().find (t);
            menu.addItem (100, proc.isLinkLearning (t) ? "Cancel MIDI learn" : "MIDI learn (move a controller)");
            if (slot >= 0)
            {
                const auto src = proc.links().sourceOf (slot);
                menu.addItem (102, "MIDI link: " + src.text() + "...");
                menu.addItem (101, "Forget " + src.shortText());
            }
        }

        void linkChosen (const link::Target& t, int result, juce::Component& near)
        {
            if (result == 100)
            {
                if (proc.isLinkLearning (t)) proc.cancelLinkLearn();
                else                         proc.startLinkLearn (t);
            }
            else if (result == 101) proc.links().clear (t);
            else if (result == 102) showLinkPopup (proc, near, t);
            refreshLinkTargets();
            refresh();
        }

        void showLearnMenu (int which)
        {
            const auto t = which == 0 ? link::Target::bank (ch) : link::Target::variation (ch);
            juce::PopupMenu menu;
            menu.addSectionHeader (which == 0 ? "Bank" : "Variation");
            addLinkItems (menu, t);
            juce::Component::SafePointer<BannerRow> safe (this);
            juce::Component* near = which == 0 ? &bankKnob : &varKnob;
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (near),
                                [safe, t, near] (int r) { if (safe != nullptr && r != 0) safe->linkChosen (t, r, *near); });
        }

        void showMacroMenu (int m)
        {
            auto& eng = proc.engine();
            auto& gr = eng.graph (ch);
            auto& mac = eng.channel (ch).macros[(size_t) m];
            const int cur = mac.dest.load();

            juce::PopupMenu menu;
            menu.addSectionHeader ("MACRO " + juce::String (m + 1) + ":  " + (macros::isLinked (gr, cur) ? macros::longName (gr, cur) : juce::String ("no destination")));
            addLinkItems (menu, link::Target::macro (ch, m));
            menu.addSeparator();

            // Every setting on the channel: its own, then each node's
            auto codes = std::make_shared<std::vector<int>>();
            auto add = [&codes, cur] (juce::PopupMenu& into, int code, const juce::String& name)
            {
                codes->push_back (code);
                into.addItem (10000 + (int) codes->size() - 1, name, true, code == cur);
            };
            juce::PopupMenu dest, chanMenu;
            for (int p : { (int) cpTranspose, (int) cpOctave, (int) cpVolume, (int) cpPan })
                add (chanMenu, macros::chanCode (p), chanParamInfo (p).name);
            dest.addSubMenu ("Channel", chanMenu, true, nullptr, macros::kindOf (cur) == macros::dkChan);

            std::vector<graph::Node*> nodes;
            for (auto& n : gr.nodeList()) if (n->numParams() > 0) nodes.push_back (n.get());
            std::sort (nodes.begin(), nodes.end(), [] (auto* a, auto* b) { return a->pos.x < b->pos.x; });
            for (auto* n : nodes)
            {
                juce::PopupMenu sub;
                const int np = juce::jmin (n->numParams(), 512);
                const bool hit = macros::kindOf (cur) == macros::dkNode && macros::uidOf (cur) == n->uid;
                for (int p = 0; p < np; ++p)
                {
                    if (n->isMask (p)) continue;
                    if (np > 48 && p % 48 == 0 && p > 0) sub.addColumnBreak();
                    add (sub, macros::nodeCode (n->uid, p), n->paramName (p));
                }
                dest.addSubMenu (n->title(), sub, true, nullptr, hit);
            }
            menu.addSubMenu ("Destination", dest);
            const int def = macros::defaultsFor (gr)[(size_t) m];
            menu.addItem (3, "Default destination (" + (def != 0 ? macros::shortName (gr, def) : juce::String ("none")) + ")", def != cur);
            menu.addItem (5, "No destination", cur != 0);
            menu.addItem (4, "Back to rest");

            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&macroKnobs[(size_t) m]),
                [safe = juce::Component::SafePointer<BannerRow> (this), m, def, codes] (int result)
                {
                    if (safe == nullptr || result == 0) return;
                    auto& e = safe->proc.engine();
                    auto& mc = e.channel (safe->ch).macros[(size_t) m];
                    if (result >= 100 && result <= 102)
                    {
                        safe->linkChosen (link::Target::macro (safe->ch, m), result, safe->macroKnobs[(size_t) m]);
                        return;
                    }
                    // A new destination: the macro takes its control's place as it is (nothing moves)
                    if (result == 3)      mc.dest.store (def);
                    else if (result == 5) { mc.value.store (e.macroNorm (safe->ch, m)); mc.dest.store (0); }
                    else if (result == 4) e.setMacroNorm (safe->ch, m, safe->restOf (mc.dest.load()));
                    else if (result >= 10000 && result - 10000 < (int) codes->size())
                        mc.dest.store ((*codes)[(size_t) (result - 10000)]);
                    safe->refresh();
                });
        }

        ZodiakProcessor& proc;
        ZodiakEditor&    editor;
        const int        ch;
        const ChannelColours& colours;

        PatternKnob bankKnob { 4, "BANK", colours }, varKnob { 8, "VAR", colours };
        std::array<ZodiakKnob, kNumMacros> macroKnobs;
        ChannelButton rollBtn, editBtn, instBtn;
        MuteSoloButton muteBtn, soloBtn;

        juce::String shownName, shownKind;
        bool     shownActive = false;
        uint32_t lastNotes = 0;
    };

    //==========================================================================
    //  A window over the banner, in its channel's colours: its title bar, its
    //  tabs, X to close
    //==========================================================================
    class PopupFrame : public juce::Component
    {
    public:
        PopupFrame (ZodiakEditor& e, int channel, const juce::String& titleText)
            : editor (e), ch (channel), colours (channelColours (channel)), title (titleText)
        {
            closeBtn.setColour (juce::TextButton::buttonColourId, colours.darkText);
            closeBtn.setColour (juce::TextButton::textColourOffId, colours.lightText);
            closeBtn.setTooltip ("Close");
            closeBtn.onClick = [this] { editor.closePopup(); };
            addAndMakeVisible (closeBtn);
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (b.translated (0.0f, 6.0f), 16.0f);
            g.setColour (win::bg);
            g.fillRoundedRectangle (b, 14.0f);
            auto bar = b.removeFromTop ((float) kBarH);
            g.setColour (colours.dark);
            g.fillRoundedRectangle (bar, 14.0f);
            g.fillRect (bar.withTrimmedTop (bar.getHeight() * 0.5f));
            g.setColour (colours.lightText);
            g.setFont (font (22.0f, true));
            g.drawText (title, bar.reduced (24.0f, 0.0f), juce::Justification::centredLeft, false);
            g.setColour (colours.edge);
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 14.0f, 2.0f);
        }

        void resized() override
        {
            closeBtn.setBounds (getWidth() - 70, 10, 56, kBarH - 20);
        }

    protected:
        static constexpr int kBarH = 60;
        juce::Rectangle<int> body() const   { return getLocalBounds().withTrimmedTop (kBarH).reduced (2); }

        ZodiakEditor& editor;
        const int ch;
        const ChannelColours& colours;
        juce::String title;
        juce::TextButton closeBtn { "X" };
    };

    //==========================================================================
    //  GRAPH: the channel's graph (ZodiakGraphUi.h): its nodes and cables, and
    //  the chosen node's settings under them
    //==========================================================================
    class GraphPopup final : public PopupFrame
    {
    public:
        GraphPopup (ZodiakProcessor& p, ZodiakEditor& e, int channel)
            : PopupFrame (e, channel, "Ch " + juce::String (channel + 1) + "   Graph   " + instrumentName (p, channel)),
              graphView (p, e.windows(), channel)
        {
            addAndMakeVisible (graphView);
        }

        void resized() override
        {
            PopupFrame::resized();
            graphView.setBounds (body().reduced (8));
        }

    private:
        GraphEditor graphView;
    };

    //==========================================================================
    //  ROLL: the pattern editor (ZodiakRoll.h)
    //==========================================================================
    class RollPopup final : public PopupFrame
    {
    public:
        RollPopup (ZodiakProcessor& p, ZodiakEditor& e, int channel)
            : PopupFrame (e, channel, {}), proc (p), roll (p, channel)
        {
            roll.onPatternChanged = [this] (const juce::String& name)
            {
                title = "Ch " + juce::String (ch + 1) + "   Roll   " + name + "   " + instrumentKind (proc, ch) + "   " + instrumentName (proc, ch);
                repaint();
            };
            roll.onPatternChanged (seq::patternName (seq::patternIndex (p.bank (channel), p.variation (channel))));
            addAndMakeVisible (roll);

            // The Roll's look, by the close button
            for (auto* b : { &brightBtn, &darkBtn })
            {
                b->setClickingTogglesState (true);
                b->setRadioGroupId (4711);
                b->setColour (juce::TextButton::buttonColourId, colours.darkText);
                b->setColour (juce::TextButton::buttonOnColourId, colours.light);
                b->setColour (juce::TextButton::textColourOffId, colours.lightText);
                b->setColour (juce::TextButton::textColourOnId, colours.darkText);
                addAndMakeVisible (*b);
            }
            brightBtn.setTooltip ("The Roll's bright look");
            darkBtn.setTooltip ("The Roll's dark look");
            (roll.isDark() ? darkBtn : brightBtn).setToggleState (true, juce::dontSendNotification);
            brightBtn.onClick = [this] { if (brightBtn.getToggleState()) roll.setDark (false); };
            darkBtn.onClick   = [this] { if (darkBtn.getToggleState())   roll.setDark (true); };
        }

        void resized() override
        {
            PopupFrame::resized();
            roll.setBounds (body().reduced (10, 8));
            darkBtn.setBounds (getWidth() - 70 - 12 - 90, 10, 90, kBarH - 20);
            brightBtn.setBounds (darkBtn.getX() - 6 - 90, 10, 90, kBarH - 20);
        }

    private:
        ZodiakProcessor& proc;
        RollEditor roll;
        juce::TextButton brightBtn { "Bright" }, darkBtn { "Dark" };
    };

    //==========================================================================
    //  TRANSPORT: rewind, play, stop, where the song is, its tempo, and FOLLOW
    //  DAW (off: Zodiak plays from its own PLAY, at its own tempo; on: it also
    //  plays with the DAW's play and stop, at the DAW's tempo)
    //==========================================================================
    class TransportButton final : public juce::Button
    {
    public:
        enum Kind { rewind, play, stop };

        explicit TransportButton (Kind k) : juce::Button (k == rewind ? "Rewind" : k == play ? "Play" : "Stop"), kind (k) {}

        void setLit (bool on)   { if (on != lit) { lit = on; repaint(); } }

        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            auto b = getLocalBounds().toFloat().reduced (2.0f);
            const auto lamp = kind == play ? juce::Colour (0xFF3DDC84) : juce::Colour (0xFFE8A33D);
            g.setColour (down ? juce::Colour (0xFF1D2024) : over ? juce::Colour (0xFF34383F) : juce::Colour (0xFF2A2E34));
            g.fillRoundedRectangle (b, 8.0f);
            g.setColour (lit ? lamp : juce::Colour (0xFF4A4F57));
            g.drawRoundedRectangle (b.reduced (0.8f), 8.0f, lit ? 2.6f : 1.6f);

            const auto c = b.getCentre();
            const float s = b.getHeight() * 0.24f;
            juce::Path p;
            if (kind == play)
                p.addTriangle (c.x - s * 0.8f, c.y - s, c.x - s * 0.8f, c.y + s, c.x + s * 1.0f, c.y);
            else if (kind == stop)
                p.addRectangle (c.x - s * 0.85f, c.y - s * 0.85f, s * 1.7f, s * 1.7f);
            else
            {
                p.addRectangle (c.x - s * 1.25f, c.y - s, s * 0.32f, s * 2.0f);
                p.addTriangle (c.x + s * 1.2f, c.y - s, c.x + s * 1.2f, c.y + s, c.x - s * 0.75f, c.y);
            }
            g.setColour (lit ? lamp : win::text);
            g.fillPath (p);
        }

    private:
        const Kind kind;
        bool lit = false;
    };

    // Its tempo: drag up/down (Shift: finer), the wheel, or double-click to type
    class TempoBox final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit TempoBox (ZodiakProcessor& p) : proc (p)
        {
            setTooltip ("Zodiak's tempo: drag up or down (Shift: finer), or double-click to type it. "
                        "With FOLLOW DAW on it plays at the DAW's.");
        }

        void refresh()
        {
            auto& eng = proc.engine();
            const bool own = ! eng.followsDaw();
            const double t = own ? (double) eng.master (mpTempo).load() : eng.tempo();
            if (std::abs (t - shownTempo) > 0.004 || own != shownOwn)
            {
                shownTempo = t;
                shownOwn = own;
                repaint();
            }
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (win::well);
            g.fillRoundedRectangle (b, 8.0f);
            g.setColour (juce::Colour (0xFF4A4F57));
            g.drawRoundedRectangle (b.reduced (0.8f), 8.0f, 1.6f);

            auto unit = b.removeFromRight (64.0f);
            g.setColour (shownOwn ? win::led : win::dim);
            g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 36.0f, juce::Font::bold)));
            g.drawText (juce::String (shownTempo, 2), b.withTrimmedLeft (8.0f), juce::Justification::centredRight, false);
            g.setColour (win::dim);
            g.setFont (font (16.0f, true));
            g.drawText (shownOwn ? "BPM" : "DAW", unit, juce::Justification::centred, false);
        }

        void mouseDown (const juce::MouseEvent&) override   { dragStart = proc.engine().master (mpTempo).load(); }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (proc.engine().followsDaw()) return;
            const float perPixel = e.mods.isShiftDown() ? 0.01f : 0.2f;
            set (dragStart - (float) e.getDistanceFromDragStartY() * perPixel);
        }

        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            if (proc.engine().followsDaw()) return;
            const float step = e.mods.isShiftDown() ? 0.1f : 1.0f;
            set (proc.engine().master (mpTempo).load() + (w.deltaY > 0.0f ? step : -step));
        }

        void mouseDoubleClick (const juce::MouseEvent&) override
        {
            if (proc.engine().followsDaw()) return;
            typing = std::make_unique<juce::TextEditor>();
            typing->setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 32.0f, juce::Font::bold)));
            typing->setJustification (juce::Justification::centred);
            typing->setInputRestrictions (7, "0123456789.");
            typing->setText (juce::String (proc.engine().master (mpTempo).load(), 2), false);
            typing->onReturnKey = [this] { set (typing->getText().getFloatValue()); endTyping(); };
            typing->onEscapeKey = [this] { endTyping(); };
            typing->onFocusLost = [this] { endTyping(); };
            addAndMakeVisible (*typing);
            typing->setBounds (getLocalBounds().reduced (4));
            typing->selectAll();
            typing->grabKeyboardFocus();
        }

    private:
        void set (float bpm)
        {
            bpm = std::round (juce::jlimit (40.0f, 240.0f, bpm) * 100.0f) / 100.0f;
            proc.engine().master (mpTempo).store (bpm);
            refresh();
        }

        void endTyping()
        {
            // Not inside the editor's own callback
            juce::Component::SafePointer<TempoBox> safe (this);
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->typing.reset(); });
        }

        ZodiakProcessor& proc;
        std::unique_ptr<juce::TextEditor> typing;
        double shownTempo = -1.0;
        bool   shownOwn = true;
        float  dragStart = 120.0f;
    };

    // FOLLOW DAW: its lamp and its name
    class FollowButton final : public juce::Button
    {
    public:
        FollowButton() : juce::Button ("Follow DAW")
        {
            setClickingTogglesState (true);
            setTooltip ("FOLLOW DAW on: Zodiak plays when the DAW plays, from where the DAW is, at its tempo "
                        "(its own Play still runs it while the DAW is stopped).  Off: it plays from its own Play, at its own tempo.");
        }

        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            auto b = getLocalBounds().toFloat().reduced (2.0f);
            const bool on = getToggleState();
            g.setColour (down ? juce::Colour (0xFF1D2024) : over ? juce::Colour (0xFF34383F) : juce::Colour (0xFF2A2E34));
            g.fillRoundedRectangle (b, 8.0f);
            g.setColour (on ? win::led : juce::Colour (0xFF4A4F57));
            g.drawRoundedRectangle (b.reduced (0.8f), 8.0f, on ? 2.4f : 1.6f);

            auto lampArea = b.removeFromLeft (52.0f);
            const auto c = lampArea.getCentre();
            if (on)
            {
                g.setColour (win::led.withAlpha (0.3f));
                g.fillEllipse (c.x - 13.0f, c.y - 13.0f, 26.0f, 26.0f);
            }
            g.setColour (on ? win::led : juce::Colour (0xFF3A3F45));
            g.fillEllipse (c.x - 9.0f, c.y - 9.0f, 18.0f, 18.0f);

            g.setColour (on ? win::text : win::label);
            g.setFont (font (21.0f, true));
            g.drawText ("FOLLOW DAW", b.withTrimmedRight (10.0f), juce::Justification::centred, false);
        }
    };

    class TransportPanel final : public juce::Component
    {
    public:
        explicit TransportPanel (ZodiakProcessor& p) : proc (p), tempo (p)
        {
            rewindBtn.setTooltip ("Rewind: back to bar 1");
            playBtn.setTooltip ("Play (Zodiak's own clock)");
            stopBtn.setTooltip ("Stop");
            rewindBtn.onClick = [this] { proc.engine().rewind(); };
            playBtn.onClick   = [this] { proc.engine().play(); };
            stopBtn.onClick   = [this] { proc.engine().stop(); };
            followBtn.onClick = [this] { proc.engine().master (mpFollow).store (followBtn.getToggleState() ? 1.0f : 0.0f); refresh(); };
            for (auto* c : std::initializer_list<juce::Component*> { &rewindBtn, &playBtn, &stopBtn, &tempo, &followBtn })
                addAndMakeVisible (*c);

            // MIDI LINK: PLAY plays and stops (a pad: each press), STOP stops, REWIND rewinds
            linkTarget (rewindBtn, proc, link::Target::transport (link::tRewind));
            linkTarget (playBtn, proc, link::Target::transport (link::tPlayStop));
            linkTarget (stopBtn, proc, link::Target::transport (link::tStop));
            linkTarget (tempo, proc, link::Target::master (mpTempo));
            linkTarget (followBtn, proc, link::Target::master (mpFollow));
            refresh();
        }

        void refresh()
        {
            auto& eng = proc.engine();
            const bool playing = eng.isPlaying();
            playBtn.setLit (playing);
            followBtn.setToggleState (eng.followsDaw(), juce::dontSendNotification);
            tempo.refresh();

            const auto text = positionText (playing || eng.position() > 0.0 ? eng.position() : 0.0, eng.beatsPerBar());
            if (text != shownPosition)
            {
                shownPosition = text;
                repaint (lcd);
            }
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (win::row);
            g.fillRoundedRectangle (b, 12.0f);

            g.setColour (win::dim);
            g.setFont (font (20.0f, true));
            g.drawText ("Transport", juce::Rectangle<int> (kChX + 4, 0, 200, getHeight()), juce::Justification::centredLeft, false);

            // The captions over the parts
            g.setFont (font (15.0f, true));
            auto caption = [&] (juce::Rectangle<int> r, const juce::String& t)
            {
                g.drawText (t, r.withY (6).withHeight (20), juce::Justification::centred, false);
            };
            caption (rewindBtn.getBounds(), "REWIND");
            caption (playBtn.getBounds(), "PLAY");
            caption (stopBtn.getBounds(), "STOP");
            caption (lcd, "BAR . BEAT . STEP");
            caption (tempo.getBounds(), "TEMPO");
            caption (followBtn.getBounds(), "PLAY / STOP");

            // The song's place
            const auto l = lcd.toFloat();
            g.setColour (win::well);
            g.fillRoundedRectangle (l, 8.0f);
            g.setColour (juce::Colour (0xFF4A4F57));
            g.drawRoundedRectangle (l.reduced (0.8f), 8.0f, 1.6f);
            g.setColour (win::led);
            g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 36.0f, juce::Font::bold)));
            g.drawText (shownPosition, l, juce::Justification::centred, false);
        }

        void resized() override
        {
            const int top = 30, h = 62, gap = 44;
            const int total = 3 * 92 + 2 * 10 + gap + 330 + gap + 250 + gap + 250;
            int x = (getWidth() - total) / 2;
            for (auto* btn : { &rewindBtn, &playBtn, &stopBtn })
            {
                btn->setBounds (x, top, 92, h);
                x += 102;
            }
            x += gap - 10;
            lcd = { x, top, 330, h };
            x += 330 + gap;
            tempo.setBounds (x, top, 250, h);
            x += 250 + gap;
            followBtn.setBounds (x, top, 250, h);
        }

    private:
        static juce::String positionText (double ppq, int beatsPerBar)
        {
            ppq = std::max (0.0, ppq);
            const double bar = (double) juce::jmax (1, beatsPerBar);
            const int barNo  = (int) std::floor (ppq / bar) + 1;
            const double in  = std::fmod (ppq, bar);
            const int beat   = (int) std::floor (in) + 1;
            const int step   = (int) std::floor (in * 4.0) + 1;
            return juce::String (barNo).paddedLeft ('0', 3) + " . " + juce::String (beat) + " . " + juce::String (step).paddedLeft ('0', 2);
        }

        ZodiakProcessor& proc;
        TransportButton rewindBtn { TransportButton::rewind }, playBtn { TransportButton::play }, stopBtn { TransportButton::stop };
        TempoBox tempo;
        FollowButton followBtn;
        juce::Rectangle<int> lcd;
        juce::String shownPosition;
    };
}

//==============================================================================
ZodiakEditor::ZodiakEditor (ZodiakProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    addAndMakeVisible (content);

    styleLabel (logoLbl, 36.0f, win::text).setText ("ZODIAK", juce::dontSendNotification);
    styleLabel (subLbl, 13.0f, win::dim, false).setText ("FANAN GROOVE BOX", juce::dontSendNotification);
    styleLabel (setLbl, 15.0f, win::dim, false).setText ("Set:", juce::dontSendNotification);
    styleLabel (setName, 19.0f, win::text);
    styleLabel (sizeLbl, 15.0f, win::dim, false).setText ("UI size", juce::dontSendNotification);
    sizeLbl.setJustificationType (juce::Justification::centredRight);
    for (auto* l : { &logoLbl, &subLbl, &setLbl, &setName, &sizeLbl }) content.addAndMakeVisible (*l);

    for (auto* b : { &newBtn, &loadBtn, &saveBtn, &saveAsBtn })
    {
        styleHeaderButton (*b);
        content.addAndMakeVisible (*b);
    }
    newBtn.onClick    = [this] { newSet(); };
    loadBtn.onClick   = [this] { loadSet(); };
    saveBtn.onClick   = [this] { saveSet(); };
    saveAsBtn.onClick = [this] { saveSetAs(); };

    for (int pct = 30, id = 1; pct <= 100; pct += 5, ++id) sizeBox.addItem (juce::String (pct) + "%", id);
    sizeBox.setSelectedId (juce::jlimit (1, 15, (int) std::lround ((proc.getUiScale() * 100.0f - 30.0f) / 5.0f) + 1),
                           juce::dontSendNotification);
    sizeBox.onChange = [this]
    {
        proc.setUiScale (0.30f + 0.05f * (float) (sizeBox.getSelectedId() - 1));
        applyScale();
    };
    content.addAndMakeVisible (sizeBox);

    // LOW VELOCITY RESPONSE (Grex's): lifts the quiet notes, OFF .. +100 %
    styleLabel (lowVelLbl, 15.0f, win::dim, false).setText ("Low velocity response", juce::dontSendNotification);
    lowVelLbl.setJustificationType (juce::Justification::centredRight);
    styleLabel (lowVelValue, 17.0f, win::led, true);
    lowVelValue.setJustificationType (juce::Justification::centredLeft);
    lowVelSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    lowVelSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    lowVelSlider.setRange (0.0, 100.0, 1.0);
    lowVelSlider.setDoubleClickReturnValue (true, 0.0);
    lowVelSlider.setColour (juce::Slider::thumbColourId, win::led);
    lowVelSlider.setColour (juce::Slider::trackColourId, win::led.withAlpha (0.7f));
    lowVelSlider.setColour (juce::Slider::backgroundColourId, win::well);
    lowVelSlider.setTooltip ("Low velocity response: makes the quiet notes louder, the quietest the most "
                             "(on every channel; the loudest stay as they are).  OFF: as played.  Double-click: OFF.");
    lowVelSlider.onValueChange = [this]
    {
        proc.engine().master (mpLowVel).store ((float) lowVelSlider.getValue());
        showLowVel();
    };
    linkTarget (lowVelSlider, proc, link::Target::master (mpLowVel));
    for (auto* c : std::initializer_list<juce::Component*> { &lowVelLbl, &lowVelSlider, &lowVelValue })
        content.addAndMakeVisible (*c);
    showLowVel();

    modeBar = std::make_unique<ModeBar> (proc);
    modeBar->onPage     = [this] (bool mixerPage) { showMixer (mixerPage); };
    content.addAndMakeVisible (*modeBar);

    steps = std::make_unique<StepLeds>();
    content.addAndMakeVisible (*steps);

    for (int ch = 0; ch < kNumChannels; ++ch)
    {
        rows[(size_t) ch] = std::make_unique<BannerRow> (proc, *this, ch);
        content.addAndMakeVisible (*rows[(size_t) ch]);
    }

    transport = std::make_unique<TransportPanel> (proc);
    content.addAndMakeVisible (*transport);

    mixer = std::make_unique<MixerPanel> (proc, hub, [this] (int ch, juce::Component*) { showMixer (false); openGraph (ch); });
    content.addAndMakeVisible (*mixer);

    showMixer (proc.mixerShown);
    applyScale();
    refreshAll();
    shownGeneration = proc.setGeneration();
    startTimerHz (30);
}

ZodiakEditor::~ZodiakEditor()
{
    stopTimer();
    popup.reset();
    hub.closeAll();
}

void ZodiakEditor::paint (juce::Graphics& g)
{
    g.fillAll (win::bg);
}

void ZodiakEditor::applyScale()
{
    shownScale = juce::jlimit (0.3f, 1.0f, proc.getUiScale());
    content.setTransform (juce::AffineTransform::scale (shownScale));
    setSize (juce::roundToInt ((float) kWidth * shownScale), juce::roundToInt ((float) kHeight * shownScale));
}

void ZodiakEditor::resized() {}

void ZodiakEditor::layoutContent()
{
    content.setBounds (0, 0, kWidth, kHeight);

    // The header
    logoLbl.setBounds (24, 8, 170, 40);
    subLbl.setBounds (26, 46, 200, 18);
    setLbl.setBounds (250, 20, 40, 32);
    setName.setBounds (292, 20, 380, 32);
    int x = 690;
    for (auto* b : { &newBtn, &loadBtn, &saveBtn, &saveAsBtn })
    {
        b->setBounds (x, 18, 112, 38);
        x += 122;
    }
    lowVelLbl.setBounds (1230, 18, 200, 38);
    lowVelSlider.setBounds (1440, 18, 300, 38);
    lowVelValue.setBounds (1750, 18, 90, 38);
    sizeLbl.setBounds (kWidth - 290, 18, 110, 38);
    sizeBox.setBounds (kWidth - 170, 18, 146, 38);

    modeBar->setBounds (12, kModeY, kWidth - 24, kModeH);

    // The page: PLAY (the steps and the banner) or MIXER, on the same room
    steps->setBounds (0, kStepsY, kWidth, kStepsH);
    for (int ch = 0; ch < kNumChannels; ++ch)
        rows[(size_t) ch]->setBounds (12, kRowsY + ch * kRowPitch, kWidth - 24, kRowH);
    mixer->setBounds (0, kStepsY, kWidth, kBannerEnd - kStepsY);

    transport->setBounds (12, kTransY, kWidth - 24, kTransH);

    if (popup != nullptr)
        popup->setBounds (12, kStepsY, kWidth - 24, kBannerEnd - kStepsY);
}

void ZodiakEditor::refreshAll()
{
    auto name = proc.setName();
    if (proc.setFile() != juce::File()) name << "   (" << proc.setFile().getFileName() << ")";
    setName.setText (name, juce::dontSendNotification);
    saveBtn.setEnabled (proc.setFile() != juce::File());
    for (auto& r : rows) r->refresh();
    mixer->rebuild();
    modeBar->setSet (proc.setName());
    showLowVel();
}

void ZodiakEditor::showMixer (bool mixerPage)
{
    proc.mixerShown = mixerPage;
    if (mixerPage) popup.reset();           // ROLL and EDIT belong to the banner
    steps->setVisible (! mixerPage);
    for (auto& r : rows) r->setVisible (! mixerPage);
    mixer->setVisible (mixerPage);
    modeBar->setPage (mixerPage);
    layoutContent();
}

// The slider and its value from the setting (a set loaded, or the slider moved)
void ZodiakEditor::showLowVel()
{
    const int amount = juce::roundToInt (proc.engine().master (mpLowVel).load());
    lowVelSlider.setValue (amount, juce::dontSendNotification);
    lowVelValue.setText (amount <= 0 ? juce::String ("OFF") : "+" + juce::String (amount) + " %", juce::dontSendNotification);
}

void ZodiakEditor::timerCallback()
{
    if (std::abs (proc.getUiScale() - shownScale) > 0.001f)
    {
        sizeBox.setSelectedId (juce::jlimit (1, 15, (int) std::lround ((proc.getUiScale() * 100.0f - 30.0f) / 5.0f) + 1),
                               juce::dontSendNotification);
        applyScale();
    }

    if (proc.setGeneration() != shownGeneration)
    {
        shownGeneration = proc.setGeneration();
        refreshAll();
    }

    steps->setStep (proc.engine().songStep());
    transport->refresh();
    if (! lowVelSlider.isMouseButtonDown() && juce::roundToInt (proc.engine().master (mpLowVel).load()) != juce::roundToInt (lowVelSlider.getValue()))
        showLowVel();                   // moved by its MIDI link
    if (popup == nullptr)
        for (auto& r : rows) r->refresh();

    // Plugins a set held that aren't on this computer: said once
    if (! proc.isRestoringPlugins())
    {
        const auto missing = proc.takeMissingPlugins();
        if (! missing.isEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Plugins not found",
                                                    "The set holds plugins that didn't load:\n\n" + missing.joinIntoString ("\n"),
                                                    "OK", this);
    }
}

//==============================================================================
void ZodiakEditor::openGraph (int ch)
{
    popup = std::make_unique<GraphPopup> (proc, *this, ch);
    content.addAndMakeVisible (*popup);
    layoutContent();
}

void ZodiakEditor::openInstrument (int ch, juce::Component* near)
{
    auto& g = proc.engine().graph (ch);
    auto* node = g.firstInstrument();
    if (node == nullptr)
    {
        // No VSTi node (the user took it out): one more, wired to the Output, and loaded
        node = g.addNode (graph::ntInstrument, { 40.0f, 420.0f }, true);
        if (node == nullptr) return;
        if (auto* o = g.output()) g.connect (node->uid, o->uid, graph::kPinMain);
    }
    openNodePlugin (proc, hub, ch, node->uid, near);
}

void ZodiakEditor::openRoll (int ch)
{
    popup = std::make_unique<RollPopup> (proc, *this, ch);
    content.addAndMakeVisible (*popup);
    layoutContent();
}

void ZodiakEditor::closePopup()
{
    // Not while its button's click is still running: on the next message
    juce::Component::SafePointer<ZodiakEditor> safe (this);
    juce::MessageManager::callAsync ([safe]
    {
        if (safe == nullptr) return;
        safe->popup.reset();
        for (auto& r : safe->rows) r->refresh();
    });
}

//==============================================================================
void ZodiakEditor::showFailure (const juce::String& title, const juce::Result& r)
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, title, r.getErrorMessage(), "OK", this);
}

void ZodiakEditor::newSet()
{
    popup.reset();
    hub.closeAll();
    proc.newSet();
    refreshAll();
}

void ZodiakEditor::loadSet()
{
    const auto folder = sets::folder();
    folder.createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Load a Zodiak set", folder, juce::String ("*") + sets::kExtension);
    juce::Component::SafePointer<ZodiakEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe] (const juce::FileChooser& fc)
        {
            if (safe == nullptr) return;
            const auto file = fc.getResult();
            if (! file.existsAsFile()) return;
            safe->popup.reset();
            safe->hub.closeAll();
            const auto r = safe->proc.loadSet (file);
            if (r.failed()) safe->showFailure ("Set not loaded", r);
            safe->refreshAll();
        });
}

void ZodiakEditor::saveSet()
{
    const auto r = proc.saveSet();
    if (r.failed()) showFailure ("Set NOT saved", r);
    refreshAll();
}

void ZodiakEditor::saveSetAs()
{
    const auto folder = sets::folder();
    folder.createDirectory();
    auto start = folder.getChildFile (juce::File::createLegalFileName (proc.setName()) + sets::kExtension);
    chooser = std::make_unique<juce::FileChooser> ("Save the set as", start, juce::String ("*") + sets::kExtension);
    juce::Component::SafePointer<ZodiakEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe] (const juce::FileChooser& fc)
        {
            if (safe == nullptr) return;
            auto file = fc.getResult();
            if (file == juce::File()) return;
            file = file.withFileExtension (sets::kExtension);
            const auto r = safe->proc.saveSetAs (file);
            if (r.failed()) safe->showFailure ("Set NOT saved", r);
            safe->refreshAll();
        });
}
