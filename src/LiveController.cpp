// D:\workspace\Fanan_web_hub\src\LiveController.cpp

#include "LiveController.h"
#include "Preferences.h"

#include <algorithm>
#include <cmath>

namespace LiveControl
{
/** Windows leaves the window out of every screen capture (elsewhere: nothing to do). */
void hideFromCaptures (juce::Component& window);

namespace
{
    // Its colours (the Live Show's).
    const juce::Colour ground { 0xff141519 };
    const juce::Colour panelColour { 0xff1b1c20 };
    const juce::Colour widget { 0xff23252a };
    const juce::Colour line { 0xff33363d };
    const juce::Colour ink { 0xffe8eaed };
    const juce::Colour muted { 0xff9aa0a8 };
    const juce::Colour red { 0xffff4d57 };
    const juce::Colour amber { 0xffffcf45 };
    const juce::Colour green { 0xff46c27a };
    const juce::Colour blue { 0xff3fa9f5 };
    const juce::Colour grey { 0xff8a8f98 };
    const juce::Colour learnColour { 0xffffd84a };

    constexpr int formatVersion = 1;
    constexpr float smallestBox = 0.06f;            // a Custom box at its smallest (of the picture's width or height)
    constexpr float boxMargin = 0.03f;              // where boxes snap, in from the picture's edges
    constexpr float snapReach = 0.012f;

    juce::var boxToVar (juce::Rectangle<float> box)
    {
        const auto round = [] (float v) { return std::round (v * 10000.0f) / 10000.0; };
        return juce::Array<juce::var> { round (box.getX()), round (box.getY()), round (box.getWidth()), round (box.getHeight()) };
    }

    /** A box from a file, kept inside the picture and not too small (fallback when it isn't one). */
    juce::Rectangle<float> boxFromVar (const juce::var& value, juce::Rectangle<float> fallback)
    {
        const auto* list = value.getArray();

        if (list == nullptr || list->size() != 4)
            return fallback;

        auto w = juce::jlimit (smallestBox, 1.0f, static_cast<float> (static_cast<double> ((*list)[2])));
        auto h = juce::jlimit (smallestBox, 1.0f, static_cast<float> (static_cast<double> ((*list)[3])));
        auto x = juce::jlimit (0.0f, 1.0f - w, static_cast<float> (static_cast<double> ((*list)[0])));
        auto y = juce::jlimit (0.0f, 1.0f - h, static_cast<float> (static_cast<double> ((*list)[1])));

        if (! std::isfinite (x + y + w + h))
            return fallback;

        return { x, y, w, h };
    }

    /** Main's focus in the vertical picture from a file (0 .. 1; fallback when it isn't one). */
    float focusFromVar (const juce::var& value, float fallback)
    {
        if (! (value.isDouble() || value.isInt() || value.isInt64()))
            return fallback;

        const auto focus = static_cast<float> (static_cast<double> (value));
        return std::isfinite (focus) ? juce::jlimit (0.0f, 1.0f, focus) : fallback;
    }

    /** One row Main at most: the first that says so keeps it. */
    template <typename Rows>
    void oneMain (Rows& rows)
    {
        bool found = false;

        for (auto& row : rows)
        {
            if (row.show != Show::main)
                continue;

            if (found)
                row.show = Show::hidden;

            found = true;
        }
    }

    /** "Camera 1 - Main", "Preset - Intro", "Enable streaming" (a dot between, as chips show it). */
    juce::String actionText (const juce::String& action)
    {
        const auto parts = juce::StringArray::fromTokens (action, ".", "");
        const juce::String dot (juce::CharPointer_UTF8 (" \xc2\xb7 "));

        if (parts.size() == 3 && parts[0] == "row")
        {
            const auto show = parts[2] == "main" ? "Main" : parts[2] == "custom" ? "Custom" : "Hidden";
            return rowName (parts[1].getIntValue() - 1) + dot + show;
        }

        if (action.startsWith ("preset."))
            return "Preset" + dot + action.fromFirstOccurrenceOf ("preset.", false, false);

        if (action == "show.monitor")       return "Monitor";
        if (action == "switch.stream")      return "Enable streaming";
        if (action == "switch.mics")        return "Mute mics";
        if (action == "switch.playback")    return "Mute playback";

        return action;
    }

    /** A MIDI port's name without what Windows puts round it ("MIDIIN2 (Pad)" and "MIDIOUT2 (Pad)"
        are one device). */
    juce::String deviceNameOf (const juce::String& port)
    {
        auto name = port.trim();

        for (const auto* prefix : { "MIDIIN", "MIDIOUT" })
        {
            if (name.startsWithIgnoreCase (prefix))
            {
                name = name.substring (juce::String (prefix).length()).trimStart();

                while (name.isNotEmpty() && juce::CharacterFunctions::isDigit (name[0]))
                    name = name.substring (1);

                name = name.trim();

                if (name.startsWithChar ('(') && name.endsWithChar (')'))
                    name = name.substring (1, name.length() - 1).trim();
            }
        }

        return name;
    }

}

//==============================================================================
juce::String rowName (int row)
{
    return row == screenRow ? juce::String ("Screen") : "Camera " + juce::String (row + 1);
}

juce::String showName (Show show)
{
    return show == Show::main ? "main" : show == Show::custom ? "custom" : "hidden";
}

Show showFromName (const juce::String& name)
{
    return name == "main" ? Show::main : name == "custom" ? Show::custom : Show::hidden;
}

juce::Rectangle<float> Setup::standardBox (int row)
{
    // Out of each other's way: the cameras in the corners (bottom ones first, the title and the
    // logo are at the top), the screen in the middle.
    constexpr float size = 0.3f;
    const auto farEdge = 1.0f - boxMargin - size;

    switch (row)
    {
        case 0:  return { farEdge, farEdge, size, size };
        case 1:  return { boxMargin, farEdge, size, size };
        case 2:  return { farEdge, 0.16f, size, size };
        case 3:  return { boxMargin, 0.16f, size, size };
        default: return { 0.35f, 0.35f, size, size };
    }
}

juce::Rectangle<float> Setup::standardTallBox (int row)
{
    // A camera's shape in the vertical picture (16:9, 42% of its width), clear of what TikTok's
    // app puts over it: two above its comments at the bottom, two under the title at the top; the
    // screen across the middle.
    constexpr float width = 0.42f;
    constexpr float height = width * (9.0f / 16.0f) * (9.0f / 16.0f);
    const auto farEdge = 1.0f - boxMargin - width;

    switch (row)
    {
        case 0:  return { farEdge, 0.62f, width, height };
        case 1:  return { boxMargin, 0.62f, width, height };
        case 2:  return { farEdge, 0.15f, width, height };
        case 3:  return { boxMargin, 0.15f, width, height };
        default: return { 0.04f, 0.36f, 0.92f, 0.92f * (9.0f / 16.0f) * (9.0f / 16.0f) };
    }
}

Setup Setup::standard()
{
    Setup made;

    for (int r = 0; r < numRows; ++r)
    {
        made.rows[static_cast<size_t> (r)].box = standardBox (r);
        made.rows[static_cast<size_t> (r)].tallBox = standardTallBox (r);
    }

    made.rows[0].device = "test";
    made.rows[0].show = Show::main;
    return made;
}

juce::var Setup::toVar() const
{
    juce::DynamicObject::Ptr object (new juce::DynamicObject());
    object->setProperty ("format", formatVersion);
    juce::Array<juce::var> list;

    for (const auto& row : rows)
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("device", row.device);
        item->setProperty ("show", showName (row.show));
        item->setProperty ("box", boxToVar (row.box));
        item->setProperty ("tallBox", boxToVar (row.tallBox));
        item->setProperty ("focus", std::round (row.tallFocus * 1000.0f) / 1000.0);
        item->setProperty ("delay", row.delayMs);
        list.add (juce::var (item.get()));
    }

    object->setProperty ("rows", list);
    object->setProperty ("view", viewTall ? "vertical" : "landscape");
    object->setProperty ("midiPort", midiPort);
    object->setProperty ("lights", lights);

    juce::DynamicObject::Ptr padList (new juce::DynamicObject());

    for (const auto& [action, source] : pads)
        padList->setProperty (action, source);

    object->setProperty ("pads", juce::var (padList.get()));
    object->setProperty ("preset", preset);

    if (! window.isEmpty())
        object->setProperty ("window", juce::Array<juce::var> { window.getX(), window.getY(), window.getWidth(), window.getHeight() });

    if (! monitor.isEmpty())
        object->setProperty ("monitor", juce::Array<juce::var> { monitor.getX(), monitor.getY(), monitor.getWidth(), monitor.getHeight() });

    object->setProperty ("monitorMode", monitorMode);
    return juce::var (object.get());
}

Setup Setup::fromVar (const juce::var& value)
{
    auto made = standard();

    if (! value.isObject())
        return made;

    if (const auto* list = value.getProperty ("rows", {}).getArray())
    {
        for (int r = 0; r < juce::jmin (numRows, list->size()); ++r)
        {
            const auto& item = (*list)[r];
            auto& row = made.rows[static_cast<size_t> (r)];
            row.device = item.getProperty ("device", {}).toString().trim();
            row.show = row.device.isEmpty() ? Show::hidden : showFromName (item.getProperty ("show", "hidden").toString());
            row.box = boxFromVar (item.getProperty ("box", {}), standardBox (r));
            row.tallBox = boxFromVar (item.getProperty ("tallBox", {}), standardTallBox (r));
            row.tallFocus = focusFromVar (item.getProperty ("focus", 0.5), 0.5f);
            row.delayMs = r == screenRow ? 0 : juce::jlimit (0, LiveStream::maxDelayMs, static_cast<int> (item.getProperty ("delay", 0)));
        }
    }

    oneMain (made.rows);
    made.viewTall = value.getProperty ("view", "landscape").toString() == "vertical";
    made.midiPort = value.getProperty ("midiPort", {}).toString();
    made.lights = static_cast<bool> (value.getProperty ("lights", true));

    if (auto* padList = value.getProperty ("pads", {}).getDynamicObject())
        for (const auto& pad : padList->getProperties())
            if (MidiLink::Source::fromKey (pad.value.toString()).has_value())
                made.pads[pad.name.toString()] = pad.value.toString();

    made.preset = value.getProperty ("preset", {}).toString();

    if (const auto* area = value.getProperty ("window", {}).getArray(); area != nullptr && area->size() == 4)
        made.window = { static_cast<int> ((*area)[0]), static_cast<int> ((*area)[1]), static_cast<int> ((*area)[2]), static_cast<int> ((*area)[3]) };

    if (const auto* area = value.getProperty ("monitor", {}).getArray(); area != nullptr && area->size() == 4)
        made.monitor = { static_cast<int> ((*area)[0]), static_cast<int> ((*area)[1]), static_cast<int> ((*area)[2]), static_cast<int> ((*area)[3]) };

    const auto mode = value.getProperty ("monitorMode", "normal").toString();
    made.monitorMode = mode == "min" || mode == "max" ? mode : juce::String ("normal");
    return made;
}

//==============================================================================
namespace
{
    /** Its lists' and buttons' words, the size of the rest of it. */
    class ControllerLook final : public juce::LookAndFeel_V4
    {
    public:
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
        {
            return juce::FontOptions (juce::jmin (13.0f, static_cast<float> (buttonHeight) * 0.55f));
        }

        juce::Font getComboBoxFont (juce::ComboBox& box) override
        {
            return juce::FontOptions (juce::jmin (13.0f, static_cast<float> (box.getHeight()) * 0.6f));
        }

