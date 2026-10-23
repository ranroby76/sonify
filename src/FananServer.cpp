// D:\workspace\Fanan_web_hub\src\FananServer.cpp

#include "FananServer.h"

#include <juce_cryptography/juce_cryptography.h>

#include <random>

namespace
{
    // The Stella AI Cloud app on Base44 and its site: Fanan accounts and credits, shared with
    // Stella AI Studio. FananWebHub-server.json beside the program can name others (for testing
    // against another server).
    constexpr const char* builtInAppId   = "6ac03f5bd8e7cddad8252682";
    constexpr const char* builtInSiteUrl = "https://rugged-stella-studio-flow.base44.app";

    constexpr int probeTimeoutMs = 8000;
    constexpr int chatTimeoutMs  = 320000;   // an AI step can take minutes: Base44 allows 5, plus a margin
    constexpr int relayTimeoutMs = 660000;   // through the relay: it gives a building step up to 10 minutes
    constexpr int relayConnectMs = 30000;    // a relay that can't be reached within this: the server answers instead
    constexpr int onlineCheckMs  = 120000;
    constexpr int offlineCheckMs = 10000;
    constexpr int watchCheckMs   = 6000;
    constexpr int watchChecks    = 100;      // ten minutes: signing in and paying take a while
    constexpr juce::uint32 lostAfterMs = 30000;   // an AI step that fails later than this had reached the server
    constexpr int listenTimeoutMs = 30000;        // turning a few seconds of speech into words
    constexpr int remoteTimeoutMs = 10000;        // the phone remote's quick checks
    constexpr int storeTimeoutMs = 60000;         // the store's answers
    constexpr int uploadTimeoutMs = 120000;       // a plugin's package going up

    const char* const blockedText = "The internet is blocked (the switch at the top left). Turn it off to use your Fanan account.";

    juce::var object()
    {
        return juce::var (new juce::DynamicObject());
    }
}

//==============================================================================
FananServer::Settings FananServer::loadSettings()
{
    Settings loaded;
    loaded.appId = builtInAppId;
    loaded.siteUrl = builtInSiteUrl;

    const auto config = juce::JSON::parse (juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                                               .getSiblingFile ("FananWebHub-server.json"));

    if (const auto value = config.getProperty ("serverUrl", {}).toString().trim(); value.isNotEmpty())
        loaded.serverUrl = value.trimCharactersAtEnd ("/");

    if (const auto value = config.getProperty ("appId", {}).toString().trim(); value.isNotEmpty())
        loaded.appId = value;

    if (const auto value = config.getProperty ("siteUrl", {}).toString().trim(); value.isNotEmpty())
        loaded.siteUrl = value.trimCharactersAtEnd ("/");

    if (const auto* list = config.getProperty ("internetTestUrls", {}).getArray())
    {
        loaded.internetTestUrls.clear();

        for (const auto& address : *list)
            if (address.toString().trim().isNotEmpty())
                loaded.internetTestUrls.add (address.toString().trim());
    }

    return loaded;
}

//==============================================================================
FananServer::FananServer (Settings s)
    : settings (std::move (s))
{
    const auto saved = juce::JSON::parse (getIdentityFile());

    key = saved.getProperty ("key", {}).toString();
    computerCode = saved.getProperty ("account", {}).toString();
    lastEmail = saved.getProperty ("lastEmail", {}).toString();

    // Stella AI Studio on this computer signs in to the same accounts: until Sonify has signed
    // in itself, the email last used there is the one suggested.
    if (lastEmail.isEmpty())
        lastEmail = juce::JSON::parse (getStellaIdentityFile()).getProperty ("lastEmail", {}).toString().trim();

    // The key is made once and kept, so this computer stays known to the server (and signed in).
    if (! isValidKey (key))
    {
        key = makeKey();
        computerCode.clear();
        saveIdentity();
    }
}

FananServer::~FananServer()
{
    stopTimer();
    alive->store (false);

    // Waiting requests are broken off, so quitting never hangs on the network.
    {
        const juce::ScopedLock lock (streamsLock);

        for (auto* stream : activeStreams)
            stream->cancel();
    }

    aiWorkers.removeAllJobs (true, 4000);
    workers.removeAllJobs (true, 4000);
}

void FananServer::start()
{
    if (started.exchange (true))
        return;

    startCheck();
}

