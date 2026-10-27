// D:\workspace\Fanan_web_hub\src\Song.cpp

#include "Song.h"
#include "BuiltinPlugins.h"
#include "Container.h"
#include "CreatorPlugin.h"
#include "DeviceHost.h"
#include "Preferences.h"

#include <limits>

namespace
{
    //==============================================================================
    juce::var object()
    {
        return juce::var (new juce::DynamicObject());
    }

    double number (const juce::var& value, double fallback)
    {
        if (! (value.isDouble() || value.isInt() || value.isInt64()))
            return fallback;

        const auto result = static_cast<double> (value);
        return std::isfinite (result) ? result : fallback;
    }

    //==============================================================================
    // Plugins: the file, the id, the plugin's own state, and its parameter values.
    juce::var pluginToVar (Plugin& plugin)
    {
        auto item = object();
        auto* fields = item.getDynamicObject();
        fields->setProperty ("file", plugin.getSourceFile().getFullPathName());
        fields->setProperty ("pluginId", plugin.getDescriptor().id);
        fields->setProperty ("name", plugin.getDescriptor().name);

        juce::MemoryBlock state;

        if (plugin.saveState (state))
            fields->setProperty ("state", juce::Base64::toBase64 (state.getData(), state.getSize()));

        juce::Array<juce::var> values;
        const auto& parameters = plugin.getParameters();

        for (size_t i = 0; i < parameters.size(); ++i)
            values.add (juce::Array<juce::var> { static_cast<juce::int64> (parameters[i].id),
                                                 plugin.readParameterValue (static_cast<int> (i)) });

        fields->setProperty ("params", values);
        return item;
    }

    /** Loads a saved plugin onto a channel; returns its insert id (0 for an instrument), or -1.
        wantedInsertId: the id an insert is to have again (an undo's snapshot), when it's free. */
    int restorePlugin (AudioEngine& engine, int channel, const juce::var& item, juce::StringArray& problems, int wantedInsertId = 0)
    {
        const auto name = item.getProperty ("name", "A plugin").toString();
        const auto pluginId = item.getProperty ("pluginId", {}).toString();

        // Sonify's own effects are wherever Sonify is now, whatever path the song kept.
        // A made plugin kept as an old .sonifyplugin folder: its .sip now, found by its id.
        const auto file = Builtins::isBuiltinId (pluginId) ? Builtins::file()
                                                           : MadePlugins::resolve (juce::File (item.getProperty ("file", {}).toString()), pluginId);

        if (! juce::File::isAbsolutePath (file.getFullPathName()) || ! file.exists())
        {
            problems.add (name + " is missing: " + file.getFullPathName());
            return -1;
        }

        int insertId = 0;
        const auto result = engine.loadPlugin (channel, file, pluginId, -1, AudioEngine::instrumentSlot, insertId, nullptr, wantedInsertId);

        if (result.failed())
        {
            problems.add (result.getErrorMessage());
            return -1;
        }

        auto* plugin = engine.getPlugin (channel, insertId);

        if (plugin == nullptr)
            return insertId;

        // The plugin's own state first; its parameter values when it has none (or it won't load).
        bool restored = false;
        const auto stateText = item.getProperty ("state", {}).toString();

        if (stateText.isNotEmpty() && plugin->canSaveState())
        {
            juce::MemoryOutputStream decoded;

            if (juce::Base64::convertFromBase64 (decoded, stateText))
                restored = plugin->loadState (decoded.getMemoryBlock());

            // A container whose plugins aren't all on this computer keeps them as they were, and says so.
            if (auto* container = dynamic_cast<ContainerPlugin*> (plugin))
                for (const auto& problem : container->getProblems())
                    problems.add (name + ": " + problem);
        }

        if (! restored)
        {
            if (const auto* values = item.getProperty ("params", {}).getArray())
            {
                for (const auto& pair : *values)
                {
                    const auto* entry = pair.getArray();

                    if (entry == nullptr || entry->size() < 2)
                        continue;

                    const auto index = plugin->findParameter (static_cast<clap_id> (static_cast<juce::int64> ((*entry)[0])));

                    if (index >= 0)
                        engine.setPluginParameter (channel, insertId, index, number ((*entry)[1], 0.0));
                }
            }
        }

        return insertId;
    }

    /** A DAWproject's placeholders (Track::kept) as songs keep them: their settings files relative
        to the song's folder when they're in it. */
    juce::var keptToVar (const juce::var& kept, const juce::File& songFile);
    juce::var keptFromVar (const juce::var& list, const juce::File& songFile);

    /** withIds: each insert's id too (an undo's snapshot of the song, which puts them back as they were). */
    juce::var insertsToVar (const EffectChain& chain, bool withIds = false)
    {
        juce::Array<juce::var> list;

        for (int i = 0; i < chain.size(); ++i)
        {
            if (auto* insert = chain.at (i); insert != nullptr && insert->plugin != nullptr)
            {
                auto item = pluginToVar (*insert->plugin);
                item.getDynamicObject()->setProperty ("bypass", insert->bypassed.load());

                if (withIds)
                    item.getDynamicObject()->setProperty ("id", insert->id);

                list.add (item);
            }
        }

        return list;
    }

    /** A channel's inserts, in their saved order; returns the insert id each saved place got (0: it
        didn't load), since what names an insert by its place (an envelope, a sidechain send) means
        the one saved there, not the one that moved up into it. keepIds: each takes its saved id. */
    std::vector<int> restoreInserts (AudioEngine& engine, int channel, const juce::var& list, juce::StringArray& problems,
                                     bool keepIds = false)
    {
        std::vector<int> insertIds;

        if (const auto* inserts = list.getArray())
        {
            for (const auto& item : *inserts)
            {
                const auto wanted = keepIds ? static_cast<int> (number (item.getProperty ("id", {}), 0.0)) : 0;
                const auto insertId = restorePlugin (engine, channel, item, problems, wanted);
                insertIds.push_back (juce::jmax (0, insertId));

                if (insertId > 0 && static_cast<bool> (item.getProperty ("bypass", false)))
                    engine.setInsertBypass (channel, insertId, true);
            }
        }

        return insertIds;
    }

    /** What a saved place names among a channel's inserts: the id of the one restored there (0 when
        none: it didn't load, or there was none). insertIds null: the channel as it is (a copy made of
        a track in this song names its places as they are now). */
    int insertAtPlace (const EffectChain& chain, const std::vector<int>* insertIds, int place)
    {
        if (place < 0)
            return 0;

        if (insertIds != nullptr)
            return place < static_cast<int> (insertIds->size()) ? (*insertIds)[static_cast<size_t> (place)] : 0;

        const auto* insert = chain.at (place);
        return insert != nullptr ? insert->id : 0;
    }

    //==============================================================================
    /** Relative to the song's folder when it's inside it, so a song folder can move whole. */
    juce::String mediaPath (const juce::String& path, const juce::File& songFile)
    {
        const juce::File file (path);
        const auto folder = songFile.getParentDirectory();

        if (songFile != juce::File() && file.isAChildOf (folder))
            return file.getRelativePathFrom (folder).replaceCharacter ('\\', '/');

        return path;
    }

    juce::File mediaFile (const juce::String& path, const juce::File& songFile)
    {
        if (juce::File::isAbsolutePath (path) || songFile == juce::File())
            return juce::File (path);

        return songFile.getParentDirectory().getChildFile (path);
    }

    juce::var keptToVar (const juce::var& kept, const juce::File& songFile)
    {
        juce::Array<juce::var> list;

        if (const auto* items = kept.getArray())
        {
            for (const auto& item : *items)
            {
                auto copy = juce::JSON::parse (juce::JSON::toString (item, true));

                if (auto* fields = copy.getDynamicObject(); fields != nullptr && fields->hasProperty ("stateFile"))
                    fields->setProperty ("stateFile", mediaPath (fields->getProperty ("stateFile").toString(), songFile));

                list.add (copy);
            }
        }

        return list;
    }

    juce::var keptFromVar (const juce::var& list, const juce::File& songFile)
    {
        juce::Array<juce::var> kept;

        if (const auto* items = list.getArray())
        {
            for (const auto& item : *items)
            {
                if (! item.isObject() || item.getProperty ("xml", {}).toString().isEmpty())
                    continue;

                auto copy = juce::JSON::parse (juce::JSON::toString (item, true));

                if (auto* fields = copy.getDynamicObject(); fields != nullptr && fields->getProperty ("stateFile").toString().isNotEmpty())
                    fields->setProperty ("stateFile", mediaFile (fields->getProperty ("stateFile").toString(), songFile).getFullPathName());

                kept.add (copy);
            }
        }

        return kept.isEmpty() ? juce::var() : juce::var (kept);
    }

