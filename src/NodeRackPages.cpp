// D:\workspace\Fanan_web_hub\src\NodeRackPages.cpp
//
// The Node Rack's Mixer and Manual pages, the Rack page's Macros strip, and the Latcher's,
// Step Seq's and Transient Splitter's editors (Colosseum's, ported).

#include "NodeRackPages.h"

#include <algorithm>
#include <cmath>

namespace
{
    juce::Font bold (float size)    { return juce::Font (juce::FontOptions (size, juce::Font::bold)); }
    juce::Font plain (float size)   { return juce::Font (juce::FontOptions (size)); }

    juce::String noteName (int note)
    {
        return juce::MidiMessage::getMidiNoteName (note, true, true, 4);
    }

    //==============================================================================
    /** What every tool editor does: it closes itself when its tool has gone, and says when a
        setting changed. */
    class ToolEditor : public juce::Component, private juce::Timer
    {
    public:
        explicit ToolEditor (NodeRackPages::ToolLink l) : link (std::move (l))
        {
            startTimerHz (20);
        }

        ~ToolEditor() override   { stopTimer(); }

    protected:
        bool alive() const       { return link.isAlive != nullptr && link.isAlive(); }

        void edited()
        {
            if (alive() && link.edited != nullptr)
                link.edited();
        }

        virtual void tick()      { repaint(); }

    private:
        void timerCallback() override
        {
            if (alive())
            {
                tick();
                return;
            }

            stopTimer();

            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
        }

        NodeRackPages::ToolLink link;
    };

    //==============================================================================
    // The Latcher's editor: the 4x4 pads (click to latch, right-click to choose), and the
    // chosen pad's trigger note, output note, velocity and channel, dragged up and down.
    class LatcherEditor final : public ToolEditor
    {
    public:
        LatcherEditor (NodeTools::Latcher& l, NodeRackPages::ToolLink toolLink)
            : ToolEditor (std::move (toolLink)), latcher (l)
        {
            setSize (520, 480);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (35, 35, 40));

            if (! alive())
                return;

            auto bounds = getLocalBounds();

            // Title, and All Off
            auto title = bounds.removeFromTop (40);
            g.setColour (juce::Colour (50, 50, 58));
            g.fillRect (title);
            g.setColour (juce::Colour (70, 70, 80));
            g.drawHorizontalLine (title.getBottom() - 1, 0.0f, static_cast<float> (getWidth()));
            g.setColour (juce::Colours::white);
            g.setFont (bold (18.0f));
            g.drawText ("The Latcher", title.reduced (12, 0), juce::Justification::centredLeft);

            allOffRect = title.removeFromRight (120).reduced (8, 6).toFloat();
            g.setColour (juce::Colour (120, 40, 40));
            g.fillRoundedRectangle (allOffRect, 5.0f);
            g.setColour (juce::Colour (180, 60, 60));
            g.drawRoundedRectangle (allOffRect, 5.0f, 1.0f);
            g.setColour (juce::Colours::black);
            g.setFont (bold (11.0f));
            g.drawText ("ALL OFF", allOffRect, juce::Justification::centred);

            // The chosen pad's settings
            auto bottom = bounds.removeFromBottom (100);
            g.setColour (juce::Colour (42, 42, 48));
            g.fillRect (bottom);
            g.setColour (juce::Colour (70, 70, 80));
            g.drawHorizontalLine (bottom.getY(), 0.0f, static_cast<float> (getWidth()));
            drawPadSettings (g, bottom);

            // The pads
            gridArea = bounds.reduced (12).toFloat();
            drawPads (g);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (! alive())
                return;

            const auto pos = e.position;

            if (allOffRect.contains (pos))
            {
                latcher.allNotesOff();
                repaint();
                return;
            }

            if (gridArea.contains (pos))
            {
                const auto col = juce::jlimit (0, NodeTools::Latcher::gridCols - 1, static_cast<int> ((pos.x - gridArea.getX()) / (gridArea.getWidth() / NodeTools::Latcher::gridCols)));
                const auto row = juce::jlimit (0, NodeTools::Latcher::gridRows - 1, static_cast<int> ((pos.y - gridArea.getY()) / (gridArea.getHeight() / NodeTools::Latcher::gridRows)));
                selected = row * NodeTools::Latcher::gridCols + col;

                if (! e.mods.isRightButtonDown())
                    latcher.togglePadFromUI (selected);

                repaint();
                return;
            }

            const auto& pad = latcher.getPad (selected);
            dragStartY = pos.y;

            if (triggerRect.contains (pos))        { dragging = Drag::trigger;  dragStartValue = pad.triggerNote; }
            else if (outputRect.contains (pos))    { dragging = Drag::output;   dragStartValue = pad.outputNote; }
            else if (velocityRect.contains (pos))  { dragging = Drag::velocity; dragStartValue = pad.velocity; }
            else if (channelRect.contains (pos))   { dragging = Drag::channel;  dragStartValue = pad.midiChannel; }
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! alive() || dragging == Drag::none)
                return;

            const auto delta = dragStartY - e.position.y;
            auto& pad = latcher.getPad (selected);

            switch (dragging)
            {
                case Drag::trigger:  pad.triggerNote = juce::jlimit (0, 127, static_cast<int> (dragStartValue + delta * 0.3f)); break;
                case Drag::output:   pad.outputNote = juce::jlimit (0, 127, static_cast<int> (dragStartValue + delta * 0.3f)); break;
                case Drag::velocity: pad.velocity = juce::jlimit (1, 127, static_cast<int> (dragStartValue + delta * 0.5f)); break;
                case Drag::channel:  pad.midiChannel = juce::jlimit (1, 16, static_cast<int> (dragStartValue + delta * 0.1f)); break;
                case Drag::none:
                default:             break;
            }

