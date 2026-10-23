// D:\workspace\Fanan_web_hub\src\MainBlobs.cpp
//
// The Sampling Studio's SFZ to Blob tab (BlobMaker.h): SFZs and their samples packed into blobs,
// the files a sampler made in the Plugins Creator Studio plays from the disk (its sample library:
// PluginLibrary.h). A list, made one after another.
//
// Page events: sampling {action: blob, op:
//   state | add (SFZs, from a file dialog) | addFolder (every SFZ in a folder and its folders) |
//   addLast (the instrument sampled last) | remove {id} | clear | beside {on} (blobs beside each
//   SFZ, or in one folder) | setFolder | samples {value: keep, s16, s24, f32} | protection {kind:
//   none, project (a Plugins Creator Studio project's protection, from a folder dialog), file (a
//   license.soni's, from a file dialog)} | start | stop | reveal {path} (a blob, or its folder, in
//   Explorer)}.
// Sonify sends: sampling {kind: blob, items [{id, name, sfz, blob, state (waiting, working, done,
// failed), progress, message, regions, samples, bytes, locked}], beside, folder, samples,
// protection {kind, source, product, name, vendor, problem}, running}.

#include "MainComponent.h"
#include "SonifySampler.h"
#include "Settings.h"

void MainComponent::handleBlobMaker (const juce::var& event)
{
    if (blobMaker == nullptr)
    {
        blobMaker = std::make_unique<BlobMaker> (SettingsFolder::file ("blob-maker.json"));
        blobMaker->onChange = [this] { pushBlobMaker(); };
    }

    const auto op = event.getProperty ("op", {}).toString();

    if (op == "add" || op == "addFolder")
    {
        const bool folder = op == "addFolder";
        blobChooser = std::make_unique<juce::FileChooser> (folder ? "A folder of SFZs (its folders' too)" : "SFZs to make blobs of",
                                                           samplingFolder(), folder ? "*" : "*.sfz");

        const auto options = juce::FileBrowserComponent::openMode
                         | (folder ? juce::FileBrowserComponent::canSelectDirectories
                                   : juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems);

        blobChooser->launchAsync (options, [safe = juce::Component::SafePointer<MainComponent> (this)] (const juce::FileChooser& chooser)
        {
            if (safe != nullptr && safe->blobMaker != nullptr && ! chooser.getResults().isEmpty())
                safe->blobMaker->add (chooser.getResults());
        });
        return;
    }

    if (op == "addLast")
    {
        if (const auto sfz = Samplers::latest(); sfz.existsAsFile())
            blobMaker->add ({ sfz });
        else
            samplingProblem ("Nothing has been sampled yet: Add SFZ... picks one.");

        return;
    }

    if (op == "remove")
    {
        blobMaker->remove ((int) event.getProperty ("id", 0));
        return;
    }

    if (op == "clear")
    {
        blobMaker->clear();
        return;
    }

    if (op == "beside")
    {
        blobMaker->setBeside ((bool) event.getProperty ("on", true));
        return;
    }

    if (op == "setFolder")
    {
        blobChooser = std::make_unique<juce::FileChooser> ("The folder the blobs go to", samplingFolder());
        blobChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                  [safe = juce::Component::SafePointer<MainComponent> (this)] (const juce::FileChooser& chooser)
        {
            if (safe != nullptr && safe->blobMaker != nullptr && chooser.getResult().isDirectory())
                safe->blobMaker->setFolder (chooser.getResult());
        });
        return;
    }

    if (op == "samples")
    {
        const auto value = event.getProperty ("value", "keep").toString();
        blobMaker->setSamples (value == "s16" ? BlobMaker::Samples::s16 : value == "s24" ? BlobMaker::Samples::s24
                             : value == "f32" ? BlobMaker::Samples::f32 : BlobMaker::Samples::asTheyAre);
        return;
    }

    if (op == "protection")
    {
        const auto kind = event.getProperty ("kind", "none").toString();

        if (kind == "project" || kind == "file")
        {
            const bool project = kind == "project";
            const auto current = blobMaker->getProtectionSource();
            const auto start = project ? (blobMaker->getProtection() == BlobMaker::Protection::project && current.isDirectory()
                                              ? current.getParentDirectory() : ::Settings().getProjectsFolder())
                                       : (current.existsAsFile() ? current : samplingFolder());

            blobChooser = std::make_unique<juce::FileChooser> (project ? "The Plugins Creator Studio project whose protection the blobs get (its folder)"
                                                                       : "The license.soni whose protection the blobs get",
                                                               start, project ? "*" : "*.soni");

            blobChooser->launchAsync (juce::FileBrowserComponent::openMode
                                          | (project ? juce::FileBrowserComponent::canSelectDirectories : juce::FileBrowserComponent::canSelectFiles),
                                      [safe = juce::Component::SafePointer<MainComponent> (this), project] (const juce::FileChooser& chooser)
            {
                if (safe == nullptr || safe->blobMaker == nullptr || chooser.getResult() == juce::File())
                {
                    if (safe != nullptr)
                        safe->pushBlobMaker();   // the page's choice goes back to what it was

                    return;
                }

                const auto problem = safe->blobMaker->setProtection (project ? BlobMaker::Protection::project : BlobMaker::Protection::soniFile,
                                                                     chooser.getResult());

                if (problem.isNotEmpty())
                {
                    safe->samplingProblem (problem);
                    safe->pushBlobMaker();
                }
            });
            return;
        }

        if (const auto problem = blobMaker->setProtection (BlobMaker::Protection::none, {}); problem.isNotEmpty())
        {
            samplingProblem (problem);
            pushBlobMaker();
        }

        return;
    }

    if (op == "start")
    {
        if (const auto problem = blobMaker->start(); problem.isNotEmpty())
            samplingProblem (problem);

        return;
    }

    if (op == "stop")
    {
        blobMaker->stop();
        return;
    }

    if (op == "reveal")
    {
        if (const auto path = event.getProperty ("path", {}).toString(); juce::File::isAbsolutePath (path))
        {
            const juce::File file (path);

            if (file.exists())
                file.revealToUser();
            else if (file.getParentDirectory().isDirectory())
                file.getParentDirectory().revealToUser();
        }

        return;
    }

    pushBlobMaker();   // state
}

void MainComponent::pushBlobMaker()
{
    if (blobMaker != nullptr)
        emitSampling (blobMaker->toVar());
}
