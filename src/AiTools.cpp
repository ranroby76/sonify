// D:\workspace\Fanan_web_hub\src\AiTools.cpp

#include "AiTools.h"
#include "RecordingAnalysis.h"
#include "SessionPlayers.h"

namespace
{
    juce::var object()
    {
        return juce::var (new juce::DynamicObject());
    }

    juce::var failure (const juce::String& message)
    {
        auto result = object();
        result.getDynamicObject()->setProperty ("error", message);
        return result;
    }

    juce::var success()
    {
        auto result = object();
        result.getDynamicObject()->setProperty ("ok", true);
        return result;
    }

    double number (const juce::var& value, double fallback)
    {
        if (value.isInt() || value.isInt64() || value.isDouble())
            return static_cast<double> (value);

        if (value.isString() && value.toString().trim().containsOnly ("0123456789.-+eE"))
            return value.toString().getDoubleValue();

        return fallback;
    }

    // The tool list, as the model reads it.
    const char* toolJson = R"json([
{"name":"get_song","description":"The whole song at a glance: tempo, time signature, transport, Main's inserts, every track and FX channel with its id, instrument, inserts (with ids), sends, MIDI inputs and clips. Call it first, and again after changes when you need fresh ids.","input_schema":{"type":"object","properties":{}}},
{"name":"get_clip","description":"The notes of one MIDI clip: pitch, start and length in beats from the clip's start, velocity 1-127.","input_schema":{"type":"object","properties":{"track_id":{"type":"integer"},"clip_id":{"type":"integer"}},"required":["track_id","clip_id"]}},
{"name":"find_plugins","description":"Search the user's installed plugins by name, vendor or type (e.g. 'serum', 'compressor', 'reverb valhalla'). Use the returned name (or file and plugin_id) to load one.","input_schema":{"type":"object","properties":{"query":{"type":"string"},"kind":{"type":"string","enum":["instrument","effect","any"]}},"required":["query"]}},
{"name":"add_track","description":"Add a track. kind: instrument (plays MIDI through an instrument plugin), audio (plays and records audio). For effect returns use add_fx_channel.","input_schema":{"type":"object","properties":{"kind":{"type":"string","enum":["instrument","audio"]},"name":{"type":"string"}},"required":["kind"]}},
{"name":"remove_track","description":"Remove a track or FX channel. Only with confirmed: true, after the user said yes in this conversation; otherwise ask them first.","input_schema":{"type":"object","properties":{"track_id":{"type":"integer"},"confirmed":{"type":"boolean"}},"required":["track_id","confirmed"]}},
{"name":"set_track","description":"Change a track or FX channel: name, volume_db (-60..10), pan (-1 left..1 right), mute, solo, monitor (hear its live input), record_arm.","input_schema":{"type":"object","properties":{"track_id":{"type":"integer"},"name":{"type":"string"},"volume_db":{"type":"number"},"pan":{"type":"number"},"mute":{"type":"boolean"},"solo":{"type":"boolean"},"monitor":{"type":"boolean"},"record_arm":{"type":"boolean"}},"required":["track_id"]}},
{"name":"set_main","description":"Main output volume in dB (-60..10).","input_schema":{"type":"object","properties":{"volume_db":{"type":"number"}},"required":["volume_db"]}},
{"name":"load_instrument","description":"Load an instrument plugin onto an instrument track (replacing its instrument). plugin: its name as find_plugins gave it.","input_schema":{"type":"object","properties":{"track_id":{"type":"integer"},"plugin":{"type":"string"},"file":{"type":"string"},"plugin_id":{"type":"string"}},"required":["track_id"]}},
{"name":"add_effect","description":"Add an effect plugin to a channel's insert chain: channel is a track id, an FX channel id, or \"main\". position: 0 is first; omit to add last. Answers the new insert_id.","input_schema":{"type":"object","properties":{"channel":{},"plugin":{"type":"string"},"file":{"type":"string"},"plugin_id":{"type":"string"},"position":{"type":"integer"}},"required":["channel"]}},
{"name":"remove_effect","description":"Remove an insert from a channel. insert_id \"instrument\" removes a track's instrument (back to the Test Synth).","input_schema":{"type":"object","properties":{"channel":{},"insert_id":{}},"required":["channel","insert_id"]}},
{"name":"move_effect","description":"Move an insert to a position in its chain (0 is first).","input_schema":{"type":"object","properties":{"channel":{},"insert_id":{"type":"integer"},"position":{"type":"integer"}},"required":["channel","insert_id","position"]}},
{"name":"bypass_effect","description":"Switch an insert off (bypassed: true) or on.","input_schema":{"type":"object","properties":{"channel":{},"insert_id":{"type":"integer"},"bypassed":{"type":"boolean"}},"required":["channel","insert_id","bypassed"]}},
{"name":"get_plugin_params","description":"A plugin's parameters: index, name, value in its own range (min..max) and the value as text. slot: \"instrument\" or an insert id. Use filter (a word in the names) for plugins with many parameters; offset pages through long lists.","input_schema":{"type":"object","properties":{"channel":{},"slot":{},"filter":{"type":"string"},"offset":{"type":"integer"}},"required":["channel","slot"]}},
{"name":"set_plugin_params","description":"Set a plugin's parameters, each by index or by name, value in the parameter's own range (see get_plugin_params). Answers each parameter's new value as text.","input_schema":{"type":"object","properties":{"channel":{},"slot":{},"changes":{"type":"array","items":{"type":"object","properties":{"index":{"type":"integer"},"name":{"type":"string"},"value":{"type":"number"}},"required":["value"]}}},"required":["channel","slot","changes"]}},
{"name":"add_fx_channel","description":"Add an FX channel (a return for reverb, delay...): tracks send to it with add_send. Answers its id; add its effects with add_effect.","input_schema":{"type":"object","properties":{"name":{"type":"string"}}}},
{"name":"add_send","description":"Send a track's sound (after its fader) to an FX channel, at level_db (default 0).","input_schema":{"type":"object","properties":{"track_id":{"type":"integer"},"fx_channel_id":{"type":"integer"},"level_db":{"type":"number"}},"required":["track_id","fx_channel_id"]}},
{"name":"add_sidechain","description":"Send a track into a plugin's sidechain input (to duck a bass under the kick, say). The plugin must have a sidechain input (get_song shows has_sidechain) and sit on another channel.","input_schema":{"type":"object","properties":{"source_track_id":{"type":"integer"},"channel":{},"slot":{},"level_db":{"type":"number"}},"required":["source_track_id","channel","slot"]}},
{"name":"set_send","description":"Change a send: level_db, on.","input_schema":{"type":"object","properties":{"track_id":{"type":"integer"},"send_id":{"type":"integer"},"level_db":{"type":"number"},"on":{"type":"boolean"}},"required":["track_id","send_id"]}},
{"name":"remove_send","description":"Remove a send (an FX send or a sidechain send).","input_schema":{"type":"object","properties":{"track_id":{"type":"integer"},"send_id":{"type":"integer"}},"required":["track_id","send_id"]}},
{"name":"add_session_players","description":"Session players: a drummer, a bass player and a keys player who play a chord progression in a style, each on a new track (Drums, Bass, Keys) with a fitting installed instrument. Use it for a band backing instead of writing drums, bass and keys note by note. chords: bars between |, two chords in a bar split it, % repeats the bar before (\"C | Am | F G | %\"); or chords_from_track_id: the chords that track plays, from where it plays them. An audio track (a recording) is listened to for its tempo, beat and chords; when it has a tempo of its own, the song takes it and the song's audio moves onto the bars (recording_tempo: its tempo when the user says it). times: how many times the progression plays. Answers the new tracks.","input_schema":{"type":"object","properties":{"chords":{"type":"string"},"chords_from_track_id":{"type":"integer"},"recording_tempo":{"type":"number"},"style":{"type":"string","enum":["pop","rock","ballad","funk","house","hiphop"]},"intensity":{"type":"number","description":"0 calm .. 1 busy (0.5 when not given)"},"players":{"type":"array","items":{"type":"string","enum":["drums","bass","keys"]},"description":"all three when not given"},"instruments":{"type":"object","description":"an installed instrument's name for a player, instead of Sonify's pick","properties":{"drums":{"type":"string"},"bass":{"type":"string"},"keys":{"type":"string"}}},"start_bar":{"type":"number"},"times":{"type":"integer"}},"required":["style"]}},
{"name":"write_midi_clip","description":"Write a MIDI clip on an instrument track. start_bar counts from 1; length_bars its length. Each note: pitch (MIDI 0-127, 60 = middle C), start_beat (quarter notes from the clip's start), length_beats, velocity (1-127). replace: remove the track's clips that overlap it first.","input_schema":{"type":"object","properties":{"track_id":{"type":"integer"},"start_bar":{"type":"number"},"length_bars":{"type":"number"},"notes":{"type":"array","items":{"type":"object","properties":{"pitch":{"type":"integer"},"start_beat":{"type":"number"},"length_beats":{"type":"number"},"velocity":{"type":"integer"}},"required":["pitch","start_beat","length_beats"]}},"replace":{"type":"boolean"}},"required":["track_id","start_bar","length_bars","notes"]}},
{"name":"delete_clip","description":"Delete a clip from a track.","input_schema":{"type":"object","properties":{"track_id":{"type":"integer"},"clip_id":{"type":"integer"}},"required":["track_id","clip_id"]}},
{"name":"set_tempo","description":"The tempo in BPM (20-250). Without at_bar: the tempo that holds where the song is (with one tempo, the whole song's). at_bar: a tempo change from that bar (counted from 1, fractions are within the bar) on; ramp: the tempo glides there from the change before it. remove_at_bar: removes the tempo change at that bar.","input_schema":{"type":"object","properties":{"bpm":{"type":"number"},"at_bar":{"type":"number"},"ramp":{"type":"boolean"},"remove_at_bar":{"type":"number"}}}},
{"name":"set_time_signature","description":"A time signature, e.g. 4/4 or 6/8. Without at_bar: the one of the bar the song is in, from its change on; at_bar (whole bars, from 1): a change from that bar on. remove: removes the change at at_bar.","input_schema":{"type":"object","properties":{"numerator":{"type":"integer"},"denominator":{"type":"integer"},"at_bar":{"type":"integer"},"remove":{"type":"boolean"}},"required":["numerator","denominator"]}},
{"name":"set_key","description":"The song's key and scale (MIDI tools follow it), from at_bar on (whole bars from 1; the start when not given). root: C, C#, D ... B; scale: major, minor, dorian, phrygian, lydian, mixolydian, locrian, harmonic minor, melodic minor, major pentatonic, minor pentatonic, blues, chromatic. remove: removes the change at at_bar.","input_schema":{"type":"object","properties":{"root":{"type":"string"},"scale":{"type":"string"},"at_bar":{"type":"integer"},"remove":{"type":"boolean"}},"required":["root","scale"]}},
{"name":"transport","description":"play, stop, or rewind (to the start). Optional loop_start_bar and loop_end_bar set and switch on the loop; loop_off switches it off.","input_schema":{"type":"object","properties":{"action":{"type":"string","enum":["play","stop","rewind","none"]},"loop_start_bar":{"type":"number"},"loop_end_bar":{"type":"number"},"loop_off":{"type":"boolean"}},"required":["action"]}},
{"name":"open_plugin_window","description":"Open a plugin's window for the user to see.","input_schema":{"type":"object","properties":{"channel":{},"slot":{}},"required":["channel","slot"]}},
{"name":"save_song","description":"Save the song (to its file; a song never saved asks the user where).","input_schema":{"type":"object","properties":{}}},
{"name":"split_stems","description":"Split a recording into stems on this computer: vocals, drums, bass and other (everything else), and guitar and piano when asked for, each on a new audio track under the clip's track, where the clip starts. Guitar or piano splits with a second model (downloaded the first time, 110 MB), and other then holds what's left after them. The original track is muted (mute_original false keeps it playing). clip_id: the audio clip, needed when the track has more than one. stems: the ones wanted (vocals, drums, bass and other when not given). It takes minutes for a whole song, and the first time its model downloads too (98 MB): the stems arrive after this request ends, so tell the user and do nothing with them now.","input_schema":{"type":"object","properties":{"track_id":{"type":"integer"},"clip_id":{"type":"integer"},"stems":{"type":"array","items":{"type":"string","enum":["vocals","drums","bass","guitar","piano","other"]}},"mute_original":{"type":"boolean"}},"required":["track_id"]}}
])json";
}