void FananServer::setBlocked (bool shouldBlock)
{
    if (blocked.exchange (shouldBlock) == shouldBlock)
        return;

    ++generation;   // a check under way when this changed is let go, and asked again

    if (shouldBlock)
    {
        stopTimer();

        // What's on its way is broken off: nothing more reaches the network.
        {
            const juce::ScopedLock lock (streamsLock);

            for (auto* stream : activeStreams)
                stream->cancel();
        }

        {
            const juce::ScopedLock lock (stateLock);
            status = Status::blocked;   // the account as last known stays shown
        }

        if (onStateChanged != nullptr)
            onStateChanged();

        return;
    }

    checkNow();
}

//==============================================================================
juce::File FananServer::getIdentityFile()
{
    // In the user's application data, not beside the program: reinstalling or updating
    // Sonify keeps the computer known (and signed in), and remembers the last email used.
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Fanan")
               .getChildFile ("Sonify")
               .getChildFile ("account.json");
}

juce::File FananServer::getStellaIdentityFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Fanan")
               .getChildFile ("Stella AI Studio")
               .getChildFile ("account.json");
}

void FananServer::saveIdentity() const
{
    auto saved = object();
    saved.getDynamicObject()->setProperty ("key", key);
    saved.getDynamicObject()->setProperty ("account", getComputerCode());
    saved.getDynamicObject()->setProperty ("lastEmail", getLastEmail());

    const auto file = getIdentityFile();
    file.getParentDirectory().createDirectory();

    juce::TemporaryFile temp (file);

    if (temp.getFile().replaceWithText (juce::JSON::toString (saved)))
        temp.overwriteTargetFileWithTemporary();
}

juce::String FananServer::makeKey()
{
    // 256 bits from the system's secure random source.
    std::random_device source;
    juce::String hex;

    for (int i = 0; i < 8; ++i)
        hex << juce::String::toHexString ((juce::uint32) source()).paddedLeft ('0', 8);

    return hex.toLowerCase();
}

bool FananServer::isValidKey (const juce::String& candidate)
{
    return candidate.length() == 64 && candidate.containsOnly ("0123456789abcdef");
}

juce::String FananServer::deviceHash()
{
    // Only a one-way hash leaves the computer, never the ID itself. Made exactly as Stella
    // AI Studio makes it, so one computer is one device to Fanan's server, whichever app asks.
    const auto id = juce::SystemStats::getUniqueDeviceID();

    if (id.isEmpty())
        return {};

    return juce::SHA256 (("stella-device:" + id).toUTF8()).toHexString();
}

juce::var FananServer::makeRequest (const juce::String& action) const
{
    auto body = object();
    body.getDynamicObject()->setProperty ("action", action);
    body.getDynamicObject()->setProperty ("token", key);
    return body;
}

//==============================================================================
FananServer::Status FananServer::getStatus() const
{
    const juce::ScopedLock lock (stateLock);
    return status;
}

int FananServer::getCredits() const
{
    const juce::ScopedLock lock (stateLock);
    return credits;
}

juce::String FananServer::getComputerCode() const
{
    const juce::ScopedLock lock (stateLock);
    return computerCode;
}

bool FananServer::isSignedIn() const
{
    const juce::ScopedLock lock (stateLock);
    return linked;
}

juce::String FananServer::getEmail() const
{
    const juce::ScopedLock lock (stateLock);
    return email;
}

juce::String FananServer::getLastEmail() const
{
    const juce::ScopedLock lock (stateLock);
    return lastEmail;
}

bool FananServer::isReady() const
{
    const juce::ScopedLock lock (stateLock);
    return status == Status::online && computerCode.isNotEmpty();
}

juce::String FananServer::getStatusText() const
{
    const juce::ScopedLock lock (stateLock);

    switch (status)
    {
        case Status::checking:    return juce::String::fromUTF8 ("Connecting\xe2\x80\xa6");
        case Status::notSetUp:    return "Online, but Fanan accounts aren't set up in this build yet";
        case Status::noInternet:  return "No internet connection";
        case Status::serverDown:  return "Fanan's server isn't responding";
        case Status::blocked:     return "Internet blocked";
        case Status::online:
            // Credits belong to the account: before signing in there's no balance to show.
            if (credits < 0 || ! linked)
                return "Online";

            return juce::String::fromUTF8 ("Online \xc2\xb7 ") + juce::String (credits) + (credits == 1 ? " credit" : " credits");
    }

    return {};
}

