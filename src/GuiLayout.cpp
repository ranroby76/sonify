// D:\workspace\Fanan_web_hub\src\GuiLayout.cpp

#include "GuiLayout.h"
#include "stella_keys.h"

namespace
{
    juce::var object()
    {
        return juce::var (new juce::DynamicObject());
    }

    void set (juce::var& target, const juce::Identifier& key, const juce::var& value)
    {
        if (auto* o = target.getDynamicObject())
            o->setProperty (key, value);
    }

    int intOf (const juce::var& v, const char* key, int fallback)
    {
        return v.hasProperty (key) ? (int) v.getProperty (key, fallback) : fallback;
    }

    /** A point as [x, y] (or { "x", "y" }); false if it isn't one. */
    bool pointOf (const juce::var& v, juce::Point<float>& point)
    {
        if (const auto* list = v.getArray(); list != nullptr && list->size() >= 2)
        {
            point = { (float) (double) list->getReference (0), (float) (double) list->getReference (1) };
            return std::isfinite (point.x) && std::isfinite (point.y);
        }

        if (v.isObject() && v.hasProperty ("x") && v.hasProperty ("y"))
        {
            point = { (float) (double) v.getProperty ("x", 0.0), (float) (double) v.getProperty ("y", 0.0) };
            return std::isfinite (point.x) && std::isfinite (point.y);
        }

        return false;
    }

    /** A line from its points in the window: "from", "to" and the "bend" it bows through (left
        out: straight). Its box holds the curve and its stroke, the points kept in it as fractions
        (the same curve as ObjectPaint draws: a quadratic through the bend point halfway). */
    bool lineFromPoints (const juce::var& item, GuiWidget& w)
    {
        juce::Point<float> a, b, m;

        if (! pointOf (item.getProperty ("from", {}), a) || ! pointOf (item.getProperty ("to", {}), b))
            return false;

        if (! pointOf (item.getProperty ("bend", item.getProperty ("through", {})), m))
            m = (a + b) * 0.5f;

        // The curve's extent: its ends, and where it turns (per axis).
        const auto c = m * 2.0f - (a + b) * 0.5f;
        auto low = juce::Point<float> (juce::jmin (a.x, b.x), juce::jmin (a.y, b.y));
        auto high = juce::Point<float> (juce::jmax (a.x, b.x), juce::jmax (a.y, b.y));

        auto extreme = [] (float p0, float p1, float p2, float& lo, float& hi)
        {
            const float denominator = p0 - 2.0f * p1 + p2;

            if (std::abs (denominator) < 1.0e-6f)
                return;

            const float t = (p0 - p1) / denominator;

            if (t > 0.0f && t < 1.0f)
            {
                const float v = (1.0f - t) * (1.0f - t) * p0 + 2.0f * t * (1.0f - t) * p1 + t * t * p2;
                lo = juce::jmin (lo, v);
                hi = juce::jmax (hi, v);
            }
        };

        extreme (a.x, c.x, b.x, low.x, high.x);
        extreme (a.y, c.y, b.y, low.y, high.y);

        const auto half = juce::jlimit (0.25f, 60.0f, w.number ("lineWidth", 4.0f) * 0.5f);
        const auto arrow = w.settingText ("arrow").trim().toLowerCase();
        const bool arrows = arrow.isNotEmpty() && arrow != "none" && arrow != "false" && arrow != "no";
        const float room = (arrows ? juce::jmax (7.0f, half * 5.0f) * 0.5f : half) + 6.0f;

        auto box = juce::Rectangle<float> (low, high).expanded (room).getSmallestIntegerContainer();
        box = box.withSizeKeepingCentre (juce::jmax (12, box.getWidth()), juce::jmax (12, box.getHeight()));
        w.bounds = box;

        const auto origin = box.getPosition().toFloat();
        const float bw = (float) box.getWidth(), bh = (float) box.getHeight();
        auto fraction = [] (float v) { return juce::roundToInt (v * 10000.0f) / 10000.0; };
        w.settings["line"] = juce::Array<juce::var> { fraction ((a.x - origin.x) / bw), fraction ((a.y - origin.y) / bh),
                                                      fraction ((m.x - origin.x) / bw), fraction ((m.y - origin.y) / bh),
                                                      fraction ((b.x - origin.x) / bw), fraction ((b.y - origin.y) / bh) };
        return true;
    }
}

//==============================================================================
juce::String GuiLayout::colourToString (juce::Colour colour)
{
    return "#" + colour.toDisplayString (true);
}

