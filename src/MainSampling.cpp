// D:\workspace\Fanan_web_hub\src\MainSampling.cpp
//
// The Sampling Studio (SamplingStudio.h), its own tab: Sampler Arena's Sampling Module. VSTi
// Sampling and Chord Sampling record the studio's nodes (SamplingRack.h: VSTis and effects loaded
// straight from their files, wired from the Sampling node to the Output), a synth outside the
// computer or the user playing into WAVs (and an SFZ); the Sample Builder makes an instrument from audio files and
// the song's clips put in each key's slots; presets are Sampler Arena's .smppreset; the samples go
// to the module's folder (Set Folder) or, for now, an Ad Hoc folder. The Loop Editor (SfzLoops.h)
// opens an SFZ: each sample's loop found (Auto Find, Batch Find All), set by hand, or picked by
// Fanan AI, then saved into the SFZ and the WAVs. What it makes plays in the Sonify Sampler
// (SonifySampler.h): Try It puts one on a new track with the instrument in it.
//
// Page events, one: sampling {action:
//   shown {shown} | state | config {settings} (the page's choices, kept) |
//   start {settings, replace} (replace: files of those names may be written over; the Sample
//   Builder builds) | stop | resume | finish | discard | skip | redo |
//   reveal {path?} (the folder in Explorer: that one, else where the samples go) |
//   setFolder | adHoc | adHocClear | presetSave {settings} | presetLoad |
//   audition {note, velocity, on, settings} (a key heard: through the nodes, or the synth) |
//   rack {op: state | browse {format, folder} | load {format, path, pluginId?, x, y} |
//         move {id, x, y} | connect {from, to, fromSide?, toSide?} | disconnect {from, to,
//         fromSide?, toSide?} (a sound wire's sides: left 0, right 1; none: both) | remove {id} |
//         mute {id, on} | pass {id, on} | window {id} | midiInputs {inputs} (the MIDI In node's
//         ports, "*": all) | visible {on} (the nodes show: the MIDI In node listens)} (the nodes) |
//   tryIt {sfz?} | builderLoad {note, row} | builderDrag {left, top, width, height, note, row} |
//   builderInfo {paths} |
//   loopLoad | loopOpen {path} | loopSelect {index} | loopFind | loopCandidate {index} |
//   loopSet {start, end} | loopRemove | loopBatch | loopSave {as} | loopPlay | loopStop |
//   loopXfade {ms, on} | loopAi {all} | blob {op ...} (SFZ to Blob: MainBlobs.cpp)}.
// Sonify sends: sampling {kind: state (everything) | progress (a session or build under way,
// with its meters) | exists {name, folder, count} (files of those names are there: write over
// them?) | builderFiles {paths, note, row} | builderInfo {files} | preset {settings} |
// rack {rack: {nodes, wires, ready}, formats, midiPorts [{name, device}]} | midiMonitor {messages
// [{n, time, source, bytes, sysex, heard}]} (what came in since, while the studio is in front: the
// page keeps the last 20) | rackBrowse {format, folder, parent, entries} |
// rackChoose {format, path, x, y, plugins} (a file with several plugins: which one?) | loop
// (the Loop Editor: its patch, the sample, its candidates, peaks when it changed) | loopPosition
// {position, playing} | tried {trackId} | problem {text} | blob (SFZ to Blob's list)}.

#include "MainComponent.h"
#include "BuiltinPlugins.h"
#include "PageDropZone.h"
#include "SamplerScreen.h"
#include "SonifySampler.h"
#include "TomahawkScreen.h"
#include "TomahawkSampler.h"

namespace
{
    template <typename Handler>
    std::function<void (const juce::var&)> afterSamplingEvent (MainComponent* owner, Handler handler)
    {
        // As every page event: after WebView2's own callback has returned (see afterEvent).
        return [safe = juce::Component::SafePointer<MainComponent> (owner), handler = std::move (handler)] (const juce::var& event)
        {
            juce::MessageManager::callAsync ([safe, handler, event]
            {
                if (safe != nullptr)
                    handler (event);
            });
        };
    }

    juce::DynamicObject::Ptr object()
    {
        return new juce::DynamicObject();
    }

    juce::String stateName (Sampling::Session::State state)
    {
        switch (state)
        {
            case Sampling::Session::State::ready:      return "ready";
            case Sampling::Session::State::running:    return "running";
            case Sampling::Session::State::stopping:   return "stopping";
            case Sampling::Session::State::stopped:    return "stopped";
            case Sampling::Session::State::finishing:  return "finishing";
            case Sampling::Session::State::done:       return "done";
            case Sampling::Session::State::failed:     return "failed";
        }

        return "ready";
    }

    juce::String stepName (Sampling::Session::Step step)
    {
        switch (step)
        {
            case Sampling::Session::Step::settling:   return "settling";
            case Sampling::Session::Step::playing:    return "playing";
            case Sampling::Session::Step::releasing:  return "releasing";
            case Sampling::Session::Step::waiting:    return "waiting";
            case Sampling::Session::Step::recording:  return "recording";
            case Sampling::Session::Step::idle:       break;
        }

        return "idle";
    }

    /** Each shot's mark, one character each: 0 to record, 1 recorded, 2 left out. */
    juce::String marksOf (const Sampling::Session& session)
    {
        const auto count = static_cast<int> (session.getShots().size());
        juce::HeapBlock<char> marks (static_cast<size_t> (count) + 1);

        for (int i = 0; i < count; ++i)
            marks[i] = session.isLeftOut (i) ? '2' : session.isRecorded (i) ? '1' : '0';

        marks[count] = 0;
        return juce::String (marks.get());
    }

    void addStatus (Sampling::Session& session, juce::DynamicObject& into)
    {
        const auto status = session.getStatus();
        into.setProperty ("state", stateName (status.state));
        into.setProperty ("step", stepName (status.step));
        into.setProperty ("shot", status.shot);
        into.setProperty ("recorded", status.recorded);
        into.setProperty ("total", status.total);
        into.setProperty ("silent", status.silent);
        into.setProperty ("written", status.written);
        into.setProperty ("progress", status.progress);
        into.setProperty ("left", status.left);
        into.setProperty ("right", status.right);
        into.setProperty ("message", status.message);
        into.setProperty ("marks", marksOf (session));
        into.setProperty ("offline", session.isOffline());
    }

    juce::String msText (int crossfade, double rate)
    {
        return juce::String (juce::roundToInt (crossfade * 1000.0 / juce::jmax (1.0, rate)));
    }
}

//==============================================================================
void MainComponent::setupSampling()
{
    // The Sonify Sampler opens its own window, and finds sampled instruments with the user's things.
    SamplerScreen::install();
    Samplers::setUserFolder ([] { return UserData::getFolder(); });

    // The Tomahawk opens its own window, and keeps its settings and presets with the user's things.
    TomahawkScreen::install();
    Tomahawks::setUserFolder ([] { return UserData::getFolder(); });

    // The folders and the nodes kept from last time. The settings start fresh every time: a setup
    // comes back only by Load Preset (nothing is loaded by itself).
    const auto kept = juce::JSON::parse (samplingSettingsFile());

    if (kept.isObject())
    {

        if (const juce::File folder (kept.getProperty ("folder", {}).toString()); juce::File::isAbsolutePath (folder.getFullPathName()))
            samplingModuleFolder = folder;

        if (const juce::File folder (kept.getProperty ("loopFolder", {}).toString()); juce::File::isAbsolutePath (folder.getFullPathName()))
            loopEdit.folder = folder;

        // The nodes start empty: no plugin loads until the user loads one (a preset brings its own).
    }

    if (! samplingSettings.isObject())
        samplingSettings = Sampling::Settings().toVar();

    // Where each format's plugins were last loaded from.
    const auto folders = juce::JSON::parse (samplingPluginFoldersFile());

    if (auto* object = folders.getDynamicObject())
        for (const auto& property : object->getProperties())
            if (const juce::File folder (property.value.toString()); juce::File::isAbsolutePath (folder.getFullPathName()))
                samplingPluginFolders[property.name.toString()] = folder;
}

juce::File MainComponent::samplingPluginFoldersFile()
{
    return SettingsFolder::file ("sampling-plugin-folders.json");
}

juce::WebBrowserComponent::Options MainComponent::withSamplingEvents (juce::WebBrowserComponent::Options options)
{
    return options.withEventListener ("sampling", afterSamplingEvent (this, [this] (const juce::var& event) { handleSampling (event); }));
}

juce::File MainComponent::samplingSettingsFile()
{
    return SettingsFolder::file ("sampling-studio.json");
}

juce::File MainComponent::samplingModuleFolderOrDefault() const
{
    return samplingModuleFolder != juce::File() ? samplingModuleFolder : Samplers::folder();
}

juce::File MainComponent::samplingFolder() const
{
    return samplingAdHoc.isDirectory() ? samplingAdHoc : samplingModuleFolderOrDefault();
}

void MainComponent::keepSampling()
{
    auto kept = object();
    kept->setProperty ("settings", samplingSettings);
    kept->setProperty ("folder", samplingModuleFolder.getFullPathName());
    kept->setProperty ("loopFolder", loopEdit.folder.getFullPathName());
    samplingSettingsFile().replaceWithText (juce::JSON::toString (juce::var (kept.get()), true));
}

void MainComponent::emitSampling (const juce::var& payload)
{
    if (webView != nullptr)
        webView->emitEventIfBrowserIsVisible ("sampling", payload);
}

void MainComponent::samplingProblem (const juce::String& text)
{
    auto item = object();
    item->setProperty ("kind", "problem");
    item->setProperty ("text", text);
    emitSampling (juce::var (item.get()));
}

