// D:\workspace\Fanan_web_hub\src\PhoneRemote.cpp

#include "PhoneRemote.h"
#include "Preferences.h"

#include <atomic>
#include <deque>
#include <mutex>

namespace
{
    constexpr int maxShortcutChars = 300;      // a shortcut is a command, not an essay: 60 of them fit the state
    constexpr int seenMs = 8000;               // a phone heard from within this long is "there"
    constexpr int maxHeaderBytes = 8192;
    constexpr int maxBodyBytes = 16384;
    constexpr int readTimeoutMs = 3000;

    juce::File settingsFile()
    {
        return SettingsFolder::file ("phone-remote.json");
    }

    juce::String newKey()
    {
        return juce::Uuid().toString();   // 32 hex digits from the system's randomness
    }

    /** This computer's address on the local network: a private IPv4 one (192.168.x.x first,
        then 10.x.x.x, then 172.16-31.x.x), else any other it has (not its own loopback or a
        self-assigned one); none when it has none. */
    juce::IPAddress localAddress()
    {
        juce::IPAddress best;
        int bestRank = 0;

        for (const auto& address : juce::IPAddress::getAllAddresses (false))
        {
            if (address.isIPv6)
                continue;

            const auto a = address.address[0], b = address.address[1];

            if (a == 127 || a == 0 || (a == 169 && b == 254))
                continue;

            const int rank = a == 192 && b == 168           ? 4
                           : a == 10                         ? 3
                           : a == 172 && b >= 16 && b <= 31  ? 2
                                                             : 1;

            if (rank > bestRank)
            {
                best = address;
                bestRank = rank;
            }
        }

        return best;
    }

    juce::var parseJson (const juce::String& text)
    {
        juce::var parsed;
        return juce::JSON::parse (text, parsed).wasOk() ? parsed : juce::var();
    }
}

//==============================================================================
/** The local page's server: a small HTTP server on the local network, on a thread of its own.
    It answers from what the message thread left for it (the page, the state), and leaves the
    phones' requests for the message thread to take. */
class PhoneRemote::LocalServer final : private juce::Thread
{
public:
    struct Request
    {
        juce::String id, kind, text, name;
        bool spoken = false;
    };

    LocalServer() : juce::Thread ("Sonify Remote (local)") {}

    ~LocalServer() override
    {
        stop();
    }

    /** Listens on the first free port from firstLocalPort; false when none is. */
    bool start (const juce::MemoryBlock& pageFile, const juce::MemoryBlock& logoFile, const juce::String& key, const juce::String& computerName)
    {
        {
            const std::lock_guard<std::mutex> lock (mutex);
            page = pageFile;
            logo = logoFile;
            pairingKey = key;
            name = computerName;
        }

        for (int candidate = firstLocalPort; candidate < firstLocalPort + 10; ++candidate)
        {
            auto socket = std::make_unique<juce::StreamingSocket>();

            if (socket->createListener (candidate))
            {
                listener = std::move (socket);
                port = candidate;
                startThread();
                return true;
            }
        }

        return false;
    }

    void stop()
    {
        if (listener != nullptr)
            listener->close();   // its wait for the next phone returns

        stopThread (4000);
        listener = nullptr;
        port = 0;
    }

    int getPort() const noexcept     { return port; }

    void setKey (const juce::String& key)
    {
        const std::lock_guard<std::mutex> lock (mutex);
        pairingKey = key;
    }

    /** The conversation, when it changed (the message thread). */
    void setState (const juce::String& newState)
    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (newState != state)
        {
            state = newState;
            ++stateAt;
        }
    }

    /** What the phones asked for since last time, oldest first (the message thread). */
    std::vector<Request> takeRequests()
    {
        const std::lock_guard<std::mutex> lock (mutex);
        std::vector<Request> taken (requests.begin(), requests.end());
        requests.clear();
        return taken;
    }

    juce::int64 lastSeenMs() const noexcept    { return lastSeen.load(); }

