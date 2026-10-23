// D:\workspace\Fanan_web_hub\src\MainAudio.cpp
//
// MainComponent: the Audio menu (the audio editor's, and an audio clip's right-click menu).
//
//   Detect Tempo      listens for the clip's own tempo (what Sync Tempo starts from)
//   Sync Tempo        a stretched copy whose beats land on the song's, from the clip's start
//   Follow Tempo      the same, made again whenever the song's tempo changes under the clip
//   Pitch             moved by semitones (and hundredths), its length kept
//   Reverse           backwards (again: forwards)
//   Back to the Original   the original itself, as recorded or imported
//   Normalize, Gain   the clip's own volume: its loudest moment at 0 dB, or a value typed
//   Audio to MIDI     what it plays, as notes (or drum hits) in a MIDI clip on a new track under it
//   Slice to Sampler  the clip cut where the Audio Editor shows (its cuts set there: at its hits, its
//                     warp markers or the grid, then corrected by hand), each slice a key of a
//                     Sonify Sampler on a new track under it, from C2 (note 36) up, and a MIDI clip that plays
//                     them as the clip did
//
// The renders come from the clip's original, which stays as it is; each change is one undo step.

#include "MainComponent.h"
#include "BuiltinPlugins.h"
#include "SonifySampler.h"

#include <algorithm>
#include <limits>
#include <cmath>

namespace
{
    juce::String historyName (const juce::String& op)
    {
        if (op == "syncTempo")   return "Sync Tempo";
        if (op == "followTempo") return "Follow Tempo";
        if (op == "pitch")       return "Pitch";
        if (op == "reverse")     return "Reverse";
        if (op == "original")    return "Back to the Original";
        if (op == "normalize")   return "Normalize";
        if (op == "gain")        return "Clip Volume";
        if (op == "toMidi")      return "Audio to MIDI";
        if (op == "slice")       return "Slice to Sampler";
        if (op == "stretch")     return "Stretch";
        if (op == "warpMove")    return "Move Warp Marker";
        if (op == "warpRemove")  return "Remove Warp Marker";
        return "Edit Audio";
    }

    bool isRenderOp (const juce::String& op)
    {
        return op == "syncTempo" || op == "followTempo" || op == "pitch" || op == "reverse" || op == "original" || op == "refresh"
            || op == "stretch" || op == "warpMove" || op == "warpRemove";
    }

    using TimeMap = std::vector<std::pair<double, double>>;

    /** Where a time map takes a time (straight lines between its points; past its ends, held). */
    double timeAt (const TimeMap& map, double t)
    {
        if (map.empty())
            return t;

        if (t <= map.front().first)
            return map.front().second;

        for (size_t i = 1; i < map.size(); ++i)
        {
            const auto& a = map[i - 1];
            const auto& b = map[i];

            if (t <= b.first)
                return b.first - a.first > 1.0e-12 ? a.second + (b.second - a.second) * (t - a.first) / (b.first - a.first) : b.second;
        }

        return map.back().second;
    }

    /** Points in order of their first time, no two at the same time (nor closer than a sample at 192 kHz). */
    TimeMap tidyMap (TimeMap points)
    {
        std::sort (points.begin(), points.end());
        TimeMap tidy;

        for (const auto& point : points)
            if (tidy.empty() || point.first - tidy.back().first > 1.0e-6)
                tidy.push_back ({ point.first, juce::jmax (point.second, tidy.empty() ? 0.0 : tidy.back().second) });

        return tidy;
    }

    /** The Stretcher's change to a map of what a clip shows: from .. to made newLength long, what's
        after it moved along. */
    TimeMap stretchMap (const TimeMap& map, double from, double to, double newLength)
    {
        const auto span = to - from, delta = newLength - span;
        TimeMap points;

        for (const auto& [t, w] : map)
        {
            const auto moved = t <= from ? t : t >= to ? t + delta : from + (t - from) * newLength / span;
            points.push_back ({ moved, w });
        }

        points.push_back ({ from, timeAt (map, from) });
        points.push_back ({ from + newLength, timeAt (map, to) });
        return tidyMap (std::move (points));
    }

    /** A warp marker moved: the moment of what the clip shows at `at` goes to newAt; what's
        between the markers either side (prev, next) squeezes or stretches to make room, and
        nothing outside them moves. */
    TimeMap warpMoveMap (const TimeMap& map, double prev, double at, double next, double newAt)
    {
        TimeMap points;

        for (const auto& [t, w] : map)
        {
            const auto moved = t <= prev || t >= next ? t
                             : t <= at ? prev + (t - prev) * (newAt - prev) / (at - prev)
                                       : newAt + (t - at) * (next - newAt) / (next - at);
            points.push_back ({ moved, w });
        }

        points.push_back ({ prev,  timeAt (map, prev) });
        points.push_back ({ newAt, timeAt (map, at) });
        points.push_back ({ next,  timeAt (map, next) });
        return tidyMap (std::move (points));
    }

    /** A warp marker taken away: the map without its point at `at` (the ends stay). False: none there. */
    bool warpRemoveMap (const TimeMap& map, double at, TimeMap& result)
    {
        result.clear();
        bool found = false;

        for (size_t i = 0; i < map.size(); ++i)
        {
            const auto inside = i > 0 && i + 1 < map.size();

            if (inside && ! found && std::abs (map[i].first - at) < 0.002)
                found = true;
            else
                result.push_back (map[i]);
        }

        return found;
    }

    /** The same map, turned round (the render read backwards). */
    TimeMap mirrored (const TimeMap& map)
    {
        if (map.size() < 2)
            return map;

        const auto t = map.back().first, w = map.back().second;
        TimeMap points;

        for (auto it = map.rbegin(); it != map.rend(); ++it)
            points.push_back ({ t - it->first, w - it->second });

        return tidyMap (std::move (points));
    }
}

//==============================================================================
const MidiClip* MainComponent::findAudioClip (int trackId, int clipId) const
{
    if (const auto* track = engine.findTrack (trackId); track != nullptr && track->kind == Track::Kind::audio)
        for (const auto& clip : track->clips)
            if (clip.id == clipId && clip.isAudio())
                return &clip;

    return nullptr;
}

AudioRender::Window MainComponent::sourceWindowOf (const MidiClip& clip)
{
    const auto renderLength = clip.isRender() && juce::File (clip.audioFile).existsAsFile()
                                ? engine.getMedia().lengthOf (juce::File (clip.audioFile), engine.getPlaybackRate()) : 0.0;
    return AudioRender::originalWindowOf (clip, renderLength);
}

bool MainComponent::shownTimeMap (const MidiClip& clip, AudioRender::Window& window, std::vector<std::pair<double, double>>& map)
{
    map.clear();

    if (clip.followsTempo())
        return false;

    // Stretched before: the part of its time map it shows.
    if (clip.isRender() && clip.warp.size() >= 2)
    {
        const auto o = clip.audioOffset, l = clip.audioLength;
        const auto w0 = timeAt (clip.warp, o), w1 = timeAt (clip.warp, o + l);

        window.file = juce::File (clip.sourceFile);
        window.length = juce::jmax (0.0, w1 - w0);
        window.offset = clip.sourceOffset + (clip.reversed ? clip.sourceLength - w1 : w0);

        map.push_back ({ 0.0, 0.0 });

        for (const auto& [t, w] : clip.warp)
            if (t > o && t < o + l)
                map.push_back ({ t - o, w - w0 });

        map.push_back ({ l, w1 - w0 });
        map = tidyMap (std::move (map));
        return true;
    }

    // Synced to the song: its tempo's map; anything else plays its window as it is.
    window = sourceWindowOf (clip);

    if (clip.isRender() && clip.sourceTempo > 0.0)
        map = renderJobFor (clip, window, clip.reversed, clip.pitch, clip.sourceTempo, {}).timeMap;

    if (map.size() < 2)
        map = { { 0.0, 0.0 }, { window.length, window.length } };

    return true;
}