AiTools::AiTools (AudioEngine& e, Library& l, Hooks h)
    : engine (e), library (l), hooks (std::move (h))
{
}

juce::var AiTools::definitions()
{
    static const juce::var parsed = []
    {
        auto list = juce::JSON::parse (juce::String (toolJson));

        // The MIDI FX tools, with every built-in kind and its settings spelled out.
        juce::StringArray kinds;
        juce::String settings;

        for (const auto& spec : MidiFx::types())
        {
            if (spec.magenta)
                continue;   // Magenta's models run on the page, for the user (the Browser's MIDI tab)

            kinds.add (spec.id);
            juce::StringArray parts;

            for (const auto& param : spec.params)
            {
                juce::String part = param.id + " ";

                if (! param.choices.isEmpty())
                    part << "(0.." << (param.choices.size() - 1) << ": " << param.choices.joinIntoString (", ") << ")";
                else if (param.unit == MidiFx::Unit::toggle)
                    part << "(0 off, 1 on)";
                else
                    part << "(" << param.min << ".." << param.max << ")";

                parts.add (part);
            }

            settings << spec.id << ": " << spec.description << " Settings: " << parts.joinIntoString ("; ") << ". ";
        }

        const auto kindsJson = juce::JSON::toString (juce::var (kinds), true);
        const auto add = [&list] (const juce::String& json) { list.getArray()->add (juce::JSON::parse (json)); };

        add ("{\"name\":\"add_midi_fx\",\"description\":" + juce::JSON::toString (juce::var (
                 "Add a MIDI FX device to an instrument track: its notes (clips and live) go through its devices, in order, before the instrument. "
                 "Devices that work in a key follow the song's key (set_key). position: 0 is first; omit to add last. At most 8. settings: by name, any left out "
                 "keep their defaults. Answers its fx_id. Kinds: " + settings)) +
             ",\"input_schema\":{\"type\":\"object\",\"properties\":{\"track_id\":{\"type\":\"integer\"},\"type\":{\"type\":\"string\",\"enum\":" + kindsJson +
             "},\"settings\":{\"type\":\"object\"},\"position\":{\"type\":\"integer\"}},\"required\":[\"track_id\",\"type\"]}}");
        add ("{\"name\":\"set_midi_fx\",\"description\":\"Change a MIDI FX device on a track: settings (by name, as add_midi_fx lists them), on (false switches it off).\","
             "\"input_schema\":{\"type\":\"object\",\"properties\":{\"track_id\":{\"type\":\"integer\"},\"fx_id\":{\"type\":\"integer\"},\"settings\":{\"type\":\"object\"},"
             "\"on\":{\"type\":\"boolean\"},\"position\":{\"type\":\"integer\"}},\"required\":[\"track_id\",\"fx_id\"]}}");
        add ("{\"name\":\"remove_midi_fx\",\"description\":\"Remove a MIDI FX device from a track.\",\"input_schema\":{\"type\":\"object\",\"properties\":"
             "{\"track_id\":{\"type\":\"integer\"},\"fx_id\":{\"type\":\"integer\"}},\"required\":[\"track_id\",\"fx_id\"]}}");
        add ("{\"name\":\"apply_midi_tool\",\"description\":\"Apply a MIDI tool (a MIDI FX kind, as add_midi_fx lists them) to a MIDI clip's notes once, for good: "
             "an arpeggio of its chords, chords from its notes, a fixed note length, notes kept in key, a Euclidean rhythm through the clip... The clip's notes become what the tool plays.\","
             "\"input_schema\":{\"type\":\"object\",\"properties\":{\"track_id\":{\"type\":\"integer\"},\"clip_id\":{\"type\":\"integer\"},\"type\":{\"type\":\"string\",\"enum\":"
             + kindsJson + "},\"settings\":{\"type\":\"object\"}},\"required\":[\"track_id\",\"clip_id\",\"type\"]}}");

        // The Analog Board: its settings by id, as set_board takes them.
        const auto describeSettings = [] (const auto& specs)
        {
            juce::StringArray parts;

            for (const auto& spec : specs)
            {
                juce::String part (spec.id);

                if (spec.kind == AnalogBoard::ParamSpec::Kind::toggle)
                    part << " (0 off, 1 on)";
                else if (spec.kind == AnalogBoard::ParamSpec::Kind::choice)
                    part << " (0.." << (spec.choiceNames().size() - 1) << ": " << spec.choiceNames().joinIntoString (", ") << ")";
                else
                    part << " (" << spec.minimum << ".." << spec.maximum << (juce::String (spec.unit).isNotEmpty() ? " " + juce::String (spec.unit) : juce::String()) << ")";

                parts.add (part);
            }

            return parts.joinIntoString ("; ");
        };

        add ("{\"name\":\"get_board\",\"description\":\"The Analog Board (a tab of its own): a second, analog-style mixer. Answers whether it mixes the song now, "
             "its master section, and each instrument and audio track's strip (settings away from where a fresh strip starts).\","
             "\"input_schema\":{\"type\":\"object\",\"properties\":{}}}");
        add ("{\"name\":\"set_board\",\"description\":" + juce::JSON::toString (juce::var (
                 "Set the Analog Board. While it mixes the song (mixing: true), each instrument and audio track's dry sound (before its inserts) goes through "
                 "its strip on the board, and the board's master plays on Main: the Console's inserts, faders, sends, FX channels, buses and Main are left out. "
                 "Switch mixing only when the user asks for the board. The first time it mixes, the strips start at the Console's faders. "
                 "strips: [{track_id, settings}]; master: settings; settings by id, any left out stay as they are; reset: true first puts every strip and the master "
                 "back to where they start; copy_console: true first gives the strips the Console's fader, pan, mute and solo. "
                 "A strip: preamp, high-pass, EQ (shelves LF and HF, bells LMF and HMF), compressor (on with compOn; threshold, ratio, release, fastAttack, makeup), "
                 "sends to the Plate reverb and the Tape Echo, pan (-1 left..1 right), volume (its fader), mute, solo. Strip settings: " + describeSettings (AnalogBoard::stripSpecs())
                 + ". Master settings (the Plate, the Tape Echo synced to the song or in ms, the Bus Compressor, the Tape Machine, the master fader): "
                 + describeSettings (AnalogBoard::masterSpecs()) + ".")) + ","
             "\"input_schema\":{\"type\":\"object\",\"properties\":{\"mixing\":{\"type\":\"boolean\"},\"reset\":{\"type\":\"boolean\"},"
             "\"copy_console\":{\"type\":\"boolean\"},\"master\":{\"type\":\"object\"},\"strips\":{\"type\":\"array\",\"items\":{\"type\":\"object\","
             "\"properties\":{\"track_id\":{\"type\":\"integer\"},\"settings\":{\"type\":\"object\"}},\"required\":[\"track_id\",\"settings\"]}}}}}");
        return list;
    }();

    return parsed;
}

double AiTools::barToPpq (double bar) const
{
    const auto& map = engine.getTempoMap();
    const auto whole = std::floor (juce::jmax (1.0, bar) - 1.0);
    const auto start = map.ppqOfBar (static_cast<int> (juce::jmin (whole, static_cast<double> (TempoMap::maxBar))));
    return start + (juce::jmax (1.0, bar) - 1.0 - whole) * map.barAt (start).length();
}

double AiTools::ppqToBar (double ppq) const
{
    const auto at = engine.getTempoMap().barAt (ppq);
    return at.index + 1.0 + (ppq - at.start) / at.length();
}

int AiTools::channelOf (const juce::var& value, juce::String& error) const
{
    if (value.toString().trim().equalsIgnoreCase ("main"))
        return AudioEngine::mainChannel;

    const auto id = static_cast<int> (number (value, -1.0));

    if (id == AudioEngine::mainChannel)
        return AudioEngine::mainChannel;

    if (id < 0 || engine.findTrack (id) == nullptr)
    {
        error = "There's no track or FX channel " + value.toString() + ". Call get_song for the ids.";
        return -1;
    }

    return id;
}

Plugin* AiTools::pluginAt (int channel, const juce::var& slot, int& slotId, juce::String& error)
{
    slotId = slot.toString().trim().equalsIgnoreCase ("instrument") ? AudioEngine::instrumentSlot
                                                                    : static_cast<int> (number (slot, -1.0));

    if (slotId < 0)
    {
        error = "slot is \"instrument\" or an insert id.";
        return nullptr;
    }

    if (auto* plugin = engine.getPlugin (channel, slotId))
        return plugin;

    error = slotId == AudioEngine::instrumentSlot ? "That track has no instrument plugin (it plays the Test Synth)."
                                                  : "There's no insert " + juce::String (slotId) + " on that channel.";
    return nullptr;
}

