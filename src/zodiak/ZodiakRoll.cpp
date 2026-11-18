// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakRoll.cpp
#include "ZodiakRoll.h"

#include <limits>

namespace zodiak::ui
{
    namespace
    {
        constexpr int kToolbarH = 58, kRulerH = 44, kLoopLaneH = 16, kLaneH = 150, kGap = 6;
        constexpr int kSynthRowH = 24;

        const juce::Colour playColour  { 0xFFFFC21A };

        // Where the last MIDI file was imported from or exported to (while the DAW runs)
        juce::File& midiFolder()
        {
            static juce::File folder;
            return folder;
        }
        const juce::Colour focusRow    { 0x40FF2A2A };        // the selected note's row: half transparent red

        bool isBlack (int key) noexcept
        {
            const int n = key % 12;
            return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
        }

        void styleToolButton (juce::TextButton& b, const ChannelColours& c)
        {
            b.setColour (juce::TextButton::buttonColourId, c.light);
            b.setColour (juce::TextButton::buttonOnColourId, c.dark);
            b.setColour (juce::TextButton::textColourOffId, c.darkText);
            b.setColour (juce::TextButton::textColourOnId, c.lightText);
        }

        void styleCombo (juce::ComboBox& b)
        {
            b.setColour (juce::ComboBox::backgroundColourId, win::well);
            b.setColour (juce::ComboBox::textColourId, win::text);
            b.setColour (juce::ComboBox::outlineColourId, win::itemLine);
            b.setColour (juce::ComboBox::arrowColourId, win::dim);
        }

        // The notes cut, copied and pasted (start times from the first one)
        std::vector<seq::Note>& noteClipboard()
        {
            static std::vector<seq::Note> clip;
            return clip;
        }
    }

    //==========================================================================
    const RollEditor::Theme& RollEditor::theme() const noexcept
    {
        static const Theme bright {
            juce::Colour (0xFFF1F2F4), juce::Colour (0xFFE1E3E7), juce::Colour (0xFFCDD0D5), juce::Colour (0xFF9EA3AA),
            juce::Colour (0xFF7E838B), juce::Colour (0xFFB2B6BC), juce::Colour (0xFFD3D6DA), juce::Colour (0x1C000000),
            juce::Colour (0xFFFFFFFF), juce::Colour (0xFF2A2C31), juce::Colour (0xFF3C3F45), juce::Colour (0xFFDADDE1),
            juce::Colour (0xFF3A3D42), juce::Colour (0xFFE6E8EB), juce::Colour (0xFFB9BDC3), juce::Colour (0xFF16171A),
            juce::Colour (0xFFE9EBEE), juce::Colour (0xFF2C2E33) };
        static const Theme darkTheme {
            juce::Colour (0xFF24262A), juce::Colour (0xFF1B1D20), juce::Colour (0xFF15161A), juce::Colour (0xFF3F4147),
            juce::Colour (0xFF6A717B), juce::Colour (0xFF3D424A), juce::Colour (0xFF2C3036), juce::Colour (0x59000000),
            juce::Colour (0xFFD9DADC), juce::Colour (0xFF2A2D32), juce::Colour (0xFF33363C), juce::Colour (0xFF15171A),
            juce::Colour (0xFFAAB0B8), juce::Colour (0xFF15171A), juce::Colour (0xFF2C3036), juce::Colour (0xFFFFFFFF),
            juce::Colour (0xFF2A2D32), juce::Colour (0xFFE6E8EB) };
        return dark ? darkTheme : bright;
    }

    //==========================================================================
    //  RULER: the locators, bars and beats, and where the pattern plays
    //==========================================================================
    class RollEditor::Ruler final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit Ruler (RollEditor& o) : owner (o)
        {
            setTooltip ("Locators: drag their ends or the bar between them (1/32 steps); double-click: the whole pattern");
        }

        void paint (juce::Graphics& g) override
        {
            const auto& geo = owner.geo;
            const auto& t = owner.theme();
            const auto& c = owner.chColours;

            g.setColour (t.ruler);
            g.fillRect (getLocalBounds());
            g.setColour (t.ruler.darker (0.08f));
            g.fillRect (geo.keyW, 0, geo.width - geo.keyW, kLoopLaneH);

            // The locators
            int ls, le;
            owner.pat.loopRange (geo.lengthTicks, ls, le);
            const float x0 = geo.xOf (ls), x1 = geo.xOf (le);
            g.setColour (c.light);
            g.fillRect (x0, 2.0f, juce::jmax (2.0f, x1 - x0), (float) kLoopLaneH - 4.0f);
            g.setColour (c.edge);
            g.fillRect (x0, 0.0f, 3.0f, (float) kLoopLaneH);
            g.fillRect (x1 - 3.0f, 0.0f, 3.0f, (float) kLoopLaneH);

            // Bars and beats
            g.setFont (font (17.0f, true));
            for (int tick = 0; tick < geo.lengthTicks; tick += geo.beatTicks)
            {
                const float x = geo.xOf (tick);
                const bool bar = tick % geo.barTicks == 0;
                g.setColour (bar ? t.bar : t.beat);
                g.drawVerticalLine ((int) x, bar ? (float) kLoopLaneH : (float) getHeight() * 0.7f, (float) getHeight());
                if (bar)
                {
                    g.setColour (t.rulerText);
                    g.drawText (juce::String (tick / geo.barTicks + 1), (int) x + 6, kLoopLaneH, 60, getHeight() - kLoopLaneH,
                                juce::Justification::centredLeft, false);
                }
            }

            g.setColour (t.rulerText);
            g.setFont (font (14.0f, true));
            g.drawText ("LOCATORS", 8, 0, geo.keyW - 12, kLoopLaneH, juce::Justification::centredLeft, false);

            if (const int play = owner.shownTick; play >= 0)
            {
                const float x = geo.xOf (play);
                juce::Path tri;
                tri.addTriangle (x - 8.0f, (float) kLoopLaneH + 2.0f, x + 8.0f, (float) kLoopLaneH + 2.0f, x, (float) kLoopLaneH + 16.0f);
                g.setColour (playColour);
                g.fillPath (tri);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const auto& geo = owner.geo;
            int ls, le;
            owner.pat.loopRange (geo.lengthTicks, ls, le);
            const float x0 = geo.xOf (ls), x1 = geo.xOf (le);
            const float x = (float) e.x;

            mode = std::abs (x - x0) <= 10.0f ? Mode::start
                 : std::abs (x - x1) <= 10.0f ? Mode::end
                 : (x > x0 && x < x1)          ? Mode::both
                                               : Mode::none;
            grabTick = geo.tickAt (x);
            baseStart = ls;
            baseEnd = le;
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (mode == Mode::none) return;
            const auto& geo = owner.geo;
            const int step = seq::Pattern::kLocatorStep;
            const int len = geo.lengthTicks;
            const int d = juce::roundToInt ((float) (geo.tickAt ((float) e.x) - grabTick) / (float) step) * step;

            int ls = baseStart, le = baseEnd;
            if (mode == Mode::start)      ls = juce::jlimit (0, le - step, baseStart + d);
            else if (mode == Mode::end)   le = juce::jlimit (ls + step, len, baseEnd + d);
            else
            {
                const int dd = juce::jlimit (-baseStart, len - baseEnd, d);
                ls = baseStart + dd;
                le = baseEnd + dd;
            }
            setLoop (ls, le);
        }

        void mouseDoubleClick (const juce::MouseEvent&) override   { setLoop (0, owner.geo.lengthTicks); }

    private:
        enum class Mode { none, start, end, both };

        void setLoop (int ls, int le)
        {
            const int whole = owner.geo.lengthTicks;
            const int newStart = ls, newEnd = (ls == 0 && le >= whole) ? 0 : le;
            if (newStart == owner.pat.loopStart && newEnd == owner.pat.loopEnd) return;
            owner.pat.loopStart = newStart;
            owner.pat.loopEnd = newEnd;
            owner.commit();
        }

        RollEditor& owner;
        Mode mode = Mode::none;
        int  grabTick = 0, baseStart = 0, baseEnd = 0;
    };