AudioRender::Job MainComponent::renderJobFor (const MidiClip& clip, const AudioRender::Window& window, bool reverse, double semitones, double tempo,
                                              const juce::String& name, double leadBeats)
{
    return AudioRender::jobFor (clip, window, reverse, semitones, tempo, engine.getTempoMap(),
                                engine.getRecordingFolder() != juce::File() ? engine.getRecordingFolder() : UserData::recordings(), name, leadBeats);
}

bool MainComponent::clipStillAsItWas (const AudioJob& job) const
{
    // Moved, trimmed, changed or undone while the job ran: its result is for a clip that isn't there.
    const auto* clip = findAudioClip (job.trackId, job.clipId);

    if (clip == nullptr)
        return false;

    const auto& was = job.clip;
    const auto same = [] (double a, double b) { return std::abs (a - b) < 1.0e-9; };

    return same (clip->start, was.start) && clip->audioFile == was.audioFile && same (clip->audioOffset, was.audioOffset)
        && same (clip->audioLength, was.audioLength) && clip->sourceFile == was.sourceFile && same (clip->sourceOffset, was.sourceOffset)
        && same (clip->sourceLength, was.sourceLength) && same (clip->sourceTempo, was.sourceTempo) && same (clip->pitch, was.pitch)
        && clip->reversed == was.reversed && clip->followTempo == was.followTempo && clip->warp == was.warp
        && (! clip->followsTempo() || (same (clip->offset, was.offset) && same (clip->length, was.length)));
}

void MainComponent::forgetUnusedAudio (const juce::String& file)
{
    // A render no clip plays any more needn't stay in memory: Undo reads it again.
    for (const auto& track : engine.getTracks())
        for (const auto& clip : track->clips)
            if (clip.audioFile == file)
                return;

    engine.getMedia().forget (juce::File (file));
}

void MainComponent::lookAtFollowersAgain()
{
    followStamp.clear();
}

bool MainComponent::changeAudioClip (const AudioJob& job, const std::function<void (MidiClip&)>& change, const juce::String& name)
{
    auto before = Edits::snapshot (engine, { job.trackId });
    auto clips = engine.getClips (job.trackId);
    juce::String previousFile;
    bool found = false;

    for (auto& clip : clips)
    {
        if (clip.id == job.clipId && clip.isAudio())
        {
            previousFile = clip.audioFile;
            const auto oldOffset = clip.audioOffset, oldLength = clip.audioLength;
            const auto wasReversed = clip.reversed;
            change (clip);

            // Its own automation follows the sound to the new file: the old window's times laid over
            // the new window's (back to front when it was turned round).
            if (! clip.envelopes.empty() && oldLength > 0.0 && clip.audioLength > 0.0)
            {
                const auto flip = wasReversed != clip.reversed;

                for (auto& line : clip.envelopes)
                {
                    for (auto& point : line.points)
                    {
                        auto share = (point.time - oldOffset) / oldLength;

                        if (flip)
                            share = 1.0 - share;

                        point.time = clip.audioOffset + share * clip.audioLength;
                    }
                }

                ClipEnvelope::tidy (clip.envelopes, true);
            }
            clip.fadeIn = juce::jlimit (0.0, clip.audioLength, clip.fadeIn);
            clip.fadeOut = juce::jlimit (0.0, clip.audioLength - clip.fadeIn, clip.fadeOut);
            found = true;
        }
    }

    if (! found)
        return false;

    engine.setClips (job.trackId, std::move (clips));

    if (const auto* now = findAudioClip (job.trackId, job.clipId); now != nullptr && now->audioFile != previousFile)
        forgetUnusedAudio (previousFile);

    // Bringing a follower up to date is no step of its own: undoing the tempo brings its
    // earlier render back by itself.
    if (job.automatic)
        markChanged();
    else
        addToHistory (name, new Edits::ClipsChanged (engine, std::move (before), Edits::snapshot (engine, { job.trackId })));

    pushTracks();
    return true;
}

//==============================================================================
void MainComponent::handleAudioOp (const juce::var& event)
{
    AudioJob job;
    job.op = event.getProperty ("op", {}).toString();
    job.trackId = static_cast<int> (event.getProperty ("trackId", 0));
    job.clipId = static_cast<int> (event.getProperty ("clipId", 0));
    job.tempo = static_cast<double> (event.getProperty ("tempo", 0.0));
    job.semitones = static_cast<double> (event.getProperty ("semitones", 0.0));
    job.follow = static_cast<bool> (event.getProperty ("on", false));
    job.mode = event.getProperty ("mode", "notes").toString() == "drums" ? AudioToMidi::Mode::drums : AudioToMidi::Mode::notes;
    job.from = static_cast<double> (event.getProperty ("from", 0.0));
    job.to = static_cast<double> (event.getProperty ("to", 0.0));
    job.newLength = static_cast<double> (event.getProperty ("length", 0.0));
    job.keepEnd = static_cast<bool> (event.getProperty ("keepEnd", false));
    job.at = static_cast<double> (event.getProperty ("at", 0.0));
    job.newAt = static_cast<double> (event.getProperty ("newAt", 0.0));

    if (! std::isfinite (job.from) || ! std::isfinite (job.to) || ! std::isfinite (job.newLength)
        || ! std::isfinite (job.at) || ! std::isfinite (job.newAt))
        return emitAudioJob (job, "failed", 0.0, "That isn't a number.");

    if (! std::isfinite (job.tempo) || ! std::isfinite (job.semitones))
        return emitAudioJob (job, "failed", 0.0, "That isn't a number.");

    // The clip's own volume: at once, nothing to render.
    if (job.op == "normalize" || job.op == "gain")
    {
        const auto* clip = findAudioClip (job.trackId, job.clipId);

        if (clip == nullptr)
            return emitAudioJob (job, "failed", 0.0, "Select an audio clip.");

        auto gain = static_cast<double> (event.getProperty ("db", clip->audioGainDb));

        if (job.op == "normalize")
        {
            juce::String error;
            const auto data = engine.getMedia().get (juce::File (clip->audioFile), engine.getPlaybackRate(), error);

            if (data == nullptr)
                return emitAudioJob (job, "failed", 0.0, "The clip's audio is missing.");

            const auto rate = data->playbackRate;
            const auto from = juce::jlimit (0, data->samples.getNumSamples(), static_cast<int> (std::llround (clip->audioOffset * rate)));
            const auto count = juce::jlimit (0, data->samples.getNumSamples() - from, static_cast<int> (std::llround (clip->audioLength * rate)));
            float peak = 0.0f;

            for (int c = 0; c < data->samples.getNumChannels(); ++c)
                peak = juce::jmax (peak, data->samples.getMagnitude (c, from, count));

            if (peak <= 1.0e-6f)
                return emitAudioJob (job, "failed", 0.0, "The clip is silent.");

            gain = -juce::Decibels::gainToDecibels (static_cast<double> (peak));
        }

        if (! std::isfinite (gain))
            return emitAudioJob (job, "failed", 0.0, "That isn't a number.");

        const auto wanted = gain;
        gain = juce::jlimit (-60.0, 12.0, std::round (gain * 10.0) / 10.0);

        changeAudioClip (job, [&] (MidiClip& c) { c.audioGainDb = gain; }, historyName (job.op));
        emitAudioJob (job, "done", 1.0, job.op == "normalize" && wanted > 12.0 ? juce::String ("It's very quiet: the clip's volume goes up to +12 dB at most.") : juce::String());
        return;
    }

    // Slice to Sampler: at once, nothing to render.
    if (job.op == "slice")
    {
        sliceToSampler (job, event.getProperty ("cuts", {}));
        return;
    }

    if (const auto problem = startAudioOp (job); problem.isNotEmpty())
        emitAudioJob (job, "failed", 0.0, problem);
}