const Library::Entry* AiTools::findEntry (const juce::var& input, const juce::String& wantKind, juce::String& error) const
{
    const auto file = input.getProperty ("file", {}).toString();
    const auto id = input.getProperty ("plugin_id", {}).toString();
    const auto name = input.getProperty ("plugin", {}).toString().trim();

    const auto kindFits = [&wantKind] (const Library::Entry& entry)
    {
        return wantKind == "instrument" ? entry.kind == "instrument" : entry.kind != "instrument";
    };

    const Library::Entry* found = nullptr;

    for (const auto& entry : library.getEntries())
    {
        if (entry.missing || ! library.isOffered (entry.file) || ! kindFits (entry))
            continue;

        if (file.isNotEmpty() && entry.file == file && (id.isEmpty() || entry.id == id))
            return &entry;

        if (name.isNotEmpty() && entry.name.equalsIgnoreCase (name))
            return &entry;

        if (found == nullptr && name.isNotEmpty() && entry.name.containsIgnoreCase (name))
            found = &entry;
    }

    if (found == nullptr)
    {
        error = "No " + (wantKind == "instrument" ? juce::String ("instrument") : juce::String ("effect"))
              + " called \"" + (name.isNotEmpty() ? name : file) + "\" is installed. Use find_plugins to see what is.";

        if (! library.offersThirdParty())
            error << " The user chose to use only Sonify's own plugins this time, so their third-party plugins aren't offered until Sonify restarts with them.";
    }

    return found;
}

//==============================================================================
juce::var AiTools::run (const juce::String& name, const juce::var& input)
{
    if (name == "get_song")            return getSong();
    if (name == "get_clip")            return getClip (input);
    if (name == "find_plugins")        return findPlugins (input);
    if (name == "add_track")           return addTrack (input);
    if (name == "remove_track")        return removeTrack (input);
    if (name == "set_track")           return setTrack (input);
    if (name == "set_main")            return setMain (input);
    if (name == "load_instrument")     return loadPlugin (input, true);
    if (name == "add_effect")          return loadPlugin (input, false);
    if (name == "remove_effect")       return removeEffect (input);
    if (name == "move_effect")         return moveEffect (input);
    if (name == "bypass_effect")       return bypassEffect (input);
    if (name == "get_plugin_params")   return getPluginParams (input);
    if (name == "set_plugin_params")   return setPluginParams (input);
    if (name == "add_fx_channel")      return addFxChannel (input);
    if (name == "add_send")            return addSend (input, false);
    if (name == "add_sidechain")       return addSend (input, true);
    if (name == "set_send")            return setSend (input);
    if (name == "remove_send")         return removeSend (input);
    if (name == "write_midi_clip")     return writeMidiClip (input);
    if (name == "delete_clip")         return deleteClip (input);
    if (name == "set_tempo")           return setTempo (input);
    if (name == "set_time_signature")  return setTimeSignature (input);
    if (name == "set_key")             return setKey (input);
    if (name == "transport")           return transport (input);
    if (name == "add_session_players") return addSessionPlayers (input);
    if (name == "split_stems")         return splitStems (input);
    if (name == "add_midi_fx")         return addMidiFx (input);
    if (name == "set_midi_fx")         return setMidiFx (input);
    if (name == "remove_midi_fx")      return removeMidiFx (input);
    if (name == "apply_midi_tool")     return applyMidiTool (input);
    if (name == "get_board")           return getBoard();
    if (name == "set_board")           return setBoard (input);

    if (name == "open_plugin_window")
    {
        juce::String error;
        const auto channel = channelOf (input.getProperty ("channel", {}), error);
        int slot = 0;

        if (channel < 0 || pluginAt (channel, input.getProperty ("slot", {}), slot, error) == nullptr)
            return failure (error);

        return hooks.openWindow != nullptr && hooks.openWindow (channel, slot) ? success() : failure ("Its window couldn't open.");
    }

    if (name == "save_song")
    {
        const auto result = hooks.saveSong != nullptr ? hooks.saveSong() : juce::Result::fail ("Saving isn't available.");
        return result.wasOk() ? success() : failure (result.getErrorMessage());
    }

    return failure ("There's no tool called " + name + ".");
}

//==============================================================================
juce::var AiTools::getSong()
{
    const auto state = engine.getTransport().getState();
    const auto bar = [this] (double ppq) { return std::round (ppqToBar (ppq) * 1000.0) / 1000.0; };

    const auto insertsOf = [] (const EffectChain& chain)
    {
        juce::Array<juce::var> list;

        for (int i = 0; i < chain.size(); ++i)
        {
            const auto* insert = chain.at (i);
            auto item = object();
            auto* fields = item.getDynamicObject();
            fields->setProperty ("id", insert->id);
            fields->setProperty ("name", insert->plugin != nullptr ? insert->plugin->getDescriptor().name : juce::String ("?"));
            fields->setProperty ("bypassed", insert->bypassed.load());

            if (insert->plugin != nullptr && insert->plugin->hasSidechain())
                fields->setProperty ("has_sidechain", true);

            list.add (item);
        }

        return list;
    };

    auto song = object();
    auto* fields = song.getDynamicObject();
    fields->setProperty ("tempo", state.bpm);
    fields->setProperty ("time_signature", juce::String (state.numerator) + "/" + juce::String (state.denominator));

    // Mixed on the Analog Board: the tracks' inserts, faders, sends and Main aren't heard (get_board).
    if (engine.isBoardMixing())
        fields->setProperty ("mixed_on_analog_board", true);

    // The song channel: tempo changes and ramps, time signatures and keys through the song.
    const auto& map = engine.getTempoMap();
    fields->setProperty ("key", TempoMap::keyName (map.keyAt (state.ppq)));

    if (! map.hasOneTempo())
    {
        juce::Array<juce::var> changes;

        for (const auto& tempo : map.getTempos())
        {
            auto change = object();
            change.getDynamicObject()->setProperty ("bar", bar (tempo.ppq));
            change.getDynamicObject()->setProperty ("bpm", tempo.bpm);
            if (tempo.ramp) change.getDynamicObject()->setProperty ("ramps_to_next", true);
            changes.add (change);
        }

        fields->setProperty ("tempo_changes", changes);
    }

    if (map.getSignatures().size() > 1)
    {
        juce::Array<juce::var> changes;

        for (const auto& signature : map.getSignatures())
        {
            auto change = object();
            change.getDynamicObject()->setProperty ("bar", signature.bar + 1);
            change.getDynamicObject()->setProperty ("time_signature", juce::String (signature.numerator) + "/" + juce::String (signature.denominator));
            changes.add (change);
        }

        fields->setProperty ("time_signature_changes", changes);
    }

    if (map.getKeys().size() > 1)
    {
        juce::Array<juce::var> changes;

        for (const auto& key : map.getKeys())
        {
            auto change = object();
            change.getDynamicObject()->setProperty ("bar", key.bar + 1);
            change.getDynamicObject()->setProperty ("key", TempoMap::keyName (key));
            changes.add (change);
        }

        fields->setProperty ("key_changes", changes);
    }
    fields->setProperty ("playing", state.playing);
    fields->setProperty ("position_bar", bar (state.ppq));

    auto loop = object();
    loop.getDynamicObject()->setProperty ("on", state.loopEnabled);
    loop.getDynamicObject()->setProperty ("start_bar", bar (state.loopStart));
    loop.getDynamicObject()->setProperty ("end_bar", bar (state.loopEnd));
    fields->setProperty ("loop", loop);

    auto main = object();
    main.getDynamicObject()->setProperty ("volume_db", engine.getMasterVolume());
    main.getDynamicObject()->setProperty ("inserts", insertsOf (engine.getMainInserts()));
    fields->setProperty ("main", main);

    juce::Array<juce::var> tracks, fxChannels;

    for (const auto& track : engine.getTracks())
    {
        auto item = object();
        auto* t = item.getDynamicObject();
        const auto isFx = track->isMixChannel();
        t->setProperty ("id", track->id);
        t->setProperty ("name", track->name);
        t->setProperty ("kind", track->kind == Track::Kind::audio ? "audio" : track->kind == Track::Kind::bus ? "bus" : isFx ? "fx" : "instrument");

        if (const auto bus = engine.getTrackBus (track->id); bus != 0)
            t->setProperty ("plays_into_bus", bus);
        t->setProperty ("volume_db", track->volumeDb.load());
        t->setProperty ("pan", track->pan.load());
        t->setProperty ("mute", track->mute.load());
        t->setProperty ("solo", track->solo.load());

        if (! isFx)
        {
            t->setProperty ("monitor", track->monitor.load());
            t->setProperty ("record_arm", track->recordArm.load());
        }

        if (track->kind == Track::Kind::instrument)
        {
            if (track->instrument != nullptr)
            {
                auto instrument = object();
                instrument.getDynamicObject()->setProperty ("name", track->instrument->getDescriptor().name);
                if (track->instrument->producesMidi()) instrument.getDynamicObject()->setProperty ("plays_notes_out", true);
                if (track->instrument->hasSidechain()) instrument.getDynamicObject()->setProperty ("has_sidechain", true);
                t->setProperty ("instrument", instrument);
            }
            else
            {
                t->setProperty ("instrument", "Test Synth (built in)");
            }

            juce::Array<juce::var> inputs;
            for (const auto& source : track->midiInputs) inputs.add (source);
            t->setProperty ("midi_inputs", inputs);

            // MIDI FX: { id, type, on, settings }, in the order the notes go through them.
            if (track->midiFx.size() > 0)
            {
                juce::Array<juce::var> devices;
                const auto midiFx = engine.getMidiFx (track->id);

                for (const auto& device : *midiFx.getArray())
                {
                    auto d = object();
                    d.getDynamicObject()->setProperty ("id", device.getProperty ("id", 0));
                    d.getDynamicObject()->setProperty ("type", device.getProperty ("type", {}));
                    d.getDynamicObject()->setProperty ("on", device.getProperty ("on", true));
                    d.getDynamicObject()->setProperty ("settings", device.getProperty ("params", {}));

                    if (device.hasProperty ("tool"))
                        d.getDynamicObject()->setProperty ("magenta_tool", device.getProperty ("toolName", {}));

                    // A Sonify device (made in the Device Studio): its name and what its settings are.
                    if (device.getProperty ("type", {}).toString() == "device")
                    {
                        d.getDynamicObject()->setProperty ("name", device.getProperty ("name", {}));
                        juce::Array<juce::var> about;

                        if (const auto* params = device.getProperty ("device", {}).getProperty ("params", {}).getArray())
                        {
                            for (const auto& param : *params)
                            {
                                auto p = object();
                                p.getDynamicObject()->setProperty ("id", param.getProperty ("id", {}));
                                p.getDynamicObject()->setProperty ("name", param.getProperty ("name", {}));
                                p.getDynamicObject()->setProperty ("min", param.getProperty ("min", 0));
                                p.getDynamicObject()->setProperty ("max", param.getProperty ("max", 1));

                                if (param.getProperty ("choices", {}).isArray())
                                    p.getDynamicObject()->setProperty ("choices", param.getProperty ("choices", {}));

                                about.add (p);
                            }
                        }

                        d.getDynamicObject()->setProperty ("device_settings", about);
                    }

                    devices.add (d);
                }

                t->setProperty ("midi_fx", devices);
            }
        }

        t->setProperty ("inserts", insertsOf (track->inserts));

        juce::Array<juce::var> sends;

        for (const auto& send : track->sends)
        {
            auto s = object();
            s.getDynamicObject()->setProperty ("id", send.id);
            s.getDynamicObject()->setProperty ("sidechain", send.sidechain);
            s.getDynamicObject()->setProperty ("to", send.target);
            if (send.sidechain) s.getDynamicObject()->setProperty ("slot", send.slot);
            s.getDynamicObject()->setProperty ("level_db", send.levelDb);
            s.getDynamicObject()->setProperty ("on", send.on);
            sends.add (s);
        }

        if (! isFx)
        {
            t->setProperty ("sends", sends);

            juce::Array<juce::var> clips;

            for (const auto& clip : engine.getClips (track->id))
            {
                auto c = object();
                c.getDynamicObject()->setProperty ("id", clip.id);
                c.getDynamicObject()->setProperty ("start_bar", bar (clip.start));
                c.getDynamicObject()->setProperty ("length_bars", std::round ((ppqToBar (clip.start + clip.length) - ppqToBar (clip.start)) * 1000.0) / 1000.0);

                if (clip.audioFile.isNotEmpty())
                    c.getDynamicObject()->setProperty ("audio", juce::File (clip.audioFile).getFileName());
                else
                    c.getDynamicObject()->setProperty ("notes", static_cast<int> (clip.notes.size()));

                clips.add (c);
            }

            t->setProperty ("clips", clips);
        }

        (isFx ? fxChannels : tracks).add (item);
    }

    fields->setProperty ("tracks", tracks);
    fields->setProperty ("fx_channels", fxChannels);
    fields->setProperty ("selected_track", engine.getSelectedTrackId());
    return song;
}

