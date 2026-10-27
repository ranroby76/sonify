// D:\workspace\Fanan_web_hub\src\StorePublisher.cpp

#include "StorePublisher.h"
#include "Licences.h"
#include "StoreClient.h"
#include "Settings.h"

#include <juce_cryptography/juce_cryptography.h>

namespace
{
    constexpr const char* listingExtension = ".json";
    constexpr const char* pictureExtension = ".png";
    constexpr int pictureLongest = 1600;       // the store's picture, at most (pixels)
    constexpr size_t maxPngBytes = 4500000;    // ...and bytes: what Fanan AI looks at when the store checks it
    constexpr int storyPictureLongest = 1568;  // what Fanan AI is shown
    constexpr int maxFactsLength = 60000;

    juce::String limited (const juce::String& text, int length)
    {
        return text.trim().substring (0, length).trim();
    }

    juce::var object()
    {
        return juce::var (new juce::DynamicObject());
    }

    void set (juce::var& target, const juce::Identifier& name, const juce::var& value)
    {
        target.getDynamicObject()->setProperty (name, value);
    }

    juce::var textList (const juce::StringArray& texts)
    {
        juce::Array<juce::var> list;

        for (const auto& text : texts)
            list.add (text);

        return list;
    }

    juce::String kindName (const juce::String& kind)
    {
        if (kind == "effect")  return "effect";
        if (kind == "widget")  return "widget";
        if (kind == "midifx")  return "MIDI FX";
        return "instrument";
    }

    /** A temporary folder of its own, for one post's files (gone once it's posted). */
    juce::File makeWorkFolder()
    {
        auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("Sonify store " + juce::Uuid().toString().substring (0, 8));
        folder.deleteRecursively();
        return folder.createDirectory() ? folder : juce::File();
    }
}

//==============================================================================
juce::var StorePublisher::Listing::toVar() const
{
    const auto listOf = [] (const juce::StringArray& texts)
    {
        juce::Array<juce::var> list;

        for (const auto& text : texts)
            list.add (text);

        return list;
    };

    auto item = object();
    set (item, "key", key);
    set (item, "kind", kind);
    set (item, "ad", ad);
    set (item, "title", title);
    set (item, "tagline", tagline);
    set (item, "story", story);
    set (item, "section", section);
    set (item, "video", video);
    set (item, "website", website);
    set (item, "priceText", priceText);
    set (item, "features", listOf (features));
    set (item, "formats", listOf (formats));
    set (item, "offersDownload", offersDownload);
    set (item, "postedVersion", postedVersion);
    set (item, "postedAt", postedAt);
    return item;
}

StorePublisher::Listing StorePublisher::Listing::fromVar (const juce::var& value)
{
    Listing l;

    if (! value.isObject())
        return l;

    l.key = value.getProperty ("key", {}).toString().trim();
    l.kind = value.getProperty ("kind", {}).toString().trim();
    l.ad = value.getProperty ("ad", {}).toString().trim();
    l.title = limited (value.getProperty ("title", {}).toString(), maxTitle);
    l.tagline = limited (value.getProperty ("tagline", {}).toString(), maxTagline);
    l.story = limited (value.getProperty ("story", {}).toString(), maxStory);
    l.section = value.getProperty ("section", {}).toString().trim();
    l.video = limited (value.getProperty ("video", {}).toString(), 300);
    l.website = limited (value.getProperty ("website", {}).toString(), maxWebsite);
    l.priceText = limited (value.getProperty ("priceText", {}).toString(), maxPriceText);

    if (const auto* list = value.getProperty ("features", {}).getArray())
        for (const auto& feature : *list)
            if (const auto text = limited (feature.toString(), maxFeature); text.isNotEmpty() && l.features.size() < maxFeatures)
                l.features.add (text);

    if (const auto* list = value.getProperty ("formats", {}).getArray())
        for (const auto& format : *list)
            if (StoreClient::allFormats().contains (format.toString()))
                l.formats.addIfNotAlreadyThere (format.toString());

    l.offersDownload = (bool) value.getProperty ("offersDownload", false);
    l.postedVersion = l.ad.isNotEmpty() ? juce::jmax (0, (int) value.getProperty ("postedVersion", 0)) : 0;
    l.postedAt = value.getProperty ("postedAt", {}).toString();
    return l;
}

//==============================================================================
juce::File StorePublisher::folder()
{
    return Settings::getDataFolder().getChildFile ("Store");
}

