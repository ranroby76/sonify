// D:\workspace\Fanan_web_hub\src\PhoneRemote.h

#pragma once

#include "FananServer.h"

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <functional>
#include <memory>

//==============================================================================
/**
    Sonify Remote: Fanan AI from a phone, by text. Two ways in, both on with the one switch:

    - In the studio (the phone on the same Wi-Fi): Sonify serves the remote's page itself, on
      the local network (http://<this computer>:<port>/). The phone scans the QR code the AI
      panel shows: its address carries this computer's pairing key, which the page keeps. No
      internet, sign-in or Fanan's server needed, and it's instant. The key is new each time
      Sonify starts, so a phone stays paired for that session only; New pairing key does the
      same at once: every phone paired before has to scan again.
    - Away: through Fanan's server (sonifyRemote): Sonify checks in every second and a half,
      posting the conversation when it changed and picking up what the phone asked for. The
      phone is the page /sonify on Fanan's site, signed in to the same Fanan account.

    Both pages show the same: Fanan AI's conversation, the shortcuts (texts the user saved as
    buttons, kept here in Sonify so every phone has the same ones), and where the last request
    is (sent, arrived, Fanan AI answering, answered). The phone asks Fanan AI (typed, or said and
    turned into words), stops it, and saves or deletes shortcuts.

    Always off at launch: no phone drives Sonify unless it was turned on here.

    Message thread (the local page's server answers on a thread of its own, from what the
    message thread left for it).
*/
class PhoneRemote final : private juce::Timer
{
public:
    explicit PhoneRemote (FananServer& server);
    ~PhoneRemote() override;

    void setEnabled (bool shouldBeOn);
    bool isEnabled() const noexcept                 { return enabled; }

    //==============================================================================
    // Away: through Fanan's server

    /** Why the away route can't work right now, for the page; empty when it can. */
    juce::String whyNot() const;

    /** Fanan's server has it open and answers its checks. */
    bool isConnected() const noexcept               { return connected; }

    /** What went wrong last (it keeps trying); empty when nothing did. */
    const juce::String& getProblem() const noexcept { return problem; }

    //==============================================================================
    // In the studio: the page on the local network

    /** The address the QR code holds (with the pairing key), while it's on and serving;
        empty when it isn't. */
    juce::String getLocalUrl() const;

    /** Why the local page isn't served (no network, no free port); empty when it is. */
    const juce::String& getLocalProblem() const noexcept   { return localProblem; }

    /** A phone asked something through the local page within the last few seconds. */
    bool isLocalPhoneSeen() const;

    /** A new pairing key: phones paired before have to scan the QR code again. */
    void newPairingKey();

    /** The local page's file (phone-remote.html) and Fanan's logo for its header, compiled into Sonify. */
    std::function<juce::MemoryBlock()> getLocalPage, getLocalLogo;

    //==============================================================================
    /** Fanan AI's state (AiAgent::getState) for the phone. */
    std::function<juce::var()> getConversation;

    /** The phone asks Fanan AI something: typed, or said (spoken) and turned into words. */
    std::function<void (const juce::String& text, bool spoken)> onAsk;

    /** The phone pressed Stop. */
    std::function<void()> onStop;

    /** On or off, connected or not, the problem, the local page or the reason it can't changed. */
    std::function<void()> onChanged;

    //==============================================================================
    /** The conversation as the phone takes it: { busy, credits, taken, shortcuts, entries }, the
        oldest entries left out when it would be longer than maxStateChars. Public for tests. */
    static juce::String stateJson (const juce::var& aiState, int credits, const juce::String& taken, const juce::var& shortcuts);

    /** A request from either page: kind "ask" (text, spoken), "stop", "shortcut_save" (text: the
        shortcut's words, name: its button's, both optional but the words) or "shortcut_delete"
        (text: the shortcut's id). Public for tests. */
    void handleRequest (const juce::String& id, const juce::String& kind, const juce::String& text, bool spoken, const juce::String& name);

    /** The shortcuts as kept: [{ id, name, text }]. */
    juce::var getShortcuts() const                  { return shortcuts; }

    static constexpr int pollMs = 1500;
    static constexpr int localTickMs = 250;
    static constexpr int maxStateChars = 50000;     // Fanan's server keeps 60,000
    static constexpr int maxEntryChars = 4000;
    static constexpr int maxRequestChars = 2000;
    static constexpr int maxShortcuts = 60;
    static constexpr int maxShortcutNameChars = 40;
    static constexpr int firstLocalPort = 47650;

private:
    class LocalServer;
    struct LocalTick;

    void timerCallback() override;
    void open();
    void poll();
    void failed (const juce::Result& result, const juce::var& answer);
    void setConnected (bool isNow, const juce::String& newProblem);
    void changed();

    void startLocal();
    void stopLocal();
    void localTick();
    juce::String currentState() const;

    void loadSettings();
    void saveSettings() const;

    FananServer& server;
    bool enabled = false, opened = false, connected = false, waiting = false;
    juce::String lastWhy, problem;
    juce::String sentState;     // the conversation last posted: only changes go
    juce::String taken;         // the last request picked up, for the phone to see
    int session = 0;            // moves on when it's turned off or on: answers from before are dropped
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    juce::String pairingKey;    // new each time Sonify starts: phones pair for one session
    juce::var shortcuts;        // [{ id, name, text }], kept
    juce::String localProblem;
    bool localSeen = false;
    std::unique_ptr<LocalServer> local;
    std::unique_ptr<LocalTick> tick;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhoneRemote)
};