//==============================================================================
void MainComponent::handleSampling (const juce::var& event)
{
    const auto action = event.getProperty ("action", {}).toString();

    if (action.startsWith ("loop"))
    {
        handleLoopEditor (action, event);
        return;
    }

    if (action == "shown")
    {
        samplingShown = static_cast<bool> (event.getProperty ("shown", false));

        if (samplingShown)
        {
            makeSamplingRack();
            pushSampling();
            pushLoopEditor (true);
            pushSamplingRack();
        }
        else
        {
            // A key held for an audition is let go.
            if (auditionOutput != nullptr)
                auditionOutput = nullptr;

            // The nodes' windows go with the studio; their plugins' settings are kept.
            if (samplingRack != nullptr)
            {
                samplingRack->closeWindows();
                keepSampling();
            }
        }

        updateSamplingRackAwake();
        return;
    }

    if (action == "blob")
    {
        handleBlobMaker (event);
        return;
    }

    if (action == "rack")
    {
        handleSamplingRack (event);
        return;
    }

    if (action == "state")
    {
        pushSampling();
        return;
    }

    if (action == "config")
    {
        if (const auto settings = event.getProperty ("settings", {}); settings.isObject())
        {
            samplingSettings = Sampling::Settings::fromVar (settings).toVar();
            keepSampling();
        }

        return;
    }

    if (action == "start")
    {
        if (const auto settings = event.getProperty ("settings", {}); settings.isObject())
        {
            samplingSettings = Sampling::Settings::fromVar (settings).toVar();
            keepSampling();
        }

        if (Sampling::Settings::fromVar (samplingSettings).mode == Sampling::Mode::builder)
            startSampleBuild (event);
        else
            startSampling (event);

        return;
    }

    if (action == "stop" || action == "resume" || action == "finish" || action == "skip" || action == "redo")
    {
        if (sampling == nullptr)
            return;

        if (action == "stop")         sampling->stop();
        else if (action == "resume")  sampling->resume();
        else if (action == "finish")  sampling->finish();
        else if (action == "skip")    sampling->skip();
        else                          sampling->redo();

        samplingTimer.startTimerHz (12);
        pushSampling();
        return;
    }

    if (action == "discard")
    {
        sampling = nullptr;   // its takes go with it (what a finished one made stays)

        if (sampleBuild != nullptr && sampleBuild->isDone())
            sampleBuild = nullptr;

        samplingStatePushed = -1;
        pushSampling();
        return;
    }

    if (action == "reveal")
    {
        const juce::File path (event.getProperty ("path", {}).toString());

        if (juce::File::isAbsolutePath (path.getFullPathName()) && path.exists())
        {
            path.revealToUser();
            return;
        }

        auto folder = samplingFolder();
        folder.createDirectory();
        folder.startAsProcess();
        return;
    }

    if (action == "setFolder" || action == "adHoc")
    {
        const bool adHoc = action == "adHoc";
        const auto start = adHoc && samplingAdHoc.isDirectory() ? samplingAdHoc : samplingModuleFolderOrDefault();
        samplingChooser = std::make_unique<juce::FileChooser> (adHoc ? "Select Ad-Hoc Output Folder" : "Select Sampling Folder", start);
        samplingChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                      [safe = juce::Component::SafePointer<MainComponent> (this), adHoc] (const juce::FileChooser& chooser)
        {
            if (safe == nullptr)
                return;

            const auto result = chooser.getResult();

            if (! result.isDirectory())
                return;

            if (adHoc)
            {
                // The module's own folder chosen: no Ad Hoc folder any more.
                safe->samplingAdHoc = result == safe->samplingModuleFolderOrDefault() ? juce::File() : result;
            }
            else
            {
                safe->samplingModuleFolder = result;
                safe->keepSampling();
            }

            safe->pushSampling();
        });
        return;
    }

    if (action == "adHocClear")
    {
        samplingAdHoc = juce::File();
        pushSampling();
        return;
    }

    if (action == "presetSave")
    {
        if (const auto settings = event.getProperty ("settings", {}); settings.isObject())
            samplingSettings = Sampling::Settings::fromVar (settings).toVar();

        const auto family = Sampling::Settings::fromVar (samplingSettings).family;
        const auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (Sfz::safeName (family) + ".smppreset");
        samplingChooser = std::make_unique<juce::FileChooser> ("Save Sampling Preset", start, "*.smppreset");
        samplingChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                          | juce::FileBrowserComponent::warnAboutOverwriting,
                                      [safe = juce::Component::SafePointer<MainComponent> (this)] (const juce::FileChooser& chooser)
        {
            if (safe == nullptr || chooser.getResult() == juce::File())
                return;

            const auto target = chooser.getResult().withFileExtension ("smppreset");
            auto tree = Sampling::Settings::fromVar (safe->samplingSettings).toPreset();
            tree.setProperty ("folder", safe->samplingModuleFolderOrDefault().getFullPathName(), nullptr);

            // Sonify's nodes with it, their plugins' settings too (Sampler Arena passes them by).
            if (safe->samplingRack != nullptr)
                tree.setProperty ("sonifyRack", juce::JSON::toString (safe->samplingRack->toVar (true), true), nullptr);

            juce::MemoryOutputStream stream;
            tree.writeToStream (stream);

            if (! target.replaceWithData (stream.getData(), stream.getDataSize()))
            {
                safe->samplingProblem ("Sonify couldn't write " + target.getFullPathName() + ".");
                return;
            }

            auto item = object();
            item->setProperty ("kind", "notice");
            item->setProperty ("text", "Preset saved: " + target.getFileName());
            safe->emitSampling (juce::var (item.get()));
        });
        return;
    }

    if (action == "presetLoad")
    {
        samplingChooser = std::make_unique<juce::FileChooser> ("Load Sampling Preset", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.smppreset");
        samplingChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                      [safe = juce::Component::SafePointer<MainComponent> (this)] (const juce::FileChooser& chooser)
        {
            if (safe == nullptr || ! chooser.getResult().existsAsFile())
                return;

            juce::MemoryBlock data;
            chooser.getResult().loadFileAsData (data);
            const auto tree = juce::ValueTree::readFromData (data.getData(), data.getSize());

            if (! tree.isValid() || ! tree.hasType ("AutoSamplerState"))
            {
                safe->samplingProblem (chooser.getResult().getFileName() + " isn't a Sampling preset.");
                return;
            }

            safe->samplingSettings = Sampling::Settings::fromPreset (tree, Sampling::Settings::fromVar (safe->samplingSettings)).toVar();

            if (const juce::File folder (tree.getProperty ("folder", {}).toString()); juce::File::isAbsolutePath (folder.getFullPathName()) && folder.isDirectory())
                safe->samplingModuleFolder = folder;

            // Its nodes, their plugins loaded again (not while a session plays through them).
            if (const auto rack = juce::JSON::parse (tree.getProperty ("sonifyRack", {}).toString()); rack.isObject())
            {
                const auto playing = safe->sampling != nullptr && (safe->sampling->getState() == Sampling::Session::State::running
                                                                   || safe->sampling->getState() == Sampling::Session::State::stopping);

                if (! playing)
                {
                    safe->makeSamplingRack();
                    juce::StringArray problems;
                    safe->samplingRack->restore (rack, problems);
                    safe->pushSamplingRack();

                    if (! problems.isEmpty())
                        safe->samplingProblem ("Some nodes' plugins couldn't be loaded: " + problems.joinIntoString ("; "));
                }
            }

            safe->keepSampling();

            auto item = object();
            item->setProperty ("kind", "preset");
            item->setProperty ("settings", safe->samplingSettings);
            item->setProperty ("name", chooser.getResult().getFileNameWithoutExtension());
            safe->emitSampling (juce::var (item.get()));
            safe->pushSampling();
        });
        return;
    }

    if (action == "audition")
    {
        auditionSampling (event);
        return;
    }

    if (action == "tryIt")
    {
        const juce::File asked (event.getProperty ("sfz", {}).toString());
        const auto sfz = juce::File::isAbsolutePath (asked.getFullPathName()) && asked.existsAsFile() ? asked : sampledInstrument;

        if (! sfz.existsAsFile())
        {
            samplingProblem ("There's no instrument to try yet: turn on Create SFZ (Settings) to make one.");
            return;
        }

        trySampledInstrument (sfz, sfz == sampledInstrument && sampledName.isNotEmpty() ? sampledName : sfz.getFileNameWithoutExtension());
        return;
    }

    if (action == "builderLoad")
    {
        const auto note = static_cast<int> (event.getProperty ("note", 60));
        const auto row = static_cast<int> (event.getProperty ("row", 0));
        auto& formats = engine.getMedia().getFormats();
        samplingChooser = std::make_unique<juce::FileChooser> ("Load Sample", samplingModuleFolderOrDefault(), formats.getWildcardForAllFormats());
        samplingChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                      [safe = juce::Component::SafePointer<MainComponent> (this), note, row] (const juce::FileChooser& chooser)
        {
            if (safe == nullptr || chooser.getResult() == juce::File())
                return;

            if (! safe->engine.getMedia().isAudioFile (chooser.getResult()))
            {
                safe->samplingProblem (chooser.getResult().getFileName() + " isn't an audio file.");
                return;
            }

            auto item = object();
            item->setProperty ("kind", "builderFiles");
            item->setProperty ("paths", juce::Array<juce::var> { chooser.getResult().getFullPathName() });
            item->setProperty ("note", note);
            item->setProperty ("row", row);
            safe->emitSampling (juce::var (item.get()));
        });
        return;
    }

    if (action == "builderDrag")
    {
        showSamplingDropZone (event);
        return;
    }

    if (action == "builderInfo")
    {
        juce::Array<juce::var> files;

        if (const auto* paths = event.getProperty ("paths", {}).getArray())
        {
            for (const auto& pathVar : *paths)
            {
                const juce::File file (pathVar.toString());
                auto info = object();
                info->setProperty ("path", file.getFullPathName());
                info->setProperty ("name", file.getFileName());
                std::unique_ptr<juce::AudioFormatReader> reader (engine.getMedia().getFormats().createReaderFor (file));
                info->setProperty ("ok", reader != nullptr && reader->sampleRate > 0.0);

                if (reader != nullptr && reader->sampleRate > 0.0)
                {
                    info->setProperty ("seconds", static_cast<double> (reader->lengthInSamples) / reader->sampleRate);
                    info->setProperty ("channels", static_cast<int> (reader->numChannels));
                    info->setProperty ("rate", reader->sampleRate);
                }

                files.add (juce::var (info.get()));
            }
        }

        auto item = object();
        item->setProperty ("kind", "builderInfo");
        item->setProperty ("files", files);
        emitSampling (juce::var (item.get()));
    }
}

