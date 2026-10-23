// D:\workspace\Fanan_web_hub\src\StoreClient.cpp

#include "StoreClient.h"
#include "CreatorPlugin.h"
#include "DeviceFiles.h"
#include "DeviceStudio.h"
#include "Settings.h"

#include <juce_cryptography/juce_cryptography.h>

#include <algorithm>

namespace
{
    constexpr int pictureTimeoutMs = 20000, packageTimeoutMs = 120000;
    constexpr juce::int64 maxPackage = 64 * 1024 * 1024, maxPicture = 8 * 1024 * 1024;
    constexpr juce::int64 maxUnpacked = 256 * 1024 * 1024;
    constexpr int maxEntries = 2000;

    juce::var object()
    {
        return juce::var (new juce::DynamicObject());
    }

    juce::var fields (std::initializer_list<std::pair<const char*, juce::var>> items)
    {
        auto o = object();

        for (const auto& [name, value] : items)
            o.getDynamicObject()->setProperty (name, value);

        return o;
    }

    juce::StringArray textList (const juce::var& value)
    {
        juce::StringArray list;

        if (const auto* array = value.getArray())
            for (const auto& each : *array)
                if (const auto text = each.toString().trim(); text.isNotEmpty())
                    list.add (text);

        return list;
    }

    /** A file from the store's files (public): its bytes, at most limit, or empty. */
    juce::MemoryBlock fetch (const juce::String& address, juce::int64 limit, int timeoutMs)
    {
        juce::MemoryBlock data;
        int status = 0;
        auto stream = juce::URL (address).createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                                 .withConnectionTimeoutMs (timeoutMs)
                                                                 .withStatusCode (&status));

        if (stream == nullptr || status < 200 || status >= 300)
            return {};

        const auto length = stream->getTotalLength();

        if (length > limit)
            return {};

        juce::MemoryOutputStream out (data, false);
        char buffer[65536];

        while (! stream->isExhausted())
        {
            const auto got = stream->read (buffer, (int) sizeof (buffer));

            if (got <= 0)
                break;

            out.write (buffer, (size_t) got);

            if ((juce::int64) out.getDataSize() > limit)
                return {};
        }

        out.flush();
        return data;
    }

    /** A zip entry's path that stays inside the folder it's unpacked into. */
    bool isSafePath (const juce::String& path)
    {
        return path.isNotEmpty() && ! path.startsWithChar ('/') && ! path.startsWithChar ('\\') && ! path.containsChar (':')
            && ! path.contains ("..") && path.length() < 400;
    }
}

//==============================================================================
const juce::StringArray& StoreClient::sectionsFor (const juce::String& kind)
{
    static const juce::StringArray instruments { "Synths", "Keys & Pianos", "Pads & Atmospheres", "Bass", "Drums & Percussion",
                                                 "Leads & Plucks", "Orchestral & Acoustic", "Experimental" };
    static const juce::StringArray effects { "Reverb & Space", "Delay & Echo", "Distortion & Saturation", "Dynamics", "EQ & Filters",
                                             "Modulation", "Pitch & Vocal", "Creative FX", "Mixing Utilities" };
    static const juce::StringArray midiFx { "Arpeggiators", "Chords & Harmony", "Rhythm & Patterns", "Generators", "MIDI Utilities" };
    static const juce::StringArray widgets { "Visualizers & Meters", "Tools", "Games & Fun" };
    static const juce::StringArray none;

    if (kind == "instrument")  return instruments;
    if (kind == "effect")      return effects;
    if (kind == "midifx")      return midiFx;
    if (kind == "widget")      return widgets;
    return none;
}

juce::String StoreClient::kindOfSection (const juce::String& section)
{
    for (const auto* kind : { "instrument", "effect", "midifx", "widget" })
        if (sectionsFor (kind).contains (section))
            return kind;

    return {};
}

const juce::StringArray& StoreClient::allFormats()
{
    static const juce::StringArray formats { "VST3", "CLAP", "AU", "Sonify" };
    return formats;
}

