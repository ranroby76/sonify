// D:\workspace\Fanan_web_hub\src\kit\kit_Settings.h

#pragma once

#include <juce_core/juce_core.h>

//==============================================================================
/**
    The folders the window's code reads from (Looks, ShapeLibrary, TextureLibrary, Primitives),
    under the names Sonify's files ask for them by. In the plugin both are its private folder
    (Preferences.h), where what it carries is unpacked:

        getDataFolder    <private folder>\data: Looks (the looks it wears that were the maker's
                         own), Shapes and Textures (what those looks draw from), Primitives
        getCacheFolder   <private folder>
*/
class Settings final
{
public:
    static juce::File getDataFolder();
    static juce::File getCacheFolder();
};
