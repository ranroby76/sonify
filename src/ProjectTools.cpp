// D:\workspace\Fanan_web_hub\src\ProjectTools.cpp

#include "ProjectTools.h"
#include "GuiLayout.h"
#include "KnobStudio.h"       // Sonify's gallery (KnobStudio::galleryFolder, readGalleryItem)
#include "KnobFile.h"
#include "LayerRender.h"
#include "Looks.h"
#include "BodyEditor.h"       // design_templates: the bodies
#include "Body3D.h"
#include "IconLibrary.h"      // find_icons
#include "ModMatrix.h"
#include "PerformControl.h"
#include "PresetPanel.h"
#include "SelectorSet.h"
#include "MaterialChoices.h"
#include "MaterialArchetypes.h"   // edit_layout's built-in material looks
#include "PanelLayouts.h"         // design_templates: the panel layouts
#include "Model3D.h"          // 3D looks: make_3d_look, the 3D Gallery
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
#include <optional>

//==============================================================================
// Sonify's galleries for Fanan AI: every knob, slider and switch in the gallery (or every 3D model
// in the 3D Gallery), numbered by name.
namespace
{
    struct GalleryEntry
    {
        int id = 0;               // its number in find_gallery_looks' lists (1, 2, 3... by name)
        juce::File file;
        juce::String name;        // its file's name
        Looks::Kind kind = Looks::Kind::knob;
        juce::String model;       // a 3D model's kind ("knob", "button", "toggle", "slider"); empty: not 3D
    };