StorePublisher::Listing StorePublisher::load (const juce::String& key, const juce::String& kind)
{
    const auto file = folder().getChildFile (juce::File::createLegalFileName (key) + listingExtension);
    const auto parsed = juce::JSON::parse (file);
    auto listing = Listing::fromVar (parsed);

    listing.key = key;
    listing.kind = kind;

    if (! StoreClient::sectionsFor (kind).contains (listing.section))
        listing.section.clear();

    return listing;
}

juce::Result StorePublisher::save (const Listing& listing)
{
    if (listing.key.isEmpty())
        return juce::Result::fail ("Nothing to keep it for.");

    const auto file = folder().getChildFile (juce::File::createLegalFileName (listing.key) + listingExtension);

    if (! file.getParentDirectory().createDirectory() || ! file.replaceWithText (juce::JSON::toString (listing.toVar())))
        return juce::Result::fail ("Couldn't keep the store's details in " + folder().getFullPathName());

    return juce::Result::ok();
}

juce::File StorePublisher::pictureFile (const juce::String& key)
{
    return folder().getChildFile (juce::File::createLegalFileName (key) + pictureExtension);
}

juce::Image StorePublisher::loadPicture (const juce::String& key)
{
    const auto file = pictureFile (key);
    return file.existsAsFile() ? juce::ImageFileFormat::loadFrom (file) : juce::Image();
}

juce::Result StorePublisher::savePicture (const juce::String& key, const juce::Image& picture)
{
    const auto file = pictureFile (key);

    if (! picture.isValid())
    {
        file.deleteFile();
        return juce::Result::ok();
    }

    const auto png = pngOf (picture, pictureLongest);

    if (png.isEmpty() || ! file.getParentDirectory().createDirectory() || ! file.replaceWithData (png.getData(), png.getSize()))
        return juce::Result::fail ("Couldn't keep the picture in " + folder().getFullPathName());

    return juce::Result::ok();
}

juce::File StorePublisher::logoFile()
{
    return folder().getChildFile ("developer-logo.png");
}

juce::Image StorePublisher::loadLogo()
{
    const auto file = logoFile();
    return file.existsAsFile() ? juce::ImageFileFormat::loadFrom (file) : juce::Image();
}

void StorePublisher::removeLogo()
{
    logoFile().deleteFile();
}

juce::Image StorePublisher::fitLogo (const juce::Image& source, juce::String& report)
{
    if (! source.isValid())
    {
        report = "That file isn't a picture Sonify can read.";
        return {};
    }

    auto image = source.convertedToFormat (juce::Image::ARGB);
    const auto w = image.getWidth(), h = image.getHeight();

    // What's around the logo: transparent pixels, or (a picture with no transparency) the plain
    // colour of its corners.
    bool transparent = false;
    juce::Colour corner;

    {
        const juce::Image::BitmapData data (image, juce::Image::BitmapData::readOnly);

        for (int y = 0; y < h && ! transparent; y += juce::jmax (1, h / 64))
            for (int x = 0; x < w && ! transparent; x += juce::jmax (1, w / 64))
                transparent = data.getPixelColour (x, y).getAlpha() < 250;

        corner = data.getPixelColour (0, 0);
    }

    const auto isEmpty = [&] (juce::Colour c)
    {
        if (transparent)
            return c.getAlpha() < 12;

        return std::abs ((int) c.getRed() - (int) corner.getRed()) < 24 && std::abs ((int) c.getGreen() - (int) corner.getGreen()) < 24
            && std::abs ((int) c.getBlue() - (int) corner.getBlue()) < 24;
    };

    int left = w, right = -1, top = h, bottom = -1;

    {
        const juce::Image::BitmapData data (image, juce::Image::BitmapData::readOnly);

        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                if (isEmpty (data.getPixelColour (x, y)))
                    continue;

                left = juce::jmin (left, x);
                right = juce::jmax (right, x);
                top = juce::jmin (top, y);
                bottom = juce::jmax (bottom, y);
            }
        }
    }

    if (right < left || bottom < top)
    {
        report = "The logo is empty: there's nothing on it but its background.";
        return {};
    }

    // A little room around it, then at most logoLongest pixels on its longer side.
    const auto pad = juce::jmax (2, juce::jmax (right - left, bottom - top) / 50);
    const auto crop = juce::Rectangle<int>::leftTopRightBottom (left - pad, top - pad, right + 1 + pad, bottom + 1 + pad)
                          .getIntersection (image.getBounds());
    auto logo = image.getClippedImage (crop).createCopy();
    const auto longest = juce::jmax (logo.getWidth(), logo.getHeight());

    if (longest > logoLongest)
    {
        const auto scale = (double) logoLongest / (double) longest;
        logo = logo.rescaled (juce::jmax (1, juce::roundToInt (logo.getWidth() * scale)), juce::jmax (1, juce::roundToInt (logo.getHeight() * scale)),
                              juce::Graphics::highResamplingQuality);
    }

    const auto size = [] (int a, int b) { return juce::String (a) + juce::String::fromUTF8 (" \xc3\x97 ") + juce::String (b); };

    report = "Your logo: " + size (w, h) + " px";

    if (crop != image.getBounds())
        report << ", trimmed to " << size (crop.getWidth(), crop.getHeight());

    report << ", kept at " << size (logo.getWidth(), logo.getHeight()) << ".";

    if (longest < logoSmall)
        report << " It's small, so it may look soft: one of " << logoSmall * 2 << " px or more looks sharper.";
    else if (! transparent)
        report << " A PNG with a transparent background sits best on the store's page.";

    return logo;
}