    //==========================================================================
    //  NOTES: the keys (or the drum sounds) and the notes
    //==========================================================================
    class RollEditor::NoteGrid final : public juce::Component
    {
    public:
        explicit NoteGrid (RollEditor& o) : owner (o) {}

        int rows() const noexcept           { return owner.maxKey() - owner.minKey() + 1; }
        int rowH() const noexcept           { return juce::jmax (1, getHeight() / rows()); }
        int keyOfRow (int r) const noexcept { return owner.maxKey() - r; }
        int rowOfKey (int k) const noexcept { return owner.maxKey() - k; }
        int keyAt (float y) const noexcept  { return keyOfRow (juce::jlimit (0, rows() - 1, (int) (y / (float) rowH()))); }

        juce::Rectangle<float> noteBounds (const seq::Note& n) const
        {
            const auto& geo = owner.geo;
            const float x0 = geo.xOf (n.start), x1 = geo.xOf (juce::jmin (geo.lengthTicks, n.start + n.length));
            return { x0, (float) (rowOfKey (n.key) * rowH()), juce::jmax (3.0f, x1 - x0), (float) rowH() };
        }

        void paint (juce::Graphics& g) override
        {
            const auto& geo = owner.geo;
            const auto& t = owner.theme();
            const auto& c = owner.chColours;
            const int rh = rowH();
            const auto clip = g.getClipBounds();

            // The rows, and their keys
            for (int r = juce::jmax (0, clip.getY() / rh); r < rows() && r * rh < clip.getBottom(); ++r)
            {
                const int key = keyOfRow (r);
                const int y = r * rh;

                if (owner.drums)
                {
                    g.setColour (r % 2 == 0 ? t.white : t.black);
                    g.fillRect (geo.keyW, y, geo.width - geo.keyW, rh);
                    g.setColour (t.labelBg);
                    g.fillRect (0, y, geo.keyW, rh);
                    g.setColour (t.labelText);
                    g.setFont (font (juce::jmin (17.0f, (float) rh * 0.55f), false));
                    g.drawFittedText (juce::String (key), 14, y, geo.keyW - 18, rh,
                                      juce::Justification::centredLeft, 1, 0.8f);
                }
                else
                {
                    const bool black = isBlack (key);
                    g.setColour (black ? t.black : t.white);
                    g.fillRect (geo.keyW, y, geo.width - geo.keyW, rh);
                    g.setColour (t.keyWhite);
                    g.fillRect (0, y, geo.keyW, rh);
                    if (black)
                    {
                        g.setColour (t.keyBlack);
                        g.fillRect (0, y, geo.keyW - 44, rh);
                    }
                    if (key % 12 == 0)
                    {
                        g.setColour (t.keyText);
                        g.setFont (font ((float) rh * 0.62f, true));
                        g.drawText (juce::MidiMessage::getMidiNoteName (key, true, true, 3), geo.keyW - 76, y, 68, rh,
                                    juce::Justification::centredRight, false);
                    }
                }

                // The selected note's row: half transparent red, across the notes
                if (key == owner.focusKey)
                {
                    g.setColour (focusRow);
                    g.fillRect (geo.keyW, y, geo.width - geo.keyW, rh);
                }

                g.setColour (! owner.drums && key % 12 == 0 ? t.octave : t.rowLine);
                g.drawHorizontalLine (y + rh - 1, 0.0f, (float) geo.width);
            }

            // The grid, the beats and the bars
            for (int tick = 0; tick <= geo.lengthTicks; tick += geo.gridTicks)
            {
                g.setColour (t.fine);
                g.drawVerticalLine ((int) geo.xOf (tick), (float) clip.getY(), (float) clip.getBottom());
            }
            for (int tick = 0; tick <= geo.lengthTicks; tick += geo.beatTicks)
            {
                g.setColour (tick % geo.barTicks == 0 ? t.bar : t.beat);
                g.drawVerticalLine ((int) geo.xOf (tick), (float) clip.getY(), (float) clip.getBottom());
            }

            // Outside the locators: dimmed
            int ls, le;
            owner.pat.loopRange (geo.lengthTicks, ls, le);
            g.setColour (t.outside);
            if (ls > 0)                g.fillRect (juce::Rectangle<float> ((float) geo.keyW, (float) clip.getY(), geo.xOf (ls) - (float) geo.keyW, (float) clip.getHeight()));
            if (le < geo.lengthTicks)  g.fillRect (juce::Rectangle<float> (geo.xOf (le), (float) clip.getY(), (float) geo.width - geo.xOf (le), (float) clip.getHeight()));

            // The notes: the channel's colour, stronger with velocity; selected ones stand out
            // in either look (darker on the bright one, lighter on the dark one, outlined)
            const auto& notes = owner.pat.notes;
            for (int i = 0; i < (int) notes.size(); ++i)
            {
                const auto& n = notes[(size_t) i];
                if (n.start >= geo.lengthTicks || n.key < owner.minKey() || n.key > owner.maxKey()) continue;
                auto b = noteBounds (n).reduced (1.0f, 2.0f);
                if (! b.intersects (clip.toFloat())) continue;

                const bool selected = owner.isSelected (i);
                const float v = (float) n.velocity / 127.0f;
                const bool accent = ! owner.drums && n.velocity >= seq::kAccentVelocity;
                auto fill = c.light.interpolatedWith (c.dark, 0.25f + 0.5f * v);
                if (selected) fill = owner.dark ? fill.interpolatedWith (juce::Colours::white, 0.5f) : fill.interpolatedWith (juce::Colours::black, 0.45f);
                g.setColour (fill);
                g.fillRoundedRectangle (b, 4.0f);
                g.setColour (selected ? t.mark : (accent ? playColour : c.edge));
                g.drawRoundedRectangle (b, 4.0f, selected || accent ? 2.4f : 1.3f);
            }

            // The box being dragged
            if (! marquee.isEmpty())
            {
                g.setColour (c.light.withAlpha (0.18f));
                g.fillRect (marquee);
                g.setColour (t.mark.withAlpha (0.7f));
                g.drawRect (marquee, 1.5f);
            }

            // Where it plays
            if (const int play = owner.shownTick; play >= 0)
            {
                g.setColour (playColour);
                g.drawVerticalLine ((int) owner.geo.xOf (play), (float) clip.getY(), (float) clip.getBottom());
            }
        }