private:
    void run() override
    {
        while (! threadShouldExit() && listener != nullptr)
        {
            std::unique_ptr<juce::StreamingSocket> client (listener->waitForNextConnection());

            if (client == nullptr || threadShouldExit())
                break;

            // A phone asks every second or so, and each answer is ready: one at a time is enough.
            serve (*client);
            client->close();
        }
    }

    //==============================================================================
    void serve (juce::StreamingSocket& client)
    {
        juce::MemoryBlock received;
        int headerEnd = -1;

        // The request line and headers.
        while (headerEnd < 0 && received.getSize() < (size_t) maxHeaderBytes)
        {
            if (client.waitUntilReady (true, readTimeoutMs) != 1)
                return;

            char chunk[2048];
            const auto got = client.read (chunk, (int) sizeof (chunk), false);

            if (got <= 0)
                return;

            received.append (chunk, (size_t) got);
            headerEnd = received.toString().indexOf ("\r\n\r\n");
        }

        if (headerEnd < 0)
            return;

        const auto all = received.toString();
        const auto head = all.substring (0, headerEnd);
        const auto lines = juce::StringArray::fromLines (head);
        const auto requestLine = juce::StringArray::fromTokens (lines[0], " ", "");
        const auto method = requestLine[0].toUpperCase();
        const auto target = requestLine[1];

        int contentLength = 0;

        for (int i = 1; i < lines.size(); ++i)
            if (lines[i].startsWithIgnoreCase ("content-length:"))
                contentLength = lines[i].fromFirstOccurrenceOf (":", false, false).trim().getIntValue();

        contentLength = juce::jlimit (0, maxBodyBytes, contentLength);

        // The body (the bytes after the headers, then the rest).
        juce::MemoryBlock body (received.begin() + headerEnd + 4, received.getSize() - (size_t) (headerEnd + 4));

        while ((int) body.getSize() < contentLength)
        {
            if (client.waitUntilReady (true, readTimeoutMs) != 1)
                return;

            char chunk[2048];
            const auto got = client.read (chunk, juce::jmin ((int) sizeof (chunk), contentLength - (int) body.getSize()), false);

            if (got <= 0)
                return;

            body.append (chunk, (size_t) got);
        }

        const auto path = target.upToFirstOccurrenceOf ("?", false, false);
        const auto query = target.fromFirstOccurrenceOf ("?", false, false);

        if (method == "GET" && (path == "/" || path == "/index.html"))
            return respondPage (client);

        if (method == "GET" && path == "/fanan-logo.png")
        {
            juce::MemoryBlock copy;

            {
                const std::lock_guard<std::mutex> lock (mutex);
                copy = logo;
            }

            return respond (client, copy.isEmpty() ? "404 Not Found" : "200 OK", "image/png", copy);
        }

        if (path == "/api/state" && method == "GET")
            return respondJson (client, stateAnswer (query));

        if (path == "/api/send" && method == "POST")
            return respondJson (client, sendAnswer (query, body.toString()));

        respond (client, "404 Not Found", "text/plain; charset=utf-8", juce::MemoryBlock ("Not here.", 9));
    }

    static juce::String queryValue (const juce::String& query, const juce::String& key)
    {
        for (const auto& pair : juce::StringArray::fromTokens (query, "&", ""))
            if (pair.upToFirstOccurrenceOf ("=", false, false) == key)
                return juce::URL::removeEscapeChars (pair.fromFirstOccurrenceOf ("=", false, false).replaceCharacter ('+', ' '));

        return {};
    }

    static juce::String refusal (const juce::String& error, const juce::String& message)
    {
        juce::DynamicObject::Ptr answer (new juce::DynamicObject());
        answer->setProperty ("ok", false);
        answer->setProperty ("error", error);
        answer->setProperty ("message", message);
        return juce::JSON::toString (juce::var (answer.get()), true);
    }

    /** The key the phone holds is this computer's (the QR code's). Counts it as seen. */
    bool paired (const juce::String& query, juce::String& refused)
    {
        const auto key = queryValue (query, "k");
        juce::String expected;

        {
            const std::lock_guard<std::mutex> lock (mutex);
            expected = pairingKey;
        }

        if (key.isEmpty() || key != expected)
        {
            refused = refusal ("pair", "Scan the QR code in Sonify's AI panel to pair this phone.");
            return false;
        }

        lastSeen = juce::Time::currentTimeMillis();
        return true;
    }

    /** As Fanan's server answers the away page: { ok, name, online, pending, stateAt, state | unchanged }. */
    juce::String stateAnswer (const juce::String& query)
    {
        juce::String refused;

        if (! paired (query, refused))
            return refused;

        const auto since = queryValue (query, "since").getLargeIntValue();
        const std::lock_guard<std::mutex> lock (mutex);

        juce::String text;
        text << "{\"ok\":true,\"name\":" << juce::JSON::toString (name) << ",\"online\":true,\"pending\":"
             << (requests.empty() ? "false" : "true") << ",\"stateAt\":" << stateAt;

        if (since == stateAt && stateAt > 0)
            text << ",\"unchanged\":true}";
        else
            text << ",\"state\":" << (state.isNotEmpty() ? state : juce::String ("null")) << "}";

        return text;
    }

    juce::String sendAnswer (const juce::String& query, const juce::String& bodyText)
    {
        juce::String refused;

        if (! paired (query, refused))
            return refused;

        const auto body = parseJson (bodyText);
        const auto kindAsked = body.getProperty ("kind", {}).toString();
        const auto kind = kindAsked == "stop" || kindAsked == "shortcut_save" || kindAsked == "shortcut_delete" ? kindAsked : juce::String ("ask");
        const auto text = body.getProperty ("text", {}).toString().trim().substring (0, maxRequestChars);

        if (kind != "stop" && text.isEmpty())
            return refusal ("invalid", kind == "ask" ? "Type something first." : "That shortcut is empty.");

        Request request;
        request.id = juce::Uuid().toString();
        request.kind = kind;
        request.text = text;
        request.name = body.getProperty ("name", {}).toString().trim().substring (0, maxShortcutNameChars);
        request.spoken = kind == "ask" && static_cast<bool> (body.getProperty ("spoken", false));

        {
            const std::lock_guard<std::mutex> lock (mutex);

            if (kind == "ask")
                for (const auto& waitingRequest : requests)
                    if (waitingRequest.kind == "ask")
                        return refusal ("busy", "Sonify hasn't picked up the last request yet. Try again in a moment.");

            requests.push_back (request);
        }

        juce::DynamicObject::Ptr answer (new juce::DynamicObject());
        answer->setProperty ("ok", true);
        answer->setProperty ("id", request.id);
        return juce::JSON::toString (juce::var (answer.get()), true);
    }

    //==============================================================================
    void respondPage (juce::StreamingSocket& client)
    {
        juce::MemoryBlock copy;

        {
            const std::lock_guard<std::mutex> lock (mutex);
            copy = page;
        }

        if (copy.isEmpty())
            copy = juce::MemoryBlock ("Sonify Remote's page is missing from this build.", 48);

        respond (client, "200 OK", "text/html; charset=utf-8", copy);
    }

    void respondJson (juce::StreamingSocket& client, const juce::String& json)
    {
        const auto utf8 = json.toUTF8();
        respond (client, "200 OK", "application/json; charset=utf-8", juce::MemoryBlock (utf8.getAddress(), utf8.sizeInBytes() - 1));
    }

    static void respond (juce::StreamingSocket& client, const juce::String& status, const juce::String& type, const juce::MemoryBlock& content)
    {
        juce::String head;
        head << "HTTP/1.1 " << status << "\r\n"
             << "Content-Type: " << type << "\r\n"
             << "Content-Length: " << (int) content.getSize() << "\r\n"
             << "Cache-Control: no-store\r\n"
             << "Connection: close\r\n\r\n";

        const auto headBytes = head.toUTF8();

        if (client.write (headBytes.getAddress(), (int) headBytes.sizeInBytes() - 1) < 0)
            return;

        for (size_t done = 0; done < content.getSize();)
        {
            const auto wrote = client.write (static_cast<const char*> (content.getData()) + done,
                                             (int) juce::jmin ((size_t) 65536, content.getSize() - done));

            if (wrote <= 0)
                return;

            done += (size_t) wrote;
        }
    }

    std::unique_ptr<juce::StreamingSocket> listener;
    int port = 0;

    mutable std::mutex mutex;
    juce::MemoryBlock page, logo;
    juce::String pairingKey, name, state;
    juce::int64 stateAt = 0;
    std::deque<Request> requests;
    std::atomic<juce::int64> lastSeen { 0 };
};