juce::var AiTools::getClip (const juce::var& input)
{
    const auto trackId = static_cast<int> (number (input.getProperty ("track_id", {}), -1.0));
    const auto clipId = static_cast<int> (number (input.getProperty ("clip_id", {}), -1.0));

    for (const auto& clip : engine.getClips (trackId))
    {
        if (clip.id != clipId)
            continue;

        juce::Array<juce::var> notes;

        for (const auto& note : clip.notes)
        {
            if (note.start < clip.offset || note.start >= clip.offset + clip.length)
                continue;   // hidden by the clip's edges

            auto n = object();
            n.getDynamicObject()->setProperty ("pitch", note.pitch);
            n.getDynamicObject()->setProperty ("start_beat", note.start - clip.offset);
            n.getDynamicObject()->setProperty ("length_beats", note.length);
            n.getDynamicObject()->setProperty ("velocity", juce::roundToInt (note.velocity * 127.0f));
            notes.add (n);
        }

        auto result = object();
        result.getDynamicObject()->setProperty ("notes", notes);
        return result;
    }

    return failure ("There's no clip " + juce::String (clipId) + " on track " + juce::String (trackId) + ".");
}

juce::var AiTools::findPlugins (const juce::var& input)
{
    const auto words = juce::StringArray::fromTokens (input.getProperty ("query", {}).toString().toLowerCase(), true);
    const auto kind = input.getProperty ("kind", "any").toString();
    std::vector<std::pair<int, const Library::Entry*>> scored;

    for (const auto& entry : library.getEntries())
    {
        if (entry.missing || ! library.isOffered (entry.file))
            continue;

        if (kind == "instrument" && entry.kind != "instrument")
            continue;

        if (kind == "effect" && entry.kind == "instrument")
            continue;

        const auto haystack = (entry.name + " " + entry.vendor + " " + entry.category + " " + entry.features.joinIntoString (" ")).toLowerCase();
        int score = 0;
        bool all = true;

        for (const auto& word : words)
        {
            if (! haystack.contains (word)) { all = false; break; }
            score += entry.name.toLowerCase().contains (word) ? 3 : 1;
        }

        if (all)
            scored.push_back ({ score + (entry.name.equalsIgnoreCase (words.joinIntoString (" ")) ? 10 : 0), &entry });
    }

    std::stable_sort (scored.begin(), scored.end(), [] (const auto& a, const auto& b) { return a.first > b.first; });

    juce::Array<juce::var> list;

    for (size_t i = 0; i < scored.size() && i < 15; ++i)
    {
        const auto& entry = *scored[i].second;
        auto item = object();
        auto* f = item.getDynamicObject();
        f->setProperty ("name", entry.name);
        f->setProperty ("vendor", entry.vendor);
        f->setProperty ("kind", entry.kind);
        f->setProperty ("type", entry.category);
        f->setProperty ("format", entry.format());
        f->setProperty ("file", entry.file);
        f->setProperty ("plugin_id", entry.id);
        list.add (item);
    }

    auto result = object();
    result.getDynamicObject()->setProperty ("plugins", list);

    if (list.isEmpty())
        result.getDynamicObject()->setProperty ("note", "Nothing installed matches. Try a shorter word or a type (synth, reverb, compressor).");

    // Third-party plugins off for this run (the question at startup): said, so the user hears why.
    if (! library.offersThirdParty())
        result.getDynamicObject()->setProperty ("only_sonify_plugins", "The user chose to use only Sonify's own plugins this time, so their third-party plugins aren't offered until Sonify restarts with them.");

    return result;
}

juce::var AiTools::addTrack (const juce::var& input)
{
    const auto kind = input.getProperty ("kind", "instrument").toString() == "audio" ? Track::Kind::audio : Track::Kind::instrument;
    const auto id = engine.addTrack (kind);

    if (const auto name = input.getProperty ("name", {}).toString().trim(); name.isNotEmpty())
        engine.setTrackName (id, name);

    auto result = object();
    result.getDynamicObject()->setProperty ("track_id", id);
    return result;
}

juce::var AiTools::removeTrack (const juce::var& input)
{
    const auto id = static_cast<int> (number (input.getProperty ("track_id", {}), -1.0));

    if (! static_cast<bool> (input.getProperty ("confirmed", false)))
        return failure ("Removing a track needs the user's yes: ask them, then call again with confirmed: true.");

    if (engine.findTrack (id) == nullptr)
        return failure ("There's no track " + juce::String (id) + ".");

    engine.removeTrack (id);
    return success();
}

juce::var AiTools::setTrack (const juce::var& input)
{
    const auto id = static_cast<int> (number (input.getProperty ("track_id", {}), -1.0));
    auto* track = engine.findTrack (id);

    if (track == nullptr)
        return failure ("There's no track " + juce::String (id) + ".");

    if (input.hasProperty ("name"))       engine.setTrackName (id, input.getProperty ("name", {}).toString());
    if (input.hasProperty ("volume_db"))  track->volumeDb = static_cast<float> (juce::jlimit (-144.0, 10.0, number (input.getProperty ("volume_db", {}), 0.0)));
    if (input.hasProperty ("pan"))        track->pan = static_cast<float> (juce::jlimit (-1.0, 1.0, number (input.getProperty ("pan", {}), 0.0)));
    if (input.hasProperty ("mute"))       track->mute = static_cast<bool> (input.getProperty ("mute", false));
    if (input.hasProperty ("solo"))       track->solo = static_cast<bool> (input.getProperty ("solo", false));
    if (input.hasProperty ("monitor"))    engine.setTrackMonitor (id, static_cast<bool> (input.getProperty ("monitor", false)), true);
    if (input.hasProperty ("record_arm")) engine.setTrackRecordArm (id, static_cast<bool> (input.getProperty ("record_arm", false)));
    return success();
}

juce::var AiTools::setMain (const juce::var& input)
{
    engine.setMasterVolume (static_cast<float> (juce::jlimit (-144.0, 10.0, number (input.getProperty ("volume_db", {}), 0.0))));
    return success();
}

juce::var AiTools::loadPlugin (const juce::var& input, bool instrument)
{
    juce::String error;
    const auto channel = instrument ? static_cast<int> (number (input.getProperty ("track_id", {}), -1.0))
                                    : channelOf (input.getProperty ("channel", {}), error);

    if (instrument)
    {
        auto* track = engine.findTrack (channel);

        if (track == nullptr || track->kind != Track::Kind::instrument)
            return failure ("Instruments load onto instrument tracks: there's no instrument track " + input.getProperty ("track_id", {}).toString() + ".");
    }
    else if (channel < 0)
    {
        return failure (error);
    }

    const auto* entry = findEntry (input, instrument ? "instrument" : "effect", error);

    if (entry == nullptr)
        return failure (error);

    const auto position = input.hasProperty ("position") ? static_cast<int> (number (input.getProperty ("position", {}), -1.0)) : -1;
    int used = 0;
    const auto loaded = engine.loadPlugin (channel, juce::File (entry->file), entry->id, position, 0, used);

    if (loaded.failed())
        return failure (entry->name + " couldn't load: " + loaded.getErrorMessage());

    // An instrument on its track: Monitor on, the first time only (as from the Browser).
    if (instrument)
        engine.monitorOnce (channel);

    auto result = object();
    result.getDynamicObject()->setProperty ("name", entry->name);

    if (! instrument)
        result.getDynamicObject()->setProperty ("insert_id", used);

    return result;
}

juce::var AiTools::removeEffect (const juce::var& input)
{
    juce::String error;
    const auto channel = channelOf (input.getProperty ("channel", {}), error);

    if (channel < 0)
        return failure (error);

    int slot = 0;

    if (pluginAt (channel, input.getProperty ("insert_id", {}), slot, error) == nullptr)
        return failure (error);

    if (slot == AudioEngine::instrumentSlot)
        engine.clearInstrument (channel);
    else
        engine.removeInsert (channel, slot);

    return success();
}

