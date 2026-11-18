// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakGraphUi.cpp
#include "ZodiakGraphUi.h"
#include "ZodiakLinkUi.h"
#include <juce_dsp/juce_dsp.h>

namespace zodiak::ui
{
    using graph::Node;
    using graph::Wire;

    namespace
    {
        //======================================================================
        //  The look (Colosseum's canvas, in Zodiak's graphite)
        //======================================================================
        constexpr float kS = 1.25f;                         // the nodes' size and their text: 125%
        constexpr float kNodeW = 168.0f * kS, kNodeH = 78.0f * kS, kPinR = 8.5f * kS, kGrid = 40.0f;

        juce::Colour typeColour (int t)
        {
            switch (t)
            {
                case graph::ntInstrument: return juce::Colour (0xFFE8A13A);
                case graph::ntPlugin:     return juce::Colour (0xFFA77BE0);
                case graph::ntVowel:      return juce::Colour (0xFF3CC2B0);
                case graph::ntFilter:     return juce::Colour (0xFF4C8DF0);
                case graph::ntGate:       return juce::Colour (0xFFF07A3C);
                case graph::ntDelay:      return juce::Colour (0xFF5FBF5A);
                case graph::ntSaturator:  return juce::Colour (0xFFE0504A);
                case graph::ntReverb:     return juce::Colour (0xFF7A6CF0);
                case graph::ntDynEq:      return juce::Colour (0xFF34B6E0);
                case graph::ntPhaser:     return juce::Colour (0xFFE060B0);
                case graph::ntWah:        return juce::Colour (0xFFB6D23C);
                case graph::ntFeedback:   return juce::Colour (0xFFFFC23A);
                case graph::ntKey:        return juce::Colour (0xFF9AA3AD);
                default:                  return juce::Colour (0xFFE8EAED);
            }
        }

        const juce::Colour kCanvasBg  { 0xFF15171A };
        const juce::Colour kGridLine  { 0xFF1F2226 };
        const juce::Colour kNodeBg    { 0xFF2A2E34 };
        const juce::Colour kWireMain  { 0xFF7FD0A0 };
        const juce::Colour kWireSc    { 0xFFF0A040 };
    }

    //==========================================================================
    juce::String nodeWindowKey (int ch, int uid)   { return "node:" + juce::String (ch) + ":" + juce::String (uid); }

    void openNodePlugin (ZodiakProcessor& p, WindowHub& hub, int ch, int uid, juce::Component* near)
    {
        auto* n = p.engine().graph (ch).find (uid);
        if (n == nullptr || ! graph::isPluginType (n->type)) return;
        if (n->plugin == nullptr || n->plugin->instance == nullptr) { chooseNodePlugin (p, hub, ch, uid, near); return; }
        hub.showPluginEditor (nodeWindowKey (ch, uid), n->plugin->instance.get(),
                              "Ch " + juce::String (ch + 1) + "  -  " + n->plugin->name, near, p, ch);
    }

    void chooseNodePlugin (ZodiakProcessor& p, WindowHub& hub, int ch, int uid, juce::Component* near)
    {
        auto* n = p.engine().graph (ch).find (uid);
        if (n == nullptr) return;
        const bool instrument = n->type == graph::ntInstrument;
        juce::Component::SafePointer<juce::Component> safeNear (near);
        auto* proc = &p;
        auto* windows = &hub;
        rack::pickSource (instrument, near,
            [proc, windows, ch, uid, safeNear] (const rack::Source& source)
            {
                windows->close (nodeWindowKey (ch, uid));
                proc->loadNodePlugin (ch, uid, source, [proc, windows, ch, uid, safeNear] (const juce::String& error)
                {
                    if (error.isNotEmpty())
                    {
                        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Not loaded", error, "OK");
                        return;
                    }
                    // A plugin just loaded opens its window, this once; from then on by hand
                    openNodePlugin (*proc, *windows, ch, uid, safeNear.getComponent());
                });
            });
    }

