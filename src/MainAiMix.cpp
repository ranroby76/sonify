// D:\workspace\Fanan_web_hub\src\MainAiMix.cpp

#include "MainComponent.h"
#include "Skills.h"

//==============================================================================
// AI Mix and AI Master (AiMix.h): Fanan AI's mixing on the Analog Board and mastering in the
// Mastering section, as a suggestion the user compares and keeps or not. The page's window
// (hub-aimix.js) sends "aiMix" events:
//   { action: "shown", shown } | { action: "state" }
//   { action: "start", mode: "mix" | "master", target: { kind: "none" | "genre" | "reference", genre }, note }
//   { action: "chooseReference" } | { action: "clearReference" }
//   { action: "stop" }                          the work stops; what it changed goes back
//   { action: "listen", which: "suggestion" | "before" }   A/B, the original at the suggestion's loudness
//   { action: "apply" } | { action: "discard" }
// Sonify answers with "aiMix" { phase, mode, label, progress, problem, entries (Fanan AI's
// conversation for this suggestion), reply, genres, suggestedMode, reference { name, lufs },
// before / after { lufs, truePeak }, listening, trim, boardMixing }.
//
// The way: the song's board and Mastering settings kept as they were (and the board switched on
// to mix, for AI Mix), Sonify measures the song (rendered as a mixdown: each strip, Main before
// and after the Mastering section) and sends it with the request; Fanan AI moves the knobs and
// measures again (measure_mix) once or twice; then Sonify measures the result. Mixing, as on the
// board: not undo steps. Discard puts everything back as it was.

namespace
{
    juce::var objectWith (std::initializer_list<std::pair<const char*, juce::var>> fields)
    {
        auto* object = new juce::DynamicObject();

        for (const auto& [key, value] : fields)
            object->setProperty (key, value);

        return juce::var (object);
    }

    /** The loudness a report's song plays at: after the Mastering section when it's in, else the mix. */
    double heardLufs (const juce::var& report)
    {
        const auto master = report.getProperty ("master", {});
        const auto value = master.isObject() ? master.getProperty ("lufs", {}) : report.getProperty ("mix", {}).getProperty ("lufs", {});
        return value.isDouble() || value.isInt() ? static_cast<double> (value) : -200.0;
    }

    juce::var heardHeadline (const juce::var& report)
    {
        const auto master = report.getProperty ("master", {});
        return MixAnalysis::headline (master.isObject() ? master : report.getProperty ("mix", {}));
    }
}

//==============================================================================
void MainComponent::setupAiMix()
{
    aiMixTools = std::make_unique<AiMix> (*aiTools, AiMix::Hooks {
        [this] (std::function<void (const juce::var&)> done) { aiMixMeasure (std::move (done)); },
        [this]
        {
            if (aiMixRender.running && ! aiMixRender.analysing)
                engine.cancelMixdown();

            aiMixCancelled->store (true);
        },
        [this] { return aiMixMasterOnly; } });

    aiMixAgent = std::make_unique<AiAgent> (*server, *aiMixTools, AiAgent::Hooks {
        [] (const juce::String&) {},
        [this] (const juce::String&, bool changed) { aiMixRequestEnded (changed); },
        [this]
        {
            // Its moves show on the board and the Mastering window as they come.
            if (boardShown || masteringShown)
                boardDue = true;

            pushAiMix();
        } });

    // Mixing and mastering are judgement, not typing: the strongest model from the first step,
    // instead of starting on the everyday one and stepping up only after eight (AiAgent::getMode).
    aiMixAgent->setMode ("deep");
}