juce::var AiTools::moveEffect (const juce::var& input)
{
    juce::String error;
    const auto channel = channelOf (input.getProperty ("channel", {}), error);
    int slot = 0;

    if (channel < 0 || pluginAt (channel, input.getProperty ("insert_id", {}), slot, error) == nullptr || slot == AudioEngine::instrumentSlot)
        return failure (error.isNotEmpty() ? error : juce::String ("Only inserts move."));

    engine.moveInsert (channel, slot, juce::jmax (0, static_cast<int> (number (input.getProperty ("position", {}), 0.0))));
    return success();
}

juce::var AiTools::bypassEffect (const juce::var& input)
{
    juce::String error;
    const auto channel = channelOf (input.getProperty ("channel", {}), error);
    int slot = 0;

    if (channel < 0 || pluginAt (channel, input.getProperty ("insert_id", {}), slot, error) == nullptr || slot == AudioEngine::instrumentSlot)
        return failure (error.isNotEmpty() ? error : juce::String ("Only inserts are switched off."));

    engine.setInsertBypass (channel, slot, static_cast<bool> (input.getProperty ("bypassed", true)));
    return success();
}

juce::var AiTools::getPluginParams (const juce::var& input)
{
    juce::String error;
    const auto channel = channelOf (input.getProperty ("channel", {}), error);
    int slot = 0;
    auto* plugin = channel < 0 ? nullptr : pluginAt (channel, input.getProperty ("slot", {}), slot, error);

    if (plugin == nullptr)
        return failure (error);

    const auto filter = input.getProperty ("filter", {}).toString().trim();
    const auto offset = juce::jmax (0, static_cast<int> (number (input.getProperty ("offset", {}), 0.0)));
    const auto& parameters = plugin->getParameters();
    juce::Array<juce::var> list;
    int matching = 0;

    for (int i = 0; i < static_cast<int> (parameters.size()); ++i)
    {
        const auto& parameter = parameters[static_cast<size_t> (i)];

        if (parameter.hidden || (filter.isNotEmpty() && ! parameter.name.containsIgnoreCase (filter)))
            continue;

        if (matching++ < offset || list.size() >= 60)
            continue;

        const auto value = plugin->getParameterValue (i);
        auto item = object();
        auto* f = item.getDynamicObject();
        f->setProperty ("index", i);
        f->setProperty ("name", parameter.name);
        f->setProperty ("value", value);
        f->setProperty ("min", parameter.minValue);
        f->setProperty ("max", parameter.maxValue);
        f->setProperty ("text", plugin->getParameterText (i, value));
        if (parameter.stepped) f->setProperty ("stepped", true);
        list.add (item);
    }

    auto result = object();
    result.getDynamicObject()->setProperty ("plugin", plugin->getDescriptor().name);
    result.getDynamicObject()->setProperty ("matching", matching);
    result.getDynamicObject()->setProperty ("params", list);

    if (matching > offset + list.size())
        result.getDynamicObject()->setProperty ("more", "Call again with offset " + juce::String (offset + list.size()) + " for the rest.");

    return result;
}

juce::var AiTools::setPluginParams (const juce::var& input)
{
    juce::String error;
    const auto channel = channelOf (input.getProperty ("channel", {}), error);
    int slot = 0;
    auto* plugin = channel < 0 ? nullptr : pluginAt (channel, input.getProperty ("slot", {}), slot, error);

    if (plugin == nullptr)
        return failure (error);

    const auto& parameters = plugin->getParameters();
    juce::Array<juce::var> results;

    if (const auto* changes = input.getProperty ("changes", {}).getArray())
    {
        for (const auto& change : *changes)
        {
            int index = change.hasProperty ("index") ? static_cast<int> (number (change.getProperty ("index", {}), -1.0)) : -1;

            if (index < 0)
            {
                const auto name = change.getProperty ("name", {}).toString().trim();

                // Hidden parameters are the plugin's own business (a mastering comparison, say).
                for (int i = 0; i < static_cast<int> (parameters.size()) && index < 0; ++i)
                    if (! parameters[static_cast<size_t> (i)].hidden && parameters[static_cast<size_t> (i)].name.equalsIgnoreCase (name))
                        index = i;

                for (int i = 0; i < static_cast<int> (parameters.size()) && index < 0 && name.isNotEmpty(); ++i)
                    if (! parameters[static_cast<size_t> (i)].hidden && parameters[static_cast<size_t> (i)].name.containsIgnoreCase (name))
                        index = i;
            }

            auto item = object();

            if (index < 0 || index >= static_cast<int> (parameters.size()) || parameters[static_cast<size_t> (index)].readOnly
                || parameters[static_cast<size_t> (index)].hidden)
            {
                item.getDynamicObject()->setProperty ("error", "No settable parameter " + (change.hasProperty ("name") ? change.getProperty ("name", {}).toString()
                                                                                                                       : change.getProperty ("index", {}).toString()));
                results.add (item);
                continue;
            }

            const auto& parameter = parameters[static_cast<size_t> (index)];
            const auto wanted = juce::jlimit (parameter.minValue, parameter.maxValue, number (change.getProperty ("value", {}), parameter.defaultValue));
            const auto applied = engine.setPluginParameter (channel, slot, index, wanted);
            item.getDynamicObject()->setProperty ("name", parameter.name);
            item.getDynamicObject()->setProperty ("value", applied);
            item.getDynamicObject()->setProperty ("text", plugin->getParameterText (index, applied));
            results.add (item);
        }
    }

    auto result = object();
    result.getDynamicObject()->setProperty ("plugin", plugin->getDescriptor().name);
    result.getDynamicObject()->setProperty ("changed", results);
    return result;
}

juce::var AiTools::addFxChannel (const juce::var& input)
{
    const auto id = engine.addTrack (Track::Kind::fx);

    if (const auto name = input.getProperty ("name", {}).toString().trim(); name.isNotEmpty())
        engine.setTrackName (id, name);

    auto result = object();
    result.getDynamicObject()->setProperty ("fx_channel_id", id);
    return result;
}

juce::var AiTools::addSend (const juce::var& input, bool sidechain)
{
    Track::Send send;
    send.sidechain = sidechain;
    send.levelDb = static_cast<float> (number (input.getProperty ("level_db", {}), 0.0));
    int source = 0;

    if (sidechain)
    {
        juce::String error;
        source = static_cast<int> (number (input.getProperty ("source_track_id", {}), -1.0));
        const auto channel = channelOf (input.getProperty ("channel", {}), error);
        int slot = 0;
        auto* plugin = channel < 0 ? nullptr : pluginAt (channel, input.getProperty ("slot", {}), slot, error);

        if (plugin == nullptr)
            return failure (error);

        if (! plugin->hasSidechain())
            return failure (plugin->getDescriptor().name + " has no sidechain input.");

        send.target = channel;
        send.slot = slot;
    }
    else
    {
        source = static_cast<int> (number (input.getProperty ("track_id", {}), -1.0));
        send.target = static_cast<int> (number (input.getProperty ("fx_channel_id", {}), -1.0));
    }

    const auto id = engine.addSend (source, send);

    if (id == 0)
        return failure (sidechain ? "That sidechain send isn't possible (a track can't key a plugin on itself, and FX channels don't send)."
                                  : "That send isn't possible: check the track and the FX channel ids (FX channels don't send).");

    auto result = object();
    result.getDynamicObject()->setProperty ("send_id", id);
    return result;
}

juce::var AiTools::setSend (const juce::var& input)
{
    const auto track = static_cast<int> (number (input.getProperty ("track_id", {}), -1.0));
    const auto send = static_cast<int> (number (input.getProperty ("send_id", {}), -1.0));
    const auto sends = engine.getSends (track);

    if (std::none_of (sends.begin(), sends.end(), [send] (const Track::Send& s) { return s.id == send; }))
        return failure ("There's no send " + juce::String (send) + " on track " + juce::String (track) + ". get_song lists them.");

    if (input.hasProperty ("level_db")) engine.setSendLevel (track, send, static_cast<float> (number (input.getProperty ("level_db", {}), 0.0)));
    if (input.hasProperty ("on"))       engine.setSendOn (track, send, static_cast<bool> (input.getProperty ("on", true)));
    return success();
}

juce::var AiTools::removeSend (const juce::var& input)
{
    const auto track = static_cast<int> (number (input.getProperty ("track_id", {}), -1.0));
    const auto send = static_cast<int> (number (input.getProperty ("send_id", {}), -1.0));
    const auto sends = engine.getSends (track);

    if (std::none_of (sends.begin(), sends.end(), [send] (const Track::Send& s) { return s.id == send; }))
        return failure ("There's no send " + juce::String (send) + " on track " + juce::String (track) + ". get_song lists them.");

    engine.removeSend (track, send);
    return success();
}

juce::var AiTools::writeMidiClip (const juce::var& input)
{
    const auto trackId = static_cast<int> (number (input.getProperty ("track_id", {}), -1.0));
    auto* track = engine.findTrack (trackId);

    if (track == nullptr || track->kind != Track::Kind::instrument)
        return failure ("MIDI clips go on instrument tracks: there's no instrument track " + juce::String (trackId) + ".");

    MidiClip clip;
    const auto startBar = juce::jmax (1.0, number (input.getProperty ("start_bar", {}), 1.0));
    clip.start = barToPpq (startBar);
    clip.length = juce::jlimit (0.25, 4096.0, barToPpq (startBar + juce::jmax (0.0, number (input.getProperty ("length_bars", {}), 1.0))) - clip.start);

    if (const auto* notes = input.getProperty ("notes", {}).getArray())
    {
        for (const auto& item : *notes)
        {
            MidiNote note;
            note.pitch = juce::jlimit (0, 127, static_cast<int> (number (item.getProperty ("pitch", {}), 60.0)));
            note.start = juce::jmax (0.0, number (item.getProperty ("start_beat", {}), 0.0));
            note.length = juce::jlimit (1.0 / 64.0, 4096.0, number (item.getProperty ("length_beats", {}), 0.25));
            note.velocity = static_cast<float> (juce::jlimit (1.0, 127.0, number (item.getProperty ("velocity", {}), 100.0)) / 127.0);
            clip.notes.push_back (note);

            if (clip.notes.size() >= 5000)
                break;
        }
    }

    std::sort (clip.notes.begin(), clip.notes.end(), [] (const MidiNote& a, const MidiNote& b) { return a.start < b.start; });

    // replace: the track's clips under it go first
    if (static_cast<bool> (input.getProperty ("replace", false)))
    {
        auto clips = engine.getClips (trackId);
        clips.erase (std::remove_if (clips.begin(), clips.end(), [&clip] (const MidiClip& other)
        {
            return other.start < clip.start + clip.length && clip.start < other.start + other.length;
        }), clips.end());
        engine.setClips (trackId, std::move (clips));
    }

    const auto id = engine.addClip (trackId, std::move (clip));
    auto result = object();
    result.getDynamicObject()->setProperty ("clip_id", id);
    return result;
}