//==============================================================================
struct PhoneRemote::LocalTick final : public juce::Timer
{
    explicit LocalTick (PhoneRemote& r) : remote (r) {}
    void timerCallback() override   { remote.localTick(); }

    PhoneRemote& remote;
};

//==============================================================================
PhoneRemote::PhoneRemote (FananServer& s)
    : server (s)
{
    loadSettings();
}

PhoneRemote::~PhoneRemote()
{
    *alive = false;   // an answer arriving later finds no one
    stopTimer();
    stopLocal();

    // As it goes: the phone hears it at once (and sees it gone within seconds anyway).
    if (opened)
        server.remote ("close", {}, nullptr);
}

void PhoneRemote::changed()
{
    if (onChanged != nullptr)
        onChanged();
}

void PhoneRemote::setEnabled (bool shouldBeOn)
{
    if (enabled == shouldBeOn)
        return;

    enabled = shouldBeOn;
    ++session;
    waiting = false;
    problem.clear();

    if (enabled)
    {
        startLocal();
        startTimer (pollMs);
        timerCallback();   // at once
    }
    else
    {
        stopTimer();
        stopLocal();

        if (opened)
            server.remote ("close", {}, nullptr);

        opened = connected = false;
        sentState.clear();
        lastWhy.clear();
    }

    changed();
}

