// D:\workspace\Fanan_web_hub\src\Cloud.cpp

#include "Cloud.h"

namespace
{
    /** The Base44 app that holds what Fanan publishes. FananWebHub-cloud.json beside the
        program can name another (and another server, as the tests do). */
    constexpr const char* fananAppId = "6abdbe690c1a2fa318bee483";   // Fanan Hub Cloud on Base44

    juce::var object()
    {
        return juce::var (new juce::DynamicObject());
    }

    /** What the server said went wrong, in its own words where it gave them. */
    juce::String errorText (int status, const juce::var& body, const juce::String& text)
    {
        for (const auto* key : { "detail", "message", "error" })
        {
            const auto value = body.getProperty (key, {});

            if (value.isString() && value.toString().isNotEmpty())
                return value.toString();

            if (value.isObject() && value.getProperty ("message", {}).toString().isNotEmpty())
                return value.getProperty ("message", {}).toString();
        }

        if (status == 401 || status == 403)
            return "The Fanan cloud refused the request.";

        if (status == 404)
            return "Not found in the Fanan cloud.";

        return "The Fanan cloud answered " + juce::String (status) + (text.isNotEmpty() && text.length() < 120 ? ": " + text : juce::String());
    }
}

//==============================================================================
Cloud::Settings Cloud::loadSettings()
{
    Settings loaded;
    loaded.appId = fananAppId;

    const auto file = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile ("FananWebHub-cloud.json");
    const auto config = juce::JSON::parse (file);

    if (const auto appId = config.getProperty ("appId", {}).toString().trim(); appId.isNotEmpty())
        loaded.appId = appId;

    if (const auto server = config.getProperty ("serverUrl", {}).toString().trim(); server.isNotEmpty())
        loaded.serverUrl = server.trimCharactersAtEnd ("/");

    return loaded;
}

Cloud::Cloud (Settings s)
    : settings (std::move (s))
{
}

Cloud::~Cloud()
{
    alive->store (false);                 // no answers after this
    worker.removeAllJobs (true, 20000);   // a job under way finishes first
}

//==============================================================================
void Cloud::run (std::function<void()> job)
{
    ++pending;
    worker.addJob ([this, job = std::move (job)]
    {
        job();
        --pending;
    });
}

void Cloud::answer (const Done& done, juce::Result result, juce::var value)
{
    std::function<void()> call = [alive = alive, done, result = std::move (result), value = std::move (value)]
    {
        if (alive->load() && done != nullptr)
            done (result, value);
    };

    if (deliverer != nullptr)
        deliverer (std::move (call));
    else
        juce::MessageManager::callAsync (std::move (call));
}

void Cloud::report (const Progress& progress, double fraction, const juce::String& text)
{
    if (progress == nullptr)
        return;

    std::function<void()> call = [alive = alive, progress, fraction, text]
    {
        if (alive->load())
            progress (fraction, text);
    };

    if (deliverer != nullptr)
        deliverer (std::move (call));
    else
        juce::MessageManager::callAsync (std::move (call));
}

//==============================================================================
Cloud::Response Cloud::request (const juce::String& method, const juce::String& path, const juce::var& json,
                                const juce::StringPairArray& query, int timeoutMs)
{
    Response response;

    if (blocked.load())
    {
        response.error = "The internet is blocked (the switch at the top left). Turn it off to use the Fanan cloud.";
        return response;
    }

    juce::URL url (settings.serverUrl + "/api/apps/" + settings.appId + path);

    for (const auto& key : query.getAllKeys())
        url = url.withParameter (key, query[key]);

    const bool hasBody = ! json.isVoid();

    if (hasBody)
        url = url.withPOSTData (juce::JSON::toString (json, true));

    juce::String headers;
    headers << "X-App-Id: " << settings.appId << "\r\nAccept: application/json\r\n";

    if (hasBody)
        headers << "Content-Type: application/json\r\n";

    const auto options = juce::URL::InputStreamOptions (hasBody ? juce::URL::ParameterHandling::inPostData
                                                                : juce::URL::ParameterHandling::inAddress)
                             .withExtraHeaders (headers)
                             .withConnectionTimeoutMs (timeoutMs)
                             .withStatusCode (&response.status)
                             .withNumRedirectsToFollow (5)
                             .withHttpRequestCmd (method);

    const auto stream = url.createInputStream (options);

    if (stream == nullptr)
    {
        response.error = "Couldn't reach the Fanan cloud. Check the internet connection.";
        return response;
    }

    const auto text = stream->readEntireStreamAsString();
    response.body = juce::JSON::parse (text);

    if (response.status == 0 || response.status >= 400)
        response.error = errorText (response.status, response.body, text);

    return response;
}

