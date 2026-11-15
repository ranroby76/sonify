// D:\workspace\Fanan_web_hub\src\Workspace3D.h

#pragma once

#include "UiHelpers.h"   // FineSlider

#include "LayerWorkspace.h"   // CanvasPreview
#include "Model3D.h"

#include <memory>
#include <vector>

class ColourSwatchButton;

//==============================================================================
/**
    The 3D Studio's builder: a 3D look (a knob, a button, a slider or a toggle switch built as
    an object) with its preview on the left (drag it or use the wheel to turn it through its
    frames; Play; the canvas's size and its frames above) and its settings on the right, in
    groups (shape, skirt, grip, top cap, pointer, materials, light ring, movement, view...),
    every one changing the preview at once. "Start from" puts a ready-made one in its place.
*/
class Workspace3D final : public juce::Component
{
public:
    Workspace3D();
    ~Workspace3D() override;

    LayerDoc& getDocument() noexcept    { return doc; }
    void setDocument (const LayerDoc&);

    /** The user changed the look (not setDocument). */
    std::function<void()> onDocumentChanged;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    struct Row
    {
        const Model3D::Param* param = nullptr;   // null: a group's heading
        juce::String group;
        std::unique_ptr<juce::Label> caption;
        std::unique_ptr<juce::Component> control;
    };

    class Panel;

    const Model3D& model() const;
    void buildRows();                 // the rows for its kind
    void showValues();                // what the model holds, and which rows apply
    void layOutPanel();
    void change (const juce::String& key, const juce::var& value);
    void changed();                   // the look changed: shown, and saved
    void pullCanvasFields();
    void showPresetMenu();

    LayerDoc doc;
    Model3D::Kind builtFor = Model3D::Kind::knob;
    bool rowsBuilt = false, updating = false;

    CanvasPreview canvas;
    juce::Slider frameSlider;
    juce::TextButton playButton { "Play" }, presetButton { juce::String::fromUTF8 ("Start from\xe2\x80\xa6") };
    juce::Label frameLabel, kindLabel;
    FineSlider canvasW, canvasH, frames;
    juce::Label canvasWLabel { {}, "W" }, canvasHLabel { {}, "H" }, framesLabel { {}, "Frames" };
    juce::ToggleButton actualSize { "Show at 1:1 size" };

    std::unique_ptr<Panel> panel;
    juce::Viewport viewport;
    std::vector<Row> rows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Workspace3D)
};