juce::Colour GuiLayout::colourFromString (const juce::String& text, juce::Colour fallback)
{
    auto hex = text.trim().trimCharactersAtStart ("#");

    if (hex.length() == 6)
        hex = "ff" + hex;

    if (hex.length() != 8 || ! hex.containsOnly ("0123456789abcdefABCDEF"))
        return fallback;

    return juce::Colour ((juce::uint32) hex.getHexValue64());
}

juce::String GuiLayout::typeName (GuiWidget::Type type)
{
    switch (type)
    {
        case GuiWidget::Type::knob:      return "knob";
        case GuiWidget::Type::slider:    return "slider";
        case GuiWidget::Type::toggle:    return "switch";
        case GuiWidget::Type::selector:  return "selector";
        case GuiWidget::Type::label:     return "label";
        case GuiWidget::Type::group:     return "group";
        case GuiWidget::Type::meter:     return "meter";
        case GuiWidget::Type::scope:     return "scope";
        case GuiWidget::Type::lamp:      return "lamp";
        case GuiWidget::Type::envelope:  return "envelope";
        case GuiWidget::Type::filter:    return "filter";
        case GuiWidget::Type::xy:        return "xy";
        case GuiWidget::Type::shape:     return "shape";
        case GuiWidget::Type::preset:    return "preset";
        case GuiWidget::Type::image:     return "image";
        case GuiWidget::Type::keyboard:  return "keyboard";
        case GuiWidget::Type::custom:    return "custom";
        case GuiWidget::Type::registration: return "registration";
        case GuiWidget::Type::tabs:      return "tabs";
    }

    return "knob";
}

juce::String GuiLayout::builtInElementFor (const juce::String& typeName)
{
    const auto n = typeName.trim().toLowerCase().removeCharacters (" -_");

    if (n == "dropdown" || n == "droplist" || n == "dropdownlist" || n == "combobox" || n == "combo" || n == "popupmenu")
        return "DropDown";

    // Controls made like programmed ones (stella_builtin_elements.cpp).
    if (n == "rangeslider" || n == "range" || n == "dualslider" || n == "doubleslider" || n == "bandslider" || n == "twohandleslider"
        || n == "hplpslider" || n == "hplp" || n == "cutslider" || n == "minmaxslider")
        return "RangeSlider";

    if (n == "meterknob" || n == "knobmeter" || n == "knobwithmeter" || n == "thresholdknob" || n == "dialmeter" || n == "meterdial")
        return "MeterKnob";

    if (n == "dualknob" || n == "concentricknob" || n == "concentricknobs" || n == "stackedknob" || n == "doubleknob" || n == "dualconcentric")
        return "DualKnob";

    // The 3D displays (stella_builtin_3d.cpp).
    if (n == "waveterrain" || n == "wave3d" || n == "wavetable3d" || n == "wavetablewaterfall" || n == "oscillator3d" || n == "osc3d")
        return "WaveTerrain";

    if (n == "morphsurface" || n == "morph3d" || n == "blend3d" || n == "mix3d")
        return "MorphSurface";

    if (n == "filtersweep" || n == "filter3d" || n == "filterwaterfall")
        return "FilterSweep";

    if (n == "soundwaterfall" || n == "waterfall" || n == "spectrum3d" || n == "spectrogram3d" || n == "scope3d")
        return "SoundWaterfall";

    return {};
}

