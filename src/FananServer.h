// D:\workspace\Fanan_web_hub\src\FananServer.h

#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <functional>
#include <memory>

//==============================================================================
/**
    Sonify's link to Fanan's server: the Fanan account, its credits, and Sonify AI.

    It's the same server, the same accounts and the same credits as Stella AI Studio (the
    Stella AI Cloud app on Base44): one Fanan account, one credit balance, both apps.

    Each computer introduces itself with a random key that stays on it, plus a one-way hash
    of the computer's ID. To sign in, Sonify asks the server for a one-time link and opens
    it in the browser; once the user is signed in there, the site links this computer to
    their Fanan account. From then on Sonify is signed in by itself at every start, until
    the user signs out. The user never types or copies a code, and no password passes
    through Sonify. The email last signed in with is remembered, to suggest it next time.

    The connection is watched all the time, and "no internet" is told apart from "Fanan's
    server isn't answering", so the user is warned with the right message. While Sonify's
    "Block internet" is on, nothing here touches the network. Nothing here names the AI
    service behind the server.

    Network waits happen on worker threads; every answer arrives on the message thread.
*/
class FananServer final : private juce::Timer
{
public:
    enum class Status
    {
        checking,       // no answer yet
        online,         // the server answered
        notSetUp,       // the internet works, but this build has no server address yet
        noInternet,     // nothing can be reached
        serverDown,     // the internet works, but Fanan's server doesn't answer properly
        blocked         // Sonify's "Block internet" is on: nothing is tried
    };

    struct Settings
    {
        juce::String serverUrl { "https://base44.app" };
        juce::String appId;      // the Stella AI Cloud app on Base44, shared with Stella AI Studio
        juce::String siteUrl;    // its site: /link signs in and links, /buy sells credits
        juce::StringArray internetTestUrls { "https://www.msftconnecttest.com/connecttest.txt",
                                             "https://1.1.1.1/cdn-cgi/trace" };
    };

    /** The built-in settings, overridden by FananWebHub-server.json beside the program. */
    static Settings loadSettings();

    using Done = std::function<void (const juce::Result& result, const juce::var& answer)>;

    explicit FananServer (Settings settings);
    ~FananServer() override;

    /** Starts watching the connection (the first check at once). Call after setBlocked. */
    void start();

    /** Sonify's "Block internet": when on, nothing reaches the network; off, it checks again. */
    void setBlocked (bool shouldBlock);
    bool isBlocked() const noexcept         { return blocked.load(); }

    Status getStatus() const;
    juce::String getStatusText() const;     // one short line for the UI
    int getCredits() const;                 // -1 until the server has said
    juce::String getComputerCode() const;   // this computer's code, for support
    bool isSignedIn() const;                // linked to a Fanan account
    juce::String getEmail() const;          // that account's email
    juce::String getLastEmail() const;      // the email last signed in with on this computer (kept after signing out;
                                            // at first, the one Stella AI Studio last used here)

    bool isConfigured() const noexcept      { return settings.appId.isNotEmpty(); }
    bool hasSite() const noexcept           { return settings.siteUrl.isNotEmpty(); }
    bool isReady() const;                   // online, and known to the server

    /** Tests the connection again now (the Try again button). */
    void checkNow();

    /** Opens the Fanan site to sign in; the site links this computer to the account.
        emailHint: the email the user typed (the site can fill it in). thenBuy: the site goes
        on to the credits page. done says whether the browser opened. */
    void signIn (bool thenBuy, const juce::String& emailHint, Done done);

    /** Unlinks this computer from its account. */
    void signOut (Done done);

    /** The credits page, for a computer that's signed in. */
    juce::URL getStoreUrl() const;

    /** Sonify Remote's page, for the phone (empty without a site). */
    juce::String getRemotePageUrl() const;

    /** Checks often for a while: a sign-in or purchase in the browser shows up in seconds. */
    void watchForChanges();

    /** Sends one step of a Sonify AI request: { system, tools, messages, max_tokens }.
        The answer holds { content, stop, used, credits }. */
    void chat (const juce::var& request, Done done);

    /** Sends one step of the Plugins Creator Studio's Sonify AI (Stella AI Studio's builder, with
        its own settings on the server: its stellaChat function). Through Fanan's relay when the
        server names one (in its "hello" answer): the relay makes the AI call with no time limit,
        so long building steps aren't cut off, and the server still checks and charges the
        credits. If the relay can't be reached at all, the step goes to the server as before. */
    void stellaChat (const juce::var& request, Done done);

