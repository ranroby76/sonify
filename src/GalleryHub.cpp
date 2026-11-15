// D:\workspace\Fanan_web_hub\src\GalleryHub.cpp

#include "GalleryHub.h"

#include "BodyEditor.h"
#include "Body3D.h"
#include "IconBrowser.h"
#include "KnobStudio.h"
#include "MaterialArchetypes.h"
#include "ObjectPaint.h"
#include "PanelLayouts.h"
#include "MatrixStudio.h"
#include "ModMatrix.h"
#include "PerformControl.h"
#include "PerformStudio.h"
#include "PluginCanvas.h"
#include "PresetPanel.h"
#include "PresetStudio.h"
#include "Render3D.h"
#include "SelectorSet.h"
#include "SelectorStudio.h"
#include "StellaLookAndFeel.h"

namespace
{
    const juce::StringArray pageNames { "Knobs, sliders, switches", "3D models", "Selector sets", "Performance controls", "Mod matrices",
                                        "Preset managers", "3D bodies", "Icons", "Guitar FX icons", "Shapes", "Material looks", "Layouts" };

    const juce::StringArray pageTips {
        "The Knob Gallery: drag one onto the window (or onto a control: it wears that look), or double-click it to add it",
        "The 3D Gallery: 3D knobs, buttons, switches and sliders, rendered; drag one onto the window or a control",
        "Selector sets: keys of their own for a selector (rows, rings, grids, tabs, a D-pad...)",
        "Performance controls: pitch and mod wheels, bender levers, joysticks, glowing pads, ribbons",
        "Mod matrices: slots, a pin grid, a patch bay with cables; jacks, source chips and assign buttons",
        "Preset managers: the bar, drop lists, tables of buttons, side panels",
        "3D bodies: the instrument's whole case, behind the controls",
        "The icon library: some 13,000 icons and symbols to show in place of words",
        "Guitar effects' icons on badges (drive, dynamics, filters, modulation, pitch, time, ambience, amps, utilities): "
        "drag one onto the window; its badge's shape and colours are set in its menu",
        "Shapes kept from the Knob Studio's layers, put on the panel as pictures",
        "Material looks: a panel's material (dropped on a shape), or the whole window's; its menu: with its 3D controls, "
        "displays, fonts and painted panel too",
        "Layouts: a whole window for a kind of plugin; Fanan AI lays this window out from one, with this plugin's own controls" };

    /** A material look's card: its window background, a panel of it lit as it's lit, its words
        and its accent (any thread). */
    void paintLookCard (juce::Graphics& g, const juce::String& name, juce::Rectangle<float> area)
    {
        const auto look = MaterialArchetypes::find (name);

        if (! look.isObject())
            return;

        const auto background = look.getProperty ("background", {});
        g.setGradientFill (juce::ColourGradient (MaterialArchetypes::colourOf (background.getProperty ("top", {})), area.getTopLeft(),
                                                 MaterialArchetypes::colourOf (background.getProperty ("bottom", {})), area.getBottomLeft(), false));
        g.fillRoundedRectangle (area, 4.0f);

        const auto panelArea = area.reduced (12.0f, 10.0f);
        GuiWidget panel;
        panel.type = GuiWidget::Type::shape;
        panel.settings["shape"] = "rectangle";
        panel.bounds = { 0, 0, juce::roundToInt (panelArea.getWidth()), juce::roundToInt (panelArea.getHeight()) };
        MaterialArchetypes::applyToWidget (panel, look);

        const auto light = look.getProperty ("light", {});
        const auto angle = (float) (double) light.getProperty ("angle", 120.0), altitude = (float) (double) light.getProperty ("altitude", 30.0);
        const auto painting = ObjectPaint::paint (ObjectPaint::jobFor (panel, panel.colour, angle, altitude));

        if (painting.image.isValid())
            g.drawImageAt (painting.image, juce::roundToInt (panelArea.getCentreX() + painting.offset.x),
                           juce::roundToInt (panelArea.getCentreY() + painting.offset.y));

        // Its words and its accent, as a panel of it would show them.
        auto words = panelArea.reduced (14.0f, 12.0f);
        g.setColour (MaterialArchetypes::colourOf (look.getProperty ("textColor", {})));
        g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
        g.drawText ("Aa", words.removeFromTop (22.0f), juce::Justification::topLeft, false);
        g.setColour (MaterialArchetypes::colourOf (look.getProperty ("accentColor", {})));
        g.fillRoundedRectangle (words.removeFromBottom (5.0f).withWidth (words.getWidth() * 0.4f), 2.5f);
    }