        juce::Font getPopupMenuFont() override
        {
            return juce::FontOptions (13.5f);
        }
    };

    /** A button of the Controller's: a pill that's lit while its thing is on, with its pad's name
        in a chip at its corner, and, while Learn is on, a dashed edge (solid and blinking while
        it waits for its pad). */
    class PadButton final : public juce::Button
    {
    public:
        PadButton (juce::String actionKey, const juce::String& label, juce::Colour accent)
            : juce::Button (label), action (std::move (actionKey)), colour (accent)
        {
            setWantsKeyboardFocus (false);
        }

        juce::String action;
        juce::Colour colour;
        bool lit = false;
        bool lamp = false;                  // a dot by its words (blinking while lit, for Enable streaming)
        juce::String pad;                   // its pad's name ("C3", "CC 20")
        bool learnMode = false, armed = false, blinkOn = true;
        std::function<void (bool rightClick)> onPress;

        /** Its look from the Controller: true when it changed. */
        bool update (bool nowLit, const juce::String& nowPad, bool nowLearning, bool nowArmed, bool nowBlink)
        {
            if (nowLit == lit && nowPad == pad && nowLearning == learnMode && nowArmed == armed && (! armed || nowBlink == blinkOn) && (! lamp || ! lit || nowBlink == blinkOn))
                return false;

            lit = nowLit;
            pad = nowPad;
            learnMode = nowLearning;
            armed = nowArmed;
            blinkOn = nowBlink;
            repaint();
            return true;
        }

        void clicked (const juce::ModifierKeys& modifiers) override
        {
            if (onPress != nullptr)
                onPress (modifiers.isPopupMenu());
        }

        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            auto area = getLocalBounds().toFloat().reduced (1.5f);
            const auto radius = area.getHeight() * 0.5f;
            auto fill = lit ? colour.withMultipliedBrightness (0.62f).withMultipliedSaturation (0.95f) : widget;

            if (down)
                fill = fill.brighter (0.12f);
            else if (over && ! lit)
                fill = fill.brighter (0.05f);

            g.setColour (fill);
            g.fillRoundedRectangle (area, radius);
            g.setColour (lit ? colour : colour.withAlpha (over ? 0.75f : 0.4f));
            g.drawRoundedRectangle (area, radius, lit ? 1.5f : 1.0f);

            auto words = area.reduced (radius * 0.6f, 0.0f);

            if (lamp)
            {
                const auto dot = juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ words.getX() + 6.0f, area.getCentreY() });
                g.setColour (lit ? (blinkOn ? colour.brighter (0.3f) : colour.withAlpha (0.55f)) : colour.withAlpha (0.3f));
                g.fillEllipse (dot);

                if (lit && blinkOn)
                {
                    g.setColour (colour.withAlpha (0.35f));
                    g.fillEllipse (dot.expanded (3.0f));
                }

                words.removeFromLeft (14.0f);
            }

            g.setColour (lit ? juce::Colours::white : ink.withAlpha (0.88f));
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawFittedText (getButtonText(), words.toNearestInt(), juce::Justification::centred, 1);

            // Its pad, in a chip at the top right.
            if (pad.isNotEmpty())
            {
                const juce::Font chipFont (juce::FontOptions (9.5f, juce::Font::bold));
                const auto chipWidth = juce::GlyphArrangement::getStringWidth (chipFont, pad) + 8.0f;
                const juce::Rectangle<float> chip (getWidth() - chipWidth - 1.0f, 0.0f, chipWidth, 13.0f);
                g.setColour (juce::Colour (0xff0d0e11).withAlpha (0.9f));
                g.fillRoundedRectangle (chip, 4.0f);
                g.setColour (learnColour.withAlpha (0.9f));
                g.drawRoundedRectangle (chip.reduced (0.5f), 4.0f, 1.0f);
                g.setFont (chipFont);
                g.drawText (pad, chip, juce::Justification::centred, false);
            }

            // Learning: a dashed edge; waiting for its pad: blinking.
            if (learnMode)
            {
                juce::Path outline;
                outline.addRoundedRectangle (area.expanded (0.5f), radius);

                if (armed)
                {
                    g.setColour (learnColour.withAlpha (blinkOn ? 0.35f : 0.12f));
                    g.fillPath (outline);
                    g.setColour (learnColour);
                    g.strokePath (outline, juce::PathStrokeType (2.0f));
                }
                else
                {
                    juce::Path dashed;
                    const float dashes[] { 4.0f, 3.0f };
                    juce::PathStrokeType (1.5f).createDashedStroke (dashed, outline, dashes, 2);
                    g.setColour (learnColour.withAlpha (0.8f));
                    g.fillPath (dashed);
                }
            }
        }
    };
}

//==============================================================================
/** The Controller's window: the preview with its boxes, the rows, the presets, the show's switches
    and the MIDI bar. */
