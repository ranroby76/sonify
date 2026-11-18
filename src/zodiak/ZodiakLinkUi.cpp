// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakLinkUi.cpp
#include "PluginProcessor.h"
#include "ZodiakLinkUi.h"
#include "ZodiakLinkApply.h"
#include "ZodiakWidgets.h"

namespace zodiak::ui
{
    namespace
    {
        constexpr const char* kPayload = "zodiakLink";

        // What's being linked: dragged, or armed by a tap (and from which Zodiak)
        struct Session
        {
            bool dragging = false;
            link::Source armed;
            ZodiakProcessor* armedBy = nullptr;
            juce::Array<juce::Component*> covers;
        };

        Session& session()
        {
            static Session s;
            return s;
        }

        const juce::Colour kLinkColour { 0xFF3CB8E0 };

        //======================================================================
        //  The cover over a control
        //======================================================================
        class LinkCover final : public juce::Component,
                                public juce::DragAndDropTarget,
                                private juce::ComponentListener
        {
        public:
            LinkCover (juce::Component& c, ZodiakProcessor& p) : control (c), proc (p)
            {
                setInterceptsMouseClicks (true, false);
                setWantsKeyboardFocus (false);
                control.addAndMakeVisible (this);
                control.addComponentListener (this);
                control.addMouseListener (this, false);
                fit();
                session().covers.add (this);
            }

            ~LinkCover() override
            {
                session().covers.removeFirstMatchingValue (this);
                control.removeComponentListener (this);
                control.removeMouseListener (this);
            }

            std::function<link::Target (juce::Point<int>)> targetAt;     // where a source dropped goes
            std::function<void (const link::Source&)>       onDrop;       // ... or what it does instead
            bool fixed = false;                                           // it shows its target's source in a badge
            bool single = false;                                          // one target (a right double-click opens its link)
            int  badgeGen = -1;                                           // the map's generation the badge was read at

            bool hitTest (int, int) override
            {
                auto& s = session();
                return s.dragging || (s.armed.valid() && s.armedBy == &proc);
            }

            bool isInterestedInDragSource (const SourceDetails& d) override
            {
                return d.description.isObject() && d.description.getDynamicObject()->hasProperty (kPayload);
            }

            void itemDragEnter (const SourceDetails&) override   { over = true;  repaint(); }
            void itemDragExit (const SourceDetails&) override    { over = false; repaint(); }

            void itemDropped (const SourceDetails& d) override
            {
                over = false;
                repaint();
                if (! isInterestedInDragSource (d)) return;
                const auto s = link::Source::fromKey ((int) d.description.getDynamicObject()->getProperty (kPayload));
                take (s, d.localPosition.toInt());
            }

            // The tap path: armed, then this control clicked
            void mouseUp (const juce::MouseEvent& e) override
            {
                if (e.eventComponent != this) return;
                auto& s = session();
                if (! s.armed.valid() || s.armedBy != &proc) return;
                const auto source = s.armed;
                s.armed = {};
                s.armedBy = nullptr;
                take (source, e.getPosition());
                refreshLinkTargets();
            }

            void mouseDown (const juce::MouseEvent&) override {}
            void mouseDrag (const juce::MouseEvent&) override {}

            // A right double-click on the control: its link's window
            void mouseDoubleClick (const juce::MouseEvent& e) override
            {
                if (e.eventComponent != &control || ! e.mods.isRightButtonDown() || targetAt == nullptr || ! single) return;
                const auto t = targetAt (e.getEventRelativeTo (this).getPosition());
                if (t.valid() && proc.links().find (t) >= 0) showLinkPopup (proc, control, t);
            }

            void paint (juce::Graphics& g) override
            {
                auto b = getLocalBounds().toFloat();
                if (over)
                {
                    g.setColour (kLinkColour.withAlpha (0.28f));
                    g.fillRoundedRectangle (b, 4.0f);
                    g.setColour (kLinkColour);
                    g.drawRoundedRectangle (b.reduced (1.0f), 4.0f, 2.0f);
                }

                // Linked: its source, small, in the corner
                if (fixed && targetAt != nullptr)
                {
                    const int gen = proc.links().generation();
                    if (gen != badgeGen)
                    {
                        badgeGen = gen;
                        const int slot = proc.links().find (targetAt ({}));
                        const auto s = proc.links().sourceOf (slot);
                        badgeText = slot < 0 ? juce::String() : (s.kind == link::srcCc ? "CC" : "N") + juce::String (s.number);
                    }
                    if (badgeText.isEmpty()) return;
                    const auto& text = badgeText;
                    const float h = juce::jlimit (9.0f, 15.0f, b.getHeight() * 0.16f);
                    const auto f = font (h * 0.82f, true);
                    const float w = juce::GlyphArrangement::getStringWidth (f, text) + h * 0.7f;
                    const juce::Rectangle<float> r (b.getRight() - w - 1.0f, b.getY() + 1.0f, w, h);
                    g.setColour (juce::Colour (0xE0101418));
                    g.fillRoundedRectangle (r, h * 0.35f);
                    g.setColour (kLinkColour);
                    g.drawRoundedRectangle (r, h * 0.35f, 1.0f);
                    g.setFont (f);
                    g.drawText (text, r, juce::Justification::centred, false);
                }
            }

        private:
            void take (const link::Source& s, juce::Point<int> where)
            {
                if (! s.valid()) return;
                if (onDrop != nullptr) { onDrop (s); return; }
                if (targetAt == nullptr) return;
                const auto t = targetAt (where);
                if (t.valid()) linkDropped (proc, control, t, s);
            }

            void fit()
            {
                setBounds (control.getLocalBounds());
                toFront (false);
            }

            void componentMovedOrResized (juce::Component&, bool, bool) override   { fit(); }
            void componentChildrenChanged (juce::Component&) override              { if (getParentComponent() != nullptr) toFront (false); }

            void componentBeingDeleted (juce::Component&) override
            {
                delete this;            // the cover goes with its control
            }

            juce::Component& control;
            ZodiakProcessor& proc;
            bool over = false;
            juce::String badgeText;
        };

