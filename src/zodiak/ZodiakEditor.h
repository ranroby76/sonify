// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakEditor.h
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <memory>
#include "PluginProcessor.h"
#include "ZodiakWidgets.h"
#include "ZodiakPanels.h"
#include "ZodiakMixer.h"

//==============================================================================
//  Zodiak's window (Rob's design):
//
//      HEADER     the logo, the SET (New, Load, Save, Save as), LOW VELOCITY
//                 RESPONSE (Grex's: lifts the quiet notes) and UI SIZE
//      MODE BAR   the page - PLAY | MIXER, a pill - MIDI LINK, and the set's name
//
//  The PLAY page:
//      STEPS      16 LEDs running with the song, over the banks and variations
//      BANNER     the 8 channels, one coloured row each: Ch N, BANK and VAR (the
//                 pattern: 4 x 8 per channel), what it plays (click: its VSTi -
//                 open, load, remove - or its graph), M and S, its e (the VSTi's
//                 own window), its nine macros, ROLL and GRAPH
//
//  The MIXER page, on the same room: one strip per channel, the MASTER
//  (ZodiakMixer.h)
//
//  Under both:
//      TRANSPORT  rewind, play, stop, bar . beat . step, the tempo, FOLLOW DAW
//
//  ROLL and GRAPH open over the banner:
//      GRAPH    the channel's graph (ZodiakGraphUi.h): its VSTi, its effects,
//               their cables, and the chosen node's settings
//      ROLL     the piano roll (the pattern editor; IMPORT / EXPORT MIDI)
//
//  A macro's right-click: MIDI LEARN, FORGET its controller, and DESTINATION -
//  any setting in its channel's graph (a node's, a plugin's parameter), or the
//  channel's transpose, octave, volume, pan.
//
//  Everything is laid out at the design size (2180 x 1200) and shown scaled by
//  UI SIZE (30% .. 100%).
//==============================================================================

namespace zodiak::ui
{
    class BannerRow;
    class StepLeds;
    class GraphPopup;
    class RollPopup;
    class TransportPanel;
    class ModeBar;
}

class ZodiakEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit ZodiakEditor (ZodiakProcessor&);
    ~ZodiakEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void openGraph (int ch);                                  // its graph, over the banner
    void openInstrument (int ch, juce::Component* near);      // its VSTi's own window (none: load one)
    void openRoll (int ch);
    void closePopup();

    /** The page under the mode bar: MIXER, or PLAY (the banner). */
    void showMixer (bool mixerPage);

    zodiak::ui::WindowHub& windows() noexcept   { return hub; }

private:
    void timerCallback() override;
    void applyScale();
    void layoutContent();
    void refreshAll();
    void showLowVel();

    void newSet();
    void loadSet();
    void saveSet();
    void saveSetAs();
    void showFailure (const juce::String& title, const juce::Result& r);

    ZodiakProcessor& proc;
    zodiak::ui::WindowHub hub;

    juce::Component  content;
    float            shownScale = 0.55f;

    juce::Label      logoLbl, subLbl, setLbl, setName, sizeLbl;
    juce::TextButton newBtn { "New" }, loadBtn { "Load" }, saveBtn { "Save" }, saveAsBtn { "Save as" };
    juce::ComboBox   sizeBox;
    juce::Label      lowVelLbl, lowVelValue;
    juce::Slider     lowVelSlider;

    std::unique_ptr<zodiak::ui::ModeBar> modeBar;
    std::unique_ptr<zodiak::ui::StepLeds> steps;
    std::array<std::unique_ptr<zodiak::ui::BannerRow>, zodiak::kNumChannels> rows;
    std::unique_ptr<zodiak::ui::TransportPanel> transport;
    std::unique_ptr<zodiak::ui::MixerPanel> mixer;
    std::unique_ptr<juce::Component> popup;

    juce::TooltipWindow tooltips { this, 600 };
    std::unique_ptr<juce::FileChooser> chooser;
    int shownGeneration = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZodiakEditor)
};
