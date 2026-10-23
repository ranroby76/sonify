// D:\workspace\Fanan_web_hub\src\WclapPlugin.cpp

#include "WclapPlugin.h"
#include "Preferences.h"

#include "wclap-bridge.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <mutex>
#include <string>

// A native CLAP plugin's own window opens in a window of Sonify's (the app; not the engine tests).
#if JUCE_MODULE_AVAILABLE_juce_gui_basics
 #include <juce_gui_basics/juce_gui_basics.h>
 #define FANAN_CLAP_WINDOWS 1
#else
 #define FANAN_CLAP_WINDOWS 0
#endif

#if JUCE_MODULE_AVAILABLE_juce_events
 #include <juce_events/juce_events.h>
#endif

namespace
{
    /** Whether this is the message thread (CLAP's main thread). */
    bool onMessageThread() noexcept
    {
       #if JUCE_MODULE_AVAILABLE_juce_events
        return juce::MessageManager::existsAndIsCurrentThread();
       #else
        return true;
       #endif
    }

    /** The plugin this thread stands in as the audio thread for, or null. CLAP lets a host mark
        any thread (the main one too) as a plugin's audio thread while no other thread is. */
    thread_local const void* audioThreadFor = nullptr;

    /** Makes the calling thread the plugin's audio thread for as long as it lives. Only while
        no real audio thread is processing that plugin (its owner makes sure). */
    struct StandInAudioThread
    {
        explicit StandInAudioThread (const void* plugin) noexcept : previous (audioThreadFor)   { audioThreadFor = plugin; }
        ~StandInAudioThread() noexcept                                                         { audioThreadFor = previous; }

        const void* previous;

        JUCE_DECLARE_NON_COPYABLE (StandInAudioThread)
    };
}

#ifndef JUCE_APPLICATION_VERSION_STRING
 #define JUCE_APPLICATION_VERSION_STRING "0.0.0"
#endif

namespace
{
    //==============================================================================
    // A single WCLAP call running longer than this is stopped by the engine, so a plugin
    // stuck in a loop can't freeze the audio thread. Wasmtime quotes about 10% speed cost.
    constexpr unsigned int callTimeLimitMs = 1000;

    std::mutex engineMutex;
    bool engineStarted = false;
    std::string compileCacheConfig;

    // Wasmtime keeps compiled plugins in a cache, so reloading one is near-instant.
    // It lives in Sonify's own folder (SettingsFolder: AppData\Fanan\Sonify), where Sonify may
    // always write: not beside the program, which an installed Sonify can't write to (Program
    // Files), and a Mac app mustn't (it would break its signature).
    void prepareCompileCache()
    {
        const auto folder = SettingsFolder::get();
        const auto cacheFolder = folder.getChildFile ("Wasmtime Cache");
        const auto configFile = folder.getChildFile ("wasmtime.toml");

        const auto quotedPath = "\"" + cacheFolder.getFullPathName().replace ("\\", "\\\\").replace ("\"", "\\\"") + "\"";

        if (cacheFolder.createDirectory() && configFile.replaceWithText ("[cache]\ndirectory = " + quotedPath + "\n"))
            compileCacheConfig = configFile.getFullPathName().toStdString();
        else
            compileCacheConfig.clear();   // no cache, rather than none of the plugins playing
    }

    bool startEngine()
    {
        const std::lock_guard<std::mutex> lock (engineMutex);

        if (! engineStarted)
        {
            prepareCompileCache();
            engineStarted = wclap_global_init (callTimeLimitMs);
        }

        return engineStarted;
    }

    //==============================================================================
    // Text coming from a plugin or an archive: stops at the terminator or the buffer end,
    // and never trusts it to be valid UTF-8.
    juce::String fromFixed (const char* text, size_t capacity)
    {
        if (text == nullptr)
            return {};

        size_t length = 0;

        while (length < capacity && text[length] != 0)
            ++length;

        if (juce::CharPointer_UTF8::isValidString (text, static_cast<int> (length)))
            return juce::String::fromUTF8 (text, static_cast<int> (length));

        std::string cleaned (text, length);

        for (auto& c : cleaned)
            if (static_cast<unsigned char> (c) > 127)
                c = '?';

        return juce::String (cleaned);
    }

    juce::String fromPlugin (const char* text)
    {
        return fromFixed (text, 4096);
    }

    bool isAscii (const juce::String& text)
    {
        for (const auto c : text.toStdString())
            if (static_cast<unsigned char> (c) > 127)
                return false;

        return true;
    }

    juce::String makeSafeName (const juce::String& name)
    {
        juce::String result;

        for (const auto c : name.toStdString())
        {
            const bool keep = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                              || c == '.' || c == '-' || c == '_';
            result << (keep ? juce::String::charToString (static_cast<juce::juce_wchar> (c)) : juce::String ("_"));
        }

        return result.isEmpty() ? juce::String ("plugin") : result;
    }

    juce::File getBundleCache()
    {
        // In Sonify's own folder, as the compile cache above (unpacked again when it's not there).
        return SettingsFolder::get().getChildFile ("WCLAP Bundles");
    }

    //==============================================================================
    // Minimal .tar.gz reader for .wclap.tar.gz bundles.
    bool readFully (juce::InputStream& in, void* destination, juce::int64 numBytes)
    {
        auto* bytes = static_cast<char*> (destination);

        while (numBytes > 0)
        {
            const auto chunk = static_cast<int> (juce::jmin<juce::int64> (numBytes, 1 << 20));
            const auto got = in.read (bytes, chunk);

            if (got <= 0)
                return false;

            bytes += got;
            numBytes -= got;
        }

        return true;
    }

    juce::int64 parseOctal (const char* field, int length)
    {
        juce::int64 value = 0;
        int i = 0;

        while (i < length && field[i] == ' ')
            ++i;

        for (; i < length && field[i] >= '0' && field[i] <= '7'; ++i)
            value = value * 8 + (field[i] - '0');

        return value;
    }

    // pax records look like "<length> <key>=<value>\n". Values of other keys can be binary
    // (macOS stores extended attributes there), so only the path record is decoded.
    juce::String pathFromPaxHeader (const juce::MemoryBlock& data)
    {
        const auto* bytes = static_cast<const char*> (data.getData());
        const auto size = data.getSize();
        size_t position = 0;

        while (position < size)
        {
            size_t recordLength = 0;
            auto cursor = position;

            while (cursor < size && bytes[cursor] >= '0' && bytes[cursor] <= '9')
                recordLength = recordLength * 10 + static_cast<size_t> (bytes[cursor++] - '0');

            if (recordLength == 0 || cursor >= size || bytes[cursor] != ' ' || position + recordLength > size)
                break;

            const auto* keyValue = bytes + cursor + 1;
            const auto keyValueLength = position + recordLength - (cursor + 1);   // includes the newline

            if (keyValueLength > 6 && std::memcmp (keyValue, "path=", 5) == 0)
                return fromFixed (keyValue + 5, keyValueLength - 6);

            position += recordLength;
        }

        return {};
    }

    bool isSafeRelativePath (const juce::String& path)
    {
        if (path.isEmpty() || path.startsWithChar ('/') || path.containsChar ('\\') || path.containsChar (':'))
            return false;

        for (const auto& part : juce::StringArray::fromTokens (path, "/", ""))
            if (part == "..")
                return false;

        return true;
    }

    bool isMacMetadata (const juce::String& path)
    {
        for (const auto& part : juce::StringArray::fromTokens (path, "/", ""))
            if (part.startsWith ("._") || part == "__MACOSX" || part == ".DS_Store")
                return true;

        return false;
    }

