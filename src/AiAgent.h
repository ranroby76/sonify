// D:\workspace\Fanan_web_hub\src\AiAgent.h

#pragma once

#include "AiTools.h"
#include "FananServer.h"

#include <juce_core/juce_core.h>

#include <functional>
#include <memory>
#include <vector>

//==============================================================================
/**
    Sonify AI: the AI driving the DAW with AiTools, acting and talking as Stella AI does.

    A request goes to Fanan's server (its sonifyChat function holds the AI service's key,
    picks the model by the mode the request says and charges the Fanan account's credits, as
    for Stella AI Studio; the modes are below, at setMode). The
    AI answers with text and tool calls; the tools run here and their results go back, until
    it's done, the user presses Stop, or the step limit is reached. Everything one request
    does is one undo step: Sonify takes the song before and after it.

    How it works and talks is in its guide, sonify_ai_guide.md (sent with every request,
    after Fanan's persona on the server): act first, short plain replies for musicians, and
    questions for the user marked to show in yellow.

    A request that can't go, or fails, leaves the conversation as it was, with a note
    saying what happened. Earlier requests are sent short (their steps as one line each),
    which keeps every request small. A request said into the mic (voice control) goes marked
    as spoken, so the AI acts on it without asking and answers in a few words.

    Runs on the message thread; only the network waits happen on FananServer's workers.
*/
class AiAgent final
{
public:
    /** What an AI works with: its guide, its tools, and how they run. Sonify AI's are AiTools and
        sonify_ai_guide.md; the Plugins Creator Studio's devices have their own. */
    struct Toolbox
    {
        virtual ~Toolbox() = default;

        /** Its instructions, the same with every request (so they're cached). */
        virtual juce::String guide() = 0;

        /** The tool list for the model: [{ name, description, input_schema }]. */
        virtual juce::var definitions() = 0;

        /** Runs one tool and calls done with its result ({ error } when it couldn't): at once, or
            later on the message thread (a build takes seconds). done is called exactly once. */
        virtual void run (const juce::String& name, const juce::var& input, std::function<void (const juce::var&)> done) = 0;

        /** One line on what a call did, for the page ("" for a tool that only looks). */
        virtual juce::String describe (const juce::String& name, const juce::var& input, const juce::var& result) = 0;

        /** Whether a tool only looks (changes nothing). */
        virtual bool looksOnly (const juce::String& name) = 0;

        /** The user pressed Stop: a tool still working (a build) stops as soon as it can, and
            answers. */
        virtual void cancel() {}
    };

    struct Hooks
    {
        std::function<void (const juce::String& request)> requestStarted;   // Sonify keeps the song as it was
        std::function<void (const juce::String& request, bool changed)> requestEnded;   // ... and makes it one undo step
        std::function<void()> stateChanged;                                 // tell the page (and show the song as it is)
    };

    /** Sonify AI: AiTools and its guide. */
    AiAgent (FananServer& server, AiTools& tools, Hooks hooks);

    /** Another AI on the same account (the Plugins Creator Studio's, for devices). */
    AiAgent (FananServer& server, Toolbox& toolbox, Hooks hooks);

    ~AiAgent();

    /** Where a request comes from. Spoken ones are marked with a mic on the page, and the AI is
        told: from the recording room (voice control: the musician hears only beeps, so it acts
        without asking whenever it can), or into the phone (Sonify Remote: they read the reply). */
    enum class Origin { typed, voice, phoneVoice };

    /** studio: the studio the user flagged the request with (Sonify AI's flag: "Analog Board",
        "Song Builder Blocks"...), or none. The AI is told, and the page shows it on the request.
        context: more for the AI with the request, not shown on the page (what the user's own
        code holds, say). */
    void send (const juce::String& text, Origin origin = Origin::typed, const juce::String& studio = {},
               const juce::String& context = {});

    /** A note in the conversation (from Sonify, not the AI): something that couldn't be sent. */
    void addNotice (const juce::String& text);

    /** Stops after the step that's running. */
    void stop();
    void clear();

    bool isBusy() const noexcept                    { return busy; }

    /** Which model Fanan's server asks: the mode it picks by. Sonify AI's is "agent" (Sonnet:
        the everyday requests) and becomes "deep" (Opus: the strongest) when the user turns Deep
        on, or by itself once a request has taken deepAfterSteps steps, since a request that long
        is a big job. Another AI on the same account says its own once (the Device Studio's
        "device": it writes code, so always the strongest). */
    void setMode (const juce::String& newMode);

    /** Deep: every request to the strongest model (Sonify AI's switch in the AI panel). */
    void setDeep (bool shouldBeDeep);
    bool isDeep() const noexcept                    { return deep; }

    /** The mode the request under way goes in, Deep and the step-up counted. */
    juce::String getMode() const;

    /** How the last request ended, for voice control's beeps: done, done with a question for
        the user, or stopped short (failed, declined, cut off, stopped). */
    enum class Outcome { none, done, question, problem };
    Outcome getLastOutcome() const noexcept         { return lastOutcome; }

    /** For the page: { busy, stopping, steps, entries: [{ role: user | assistant | notice, text,
        spoken (user, said into the mic), steps: [{ text, ok }] }] }. */
    juce::var getState() const;

    /** Sonify AI's guide (sonify_ai_guide.md): the same with every request, so it's cached. */
    static juce::String systemPrompt();

    /** The conversation, to keep and carry on later (the Plugins Creator Studio keeps each device's in its
        file): { messages, entries }, earlier requests short as they're sent. Restoring it
        replaces the conversation (not while busy). */
    juce::var saveConversation() const;
    void restoreConversation (const juce::var& saved);

    static constexpr int maxSteps = 40;             // AI steps in one request
    static constexpr int deepAfterSteps = 8;        // a request this long finishes on the strongest model
    static constexpr int maxHistory = 80;           // messages kept in the conversation
    static constexpr int maxAnswerTokens = 12000;   // one step's answer, thinking included

private:
    struct Call
    {
        juce::String id, name;
        juce::var input;
    };

    juce::String whyNotSent() const;
    void callModel();
    void onAnswer (const juce::Result& result, const juce::var& answer);
    void runNextCall();
    void callDone (const Call& call, const juce::var& outcome);
    void afterCalls();
    void finish (Outcome outcome, const juce::String& notice = {}, const juce::String& note = {});
    juce::Array<juce::var> compacted() const;
    juce::var& currentEntry();
    void addEntry (const juce::String& role, const juce::String& text);
    void trimConversation();
    void notify();

    FananServer& server;
    std::unique_ptr<Toolbox> ownToolbox;   // Sonify AI's, around AiTools
    Toolbox& toolbox;
    Hooks hooks;

    // The calls of the answer being worked through, one at a time.
    std::vector<Call> calls;
    size_t nextCall = 0;
    juce::Array<juce::var> results;
    juce::String stopReason, said;
    int requestNumber = 0;              // a result for an earlier request (cleared since) is dropped

    juce::Array<juce::var> messages;   // the conversation, in the AI's own format
    juce::Array<juce::var> entries;    // what the page shows
    juce::String requestText;
    juce::String mode { "agent" };     // what it asks for; "deep" while the request is a big one
    bool deep = false;                 // the user's Deep switch: every request to the strongest
    bool busy = false, stopping = false, changed = false;
    Outcome lastOutcome = Outcome::none;
    int steps = 0;
    int turnStart = 0;                 // where the request under way begins in messages
    int requestEntry = -1;             // its entry in entries (notices may come after it while it runs)
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AiAgent)
};