//==============================================================================
void MainComponent::auditionSampling (const juce::var& event)
{
    const auto note = juce::jlimit (0, 127, static_cast<int> (event.getProperty ("note", 60)));
    const auto velocity = juce::jlimit (1, 127, static_cast<int> (event.getProperty ("velocity", 100)));
    const auto on = static_cast<bool> (event.getProperty ("on", false));
    const auto settings = Sampling::Settings::fromVar (event.getProperty ("settings", samplingSettings));

    // Not while a session plays the same instrument.
    if (sampling != nullptr && (sampling->getState() == Sampling::Session::State::running || sampling->getState() == Sampling::Session::State::stopping))
        return;

    if (settings.source == Sampling::Source::nodes)
    {
        // Through the nodes: heard once a VSTi's chain reaches the Output.
        if (samplingRack == nullptr || ! samplingRack->isReady())
        {
            if (on)
                samplingProblem ("Wire the Sampling node's MIDI to a VSTi, and its chain to the Output, to hear it.");

            return;
        }

        samplingRack->setAwake (true);
        engine.auditionNote (AudioEngine::rackTrackId, note, velocity, on);
        return;
    }

    if (settings.source == Sampling::Source::hardware)
    {
        if (auditionOutput == nullptr || auditionOutputId != settings.midiOutput)
        {
            auditionOutput = nullptr;

            if (! on)
                return;

            auditionOutput = juce::MidiOutput::openDevice (settings.midiOutput);
            auditionOutputId = settings.midiOutput;

            if (auditionOutput == nullptr)
            {
                samplingProblem ("The MIDI output to the synth can't be opened: another app (or Sonify's MIDI clock) may be using it.");
                return;
            }
        }

        auditionOutput->sendMessageNow (on ? juce::MidiMessage::noteOn (settings.midiChannel, note, static_cast<juce::uint8> (velocity))
                                           : juce::MidiMessage::noteOff (settings.midiChannel, note));
    }
}

//==============================================================================
void MainComponent::startSampling (const juce::var& event)
{
    if (sampling != nullptr)
    {
        const auto state = sampling->getState();

        if (state == Sampling::Session::State::running || state == Sampling::Session::State::stopping
            || state == Sampling::Session::State::stopped || state == Sampling::Session::State::finishing)
        {
            samplingProblem ("A sampling session is under way: finish it, or discard it, first.");
            return;
        }
    }

    auto settings = Sampling::Settings::fromVar (samplingSettings);
    settings.sampleRate = engine.getPlaybackRate();
    settings.songTempo = engine.getTransport().getState().bpm;
    const auto* track = engine.findTrack (settings.trackId);

    if (! engine.isAudioRunning())
    {
        samplingProblem ("The audio device isn't running: choose one in Settings > Audio Setup.");
        return;
    }

    if (engine.isLiveShowOn())
    {
        samplingProblem ("The Live Show is playing: it has the outputs. Stop it, then sample.");
        return;
    }

    if (engine.isMixdownRunning())
    {
        samplingProblem ("A mixdown is being made: sample once it's done.");
        return;
    }

    if (settings.source == Sampling::Source::nodes)
    {
        if (samplingRack == nullptr || ! samplingRack->isReady())
        {
            samplingProblem ("Wire the Sampling node's MIDI to a VSTi, and that VSTi's chain to the Output, first.");
            return;
        }
    }
    else if (track == nullptr)
    {
        samplingProblem ("Choose the track to sample (Source).");
        return;
    }

    if (settings.source != Sampling::Source::nodes)
    {
        if (track->kind != Track::Kind::audio)
        {
            samplingProblem (track->name + " isn't an audio track: the sound comes in through an audio track's input.");
            return;
        }

        if (track->inputLeft.load() < 0)
        {
            samplingProblem (track->name + " has no input: choose its input (where the "
                             + juce::String (settings.source == Sampling::Source::hardware ? "synth" : "mic") + " comes in) first.");
            return;
        }
    }

    if (settings.source == Sampling::Source::hardware)
    {
        bool found = false;

        for (const auto& output : juce::MidiOutput::getAvailableDevices())
            found = found || output.identifier == settings.midiOutput;

        if (! found)
        {
            samplingProblem ("Choose the MIDI output the synth is on (Source).");
            return;
        }

        auditionOutput = nullptr;   // the session opens the port itself
    }

    const auto shots = Sampling::plan (settings);

    if (shots.empty())
    {
        samplingProblem (settings.mode == Sampling::Mode::chord ? "No chord fits: pick chord types (Chord Types...) and a key range wide enough for them (Settings)."
                                                                : "No keys selected. Click keys on the keyboard to pick the ones to sample.");
        return;
    }

    const auto folder = samplingFolder();

    if (! folder.createDirectory())
    {
        samplingProblem ("Sonify can't make the folder " + folder.getFullPathName() + ".");
        return;
    }

    // Files of those names there already: the page asks first.
    if (! static_cast<bool> (event.getProperty ("replace", false)))
    {
        const auto samples = Sampling::samplesFolderFor (settings, folder);
        const auto sfz = Sampling::sfzFileFor (settings, folder);
        int clashes = 0;
        juce::String first;

        if (sfz.existsAsFile())
        {
            ++clashes;
            first = sfz.getFileName();
        }

        for (const auto& shot : shots)
        {
            if (samples.getChildFile (shot.file).existsAsFile())
            {
                if (clashes++ == 0)
                    first = shot.file;
            }
        }

        if (clashes > 0)
        {
            auto item = object();
            item->setProperty ("kind", "exists");
            item->setProperty ("name", first);
            item->setProperty ("count", clashes);
            item->setProperty ("folder", samples.getFullPathName());
            emitSampling (juce::var (item.get()));
            return;
        }
    }

    // The last session (done, or failed) goes first: a failed one takes its takes' folder with
    // it, and that folder may be this one's.
    sampling = nullptr;

    auto session = std::make_unique<Sampling::Session> (engine, settings, folder);
    const auto result = session->start();

    if (result.failed())
    {
        pushSampling();
        samplingProblem (result.getErrorMessage());
        return;
    }

    sampling = std::move (session);
    samplingStatePushed = -1;
    samplingTimer.startTimerHz (12);
    updateSamplingRackAwake();
    pushSampling();
}

void MainComponent::startSampleBuild (const juce::var& event)
{
    if (sampleBuild != nullptr && ! sampleBuild->isDone())
    {
        samplingProblem ("The Sample Builder is still writing the last instrument.");
        return;
    }

    auto settings = Sampling::Settings::fromVar (samplingSettings);
    settings.sampleRate = engine.getPlaybackRate();
    size_t count = 0;

    for (const auto& [note, list] : settings.slots)
        if (settings.keys.count (note) != 0)
            count += list.size();

    if (count == 0)
    {
        samplingProblem ("No sample slots loaded. Use the E button on each key to load files.");
        return;
    }

    const auto folder = samplingFolder();

    if (! folder.createDirectory())
    {
        samplingProblem ("Sonify can't make the folder " + folder.getFullPathName() + ".");
        return;
    }

    if (! static_cast<bool> (event.getProperty ("replace", false)))
    {
        const auto samples = Sampling::samplesFolderFor (settings, folder);
        const auto sfz = Sampling::sfzFileFor (settings, folder);
        int clashes = 0;
        juce::String first;

        if (sfz.existsAsFile())
        {
            ++clashes;
            first = sfz.getFileName();
        }

        for (const auto& [note, list] : settings.slots)
        {
            if (settings.keys.count (note) == 0)
                continue;

            for (const auto& slot : list)
            {
                const auto name = Sfz::safeName (slot.name.isNotEmpty() ? slot.name : slot.file.getFileNameWithoutExtension()) + ".wav";

                if (samples.getChildFile (name).existsAsFile() && clashes++ == 0)
                    first = name;
            }
        }

        if (clashes > 0)
        {
            auto item = object();
            item->setProperty ("kind", "exists");
            item->setProperty ("name", first);
            item->setProperty ("count", clashes);
            item->setProperty ("folder", samples.getFullPathName());
            emitSampling (juce::var (item.get()));
            return;
        }
    }

    sampleBuild = std::make_unique<Sampling::Build> (settings, folder);
    sampleBuildName = settings.family;
    sampleBuildPushed = false;
    sampleBuild->start();
    samplingTimer.startTimerHz (12);
    pushSampling();
}

//==============================================================================
void MainComponent::updateSampling()
{
    bool busy = false;

    if (sampling != nullptr)
    {
        sampling->update();
        const auto state = sampling->getState();

        // A new state: everything again (and what it made, once it's made).
        if (static_cast<int> (state) != samplingStatePushed)
        {
            if (state == Sampling::Session::State::done)
            {
                const auto sfz = sampling->getSfzFile();
                sampledInstrument = sfz.existsAsFile() ? sfz : juce::File();
                sampledFolder = Sampling::samplesFolderFor (sampling->getSettings(), sampling->getFolder());
                sampledName = sampling->getSettings().family;

                if (sampledInstrument.existsAsFile())
                    Samplers::setLatest (sampledInstrument);
            }

            pushSampling();
        }
        else if (state == Sampling::Session::State::running || state == Sampling::Session::State::stopping
                 || state == Sampling::Session::State::finishing)
        {
            pushSampling (true);
        }

        busy = state == Sampling::Session::State::running || state == Sampling::Session::State::stopping
            || state == Sampling::Session::State::finishing;
    }

    updateSamplingRackAwake();

    if (sampleBuild != nullptr)
    {
        if (sampleBuild->isDone())
        {
            if (! sampleBuildPushed)
            {
                sampleBuildPushed = true;

                if (sampleBuild->getProblem().isEmpty())
                {
                    const auto sfz = sampleBuild->getSfzFile();
                    sampledInstrument = sfz.existsAsFile() ? sfz : juce::File();
                    sampledFolder = Sampling::samplesFolderFor (Sampling::Settings::fromVar (samplingSettings), samplingFolder());
                    sampledName = sampleBuildName;

                    if (sampledInstrument.existsAsFile())
                        Samplers::setLatest (sampledInstrument);
                }

                pushSampling();
            }
        }
        else
        {
            busy = true;
            pushSampling (true);
        }
    }

    if (! busy)
        samplingTimer.stopTimer();
}