    bool extractTarGz (const juce::File& archive, const juce::File& destination, juce::String& error)
    {
        constexpr juce::int64 maxTotalBytes = juce::int64 { 512 } * 1024 * 1024;

        juce::FileInputStream fileStream (archive);

        if (! fileStream.openedOk())
        {
            error = "Couldn't open " + archive.getFileName() + ".";
            return false;
        }

        juce::GZIPDecompressorInputStream tar (&fileStream, false, juce::GZIPDecompressorInputStream::gzipFormat);

        destination.deleteRecursively();

        if (! destination.createDirectory())
        {
            error = "Couldn't create " + destination.getFullPathName() + ".";
            return false;
        }

        juce::int64 totalBytes = 0;
        juce::String pendingName;
        int filesWritten = 0;
        char header[512];

        while (readFully (tar, header, 512))
        {
            if (std::all_of (header, header + 512, [] (char c) { return c == 0; }))
                break;   // end-of-archive marker

            const auto size = parseOctal (header + 124, 12);
            const auto type = header[156];
            totalBytes += size;

            if (size < 0 || totalBytes > maxTotalBytes)
            {
                error = archive.getFileName() + " is too large to unpack.";
                return false;
            }

            juce::MemoryBlock content;

            if (size > 0)
            {
                content.setSize (static_cast<size_t> (size));
                char padding[512];
                const auto paddingBytes = (512 - size % 512) % 512;

                if (! readFully (tar, content.getData(), size) || ! readFully (tar, padding, paddingBytes))
                {
                    error = archive.getFileName() + " is damaged or incomplete.";
                    return false;
                }
            }

            if (type == 'L')   // GNU long name for the next entry
            {
                pendingName = fromFixed (static_cast<const char*> (content.getData()), content.getSize());
                continue;
            }

            if (type == 'x')   // pax header: may carry a long path for the next entry
            {
                const auto paxPath = pathFromPaxHeader (content);

                if (paxPath.isNotEmpty())
                    pendingName = paxPath;

                continue;
            }

            if (type == 'g')
                continue;

            auto name = pendingName;
            pendingName.clear();

            if (name.isEmpty())
            {
                name = fromFixed (header, 100);

                if (std::memcmp (header + 257, "ustar", 5) == 0)
                {
                    const auto prefix = fromFixed (header + 345, 155);

                    if (prefix.isNotEmpty())
                        name = prefix + "/" + name;
                }
            }

            while (name.startsWith ("./"))
                name = name.substring (2);

            name = name.trimCharactersAtEnd ("/");

            if (name.isEmpty() || name == ".")
                continue;

            if (! isSafeRelativePath (name))
            {
                error = archive.getFileName() + " contains an unsafe path (" + name + ").";
                return false;
            }

            if (isMacMetadata (name))
                continue;

            const auto target = destination.getChildFile (name);

            if (type == '5')
            {
                target.createDirectory();
                continue;
            }

            if (type != '0' && type != 0)
                continue;   // links and special files are not needed by a plugin bundle

            target.getParentDirectory().createDirectory();

            if (! target.replaceWithData (content.getData(), content.getSize()))
            {
                error = "Couldn't write " + target.getFullPathName() + ".";
                return false;
            }

            ++filesWritten;
        }

        if (filesWritten == 0)
        {
            error = archive.getFileName() + " isn't a .tar.gz bundle.";
            return false;
        }

        return true;
    }

    //==============================================================================
    // The bridge opens files with narrow-character paths, so anything outside plain ASCII
    // is copied into the plugin cache first.
    juce::File ensureAsciiPath (const juce::File& location, juce::String& error)
    {
        if (isAscii (location.getFullPathName()))
            return location;

        const auto copy = getBundleCache().getChildFile (makeSafeName (location.getFileName()));
        copy.deleteRecursively();
        getBundleCache().createDirectory();

        const bool copied = location.isDirectory() ? location.copyDirectoryTo (copy) : location.copyFileTo (copy);

        if (! copied)
        {
            error = "Couldn't copy the plugin to " + copy.getFullPathName() + ".";
            return {};
        }

        return copy;
    }

    juce::File prepareLocation (const juce::File& chosen, juce::String& error)
    {
        const auto fileName = chosen.getFileName();
        const auto lowerName = fileName.toLowerCase();

        if (lowerName.endsWith (".tar.gz") || lowerName.endsWith (".tgz"))
        {
            const auto baseName = fileName.dropLastCharacters (lowerName.endsWith (".tgz") ? 4 : 7);
            const auto destination = getBundleCache().getChildFile (makeSafeName (baseName));

            if (! extractTarGz (chosen, destination, error))
                return {};

            if (destination.getChildFile ("module.wasm").existsAsFile())
                return destination;

            // Some bundles wrap everything in a single folder.
            const auto folders = destination.findChildFiles (juce::File::findDirectories, false);

            if (folders.size() == 1 && folders.getFirst().getChildFile ("module.wasm").existsAsFile())
                return folders.getFirst();

            error = fileName + " has no module.wasm inside.";
            return {};
        }

        if (chosen.isDirectory())
        {
            if (chosen.getChildFile ("module.wasm").existsAsFile())
                return ensureAsciiPath (chosen, error);

            error = fileName + " has no module.wasm inside.";
            return {};
        }

        if (! chosen.existsAsFile())
        {
            error = fileName + " doesn't exist.";
            return {};
        }

        // module.wasm picked from inside a .wclap folder: open the whole folder, so the
        // plugin can reach its own resources.
        if (lowerName == "module.wasm")
            return ensureAsciiPath (chosen.getParentDirectory(), error);

        return ensureAsciiPath (chosen, error);
    }

    //==============================================================================
    clap_event_header_t makeHeader (uint32_t size, int time, uint16_t type) noexcept
    {
        clap_event_header_t header {};
        header.size = size;
        header.time = static_cast<uint32_t> (time);
        header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        header.type = type;
        header.flags = 0;
        return header;
    }
}

//==============================================================================
// Called by the patched wclap-bridge while the engine starts (see WclapBridge.cmake).
extern "C" const char* fanan_wasmtime_cache_config (void);

extern "C" const char* fanan_wasmtime_cache_config (void)
{
    return compileCacheConfig.empty() ? nullptr : compileCacheConfig.c_str();
}

//==============================================================================
// A native CLAP library: opened once however many plugins come from it, clap_entry's init on
// opening and deinit when the last plugin from it goes (CLAP 1.2: in matched pairs). Message thread.
struct WclapPlugin::NativeModule
{
    juce::DynamicLibrary library;
    const clap_plugin_entry_t* entry = nullptr;
    const clap_plugin_factory_t* factory = nullptr;
    juce::String key;

    ~NativeModule()
    {
        if (entry != nullptr)
            entry->deinit();

        entry = nullptr;
        library.close();
    }
};

namespace
{
    std::map<juce::String, std::weak_ptr<WclapPlugin::NativeModule>>& nativeModules()
    {
        static std::map<juce::String, std::weak_ptr<WclapPlugin::NativeModule>> modules;
        return modules;
    }

    /** The library inside a plugin: the file itself, or a macOS bundle's executable. */
    juce::File nativeBinaryOf (const juce::File& file)
    {
        if (! file.isDirectory())
            return file;

        const auto macOS = file.getChildFile ("Contents").getChildFile ("MacOS");
        const auto named = macOS.getChildFile (file.getFileNameWithoutExtension());

        if (named.existsAsFile())
            return named;

        for (const auto& entry : juce::RangedDirectoryIterator (macOS, false, "*", juce::File::findFiles))
            return entry.getFile();

        return {};
    }
}

std::unique_ptr<WclapPlugin> WclapPlugin::loadNative (const juce::File& file, const juce::String& pluginId, juce::String& error)
{
    error.clear();
    const auto key = file.getFullPathName();
    auto module = nativeModules()[key].lock();

    if (module == nullptr)
    {
        const auto binary = nativeBinaryOf (file);

        if (! binary.existsAsFile())
        {
            error = file.getFileName() + " has no CLAP library inside.";
            return nullptr;
        }

        auto opened = std::make_shared<NativeModule>();
        opened->key = key;

        if (! opened->library.open (binary.getFullPathName()))
        {
            error = file.getFileName() + " couldn't be opened as a CLAP plugin (made for another kind of computer?).";
            return nullptr;
        }

        const auto* entry = static_cast<const clap_plugin_entry_t*> (opened->library.getFunction ("clap_entry"));

        if (entry == nullptr || entry->init == nullptr || entry->get_factory == nullptr || ! clap_version_is_compatible (entry->clap_version))
        {
            error = file.getFileName() + " isn't a CLAP plugin Sonify can host.";
            return nullptr;
        }

        // The path CLAP wants: the library itself, or the bundle on macOS.
        if (! entry->init (key.toRawUTF8()))
        {
            error = file.getFileName() + " failed to start.";
            return nullptr;
        }

        opened->entry = entry;
        opened->factory = static_cast<const clap_plugin_factory_t*> (entry->get_factory (CLAP_PLUGIN_FACTORY_ID));

        if (opened->factory == nullptr)
        {
            error = file.getFileName() + " has no CLAP plugin factory.";
            return nullptr;
        }

        module = opened;
        nativeModules()[key] = module;
    }

    std::unique_ptr<WclapPlugin> result (new WclapPlugin());
    result->sourceFile = file;
    result->nativeModule = module;
    result->factory = module->factory;

    if (! result->createFromFactory (pluginId, file.getFileName(), error))
        return nullptr;

    return result;
}