//==============================================================================
void MainComponent::handleAiMix (const juce::var& event)
{
    const auto action = event.getProperty ("action", {}).toString();

    // Another song since: what was going on is over (the old song's settings are gone with it).
    if (aiMixPhase != AiMixPhase::idle && aiMixGeneration != songGeneration)
    {
        aiMixAgent->stop();
        aiMixPhase = AiMixPhase::idle;
        engine.setListenTrim (0.0f);
    }

    if (action == "shown")
    {
        aiMixShown = static_cast<bool> (event.getProperty ("shown", false));
        pushAiMix();
        return;
    }

    if (action == "start")               { startAiMix (event);  return; }
    if (action == "stop")                { aiMixStop();         return; }
    if (action == "apply")               { aiMixEnd (true);     return; }
    if (action == "discard")             { aiMixEnd (false);    return; }
    if (action == "chooseReference")     { chooseAiMixReference(); return; }

    if (action == "clearReference")
    {
        aiMixReference = juce::File();
        aiMixReferenceReport = juce::var();
        pushAiMix();
        return;
    }

    if (action == "listen")
    {
        aiMixListen (event.getProperty ("which", "suggestion").toString() != "before");
        return;
    }

    pushAiMix();
}

void MainComponent::pushAiMix()
{
    if (webView == nullptr)
        return;

    aiMixPushedMs = juce::Time::getMillisecondCounterHiRes();
    const auto phase = aiMixPhase == AiMixPhase::measuring ? "measuring" : aiMixPhase == AiMixPhase::working ? "working"
                     : aiMixPhase == AiMixPhase::checking  ? "checking"  : aiMixPhase == AiMixPhase::suggestion ? "suggestion" : "idle";

    auto state = objectWith ({ { "phase", phase }, { "mode", aiMixMasterOnly ? "master" : "mix" }, { "label", aiMixLabel },
                               { "progress", aiMixProgress.load() }, { "problem", aiMixProblem },
                               { "suggestedMode", masteringModeNow() == "masterOnly" ? "master" : "mix" },
                               { "boardMixing", engine.isBoardMixing() }, { "listening", aiMixListeningBefore ? "before" : "suggestion" },
                               { "referenceBusy", aiMixReferenceBusy } });
    auto* fields = state.getDynamicObject();

    juce::Array<juce::var> genres;

    for (const auto& genre : Skills::headings ("mix-genres"))
        genres.add (genre);

    fields->setProperty ("genres", genres);

    if (aiMixReference != juce::File())
        fields->setProperty ("reference", objectWith ({ { "name", aiMixReference.getFileName() },
                                                        { "lufs", aiMixReferenceReport.getProperty ("reference", {}).getProperty ("lufs", {}) },
                                                        { "error", aiMixReferenceReport.getProperty ("reference", {}).getProperty ("error", {}) } }));

    if (aiMixBeforeReport.isObject())
        fields->setProperty ("before", heardHeadline (aiMixBeforeReport));

    if (aiMixAfterReport.isObject())
        fields->setProperty ("after", heardHeadline (aiMixAfterReport));

    if (aiMixBeforeReport.isObject() && aiMixAfterReport.isObject())
        fields->setProperty ("trim", std::round ((heardLufs (aiMixAfterReport) - heardLufs (aiMixBeforeReport)) * 10.0) / 10.0);

    // Fanan AI's side: its conversation for this suggestion (the page shows its steps and reply).
    auto agent = aiMixAgent->getState();
    fields->setProperty ("busy", agent.getProperty ("busy", false));
    fields->setProperty ("entries", agent.getProperty ("entries", {}));
    addAiAccount (state);

    webView->emitEventIfBrowserIsVisible ("aiMix", state);
}

//==============================================================================
juce::var MainComponent::boardSnapshot()
{
    juce::DynamicObject::Ptr strips (new juce::DynamicObject());

    for (const auto& track : engine.getTracks())
        if (! track->isMixChannel())
            strips->setProperty (juce::String (track->id), track->board.settings.toVar (true));

    return objectWith ({ { "mixing", engine.isBoardMixing() }, { "master", engine.getBoard().settings.toVar (true) },
                         { "mastering", engine.getMastering().settings.toVar (true) }, { "strips", juce::var (strips.get()) } });
}