class Controller::Panel final : public juce::Component,
                                private juce::Timer
{
public:
    explicit Panel (Controller& owner)
        : controller (owner), preview (*this), presets (*this)
    {
        lookAndFeel.setColourScheme ({ ground, widget, panelColour, line, ink, green, juce::Colours::white, green.darker (0.5f), ink });
        lookAndFeel.setColour (juce::ListBox::backgroundColourId, panelColour);
        lookAndFeel.setColour (juce::ListBox::outlineColourId, line);
        lookAndFeel.setColour (juce::TextButton::buttonColourId, widget);
        lookAndFeel.setColour (juce::TextButton::textColourOffId, ink);
        lookAndFeel.setColour (juce::ComboBox::backgroundColourId, widget);
        lookAndFeel.setColour (juce::ComboBox::outlineColourId, line);
        lookAndFeel.setColour (juce::Slider::trackColourId, blue.withAlpha (0.7f));
        lookAndFeel.setColour (juce::Slider::backgroundColourId, widget.brighter (0.1f));
        lookAndFeel.setColour (juce::Slider::thumbColourId, ink);
        lookAndFeel.setColour (juce::Slider::textBoxBackgroundColourId, widget);
        lookAndFeel.setColour (juce::Slider::textBoxOutlineColourId, line);
        lookAndFeel.setColour (juce::Slider::textBoxTextColourId, ink);
        setLookAndFeel (&lookAndFeel);

        for (auto* button : { &streamButton, &micsButton, &playbackButton, &monitorButton })
        {
            addAndMakeVisible (*button);
            button->onPress = [this, button] (bool rightClick) { press (button->action, rightClick); };
        }

        streamButton.lamp = true;
        monitorButton.setTooltip ("Monitor: the picture exactly as the viewers see it, in a small window over every other (never in the stream)");
        streamButton.setTooltip ("Enable streaming: the picture and the sound go out to the stream (off: viewers see the show's name)");
        micsButton.setTooltip ("Mute mics: the live channels go silent");
        playbackButton.setTooltip ("Mute playback: the player, the decks and the pads go silent");
        addAndMakeVisible (preview);

        for (int r = 0; r < numRows; ++r)
        {
            rows[static_cast<size_t> (r)] = std::make_unique<RowView> (*this, r);
            addAndMakeVisible (*rows[static_cast<size_t> (r)]);
        }

        addAndMakeVisible (presets);

        for (auto* button : { &saveButton, &saveAsButton, &renameButton, &deleteButton, &folderButton })
            addAndMakeVisible (*button);

        saveButton.setTooltip ("Keep the rows as they are now in the preset picked");
        saveAsButton.setTooltip ("Keep the rows as they are now as a new preset");
        renameButton.setTooltip ("Give the preset picked another name");
        deleteButton.setTooltip ("Delete the preset picked");
        folderButton.setTooltip ("Show the presets' folder");
        saveButton.onClick = [this] { saveCurrent(); };
        saveAsButton.onClick = [this] { saveAs(); };
        renameButton.onClick = [this] { renamePicked(); };
        deleteButton.onClick = [this] { deletePicked(); };
        folderButton.onClick = [] { auto folder = Controller::getPresetsFolder(); folder.createDirectory(); folder.revealToUser(); };

        midiLabel.setText ("MIDI", juce::dontSendNotification);
        midiLabel.setColour (juce::Label::textColourId, muted);
        midiLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        addAndMakeVisible (midiLabel);
        addAndMakeVisible (midiPorts);
        midiPorts.setTooltip ("The MIDI device the Controller listens to");
        midiPorts.onChange = [this] { midiPortPicked(); };

        for (auto* button : { &learnButton, &lightsButton })
        {
            addAndMakeVisible (*button);
            button->onPress = [this, button] (bool) { press (button->action, false); };
        }

        learnButton.setTooltip ("Learn: click a button, then press a pad on your device (right-click a button: forget its pad)");
        lightsButton.setTooltip ("Pad lights: your controller's pads light up for what's live (when it has lights)");
        addAndMakeVisible (lookAgain);
        lookAgain.setTooltip ("Look for cameras again (one plugged in since)");
        lookAgain.onClick = [this] { refreshDevices (true); };

        status.setJustificationType (juce::Justification::centredRight);
        status.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        addAndMakeVisible (status);
        note.setColour (juce::Label::textColourId, muted);
        note.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (note);

        refreshDevices (false);
        refreshMidiPorts();
        presets.refresh();
        refreshLit();
        startTimerHz (15);
    }

    ~Panel() override
    {
        ask = nullptr;
        setLookAndFeel (nullptr);
    }

    /** Words on its line, for a while. */
    void setNote (const juce::String& words)
    {
        note.setText (words, juce::dontSendNotification);
        noteUntil = juce::Time::getMillisecondCounterHiRes() + 10000.0;
    }

    /** The rows' cameras and screens, as Sonify finds them (again: looked for anew). */
    void refreshDevices (bool again)
    {
        cameras = controller.host.controllerCameras (again);
        screens = controller.host.controllerScreens();

        for (auto& row : rows)
            row->refreshDevices();

        if (again)
            setNote (juce::String (static_cast<int> (std::count_if (cameras.begin(), cameras.end(), [] (const LiveStream::Device& d) { return d.id != "test"; })))
                     + " cameras found.");
    }

    void refreshMidiPorts()
    {
        knownPorts = controller.host.controllerMidiPorts();
        midiPortNames = knownPorts;
        const auto& chosen = controller.getSetup().midiPort;

        if (chosen.isNotEmpty() && ! midiPortNames.contains (chosen))
            midiPortNames.add (chosen);   // not there now: shown as it was chosen

        midiPorts.clear (juce::dontSendNotification);
        midiPorts.addItem ("No MIDI device", 1);

        for (int i = 0; i < midiPortNames.size(); ++i)
            midiPorts.addItem (midiPortNames[i] + (knownPorts.contains (midiPortNames[i]) ? juce::String() : juce::String (" (not connected)")), i + 2);

        midiPorts.setSelectedId (chosen.isEmpty() ? 1 : midiPortNames.indexOf (chosen) + 2, juce::dontSendNotification);
    }

    /** Every button's light, pad and learning look; the rows and the list as the Controller has them. */
    void refreshLit()
    {
        const auto learnMode = controller.isLearnMode();
        const auto& learning = controller.getLearning();
        const auto blink = blinkOn;
        const auto look = [&] (PadButton& button)
        {
            button.update (controller.isLit (button.action), controller.padLabel (button.action), learnMode, learning == button.action, blink);
        };

        look (streamButton);
        look (micsButton);
        look (playbackButton);
        look (monitorButton);
        learnButton.update (learnMode, {}, false, false, blink);
        lightsButton.update (controller.getSetup().lights, {}, false, false, blink);

        for (auto& row : rows)
        {
            for (auto* button : row->buttons())
                look (*button);

            row->refresh();
        }

        // The list drawn again when what it shows changed (and while it blinks for a pad).
        const auto listLook = controller.getSetup().preset + "|" + juce::String ((int) controller.isPresetChanged()) + "|"
                            + juce::String ((int) learnMode) + "|" + learning + "|" + juce::String ((int) (learning.startsWith ("preset.") && blink))
                            + "|" + juce::String (static_cast<int> (controller.getSetup().pads.size()));

        if (listLook != presetsLook)
        {
            presetsLook = listLook;
            presets.list.repaint();
        }

        renameButton.setEnabled (controller.getSetup().preset.isNotEmpty());
        deleteButton.setEnabled (controller.getSetup().preset.isNotEmpty());

        // How the stream goes.
        bool live = false;
        const auto words = controller.host.controllerStatus (live);

        if (words != status.getText() || live != statusLive)
        {
            statusLive = live;
            status.setText (words, juce::dontSendNotification);
            status.setColour (juce::Label::textColourId, live ? red : muted);
        }
    }

    /** A button pressed: learning, it's armed (a right-click forgets its pad); else it does its thing. */
    void press (const juce::String& action, bool rightClick)
    {
        if (action == "learn")
        {
            if (! controller.isLearnMode() && controller.getSetup().midiPort.isEmpty())
            {
                setNote ("Choose your MIDI device first (the list beside Learn).");
                return;
            }

            controller.setLearnMode (! controller.isLearnMode());
            setNote (controller.isLearnMode() ? "Learn: click a button, then press a pad on your device. Right-click a button to forget its pad."
                                              : juce::String());
            refreshLit();
            return;
        }

        if (action == "lights")
        {
            controller.setLights (! controller.getSetup().lights);
            refreshLit();
            return;
        }

        if (controller.isLearnMode())
        {
            if (rightClick)
                controller.forget (action);
            else
                controller.learn (action);

            refreshLit();
            return;
        }

        if (! rightClick)
            controller.perform (action);

        refreshLit();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (ground);
        g.setColour (panelColour);
        g.fillRect (topBar);
        g.fillRect (bottomBar);
        g.fillRect (presetArea);
        g.setColour (line);
        g.fillRect (topBar.removeFromBottom (1));
        g.fillRect (bottomBar.removeFromTop (1));
        g.fillRect (presetArea.removeFromLeft (1));

        g.setColour (muted);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText ("PRESETS", presetTitle, juce::Justification::centredLeft, false);
        g.drawText ("Click one: it's on, at once", presetTitle.withTrimmedLeft (70), juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        topBar = area.removeFromTop (52);
        bottomBar = area.removeFromBottom (48);
        presetArea = area.removeFromRight (250);

        // The show's switches, and how the stream goes.
        auto top = topBar.reduced (14, 10);
        streamButton.setBounds (top.removeFromLeft (180));
        top.removeFromLeft (10);
        micsButton.setBounds (top.removeFromLeft (120));
        top.removeFromLeft (8);
        playbackButton.setBounds (top.removeFromLeft (140));
        top.removeFromLeft (16);
        monitorButton.setBounds (top.removeFromLeft (110));
        top.removeFromLeft (12);
        status.setBounds (top);

        // The presets.
        auto list = presetArea.reduced (12, 10);
        presetTitle = list.removeFromTop (22);
        auto buttons = list.removeFromBottom (64);
        list.removeFromBottom (8);
        presets.setBounds (list);
        auto firstLine = buttons.removeFromTop (28);
        saveButton.setBounds (firstLine.removeFromLeft (firstLine.getWidth() / 2 - 3));
        firstLine.removeFromLeft (6);
        saveAsButton.setBounds (firstLine);
        buttons.removeFromTop (8);
        const auto third = (buttons.getWidth() - 12) / 3;
        renameButton.setBounds (buttons.removeFromLeft (third));
        buttons.removeFromLeft (6);
        deleteButton.setBounds (buttons.removeFromLeft (third));
        buttons.removeFromLeft (6);
        folderButton.setBounds (buttons);

        // The rows under the preview.
        auto left = area.reduced (14, 12);
        constexpr int rowHeight = 64, rowGap = 6;
        auto rowsArea = left.removeFromBottom (numRows * rowHeight + (numRows - 1) * rowGap);
        left.removeFromBottom (10);
        preview.setBounds (left);

        for (auto& row : rows)
        {
            row->setBounds (rowsArea.removeFromTop (rowHeight));
            rowsArea.removeFromTop (rowGap);
        }

        // MIDI.
        auto bottom = bottomBar.reduced (14, 10);
        midiLabel.setBounds (bottom.removeFromLeft (38));
        midiPorts.setBounds (bottom.removeFromLeft (240));
        bottom.removeFromLeft (10);
        learnButton.setBounds (bottom.removeFromLeft (84));
        bottom.removeFromLeft (8);
        lightsButton.setBounds (bottom.removeFromLeft (104));
        bottom.removeFromLeft (14);
        lookAgain.setBounds (bottom.removeFromRight (180));
        bottom.removeFromRight (10);
        note.setBounds (bottom);
    }

private:
    //==============================================================================
    /** The picture laid out (the landscape one or the vertical one, by the tabs over it), and the
        Custom boxes over it: dragged to move, by the corner to size (Shift keeps the box's shape).
        In the vertical picture, dragging across Main picks the part of it that shows. */
    class Preview final : public juce::Component
    {
    public:
        explicit Preview (Panel& p) : panel (p) {}

        void paint (juce::Graphics& g) override
        {
            const auto tall = isTall();

            // The tabs: which picture it shows.
            for (const auto which : { false, true })
            {
                const auto area = tabArea (which);
                const auto on = which == tall;
                g.setColour (on ? widget.brighter (0.25f) : widget);
                g.fillRoundedRectangle (area, 4.0f);
                g.setColour (on ? blue : line);
                g.drawRoundedRectangle (area.reduced (0.5f), 4.0f, 1.0f);
                g.setColour (on ? ink : muted);
                g.setFont (juce::FontOptions (11.5f, on ? juce::Font::bold : juce::Font::plain));
                g.drawText (tabText (which), area, juce::Justification::centred, false);
            }

            g.setColour (muted);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (tall ? "Drag across the picture: the part of Main that shows. Each box has its place here too."
                             : "Each box: dragged to move, by its corner to size (Shift keeps its shape).",
                        hintArea(), juce::Justification::centredLeft, true);

            const auto frame = frameArea();
            g.setColour (juce::Colours::black);
            g.fillRect (frame);
            const auto picture = tall ? panel.controller.host.controllerTallPicture() : panel.controller.host.controllerPicture();

            if (picture.isValid())
            {
                g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
                g.drawImage (picture, frame, juce::RectanglePlacement::centred);
            }
            else
            {
                g.setColour (muted);
                g.setFont (juce::FontOptions (13.0f));
                g.drawFittedText ("The picture shows here once the cameras open", frame.toNearestInt().reduced (8), juce::Justification::centred, 3);
            }

            // The boxes: their edge, their name, their corner.
            const auto& setup = panel.controller.getSetup();

            for (int r = 0; r < numRows; ++r)
            {
                const auto& row = setup.rows[static_cast<size_t> (r)];

                if (row.show != Show::custom || row.device.isEmpty())
                    continue;

                const auto box = onScreen (boxOf (row));
                const bool active = r == dragRow || (dragRow < 0 && r == hoverRow);
                juce::Path outline;
                outline.addRectangle (box);

                if (active)
                {
                    g.setColour (blue);
                    g.strokePath (outline, juce::PathStrokeType (2.0f));
                }
                else
                {
                    juce::Path dashed;
                    const float dashes[] { 5.0f, 4.0f };
                    juce::PathStrokeType (1.2f).createDashedStroke (dashed, outline, dashes, 2);
                    g.setColour (juce::Colours::white.withAlpha (0.75f));
                    g.fillPath (dashed);
                }

                const juce::Font font (juce::FontOptions (11.0f, juce::Font::bold));
                const auto name = rowName (r);
                const juce::Rectangle<float> tag (box.getX(), box.getY(), juce::GlyphArrangement::getStringWidth (font, name) + 10.0f, 16.0f);
                g.setColour ((active ? blue : juce::Colour (0xff0d0e11)).withAlpha (0.85f));
                g.fillRect (tag);
                g.setColour (juce::Colours::white);
                g.setFont (font);
                g.drawText (name, tag, juce::Justification::centred, false);

                const auto grip = gripOf (box);
                g.setColour (active ? blue : juce::Colours::white.withAlpha (0.85f));
                g.fillRect (grip.reduced (2.0f));
            }

            // Streaming not enabled: what goes out is the show's name.
            if (! panel.controller.host.controllerSwitch ("stream"))
            {
                const juce::String words (tall ? "Streaming not enabled" : "Streaming not enabled: viewers see the show's name");
                const juce::Font font (juce::FontOptions (11.5f, juce::Font::bold));
                const auto badgeWidth = juce::jmin (frame.getWidth() - 16.0f, juce::GlyphArrangement::getStringWidth (font, words) + 16.0f);
                const juce::Rectangle<float> badge (frame.getX() + 8.0f, frame.getBottom() - 26.0f, badgeWidth, 19.0f);
                g.setColour (juce::Colour (0xff3a0f14).withAlpha (0.9f));
                g.fillRoundedRectangle (badge, 4.0f);
                g.setColour (red);
                g.drawRoundedRectangle (badge.reduced (0.5f), 4.0f, 1.0f);
                g.setColour (juce::Colours::white);
                g.setFont (font);
                g.drawFittedText (words, badge.toNearestInt(), juce::Justification::centred, 1);
            }

            g.setColour (line);
            g.drawRect (frame.expanded (1.0f), 1.0f);
        }

        void mouseMove (const juce::MouseEvent& e) override
        {
            bool corner = false;
            const auto row = boxAt (e.position, corner);

            if (row != hoverRow)
            {
                hoverRow = row;
                repaint();
            }

            const auto overTab = tabArea (false).contains (e.position) || tabArea (true).contains (e.position);
            setMouseCursor (overTab ? juce::MouseCursor::PointingHandCursor
                          : row >= 0 ? (corner ? juce::MouseCursor::BottomRightCornerResizeCursor : juce::MouseCursor::DraggingHandCursor)
                          : isTall() && mainRow() >= 0 && frameArea().contains (e.position) ? juce::MouseCursor::LeftRightResizeCursor
                                                                                             : juce::MouseCursor::NormalCursor);
        }

        void mouseExit (const juce::MouseEvent&) override
        {
            if (hoverRow >= 0 && dragRow < 0)
            {
                hoverRow = -1;
                repaint();
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            for (const auto which : { false, true })
            {
                if (tabArea (which).contains (e.position))
                {
                    panel.controller.setViewTall (which);
                    hoverRow = dragRow = -1;
                    repaint();
                    return;
                }
            }

            dragRow = boxAt (e.position, dragCorner);
            dragFrom = e.position;
            focusRow = -1;

            if (dragRow >= 0)
            {
                dragStart = boxOf (panel.controller.getSetup().rows[static_cast<size_t> (dragRow)]);
                return;
            }

            // The vertical picture: Main dragged across.
            if (isTall() && frameArea().contains (e.position))
            {
                focusRow = mainRow();

                if (focusRow >= 0)
                    focusStart = panel.controller.getSetup().rows[static_cast<size_t> (focusRow)].tallFocus;
            }
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            const auto frame = frameArea();
            const auto dx = (e.position.x - dragFrom.x) / juce::jmax (1.0f, frame.getWidth());
            const auto dy = (e.position.y - dragFrom.y) / juce::jmax (1.0f, frame.getHeight());

            if (focusRow >= 0)
            {
                // The picture follows the mouse: how far it moves across, as a share of what's cut off.
                const auto overflow = overflowOf (focusRow, frame);

                if (overflow > 0.001f)
                    panel.controller.setTallFocus (focusRow, focusStart - dx / overflow);

                repaint();
                return;
            }

            if (dragRow < 0)
                return;

            auto box = dragStart;

            if (dragCorner)
            {
                auto w = juce::jlimit (smallestBox, 1.0f - box.getX(), box.getWidth() + dx);
                auto h = juce::jlimit (smallestBox, 1.0f - box.getY(), box.getHeight() + dy);

                // Shift: the box keeps its shape.
                if (e.mods.isShiftDown() && dragStart.getWidth() > 0.0f && dragStart.getHeight() > 0.0f)
                {
                    const auto shape = dragStart.getHeight() / dragStart.getWidth();
                    auto scale = juce::jmax (w / dragStart.getWidth(), h / dragStart.getHeight());
                    scale = juce::jmin (scale, (1.0f - box.getX()) / dragStart.getWidth(), (1.0f - box.getY()) / dragStart.getHeight());
                    w = juce::jmax (smallestBox, dragStart.getWidth() * scale);
                    h = juce::jmax (smallestBox, w * shape);
                }

                box.setSize (w, h);
            }
            else
            {
                auto x = juce::jlimit (0.0f, 1.0f - box.getWidth(), box.getX() + dx);
                auto y = juce::jlimit (0.0f, 1.0f - box.getHeight(), box.getY() + dy);

                // Near the picture's edges, or just in from them, it snaps there.
                const auto snap = [] (float v, float size)
                {
                    for (const auto at : { 0.0f, boxMargin, 1.0f - boxMargin - size, 1.0f - size })
                        if (std::abs (v - at) < snapReach)
                            return at;

                    return v;
                };

                box.setPosition (snap (x, box.getWidth()), snap (y, box.getHeight()));
            }

            if (isTall())
                panel.controller.setTallBox (dragRow, box);
            else
                panel.controller.setBox (dragRow, box);

            repaint();
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            dragRow = -1;
            focusRow = -1;
            repaint();
        }

    private:
        bool isTall() const   { return panel.controller.isViewTall(); }

        static juce::String tabText (bool tall)
        {
            return tall ? juce::String ("Vertical (TikTok)") : juce::String ("Landscape (YouTube, Facebook, Twitch)");
        }

        juce::Rectangle<float> tabArea (bool tall) const
        {
            const juce::Font font (juce::FontOptions (11.5f, juce::Font::bold));
            const auto wideWidth = juce::GlyphArrangement::getStringWidth (font, tabText (false)) + 24.0f;
            const auto tallWidth = juce::GlyphArrangement::getStringWidth (font, tabText (true)) + 24.0f;
            return tall ? juce::Rectangle<float> (wideWidth + 6.0f, 0.0f, tallWidth, tabHeight)
                        : juce::Rectangle<float> (0.0f, 0.0f, wideWidth, tabHeight);
        }

        juce::Rectangle<float> hintArea() const
        {
            const auto right = tabArea (true).getRight() + 14.0f;
            return { right, 0.0f, juce::jmax (0.0f, static_cast<float> (getWidth()) - right), tabHeight };
        }

        juce::Rectangle<float> frameArea() const
        {
            const auto area = getLocalBounds().toFloat().withTrimmedTop (tabHeight + 6.0f).reduced (1.0f);
            const auto shape = isTall() ? 16.0f / 9.0f : 9.0f / 16.0f;   // height over width
            const auto w = juce::jmin (area.getWidth(), area.getHeight() / shape);
            return juce::Rectangle<float> (w, w * shape).withCentre (area.getCentre());
        }

        juce::Rectangle<float> boxOf (const Row& row) const
        {
            return isTall() ? row.tallBox : row.box;
        }

        juce::Rectangle<float> onScreen (juce::Rectangle<float> box) const
        {
            const auto frame = frameArea();
            return { frame.getX() + box.getX() * frame.getWidth(), frame.getY() + box.getY() * frame.getHeight(),
                     box.getWidth() * frame.getWidth(), box.getHeight() * frame.getHeight() };
        }

        static juce::Rectangle<float> gripOf (juce::Rectangle<float> box)
        {
            return juce::Rectangle<float> (12.0f, 12.0f).withPosition (box.getRight() - 12.0f, box.getBottom() - 12.0f);
        }

        /** The row on Main (with its camera or screen), or -1. */
        int mainRow() const
        {
            const auto& setup = panel.controller.getSetup();

            for (int r = 0; r < numRows; ++r)
                if (setup.rows[static_cast<size_t> (r)].show == Show::main && setup.rows[static_cast<size_t> (r)].device.isNotEmpty())
                    return r;

            return -1;
        }

        /** How much of Main is cut off across in the vertical picture, in the picture's widths (its
            own shape when it has come; a camera's 16:9 till then). */
        float overflowOf (int row, juce::Rectangle<float> frame) const
        {
            const auto picture = panel.controller.host.controllerSourcePicture (row);
            const auto shape = picture.isValid() && picture.getHeight() > 0 ? static_cast<float> (picture.getWidth()) / static_cast<float> (picture.getHeight())
                                                                            : 16.0f / 9.0f;
            const auto shownWidth = frame.getHeight() * shape;   // filling the frame's height
            return juce::jmax (0.0f, shownWidth / juce::jmax (1.0f, frame.getWidth()) - 1.0f);
        }

        /** The box at a point: the one on top (the last row's), and whether it's its corner. */
        int boxAt (juce::Point<float> at, bool& corner) const
        {
            const auto& setup = panel.controller.getSetup();

            for (int r = numRows - 1; r >= 0; --r)
            {
                const auto& row = setup.rows[static_cast<size_t> (r)];

                if (row.show != Show::custom || row.device.isEmpty())
                    continue;

                const auto box = onScreen (boxOf (row));

                if (gripOf (box).expanded (3.0f).contains (at))
                {
                    corner = true;
                    return r;
                }

                if (box.contains (at))
                {
                    corner = false;
                    return r;
                }
            }

            corner = false;
            return -1;
        }

        static constexpr float tabHeight = 22.0f;

        Panel& panel;
        int dragRow = -1, hoverRow = -1, focusRow = -1;
        bool dragCorner = false;
        juce::Rectangle<float> dragStart;
        juce::Point<float> dragFrom;
        float focusStart = 0.5f;
    };

    //==============================================================================
    /** A row: its picture as it comes, its camera (or screen), Main, Custom and Hidden, and a
        camera's Sync. */
    class RowView final : public juce::Component
    {
    public:
        RowView (Panel& p, int r)
            : panel (p), row (r),
              mainButton ("row." + juce::String (r + 1) + ".main", "Main", green),
              customButton ("row." + juce::String (r + 1) + ".custom", "Custom", blue),
              hiddenButton ("row." + juce::String (r + 1) + ".hidden", "Hidden", grey)
        {
            addAndMakeVisible (device);
            device.setTooltip (r == screenRow ? "The screen this row shows" : "The camera this row shows");
            device.onChange = [this] { devicePicked(); };

            for (auto* button : buttons())
            {
                addAndMakeVisible (*button);
                button->onPress = [this, button] (bool rightClick) { panel.press (button->action, rightClick); };
            }

            mainButton.setTooltip ("Main: the whole picture (the Main before it goes to Hidden)");
            customButton.setTooltip ("Custom: a box of its own size and place over the picture (drag it on the preview)");
            hiddenButton.setTooltip ("Hidden: not in the picture (it stays open, so it comes back at once)");

            if (row != screenRow)
            {
                addAndMakeVisible (sync);
                sync.setColour (juce::Slider::textBoxOutlineColourId, line);
                sync.setColour (juce::Slider::textBoxBackgroundColourId, widget);
                sync.setColour (juce::Slider::textBoxTextColourId, ink);
                sync.setSliderStyle (juce::Slider::LinearHorizontal);
                sync.setRange (0.0, LiveStream::maxDelayMs, 10.0);
                sync.setTextBoxStyle (juce::Slider::TextBoxRight, false, 58, 20);
                sync.setTextValueSuffix (" ms");
                sync.setDoubleClickReturnValue (true, 0.0);
                sync.setTooltip ("Sync: holds this camera's pictures a little, so a quick camera waits for a slower one (double-click: 0)");
                sync.onValueChange = [this] { panel.controller.setDelay (row, juce::roundToInt (sync.getValue())); };
            }
        }

        std::array<PadButton*, 3> buttons()     { return { &mainButton, &customButton, &hiddenButton }; }

        void refreshDevices()
        {
            const auto& list = row == screenRow ? panel.screens : panel.cameras;
            const auto& chosen = panel.controller.getSetup().rows[static_cast<size_t> (row)].device;
            ids.clear();
            device.clear (juce::dontSendNotification);
            device.addItem (row == screenRow ? "No screen" : "No camera", 1);
            ids.add ({});

            for (const auto& d : list)
            {
                device.addItem (d.name, ids.size() + 1);
                ids.add (d.id);
            }

            if (chosen.isNotEmpty() && ! ids.contains (chosen))
            {
                device.addItem (chosen + " (not found)", ids.size() + 1);
                ids.add (chosen);
            }

            device.setSelectedId (ids.indexOf (chosen) + 1, juce::dontSendNotification);
        }

        /** Its picture, and Sync as the Controller has it. */
        void refresh()
        {
            const auto& r = panel.controller.getSetup().rows[static_cast<size_t> (row)];

            if (row != screenRow && ! sync.isMouseButtonDown() && juce::roundToInt (sync.getValue()) != r.delayMs)
                sync.setValue (r.delayMs, juce::dontSendNotification);

            if (ids.indexOf (r.device) + 1 != device.getSelectedId())
                refreshDevices();

            repaint (thumb);
        }

        void paint (juce::Graphics& g) override
        {
            const auto& r = panel.controller.getSetup().rows[static_cast<size_t> (row)];
            const auto bounds = getLocalBounds().toFloat();
            g.setColour (panelColour);
            g.fillRoundedRectangle (bounds, 8.0f);
            g.setColour (r.show == Show::main && r.device.isNotEmpty() ? green.withAlpha (0.8f)
                         : r.show == Show::custom && r.device.isNotEmpty() ? blue.withAlpha (0.7f) : line);
            g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

            // Its picture as it comes.
            const auto area = thumb.toFloat();
            g.setColour (juce::Colours::black);
            g.fillRoundedRectangle (area, 4.0f);
            const auto picture = r.device.isNotEmpty() ? panel.controller.host.controllerSourcePicture (row) : juce::Image();

            if (picture.isValid())
            {
                g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
                g.drawImage (picture, area.reduced (1.0f), juce::RectanglePlacement::centred);
            }
            else
            {
                const auto problem = r.device.isNotEmpty() ? panel.controller.host.controllerSourceProblem (row) : juce::String();
                g.setColour (problem.isNotEmpty() ? red.withAlpha (0.9f) : muted);
                g.setFont (juce::FontOptions (10.5f));
                g.drawFittedText (r.device.isEmpty() ? juce::String ("Off") : problem.isNotEmpty() ? problem : juce::String ("Opening..."),
                                  thumb.reduced (4), juce::Justification::centred, 3);
            }

            g.setColour (ink);
            g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
            g.drawText (rowName (row), nameArea, juce::Justification::centredLeft, false);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (6);
            thumb = area.removeFromLeft (juce::roundToInt (area.getHeight() * 16.0f / 9.0f));
            area.removeFromLeft (10);
            auto words = area.removeFromLeft (230);
            nameArea = words.removeFromTop (words.getHeight() / 2 - 2);
            words.removeFromTop (2);
            device.setBounds (words.withHeight (juce::jmin (24, words.getHeight())));
            area.removeFromLeft (12);
            auto buttonsArea = area.removeFromLeft (3 * 84 + 2 * 6).withSizeKeepingCentre (3 * 84 + 2 * 6, 30);

            for (auto* button : buttons())
            {
                button->setBounds (buttonsArea.removeFromLeft (84));
                buttonsArea.removeFromLeft (6);
            }

            area.removeFromLeft (12);

            if (row != screenRow)
                sync.setBounds (area.withSizeKeepingCentre (juce::jmin (area.getWidth(), 220), 24));
        }

    private:
        void devicePicked()
        {
            const auto index = device.getSelectedId() - 1;

            if (juce::isPositiveAndBelow (index, ids.size()))
                panel.controller.setDevice (row, ids[index]);

            panel.refreshLit();
        }

        Panel& panel;
        const int row;
        juce::ComboBox device;
        juce::StringArray ids;                  // the list's devices, by item id - 1
        PadButton mainButton, customButton, hiddenButton;
        juce::Slider sync;
        juce::Rectangle<int> thumb, nameArea;
    };

    //==============================================================================
    /** The presets, by name: a click puts one on at once (learning: a click arms it, a right-click
        forgets its pad); the one picked is lit (a dot: its rows moved since). */
    class PresetList final : public juce::Component,
                             private juce::ListBoxModel
    {
    public:
        explicit PresetList (Panel& p) : panel (p)
        {
            list.setModel (this);
            list.setRowHeight (30);
            list.setColour (juce::ListBox::backgroundColourId, panelColour);
            addAndMakeVisible (list);
        }

        void refresh()
        {
            names = panel.controller.getPresets();
            list.updateContent();
            list.repaint();
        }

        void resized() override     { list.setBounds (getLocalBounds()); }

        juce::ListBox list;
        juce::StringArray names;

    private:
        int getNumRows() override   { return names.size(); }

        void paintListBoxItem (int index, juce::Graphics& g, int width, int height, bool) override
        {
            if (! juce::isPositiveAndBelow (index, names.size()))
                return;

            const auto& name = names[index];
            const auto action = "preset." + name;
            const auto& controller = panel.controller;
            const bool picked = controller.getSetup().preset == name;
            const auto area = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height)).reduced (2.0f, 2.0f);

            if (picked)
            {
                g.setColour (green.withAlpha (controller.isPresetChanged() ? 0.18f : 0.32f));
                g.fillRoundedRectangle (area, 5.0f);
                g.setColour (green.withAlpha (0.8f));
                g.drawRoundedRectangle (area.reduced (0.5f), 5.0f, 1.0f);
            }

            auto words = area.reduced (10.0f, 0.0f);
            const auto pad = controller.padLabel (action);

            if (pad.isNotEmpty())
            {
                const juce::Font chipFont (juce::FontOptions (9.5f, juce::Font::bold));
                const auto chipWidth = juce::GlyphArrangement::getStringWidth (chipFont, pad) + 8.0f;
                const auto chip = words.removeFromRight (chipWidth).withSizeKeepingCentre (chipWidth, 14.0f);
                g.setColour (learnColour.withAlpha (0.9f));
                g.drawRoundedRectangle (chip, 4.0f, 1.0f);
                g.setFont (chipFont);
                g.drawText (pad, chip, juce::Justification::centred, false);
                words.removeFromRight (6.0f);
            }

            g.setColour (picked ? juce::Colours::white : ink);
            g.setFont (juce::FontOptions (12.5f, picked ? juce::Font::bold : juce::Font::plain));
            g.drawText (name + (picked && controller.isPresetChanged() ? juce::String (juce::CharPointer_UTF8 (" \xe2\x80\xa2")) : juce::String()),
                        words, juce::Justification::centredLeft, true);

            if (controller.isLearnMode())
            {
                juce::Path outline;
                outline.addRoundedRectangle (area, 5.0f);

                if (controller.getLearning() == action)
                {
                    g.setColour (learnColour.withAlpha (panel.blinkOn ? 0.3f : 0.1f));
                    g.fillPath (outline);
                    g.setColour (learnColour);
                    g.strokePath (outline, juce::PathStrokeType (1.5f));
                }
                else
                {
                    juce::Path dashed;
                    const float dashes[] { 4.0f, 3.0f };
                    juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
                    g.setColour (learnColour.withAlpha (0.6f));
                    g.fillPath (dashed);
                }
            }
        }

        void listBoxItemClicked (int index, const juce::MouseEvent& e) override
        {
            if (! juce::isPositiveAndBelow (index, names.size()))
                return;

            const auto name = names[index];

            if (panel.controller.isLearnMode())
            {
                panel.press ("preset." + name, e.mods.isPopupMenu());
                return;
            }

            if (e.mods.isPopupMenu())
            {
                presetMenu (name);
                return;
            }

            panel.controller.loadPreset (name);
            panel.refreshLit();
        }

        void presetMenu (const juce::String& name)
        {
            juce::PopupMenu menu;
            menu.addSectionHeader (name);
            menu.addItem (1, "Put it on");
            menu.addItem (2, "Keep the rows as they are now in it");
            menu.addItem (3, "Rename...");
            menu.addItem (4, "Delete...");
            menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition(),
                                [safe = juce::Component::SafePointer<PresetList> (this), name] (int chosen)
            {
                if (safe == nullptr)
                    return;

                auto& panelNow = safe->panel;

                if (chosen == 1)
                    panelNow.controller.loadPreset (name);
                else if (chosen == 2)
                    panelNow.controller.savePreset (name);
                else if (chosen == 3)
                    panelNow.renamePreset (name);
                else if (chosen == 4)
                    panelNow.deletePreset (name);

                panelNow.presets.refresh();
                panelNow.refreshLit();
            });
        }

        Panel& panel;
    };

    //==============================================================================
    void timerCallback() override
    {
        if (! isShowing())
            return;   // minimised: nothing to draw (the pads go on working)

        const auto now = juce::Time::getMillisecondCounterHiRes();
        const auto blink = std::fmod (now, 900.0) < 520.0;

        if (blink != blinkOn)
            blinkOn = blink;

        preview.repaint();
        refreshLit();

        if (noteUntil > 0.0 && now > noteUntil && ! controller.isLearnMode())
        {
            noteUntil = 0.0;
            note.setText ({}, juce::dontSendNotification);
        }

        // A preset made or gone outside it (its folder, another Sonify): the list follows, now and then.
        if (++ticks % 30 == 0)
        {
            if (controller.getPresets() != presets.names)
                presets.refresh();

            if (controller.host.controllerMidiPorts() != knownPorts)
                refreshMidiPorts();
        }
    }

    void midiPortPicked()
    {
        const auto index = midiPorts.getSelectedId() - 2;
        controller.setMidiPort (juce::isPositiveAndBelow (index, midiPortNames.size()) ? midiPortNames[index] : juce::String());

        if (controller.getSetup().midiPort.isEmpty() && controller.isLearnMode())
            controller.setLearnMode (false);

        refreshLit();
    }

    //==============================================================================
    // The presets' buttons.
    void saveCurrent()
    {
        const auto& picked = controller.getSetup().preset;

        if (picked.isEmpty() || ! controller.getPresets().contains (picked))
        {
            saveAs();
            return;
        }

        if (controller.savePreset (picked))
            setNote ("Kept in " + picked + ".");

        presets.refresh();
        refreshLit();
    }

    void saveAs()
    {
        askName ("Save as a preset", "Its name:", "Save", nextName(), [this] (const juce::String& name)
        {
            const auto keep = [this, name]
            {
                if (controller.savePreset (name))
                    setNote ("Kept as " + name + ".");
                else
                    setNote ("Sonify couldn't keep " + name + " (its folder can't be written to).");

                presets.refresh();
                refreshLit();
            };

            if (controller.getPresets().contains (name))
                confirm ("Replace " + name + "?", "A preset with that name is there already.", "Replace", keep);
            else
                keep();
        });
    }

    void renamePicked()
    {
        if (controller.getSetup().preset.isNotEmpty())
            renamePreset (controller.getSetup().preset);
    }

    void renamePreset (const juce::String& name)
    {
        askName ("Rename " + name, "Its new name:", "Rename", name, [this, name] (const juce::String& to)
        {
            if (to == name)
                return;

            if (controller.getPresets().contains (to))
            {
                setNote ("There's a preset called " + to + " already.");
                return;
            }

            if (! controller.renamePreset (name, to))
                setNote ("Sonify couldn't rename " + name + ".");

            presets.refresh();
            refreshLit();
        });
    }

    void deletePicked()
    {
        if (controller.getSetup().preset.isNotEmpty())
            deletePreset (controller.getSetup().preset);
    }

    void deletePreset (const juce::String& name)
    {
        confirm ("Delete " + name + "?", "The preset's file goes (the rows stay as they are now).", "Delete", [this, name]
        {
            controller.deletePreset (name);
            presets.refresh();
            refreshLit();
        });
    }

    juce::String nextName() const
    {
        const auto names = controller.getPresets();

        for (int i = names.size() + 1;; ++i)
            if (! names.contains ("Preset " + juce::String (i)))
                return "Preset " + juce::String (i);
    }

    /** A name typed in a small window of its own (made fit for a file name). */
    void askName (const juce::String& title, const juce::String& prompt, const juce::String& okText, const juce::String& suggested,
                  std::function<void (const juce::String&)> then)
    {
        ask = std::make_unique<juce::AlertWindow> (title, prompt, juce::MessageBoxIconType::NoIcon, this);
        ask->setLookAndFeel (&lookAndFeel);
        ask->addTextEditor ("name", suggested);
        ask->addButton (okText, 1, juce::KeyPress (juce::KeyPress::returnKey));
        ask->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        // The panel may be gone by the time it answers (the page can close it): then nothing is
        // done. Only this window is let go after, not one then opened (a "Replace?").
        ask->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<Panel> (this),
                                                                          dialog = juce::Component::SafePointer<juce::AlertWindow> (ask.get()), then] (int result)
        {
            if (safe == nullptr)
                return;

            const auto typed = dialog != nullptr ? dialog->getTextEditorContents ("name") : juce::String();
            juce::MessageManager::callAsync ([safe, dialog]
            {
                if (safe != nullptr && dialog != nullptr && safe->ask.get() == dialog.getComponent())
                    safe->ask = nullptr;
            });

            const auto name = juce::File::createLegalFileName (typed.trim()).trim();

            if (result == 1 && name.isNotEmpty())
                then (name);
        }), false);
    }

    void confirm (const juce::String& title, const juce::String& message, const juce::String& yesText, std::function<void()> then)
    {
        ask = std::make_unique<juce::AlertWindow> (title, message, juce::MessageBoxIconType::QuestionIcon, this);
        ask->setLookAndFeel (&lookAndFeel);
        ask->addButton (yesText, 1, juce::KeyPress (juce::KeyPress::returnKey));
        ask->addButton ("Keep it", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        // As askName: nothing once the panel's gone, and only this window let go after.
        ask->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<Panel> (this),
                                                                          dialog = juce::Component::SafePointer<juce::AlertWindow> (ask.get()), then] (int result)
        {
            if (safe == nullptr)
                return;

            juce::MessageManager::callAsync ([safe, dialog]
            {
                if (safe != nullptr && dialog != nullptr && safe->ask.get() == dialog.getComponent())
                    safe->ask = nullptr;
            });

            if (result == 1)
                then();
        }), false);
    }

    Controller& controller;
    ControllerLook lookAndFeel;
    PadButton streamButton { "switch.stream", "Enable streaming", red };
    PadButton micsButton { "switch.mics", "Mute mics", amber };
    PadButton playbackButton { "switch.playback", "Mute playback", amber };
    PadButton monitorButton { "show.monitor", "Monitor", juce::Colour (0xff2ec4c4) };
    juce::Label status;
    Preview preview;
    std::array<std::unique_ptr<RowView>, numRows> rows;
    PresetList presets;
    juce::TextButton saveButton { "Save" }, saveAsButton { "Save as..." }, renameButton { "Rename..." }, deleteButton { "Delete..." }, folderButton { "Folder" };
    juce::Label midiLabel;
    juce::ComboBox midiPorts;
    juce::StringArray midiPortNames, knownPorts;
    PadButton learnButton { "learn", "Learn", learnColour };
    PadButton lightsButton { "lights", "Pad lights", green };
    juce::TextButton lookAgain { "Look for cameras again" };
    juce::Label note;
    std::unique_ptr<juce::AlertWindow> ask;
    std::vector<LiveStream::Device> cameras, screens;
    juce::String presetsLook;
    juce::TooltipWindow tooltips { this, 600 };
    juce::Rectangle<int> topBar, bottomBar, presetArea, presetTitle;
    bool blinkOn = true, statusLive = false;
    double noteUntil = 0.0;
    int ticks = 0;
};