juce::String StoreClient::youtubeId (const juce::String& link)
{
    const auto isId = [] (const juce::String& text)
    {
        return text.length() == 11 && text.containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_");
    };

    const auto text = link.trim();

    if (isId (text))
        return text;

    const juce::URL url (text.startsWithIgnoreCase ("http") ? text : "https://" + text);
    const auto host = url.getDomain().toLowerCase();
    const auto path = url.getSubPath();
    juce::String id;

    if (host == "youtu.be" || host == "www.youtu.be")
        id = path.upToFirstOccurrenceOf ("/", false, false);
    else if (host.endsWith ("youtube.com") || host.endsWith ("youtube-nocookie.com"))
    {
        if (path == "watch")
        {
            const auto names = url.getParameterNames();
            const auto index = names.indexOf ("v");
            id = index >= 0 ? url.getParameterValues()[index] : juce::String();
        }
        else
        {
            for (const auto* prefix : { "shorts/", "embed/", "live/", "v/" })
                if (path.startsWith (prefix))
                    id = path.fromFirstOccurrenceOf (prefix, false, false).upToFirstOccurrenceOf ("/", false, false);
        }
    }

    id = id.upToFirstOccurrenceOf ("?", false, false).upToFirstOccurrenceOf ("&", false, false);
    return isId (id) ? id : juce::String();
}

bool StoreClient::isWebsite (const juce::String& link)
{
    const auto text = link.trim();

    if (text.length() > 300 || text.containsAnyOf (" \t\r\n\"'<>\\@`"))
        return false;

    juce::String rest;

    if (text.startsWithIgnoreCase ("https://"))
        rest = text.substring (8);
    else if (text.startsWithIgnoreCase ("http://"))
        rest = text.substring (7);
    else
        return false;

    // The host: up to the path, the query or the anchor, without a port.
    auto host = rest;

    for (const auto* end : { "/", "?", "#" })
        host = host.upToFirstOccurrenceOf (end, false, false);

    host = host.upToFirstOccurrenceOf (":", false, false).toLowerCase();

    return host.length() >= 4 && host.contains (".") && ! host.startsWithChar ('.') && ! host.endsWithChar ('.')
        && ! host.contains ("..") && host.containsOnly ("abcdefghijklmnopqrstuvwxyz0123456789.-");
}

//==============================================================================
juce::String StoreClient::Ad::kindName() const
{
    if (kind == "effect")  return "Effect";
    if (kind == "widget")  return "Widget";
    if (kind == "midifx")  return "MIDI FX";
    return "Instrument";
}