void MainComponent::restoreBoardSnapshot (const juce::var& snapshot)
{
    if (! snapshot.isObject())
        return;

    engine.getBoard().settings.fromVar (snapshot.getProperty ("master", {}), true);
    engine.getMastering().settings.fromVar (snapshot.getProperty ("mastering", {}), true);

    if (const auto* strips = snapshot.getProperty ("strips", {}).getDynamicObject())
        for (const auto& [id, values] : strips->getProperties())
            if (auto* track = engine.findTrack (id.toString().getIntValue()); track != nullptr && ! track->isMixChannel())
                track->board.settings.fromVar (values, true);

    // Which mixer mixes: not while a mixdown runs (one never does here: the work waits for its renders).
    if (const bool mixing = snapshot.getProperty ("mixing", false); mixing != engine.isBoardMixing() && ! engine.isMixdownRunning())
        engine.setBoardMixing (mixing);

    boardDue = true;
    pushTracks();
}

//==============================================================================
void MainComponent::startAiMix (const juce::var& event)
{
    const auto refuse = [this] (const juce::String& why)
    {
        aiMixProblem = why;
        pushAiMix();
    };

    if (aiMixPhase == AiMixPhase::suggestion)
        return refuse ("Apply or discard the suggestion first.");

    if (aiMixPhase != AiMixPhase::idle || aiMixAgent->isBusy())
        return refuse ("Fanan AI is working on one already.");

    if (engine.isMixdownRunning() || masteringStage != MasteringStage::idle || freezingTrackId != 0 || videoExportStage != VideoExportStage::idle)
        return refuse ("Not while a render (an export, a freeze, the Mastering Assistant) is running.");

    if (engine.isRecording() || engine.isTakeActive())
        return refuse ("Not while recording.");

    if (engine.getSongEnd() <= 0.0)
        return refuse ("The song is empty: there's nothing to mix yet.");

    const auto target = event.getProperty ("target", {});
    const auto kind = target.getProperty ("kind", "none").toString();
    const auto genre = target.getProperty ("genre", {}).toString().trim();
    const auto note = event.getProperty ("note", {}).toString().trim().substring (0, 1000);

    if (kind == "reference" && ! aiMixReferenceReport.getProperty ("reference", {}).hasProperty ("lufs"))
        return refuse ("Choose a reference track first (and wait until Sonify has listened to it).");

    if (kind == "genre" && genre.isEmpty())
        return refuse ("Pick a genre first.");

    aiMixMasterOnly = event.getProperty ("mode", "mix").toString() == "master";
    aiMixProblem.clear();
    aiMixBeforeReport = aiMixAfterReport = juce::var();
    aiMixAfter = juce::var();
    aiMixListeningBefore = false;
    engine.setListenTrim (0.0f);
    aiMixGeneration = songGeneration;

    // As it was, for Discard; then the board takes the song (AI Mix), starting at the Console's levels.
    aiMixBefore = boardSnapshot();

    if (! aiMixMasterOnly && ! engine.isBoardMixing())
    {
        if (engine.isBoardFresh())
            engine.copyConsoleToBoard();

        engine.setBoardMixing (true);
        pushTracks();
    }

    juce::String targetText = kind == "genre"     ? "the genre \"" + genre + "\" (read the mix-genres skill for its profile)"
                            : kind == "reference" ? "the reference track \"" + aiMixReference.getFileName() + "\" (its numbers come with every measurement, as reference)"
                                                  : "none: a clean, balanced, modern sound at -14 LUFS";

    aiMixLabel = (aiMixMasterOnly ? juce::String ("AI Master Suggestion") : juce::String ("AI Mix Suggestion"))
               + (kind == "genre" ? " · " + genre : kind == "reference" ? " · like " + aiMixReference.getFileNameWithoutExtension() : juce::String());

    const auto request = aiMixLabel + (note.isNotEmpty() ? ". " + note : juce::String());
    const auto context = juce::String ("Mode: ") + (aiMixMasterOnly ? "Master only (change only the Mastering section)" : "Mix + Master (the board mixes the song now)")
                       + "\nTarget: " + targetText
                       + (note.isNotEmpty() ? "\nThe user's note: " + note : juce::String());

    // Sonify listens first: that measurement goes with the request.
    aiMixPhase = AiMixPhase::measuring;
    pushAiMix();

    aiMixMeasure ([this, request, context, generation = songGeneration] (const juce::var& report)
    {
        if (aiMixPhase != AiMixPhase::measuring || generation != songGeneration)
            return;

        if (report.hasProperty ("error"))
        {
            restoreBoardSnapshot (aiMixBefore);
            aiMixPhase = AiMixPhase::idle;
            aiMixProblem = report.getProperty ("error", {}).toString() == "Cancelled" ? juce::String() : "Couldn't measure the song: " + report.getProperty ("error", {}).toString();
            pushAiMix();
            return;
        }

        aiMixBeforeReport = report;
        aiMixPhase = AiMixPhase::working;
        aiMixAgent->clear();
        aiMixAgent->send (request, AiAgent::Origin::typed, aiMixMasterOnly ? "Mastering" : "Analog Board",
                          context + "\n\nSonify's first measurement (the song before your changes), with the board's settings:\n"
                                  + MixAnalysis::toText (report));

        // Not sent (no sign-in, no credits, no internet): nothing changed, so it's over.
        if (! aiMixAgent->isBusy())
        {
            restoreBoardSnapshot (aiMixBefore);
            aiMixPhase = AiMixPhase::idle;
        }

        pushAiMix();
    });
}

