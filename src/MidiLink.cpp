// D:\workspace\Fanan_web_hub\src\MidiLink.cpp

#include "MidiLink.h"
#include "AnalogBoard.h"
#include "LiveShow.h"
#include "Preferences.h"

#include <algorithm>

//==============================================================================
namespace
{
    const char* kindName (MidiLink::Kind kind)
    {
        switch (kind)
        {
            case MidiLink::Kind::note:      return "note";
            case MidiLink::Kind::pitchBend: return "pb";
            case MidiLink::Kind::cc:        break;
        }

        return "cc";
    }

    /** "track.3", "fx.1", "bus.2": a channel by its number among its kind, from 1. */
    bool isChannelKey (const juce::StringArray& parts)
    {
        return parts.size() >= 3 && (parts[0] == "track" || parts[0] == "fx" || parts[0] == "bus")
            && parts[1].containsOnly ("0123456789") && parts[1].getIntValue() >= 1;
    }

    /** Focus: the selected track's own controls, whichever track it is. 1 a fader or knob, 0 a
        button, -1 not one:
            focus.volume|pan, focus.send.<k>, focus.knob.<k> (1 .. 8)    faders and knobs
            focus.mute|solo|arm|monitor                                   buttons */
    int focusKindOf (const juce::String& key)
    {
        const auto parts = juce::StringArray::fromTokens (key, ".", "");
        const auto numbered = [&parts] (int most) { return parts.size() == 3 && parts[2].containsOnly ("0123456789") && parts[2].getIntValue() >= 1
                                                           && parts[2].getIntValue() <= most; };

        if (parts.size() < 2 || parts[0] != "focus")
            return -1;

        if (parts.size() == 2)
            return parts[1] == "volume" || parts[1] == "pan" ? 1
                 : parts[1] == "mute" || parts[1] == "solo" || parts[1] == "arm" || parts[1] == "monitor" ? 0 : -1;

        if (parts[1] == "send")
            return numbered (64) ? 1 : -1;

        return parts[1] == "knob" && numbered (8) ? 1 : -1;
    }

    /** Song Blocks' Live (the clip launcher), by place in its grid:
            launch.cell.<row>.<column>   a cell: the row-th block on the column-th track
            launch.row.<row>             a block's name: all of it
            launch.stop.<column>         a track's stop
            launch.stopall               Stop All */
    bool isLaunchKey (const juce::String& key)
    {
        const auto parts = juce::StringArray::fromTokens (key, ".", "");
        const auto number = [&parts] (int i) { return parts[i].containsOnly ("0123456789") && parts[i].getIntValue() >= 1; };

        if (parts.size() < 2 || parts[0] != "launch")
            return false;

        if (parts[1] == "stopall")
            return parts.size() == 2;

        if (parts[1] == "row" || parts[1] == "stop")
            return parts.size() == 3 && number (2);

        return parts[1] == "cell" && parts.size() == 4 && number (2) && number (3);
    }

    /** "board.<n>.<setting>" (a track's strip) or "board.master.<setting>": the setting; null otherwise. */
    const AnalogBoard::ParamSpec* boardSpecOf (const juce::String& key)
    {
        const auto parts = juce::StringArray::fromTokens (key, ".", "");

        if (parts.size() != 3 || parts[0] != "board")
            return nullptr;

        if (parts[1] == "master")
        {
            const auto index = AnalogBoard::masterIndexOf (parts[2]);
            return index >= 0 ? &AnalogBoard::masterSpecs()[static_cast<size_t> (index)] : nullptr;
        }

        if (! parts[1].containsOnly ("0123456789") || parts[1].getIntValue() < 1)
            return nullptr;

        const auto index = AnalogBoard::stripIndexOf (parts[2]);
        return index >= 0 ? &AnalogBoard::stripSpecs()[static_cast<size_t> (index)] : nullptr;
    }