juce::URL FananServer::getStoreUrl() const
{
    return hasSite() ? juce::URL (settings.siteUrl + "/buy").withParameter ("app", "sonify") : juce::URL();
}

juce::String FananServer::getRemotePageUrl() const
{
    return hasSite() ? settings.siteUrl + "/sonify" : juce::String();
}

//==============================================================================
void FananServer::checkNow()
{
    stopTimer();

    {
        const juce::ScopedLock lock (stateLock);

        if (status != Status::online)
            status = blocked.load() ? Status::blocked : Status::checking;
    }

    if (onStateChanged != nullptr)
        onStateChanged();

    startCheck();
}

void FananServer::watchForChanges()
{
    fastChecksLeft = watchChecks;

    if (! checking.load() && ! blocked.load())
        startTimer (watchCheckMs);
}

void FananServer::timerCallback()
{
    stopTimer();
    startCheck();
}

void FananServer::startCheck()
{
    if (! started.load())
        return;

    if (checking.exchange (true))
        return;   // one is under way; its answer schedules the next

    workers.addJob ([this, stillAlive = alive, asked = generation]
    {
        if (stillAlive->load())
            runCheck (asked);
    });
}

void FananServer::runCheck (int asked)
{
    Snapshot snapshot;

    if (blocked.load())
    {
        snapshot.status = Status::blocked;   // nothing is tried
    }
    else if (! isConfigured())
    {
        snapshot.status = canReachInternet() ? Status::notSetUp : Status::noInternet;
    }
    else
    {
        // "hello" makes this computer known the first time, and reports its credits and
        // account (if it's signed in) after that: signing in at startup is just this.
        auto body = makeRequest ("hello");
        body.getDynamicObject()->setProperty ("device", deviceHash());
        body.getDynamicObject()->setProperty ("app", juce::String ("Sonify ") + JUCE_APPLICATION_VERSION_STRING);

        const auto response = post ("stellaAccount", body, probeTimeoutMs);

        if (blocked.load())
        {
            snapshot.status = Status::blocked;   // blocked while it was asking
        }
        else if (response.connected && response.status == 200 && (bool) response.body.getProperty ("ok", false))
        {
            snapshot.status = Status::online;
            snapshot.known = true;
            snapshot.credits = juce::jmax (0, (int) response.body.getProperty ("credits", 0));
            snapshot.code = response.body.getProperty ("account", {}).toString();
            snapshot.linked = (bool) response.body.getProperty ("linked", false);
            snapshot.email = response.body.getProperty ("email", {}).toString();
            snapshot.relay = response.body.getProperty ("relay", {}).toString().trim();
        }
        else if (response.connected)
        {
            snapshot.status = Status::serverDown;   // it answered, but not properly
        }
        else
        {
            snapshot.status = canReachInternet() ? Status::serverDown : Status::noInternet;
        }
    }

    deliver ([this, snapshot, asked]
    {
        checking.store (false);

        // Asked before this computer signed out or Block internet changed: what it found is
        // out of date, so it's asked again at once.
        if (asked != generation)
        {
            startCheck();
            return;
        }

        setState (snapshot);
    });
}

void FananServer::setState (const Snapshot& snapshot)
{
    bool identityChanged = false;

    {
        const juce::ScopedLock lock (stateLock);

        status = blocked.load() ? Status::blocked : snapshot.status;

        if (snapshot.known)
        {
            credits = snapshot.credits;
            // A dot or slash left at the end ("https://relay.workers.dev.") is the same address, but
            // a dot after the host fails the secure connection's name check: the relay would be
            // skipped, and long building steps sent straight to the server, past its time limit.
            auto relay = snapshot.relay.trim();

            while (relay.endsWithChar ('.') || relay.endsWithChar ('/'))
                relay = relay.dropLastCharacters (1);

            relayUrl = isUsableRelay (relay) ? relay : juce::String();
            linked = snapshot.linked;
            email = snapshot.linked ? snapshot.email : juce::String();

            if (snapshot.code.isNotEmpty() && snapshot.code != computerCode)
            {
                computerCode = snapshot.code;
                identityChanged = true;
            }

            // Remembered for next time: after signing out, Sonify suggests it.
            if (email.isNotEmpty() && email != lastEmail)
            {
                lastEmail = email;
                identityChanged = true;
            }
        }
    }

    if (identityChanged)
        saveIdentity();

    scheduleNextCheck();

    if (onStateChanged != nullptr)
        onStateChanged();
}

