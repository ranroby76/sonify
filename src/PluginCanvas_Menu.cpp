// D:\workspace\Fanan_web_hub\src\PluginCanvas_Menu.cpp
// The canvas's edit menu (an element's, several elements', or the window's: the one menu a
// right-click opens) with its actions and its Sonify AI instruction, the Sonify AI banner,
// and the selection.

#include "PluginCanvas.h"
#include "PluginCanvasMenu.h"
#include "StellaLookAndFeel.h"
#include "stella_keys.h"
#include "FontLibrary.h"
#include "MeterPaint.h"
#include "PanelParts.h"        // the lamps', selectors', keyboards' and labels' displays
#include "MaterialChoices.h"   // the painted backgrounds' and shapes' materials, finishes and patterns (as PanelPainter has them)

namespace
{
    const juce::StringArray meterModes { "peak", "rms" };
    const juce::StringArray meterModeNames { "Peak", "RMS" };
    const juce::StringArray filterModes { "lowpass", "lowpass24", "highpass", "highpass24", "bandpass" };
    const juce::StringArray filterModeNames { "Low-pass", "Low-pass 24 dB", "High-pass", "High-pass 24 dB", "Band-pass" };

    /** A filter curve's "Live cutoff" choice for following its cutoff knob alone. */
    juce::String knobOnly()    { return "Its cutoff knob only"; }

    const GuiWidget::Type typeOrder[] { GuiWidget::Type::knob, GuiWidget::Type::slider, GuiWidget::Type::toggle,
                                        GuiWidget::Type::selector, GuiWidget::Type::xy, GuiWidget::Type::keyboard, GuiWidget::Type::preset,
                                        GuiWidget::Type::tabs,
                                        GuiWidget::Type::meter, GuiWidget::Type::scope, GuiWidget::Type::lamp,
                                        GuiWidget::Type::envelope, GuiWidget::Type::filter,
                                        GuiWidget::Type::image, GuiWidget::Type::label, GuiWidget::Type::group, GuiWidget::Type::shape };
    constexpr int numTypes = (int) (sizeof (typeOrder) / sizeof (typeOrder[0]));

    constexpr int headerHeight = 46, actionsHeight = 40, pad = 12;
    const juce::Colour askYellow { 0xffffcc00 };   // as in the chat: what the user should answer or do

    /** Whether its colour is the menu's to set (knobs, sliders and switches wear looks). */
    bool takesColour (const GuiWidget& w)
    {
        return ! Looks::takesLook (w) && w.type != GuiWidget::Type::image && w.type != GuiWidget::Type::meter;
    }

    /** Whether it has text the menu's font is for: captions, labels, titles, buttons, the
        preset manager, a programmed element's own text. */
    bool takesFont (const GuiWidget& w)
    {
        return w.type != GuiWidget::Type::image && w.type != GuiWidget::Type::shape && w.type != GuiWidget::Type::keyboard
            && w.type != GuiWidget::Type::registration;
    }

    juce::String pictureChoice (const juce::String& name)
    {
        return name.isNotEmpty() ? name : juce::String::fromUTF8 ("Choose picture\xe2\x80\xa6");
    }

    using namespace MaterialChoices;   // the materials, finishes, patterns and blends the menu offers

    juce::String hexOf (juce::Colour c)    { return "#" + c.toDisplayString (true); }

    /** A slider on a line of its own: its value in a box at its right, typed in or dragged. */
    void setUpSlider (juce::Slider& s, double low, double high, double step, double middle, const juce::String& tip,
                      const juce::String& suffix = {})
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
        s.setRange (low, high, step);
        s.setTextValueSuffix (suffix);

        if (middle > low && middle < high)
            s.setSkewFactorFromMidPoint (middle);

        s.setTooltip (tip);

        // Its whole track shows, the part up to its value in green.
        s.setColour (juce::Slider::backgroundColourId, Theme::outline.brighter (0.45f));
        s.setColour (juce::Slider::trackColourId, Theme::accent);
        s.setColour (juce::Slider::thumbColourId, Theme::text);
    }

    constexpr int tabsHeight = 34;

    // The Type list's shapes, after the other types: their ids, kinds and names.
    constexpr int shapeIds = 100;
    const juce::StringArray shapeKindIds { "rectangle", "circle", "triangle", "star", "line", "drawn" };
    const juce::StringArray shapeKindNames { "Rectangle", "Circle", "Triangle", "Star", "Line", "Drawn shape" };

    /** A new shape's box: a star about as tall as it's wide, the rest as they look best. */
    juce::Rectangle<int> shapeBox (const juce::String& kind, int x, int y)
    {
        if (kind == "rectangle") return { x, y, 120, 72 };
        if (kind == "triangle")  return { x, y, 80, 70 };
        if (kind == "star")      return { x, y, 64, 61 };
        if (kind == "line")      return { x, y, 160, 32 };
        return { x, y, 80, 80 };
    }

    // A label's arrowheads, and a line's.
    const juce::StringArray arrowIds { "none", "left", "right", "both", "up", "down" };
    const juce::StringArray arrowNames { "None", "On the left", "On the right", "Both sides", "Pointing up", "Pointing down" };
    const juce::StringArray lineArrowIds { "none", "start", "end", "both" };
    const juce::StringArray lineArrowNames { "None", "At its start", "At its end", "At both ends" };
    const juce::StringArray lineCapIds { "round", "flat", "square" };
    const juce::StringArray lineCapNames { "Round", "Flat", "Square" };

    /** A number for a box: no trailing zeros. */
    juce::String shortNumber (double v, int decimals)
    {
        auto text = juce::String (v, decimals);
        return text.containsChar ('.') ? text.trimCharactersAtEnd ("0").trimCharactersAtEnd (".") : text;
    }

    // A keyboard's lowest and highest key: any white key a layout may have (stella::keys::normalise's
    // range: MIDI's 0 to 127, the lowest at most an octave below the top).
    constexpr int lowestKey = 0, highestLowKey = 115, highestKey = 127;

    void fillKeys (juce::ComboBox& box, int selected, int highest)
    {
        box.clear (juce::dontSendNotification);

        for (int note = lowestKey; note <= highest; ++note)
            if (! stella::keys::isBlack (note))
                box.addItem (GuiLayout::noteName (note) + (note == 60 ? "  (middle C)" : ""), note + 1);

        box.setSelectedId (selected + 1, juce::dontSendNotification);
    }
}

//==============================================================================
bool PluginCanvas::ElementMenu::Rows::isShowing (size_t index, const juce::String& inTab) const
{
    const auto& row = rows[index];

    if (row.tab != inTab)
        return false;

    if (! row.header)
        return row.shown;

    // A group's title: while a row of its group shows.
    for (auto i = index + 1; i < rows.size() && ! rows[i].header; ++i)
        if (rows[i].tab == inTab && rows[i].shown)
            return true;

    return false;
}

namespace
{
    constexpr int rowCaption = 18, rowGap = 8, headerRow = 30;
}

int PluginCanvas::ElementMenu::Rows::measure (const juce::String& inTab) const
{
    int y = 2;

    for (size_t i = 0; i < rows.size(); ++i)
    {
        if (! isShowing (i, inTab))
            continue;

        const auto& row = rows[i];
        y += row.header ? headerRow : (row.caption.isNotEmpty() ? rowCaption : 0) + row.height + rowGap;
    }

    return y + 4;
}

int PluginCanvas::ElementMenu::Rows::layOut (int width)
{
    int y = 2;

    for (size_t i = 0; i < rows.size(); ++i)
    {
        auto& row = rows[i];
        const bool showing = isShowing (i, tab);
        row.laidOut = showing;

        if (! row.header)
        {
            row.first->setVisible (showing);

            if (row.second != nullptr)
                row.second->setVisible (showing);
        }

        if (! showing)
            continue;

        if (row.header)
        {
            row.top = y;
            y += headerRow;
            continue;
        }

        if (row.caption.isNotEmpty())
            y += rowCaption;

        row.top = y;

        // A colour swatch sits inside its frame (painted around it).
        auto place = [] (juce::Component& c, juce::Rectangle<int> box)
        {
            c.setBounds (dynamic_cast<ColourSwatchButton*> (&c) != nullptr ? box.reduced (2, 0) : box);
        };

        if (row.second != nullptr)
        {
            const auto firstWidth = juce::roundToInt ((float) (width - 8) * row.firstShare);
            place (*row.first, { 0, y, firstWidth, row.height });
            place (*row.second, { firstWidth + 8, y, width - firstWidth - 8, row.height });
        }
        else
        {
            place (*row.first, { 0, y, juce::roundToInt ((float) width * row.firstShare), row.height });
        }

        y += row.height + rowGap;
    }

    y += 4;
    setSize (width, y);
    return y;
}

void PluginCanvas::ElementMenu::Rows::paint (juce::Graphics& g)
{
    for (const auto& row : rows)
    {
        if (! row.laidOut)
            continue;

        if (row.header)
        {
            // A group: its name, and a line across under it.
            const auto lineY = row.top + headerRow - 7;
            g.setColour (Theme::accent.withAlpha (0.9f));
            g.setFont (Theme::font (11.5f, true));
            g.drawText (row.caption.toUpperCase(), 0, row.top + 4, getWidth(), 16, juce::Justification::centredLeft, true);
            g.setColour (Theme::outline.brighter (0.25f));
            g.fillRect (0, lineY, getWidth(), 1);
            continue;
        }

        if (row.caption.isNotEmpty())
        {
            g.setColour (Theme::muted);
            g.setFont (Theme::font (12.5f));
            g.drawText (row.caption, 0, row.top - rowCaption, getWidth(), 16, juce::Justification::centredLeft, true);
        }

        // Colour swatches get a frame, so dark colours still show where they are.
        for (auto* c : { row.first, row.second })
        {
            if (dynamic_cast<ColourSwatchButton*> (c) != nullptr)
            {
                g.setColour (Theme::outline.brighter (0.35f));
                g.drawRoundedRectangle (c->getBounds().toFloat().expanded (1.5f), 4.0f, 1.0f);
            }
        }
    }
}

//==============================================================================
std::vector<juce::Rectangle<float>> PluginCanvas::ElementMenu::TabStrip::cells() const
{
    // Each as wide as its name needs, spread over the strip.
    std::vector<juce::Rectangle<float>> result;
    const auto font = Theme::font (12.5f, true);
    std::vector<float> widths;
    float total = 0.0f;

    for (const auto& name : names)
    {
        widths.push_back ((float) juce::GlyphArrangement::getStringWidthInt (font, name) + 16.0f);
        total += widths.back();
    }

    const auto gap = 3.0f;
    const auto room = (float) getWidth() - gap * (float) juce::jmax (0, names.size() - 1);
    const auto scale = total > 0.0f ? room / total : 1.0f;
    float x = 0.0f;

    for (const auto w : widths)
    {
        result.push_back ({ x, 4.0f, w * scale, (float) getHeight() - 8.0f });
        x += w * scale + gap;
    }

    return result;
}

void PluginCanvas::ElementMenu::TabStrip::paint (juce::Graphics& g)
{
    const auto boxes = cells();

    for (int i = 0; i < names.size(); ++i)
    {
        const auto box = boxes[(size_t) i];
        const bool on = names[i] == current;

        g.setColour (on ? Theme::accentDeep : Theme::raised);
        g.fillRoundedRectangle (box, 5.0f);
        g.setColour (on ? Theme::accent : Theme::outline.brighter (0.2f));
        g.drawRoundedRectangle (box.reduced (0.5f), 5.0f, 1.0f);
        g.setColour (on ? Theme::text : Theme::muted.brighter (0.2f));
        g.setFont (Theme::font (12.5f, true));
        g.drawFittedText (names[i], box.toNearestInt(), juce::Justification::centred, 1, 0.8f);
    }
}

void PluginCanvas::ElementMenu::TabStrip::mouseDown (const juce::MouseEvent& e)
{
    const auto boxes = cells();

    for (int i = 0; i < names.size(); ++i)
        if (boxes[(size_t) i].contains (e.position) && onPick != nullptr)
            onPick (names[i]);
}

void PluginCanvas::ElementMenu::FontSample::paint (juce::Graphics& g)
{
    const auto box = getLocalBounds().toFloat();
    g.setColour (Theme::inset);
    g.fillRoundedRectangle (box, 5.0f);
    g.setColour (Theme::text);
    g.setFont (FontLibrary::font (family, 22.0f));
    g.drawFittedText (text, getLocalBounds().reduced (10, 2), juce::Justification::centredLeft, 1, 0.7f);
}

