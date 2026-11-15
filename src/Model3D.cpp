// D:\workspace\Fanan_web_hub\src\Model3D.cpp

#include "Model3D.h"
#include "LayerDoc.h"

namespace
{
    using P = Model3D::Param;
    using T = Model3D::Param::Type;

    P num (const char* key, const char* label, const char* group, float min, float max, float step, float def,
           const char* suffix = "", const char* tip = "", const char* shownWhen = "")
    {
        P p;
        p.key = key; p.label = label; p.group = group; p.type = T::number;
        p.min = min; p.max = max; p.step = step; p.def = def;
        p.suffix = suffix; p.tip = tip; p.shownWhen = shownWhen;
        return p;
    }

    P pick (const char* key, const char* label, const char* group, juce::StringArray choices, int def,
            const char* tip = "", const char* shownWhen = "")
    {
        P p;
        p.key = key; p.label = label; p.group = group; p.type = T::choice;
        p.choices = std::move (choices); p.def = (float) def; p.min = 0.0f; p.max = (float) (p.choices.size() - 1);
        p.tip = tip; p.shownWhen = shownWhen;
        return p;
    }

    P col (const char* key, const char* label, const char* group, juce::uint32 def, const char* tip = "", const char* shownWhen = "")
    {
        P p;
        p.key = key; p.label = label; p.group = group; p.type = T::colour; p.colour = def;
        p.tip = tip; p.shownWhen = shownWhen;
        return p;
    }

    P tick (const char* key, const char* label, const char* group, bool def, const char* tip = "", const char* shownWhen = "")
    {
        P p;
        p.key = key; p.label = label; p.group = group; p.type = T::flag; p.def = def ? 1.0f : 0.0f;
        p.tip = tip; p.shownWhen = shownWhen;
        return p;
    }

    P words (const char* key, const char* label, const char* group, const char* def, const char* tip = "", const char* shownWhen = "")
    {
        P p;
        p.key = key; p.label = label; p.group = group; p.type = T::text; p.text = def;
        p.tip = tip; p.shownWhen = shownWhen;
        return p;
    }

    const juce::StringArray& materials()
    {
        static const juce::StringArray names { "Gloss plastic", "Satin plastic", "Matte plastic", "Rubber", "Bakelite",
                                               "Brushed aluminium", "Spun aluminium", "Chrome", "Steel", "Gold", "Brass",
                                               "Copper", "Anodized", "Painted metal" };
        return names;
    }

    /** The view and light rows every kind has. */
    void addView (std::vector<P>& list)
    {
        list.push_back (num ("tilt", "Seen from (0 straight above)", "View", 0.0f, 60.0f, 1.0f, 0.0f, " deg",
                             "Where the camera is: 0 straight above (as a plugin's controls are seen), up to 60 from in front, "
                             "the sides of the control showing"));
        list.push_back (num ("perspective", "Perspective", "View", 0.0f, 100.0f, 1.0f, 30.0f, " %",
                             "How much nearer parts look bigger: 0 none, 100 close up"));
        list.push_back (num ("shadow", "Shadow", "View", 0.0f, 100.0f, 1.0f, 50.0f, " %",
                             "How dark the shadow it casts on the panel is (lit by the panel's global light)"));
        list.push_back (num ("softness", "Shadow softness", "View", 0.0f, 100.0f, 1.0f, 55.0f, " %"));
        list.push_back (num ("exposure", "Exposure", "View", -100.0f, 100.0f, 1.0f, 0.0f, " %",
                             "Brighter or darker overall"));
    }

