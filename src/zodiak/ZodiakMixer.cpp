// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakMixer.cpp
#include "ZodiakMixer.h"
#include "ZodiakLinkUi.h"
#include "ZodiakLinkApply.h"

namespace zodiak::ui
{
    namespace
    {
        constexpr int kFaderW  = 124;     // a strip's fader section
        constexpr int kDeptW   = 300;     // its departments panel, open
        constexpr int kGap     = 10;
        constexpr int kTitleH  = 34;
        constexpr int kItemH   = 34;
        constexpr int kHeadH   = 30;


        // Where a window opens: centred on `near` (else the screen), wholly on its screen
        // with its title bar showing - never above the top, where it couldn't be moved
        void placeOnScreen (juce::Component& w, juce::Component* near)
        {
            const auto& displays = juce::Desktop::getInstance().getDisplays();
            const bool nearShown = near != nullptr && near->isShowing();
            const auto* display = nearShown ? displays.getDisplayForRect (near->getScreenBounds()) : displays.getPrimaryDisplay();
            const auto user = display != nullptr ? display->userArea : juce::Rectangle<int> (0, 0, 1600, 900);
            auto b = w.getBounds().withCentre (nearShown ? near->getScreenBounds().getCentre() : user.getCentre());
            b = b.constrainedWithin (user.withTrimmedTop (36));
            w.setBounds (b);
        }

        juce::String dbText (float db)
        {
            if (db <= -59.5f) return "-inf";
            return juce::String (db, 1) + " dB";
        }
    }

    //==========================================================================
    //  WindowHub
    //==========================================================================
    class WindowHub::Window final : public juce::DocumentWindow
    {
    public:
        Window (WindowHub& h, const juce::String& k, const juce::String& title, juce::AudioProcessor* p)
            : juce::DocumentWindow (title, win::bg, juce::DocumentWindow::closeButton), hub (h), key (k), plugin (p)
        {
            setUsingNativeTitleBar (true);
            setAlwaysOnTop (true);
        }

        void closeButtonPressed() override   { hub.closeLater (key); }

        WindowHub& hub;
        const juce::String key;
        juce::AudioProcessor* plugin = nullptr;
    };

    WindowHub::WindowHub() = default;
    WindowHub::~WindowHub()   { closeAll(); }

    void WindowHub::showPanel (const juce::String& key, const juce::String& title, std::unique_ptr<juce::Component> content,
                               juce::Component* near)
    {
        if (auto it = windows.find (key); it != windows.end())
        {
            it->second->toFront (true);
            return;
        }

        auto w = std::make_unique<Window> (*this, key, title, nullptr);
        w->setContentOwned (content.release(), true);
        placeOnScreen (*w, near);
        w->setVisible (true);
        windows[key] = std::move (w);
    }

    namespace
    {
        // Its content at its design size, scaled to the window
        class ScaledFrame final : public juce::Component
        {
        public:
            ScaledFrame (juce::Component* borrowedChild, std::unique_ptr<juce::Component> ownedChild, juce::Rectangle<int> d)
                : owned (std::move (ownedChild)), child (owned != nullptr ? owned.get() : borrowedChild), design (d.withZeroOrigin())
            {
                if (child != nullptr) addAndMakeVisible (*child);
            }

            ~ScaledFrame() override
            {
                if (owned == nullptr && child != nullptr)
                {
                    removeChildComponent (child);
                    child->setTransform ({});
                }
            }

            void paint (juce::Graphics& g) override   { g.fillAll (win::bg); }

