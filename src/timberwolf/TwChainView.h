// D:\workspace\Fanan_web_hub\src\timberwolf\TwChainView.h
#pragma once
//==============================================================================
//  TwChainView.h  -  the chain graph on Timber Wolf's screen.
//
//  The signal runs left to right, from IN to OUT.  Every effect is its
//  family's badge with its icon, its name under it; a split fans out into its
//  two paths (A above, B under it) and meets again at its blend.  Between any
//  two places, and at the ends of every path, is a gap where an effect can go.
//
//      double-click        an effect (or a split / blend) opens on the panel below
//      drag                moves it (a split moves with everything in it): dropped
//                          on a gap, it goes there
//      right-click         its menu: on / off, replace, split round it, remove;
//                          a split's: swap paths, Y or crossover, unsplit, remove
//      click a gap's +     add an effect there (or a new split)
//
//  Message thread only.
//==============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include "TwGraph.h"
#include "TwLook.h"

class TimberWolfProcessor;

namespace tw
{
    class ChainView final : public juce::Component,
                            public juce::TooltipClient
    {
    public:
        explicit ChainView (TimberWolfProcessor&);

        /** The graph changed: laid out again. */
        void refresh();

        /** What the panel shows (an effect's or a parallel's uid; 0: none), highlighted here. */
        void setSelected (int uid);
        int getSelected() const noexcept                    { return selected; }

        std::function<void (int uid)> onOpen;               // double-click: show it on the panel

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        juce::String getTooltip() override                  { return tip; }

    private:
        struct Item
        {
            enum class Kind { block, split, blend, gap, input, output };

            Kind kind = Kind::block;
            int uid = 0;                    // block / split / blend: its node's uid
            Path chain;                     // gap: the chain it's in
            int index = 0;                  // gap: where in it
            juce::Rectangle<float> box;     // where it's drawn (a gap: where a drop is taken)
            int col = 0, row = 0;
        };

        struct Wire
        {
            juce::Path path;
        };

        void layout();
        int place (const std::vector<Node>& chain, const Path& path, int col, int row, int& rows,
                   std::vector<std::pair<int, int>>& ends);
        juce::Point<float> cellCentre (int col, int row) const;

        const Item* itemAt (juce::Point<float>, bool gapsToo) const;
        const Item* gapNear (juce::Point<float>) const;
        void showMenu (const Item&);
        void showAddMenu (const Item& gap);
        void addEffectsTo (juce::PopupMenu& menu, std::function<void (const juce::String&)> chosen, const juce::String& current = {});

        TimberWolfProcessor& proc;
        std::vector<Item> items;
        std::vector<juce::Path> wires;
        int cols = 1, rowsUsed = 1;
        float cellW = 80.0f, cellH = 80.0f, originX = 0.0f, originY = 0.0f;

        int selected = 0;
        juce::String tip;
        int hoverUid = 0;
        int hoverGap = -1;

        // A drag in progress.
        int dragUid = 0;
        bool dragging = false;
        juce::Point<float> dragAt;
        int dropGap = -1;
    };
}
