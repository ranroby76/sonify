// D:\workspace\Fanan_web_hub\src\PresetLibrary.cpp

#include "PresetLibrary.h"
#include "Preferences.h"   // Sonify's user data folder

#include <algorithm>

namespace
{
    constexpr const char* presetFormat = "Sonify preset";
    constexpr const char* bankFormat = "Sonify preset bank";
    constexpr int maxNameLength = 60;
    constexpr int maxBankPresets = 1000;

    juce::String cleanName (const juce::String& name)
    {
        return name.replaceCharacters ("\r\n\t", "   ").trim().substring (0, maxNameLength).trim();
    }

    /** A preset from a file's JSON: its name (or the file's) and its values. */
    Preset presetFrom (const juce::var& json, const juce::String& fallbackName)
    {
        auto preset = PresetBank::fromVar (json);

        if (preset.name.isEmpty())
            preset.name = fallbackName;

        preset.name = cleanName (preset.name);
        return preset;
    }

    juce::String legalName (const juce::String& name)
    {
        auto legal = juce::File::createLegalFileName (name).trim();
        return legal.isNotEmpty() ? legal : juce::String ("Preset");
    }

    bool byName (const juce::File& a, const juce::File& b)
    {
        return a.getFileName().compareNatural (b.getFileName()) < 0;
    }
}

//==============================================================================
juce::String PresetMenu::currentText() const
{
    if (juce::isPositiveAndBelow (current, names.size()))
        return names[current] + (modified ? " *" : "");

    return names.isEmpty() ? juce::String ("Default") : juce::String::fromUTF8 ("Choose a preset\xe2\x80\xa6");
}

int PresetMenu::step (int delta) const
{
    const auto count = names.size();

    if (count == 0)
        return -1;

    if (current < 0)
        return delta >= 0 ? 0 : count - 1;

    return ((current + delta) % count + count) % count;
}

bool PresetMenu::operator== (const PresetMenu& other) const
{
    return names == other.names && groups == other.groups && current == other.current && modified == other.modified
        && canSave == other.canSave && canDelete == other.canDelete && canSaveFactory == other.canSaveFactory;
}

void PresetMenus::addPresets (juce::PopupMenu& menu, const PresetMenu& presets, int firstId)
{
    juce::String heading;

    for (int i = 0; i < presets.names.size(); ++i)
    {
        if (i == 0 || presets.groups[i] != heading)
        {
            heading = presets.groups[i];
            menu.addSectionHeader (heading);
        }

        menu.addItem (firstId + i, presets.names[i], true, i == presets.current);
    }

    if (presets.names.isEmpty())
        menu.addSectionHeader ("No presets yet: Save as makes one");
}

void PresetMenus::addActions (juce::PopupMenu& menu, const PresetMenu& presets, int firstId)
{
    const auto id = [firstId] (PresetAction action) { return firstId + (int) action; };
    const auto chosen = juce::isPositiveAndBelow (presets.current, presets.names.size()) ? presets.names[presets.current] : juce::String();

    menu.addItem (id (PresetAction::save), chosen.isNotEmpty() && presets.canSave ? "Save \"" + chosen + "\"" : juce::String ("Save"), presets.canSave);
    menu.addItem (id (PresetAction::saveAs), juce::String::fromUTF8 ("Save as\xe2\x80\xa6"));

    if (presets.canSaveFactory)
        menu.addItem (id (PresetAction::saveFactory), juce::String::fromUTF8 ("Save as a factory preset\xe2\x80\xa6"));

    menu.addItem (id (PresetAction::loadFile), juce::String::fromUTF8 ("Load a preset file\xe2\x80\xa6"));
    menu.addSeparator();
    menu.addItem (id (PresetAction::saveBank), juce::String::fromUTF8 ("Save bank\xe2\x80\xa6"), ! presets.names.isEmpty());
    menu.addItem (id (PresetAction::loadBank), juce::String::fromUTF8 ("Load bank\xe2\x80\xa6"));
    menu.addSeparator();
    menu.addItem (id (PresetAction::remove), chosen.isNotEmpty() ? juce::String::fromUTF8 ("Delete \"") + chosen + juce::String::fromUTF8 ("\"\xe2\x80\xa6")
                                                                 : juce::String ("Delete"),
                  presets.canDelete);
    menu.addItem (id (PresetAction::showFolder), "Show the presets folder");
}