    /** A layout's card: its window drawn plainly (any thread): groups, knobs, sliders, displays,
        the title and the keyboard, fitted into the card. */
    void paintLayoutCard (juce::Graphics& g, const juce::var& layout, juce::Rectangle<float> area)
    {
        const auto W = (float) juce::jmax (1, (int) layout.getProperty ("width", 1)), H = (float) juce::jmax (1, (int) layout.getProperty ("height", 1));
        const auto scale = juce::jmin (area.getWidth() / W, area.getHeight() / H);
        const auto window = juce::Rectangle<float> (W * scale, H * scale).withCentre (area.getCentre());
        const auto pages = layout.getProperty ("pages", {});
        const auto firstPage = pages.size() > 0 ? pages[0].toString() : juce::String();

        g.setColour (juce::Colour (0xff26282c));
        g.fillRoundedRectangle (window, 3.0f);

        const auto at = [&] (const juce::var& w, float wide, float tall)
        {
            return juce::Rectangle<float> (window.getX() + (float) (double) w.getProperty ("x", 0) * scale,
                                           window.getY() + (float) (double) w.getProperty ("y", 0) * scale, wide * scale, tall * scale);
        };

        if (const auto* widgets = layout.getProperty ("widgets", {}).getArray())
        {
            for (const auto& w : *widgets)
            {
                const auto type = w.getProperty ("type", {}).toString();

                if (const auto page = w.getProperty ("page", {}).toString(); page.isNotEmpty() && page != firstPage)
                    continue;

                const auto wide = (float) (double) w.getProperty ("w", 0), tall = (float) (double) w.getProperty ("h", 0);

                if (type == "knob")
                {
                    const auto size = (float) (double) w.getProperty ("size", 48);
                    const auto r = at (w, size, size);
                    g.setColour (juce::Colour (0xff111113));
                    g.fillEllipse (r);
                    g.setColour (juce::Colour (0xffa8a8ae));
                    g.drawEllipse (r, 1.0f);
                    g.drawLine (r.getCentreX(), r.getCentreY(), r.getCentreX(), r.getY() + r.getHeight() * 0.12f, 1.0f);
                }
                else if (type == "group")
                {
                    g.setColour (juce::Colour (0xff6e737d));
                    g.drawRoundedRectangle (at (w, wide, tall), 3.0f, 1.0f);
                }
                else if (type == "label")
                {
                    const auto r = at (w, wide, tall);

                    if (w.hasProperty ("param"))
                    {
                        g.setColour (juce::Colour (0xff0b0b0c));
                        g.fillRect (r);
                    }
                    else if ((double) w.getProperty ("size", 14) >= 18.0)
                    {
                        g.setColour (juce::Colour (0xffe8e8e8));
                        g.fillRect (r.withSizeKeepingCentre (r.getWidth() * 0.8f, juce::jmax (2.0f, r.getHeight() * 0.35f)).withX (r.getX()));
                    }
                }
                else if (type == "keyboard")
                {
                    const auto r = at (w, wide, tall);
                    g.setColour (juce::Colour (0xffe6e6e6));
                    g.fillRect (r);
                    g.setColour (juce::Colour (0xff222222));

                    for (float x = r.getX() + 5.0f * scale * 2.0f; x < r.getRight(); x += 14.0f * scale)
                        g.drawVerticalLine (juce::roundToInt (x), r.getY(), r.getBottom());
                }
                else if (type != "tabs" && type != "preset" && type != "lamp")
                {
                    const auto r = at (w, wide, tall);
                    g.setColour (type == "slider" ? juce::Colour (0xff3c5a8c) : type == "switch" ? juce::Colour (0xff78503c)
                               : type == "selector" || type == "dropdown" ? juce::Colour (0xff466e5a) : type == "meter" ? juce::Colour (0xff287840)
                               : type == "xy" ? juce::Colour (0xff503c6e) : juce::Colour (0xff143c46));
                    g.fillRect (r);
                }
                else
                {
                    g.setColour (type == "lamp" ? juce::Colour (0xffc8a028) : juce::Colour (0xff5a5a5e));
                    g.fillRect (at (w, wide, tall));
                }
            }
        }
    }

