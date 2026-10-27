// C:\workspace\SONIFY PLUGIN zax\src\PluginEditor.h
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>
#include "PluginProcessor.h"
#include "ZaxKeyboard.h"
#include "ZaxWidgets.h"

//==============================================================================
//  Zax's window.
//
//      HEADER    the kit (factory kits, then My Kits), LOAD, SAVE, SAVE AS,
//                RELOAD, and UI SIZE: the whole window 40% .. 100% in 5% steps
//                LOAD takes a .zaxkit from anywhere (a kit someone shared, a
//                folder of your own); SAVE writes over the kit's own file, and
//                SAVE AS keeps it as a new kit in My Kits
//      KEYBOARD  GM keys 27..84 with their names on arrows (ZaxKeyboard.h)
//      KEY       the selected key: its GM name and LOCKED role, its SOUND
//                (only sounds of that role), its choke group, its MIDI LED
//      SLIDERS   Grex's vertical sliders under a tab selector:
//                MAIN (the macros, gain and the key's filter), the sound's own
//                tabs (every value that shapes it), FX (the key's family
//                chain: SWEET > EQ > SAT > COMP > CHORUS > DELAY > REVERB > OUT)
//                and KIT (the kit's volume).  Every value 0..200.
//
//  Every change plays from the next hit.  Message thread only.
//==============================================================================
class ZaxEditor final : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit ZaxEditor (ZaxProcessor&);
    ~ZaxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    enum { kTune = 0, kDecay, kDamp, kSnap, kColor, kDrive, kHuman, kVelSens, kGain, kNumKnobs };

    void timerCallback() override;
    void applyScale();
    void layoutContent();

    // Kit menu and files
    void refreshKitMenu();
    void kitChosen();
    void loadKitFile();
    void saveKit();
    void saveKitAs();
    void reloadKit();
    void updateKitStatus();

    // The selected key
    zax::Cell& cell()                       { return proc.kit().cells[(size_t) selectedKey]; }
    void selectKey (int key);
    void refreshEditor();
    void rebuildSliders();
    void layoutSliders();
    void soundChosen();
    void knobMoved (int k);
    void paramMoved (int idx);
    void showTabInfo (int i);
    void edited();

    // The FX tab
    void buildFxPage();
    void fxMoved (int idx);
    void toggleFxStage();
    void refreshPageBar();

    static float toUi (int k, float v);
    static float fromUi (int k, float u);
    static float defaultFor (int k);

    ZaxProcessor& proc;

    // Every control lives here, laid out at the design size (1560 x 860) and
    // shown scaled by the UI SIZE menu (40% .. 100%).
    juce::Component   content;
    float             shownScale = 0.8f;
    juce::Label       sizeLbl;
    juce::ComboBox    sizeBox;

    juce::Label       titleLbl, subLbl, statusLbl;
    juce::ComboBox    kitBox;
    juce::TextButton  loadBtn { "LOAD" }, saveBtn { "SAVE" }, saveAsBtn { "SAVE AS" }, reloadBtn { "RELOAD" };
    std::vector<zax::files::Entry> kitEntries;
    int               shownGeneration = -1;

    ZaxKeyboard       keyboard;

    juce::Label       keyLbl, gmLbl, roleLbl, soundLbl, chokeLbl, hintLbl;
    juce::ComboBox    soundBox, chokeBox;
    juce::TextButton  resetBtn { "RESET KEY" };
    ZaxLed            midiLed;
    std::vector<int>  soundIndices;

    ZaxTabBar         tabBar;
    ZaxPageBar        pageBar;
    juce::TextButton  stageBtn { "ON" };
    juce::Label       fxFamilyLbl;
    BandFilterSlider  filterSlider { "FILTER" };
    std::vector<std::unique_ptr<GoldSlider>>        sliders;
    std::vector<const edm::params::Spec*>           sliderSpecs;
    std::vector<int>                                fxSliderSpec;
    juce::Rectangle<int> sliderArea;

    int  selectedKey = 36;
    int  currentTab  = 0;
    int  tabKey      = -1;
    int  fxTab       = -1, kitTab = -1;
    int  fxPage      = 0;
    bool updating    = false;
    int  headerBottom = 0, keyboardBottom = 0;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZaxEditor)
};