juce::Result StorePublisher::importLogo (const juce::File& file, juce::String& report)
{
    juce::Image source;

    if (file.hasFileExtension ("svg"))
    {
        // A drawing: drawn at twice the size kept, so it's sharp.
        if (auto drawable = juce::Drawable::createFromSVGFile (file))
        {
            const auto bounds = drawable->getDrawableBounds();

            if (! bounds.isEmpty())
            {
                const auto scale = (float) (logoLongest * 2) / juce::jmax (bounds.getWidth(), bounds.getHeight());
                source = juce::Image (juce::Image::ARGB, juce::jmax (1, juce::roundToInt (bounds.getWidth() * scale)),
                                      juce::jmax (1, juce::roundToInt (bounds.getHeight() * scale)), true);
                juce::Graphics g (source);
                drawable->drawWithin (g, source.getBounds().toFloat(), juce::RectanglePlacement::centred, 1.0f);
            }
        }
    }
    else
    {
        source = juce::ImageFileFormat::loadFrom (file);
    }

    const auto logo = fitLogo (source, report);

    if (! logo.isValid())
        return juce::Result::fail (report);

    const auto png = pngOf (logo, logoLongest);

    if (png.isEmpty() || ! logoFile().getParentDirectory().createDirectory() || ! logoFile().replaceWithData (png.getData(), png.getSize()))
        return juce::Result::fail ("Couldn't keep the logo in " + folder().getFullPathName());

    return juce::Result::ok();
}

juce::String StorePublisher::completeWebsite (const juce::String& typed)
{
    const auto text = typed.trim();

    if (text.isEmpty() || text.containsIgnoreCase ("://"))
        return text;

    return "https://" + text;
}

bool StorePublisher::isProtected (const juce::File& projectFolder)
{
    return projectFolder.isDirectory() && Licences::loadProtection (projectFolder).isValid();
}

juce::String StorePublisher::problemWith (const Listing& listing, const juce::Image& picture)
{
    if (listing.title.trim().isEmpty())
        return "Give it a title: its name on the board.";

    if (! StoreClient::sectionsFor (listing.kind).contains (listing.section))
        return "Choose its section: where people look for it.";

    if (listing.story.trim().isEmpty())
        return "Write its story (or let Fanan AI write it): what people read on the ad.";

    if (listing.website.trim().isEmpty())
        return "Type your website's address: where people buy it and get their serial.";

    if (! StoreClient::isWebsite (listing.website))
        return "The website's address doesn't look right: it starts with https:// (or http://), like https://www.example.com";

    if (listing.video.trim().isNotEmpty() && StoreClient::youtubeId (listing.video).isEmpty())
        return "The video link isn't a YouTube video's link (or leave it empty).";

    if (listing.formats.isEmpty() && ! listing.offersDownload)
        return "Tick the formats it comes in (VST3, CLAP, AU or Sonify).";

    if (! picture.isValid())
        return "Give it a picture: take the panel, or choose one.";

    return {};
}

//==============================================================================
StorePublisher::StorePublisher (FananServer& fananServer)
    : server (fananServer)
{
}