    std::vector<P> knobParams()
    {
        std::vector<P> l;
        const auto& m = materials();
        const juce::StringArray tops { "Flat", "Dome", "Dish" };

        l.push_back (num ("diameter", "Diameter", "Shape", 20.0f, 96.0f, 1.0f, 66.0f, " %", "Of the canvas (the skirt goes past it)"));
        l.push_back (num ("height", "Height", "Shape", 5.0f, 200.0f, 1.0f, 30.0f, " %",
                          "Of its diameter: a flat disc to a tall cylinder (100: as tall as it's wide; up to 200). With Taper, a tall cone"));
        l.push_back (num ("taper", "Taper", "Shape", -30.0f, 90.0f, 1.0f, 6.0f, " %",
                          "Its top narrower than its foot (minus: wider): 0 a cylinder, 40 a cone with a broad top, 90 a cone almost to a point"));
        l.push_back (num ("edge", "Edge rounding", "Shape", 0.0f, 45.0f, 1.0f, 14.0f, " %", "Its top edge rounded, as a share of its height"));
        l.push_back (pick ("top", "Top", "Shape", tops, 0, "A flat top, a dome or a dish"));
        l.push_back (num ("topDepth", "Dome or dish depth", "Shape", 0.0f, 40.0f, 1.0f, 8.0f, " %", "", "top!=0"));

        l.push_back (tick ("skirt", "Skirt", "Skirt", true, "A wider foot round its base"));
        l.push_back (num ("skirtDiameter", "Skirt diameter", "Skirt", 100.0f, 150.0f, 1.0f, 122.0f, " %", "Of the knob's", "skirt"));
        l.push_back (num ("skirtHeight", "Skirt height", "Skirt", 5.0f, 70.0f, 1.0f, 24.0f, " %", "Of the knob's height", "skirt"));
        l.push_back (num ("skirtBevel", "Skirt bevel", "Skirt", 0.0f, 100.0f, 1.0f, 55.0f, " %", "How far its top slopes down to its rim", "skirt"));
        l.push_back (pick ("skirtMaterial", "Skirt material", "Skirt", m, 0, "", "skirt"));
        l.push_back (col ("skirtColour", "Skirt colour", "Skirt", 0xff1a1a1d, "", "skirt"));
        l.push_back (tick ("skirtLine", "Line on the skirt", "Skirt", false, "The pointer goes on over the skirt too", "skirt"));

        l.push_back (pick ("grip", "Grip", "Grip", { "Smooth", "Flutes", "Knurled", "Ribs" }, 0, "Its side: smooth, fluted, knurled or ribbed"));
        l.push_back (num ("gripCount", "How many", "Grip", 6.0f, 120.0f, 1.0f, 28.0f, "", "", "grip!=0"));
        l.push_back (num ("gripDepth", "Depth", "Grip", 0.0f, 12.0f, 0.5f, 3.0f, " %", "Of its radius", "grip!=0"));

        l.push_back (tick ("cap", "Cap insert", "Top cap", false, "A disc of another material set into its top"));
        l.push_back (num ("capDiameter", "Cap diameter", "Top cap", 20.0f, 95.0f, 1.0f, 72.0f, " %", "Of the top", "cap"));
        l.push_back (pick ("capMaterial", "Cap material", "Top cap", m, 6, "", "cap"));
        l.push_back (col ("capColour", "Cap colour", "Top cap", 0xffc9ccd2, "", "cap"));
        l.push_back (tick ("capGroove", "Groove round it", "Top cap", true, "", "cap"));

        l.push_back (pick ("pointer", "Pointer", "Pointer", { "None", "Line", "Dot", "Groove", "Ridge" }, 1,
                           "Painted (a line, a dot), cut in (a groove) or standing up (a ridge)"));
        l.push_back (col ("pointerColour", "Pointer colour", "Pointer", 0xfff4f1ea, "", "pointer!=0"));
        l.push_back (num ("pointerLength", "Length", "Pointer", 10.0f, 100.0f, 1.0f, 62.0f, " %", "Of the top's radius", "pointer!=0"));
        l.push_back (num ("pointerWidth", "Width", "Pointer", 2.0f, 30.0f, 0.5f, 8.0f, " %", "Of the top's radius", "pointer!=0"));
        l.push_back (tick ("pointerSide", "Down its side too", "Pointer", false,
                           "The line (or groove) runs on down its side as well: for tall cylinders and cones, whose side shows more than their top",
                           "pointer!=0"));
        l.push_back (num ("pointerY", "Y position", "Pointer", -50.0f, 50.0f, 1.0f, 0.0f, " %",
                          "Moves the pointer nearer the knob's edge (up to +50) or its centre (down to -50), in % of the top's radius",
                          "pointer!=0"));

        l.push_back (pick ("material", "Material", "Body", m, 0));
        l.push_back (col ("colour", "Colour", "Body", 0xff202024));

        l.push_back (tick ("ring", "Light ring", "Light ring", false, "LEDs round it on the panel, lit up to its value"));
        l.push_back (col ("ringColour", "Light colour", "Light ring", 0xff2f8cff, "", "ring"));
        l.push_back (num ("ringSegments", "LEDs (0 a solid band)", "Light ring", 0.0f, 41.0f, 1.0f, 0.0f, "", "", "ring"));
        l.push_back (num ("ringWidth", "Ring width", "Light ring", 2.0f, 25.0f, 0.5f, 7.0f, " %", "Of the knob's radius", "ring"));
        l.push_back (num ("ringGap", "Gap", "Light ring", 0.0f, 30.0f, 0.5f, 5.0f, " %", "Between it and the knob", "ring"));
        l.push_back (pick ("ringFrom", "Lights from", "Light ring", { "Its start", "The middle" }, 0, "From the start of its turn, or both ways from the middle (pan, balance)", "ring"));

        l.push_back (num ("turnFrom", "Turns from", "Movement", -180.0f, 0.0f, 1.0f, -150.0f, " deg", "Its first frame, from 12 o'clock"));
        l.push_back (num ("turnTo", "Turns to", "Movement", 0.0f, 180.0f, 1.0f, 150.0f, " deg", "Its last frame"));

        addView (l);
        return l;
    }