void FananServer::scheduleNextCheck()
{
    if (blocked.load())
        return;   // checked again when the internet is allowed

    if (fastChecksLeft > 0)
    {
        --fastChecksLeft;
        startTimer (watchCheckMs);
        return;
    }

    const auto current = getStatus();
    startTimer (current == Status::online || current == Status::notSetUp ? onlineCheckMs : offlineCheckMs);
}

//==============================================================================
juce::String FananServer::failureText (const Response& response, const juce::String& fallback)
{
    if (! response.connected)
        return "Sonify couldn't reach Fanan's server. Check the internet connection and try again.";

    const auto message = response.body.getProperty ("message", {}).toString().trim();
    return message.isNotEmpty() ? message : fallback;
}

void FananServer::signIn (bool thenBuy, const juce::String& emailHint, Done done)
{
    if (blocked.load())
    {
        if (done != nullptr)
            done (juce::Result::fail (blockedText), {});

        return;
    }

    if (! hasSite() || ! isConfigured())
    {
        if (done != nullptr)
            done (juce::Result::fail ("Signing in isn't set up in this build yet."), {});

        return;
    }

    workers.addJob ([this, stillAlive = alive, thenBuy, hint = emailHint.trim(), done]
    {
        if (! stillAlive->load())
            return;

        // A one-time link: it works once, for a few minutes, and only for this computer.
        const auto response = post ("stellaAccount", makeRequest ("link"), probeTimeoutMs);
        const auto linkToken = response.body.getProperty ("link", {}).toString();

        auto result = juce::Result::ok();

        if (! response.connected || response.status != 200 || ! (bool) response.body.getProperty ("ok", false) || linkToken.isEmpty())
            result = juce::Result::fail (failureText (response, "Signing in isn't available right now. Please try again."));

        deliver ([this, result, linkToken, thenBuy, hint, done]
        {
            auto outcome = result;

            if (outcome.wasOk())
            {
                // app=sonify: the site's pages speak of Sonify, not Stella AI Studio.
                auto url = juce::URL (settings.siteUrl + "/link").withParameter ("t", linkToken)
                                                                 .withParameter ("app", "sonify");

                if (hint.containsChar ('@'))
                    url = url.withParameter ("email", hint);   // the site can fill it in

                if (thenBuy)
                    url = url.withParameter ("next", "buy");

                if (url.launchInDefaultBrowser())
                    watchForChanges();   // the account shows up here within seconds of signing in
                else
                    outcome = juce::Result::fail ("The web browser couldn't be opened.");
            }

            if (done != nullptr)
                done (outcome, {});
        });
    });
}

void FananServer::signOut (Done done)
{
    if (blocked.load())
    {
        if (done != nullptr)
            done (juce::Result::fail (blockedText), {});

        return;
    }

    workers.addJob ([this, stillAlive = alive, done]
    {
        if (! stillAlive->load())
            return;

        const auto response = post ("stellaAccount", makeRequest ("unlink"), probeTimeoutMs);

        auto result = juce::Result::ok();

        if (! response.connected || response.status != 200 || ! (bool) response.body.getProperty ("ok", false))
            result = juce::Result::fail (failureText (response, "Signing out didn't work. Please try again."));

        deliver ([this, result, done]
        {
            if (result.wasOk())
            {
                // Signed out on the server: shown at once, and checked again (a check already
                // under way still saw the account, so its answer is let go).
                ++generation;

                {
                    const juce::ScopedLock lock (stateLock);
                    linked = false;
                    email.clear();
                    credits = -1;
                }

                checkNow();
            }

            if (done != nullptr)
                done (result, {});
        });
    });
}

//==============================================================================
void FananServer::chat (const juce::var& request, Done done)
{
    ask (chatFunction, false, request, std::move (done));
}

void FananServer::stellaChat (const juce::var& request, Done done)
{
    ask (builderFunction, true, request, std::move (done));
}