void MainComponent::pushSampling (bool progressOnly)
{
    if (webView == nullptr)
        return;

    auto item = object();

    const auto addBuild = [this] (juce::DynamicObject& into, bool whole)
    {
        auto build = object();
        build->setProperty ("done", sampleBuild->isDone());
        build->setProperty ("progress", sampleBuild->getProgress());
        build->setProperty ("written", sampleBuild->getWritten());

        if (whole)
        {
            build->setProperty ("problem", sampleBuild->getProblem());
            build->setProperty ("name", sampleBuildName);
            build->setProperty ("sfz", sampleBuild->getSfzFile().getFullPathName());
        }

        into.setProperty ("build", juce::var (build.get()));
    };

    if (progressOnly)
    {
        item->setProperty ("kind", "progress");

        if (sampling != nullptr)
        {
            auto session = object();
            addStatus (*sampling, *session);
            item->setProperty ("session", juce::var (session.get()));
        }

        if (sampleBuild != nullptr)
            addBuild (*item, false);

        emitSampling (juce::var (item.get()));
        return;
    }

    item->setProperty ("kind", "state");
    item->setProperty ("moduleFolder", samplingModuleFolderOrDefault().getFullPathName());
    item->setProperty ("adHoc", samplingAdHoc.isDirectory() ? samplingAdHoc.getFullPathName() : juce::String());
    item->setProperty ("folder", samplingFolder().getFullPathName());
    item->setProperty ("rate", engine.getPlaybackRate());
    item->setProperty ("tempo", engine.getTransport().getState().bpm);
    item->setProperty ("audioRunning", engine.isAudioRunning());
    item->setProperty ("signedIn", server != nullptr && server->isSignedIn());
    item->setProperty ("kept", samplingSettings);

    // The MIDI outputs a synth may be on, and the one MIDI Setup's first instrument sends to.
    juce::Array<juce::var> outputs;
    juce::String synthPort;
    const auto available = juce::MidiOutput::getAvailableDevices();

    for (const auto& output : available)
    {
        auto port = object();
        port->setProperty ("id", output.identifier);
        port->setProperty ("name", output.name);
        outputs.add (juce::var (port.get()));
    }

    for (const auto& device : prefs.devices)
    {
        if (device.kind != "instrument" || device.output.isEmpty() || synthPort.isNotEmpty())
            continue;

        for (const auto& output : available)
            if (output.name == device.output)
                synthPort = output.identifier;
    }

    item->setProperty ("midiOutputs", outputs);
    item->setProperty ("synthPort", synthPort);

    if (sampling != nullptr)
    {
        auto session = object();
        addStatus (*sampling, *session);
        session->setProperty ("settings", sampling->getSettings().toVar());
        session->setProperty ("folder", Sampling::samplesFolderFor (sampling->getSettings(), sampling->getFolder()).getFullPathName());
        session->setProperty ("sfz", sampling->getSfzFile().getFullPathName());

        juce::Array<juce::var> shots;

        for (const auto& shot : sampling->getShots())
        {
            juce::Array<juce::var> notes;

            for (const auto note : shot.notes)
                notes.add (note);

            shots.add (juce::Array<juce::var> { shot.note, shot.velocity, shot.layer, shot.take, shot.bpm, notes, shot.chordSuffix });
        }

        session->setProperty ("shots", shots);
        item->setProperty ("session", juce::var (session.get()));
        samplingStatePushed = static_cast<int> (sampling->getState());
    }
    else
    {
        samplingStatePushed = -1;
    }

    if (sampleBuild != nullptr)
        addBuild (*item, true);

    if (sampledInstrument.existsAsFile() || sampledFolder.isDirectory())
    {
        auto made = object();
        made->setProperty ("sfz", sampledInstrument.existsAsFile() ? sampledInstrument.getFullPathName() : juce::String());
        made->setProperty ("folder", sampledFolder.getFullPathName());
        made->setProperty ("name", sampledName);
        item->setProperty ("made", juce::var (made.get()));
    }

    emitSampling (juce::var (item.get()));
}

//==============================================================================
// MIDI In: the studio in front takes the live MIDI (AudioEngine::setMidiFocus). The song's
// studios share it (the tracks); the Plugins Creator Studio and the Sampling Studio have a MIDI In
// of their own; Sonify AI, the Web Browser and the Guide hear none.
void MainComponent::studioShown (const juce::String& id)
{
    using Focus = AudioEngine::MidiFocus;

    const auto focus = id == "plugins" ? Focus::creator
                     : id == "sampling" ? Focus::sampling
                     : (id == "ai" || id == "web" || id == "guide") ? Focus::none
                     : Focus::song;

    engine.setMidiFocus (focus);

    // The Sampling Studio's MIDI monitor, while it's in front.
    const auto monitoring = focus == Focus::sampling;
    engine.setMidiMonitor (monitoring);

    if (monitoring)
    {
        if (! midiMonitorTimer.isTimerRunning())
            midiMonitorTimer.startTimerHz (20);
    }
    else
    {
        midiMonitorTimer.stopTimer();

        std::vector<AudioEngine::MonitoredMessage> dropped;
        engine.takeMonitored (dropped);
    }
}

void MainComponent::pushMidiMonitor()
{
    std::vector<AudioEngine::MonitoredMessage> taken;
    engine.takeMonitored (taken);

    if (taken.empty() || webView == nullptr)
        return;

    // The page shows the last 20; the rest are counted.
    constexpr size_t shown = 20;
    const auto first = taken.size() > shown ? taken.size() - shown : size_t (0);
    midiMonitorCount += static_cast<int> (first);

    juce::Array<juce::var> messages;

    for (auto i = first; i < taken.size(); ++i)
    {
        const auto& message = taken[i];
        juce::Array<juce::var> bytes;

        for (int b = 0; b < message.size; ++b)
            bytes.add (static_cast<int> (message.bytes[b]));

        auto entry = object();
        entry->setProperty ("n", ++midiMonitorCount);
        entry->setProperty ("time", message.time);
        entry->setProperty ("source", message.source);
        entry->setProperty ("bytes", bytes);
        entry->setProperty ("sysex", message.sysexLength);
        entry->setProperty ("heard", message.heard);
        messages.add (juce::var (entry.get()));
    }

    auto item = object();
    item->setProperty ("kind", "midiMonitor");
    item->setProperty ("messages", messages);
    emitSampling (juce::var (item.get()));
}

//==============================================================================
// The nodes (SamplingRack.h)
void MainComponent::makeSamplingRack()
{
    if (samplingRack != nullptr)
        return;

    samplingRack = std::make_unique<SamplingRack> (engine);
    engine.setSamplingRack (samplingRack.get());

    // The plugins kept from last time, loaded again.
    if (samplingRackKept.isObject())
    {
        juce::StringArray problems;
        samplingRack->restore (samplingRackKept, problems);

        if (! problems.isEmpty())
        {
            auto item = object();
            item->setProperty ("kind", "notice");
            item->setProperty ("text", "Some nodes' plugins couldn't be loaded: " + problems.joinIntoString ("; "));
            emitSampling (juce::var (item.get()));
        }
    }
}

void MainComponent::updateSamplingRackAwake()
{
    if (samplingRack == nullptr)
        return;

    const auto running = sampling != nullptr && (sampling->getState() == Sampling::Session::State::running
                                                 || sampling->getState() == Sampling::Session::State::stopping);
    samplingRack->setAwake (samplingShown || running);
}

void MainComponent::pushSamplingRack()
{
    if (webView == nullptr || samplingRack == nullptr)
        return;

    auto item = object();
    item->setProperty ("kind", "rack");
    item->setProperty ("rack", samplingRack->toVar (false));

    juce::Array<juce::var> formats;

    for (const auto& format : Plugin::formatsForThisComputer())
    {
        auto entry = object();
        entry->setProperty ("id", format);
        entry->setProperty ("available", Plugin::isFormatAvailable (format));
        entry->setProperty ("folder", samplingPluginFolderFor (format).getFullPathName());
        entry->setProperty ("defaultFolder", Plugin::defaultFolderFor (format).getFullPathName());
        formats.add (juce::var (entry.get()));
    }

    item->setProperty ("formats", formats);

    // What the MIDI In node can pick.
    juce::Array<juce::var> ports;

    for (const auto& port : engine.getMidiInPorts())
    {
        auto entry = object();
        entry->setProperty ("name", port.name);
        entry->setProperty ("device", port.device);
        ports.add (juce::var (entry.get()));
    }

    item->setProperty ("midiPorts", ports);
    emitSampling (juce::var (item.get()));

    // The plugins' upkeep, while there are any.
    if (samplingRack->hasPlugins())
    {
        if (! samplingRackTimer.isTimerRunning())
            samplingRackTimer.startTimerHz (10);
    }
    else
    {
        samplingRackTimer.stopTimer();
    }
}

juce::File MainComponent::samplingPluginFolderFor (const juce::String& format) const
{
    if (const auto found = samplingPluginFolders.find (format); found != samplingPluginFolders.end() && found->second.isDirectory())
        return found->second;

    return Plugin::defaultFolderFor (format);
}

void MainComponent::keepSamplingPluginFolders()
{
    auto kept = object();

    for (const auto& [format, folder] : samplingPluginFolders)
        kept->setProperty (format, folder.getFullPathName());

    samplingPluginFoldersFile().replaceWithText (juce::JSON::toString (juce::var (kept.get()), true));
}

