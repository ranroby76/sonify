// D:\workspace\Fanan_web_hub\src\ProjectTools.cpp

#include "ProjectTools.h"
#include "GuiLayout.h"
#include "GalleryBrowser.h"   // the KnobMan gallery on this computer
#include "KnobFile.h"
#include "LayerRender.h"
#include "Looks.h"
#include "MaterialChoices.h"
#include "PanelParts.h"
#include "PluginLibrary.h"
#include "PresetBank.h"
#include "Settings.h"
#include "Skills.h"
#include "SoundCheck.h"
#include "SoundMeasure.h"
#include "WasmCompiler.h"
#include "WasmElements.h"
#include "stella_element_host.h"

#include "StellaRuntimeData.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

namespace
{
    /** What a plugin of each type is, for the AI. */
    juce::String kindRole (PluginKind kind)
    {
        switch (kind)
        {
            case PluginKind::effect:
                return "It's an effect: it reads plugin.in L / plugin.in R and writes plugin.out L / plugin.out R.";

            case PluginKind::widget:
                return "It's a widget: something that isn't an instrument, an effect or a MIDI FX (a tuner, a visualizer, an analyzer, "
                       "a tool, or something new). It gets the audio (plugin.in L / plugin.in R) and the notes, and writes plugin.out L / "
                       "plugin.out R: pass the audio through unchanged unless it's meant to change it. Its panel is often the heart of it.";

            case PluginKind::noteEffect:
                return "It's an old MIDI effect project: plugins made here don't send MIDI. A MIDI FX is made by starting a new plugin "
                       "of type MIDI FX (it's built as one of Sonify's MIDI devices, in this same conversation panel).";

            case PluginKind::instrument:
                break;
        }

        return "It's an instrument: notes in, audio out on plugin.out L / plugin.out R.";
    }


    constexpr int maxFileBytes = 200 * 1024;
    constexpr int maxLogChars = 8000;
    constexpr int maxListedElements = 200;   // the GUI elements listed in the project's description
    constexpr int maxListedReference = 60;   // the user's own code files listed by name, with every request (search_reference finds the rest)
    constexpr int maxDesignChars = 9000;     // the design brief shown with every request (the rest is read with read_file)

    juce::var object()
    {
        return juce::var (new juce::DynamicObject());
    }

    juce::var tool (const juce::String& name, const juce::String& description, const juce::var& properties,
                    const juce::StringArray& required)
    {
        auto schema = object();
        schema.getDynamicObject()->setProperty ("type", "object");
        schema.getDynamicObject()->setProperty ("properties", properties.isObject() ? properties : object());

        juce::Array<juce::var> needed;

        for (const auto& r : required)
            needed.add (r);

        schema.getDynamicObject()->setProperty ("required", needed);

        auto definition = object();
        definition.getDynamicObject()->setProperty ("name", name);
        definition.getDynamicObject()->setProperty ("description", description);
        definition.getDynamicObject()->setProperty ("input_schema", schema);
        return definition;
    }

    juce::var property (const juce::String& type, const juce::String& description)
    {
        auto p = object();
        p.getDynamicObject()->setProperty ("type", type);
        p.getDynamicObject()->setProperty ("description", description);
        return p;
    }

    juce::var listProperty (const juce::String& itemType, const juce::String& description)
    {
        auto items = object();
        items.getDynamicObject()->setProperty ("type", itemType);

        auto p = property ("array", description);
        p.getDynamicObject()->setProperty ("items", items);
        return p;
    }

    /** One element of the GUI in a line, its fields named as in layout.json: what the AI needs
        to find it and change it with edit_layout. */
    /** A shape's layer style, the effects that are on: "bevel,stroke,dropShadow". */
    juce::String effectsOf (const GuiWidget& w)
    {
        const auto found = w.settings.find ("effects");

        if (found == w.settings.end())
            return {};

        const auto style = LayerStyle::fromVar (found->second);
        juce::StringArray on;

        if (style.bevel.enabled)         on.add (style.bevel.contourEnabled ? "bevel+contour" : "bevel");
        if (style.stroke.enabled)        on.add ("stroke");
        if (style.innerShadow.enabled)   on.add ("innerShadow");
        if (style.colourOverlay.enabled) on.add ("colorOverlay");
        if (style.dropShadow.enabled)    on.add ("dropShadow");

        return on.joinIntoString (",");
    }

    juce::String elementLine (int index, const GuiWidget& w, const juce::StringArray& paramIds)
    {
        juce::String line;
        const bool dropdown = w.type == GuiWidget::Type::custom && w.element == GuiLayout::builtInElementFor ("dropdown");
        const auto kind = w.type == GuiWidget::Type::shape ? w.shapeKind() : juce::String();
        line << index << ": " << (dropdown ? juce::String ("dropdown")
                                  : kind.isNotEmpty() && kind != "drawn" ? kind   // a shape's type is its kind
                                                                          : GuiLayout::typeName (w.type));

        if (w.type == GuiWidget::Type::custom && ! dropdown)
            line << " element=" << (w.element.isNotEmpty() ? w.element : juce::String ("(none)"));

        if (w.label.isNotEmpty())
            line << " \"" << w.label.replaceCharacters ("\r\n", "  ").substring (0, 48) << "\"";

        auto addParam = [&] (const juce::String& key, const juce::String& id)
        {
            line << " " << key << "=" << id;

            if (! paramIds.isEmpty() && ! paramIds.contains (id))
                line << " (not in the plugin yet)";
        };

        if (w.param.isNotEmpty())
            addParam ("param", w.param);

        for (const auto& [role, id] : w.roles)
            addParam (role, id);

        if (w.source.isNotEmpty())
            line << " source=" << w.source;

        for (const auto& [role, watched] : w.sources)
            line << " source." << role << "=" << watched;

        if (! w.options.isEmpty() && (w.type == GuiWidget::Type::selector || w.type == GuiWidget::Type::custom))
            line << " options=" << w.options.joinIntoString ("|").substring (0, 80);

        if (w.image.isNotEmpty())
            line << " image=" << w.image;

        if (w.type == GuiWidget::Type::keyboard)
            line << " low=" << GuiLayout::noteName (w.lowNote) << " high=" << GuiLayout::noteName (w.highNote);

        line << " x=" << w.bounds.getX() << " y=" << w.bounds.getY();

        if (w.type == GuiWidget::Type::knob)
            line << " size=" << w.bounds.getWidth();
        else
            line << " w=" << w.bounds.getWidth() << " h=" << w.bounds.getHeight();

        if (w.style.isNotEmpty())
            line << " style=\"" << w.style << "\"";

        // A shape's material and its layer style (read gui/layout.json for their settings).
        if (w.type == GuiWidget::Type::shape)
        {
            if (const auto found = w.settings.find ("material"); found != w.settings.end() && found->second.toString().isNotEmpty())
                line << " material=" << found->second.toString();

            if (const auto effects = effectsOf (w); effects.isNotEmpty())
                line << " effects=" << effects;

            if (const auto found = w.settings.find ("rounding"); found != w.settings.end())
                line << " rounding=" << juce::String ((double) found->second, 2);
        }

        // A meter's display (read gui/layout.json for its colours and the rest).
        if (w.type == GuiWidget::Type::meter)
            if (const auto found = w.settings.find ("display"); found != w.settings.end() && found->second.toString().isNotEmpty())
                line << " display=" << found->second.toString();

        // Its text's font, and the preset manager's corners.
        if (w.font.isNotEmpty())
            line << " font=\"" << w.font << "\"";

        if (w.type == GuiWidget::Type::preset && w.rounding >= 0.0f)
            line << " rounding=" << juce::String (w.rounding, 2);

        // Every element is an object: turned, see-through, on a page.
        if (w.shownAngle() != 0.0f)
            line << " angle=" << juce::String (w.angle, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".");

        if (w.shownOpacity() < 1.0f)
            line << " opacity=" << juce::String (w.opacity, 2);

        if (w.page.isNotEmpty())
            line << " page=\"" << w.page << "\"";

        return line;
    }

    /** An element's index as the AI gave it: a number (or a number in a string); -1 if it isn't one. */
    int indexFrom (const juce::var& value)
    {
        if (value.isInt() || value.isInt64())
            return (int) value;

        if (value.isDouble())
        {
            const auto number = (double) value;
            return std::abs (number - std::round (number)) < 1.0e-6 ? juce::roundToInt (number) : -1;
        }

        const auto text = value.toString().trim();
        return text.isNotEmpty() && text.containsOnly ("0123456789") ? text.getIntValue() : -1;
    }

    /** An element with some of its fields set anew (named as in layout.json); the rest stay. */
    GuiWidget changedWidget (const GuiWidget& widget, const juce::var& change)
    {
        auto merged = GuiLayout::widgetToVar (widget);
        auto* target = merged.getDynamicObject();
        const auto* fields = change.getDynamicObject();

        if (target == nullptr || fields == nullptr)
            return widget;

        // Fields that go by more than one name: the one given replaces them all.
        const juce::StringArray synonyms[] { { "label", "text" }, { "color", "colour" }, { "style", "look" },
                                             { "low", "from" }, { "high", "to" } };

        for (const auto& field : fields->getProperties())
            for (const auto& names : synonyms)
                if (names.contains (field.name.toString()))
                    for (const auto& name : names)
                        target->removeProperty (name);

        // A knob's diameter is its "size" (or "w").
        const auto typeAfter = fields->hasProperty ("type") ? GuiLayout::typeFromName (fields->getProperty ("type").toString()) : widget.type;

        if (typeAfter == GuiWidget::Type::knob && (fields->hasProperty ("size") || fields->hasProperty ("w")))
        {
            target->removeProperty ("size");
            target->removeProperty ("w");
        }

        for (const auto& field : fields->getProperties())
            if (field.name.toString() != "index")
                target->setProperty (field.name, field.value);

        return GuiLayout::widgetFromVar (merged);
    }

    bool isIdentifier (const juce::String& text)
    {
        return text.isNotEmpty() && text.length() <= 64
            && text.containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")
            && ! juce::CharacterFunctions::isDigit (text[0]);
    }

    int countLines (const juce::String& text)
    {
        return juce::StringArray::fromLines (text).size();
    }

    /** A picture's size in pixels, read once per version of the file. */
    juce::Point<int> pictureSize (const juce::File& file)
    {
        static std::map<juce::String, std::pair<juce::Time, juce::Point<int>>> known;

        const auto modified = file.getLastModificationTime();
        auto& entry = known[file.getFullPathName()];

        if (entry.first != modified)
        {
            const auto image = juce::ImageFileFormat::loadFrom (file);
            entry = { modified, { image.getWidth(), image.getHeight() } };
        }

        return entry.second;
    }

    /** What a painter element asks while it paints: its settings, label, options and colour from
        the call, the plugin's pictures; no parameters, no presets, no sound, the clock stopped. */
    struct PainterHost final : stella::ui::ElementHost
    {
        juce::String label;
        juce::StringArray options;
        std::map<juce::String, juce::String> settings;
        std::uint32_t colourValue = 0;
        juce::File images;                              // gui/images
        std::map<std::string, juce::Image> pictures;   // read when asked for

        juce::Image pictureNamed (const std::string& name)
        {
            const auto file = juce::String::fromUTF8 (name.data(), (int) name.size()).trim();

            // A file name only: nothing outside the images folder.
            if (file.isEmpty() || file.containsAnyOf ("/\\:") || file.startsWithChar ('.') || images == juce::File())
                return {};

            auto found = pictures.find (name);

            if (found == pictures.end())
                found = pictures.emplace (name, juce::ImageFileFormat::loadFrom (images.getChildFile (file))).first;

            return found->second;
        }

        bool pictureSize (int, const std::string& name, int& width, int& height) override
        {
            const auto image = pictureNamed (name);
            width = image.getWidth();
            height = image.getHeight();
            return image.isValid();
        }

        bool readPicture (int, const std::string& name, int width, int height, std::uint32_t* destination) override
        {
            auto image = pictureNamed (name);

            if (! image.isValid() || width <= 0 || height <= 0 || destination == nullptr)
                return false;

            if (image.getWidth() != width || image.getHeight() != height)
                image = image.rescaled (width, height, juce::Graphics::highResamplingQuality);

            image = image.convertedToFormat (juce::Image::ARGB);
            const juce::Image::BitmapData data (image, juce::Image::BitmapData::readOnly);

            if (data.pixelStride != (int) sizeof (juce::PixelARGB))
                return false;

            for (int y = 0; y < height; ++y)
            {
                const auto* line = reinterpret_cast<const juce::PixelARGB*> (data.getLinePointer (y));

                for (int x = 0; x < width; ++x)
                    destination[(size_t) y * (size_t) width + (size_t) x] = line[x].getInARGBMaskOrder();
            }

            return true;
        }

        bool paramInfo (int, int, int, float&) override                  { return false; }
        int findSlot (int, const std::string&) override                  { return -1; }
        void setValue (int, int, float) override                         {}
        void gesture (int, int, bool) override                           {}