    /** The Live Show's: 1 a knob or fader, 0 a button, -1 not one of its controls.
            live.deck.<n>.<setting>|play|rewind|hotcue<k>|loop<k>        a deck (1 .. 4)
            live.channel.<n>.<setting>                                    the show's n-th channel
            live.master.<setting>                                         the mixer, headphones, ducker
            live.pad.<n>, live.onair, live.mute (both mutes), live.mutemics, live.muteplayback,
            live.record */
    int liveKindOf (const juce::String& key)
    {
        const auto parts = juce::StringArray::fromTokens (key, ".", "");

        if (parts.isEmpty() || parts[0] != "live")
            return -1;

        if (parts.size() == 2)
            return parts[1] == "onair" || parts[1] == "mute" || parts[1] == "mutemics" || parts[1] == "muteplayback"
                   || parts[1] == "record" ? 0 : -1;

        const auto numbered = [&parts] (int most)
        {
            return parts[2].containsOnly ("0123456789") && parts[2].getIntValue() >= 1 && parts[2].getIntValue() <= most;
        };

        const auto kindOf = [] (const LiveShow::ParamSpec& spec) { return spec.kind == LiveShow::ParamSpec::Kind::toggle ? 0 : 1; };

        if (parts[1] == "pad")
            return parts.size() == 3 && numbered (LiveShow::numPads) ? 0 : -1;

        if (parts[1] == "master" && parts.size() == 3)
        {
            const auto index = LiveShow::masterIndexOf (parts[2]);
            return index >= 0 ? kindOf (LiveShow::masterSpecs()[static_cast<size_t> (index)]) : -1;
        }

        if (parts.size() != 4)
            return -1;

        if (parts[1] == "deck" && numbered (LiveShow::maxDecks))
        {
            if (const auto index = LiveShow::deckIndexOf (parts[3]); index >= 0)
                return kindOf (LiveShow::deckSpecs()[static_cast<size_t> (index)]);

            static const juce::StringArray actions { "play", "rewind", "hotcue1", "hotcue2", "hotcue3", "hotcue4",
                                                     "loop1", "loop2", "loop3", "loop4" };
            return actions.contains (parts[3]) ? 0 : -1;
        }

        if (parts[1] == "channel" && numbered (999))
        {
            const auto index = LiveShow::channelIndexOf (parts[3]);
            return index >= 0 ? kindOf (LiveShow::channelSpecs()[static_cast<size_t> (index)]) : -1;
        }

        return -1;
    }
}

//==============================================================================
juce::String MidiLink::Source::key() const
{
    return juce::String (kindName (kind)) + "|" + juce::String (channel) + "|" + juce::String (number) + "|" + port;
}

std::optional<MidiLink::Source> MidiLink::Source::fromKey (const juce::String& key)
{
    // The port's name comes last, so a "|" in it is kept.
    const auto kindText = key.upToFirstOccurrenceOf ("|", false, false);
    auto rest = key.fromFirstOccurrenceOf ("|", false, false);
    const auto channelText = rest.upToFirstOccurrenceOf ("|", false, false);
    rest = rest.fromFirstOccurrenceOf ("|", false, false);
    const auto numberText = rest.upToFirstOccurrenceOf ("|", false, false);
    const auto port = rest.fromFirstOccurrenceOf ("|", false, false);

    Source source;
    source.kind = kindText == "note" ? Kind::note : kindText == "pb" ? Kind::pitchBend : Kind::cc;
    source.channel = channelText.getIntValue();
    source.number = numberText.getIntValue();
    source.port = port;

    if ((kindText != "cc" && kindText != "note" && kindText != "pb") || port.isEmpty()
        || ! channelText.containsOnly ("0123456789") || ! numberText.containsOnly ("0123456789")
        || source.channel < 1 || source.channel > 16 || ! juce::isPositiveAndBelow (source.number, 128))
        return std::nullopt;

    if (source.kind == Kind::pitchBend)
        source.number = 0;

    return source;
}

juce::String MidiLink::Source::label() const
{
    if (kind == Kind::pitchBend)
        return "Pitch Bend";

    if (kind == Kind::note)   // middle C (60) is C4, as in the Editor
        return "Note " + juce::String (number) + " (" + juce::MidiMessage::getMidiNoteName (number, true, true, 4) + ")";

    return "CC " + juce::String (number);
}

bool MidiLink::Source::operator== (const Source& other) const noexcept
{
    return kind == other.kind && channel == other.channel && number == other.number && port == other.port;
}

//==============================================================================
juce::String MidiLink::Target::id() const
{
    return key == "plugin" ? "plugin|" + pluginId + "|" + juce::String (static_cast<juce::int64> (paramId)) : key;
}

