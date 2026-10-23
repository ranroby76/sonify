// D:\workspace\Fanan_web_hub\src\NodeRackPages.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Container.h"
#include "NodeTools.h"

#include <functional>
#include <memory>
#include <vector>

//==============================================================================
/**
    The Node Rack window's pages besides the Rack (ContainerCanvas.h): the Mixer and the
    Manual; and the editors the Latcher, Step Seq and Transient Splitter open with their E
    (Colosseum's, in a callout by the node).

    An editor is told how to know its tool is still there (a rack can lose a node, or be loaded
    over, while the editor is open): when it isn't, the editor closes itself and touches nothing.
*/
namespace NodeRackPages
{
    struct ToolLink
    {
        std::function<bool()> isAlive;      // the tool is still in its rack
        std::function<void()> edited;       // a setting changed: the song has unsaved changes
    };

    std::unique_ptr<juce::Component> makeLatcherEditor (NodeTools::Latcher& latcher, ToolLink link);
    std::unique_ptr<juce::Component> makeStepSeqEditor (NodeTools::StepSeq& stepSeq, ToolLink link);
    std::unique_ptr<juce::Component> makeTransientSplitterEditor (NodeTools::TransientSplitter& splitter, ToolLink link);
}

//==============================================================================
/** The Mixer page (Colosseum's): a fader and a meter for the rack's output, for each
    instrument in it (a Container too) and for each effect. It shows the rack the Rack page
    shows (the Container entered, when one is). */
class NodeRackMixer final : public juce::Component, private juce::Timer
{
public:
    explicit NodeRackMixer (std::function<ContainerPlugin&()> shown);
    ~NodeRackMixer() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    class Strip;

private:
    void timerCallback() override;
    void rebuild();

    std::function<ContainerPlugin&()> shown;
    ContainerPlugin* builtFor = nullptr;
    juce::uint32 builtAt = 0;

    juce::Viewport viewport;
    juce::Component content;
    juce::OwnedArray<Strip> strips;
    int outputCount = 0, instrumentCount = 0, effectCount = 0;

    static constexpr int stripWidth = 84, stripGap = 6, sectionGap = 22, labelHeight = 22;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NodeRackMixer)
};

//==============================================================================
/** The Rack page's Macros strip: the rack's eight macro knobs (ContainerPlugin's macros). Each
    has its name (double-click to rename), its knob, Learn (then move a control in the window of
    a plugin in this rack: the macro drives it) and its menu (assign a control from a list of
    every parameter in the rack, the Mixer's levels and the rack's Output; each control's range
    and invert; take a control away; rename). */
class NodeRackMacros final : public juce::Component, private juce::Timer
{
public:
    explicit NodeRackMacros (ContainerPlugin& rack);
    ~NodeRackMacros() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    static constexpr int height = 112;

    class Cell;

private:
    void timerCallback() override;
    void startLearning (int macro);
    void showMenu (int macro, juce::Component& anchor);
    void editRange (const ContainerPlugin::MacroLink& link, juce::Component& anchor);

    ContainerPlugin& rack;
    juce::OwnedArray<Cell> cells;
    int learning = -1;                 // the macro waiting for a control to be moved, or -1
    juce::uint32 learningSince = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NodeRackMacros)
};

//==============================================================================
/** The Manual page: what the Node Rack is and how it's worked. */
class NodeRackManual final : public juce::Component
{
public:
    NodeRackManual();
    ~NodeRackManual() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    class Text;

private:
    juce::Viewport viewport;
    std::unique_ptr<Text> text;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NodeRackManual)
};