            repaint();
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (std::exchange (dragging, Drag::none) != Drag::none)
                edited();
        }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (! alive())
                return;

            // A value double-clicked goes back to what the pad started with.
            auto& pad = latcher.getPad (selected);
            const auto fresh = NodeTools::Latcher::defaultPad (selected);
            const auto pos = e.position;

            if (triggerRect.contains (pos))        pad.triggerNote = fresh.triggerNote;
            else if (outputRect.contains (pos))    pad.outputNote = fresh.outputNote;
            else if (velocityRect.contains (pos))  pad.velocity = fresh.velocity;
            else if (channelRect.contains (pos))   pad.midiChannel = fresh.midiChannel;
            else return;

            edited();
            repaint();
        }

    private:
        void drawPads (juce::Graphics& g)
        {
            const auto padW = gridArea.getWidth() / NodeTools::Latcher::gridCols;
            const auto padH = gridArea.getHeight() / NodeTools::Latcher::gridRows;
            constexpr float gap = 4.0f;

            for (int row = 0; row < NodeTools::Latcher::gridRows; ++row)
            {
                for (int col = 0; col < NodeTools::Latcher::gridCols; ++col)
                {
                    const auto index = row * NodeTools::Latcher::gridCols + col;
                    auto r = juce::Rectangle<float> (gridArea.getX() + col * padW + gap / 2, gridArea.getY() + row * padH + gap / 2, padW - gap, padH - gap);
                    const auto on = latcher.isPadLatched (index);
                    const auto& pad = latcher.getPad (index);

                    if (on)
                    {
                        g.setColour (juce::Colour (60, 160, 255).withAlpha (0.2f));
                        g.fillRoundedRectangle (r.expanded (2.0f), 8.0f);
                        g.setColour (juce::Colour (30, 120, 200));
                    }
                    else
                    {
                        g.setColour (juce::Colour (55, 55, 65));
                    }

                    g.fillRoundedRectangle (r, 6.0f);
                    g.setColour (index == selected ? juce::Colours::orange : on ? juce::Colour (80, 160, 240) : juce::Colour (80, 80, 90));
                    g.drawRoundedRectangle (r, 6.0f, index == selected ? 2.5f : 1.0f);

                    g.setColour (juce::Colours::white.withAlpha (0.4f));
                    g.setFont (plain (10.0f));
                    g.drawText (juce::String (index + 1), r.reduced (6.0f, 4.0f), juce::Justification::topLeft);

                    g.setColour (on ? juce::Colours::white : juce::Colours::white.withAlpha (0.7f));
                    g.setFont (bold (on ? 16.0f : 14.0f));
                    g.drawText (noteName (pad.outputNote), r, juce::Justification::centred);

                    g.setColour (juce::Colours::white.withAlpha (0.35f));
                    g.setFont (plain (9.0f));
                    g.drawText ("Ch" + juce::String (pad.midiChannel), r.reduced (6.0f, 4.0f), juce::Justification::bottomRight);

                    auto bar = r.removeFromBottom (3.0f).reduced (8.0f, 0.0f);
                    g.setColour (juce::Colour (60, 60, 70));
                    g.fillRoundedRectangle (bar, 1.0f);
                    g.setColour (on ? juce::Colour (100, 200, 255) : juce::Colour (100, 100, 120));
                    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * static_cast<float> (pad.velocity) / 127.0f), 1.0f);
                }
            }
        }

        void drawPadSettings (juce::Graphics& g, juce::Rectangle<int> area)
        {
            const auto& pad = latcher.getPad (selected);
            auto content = area.reduced (12, 8);

            g.setColour (juce::Colours::orange);
            g.setFont (bold (12.0f));
            g.drawText ("Pad " + juce::String (selected + 1) + " Settings", content.removeFromTop (20), juce::Justification::centredLeft);

            const auto row = content.toFloat();
            const auto boxW = (row.getWidth() - 30.0f) / 4.0f;
            triggerRect = juce::Rectangle<float> (row.getX(), row.getY(), boxW, row.getHeight()).reduced (2.0f);
            outputRect = juce::Rectangle<float> (row.getX() + boxW + 10.0f, row.getY(), boxW, row.getHeight()).reduced (2.0f);
            velocityRect = juce::Rectangle<float> (row.getX() + (boxW + 10.0f) * 2.0f, row.getY(), boxW, row.getHeight()).reduced (2.0f);
            channelRect = juce::Rectangle<float> (row.getX() + (boxW + 10.0f) * 3.0f, row.getY(), boxW, row.getHeight()).reduced (2.0f);

            const auto box = [&g] (juce::Rectangle<float> r, const juce::String& label, const juce::String& value, const juce::String& hint)
            {
                g.setColour (juce::Colour (30, 30, 35));
                g.fillRoundedRectangle (r, 4.0f);
                g.setColour (juce::Colour (60, 60, 70));
                g.drawRoundedRectangle (r, 4.0f, 1.0f);
                g.setColour (juce::Colour (160, 160, 180));
                g.setFont (plain (9.0f));
                g.drawText (label, r.reduced (4.0f, 2.0f), juce::Justification::topLeft);
                g.setColour (juce::Colours::white);
                g.setFont (bold (16.0f));
                g.drawText (value, r, juce::Justification::centred);
                g.setColour (juce::Colour (120, 120, 140));
                g.setFont (plain (8.0f));
                g.drawText (hint, r.reduced (4.0f, 2.0f), juce::Justification::centredBottom);
            };

            const juce::String dragHint (juce::CharPointer_UTF8 ("Drag \xe2\x86\x95"));
            box (triggerRect, "TRIGGER", noteName (pad.triggerNote), dragHint + " (Note " + juce::String (pad.triggerNote) + ")");
            box (outputRect, "OUTPUT", noteName (pad.outputNote), dragHint + " (Note " + juce::String (pad.outputNote) + ")");
            box (velocityRect, "VELOCITY", juce::String (pad.velocity), dragHint);
            box (channelRect, "CHANNEL", juce::String (pad.midiChannel), dragHint + " (1-16)");
        }

        enum class Drag { none, trigger, output, velocity, channel };

        NodeTools::Latcher& latcher;
        int selected = 0;
        juce::Rectangle<float> gridArea, allOffRect, triggerRect, outputRect, velocityRect, channelRect;
        Drag dragging = Drag::none;
        float dragStartY = 0.0f, dragStartValue = 0.0f;
    };

    //==============================================================================
    // The Step Seq's editor: the 16 lanes on the left, the chosen lane's steps (drawn with the
    // mouse), its tools on the right (menu, up, down, shift left and right, clear), and its
    // settings along the bottom.
    class StepSeqEditor final : public ToolEditor
    {
    public:
        StepSeqEditor (NodeTools::StepSeq& s, NodeRackPages::ToolLink toolLink)
            : ToolEditor (std::move (toolLink)), seq (s)
        {
            setSize (900, 520);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (35, 35, 40));

            if (! alive())
                return;

            auto bounds = getLocalBounds();
            auto& slot = seq.getSlot (selected);

            // Title: the lane's on/off (the play mark), its number and channel
            auto title = bounds.removeFromTop (36);
            g.setColour (juce::Colour (50, 50, 58));
            g.fillRect (title);
            g.setColour (juce::Colour (70, 70, 80));
            g.drawHorizontalLine (title.getBottom() - 1, 0.0f, static_cast<float> (getWidth()));

            const auto toggle = enableRect();
            g.setColour (slot.enabled ? juce::Colours::limegreen : juce::Colour (80, 80, 80));
            g.fillEllipse (toggle);
            g.setColour (juce::Colours::white.withAlpha (0.3f));
            g.drawEllipse (toggle, 1.0f);
            juce::Path triangle;
            const auto c = toggle.getCentre();
            triangle.addTriangle (c.x - 2.1f, c.y - 3.0f, c.x - 2.1f, c.y + 3.0f, c.x + 3.3f, c.y);
            g.setColour (slot.enabled ? juce::Colours::black : juce::Colour (50, 50, 50));
            g.fillPath (triangle);

            g.setColour (juce::Colours::white);
            g.setFont (bold (16.0f));
            g.drawText ("Sequence #" + juce::String (selected + 1) + "  (Ch " + juce::String (slot.midiChannel) + ")",
                        title.withTrimmedLeft (36).reduced (8, 0), juce::Justification::centredLeft);

            g.setColour (seq.isPlaying() ? juce::Colours::limegreen : juce::Colour (140, 140, 160));
            g.setFont (bold (11.0f));
            g.drawText ((seq.isPlaying() ? "PLAYING  " : "STOPPED  ") + juce::String (seq.getEffectiveBpm(), 1) + " BPM"
                            + (seq.isSyncedToSongTempo() ? " (song)" : ""),
                        title.reduced (12, 0), juce::Justification::centredRight);

            auto bottom = bounds.removeFromBottom (56);
            g.setColour (juce::Colour (42, 42, 48));
            g.fillRect (bottom);
            g.setColour (juce::Colour (70, 70, 80));
            g.drawHorizontalLine (bottom.getY(), 0.0f, static_cast<float> (getWidth()));
            drawControls (g, bottom);

            auto lanes = bounds.removeFromLeft (40);
            g.setColour (juce::Colour (42, 42, 48));
            g.fillRect (lanes);
            drawLanes (g, lanes);

            auto tools = bounds.removeFromRight (36);
            g.setColour (juce::Colour (42, 42, 48));
            g.fillRect (tools);
            drawTools (g, tools);

            gridArea = bounds.reduced (2).toFloat();
            drawSteps (g);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (! alive())
                return;

            const auto pos = e.position;
            auto bounds = getLocalBounds();
            bounds.removeFromTop (36);

            if (enableRect().contains (pos))
            {
                auto& slot = seq.getSlot (selected);
                slot.enabled = ! slot.enabled;
                edited();
                repaint();
                return;
            }

            const auto bottom = bounds.removeFromBottom (56);

            if (bottom.toFloat().contains (pos))
            {
                controlClicked (pos, bottom);
                return;
            }

            const auto lanes = bounds.removeFromLeft (40);

            if (lanes.toFloat().contains (pos))
            {
                selected = juce::jlimit (0, NodeTools::StepSeq::maxSlots - 1, static_cast<int> ((pos.y - static_cast<float> (lanes.getY())) / (static_cast<float> (lanes.getHeight()) / 16.0f)));
                repaint();
                return;
            }

            const auto tools = bounds.removeFromRight (36);

            if (tools.toFloat().contains (pos))
            {
                toolClicked (pos, tools);
                return;
            }

            if (gridArea.contains (pos))
            {
                if (e.mods.isRightButtonDown())
                {
                    stepMenu (pos);
                    return;
                }

                drawStepAt (pos);
                drawing = true;
            }
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (alive() && drawing && gridArea.contains (e.position))
                drawStepAt (e.position);
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (std::exchange (drawing, false))
                edited();
        }

    private:
        void tick() override
        {
            const auto step = seq.getSlotCurrentStep (selected);

            if (step != lastStep || seq.isPlaying() != wasPlaying)
            {
                lastStep = step;
                wasPlaying = seq.isPlaying();
                repaint();
            }
        }

        static juce::Rectangle<float> enableRect()   { return { 8.0f, 8.0f, 20.0f, 20.0f }; }

        void drawSteps (juce::Graphics& g)
        {
            const auto& slot = seq.getSlot (selected);
            const auto count = slot.stepCount;
            const auto barW = gridArea.getWidth() / static_cast<float> (count);
            const auto maxH = gridArea.getHeight();

            g.setColour (juce::Colour (25, 25, 30));
            g.fillRect (gridArea);

            g.setColour (juce::Colour (45, 45, 55));
            for (int i = 4; i < count; i += 4)
                g.drawVerticalLine (static_cast<int> (gridArea.getX() + i * barW), gridArea.getY(), gridArea.getBottom());

            g.setColour (juce::Colour (40, 40, 50));
            for (const auto fraction : { 0.25f, 0.5f, 0.75f })
                g.drawHorizontalLine (static_cast<int> (gridArea.getBottom() - fraction * maxH), gridArea.getX(), gridArea.getRight());

            const auto active = seq.getSlotCurrentStep (selected);
            const auto live = seq.isPlaying() && slot.enabled;

            if (live && active >= 0 && active < count)
            {
                g.setColour (juce::Colours::white.withAlpha (0.08f));
                g.fillRect (gridArea.getX() + active * barW, gridArea.getY(), barW, maxH);
            }

            for (int i = 0; i < count; ++i)
            {
                const auto value = static_cast<float> (slot.steps[static_cast<size_t> (i)]) / 127.0f;
                const auto h = value * maxH;
                const auto x = gridArea.getX() + i * barW;

                if (h > 0.5f)
                {
                    auto colour = value < 0.33f ? juce::Colour (40, 120, 140) : value < 0.66f ? juce::Colour (60, 160, 80) : juce::Colour (140, 200, 60);

                    if (live && i == active)
                        colour = colour.brighter (0.4f);

                    g.setColour (colour);
                    g.fillRect (juce::Rectangle<float> (x + 1.0f, gridArea.getBottom() - h, barW - 2.0f, h));
                    g.setColour (colour.brighter (0.3f));
                    g.fillRect (x + 1.0f, gridArea.getBottom() - h, barW - 2.0f, 2.0f);
                }

                g.setColour (juce::Colour (55, 55, 65));
                g.drawRect (x, gridArea.getY(), barW, maxH, 0.5f);
            }

            g.setColour (juce::Colour (70, 70, 80));
            g.drawRect (gridArea, 1.0f);
        }

        void drawLanes (juce::Graphics& g, juce::Rectangle<int> strip)
        {
            const auto h = static_cast<float> (strip.getHeight()) / 16.0f;

            for (int i = 0; i < NodeTools::StepSeq::maxSlots; ++i)
            {
                const juce::Rectangle<float> r (static_cast<float> (strip.getX()), static_cast<float> (strip.getY()) + i * h, static_cast<float> (strip.getWidth()), h);
                const auto isSelected = i == selected;
                const auto enabled = seq.getSlot (i).enabled;

                g.setColour (isSelected ? juce::Colour (60, 90, 140) : enabled ? juce::Colour (50, 60, 70) : juce::Colour (42, 42, 48));
                g.fillRect (r.reduced (1.0f));
                g.setColour (isSelected ? juce::Colours::white : seq.slotHasData (i) ? juce::Colour (180, 180, 200) : juce::Colour (90, 90, 100));
                g.setFont (bold (11.0f));
                g.drawText (juce::String (i + 1), r, juce::Justification::centred);

                if (enabled && seq.isPlaying())
                {
                    g.setColour (juce::Colours::limegreen);
                    g.fillEllipse (r.getX() + 2.0f, r.getY() + 2.0f, 4.0f, 4.0f);
                }
            }
        }

        static juce::Rectangle<float> toolRect (juce::Rectangle<int> strip, int index)
        {
            return { static_cast<float> (strip.getX()) + 3.0f, static_cast<float> (strip.getY()) + 4.0f + static_cast<float> (index) * 33.0f,
                     static_cast<float> (strip.getWidth()) - 6.0f, 30.0f };
        }

        void drawTools (juce::Graphics& g, juce::Rectangle<int> strip)
        {
            static const char* labels[] = { "=", "^", "v", "<", ">", "C" };

            for (int i = 0; i < 6; ++i)
            {
                const auto colour = i == 0 ? juce::Colour (100, 100, 120) : i == 5 ? juce::Colour (180, 80, 80) : juce::Colour (80, 130, 180);
                const auto r = toolRect (strip, i);
                g.setColour (colour.darker (0.3f));
                g.fillRoundedRectangle (r, 3.0f);
                g.setColour (colour);
                g.drawRoundedRectangle (r, 3.0f, 1.0f);
                g.setColour (juce::Colours::white);
                g.setFont (bold (14.0f));
                g.drawText (labels[i], r, juce::Justification::centred);
            }
        }

        static constexpr int numControls = 11;

        static float controlWidth (int index)
        {
            static const float widths[numControls] = { 45, 55, 50, 55, 65, 75, 60, 55, 55, 60, 50 };
            return widths[index];
        }

        void drawControls (juce::Graphics& g, juce::Rectangle<int> bar)
        {
            const auto& slot = seq.getSlot (selected);
            const auto area = bar.reduced (6, 4);
            const auto signature = seq.isSyncedToSongTimeSig() ? juce::String ("Song")
                                                               : juce::String (seq.getTimeSigNumerator()) + "/" + juce::String (seq.getTimeSigDenominator());
            const juce::String labels[numControls] = { "Ch", "Steps", "CC#", "Rate", "Type", "Order", "Speed", "Swing", "Smooth", "T.Sig", "SyncTS" };
            const juce::String values[numControls] = { juce::String (slot.midiChannel), juce::String (slot.stepCount), juce::String (slot.ccNumber),
                                                       NodeTools::StepSeq::rateName (slot.rate), NodeTools::StepSeq::typeName (slot.type),
                                                       NodeTools::StepSeq::orderName (slot.order), NodeTools::StepSeq::speedName (slot.speed),
                                                       juce::String (static_cast<int> (slot.swing)) + "%", slot.smooth ? "On" : "Off",
                                                       signature, seq.isSyncedToSongTimeSig() ? "On" : "Off" };

            auto x = static_cast<float> (area.getX());
            const auto labelY = static_cast<float> (area.getY());

            for (int i = 0; i < numControls; ++i)
            {
                const auto w = controlWidth (i);
                g.setColour (juce::Colour (140, 140, 160));
                g.setFont (bold (9.0f));
                g.drawText (labels[i], juce::Rectangle<float> (x, labelY, w, 16.0f), juce::Justification::centred);

                const juce::Rectangle<float> value (x + 2.0f, labelY + 18.0f, w - 4.0f, 22.0f);
                g.setColour (i == 10 && seq.isSyncedToSongTimeSig() ? juce::Colour (30, 70, 30) : juce::Colour (30, 30, 38));
                g.fillRoundedRectangle (value, 3.0f);
                g.setColour (juce::Colour (70, 70, 85));
                g.drawRoundedRectangle (value, 3.0f, 1.0f);
                g.setColour (juce::Colours::white);
                g.setFont (bold (11.0f));
                g.drawText (values[i], value, juce::Justification::centred);
                x += w + 6.0f;
            }
        }

        void drawStepAt (juce::Point<float> pos)
        {
            auto& slot = seq.getSlot (selected);
            const auto barW = gridArea.getWidth() / static_cast<float> (slot.stepCount);
            const auto step = juce::jlimit (0, slot.stepCount - 1, static_cast<int> ((pos.x - gridArea.getX()) / barW));
            const auto normal = 1.0f - (pos.y - gridArea.getY()) / gridArea.getHeight();
            slot.steps[static_cast<size_t> (step)] = juce::jlimit (0, 127, static_cast<int> (normal * 127.0f));
            repaint();
        }

        /** Runs fn on the chosen lane later (after a menu), if the tool and this editor are still there. */
        template <typename Fn>
        auto later (Fn fn)
        {
            juce::Component::SafePointer<StepSeqEditor> safe (this);
            const auto lane = selected;

            return [safe, lane, fn] (int result)
            {
                if (safe == nullptr || ! safe->alive() || result == 0)
                    return;

                fn (*safe, safe->seq.getSlot (lane), result);
                safe->edited();
                safe->repaint();
            };
        }

        void controlClicked (juce::Point<float> pos, juce::Rectangle<int> bar)
        {
            const auto area = bar.reduced (6, 4);
            auto x = static_cast<float> (area.getX());
            const auto valueY = static_cast<float> (area.getY()) + 18.0f;

            for (int i = 0; i < numControls; ++i)
            {
                const auto w = controlWidth (i);

                if (juce::Rectangle<float> (x + 2.0f, valueY, w - 4.0f, 22.0f).contains (pos))
                {
                    controlMenu (i);
                    return;
                }

                x += w + 6.0f;
            }
        }

        void controlMenu (int control)
        {
            using Seq = NodeTools::StepSeq;
            auto& slot = seq.getSlot (selected);
            juce::PopupMenu menu;

            switch (control)
            {
                case 0:
                    for (int ch = 1; ch <= 16; ++ch)
                        menu.addItem (ch, "Ch " + juce::String (ch), true, ch == slot.midiChannel);

                    menu.showMenuAsync ({}, later ([] (StepSeqEditor&, Seq::Slot& s, int r) { s.midiChannel = r; }));
                    break;

                case 1:
                    for (size_t i = 0; i < Seq::allowedStepCounts.size(); ++i)
                        menu.addItem (static_cast<int> (i) + 1, juce::String (Seq::allowedStepCounts[i]), true, Seq::allowedStepCounts[i] == slot.stepCount);

                    menu.showMenuAsync ({}, later ([] (StepSeqEditor&, Seq::Slot& s, int r) { s.stepCount = Seq::allowedStepCounts[static_cast<size_t> (r - 1)]; }));
                    break;

                case 2:
                {
                    menu.addSectionHeader ("Common CCs");
                    const std::pair<int, const char*> common[] = { { 1, "1-Mod" }, { 7, "7-Vol" }, { 10, "10-Pan" }, { 11, "11-Expr" }, { 64, "64-Sustain" },
                                                                   { 71, "71-Res" }, { 74, "74-Cut" }, { 91, "91-Rev" }, { 93, "93-Cho" } };

                    for (const auto& [number, label] : common)
                        menu.addItem (1000 + number, label, true, number == slot.ccNumber);

                    menu.addSeparator();
                    juce::PopupMenu all;

                    for (int cc = 0; cc < 128; ++cc)
                        all.addItem (1000 + cc, "CC " + juce::String (cc), true, cc == slot.ccNumber);

                    menu.addSubMenu ("All CCs", all);
                    menu.showMenuAsync ({}, later ([] (StepSeqEditor&, Seq::Slot& s, int r) { s.ccNumber = juce::jlimit (0, 127, r - 1000); }));
                    break;
                }

                case 3:
                    for (int r = 0; r < Seq::numRates; ++r)
                        menu.addItem (r + 1, Seq::rateName (static_cast<Seq::RateDiv> (r)), true, r == static_cast<int> (slot.rate));

                    menu.showMenuAsync ({}, later ([] (StepSeqEditor&, Seq::Slot& s, int r) { s.rate = static_cast<Seq::RateDiv> (r - 1); }));
                    break;

                case 4:
                    for (int t = 0; t < Seq::numRateTypes; ++t)
                        menu.addItem (t + 1, Seq::typeName (static_cast<Seq::RateType> (t)), true, t == static_cast<int> (slot.type));

                    menu.showMenuAsync ({}, later ([] (StepSeqEditor&, Seq::Slot& s, int r) { s.type = static_cast<Seq::RateType> (r - 1); }));
                    break;

                case 5:
                    for (int o = 0; o < Seq::numOrders; ++o)
                        menu.addItem (o + 1, Seq::orderName (static_cast<Seq::StepOrder> (o)), true, o == static_cast<int> (slot.order));

                    menu.showMenuAsync ({}, later ([] (StepSeqEditor&, Seq::Slot& s, int r) { s.order = static_cast<Seq::StepOrder> (r - 1); }));
                    break;

                case 6:
                    for (int sp = 0; sp < Seq::numSpeedModes; ++sp)
                        menu.addItem (sp + 1, Seq::speedName (static_cast<Seq::SpeedMode> (sp)), true, sp == static_cast<int> (slot.speed));

                    menu.showMenuAsync ({}, later ([] (StepSeqEditor&, Seq::Slot& s, int r) { s.speed = static_cast<Seq::SpeedMode> (r - 1); }));
                    break;

                case 7:
                    for (int i = 0; i <= 10; ++i)
                        menu.addItem (i + 1, juce::String (i * 10) + "%", true, static_cast<int> (slot.swing) == i * 10);

                    menu.showMenuAsync ({}, later ([] (StepSeqEditor&, Seq::Slot& s, int r) { s.swing = static_cast<float> ((r - 1) * 10); }));
                    break;

                case 8:
                    slot.smooth = ! slot.smooth;
                    edited();
                    repaint();
                    break;

                case 9:
                    if (seq.isSyncedToSongTimeSig())
                    {
                        menu.addItem (1, "Follows the song (turn SyncTS off first)", false);
                        menu.showMenuAsync ({}, [] (int) {});
                        break;
                    }

                    menu.addSectionHeader ("Numerator");
                    for (int n = 1; n <= 12; ++n)
                        menu.addItem (n, juce::String (n), true, n == seq.getTimeSigNumerator());

                    menu.addSeparator();
                    menu.addSectionHeader ("Denominator");
                    for (int i = 0; i < 4; ++i)
                        menu.addItem (100 + i, juce::String (2 << i), true, (2 << i) == seq.getTimeSigDenominator());

                    menu.showMenuAsync ({}, later ([] (StepSeqEditor& editor, Seq::Slot&, int r)
                    {
                        if (r >= 1 && r <= 12)         editor.seq.setTimeSigNumerator (r);
                        else if (r >= 100 && r <= 103) editor.seq.setTimeSigDenominator (2 << (r - 100));
                    }));
                    break;

                case 10:
                    seq.setSyncToSongTimeSig (! seq.isSyncedToSongTimeSig());
                    edited();
                    repaint();
                    break;

                default:
                    break;
            }
        }

        void toolClicked (juce::Point<float> pos, juce::Rectangle<int> strip)
        {
            int index = -1;

            for (int i = 0; i < 6; ++i)
                if (toolRect (strip, i).contains (pos))
                    index = i;

            if (index < 0)
                return;

            auto& slot = seq.getSlot (selected);
            const auto count = slot.stepCount;
            auto& steps = slot.steps;

            switch (index)
            {
                case 0:
                {
                    juce::PopupMenu menu;
                    menu.addItem (1, "Copy Sequence");
                    menu.addItem (2, "Paste Sequence", seq.hasClipboardData());
                    menu.addSeparator();
                    menu.addItem (3, "Randomize");
                    menu.addItem (4, "Invert Values");
                    menu.addItem (5, "Set All to 64");
                    menu.addSeparator();
                    menu.addItem (6, "Clear Sequence");
                    const auto lane = selected;

                    menu.showMenuAsync ({}, later ([lane] (StepSeqEditor& editor, NodeTools::StepSeq::Slot& s, int r)
                    {
                        juce::Random random;

                        switch (r)
                        {
                            case 1: editor.seq.copySlotToClipboard (lane); break;
                            case 2: editor.seq.pasteSlotFromClipboard (lane); break;
                            case 3: for (int i = 0; i < s.stepCount; ++i) s.steps[static_cast<size_t> (i)] = random.nextInt (128); break;
                            case 4: for (int i = 0; i < s.stepCount; ++i) s.steps[static_cast<size_t> (i)] = 127 - s.steps[static_cast<size_t> (i)]; break;
                            case 5: for (int i = 0; i < s.stepCount; ++i) s.steps[static_cast<size_t> (i)] = 64; break;
                            case 6: editor.seq.clearSlot (lane); break;
                            default: break;
                        }
                    }));
                    return;
                }

                case 1: for (int i = 0; i < count; ++i) steps[static_cast<size_t> (i)] = juce::jmin (127, steps[static_cast<size_t> (i)] + 1); break;
                case 2: for (int i = 0; i < count; ++i) steps[static_cast<size_t> (i)] = juce::jmax (0, steps[static_cast<size_t> (i)] - 1); break;
                case 3: std::rotate (steps.begin(), steps.begin() + 1, steps.begin() + count); break;
                case 4: std::rotate (steps.begin(), steps.begin() + (count - 1), steps.begin() + count); break;
                case 5: seq.clearSlot (selected); break;
                default: break;
            }

            edited();
            repaint();
        }

        void stepMenu (juce::Point<float> pos)
        {
            const auto& slot = seq.getSlot (selected);
            const auto barW = gridArea.getWidth() / static_cast<float> (slot.stepCount);
            const auto step = juce::jlimit (0, slot.stepCount - 1, static_cast<int> ((pos.x - gridArea.getX()) / barW));

            juce::PopupMenu menu;
            menu.addItem (1, "Enter Value...");
            menu.addItem (2, "Set to 0");
            menu.addItem (3, "Set to 64");
            menu.addItem (4, "Set to 127");

            menu.showMenuAsync ({}, later ([step] (StepSeqEditor& editor, NodeTools::StepSeq::Slot& s, int r)
            {
                if (r == 2) s.steps[static_cast<size_t> (step)] = 0;
                if (r == 3) s.steps[static_cast<size_t> (step)] = 64;
                if (r == 4) s.steps[static_cast<size_t> (step)] = 127;
                if (r == 1) editor.askValue (step);
            }));
        }

        void askValue (int step)
        {
            const auto lane = selected;
            auto* dialog = new juce::AlertWindow ("Enter CC Value", "Step " + juce::String (step + 1) + " (0-127):", juce::MessageBoxIconType::NoIcon);
            dialog->addTextEditor ("value", juce::String (seq.getSlot (lane).steps[static_cast<size_t> (step)]));
            dialog->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
            dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            juce::Component::SafePointer<StepSeqEditor> safe (this);

            dialog->enterModalState (true, juce::ModalCallbackFunction::create ([safe, dialog, lane, step] (int result)
            {
                if (result == 1 && safe != nullptr && safe->alive())
                {
                    safe->seq.getSlot (lane).steps[static_cast<size_t> (step)] = juce::jlimit (0, 127, dialog->getTextEditorContents ("value").getIntValue());
                    safe->edited();
                    safe->repaint();
                }
            }), true);
        }

        NodeTools::StepSeq& seq;
        int selected = 0, lastStep = -1;
        bool wasPlaying = false, drawing = false;
        juce::Rectangle<float> gridArea;
    };

    //==============================================================================
    // The Transient Splitter's editor: its meters, then every setting.
    class TransientSplitterEditor final : public ToolEditor
    {
    public:
        TransientSplitterEditor (NodeTools::TransientSplitter& s, NodeRackPages::ToolLink toolLink)
            : ToolEditor (std::move (toolLink)), splitter (s)
        {
            setSize (440, 560);

            title.setText ("Transient Splitter", juce::dontSendNotification);
            title.setFont (bold (16.0f));
            title.setColour (juce::Label::textColourId, juce::Colours::white);
            title.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (title);

            section (detectionLabel, "DETECTION");
            slider (sensitivity, sensitivityLabel, "Sensitivity", 0.0, 1.0, 0.01, splitter.sensitivity, "");
            slider (decay, decayLabel, "Decay", 1.0, 500.0, 1.0, splitter.decay, " ms", 50.0);
            slider (hold, holdLabel, "Hold Time", 0.0, 100.0, 0.5, splitter.holdTime, " ms");
            slider (smooth, smoothLabel, "Smoothing", 0.1, 50.0, 0.1, splitter.smoothing, " ms", 5.0);

            section (focusLabel, "FREQUENCY FOCUS (Detection Only)");
            slider (highPass, highPassLabel, "High-Pass", 20.0, 20000.0, 1.0, splitter.focusHighPass, " Hz", 500.0);
            slider (lowPass, lowPassLabel, "Low-Pass", 20.0, 20000.0, 1.0, splitter.focusLowPass, " Hz", 2000.0);

            section (outputLabel, "OUTPUT");
            slider (transientGain, transientGainLabel, "Transient Gain", -60.0, 12.0, 0.1, splitter.transientGainDb, " dB");
            slider (sustainGain, sustainGainLabel, "Sustain Gain", -60.0, 12.0, 0.1, splitter.sustainGainDb, " dB");
            slider (balance, balanceLabel, "Balance", -1.0, 1.0, 0.01, splitter.balance, "");

            section (modeLabel, "MODE");
            toggle (stereoLink, "Stereo Link", splitter.stereoLinked, juce::Colour (0, 180, 255));
            toggle (gateMode, "Gate Mode (Hard Split)", splitter.gateMode, juce::Colour (255, 180, 0));
            toggle (invertMode, "Invert (Swap Transient/Sustain)", splitter.invertMode, juce::Colour (255, 80, 80));
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (12);
            title.setBounds (area.removeFromTop (26));
            area.removeFromTop (6);
            meterArea = area.removeFromTop (50);
            area.removeFromTop (6);

            const auto row = [&area] (juce::Slider& s, juce::Label& l)
            {
                auto r = area.removeFromTop (24);
                l.setBounds (r.removeFromLeft (120));
                s.setBounds (r);
                area.removeFromTop (2);
            };

            const auto head = [&area] (juce::Label& l)
            {
                l.setBounds (area.removeFromTop (18));
                area.removeFromTop (2);
            };

            head (detectionLabel);
            row (sensitivity, sensitivityLabel); row (decay, decayLabel); row (hold, holdLabel); row (smooth, smoothLabel);
            area.removeFromTop (4);
            head (focusLabel);
            row (highPass, highPassLabel); row (lowPass, lowPassLabel);
            area.removeFromTop (4);
            head (outputLabel);
            row (transientGain, transientGainLabel); row (sustainGain, sustainGainLabel); row (balance, balanceLabel);
            area.removeFromTop (4);
            head (modeLabel);
            stereoLink.setBounds (area.removeFromTop (22));
            gateMode.setBounds (area.removeFromTop (22));
            invertMode.setBounds (area.removeFromTop (22));
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (35, 35, 40));

            if (! alive() || meterArea.isEmpty())
                return;

            const auto area = meterArea.toFloat();
            const auto activity = splitter.transientActivity.load();
            const juce::Rectangle<float> led (area.getX(), area.getY() + 2.0f, 10.0f, 10.0f);
            g.setColour (juce::Colour::fromHSV (0.0f, 1.0f - activity * 0.3f, 0.3f + activity * 0.7f, 1.0f));
            g.fillEllipse (led);
            g.setColour (juce::Colour (100, 100, 110));
            g.drawEllipse (led, 1.0f);
            g.setColour (juce::Colour (160, 160, 180));
            g.setFont (plain (10.0f));
            g.drawText ("TRANSIENT", juce::Rectangle<float> (led.getRight() + 4.0f, led.getY() - 1.0f, 70.0f, 14.0f), juce::Justification::centredLeft);

            const auto halfW = (area.getWidth() - 10.0f) / 2.0f;
            const auto barY = area.getY() + 18.0f;
            constexpr float barH = 12.0f;
            const auto cyan = juce::Colour (0, 200, 255), amber = juce::Colour (255, 180, 50);
            meter (g, { area.getX(), barY, halfW, barH }, splitter.transientRmsL.load(), cyan, "T-L");
            meter (g, { area.getX(), barY + barH + 2.0f, halfW, barH }, splitter.transientRmsR.load(), cyan, "T-R");
            meter (g, { area.getX() + halfW + 10.0f, barY, halfW, barH }, splitter.sustainRmsL.load(), amber, "S-L");
            meter (g, { area.getX() + halfW + 10.0f, barY + barH + 2.0f, halfW, barH }, splitter.sustainRmsR.load(), amber, "S-R");
        }

    private:
        void tick() override   { repaint (meterArea); }

        static void meter (juce::Graphics& g, juce::Rectangle<float> r, float rms, juce::Colour colour, const juce::String& label)
        {
            g.setColour (juce::Colour (25, 25, 30));
            g.fillRoundedRectangle (r, 2.0f);
            const auto fill = juce::jlimit (0.0f, 1.0f, juce::jmap (juce::Decibels::gainToDecibels (rms, -60.0f), -60.0f, 0.0f, 0.0f, 1.0f));

            if (fill > 0.001f)
            {
                g.setColour (colour.withAlpha (0.8f));
                g.fillRoundedRectangle (r.reduced (1.0f).withWidth ((r.getWidth() - 2.0f) * fill), 1.5f);
            }

            g.setColour (juce::Colour (180, 180, 190));
            g.setFont (plain (9.0f));
            g.drawText (label, r.withTrimmedLeft (3.0f), juce::Justification::centredLeft);
        }

        void section (juce::Label& label, const juce::String& text)
        {
            label.setText (text, juce::dontSendNotification);
            label.setFont (bold (11.0f));
            label.setColour (juce::Label::textColourId, juce::Colour (120, 180, 255));
            addAndMakeVisible (label);
        }

        void slider (juce::Slider& s, juce::Label& label, const juce::String& name, double min, double max, double step,
                     std::atomic<float>& value, const juce::String& suffix, double midPoint = 0.0)
        {
            label.setText (name, juce::dontSendNotification);
            label.setFont (plain (11.0f));
            label.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
            addAndMakeVisible (label);

            s.setRange (min, max, step);

            if (midPoint > 0.0)
                s.setSkewFactorFromMidPoint (midPoint);

            s.setTextValueSuffix (suffix);
            s.setValue (value.load(), juce::dontSendNotification);
            s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 65, 20);
            s.setColour (juce::Slider::trackColourId, juce::Colour (80, 80, 100));
            s.setColour (juce::Slider::thumbColourId, juce::Colour (180, 180, 200));
            s.onValueChange = [this, &s, &value] { if (alive()) value.store (static_cast<float> (s.getValue())); };
            s.onDragEnd = [this] { edited(); };
            addAndMakeVisible (s);
        }

        void toggle (juce::ToggleButton& button, const juce::String& text, std::atomic<bool>& value, juce::Colour tick)
        {
            button.setButtonText (text);
            button.setToggleState (value.load(), juce::dontSendNotification);
            button.setColour (juce::ToggleButton::tickColourId, tick);
            button.onClick = [this, &button, &value]
            {
                if (! alive())
                    return;

                value.store (button.getToggleState());
                edited();
            };
            addAndMakeVisible (button);
        }

        NodeTools::TransientSplitter& splitter;
        juce::Rectangle<int> meterArea;
        juce::Label title, detectionLabel, focusLabel, outputLabel, modeLabel;
        juce::Slider sensitivity, decay, hold, smooth, highPass, lowPass, transientGain, sustainGain, balance;
        juce::Label sensitivityLabel, decayLabel, holdLabel, smoothLabel, highPassLabel, lowPassLabel, transientGainLabel, sustainGainLabel, balanceLabel;
        juce::ToggleButton stereoLink, gateMode, invertMode;
    };
}