//==============================================================================
// Slice to Sampler: each slice (between the cuts the page sends, seconds of what the clip shows)
// written as a WAV of its own, with an SFZ playing them from C2 (note 36) up, in a folder of its own in the
// Samples folder; a new instrument track under the clip's with a Sonify Sampler playing it, and a
// MIDI clip where the audio clip is that plays them as it did. The audio clip stays as it is.
void MainComponent::sliceToSampler (const AudioJob& job, const juce::var& cuts)
{
    constexpr int firstKey = 36;                        // C2 (middle C, 60, is C4)
    constexpr int mostSlices = 128 - firstKey;

    const auto* found = findAudioClip (job.trackId, job.clipId);
    const auto* source = engine.findTrack (job.trackId);

    if (found == nullptr || source == nullptr)
        return emitAudioJob (job, "failed", 0.0, "Select an audio clip.");

    if (engine.isRecording() || engine.isTakeActive())
        return emitAudioJob (job, "failed", 0.0, "Not while recording.");

    const auto clip = *found;
    juce::String error;
    const auto data = engine.getMedia().get (juce::File (clip.audioFile), engine.getPlaybackRate(), error);

    if (data == nullptr || ! (data->playbackRate > 0.0) || data->samples.getNumSamples() == 0)
        return emitAudioJob (job, "failed", 0.0, "The clip's audio is missing.");

    // Where it's cut (seconds of what the clip shows), in order: the clip's start, then each cut.
    std::vector<double> edges { 0.0 };

    if (const auto* list = cuts.getArray())
        for (const auto& value : *list)
            if (const auto s = static_cast<double> (value); std::isfinite (s) && s > 0.005 && s < clip.audioLength - 0.005)
                edges.push_back (s);

    std::sort (edges.begin(), edges.end());
    edges.erase (std::unique (edges.begin(), edges.end(), [] (double a, double b) { return b - a < 0.005; }), edges.end());

    if (edges.size() > static_cast<size_t> (mostSlices))
        return emitAudioJob (job, "failed", 0.0, "That's " + juce::String (static_cast<int> (edges.size())) + " slices: a sampler's keys from C2 up take "
                                                 + juce::String (mostSlices) + " at most. Take some cuts away.");

    edges.push_back (clip.audioLength);

    // Its folder: Samples\Slices\<track> <bar>.
    const auto& map = engine.getTempoMap();
    const auto name = source->name + " Slices";
    const auto folder = Samplers::folder().getChildFile ("Slices")
                                          .getNonexistentChildFile (juce::File::createLegalFileName (source->name + " bar " + juce::String (map.barAt (clip.start + 1.0e-9).index + 1)), "", false);

    if (! folder.createDirectory())
        return emitAudioJob (job, "failed", 0.0, "Couldn't make the folder " + folder.getFullPathName());

    const auto rate = data->playbackRate;
    const auto& samples = data->samples;
    const auto channels = juce::jlimit (1, 2, samples.getNumChannels());
    const auto total = samples.getNumSamples();
    const auto fadeIn = juce::jmax (1, static_cast<int> (rate * 0.001)), fadeOut = juce::jmax (1, static_cast<int> (rate * 0.003));

    juce::String sfz;
    sfz << "// Made by Sonify: Slice to Sampler, from " << juce::File (clip.audioFile).getFileName() << "\n"
        << "<global>\nampeg_attack=0\nampeg_release=0.02\n\n";

    MidiClip part;
    part.start = clip.start;
    part.length = clip.length;
    const auto startSeconds = map.secondsAt (clip.start);

    for (size_t i = 0; i + 1 < edges.size(); ++i)
    {
        const auto from = juce::jlimit (0, total, static_cast<int> (std::llround ((clip.audioOffset + edges[i]) * rate)));
        const auto to = juce::jlimit (from, total, static_cast<int> (std::llround ((clip.audioOffset + edges[i + 1]) * rate)));
        const auto count = to - from;

        if (count <= 0)
            continue;

        // The slice, a moment's fade at each end (no click where it's cut).
        juce::AudioBuffer<float> slice (2, count);

        for (int c = 0; c < 2; ++c)
            slice.copyFrom (c, 0, samples, juce::jmin (c, channels - 1), from, count);

        slice.applyGainRamp (0, juce::jmin (fadeIn, count), 0.0f, 1.0f);
        slice.applyGainRamp (count - juce::jmin (fadeOut, count), juce::jmin (fadeOut, count), 1.0f, 0.0f);

        const auto key = firstKey + static_cast<int> (i);
        const auto fileName = "Slice " + juce::String (i + 1).paddedLeft ('0', 2) + ".wav";
        const auto file = folder.getChildFile (fileName);
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> writer;

        if (stream != nullptr)
            writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                                         .withSampleRate (rate)
                                                                         .withNumChannels (2)
                                                                         .withBitsPerSample (24));

        if (writer == nullptr || ! writer->writeFromAudioSampleBuffer (slice, 0, count))
            return emitAudioJob (job, "failed", 0.0, "Couldn't write " + file.getFullPathName());

        writer.reset();

        sfz << "<region>\nsample=" << fileName << "\nkey=" << key << "\n";

        if (std::abs (clip.audioGainDb) > 0.01)
            sfz << "volume=" << juce::String (clip.audioGainDb, 1) << "\n";

        sfz << "\n";

        // Its note, where the slice plays in the song.
        MidiNote note;
        note.pitch = key;
        note.start = juce::jmax (0.0, map.ppqAt (startSeconds + edges[i]) - clip.start);
        note.length = juce::jmax (1.0 / 64.0, map.ppqAt (startSeconds + edges[i + 1]) - clip.start - note.start);
        note.velocity = 1.0f;

        if (note.start < clip.length)
            part.notes.push_back (note);
    }

    const auto sfzFile = folder.getChildFile (juce::File::createLegalFileName (name) + ".sfz");

    if (part.notes.empty() || ! sfzFile.replaceWithText (sfz))
        return emitAudioJob (job, "failed", 0.0, "Couldn't write the slices' instrument in " + folder.getFullPathName());

    // A new instrument track under the clip's, its Sonify Sampler playing the slices: one undo step.
    int index = static_cast<int> (engine.getTracks().size());

    for (size_t i = 0; i < engine.getTracks().size(); ++i)
        if (engine.getTracks()[i]->id == job.trackId)
            index = static_cast<int> (i) + 1;

    const auto id = engine.addTrack (Track::Kind::instrument);
    int at = 0;

    if (auto moved = engine.detachTrack (id, at))
        engine.attachTrack (std::move (moved), index);

    juce::String note;
    int insertId = 0;

    if (engine.loadPlugin (id, Builtins::file(), Samplers::playerId, -1, 0, insertId).wasOk())
    {
        if (auto* sampler = dynamic_cast<SonifySampler*> (engine.getPlugin (id, insertId)))
            sampler->loadInstrument (sfzFile, true);

        library.markUsed (Builtins::file().getFullPathName(), Samplers::playerId);
    }
    else
    {
        note = "The Sonify Sampler didn't load: the slices are in " + folder.getFullPathName();
    }

    engine.setTrackName (id, name);
    engine.addClip (id, std::move (part));
    addToHistory (historyName (job.op), new Edits::TrackAdded (engine, id, index));
    pushTracks();
    emitAudioJob (job, "done", 1.0, note, id);
}