void PluginCanvas::ElementMenu::SendButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto circle = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre()).reduced (1.0f);
    const auto centre = circle.getCentre();

    g.setColour (down ? Theme::accent.darker (0.2f) : (highlighted ? Theme::accent.brighter (0.15f) : Theme::accent));
    g.fillEllipse (circle);

    // An arrow pointing up, as in the chat.
    juce::Path arrow;
    const auto a = size * 0.22f;
    arrow.startNewSubPath (centre.x, centre.y + a);
    arrow.lineTo (centre.x, centre.y - a);
    arrow.startNewSubPath (centre.x - a * 0.8f, centre.y - a * 0.2f);
    arrow.lineTo (centre.x, centre.y - a);
    arrow.lineTo (centre.x + a * 0.8f, centre.y - a * 0.2f);
    g.setColour (juce::Colours::white);
    g.strokePath (arrow, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
PluginCanvas::ElementMenu::ElementMenu (PluginCanvas& owner)
    : canvas (owner)
{
    closeButton.setButtonText (juce::String::fromUTF8 ("\xc3\x97"));
    closeButton.setTooltip ("Close (Esc)");
    closeButton.onClick = [this] { canvas.select (-1); };
    addAndMakeVisible (closeButton);

    pictureMode.addItemList (GuiLayout::pictureModeNames(), 1);

    for (auto* box : { &type, &source, &mode, &style, &pictureMode, &lowKey, &highKey })
        box->onChange = [this] { apply(); };

    for (auto* editor : { &label, &options, &textSize, &widthBox, &heightBox, &param, &roles[0], &roles[1], &roles[2], &roles[3] })
    {
        editor->setFont (Theme::font (14.0f));
        editor->setIndents (6, 4);
    }

    // Parameters are typed in, an id (or a name): they're used on Enter, or on leaving the box
    // (for what they were typed for: see commitTyped).
    for (auto* editor : { &param, &roles[0], &roles[1], &roles[2], &roles[3] })
    {
        editor->setTextToShowWhenEmpty ("none: type a parameter id", Theme::muted);
        editor->setTooltip ("The parameter it works: its id (or name). A new id is a parameter for Fanan AI to add to the plugin");
        editor->onTextChange = [this, editor] { typedBox = editor; };
        editor->onReturnKey = [this] { typedBox = nullptr; apply(); };
        editor->onFocusLost = [this, editor] { if (typedBox == editor) commitTyped(); };
    }

    // What it does, under the title.
    copyButton.setTooltip ("Copy (Ctrl+C): a frame takes what's inside it along");
    pasteButton.setTooltip ("Paste (Ctrl+V)");
    duplicateButton.setTooltip ("Duplicate (Ctrl+D)");
    layerBox.setTooltip ("Bring to front or forward, send backward or to the back (Ctrl+Shift+], Ctrl+], Ctrl+[, Ctrl+Shift+[)");

    copyButton.onClick = [this]
    {
        canvas.copyElements (targets());
        pasteButton.setEnabled (true);
    };

    pasteButton.onClick = [this]
    {
        // The first paste lands where the menu was opened, the next ones a step further each.
        const auto at = canvas.pastePoint;
        canvas.pastePoint.reset();
        canvas.pasteElements (at);
    };

    duplicateButton.onClick = [this]
    {
        canvas.copyElements (targets());
        canvas.pasteElements ({});
    };

    layerBox.onChange = [this]
    {
        const auto choice = layerBox.getSelectedId();
        layerBox.setText ("Layer", juce::dontSendNotification);   // it stays "Layer"
        layer (choice);
    };

    addChildComponent (copyButton);
    addChildComponent (pasteButton);
    addChildComponent (duplicateButton);
    addChildComponent (layerBox);

    label.onTextChange = [this] { apply(); };
    options.onTextChange = [this] { apply(); };
    options.setTextToShowWhenEmpty ("Saw, Square, Triangle", Theme::muted);

    textSize.setInputRestrictions (4, "0123456789.");
    textSize.setTextToShowWhenEmpty ("auto", Theme::muted);
    textSize.onTextChange = [this] { typedBox = &textSize; };
    textSize.onReturnKey = [this] { typedBox = nullptr; apply(); };
    textSize.onFocusLost = [this] { if (typedBox == &textSize) commitTyped(); };
    boldToggle.onClick = [this] { apply(); };

    colour.setTooltip ("Its colour (see-through: the standard one)");
    colour.onColourChange = [this] { apply(); };

    for (auto* box : { &widthBox, &heightBox })
    {
        box->setInputRestrictions (4, "0123456789");
        box->onTextChange = [this, box] { typedBox = box; };
        box->onReturnKey = [this] { typedBox = nullptr; applyWindow(); showFor (-1); };
        box->onFocusLost = [this, box] { if (typedBox == box) commitTyped(); };
    }

    topColour.onColourChange = [this] { applyWindow(); };
    bottomColour.onColourChange = [this] { applyWindow(); };

    pictureButton.setTooltip ("Choose the picture it shows (PNG, JPEG or GIF): it's copied into the project");
    pictureButton.onClick = [this] { canvas.choosePicture (widget); };

    paintBackgroundButton.setTooltip ("A background painted in code and lit like the knobs: then choose its material (metals, stone, "
                                      "wood, plastics...), colours, finish and a pattern in its menu");
    paintBackgroundButton.setButtonText (juce::String::fromUTF8 ("Paint a lit background (materials, patterns)\xe2\x80\xa6"));
    paintBackgroundButton.onClick = [this] { canvas.paintNewBackground(); };

    // A painted background's surface (and a shape's).
    {
        fillMaterials (false);

        for (size_t i = 0; i < finishChoices().size(); ++i)   surfaceFinish.addItem (finishChoices()[i].label, (int) i + 1);
        for (size_t i = 0; i < patternChoices().size(); ++i)  patternBox.addItem (patternChoices()[i].label, (int) i + 1);
        for (size_t i = 0; i < blendChoices().size(); ++i)    blendBox.addItem (blendChoices()[i].label, (int) i + 1);

        surfaceMaterial.onChange = [this] { applySurface (true); };

        for (auto* box : { &surfaceFinish, &patternBox, &blendBox })
            box->onChange = [this] { applySurface (false); };

        for (auto* swatch : { &surfaceColour, &surfaceColour2, &patternColour })
            swatch->onColourChange = [this] { applySurface (false); };

        surfaceColour.setTooltip ("Its main colour");
        surfaceColour2.setTooltip ("Its second colour: marble's veins, granite's flecks, wood's dark grain, a two-tone shade away from the light");
        patternColour.setTooltip ("The pattern's colour (see-through: black or white, as its blend suits)");

        setUpSlider (textureSize, 0.25, 4.0, 0.01, 1.0, "How big the material's texture is");
        setUpSlider (textureStrength, 0.0, 2.0, 0.01, 1.0, "How strong the material's texture is");
        setUpSlider (patternOpacity, 0.0, 1.0, 0.01, 0.0, "How strong the pattern is");
        setUpSlider (patternSize, 4.0, 200.0, 1.0, 24.0, "The pattern's size, in pixels", " px");
        setUpSlider (patternAngle, -180.0, 180.0, 1.0, 0.0, "The pattern's angle, in degrees", juce::String::fromUTF8 ("\xc2\xb0"));
        setUpSlider (keyLight, 0.0, 1.0, 0.01, 0.0, "How strong the key light's pool is (the light itself is the panel's global light)");

        for (auto* slider : { &textureSize, &textureStrength, &patternOpacity, &patternSize, &patternAngle, &keyLight })
            slider->onValueChange = [this] { applySurface (false); };
    }

    // Pages: the element's page, and the window's list of them.
    pageBox.setTooltip ("The page it lies on (Every page: it shows whichever page is shown)");
    pageBox.onChange = [this] { apply(); };

    pagesBox.setFont (Theme::font (14.0f));
    pagesBox.setIndents (6, 4);
    pagesBox.setTextToShowWhenEmpty ("One page: type names (Main, Effects...) to make pages", Theme::muted);
    pagesBox.setTooltip ("The window's pages in order, comma separated: rename one by changing its name, take it out to delete it "
                         "(its elements go to the page before). The page tabs show them; above the window, a click on a page's name shows it");
    pagesBox.onTextChange = [this] { typedBox = &pagesBox; };
    pagesBox.onReturnKey = [this] { typedBox = nullptr; canvas.setPages (juce::StringArray::fromTokens (pagesBox.getText(), ",", "")); showFor (-1); };
    pagesBox.onFocusLost = [this] { if (typedBox == &pagesBox) commitTyped(); };

    // Every element: where it lies, how big, turned and see-through.
    for (auto* box : { &xBox, &yBox, &wBox, &hBox, &angleBox })
    {
        box->setFont (Theme::font (14.0f));
        box->setIndents (6, 4);
        box->setInputRestrictions (7, "-0123456789.");
        box->onTextChange = [this, box] { typedBox = box; };
        box->onReturnKey = [this] { typedBox = nullptr; applyObject(); };
        box->onFocusLost = [this, box] { if (typedBox == box) commitTyped(); };
    }

    xBox.setTooltip ("Its left edge, in the window's pixels");
    yBox.setTooltip ("Its top edge, in the window's pixels");
    wBox.setTooltip ("Its width, in pixels");
    hBox.setTooltip ("Its height, in pixels");
    angleBox.setTooltip ("Its angle in degrees, clockwise (or drag the round handle above it; Shift: steps of 15)");

    setUpSlider (opacitySlider, 0.0, 100.0, 1.0, 0.0, "How solid it is: 100 solid, 0 invisible (in Edit a dashed frame still shows where it is)", " %");
    opacitySlider.onValueChange = [this] { applyObject(); };

    // A shape's geometry; its layer style in a window of its own.
    setUpSlider (cornerRadius, 0.0, 100.0, 1.0, 0.0, "How round its corners are: 0 sharp, 100 as round as they go (a rectangle "
                                                    "becomes a pill; a rectangle's yellow handle drags it too)", " %");
    setUpSlider (starPoints, 3.0, 24.0, 1.0, 0.0, "How many points the star has");
    setUpSlider (starInner, 0.05, 1.0, 0.01, 0.0, "How far in its points' roots lie: small for sharp points, 1 for a polygon");
    setUpSlider (topPoint, -0.95, 0.95, 0.01, 0.0, "Where its top point lies, left to right (or drag its yellow handle)");
    setUpSlider (middleX, -0.95, 0.95, 0.01, 0.0, "Its middle moved sideways: lopsided, longer on one side (or drag its yellow handle)");
    setUpSlider (middleY, -0.95, 0.95, 0.01, 0.0, "Its middle moved up or down: lopsided, longer on one side (or drag its yellow handle)");
    setUpSlider (leanX, -1.0, 1.0, 0.01, 0.0, "Leant sideways: its top one way, its bottom the other");
    setUpSlider (leanY, -1.0, 1.0, 0.01, 0.0, "Leant up or down: its right side one way, its left the other");

    for (auto* slider : { &cornerRadius, &starPoints, &starInner, &topPoint, &middleX, &middleY, &leanX, &leanY })
        slider->onValueChange = [this] { applyShape(); };

    for (auto* slider : { &topPoint, &middleX, &middleY, &leanX, &leanY })
        slider->setDoubleClickReturnValue (true, 0.0);

    layerStyleButton.setButtonText (juce::String::fromUTF8 ("Layer style (fx)\xe2\x80\xa6"));
    layerStyleButton.setTooltip ("Photoshop's Layer Style: Bevel & Emboss with its Contour, Stroke, Inner Shadow, Color Overlay "
                                 "and Drop Shadow, with their blend modes (or double-click the shape)");
    layerStyleButton.onClick = [this]
    {
        if (widget >= 0)
            canvas.editLayerStyle (widget);
    };

    globalLightButton.setButtonText (juce::String::fromUTF8 ("Global light\xe2\x80\xa6"));
    globalLightButton.setTooltip ("Where the panel's one light comes from: the knobs' looks, the shapes and painted backgrounds "
                                  "are all lit from it (Photoshop's Global Light)");
    globalLightButton.onClick = [this] { canvas.editGlobalLight(); };

    backgroundButton.setTooltip ("A picture behind everything, filling the window (it's copied into the project). "
                                 "Drag its corners and sides to stretch it; right-click it to change its layer");
    backgroundButton.setButtonText (juce::String::fromUTF8 ("Add a background picture\xe2\x80\xa6"));
    backgroundButton.onClick = [this] { canvas.choosePicture (-1); };

    knobStudioButton.setTooltip ("Open its look in the Knob Studio: layers, bevels, textures, the KnobMan gallery");
    knobStudioButton.onClick = [this]
    {
        if (widget >= 0 && canvas.onEditLook != nullptr)
            canvas.onEditLook (widget, similarToggle.getToggleState());
    };

    similarToggle.setTooltip ("Design every control like this one at once (the same kind, wearing the same look). "
                              "Unticked, only this one changes");

    // Delete, in the header beside the close button: on every tab.
    removeButton.setTooltip ("Delete it from the window (Delete key)");
    removeButton.onClick = [this] { canvas.removeSelected(); };
    removeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff5c1f24));
    removeButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    addChildComponent (removeButton);

    // The look it had before others were tried, and a built-in look as it came.
    originalLookButton.onClick = [this]
    {
        if (! juce::isPositiveAndBelow (widget, (int) canvas.layout.widgets.size()))
            return;

        auto& w = canvas.layout.widgets[(size_t) widget];
        w.style = w.originalStyle;
        w.originalKnown = false;
        w.originalStyle.clear();
        canvas.edited();
        showFor (widget);
    };

    restoreLookButton.onClick = [this]
    {
        if (! juce::isPositiveAndBelow (widget, (int) canvas.layout.widgets.size()) || canvas.looks == nullptr)
            return;

        const auto name = canvas.looks->lookFor (canvas.layout.widgets[(size_t) widget]).name;
        canvas.looks->removeProjectLook (name);
        canvas.repaint();
        showFor (widget);
    };

    // A level meter's display, colours and details.
    for (int i = 0; i < MeterPaint::displays().size(); ++i)
        meterDisplay.addItem (MeterPaint::displayNames()[i], i + 1);

    meterDisplay.setTooltip ("How it shows the level: a bar, LEDs, a neon tube, a glass tube, an analog VU needle or a ring");
    meterDisplay.onChange = [this] { applyMeter(); };

    for (auto* swatch : { &meterLow, &meterMid, &meterHigh, &meterBack })
        swatch->onColourChange = [this] { applyMeter(); };

    meterLow.setTooltip ("Its colour at low levels (see-through: the standard one)");
    meterMid.setTooltip ("Its colour from -12 dB (see-through: the standard one)");
    meterHigh.setTooltip ("Its colour from -3 dB, near clipping: a VU's red zone and peak lamp too (see-through: the standard one)");
    meterBack.setTooltip ("Its background: a VU's face, a ring's disc (see-through: the standard one)");

    setUpSlider (meterSegments, 0.0, 48.0, 1.0, 0.0, "How many LEDs (0: as many as fit)");
    setUpSlider (meterGlow, 0.0, 100.0, 1.0, 0.0, "How much its light blooms", " %");

    for (auto* slider : { &meterSegments, &meterGlow })
        slider->onValueChange = [this] { applyMeter(); };

    meterPeakToggle.setTooltip ("A mark stays at the highest level for a moment, then falls");
    meterScaleToggle.setTooltip ("Its dB marks (a display's: quarters)");

    for (auto* toggle : { &meterPeakToggle, &meterScaleToggle })
        toggle->onClick = [this] { applyMeter(); };

    // A lamp's, selector's, keyboard's or label's display (its list is filled for its kind).
    partDisplay.setTooltip ("How it looks: a lamp as a domed LED, a jewel, a glowing tube...; a selector as a sliding pill, console keys, "
                            "lamps or a glowing display; a keyboard in reverse colours or modern slate; a label as a value readout, "
                            "a glowing display or a backlit LCD");
    partDisplay.onChange = [this] { applyPart(); };
    followToggle.setTooltip ("Its light follows the level, brighter as it rises (otherwise it's on from its threshold)");
    followToggle.onClick = [this] { applyPart(); };
    arrowsBox.addItemList (arrowNames, 1);
    arrowsBox.setTooltip ("Small arrowheads beside its words");
    arrowsBox.onChange = [this] { applyPart(); };

    // A line's stroke.
    setUpSlider (lineWidthSlider, 0.5, 40.0, 0.5, 6.0, "How wide the line is, in pixels", " px");
    lineWidthSlider.onValueChange = [this] { applyShape(); };
    lineArrowBox.addItemList (lineArrowNames, 1);
    lineArrowBox.setTooltip ("Arrowheads at its ends");
    lineArrowBox.onChange = [this] { applyShape(); };
    lineCapBox.addItemList (lineCapNames, 1);
    lineCapBox.setTooltip ("How its ends finish where there's no arrowhead");
    lineCapBox.onChange = [this] { applyShape(); };
    straightenButton.setTooltip ("Its bend point back on the straight way between its ends (or double-click the bend point). "
                                 "Drag the line to bow it, drag its ends to move them, right-drag it to move it all");
    straightenButton.onClick = [this]
    {
        if (! canvas.isLine (widget))
            return;

        ObjectPaint::straighten (canvas.layout.widgets[(size_t) widget]);
        canvas.edited();
        canvas.repaint();
        showFor (widget);
    };

    windowButton.setTooltip ("The window's own menu: its size, colours and background picture");
    windowButton.onClick = [this]
    {
        canvas.selectPanel();
        canvas.openMenu();
    };

    // Its text's font: the standard one, or one of the fonts built in, by group.
    fontBox.addItem ("Standard", 1);

    for (const auto& group : FontLibrary::groups())
    {
        fontBox.addSectionHeading (group);

        for (int i = 0; i < (int) FontLibrary::families().size(); ++i)
            if (FontLibrary::families()[(size_t) i].group == group)
                fontBox.addItem (FontLibrary::families()[(size_t) i].name, i + 2);
    }

    fontBox.setTooltip ("Its text's font: built into Sonify, so the panel looks the same on every computer");
    fontBox.onChange = [this] { apply(); };

    setUpSlider (presetRounding, 0.0, 100.0, 1.0, 0.0, "How round its corners are: 0 square, 100 round ends", " %");
    presetRounding.onValueChange = [this] { apply(); };

    for (auto* button : { &windowLightButton, &shapeLightButton })
    {
        button->setButtonText (juce::String::fromUTF8 ("Global light\xe2\x80\xa6"));
        button->setTooltip (globalLightButton.getTooltip());
        button->onClick = [this] { canvas.editGlobalLight(); };
    }

    fontSample.setInterceptsMouseClicks (false, false);

    cropToolButton.setButtonText (juce::String::fromUTF8 ("Crop the window (C)\xe2\x80\xa6"));
    cropToolButton.setTooltip ("Photoshop's Crop tool: a box over the window, its sides and corners dragged in or out, then Enter");
    cropToolButton.onClick = [this]
    {
        auto& c = canvas;   // the menu closes as the tool starts
        c.select (-1);
        c.startCrop();
    };

    tabStrip.onPick = [this] (const juce::String& name) { pickTab (name); };
    addChildComponent (tabStrip);

    // An element's rows, tab by tab, in the order they show...
    const juce::String object ("Object"), control ("Control"), text ("Text"), look ("Look"), shape ("Shape"), surface ("Surface"),
                       window ("Window"), background ("Background");

    addHeader (object, "Kind");
    addRow (object, "Type", type);
    addHeader (object, "Place");
    addRow (object, "Position (left, top)", xBox, &yBox, 0.5f);
    addRow (object, "Size (width, height)", wBox, &hBox, 0.5f);
    addHeader (object, "Turn and see-through");
    addRow (object, "Angle (degrees)", angleBox, nullptr, 0.4f);
    addRow (object, "Opacity", opacitySlider, nullptr, 1.0f, 24);
    addHeader (object, "Page");
    addRow (object, "Page", pageBox);
    addHeader (object, "Window");
    addRow (object, {}, windowButton, nullptr, 1.0f, 28);

    addHeader (control, "Parameters");
    addRow (control, "Parameter", param);
    addRow (control, "Watches", source);

    for (auto& box : roles)
        addRow (control, "Role", box);

    addRow (control, "Mode", mode);
    addHeader (control, "Choices");
    addRow (control, "Positions (comma separated)", options);
    addRow (control, "Keys (lowest, highest)", lowKey, &highKey, 0.5f);

    const juce::String meter ("Meter");
    addHeader (meter, "Display");
    addRow (meter, "Display", meterDisplay);
    addHeader (meter, "Colours");
    addRow (meter, "Low", meterLow, nullptr, 1.0f, 22);
    addRow (meter, "Middle (from -12 dB)", meterMid, nullptr, 1.0f, 22);
    addRow (meter, "Top (from -3 dB)", meterHigh, nullptr, 1.0f, 22);
    addRow (meter, "Background", meterBack, nullptr, 1.0f, 22);
    addHeader (meter, "Details");
    addRow (meter, "LEDs", meterSegments, nullptr, 1.0f, 24);
    addRow (meter, "Glow", meterGlow, nullptr, 1.0f, 24);
    addRow (meter, {}, meterPeakToggle, &meterScaleToggle, 0.5f, 24);

    addHeader (text, "Words");
    addRow (text, "Text", label);
    addRow (text, "Text size, bold", textSize, &boldToggle, 0.4f);
    addHeader (text, "Font");
    addRow (text, "Font", fontBox);
    addRow (text, {}, fontSample, nullptr, 1.0f, 40);

    addHeader (look, "Display");
    addRow (look, "Display", partDisplay);
    addRow (look, {}, followToggle, nullptr, 1.0f, 24);
    addRow (look, "Arrowheads", arrowsBox);
    addHeader (look, "Look");
    addRow (look, "Look", style);
    addRow (look, {}, knobStudioButton, &similarToggle, 0.64f, 28);
    addRow (look, {}, originalLookButton, nullptr, 1.0f, 28);
    addRow (look, {}, restoreLookButton, nullptr, 1.0f, 28);
    addHeader (look, "Picture");
    addRow (look, "Picture", pictureButton);
    addRow (look, "Picture fit", pictureMode);
    addHeader (look, "Colour");
    addRow (look, "Colour", colour, nullptr, 1.0f, 22);
    addHeader (look, "Corners");
    addRow (look, "Rounding", presetRounding, nullptr, 1.0f, 24);
    addHeader (look, "Light");
    addRow (look, {}, globalLightButton, nullptr, 1.0f, 28);   // what's lit: looks, painted pictures

    addHeader (shape, "Line");
    addRow (shape, "Width", lineWidthSlider, nullptr, 1.0f, 24);
    addRow (shape, "Arrowheads", lineArrowBox);
    addRow (shape, "Ends", lineCapBox);
    addRow (shape, {}, straightenButton, nullptr, 1.0f, 28);
    addHeader (shape, "Corners");
    addRow (shape, "Rounding", cornerRadius, nullptr, 1.0f, 24);
    addHeader (shape, "Star");
    addRow (shape, "Points", starPoints, nullptr, 1.0f, 24);
    addRow (shape, "Inner size", starInner, nullptr, 1.0f, 24);
    addHeader (shape, "Form");
    addRow (shape, "Top point (left to right)", topPoint, nullptr, 1.0f, 24);
    addRow (shape, "Middle (sideways)", middleX, nullptr, 1.0f, 24);
    addRow (shape, "Middle (up and down)", middleY, nullptr, 1.0f, 24);
    addRow (shape, "Lean (sideways)", leanX, nullptr, 1.0f, 24);
    addRow (shape, "Lean (up and down)", leanY, nullptr, 1.0f, 24);
    addHeader (shape, "Effects and light");
    addRow (shape, {}, layerStyleButton, &shapeLightButton, 0.5f, 28);

    // A shape's surface, a painted picture's (and the window's painted background's, in its
    // Background tab).
    addHeader (surface, "Material");
    addRow (surface, "Material", surfaceMaterial);
    addRow (surface, "Finish", surfaceFinish);
    addRow (surface, "Colours (main, second)", surfaceColour, &surfaceColour2, 0.5f, 22);
    addHeader (surface, "Texture");
    addRow (surface, "Texture size", textureSize, nullptr, 1.0f, 24);
    addRow (surface, "Texture strength", textureStrength, nullptr, 1.0f, 24);
    addHeader (surface, "Light");
    addRow (surface, "Key light", keyLight, nullptr, 1.0f, 24);

    // Its pattern layer, Photoshop-style (the window's too).
    const juce::String pattern ("Pattern");
    addHeader (pattern, "Pattern");
    addRow (pattern, "Pattern", patternBox);
    addRow (pattern, "Blend", blendBox);
    addRow (pattern, "Colour", patternColour, nullptr, 1.0f, 22);
    addHeader (pattern, "Strength and scale");
    addRow (pattern, "Opacity", patternOpacity, nullptr, 1.0f, 24);
    addRow (pattern, "Size", patternSize, nullptr, 1.0f, 24);
    addRow (pattern, "Angle", patternAngle, nullptr, 1.0f, 24);

    // ...and the window's.
    addHeader (window, "Size");
    addRow (window, "Window size (width, height)", widthBox, &heightBox, 0.5f);
    addRow (window, {}, cropToolButton, nullptr, 1.0f, 28);
    addHeader (window, "Pages");
    addRow (window, "Pages (in order, comma separated)", pagesBox);
    addHeader (window, "Light");
    addRow (window, {}, windowLightButton, nullptr, 1.0f, 28);

    addHeader (background, "Colours");
    addRow (background, "Background colours (top, bottom)", topColour, &bottomColour, 0.5f, 22);
    addHeader (background, "Picture");
    addRow (background, "Background picture", backgroundButton);
    addRow (background, {}, paintBackgroundButton, nullptr, 1.0f, 28);

    viewport.setViewedComponent (&rows, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);

    // Sonify AI, at the foot.
    instruction.setMultiLine (true, true);
    instruction.setReturnKeyStartsNewLine (false);
    instruction.setFont (Theme::font (14.0f));
    instruction.setIndents (8, 6);
    instruction.onReturnKey = [this] { send(); };
    addAndMakeVisible (instruction);

    sendButton.setTooltip ("Send it to Fanan AI (Enter)");
    sendButton.onClick = [this] { send(); };
    addAndMakeVisible (sendButton);
}

