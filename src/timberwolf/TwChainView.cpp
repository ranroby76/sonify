// D:\workspace\Fanan_web_hub\src\timberwolf\TwChainView.cpp
#include "TwChainView.h"
#include "PluginProcessor.h"

namespace tw
{
    ChainView::ChainView (TimberWolfProcessor& p) : proc (p)
    {
        setRepaintsOnMouseActivity (false);
        refresh();
    }

    void ChainView::refresh()
    {
        layout();
        repaint();
    }

    void ChainView::setSelected (int uid)
    {
        selected = uid;
        repaint();
    }

    void ChainView::resized()   { layout(); }

    //==========================================================================
    juce::Point<float> ChainView::cellCentre (int col, int row) const
    {
        return { originX + ((float) col + 0.5f) * cellW, originY + (float) row * cellH + cellH * 0.42f };
    }

    int ChainView::place (const std::vector<Node>& chain, const Path& path, int col, int row, int& rows,
                          std::vector<std::pair<int, int>>& ends)
    {
        juce::ignoreUnused (ends);
        rows = 1;
        int c = col;

        const auto addGap = [&] (int index, int atCol)
        {
            Item gap;
            gap.kind = Item::Kind::gap;
            gap.chain = path;
            gap.index = index;
            gap.col = atCol;
            gap.row = row;
            const juce::Point<float> at (originX + (float) atCol * cellW, cellCentre (0, row).y);
            gap.box = juce::Rectangle<float> (18.0f, 18.0f).withCentre (at);
            items.push_back (gap);
        };

        if (chain.empty())
        {
            // An empty path: one column, its gap in the middle of it.
            Item gap;
            gap.kind = Item::Kind::gap;
            gap.chain = path;
            gap.index = 0;
            gap.col = col;
            gap.row = row;
            gap.box = juce::Rectangle<float> (18.0f, 18.0f).withCentre (cellCentre (col, row));
            items.push_back (gap);
            return 1;
        }

        addGap (0, c);

        for (int i = 0; i < (int) chain.size(); ++i)
        {
            const auto& n = chain[(size_t) i];

            if (n.kind == Node::Kind::block)
            {
                Item it;
                it.kind = Item::Kind::block;
                it.uid = n.uid;
                it.col = c;
                it.row = row;
                const float s = juce::jmin (cellW, cellH) * 0.50f;
                const bool wide = proc.infoOf (n.uid) != nullptr && proc.infoOf (n.uid)->family == Family::ampcab;
                it.box = juce::Rectangle<float> (wide ? s * 1.45f : s, s).withCentre (cellCentre (c, row));
                items.push_back (it);
                ++c;
            }
            else
            {
                const int splitCol = c;
                const float s = juce::jmin (cellW, cellH) * 0.30f;

                Item split;
                split.kind = Item::Kind::split;
                split.uid = n.uid;
                split.col = splitCol;
                split.row = row;
                split.box = juce::Rectangle<float> (s, s).withCentre (cellCentre (splitCol, row));
                items.push_back (split);

                auto pathA = path;  pathA.push_back ({ i, false });
                auto pathB = path;  pathB.push_back ({ i, true });

                int ra = 1, rb = 1;
                const int wa = place (n.a, pathA, splitCol + 1, row, ra, ends);
                const int wb = place (n.b, pathB, splitCol + 1, row + ra, rb, ends);
                const int inner = juce::jmax (1, wa, wb);
                const int blendCol = splitCol + 1 + inner;

                Item blend;
                blend.kind = Item::Kind::blend;
                blend.uid = n.uid;
                blend.col = blendCol;
                blend.row = row;
                blend.box = juce::Rectangle<float> (s, s).withCentre (cellCentre (blendCol, row));
                items.push_back (blend);

                // The wires: the split's fan out, each path's rail to the blend, and back in.
                const float yA = cellCentre (0, row).y, yB = cellCentre (0, row + ra).y;
                const float xIn = originX + (float) (splitCol + 1) * cellW, xOut = originX + (float) blendCol * cellW;
                const float xs = cellCentre (splitCol, row).x, xb = cellCentre (blendCol, row).x;

                juce::Path w;
                w.startNewSubPath (xs, yA);
                w.lineTo (xIn, yA);
                w.lineTo (xOut, yA);
                w.lineTo (xb, yA);
                wires.push_back (w);

                juce::Path wB;
                wB.startNewSubPath (xs, yA);
                wB.quadraticTo (xIn - cellW * 0.15f, yA, xIn - cellW * 0.15f, (yA + yB) * 0.5f);
                wB.quadraticTo (xIn - cellW * 0.15f, yB, xIn, yB);
                wB.lineTo (xOut, yB);
                wB.quadraticTo (xOut + cellW * 0.15f, yB, xOut + cellW * 0.15f, (yA + yB) * 0.5f);
                wB.quadraticTo (xOut + cellW * 0.15f, yA, xb, yA);
                wires.push_back (wB);

                rows = juce::jmax (rows, ra + rb);
                c = blendCol + 1;
            }

            addGap (i + 1, c);
        }

        return c - col;
    }

