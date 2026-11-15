// D:\workspace\Fanan_web_hub\src\Workspace3D.cpp

#include "Workspace3D.h"
#include "ColourSwatch.h"
#include "StellaLookAndFeel.h"
#include "UiHelpers.h"

namespace
{
    constexpr int rowHeight = 26, headingHeight = 30, captionWidth = 150, panelWidth = 400;

    juce::String hexOf (juce::Colour c)    { return "#" + c.toDisplayString (true); }

    std::shared_ptr<const Model3D> defaultModel()
    {
        static const auto model = std::make_shared<const Model3D>();
        return model;
    }
}

//==============================================================================
/** The settings' rows, laid out in the viewport. */
class Workspace3D::Panel final : public juce::Component
{
public:
    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::panel);

        for (const auto& line : headings)
        {
            g.setColour (Theme::accent);
            g.setFont (Theme::font (12.5f, true));
            g.drawText (line.second.toUpperCase(), 12, line.first + 8, getWidth() - 24, 16, juce::Justification::centredLeft, true);
            g.setColour (Theme::outline);
            g.fillRect (12, line.first + headingHeight - 4, getWidth() - 24, 1);
        }
    }

    std::vector<std::pair<int, juce::String>> headings;   // where each group's title is
};

//==============================================================================
Workspace3D::Workspace3D()
{
    panel = std::make_unique<Panel>();
    viewport.setViewedComponent (panel.get(), false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    doc = Model3D::newDocument (Model3D::preset (Model3D::Kind::knob, 0));
    canvas.setDocument (&doc);
    addAndMakeVisible (canvas);

    canvas.onFrameDragged = [this] (float t) { frameSlider.setValue (t, juce::dontSendNotification); frameLabel.setText (juce::String (juce::roundToInt (t * (float) juce::jmax (1, doc.frames - 1)) + 1) + " / " + juce::String (doc.frames), juce::dontSendNotification); };

    frameSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    frameSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    frameSlider.setRange (0.0, 1.0, 0.0);
    frameSlider.onValueChange = [this]
    {
        canvas.setFrame ((float) frameSlider.getValue());
        frameLabel.setText (juce::String (juce::roundToInt ((float) frameSlider.getValue() * (float) juce::jmax (1, doc.frames - 1)) + 1)
                              + " / " + juce::String (doc.frames), juce::dontSendNotification);
    };
    addAndMakeVisible (frameSlider);

    playButton.setClickingTogglesState (true);
    playButton.onClick = [this] { canvas.setPlaying (playButton.getToggleState()); };
    addAndMakeVisible (playButton);

    for (auto* label : { &frameLabel, &kindLabel })
    {
        label->setColour (juce::Label::textColourId, Theme::muted);
        label->setFont (Theme::font (12.5f));
        addAndMakeVisible (label);
    }

    kindLabel.setFont (Theme::font (14.0f, true));
    kindLabel.setColour (juce::Label::textColourId, Theme::text);
    frameLabel.setJustificationType (juce::Justification::centred);

    presetButton.setTooltip ("A ready-made one of its kind in place of this one (its canvas and frames stay)");
    presetButton.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (presetButton);

    auto spin = [this] (juce::Slider& s, double low, double high)
    {
        s.setSliderStyle (juce::Slider::IncDecButtons);
        s.setRange (low, high, 1.0);
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 48, 20);
        s.onValueChange = [this] { pullCanvasFields(); };
        addAndMakeVisible (s);
    };

    spin (canvasW, 8, 2048);
    spin (canvasH, 8, 2048);
    spin (frames, 1, 512);

    for (auto* l : { &canvasWLabel, &canvasHLabel, &framesLabel })
    {
        l->setColour (juce::Label::textColourId, Theme::muted);
        l->setFont (Theme::font (12.0f));
        l->setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (l);
    }

    actualSize.setTooltip ("Show it at its real size, one pixel for one, instead of fitted to the view");
    actualSize.onClick = [this] { canvas.setActualSize (actualSize.getToggleState()); };
    addAndMakeVisible (actualSize);

    setDocument (doc);
}

Workspace3D::~Workspace3D()
{
    canvas.setPlaying (false);
}

const Model3D& Workspace3D::model() const
{
    return doc.model3d != nullptr ? *doc.model3d : *defaultModel();
}

void Workspace3D::setDocument (const LayerDoc& newDoc)
{
    doc = newDoc;

    if (doc.model3d == nullptr)
        doc.model3d = defaultModel();

    canvas.setDocument (&doc);

    const juce::ScopedValueSetter<bool> quiet (updating, true);
    canvasW.setValue (doc.canvasWidth, juce::dontSendNotification);
    canvasH.setValue (doc.canvasHeight, juce::dontSendNotification);
    frames.setValue (doc.frames, juce::dontSendNotification);

    if (! rowsBuilt || builtFor != model().kind)
        buildRows();

    showValues();
    frameSlider.onValueChange();
    resized();
}