juce::String PhoneRemote::whyNot() const
{
    // Away, it works through Fanan's server, like Fanan AI.
    if (! server.isConfigured() || ! server.hasSite())
        return "Away from the studio, Sonify Remote isn't set up in this build yet.";

    if (server.isBlocked())
        return "The internet is blocked (the switch at the top left): away from the studio, Sonify Remote works through it.";

    if (! server.isReady())
    {
        const auto status = server.getStatus();

        return status == FananServer::Status::checking   ? juce::String::fromUTF8 ("Connecting to Fanan's server\xe2\x80\xa6")
             : status == FananServer::Status::noInternet ? juce::String ("No internet connection: away from the studio, Sonify Remote works through it.")
                                                         : juce::String ("Fanan's server isn't responding right now.");
    }

    if (! server.isSignedIn())
        return "Sign in to use Sonify Remote away from the studio: the phone signs in to the same Fanan account.";

    return {};
}

//==============================================================================
// Away: through Fanan's server
void PhoneRemote::timerCallback()
{
    if (! enabled)
        return;

    const auto why = whyNot();

    if (why != lastWhy)
    {
        lastWhy = why;
        changed();
    }

    if (why.isNotEmpty())
    {
        // It waits, and opens afresh once it can again. Signed out, the server hears it closed
        // at once (offline, it can't: the phone sees it gone within seconds anyway).
        if (opened)
        {
            opened = false;
            server.remote ("close", {}, nullptr);
        }

        setConnected (false, {});
        return;
    }

    if (waiting)
        return;   // one call at a time

    if (opened)
        poll();
    else
        open();
}

void PhoneRemote::open()
{
    juce::DynamicObject::Ptr fields (new juce::DynamicObject());
    fields->setProperty ("name", juce::SystemStats::getComputerName().trim().substring (0, 60));

    waiting = true;

    server.remote ("open", juce::var (fields.get()), [this, flag = alive, asked = session] (const juce::Result& result, const juce::var& answer)
    {
        if (! *flag || asked != session)
            return;

        waiting = false;

        if (result.failed())
            return failed (result, answer);

        opened = true;
        sentState.clear();   // the server starts without it: the first check posts it
        setConnected (true, {});
    });
}

void PhoneRemote::poll()
{
    const auto state = currentState();
    const auto sending = state != sentState;

    juce::DynamicObject::Ptr fields (new juce::DynamicObject());

    if (sending)
        fields->setProperty ("state", state);

    waiting = true;

    server.remote ("poll", juce::var (fields.get()), [this, flag = alive, asked = session, state, sending] (const juce::Result& result, const juce::var& answer)
    {
        if (! *flag || asked != session)
            return;

        waiting = false;

        if (result.failed())
            return failed (result, answer);

        if (sending)
            sentState = state;

        setConnected (true, {});

        // What the phone asked for: handed over once. A shortcut to save comes as its words
        // and name together, in the request's text ({ "text", "name" }).
        if (const auto request = answer.getProperty ("request", {}); request.isObject())
        {
            const auto kind = request.getProperty ("kind", {}).toString();
            auto text = request.getProperty ("text", {}).toString();
            juce::String name;

            if (kind == "shortcut_save")
                if (const auto packed = parseJson (text); packed.isObject())
                {
                    name = packed.getProperty ("name", {}).toString();
                    text = packed.getProperty ("text", {}).toString();
                }

            handleRequest (request.getProperty ("id", {}).toString(), kind, text,
                           static_cast<bool> (request.getProperty ("spoken", false)), name);

            // The phone sees at once that it arrived (and the shortcuts changed).
            sentState.clear();
        }
    });
}