void MainComponent::browseSamplingPlugins (const juce::String& format, const juce::String& where)
{
    auto item = object();
    item->setProperty ("kind", "rackBrowse");
    item->setProperty ("format", format);

    juce::Array<juce::var> entries;
    const auto describe = [&entries] (const juce::File& file, const juce::String& kind, const juce::String& name)
    {
        auto entry = object();
        entry->setProperty ("name", name);
        entry->setProperty ("path", file.getFullPathName());
        entry->setProperty ("kind", kind);
        entries.add (juce::var (entry.get()));
    };

    // The computer itself: its drives (or its root).
    if (where == "*computer*")
    {
        juce::Array<juce::File> roots;
        juce::File::findFileSystemRoots (roots);

        for (const auto& root : roots)
            describe (root, "folder", root.getFullPathName());

        item->setProperty ("folder", "");
        item->setProperty ("parent", "");
        item->setProperty ("entries", entries);
        emitSampling (juce::var (item.get()));
        return;
    }

    juce::File folder (where);

    if (where.isEmpty() || ! juce::File::isAbsolutePath (where) || ! folder.isDirectory())
        folder = samplingPluginFolderFor (format);

    std::vector<juce::File> folders, plugins;

    for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "*", juce::File::findFilesAndDirectories | juce::File::ignoreHiddenFiles))
    {
        const auto file = entry.getFile();

        if (Plugin::looksLikePlugin (file, format))
            plugins.push_back (file);
        else if (entry.isDirectory())
            folders.push_back (file);
    }

    const auto byName = [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; };
    std::sort (folders.begin(), folders.end(), byName);
    std::sort (plugins.begin(), plugins.end(), byName);

    for (const auto& file : folders)
        describe (file, "folder", file.getFileName());

    for (const auto& file : plugins)
        describe (file, "plugin", file.getFileName());

    const auto parent = folder.getParentDirectory();
    item->setProperty ("folder", folder.getFullPathName());
    item->setProperty ("parent", parent != folder ? parent.getFullPathName() : juce::String ("*computer*"));
    item->setProperty ("entries", entries);
    emitSampling (juce::var (item.get()));
}

void MainComponent::loadSamplingPlugin (const juce::String& format, const juce::File& file, const juce::String& pluginId, float x, float y)
{
    if (samplingRack == nullptr)
        return;

    juce::String error;
    Plugin::approve (file);   // picked by the user in the Sampling Studio's own folder list
    auto plugin = Plugin::loadFormat (file, format, pluginId, error);

    if (plugin == nullptr)
    {
        samplingProblem (error.isNotEmpty() ? error : "Sonify couldn't load " + file.getFileName() + ".");
        return;
    }

    // Several plugins in the file (a shell, a collection): which one?
    if (pluginId.isEmpty() && plugin->getPluginsInFile().size() > 1)
    {
        juce::Array<juce::var> list;

        for (const auto& described : plugin->getPluginsInFile())
        {
            auto entry = object();
            entry->setProperty ("id", described.id);
            entry->setProperty ("name", described.name);
            entry->setProperty ("vendor", described.vendor);
            list.add (juce::var (entry.get()));
        }

        auto item = object();
        item->setProperty ("kind", "rackChoose");
        item->setProperty ("format", format);
        item->setProperty ("path", file.getFullPathName());
        item->setProperty ("x", x);
        item->setProperty ("y", y);
        item->setProperty ("plugins", list);
        emitSampling (juce::var (item.get()));
        return;
    }

    const auto id = samplingRack->addPlugin (std::move (plugin), format, x, y, error);

    if (id == 0)
    {
        samplingProblem (error);
        return;
    }

    // Next time, that format's window opens where this one came from.
    samplingPluginFolders[format] = file.getParentDirectory();
    keepSamplingPluginFolders();
    keepSampling();
    pushSamplingRack();
}

void MainComponent::handleSamplingRack (const juce::var& event)
{
    makeSamplingRack();
    const auto op = event.getProperty ("op", {}).toString();
    const auto id = static_cast<int> (event.getProperty ("id", 0));

    // The wiring stays as it is while a session plays through it.
    const auto playing = sampling != nullptr && (sampling->getState() == Sampling::Session::State::running
                                                 || sampling->getState() == Sampling::Session::State::stopping);
    const auto notWhilePlaying = [this, playing]
    {
        if (playing)
            samplingProblem ("The nodes stay as they are while sampling: stop first.");

        return playing;
    };

    if (op == "state")
    {
        pushSamplingRack();
        return;
    }

    if (op == "browse")
    {
        const auto format = event.getProperty ("format", "VST3").toString();

        if (! Plugin::isFormatAvailable (format))
        {
            samplingProblem (format == "VST2" ? "VST2 plugins can't be loaded in this Sonify: it was built without Steinberg's VST2 SDK."
                                              : format + " plugins can't be loaded on this computer.");
            return;
        }

        browseSamplingPlugins (format, event.getProperty ("folder", {}).toString());
        return;
    }

    if (op == "load")
    {
        if (notWhilePlaying())
            return;

        const juce::File file (event.getProperty ("path", {}).toString());

        if (! juce::File::isAbsolutePath (file.getFullPathName()) || ! file.exists())
        {
            samplingProblem ("That plugin isn't there any more.");
            return;
        }

        loadSamplingPlugin (event.getProperty ("format", "VST3").toString(), file, event.getProperty ("pluginId", {}).toString(),
                            static_cast<float> (static_cast<double> (event.getProperty ("x", -1.0))),
                            static_cast<float> (static_cast<double> (event.getProperty ("y", -1.0))));
        return;
    }

    if (op == "move")
    {
        samplingRack->moveNode (id, static_cast<float> (static_cast<double> (event.getProperty ("x", 0.0))),
                                static_cast<float> (static_cast<double> (event.getProperty ("y", 0.0))));
        return;
    }

    if (op == "connect" || op == "disconnect" || op == "remove")
    {
        if (notWhilePlaying())
        {
            pushSamplingRack();
            return;
        }

        const auto from = static_cast<int> (event.getProperty ("from", 0));
        const auto to = static_cast<int> (event.getProperty ("to", 0));

        // A sound wire's sides (left 0, right 1); none: node to node, both sides.
        const auto sideOf = [&event] (const char* name)
        {
            const auto value = event.getProperty (name, {});
            return value.isVoid() ? SamplingRack::bothSides : juce::jlimit (0, 1, static_cast<int> (value));
        };

        if (op == "connect")
        {
            if (const auto why = samplingRack->connect (from, to, sideOf ("fromSide"), sideOf ("toSide")); why.isNotEmpty())
            {
                auto item = object();
                item->setProperty ("kind", "notice");
                item->setProperty ("text", why);
                emitSampling (juce::var (item.get()));
            }
        }
        else if (op == "disconnect")
        {
            samplingRack->disconnect (from, to, sideOf ("fromSide"), sideOf ("toSide"));
        }
        else
        {
            samplingRack->removeNode (id);
        }

        keepSampling();
        pushSamplingRack();
        return;
    }

    // The MIDI In node's ports (AudioEngine::everyMidiInput: all of them).
    if (op == "midiInputs")
    {
        juce::StringArray ports;

        if (const auto* list = event.getProperty ("inputs", {}).getArray())
            for (const auto& port : *list)
                ports.add (port.toString());

        samplingRack->setMidiInputs (ports);
        keepSampling();
        pushSamplingRack();
        return;
    }

    // The nodes show (the Nodes source): the MIDI In node hears the live notes meanwhile.
    if (op == "visible")
    {
        engine.setRackMidiInOpen (static_cast<bool> (event.getProperty ("on", false)));
        return;
    }

    if (op == "mute" || op == "pass")
    {
        const auto on = static_cast<bool> (event.getProperty ("on", false));

        if (op == "mute")
            samplingRack->setMuted (id, on);
        else
            samplingRack->setPassThrough (id, on);

        pushSamplingRack();
        return;
    }

    if (op == "window")
    {
        juce::String why;

        if (! samplingRack->showWindow (id, why))
        {
            auto item = object();
            item->setProperty ("kind", "notice");
            item->setProperty ("text", why);
            emitSampling (juce::var (item.get()));
        }
    }
}

//==============================================================================
void MainComponent::trySampledInstrument (const juce::File& sfz, const juce::String& name)
{
    // A new instrument track with a Sonify Sampler playing it: one undo step.
    const auto id = engine.addTrack (Track::Kind::instrument);
    addToHistory ("Add Track", new Edits::TrackAdded (engine, id, static_cast<int> (engine.getTracks().size()) - 1));

    int insertId = 0;
    AudioEngine::Displaced displaced;
    const auto nameBefore = Edits::nameOf (engine, id);
    const auto result = engine.loadPlugin (id, Builtins::file(), Samplers::playerId, -1, 0, insertId, &displaced);

    if (result.failed())
    {
        trackMessages[id] = result.getErrorMessage();
    }
    else
    {
        if (auto* sampler = dynamic_cast<SonifySampler*> (engine.getPlugin (id, insertId)))
            sampler->loadInstrument (sfz, true);

        engine.setTrackName (id, name);
        recordPluginLoad (id, insertId, displaced, nameBefore, true);
        library.markUsed (Builtins::file().getFullPathName(), Samplers::playerId);
        pushLibrary();
    }

    engine.selectTrack (id);
    pushTracks();

    auto item = object();
    item->setProperty ("kind", "tried");
    item->setProperty ("trackId", id);
    emitSampling (juce::var (item.get()));
}