std::unique_ptr<juce::Component> NodeRackPages::makeLatcherEditor (NodeTools::Latcher& latcher, ToolLink link)
{
    return std::make_unique<LatcherEditor> (latcher, std::move (link));
}

std::unique_ptr<juce::Component> NodeRackPages::makeStepSeqEditor (NodeTools::StepSeq& stepSeq, ToolLink link)
{
    return std::make_unique<StepSeqEditor> (stepSeq, std::move (link));
}

std::unique_ptr<juce::Component> NodeRackPages::makeTransientSplitterEditor (NodeTools::TransientSplitter& splitter, ToolLink link)
{
    return std::make_unique<TransientSplitterEditor> (splitter, std::move (link));
}

//==============================================================================
// The Mixer
class NodeRackMixer::Strip final : public juce::Component
{
public:
    enum class Kind { output, instrument, effect };

    /** A fader for the rack's output (node 0), or for one of its plugins' nodes. */
    Strip (NodeRackMixer& m, Kind k, juce::AudioProcessorGraph::NodeID id, const juce::String& stripName)
        : mixer (m), kind (k), nodeId (id), name (stripName)
    {
        fader.setSliderStyle (juce::Slider::LinearVertical);
        fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        fader.setRange (-60.0, 25.0, 0.1);
        fader.setSkewFactorFromMidPoint (-6.0);
        fader.setDoubleClickReturnValue (true, 0.0);
        fader.setColour (juce::Slider::thumbColourId, colour());
        fader.setColour (juce::Slider::trackColourId, colour().darker());
        fader.setColour (juce::Slider::backgroundColourId, juce::Colour (25, 25, 30));
        fader.setValue (toDb (readGain()), juce::dontSendNotification);
        fader.onValueChange = [this] { writeGain (fromDb (fader.getValue())); repaint(); };
        fader.onDragEnd = [this] { if (auto* c = rack()) c->toolEdited(); };
        addAndMakeVisible (fader);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (40, 34, 52));
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (juce::Colour (80, 60, 110));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