    /** The one Gallery window, deleted when it's closed. */
    class HubWindow final : public juce::DocumentWindow
    {
    public:
        HubWindow (std::unique_ptr<GalleryHub> content, juce::Component* near)
            : juce::DocumentWindow ("Gallery", Theme::panel, juce::DocumentWindow::closeButton)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (content.release(), true);
            setResizable (true, false);

            if (near != nullptr)
                centreAroundComponent (near, getWidth(), getHeight());
            else
                centreWithSize (getWidth(), getHeight());

            setVisible (true);
        }

        void closeButtonPressed() override
        {
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<juce::Component> (this)] { delete safe.getComponent(); });
        }

        bool keyPressed (const juce::KeyPress& key) override
        {
            if (key == juce::KeyPress::escapeKey)
            {
                closeButtonPressed();
                return true;
            }

            return juce::DocumentWindow::keyPressed (key);
        }
    };

    juce::Component::SafePointer<HubWindow>& openWindow()
    {
        static juce::Component::SafePointer<HubWindow> window;
        return window;
    }
}

//==============================================================================
/** A studio's gallery (or the bodies'): its templates and the ones kept there, as cards with
    their pictures, drawn on a thread of their own. */
class GalleryHub::Designs final : public juce::Component
{
public:
    Designs (PluginCanvas& c, Page p) : canvas (c), page (p)
    {
        hint.setFont (Theme::font (12.5f));
        hint.setColour (juce::Label::textColourId, Theme::muted);
        hint.setText (page == Page::bodies ? "Drag one onto the window: its body (Undo brings the old one back). Right-click: more."
                    : page == Page::materialLooks ? "Drag one onto a panel (a shape): its material; elsewhere: every panel's. Right-click: "
                                                    "with its 3D controls, displays, fonts and painted panel too."
                    : page == Page::layouts ? "Drag one onto the window (or right-click): Fanan AI lays the window out from it, with this "
                                              "plugin's own controls. Then give it a material look."
                                           : "Drag one onto the window: dropped on an element of its kind it wears it, elsewhere it's a new one. "
                                             "Right-click: more.",
                      juce::dontSendNotification);
        addAndMakeVisible (hint);

        view.setViewedComponent (&grid, false);
        view.setScrollBarsShown (true, false);
        addAndMakeVisible (view);
        grid.owner = this;
        reload();
    }

    ~Designs() override
    {
        painter.removeAllJobs (true, 4000);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        hint.setBounds (r.removeFromTop (22));
        r.removeFromTop (6);
        view.setBounds (r);
        layOut();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::panel);
    }