StorePublisher::~StorePublisher()
{
    alive->store (false);
    worker.removeAllJobs (true, 60000);
}

void StorePublisher::finishPost (const Posted& done, const juce::Result& result, const Listing& listing)
{
    posting = false;

    if (done != nullptr)
        done (result, listing);
}

void StorePublisher::post (Item item, Progress progress, Posted done)
{
    if (posting)
        return;

    const auto say = [progress] (const juce::String& step) { if (progress != nullptr) progress (step); };

    item.listing.website = completeWebsite (item.listing.website);

    if (const auto problem = problemWith (item.listing, item.picture); problem.isNotEmpty())
    {
        if (done != nullptr)
            done (juce::Result::fail (problem), item.listing);

        return;
    }

    if (! server.isSignedIn())
    {
        if (done != nullptr)
            done (juce::Result::fail ("Sign in to your Fanan account first (in Build with AI): the ads you post are your account's."), item.listing);

        return;
    }

    item.sendPackage = item.sendPackage && item.listing.offersDownload;

    // The panel's files as they are now: the studio may go on saving the project meanwhile.
    const auto work = makeWorkFolder();

    if (work == juce::File())
    {
        if (done != nullptr)
            done (juce::Result::fail ("Couldn't make a folder for the files to send."), item.listing);

        return;
    }

    if (item.sendPackage && ! item.isMidiFx)
    {
        const auto gui = work.getChildFile ("source gui");

        if (item.package.guiFolder.isDirectory() && item.package.guiFolder.copyDirectoryTo (gui))
            item.package.guiFolder = gui;
        else
            item.package.guiFolder = juce::File();

        const auto presets = work.getChildFile ("source presets.json");

        if (item.package.presetsFile.existsAsFile() && item.package.presetsFile.copyFileTo (presets))
            item.package.presetsFile = presets;
        else
            item.package.presetsFile = juce::File();
    }

    posting = true;

    const auto& l = item.listing;
    auto fields = object();
    set (fields, "key", l.key);
    set (fields, "kind", l.kind);
    set (fields, "title", l.title);
    set (fields, "package", item.sendPackage);

    if (l.ad.isNotEmpty())
        set (fields, "ad", l.ad);

    // First, Fanan AI reads what the ad says against the Store's rules (on the store's side, so
    // nothing harmful is sent at all): a post that breaks them stops here, with what broke which
    // rule. Its pictures are checked the same way when it's published, before it's on the board.
    auto words = object();
    set (words, "kind", l.kind);
    set (words, "section", l.section);
    set (words, "title", l.title);
    set (words, "tagline", l.tagline);
    set (words, "story", l.story);
    set (words, "features", textList (l.features));
    set (words, "website", l.website.trim());
    set (words, "priceText", l.priceText.trim());
    set (words, "maker", (item.maker.trim().isNotEmpty() ? item.maker.trim()
                                                         : (item.isMidiFx ? item.device.vendor : item.package.vendor).trim()).substring (0, 60));

    if (l.ad.isNotEmpty())
        set (words, "ad", l.ad);

    say (juce::String::fromUTF8 ("Fanan AI is checking the ad against the Store's rules\xe2\x80\xa6"));

    server.store ("check", words, [this, stillAlive = alive, item, work, say, done, fields] (const juce::Result& checked, const juce::var&) mutable
    {
        if (! stillAlive->load())
            return;

        if (checked.failed())
        {
            work.deleteRecursively();
            finishPost (done, checked, item.listing);
            return;
        }

        say (juce::String::fromUTF8 ("Asking the store\xe2\x80\xa6"));

    server.store ("begin", fields, [this, stillAlive, item, work, say, done] (const juce::Result& begun, const juce::var& answer) mutable
    {
        if (! stillAlive->load())
            return;

        if (begun.failed())
        {
            work.deleteRecursively();
            finishPost (done, begun, item.listing);
            return;
        }

        const auto adId = answer.getProperty ("ad", {}).toString().trim();
        const auto version = juce::jmax (1, (int) answer.getProperty ("version", item.listing.postedVersion + 1));
        const auto uploads = answer.getProperty ("uploads", {});
        const auto packageAddress = uploads.getProperty ("package", {}).toString();
        const auto pictureAddress = uploads.getProperty ("picture", {}).toString();
        const auto logoAddress = uploads.getProperty ("logo", {}).toString();

        if (adId.isEmpty() || adId.length() > 80 || ! adId.containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_."))
        {
            work.deleteRecursively();
            finishPost (done, juce::Result::fail ("The store's answer didn't make sense (its id for the ad). Please try again later."), item.listing);
            return;
        }

        if (pictureAddress.isEmpty() || (item.sendPackage && packageAddress.isEmpty()))
        {
            work.deleteRecursively();
            finishPost (done, juce::Result::fail ("The store didn't say where to send the files. Please try again later."), item.listing);
            return;
        }

        // The id is the ad's for good, from now on, posted or not (a new one when the old ad
        // was deleted).
        if (adId != item.listing.ad)
            item.listing.postedVersion = 0;

        item.listing.ad = adId;
        save (item.listing);

        if (item.sendPackage)
            say (! item.isMidiFx && isProtected (item.projectFolder) ? juce::String::fromUTF8 ("Building the Sonify version with its protection\xe2\x80\xa6")
                                                                     : juce::String::fromUTF8 ("Packing the Sonify version\xe2\x80\xa6"));

        auto sending = std::make_shared<Sending>();
        sending->listing = item.listing;
        sending->maker = item.maker.trim().isNotEmpty() ? item.maker.trim()
                                                        : (item.isMidiFx ? item.device.vendor : item.package.vendor).trim();
        sending->adId = adId;
        sending->version = version;
        sending->say = say;
        sending->done = done;
        sending->packageAddress = packageAddress;
        sending->pictureAddress = pictureAddress;
        sending->logoAddress = logoAddress;

        worker.addJob ([this, stillAlive, item, work, sending]
        {
            Prepared prepared;

            if (item.sendPackage)
                prepared = prepare (item, sending->adId, sending->version);

            sending->picture = pngOf (item.picture, pictureLongest);
            sending->logo = pngOf (loadLogo(), logoLongest);
            work.deleteRecursively();

            juce::MessageManager::callAsync ([this, stillAlive, sending, sendsPackage = item.sendPackage, prepared = std::move (prepared)]
            {
                if (! stillAlive->load())
                    return;

                if (prepared.error.isNotEmpty())
                {
                    finishPost (sending->done, juce::Result::fail (prepared.error), sending->listing);
                    return;
                }

                if (sending->picture.isEmpty())
                {
                    finishPost (sending->done, juce::Result::fail ("The picture couldn't be made ready. Choose it again."), sending->listing);
                    return;
                }

                if (sendsPackage)
                {
                    sending->package = prepared.package;
                    sendPackage (sending);
                }
                else
                {
                    sendPicture (sending);
                }
            });
        });
    });
    });
}

//==============================================================================
void StorePublisher::rules (std::function<void (const juce::Result&, const Rules&)> done)
{
    server.store ("rules", object(), [stillAlive = alive, done] (const juce::Result& result, const juce::var& answer)
    {
        if (! stillAlive->load() || done == nullptr)
            return;

        Rules rules;
        rules.accepted = (bool) answer.getProperty ("accepted", false);
        rules.banned = (bool) answer.getProperty ("banned", false);
        rules.version = juce::jmax (1, (int) answer.getProperty ("version", 1));
        rules.strikes = juce::jmax (0, (int) answer.getProperty ("strikes", 0));
        rules.text = answer.getProperty ("rules", {}).toString().trim().substring (0, 6000);

        if (result.wasOk() && ! rules.accepted && ! rules.banned && rules.text.isEmpty())
        {
            done (juce::Result::fail ("The store didn't send its rules. Please try again later."), rules);
            return;
        }

        done (result, rules);
    });
}

void StorePublisher::acceptRules (int version, std::function<void (const juce::Result&)> done)
{
    auto fields = object();
    set (fields, "version", version);

    server.store ("acceptRules", fields, [stillAlive = alive, done] (const juce::Result& result, const juce::var&)
    {
        if (stillAlive->load() && done != nullptr)
            done (result);
    });
}

void StorePublisher::sendPackage (std::shared_ptr<Sending> sending)
{
    sending->say (juce::String::fromUTF8 ("Sending the Sonify version\xe2\x80\xa6"));

    server.upload (sending->packageAddress, sending->package, "application/zip", [this, stillAlive = alive, sending] (const juce::Result& sent, const juce::var&)
    {
        if (! stillAlive->load())
            return;

        if (sent.failed())
        {
            finishPost (sending->done, sent, sending->listing);
            return;
        }

        sending->packageSent = true;
        sendPicture (sending);
    });
}

void StorePublisher::sendPicture (std::shared_ptr<Sending> sending)
{
    sending->say (juce::String::fromUTF8 ("Sending its picture\xe2\x80\xa6"));

    server.upload (sending->pictureAddress, sending->picture, "image/png", [this, stillAlive = alive, sending] (const juce::Result& sent, const juce::var&)
    {
        if (! stillAlive->load())
            return;

        if (sent.failed())
            finishPost (sending->done, sent, sending->listing);
        else if (sending->logoAddress.isNotEmpty() && ! sending->logo.isEmpty())
            sendLogo (sending);
        else
            publish (sending);
    });
}

void StorePublisher::sendLogo (std::shared_ptr<Sending> sending)
{
    sending->say (juce::String::fromUTF8 ("Sending your logo\xe2\x80\xa6"));

    server.upload (sending->logoAddress, sending->logo, "image/png", [this, stillAlive = alive, sending] (const juce::Result& sent, const juce::var&)
    {
        if (! stillAlive->load())
            return;

        if (sent.failed())
        {
            finishPost (sending->done, sent, sending->listing);
            return;
        }

        sending->logoSent = true;
        publish (sending);
    });
}

void StorePublisher::publish (std::shared_ptr<Sending> sending)
{
    sending->say (juce::String::fromUTF8 ("Putting it on the board\xe2\x80\xa6"));

    const auto& listing = sending->listing;
    const auto videoId = StoreClient::youtubeId (listing.video);

    const auto listOf = [] (const juce::StringArray& texts)
    {
        juce::Array<juce::var> list;

        for (const auto& text : texts)
            list.add (text);

        return list;
    };

    auto formats = listing.formats;

    if (listing.offersDownload)
        formats.addIfNotAlreadyThere ("Sonify");

    auto details = object();
    set (details, "ad", sending->adId);
    set (details, "version", sending->version);
    set (details, "key", listing.key);
    set (details, "kind", listing.kind);
    set (details, "title", listing.title);
    set (details, "tagline", listing.tagline);
    set (details, "story", listing.story);
    set (details, "section", listing.section);
    set (details, "video", videoId.isNotEmpty() ? "https://www.youtube.com/watch?v=" + videoId : juce::String());
    set (details, "features", listOf (listing.features));
    set (details, "website", listing.website.trim());
    set (details, "formats", listOf (formats));
    set (details, "priceText", listing.priceText.trim());
    set (details, "maker", sending->maker.substring (0, 60));
    set (details, "offersDownload", listing.offersDownload);
    set (details, "picture", true);
    set (details, "logo", sending->logoSent);

    if (sending->packageSent)
    {
        auto package = object();
        set (package, "size", (juce::int64) sending->package.getSize());
        set (package, "sha256", juce::SHA256 (sending->package).toHexString());
        set (details, "package", package);
    }

    server.store ("publish", details, [this, stillAlive = alive, sending] (const juce::Result& published, const juce::var&)
    {
        if (! stillAlive->load())
            return;

        auto posted = sending->listing;

        if (published.wasOk())
        {
            posted.postedVersion = sending->version;
            posted.postedAt = juce::Time::getCurrentTime().toISO8601 (true);
            save (posted);
        }

        finishPost (sending->done, published, posted);
    });
}

//==============================================================================
StorePublisher::Prepared StorePublisher::prepare (const Item& item, const juce::String& adId, int version)
{
    Prepared result;
    const auto work = makeWorkFolder();

    if (work == juce::File())
    {
        result.error = "Couldn't make a folder for the files to send.";
        return result;
    }

    if (item.isMidiFx)
    {
        // Its last build: its code and its conversation stay here. A protected one's
        // license.soni travels inside its file (DeviceFile).
        auto device = item.device;
        device.sources.clear();
        device.chat = juce::var();

        if (device.wasm.isEmpty())
        {
            result.error = "The MIDI FX hasn't been built yet: ask Fanan AI to build it first.";
        }
        else
        {
            const auto folder = work.getChildFile ("package");
            const auto file = folder.getChildFile (juce::File::createLegalFileName (device.name.trim().isNotEmpty() ? device.name.trim()
                                                                                                                   : juce::String ("MIDI FX"))
                                                   + Devices::fileExtension);

            if (! folder.createDirectory())
                result.error = "Couldn't make a folder for the files to send.";
            else if (const auto written = device.write (file); written.failed())
                result.error = written.getErrorMessage();
            else if (const auto zipped = zipFolder (folder, result.package); zipped.failed())
                result.error = zipped.getErrorMessage();
        }

        work.deleteRecursively();
        return result;
    }

    auto package = item.package;
    package.forOthers = true;   // its blobs by name only, never where they are on this computer

    // Protected: built again with its product's id and demo inside, with its license.soni
    // (MadePlugins::protect), so it plays its demo until the buyer registers it with the serial
    // from the maker's site.
    if (const auto protection = Licences::loadProtection (item.projectFolder); item.projectFolder.isDirectory() && protection.isValid())
    {
        if (const auto protectedNow = MadePlugins::protect (package, item.projectFolder, protection); protectedNow.failed())
        {
            result.error = protectedNow.getErrorMessage();
            work.deleteRecursively();
            return result;
        }
    }

    package.projectId = "store-" + adId;   // its own id in Sonify: never the poster's project

    const auto folder = work.getChildFile ("package");
    const auto name = package.name.trim().isNotEmpty() ? package.name.trim() : juce::String ("My Plugin");
    const auto target = folder.getChildFile (juce::File::createLegalFileName (name) + MadePlugins::folderExtension);

    if (! folder.createDirectory())
        result.error = "Couldn't make a folder for the files to send.";
    else if (const auto written = MadePlugins::writeFolder (package, target, version); written.failed())
        result.error = written.getErrorMessage();
    else if (const auto zipped = zipFolder (folder, result.package); zipped.failed())
        result.error = zipped.getErrorMessage();

    work.deleteRecursively();
    return result;
}

juce::Result StorePublisher::zipFolder (const juce::File& source, juce::MemoryBlock& out)
{
    juce::ZipFile::Builder zip;
    int count = 0;

    for (const auto& entry : juce::RangedDirectoryIterator (source, true, "*", juce::File::findFiles))
    {
        const auto file = entry.getFile();
        zip.addFile (file, 9, file.getRelativePathFrom (source).replaceCharacter ('\\', '/'));
        ++count;
    }

    if (count == 0)
        return juce::Result::fail ("There was nothing to send.");

    out.reset();
    juce::MemoryOutputStream stream (out, false);

    if (! zip.writeToStream (stream, nullptr))
        return juce::Result::fail ("Couldn't pack the files to send.");

    stream.flush();
    return juce::Result::ok();
}

juce::MemoryBlock StorePublisher::pngOf (const juce::Image& image, int longest)
{
    if (! image.isValid())
        return {};

    auto picture = image;
    const auto scale = (float) longest / (float) juce::jmax (picture.getWidth(), picture.getHeight());

    if (scale < 1.0f)
        picture = picture.rescaled (juce::jmax (1, juce::roundToInt ((float) picture.getWidth() * scale)),
                                    juce::jmax (1, juce::roundToInt ((float) picture.getHeight() * scale)),
                                    juce::Graphics::highResamplingQuality);

    // Small enough for Fanan AI to look at when the store checks it (at most maxPngBytes):
    // made smaller until it is.
    for (int tries = 0;; ++tries)
    {
        juce::MemoryOutputStream encoded;
        juce::PNGImageFormat().writeImageToStream (picture, encoded);

        if (encoded.getDataSize() <= maxPngBytes || tries >= 6 || juce::jmax (picture.getWidth(), picture.getHeight()) < 200)
            return encoded.getMemoryBlock();

        picture = picture.rescaled (juce::jmax (1, juce::roundToInt ((float) picture.getWidth() * 0.8f)),
                                    juce::jmax (1, juce::roundToInt ((float) picture.getHeight() * 0.8f)),
                                    juce::Graphics::highResamplingQuality);
    }
}

//==============================================================================
juce::String StorePublisher::storyGuide()
{
    return R"(You write ads for Sonify's Store, a notice board where Sonify users show the plugins and MIDI FX they made in Sonify's Plugins Creator Studio, with a link to their own website.

You get facts about one product (from its code: its modules, parameters, controls and what they do) and, usually, a picture of its panel. Write its ad from these alone.

Answer with one JSON object and nothing else:
{"tagline": "...", "story": "...", "features": ["...", "..."], "section": "..."}

- tagline: one line, at most 100 characters: what it is and why someone wants it.
- story: 90 to 220 words, two or three short paragraphs separated by a blank line. What it sounds like, what it's good for (styles, uses), how it plays. Warm and concrete, never hype.
- features: 3 to 6 short lines, at most 80 characters each, each a real thing it has (a module, a control, a mode).
- section: where it belongs on the board, exactly one of the sections listed in the request.
- Only what the facts and the picture show: never invent features, presets, formats, prices or numbers.
- Never mention prices, serials, licences, AI, code, WebAssembly or how it was made.
- The language of the product's name and of the poster's notes, if they wrote some; otherwise English.)";
}