std::unique_ptr<WclapPlugin> WclapPlugin::load (const juce::File& file, const juce::String& pluginId, juce::String& error)
{
    error.clear();

    if (! startEngine())
    {
        error = "The WebAssembly engine couldn't start.";
        return nullptr;
    }

    const auto location = prepareLocation (file, error);

    if (location == juce::File())
        return nullptr;

    std::unique_ptr<WclapPlugin> result (new WclapPlugin());
    result->sourceFile = file;

    if (! result->initialise (location, pluginId, error))
        return nullptr;

    return result;
}

void WclapPlugin::shutdownEngine()
{
    const std::lock_guard<std::mutex> lock (engineMutex);

    if (engineStarted)
    {
        wclap_global_deinit();
        engineStarted = false;
    }
}

WclapPlugin::~WclapPlugin()
{
    destroyNativeScreen();   // its window goes before the plugin it shows

    if (plugin != nullptr)
    {
        if (initialised)
            deactivate();

        plugin->destroy (plugin);
        plugin = nullptr;
    }

    if (module != nullptr)
    {
        wclap_close (module);
        module = nullptr;
    }

    // The library goes once no plugin from it is left.
    if (nativeModule != nullptr)
    {
        const auto key = nativeModule->key;
        nativeModule.reset();

        if (const auto found = nativeModules().find (key); found != nativeModules().end() && found->second.expired())
            nativeModules().erase (found);
    }
}

//==============================================================================
bool WclapPlugin::initialise (const juce::File& location, const juce::String& pluginId, juce::String& error)
{
    const auto displayName = sourceFile.getFileName();

    module = wclap_open (location.getFullPathName().toRawUTF8());

    if (module == nullptr)
    {
        error = displayName + " couldn't be read as a WCLAP.";
        return false;
    }

    juce::String bridgeError;

    if (getError (bridgeError))
    {
        error = displayName + ": " + bridgeError;
        return false;
    }

    factory = static_cast<const clap_plugin_factory_t*> (wclap_get_factory (module, CLAP_PLUGIN_FACTORY_ID));

    if (factory == nullptr)
    {
        error = displayName + " has no CLAP plugin factory.";
        return false;
    }

    return createFromFactory (pluginId, displayName, error);
}

bool WclapPlugin::createFromFactory (const juce::String& pluginId, const juce::String& displayName, juce::String& error)
{
    juce::String bridgeError;
    const clap_plugin_descriptor_t* chosen = nullptr;
    const auto count = factory->get_plugin_count (factory);

    for (uint32_t i = 0; i < count; ++i)
    {
        const auto* desc = factory->get_plugin_descriptor (factory, i);

        if (desc == nullptr || desc->id == nullptr)
            continue;

        Descriptor info;
        info.id = fromPlugin (desc->id);
        info.name = fromPlugin (desc->name != nullptr ? desc->name : desc->id);
        info.vendor = fromPlugin (desc->vendor);
        info.version = fromPlugin (desc->version);

        for (auto* const* feature = desc->features; feature != nullptr && *feature != nullptr; ++feature)
            info.features.add (fromPlugin (*feature));

        pluginsInFile.push_back (info);

        if (chosen == nullptr && (pluginId.isEmpty() || info.id == pluginId))
        {
            chosen = desc;
            descriptor = info;
        }
    }

    if (pluginsInFile.empty())
    {
        error = displayName + " contains no plugins.";
        return false;
    }

    if (chosen == nullptr)
    {
        error = displayName + " doesn't contain " + pluginId + ".";
        return false;
    }

    host.clap_version = CLAP_VERSION;
    host.host_data = this;
    host.name = "Sonify";
    host.vendor = "Fanan";
    host.url = "";
    host.version = JUCE_APPLICATION_VERSION_STRING;
    host.get_extension = hostGetExtension;
    host.request_restart = hostRequestRestart;
    host.request_process = hostRequestProcess;
    host.request_callback = hostRequestCallback;

    plugin = factory->create_plugin (factory, &host, chosen->id);

    if (plugin == nullptr)
    {
        error = descriptor.name + " couldn't be created.";

        if (getError (bridgeError))
            error << " " << bridgeError;

        return false;
    }

    if (! plugin->init (plugin))
    {
        error = descriptor.name + " failed to start.";

        if (getError (bridgeError))
            error << " " << bridgeError;

        return false;
    }

    initialised = true;

    paramsExt = static_cast<const clap_plugin_params_t*> (plugin->get_extension (plugin, CLAP_EXT_PARAMS));
    latencyExt = static_cast<const clap_plugin_latency_t*> (plugin->get_extension (plugin, CLAP_EXT_LATENCY));
    stateExt = static_cast<const clap_plugin_state_t*> (plugin->get_extension (plugin, CLAP_EXT_STATE));
    audioPortsExt = static_cast<const clap_plugin_audio_ports_t*> (plugin->get_extension (plugin, CLAP_EXT_AUDIO_PORTS));
    notePortsExt = static_cast<const clap_plugin_note_ports_t*> (plugin->get_extension (plugin, CLAP_EXT_NOTE_PORTS));
    webviewExt = static_cast<const clap_plugin_webview_t*> (plugin->get_extension (plugin, CLAP_EXT_WEBVIEW));
    guiExt = static_cast<const clap_plugin_gui_t*> (plugin->get_extension (plugin, CLAP_EXT_GUI));

    // A native plugin's window, embedded in one of Sonify's.
   #if FANAN_CLAP_WINDOWS
    #if JUCE_WINDOWS
     static constexpr const char* windowApi = CLAP_WINDOW_API_WIN32;
    #elif JUCE_MAC
     static constexpr const char* windowApi = CLAP_WINDOW_API_COCOA;
    #else
     static constexpr const char* windowApi = CLAP_WINDOW_API_X11;
    #endif

    nativeGui = nativeModule != nullptr && guiExt != nullptr && guiExt->is_api_supported != nullptr && guiExt->create != nullptr
                && guiExt->set_parent != nullptr && guiExt->is_api_supported (plugin, windowApi, false);
   #endif

    events.resize (static_cast<size_t> (maxEvents));
    inputEvents.ctx = this;
    inputEvents.size = inputEventsSize;
    inputEvents.get = inputEventsGet;
    outputEvents.ctx = this;
    outputEvents.try_push = outputEventsTryPush;

    scanPorts();
    rescanParameters();

    return true;
}