bool Cloud::download (const juce::String& address, const juce::File& destination, juce::String& error)
{
    if (blocked.load())
    {
        error = "The internet is blocked.";
        return false;
    }

    int status = 0;
    const auto stream = juce::URL (address).createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                                   .withConnectionTimeoutMs (60000)
                                                                   .withStatusCode (&status)
                                                                   .withNumRedirectsToFollow (5));

    if (stream == nullptr || status >= 400)
    {
        error = "Couldn't download " + destination.getFileName() + (status >= 400 ? " (" + juce::String (status) + ")" : juce::String());
        return false;
    }

    destination.getParentDirectory().createDirectory();
    const auto partial = destination.getSiblingFile (destination.getFileName() + ".part");
    partial.deleteFile();

    {
        juce::FileOutputStream out (partial);
        const auto written = out.openedOk() ? out.writeFromInputStream (*stream, -1) : -1;
        out.flush();

        if (written < 0 || out.getStatus().failed())
        {
            error = "Couldn't write " + destination.getFullPathName();
            return false;
        }
    }

    destination.deleteFile();
    return partial.moveFileTo (destination);
}

//==============================================================================
namespace
{
    /** The file's own extension from its address, the double ones included (.wclap.tar.gz). */
    juce::String extensionOf (const juce::String& address)
    {
        const auto name = juce::URL (address).getFileName().toLowerCase();

        for (const auto* ending : { ".wclap.tar.gz", ".wclap.tgz", ".tar.gz" })
            if (name.endsWith (ending))
                return ending;

        // As File::getFileExtension would, without making a File of a bare name (a debug assertion).
        return name.containsChar ('.') ? name.fromLastOccurrenceOf (".", true, false) : juce::String();
    }

    /** Where an item of a kind goes under the content folder. */
    juce::File placeFor (const juce::String& kind, const juce::File& root)
    {
        if (kind == "loop")    return root.getChildFile ("Loops");
        if (kind == "sample")  return root.getChildFile ("Samples");
        if (kind == "song")    return root.getChildFile ("Songs");
        if (kind == "plugin")  return root.getChildFile ("Plugins");
        if (kind == "miditool") return root.getChildFile ("MIDI Tools");
        return root.getChildFile ("Other");
    }

    /** A song's package, unpacked only if every file in it stays inside its folder. */
    juce::Result unpack (const juce::File& package, const juce::File& folder)
    {
        juce::ZipFile zip (package);

        if (zip.getNumEntries() == 0)
            return juce::Result::fail ("The song's package is empty or damaged.");

        for (int i = 0; i < zip.getNumEntries(); ++i)
        {
            const auto* entry = zip.getEntry (i);
            const auto target = folder.getChildFile (entry->filename);

            if (entry->filename.contains ("..") || juce::File::isAbsolutePath (entry->filename) || ! target.isAChildOf (folder))
                return juce::Result::fail ("The song's package tries to write outside its folder, so it wasn't opened.");
        }

        folder.createDirectory();
        return zip.uncompressTo (folder, true);
    }
}