        // The level as dB, at the top
        const auto db = fader.getValue();
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.setFont (bold (10.0f));
        g.drawText (db <= -59.9 ? juce::String ("-inf") : (db > 0.0 ? "+" : "") + juce::String (db, 1), bounds.removeFromTop (18.0f), juce::Justification::centred);

        // The name, at the bottom
        auto label = bounds.removeFromBottom (30.0f).reduced (3.0f, 2.0f);
        g.setColour (colour());
        g.setFont (bold (10.0f));
        g.drawFittedText (name, label.toNearestInt(), juce::Justification::centred, 2, 0.8f);

        // The meter, beside the fader
        const auto m = meterBounds().toFloat();
        g.setColour (juce::Colour (20, 20, 24));
        g.fillRoundedRectangle (m, 2.0f);
        const auto fill = juce::jlimit (0.0f, 1.0f, juce::jmap (juce::Decibels::gainToDecibels (level, -60.0f), -60.0f, 6.0f, 0.0f, 1.0f));

        if (fill > 0.0f)
        {
            const auto bar = m.reduced (1.0f).withTop (m.getBottom() - 1.0f - (m.getHeight() - 2.0f) * fill);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff22cc66), m.getX(), m.getBottom(), juce::Colour (0xffff4444), m.getX(), m.getY(), false));
            g.fillRect (bar);
        }

        // 0 dB on the meter
        const auto zero = m.getBottom() - m.getHeight() * juce::jmap (0.0f, -60.0f, 6.0f, 0.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawHorizontalLine (static_cast<int> (zero), m.getX(), m.getRight());
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (4, 0);
        area.removeFromTop (18);
        area.removeFromBottom (30);
        area.removeFromRight (14);
        fader.setBounds (area.reduced (0, 4));
    }

    /** The meter, and the fader if the level was changed elsewhere. */
    void refresh()
    {
        const auto now = readLevel();

        if (std::abs (now - level) > 0.0005f)
        {
            level = now;
            repaint (meterBounds());
        }

        if (! fader.isMouseButtonDown())
        {
            const auto db = toDb (readGain());

            if (std::abs (db - fader.getValue()) > 0.05)
                fader.setValue (db, juce::dontSendNotification);
        }
    }