//==============================================================================
/** The Controller's own window: its own button on the taskbar (it minimises there), native title
    bar, sized as the user leaves it. */
class Controller::Window final : public juce::DocumentWindow
{
public:
    explicit Window (Controller& owner)
        : juce::DocumentWindow ("Sonify Controller", ground, juce::DocumentWindow::allButtons),
          controller (owner)
    {
        setUsingNativeTitleBar (true);
        panel = new Panel (owner);
        setContentOwned (panel, false);
        setResizable (true, false);
        setResizeLimits (1040, 700, 6000, 4000);

        // Where it was left, if that's on a screen now; else in the middle of the main screen.
        const auto kept = owner.setup.window;
        const auto& displays = juce::Desktop::getInstance().getDisplays();
        bool placed = false;

        if (! kept.isEmpty())
        {
            for (const auto& display : displays.displays)
            {
                const auto shared = display.userBounds.toNearestInt().getIntersection (kept);

                if (shared.getWidth() > 200 && shared.getHeight() > 120)
                {
                    setBounds (kept);
                    placed = true;
                    break;
                }
            }
        }

        if (! placed)
            centreWithSize (1180, 760);

        setVisible (true);
        toFront (true);
        ready = true;
    }

    void closeButtonPressed() override
    {
        // Gone just after (not inside its own button's call); hidden at once.
        setVisible (false);
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<Window> (this)]
        {
            if (safe != nullptr)
                safe->controller.host.controllerClosed();
        });
    }

    void moved() override      { juce::DocumentWindow::moved(); keepPlace(); }
    void resized() override    { juce::DocumentWindow::resized(); keepPlace(); }

    Panel& getPanel() noexcept { return *panel; }

