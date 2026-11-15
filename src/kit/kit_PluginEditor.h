// D:\workspace\Fanan_web_hub\src\kit\kit_PluginEditor.h

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Looks.h"
#include "PluginCanvas.h"
#include "PluginProcessor.h"
#include "PresetLibrary.h"
#include "StellaLookAndFeel.h"

#include <memory>
#include <vector>

//==============================================================================
/**
    The plugin's window: its panel as designed in Sonify's Plugins Creator Studio
    (PluginDesign::window(), PluginWindow.cpp), drawn and played by Sonify's own window code
    (PluginCanvas, in Play, the panel alone), so it looks and works exactly as it does there
    and on Sonify's tracks:

        controls      move the plugin's parameters (the host hears them: automation)
        meters, lamps, scopes   watch the plugin's signals and module displays
        keyboards     play it
        preset manager   its presets, the user's own (Save, Save as, Load, banks)
        pages, tabs, programmed elements, the 3D body, the global light and camera: as designed

    It grows and shrinks keeping its shape; a side panel (a preset manager's) opening makes it
    wider. A sampler whose blobs aren't on this computer says which under the panel. A protected
    plugin's registration badge opens its Registration window.
*/
class SonifyPluginEditor final : public juce::AudioProcessorEditor,
                                 private juce::Timer
{
public:
    explicit SonifyPluginEditor (SonifyPluginProcessor&);
    ~SonifyPluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void pushParameters (bool always);
    void followWindow();
    void showRegistration();

    SonifyPluginProcessor& processor;
    StellaLookAndFeel lookAndFeel;     // Sonify's: its menus and dialogs
    Looks looks;                       // before the canvas, which draws with it
    PluginCanvas canvas;
    PresetUi presetUi { *this, processor.presets };
    juce::Label libraryBar;
    std::unique_ptr<juce::AlertWindow> registration;

    juce::Rectangle<int> shownWindow;  // the window's plugin pixels, as last fitted
    std::vector<float> shown;          // the values the canvas has
    bool wasRegistered = true;

    static constexpr int barHeight = 34;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SonifyPluginEditor)
};