    std::vector<P> buttonParams()
    {
        std::vector<P> l;
        const auto& m = materials();

        l.push_back (pick ("shape", "Shape", "Shape", { "Round", "Rounded square", "Square", "Pill", "Rounded rectangle" }, 1));
        l.push_back (num ("width", "Width", "Shape", 20.0f, 96.0f, 1.0f, 66.0f, " %", "Of the canvas's width"));
        l.push_back (num ("depth", "Depth (top to bottom)", "Shape", 20.0f, 96.0f, 1.0f, 66.0f, " %", "Of the canvas's height (round and square: as wide)"));
        l.push_back (num ("corner", "Corner rounding", "Shape", 0.0f, 100.0f, 1.0f, 30.0f, " %", "", "shape=4"));
        l.push_back (num ("height", "Height", "Shape", 3.0f, 50.0f, 1.0f, 20.0f, " %", "Of its width"));
        l.push_back (num ("edge", "Edge rounding", "Shape", 0.0f, 60.0f, 1.0f, 30.0f, " %", "Of its height"));
        l.push_back (pick ("top", "Top", "Shape", { "Flat", "Convex", "Concave" }, 0));
        l.push_back (num ("topDepth", "Bulge or dip", "Shape", 0.0f, 30.0f, 1.0f, 6.0f, " %", "", "top!=0"));
        l.push_back (pick ("material", "Material", "Shape", m, 0));
        l.push_back (col ("colour", "Colour", "Shape", 0xff26262a));
        l.push_back (num ("travel", "Goes down by", "Shape", 0.0f, 70.0f, 1.0f, 35.0f, " %", "Pressed (its second frame): of its height"));

        l.push_back (tick ("bezel", "Bezel", "Bezel", true, "A frame round it"));
        l.push_back (num ("bezelWidth", "Bezel width", "Bezel", 2.0f, 30.0f, 0.5f, 8.0f, " %", "Of its width", "bezel"));
        l.push_back (num ("bezelHeight", "Bezel height", "Bezel", 2.0f, 60.0f, 1.0f, 40.0f, " %", "Of its height", "bezel"));
        l.push_back (pick ("bezelMaterial", "Bezel material", "Bezel", m, 8, "", "bezel"));
        l.push_back (col ("bezelColour", "Bezel colour", "Bezel", 0xff9ea2a8, "", "bezel"));

        l.push_back (pick ("light", "Light", "Light", { "None", "Dot", "Bar", "Ring round it", "The whole cap" }, 1,
                           "What lights up when it's on (its second frame)"));
        l.push_back (pick ("lightPlace", "Where", "Light", { "Top", "Middle", "Left" }, 0, "", "light=1"));
        l.push_back (col ("lightColour", "Light colour", "Light", 0xffffc21a, "", "light!=0"));
        l.push_back (num ("lightOff", "Off, it shows", "Light", 0.0f, 60.0f, 1.0f, 18.0f, " %", "How much of its colour shows while it's off", "light!=0"));

        l.push_back (words ("label", "Label", "Label", "", "Words printed on its top"));
        l.push_back (col ("labelColour", "Label colour", "Label", 0xffeae6dc));
        l.push_back (num ("labelSize", "Label size", "Label", 10.0f, 90.0f, 1.0f, 36.0f, " %", "Of its depth"));

        addView (l);
        return l;
    }

