// D:\workspace\Fanan_web_hub\src\LivePreview.cpp

#include "LivePreview.h"
#include "WasmCompiler.h"
#include "WasmElements.h"

#include <map>

//==============================================================================
LivePreview::LivePreview (CreatorAudio& creatorAudio)
    : audio (creatorAudio)
{
    startTimerHz (4);   // notices a plugin that stopped
}

LivePreview::~LivePreview()
{
    stopTimer();
    alive->store (false);
    closing->store (true);
    worker.removeAllJobs (true, 10000);
}

void LivePreview::setState (State newState, const juce::String& newStatus)
{
    state = newState;
    status = newStatus;

    if (onChanged != nullptr)
        onChanged();
}

//==============================================================================
void LivePreview::build (const juce::File& projectFolder, const juce::String& projectId, std::function<void (bool)> done)
{
    const auto thisBuild = ++generation;
    setState (State::building, juce::String::fromUTF8 ("Building\xe2\x80\xa6"));

    const auto rate = audio.getCurrentSampleRate();
    const auto block = audio.getCurrentBlockSize();

    worker.addJob ([this, stillAlive = alive, stopping = closing, thisBuild, projectFolder, projectId, rate, block, done]
    {
        const auto compiled = WasmCompiler::compile (projectFolder, projectId, {}, stopping.get());

        std::vector<std::uint8_t> bytes, elementsBytes;
        auto holder = std::make_shared<std::unique_ptr<WasmPlugin>>();
        std::string error;

        // The GUI's programmed elements: built and started once the sound has built.
        WasmCompiler::Result elementsCompiled;
        std::shared_ptr<WasmElements> elements;
        std::string elementsError;

        auto loadFile = [] (const juce::File& file)
        {
            juce::MemoryBlock data;
            file.loadFileAsData (data);
            return std::vector<std::uint8_t> (static_cast<const std::uint8_t*> (data.getData()),
                                              static_cast<const std::uint8_t*> (data.getData()) + data.getSize());
        };

        if (compiled.ok)
        {
            bytes = loadFile (compiled.wasm);
            *holder = WasmPlugin::load (bytes, rate, block, error);

            elementsCompiled = WasmCompiler::compileElements (projectFolder, projectId, stopping.get());

            if (elementsCompiled.ok && elementsCompiled.wasm != juce::File())
            {
                elementsBytes = loadFile (elementsCompiled.wasm);
                elements = WasmElements::load (elementsBytes, elementsError);
            }
        }

        juce::MessageManager::callAsync ([this, stillAlive, thisBuild, compiled, bytes, elementsBytes, holder, error, rate, done,
                                          elementsCompiled, elements, elementsError]
        {
            if (! stillAlive->load())
                return;

            if (thisBuild != generation)
            {
                if (done != nullptr)
                    done (false);   // a newer build took over

                return;
            }

            const bool hasElements = elementsCompiled.wasm != juce::File();

            if (! compiled.ok)
            {
                log = compiled.log;
                setState (State::failed, "Build failed: see the log");
            }
            else if (! elementsCompiled.ok)
            {
                log = "The GUI's programmed elements didn't build:\n" + elementsCompiled.log;
                setState (State::failed, "The GUI's elements didn't build: see the log");
            }
            else if (hasElements && elements == nullptr)
            {
                log = "The GUI's programmed elements built, but didn't start: " + juce::String (elementsError);
                setState (State::failed, "The GUI's elements didn't start: see the log");
            }
            else if (*holder == nullptr)
            {
                log = (compiled.log.isNotEmpty() ? compiled.log + "\n\n" : juce::String()) + juce::String (error);
                setState (State::failed, "Built, but it didn't start: see the log");
            }
            else
            {
                wasm = bytes;
                elementsWasm = elementsBytes;
                loadedRate = rate;
                log = compiled.log;

                if (elementsCompiled.log.isNotEmpty())
                    log << (log.isNotEmpty() ? "\n\n" : "") << "GUI elements:\n" << elementsCompiled.log;

                install (std::move (*holder));
                elementsProblem.clear();

                if (onElements != nullptr)
                    onElements (elements);

                setState (State::playing, juce::String::fromUTF8 ("Playing \xc2\xb7 built in ")
                                              + juce::String (compiled.seconds + elementsCompiled.seconds, 1) + " s");
            }

            // The sample rate changed while it built (deviceChanged waited for it): the plugin
            // playing now, this build or the last one, restarts at the new rate.
            deviceChanged();

            if (done != nullptr)
                done (state == State::playing);
        });
    });
}