private:
    juce::Rectangle<int> meterBounds() const
    {
        return { getWidth() - 16, 22, 8, getHeight() - 22 - 34 };
    }

    juce::Colour colour() const
    {
        return kind == Kind::output ? juce::Colours::lime : kind == Kind::instrument ? juce::Colours::orange : juce::Colour (0xff66ccff);
    }

    static double toDb (float gain)     { return gain <= 0.001f ? -60.0 : juce::jlimit (-60.0, 25.0, static_cast<double> (juce::Decibels::gainToDecibels (gain))); }
    static float fromDb (double db)     { return db <= -59.9 ? 0.0f : juce::Decibels::decibelsToGain (static_cast<float> (db)); }

    ContainerPlugin* rack() const       { return mixer.builtFor; }

    PluginNode* node() const
    {
        auto* c = rack();
        auto* n = c != nullptr ? c->getGraph().getNodeForId (nodeId) : nullptr;
        return n != nullptr ? dynamic_cast<PluginNode*> (n->getProcessor()) : nullptr;
    }

    float readGain() const
    {
        if (kind == Kind::output)
            return rack() != nullptr ? rack()->getOutputGain() : 1.0f;

        auto* n = node();
        return n != nullptr ? n->getGain() : 1.0f;
    }

    float readLevel() const
    {
        if (kind == Kind::output)
            return rack() != nullptr ? rack()->getOutputLevel() : 0.0f;

        auto* n = node();
        return n != nullptr ? n->getLevel() : 0.0f;
    }

    void writeGain (float gain)
    {
        if (kind == Kind::output)
        {
            if (auto* c = rack())
                c->setOutputGain (gain);
        }
        else if (auto* n = node())
        {
            n->setGain (gain);
        }
    }

    NodeRackMixer& mixer;
    const Kind kind;
    const juce::AudioProcessorGraph::NodeID nodeId;
    const juce::String name;
    juce::Slider fader;
    float level = 0.0f;
};

