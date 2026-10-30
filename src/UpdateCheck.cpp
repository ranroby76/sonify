// D:\workspace\Fanan_web_hub\src\UpdateCheck.cpp

#include "UpdateCheck.h"
#include "Preferences.h"

namespace
{
    /** Fanan's site's sonifyLatest: the release set in the product manager. */
    constexpr const char* releaseUrl = "https://fanan-team-com.base44.app/functions/sonifyLatest";

    constexpr int firstCheckMs = 8000;               // after Sonify starts, once it has settled
    constexpr int everyMs = 24 * 60 * 60 * 1000;     // then once a day while it stays open
    constexpr int timeoutMs = 10000;

    juce::File skipFile()   { return SettingsFolder::file ("update.json"); }

    juce::String skippedVersion()
    {
        return juce::JSON::parse (skipFile().loadFileAsString()).getProperty ("skipped", {}).toString();
    }

    /** A version's numbers, "0.10.2" as 0, 10, 2; empty when it isn't numbers and dots. */
    juce::Array<int> numbersOf (const juce::String& version)
    {
        juce::Array<int> numbers;
        const auto text = version.trim();

        if (text.isEmpty() || ! text.containsOnly ("0123456789."))
            return numbers;

        for (const auto& part : juce::StringArray::fromTokens (text, ".", {}))
        {
            if (part.isEmpty())
                return {};

            numbers.add (part.getIntValue());
        }

        return numbers;
    }
}

//==============================================================================
UpdateCheck::UpdateCheck (std::function<void (const Release&)> newer)
    : juce::Thread ("Sonify update check"), onNewer (std::move (newer))
{
    startThread (juce::Thread::Priority::low);
}

UpdateCheck::~UpdateCheck()
{
    alive->store (false);
    stopThread (timeoutMs + 2000);
}

juce::String UpdateCheck::currentVersion()
{
   #ifdef JUCE_APPLICATION_VERSION_STRING
    return JUCE_APPLICATION_VERSION_STRING;
   #else
    return "0.0.0";
   #endif
}

bool UpdateCheck::isNewer (const juce::String& theirs, const juce::String& ours)
{
    const auto a = numbersOf (theirs), b = numbersOf (ours);

    if (a.isEmpty() || b.isEmpty())
        return false;

    for (int i = 0; i < juce::jmax (a.size(), b.size()); ++i)
    {
        const auto x = a[i], y = b[i];   // a missing number counts as 0

        if (x != y)
            return x > y;
    }

    return false;
}

void UpdateCheck::skip (const juce::String& version)
{
    if (numbersOf (version).isEmpty())
        return;

    juce::DynamicObject::Ptr kept (new juce::DynamicObject());
    kept->setProperty ("skipped", version.trim());
    skipFile().replaceWithText (juce::JSON::toString (juce::var (kept.get())));
}

juce::String UpdateCheck::sonifyPage()
{
    return "https://fananteam.site/Sonify";
}

juce::String UpdateCheck::safePage (const juce::String& page)
{
    const auto text = page.trim();
    return text.startsWith ("https://fananteam.site/") && ! text.containsAnyOf (" \"'<>\\") ? text : sonifyPage();
}

//==============================================================================
void UpdateCheck::run()
{
    wait (firstCheckMs);   // a stop wakes it at once

    while (! threadShouldExit())
    {
        check();
        wait (everyMs);
    }
}

void UpdateCheck::check()
{
    const juce::URL url (releaseUrl);

    if (! juce::String (releaseUrl).startsWith ("https://"))
        return;

    std::unique_ptr<juce::InputStream> in (url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                                      .withConnectionTimeoutMs (timeoutMs)));

    if (in == nullptr || threadShouldExit())
        return;

    const auto answer = juce::JSON::parse (in->readEntireStreamAsString());

    if (! answer.isObject() || ! static_cast<bool> (answer.getProperty ("ok", true)))
        return;

    Release release;
    release.version = answer.getProperty ("version", {}).toString().trim();

    if (! isNewer (release.version, currentVersion()))
        return;

    release.important = static_cast<bool> (answer.getProperty ("important", false));

    // Skipped by the user: quiet until a later one (an important one is shown all the same).
    if (! release.important && release.version == skippedVersion())
        return;

    release.released = answer.getProperty ("released", {}).toString().trim().substring (0, 20);
    release.page = safePage (answer.getProperty ("page", {}).toString());

    if (const auto* notes = answer.getProperty ("notes", {}).getArray())
        for (const auto& note : *notes)
            if (const auto line = note.toString().trim(); line.isNotEmpty() && release.notes.size() < 12)
                release.notes.add (line.substring (0, 200));

    juce::MessageManager::callAsync ([flag = alive, callback = onNewer, release]
    {
        if (flag->load() && callback != nullptr)
            callback (release);
    });
}
