#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"

/** The editor is a thin shell around a WebBrowserComponent.

    All of the UI lives in Resources/ui and is embedded as binary data, so the
    plugin has no external files at runtime. The native side exposes a small,
    generic bridge rather than one relay per parameter — with every pedal's
    parameters that is both less code and less to keep in sync.
*/
class AxisAudioProcessorEditor : public juce::AudioProcessorEditor,
                                 private juce::Timer
{
public:
    explicit AxisAudioProcessorEditor (AxisAudioProcessor&);
    ~AxisAudioProcessorEditor() override;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    std::optional<juce::WebBrowserComponent::Resource> provide (const juce::String& url);

    juce::var handleSetParam     (const juce::Array<juce::var>& args);
    juce::var handleGesture      (const juce::Array<juce::var>& args, bool begin);
    juce::var handleGetState     (const juce::Array<juce::var>& args);
    juce::var handleChainOrder   (const juce::Array<juce::var>& args);
    // The cabinet: its plate on the rail opens a menu drawn in the page (as the rig's is), and
    // what's picked comes back here: a modelled cabinet, an IR from the AXIS pack, an IR file, or
    // none.
    juce::var handleCabChoose    (const juce::Array<juce::var>& args);

    // Rigs (presets): the name on the LCD opens the menu, its arrows step through the rigs, and
    // LOAD, SAVE and QUICK are the three buttons beside it. The menu itself is drawn in the page
    // (a juce::PopupMenu opened from a click inside the web view is dismissed by the click that
    // asked for it, so it only flickers); the page says what was picked.
    juce::var handlePresetChoose (const juce::Array<juce::var>& args);
    juce::var handlePresetStep  (const juce::Array<juce::var>& args);
    juce::var handlePresetLoad  (const juce::Array<juce::var>& args);
    juce::var handlePresetSave  (const juce::Array<juce::var>& args);
    juce::var handlePresetQuick (const juce::Array<juce::var>& args);

    void savePresetAs();
    void report (const juce::Result& result);

    juce::String buildStateJson() const;

    AxisAudioProcessor& proc;

    std::unique_ptr<juce::WebBrowserComponent> web;
    std::unique_ptr<juce::FileChooser> chooser;

    bool uiReady = false;
    int presetCursor = -1;      // where the arrows are in the list of rigs (-1: nowhere yet)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AxisAudioProcessorEditor)
};
