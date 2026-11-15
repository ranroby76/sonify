// D:\workspace\Fanan_web_hub\src\Model3D.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

struct LayerDoc;

//==============================================================================
/**
    The 3D Studio's controls: a knob, a push button, a slider or a toggle switch, built as a
    real object (a turned body, a rounded cap, a fader on its track, a lever on its nut) in
    materials, lit by the panel's global light and rendered (Render3D) into the frames of a
    look: a knob turns through them (its highlights stay where the light is, as on hardware),
    a slider's cap travels its track, a button goes down and lights up, a lever flips.

    A model is its kind and its settings by name (paramsFor lists them, with their ranges and
    groups, for the studio's panel and for anyone writing one by hand); a missing setting is
    its default. It lives in a look's document (LayerDoc::model3d), so a 3D look is a look like
    any other: worn by controls, in the Gallery, exported as a filmstrip, in a design's code.
*/
struct Model3D
{
    enum class Kind { knob = 0, button, slider, toggle };

    Kind kind = Kind::knob;
    juce::NamedValueSet values;   // its settings, by key (missing: the default)

    //==========================================================================
    /** One setting: what the panel shows for it. */
    struct Param
    {
        enum class Type { number, choice, colour, flag, text };

        juce::String key, label, group, tip;
        Type type = Type::number;
        float min = 0.0f, max = 1.0f, step = 1.0f, def = 0.0f;
        juce::StringArray choices;      // choice: its names (stored as the index)
        juce::uint32 colour = 0;        // colour: the default
        juce::String text;              // text: the default
        juce::String suffix;            // number: its unit (" %", " deg")
        juce::String shownWhen;         // "key=value" (or "key" for a flag that's on): only then
    };

    static const std::vector<Param>& paramsFor (Kind);
    static const Param* findParam (Kind, const juce::String& key);

    float number (const juce::String& key) const;
    int choice (const juce::String& key) const;
    bool flag (const juce::String& key) const;
    juce::Colour colour (const juce::String& key) const;
    juce::String text (const juce::String& key) const;
    void set (const juce::String& key, const juce::var& value);

    /** Whether a setting applies now (its shownWhen). */
    bool isShown (const Param&) const;

    /** A setting given by name, as Fanan AI (make_3d_look) or a design's code gives it: a choice
        by its name (any case) or its number, a flag as true or false, a colour as "#AARRGGBB"
        (or "#RRGGBB"), a number kept in its range, words as they are. "" when it's set, else
        what was wrong (a key the kind doesn't have, a choice it doesn't offer...). */
    juce::String setByName (const juce::String& key, const juce::var& value);

    /** A kind's settings, a line each, in its groups: key, what it is, its range (or choices),
        its default, when it applies. For Fanan AI and anyone writing a model by hand. */
    static juce::String describe (Kind);

    //==========================================================================
    static juce::StringArray kindNames();               // "Knob", "Button", "Slider", "Toggle switch"
    static juce::String kindKey (Kind);                 // "knob", "button", "slider", "toggle"
    static Kind kindFromKey (const juce::String&);

    /** The materials every part can be made of. */
    static const juce::StringArray& materialNames();

    /** Ready-made ones of a kind, as starting points. */
    static juce::StringArray presetNames (Kind);
    static Model3D preset (Kind, int index);

    /** A new look's document for a model: its canvas and frames as its kind needs them (a
        slider: across when `across`). */
    static LayerDoc newDocument (const Model3D& model, bool across = false);

    juce::ValueTree toValueTree() const;
    static std::shared_ptr<const Model3D> fromValueTree (const juce::ValueTree&);
};