    /** The Stretcher's time map from [render seconds, window seconds] points: rising, in range. */
    std::vector<std::pair<double, double>> readWarp (const juce::var& list)
    {
        std::vector<std::pair<double, double>> points;

        if (const auto* items = list.getArray())
        {
            for (const auto& value : *items)
            {
                const auto* pair = value.getArray();

                if (pair == nullptr || pair->size() < 2)
                    continue;

                const auto out = number ((*pair)[0], -1.0), in = number ((*pair)[1], -1.0);

                if (out < 0.0 || in < 0.0 || (! points.empty() && (out <= points.back().first || in < points.back().second)))
                    return {};   // not a time map: none

                points.push_back ({ out, in });
            }
        }

        return points.size() >= 2 ? points : std::vector<std::pair<double, double>>();
    }

    MidiClip::Silence readSilence (const juce::var& value)
    {
        MidiClip::Silence silence;

        if (value.isObject())
        {
            silence.on = true;
            silence.thresholdDb = juce::jlimit (-120.0, 0.0, number (value.getProperty ("threshold", {}), -50.0));
            silence.shortest = juce::jlimit (0.01, 10.0, number (value.getProperty ("shortest", {}), 0.15));
            silence.fade = juce::jlimit (0.001, 1.0, number (value.getProperty ("fade", {}), 0.01));
        }

        return silence;
    }

    juce::var clipToVar (const MidiClip& clip, const juce::File& songFile)
    {
        juce::Array<juce::var> notes;

        // Each [pitch, start, length, velocity 1-127], then 1 for a muted note, then its operators
        // (MidiNote::toVar).
        for (const auto& note : clip.notes)
            notes.add (note.toVar());

        auto item = object();
        auto* fields = item.getDynamicObject();
        fields->setProperty ("start", clip.start);
        fields->setProperty ("length", clip.length);
        fields->setProperty ("offset", clip.offset);
        fields->setProperty ("notes", notes);

        if (clip.muted)
            fields->setProperty ("muted", true);

        if (clip.seed != 0)
            fields->setProperty ("seed", static_cast<juce::int64> (clip.seed));

        // Pitch bend and aftertouch as [time, value] points, when the clip has them.
        const auto line = [] (const std::vector<MidiControlPoint>& points)
        {
            juce::Array<juce::var> list;

            for (const auto& point : points)
                list.add (juce::Array<juce::var> { point.time, point.value });

            return list;
        };

        if (! clip.pitchBend.empty())
            fields->setProperty ("pitchBend", line (clip.pitchBend));

        if (! clip.aftertouch.empty())
            fields->setProperty ("aftertouch", line (clip.aftertouch));

        // Its own automation lines.
        if (! clip.envelopes.empty())
            fields->setProperty ("envelopes", Song::envelopesToVar (clip.envelopes));

        if (clip.isAudio())
        {
            fields->setProperty ("audioFile", mediaPath (clip.audioFile, songFile));
            fields->setProperty ("audioOffset", clip.audioOffset);
            fields->setProperty ("audioLength", clip.audioLength);
            fields->setProperty ("gain", clip.audioGainDb);
            fields->setProperty ("fadeIn", clip.fadeIn);
            fields->setProperty ("fadeOut", clip.fadeOut);

            if (clip.isRender())
            {
                fields->setProperty ("sourceFile", mediaPath (clip.sourceFile, songFile));
                fields->setProperty ("sourceOffset", clip.sourceOffset);
                fields->setProperty ("sourceLength", clip.sourceLength);
                fields->setProperty ("sourceTempo", clip.sourceTempo);
                fields->setProperty ("pitch", clip.pitch);
                fields->setProperty ("reversed", clip.reversed);
                fields->setProperty ("followTempo", clip.followTempo);
            }

            // The Stretcher's time map, as [render seconds, window seconds] points.
            if (! clip.warp.empty())
            {
                juce::Array<juce::var> points;

                for (const auto& [out, in] : clip.warp)
                    points.add (juce::Array<juce::var> { out, in });

                fields->setProperty ("warp", points);
            }

            // Silence Areas.
            if (clip.silence.on)
            {
                auto quiet = object();
                quiet.getDynamicObject()->setProperty ("threshold", clip.silence.thresholdDb);
                quiet.getDynamicObject()->setProperty ("shortest", clip.silence.shortest);
                quiet.getDynamicObject()->setProperty ("fade", clip.silence.fade);
                fields->setProperty ("silence", quiet);
            }

            // Its own effect: the plugin and its state (taken from it just before, AudioEngine::captureClipEffects).
            if (const auto& effect = clip.effect)
            {
                auto fx = object();
                auto* fxFields = fx.getDynamicObject();
                fxFields->setProperty ("file", effect->file);
                fxFields->setProperty ("pluginId", effect->pluginId);
                fxFields->setProperty ("name", effect->name);
                fxFields->setProperty ("format", effect->format);

                if (effect->state.getSize() > 0)
                    fxFields->setProperty ("state", juce::Base64::toBase64 (effect->state.getData(), effect->state.getSize()));

                fields->setProperty ("effect", fx);
            }
        }

        return item;
    }

    MidiClip clipFromVar (const juce::var& item, const juce::File& songFile)
    {
        MidiClip clip;
        const auto audioFile = item.getProperty ("audioFile", {}).toString();

        if (audioFile.isNotEmpty())
        {
            clip.audioFile = mediaFile (audioFile, songFile).getFullPathName();
            clip.audioOffset = number (item.getProperty ("audioOffset", {}), 0.0);
            clip.audioLength = number (item.getProperty ("audioLength", {}), 0.0);
            clip.audioGainDb = juce::jlimit (-60.0, 12.0, number (item.getProperty ("gain", {}), 0.0));
            clip.fadeIn = juce::jmax (0.0, number (item.getProperty ("fadeIn", {}), 0.0));
            clip.fadeOut = juce::jmax (0.0, number (item.getProperty ("fadeOut", {}), 0.0));

            if (const auto source = item.getProperty ("sourceFile", {}).toString(); source.isNotEmpty())
            {
                clip.sourceFile = mediaFile (source, songFile).getFullPathName();
                clip.sourceOffset = juce::jmax (0.0, number (item.getProperty ("sourceOffset", {}), 0.0));
                clip.sourceLength = juce::jmax (0.0, number (item.getProperty ("sourceLength", {}), 0.0));
                clip.sourceTempo = juce::jlimit (0.0, 400.0, number (item.getProperty ("sourceTempo", {}), 0.0));
                clip.pitch = juce::jlimit (-24.0, 24.0, number (item.getProperty ("pitch", {}), 0.0));
                clip.reversed = static_cast<bool> (item.getProperty ("reversed", false));
                clip.followTempo = static_cast<bool> (item.getProperty ("followTempo", false)) && clip.sourceTempo > 0.0;
            }

            clip.warp = readWarp (item.getProperty ("warp", {}));
            clip.silence = readSilence (item.getProperty ("silence", {}));

            // Its own effect (kept even when its plugin isn't on this computer: the clip says so).
            if (const auto fx = item.getProperty ("effect", {}); fx.isObject() && fx.getProperty ("pluginId", {}).toString().isNotEmpty())
            {
                auto effect = std::make_shared<ClipEffect>();
                effect->file = fx.getProperty ("file", {}).toString();
                effect->pluginId = fx.getProperty ("pluginId", {}).toString();
                effect->name = fx.getProperty ("name", {}).toString();
                effect->format = fx.getProperty ("format", {}).toString();
                effect->uid = ClipEffect::newUid();

                if (const auto state = fx.getProperty ("state", {}).toString(); state.isNotEmpty())
                {
                    juce::MemoryOutputStream decoded;

                    if (juce::Base64::convertFromBase64 (decoded, state))
                        effect->state = decoded.getMemoryBlock();
                }

                clip.effect = std::move (effect);
            }
        }

        clip.start = juce::jmax (0.0, number (item.getProperty ("start", {}), 0.0));
        clip.length = number (item.getProperty ("length", {}), 4.0);
        clip.offset = number (item.getProperty ("offset", {}), 0.0);
        clip.muted = static_cast<bool> (item.getProperty ("muted", false));
        clip.seed = static_cast<juce::uint32> (static_cast<juce::int64> (number (item.getProperty ("seed", {}), 0.0)));

        if (const auto* notes = item.getProperty ("notes", {}).getArray())
        {
            for (const auto& value : *notes)
                if (MidiNote note; MidiNote::fromVar (value, note))
                    clip.notes.push_back (note);
        }

        const auto line = [] (const juce::var& list, int minimum, int maximum)
        {
            std::vector<MidiControlPoint> points;

            if (const auto* items = list.getArray())
                for (const auto& value : *items)
                    if (const auto* fields = value.getArray(); fields != nullptr && fields->size() >= 2)
                        points.push_back ({ number ((*fields)[0], 0.0), juce::roundToInt (number ((*fields)[1], 0.0)) });

            MidiClip::tidy (points, minimum, maximum);
            return points;
        };

        clip.pitchBend = line (item.getProperty ("pitchBend", {}), MidiClip::pitchBendMin, MidiClip::pitchBendMax);
        clip.aftertouch = line (item.getProperty ("aftertouch", {}), 0, MidiClip::aftertouchMax);
        clip.envelopes = Song::envelopesFromVar (item.getProperty ("envelopes", {}), clip.isAudio());

        return clip;
    }