void MainComponent::showSamplingDropZone (const juce::var& event)
{
    if (webView == nullptr)
        return;

    const juce::Rectangle<int> area (juce::roundToInt (static_cast<double> (event.getProperty ("left", 0.0))),
                                     juce::roundToInt (static_cast<double> (event.getProperty ("top", 0.0))),
                                     juce::roundToInt (static_cast<double> (event.getProperty ("width", 0.0))),
                                     juce::roundToInt (static_cast<double> (event.getProperty ("height", 0.0))));

    if (area.getWidth() < 40 || area.getHeight() < 16)
        return;

    samplingDropNote = juce::jlimit (0, 127, static_cast<int> (event.getProperty ("note", 60)));
    samplingDropRow = juce::jmax (0, static_cast<int> (event.getProperty ("row", 0)));

    if (samplingDropZone == nullptr)
    {
        auto zone = std::make_unique<PageDropZone> ("Release to drop");

        zone->accepts = [this] (const juce::File& file)
        {
            return engine.getMedia().isAudioFile (file);
        };

        zone->onDrop = [this] (const juce::StringArray& dropped, juce::Point<int>)
        {
            // The first audio file, as Sampler Arena's slot takes it.
            juce::Array<juce::var> paths;

            for (const auto& path : dropped)
            {
                if (engine.getMedia().isAudioFile (juce::File (path)))
                {
                    paths.add (juce::File (path).getFullPathName());
                    break;
                }
            }

            if (paths.isEmpty())
            {
                samplingProblem ("Only audio files go in the slots.");
                return;
            }

            auto item = object();
            item->setProperty ("kind", "builderFiles");
            item->setProperty ("paths", paths);
            item->setProperty ("note", samplingDropNote);
            item->setProperty ("row", samplingDropRow);
            emitSampling (juce::var (item.get()));
        };

        samplingDropZone = std::move (zone);
    }

    static_cast<PageDropZone*> (samplingDropZone.get())->showOver (webView->localAreaToGlobal (area), area.getPosition());
}

//==============================================================================
// The Loop Editor.

int MainComponent::loopCrossfadeSamples() const
{
    return loopEdit.crossfadeOn ? juce::roundToInt (loopEdit.rate * loopEdit.crossfadeMs / 1000.0) : 0;
}

void MainComponent::handleLoopEditor (const juce::String& action, const juce::var& event)
{
    auto& le = loopEdit;
    const auto hasSample = juce::isPositiveAndBelow (le.selected, static_cast<int> (le.patch.regions.size()));

    if (action == "loopLoad")
    {
        engine.stopPreview();
        le.playing = false;
        const auto start = le.folder.isDirectory() ? le.folder : samplingModuleFolderOrDefault();
        samplingChooser = std::make_unique<juce::FileChooser> ("Load SFZ Patch", start, "*.sfz");
        samplingChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                      [safe = juce::Component::SafePointer<MainComponent> (this)] (const juce::FileChooser& chooser)
        {
            if (safe != nullptr && chooser.getResult().existsAsFile())
                safe->openLoopPatch (chooser.getResult());
        });
        return;
    }

    if (action == "loopOpen")
    {
        const juce::File file (event.getProperty ("path", {}).toString());

        if (juce::File::isAbsolutePath (file.getFullPathName()) && file.existsAsFile())
            openLoopPatch (file);

        return;
    }

    if (le.busy && action != "loopPlay" && action != "loopStop" && action != "loopSelect")
    {
        pushLoopEditor (false);
        return;
    }

    if (action == "loopSelect")
    {
        selectLoopSample (static_cast<int> (event.getProperty ("index", -1)));
        return;
    }

    if (action == "loopFind")
    {
        if (! hasSample)
        {
            le.status = "No sample selected";
            pushLoopEditor (false);
            return;
        }

        auto& region = le.patch.regions[static_cast<size_t> (le.selected)];

        if (! region.file.existsAsFile() || le.audio.getNumSamples() == 0)
        {
            le.status = "File not found";
            pushLoopEditor (false);
            return;
        }

        engine.stopPreview();
        le.playing = false;
        le.found = LoopFinder::analyze (le.audio, le.rate, 5, le.crossfadeMs);

        if (! le.found.success)
        {
            le.status = "Analysis: " + le.found.error;
            pushLoopEditor (false);
            return;
        }

        le.candidates = le.found.candidates;
        selectLoopCandidate (0);
        le.status = "Found " + juce::String (static_cast<int> (le.candidates.size())) + " loop candidates";
        pushLoopEditor (false);
        return;
    }

    if (action == "loopCandidate")
    {
        selectLoopCandidate (static_cast<int> (event.getProperty ("index", 0)));
        pushLoopEditor (false);
        return;
    }

    if (action == "loopSet")
    {
        if (! hasSample || le.candidates.empty())
            return;

        auto& region = le.patch.regions[static_cast<size_t> (le.selected)];
        const auto frames = static_cast<juce::int64> (le.audio.getNumSamples());
        auto start = juce::jlimit<juce::int64> (0, juce::jmax<juce::int64> (0, frames - 1), static_cast<juce::int64> (event.getProperty ("start", 0)));
        auto end = juce::jlimit<juce::int64> (0, juce::jmax<juce::int64> (0, frames - 1), static_cast<juce::int64> (event.getProperty ("end", 0)));

        // As Sampler Arena's markers: at least 1024 samples apart.
        if (end < start + 1024)
            end = juce::jmin (frames - 1, start + 1024);

        if (end < start + 1024)
            start = juce::jmax<juce::int64> (0, end - 1024);

        auto& c = le.candidates[static_cast<size_t> (juce::jlimit (0, static_cast<int> (le.candidates.size()) - 1, le.candidate))];
        c.loopStart = start;
        c.loopEnd = end;
        le.score = "Score: manual";
        applyLoopToRegion (region, start, end, loopCrossfadeSamples(), c.score);

        if (le.playing)
            engine.setPreviewLoop (start, end, region.crossfade);

        pushLoopEditor (false);
        return;
    }

    if (action == "loopRemove")
    {
        if (! hasSample)
            return;

        engine.stopPreview();
        le.playing = false;
        auto& region = le.patch.regions[static_cast<size_t> (le.selected)];
        region.looped = false;
        region.loopStart = region.loopEnd = 0;
        region.loopMode.clear();
        region.crossfade = 0;
        region.changed = true;
        Sfz::removeLoopFile (region.file);
        le.candidates.clear();
        le.candidate = -1;
        le.score = "Score: --";
        le.why.erase (le.selected);
        le.status = "Loop removed";
        pushLoopEditor (false);
        return;
    }

    if (action == "loopXfade")
    {
        le.crossfadeMs = juce::jlimit (20, 500, static_cast<int> (event.getProperty ("ms", le.crossfadeMs)));
        le.crossfadeOn = static_cast<bool> (event.getProperty ("on", le.crossfadeOn));

        // The selected sample's loop takes it (Sampler Arena only showed it).
        if (hasSample)
        {
            auto& region = le.patch.regions[static_cast<size_t> (le.selected)];

            if (region.looped)
            {
                region.crossfade = loopCrossfadeSamples();
                region.changed = true;
                Sfz::writeLoopFile (region.file, region.loopStart, region.loopEnd, region.crossfade,
                                    juce::isPositiveAndBelow (le.candidate, static_cast<int> (le.candidates.size())) ? le.candidates[static_cast<size_t> (le.candidate)].score : 0.0f);

                if (le.playing)
                    engine.setPreviewLoop (region.loopStart, region.loopEnd, region.crossfade);
            }
        }

        pushLoopEditor (false);
        return;
    }

    if (action == "loopPlay")
    {
        if (! hasSample || le.audio.getNumSamples() == 0)
            return;

        const auto& region = le.patch.regions[static_cast<size_t> (le.selected)];

        if (! region.looped || region.loopEnd <= region.loopStart)
        {
            le.status = "No loop points set";
            pushLoopEditor (false);
            return;
        }

        if (! engine.previewLoop (region.file, region.loopStart, region.loopEnd, region.crossfade))
        {
            le.status = "Error loading: " + region.file.getFileName();
            pushLoopEditor (false);
            return;
        }

        le.playing = true;
        loopTimer.startTimerHz (30);
        pushLoopEditor (false);
        return;
    }

    if (action == "loopStop")
    {
        engine.stopPreview();
        le.playing = false;
        loopTimer.stopTimer();
        pushLoopEditor (false);
        return;
    }

    if (action == "loopBatch")
    {
        batchFindLoops();
        return;
    }

    if (action == "loopSave")
    {
        if (le.patch.regions.empty())
            return;

        engine.stopPreview();
        le.playing = false;

        if (static_cast<bool> (event.getProperty ("as", false)) || ! le.patch.file.existsAsFile())
        {
            const auto start = le.patch.file.existsAsFile() ? le.patch.file : le.folder.getChildFile ("patch.sfz");
            samplingChooser = std::make_unique<juce::FileChooser> ("Save SFZ Patch As", start, "*.sfz");
            samplingChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                              | juce::FileBrowserComponent::warnAboutOverwriting,
                                          [safe = juce::Component::SafePointer<MainComponent> (this)] (const juce::FileChooser& chooser)
            {
                if (safe == nullptr || chooser.getResult() == juce::File())
                    return;

                const auto result = chooser.getResult();
                safe->saveLoopPatch (result.hasFileExtension ("sfz") ? result : result.withFileExtension ("sfz"));
            });
            return;
        }

        saveLoopPatch (le.patch.file);
        return;
    }

    if (action == "loopAi")
    {
        askLoopAi (static_cast<bool> (event.getProperty ("all", false)));
        return;
    }
}

bool MainComponent::openLoopPatch (const juce::File& sfz)
{
    auto& le = loopEdit;
    engine.stopPreview();
    le.playing = false;
    loopTimer.stopTimer();

    SfzLoops::Patch patch;
    juce::String why;

    if (! SfzLoops::load (sfz, patch, why))
    {
        le.status = why;
        pushLoopEditor (false);
        return false;
    }

    ++le.generation;
    le.busy = false;
    le.patch = std::move (patch);
    le.why.clear();
    le.folder = sfz.getParentDirectory();
    keepSampling();

    le.selected = -1;
    le.audio.setSize (0, 0);
    le.found = {};
    le.candidates.clear();
    le.candidate = -1;
    le.score = "Score: --";
    selectLoopSample (0);
    le.status = juce::String (static_cast<int> (le.patch.regions.size())) + " regions loaded";
    pushLoopEditor (true);
    return true;
}

