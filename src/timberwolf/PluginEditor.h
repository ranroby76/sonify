// D:\workspace\Fanan_web_hub\src\timberwolf\PluginEditor.h
#pragma once
//==============================================================================
//  Timber Wolf's window: a modern multi-effect unit, in 3D.
//
//      LEFT      the name, the rigs (factory, then the user's: click one to load
//                it), Save / Save As / Load / Folder / bank, the four
//                snapshots, the tempo and TAP, the tuner, input and output,
//                and the window's size
//      SCREEN    the chain graph (TwChainView.h); the tuner shows here too
//      DECK      the effect being edited: its badge, name and description, its
//                on switch and its knobs (3D: TwControls.h); a split's or a
//                blend's settings when one is open
//      FOOT      eight footswitches and two expression pedals: right-click one
//                for its job, right double-click to MIDI learn it
//
//  The whole window scales (UI size) from its design size, 1180 x 720.
//  Message thread only.
//==============================================================================
#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "TwChainView.h"
#include "TwControls.h"

#include <memory>
#include <vector>

class TimberWolfEditor final : public juce::AudioProcessorEditor,
                               private juce::Timer
{
public:
    explicit TimberWolfEditor (TimberWolfProcessor&);
    ~TimberWolfEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designWidth = 1180, designHeight = 720;

private:
    class Content;
    class Panel;
    class PresetList;
    class Lnf;

    void timerCallback() override;
    void applyScale();
    void chainChanged();
    void valuesChanged();
    void open (int uid);

    void footswitchMenu (int i);
    void pedalMenu (int i);
    void snapshotMenu (int i);
    void savePreset (bool askName);
    void loadPresetFromFile();
    void saveBank();
    void loadBank();

    TimberWolfProcessor& proc;
    std::unique_ptr<Lnf> lnf;
    std::unique_ptr<Content> content;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimberWolfEditor)
};
