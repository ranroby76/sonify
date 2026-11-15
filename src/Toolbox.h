// D:\workspace\Fanan_web_hub\src\Toolbox.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

//==============================================================================
/**
    The Edit UI tab's toolbox, FlowStone-style: every primitive by category, with a search
    box on top and a button for each category under it (several can be on at once: only the
    categories lit show their items; a right-click shows that one alone, again all of them;
    a search looks through every category). Drag one onto the plugin window to add it there,
    or double-click it to add it in the middle of what's showing.

    It must sit inside a DragAndDropContainer that also holds the canvas.
*/
class Toolbox final : public juce::Component
{
public:
    Toolbox();
    ~Toolbox() override;

    /** A double-click: add this primitive in the middle of what's showing. */
    std::function<void (const juce::String& primitiveId)> onAdd;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** A primitive's icon, by its "icon" name (unknown names get a plain square). */
    static void drawIcon (juce::Graphics&, juce::Rectangle<float> area, const juce::String& icon);

    static constexpr int width = 214;

private:
    class Item;
    class List;

    juce::TextEditor search;
    juce::Viewport viewport;
    std::unique_ptr<List> list;

    // The categories' buttons: lit, their items show (the ones off remembered).
    juce::OwnedArray<juce::TextButton> categoryButtons;
    juce::StringArray hiddenCategories;
    int categoriesHeight = 0;

    void categoryClicked (juce::TextButton& button, bool alone);
    void saveHidden() const;
    int layOutCategories (int width, bool place);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Toolbox)
};