void LivePreview::deviceChanged()
{
    const auto rate = audio.getCurrentSampleRate();

    // While a build runs, it's left alone: it checks the rate when it's done.
    if (wasm.empty() || state == State::building || std::abs (rate - loadedRate) < 1.0)
        return;

    // A build that failed leaves the last one playing, which follows the rate too (one that
    // stopped, crashed or stuck, stays stopped).
    if (state != State::playing)
    {
        const auto* playing = audio.getPlugin();

        if (playing == nullptr || playing->hasFailed())
            return;
    }

    // Same plugin, restarted at the new sample rate; parameters carry over.
    const auto thisBuild = ++generation;
    const auto block = audio.getCurrentBlockSize();
    const auto bytes = wasm;

    worker.addJob ([this, stillAlive = alive, thisBuild, bytes, rate, block]
    {
        auto holder = std::make_shared<std::unique_ptr<WasmPlugin>>();
        std::string error;
        *holder = WasmPlugin::load (bytes, rate, block, error);

        juce::MessageManager::callAsync ([this, stillAlive, thisBuild, holder, error, rate]
        {
            if (! stillAlive->load() || thisBuild != generation)
                return;

            if (*holder == nullptr)
            {
                log = juce::String (error);
                setState (State::failed, "Didn't restart at the new sample rate: see the log");
                return;
            }

            loadedRate = rate;
            install (std::move (*holder));
            setState (state, status);   // playing stays playing; a failed build's news stays
        });
    });
}

void LivePreview::unload()
{
    ++generation;
    audio.setPlugin (nullptr);

    elementsProblem.clear();

    if (onElements != nullptr)
        onElements (nullptr);

    wasm.clear();
    elementsWasm.clear();
    log.clear();
    params.clear();
    modules.clear();
    displays.clear();
    bypassed.clear();
    probeModule.clear();
    ++paramsVersion;
    setState (State::idle, {});
}

//==============================================================================
void LivePreview::install (std::unique_ptr<WasmPlugin> plugin)
{
    // Values the user set carry over to the new version, matched by parameter id.
    std::map<juce::String, float> previous;

    for (const auto& p : params)
        previous[p.id] = getParameterValue (p.index);

    auto newParams = parseParameters (plugin->getDescription());

    for (const auto& p : newParams)
        if (const auto found = previous.find (p.id); found != previous.end())
            plugin->setParameter (p.index, found->second);

    params = newParams;
    ++paramsVersion;

    // The modules and their ports, for the schematic.
    modules.clear();
    const auto json = juce::JSON::parse (juce::String (plugin->getDescription()));

    if (const auto* list = json.getProperty ("modules", {}).getArray())
    {
        for (int i = 0; i < list->size(); ++i)
        {
            const auto& item = list->getReference (i);
            ModuleInfo info;
            info.index = i;
            info.id = item.getProperty ("id", {}).toString();
            info.type = item.getProperty ("type", {}).toString();
            info.name = item.getProperty ("name", {}).toString();

            if (const auto* ins = item.getProperty ("inputs", {}).getArray())
                for (const auto& port : *ins)
                    info.inputs.add (port.toString());

            if (const auto* outs = item.getProperty ("outputs", {}).getArray())
                for (const auto& port : *outs)
                    info.outputs.add (port.toString());

            modules.add (info);
        }
    }

    displays.clear();

    if (const auto* list = json.getProperty ("displays", {}).getArray())
        for (const auto& item : *list)
            displays.add ({ (int) item.getProperty ("index", 0), item.getProperty ("id", {}).toString(),
                            item.getProperty ("module", {}).toString(), item.getProperty ("name", {}).toString() });

    plugin->setLibrary (library);
    audio.setPlugin (std::move (plugin));
    applyBypassAndProbe();
    applyGuiSources();
}

void LivePreview::setLibrary (std::vector<stella::blob::Entry> instruments)
{
    library = std::move (instruments);

    if (auto* plugin = audio.getPlugin())
        plugin->setLibrary (library);
}

int LivePreview::getLibraryState (int instrument) const
{
    const auto* plugin = audio.getPlugin();
    return plugin != nullptr ? plugin->getLibraryState (instrument) : stella::blob::notLoaded;
}

juce::String LivePreview::getLibraryProblem (int instrument) const
{
    const auto* plugin = audio.getPlugin();
    return plugin != nullptr ? juce::String (juce::CharPointer_UTF8 (plugin->getLibraryProblem (instrument).c_str())) : juce::String();
}

bool LivePreview::resolve (const juce::String& source, int& moduleIndex, int& port) const
{
    const auto module = source.upToFirstOccurrenceOf (".", false, false);
    const auto portName = source.fromFirstOccurrenceOf (".", false, false);

    if (module == "plugin")
    {
        // "plugin.out L" is what the plugin plays; "plugin.in L" what it gets.
        moduleIndex = portName.startsWith ("in") ? WasmPlugin::pluginInput : WasmPlugin::pluginOutput;
        port = portName.endsWithChar ('R') ? 1 : 0;
        return true;
    }

    for (const auto& m : modules)
    {
        if (m.id == module && m.outputs.contains (portName))
        {
            moduleIndex = m.index;
            port = m.outputs.indexOf (portName);
            return true;
        }
    }

    return false;
}

void LivePreview::setGuiSources (const juce::StringArray& levels, const juce::StringArray& scopes)
{
    levelSources = levels;
    scopeSources = scopes;
    applyGuiSources();
}