bool MidiLink::Target::isContinuous() const
{
    if (key == "plugin" || key == "main.volume")
        return true;

    if (key.startsWith ("launch."))   // pads: each press launches
        return false;

    if (key.startsWith ("focus."))
        return focusKindOf (key) == 1;

    // The Analog Board's knobs and faders; its buttons aren't.
    if (const auto board = boardSpecOf (key))
        return board->kind != AnalogBoard::ParamSpec::Kind::toggle;

    // The Live Show's, likewise.
    if (key.startsWith ("live."))
        return liveKindOf (key) == 1;

    const auto parts = juce::StringArray::fromTokens (key, ".", "");
    return isChannelKey (parts) && (parts[2] == "volume" || parts[2] == "pan" || parts[2] == "send");
}

bool MidiLink::Target::isValid() const
{
    if (key == "plugin")
        return pluginId.isNotEmpty();

    if (key == "main.volume" || key == "main.mute")
        return true;

    if (key.startsWith ("transport."))
        return juce::StringArray { "play", "stop", "record", "loop", "metronome", "start" }.contains (key.fromFirstOccurrenceOf (".", false, false));

    if (key.startsWith ("board."))
        return boardSpecOf (key) != nullptr;

    if (key.startsWith ("live."))
        return liveKindOf (key) >= 0;

    if (key.startsWith ("launch."))
        return isLaunchKey (key);

    if (key.startsWith ("focus."))
        return focusKindOf (key) >= 0;

    const auto parts = juce::StringArray::fromTokens (key, ".", "");

    if (! isChannelKey (parts))
        return false;

    const auto& control = parts[2];

    if (control == "send")
        return parts.size() == 4 && parts[3].containsOnly ("0123456789") && parts[3].getIntValue() >= 1;

    if (parts.size() != 3)
        return false;

    // FX channels and buses have no recording and no monitoring.
    return control == "volume" || control == "pan" || control == "mute" || control == "solo"
        || (parts[0] == "track" && (control == "arm" || control == "monitor"));
}

juce::var MidiLink::Target::toVar() const
{
    juce::DynamicObject::Ptr object (new juce::DynamicObject());
    object->setProperty ("key", key);

    if (key == "plugin")
    {
        object->setProperty ("pluginId", pluginId);
        object->setProperty ("pluginName", pluginName);
        object->setProperty ("paramId", static_cast<juce::int64> (paramId));
        object->setProperty ("paramName", paramName);
    }

    return juce::var (object.get());
}

MidiLink::Target MidiLink::Target::fromVar (const juce::var& value)
{
    Target target;
    target.key = value.getProperty ("key", {}).toString();

    if (target.key == "plugin")
    {
        target.pluginId = value.getProperty ("pluginId", {}).toString();
        target.pluginName = value.getProperty ("pluginName", {}).toString();
        target.paramId = static_cast<juce::uint32> (static_cast<juce::int64> (value.getProperty ("paramId", 0)));
        target.paramName = value.getProperty ("paramName", {}).toString();
    }

    return target;
}

//==============================================================================
MidiLink::MidiLink (Host& h, TakeIncoming take)
    : host (h), takeIncoming (std::move (take))
{
    incoming.reserve (256);

    if (takeIncoming)
        startTimerHz (60);
}

MidiLink::~MidiLink()
{
    stopTimer();
}

void MidiLink::timerCallback()
{
    incoming.clear();
    takeIncoming (incoming);

    for (const auto& [port, message] : incoming)
        if (claim == nullptr || ! claim (port, message))
            receive (port, message);
}

void MidiLink::setDevices (std::map<juce::String, juce::String> names, juce::StringArray pads)
{
    if (names != deviceNames)
        stripChanged = true;   // chips show the devices' names

    deviceNames = std::move (names);
    padPorts = std::move (pads);
}

juce::String MidiLink::deviceOf (const juce::String& port) const
{
    const auto found = deviceNames.find (port);
    return found != deviceNames.end() && found->second.isNotEmpty() ? found->second : port;
}