void Cloud::listContent (Done done)
{
    run ([this, done]
    {
        juce::StringPairArray query;
        query.set ("sort", "-created_date");
        query.set ("limit", "500");

        auto published = object();
        published.getDynamicObject()->setProperty ("published", true);
        query.set ("q", juce::JSON::toString (published, true));

        const auto response = request ("GET", juce::String ("/entities/") + contentEntityName, {}, query);

        if (response.error.isNotEmpty())
        {
            answer (done, juce::Result::fail (response.error));
            return;
        }

        juce::Array<juce::var> items;

        if (const auto* records = response.body.getArray())
        {
            for (const auto& record : *records)
            {
                const auto kind = record.getProperty ("kind", {}).toString().toLowerCase();
                const auto file = record.getProperty ("file_url", {}).toString();

                if (file.isEmpty() || ! juce::StringArray { "loop", "sample", "song", "plugin", "miditool" }.contains (kind))
                    continue;   // nothing the Hub knows how to use

                auto item = object();
                auto* fields = item.getDynamicObject();
                fields->setProperty ("id", record.getProperty ("id", {}).toString());
                fields->setProperty ("title", record.getProperty ("title", "Untitled").toString());
                fields->setProperty ("kind", kind);
                fields->setProperty ("description", record.getProperty ("description", {}).toString());
                fields->setProperty ("tags", record.getProperty ("tags", juce::Array<juce::var>()));
                fields->setProperty ("bpm", record.getProperty ("bpm", 0));
                fields->setProperty ("key", record.getProperty ("key", {}).toString());
                fields->setProperty ("size", record.getProperty ("size_bytes", 0));
                fields->setProperty ("version", record.getProperty ("version", "1").toString());
                fields->setProperty ("file_url", file);
                fields->setProperty ("preview_url", record.getProperty ("preview_url", {}).toString());
                fields->setProperty ("tool_id", record.getProperty ("tool_id", {}).toString());
                items.add (item);
            }
        }

        answer (done, juce::Result::ok(), items);
    });
}

void Cloud::installContent (const juce::var& item, const juce::File& root, Done done, Progress progress)
{
    run ([this, item, root, done, progress]
    {
        const auto kind = item.getProperty ("kind", {}).toString();
        const auto address = item.getProperty ("file_url", {}).toString();
        const auto title = juce::File::createLegalFileName (item.getProperty ("title", "Untitled").toString());
        const auto place = placeFor (kind, root);
        place.createDirectory();

        juce::String error;
        const auto extension = extensionOf (address);

        // A MIDI tool: its definition (the model's files are fetched from where it says) or a package.
        if (kind == "miditool")
        {
            if (extension != ".json" && extension != ".zip")
            {
                answer (done, juce::Result::fail ("That MIDI tool isn't a definition or a package Sonify can use."));
                return;
            }

            report (progress, 0.0, "Downloading " + title);
            const auto file = place.getChildFile (juce::Uuid().toString() + extension);

            if (! download (address, file, error))
            {
                answer (done, juce::Result::fail (error));
                return;
            }

            juce::var tool;

            if (extension == ".json")
                tool = juce::JSON::parse (file);
            else if (auto zip = juce::ZipFile (file); const auto* entry = zip.getEntry ("tool.json"))
                if (std::unique_ptr<juce::InputStream> in { zip.createStreamForEntry (*entry) })
                    tool = juce::JSON::parse (in->readEntireStreamAsString());

            if (tool.isObject() && item.getProperty ("tool_id", {}).toString().isNotEmpty())
                tool.getDynamicObject()->setProperty ("id", item.getProperty ("tool_id", {}).toString());

            if (tool.isObject())
                tool.getDynamicObject()->setProperty ("version", item.getProperty ("version", "1").toString());

            const auto result = installToolNow (tool, place, extension == ".zip" ? file : juce::File(), progress);
            file.deleteFile();

            if (result.failed())
            {
                answer (done, result);
                return;
            }

            auto value = object();
            value.getDynamicObject()->setProperty ("path", place.getChildFile (tool.getProperty ("id", {}).toString()).getFullPathName());
            answer (done, juce::Result::ok(), value);
            return;
        }

        // Only what the Hub can use safely is taken: plugins as WCLAP (sandboxed WebAssembly,
        // never native code), loops and samples as audio files, songs as packages.
        if (kind == "plugin" && ! juce::StringArray { ".wclap.tar.gz", ".wclap.tgz", ".wasm" }.contains (extension))
        {
            answer (done, juce::Result::fail ("Only WCLAP plugins can be installed from Fanan's content."));
            return;
        }

        if ((kind == "loop" || kind == "sample") && ! juce::StringArray { ".wav", ".aif", ".aiff", ".flac", ".ogg", ".mp3" }.contains (extension))
        {
            answer (done, juce::Result::fail ("That " + kind + " isn't an audio file Sonify can use."));
            return;
        }

        report (progress, 0.0, "Downloading " + title);

        if (kind == "song")
        {
            // A song comes as a package (the song file and its audio), unpacked into its own folder.
            const auto package = place.getChildFile (title + ".download.zip");

            if (! download (address, package, error))
            {
                answer (done, juce::Result::fail (error));
                return;
            }

            const auto folder = place.getChildFile (title);
            const auto unpacked = unpack (package, folder);
            package.deleteFile();

            if (unpacked.failed())
            {
                answer (done, unpacked);
                return;
            }

            const auto songs = folder.findChildFiles (juce::File::findFiles, true, "*.fsong");

            if (songs.isEmpty())
            {
                answer (done, juce::Result::fail ("The package has no song in it."));
                return;
            }

            auto value = object();
            value.getDynamicObject()->setProperty ("path", songs.getFirst().getFullPathName());
            answer (done, juce::Result::ok(), value);
            return;
        }

        const auto target = place.getChildFile (title + extension);

        if (! download (address, target, error))
        {
            answer (done, juce::Result::fail (error));
            return;
        }

        report (progress, 1.0, "Installed " + title);
        auto value = object();
        value.getDynamicObject()->setProperty ("path", target.getFullPathName());
        answer (done, juce::Result::ok(), value);
    });
}

