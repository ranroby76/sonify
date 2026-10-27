// D:\workspace\Fanan_web_hub\src\PluginHeader.cpp

#include "PluginHeader.h"
#include "Plugin.h"
#include "Preferences.h"
#include "SonifyPresets.h"
#include "Vst3Plugin.h"

#include <utility>
#include <vector>

namespace
{
    std::vector<std::pair<int, PluginHeader::Switcher>>& switchers()
    {
        static std::vector<std::pair<int, PluginHeader::Switcher>> list;
        return list;
    }

    int nextSwitcherId = 0;

    const juce::Colour colTop     { 0xff2a2c31 };
    const juce::Colour colBottom  { 0xff1e2024 };
    const juce::Colour colLine    { 0xff111214 };
    const juce::Colour colName    { 0xffdcdfe4 };
    const juce::Colour colMuted   { 0xff8b9098 };
    const juce::Colour colOn      { 0xff3ddc84 };
    const juce::Colour colOnBack  { 0xff1d3a29 };
    const juce::Colour colOffBack { 0xff34363c };

    constexpr const char* vst3PresetExtension = ".vstpreset";
    constexpr size_t maxPresetBytes = 64 * 1024 * 1024;

    Vst3Plugin* vst3Of (Plugin& plugin)
    {
        auto* vst3 = dynamic_cast<Vst3Plugin*> (&plugin);
        return vst3 != nullptr && plugin.getFormatName() == "VST3" ? vst3 : nullptr;
    }

    /** What a format is called on the strip. */
    juce::String formatLabel (const juce::String& format)
    {
        if (format.equalsIgnoreCase ("Sonify"))
            return "SONIFY";

        return format.toUpperCase();
    }
}

//==============================================================================
/** The ON/OFF: a pill with its light. */
class PluginHeader::Switch final : public juce::Button
{
public:
    Switch() : juce::Button ("ON/OFF")
    {
        setClickingTogglesState (false);
        setWantsKeyboardFocus (false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip ("On: click to switch it off (bypassed)");
        setTitle ("On");
    }

    void setOn (bool shouldBeOn)
    {
        if (on == shouldBeOn)
            return;

        on = shouldBeOn;
        setTooltip (on ? "On: click to switch it off (bypassed)" : "Off (bypassed): click to switch it on");
        setTitle (on ? "On" : "Off");
        repaint();
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        auto area = getLocalBounds().toFloat().reduced (0.5f, 3.5f);
        auto back = on ? colOnBack : colOffBack;

        if (highlighted)
            back = back.brighter (0.18f);

        if (down)
            back = back.darker (0.15f);

        const auto radius = area.getHeight() * 0.5f;
        g.setColour (back);
        g.fillRoundedRectangle (area, radius);
        g.setColour ((on ? colOn : colMuted).withAlpha (0.45f));
        g.drawRoundedRectangle (area, radius, 1.0f);

        // The light: lit and glowing on, dark off.
        const auto led = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ area.getX() + radius + 1.0f, area.getCentreY() });

        if (on)
        {
            g.setColour (colOn.withAlpha (0.28f));
            g.fillEllipse (led.expanded (2.5f));
        }

        g.setColour (on ? colOn : juce::Colour (0xff5b5e66));
        g.fillEllipse (led);

        g.setColour (on ? juce::Colour (0xffe9f7ee) : colMuted);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (on ? "ON" : "OFF", area.withTrimmedLeft (radius * 2.0f + 1.0f).withTrimmedRight (5.0f),
                    juce::Justification::centred, false);
    }

private:
    bool on = true;
};

//==============================================================================
void PluginHeader::attach (juce::DocumentWindow& window, Plugin& plugin)
{
    auto* header = new PluginHeader (plugin);
    window.setMenuBarComponent (header);     // the window owns it from here
    window.setConstrainer (&header->getConstrainer());
}