    /** A track as songs keep it: its settings, plugins, envelopes, sends and clips. Other tracks
        it names (a send's FX channel, a track it takes MIDI from) are named by their place.
        withIds: its id, its inserts' and its clips' too (an undo's snapshot of the song). */
    juce::var captureTrack (AudioEngine& engine, const Track& track, const juce::File& songFile, bool withIds = false)
    {
        auto item = object();
        auto* trackFields = item.getDynamicObject();
        trackFields->setProperty ("kind", track.kind == Track::Kind::audio ? "audio" : track.kind == Track::Kind::fx ? "fx"
                                        : track.kind == Track::Kind::bus ? "bus" : "instrument");
        trackFields->setProperty ("autoMode", Automation::modeName (track.automationMode.load()));

        if (withIds)
            trackFields->setProperty ("id", track.id);

        // Envelopes; a plugin parameter's insert is named by its place in the chain, since
        // insert ids are made afresh when a song opens (-1: the instrument).
        juce::Array<juce::var> lanes;

        for (const auto& lane : track.automation)
        {
            const auto& target = lane.target;

            // An envelope of an insert that's out of the song (removed or replaced; an undo would
            // bring it back) has no place to be named by: one by its place would name another
            // plugin, or the instrument. Files leave it out; an undo's snapshot keeps it by the
            // insert's id alone (slot), for when that undo brings the insert back.
            const bool insertGone = target.kind == AutomationTarget::Kind::parameter && target.slot != AudioEngine::instrumentSlot
                                    && track.inserts.indexOf (target.slot) < 0;

            if (insertGone && ! withIds)
                continue;

            auto laneItem = object();
            auto* laneFields = laneItem.getDynamicObject();
            laneFields->setProperty ("target", target.kind == AutomationTarget::Kind::volume ? "volume"
                                               : target.kind == AutomationTarget::Kind::pan ? "pan" : "param");

            if (target.kind == AutomationTarget::Kind::parameter)
            {
                if (! insertGone)
                    laneFields->setProperty ("insert", target.slot == AudioEngine::instrumentSlot ? -1 : track.inserts.indexOf (target.slot));

                if (withIds)
                    laneFields->setProperty ("slot", target.slot);

                laneFields->setProperty ("param", static_cast<juce::int64> (target.paramId));
            }

            juce::Array<juce::var> points;

            for (const auto& point : lane.points)
                points.add (juce::Array<juce::var> { point.ppq, point.value });

            laneFields->setProperty ("points", points);

            // The Automation view's shape regions on it, as the page keeps them.
            if (lane.shapes.isArray() && lane.shapes.size() > 0)
                laneFields->setProperty ("shapes", juce::JSON::parse (juce::JSON::toString (lane.shapes, true)));

            lanes.add (laneItem);
        }

        trackFields->setProperty ("automation", lanes);
        trackFields->setProperty ("input", track.input);

        if (track.kind == Track::Kind::audio)
            trackFields->setProperty ("mono", track.monoMode.load());   // its channel: mono or stereo

        // A choir track's voices (Choir.h); the track its chords come from, by its place.
        if (track.choir != nullptr)
        {
            const auto& choirSettings = track.choir->getSettings();
            auto choir = choirSettings.toVar();
            int chordIndex = -1;

            for (size_t i = 0; i < engine.getTracks().size(); ++i)
                if (choirSettings.chordTrack != 0 && engine.getTracks()[i]->id == choirSettings.chordTrack)
                    chordIndex = static_cast<int> (i);

            choir.getDynamicObject()->setProperty ("chordTrack", chordIndex);
            trackFields->setProperty ("choir", choir);
        }

        // Frozen: its render (it plays that until something it plays changes), made for the
        // Console or (dry) for the Analog Board.
        if (track.isFrozen())
        {
            trackFields->setProperty ("frozen", mediaPath (track.frozenFile.getFullPathName(), songFile));
            trackFields->setProperty ("frozenDry", track.frozenDry);
        }

        trackFields->setProperty ("output", track.output);
        trackFields->setProperty ("name", track.name);
        trackFields->setProperty ("nameIsAutomatic", track.nameIsAutomatic);
        trackFields->setProperty ("colour", juce::String::toHexString (static_cast<juce::int64> (track.colour)).paddedLeft ('0', 8));   // AARRGGBB
        trackFields->setProperty ("volume", static_cast<double> (track.volumeDb.load()));
        trackFields->setProperty ("pan", static_cast<double> (track.pan.load()));
        trackFields->setProperty ("mute", track.mute.load());
        trackFields->setProperty ("solo", track.solo.load());
        trackFields->setProperty ("polarity", track.polarity.load());

        if (track.drumMap.isObject())
            trackFields->setProperty ("drumMap", track.drumMap);

        // A DAWproject's devices that can't play here, kept as they came (placeholders).
        if (const auto* kept = track.kept.getArray(); kept != nullptr && ! kept->isEmpty())
            trackFields->setProperty ("kept", keptToVar (track.kept, songFile));

        trackFields->setProperty ("monitor", track.monitor.load());
        trackFields->setProperty ("monitorDecided", track.monitorDecided);
        trackFields->setProperty ("recordArm", track.recordArm.load());
        trackFields->setProperty ("instrument", track.instrument != nullptr ? pluginToVar (*track.instrument) : juce::var());

        if (track.instrument != nullptr && track.instrumentOff.load())
            trackFields->setProperty ("instrumentOff", true);
        trackFields->setProperty ("inserts", insertsToVar (track.inserts, withIds));

        if (track.midiFx.size() > 0)
            trackFields->setProperty ("midiFx", engine.getMidiFx (track.id));

        // Sends, their destinations named by place (tracks by index, inserts by position),
        // since ids are made afresh when a song opens.
        juce::Array<juce::var> sendList;
        const auto& allTracks = engine.getTracks();

        for (const auto& send : track.sends)
        {
            auto sendItem = object();
            auto* sendFields = sendItem.getDynamicObject();
            sendFields->setProperty ("sidechain", send.sidechain);
            sendFields->setProperty ("level", send.levelDb);
            sendFields->setProperty ("on", send.on);

            int targetIndex = -1;

            for (size_t i = 0; i < allTracks.size(); ++i)
                if (allTracks[i]->id == send.target)
                    targetIndex = static_cast<int> (i);

            // An undo's snapshot names its destination by id too: the track or insert may be out of
            // the song now, until an undo brings it back.
            if (withIds)
            {
                sendFields->setProperty ("targetId", send.target);
                sendFields->setProperty ("slot", send.slot);
            }

            // A sidechain's insert is named by its place (-1: the instrument). One that's out of the
            // song (removed or replaced; an undo would bring it back) has no place: a file leaves
            // the send out (by a place it would key another plugin, or the instrument).
            bool named = true;

            if (send.sidechain && send.target == AudioEngine::mainChannel)
            {
                const auto place = engine.getMainInserts().indexOf (send.slot);
                named = place >= 0;

                if (named)
                {
                    sendFields->setProperty ("main", true);
                    sendFields->setProperty ("insert", place);
                }
            }
            else if (targetIndex >= 0)
            {
                sendFields->setProperty ("track", targetIndex);

                if (send.sidechain)
                {
                    const auto place = send.slot == AudioEngine::instrumentSlot
                                           ? -1 : allTracks[static_cast<size_t> (targetIndex)]->inserts.indexOf (send.slot);
                    named = send.slot == AudioEngine::instrumentSlot || place >= 0;

                    if (named)
                        sendFields->setProperty ("insert", place);
                }
            }
            else
            {
                named = false;   // its destination is gone
            }

            if (! named && ! withIds)
                continue;

            sendList.add (sendItem);
        }

        trackFields->setProperty ("sends", sendList);
        trackFields->setProperty ("height", track.height);

        // Its strip on the Analog Board: the settings away from where a strip starts. Its dynamic
        // EQ's key (another track) is named by its place, as ids are made afresh when a song opens.
        if (! track.isMixChannel())
        {
            const auto board = track.board.settings.toVar();

            if (auto* boardFields = board.getDynamicObject())
            {
                boardFields->removeProperty ("deqKey");

                if (boardFields->getProperties().size() > 0)
                    trackFields->setProperty ("board", board);
            }

            if (const auto key = track.board.getKeyTrack(); key != 0 && key != track.id)
                for (size_t i = 0; i < allTracks.size(); ++i)
                    if (allTracks[i]->id == key)
                        trackFields->setProperty ("boardKey", static_cast<int> (i));
        }

        // Its channel on the Live Show: whether it's one of the show's channels, and its settings
        // away from where a channel starts.
        if (! track.isMixChannel())
        {
            const auto liveValues = track.live.settings.toVar();
            const bool isChannel = track.liveChannel.load();

            if (isChannel || liveValues.getDynamicObject()->getProperties().size() > 0)
            {
                auto channel = object();
                channel.getDynamicObject()->setProperty ("channel", isChannel);
                channel.getDynamicObject()->setProperty ("values", liveValues);
                trackFields->setProperty ("live", channel);
            }
        }

        // The bus it plays into, by its place.
        if (const auto bus = engine.getTrackBus (track.id); bus != 0)
            for (size_t i = 0; i < allTracks.size(); ++i)
                if (allTracks[i]->id == bus)
                    trackFields->setProperty ("bus", static_cast<int> (i));

        // MIDI in: devices by name; other tracks by their place in the song.
        juce::Array<juce::var> midiList;

        for (const auto& input : track.midiInputs)
        {
            if (input.startsWith ("track:"))
            {
                const auto sourceId = input.substring (6).getIntValue();

                for (size_t i = 0; i < allTracks.size(); ++i)
                    if (allTracks[i]->id == sourceId)
                        midiList.add ("track:" + juce::String (static_cast<int> (i)));
            }
            else
            {
                midiList.add (input);
            }
        }

        trackFields->setProperty ("midiInputs", midiList);

        juce::Array<juce::var> clips;

        for (const auto& clip : track.clips)
        {
            auto clipItem = clipToVar (clip, songFile);

            if (withIds)
                clipItem.getDynamicObject()->setProperty ("id", clip.id);

            clips.add (clipItem);
        }

        trackFields->setProperty ("clips", clips);
        return item;
    }