        LinkCover& coverOf (juce::Component& control, ZodiakProcessor& p)
        {
            for (auto* c : control.getChildren())
                if (auto* cover = dynamic_cast<LinkCover*> (c)) return *cover;
            return *new LinkCover (control, p);     // owned by its control (it goes with it)
        }

        //======================================================================
        //  A link's window
        //======================================================================
        class LinkPopup final : public juce::Component
        {
        public:
            struct Before
            {
                link::Source source;  int mode = link::kKnob;      // the target's link before the drop
                link::Target displaced; int displacedMode = 0;     // what the source moved before it
                bool valid = false;
            };

            LinkPopup (ZodiakProcessor& p, const link::Target& t, Before was) : proc (p), target (t), before (std::move (was))
            {
                auto label = [this] (juce::Label& l, float size, juce::Colour c)
                {
                    l.setJustificationType (juce::Justification::centred);
                    l.setColour (juce::Label::textColourId, c);
                    l.setFont (font (size, true));
                    addAndMakeVisible (l);
                };
                label (title, 15.0f, juce::Colours::white);
                label (sourceLbl, 18.0f, kLinkColour);
                label (hint, 11.0f, juce::Colours::white.withAlpha (0.55f));

                auto mode = [this] (juce::TextButton& b, int m)
                {
                    b.setButtonText (link::modeName (m));
                    b.onClick = [this, m]
                    {
                        const int slot = proc.links().find (target);
                        if (slot >= 0) proc.links().setMode (slot, m);
                        refresh();
                    };
                    addAndMakeVisible (b);
                };
                mode (knobBtn, link::kKnob);
                mode (toggleBtn, link::kToggle);
                mode (pushBtn, link::kPush);

                clearBtn.setButtonText ("CLEAR");
                clearBtn.onClick = [this] { proc.links().clear (target); close(); };
                addAndMakeVisible (clearBtn);

                cancelBtn.setButtonText (before.valid ? "CANCEL" : "CLOSE");
                cancelBtn.onClick = [this]
                {
                    if (before.valid)
                    {
                        // As it was before the drop
                        auto& map = proc.links();
                        map.clear (target);
                        if (before.displaced.valid() && dropped.valid())
                            map.assign (dropped, before.displaced, before.displacedMode);
                        if (before.source.valid())
                            map.assign (before.source, target, before.mode);
                    }
                    close();
                };
                addAndMakeVisible (cancelBtn);

                setSize (260, 214);
                refresh();
            }