//==============================================================================
juce::File PresetLibrary::root()
{
    return UserData::getFolder().getChildFile ("Presets");
}

void PresetLibrary::setPlugin (const juce::String& name, const juce::File& factoryPresets, bool editable)
{
    pluginName = name.trim();
    factoryFile = factoryPresets;
    factoryEditable = editable;
    current = -1;
    modified = false;
    read();
}

juce::File PresetLibrary::getFolder() const
{
    return pluginName.isNotEmpty() ? root().getChildFile (legalName (pluginName)) : juce::File();
}

void PresetLibrary::refresh()
{
    const auto* chosen = getCurrentEntry();
    const auto name = chosen != nullptr ? chosen->preset.name : juce::String();
    const auto group = chosen != nullptr ? chosen->group : juce::String();

    read();

    current = name.isNotEmpty() ? find (name, group) : -1;

    if (current < 0)
        modified = false;
}

void PresetLibrary::read()
{
    entries.clear();

    if (pluginName.isEmpty())
        return;

    // The plugin's own first.
    if (factoryFile.existsAsFile())
    {
        PresetBank bank;
        const auto json = juce::JSON::parse (factoryFile);   // kept: the list lives in it

        if (const auto* list = json.getProperty ("presets", {}).getArray())
            for (const auto& item : *list)
                if (auto preset = PresetBank::fromVar (item); preset.name.isNotEmpty())
                    bank.put (preset);

        for (const auto& preset : bank.presets)
            entries.push_back ({ preset, factoryGroup, {} });
    }

    // Then the user's: My presets, and each bank's folder.
    const auto folder = getFolder();

    if (! folder.isDirectory())
        return;

    const auto readFolder = [this] (const juce::File& dir, const juce::String& group)
    {
        auto files = dir.findChildFiles (juce::File::findFiles, false, juce::String ("*") + presetExtension);
        std::sort (files.begin(), files.end(), byName);

        for (const auto& file : files)
        {
            const auto json = juce::JSON::parse (file);

            if (! json.isObject())
                continue;

            auto preset = presetFrom (json, file.getFileNameWithoutExtension());

            if (preset.name.isNotEmpty() && find (preset.name, group) < 0)
                entries.push_back ({ preset, group, file });
        }
    };

    readFolder (folder, userGroup);

    auto banks = folder.findChildFiles (juce::File::findDirectories, false);
    std::sort (banks.begin(), banks.end(), byName);

    for (const auto& bank : banks)
        readFolder (bank, bank.getFileName());
}

int PresetLibrary::find (const juce::String& name, const juce::String& group) const
{
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].preset.name.equalsIgnoreCase (name) && (group.isEmpty() || entries[i].group == group))
            return (int) i;

    return -1;
}

const PresetLibrary::Entry* PresetLibrary::getCurrentEntry() const
{
    return juce::isPositiveAndBelow (current, (int) entries.size()) ? &entries[(size_t) current] : nullptr;
}

void PresetLibrary::choose (int index)
{
    current = juce::isPositiveAndBelow (index, (int) entries.size()) ? index : -1;
    modified = false;
}

bool PresetLibrary::restore (const juce::String& name, const juce::String& group, bool wasModified)
{
    current = name.isNotEmpty() ? find (name, group) : -1;

    // A bank's preset whose group was renamed: the name alone.
    if (current < 0 && name.isNotEmpty() && group.isNotEmpty())
        current = find (name, {});

    modified = current >= 0 && wasModified;
    return current >= 0;
}