juce::String MainComponent::startAudioOp (const AudioJob& request)
{
    const auto* clip = findAudioClip (request.trackId, request.clipId);

    if (clip == nullptr)
        return "Select an audio clip.";

    if (engine.isRecording() || engine.isTakeActive())
        return "Not while recording.";

    // A job of the user's own goes ahead of bringing a follower up to date.
    if (audioRender != nullptr || audioToMidi != nullptr || audioTempo != nullptr)
    {
        if (! audioJob.automatic || request.automatic)
            return "Sonify is still working on the last one.";

        cancelAudioJob (true);
    }

    auto job = request;
    job.generation = songGeneration;
    job.clip = *clip;
    job.mapGeneration = engine.getTransport().getMapGeneration();

    if (job.op == "detectTempo" || job.op == "analyse")
    {
        const auto analyse = job.op == "analyse";

        // Known already: the tempo it was synced from (Analyse listens all the same: its key and chords).
        if (! analyse && clip->sourceTempo > 0.0)
        {
            juce::DynamicObject::Ptr reply (new juce::DynamicObject());
            reply->setProperty ("trackId", job.trackId);
            reply->setProperty ("clipId", job.clipId);
            reply->setProperty ("bpm", clip->sourceTempo);
            reply->setProperty ("known", true);
            if (webView != nullptr) webView->emitEventIfBrowserIsVisible ("audioTempo", juce::var (reply.get()));
            return {};
        }

        if (clip->audioLength < 2.0)
            return analyse ? "The clip is too short to hear its chords in." : "The clip is too short to hear a tempo in.";

        juce::String error;
        RecordingAnalysis::Clip heardClip;
        heardClip.data = engine.getMedia().get (juce::File (clip->audioFile), engine.getPlaybackRate(), error);
        heardClip.offsetSeconds = clip->audioOffset;
        heardClip.lengthSeconds = clip->audioLength;

        if (heardClip.data == nullptr)
            return "The clip's audio is missing.";

        const auto state = engine.getTransport().getState();
        auto grid = RecordingAnalysis::Grid { state.bpm, state.numerator, state.denominator };

        // Analyse hears it where it plays, against the song's beat there: when it plays to that
        // beat, its chords are on the song's bars.
        if (analyse)
        {
            const auto& map = engine.getTempoMap();
            heardClip.startSeconds = map.secondsAt (clip->start);
            heardClip.gain = juce::Decibels::decibelsToGain (static_cast<float> (clip->audioGainDb), -100.0f);
            grid = RecordingAnalysis::gridAt (map, clip->start);
        }

        audioTempo = std::make_unique<RecordingAnalysis> (std::vector<RecordingAnalysis::Clip> { heardClip }, grid, 0.0);
        audioTempo->start();
        audioJob = job;
        audioProgressSent = 0.0;
        emitAudioJob (job, "working");
        return {};
    }

    if (job.op == "toMidi")
    {
        if (job.mode == AudioToMidi::Mode::notes)
            if (const auto problem = AudioToMidi::notesProblem(); problem.isNotEmpty())
                return problem;

        if (! juce::File (clip->audioFile).existsAsFile())
            return "The clip's audio is missing: " + juce::File (clip->audioFile).getFileName();

        audioToMidi = std::make_unique<AudioToMidi> (AudioToMidi::Job { juce::File (clip->audioFile), clip->audioOffset, clip->audioLength, job.mode });
        audioToMidi->start();
        audioJob = job;
        audioProgressSent = 0.0;
        emitAudioJob (job, "working");
        return {};
    }

    if (! isRenderOp (job.op))
        return "Sonify doesn't know that one.";

    if (job.op == "original" && ! clip->isRender())
        return "The clip plays its original already.";

    const auto warping = job.op == "stretch" || job.op == "warpMove" || job.op == "warpRemove";

    if (warping && clip->followsTempo())
        return "The clip follows the song's tempo: turn Follow off first, then stretch it.";

    // What the clip becomes, from its original, keeping what was done to it before.
    auto tempo = clip->sourceTempo, semitones = clip->pitch;
    auto reverse = clip->reversed, follow = clip->followTempo;

    if (job.op == "syncTempo" || warping)           tempo = warping ? clip->sourceTempo : job.tempo;
    else if (job.op == "followTempo" && job.follow) { follow = true; if (job.tempo > 0.0) tempo = job.tempo; }
    else if (job.op == "followTempo")               follow = false;
    else if (job.op == "pitch")                     semitones = job.semitones;
    else if (job.op == "reverse")                   reverse = ! reverse;
    else if (job.op == "original")                  { tempo = 0.0; semitones = 0.0; reverse = false; follow = false; }

    if ((job.op == "syncTempo" || (job.op == "followTempo" && follow)) && ! (tempo > 0.0))
        return "Give the clip's own tempo (Detect Tempo finds it).";

    tempo = tempo > 0.0 ? juce::jlimit (20.0, 400.0, tempo) : 0.0;
    semitones = juce::jlimit (-24.0, 24.0, std::round (semitones * 100.0) / 100.0);
    follow = follow && tempo > 0.0;

    // A follower is rendered whole, its window counted in the original's beats (at the tempo
    // given now); one becoming a follower starts from what it shows.
    AudioRender::Window window;

    if (follow && clip->followsTempo())
    {
        window = AudioRender::wholeWindowOf (*clip);
        const auto scale = tempo / clip->sourceTempo;
        job.leadBeats = clip->offset * scale;
        job.beats = clip->length * scale;

        // Turned round, the same audio still shows: counted from the other end.
        if (reverse != clip->reversed)
            job.leadBeats = juce::jmax (0.0, window.length * tempo / 60.0 - job.leadBeats - job.beats);
    }
    else
    {
        std::vector<std::pair<double, double>> shown;

        if (! shownTimeMap (*clip, window, shown))
            window = sourceWindowOf (*clip);

        job.leadBeats = 0.0;
        job.beats = window.length * tempo / 60.0;

        // The Stretcher: what it shows, that stretch made longer or shorter.
        if (job.op == "stretch")
        {
            const auto shownLength = shown.empty() ? 0.0 : shown.back().first;
            job.from = juce::jlimit (0.0, shownLength, job.from);
            job.to = juce::jlimit (0.0, shownLength, job.to);

            if (job.to - job.from < 0.01)
                return "Select a part of the clip to stretch.";

            const auto span = job.to - job.from;

            if (! (job.newLength >= 0.01) || job.newLength > span * 8.0 || job.newLength < span / 8.0)
                return "That's too far: up to 8 times longer or shorter.";

            job.shiftSeconds = job.keepEnd ? -(job.newLength - span) : 0.0;

            if (job.keepEnd && engine.getTempoMap().secondsAt (clip->start) + job.shiftSeconds < 0.0)
                return "There's no room before the clip: it would start before the song.";

            job.warp = stretchMap (shown, job.from, job.to, job.newLength);
        }
        // A warp marker dragged: from .. to are the markers either side of it (or the clip's ends).
        else if (job.op == "warpMove")
        {
            const auto shownLength = shown.empty() ? 0.0 : shown.back().first;
            const auto prev = juce::jlimit (0.0, shownLength, job.from);
            const auto next = juce::jlimit (0.0, shownLength, job.to);
            const auto at = job.at, newAt = job.newAt;

            if (! (prev < at - 0.005 && at < next - 0.005))
                return "That warp marker isn't inside the clip.";

            if (! (prev < newAt - 0.005 && newAt < next - 0.005))
                return "A warp marker can't pass the ones beside it.";

            const auto before = (newAt - prev) / (at - prev), after = (next - newAt) / (next - at);

            if (before > 8.0 || before < 1.0 / 8.0 || after > 8.0 || after < 1.0 / 8.0)
                return "That's too far: up to 8 times longer or shorter.";

            job.warp = warpMoveMap (shown, prev, at, next, newAt);
        }
        else if (job.op == "warpRemove")
        {
            if (! warpRemoveMap (shown, job.at, job.warp))
                return "There's no warp marker there.";
        }
        // Stretched before: pitch, reverse and the rest keep its stretches (Sync Tempo, Follow
        // and Back to the Original start again from the original).
        else if (! clip->warp.empty() && job.op != "syncTempo" && job.op != "followTempo" && job.op != "original")
        {
            job.warp = reverse != clip->reversed ? mirrored (shown) : shown;
        }
    }

    if (! window.file.existsAsFile())
        return "The clip's original is missing: " + window.file.getFileName();

    if (window.length < 0.01)
        return "The clip is too short.";

    job.tempo = tempo;
    job.semitones = semitones;
    job.reverse = reverse;
    job.follow = follow;
    job.source = window.file;
    job.sourceOffset = window.offset;
    job.sourceLength = window.length;

    // Follow Tempo off: the clip keeps the render it has and stops following.
    if (job.op == "followTempo" && ! follow)
    {
        changeAudioClip (job, [] (MidiClip& c) { c.followTempo = false; }, historyName (job.op));
        emitAudioJob (job, "done", 1.0);
        return {};
    }

    // Nothing left done to it: the original itself.
    if (tempo <= 0.0 && std::abs (semitones) < 1.0e-6 && ! reverse && job.warp.empty())
    {
        changeAudioClip (job, [this, &window] (MidiClip& c)
        {
            c.audioFile = window.file.getFullPathName();
            c.audioOffset = window.offset;
            c.audioLength = window.length;
            c.sourceFile.clear();
            c.sourceOffset = c.sourceLength = c.sourceTempo = c.pitch = 0.0;
            c.reversed = c.followTempo = false;
            c.length = engine.audioLengthInPpq (c.start, c.audioLength);
        }, historyName (job.op));

        emitAudioJob (job, "done", 1.0);
        return {};
    }

    auto renderJob = renderJobFor (*clip, window, reverse, semitones, job.warp.empty() ? tempo : 0.0,
                                   window.file.getFileNameWithoutExtension(), follow ? job.leadBeats : 0.0);

    // Its own time map (the Stretcher's) in place of the tempo's.
    if (job.warp.size() >= 2)
    {
        renderJob.timeMap = job.warp;
        renderJob.outputLength = job.warp.back().first;
    }

    if ((tempo > 0.0 || ! job.warp.empty()) && (renderJob.outputLength > window.length * 8.0 || renderJob.outputLength < window.length / 8.0))
        return job.warp.empty() ? "That tempo is too far from the song's to stretch to." : "That's stretched too far: up to 8 times longer or shorter.";

    job.render = renderJob;
    audioRender = std::make_unique<AudioRender> (renderJob);
    audioRender->start();
    audioJob = job;
    audioProgressSent = 0.0;
    emitAudioJob (job, "working");
    return {};
}

