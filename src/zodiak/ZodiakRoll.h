// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakRoll.h
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>
#include "PluginProcessor.h"
#include "ZodiakWidgets.h"
#include "ZodiakPattern.h"

//==============================================================================
//  ZodiakRoll.h  —  ROLL: a channel's pattern, drawn and edited (Sonify's
//  piano roll, hub-editor.js, is the reference).
//
//      TOOLBAR   the pattern (banks A-D, variations 1-8: the same choice as on
//                the banner), BARS 1 / 2 / 4, GRID, TIME SIG (SYNC DAW, or its
//                own), SWING (50% straight .. 75%), IMPORT MIDI / EXPORT MIDI
//                (a standard MIDI file into the pattern / the pattern as one),
//                CLEAR
//      RULER     the LOCATORS (the stretch that loops: drag their ends, or the
//                bar between them, on the 1/32 grid; double-click: the whole
//                pattern), bars and beats, and where the pattern plays
//      NOTES     a piano roll (128 keys, scrolled), on every channel
//      VELOCITY  one bar per note; 115 and over is an ACCENT.
//                With a note selected, its row's bars come to the front and only
//                they are edited (the row is marked red across the notes)
//
//  The look: BRIGHT (the default) or DARK, chosen in the Roll's title bar.
//
//  Mouse:  double-click an empty place: a note.  Click a note: it's selected
//          (Shift or Ctrl: added or taken away); drag on an empty place: the
//          notes in the box are selected.  Drag a selected note: all of them
//          move, any direction; drag its right edge (the pointer turns to a
//          left-right arrow there): they grow or shrink.
//          Right-click: Cut, Copy, Paste (where you clicked), Duplicate,
//          Delete, Select All.
//  Keys:   Delete, Ctrl+A / C / X / V / D, Esc (deselect), Up / Down (a key;
//          Shift: an octave), Left / Right (a grid step).
//
//  Every change goes to the sequencer at once, so a playing pattern plays it.
//==============================================================================

namespace zodiak::ui
{
    class RollEditor final : public juce::Component,
                             private juce::Timer
    {
    public:
        RollEditor (ZodiakProcessor& p, int channel);
        ~RollEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        bool keyPressed (const juce::KeyPress&) override;

        /** The pattern it shows changed (another one chosen): its name, for the title. */
        std::function<void (const juce::String&)> onPatternChanged;

        /** BRIGHT or DARK (kept by the processor for every Zodiak). */
        void setDark (bool dark);
        bool isDark() const noexcept        { return dark; }

        class Ruler;
        class NoteGrid;
        class VelocityLane;

    private:
        // What the parts share
        struct Geometry
        {
            int keyW = 150;                 // the keys' (or the drum names') column
            int width = 0;                  // the whole width, keys included
            int lengthTicks = 96 * 4;
            int barTicks = 96 * 4, beatTicks = 96, gridTicks = 24;

            float xOf (int tick) const noexcept     { return (float) keyW + (float) tick * (float) (width - keyW) / (float) juce::jmax (1, lengthTicks); }
            int   tickAt (float x) const noexcept   { return juce::roundToInt ((x - (float) keyW) * (float) lengthTicks / (float) juce::jmax (1, width - keyW)); }
            int   snapDown (int t) const noexcept   { return juce::jlimit (0, juce::jmax (0, lengthTicks - gridTicks), (t / gridTicks) * gridTicks); }
        };

        struct Theme
        {
            juce::Colour white, black, rowLine, octave, bar, beat, fine, outside;
            juce::Colour keyWhite, keyBlack, keyText, ruler, rulerText, lane, laneLine, mark, labelBg, labelText;
        };

        void timerCallback() override;
        void load();                        // the chosen pattern, from the sequencer
        void commit();                      // the pattern, to the sequencer
        void updateGeometry();
        void refreshToolbar();
        void repaintAll();
        const Theme& theme() const noexcept;

        // The notes and the selection
        bool isSelected (int i) const noexcept      { return juce::isPositiveAndBelow (i, (int) sel.size()) && sel[(size_t) i] != 0; }
        int  numSelected() const noexcept;
        void selectOnly (int i);
        void selectNone();
        void selectAll();
        void addNotes (const std::vector<seq::Note>& notes, bool selectThem);
        void deleteSelected();
        void copySelected();
        void paste (int atTick);
        void duplicateSelected();
        void nudgeSelected (int ticks, int keys);
        void showNoteMenu (int tick);
        int  minKey() const noexcept                { return drums ? 35 : 0; }
        int  maxKey() const noexcept                { return drums ? 51 : 127; }

        void exportMidi();
        void importMidi();
        void audition (int key, int velocity);
        void auditionOff();

        ZodiakProcessor& proc;
        const int ch;
        bool drums;                         // always false now: every channel is a piano roll (kept for the code that asks)
        void updateRollKind();              // a VSTi came onto the drum channel, or went
        const ChannelColours& chColours;
        bool dark = false;

        seq::Pattern pat;
        std::vector<uint8_t> sel;           // a flag for each note: selected
        int  index = -1, shownGeneration = -1;
        int  focusKey = -1;                 // the row whose velocities are in front (-1: none)
        int  lastVelocity = 100;
        int  shownTick = -1;
        int  auditioning = -1;
        int  pasteTick = 0;
        Geometry geo;

        // Toolbar
        juce::OwnedArray<juce::TextButton> bankBtns, varBtns, barBtns;
        juce::ComboBox gridBox, numBox, denBox, swingBox;
        juce::ToggleButton syncBtn { "SYNC DAW" };
        juce::TextButton clearBtn { "CLEAR" }, exportBtn { "EXPORT MIDI" }, importBtn { "IMPORT MIDI" };
        std::unique_ptr<juce::FileChooser> chooser;
        juce::Rectangle<int> toolbar;

        std::unique_ptr<Ruler> ruler;
        std::unique_ptr<NoteGrid> grid;
        juce::Viewport view;
        std::unique_ptr<VelocityLane> lane;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RollEditor)
    };
}