bool FananServer::isUsableRelay (const juce::String& address)
{
    // Only over HTTPS (or this computer, for testing).
    const juce::URL url (address);
    const auto host = url.getDomain().toLowerCase();

    return address.startsWithIgnoreCase ("https://")
        || (address.startsWithIgnoreCase ("http://") && (host == "127.0.0.1" || host == "localhost"));
}

void FananServer::ask (const juce::String& function, bool throughRelay, const juce::var& request, Done done)
{
    if (blocked.load())
    {
        if (done != nullptr)
            done (juce::Result::fail ("The internet is blocked (the switch at the top left), and Fanan AI works through it."), {});

        return;
    }

    auto body = request.isObject() ? request.clone() : object();
    body.getDynamicObject()->setProperty ("token", key);

    aiWorkers.addJob ([this, stillAlive = alive, body, done, function, throughRelay]
    {
        if (! stillAlive->load())
            return;

        juce::String relay;

        if (throughRelay)
        {
            const juce::ScopedLock lock (stateLock);
            relay = relayUrl;
        }

        // Through the relay when there is one; if it can't be reached at all, the server
        // answers as before (nothing was sent, so nothing is done twice).
        const auto sentAt = juce::Time::getMillisecondCounter();
        Response response;
        bool viaRelay = false;   // for a failure's code: which way it went

        if (relay.isNotEmpty())
        {
            response = postTo (juce::URL (relay), body, relayTimeoutMs);
            viaRelay = response.connected;

            if (! response.connected && ! blocked.load() && juce::Time::getMillisecondCounter() - sentAt < (juce::uint32) relayConnectMs)
                response = post (function, body, chatTimeoutMs);
        }
        else
        {
            response = post (function, body, chatTimeoutMs);
        }

        const auto waitedMs = juce::Time::getMillisecondCounter() - sentAt;
        const auto& answer = response.body;
        const auto error = answer.getProperty ("error", {}).toString();

        auto result = juce::Result::ok();
        bool checkAgain = false;

        if (blocked.load())
        {
            // Broken off by "Block internet": the server may have finished the step (and charged it).
            result = juce::Result::fail ("Stopped: the internet was blocked while Fanan AI was working.");
        }
        else if (! response.connected && waitedMs < lostAfterMs)
        {
            checkAgain = true;
            result = juce::Result::fail ("Not sent, and nothing was charged: Sonify couldn't reach Fanan's server. "
                                         "Send again once the connection is back.");
        }
        else if (! response.connected)
        {
            // It got there and was being answered: the server may have finished the step (and charged it).
            checkAgain = true;
            result = juce::Result::fail ("The connection to Fanan's server was lost while Fanan AI was working, so its answer "
                                         "didn't arrive (the step may still have used credits). Send again once the connection is back.");
        }
        else if (response.status == 402 || error == "no_credits")
        {
            result = juce::Result::fail ("You're out of credits. Use Buy credits to get more, then send again.");
        }
        else if (error == "sign_in" || error == "unknown")
        {
            checkAgain = true;   // signed out elsewhere (the site's account page): the account shows it
            result = juce::Result::fail (failureText (response, "Sign in to use Fanan AI."));
        }
        else if (response.status != 200 || ! (bool) answer.getProperty ("ok", false))
        {
            // An answer without words of its own (a server's error page, a cut-off answer) says
            // which way it went and its status, so it can be traced.
            auto text = failureText (response, "Fanan AI couldn't answer that. Please try again.");

            if (answer.getProperty ("message", {}).toString().trim().isEmpty())
                text << " (" << (viaRelay ? "relay-" : "server-") << response.status << ")";

            result = juce::Result::fail (text);
        }

        deliver ([this, result, answer, done, checkAgain]
        {
            if (answer.hasProperty ("credits"))
            {
                const juce::ScopedLock lock (stateLock);
                credits = juce::jmax (0, (int) answer.getProperty ("credits", 0));
            }

            if (checkAgain)
                checkNow();   // the warning shows at once, and the retries begin
            else if (onStateChanged != nullptr)
                onStateChanged();

            if (done != nullptr)
                done (result, answer);
        });
    });
}