GuiWidget::Type GuiLayout::typeFromName (const juce::String& name)
{
    const auto n = name.trim().toLowerCase();

    if (n == "slider" || n == "fader")               return GuiWidget::Type::slider;
    if (n == "switch" || n == "toggle" || n == "button") return GuiWidget::Type::toggle;
    if (n == "selector" || n == "choice")            return GuiWidget::Type::selector;
    if (n == "label" || n == "text")                 return GuiWidget::Type::label;
    if (n == "group" || n == "section" || n == "frame") return GuiWidget::Type::group;
    if (n == "meter" || n == "vu" || n == "level")   return GuiWidget::Type::meter;
    if (n == "scope" || n == "oscilloscope")         return GuiWidget::Type::scope;
    if (n == "lamp" || n == "led" || n == "light")   return GuiWidget::Type::lamp;
    if (n == "envelope" || n == "adsr")              return GuiWidget::Type::envelope;
    if (n == "filter" || n == "curve")               return GuiWidget::Type::filter;
    if (n == "xy" || n == "pad" || n == "xypad")     return GuiWidget::Type::xy;
    if (n == "shape" || n == "path" || n == "drawing" || GuiWidget::shapeKindFromName (n).isNotEmpty()) return GuiWidget::Type::shape;
    if (n == "preset" || n == "presets" || n == "program") return GuiWidget::Type::preset;
    if (n == "image" || n == "picture" || n == "bitmap" || n == "photo") return GuiWidget::Type::image;
    if (n == "keyboard" || n == "keys" || n == "piano" || n == "pianokeyboard" || n == "virtualkeyboard" || n == "midikeyboard")
        return GuiWidget::Type::keyboard;
    if (n == "registration" || n == "register" || n == "protection" || n == "copyprotection" || n == "serial" || n == "licence"
        || n == "license")
        return GuiWidget::Type::registration;
    if (n == "tabs" || n == "tab" || n == "tabbar" || n == "pages" || n == "pagetabs") return GuiWidget::Type::tabs;
    if (n.isEmpty() || n == "knob" || n == "rotary" || n == "dial")   return GuiWidget::Type::knob;

    // Anything else is a programmed element: "custom" with its "element", or its own name.
    return GuiWidget::Type::custom;
}

juce::String GuiLayout::noteName (int note)
{
    static const char* const names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    note = juce::jlimit (0, 127, note);
    return juce::String (names[note % 12]) + juce::String (note / 12 - 1);
}