void MainComponent::cancelAudioJob (bool automaticOnly)
{
    if (automaticOnly && ! audioJob.automatic)
        return;

    // Stopped here and now: what it was doing is let go.
    const auto job = audioJob;
    audioRender = nullptr;
    audioToMidi = nullptr;
    audioTempo = nullptr;
    audioJob = {};
    emitAudioJob (job, "cancelled");

    // A follower set aside for the user's job is brought up to date after it.
    if (job.automatic)
        lookAtFollowersAgain();
}

//==============================================================================
void MainComponent::updateAudioJobs()
{
    const auto stale = audioJob.generation != songGeneration;

    const auto showProgress = [this] (double progress)
    {
        if (progress >= audioProgressSent + 0.02)
        {
            audioProgressSent = progress;
            emitAudioJob (audioJob, "working", progress);
        }
    };

    if (audioRender != nullptr)
    {
        if (! audioRender->isDone())
        {
            if (stale)
                cancelAudioJob (false);
            else
                showProgress (audioRender->getProgress());

            return;
        }

        const auto result = audioRender->getResult();
        audioRender = nullptr;

        if (stale)
        {
            emitAudioJob (std::exchange (audioJob, {}), "cancelled");
            return;
        }

        return finishAudioRender (result);
    }

    if (audioToMidi != nullptr)
    {
        if (! audioToMidi->isDone())
        {
            if (stale)
                cancelAudioJob (false);
            else
                showProgress (audioToMidi->getProgress());

            return;
        }

        // An AI request makes one undo step of what it does: the new track waits for it to end.
        if (aiSongBefore.isNotEmpty() && ! stale)
            return;

        const auto result = audioToMidi->getResult();
        audioToMidi = nullptr;

        if (stale)
        {
            emitAudioJob (std::exchange (audioJob, {}), "cancelled");
            return;
        }

        return finishAudioToMidi (result);
    }

    if (audioTempo != nullptr)
    {
        if (! audioTempo->isDone())
        {
            if (stale)
                cancelAudioJob (false);
            else
                showProgress (audioTempo->getProgress());

            return;
        }

        const auto result = audioTempo->getResult();
        audioTempo = nullptr;
        const auto job = std::exchange (audioJob, {});

        if (stale)
            return emitAudioJob (job, "cancelled");

        if (! result.ok || ! (result.bpm > 0.0))
            return emitAudioJob (job, result.error == "Cancelled" ? "cancelled" : "failed", 0.0,
                                 result.error.isNotEmpty() && result.error != "Cancelled" ? result.error : juce::String ("No steady tempo could be heard in the clip."));

        // Analyse: its tempo, key and chords (a bar at a time, from its first downbeat).
        if (job.op == "analyse")
        {
            juce::DynamicObject::Ptr analysis (new juce::DynamicObject());
            analysis->setProperty ("trackId", job.trackId);
            analysis->setProperty ("clipId", job.clipId);
            analysis->setProperty ("bpm", std::round (result.bpm * 100.0) / 100.0);
            analysis->setProperty ("steadiness", result.steadiness);
            analysis->setProperty ("keyTonic", result.keyTonic);
            analysis->setProperty ("keyMinor", result.keyMinor);
            analysis->setProperty ("chords", result.chords);
            analysis->setProperty ("bars", result.bars);
            analysis->setProperty ("followsSong", result.followsSong);
            analysis->setProperty ("firstBar", result.firstBar);
            analysis->setProperty ("downbeat", juce::jmax (0.0, result.downbeatSeconds - engine.getTempoMap().secondsAt (job.clip.start)));   // seconds into the clip

            if (webView != nullptr)
                webView->emitEventIfBrowserIsVisible ("audioAnalysis", juce::var (analysis.get()));

            emitAudioJob (job, "done", 1.0);
            return;
        }

        juce::DynamicObject::Ptr reply (new juce::DynamicObject());
        reply->setProperty ("trackId", job.trackId);
        reply->setProperty ("clipId", job.clipId);
        reply->setProperty ("bpm", std::round (result.bpm * 100.0) / 100.0);
        reply->setProperty ("steadiness", result.steadiness);
        reply->setProperty ("known", false);

        if (webView != nullptr)
            webView->emitEventIfBrowserIsVisible ("audioTempo", juce::var (reply.get()));

        emitAudioJob (job, "done", 1.0);
        return;
    }

    bringFollowersUpToDate();
}

void MainComponent::finishAudioRender (const AudioRender::Result& result)
{
    const auto job = std::exchange (audioJob, {});

    if (! result.ok)
    {
        // A follower that can't be made isn't tried again and again (until the tempo changes);
        // the others still are.
        if (job.automatic)
        {
            followFailed.insert (juce::String (job.trackId) + ":" + juce::String (job.clipId));
            lookAtFollowersAgain();
        }

        const auto cancelled = result.error == "Cancelled";
        return emitAudioJob (job, cancelled ? "cancelled" : "failed", 0.0, cancelled ? juce::String() : result.error);
    }

    // The clip changed meanwhile (or, for a follower brought up to date, the tempo did again):
    // this render is for something that isn't there now.
    if (! clipStillAsItWas (job) || (job.automatic && job.mapGeneration != engine.getTransport().getMapGeneration()))
    {
        lookAtFollowersAgain();
        return emitAudioJob (job, "failed", 0.0, "The clip changed while Sonify was working on it: try again.");
    }

    const auto changed = changeAudioClip (job, [this, &job, &result] (MidiClip& c)
    {
        c.audioFile = result.file.getFullPathName();
        c.sourceFile = job.source.getFullPathName();
        c.sourceOffset = job.sourceOffset;
        c.sourceLength = job.sourceLength;
        c.sourceTempo = job.tempo;
        c.pitch = job.semitones;
        c.reversed = job.reverse;
        c.followTempo = job.follow;

        if (c.followsTempo())
        {
            // The render is the whole window: the clip shows its beats of it.
            const auto perBeat = 60.0 / job.tempo;
            c.audioOffset = juce::jlimit (0.0, result.lengthSeconds, AudioRender::outputSecondsAt (job.render, job.leadBeats * perBeat));
            c.audioLength = juce::jlimit (0.0, result.lengthSeconds - c.audioOffset,
                                          AudioRender::outputSecondsAt (job.render, (job.leadBeats + job.beats) * perBeat) - c.audioOffset);
            c.offset = job.leadBeats;
            c.length = job.beats;
        }
        else
        {
            // The Stretcher's: before the stretch moved along with its end (keepEnd), the clip starts earlier or later.
            if (std::abs (job.shiftSeconds) > 1.0e-9)
            {
                const auto& map = engine.getTempoMap();
                c.start = juce::jmax (0.0, map.ppqAt (juce::jmax (0.0, map.secondsAt (c.start) + job.shiftSeconds)));
            }

            c.audioOffset = 0.0;
            c.audioLength = result.lengthSeconds;
            c.offset = 0.0;
            c.length = engine.audioLengthInPpq (c.start, c.audioLength);
        }

        c.warp = job.warp;   // the Stretcher's time map (none: the render's time is the tempo's, or its own)
    }, historyName (job.op));

    emitAudioJob (job, changed ? "done" : "failed", 1.0, changed ? juce::String() : juce::String ("The clip is gone."));
}