        //======================================================================
        void mouseDown (const juce::MouseEvent& e) override
        {
            owner.grabKeyboardFocus();
            const auto& geo = owner.geo;
            mode = Mode::none;

            // The keys: heard
            if (e.x < geo.keyW)
            {
                owner.audition (keyAt ((float) e.y), owner.lastVelocity);
                return;
            }

            const int key  = keyAt ((float) e.y);
            const int tick = juce::jlimit (0, geo.lengthTicks - 1, geo.tickAt ((float) e.x));
            const int hit  = noteAt (key, tick);
            owner.pasteTick = geo.snapDown (tick);

            if (e.mods.isPopupMenu())
            {
                if (hit >= 0 && ! owner.isSelected (hit)) owner.selectOnly (hit);
                owner.showNoteMenu (owner.pasteTick);
                return;
            }

            if (hit >= 0)
            {
                auto& n = owner.pat.notes[(size_t) hit];
                if (e.mods.isShiftDown() || e.mods.isCommandDown())
                {
                    owner.sel[(size_t) hit] = owner.isSelected (hit) ? 0 : 1;
                    owner.focusKey = owner.isSelected (hit) ? n.key : (owner.numSelected() > 0 ? owner.focusKey : -1);
                    owner.repaintAll();
                    return;
                }
                if (! owner.isSelected (hit)) owner.selectOnly (hit);
                owner.focusKey = n.key;
                owner.lastVelocity = n.velocity;
                owner.repaintAll();

                // Drag: all the selected ones move (or, by the right edge, grow)
                base = owner.pat.notes;
                grabTick = tick;
                grabKey = key;
                lastKey = key;
                baseFocus = owner.focusKey;
                mode = (float) e.x > noteBounds (n).getRight() - 12.0f ? Mode::resize : Mode::move;
                if (mode == Mode::resize) setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
                owner.audition (n.key, n.velocity);
                return;
            }

            // An empty place: a box selects
            if (! (e.mods.isShiftDown() || e.mods.isCommandDown()))
                owner.selectNone();
            baseSel = owner.sel;
            mode = Mode::box;
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            const auto& geo = owner.geo;
            auto& notes = owner.pat.notes;

            if (mode == Mode::box)
            {
                marquee = juce::Rectangle<float> (e.mouseDownPosition, e.position).withLeft (juce::jmax ((float) geo.keyW, juce::jmin (e.mouseDownPosition.x, e.position.x)));
                owner.sel = baseSel;
                int last = -1;
                for (int i = 0; i < (int) notes.size(); ++i)
                    if (noteBounds (notes[(size_t) i]).intersects (marquee))
                    {
                        owner.sel[(size_t) i] = 1;
                        last = i;
                    }
                owner.focusKey = last >= 0 ? notes[(size_t) last].key : (owner.numSelected() > 0 ? owner.focusKey : -1);
                owner.repaintAll();
                return;
            }
            if (mode != Mode::move && mode != Mode::resize) return;

            const int grid = geo.gridTicks;
            const int tick = geo.tickAt ((float) e.x);
            const int dt = juce::roundToInt ((float) (tick - grabTick) / (float) grid) * grid;

            // The selected ones' reach, so all of them stay inside
            int minStart = geo.lengthTicks, maxStart = 0, lowKey = 127, highKey = 0;
            for (int i = 0; i < (int) base.size(); ++i)
                if (owner.isSelected (i))
                {
                    minStart = juce::jmin (minStart, base[(size_t) i].start);
                    maxStart = juce::jmax (maxStart, base[(size_t) i].start);
                    lowKey   = juce::jmin (lowKey, base[(size_t) i].key);
                    highKey  = juce::jmax (highKey, base[(size_t) i].key);
                }

            bool changed = false;
            if (mode == Mode::move)
            {
                const int d  = juce::jlimit (-minStart, juce::jmax (0, geo.lengthTicks - 1 - maxStart), dt);
                const int dk = juce::jlimit (owner.minKey() - lowKey, owner.maxKey() - highKey, keyAt ((float) e.y) - grabKey);
                for (int i = 0; i < (int) base.size() && i < (int) notes.size(); ++i)
                    if (owner.isSelected (i))
                    {
                        const int s = base[(size_t) i].start + d, k = base[(size_t) i].key + dk;
                        if (notes[(size_t) i].start != s || notes[(size_t) i].key != k) changed = true;
                        notes[(size_t) i].start = s;
                        notes[(size_t) i].key = k;
                    }
                const int nowKey = grabKey + dk;
                if (nowKey != lastKey)
                {
                    lastKey = nowKey;
                    owner.focusKey = baseFocus >= 0 ? baseFocus + dk : -1;    // the red row goes with the notes
                    owner.audition (nowKey, owner.lastVelocity);
                    owner.repaintAll();
                }
            }
            else
            {
                for (int i = 0; i < (int) base.size() && i < (int) notes.size(); ++i)
                    if (owner.isSelected (i))
                    {
                        const auto& b = base[(size_t) i];
                        const int len = juce::jlimit (juce::jmin (grid, b.length), geo.lengthTicks - b.start, b.length + dt);
                        if (notes[(size_t) i].length != len) changed = true;
                        notes[(size_t) i].length = len;
                    }
            }
            if (changed) owner.commit();
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (mode == Mode::box)
            {
                marquee = {};
                repaint();
            }
            mode = Mode::none;
            base.clear();
            owner.auditionOff();
            mouseMove (e);          // the pointer as it is where the drag ended
        }

