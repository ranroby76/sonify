// D:\workspace\Fanan_web_hub\src\PhoneRemote.h

#pragma once

#include "FananServer.h"

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <functional>
#include <memory>

//==============================================================================
/**
    Sonify Remote: Sonify AI from a phone, anywhere in the studio.

    With Phone remote on, Sonify opens its remote on Fanan's server (sonifyRemote) and checks in
    every second and a half: it posts Sonify AI's conversation, when it changed, for the phone to
    show, and picks up what the phone asked for. The phone is the page /sonify on Fanan's site,
    signed in to the same Fanan account: it asks Sonify AI (typed, or said and turned into words)
    and can stop it.

    It works while Fanan's server and the account can, and goes on by itself when they can
    again. Always off at launch: no phone drives Sonify unless it was turned on here.

    Message thread.
*/
class PhoneRemote final : private juce::Timer
{
public:
    explicit PhoneRemote (FananServer& server);
    ~PhoneRemote() override;

    void setEnabled (bool shouldBeOn);
    bool isEnabled() const noexcept                 { return enabled; }

    /** Why it can't work right now, for the page; empty when it can. */
    juce::String whyNot() const;

    /** Fanan's server has it open and answers its checks. */
    bool isConnected() const noexcept               { return connected; }

    /** What went wrong last (it keeps trying); empty when nothing did. */
    const juce::String& getProblem() const noexcept { return problem; }

    /** Sonify AI's state (AiAgent::getState) for the phone. */
    std::function<juce::var()> getConversation;

    /** The phone asks Sonify AI something: typed, or said (spoken) and turned into words. */
    std::function<void (const juce::String& text, bool spoken)> onAsk;

    /** The phone pressed Stop. */
    std::function<void()> onStop;

    /** On or off, connected or not, the problem or the reason it can't changed. */
    std::function<void()> onChanged;

    /** The conversation as the phone takes it: { busy, credits, taken, entries }, the oldest entries
        left out when it would be longer than maxStateChars. Public for tests. */
    static juce::String stateJson (const juce::var& aiState, int credits, const juce::String& taken);

    static constexpr int pollMs = 1500;
    static constexpr int maxStateChars = 50000;     // Fanan's server keeps 60,000
    static constexpr int maxEntryChars = 4000;
    static constexpr int maxRequestChars = 2000;

private:
    void timerCallback() override;
    void open();
    void poll();
    void failed (const juce::Result& result, const juce::var& answer);
    void setConnected (bool isNow, const juce::String& newProblem);
    void changed();

    FananServer& server;
    bool enabled = false, opened = false, connected = false, waiting = false;
    juce::String lastWhy, problem;
    juce::String sentState;     // the conversation last posted: only changes go
    juce::String taken;         // the last request picked up, for the phone to see
    int session = 0;            // moves on when it's turned off or on: answers from before are dropped
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhoneRemote)
};