private:
    struct Item
    {
        juce::String name;
        juce::var design, settings;
        int keys = 4, width = 0, height = 0;   // a set's keys; a body's window
        juce::File file;
        juce::Image picture;
    };

    struct Grid final : public juce::Component
    {
        Designs* owner = nullptr;
        int hovered = -1;

        void paint (juce::Graphics& g) override
        {
            g.fillAll (Theme::inset);

            for (size_t i = 0; i < owner->items.size(); ++i)
            {
                const auto card = owner->cardBounds ((int) i);

                if (! g.clipRegionIntersects (card))
                    continue;

                const auto& item = owner->items[i];
                g.setColour ((int) i == hovered ? Theme::raised.brighter (0.12f) : Theme::raised);
                g.fillRoundedRectangle (card.toFloat(), 6.0f);

                const auto pictureArea = card.reduced (8).withTrimmedBottom (20);

                if (item.picture.isValid())
                    g.drawImage (item.picture, pictureArea.toFloat(), juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
                else
                {
                    g.setColour (Theme::muted);
                    g.drawText (juce::String::fromUTF8 ("\xe2\x80\xa6"), pictureArea, juce::Justification::centred);
                }

                g.setColour (Theme::text);
                g.setFont (Theme::font (12.5f, item.file == juce::File()));
                g.drawText (item.name, card.reduced (10, 4).removeFromBottom (18), juce::Justification::centredLeft, true);
            }
        }

        int itemAt (juce::Point<int> p) const
        {
            for (size_t i = 0; i < owner->items.size(); ++i)
                if (owner->cardBounds ((int) i).contains (p))
                    return (int) i;

            return -1;
        }

        void mouseMove (const juce::MouseEvent& e) override
        {
            if (const auto now = itemAt (e.getPosition()); now != hovered)
            {
                hovered = now;
                repaint();
            }
        }

        void mouseExit (const juce::MouseEvent&) override
        {
            hovered = -1;
            repaint();
        }

        void mouseDown (const juce::MouseEvent&) override
        {
            dragging = false;
        }

        /** Dragged onto the window. */
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragging || e.mods.isPopupMenu() || e.getDistanceFromDragStart() < 5)
                return;

            const auto index = itemAt (e.getMouseDownPosition());
            auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);

            if (index < 0 || container == nullptr)
                return;

            dragging = true;
            const auto& item = owner->items[(size_t) index];
            auto* description = new juce::DynamicObject();
            description->setProperty ("galleryDesign", owner->page == Page::bodies ? juce::String ("body") : owner->kind());
            description->setProperty ("design", item.design.clone());
            description->setProperty ("settings", item.settings);
            description->setProperty ("keys", item.keys);
            description->setProperty ("width", item.width);
            description->setProperty ("height", item.height);

            // Its picture, smaller, under the mouse.
            juce::Image image (juce::Image::ARGB, 120, 76, true);

            if (item.picture.isValid())
            {
                juce::Graphics g (image);
                g.drawImage (item.picture, image.getBounds().toFloat(), juce::RectanglePlacement::centred);
            }

            const juce::Point<int> offset (-60, -38);
            container->startDragging (juce::var (description), this, juce::ScaledImage (image), true, &offset);
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            const auto index = itemAt (e.getPosition());

            if (index >= 0 && e.mods.isPopupMenu() && ! dragging)
                owner->showMenu (index);

            dragging = false;
        }

        bool dragging = false;
    };

    PluginCanvas& canvas;
    const Page page;
    std::vector<Item> items;
    juce::Label hint;
    juce::Viewport view;
    Grid grid;
    juce::ThreadPool painter { juce::ThreadPoolOptions{}.withNumberOfThreads (1) };
    int generation = 0;

    static constexpr int cardWidth = 250, cardHeight = 156, gap = 10;

    int columns() const   { return juce::jmax (1, (view.getMaximumVisibleWidth() - gap) / (cardWidth + gap)); }

    juce::Rectangle<int> cardBounds (int i) const
    {
        const auto across = columns();
        return { gap + (i % across) * (cardWidth + gap), gap + (i / across) * (cardHeight + gap), cardWidth, cardHeight };
    }

    void layOut()
    {
        const auto rows = ((int) items.size() + columns() - 1) / columns();
        grid.setSize (juce::jmax (1, view.getMaximumVisibleWidth()), juce::jmax (view.getMaximumVisibleHeight(), gap + rows * (cardHeight + gap)));
        grid.repaint();
    }

    juce::String kind() const
    {
        return page == Page::selectorSets ? "set" : page == Page::perform ? "perform" : page == Page::matrix ? "matrix"
             : page == Page::materialLooks ? "materialLook" : page == Page::layouts ? "layout" : "presets";
    }

    void reload()
    {
        painter.removeAllJobs (true, 4000);
        ++generation;
        items.clear();
        const auto& layout = canvas.getLayout();
        const int W = juce::jmax (8, layout.width), H = juce::jmax (8, layout.height);

        auto kept = [this] (const juce::File& folder, const juce::String& extension, const char* key, auto&& add)
        {
            for (const auto& file : folder.findChildFiles (juce::File::findFiles, false, "*" + extension))
            {
                const auto json = juce::JSON::parse (file);

                if (json.getProperty (key, {}).isObject())
                    add (json, file);
            }
        };

        if (page == Page::selectorSets)
        {
            const auto names = SelectorSet::templateNames();

            for (int i = 0; i < names.size(); ++i)
                items.push_back ({ names[i], SelectorSet::makeTemplate (i), {}, SelectorSet::templateKeys (i), 0, 0, {}, {} });

            kept (SelectorSet::galleryFolder(), SelectorSet::fileExtension, "set", [this] (const juce::var& json, const juce::File& file)
            {
                items.push_back ({ json.getProperty ("name", file.getFileNameWithoutExtension()).toString(), json.getProperty ("set", {}), {},
                                   juce::jlimit (1, 32, (int) json.getProperty ("keys", 4)), 0, 0, file, {} });
            });
        }
        else if (page == Page::perform)
        {
            const auto names = PerformControl::templateNames();

            for (int i = 0; i < names.size(); ++i)
                items.push_back ({ names[i], PerformControl::makeTemplate (i), PerformControl::templateSettings (i), 0, 0, 0, {}, {} });

            kept (PerformControl::galleryFolder(), PerformControl::fileExtension, "perform", [this] (const juce::var& json, const juce::File& file)
            {
                items.push_back ({ json.getProperty ("name", file.getFileNameWithoutExtension()).toString(), json.getProperty ("perform", {}),
                                   json.getProperty ("settings", {}), 0, 0, 0, file, {} });
            });
        }
        else if (page == Page::matrix)
        {
            const auto names = ModMatrix::templateNames();

            for (int i = 0; i < names.size(); ++i)
                items.push_back ({ names[i], ModMatrix::makeTemplate (i), {}, 0, 0, 0, {}, {} });

            kept (ModMatrix::galleryFolder(), ModMatrix::fileExtension, "matrix", [this] (const juce::var& json, const juce::File& file)
            {
                items.push_back ({ json.getProperty ("name", file.getFileNameWithoutExtension()).toString(), json.getProperty ("matrix", {}), {}, 0, 0, 0, file, {} });
            });
        }
        else if (page == Page::presets)
        {
            const auto names = PresetPanel::templateNames();

            for (int i = 0; i < names.size(); ++i)
                items.push_back ({ names[i], PresetPanel::makeTemplate (i), {}, 0, 0, 0, {}, {} });

            kept (PresetPanel::galleryFolder(), PresetPanel::fileExtension, "presets", [this] (const juce::var& json, const juce::File& file)
            {
                items.push_back ({ json.getProperty ("name", file.getFileNameWithoutExtension()).toString(), json.getProperty ("presets", {}), {}, 0, 0, 0, file, {} });
            });
        }
        else if (page == Page::materialLooks)
        {
            for (const auto& look : MaterialArchetypes::all())
                items.push_back ({ look.name, juce::var (juce::String (look.name)), {}, 0, 0, 0, {}, {} });
        }
        else if (page == Page::layouts)
        {
            for (int i = 0; i < (int) PanelLayouts::all().size(); ++i)
                items.push_back ({ PanelLayouts::all()[(size_t) i].name, PanelLayouts::layout (i), {}, 0, 0, 0, {}, {} });
        }
        else if (page == Page::bodies)
        {
            const auto names = BodyEditor::templateNames();

            for (int i = 0; i < names.size(); ++i)
                items.push_back ({ names[i], BodyEditor::makeTemplate (i, W, H), {}, 0, W, H, {}, {} });

            kept (BodyEditor::galleryFolder(), ".fkbody", "body", [this, W, H] (const juce::var& json, const juce::File& file)
            {
                items.push_back ({ file.getFileNameWithoutExtension(), json.getProperty ("body", {}), {}, 0,
                                   juce::jmax (8, (int) json.getProperty ("width", W)), juce::jmax (8, (int) json.getProperty ("height", H)), file, {} });
            });
        }

        // The pictures, one after another, off the message thread (3D ones take a while).
        const auto angle = layout.lightAngle, altitude = layout.lightAltitude;

        for (size_t i = 0; i < items.size(); ++i)
        {
            painter.addJob ([safe = juce::Component::SafePointer<Designs> (this), i, gen = generation, which = page, item = items[i],
                             design = items[i].design.clone(), W, H, angle, altitude]
            {
                juce::Image picture;
                const int w = cardWidth - 16, h = cardHeight - 36;

                if (which == Page::bodies)
                {
                    const auto body = BodyEditor::scaledBody (design, item.width, item.height, W, H);

                    if (const auto shape = Body3D::fromVar (body, W, H))
                    {
                        const Render3D::ScopedDraft quick;
                        picture = Render3D::renderBody (*shape, juce::jmin (1.0f, (float) w / (float) W), angle, altitude, 0.0f);
                    }
                }
                else
                {
                    picture = juce::Image (juce::Image::ARGB, w, h, true);
                    juce::Graphics g (picture);
                    const juce::Rectangle<float> area ((float) w, (float) h);

                    if (which == Page::materialLooks)      paintLookCard (g, design.toString(), area);
                    else if (which == Page::layouts)       paintLayoutCard (g, design, area);
                    else if (which == Page::selectorSets)  SelectorStudio::paintGalleryPicture (g, design, item.keys, area, angle, altitude);
                    else if (which == Page::perform)  PerformStudio::paintGalleryPicture (g, design, area, angle, altitude);
                    else if (which == Page::matrix)   MatrixStudio::paintGalleryPicture (g, design, area);
                    else                              PresetStudio::paintGalleryPicture (g, design, area, angle, altitude);
                }

                juce::MessageManager::callAsync ([safe, i, gen, picture]
                {
                    if (safe != nullptr && safe->generation == gen && i < safe->items.size())
                    {
                        safe->items[i].picture = picture;
                        safe->grid.repaint (safe->cardBounds ((int) i));
                    }
                });
            });
        }

        layOut();
    }

    void use (int index, bool asNew)
    {
        if (! juce::isPositiveAndBelow (index, (int) items.size()))
            return;

        const auto& item = items[(size_t) index];

        if (page == Page::bodies)
            canvas.useGalleryBody (item.design, item.width, item.height);
        else if (page == Page::layouts)
            canvas.startFromLayout (item.name, {}, false, false, false);
        else if (page == Page::materialLooks)
            canvas.applyMaterialLook (item.name, asNew ? -1 : canvas.selectedElement());
        else
            canvas.useGalleryDesign (kind(), item.design, item.settings, item.keys, asNew);
    }

    void showMenu (int index)
    {
        const auto& item = items[(size_t) index];
        juce::PopupMenu menu;

        if (page == Page::bodies)
        {
            menu.addItem (1, "Use it as the window's body");
        }
        else if (page == Page::layouts)
        {
            menu.addItem (1, juce::String::fromUTF8 ("Start from this layout (Fanan AI)\xe2\x80\xa6"));
        }
        else if (page == Page::materialLooks)
        {
            const auto selected = canvas.selectedElement();
            const auto& widgets = canvas.getLayout().widgets;
            const bool onShape = juce::isPositiveAndBelow (selected, (int) widgets.size()) && widgets[(size_t) selected].type == GuiWidget::Type::shape
                              && widgets[(size_t) selected].shapeKind() != "line";

            menu.addItem (1, "Put it on the selected panel", onShape);
            menu.addItem (4, "Put it on every panel (the window's background, light and text colours too)");
            menu.addItem (5, "With its 3D controls too");
            menu.addItem (6, "With its 3D controls, displays and fonts too");
            menu.addItem (7, "The whole theme: with its controls, displays, fonts and painted panel", canvas.onPaintPicture != nullptr);
        }
        else
        {
            menu.addItem (1, "Use it on the selected element");
            menu.addItem (4, "Add it as a new element");
        }

        menu.addItem (2, "Show the file", item.file != juce::File());
        menu.addItem (3, "Take it out of the gallery", item.file != juce::File());
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&grid).withMousePosition(),
                            [safe = juce::Component::SafePointer<Designs> (this), index] (int result)
                            {
                                if (safe == nullptr || ! juce::isPositiveAndBelow (index, (int) safe->items.size()))
                                    return;

                                const auto name = safe->items[(size_t) index].name;

                                if (result == 1)       safe->use (index, false);
                                else if (result == 4)  safe->use (index, true);
                                else if (result >= 5 && result <= 7 && safe->page == Page::materialLooks)
                                    safe->canvas.applyMaterialLook (name, -1, true, result >= 6, result == 7);
                                else if (result == 2)  safe->items[(size_t) index].file.revealToUser();
                                else if (result == 3 && safe->items[(size_t) index].file.deleteFile())
                                    safe->reload();
                            });
    }

    friend struct Grid;
};