        // Double-click an empty place: a note
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const auto& geo = owner.geo;
            if (e.x < geo.keyW || e.mods.isPopupMenu()) return;
            const int key  = keyAt ((float) e.y);
            const int tick = juce::jlimit (0, geo.lengthTicks - 1, geo.tickAt ((float) e.x));
            if (noteAt (key, tick) >= 0) return;

            owner.addNotes ({ { geo.snapDown (tick), geo.gridTicks, key, owner.lastVelocity } }, true);
            owner.focusKey = key;
            owner.audition (key, owner.lastVelocity);
            owner.repaintAll();
        }

        // Over a note's right edge (where a drag changes its length): the left-right arrow
        void mouseMove (const juce::MouseEvent& e) override
        {
            setMouseCursor (overNoteEdge (e.position) ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
        }

        void mouseExit (const juce::MouseEvent&) override
        {
            if (mode != Mode::resize) setMouseCursor (juce::MouseCursor::NormalCursor);
        }

        bool overNoteEdge (juce::Point<float> p) const
        {
            const auto& geo = owner.geo;
            if (p.x < (float) geo.keyW) return false;
            const int key  = keyAt (p.y);
            const int tick = juce::jlimit (0, geo.lengthTicks - 1, geo.tickAt (p.x));
            const int hit  = noteAt (key, tick);
            return hit >= 0 && p.x > noteBounds (owner.pat.notes[(size_t) hit]).getRight() - 12.0f;
        }

    private:
        enum class Mode { none, move, resize, box };

        int noteAt (int key, int tick) const
        {
            const auto& notes = owner.pat.notes;
            for (int i = (int) notes.size(); --i >= 0;)
                if (notes[(size_t) i].key == key && tick >= notes[(size_t) i].start && tick < notes[(size_t) i].start + juce::jmax (notes[(size_t) i].length, owner.geo.gridTicks / 2))
                    return i;
            return -1;
        }

        RollEditor& owner;
        Mode mode = Mode::none;
        std::vector<seq::Note> base;            // the notes as the drag found them
        std::vector<uint8_t> baseSel;           // the selection as the box found it
        juce::Rectangle<float> marquee;
        int grabTick = 0, grabKey = 0, lastKey = 0, baseFocus = -1;
    };

    //==========================================================================
    //  VELOCITY: a bar per note; the focus row's in front, and only they edited
    //==========================================================================
    class RollEditor::VelocityLane final : public juce::Component
    {
    public:
        explicit VelocityLane (RollEditor& o) : owner (o) {}

        void paint (juce::Graphics& g) override
        {
            const auto& geo = owner.geo;
            const auto& t = owner.theme();
            const auto& c = owner.chColours;
            auto area = laneArea();

            g.setColour (t.lane);
            g.fillRect (getLocalBounds());
            g.setColour (t.labelBg);
            g.fillRect (0, 0, geo.keyW, getHeight());
            g.setColour (t.labelText);
            g.setFont (font (16.0f, true));
            g.drawText ("VELOCITY", 10, getHeight() - 34, geo.keyW - 14, 22, juce::Justification::centredLeft, false);
            if (owner.focusKey >= 0)
            {
                g.setColour (focusRow.withAlpha (1.0f));
                g.setFont (font (15.0f, true));
                g.drawFittedText (juce::MidiMessage::getMidiNoteName (owner.focusKey, true, true, 3),
                                  10, getHeight() / 2 - 11, geo.keyW - 14, 22, juce::Justification::centredLeft, 1, 0.8f);
            }

            for (int tick = 0; tick <= geo.lengthTicks; tick += geo.beatTicks)
            {
                g.setColour (tick % geo.barTicks == 0 ? t.bar : t.laneLine);
                g.drawVerticalLine ((int) geo.xOf (tick), area.getY(), area.getBottom());
            }

            if (! owner.drums)
            {
                const float y = yOf (seq::kAccentVelocity);
                g.setColour (playColour.withAlpha (0.7f));
                g.drawHorizontalLine ((int) y, (float) geo.keyW, (float) geo.width);
                g.setFont (font (15.0f, true));
                g.setColour (t.labelText);
                g.drawText ("ACCENT", 10, (int) y + 2, geo.keyW - 14, 20, juce::Justification::centredLeft, false);
            }

            // The other rows' bars behind, the focus row's in front
            const float barW = juce::jmax (4.0f, (geo.xOf (geo.gridTicks) - geo.xOf (0)) * 0.45f);
            const auto& notes = owner.pat.notes;
            for (int pass = 0; pass < 2; ++pass)
                for (int i = 0; i < (int) notes.size(); ++i)
                {
                    const auto& n = notes[(size_t) i];
                    if (n.start >= geo.lengthTicks) continue;
                    const bool front = owner.focusKey < 0 || n.key == owner.focusKey;
                    if (front != (pass == 1)) continue;

                    const float x = geo.xOf (n.start);
                    const float y = yOf (n.velocity);
                    const bool accent = ! owner.drums && n.velocity >= seq::kAccentVelocity;
                    auto colour = owner.isSelected (i) ? t.mark : accent ? playColour : c.dark.interpolatedWith (c.light, 0.3f);
                    if (! front) colour = colour.withAlpha (0.28f);
                    g.setColour (colour);
                    g.fillRect (x + 1.0f, y, barW, area.getBottom() - y);
                    g.fillEllipse (x + 1.0f + barW * 0.5f - 5.0f, y - 5.0f, 10.0f, 10.0f);
                }
        }