    std::vector<P> sliderParams()
    {
        std::vector<P> l;
        const auto& m = materials();

        l.push_back (num ("trackLength", "Track length", "Track", 40.0f, 98.0f, 1.0f, 84.0f, " %", "Of the canvas's length"));
        l.push_back (num ("trackWidth", "Track width", "Track", 2.0f, 40.0f, 0.5f, 9.0f, " %", "Of the canvas's width"));
        l.push_back (col ("trackColour", "Track colour", "Track", 0xff08080a));
        l.push_back (num ("ticks", "Scale marks (0 none)", "Track", 0.0f, 31.0f, 1.0f, 11.0f));
        l.push_back (num ("tickLength", "Mark length", "Track", 4.0f, 45.0f, 0.5f, 16.0f, " %", "Of the canvas's width", "ticks!=0"));
        l.push_back (col ("tickColour", "Mark colour", "Track", 0xffbdbdc2, "", "ticks!=0"));

        l.push_back (pick ("cap", "Cap", "Cap", { "Classic fader", "Block", "Round" }, 0));
        l.push_back (num ("capWidth", "Cap width", "Cap", 20.0f, 100.0f, 1.0f, 74.0f, " %", "Of the canvas's width"));
        l.push_back (num ("capLength", "Cap length", "Cap", 4.0f, 50.0f, 0.5f, 14.0f, " %", "Of the canvas's length", "cap!=2"));
        l.push_back (num ("capHeight", "Cap height", "Cap", 3.0f, 60.0f, 1.0f, 26.0f, " %", "Of the canvas's width"));
        l.push_back (num ("grooves", "Grooves", "Cap", 0.0f, 12.0f, 1.0f, 4.0f, "", "", "cap=0"));
        l.push_back (pick ("material", "Cap material", "Cap", m, 0));
        l.push_back (col ("colour", "Cap colour", "Cap", 0xff2a2a2e));
        l.push_back (col ("lineColour", "Line across it", "Cap", 0xfff4f1ea, "Its centre line (see-through: none)"));

        addView (l);
        return l;
    }

    std::vector<P> toggleParams()
    {
        std::vector<P> l;
        const auto& m = materials();

        l.push_back (pick ("base", "Nut", "Base", { "Hex nut", "Round nut", "Plate" }, 0));
        l.push_back (num ("baseDiameter", "Nut size", "Base", 20.0f, 90.0f, 1.0f, 52.0f, " %", "Of the canvas's width"));
        l.push_back (pick ("baseMaterial", "Nut material", "Base", m, 7));
        l.push_back (col ("baseColour", "Nut colour", "Base", 0xffd8dbe0));

        l.push_back (num ("leverLength", "Lever length", "Lever", 15.0f, 60.0f, 1.0f, 38.0f, " %", "Of the canvas's height"));
        l.push_back (num ("leverWidth", "Lever thickness", "Lever", 3.0f, 25.0f, 0.5f, 11.0f, " %", "Of the canvas's width"));
        l.push_back (pick ("tip", "Tip", "Lever", { "Plain", "Ball", "Paddle" }, 0));
        l.push_back (num ("angle", "Tilt", "Lever", 10.0f, 50.0f, 1.0f, 30.0f, " deg", "How far it leans up or down"));
        l.push_back (pick ("leverMaterial", "Lever material", "Lever", m, 7));
        l.push_back (col ("leverColour", "Lever colour", "Lever", 0xffe2e5ea));

        addView (l);
        return l;
    }