    /** A track from a song: added at the end with everything but its sends and MIDI inputs (they
        can name tracks still to come; restoreTrackLinks puts them in). Its monitoring is handed
        back for later, and the insert id each saved place of its chain got (insertIds). keepIds:
        it takes its saved id, and its inserts and clips theirs (an undo's snapshot). Returns its id. */
    int restoreTrack (AudioEngine& engine, const juce::var& item, juce::StringArray& problems, const juce::File& songFile, bool& monitor,
                      bool keepIds = false, std::vector<int>* insertIds = nullptr)
    {
        const auto kindName = item.getProperty ("kind", {}).toString();
        const auto audio = kindName == "audio";
        const auto id = engine.addTrack (audio ? Track::Kind::audio : kindName == "fx" ? Track::Kind::fx
                                         : kindName == "bus" ? Track::Kind::bus : Track::Kind::instrument,
                                         keepIds ? static_cast<int> (number (item.getProperty ("id", {}), 0.0)) : 0);

        if (audio)
        {
            const auto input = static_cast<int> (number (item.getProperty ("input", {}), 0.0));
            engine.setTrackInput (id, input);

            // Its channel. A song from before channels had one heard a mono input on several
            // channels as one sound (mono), and the rest as stereo does.
            const auto& inputs = engine.getInputs();
            const auto oneSound = juce::isPositiveAndBelow (input, static_cast<int> (inputs.size()))
                               && inputs[static_cast<size_t> (input)].right < 0 && ! inputs[static_cast<size_t> (input)].also.empty();
            engine.setTrackMono (id, item.hasProperty ("mono") ? static_cast<bool> (item.getProperty ("mono", false)) : oneSound);

            // A choir track: its voices now, its chord track once every track is in (restoreTrackLinks).
            if (const auto choir = item.getProperty ("choir", {}); choir.isObject())
            {
                auto settings = ChoirSettings::fromVar (choir);
                settings.chordTrack = 0;
                engine.setTrackChoir (id, settings);
            }
        }

        engine.setTrackOutput (id, static_cast<int> (number (item.getProperty ("output", {}), 0.0)));

        const auto instrument = item.getProperty ("instrument", {});

        if (instrument.isObject())
        {
            restorePlugin (engine, id, instrument, problems);

            // Switched off on its window's header when the song was saved.
            if (auto* restored = engine.findTrack (id); restored != nullptr && restored->instrument != nullptr)
                restored->instrumentOff.store (static_cast<bool> (item.getProperty ("instrumentOff", false)));
        }

        const auto placed = restoreInserts (engine, id, item.getProperty ("inserts", {}), problems, keepIds);

        if (insertIds != nullptr)
            *insertIds = placed;

        // MIDI FX in front of the instrument; a kind this Sonify doesn't have is left out.
        if (const auto* devices = item.getProperty ("midiFx", {}).getArray())
        {
            engine.setMidiFx (id, item.getProperty ("midiFx", {}));

            if (const auto* track = engine.findTrack (id); track != nullptr && track->midiFx.size() < devices->size())
                problems.add ("Some MIDI FX on \"" + item.getProperty ("name", {}).toString() + "\" aren't in this Sonify: they were left out.");
        }

        // After the instrument, which renames a track that still has its automatic name.
        auto* track = engine.findTrack (id);
        const auto name = item.getProperty ("name", {}).toString();

        if (name.isNotEmpty())
            track->name = name;

        track->nameIsAutomatic = static_cast<bool> (item.getProperty ("nameIsAutomatic", false));

        const auto colour = item.getProperty ("colour", {}).toString();

        if (colour.length() == 8 && colour.containsOnly ("0123456789abcdefABCDEF"))
            track->colour = static_cast<juce::uint32> (colour.getHexValue64());

        track->volumeDb.store (static_cast<float> (juce::jlimit (-144.0, 10.0, number (item.getProperty ("volume", {}), 0.0))));
        track->pan.store (static_cast<float> (juce::jlimit (-1.0, 1.0, number (item.getProperty ("pan", {}), 0.0))));
        track->mute.store (static_cast<bool> (item.getProperty ("mute", false)));
        track->solo.store (static_cast<bool> (item.getProperty ("solo", false)));
        track->polarity.store (static_cast<bool> (item.getProperty ("polarity", false)));
        track->drumMap = Track::sanitiseDrumMap (item.getProperty ("drumMap", {}));
        track->kept = keptFromVar (item.getProperty ("kept", {}), songFile);
        monitor = static_cast<bool> (item.getProperty ("monitor", false));

        // Whether Sonify may still turn Monitor on by itself; a song from before kept it for
        // tracks with no instrument plugin yet.
        track->monitorDecided = item.hasProperty ("monitorDecided") ? static_cast<bool> (item.getProperty ("monitorDecided", false))
                                                                   : track->instrument != nullptr;
        engine.setTrackRecordArm (id, static_cast<bool> (item.getProperty ("recordArm", false)));

        // Envelopes, once the track's plugins are in (inserts are found by their saved place).
        if (const auto* lanes = item.getProperty ("automation", {}).getArray())
        {
            std::vector<AutomationLane> restored;

            for (const auto& laneItem : *lanes)
            {
                AutomationLane lane;
                const auto kind = laneItem.getProperty ("target", {}).toString();
                lane.target.kind = kind == "pan" ? AutomationTarget::Kind::pan
                                 : kind == "param" ? AutomationTarget::Kind::parameter : AutomationTarget::Kind::volume;

                if (lane.target.kind == AutomationTarget::Kind::parameter)
                {
                    // An undo's snapshot: the slot itself (its inserts have their ids again). Else
                    // -1 names the instrument; a place, the insert restored there; anything else (an
                    // insert that didn't load, none named) leaves the envelope out.
                    const auto place = static_cast<int> (number (laneItem.getProperty ("insert", {}), -2.0));
                    const auto insertId = juce::isPositiveAndBelow (place, static_cast<int> (placed.size())) ? placed[static_cast<size_t> (place)] : 0;

                    if (keepIds && laneItem.hasProperty ("slot"))
                        lane.target.slot = static_cast<int> (number (laneItem.getProperty ("slot", {}), 0.0));
                    else if (place != -1 && insertId <= 0)
                        continue;
                    else
                        lane.target.slot = place == -1 ? AudioEngine::instrumentSlot : insertId;

                    lane.target.paramId = static_cast<uint32_t> (static_cast<juce::int64> (laneItem.getProperty ("param", 0)));
                }

                if (const auto* points = laneItem.getProperty ("points", {}).getArray())
                    for (const auto& point : *points)
                        if (const auto* pair = point.getArray(); pair != nullptr && pair->size() == 2)
                            lane.points.push_back ({ number ((*pair)[0], -1.0), number ((*pair)[1], 0.0) });

                if (const auto shapes = laneItem.getProperty ("shapes", {}); shapes.isArray())
                    lane.shapes = juce::JSON::parse (juce::JSON::toString (shapes, true));

                restored.push_back (std::move (lane));
            }

            engine.setAutomationLanes (id, std::move (restored));
        }

        engine.setAutomationMode (id, Automation::modeFromName (item.getProperty ("autoMode", "read").toString()));

        if (auto* boardTrack = engine.findTrack (id); boardTrack != nullptr && item.getProperty ("board", {}).isObject())
            boardTrack->board.settings.fromVar (item.getProperty ("board", {}), true);

        if (auto* liveTrack = engine.findTrack (id); liveTrack != nullptr && ! liveTrack->isMixChannel())
        {
            if (const auto liveData = item.getProperty ("live", {}); liveData.isObject())
            {
                liveTrack->live.settings.fromVar (liveData.getProperty ("values", {}), true);
                engine.setTrackLive (id, static_cast<bool> (liveData.getProperty ("channel", false)));
            }
        }

        if (const auto* clips = item.getProperty ("clips", {}).getArray())
        {
            for (const auto& value : *clips)
            {
                auto clip = clipFromVar (value, songFile);

                if (clip.isAudio() && ! juce::File (clip.audioFile).existsAsFile())
                    problems.add ("An audio file is missing: " + clip.audioFile);

                if (keepIds)
                    clip.id = static_cast<int> (number (value.getProperty ("id", {}), 0.0));

                engine.addClip (id, clip, keepIds);
            }
        }

        return id;
    }