        bool text (int, int which, int index, const std::string& key, std::string& result) override
        {
            if (which == labelText && label.isNotEmpty())
            {
                result = label.toStdString();
                return true;
            }

            if (which == optionText && juce::isPositiveAndBelow (index, options.size()))
            {
                result = options[index].toStdString();
                return true;
            }

            if (which == settingText)
            {
                if (const auto found = settings.find (juce::String (key)); found != settings.end())
                {
                    result = found->second.toStdString();
                    return true;
                }
            }

            return false;
        }

        int numOptions (int) override                                    { return options.size(); }
        std::uint32_t colour (int) override                              { return colourValue; }
        float level (int, bool) override                                 { return 0.0f; }
        void readScope (int, float* destination, int n) override         { std::fill (destination, destination + juce::jmax (0, n), 0.0f); }
        void playNote (int, int, float) override                         {}
        bool isNoteDown (int) override                                   { return false; }
        double seconds() override                                        { return 0.0; }
    };

    /** A look whose shadow or glow falls past the control: how far, for the layout to leave room. */
    juce::String shadowNote (const Looks::Look& look)
    {
        const auto side = (float) juce::jmax (8, juce::jmin (look.doc.canvasWidth, look.doc.canvasHeight));
        const auto reach = LayerRender::overhangOf (look.doc);

        if (reach <= 0.0f)
            return {};

        return " [its shadow falls " + juce::String (juce::roundToInt (reach / side * 100.0f)) + "% of its size past the control: leave that room]";
    }

    /** A look's layers, bottom first, each with its material: "layers: Shadow, Bezel=anodized, Body=titanium, Pointer". */
    juce::String layersNote (const Looks::Look& look)
    {
        juce::StringArray names;

        for (const auto& layer : look.doc.layers)
            if (layer.visible)
                names.add (layer.name + (layer.prim.material.isNotEmpty() ? "=" + layer.prim.material : juce::String()));

        return names.isEmpty() ? juce::String() : " {layers: " + names.joinIntoString (", ") + "}";
    }

    /** Where a look's own surface is, for a material given without a layer: the layer called
        Body, Top, Cap, Face, Head or Knob, else the first with a material (-1: none). */
    int bodyLayerOf (const LayerDoc& doc)
    {
        for (const auto* wanted : { "body", "top", "cap", "face", "head", "knob" })
            for (size_t i = 0; i < doc.layers.size(); ++i)
                if (doc.layers[i].name.trim().equalsIgnoreCase (wanted))
                    return (int) i;

        for (size_t i = 0; i < doc.layers.size(); ++i)
            if (doc.layers[i].prim.material.isNotEmpty())
                return (int) i;

        return -1;
    }

    /** A layer's surface from make_look's settings: its material, finish, colours, texture and
        pattern. Empty, or what was wrong. described: what changed, in a few words. */
    juce::String setSurface (DocLayer& layer, const juce::var& settings, juce::StringArray& described)
    {
        using namespace MaterialChoices;
        auto& p = layer.prim;
        auto text = [&settings] (const char* key) { return settings.getProperty (key, {}).toString().trim(); };
        auto number = [&settings] (const char* key, float fallback, float low, float high)
        {
            return juce::jlimit (low, high, settings.hasProperty (key) ? (float) (double) settings.getProperty (key, fallback) : fallback);
        };

        if (settings.hasProperty ("material"))
        {
            const auto material = text ("material").toLowerCase();

            if (material.isEmpty() || material == "none" || material == "flat")
            {
                p.material.clear();
                p.materialFinish.clear();
                p.materialColour2 = {};
                described.add ("no material");
            }
            else
            {
                const auto all = allMaterials();
                const auto index = indexOf (all, material);

                if (index < 0)
                {
                    juce::StringArray names;

                    for (const auto& c : all)
                        names.add (c.name);

                    return "There's no material \"" + material + "\". The materials: " + names.joinIntoString (", ") + ".";
                }

                // A new material comes in its own colours and finish, as in the Knob Studio.
                p.material = all[(size_t) index].name;
                juce::Colour main, second;
                materialColours (p.material, main, second);
                p.colour = main;
                p.materialColour2 = {};
                p.materialFinish.clear();
                described.add (p.material);
            }
        }

        if (settings.hasProperty ("finish"))
        {
            const auto index = indexOf (finishChoices(), text ("finish"));

            if (index < 0)
                return "There's no finish \"" + text ("finish") + "\". The finishes: natural, polished, satin, brushed, hammered, sandblasted, matte, liquid.";

            p.materialFinish = index == 0 ? juce::String() : juce::String (finishChoices()[(size_t) index].name);
            described.add (finishChoices()[(size_t) index].name);
        }

        if (const auto colour = text ("color").isNotEmpty() ? text ("color") : text ("colour"); colour.isNotEmpty())
        {
            p.colour = GuiLayout::colourFromString (colour, p.colour);
            described.add ("colour " + colour);
        }

        if (const auto colour = text ("color2").isNotEmpty() ? text ("color2") : text ("colour2"); colour.isNotEmpty())
            p.materialColour2 = GuiLayout::colourFromString (colour, p.materialColour2);

        p.materialScale = number ("scale", p.materialScale, 0.25f, 4.0f);
        p.materialGrain = number ("grain", p.materialGrain, 0.0f, 2.0f);

        if (settings.hasProperty ("pattern"))
        {
            const auto index = indexOf (patternChoices(), text ("pattern"));

            if (index < 0)
            {
                juce::StringArray names;

                for (const auto& c : patternChoices())
                    names.add (c.name);

                return "There's no pattern \"" + text ("pattern") + "\". The patterns: " + names.joinIntoString (", ") + ".";
            }

            p.pattern = index == 0 ? juce::String() : juce::String (patternChoices()[(size_t) index].name);

            if (p.pattern.isNotEmpty())
                described.add (p.pattern + " pattern");
        }

        if (settings.hasProperty ("patternBlend"))
        {
            const auto index = indexOf (blendChoices(), text ("patternBlend"));

            if (index < 0)
                return "There's no pattern blend \"" + text ("patternBlend") + "\". The blends: normal, multiply, screen, overlay, softlight, "
                       "add, darken, lighten, emboss, deboss, engrave.";

            p.patternBlend = blendChoices()[(size_t) index].name;
        }

        if (const auto colour = text ("patternColor").isNotEmpty() ? text ("patternColor") : text ("patternColour"); colour.isNotEmpty())
            p.patternColour = GuiLayout::colourFromString (colour, p.patternColour);

        p.patternOpacity = number ("patternOpacity", p.patternOpacity, 0.0f, 100.0f);
        p.patternSize = number ("patternSize", p.patternSize, 2.0f, 200.0f);
        p.patternAngle = number ("patternAngle", p.patternAngle, -180.0f, 180.0f);
        return {};
    }

    /** Where a control wearing a look draws it, as the canvas does (a knob: the square above its caption). */
    juce::Rectangle<int> lookArea (const GuiWidget& w)
    {
        if (w.type == GuiWidget::Type::knob)
        {
            const auto size = juce::jmax (8, juce::jmin (w.bounds.getWidth(), w.bounds.getHeight() - GuiLayout::captionHeight));
            return { w.bounds.getX() + (w.bounds.getWidth() - size) / 2, w.bounds.getY(), size, size };
        }

        return w.bounds;
    }

    /** A picture's file name from what the AI asked for: letters, digits, - and _ only, .png. */
    juce::String pictureFileName (const juce::String& asked)
    {
        auto stem = asked.trim();

        if (stem.endsWithIgnoreCase (".png"))
            stem = stem.dropLastCharacters (4);

        stem = stem.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_").substring (0, 48);
        return stem.isEmpty() ? juce::String() : stem + ".png";
    }
}

//==============================================================================
ProjectTools::ProjectTools (Project& p, LivePreview& lp)
    : project (p), preview (lp)
{
    // Code brought for a plugin that was never created (Sonify closed before it was) isn't
    // for whatever opens next.
    waitingReferenceFolder().deleteRecursively();
}

ProjectTools::~ProjectTools()
{
    // Work still under way answers no one now.
    *alive = false;
    worker.removeAllJobs (true, 10000);
}

const juce::String& ProjectTools::getGuide()
{
    static const juce::String guide = [] {
        juce::String text = juce::String::fromUTF8 (StellaRuntimeData::stella_builder_guide_md, StellaRuntimeData::stella_builder_guide_mdSize);

        // The file's path title is for the project, not for the AI.
        if (text.startsWith ("<!--"))
            text = text.fromFirstOccurrenceOf ("-->", false, false).trimStart();

        text << "\n\n## stella_api.h\n```cpp\n"
             << juce::String::fromUTF8 (StellaRuntimeData::stella_api_h, StellaRuntimeData::stella_api_hSize)
             << "\n```\n"
             << "\n## stella_element_api.h (programmed GUI elements)\n```cpp\n"
             << juce::String::fromUTF8 (StellaRuntimeData::stella_element_api_h, StellaRuntimeData::stella_element_api_hSize)
             << "\n```\n";

        if (const auto skills = Skills::index (Skills::For::plugins); skills.isNotEmpty())
            text << "\n" << skills;

        return text;
    }();

    return guide;
}