NodeRackMixer::NodeRackMixer (std::function<ContainerPlugin&()> shownRack)
    : shown (std::move (shownRack))
{
    setOpaque (true);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (false, true);
    addAndMakeVisible (viewport);
    startTimerHz (25);
}

NodeRackMixer::~NodeRackMixer()
{
    stopTimer();
}

void NodeRackMixer::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (28, 14, 42));

    const auto top = viewport.getY() + 8;
    auto x = viewport.getX() + 12 - viewport.getViewPositionX();
    g.setFont (bold (11.0f));

    const auto heading = [&] (int count, const juce::String& title)
    {
        if (count == 0)
            return;

        const auto w = count * (stripWidth + stripGap) - stripGap;
        g.setColour (juce::Colour (200, 170, 255));
        g.drawText (title, x, top, juce::jmax (w, 80), labelHeight - 4, juce::Justification::centredLeft);
        g.setColour (juce::Colour (140, 70, 200));
        g.fillRect (x, top + labelHeight - 3, w, 1);
        x += w + sectionGap;
    };

    heading (outputCount, "OUTPUT");
    heading (instrumentCount, "INSTRUMENTS");
    heading (effectCount, "EFFECTS");

    if (instrumentCount + effectCount == 0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (plain (13.0f));
        g.drawText ("Plugins put in the rack get their faders here.", getLocalBounds().withTrimmedLeft (stripWidth + 40), juce::Justification::centred);
    }
}

void NodeRackMixer::resized()
{
    viewport.setBounds (getLocalBounds());

    const auto h = juce::jmax (160, viewport.getHeight() - viewport.getScrollBarThickness() - 4);
    auto x = 12;
    int index = 0;

    const auto place = [&] (int count)
    {
        for (int i = 0; i < count && index < strips.size(); ++i, ++index)
        {
            strips[index]->setBounds (x, 8 + labelHeight + 4, stripWidth, h - labelHeight - 20);
            x += stripWidth + stripGap;
        }

        if (count > 0)
            x += sectionGap - stripGap;
    };

    place (outputCount);
    place (instrumentCount);
    place (effectCount);
    content.setSize (juce::jmax (x + 12, viewport.getWidth()), h);
    repaint();
}

void NodeRackMixer::timerCallback()
{
    auto& rack = shown();

    if (&rack != builtFor || ContainerPlugin::getStructureGeneration() != builtAt)
        rebuild();

    if (isShowing())
        for (auto* strip : strips)
            strip->refresh();
}

void NodeRackMixer::rebuild()
{
    auto& rack = shown();
    builtFor = &rack;
    builtAt = ContainerPlugin::getStructureGeneration();
    strips.clear();

    struct Entry { juce::AudioProcessorGraph::NodeID id; juce::String name; float x; };
    std::vector<Entry> instruments, effects;

    for (auto* node : rack.getGraph().getNodes())
    {
        auto* pluginNode = dynamic_cast<PluginNode*> (node->getProcessor());

        if (pluginNode == nullptr)
            continue;

        const auto x = static_cast<float> (static_cast<double> (node->properties.getWithDefault ("x", 0.0)));
        const Entry entry { node->nodeID, pluginNode->getName(), x };
        (pluginNode->isInstrument() ? instruments : effects).push_back (entry);
    }

    // Left to right as they sit in the Rack.
    const auto byPlace = [] (const Entry& a, const Entry& b) { return a.x < b.x; };
    std::sort (instruments.begin(), instruments.end(), byPlace);
    std::sort (effects.begin(), effects.end(), byPlace);

    strips.add (new Strip (*this, Strip::Kind::output, {}, "Output"));

    for (const auto& entry : instruments)
        strips.add (new Strip (*this, Strip::Kind::instrument, entry.id, entry.name));

    for (const auto& entry : effects)
        strips.add (new Strip (*this, Strip::Kind::effect, entry.id, entry.name));

    for (auto* strip : strips)
        content.addAndMakeVisible (strip);

    outputCount = 1;
    instrumentCount = static_cast<int> (instruments.size());
    effectCount = static_cast<int> (effects.size());
    resized();
}

//==============================================================================
// The Manual
class NodeRackManual::Text final : public juce::Component
{
public:
    Text()
    {
        const auto h = [this] (const juce::String& s)  { parts.push_back ({ true, s }); };
        const auto p = [this] (const juce::String& s)  { parts.push_back ({ false, s }); };

        h ("Node Rack");
        p ("Node Rack is Sonify's modular plugin rack, built on Colosseum's node environment. It sits in a track like any "
           "plugin: Node Rack on an instrument slot (the track's notes come in, its sound goes out), Node Rack FX in the "
           "inserts (the track's sound comes in, with its sidechain, and goes out). Inside, any number of instruments, "
           "effects and system tools are wired together freely, and everything follows the song's tempo and time signature.");

        h ("The pages");
        p ("Rack: the node canvas, where plugins and tools sit and are wired.\n"
           "Mixer: a fader and a meter for the rack's output, for each instrument and for each effect in it.\n"
           "Manual: this page.");

        h ("Getting plugins in");
        p ("- Drag a plugin from Sonify's Browser (the right panel) and drop it on the Rack.\n"
           "- Browse (in the bar) picks a plugin file. It opens the VST3 folder the first time, then the folder last used.\n"
           "- Drop a plugin file (.vst3, .clap, .sip) from the computer's folders onto the Rack.\n"
           "- Right-click an empty spot: the System Tools, Sonify's instruments, effects and note FX, and the saved patches.");

        h ("Wiring");
        p ("Pins: blue for audio, green for sidechain, red for MIDI. Drag from an output pin to an input pin to wire them; "
           "an output can feed many inputs, and an input many outputs (they're summed).\n"
           "Drag a connected input pin to pick its cable up and move it. Double-click a cable, or right-click it, to delete it.\n"
           "Right-click a pin to see what it is. Right-click a node's title bar to disconnect all its wires or delete it.\n"
           "The rack's own nodes are its doors: MIDI Input (the track's notes), Audio Input (Node Rack FX: the track's sound "
           "and its sidechain), Audio Output and MIDI Output. Audio Input and Audio Output switch ON and OFF. MIDI Output sends "
           "what reaches it to other tracks that listen to this one.");

        h ("Node buttons");
        p ("E  opens the plugin's window (or enters a Container).\n"
           "CH  the MIDI channels an instrument hears.\n"
           "M  mutes the node (its notes are let go).\n"
           "P  passes the sound through an effect untouched.\n"
           "T  a tempo and time signature of its own instead of the song's.\n"
           "X  deletes the node.\n"
           "Right-click any button to see what it does.");

        h ("System Tools");
        p ("Connector: a stereo point to sum or split cables, with a volume (drag it) and a mute.\n"
           "Container: a rack inside the rack (FX Container takes audio in). E enters it, Back leaves it.\n"
           "Latcher: 4x4 pads that latch notes. Click a pad on the node (or play its trigger note) to hold its note; again to "
           "let it go. E opens the editor: right-click a pad to choose it, then drag its trigger note, output note, velocity "
           "and channel up and down (double-click one to put it back). ALL OFF lets every pad go.\n"
           "Step Seq: 16 lanes of CC steps. On the node: play and stop, the tempo (drag it when SYN is off) and SYN, which follows "
           "the song's tempo. E opens the editor: pick a lane on the left, draw its steps, turn it on with the green button, "
           "and set its channel, CC, steps, rate, type, order, speed, swing and smoothing along the bottom.\n"
           "Transient Splitter: a stereo sound in, its attacks out of pins 1-2 and what sustains out of pins 3-4. E opens its "
           "settings: sensitivity, decay, hold, smoothing, a detection band, the two gains, balance, gate and swap.\n"
           "Also: Stereo Meter, MIDI Monitor, Playlist Player, Sonify Sampler and Tomahawk.");

        h ("Tempo");
        p ("The rack follows the song: plugins and tools inside it get the song's tempo, time signature and position. A node's T "
           "(or a Container's) gives it a tempo of its own; the Step Seq's SYN chooses the song's tempo or its own.");

        h ("Mixer");
        p ("Each fader sets a level from silence to +25 dB (double-click: 0 dB); the meter beside it shows what plays. Output is the "
           "whole rack. The levels are saved with the song and with patches.");

        h ("Patches");
        p ("Save Patch keeps the whole rack in a .container file: every node with its settings and its plugin's state, every "
           "cable and the mixer's levels. Load Patch puts one back (over what's there). Reset empties the rack. When a Container "
           "is entered, these act on that Container.\n"
           "A plugin a patch needs that isn't on this computer is kept as it was (shown in red), so saving again loses nothing.");

        h ("PANIC");
        p ("Lets every note go: notes off and sound off on every channel, every plugin in the rack (and in its Containers) "
           "releases what it holds, and every Latcher pad opens.");

        h ("Moving around");
        p ("Drag an empty spot to pan; the mouse wheel scrolls (Shift: sideways), Ctrl and the wheel zooms. The minimap jumps "
           "anywhere. Nodes snap to the ones they're wired to; hold Shift while dragging to place one freely.");
    }