    juce::Identifier id (const juce::String& key)    { return juce::Identifier (key); }
}

//==============================================================================
const std::vector<Model3D::Param>& Model3D::paramsFor (Kind kind)
{
    static const std::vector<Param> knob = knobParams(), button = buttonParams(), slider = sliderParams(), toggle = toggleParams();

    switch (kind)
    {
        case Kind::button: return button;
        case Kind::slider: return slider;
        case Kind::toggle: return toggle;
        case Kind::knob:
        default:           return knob;
    }
}

const Model3D::Param* Model3D::findParam (Kind kind, const juce::String& key)
{
    for (const auto& p : paramsFor (kind))
        if (p.key == key)
            return &p;

    return nullptr;
}

float Model3D::number (const juce::String& key) const
{
    const auto* p = findParam (kind, key);
    const auto fallback = p != nullptr ? p->def : 0.0f;

    if (const auto* v = values.getVarPointer (id (key)); v != nullptr && ! v->isVoid())
    {
        const auto value = (float) (double) *v;
        return p != nullptr && p->type == Param::Type::number ? juce::jlimit (p->min, p->max, value) : value;
    }

    return fallback;
}

int Model3D::choice (const juce::String& key) const
{
    const auto* p = findParam (kind, key);
    const auto count = p != nullptr ? p->choices.size() : 1;
    return juce::jlimit (0, juce::jmax (0, count - 1), juce::roundToInt (number (key)));
}

bool Model3D::flag (const juce::String& key) const
{
    return number (key) >= 0.5f;
}

juce::Colour Model3D::colour (const juce::String& key) const
{
    const auto* p = findParam (kind, key);
    const juce::Colour fallback (p != nullptr ? p->colour : 0xff808080);

    if (const auto* v = values.getVarPointer (id (key)); v != nullptr && v->toString().isNotEmpty())
    {
        auto hex = v->toString().trim().trimCharactersAtStart ("#");

        if (hex.length() == 6)
            hex = "ff" + hex;

        if (hex.length() == 8 && hex.containsOnly ("0123456789abcdefABCDEF"))
            return juce::Colour ((juce::uint32) hex.getHexValue64());
    }

    return fallback;
}

juce::String Model3D::text (const juce::String& key) const
{
    const auto* p = findParam (kind, key);

    if (const auto* v = values.getVarPointer (id (key)); v != nullptr)
        return v->toString();

    return p != nullptr ? p->text : juce::String();
}

void Model3D::set (const juce::String& key, const juce::var& value)
{
    values.set (id (key), value);
}