juce::var AiTools::deleteClip (const juce::var& input)
{
    const auto trackId = static_cast<int> (number (input.getProperty ("track_id", {}), -1.0));
    const auto clipId = static_cast<int> (number (input.getProperty ("clip_id", {}), -1.0));
    auto clips = engine.getClips (trackId);
    const auto before = clips.size();
    clips.erase (std::remove_if (clips.begin(), clips.end(), [clipId] (const MidiClip& c) { return c.id == clipId; }), clips.end());

    if (clips.size() == before)
        return failure ("There's no clip " + juce::String (clipId) + " on track " + juce::String (trackId) + ".");

    engine.setClips (trackId, std::move (clips));
    return success();
}

juce::var AiTools::setTempo (const juce::var& input)
{
    const auto& map = engine.getTempoMap();

    if (input.hasProperty ("remove_at_bar"))
    {
        const auto at = barToPpq (number (input.getProperty ("remove_at_bar", {}), 1.0));
        auto list = map.getTempos();
        const auto before = list.size();
        list.erase (std::remove_if (list.begin() + 1, list.end(), [at] (const TempoMap::Tempo& t) { return std::abs (t.ppq - at) < 1.0e-6; }), list.end());

        if (list.size() == before)
            return failure ("There's no tempo change at that bar (the song's first tempo stays).");

        auto edited = map;
        edited.setTempos (std::move (list));
        engine.setTempoMap (edited);
        return success();
    }

    if (! input.hasProperty ("bpm"))
        return failure ("Give bpm.");

    const auto bpm = juce::jlimit (TempoMap::minTempo, TempoMap::maxTempo, number (input.getProperty ("bpm", {}), 120.0));

    // The tempo that holds where the song is (with one tempo, the whole song's).
    if (! input.hasProperty ("at_bar"))
    {
        engine.setTempoMap (map.withTempoAt (engine.getTransport().getState().ppq, bpm));
        return success();
    }

    // A change from a bar on; a ramp glides into it from the change before.
    const auto at = barToPpq (number (input.getProperty ("at_bar", {}), 1.0));
    auto list = map.getTempos();
    list.erase (std::remove_if (list.begin(), list.end(), [at] (const TempoMap::Tempo& t) { return std::abs (t.ppq - at) < 1.0e-6; }), list.end());

    const auto previous = std::find_if (list.rbegin(), list.rend(), [at] (const TempoMap::Tempo& t) { return t.ppq < at; });

    if (previous != list.rend())
        previous->ramp = static_cast<bool> (input.getProperty ("ramp", false));

    list.push_back ({ at, bpm, false });
    auto edited = map;
    edited.setTempos (std::move (list));
    engine.setTempoMap (edited);
    return success();
}

juce::var AiTools::setTimeSignature (const juce::var& input)
{
    const auto numerator = juce::jlimit (1, 32, static_cast<int> (number (input.getProperty ("numerator", {}), 4.0)));
    const auto denominator = static_cast<int> (number (input.getProperty ("denominator", {}), 4.0));

    if (denominator != 1 && denominator != 2 && denominator != 4 && denominator != 8 && denominator != 16 && denominator != 32)
        return failure ("The denominator is 1, 2, 4, 8, 16 or 32.");

    const auto& map = engine.getTempoMap();

    if (! input.hasProperty ("at_bar"))
    {
        engine.setTempoMap (map.withSignatureAt (engine.getTransport().getState().ppq, numerator, denominator));
        return success();
    }

    const auto bar = juce::jlimit (0, TempoMap::maxBar, static_cast<int> (number (input.getProperty ("at_bar", {}), 1.0)) - 1);
    auto list = map.getSignatures();
    list.erase (std::remove_if (list.begin(), list.end(), [bar] (const TempoMap::Signature& s) { return s.bar == bar; }), list.end());

    if (static_cast<bool> (input.getProperty ("remove", false)))
    {
        if (bar == 0 || list.size() == map.getSignatures().size())
            return failure ("There's no time signature change at that bar (the song's first stays).");
    }
    else
    {
        list.push_back ({ bar, numerator, denominator });
    }

    auto edited = map;
    edited.setSignatures (std::move (list));
    engine.setTempoMap (edited);
    return success();
}

