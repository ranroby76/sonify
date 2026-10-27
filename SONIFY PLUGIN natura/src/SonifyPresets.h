// C:\workspace\SONIFY PLUGIN natura\src\SonifyPresets.h
#pragma once
//==============================================================================
//  SonifyPresets.h  -  the preset files every Sonify plugin shares.
//
//  A preset is one .sonifypreset file in Sonify's own presets folder:
//
//      <Sonify's user data>\Presets\<plugin>\<name>.sonifypreset
//
//  <Sonify's user data> is what Settings > Locations > User Data says, read
//  from Sonify's own settings (settings.json beside its other settings, in
//  %APPDATA%\Fanan\Sonify on Windows, Library/Fanan/Sonify on a Mac), and
//  Documents\Sonify when Sonify hasn't been told otherwise.  So a preset saved
//  on the plugin's own panel shows in Sonify's preset manager and the other way
//  round: it is the same folder and the same file.
//
//  The file holds
//
//      { "format": "Sonify preset", "formatVersion": 1, "plugin": "<plugin>",
//        "name": "<name>", "values": { "<parameter id>": <number>, ... } }
//
//  plus anything the plugin adds of its own beside those (a capture's file
//  name, say): Sonify reads the values and leaves the rest alone.
//
//  Presets a Sonify plugin kept in AppData before this are still listed, so
//  nothing saved then is lost; saving always writes to the folder above.
//
//  Message thread (it reads and writes files).
//==============================================================================
#include <juce_core/juce_core.h>

#include <map>
#include <optional>

namespace sonify::presets
{
    inline constexpr const char* extension = ".sonifypreset";
    inline constexpr const char* formatName = "Sonify preset";
    inline constexpr int formatVersion = 1;
    inline constexpr int maxNameLength = 60;      // as Sonify's own preset manager keeps them