//==============================================================================
void FananServer::transcribe (const juce::MemoryBlock& wav, Done done)
{
    if (blocked.load())
    {
        if (done != nullptr)
            done (juce::Result::fail ("The internet is blocked (the switch at the top left), and voice control works through it."), {});

        return;
    }

    auto body = object();
    body.getDynamicObject()->setProperty ("token", key);
    body.getDynamicObject()->setProperty ("format", "wav");
    body.getDynamicObject()->setProperty ("audio", juce::Base64::toBase64 (wav.getData(), wav.getSize()));

    workers.addJob ([this, stillAlive = alive, body, done]
    {
        if (! stillAlive->load())
            return;

        const auto response = post (listenFunction, body, listenTimeoutMs);
        const auto& answer = response.body;
        const auto error = answer.getProperty ("error", {}).toString();

        auto result = juce::Result::ok();
        bool checkAgain = false;

        if (blocked.load())
        {
            result = juce::Result::fail ("Stopped: the internet was blocked.");
        }
        else if (! response.connected)
        {
            checkAgain = true;
            result = juce::Result::fail ("Sonify couldn't reach Fanan's server. Check the internet connection.");
        }
        else if (response.status == 402 || error == "no_credits")
        {
            checkAgain = true;   // the credits show it, and voice control waits for more
            result = juce::Result::fail ("You're out of credits. Use Buy credits to get more.");
        }
        else if (error == "sign_in" || error == "unknown")
        {
            checkAgain = true;   // signed out elsewhere: the account shows it
            result = juce::Result::fail (failureText (response, "Sign in to use voice control."));
        }
        else if (response.status != 200 || ! (bool) answer.getProperty ("ok", false))
        {
            result = juce::Result::fail (failureText (response, "Voice control couldn't understand that right now."));
        }

        deliver ([this, result, answer, done, checkAgain]
        {
            if (checkAgain)
                checkNow();

            if (done != nullptr)
                done (result, answer);
        });
    });
}

//==============================================================================
void FananServer::remote (const juce::String& action, const juce::var& fields, Done done)
{
    if (blocked.load())
    {
        if (done != nullptr)
            done (juce::Result::fail ("The internet is blocked (the switch at the top left), and Sonify Remote works through it."), {});

        return;
    }

    auto body = fields.isObject() ? fields.clone() : object();
    body.getDynamicObject()->setProperty ("token", key);
    body.getDynamicObject()->setProperty ("action", action);

    workers.addJob ([this, stillAlive = alive, body, done]
    {
        if (! stillAlive->load())
            return;

        const auto response = post (remoteFunction, body, remoteTimeoutMs);
        const auto& answer = response.body;
        const auto error = answer.getProperty ("error", {}).toString();

        auto result = juce::Result::ok();
        bool checkAgain = false;

        if (blocked.load())
        {
            result = juce::Result::fail ("Stopped: the internet was blocked.");
        }
        else if (! response.connected)
        {
            checkAgain = true;
            result = juce::Result::fail ("Sonify couldn't reach Fanan's server. Check the internet connection.");
        }
        else if (error == "sign_in" || error == "unknown")
        {
            checkAgain = true;   // signed out elsewhere: the account shows it
            result = juce::Result::fail (failureText (response, "Sign in to use Sonify Remote."));
        }
        else if (response.status != 200 || ! (bool) answer.getProperty ("ok", false))
        {
            result = juce::Result::fail (failureText (response, "Sonify Remote couldn't reach your phone right now."));
        }

        deliver ([this, result, answer, done, checkAgain]
        {
            if (checkAgain)
                checkNow();

            if (done != nullptr)
                done (result, answer);
        });
    });
}

//==============================================================================
void FananServer::store (const juce::String& action, const juce::var& fields, Done done)
{
    if (blocked.load())
    {
        if (done != nullptr)
            done (juce::Result::fail ("The internet is blocked (the switch at the top left), and the store works through it."), {});

        return;
    }

    auto body = fields.isObject() ? fields.clone() : object();
    body.getDynamicObject()->setProperty ("token", key);
    body.getDynamicObject()->setProperty ("action", action);
    body.getDynamicObject()->setProperty ("device", deviceHash());   // the computer its licences name

    workers.addJob ([this, stillAlive = alive, body, done]
    {
        if (! stillAlive->load())
            return;

        const auto response = post (storeFunction, body, storeTimeoutMs);
        const auto& answer = response.body;
        auto result = juce::Result::ok();

        if (blocked.load())
            result = juce::Result::fail ("Stopped: the internet was blocked.");
        else if (! response.connected)
            result = juce::Result::fail ("Sonify couldn't reach the store. Check the internet connection.");
        else if (response.status != 200 || ! (bool) answer.getProperty ("ok", false))
            result = juce::Result::fail (failureText (response, "The store couldn't do that right now. Please try again."));

        deliver ([result, answer, done]
        {
            if (done != nullptr)
                done (result, answer);
        });
    });
}