juce::String Model3D::setByName (const juce::String& key, const juce::var& value)
{
    const auto* p = findParam (kind, key.trim());

    if (p == nullptr)
        return "a " + kindNames()[(int) kind].toLowerCase() + " has no setting \"" + key + "\"";

    switch (p->type)
    {
        case Param::Type::choice:
        {
            auto index = -1;

            if (value.isString() && ! value.toString().trim().containsOnly ("0123456789"))
            {
                const auto wanted = value.toString().trim();

                for (int i = 0; i < p->choices.size() && index < 0; ++i)
                    if (p->choices[i].equalsIgnoreCase (wanted))
                        index = i;

                for (int i = 0; i < p->choices.size() && index < 0; ++i)   // "brushed" for "Brushed aluminium"
                    if (p->choices[i].containsIgnoreCase (wanted))
                        index = i;
            }
            else
            {
                index = value.isString() ? value.toString().getIntValue() : (int) value;
            }

            if (! juce::isPositiveAndBelow (index, p->choices.size()))
                return "\"" + key + "\" is one of: " + p->choices.joinIntoString (", ");

            set (p->key, index);
            return {};
        }

        case Param::Type::flag:
        {
            const auto text = value.toString().trim().toLowerCase();
            set (p->key, value.isBool() ? (bool) value : (text == "true" || text == "yes" || text == "on" || text == "1"));
            return {};
        }

        case Param::Type::colour:
        {
            auto hex = value.toString().trim().trimCharactersAtStart ("#");

            if (hex.length() == 6)
                hex = "ff" + hex;

            if (hex.length() != 8 || ! hex.containsOnly ("0123456789abcdefABCDEF"))
                return "\"" + key + "\" is a colour, \"#AARRGGBB\"";

            set (p->key, "#" + hex.toUpperCase());
            return {};
        }

        case Param::Type::text:
            set (p->key, value.toString());
            return {};

        case Param::Type::number:
        default:
        {
            const auto number = value.isString() ? value.toString().getDoubleValue() : (double) value;
            set (p->key, juce::jlimit ((double) p->min, (double) p->max, number));
            return {};
        }
    }
}

juce::String Model3D::describe (Kind kind)
{
    juce::String text, group;

    for (const auto& p : paramsFor (kind))
    {
        if (p.group != group)
        {
            group = p.group;
            text << group << ":\n";
        }

        text << "- " << p.key << " (" << p.label << "): ";

        switch (p.type)
        {
            case Param::Type::choice:  text << "one of " << p.choices.joinIntoString (", ") << "; default " << p.choices[juce::roundToInt (p.def)]; break;
            case Param::Type::flag:    text << "true or false; default " << (p.def > 0.5f ? "true" : "false"); break;
            case Param::Type::colour:  text << "a colour; default #" << juce::String::toHexString ((int) p.colour).paddedLeft ('0', 8).toUpperCase(); break;
            case Param::Type::text:    text << "words; default \"" << p.text << "\""; break;
            case Param::Type::number:
            default:                   text << juce::String (p.min, p.step < 1.0f ? 1 : 0) << " to " << juce::String (p.max, p.step < 1.0f ? 1 : 0)
                                            << p.suffix << "; default " << juce::String (p.def, p.step < 1.0f ? 1 : 0); break;
        }

        if (p.shownWhen.isNotEmpty())
            text << "; only when " << p.shownWhen;

        if (p.tip.isNotEmpty())
            text << ". " << p.tip;

        text << "\n";
    }

    return text;
}

bool Model3D::isShown (const Param& p) const
{
    if (p.shownWhen.isEmpty())
        return true;

    if (p.shownWhen.contains ("!="))
        return juce::roundToInt (number (p.shownWhen.upToFirstOccurrenceOf ("!=", false, false)))
            != p.shownWhen.fromFirstOccurrenceOf ("!=", false, false).getIntValue();

    if (p.shownWhen.contains ("="))
        return juce::roundToInt (number (p.shownWhen.upToFirstOccurrenceOf ("=", false, false)))
            == p.shownWhen.fromFirstOccurrenceOf ("=", false, false).getIntValue();

    return flag (p.shownWhen);
}

//==============================================================================
juce::StringArray Model3D::kindNames()   { return { "Knob", "Button", "Slider", "Toggle switch" }; }

juce::String Model3D::kindKey (Kind kind)
{
    switch (kind)
    {
        case Kind::button: return "button";
        case Kind::slider: return "slider";
        case Kind::toggle: return "toggle";
        case Kind::knob:
        default:           return "knob";
    }
}