    void ChainView::layout()
    {
        const auto& g = proc.graph();

        // A first pass to measure (columns and rows), then the real one in the fitted cells.
        for (int pass = 0; pass < 2; ++pass)
        {
            items.clear();
            wires.clear();

            std::vector<std::pair<int, int>> ends;
            int rows = 1;
            const int width = place (g.main, {}, 1, 0, rows, ends);

            if (pass == 0)
            {
                cols = width + 2;
                rowsUsed = rows;
                const auto area = getLocalBounds().toFloat().reduced (10.0f, 8.0f);
                cellW = juce::jlimit (34.0f, 104.0f, area.getWidth() / (float) cols);
                cellH = juce::jlimit (46.0f, 112.0f, area.getHeight() / (float) rows);
                originX = area.getX() + (area.getWidth() - cellW * (float) cols) * 0.5f;
                originY = area.getY() + juce::jmax (0.0f, (area.getHeight() - cellH * (float) rows) * 0.5f);
                continue;
            }

            Item in;
            in.kind = Item::Kind::input;
            in.box = juce::Rectangle<float> (26.0f, 26.0f).withCentre (cellCentre (0, 0));
            items.push_back (in);

            Item out;
            out.kind = Item::Kind::output;
            out.col = width + 1;
            out.box = juce::Rectangle<float> (26.0f, 26.0f).withCentre (cellCentre (width + 1, 0));
            items.push_back (out);

            juce::Path main;
            main.startNewSubPath (cellCentre (0, 0));
            main.lineTo (cellCentre (width + 1, 0));
            wires.insert (wires.begin(), main);
        }
    }