PresetMenu PresetLibrary::getMenu() const
{
    PresetMenu menu;

    for (const auto& entry : entries)
    {
        menu.names.add (entry.preset.name);
        menu.groups.add (entry.group);
    }

    menu.current = current;
    menu.modified = modified;

    const auto* chosen = getCurrentEntry();
    menu.canSave = chosen != nullptr && (! chosen->isFactory() || factoryEditable);
    menu.canDelete = chosen != nullptr && (! chosen->isFactory() || factoryEditable);
    menu.canSaveFactory = factoryEditable && factoryFile != juce::File();
    return menu;
}

bool PresetLibrary::hasPreset (const juce::String& name, bool factory) const
{
    return find (cleanName (name), factory ? factoryGroup : userGroup) >= 0;
}

juce::String PresetLibrary::suggestedBankName() const
{
    return (pluginName.isNotEmpty() ? pluginName : juce::String ("My")) + " presets";
}

//==============================================================================
juce::Result PresetLibrary::writePresetFile (const juce::File& file, const Preset& preset) const
{
    auto json = PresetBank::toVar (preset);
    auto* fields = json.getDynamicObject();
    fields->setProperty ("format", presetFormat);
    fields->setProperty ("formatVersion", 1);
    fields->setProperty ("plugin", pluginName);

    if (! file.getParentDirectory().createDirectory())
        return juce::Result::fail ("Couldn't make the folder " + file.getParentDirectory().getFullPathName());

    if (! file.replaceWithText (juce::JSON::toString (json)))
        return juce::Result::fail ("Couldn't write " + file.getFullPathName());

    return juce::Result::ok();
}

juce::Result PresetLibrary::save (const std::map<juce::String, float>& values)
{
    const auto* chosen = getCurrentEntry();

    if (chosen == nullptr)
        return juce::Result::fail ("No preset is chosen: Save as makes a new one.");

    if (chosen->isFactory())
    {
        if (! factoryEditable)
            return juce::Result::fail ("Factory presets come with the plugin: Save as keeps your version as a preset of your own.");

        return saveFactory (chosen->preset.name, values);
    }

    auto preset = chosen->preset;
    preset.values = values;

    if (const auto written = writePresetFile (chosen->file, preset); written.failed())
        return written;

    const auto name = preset.name;
    const auto group = chosen->group;
    read();
    current = find (name, group);
    modified = false;
    return juce::Result::ok();
}

juce::Result PresetLibrary::saveAs (const juce::String& name, const std::map<juce::String, float>& values)
{
    const auto clean = cleanName (name);

    if (clean.isEmpty())
        return juce::Result::fail ("The preset needs a name.");

    if (pluginName.isEmpty())
        return juce::Result::fail ("There's no plugin to save presets for.");

    Preset preset;
    preset.name = clean;
    preset.values = values;

    // Over the one of the same name, if it's there; otherwise a file named after it.
    auto file = getFolder().getChildFile (legalName (clean) + presetExtension);

    if (const auto existing = find (clean, userGroup); existing >= 0)
        file = entries[(size_t) existing].file;
    else
        file = file.getNonexistentSibling (false);

    if (const auto written = writePresetFile (file, preset); written.failed())
        return written;

    read();
    current = find (clean, userGroup);
    modified = false;
    return juce::Result::ok();
}

juce::Result PresetLibrary::saveFactory (const juce::String& name, const std::map<juce::String, float>& values)
{
    const auto clean = cleanName (name);

    if (! factoryEditable || factoryFile == juce::File())
        return juce::Result::fail ("Factory presets are made in the Plugins Creator Studio.");

    if (clean.isEmpty())
        return juce::Result::fail ("The preset needs a name.");

    // An unreadable presets.json isn't written over: the presets in it would be lost.
    PresetBank bank;

    if (const auto loaded = bank.load (factoryFile.getParentDirectory()); loaded.failed())
        return juce::Result::fail (loaded.getErrorMessage() + " (" + factoryFile.getFullPathName() + "), so the preset wasn't saved: "
                                   "saving now would lose the presets in it. Fix or move that file, then save again.");

    Preset preset;
    preset.name = clean;
    preset.values = values;
    bank.put (preset);

    if (const auto saved = bank.save (factoryFile.getParentDirectory()); saved.failed())
        return saved;

    read();
    current = find (clean, factoryGroup);
    modified = false;
    return juce::Result::ok();
}