bool StorePublisher::readStory (const juce::var& answer, Listing& story, juce::String& why)
{
    juce::String text = answer.getProperty ("reply", {}).toString();

    if (const auto* blocks = answer.getProperty ("content", {}).getArray())
        for (const auto& block : *blocks)
            if (block.getProperty ("type", {}).toString() == "text")
                text << block.getProperty ("text", {}).toString() << "\n";

    const auto start = text.indexOfChar ('{');
    const auto end = text.lastIndexOfChar ('}');

    if (start < 0 || end <= start)
    {
        why = "Fanan AI didn't write the story this time. Please try again.";
        return false;
    }

    const auto parsed = juce::JSON::parse (text.substring (start, end + 1));
    const auto storyText = limited (parsed.getProperty ("story", {}).toString(), maxStory);

    if (storyText.isEmpty())
    {
        why = "Fanan AI didn't write the story this time. Please try again.";
        return false;
    }

    story.story = storyText;
    story.tagline = limited (parsed.getProperty ("tagline", {}).toString(), maxTagline);

    if (const auto section = parsed.getProperty ("section", {}).toString().trim(); StoreClient::sectionsFor (story.kind).contains (section))
        story.section = section;
    story.features.clear();

    if (const auto* list = parsed.getProperty ("features", {}).getArray())
        for (const auto& feature : *list)
            if (const auto line = limited (feature.toString(), maxFeature); line.isNotEmpty() && story.features.size() < maxFeatures)
                story.features.add (line);

    return true;
}