            void resized() override
            {
                if (child == nullptr || design.isEmpty()) return;
                child->setTransform ({});
                child->setBounds (design);
                child->setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) design.getWidth(),
                                                                   (float) getHeight() / (float) design.getHeight()));
            }

        private:
            std::unique_ptr<juce::Component> owned;
            juce::Component* child = nullptr;
            juce::Rectangle<int> design;
        };
    }

    void WindowHub::showScaled (const juce::String& key, const juce::String& title, juce::Component* borrowed,
                                std::unique_ptr<juce::Component> owned, juce::Rectangle<int> design, juce::Component* near)
    {
        if (auto it = windows.find (key); it != windows.end())
        {
            it->second->setName (title);
            it->second->toFront (true);
            return;
        }
        if (design.isEmpty() || (borrowed == nullptr && owned == nullptr)) return;

        // The screen it opens on: as designed, smaller only to fit it
        const auto& displays = juce::Desktop::getInstance().getDisplays();
        const auto* display = (near != nullptr && near->isShowing()) ? displays.getDisplayForRect (near->getScreenBounds())
                                                                     : displays.getPrimaryDisplay();
        const auto user = display != nullptr ? display->userArea : juce::Rectangle<int> (0, 0, 1600, 900);
        const float s = juce::jmin (1.0f, (float) user.getWidth() * 0.94f / (float) design.getWidth(),
                                    (float) (user.getHeight() - 48) * 0.94f / (float) design.getHeight());

        auto frame = std::make_unique<ScaledFrame> (borrowed, std::move (owned), design);
        frame->setSize (juce::roundToInt ((float) design.getWidth() * s), juce::roundToInt ((float) design.getHeight() * s));

        auto w = std::make_unique<Window> (*this, key, title, nullptr);
        w->setContentOwned (frame.release(), true);
        w->setResizable (true, false);
        w->setResizeLimits (design.getWidth() / 4, design.getHeight() / 4, design.getWidth() * 2, design.getHeight() * 2);
        if (auto* c = w->getConstrainer())
            c->setFixedAspectRatio ((double) design.getWidth() / (double) design.getHeight());

        auto bounds = w->getBounds().withCentre (near != nullptr && near->isShowing() ? near->getScreenBounds().getCentre()
                                                                                       : user.getCentre());
        w->setBounds (bounds.constrainedWithin (user.withTrimmedTop (32)));
        w->setVisible (true);
        windows[key] = std::move (w);
    }

    namespace
    {
        //======================================================================
        //  A plugin's window's content: the header, then the plugin's own editor
        //======================================================================
        class PluginFrame final : public juce::Component,
                                  private juce::ComponentListener,
                                  private juce::AudioProcessorListener,
                                  private juce::Timer
        {
        public:
            static constexpr int kHeaderH = 46, kMinW = 600;

            PluginFrame (juce::AudioProcessorEditor* ed, juce::AudioProcessor* p, ZodiakProcessor& zp, int ch, const juce::String& title)
                : editor (ed), plugin (p), proc (zp), rackCh (ch), name (title)
            {
                addAndMakeVisible (*editor);
                editor->addComponentListener (this);

                onBtn.setClickingTogglesState (false);
                onBtn.setTooltip ("ON: the plugin plays.  OFF: it is bypassed (a VSTi falls silent).");
                onBtn.onClick = [this]
                {
                    if (auto* on = onFlag()) on->store (! on->load());
                    refreshHeader();
                };
                addAndMakeVisible (onBtn);

                linkZone.setInterceptsMouseClicks (true, false);
                linkZone.setTooltip ("MIDI LINK: drop a source here (from the mode bar's MIDI LINK), then move the control "
                                     "in the plugin it should turn.  Click: stop waiting.  Right-click: this plugin's links.");
                linkZone.onPaint = [this] (juce::Graphics& g) { paintLinkZone (g); };
                linkZone.onClick = [this] (const juce::MouseEvent& e)
                {
                    if (e.mods.isPopupMenu()) showLinksMenu();
                    else if (capturing.valid()) stopCapture ("");
                };
                addAndMakeVisible (linkZone);
                linkDropAction (linkZone, proc, [this] (const link::Source& s) { startCapture (s); });

                fitToEditor();
                refreshHeader();
                startTimerHz (12);
            }

            ~PluginFrame() override
            {
                stopTimer();
                if (listening) plugin->removeListener (this);
                if (editor != nullptr) editor->removeComponentListener (this);
                // The editor goes before the plugin does (WindowHub closes this first)
                if (editor != nullptr) plugin->editorBeingDeleted (editor.get());
                editor.reset();
            }

            void paint (juce::Graphics& g) override
            {
                auto h = getLocalBounds().removeFromTop (kHeaderH);
                g.setColour (win::row);
                g.fillRect (h);
                g.setColour (juce::Colour (0xFF33363C));
                g.fillRect (h.removeFromBottom (1));

                g.setColour (win::text);
                g.setFont (font (18.0f, true));
                g.drawFittedText (name, titleArea, juce::Justification::centredLeft, 1, 0.8f);
            }

            void resized() override
            {
                auto h = getLocalBounds().removeFromTop (kHeaderH).reduced (10, 6);
                onBtn.setBounds (h.removeFromRight (78));
                h.removeFromRight (10);
                linkZone.setBounds (h.removeFromRight (juce::jmin (330, h.getWidth() / 2)));
                h.removeFromRight (10);
                titleArea = h;

                if (editor != nullptr && ! fitting)
                {
                    const juce::ScopedValueSetter<bool> svs (fitting, true);
                    editor->setBounds (0, kHeaderH, editor->isResizable() ? getWidth() : editor->getWidth(),
                                       editor->isResizable() ? getHeight() - kHeaderH : editor->getHeight());
                }
            }

            // The header moves the window too (whatever the title bar does), kept on the screen
            void mouseDown (const juce::MouseEvent& e) override
            {
                if (e.y < kHeaderH)
                    if (auto* top = getTopLevelComponent())
                        dragger.startDraggingComponent (top, e.getEventRelativeTo (top));
            }

            void mouseDrag (const juce::MouseEvent& e) override
            {
                if (e.mouseDownPosition.y >= kHeaderH) return;
                if (auto* top = getTopLevelComponent())
                {
                    keepOn.setMinimumOnscreenAmounts (80, 120, 80, 120);
                    dragger.dragComponent (top, e.getEventRelativeTo (top), &keepOn);
                }
            }

        private:
            //------------------------------------------------------------------
            //  A thin component: the LINK zone
            //------------------------------------------------------------------
            struct Zone final : public juce::Component, public juce::SettableTooltipClient
            {
                std::function<void (juce::Graphics&)> onPaint;
                std::function<void (const juce::MouseEvent&)> onClick;
                void paint (juce::Graphics& g) override        { if (onPaint) onPaint (g); }
                void mouseUp (const juce::MouseEvent& e) override { if (onClick && ! isLinkAssigning()) onClick (e); }
            };

            // The editor's size sets the window's (a plugin that resizes itself)
            void componentMovedOrResized (juce::Component& c, bool, bool wasResized) override
            {
                if (&c == editor.get() && wasResized && ! fitting) fitToEditor();
            }

            void fitToEditor()
            {
                const juce::ScopedValueSetter<bool> svs (fitting, true);
                editor->setTopLeftPosition (0, kHeaderH);
                setSize (juce::jmax (kMinW, editor->getWidth()), editor->getHeight() + kHeaderH);
            }

            //------------------------------------------------------------------
            int slot() const                            { return link::slotOf (proc.engine(), rackCh, plugin); }

            std::atomic<bool>* onFlag()
            {
                const int s = slot();
                if (s < 0) return nullptr;
                if (rackCh != link::kMasterRack)
                {
                    auto* n = proc.engine().graph (rackCh).find (s);      // its node's bypass
                    return n != nullptr ? &n->on : nullptr;
                }
                auto& r = proc.engine().masterRack();
                return s >= 1 && s <= r.numInserts() ? &r.fx[(size_t) (s - 1)]->on : nullptr;
            }

            juce::String pluginName()
            {
                if (auto* h = link::findPlugin (proc.engine(), rackCh, slot(), {})) return h->name;
                return {};
            }

            void refreshHeader()
            {
                const auto* on = onFlag();
                const bool isOn = on == nullptr || on->load();
                onBtn.setButtonText (isOn ? "ON" : "OFF");
                onBtn.setColour (juce::TextButton::buttonColourId, isOn ? win::on : win::off);
                onBtn.setColour (juce::TextButton::textColourOffId, isOn ? juce::Colours::black : win::text);

                // Its on / bypass takes a MIDI LINK source too
                const int s = slot();
                if (s != linkedSlot)
                {
                    linkedSlot = s;
                    if (s >= 0) linkTarget (onBtn, proc, link::Target::pluginOn (rackCh, s, pluginName()));
                }
            }

            void timerCallback() override
            {
                refreshHeader();
                if (! capturing.valid()) return;

                // The control moved: a gesture's start first; a plain change after a moment
                int idx = gestured.exchange (-1);
                if (idx < 0)
                {
                    const int c = changed.load();
                    if (c >= 0 && juce::Time::getMillisecondCounter() - changedAt.load() > 350) idx = changed.exchange (-1);
                }
                if (idx < 0) return;

                const int s = slot();
                const auto& params = plugin->getParameters();
                if (s < 0 || idx >= params.size()) { stopCapture ("This plugin isn't in its rack any more"); return; }
                const auto source = capturing;
                proc.linkSource (source, link::Target::plugin (rackCh, s, idx, pluginName()));
                refreshLinkTargets();
                stopCapture (source.shortText() + "  ->  " + params[idx]->getName (40));
            }

            void startCapture (const link::Source& s)
            {
                capturing = s;
                gestured.store (-1);
                changed.store (-1);
                if (! listening) { plugin->addListener (this); listening = true; }
                status = "Move a control in the plugin...";
                linkZone.repaint();
            }

            void stopCapture (const juce::String& text)
            {
                capturing = {};
                if (listening) { plugin->removeListener (this); listening = false; }
                status = text;
                linkZone.repaint();
            }

            // Any thread
            void audioProcessorParameterChanged (juce::AudioProcessor*, int index, float) override
            {
                int expected = -1;
                if (changed.compare_exchange_strong (expected, index)) changedAt.store (juce::Time::getMillisecondCounter());
            }
            void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int index) override
            {
                int expected = -1;
                gestured.compare_exchange_strong (expected, index);
            }
            void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}

            void paintLinkZone (juce::Graphics& g)
            {
                auto b = linkZone.getLocalBounds().toFloat().reduced (1.0f);
                const bool waiting = capturing.valid();
                g.setColour (juce::Colour (0xFF121417));
                g.fillRoundedRectangle (b, 7.0f);
                g.setColour (waiting ? juce::Colour (0xFF3CB8E0) : juce::Colours::white.withAlpha (0.22f));
                g.drawRoundedRectangle (b, 7.0f, waiting ? 2.0f : 1.2f);

                auto r = linkZone.getLocalBounds().reduced (10, 2);
                g.setColour (juce::Colours::white.withAlpha (0.6f));
                g.setFont (font (12.5f, true));
                g.drawText ("MIDI LINK", r.removeFromLeft (78), juce::Justification::centredLeft, false);

                int n = 0;
                const auto me = pluginName();
                for (int i = 0; i < link::Map::kMaxLinks; ++i)
                {
                    const auto t = proc.links().targetOf (i);
                    if (t.kind == link::kPlugin && t.ch == rackCh && t.a == slot() && t.text == me && proc.links().sourceOf (i).valid()) ++n;
                }

                g.setColour (waiting ? juce::Colour (0xFF3CB8E0) : win::text);
                g.setFont (font (13.5f, waiting));
                const auto text = waiting ? capturing.shortText() + ":  move a control in the plugin"
                                : status.isNotEmpty() ? status
                                : (n > 0 ? juce::String (n) + (n == 1 ? " link - drop a source here" : " links - drop a source here")
                                         : juce::String ("drop a source here"));
                g.drawFittedText (text, r, juce::Justification::centredLeft, 1, 0.8f);
            }

            void showLinksMenu()
            {
                juce::PopupMenu m;
                m.addSectionHeader (pluginName() + ":  MIDI LINK");
                const auto me = pluginName();
                const int s = slot();
                for (int i = 0; i < link::Map::kMaxLinks; ++i)
                {
                    const auto t = proc.links().targetOf (i);
                    const auto src = proc.links().sourceOf (i);
                    if (! src.valid() || t.kind != link::kPlugin || t.ch != rackCh || t.a != s || t.text != me) continue;
                    m.addItem (1000 + i, src.text() + "  ->  " + link::pluginParamName (proc.engine(), t) + "   (forget)");
                }
                if (capturing.valid()) m.addItem (1, "Stop waiting for a control");
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&linkZone),
                                 [safe = juce::Component::SafePointer<PluginFrame> (this)] (int r)
                {
                    if (safe == nullptr || r == 0) return;
                    if (r == 1) { safe->stopCapture (""); return; }
                    const auto t = safe->proc.links().targetOf (r - 1000);
                    if (t.valid()) safe->proc.links().clear (t);
                    safe->status = {};
                    safe->linkZone.repaint();
                    refreshLinkTargets();
                });
            }

            std::unique_ptr<juce::AudioProcessorEditor> editor;
            juce::AudioProcessor* plugin;
            ZodiakProcessor& proc;
            const int rackCh;
            const juce::String name;

            juce::TextButton onBtn;
            Zone linkZone;
            juce::Rectangle<int> titleArea;
            juce::ComponentDragger dragger;
            juce::ComponentBoundsConstrainer keepOn;
            bool fitting = false, listening = false;
            int  linkedSlot = -2;

            link::Source capturing;
            std::atomic<int> gestured { -1 }, changed { -1 };
            std::atomic<juce::uint32> changedAt { 0 };
            juce::String status;
        };
    }

    void WindowHub::showPluginEditor (const juce::String& key, juce::AudioProcessor* plugin, const juce::String& title,
                                      juce::Component* near, ZodiakProcessor& proc, int rackCh)
    {
        if (plugin == nullptr) return;
        if (auto it = windows.find (key); it != windows.end())
        {
            it->second->setVisible (true);
            it->second->toFront (true);
            return;
        }

        auto* editor = plugin->createEditorIfNeeded();
        if (editor == nullptr)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, title, title + " has no window of its own.", "OK");
            return;
        }

        auto w = std::make_unique<Window> (*this, key, title, plugin);
        w->setContentOwned (new PluginFrame (editor, plugin, proc, rackCh, title), true);
        w->setResizable (editor->isResizable(), false);
        placeOnScreen (*w, near);
        w->setVisible (true);
        w->toFront (true);
        windows[key] = std::move (w);
    }

    void WindowHub::close (const juce::String& key)
    {
        if (auto it = windows.find (key); it != windows.end())
        {
            auto w = std::move (it->second);
            windows.erase (it);
            w->clearContentComponent();
        }
    }

    void WindowHub::closeLater (const juce::String& key)
    {
        // Not inside the window's own click
        juce::MessageManager::callAsync ([this, key, alive = std::weak_ptr<bool> (token)] { if (! alive.expired()) close (key); });
    }

    void WindowHub::closeAll()
    {
        auto all = std::move (windows);
        windows.clear();
        for (auto& w : all) w.second->clearContentComponent();
    }

    bool WindowHub::isOpen (const juce::String& key) const   { return windows.find (key) != windows.end(); }

    void WindowHub::closeEditorOf (juce::AudioProcessor* plugin)
    {
        for (auto it = windows.begin(); it != windows.end();)
            if (it->second->plugin == plugin)
            {
                auto w = std::move (it->second);
                it = windows.erase (it);
                w->clearContentComponent();       // the editor goes first, the plugin after
            }
            else
                ++it;
    }

    //==========================================================================
    //  The pieces of a strip
    //==========================================================================
    namespace
    {
        // Pan: a short line with a dot, its value under it ("<C>", "L40")
        class PanControl final : public juce::Component
        {
        public:
            std::function<float()>       getValue;
            std::function<void (float)>  setValue;

            void refresh()
            {
                const float v = getValue ? getValue() : 0.0f;
                if (std::abs (v - shown) > 1.0e-4f) { shown = v; repaint(); }
            }

            void paint (juce::Graphics& g) override
            {
                auto b = getLocalBounds().toFloat();
                auto line = b.removeFromTop (b.getHeight() * 0.5f);
                const float y = line.getCentreY();
                g.setColour (juce::Colour (0xFF4A4E55));
                g.drawLine (line.getX() + 10.0f, y, line.getRight() - 10.0f, y, 3.0f);
                const float x = line.getX() + 10.0f + (shown + 1.0f) * 0.5f * (line.getWidth() - 20.0f);
                g.setColour (juce::Colour (0xFFD5D8DC));
                g.fillEllipse (x - 6.0f, y - 6.0f, 12.0f, 12.0f);
                g.setColour (win::dim);
                g.setFont (font (12.0f));
                const auto text = std::abs (shown) < 0.01f ? juce::String ("<C>")
                                                           : (shown < 0 ? "L" : "R") + juce::String ((int) std::lround (std::abs (shown) * 100.0f));
                g.drawText (text, b, juce::Justification::centred, false);
            }

            void mouseDown (const juce::MouseEvent&) override   { start = shown; }
            void mouseDrag (const juce::MouseEvent& e) override { set (start + (float) e.getDistanceFromDragStartX() / juce::jmax (1.0f, (float) getWidth() - 20.0f) * 2.0f); }
            void mouseDoubleClick (const juce::MouseEvent&) override { set (0.0f); }

        private:
            void set (float v)
            {
                shown = juce::jlimit (-1.0f, 1.0f, v);
                if (setValue) setValue (shown);
                repaint();
            }

            float shown = 0.0f, start = 0.0f;
        };

        // A small toggle (M, S, the reverb's On)
        void styleSmallToggle (juce::TextButton& b, juce::Colour onColour, juce::Colour onText)
        {
            b.setClickingTogglesState (true);
            b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF3A3D43));
            b.setColour (juce::TextButton::buttonOnColourId, onColour);
            b.setColour (juce::TextButton::textColourOffId, win::label);
            b.setColour (juce::TextButton::textColourOnId, onText);
        }

        //======================================================================
        //  One item in a department: [on]  name ........ [load] [e]
        //======================================================================
        class ItemRow final : public juce::Component, public juce::SettableTooltipClient
        {
        public:
            std::function<bool()> isOn;
            std::function<void()> toggle, load, edit, menu;
            juce::String name;

            void paint (juce::Graphics& g) override
            {
                auto b = getLocalBounds().toFloat().reduced (0.0f, 2.0f);
                g.setColour (win::item);
                g.fillRoundedRectangle (b, 4.0f);
                g.setColour (win::itemLine);
                g.drawRoundedRectangle (b, 4.0f, 1.0f);

                const bool on = isOn == nullptr || isOn();
                lightArea = { b.getX() + 6.0f, b.getCentreY() - 8.0f, 16.0f, 16.0f };
                g.setColour (on ? win::on : win::off);
                g.fillEllipse (lightArea);
                g.setColour (juce::Colour (0xFF1C1F23));
                g.drawEllipse (lightArea, 1.5f);

                auto rest = b.withTrimmedLeft (30.0f);
                if (edit != nullptr)
                {
                    editArea = rest.removeFromRight (34.0f).reduced (3.0f, 4.0f);
                    g.setColour (win::off);
                    g.fillRoundedRectangle (editArea, 3.0f);
                    g.setColour (win::text);
                    g.setFont (font (19.0f, true));
                    g.drawText ("e", editArea, juce::Justification::centred, false);
                }
                if (load != nullptr)
                {
                    loadArea = rest.removeFromRight (34.0f).reduced (3.0f, 4.0f);
                    g.setColour (win::off);
                    g.fillRoundedRectangle (loadArea, 3.0f);
                    // a folder
                    auto f = loadArea.reduced (5.0f, 6.0f);
                    juce::Path p;
                    p.startNewSubPath (f.getX(), f.getBottom());
                    p.lineTo (f.getX(), f.getY());
                    p.lineTo (f.getX() + f.getWidth() * 0.4f, f.getY());
                    p.lineTo (f.getX() + f.getWidth() * 0.5f, f.getY() + f.getHeight() * 0.2f);
                    p.lineTo (f.getRight(), f.getY() + f.getHeight() * 0.2f);
                    p.lineTo (f.getRight(), f.getBottom());
                    p.closeSubPath();
                    g.setColour (win::text);
                    g.strokePath (p, juce::PathStrokeType (1.4f));
                }

                g.setColour (on ? win::text : win::dim);
                g.setFont (font (19.0f));
                g.drawFittedText (name, rest.reduced (4.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
            }

            void mouseDown (const juce::MouseEvent& e) override
            {
                if (e.mods.isPopupMenu()) { if (menu) menu(); return; }
                const auto p = e.position;
                if (lightArea.expanded (4.0f).contains (p))      { if (toggle) toggle(); repaint(); }
                else if (edit != nullptr && editArea.contains (p)) edit();
                else if (load != nullptr && loadArea.contains (p)) load();
            }

            void mouseDoubleClick (const juce::MouseEvent& e) override
            {
                if (! lightArea.expanded (4.0f).contains (e.position) && ! editArea.contains (e.position)
                    && ! loadArea.contains (e.position) && edit != nullptr)
                    edit();
            }

        private:
            juce::Rectangle<float> lightArea, editArea, loadArea;
        };

        //======================================================================
        //  A department: its title, its + (when it has one), its items
        //======================================================================
        class Department final : public juce::Component
        {
        public:
            Department (const juce::String& t, juce::String emptyText) : title (t), empty (std::move (emptyText)) {}

            std::function<void()> onPlus;
            std::vector<std::unique_ptr<ItemRow>> items;

            ItemRow& add()
            {
                items.push_back (std::make_unique<ItemRow>());
                addAndMakeVisible (*items.back());
                return *items.back();
            }

            void clear()    { items.clear(); }

            void paint (juce::Graphics& g) override
            {
                auto b = getLocalBounds().toFloat();
                g.setColour (win::dept);
                g.fillRoundedRectangle (b, 4.0f);
                g.setColour (win::stripLine);
                g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);

                g.setColour (win::dim);
                g.setFont (font (17.0f, true));
                g.drawText (title, juce::Rectangle<float> (10.0f, 0.0f, b.getWidth() - 40.0f, (float) kHeadH), juce::Justification::centredLeft, false);

                if (onPlus != nullptr)
                {
                    g.setColour (win::plus);
                    g.setFont (font (26.0f, true));
                    g.drawText ("+", plusArea(), juce::Justification::centred, false);
                }

                if (items.empty())
                {
                    g.setColour (win::dim);
                    g.setFont (font (17.0f));
                    g.drawText (empty, juce::Rectangle<float> (10.0f, (float) kHeadH, b.getWidth() - 20.0f, 24.0f), juce::Justification::centredLeft, false);
                }
            }

            void resized() override
            {
                auto r = getLocalBounds().reduced (6, 0);
                r.removeFromTop (kHeadH);
                r.removeFromBottom (4);
                // A long list draws its items closer together (down to 26 high)
                const int n = juce::jmax (1, (int) items.size());
                const int h = juce::jlimit (26, kItemH, r.getHeight() / n);
                for (auto& i : items)
                    i->setBounds (r.removeFromTop (h));
            }

            /** The height that shows one item: VSTI and SIDECHAIN hold one each. */
            static int oneItemHeight()   { return kHeadH + kItemH + 6; }

            void mouseDown (const juce::MouseEvent& e) override
            {
                if (onPlus != nullptr && plusArea().expanded (4.0f).contains (e.position)) onPlus();
            }

        private:
            juce::Rectangle<float> plusArea() const   { return { (float) getWidth() - 30.0f, 2.0f, 26.0f, (float) kHeadH - 4.0f }; }

            juce::String title, empty;
        };
    }

    //==========================================================================
    //  Strip: a channel's (0..7) or the master's (kMaster)
    //==========================================================================
    class MixerPanel::Strip final : public juce::Component
    {
    public:
        static constexpr int kMaster = -1;

        Strip (MixerPanel& m, int index) : mixer (m), proc (m.proc), id (index)
        {
            const bool channel = id >= 0;
            colours = channel ? &channelColours (id) : &masterColours();

            pan.getValue = [this] { return panValue().load(); };
            pan.setValue = [this] (float v) { panValue().store (v); };
            addAndMakeVisible (pan);

            styleSmallToggle (muteBtn, juce::Colour (0xFFD05050), juce::Colours::white);
            if (channel)
            {
                styleSmallToggle (soloBtn, juce::Colour (0xFFE0C040), juce::Colours::black);
                auto& st = proc.engine().channel (id);
                muteBtn.onClick = [this, &st] { st.chan[cpMute].store (muteBtn.getToggleState() ? 1.0f : 0.0f); };
                soloBtn.onClick = [this, &st] { st.chan[cpSolo].store (soloBtn.getToggleState() ? 1.0f : 0.0f); };
                addAndMakeVisible (soloBtn);

                // GRAPH: the channel's effects, Colosseum's way
                graphBtn.setColour (juce::TextButton::buttonColourId, colours->dark);
                graphBtn.setColour (juce::TextButton::textColourOffId, colours->lightText);
                graphBtn.setTooltip ("The channel's graph: its VSTi and its effects, and how they're wired");
                graphBtn.onClick = [this] { if (mixer.openGraph) mixer.openGraph (id, &graphBtn); };
                addAndMakeVisible (graphBtn);
            }
            else
                muteBtn.onClick = [this] { proc.engine().master (mpMute).store (muteBtn.getToggleState() ? 1.0f : 0.0f); };
            addAndMakeVisible (muteBtn);

            fader.getDb = [this] { return faderValue().load(); };
            fader.setDb = [this] (float db) { faderValue().store (db); };
            fader.setTooltip ("Drag; double-click: 0 dB");
            addAndMakeVisible (fader);
            addAndMakeVisible (meterL);
            addAndMakeVisible (meterR);

            if (! channel)
            {
                arrowBtn.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
                arrowBtn.setColour (juce::TextButton::textColourOffId, win::arrow);
                arrowBtn.setTooltip ("Show or hide the master's VST FX");
                arrowBtn.onClick = [this] { setOpen (! isOpen()); };
                addAndMakeVisible (arrowBtn);

                fxDep = std::make_unique<Department> ("VST FX", "Drop an effect here");
                fxDep->onPlus = [this] { fxMenu(); };
                addChildComponent (*fxDep);
            }

            // MIDI LINK: every control of the strip takes a source
            if (channel)
            {
                linkTarget (pan, proc, link::Target::chan (id, cpPan));
                linkTarget (fader, proc, link::Target::chan (id, cpVolume));
                linkTarget (muteBtn, proc, link::Target::chan (id, cpMute));
                linkTarget (soloBtn, proc, link::Target::chan (id, cpSolo));
            }
            else
            {
                linkTarget (pan, proc, link::Target::master (mpPan));
                linkTarget (fader, proc, link::Target::master (mpVolume));
                linkTarget (muteBtn, proc, link::Target::master (mpMute));
            }

            rebuildItems();
            refresh();
        }

        int preferredWidth() const   { return kFaderW + (isOpen() ? 4 + kDeptW : 0); }

        // The fader's part: a channel's strip is all fader part (as wide as the room
        // gives it); the master's, kFaderW (its VST FX beside it)
        int faderW() const           { return id >= 0 ? getWidth() : kFaderW; }

        bool isOpen() const          { return id == kMaster && proc.isMasterOpen(); }

        void setOpen (bool open)
        {
            if (id != kMaster) return;
            proc.setMasterOpen (open);
            mixer.layoutStrips();
        }

        //======================================================================
        void rebuildItems()
        {
            if (fxDep != nullptr)
            {
                auto& r = rackOf();
                fxDep->clear();
                int slot = 0;
                for (auto& ins : r.fx)
                {
                    auto& item = fxDep->add();
                    const int uid = ins->uid;
                    item.name = ins->name();
                    ++slot;
                    if (ins->plugin != nullptr)          // a plugin's on / bypass takes a MIDI LINK source
                        linkTarget (item, proc, link::Target::pluginOn (link::kMasterRack, slot, ins->plugin->name));
                    item.isOn   = [this, uid] { auto* i = rackOf().findInsert (uid); return i != nullptr && i->on.load(); };
                    item.toggle = [this, uid] { if (auto* i = rackOf().findInsert (uid)) i->on.store (! i->on.load()); };
                    item.load   = [this, uid] { chooseAndLoad (uid); };
                    item.edit   = [this, uid, &item] { editInsert (uid, &item); };
                    item.menu   = [this, uid] { insertMenu (uid); };
                    item.setTooltip (ins->plugin != nullptr ? (ins->plugin->sonifyId.isNotEmpty() ? juce::String ("Sonify's own plugin") : ins->plugin->file.getFullPathName())
                                                            : juce::String ("Built in. Right-click: remove"));
                }
                fxDep->resized();
                fxDep->repaint();
            }
            resized();
            repaint();
        }

        //======================================================================
        void refresh()
        {
            pan.refresh();
            fader.refresh();

            if (id >= 0)
            {
                auto& st = proc.engine().channel (id);
                muteBtn.setToggleState (st.chan[cpMute].load() > 0.5f, juce::dontSendNotification);
                soloBtn.setToggleState (st.chan[cpSolo].load() > 0.5f, juce::dontSendNotification);
                meterL.push (st.peakL.exchange (0.0f));
                meterR.push (st.peakR.exchange (0.0f));
                const auto name = proc.instrumentName (id);
                if (name != shownInstrument) { shownInstrument = name; repaint(); }
            }
            else
            {
                muteBtn.setToggleState (proc.engine().master (mpMute).load() > 0.5f, juce::dontSendNotification);
                meterL.push (proc.engine().takeMasterPeak (0));
                meterR.push (proc.engine().takeMasterPeak (1));
            }

            const auto text = dbText (faderValue().load());
            if (text != shownDb) { shownDb = text; repaint (dbArea.toNearestInt()); }

            if (fxDep != nullptr && fxDep->isVisible())
                for (auto& i : fxDep->items) i->repaint();
        }

        //======================================================================
        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            auto fs = b.removeFromLeft ((float) faderW());
            g.setColour (win::strip);
            g.fillRoundedRectangle (fs, 6.0f);
            g.setColour (win::stripLine);
            g.drawRoundedRectangle (fs.reduced (0.5f), 6.0f, 1.0f);

            // Its level
            g.setColour (win::text);
            g.setFont (font (15.0f, true));
            g.drawText (shownDb, dbArea, juce::Justification::centred, false);

            // The scale beside the fader
            g.setColour (win::dim);
            g.setFont (font (11.0f));
            const auto f = fader.getBounds().toFloat().reduced (0.0f, 13.0f);
            for (float db : { 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -36.0f, -48.0f })
            {
                const float y = f.getBottom() - Fader::dbToNorm (db) * f.getHeight();
                g.drawText (juce::String ((int) db), juce::Rectangle<float> ((float) meterR.getRight() + 2.0f, y - 7.0f, 26.0f, 14.0f),
                            juce::Justification::centredLeft, false);
            }

            // What the channel plays (its first VSTi)
            if (id >= 0)
            {
                g.setColour (shownInstrument.isEmpty() ? win::dim : win::text);
                g.setFont (font (12.0f));
                g.drawFittedText (shownInstrument.isEmpty() ? juce::String ("no VSTi") : shownInstrument,
                                  instArea.toNearestInt(), juce::Justification::centred, 2, 0.8f);
            }

            // Its name, in its colour
            g.setColour (colours->dark);
            g.fillRoundedRectangle (nameArea, 4.0f);
            g.setColour (juce::Colours::white);
            g.setFont (font (15.0f, true));
            g.drawText (id >= 0 ? "Ch " + juce::String (id + 1) : juce::String ("Master"), nameArea, juce::Justification::centred, false);
        }

        void resized() override
        {
            auto b = getLocalBounds();
            auto fs = b.removeFromLeft (faderW()).reduced (8, 10);

            pan.setBounds (fs.removeFromTop (42).withSizeKeepingCentre (juce::jmin (fs.getWidth(), kFaderW - 16), 42));
            fs.removeFromTop (6);
            auto ms = fs.removeFromTop (34).withSizeKeepingCentre (juce::jmin (fs.getWidth(), kFaderW - 16), 34);
            if (id >= 0)
            {
                muteBtn.setBounds (ms.removeFromLeft (ms.getWidth() / 2).reduced (3, 0));
                soloBtn.setBounds (ms.reduced (3, 0));
            }
            else
                muteBtn.setBounds (ms.withSizeKeepingCentre (48, 34));

            fs.removeFromTop (8);
            dbArea = fs.removeFromTop (24).toFloat();
            fs.removeFromTop (6);

            auto bottom = fs.removeFromBottom (id >= 0 ? 116 : 82);
            nameArea = bottom.removeFromBottom (40).toFloat();
            if (id >= 0)
            {
                instArea = bottom.removeFromBottom (36).toFloat();
                graphBtn.setBounds (bottom.removeFromTop (34).reduced (4, 0));
            }
            else
            {
                arrowBtn.setBounds (bottom.removeFromTop (34).withSizeKeepingCentre (40, 30));
                arrowBtn.setButtonText (isOpen() ? "<" : ">");
            }

            auto meters = fs.withSizeKeepingCentre (juce::jmin (fs.getWidth(), kFaderW - 16), fs.getHeight());
            fader.setBounds (meters.removeFromLeft (38));
            meters.removeFromLeft (6);
            meterL.setBounds (meters.removeFromLeft (9).reduced (0, 13));
            meters.removeFromLeft (4);
            meterR.setBounds (meters.removeFromLeft (9).reduced (0, 13));

            // The master's VST FX
            if (fxDep != nullptr)
            {
                fxDep->setVisible (isOpen());
                if (isOpen()) fxDep->setBounds (b.withTrimmedLeft (4));
            }
        }

    private:
        rack::Rack& rackOf()   { return proc.engine().masterRack(); }

        std::atomic<float>& panValue()
        {
            return id >= 0 ? proc.engine().channel (id).chan[cpPan] : proc.engine().master (mpPan);
        }

        std::atomic<float>& faderValue()
        {
            return id >= 0 ? proc.engine().channel (id).chan[cpVolume] : proc.engine().master (mpVolume);
        }

        //======================================================================
        void fxMenu()
        {
            juce::PopupMenu m;
            const bool full = rackOf().numInserts() >= rack::Rack::kMaxInserts;
            m.addItem (1, "Wah", ! full);
            m.addItem (2, "Phaser", ! full);
            m.addSeparator();
            m.addItem (3, "Load VST from file...", ! full);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (fxDep.get()),
                [safe = juce::Component::SafePointer<Strip> (this)] (int res)
                {
                    if (safe == nullptr || res == 0) return;
                    if (res == 1 || res == 2)
                    {
                        safe->rackOf().addBuiltIn (res == 1 ? inserts::kWah : inserts::kPhaser);
                        safe->proc.rackChanged();
                    }
                    if (res == 3) safe->chooseAndLoad (0);
                });
        }

        void insertMenu (int uid)
        {
            auto* ins = rackOf().findInsert (uid);
            if (ins == nullptr) return;
            juce::PopupMenu m;
            m.addSectionHeader (ins->name());
            m.addItem (1, "Remove");
            m.showMenuAsync ({}, [safe = juce::Component::SafePointer<Strip> (this), uid] (int res)
            {
                if (safe == nullptr || res != 1) return;
                safe->mixer.hub.close (safe->insertKey (uid));
                safe->rackOf().removeInsert (uid);
                safe->proc.rackChanged();
            });
        }

        juce::String insertKey (int uid) const   { return "fx:master:" + juce::String (uid); }

        void editInsert (int uid, juce::Component* near)
        {
            auto* ins = rackOf().findInsert (uid);
            if (ins == nullptr) return;
            if (ins->builtIn != nullptr)
                mixer.hub.showPanel (insertKey (uid), "Master  -  " + ins->name(),
                                     std::make_unique<InsertPanel> (*ins->builtIn, *colours), near);
            else if (ins->plugin != nullptr)
                mixer.hub.showPluginEditor (insertKey (uid), ins->plugin->instance.get(), "Master  -  " + ins->name(), near,
                                            proc, link::kMasterRack);
        }

        // Load: a VST (new, or in place of an insert).  A plugin just loaded opens its
        // window, this once; from then on it opens and closes only by hand.
        void chooseAndLoad (int replaceUid)
        {
            rack::pickSource (false, this,
                [safe = juce::Component::SafePointer<Strip> (this), replaceUid] (const rack::Source& source)
                {
                    if (safe == nullptr) return;
                    auto& eng = safe->proc.engine();
                    rack::loadPlugin (source, eng.sampleRate(), eng.maxBlock(), false,
                        [safe, replaceUid] (std::unique_ptr<rack::Hosted> h, const juce::String& error)
                        {
                            if (safe == nullptr) return;
                            if (h == nullptr)
                            {
                                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Not loaded", error, "OK");
                                return;
                            }
                            auto& e = safe->proc.engine();
                            if (replaceUid != 0)
                            {
                                safe->mixer.hub.close (safe->insertKey (replaceUid));
                                if (e.replaceMasterPlugin (replaceUid, std::move (h)))
                                    safe->editInsert (replaceUid, safe.getComponent());
                            }
                            else if (const int uid = e.addMasterPlugin (std::move (h)); uid == 0)
                                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "VST FX full",
                                                                        "The master takes 8 effects.", "OK");
                            else
                                safe->editInsert (uid, safe.getComponent());
                            safe->proc.rackChanged();
                        });
                });
        }

        MixerPanel& mixer;
        ZodiakProcessor& proc;
        const int id;
        const ChannelColours* colours = nullptr;

        PanControl pan;
        juce::TextButton muteBtn { "M" }, soloBtn { "S" }, arrowBtn { ">" }, graphBtn { "GRAPH" };
        Fader fader;
        LevelMeter meterL, meterR;
        juce::String shownDb, shownInstrument;
        juce::Rectangle<float> dbArea, nameArea, instArea;

        std::unique_ptr<Department> fxDep;
    };

    //==========================================================================
    //  MixerPanel
    //==========================================================================
    MixerPanel::MixerPanel (ZodiakProcessor& p, WindowHub& h, std::function<void (int, juce::Component*)> graphOpener)
        : openGraph (std::move (graphOpener)), proc (p), hub (h)
    {
        title.setText ("Mixer", juce::dontSendNotification);
        title.setFont (font (17.0f, true));
        title.setColour (juce::Label::textColourId, win::text);
        addAndMakeVisible (title);

        view.setViewedComponent (&holder, false);
        view.setScrollBarsShown (false, true, false, true);
        view.setScrollBarThickness (14);
        addAndMakeVisible (view);

        for (int ch = 0; ch < kNumChannels; ++ch)
        {
            strips[(size_t) ch] = std::make_unique<Strip> (*this, ch);
            holder.addAndMakeVisible (*strips[(size_t) ch]);
        }
        masterStrip = std::make_unique<Strip> (*this, Strip::kMaster);
        addAndMakeVisible (*masterStrip);

        shownRackGen = proc.rackGeneration();
        startTimerHz (30);
    }

    MixerPanel::~MixerPanel()   { stopTimer(); }

    void MixerPanel::rebuild()
    {
        for (auto& s : strips) s->rebuildItems();
        masterStrip->rebuildItems();
        layoutStrips();
    }

    void MixerPanel::paint (juce::Graphics& g)
    {
        g.setColour (win::stripLine);
        g.drawHorizontalLine (0, 12.0f, (float) getWidth() - 12.0f);
    }

    void MixerPanel::resized()
    {
        auto r = getLocalBounds().reduced (12, 0);
        title.setBounds (r.removeFromTop (kTitleH).withTrimmedTop (4));
        layoutStrips();
    }

    void MixerPanel::layoutStrips()
    {
        auto r = getLocalBounds().reduced (12, 0);
        r.removeFromTop (kTitleH);
        r.removeFromBottom (8);

        // On the right, always: the master
        const int mw = masterStrip->preferredWidth();
        masterStrip->setBounds (r.removeFromRight (mw));
        r.removeFromRight (kGap + 6);

        view.setBounds (r);
        int x = 0;
        const int h = r.getHeight() - 16;
        const int each = juce::jlimit (kFaderW, kFaderW * 2, (r.getWidth() - (kNumChannels - 1) * kGap) / kNumChannels);
        for (auto& s : strips)
        {
            const int w = each;
            s->setBounds (x, 0, w, h);
            x += w + kGap;
        }
        holder.setSize (juce::jmax (r.getWidth(), x - kGap), h);
        for (auto& s : strips) s->resized();
        masterStrip->resized();
    }

    void MixerPanel::timerCallback()
    {
        if (proc.rackGeneration() != shownRackGen)
        {
            shownRackGen = proc.rackGeneration();
            rebuild();
        }
        for (auto& s : strips) s->refresh();
        masterStrip->refresh();
    }
}