void FananServer::upload (const juce::String& address, const juce::MemoryBlock& data, const juce::String& contentType, Done done)
{
    if (blocked.load())
    {
        if (done != nullptr)
            done (juce::Result::fail ("The internet is blocked (the switch at the top left)."), {});

        return;
    }

    workers.addJob ([this, stillAlive = alive, address, data, contentType, done]
    {
        if (! stillAlive->load())
            return;

        // The address exactly as the store signed it: its query stays in the address (parsed into
        // parameters, it would be moved into the body and the file would arrive spoiled).
        int httpStatus = 0;
        const auto url = juce::URL::createWithoutParsing (address).withPOSTData (data);
        const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                 .withHttpRequestCmd ("PUT")
                                 .withExtraHeaders ("Content-Type: " + contentType + "\r\n")
                                 .withConnectionTimeoutMs (uploadTimeoutMs)
                                 .withStatusCode (&httpStatus);

        auto stream = url.createInputStream (options);

        if (stream != nullptr)
            stream->readEntireStreamAsString();

        auto result = juce::Result::ok();

        if (blocked.load())
            result = juce::Result::fail ("Stopped: the internet was blocked.");
        else if (stream == nullptr || httpStatus == 0)
            result = juce::Result::fail ("Sonify couldn't reach the store to send the file. Check the internet connection.");
        else if (httpStatus < 200 || httpStatus >= 300)
            result = juce::Result::fail ("The store didn't take the file (" + juce::String (httpStatus) + "). Please try again.");

        auto answer = object();
        answer.getDynamicObject()->setProperty ("status", httpStatus);

        deliver ([result, answer, done]
        {
            if (done != nullptr)
                done (result, answer);
        });
    });
}

//==============================================================================
FananServer::Response FananServer::post (const juce::String& function, const juce::var& body, int timeoutMs)
{
    return postTo (juce::URL (settings.serverUrl + "/api/apps/" + settings.appId + "/functions/" + function), body, timeoutMs);
}

FananServer::Response FananServer::postTo (const juce::URL& address, const juce::var& body, int timeoutMs)
{
    const auto url = address.withPOSTData (juce::JSON::toString (body, true));

    juce::String headers;
    headers << "X-App-Id: " << settings.appId << "\r\n"
            << "Accept: application/json\r\n"
            << "Content-Type: application/json\r\n";

    return fetch (url, headers, timeoutMs, true);
}

FananServer::Response FananServer::fetch (const juce::URL& url, const juce::String& headers, int timeoutMs, bool isPost)
{
    Response response;

    if (! alive->load() || blocked.load())
        return response;

    juce::WebInputStream stream (url, isPost);
    stream.withExtraHeaders (headers)
          .withConnectionTimeout (timeoutMs)
          .withNumRedirectsToFollow (5);

    {
        const juce::ScopedLock lock (streamsLock);
        activeStreams.add (&stream);
    }

    if (alive->load() && ! blocked.load() && stream.connect (nullptr))
    {
        response.status = stream.getStatusCode();
        response.connected = response.status > 0;

        if (response.connected)
            response.body = juce::JSON::parse (stream.readEntireStreamAsString());
    }

    {
        const juce::ScopedLock lock (streamsLock);
        activeStreams.removeFirstMatchingValue (&stream);
    }

    return response;
}

bool FananServer::canReachInternet()
{
    for (const auto& address : settings.internetTestUrls)
    {
        const auto response = fetch (juce::URL (address), {}, probeTimeoutMs, false);

        if (response.connected && response.status >= 200 && response.status < 400)
            return true;
    }

    return false;
}

void FananServer::deliver (std::function<void()> fn)
{
    juce::MessageManager::callAsync ([stillAlive = alive, fn = std::move (fn)]
    {
        if (stillAlive->load())
            fn();
    });
}