//==============================================================================
GalleryHub::GalleryHub (PluginCanvas& c, Page first)
    : canvas (c)
{
    for (int i = 0; i < pageNames.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (pageNames[i]));
        b->setClickingTogglesState (false);
        b->setRadioGroupId (0x6a11);
        b->setTooltip (pageTips[i]);
        b->setColour (juce::TextButton::buttonOnColourId, Theme::accentDeep);
        b->onClick = [this, i] { showPage ((Page) i); };
        addAndMakeVisible (b);
    }

    // As wide as the Knob Gallery, and its buttons beside it.
    setSize (buttonsWidth + 1160, 680);
    showPage (first);
}

GalleryHub::~GalleryHub() = default;

void GalleryHub::show (PluginCanvas& canvas, Page page, juce::Component* near)
{
    auto& window = openWindow();

    if (window != nullptr)
    {
        if (auto* hub = dynamic_cast<GalleryHub*> (window->getContentComponent()))
            hub->showPage (page);

        window->toFront (true);
        return;
    }

    window = new HubWindow (std::make_unique<GalleryHub> (canvas, page), near);
}

void GalleryHub::showPage (Page newPage)
{
    page = newPage;

    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (i == (int) page, juce::dontSendNotification);

    content.reset();
    auto& c = canvas;

    switch (page)
    {
        case Page::knobs:
        case Page::models3d:
            content = KnobStudio::makeGallery ([&c] (const juce::File& file) { c.addGalleryItem (file); }, page == Page::models3d ? 1 : 0);
            break;

        case Page::shapes:
            content = KnobStudio::makeGallery ([&c] (const juce::File& file) { c.addGalleryShape (file); }, 2);
            break;

        case Page::icons:
            content = std::make_unique<IconBrowser> (nullptr, true);   // dragged onto the window
            break;

        case Page::fxIcons:
            content = std::make_unique<IconBrowser> (nullptr, true, true);   // on their badges
            break;

        case Page::selectorSets:
        case Page::perform:
        case Page::matrix:
        case Page::presets:
        case Page::bodies:
        case Page::materialLooks:
        case Page::layouts:
            content = std::make_unique<Designs> (canvas, page);
            break;
    }

    addAndMakeVisible (*content);
    resized();
    repaint();
}

void GalleryHub::paint (juce::Graphics& g)
{
    g.fillAll (Theme::window);
    g.setColour (Theme::outline);
    g.fillRect (buttonsWidth - 1, 0, 1, getHeight());

    g.setColour (Theme::muted);
    g.setFont (Theme::font (11.5f, true));
    g.drawText ("GALLERIES", juce::Rectangle<int> (14, 8, buttonsWidth - 20, 22), juce::Justification::centredLeft, false);
}

void GalleryHub::resized()
{
    auto r = getLocalBounds();
    auto left = r.removeFromLeft (buttonsWidth).reduced (8, 0);
    left.removeFromTop (34);

    for (auto* b : buttons)
    {
        b->setBounds (left.removeFromTop (34));
        left.removeFromTop (4);
    }

    if (content != nullptr)
        content->setBounds (r);
}
