// D:\workspace\Fanan_web_hub\src\MainScore.cpp

#include "MainComponent.h"

//==============================================================================
// The Score's exports (hub-score.js): the page makes the file, Sonify asks where it goes and
// writes it. "score" events:
//   { action: "export", format: "pdf" | "musicxml", name, data }   data: the PDF in base64, or
//                                                                  the MusicXML text
//   { action: "reveal", path }                                     shows a saved file in its folder
// Sonify answers "scoreExported": { ok, name, path } or { ok: false, message }.

void MainComponent::handleScore (const juce::var& event)
{
    const auto action = event.getProperty ("action", {}).toString();

    if (action == "reveal")
    {
        if (const juce::File file (event.getProperty ("path", {}).toString()); file.existsAsFile())
            file.revealToUser();

        return;
    }

    if (action != "export" || choosingFile)
        return;

    const auto pdf = event.getProperty ("format", {}).toString() == "pdf";
    const auto data = event.getProperty ("data", {}).toString();

    if (data.isEmpty())
        return;

    // Next to the song once it's saved; otherwise in a Scores folder of the user data.
    const auto folder = songFile.existsAsFile() ? songFile.getParentDirectory() : UserData::getFolder().getChildFile ("Scores");
    folder.createDirectory();

    const auto extension = juce::String (pdf ? ".pdf" : ".musicxml");
    const auto name = juce::File::createLegalFileName (event.getProperty ("name", "Score").toString().trim().substring (0, 120));
    choosingFile = true;
    scoreChooser = std::make_unique<juce::FileChooser> (pdf ? "Export the score as PDF" : "Export the score as MusicXML",
                                                        folder.getChildFile ((name.isNotEmpty() ? name : juce::String ("Score")) + extension),
                                                        "*" + extension);

    const auto chooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting;

    scoreChooser->launchAsync (chooserFlags, [safeThis = juce::Component::SafePointer<MainComponent> (this), pdf, data, extension] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        safeThis->choosingFile = false;
        auto file = chooser.getResult();

        if (file == juce::File())
            return;

        file = file.withFileExtension (extension);
        bool written = false;

        if (pdf)
        {
            juce::MemoryOutputStream bytes;

            if (juce::Base64::convertFromBase64 (bytes, data) && bytes.getDataSize() > 0)
                written = file.replaceWithData (bytes.getData(), bytes.getDataSize());
        }
        else
        {
            written = file.replaceWithText (data, false, false, "\n");
        }

        juce::DynamicObject::Ptr answer (new juce::DynamicObject());
        answer->setProperty ("ok", written);
        answer->setProperty ("name", file.getFileName());
        answer->setProperty ("path", file.getFullPathName());

        if (! written)
            answer->setProperty ("message", "Couldn't write " + file.getFullPathName());

        if (safeThis->webView != nullptr)
            safeThis->webView->emitEventIfBrowserIsVisible ("scoreExported", juce::var (answer.get()));
    });
}