    /** A track's height, MIDI inputs and sends, from a song: other tracks are named by their
        place among ids, and inserts by their saved place among the ids restoreInserts gave each
        channel (insertIds, by track id; Main's under AudioEngine::mainChannel). insertIds null:
        by their place in the channels as they are now. keepIds: an undo's snapshot, whose sends
        name their destinations by id (the tracks and inserts have their ids again). */
    void restoreTrackLinks (AudioEngine& engine, int id, const juce::var& songTrack, const std::vector<int>& ids,
                            const std::map<int, std::vector<int>>* insertIds = nullptr, bool keepIds = false)
    {
        const auto placesOf = [insertIds] (int channel) -> const std::vector<int>*
        {
            if (insertIds == nullptr)
                return nullptr;

            static const std::vector<int> none;
            const auto found = insertIds->find (channel);
            return found != insertIds->end() ? &found->second : &none;
        };

        if (auto* restoredTrack = engine.findTrack (id))
            restoredTrack->height = juce::jlimit (0, 1000, static_cast<int> (number (songTrack.getProperty ("height", {}), 0.0)));

        // A choir track's chords: the track they come from, named by its place.
        if (const auto choir = songTrack.getProperty ("choir", {}); choir.isObject())
        {
            const auto index = static_cast<int> (number (choir.getProperty ("chordTrack", {}), -1.0));

            if (const auto* settings = engine.getTrackChoir (id); settings != nullptr && index >= 0 && index < static_cast<int> (ids.size()))
            {
                auto withChords = *settings;
                withChords.chordTrack = ids[static_cast<size_t> (index)];
                engine.setTrackChoir (id, withChords);
            }
        }

        // Its Analog Board strip's dynamic EQ key: the strip it listens to, named by its place.
        if (songTrack.hasProperty ("boardKey"))
            if (const auto index = static_cast<int> (number (songTrack.getProperty ("boardKey", {}), -1.0)); index >= 0 && index < static_cast<int> (ids.size()))
                if (auto* keyed = engine.findTrack (id); keyed != nullptr && ! keyed->isMixChannel() && ids[static_cast<size_t> (index)] != id)
                    keyed->board.settings.set (AnalogBoard::deqKey, static_cast<float> (ids[static_cast<size_t> (index)]));

        // The bus it plays into (named by its place, as it may come after it).
        if (songTrack.hasProperty ("bus"))
            if (const auto index = static_cast<int> (number (songTrack.getProperty ("bus", {}), -1.0)); index >= 0 && index < static_cast<int> (ids.size()))
                engine.setTrackBus (id, ids[static_cast<size_t> (index)]);

        // A song from before per-track MIDI keeps what a new track takes (the first MIDI device).
        if (const auto* midiItems = songTrack.getProperty ("midiInputs", {}).getArray())
        {
            juce::StringArray midiInputs;

            for (const auto& item : *midiItems)
            {
                const auto text = item.toString();

                if (text.startsWith ("track:"))
                {
                    const auto index = text.substring (6).getIntValue();

                    if (index >= 0 && index < static_cast<int> (ids.size()))
                        midiInputs.add ("track:" + juce::String (ids[static_cast<size_t> (index)]));
                }
                else
                {
                    midiInputs.add (text);
                }
            }

            engine.setTrackMidiInputs (id, midiInputs);
        }

        const auto* sendItems = songTrack.getProperty ("sends", {}).getArray();

        if (sendItems == nullptr)
            return;

        std::vector<Track::Send> restored;

        for (const auto& sendItem : *sendItems)
        {
            Track::Send send;
            send.id = static_cast<int> (restored.size()) + 1;
            send.sidechain = static_cast<bool> (sendItem.getProperty ("sidechain", false));
            send.levelDb = static_cast<float> (number (sendItem.getProperty ("level", {}), 0.0));
            send.on = static_cast<bool> (sendItem.getProperty ("on", true));

            if (keepIds && sendItem.hasProperty ("targetId"))
            {
                send.target = static_cast<int> (number (sendItem.getProperty ("targetId", {}), 0.0));
                send.slot = static_cast<int> (number (sendItem.getProperty ("slot", {}), 0.0));
                restored.push_back (send);
                continue;
            }

            // A sidechain's insert: -1 names the instrument; a place, the insert restored there;
            // anything else (an insert that didn't load, none named) leaves the send out.
            const auto place = static_cast<int> (number (sendItem.getProperty ("insert", {}), -2.0));

            if (static_cast<bool> (sendItem.getProperty ("main", false)))
            {
                const auto insertId = insertAtPlace (engine.getMainInserts(), placesOf (AudioEngine::mainChannel), place);

                if (insertId <= 0)
                    continue;

                send.target = AudioEngine::mainChannel;
                send.slot = insertId;
            }
            else
            {
                const auto index = static_cast<int> (number (sendItem.getProperty ("track", {}), -1.0));

                if (index < 0 || index >= static_cast<int> (ids.size()))
                    continue;

                send.target = ids[static_cast<size_t> (index)];

                if (send.sidechain)
                {
                    const auto* targetTrack = engine.findTrack (send.target);
                    const auto insertId = targetTrack != nullptr ? insertAtPlace (targetTrack->inserts, placesOf (send.target), place) : 0;

                    if (place != -1 && insertId <= 0)
                        continue;

                    send.slot = place == -1 ? AudioEngine::instrumentSlot : insertId;
                }
            }

            restored.push_back (send);
        }

        engine.setSends (id, std::move (restored));
    }

    /** Song Blocks as songs keep them: each clip names its track by its place in the song. */
    juce::var blocksToVar (const SongBlocks& songBlocks, const std::map<int, int>& placeOf, const juce::File& songFile)
    {
        juce::Array<juce::var> blocks;

        for (const auto& block : songBlocks.blocks)
        {
            juce::Array<juce::var> parts;

            for (const auto& part : block.parts)
            {
                const auto place = placeOf.find (part.trackId);

                if (place == placeOf.end())
                    continue;   // its track has gone (an undo would bring it back, but not after saving)

                auto item = object();
                item.getDynamicObject()->setProperty ("track", place->second);
                item.getDynamicObject()->setProperty ("clip", clipToVar (part.clip, songFile));
                parts.add (item);
            }

            auto item = object();
            auto* fields = item.getDynamicObject();
            fields->setProperty ("id", block.id);
            fields->setProperty ("name", block.name);
            fields->setProperty ("colour", block.colour);
            fields->setProperty ("length", block.length);
            fields->setProperty ("bars", block.bars);
            fields->setProperty ("parts", parts);
            blocks.add (item);
        }

        juce::Array<juce::var> sequence;

        for (const auto id : songBlocks.sequence)
            sequence.add (id);

        auto result = object();
        result.getDynamicObject()->setProperty ("blocks", blocks);
        result.getDynamicObject()->setProperty ("sequence", sequence);
        result.getDynamicObject()->setProperty ("nextId", songBlocks.nextId);
        return result;
    }

    SongBlocks blocksFromVar (const juce::var& data, const std::vector<int>& trackIds, const juce::File& songFile)
    {
        SongBlocks songBlocks;

        if (const auto* blocks = data.getProperty ("blocks", {}).getArray())
        {
            for (const auto& item : *blocks)
            {
                SongBlock block;
                block.id = static_cast<int> (number (item.getProperty ("id", {}), 0.0));
                block.name = item.getProperty ("name", {}).toString().substring (0, 60);
                block.colour = item.getProperty ("colour", {}).toString();
                block.length = number (item.getProperty ("length", {}), 0.0);
                block.bars = juce::jmax (1, static_cast<int> (number (item.getProperty ("bars", {}), 1.0)));

                const auto hex = block.colour.substring (1);

                if (block.colour.length() != 7 || ! block.colour.startsWithChar ('#') || ! hex.containsOnly ("0123456789abcdefABCDEF"))
                    block.colour = SongBlocks::colourFor (static_cast<int> (songBlocks.blocks.size()));

                if (block.id <= 0 || songBlocks.find (block.id) != nullptr || ! (block.length > 0.0))
                    continue;

                if (const auto* parts = item.getProperty ("parts", {}).getArray())
                {
                    for (const auto& value : *parts)
                    {
                        const auto place = static_cast<int> (number (value.getProperty ("track", {}), -1.0));

                        if (place < 0 || place >= static_cast<int> (trackIds.size()) || trackIds[static_cast<size_t> (place)] == 0)
                            continue;

                        block.parts.push_back ({ trackIds[static_cast<size_t> (place)], clipFromVar (value.getProperty ("clip", {}), songFile) });
                    }
                }

                songBlocks.nextId = juce::jmax (songBlocks.nextId, block.id + 1);
                songBlocks.blocks.push_back (std::move (block));
            }
        }

        if (const auto* sequence = data.getProperty ("sequence", {}).getArray())
            for (const auto& value : *sequence)
                if (const auto id = static_cast<int> (number (value, 0.0)); songBlocks.find (id) != nullptr)
                    songBlocks.sequence.push_back (id);

        songBlocks.nextId = juce::jmax (songBlocks.nextId, static_cast<int> (number (data.getProperty ("nextId", {}), 1.0)));
        return songBlocks;
    }
}

