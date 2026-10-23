// D:\workspace\Fanan_web_hub\src\Settings.h

#pragma once

#include <juce_core/juce_core.h>

//==============================================================================
/**
    The Plugins Creator Studio's own settings (Stella AI Studio's, inside Sonify): recent
    projects, the projects folder, the company name new plugins get, whether a sound says
    Sonify AI has finished, and what Sonify AI's plan steps really cost (for its estimates).

    Kept in one JSON file next to the executable, as Sonify keeps its own, so a development
    build writes nothing outside its build folder.

    Its folders: the user's things in <user data>\Plugins Creator (Settings > Locations in
    Sonify; Documents\Sonify by default): Projects, Looks, Primitives, Shapes, Textures; and
    what can be fetched again in Sonify's own application data (Plugins Creator).
*/
class Settings final
{
public:
    Settings();

    void save() const;

    juce::File getProjectsFolder() const;
    void setProjectsFolder (const juce::File& folder);

    juce::String getVendor() const;
    void setVendor (const juce::String& vendor);

    /** Project files, most recent first. Files that no longer exist are left out. */
    juce::StringArray getRecentProjects() const;
    void addRecentProject (const juce::File& projectFile);
    void removeRecentProject (const juce::File& projectFile);

    juce::String getWindowState() const;
    void setWindowState (const juce::String& state);

    /** A short sound when Sonify AI finishes a request (on unless turned off). */
    bool getFinishSound() const;
    void setFinishSound (bool shouldPlay);

    /** The studio's MIDI In: the ports it hears, by name ("*", the first time: all of them). */
    juce::StringArray getMidiInputs() const;
    void setMidiInputs (const juce::StringArray& ports);

    /** What a plan step of a size ("small", "medium", "large") really cost on average, in
        credits, over the last steps of that size (0 until a few were measured), and a
        finished step's cost. */
    double getStepCost (const juce::String& size) const;
    void addStepCost (const juce::String& size, int credits);
    static constexpr int stepCostMinimum = 3, stepCostWindow = 20;

    static juce::File getFile();
    static juce::File getDefaultProjectsFolder();

    /** <user data>\Plugins Creator: the user's projects, looks, primitives, shapes and textures. */
    static juce::File getDataFolder();

    /** Sonify's application data, Plugins Creator: downloads and caches that can be fetched again. */
    static juce::File getCacheFolder();

    /** Sonify's "Block internet": while on, nothing here reaches the network (the gallery, say). */
    static void setInternetBlocked (bool blocked) noexcept;
    static bool isInternetBlocked() noexcept;

    static constexpr int maxRecentProjects = 12;

private:
    juce::String getString (const juce::Identifier& key) const;
    void setString (const juce::Identifier& key, const juce::String& value);

    juce::var data;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Settings)
};