//==============================================================================
void MainComponent::aiMixMeasure (std::function<void (const juce::var&)> done)
{
    if (aiMixRender.running)
        return done (objectWith ({ { "error", "A measurement is under way already." } }));

    if (engine.isMixdownRunning() || masteringStage != MasteringStage::idle || freezingTrackId != 0 || videoExportStage != VideoExportStage::idle)
        return done (objectWith ({ { "error", "Another render is running: try again once it's done." } }));

    const auto end = engine.getSongEnd();

    if (end <= 0.0)
        return done (objectWith ({ { "error", "The song is empty." } }));

    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Sonify AI Mix").getChildFile (juce::Uuid().toString());

    if (! folder.createDirectory())
        return done (objectWith ({ { "error", "Couldn't make a folder to render into." } }));

    AudioEngine::MixdownSettings settings;
    settings.bits = 32;
    settings.startPpq = 0.0;
    settings.endPpq = end;
    settings.tailSeconds = 1.0;
    settings.file = folder.getChildFile ("main.wav");

    std::vector<MixAnalysis::Input> inputs;
    const bool mastered = engine.getMastering().settings.isOn (MasterSection::on);

    if (mastered)
    {
        settings.unmasteredFile = folder.getChildFile ("mix.wav");
        inputs.push_back ({ "mix", "mix", 0, settings.unmasteredFile });
        inputs.push_back ({ "master", "master", 0, settings.file });
    }
    else
    {
        inputs.push_back ({ "mix", "mix", 0, settings.file });
    }

    // Each strip as it leaves its fader (AI Mix, while the board mixes).
    if (! aiMixMasterOnly && engine.isBoardMixing())
    {
        for (const auto& track : engine.getTracks())
        {
            if (track->isMixChannel())
                continue;

            const auto file = folder.getChildFile ("stem " + juce::String (track->id) + ".wav");
            settings.stems.push_back ({ track->id, file });
            inputs.push_back ({ track->name.isNotEmpty() ? track->name : "Track " + juce::String (track->id), "stem", track->id, file });
        }
    }

    if (const auto result = engine.startMixdown (settings); result.failed())
    {
        folder.deleteRecursively();
        return done (objectWith ({ { "error", result.getErrorMessage() } }));
    }

    aiMixCancelled = std::make_shared<std::atomic<bool>> (false);
    aiMixProgress = 0.0;
    aiMixRender = {};
    aiMixRender.running = true;
    aiMixRender.folder = folder;
    aiMixRender.inputs = std::move (inputs);
    aiMixRender.done = std::move (done);
    pushStatus();
}