//==============================================================================
juce::File Song::getDefaultFolder()
{
    return UserData::songs();   // Settings > Locations > User Data (Documents\Sonify by default)
}

juce::var Song::capture (AudioEngine& engine, const juce::String& name, const juce::var& view, const juce::File& songFile, bool withIds)
{
    engine.captureClipEffects();   // each clip effect's state as its plugin has it now

    auto song = object();
    auto* fields = song.getDynamicObject();
    fields->setProperty ("format", formatName);
    fields->setProperty ("version", formatVersion);
    fields->setProperty ("name", name);

    // An undo's snapshot: the ids of its tracks, inserts and clips too, which restore gives back.
    if (withIds)
        fields->setProperty ("ids", true);

    // The song channel's map, and (for Sonify from before it) the song's first tempo and signature.
    const auto state = engine.getTransport().getState();
    const auto& map = engine.getTempoMap();
    auto transport = object();
    auto* transportFields = transport.getDynamicObject();
    transportFields->setProperty ("bpm", map.getTempos().front().bpm);
    transportFields->setProperty ("numerator", map.getSignatures().front().numerator);
    transportFields->setProperty ("denominator", map.getSignatures().front().denominator);
    transportFields->setProperty ("tempoMap", map.toVar());
    transportFields->setProperty ("loop", state.loopEnabled);
    transportFields->setProperty ("loopStart", state.loopStart);
    transportFields->setProperty ("loopEnd", state.loopEnd);
    transportFields->setProperty ("metronome", state.metronomeEnabled);
    transportFields->setProperty ("metronomeVolume", static_cast<double> (state.metronomeVolume));
    transportFields->setProperty ("recordOptions", Transport::recordOptionsToVar (engine.getTransport().getRecordOptions()));
    transportFields->setProperty ("position", state.ppq);
    fields->setProperty ("transport", transport);

    fields->setProperty ("inputs", inputsToVar (engine.getInputs()));
    fields->setProperty ("outputs", outputsToVar (engine.getOutputs()));
    fields->setProperty ("nextOutputId", engine.getNextOutputId());
    fields->setProperty ("audition", engine.getAuditionOutput());
    fields->setProperty ("recordBits", engine.getRecordBits());

    auto main = object();
    auto* mainFields = main.getDynamicObject();
    mainFields->setProperty ("volume", static_cast<double> (engine.getMasterVolume()));
    mainFields->setProperty ("mute", engine.isMasterMuted());
    mainFields->setProperty ("inserts", insertsToVar (engine.getMainInserts(), withIds));

    if (const auto* kept = engine.getMainKept().getArray(); kept != nullptr && ! kept->isEmpty())
        mainFields->setProperty ("kept", keptToVar (engine.getMainKept(), songFile));   // a DAWproject's placeholders

    juce::Array<juce::var> mainPoints;   // the song channel's Main Volume lane

    for (const auto& point : engine.getMainAutomation())
        mainPoints.add (juce::Array<juce::var> { point.ppq, point.value });

    mainFields->setProperty ("automation", mainPoints);
    fields->setProperty ("main", main);

    juce::Array<juce::var> tracks;
    std::map<int, int> placeOf;   // track id -> its place, for what names tracks (Song Blocks)
    int selectedIndex = 0;

    for (const auto& track : engine.getTracks())
    {
        if (track->id == engine.getSelectedTrackId())
            selectedIndex = tracks.size();

        placeOf[track->id] = tracks.size();
        tracks.add (captureTrack (engine, *track, songFile, withIds));
    }

    fields->setProperty ("tracks", tracks);
    fields->setProperty ("selectedTrack", selectedIndex);

    // The Sonify devices the MIDI FX play, kept in the song (each build once) so it plays on any
    // computer, with or without the devices' files.
    juce::Array<juce::var> deviceBuilds;
    juce::StringArray deviceHashes;

    for (const auto& track : tracks)
    {
        if (const auto* devices = track.getProperty ("midiFx", {}).getArray())
        {
            for (const auto& device : *devices)
            {
                // Each device its own copy (two devices can have the same WebAssembly).
                const auto description = device.getProperty ("device", {});
                const auto hash = description.getProperty ("hash", {}).toString();
                const auto deviceId = description.getProperty ("deviceId", {}).toString();
                const auto key = deviceId + "/" + hash;

                if (device.getProperty ("type", {}).toString() != "device" || hash.isEmpty() || deviceHashes.contains (key))
                    continue;

                if (const auto copy = Devices::songCopyOf (hash, deviceId); copy.isObject())
                {
                    deviceHashes.add (key);
                    deviceBuilds.add (copy);
                }
            }
        }
    }

    if (! deviceBuilds.isEmpty())
        fields->setProperty ("devices", deviceBuilds);

    if (! engine.getSongBlocks().blocks.empty())
        fields->setProperty ("songBlocks", blocksToVar (engine.getSongBlocks(), placeOf, songFile));

    // The Analog Board: whether it mixes the song, and its master section (the strips are the tracks').
    {
        auto& board = engine.getBoard();
        const auto master = board.settings.toVar();

        if (engine.isBoardMixing() || master.getDynamicObject()->getProperties().size() > 0)
        {
            auto item = object();
            item.getDynamicObject()->setProperty ("mixing", engine.isBoardMixing());
            item.getDynamicObject()->setProperty ("master", master);
            fields->setProperty ("board", item);
        }
    }

    // The Mastering section on Main: its settings away from where a new song's start.
    if (const auto mastering = engine.getMastering().settings.toVar(); mastering.getDynamicObject()->getProperties().size() > 0)
        fields->setProperty ("mastering", mastering);

    // The Live Show: its master and its decks (their settings away from where they start, and the
    // tracks on them), its pads' sounds and its headphones' output. Whether it plays isn't kept: a
    // song opens in the Studio.
    {
        auto& show = engine.getLiveShow();
        const auto master = show.settings.toVar();
        bool any = master.getDynamicObject()->getProperties().size() > 0;
        juce::Array<juce::var> decks;

        for (int d = 0; d < LiveShow::maxDecks; ++d)
        {
            auto values = show.decks[static_cast<size_t> (d)].settings.toVar();

            // Where a deck sits on the crossfader at first isn't news.
            if (const auto assign = values.getProperty ("assign", {}); ! assign.isVoid()
                                                                     && juce::roundToInt (static_cast<double> (assign)) == LiveShow::defaultAssign (d))
                values.getDynamicObject()->removeProperty ("assign");

            auto deck = object();
            deck.getDynamicObject()->setProperty ("values", values);

            if (const auto loaded = engine.getDeckTrack (d); loaded != nullptr)
                deck.getDynamicObject()->setProperty ("file", mediaPath (loaded->file.getFullPathName(), songFile));

            any = any || values.getDynamicObject()->getProperties().size() > 0 || deck.hasProperty ("file");
            decks.add (deck);
        }

        // The pads' sounds, by their files ("" for a pad without one).
        juce::Array<juce::var> pads;
        bool anyPad = false;

        for (int p = 0; p < LiveShow::numPads; ++p)
        {
            const auto sound = engine.getPadSound (p);
            pads.add (sound != nullptr ? mediaPath (sound->file.getFullPathName(), songFile) : juce::String());
            anyPad = anyPad || sound != nullptr;
        }

        const auto phones = engine.getLivePhonesOutput();
        const auto& extras = engine.getLiveShowExtras();   // its tab and its playlist

        if (any || anyPad || phones != 0 || extras.isObject())
        {
            auto item = object();
            item.getDynamicObject()->setProperty ("master", master);
            item.getDynamicObject()->setProperty ("decks", decks);

            if (extras.isObject())
                item.getDynamicObject()->setProperty ("show", juce::JSON::parse (juce::JSON::toString (extras)));

            if (anyPad)
                item.getDynamicObject()->setProperty ("pads", pads);

            // The headphones' Sub output, by its id.
            if (phones != 0)
                item.getDynamicObject()->setProperty ("phones", phones);

            fields->setProperty ("live", item);
        }
    }

    // The video: its file (beside the song when it's in its folder) and where its picture starts
    // (a song position, quarter notes).
    if (const auto& video = engine.getSongVideo(); ! video.isEmpty())
    {
        auto item = object();
        item.getDynamicObject()->setProperty ("file", mediaPath (video.file.getFullPathName(), songFile));
        item.getDynamicObject()->setProperty ("start", video.start);
        fields->setProperty ("video", item);
    }

    fields->setProperty ("view", view);
    return song;
}