private:
    void keepPlace()
    {
        if (! ready || isMinimised() || isFullScreen())
            return;

        controller.setup.window = getBounds();
        controller.saveSoon();
    }

    Controller& controller;
    Panel* panel = nullptr;     // owned as its content
    bool ready = false;
};

//==============================================================================
/** The Monitor: the picture exactly as the viewers see it, in a small window of its own over
    every other window (Sonify's and the other apps', Sonify minimised too), never in the stream:
    Windows leaves it out of every screen capture. Moved by its bar, sized by its corner (the
    picture keeps its shape); its bar: how the stream goes, minimise (just its bar), maximise (as
    big as its screen lets the picture be), back to its size, close. It never takes the keys. */
class Controller::Monitor final : public juce::Component,
                                  private juce::Timer
{
public:
    static constexpr int barHeight = 28;
    static constexpr int narrowest = 300;           // its width at the least (its bar's words and buttons)

    explicit Monitor (Controller& owner) : controller (owner)
    {
        setOpaque (true);
        setWantsKeyboardFocus (false);
        setMouseClickGrabsKeyboardFocus (false);    // (Windows: clicked, it isn't made the active window)
    }

    /** Over every window, where it was left (the main screen's top right the first time). */
    void open()
    {
        setAlwaysOnTop (true);
        setBounds (boundsNow());
        addToDesktop (juce::ComponentPeer::windowHasDropShadow | juce::ComponentPeer::windowIgnoresKeyPresses);
        setVisible (true);
        hideFromCaptures (*this);
        startTimerHz (30);
    }

    /** Shown again (it may have been hidden by its close button a moment ago). */
    void reopen()
    {
        setVisible (true);
        hideFromCaptures (*this);
        toFront (false);
    }

    static int heightFor (int width)                { return barHeight + juce::roundToInt (width * 9.0 / 16.0); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black);
        auto bar = getLocalBounds().removeFromTop (barHeight);
        g.setColour (juce::Colour (0xff1b1d21));
        g.fillRect (bar);
        g.setColour (juce::Colour (0xff2c2e34));
        g.fillRect (bar.removeFromBottom (1));

        // "Monitor", what it is, and how the stream goes.
        auto words = getLocalBounds().removeFromTop (barHeight).withTrimmedLeft (10).withRight (areaOf (Part::minimise).getX() - 8);
        const juce::Font label (juce::FontOptions (12.5f, juce::Font::bold));
        const auto labelWidth = juce::GlyphArrangement::getStringWidthInt (label, "Monitor") + 8;
        g.setFont (label);
        g.setColour (ink);
        g.drawText ("Monitor", words.removeFromLeft (labelWidth), juce::Justification::centredLeft, false);
        const juce::Font status (juce::FontOptions (11.5f, live ? juce::Font::bold : juce::Font::plain));
        const auto statusWidth = juce::jmin (words.getWidth(), juce::GlyphArrangement::getStringWidthInt (status, statusText) + 4);
        g.setFont (status);
        g.setColour (live ? red : muted);
        g.drawText (statusText, words.removeFromRight (statusWidth), juce::Justification::centredRight, true);
        g.setFont (juce::FontOptions (11.5f));
        g.setColour (muted);
        g.drawText (controller.setup.viewTall ? "vertical, as the viewers see it" : "as the viewers see it", words, juce::Justification::centredLeft, true);

        for (const auto part : { Part::minimise, Part::maximise, Part::close })
            paintButton (g, part);

        // The picture as it goes out.
        if (controller.setup.monitorMode != "min")
        {
            const auto area = getLocalBounds().withTrimmedTop (barHeight).toFloat();
            const auto picture = controller.setup.viewTall ? controller.host.controllerTallOutputPicture()
                                                           : controller.host.controllerOutputPicture();

            if (picture.isValid())
            {
                g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
                g.drawImage (picture, area, juce::RectanglePlacement::centred);
            }
            else
            {
                g.setColour (muted);
                g.setFont (juce::FontOptions (12.5f));
                g.drawText ("The picture shows here once it's made", area, juce::Justification::centred, true);
            }
        }
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        // Its edge, over any picture.
        g.setColour (juce::Colour (0xff3a3d44));
        g.drawRect (getLocalBounds(), 1);

        // The corner it's sized by (dark under light: seen on any picture).
        if (controller.setup.monitorMode == "normal")
        {
            const auto grip = gripArea().toFloat();
            const auto light = juce::Colours::white.withAlpha (hover == Part::grip || sizing ? 0.95f : 0.7f);

            for (int i = 1; i <= 3; ++i)
            {
                const auto d = 3.5f * static_cast<float> (i) + 1.0f;
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.drawLine (grip.getRight() - d + 1.0f, grip.getBottom() - 1.0f, grip.getRight() - 1.0f, grip.getBottom() - d + 1.0f, 1.6f);
                g.setColour (light);
                g.drawLine (grip.getRight() - d, grip.getBottom() - 2.0f, grip.getRight() - 2.0f, grip.getBottom() - d, 1.2f);
            }
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        setHover (partAt (e.getPosition()));
        setMouseCursor (hover == Part::grip ? juce::MouseCursor::BottomRightCornerResizeCursor : juce::MouseCursor::NormalCursor);
    }

    void mouseExit (const juce::MouseEvent&) override   { setHover (Part::none); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        pressed = partAt (e.getPosition());
        startBounds = getBounds();
        startMouse = e.getScreenPosition();
        moving = pressed == Part::bar && controller.setup.monitorMode != "max";
        sizing = pressed == Part::grip;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto delta = e.getScreenPosition() - startMouse;

        if (moving)
        {
            setTopLeftPosition (startBounds.getPosition() + delta);
        }
        else if (sizing)
        {
            const auto width = juce::jmax (narrowest, startBounds.getWidth() + delta.x);
            setSize (width, heightFor (width));
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (moving || sizing)
        {
            moving = sizing = false;
            keepPlace();
            return;
        }

        const auto part = partAt (e.getPosition());

        if (part != pressed)
            return;

        const auto& mode = controller.setup.monitorMode;

        if (part == Part::minimise)
            setMode (mode == "min" ? "normal" : "min");
        else if (part == Part::maximise)
            setMode (mode == "max" ? "normal" : "max");
        else if (part == Part::close)
            close();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (partAt (e.getPosition()) == Part::bar)
            setMode (controller.setup.monitorMode == "max" ? "normal" : "max");
    }

private:
    enum class Part { none, bar, minimise, maximise, close, grip };

    juce::Rectangle<int> areaOf (Part part) const
    {
        constexpr int buttonWidth = 26, gap = 2;
        auto bar = getLocalBounds().removeFromTop (barHeight).reduced (4, 4);
        const auto close = bar.removeFromRight (buttonWidth);
        bar.removeFromRight (gap);
        const auto maximise = bar.removeFromRight (buttonWidth);
        bar.removeFromRight (gap);
        const auto minimise = bar.removeFromRight (buttonWidth);

        return part == Part::close ? close : part == Part::maximise ? maximise : part == Part::minimise ? minimise : juce::Rectangle<int>();
    }

    juce::Rectangle<int> gripArea() const           { return getLocalBounds().removeFromBottom (16).removeFromRight (16); }

    Part partAt (juce::Point<int> at) const
    {
        for (const auto part : { Part::minimise, Part::maximise, Part::close })
            if (areaOf (part).contains (at))
                return part;

        if (controller.setup.monitorMode == "normal" && gripArea().contains (at))
            return Part::grip;

        return at.y < barHeight ? Part::bar : Part::none;
    }

    void setHover (Part part)
    {
        if (part == hover)
            return;

        hover = part;
        repaint();
    }

    void paintButton (juce::Graphics& g, Part part)
    {
        const auto area = areaOf (part).toFloat();

        if (hover == part)
        {
            g.setColour (part == Part::close ? red.withAlpha (0.85f) : juce::Colour (0xff34373e));
            g.fillRoundedRectangle (area, 4.0f);
        }

        const auto c = area.getCentre();
        g.setColour (hover == part ? juce::Colours::white : juce::Colour (0xffc9ccd1));
        const auto& mode = controller.setup.monitorMode;

        if (part == Part::minimise)
        {
            g.fillRect (juce::Rectangle<float> (10.0f, 1.6f).withCentre (c.translated (0.0f, 3.0f)));
        }
        else if (part == Part::maximise)
        {
            if (mode == "max")
            {
                g.drawRect (juce::Rectangle<float> (8.0f, 7.0f).withCentre (c.translated (-1.5f, 1.5f)), 1.2f);
                g.drawRect (juce::Rectangle<float> (8.0f, 7.0f).withCentre (c.translated (1.5f, -1.5f)), 1.2f);
            }
            else
            {
                g.drawRect (juce::Rectangle<float> (10.0f, 8.0f).withCentre (c), 1.3f);
            }
        }
        else
        {
            g.drawLine (c.x - 4.0f, c.y - 4.0f, c.x + 4.0f, c.y + 4.0f, 1.5f);
            g.drawLine (c.x + 4.0f, c.y - 4.0f, c.x - 4.0f, c.y + 4.0f, 1.5f);
        }
    }

    /** Where it belongs now: its own place and size (or minimised or maximised), on a screen
        that's there. */
    juce::Rectangle<int> boundsNow() const
    {
        const auto& displays = juce::Desktop::getInstance().getDisplays();
        auto kept = controller.setup.monitor;
        const auto* display = kept.isEmpty() ? displays.getPrimaryDisplay() : displays.getDisplayForRect (kept);

        if (display == nullptr)
            display = displays.getPrimaryDisplay();

        if (display == nullptr)
            return { 100, 100, 480, heightFor (480) };

        const auto area = display->userBounds.toNearestInt();

        if (kept.isEmpty() || area.getIntersection (kept).getWidth() < 120 || area.getIntersection (kept).getHeight() < barHeight)
            kept = juce::Rectangle<int> (480, heightFor (480)).withPosition (area.getRight() - 480 - 24, area.getY() + 24);

        const auto width = juce::jlimit (narrowest, juce::jmax (narrowest, area.getWidth()), kept.getWidth());
        const auto& mode = controller.setup.monitorMode;

        if (mode == "max")
        {
            const auto most = juce::jmin (area.getWidth(), juce::roundToInt ((area.getHeight() - barHeight) * 16.0 / 9.0));
            return juce::Rectangle<int> (most, heightFor (most)).withCentre (area.getCentre());
        }

        if (mode == "min")
            return kept.withSize (width, barHeight);

        return kept.withSize (width, heightFor (width));
    }

    void setMode (const juce::String& mode)
    {
        controller.setup.monitorMode = mode;
        setBounds (boundsNow());
        controller.saveSoon();
        repaint();
    }

    /** Its own place and size, moved or sized by hand (not while minimised or maximised). */
    void keepPlace()
    {
        const auto& mode = controller.setup.monitorMode;

        if (mode == "max")
            return;

        controller.setup.monitor = mode == "min" ? getBounds().withHeight (heightFor (getWidth())) : getBounds();
        controller.saveSoon();
    }

    /** Gone just after (not inside its own button's call); hidden at once. */
    void close()
    {
        setVisible (false);
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<Monitor> (this)]
        {
            if (safe != nullptr)
                safe->controller.showMonitor (false);
        });
    }

    void timerCallback() override
    {
        if (! isVisible())
            return;

        bool isLive = false;
        const auto words = controller.host.controllerStatus (isLive);

        if (words != statusText || isLive != live)
        {
            statusText = words;
            live = isLive;
            repaint (getLocalBounds().removeFromTop (barHeight));
        }

        if (controller.setup.monitorMode != "min")
            repaint (getLocalBounds().withTrimmedTop (barHeight));
    }

    Controller& controller;
    Part hover = Part::none, pressed = Part::none;
    bool moving = false, sizing = false;
    juce::Rectangle<int> startBounds;
    juce::Point<int> startMouse;
    juce::String statusText { "Not live" };
    bool live = false;
};