PluginCanvas::ElementMenu::~ElementMenu() = default;

void PluginCanvas::ElementMenu::addRow (const juce::String& tab, const juce::String& caption, juce::Component& first, juce::Component* second,
                                        float firstShare, int height)
{
    Rows::Row row;
    row.caption = caption;
    row.first = &first;
    row.second = second;
    row.firstShare = firstShare;
    row.height = height;
    row.shown = false;
    row.tab = tab;
    row.surface = tab == "Surface";
    rows.rows.push_back (row);
    rows.addChildComponent (first);

    if (second != nullptr)
        rows.addChildComponent (*second);
}

void PluginCanvas::ElementMenu::addHeader (const juce::String& tab, const juce::String& heading)
{
    Rows::Row row;
    row.caption = heading;
    row.tab = tab;
    row.surface = tab == "Surface";
    row.header = true;
    row.height = 0;
    rows.rows.push_back (row);
}

void PluginCanvas::ElementMenu::showTabs()
{
    // The surface rows: the shape's or painted picture's Surface tab, or the window's painted
    // background's, in its Background tab.
    const bool forWindow = widget < 0 && several.empty();

    for (auto& row : rows.rows)
        if (row.surface)
            row.tab = forWindow ? "Background" : "Surface";

    // The tabs with a row showing, in their order.
    juce::StringArray names;

    for (const auto* name : { "Object", "Control", "Meter", "Text", "Look", "Shape", "Surface", "Window", "Background", "Pattern" })
        for (const auto& row : rows.rows)
            if (! row.header && row.shown && row.tab == name)
            {
                names.add (name);
                break;
            }

    tabStrip.names = names;
    tabStrip.setVisible (names.size() > 1);
    tabStrip.current = names.contains (chosenTab) ? chosenTab : names[0];
    rows.tab = tabStrip.current;
    tabStrip.repaint();
}

void PluginCanvas::ElementMenu::pickTab (const juce::String& name)
{
    chosenTab = name;
    showTabs();
    viewport.setViewPosition (0, 0);
    resized();
    repaint();
}

void PluginCanvas::ElementMenu::setShown (juce::Component& first, bool shouldShow, const juce::String& caption)
{
    for (auto& row : rows.rows)
    {
        if (row.first != &first)
            continue;

        row.shown = shouldShow;

        if (caption.isNotEmpty())
            row.caption = caption;
    }
}