        void mouseDown (const juce::MouseEvent& e) override   { setAt (e); }
        void mouseDrag (const juce::MouseEvent& e) override   { setAt (e); }

    private:
        juce::Rectangle<float> laneArea() const   { return getLocalBounds().toFloat().reduced (0.0f, 10.0f); }
        float yOf (int velocity) const            { auto a = laneArea(); return a.getBottom() - a.getHeight() * (float) velocity / 127.0f; }

        // The bars under the mouse (only the focus row's, when there is one) take its height
        void setAt (const juce::MouseEvent& e)
        {
            const auto& geo = owner.geo;
            if (e.x < geo.keyW) return;
            auto a = laneArea();
            const int v = juce::jlimit (1, 127, juce::roundToInt ((a.getBottom() - (float) e.y) / a.getHeight() * 127.0f));
            const float half = juce::jmax (6.0f, (geo.xOf (geo.gridTicks) - geo.xOf (0)) * 0.5f);

            bool changed = false;
            for (auto& n : owner.pat.notes)
            {
                if (owner.focusKey >= 0 && n.key != owner.focusKey) continue;
                const float x = geo.xOf (n.start) + half * 0.45f;
                if (std::abs ((float) e.x - x) <= half && n.velocity != v)
                {
                    n.velocity = v;
                    changed = true;
                }
            }
            owner.lastVelocity = v;
            if (changed) owner.commit();
        }

        RollEditor& owner;
    };

    //==========================================================================
    RollEditor::RollEditor (ZodiakProcessor& p, int channel)
        : proc (p), ch (channel), drums (false), chColours (channelColours (channel)), dark (p.isRollDark())
    {
        setWantsKeyboardFocus (true);

        // The pattern: the same choice as the banner's
        for (int b = 0; b < 4; ++b)
        {
            auto* btn = bankBtns.add (new juce::TextButton (juce::String::charToString ((juce::juce_wchar) ('A' + b))));
            styleToolButton (*btn, chColours);
            btn->onClick = [this, b] { proc.choosePattern (ch, b, proc.variation (ch)); load(); };
            addAndMakeVisible (btn);
        }
        for (int v = 0; v < 8; ++v)
        {
            auto* btn = varBtns.add (new juce::TextButton (juce::String (v + 1)));
            styleToolButton (*btn, chColours);
            btn->onClick = [this, v] { proc.choosePattern (ch, proc.bank (ch), v); load(); };
            addAndMakeVisible (btn);
        }

        static const int barCounts[3] = { 1, 2, 4 };
        for (int i = 0; i < 3; ++i)
        {
            auto* btn = barBtns.add (new juce::TextButton (juce::String (barCounts[i])));
            styleToolButton (*btn, chColours);
            btn->setTooltip ("The pattern's length: " + juce::String (barCounts[i]) + (i == 0 ? " bar" : " bars"));
            btn->onClick = [this, i] { pat.bars = barCounts[i]; commit(); };
            addAndMakeVisible (btn);
        }

        for (int g = 0; g < seq::kNumGrids; ++g) gridBox.addItem (seq::gridName (g), g + 1);
        gridBox.setTooltip ("What notes snap to (and a new note's length)");
        gridBox.onChange = [this] { pat.grid = gridBox.getSelectedId() - 1; commit(); };
        styleCombo (gridBox);
        addAndMakeVisible (gridBox);

        syncBtn.setTooltip ("SYNC DAW: the pattern's bars follow the DAW's time signature.  Off: its own (on the right).");
        syncBtn.setColour (juce::ToggleButton::textColourId, win::text);
        syncBtn.setColour (juce::ToggleButton::tickColourId, chColours.light);
        syncBtn.onClick = [this] { pat.ownSignature = ! syncBtn.getToggleState(); commit(); };
        addAndMakeVisible (syncBtn);

        for (int n = 1; n <= 16; ++n) numBox.addItem (juce::String (n), n);
        for (int d : { 2, 4, 8, 16 }) denBox.addItem (juce::String (d), d);
        numBox.onChange = [this] { pat.numerator = numBox.getSelectedId(); commit(); };
        denBox.onChange = [this] { pat.denominator = denBox.getSelectedId(); commit(); };
        numBox.setTooltip ("Its own time signature: beats in a bar");
        denBox.setTooltip ("Its own time signature: the beat");
        styleCombo (numBox);
        styleCombo (denBox);
        addAndMakeVisible (numBox);
        addAndMakeVisible (denBox);

        for (int v = seq::Pattern::kSwingMin; v <= seq::Pattern::kSwingMax; ++v)
            swingBox.addItem (v == seq::Pattern::kSwingMin ? juce::String ("OFF") : juce::String (v) + "%", v);
        swingBox.setTooltip ("SWING: the second 16th of each 8th comes late (66%: a triplet feel; 75%: half a 16th late).  Heard, not drawn: the notes stay on the grid.");
        swingBox.onChange = [this] { pat.swing = swingBox.getSelectedId(); commit(); };
        styleCombo (swingBox);
        addAndMakeVisible (swingBox);

        styleToolButton (clearBtn, chColours);
        clearBtn.setTooltip ("Clear the pattern's notes");
        clearBtn.onClick = [this] { pat.notes.clear(); sel.clear(); focusKey = -1; commit(); };
        addAndMakeVisible (clearBtn);

        styleToolButton (exportBtn, chColours);
        exportBtn.setTooltip ("Save the pattern as a standard MIDI file: what loops (between the locators), quantized, with its tempo, time signature and swing setting");
        exportBtn.onClick = [this] { exportMidi(); };
        addAndMakeVisible (exportBtn);

        styleToolButton (importBtn, chColours);
        importBtn.setTooltip ("Load a standard MIDI file into this pattern (it replaces its notes): its length sets BARS and the locators, its time signature and swing come along");
        importBtn.onClick = [this] { importMidi(); };
        addAndMakeVisible (importBtn);

        ruler = std::make_unique<Ruler> (*this);
        grid  = std::make_unique<NoteGrid> (*this);
        lane  = std::make_unique<VelocityLane> (*this);
        addAndMakeVisible (*ruler);
        addAndMakeVisible (*lane);
        view.setViewedComponent (grid.get(), false);
        view.setScrollBarsShown (! drums, false);
        view.setScrollBarThickness (14);
        addAndMakeVisible (view);

        load();
        startTimerHz (30);
    }