void MainComponent::selectLoopSample (int index)
{
    auto& le = loopEdit;

    if (! juce::isPositiveAndBelow (index, static_cast<int> (le.patch.regions.size())))
        return;

    engine.stopPreview();
    le.playing = false;
    loopTimer.stopTimer();
    le.selected = index;
    le.audio.setSize (0, 0);
    le.found = {};
    le.candidates.clear();
    le.candidate = -1;
    le.score = "Score: --";

    const auto& region = le.patch.regions[static_cast<size_t> (index)];

    if (! region.file.existsAsFile())
    {
        le.status = "File not found: " + region.sample;
        pushLoopEditor (true);
        return;
    }

    std::unique_ptr<juce::AudioFormatReader> reader (engine.getMedia().getFormats().createReaderFor (region.file));

    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->lengthInSamples > static_cast<juce::int64> (reader->sampleRate * 600.0))
    {
        le.status = "Error loading: " + region.file.getFileName();
        pushLoopEditor (true);
        return;
    }

    const auto channels = static_cast<int> (juce::jlimit (1u, 2u, reader->numChannels));
    le.audio.setSize (channels, static_cast<int> (reader->lengthInSamples));
    reader->read (&le.audio, 0, static_cast<int> (reader->lengthInSamples), 0, true, channels > 1);
    le.rate = reader->sampleRate;

    if (region.looped && region.loopEnd > region.loopStart)
    {
        LoopFinder::Candidate existing;
        existing.loopStart = region.loopStart;
        existing.loopEnd = region.loopEnd;
        existing.crossfadeSamples = region.crossfade;
        existing.score = 1.0f;
        le.candidates.push_back (existing);
        le.candidate = 0;
        le.score = "Score: existing";

        // The XFade control shows its crossfade.
        le.crossfadeOn = region.crossfade > 0;

        if (region.crossfade > 0)
            le.crossfadeMs = juce::jlimit (20, 500, juce::roundToInt (region.crossfade * 1000.0 / le.rate / 10.0) * 10);
    }

    le.status = "Loaded: " + region.file.getFileName() + " (" + juce::String (static_cast<double> (le.audio.getNumSamples()) / le.rate, 2)
              + "s, " + juce::String (channels) + "ch)";

    if (const auto found = le.why.find (index); found != le.why.end())
        le.status << "  Fanan AI: " << found->second;

    pushLoopEditor (true);
}

void MainComponent::applyLoopToRegion (SfzLoops::Region& region, juce::int64 loopStart, juce::int64 loopEnd, int crossfade, float score)
{
    region.looped = true;

    if (region.loopMode != "loop_sustain" && region.loopMode != "loop_continuous")
        region.loopMode = "loop_sustain";

    region.loopStart = loopStart;
    region.loopEnd = loopEnd;
    region.crossfade = juce::jmax (0, crossfade);
    region.changed = true;
    Sfz::writeLoopFile (region.file, loopStart, loopEnd, region.crossfade, score);
}

void MainComponent::selectLoopCandidate (int index)
{
    auto& le = loopEdit;

    if (le.candidates.empty() || ! juce::isPositiveAndBelow (le.selected, static_cast<int> (le.patch.regions.size())))
        return;

    le.candidate = juce::jlimit (0, static_cast<int> (le.candidates.size()) - 1, index);
    const auto& c = le.candidates[static_cast<size_t> (le.candidate)];
    le.score = "Score: " + juce::String (static_cast<int> (c.score * 100.0f)) + "%";

    auto& region = le.patch.regions[static_cast<size_t> (le.selected)];
    applyLoopToRegion (region, c.loopStart, c.loopEnd, loopCrossfadeSamples(), c.score);

    if (le.playing)
        engine.setPreviewLoop (c.loopStart, c.loopEnd, region.crossfade);
}

void MainComponent::saveLoopPatch (const juce::File& target)
{
    auto& le = loopEdit;
    const auto keep = le.selected;
    const auto result = SfzLoops::save (le.patch, target);

    if (result.wasOk())
        le.status = "Saved: " + target.getFileName();
    else
        le.status = result.getErrorMessage();

    le.folder = target.getParentDirectory();
    keepSampling();

    if (juce::isPositiveAndBelow (keep, static_cast<int> (le.patch.regions.size())))
    {
        const auto status = le.status;
        selectLoopSample (keep);
        le.status = status;
    }

    pushLoopEditor (true);
}

void MainComponent::batchFindLoops()
{
    auto& le = loopEdit;

    if (le.patch.regions.empty())
    {
        le.status = "Load an SFZ first";
        pushLoopEditor (false);
        return;
    }

    engine.stopPreview();
    le.playing = false;
    le.busy = true;
    le.status = "Batch: analyzing " + juce::String (static_cast<int> (le.patch.regions.size())) + " samples...";
    pushLoopEditor (false);

    std::vector<juce::File> files;

    for (const auto& region : le.patch.regions)
        files.push_back (region.file);

    const auto crossfadeMs = le.crossfadeMs;
    const auto generation = le.generation;

    loopWorker.addJob ([safe = juce::Component::SafePointer<MainComponent> (this), files, crossfadeMs, generation]
    {
        struct Found { bool missing = false, looped = false; LoopFinder::Candidate c; };
        auto results = std::make_shared<std::vector<Found>> (files.size());

        for (size_t i = 0; i < files.size(); ++i)
        {
            auto& r = (*results)[i];

            if (! files[i].existsAsFile())
            {
                r.missing = true;
                continue;
            }

            const auto found = LoopFinder::analyze (files[i], 5, crossfadeMs);

            if (found.success && ! found.candidates.empty())
            {
                r.looped = true;
                r.c = found.candidates.front();   // the best, level-matched
            }

            if (i % 4 == 3)
            {
                juce::MessageManager::callAsync ([safe, generation, done = static_cast<int> (i + 1), total = static_cast<int> (files.size())]
                {
                    if (safe != nullptr && safe->loopEdit.generation == generation && safe->loopEdit.busy)
                    {
                        safe->loopEdit.status = "Batch: analyzing " + juce::String (done) + " of " + juce::String (total) + "...";
                        safe->pushLoopEditor (false);
                    }
                });
            }
        }

        juce::MessageManager::callAsync ([safe, generation, results]
        {
            if (safe == nullptr || safe->loopEdit.generation != generation)
                return;

            auto& edit = safe->loopEdit;
            int relooped = 0, skipped = 0, missing = 0;

            for (size_t i = 0; i < results->size() && i < edit.patch.regions.size(); ++i)
            {
                const auto& r = (*results)[i];

                if (r.missing)        { ++missing; continue; }
                if (! r.looped)       { ++skipped; continue; }   // a one-shot, or a sound that dies away: left as it was

                safe->applyLoopToRegion (edit.patch.regions[i], r.c.loopStart, r.c.loopEnd, r.c.crossfadeSamples, r.c.score);
                ++relooped;
            }

            edit.busy = false;

            if (juce::isPositiveAndBelow (edit.selected, static_cast<int> (edit.patch.regions.size())))
                safe->selectLoopSample (edit.selected);

            edit.status = "Batch: " + juce::String (relooped) + " re-looped, " + juce::String (skipped) + " skipped (no clean loop), "
                        + juce::String (missing) + " missing. Press Save SFZ to write.";
            safe->pushLoopEditor (true);
        });
    });
}

void MainComponent::askLoopAi (bool all)
{
    auto& le = loopEdit;

    if (le.patch.regions.empty())
    {
        le.status = "Load an SFZ first";
        pushLoopEditor (false);
        return;
    }

    if (server == nullptr || ! server->isSignedIn())
    {
        le.status = "Sign in to your Fanan account first (in Build with AI): Ask Fanan AI uses credits.";
        pushLoopEditor (false);
        return;
    }

    struct Item
    {
        int index = 0;
        juce::File file;
        juce::String name;
        int keycenter = 60, lovel = 0, hivel = 127;
        bool looped = false;
        juce::int64 loopStart = 0, loopEnd = 0;
        int crossfade = 0;
    };

    std::vector<Item> items;

    for (int i = 0; i < static_cast<int> (le.patch.regions.size()); ++i)
    {
        if (! all && i != le.selected)
            continue;

        const auto& r = le.patch.regions[static_cast<size_t> (i)];

        if (r.file.existsAsFile())
            items.push_back ({ i, r.file, r.file.getFileName(), r.keycenter, r.lovel, r.hivel, r.looped, r.loopStart, r.loopEnd, r.crossfade });
    }

    if (items.empty())
    {
        le.status = all ? juce::String ("None of the samples can be found.") : juce::String ("No sample selected (or its file isn't there).");
        pushLoopEditor (false);
        return;
    }

    engine.stopPreview();
    le.playing = false;
    le.busy = true;
    le.status = "Fanan AI: listening to " + juce::String (static_cast<int> (items.size())) + (items.size() == 1 ? " sample..." : " samples...");
    pushLoopEditor (false);

    const auto crossfadeMs = le.crossfadeMs;
    const auto generation = le.generation;

    loopWorker.addJob ([safe = juce::Component::SafePointer<MainComponent> (this), items, crossfadeMs, generation]
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        auto samples = std::make_shared<std::vector<SfzLoops::AiSample>>();

        for (const auto& item : items)
        {
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (item.file));

            if (reader == nullptr || reader->lengthInSamples <= 0 || reader->lengthInSamples > static_cast<juce::int64> (reader->sampleRate * 600.0))
                continue;

            juce::AudioBuffer<float> audio (static_cast<int> (juce::jlimit (1u, 2u, reader->numChannels)), static_cast<int> (reader->lengthInSamples));
            reader->read (&audio, 0, audio.getNumSamples(), 0, true, audio.getNumChannels() > 1);

            SfzLoops::AiSample s;
            s.index = item.index;
            s.name = item.name;
            s.keycenter = item.keycenter;
            s.lovel = item.lovel;
            s.hivel = item.hivel;
            s.channels = audio.getNumChannels();
            s.seconds = static_cast<double> (audio.getNumSamples()) / reader->sampleRate;
            s.found = LoopFinder::analyze (audio, reader->sampleRate, 8, crossfadeMs);
            s.found.sampleRate = reader->sampleRate;

            if (item.looped && item.loopEnd > item.loopStart && item.loopEnd < audio.getNumSamples())
            {
                s.hasCurrent = true;
                s.current = LoopFinder::describe (audio, reader->sampleRate, item.loopStart, item.loopEnd, s.found.periodSamples);
                s.current.crossfadeSamples = item.crossfade;
            }

            samples->push_back (std::move (s));
        }

        juce::MessageManager::callAsync ([safe, generation, samples]
        {
            if (safe == nullptr || safe->loopEdit.generation != generation)
                return;

            if (samples->empty())
            {
                safe->loopEdit.busy = false;
                safe->loopEdit.status = "Fanan AI: the samples couldn't be read.";
                safe->pushLoopEditor (false);
                return;
            }

            safe->sendLoopAi (generation, samples, 0, 0, 0);
        });
    });
}

