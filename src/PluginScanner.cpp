// C:\workspace\Fanan_web_hub\src\PluginScanner.cpp

#include "PluginScanner.h"
#include "Vst3Plugin.h"

PluginScanner::PluginScanner (std::vector<juce::File> filesToRead, int timeout, juce::File workerProgram)
    : juce::Thread ("Fanan plugin scanner"),
      files (std::move (filesToRead)), timeoutSeconds (juce::jmax (1, timeout)), worker (std::move (workerProgram))
{
}

PluginScanner::~PluginScanner()
{
    // A copy still reading is stopped on the way out (run() kills it when asked to stop).
    signalThreadShouldExit();
    stopThread (10000);
}

void PluginScanner::start()
{
    startThread();
}

void PluginScanner::cancel()
{
    cancelled = true;
    signalThreadShouldExit();
}

juce::String PluginScanner::getCurrent() const
{
    const juce::ScopedLock sl (lock);
    return current;
}

std::vector<PluginScanner::Outcome> PluginScanner::takeOutcomes()
{
    const juce::ScopedLock sl (lock);
    std::vector<Outcome> taken;
    taken.swap (outcomes);
    return taken;
}

void PluginScanner::run()
{
    for (const auto& file : files)
    {
        if (threadShouldExit())
            break;

        {
            const juce::ScopedLock sl (lock);
            current = file.getFileName();
        }

        auto outcome = read (file);

        if (threadShouldExit() && outcome.error.isNotEmpty())
            break;   // stopped while reading it: it isn't judged, it's read next time

        {
            const juce::ScopedLock sl (lock);
            outcomes.push_back (std::move (outcome));
        }

        ++done;
    }

    {
        const juce::ScopedLock sl (lock);
        current.clear();
    }

    finished = true;
}

PluginScanner::Outcome PluginScanner::read (const juce::File& file)
{
    Outcome outcome;
    outcome.file = file;

    const auto result = juce::File::createTempFile (".json");
    juce::ChildProcess child;

    if (! child.start (juce::StringArray { worker.getFullPathName(), workerFlag, file.getFullPathName(), result.getFullPathName() }, 0))
    {
        outcome.error = "The scanner couldn't start.";
        return outcome;
    }

    // Waits for the copy, a little at a time, so a cancel or a hung plugin doesn't hold the Hub up.
    const auto deadline = juce::Time::getMillisecondCounter() + static_cast<juce::uint32> (timeoutSeconds) * 1000u;

    while (child.isRunning())
    {
        if (threadShouldExit())
        {
            child.kill();
            result.deleteFile();
            outcome.error = "Cancelled.";
            return outcome;
        }

        if (juce::Time::getMillisecondCounter() > deadline)
        {
            child.kill();
            result.deleteFile();
            outcome.error = "It didn't answer within " + juce::String (timeoutSeconds) + (timeoutSeconds == 1 ? " second" : " seconds")
                          + ", so it was stopped.";
            return outcome;
        }

        wait (20);
    }

    // The answer counts even if the copy crashed on its way out afterwards (some plugins do).
    const auto answer = juce::JSON::parse (result);
    result.deleteFile();

    if (! answer.isObject())
    {
        outcome.error = "It crashed while being read.";
        return outcome;
    }

    if (const auto* list = answer.getProperty ("plugins", {}).getArray())
        for (const auto& item : *list)
            outcome.plugins.push_back (fromVar (item));

    if (outcome.plugins.empty())
        outcome.error = answer.getProperty ("error", "It holds no plugins.").toString();

    return outcome;
}

//==============================================================================
bool PluginScanner::isWorkerCommandLine (const juce::String& commandLine)
{
    return commandLine.contains (workerFlag);
}

int PluginScanner::runWorker (const juce::StringArray& arguments)
{
    const auto at = arguments.indexOf (workerFlag);

    if (at < 0 || at + 2 >= arguments.size())
        return 2;

    const juce::File plugin (arguments[at + 1].unquoted());
    const juce::File output (arguments[at + 2].unquoted());

    // This copy exists only to read this one plugin: if the plugin crashes, only this copy goes.
    juce::String error;
    const auto found = Plugin::describe (plugin, error);

    juce::Array<juce::var> list;

    for (const auto& descriptor : found)
        list.add (toVar (descriptor));

    juce::DynamicObject::Ptr answer (new juce::DynamicObject());
    answer->setProperty ("plugins", list);
    answer->setProperty ("error", error);
    const auto written = output.replaceWithText (juce::JSON::toString (juce::var (answer.get())));

    // The VST3 host is let go properly, as the Hub does on its way out.
    Vst3Plugin::shutdownFormats();
    return written ? 0 : 3;
}

juce::var PluginScanner::toVar (const Plugin::Descriptor& descriptor)
{
    juce::DynamicObject::Ptr item (new juce::DynamicObject());
    item->setProperty ("id", descriptor.id);
    item->setProperty ("name", descriptor.name);
    item->setProperty ("vendor", descriptor.vendor);
    item->setProperty ("version", descriptor.version);

    juce::Array<juce::var> features;

    for (const auto& feature : descriptor.features)
        features.add (feature);

    item->setProperty ("features", features);
    return juce::var (item.get());
}

Plugin::Descriptor PluginScanner::fromVar (const juce::var& value)
{
    Plugin::Descriptor descriptor;
    descriptor.id = value.getProperty ("id", {}).toString();
    descriptor.name = value.getProperty ("name", {}).toString();
    descriptor.vendor = value.getProperty ("vendor", {}).toString();
    descriptor.version = value.getProperty ("version", {}).toString();

    if (const auto* features = value.getProperty ("features", {}).getArray())
        for (const auto& feature : *features)
            descriptor.features.add (feature.toString());

    return descriptor;
}