void Cloud::fetchPreview (const juce::var& item, const juce::File& cache, Done done)
{
    run ([this, item, cache, done]
    {
        auto address = item.getProperty ("preview_url", {}).toString();
        const auto kind = item.getProperty ("kind", {}).toString();

        // Loops and samples can be heard as they are when they have no preview of their own.
        if (address.isEmpty() && (kind == "loop" || kind == "sample"))
            address = item.getProperty ("file_url", {}).toString();

        if (address.isEmpty())
        {
            answer (done, juce::Result::fail ("There's nothing to preview."));
            return;
        }

        const auto file = cache.getChildFile (juce::File::createLegalFileName (item.getProperty ("id", "item").toString()
                                                                                + "-" + item.getProperty ("version", "1").toString())
                                              + extensionOf (address));
        juce::String error;

        if (! file.existsAsFile() && ! download (address, file, error))
        {
            answer (done, juce::Result::fail (error));
            return;
        }

        auto value = object();
        value.getDynamicObject()->setProperty ("file", file.getFullPathName());
        answer (done, juce::Result::ok(), value);
    });
}

//==============================================================================
// MIDI tools
namespace
{
    /** A plain file name: no folders, nothing that climbs out of one. */
    bool isPlainName (const juce::String& name)
    {
        return name.isNotEmpty() && name.length() <= 120 && name.containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-")
            && ! name.startsWith (".");
    }

    bool isToolId (const juce::String& id)
    {
        return id.isNotEmpty() && id.length() <= 64 && id.containsOnly ("abcdefghijklmnopqrstuvwxyz0123456789._-") && ! id.startsWith (".");
    }
}

void Cloud::installMidiTool (const juce::var& tool, const juce::File& root, Done done, Progress progress)
{
    run ([this, tool, root, done, progress]
    {
        const auto result = installToolNow (tool, root, {}, progress);

        if (result.failed())
        {
            answer (done, result);
            return;
        }

        auto value = object();
        value.getDynamicObject()->setProperty ("id", tool.getProperty ("id", {}).toString());
        value.getDynamicObject()->setProperty ("path", root.getChildFile (tool.getProperty ("id", {}).toString()).getFullPathName());
        answer (done, juce::Result::ok(), value);
    });
}