    /** Where Sonify keeps its settings: %APPDATA%\Fanan\Sonify on Windows, Library/Fanan/Sonify
        on a Mac, ~/.config/Fanan/Sonify on Linux. */
    inline juce::File settingsFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Fanan")
                   .getChildFile ("Sonify");
    }

    /** Sonify's user data folder: the one chosen in Settings > Locations, else Documents\Sonify.
        Read from Sonify's settings every time, so moving it moves the presets with it. */
    inline juce::File userDataFolder()
    {
        const auto settings = juce::JSON::parse (settingsFolder().getChildFile ("settings.json"));
        const auto chosen = settings.getProperty ("locations", {}).getProperty ("userData", {}).toString().trim();

        if (chosen.isNotEmpty() && juce::File::isAbsolutePath (chosen))
            return juce::File (chosen);

        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Sonify");
    }

    /** A name as a folder or file name ("Tweed 5E3" stays readable). */
    inline juce::String legalName (const juce::String& name)
    {
        const auto legal = juce::File::createLegalFileName (name).trim();
        return legal.isNotEmpty() ? legal : juce::String ("Preset");
    }

    /** A preset's name as it is kept, exactly as Sonify's own preset manager keeps it: the lines
        and tabs out of it, trimmed, and no longer than 60. Characters a file name can't hold stay
        in the name ("Amber Room / Edge Crunch" reads as it was typed); the file is named with
        legalName, which takes them out of the file name alone. */
    inline juce::String cleanName (const juce::String& name)
    {
        const auto clean = name.replaceCharacters ("\r\n\t", "   ").trim().substring (0, maxNameLength).trim();
        return clean.isNotEmpty() ? clean : juce::String ("Untitled");
    }

    /** This plugin's presets folder, the one Sonify reads. Made when something is saved into it. */
    inline juce::File folderFor (const juce::String& plugin)
    {
        return userDataFolder().getChildFile ("Presets").getChildFile (legalName (plugin));
    }

    /** Where a Sonify plugin kept its presets before this: still read, never written. */
    inline juce::File oldFolderFor (const juce::String& plugin)
    {
        return settingsFolder().getChildFile ("Presets").getChildFile (legalName (plugin));
    }

    /** The file a preset of this name goes in. */
    inline juce::File fileFor (const juce::String& plugin, const juce::String& name)
    {
        return folderFor (plugin).getChildFile (legalName (cleanName (name)) + extension);
    }

    /** Every preset file, this plugin's folder first and then the old one, each alphabetical; a
        name in both is listed once (the one Sonify reads wins). */
    inline juce::Array<juce::File> list (const juce::String& plugin)
    {
        const juce::String pattern = juce::String ("*") + extension;

        auto files = folderFor (plugin).findChildFiles (juce::File::findFiles, false, pattern);
        files.sort();

        auto older = oldFolderFor (plugin).findChildFiles (juce::File::findFiles, false, pattern);
        older.sort();

        for (const auto& old : older)
        {
            bool already = false;

            for (const auto& f : files)
                already = already || f.getFileName().equalsIgnoreCase (old.getFileName());

            if (! already)
                files.add (old);
        }

        return files;
    }

    /** Writes one. extras: an object whose properties go beside name and values (what only this
        plugin reads); nothing when it's not an object. */
    inline juce::Result save (const juce::File& file,
                              const juce::String& plugin,
                              const juce::String& name,
                              const std::map<juce::String, float>& values,
                              const juce::var& extras = {})
    {
        juce::DynamicObject::Ptr valueFields (new juce::DynamicObject());

        for (const auto& [id, value] : values)
            valueFields->setProperty (id, value);

        juce::DynamicObject::Ptr root (new juce::DynamicObject());
        root->setProperty ("format", formatName);
        root->setProperty ("formatVersion", formatVersion);
        root->setProperty ("plugin", plugin);
        root->setProperty ("name", cleanName (name));
        root->setProperty ("values", juce::var (valueFields.get()));

        if (auto* more = extras.getDynamicObject())
            for (const auto& property : more->getProperties())
                if (! root->hasProperty (property.name))
                    root->setProperty (property.name, property.value);

        if (const auto made = file.getParentDirectory().createDirectory(); made.failed())
            return juce::Result::fail ("Couldn't make the folder " + file.getParentDirectory().getFullPathName()
                                       + " (" + made.getErrorMessage().trim() + ")");

        if (! file.replaceWithText (juce::JSON::toString (juce::var (root.get()))))
            return juce::Result::fail ("Couldn't write " + file.getFullPathName());

        return juce::Result::ok();
    }

    struct Loaded
    {
        juce::String name;                          // the preset's name (the file's, when it holds none)
        juce::String plugin;                        // the plugin it was saved from ("" when it says none)
        std::map<juce::String, float> values;       // only the numbers: what a parameter can take
        juce::var whole;                            // the file's JSON, for whatever else a plugin keeps in it
    };

    /** Reads one. Nothing when the file isn't a preset (no values in it). */
    inline std::optional<Loaded> load (const juce::File& file)
    {
        const auto json = juce::JSON::parse (file.loadFileAsString());
        auto* values = json.getProperty ("values", {}).getDynamicObject();

        if (values == nullptr)
            return std::nullopt;

        Loaded out;
        out.whole = json;
        out.plugin = json.getProperty ("plugin", {}).toString().trim();
        out.name = json.getProperty ("name", {}).toString().trim();

        if (out.name.isEmpty())
            out.name = file.getFileNameWithoutExtension();

        for (const auto& property : values->getProperties())
            if (property.value.isDouble() || property.value.isInt() || property.value.isInt64() || property.value.isBool())
                out.values[property.name.toString()] = (float) (double) property.value;

        return out;
    }

    /** Opens the plugin's presets folder in the computer's file browser. */
    inline void showFolder (const juce::String& plugin)
    {
        const auto folder = folderFor (plugin);
        folder.createDirectory();
        folder.startAsProcess();
    }
}