Model3D::Kind Model3D::kindFromKey (const juce::String& key)
{
    const auto k = key.trim().toLowerCase();

    if (k == "button" || k == "push" || k == "pushbutton") return Kind::button;
    if (k == "slider" || k == "fader")                     return Kind::slider;
    if (k == "toggle" || k == "lever" || k == "switch")    return Kind::toggle;
    return Kind::knob;
}

const juce::StringArray& Model3D::materialNames()
{
    return materials();
}

//==============================================================================
juce::StringArray Model3D::presetNames (Kind kind)
{
    switch (kind)
    {
        case Kind::button: return { "Black cap with a light", "Keycap with a label", "Round chrome", "Rubber pad", "Arcade (lit cap)", "Pill with a light bar" };
        case Kind::slider: return { "Console fader", "Black block", "Round cap" };
        case Kind::toggle: return { "Chrome lever", "Black ball lever", "Paddle on a plate" };
        case Kind::knob:
        default:           return { "Black skirted", "Spun aluminium", "Chrome cap on black", "Soft rubber", "Vintage bakelite",
                                    "Gold fluted", "Knurled steel", "Blue light ring" };
    }
}

Model3D Model3D::preset (Kind kind, int index)
{
    Model3D m;
    m.kind = kind;

    auto set = [&m] (const char* key, juce::var v) { m.set (key, v); };

    switch (kind)
    {
        case Kind::knob:
            switch (index)
            {
                case 1:   // Spun aluminium
                    set ("material", 6); set ("colour", "#FFC8CBD0"); set ("skirt", false); set ("height", 22); set ("edge", 10);
                    set ("grip", 0); set ("pointer", 3); set ("pointerColour", "#FF202024");
                    break;
                case 2:   // Chrome cap on black
                    set ("cap", true); set ("capMaterial", 7); set ("capColour", "#FFE4E7EC"); set ("capDiameter", 64);
                    set ("grip", 1); set ("gripCount", 24); set ("pointer", 1); set ("top", 0);
                    break;
                case 3:   // Soft rubber
                    set ("material", 3); set ("colour", "#FF2A2B2E"); set ("skirt", false); set ("height", 34); set ("edge", 32);
                    set ("grip", 3); set ("gripCount", 36); set ("gripDepth", 2.5); set ("pointer", 2); set ("pointerColour", "#FFFF6A2A");
                    break;
                case 4:   // Vintage bakelite
                    set ("material", 4); set ("colour", "#FF2B1A12"); set ("skirtMaterial", 4); set ("skirtColour", "#FF2B1A12");
                    set ("grip", 1); set ("gripCount", 18); set ("gripDepth", 5); set ("taper", 14); set ("pointer", 1);
                    set ("pointerColour", "#FFE8DCC0"); set ("skirtLine", true);
                    break;
                case 5:   // Gold fluted
                    set ("material", 9); set ("colour", "#FFE0B860"); set ("skirtMaterial", 9); set ("skirtColour", "#FFE0B860");
                    set ("grip", 1); set ("gripCount", 30); set ("gripDepth", 4); set ("top", 1); set ("topDepth", 6);
                    set ("pointer", 3);
                    break;
                case 6:   // Knurled steel
                    set ("material", 8); set ("colour", "#FFB9BDC3"); set ("skirt", false); set ("grip", 2); set ("gripCount", 60);
                    set ("gripDepth", 2.5); set ("height", 36); set ("edge", 8); set ("pointer", 3);
                    break;
                case 7:   // Blue light ring
                    set ("ring", true); set ("ringColour", "#FF2F8CFF"); set ("ringSegments", 0); set ("top", 1); set ("topDepth", 10);
                    set ("diameter", 58); set ("grip", 1); set ("gripCount", 32); set ("gripDepth", 2);
                    break;
                case 0:
                default:  // Black skirted
                    set ("grip", 1); set ("gripCount", 28); set ("gripDepth", 2.5);
                    break;
            }
            break;

        case Kind::button:
            switch (index)
            {
                case 1:   // Keycap with a label
                    set ("shape", 1); set ("bezel", false); set ("light", 0); set ("top", 2); set ("topDepth", 5); set ("label", "1");
                    set ("colour", "#FF2C2C30"); set ("height", 26); set ("edge", 24);
                    break;
                case 2:   // Round chrome
                    set ("shape", 0); set ("material", 7); set ("colour", "#FFE2E5EA"); set ("top", 1); set ("bezel", true);
                    set ("bezelMaterial", 2); set ("bezelColour", "#FF151517"); set ("light", 3); set ("lightColour", "#FF2FD0FF");
                    break;
                case 3:   // Rubber pad
                    set ("shape", 1); set ("material", 3); set ("colour", "#FF3A3B40"); set ("bezel", false); set ("edge", 55);
                    set ("light", 4); set ("lightColour", "#FFFF3B30"); set ("lightOff", 10);
                    break;
                case 4:   // Arcade (lit cap)
                    set ("shape", 0); set ("material", 0); set ("colour", "#FFFFCC22"); set ("top", 2); set ("topDepth", 10);
                    set ("bezel", true); set ("bezelMaterial", 0); set ("bezelColour", "#FF141416"); set ("light", 4);
                    set ("lightColour", "#FFFFD84A"); set ("lightOff", 45);
                    break;
                case 5:   // Pill with a light bar
                    set ("shape", 3); set ("depth", 36); set ("width", 80); set ("light", 2); set ("lightColour", "#FF3AF06A");
                    break;
                case 0:
                default:  // Black cap with a light
                    break;
            }
            break;

        case Kind::slider:
            switch (index)
            {
                case 1: set ("cap", 1); set ("colour", "#FF18181B"); set ("lineColour", "#FFFF7A1A"); break;
                case 2: set ("cap", 2); set ("material", 6); set ("colour", "#FFC8CBD0"); set ("capWidth", 60); set ("lineColour", "#FF1A1A1E"); break;
                case 0:
                default: break;
            }
            break;

        case Kind::toggle:
            switch (index)
            {
                case 1: set ("tip", 1); set ("leverMaterial", 0); set ("leverColour", "#FF1C1C1F"); break;
                case 2: set ("tip", 2); set ("base", 2); set ("baseMaterial", 13); set ("baseColour", "#FF2A2A2E"); break;
                case 0:
                default: break;
            }
            break;
    }

    return m;
}