juce::Result Cloud::installToolNow (juce::var tool, const juce::File& root, const juce::File& package, const Progress& progress)
{
    if (! tool.isObject())
        return juce::Result::fail ("That MIDI tool's definition couldn't be read.");

    const auto id = tool.getProperty ("id", {}).toString();
    const auto title = tool.getProperty ("title", id).toString();

    if (! isToolId (id))
        return juce::Result::fail ("That MIDI tool has no proper id.");

    root.createDirectory();
    const auto staging = root.getChildFile (id + "~download");   // "~" is in no tool id: never listed, never another tool's
    staging.deleteRecursively();
    const auto model = staging.getChildFile ("model");
    model.createDirectory();

    const auto fail = [&staging] (const juce::String& why)
    {
        staging.deleteRecursively();
        return juce::Result::fail (why);
    };

    juce::String error;

    if (package.existsAsFile())
    {
        // A package: tool.json and model\ (its files only, nothing outside the folder).
        const auto unpacked = unpack (package, staging);

        if (unpacked.failed())
            return fail ("The MIDI tool's package couldn't be opened.");

        if (! staging.getChildFile ("model").getChildFile ("weights_manifest.json").existsAsFile())
            return fail ("The MIDI tool's package has no model in it.");
    }
    else
    {
        // The checkpoint: config.json (Coconet has none), the manifest, then each weight file it names.
        const auto base = tool.getProperty ("checkpoint", {}).toString().trimCharactersAtEnd ("/");

        if (! base.startsWithIgnoreCase ("https://"))
            return fail ("That MIDI tool doesn't say where its model is.");

        report (progress, 0.0, "Downloading " + title);
        const bool needsConfig = tool.getProperty ("model", {}).toString() != "Coconet";

        if (! download (base + "/config.json", model.getChildFile ("config.json"), error) && needsConfig)
            return fail (error);

        if (! download (base + "/weights_manifest.json", model.getChildFile ("weights_manifest.json"), error))
            return fail (error);

        juce::StringArray files;
        const auto manifest = juce::JSON::parse (model.getChildFile ("weights_manifest.json"));

        if (const auto* groups = manifest.getArray())
            for (const auto& group : *groups)
                if (const auto* paths = group.getProperty ("paths", {}).getArray())
                    for (const auto& path : *paths)
                        files.addIfNotAlreadyThere (path.toString());

        if (files.isEmpty())
            return fail ("The model's manifest lists no weights.");

        for (const auto& name : files)
            if (! isPlainName (name))
                return fail ("The model's manifest names a file Sonify won't write.");

        for (int i = 0; i < files.size(); ++i)
        {
            if (blocked.load())
                return fail ("The internet is blocked.");

            report (progress, (i + 1.0) / (files.size() + 1.0), "Downloading " + title);

            if (! download (base + "/" + files[i], model.getChildFile (files[i]), error))
                return fail (error);
        }
    }

    // What it is, kept with it (the page reads it to know what the model does).
    auto definition = tool;

    if (const auto packaged = juce::JSON::parse (staging.getChildFile ("tool.json")); packaged.isObject() && package.existsAsFile())
    {
        definition = packaged;
        definition.getDynamicObject()->setProperty ("id", id);
        definition.getDynamicObject()->setProperty ("version", tool.getProperty ("version", packaged.getProperty ("version", "1")));
    }

    definition.getDynamicObject()->setProperty ("installed", juce::Time::getCurrentTime().toISO8601 (true));

    if (! staging.getChildFile ("tool.json").replaceWithText (juce::JSON::toString (definition)))
        return fail ("Couldn't write the MIDI tool into " + root.getFullPathName());

    const auto folder = root.getChildFile (id);
    folder.deleteRecursively();

    if (! staging.moveFileTo (folder))
        return fail ("Couldn't put the MIDI tool into " + root.getFullPathName());

    report (progress, 1.0, "Installed " + title);
    return juce::Result::ok();
}