void MainComponent::finishAudioToMidi (const AudioToMidi::Result& result)
{
    const auto job = std::exchange (audioJob, {});

    if (! result.ok)
    {
        const auto cancelled = result.error == "Cancelled";
        return emitAudioJob (job, cancelled ? "cancelled" : "failed", 0.0, cancelled ? juce::String() : result.error);
    }

    const auto* clip = findAudioClip (job.trackId, job.clipId);
    const auto* source = engine.findTrack (job.trackId);

    if (clip == nullptr || source == nullptr)
        return emitAudioJob (job, "failed", 0.0, "The clip is gone.");

    if (! clipStillAsItWas (job))
        return emitAudioJob (job, "failed", 0.0, "The clip changed while Sonify was listening to it: try again.");

    if (result.notes.empty())
        return emitAudioJob (job, "failed", 0.0, job.mode == AudioToMidi::Mode::drums ? "No hits could be heard in the clip." : "No notes could be heard in the clip.");

    // The notes on the song's beats, from where the clip starts.
    const auto& map = engine.getTempoMap();
    const auto clipStart = clip->start, clipLength = clip->length;
    const auto startSeconds = map.secondsAt (clipStart);
    MidiClip part;
    part.start = clipStart;
    part.length = clipLength;

    for (const auto& n : result.notes)
    {
        MidiNote note;
        note.pitch = juce::jlimit (0, 127, n.pitch);
        note.start = juce::jmax (0.0, map.ppqAt (startSeconds + n.start) - clipStart);
        note.length = juce::jmax (1.0 / 64.0, map.ppqAt (startSeconds + n.start + n.length) - clipStart - note.start);
        note.velocity = static_cast<float> (juce::jlimit (1, 127, n.velocity)) / 127.0f;

        if (note.start < clipLength)
            part.notes.push_back (note);
    }

    std::sort (part.notes.begin(), part.notes.end(), [] (const MidiNote& a, const MidiNote& b) { return a.start < b.start; });

    // A new instrument track under the clip's, with the instrument that fits it best.
    int index = static_cast<int> (engine.getTracks().size());

    for (size_t i = 0; i < engine.getTracks().size(); ++i)
        if (engine.getTracks()[i]->id == job.trackId)
            index = static_cast<int> (i) + 1;

    const auto name = source->name + (job.mode == AudioToMidi::Mode::drums ? " Drums" : " MIDI");
    const auto id = engine.addTrack (Track::Kind::instrument);
    int at = 0;

    if (auto moved = engine.detachTrack (id, at))
        engine.attachTrack (std::move (moved), index);

    juce::String note;

    if (const auto* best = SessionPlayers::bestInstrument (library, job.mode == AudioToMidi::Mode::drums ? SessionPlayers::Player::drums
                                                                                                       : SessionPlayers::Player::keys))
    {
        int slot = 0;

        if (engine.loadPlugin (id, juce::File (best->file), best->id, -1, 0, slot).wasOk())
            library.markUsed (best->file, best->id);
        else
            note = best->name + " didn't load, so the new track plays the Test Synth.";
    }

    engine.setTrackName (id, name);
    engine.addClip (id, std::move (part));
    addToHistory (historyName (job.op), new Edits::TrackAdded (engine, id, index));
    pushTracks();
    emitAudioJob (job, "done", 1.0, note, id);
}

//==============================================================================
void MainComponent::bringFollowersUpToDate()
{
    if (engine.isRecording() || engine.isTakeActive() || aiSongBefore.isNotEmpty())
        return;

    // Only when the song's renders or its tempo changed is it worth looking again.
    juce::String stamp;
    stamp << static_cast<juce::int64> (engine.getTransport().getMapGeneration()) << "|" << songGeneration;

    for (const auto& track : engine.getTracks())
        if (track->kind == Track::Kind::audio)
            for (const auto& clip : track->clips)
                if (clip.isRender())
                    stamp << ";" << track->id << ":" << clip.id << ":" << juce::String (clip.start, 6) << ":" << clip.audioFile
                          << ":" << juce::String (clip.audioOffset, 6) << ":" << juce::String (clip.audioLength, 6) << ":" << juce::String (clip.offset, 6)
                          << ":" << (clip.followTempo ? 1 : 0);

    const auto now = juce::Time::getMillisecondCounter();

    // Renders that failed are tried again once the tempo (or the song) has changed.
    const auto failedFor = juce::String (static_cast<juce::int64> (engine.getTransport().getMapGeneration())) + "|" + juce::String (songGeneration);

    if (failedFor != followFailedFor)
    {
        followFailedFor = failedFor;
        followFailed.clear();
    }

    if (stamp != followStamp)
    {
        followStamp = stamp;
        followSeenAt = now;
        followNext.reset();

        // What each follower (and each render whose file is gone) should play now.
        for (const auto& track : engine.getTracks())
        {
            if (track->kind != Track::Kind::audio || followNext.has_value())
                continue;

            for (const auto& clip : track->clips)
            {
                if (! clip.isRender() || followFailed.count (juce::String (track->id) + ":" + juce::String (clip.id)) > 0)
                    continue;

                const auto missing = ! juce::File (clip.audioFile).existsAsFile();

                if ((! clip.followTempo && ! missing) || ! juce::File (clip.sourceFile).existsAsFile())
                    continue;

                // A follower: the whole window, from its beats before the clip; any other: what it shows.
                const auto window = clip.followsTempo() ? AudioRender::wholeWindowOf (clip) : sourceWindowOf (clip);
                const auto job = renderJobFor (clip, window, clip.reversed, clip.pitch, clip.sourceTempo, window.file.getFileNameWithoutExtension(),
                                               clip.followsTempo() ? clip.offset : 0.0);

                if (! missing && AudioRender::fileFor (job).getFullPathName() == clip.audioFile)
                    continue;

                AudioJob next;
                next.op = "refresh";
                next.trackId = track->id;
                next.clipId = clip.id;
                next.automatic = true;
                followNext = next;
                break;
            }
        }
    }

    // Once the tempo has stopped changing for a moment (a drag in the tempo lane).
    if (followNext.has_value() && now - followSeenAt >= 400)
    {
        const auto next = *followNext;
        followNext.reset();

        if (startAudioOp (next).isNotEmpty())
        {
            followFailed.insert (juce::String (next.trackId) + ":" + juce::String (next.clipId));
            lookAtFollowersAgain();
        }
    }
}

//==============================================================================
void MainComponent::emitAudioJob (const AudioJob& job, const juce::String& state, double progress, const juce::String& error, int newTrackId)
{
    if (webView == nullptr || job.automatic)
        return;

    juce::DynamicObject::Ptr object (new juce::DynamicObject());
    object->setProperty ("state", state);
    object->setProperty ("op", job.op);
    object->setProperty ("progress", progress);
    object->setProperty ("error", error);
    object->setProperty ("trackId", job.trackId);
    object->setProperty ("clipId", job.clipId);

    if (newTrackId != 0)
        object->setProperty ("newTrackId", newTrackId);

    webView->emitEventIfBrowserIsVisible ("audioJob", juce::var (object.get()));
}