juce::Result PresetLibrary::remove (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) entries.size()))
        return juce::Result::fail ("That preset isn't there.");

    const auto entry = entries[(size_t) index];

    if (entry.isFactory())
    {
        if (! factoryEditable)
            return juce::Result::fail ("Factory presets come with the plugin: they can't be deleted here.");

        PresetBank bank;

        if (const auto loaded = bank.load (factoryFile.getParentDirectory()); loaded.failed())
            return juce::Result::fail (loaded.getErrorMessage() + " (" + factoryFile.getFullPathName() + "), so nothing was deleted: "
                                       "saving now would lose the presets in it. Fix or move that file, then try again.");

        if (const auto at = bank.indexOf (entry.preset.name); at >= 0)
            bank.presets.erase (bank.presets.begin() + at);

        if (const auto saved = bank.save (factoryFile.getParentDirectory()); saved.failed())
            return saved;
    }
    else if (! entry.file.moveToTrash() && ! entry.file.deleteFile())
    {
        return juce::Result::fail ("Couldn't delete " + entry.file.getFullPathName());
    }

    // A bank's folder with nothing left in it goes too.
    if (! entry.isFactory() && entry.group != userGroup)
        if (const auto dir = entry.file.getParentDirectory(); dir.isDirectory() && dir.getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 0)
            dir.deleteRecursively();

    const auto* chosen = getCurrentEntry();
    const auto keepName = chosen != nullptr && index != current ? chosen->preset.name : juce::String();
    const auto keepGroup = chosen != nullptr ? chosen->group : juce::String();

    read();
    current = keepName.isNotEmpty() ? find (keepName, keepGroup) : -1;

    if (current < 0)
        modified = false;

    return juce::Result::ok();
}

juce::Result PresetLibrary::importPreset (const juce::File& file)
{
    const auto json = juce::JSON::parse (file);

    if (! json.isObject() || ! json.getProperty ("values", {}).isObject())
        return juce::Result::fail (file.getFileName() + " isn't a Sonify preset.");

    const auto preset = presetFrom (json, file.getFileNameWithoutExtension());
    return saveAs (preset.name, preset.values);
}

juce::Result PresetLibrary::exportBank (const juce::File& file) const
{
    if (entries.empty())
        return juce::Result::fail ("There are no presets to put in a bank.");

    juce::Array<juce::var> list;

    for (const auto& entry : entries)
    {
        auto item = PresetBank::toVar (entry.preset);
        item.getDynamicObject()->setProperty ("group", entry.group);
        list.add (item);
    }

    juce::DynamicObject::Ptr bank (new juce::DynamicObject());
    bank->setProperty ("format", bankFormat);
    bank->setProperty ("formatVersion", 1);
    bank->setProperty ("plugin", pluginName);
    bank->setProperty ("name", file.getFileNameWithoutExtension());
    bank->setProperty ("presets", list);

    if (! file.replaceWithText (juce::JSON::toString (juce::var (bank.get()))))
        return juce::Result::fail ("Couldn't write " + file.getFullPathName());

    return juce::Result::ok();
}