    /** Lays the text out for a width; returns the height it takes. */
    int layout (int width)
    {
        layouts.clear();
        auto y = 20.0f;
        const auto w = static_cast<float> (juce::jmax (200, width - 48));

        for (const auto& part : parts)
        {
            juce::AttributedString text;
            text.setWordWrap (juce::AttributedString::byWord);
            text.setLineSpacing (part.heading ? 0.0f : 3.0f);
            text.append (part.text, part.heading ? bold (part.text == "Node Rack" ? 22.0f : 15.0f) : plain (13.5f),
                         part.heading ? juce::Colour (200, 170, 255) : juce::Colour (215, 210, 225));

            juce::TextLayout layout;
            layout.createLayout (text, w);

            if (part.heading)
                y += 10.0f;

            layouts.push_back ({ std::move (layout), y });
            y += layouts.back().layout.getHeight() + (part.heading ? 6.0f : 8.0f);
        }

        return static_cast<int> (y + 30.0f);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (28, 14, 42));

        for (const auto& placed : layouts)
            placed.layout.draw (g, { 24.0f, placed.y, static_cast<float> (getWidth()) - 48.0f, placed.layout.getHeight() });
    }

private:
    struct Part { bool heading; juce::String text; };
    struct Placed { juce::TextLayout layout; float y; };

    std::vector<Part> parts;
    std::vector<Placed> layouts;
};

NodeRackManual::NodeRackManual()
    : text (std::make_unique<Text>())
{
    setOpaque (true);
    viewport.setViewedComponent (text.get(), false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
}

NodeRackManual::~NodeRackManual() = default;

void NodeRackManual::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (28, 14, 42));
}

void NodeRackManual::resized()
{
    viewport.setBounds (getLocalBounds());
    const auto width = juce::jmin (900, viewport.getWidth() - viewport.getScrollBarThickness());
    text->setSize (width, text->layout (width));
}

//==============================================================================
// The Macros strip
namespace
{
    const juce::Colour macroBack (34, 16, 50), macroEdge (140, 70, 200), macroText (210, 190, 240), macroDim (150, 120, 190);
    const juce::Colour macroLearn (230, 160, 40);

    /** The macro knobs' look: a ring with the value lit along it, and a pointer. */
    class MacroKnobLook final : public juce::LookAndFeel_V4
    {
    public:
        void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float position,
                               float startAngle, float endAngle, juce::Slider&) override
        {
            const auto size = static_cast<float> (juce::jmin (width, height)) - 4.0f;
            const auto centre = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                        static_cast<float> (width), static_cast<float> (height)).getCentre();
            const auto radius = size / 2.0f;
            const auto ring = juce::jmax (3.0f, radius * 0.16f);
            const auto angle = startAngle + position * (endAngle - startAngle);

            g.setColour (juce::Colour (24, 12, 36));
            g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (centre));

            juce::Path track;
            track.addCentredArc (centre.x, centre.y, radius - ring, radius - ring, 0.0f, startAngle, endAngle, true);
            g.setColour (juce::Colour (70, 40, 100));
            g.strokePath (track, juce::PathStrokeType (ring, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            if (position > 0.001f)
            {
                juce::Path lit;
                lit.addCentredArc (centre.x, centre.y, radius - ring, radius - ring, 0.0f, startAngle, angle, true);
                g.setColour (juce::Colour (180, 120, 255));
                g.strokePath (lit, juce::PathStrokeType (ring, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            const auto inner = radius - ring * 2.2f;
            const juce::Point<float> tip (centre.x + inner * std::sin (angle), centre.y - inner * std::cos (angle));
            g.setColour (juce::Colours::white);
            g.drawLine ({ centre.getPointOnCircumference (inner * 0.35f, angle), tip }, 2.0f);

        }
    };

    MacroKnobLook& macroKnobLook()
    {
        static MacroKnobLook look;
        return look;
    }

    /** A link's range and invert, in a callout by the menu. */
    class MacroRangeEditor final : public juce::Component
    {
    public:
        MacroRangeEditor (ContainerPlugin& r, const ContainerPlugin::MacroLink& l) : rack (r), link (l)
        {
            title.setText (rack.describeMacroLink (link), juce::dontSendNotification);
            title.setFont (bold (13.0f));
            title.setColour (juce::Label::textColourId, juce::Colours::white);
            addAndMakeVisible (title);

            for (auto* slider : { &from, &to })
            {
                slider->setSliderStyle (juce::Slider::LinearHorizontal);
                slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
                slider->setRange (0.0, 100.0, 1.0);
                slider->setTextValueSuffix ("%");
                slider->setColour (juce::Slider::trackColourId, macroEdge);
                slider->onValueChange = [this] { changed(); };
                addAndMakeVisible (*slider);
            }

            from.setValue (link.from * 100.0, juce::dontSendNotification);
            to.setValue (link.to * 100.0, juce::dontSendNotification);

            for (auto* label : { &fromLabel, &toLabel })
            {
                label->setFont (plain (12.0f));
                label->setColour (juce::Label::textColourId, macroText);
                addAndMakeVisible (*label);
            }

            fromLabel.setText ("Macro at 0", juce::dontSendNotification);
            toLabel.setText ("Macro at 100", juce::dontSendNotification);

            invert.setButtonText ("Invert: the macro turned up turns it down");
            invert.setToggleState (link.inverted, juce::dontSendNotification);
            invert.setColour (juce::ToggleButton::textColourId, macroText);
            invert.onClick = [this] { changed(); };
            addAndMakeVisible (invert);

            setSize (360, 132);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (10, 8);
            title.setBounds (area.removeFromTop (22));
            area.removeFromTop (4);

            for (auto [label, slider] : { std::pair<juce::Label*, juce::Slider*> { &fromLabel, &from }, { &toLabel, &to } })
            {
                auto row = area.removeFromTop (26);
                label->setBounds (row.removeFromLeft (92));
                slider->setBounds (row);
            }

            area.removeFromTop (4);
            invert.setBounds (area.removeFromTop (24));
        }

    private:
        void changed()
        {
            // The link as the rack has it now (another may have gone meanwhile).
            const auto& links = rack.getMacroLinks();

            for (size_t i = 0; i < links.size(); ++i)
            {
                if (! links[i].sameControl (link))
                    continue;

                link.from = static_cast<float> (from.getValue() / 100.0);
                link.to = static_cast<float> (to.getValue() / 100.0);
                link.inverted = invert.getToggleState();
                rack.setMacroLink (static_cast<int> (i), link);
                return;
            }
        }

        ContainerPlugin& rack;
        ContainerPlugin::MacroLink link;
        juce::Label title, fromLabel, toLabel;
        juce::Slider from, to;
        juce::ToggleButton invert;
    };
}

/** One macro: its name, its knob, Learn and its menu. */
class NodeRackMacros::Cell final : public juce::Component
{
public:
    Cell (NodeRackMacros& o, int m) : owner (o), macro (m)
    {
        name.setJustificationType (juce::Justification::centred);
        name.setFont (bold (11.0f));
        name.setColour (juce::Label::textColourId, macroText);
        name.setColour (juce::Label::textWhenEditingColourId, juce::Colours::white);
        name.setColour (juce::Label::backgroundWhenEditingColourId, juce::Colour (20, 10, 30));
        name.setEditable (false, true, false);
        name.setTooltip ("Double-click to rename the macro");
        name.onTextChange = [this] { owner.rack.setMacroName (macro, name.getText()); };
        addAndMakeVisible (name);

        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        knob.setRange (0.0, 1.0, 0.0);
        knob.setDoubleClickReturnValue (true, 0.0);
        knob.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (170, 110, 240));
        knob.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (60, 34, 86));
        knob.setColour (juce::Slider::thumbColourId, juce::Colours::white);
        knob.onValueChange = [this] { owner.rack.setMacroValue (macro, knob.getValue()); };
        knob.setLookAndFeel (&macroKnobLook());
        knob.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
        knob.setPopupDisplayEnabled (true, true, nullptr);
        addAndMakeVisible (knob);

        learn.setButtonText ("Learn");
        learn.setTooltip ("Learn: then move a control in the window of a plugin in this rack; the macro drives it");
        learn.onClick = [this] { owner.startLearning (owner.learning == macro ? -1 : macro); };
        addAndMakeVisible (learn);

        menu.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xbe"));
        menu.setTooltip ("Assign a control from a list; each control's range and invert; take one away; rename");
        menu.onClick = [this] { owner.showMenu (macro, menu); };
        addAndMakeVisible (menu);

        for (auto* button : { &learn, &menu })
        {
            button->setColour (juce::TextButton::buttonColourId, juce::Colour (52, 24, 78));
            button->setColour (juce::TextButton::textColourOffId, macroText);
        }
    }

    ~Cell() override
    {
        knob.setLookAndFeel (nullptr);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (3, 2);
        name.setBounds (area.removeFromTop (18));
        auto buttons = area.removeFromBottom (20);
        menu.setBounds (buttons.removeFromRight (24));
        buttons.removeFromRight (3);
        learn.setBounds (buttons);
        knob.setBounds (area.reduced (0, 1));
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (learning ? macroLearn.withAlpha (0.18f) : juce::Colour (44, 22, 64));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f);
        g.setColour (learning ? macroLearn : juce::Colour (90, 50, 130));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f, 1.0f);

        // How many controls it drives, in the corner.
        if (count > 0)
        {
            g.setColour (macroDim);
            g.setFont (plain (10.0f));
            g.drawText (juce::String (count), getLocalBounds().reduced (6, 0).withTrimmedTop (getHeight() - 42).withHeight (14),
                        juce::Justification::centredRight);
        }
    }

    /** As the rack has it now (the song's automation moves the knob too). */
    void refresh()
    {
        if (! name.isBeingEdited() && name.getText() != owner.rack.getMacroName (macro))
            name.setText (owner.rack.getMacroName (macro), juce::dontSendNotification);

        if (! knob.isMouseButtonDown() && std::abs (knob.getValue() - owner.rack.getMacroValue (macro)) > 1.0e-6)
            knob.setValue (owner.rack.getMacroValue (macro), juce::dontSendNotification);

        int links = 0;

        for (const auto& link : owner.rack.getMacroLinks())
            links += link.macro == macro ? 1 : 0;

        const auto nowLearning = owner.learning == macro;

        if (links != count || nowLearning != learning)
        {
            count = links;
            learning = nowLearning;
            learn.setButtonText (learning ? "Move one..." : "Learn");
            learn.setColour (juce::TextButton::buttonColourId, learning ? macroLearn.darker (0.3f) : juce::Colour (52, 24, 78));
            knob.setTooltip (owner.rack.getMacroName (macro) + ": " + (count == 0 ? juce::String ("drives nothing yet (Learn, or its menu)")
                                                                                   : "drives " + juce::String (count) + (count == 1 ? " control" : " controls")));
            repaint();
        }
    }

    void rename()                       { name.showEditor(); }