//==============================================================================
Controller::Controller (Host& h, const juce::File& setupFile, const Setup& fallback)
    : host (h), file (setupFile)
{
    const auto kept = juce::JSON::parse (file);
    setup = kept.isObject() ? Setup::fromVar (kept) : fallback;
    oneMain (setup.rows);
    openLights();
    startTimer (200);
}

Controller::~Controller()
{
    stopTimer();
    monitor = nullptr;
    window = nullptr;

    if (saveDue)
        save();

    sendLightsOff();   // the pads' lights go out with it
}

std::array<LiveStream::Source, numRows> Controller::getSources() const
{
    std::array<LiveStream::Source, numRows> sources;

    for (size_t i = 0; i < sources.size(); ++i)
    {
        const auto& row = setup.rows[i];
        sources[i].id = row.device;
        sources[i].show = row.device.isEmpty() ? Show::hidden : row.show;
        sources[i].box = row.box;
        sources[i].tallBox = row.tallBox;
        sources[i].tallFocus = row.tallFocus;
        sources[i].delayMs = row.delayMs;
    }

    return sources;
}

//==============================================================================
void Controller::setShow (int row, Show show)
{
    if (! juce::isPositiveAndBelow (row, numRows))
        return;

    auto& chosen = setup.rows[static_cast<size_t> (row)];

    if (chosen.device.isEmpty() && show != Show::hidden)
    {
        say ("Pick " + (row == screenRow ? juce::String ("a screen") : juce::String ("a camera")) + " for " + rowName (row) + " first.");
        return;
    }

    if (chosen.show == show)
        return;

    // One Main at a time: the one before goes to Hidden.
    if (show == Show::main)
        for (auto& other : setup.rows)
            if (other.show == Show::main)
                other.show = Show::hidden;

    chosen.show = show;
    presetChanged = setup.preset.isNotEmpty();
    changed (true);
}