juce::Result Song::restore (AudioEngine& engine, const juce::var& song, juce::StringArray& problems, const juce::File& songFile)
{
    // Checked before anything is cleared, so a bad file leaves the current song alone.
    if (song.getProperty ("format", {}).toString() != formatName)
        return juce::Result::fail ("That file isn't a Sonify song.");

    if (static_cast<int> (song.getProperty ("version", 0)) > formatVersion)
        return juce::Result::fail ("That song was saved by a newer version of Sonify.");

    // An undo's snapshot puts tracks, inserts (Main's too) and clips back with the ids they had,
    // so the undo steps before it, which name them by id, still find them. It leaves the Live
    // Show as it is (an AI request never changes it): a deck playing goes on.
    const auto keepIds = static_cast<bool> (song.getProperty ("ids", false));

    engine.clearSong (keepIds);

    // The song's audio files read together, on several threads, before its clips ask for them
    // one by one.
    {
        juce::StringArray audioFiles;

        if (const auto* songTracks = song.getProperty ("tracks", {}).getArray())
            for (const auto& item : *songTracks)
                if (const auto* clips = item.getProperty ("clips", {}).getArray())
                    for (const auto& clip : *clips)
                        if (const auto path = clip.getProperty ("audioFile", {}).toString(); path.isNotEmpty())
                            audioFiles.addIfNotAlreadyThere (mediaFile (path, songFile).getFullPathName());

        engine.getMedia().prefetch (audioFiles, engine.getPlaybackRate());
    }

    const auto transportData = song.getProperty ("transport", {});
    auto& transport = engine.getTransport();
    const auto defaults = transport.getState();

    if (transportData.hasProperty ("tempoMap"))
    {
        engine.setTempoMap (TempoMap::fromVar (transportData.getProperty ("tempoMap", {})));
    }
    else
    {
        // A song from before the song channel: one tempo and one signature.
        TempoMap map;
        map.setTempos ({ { 0.0, number (transportData.getProperty ("bpm", {}), 120.0), false } });
        const auto denominator = static_cast<int> (number (transportData.getProperty ("denominator", {}), 4.0));
        map.setSignatures ({ { 0, juce::jlimit (1, 32, static_cast<int> (number (transportData.getProperty ("numerator", {}), 4.0))),
                               TempoMap::isValidDenominator (denominator) ? denominator : 4 } });
        engine.setTempoMap (map);
    }

    transport.setLoop (static_cast<bool> (transportData.getProperty ("loop", false)),
                       number (transportData.getProperty ("loopStart", {}), 0.0),
                       number (transportData.getProperty ("loopEnd", {}), 16.0));
    transport.setMetronome (static_cast<bool> (transportData.getProperty ("metronome", false)),
                            static_cast<float> (number (transportData.getProperty ("metronomeVolume", {}), defaults.metronomeVolume)));
    transport.setRecordOptions (Transport::recordOptionsFromVar (transportData.getProperty ("recordOptions", {}), {}));
    transport.locate (number (transportData.getProperty ("position", {}), 0.0));

    // A song from before it had outputs keeps the ones new songs start with.
    if (song.getProperty ("inputs", {}).isArray())
        engine.setInputs (inputsFromVar (song.getProperty ("inputs", {})));

    if (song.getProperty ("outputs", {}).isArray())
    {
        engine.setNextOutputId (static_cast<int> (number (song.getProperty ("nextOutputId", {}), 1.0)));
        engine.setOutputs (outputsFromVar (song.getProperty ("outputs", {})),
                           static_cast<int> (number (song.getProperty ("audition", {}), 0.0)));
    }

    engine.setRecordBits (static_cast<int> (number (song.getProperty ("recordBits", {}), 24.0)));

    const auto mainData = song.getProperty ("main", {});
    engine.setMasterVolume (static_cast<float> (number (mainData.getProperty ("volume", {}), 0.0)));
    engine.setMasterMute (static_cast<bool> (mainData.getProperty ("mute", false)));
    engine.setMainKept (keptFromVar (mainData.getProperty ("kept", {}), songFile));

    std::vector<AutomationPoint> mainPoints;

    if (const auto* points = mainData.getProperty ("automation", {}).getArray())
        for (const auto& point : *points)
            if (const auto* pair = point.getArray(); pair != nullptr && pair->size() == 2)
                mainPoints.push_back ({ number ((*pair)[0], -1.0), number ((*pair)[1], 0.0) });

    engine.setMainAutomation (std::move (mainPoints));

    // Each channel's inserts by their saved places (what names them goes by those): Main's, then each track's.
    std::map<int, std::vector<int>> insertIds;
    insertIds[AudioEngine::mainChannel] = restoreInserts (engine, AudioEngine::mainChannel, mainData.getProperty ("inserts", {}), problems, keepIds);

    // The Sonify devices the song keeps, ready before its MIDI FX ask for them. One that can't
    // play here stays in the song as it was (its MIDI FX let the notes through).
    if (const auto* builds = song.getProperty ("devices", {}).getArray())
    {
        for (const auto& build : *builds)
        {
            juce::String error;

            if (Devices::buildFromVar (build, error) == nullptr)
            {
                Devices::keepUnplayable (build);
                problems.add ("The device \"" + build.getProperty ("name", "Device").toString() + "\" can't play here: " + error);
            }
        }
    }

    std::vector<int> ids;
    std::vector<bool> monitors;

    if (const auto* tracks = song.getProperty ("tracks", {}).getArray())
    {
        for (const auto& item : *tracks)
        {
            bool monitor = false;
            std::vector<int> placed;
            ids.push_back (restoreTrack (engine, item, problems, songFile, monitor, keepIds, &placed));
            insertIds[ids.back()] = std::move (placed);
            monitors.push_back (monitor);
        }
    }

    // Sends and MIDI inputs, once every track is there (either can name a track further down).
    if (const auto* songTracks = song.getProperty ("tracks", {}).getArray())
    {
        for (int t = 0; t < songTracks->size() && t < static_cast<int> (ids.size()); ++t)
            restoreTrackLinks (engine, ids[static_cast<size_t> (t)], songTracks->getReference (t), ids, &insertIds, keepIds);
    }

    if (! ids.empty())
    {
        const auto selected = static_cast<size_t> (juce::jlimit (0, static_cast<int> (ids.size()) - 1,
                                                                 static_cast<int> (song.getProperty ("selectedTrack", 0))));
        engine.selectTrack (ids[selected]);

        // Each track's own monitoring, as the song saved it.
        for (size_t i = 0; i < ids.size(); ++i)
            engine.setTrackMonitor (ids[i], monitors[i]);
    }

    if (song.hasProperty ("songBlocks"))
        engine.setSongBlocks (blocksFromVar (song.getProperty ("songBlocks", {}), ids, songFile));

    if (const auto board = song.getProperty ("board", {}); board.isObject())
    {
        engine.getBoard().settings.fromVar (board.getProperty ("master", {}), true);
        engine.setBoardMixing (static_cast<bool> (board.getProperty ("mixing", false)));
    }

    // The Mastering section (clearSong put it where a new song's starts).
    if (const auto mastering = song.getProperty ("mastering", {}); mastering.isObject())
        engine.getMastering().settings.fromVar (mastering, true);

    // The Live Show's settings, the tracks its decks had and its pads' sounds (loaded once the song
    // is in: MainLive.cpp). clearSong has started it afresh, so a song without them leaves none of
    // the last song's. An undo's snapshot left the show as it is.
    if (const auto live = song.getProperty ("live", {}); live.isObject() && ! keepIds)
    {
        auto& show = engine.getLiveShow();
        show.settings.fromVar (live.getProperty ("master", {}), false);

        // A song saved before the dynamic EQ's bells had a level of their own and a Duck: a bell's
        // Depth (dB) was as far as it went down, from as it is.
        if (const auto master = live.getProperty ("master", {}); master.isObject())
        {
            for (int bell = 0; bell < 2; ++bell)
            {
                const auto depth = "b" + juce::String (bell + 1) + "Depth";

                if (master.hasProperty (depth) && ! master.hasProperty ("b" + juce::String (bell + 1) + "Duck"))
                    show.settings.set (LiveShow::Ducker::bellParam (bell, LiveShow::bell1Duck),
                                       0.5f * std::pow (10.0f, -juce::jlimit (0.0f, 24.0f, static_cast<float> (master.getProperty (depth, 0.0))) / 40.0f));
            }
        }

        std::array<juce::File, LiveShow::maxDecks> files;

        if (const auto* decks = live.getProperty ("decks", {}).getArray())
        {
            for (int d = 0; d < juce::jmin (decks->size(), LiveShow::maxDecks); ++d)
            {
                const auto& deck = decks->getReference (d);
                show.decks[static_cast<size_t> (d)].settings.fromVar (deck.getProperty ("values", {}), false);

                if (const auto path = deck.getProperty ("file", {}).toString(); path.isNotEmpty())
                    files[static_cast<size_t> (d)] = mediaFile (path, songFile);
            }
        }

        engine.setDeckFilesToLoad (files);

        std::array<juce::File, LiveShow::numPads> padFiles;

        if (const auto* pads = live.getProperty ("pads", {}).getArray())
            for (int p = 0; p < juce::jmin (pads->size(), LiveShow::numPads); ++p)
                if (const auto path = pads->getReference (p).toString(); path.isNotEmpty())
                    padFiles[static_cast<size_t> (p)] = mediaFile (path, songFile);

        engine.setPadFilesToLoad (padFiles);
        engine.setLivePhonesOutput (static_cast<int> (live.getProperty ("phones", 0)));
        engine.setLiveShowExtras (live.getProperty ("show", {}));
    }

    if (const auto video = song.getProperty ("video", {}); video.isObject())
    {
        const auto path = video.getProperty ("file", {}).toString();

        if (path.isNotEmpty())
            engine.setSongVideo ({ mediaFile (path, songFile), juce::jmax (0.0, number (video.getProperty ("start", {}), 0.0)) });
    }

    // Frozen tracks last, once everything they play (and what they need, the mixer) is in.
    if (const auto* songTracks = song.getProperty ("tracks", {}).getArray())
    {
        for (int t = 0; t < songTracks->size() && t < static_cast<int> (ids.size()); ++t)
        {
            const auto& item = songTracks->getReference (t);
            const auto frozen = item.getProperty ("frozen", {}).toString();
            const auto dry = static_cast<bool> (item.getProperty ("frozenDry", false));

            // (Made for the other mixer: it plays itself.)
            if (frozen.isNotEmpty() && dry == engine.isBoardMixing() && ! engine.freezeTrack (ids[static_cast<size_t> (t)], mediaFile (frozen, songFile), dry))
                problems.add ("\"" + item.getProperty ("name", "A track").toString() + "\" was frozen, but its frozen sound is missing: it plays itself again.");
        }
    }

    return juce::Result::ok();
}