//==============================================================================
// Strip Silence: the clip cut into the parts that sound (each fading in and out), the rest gone.
bool MainComponent::stripSilence (int trackId, int clipId, const MidiClip::Silence& requested, juce::String& problem)
{
    const auto* found = findAudioClip (trackId, clipId);

    if (found == nullptr)
        return false;

    const auto clip = *found;
    auto silence = requested;
    silence.on = true;

    double silenced = 0.0;
    const auto quiet = engine.silenceSeconds (clip.audioFile, silence, clip.audioOffset, clip.audioOffset + clip.audioLength, silenced);

    if (quiet.empty())
    {
        problem = "Nothing in the clip is that quiet: raise the threshold.";
        return false;
    }

    // The parts between the silent stretches (seconds of the file), the shortest let go.
    std::vector<std::pair<double, double>> sounding;
    auto from = clip.audioOffset;

    for (const auto& [a, b] : quiet)
    {
        if (a - from >= 0.03)
            sounding.push_back ({ from, a });

        from = b;
    }

    if (clip.audioOffset + clip.audioLength - from >= 0.03)
        sounding.push_back ({ from, clip.audioOffset + clip.audioLength });

    if (sounding.empty())
    {
        problem = "The whole clip is quieter than that: lower the threshold.";
        return false;
    }

    const auto& map = engine.getTempoMap();
    const auto clipSeconds = map.secondsAt (clip.start);

    if (! engine.deleteClip (trackId, clipId))
        return false;

    for (const auto& [a, b] : sounding)
    {
        auto part = clip;
        part.id = 0;
        part.silence = {};
        part.audioOffset = a;
        part.audioLength = b - a;
        part.start = map.ppqAt (clipSeconds + a - clip.audioOffset);
        part.length = engine.audioLengthInPpq (part.start, part.audioLength);

        // Soft edges where it was cut out of the silence (the clip's own fades where it had them).
        const auto edge = juce::jmin (juce::jmax (0.005, silence.fade), part.audioLength / 3.0);
        part.fadeIn = a <= clip.audioOffset + 1.0e-9 ? juce::jmin (clip.fadeIn, part.audioLength / 2.0) : edge;
        part.fadeOut = b >= clip.audioOffset + clip.audioLength - 1.0e-9 ? juce::jmin (clip.fadeOut, part.audioLength / 2.0) : edge;
        engine.addClip (trackId, std::move (part));
    }

    return true;
}

//==============================================================================
// Merge Clips and Bounce to New Track
void MainComponent::mergeAudioClips (const juce::var& event)
{
    const auto trackId = static_cast<int> (event.getProperty ("trackId", 0));
    const auto* track = engine.findTrack (trackId);
    std::vector<int> selected;

    if (const auto* ids = event.getProperty ("clipIds", {}).getArray())
        for (const auto& id : *ids)
            if (findAudioClip (trackId, static_cast<int> (id)) != nullptr)
                selected.push_back (static_cast<int> (id));

    if (track == nullptr || selected.size() < 2)
        return pushTracks ("Select two or more audio clips on one track to merge.");

    // As Studio One's Bounce Selection: from the first selected clip to the last one's end, with
    // the clips between them (the muted ones stay as they are).
    auto from = std::numeric_limits<double>::max(), to = 0.0;

    for (const auto& clip : track->clips)
        if (std::find (selected.begin(), selected.end(), clip.id) != selected.end())
        {
            from = juce::jmin (from, clip.start);
            to = juce::jmax (to, clip.start + clip.length);
        }

    std::vector<int> merged;

    for (const auto& clip : track->clips)
    {
        const bool chosen = std::find (selected.begin(), selected.end(), clip.id) != selected.end();
        const bool between = clip.isAudio() && ! clip.muted && clip.start >= from && clip.start < to;

        if (chosen || between)
            merged.push_back (clip.id);
    }

    startClipBounce (ClipBounce::Kind::merge, trackId, std::move (merged));
}

void MainComponent::bounceClipsToNewTrack (const juce::var& event)
{
    const auto trackId = static_cast<int> (event.getProperty ("trackId", 0));
    const auto* track = engine.findTrack (trackId);
    std::vector<int> chosen;

    if (track != nullptr)
        if (const auto* ids = event.getProperty ("clipIds", {}).getArray())
            for (const auto& id : *ids)
                for (const auto& clip : track->clips)
                    if (clip.id == static_cast<int> (id))
                        chosen.push_back (clip.id);

    if (track == nullptr || chosen.empty())
        return pushTracks ("Select the clips to bounce (on one track).");

    startClipBounce (ClipBounce::Kind::newTrack, trackId, std::move (chosen));
}

void MainComponent::startClipBounce (ClipBounce::Kind kind, int trackId, std::vector<int> clipIds)
{
    const auto refuse = [this] (const juce::String& why) { pushTracks (why); };
    const auto what = juce::String (kind == ClipBounce::Kind::merge ? "merged" : "bounced");

    if (clipBounce.trackId != 0 || clipExport.trackId != 0)
        return refuse ("Clips are being rendered already: one at a time.");

    if (freezingTrackId != 0 || masteringStage != MasteringStage::idle || videoExportStage != VideoExportStage::idle || engine.isMixdownRunning())
        return refuse ("Not while Sonify is rendering something else.");

    if (engine.isRecording() || engine.isTakeActive())
        return refuse ("Not while recording.");

    if (! (engine.getPlaybackRate() > 0.0))
        return refuse ("Clips are " + what + " with an audio device open.");

    const auto* track = engine.findTrack (trackId);

    if (track == nullptr)
        return refuse ("That track isn't in the song any more.");

    auto from = std::numeric_limits<double>::max(), to = 0.0;

    for (const auto& clip : track->clips)
        if (std::find (clipIds.begin(), clipIds.end(), clip.id) != clipIds.end())
        {
            from = juce::jmin (from, clip.start);
            to = juce::jmax (to, clip.start + clip.length);
        }

    const auto& map = engine.getTempoMap();

    if (to <= from || map.secondsAt (to) - map.secondsAt (from) > 30.0 * 60.0)
        return refuse ("Those clips can't be " + what + ": 30 minutes at most.");

    // A float WAV with the song's recordings.
    auto folder = engine.getRecordingFolder() != juce::File() ? engine.getRecordingFolder() : UserData::recordings();
    folder = folder.getChildFile (kind == ClipBounce::Kind::merge ? "Merged" : "Bounced");

    if (! folder.createDirectory())
        return refuse ("Couldn't make the folder " + folder.getFullPathName());

    const auto name = juce::File::createLegalFileName (track->name.trim().isNotEmpty() ? track->name.trim() : juce::String ("Track"))
                    + (kind == ClipBounce::Kind::merge ? " merged" : " bounce");

    ClipBounce job;
    job.kind = kind;
    job.trackId = trackId;
    job.clipIds = clipIds;
    job.startPpq = from;
    job.endPpq = to;
    job.file = folder.getNonexistentChildFile (name, ".wav", false);
    job.generation = songGeneration;

    // Merge: exactly the clips, without the track's inserts. Bounce: through the track, with
    // what rings on after (trimmed where it falls silent).
    const bool merge = kind == ClipBounce::Kind::merge;

    if (const auto result = engine.startClipRender (trackId, clipIds, job.file, merge ? 0.0 : 4.0, merge); result.failed())
        return refuse (result.getErrorMessage());

    clipBounce = job;
    pushTracks (merge ? juce::String ("Merging the clips: the song is quiet until it's done.")
                      : juce::String ("Bouncing to a new track: the song is quiet until it's done."));
    pushStatus();
}