int GuiLayout::noteFromVar (const juce::var& value, int fallback)
{
    if (value.isInt() || value.isInt64() || value.isDouble())
        return juce::jlimit (0, 127, juce::roundToInt ((double) value));

    const auto text = value.toString().trim().toUpperCase().replace (juce::String::fromUTF8 ("\xe2\x99\xaf"), "#")
                                                            .replace (juce::String::fromUTF8 ("\xe2\x99\xad"), "B");

    if (text.isEmpty())
        return fallback;

    if (text.containsOnly ("0123456789"))
        return juce::jlimit (0, 127, text.getIntValue());

    // A letter, then sharps or flats, then the octave (C4 = 60; "C-1" = 0).
    static const int letters[] { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
    const auto letter = text[0];

    if (letter < 'A' || letter > 'G')
        return fallback;

    int note = letters[letter - 'A'];
    int i = 1;

    for (; i < text.length(); ++i)
    {
        if (text[i] == '#')       ++note;
        else if (text[i] == 'B')  --note;
        else                      break;
    }

    const auto octave = text.substring (i);

    if (octave.isEmpty() || ! octave.trimCharactersAtStart ("-").containsOnly ("0123456789"))
        return fallback;

    return juce::jlimit (0, 127, note + (octave.getIntValue() + 1) * 12);
}

//==============================================================================
GuiLayout GuiLayout::fromVar (const juce::var& json)
{
    GuiLayout layout;

    layout.width = juce::jlimit (minWidth, maxWidth, intOf (json, "width", layout.width));
    layout.height = juce::jlimit (minHeight, maxHeight, intOf (json, "height", layout.height));

    const auto background = json.getProperty ("background", {});
    layout.backgroundTop = colourFromString (background.getProperty ("top", {}).toString(), layout.backgroundTop);
    layout.backgroundBottom = colourFromString (background.getProperty ("bottom", {}).toString(), layout.backgroundBottom);
    layout.backgroundImage = background.getProperty ("image", {}).toString().trim();

    if (const auto mode = background.getProperty ("mode", {}).toString().trim().toLowerCase(); pictureModes().contains (mode))
        layout.backgroundMode = mode;

    if (const auto light = json.getProperty ("light", {}); light.isObject())
    {
        layout.lightAngle = (float) (double) light.getProperty ("angle", layout.lightAngle);
        layout.lightAltitude = juce::jlimit (0.0f, 90.0f, (float) (double) light.getProperty ("altitude", layout.lightAltitude));
    }

    if (const auto* list = json.getProperty ("pages", {}).getArray())
        for (const auto& name : *list)
            if (const auto text = name.toString().trim(); text.isNotEmpty())
                layout.pages.addIfNotAlreadyThere (text);

    if (const auto* list = json.getProperty ("widgets", {}).getArray())
        for (const auto& item : *list)
            layout.widgets.push_back (widgetFromVar (item));

    // A background picture is a picture element covering the window, at the back (layouts
    // from before, or Sonify AI's "background": { "image" }, become one), so it can be
    // stretched and layered like any other. One that's there already takes the new picture.
    if (layout.backgroundImage.isNotEmpty())
    {
        const juce::Rectangle<int> window (0, 0, layout.width, layout.height);
        auto& widgets = layout.widgets;

        if (! widgets.empty() && widgets.front().type == GuiWidget::Type::image && widgets.front().bounds.contains (window))
        {
            widgets.front().image = layout.backgroundImage;
            widgets.front().mode = layout.backgroundMode;
        }
        else
        {
            GuiWidget backdrop;
            backdrop.type = GuiWidget::Type::image;
            backdrop.image = layout.backgroundImage;
            backdrop.mode = layout.backgroundMode;
            backdrop.bounds = window;
            widgets.insert (widgets.begin(), backdrop);
        }

        layout.backgroundImage.clear();
    }

    return layout;
}

juce::String GuiLayout::backgroundPicture() const
{
    for (const auto& w : widgets)
        if (w.type == GuiWidget::Type::image && w.image.isNotEmpty() && w.bounds.contains (juce::Rectangle<int> (0, 0, width, height)))
            return w.image;

    return {};
}

juce::var GuiLayout::toVar() const
{
    auto json = object();
    set (json, "format", 1);
    set (json, "width", width);
    set (json, "height", height);

    auto background = object();
    set (background, "top", colourToString (backgroundTop));
    set (background, "bottom", colourToString (backgroundBottom));

    if (backgroundImage.isNotEmpty())
    {
        set (background, "image", backgroundImage);
        set (background, "mode", backgroundMode);
    }

    set (json, "background", background);

    auto light = object();
    set (light, "angle", juce::roundToInt (lightAngle * 10.0f) / 10.0);
    set (light, "altitude", juce::roundToInt (lightAltitude * 10.0f) / 10.0);
    set (json, "light", light);

    if (! pages.isEmpty())
    {
        juce::Array<juce::var> names;

        for (const auto& name : pages)
            names.add (name);

        set (json, "pages", names);
    }

    juce::Array<juce::var> list;

    for (const auto& w : widgets)
        list.add (widgetToVar (w));

    set (json, "widgets", list);
    return json;
}

GuiWidget GuiLayout::widgetFromVar (const juce::var& item)
{
    GuiWidget w;
    const auto typeText = item.getProperty ("type", {}).toString().trim();
    w.type = typeFromName (typeText);
    w.param = item.getProperty ("param", {}).toString().trim();

    if (w.type == GuiWidget::Type::custom)
    {
        // { "type": "custom", "element": "DropDown" }, or { "type": "DropDown" }.
        w.element = item.getProperty ("element", {}).toString().trim();

        if (w.element.isEmpty() && ! typeText.equalsIgnoreCase ("custom") && ! typeText.equalsIgnoreCase ("element"))
            w.element = builtInElementFor (typeText).isNotEmpty() ? builtInElementFor (typeText) : typeText;

        if (auto* sources = item.getProperty ("sources", {}).getDynamicObject())
            for (const auto& source : sources->getProperties())
                if (const auto watched = source.value.toString().trim(); watched.isNotEmpty())
                    w.sources[source.name.toString()] = watched;
    }

    // Any other element's settings: under "settings" (a meter's may lie beside "type" too).
    if (w.type != GuiWidget::Type::shape)
    {
        if (auto* settings = item.getProperty ("settings", {}).getDynamicObject())
            for (const auto& setting : settings->getProperties())
                w.settings[setting.name.toString()] = setting.value;

        if (w.type == GuiWidget::Type::meter)
            for (const auto& key : GuiWidget::meterKeys())
                if (item.hasProperty (juce::Identifier (key)) && w.settings.count (key) == 0)
                    w.settings[key] = item.getProperty (juce::Identifier (key), {});

        // A lamp's, selector's, keyboard's or label's display may lie beside "type" too.
        if (w.takesPartDisplay())
            for (const auto& key : GuiWidget::partKeys())
                if (item.hasProperty (juce::Identifier (key)) && w.settings.count (key) == 0)
                    w.settings[key] = item.getProperty (juce::Identifier (key), {});
    }

    if (w.type == GuiWidget::Type::shape)
    {
        // A shape's settings lie beside "type" ({ "type": "star", "material": "gold", ... }), or
        // under "settings"; "type": "circle" (or any kind) says its kind.
        if (auto* settings = item.getProperty ("settings", {}).getDynamicObject())
            for (const auto& setting : settings->getProperties())
                w.settings[setting.name.toString()] = setting.value;

        static const std::pair<const char*, const char*> aliases[] { { "colour2", "color2" }, { "centreX", "centerX" }, { "centreY", "centerY" },
                                                                     { "patternColour", "patternColor" }, { "cornerRadius", "radius" },
                                                                     { "corners", "radius" }, { "kind", "shape" } };

        for (const auto& key : GuiWidget::shapeKeys())
            if (item.hasProperty (juce::Identifier (key)))
                w.settings[key] = item.getProperty (juce::Identifier (key), {});

        for (const auto& [alias, key] : aliases)
            if (item.hasProperty (alias) && w.settings.count (key) == 0)
                w.settings[key] = item.getProperty (alias, {});

        if (const auto kind = GuiWidget::shapeKindFromName (typeText); kind.isNotEmpty())
            w.settings["shape"] = kind;
        else if (const auto found = w.settings.find ("shape"); found != w.settings.end())
            found->second = GuiWidget::shapeKindFromName (found->second.toString());

        // "type": "arrow" is a line with an arrowhead at its end.
        if (typeText.equalsIgnoreCase ("arrow") && w.settings.count ("arrow") == 0)
            w.settings["arrow"] = "end";

        if (w.settings.count ("shape") > 0 && w.settings["shape"].toString().isEmpty())
            w.settings.erase ("shape");
    }

    // Every element: turned (degrees, clockwise) and see-through.
    w.angle = (float) std::fmod ((double) item.getProperty ("angle", item.getProperty ("rotation", 0.0)), 360.0);

    if (w.angle > 180.0f)        w.angle -= 360.0f;
    else if (w.angle <= -180.0f) w.angle += 360.0f;

    w.opacity = juce::jlimit (0.0f, 1.0f, (float) (double) item.getProperty ("opacity", 1.0));
    w.page = item.getProperty ("page", {}).toString().trim();

    w.label = item.getProperty (w.type == GuiWidget::Type::label || w.type == GuiWidget::Type::group || w.type == GuiWidget::Type::registration
                                    ? "text" : "label", {}).toString();

    if (w.label.isEmpty())
        w.label = item.getProperty ("label", item.getProperty ("text", {})).toString();

    w.style = item.getProperty ("style", item.getProperty ("look", {})).toString().trim();
    w.originalKnown = item.hasProperty ("originalStyle");
    w.originalStyle = item.getProperty ("originalStyle", {}).toString().trim();
    w.colour = colourFromString (item.getProperty ("color", item.getProperty ("colour", {})).toString(), juce::Colours::transparentBlack);
    w.fontSize = (float) (double) item.getProperty ("size", 0.0);
    w.bold = (bool) item.getProperty ("bold", false);
    w.font = item.getProperty ("font", {}).toString().trim();

    if (w.type != GuiWidget::Type::shape && item.hasProperty ("rounding"))
        w.rounding = juce::jlimit (0.0f, 1.0f, (float) (double) item.getProperty ("rounding", 0.0));
    w.vertical = item.getProperty ("orientation", "vertical").toString() != "horizontal";

    if (const auto* options = item.getProperty ("options", {}).getArray())
        for (const auto& option : *options)
            w.options.add (option.toString());

    w.source = item.getProperty ("source", {}).toString().trim();
    w.mode = item.getProperty ("mode", {}).toString().trim();
    w.threshold = (float) (double) item.getProperty ("threshold", 0.5);
    w.path = item.getProperty ("path", {}).toString();
    w.stroke = colourFromString (item.getProperty ("stroke", {}).toString(), juce::Colours::transparentBlack);
    w.strokeWidth = (float) (double) item.getProperty ("strokeWidth", 0.0);
    w.image = item.getProperty ("image", {}).toString().trim();

    if (auto* roles = item.getProperty ("params", {}).getDynamicObject())
        for (const auto& role : roles->getProperties())
            w.roles[role.name.toString()] = role.value.toString();

    if (w.type == GuiWidget::Type::keyboard)
    {
        w.lowNote = noteFromVar (item.getProperty ("low", item.getProperty ("from", {})), w.lowNote);
        w.highNote = noteFromVar (item.getProperty ("high", item.getProperty ("to", {})), w.highNote);
        stella::keys::normalise (w.lowNote, w.highNote);
    }

    const auto x = intOf (item, "x", 0), y = intOf (item, "y", 0);

    // A line given by its points in the window ("from", "to", and "bend" where it bows
    // through): its box is made round it, its points kept in it as fractions.
    if (w.type == GuiWidget::Type::shape && w.shapeKind() == "line" && lineFromPoints (item, w))
        return w;

    if (w.type == GuiWidget::Type::knob)
    {
        // A knob is given by its diameter; its caption sits under it.
        const auto size = juce::jlimit (16, 400, intOf (item, "size", intOf (item, "w", 56)));
        w.bounds = { x, y, size, size + captionHeight };
        w.fontSize = 0.0f;
    }
    else
    {
        if (w.type != GuiWidget::Type::label && w.type != GuiWidget::Type::group && w.type != GuiWidget::Type::tabs)
            w.fontSize = 0.0f;

        const auto t = w.type;
        const auto defaultW = t == GuiWidget::Type::slider ? 32 : t == GuiWidget::Type::toggle ? 48 : t == GuiWidget::Type::meter ? 18
                            : t == GuiWidget::Type::lamp ? 30 : t == GuiWidget::Type::xy ? 160 : t == GuiWidget::Type::label ? 160
                            : t == GuiWidget::Type::image ? 160 : t == GuiWidget::Type::keyboard ? 600 : t == GuiWidget::Type::custom ? 160
                            : t == GuiWidget::Type::registration ? 236 : t == GuiWidget::Type::tabs ? 300 : 200;
        const auto defaultH = t == GuiWidget::Type::slider ? 140 : t == GuiWidget::Type::toggle ? 48 : t == GuiWidget::Type::meter ? 140
                            : t == GuiWidget::Type::lamp ? 44 : t == GuiWidget::Type::xy ? 180 : t == GuiWidget::Type::label ? 28
                            : t == GuiWidget::Type::preset ? 28 : t == GuiWidget::Type::group ? 160 : t == GuiWidget::Type::keyboard ? 90
                            : t == GuiWidget::Type::custom ? 32 : t == GuiWidget::Type::registration ? 26 : t == GuiWidget::Type::tabs ? 30 : 120;
        w.bounds = { x, y, juce::jlimit (8, maxWidth, intOf (item, "w", defaultW)), juce::jlimit (8, maxHeight, intOf (item, "h", defaultH)) };
    }

    return w;
}

juce::var GuiLayout::widgetToVar (const GuiWidget& w)
{
    auto item = object();

    // The toolbox's drop-down list is a custom element the studio makes: "type": "dropdown".
    if (w.type == GuiWidget::Type::custom && w.element == builtInElementFor ("dropdown"))
    {
        set (item, "type", "dropdown");
    }
    else
    {
        set (item, "type", typeName (w.type));

        if (w.type == GuiWidget::Type::custom)
            set (item, "element", w.element);
    }

    set (item, "x", w.bounds.getX());
    set (item, "y", w.bounds.getY());

    if (w.type == GuiWidget::Type::knob)
    {
        set (item, "size", w.bounds.getWidth());
    }
    else
    {
        set (item, "w", w.bounds.getWidth());
        set (item, "h", w.bounds.getHeight());
    }

    if (w.param.isNotEmpty())
        set (item, "param", w.param);

    if (w.label.isNotEmpty())
        set (item, w.type == GuiWidget::Type::label || w.type == GuiWidget::Type::group || w.type == GuiWidget::Type::registration
                       ? "text" : "label", w.label);

    if (w.style.isNotEmpty())
        set (item, "style", w.style);

    if (w.originalKnown)
        set (item, "originalStyle", w.originalStyle);   // "" too: the standard look

    if (! w.colour.isTransparent())
        set (item, "color", colourToString (w.colour));

    if (w.fontSize > 0.0f && (w.type == GuiWidget::Type::label || w.type == GuiWidget::Type::group || w.type == GuiWidget::Type::tabs))
        set (item, "size", w.fontSize);

    if (w.bold)
        set (item, "bold", true);

    if (w.font.isNotEmpty())
        set (item, "font", w.font);

    if (w.type != GuiWidget::Type::shape && w.rounding >= 0.0f)
        set (item, "rounding", juce::roundToInt (w.rounding * 100.0f) / 100.0);

    if (w.type == GuiWidget::Type::slider && ! w.vertical)
        set (item, "orientation", "horizontal");

    if (w.type == GuiWidget::Type::selector || (w.type == GuiWidget::Type::custom && ! w.options.isEmpty()))
    {
        juce::Array<juce::var> options;

        for (const auto& o : w.options)
            options.add (o);

        set (item, "options", options);
    }

    if (w.source.isNotEmpty())       set (item, "source", w.source);
    if (w.mode.isNotEmpty())         set (item, "mode", w.mode);
    if (w.type == GuiWidget::Type::lamp) set (item, "threshold", w.threshold);
    if (w.path.isNotEmpty())         set (item, "path", w.path);
    if (! w.stroke.isTransparent())  set (item, "stroke", colourToString (w.stroke));
    if (w.strokeWidth > 0.0f)        set (item, "strokeWidth", w.strokeWidth);
    if (w.image.isNotEmpty())        set (item, "image", w.image);

    if (w.type == GuiWidget::Type::keyboard)
    {
        set (item, "low", noteName (w.lowNote));
        set (item, "high", noteName (w.highNote));
    }

    if (! w.roles.empty())
    {
        auto roles = object();

        for (const auto& [role, id] : w.roles)
            set (roles, role, id);

        set (item, "params", roles);
    }

    if (! w.sources.empty())
    {
        auto sources = object();

        for (const auto& [role, watched] : w.sources)
            set (sources, role, watched);

        set (item, "sources", sources);
    }

    if (w.type == GuiWidget::Type::shape)
    {
        // A shape's settings beside its type.
        for (const auto& [key, value] : w.settings)
            if (GuiWidget::shapeKeys().contains (key) && ! value.isVoid())
                set (item, key, value);
    }
    else if (! w.settings.empty())
    {
        auto settings = object();

        for (const auto& [key, value] : w.settings)
            set (settings, key, value);

        set (item, "settings", settings);
    }

    if (w.angle != 0.0f && w.type != GuiWidget::Type::registration)
        set (item, "angle", juce::roundToInt (w.angle * 10.0f) / 10.0);

    if (w.opacity < 1.0f && w.type != GuiWidget::Type::registration)
        set (item, "opacity", juce::roundToInt (w.opacity * 100.0f) / 100.0);

    if (w.page.isNotEmpty())
        set (item, "page", w.page);

    return item;
}

//==============================================================================
bool GuiLayout::isPicture (const juce::File& file)
{
    return file.existsAsFile() && file.hasFileExtension (pictureFiles().removeCharacters ("*"));
}

juce::StringArray GuiLayout::paintedPictures (const juce::File& guiFolder)
{
    juce::StringArray names;

    const auto parsed = juce::JSON::parse (guiFolder.getChildFile (paintedListName));   // kept: the list lives in it

    if (const auto* list = parsed.getArray())
        for (const auto& entry : *list)
            names.addIfNotAlreadyThere (entry.isObject() ? entry.getProperty ("file", {}).toString() : entry.toString());

    names.removeEmptyStrings();
    return names;
}

juce::var GuiLayout::paintedRecipe (const juce::File& guiFolder, const juce::String& pictureName)
{
    const auto parsed = juce::JSON::parse (guiFolder.getChildFile (paintedListName));   // kept: the list lives in it

    if (const auto* list = parsed.getArray())
        for (const auto& entry : *list)
            if (entry.isObject() && entry.getProperty ("file", {}).toString() == pictureName)
                return entry;

    return {};
}

void GuiLayout::rememberPainted (const juce::File& guiFolder, const juce::String& pictureName, const juce::var& recipe)
{
    juce::Array<juce::var> list;

    const auto parsed = juce::JSON::parse (guiFolder.getChildFile (paintedListName));   // kept: the list lives in it

    if (const auto* old = parsed.getArray())
        for (const auto& entry : *old)
            if ((entry.isObject() ? entry.getProperty ("file", {}).toString() : entry.toString()) != pictureName)
                list.add (entry);

    auto entry = recipe.isObject() ? recipe.clone() : juce::var (new juce::DynamicObject());
    entry.getDynamicObject()->setProperty ("file", pictureName);
    entry.getDynamicObject()->removeProperty ("quiet");
    list.add (entry);

    guiFolder.getChildFile (paintedListName).replaceWithText (juce::JSON::toString (juce::var (list)));
}

juce::String GuiLayout::importPicture (const juce::File& picture, const juce::File& imagesFolderFile)
{
    if (! isPicture (picture) || ! imagesFolderFile.createDirectory())
        return {};

    // Already in the folder: nothing to copy.
    if (picture.getParentDirectory() == imagesFolderFile)
        return picture.getFileName();

    const auto base = juce::File::createLegalFileName (picture.getFileNameWithoutExtension()).trim();
    const auto extension = picture.getFileExtension().toLowerCase();

    for (int n = 1; n < 1000; ++n)
    {
        const auto name = (base.isNotEmpty() ? base : juce::String ("picture")) + (n > 1 ? " " + juce::String (n) : juce::String()) + extension;
        const auto target = imagesFolderFile.getChildFile (name);

        if (target.existsAsFile())
        {
            if (target.getSize() == picture.getSize() && target.hasIdenticalContentTo (picture))
                return name;   // the same picture again

            continue;
        }

        return picture.copyFileTo (target) ? name : juce::String();
    }

    return {};
}

juce::Result GuiLayout::load (const juce::File& file, GuiLayout& result)
{
    if (! file.existsAsFile())
        return juce::Result::fail ("No layout yet");

    juce::var json;

    if (const auto parsed = juce::JSON::parse (file.loadFileAsString(), json); parsed.failed() || ! json.isObject())
        return juce::Result::fail ("gui/layout.json isn't valid JSON");

    result = fromVar (json);
    return juce::Result::ok();
}

juce::Result GuiLayout::save (const juce::File& file) const
{
    file.getParentDirectory().createDirectory();

    juce::TemporaryFile temp (file);

    if (! temp.getFile().replaceWithText (juce::JSON::toString (toVar())) || ! temp.overwriteTargetFileWithTemporary())
        return juce::Result::fail ("Couldn't save " + file.getFullPathName());

    return juce::Result::ok();
}

//==============================================================================
GuiLayout GuiLayout::makeDefault (const juce::Array<ParamInfo>& params, const juce::String& title)
{
    // One titled group per module, its controls in rows of up to four: plain, tidy, and a
    // starting point for Sonify AI or the user to reshape. The controls take the default looks.
    GuiLayout layout;

    constexpr int margin = 20, knob = 52, cell = 76, rowH = knob + captionHeight + 18, perRow = 4, titleH = 44;
    constexpr int widest = 1100;

    GuiWidget heading;
    heading.type = GuiWidget::Type::label;
    heading.label = title.toUpperCase();
    heading.fontSize = 22.0f;
    heading.bold = true;
    heading.bounds = { margin, 12, 600, 30 };
    layout.widgets.push_back (heading);

    juce::StringArray modules;

    for (const auto& p : params)
        modules.addIfNotAlreadyThere (p.module);

    int x = margin, y = titleH + 8, rowBottom = y, right = 0;

    for (const auto& module : modules)
    {
        juce::Array<ParamInfo> mine;

        for (const auto& p : params)
            if (p.module == module)
                mine.add (p);

        const auto columns = juce::jmin (perRow, mine.size());
        const auto rows = (mine.size() + perRow - 1) / perRow;
        const auto groupW = columns * cell + 16;
        const auto groupH = rows * rowH + 34;

        if (x + groupW > widest && x > margin)
        {
            x = margin;
            y = rowBottom + 16;
        }

        GuiWidget group;
        group.type = GuiWidget::Type::group;
        group.label = module.toUpperCase();
        group.bounds = { x, y, groupW, groupH };
        layout.widgets.push_back (group);

        for (int i = 0; i < mine.size(); ++i)
        {
            const auto& p = mine.getReference (i);
            const auto cx = x + 8 + (i % perRow) * cell;
            const auto cy = y + 28 + (i / perRow) * rowH;

            GuiWidget w;
            w.param = p.id;
            w.label = p.name;

            // An on/off parameter (0..1, starting at 0 or 1, no unit) gets a switch.
            auto near = [] (float a, float b) { return std::abs (a - b) < 1.0e-6f; };
            const bool isSwitch = near (p.min, 0.0f) && near (p.max, 1.0f) && p.unit.isEmpty() && (near (p.def, 0.0f) || near (p.def, 1.0f));

            if (isSwitch)
            {
                w.type = GuiWidget::Type::toggle;
                w.bounds = { cx + (cell - 48) / 2, cy + 6, 48, knob - 6 + captionHeight };
            }
            else
            {
                w.type = GuiWidget::Type::knob;
                w.bounds = { cx + (cell - knob) / 2, cy, knob, knob + captionHeight };
            }

            layout.widgets.push_back (w);
        }

        x += groupW + 14;
        right = juce::jmax (right, x);
        rowBottom = juce::jmax (rowBottom, y + groupH);
    }

    layout.width = juce::jlimit (420, 1200, right - 14 + margin);
    layout.height = juce::jlimit (240, 800, rowBottom + margin);
    return layout;
}