//==============================================================================
void MidiLink::receive (const juce::String& port, const juce::MidiMessage& message)
{
    Source source;
    source.port = port;
    source.channel = message.getChannel();
    float value = 0.0f;
    bool down = false;

    if (message.isController())
    {
        source.kind = Kind::cc;
        source.number = message.getControllerNumber();
        value = static_cast<float> (message.getControllerValue()) / 127.0f;
        down = message.getControllerValue() >= 64;   // a button sends 127 pressed, 0 released
    }
    else if (message.isNoteOnOrOff())
    {
        source.kind = Kind::note;
        source.number = message.getNoteNumber();
        down = message.isNoteOn();
        value = down ? message.getFloatVelocity() : 0.0f;
    }
    else if (message.isPitchWheel())
    {
        source.kind = Kind::pitchBend;
        value = static_cast<float> (message.getPitchWheelValue()) / 16383.0f;
        down = value > 0.75f;                        // pushed up, well past the middle
    }
    else
    {
        return;
    }

    if (source.channel < 1 || source.channel > 16)
        return;

    // What the strip shows, held until another source sends (one knob turned is many messages
    // from one source); a key's note-off leaves its note there.
    {
        const auto found = std::find_if (recent.begin(), recent.end(), [&source] (const Recent& r) { return r.source == source; });

        if (found == recent.begin() && found != recent.end())
        {
            found->value = value;
        }
        else
        {
            if (found != recent.end())
                recent.erase (found);

            recent.insert (recent.begin(), { source, value });

            if (recent.size() > maxRecent)
                recent.resize (maxRecent);

            stripChanged = true;
        }
    }

    // Learn: a control waits; this is what moves it from now on (a key or a pad: its press), and
    // this message plays nothing.
    if (learning.has_value())
    {
        if (source.kind == Kind::note && ! down)
            return;

        const auto target = *learning;
        learning.reset();

        if (link (source, target, defaultModeFor (source, target)))
            learned = source;

        stripChanged = true;
        return;
    }

    // Its link, if it has one.
    for (size_t i = 0; i < links.size(); ++i)
    {
        const auto& link = links[i];

        if (link.source != source)
            continue;

        const auto fader = link.target.isContinuous();
        const auto pressed = down && ! held[i];
        const auto released = ! down && held[i];

        switch (link.mode)
        {
            case Mode::knob:        // follows the value; a button is on past the middle
                if (fader)
                    host.linkMove (link.target, value);
                else if (pressed || released)
                    host.linkSet (link.target, down);
                break;

            case Mode::push:        // Momentary: 1 while pressed, 0 when let go
                if (pressed || released)
                {
                    if (fader)
                        host.linkMove (link.target, down ? 1.0f : 0.0f);
                    else
                        host.linkSet (link.target, down);
                }
                break;

            case Mode::toggle:      // each press switches it
                if (pressed)
                {
                    if (fader)
                    {
                        raised[i] = ! raised[i];
                        host.linkMove (link.target, raised[i] ? 1.0f : 0.0f);
                    }
                    else
                    {
                        host.linkSet (link.target, ! host.linkIsOn (link.target));
                    }
                }
                break;

            case Mode::trigger:     // each press fires it once; letting go does nothing
                if (pressed)
                {
                    if (fader)
                        host.linkMove (link.target, 1.0f);
                    else
                        host.linkSet (link.target, true);
                }
                break;
        }

        held[i] = down;
        break;
    }
}

//==============================================================================
const MidiLink::Link* MidiLink::findLink (const Source& source) const
{
    const auto found = std::find_if (links.begin(), links.end(), [&source] (const Link& l) { return l.source == source; });
    return found != links.end() ? &*found : nullptr;
}

MidiLink::Mode MidiLink::defaultModeFor (const Target& target)
{
    // The clip launcher's: each press launches (Trigger).
    if (target.key.startsWith ("launch."))
        return Mode::trigger;

    return target.isContinuous() ? Mode::knob : Mode::toggle;
}

MidiLink::Mode MidiLink::defaultModeFor (const Source& source, const Target& target)
{
    // A key or a pad has no value to give a fader: it's all the way up while held.
    return target.isContinuous() && source.kind == Kind::note ? Mode::push : defaultModeFor (target);
}

bool MidiLink::link (const Source& source, const Target& target, Mode mode)
{
    if (! target.isValid() || source.port.isEmpty())
        return false;

    // One source moves one control.
    links.erase (std::remove_if (links.begin(), links.end(), [&source] (const Link& l) { return l.source == source; }), links.end());
    links.push_back ({ source, target, mode });
    linksChanged();
    return true;
}

bool MidiLink::setMode (const Source& source, Mode mode)
{
    for (auto& link : links)
    {
        if (link.source != source)
            continue;

        link.mode = mode;
        linksChanged();
        return true;
    }

    return false;
}