void MainComponent::updateClipBounce()
{
    juce::File written;
    const auto result = engine.collectMixdown (written);

    if (! result.has_value())
        return;   // still rendering

    const auto job = std::exchange (clipBounce, {});
    const bool merge = job.kind == ClipBounce::Kind::merge;
    const auto finish = [this, &job] (const juce::String& notice)
    {
        job.file.deleteFile();
        pushTracks (notice);
        pushStatus();
    };

    if (result->failed())
        return finish (result->getErrorMessage() == "Cancelled" ? juce::String() : "Couldn't render the clips: " + result->getErrorMessage());

    // The song as it was when it started: the same song, the track and its clips still there.
    const auto* track = engine.findTrack (job.trackId);
    bool allThere = track != nullptr && job.generation == songGeneration;

    for (const auto id : job.clipIds)
        allThere = allThere && std::any_of (track->clips.begin(), track->clips.end(), [id] (const MidiClip& c) { return c.id == id; });

    if (! allThere)
        return finish ("The clips changed while they rendered: nothing was " + juce::String (merge ? "merged." : "bounced."));

    // How long the render sounds: a bounce ends where what rings on falls silent.
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (job.file));

    if (reader == nullptr || reader->lengthInSamples <= 0 || ! (reader->sampleRate > 0.0))
        return finish ("Couldn't read the render back.");

    const auto& map = engine.getTempoMap();
    const auto rate = reader->sampleRate;
    const auto spanSeconds = map.secondsAt (job.endPpq) - map.secondsAt (job.startPpq);
    auto seconds = juce::jmin (spanSeconds, static_cast<double> (reader->lengthInSamples) / rate);

    if (! merge)
    {
        const auto spanEnd = static_cast<juce::int64> (spanSeconds * rate);
        constexpr juce::int64 chunk = 8192;
        juce::AudioBuffer<float> block (static_cast<int> (juce::jmax (1u, reader->numChannels)), static_cast<int> (chunk));
        juce::int64 lastSounding = 0;

        for (juce::int64 at = spanEnd; at < reader->lengthInSamples; at += chunk)
        {
            const auto count = static_cast<int> (juce::jmin (chunk, reader->lengthInSamples - at));
            reader->read (&block, 0, count, at, true, true);

            for (int c = 0; c < block.getNumChannels(); ++c)
                for (int i = count; --i >= 0;)
                    if (std::abs (block.getSample (c, i)) > 1.0e-4f)   // -80 dB
                    {
                        lastSounding = juce::jmax (lastSounding, at + i + 1);
                        break;
                    }
        }

        const auto ringsOn = juce::jmin (static_cast<double> (reader->lengthInSamples), static_cast<double> (lastSounding) + 0.05 * rate) / rate;
        seconds = juce::jmax (spanSeconds, ringsOn);
        seconds = juce::jmin (seconds, static_cast<double> (reader->lengthInSamples) / rate);
    }

    reader.reset();

    MidiClip made;
    made.audioFile = job.file.getFullPathName();
    made.audioOffset = 0.0;
    made.audioLength = seconds;
    made.start = job.startPpq;
    made.length = engine.audioLengthInPpq (made.start, seconds);

    if (merge)
    {
        // The merged clip in their place: one undo step.
        auto before = Edits::snapshot (engine, { job.trackId });

        for (const auto id : job.clipIds)
            engine.deleteClip (job.trackId, id);

        engine.addClip (job.trackId, std::move (made));
        addToHistory ("Merge Clips", new Edits::ClipsChanged (engine, std::move (before), Edits::snapshot (engine, { job.trackId })));
    }
    else
    {
        // A new audio track under the track with the bounce, and the clips bounced muted: one undo step.
        const auto newTrack = addAudioTrackAfter (job.trackId, track->name.trim() + " Bounce", "Bounce to New Track", false);
        auto beforeNew = Edits::snapshot (engine, { newTrack });
        engine.addClip (newTrack, std::move (made));
        addToHistory ("Bounce to New Track", new Edits::ClipsChanged (engine, std::move (beforeNew), Edits::snapshot (engine, { newTrack })), true);

        auto beforeSource = Edits::snapshot (engine, { job.trackId });

        for (const auto id : job.clipIds)
            engine.setClipMuted (job.trackId, id, true);

        addToHistory ("Bounce to New Track", new Edits::ClipsChanged (engine, std::move (beforeSource), Edits::snapshot (engine, { job.trackId })), true);
        selectNewTracks ({ newTrack });
    }

    markChanged();
    pushTracks();
    pushStatus();
}

//==============================================================================
// Detect Transients: where the clip's sound jumps up (a hit, a pluck, a syllable), in seconds of its
// file within its window; sensitivity 0 (only the strongest) .. 1 (the faintest).
void MainComponent::sendTransients (const juce::var& event)
{
    const auto trackId = static_cast<int> (event.getProperty ("trackId", 0));
    const auto clipId = static_cast<int> (event.getProperty ("clipId", 0));
    const auto sensitivity = juce::jlimit (0.0, 1.0, static_cast<double> (event.getProperty ("sensitivity", 0.5)));
    const auto* clip = findAudioClip (trackId, clipId);

    if (clip == nullptr || webView == nullptr)
        return;

    juce::String error;
    const auto data = engine.getMedia().get (juce::File (clip->audioFile), engine.getPlaybackRate(), error);
    juce::Array<juce::var> times;

    if (data != nullptr && data->playbackRate > 0.0)
    {
        const auto rate = data->playbackRate;
        const auto& samples = data->samples;
        const auto from = juce::jlimit (0, samples.getNumSamples(), static_cast<int> (std::llround (clip->audioOffset * rate)));
        const auto to = juce::jlimit (from, samples.getNumSamples(), static_cast<int> (std::llround ((clip->audioOffset + clip->audioLength) * rate)));
        constexpr int hop = 256, window = 1024;

        // The energy (dB) of each hop, its channels together.
        std::vector<double> energy;

        for (int at = from; at + hop <= to; at += hop)
        {
            double sum = 0.0;
            const auto end = juce::jmin (to, at + window);

            for (int c = 0; c < samples.getNumChannels(); ++c)
                for (int i = at; i < end; ++i)
                    sum += static_cast<double> (samples.getSample (c, i)) * samples.getSample (c, i);

            energy.push_back (10.0 * std::log10 (sum / juce::jmax (1, (end - at) * samples.getNumChannels()) + 1.0e-12));
        }

        // A rise of `jump` dB over the recent average, loud enough to count, at least 50 ms apart.
        const auto jump = 9.0 - 6.0 * sensitivity;          // 9 dB (low) .. 3 dB (high)
        const auto floorDb = -55.0 + 10.0 * (1.0 - sensitivity);
        const auto gap = static_cast<int> (std::ceil (0.05 * rate / hop));
        int lastHit = -gap;

        for (int n = 4; n < static_cast<int> (energy.size()) - 1; ++n)
        {
            double recent = 0.0;
            const auto span = juce::jmin (8, n);

            for (int k = 1; k <= span; ++k)
                recent += energy[static_cast<size_t> (n - k)];

            recent /= span;
            const auto rise = energy[static_cast<size_t> (n)] - recent;

            if (rise >= jump && energy[static_cast<size_t> (n)] > floorDb && energy[static_cast<size_t> (n)] >= energy[static_cast<size_t> (n + 1)] - 1.5
                && n - lastHit >= gap)
            {
                // Back to where the rise starts: the quietest hop just before it.
                auto start = n;

                for (int k = n - 1; k >= juce::jmax (0, n - 4); --k)
                    if (energy[static_cast<size_t> (k)] < energy[static_cast<size_t> (start)])
                        start = k;

                const auto seconds = static_cast<double> (from + (start + (start < n ? 1 : 0)) * hop) / rate;

                if (seconds > clip->audioOffset + 0.01 && seconds < clip->audioOffset + clip->audioLength - 0.01)
                    times.add (std::round (seconds * 10000.0) / 10000.0);

                lastHit = n;
            }
        }
    }

    juce::DynamicObject::Ptr reply (new juce::DynamicObject());
    reply->setProperty ("trackId", trackId);
    reply->setProperty ("clipId", clipId);
    reply->setProperty ("key", event.getProperty ("key", {}));
    reply->setProperty ("times", times);
    webView->emitEventIfBrowserIsVisible ("transients", juce::var (reply.get()));
}