void Controller::setBox (int row, juce::Rectangle<float> box)
{
    if (! juce::isPositiveAndBelow (row, numRows))
        return;

    auto w = juce::jlimit (smallestBox, 1.0f, box.getWidth());
    auto h = juce::jlimit (smallestBox, 1.0f, box.getHeight());
    const juce::Rectangle<float> kept (juce::jlimit (0.0f, 1.0f - w, box.getX()), juce::jlimit (0.0f, 1.0f - h, box.getY()), w, h);
    auto& chosen = setup.rows[static_cast<size_t> (row)];

    if (chosen.box == kept)
        return;

    chosen.box = kept;
    presetChanged = setup.preset.isNotEmpty();
    changed (true);
}

void Controller::setTallBox (int row, juce::Rectangle<float> box)
{
    if (! juce::isPositiveAndBelow (row, numRows))
        return;

    auto w = juce::jlimit (smallestBox, 1.0f, box.getWidth());
    auto h = juce::jlimit (smallestBox, 1.0f, box.getHeight());
    const juce::Rectangle<float> kept (juce::jlimit (0.0f, 1.0f - w, box.getX()), juce::jlimit (0.0f, 1.0f - h, box.getY()), w, h);
    auto& chosen = setup.rows[static_cast<size_t> (row)];

    if (chosen.tallBox == kept)
        return;

    chosen.tallBox = kept;
    presetChanged = setup.preset.isNotEmpty();
    changed (true);
}

void Controller::setTallFocus (int row, float focus)
{
    if (! juce::isPositiveAndBelow (row, numRows) || ! std::isfinite (focus))
        return;

    auto& chosen = setup.rows[static_cast<size_t> (row)];
    const auto kept = juce::jlimit (0.0f, 1.0f, focus);

    if (std::abs (chosen.tallFocus - kept) < 0.0005f)
        return;

    chosen.tallFocus = kept;
    presetChanged = setup.preset.isNotEmpty();
    changed (true);
}

void Controller::setViewTall (bool tall)
{
    if (setup.viewTall == tall)
        return;

    setup.viewTall = tall;
    saveSoon();
    host.controllerViewChanged();
}

void Controller::setDevice (int row, const juce::String& device)
{
    if (! juce::isPositiveAndBelow (row, numRows))
        return;

    auto& chosen = setup.rows[static_cast<size_t> (row)];

    if (chosen.device == device)
        return;

    chosen.device = device;

    if (device.isEmpty())
        chosen.show = Show::hidden;

    changed (true);
}

void Controller::setDelay (int row, int ms)
{
    if (! juce::isPositiveAndBelow (row, numRows) || row == screenRow)
        return;

    auto& chosen = setup.rows[static_cast<size_t> (row)];
    const auto kept = juce::jlimit (0, LiveStream::maxDelayMs, ms);

    if (chosen.delayMs == kept)
        return;

    chosen.delayMs = kept;
    changed (true);
}

//==============================================================================
juce::File Controller::getPresetsFolder()
{
    return UserData::getFolder().getChildFile ("Live Controller");
}

juce::File Controller::presetFile (const juce::String& name) const
{
    return getPresetsFolder().getChildFile (juce::File::createLegalFileName (name) + presetExtension);
}

juce::StringArray Controller::getPresets() const
{
    juce::StringArray names;

    for (const auto& found : getPresetsFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + presetExtension))
        names.add (found.getFileNameWithoutExtension());

    names.sortNatural();
    return names;
}

bool Controller::loadPreset (const juce::String& name)
{
    const auto value = juce::JSON::parse (presetFile (name));
    const auto* list = value.getProperty ("rows", {}).getArray();

    if (list == nullptr)
    {
        say (name + " isn't a preset Sonify can read.");
        return false;
    }

    for (int r = 0; r < juce::jmin (numRows, list->size()); ++r)
    {
        const auto& item = (*list)[r];
        auto& row = setup.rows[static_cast<size_t> (r)];
        row.show = showFromName (item.getProperty ("show", "hidden").toString());
        row.box = boxFromVar (item.getProperty ("box", {}), row.box);
        row.tallBox = boxFromVar (item.getProperty ("tallBox", {}), row.tallBox);
        row.tallFocus = focusFromVar (item.getProperty ("focus", {}), row.tallFocus);
    }

    oneMain (setup.rows);
    setup.preset = name;
    presetChanged = false;
    changed (true);
    say ("Preset: " + name + ".");
    return true;
}

bool Controller::savePreset (const juce::String& name)
{
    juce::DynamicObject::Ptr object (new juce::DynamicObject());
    object->setProperty ("format", formatVersion);
    juce::Array<juce::var> list;

    for (int r = 0; r < numRows; ++r)
    {
        const auto& row = setup.rows[static_cast<size_t> (r)];
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("row", rowName (r));
        item->setProperty ("show", showName (row.show));
        item->setProperty ("box", boxToVar (row.box));
        item->setProperty ("tallBox", boxToVar (row.tallBox));
        item->setProperty ("focus", std::round (row.tallFocus * 1000.0f) / 1000.0);
        list.add (juce::var (item.get()));
    }

    object->setProperty ("rows", list);
    const auto target = presetFile (name);
    target.getParentDirectory().createDirectory();

    if (! target.replaceWithText (juce::JSON::toString (juce::var (object.get()))))
        return false;

    setup.preset = target.getFileNameWithoutExtension();
    presetChanged = false;
    changed (false);
    return true;
}