void PhoneRemote::failed (const juce::Result& result, const juce::var& answer)
{
    const auto error = answer.getProperty ("error", {}).toString();

    if (error == "closed")
    {
        opened = false;   // the server closed it (it signed in to another account, say): it opens again
        return;
    }

    if (error == "sign_in" || error == "unknown")
    {
        opened = false;
        setConnected (false, {});   // the account shows why, and whyNot says it
        return;
    }

    setConnected (false, result.getErrorMessage());   // it keeps trying
}

void PhoneRemote::setConnected (bool isNow, const juce::String& newProblem)
{
    if (connected == isNow && problem == newProblem)
        return;

    connected = isNow;
    problem = newProblem;
    changed();
}

//==============================================================================
// Both: what the phone asked for
void PhoneRemote::handleRequest (const juce::String& id, const juce::String& kind, const juce::String& text, bool spoken, const juce::String& name)
{
    taken = id;   // the phone sees it arrive

    if (kind == "stop")
    {
        if (onStop != nullptr)
            onStop();

        return;
    }

    if (kind == "shortcut_save")
    {
        const auto words = text.trim().substring (0, maxShortcutChars);

        if (words.isEmpty())
            return;

        auto* list = shortcuts.getArray();

        // The same words once: saved again, it takes the new name.
        for (auto& item : *list)
            if (item.getProperty ("text", {}).toString() == words)
            {
                if (auto* object = item.getDynamicObject())
                    object->setProperty ("name", name.trim().substring (0, maxShortcutNameChars));

                saveSettings();
                return;
            }

        if (list->size() >= maxShortcuts)
            list->remove (0);   // the oldest makes room

        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("id", juce::Uuid().toString().substring (0, 12));
        item->setProperty ("name", name.trim().substring (0, maxShortcutNameChars));
        item->setProperty ("text", words);
        list->add (juce::var (item.get()));
        saveSettings();
        return;
    }

    if (kind == "shortcut_delete")
    {
        auto* list = shortcuts.getArray();

        for (int i = list->size(); --i >= 0;)
            if (list->getReference (i).getProperty ("id", {}).toString() == text.trim())
                list->remove (i);

        saveSettings();
        return;
    }

    if (const auto words = text.trim(); words.isNotEmpty() && onAsk != nullptr)
        onAsk (words.substring (0, maxRequestChars), spoken);
}

juce::String PhoneRemote::currentState() const
{
    return stateJson (getConversation != nullptr ? getConversation() : juce::var(), server.getCredits(), taken, shortcuts);
}

//==============================================================================
// In the studio: the page on the local network
void PhoneRemote::startLocal()
{
    stopLocal();
    localProblem.clear();
    localSeen = false;

    if (localAddress().isNull())
    {
        localProblem = "This computer isn't on a network: in the studio, the phone and Sonify meet on the same Wi-Fi.";
        return;
    }

    auto made = std::make_unique<LocalServer>();

    if (! made->start (getLocalPage != nullptr ? getLocalPage() : juce::MemoryBlock(),
                       getLocalLogo != nullptr ? getLocalLogo() : juce::MemoryBlock(), pairingKey,
                       juce::SystemStats::getComputerName().trim().substring (0, 60)))
    {
        localProblem = "Sonify couldn't open its page for phones on this network (ports "
                     + juce::String (firstLocalPort) + "-" + juce::String (firstLocalPort + 9) + " are all in use).";
        return;
    }

    local = std::move (made);
    local->setState (currentState());

    tick = std::make_unique<LocalTick> (*this);
    tick->startTimer (localTickMs);
}

void PhoneRemote::stopLocal()
{
    tick = nullptr;
    local = nullptr;
    localSeen = false;
}

void PhoneRemote::localTick()
{
    if (local == nullptr)
        return;

    for (const auto& request : local->takeRequests())
        handleRequest (request.id, request.kind, request.text, request.spoken, request.name);

    local->setState (currentState());

    if (const auto seen = isLocalPhoneSeen(); seen != localSeen)
    {
        localSeen = seen;
        changed();
    }
}

juce::String PhoneRemote::getLocalUrl() const
{
    if (local == nullptr || local->getPort() == 0)
        return {};

    const auto address = localAddress();

    if (address.isNull())
        return {};

    return "http://" + address.toString() + ":" + juce::String (local->getPort()) + "/?k=" + pairingKey;
}