int Song::duplicateTrack (AudioEngine& engine, int trackId, juce::StringArray& problems)
{
    const auto& tracks = engine.getTracks();
    const auto found = std::find_if (tracks.begin(), tracks.end(), [trackId] (const auto& t) { return t->id == trackId; });

    if (found == tracks.end())
        return 0;

    // As a song would keep it (audio files by their full paths), then made again at the end:
    // the tracks it names are named by their place, which the copy maps back to the same tracks.
    const auto item = captureTrack (engine, **found, {});
    std::vector<int> ids;

    for (const auto& track : tracks)
        ids.push_back (track->id);

    bool monitor = false;
    const auto id = restoreTrack (engine, item, problems, {}, monitor);
    restoreTrackLinks (engine, id, item, ids);
    engine.setTrackMonitor (id, monitor);
    return id;
}

//==============================================================================
juce::var Song::captureTrackPreset (AudioEngine& engine, const std::vector<int>& trackIds)
{
    juce::Array<juce::var> list;

    for (const auto id : trackIds)
    {
        const auto* track = engine.findTrack (id);

        if (track == nullptr)
            continue;

        auto item = captureTrack (engine, *track, {});
        auto* fields = item.getDynamicObject();

        for (const auto* key : { "clips", "automation", "autoMode", "sends", "bus", "boardKey", "output", "input", "height", "frozen", "frozenDry",
                                 "mute", "solo", "monitor", "monitorDecided", "recordArm", "live", "kept" })
            fields->removeProperty (key);

        // MIDI from devices (by name) comes along; from other tracks of this song, not.
        juce::Array<juce::var> devices;

        if (const auto* midiInputs = item.getProperty ("midiInputs", {}).getArray())
            for (const auto& input : *midiInputs)
                if (! input.toString().startsWith ("track:"))
                    devices.add (input);

        fields->setProperty ("midiInputs", devices);
        list.add (item);
    }

    auto preset = object();
    preset.getDynamicObject()->setProperty ("format", trackPresetFormat);
    preset.getDynamicObject()->setProperty ("version", 1);
    preset.getDynamicObject()->setProperty ("tracks", list);
    return preset;
}

std::vector<int> Song::restoreTrackPreset (AudioEngine& engine, const juce::var& preset, juce::StringArray& problems)
{
    std::vector<int> ids;

    if (preset.getProperty ("format", {}).toString() != trackPresetFormat)
        return ids;

    if (const auto* items = preset.getProperty ("tracks", {}).getArray())
    {
        for (const auto& item : *items)
        {
            if (! item.isObject())
                continue;

            bool monitor = false;
            const auto id = restoreTrack (engine, item, problems, {}, monitor);
            restoreTrackLinks (engine, id, item, {});
            ids.push_back (id);
        }
    }

    return ids;
}

//==============================================================================
juce::var Song::inputsToVar (const std::vector<AudioEngine::SongInput>& inputs)
{
    juce::Array<juce::var> list;

    for (const auto& input : inputs)
    {
        auto item = object();
        auto* fields = item.getDynamicObject();
        fields->setProperty ("name", input.name);
        fields->setProperty ("left", input.left);
        fields->setProperty ("right", input.right);

        // A mono input's other channels (all of them heard together).
        if (! input.also.empty())
        {
            juce::Array<juce::var> also;

            for (const auto channel : input.also)
                also.add (channel);

            fields->setProperty ("also", also);
        }

        list.add (item);
    }

    return list;
}

std::vector<AudioEngine::SongInput> Song::inputsFromVar (const juce::var& list)
{
    std::vector<AudioEngine::SongInput> inputs;

    if (const auto* items = list.getArray())
    {
        for (const auto& item : *items)
        {
            AudioEngine::SongInput input { item.getProperty ("name", "Input").toString(),
                                           static_cast<int> (number (item.getProperty ("left", {}), 0.0)),
                                           static_cast<int> (number (item.getProperty ("right", {}), -1.0)), {} };

            if (const auto* also = item.getProperty ("also", {}).getArray())
                for (const auto& channel : *also)
                    input.also.push_back (static_cast<int> (number (channel, 0.0)));

            inputs.push_back (std::move (input));
        }
    }

    return inputs;
}

juce::var Song::outputsToVar (const std::vector<AudioEngine::SongOutput>& outputs)
{
    juce::Array<juce::var> list;

    for (const auto& output : outputs)
    {
        auto item = object();
        auto* fields = item.getDynamicObject();
        fields->setProperty ("id", output.id);
        fields->setProperty ("name", output.name);
        fields->setProperty ("left", output.left);
        fields->setProperty ("right", output.right);
        fields->setProperty ("mainMix", output.mainMix);
        fields->setProperty ("volume", static_cast<double> (output.volumeDb));
        fields->setProperty ("mute", output.mute);
        list.add (item);
    }

    return list;
}

std::vector<AudioEngine::SongOutput> Song::outputsFromVar (const juce::var& list)
{
    std::vector<AudioEngine::SongOutput> outputs;

    if (const auto* items = list.getArray())
    {
        for (const auto& item : *items)
        {
            AudioEngine::SongOutput output;
            output.id = static_cast<int> (number (item.getProperty ("id", {}), -1.0));
            output.name = item.getProperty ("name", {}).toString();
            output.left = static_cast<int> (number (item.getProperty ("left", {}), 0.0));
            output.right = static_cast<int> (number (item.getProperty ("right", {}), -1.0));
            output.mainMix = static_cast<bool> (item.getProperty ("mainMix", false));
            output.volumeDb = static_cast<float> (number (item.getProperty ("volume", {}), 0.0));
            output.mute = static_cast<bool> (item.getProperty ("mute", false));
            outputs.push_back (output);
        }
    }

    return outputs;
}

//==============================================================================
std::vector<std::pair<double, double>> Song::warpFromVar (const juce::var& points)   { return readWarp (points); }
MidiClip::Silence Song::silenceFromVar (const juce::var& value)                      { return readSilence (value); }

juce::var Song::envelopesToVar (const std::vector<ClipEnvelope>& envelopes)
{
    juce::Array<juce::var> list;

    for (const auto& line : envelopes)
    {
        juce::Array<juce::var> points;

        for (const auto& point : line.points)
            points.add (juce::Array<juce::var> { point.time, point.value });

        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("key", line.key);
        item->setProperty ("points", points);
        list.add (juce::var (item.get()));
    }

    return list;
}

std::vector<ClipEnvelope> Song::envelopesFromVar (const juce::var& list, bool audio)
{
    std::vector<ClipEnvelope> envelopes;

    if (const auto* items = list.getArray())
    {
        for (const auto& item : *items)
        {
            ClipEnvelope line;
            line.key = item.getProperty ("key", {}).toString();

            if (const auto* points = item.getProperty ("points", {}).getArray())
                for (const auto& point : *points)
                    if (const auto* pair = point.getArray(); pair != nullptr && pair->size() >= 2)
                        line.points.push_back ({ number ((*pair)[0], std::numeric_limits<double>::quiet_NaN()),
                                                 number ((*pair)[1], std::numeric_limits<double>::quiet_NaN()) });

            envelopes.push_back (std::move (line));
        }
    }

    ClipEnvelope::tidy (envelopes, audio);
    return envelopes;
}

juce::var Song::warpToVar (const std::vector<std::pair<double, double>>& warp)
{
    juce::Array<juce::var> points;

    for (const auto& [out, in] : warp)
        points.add (juce::Array<juce::var> { out, in });

    return points;
}
