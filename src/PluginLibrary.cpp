// D:\workspace\Fanan_web_hub\src\PluginLibrary.cpp

#include "PluginLibrary.h"

#include "StellaLookAndFeel.h"

#include <map>

// The blob engine (stella_blob.h), compiled into Sonify here, once, with what protected blobs need
// of this computer (stella_licence.h's native part: its ID, the key files, the license.soni beside
// a blob). stella_licence.h is first included here, by the engine: after every JUCE header, as it
// brings in <windows.h> on Windows (lean, without min and max).
#ifndef STELLA_LICENCE_NATIVE
 #define STELLA_LICENCE_NATIVE 1
#endif
#define STELLA_BLOB_IMPLEMENTATION 1
#include "stella_blob.h"

namespace
{
    juce::File dataFolder()
    {
       #if JUCE_MAC
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Application Support");
       #elif JUCE_LINUX || JUCE_BSD
        if (const auto config = juce::SystemStats::getEnvironmentVariable ("XDG_CONFIG_HOME", {}); juce::File::isAbsolutePath (config))
            return juce::File (config);

        return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".config");
       #else
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
       #endif
    }

    juce::File documentsFolder()
    {
       #if JUCE_LINUX || JUCE_BSD
        return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Documents");
       #else
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
       #endif
    }

    juce::String sizeText (juce::int64 bytes)
    {
        if (bytes >= (juce::int64) 1 << 30) return juce::String ((double) bytes / (double) (1 << 30), 1) + " GB";
        if (bytes >= 1 << 20)               return juce::String ((double) bytes / (double) (1 << 20), 1) + " MB";
        return juce::String (juce::jmax ((juce::int64) 1, bytes / 1024)) + " KB";
    }

    bool sameName (const juce::String& a, const juce::String& b)
    {
       #if JUCE_LINUX || JUCE_BSD
        return a == b;
       #else
        return a.equalsIgnoreCase (b);
       #endif
    }
}

//==============================================================================
juce::Array<PluginLibrary::Instrument> PluginLibrary::load (const juce::File& folder)
{
    juce::Array<Instrument> instruments;
    const auto parsed = juce::JSON::parse (folder.getChildFile (fileName));
    const auto* list = parsed.isArray() ? parsed.getArray() : parsed.getProperty ("instruments", {}).getArray();

    if (list == nullptr)
        return instruments;

    for (const auto& item : *list)
    {
        Instrument instrument;
        instrument.name = item.getProperty ("name", {}).toString().trim();
        const auto file = item.getProperty ("file", {}).toString();
        const auto origin = item.getProperty ("origin", {}).toString();

        if (juce::File::isAbsolutePath (file))
            instrument.file = juce::File (file);
        else if (juce::File::isAbsolutePath (origin))
            instrument.file = juce::File (origin);
        else if (file.isNotEmpty())
            instrument.file = folder.getChildFile ("library").getChildFile (file);   // a made plugin's, with no origin

        if (instrument.name.isEmpty())
            instrument.name = instrument.file.getFileNameWithoutExtension();

        if (instrument.name.isNotEmpty())
            instruments.add (instrument);
    }

    return instruments;
}

bool PluginLibrary::save (const juce::File& folder, const juce::Array<Instrument>& instruments)
{
    juce::Array<juce::var> list;

    for (const auto& instrument : instruments)
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("name", instrument.name);
        item->setProperty ("file", instrument.file.getFullPathName());
        list.add (juce::var (item.get()));
    }

    const auto file = folder.getChildFile (fileName);

    if (instruments.isEmpty())
        return ! file.exists() || file.deleteFile();

    return file.replaceWithText (juce::JSON::toString (juce::var (list)));
}

bool PluginLibrary::writeMade (const juce::File& pluginFolder, const juce::Array<Instrument>& instruments, bool origins)
{
    const auto file = pluginFolder.getChildFile (fileName);

    if (instruments.isEmpty())
        return ! file.exists() || file.deleteFile();

    juce::Array<juce::var> list;

    for (const auto& instrument : instruments)
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("name", instrument.name);
        item->setProperty ("file", instrument.file.getFileName());

        if (origins)
            item->setProperty ("origin", instrument.file.getFullPathName());

        list.add (juce::var (item.get()));
    }

    return file.replaceWithText (juce::JSON::toString (juce::var (list)));
}