juce::Result PresetLibrary::importBank (const juce::File& file)
{
    const auto json = juce::JSON::parse (file);
    const auto* list = json.getProperty ("presets", {}).getArray();

    if (! json.isObject() || list == nullptr)
        return juce::Result::fail (file.getFileName() + " isn't a Sonify preset bank.");

    if (pluginName.isEmpty())
        return juce::Result::fail ("There's no plugin to load presets for.");

    auto bankName = cleanName (json.getProperty ("name", {}).toString());

    if (bankName.isEmpty())
        bankName = cleanName (file.getFileNameWithoutExtension());

    // Not over the user's own or the plugin's: a bank keeps to a folder of its own.
    if (bankName.isEmpty() || bankName.equalsIgnoreCase (userGroup) || bankName.equalsIgnoreCase (factoryGroup))
        bankName = "Bank " + bankName;

    const auto dir = getFolder().getChildFile (legalName (bankName));
    int count = 0;

    for (const auto& item : *list)
    {
        if (count >= maxBankPresets)
            break;

        const auto preset = presetFrom (item, {});

        if (preset.name.isEmpty())
            continue;

        if (const auto written = writePresetFile (dir.getChildFile (legalName (preset.name) + presetExtension), preset); written.failed())
            return written;

        ++count;
    }

    if (count == 0)
        return juce::Result::fail (file.getFileName() + " has no presets in it.");

    // Listed by name, as every folder of presets is: the first of the bank's is chosen.
    read();
    current = -1;

    for (size_t i = 0; i < entries.size() && current < 0; ++i)
        if (entries[i].group == dir.getFileName())
            current = (int) i;

    modified = false;
    return juce::Result::ok();
}

//==============================================================================
PresetUi::PresetUi (juce::Component& dialogParent, PresetLibrary& presets)
    : parent (dialogParent), library (presets)
{
}

PresetUi::~PresetUi()
{
    *alive = false;
}

void PresetUi::choose (int index)
{
    library.choose (index);
    chooseCurrentAndApply();
}

void PresetUi::chooseCurrentAndApply()
{
    if (const auto* entry = library.getCurrentEntry(); entry != nullptr && apply != nullptr)
        apply (entry->preset);

    if (changed != nullptr)
        changed();
}

void PresetUi::report (const juce::Result& result, const juce::String& what)
{
    if (result.failed())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, what, result.getErrorMessage(), "OK", &parent);
}

void PresetUi::perform (PresetAction action)
{
    if (! library.hasPlugin())
        return;

    const auto values = [this] { return getValues != nullptr ? getValues() : std::map<juce::String, float>(); };
    const auto* chosen = library.getCurrentEntry();
    const auto chosenName = chosen != nullptr ? chosen->preset.name : juce::String();

    switch (action)
    {
        case PresetAction::save:
        {
            if (chosen == nullptr || (chosen->isFactory() && ! library.getMenu().canSave))
            {
                perform (PresetAction::saveAs);
                return;
            }

            report (library.save (values()), "Couldn't save the preset");
            break;
        }

        case PresetAction::saveAs:
        case PresetAction::saveFactory:
        {
            const bool factory = action == PresetAction::saveFactory;

            askName (factory ? "Save as a factory preset" : "Save preset",
                     factory ? "A name for the current values. Factory presets come with the plugin when it's added to Sonify."
                             : "A name for the current values. Your presets are kept on this computer, for this plugin wherever it plays.",
                     chosenName.isNotEmpty() ? chosenName : juce::String ("My preset"),
                     factory,
                     [this, factory, values] (const juce::String& name)
                     {
                         report (factory ? library.saveFactory (name, values()) : library.saveAs (name, values()), "Couldn't save the preset");

                         if (changed != nullptr)
                             changed();
                     });
            return;
        }

        case PresetAction::loadFile:
        {
            chooseFile (false, false, [this] (const juce::File& file)
            {
                const auto result = library.importPreset (file);
                report (result, "Couldn't load the preset");

                if (result.wasOk())
                    chooseCurrentAndApply();
            });
            return;
        }

        case PresetAction::saveBank:
        {
            chooseFile (true, true, [this] (const juce::File& file)
            {
                report (library.exportBank (file.withFileExtension (PresetLibrary::bankExtension)), "Couldn't save the bank");
            });
            return;
        }

        case PresetAction::loadBank:
        {
            chooseFile (false, true, [this] (const juce::File& file)
            {
                const auto result = library.importBank (file);
                report (result, "Couldn't load the bank");

                if (result.wasOk())
                    chooseCurrentAndApply();
            });
            return;
        }

        case PresetAction::remove:
        {
            if (chosen == nullptr || ! library.getMenu().canDelete)
                return;

            const auto index = library.getCurrent();
            confirm ("Delete the preset?",
                     chosen->isFactory() ? "\"" + chosenName + "\" is taken out of the plugin's factory presets."
                                         : "\"" + chosenName + "\" goes to the Recycle Bin.",
                     "Delete",
                     [this, index, chosenName]
                     {
                         // Still the same one (the list may have changed meanwhile).
                         const auto* now = library.getCurrentEntry();

                         if (now == nullptr || library.getCurrent() != index || now->preset.name != chosenName)
                             return;

                         report (library.remove (index), "Couldn't delete the preset");

                         if (changed != nullptr)
                             changed();
                     });
            return;
        }

        case PresetAction::showFolder:
        {
            const auto folder = library.getFolder();
            folder.createDirectory();
            folder.revealToUser();
            return;
        }
    }

    if (changed != nullptr)
        changed();
}

