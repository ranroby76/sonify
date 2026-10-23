// D:\workspace\Fanan_web_hub\src\MainDawProject.cpp

#include "MainComponent.h"
#include "DawProject.h"
#include "Song.h"

//==============================================================================
// File > Import DAWproject... and File > Export DAWproject... (DawProject.h).
//
// Import: a .dawproject becomes a new song in a folder of its own (in the songs folder, named
// after its title), written as a .fsong and then opened as any song is. The old song goes first,
// as with File > Open (after asking to save its changes). What didn't come across as it was is
// said once the song is open.
//
// Export: the song as it is now, into a .dawproject (the song itself isn't saved or changed). The
// plugins give their settings here, on the message thread; the file is written on a thread of its
// own (its audio can be large), and Sonify says when it's done and what isn't in it.

void MainComponent::handleDawProject (const juce::var& event)
{
    const auto action = event.getProperty ("action", {}).toString();

    if (action == "import")
        whenSongIsSafe ([this] { chooseDawProjectToImport(); });
    else if (action == "export")
        chooseDawProjectExport();
}

void MainComponent::chooseDawProjectToImport()
{
    dawProjectChooser = std::make_unique<juce::FileChooser> ("Import a DAWproject", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                             juce::String ("*") + DawProject::fileExtension);
    dawProjectChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                    [safeThis = juce::Component::SafePointer<MainComponent> (this)] (const juce::FileChooser& chooser)
    {
        if (safeThis != nullptr && chooser.getResult().existsAsFile())
            safeThis->importDawProject (chooser.getResult());
    });
}

void MainComponent::importDawProject (const juce::File& source)
{
    // Unpacking its audio can take a moment: the page shows "Importing" first.
    pushSong (false, "Importing " + source.getFileNameWithoutExtension() + "...");

    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<MainComponent> (this), source]
    {
        if (safeThis == nullptr)
            return;

        // Its own folder in the songs folder, as File > New Song makes; a name that's taken gets " (2)".
        auto name = juce::File::createLegalFileName (DawProject::titleOf (source).trim());

        if (name.isEmpty())
            name = "Imported";

        auto folder = Song::getDefaultFolder().getChildFile (name);

        if (folder.exists())
            folder = folder.getNonexistentSibling (true);

        if (! folder.createDirectory())
        {
            safeThis->pushSong();
            safeThis->showSongMessage ("Couldn't import " + source.getFileName(), "The folder " + folder.getFullPathName() + " couldn't be made.");
            return;
        }

        // The VST3 plugins in the Browser: the ones the project's plugins are looked for among.
        std::vector<DawProject::InstalledVst3> installed;

        for (const auto& entry : safeThis->library.getEntries())
            if (! entry.missing && entry.format() == "VST3")
                installed.push_back ({ entry.file, entry.id, entry.name });

        juce::File written;
        juce::StringArray notes;
        const auto result = DawProject::importProject (source, folder, installed, written, notes);

        if (result.failed())
        {
            folder.deleteRecursively();   // made just now, for this song
            safeThis->pushSong();
            safeThis->showSongMessage ("Couldn't import " + source.getFileName(), result.getErrorMessage());
            return;
        }

        safeThis->importTitle = notes.isEmpty() ? juce::String() : "Imported " + source.getFileName();
        safeThis->importNotes = notes;
        safeThis->openSong (written);
    });
}

void MainComponent::chooseDawProjectExport()
{
    if (dawProjectWriter != nullptr)
    {
        showSongMessage ("Export DAWproject", "The last export is still being written.");
        return;
    }

    auto folder = songFile != juce::File() ? songFile.getParentDirectory() : Song::getDefaultFolder();
    folder.createDirectory();

    dawProjectChooser = std::make_unique<juce::FileChooser> ("Export the song as a DAWproject",
                                                             folder.getChildFile (juce::File::createLegalFileName (songTitle()) + DawProject::fileExtension),
                                                             juce::String ("*") + DawProject::fileExtension);
    dawProjectChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                        | juce::FileBrowserComponent::warnAboutOverwriting,
                                    [safeThis = juce::Component::SafePointer<MainComponent> (this)] (const juce::FileChooser& chooser)
    {
        if (safeThis != nullptr && chooser.getResult() != juce::File())
            safeThis->exportDawProject (chooser.getResult().withFileExtension (DawProject::fileExtension));
    });
}

void MainComponent::exportDawProject (const juce::File& target)
{
    if (dawProjectWriter != nullptr)
        return;

    juce::StringArray notes;
    auto package = DawProject::exportProject (engine, songTitle(), notes);

    pushSong (false, "Exporting " + target.getFileNameWithoutExtension() + "...");

    dawProjectWriter = std::make_unique<DawProject::Writer> (std::move (package), target,
                                                             [safeThis = juce::Component::SafePointer<MainComponent> (this), target, notes] (juce::Result result)
    {
        if (safeThis == nullptr)
            return;

        safeThis->dawProjectWriter.reset();
        safeThis->pushSong();

        if (result.failed())
        {
            safeThis->showSongMessage ("Couldn't export " + target.getFileName(), result.getErrorMessage());
            return;
        }

        auto shown = notes;

        if (shown.size() > 24)
        {
            shown.removeRange (24, shown.size());
            shown.add ("... and " + juce::String (notes.size() - 24) + " more.");
        }

        safeThis->showSongMessage ("Exported " + target.getFileName(),
                                   "The song is in " + target.getFullPathName() + "."
                                       + (shown.isEmpty() ? juce::String() : "\n\nNot in it:\n" + shown.joinIntoString ("\n")));
    });
}

juce::var MainComponent::keptToPage (const juce::var& kept)
{
    juce::Array<juce::var> list;

    if (const auto* items = kept.getArray())
    {
        for (const auto& item : *items)
        {
            juce::DynamicObject::Ptr shown (new juce::DynamicObject());
            shown->setProperty ("name", item.getProperty ("name", "Device"));
            shown->setProperty ("format", DawProject::formatName (item.getProperty ("format", {}).toString()));
            list.add (juce::var (shown.get()));
        }
    }

    return list;
}
