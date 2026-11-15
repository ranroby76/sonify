// D:\workspace\Fanan_web_hub\src\kit\kit_Settings.cpp

#include "Settings.h"
#include "Preferences.h"

juce::File Settings::getDataFolder()     { return SettingsFolder::get().getChildFile ("data"); }
juce::File Settings::getCacheFolder()    { return SettingsFolder::get(); }