void WclapPlugin::scanPorts()
{
    inputPorts.clear();
    outputPorts.clear();
    mainInputPort = -1;
    mainOutputPort = -1;

    if (audioPortsExt != nullptr)
    {
        for (const bool isInput : { true, false })
        {
            auto& ports = isInput ? inputPorts : outputPorts;
            auto& mainPort = isInput ? mainInputPort : mainOutputPort;
            const auto count = juce::jmin (audioPortsExt->count (plugin, isInput), 16u);

            for (uint32_t i = 0; i < count; ++i)
            {
                clap_audio_port_info_t info {};
                Port port;

                if (audioPortsExt->get (plugin, i, isInput, &info))
                    port.channels = static_cast<int> (juce::jmin (info.channel_count, 32u));

                ports.push_back (std::move (port));

                if (mainPort < 0 && (info.flags & CLAP_AUDIO_PORT_IS_MAIN) != 0)
                    mainPort = static_cast<int> (i);
            }

            if (mainPort < 0 && ! ports.empty())
                mainPort = 0;
        }
    }
    else if (! descriptor.features.contains (CLAP_PLUGIN_FEATURE_NOTE_EFFECT))
    {
        // CLAP requires the audio-ports extension, but plugins without it usually still
        // expect a stereo pair.
        Port output;
        output.channels = 2;
        outputPorts.push_back (std::move (output));
        mainOutputPort = 0;

        if (descriptor.features.contains (CLAP_PLUGIN_FEATURE_AUDIO_EFFECT))
        {
            Port input;
            input.channels = 2;
            inputPorts.push_back (std::move (input));
            mainInputPort = 0;
        }
    }

    mainInputChannels = mainInputPort >= 0 ? inputPorts[static_cast<size_t> (mainInputPort)].channels : 0;

    // A second input port is a sidechain (a compressor's key input, say): tracks can send to it.
    sidechainPort = -1;

    for (size_t i = 0; i < inputPorts.size(); ++i)
        if (static_cast<int> (i) != mainInputPort && inputPorts[i].channels > 0 && sidechainPort < 0)
            sidechainPort = static_cast<int> (i);
    mainOutputChannels = mainOutputPort >= 0 ? outputPorts[static_cast<size_t> (mainOutputPort)].channels : 0;

    hasNoteInput = false;
    hasNoteOutput = notePortsExt != nullptr && notePortsExt->count (plugin, false) > 0;

    if (notePortsExt != nullptr && notePortsExt->count (plugin, true) > 0)
    {
        clap_note_port_info_t info {};

        if (notePortsExt->get (plugin, 0, true, &info))
        {
            hasNoteInput = true;
            notePortIndex = 0;
            sendClapNotes = (info.supported_dialects & CLAP_NOTE_DIALECT_CLAP) != 0;
            sendMidi = (info.supported_dialects & CLAP_NOTE_DIALECT_MIDI) != 0;

            if (! sendClapNotes && ! sendMidi)
                sendClapNotes = true;
        }
    }

    // The plugin's own features decide first: some instruments also declare an audio input.
    if (mainOutputChannels == 0)
        role = Role::noAudio;
    else if (descriptor.features.contains (CLAP_PLUGIN_FEATURE_INSTRUMENT) || mainInputChannels == 0)
        role = Role::instrument;
    else
        role = Role::effect;
}

//==============================================================================
bool WclapPlugin::activate (double sampleRate, int maxBlockSize)
{
    if (! initialised || sampleRate <= 0.0)
        return false;

    const auto frames = juce::jmax (maxBlockSize, 256);

    if (active.load() && juce::exactlyEqual (sampleRate, currentSampleRate) && frames <= maxFrames)
        return true;

    deactivate();

    maxFrames = frames;
    allocateBuffers();
    midiOutput.ensureSize (8192);

    if (sidechainPort >= 0)
        prepareSidechain (maxBlockSize);

    if (! plugin->activate (plugin, sampleRate, 1, static_cast<uint32_t> (maxFrames)))
        return false;

    // Its latency as it starts (CLAP: it changes only while the plugin is started again).
    latency.store (latencyExt != nullptr ? static_cast<int> (juce::jmin<uint32_t> (latencyExt->get (plugin), 1u << 20)) : 0);

    currentSampleRate = sampleRate;
    steadyTime = 0;
    active.store (true);
    return true;
}

void WclapPlugin::deactivate()
{
    stopProcessing();

    if (active.exchange (false))
        plugin->deactivate (plugin);
}

void WclapPlugin::stopProcessing()
{
    // CLAP stops processing on the audio thread. This runs on the message thread while nothing
    // processes the plugin (the engine makes sure), so this thread stands in as its audio thread.
    if (processing.exchange (false))
    {
        const StandInAudioThread audioThread (this);
        plugin->stop_processing (plugin);
    }

    for (auto& channel : heldNotes)
        channel = {};
}

void WclapPlugin::allocateBuffers()
{
    const auto setUp = [this] (std::vector<Port>& ports, std::vector<clap_audio_buffer_t>& buffers)
    {
        buffers.clear();

        for (auto& port : ports)
        {
            port.storage.assign (static_cast<size_t> (port.channels), std::vector<float> (static_cast<size_t> (maxFrames), 0.0f));
            port.pointers.clear();

            for (auto& channel : port.storage)
                port.pointers.push_back (channel.data());

            clap_audio_buffer_t buffer {};
            buffer.data32 = port.pointers.empty() ? nullptr : port.pointers.data();
            buffer.data64 = nullptr;
            buffer.channel_count = static_cast<uint32_t> (port.channels);
            buffer.latency = 0;
            buffer.constant_mask = 0;
            buffers.push_back (buffer);
        }
    };

    setUp (inputPorts, inputBuffers);
    setUp (outputPorts, outputBuffers);
}

//==============================================================================
void WclapPlugin::process (const float* inLeft, const float* inRight,
                           float* outLeft, float* outRight,
                           int numSamples, const juce::MidiBuffer& midi,
                           const clap_event_transport_t* transport) noexcept
{
    // The notes it plays in this block (an arpeggiator's), collected as it puts them out.
    midiOutput.clear();
    inProcess = true;
    const juce::ScopeGuard notProcessing { [this] { inProcess = false; } };

    const auto clearOutputs = [&]
    {
        if (outLeft != nullptr)
            juce::FloatVectorOperations::clear (outLeft, numSamples);

        if (outRight != nullptr && outRight != outLeft)
            juce::FloatVectorOperations::clear (outRight, numSamples);
    };

    const auto frames = juce::jmin (numSamples, maxFrames);

    if (frames <= 0 || ! active.load (std::memory_order_acquire))
    {
        clearOutputs();
        return;
    }

    if (! processing.load (std::memory_order_relaxed))
    {
        if (! plugin->start_processing (plugin))
        {
            processErrorSeen.store (true);
            clearOutputs();
            return;
        }

        processing.store (true);
    }

    // Inputs: the engine's stereo signal goes to the main port, other ports get silence.
    for (size_t portIndex = 0; portIndex < inputPorts.size(); ++portIndex)
    {
        auto& port = inputPorts[portIndex];
        const bool isMain = static_cast<int> (portIndex) == mainInputPort;

        for (int channel = 0; channel < port.channels; ++channel)
        {
            auto* destination = port.pointers[static_cast<size_t> (channel)];
            const float* source = nullptr;

            if (isMain)
                source = channel == 0 ? inLeft : (channel == 1 ? (inRight != nullptr ? inRight : inLeft) : nullptr);
            else if (static_cast<int> (portIndex) == sidechainPort && frames <= sidechain.getNumSamples())
                source = sidechain.getReadPointer (juce::jmin (channel, 1));   // what tracks send to it

            if (source == nullptr)
            {
                juce::FloatVectorOperations::clear (destination, frames);
            }
            else if (isMain && port.channels == 1 && inRight != nullptr && inRight != inLeft)
            {
                juce::FloatVectorOperations::copyWithMultiply (destination, inLeft, 0.5f, frames);
                juce::FloatVectorOperations::addWithMultiply (destination, inRight, 0.5f, frames);
            }
            else
            {
                juce::FloatVectorOperations::copy (destination, source, frames);
            }
        }
    }

    for (auto& port : outputPorts)
        for (auto* channel : port.pointers)
            juce::FloatVectorOperations::clear (channel, frames);

    buildInputEvents (midi, frames);

    clap_process_t processData {};
    processData.steady_time = steadyTime;
    processData.frames_count = static_cast<uint32_t> (frames);
    processData.transport = transport;
    processData.audio_inputs = inputBuffers.empty() ? nullptr : inputBuffers.data();
    processData.audio_inputs_count = static_cast<uint32_t> (inputBuffers.size());
    processData.audio_outputs = outputBuffers.empty() ? nullptr : outputBuffers.data();
    processData.audio_outputs_count = static_cast<uint32_t> (outputBuffers.size());
    processData.in_events = &inputEvents;
    processData.out_events = &outputEvents;

    const auto status = plugin->process (plugin, &processData);
    steadyTime += frames;
    numEvents = 0;

    if (status == CLAP_PROCESS_ERROR)
    {
        processErrorSeen.store (true);
        clearOutputs();
        return;
    }

    if (mainOutputPort < 0 || outLeft == nullptr)
    {
        clearOutputs();
        return;
    }

    const auto& port = outputPorts[static_cast<size_t> (mainOutputPort)];
    const float* left = port.channels > 0 ? port.pointers[0] : nullptr;
    const float* right = port.channels > 1 ? port.pointers[1] : left;

    if (left != nullptr)
        juce::FloatVectorOperations::copy (outLeft, left, frames);
    else
        juce::FloatVectorOperations::clear (outLeft, frames);

    if (outRight != nullptr && outRight != outLeft)
    {
        if (right != nullptr)
            juce::FloatVectorOperations::copy (outRight, right, frames);
        else
            juce::FloatVectorOperations::clear (outRight, frames);
    }

    if (frames < numSamples)
    {
        juce::FloatVectorOperations::clear (outLeft + frames, numSamples - frames);

        if (outRight != nullptr && outRight != outLeft)
            juce::FloatVectorOperations::clear (outRight + frames, numSamples - frames);
    }
}