StoreClient::Ad StoreClient::Ad::fromVar (const juce::var& value)
{
    Ad ad;

    if (! value.isObject())
        return ad;

    ad.id = value.getProperty ("ad", {}).toString().trim();
    ad.kind = value.getProperty ("kind", "instrument").toString();
    ad.title = value.getProperty ("title", {}).toString();
    ad.tagline = value.getProperty ("tagline", {}).toString();
    ad.story = value.getProperty ("story", {}).toString();
    ad.maker = value.getProperty ("maker", {}).toString();
    ad.section = value.getProperty ("section", {}).toString();
    ad.pictureUrl = value.getProperty ("picture", {}).toString();
    ad.logoUrl = value.getProperty ("logo", {}).toString();
    ad.video = youtubeId (value.getProperty ("video", {}).toString()).isNotEmpty() ? value.getProperty ("video", {}).toString() : juce::String();
    ad.website = isWebsite (value.getProperty ("website", {}).toString()) ? value.getProperty ("website", {}).toString().trim() : juce::String();
    ad.priceText = value.getProperty ("priceText", {}).toString().trim();
    ad.features = textList (value.getProperty ("features", {}));
    ad.offersDownload = (bool) value.getProperty ("offersDownload", false);
    ad.version = (int) value.getProperty ("version", 0);
    ad.created = (juce::int64) value.getProperty ("created", 0);
    ad.updated = (juce::int64) value.getProperty ("updated", 0);
    ad.mine = (bool) value.getProperty ("mine", false);
    ad.hidden = (bool) value.getProperty ("hidden", false);
    ad.key = value.getProperty ("key", {}).toString().trim();

    for (const auto& format : textList (value.getProperty ("formats", {})))
        if (allFormats().contains (format))
            ad.formats.addIfNotAlreadyThere (format);

    if (const auto package = value.getProperty ("package", {}); package.isObject())
    {
        ad.package.url = package.getProperty ("url", {}).toString().trim();
        ad.package.sha256 = package.getProperty ("sha256", {}).toString().trim().toLowerCase();
        ad.package.size = (juce::int64) package.getProperty ("size", 0);
        ad.package.version = juce::jmax (1, (int) package.getProperty ("version", 1));
    }

    // The ad's id: what downloads and their folders are named after (letters, digits, '-', '_', '.').
    if (ad.id.length() > 80 || ! ad.id.containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_."))
        ad.id.clear();

    return ad;
}

//==============================================================================
StoreClient::StoreClient (FananServer& fananServer)
    : server (fananServer)
{
}

StoreClient::~StoreClient()
{
    alive->store (false);
    workers.removeAllJobs (true, 30000);
}

void StoreClient::call (const juce::String& action, juce::var body, FananServer::Done done)
{
    server.store (action, body, [stillAlive = alive, done] (const juce::Result& result, const juce::var& answer)
    {
        if (stillAlive->load() && done != nullptr)
            done (result, answer);
    });
}

//==============================================================================
void StoreClient::listFrom (const juce::String& action, juce::var body, bool mineToo, Listed done)
{
    call (action, body, [mineToo, done] (const juce::Result& result, const juce::var& answer)
    {
        Page found;

        if (result.wasOk())
        {
            if (const auto* list = answer.getProperty ("ads", {}).getArray())
            {
                for (const auto& value : *list)
                {
                    if (auto ad = Ad::fromVar (value); ad.isValid())
                    {
                        ad.mine = ad.mine || mineToo;
                        found.ads.push_back (std::move (ad));
                    }
                }
            }

            found.page = juce::jmax (1, (int) answer.getProperty ("page", 1));
            found.pages = juce::jmax (1, (int) answer.getProperty ("pages", 1));
            found.total = juce::jmax ((int) found.ads.size(), (int) answer.getProperty ("total", 0));
        }

        if (done != nullptr)
            done (result, found);
    });
}

void StoreClient::list (const juce::String& kind, const juce::String& section, const juce::String& words, int page, Listed done)
{
    listFrom ("list", fields ({ { "kind", kind }, { "section", section }, { "query", words.trim() }, { "page", juce::jmax (1, page) } }),
              false, std::move (done));
}

void StoreClient::mine (Listed done)
{
    listFrom ("mine", object(), true, std::move (done));
}

void StoreClient::get (const juce::String& adId, std::function<void (const juce::Result&, const Ad&)> done)
{
    call ("get", fields ({ { "ad", adId } }), [done] (const juce::Result& result, const juce::var& answer)
    {
        if (done != nullptr)
            done (result, result.wasOk() ? Ad::fromVar (answer.getProperty ("ad", {})) : Ad());
    });
}

//==============================================================================
juce::File StoreClient::pictureCacheFile (const Ad& ad, const juce::String& part)
{
    return Settings::getCacheFolder().getChildFile ("Store pictures")
               .getChildFile (juce::File::createLegalFileName (ad.id) + "-v" + juce::String (ad.version) + part + ".png");
}

void StoreClient::picture (const Ad& ad, std::function<void (const juce::Image&)> done)
{
    fetchImage (ad.pictureUrl, ad.id + "-v" + juce::String (ad.version), pictureCacheFile (ad, {}), std::move (done));
}

void StoreClient::logo (const Ad& ad, std::function<void (const juce::Image&)> done)
{
    if (ad.logoUrl.isEmpty())
    {
        if (done != nullptr)
            done ({});

        return;
    }

    fetchImage (ad.logoUrl, ad.id + "-v" + juce::String (ad.version) + "-logo", pictureCacheFile (ad, "-logo"), std::move (done));
}

void StoreClient::fetchImage (const juce::String& address, const juce::String& key, const juce::File& cacheFile,
                              std::function<void (const juce::Image&)> done)
{
    if (const auto known = pictures.find (key); known != pictures.end())
    {
        if (done != nullptr)
            done (known->second);

        return;
    }

    auto& waiting = waitingPictures[key];
    waiting.push_back (std::move (done));

    if (waiting.size() > 1)
        return;   // already on its way

    const bool online = ! server.isBlocked();

    workers.addJob ([this, stillAlive = alive, key, cacheFile, address, online]
    {
        auto image = cacheFile.existsAsFile() ? juce::ImageFileFormat::loadFrom (cacheFile) : juce::Image();

        if (! image.isValid() && online && address.startsWithIgnoreCase ("http"))
        {
            const auto data = fetch (address, maxPicture, pictureTimeoutMs);
            image = juce::ImageFileFormat::loadFrom (data.getData(), data.getSize());

            if (image.isValid() && cacheFile.getParentDirectory().createDirectory())
                cacheFile.replaceWithData (data.getData(), data.getSize());
        }

        juce::MessageManager::callAsync ([this, stillAlive, key, image]
        {
            if (! stillAlive->load())
                return;

            if (image.isValid())
                pictures[key] = image;

            auto callbacks = std::move (waitingPictures[key]);
            waitingPictures.erase (key);

            for (auto& callback : callbacks)
                if (callback != nullptr)
                    callback (image);
        });
    });
}

//==============================================================================
int StoreClient::installedVersion (const Ad& ad) const
{
    if (! ad.isValid())
        return 0;

    if (ad.isMidiFx())
    {
        if (deviceStudio == nullptr)
            return 0;

        const auto* entry = deviceStudio->findInLibrary (deviceIdFor (ad.id));
        return entry != nullptr ? juce::jmax (1, entry->version) : 0;
    }

    return MadePlugins::installedVersion (MadePlugins::storeId (ad.id));
}

bool StoreClient::isInstalling (const juce::String& adId) const
{
    return std::find (installing.begin(), installing.end(), adId) != installing.end();
}

void StoreClient::install (const Ad& ad, Progress progress, Done done)
{
    const auto say = [progress] (const juce::String& step) { if (progress != nullptr) progress (step); };

    if (! ad.isValid() || isInstalling (ad.id))
        return;

    if (! ad.offersDownload)
    {
        if (done != nullptr)
            done (juce::Result::fail ("This ad doesn't offer a download: get it from its maker's website."));

        return;
    }

    if (ad.isMidiFx() && deviceStudio == nullptr)
    {
        if (done != nullptr)
            done (juce::Result::fail ("MIDI FX can't be added in this Sonify: its MIDI devices aren't available."));

        return;
    }

    installing.push_back (ad.id);
    say (juce::String::fromUTF8 ("Asking the store\xe2\x80\xa6"));

    const auto finish = [this, adId = ad.id, done] (const juce::Result& result)
    {
        installing.erase (std::remove (installing.begin(), installing.end(), adId), installing.end());

        if (done != nullptr)
            done (result);
    };

    // The ad as it is now: its maker may have posted a newer download, or stopped offering one.
    get (ad.id, [this, say, finish] (const juce::Result& asked, const Ad& fresh)
    {
        if (asked.failed())
        {
            finish (asked);
            return;
        }

        if (! fresh.isValid() || ! fresh.canDownload())
        {
            finish (juce::Result::fail ("The maker doesn't offer the Sonify version for download any more. Visit their website."));
            return;
        }

        download (fresh, say, finish);
    });
}

void StoreClient::download (const Ad& ad, Progress say, Done finish)
{
    if (server.isBlocked())
    {
        finish (juce::Result::fail ("The internet is blocked (the switch at the top left)."));
        return;
    }

    say (juce::String::fromUTF8 ("Downloading\xe2\x80\xa6"));

    workers.addJob ([this, stillAlive = alive, ad, finish]
    {
        Unpacked unpacked;
        const auto package = fetch (ad.package.url, maxPackage, packageTimeoutMs);

        if (package.isEmpty())
            unpacked.error = "The download didn't arrive. Check the internet connection and try again.";
        else if ((ad.package.size > 0 && (juce::int64) package.getSize() != ad.package.size)
                 || (ad.package.sha256.isNotEmpty() && juce::SHA256 (package).toHexString() != ad.package.sha256))
            unpacked.error = "The download arrived damaged. Please try again.";
        else
            unpacked = unpack (package, ad);

        juce::MessageManager::callAsync ([this, stillAlive, ad, unpacked, finish]
        {
            if (! stillAlive->load())
            {
                unpacked.folder.deleteRecursively();
                return;
            }

            finishInstall (ad, unpacked, finish);
        });
    });
}

StoreClient::Unpacked StoreClient::unpack (const juce::MemoryBlock& package, const Ad& ad)
{
    Unpacked result;
    juce::MemoryInputStream in (package, false);
    juce::ZipFile zip (in);
    juce::int64 total = 0;

    if (zip.getNumEntries() == 0 || zip.getNumEntries() > maxEntries)
    {
        result.error = "The download isn't one Sonify can read.";
        return result;
    }

    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        const auto* entry = zip.getEntry (i);
        total += entry->uncompressedSize;

        if (! isSafePath (entry->filename) || total > maxUnpacked)
        {
            result.error = "The download isn't one Sonify can read.";
            return result;
        }
    }

    result.folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Sonify store " + juce::Uuid().toString().substring (0, 8));
    const auto files = result.folder.getChildFile ("package");

    if (! files.createDirectory() || zip.uncompressTo (files, true).failed())
    {
        result.error = "The download couldn't be unpacked.";
        return result;
    }

    const auto version = juce::jmax (1, ad.package.version);

    if (ad.isMidiFx())
    {
        // A MIDI FX: its file, as this ad's (its id from the ad's), without code or conversation.
        // Its license.soni, when it's protected, travels inside its file (DeviceFile).
        const auto found = files.findChildFiles (juce::File::findFiles, false, juce::String ("*") + Devices::fileExtension);
        Devices::DeviceFile device;

        if (found.size() != 1 || device.read (found.getFirst()).failed() || device.wasm.isEmpty())
        {
            result.error = "The download isn't a MIDI FX Sonify can play.";
            return result;
        }

        device.id = deviceIdFor (ad.id);
        device.version = version;
        device.sources.clear();
        device.chat = juce::var();
        device.builtFrom.clear();

        if (device.vendor.trim().isEmpty())
            device.vendor = ad.maker;

        result.name = device.name;
        result.device = result.folder.getChildFile (juce::File::createLegalFileName (device.name.isNotEmpty() ? device.name : juce::String ("MIDI FX"))
                                                    + Devices::fileExtension);

        if (const auto written = device.write (result.device); written.failed())
            result.error = written.getErrorMessage();

        return result;
    }

    // A plugin: its folder (with its license.soni when it's protected), as this ad's.
    const auto folders = files.findChildFiles (juce::File::findDirectories, false, juce::String ("*") + MadePlugins::extension);
    const auto info = folders.size() == 1 ? folders.getFirst().getChildFile ("plugin.json") : juce::File();
    auto json = info.existsAsFile() ? juce::JSON::parse (info) : juce::var();

    if (auto* o = json.getDynamicObject(); o != nullptr && json.getProperty ("format", {}).toString() == "Sonify plugin")
    {
        o->setProperty ("id", MadePlugins::storeId (ad.id));
        o->setProperty ("project", "store-" + ad.id);
        o->setProperty ("version", version);
        o->setProperty ("added", juce::Time::getCurrentTime().toISO8601 (true));
        o->removeProperty ("store");   // the old store's sale details: nothing is sold any more

        result.name = json.getProperty ("name", "Store plugin").toString();
        result.plugin = folders.getFirst();

        if (! info.replaceWithText (juce::JSON::toString (json)))
            result.error = "The download couldn't be unpacked.";
    }
    else
    {
        result.error = "The download isn't a plugin made in Sonify.";
    }

    return result;
}