//==============================================================================
void PluginCanvas::ElementMenu::showFor (int widgetIndex)
{
    const juce::ScopedValueSetter<bool> quiet (updating, true);
    widget = juce::isPositiveAndBelow (widgetIndex, (int) canvas.layout.widgets.size()) ? widgetIndex : -1;
    several.clear();
    typedBox = nullptr;   // the boxes show what it is now

    for (auto& row : rows.rows)
        row.shown = false;

    showActions (widget >= 0);
    removeButton.setVisible (widget >= 0);
    instruction.setVisible (true);
    sendButton.setVisible (true);

    if (widget < 0)
    {
        // The window itself.
        const auto& l = canvas.layout;
        title = "Plugin window";
        subtitle = juce::String (l.width) + juce::String::fromUTF8 (" \xc3\x97 ") + juce::String (l.height);

        widthBox.setText (juce::String (l.width), juce::dontSendNotification);
        heightBox.setText (juce::String (l.height), juce::dontSendNotification);
        topColour.setColourValue (l.backgroundTop, juce::dontSendNotification);
        bottomColour.setColourValue (l.backgroundBottom, juce::dontSendNotification);

        setShown (widthBox, true);
        setShown (cropToolButton, true);
        setShown (topColour, true);
        setShown (backgroundButton, true);
        pagesBox.setText (l.pages.joinIntoString (", "), juce::dontSendNotification);
        setShown (pagesBox, true);

        // A painted background: its material, colours, finish, texture, pattern and light, in
        // the Background tab (the one filling the window, or else the first). None yet: a
        // button to paint one.
        juce::var painted;

        for (const auto& w : l.widgets)
            if (const auto recipe = canvas.paintedRecipeOf (w); recipe.isObject() && (! painted.isObject() || canvas.isBackdrop (w)))
                painted = recipe;

        if (painted.isObject())
        {
            shapeSurface = false;
            showSurface (painted);
        }

        setShown (paintBackgroundButton, ! painted.isObject() && canvas.onPaintPicture != nullptr);
        setShown (windowLightButton, canvas.hasPanel());

        instruction.setTextToShowWhenEmpty ("e.g. a dark wooden look with brass knobs", Theme::muted);
    }
    else
    {
        const auto& w = canvas.layout.widgets[(size_t) widget];
        const auto t = w.type;
        const bool custom = t == GuiWidget::Type::custom;
        const bool dropdown = custom && w.element == GuiLayout::builtInElementFor ("dropdown");
        title = dropdown ? juce::String ("Drop-down list")
              : custom ? w.element
              : t == GuiWidget::Type::shape ? shapeKindNames[juce::jmax (0, shapeKindIds.indexOf (w.shapeKind()))]
                                            : PluginCanvas::typeDisplayName (t);
        subtitle = custom && ! dropdown ? PluginCanvas::typeDisplayName (t) : (t == GuiWidget::Type::image ? w.image : w.label);

        // The types, then the shapes by kind (a drawn one only while it's one).
        type.clear (juce::dontSendNotification);

        for (int i = 0; i < numTypes; ++i)
            if (typeOrder[i] != GuiWidget::Type::shape)
                type.addItem (PluginCanvas::typeDisplayName (typeOrder[i]), i + 1);

        type.addSectionHeading ("Shapes");
        const auto kind = t == GuiWidget::Type::shape ? w.shapeKind() : juce::String();

        for (int k = 0; k < shapeKindIds.size(); ++k)
            if (shapeKindIds[k] != "drawn" || kind == "drawn")
                type.addItem (shapeKindNames[k], shapeIds + k);

        if (t == GuiWidget::Type::shape)
            type.setSelectedId (shapeIds + juce::jmax (0, shapeKindIds.indexOf (kind)), juce::dontSendNotification);
        else
            for (int i = 0; i < numTypes; ++i)
                if (typeOrder[i] == t)
                    type.setSelectedId (i + 1, juce::dontSendNotification);

        setShown (type, ! custom && t != GuiWidget::Type::registration);   // a programmed element (or the badge) stays what it is
        showObject (w);

        // Its page, when the window has pages.
        pageBox.clear (juce::dontSendNotification);
        pageBox.addItem ("Every page", 1);

        for (int i = 0; i < canvas.layout.pages.size(); ++i)
            pageBox.addItem (canvas.layout.pages[i], i + 2);

        if (const auto pageIndex = canvas.layout.pages.indexOf (w.page); w.page.isEmpty() || pageIndex >= 0)
            pageBox.setSelectedId (w.page.isEmpty() ? 1 : pageIndex + 2, juce::dontSendNotification);
        else
            pageBox.setText (w.page, juce::dontSendNotification);   // a page the window doesn't have: its name, nothing picked
        setShown (pageBox, ! canvas.layout.pages.isEmpty() && t != GuiWidget::Type::registration);

        if (w.isControl() || custom || (t == GuiWidget::Type::label && PanelParts::labelDisplayOf (w) == "readout"))
        {
            param.setText (w.param, juce::dontSendNotification);
            setShown (param, true, paramCaption (t == GuiWidget::Type::label ? "Shows the value of" : "Parameter", w.param));
        }

        if (w.isLive())
        {
            source.clear (juce::dontSendNotification);
            juce::StringArray sources (canvas.signalSources);
            sources.addArray (canvas.displaySources);

            if (w.source.isNotEmpty())
                sources.addIfNotAlreadyThere (w.source);

            source.addItemList (sources, 1);
            source.setSelectedId (sources.indexOf (w.source) + 1, juce::dontSendNotification);
            setShown (source, true, "Watches");
        }
        else if (t == GuiWidget::Type::filter)
        {
            // A filter curve can follow the plugin's live cutoff (a display): it opens and closes
            // as it plays. "Its cutoff knob only" leaves it on the knob.
            source.clear (juce::dontSendNotification);
            juce::StringArray sources { knobOnly() };
            sources.addArray (canvas.displaySources);

            if (w.source.isNotEmpty())
                sources.addIfNotAlreadyThere (w.source);

            source.addItemList (sources, 1);
            source.setSelectedId (w.source.isEmpty() ? 1 : sources.indexOf (w.source) + 1, juce::dontSendNotification);
            setShown (source, true, "Live cutoff");
        }

        roleNames = w.rolesFor();

        for (int i = 0; i < 4; ++i)
        {
            if (i >= roleNames.size())
                continue;

            const auto found = w.roles.find (roleNames[i]);
            const auto id = found != w.roles.end() ? found->second : juce::String();
            roles[i].setText (id, juce::dontSendNotification);
            setShown (roles[i], true, paramCaption (roleNames[i].substring (0, 1).toUpperCase() + roleNames[i].substring (1), id));
        }

        mode.clear (juce::dontSendNotification);

        if (t == GuiWidget::Type::meter)
        {
            mode.addItemList (meterModeNames, 1);
            mode.setSelectedId (juce::jmax (0, meterModes.indexOf (w.mode)) + 1, juce::dontSendNotification);
            setShown (mode, true);
        }
        else if (t == GuiWidget::Type::filter)
        {
            mode.addItemList (filterModeNames, 1);
            mode.setSelectedId (juce::jmax (0, filterModes.indexOf (w.mode)) + 1, juce::dontSendNotification);
            setShown (mode, true);
        }

        const bool isText = t == GuiWidget::Type::label || t == GuiWidget::Type::group || t == GuiWidget::Type::registration
                         || t == GuiWidget::Type::tabs;
        const bool hasCaption = t != GuiWidget::Type::image && t != GuiWidget::Type::shape && t != GuiWidget::Type::preset
                             && t != GuiWidget::Type::keyboard && t != GuiWidget::Type::tabs;

        label.setText (w.label, juce::dontSendNotification);
        setShown (label, hasCaption, isText ? "Text" : "Caption");

        textSize.setText (w.fontSize > 0.0f ? juce::String (w.fontSize, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") : juce::String(),
                          juce::dontSendNotification);
        boldToggle.setToggleState (w.bold, juce::dontSendNotification);
        setShown (textSize, isText && t != GuiWidget::Type::registration);   // the badge's words fit its height

        // Its text's font.
        int fontId = 1;

        for (int i = 0; i < (int) FontLibrary::families().size(); ++i)
            if (FontLibrary::families()[(size_t) i].name == FontLibrary::canonical (w.font))
                fontId = i + 2;

        fontBox.setSelectedId (fontId, juce::dontSendNotification);
        setShown (fontBox, takesFont (w));
        setShown (fontSample, takesFont (w));
        fontSample.family = w.font;
        fontSample.text = w.label.isNotEmpty() && t != GuiWidget::Type::tabs ? w.label : juce::String ("Sonify 0123456789");
        fontSample.repaint();

        // The preset manager's corners: square to round ends.
        if (t == GuiWidget::Type::preset)
        {
            const auto half = juce::jmax (1.0f, juce::jmin ((float) w.bounds.getWidth(), (float) w.bounds.getHeight()) * 0.5f);
            const auto rounding = w.rounding >= 0.0f ? w.rounding : juce::jmin (1.0f, 4.0f / half);
            presetRounding.setValue (juce::roundToInt (rounding * 100.0f), juce::dontSendNotification);
            setShown (presetRounding, true);
        }

        if (Looks::takesLook (w) && canvas.looks != nullptr)
        {
            // Every look of its kind: the plugin's own, mine, then the built-in ones.
            style.clear (juce::dontSendNotification);
            lookNames.clear();

            const auto& worn = canvas.looks->lookFor (w);
            const auto shown = worn.name;
            auto origin = Looks::Origin::builtIn;
            bool first = true;

            style.addItem ("Standard look", standardLookId);

            bool poolHeading = false;

            for (const auto* look : canvas.looks->listFor (Looks::kindOf (w)))
            {
                if (first || look->origin != origin)
                {
                    origin = look->origin;
                    style.addSectionHeading (origin == Looks::Origin::project ? "This plugin's own"
                                           : origin == Looks::Origin::mine    ? "My looks"
                                                                              : "Built in");
                    first = false;
                }

                // The inspiration pool's, after the other built-in ones.
                if (origin == Looks::Origin::builtIn && look->pool && ! poolHeading)
                {
                    style.addSectionHeading ("Inspiration pool");
                    poolHeading = true;
                }

                lookNames.add (look->name);
                style.addItem (look->name, lookNames.size());
            }

            style.setSelectedId (w.style.isEmpty() ? standardLookId : lookNames.indexOf (shown) + 1, juce::dontSendNotification);
            setShown (style, true);
            setShown (knobStudioButton, true);

            // Back to the look it had before others were tried.
            if (w.originalKnown && w.originalStyle != w.style)
            {
                const auto name = w.originalStyle.isEmpty() ? juce::String ("the standard look")
                                                            : juce::String::fromUTF8 ("\xe2\x80\x9c") + w.originalStyle + juce::String::fromUTF8 ("\xe2\x80\x9d");
                originalLookButton.setButtonText (juce::String::fromUTF8 ("\xe2\x86\xa9  Back to its original look: ") + name);
                originalLookButton.setTooltip ("The look it wore before you tried others");
                setShown (originalLookButton, true);
            }

            // A built-in look (or one of mine) changed in the Knob Studio for this plugin: as it came.
            if (worn.origin == Looks::Origin::project
                && (canvas.looks->find (shown, Looks::Origin::builtIn) != nullptr || canvas.looks->find (shown, Looks::Origin::mine) != nullptr))
            {
                restoreLookButton.setButtonText (juce::String::fromUTF8 ("Undo the Knob Studio's changes to \xe2\x80\x9c") + shown
                                                 + juce::String::fromUTF8 ("\xe2\x80\x9d"));
                restoreLookButton.setTooltip ("The look as it came, before it was changed in the Knob Studio: every control wearing it changes back");
                setShown (restoreLookButton, true);
            }
        }

        options.setText (w.options.joinIntoString (", "), juce::dontSendNotification);
        setShown (options, t == GuiWidget::Type::selector || custom);

        if (t == GuiWidget::Type::keyboard)
        {
            fillKeys (lowKey, w.lowNote, highestLowKey);
            fillKeys (highKey, w.highNote, highestKey);
            setShown (lowKey, true);
        }

        if (t == GuiWidget::Type::image)
        {
            pictureButton.setButtonText (pictureChoice (w.image));
            pictureMode.setSelectedId (juce::jmax (0, GuiLayout::pictureModes().indexOf (w.mode.isNotEmpty() ? w.mode : juce::String ("fit"))) + 1,
                                       juce::dontSendNotification);
            setShown (pictureButton, true);
            setShown (pictureMode, true);

            // Painted in code: its material, colours, texture, pattern and light are the menu's.
            if (const auto recipe = canvas.paintedRecipeOf (w); recipe.isObject())
            {
                shapeSurface = false;
                showSurface (recipe);
            }
        }

        if (t == GuiWidget::Type::shape)
            showShape (w);

        colour.setColourValue (w.colour, juce::dontSendNotification);
        colour.setStandardColour (PluginCanvas::standardColour (w));
        setShown (colour, takesColour (w) && t != GuiWidget::Type::shape, t == GuiWidget::Type::keyboard ? "Colour of the keys that are down" : "Colour");
        setShown (windowButton, true);

        if (t == GuiWidget::Type::meter)
            showMeter (w);

        if (w.takesPartDisplay())
            showPart (w);

        instruction.setTextToShowWhenEmpty ("e.g. make it bigger and gold", Theme::muted);
    }

    // The panel's one light: in the menus of what it lights (the looks, painted pictures; a
    // shape has its own button, the window too).
    if (widget >= 0 && canvas.hasPanel())
    {
        const auto& w = canvas.layout.widgets[(size_t) widget];
        setShown (globalLightButton, Looks::takesLook (w) || canvas.paintedRecipeOf (w).isObject());
    }

    showTabs();
    resized();
    repaint();
}

void PluginCanvas::ElementMenu::followDrag()
{
    const juce::ScopedValueSetter<bool> quiet (updating, true);

    if (several.empty() && juce::isPositiveAndBelow (widget, (int) canvas.layout.widgets.size()))
    {
        showObject (canvas.layout.widgets[(size_t) widget]);
    }
    else if (several.empty() && widget < 0)
    {
        const auto& l = canvas.layout;
        widthBox.setText (juce::String (l.width), juce::dontSendNotification);
        heightBox.setText (juce::String (l.height), juce::dontSendNotification);
        subtitle = juce::String (l.width) + juce::String::fromUTF8 (" \xc3\x97 ") + juce::String (l.height);
        repaint (headerArea);
    }
}

void PluginCanvas::ElementMenu::commitTyped()
{
    auto* box = typedBox;
    typedBox = nullptr;

    if (box == nullptr)
        return;

    if (box == &pagesBox)
    {
        if (widget < 0 && several.empty())
            canvas.setPages (juce::StringArray::fromTokens (pagesBox.getText(), ",", ""));
    }
    else if (box == &widthBox || box == &heightBox)
    {
        applyWindow();
    }
    else if (! several.empty())
    {
        // Several selected together show none of these boxes (their colour only).
    }
    else if (box == &xBox || box == &yBox || box == &wBox || box == &hBox || box == &angleBox)
    {
        applyObject();
    }
    else
    {
        apply();   // a parameter, a role or the text size
    }
}

void PluginCanvas::ElementMenu::showMeter (const GuiWidget& w)
{
    const auto s = MeterPaint::styleOf (w);
    const auto standard = MeterPaint::standardStyle();

    auto chosen = [&w] (const char* key)
    {
        const auto found = w.settings.find (key);
        return found != w.settings.end() && found->second.toString().isNotEmpty()
                   ? GuiLayout::colourFromString (found->second.toString(), juce::Colours::transparentBlack)
                   : juce::Colours::transparentBlack;
    };

    meterDisplay.setSelectedId (juce::jmax (0, MeterPaint::displays().indexOf (s.display)) + 1, juce::dontSendNotification);

    const std::pair<ColourSwatchButton*, const char*> swatches[] { { &meterLow, "colorLow" }, { &meterMid, "colorMid" },
                                                                    { &meterHigh, "colorHigh" }, { &meterBack, "background" } };

    for (const auto& [swatch, key] : swatches)
        swatch->setColourValue (chosen (key), juce::dontSendNotification);

    meterLow.setStandardColour (standard.low);
    meterMid.setStandardColour (standard.mid);
    meterHigh.setStandardColour (standard.high);
    meterBack.setStandardColour (s.display == "vu" ? juce::Colour (0xfff1e4c3) : standard.background);

    meterSegments.setValue (s.segments, juce::dontSendNotification);
    meterGlow.setValue (juce::roundToInt (s.glow * 100.0f), juce::dontSendNotification);
    meterPeakToggle.setToggleState (s.peakHold, juce::dontSendNotification);
    meterScaleToggle.setToggleState (s.scale, juce::dontSendNotification);

    for (auto* first : std::initializer_list<juce::Component*> { &meterDisplay, &meterLow, &meterMid, &meterHigh, &meterBack, &meterPeakToggle })
        setShown (*first, true);

    setShown (meterSegments, s.display == "segments" || s.display == "dots");
    setShown (meterGlow, s.display == "bar" || s.display == "dots" || s.display == "neon");
}

void PluginCanvas::ElementMenu::applyMeter()
{
    if (updating || ! several.empty() || ! juce::isPositiveAndBelow (widget, (int) canvas.layout.widgets.size()))
        return;

    auto& w = canvas.layout.widgets[(size_t) widget];

    if (w.type != GuiWidget::Type::meter)
        return;

    const juce::ScopedValueSetter<bool> quiet (updating, true);
    const auto display = MeterPaint::displays()[juce::jlimit (0, MeterPaint::displays().size() - 1, meterDisplay.getSelectedId() - 1)];

    if (display == "bar") w.settings.erase ("display");
    else                  w.settings["display"] = display;

    const std::pair<ColourSwatchButton*, const char*> swatches[] { { &meterLow, "colorLow" }, { &meterMid, "colorMid" },
                                                                    { &meterHigh, "colorHigh" }, { &meterBack, "background" } };

    for (const auto& [swatch, key] : swatches)
    {
        if (swatch->getColourValue().isTransparent()) w.settings.erase (key);
        else                                          w.settings[key] = hexOf (swatch->getColourValue());
    }

    const auto segments = juce::roundToInt (meterSegments.getValue());
    const auto glow = juce::roundToInt (meterGlow.getValue());

    if (segments == 0) w.settings.erase ("segments");
    else               w.settings["segments"] = segments;

    if (glow == 50) w.settings.erase ("glow");
    else            w.settings["glow"] = glow / 100.0;

    for (const auto& [toggle, key] : { std::pair<juce::ToggleButton*, const char*> { &meterPeakToggle, "peakHold" }, { &meterScaleToggle, "scale" } })
    {
        if (toggle->getToggleState()) w.settings[key] = true;
        else                          w.settings.erase (key);
    }

    // Its LEDs and glow show for the displays that have them.
    showMeter (w);
    canvas.edited();
    canvas.layOutOverlays();
    resized();
    repaint();
}

void PluginCanvas::ElementMenu::showPart (const GuiWidget& w)
{
    const auto& ids = PanelParts::displaysFor (w.type);
    const auto& names = PanelParts::displayNamesFor (w.type);

    if (ids.isEmpty())
        return;

    const auto current = w.type == GuiWidget::Type::lamp     ? PanelParts::lampDisplayOf (w)
                       : w.type == GuiWidget::Type::selector ? PanelParts::selectorDisplayOf (w)
                       : w.type == GuiWidget::Type::keyboard ? PanelParts::keyboardDisplayOf (w)
                                                             : PanelParts::labelDisplayOf (w);

    partDisplay.clear (juce::dontSendNotification);
    partDisplay.addItemList (names, 1);
    partDisplay.setSelectedId (juce::jmax (0, ids.indexOf (current)) + 1, juce::dontSendNotification);
    setShown (partDisplay, true);

    followToggle.setToggleState (w.type == GuiWidget::Type::lamp && PanelParts::lampFollows (w), juce::dontSendNotification);
    setShown (followToggle, w.type == GuiWidget::Type::lamp && current != "tube" && current != "eye");

    const auto arrows = w.settingText ("arrows").trim().toLowerCase();
    arrowsBox.setSelectedId (juce::jmax (0, arrowIds.indexOf (arrows)) + 1, juce::dontSendNotification);
    setShown (arrowsBox, w.type == GuiWidget::Type::label && current == "plain");
}

void PluginCanvas::ElementMenu::applyPart()
{
    if (updating || ! several.empty() || ! juce::isPositiveAndBelow (widget, (int) canvas.layout.widgets.size()))
        return;

    auto& w = canvas.layout.widgets[(size_t) widget];

    if (! w.takesPartDisplay())
        return;

    const juce::ScopedValueSetter<bool> quiet (updating, true);
    const auto& ids = PanelParts::displaysFor (w.type);
    const auto display = ids[juce::jlimit (0, ids.size() - 1, partDisplay.getSelectedId() - 1)];
    const auto before = w.settingText ("display");

    // The standard (the first) needs no setting.
    if (display == ids[0]) w.settings.erase ("display");
    else                   w.settings["display"] = display;

    if (w.type == GuiWidget::Type::lamp && display != "tube" && display != "eye")
    {
        if (followToggle.getToggleState()) w.settings["follow"] = true;
        else                               w.settings.erase ("follow");
    }

    if (w.type == GuiWidget::Type::label)
    {
        const auto arrows = arrowIds[juce::jlimit (0, arrowIds.size() - 1, arrowsBox.getSelectedId() - 1)];

        if (arrows == "none") w.settings.erase ("arrows");
        else                  w.settings["arrows"] = arrows;
    }

    canvas.edited();
    canvas.repaint();

    // A new display can bring other rows (a readout's parameter, a lamp's follow).
    if (before != w.settingText ("display"))
    {
        showFor (widget);
        canvas.layOutOverlays();
    }
    else
    {
        showPart (w);
        resized();
        repaint();
    }
}

void PluginCanvas::ElementMenu::fillMaterials (bool withFlat)
{
    surfaceMaterial.clear (juce::dontSendNotification);

    if (withFlat)
        surfaceMaterial.addItem ("Flat colour (no material)", flatMaterialId);

    int id = 1;

    for (const auto& [group, choices] : materialGroups())
    {
        surfaceMaterial.addSectionHeading (group);

        for (const auto& c : choices)
            surfaceMaterial.addItem (c.label, id++);
    }
}

void PluginCanvas::ElementMenu::showSurface (const juce::var& recipe)
{
    surfaceRecipe = recipe.clone();
    fillMaterials (shapeSurface);

    const auto settings = recipe.getProperty ("settings", {});
    auto text = [&settings] (const char* key, const juce::String& fallback = {})
    {
        return settings.hasProperty (key) ? settings.getProperty (key, {}).toString() : fallback;
    };
    auto number = [&settings] (const char* key, double fallback)
    {
        return settings.hasProperty (key) ? (double) settings.getProperty (key, fallback) : fallback;
    };

    // The material (an older name, as its own one), and its colours. A shape may have none: a
    // flat colour.
    const auto materials = allMaterials();
    auto material = text ("material", shapeSurface ? juce::String() : juce::String ("powder")).trim().toLowerCase();
    const bool flat = shapeSurface && (material.isEmpty() || material == "none" || material == "flat");

    if (! flat && indexOf (materials, material) < 0)
        material = stella::paint::infoOf (stella::paint::materialNamed (material.toStdString())).name;

    if (flat)
        surfaceMaterial.setSelectedId (flatMaterialId, juce::dontSendNotification);
    else
        surfaceMaterial.setSelectedId (juce::jmax (0, indexOf (materials, material)) + 1, juce::dontSendNotification);

    surfaceFinish.setSelectedId (juce::jmax (0, indexOf (finishChoices(), text ("finish", "natural"))) + 1, juce::dontSendNotification);

    juce::Colour main, second;
    materialColours (flat ? juce::String ("smooth") : material, main, second);
    const auto mainText = recipe.getProperty ("color", settings.getProperty ("color", {})).toString();
    const auto secondText = text ("color2");
    surfaceColour.setColourValue (mainText.isNotEmpty() ? GuiLayout::colourFromString (mainText, main) : main, juce::dontSendNotification);
    surfaceColour2.setColourValue (secondText.isNotEmpty() ? GuiLayout::colourFromString (secondText, second)
                                                           : (flat ? surfaceColour.getColourValue() : second),
                                   juce::dontSendNotification);
    lastMain = surfaceColour.getColourValue();

    textureSize.setValue (number ("scale", 1.0), juce::dontSendNotification);
    textureStrength.setValue (number ("grain", 1.0), juce::dontSendNotification);
    keyLight.setValue (number ("key", 0.6), juce::dontSendNotification);

    // Its first pattern layer: from the list, or a name with its settings beside it.
    juce::var layer;

    if (const auto* list = settings.getProperty ("patterns", {}).getArray(); list != nullptr && ! list->isEmpty())
        layer = list->getFirst();
    else if (settings.hasProperty ("pattern"))
        layer = settings.getProperty ("pattern", {}).isObject() ? settings.getProperty ("pattern", {}) : juce::var();

    auto layerText = [&layer, &settings] (const char* key, const char* shorthand)
    {
        if (layer.isObject())
            return layer.getProperty (key, {}).toString();

        return juce::String (key) == "pattern" ? settings.getProperty ("pattern", {}).toString() : settings.getProperty (shorthand, {}).toString();
    };

    const auto patternName = layerText ("pattern", "pattern");
    patternBox.setSelectedId (juce::jmax (0, indexOf (patternChoices(), patternName)) + 1, juce::dontSendNotification);
    blendBox.setSelectedId (juce::jmax (0, indexOf (blendChoices(), layerText ("blend", "patternBlend"))) + 1, juce::dontSendNotification);

    const auto inkText = layerText ("color", "patternColor");
    patternColour.setColourValue (inkText.isNotEmpty() ? GuiLayout::colourFromString (inkText, juce::Colours::transparentBlack) : juce::Colours::transparentBlack,
                                  juce::dontSendNotification);
    patternColour.setStandardColour (juce::Colours::black);

    auto layerNumber = [&layerText] (const char* key, const char* shorthand, double fallback)
    {
        const auto v = layerText (key, shorthand);
        return v.isNotEmpty() ? v.getDoubleValue() : fallback;
    };

    patternOpacity.setValue (layerNumber ("opacity", "patternOpacity", 0.5), juce::dontSendNotification);
    patternSize.setValue (layerNumber ("size", "patternSize", 24.0), juce::dontSendNotification);
    patternAngle.setValue (layerNumber ("angle", "patternAngle", 0.0), juce::dontSendNotification);

    for (auto* first : std::initializer_list<juce::Component*> { &surfaceMaterial, &surfaceColour, &patternBox })
        setShown (*first, true);

    showSurfaceRows();
}

void PluginCanvas::ElementMenu::showSurfaceRows()
{
    // A flat colour has no finish or texture; a pattern has its colour and size; a painted
    // picture has a key light's pool (both are lit by the global light).
    const bool flat = surfaceMaterial.getSelectedId() == flatMaterialId;
    const bool hasPattern = patternBox.getSelectedId() > 1;

    setShown (surfaceFinish, ! flat);
    setShown (textureSize, ! flat);
    setShown (textureStrength, ! flat);

    for (auto* first : std::initializer_list<juce::Component*> { &blendBox, &patternColour, &patternOpacity, &patternSize, &patternAngle })
        setShown (*first, hasPattern);

    setShown (keyLight, ! shapeSurface);

    // The window's subtitle stays its size.
    if (widget >= 0)
        subtitle = flat ? juce::String ("Flat colour") : surfaceMaterial.getText() + (shapeSurface ? juce::String() : juce::String (" (painted)"));
}

void PluginCanvas::ElementMenu::applySurface (bool materialChanged)
{
    if (updating || ! surfaceRecipe.isObject())
        return;

    const juce::ScopedValueSetter<bool> quiet (updating, true);
    auto recipe = surfaceRecipe.clone();
    auto settings = recipe.getProperty ("settings", {}).isObject() ? recipe.getProperty ("settings", {}).clone() : juce::var (new juce::DynamicObject());
    auto* s = settings.getDynamicObject();

    // The material: a new one comes in its own colours, as it comes. A shape may be a flat colour.
    const auto materials = allMaterials();
    const bool flat = surfaceMaterial.getSelectedId() == flatMaterialId;
    const auto chosen = juce::jlimit (0, (int) materials.size() - 1, surfaceMaterial.getSelectedId() - 1);
    const juce::String material (flat ? "" : materials[(size_t) chosen].name);

    if (flat) s->removeProperty ("material");
    else      s->setProperty ("material", material);

    if (materialChanged)
    {
        if (! flat)
        {
            juce::Colour main, second;
            materialColours (material, main, second);
            surfaceColour.setColourValue (main, juce::dontSendNotification);
            surfaceColour2.setColourValue (second, juce::dontSendNotification);
        }
        else
        {
            surfaceColour2.setColourValue (surfaceColour.getColourValue(), juce::dontSendNotification);
        }

        surfaceFinish.setSelectedId (1, juce::dontSendNotification);
    }
    else if (surfaceColour2.getColourValue() == lastMain && surfaceColour.getColourValue() != lastMain)
    {
        // The second colour follows the main one until it's chosen for itself.
        surfaceColour2.setColourValue (surfaceColour.getColourValue(), juce::dontSendNotification);
    }

    lastMain = surfaceColour.getColourValue();

    recipe.getDynamicObject()->setProperty ("color", hexOf (surfaceColour.getColourValue()));
    s->removeProperty ("color");
    s->removeProperty ("colour");
    s->removeProperty ("colour2");

    if (surfaceColour2.getColourValue() == surfaceColour.getColourValue() && shapeSurface)
        s->removeProperty ("color2");
    else
        s->setProperty ("color2", hexOf (surfaceColour2.getColourValue()));

    const auto finish = juce::String (finishChoices()[(size_t) juce::jlimit (0, (int) finishChoices().size() - 1, surfaceFinish.getSelectedId() - 1)].name);

    if (finish == "natural" || flat) s->removeProperty ("finish");
    else                             s->setProperty ("finish", finish);

    const auto scale = juce::roundToInt (textureSize.getValue() * 100.0) / 100.0;
    const auto grain = juce::roundToInt (textureStrength.getValue() * 100.0) / 100.0;

    if (shapeSurface && (flat || scale == 1.0)) s->removeProperty ("scale");
    else                                        s->setProperty ("scale", scale);

    if (shapeSurface && (flat || grain == 1.0)) s->removeProperty ("grain");
    else                                        s->setProperty ("grain", grain);

    if (! shapeSurface)
        s->setProperty ("key", juce::roundToInt (keyLight.getValue() * 100.0) / 100.0);   // its light: the global light

    // The pattern: the first layer is the menu's; any more the AI painted stay over it.
    juce::Array<juce::var> layers;

    if (const auto* old = settings.getProperty ("patterns", {}).getArray())
        for (int i = 1; i < old->size(); ++i)
            layers.add (old->getReference (i));

    for (const auto* key : { "pattern", "patternColor", "patternColour", "patternOpacity", "patternBlend", "patternSize", "patternScale",
                             "patternAngle", "patternWidth", "patternDepth" })
        s->removeProperty (key);

    const auto pattern = juce::String (patternChoices()[(size_t) juce::jlimit (0, (int) patternChoices().size() - 1, patternBox.getSelectedId() - 1)].name);

    if (pattern != "none")
    {
        auto* layer = new juce::DynamicObject();
        layer->setProperty ("pattern", pattern);
        layer->setProperty ("blend", juce::String (blendChoices()[(size_t) juce::jlimit (0, (int) blendChoices().size() - 1, blendBox.getSelectedId() - 1)].name));

        if (! patternColour.getColourValue().isTransparent())
            layer->setProperty ("color", hexOf (patternColour.getColourValue()));

        layer->setProperty ("opacity", juce::roundToInt (patternOpacity.getValue() * 100.0) / 100.0);
        layer->setProperty ("size", juce::roundToInt (patternSize.getValue()));
        layer->setProperty ("angle", juce::roundToInt (patternAngle.getValue()));
        layers.insert (0, juce::var (layer));
    }

    if (layers.isEmpty()) s->removeProperty ("patterns");
    else                  s->setProperty ("patterns", layers);

    recipe.getDynamicObject()->setProperty ("settings", settings);
    surfaceRecipe = recipe;

    showSurfaceRows();
    canvas.layOutOverlays();   // a pattern's rows come or go
    resized();
    repaint();

    if (! shapeSurface)
    {
        canvas.repaintPainted (recipe);
        return;
    }

    // A shape: its colour and surface settings (the rest of its settings stay as they are).
    if (! juce::isPositiveAndBelow (widget, (int) canvas.layout.widgets.size()))
        return;

    auto& w = canvas.layout.widgets[(size_t) widget];
    w.colour = surfaceColour.getColourValue();

    for (const auto* key : { "material", "finish", "color2", "scale", "grain", "patterns", "pattern", "patternColor", "patternOpacity",
                             "patternBlend", "patternSize", "patternAngle", "patternWidth", "patternDepth" })
    {
        if (s->hasProperty (key))
            w.settings[key] = s->getProperty (key);
        else
            w.settings.erase (key);
    }

    canvas.edited();
    canvas.repaint();
}

//==============================================================================
void PluginCanvas::ElementMenu::showObject (const GuiWidget& w)
{
    const bool knob = w.type == GuiWidget::Type::knob;
    xBox.setText (juce::String (w.bounds.getX()), juce::dontSendNotification);
    yBox.setText (juce::String (w.bounds.getY()), juce::dontSendNotification);
    wBox.setText (juce::String (w.bounds.getWidth()), juce::dontSendNotification);
    hBox.setText (juce::String (w.bounds.getHeight()), juce::dontSendNotification);
    hBox.setEnabled (! knob);   // a knob stays round: its width says its size
    angleBox.setText (shortNumber (w.angle, 1), juce::dontSendNotification);
    opacitySlider.setValue (juce::roundToInt (w.shownOpacity() * 100.0f), juce::dontSendNotification);

    setShown (xBox, true);
    setShown (wBox, true, knob ? "Size (across, with its caption below)" : "Size (width, height)");
    setShown (angleBox, w.type != GuiWidget::Type::registration);
    setShown (opacitySlider, w.type != GuiWidget::Type::registration);
}

void PluginCanvas::ElementMenu::applyObject()
{
    if (updating || ! several.empty() || ! juce::isPositiveAndBelow (widget, (int) canvas.layout.widgets.size()))
        return;

    const juce::ScopedValueSetter<bool> quiet (updating, true);
    auto& w = canvas.layout.widgets[(size_t) widget];
    const auto x = juce::jlimit (-GuiLayout::maxWidth, GuiLayout::maxWidth * 2, xBox.getText().getIntValue());
    const auto y = juce::jlimit (-GuiLayout::maxHeight, GuiLayout::maxHeight * 2, yBox.getText().getIntValue());
    const auto width = juce::jlimit (8, GuiLayout::maxWidth, wBox.getText().getIntValue());
    auto height = juce::jlimit (8, GuiLayout::maxHeight, hBox.getText().getIntValue());

    if (w.type == GuiWidget::Type::knob)
        height = width + GuiLayout::captionHeight;

    auto angle = std::fmod (angleBox.getText().getFloatValue(), 360.0f);

    if (angle > 180.0f)        angle -= 360.0f;
    else if (angle <= -180.0f) angle += 360.0f;

    angle = (float) juce::roundToInt (angle * 10.0f) / 10.0f;
    const auto opacity = juce::jlimit (0.0f, 1.0f, (float) opacitySlider.getValue() / 100.0f);
    const juce::Rectangle<int> bounds (x, y, width, height);
    const bool object = w.type != GuiWidget::Type::registration;

    // Nothing new (a box only lost the keyboard focus): nothing to save.
    if (bounds == w.bounds && (! object || (angle == w.angle && std::abs (opacity - w.opacity) < 0.005f)))
    {
        showObject (w);
        return;
    }

    w.bounds = bounds;

    if (object)
    {
        w.angle = angle;
        w.opacity = opacity;
    }

    showObject (w);

    canvas.edited();
    canvas.layOutOverlays();
    canvas.repaint();
}

//==============================================================================
void PluginCanvas::ElementMenu::showShape (const GuiWidget& w)
{
    const auto kind = w.shapeKind();

    // Its corners' rounding, 0 to 100 % (one from before it in pixels, as it looked).
    const auto half = juce::jmax (1.0f, juce::jmin ((float) w.bounds.getWidth(), (float) w.bounds.getHeight()) * 0.5f);
    const auto rounding = w.settings.count ("rounding") > 0 ? w.number ("rounding", 0.0f) : w.number ("radius", 0.0f) / half;
    cornerRadius.setValue (juce::roundToInt (juce::jlimit (0.0f, 1.0f, rounding) * 100.0f), juce::dontSendNotification);
    starPoints.setValue (w.number ("points", 5.0f), juce::dontSendNotification);
    starInner.setValue (w.number ("inner", 0.45f), juce::dontSendNotification);
    topPoint.setValue (w.number ("centerX", 0.0f), juce::dontSendNotification);
    middleX.setValue (w.number ("centerX", 0.0f), juce::dontSendNotification);
    middleY.setValue (w.number ("centerY", 0.0f), juce::dontSendNotification);
    leanX.setValue (w.number ("leanX", 0.0f), juce::dontSendNotification);
    leanY.setValue (w.number ("leanY", 0.0f), juce::dontSendNotification);

    setShown (cornerRadius, kind == "rectangle" || kind == "triangle" || kind == "star");
    setShown (starPoints, kind == "star");
    setShown (starInner, kind == "star");
    setShown (topPoint, kind == "triangle");
    setShown (middleX, kind == "circle" || kind == "star" || kind == "drawn");
    setShown (middleY, kind == "circle" || kind == "star" || kind == "drawn");
    setShown (leanX, kind != "line");
    setShown (leanY, kind != "line");
    setShown (layerStyleButton, true);

    // A line: its width, arrowheads and ends; straight again while it's bent.
    const bool line = kind == "line";
    lineWidthSlider.setValue (ObjectPaint::lineWidthOf (w), juce::dontSendNotification);
    lineArrowBox.setSelectedId (juce::jmax (0, lineArrowIds.indexOf (w.settingText ("arrow").trim().toLowerCase())) + 1, juce::dontSendNotification);
    lineCapBox.setSelectedId (juce::jmax (0, lineCapIds.indexOf (w.settingText ("cap").trim().toLowerCase())) + 1, juce::dontSendNotification);
    setShown (lineWidthSlider, line);
    setShown (lineArrowBox, line);
    setShown (lineCapBox, line);
    setShown (straightenButton, line && ObjectPaint::isBent (w));

    // Its surface: a flat colour, or a material with its finish, colours, texture and pattern.
    auto* settings = new juce::DynamicObject();

    for (const auto& [key, value] : w.settings)
        if (! value.isVoid())
            settings->setProperty (key, value);

    auto* recipe = new juce::DynamicObject();
    recipe->setProperty ("color", hexOf (w.colour.isTransparent() ? PluginCanvas::standardColour (w).withAlpha (0.9f) : w.colour));
    recipe->setProperty ("settings", juce::var (settings));

    shapeSurface = true;
    showSurface (juce::var (recipe));
}

void PluginCanvas::ElementMenu::applyShape()
{
    if (updating || ! several.empty() || ! juce::isPositiveAndBelow (widget, (int) canvas.layout.widgets.size()))
        return;

    auto& w = canvas.layout.widgets[(size_t) widget];

    if (w.type != GuiWidget::Type::shape)
        return;

    const auto kind = w.shapeKind();
    auto put = [&w] (const char* key, double value, double standard, double step)
    {
        const auto v = juce::roundToInt (value / step) * step;

        if (std::abs (v - standard) < step * 0.5) w.settings.erase (key);
        else                                      w.settings[key] = v;
    };

    if (kind == "rectangle" || kind == "triangle" || kind == "star")
    {
        w.settings.erase ("radius");   // a rectangle's from before, in pixels
        put ("rounding", cornerRadius.getValue() / 100.0, 0.0, 0.01);
    }

    if (kind == "star")
    {
        put ("points", starPoints.getValue(), 5.0, 1.0);
        put ("inner", starInner.getValue(), 0.45, 0.01);
    }

    if (kind == "triangle")
        put ("centerX", topPoint.getValue(), 0.0, 0.01);

    if (kind == "circle" || kind == "star" || kind == "drawn")
    {
        put ("centerX", middleX.getValue(), 0.0, 0.01);
        put ("centerY", middleY.getValue(), 0.0, 0.01);
    }

    if (kind == "line")
    {
        put ("lineWidth", lineWidthSlider.getValue(), 4.0, 0.5);

        const auto arrow = lineArrowIds[juce::jlimit (0, lineArrowIds.size() - 1, lineArrowBox.getSelectedId() - 1)];
        const auto cap = lineCapIds[juce::jlimit (0, lineCapIds.size() - 1, lineCapBox.getSelectedId() - 1)];

        if (arrow == "none") w.settings.erase ("arrow");
        else                 w.settings["arrow"] = arrow;

        if (cap == "round") w.settings.erase ("cap");
        else                w.settings["cap"] = cap;

        // Its box fitted round its new stroke (a wider line, arrowheads).
        if (! w.isTurned())
            ObjectPaint::setLinePoints (w, ObjectPaint::linePoints (w), true);
    }
    else
    {
        put ("leanX", leanX.getValue(), 0.0, 0.01);
        put ("leanY", leanY.getValue(), 0.0, 0.01);
    }

    canvas.edited();
    canvas.repaint();
}

void PluginCanvas::ElementMenu::showForSeveral (const std::vector<int>& indexes)
{
    std::vector<int> valid;

    for (const auto i : indexes)
        if (juce::isPositiveAndBelow (i, (int) canvas.layout.widgets.size()))
            valid.push_back (i);

    if (valid.size() < 2)
    {
        showFor (valid.empty() ? -1 : valid.front());
        return;
    }

    const juce::ScopedValueSetter<bool> quiet (updating, true);
    widget = -1;
    several = valid;
    typedBox = nullptr;   // their boxes aren't shown

    for (auto& row : rows.rows)
        row.shown = false;

    title = juce::String ((int) several.size()) + " elements";
    subtitle.clear();
    showActions (true);

    // One colour for all of them that take one (from the first of them).
    for (const auto i : several)
    {
        const auto& w = canvas.layout.widgets[(size_t) i];

        if (takesColour (w))
        {
            colour.setColourValue (w.colour, juce::dontSendNotification);
            colour.setStandardColour (PluginCanvas::standardColour (w));
            setShown (colour, true, "Colour (all of them)");
            break;
        }
    }

    removeButton.setVisible (true);
    setShown (windowButton, true);

    // Sonify AI hears about one element at a time.
    instruction.setVisible (false);
    sendButton.setVisible (false);

    showTabs();
    resized();
    repaint();
}

void PluginCanvas::ElementMenu::showActions (bool forElements)
{
    copyButton.setVisible (forElements);
    duplicateButton.setVisible (forElements);
    layerBox.setVisible (forElements);
    pasteButton.setVisible (true);
    pasteButton.setEnabled (! canvas.clipboardElements().empty());

    // The layers, a list of their own: several go all the way to the front or the back.
    layerBox.clear (juce::dontSendNotification);
    layerBox.setText ("Layer", juce::dontSendNotification);

    if (! forElements)
        return;

    const auto last = (int) canvas.layout.widgets.size() - 1;
    const bool one = several.empty();

    layerBox.addItem ("Bring to front", 1);
    layerBox.setItemEnabled (1, ! one || widget < last);

    if (one)
    {
        layerBox.addItem ("Bring forward", 2);
        layerBox.addItem ("Send backward", 3);
        layerBox.setItemEnabled (2, widget < last);
        layerBox.setItemEnabled (3, widget > 0);
    }

    layerBox.addItem ("Send to back", 4);
    layerBox.setItemEnabled (4, ! one || widget > 0);
}

void PluginCanvas::ElementMenu::layer (int choice)
{
    if (! several.empty())
    {
        if (choice == 1 || choice == 4)
            canvas.reorderSelection (choice == 1);
    }
    else if (choice >= 1 && choice <= 4)
    {
        const Order orders[] { Order::front, Order::forward, Order::backward, Order::back };
        canvas.reorder (widget, orders[choice - 1]);
    }

    canvas.grabKeyboardFocus();   // the layer keys work again
}

std::vector<int> PluginCanvas::ElementMenu::targets() const
{
    if (! several.empty())
        return several;

    return widget >= 0 ? std::vector<int> { widget } : std::vector<int>();
}

juce::String PluginCanvas::ElementMenu::paramIdFor (const juce::String& typed) const
{
    const auto text = typed.trim();

    if (text.isEmpty())
        return {};

    for (const auto& p : canvas.params)
        if (p.id == text)
            return p.id;

    for (const auto& p : canvas.params)
        if (p.id.equalsIgnoreCase (text) || p.name.equalsIgnoreCase (text))
            return p.id;

    // One the plugin doesn't have yet: an id has no spaces.
    return juce::StringArray::fromTokens (text, " \t", "").joinIntoString ("_");
}

juce::String PluginCanvas::ElementMenu::paramCaption (const juce::String& caption, const juce::String& id) const
{
    // Nothing known before the first build.
    if (id.isEmpty() || canvas.params.isEmpty() || canvas.paramById (id) != nullptr)
        return caption;

    return caption + juce::String::fromUTF8 ("  \xc2\xb7  not in the plugin yet");
}

int PluginCanvas::ElementMenu::getIdealHeight() const
{
    // The tallest tab's: the menu keeps its size (and its tabs their place) from tab to tab.
    int rowsHeight = rows.measure (rows.tab);

    for (const auto& name : tabStrip.names)
        rowsHeight = juce::jmax (rowsHeight, rows.measure (name));

    return headerHeight + actionsHeight + (tabStrip.isVisible() ? tabsHeight : 0) + rowsHeight + 4 + footerSize();
}

void PluginCanvas::ElementMenu::resized()
{
    auto area = getLocalBounds();

    headerArea = area.removeFromTop (headerHeight);
    closeButton.setBounds (headerArea.withTrimmedRight (8).removeFromRight (30).withSizeKeepingCentre (30, 28));
    removeButton.setBounds (headerArea.withTrimmedRight (8 + 30 + 6).removeFromRight (64).withSizeKeepingCentre (64, 26));

    // The actions, in a row: the window's menu has Paste only.
    actionsArea = area.removeFromTop (actionsHeight);
    auto strip = actionsArea.reduced (pad, 7);

    if (! copyButton.isVisible())
    {
        pasteButton.setBounds (strip.removeFromLeft (80));
    }
    else
    {
        const std::pair<juce::Component*, float> parts[] { { &copyButton, 1.0f }, { &pasteButton, 1.0f },
                                                           { &duplicateButton, 1.4f }, { &layerBox, 1.4f } };
        const auto gap = 6;
        const auto unit = (float) (strip.getWidth() - gap * 3) / 4.8f;

        for (int i = 0; i < 4; ++i)
        {
            const auto width = i == 3 ? strip.getWidth() : juce::roundToInt (unit * parts[i].second);
            parts[i].first->setBounds (strip.removeFromLeft (width));
            strip.removeFromLeft (gap);
        }
    }

    footerArea = area.removeFromBottom (footerSize());

    // The tabs, then the tab's rows: they scroll when the menu is shorter than they are.
    if (tabStrip.isVisible())
        tabStrip.setBounds (area.removeFromTop (tabsHeight).reduced (pad, 0).withTrimmedTop (2));

    area.removeFromTop (4);
    viewport.setBounds (area.reduced (pad, 0));
    auto width = viewport.getWidth();

    if (rows.layOut (width) > viewport.getHeight())
        rows.layOut (width - viewport.getScrollBarThickness() - 4);

    auto foot = footerArea.reduced (pad, 0);
    foot.removeFromTop (34);
    auto box = foot.removeFromTop (66);
    sendButton.setBounds (box.removeFromRight (34).removeFromBottom (34));
    box.removeFromRight (6);
    instruction.setBounds (box);
}

void PluginCanvas::ElementMenu::paint (juce::Graphics& g)
{
    const auto box = getLocalBounds().toFloat();

    juce::DropShadow (juce::Colours::black.withAlpha (0.5f), 16, { 0, 4 }).drawForRectangle (g, getLocalBounds());
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (box, 9.0f);
    g.setColour (Theme::outline.brighter (0.15f));
    g.drawRoundedRectangle (box.reduced (0.5f), 9.0f, 1.0f);

    // What it edits.
    auto header = headerArea.reduced (pad, 0).withTrimmedRight (removeButton.isVisible() ? 34 + 70 : 34);
    const auto titleFont = Theme::font (15.5f, true);
    g.setColour (Theme::text);
    g.setFont (titleFont);
    g.drawText (title, header, juce::Justification::centredLeft, true);

    if (subtitle.isNotEmpty())
    {
        const auto titleWidth = juce::GlyphArrangement::getStringWidthInt (titleFont, title) + 10;
        g.setColour (Theme::muted);
        g.setFont (Theme::font (13.5f));
        g.drawText (subtitle, header.withTrimmedLeft (titleWidth), juce::Justification::centredLeft, true);
    }

    g.setColour (Theme::outline);
    g.fillRect (pad, headerArea.getBottom() - 1, getWidth() - pad * 2, 1);
    g.fillRect (pad, actionsArea.getBottom() - 1, getWidth() - pad * 2, 1);

    if (footerArea.isEmpty())
        return;

    g.fillRect (pad, footerArea.getY(), getWidth() - pad * 2, 1);

    // Sonify AI's corner.
    g.setColour (askYellow);
    g.setFont (Theme::font (13.5f, true));
    g.drawText (widget < 0 ? "Tell Fanan AI what to do with the window" : "Tell Fanan AI what to do with it",
                footerArea.reduced (pad, 0).removeFromTop (34), juce::Justification::centredLeft, true);
}

//==============================================================================
void PluginCanvas::ElementMenu::apply()
{
    if (updating)
        return;

    if (! several.empty())
    {
        applySeveral();
        return;
    }

    if (! juce::isPositiveAndBelow (widget, (int) canvas.layout.widgets.size()))
        return;

    auto& w = canvas.layout.widgets[(size_t) widget];

    // The Type list changes what it is; one that isn't in the list (a programmed element, the
    // registration badge) stays what it is.
    bool listed = false;

    for (int i = 0; i < numTypes; ++i)
        listed = listed || typeOrder[i] == w.type;

    const auto chosen = listed ? type.getSelectedId() : 0;
    const bool toShape = chosen >= shapeIds;
    const auto newKind = toShape ? shapeKindIds[juce::jlimit (0, shapeKindIds.size() - 1, chosen - shapeIds)] : juce::String();
    const auto newType = chosen <= 0 ? w.type
                       : (toShape ? GuiWidget::Type::shape : typeOrder[juce::jlimit (0, numTypes - 1, chosen - 1)]);

    if (newType != w.type)
    {
        w.bounds = toShape ? shapeBox (newKind, w.bounds.getX(), w.bounds.getY())
                           : PluginCanvas::defaultBounds (newType, w.bounds.getX(), w.bounds.getY());
        w.type = newType;
        w.roles.clear();
        w.mode.clear();
        w.style.clear();

        if (Looks::takesLook (w))
            w.style = canvas.commonLook (w);   // the look its new kind mostly wears here

        if (toShape)
            w.settings["shape"] = newKind;

        if (newKind == "drawn" && w.path.isEmpty())
            w.path = PluginCanvas::defaultStarPath();

        canvas.edited();
        showFor (widget);
        canvas.layOutOverlays();
        return;
    }

    // Another kind of shape: in the same box, keeping its material, colours and the rest.
    if (toShape && newKind != w.shapeKind())
    {
        w.settings["shape"] = newKind;
        canvas.edited();
        canvas.repaint();
        showFor (widget);
        return;
    }

    // The parameters typed in: the box shows the id each became (a readout shows its parameter's value).
    if (w.isControl() || w.type == GuiWidget::Type::custom || (w.type == GuiWidget::Type::label && PanelParts::labelDisplayOf (w) == "readout"))
    {
        w.param = paramIdFor (param.getText());
        param.setText (w.param, juce::dontSendNotification);
        setShown (param, true, paramCaption ("Parameter", w.param));
    }

    if (w.isLive())
        w.source = source.getText();
    else if (w.type == GuiWidget::Type::filter)
        w.source = source.getText() == knobOnly() ? juce::String() : source.getText();

    for (int i = 0; i < roleNames.size() && i < 4; ++i)
    {
        const auto id = paramIdFor (roles[i].getText());

        if (id.isNotEmpty()) w.roles[roleNames[i]] = id;
        else                 w.roles.erase (roleNames[i]);

        roles[i].setText (id, juce::dontSendNotification);
        setShown (roles[i], true, paramCaption (roleNames[i].substring (0, 1).toUpperCase() + roleNames[i].substring (1), id));
    }

    rows.repaint();

    if (w.type == GuiWidget::Type::meter)
        w.mode = meterModes[juce::jmax (0, mode.getSelectedId() - 1)];
    else if (w.type == GuiWidget::Type::filter)
        w.mode = filterModes[juce::jmax (0, mode.getSelectedId() - 1)];
    else if (w.type == GuiWidget::Type::image)
        w.mode = GuiLayout::pictureModes()[juce::jmax (0, pictureMode.getSelectedId() - 1)];

    if (w.type != GuiWidget::Type::image && w.type != GuiWidget::Type::shape && w.type != GuiWidget::Type::preset
        && w.type != GuiWidget::Type::keyboard && w.type != GuiWidget::Type::tabs)
        w.label = label.getText();

    // Its page: only one picked in the list (a page the window doesn't have shows unpicked, and stays).
    const auto pageIndex = canvas.layout.pages.indexOf (w.page);
    const auto pageId = w.page.isEmpty() ? 1 : (pageIndex >= 0 ? pageIndex + 2 : 0);

    if (! canvas.layout.pages.isEmpty() && w.type != GuiWidget::Type::registration
        && pageBox.getSelectedId() > 0 && pageBox.getSelectedId() != pageId)
    {
        const auto chosenPage = pageBox.getSelectedId() - 2;
        const auto newPage = juce::isPositiveAndBelow (chosenPage, canvas.layout.pages.size()) ? canvas.layout.pages[chosenPage] : juce::String();

        if (newPage != w.page)
        {
            w.page = newPage;

            // Off to another page: show that page, so it stays in view and selected.
            if (newPage.isNotEmpty() && newPage != canvas.shownPageName())
                canvas.showPage (chosenPage);
        }
    }

    if (w.type == GuiWidget::Type::keyboard)
    {
        // A key picked in the lists (one not picked stays as it is); the highest stays above the
        // lowest, at least an octave up.
        const auto low = lowKey.getSelectedId() - 1, high = highKey.getSelectedId() - 1;

        if ((low >= 0 && low != w.lowNote) || (high >= 0 && high != w.highNote))
        {
            if (low >= 0)  w.lowNote = low;
            if (high >= 0) w.highNote = high;
            stella::keys::normalise (w.lowNote, w.highNote);
        }

        if (lowKey.getSelectedId() != w.lowNote + 1)
            fillKeys (lowKey, w.lowNote, highestLowKey);

        if (highKey.getSelectedId() != w.highNote + 1)
            fillKeys (highKey, w.highNote, highestKey);
    }

    bool lookChanged = false;

    if (Looks::takesLook (w))
    {
        auto chosenLook = w.style;

        if (style.getSelectedId() == standardLookId)
            chosenLook.clear();
        else if (juce::isPositiveAndBelow (style.getSelectedId() - 1, lookNames.size()))
            chosenLook = lookNames[style.getSelectedId() - 1];

        if (chosenLook != w.style)
        {
            PluginCanvas::rememberOriginalLook (w);
            w.style = chosenLook;
            lookChanged = true;
        }
    }

    if (w.type == GuiWidget::Type::selector || w.type == GuiWidget::Type::custom)
    {
        w.options = juce::StringArray::fromTokens (options.getText(), ",", "");
        w.options.trim();
        w.options.removeEmptyStrings();
    }

    if (w.type == GuiWidget::Type::label || w.type == GuiWidget::Type::group || w.type == GuiWidget::Type::tabs)
    {
        w.fontSize = juce::jlimit (0.0f, 200.0f, textSize.getText().getFloatValue());
        w.bold = boldToggle.getToggleState();
    }

    if (takesColour (w) && w.type != GuiWidget::Type::shape)   // a shape's colours are its surface's
        w.colour = colour.getColourValue();

    if (takesFont (w))
    {
        const auto chosenFont = fontBox.getSelectedId() - 2;
        w.font = juce::isPositiveAndBelow (chosenFont, (int) FontLibrary::families().size())
                     ? FontLibrary::families()[(size_t) chosenFont].name : juce::String();
        fontSample.family = w.font;

        if (w.label.isNotEmpty() && w.type != GuiWidget::Type::tabs)
            fontSample.text = w.label;

        fontSample.repaint();
    }

    if (w.type == GuiWidget::Type::preset)
        w.rounding = (float) juce::roundToInt (presetRounding.getValue()) / 100.0f;

    subtitle = w.type == GuiWidget::Type::image ? w.image : (w.type == GuiWidget::Type::shape ? subtitle : w.label);
    repaint (headerArea);
    canvas.edited();

    if (lookChanged)
    {
        showFor (widget);   // "Back to its original look" shows now
        canvas.layOutOverlays();
    }
}

void PluginCanvas::ElementMenu::applySeveral()
{
    // Only the colour shows: it goes to every one of them that takes one.
    for (const auto i : several)
        if (juce::isPositiveAndBelow (i, (int) canvas.layout.widgets.size()))
            if (auto& w = canvas.layout.widgets[(size_t) i]; takesColour (w))
                w.colour = colour.getColourValue();

    canvas.edited();
}

void PluginCanvas::ElementMenu::applyWindow()
{
    if (updating || widget >= 0 || ! several.empty())
        return;

    auto& l = canvas.layout;
    const auto newWidth = juce::jlimit (GuiLayout::minWidth, GuiLayout::maxWidth, widthBox.getText().getIntValue());
    const auto newHeight = juce::jlimit (GuiLayout::minHeight, GuiLayout::maxHeight, heightBox.getText().getIntValue());
    const bool resizedWindow = newWidth != l.width || newHeight != l.height;

    // A picture filling the window keeps filling it.
    const juce::Rectangle<int> oldWindow (0, 0, l.width, l.height);

    for (auto& w : l.widgets)
        if (w.type == GuiWidget::Type::image && w.bounds == oldWindow)
            w.bounds = { 0, 0, newWidth, newHeight };

    l.width = newWidth;
    l.height = newHeight;
    l.backgroundTop = topColour.getColourValue();
    l.backgroundBottom = bottomColour.getColourValue();

    subtitle = juce::String (l.width) + juce::String::fromUTF8 (" \xc3\x97 ") + juce::String (l.height);
    repaint (headerArea);
    canvas.edited();

    if (resizedWindow)
    {
        canvas.fitPaintedBackdrops();
        canvas.viewChanged();
    }
}

void PluginCanvas::ElementMenu::send()
{
    const auto text = instruction.getText().trim();

    if (text.isEmpty() || canvas.onAskAi == nullptr)
        return;

    instruction.clear();
    canvas.onAskAi (widget, text);
}

//==============================================================================
PluginCanvas::AiBanner::AiBanner (PluginCanvas& owner)
    : canvas (owner)
{
    chatButton.setTooltip ("See the whole conversation in Build with AI");
    chatButton.onClick = [this] { if (canvas.onOpenChat != nullptr) canvas.onOpenChat(); };

    closeButton.setButtonText (juce::String::fromUTF8 ("\xc3\x97"));
    closeButton.onClick = [this]
    {
        stopTimer();
        setVisible (false);
    };

    addAndMakeVisible (chatButton);
    addAndMakeVisible (closeButton);
}

PluginCanvas::AiBanner::~AiBanner()
{
    stopTimer();
}

void PluginCanvas::AiBanner::showWorking (const juce::String& what)
{
    text = what;
    working = true;
    dots = 0;
    startTimerHz (3);
    setVisible (true);
    canvas.layOutOverlays();
    repaint();
}

void PluginCanvas::AiBanner::showReply (const juce::String& reply)
{
    text = reply.trim().isNotEmpty() ? reply.trim() : juce::String ("Done.");
    working = false;
    stopTimer();
    setVisible (true);
    canvas.layOutOverlays();
    repaint();
}

void PluginCanvas::AiBanner::timerCallback()
{
    dots = (dots + 1) % 4;
    repaint();
}

juce::AttributedString PluginCanvas::AiBanner::makeText() const
{
    juce::AttributedString attributed;
    attributed.setWordWrap (juce::AttributedString::byWord);
    attributed.setJustification (juce::Justification::topLeft);
    attributed.setLineSpacing (2.0f);

    if (working)
    {
        attributed.append ("Fanan AI is working on " + text + juce::String::repeatedString (".", dots), Theme::font (14.0f), Theme::text);
        return attributed;
    }

    // Short markdown, as in the chat: **bold**, and ==what the user should answer or do== in yellow.
    attributed.append ("Fanan AI  ", Theme::font (14.0f, true), Theme::accent.brighter (0.2f));

    bool bold = false, ask = false;
    juce::String run;

    auto flush = [&]
    {
        if (run.isNotEmpty())
            attributed.append (run, Theme::font (14.0f, bold), ask ? askYellow : Theme::text);

        run.clear();
    };

    for (int i = 0; i < text.length();)
    {
        const auto pair = text.substring (i, i + 2);

        if (pair == "**" || pair == "==")
        {
            flush();
            (pair == "**" ? bold : ask) = ! (pair == "**" ? bold : ask);
            i += 2;
            continue;
        }

        run += text.substring (i, i + 1);
        ++i;
    }

    flush();
    return attributed;
}

int PluginCanvas::AiBanner::getIdealHeight (int width) const
{
    juce::TextLayout textLayout;
    textLayout.createLayout (makeText(), (float) juce::jmax (100, width - 28 - 130));
    return juce::jlimit (46, 180, juce::roundToInt (textLayout.getHeight()) + 26);
}

void PluginCanvas::AiBanner::paint (juce::Graphics& g)
{
    const auto box = getLocalBounds().toFloat().reduced (0.5f);

    juce::DropShadow (juce::Colours::black.withAlpha (0.45f), 14, { 0, 3 }).drawForRectangle (g, getLocalBounds());
    g.setColour (Theme::raised);
    g.fillRoundedRectangle (box, 9.0f);
    g.setColour (working ? Theme::hot.withAlpha (0.7f) : Theme::accent.withAlpha (0.55f));
    g.drawRoundedRectangle (box, 9.0f, 1.2f);

    juce::TextLayout textLayout;
    const auto area = getLocalBounds().toFloat().reduced (14.0f, 13.0f).withTrimmedRight (124.0f);
    textLayout.createLayout (makeText(), area.getWidth());
    textLayout.draw (g, area);
}

void PluginCanvas::AiBanner::resized()
{
    auto top = getLocalBounds().reduced (10, 9).removeFromTop (28);
    closeButton.setBounds (top.removeFromRight (28));
    top.removeFromRight (6);
    chatButton.setBounds (top.removeFromRight (88));
}

//==============================================================================
void PluginCanvas::showAiWorking (const juce::String& what)
{
    banner->showWorking (what);
    banner->toFront (false);
}

void PluginCanvas::showAiReply (const juce::String& reply)
{
    banner->showReply (reply);
    banner->toFront (false);
}

std::vector<int> PluginCanvas::selection() const
{
    std::vector<int> result (manySelected.begin(), manySelected.end());

    if (result.empty() && juce::isPositiveAndBelow (selected, (int) layout.widgets.size()))
        result.push_back (selected);

    return result;
}

bool PluginCanvas::isSelected (int index) const
{
    return manySelected.empty() ? index == selected : manySelected.count (index) > 0;
}

void PluginCanvas::toggleSelection (int index)
{
    const auto current = selection();
    std::set<int> indexes (current.begin(), current.end());

    if (indexes.count (index) > 0)
        indexes.erase (index);
    else
        indexes.insert (index);

    selectMany (indexes, index);
}

void PluginCanvas::selectMany (const std::set<int>& indexes, int primary)
{
    std::set<int> valid;

    for (const auto i : indexes)
        if (juce::isPositiveAndBelow (i, (int) layout.widgets.size()))
            valid.insert (i);

    if (valid.size() <= 1 || ! design)
    {
        select (valid.empty() ? -1 : *valid.begin());
        return;
    }

    // Several: an open menu turns into theirs; `selected` is the last one picked.
    manySelected = valid;
    selected = valid.count (primary) > 0 ? primary : *valid.rbegin();
    panelSelected = false;

    if (menuOpen)
        openMenu();

    repaint();
}

void PluginCanvas::select (int index)
{
    selected = design && juce::isPositiveAndBelow (index, (int) layout.widgets.size()) ? index : -1;
    panelSelected = false;
    manySelected.clear();

    if (selected < 0)
        closeMenu();
    else if (menuOpen)
        openMenu();   // the menu follows the selection

    repaint();
}

void PluginCanvas::selectPanel()
{
    selected = -1;
    manySelected.clear();
    panelSelected = design && hasPanel();

    if (! panelSelected)
        closeMenu();
    else if (menuOpen)
        openMenu();

    repaint();
}

void PluginCanvas::openMenu()
{
    if (! design || (selected < 0 && ! panelSelected && manySelected.empty()))
    {
        closeMenu();
        return;
    }

    if (manySelected.size() > 1)
        menu->showForSeveral (selection());
    else
        menu->showFor (panelSelected ? -1 : selected);
    menuOpen = true;
    menu->setVisible (true);
    layOutOverlays();
    menu->toFront (false);
}

void PluginCanvas::closeMenu()
{
    menuOpen = false;

    if (menu != nullptr)
        menu->setVisible (false);
}

void PluginCanvas::layOutOverlays()
{
    const auto view = viewArea().toNearestInt();

    // The crop tool and the zoom, bottom left, just above the scrollbar.
    auto bar = juce::Rectangle<int> (view.getX() + 10, view.getBottom() - 36, 296, 26);
    cropButton.setBounds (bar.removeFromLeft (52));
    bar.removeFromLeft (8);
    fitButton.setBounds (bar.removeFromLeft (46));
    bar.removeFromLeft (8);
    zoomSlider.setBounds (bar.removeFromLeft (124));
    bar.removeFromLeft (6);
    zoomLabel.setBounds (bar);

    // The crop tool's Apply and Cancel, bottom middle.
    {
        const auto left = juce::jmax (view.getCentreX() - 95, bar.getRight() + 24);   // clear of the zoom
        const auto buttons = juce::Rectangle<int> (left, view.getBottom() - 38, 190, 28);
        cropApplyButton.setBounds (buttons.withWidth (90));
        cropCancelButton.setBounds (buttons.withTrimmedLeft (100));
    }

    // Sonify AI's banner, across the top.
    if (banner->isVisible())
    {
        const auto width = juce::jmin (620, view.getWidth() - 40);
        banner->setBounds (view.getCentreX() - width / 2, view.getY() + 10, width, banner->getIdealHeight (width));
    }

    // The edit menu, beside what it edits: on its right if there's room, otherwise on its left,
    // otherwise along the canvas's right edge.
    if (menuOpen && menu->isVisible())
    {
        auto target = panelSelected || ! juce::isPositiveAndBelow (selected, (int) layout.widgets.size())
                          ? panelArea()
                          : toView (layout.widgets[(size_t) selected].bounds);

        for (const auto i : manySelected)
            if (juce::isPositiveAndBelow (i, (int) layout.widgets.size()))
                target = target.getUnion (toView (layout.widgets[(size_t) i].bounds));
        const auto width = ElementMenu::menuWidth;
        const auto height = juce::jmin (menu->getIdealHeight(), view.getHeight() - 20);

        auto x = (int) target.getRight() + 14;

        if (x + width > view.getRight() - 10)
            x = (int) target.getX() - 14 - width;

        if (x < view.getX() + 10)
            x = view.getRight() - 10 - width;

        const auto y = juce::jlimit (view.getY() + 10, juce::jmax (view.getY() + 10, view.getBottom() - 10 - height), (int) target.getY());
        menu->setBounds (x, y, width, height);
    }
}