//==============================================================================
void Workspace3D::buildRows()
{
    rows.clear();
    builtFor = model().kind;
    rowsBuilt = true;

    juce::String group;

    for (const auto& p : Model3D::paramsFor (builtFor))
    {
        if (p.group != group)
        {
            group = p.group;
            Row heading;
            heading.group = group;
            rows.push_back (std::move (heading));
        }

        Row row;
        row.param = &p;
        row.group = group;
        row.caption = std::make_unique<juce::Label> (juce::String(), p.label);
        row.caption->setFont (Theme::font (13.0f));
        row.caption->setColour (juce::Label::textColourId, Theme::text.withAlpha (0.85f));
        row.caption->setTooltip (p.tip);
        const auto key = p.key;

        switch (p.type)
        {
            case Model3D::Param::Type::number:
            {
                auto slider = std::make_unique<FineSlider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
                slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 20);
                slider->setRange (p.min, p.max, p.step);
                slider->setTextValueSuffix (p.suffix);
                slider->setDoubleClickReturnValue (true, p.def);
                slider->setTooltip (p.tip);
                slider->setColour (juce::Slider::backgroundColourId, Theme::outline.brighter (0.45f));
                slider->setColour (juce::Slider::trackColourId, Theme::accent);
                slider->setColour (juce::Slider::thumbColourId, Theme::text);
                auto* s = slider.get();
                slider->onValueChange = [this, key, s] { change (key, s->getValue()); };
                row.control = std::move (slider);
                break;
            }

            case Model3D::Param::Type::choice:
            {
                auto box = std::make_unique<juce::ComboBox>();
                box->addItemList (p.choices, 1);
                box->setTooltip (p.tip);
                auto* b = box.get();
                box->onChange = [this, key, b] { change (key, b->getSelectedId() - 1); };
                row.control = std::move (box);
                break;
            }

            case Model3D::Param::Type::colour:
            {
                auto swatch = std::make_unique<ColourSwatchButton> (p.label);
                swatch->setTooltip (p.tip.isNotEmpty() ? p.tip : p.label);
                auto* sw = swatch.get();
                swatch->onColourChange = [this, key, sw] { change (key, hexOf (sw->getColourValue())); };
                row.control = std::move (swatch);
                break;
            }

            case Model3D::Param::Type::flag:
            {
                auto tick = std::make_unique<juce::ToggleButton> (p.label);
                tick->setTooltip (p.tip);
                auto* t = tick.get();
                tick->onClick = [this, key, t] { change (key, t->getToggleState()); };
                row.caption->setText ({}, juce::dontSendNotification);
                row.control = std::move (tick);
                break;
            }

            case Model3D::Param::Type::text:
            {
                auto editor = std::make_unique<juce::TextEditor>();
                editor->setFont (Theme::font (14.0f));
                editor->setIndents (6, 4);
                editor->setTooltip (p.tip);
                auto* ed = editor.get();
                editor->onTextChange = [this, key, ed] { change (key, ed->getText()); };
                row.control = std::move (editor);
                break;
            }
        }

        panel->addAndMakeVisible (*row.caption);
        panel->addAndMakeVisible (*row.control);
        rows.push_back (std::move (row));
    }

    configurePanelControls (*panel);
}

void Workspace3D::showValues()
{
    const juce::ScopedValueSetter<bool> quiet (updating, true);
    const auto& m = model();

    kindLabel.setText ("3D " + Model3D::kindNames()[(int) m.kind].toLowerCase(), juce::dontSendNotification);

    for (auto& row : rows)
    {
        if (row.param == nullptr)
            continue;

        const auto& p = *row.param;

        switch (p.type)
        {
            case Model3D::Param::Type::number:
                static_cast<juce::Slider*> (row.control.get())->setValue (m.number (p.key), juce::dontSendNotification);
                break;
            case Model3D::Param::Type::choice:
                static_cast<juce::ComboBox*> (row.control.get())->setSelectedId (m.choice (p.key) + 1, juce::dontSendNotification);
                break;
            case Model3D::Param::Type::colour:
                static_cast<ColourSwatchButton*> (row.control.get())->setColourValue (m.colour (p.key), juce::dontSendNotification);
                break;
            case Model3D::Param::Type::flag:
                static_cast<juce::ToggleButton*> (row.control.get())->setToggleState (m.flag (p.key), juce::dontSendNotification);
                break;
            case Model3D::Param::Type::text:
                if (auto* ed = static_cast<juce::TextEditor*> (row.control.get()); ed->getText() != m.text (p.key))
                    ed->setText (m.text (p.key), juce::dontSendNotification);
                break;
        }
    }

    layOutPanel();
}