bool PhoneRemote::isLocalPhoneSeen() const
{
    return local != nullptr && juce::Time::currentTimeMillis() - local->lastSeenMs() < seenMs;
}

void PhoneRemote::newPairingKey()
{
    pairingKey = newKey();
    saveSettings();

    if (local != nullptr)
        local->setKey (pairingKey);

    changed();
}

//==============================================================================
void PhoneRemote::loadSettings()
{
    const auto kept = parseJson (settingsFile().loadFileAsString());

    pairingKey = kept.getProperty ("key", {}).toString();

    if (pairingKey.length() < 16)
    {
        pairingKey = newKey();
        shortcuts = juce::Array<juce::var>();
        saveSettings();
    }

    juce::Array<juce::var> list;

    if (const auto* items = kept.getProperty ("shortcuts", {}).getArray())
        for (const auto& item : *items)
            if (const auto words = item.getProperty ("text", {}).toString().trim(); words.isNotEmpty() && list.size() < maxShortcuts)
            {
                juce::DynamicObject::Ptr clean (new juce::DynamicObject());
                clean->setProperty ("id", item.getProperty ("id", juce::Uuid().toString().substring (0, 12)).toString());
                clean->setProperty ("name", item.getProperty ("name", {}).toString().substring (0, maxShortcutNameChars));
                clean->setProperty ("text", words.substring (0, maxShortcutChars));
                list.add (juce::var (clean.get()));
            }

    shortcuts = list;
}

void PhoneRemote::saveSettings() const
{
    juce::DynamicObject::Ptr root (new juce::DynamicObject());
    root->setProperty ("key", pairingKey);
    root->setProperty ("shortcuts", shortcuts.isArray() ? shortcuts : juce::var (juce::Array<juce::var>()));
    settingsFile().replaceWithText (juce::JSON::toString (juce::var (root.get())));
}

//==============================================================================
juce::String PhoneRemote::stateJson (const juce::var& aiState, int credits, const juce::String& taken, const juce::var& shortcuts)
{
    juce::DynamicObject::Ptr top (new juce::DynamicObject());
    top->setProperty ("busy", static_cast<bool> (aiState.getProperty ("busy", false)));

    if (credits >= 0)
        top->setProperty ("credits", credits);

    top->setProperty ("taken", taken);
    top->setProperty ("shortcuts", shortcuts.isArray() ? shortcuts : juce::var (juce::Array<juce::var>()));
    top->setProperty ("entries", juce::Array<juce::var>());

    // The newest entries, as many as fit.
    auto used = juce::JSON::toString (juce::var (top.get()), true).length();
    juce::Array<juce::var> newestFirst;

    if (const auto* entries = aiState.getProperty ("entries", {}).getArray())
    {
        for (int i = entries->size(); --i >= 0;)
        {
            const auto& entry = entries->getReference (i);
            const auto role = entry.getProperty ("role", {}).toString();
            auto text = entry.getProperty ("text", {}).toString();

            if (text.length() > maxEntryChars)
                text = text.substring (0, maxEntryChars) + juce::String::fromUTF8 ("\xe2\x80\xa6");

            juce::DynamicObject::Ptr item (new juce::DynamicObject());
            item->setProperty ("role", role);
            item->setProperty ("text", text);

            if (role == "user" && static_cast<bool> (entry.getProperty ("spoken", false)))
                item->setProperty ("spoken", true);

            if (role == "assistant")
            {
                juce::Array<juce::var> steps;

                if (const auto* list = entry.getProperty ("steps", {}).getArray())
                    for (const auto& step : *list)
                        if (const auto line = step.getProperty ("text", {}).toString(); line.isNotEmpty())
                            steps.add (line);

                item->setProperty ("steps", steps);
            }

            const auto length = juce::JSON::toString (juce::var (item.get()), true).length() + 1;

            if (used + length > maxStateChars)
                break;

            used += length;
            newestFirst.add (juce::var (item.get()));
        }
    }

    juce::Array<juce::var> inOrder;

    for (int i = newestFirst.size(); --i >= 0;)
        inOrder.add (newestFirst.getReference (i));

    top->setProperty ("entries", inOrder);
    return juce::JSON::toString (juce::var (top.get()), true);
}
