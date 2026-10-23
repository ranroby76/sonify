// D:\workspace\Fanan_web_hub\src\PresetLibrary.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PresetBank.h"

#include <functional>
#include <map>
#include <memory>
#include <vector>

//==============================================================================
/** What a preset menu shows: the plugin's preset manager on its panel, and the studio's Play row. */
struct PresetMenu
{
    juce::StringArray names, groups;   // each preset's name, and the heading it's listed under
    int current = -1;                  // the chosen one (-1: none)
    bool modified = false;             // its values changed since it was chosen
    bool canSave = false;              // Save writes over the chosen one (a preset of the user's)
    bool canDelete = false;            // the chosen one can be deleted
    bool canSaveFactory = false;       // the studio: presets that come with the plugin can be made

    /** What a preset manager shows: the chosen one's name (a * when its values changed), or what
        to do when none is chosen. Never "No presets" while there are some. */
    juce::String currentText() const;

    /** The next (+1) or previous (-1) preset, round the list; -1 when there are none. */
    int step (int delta) const;

    bool operator== (const PresetMenu&) const;
    bool operator!= (const PresetMenu& other) const    { return ! operator== (other); }
};

/** What a preset menu asks for besides choosing a preset. */
enum class PresetAction { save, saveAs, saveFactory, loadFile, saveBank, loadBank, remove, showFolder };

namespace PresetMenus
{
    /** The presets under their headings, ticked when chosen: item ids firstId + index. */
    void addPresets (juce::PopupMenu& menu, const PresetMenu& presets, int firstId);

    /** Save, Save as, Load, the banks, Delete and the folder: item ids firstId + (int) action. */
    void addActions (juce::PopupMenu& menu, const PresetMenu& presets, int firstId);
}

//==============================================================================
/**
    A plugin's presets, kept the way a modern plugin keeps them:

        Factory     the presets that come with the plugin (presets.json in its project, and in the
                    plugin added to Sonify): made in the Plugins Creator Studio by the user or by
                    Sonify AI
        My presets  the user's own, saved on the hard drive, one file each:
                    <user data>\Presets\<plugin's name>\<preset>.sonifypreset
        banks       a bank file (.sonifybank) holds a whole set of presets in one file: Save bank
                    writes every preset in the list into one, Load bank brings one's presets in
                    (into a folder of their own beside My presets, listed under the bank's name)

    One preset is chosen at a time (or none), and whether its values changed since it was chosen
    is kept, so a song can remember both. The plugin made and the plugin on a track share their
    presets by the plugin's name: a preset saved in one shows in the other.

    Message thread.
*/
class PresetLibrary final
{
public:
    struct Entry
    {
        Preset preset;
        juce::String group;     // "Factory", "My presets", or a bank's name
        juce::File file;        // a preset of the user's: its file (none: it comes with the plugin)

        bool isFactory() const noexcept    { return file == juce::File(); }
    };

    static constexpr const char* presetExtension = ".sonifypreset";
    static constexpr const char* bankExtension = ".sonifybank";
    static constexpr const char* factoryGroup = "Factory";
    static constexpr const char* userGroup = "My presets";

    /** <user data>\Presets: every plugin's own folder of presets. */
    static juce::File root();

    /** Whose presets: the plugin's name (its folder of presets is named after it) and the file
        with its factory presets (presets.json, which may not be there). factoryEditable: the
        studio, where factory presets are made and deleted. Reads them all; none is chosen.
        An empty name: no plugin, no presets. */
    void setPlugin (const juce::String& pluginName, const juce::File& factoryPresets, bool factoryEditable);

    /** Reads everything again (the files may have changed), keeping the chosen preset while it's
        still there. */
    void refresh();

    const std::vector<Entry>& getEntries() const noexcept    { return entries; }
    bool isEmpty() const noexcept                            { return entries.empty(); }
    bool hasPlugin() const noexcept                          { return pluginName.isNotEmpty(); }

    /** The plugin's folder of presets (made when something is saved into it). */
    juce::File getFolder() const;

    int getCurrent() const noexcept                          { return current; }
    const Entry* getCurrentEntry() const;
    bool isModified() const noexcept                         { return modified; }

    /** Chooses one (-1: none). Applying its values is the host's. */
    void choose (int index);
    void setModified (bool shouldBeModified)                 { modified = shouldBeModified && current >= 0; }

    /** The chosen preset again, by what a song remembered of it (its name and group) and whether
        its values had changed. False (and none chosen) when it's not there any more. */
    bool restore (const juce::String& name, const juce::String& group, bool wasModified);

    PresetMenu getMenu() const;

    //==========================================================================
    /** Save: the values over the chosen preset (one of the user's). */
    juce::Result save (const std::map<juce::String, float>& values);

    /** Save as: a new preset of the user's (one with the same name in My presets is replaced),
        chosen. */
    juce::Result saveAs (const juce::String& name, const std::map<juce::String, float>& values);

    /** The studio: a preset that comes with the plugin (presets.json; the same name replaced). */
    juce::Result saveFactory (const juce::String& name, const std::map<juce::String, float>& values);

    /** Whether My presets (or, factory: the factory presets) has one by this name. */
    bool hasPreset (const juce::String& name, bool factory) const;

    /** Deletes one: a preset of the user's goes to the Recycle Bin; a factory one only in the
        studio. */
    juce::Result remove (int index);

    /** A preset file from anywhere, copied into My presets and chosen. */
    juce::Result importPreset (const juce::File& file);

    /** Every preset in the list, into one bank file. */
    juce::Result exportBank (const juce::File& file) const;

    /** A bank file's presets, into a folder of their own named after the bank (replacing ones of
        the same name there); the first of them in the list chosen. */
    juce::Result importBank (const juce::File& file);

    /** A bank's name for a file chooser: "<plugin> presets". */
    juce::String suggestedBankName() const;

private:
    void read();
    juce::Result writePresetFile (const juce::File& file, const Preset& preset) const;
    int find (const juce::String& name, const juce::String& group) const;

    juce::String pluginName;
    juce::File factoryFile;
    bool factoryEditable = false;

    std::vector<Entry> entries;
    int current = -1;
    bool modified = false;
};

//==============================================================================
/**
    The preset manager's dialogs and choices, for a window that shows a plugin's presets (the
    studio, a made plugin's own window): it asks what an action needs (a name, a file), carries
    it out on the library, and says what changed.
*/
class PresetUi final
{
public:
    PresetUi (juce::Component& dialogParent, PresetLibrary& presets);
    ~PresetUi();

    /** The plugin's values now, by parameter id (what Save writes). */
    std::function<std::map<juce::String, float>()> getValues;

    /** A preset's values, to put on the plugin. */
    std::function<void (const Preset&)> apply;

    /** The presets, or the chosen one, changed: show them again. */
    std::function<void()> changed;

    /** Chooses one and puts its values on the plugin. */
    void choose (int index);

    /** Carries out what a menu asked for. */
    void perform (PresetAction action);

private:
    void askName (const juce::String& title, const juce::String& message, const juce::String& suggested, bool factory,
                  std::function<void (const juce::String&)> then);
    void confirm (const juce::String& title, const juce::String& message, const juce::String& yes, std::function<void()> then);
    void chooseFile (bool saving, bool bank, std::function<void (const juce::File&)> then);
    void report (const juce::Result& result, const juce::String& what);
    void chooseCurrentAndApply();

    juce::Component& parent;
    PresetLibrary& library;
    std::unique_ptr<juce::FileChooser> chooser;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetUi)
};