//==============================================================================
bool WclapPlugin::pushEvent (const EventSlot& slot) noexcept
{
    if (numEvents >= maxEvents)
        return false;

    events[static_cast<size_t> (numEvents++)] = slot;
    return true;
}

void WclapPlugin::addParameterEvent (int index, double value) noexcept
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (parameters.size())))
        return;

    const auto& parameter = parameters[static_cast<size_t> (index)];

    EventSlot slot {};
    slot.param.header = makeHeader (static_cast<uint32_t> (sizeof (clap_event_param_value_t)), 0, CLAP_EVENT_PARAM_VALUE);
    slot.param.param_id = parameter.id;
    slot.param.cookie = parameter.cookie;
    slot.param.note_id = -1;
    slot.param.port_index = -1;
    slot.param.channel = -1;
    slot.param.key = -1;
    slot.param.value = value;
    pushEvent (slot);
}

void WclapPlugin::addParameterEvents() noexcept
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    const auto ready = paramFifo.getNumReady();
    paramFifo.prepareToRead (ready, start1, size1, start2, size2);

    for (int i = 0; i < size1; ++i)
        addParameterEvent (paramQueue[static_cast<size_t> (start1 + i)].index, paramQueue[static_cast<size_t> (start1 + i)].value);

    for (int i = 0; i < size2; ++i)
        addParameterEvent (paramQueue[static_cast<size_t> (start2 + i)].index, paramQueue[static_cast<size_t> (start2 + i)].value);

    paramFifo.finishedRead (size1 + size2);

    // Automation's changes come last: they win over a knob moved in the same block.
    for (int i = 0; i < numAutomationChanges; ++i)
        addParameterEvent (automationChanges[static_cast<size_t> (i)].index, automationChanges[static_cast<size_t> (i)].value);

    numAutomationChanges = 0;
}

void WclapPlugin::setParameterFromAudio (uint32_t paramId, int& indexHint, double value) noexcept
{
    const auto count = static_cast<int> (parameters.size());

    if (! juce::isPositiveAndBelow (indexHint, count) || parameters[static_cast<size_t> (indexHint)].id != paramId)
    {
        indexHint = -1;

        for (int i = 0; i < count; ++i)
            if (parameters[static_cast<size_t> (i)].id == paramId)
                indexHint = i;

        if (indexHint < 0)
            return;
    }

    const auto& parameter = parameters[static_cast<size_t> (indexHint)];
    value = juce::jlimit (parameter.minValue, parameter.maxValue, value);

    if (parameter.stepped)
        value = std::round (value);

    parameterValues[static_cast<size_t> (indexHint)].store (value);

    if (numAutomationChanges < automationQueueSize)
        automationChanges[static_cast<size_t> (numAutomationChanges++)] = { indexHint, value };
}

void WclapPlugin::addNoteEvent (uint16_t type, int time, int channel, int key, double velocity) noexcept
{
    auto& word = heldNotes[static_cast<size_t> (channel)][static_cast<size_t> (key >> 6)];
    const auto bit = juce::uint64 { 1 } << (key & 63);

    if (type == CLAP_EVENT_NOTE_ON)
        word |= bit;
    else
        word &= ~bit;

    EventSlot slot {};
    slot.note.header = makeHeader (static_cast<uint32_t> (sizeof (clap_event_note_t)), time, type);
    slot.note.note_id = -1;
    slot.note.port_index = static_cast<int16_t> (notePortIndex);
    slot.note.channel = static_cast<int16_t> (channel);
    slot.note.key = static_cast<int16_t> (key);
    slot.note.velocity = velocity;
    pushEvent (slot);
}

void WclapPlugin::releaseHeldNotes (int time, int channel) noexcept
{
    for (int key = 0; key < 128; ++key)
        if (((heldNotes[static_cast<size_t> (channel)][static_cast<size_t> (key >> 6)] >> (key & 63)) & 1u) != 0)
            addNoteEvent (CLAP_EVENT_NOTE_OFF, time, channel, key, 0.0);
}

void WclapPlugin::buildInputEvents (const juce::MidiBuffer& midi, int numFrames) noexcept
{
    numEvents = 0;
    addParameterEvents();

    if (! hasNoteInput)
        return;

    for (const auto metadata : midi)
    {
        const auto size = metadata.numBytes;

        if (size < 1 || size > 3)
            continue;   // SysEx is not forwarded

        const auto* data = metadata.data;
        const auto time = juce::jlimit (0, numFrames - 1, metadata.samplePosition);
        const auto status = data[0] & 0xf0;
        const auto channel = data[0] & 0x0f;

        if (sendClapNotes && size == 3)
        {
            const auto key = data[1] & 0x7f;
            const auto value = data[2] & 0x7f;

            if (status == 0x90 && value > 0)
            {
                addNoteEvent (CLAP_EVENT_NOTE_ON, time, channel, key, value / 127.0);
                continue;
            }

            if (status == 0x80 || status == 0x90)
            {
                addNoteEvent (CLAP_EVENT_NOTE_OFF, time, channel, key, status == 0x80 ? value / 127.0 : 0.0);
                continue;
            }

            if (status == 0xb0 && (key == 120 || key == 123))
                releaseHeldNotes (time, channel);
        }

        if (sendMidi)
        {
            EventSlot slot {};
            slot.midi.header = makeHeader (static_cast<uint32_t> (sizeof (clap_event_midi_t)), time, CLAP_EVENT_MIDI);
            slot.midi.port_index = static_cast<uint16_t> (notePortIndex);

            for (int i = 0; i < size; ++i)
                slot.midi.data[i] = data[i];

            pushEvent (slot);
        }
    }
}

//==============================================================================
std::shared_ptr<WclapPlugin::ParameterLists> WclapPlugin::scanParameters() const
{
    auto lists = std::make_shared<ParameterLists>();

    if (paramsExt != nullptr)
    {
        const auto count = juce::jmin (paramsExt->count (plugin), 4096u);
        lists->parameters.reserve (count);

        for (uint32_t i = 0; i < count; ++i)
        {
            clap_param_info_t info {};

            if (! paramsExt->get_info (plugin, i, &info))
                continue;

            Parameter parameter;
            parameter.id = info.id;
            parameter.cookie = info.cookie;
            parameter.name = fromFixed (info.name, CLAP_NAME_SIZE);
            parameter.minValue = info.min_value;
            parameter.maxValue = juce::jmax (info.max_value, info.min_value);
            parameter.defaultValue = juce::jlimit (parameter.minValue, parameter.maxValue, info.default_value);
            parameter.stepped = (info.flags & CLAP_PARAM_IS_STEPPED) != 0;
            parameter.hidden = (info.flags & CLAP_PARAM_IS_HIDDEN) != 0;
            parameter.readOnly = (info.flags & CLAP_PARAM_IS_READONLY) != 0;
            lists->parameters.push_back (parameter);
        }
    }

    const auto& found = lists->parameters;
    lists->values.reset (new std::atomic<double>[juce::jmax<size_t> (found.size(), 1)]);
    lists->lookup.reserve (found.size());

    for (size_t i = 0; i < found.size(); ++i)
    {
        auto value = found[i].defaultValue;

        if (paramsExt != nullptr && ! paramsExt->get_value (plugin, found[i].id, &value))
            value = found[i].defaultValue;

        lists->values[i].store (value);
        lists->lookup.emplace_back (found[i].id, static_cast<int> (i));
    }

    std::sort (lists->lookup.begin(), lists->lookup.end());
    return lists;
}

