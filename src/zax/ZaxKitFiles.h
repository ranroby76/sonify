// C:\workspace\SONIFY PLUGIN zax\src\ZaxKitFiles.h
#pragma once

#include <juce_core/juce_core.h>
#include <memory>
#include <vector>
#include "ZaxFactoryKits.h"
#include "../Preferences.h"       // SettingsFolder: where Sonify keeps what it learns

//==============================================================================
//  ZaxKitFiles.h  —  where Zax keeps its kits, in Sonify's own folder:
//
//      <app data>\Fanan\Sonify\Zax\Factory Kits\<name>.zaxkit   the shipped kits
//      <app data>\Fanan\Sonify\Zax\My Kits\<name>.zaxkit        the user's own
//
//  <app data> is Sonify's own settings folder (SettingsFolder::get(): %APPDATA%\Fanan\Sonify on
//  Windows, Library/Fanan/Sonify on a Mac), so Zax's kits sit with everything else Sonify keeps.
//  The factory kits are written there from the
//  built-in definitions (ZaxFactoryKits.h) the first time Zax runs, and only
//  when a file is missing - so a factory kit reshaped and saved stays as saved.
//
//  Message thread only.
//==============================================================================

namespace zax::files
{
    inline constexpr const char* kExtension = ".zaxkit";

    // Sonify's own settings folder, the one it keeps everything else in: %APPDATA%\Fanan\Sonify
    // on Windows, Library/Fanan/Sonify on a Mac, ~/.config/Fanan/Sonify on Linux. Zax is built
    // into Sonify, so it asks Sonify rather than working the path out again.
    inline juce::File sonifyFolder()
    {
        return SettingsFolder::get();
    }

    inline juce::File zaxFolder()       { return sonifyFolder().getChildFile ("Zax"); }
    inline juce::File factoryFolder()   { return zaxFolder().getChildFile ("Factory Kits"); }
    inline juce::File userFolder()      { return zaxFolder().getChildFile ("My Kits"); }

    // A kit name made safe as a file name ("Drum & Bass" stays readable)
    inline juce::String fileNameFor (const juce::String& kitName)
    {
        auto s = juce::File::createLegalFileName (kitName.trim());
        return (s.isEmpty() ? juce::String ("Kit") : s) + kExtension;
    }

    inline juce::Result writeKit (const Kit& kit, const juce::File& file)
    {
        if (const auto r = file.getParentDirectory().createDirectory(); r.failed())
            return r;
        if (! file.replaceWithText (juce::String::fromUTF8 (serializeKit (kit).c_str())))
            return juce::Result::fail ("Could not write " + file.getFullPathName());
        return juce::Result::ok();
    }

    inline juce::Result readKit (const juce::File& file, Kit& out)
    {
        if (! file.existsAsFile())
            return juce::Result::fail ("No such kit: " + file.getFullPathName());
        auto heap = std::make_unique<Kit>();
        heap->name = file.getFileNameWithoutExtension().toStdString();
        if (! deserializeKit (file.loadFileAsString().toStdString(), *heap))
            return juce::Result::fail (file.getFileName() + " is not a Zax kit.");
        out = *heap;
        return juce::Result::ok();
    }

    // Writes every factory kit whose file is missing.  Run once at startup.
    inline void installFactoryKits()
    {
        const auto folder = factoryFolder();
        folder.createDirectory();
        for (const auto& k : factoryKits())
        {
            const auto f = folder.getChildFile (fileNameFor (juce::String (k.name)));
            if (! f.existsAsFile())
                writeKit (k, f);
        }
    }

    struct Entry
    {
        juce::String name;
        juce::File   file;
        bool         factory = false;
    };

    // The kit menu: the factory kits in their shipping order (then any other
    // file in that folder), then the user's kits, alphabetical.
    inline std::vector<Entry> listKits()
    {
        std::vector<Entry> out;
        juce::StringArray seen;
        const auto ff = factoryFolder();
        for (const auto& k : factoryKits())
        {
            const auto f = ff.getChildFile (fileNameFor (juce::String (k.name)));
            out.push_back ({ juce::String (k.name), f, true });
            seen.add (f.getFileName());
        }
        auto extra = ff.findChildFiles (juce::File::findFiles, false, juce::String ("*") + kExtension);
        extra.sort();
        for (const auto& f : extra)
            if (! seen.contains (f.getFileName()))
                out.push_back ({ f.getFileNameWithoutExtension(), f, true });

        auto mine = userFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + kExtension);
        mine.sort();
        for (const auto& f : mine)
            out.push_back ({ f.getFileNameWithoutExtension(), f, false });
        return out;
    }

    // A kit by its entry: the file when it reads, else the built-in kit of a
    // factory entry (a deleted or broken factory file can never leave Zax mute).
    inline juce::Result loadEntry (const Entry& e, Kit& out)
    {
        const auto r = readKit (e.file, out);
        if (r.wasOk()) { out.name = e.name.toStdString(); return r; }
        if (e.factory)
            if (const auto* k = findFactoryKit (e.name.toStdString())) { out = *k; return juce::Result::ok(); }
        return r;
    }
}
