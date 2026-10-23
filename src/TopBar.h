// D:\workspace\Fanan_web_hub\src\TopBar.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

//==============================================================================
/**
    The Plugins Creator Studio's strip across the top, under Sonify's own top bar.

        left    the FANAN AI wordmark, the Options menu, then the open plugin's name
        middle  the studio's tabs: Build with AI, Edit UI, Knob Studio, Protection, Store
        right   MIDI In (the ports the studio hears, with a light that blinks with them), the
                studio's own output meter (what it plays) and Sonify's audio load

    Sonify's audio device is Sonify's own business: its button is in Sonify's bottom bar.
*/
class TopBar final : public juce::Component,
                     private juce::Timer
{
public:
    TopBar();
    ~TopBar() override;

    void setProjectName (const juce::String& name);   // empty: no project open

    /** Which tab is showing, in the order of tabNames. */
    void setCurrentTab (int index);

    static constexpr int numTabs = 5;
    static constexpr const char* tabNames[numTabs] { "Build with AI", "Edit UI", "Knob Studio", "Protection", "Store" };

    std::function<void (int index)> onTabChosen;

    /** Read 30 times a second: a channel's peak since the last call, and the audio load (0..1). */
    std::function<float (int channel)> takePeak;
    std::function<double()> audioLoad;

    std::function<void()> onNew;
    std::function<void()> onOpen;
    std::function<void()> onSave;     // what's waiting is written at once
    std::function<void()> onSaveAs;   // the plugin copied into a new folder, under a new name
    std::function<void()> onRevealProject;
    std::function<void()> onToggleFinishSound;   // the "Sound when Sonify AI finishes" item
    std::function<bool()> isFinishSoundOn;
    std::function<void()> onCloseProject;
    std::function<void()> onSampleLibrary;       // a sampler's blobs (PluginLibrary)
    std::function<void (const juce::File& projectFile)> onOpenRecent;

    /** Asked for each time the Options menu opens: recent project files, newest first. */
    std::function<juce::StringArray()> recentProjects;

    /** MIDI In: the ports the studio can hear (each with the MIDI device on it, if there's one),
        those it hears ("*": all of them; its keyboards it always hears), and a count that goes
        up as MIDI comes through (the light blinks). The studio hears MIDI only while it's in
        front (AudioEngine::setMidiFocus). */
    struct MidiPort
    {
        juce::String name, device;
    };

    std::function<std::vector<MidiPort>()> midiInPorts;
    std::function<juce::StringArray()> midiInChosen;
    std::function<void (const juce::StringArray& ports)> onMidiInChosen;
    std::function<juce::uint32()> midiInActivity;

    /** MIDI In's button says again what it hears. */
    void refreshMidiIn();

    /** A short word in place of the plugin's name for a moment: "Saved". */
    void showNotice (const juce::String& text);

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int height = 46;

private:
    /** "Options", in plain text like a Windows menu bar: it lights up under the mouse and
        while its menu is open. */
    class MenuTitle final : public juce::Button
    {
    public:
        explicit MenuTitle (const juce::String& title) : juce::Button (title) {}
        void setOpen (bool shouldBeOpen)    { open = shouldBeOpen; repaint(); }
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;

    private:
        bool open = false;
    };

    /** One of the main tabs: its name, underlined in the accent while it's showing. */
    class TabButton final : public juce::Button
    {
    public:
        explicit TabButton (const juce::String& name) : juce::Button (name) {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    };

    /** Left and right output levels: green to -6 dB, amber to 0 dB, red above. */
    class Meter final : public juce::Component,
                        public juce::SettableTooltipClient
    {
    public:
        void setLevels (float left, float right);
        void paint (juce::Graphics&) override;

    private:
        static float toPosition (float gain) noexcept;

        float levels[2] {}, holds[2] {};
        int holdFrames[2] {};
    };

    /** MIDI In's button: its light, and what it hears ("All", a port, "2 ports"). */
    class MidiInButton final : public juce::Button
    {
    public:
        MidiInButton() : juce::Button ("MIDI In") {}
        void setSummary (const juce::String& text)   { summary = text; repaint(); }
        void setLit (bool on)                        { if (lit != on) { lit = on; repaint(); } }
        void setCompact (bool shouldBeCompact)       { compact = shouldBeCompact; }
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;

    private:
        juce::String summary { "All" };
        bool lit = false, compact = false;
    };

    void timerCallback() override;
    void showOptionsMenu();
    void showMidiInList();

    MenuTitle optionsMenu { "Options" };
    juce::OwnedArray<TabButton> tabs;
    MidiInButton midiInButton;
    juce::uint32 midiInSeen = 0;
    int midiInLitFrames = 0;
    Meter meter;
    juce::String loadText { "0%" };

    juce::String projectName;                                 // empty: no project open
    juce::Rectangle<int> wordmarkArea, loadArea;              // empty when the bar is too narrow for them
    juce::Rectangle<int> noticeArea;                          // between Options and the tabs: the plugin's name
    juce::String notice;
    juce::uint32 noticeUntil = 0;                             // when it goes (milliseconds)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopBar)
};