int PluginHeader::addSwitcher (Switcher switcher)
{
    const auto id = ++nextSwitcherId;
    switchers().emplace_back (id, std::move (switcher));
    return id;
}

void PluginHeader::removeSwitcher (int id)
{
    auto& list = switchers();

    for (auto it = list.begin(); it != list.end(); ++it)
    {
        if (it->first == id)
        {
            list.erase (it);
            return;
        }
    }
}

int PluginHeader::stateOf (const Plugin& plugin)
{
    for (const auto& entry : switchers())
        if (entry.second.state != nullptr)
            if (const auto s = entry.second.state (plugin); s >= 0)
                return s > 0 ? 1 : 0;

    return -1;
}

//==============================================================================
void PluginHeader::Constrainer::checkBounds (juce::Rectangle<int>& bounds, const juce::Rectangle<int>& previousBounds,
                                             const juce::Rectangle<int>& limits, bool isStretchingTop, bool isStretchingLeft,
                                             bool isStretchingBottom, bool isStretchingRight)
{
    // The content's rectangle (the header is at the top: the content's bottom is the window's).
    auto content = bounds.withHeight (juce::jmax (0, bounds.getHeight() - PluginHeader::height));
    const auto before = previousBounds.withHeight (juce::jmax (0, previousBounds.getHeight() - PluginHeader::height));

    juce::ComponentBoundsConstrainer::checkBounds (content, before, limits, isStretchingTop, isStretchingLeft,
                                                   isStretchingBottom, isStretchingRight);

    bounds = content.withHeight (content.getHeight() + PluginHeader::height);
}

//==============================================================================
PluginHeader::PluginHeader (Plugin& p)
    : plugin (p),
      button (std::make_unique<Switch>())
{
    setOpaque (true);
    setSize (400, height);

    name = plugin.getDescriptor().name;
    format = formatLabel (plugin.getFormatName());

    button->onClick = [this] { toggle(); };
    addChildComponent (*button);

    presetsButton.setButtonText (juce::String::fromUTF8 ("PRESETS \xe2\x96\xbe"));
    presetsButton.setTooltip ("Save or open a preset file: Sonify's own (any plugin), or a VST3 preset (.vstpreset)");
    presetsButton.setWantsKeyboardFocus (false);
    presetsButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    presetsButton.setColour (juce::TextButton::buttonColourId, colOffBack);
    presetsButton.setColour (juce::TextButton::buttonOnColourId, colOffBack.brighter (0.2f));
    presetsButton.setColour (juce::TextButton::textColourOffId, colName);
    presetsButton.setColour (juce::ComboBox::outlineColourId, colMuted.withAlpha (0.45f));
    presetsButton.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (presetsButton);

    refresh();
    startTimerHz (4);
}

PluginHeader::~PluginHeader()
{
    stopTimer();
}

void PluginHeader::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.setGradientFill (juce::ColourGradient (colTop, 0.0f, 0.0f, colBottom, 0.0f, (float) bounds.getHeight(), false));
    g.fillRect (bounds);
    g.setColour (colLine);
    g.fillRect (bounds.removeFromBottom (1));

    auto area = getLocalBounds().reduced (10, 0);
    area.setLeft (presetsButton.getRight() + 10);

    // The format, boxed, at the right; the name in what's left.
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    const auto formatWidth = format.isEmpty() ? 0 : juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), format) + 12;

    if (formatWidth > 0 && formatWidth < area.getWidth() / 2)
    {
        const auto box = area.removeFromRight (formatWidth).toFloat().reduced (0.5f, 5.5f);
        g.setColour (colMuted.withAlpha (0.55f));
        g.drawRoundedRectangle (box, 3.0f, 1.0f);
        g.setColour (colMuted);
        g.drawText (format, box, juce::Justification::centred, false);
        area.removeFromRight (8);
    }

    g.setColour (state == 0 ? colMuted : colName);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawFittedText (name, area, juce::Justification::centredLeft, 1, 0.9f);
}

