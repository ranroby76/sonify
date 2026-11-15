// D:\workspace\Fanan_web_hub\src\IconBrowser.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

//==============================================================================
/**
    The icon library (IconLibrary) to pick from: a search field, its categories, the icons a
    grid at a time, the one under the mouse named. A click picks one (onPick, with its name);
    it stays open for more until a click outside it. dragOut: icons are dragged out instead
    (onto the window: the Gallery window's), as { "galleryIcon": name }.

    badges: the Guitar FX icons on their badges (the shape chosen at its top, each family its
    colour), dragged out with them: { "galleryIcon", "badge", "badgeColour", "iconColour" }.

    show (near, onPick) opens it in a call-out beside a component.
*/
class IconBrowser final : public juce::Component,
                          private juce::Timer
{
public:
    explicit IconBrowser (std::function<void (const juce::String& name)> onPick, bool dragOut = false, bool badges = false);

    /** A Guitar FX icon's family colours: its badge's, and its own on it. */
    static void familyColours (const juce::String& name, juce::Colour& badge, juce::Colour& fore);

    static constexpr const char* fxCategory = "Guitar FX";
    ~IconBrowser() override;

    static void show (juce::Component& near, std::function<void (const juce::String& name)> onPick);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Grid;

    std::function<void (const juce::String&)> onPick;
    bool dragOut = false, badges = false;
    juce::ComboBox shapeBox;   // badges: their shape
    juce::TextEditor searchBox;
    juce::ComboBox categoryBox;
    juce::Label nameLabel;
    juce::Viewport view;
    std::unique_ptr<Grid> grid;

    void refill();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IconBrowser)
};