void PresetUi::askName (const juce::String& title, const juce::String& message, const juce::String& suggested, bool factory,
                        std::function<void (const juce::String&)> then)
{
    auto* dialog = new juce::AlertWindow (title, message, juce::MessageBoxIconType::NoIcon, &parent);
    dialog->addTextEditor ("name", suggested);
    dialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<juce::AlertWindow> safeDialog (dialog);
    std::weak_ptr<bool> weak = alive;

    dialog->enterModalState (true, juce::ModalCallbackFunction::create ([this, weak, safeDialog, factory, then] (int result)
    {
        if (weak.expired() || safeDialog == nullptr || result != 1)
            return;

        const auto name = safeDialog->getTextEditorContents ("name").trim();

        if (name.isEmpty())
            return;

        // The same name: replaced only once the user says so.
        if (! library.hasPreset (name, factory))
        {
            then (name);
            return;
        }

        juce::MessageManager::callAsync ([this, weak, name, then]
        {
            if (! weak.expired())
                confirm ("Replace the preset?", "There's a preset called \"" + name + "\" already. Replace it with the current values?",
                         "Replace", [name, then] { then (name); });
        });
    }), true);

    juce::MessageManager::callAsync ([safeDialog]
    {
        if (safeDialog != nullptr)
            if (auto* editor = safeDialog->getTextEditor ("name"))
            {
                editor->grabKeyboardFocus();
                editor->selectAll();
            }
    });
}

void PresetUi::confirm (const juce::String& title, const juce::String& message, const juce::String& yes, std::function<void()> then)
{
    auto* dialog = new juce::AlertWindow (title, message, juce::MessageBoxIconType::QuestionIcon, &parent);
    dialog->addButton (yes, 1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    std::weak_ptr<bool> weak = alive;

    dialog->enterModalState (true, juce::ModalCallbackFunction::create ([weak, then] (int result)
    {
        if (! weak.expired() && result == 1)
            then();
    }), true);
}

void PresetUi::chooseFile (bool saving, bool bank, std::function<void (const juce::File&)> then)
{
    const auto folder = library.getFolder();
    const auto start = folder.isDirectory() ? folder : PresetLibrary::root().isDirectory() ? PresetLibrary::root()
                                                     : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    const auto extension = juce::String (bank ? PresetLibrary::bankExtension : PresetLibrary::presetExtension);

    chooser = std::make_unique<juce::FileChooser> (saving ? "Save the presets in a bank" : bank ? "Load a preset bank" : "Load a preset",
                                                   saving ? start.getChildFile (juce::File::createLegalFileName (library.suggestedBankName()) + extension)
                                                          : start,
                                                   "*" + extension);

    const auto flags = saving ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                    | juce::FileBrowserComponent::warnAboutOverwriting
                              : juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    std::weak_ptr<bool> weak = alive;

    chooser->launchAsync (flags, [weak, then] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();

        if (! weak.expired() && file != juce::File())
            then (file);
    });
}