private:
    NodeRackMacros& owner;
    const int macro;
    juce::Label name;
    juce::Slider knob;
    juce::TextButton learn, menu;
    int count = -1;
    bool learning = false;
};

NodeRackMacros::NodeRackMacros (ContainerPlugin& r) : rack (r)
{
    setOpaque (true);

    for (int i = 0; i < ContainerPlugin::numMacros; ++i)
        addAndMakeVisible (cells.add (new Cell (*this, i)));

    startTimerHz (15);
}

NodeRackMacros::~NodeRackMacros()
{
    stopTimer();
}

void NodeRackMacros::paint (juce::Graphics& g)
{
    g.fillAll (macroBack);
    g.setColour (macroEdge);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    g.setColour (juce::Colour (200, 170, 255));
    g.setFont (bold (12.0f));
    g.drawText ("MACROS", 12, 10, 76, 18, juce::Justification::centredLeft);
    g.setColour (macroDim);
    g.setFont (plain (10.5f));
    g.drawFittedText (learning >= 0 ? "Move a control in a plugin's window in this rack (Learn again: stop)"
                                    : "Each knob drives any controls in the rack: Learn, or its menu",
                      12, 30, 76, 66, juce::Justification::topLeft, 6);
}

void NodeRackMacros::resized()
{
    auto area = getLocalBounds().reduced (6, 6);
    area.removeFromLeft (88);
    const auto width = area.getWidth() / ContainerPlugin::numMacros;

    for (auto* cell : cells)
        cell->setBounds (area.removeFromLeft (width).reduced (2, 0));
}

void NodeRackMacros::timerCallback()
{
    // Learn: the first control moved in a plugin's window in this rack is the macro's.
    if (learning >= 0)
    {
        juce::uint32 node = 0;
        uint32_t param = 0;

        if (rack.takeTouchedControl (node, param))
        {
            ContainerPlugin::MacroLink link;
            link.macro = learning;
            link.kind = ContainerPlugin::MacroLink::Kind::parameter;
            link.nodeUid = node;
            link.paramId = param;
            rack.addMacroLink (link);
            learning = -1;
            repaint();
        }
        else if (juce::Time::getMillisecondCounter() - learningSince > 60000)
        {
            learning = -1;   // a minute and nothing moved: Learn gives up
            repaint();
        }
    }

    for (auto* cell : cells)
        cell->refresh();
}

void NodeRackMacros::startLearning (int macro)
{
    learning = macro;
    learningSince = juce::Time::getMillisecondCounter();

    if (macro >= 0)
        rack.forgetTouched();   // only what's touched from now on

    repaint();
}

void NodeRackMacros::showMenu (int macro, juce::Component& anchor)
{
    using Link = ContainerPlugin::MacroLink;
    auto choices = std::make_shared<std::vector<Link>>();

    const auto choice = [&choices, macro] (Link::Kind kind, juce::uint32 node, uint32_t param)
    {
        Link link;
        link.macro = macro;
        link.kind = kind;
        link.nodeUid = node;
        link.paramId = param;
        choices->push_back (link);
        return static_cast<int> (choices->size());   // its menu id
    };

    // Every control in the rack: each plugin's parameters and its Mixer level, and the Output.
    juce::PopupMenu assign;

    for (auto* node : rack.getGraph().getNodes())
    {
        auto* pluginNode = dynamic_cast<PluginNode*> (node->getProcessor());

        if (pluginNode == nullptr)
            continue;

        auto& plugin = pluginNode->getPlugin();
        juce::PopupMenu controls;
        controls.addItem (choice (Link::Kind::level, node->nodeID.uid, 0), "Level (its fader in the Mixer)");
        controls.addSeparator();

        int shown = 0;

        for (const auto& p : plugin.getParameters())
        {
            if (p.hidden || p.readOnly || p.name.isEmpty())
                continue;

            if (++shown > 400)
            {
                controls.addItem (-1, "(more: use Learn)", false);
                break;
            }

            controls.addItem (choice (Link::Kind::parameter, node->nodeID.uid, p.id), p.name);
        }

        assign.addSubMenu (plugin.getDescriptor().name, controls);
    }

    assign.addSeparator();
    assign.addItem (choice (Link::Kind::output, 0, 0), "Rack Output (the Mixer's Output fader)");

    juce::PopupMenu menu;
    menu.addSectionHeader (rack.getMacroName (macro));
    menu.addSubMenu ("Assign a Control", assign);

    // What it drives now: each one's range and invert, or taken away.
    constexpr int rangeBase = 100000, removeBase = 200000, renameId = 300000;
    const auto& links = rack.getMacroLinks();
    bool any = false;

    for (size_t i = 0; i < links.size(); ++i)
    {
        if (links[i].macro != macro)
            continue;

        if (! any)
        {
            menu.addSeparator();
            menu.addSectionHeader ("It drives");
            any = true;
        }

        juce::PopupMenu each;
        each.addItem (rangeBase + static_cast<int> (i), "Range and Invert...");
        each.addItem (removeBase + static_cast<int> (i), "Take Away");
        const auto range = juce::String (juce::roundToInt (links[i].from * 100.0f)) + "-" + juce::String (juce::roundToInt (links[i].to * 100.0f)) + "%"
                         + (links[i].inverted ? ", inverted" : "");
        menu.addSubMenu (rack.describeMacroLink (links[i]) + "  (" + range + ")", each);
    }

    menu.addSeparator();
    menu.addItem (renameId, "Rename...");

    auto safe = juce::Component::SafePointer<NodeRackMacros> (this);
    auto anchorSafe = juce::Component::SafePointer<juce::Component> (&anchor);
    const auto linksNow = links;   // as they were when the menu opened

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&anchor), [safe, anchorSafe, choices, linksNow, macro] (int id)
    {
        if (safe == nullptr || id <= 0)
            return;

        auto& rack = safe->rack;

        if (id == renameId)
        {
            safe->cells[macro]->rename();
            return;
        }

        // A link from the menu, if it's still there (found again by what it drives).
        const auto findNow = [&rack] (const Link& link) -> int
        {
            const auto& now = rack.getMacroLinks();

            for (size_t i = 0; i < now.size(); ++i)
                if (now[i].sameControl (link))
                    return static_cast<int> (i);

            return -1;
        };

        if (id >= removeBase && id < renameId)
        {
            const auto index = static_cast<size_t> (id - removeBase);

            if (index < linksNow.size())
                rack.removeMacroLink (findNow (linksNow[index]));

            return;
        }

        if (id >= rangeBase && id < removeBase)
        {
            const auto index = static_cast<size_t> (id - rangeBase);

            if (index < linksNow.size() && findNow (linksNow[index]) >= 0 && anchorSafe != nullptr)
                safe->editRange (rack.getMacroLinks()[static_cast<size_t> (findNow (linksNow[index]))], *anchorSafe);

            return;
        }

        if (id >= 1 && id <= static_cast<int> (choices->size()))
            rack.addMacroLink ((*choices)[static_cast<size_t> (id - 1)]);
    });
}

void NodeRackMacros::editRange (const ContainerPlugin::MacroLink& link, juce::Component& anchor)
{
    // Inside the rack's window: it goes with the window (and with the rack).
    auto editor = std::make_unique<MacroRangeEditor> (rack, link);
    auto* top = getTopLevelComponent();
    juce::CallOutBox::launchAsynchronously (std::move (editor), top->getLocalArea (&anchor, anchor.getLocalBounds()), top);
}