juce::String PluginLibrary::folderName (const juce::String& text)
{
    juce::String out;

    for (auto c : text)
        out << (c < 32 || juce::String ("<>:\"/\\|?*").containsChar (c) ? juce::String ("_") : juce::String::charToString (c));

    while (out.endsWithChar ('.') || out.endsWithChar (' '))
        out = out.dropLastCharacters (1);

    return out.isEmpty() ? juce::String ("Plugin") : out;
}

namespace
{
    /** A plugin's company as its folder: a plugin made without one is "Made in Sonify"'s. */
    juce::String vendorFolder (const juce::String& vendor)
    {
        return PluginLibrary::folderName (vendor.trim().isNotEmpty() ? vendor : juce::String ("Made in Sonify"));
    }
}

juce::File PluginLibrary::homeFolder (const juce::String& vendor, const juce::String& plugin)
{
    return documentsFolder().getChildFile (vendorFolder (vendor)).getChildFile (folderName (plugin));
}

juce::File PluginLibrary::listFile (const juce::String& vendor, const juce::String& plugin)
{
    return dataFolder().getChildFile (vendorFolder (vendor)).getChildFile (folderName (plugin)).getChildFile ("library.txt");
}

void PluginLibrary::addToList (const juce::String& vendor, const juce::String& plugin, const juce::StringArray& paths)
{
    const auto file = listFile (vendor, plugin);
    juce::StringArray lines;

    if (file.existsAsFile())
        lines.addLines (file.loadFileAsString());

    bool changed = false;

    for (const auto& path : paths)
    {
        bool known = false;

        for (const auto& line : lines)
            known = known || sameName (line.trim(), path);

        if (! known && path.isNotEmpty())
        {
            lines.add (path);
            changed = true;
        }
    }

    if (! changed)
        return;

    lines.removeEmptyStrings();

    if (file.getParentDirectory().createDirectory())
        file.replaceWithText (lines.joinIntoString ("\n") + "\n");
}

juce::File PluginLibrary::find (const juce::String& blobName, const juce::String& vendor, const juce::String& plugin,
                                const juce::Array<juce::File>& besides, const juce::File& origin)
{
    if (blobName.isEmpty())
        return {};

    // 1. The plugin's list: folders, or the blobs themselves.
    if (const auto list = listFile (vendor, plugin); list.existsAsFile())
    {
        for (auto line : juce::StringArray::fromLines (list.loadFileAsString()))
        {
            line = line.trim().unquoted().trim();

            if (line.isEmpty() || line.startsWithChar ('#') || ! juce::File::isAbsolutePath (line))
                continue;

            const juce::File listed (line);

            if (listed.existsAsFile() && sameName (listed.getFileName(), blobName))
                return listed;

            if (const auto inside = listed.getChildFile (blobName); inside.existsAsFile())
                return inside;
        }
    }

    // 2. Where a buyer puts them.
    if (const auto home = homeFolder (vendor, plugin).getChildFile (blobName); home.existsAsFile())
        return home;

    // 3. Beside the plugin.
    for (const auto& folder : besides)
        if (const auto beside = folder.getChildFile (blobName); beside.existsAsFile())
            return beside;

    // 4. Where it was added from.
    if (origin.existsAsFile())
        return origin;

    return {};
}

std::vector<stella::blob::Entry> PluginLibrary::entriesFor (const juce::Array<Instrument>& instruments, const juce::String& vendor,
                                                            const juce::String& plugin, const juce::Array<juce::File>& besides,
                                                            bool ownFirst)
{
    std::vector<stella::blob::Entry> entries;

    for (const auto& instrument : instruments)
    {
        stella::blob::Entry entry;
        entry.name = instrument.name.toStdString();
        const auto found = ownFirst && instrument.file.existsAsFile() ? instrument.file
                                                                      : find (instrument.file.getFileName(), vendor, plugin, besides, instrument.file);
        entry.path = found.getFullPathName().toStdString();
        entries.push_back (entry);
    }

    return entries;
}

