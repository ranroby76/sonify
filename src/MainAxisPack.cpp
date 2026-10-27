// D:\workspace\Fanan_web_hub\src\MainAxisPack.cpp
//
// The Sampling Studio's AXIS Pack tab (AxisPacker.h): cabinet IRs (.wav, .aiff, .flac) packed
// into one blob (AxisPack.h), the file AXIS plays its IR cabinets from. The same
// list, the same protection and the same way of working as SFZ to Blob beside it (MainBlobs.cpp);
// one pack out of everything listed, instead of a blob of each file.
//
// Page events: sampling {action: axisPack, op:
//   state | addCabs (IR files, from a file dialog) |
//   addFolder (every IR in a folder and its folders) | remove {id} | clear |
//   setPack (where the pack is written, from a save dialog) | irs {value: keep, trim, level} |
//   protection {kind: none, project (a Plugins Creator Studio project's protection, from a folder
//   dialog), file (a license.soni's, from a file dialog)} | start | stop | reveal {path}}.
// Sonify sends: sampling {kind: axisPack, items [{id, name, file, kind (amp, cab), state (waiting,
// working, done, failed), progress, message}], pack (where it's written), made {state (none,
// working, done, failed), progress, message, amps, cabs, bytes, locked}, irs, protection {kind,
// source, product, name, vendor, problem}, running}.

#include "MainComponent.h"
#include "Settings.h"

void MainComponent::handleAxisPacker (const juce::var& event)
{
    if (axisPacker == nullptr)
    {
        axisPacker = std::make_unique<AxisPacker> (SettingsFolder::file ("axis-pack.json"));
        axisPacker->onChange = [this] { pushAxisPacker(); };
    }

    const auto op = event.getProperty ("op", {}).toString();

    if (op == "addCabs" || op == "addFolder")
    {
        const bool folder = op == "addFolder";

        blobChooser = std::make_unique<juce::FileChooser> (folder ? "A folder of cabinet impulse responses (its folders' too)"
                                                                  : "Cabinet impulse responses to pack",
                                                           samplingFolder(),
                                                           folder ? "*" : AxisPack::irWildcards());

        const auto options = juce::FileBrowserComponent::openMode
                         | (folder ? juce::FileBrowserComponent::canSelectDirectories
                                   : juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems);

        blobChooser->launchAsync (options, [safe = juce::Component::SafePointer<MainComponent> (this)] (const juce::FileChooser& chooser)
        {
            if (safe != nullptr && safe->axisPacker != nullptr && ! chooser.getResults().isEmpty())
                safe->axisPacker->add (chooser.getResults());
        });
        return;
    }

    if (op == "remove")
    {
        axisPacker->remove ((int) event.getProperty ("id", 0));
        return;
    }

    if (op == "clear")
    {
        axisPacker->clear();
        return;
    }

    if (op == "setPack")
    {
        const auto now = axisPacker->getPackFile();
        blobChooser = std::make_unique<juce::FileChooser> ("Where the AXIS pack is written", now, "*.blob");

        blobChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [safe = juce::Component::SafePointer<MainComponent> (this)] (const juce::FileChooser& chooser)
        {
            if (safe != nullptr && safe->axisPacker != nullptr && chooser.getResult() != juce::File())
                safe->axisPacker->setPackFile (chooser.getResult());
        });
        return;
    }

    if (op == "irs")
    {
        const auto value = event.getProperty ("value", "level").toString();
        axisPacker->setConditioning (value == "keep" ? AxisPack::Conditioning::asItIs
                                   : value == "trim" ? AxisPack::Conditioning::trimmed
                                                     : AxisPack::Conditioning::levelled);
        return;
    }

    if (op == "protection")
    {
        const auto kind = event.getProperty ("kind", "none").toString();

        if (kind == "project" || kind == "file")
        {
            const bool project = kind == "project";
            const auto current = axisPacker->getProtectionSource();
            const auto start = project ? (axisPacker->getProtection() == AxisPacker::Protection::project && current.isDirectory()
                                              ? current.getParentDirectory() : ::Settings().getProjectsFolder())
                                       : (current.existsAsFile() ? current : samplingFolder());

            blobChooser = std::make_unique<juce::FileChooser> (project ? "The Plugins Creator Studio project whose protection the pack gets (its folder)"
                                                                       : "The license.soni whose protection the pack gets",
                                                               start, project ? "*" : "*.soni");

            blobChooser->launchAsync (juce::FileBrowserComponent::openMode
                                          | (project ? juce::FileBrowserComponent::canSelectDirectories : juce::FileBrowserComponent::canSelectFiles),
                                      [safe = juce::Component::SafePointer<MainComponent> (this), project] (const juce::FileChooser& chooser)
            {
                if (safe == nullptr || safe->axisPacker == nullptr || chooser.getResult() == juce::File())
                {
                    if (safe != nullptr)
                        safe->pushAxisPacker();   // the page's choice goes back to what it was

                    return;
                }

                const auto problem = safe->axisPacker->setProtection (project ? AxisPacker::Protection::project : AxisPacker::Protection::soniFile,
                                                                      chooser.getResult());

                if (problem.isNotEmpty())
                {
                    safe->samplingProblem (problem);
                    safe->pushAxisPacker();
                }
            });
            return;
        }

        if (const auto problem = axisPacker->setProtection (AxisPacker::Protection::none, {}); problem.isNotEmpty())
        {
            samplingProblem (problem);
            pushAxisPacker();
        }

        return;
    }

    if (op == "start")
    {
        if (const auto problem = axisPacker->start(); problem.isNotEmpty())
            samplingProblem (problem);

        return;
    }

    if (op == "stop")
    {
        axisPacker->stop();
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

    pushAxisPacker();   // state
}

void MainComponent::pushAxisPacker()
{
    if (axisPacker != nullptr)
        emitSampling (axisPacker->toVar());
}
