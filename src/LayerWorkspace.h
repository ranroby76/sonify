// D:\workspace\Fanan_web_hub\src\LayerWorkspace.h
// From KnobMaker (C:\workspace\knobmaker); in Stella it edits the look the Knob Studio hands it
// and says when it changed (onDocumentChanged), a .knob import becomes a new look
// (onImported) instead of replacing this one, the examples box is the studio's New look menu,
// and the tabs take the studio's own colours.

/*
    LayerWorkspace.h

    Composes the layer editing surface: canvas preview with a frame scrubber on
    the left, layer stack and the selected layer's primitive/effect panels on
    the right.
*/

#pragma once

#include "KnobFile.h"
#include "LayerPanels.h"
#include "LayerRender.h"

//==============================================================================
class CanvasPreview final : public juce::Component,
                            private juce::Timer
{
public:
    CanvasPreview();

    void setDocument (LayerDoc*);
    void invalidate();

    void setFrame (float t);
    float getFrame() const noexcept { return time; }

    void cycleBackground();

    /** 1:1 — one document pixel to one screen pixel, no fitting. */
    void setActualSize (bool);
    bool isActualSize() const noexcept { return actualSize; }

    void paint (juce::Graphics&) override;
    void resized() override;

    void setPlaying (bool);
    bool isPlaying() const noexcept { return playing; }

    /** Drag or wheel on the rendered widget to move through its frames, the way
        you would turn the real control. Fired so the transport slider and the
        frame readout can follow. */
    std::function<void (float)> onFrameDragged;

    /** Anything the renderer wants the user to know — a shadow that has run out
        of canvas, and so on. Fired only when the message changes, so it does
        not spam during playback. Empty string means all clear. */
    std::function<void (const juce::String&)> onRenderNote;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;
    void nudgeFrame (float delta);
    void rerender (float scale);

    LayerDoc*   document = nullptr;
    juce::Image frame;
    float       time = 0.0f;
    float       renderedScale = 0.0f;   ///< the scale `frame` was rasterised at
    float       renderedPad   = 0.0f;   ///< Stella: the room `frame` has around the canvas, in its own pixels
    int         backgroundMode = 0;
    bool        actualSize = false;
    bool        playing = false;
    bool        dirty = true;

    // A look from KnobMan takes longer to draw at a big size: while it's being changed (a
    // slider dragged, the frame scrubbed) or played, it's drawn smaller, and sharp again once
    // the changes stop.
    juce::uint32 quickUntil = 0;
    int refineCount = 0;
    bool quickNow() const;

    float dragStartTime = 0.0f;
    juce::String lastRenderNote;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CanvasPreview)
};

//==============================================================================
class LayerWorkspace final : public juce::Component
{
public:
    LayerWorkspace();
    ~LayerWorkspace() override;

    LayerDoc& getDocument() noexcept { return doc; }
    void setDocument (const LayerDoc&);

    void resized() override;
    void paint (juce::Graphics&) override;

    void cycleBackground() { canvas.cycleBackground(); }

    /** So the Light Studio can put its key light where the knobs think it is.
        A panel lit from one side while the bevels say the other is the fault
        nobody can name when they look at it. */
    float getGlobalLightAngle() const noexcept { return doc.globalLightAngle; }

    void setActualSize (bool b)        { canvas.setActualSize (b); actualSizeButton.setToggleState (b, juce::dontSendNotification); }
    bool isActualSize() const noexcept { return canvas.isActualSize(); }

    void importKnobFile();

    std::function<void (const juce::String&)> onStatus;

    /** Stella: the user changed the document (not setDocument). */
    std::function<void()> onDocumentChanged;

    /** Stella: a knob read from a .knob file, for the studio to add as a look of its own. */
    std::function<void (const LayerDoc&, const juce::String& name)> onImported;

private:
    void applyImport (const LayerDoc&, const juce::String& name, const juce::StringArray& warnings);

    void selectionChanged();
    void documentChanged();
    void updateCanvasSizeFields();
    void pullCanvasFields();

    /** Re-sizes both editor panels to whatever height they currently need.
        Called from resized(), and again whenever a panel grows or shrinks on
        its own — enabling a mask, switching primitive type — so the Viewport's
        scroll range always matches the content. */
    void layoutEditorPanels();

    LayerDoc doc { LayerDoc::knobExample() };
    bool settingDocument = false;

    CanvasPreview   canvas;
    LayerListPanel  layerList;
    PrimitivePanel  primPanel;
    EffectPanel     effectPanel;
    juce::Viewport  primViewport;
    juce::Viewport  effectViewport;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    juce::Slider     frameSlider;
    juce::TextButton playButton { "Play" };
    juce::Label      frameLabel;

    std::unique_ptr<juce::FileChooser> knobChooser;
    juce::Slider   canvasWSlider, canvasHSlider, framesSlider;
    juce::Label    canvasWLabel { {}, "W" }, canvasHLabel { {}, "H" }, framesLabel { {}, "Frames" };
    juce::ToggleButton linkSizeButton { "Link" };

    // A look from KnobMan: its spheres and metal circles lit by Sonify, or as KnobMan lights them.
    juce::ToggleButton sonifyLightButton { "Sonify lighting" };

    // The knob at its real size, one pixel for one, instead of fitted to the view.
    juce::ToggleButton actualSizeButton { "Show at 1:1 size" };
    bool keepingKnobMan = false;   // a change that keeps KnobMan's drawing (the lighting switch)

    // The visible area inside the editor Viewports, cached by resized() so a
    // panel that changes height later can be re-laid-out without a full pass.
    int editorViewW = 100;
    int editorViewH = 100;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LayerWorkspace)
};