juce::String PluginLibrary::describe (const juce::File& blob)
{
    if (! blob.existsAsFile())
        return "Not on this computer";

    // Read once for each version of the file (Fanan AI is told the library with every request).
    static juce::CriticalSection cacheLock;
    static std::map<juce::String, std::pair<juce::Time, juce::String>> cache;
    const auto key = blob.getFullPathName() + "|"   // a protected one opens once its license.soni is beside it
                   + juce::String (blob.getSiblingFile ("license.soni").getLastModificationTime().toMilliseconds());
    const auto modified = blob.getLastModificationTime();

    {
        const juce::ScopedLock hold (cacheLock);

        if (const auto found = cache.find (key); found != cache.end() && found->second.first == modified)
            return found->second.second;
    }

    stella::blob::Summary summary;
    std::string error;
    juce::String text;

    if (! stella::blob::summarise (blob.getFullPathName().toStdString(), summary, error))
        text = juce::String (error);
    else if (summary.locked)
        text = "Protected (" + juce::String (juce::CharPointer_UTF8 (summary.product.c_str())) + "), its license.soni isn't beside it, "
             + sizeText ((juce::int64) summary.bytes);
    else
        text = (summary.licenceName.empty() ? juce::String() : "Protected (" + juce::String (juce::CharPointer_UTF8 (summary.licenceName.c_str())) + "), ")
             + juce::String (summary.regions) + (summary.regions == 1 ? " region, " : " regions, ") + sizeText ((juce::int64) summary.bytes);

    const juce::ScopedLock hold (cacheLock);
    cache[key] = { modified, text };
    return text;
}

//==============================================================================
std::vector<stella::blob::LicenceInfo> PluginLibrary::licencesInUse()
{
    return stella::blob::licencesInUse();
}

bool PluginLibrary::licenceOf (const juce::String& product, stella::licence::Soni& soni)
{
    return stella::blob::licenceOf (product.toStdString(), soni);
}

juce::String PluginLibrary::registerSerial (const juce::String& serial)
{
    return juce::String (juce::CharPointer_UTF8 (stella::blob::registerSerial (serial.toStdString()).c_str()));
}

void PluginLibrary::recheckLicences()
{
    stella::blob::recheckLicences();
}