void LivePreview::applyGuiSources()
{
    auto* plugin = audio.getPlugin();
    tappedSources.clear();

    if (plugin == nullptr)
        return;

    std::vector<std::pair<int, int>> taps;

    for (const auto& source : levelSources)
    {
        int moduleIndex = 0, port = 0;

        if (taps.size() < (size_t) WasmPlugin::maxTaps && resolve (source, moduleIndex, port))
        {
            taps.push_back ({ moduleIndex, port });
            tappedSources.add (source);
        }
    }

    plugin->setTaps (taps);

    // Slot 0 is the schematic's probe; the GUI's scopes use 1 to 3.
    for (int slot = 1; slot < WasmPlugin::maxScopes; ++slot)
    {
        int moduleIndex = WasmPlugin::off, port = 0;

        if (slot - 1 < scopeSources.size())
            resolve (scopeSources[slot - 1], moduleIndex, port);

        plugin->setScope (slot, moduleIndex, port);
    }
}

float LivePreview::takeLevel (const juce::String& source, bool rms)
{
    auto* plugin = audio.getPlugin();

    if (plugin == nullptr)
        return 0.0f;

    for (const auto& d : displays)
        if (d.id == source)
            return plugin->getDisplay (d.index);

    const auto tap = tappedSources.indexOf (source);

    if (tap < 0)
        return 0.0f;

    return rms ? plugin->getTapRms (tap) : plugin->takeTapPeak (tap);
}

void LivePreview::readScope (const juce::String& source, float* destination, int numSamples) const
{
    const auto slot = scopeSources.indexOf (source) + 1;

    if (auto* plugin = audio.getPlugin(); plugin != nullptr && slot >= 1 && slot < WasmPlugin::maxScopes)
        plugin->readScope (slot, destination, numSamples);
    else
        juce::FloatVectorOperations::clear (destination, numSamples);
}

void LivePreview::applyBypassAndProbe()
{
    auto* plugin = audio.getPlugin();

    if (plugin == nullptr)
        return;

    // Ids that no longer exist are forgotten.
    juce::StringArray stillThere;

    for (const auto& m : modules)
    {
        const bool off = bypassed.contains (m.id);
        plugin->setBypass (m.index, off);

        if (off)
            stillThere.add (m.id);
    }

    bypassed = stillThere;

    int probeIndex = WasmPlugin::off;

    if (probeModule == "plugin")
        probeIndex = WasmPlugin::pluginInput;
    else
        for (const auto& m : modules)
            if (m.id == probeModule && probePort < m.outputs.size())
                probeIndex = m.index;

    if (probeIndex == WasmPlugin::off)
        probeModule.clear();

    plugin->setScope (0, probeIndex, probePort);
}

void LivePreview::setBypass (const juce::String& moduleId, bool shouldBypass)
{
    bypassed.removeString (moduleId);

    if (shouldBypass)
        bypassed.add (moduleId);

    applyBypassAndProbe();

    if (onChanged != nullptr)
        onChanged();
}

void LivePreview::setProbe (const juce::String& moduleId, int port)
{
    probeModule = moduleId;
    probePort = port;
    applyBypassAndProbe();
}

void LivePreview::readProbe (float* destination, int numSamples) const
{
    if (auto* plugin = audio.getPlugin())
        plugin->readScope (0, destination, numSamples);
    else
        juce::FloatVectorOperations::clear (destination, numSamples);
}

juce::Array<LivePreview::Param> LivePreview::parseParameters (const std::string& description)
{
    juce::Array<Param> result;
    const auto json = juce::JSON::parse (juce::String (description));

    if (const auto* list = json.getProperty ("params", {}).getArray())
    {
        for (const auto& item : *list)
        {
            Param p;
            p.index  = (int) item.getProperty ("index", 0);
            p.id     = item.getProperty ("id", {}).toString();
            p.name   = item.getProperty ("name", {}).toString();
            p.module = item.getProperty ("module", {}).toString();
            p.unit   = item.getProperty ("unit", {}).toString();
            p.min    = (float) (double) item.getProperty ("min", 0.0);
            p.max    = (float) (double) item.getProperty ("max", 1.0);
            p.def    = (float) (double) item.getProperty ("def", 0.0);
            p.skew   = (float) (double) item.getProperty ("skew", 1.0);

            if (p.max > p.min)
                result.add (p);
        }
    }

    return result;
}

float LivePreview::getParameterValue (int index) const
{
    if (auto* plugin = audio.getPlugin())
        return plugin->getParameter (index);

    for (const auto& p : params)
        if (p.index == index)
            return p.def;

    return 0.0f;
}

void LivePreview::setParameterValue (int index, float value)
{
    if (auto* plugin = audio.getPlugin())
        plugin->setParameter (index, value);
}

//==============================================================================
void LivePreview::timerCallback()
{
    if (state != State::playing)
        return;

    if (auto* plugin = audio.getPlugin(); plugin != nullptr && plugin->hasFailed())
    {
        log << (log.isNotEmpty() ? "\n\n" : "") << "Stopped while playing: " << juce::String (plugin->getFailure());
        setState (State::failed, "Stopped: see the log");
    }
}