    /** Voice control: a short recording (16 kHz mono WAV) to be turned into words. The answer
        holds { text }. Nothing is charged; the server keeps a daily limit per account. */
    void transcribe (const juce::MemoryBlock& wav, Done done);

    /** Sonify Remote (the phone page): action "open" { name }, "poll" { state? } or "close". A poll's
        answer holds { request: { id, kind, text, spoken } or null }; a failed call's answer holds
        the server's { error } ("closed": open it again). */
    void remote (const juce::String& action, const juce::var& fields, Done done);

    /** Sonify's store (the server's sonifyStore function): an action ("begin", "publish", "list",
        "buy", "licence"...) with its fields; the answer is the server's ({ ok, ... }, or
        { error }). A failed call's answer still holds the server's error. */
    void store (const juce::String& action, const juce::var& fields, Done done);

    /** Sends a file to an address the store gave (an HTTP PUT): a plugin's package or its
        picture. The answer holds { status }. */
    void upload (const juce::String& address, const juce::MemoryBlock& data, const juce::String& contentType, Done done);

    /** Called on the message thread whenever the status, credits or account change. */
    std::function<void()> onStateChanged;

    static constexpr const char* chatFunction = "sonifyChat";
    static constexpr const char* builderFunction = "stellaChat";
    static constexpr const char* listenFunction = "sonifyListen";
    static constexpr const char* remoteFunction = "sonifyRemote";
    static constexpr const char* storeFunction = "sonifyStore";

private:
    struct Response
    {
        bool connected = false;   // a server answered at all
        int status = 0;
        juce::var body;
    };

    struct Snapshot
    {
        Status status = Status::checking;
        int credits = -1;
        juce::String code, email;
        bool linked = false;
        bool known = false;   // the server answered "hello"
        juce::String relay;   // Fanan's relay for building steps, when the server names one
    };

    void timerCallback() override;
    void startCheck();
    void runCheck (int asked);
    void setState (const Snapshot& snapshot);
    void scheduleNextCheck();

    Response post (const juce::String& function, const juce::var& body, int timeoutMs);
    Response postTo (const juce::URL& address, const juce::var& body, int timeoutMs);

    /** One step of an AI request to a server function (through the relay when asked and there is one). */
    void ask (const juce::String& function, bool throughRelay, const juce::var& request, Done done);
    static bool isUsableRelay (const juce::String& address);
    Response fetch (const juce::URL& url, const juce::String& headers, int timeoutMs, bool isPost);
    bool canReachInternet();
    juce::var makeRequest (const juce::String& action) const;
    static juce::String failureText (const Response& response, const juce::String& fallback);

    void deliver (std::function<void()> fn);
    void saveIdentity() const;

    static juce::File getIdentityFile();
    static juce::File getStellaIdentityFile();   // Stella AI Studio's, read only for its last email
    static juce::String makeKey();
    static bool isValidKey (const juce::String& key);
    static juce::String deviceHash();

    const Settings settings;
    juce::String key;   // set once in the constructor, then only read

    mutable juce::CriticalSection stateLock;
    Status status = Status::checking;
    int credits = -1;
    juce::String computerCode, email, lastEmail;
    juce::String relayUrl;   // under stateLock
    bool linked = false;

    std::atomic<bool> blocked { false };
    std::atomic<bool> started { false };
    std::atomic<bool> checking { false };
    int generation = 0;        // moves on when this computer signs out or Block internet changes (message thread only)
    int fastChecksLeft = 0;

    juce::CriticalSection streamsLock;
    juce::Array<juce::WebInputStream*> activeStreams;

    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    // Last, so they're the first to go: their jobs finish while everything above still exists.
    // An AI step can take minutes (Sonify AI, the Plugins Creator Studio's, the Guide's, Stella
    // AI's): those have threads of their own, so the checks, voice control, the phone remote and
    // the store never wait behind them.
    juce::ThreadPool workers { juce::ThreadPoolOptions{}.withThreadName ("Fanan server").withNumberOfThreads (4) };
    juce::ThreadPool aiWorkers { juce::ThreadPoolOptions{}.withThreadName ("Fanan AI").withNumberOfThreads (4) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FananServer)
};
