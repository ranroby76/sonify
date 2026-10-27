// C:\workspace\SONIFY PLUGIN natura\src\PluginEditor.h
#pragma once
//==============================================================================
//  PluginEditor.h  -  the panel: archetype selector and presets on top,
//  MODEL / ARTIC / SYNTHESIS / MODULATION / FX tabs, the keyboard below
//  (keyswitch keys lit in gold).
//==============================================================================
#include "NaturaPanels.h"
#include "FananLookAndFeel.h"

namespace natura
{
    /** Bambino's look, plus Grex's square buttons: a button whose colours were
        set (InstrEditStyle::styleSquareButton) is drawn in those colours. */
    class NaturaLookAndFeel final : public FananLookAndFeel
    {
    public:
        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg,
                                   bool highlighted, bool down) override
        {
            if (! b.isColourSpecified (juce::TextButton::buttonColourId))
            {
                FananLookAndFeel::drawButtonBackground (g, b, bg, highlighted, down);
                return;
            }
            const bool on = b.findColour (juce::TextButton::buttonColourId) == juce::Colour (Theme::Gold::kAccent);
            auto r = b.getLocalBounds().toFloat().reduced (0.5f);
            const float rad = juce::jmin (5.0f, r.getHeight() * 0.25f);
            auto fill = b.findColour (juce::TextButton::buttonColourId);
            if (! on && (highlighted || down)) fill = fill.brighter (down ? 0.05f : 0.15f);
            g.setColour (fill);
            g.fillRoundedRectangle (r, rad);
            g.setColour (on ? juce::Colour (Theme::Gold::kAccentLight) : Theme::Color::border);
            g.drawRoundedRectangle (r, rad, 1.0f);
        }
    };

    //==========================================================================
    class ModelPage final : public Page
    {
    public:
        explicit ModelPage (NaturaProcessor& p);
        void resized() override;
        void setArchetype (int a);

    private:
        int fExciter = -1, fRes = -1, fBody = -1, fChar = -1, fUnison = -1;
        int fOsc1 = -1, fOsc2 = -1, fMix = -1, fLadder = -1, fLadEnv = -1;
        std::vector<int> fSpec;                 // one per acoustic archetype
        int arch = 0;
        juce::Component* unisonSliders[2] {};
    };

    class ArticPage final : public Page, private juce::Timer
    {
    public:
        explicit ArticPage (NaturaProcessor& p);
        void resized() override;
        void paint (juce::Graphics& g) override;
        void setArchetype (int a);

    private:
        void timerCallback() override;
        void refreshButtons();
        juce::OwnedArray<juce::TextButton> artButtons;
        juce::Component artGrid;
        int fArts = -1, fKs = -1, fCc = -1, fVel = -1, fAuto = -1, fRates = -1;
        int arch = 0, shown = -1;
    };

    class SynthesisPage final : public Page
    {
    public:
        explicit SynthesisPage (NaturaProcessor& p);
        void resized() override;

    private:
        struct CurveBox : juce::Component
        {
            CurveBox (NaturaProcessor& proc);
            void resized() override { display.setBounds (getLocalBounds().withSizeKeepingCentre (juce::jmin (getWidth(), getHeight()), juce::jmin (getWidth(), getHeight()))); }
            VelCurveDisplay display;
            juce::ParameterAttachment attach;
        };
        std::unique_ptr<CurveBox> curve;
        std::unique_ptr<ParamRadio> filterType, voiceMode;
        int fAmp = -1, fFilter = -1, fFenv = -1, fCurve = -1, fVoice = -1, fOct = -1, fRange = -1;
    };

    class ModulationPage final : public Page
    {
    public:
        explicit ModulationPage (NaturaProcessor& p);
        void resized() override;

    private:
        int fAlfo = -1, fFlfo = -1, fVib = -1, fGlide = -1, fPerf = -1;
    };

    class FxPage final : public Page
    {
    public:
        explicit FxPage (NaturaProcessor& p);
        void resized() override;

    private:
        int fRot = -1, fPha = -1, fCho = -1, fDly = -1, fRev = -1, fOut = -1;
    };

    //==========================================================================
    /** The keyboard: keyswitch keys lit, the active one brightest. */
    class NaturaKeyboard final : public juce::MidiKeyboardComponent
    {
    public:
        NaturaKeyboard (NaturaProcessor& p);
        void update();

    protected:
        void drawWhiteNote (int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                            bool isDown, bool isOver, juce::Colour lineColour, juce::Colour textColour) override;
        void drawBlackNote (int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                            bool isDown, bool isOver, juce::Colour noteFillColour) override;

    private:
        int ksState (int note) const;          // 0 none, 1 keyswitch, 2 the active one
        NaturaProcessor& proc;
        int ksBase = -1, ksCount = 0, ksActive = -1;
        bool ksOn = false;
    };
}

//==============================================================================
class NaturaEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NaturaEditor (NaturaProcessor&);
    ~NaturaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showTab (int index);
    void showPresetMenu();
    void stepPreset (int delta);
    void loadPreset();
    void savePresetAs();
    void quickSave();
    void refreshPresetLabel();

    NaturaProcessor& proc;
    natura::NaturaLookAndFeel laf;

    juce::ComboBox archetypeBox;
    juce::TextButton presetName, prevBtn { "<" }, nextBtn { ">" };
    juce::TextButton loadBtn { "LOAD" }, saveBtn { "SAVE" }, quickBtn { "QUICK SAVE" };
    juce::Label status;
    juce::TextButton tabs[5];
    std::unique_ptr<natura::ModelPage> model;
    std::unique_ptr<natura::ArticPage> artic;
    std::unique_ptr<natura::SynthesisPage> synthesis;
    std::unique_ptr<natura::ModulationPage> modulation;
    std::unique_ptr<natura::FxPage> fxPage;
    std::unique_ptr<natura::NaturaKeyboard> keyboard;
    std::unique_ptr<juce::FileChooser> chooser;

    int currentTab = 0;
    int shownArch = -1;
    int presetCursor = -1;          // position in the combined factory + user list
    juce::String shownPreset;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NaturaEditor)
};
