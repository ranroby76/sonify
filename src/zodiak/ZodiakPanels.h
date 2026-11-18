// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakPanels.h
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <memory>
#include <vector>
#include "ZodiakWidgets.h"
#include "ZodiakChrome.h"
#include "PluginProcessor.h"
#include "DuckerEditor.h"

//==============================================================================
//  ZodiakPanels.h  —  the small windows and parts Zodiak's pages share:
//
//      SwitchButton    an effect's ON / OFF, in letters that can be read
//      InsertPanel     a master's built-in VST FX's window (its e): Wah or Phaser
//      DynEqPanel      the Dyn EQ node's settings: Grex's dynamic EQ, its two
//                      points on the curve and their dynamics, the key meters
//
//  Message thread only.
//==============================================================================

namespace zodiak::ui
{
    //==========================================================================
    //  An effect's switch: ON (lit) or OFF, in letters that can be read
    //==========================================================================
    class SwitchButton final : public juce::Button
    {
    public:
        SwitchButton() : juce::Button ("switch")     { setClickingTogglesState (true); }

        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            const auto b = getLocalBounds().toFloat().reduced (2.0f);
            const bool on = getToggleState();
            g.setColour (on ? win::led.withMultipliedBrightness (down ? 0.8f : 1.0f) : (over ? win::well.brighter (0.08f) : win::well));
            g.fillRoundedRectangle (b, 8.0f);
            g.setColour (on ? win::led.brighter (0.3f) : juce::Colour (0xFF4A4F57));
            g.drawRoundedRectangle (b.reduced (0.8f), 8.0f, 1.6f);
            g.setColour (on ? juce::Colours::black : win::label);
            g.setFont (font (26.0f, true));
            g.drawText (on ? "ON" : "OFF", getLocalBounds(), juce::Justification::centred, false);
        }
    };

    //==========================================================================
    class InsertPanel final : public juce::Component,
                              private juce::Timer
    {
    public:
        InsertPanel (inserts::BuiltIn& insert, const ChannelColours& colours);
        ~InsertPanel() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void timerCallback() override;

        inserts::BuiltIn& fx;
        std::vector<std::unique_ptr<ZodiakKnob>> knobs;
    };

    //==========================================================================
    class DynEqPanel final : public juce::Component,
                             private juce::Timer
    {
    public:
        DynEqPanel (ZBetel::StyleDucker& ducker, juce::String keyedBy);
        ~DynEqPanel() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        struct Row
        {
            juce::Label  name;
            juce::Slider slider;
        };

        struct Column
        {
            juce::ToggleButton on;
            Row freq, q, depth, thresh, ratio, attack, release;
            juce::Rectangle<float> keyArea;
        };

        void timerCallback() override;
        void refreshFromDucker();
        void setupRow (Row& r, const juce::String& name, double lo, double hi, double step, double skewMid,
                       std::function<void (float)> apply, const juce::String& suffix);
        void paintKeyMeter (juce::Graphics& g, int band);

        ZBetel::StyleDucker& ducker;
        ZBetel::DuckerEditor curve;
        juce::ToggleButton  onBtn { "Dyn EQ on" };
        juce::Label         keyLbl, hint;
        std::array<Column, ZBetel::StyleDucker::kNumBands> cols;
        bool seeding = false;
    };

    //==========================================================================
    //  The text of a channel setting
    //==========================================================================
    juce::String chanText (int param, float v);

    /** A knob on one setting (an atomic), in its 0..1. */
    void bindKnob (ZodiakKnob& k, std::atomic<float>& value, float min, float max, float def, float skew, bool stepped,
                   std::function<juce::String (float)> text);
}