void WclapPlugin::rescanParameters()
{
    readLists = scanParameters();
    takeReadParameters();   // the old lists go here: nothing plays while this is used
}

bool WclapPlugin::readParameters()
{
    // CLAP's parameter info and values are main-thread calls, allowed while the plugin plays.
    readLists = scanParameters();
    return true;
}

std::shared_ptr<void> WclapPlugin::takeReadParameters()
{
    if (readLists == nullptr)
        return {};

    // A swap: the device gets the new lists, the old ones go back to be let go of after the lock.
    std::swap (parameters, readLists->parameters);
    std::swap (parameterLookup, readLists->lookup);
    std::swap (parameterValues, readLists->values);
    paramFifo.reset();
    return std::static_pointer_cast<void> (std::exchange (readLists, nullptr));
}

int WclapPlugin::findParameterIndex (clap_id id) const noexcept
{
    const auto found = std::lower_bound (parameterLookup.begin(), parameterLookup.end(), std::make_pair (id, 0),
                                         [] (const auto& a, const auto& b) { return a.first < b.first; });

    return found != parameterLookup.end() && found->first == id ? found->second : -1;
}

double WclapPlugin::getParameterValue (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, static_cast<int> (parameters.size()))
               ? parameterValues[static_cast<size_t> (index)].load()
               : 0.0;
}

juce::String WclapPlugin::getParameterText (int index, double value)
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (parameters.size())))
        return {};

    const auto& parameter = parameters[static_cast<size_t> (index)];

    if (paramsExt != nullptr && paramsExt->value_to_text != nullptr)
    {
        char text[128] = {};

        if (paramsExt->value_to_text (plugin, parameter.id, value, text, static_cast<uint32_t> (sizeof (text))))
            return fromFixed (text, sizeof (text));
    }

    return parameter.stepped ? juce::String (juce::roundToInt (value)) : juce::String (value, 3);
}

double WclapPlugin::queueParameter (int index, double value)
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (parameters.size())))
        return 0.0;

    const auto& parameter = parameters[static_cast<size_t> (index)];
    value = juce::jlimit (parameter.minValue, parameter.maxValue, value);

    if (parameter.stepped)
        value = std::round (value);

    parameterValues[static_cast<size_t> (index)].store (value);

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    paramFifo.prepareToWrite (1, start1, size1, start2, size2);

    if (size1 > 0)
    {
        paramQueue[static_cast<size_t> (start1)] = { index, value };
        paramFifo.finishedWrite (1);
    }

    return value;
}

void WclapPlugin::flushQueuedParameters()
{
    numEvents = 0;
    addParameterEvents();

    // CLAP flushes an active plugin on the audio thread (an inactive one on the main thread).
    // Nothing processes it now (the callers make sure), so this thread stands in as its audio thread.
    if (numEvents > 0 && paramsExt != nullptr && paramsExt->flush != nullptr)
    {
        const StandInAudioThread audioThread (active.load() ? static_cast<const void*> (this) : audioThreadFor);
        paramsExt->flush (plugin, &inputEvents, &outputEvents);
    }

    numEvents = 0;
}

//==============================================================================
WclapPlugin::Requests WclapPlugin::takeRequests() noexcept
{
    Requests requests;
    requests.callback = callbackRequested.exchange (false);
    requests.rescan = rescanRequested.exchange (false);
    requests.restart = restartRequested.exchange (false);
    requests.flush = flushRequested.exchange (false);
    requests.processError = processErrorSeen.exchange (false);
    return requests;
}

void WclapPlugin::runOnMainThread()
{
    if (initialised)
        plugin->on_main_thread (plugin);
}

bool WclapPlugin::getError (juce::String& message) const
{
    if (module == nullptr)
        return false;

    char text[512] = {};

    if (! wclap_get_error (module, text, static_cast<uint32_t> (sizeof (text))))
        return false;

    message = fromFixed (text, sizeof (text));
    return true;
}

//==============================================================================
const void* CLAP_ABI WclapPlugin::hostGetExtension (const clap_host_t* clapHost, const char* extensionId)
{
    static const clap_host_gui_t guiExtension { hostGuiResizeHintsChanged, hostGuiRequestResize, hostGuiRequestShow,
                                                hostGuiRequestHide, hostGuiClosed };
    static const clap_host_thread_check_t threadCheckExtension { hostIsMainThread, hostIsAudioThread };
    static const clap_host_log_t logExtension { hostLog };
    static const clap_host_params_t paramsExtension { hostParamsRescan, hostParamsClear, hostParamsRequestFlush };
    static const clap_host_note_ports_t notePortsExtension { hostNotePortsDialects, hostNotePortsRescan };
    static const clap_host_state_t stateExtension { hostStateMarkDirty };
    static const clap_host_latency_t latencyExtension { hostLatencyChanged };
    static const clap_host_webview_t webviewExtension { hostWebviewSend };

    if (extensionId == nullptr)
        return nullptr;

    if (std::strcmp (extensionId, CLAP_EXT_LOG) == 0)          return &logExtension;
    if (std::strcmp (extensionId, CLAP_EXT_PARAMS) == 0)       return &paramsExtension;
    if (std::strcmp (extensionId, CLAP_EXT_NOTE_PORTS) == 0)   return &notePortsExtension;
    if (std::strcmp (extensionId, CLAP_EXT_STATE) == 0)        return &stateExtension;
    if (std::strcmp (extensionId, CLAP_EXT_LATENCY) == 0)      return &latencyExtension;
    if (std::strcmp (extensionId, CLAP_EXT_WEBVIEW) == 0)      return &webviewExtension;

    // A native plugin's own window, and which thread is which (a WCLAP's go through its bridge).
    if (clapHost != nullptr && fromHost (clapHost).nativeModule != nullptr)
    {
        if (std::strcmp (extensionId, CLAP_EXT_GUI) == 0)           return &guiExtension;
        if (std::strcmp (extensionId, CLAP_EXT_THREAD_CHECK) == 0)  return &threadCheckExtension;
    }

    return nullptr;
}

namespace
{
    // The streams the plugin writes its state into and reads it back from.
    int64_t CLAP_ABI writeState (const clap_ostream_t* stream, const void* buffer, uint64_t size)
    {
        auto* out = static_cast<juce::MemoryOutputStream*> (stream->ctx);
        return out->write (buffer, static_cast<size_t> (size)) ? static_cast<int64_t> (size) : -1;
    }

    struct StateReader
    {
        const juce::MemoryBlock* block = nullptr;
        size_t position = 0;
    };

    int64_t CLAP_ABI readState (const clap_istream_t* stream, void* buffer, uint64_t size)
    {
        auto* reader = static_cast<StateReader*> (stream->ctx);
        const auto available = reader->block->getSize() - reader->position;
        const auto count = static_cast<size_t> (juce::jmin<uint64_t> (size, available));

        if (count > 0)
            std::memcpy (buffer, static_cast<const char*> (reader->block->getData()) + reader->position, count);

        reader->position += count;
        return static_cast<int64_t> (count);   // 0 once it has all been read
    }
}

bool WclapPlugin::saveState (juce::MemoryBlock& destination)
{
    if (stateExt == nullptr || plugin == nullptr)
        return false;

    juce::MemoryOutputStream out;
    const clap_ostream_t stream { &out, writeState };

    if (! stateExt->save (plugin, &stream))
        return false;

    destination = out.getMemoryBlock();
    return true;
}

bool WclapPlugin::loadState (const juce::MemoryBlock& source)
{
    if (stateExt == nullptr || plugin == nullptr)
        return false;

    StateReader reader { &source, 0 };
    const clap_istream_t stream { &reader, readState };
    const auto loaded = stateExt->load (plugin, &stream);

    // The values the page shows are read from here, so take the restored ones.
    for (size_t i = 0; i < parameters.size(); ++i)
    {
        auto value = parameters[i].defaultValue;

        if (paramsExt != nullptr && paramsExt->get_value (plugin, parameters[i].id, &value))
            parameterValues[i].store (value);
    }

    return loaded;
}