void MainComponent::sendLoopAi (int generation, std::shared_ptr<std::vector<SfzLoops::AiSample>> samples, size_t from, int picked, int unlooped)
{
    auto& le = loopEdit;
    const auto to = juce::jmin (samples->size(), from + static_cast<size_t> (SfzLoops::aiSamplesPerRequest));
    const std::vector<SfzLoops::AiSample> part (samples->begin() + static_cast<std::ptrdiff_t> (from), samples->begin() + static_cast<std::ptrdiff_t> (to));

    if (samples->size() > static_cast<size_t> (SfzLoops::aiSamplesPerRequest))
        le.status = "Fanan AI: choosing loops (" + juce::String (static_cast<int> (to)) + " of " + juce::String (static_cast<int> (samples->size())) + ")...";
    else
        le.status = "Fanan AI: choosing...";

    pushLoopEditor (false);

    auto text = object();
    text->setProperty ("type", "text");
    text->setProperty ("text", SfzLoops::aiRequest (part));

    auto message = object();
    message->setProperty ("role", "user");
    message->setProperty ("content", juce::Array<juce::var> { juce::var (text.get()) });

    auto body = object();
    body->setProperty ("mode", "pick");   // it picks the best of the loops already found: the quickest model
    body->setProperty ("purpose", "loop_points");
    body->setProperty ("guide", SfzLoops::aiGuide());
    body->setProperty ("context", "Sonify's Sampling Studio, Loop Editor: " + le.patch.file.getFileName());
    body->setProperty ("messages", juce::Array<juce::var> { juce::var (message.get()) });
    body->setProperty ("tools", juce::Array<juce::var>());
    body->setProperty ("request_id", juce::Uuid().toString());

    server->stellaChat (juce::var (body.get()), [safe = juce::Component::SafePointer<MainComponent> (this), generation, samples, from, to, picked, unlooped]
                                                (const juce::Result& result, const juce::var& answer) mutable
    {
        if (safe == nullptr || safe->loopEdit.generation != generation)
            return;

        auto& edit = safe->loopEdit;

        if (result.failed())
        {
            edit.busy = false;
            edit.status = "Fanan AI: " + result.getErrorMessage();
            safe->pushLoopEditor (false);
            return;
        }

        juce::String why;
        const auto picks = SfzLoops::aiPicks (answer, why);

        if (picks.empty())
        {
            edit.busy = false;
            edit.status = why;
            safe->pushLoopEditor (false);
            return;
        }

        for (const auto& pick : picks)
        {
            const auto sample = std::find_if (samples->begin() + static_cast<std::ptrdiff_t> (from), samples->begin() + static_cast<std::ptrdiff_t> (to),
                                              [&pick] (const SfzLoops::AiSample& s) { return s.index == pick.index; });

            if (sample == samples->begin() + static_cast<std::ptrdiff_t> (to) || ! juce::isPositiveAndBelow (pick.index, static_cast<int> (edit.patch.regions.size())))
                continue;

            auto& region = edit.patch.regions[static_cast<size_t> (pick.index)];
            const auto rate = sample->found.sampleRate > 0.0 ? sample->found.sampleRate : 44100.0;
            const auto crossfade = juce::roundToInt (rate * pick.crossfadeMs / 1000.0);

            if (pick.candidate >= 1 && pick.candidate <= static_cast<int> (sample->found.candidates.size()))
            {
                const auto& c = sample->found.candidates[static_cast<size_t> (pick.candidate - 1)];
                safe->applyLoopToRegion (region, c.loopStart, c.loopEnd, crossfade, c.score);
                ++picked;
            }
            else if (pick.candidate == 0 && sample->hasCurrent)
            {
                safe->applyLoopToRegion (region, region.loopStart, region.loopEnd, crossfade, sample->current.score);
                ++picked;
            }
            else if (pick.candidate < 0)
            {
                region.looped = false;
                region.loopStart = region.loopEnd = 0;
                region.loopMode.clear();
                region.crossfade = 0;
                region.changed = true;
                Sfz::removeLoopFile (region.file);
                ++unlooped;
            }
            else
            {
                continue;
            }

            edit.why[pick.index] = pick.why;

            // The selected sample: its candidates are the ones Fanan AI chose from.
            if (pick.index == edit.selected)
            {
                edit.found = sample->found;
                edit.candidates = sample->found.candidates;
                edit.candidate = pick.candidate - 1;

                if (pick.candidate == 0 && sample->hasCurrent)
                {
                    edit.candidates.insert (edit.candidates.begin(), sample->current);
                    edit.candidate = 0;
                }

                edit.crossfadeOn = true;
                edit.crossfadeMs = pick.crossfadeMs;
                edit.score = edit.candidate >= 0 ? "Score: Fanan AI" : "Score: --";

                if (edit.candidate < 0)
                    edit.candidates.clear();
            }
        }

        if (to < samples->size())
        {
            safe->sendLoopAi (generation, samples, to, picked, unlooped);
            return;
        }

        edit.busy = false;
        edit.status = "Fanan AI chose loops for " + juce::String (picked) + (picked == 1 ? " sample" : " samples");

        if (unlooped > 0)
            edit.status << " (" << unlooped << " better without a loop)";

        edit.status << ". Press Save to write them.";

        if (const auto found = edit.why.find (edit.selected); found != edit.why.end() && found->second.isNotEmpty())
            edit.status << "  " << found->second;

        safe->pushLoopEditor (true);
    });
}

void MainComponent::pushLoopPosition()
{
    auto& le = loopEdit;

    if (! le.playing)
    {
        loopTimer.stopTimer();
        return;
    }

    const auto position = engine.getPreviewFilePosition();
    const auto file = juce::isPositiveAndBelow (le.selected, static_cast<int> (le.patch.regions.size())) ? le.patch.regions[static_cast<size_t> (le.selected)].file : juce::File();

    // Another preview took over (the Browser's), or it's over.
    if (position < 0 || engine.getPreviewFile() != file.getFullPathName())
    {
        le.playing = false;
        loopTimer.stopTimer();
        pushLoopEditor (false);
        return;
    }

    if (! samplingShown)
        return;

    auto item = object();
    item->setProperty ("kind", "loopPosition");
    item->setProperty ("position", position);
    item->setProperty ("playing", true);
    emitSampling (juce::var (item.get()));
}

void MainComponent::pushLoopEditor (bool withPeaks)
{
    if (webView == nullptr || ! samplingShown)
        return;

    const auto& le = loopEdit;
    auto item = object();
    item->setProperty ("kind", "loop");
    item->setProperty ("file", le.patch.file.getFullPathName());
    item->setProperty ("name", le.patch.file.existsAsFile() ? le.patch.file.getFileName() : juce::String());

    juce::Array<juce::var> regions;

    for (const auto& r : le.patch.regions)
    {
        auto region = object();
        region->setProperty ("name", r.file.getFileName().isNotEmpty() ? r.file.getFileName() : r.sample);
        region->setProperty ("looped", r.looped && r.loopEnd > r.loopStart);
        region->setProperty ("changed", r.changed);
        region->setProperty ("missing", ! r.file.existsAsFile());
        region->setProperty ("key", r.keycenter);
        region->setProperty ("lovel", r.lovel);
        region->setProperty ("hivel", r.hivel);
        regions.add (juce::var (region.get()));
    }

    item->setProperty ("regions", regions);
    item->setProperty ("selected", le.selected);
    item->setProperty ("status", le.status);
    item->setProperty ("score", le.score);
    item->setProperty ("candidate", le.candidate);
    item->setProperty ("candidates", static_cast<int> (le.candidates.size()));
    item->setProperty ("crossfadeMs", le.crossfadeMs);
    item->setProperty ("crossfadeOn", le.crossfadeOn);
    item->setProperty ("playing", le.playing);
    item->setProperty ("busy", le.busy);
    item->setProperty ("signedIn", server != nullptr && server->isSignedIn());

    if (juce::isPositiveAndBelow (le.selected, static_cast<int> (le.patch.regions.size())))
    {
        const auto& r = le.patch.regions[static_cast<size_t> (le.selected)];
        auto sample = object();
        sample->setProperty ("frames", le.audio.getNumSamples());
        sample->setProperty ("rate", le.rate);
        sample->setProperty ("channels", le.audio.getNumChannels());
        sample->setProperty ("looped", r.looped && r.loopEnd > r.loopStart);
        sample->setProperty ("loopStart", r.loopStart);
        sample->setProperty ("loopEnd", r.loopEnd);
        sample->setProperty ("crossfade", r.crossfade);
        sample->setProperty ("crossfadeText", r.crossfade > 0 ? msText (r.crossfade, le.rate) + " ms" : juce::String ("none"));
        sample->setProperty ("sustainStart", le.found.success ? le.found.sustainStart : 0);
        sample->setProperty ("sustainEnd", le.found.success ? le.found.sustainEnd : 0);
        sample->setProperty ("why", le.why.count (le.selected) != 0 ? le.why.at (le.selected) : juce::String());

        if (withPeaks && le.audio.getNumSamples() > 0)
            sample->setProperty ("peaks", SfzLoops::peaks (le.audio, le.rate).getProperty ("peaks", {}));

        item->setProperty ("sample", juce::var (sample.get()));
    }

    item->setProperty ("withPeaks", withPeaks);
    emitSampling (juce::var (item.get()));
}