    //==========================================================================
    //  The canvas
    //==========================================================================
    class GraphCanvas final : public juce::Component,
                              private juce::Timer
    {
    public:
        GraphCanvas (ZodiakProcessor& p, WindowHub& h, int channel, GraphEditor& ed)
            : proc (p), hub (h), ch (channel), editor (ed)
        {
            setWantsKeyboardFocus (true);
            startTimerHz (30);
        }

        ~GraphCanvas() override   { stopTimer(); }

        int selectedNode() const noexcept   { return selected; }

        //======================================================================
        void paint (juce::Graphics& g) override
        {
            g.fillAll (kCanvasBg);

            // The grid
            g.setColour (kGridLine);
            const float step = kGrid * zoom;
            if (step >= 6.0f)
            {
                for (float x = std::fmod (-pan.x * zoom, step); x < (float) getWidth(); x += step)
                    g.drawVerticalLine ((int) x, 0.0f, (float) getHeight());
                for (float y = std::fmod (-pan.y * zoom, step); y < (float) getHeight(); y += step)
                    g.drawHorizontalLine ((int) y, 0.0f, (float) getWidth());
            }

            auto& gr = graphRef();

            // The cables, under the nodes
            for (auto& w : gr.wireList())
            {
                const auto* s = gr.find (w.src);
                const auto* d = gr.find (w.dst);
                if (s == nullptr || d == nullptr) continue;
                const bool fb = d->type == graph::ntFeedback;
                const bool chosen = wireChosen && w == chosenWire;
                drawWire (g, outPin (*s), inPin (*d, w.pin), w.pin == graph::kPinSc ? kWireSc : kWireMain, chosen, fb);
            }

            if (dragging == Drag::cable)
            {
                if (const auto* s = gr.find (cableFrom))
                {
                    const bool okHere = hoverPin.uid != 0;
                    drawWire (g, outPin (*s), cableEnd, okHere ? juce::Colours::white : kWireMain.withAlpha (0.7f), okHere, false);
                }
            }

            for (auto& n : gr.nodeList()) drawNode (g, *n);

            // The graph's latency, and what the canvas last had to say
            g.setFont (font (14.0f));
            if (const int lat = gr.latency(); lat > 0)
            {
                g.setColour (win::dim);
                g.drawText ("Latency " + juce::String (lat) + " samples (" + juce::String (1000.0 * lat / juce::jmax (1.0, proc.engine().sampleRate()), 1)
                            + " ms), every branch kept in time", getLocalBounds().reduced (12, 8), juce::Justification::topRight, false);
            }
            if (message.isNotEmpty() && juce::Time::getMillisecondCounter() < messageUntil)
            {
                auto r = getLocalBounds().reduced (12).removeFromBottom (34);
                g.setColour (juce::Colour (0xE0303438));
                g.fillRoundedRectangle (r.toFloat(), 8.0f);
                g.setColour (juce::Colour (0xFFFFC23A));
                g.setFont (font (16.0f, true));
                g.drawText (message, r.reduced (12, 0), juce::Justification::centredLeft, false);
            }
            else
            {
                g.setColour (win::dim.withAlpha (0.7f));
                g.setFont (font (13.0f));
                g.drawText ("Right-click: add a node.  Drag from an output to an input: a cable.  Ctrl + wheel: zoom.",
                            getLocalBounds().reduced (12, 8), juce::Justification::bottomLeft, false);
            }
        }

        //======================================================================
        void mouseDown (const juce::MouseEvent& e) override
        {
            grabKeyboardFocus();
            const auto pos = e.position;
            auto& gr = graphRef();

            if (e.mods.isPopupMenu())
            {
                if (auto* n = nodeAt (pos)) { choose (n->uid); nodeMenu (*n); return; }
                if (const auto w = wireAt (pos); w.src != 0) { wireChosen = true; chosenWire = w; repaint(); wireMenu (w); return; }
                addMenu (toGraph (pos));
                return;
            }

            // An output: a new cable
            if (const auto pin = pinAt (pos, false); pin.uid != 0)
            {
                dragging = Drag::cable;
                cableFrom = pin.uid;
                cableEnd = pos;
                return;
            }

            // An input: its last cable picked up
            if (const auto pin = pinAt (pos, true); pin.uid != 0)
            {
                for (auto it = gr.wireList().rbegin(); it != gr.wireList().rend(); ++it)
                    if (it->dst == pin.uid && it->pin == pin.pin)
                    {
                        const auto w = *it;
                        gr.disconnect (w);
                        dragging = Drag::cable;
                        cableFrom = w.src;
                        cableEnd = pos;
                        repaint();
                        return;
                    }
                return;
            }

            if (auto* n = nodeAt (pos))
            {
                const auto r = nodeRect (*n);
                if (bypassRect (r).expanded (4.0f).contains (pos))
                {
                    n->on.store (! n->on.load());
                    repaint();
                    return;
                }
                if (graph::isPluginType (n->type) && editRect (r).contains (pos))
                {
                    openNodePlugin (proc, hub, ch, n->uid, this);
                    return;
                }
                choose (n->uid);
                dragging = Drag::node;
                dragUid = n->uid;
                dragOffset = toGraph (pos) - n->pos;
                return;
            }

            if (const auto w = wireAt (pos); w.src != 0)
            {
                wireChosen = true;
                chosenWire = w;
                repaint();
                return;
            }

            wireChosen = false;
            dragging = Drag::view;
            panStart = pan;
            repaint();
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            switch (dragging)
            {
                case Drag::cable:
                    cableEnd = e.position;
                    hoverPin = pinAt (e.position, true);
                    repaint();
                    break;
                case Drag::node:
                    if (auto* n = graphRef().find (dragUid))
                    {
                        auto at = toGraph (e.position) - dragOffset;
                        if (! e.mods.isAltDown())          // the grid's half steps
                            at = { std::round (at.x / (kGrid * 0.5f)) * kGrid * 0.5f, std::round (at.y / (kGrid * 0.5f)) * kGrid * 0.5f };
                        n->pos = at;
                        repaint();
                    }
                    break;
                case Drag::view:
                    pan = panStart - e.getOffsetFromDragStart().toFloat() / zoom;
                    repaint();
                    break;
                case Drag::none:
                    break;
            }
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (dragging == Drag::cable)
            {
                const auto pin = pinAt (e.position, true);
                if (pin.uid != 0) connect (cableFrom, pin.uid, pin.pin);
                hoverPin = {};
            }
            dragging = Drag::none;
            repaint();
        }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (auto* n = nodeAt (e.position))
            {
                if (graph::isPluginType (n->type)) openNodePlugin (proc, hub, ch, n->uid, this);
                else                               choose (n->uid);
            }
        }

        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            if (e.mods.isCtrlDown() || e.mods.isCommandDown())
            {
                const auto before = toGraph (e.position);
                zoom = juce::jlimit (0.35f, 1.6f, zoom * (w.deltaY > 0 ? 1.1f : 1.0f / 1.1f));
                pan = before - e.position / zoom;
            }
            else
                pan -= juce::Point<float> (w.deltaX, w.deltaY) * (300.0f / zoom);
            repaint();
        }

        bool keyPressed (const juce::KeyPress& k) override
        {
            if (k.isKeyCode (juce::KeyPress::deleteKey) || k.isKeyCode (juce::KeyPress::backspaceKey))
            {
                if (wireChosen) { graphRef().disconnect (chosenWire); wireChosen = false; repaint(); return true; }
                if (selected != 0) { removeNode (selected); return true; }
            }
            return false;
        }

    private:
        enum class Drag { none, cable, node, view };
        struct PinHit { int uid = 0, pin = 0; };

        graph::Graph& graphRef()   { return proc.engine().graph (ch); }

        juce::Point<float> toScreen (juce::Point<float> p) const   { return (p - pan) * zoom; }
        juce::Point<float> toGraph (juce::Point<float> s) const    { return s / zoom + pan; }

        juce::Rectangle<float> nodeRect (const Node& n) const
        {
            const auto tl = toScreen (n.pos);
            return { tl.x, tl.y, kNodeW * zoom, kNodeH * zoom };
        }

        juce::Rectangle<float> bypassRect (juce::Rectangle<float> r) const
        {
            const float zs = zoom * kS;
            const float s = 16.0f * zs;
            return { r.getRight() - s - 8.0f * zs, r.getY() + 8.0f * zs, s, s };
        }

        juce::Rectangle<float> editRect (juce::Rectangle<float> r) const
        {
            const float zs = zoom * kS;
            const float w = 30.0f * zs, h = 22.0f * zs;
            return { r.getRight() - w - 8.0f * zs, r.getBottom() - h - 10.0f * zs, w, h };
        }

        juce::Point<float> outPin (const Node& n) const
        {
            const auto r = nodeRect (n);
            return { r.getRight(), r.getCentreY() };
        }

        juce::Point<float> inPin (const Node& n, int pin) const
        {
            const auto r = nodeRect (n);
            if (n.numInputs() > 1) return { r.getX(), r.getY() + r.getHeight() * (pin == graph::kPinSc ? 0.76f : 0.36f) };
            return { r.getX(), r.getCentreY() };
        }

        Node* nodeAt (juce::Point<float> s)
        {
            auto& list = graphRef().nodeList();
            for (auto it = list.rbegin(); it != list.rend(); ++it)
                if (nodeRect (**it).contains (s)) return it->get();
            return nullptr;
        }

        PinHit pinAt (juce::Point<float> s, bool inputs)
        {
            const float r = (kPinR + 5.0f) * juce::jmax (0.7f, zoom);
            for (auto& n : graphRef().nodeList())
            {
                if (inputs)
                {
                    for (int p = 0; p < n->numInputs(); ++p)
                        if (inPin (*n, p).getDistanceFrom (s) <= r) return { n->uid, p };
                }
                else if (n->hasOutput() && outPin (*n).getDistanceFrom (s) <= r)
                    return { n->uid, 0 };
            }
            return {};
        }

        static juce::Path wirePath (juce::Point<float> a, juce::Point<float> b)
        {
            juce::Path p;
            const float dx = juce::jmax (40.0f, std::abs (b.x - a.x) * 0.5f);
            p.startNewSubPath (a);
            p.cubicTo (a.x + dx, a.y, b.x - dx, b.y, b.x, b.y);
            return p;
        }

        Wire wireAt (juce::Point<float> s)
        {
            auto& gr = graphRef();
            for (auto& w : gr.wireList())
            {
                const auto* a = gr.find (w.src);
                const auto* b = gr.find (w.dst);
                if (a == nullptr || b == nullptr) continue;
                const auto path = wirePath (outPin (*a), inPin (*b, w.pin));
                juce::Point<float> near;
                path.getNearestPoint (s, near);
                if (near.getDistanceFrom (s) < 7.0f) return w;
            }
            return {};
        }

        void drawWire (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, juce::Colour c, bool lit, bool dashed)
        {
            const auto path = wirePath (a, b);
            const float t = (lit ? 4.0f : 2.6f) * juce::jmax (0.7f, zoom);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.strokePath (path, juce::PathStrokeType (t + 2.5f));
            g.setColour (lit ? c.brighter (0.4f) : c);
            if (dashed)
            {
                juce::Path d;
                const float dash[] = { 9.0f, 6.0f };
                juce::PathStrokeType (t).createDashedStroke (d, path, dash, 2);
                g.fillPath (d);
            }
            else
                g.strokePath (path, juce::PathStrokeType (t));
        }

        void drawPin (juce::Graphics& g, juce::Point<float> at, juce::Colour c, bool hot)
        {
            const float r = kPinR * juce::jmax (0.7f, zoom);
            g.setColour (juce::Colours::black);
            g.fillEllipse (at.x - r - 1.5f, at.y - r - 1.5f, 2.0f * r + 3.0f, 2.0f * r + 3.0f);
            g.setColour (hot ? juce::Colours::white : c);
            g.fillEllipse (at.x - r, at.y - r, 2.0f * r, 2.0f * r);
        }

        void drawNode (juce::Graphics& g, Node& n)
        {
            const float zs = zoom * kS;
            const auto r = nodeRect (n);
            const auto c = typeColour (n.type);
            const bool on = n.on.load();
            const bool chosen = n.uid == selected;
            const float rad = 9.0f * zs;

            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (r.translated (0.0f, 4.0f * zs), rad);
            g.setColour (kNodeBg.withMultipliedBrightness (on ? 1.0f : 0.7f));
            g.fillRoundedRectangle (r, rad);

            // Its kind's colour along the top
            auto top = r.withHeight (8.0f * zs);
            g.setColour (c.withAlpha (on ? 1.0f : 0.35f));
            g.fillRoundedRectangle (top, rad * 0.6f);
            g.fillRect (top.withTrimmedTop (top.getHeight() * 0.5f));

            g.setColour (chosen ? juce::Colours::white : c.withAlpha (on ? 0.8f : 0.35f));
            g.drawRoundedRectangle (r.reduced (0.5f), rad, chosen ? 2.6f : 1.4f);

            // Its name, and what it is
            auto text = r.reduced (12.0f * zs, 0.0f).withTrimmedTop (14.0f * zs);
            g.setColour (on ? win::text : win::dim);
            g.setFont (font (17.0f * zs, true));
            const bool plugin = graph::isPluginType (n.type);
            const bool empty = plugin && n.plugin == nullptr;
            const auto title = empty && n.wantedName.isNotEmpty() ? n.wantedName : n.title();
            g.drawFittedText (title, text.removeFromTop (24.0f * zs).withTrimmedRight (24.0f * zs).toNearestInt(),
                              juce::Justification::centredLeft, 1, 0.7f);
            g.setFont (font (13.0f * zs));
            juce::String sub = graph::typeName (n.type);
            if (empty) sub = n.wantedName.isNotEmpty() ? juce::String ("missing - load it") : juce::String ("empty - load a plugin");
            else if (n.type == graph::ntDynEq) sub = "IN, and its key on SC";
            else if (n.type == graph::ntOutput) sub = "into the mixer strip";
            else if (! plugin) sub = graph::builtInSummary (n.type, n.values.data());
            else if (n.plugin != nullptr && n.plugin->instance != nullptr && n.plugin->instance->getLatencySamples() > 0)
                sub << "  " << n.plugin->instance->getLatencySamples() << " smp";
            g.setColour (empty ? juce::Colour (0xFFFFC23A) : win::dim);
            g.drawFittedText (sub, text.removeFromTop (20.0f * zs).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);

            // ON (the dot)
            if (n.type != graph::ntOutput)
            {
                const auto b = bypassRect (r);
                g.setColour (on ? win::on : juce::Colour (0xFF5A1E1E));
                g.fillEllipse (b);
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.drawEllipse (b, 1.2f);
            }

            // e: the plugin's window
            if (plugin)
            {
                const auto er = editRect (r);
                g.setColour (win::well);
                g.fillRoundedRectangle (er, 4.0f);
                g.setColour (c);
                g.drawRoundedRectangle (er, 4.0f, 1.2f);
                g.setFont (font (15.0f * zs, true));
                g.drawText (empty ? "+" : "e", er, juce::Justification::centred, false);
            }

            // Its level
            if (n.type != graph::ntOutput)
            {
                const float pk = juce::jmin (1.0f, n.takePeak());
                peaks[n.uid] = juce::jmax (pk, peaks[n.uid] * 0.85f);
                const float lv = peaks[n.uid];
                auto bar = juce::Rectangle<float> (r.getX() + 12.0f * zs, r.getBottom() - 9.0f * zs, (r.getWidth() - 60.0f * zs), 3.0f * zs);
                g.setColour (win::well);
                g.fillRect (bar);
                g.setColour (lv > 0.99f ? win::led : c.withAlpha (0.9f));
                g.fillRect (bar.withWidth (bar.getWidth() * std::sqrt (lv)));
            }

            // The pins
            for (int p = 0; p < n.numInputs(); ++p)
            {
                const auto at = inPin (n, p);
                const bool hot = dragging == Drag::cable && hoverPin.uid == n.uid && hoverPin.pin == p;
                drawPin (g, at, p == graph::kPinSc ? kWireSc : kWireMain, hot);
                if (n.numInputs() > 1)
                {
                    g.setColour (win::dim);
                    g.setFont (font (11.0f * zs, true));
                    g.drawText (p == graph::kPinSc ? "SC" : "IN", juce::Rectangle<float> (at.x + 11.0f * zs, at.y - 7.0f * zs, 24.0f * zs, 14.0f * zs),
                                juce::Justification::centredLeft, false);
                }
            }
            if (n.hasOutput()) drawPin (g, outPin (n), kWireMain, false);
        }

        //======================================================================
        void choose (int uid)
        {
            selected = uid;
            wireChosen = false;
            editor.select (uid);
            repaint();
        }

        void say (const juce::String& text)
        {
            message = text;
            messageUntil = juce::Time::getMillisecondCounter() + 4000;
            repaint();
        }

        void connect (int src, int dst, int pin)
        {
            switch (graphRef().connect (src, dst, pin))
            {
                case graph::Graph::Connect::loop:
                    say ("Not wired: that cable would close a loop.  A loop must go through a Feedback node.");
                    break;
                case graph::Graph::Connect::invalid:
                    say ("Not wired: an output goes into an input.");
                    break;
                case graph::Graph::Connect::ok:
                case graph::Graph::Connect::already:
                    break;
            }
        }

        void removeNode (int uid)
        {
            auto* n = graphRef().find (uid);
            if (n == nullptr) return;
            if (n->type == graph::ntOutput) { say ("The Output stays: it is the graph's way into the mixer."); return; }
            hub.close (nodeWindowKey (ch, uid));
            graphRef().removeNode (uid);
            if (selected == uid) choose (0);
            proc.rackChanged();
            repaint();
        }

        void addMenu (juce::Point<float> at)
        {
            static const int order[] = { graph::ntInstrument, graph::ntPlugin, 0, graph::ntVowel, graph::ntFilter, graph::ntGate, graph::ntDelay,
                                         graph::ntSaturator, graph::ntReverb, graph::ntDynEq, graph::ntPhaser, graph::ntWah, 0,
                                         graph::ntFeedback, graph::ntKey };
            juce::PopupMenu m;
            m.addSectionHeader ("Add a node");
            const bool full = (int) graphRef().nodeList().size() >= graph::kMaxNodes;
            for (int t : order)
            {
                if (t == 0) { m.addSeparator(); continue; }
                juce::String name = graph::typeName (t);
                if (graph::isPluginType (t)) name << "  (load from file)";
                m.addItem (t + 1, name, ! full);
            }
            m.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<GraphCanvas> (this), at] (int res)
            {
                if (safe == nullptr || res <= 0) return;
                const int type = res - 1;
                auto* n = safe->graphRef().addNode (type, at - juce::Point<float> (kNodeW, kNodeH) * 0.5f, true);
                if (n == nullptr) return;
                if (type == graph::ntKey) n->values[0].store ((float) (safe->ch == 0 ? 1 : 0));
                safe->choose (n->uid);
                if (graph::isPluginType (type)) chooseNodePlugin (safe->proc, safe->hub, safe->ch, n->uid, safe.getComponent());
            });
        }

        void nodeMenu (Node& n)
        {
            const int uid = n.uid;
            const bool plugin = graph::isPluginType (n.type);
            juce::PopupMenu m;
            m.addSectionHeader (n.title());
            if (plugin)
            {
                m.addItem (1, "Open its window", n.plugin != nullptr);
                m.addItem (2, n.plugin != nullptr ? "Replace the plugin..." : "Load a plugin...");
                m.addItem (3, "Remove the plugin (the node stays)", n.plugin != nullptr);
                m.addSeparator();
            }
            if (n.type != graph::ntOutput) m.addItem (4, "On", true, n.on.load());
            m.addItem (5, "Cut its cables");
            if (n.type != graph::ntOutput) m.addItem (6, "Remove the node");
            m.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<GraphCanvas> (this), uid] (int res)
            {
                if (safe == nullptr || res == 0) return;
                auto& gr = safe->graphRef();
                auto* nd = gr.find (uid);
                if (nd == nullptr) return;
                switch (res)
                {
                    case 1: openNodePlugin (safe->proc, safe->hub, safe->ch, uid, safe.getComponent()); break;
                    case 2: chooseNodePlugin (safe->proc, safe->hub, safe->ch, uid, safe.getComponent()); break;
                    case 3:
                        safe->hub.close (nodeWindowKey (safe->ch, uid));
                        nd->wantedFile = juce::File();
                        nd->wantedSonify = {};
                        nd->wantedName = {};
                        gr.setPlugin (uid, nullptr);
                        safe->proc.rackChanged();
                        break;
                    case 4: nd->on.store (! nd->on.load()); break;
                    case 5: gr.disconnectAll (uid); break;
                    case 6: safe->removeNode (uid); break;
                    default: break;
                }
                safe->repaint();
            });
        }

        void wireMenu (Wire w)
        {
            juce::PopupMenu m;
            m.addItem (1, "Remove the cable");
            m.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<GraphCanvas> (this), w] (int res)
            {
                if (safe == nullptr || res != 1) return;
                safe->graphRef().disconnect (w);
                safe->wireChosen = false;
                safe->repaint();
            });
        }

        void timerCallback() override
        {
            repaint();
        }

        ZodiakProcessor& proc;
        WindowHub& hub;
        const int ch;
        GraphEditor& editor;

        juce::Point<float> pan { 0.0f, 60.0f }, panStart;
        float zoom = 0.82f;
        Drag dragging = Drag::none;
        int cableFrom = 0, dragUid = 0, selected = 0;
        juce::Point<float> cableEnd, dragOffset;
        PinHit hoverPin;
        bool wireChosen = false;
        Wire chosenWire;
        juce::String message;
        juce::uint32 messageUntil = 0;
        std::map<int, float> peaks;
    };

    //==========================================================================
    //  A node's settings
    //==========================================================================
    namespace
    {
        void styleToggle (juce::TextButton& b)
        {
            b.setClickingTogglesState (true);
            b.setColour (juce::TextButton::buttonColourId, win::well);
            b.setColour (juce::TextButton::buttonOnColourId, win::led);
            b.setColour (juce::TextButton::textColourOffId, win::text);
            b.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        }

        void styleButton (juce::TextButton& b)
        {
            b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2E34));
            b.setColour (juce::TextButton::textColourOffId, win::text);
        }
    }

    //==========================================================================
    //  The filter's curve: its type's response at its cutoff and resonance,
    //  measured (an impulse through a copy of the filter, read by an FFT), so
    //  every one of the 17 types draws true.  The cutoff moves with the
    //  envelope as it plays; the dashed line is where ENV takes it.
    //  Drag: across = cutoff, up and down = resonance.
    //==========================================================================
    class FilterGraph final : public juce::Component
    {
    public:
        std::function<int()>   getType;
        std::function<float()> getCut, getRes, getEnv, getLive, getDrive;
        std::function<void (float cut, float res)> onDrag;

        void refresh()
        {
            const int type = getType();
            const float live = getLive();
            const float res = getRes();
            const float drive = fx::driveIsInside (type) ? getDrive() : 0.0f;
            const auto key = std::make_tuple (type, (int) std::lround (std::log2 (juce::jmax (20.0f, live)) * 96.0f), (int) std::lround (res * 400.0f),
                                              (int) std::lround (drive * 100.0f));
            if (key != shownKey) { shownKey = key; measure (type, live, res, drive); }
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            const auto r = plot();
            g.setColour (win::well);
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);

            // The grid: octaves of frequency, 12 dB steps
            g.setFont (font (13.0f));
            for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
            {
                const float x = xOf (f);
                g.setColour (juce::Colours::white.withAlpha (0.07f));
                g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
                g.setColour (win::dim);
                g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f), juce::Rectangle<float> (x - 20.0f, r.getBottom() + 2.0f, 40.0f, 16.0f),
                            juce::Justification::centred, false);
            }
            for (float db : { 12.0f, 0.0f, -12.0f, -24.0f, -36.0f })
            {
                const float y = yOf (db);
                g.setColour (juce::Colours::white.withAlpha (std::abs (db) < 0.5f ? 0.16f : 0.07f));
                g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
                g.setColour (win::dim);
                g.drawText (juce::String ((int) db), juce::Rectangle<float> (r.getX() - 34.0f, y - 8.0f, 30.0f, 16.0f), juce::Justification::centredRight, false);
            }

            // The curve
            juce::Path line, fill;
            for (int i = 0; i < kPoints; ++i)
            {
                const float x = r.getX() + r.getWidth() * (float) i / (float) (kPoints - 1);
                const float y = juce::jlimit (r.getY(), r.getBottom(), yOf (curve[(size_t) i]));
                if (i == 0) { line.startNewSubPath (x, y); fill.startNewSubPath (x, r.getBottom()); }
                else        line.lineTo (x, y);
                fill.lineTo (x, y);
            }
            fill.lineTo (r.getRight(), r.getBottom());
            fill.closeSubPath();
            const auto c = juce::Colour (0xFF4C8DF0);
            g.setGradientFill (juce::ColourGradient (c.withAlpha (0.35f), 0.0f, r.getY(), c.withAlpha (0.03f), 0.0f, r.getBottom(), false));
            g.fillPath (fill);
            g.setColour (c.brighter (0.5f));
            g.strokePath (line, juce::PathStrokeType (2.5f));

            if (getType() > 0)
            {
                // The cutoff the knob sets, where the envelope takes it, and where it is now
                const float cut = getCut(), env = getEnv();
                g.setColour (juce::Colours::white.withAlpha (0.5f));
                g.drawVerticalLine ((int) xOf (cut), r.getY(), r.getBottom());
                if (std::abs (env) > 0.01f)
                {
                    juce::Path d;
                    const float xe = xOf (juce::jlimit (20.0f, 20000.0f, cut * std::exp2 (env)));
                    d.startNewSubPath (xe, r.getY());
                    d.lineTo (xe, r.getBottom());
                    juce::Path dashed;
                    const float dash[] = { 6.0f, 5.0f };
                    juce::PathStrokeType (1.5f).createDashedStroke (dashed, d, dash, 2);
                    g.setColour (juce::Colour (0xFFFFC23A));
                    g.fillPath (dashed);
                }
                const float xl = xOf (getLive());
                g.setColour (juce::Colour (0xFFFFC23A));
                g.fillEllipse (xl - 6.0f, r.getY() - 6.0f, 12.0f, 12.0f);
            }
            else
            {
                g.setColour (win::dim);
                g.setFont (font (16.0f, true));
                g.drawText ("OFF", r, juce::Justification::centred, false);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override   { mouseDrag (e); }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            const auto r = plot();
            const float t = juce::jlimit (0.0f, 1.0f, (e.position.x - r.getX()) / r.getWidth());
            const float cut = 20.0f * std::pow (1000.0f, t);
            const float res = juce::jlimit (0.0f, 1.0f, 1.0f - (e.position.y - r.getY()) / r.getHeight());
            if (onDrag) onDrag (cut, res);
        }

    private:
        static constexpr int kPoints = 240, kOrder = 13, kSize = 1 << kOrder;
        static constexpr float kTopDb = 18.0f, kBottomDb = -42.0f;

        juce::Rectangle<float> plot() const   { return getLocalBounds().toFloat().reduced (12.0f).withTrimmedLeft (30.0f).withTrimmedBottom (16.0f); }
        float xOf (float f) const             { const auto r = plot(); return r.getX() + r.getWidth() * std::log (juce::jmax (20.0f, f) / 20.0f) / std::log (1000.0f); }
        float yOf (float db) const            { const auto r = plot(); return r.getY() + r.getHeight() * (kTopDb - db) / (kTopDb - kBottomDb); }

        void measure (int type, float cut, float res, float drive)
        {
            if (type <= 0) { curve.fill (0.0f); return; }
            constexpr double sr = 48000.0;
            fx::FilterVoice f;
            f.prepare (sr);
            f.setType (type);
            f.reset();
            f.setParams (juce::jlimit (20.0f, (float) (sr * 0.45), cut), res, drive);
            for (int i = 0; i < 2048; ++i) f.process (0.0f);         // its own glides settle

            constexpr float amp = 0.01f;                             // small: the saturating types stay linear
            std::vector<float> buf ((size_t) kSize * 2, 0.0f);
            for (int i = 0; i < kSize; ++i) buf[(size_t) i] = f.process (i == 0 ? amp : 0.0f) / amp;
            juce::dsp::FFT fft (kOrder);
            fft.performFrequencyOnlyForwardTransform (buf.data());

            for (int i = 0; i < kPoints; ++i)
            {
                const double hz = 20.0 * std::pow (1000.0, (double) i / (kPoints - 1));
                const double bin = hz * kSize / sr;
                const int b0 = juce::jlimit (0, kSize / 2 - 1, (int) bin);
                const double fr = bin - b0;
                const double mag = buf[(size_t) b0] * (1.0 - fr) + buf[(size_t) b0 + 1] * fr;
                curve[(size_t) i] = (float) juce::Decibels::gainToDecibels (mag, -120.0);
            }
        }

        std::array<float, kPoints> curve {};
        std::tuple<int, int, int, int> shownKey { -1, 0, 0, 0 };
    };

    class BuiltInPanel final : public juce::Component,
                               private juce::Timer
    {
    public:
        BuiltInPanel (ZodiakProcessor& p, int channel, int nodeUid) : proc (p), ch (channel), uid (nodeUid)
        {
            auto* n = node();
            type = n->type;
            title = n->title();

            onBtn.setTooltip ("ON, or bypassed (the sound passes it by)");
            onBtn.onClick = [this] { if (auto* nd = node()) nd->on.store (onBtn.getToggleState()); };
            addAndMakeVisible (onBtn);
            linkTarget (onBtn, proc, link::Target::nodeOn (ch, uid));

            if (type == graph::ntDynEq)
            {
                dynEq = std::make_unique<DynEqPanel> (*n->ducker(), "its SC input");
                addAndMakeVisible (*dynEq);
            }
            else
            {
                const int np = n->numParams();
                for (int pi = 0; pi < np; ++pi)
                {
                    const auto* info = graph::builtInParam (type, pi);
                    const juce::String id (info->id);
                    if (info->kind == pkMask) continue;
                    if (type == graph::ntVowel && id.startsWith ("c")) continue;       // the columns: the grid
                    if (type == graph::ntFilter && id == "type")                       // the types: a menu, in families
                    {
                        auto* root = typeBox.getRootMenu();
                        auto add = [root] (int from, int to)
                        {
                            for (int t = from; t <= to; ++t) root->addItem (t + 1, filterTypeName (t));
                        };
                        root->addItem (1, "OFF");
                        root->addSectionHeader ("Basic");                          add (1, 10);
                        root->addSectionHeader ("Character");                      add (11, 17);
                        root->addSectionHeader ("Avenger-style");                  add (18, 33);
                        root->addSectionHeader ("Crying  (trance, rave)");         add (34, 37);
                        typeBox.setColour (juce::ComboBox::backgroundColourId, win::well);
                        typeBox.setColour (juce::ComboBox::textColourId, win::text);
                        typeBox.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xFF4C8DF0));
                        typeBox.onChange = [this, pi]
                        {
                            if (auto* nd = node()) if (typeBox.getSelectedId() > 0) nd->set (pi, (float) (typeBox.getSelectedId() - 1));
                        };
                        addAndMakeVisible (typeBox);
                        linkTarget (typeBox, proc, link::Target::node (ch, uid, pi, id));
                        continue;
                    }

                    if (info->kind == pkToggle)
                    {
                        auto b = std::make_unique<juce::TextButton> (juce::String (info->name).toUpperCase());
                        styleToggle (*b);
                        b->onClick = [this, pi, raw = b.get()] { if (auto* nd = node()) nd->set (pi, raw->getToggleState() ? 1.0f : 0.0f); };
                        addAndMakeVisible (*b);
                        linkTarget (*b, proc, link::Target::node (ch, uid, pi, id));
                        toggles.push_back ({ pi, std::move (b) });
                        continue;
                    }

                    Mapping m;
                    m.min = info->min; m.max = info->max; m.def = info->add; m.skew = info->skew;
                    m.stepped = info->kind == pkStepped;
                    const int t = type;
                    m.text = [t, pi] (float v) { return graph::builtInText (t, pi, v); };
                    auto b = std::make_unique<Binding> (*this, info->name, n->values[(size_t) pi], m, [] {});
                    linkTarget (b->gold(), proc, link::Target::node (ch, uid, pi, id));
                    sliders.push_back ({ pi, std::move (b) });
                }

                if (type == graph::ntVowel)
                {
                    vowel.getVowel    = [this] (int c) { auto* nd = node(); return nd != nullptr ? (int) std::lround (nd->values[(size_t) (4 + c)].load()) : 0; };
                    vowel.setVowel    = [this] (int c, int v) { if (auto* nd = node()) nd->set (4 + c, (float) v); };
                    vowel.getPosition = [this] { auto* nd = node(); return nd != nullptr ? nd->shown() : 0.0f; };
                    vowel.getMix      = [this] { auto* nd = node(); return nd != nullptr ? nd->values[0].load() : 0.0f; };
                    addAndMakeVisible (vowel);
                }
                if (type == graph::ntFilter)
                {
                    filterGraph.getType = [this] { auto* nd = node(); return nd != nullptr ? (int) std::lround (nd->values[0].load()) : 0; };
                    filterGraph.getCut  = [this] { auto* nd = node(); return nd != nullptr ? nd->values[1].load() : 1000.0f; };
                    filterGraph.getRes  = [this] { auto* nd = node(); return nd != nullptr ? nd->values[2].load() : 0.0f; };
                    filterGraph.getEnv  = [this] { auto* nd = node(); return nd != nullptr ? nd->values[3].load() : 0.0f; };
                    filterGraph.getDrive = [this] { auto* nd = node(); return nd != nullptr ? nd->values[8].load() : 0.0f; };
                    filterGraph.getLive = [this]         // the cutoff as the envelope has it now
                    {
                        auto* nd = node();
                        return nd != nullptr ? juce::jlimit (20.0f, 20000.0f, nd->values[1].load() * std::exp2 (nd->shown())) : 1000.0f;
                    };
                    filterGraph.onDrag = [this] (float cut, float res)
                    {
                        if (auto* nd = node())
                        {
                            nd->set (1, cut);
                            nd->set (2, res);
                            if ((int) std::lround (nd->values[0].load()) == 0) nd->set (0, 2.0f);    // OFF: dragging it turns it on (LP 24)
                        }
                    };
                    addAndMakeVisible (filterGraph);
                }
                if (type == graph::ntGate)
                {
                    steps.getSteps   = [this] { auto* nd = node(); return nd != nullptr ? (int) nd->values[4].load() : 0; };
                    steps.setSteps   = [this] (int s) { if (auto* nd = node()) nd->set (4, (float) s); };
                    steps.getCurrent = [this] { auto* nd = node(); return nd != nullptr ? (int) nd->shown() : -1; };
                    addAndMakeVisible (steps);
                }
            }

            hint.setFont (font (14.0f));
            hint.setColour (juce::Label::textColourId, win::dim);
            hint.setJustificationType (juce::Justification::topLeft);
            hint.setText (hintFor (type), juce::dontSendNotification);
            addAndMakeVisible (hint);

            refreshAll();
            startTimerHz (30);
        }

        ~BuiltInPanel() override   { stopTimer(); }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (win::bg);
            g.setColour (typeColour (type));
            g.fillRoundedRectangle (titleArea.toFloat().withWidth (8.0f), 3.0f);
            g.setColour (win::text);
            g.setFont (font (22.0f, true));
            g.drawText (title, titleArea.withTrimmedLeft (20), juce::Justification::centredLeft, false);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (20, 12);
            auto head = area.removeFromLeft (200);
            titleArea = head.removeFromTop (40);
            head.removeFromTop (10);
            onBtn.setBounds (head.removeFromTop (54).withWidth (130));
            head.removeFromTop (12);
            hint.setBounds (head);
            area.removeFromLeft (20);

            if (dynEq != nullptr) { dynEq->setBounds (area); return; }

            if (type == graph::ntVowel)
            {
                vowel.setBounds (area.removeFromLeft (juce::jmin (720, area.getWidth() / 2)));
                area.removeFromLeft (24);
            }
            if (type == graph::ntFilter)
            {
                auto col = area.removeFromLeft (juce::jmin (640, area.getWidth() / 2));
                typeBox.setBounds (col.removeFromTop (44));
                col.removeFromTop (8);
                filterGraph.setBounds (col);
                area.removeFromLeft (24);
            }
            if (type == graph::ntGate)
            {
                steps.setBounds (area.removeFromTop (70).withWidth (juce::jmin (area.getWidth(), 1000)));
                area.removeFromTop (12);
            }

            const int colW = 140;
            int x = area.getX();
            for (auto& s : sliders)
            {
                const bool shown = s.binding->gold().isVisible();
                if (! shown) continue;
                s.binding->gold().setBounds (x, area.getY(), colW, area.getHeight());
                x += colW;
            }
            int ty = area.getY() + 10;
            for (auto& t : toggles)
            {
                t.button->setBounds (x + 30, ty, 140, 48);
                ty += 60;
            }
        }

    private:
        struct SliderItem { int param; std::unique_ptr<Binding> binding; };
        struct ToggleItem { int param; std::unique_ptr<juce::TextButton> button; };

        Node* node()   { return proc.engine().graph (ch).find (uid); }

        static juce::String hintFor (int t)
        {
            switch (t)
            {
                case graph::ntVowel:    return "Click a column's letter to choose its vowel. RATE marches the columns with the tempo.";
                case graph::ntFilter:   return "Drag the curve: across = cutoff, up and down = resonance. ENV opens it on a key's press and closes it on the release. DRIVE: into the Crying types, the band of SSM.";
                case graph::ntGate:     return "Click the steps: lit ones let the sound through. DEPTH 0 passes it all.";
                case graph::ntDelay:    return "SYNC on: TIME is a note value at the tempo. Off: milliseconds. Each keeps its own.";
                case graph::ntSaturator:return "Tape, Tube, Fuzz, Clip. DRIVE colours more than it louds.";
                case graph::ntReverb:   return "Nine voicings, Greyhole the ninth. MIX 0: it rests and costs nothing.";
                case graph::ntFeedback: return "What comes in goes out one block later, at its LEVEL: the one way to close a loop.";
                case graph::ntKey:      return "Another channel's sound, as a source: wire it into a Dyn EQ's SC.";
                case graph::ntDynEq:    return "Wire a key into SC: the two points duck by it.";
                default:                return {};
            }
        }

        void refreshAll()
        {
            auto* n = node();
            if (n == nullptr) return;
            onBtn.setToggleState (n->on.load(), juce::dontSendNotification);
            for (auto& t : toggles) t.button->setToggleState (n->get (t.param) >= 0.5f, juce::dontSendNotification);

            // The delay: SYNC shows the note value, off the milliseconds
            if (type == graph::ntDelay)
            {
                const bool synced = n->get (0) >= 0.5f;
                if (synced != shownSync)
                {
                    shownSync = synced;
                    for (auto& s : sliders)
                    {
                        const juce::String id (graph::builtInParam (type, s.param)->id);
                        if (id == "note") s.binding->gold().setVisible (synced);
                        if (id == "ms")   s.binding->gold().setVisible (! synced);
                    }
                    resized();
                }
            }
            for (auto& s : sliders) s.binding->refresh();
            if (vowel.isVisible()) vowel.refresh();
            if (steps.isVisible()) steps.refresh();
            if (filterGraph.isVisible())
            {
                filterGraph.refresh();
                const int t = (int) std::lround (n->values[0].load()) + 1;
                if (typeBox.getSelectedId() != t) typeBox.setSelectedId (t, juce::dontSendNotification);
            }
        }

        void timerCallback() override   { refreshAll(); }

        ZodiakProcessor& proc;
        const int ch, uid;
        int type = 0;
        juce::String title;
        juce::Rectangle<int> titleArea;
        SwitchButton onBtn;
        juce::Label hint;
        std::vector<SliderItem> sliders;
        std::vector<ToggleItem> toggles;
        VowelGrid vowel;
        GateSteps steps;
        FilterGraph filterGraph;
        juce::ComboBox typeBox;
        std::unique_ptr<DynEqPanel> dynEq;
        bool shownSync = false;
    };

    //==========================================================================
    //  A plugin node's (VSTi / VST FX) and the Output's
    //==========================================================================
    class PluginPanel final : public juce::Component,
                              private juce::Timer
    {
    public:
        PluginPanel (ZodiakProcessor& p, WindowHub& h, int channel, int nodeUid) : proc (p), hub (h), ch (channel), uid (nodeUid)
        {
            auto* n = node();
            type = n->type;
            if (type != graph::ntOutput)
            {
                onBtn.onClick = [this] { if (auto* nd = node()) nd->on.store (onBtn.getToggleState()); };
                addAndMakeVisible (onBtn);
                linkTarget (onBtn, proc, link::Target::nodeOn (ch, uid));

                for (auto* b : { &openBtn, &loadBtn, &removeBtn }) { styleButton (*b); addAndMakeVisible (*b); }
                openBtn.onClick = [this] { openNodePlugin (proc, hub, ch, uid, &openBtn); };
                loadBtn.onClick = [this] { chooseNodePlugin (proc, hub, ch, uid, &loadBtn); };
                removeBtn.onClick = [this]
                {
                    hub.close (nodeWindowKey (ch, uid));
                    if (auto* nd = node()) { nd->wantedFile = juce::File(); nd->wantedSonify = {}; nd->wantedName = {}; }
                    proc.engine().graph (ch).setPlugin (uid, nullptr);
                    proc.rackChanged();
                };
            }
            info.setFont (font (16.0f));
            info.setColour (juce::Label::textColourId, win::label);
            info.setJustificationType (juce::Justification::topLeft);
            addAndMakeVisible (info);
            refresh();
            startTimerHz (4);
        }

        ~PluginPanel() override   { stopTimer(); }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (win::bg);
            g.setColour (typeColour (type));
            g.fillRoundedRectangle (titleArea.toFloat().withWidth (8.0f), 3.0f);
            g.setColour (win::text);
            g.setFont (font (22.0f, true));
            g.drawText (title, titleArea.withTrimmedLeft (20), juce::Justification::centredLeft, false);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (20, 12);
            titleArea = area.removeFromTop (40);
            area.removeFromTop (10);
            if (type != graph::ntOutput)
            {
                auto row = area.removeFromTop (54);
                onBtn.setBounds (row.removeFromLeft (130));
                row.removeFromLeft (20);
                openBtn.setBounds (row.removeFromLeft (180).reduced (0, 4));
                row.removeFromLeft (12);
                loadBtn.setBounds (row.removeFromLeft (200).reduced (0, 4));
                row.removeFromLeft (12);
                removeBtn.setBounds (row.removeFromLeft (180).reduced (0, 4));
                area.removeFromTop (14);
            }
            info.setBounds (area);
        }

    private:
        Node* node()   { return proc.engine().graph (ch).find (uid); }

        void refresh()
        {
            auto* n = node();
            if (n == nullptr) return;
            title = n->title();
            juce::String text;
            if (type == graph::ntOutput)
            {
                auto& gr = proc.engine().graph (ch);
                text << "The graph's end: what comes in goes to the channel's mixer strip.\n";
                text << "Latency of the graph: " << gr.latency() << " samples; lined up with the other channels by " << gr.tailDelay() << " more.";
            }
            else
            {
                onBtn.setToggleState (n->on.load(), juce::dontSendNotification);
                const bool loaded = n->plugin != nullptr && n->plugin->instance != nullptr;
                openBtn.setEnabled (loaded);
                removeBtn.setEnabled (loaded);
                loadBtn.setButtonText (loaded ? "Replace..." : "Load...");
                if (loaded)
                {
                    text << (n->plugin->sonifyId.isNotEmpty() ? juce::String ("Sonify's own plugin") : n->plugin->file.getFullPathName()) << "\n";
                    text << n->plugin->instance->getParameters().size() << " parameters (every one a macro's destination)";
                    if (const int lat = n->plugin->instance->getLatencySamples(); lat > 0) text << ", latency " << lat << " samples";
                    if (type == graph::ntInstrument) text << "\nPlays the channel's notes (MIDI channel " << ch + 1 << ", the patterns).";
                }
                else if (n->wantedName.isNotEmpty())
                    text << n->wantedName << " was not found:\n"
                         << (n->wantedSonify.isNotEmpty() ? juce::String ("Sonify's own plugin, not in this Sonify") : n->wantedFile.getFullPathName())
                         << "\nLoad it (or another) here.";
                else
                    text << (type == graph::ntInstrument ? "No VSTi loaded: load one, and it plays the channel's notes." : "No VST loaded: load an effect.");
            }
            info.setText (text, juce::dontSendNotification);
            repaint (titleArea);
        }

        void timerCallback() override   { refresh(); }

        ZodiakProcessor& proc;
        WindowHub& hub;
        const int ch, uid;
        int type = 0;
        juce::String title;
        juce::Rectangle<int> titleArea;
        SwitchButton onBtn;
        juce::TextButton openBtn { "Open its window" }, loadBtn { "Load..." }, removeBtn { "Remove plugin" };
        juce::Label info;
    };

    //==========================================================================
    //  GraphEditor
    //==========================================================================
    GraphEditor::GraphEditor (ZodiakProcessor& p, WindowHub& h, int channel) : proc (p), hub (h), ch (channel)
    {
        canvas = std::make_unique<GraphCanvas> (p, h, channel, *this);
        addAndMakeVisible (*canvas);
        shownRackGen = proc.rackGeneration();
        select (0);
        startTimerHz (4);
    }

    GraphEditor::~GraphEditor()
    {
        stopTimer();
        panel.reset();
        canvas.reset();
    }

    void GraphEditor::paint (juce::Graphics& g)
    {
        g.fillAll (win::bg);
        if (panel == nullptr)
        {
            g.setColour (win::dim);
            g.setFont (font (16.0f));
            g.drawText ("Click a node: its settings show here.", getLocalBounds().withTrimmedTop (canvas->getBottom()).reduced (24),
                        juce::Justification::centredLeft, false);
        }
        g.setColour (win::stripLine);
        g.drawHorizontalLine (canvas->getBottom() + 4, 0.0f, (float) getWidth());
    }

    void GraphEditor::resized()
    {
        auto r = getLocalBounds();
        auto bottom = r.removeFromBottom (juce::jmin (330, r.getHeight() / 2));
        canvas->setBounds (r.withTrimmedBottom (8));
        if (panel != nullptr) panel->setBounds (bottom);
    }

    void GraphEditor::select (int uid)
    {
        if (uid == shownUid) return;
        shownUid = uid;
        panel.reset();
        if (auto* n = proc.engine().graph (ch).find (uid))
        {
            if (graph::isPluginType (n->type) || n->type == graph::ntOutput)
                panel = std::make_unique<PluginPanel> (proc, hub, ch, uid);
            else
                panel = std::make_unique<BuiltInPanel> (proc, ch, uid);
            addAndMakeVisible (*panel);
        }
        resized();
        repaint();
    }

    void GraphEditor::timerCallback()
    {
        // A node removed (a set loaded): its settings go
        if (shownUid != 0 && proc.engine().graph (ch).find (shownUid) == nullptr) select (0);
        if (proc.rackGeneration() != shownRackGen)
        {
            shownRackGen = proc.rackGeneration();
            if (canvas->selectedNode() == 0 && shownUid != 0) select (0);
        }
    }
}