//==============================================================================
juce::var ProjectTools::getDefinitions (bool builder) const
{
    juce::Array<juce::var> tools;

    tools.add (tool ("read_project",
                     "Shows the open project: its name and type, graph.json, its files, and the last build's result.",
                     object(), {}));

    {
        auto props = object();
        props.getDynamicObject()->setProperty ("path", property ("string", "graph.json, gui/layout.json, design.md, a file in modules/ or elements/ such as "
                                                                           "modules/LowPass.cpp, or a file of the user's own code in reference/ (read only)"));
        props.getDynamicObject()->setProperty ("from", property ("integer", "For a file in reference/: the first line to read (default 1)."));
        props.getDynamicObject()->setProperty ("lines", property ("integer", "For a file in reference/: how many lines (default and most: 400). A long file is "
                                                                            "read a part at a time."));
        tools.add (tool ("read_file", "Reads one of the project's files.", props, { "path" }));
    }

    {
        auto props = object();
        props.getDynamicObject()->setProperty ("text", property ("string", "What to look for (any case): a function's, class's or parameter's name, a word..."));
        tools.add (tool ("search_reference",
                         "Finds a text in all of the user's own code (reference/, which the project's description lists): the files it's in the name of, "
                         "and each file and line it's on.",
                         props, { "text" }));
    }

    {
        auto props = object();
        props.getDynamicObject()->setProperty ("layout", property ("object", "The whole GUI: { \"format\": 1, \"width\", \"height\", \"background\", \"light\", \"widgets\": [...] }"));
        tools.add (tool ("set_layout",
                         "Replaces the whole GUI (gui/layout.json), for a new design. It shows at once; no build needed. "
                         "To change part of it, use edit_layout instead. If you do replace it, read gui/layout.json just before, "
                         "to keep what the user arranged by hand. Toolbox types (a drop-down list is \"dropdown\"; RangeSlider, DualKnob and MeterKnob are built in) and the project's "
                         "programmed elements only: an element the toolbox doesn't have is programmed first, never replaced by a toolbox type.",
                         props, { "layout" }));
    }

    {
        auto props = object();
        props.getDynamicObject()->setProperty ("remove", listProperty ("integer", "Indexes of elements to delete."));
        props.getDynamicObject()->setProperty ("change", listProperty ("object", "Elements to change: { \"index\": 3, then the fields to set, named as in layout.json, "
                                                                                  "e.g. \"x\": 40, \"label\": \"Cutoff\", \"style\": \"Black knob\" }. Fields not given stay as they are."));
        props.getDynamicObject()->setProperty ("add", listProperty ("object", "New elements, written as in layout.json, e.g. { \"type\": \"keyboard\", \"x\": 20, \"y\": 330, "
                                                                               "\"w\": 720, \"h\": 80, \"low\": \"C2\", \"high\": \"C6\" }, a drop-down list "
                                                                               "{ \"type\": \"dropdown\", \"param\": \"filter.type\", \"options\": [...], ... }, or a programmed one "
                                                                               "{ \"type\": \"custom\", \"element\": \"<Name>\", ... }. They go in front of the others; "
                                                                               "\"at\": 0 puts one at the back."));
        props.getDynamicObject()->setProperty ("width", property ("integer", "The window's new width."));
        props.getDynamicObject()->setProperty ("height", property ("integer", "The window's new height."));
        props.getDynamicObject()->setProperty ("background", property ("object", "The window's colours: { \"top\": \"#FF2B2B30\", \"bottom\": \"#FF17171A\" }."));
        props.getDynamicObject()->setProperty ("pages", listProperty ("string", "The window's pages in order, replacing the list, e.g. [\"Main\", \"Effects\"] "
                                                                               "([] for one page). Elements say their page with \"page\"; ones on a page "
                                                                               "that's gone move to the first. A \"tabs\" element switches them."));
        props.getDynamicObject()->setProperty ("light", property ("object", "The panel's global light, which lights the knobs' looks, the shapes and painted "
                                                                           "backgrounds alike: { \"angle\": 120, \"altitude\": 30 } (Photoshop's: degrees it comes "
                                                                           "from, 120 the upper left; how high, 0..90). Painted backgrounds are painted again in it."));
        tools.add (tool ("edit_layout",
                         "Changes part of the plugin's GUI and keeps everything else exactly as it is: removes, changes or adds elements, "
                         "or resizes the window. Elements are named by their index in the project's \"GUI elements\" list; one call's "
                         "changes and removals all use that list, then its additions go in. Put all of a step's GUI edits in one call: the "
                         "indexes change after it. The window grows by itself to fit what's added or moved. It shows at once; no build needed. "
                         "Toolbox types (a drop-down list is \"dropdown\"; RangeSlider, DualKnob and MeterKnob are built in) and the project's programmed elements only: an element the "
                         "toolbox doesn't have is programmed first, never replaced by a toolbox type.",
                         props, {}));
    }

    {
        auto props = object();
        props.getDynamicObject()->setProperty ("name", property ("string", "The preset's name, e.g. \"Warm Pad\". A preset with that name is replaced."));
        props.getDynamicObject()->setProperty ("values", property ("object", "Parameter values by id, e.g. { \"filter.cutoff\": 800 }. Parameters left out keep their defaults."));
        tools.add (tool ("save_preset",
                         "Saves a named preset: a set of parameter values the user picks from the preset list, also in the plugin added to Sonify.",
                         props, { "name", "values" }));
    }

    {
        auto props = object();
        props.getDynamicObject()->setProperty ("name", property ("string", "A short name for the plugin, from the request, e.g. \"Ice Synth\"."));
        tools.add (tool ("create_project",
                         "Creates the new plugin the user is starting, of the type they chose (the project's description says which: it's "
                         "never yours to pick), and opens it, ready to build. Only when no project is open. Don't ask the user to do it.",
                         props, { "name" }));
    }

    {
        // Handled by the conversation itself (StellaAi): the plan and its price, for the user to approve.
        auto stepItem = object();
        stepItem.getDynamicObject()->setProperty ("type", "object");
        auto stepProps = object();
        stepProps.getDynamicObject()->setProperty ("title", property ("string", "A few plain words in the user's language, e.g. \"Filter sound\" (no file or code names)."));
        stepProps.getDynamicObject()->setProperty ("builds", property ("string", "What this step builds, in one short plain sentence for a musician."));
        stepProps.getDynamicObject()->setProperty ("size", property ("string", "\"small\", \"medium\" or \"large\", as the guide's \"Plans and prices\" defines them."));
        stepItem.getDynamicObject()->setProperty ("properties", stepProps);
        stepItem.getDynamicObject()->setProperty ("required", juce::Array<juce::var> { "title", "builds", "size" });

        auto phaseItem = object();
        phaseItem.getDynamicObject()->setProperty ("type", "object");
        auto phaseProps = object();
        phaseProps.getDynamicObject()->setProperty ("title", property ("string", "The phase in a few plain words."));
        phaseProps.getDynamicObject()->setProperty ("builds", property ("string", "What it adds, in one short plain sentence."));
        phaseProps.getDynamicObject()->setProperty ("steps", property ("integer", "How many steps it will take (1 to 12)."));
        phaseProps.getDynamicObject()->setProperty ("size", property ("string", "Its typical step: \"small\", \"medium\" or \"large\"."));
        phaseItem.getDynamicObject()->setProperty ("properties", phaseProps);
        phaseItem.getDynamicObject()->setProperty ("required", juce::Array<juce::var> { "title", "builds", "steps", "size" });

        auto props = object();
        props.getDynamicObject()->setProperty ("summary", property ("string", "What the user gets, honestly, in one or two plain sentences in their language."));

        auto stepsList = property ("array", "This plan's steps in order, at most 12: one file per step, then build and fix, then the panel.");
        stepsList.getDynamicObject()->setProperty ("items", stepItem);
        props.getDynamicObject()->setProperty ("steps", stepsList);

        auto laterList = property ("array", "Only for a request too big for 12 steps: the phases that come after this plan, in order.");
        laterList.getDynamicObject()->setProperty ("items", phaseItem);
        props.getDynamicObject()->setProperty ("later", laterList);

        tools.add (tool ("propose_plan",
                         "Shows the user your plan for this request with its price in credits, and waits for them to start it, change "
                         "it or cancel it. Every request that changes the plugin (sound, panel or presets) needs an approved plan: "
                         "nothing changes before. Call it as soon as you know the job (read the project first if you need to), and "
                         "write nothing else in that answer.",
                         props, { "summary", "steps" }));
    }

    {
        // Handled by the conversation itself (StellaAi): the checklist ticks along.
        auto props = object();
        props.getDynamicObject()->setProperty ("current", property ("integer", "The step starting now, counting from 1."));
        props.getDynamicObject()->setProperty ("done", property ("boolean", "true when the last step is finished."));
        tools.add (tool ("plan",
                         "Moves the approved plan's checklist along: call it with current as each step starts (in the same answer as "
                         "that step's own tool calls), then with done: true after the last step. The project's description shows an "
                         "unfinished plan: carry on from it.",
                         props, {}));
    }

    {
        // In both lists: a conversation hands over with it, and the builder must still
        // recognise that step in the request it takes over.
        auto props = object();
        props.getDynamicObject()->setProperty ("task", property ("string", "What to build or change, in a few sentences."));
        tools.add (tool ("start_building",
                         builder ? "Already building: there's no need to call this."
                                 : "Hands over to the builder to create or change the plugin's sound: anything that needs code written or "
                                   "changed (a new kind of GUI element the toolbox doesn't have, too), or the graph rewired. Call it as soon as "
                                   "the user asks for a plugin or a change to one: the builder sizes the job and shows its plan and price.",
                         props, { "task" }));
    }

    tools.add (Skills::toolDefinition (Skills::For::plugins));

    {
        auto notes = listProperty ("string", "The notes played together, as names (\"C3\", \"F#2\", \"Bb4\"; C4 is middle C) or MIDI numbers. "
                                             "Default [\"C3\"]. One note measures pitch and aliasing too; a chord shows how voices add up.");
        auto signal = property ("string", "Effects and widgets: the test signal. \"music\" (drums, bass and a chord; the default), \"drums\" "
                                          "(dynamics, transients), \"chord\" (tone, chorus and modulation), \"noise\" (filters and EQ), "
                                          "\"impulse\" (reverb and delay tails), \"sine\" (distortion: harmonics and aliasing).");
        juce::Array<juce::var> signalNames;

        for (const auto& name : SoundMeasure::signalNames())
            signalNames.add (juce::String (name));

        signal.getDynamicObject()->setProperty ("enum", signalNames);

        auto props = object();
        props.getDynamicObject()->setProperty ("notes", notes);
        props.getDynamicObject()->setProperty ("velocity", property ("number", "0..1, default 0.8 (the same notes are played softly too, for velocity)."));
        props.getDynamicObject()->setProperty ("pitched", property ("boolean", "false when the notes are triggers rather than pitches (a drum kit: 36 kick, "
                                                                               "38 snare, 42 hat...): no tuning check. Default true."));
        props.getDynamicObject()->setProperty ("hold", property ("number", "Seconds the notes (or the signal) last: default 1.2, at most 8."));
        props.getDynamicObject()->setProperty ("tail", property ("number", "Seconds measured after they end: default 1.5, at most 8 (longer for long releases and reverbs)."));
        props.getDynamicObject()->setProperty ("signal", signal);
        props.getDynamicObject()->setProperty ("values", property ("object", "Parameter values to try for this test only, by id, e.g. { \"filter.cutoff\": 900 }: "
                                                                             "the plugin itself doesn't change. Leave it out to hear it as it is."));
        tools.add (tool ("analyze_sound",
                         "Listens to the plugin as it plays now and measures it: a copy plays a test offline (the user hears nothing), "
                         "notes for an instrument or a test signal for an effect or widget. Returns its level and peaks, tone and "
                         "brightness, envelope, movement while held, pitch and aliasing, stereo image and clicks, and for an effect what "
                         "it did to the signal (tone, width, tail, distortion), then what's worth fixing. Use it to check a sound after "
                         "building it and to act on words like warmer, punchier or wider (the sound-critic skill says how). Changes nothing.",
                         props, {}));
    }

    {
        auto props = object();
        props.getDynamicObject()->setProperty ("element", property ("string", "The painter: \"PanelPainter\" (built in: a whole lit panel from its settings, nothing to write), "
                                                                              "or a programmed element of yours whose paint() draws the whole picture."));
        props.getDynamicObject()->setProperty ("file", property ("string", "The picture's name, e.g. \"background\": saved as gui/images/<file>.png, replacing "
                                                                           "a picture you painted before under that name (never one of the user's)."));
        props.getDynamicObject()->setProperty ("width", property ("integer", "Its width in pixels: a background is the window's width."));
        props.getDynamicObject()->setProperty ("height", property ("integer", "Its height in pixels."));
        props.getDynamicObject()->setProperty ("settings", property ("object", "Passed to the element as its \"settings\" (read with setting() and settingText()), "
                                                                               "e.g. { \"seed\": 7, \"grain\": 0.4 }: one painter, many pictures."));
        props.getDynamicObject()->setProperty ("color", property ("string", "Its colour() as \"#AARRGGBB\" (optional)."));
        tools.add (tool ("paint_picture",
                         "Paints a picture for the panel and saves it in gui/images as a PNG: a background, a plate, a screen, a logo. "
                         "PanelPainter, built in, paints a whole panel lit by one light from its settings, in any of 38 materials (metals "
                         "from brushed aluminium to gold and liquid titanium, marble and other stone, plastics, wood, leather, glass...) "
                         "with a finish and Photoshop-style pattern layers (diamond plate, knurl, guilloche, hexagons... blended, embossed "
                         "or engraved), and sunk sections, raised plates, screens, wells for knobs, grooves, vents, engraved words: nothing "
                         "to write. To change a painted picture, repaint it from its recipe (the project's painted pictures list it), "
                         "changing only what was asked. For anything else, a programmed element you wrote draws the picture in its "
                         "paint(), once, with up to 10 seconds to do it: write it, build, then paint. The background-painter skill has "
                         "the settings and the recipes. Then place the picture with edit_layout or set_layout (a background is an image "
                         "element covering the window, first in the list).",
                         props, { "element", "file", "width", "height" }));
    }

    {
        auto surface = [] (const juce::String& what)
        {
            return property ("object", what + ": { \"material\" (one of the materials, or \"none\"), \"finish\" (natural, polished, satin, "
                                              "brushed, hammered, sandblasted, matte, liquid), \"color\" and \"color2\" (\"#AARRGGBB\"; a new "
                                              "material comes in its own colours), \"scale\" (0.25..4) and \"grain\" (0..2) of its texture, "
                                              "\"pattern\" with \"patternBlend\", \"patternColor\", \"patternOpacity\" (0..100), "
                                              "\"patternSize\" (pixels), \"patternAngle\" }; only what's given changes.");
        };

        auto props = object();
        props.getDynamicObject()->setProperty ("name", property ("string", "The new look's name, e.g. \"Brass knob\": controls wear it by this name. "
                                                                           "A look of this plugin's own with that name is replaced."));
        props.getDynamicObject()->setProperty ("from", property ("string", "The look it starts from (from the project's looks list): its shape, layers, "
                                                                           "pointer and shadow. Left out: the kind's default look."));
        props.getDynamicObject()->setProperty ("kind", property ("string", "\"knob\", \"slider\", \"hslider\" (a slider across) or \"switch\": "
                                                                           "only when from is left out."));
        props.getDynamicObject()->setProperty ("description", property ("string", "A few words for the looks list, e.g. \"polished brass with a black pointer\"."));
        props.getDynamicObject()->setProperty ("surface", surface ("The look's own surface (its Body, Top, Cap, Face or Head layer)"));
        props.getDynamicObject()->setProperty ("layers", property ("object", "Other layers by name (the looks list shows each look's layers), each as surface "
                                                                             "is, e.g. { \"Bezel\": { \"material\": \"anodized\", \"color\": "
                                                                             "\"#FF202024\" }, \"Pointer\": { \"color\": \"#FFFFFFFF\" } }."));
        tools.add (tool ("make_look",
                         "Makes a look for knobs, sliders or switches from one in the project's looks list, in a material: any of the 38 "
                         "materials (silver, chrome, gold, brass, copper, liquid titanium, anodized, marble, granite, plastics, bakelite, "
                         "woods, carbon, glass, pearl...) with a finish, colours and a pattern (knurl, guilloche, hexagons... blended, "
                         "embossed or engraved), on its surface and on any of its layers (a bezel, the pointer). Lit by the panel's "
                         "global light; it keeps the light where it is as the knob turns. It's saved as the plugin's own look: put it on "
                         "controls with their \"style\" (edit_layout). Use it when the user asks for knobs in a material or a colour "
                         "the looks don't have.",
                         props, { "name" }));
    }

    // The KnobMan gallery: knobs, sliders and switches made by its community, the ones whose
    // licence allows them in a plugin that's sold. Looked through first, before designing a look.
    {
        auto props = object();
        auto kind = property ("string", "\"knob\", \"slider\" (either way up) or \"switch\".");
        kind.getDynamicObject()->setProperty ("enum", juce::Array<juce::var> { "knob", "slider", "switch" });
        props.getDynamicObject()->setProperty ("kind", kind);
        props.getDynamicObject()->setProperty ("words", property ("string", "What to look for in their names, tags and notes, e.g. \"metal vintage\", "
                                                                            "\"black chicken head\", \"led ring\". Left out: the newest."));
        tools.add (tool ("find_gallery_looks",
                         "Looks through the KnobMan gallery on this computer for knobs, sliders or switches whose licence allows them "
                         "in a plugin that's sold: up to 20, each with its id, name, tags, note, author and licence. Use it first when "
                         "the panel needs knobs (or sliders, switches), before designing a look: pick the item whose name, tags and "
                         "note suit the plugin's character, then use_gallery_look. Changes nothing.",
                         props, { "kind" }));
    }

    {
        auto props = object();
        props.getDynamicObject()->setProperty ("id", property ("integer", "The gallery item's id (from find_gallery_looks)."));
        props.getDynamicObject()->setProperty ("name", property ("string", "The look's name in this plugin, e.g. \"Vintage knob\" (left out: the item's own)."));
        tools.add (tool ("use_gallery_look",
                         "Brings a gallery item (from find_gallery_looks) into the plugin as its own look, drawn exactly as KnobMan "
                         "draws it, its author and licence kept with it. Then put it on controls with their \"style\" (edit_layout).",
                         props, { "id" }));
    }

    if (! builder)
        return tools;

    {
        auto props = object();
        props.getDynamicObject()->setProperty ("path", property ("string", "modules/<Name>.cpp for a module, modules/<Name>.h for a shared helper, "
                                                                           "elements/<Name>.cpp for a programmed GUI element, or design.md for the "
                                                                           "plugin's design brief"));
        props.getDynamicObject()->setProperty ("content", property ("string", "The complete file."));
        tools.add (tool ("write_file", "Creates or replaces a file in modules/ or elements/, or design.md (the design brief: it needs no plan). "
                                       "Always give the complete file.", props, { "path", "content" }));
    }

    {
        auto props = object();
        props.getDynamicObject()->setProperty ("path", property ("string", "A file in modules/ or elements/"));
        tools.add (tool ("delete_file", "Deletes a file in modules/ or elements/ that the plugin no longer uses.", props, { "path" }));
    }

    {
        auto props = object();
        props.getDynamicObject()->setProperty ("graph", property ("object", "The whole graph: { \"format\": 1, \"modules\": [...], \"wires\": [...] }"));
        tools.add (tool ("set_graph", "Replaces graph.json: which modules the plugin has and how they're wired.", props, { "graph" }));
    }

    tools.add (tool ("build",
                     "Compiles the project (its modules, and its programmed GUI elements) and plays it live in the studio. Returns the plugin's "
                     "parameters, or the compiler's errors to fix.",
                     object(), {}));

    return tools;
}