double WclapPlugin::readParameterValue (int index)
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (parameters.size())))
        return 0.0;

    auto value = parameterValues[static_cast<size_t> (index)].load();
    double current = value;

    if (paramsExt != nullptr && plugin != nullptr && paramsExt->get_value (plugin, parameters[static_cast<size_t> (index)].id, &current))
        value = current;

    return value;
}

int WclapPlugin::findParameter (clap_id id) const noexcept
{
    for (size_t i = 0; i < parameters.size(); ++i)
        if (parameters[i].id == id)
            return static_cast<int> (i);

    return -1;
}

//==============================================================================
void CLAP_ABI WclapPlugin::hostRequestRestart (const clap_host_t* h)            { fromHost (h).restartRequested.store (true); }
void CLAP_ABI WclapPlugin::hostRequestProcess (const clap_host_t*)              {}
void CLAP_ABI WclapPlugin::hostRequestCallback (const clap_host_t* h)           { fromHost (h).callbackRequested.store (true); }

void CLAP_ABI WclapPlugin::hostLog (const clap_host_t*, clap_log_severity, [[maybe_unused]] const char* message)
{
    DBG ("WCLAP: " << fromPlugin (message));
}

void CLAP_ABI WclapPlugin::hostParamsRescan (const clap_host_t* h, clap_param_rescan_flags)     { fromHost (h).rescanRequested.store (true); }
void CLAP_ABI WclapPlugin::hostParamsClear (const clap_host_t*, clap_id, clap_param_clear_flags) {}
void CLAP_ABI WclapPlugin::hostParamsRequestFlush (const clap_host_t* h)                         { fromHost (h).flushRequested.store (true); }

uint32_t CLAP_ABI WclapPlugin::hostNotePortsDialects (const clap_host_t*)
{
    return CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
}

void CLAP_ABI WclapPlugin::hostNotePortsRescan (const clap_host_t*, uint32_t)   {}
void CLAP_ABI WclapPlugin::hostStateMarkDirty (const clap_host_t* h)            { fromHost (h).stateChanged.store (true); }
void CLAP_ABI WclapPlugin::hostLatencyChanged (const clap_host_t*)             {}

//==============================================================================
// Web screens
bool CLAP_ABI WclapPlugin::hostWebviewSend (const clap_host_t* h, const void* buffer, uint32_t size)
{
    auto& self = fromHost (h);

    // As CLAP asks: it fails while the screen isn't open.
    if (! self.screenOpen.load() || buffer == nullptr)
        return false;

    const juce::ScopedLock lock (self.screenLock);

    if (self.screenMessages.size() >= 4096)   // a page that stopped listening doesn't grow this forever
        self.screenMessages.erase (self.screenMessages.begin());

    self.screenMessages.emplace_back (buffer, static_cast<size_t> (size));
    return true;
}

juce::String WclapPlugin::getScreenUri()
{
    if (webviewExt == nullptr || webviewExt->get_uri == nullptr)
        return {};

    const auto length = webviewExt->get_uri (plugin, nullptr, 0);   // with the terminating zero

    if (length <= 0)
        return {};

    std::vector<char> uri (static_cast<size_t> (length) + 1, 0);
    webviewExt->get_uri (plugin, uri.data(), static_cast<uint32_t> (uri.size()));
    return juce::String::fromUTF8 (uri.data());
}

bool WclapPlugin::getScreenResource (const juce::String& path, juce::String& mime, juce::MemoryBlock& data)
{
    if (webviewExt == nullptr || webviewExt->get_resource == nullptr)
        return false;

    data.reset();
    clap_ostream_t stream {};
    stream.ctx = &data;
    stream.write = [] (const clap_ostream_t* s, const void* buffer, uint64_t size) -> int64_t
    {
        static_cast<juce::MemoryBlock*> (s->ctx)->append (buffer, static_cast<size_t> (size));
        return static_cast<int64_t> (size);
    };

    char mimeType[256] = {};
    const auto found = webviewExt->get_resource (plugin, path.toRawUTF8(), mimeType, sizeof (mimeType) - 1, &stream);
    mime = juce::String::fromUTF8 (mimeType);
    return found;
}

void WclapPlugin::setScreenOpen (bool open)
{
    screenOpen.store (open);

    if (! open)
    {
        const juce::ScopedLock lock (screenLock);
        screenMessages.clear();
    }
}

bool WclapPlugin::sendToPlugin (const void* data, size_t size)
{
    return webviewExt != nullptr && webviewExt->receive != nullptr && screenOpen.load()
           && webviewExt->receive (plugin, data, static_cast<uint32_t> (size));
}

std::vector<juce::MemoryBlock> WclapPlugin::takeScreenMessages()
{
    const juce::ScopedLock lock (screenLock);
    return std::exchange (screenMessages, {});
}

bool WclapPlugin::getScreenSize (int& width, int& height)
{
    uint32_t w = 0, h = 0;

    if (guiExt == nullptr || guiExt->get_size == nullptr || ! guiExt->get_size (plugin, &w, &h) || w == 0 || h == 0)
        return false;

    width = static_cast<int> (w);
    height = static_cast<int> (h);
    return true;
}

//==============================================================================
uint32_t CLAP_ABI WclapPlugin::inputEventsSize (const clap_input_events_t* list)
{
    return static_cast<uint32_t> (static_cast<const WclapPlugin*> (list->ctx)->numEvents);
}

const clap_event_header_t* CLAP_ABI WclapPlugin::inputEventsGet (const clap_input_events_t* list, uint32_t index)
{
    const auto& self = *static_cast<const WclapPlugin*> (list->ctx);

    return index < static_cast<uint32_t> (self.numEvents) ? &self.events[index].header : nullptr;
}

bool CLAP_ABI WclapPlugin::outputEventsTryPush (const clap_output_events_t* list, const clap_event_header_t* event)
{
    auto& self = *static_cast<WclapPlugin*> (list->ctx);

    // Notes it plays itself, while it processes: they go on to the tracks taking MIDI from it.
    if (event != nullptr && event->space_id == CLAP_CORE_EVENT_SPACE_ID && self.inProcess)
    {
        const auto sample = static_cast<int> (event->time);

        if (event->type == CLAP_EVENT_NOTE_ON || event->type == CLAP_EVENT_NOTE_OFF)
        {
            const auto* note = reinterpret_cast<const clap_event_note_t*> (event);

            if (note->key >= 0 && note->key < 128)
            {
                const auto channel = juce::jlimit (1, 16, note->channel < 0 ? 1 : note->channel + 1);
                const auto velocity = static_cast<float> (juce::jlimit (0.0, 1.0, note->velocity));
                self.midiOutput.addEvent (event->type == CLAP_EVENT_NOTE_ON ? juce::MidiMessage::noteOn (channel, note->key, velocity)
                                                                            : juce::MidiMessage::noteOff (channel, note->key, velocity),
                                          sample);
            }

            return true;
        }

        if (event->type == CLAP_EVENT_MIDI)
        {
            const auto* midi = reinterpret_cast<const clap_event_midi_t*> (event);
            self.midiOutput.addEvent (midi->data, 3, sample);
            return true;
        }
    }

    if (event != nullptr && event->space_id == CLAP_CORE_EVENT_SPACE_ID && event->type == CLAP_EVENT_PARAM_VALUE)
    {
        const auto* change = reinterpret_cast<const clap_event_param_value_t*> (event);
        const auto index = self.findParameterIndex (change->param_id);

        if (index >= 0)
        {
            self.parameterValues[static_cast<size_t> (index)].store (change->value);
            self.touchedParameter.store (index);   // the plugin's screen moved it
        }
    }

    // The user took hold of a knob on the plugin's screen.
    if (event != nullptr && event->space_id == CLAP_CORE_EVENT_SPACE_ID && event->type == CLAP_EVENT_PARAM_GESTURE_BEGIN)
    {
        const auto* gesture = reinterpret_cast<const clap_event_param_gesture_t*> (event);

        if (const auto index = self.findParameterIndex (gesture->param_id); index >= 0)
            self.touchedParameter.store (index);
    }

    return true;
}