void MainComponent::updateAiMixRender()
{
    if (aiMixRender.analysing)
    {
        if (aiMixShown && juce::Time::getMillisecondCounterHiRes() - aiMixPushedMs > 250.0)
            pushAiMix();

        return;
    }

    juce::File written;
    const auto result = engine.collectMixdown (written);

    if (! result)
    {
        // Rendering: half the way.
        aiMixProgress = 0.5 * engine.getMixdownProgress();

        if (aiMixShown && juce::Time::getMillisecondCounterHiRes() - aiMixPushedMs > 250.0)
            pushAiMix();

        return;
    }

    if (result->failed())
    {
        auto done = std::move (aiMixRender.done);
        aiMixRender.folder.deleteRecursively();
        aiMixRender = {};
        pushStatus();

        if (done != nullptr)
            done (objectWith ({ { "error", result->getErrorMessage() } }));

        return;
    }

    // Then listened to, on a thread of its own: the other half.
    aiMixRender.analysing = true;
    pushStatus();

    auto inputs = aiMixRender.inputs;

    aiMixWorker.addJob ([inputs, cancelled = aiMixCancelled, safeThis = juce::Component::SafePointer<MainComponent> (this), progress = &aiMixProgress]
    {
        auto report = MixAnalysis::analyse (inputs, *cancelled, [progress] (double fraction) { progress->store (0.5 + 0.5 * fraction); });

        juce::MessageManager::callAsync ([safeThis, report]
        {
            if (safeThis == nullptr)
                return;

            auto& self = *safeThis;
            auto done = std::move (self.aiMixRender.done);
            self.aiMixRender.folder.deleteRecursively();
            self.aiMixRender = {};

            // With the reference's numbers (measured once, when it was chosen), and the board's and
            // the Mastering section's settings now, for the AI.
            if (! report.hasProperty ("error"))
            {
                if (const auto reference = self.aiMixReferenceReport.getProperty ("reference", {}); reference.isObject())
                    report.getDynamicObject()->setProperty ("reference", reference);

                report.getDynamicObject()->setProperty ("board", self.aiTools->run ("get_board", {}));
            }

            if (done != nullptr)
                done (report);
        });
    });
}

//==============================================================================
void MainComponent::aiMixRequestEnded (bool changed)
{
    if (aiMixPhase != AiMixPhase::working)
    {
        pushAiMix();
        return;
    }

    if (aiMixGeneration != songGeneration)
    {
        aiMixPhase = AiMixPhase::idle;
        pushAiMix();
        return;
    }

    // Stopped short, failed, or nothing moved: what it changed goes back, and that's all.
    if (! changed || aiMixAgent->getLastOutcome() == AiAgent::Outcome::problem)
    {
        restoreBoardSnapshot (aiMixBefore);
        aiMixPhase = AiMixPhase::idle;

        if (aiMixProblem.isEmpty() && changed)
            aiMixProblem = "Fanan AI stopped before it finished: everything is back as it was.";

        pushAiMix();
        return;
    }

    // Sonify listens to the result, for the comparison.
    aiMixAfter = boardSnapshot();
    aiMixPhase = AiMixPhase::checking;
    pushAiMix();

    aiMixMeasure ([this, generation = songGeneration] (const juce::var& report)
    {
        if (aiMixPhase != AiMixPhase::checking || generation != songGeneration)
            return;

        if (report.hasProperty ("error"))
        {
            // Not measured: the suggestion stands, without the loudness match.
            aiMixAfterReport = juce::var();

            if (report.getProperty ("error", {}).toString() != "Cancelled")
                aiMixProblem = "The result couldn't be measured, so A/B isn't loudness-matched: " + report.getProperty ("error", {}).toString();
        }
        else
        {
            aiMixAfterReport = report;
        }

        aiMixPhase = AiMixPhase::suggestion;
        aiMixListeningBefore = false;
        boardDue = true;
        pushTracks();
        pushAiMix();
    });
}