void MidiLink::unlinkTarget (const juce::String& targetId)
{
    const auto before = links.size();
    links.erase (std::remove_if (links.begin(), links.end(), [&targetId] (const Link& l) { return l.target.id() == targetId; }), links.end());

    if (links.size() != before)
        linksChanged();
}

void MidiLink::unlinkSource (const Source& source)
{
    const auto before = links.size();
    links.erase (std::remove_if (links.begin(), links.end(), [&source] (const Link& l) { return l.source == source; }), links.end());

    if (links.size() != before)
        linksChanged();
}

void MidiLink::clear()
{
    if (links.empty())
        return;

    links.clear();
    linksChanged();
}

void MidiLink::setLinks (std::vector<Link> newLinks)
{
    links.clear();

    // As link() would have them: well-formed, a source once.
    for (auto& link : newLinks)
    {
        if (! link.target.isValid() || link.source.port.isEmpty())
            continue;

        links.erase (std::remove_if (links.begin(), links.end(), [&link] (const Link& l) { return l.source == link.source; }), links.end());
        links.push_back (std::move (link));
    }

    linksChanged();
}

void MidiLink::linksChanged()
{
    held.assign (links.size(), false);
    raised.assign (links.size(), false);
    stripChanged = true;

    if (onLinkedSourcesChanged)
    {
        std::vector<Source> sources;

        for (const auto& link : links)
            sources.push_back (link.source);

        onLinkedSourcesChanged (sources);
    }

    if (autoFile != juce::File())
        save (autoFile);
}

//==============================================================================
juce::String MidiLink::modeName (Mode mode)
{
    switch (mode)
    {
        case Mode::push:   return "push";
        case Mode::toggle:  return "toggle";
        case Mode::trigger: return "trigger";
        case Mode::knob:    break;
    }

    return "knob";
}

MidiLink::Mode MidiLink::modeFromName (const juce::String& name, Mode fallback)
{
    return name == "push" ? Mode::push : name == "toggle" ? Mode::toggle : name == "knob" ? Mode::knob
         : name == "trigger" ? Mode::trigger : fallback;
}

juce::var MidiLink::toVar() const
{
    juce::Array<juce::var> list;

    for (const auto& link : links)
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("source", link.source.key());
        item->setProperty ("target", link.target.toVar());
        item->setProperty ("mode", modeName (link.mode));
        list.add (juce::var (item.get()));
    }

    juce::DynamicObject::Ptr root (new juce::DynamicObject());
    root->setProperty ("sonifyMidiLink", 1);
    root->setProperty ("links", list);
    return juce::var (root.get());
}

juce::Result MidiLink::fromVar (const juce::var& value)
{
    if (! value.isObject() || ! value.hasProperty ("sonifyMidiLink"))
        return juce::Result::fail ("This isn't a MIDI Link map.");

    std::vector<Link> loaded;

    if (const auto* list = value.getProperty ("links", {}).getArray())
    {
        for (const auto& item : *list)
        {
            const auto source = Source::fromKey (item.getProperty ("source", {}).toString());
            const auto target = Target::fromVar (item.getProperty ("target", {}));

            // A link this Sonify doesn't know is left out, not the whole map.
            if (! source.has_value() || ! target.isValid())
                continue;

            // No type written: the default for its control.
            loaded.push_back ({ *source, target, modeFromName (item.getProperty ("mode", {}).toString(), defaultModeFor (*source, target)) });
        }
    }

    setLinks (std::move (loaded));
    return juce::Result::ok();
}

bool MidiLink::save (const juce::File& file) const
{
    return file.getParentDirectory().createDirectory() && file.replaceWithText (juce::JSON::toString (toVar()));
}

juce::Result MidiLink::load (const juce::File& file)
{
    if (! file.existsAsFile())
        return juce::Result::fail ("There's no " + file.getFileName() + ".");

    juce::var parsed;

    if (const auto result = juce::JSON::parse (file.loadFileAsString(), parsed); result.failed())
        return juce::Result::fail (file.getFileName() + " isn't a MIDI Link map.");

    return fromVar (parsed);
}

juce::File MidiLink::getDefaultAutoFile()
{
    return SettingsFolder::file ("midilink.json", "FananWebHub-midilink.json");
}

juce::File MidiLink::getMapsFolder()
{
    return UserData::getFolder().getChildFile ("MIDI Link");
}