void PluginHeader::resized()
{
    button->setBounds (8, 0, 52, getHeight());
    const auto left = button->isVisible() ? button->getRight() + 6 : 8;
    presetsButton.setBounds (left, 4, 74, getHeight() - 8);
}

void PluginHeader::enablementChanged()
{
    // DocumentWindow greys its menu bar while it isn't the active window; this strip stays live,
    // so one click on ON/OFF does it, whichever window was in front.
    if (! isEnabled())
        setEnabled (true);
}

void PluginHeader::timerCallback()
{
    refresh();
}

void PluginHeader::refresh()
{
    const auto now = stateOf (plugin);

    if (now == state)
        return;

    state = now;
    button->setVisible (state >= 0);
    button->setOn (state != 0);
    resized();
    repaint();
}

//==============================================================================
juce::File PluginHeader::presetFolder() const
{
    return UserData::getFolder().getChildFile ("Presets").getChildFile (sonify::presets::legalName (plugin.getDescriptor().name));
}

void PluginHeader::tell (const juce::String& title, const juce::String& message)
{
    juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOk (juce::MessageBoxIconType::WarningIcon, title, message, {}, this),
                                  nullptr);
}

void PluginHeader::showPresetMenu()
{
    const bool canState = plugin.canSaveState();
    const bool isVst3 = vst3Of (plugin) != nullptr;

    juce::PopupMenu menu;
    menu.addSectionHeader ("Sonify preset (any plugin)");
    menu.addItem (1, juce::String::fromUTF8 ("Save Preset\xe2\x80\xa6"), canState);
    menu.addItem (2, juce::String::fromUTF8 ("Open Preset\xe2\x80\xa6"), canState);
    menu.addSeparator();
    menu.addSectionHeader ("VST3 preset (.vstpreset)");
    menu.addItem (3, juce::String::fromUTF8 ("Save as VST3 Preset\xe2\x80\xa6"), isVst3);
    menu.addItem (4, juce::String::fromUTF8 ("Open VST3 Preset\xe2\x80\xa6"), isVst3);
    menu.addSeparator();
    menu.addItem (5, "Show Presets Folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetsButton),
                        [safe = juce::Component::SafePointer<PluginHeader> (this)] (int choice)
    {
        if (safe == nullptr)
            return;

        if (choice == 1)       safe->savePreset (false);
        else if (choice == 2)  safe->openPreset (false);
        else if (choice == 3)  safe->savePreset (true);
        else if (choice == 4)  safe->openPreset (true);
        else if (choice == 5)
        {
            const auto folder = safe->presetFolder();
            folder.createDirectory();
            folder.revealToUser();
        }
    });
}

void PluginHeader::savePreset (bool vst3)
{
    const auto folder = presetFolder();
    folder.createDirectory();

    const auto extension = vst3 ? juce::String (vst3PresetExtension) : juce::String (sonify::presets::extension);
    const auto suggested = folder.getChildFile (sonify::presets::legalName (plugin.getDescriptor().name + " preset") + extension);

    chooser = std::make_unique<juce::FileChooser> (vst3 ? "Save as VST3 preset" : "Save preset", suggested, "*" + extension);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe = juce::Component::SafePointer<PluginHeader> (this), vst3, extension] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == juce::File())
            return;

        const auto file = fc.getResult().withFileExtension (extension);
        auto& plugin = safe->plugin;

        if (vst3)
        {
            auto* host = vst3Of (plugin);
            const auto data = host != nullptr ? host->getVst3Preset() : juce::MemoryBlock();

            if (data.isEmpty())
                return safe->tell ("Couldn't save the VST3 preset", plugin.getDescriptor().name + " didn't give its settings as a VST3 preset.");

            if (! file.replaceWithData (data.getData(), data.getSize()))
                safe->tell ("Couldn't save the VST3 preset", "Couldn't write " + file.getFullPathName());

            return;
        }

        juce::MemoryBlock state;

        if (! plugin.saveState (state) || state.isEmpty())
            return safe->tell ("Couldn't save the preset", plugin.getDescriptor().name + " didn't give its settings.");

        juce::DynamicObject::Ptr extras (new juce::DynamicObject());
        extras->setProperty ("pluginId", plugin.getDescriptor().id);
        extras->setProperty ("pluginFormat", plugin.getFormatName());
        extras->setProperty ("vendor", plugin.getDescriptor().vendor);
        extras->setProperty ("state", juce::Base64::toBase64 (state.getData(), state.getSize()));

        const auto saved = sonify::presets::save (file, plugin.getDescriptor().name, file.getFileNameWithoutExtension(), {},
                                                  juce::var (extras.get()));

        if (saved.failed())
            safe->tell ("Couldn't save the preset", saved.getErrorMessage());
    });
}