//==============================================================================
// A native plugin's own window (CLAP's gui extension), in a window of Sonify's.
#if FANAN_CLAP_WINDOWS
class WclapPlugin::NativeScreen final : public juce::DocumentWindow
{
public:
    NativeScreen (WclapPlugin& plugin, const juce::String& title)
        : DocumentWindow (title, juce::Colour (0xff2b2d31), DocumentWindow::closeButton), owner (plugin)
    {
        setUsingNativeTitleBar (true);
        holder.setSize (400, 300);
        setContentNonOwned (&holder, true);
        setResizable (false, false);
    }

    ~NativeScreen() override
    {
        clearContentComponent();
    }

    void closeButtonPressed() override   { owner.hideNativeScreen(); }

    /** Where the plugin's window goes: this window's own (it fills it, under the title bar). */
    void* getParentHandle()
    {
        auto* peer = getPeer();
        return peer != nullptr ? peer->getNativeHandle() : nullptr;
    }

    double getScale()
    {
       #if JUCE_WINDOWS
        if (auto* peer = getPeer())
            return juce::jmax (0.5, peer->getPlatformScaleFactor());
       #endif
        return 1.0;
    }

    /** The plugin's size (in its own pixels) as this window's. */
    void setPluginSize (int width, int height)
    {
        const auto scale = owner.guiScale > 0.0 ? owner.guiScale : 1.0;
        settingSize = true;
        holder.setSize (juce::jmax (40, juce::roundToInt (width / scale)), juce::jmax (40, juce::roundToInt (height / scale)));
        settingSize = false;
    }

private:
    // Nothing of its own is drawn: the plugin's window covers it.
    struct Holder final : public juce::Component
    {
        explicit Holder (NativeScreen& screen) : window (screen)    { setOpaque (true); }
        void paint (juce::Graphics& g) override                      { g.fillAll (juce::Colours::black); }

        void resized() override
        {
            if (! window.settingSize)
                window.owner.nativeScreenResized (getWidth(), getHeight());
        }

        NativeScreen& window;
    };

    WclapPlugin& owner;
    bool settingSize = false;
    Holder holder { *this };
};
#else
class WclapPlugin::NativeScreen {};
#endif

bool WclapPlugin::showNativeScreen (const juce::String& title)
{
   #if FANAN_CLAP_WINDOWS
    if (! nativeGui || ! initialised)
        return false;

    if (nativeScreen == nullptr)
    {
       #if JUCE_WINDOWS
        static constexpr const char* windowApi = CLAP_WINDOW_API_WIN32;
       #elif JUCE_MAC
        static constexpr const char* windowApi = CLAP_WINDOW_API_COCOA;
       #else
        static constexpr const char* windowApi = CLAP_WINDOW_API_X11;
       #endif

        auto screen = std::make_unique<NativeScreen> (*this, title);
        screen->centreWithSize (screen->getWidth(), screen->getHeight());
        screen->setVisible (true);   // its window first: the plugin's goes in it

        auto* parent = screen->getParentHandle();

        if (parent == nullptr || ! guiExt->create (plugin, windowApi, false))
            return false;

        guiCreated = true;
        guiScale = 1.0;

        // Windows works in the screen's own pixels: the plugin hears the scale, and its size is
        // turned into Sonify's.
       #if JUCE_WINDOWS
        guiScale = screen->getScale();

        if (guiExt->set_scale != nullptr && ! guiExt->set_scale (plugin, guiScale))
            guiScale = 1.0;
       #endif

        uint32_t width = 0, height = 0;

        if (guiExt->get_size != nullptr && guiExt->get_size (plugin, &width, &height) && width > 0 && height > 0)
            screen->setPluginSize (static_cast<int> (width), static_cast<int> (height));

        clap_window_t window {};
        window.api = windowApi;
       #if JUCE_WINDOWS
        window.win32 = parent;
       #elif JUCE_MAC
        window.cocoa = parent;
       #else
        window.x11 = static_cast<clap_xwnd> (reinterpret_cast<juce::pointer_sized_uint> (parent));
       #endif

        if (! guiExt->set_parent (plugin, &window))
        {
            guiExt->destroy (plugin);
            guiCreated = false;
            return false;
        }

        screen->setResizable (guiExt->can_resize != nullptr && guiExt->can_resize (plugin), false);
        nativeScreen = std::move (screen);
    }

    nativeScreen->setName (title);
    nativeScreen->setVisible (true);
    nativeScreen->toFront (true);

    if (guiExt->show != nullptr)
        guiExt->show (plugin);

    return true;
   #else
    juce::ignoreUnused (title);
    return false;
   #endif
}

void WclapPlugin::hideNativeScreen()
{
   #if FANAN_CLAP_WINDOWS
    if (nativeScreen == nullptr)
        return;

    if (guiCreated && guiExt->hide != nullptr)
        guiExt->hide (plugin);

    nativeScreen->setVisible (false);
   #endif
}

void WclapPlugin::destroyNativeScreen()
{
   #if FANAN_CLAP_WINDOWS
    if (guiCreated && plugin != nullptr && guiExt != nullptr && guiExt->destroy != nullptr)
        guiExt->destroy (plugin);

    guiCreated = false;
   #endif
    nativeScreen.reset();
}

void WclapPlugin::nativeScreenResized (int width, int height)
{
   #if FANAN_CLAP_WINDOWS
    if (! guiCreated || guiExt->set_size == nullptr)
        return;

    auto w = static_cast<uint32_t> (juce::jmax (1, juce::roundToInt (width * guiScale)));
    auto h = static_cast<uint32_t> (juce::jmax (1, juce::roundToInt (height * guiScale)));

    // The nearest size it takes, then that size for the window.
    if (guiExt->adjust_size != nullptr)
        guiExt->adjust_size (plugin, &w, &h);

    guiExt->set_size (plugin, w, h);

    if (nativeScreen != nullptr)
        nativeScreen->setPluginSize (static_cast<int> (w), static_cast<int> (h));
   #else
    juce::ignoreUnused (width, height);
   #endif
}

void CLAP_ABI WclapPlugin::hostGuiResizeHintsChanged (const clap_host_t*) {}

bool CLAP_ABI WclapPlugin::hostGuiRequestResize (const clap_host_t* h, uint32_t width, uint32_t height)
{
   #if FANAN_CLAP_WINDOWS
    auto& self = fromHost (h);

    // Only on the message thread (where it asks from, as a rule).
    if (! onMessageThread() || self.nativeScreen == nullptr)
        return false;

    self.nativeScreen->setPluginSize (static_cast<int> (width), static_cast<int> (height));
    return true;
   #else
    juce::ignoreUnused (h, width, height);
    return false;
   #endif
}

bool CLAP_ABI WclapPlugin::hostGuiRequestShow (const clap_host_t* h)
{
    auto& self = fromHost (h);

    if (! onMessageThread() || self.nativeScreen == nullptr)
        return false;

    return self.showNativeScreen (self.descriptor.name);
}

bool CLAP_ABI WclapPlugin::hostGuiRequestHide (const clap_host_t* h)
{
    auto& self = fromHost (h);

    if (! onMessageThread())
        return false;

    self.hideNativeScreen();
    return true;
}

void CLAP_ABI WclapPlugin::hostGuiClosed (const clap_host_t* h, bool wasDestroyed)
{
   #if FANAN_CLAP_WINDOWS
    auto& self = fromHost (h);

    if (! onMessageThread())
        return;

    // Its window closed by the plugin itself: Sonify's goes too (made again when next opened).
    if (wasDestroyed)
    {
        self.guiCreated = false;
        self.nativeScreen.reset();
    }
    else if (self.nativeScreen != nullptr)
    {
        self.nativeScreen->setVisible (false);
    }
   #else
    juce::ignoreUnused (h, wasDestroyed);
   #endif
}

// The message thread stays the main thread even while it stands in as a plugin's audio thread
// (stop_processing, params.flush): CLAP allows one thread to be both.
bool CLAP_ABI WclapPlugin::hostIsMainThread (const clap_host_t*)
{
    return onMessageThread();
}

bool CLAP_ABI WclapPlugin::hostIsAudioThread (const clap_host_t* h)
{
    return (h != nullptr && audioThreadFor == &fromHost (h)) || ! onMessageThread();
}
