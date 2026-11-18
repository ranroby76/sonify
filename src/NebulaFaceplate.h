// NebulaFaceplate.h
//
// The NebulaVerb / Space Lifter 5 faceplate: the whole dark rack panel (knobs, lever switches,
// algorithm selector, amber display, LED meter), drawn in code with juce::Graphics.
//
// ONE SOURCE, TWO HOSTS. This file and NebulaFaceplate.cpp are compiled unchanged by:
//   - the NebulaVerb plugin (D:\workspace\NebulaVerb\Source\ui), and
//   - Sonify's built-in Space Lifter 5 (D:\workspace\Fanan_web_hub\src).
// Keep the two copies byte-identical. Each host only supplies a Model: how a parameter is read and
// written, the output peaks, the reverb types' shapes and the brand words. Nothing else differs.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

namespace nebula_ui
{
// The parameters the faceplate shows, in real units (ms, s, Hz, %, dB, semitones, 0/1 switches).
enum class Param
{
    type, mix, predelay, decay, modulation, earlyLate, lowCut, highCut, damping,
    shimmer, feedback, shift, split, gate, threshold, attack, hold, release, bypass
};

constexpr int numParams = 19;

// The knob mapping (how far round the knob a value sits), its default and its words:
// the same everywhere the faceplate is shown.
juce::NormalisableRange<float> rangeOf (Param);
float defaultOf (Param);
juce::String textOf (Param, float value);

// What the display needs to know about each reverb type.
struct TypeShape
{
    juce::String name;
    float hiMult = 0.6f;              // high-band decay multiplier at damping 50 %
    int erTaps = 12;
    float erStartMs = 8.0f, erSpanMs = 80.0f;
};

// The host's side.
struct Model
{
    virtual ~Model() = default;

    virtual float get (Param) const = 0;                // real value
    virtual void set (Param, float realValue) = 0;      // from a knob, switch or the selector
    virtual void beginGesture (Param) {}
    virtual void endGesture (Param) {}

    virtual float outputPeak (int channel) const = 0;   // linear, 0..1+ (the LED meter)
    virtual unsigned outputOvers (int) const { return 0; }   // blocks past 0 dBFS so far (the clip light: a change lights it)

    virtual std::vector<TypeShape> types() const = 0;

    // Brand words: the plugin's own name on its plate.
    virtual juce::String title() const = 0;             // "NEBULA"
    virtual juce::String badgeCode() const = 0;         // "NV-8"
    virtual juce::String footer() const = 0;            // "FANAN · NEBULA NV-8"
};

//==============================================================================
// The panel at its design size (designW x designH), every control on the model.
class Faceplate final : public juce::Component
{
public:
    static constexpr int designW = 1440, designH = 580;

    explicit Faceplate (Model&);
    ~Faceplate() override;

    void resized() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

// The faceplate scaled to whatever size this component is given (keep its aspect ratio).
class ScaledFaceplate final : public juce::Component
{
public:
    explicit ScaledFaceplate (Model& model) : plate (model) { addAndMakeVisible (plate); }

    void paint (juce::Graphics& g) override   { g.fillAll (juce::Colour (0xff070708)); }

    void resized() override
    {
        plate.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) Faceplate::designW,
                                                          (float) getHeight() / (float) Faceplate::designH));
        plate.setBounds (0, 0, Faceplate::designW, Faceplate::designH);
    }

private:
    Faceplate plate;
};
} // namespace nebula_ui