//==============================================================================
juce::String ProjectTools::describeProject() const
{
    if (! project.isOpen())
    {
        if (! chosenKind.has_value())
            return "No project is open, and the user hasn't chosen what they're building yet. Don't create anything: ask them in one "
                   "sentence to choose the type beside Send (Instrument, Effect, MIDI FX or Widget), then describe it.";

        auto text = "No project is open. The user is starting a new " + Project::kindDisplayName (*chosenKind) + ": they chose that type "
                    "beside Send, so it's exactly what you build (never another type). " + kindRole (*chosenKind)
                  + "\nCreate it with create_project (a short name from the request), then build it as the guide says.";

        if (const auto code = getReference().listing (maxListedReference); code.isNotEmpty())
            text << "\n\nThe user brought their own code (reference/, read only; it goes into the project you create). Read it "
                    "(read_file, search_reference) and port it as the guide's \"The user's own code\" says:\n" << code;

        return text;
    }

    const auto& info = project.getInfo();
    juce::String text;

    text << "Project \"" << info.name << "\" (" << Project::kindDisplayName (info.kind) << ") by " << info.vendor << ".\n"
         << kindRole (info.kind) << "\n"
         << "Its type is the user's choice: never change it. If they ask for something of another type (a MIDI FX while an "
            "instrument is open, say), tell them in one sentence to start a new plugin and choose its type beside Send.\n";

    // The design brief: the plugin's memory, since conversations aren't kept.
    if (const auto design = project.getFolder().getChildFile (designFileName); design.existsAsFile())
    {
        auto brief = design.loadFileAsString().trim();

        if (brief.length() > maxDesignChars)
            brief = brief.substring (0, maxDesignChars) + "\n... (read design.md for the rest)";

        text << "\nThe plugin's design brief (design.md): build what it says, and when the user changes direction, write it "
                "again with the change in, in the same step:\n" << brief << "\n";
    }
    else if (! hasBuiltOnce())
    {
        text << "\nNo design brief yet (design.md): a new plugin's design comes first (see \"The design brief\").\n";
    }

    const auto graph = project.getFolder().getChildFile (WasmCompiler::graphFileName);
    text << "\ngraph.json:\n" << (graph.existsAsFile() ? graph.loadFileAsString().trim() : juce::String ("(none yet)")) << "\n";

    // The sample library a sampler plays (stella::blob): the user's, in Options > Sample library...
    if (const auto library = PluginLibrary::load (project.getFolder()); library.isEmpty())
    {
        text << "\nSample library: empty (the user adds blobs in Options > Sample library...).\n";
    }
    else
    {
        text << "\nSample library (stella::blob, by number):\n";

        for (int i = 0; i < library.size(); ++i)
            text << "- " << i << ": " << library[i].name << " (" << PluginLibrary::describe (library[i].file) << ")\n";
    }

    auto files = project.getModulesFolder().findChildFiles (juce::File::findFiles, false, "*.cpp;*.h");
    files.sort();

    text << "\nFiles in modules/:\n";

    if (files.isEmpty())
        text << "(none)\n";

    for (const auto& f : files)
        text << "- modules/" << f.getFileName() << " (" << countLines (f.loadFileAsString()) << " lines)\n";

    // Programmed GUI elements: their files, and the elements they make.
    auto elementFiles = project.getFolder().getChildFile (WasmCompiler::elementsFolderName).findChildFiles (juce::File::findFiles, false, "*.cpp;*.h");
    elementFiles.sort();

    if (! elementFiles.isEmpty())
    {
        text << "\nFiles in elements/:\n";

        for (const auto& f : elementFiles)
            text << "- elements/" << f.getFileName() << " (" << countLines (f.loadFileAsString()) << " lines)\n";
    }

    // The user's own code: read as it's needed, never sent whole.
    if (const auto code = getReference().listing (maxListedReference); code.isNotEmpty())
        text << "\nThe user's own code (reference/, read only: read_file a part at a time, search_reference across it; port it as the guide's "
                "\"The user's own code\" says):\n" << code;

    const auto programmed = WasmCompiler::findElements (project.getFolder());

    text << "\nProgrammed elements (\"type\": \"custom\", \"element\": <name>; use them before writing new ones):";

    if (programmed.isEmpty())
        text << " none yet.\n";
    else
        text << "\n";

    for (const auto& e : programmed)
        text << "- " << e.name << ": " << e.description << " (" << e.file << ")\n";

    const auto layoutFile = project.getGuiFolder().getChildFile (GuiLayout::fileName);
    GuiLayout layout;

    if (GuiLayout::load (layoutFile, layout).wasOk())
    {
        text << "\nGUI: gui/layout.json, " << layout.width << " x " << layout.height << ", " << (int) layout.widgets.size() << " elements"
             << (layout.backgroundPicture().isNotEmpty() ? ", background picture " + layout.backgroundPicture() : juce::String())
             << (layout.pages.isEmpty() ? juce::String() : ", pages: " + layout.pages.joinIntoString (", "))
             << ", global light from " << juce::String (layout.lightAngle, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".")
             << " degrees, " << juce::String (layout.lightAltitude, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") << " up.\n";

        // Every element, by index: edit_layout changes them without the whole file going back and forth.
        if (! layout.widgets.empty())
        {
            juce::StringArray paramIds;

            for (const auto& p : preview.getParameters())
                paramIds.add (p.id);

            text << "GUI elements, back to front (edit_layout names them by these indexes):\n";

            const auto listed = juce::jmin ((int) layout.widgets.size(), maxListedElements);

            for (int i = 0; i < listed; ++i)
                text << elementLine (i, layout.widgets[(size_t) i], paramIds) << "\n";

            if (listed < (int) layout.widgets.size())
                text << "... and " << ((int) layout.widgets.size() - listed) << " more (read gui/layout.json for all of them).\n";
        }
    }
    else
    {
        text << "\nGUI: none yet (the studio makes a plain automatic one after the first build).\n";
    }

    // The pictures the user added, for backgrounds and picture elements.
    auto pictures = project.getGuiFolder().getChildFile (GuiLayout::imagesFolder)
                        .findChildFiles (juce::File::findFiles, false, GuiLayout::pictureFiles());
    pictures.sort();

    if (! pictures.isEmpty())
    {
        const auto painted = GuiLayout::paintedPictures (project.getGuiFolder());
        text << "\nPictures in gui/images (use them by file name; \"painted by you\" ones you may repaint, the others are the user's):\n";

        for (const auto& f : pictures)
        {
            const auto size = pictureSize (f);
            text << "- " << f.getFileName() << " (" << size.x << " x " << size.y << ")"
                 << (painted.contains (f.getFileName()) ? " painted by you" : "") << "\n";

            // How it was painted (the user may have changed its material in the Edit tab since):
            // to change it, paint again from these settings, changing only what's asked.
            if (const auto recipe = GuiLayout::paintedRecipe (project.getGuiFolder(), f.getFileName()); recipe.isObject())
            {
                auto line = "{ \"element\": " + juce::JSON::toString (recipe.getProperty ("element", {}), true)
                          + (recipe.hasProperty ("color") ? ", \"color\": " + juce::JSON::toString (recipe.getProperty ("color", {}), true) : juce::String())
                          + ", \"settings\": " + juce::JSON::toString (recipe.getProperty ("settings", {}), true) + " }";

                if (line.length() > 2500)
                    line = line.substring (0, 2500) + " [... cut]";

                text << "  painted with " << line << "\n";
            }
        }
    }

    // The looks knobs, sliders and switches can wear.
    {
        Looks looks;
        looks.setGuiFolder (project.getGuiFolder());

        text << "\nLooks for knobs, sliders and switches (a control's \"style\"; without one it wears its kind's default):\n";

        auto kindName = [] (Looks::Kind kind)
        {
            return kind == Looks::Kind::knob         ? juce::String ("knobs")
                 : kind == Looks::Kind::slider       ? juce::String ("sliders")
                 : kind == Looks::Kind::sliderAcross ? juce::String ("sliders across, \"orientation\": \"horizontal\"")
                                                     : juce::String ("switches");
        };

        juce::StringArray poolLines;

        for (const auto kind : { Looks::Kind::knob, Looks::Kind::slider, Looks::Kind::sliderAcross, Looks::Kind::button })
        {
            juce::StringArray entries, pool;

            for (const auto* look : looks.listFor (kind))
            {
                // The inspiration pool's: by name only (the inspiration-looks skill has each one's
                // character, layers and the panels it suits), so every request stays small.
                if (look->origin == Looks::Origin::builtIn && look->pool)
                {
                    pool.add (look->name);
                    continue;
                }

                entries.add (look->name
                             + (look->description.isNotEmpty() ? " (" + look->description + ")" : juce::String())
                             + layersNote (*look)
                             + shadowNote (*look)
                             + (look->origin == Looks::Origin::project ? juce::String (" [this plugin's own]")
                                : look->origin == Looks::Origin::mine  ? juce::String (" [the user's]") : juce::String()));
            }

            text << "- " << kindName (kind) << " (default " << looks.defaultFor (kind).name << "): " << entries.joinIntoString ("; ") << "\n";

            if (! pool.isEmpty())
                poolLines.add ("- " + kindName (kind) + ": " + pool.joinIntoString (", "));
        }

        if (! poolLines.isEmpty())
            text << "Looks of your inspiration pool, worn the same way (read the inspiration-looks skill for what each is like):\n"
                 << poolLines.joinIntoString ("\n") << "\n";

        // The KnobMan gallery on this computer: what find_gallery_looks can offer.
        int knobs = 0, sliders = 0, switches = 0;

        for (const auto& item : GalleryLibrary::loadIndex())
        {
            if (! item.isCommercialSafe() || ! GalleryLibrary::knobFile (item.id).existsAsFile())
                continue;

            const auto category = item.category();
            knobs += category == "$knob" ? 1 : 0;
            sliders += category == "$slider" ? 1 : 0;
            switches += category == "$switch" ? 1 : 0;
        }

        if (knobs + sliders + switches > 0)
            text << "The KnobMan gallery on this computer (find_gallery_looks, use_gallery_look; licences that allow selling only): "
                 << knobs << " knobs, " << sliders << " sliders, " << switches << " switches.\n";
        else
            text << "The KnobMan gallery isn't downloaded on this computer: design looks from the list above (make_look).\n";
    }

    PresetBank bank;
    bank.load (project.getFolder());

    if (! bank.presets.empty())
        text << "\nPresets: " << bank.getNames().joinIntoString (", ") << "\n";

    if (! preview.getParameters().isEmpty())
    {
        text << "\nParameters (bind GUI controls to these ids):\n";

        for (const auto& p : preview.getParameters())
            text << "- " << p.id << " \"" << p.name << "\" " << juce::String (p.min, 3) << ".." << juce::String (p.max, 3)
                 << (p.unit.isNotEmpty() ? " " + p.unit : juce::String()) << "\n";
    }

    if (! preview.getModules().isEmpty())
    {
        juce::StringArray signals { "plugin.out L", "plugin.out R" };

        if (info.kind == PluginKind::effect || info.kind == PluginKind::widget)
            signals.addArray ({ "plugin.in L", "plugin.in R" });

        for (const auto& m : preview.getModules())
            for (const auto& out : m.outputs)
                signals.add (m.id + "." + out);

        text << "\nSignals meters and scopes can watch: " << signals.joinIntoString (", ") << "\n";

        juce::StringArray displays;

        for (const auto& d : preview.getDisplays())
            displays.add (d.id);

        if (! displays.isEmpty())
            text << "Module displays (0..1 for meters, or lamps): " << displays.joinIntoString (", ") << "\n";
    }

    text << "\nLast build: ";

    switch (preview.getState())
    {
        case LivePreview::State::idle:      text << "none yet.\n"; break;
        case LivePreview::State::building:  text << "building now.\n"; break;
        case LivePreview::State::playing:   text << "playing live (" << preview.getStatus() << ").\n"; break;
        case LivePreview::State::failed:    text << "failed: " << preview.getStatus() << "\n" << preview.getLog().substring (0, 1500) << "\n"; break;
    }

    if (const auto problem = preview.getElementsProblem(); problem.isNotEmpty())
        text << "The GUI's programmed elements stopped while running (" << problem.substring (0, 600)
             << "). Fix the element's code (out-of-range indexes, endless loops) and build again.\n";

    return text;
}

//==============================================================================
bool ProjectTools::hasDesign() const
{
    return project.isOpen() && project.getFolder().getChildFile (designFileName).existsAsFile();
}

juce::File ProjectTools::waitingReferenceFolder()
{
    return Settings::getCacheFolder().getChildFile ("Code waiting for a project");
}

CodeReference ProjectTools::getReference() const
{
    CodeReference waiting (waitingReferenceFolder());

    if (! project.isOpen())
        return waiting;

    // Code brought before the project was created goes into it.
    const auto folder = project.getFolder().getChildFile (referenceFolderName);

    if (waitingReferenceFolder().isDirectory())
        waiting.moveInto (folder);

    return CodeReference (folder);
}

CodeReference::Added ProjectTools::addReference (const juce::Array<juce::File>& sources)
{
    return getReference().add (sources);
}

juce::Result ProjectTools::resolve (const juce::String& path, juce::File& file, bool forWriting) const
{
    if (! project.isOpen())
        return juce::Result::fail ("No project is open: the user has to create one (New) or open one first.");

    const auto clean = path.trim().replaceCharacter ('\\', '/');

    if (clean.startsWith (juce::String (referenceFolderName) + "/"))
        return juce::Result::fail (forWriting ? "reference/ is the user's own code: it's read only. Port what the plugin needs into modules/ "
                                                "(and elements/), as the guide's \"The user's own code\" says."
                                              : "Read the user's code with read_file (it reads reference/ files a part at a time).");

    if (clean == WasmCompiler::graphFileName && ! forWriting)
    {
        file = project.getFolder().getChildFile (WasmCompiler::graphFileName);
        return juce::Result::ok();
    }

    if (clean == juce::String ("gui/") + GuiLayout::fileName && ! forWriting)
    {
        file = project.getGuiFolder().getChildFile (GuiLayout::fileName);
        return juce::Result::ok();
    }

    if (clean.equalsIgnoreCase (designFileName))
    {
        file = project.getFolder().getChildFile (designFileName);
        return juce::Result::ok();
    }

    // Only modules/<identifier>.cpp or .h, or elements/<identifier>.cpp or .h: nothing outside
    // the project's modules and elements folders.
    for (const auto* folderName : { "modules", WasmCompiler::elementsFolderName })
    {
        const auto prefix = juce::String (folderName) + "/";

        if (! clean.startsWith (prefix))
            continue;

        const auto name = clean.fromFirstOccurrenceOf (prefix, false, false);
        const auto stem = name.upToLastOccurrenceOf (".", false, false);
        const auto extension = name.fromLastOccurrenceOf (".", false, false);

        if (isIdentifier (stem) && (extension == "cpp" || extension == "h"))
        {
            file = project.getFolder().getChildFile (folderName).getChildFile (name);
            return juce::Result::ok();
        }
    }

    return juce::Result::fail ("\"" + path + "\" isn't allowed: use graph.json or gui/layout.json (read only; write them with "
                               "set_graph and set_layout), modules/<Name>.cpp / modules/<Name>.h, elements/<Name>.cpp / elements/<Name>.h, "
                               "or design.md (the plugin's design brief).");
}

juce::String ProjectTools::describeCall (const juce::String& name, const juce::var& input)
{
    // In plain words: the user sees what's happening, not how it's made.
    const auto path = input.getProperty ("path", {}).toString();

    if (name == "write_file")
    {
        if (path.trim().equalsIgnoreCase (designFileName))
            return "Designing the plugin";

        // modules/StereoWidener.cpp: "Making the stereo widener"; elements/DropDown.cpp: "Making
        // the drop down for the panel". Headers go by quietly.
        const bool element = path.startsWith (juce::String (WasmCompiler::elementsFolderName) + "/");

        if ((! path.startsWith ("modules/") && ! element) || path.endsWithIgnoreCase (".h"))
            return {};

        const auto stem = path.fromLastOccurrenceOf ("/", false, false).upToLastOccurrenceOf (".", false, false);
        juce::String words;

        for (int i = 0; i < stem.length(); ++i)
        {
            const auto c = stem[i];

            if (c == '_' || c == '-')
            {
                words << ' ';
                continue;
            }

            if (i > 0 && juce::CharacterFunctions::isUpperCase (c) && ! juce::CharacterFunctions::isUpperCase (stem[i - 1]))
                words << ' ';

            words << juce::String::charToString (juce::CharacterFunctions::toLowerCase (c));
        }

        return "Making the " + words.trim() + (element ? " for the panel" : "");
    }

    if (name == "delete_file")
        return "Tidying up";

    if (name == "read_file" && path.trim().replaceCharacter ('\\', '/').startsWith (juce::String (referenceFolderName) + "/"))
        return "Reading your code";

    if (name == "search_reference")
        return "Looking through your code";

    if (name == "set_graph")
        return "Connecting everything";

    if (name == "build")
        return juce::String::fromUTF8 ("Building\xe2\x80\xa6");

    if (name == "read_project")
        return "Looking at your plugin";

    if (name == "save_preset")
        return "Saving the preset \"" + input.getProperty ("name", {}).toString() + "\"";

    if (name == "create_project")
        return "Creating the project \"" + input.getProperty ("name", {}).toString() + "\"";

    if (name == "set_layout" || name == "edit_layout")
        return "Designing the panel";

    if (name == "read_skill")
        return "Reading up on " + Skills::titleOf (input.getProperty ("name", {}).toString());

    if (name == "analyze_sound")
        return juce::String::fromUTF8 ("Listening to it\xe2\x80\xa6");

    if (name == "paint_picture")
        return "Painting " + (input.getProperty ("file", {}).toString().containsIgnoreCase ("background") ? juce::String ("the background")
                                                                                                         : juce::String ("a picture for the panel"));

    if (name == "make_look")
        return "Designing the look \"" + input.getProperty ("name", {}).toString().trim() + "\"";

    if (name == "find_gallery_looks")
        return juce::String::fromUTF8 ("Looking through the knob gallery\xe2\x80\xa6");

    if (name == "use_gallery_look")
        return "Taking a look from the knob gallery";

    return {};
}

juce::String ProjectTools::buildResult (bool ok) const
{
    if (! ok)
    {
        auto log = preview.getLog();

        if (log.length() > maxLogChars)
            log = log.substring (0, maxLogChars) + "\n[... more errors cut]";

        return preview.getStatus() + "\n" + log;
    }

    juce::String text;
    text << preview.getStatus() << ". It's playing live now.\n";

    const auto& params = preview.getParameters();
    text << "Parameters (" << params.size() << "):\n";

    for (const auto& p : params)
        text << "- " << p.id << " \"" << p.name << "\" " << juce::String (p.min, 3) << ".." << juce::String (p.max, 3)
             << (p.unit.isNotEmpty() ? " " + p.unit : juce::String()) << ", default " << juce::String (p.def, 3) << "\n";

    if (preview.getLog().isNotEmpty())
        text << "\nCompiler warnings:\n" << preview.getLog().substring (0, 3000);

    return text;
}

juce::String ProjectTools::checkLayout (const GuiLayout& layout, const GuiLayout& before) const
{
    juce::StringArray unbound;

    for (const auto& w : layout.widgets)
    {
        juce::StringArray ids;

        if (w.param.isNotEmpty())
            ids.add (w.param);

        for (const auto& [role, id] : w.roles)
            if (id.isNotEmpty())
                ids.add (id);

        for (const auto& id : ids)
        {
            bool known = false;

            for (const auto& p : preview.getParameters())
                known = known || p.id == id;

            if (! known)
                unbound.addIfNotAlreadyThere (id);
        }
    }

    // Pictures must be files the user added.
    juce::StringArray missingPictures;
    const auto pictures = project.getGuiFolder().getChildFile (GuiLayout::imagesFolder);

    for (const auto& w : layout.widgets)
        if (w.type == GuiWidget::Type::image && w.image.isNotEmpty() && ! pictures.getChildFile (w.image).existsAsFile())
            missingPictures.addIfNotAlreadyThere (w.image);

    // Looks must exist, for that kind of control (an old name that was already there isn't Stella's to answer for).
    juce::StringArray unknownLooks;

    {
        Looks looks;
        looks.setGuiFolder (project.getGuiFolder());

        juce::StringArray woreBefore;

        for (const auto& w : before.widgets)
            if (Looks::takesLook (w))
                woreBefore.addIfNotAlreadyThere (Looks::kindName (Looks::kindOf (w)) + "|" + w.style);

        for (const auto& w : layout.widgets)
            if (Looks::takesLook (w) && w.style.isNotEmpty() && ! woreBefore.contains (Looks::kindName (Looks::kindOf (w)) + "|" + w.style))
                if (const auto* look = looks.find (w.style); look == nullptr || look->kind != Looks::kindOf (w))
                    unknownLooks.addIfNotAlreadyThere (w.style);
    }

    // A look's shadow falls past its control: it needs room, not a neighbour under it.
    juce::StringArray crowdedShadows;

    {
        Looks looks;
        looks.setGuiFolder (project.getGuiFolder());

        for (const auto& w : layout.widgets)
        {
            if (! Looks::takesLook (w))
                continue;

            const auto area = lookArea (w);
            const auto room = Looks::overhang (looks.lookFor (w), area.getWidth(), area.getHeight());

            if (room <= 1)
                continue;

            const auto shadow = area.expanded (room);

            for (const auto& other : layout.widgets)
            {
                if (&other == &w || ! Looks::takesLook (other))
                    continue;

                if (w.page.isNotEmpty() && other.page.isNotEmpty() && w.page != other.page)
                    continue;   // never shown together

                if (shadow.intersects (lookArea (other)))
                {
                    crowdedShadows.addIfNotAlreadyThere ((w.label.isNotEmpty() ? w.label : w.param) + " (needs " + juce::String (room) + " px)");
                    break;
                }
            }
        }
    }

    // Custom elements must be programmed: a file in elements/ that makes them.
    juce::StringArray programmed, unprogrammed;

    for (const auto& e : WasmCompiler::findElements (project.getFolder()))
        programmed.add (e.name);

    for (const auto& e : WasmCompiler::builtInElements())
        programmed.add (e.name);

    for (const auto& w : layout.widgets)
        if (w.type == GuiWidget::Type::custom && ! programmed.contains (w.element))
            unprogrammed.addIfNotAlreadyThere (w.element.isNotEmpty() ? w.element : juce::String ("(no element name)"));

    // Pages: a tabs element shows them, and every element's page is one of them.
    juce::StringArray strayPages;
    bool hasTabs = false;

    for (const auto& w : layout.widgets)
    {
        hasTabs = hasTabs || w.type == GuiWidget::Type::tabs;

        if (w.page.isNotEmpty() && ! layout.pages.contains (w.page))
            strayPages.addIfNotAlreadyThere (w.page);
    }

    const bool tabsMissing = layout.pages.size() >= 2 && ! hasTabs;

    // A selector is a row of buttons: with many positions they're too small to read (a glowing
    // display shows one name at a time, so it takes any number).
    juce::StringArray crowded;

    for (const auto& w : layout.widgets)
        if (w.type == GuiWidget::Type::selector && w.options.size() > 8 && PanelParts::selectorDisplayOf (w) != "vfd")
            crowded.addIfNotAlreadyThere (w.label.isNotEmpty() ? w.label : w.param);

    return (tabsMissing ? juce::String ("\nThe window has pages but no \"tabs\" element to show them: add one ({ \"type\": \"tabs\", ... }), "
                                        "or only the first page can be seen.")
                        : juce::String())
         + (strayPages.isEmpty() ? juce::String()
                                 : "\nThese pages aren't in the window's \"pages\" list (their elements show on the first page): "
                                       + strayPages.joinIntoString (", ") + ".")
         + (crowdedShadows.isEmpty() ? juce::String()
                                     : "\nThese controls' shadows fall onto the next control (their look's shadow reaches past them): "
                                           "move them apart or make them a little smaller, so each shadow has its room: " + crowdedShadows.joinIntoString (", ") + ".")
         + (crowded.isEmpty() ? juce::String()
                              : "\nThese selectors have too many buttons to read (" + crowded.joinIntoString (", ")
                                    + "): make each a \"dropdown\" instead, or give it \"display\": \"vfd\" (one name at a time).")
         + (unprogrammed.isEmpty() ? juce::String()
                                   : "\nThese elements aren't toolbox types and aren't programmed: " + unprogrammed.joinIntoString (", ")
                                         + ". Use a toolbox type, or program each one (elements/<Name>.cpp ending with STELLA_ELEMENT (<Name>, ...), then build).")
         + (unbound.isEmpty() || preview.getParameters().isEmpty() ? juce::String()
                                                                  : "\nThese parameter ids aren't in the plugin yet: " + unbound.joinIntoString (", ")
                                                                        + ". Add each to the plugin's code (or bind the element to a parameter it has).")
         + (missingPictures.isEmpty() ? juce::String()
                                      : "\nThese pictures aren't in gui/images (use only the listed ones): " + missingPictures.joinIntoString (", "))
         + (unknownLooks.isEmpty() ? juce::String()
                                   : "\nThese looks don't exist for those controls, so they show a default (use only the listed looks, of the right kind): "
                                         + unknownLooks.joinIntoString (", "));
}

//==============================================================================
void ProjectTools::run (const juce::String& name, const juce::var& input, Done done)
{
    auto fail = [&done] (const juce::String& message) { done (message, true); };

    if (name == "read_project")
    {
        done (describeProject(), false);
        return;
    }

    if (name == "read_file")
    {
        const auto path = input.getProperty ("path", {}).toString().trim().replaceCharacter ('\\', '/');

        // The user's own code: a part at a time.
        if (path.startsWith (juce::String (referenceFolderName) + "/"))
        {
            juce::String text;

            if (const auto r = getReference().read (path, (int) input.getProperty ("from", 1), (int) input.getProperty ("lines", 0), text); r.failed())
                return fail (r.getErrorMessage());

            done (text, false);
            return;
        }

        juce::File file;

        if (const auto r = resolve (input.getProperty ("path", {}).toString(), file, false); r.failed())
            return fail (r.getErrorMessage());

        if (! file.existsAsFile())
            return fail ("There's no file " + input.getProperty ("path", {}).toString() + ".");

        done (file.loadFileAsString(), false);
        return;
    }

    if (name == "search_reference")
    {
        const auto reference = getReference();

        if (reference.isEmpty())
            return fail ("The user hasn't brought any code of their own.");

        done (reference.search (input.getProperty ("text", {}).toString(), CodeReference::maxSearchResults), false);
        return;
    }

    if (name == "write_file")
    {
        juce::File file;

        if (const auto r = resolve (input.getProperty ("path", {}).toString(), file, true); r.failed())
            return fail (r.getErrorMessage());

        const auto content = input.getProperty ("content", {}).toString();

        if (content.getNumBytesAsUTF8() > (size_t) maxFileBytes)
            return fail ("That file is too big (over 200 KB). Split it into smaller modules.");

        file.getParentDirectory().createDirectory();

        if (! file.replaceWithText (content))
            return fail ("Couldn't write " + file.getFullPathName());

        const bool isDesign = file == project.getFolder().getChildFile (designFileName);
        const auto written = isDesign ? juce::String (designFileName) : file.getParentDirectory().getFileName() + "/" + file.getFileName();
        const bool isElement = file.getParentDirectory().getFileName() == WasmCompiler::elementsFolderName;
        done ("Wrote " + written + " (" + juce::String (countLines (content)) + " lines)."
                  + (isElement ? " Build to compile it; then place it with { \"type\": \"custom\", \"element\": <name> }." : ""),
              false);
        return;
    }

    if (name == "delete_file")
    {
        juce::File file;

        if (const auto r = resolve (input.getProperty ("path", {}).toString(), file, true); r.failed())
            return fail (r.getErrorMessage());

        if (file == project.getFolder().getChildFile (designFileName))
            return fail ("The design brief stays: write it again with what changed instead.");

        if (file.existsAsFile() && ! file.deleteFile())
            return fail ("Couldn't delete " + file.getFullPathName());

        done ("Deleted " + file.getParentDirectory().getFileName() + "/" + file.getFileName() + ".", false);
        return;
    }

    if (name == "set_graph")
    {
        if (! project.isOpen())
            return fail ("No project is open: the user has to create one (New) or open one first.");

        const auto graph = input.getProperty ("graph", {});

        if (! graph.isObject() || graph.getProperty ("modules", {}).getArray() == nullptr)
            return fail ("The graph needs a \"modules\" list (and usually \"wires\").");

        const auto file = project.getFolder().getChildFile (WasmCompiler::graphFileName);

        if (! file.replaceWithText (juce::JSON::toString (graph)))
            return fail ("Couldn't write graph.json");

        const auto* modules = graph.getProperty ("modules", {}).getArray();
        const auto* wires = graph.getProperty ("wires", {}).getArray();
        done ("graph.json saved: " + juce::String (modules->size()) + " modules, "
              + juce::String (wires != nullptr ? wires->size() : 0) + " wires. Build to hear it.", false);
        return;
    }

    if (name == "set_layout")
    {
        if (! project.isOpen())
            return fail ("No project is open: the user has to create one (New) or open one first.");

        const auto json = input.getProperty ("layout", {});

        if (! json.isObject() || json.getProperty ("widgets", {}).getArray() == nullptr)
            return fail ("The layout needs a \"widgets\" list (and usually width, height and background).");

        // Read through the layout model, so what's saved is always valid and tidy.
        const auto layout = GuiLayout::fromVar (json);
        const auto file = project.getGuiFolder().getChildFile (GuiLayout::fileName);

        // What it wore before: an old name that was already there isn't Stella's to answer for.
        GuiLayout before;
        GuiLayout::load (file, before);

        if (const auto saved = layout.save (file); saved.failed())
            return fail (saved.getErrorMessage());

        if (onLayoutChanged != nullptr)
            onLayoutChanged();

        done ("The GUI is showing: " + juce::String ((int) layout.widgets.size()) + " elements, " + juce::String (layout.width)
                  + " x " + juce::String (layout.height) + "." + checkLayout (layout, before),
              false);
        return;
    }

    if (name == "edit_layout")
    {
        if (! project.isOpen())
            return fail ("No project is open: the user has to create one (New) or open one first.");

        const auto file = project.getGuiFolder().getChildFile (GuiLayout::fileName);
        GuiLayout layout;

        if (! file.existsAsFile() || GuiLayout::load (file, layout).failed())
            return fail ("There's no GUI yet: build first (the studio then makes a plain one), or design one with set_layout.");

        const auto before = layout;
        const auto count = (int) layout.widgets.size();
        const juce::Rectangle<int> oldWindow (0, 0, layout.width, layout.height);
        juce::StringArray problems;
        std::vector<juce::Rectangle<int>> placed;   // where changed and added elements went, for the window to fit
        int changedCount = 0, addedCount = 0;

        // Changes first, then removals, both by the indexes as listed.
        if (const auto* changes = input.getProperty ("change", {}).getArray())
        {
            for (const auto& change : *changes)
            {
                const auto index = change.hasProperty ("index") ? indexFrom (change.getProperty ("index", {})) : -1;

                if (! juce::isPositiveAndBelow (index, count) || change.getDynamicObject() == nullptr)
                {
                    problems.add ("there's no element " + change.getProperty ("index", "?").toString() + " to change");
                    continue;
                }

                auto& w = layout.widgets[(size_t) index];
                w = changedWidget (w, change);
                placed.push_back (w.bounds);
                ++changedCount;
            }
        }

        std::vector<int> removals;

        if (const auto* list = input.getProperty ("remove", {}).getArray())
        {
            for (const auto& item : *list)
            {
                const auto index = indexFrom (item);

                if (juce::isPositiveAndBelow (index, count))
                    removals.push_back (index);
                else
                    problems.add ("there's no element " + item.toString() + " to remove");
            }
        }

        std::sort (removals.begin(), removals.end());
        removals.erase (std::unique (removals.begin(), removals.end()), removals.end());

        for (auto it = removals.rbegin(); it != removals.rend(); ++it)
            layout.widgets.erase (layout.widgets.begin() + *it);

        // Then additions: in front, or at "at".
        if (const auto* additions = input.getProperty ("add", {}).getArray())
        {
            for (const auto& item : *additions)
            {
                if (item.getDynamicObject() == nullptr || ! item.hasProperty ("type"))
                {
                    problems.add ("an addition had no \"type\"");
                    continue;
                }

                const auto w = GuiLayout::widgetFromVar (item);
                const auto at = item.hasProperty ("at") ? juce::jlimit (0, (int) layout.widgets.size(), (int) item.getProperty ("at", 0))
                                                        : (int) layout.widgets.size();
                layout.widgets.insert (layout.widgets.begin() + at, w);
                placed.push_back (w.bounds);
                ++addedCount;
            }
        }

        // The window: as asked, then big enough for what was placed; a picture filling it keeps filling it.
        if (input.hasProperty ("width"))
            layout.width = juce::jlimit (GuiLayout::minWidth, GuiLayout::maxWidth, (int) input.getProperty ("width", layout.width));

        if (input.hasProperty ("height"))
            layout.height = juce::jlimit (GuiLayout::minHeight, GuiLayout::maxHeight, (int) input.getProperty ("height", layout.height));

        // The global light.
        if (const auto light = input.getProperty ("light", {}); light.isObject())
        {
            auto angle = std::fmod ((float) (double) light.getProperty ("angle", layout.lightAngle), 360.0f);

            if (angle > 180.0f)        angle -= 360.0f;
            else if (angle <= -180.0f) angle += 360.0f;

            layout.lightAngle = angle;
            layout.lightAltitude = juce::jlimit (0.0f, 90.0f, (float) (double) light.getProperty ("altitude", layout.lightAltitude));
        }

        // The pages, as listed: elements on a page that's gone go to the first one (with one page
        // or none, the window is one page again and nothing has a page).
        if (const auto* list = input.getProperty ("pages", {}).getArray())
        {
            juce::StringArray names;

            for (const auto& entry : *list)
                if (const auto text = entry.toString().trim(); text.isNotEmpty())
                    names.addIfNotAlreadyThere (text);

            layout.pages = names.size() >= 2 ? names : juce::StringArray();

            for (auto& w : layout.widgets)
                if (w.page.isNotEmpty() && ! layout.pages.contains (w.page))
                    w.page = layout.pages.isEmpty() ? juce::String() : layout.pages[0];
        }

        if (const auto colours = input.getProperty ("background", {}); colours.isObject())
        {
            layout.backgroundTop = GuiLayout::colourFromString (colours.getProperty ("top", {}).toString(), layout.backgroundTop);
            layout.backgroundBottom = GuiLayout::colourFromString (colours.getProperty ("bottom", {}).toString(), layout.backgroundBottom);
        }

        const auto askedWidth = layout.width, askedHeight = layout.height;

        for (const auto& bounds : placed)
        {
            if (bounds == oldWindow)
                continue;   // a picture filling the window

            layout.width = juce::jlimit (GuiLayout::minWidth, GuiLayout::maxWidth, juce::jmax (layout.width, bounds.getRight()));
            layout.height = juce::jlimit (GuiLayout::minHeight, GuiLayout::maxHeight, juce::jmax (layout.height, bounds.getBottom()));
        }

        if (layout.width != oldWindow.getWidth() || layout.height != oldWindow.getHeight())
            for (auto& w : layout.widgets)
                if (w.type == GuiWidget::Type::image && w.bounds == oldWindow)
                    w.bounds = { 0, 0, layout.width, layout.height };

        if (const auto saved = layout.save (file); saved.failed())
            return fail (saved.getErrorMessage());

        if (onLayoutChanged != nullptr)
            onLayoutChanged();

        juce::StringArray what;

        if (changedCount > 0)          what.add ("changed " + juce::String (changedCount));
        if (! removals.empty())        what.add ("removed " + juce::String ((int) removals.size()));
        if (addedCount > 0)            what.add ("added " + juce::String (addedCount));

        done ("The GUI is showing: " + juce::String ((int) layout.widgets.size()) + " elements, " + juce::String (layout.width) + " x "
                  + juce::String (layout.height) + (what.isEmpty() ? juce::String (".") : " (" + what.joinIntoString (", ") + ").")
                  + (layout.width != askedWidth || layout.height != askedHeight ? "\nThe window grew to fit what was placed." : juce::String())
                  + (problems.isEmpty() ? juce::String() : "\nNot done: " + problems.joinIntoString ("; ") + ".")
                  + checkLayout (layout, before)
                  + (what.isEmpty() ? juce::String() : "\nThe indexes have changed: use the project's updated \"GUI elements\" list for further edits."),
              ! problems.isEmpty() && what.isEmpty());
        return;
    }

    if (name == "create_project")
    {
        const auto projectName = input.getProperty ("name", {}).toString().trim().substring (0, 60);

        // The type is the user's, chosen beside Send: never the AI's guess.
        if (project.isOpen())
            return fail ("A project is open already. For another plugin the user starts a new one (New plugin) and chooses its type.");

        if (! chosenKind.has_value() || *chosenKind == PluginKind::noteEffect)
            return fail ("The user hasn't chosen what they're building: ask them to choose the type beside Send.");

        const auto kind = *chosenKind;

        if (projectName.isEmpty())
            return fail ("The project needs a name.");

        if (onCreateProject == nullptr)
            return fail ("Projects can't be created from here.");

        if (const auto created = onCreateProject (projectName, kind); created.failed())
            return fail (created.getErrorMessage());

        done ("Created and opened the project \"" + projectName + "\" (" + Project::kindDisplayName (kind)
                  + "). It's empty: write its modules, wire them with set_graph, then build.\n\n"
                  + describeProject(),
              false);
        return;
    }

    if (name == "save_preset")
    {
        if (! project.isOpen())
            return fail ("No project is open: the user has to create one (New) or open one first.");

        Preset preset;
        preset.name = input.getProperty ("name", {}).toString().trim().substring (0, 60);

        if (preset.name.isEmpty())
            return fail ("The preset needs a name.");

        // Every parameter gets a value: the ones given (kept in range), the rest their defaults.
        const auto* given = input.getProperty ("values", {}).getDynamicObject();
        juce::StringArray unknown;

        for (const auto& p : preview.getParameters())
        {
            auto value = p.def;

            if (given != nullptr && given->hasProperty (p.id))
                value = juce::jlimit (p.min, p.max, (float) (double) given->getProperty (p.id));

            preset.values[p.id] = value;
        }

        if (given != nullptr)
            for (const auto& entry : given->getProperties())
                if (preset.values.find (entry.name.toString()) == preset.values.end())
                    unknown.add (entry.name.toString());

        // An unreadable presets.json isn't written over: the presets in it would be lost.
        PresetBank bank;

        if (const auto loaded = bank.load (project.getFolder()); loaded.failed())
            return fail (loaded.getErrorMessage() + ", so the preset wasn't saved: saving now would lose the presets in it. "
                         "Tell the user the file needs fixing (or moving out of the project) before presets can be saved.");

        bank.put (preset);

        if (const auto saved = bank.save (project.getFolder()); saved.failed())
            return fail (saved.getErrorMessage());

        if (onPresetsChanged != nullptr)
            onPresetsChanged();

        done ("Saved the preset \"" + preset.name + "\" (" + juce::String ((int) bank.presets.size()) + " presets now)."
                  + (unknown.isEmpty() ? juce::String() : "\nThese ids don't exist and were left out: " + unknown.joinIntoString (", ")),
              false);
        return;
    }

    if (name == "build")
    {
        if (! project.isOpen())
            return fail ("No project is open: the user has to create one (New) or open one first.");

        preview.build (project.getFolder(), project.getInfo().uuid, [this, done, folder = project.getFolder()] (bool ok)
        {
            if (ok && project.isOpen() && project.getFolder() == folder)
                project.markBuilt();

            done (buildResult (ok), ! ok);
        });
        return;
    }

    if (name == "read_skill")
    {
        juce::String text;

        if (const auto r = Skills::read (Skills::For::plugins, input.getProperty ("name", {}).toString(), text); r.failed())
            return fail (r.getErrorMessage());

        done (text, false);
        return;
    }

    if (name == "analyze_sound")
        return analyzeSound (input, std::move (done));

    if (name == "paint_picture")
        return paintPicture (input, std::move (done));

    if (name == "make_look")
        return makeLook (input, std::move (done));

    if (name == "find_gallery_looks")
        return findGalleryLooks (input, std::move (done));

    if (name == "use_gallery_look")
        return useGalleryLook (input, std::move (done));

    fail ("There's no tool called " + name + ".");
}

//==============================================================================
void ProjectTools::analyzeSound (const juce::var& input, Done done)
{
    if (! project.isOpen())
        return done ("No project is open: the user has to create one (New) or open one first.", true);

    if (preview.getState() != LivePreview::State::playing || preview.getPluginWasm().empty())
        return done (preview.getState() == LivePreview::State::building ? "It's still building: listen once the build is done."
                                                                         : "Nothing is playing: build the plugin first (and fix it if the build failed).",
                     true);

    SoundCheck::Request request;
    const auto kind = project.getInfo().kind;
    request.instrument = kind == PluginKind::instrument || kind == PluginKind::noteEffect;
    request.library = PluginLibrary::entriesFor (PluginLibrary::load (project.getFolder()), project.getInfo().vendor, project.getInfo().name, {}, true);

    if (const auto* notes = input.getProperty ("notes", {}).getArray(); notes != nullptr && ! notes->isEmpty())
    {
        request.notes.clear();

        for (const auto& n : *notes)
            if (request.notes.size() < 16)
                request.notes.push_back (juce::jlimit (0, 127, GuiLayout::noteFromVar (n, 48)));
    }
    else if (input.hasProperty ("notes") && ! input.getProperty ("notes", {}).isArray())
    {
        request.notes = { juce::jlimit (0, 127, GuiLayout::noteFromVar (input.getProperty ("notes", {}), 48)) };
    }

    request.velocity = juce::jlimit (0.05f, 1.0f, (float) (double) input.getProperty ("velocity", 0.8));
    request.pitched = (bool) input.getProperty ("pitched", true);
    request.hold = juce::jlimit (0.1, 8.0, (double) input.getProperty ("hold", 1.2));
    request.tail = juce::jlimit (0.1, 8.0, (double) input.getProperty ("tail", 1.5));

    const auto signal = input.getProperty ("signal", "music").toString().trim().toLowerCase();
    const auto& names = SoundMeasure::signalNames();

    if (std::find (names.begin(), names.end(), signal.toStdString()) == names.end())
        return done ("There's no test signal called \"" + signal + "\": use music, drums, chord, noise, impulse or sine.", true);

    request.signal = signal.toStdString();

    // As it plays now: the live values, then any given to try; the modules bypassed in the studio.
    std::map<juce::String, int> slotOf;

    for (const auto& p : preview.getParameters())
    {
        request.values.push_back ({ p.index, preview.getParameterValue (p.index) });
        slotOf[p.id] = (int) request.values.size() - 1;
    }

    juce::StringArray unknown;

    if (const auto* given = input.getProperty ("values", {}).getDynamicObject())
    {
        for (const auto& entry : given->getProperties())
        {
            const auto found = slotOf.find (entry.name.toString());

            if (found == slotOf.end())
            {
                unknown.add (entry.name.toString());
                continue;
            }

            for (const auto& p : preview.getParameters())
                if (p.id == entry.name.toString())
                    request.values[(size_t) found->second].second = juce::jlimit (p.min, p.max, (float) (double) entry.value);
        }
    }

    for (const auto& m : preview.getModules())
        if (preview.isBypassed (m.id))
            request.bypassed.push_back (m.index);

    const auto note = unknown.isEmpty() ? juce::String() : "\n(These ids don't exist and were left out: " + unknown.joinIntoString (", ") + ".)";

    worker.addJob ([wasm = preview.getPluginWasm(), request, done, stillAlive = alive, note]
    {
        bool isError = false;
        const auto report = SoundCheck::run (wasm, request, isError) + note;

        juce::MessageManager::callAsync ([report, isError, done, stillAlive]
        {
            if (*stillAlive)
                done (report, isError);
        });
    });
}

void ProjectTools::makeLook (const juce::var& input, Done done)
{
    if (! project.isOpen())
        return done ("No project is open: the user has to create one (New) or open one first.", true);

    const auto name = input.getProperty ("name", {}).toString().trim().removeCharacters ("\\/:*?\"<>|").substring (0, 48).trim();

    if (name.isEmpty())
        return done ("Give the look a name (\"name\"), e.g. \"Brass knob\".", true);

    Looks looks;
    looks.setGuiFolder (project.getGuiFolder());

    // What it starts from: a look by its name (any case), or the kind's default.
    const auto fromName = input.getProperty ("from", {}).toString().trim();
    const Looks::Look* from = nullptr;

    if (fromName.isNotEmpty())
    {
        from = looks.find (fromName);

        for (const auto* look : looks.all())
            if (from == nullptr && look->name.equalsIgnoreCase (fromName))
                from = look;

        if (from == nullptr)
        {
            juce::StringArray names;

            for (const auto* look : looks.all())
                names.add (look->name);

            return done ("There's no look called \"" + fromName + "\". The looks: " + names.joinIntoString (", ") + ".", true);
        }
    }
    else
    {
        from = &looks.defaultFor (Looks::kindFromName (input.getProperty ("kind", "knob").toString()));
    }

    auto doc = from->doc;
    const auto kind = from->kind;
    const auto fromLabel = from->name;   // from may be the look replaced below
    juce::StringArray changes;

    // Its own surface, then the layers named.
    if (const auto surface = input.getProperty ("surface", {}); surface.isObject())
    {
        const auto body = bodyLayerOf (doc);

        if (body < 0)
            return done ("\"" + from->name + "\" has no body layer to take a surface: name the layer in \"layers\" (its layers: "
                         + layersNote (*from).trim() + ").", true);

        juce::StringArray described;

        if (const auto problem = setSurface (doc.layers[(size_t) body], surface, described); problem.isNotEmpty())
            return done (problem, true);

        changes.add (doc.layers[(size_t) body].name + " " + described.joinIntoString (", "));
    }

    if (const auto* layers = input.getProperty ("layers", {}).getDynamicObject())
    {
        for (const auto& entry : layers->getProperties())
        {
            const auto layerName = entry.name.toString().trim();
            int index = -1;

            for (size_t i = 0; i < doc.layers.size() && index < 0; ++i)
                if (doc.layers[i].name.trim().equalsIgnoreCase (layerName))
                    index = (int) i;

            if (index < 0)
                return done ("\"" + from->name + "\" has no layer called \"" + layerName + "\" (its layers: " + layersNote (*from).trim() + ").", true);

            if (! entry.value.isObject())
                return done ("The layer \"" + layerName + "\" needs its settings as an object, e.g. { \"material\": \"gold\" }.", true);

            juce::StringArray described;

            if (const auto problem = setSurface (doc.layers[(size_t) index], entry.value, described); problem.isNotEmpty())
                return done (problem, true);

            changes.add (doc.layers[(size_t) index].name + " " + described.joinIntoString (", "));
        }
    }

    if (changes.isEmpty())
        return done ("Say what the look is made of: \"surface\" (its own) and/or \"layers\" (by name), e.g. \"surface\": { \"material\": \"brass\", "
                     "\"finish\": \"polished\" }.", true);

    const auto description = input.getProperty ("description", {}).toString().trim().substring (0, 120);
    looks.setProjectLook (name, kind, doc, description.isNotEmpty() ? description : changes.joinIntoString ("; "));

    if (looks.find (name, Looks::Origin::project) == nullptr)
        return done ("The look couldn't be saved in gui/looks.", true);

    // The studio reads its looks again, and shows controls wearing it in the new one.
    if (onLayoutChanged != nullptr)
        onLayoutChanged();

    done ("Made the look \"" + name + "\" (" + Looks::kindDisplayName (kind) + ", from \"" + fromLabel + "\"): " + changes.joinIntoString ("; ")
              + ". It's this plugin's own: put it on controls with \"style\": \"" + name + "\" (edit_layout).",
          false);
}

//==============================================================================
// The KnobMan gallery (GalleryLibrary): its items whose licence allows them in a plugin that's sold
namespace
{
    /** "$knob", "$slider", "$switch" for find_gallery_looks' kind. */
    juce::String galleryCategory (const juce::String& kind)
    {
        return kind == "slider" || kind == "hslider" ? "$slider" : kind == "switch" ? "$switch" : "$knob";
    }

    /** What a KnobMan knob is, by its shape and frames (as the Knob Studio has it). */
    Looks::Kind kindOfKnobDoc (const LayerDoc& doc)
    {
        return doc.frames == 2                           ? Looks::Kind::button
             : doc.canvasHeight >= doc.canvasWidth * 2   ? Looks::Kind::slider
             : doc.canvasWidth >= doc.canvasHeight * 2   ? Looks::Kind::sliderAcross
                                                         : Looks::Kind::knob;
    }
}

void ProjectTools::findGalleryLooks (const juce::var& input, Done done)
{
    const auto category = galleryCategory (input.getProperty ("kind", "knob").toString().trim().toLowerCase());
    juce::StringArray words;
    words.addTokens (input.getProperty ("words", {}).toString().toLowerCase(), " ,;", "\"");
    words.removeEmptyStrings();

    struct Found { const GalleryItem* item; int score; };
    const auto index = GalleryLibrary::loadIndex();
    std::vector<Found> found;

    for (const auto& item : index)
    {
        if (! item.isCommercialSafe() || item.category() != category || ! GalleryLibrary::knobFile (item.id).existsAsFile())
            continue;

        const auto text = (item.file + " " + item.tags + " " + item.comment).toLowerCase();
        int score = 0;

        for (const auto& w : words)
            score += text.contains (w) ? 1 : 0;

        if (words.isEmpty() || score > 0)
            found.push_back ({ &item, score });
    }

    if (index.isEmpty())
        return done ("The KnobMan gallery isn't downloaded on this computer: design the look from the project's looks list (make_look).", false);

    // The best matches first, then the newest.
    std::sort (found.begin(), found.end(), [] (const Found& a, const Found& b)
    {
        return a.score != b.score ? a.score > b.score : a.item->id > b.item->id;
    });

    if (found.empty())
        return done ("No gallery " + category.substring (1) + " allowed in a sold plugin matches that. Try other words, "
                     "or design the look from the project's looks list (make_look).", false);

    juce::String text;
    text << juce::String ((int) juce::jmin ((size_t) 20, found.size())) << " of " << juce::String ((int) found.size())
         << " gallery " << category.substring (1) << "s (all allowed in a sold plugin):\n";

    for (size_t i = 0; i < found.size() && i < 20; ++i)
    {
        const auto& item = *found[i].item;
        auto tags = item.tags.removeCharacters ("$").replace (",", ", ").trim();
        text << "- id " << item.id << ": \"" << item.file.upToLastOccurrenceOf (".", false, false) << "\""
             << (tags.isNotEmpty() ? ", tags: " + tags : juce::String())
             << (item.comment.trim().isNotEmpty() ? ", note: " + item.comment.trim().substring (0, 90) : juce::String())
             << (item.author.isNotEmpty() ? ", by " + item.author : juce::String())
             << ", licence " << (item.licence.isNotEmpty() ? item.licence : juce::String ("not given")) << "\n";
    }

    done (text + "Then use_gallery_look with the id of the one that suits the plugin.", false);
}

void ProjectTools::useGalleryLook (const juce::var& input, Done done)
{
    if (! project.isOpen())
        return done ("No project is open: the user has to create one (New) or open one first.", true);

    const auto id = (int) input.getProperty ("id", 0);
    const GalleryItem* item = nullptr;
    const auto index = GalleryLibrary::loadIndex();

    for (const auto& candidate : index)
        if (candidate.id == id)
            item = &candidate;

    if (item == nullptr || ! GalleryLibrary::knobFile (id).existsAsFile())
        return done ("There's no gallery item " + juce::String (id) + " on this computer: pick one find_gallery_looks listed.", true);

    // Only what its licence allows in a plugin that's sold.
    if (! item->isCommercialSafe())
        return done ("Gallery item " + juce::String (id) + "'s licence (" + item->licence + ") doesn't allow it in a sold plugin: pick another.", true);

    const auto parsed = KnobFile::parseFile (GalleryLibrary::knobFile (id));

    if (! parsed.ok)
        return done ("Gallery item " + juce::String (id) + " couldn't be read (" + parsed.error + "): pick another.", true);

    Looks looks;
    looks.setGuiFolder (project.getGuiFolder());

    const auto own = item->file.upToLastOccurrenceOf (".", false, false).replaceCharacters ("_", " ").trim();
    auto wanted = input.getProperty ("name", {}).toString().trim();
    wanted = (wanted.isNotEmpty() ? wanted : own.isNotEmpty() ? own : "Gallery " + juce::String (id))
                 .removeCharacters ("\\/:*?\"<>|").substring (0, 48).trim();
    const auto name = looks.freeName (wanted);
    const auto kind = kindOfKnobDoc (parsed.document);
    const auto credits = "From the KnobMan gallery (#" + juce::String (id) + ")"
                       + (item->author.isNotEmpty() ? ", by " + item->author : juce::String())
                       + ". Licence: " + (item->licence.isNotEmpty() ? item->licence : juce::String ("not given")) + ".";

    looks.setProjectLook (name, kind, parsed.document, credits);

    if (looks.find (name, Looks::Origin::project) == nullptr)
        return done ("The look couldn't be saved in gui/looks.", true);

    if (onLayoutChanged != nullptr)
        onLayoutChanged();

    done ("Added the gallery look \"" + name + "\" (" + Looks::kindDisplayName (kind) + ", " + credits
              + ") as this plugin's own: put it on controls with \"style\": \"" + name + "\" (edit_layout)"
              + (kind == Looks::Kind::sliderAcross ? juce::String (", on sliders across (\"orientation\": \"horizontal\")") : juce::String())
              + (parsed.warnings.isEmpty() ? juce::String (".") : ". " + parsed.warnings.joinIntoString (" ")),
          false);
}

void ProjectTools::paintPicture (const juce::var& input, Done done)
{
    if (! project.isOpen())
        return done ("No project is open: the user has to create one (New) or open one first.", true);

    const auto element = input.getProperty ("element", {}).toString().trim();
    const auto fileName = pictureFileName (input.getProperty ("file", {}).toString());
    const auto width = (int) input.getProperty ("width", 0);
    const auto height = (int) input.getProperty ("height", 0);

    if (element.isEmpty())
        return done ("Name the programmed element that paints it (\"element\").", true);

    if (fileName.isEmpty())
        return done ("Give the picture a name (\"file\"), e.g. \"background\".", true);

    if (width < 8 || height < 8 || width > GuiLayout::maxWidth || height > GuiLayout::maxHeight)
        return done ("The picture's size must be from 8 x 8 to " + juce::String (GuiLayout::maxWidth) + " x " + juce::String (GuiLayout::maxHeight) + ".", true);

    const auto images = project.getGuiFolder().getChildFile (GuiLayout::imagesFolder);
    const auto target = images.getChildFile (fileName);

    if (target.existsAsFile() && ! GuiLayout::paintedPictures (project.getGuiFolder()).contains (fileName))
        return done ("There's a picture of the user's called " + fileName + ": paint under another name, and keep theirs.", true);

    const auto& elementsWasm = preview.getElementsWasm();

    if (elementsWasm.empty())
        return done (preview.getState() == LivePreview::State::playing
                         ? "The plugin has no programmed elements built: write the painter in elements/, build, then paint."
                         : "Build first (once): the painters, PanelPainter too, are part of the plugin's build.",
                     true);

    PainterHost host;
    host.label = input.getProperty ("label", {}).toString();
    host.images = project.getGuiFolder().getChildFile (GuiLayout::imagesFolder);

    if (const auto* options = input.getProperty ("options", {}).getArray())
        for (const auto& o : *options)
            host.options.add (o.toString());

    if (const auto* settings = input.getProperty ("settings", {}).getDynamicObject())
        for (const auto& entry : settings->getProperties())
            host.settings[entry.name.toString()] = entry.value.isArray() || entry.value.isObject() ? juce::JSON::toString (entry.value, true)
                                                                                                   : entry.value.toString();

    if (const auto colourText = input.getProperty ("color", input.getProperty ("colour", {})).toString(); colourText.isNotEmpty())
        host.colourValue = GuiLayout::colourFromString (colourText, juce::Colours::transparentBlack).getARGB();

    // Lit by the panel's global light (the layout's "light"), unless it says where its own comes from.
    if (host.settings.count ("light") == 0 || host.settings.count ("altitude") == 0)
    {
        GuiLayout layout;

        if (GuiLayout::load (project.getGuiFolder().getChildFile (GuiLayout::fileName), layout).wasOk())
        {
            if (host.settings.count ("light") == 0)
                host.settings["light"] = juce::String (layout.lightAngle);

            if (host.settings.count ("altitude") == 0)
                host.settings["altitude"] = juce::String (layout.lightAltitude);
        }
    }

    // What it was painted with, kept beside it: the Edit tab's material controls paint it again
    // from there. quiet: the Edit tab asked, and it reads the picture afresh itself.
    const bool quiet = (bool) input.getProperty ("quiet", false);
    auto recipe = input.clone();

    if (auto* object = recipe.getDynamicObject())
    {
        object->setProperty ("element", element);
        object->setProperty ("width", width);
        object->setProperty ("height", height);
        object->removeProperty ("quiet");
    }

    auto onDone = [this, done, stillAlive = alive, fileName, width, height, target, quiet, recipe] (const juce::String& problem)
    {
        if (! *stillAlive)
            return;

        if (problem.isNotEmpty())
            return done (problem, true);

        GuiLayout::rememberPainted (project.getGuiFolder(), fileName, recipe);

        if (onLayoutChanged != nullptr && ! quiet)
            onLayoutChanged();   // the canvas reads the picture afresh

        done ("Painted gui/images/" + fileName + " (" + juce::String (width) + " x " + juce::String (height) + "). Place it with "
              "edit_layout or set_layout: as the background, { \"type\": \"image\", \"x\": 0, \"y\": 0, \"w\": <the window's width>, "
              "\"h\": <its height>, \"image\": \"" + fileName + "\", \"mode\": \"stretch\" } first in the list (\"at\": 0 with edit_layout). "
              "Where it's already placed, it shows the new picture now.",
              false);
    };

    worker.addJob ([wasm = elementsWasm, host, element, width, height, target, onDone]() mutable
    {
        juce::String problem;
        std::string error;
        auto elements = WasmElements::load (wasm, error);

        if (elements == nullptr)
        {
            problem = "The programmed elements couldn't start: " + juce::String (error);
        }
        else if (! elements->hasType (element.toStdString()))
        {
            juce::StringArray names;

            for (const auto& t : elements->getTypes())
                names.add (t.name);

            problem = "There's no programmed element called " + element + " in the last build (there are: " + names.joinIntoString (", ")
                    + "). Write it in elements/" + element + ".cpp and build first.";
        }
        else
        {
            elements->setHost (&host);
            elements->setPatience (10.0);

            const std::uint32_t* pixels = nullptr;

            if (! elements->create (0, element.toStdString(), width, height) || ! elements->render (0)
                || (pixels = elements->pixels (0, false, width, height)) == nullptr)
            {
                problem = elements->hasFailed() ? "The painter stopped: " + juce::String (elements->getFailure())
                                                    + ". Fix its code (out-of-range indexes, endless loops, too slow) and build again."
                                                : juce::String ("The painter drew nothing.");
            }
            else
            {
                juce::Image image (juce::Image::ARGB, width, height, false, juce::SoftwareImageType());
                bool anything = false;

                {
                    juce::Image::BitmapData data (image, juce::Image::BitmapData::writeOnly);

                    for (int y = 0; y < height; ++y)
                    {
                        const auto* row = pixels + (std::size_t) y * (std::size_t) width;
                        std::memcpy (data.getLinePointer (y), row, sizeof (std::uint32_t) * (std::size_t) width);

                        for (int x = 0; x < width && ! anything; ++x)
                            anything = (row[x] >> 24) != 0;
                    }
                }

                if (! anything)
                {
                    problem = "The painter drew nothing: every pixel is transparent.";
                }
                else
                {
                    target.getParentDirectory().createDirectory();
                    const auto temp = target.getSiblingFile (target.getFileNameWithoutExtension() + ".painting");
                    temp.deleteFile();
                    bool written = false;

                    {
                        juce::FileOutputStream out (temp);
                        written = out.openedOk() && juce::PNGImageFormat().writeImageToStream (image, out);
                    }

                    if (! written || ! temp.moveFileTo (target))
                    {
                        temp.deleteFile();
                        problem = "Couldn't save " + target.getFullPathName();
                    }
                }
            }
        }

        juce::MessageManager::callAsync ([onDone, problem] { onDone (problem); });
    });
}