//==============================================================================
namespace
{
    class LibraryEditor final : public juce::Component,
                                private juce::ListBoxModel,
                                private juce::Timer
    {
    public:
        explicit LibraryEditor (PluginLibrary::WindowHooks h)
            : hooks (std::move (h))
        {
            intro.setText ("The plugin's instruments: blobs made by the Sampling Studio's SFZ to Blob. It plays them by number, "
                           "0 first; an Instrument list from the Edit UI toolbox shows their names.", juce::dontSendNotification);
            intro.setFont (Theme::font (13.0f));
            intro.setColour (juce::Label::textColourId, Theme::muted);
            intro.setJustificationType (juce::Justification::topLeft);
            addAndMakeVisible (intro);

            list.setModel (this);
            list.setRowHeight (44);
            list.setColour (juce::ListBox::backgroundColourId, Theme::inset);
            list.setColour (juce::ListBox::outlineColourId, Theme::outline);
            list.setOutlineThickness (1);
            list.setMultipleSelectionEnabled (false);
            addAndMakeVisible (list);

            for (auto* b : { &add, &rename, &up, &down, &remove })
                addAndMakeVisible (b);

            add.setTooltip ("Add blobs (.blob files) to the library");
            rename.setTooltip ("The name the plugin shows for the chosen instrument");
            remove.setTooltip ("Take the chosen instrument out of the library (its blob stays where it is)");

            add.onClick = [this] { addBlobs(); };
            rename.onClick = [this] { renameChosen(); };
            up.onClick = [this] { move (-1); };
            down.onClick = [this] { move (1); };
            remove.onClick = [this] { removeChosen(); };

            reload();
            setSize (620, 420);
            startTimerHz (2);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (Theme::panel);

            if (instruments.isEmpty())
            {
                g.setColour (Theme::muted);
                g.setFont (Theme::font (14.0f));
                g.drawFittedText ("No instruments yet: Add... picks blobs.", list.getBounds().reduced (20), juce::Justification::centred, 2);
            }
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (14);
            intro.setBounds (area.removeFromTop (38));
            area.removeFromTop (6);

            auto buttons = area.removeFromBottom (30);
            area.removeFromBottom (10);
            list.setBounds (area);

            add.setBounds (buttons.removeFromLeft (90));
            buttons.removeFromLeft (8);
            rename.setBounds (buttons.removeFromLeft (90));
            remove.setBounds (buttons.removeFromRight (90));
            buttons.removeFromRight (8);
            down.setBounds (buttons.removeFromRight (70));
            buttons.removeFromRight (6);
            up.setBounds (buttons.removeFromRight (70));
        }

        void reload()
        {
            instruments = hooks.get != nullptr ? hooks.get() : juce::Array<PluginLibrary::Instrument>();
            summaries.clearQuick();

            for (const auto& instrument : instruments)
                summaries.add (PluginLibrary::describe (instrument.file));

            list.updateContent();
            list.repaint();
            updateButtons();
            repaint();
        }

    private:
        int getNumRows() override    { return instruments.size(); }

        void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
        {
            if (! juce::isPositiveAndBelow (row, instruments.size()))
                return;

            if (selected)
                g.fillAll (Theme::accentDeep);

            g.setColour (Theme::outline);
            g.fillRect (0, height - 1, width, 1);

            const auto& instrument = instruments.getReference (row);
            auto area = juce::Rectangle<int> (0, 0, width, height).reduced (10, 4);

            g.setColour (Theme::muted);
            g.setFont (Theme::monoFont (13.0f));
            g.drawText (juce::String (row), area.removeFromLeft (30), juce::Justification::centredLeft, false);

            const auto state = hooks.stateOf != nullptr ? hooks.stateOf (row) : stella::blob::notLoaded;
            juce::String status;
            auto statusColour = Theme::muted;

            if (! instrument.file.existsAsFile())
            {
                status = "Not on this computer";
                statusColour = Theme::hot;
            }
            else if (state == stella::blob::loading)
            {
                status = juce::String::fromUTF8 ("Loading\xe2\x80\xa6");
            }
            else if (state == stella::blob::ready)
            {
                status = "Playing";
                statusColour = Theme::accent;
            }
            else if (state == stella::blob::unavailable)
            {
                status = hooks.problemOf != nullptr ? hooks.problemOf (row) : juce::String ("Can't play");
                statusColour = Theme::hot;
            }

            auto top = area.removeFromTop (area.getHeight() / 2);
            g.setColour (Theme::text);
            g.setFont (Theme::font (14.5f, true));
            g.drawText (instrument.name, top.withTrimmedRight (160), juce::Justification::centredLeft, true);

            g.setColour (statusColour);
            g.setFont (Theme::font (12.5f));
            g.drawText (status, top.removeFromRight (160), juce::Justification::centredRight, true);

            g.setColour (Theme::muted);
            g.setFont (Theme::font (12.0f));
            g.drawText (instrument.file.getFileName() + juce::String::fromUTF8 ("  \xc2\xb7  ") + summaries[row], area,
                        juce::Justification::centredLeft, true);
        }

        void selectedRowsChanged (int) override    { updateButtons(); }
        void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override    { renameChosen(); }

        void deleteKeyPressed (int) override       { removeChosen(); }

        void timerCallback() override
        {
            list.repaint();   // the plugin's word on each: loading, playing...
        }

        void updateButtons()
        {
            const auto row = list.getSelectedRow();
            const bool chosen = juce::isPositiveAndBelow (row, instruments.size());
            rename.setEnabled (chosen);
            remove.setEnabled (chosen);
            up.setEnabled (chosen && row > 0);
            down.setEnabled (chosen && row < instruments.size() - 1);
        }

        void commit (int select)
        {
            if (hooks.set != nullptr)
                hooks.set (instruments);

            reload();

            if (juce::isPositiveAndBelow (select, instruments.size()))
                list.selectRow (select);
        }

        void addBlobs()
        {
            const auto start = hooks.startFolder != nullptr ? hooks.startFolder() : juce::File();
            chooser = std::make_unique<juce::FileChooser> ("Add blobs to the library", start, "*.blob");

            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::canSelectMultipleItems,
                                  [safe = juce::Component::SafePointer<LibraryEditor> (this)] (const juce::FileChooser& fc)
                                  {
                                      if (safe == nullptr || fc.getResults().isEmpty())
                                          return;

                                      auto& self = *safe;
                                      int last = -1;

                                      for (const auto& file : fc.getResults())
                                      {
                                          bool known = false;

                                          for (const auto& instrument : self.instruments)
                                              known = known || instrument.file == file;

                                          if (known || self.instruments.size() >= stella::blob::maxInstruments)
                                              continue;

                                          stella::blob::Summary summary;
                                          std::string error;
                                          stella::blob::summarise (file.getFullPathName().toStdString(), summary, error);

                                          PluginLibrary::Instrument instrument;
                                          instrument.file = file;
                                          instrument.name = summary.name.empty() ? file.getFileNameWithoutExtension()
                                                                                 : juce::String (juce::CharPointer_UTF8 (summary.name.c_str()));
                                          self.instruments.add (instrument);
                                          last = self.instruments.size() - 1;
                                      }

                                      if (last >= 0)
                                          self.commit (last);
                                  });
        }