bool Controller::renamePreset (const juce::String& from, const juce::String& to)
{
    const auto source = presetFile (from);
    const auto target = presetFile (to);

    if (! source.existsAsFile() || target.exists() || ! source.moveFileTo (target))
        return false;

    const auto newName = target.getFileNameWithoutExtension();

    if (const auto found = setup.pads.find ("preset." + from); found != setup.pads.end())
    {
        const auto key = found->second;
        setup.pads.erase (found);
        setup.pads["preset." + newName] = key;
    }

    if (setup.preset == from)
        setup.preset = newName;

    changed (false);
    return true;
}

bool Controller::deletePreset (const juce::String& name)
{
    forget ("preset." + name);

    if (! presetFile (name).deleteFile())
        return false;

    if (setup.preset == name)
    {
        setup.preset.clear();
        presetChanged = false;
    }

    changed (false);
    say (name + " is deleted.");
    return true;
}

//==============================================================================
std::optional<MidiLink::Source> Controller::sourceOf (const juce::String& port, const juce::MidiMessage& message, bool& down)
{
    MidiLink::Source source;
    source.port = port;
    source.channel = message.getChannel();

    if (message.isController())
    {
        source.kind = MidiLink::Kind::cc;
        source.number = message.getControllerNumber();
        down = message.getControllerValue() >= 64;   // a button sends 127 pressed, 0 let go
    }
    else if (message.isNoteOnOrOff())
    {
        source.kind = MidiLink::Kind::note;
        source.number = message.getNoteNumber();
        down = message.isNoteOn();
    }
    else
    {
        return std::nullopt;
    }

    if (source.channel < 1 || source.channel > 16)
        return std::nullopt;

    return source;
}

juce::String Controller::actionOf (const MidiLink::Source& source) const
{
    const auto key = source.key();

    for (const auto& [action, bound] : setup.pads)
        if (bound == key)
            return action;

    return {};
}

bool Controller::handleMidi (const juce::String& port, const juce::MidiMessage& message)
{
    if (setup.midiPort.isEmpty() || port != setup.midiPort)
        return false;

    bool down = false;
    const auto source = sourceOf (port, message, down);

    if (! source.has_value())
        return false;

    // Learning: the next pad pressed is the armed button's (one pad, one button).
    if (learning.isNotEmpty())
    {
        if (! down)
            return true;

        const auto key = source->key();

        for (auto it = setup.pads.begin(); it != setup.pads.end();)
            it = it->second == key ? setup.pads.erase (it) : std::next (it);

        setup.pads[learning] = key;
        say (actionText (learning) + juce::String (juce::CharPointer_UTF8 (" \xe2\x86\x90 ")) + source->label());
        learning.clear();
        litSent.clear();
        saveSoon();

        if (onPadsChanged != nullptr)
            onPadsChanged();

        sendLights (true);
        refresh();
        return true;
    }

    const auto action = actionOf (*source);

    if (action.isEmpty())
        return false;

    if (down)
        perform (action);

    return true;
}

void Controller::perform (const juce::String& action)
{
    const auto parts = juce::StringArray::fromTokens (action, ".", "");

    if (parts.size() == 3 && parts[0] == "row")
        setShow (parts[1].getIntValue() - 1, showFromName (parts[2]));
    else if (action.startsWith ("preset."))
        loadPreset (action.fromFirstOccurrenceOf ("preset.", false, false));
    else if (action.startsWith ("switch."))
    {
        const auto which = action.fromFirstOccurrenceOf ("switch.", false, false);
        host.controllerSetSwitch (which, ! host.controllerSwitch (which));
    }
    else if (action == "show.monitor")
    {
        showMonitor (! isMonitorShown());
    }

    sendLights (false);
    refresh();
}

bool Controller::isLit (const juce::String& action)
{
    const auto parts = juce::StringArray::fromTokens (action, ".", "");

    if (parts.size() == 3 && parts[0] == "row")
    {
        const auto row = parts[1].getIntValue() - 1;

        if (! juce::isPositiveAndBelow (row, numRows))
            return false;

        const auto& r = setup.rows[static_cast<size_t> (row)];
        return r.device.isNotEmpty() && r.show == showFromName (parts[2]);
    }

    if (action.startsWith ("preset."))
        return setup.preset == action.fromFirstOccurrenceOf ("preset.", false, false) && ! presetChanged;

    if (action.startsWith ("switch."))
        return host.controllerSwitch (action.fromFirstOccurrenceOf ("switch.", false, false));

    if (action == "show.monitor")
        return isMonitorShown();

    return false;
}

void Controller::setMidiPort (const juce::String& port)
{
    if (port == setup.midiPort)
        return;

    sendLightsOff();
    setup.midiPort = port;
    learning.clear();

    if (port.isNotEmpty())
        host.controllerMidiPortChosen (port);

    openLights();
    saveSoon();

    if (onPadsChanged != nullptr)
        onPadsChanged();

    say (port.isEmpty() ? juce::String ("No MIDI device: the pads rest.") : "Listening to " + port + ".");
}

void Controller::setLights (bool on)
{
    if (on == setup.lights)
        return;

    if (! on)
        sendLightsOff();

    setup.lights = on;
    openLights();
    saveSoon();
}

void Controller::setLearnMode (bool on)
{
    learnMode = on;

    if (! on)
        learning.clear();

    refresh();
}

void Controller::learn (const juce::String& action)
{
    learning = learning == action ? juce::String() : action;

    if (learning.isNotEmpty())
        say ("Press a pad on " + setup.midiPort + " for " + actionText (action) + ".");
}

void Controller::forget (const juce::String& action)
{
    const auto found = setup.pads.find (action);

    if (found == setup.pads.end())
        return;

    // Its light goes out first.
    if (lights != nullptr)
        if (const auto source = MidiLink::Source::fromKey (found->second); source.has_value())
            sendLight (*source, false);

    setup.pads.erase (found);
    litSent.erase (action);

    if (learning == action)
        learning.clear();

    saveSoon();

    if (onPadsChanged != nullptr)
        onPadsChanged();

    say (actionText (action) + ": its pad is forgotten.");
    refresh();
}

juce::String Controller::padLabel (const juce::String& action) const
{
    const auto found = setup.pads.find (action);

    if (found == setup.pads.end())
        return {};

    const auto source = MidiLink::Source::fromKey (found->second);
    return source.has_value() ? source->label() : juce::String();
}

std::vector<MidiLink::Source> Controller::getPadSources() const
{
    std::vector<MidiLink::Source> sources;

    for (const auto& [action, key] : setup.pads)
    {
        juce::ignoreUnused (action);

        if (const auto source = MidiLink::Source::fromKey (key); source.has_value() && source->port == setup.midiPort)
            sources.push_back (*source);
    }

    return sources;
}

//==============================================================================
void Controller::openLights()
{
    const auto wanted = setup.lights && setup.midiPort.isNotEmpty() ? setup.midiPort : juce::String();

    if (wanted == lightsPort && (lights != nullptr || wanted.isEmpty()))
        return;

    lights = nullptr;
    lightsPort.clear();
    litSent.clear();

    if (wanted.isEmpty())
        return;

    // Its MIDI output: the one with its name (Windows may put MIDIIN / MIDIOUT round them).
    const auto device = deviceNameOf (wanted);

    for (const auto& output : juce::MidiOutput::getAvailableDevices())
    {
        if (output.name == wanted || deviceNameOf (output.name) == device)
        {
            lights = juce::MidiOutput::openDevice (output.identifier);

            if (lights != nullptr)
            {
                lightsPort = wanted;
                sendLights (true);
            }

            return;
        }
    }
}

void Controller::sendLight (const MidiLink::Source& source, bool lit)
{
    if (lights == nullptr)
        return;

    if (source.kind == MidiLink::Kind::note)
        lights->sendMessageNow (juce::MidiMessage::noteOn (source.channel, source.number, static_cast<juce::uint8> (lit ? 127 : 0)));
    else if (source.kind == MidiLink::Kind::cc)
        lights->sendMessageNow (juce::MidiMessage::controllerEvent (source.channel, source.number, lit ? 127 : 0));
}

void Controller::sendLights (bool all)
{
    if (lights == nullptr)
        return;

    for (const auto& [action, key] : setup.pads)
    {
        const auto source = MidiLink::Source::fromKey (key);

        if (! source.has_value() || source->port != setup.midiPort)
            continue;

        const auto lit = isLit (action);
        const auto sent = litSent.find (action);

        if (! all && sent != litSent.end() && sent->second == lit)
            continue;

        sendLight (*source, lit);
        litSent[action] = lit;
    }
}

void Controller::sendLightsOff()
{
    if (lights == nullptr)
        return;

    for (const auto& [action, key] : setup.pads)
    {
        juce::ignoreUnused (action);

        if (const auto source = MidiLink::Source::fromKey (key); source.has_value() && source->port == setup.midiPort)
            sendLight (*source, false);
    }

    litSent.clear();
}

//==============================================================================
void Controller::showWindow (bool shouldShow)
{
    if (! shouldShow)
    {
        if (learnMode)
            setLearnMode (false);

        window = nullptr;
        return;
    }

    if (window == nullptr)
    {
        window = std::make_unique<Window> (*this);
        return;
    }

    if (window->isMinimised())
        window->setMinimised (false);

    window->setVisible (true);
    window->toFront (true);
}

void Controller::showMonitor (bool shouldShow)
{
    if (! shouldShow)
    {
        if (monitor == nullptr)
            return;

        monitor = nullptr;
    }
    else if (monitor == nullptr)
    {
        monitor = std::make_unique<Monitor> (*this);
        monitor->open();
    }
    else
    {
        monitor->reopen();
    }

    host.controllerMonitorChanged();
    sendLights (false);
    refresh();
}

void Controller::refresh()
{
    if (window != nullptr)
        window->getPanel().refreshLit();
}

void Controller::say (const juce::String& words)
{
    if (window != nullptr)
        window->getPanel().setNote (words);
}

void Controller::changed (bool rows)
{
    if (rows)
        host.controllerChanged();

    saveSoon();
    sendLights (false);
    refresh();
}

void Controller::saveSoon()
{
    saveDue = true;
    changedAt = juce::Time::getMillisecondCounterHiRes();
}

void Controller::save()
{
    saveDue = false;
    file.getParentDirectory().createDirectory();
    file.replaceWithText (juce::JSON::toString (setup.toVar()));
}

void Controller::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();

    // Kept a moment after the last change (a box being dragged is many).
    if (saveDue && now - changedAt > 800.0)
        save();

    // The switches may move elsewhere (the page, MIDI Link): the pads' lights follow.
    sendLights (false);

    // The device plugged in after: its lights found now and then.
    if (setup.lights && setup.midiPort.isNotEmpty() && lights == nullptr && now - lightsTried > 3000.0)
    {
        lightsTried = now;
        lightsPort.clear();
        openLights();
    }
}
}

//==============================================================================
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#endif

void LiveControl::hideFromCaptures (juce::Component& window)
{
   #if JUCE_WINDOWS
    // Left out of every screen capture (Windows 10 2004 and later); older Windows shows it black there.
    if (auto* peer = window.getPeer())
    {
        const auto handle = static_cast<HWND> (peer->getNativeHandle());

        if (! SetWindowDisplayAffinity (handle, 0x00000011 /* WDA_EXCLUDEFROMCAPTURE */))
            SetWindowDisplayAffinity (handle, 0x00000001 /* WDA_MONITOR */);
    }
   #else
    juce::ignoreUnused (window);
   #endif
}