            link::Source dropped;

        private:
            void refresh()
            {
                const int slot = proc.links().find (target);
                const auto s = proc.links().sourceOf (slot);
                title.setText (link::labelOf (target) + pluginParam(), juce::dontSendNotification);
                sourceLbl.setText (slot >= 0 ? s.text() : juce::String ("not linked"), juce::dontSendNotification);
                const int m = proc.links().modeOf (slot);
                auto tint = [slot, m] (juce::TextButton& b, int mine)
                {
                    b.setColour (juce::TextButton::buttonColourId, slot >= 0 && m == mine ? juce::Colour (0xFF1F6E8C) : juce::Colour (0xFF2A2D33));
                };
                tint (knobBtn, link::kKnob);
                tint (toggleBtn, link::kToggle);
                tint (pushBtn, link::kPush);
                hint.setText (m == link::kKnob ? "KNOB: the source's travel moves it (a selector: in equal steps)"
                            : m == link::kToggle ? "TOGGLE: each press flips it"
                                                 : "PUSH: on while held", juce::dontSendNotification);
                for (auto* b : { &knobBtn, &toggleBtn, &pushBtn, &clearBtn }) b->setEnabled (slot >= 0);
            }

            juce::String pluginParam()
            {
                const auto n = link::pluginParamName (proc.engine(), target);
                return n.isNotEmpty() ? "  " + n : juce::String();
            }

            void close()
            {
                if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                    box->dismiss();
            }

            void paint (juce::Graphics& g) override
            {
                g.fillAll (juce::Colour (0xFF1C1F23));
            }

            void resized() override
            {
                auto r = getLocalBounds().reduced (10);
                title.setBounds (r.removeFromTop (22));
                sourceLbl.setBounds (r.removeFromTop (26));
                hint.setBounds (r.removeFromTop (16));
                r.removeFromTop (6);
                auto row = r.removeFromTop (30);
                const int w = row.getWidth() / 3;
                knobBtn.setBounds (row.removeFromLeft (w).reduced (2, 0));
                toggleBtn.setBounds (row.removeFromLeft (w).reduced (2, 0));
                pushBtn.setBounds (row.reduced (2, 0));
                r.removeFromTop (10);
                auto bottom = r.removeFromTop (30);
                clearBtn.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2).reduced (2, 0));
                cancelBtn.setBounds (bottom.reduced (2, 0));
            }