//==============================================================================
LayerDoc Model3D::newDocument (const Model3D& model, bool across)
{
    auto doc = LayerDoc::emptyDocument();

    switch (model.kind)
    {
        case Kind::button: doc.canvasWidth = 64;  doc.canvasHeight = 64;  doc.frames = 2;  break;
        case Kind::toggle: doc.canvasWidth = 64;  doc.canvasHeight = 96;  doc.frames = 2;  break;
        case Kind::slider:
            doc.canvasWidth = across ? 256 : 64;
            doc.canvasHeight = across ? 64 : 256;
            doc.frames = 65;
            break;
        case Kind::knob:
        default:           doc.canvasWidth = 128; doc.canvasHeight = 128; doc.frames = 65; break;
    }

    doc.model3d = std::make_shared<const Model3D> (model);
    return doc;
}

juce::ValueTree Model3D::toValueTree() const
{
    juce::ValueTree tree ("Model3D");
    tree.setProperty ("kind", kindKey (kind), nullptr);

    for (const auto& v : values)
        tree.setProperty (v.name, v.value, nullptr);

    return tree;
}

std::shared_ptr<const Model3D> Model3D::fromValueTree (const juce::ValueTree& tree)
{
    if (! tree.isValid() || ! tree.hasType ("Model3D"))
        return nullptr;

    auto model = std::make_shared<Model3D>();
    model->kind = kindFromKey (tree.getProperty ("kind", "knob").toString());

    for (int i = 0; i < tree.getNumProperties(); ++i)
        if (const auto name = tree.getPropertyName (i); name.toString() != "kind")
            model->values.set (name, tree.getProperty (name));

    return model;
}