juce::var AiTools::setKey (const juce::var& input)
{
    static const juce::StringArray sharps { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const juce::StringArray flats { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    const auto name = input.getProperty ("root", {}).toString().trim();
    auto root = sharps.indexOf (name, true);

    if (root < 0)
        root = flats.indexOf (name, true);

    const auto scale = input.getProperty ("scale", {}).toString().trim().toLowerCase();

    if (root < 0)
        return failure ("root is C, C#, D ... B.");

    if (! TempoMap::getScales().contains (scale))
        return failure ("scale is one of: " + TempoMap::getScales().joinIntoString (", ") + ".");

    const auto& map = engine.getTempoMap();
    const auto bar = juce::jlimit (0, TempoMap::maxBar, static_cast<int> (number (input.getProperty ("at_bar", {}), 1.0)) - 1);
    auto list = map.getKeys();
    list.erase (std::remove_if (list.begin(), list.end(), [bar] (const TempoMap::Key& k) { return k.bar == bar; }), list.end());

    if (static_cast<bool> (input.getProperty ("remove", false)))
    {
        if (bar == 0 || list.size() == map.getKeys().size())
            return failure ("There's no key change at that bar (the song's first key stays).");
    }
    else
    {
        list.push_back ({ bar, root, scale });
    }

    auto edited = map;
    edited.setKeys (std::move (list));
    engine.setTempoMap (edited);
    return success();
}

juce::var AiTools::transport (const juce::var& input)
{
    auto& t = engine.getTransport();

    if (input.hasProperty ("loop_start_bar") && input.hasProperty ("loop_end_bar"))
    {
        const auto start = barToPpq (number (input.getProperty ("loop_start_bar", {}), 1.0));
        const auto end = juce::jmax (start + 0.25, barToPpq (number (input.getProperty ("loop_end_bar", {}), 2.0)));
        t.setLoop (true, start, end);
    }
    else if (static_cast<bool> (input.getProperty ("loop_off", false)))
    {
        const auto state = t.getState();
        t.setLoop (false, state.loopStart, state.loopEnd);
    }

    const auto action = input.getProperty ("action", "none").toString();

    if (action == "play")        t.setPlaying (true);
    else if (action == "stop")   t.setPlaying (false);
    else if (action == "rewind") t.locate (0.0);

    return success();
}

juce::var AiTools::splitStems (const juce::var& input)
{
    const auto trackId = static_cast<int> (number (input.getProperty ("track_id", {}), -1.0));
    const auto* track = engine.findTrack (trackId);

    if (track == nullptr || track->kind != Track::Kind::audio)
        return failure ("Track " + juce::String (trackId) + " isn't an audio track.");

    auto clipId = static_cast<int> (number (input.getProperty ("clip_id", {}), 0.0));

    if (clipId <= 0)
    {
        juce::Array<int> clips;

        for (const auto& clip : track->clips)
            if (clip.isAudio())
                clips.add (clip.id);

        if (clips.isEmpty())
            return failure ("Track " + juce::String (trackId) + " has no recording.");

        if (clips.size() > 1)
            return failure ("Track " + juce::String (trackId) + " has " + juce::String (clips.size()) + " audio clips: give clip_id (get_song lists them).");

        clipId = clips.getFirst();
    }

    juce::StringArray stems;

    if (const auto* list = input.getProperty ("stems", {}).getArray())
        for (const auto& stem : *list)
            stems.addIfNotAlreadyThere (stem.toString().trim().toLowerCase());

    if (stems.isEmpty())
        stems = { "vocals", "drums", "bass", "other" };

    if (hooks.splitStems == nullptr)
        return failure ("Splitting stems isn't available.");

    if (const auto error = hooks.splitStems (trackId, clipId, stems, static_cast<bool> (input.getProperty ("mute_original", true))); error.isNotEmpty())
        return failure (error);

    auto result = object();
    result.getDynamicObject()->setProperty ("ok", true);
    result.getDynamicObject()->setProperty ("note", "Splitting started: " + stems.joinIntoString (", ") + ". Each arrives on a new track under "
                                                    + track->name + " when it's done (Sonify shows the progress), after this request.");
    return result;
}

juce::var AiTools::addSessionPlayers (const juce::var& input)
{
    SessionPlayers::Request request;
    request.style = SessionPlayers::styleFrom (input.getProperty ("style", "pop").toString());
    request.intensity = juce::jlimit (0.0, 1.0, number (input.getProperty ("intensity", {}), 0.5));
    request.startBar = juce::jlimit (1, 9999, static_cast<int> (number (input.getProperty ("start_bar", {}), 1.0)));
    request.times = juce::jlimit (1, 64, static_cast<int> (number (input.getProperty ("times", {}), 1.0)));
    request.chords = input.getProperty ("chords", {}).toString().trim();

    if (const auto* players = input.getProperty ("players", {}).getArray(); players != nullptr && ! players->isEmpty())
    {
        request.players.clear();

        for (const auto& name : *players)
        {
            SessionPlayers::Player player;

            if (! SessionPlayers::playerFrom (name.toString(), player))
                return failure ("The players are drums, bass and keys.");

            if (std::find (request.players.begin(), request.players.end(), player) == request.players.end())
                request.players.push_back (player);
        }
    }

    // An instrument named for a player: an installed one.
    const auto instruments = input.getProperty ("instruments", {});

    for (const auto player : request.players)
    {
        const auto wanted = instruments.getProperty (SessionPlayers::nameOf (player), {}).toString().trim();

        if (wanted.isEmpty())
            continue;

        juce::String error;
        auto query = object();
        query.getDynamicObject()->setProperty ("plugin", wanted);

        const auto* entry = findEntry (query, "instrument", error);

        if (entry == nullptr)
            return failure (error);

        request.instruments[player] = { entry->file, entry->id };
    }

    if (request.players.empty())
        return failure ("Choose at least one player: drums, bass or keys.");

    // The chords a track plays, from where it plays them. A recording is listened to for its
    // tempo and beat too; one with a tempo of its own gives the song its tempo, and the song's
    // audio moves onto the bars. That move waits until the players are made: if they can't be,
    // the song stays as it was.
    juce::String lined;
    bool aligning = false;
    RecordingAnalysis::Alignment alignment;

    if (request.chords.isEmpty() && input.hasProperty ("chords_from_track_id"))
    {
        const auto trackId = static_cast<int> (number (input.getProperty ("chords_from_track_id", {}), -1.0));
        const auto* track = engine.findTrack (trackId);

        if (track != nullptr && track->kind == Track::Kind::audio)
        {
            const auto heard = RecordingAnalysis::listen (engine, trackId, juce::jlimit (0.0, 300.0, number (input.getProperty ("recording_tempo", {}), 0.0)));

            if (! heard.ok)
                return failure (heard.error);

            request.chords = heard.chords;

            if (heard.followsSong)
            {
                if (! input.hasProperty ("start_bar"))
                    request.startBar = heard.firstBar;
            }
            else
            {
                alignment = RecordingAnalysis::alignmentFor (heard, engine);
                aligning = true;
                request.startBar = alignment.startBar;
                lined = "The song is now at " + juce::String (alignment.bpm, 2) + " BPM, the recording's tempo, its audio moved onto the bars.";

                if (heard.steadiness < 0.7)
                    lined << " Its tempo wanders, so the players may drift from it in places.";
            }
        }
        else if (track != nullptr && track->kind == Track::Kind::instrument)
        {
            int firstBar = 1, bars = 0;
            request.chords = SessionPlayers::detectChords (engine.getClips (trackId), engine.getTempoMap(), firstBar, bars);

            if (request.chords.isEmpty())
                return failure ("That track plays no notes to take the chords from.");

            if (! input.hasProperty ("start_bar"))
                request.startBar = firstBar;
        }
        else
        {
            return failure ("There's no track " + juce::String (trackId) + " to take the chords from.");
        }
    }

    std::vector<int> trackIds;
    juce::StringArray notes;
    const auto made = SessionPlayers::create (engine, library, request, trackIds, notes);

    if (made.failed())
        return failure (made.getErrorMessage());

    // The players' tracks stay on their bars (MIDI keeps its bars at a new tempo); the song's
    // audio and tempo change only now.
    if (aligning)
        RecordingAnalysis::align (engine, alignment);

    juce::Array<juce::var> tracks, told;

    for (size_t i = 0; i < trackIds.size() && i < request.players.size(); ++i)
    {
        const auto* plugin = engine.getPlugin (trackIds[i], AudioEngine::instrumentSlot);
        auto item = object();
        item.getDynamicObject()->setProperty ("track_id", trackIds[i]);
        item.getDynamicObject()->setProperty ("player", SessionPlayers::nameOf (request.players[i]));
        item.getDynamicObject()->setProperty ("instrument", plugin != nullptr ? plugin->getDescriptor().name : juce::String ("Test Synth"));
        tracks.add (item);
    }

    for (const auto& note : notes)
        told.add (note);

    auto result = object();
    result.getDynamicObject()->setProperty ("tracks", tracks);
    result.getDynamicObject()->setProperty ("chords", request.chords);
    result.getDynamicObject()->setProperty ("start_bar", request.startBar);
    result.getDynamicObject()->setProperty ("times", request.times);

    if (lined.isNotEmpty())
        told.insert (0, lined);

    if (! told.isEmpty())
        result.getDynamicObject()->setProperty ("notes", told);

    return result;
}

//==============================================================================
juce::String AiTools::describe (const juce::String& name, const juce::var& input, const juce::var& result)
{
    if (result.hasProperty ("error"))
        return "Couldn't: " + result.getProperty ("error", {}).toString();

    const auto plugin = result.getProperty ("name", input.getProperty ("plugin", {})).toString();

    if (name == "add_track")          return "Added an " + input.getProperty ("kind", "instrument").toString() + " track"
                                             + (input.getProperty ("name", {}).toString().isNotEmpty() ? " \"" + input.getProperty ("name", {}).toString() + "\"" : juce::String());
    if (name == "remove_track")       return "Removed a track";
    if (name == "set_track")          return "Changed a track's settings";
    if (name == "set_main")           return "Set Main to " + juce::String (static_cast<double> (input.getProperty ("volume_db", 0.0)), 1) + " dB";
    if (name == "load_instrument")    return "Loaded " + plugin + " as the instrument";
    if (name == "add_effect")         return "Added " + plugin;
    if (name == "remove_effect")      return "Removed an effect";
    if (name == "move_effect")        return "Moved an effect";
    if (name == "bypass_effect")      return static_cast<bool> (input.getProperty ("bypassed", true)) ? "Switched an effect off" : "Switched an effect on";
    if (name == "set_plugin_params")
    {
        const auto* changed = result.getProperty ("changed", {}).getArray();
        const auto count = changed != nullptr ? changed->size() : 0;
        return "Adjusted " + (count == 1 ? juce::String ("a setting") : juce::String (count) + " settings") + " on " + result.getProperty ("plugin", {}).toString();
    }
    if (name == "add_fx_channel")     return "Added an FX channel" + (input.getProperty ("name", {}).toString().isNotEmpty() ? " \"" + input.getProperty ("name", {}).toString() + "\"" : juce::String());
    if (name == "add_send")           return "Added a send";
    if (name == "add_sidechain")      return "Added a sidechain send";
    if (name == "set_send")           return "Changed a send";
    if (name == "remove_send")        return "Removed a send";
    if (name == "write_midi_clip")
    {
        const auto* notes = input.getProperty ("notes", {}).getArray();
        return "Wrote a clip: " + input.getProperty ("length_bars", 1).toString() + " bars, " + juce::String (notes != nullptr ? notes->size() : 0) + " notes";
    }
    if (name == "delete_clip")        return "Deleted a clip";
    if (name == "set_tempo")          return input.hasProperty ("remove_at_bar") ? "Removed a tempo change"
                                           : "Tempo " + input.getProperty ("bpm", {}).toString() + " BPM"
                                             + (input.hasProperty ("at_bar") ? " from bar " + input.getProperty ("at_bar", {}).toString() : juce::String());
    if (name == "set_key")            return "Key " + input.getProperty ("root", {}).toString() + " " + input.getProperty ("scale", {}).toString();
    if (name == "set_time_signature") return "Time signature " + input.getProperty ("numerator", {}).toString() + "/" + input.getProperty ("denominator", {}).toString();
    if (name == "transport")
    {
        const auto action = input.getProperty ("action", "none").toString();
        return action == "play" ? "Started playing" : action == "stop" ? "Stopped" : action == "rewind" ? "Back to the start" : "Set the loop";
    }
    if (name == "add_session_players")
    {
        juce::StringArray players;

        if (const auto* tracks = result.getProperty ("tracks", {}).getArray())
            for (const auto& track : *tracks)
                players.add (track.getProperty ("player", {}).toString());

        const auto style = SessionPlayers::nameOf (SessionPlayers::styleFrom (input.getProperty ("style", "pop").toString()));
        return "Added session players (" + players.joinIntoString (", ") + "), " + (style == "hiphop" ? juce::String ("hip-hop") : style);
    }
    if (name == "open_plugin_window") return "Opened a plugin's window";
    if (name == "save_song")          return "Saved the song";
    if (name == "split_stems")        return "Splitting into stems";
    if (name == "add_midi_fx")        return "Added " + nameOfMidiFx (input.getProperty ("type", {}).toString()) + " to a track's MIDI FX";
    if (name == "set_midi_fx")        return "Changed a MIDI FX device";
    if (name == "remove_midi_fx")     return "Removed a MIDI FX device";
    if (name == "apply_midi_tool")    return "Applied " + nameOfMidiFx (input.getProperty ("type", {}).toString()) + " to a clip";
    if (name == "set_board")
    {
        if (input.hasProperty ("mixing") && input.getProperty ("strips", {}).size() == 0 && ! input.getProperty ("master", {}).isObject())
            return static_cast<bool> (input.getProperty ("mixing", false)) ? "The Analog Board mixes the song" : "The Console mixes the song";

        const auto strips = input.getProperty ("strips", {}).size();
        return "Set the Analog Board" + (strips > 0 ? " (" + juce::String (strips) + (strips == 1 ? " strip)" : " strips)") : juce::String());
    }
    return {};   // looking only: get_song, get_clip, find_plugins, get_plugin_params, get_board
}

//==============================================================================
// MIDI FX
juce::String AiTools::nameOfMidiFx (const juce::String& type)
{
    const auto* spec = MidiFx::findType (type);
    return spec != nullptr ? spec->name : juce::String ("a MIDI tool");
}

namespace
{
    /** Settings by name ({ rate: 7 } or { rate: "1/16" }) as the engine keeps them ({ params: { rate: 7 } });
        unknown names, and values that aren't numbers or choices, listed. */
    juce::var midiFxSettings (const MidiFx::TypeSpec& spec, const juce::var& settings, juce::StringArray& unknown)
    {
        auto params = object();

        if (const auto* given = settings.getDynamicObject())
        {
            for (const auto& property : given->getProperties())
            {
                const auto name = property.name.toString();
                const auto param = std::find_if (spec.params.begin(), spec.params.end(), [&] (const MidiFx::ParamSpec& p) { return p.id == name; });

                if (param == spec.params.end())
                {
                    unknown.add (name);
                    continue;
                }

                // A number, true/false, a number as text, or a choice by its name ("1/16", "up").
                const auto& value = property.value;
                const auto text = value.toString().trim();
                double result = 0.0;
                bool ok = true;

                if (value.isBool())
                    result = static_cast<bool> (value) ? 1.0 : 0.0;
                else if (value.isInt() || value.isInt64() || value.isDouble())
                    result = static_cast<double> (value);
                else if (! value.isString() || text.isEmpty())
                    ok = false;
                else if (param->choices.contains (text, true))
                    result = param->choices.indexOf (text, true);
                else if (text.containsOnly ("0123456789.-+eE") && text.containsAnyOf ("0123456789"))
                    result = text.getDoubleValue();
                else if (text.equalsIgnoreCase ("true") || text.equalsIgnoreCase ("on") || text.equalsIgnoreCase ("yes"))
                    result = 1.0;
                else if (text.equalsIgnoreCase ("false") || text.equalsIgnoreCase ("off") || text.equalsIgnoreCase ("no"))
                    result = 0.0;
                else
                    ok = false;

                if (ok)
                    params.getDynamicObject()->setProperty (property.name, result);
                else
                    unknown.add (name + " (" + (text.isEmpty() ? juce::String ("no value") : text.quoted()) + ": a number"
                                 + (param->choices.isEmpty() ? juce::String() : " or one of " + param->choices.joinIntoString (", ")) + ")");
            }
        }

        auto wrapped = object();
        wrapped.getDynamicObject()->setProperty ("params", params);
        return wrapped;
    }
}

juce::var AiTools::addMidiFx (const juce::var& input)
{
    const auto trackId = static_cast<int> (number (input.getProperty ("track_id", {}), -1.0));
    const auto type = input.getProperty ("type", {}).toString();
    const auto* track = engine.findTrack (trackId);
    const auto* spec = MidiFx::findType (type);

    if (track == nullptr || track->kind != Track::Kind::instrument)
        return failure ("MIDI FX go on instrument tracks: there's no instrument track " + juce::String (trackId) + ".");

    if (spec == nullptr || spec->magenta)
        return failure ("There's no MIDI FX kind \"" + type + "\". The kinds are in add_midi_fx's description.");

    juce::StringArray unknown;
    const auto settings = midiFxSettings (*spec, input.getProperty ("settings", {}), unknown);
    const auto id = engine.addMidiFx (trackId, type, static_cast<int> (number (input.getProperty ("position", {}), -1.0)), settings);

    if (id == 0)
        return failure ("That track's MIDI FX are full (8).");

    auto result = success();
    result.getDynamicObject()->setProperty ("fx_id", id);

    if (! unknown.isEmpty())
        result.getDynamicObject()->setProperty ("ignored_settings", unknown.joinIntoString ("; "));

    return result;
}

juce::var AiTools::setMidiFx (const juce::var& input)
{
    const auto trackId = static_cast<int> (number (input.getProperty ("track_id", {}), -1.0));
    const auto fxId = static_cast<int> (number (input.getProperty ("fx_id", {}), -1.0));
    const auto devices = engine.getMidiFx (trackId);
    const juce::var* device = nullptr;

    for (const auto& item : *devices.getArray())
        if (static_cast<int> (item.getProperty ("id", 0)) == fxId)
            device = &item;

    if (device == nullptr)
        return failure ("There's no MIDI FX " + juce::String (fxId) + " on track " + juce::String (trackId) + ". get_song lists them.");

    // The device's own settings (a Sonify device has its own, not a built-in kind's).
    const auto* track = engine.findTrack (trackId);
    const auto* slot = track != nullptr ? track->midiFx.find (fxId) : nullptr;
    const auto* spec = slot != nullptr ? &slot->device->spec : MidiFx::findType (device->getProperty ("type", {}).toString());
    juce::StringArray unknown;

    if (spec != nullptr)
    {
        const auto settings = midiFxSettings (*spec, input.getProperty ("settings", {}), unknown);

        for (size_t i = 0; i < spec->params.size(); ++i)
        {
            const auto& value = settings["params"].getProperty (spec->params[i].id, {});

            if (! value.isVoid())
                engine.setMidiFxParam (trackId, fxId, static_cast<int> (i), static_cast<float> (static_cast<double> (value)));
        }
    }

    if (input.hasProperty ("on"))
        engine.setMidiFxOn (trackId, fxId, static_cast<bool> (input.getProperty ("on", true)));

    if (input.hasProperty ("position"))
        engine.moveMidiFx (trackId, fxId, static_cast<int> (number (input.getProperty ("position", {}), 0.0)));

    auto result = success();

    if (! unknown.isEmpty())
        result.getDynamicObject()->setProperty ("ignored_settings", unknown.joinIntoString ("; "));

    return result;
}

juce::var AiTools::removeMidiFx (const juce::var& input)
{
    const auto trackId = static_cast<int> (number (input.getProperty ("track_id", {}), -1.0));
    const auto fxId = static_cast<int> (number (input.getProperty ("fx_id", {}), -1.0));

    if (! engine.removeMidiFx (trackId, fxId))
        return failure ("There's no MIDI FX " + juce::String (fxId) + " on track " + juce::String (trackId) + ".");

    return success();
}

juce::var AiTools::applyMidiTool (const juce::var& input)
{
    const auto trackId = static_cast<int> (number (input.getProperty ("track_id", {}), -1.0));
    const auto clipId = static_cast<int> (number (input.getProperty ("clip_id", {}), -1.0));
    const auto type = input.getProperty ("type", {}).toString();
    const auto* spec = MidiFx::findType (type);
    const auto* track = engine.findTrack (trackId);

    if (spec == nullptr || spec->magenta)
        return failure ("There's no MIDI tool \"" + type + "\". The kinds are in add_midi_fx's description.");

    if (track == nullptr)
        return failure ("There's no track " + juce::String (trackId) + ".");

    const MidiClip* clip = nullptr;

    for (const auto& c : track->clips)
        if (c.id == clipId && ! c.isAudio())
            clip = &c;

    if (clip == nullptr)
        return failure ("There's no MIDI clip " + juce::String (clipId) + " on track " + juce::String (trackId) + ".");

    juce::StringArray unknown;
    std::vector<std::unique_ptr<MidiFx::Device>> devices;
    devices.push_back (MidiFx::create (type, midiFxSettings (*spec, input.getProperty ("settings", {}), unknown)));

    // The notes the clip shows and plays, at song positions; the ones a trim hides and the muted
    // ones stay as they are.
    std::vector<MidiFx::Note> notes;
    std::vector<MidiNote> hidden, shown;

    for (const auto& n : clip->notes)
    {
        if (! clip->shows (n) || n.muted)
            hidden.push_back (n);
        else
            shown.push_back (n);
    }

    // In the order the tool gives its notes back: by start, then pitch.
    std::stable_sort (shown.begin(), shown.end(), [] (const MidiNote& x, const MidiNote& y)
    {
        return x.start < y.start || (! (y.start < x.start) && x.pitch < y.pitch);
    });

    for (const auto& n : shown)
    {
        MidiFx::Note note;
        note.pitch = n.pitch;
        note.start = clip->songPosition (n.start);
        note.length = n.length;
        note.velocity = juce::jlimit (1, 127, juce::roundToInt (n.velocity * 127.0f));
        notes.push_back (note);
    }

    const auto from = clip->start, to = clip->start + clip->length;
    std::vector<MidiNote> result = hidden;

    engine.prepareOfflineFollowing (devices);   // Harmonize: the track it follows, from its clips
    const auto played = MidiFx::runOffline (devices, std::move (notes), engine.getTempoMap(), from, to);

    // Each note keeps its operators (Chance, Repeats...): one for one when the tool gives back as
    // many notes as it was given (moved, transposed, reshaped), otherwise the note at the same
    // pitch that started at the same moment.
    const auto oneForOne = played.size() == shown.size();

    for (size_t i = 0; i < played.size(); ++i)
    {
        const auto& n = played[i];

        if (n.start >= to || n.start + n.length <= from)
            continue;

        const auto start = juce::jmax (from, n.start);
        MidiNote made { n.pitch, clip->contentPosition (start), n.length - (start - n.start), static_cast<float> (n.velocity) / 127.0f };

        if (oneForOne)
        {
            made.ops = shown[i].ops;
        }
        else
        {
            for (const auto& s : shown)
            {
                if (s.pitch == n.pitch && std::abs (clip->songPosition (s.start) - n.start) < 1.0e-6)
                {
                    made.ops = s.ops;
                    break;
                }
            }
        }

        result.push_back (made);
    }

    if (! engine.setClipNotes (trackId, clipId, std::move (result), nullptr, nullptr, nullptr))
        return failure ("The clip didn't change.");

    auto answer = success();
    answer.getDynamicObject()->setProperty ("notes", static_cast<int> (clip->notes.size()));

    if (! unknown.isEmpty())
        answer.getDynamicObject()->setProperty ("ignored_settings", unknown.joinIntoString ("; "));

    return answer;
}

//==============================================================================
juce::var AiTools::getBoard()
{
    auto result = success();
    auto* fields = result.getDynamicObject();
    fields->setProperty ("mixing", engine.isBoardMixing());
    fields->setProperty ("master", engine.getBoard().settings.toVar (true));

    juce::Array<juce::var> strips;

    for (const auto& track : engine.getTracks())
    {
        if (track->isMixChannel())
            continue;

        auto strip = object();
        strip.getDynamicObject()->setProperty ("track_id", track->id);
        strip.getDynamicObject()->setProperty ("name", track->name);
        strip.getDynamicObject()->setProperty ("settings", track->board.settings.toVar());
        strips.add (strip);
    }

    fields->setProperty ("strips", strips);
    return result;
}

juce::var AiTools::setBoard (const juce::var& input)
{
    // Checked first, so a mistake changes nothing.
    if (input.hasProperty ("mixing") && static_cast<bool> (input.getProperty ("mixing", false)) != engine.isBoardMixing() && engine.isMixdownRunning())
        return failure ("A mixdown is being made: which mixer mixes the song can't change until it's done.");

    std::vector<std::pair<Track*, juce::var>> strips;

    if (const auto* list = input.getProperty ("strips", {}).getArray())
    {
        for (const auto& item : *list)
        {
            const auto id = static_cast<int> (number (item.getProperty ("track_id", {}), -1.0));
            auto* track = engine.findTrack (id);

            if (track == nullptr || track->isMixChannel())
                return failure ("Track " + item.getProperty ("track_id", {}).toString() + " has no strip on the board: only instrument and audio tracks do. "
                                "Call get_board for the ids.");

            strips.emplace_back (track, item.getProperty ("settings", {}));
        }
    }

    if (static_cast<bool> (input.getProperty ("reset", false)))
        engine.resetBoard();

    if (static_cast<bool> (input.getProperty ("copy_console", false)))
        engine.copyConsoleToBoard();

    if (input.hasProperty ("mixing"))
    {
        const auto on = static_cast<bool> (input.getProperty ("mixing", false));

        if (on && ! engine.isBoardMixing() && engine.isBoardFresh())
            engine.copyConsoleToBoard();

        engine.setBoardMixing (on);
    }

    juce::StringArray ignored;

    if (const auto master = input.getProperty ("master", {}); master.isObject())
        for (const auto& id : engine.getBoard().settings.fromVar (master, false))
            ignored.add ("master." + id);

    for (const auto& [track, settings] : strips)
        for (const auto& id : track->board.settings.fromVar (settings, false))
            ignored.add (juce::String (track->id) + "." + id);

    auto result = getBoard();

    if (! ignored.isEmpty())
        result.getDynamicObject()->setProperty ("ignored_settings", "Left out (see set_board's list): " + ignored.joinIntoString (", "));

    return result;
}