void Workspace3D::layOutPanel()
{
    const auto& m = model();
    const int width = juce::jmax (220, viewport.getMaximumVisibleWidth());
    int y = 4;
    panel->headings.clear();

    // A group shows when a row of it does.
    for (size_t i = 0; i < rows.size(); ++i)
    {
        auto& row = rows[i];

        if (row.param == nullptr)
        {
            bool any = false;

            for (size_t k = i + 1; k < rows.size() && rows[k].param != nullptr; ++k)
                any = any || m.isShown (*rows[k].param);

            if (any)
            {
                panel->headings.push_back ({ y, row.group });
                y += headingHeight;
            }

            continue;
        }

        const bool shown = m.isShown (*row.param);
        row.caption->setVisible (shown);
        row.control->setVisible (shown);

        if (! shown)
            continue;

        const bool tick = row.param->type == Model3D::Param::Type::flag;
        const int height = row.param->type == Model3D::Param::Type::colour ? 22 : 24;

        if (tick)
        {
            row.control->setBounds (12, y, width - 24, height);
        }
        else
        {
            row.caption->setBounds (12, y, captionWidth, height);
            row.control->setBounds (12 + captionWidth, y + (rowHeight - height) / 2 - 1, width - 24 - captionWidth, height);
        }

        y += rowHeight + 2;
    }

    panel->setSize (width, y + 12);
    panel->repaint();
}

void Workspace3D::change (const juce::String& key, const juce::var& value)
{
    if (updating)
        return;

    auto edited = std::make_shared<Model3D> (model());
    edited->set (key, value);
    doc.model3d = edited;

    // A choice or a tick can bring rows or take them away.
    if (const auto* p = Model3D::findParam (edited->kind, key); p != nullptr && p->type != Model3D::Param::Type::number
                                                                 && p->type != Model3D::Param::Type::colour && p->type != Model3D::Param::Type::text)
        layOutPanel();

    changed();
}

void Workspace3D::changed()
{
    canvas.invalidate();

    if (onDocumentChanged != nullptr)
        onDocumentChanged();
}

void Workspace3D::pullCanvasFields()
{
    if (updating)
        return;

    doc.canvasWidth = juce::roundToInt (canvasW.getValue());
    doc.canvasHeight = juce::roundToInt (canvasH.getValue());
    doc.frames = juce::roundToInt (frames.getValue());
    frameSlider.onValueChange();
    changed();
}

void Workspace3D::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto kind = model().kind;
    const auto names = Model3D::presetNames (kind);

    for (int i = 0; i < names.size(); ++i)
        menu.addItem (i + 1, names[i]);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton),
                        [safeThis = juce::Component::SafePointer<Workspace3D> (this), kind] (int result)
                        {
                            if (safeThis == nullptr || result <= 0)
                                return;

                            safeThis->doc.model3d = std::make_shared<const Model3D> (Model3D::preset (kind, result - 1));
                            safeThis->showValues();
                            safeThis->changed();
                        });
}

//==============================================================================
void Workspace3D::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::outline);
    g.fillRect (getWidth() - panelWidth - 1, 0, 1, getHeight());
}

void Workspace3D::resized()
{
    auto area = getLocalBounds();
    auto right = area.removeFromRight (panelWidth);
    area.removeFromRight (1);

    // The settings: what it is and where it starts from, then its groups.
    auto head = right.removeFromTop (44).reduced (12, 8);
    presetButton.setBounds (head.removeFromRight (130));
    kindLabel.setBounds (head);
    viewport.setBounds (right);
    layOutPanel();

    // The canvas's size and frames, the preview, its frames.
    auto bar = area.removeFromTop (36).reduced (10, 6);
    canvasWLabel.setBounds (bar.removeFromLeft (20));
    canvasW.setBounds (bar.removeFromLeft (104));
    bar.removeFromLeft (6);
    canvasHLabel.setBounds (bar.removeFromLeft (20));
    canvasH.setBounds (bar.removeFromLeft (104));
    bar.removeFromLeft (6);
    framesLabel.setBounds (bar.removeFromLeft (52));
    frames.setBounds (bar.removeFromLeft (104));
    bar.removeFromLeft (12);
    actualSize.setBounds (bar.removeFromLeft (150));

    auto transport = area.removeFromBottom (40).reduced (10, 8);
    playButton.setBounds (transport.removeFromLeft (60));
    transport.removeFromLeft (10);
    frameLabel.setBounds (transport.removeFromRight (70));
    frameSlider.setBounds (transport);

    canvas.setBounds (area.reduced (10));
}