void StoreClient::finishInstall (const Ad& ad, Unpacked unpacked, Done done)
{
    auto result = juce::Result::ok();

    if (unpacked.error.isNotEmpty())
    {
        result = juce::Result::fail (unpacked.error);
    }
    else if (unpacked.device != juce::File())
    {
        juce::File copied;

        if (deviceStudio == nullptr)
            result = juce::Result::fail ("MIDI FX can't be added in this Sonify: its MIDI devices aren't available.");
        else
            result = deviceStudio->import (unpacked.device, copied);

        if (result.wasOk() && onDeviceInstalled != nullptr)
            onDeviceInstalled (unpacked.name);
    }
    else
    {
        juce::File folder;
        bool replaced = false;
        result = MadePlugins::installUnpacked (unpacked.plugin, MadePlugins::storeId (ad.id), folder, replaced);

        if (result.wasOk() && onPluginInstalled != nullptr)
            onPluginInstalled (folder, unpacked.name.isNotEmpty() ? unpacked.name : ad.title, replaced);
    }

    unpacked.folder.deleteRecursively();

    if (done != nullptr)
        done (result);
}

//==============================================================================
void StoreClient::hide (const Ad& ad, bool hidden, Done done)
{
    call (hidden ? "hide" : "unhide", fields ({ { "ad", ad.id } }), [done] (const juce::Result& result, const juce::var&)
    {
        if (done != nullptr)
            done (result);
    });
}

void StoreClient::remove (const Ad& ad, Done done)
{
    call ("delete", fields ({ { "ad", ad.id } }), [done] (const juce::Result& result, const juce::var&)
    {
        if (done != nullptr)
            done (result);
    });
}