void PluginHeader::openPreset (bool vst3)
{
    const auto folder = presetFolder();
    const auto extension = vst3 ? juce::String (vst3PresetExtension) : juce::String (sonify::presets::extension);

    chooser = std::make_unique<juce::FileChooser> (vst3 ? "Open a VST3 preset" : "Open a preset",
                                                   folder.isDirectory() ? folder : UserData::getFolder(), "*" + extension);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<PluginHeader> (this), vst3] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || ! fc.getResult().existsAsFile())
            return;

        const auto file = fc.getResult();
        auto& plugin = safe->plugin;
        const auto name = plugin.getDescriptor().name;

        if (file.getSize() > (juce::int64) maxPresetBytes)
            return safe->tell ("Couldn't open the preset", file.getFileName() + " is too big to be a preset.");

        if (vst3)
        {
            juce::MemoryBlock data;
            auto* host = vst3Of (plugin);

            if (host == nullptr || ! file.loadFileAsData (data) || data.getSize() < 48
                || std::memcmp (data.getData(), "VST3", 4) != 0)
                return safe->tell ("Couldn't open the VST3 preset", file.getFileName() + " isn't a VST3 preset.");

            if (! host->setVst3Preset (data))
                safe->tell ("Couldn't open the VST3 preset", file.getFileName() + " isn't one of " + name + "'s presets.");

            return;
        }

        const auto json = juce::JSON::parse (file.loadFileAsString());

        if (json.getProperty ("format", {}).toString() != sonify::presets::formatName)
            return safe->tell ("Couldn't open the preset", file.getFileName() + " isn't a Sonify preset.");

        const auto stateText = json.getProperty ("state", {}).toString();

        if (stateText.isEmpty())
            return safe->tell ("Open it on its panel", file.getFileName() + " is one of " + json.getProperty ("plugin", name).toString()
                                                       + "'s own presets: choose it in the preset manager on its panel.");

        if (json.getProperty ("pluginId", {}).toString() != plugin.getDescriptor().id)
            return safe->tell ("Couldn't open the preset", "This preset is for " + json.getProperty ("plugin", "another plugin").toString()
                                                           + ", not " + name + ".");

        juce::MemoryOutputStream decoded;

        if (! juce::Base64::convertFromBase64 (decoded, stateText) || decoded.getDataSize() == 0)
            return safe->tell ("Couldn't open the preset", file.getFileName() + " is damaged.");

        if (! plugin.loadState (decoded.getMemoryBlock()))
            safe->tell ("Couldn't open the preset", name + " didn't take the preset's settings.");
    });
}

void PluginHeader::toggle()
{
    const auto now = stateOf (plugin);

    if (now < 0)
        return;

    for (const auto& entry : switchers())
    {
        if (entry.second.state != nullptr && entry.second.state (plugin) >= 0)
        {
            if (entry.second.set != nullptr)
                entry.second.set (plugin, now == 0);

            break;
        }
    }

    refresh();
}