            ZodiakProcessor& proc;
            const link::Target target;
            const Before before;
            juce::Label title, sourceLbl, hint;
            juce::TextButton knobBtn, toggleBtn, pushBtn, clearBtn, cancelBtn;
        };

        void launchPopup (std::unique_ptr<LinkPopup> popup, juce::Component& near)
        {
            juce::CallOutBox::launchAsynchronously (std::move (popup), near.getScreenBounds(), nullptr);
        }
    }

    //==========================================================================
    void linkTarget (juce::Component& control, ZodiakProcessor& p, const link::Target& target, bool badge)
    {
        auto& cover = coverOf (control, p);
        cover.targetAt = [target] (juce::Point<int>) { return target; };
        cover.onDrop = nullptr;
        cover.fixed = badge;
        cover.single = true;
        cover.badgeGen = -1;
        cover.repaint();
    }

    void linkTarget (juce::Component& control, ZodiakProcessor& p, std::function<link::Target (juce::Point<int>)> targetAt)
    {
        auto& cover = coverOf (control, p);
        cover.targetAt = std::move (targetAt);
        cover.onDrop = nullptr;
        cover.fixed = false;
        cover.single = false;
    }

    void linkDropAction (juce::Component& control, ZodiakProcessor& p, std::function<void (const link::Source&)> onDrop)
    {
        auto& cover = coverOf (control, p);
        cover.targetAt = nullptr;
        cover.onDrop = std::move (onDrop);
        cover.fixed = false;
        cover.single = false;
    }

    void unlinkTarget (juce::Component& control)
    {
        for (auto* c : control.getChildren())
            if (auto* cover = dynamic_cast<LinkCover*> (c))
            {
                delete cover;
                return;
            }
    }

    void refreshLinkTargets()
    {
        for (auto* c : session().covers) c->repaint();
    }

    bool isLinkAssigning()
    {
        return session().dragging || session().armed.valid();
    }

    void showLinkPopup (ZodiakProcessor& p, juce::Component& near, const link::Target& target)
    {
        launchPopup (std::make_unique<LinkPopup> (p, target, LinkPopup::Before {}), near);
    }

    void linkDropped (ZodiakProcessor& p, juce::Component& near, const link::Target& target, const link::Source& source)
    {
        auto& map = p.links();
        LinkPopup::Before before;
        before.valid = true;
        if (const int slot = map.find (target); slot >= 0)
        {
            before.source = map.sourceOf (slot);
            before.mode = map.modeOf (slot);
        }
        if (const int slot = map.findSource (source); slot >= 0)
        {
            before.displaced = map.targetOf (slot);
            before.displacedMode = map.modeOf (slot);
            if (before.displaced.sameAs (target)) before.displaced = {};
        }

        p.linkSource (source, target);
        refreshLinkTargets();

        auto popup = std::make_unique<LinkPopup> (p, target, before);
        popup->dropped = source;
        launchPopup (std::move (popup), near);
    }

    //==========================================================================
    //  The MIDI LINK window
    //==========================================================================
    LinkCanvas::LinkCanvas (ZodiakProcessor& p) : proc (p)
    {
        setTooltip ("MIDI LINK: move a knob, fader or pad on your controller, then drag what shows here onto any control of Zodiak "
                    "(or click here, then click the control).  Right-click: the links, save or load a map, clear them all.");
        startTimerHz (25);
    }

    LinkCanvas::~LinkCanvas()
    {
        auto& s = session();
        if (s.armedBy == &proc) { s.armed = {}; s.armedBy = nullptr; }
    }

    void LinkCanvas::paint (juce::Graphics& g)
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colour (0xFF121417));
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (armed ? kLinkColour : juce::Colours::white.withAlpha (0.22f));
        g.drawRoundedRectangle (b, 8.0f, armed ? 2.0f : 1.2f);

        auto r = getLocalBounds().reduced (12, 3);
        auto left = r.removeFromLeft (juce::jmin (150, r.getWidth() / 2));
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.setFont (font (15.0f, true));
        g.drawText ("MIDI LINK", left.removeFromTop (left.getHeight() / 2), juce::Justification::bottomLeft, false);
        g.setFont (font (13.0f));
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.drawText (armed ? "click a control" : (held.valid() ? "drag to a control" : "move a controller"),
                    left, juce::Justification::topLeft, false);

        const bool have = held.valid();
        g.setColour (have ? (armed ? kLinkColour : juce::Colours::white) : juce::Colours::white.withAlpha (0.25f));
        g.setFont (font (have ? 23.0f : 18.0f, true));
        g.drawText (have ? held.shortText() : juce::String ("--"), r.withTrimmedRight (60), juce::Justification::centred, false);
        if (have)
        {
            g.setFont (font (14.0f));
            g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.drawText ("ch " + juce::String (held.midiCh + 1), r.removeFromRight (60), juce::Justification::centredRight, false);
        }
    }

    void LinkCanvas::mouseDown (const juce::MouseEvent& e)
    {
        dragStarted = false;
        if (e.mods.isPopupMenu())
        {
            dragStarted = true;             // no tap after a right-click
            showMenu();
        }
    }

    void LinkCanvas::mouseDrag (const juce::MouseEvent& e)
    {
        if (dragStarted || ! held.valid() || e.getDistanceFromDragStart() < 6) return;
        dragStarted = true;

        juce::var payload (new juce::DynamicObject());
        payload.getDynamicObject()->setProperty (kPayload, held.key());
        auto& s = session();
        s.dragging = true;
        refreshLinkTargets();
        startDragging (payload, this, juce::ScaledImage(), true);     // into any of Zodiak's windows
    }

    void LinkCanvas::mouseUp (const juce::MouseEvent&)
    {
        if (dragStarted || ! held.valid()) return;

        // A tap: arm (or disarm) the source
        auto& s = session();
        armed = ! armed;
        s.armed = armed ? held : link::Source();
        s.armedBy = armed ? &proc : nullptr;
        refreshLinkTargets();
        repaint();
    }

    void LinkCanvas::dragOperationEnded (const juce::DragAndDropTarget::SourceDetails&)
    {
        session().dragging = false;
        refreshLinkTargets();
    }

    void LinkCanvas::timerCallback()
    {
        // A new source replaces the one held (and an armed one is let go)
        const int word = proc.engine().lastMidi();
        if (word != lastWord)
        {
            lastWord = word;
            const auto s = link::unpackLast (word);
            if (s.valid() && s.key() != held.key())
            {
                held = s;
                if (armed)
                {
                    armed = false;
                    auto& ses = session();
                    if (ses.armedBy == &proc) { ses.armed = {}; ses.armedBy = nullptr; }
                    refreshLinkTargets();
                }
                repaint();
            }
        }

        // Armed elsewhere's taken (a control clicked): the canvas follows
        if (armed && ! session().armed.valid())
        {
            armed = false;
            repaint();
        }

        // The links changed: the badges
        const int gen = proc.links().generation();
        if (gen != shownGen)
        {
            shownGen = gen;
            refreshLinkTargets();
        }
    }

    void LinkCanvas::showMenu()
    {
        auto& map = proc.links();
        juce::PopupMenu menu;
        const int n = map.count();
        menu.addSectionHeader ("MIDI LINK  -  " + juce::String (n) + (n == 1 ? " link" : " links"));

        // The links, each with FORGET
        juce::PopupMenu list;
        juce::Array<int> slots;
        for (int i = 0; i < link::Map::kMaxLinks; ++i)
        {
            const auto s = map.sourceOf (i);
            if (! s.valid()) continue;
            slots.add (i);
            list.addItem (1000 + i, s.text() + "  ->  " + link::labelOf (map.targetOf (i)) + "   (forget)");
        }
        menu.addSubMenu ("Links (click one to forget it)", list, n > 0);
        menu.addSeparator();
        menu.addItem (1, "Save map as...");
        menu.addItem (2, "Load map...");
        menu.addItem (3, "Clear all links", n > 0);
        menu.addSeparator();
        menu.addItem (4, "The map in force: " + link::mapFile().getFullPathName(), false, false);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [safe = juce::Component::SafePointer<LinkCanvas> (this)] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& p = safe->proc;
            if (r >= 1000)
            {
                const auto t = p.links().targetOf (r - 1000);
                if (t.valid()) p.links().clear (t);
                refreshLinkTargets();
                return;
            }
            if (r == 3) { p.links().clearAll(); refreshLinkTargets(); return; }

            link::mapsFolder().createDirectory();
            if (r == 1)
            {
                safe->chooser = std::make_unique<juce::FileChooser> ("Save the MIDI map as", link::mapsFolder().getChildFile ("My Map.zmap"), "*.zmap");
                safe->chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                    [safe] (const juce::FileChooser& fc)
                    {
                        if (safe == nullptr || fc.getResult() == juce::File()) return;
                        const auto res = safe->proc.saveLinkMapAs (fc.getResult().withFileExtension (link::kMapExtension));
                        if (res.failed())
                            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "MIDI map", res.getErrorMessage());
                    });
            }
            else if (r == 2)
            {
                safe->chooser = std::make_unique<juce::FileChooser> ("Load a MIDI map", link::mapsFolder(), "*.zmap");
                safe->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [safe] (const juce::FileChooser& fc)
                    {
                        if (safe == nullptr || fc.getResult() == juce::File()) return;
                        const auto res = safe->proc.loadLinkMap (fc.getResult());
                        if (res.failed())
                            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "MIDI map", res.getErrorMessage());
                        refreshLinkTargets();
                    });
            }
        });
    }
}