    //==========================================================================
    void ChainView::paint (juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();

        // The screen: a faint grid in its glass.
        g.setColour (juce::Colour (0xff8ea3c4).withAlpha (0.04f));
        for (float x = bounds.getX(); x < bounds.getRight(); x += 24.0f) g.drawVerticalLine ((int) x, bounds.getY(), bounds.getBottom());
        for (float y = bounds.getY(); y < bounds.getBottom(); y += 24.0f) g.drawHorizontalLine ((int) y, bounds.getX(), bounds.getRight());

        // The wires: a soft glow under a thin bright line.
        for (const auto& w : wires)
        {
            g.setColour (look::screenLine.withAlpha (0.12f));
            g.strokePath (w, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (look::screenLine.withAlpha (0.75f));
            g.strokePath (w, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        for (int i = 0; i < (int) items.size(); ++i)
        {
            const auto& it = items[(size_t) i];

            switch (it.kind)
            {
                case Item::Kind::input:
                case Item::Kind::output:
                {
                    g.setColour (juce::Colour (0xff10151e));
                    g.fillEllipse (it.box);
                    g.setColour (look::screenLine);
                    g.drawEllipse (it.box, 1.4f);
                    g.setFont (look::font (10.0f, true));
                    g.drawText (it.kind == Item::Kind::input ? "IN" : "OUT",
                                juce::Rectangle<float> (40.0f, 14.0f).withCentre ({ it.box.getCentreX(), it.box.getBottom() + 10.0f }).toNearestInt(),
                                juce::Justification::centred);
                    look::drawIcon (g, "gfx-jack", it.box.reduced (5.0f), look::screenLine);
                    break;
                }

                case Item::Kind::gap:
                {
                    const bool show = i == hoverGap || (dragging && i == dropGap);

                    if (dragging && i == dropGap)
                    {
                        g.setColour (look::accent.withAlpha (0.35f));
                        g.fillRoundedRectangle (it.box.expanded (6.0f), 6.0f);
                    }

                    if (show || ! dragging)
                    {
                        g.setColour (show ? look::accent : look::screenLine.withAlpha (0.0f));
                        if (show)
                        {
                            g.setColour (juce::Colour (0xff0d1420));
                            g.fillEllipse (it.box);
                            g.setColour (look::accent);
                            g.drawEllipse (it.box, 1.5f);
                            const auto c = it.box.getCentre();
                            g.drawLine (c.x - 4.5f, c.y, c.x + 4.5f, c.y, 1.6f);
                            g.drawLine (c.x, c.y - 4.5f, c.x, c.y + 4.5f, 1.6f);
                        }
                    }

                    break;
                }

                case Item::Kind::split:
                case Item::Kind::blend:
                {
                    const auto& info = it.kind == Item::Kind::split ? registry::splitInfo() : registry::blendInfo();
                    look::drawBadge (g, Family::utility, info.icon, it.box, true, it.uid == selected, 0.4f);
                    break;
                }

                case Item::Kind::block:
                default:
                {
                    if (dragging && it.uid == dragUid)
                    {
                        g.setColour (look::screenLine.withAlpha (0.25f));
                        g.drawEllipse (it.box.reduced (2.0f), 1.0f);
                        break;
                    }

                    const auto* info = proc.infoOf (it.uid);

                    if (info == nullptr)
                        break;

                    const bool on = proc.isOn (it.uid);
                    look::drawBadge (g, info->family, info->icon, it.box, on, it.uid == selected, on ? 1.0f : 0.0f);

                    // Its name under it.
                    auto label = juce::Rectangle<float> (cellW * 1.05f, 14.0f).withCentre ({ it.box.getCentreX(), it.box.getBottom() + 12.0f });
                    g.setColour (on ? look::ink.withAlpha (it.uid == hoverUid ? 1.0f : 0.85f) : look::inkFaint);
                    g.setFont (look::font (cellW < 60.0f ? 9.5f : 11.0f, it.uid == selected));
                    g.drawFittedText (info->name, label.toNearestInt(), juce::Justification::centred, 1, 0.8f);
                    break;
                }
            }
        }

        // The one being dragged, under the mouse.
        if (dragging && dragUid != 0)
        {
            const auto* info = proc.infoOf (dragUid);
            const float s = juce::jmin (cellW, cellH) * 0.5f;
            const auto box = juce::Rectangle<float> (s, s).withCentre (dragAt);

            if (info != nullptr)
            {
                look::drawBadge (g, info->family, info->icon, box, true, true, 0.8f);
            }
            else
            {
                look::drawBadge (g, Family::utility, registry::splitInfo().icon, box.reduced (s * 0.2f), true, true, 0.5f);
            }
        }

        if (proc.graph().main.empty())
        {
            g.setColour (look::inkDim);
            g.setFont (look::font (14.0f));
            g.drawText ("Click + to add an effect", getLocalBounds().withTrimmedTop (getHeight() / 2 + 24), juce::Justification::centredTop);
        }
    }

    //==========================================================================
    const ChainView::Item* ChainView::itemAt (juce::Point<float> p, bool gapsToo) const
    {
        for (const auto& it : items)
        {
            if (it.kind == Item::Kind::gap && ! gapsToo)
                continue;

            if (it.kind == Item::Kind::input || it.kind == Item::Kind::output)
                continue;

            if (it.box.expanded (it.kind == Item::Kind::gap ? 6.0f : 4.0f).contains (p))
                return &it;
        }

        return nullptr;
    }

    const ChainView::Item* ChainView::gapNear (juce::Point<float> p) const
    {
        const Item* best = nullptr;
        float bestD = juce::jmax (cellW, cellH) * 0.75f;

        for (const auto& it : items)
        {
            if (it.kind != Item::Kind::gap)
                continue;

            const float d = it.box.getCentre().getDistanceFrom (p);

            if (d < bestD)
            {
                bestD = d;
                best = &it;
            }
        }

        return best;
    }

    void ChainView::mouseMove (const juce::MouseEvent& e)
    {
        const auto* it = itemAt (e.position, true);
        const int uid = it != nullptr && it->kind != Item::Kind::gap ? it->uid : 0;
        const auto* gap = it == nullptr ? gapNear (e.position) : (it->kind == Item::Kind::gap ? it : nullptr);
        const int g = gap != nullptr && gap->box.getCentre().getDistanceFrom (e.position) < 22.0f ? (int) (gap - items.data()) : -1;

        if (uid != hoverUid || g != hoverGap)
        {
            hoverUid = uid;
            hoverGap = g;
            setMouseCursor (g >= 0 || uid != 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }

        tip = {};

        if (uid != 0)
            if (const auto* info = proc.infoOf (uid))
                tip = info->name + ": " + info->description + "  (double-click to edit, drag to move, right-click for more)";
    }

    void ChainView::mouseExit (const juce::MouseEvent&)
    {
        hoverUid = 0;
        hoverGap = -1;
        repaint();
    }

    void ChainView::mouseDown (const juce::MouseEvent& e)
    {
        const auto* it = itemAt (e.position, true);

        if (e.mods.isPopupMenu())
        {
            if (it != nullptr && it->kind != Item::Kind::gap)
                showMenu (*it);
            return;
        }

        if (it == nullptr && hoverGap >= 0)
            it = &items[(size_t) hoverGap];

        if (it != nullptr && it->kind == Item::Kind::gap)
        {
            showAddMenu (*it);
            return;
        }

        dragUid = it != nullptr ? it->uid : 0;
        dragging = false;
    }

    void ChainView::mouseDrag (const juce::MouseEvent& e)
    {
        if (dragUid == 0 || e.mods.isPopupMenu())
            return;

        if (! dragging && e.getDistanceFromDragStart() < 5)
            return;

        dragging = true;
        dragAt = e.position;
        const auto* gap = gapNear (e.position);
        dropGap = gap != nullptr ? (int) (gap - items.data()) : -1;
        repaint();
    }

    void ChainView::mouseUp (const juce::MouseEvent&)
    {
        if (dragging && dragUid != 0 && dropGap >= 0)
        {
            const auto gap = items[(size_t) dropGap];
            const int uid = dragUid;
            dragging = false;
            dragUid = 0;
            dropGap = -1;
            proc.moveNode (uid, gap.chain, gap.index);   // rebuilds, then refresh() through the processor
            return;
        }

        dragging = false;
        dragUid = 0;
        dropGap = -1;
        repaint();
    }

    void ChainView::mouseDoubleClick (const juce::MouseEvent& e)
    {
        if (const auto* it = itemAt (e.position, false))
        {
            if (it->kind == Item::Kind::block || it->kind == Item::Kind::split || it->kind == Item::Kind::blend)
            {
                selected = it->uid;
                repaint();

                if (onOpen)
                    onOpen (it->uid);
            }
        }
    }

    //==========================================================================
    void ChainView::addEffectsTo (juce::PopupMenu& menu, std::function<void (const juce::String&)> chosen, const juce::String& current)
    {
        for (int f = 0; f < (int) Family::count; ++f)
        {
            juce::PopupMenu sub;

            for (const auto& info : registry::all())
            {
                if (info.hidden || (int) info.family != f)
                    continue;

                const auto id = info.id;
                sub.addItem (info.name, true, id == current, [chosen, id] { chosen (id); });
            }

            if (sub.getNumItems() > 0)
                menu.addSubMenu (familyName ((Family) f), sub);
        }
    }

    void ChainView::showAddMenu (const Item& gap)
    {
        juce::PopupMenu menu;
        const auto path = gap.chain;
        const int index = gap.index;
        auto* self = this;

        if (proc.graph().isFull())
        {
            menu.addItem ("The chain is full (16 effects)", false, false, [] {});
        }
        else
        {
            menu.addSectionHeader ("Add an effect");
            addEffectsTo (menu, [self, path, index] (const juce::String& id)
            {
                const int uid = self->proc.addBlock (id, path, index);

                if (uid != 0 && self->onOpen)
                {
                    self->selected = uid;
                    self->onOpen (uid);
                }
            });
        }

        menu.addSeparator();
        menu.addItem ("Add a split (two paths) here", proc.graph().numParallels() < maxBlocks - 1, false,
                      [self, path, index] { self->proc.addParallel (path, index); });

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                                .withTargetScreenArea (localAreaToGlobal (gap.box.toNearestInt())));
    }

    void ChainView::showMenu (const Item& it)
    {
        juce::PopupMenu menu;
        const int uid = it.uid;
        auto* self = this;

        if (it.kind == Item::Kind::block)
        {
            const auto* info = proc.infoOf (uid);
            const bool on = proc.isOn (uid);

            menu.addSectionHeader (info != nullptr ? info->name : juce::String ("Effect"));
            menu.addItem ("Edit", [self, uid] { self->selected = uid; if (self->onOpen) self->onOpen (uid); });
            menu.addItem (on ? "Switch off (bypass)" : "Switch on", [self, uid, on] { self->proc.setOn (uid, ! on); self->repaint(); });

            juce::PopupMenu replace;
            addEffectsTo (replace, [self, uid] (const juce::String& id) { self->proc.replaceBlock (uid, id); if (self->onOpen) self->onOpen (uid); },
                          info != nullptr ? info->id : juce::String());
            menu.addSubMenu ("Replace with", replace);

            menu.addItem ("Split round it (it becomes path A)", proc.graph().numParallels() < maxBlocks - 1, false,
                          [self, uid] { self->proc.splitAround (uid); });
            menu.addSeparator();
            menu.addItem ("Remove", [self, uid] { if (self->selected == uid) self->selected = 0; self->proc.removeNode (uid); });
        }
        else
        {
            const auto* node = proc.graph().node (uid);

            if (node == nullptr)
                return;

            const auto settings = node->settings;
            menu.addSectionHeader ("Split / Blend");
            menu.addItem ("Edit", [self, uid] { self->selected = uid; if (self->onOpen) self->onOpen (uid); });
            menu.addItem ("Y split (both paths get all of it)", true, settings.splitType == 0,
                          [self, uid, settings] { auto s = settings; s.splitType = 0; self->proc.setParallel (uid, s); });
            menu.addItem ("Crossover (A the lows, B the highs)", true, settings.splitType == 1,
                          [self, uid, settings] { auto s = settings; s.splitType = 1; self->proc.setParallel (uid, s); });
            menu.addItem ("Swap paths A and B", [self, uid] { self->proc.swapPaths (uid); });
            menu.addSeparator();
            menu.addItem ("Unsplit (keep the effects, one after another)", [self, uid] { if (self->selected == uid) self->selected = 0; self->proc.unsplit (uid); });
            menu.addItem ("Remove the split and its effects", [self, uid] { if (self->selected == uid) self->selected = 0; self->proc.removeNode (uid); });
        }

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                                .withTargetScreenArea (localAreaToGlobal (it.box.toNearestInt())));
    }
}