    std::vector<GalleryEntry> galleryEntries (bool models = false)
    {
        // What each file is, read once (a Sonify knob file names its kind at its start, a 3D model
        // its model's kind; a KnobMan one is read whole), again only when the file changes.
        struct Known
        {
            juce::int64 stamp = 0;
            Looks::Kind kind = Looks::Kind::knob;
            juce::String model;
        };

        static juce::CriticalSection lock;
        static std::map<juce::String, Known> known;
        const juce::ScopedLock sl (lock);

        const auto folder = models ? KnobStudio::modelGalleryFolder() : KnobStudio::galleryFolder();
        auto files = folder.findChildFiles (juce::File::findFiles, true, models ? "*" + juce::String (Looks::modelExtension)
                                                                                : juce::String ("*.knob;*") + Looks::fileExtension);
        std::sort (files.begin(), files.end(), [&folder] (const juce::File& a, const juce::File& b)
        {
            return a.getRelativePathFrom (folder).compareNatural (b.getRelativePathFrom (folder)) < 0;
        });

        std::vector<GalleryEntry> entries;

        for (const auto& file : files)
        {
            const auto stamp = file.getLastModificationTime().toMilliseconds() ^ file.getSize();
            auto found = known.find (file.getFullPathName());

            if (found == known.end() || found->second.stamp != stamp)
            {
                std::optional<Looks::Kind> kind;
                juce::String model;

                if (file.hasFileExtension (Looks::fileExtension) || file.hasFileExtension (Looks::modelExtension))
                {
                    juce::FileInputStream in (file);
                    juce::MemoryBlock start;

                    if (in.openedOk())
                        in.readIntoMemoryBlock (start, 32768);   // (a 3D model is small: its model's kind is in it too)

                    const auto text = start.toString();

                    if (const auto at = text.indexOf ("stellaKind=\""); at >= 0)
                        kind = Looks::kindFromName (text.substring (at + 12).upToFirstOccurrenceOf ("\"", false, false));

                    if (const auto at = text.indexOf ("<Model3D"); at >= 0)
                        if (const auto k = text.indexOf (at, "kind=\""); k >= 0)
                            model = text.substring (k + 6).upToFirstOccurrenceOf ("\"", false, false);
                }

                if (! kind.has_value())
                {
                    LayerDoc doc;
                    Looks::Kind read = Looks::Kind::knob;
                    juce::String credits, error;
                    bool commercialSafe = true;

                    if (! KnobStudio::readGalleryItem (file, doc, read, credits, commercialSafe, error))
                        continue;   // not a knob it can read: left out

                    kind = read;
                }

                found = known.insert_or_assign (file.getFullPathName(), Known { stamp, *kind, model }).first;
            }

            if (models && found->second.model.isEmpty())
                continue;   // (the 3D Gallery: 3D models only)

            GalleryEntry entry;
            entry.id = (int) entries.size() + 1;
            entry.file = file;
            entry.name = file.getFileNameWithoutExtension();
            entry.kind = found->second.kind;
            entry.model = found->second.model;
            entries.push_back (entry);
        }

        return entries;
    }
}

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

        // Its text: font, style, a caption's colour and shift (the edit menu's Text tab).
        if (w.font.isNotEmpty())
            line << " font=\"" << w.font << "\"";

        if (w.bold)      line << " bold";
        if (w.italic)    line << " italic";
        if (w.underline) line << " underline";

        if (! w.textColour.isTransparent())
            line << " textColor=#" << w.textColour.toDisplayString (true);

        if (w.textShift != 0)
            line << " textShift=" << w.textShift;

        if (w.fontSize > 0.0f && w.type != GuiWidget::Type::label && w.type != GuiWidget::Type::group && w.type != GuiWidget::Type::tabs)
            line << " textSize=" << juce::String (w.fontSize, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".");

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

        // Its settings merge: the keys given replace theirs (null removes one), the rest stay.
        auto settings = target->getProperty ("settings");

        if (const auto* given = fields->getProperty ("settings").getDynamicObject(); given != nullptr)
        {
            auto kept = settings.isObject() ? settings.clone() : juce::var (new juce::DynamicObject());

            for (const auto& entry : given->getProperties())
            {
                if (entry.value.isVoid() || entry.value.isUndefined())
                    kept.getDynamicObject()->removeProperty (entry.name);
                else
                    kept.getDynamicObject()->setProperty (entry.name, entry.value);
            }

            settings = kept;
        }

        for (const auto& field : fields->getProperties())
            if (field.name.toString() != "index")
                target->setProperty (field.name, field.value);

        if (settings.isObject())
            target->setProperty ("settings", settings);

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

    /** A 3D look: its model's kind, material, colour and view ("3D knob: Brass, #FFD4A94F, seen from 0 deg"). */
    juce::String modelNote (const Looks::Look& look)
    {
        const auto& m = *look.doc.model3d;
        const auto* material = Model3D::findParam (m.kind, "material");
        return " {3D " + Model3D::kindKey (m.kind) + ": "
             + (material != nullptr ? material->choices[m.choice ("material")] : juce::String())
             + ", " + "#" + m.colour ("colour").toDisplayString (true)
             + ", seen from " + juce::String (juce::roundToInt (m.number ("tilt"))) + " deg"
             + ", perspective " + juce::String (juce::roundToInt (m.number ("perspective"))) + "%}";
    }

    /** A look's layers, bottom first, each with its material: "layers: Shadow, Bezel=anodized, Body=titanium, Pointer". */
    juce::String layersNote (const Looks::Look& look)
    {
        if (look.doc.model3d != nullptr)
            return modelNote (look);

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
        props.getDynamicObject()->setProperty ("layout", property ("object", "The whole GUI: { \"format\": 1, \"width\", \"height\", \"background\", \"light\", \"camera\", \"body\" (its 3D body: the 3d-body skill), \"widgets\": [...] }"));
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
        props.getDynamicObject()->setProperty ("camera", property ("number", "The panel camera, 0..100 (50 standard): how one camera over the whole panel "
                                                                         "sees its 3D looks and body (the 3d-panel skill)."));
        props.getDynamicObject()->setProperty ("body", property ("object", "The window's 3D body, replacing it whole: { \"tilt\", \"shadow\", \"floor\", "
                                                                         "\"parts\": [...] } (the 3d-body skill; design_templates has ready-made ones). "
                                                                         "null removes it. Read gui/layout.json first to keep what the user built."));
        props.getDynamicObject()->setProperty ("materialLook", property ("string", "Only when the user asks for it: one of the built-in material looks, by its exact name: "
                                                                                  + MaterialArchetypes::names().joinIntoString (", ")
                                                                                  + ". Without lookIndex: every rectangular panel, the window's background, light and text colours. "
                                                                                    "It's put on before this call's changes, so they win."));
        props.getDynamicObject()->setProperty ("lookIndex", property ("integer", "With materialLook: the one shape to put it on, by its index in the \"GUI elements\" list."));
        props.getDynamicObject()->setProperty ("lookControls", property ("boolean", "With materialLook and no lookIndex, only when the user asks for the look's controls too: "
                                                                                   "every knob, slider and switch wears the look's matching 3D controls (the looks \"<look> knob\", "
                                                                                   "\"<look> slider\", \"<look> slider across\", \"<look> button\"; \"<look> lever\" is there for a switch too)."));
        props.getDynamicObject()->setProperty ("lookTheme", property ("boolean", "With materialLook and no lookIndex, only when the user asks for the look's displays or fonts too (or the "
                                                                                "whole theme): its lamps, selectors, readouts, LED digits, keyboards and meters, its fonts and title "
                                                                                "lettering and its panel camera; places, sizes, parameters and words stay as they are."));
        props.getDynamicObject()->setProperty ("lookPainting", property ("boolean", "With materialLook and no lookIndex, only when the user asks for the look's painted panel (a painted "
                                                                                   "or lit background in it): the panel shapes step back (hidden, kept) and the title takes the plate's "
                                                                                   "lettering; the result gives the paint_picture recipe to paint next, its parts placed from the layout."));
        tools.add (tool ("edit_layout",
                         "Changes part of the plugin's GUI and keeps everything else exactly as it is: removes, changes or adds elements, "
                         "or resizes the window, its light, camera and 3D body. A change's \"settings\" merge: the keys given replace theirs "
                         "(null removes one), the rest stay. Elements are named by their index in the project's \"GUI elements\" list; one call's "
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

    // 3D looks: knobs, buttons, sliders and toggle switches built as real objects and rendered.
    {
        auto props = object();
        props.getDynamicObject()->setProperty ("name", property ("string", "The new 3D look's name, e.g. \"Brass 3D knob\": controls wear it by this "
                                                                           "name. A look of this plugin's own with that name is replaced."));
        props.getDynamicObject()->setProperty ("from", property ("string", "A 3D look in the project's looks list to start from (its settings kept, "
                                                                           "those given changed): one marked {3D ...}. Left out: a ready-made one (preset)."));
        auto kind = property ("string", "Its kind, when from is left out: \"knob\", \"button\", \"toggle\" (a lever switch), \"slider\" "
                                        "(upright) or \"hslider\" (a slider across).");
        kind.getDynamicObject()->setProperty ("enum", juce::Array<juce::var> { "knob", "button", "toggle", "slider", "hslider" });
        props.getDynamicObject()->setProperty ("kind", kind);
        props.getDynamicObject()->setProperty ("preset", property ("string", "A ready-made one of the kind to start from (the 3d-controls skill names "
                                                                             "them; left out: the kind's first)."));
        props.getDynamicObject()->setProperty ("settings", property ("object", "Its settings by key, only those to change, e.g. { \"material\": "
                                                                               "\"Brass\", \"colour\": \"#FFD4A94F\", \"grip\": \"Flutes\", "
                                                                               "\"gripCount\": 24, \"ring\": true, \"tilt\": 20 }: a choice by "
                                                                               "its name, a flag true or false, a colour \"#AARRGGBB\", a number "
                                                                               "(kept in its range). Every key, its range and its default: the "
                                                                               "3d-controls skill."));
        props.getDynamicObject()->setProperty ("description", property ("string", "A few words for the looks list, e.g. \"brass, fluted, lit ring\"."));
        tools.add (tool ("make_3d_look",
                         "Makes a 3D look: a knob, button, toggle switch or slider built as a real object (a turned body with its skirt, "
                         "grip, cap and pointer; a cap on its bezel with its light; a fader on its track; a lever on its nut) in "
                         "materials (plastics, rubber, bakelite, brushed and spun aluminium, chrome, steel, gold, brass, copper, "
                         "anodized, painted metal), rendered, lit by the panel's global light and seen by the panel's camera. Starts "
                         "from a 3D look of the project's (from), or a ready-made one of a kind (kind, preset), and changes the "
                         "settings given. Saved as the plugin's own look: put it on controls with their \"style\" (edit_layout). "
                         "Read the 3d-controls and 3d-panel skills before a panel of 3D controls.",
                         props, { "name" }));
    }

    // Sonify's galleries: ready-made knobs, sliders and switches (and 3D models), every one allowed
    // in any plugin. Looked through first, before designing a look.
    {
        auto props = object();
        auto kind = property ("string", "\"knob\", \"slider\" (either way up) or \"switch\" (in the 3D Gallery: buttons and toggle switches).");
        kind.getDynamicObject()->setProperty ("enum", juce::Array<juce::var> { "knob", "slider", "switch" });
        props.getDynamicObject()->setProperty ("kind", kind);
        props.getDynamicObject()->setProperty ("words", property ("string", "What to look for in their names, e.g. \"metal\", \"vintage\", "
                                                                            "\"black\", \"vu\". Left out: all of that kind."));
        props.getDynamicObject()->setProperty ("3d", property ("boolean", "true: the 3D Gallery (3D models, for a panel of 3D controls) instead "
                                                                          "of the knob gallery."));
        tools.add (tool ("find_gallery_looks",
                         "Looks through Sonify's gallery for knobs, sliders or switches (with \"3d\": true, the 3D Gallery's 3D models): "
                         "ready-made designs, every one allowed in any plugin, sold ones too. Lists up to 30, each with its id and "
                         "name. Use it first when the panel needs knobs (or sliders, switches), before designing a look: pick the "
                         "item whose name suits the plugin's character, then use_gallery_look. Changes nothing.",
                         props, { "kind" }));
    }

    {
        auto props = object();
        props.getDynamicObject()->setProperty ("id", property ("integer", "The gallery item's id (from find_gallery_looks)."));
        props.getDynamicObject()->setProperty ("name", property ("string", "The look's name in this plugin, e.g. \"Vintage knob\" (left out: the item's own)."));
        props.getDynamicObject()->setProperty ("3d", property ("boolean", "true: an item of the 3D Gallery (as find_gallery_looks listed it with \"3d\": true)."));
        tools.add (tool ("use_gallery_look",
                         "Brings a gallery item (from find_gallery_looks) into the plugin as its own look, drawn exactly as in the "
                         "gallery. Then put it on controls with their \"style\" (edit_layout). A 3D one's settings can then be "
                         "changed with make_3d_look (from it).",
                         props, { "id" }));
    }

    // The studios' galleries of designs: selector sets, performance controls, matrices, preset
    // managers, 3D bodies.
    {
        auto props = object();
        auto kind = property ("string", "\"set\" (a selector's keys: its settings \"set\"), \"perform\" (wheels, levers, joysticks, pads, "
                                        "ribbons: an xy element's settings \"perform\"), \"matrix\" (a matrix element's settings \"matrix\"), "
                                        "\"presets\" (a preset manager: a preset element's settings \"presets\"), \"body\" (the "
                                        "window's 3D body: edit_layout's \"body\") or \"layout\" (a whole window laid out for a kind of "
                                        "plugin: a synth, a drum machine, a compressor...: set_layout's starting point).");
        kind.getDynamicObject()->setProperty ("enum", juce::Array<juce::var> { "set", "perform", "matrix", "presets", "body", "layout" });
        props.getDynamicObject()->setProperty ("kind", kind);
        props.getDynamicObject()->setProperty ("name", property ("string", "One design's name (as listed): its JSON. Left out: the list."));
        tools.add (tool ("design_templates",
                         "The studio's ready-made designs of a kind: lists them, or gives one's JSON to put in an element's settings "
                         "(or, for a body, in edit_layout's \"body\", made for this window's size; for a layout, a whole window to "
                         "start a new plugin's first design from). Start from the nearest one and "
                         "change what differs; the selector-sets, performance-controls, mod-matrix, preset-managers and 3d-body "
                         "skills have every key. Changes nothing.",
                         props, { "kind" }));
    }

    // The icon library: icons in place of words, or among them.
    {
        auto props = object();
        props.getDynamicObject()->setProperty ("words", property ("string", "What to look for in their names and tags, e.g. \"sine\", "
                                                                            "\"filter lowpass\", \"play\", \"microphone\", \"arrow left\"."));
        props.getDynamicObject()->setProperty ("category", property ("string", "Only this category (left out: all): " + IconLibrary::categories().joinIntoString (", ")
                                                                               + ". \"Audio\" has waves, filters, envelopes, LFOs, arpeggio modes, routing, "
                                                                                 "stereo, effects, voices and note values."));
        tools.add (tool ("find_icons",
                         "Looks through the icon library (some 13,000 icons and symbols, free in any plugin with no credit) for icons "
                         "to show in place of words: their names, best matches first (up to 60). In any text an icon is its name in "
                         "braces: \"{wave-sine}\", \"{filter-lowpass} Cutoff\". Changes nothing.",
                         props, { "words" }));
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

        // A GUI the user designed first, chosen for this plugin: create_project brings it in.
        if (GuiLayout draft; userGui.isDirectory() && GuiLayout::load (userGui.getChildFile (GuiLayout::fileName), draft).wasOk()
                             && ! draft.widgets.empty())
        {
            text << "\n\nThe user designed this plugin's GUI themselves before starting it, and chose to build the plugin on it: "
                    "create_project brings it in as the plugin's GUI (" << draft.width << " x " << draft.height << ", "
                 << (int) draft.widgets.size() << " elements). Their captions, and the parameter ids typed into their controls, say "
                    "what each control is for: build the parameters for them. Their elements:\n";

            const auto listed = juce::jmin ((int) draft.widgets.size(), maxListedElements);

            for (int i = 0; i < listed; ++i)
                text << elementLine (i, draft.widgets[(size_t) i], {}) << "\n";

            if (listed < (int) draft.widgets.size())
                text << "... and " << ((int) draft.widgets.size() - listed) << " more.\n";
        }

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

    // The user's own GUI design: kept as it is (the guide's "The user's own GUI").
    if (isUserGui())
        text << "\nThe GUI is the user's own design (they drew it before the plugin existed): follow the guide's \"The user's own GUI\". "
                "Keep their elements exactly as they are; wire their controls; for a new feature with no control of theirs, ask "
                "before you style one.\n";

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
             << " degrees, " << juce::String (layout.lightAltitude, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") << " up"
             << ", panel camera " << juce::roundToInt (layout.camera) << ".\n";

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

        // Sonify's gallery: what find_gallery_looks can offer.
        int knobs = 0, sliders = 0, switches = 0;

        for (const auto& item : galleryEntries())
        {
            knobs += item.kind == Looks::Kind::knob ? 1 : 0;
            sliders += item.kind == Looks::Kind::slider || item.kind == Looks::Kind::sliderAcross ? 1 : 0;
            switches += item.kind == Looks::Kind::button ? 1 : 0;
        }

        if (knobs + sliders + switches > 0)
            text << "Sonify's gallery (find_gallery_looks, use_gallery_look; every item allowed in any plugin, sold ones too): "
                 << knobs << " knobs, " << sliders << " sliders, " << switches << " switches.\n";
        else
            text << "Sonify's gallery is empty on this computer: design looks from the list above (make_look).\n";

        // ...and the 3D Gallery's models (find_gallery_looks with "3d": true).
        int models3d = 0;

        for (const auto& item : galleryEntries (true))
            models3d += item.model.isNotEmpty() ? 1 : 0;

        text << (models3d > 0 ? "Sonify's 3D Gallery (find_gallery_looks and use_gallery_look with \"3d\": true): " + juce::String (models3d)
                                    + " 3D models (knobs, buttons, toggle switches, sliders). "
                              : juce::String ("Sonify's 3D Gallery is empty on this computer. "))
             << "3D looks are made with make_3d_look; the panel's \"camera\" (0..100, now " << juce::roundToInt (layout.camera)
             << ") is how its one camera sees them.\n";
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

bool ProjectTools::isUserGui() const
{
    return project.isOpen() && project.getGuiFolder().getChildFile (userGuiMarker).existsAsFile();
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

    if (name == "make_3d_look")
        return "Building the 3D look \"" + input.getProperty ("name", {}).toString().trim() + "\"";

    if (name == "find_gallery_looks")
        return juce::String::fromUTF8 ((bool) input.getProperty ("3d", false) ? "Looking through the 3D Gallery\xe2\x80\xa6"
                                                                              : "Looking through the knob gallery\xe2\x80\xa6");

    if (name == "use_gallery_look")
        return "Taking a look from the knob gallery";

    if (name == "design_templates")
        return juce::String::fromUTF8 ("Looking through the studio's designs\xe2\x80\xa6");

    if (name == "find_icons")
        return juce::String::fromUTF8 ("Looking through the icons\xe2\x80\xa6");

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
        if (w.type == GuiWidget::Type::selector && w.options.size() > 8 && PanelParts::selectorDisplayOf (w) != "vfd" && PanelParts::selectorDisplayOf (w) != "dropdown")
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
                                    + "): make each a \"dropdown\" instead, or give it \"display\": \"dropdown\" (a list) or \"vfd\" (one name at a time).")
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

        // A built-in material look first: this call's own changes win over it.
        juce::String lookDone;

        if (const auto lookName = input.getProperty ("materialLook", {}).toString().trim(); lookName.isNotEmpty())
        {
            const auto look = MaterialArchetypes::find (lookName);

            if (! look.isObject())
            {
                problems.add ("there's no material look \"" + lookName + "\"");
            }
            else if (input.hasProperty ("lookIndex"))
            {
                const auto index = indexFrom (input.getProperty ("lookIndex", {}));

                if (juce::isPositiveAndBelow (index, count) && layout.widgets[(size_t) index].type == GuiWidget::Type::shape)
                {
                    MaterialArchetypes::applyToWidget (layout.widgets[(size_t) index], look);
                    lookDone = "Element " + juce::String (index) + " took the look \"" + lookName + "\".";
                }
                else
                {
                    problems.add ("element " + input.getProperty ("lookIndex", "?").toString() + " isn't a shape, so it can't take a material look");
                }
            }
            else
            {
                const auto panels = MaterialArchetypes::applyToLayout (layout, look);
                lookDone = juce::String (panels) + " panels took the look \"" + lookName + "\", with the window's background, light and text colours.";

                if ((bool) input.getProperty ("lookControls", false))
                    lookDone << " " << juce::String (MaterialArchetypes::applyControls (layout, lookName)) << " controls took its matching 3D controls.";

                if ((bool) input.getProperty ("lookTheme", false))
                    lookDone << " " << juce::String (MaterialArchetypes::applyTheme (layout, lookName)) << " elements took its displays and fonts.";

                if ((bool) input.getProperty ("lookPainting", false) && MaterialArchetypes::findPaintedPanel (lookName).isObject())
                {
                    const auto hidden = MaterialArchetypes::prepareForPainting (layout, lookName);
                    juce::String file ("background.png");

                    for (const auto& w : layout.widgets)
                        if (w.type == GuiWidget::Type::image && w.bounds == juce::Rectangle<int> (0, 0, layout.width, layout.height)
                            && GuiLayout::paintedPictures (project.getGuiFolder()).contains (w.image))
                            file = w.image;

                    lookDone << " " << juce::String (hidden) << " panel shapes stepped back (hidden, kept) for its painted panel. Paint it now: "
                             << "paint_picture with exactly " << juce::JSON::toString (MaterialArchetypes::painterRecipe (layout, lookName, file), true)
                             << ", then, unless an image showing " << file << " already fills the window, add { \"type\": \"image\", \"x\": 0, \"y\": 0, "
                             << "\"w\": " << juce::String (layout.width) << ", \"h\": " << juce::String (layout.height) << ", \"image\": \"" << file
                             << "\", \"mode\": \"stretch\" } with edit_layout at 0.";
                }
            }
        }

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

        // The panel camera, and the 3D body (replaced whole; null removes it).
        if (input.hasProperty ("camera"))
            layout.camera = juce::jlimit (0.0f, 100.0f, (float) (double) input.getProperty ("camera", layout.camera));

        if (input.hasProperty ("body"))
        {
            const auto body = input.getProperty ("body", {});
            layout.body = body.isObject() && body.getDynamicObject()->getProperties().size() > 0 ? body.clone() : juce::var();
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

        // The 3D body grows or shrinks with the window (unless this call gave a new one).
        if ((layout.width != oldWindow.getWidth() || layout.height != oldWindow.getHeight()) && ! input.hasProperty ("body")
            && Body3D::hasParts (layout.body))
            layout.body = BodyEditor::scaledBody (layout.body, oldWindow.getWidth(), oldWindow.getHeight(), layout.width, layout.height);

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
                  + (lookDone.isEmpty() ? juce::String() : "\n" + lookDone)
                  + (problems.isEmpty() ? juce::String() : "\nNot done: " + problems.joinIntoString ("; ") + ".")
                  + checkLayout (layout, before)
                  + (what.isEmpty() ? juce::String() : "\nThe indexes have changed: use the project's updated \"GUI elements\" list for further edits."),
              ! problems.isEmpty() && what.isEmpty() && lookDone.isEmpty());
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

        // The GUI the user designed first: it's this plugin's now, marked as theirs.
        juce::String takenGui;

        if (GuiLayout draft; userGui.isDirectory() && GuiLayout::load (userGui.getChildFile (GuiLayout::fileName), draft).wasOk()
                             && ! draft.widgets.empty())
        {
            const auto gui = project.getGuiFolder();

            if (userGui.copyDirectoryTo (gui))
            {
                gui.getChildFile (userGuiMarker).replaceWithText ("The user designed this GUI before the plugin existed: Fanan AI keeps it.\n");
                takenGui = " The user's own GUI design is its GUI: keep it, and wire it as the guide's \"The user's own GUI\" says.";

                if (onUserGuiTaken != nullptr)
                    onUserGuiTaken();
            }
        }

        userGui = juce::File();

        done ("Created and opened the project \"" + projectName + "\" (" + Project::kindDisplayName (kind)
                  + "). It's empty: write its modules, wire them with set_graph, then build." + takenGui + "\n\n"
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

    if (name == "make_3d_look")
        return make3dLook (input, std::move (done));

    if (name == "find_gallery_looks")
        return findGalleryLooks (input, std::move (done));

    if (name == "use_gallery_look")
        return useGalleryLook (input, std::move (done));

    if (name == "find_icons")
    {
        const auto words = input.getProperty ("words", {}).toString().trim();
        const auto category = input.getProperty ("category", {}).toString().trim();
        const auto found = IconLibrary::search (words, IconLibrary::categories().contains (category) ? category : juce::String(), 60);

        if (found.empty())
            return done ("No icons match \"" + words + "\": try fewer or other words (\"wave\", \"filter\", \"arrow\").", false);

        juce::StringArray lines;

        for (const auto index : found)
        {
            const auto& icon = IconLibrary::all()[(size_t) index];
            lines.add ("{" + icon.name + "} (" + icon.category + ")");
        }

        return done (juce::String ((int) found.size()) + " icons, best first:\n" + lines.joinIntoString ("\n"), false);
    }

    if (name == "design_templates")
    {
        const auto kind = input.getProperty ("kind", {}).toString().trim().toLowerCase();
        const auto wanted = input.getProperty ("name", {}).toString().trim();
        const auto names = kind == "set" ? SelectorSet::templateNames()
                         : kind == "perform" ? PerformControl::templateNames()
                         : kind == "matrix" ? ModMatrix::templateNames()
                         : kind == "presets" ? PresetPanel::templateNames()
                         : kind == "body" ? BodyEditor::templateNames()
                         : kind == "layout" ? PanelLayouts::names() : juce::StringArray();

        if (names.isEmpty())
            return fail ("kind is one of set, perform, matrix, presets, body, layout.");

        if (wanted.isEmpty() && kind == "layout")
        {
            done ("The panel layouts (ask for one by its name for its JSON):\n" + PanelLayouts::describe(), false);
            return;
        }

        if (wanted.isEmpty())
        {
            done ("The " + kind + " designs: " + names.joinIntoString (", ") + ". Ask for one by its name for its JSON.", false);
            return;
        }

        auto index = names.indexOf (wanted, true);

        for (int i = 0; index < 0 && i < names.size(); ++i)
            if (names[i].containsIgnoreCase (wanted))
                index = i;

        if (index < 0)
            return fail ("There's no " + kind + " design called \"" + wanted + "\". There are: " + names.joinIntoString (", ") + ".");

        juce::String text;

        if (kind == "body")
        {
            int width = 760, height = 420;

            if (project.isOpen())
            {
                GuiLayout layout;

                if (const auto file = project.getGuiFolder().getChildFile (GuiLayout::fileName);
                    file.existsAsFile() && GuiLayout::load (file, layout).wasOk())
                {
                    width = layout.width;
                    height = layout.height;
                }
            }

            text = "The body \"" + names[index] + "\" for a " + juce::String (width) + " x " + juce::String (height)
                 + " window (edit_layout's \"body\"):\n" + juce::JSON::toString (BodyEditor::makeTemplate (index, width, height), true);
        }
        else if (kind == "layout")
        {
            text = "The layout \"" + names[index] + "\": a starting point for set_layout. Its \"param\" ids (and \"params\", \"source\"s) are "
                   "roles: bind each to the plugin's own ids, drop the controls it has no parameter for and add the ones it needs, in the "
                   "same groups and on the same grid; resize the window to what's left. Keep its title and preset manager. Its look comes "
                   "from a material look (edit_layout's \"materialLook\", with \"lookControls\", \"lookTheme\" and \"lookPainting\" when "
                   "the user wants the whole theme):\n" + juce::JSON::toString (PanelLayouts::layout (index), true);
        }
        else if (kind == "set")
        {
            text = "The set \"" + names[index] + "\" (a selector's settings \"set\"; its picture shows "
                 + juce::String (SelectorSet::templateKeys (index)) + " keys):\n" + juce::JSON::toString (SelectorSet::makeTemplate (index), true);
        }
        else if (kind == "perform")
        {
            text = "The control \"" + names[index] + "\" (an xy element's settings \"perform\"):\n"
                 + juce::JSON::toString (PerformControl::makeTemplate (index), true)
                 + "\nThe element's settings it comes with: " + juce::JSON::toString (PerformControl::templateSettings (index), true);
        }
        else
        {
            text = "The design \"" + names[index] + "\" (a " + (kind == "matrix" ? juce::String ("matrix") : juce::String ("preset"))
                 + " element's settings \"" + kind + "\"):\n"
                 + juce::JSON::toString (kind == "matrix" ? ModMatrix::makeTemplate (index) : PresetPanel::makeTemplate (index), true);
        }

        done (text, false);
        return;
    }

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

    if (from->doc.model3d != nullptr)
        return done ("\"" + from->name + "\" is a 3D look: change it with make_3d_look (from it).", true);

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
// Sonify's gallery (KnobStudio::galleryFolder): its knobs, sliders and switches, every one
// allowed in any plugin, sold ones too.
void ProjectTools::findGalleryLooks (const juce::var& input, Done done)
{
    const auto wantedKind = input.getProperty ("kind", "knob").toString().trim().toLowerCase();
    const bool models = (bool) input.getProperty ("3d", false);
    const auto galleryName = juce::String (models ? "Sonify's 3D Gallery" : "Sonify's gallery");
    const auto instead = juce::String (models ? "make the 3D look yourself (make_3d_look)" : "design the look from the project's looks list (make_look)");
    juce::StringArray words;
    words.addTokens (input.getProperty ("words", {}).toString().toLowerCase(), " ,;", "\"");
    words.removeEmptyStrings();

    const auto all = galleryEntries (models);

    if (all.empty())
        return done (galleryName + " is empty on this computer: " + instead + ".", false);

    struct Found { const GalleryEntry* item; int score; };
    std::vector<Found> found;

    for (const auto& item : all)
    {
        const auto isSlider = item.kind == Looks::Kind::slider || item.kind == Looks::Kind::sliderAcross;

        if (wantedKind == "slider" || wantedKind == "hslider" ? ! isSlider
            : wantedKind == "switch"                          ? item.kind != Looks::Kind::button
                                                              : item.kind != Looks::Kind::knob)
            continue;

        const auto text = item.name.toLowerCase().replaceCharacters ("_-", "  ");
        int score = 0;

        for (const auto& w : words)
            score += text.contains (w) ? 1 : 0;

        found.push_back ({ &item, score });
    }

    if (found.empty())
        return done (galleryName + " has no " + wantedKind + " yet: " + instead + ".", false);

    // The best matches first (names that match none of the words still listed after them).
    std::stable_sort (found.begin(), found.end(), [] (const Found& x, const Found& y) { return x.score > y.score; });

    juce::String text;
    text << juce::String ((int) juce::jmin ((size_t) 30, found.size())) << " of " << juce::String ((int) found.size())
         << (models ? " 3D Gallery " : " gallery ") << (wantedKind == "hslider" ? juce::String ("slider") : wantedKind) << "s:\n";

    for (size_t i = 0; i < found.size() && i < 30; ++i)
    {
        const auto& item = *found[i].item;
        text << "- id " << item.id << ": \"" << item.name << "\" ("
             << (item.model.isNotEmpty() ? "3D " + item.model + ", " : juce::String()) << Looks::kindDisplayName (item.kind) << ")\n";
    }

    done (text + (models ? "Then use_gallery_look with \"3d\": true and the id of the one that suits the plugin."
                         : "Then use_gallery_look with the id of the one that suits the plugin."), false);
}

void ProjectTools::useGalleryLook (const juce::var& input, Done done)
{
    if (! project.isOpen())
        return done ("No project is open: the user has to create one (New) or open one first.", true);

    const auto id = (int) input.getProperty ("id", 0);
    const bool models = (bool) input.getProperty ("3d", false);
    const auto all = galleryEntries (models);
    const GalleryEntry* item = nullptr;

    for (const auto& candidate : all)
        if (candidate.id == id)
            item = &candidate;

    if (item == nullptr)
        return done ("There's no gallery item " + juce::String (id) + ": pick one find_gallery_looks listed.", true);

    LayerDoc doc;
    Looks::Kind kind = item->kind;
    juce::String credits, error;
    bool commercialSafe = true;

    if (! KnobStudio::readGalleryItem (item->file, doc, kind, credits, commercialSafe, error))
        return done ("Gallery item " + juce::String (id) + " couldn't be read (" + error + "): pick another.", true);

    Looks looks;
    looks.setGuiFolder (project.getGuiFolder());

    auto wanted = input.getProperty ("name", {}).toString().trim();
    wanted = (wanted.isNotEmpty() ? wanted : item->name.replaceCharacters ("_", " ").trim())
                 .removeCharacters ("\\/:*?\"<>|").substring (0, 48).trim();
    const auto name = looks.freeName (wanted.isNotEmpty() ? wanted : "Gallery " + juce::String (id));

    looks.setProjectLook (name, kind, doc, credits);

    if (looks.find (name, Looks::Origin::project) == nullptr)
        return done ("The look couldn't be saved in gui/looks.", true);

    if (onLayoutChanged != nullptr)
        onLayoutChanged();

    done ("Added the " + juce::String (doc.model3d != nullptr ? "3D " : "") + "gallery look \"" + name + "\" (" + Looks::kindDisplayName (kind)
              + ") as this plugin's own: put it on controls with \"style\": \"" + name + "\" (edit_layout)"
              + (kind == Looks::Kind::sliderAcross ? juce::String (", on sliders across (\"orientation\": \"horizontal\")") : juce::String())
              + (doc.model3d != nullptr ? juce::String (". Its settings change with make_3d_look (\"from\": \"") + name + "\")" : juce::String())
              + ".",
          false);
}

//==============================================================================
// A 3D look (Model3D): from one of the project's, or a ready-made one of a kind, its settings
// changed as given. Saved as the plugin's own look.
void ProjectTools::make3dLook (const juce::var& input, Done done)
{
    if (! project.isOpen())
        return done ("No project is open: the user has to create one (New) or open one first.", true);

    const auto name = input.getProperty ("name", {}).toString().trim().removeCharacters ("\\/:*?\"<>|").substring (0, 48).trim();

    if (name.isEmpty())
        return done ("Give the 3D look a name (\"name\"), e.g. \"Brass 3D knob\".", true);

    Looks looks;
    looks.setGuiFolder (project.getGuiFolder());

    Model3D model;
    LayerDoc doc;
    Looks::Kind lookKind = Looks::Kind::knob;
    juce::String fromLabel;

    if (const auto fromName = input.getProperty ("from", {}).toString().trim(); fromName.isNotEmpty())
    {
        const Looks::Look* from = looks.find (fromName);

        for (const auto* look : looks.all())
            if (from == nullptr && look->name.equalsIgnoreCase (fromName))
                from = look;

        if (from == nullptr || from->doc.model3d == nullptr)
        {
            juce::StringArray names;

            for (const auto* look : looks.all())
                if (look->doc.model3d != nullptr)
                    names.add (look->name);

            return done ((from == nullptr ? "There's no look called \"" + fromName + "\"" : "\"" + fromName + "\" isn't a 3D look")
                             + (names.isEmpty() ? juce::String (". There are no 3D looks yet: leave from out and give a kind.")
                                                : ". The 3D looks: " + names.joinIntoString (", ") + "."),
                         true);
        }

        model = *from->doc.model3d;
        doc = from->doc;
        lookKind = from->kind;
        fromLabel = from->name;
    }
    else
    {
        const auto kindText = input.getProperty ("kind", "knob").toString().trim().toLowerCase();
        const bool across = kindText == "hslider";
        const auto kind = kindText == "button" ? Model3D::Kind::button
                        : kindText == "toggle" || kindText == "switch" ? Model3D::Kind::toggle
                        : kindText.contains ("slider") ? Model3D::Kind::slider
                                                       : Model3D::Kind::knob;

        // A ready-made one: by its name (any case, or a part of it), else the kind's first.
        const auto presets = Model3D::presetNames (kind);
        const auto wanted = input.getProperty ("preset", {}).toString().trim();
        auto index = 0;

        if (wanted.isNotEmpty())
        {
            index = -1;

            for (int i = 0; i < presets.size() && index < 0; ++i)
                if (presets[i].equalsIgnoreCase (wanted))
                    index = i;

            for (int i = 0; i < presets.size() && index < 0; ++i)
                if (presets[i].containsIgnoreCase (wanted))
                    index = i;

            if (index < 0)
                return done ("A 3D " + Model3D::kindKey (kind) + " has no ready-made \"" + wanted + "\": " + presets.joinIntoString (", ") + ".", true);
        }

        model = Model3D::preset (kind, index);
        doc = Model3D::newDocument (model, across);
        lookKind = kind == Model3D::Kind::knob ? Looks::Kind::knob
                 : kind == Model3D::Kind::slider ? (across ? Looks::Kind::sliderAcross : Looks::Kind::slider)
                                                 : Looks::Kind::button;
        fromLabel = presets[index];
    }

    // The settings given.
    juce::StringArray changes, problems;

    if (const auto* settings = input.getProperty ("settings", {}).getDynamicObject())
    {
        for (const auto& setting : settings->getProperties())
        {
            if (const auto problem = model.setByName (setting.name.toString(), setting.value); problem.isNotEmpty())
                problems.add (problem);
            else
                changes.add (setting.name.toString() + " " + juce::JSON::toString (setting.value, true));
        }
    }

    if (! problems.isEmpty())
        return done ("Nothing made: " + problems.joinIntoString ("; ") + ". A 3D " + Model3D::kindKey (model.kind) + "'s settings:\n"
                         + Model3D::describe (model.kind),
                     true);

    doc.model3d = std::make_shared<const Model3D> (model);

    auto description = input.getProperty ("description", {}).toString().trim();

    if (description.isEmpty())
        description = "3D " + Model3D::kindNames()[(int) model.kind].toLowerCase();

    looks.setProjectLook (name, lookKind, doc, description);

    if (looks.find (name, Looks::Origin::project) == nullptr)
        return done ("The 3D look couldn't be saved in gui/looks.", true);

    if (onLayoutChanged != nullptr)
        onLayoutChanged();

    done ("Made the 3D look \"" + name + "\" (3D " + Model3D::kindKey (model.kind) + ", from \"" + fromLabel + "\")"
              + (changes.isEmpty() ? juce::String() : ": " + changes.joinIntoString ("; "))
              + ". It's this plugin's own: put it on controls with \"style\": \"" + name + "\" (edit_layout)"
              + (lookKind == Looks::Kind::sliderAcross ? juce::String (", on sliders across (\"orientation\": \"horizontal\")") : juce::String())
              + (lookKind == Looks::Kind::button ? juce::String (", on switches") : juce::String())
              + ".",
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
