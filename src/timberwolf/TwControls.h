// D:\workspace\Fanan_web_hub\src\timberwolf\TwControls.h
#pragma once
//==============================================================================
//  TwControls.h  -  Timber Wolf's 3D controls.
//
//  The knobs, levers and footswitches are real 3D objects, rendered by Sonify's
//  own 3D renderer (Render3D, the 3D Studio's): a chrome cap on a black knurled
//  body with an amber light ring that shows the value, a chrome lever, a chrome
//  footswitch.  Each is rendered once per size into frames (a knob 61, a lever
//  and a footswitch 2), in the background, and kept on disk under Sonify's
//  folder (Timber Wolf\cache), so the next window opens with them at once.
//  The window's body - the unit's case - is rendered the same way.
//
//  Until a control's frames are ready it draws a flat stand-in, and repaints
//  when they arrive.
//
//  Threads: message thread, apart from the renders (a pool of their own).
//==============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include "TwLook.h"

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace tw
{
    class Frames final
    {
    public:
        enum class Kind { knob = 0, lever, footswitch, body };

        static Frames& get();

        /** A control's frames at size (window pixels) and scale (pixels per window pixel);
            nullptr while they're being made (who is repainted when they're in). */
        const std::vector<juce::Image>* find (Kind kind, int width, int height, float scale, juce::Component* who);

        /** How far a control's picture reaches past its box (its shadow), in window pixels. */
        float padOf (Kind kind, int size) const;

        /** The case's JSON (Body3D), for a window w x h. */
        static juce::var bodyJson (int w, int h);

        ~Frames();

    private:
        Frames();

        struct Job;
        juce::String keyOf (Kind, int w, int h, float scale) const;
        void done (const juce::String& key, std::vector<juce::Image> frames);
        static juce::File cacheFolder();

        std::map<juce::String, std::vector<juce::Image>> made;
        std::set<juce::String> making;
        std::map<juce::String, std::vector<juce::Component::SafePointer<juce::Component>>> waiting;
        juce::ThreadPool pool { juce::ThreadPoolOptions{}.withNumberOfThreads (2) };
    };

    //==========================================================================
    /** What a control shows and changes: the knob's travel (0..1) and text. */
    struct Binding
    {
        std::function<float()> get;                  // 0..1
        std::function<void (float)> set;             // 0..1
        std::function<void()> begin, end;            // a gesture (one drag)
        std::function<juce::String()> text;          // the value as it reads
        std::function<void()> reset;                 // double-click: its default
        juce::String name;
        int steps = 0;                               // a selector's positions (0: continuous)
    };

    /** A 3D knob: its name over it, its value under it in a little display. */
    class KnobView final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit KnobView (Binding b);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

        Binding binding;
        juce::Colour accent { look::accent };

    private:
        juce::Rectangle<int> knobArea() const;
        float startValue = 0.0f;
        bool dragging = false;
    };

    /** A chrome lever for a two-way switch (Off / On, Low / High...). */
    class LeverView final : public juce::Component
    {
    public:
        explicit LeverView (Binding b);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

        Binding binding;
    };

    /** A footswitch: chrome, its light over it, its label strip under it. */
    class FootswitchView final : public juce::Component
    {
    public:
        FootswitchView();

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;

        std::function<void()> onPress;               // stamped on
        std::function<void()> onMenu;                // right-click: its job
        std::function<void()> onLearn;               // right double-click: MIDI learn
        juce::String label, number, midi;
        bool lit = false, learning = false;
        juce::Colour light { look::ledOn };

    private:
        bool down = false;
        juce::uint32 lastRightClick = 0;
    };

    /** An expression pedal: its treadle, rocked by dragging up and down. */
    class PedalView final : public juce::Component
    {
    public:
        PedalView();

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        std::function<float()> get;
        std::function<void (float)> set;
        std::function<void()> onMenu, onLearn;
        juce::String label, target, midi;
        bool learning = false;

    private:
        juce::uint32 lastRightClick = 0;
    };
}
