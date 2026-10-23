// D:\workspace\Fanan_web_hub\src\MaterialChoices.h

#pragma once

#include <juce_graphics/juce_graphics.h>

#include "stella_paint.h"   // the materials, finishes, patterns and blends themselves

#include <utility>
#include <vector>

//==============================================================================
/**
    The materials, finishes, pattern layers and blend modes as the menus offer them (the
    Edit tab's painted backgrounds and shapes, the Knob Studio's layers): each a setting's name
    (stella_paint.h's) and what the menu calls it.
*/
namespace MaterialChoices
{
    struct Choice { const char* name; const char* label; };

    inline const std::vector<std::pair<const char*, std::vector<Choice>>>& materialGroups()
    {
        static const std::vector<std::pair<const char*, std::vector<Choice>>> groups {
            { "Metals", { { "aluminium", "Brushed aluminium" }, { "steel", "Brushed steel" }, { "silver", "Silver" }, { "chrome", "Chrome" },
                          { "gold", "Gold" }, { "rosegold", "Rose gold" }, { "brass", "Brass" }, { "copper", "Copper" }, { "bronze", "Bronze" },
                          { "titanium", "Liquid titanium" }, { "gunmetal", "Gunmetal" }, { "iron", "Cast iron" }, { "hammertone", "Hammertone" },
                          { "anodized", "Anodized" } } },
            { "Stone", { { "marble", "Marble" }, { "blackmarble", "Black marble" }, { "granite", "Granite" }, { "slate", "Slate" },
                         { "concrete", "Concrete" }, { "terrazzo", "Terrazzo" }, { "sandstone", "Sandstone" } } },
            { "Plastic, paint and ceramic", { { "powder", "Powder coat" }, { "plastic", "Glossy plastic" }, { "matteplastic", "Matte plastic" },
                                              { "rubber", "Rubber" }, { "bakelite", "Bakelite" }, { "ceramic", "Ceramic" }, { "smooth", "Plain colour" } } },
            { "Wood, leather and cloth", { { "wood", "Wood" }, { "walnut", "Walnut" }, { "oak", "Oak" }, { "rosewood", "Rosewood" },
                                           { "ebony", "Ebony" },
                                           { "whitewood", "White wood" }, { "orangewood", "Light orange wood" }, { "bordowood", "Bordeaux wood" },
                                           { "greywood", "Dark grey wood" }, { "guitarwood", "Yellow guitar wood (flamed)" }, { "burl", "Burl" }, { "leather", "Leather" }, { "tolex", "Tolex" }, { "felt", "Felt" } } },
            { "Glass", { { "clearglass", "Clear glass (see-through)" }, { "tintedglass", "Tinted glass (see-through)" },
                         { "frosted", "Frosted glass" }, { "glass", "Dark screen glass" } } },
            { "More", { { "carbon", "Carbon fibre" }, { "pearl", "Pearl" }, { "paper", "Paper" } } },
        };

        return groups;
    }

    inline std::vector<Choice> allMaterials()
    {
        std::vector<Choice> all;

        for (const auto& [group, choices] : materialGroups())
            all.insert (all.end(), choices.begin(), choices.end());

        return all;
    }

    inline const std::vector<Choice>& finishChoices()
    {
        static const std::vector<Choice> choices { { "natural", "As the material comes" }, { "polished", "Polished" }, { "satin", "Satin" },
                                                   { "brushed", "Brushed" }, { "hammered", "Hammered" }, { "sandblasted", "Sandblasted" },
                                                   { "matte", "Matte" }, { "liquid", "Liquid" } };
        return choices;
    }

    inline const std::vector<Choice>& patternChoices()
    {
        static const std::vector<Choice> choices { { "none", "No pattern" }, { "stripes", "Stripes" }, { "pinstripes", "Pinstripes" },
                                                   { "checkers", "Checkers" }, { "dots", "Polka dots" }, { "grid", "Grid" },
                                                   { "hexagons", "Honeycomb" }, { "diamondplate", "Diamond plate" }, { "knurl", "Knurling" },
                                                   { "crosshatch", "Crosshatch" }, { "waves", "Waves" }, { "chevron", "Chevron" },
                                                   { "scales", "Fish scales" }, { "rings", "Rings" }, { "guilloche", "Guilloche (engine-turned)" },
                                                   { "sunburst", "Sunburst" }, { "perforated", "Perforated" }, { "weave", "Weave" },
                                                   { "noise", "Noise" }, { "camo", "Camouflage" }, { "damascus", "Damascus" },
                                                   { "bricks", "Bricks" }, { "triangles", "Triangles" } };
        return choices;
    }

    inline const std::vector<Choice>& blendChoices()
    {
        static const std::vector<Choice> choices { { "normal", "Normal" }, { "multiply", "Multiply" }, { "screen", "Screen" },
                                                   { "overlay", "Overlay" }, { "softlight", "Soft light" }, { "add", "Add" },
                                                   { "darken", "Darken" }, { "lighten", "Lighten" }, { "emboss", "Emboss (raised)" },
                                                   { "deboss", "Deboss (pressed in)" }, { "engrave", "Engrave (cut and darkened)" } };
        return choices;
    }

    /** A choice by its name (any case, spaces and dashes ignored); -1 if none. */
    inline int indexOf (const std::vector<Choice>& choices, const juce::String& name)
    {
        const auto wanted = name.trim().toLowerCase().removeCharacters (" -_");

        for (size_t i = 0; i < choices.size(); ++i)
            if (wanted == choices[i].name)
                return (int) i;

        return -1;
    }

    /** A material's own colours (a wood's by its name), as stella_paint paints it. */
    inline void materialColours (const juce::String& name, juce::Colour& main, juce::Colour& second)
    {
        namespace p = stella::paint;
        const auto& info = p::infoOf (p::materialNamed (name.toStdString()));
        main = juce::Colour (info.colour);
        second = juce::Colour (info.colour2);

        if (p::Rgb wood, grain; p::woodColours (name.toStdString(), wood, grain))
        {
            main = juce::Colour::fromFloatRGBA (wood.r, wood.g, wood.b, 1.0f);
            second = juce::Colour::fromFloatRGBA (grain.r, grain.g, grain.b, 1.0f);
        }
    }

    /** Whether a material's texture uses its second colour (marble's veins, wood's grain...);
        on the others a second colour is a two-tone shade. */
    inline bool usesSecondColour (const juce::String& name)
    {
        namespace p = stella::paint;

        switch (p::materialNamed (name.toStdString()))
        {
            case p::Material::marble: case p::Material::blackMarble: case p::Material::granite: case p::Material::terrazzo:
            case p::Material::slate: case p::Material::concrete: case p::Material::sandstone: case p::Material::wood:
            case p::Material::burl: case p::Material::flamewood: case p::Material::bakelite:
                return true;

            default:
                return false;
        }
    }
}