    RollEditor::~RollEditor()
    {
        auditionOff();
    }

    void RollEditor::setDark (bool d)
    {
        dark = d;
        proc.setRollDark (d);
        repaintAll();
    }

    //==========================================================================
    void RollEditor::load()
    {
        index = seq::patternIndex (proc.bank (ch), proc.variation (ch));
        shownGeneration = proc.setGeneration();
        pat = proc.engine().patterns().get (ch, index);
        sel.assign (pat.notes.size(), 0);
        focusKey = -1;
        refreshToolbar();
        updateGeometry();
        if (onPatternChanged) onPatternChanged (seq::patternName (index));
    }

    void RollEditor::commit()
    {
        // Notes past a shorter length stay (they play again when it grows)
        sel.resize (pat.notes.size(), 0);
        proc.engine().patterns().set (ch, index, pat);
        refreshToolbar();
        updateGeometry();
    }

    void RollEditor::exportMidi()
    {
        const auto patternLabel = seq::patternName (index);
        const auto title = "Zodiak CH" + juce::String (ch + 1) + " " + patternLabel;

        auto& lastFolder = midiFolder();
        const auto folder = lastFolder.isDirectory() ? lastFolder : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

        chooser = std::make_unique<juce::FileChooser> ("Export the pattern as MIDI", folder.getChildFile (title + ".mid"), "*.mid");
        juce::Component::SafePointer<RollEditor> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safe, title] (const juce::FileChooser& fc)
        {
            if (safe == nullptr) return;
            auto file = fc.getResult();
            if (file == juce::File()) return;
            if (! file.hasFileExtension ("mid;midi")) file = file.withFileExtension ("mid");

            auto& sq = safe->proc.engine().patterns();
            double bpm = safe->proc.engine().tempo();
            if (bpm <= 1.0) bpm = safe->proc.engine().master (mpTempo).load();
            const auto midi = seq::patternToMidi (safe->pat, sq.dawNumerator(), sq.dawDenominator(), bpm, false, title);

            const auto temp = file.getSiblingFile (file.getFileNameWithoutExtension() + ".tmp");
            bool ok = false;
            {
                temp.deleteFile();
                juce::FileOutputStream out (temp);
                ok = out.openedOk() && midi.writeTo (out, 1);
                out.flush();
                ok = ok && ! out.getStatus().failed();
            }
            ok = ok && temp.moveFileTo (file);
            if (! ok)
            {
                temp.deleteFile();
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Export MIDI", "Couldn't write " + file.getFullPathName());
                return;
            }
            midiFolder() = file.getParentDirectory();
        });
    }

    void RollEditor::importMidi()
    {
        const auto& lastFolder = midiFolder();
        const auto folder = lastFolder.isDirectory() ? lastFolder : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

        chooser = std::make_unique<juce::FileChooser> ("Import a MIDI file into pattern " + seq::patternName (index), folder, "*.mid;*.midi");
        juce::Component::SafePointer<RollEditor> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe] (const juce::FileChooser& fc)
        {
            if (safe == nullptr) return;
            const auto file = fc.getResult();
            if (! file.existsAsFile()) return;
            midiFolder() = file.getParentDirectory();

            auto fail = [&file] (const juce::String& why)
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Import MIDI", file.getFileName() + ": " + why);
            };

            juce::MidiFile midi;
            {
                juce::FileInputStream in (file);
                if (! in.openedOk() || ! midi.readFrom (in, true)) { fail ("not a standard MIDI file"); return; }
            }

            auto& sq = safe->proc.engine().patterns();
            auto p = safe->pat;
            std::function<int (int)> mapKey;
            // Any MIDI channel's notes (10, the GM drums, first when the file has it): a
            // channel plays whatever its VSTi is
            const auto r = seq::patternFromMidi (midi, true, sq.dawNumerator(), sq.dawDenominator(), mapKey, p);
            if (! r.ok) { fail (r.error); return; }

            safe->pat = std::move (p);
            safe->sel.assign (safe->pat.notes.size(), 0);
            safe->focusKey = -1;
            safe->commit();
            safe->repaintAll();

            if (r.cut || r.dropped > 0)
            {
                juce::String text;
                if (r.cut) text << "Longer than 4 bars: the first 4 were kept.\n";
                if (r.dropped > 0) text << juce::String (r.dropped) << (r.dropped == 1 ? " note was" : " notes were")
                                        << " left out (past the pattern's end, or keys this channel doesn't play).";
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Import MIDI", file.getFileName() + "\n\n" + text.trim());
            }
        });
    }

    void RollEditor::refreshToolbar()
    {
        for (int b = 0; b < 4; ++b) bankBtns[b]->setToggleState (proc.bank (ch) == b, juce::dontSendNotification);
        for (int v = 0; v < 8; ++v) varBtns[v]->setToggleState (proc.variation (ch) == v, juce::dontSendNotification);
        static const int barCounts[3] = { 1, 2, 4 };
        for (int i = 0; i < 3; ++i) barBtns[i]->setToggleState (pat.bars == barCounts[i], juce::dontSendNotification);
        gridBox.setSelectedId (pat.grid + 1, juce::dontSendNotification);
        syncBtn.setToggleState (! pat.ownSignature, juce::dontSendNotification);
        numBox.setSelectedId (pat.numerator, juce::dontSendNotification);
        denBox.setSelectedId (pat.denominator, juce::dontSendNotification);
        swingBox.setSelectedId (juce::jlimit (seq::Pattern::kSwingMin, seq::Pattern::kSwingMax, pat.swing), juce::dontSendNotification);
        numBox.setEnabled (pat.ownSignature);
        denBox.setEnabled (pat.ownSignature);
    }

    void RollEditor::updateGeometry()
    {
        auto& s = proc.engine().patterns();
        int num, den;
        pat.signature (s.dawNumerator(), s.dawDenominator(), num, den);

        geo.barTicks    = pat.barTicks (s.dawNumerator(), s.dawDenominator());
        geo.lengthTicks = pat.bars * geo.barTicks;
        geo.beatTicks   = seq::kTicksPerQuarter * 4 / den;
        geo.gridTicks   = seq::gridTicks (pat.grid);
        geo.width       = grid->getWidth() > 0 ? grid->getWidth() : getWidth();
        repaintAll();
    }

    void RollEditor::repaintAll()
    {
        ruler->repaint();
        grid->repaint();
        lane->repaint();
    }

    //==========================================================================
    //  The notes and the selection
    //==========================================================================
    int RollEditor::numSelected() const noexcept
    {
        int n = 0;
        for (auto s : sel) n += s != 0 ? 1 : 0;
        return n;
    }

    void RollEditor::selectOnly (int i)
    {
        sel.assign (pat.notes.size(), 0);
        if (juce::isPositiveAndBelow (i, (int) sel.size()))
        {
            sel[(size_t) i] = 1;
            focusKey = pat.notes[(size_t) i].key;
        }
        repaintAll();
    }

    void RollEditor::selectNone()
    {
        sel.assign (pat.notes.size(), 0);
        focusKey = -1;
        repaintAll();
    }

    void RollEditor::selectAll()
    {
        sel.assign (pat.notes.size(), 1);
        repaintAll();
    }

    void RollEditor::addNotes (const std::vector<seq::Note>& notes, bool selectThem)
    {
        if (selectThem) sel.assign (pat.notes.size(), 0);
        for (const auto& n : notes)
        {
            if ((int) pat.notes.size() >= seq::kMaxNotes) break;
            if (n.start < 0 || n.start >= geo.lengthTicks || n.key < minKey() || n.key > maxKey()) continue;
            pat.notes.push_back (n);
            sel.push_back (selectThem ? 1 : 0);
        }
        commit();
    }

    void RollEditor::deleteSelected()
    {
        std::vector<seq::Note> kept;
        for (int i = 0; i < (int) pat.notes.size(); ++i)
            if (! isSelected (i)) kept.push_back (pat.notes[(size_t) i]);
        if (kept.size() == pat.notes.size()) return;
        pat.notes = std::move (kept);
        sel.assign (pat.notes.size(), 0);
        focusKey = -1;
        commit();
    }

    void RollEditor::copySelected()
    {
        auto& clip = noteClipboard();
        clip.clear();
        int first = std::numeric_limits<int>::max();
        for (int i = 0; i < (int) pat.notes.size(); ++i)
            if (isSelected (i)) first = juce::jmin (first, pat.notes[(size_t) i].start);
        for (int i = 0; i < (int) pat.notes.size(); ++i)
            if (isSelected (i))
            {
                auto n = pat.notes[(size_t) i];
                n.start -= first;
                clip.push_back (n);
            }
    }

    void RollEditor::paste (int atTick)
    {
        std::vector<seq::Note> pasted;
        for (auto n : noteClipboard())
        {
            n.start += atTick;
            pasted.push_back (n);
        }
        if (! pasted.empty()) addNotes (pasted, true);
    }

    // A copy right after the selection (Studio One's Duplicate)
    void RollEditor::duplicateSelected()
    {
        int first = std::numeric_limits<int>::max(), end = 0;
        for (int i = 0; i < (int) pat.notes.size(); ++i)
            if (isSelected (i))
            {
                first = juce::jmin (first, pat.notes[(size_t) i].start);
                end   = juce::jmax (end, pat.notes[(size_t) i].start + pat.notes[(size_t) i].length);
            }
        if (end <= 0) return;

        const int step = geo.gridTicks;
        const int offset = juce::jmax (step, ((end - first + step - 1) / step) * step);
        std::vector<seq::Note> copies;
        for (int i = 0; i < (int) pat.notes.size(); ++i)
            if (isSelected (i))
            {
                auto n = pat.notes[(size_t) i];
                n.start += offset;
                copies.push_back (n);
            }
        addNotes (copies, true);
    }

    void RollEditor::nudgeSelected (int ticks, int keys)
    {
        if (numSelected() == 0) return;
        for (int i = 0; i < (int) pat.notes.size(); ++i)
            if (isSelected (i))
            {
                const auto& n = pat.notes[(size_t) i];
                if (n.start + ticks < 0 || n.start + ticks >= geo.lengthTicks || n.key + keys < minKey() || n.key + keys > maxKey())
                    return;                                 // all of them, or none
            }
        for (int i = 0; i < (int) pat.notes.size(); ++i)
            if (isSelected (i))
            {
                pat.notes[(size_t) i].start += ticks;
                pat.notes[(size_t) i].key += keys;
            }
        if (focusKey >= 0) focusKey += keys;
        commit();
    }

    void RollEditor::showNoteMenu (int tick)
    {
        const bool any = numSelected() > 0;
        juce::PopupMenu menu;
        menu.addItem (1, "Cut", any);
        menu.addItem (2, "Copy", any);
        menu.addItem (3, "Paste", ! noteClipboard().empty());
        menu.addItem (4, "Duplicate", any);
        menu.addItem (5, "Delete", any);
        menu.addSeparator();
        menu.addItem (6, "Select All", ! pat.notes.empty());

        juce::Component::SafePointer<RollEditor> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options(), [safe, tick] (int r)
        {
            if (safe == nullptr) return;
            switch (r)
            {
                case 1: safe->copySelected(); safe->deleteSelected(); break;
                case 2: safe->copySelected(); break;
                case 3: safe->paste (tick); break;
                case 4: safe->duplicateSelected(); break;
                case 5: safe->deleteSelected(); break;
                case 6: safe->selectAll(); break;
                default: break;
            }
        });
    }

    void RollEditor::audition (int key, int velocity)
    {
        auditionOff();
        proc.engine().audition (ch, key, juce::jlimit (1, 127, velocity));
        auditioning = key;
    }

    void RollEditor::auditionOff()
    {
        if (auditioning >= 0) proc.engine().audition (ch, auditioning, 0);
        auditioning = -1;
    }

    bool RollEditor::keyPressed (const juce::KeyPress& k)
    {
        const auto mods = k.getModifiers();
        const int code = k.getKeyCode();

        if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) { deleteSelected(); return true; }
        if (k == juce::KeyPress::escapeKey)  { selectNone(); return true; }
        if (mods.isCommandDown())
        {
            if (code == 'A') { selectAll(); return true; }
            if (code == 'C') { copySelected(); return true; }
            if (code == 'X') { copySelected(); deleteSelected(); return true; }
            if (code == 'V') { paste (pasteTick); return true; }
            if (code == 'D') { duplicateSelected(); return true; }
        }
        if (k.isKeyCode (juce::KeyPress::upKey))    { nudgeSelected (0, mods.isShiftDown() && ! drums ? 12 : 1); return true; }
        if (k.isKeyCode (juce::KeyPress::downKey))  { nudgeSelected (0, mods.isShiftDown() && ! drums ? -12 : -1); return true; }
        if (k.isKeyCode (juce::KeyPress::leftKey))  { nudgeSelected (-geo.gridTicks, 0); return true; }
        if (k.isKeyCode (juce::KeyPress::rightKey)) { nudgeSelected (geo.gridTicks, 0); return true; }
        return false;
    }

    // Every channel is a piano roll now (no built-in drum kit): kept for its callers
    void RollEditor::updateRollKind()
    {
        const bool wantRows = false;
        if (wantRows == drums) return;
        drums = wantRows;
        focusKey = -1;
        view.setScrollBarsShown (! drums, false);
        grid->setSize (grid->getWidth(), 0);        // laid out afresh: the piano roll opens on the notes
        resized();
        repaintAll();
    }

    void RollEditor::timerCallback()
    {
        updateRollKind();

        // Another pattern chosen (the banner, MIDI, a set loaded)
        if (seq::patternIndex (proc.bank (ch), proc.variation (ch)) != index || proc.setGeneration() != shownGeneration)
            load();

        // The DAW's time signature changed (a SYNC DAW pattern follows it)
        auto& s = proc.engine().patterns();
        if (pat.barTicks (s.dawNumerator(), s.dawDenominator()) != geo.barTicks) updateGeometry();

        // Where it plays: only while the chosen pattern is the one playing
        const int tick = s.playing (ch) == index ? s.position (ch) : -1;
        if (tick != shownTick)
        {
            const auto repaintAt = [this] (int t)
            {
                if (t < 0) return;
                const int x = (int) geo.xOf (t);
                grid->repaint (x - 3, 0, 7, grid->getHeight());
                ruler->repaint (x - 10, 0, 21, ruler->getHeight());
            };
            repaintAt (shownTick);
            shownTick = tick;
            repaintAt (shownTick);
        }
    }

    //==========================================================================
    void RollEditor::paint (juce::Graphics& g)
    {
        g.setColour (win::row);
        g.fillRoundedRectangle (toolbar.toFloat(), 10.0f);

        g.setColour (win::dim);
        g.setFont (font (16.0f, true));
        auto caption = [&] (juce::Component& c, const juce::String& text, int width)
        {
            g.drawText (text, c.getX() - width - 8, toolbar.getY(), width, toolbar.getHeight(), juce::Justification::centredRight, false);
        };
        caption (*bankBtns[0], "PATTERN", 90);
        caption (*barBtns[0], "BARS", 56);
        caption (gridBox, "GRID", 50);
        caption (syncBtn, "TIME SIG", 90);
        caption (swingBox, "SWING", 64);
        g.drawText ("/", numBox.getRight(), toolbar.getY(), denBox.getX() - numBox.getRight(), toolbar.getHeight(), juce::Justification::centred, false);
    }

    void RollEditor::resized()
    {
        auto r = getLocalBounds();
        toolbar = r.removeFromTop (kToolbarH);

        // The toolbar
        auto t = toolbar.reduced (10, 9);
        t.removeFromLeft (98);
        for (auto* b : bankBtns) { b->setBounds (t.removeFromLeft (44)); t.removeFromLeft (3); }
        t.removeFromLeft (12);
        for (auto* b : varBtns)  { b->setBounds (t.removeFromLeft (40)); t.removeFromLeft (3); }
        t.removeFromLeft (30 + 64);
        for (auto* b : barBtns)  { b->setBounds (t.removeFromLeft (44)); t.removeFromLeft (3); }
        t.removeFromLeft (30 + 58);
        gridBox.setBounds (t.removeFromLeft (110));
        t.removeFromLeft (30 + 98);
        syncBtn.setBounds (t.removeFromLeft (150));
        t.removeFromLeft (8);
        numBox.setBounds (t.removeFromLeft (70));
        t.removeFromLeft (22);
        denBox.setBounds (t.removeFromLeft (70));
        t.removeFromLeft (30 + 72);
        swingBox.setBounds (t.removeFromLeft (90));
        auto right = toolbar.reduced (10, 9);
        clearBtn.setBounds (right.removeFromRight (90));
        right.removeFromRight (8);
        exportBtn.setBounds (right.removeFromRight (124));
        right.removeFromRight (8);
        importBtn.setBounds (right.removeFromRight (124));

        r.removeFromTop (kGap);
        lane->setBounds (r.removeFromBottom (kLaneH));
        r.removeFromBottom (kGap);
        ruler->setBounds (r.removeFromTop (kRulerH));
        view.setBounds (r);

        // The notes: as wide as the window (the scroll bar aside); the drums fit, the keys scroll
        const int w = r.getWidth() - (drums ? 0 : view.getScrollBarThickness());
        const int h = drums ? r.getHeight() : 128 * kSynthRowH;
        const bool first = grid->getHeight() == 0;
        grid->setSize (w, h);
        ruler->setSize (w, kRulerH);
        lane->setSize (w, kLaneH);

        // A synth channel opens on its notes, or with C3..C5 in view
        if (first && ! drums)
        {
            int top = 127 - 72;
            if (! pat.notes.empty())
            {
                int high = 0;
                for (const auto& n : pat.notes) high = juce::jmax (high, n.key);
                top = 127 - juce::jmin (127, high + 6);
            }
            view.setViewPosition (0, juce::jmax (0, top * kSynthRowH - kSynthRowH * 4));
        }

        updateGeometry();
    }
}
