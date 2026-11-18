// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakLinkUi.h
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "ZodiakLink.h"

class ZodiakProcessor;

//==============================================================================
//  ZodiakLinkUi.h  —  MIDI LINK's windows (ZodiakLink.h has how it works):
//
//      LinkCanvas      the MIDI LINK window on the mode bar: the last source
//                      moved, to drag onto a control (or tap to arm, then click
//                      a control).  Right-click: the links, save / load a map,
//                      clear them all.
//      linkTarget      makes a control one a source can be dropped on: an
//                      invisible cover over it, there only while a source is
//                      dragged or armed, so the control works as before at any
//                      other time.  A linked control shows its source in a small
//                      badge; a right double-click opens its link's window.
//      showLinkPopup   a link's small window: KNOB / TOGGLE / PUSH, CLEAR, CANCEL
//                      (CANCEL after a drop puts back what was there before it).
//
//  Message thread only.
//==============================================================================

namespace zodiak::ui
{
    //==========================================================================
    class LinkCanvas final : public juce::Component,
                             public juce::DragAndDropContainer,
                             public juce::SettableTooltipClient,
                             private juce::Timer
    {
    public:
        explicit LinkCanvas (ZodiakProcessor& p);
        ~LinkCanvas() override;

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        void dragOperationEnded (const juce::DragAndDropTarget::SourceDetails&) override;

    private:
        void timerCallback() override;
        void showMenu();

        ZodiakProcessor& proc;
        link::Source held;
        int  lastWord = 0, shownGen = -1;
        bool dragStarted = false, armed = false;
        std::unique_ptr<juce::FileChooser> chooser;
    };

    //==========================================================================
    /** A control a source can be dropped on, linking it to this target (badge: its cover
        shows the source; off for a control that shows it itself). */
    void linkTarget (juce::Component& control, ZodiakProcessor& p, const link::Target& target, bool badge = true);

    /** ... a control holding many: the target under the drop. */
    void linkTarget (juce::Component& control, ZodiakProcessor& p, std::function<link::Target (juce::Point<int>)> targetAt);

    /** ... a control that does something of its own with the source dropped (a plugin's LINK). */
    void linkDropAction (juce::Component& control, ZodiakProcessor& p, std::function<void (const link::Source&)> onDrop);

    /** The control no longer takes a source. */
    void unlinkTarget (juce::Component& control);

    /** A link's window over a control: its mode, CLEAR, CANCEL. */
    void showLinkPopup (ZodiakProcessor& p, juce::Component& near, const link::Target& target);

    /** A source dropped on a target: linked, and its window opened. */
    void linkDropped (ZodiakProcessor& p, juce::Component& near, const link::Target& target, const link::Source& source);

    /** Every cover redrawn (the links changed, a drag began or ended). */
    void refreshLinkTargets();

    /** True while a source is being dragged or is armed. */
    bool isLinkAssigning();
}