        void renameChosen()
        {
            const auto row = list.getSelectedRow();

            if (! juce::isPositiveAndBelow (row, instruments.size()))
                return;

            auto* window = new juce::AlertWindow ("Rename", "The name the plugin shows for this instrument:", juce::MessageBoxIconType::NoIcon, this);
            window->addTextEditor ("name", instruments[row].name);
            window->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
            window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

            window->enterModalState (true, juce::ModalCallbackFunction::create (
                [safe = juce::Component::SafePointer<LibraryEditor> (this), window, row] (int result)
                {
                    if (safe == nullptr || result != 1)
                        return;

                    if (const auto name = window->getTextEditorContents ("name").trim(); name.isNotEmpty()
                        && juce::isPositiveAndBelow (row, safe->instruments.size()))
                    {
                        safe->instruments.getReference (row).name = name;
                        safe->commit (row);
                    }
                }), true);
        }

        void move (int delta)
        {
            const auto row = list.getSelectedRow();
            const auto target = row + delta;

            if (! juce::isPositiveAndBelow (row, instruments.size()) || ! juce::isPositiveAndBelow (target, instruments.size()))
                return;

            instruments.swap (row, target);
            commit (target);
        }

        void removeChosen()
        {
            const auto row = list.getSelectedRow();

            if (! juce::isPositiveAndBelow (row, instruments.size()))
                return;

            instruments.remove (row);
            commit (juce::jmin (row, instruments.size() - 1));
        }

        PluginLibrary::WindowHooks hooks;
        juce::Array<PluginLibrary::Instrument> instruments;
        juce::StringArray summaries;
        juce::Label intro;
        juce::ListBox list;
        juce::TextButton add { juce::String::fromUTF8 ("Add\xe2\x80\xa6") }, rename { juce::String::fromUTF8 ("Rename\xe2\x80\xa6") },
                         up { "Up" }, down { "Down" }, remove { "Remove" };
        std::unique_ptr<juce::FileChooser> chooser;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibraryEditor)
    };

    class LibraryWindow final : public juce::DocumentWindow
    {
    public:
        LibraryWindow (const juce::String& pluginName, PluginLibrary::WindowHooks hooks)
            : DocumentWindow ("Sample library: " + pluginName, Theme::panel, DocumentWindow::closeButton),
              editor (std::move (hooks))
        {
            setUsingNativeTitleBar (true);
            setContentNonOwned (&editor, true);
            setResizable (true, false);
            setResizeLimits (440, 300, 1600, 1400);
        }

        ~LibraryWindow() override
        {
            clearContentComponent();   // the editor is a member: let go of it before it goes
        }

        void closeButtonPressed() override    { setVisible (false); }

        void visibilityChanged() override
        {
            DocumentWindow::visibilityChanged();

            if (isVisible())
                editor.reload();
        }

    private:
        LibraryEditor editor;
    };
}

std::unique_ptr<juce::DocumentWindow> PluginLibrary::makeWindow (const juce::String& pluginName, WindowHooks hooks)
{
    return std::make_unique<LibraryWindow> (pluginName, std::move (hooks));
}
