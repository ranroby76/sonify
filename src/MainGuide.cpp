// D:\workspace\Fanan_web_hub\src\MainGuide.cpp

#include "MainComponent.h"

#include "BinaryData.h"

//==============================================================================
// The Guide (its own tab, hub-guide.js): Sonify's user guide, sonify_guide.md, which the page
// reads and shows in tabs and chapters, and "Ask anything about Sonify": Fanan AI answering the
// user's questions from that same guide (GuideAi), with the Fanan account's credits.
//
// The page sends "guide" events: { action: "ask", text, reading } (reading: the chapter it has
// open, "Tab > Chapter"), { action: "stop" } and { action: "clear" }. Sonify sends "guideState":
// the conversation, whether a question is under way, and the account (as the AI panels have it).

namespace
{
    /** The guide, as compiled into the program with the page's files. */
    juce::String guideText()
    {
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            if (juce::String (BinaryData::originalFilenames[i]) == "sonify_guide.md")
            {
                int size = 0;

                if (const auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size))
                    return juce::String::fromUTF8 (data, size);
            }
        }

        jassertfalse;   // sonify_guide.md belongs in FananWebHubWeb (CMakeLists.txt)
        return {};
    }
}

void MainComponent::setupGuide()
{
    guideAi = std::make_unique<GuideAi> (*server, guideText());
    guideAi->onChanged = [this] { pushGuide(); };
}

void MainComponent::handleGuide (const juce::var& event)
{
    if (guideAi == nullptr)
        return;

    const auto action = event.getProperty ("action", {}).toString();

    if (action == "ask")
        guideAi->ask (event.getProperty ("text", {}).toString(), event.getProperty ("reading", {}).toString());
    else if (action == "stop")
        guideAi->stop();
    else if (action == "clear")
        guideAi->clear();
    else if (action == "state")
        pushGuide();
}

void MainComponent::pushGuide()
{
    if (webView == nullptr || guideAi == nullptr)
        return;

    auto state = guideAi->getState();
    addAiAccount (state);
    webView->emitEventIfBrowserIsVisible ("guideState", state);
}