void MainComponent::aiMixListen (bool suggestion)
{
    if (aiMixPhase != AiMixPhase::suggestion || suggestion != aiMixListeningBefore)
        return pushAiMix();

    aiMixListeningBefore = ! suggestion;
    restoreBoardSnapshot (suggestion ? aiMixAfter : aiMixBefore);

    // The original, heard at the suggestion's loudness: louder never wins by being louder.
    if (! suggestion && aiMixBeforeReport.isObject() && aiMixAfterReport.isObject())
    {
        const auto before = heardLufs (aiMixBeforeReport), after = heardLufs (aiMixAfterReport);

        if (before > -70.0 && after > -70.0)
            engine.setListenTrim (static_cast<float> (after - before));
    }
    else
    {
        engine.setListenTrim (0.0f);
    }

    pushAiMix();
}

void MainComponent::aiMixEnd (bool apply)
{
    if (aiMixPhase != AiMixPhase::suggestion)
        return pushAiMix();

    restoreBoardSnapshot (apply ? aiMixAfter : aiMixBefore);
    engine.setListenTrim (0.0f);
    aiMixListeningBefore = false;
    aiMixPhase = AiMixPhase::idle;
    aiMixProblem = apply ? juce::String() : "Discarded: everything is back as it was.";
    markChanged();
    pushAiMix();
}

void MainComponent::aiMixStop()
{
    if (aiMixPhase == AiMixPhase::working)
    {
        aiMixAgent->stop();   // its request ends (aiMixRequestEnded puts things back)
        return;
    }

    // Sonify listening (first, or to the result): the render or the listening stops, and its
    // answer ("Cancelled") ends it: everything back as it was, or the suggestion without the
    // loudness match. (Put back only once the render has stopped: the mixer can't switch during one.)
    if (aiMixPhase == AiMixPhase::measuring || aiMixPhase == AiMixPhase::checking)
    {
        if (aiMixRender.running && ! aiMixRender.analysing)
            engine.cancelMixdown();

        aiMixCancelled->store (true);
        aiMixProblem = aiMixPhase == AiMixPhase::checking ? "Stopped before the result was measured: A/B isn't loudness-matched." : juce::String();
    }

    pushAiMix();
}

//==============================================================================
void MainComponent::chooseAiMixReference()
{
    if (choosingFile || aiMixReferenceBusy)
        return;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    choosingFile = true;

    aiMixChooser = std::make_unique<juce::FileChooser> ("Choose a reference track", lastMediaFolder.isDirectory() ? lastMediaFolder
                                                            : juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                        formats.getWildcardForAllFormats());

    aiMixChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                               [safeThis = juce::Component::SafePointer<MainComponent> (this)] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        auto& self = *safeThis;
        self.choosingFile = false;
        const auto file = chooser.getResult();

        if (! file.existsAsFile())
            return;

        // Listened to once now (its numbers go with every measurement).
        self.aiMixReference = file;
        self.aiMixReferenceReport = juce::var();
        self.aiMixReferenceBusy = true;
        self.pushAiMix();

        auto cancelled = std::make_shared<std::atomic<bool>> (false);

        self.aiMixWorker.addJob ([file, cancelled, safe = juce::Component::SafePointer<MainComponent> (&self)]
        {
            const auto report = MixAnalysis::analyse ({ { "reference", "reference", 0, file } }, *cancelled);

            juce::MessageManager::callAsync ([safe, file, report]
            {
                if (safe == nullptr || safe->aiMixReference != file)
                    return;

                safe->aiMixReferenceReport = report;
                safe->aiMixReferenceBusy = false;
                safe->pushAiMix();
            });
        });
    });
}