void StorePublisher::writeStory (const juce::String& facts, const juce::Image& picture, const Listing& current, Written done)
{
    if (writing)
        return;

    if (! server.isSignedIn())
    {
        if (done != nullptr)
            done (juce::Result::fail ("Sign in to your Fanan account first (in Build with AI): Fanan AI's story uses credits."), current);

        return;
    }

    juce::Array<juce::var> content;

    if (const auto png = pngOf (picture, storyPictureLongest); ! png.isEmpty())
    {
        auto source = object();
        set (source, "type", "base64");
        set (source, "media_type", "image/png");
        set (source, "data", juce::Base64::toBase64 (png.getData(), png.getSize()));

        auto block = object();
        set (block, "type", "image");
        set (block, "source", source);
        content.add (block);
    }

    juce::String request;
    request << "Write the ad for \"" << current.title << "\", " << kindName (current.kind) << ".";

    if (const auto& sections = StoreClient::sectionsFor (current.kind); ! sections.isEmpty())
        request << "\nIts section, one of: " << sections.joinIntoString (" | ") << ".";

    if (current.story.trim().isNotEmpty() || current.tagline.trim().isNotEmpty())
        request << "\n\nThe poster's notes so far (keep what's true in them):\n" << current.tagline.trim() << "\n" << current.story.trim();

    auto text = object();
    set (text, "type", "text");
    set (text, "text", request);
    content.add (text);

    auto message = object();
    set (message, "role", "user");
    set (message, "content", content);

    auto body = object();
    set (body, "mode", "chat");
    set (body, "purpose", "store_story");
    set (body, "guide", storyGuide());
    set (body, "context", facts.substring (0, maxFactsLength));
    set (body, "messages", juce::Array<juce::var> { message });
    set (body, "tools", juce::Array<juce::var>());
    set (body, "request_id", juce::Uuid().toString());

    writing = true;

    server.stellaChat (body, [this, stillAlive = alive, current, done] (const juce::Result& result, const juce::var& answer)
    {
        if (! stillAlive->load())
            return;

        writing = false;
        auto story = current;

        if (result.failed())
        {
            if (done != nullptr)
                done (result, current);

            return;
        }

        juce::String why;

        if (! readStory (answer, story, why))
        {
            if (done != nullptr)
                done (juce::Result::fail (why), current);

            return;
        }

        if (done != nullptr)
            done (juce::Result::ok(), story);
    });
}
