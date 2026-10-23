// D:\workspace\Fanan_web_hub\src\AiAgent.cpp

#include "AiAgent.h"
#include "SonifyAiData.h"

namespace
{
    juce::var object()
    {
        return juce::var (new juce::DynamicObject());
    }

    juce::var textBlock (const juce::String& text)
    {
        auto block = object();
        block.getDynamicObject()->setProperty ("type", "text");
        block.getDynamicObject()->setProperty ("text", text);
        return block;
    }

    juce::var makeMessage (const juce::String& role, const juce::var& content)
    {
        auto message = object();
        message.getDynamicObject()->setProperty ("role", role);
        message.getDynamicObject()->setProperty ("content", content);
        return message;
    }

    /** One of the user's own requests (tool results come as a list of blocks instead). */
    bool isRequest (const juce::var& message)
    {
        return message.getProperty ("role", {}).toString() == "user" && message.getProperty ("content", {}).isString();
    }

    juce::String shortened (const juce::String& text, int length)
    {
        return text.length() > length ? text.substring (0, length) + juce::String::fromUTF8 ("\xe2\x80\xa6") : text;
    }

    /** Sonify AI's toolbox: AiTools, with its guide. */
    class SonifyToolbox final : public AiAgent::Toolbox
    {
    public:
        explicit SonifyToolbox (AiTools& t) : tools (t) {}

        juce::String guide() override            { return AiAgent::systemPrompt(); }
        juce::var definitions() override         { return AiTools::definitions(); }

        void run (const juce::String& name, const juce::var& input, std::function<void (const juce::var&)> done) override
        {
            done (tools.run (name, input));
        }

        juce::String describe (const juce::String& name, const juce::var& input, const juce::var& result) override
        {
            return AiTools::describe (name, input, result);
        }

        // Tools that only look: they don't change the song.
        bool looksOnly (const juce::String& name) override
        {
            return name == "get_song" || name == "get_clip" || name == "find_plugins" || name == "get_plugin_params" || name == "get_board";
        }

    private:
        AiTools& tools;
    };
}

AiAgent::AiAgent (FananServer& s, AiTools& t, Hooks h)
    : server (s), ownToolbox (std::make_unique<SonifyToolbox> (t)), toolbox (*ownToolbox), hooks (std::move (h))
{
}

AiAgent::AiAgent (FananServer& s, Toolbox& t, Hooks h)
    : server (s), toolbox (t), hooks (std::move (h))
{
}

AiAgent::~AiAgent()
{
    *alive = false;   // an answer arriving later finds no one
}

juce::String AiAgent::systemPrompt()
{
    static const juce::String guide = []
    {
        auto text = juce::String::fromUTF8 (SonifyAiData::sonify_ai_guide_md, SonifyAiData::sonify_ai_guide_mdSize);

        // The first line names the file's place in the project, for people: the AI doesn't need it.
        if (text.startsWith ("<!--"))
            text = text.fromFirstOccurrenceOf ("-->", false, false).trimStart();

        return text;
    }();

    return guide;
}

void AiAgent::notify()
{
    if (hooks.stateChanged) hooks.stateChanged();
}

void AiAgent::addEntry (const juce::String& role, const juce::String& text)
{
    auto entry = object();
    entry.getDynamicObject()->setProperty ("role", role);
    entry.getDynamicObject()->setProperty ("text", text);
    entries.add (entry);
}

juce::var& AiAgent::currentEntry()
{
    // The request's own entry, even when a notice came in after it while it runs (from the
    // phone, say): its steps and words stay in one place.
    if (! juce::isPositiveAndBelow (requestEntry, entries.size())
        || entries.getReference (requestEntry).getProperty ("role", {}).toString() != "assistant")
    {
        addEntry ("assistant", {});
        entries.getReference (entries.size() - 1).getDynamicObject()->setProperty ("steps", juce::Array<juce::var>());
        requestEntry = entries.size() - 1;
    }

    return entries.getReference (requestEntry);
}

void AiAgent::trimConversation()
{
    // A long conversation is cut before one of the user's own requests, so tool calls keep their results.
    while (messages.size() > maxHistory)
    {
        int cut = -1;

        for (int i = 1; i < messages.size(); ++i)
            if (isRequest (messages.getReference (i)))
                { cut = i; break; }

        if (cut < 0)
            break;

        messages.removeRange (0, cut);
    }
}

juce::String AiAgent::whyNotSent() const
{
    // Nothing is sent (or charged) unless Fanan's server can take it.
    if (! server.isConfigured())
        return "Fanan AI isn't set up in this build yet.";

    if (server.isBlocked())
        return "Not sent: the internet is blocked (the switch at the top left), and Fanan AI works through it.";

    if (! server.isReady())
    {
        const auto status = server.getStatus();

        return status == FananServer::Status::checking   ? "Not sent: Sonify is still connecting to Fanan's server. Send again in a moment."
             : status == FananServer::Status::noInternet ? "Not sent: there's no internet connection. Check it and send again."
                                                         : "Not sent: Fanan's server can't be reached right now. Please try again in a moment.";
    }

    if (! server.isSignedIn())
        return "Not sent: sign in (the account button, top left) to use Fanan AI.";

    if (server.getCredits() == 0)
        return "Not sent: you're out of credits. Use Buy credits to get more, then send again.";

    return {};
}

void AiAgent::addNotice (const juce::String& text)
{
    if (text.isEmpty())
        return;

    addEntry ("notice", text);
    notify();
}

void AiAgent::send (const juce::String& text, Origin origin, const juce::String& studio, const juce::String& context)
{
    const auto request = text.trim();

    if (busy || request.isEmpty())
        return;

    // The request shows in the conversation even when it can't go, so its words aren't lost.
    addEntry ("user", request);

    if (origin != Origin::typed)
        entries.getReference (entries.size() - 1).getDynamicObject()->setProperty ("spoken", true);

    const auto flag = studio.trim();

    if (flag.isNotEmpty())
        entries.getReference (entries.size() - 1).getDynamicObject()->setProperty ("studio", flag);

    if (const auto why = whyNotSent(); why.isNotEmpty())
    {
        if (server.isConfigured() && ! server.isBlocked() && ! server.isReady())
            server.checkNow();

        addEntry ("notice", why);
        lastOutcome = Outcome::problem;
        notify();
        return;
    }

    trimConversation();
    turnStart = messages.size();

    // The guide says how to take a spoken request (sonify_ai_guide.md, "Spoken requests") and a
    // flagged one ("Sonify's studios").
    const auto marker = origin == Origin::voice      ? juce::String ("(Spoken from the recording room) ")
                      : origin == Origin::phoneVoice ? juce::String ("(Spoken into the phone) ")
                                                     : juce::String();
    const auto flagged = flag.isNotEmpty() ? "(For the " + flag + ") " : juce::String();
    messages.add (makeMessage ("user", flagged + marker + request + context));

    busy = true;
    stopping = false;
    changed = false;
    lastOutcome = Outcome::none;
    steps = 0;
    requestText = request;
    requestEntry = -1;   // its entry comes with its first answer

    if (hooks.requestStarted) hooks.requestStarted (request);
    notify();
    callModel();
}

juce::Array<juce::var> AiAgent::compacted() const
{
    // As in Stella AI: earlier requests are sent short (each step as one line, long results
    // cut), which keeps every request small; Sonify AI looks at the song again when it needs
    // to. The request under way goes exactly as it is: its thinking must go back unchanged
    // with the tool results.
    juce::Array<juce::var> result;

    for (int i = 0; i < messages.size(); ++i)
    {
        const auto& message = messages.getReference (i);
        const auto* blocks = message.getProperty ("content", {}).getArray();

        if (i >= turnStart || blocks == nullptr)
        {
            result.add (message);
            continue;
        }

        juce::StringArray lines;

        for (const auto& block : *blocks)
        {
            const auto type = block.getProperty ("type", {}).toString();

            if (type == "text")
            {
                if (const auto text = block.getProperty ("text", {}).toString().trim(); text.isNotEmpty())
                    lines.add (shortened (text, 1500));
            }
            else if (type == "tool_use")
            {
                lines.add ("(Earlier step: " + block.getProperty ("name", {}).toString() + " "
                           + shortened (juce::JSON::toString (block.getProperty ("input", {}), true), 200) + ")");
            }
            else if (type == "tool_result")
            {
                lines.add ("(Result: " + shortened (block.getProperty ("content", {}).toString(), 300) + ")");
            }

            // Thinking blocks are left out: the AI service allows that for earlier requests.
        }

        result.add (makeMessage (message.getProperty ("role", {}).toString(),
                                 lines.isEmpty() ? juce::String ("(No reply.)") : lines.joinIntoString ("\n")));
    }

    return result;
}

void AiAgent::callModel()
{
    // The guide and the tool list are the same every step: marked for caching.
    auto cache = object();
    cache.getDynamicObject()->setProperty ("type", "ephemeral");

    auto system = textBlock (toolbox.guide());
    system.getDynamicObject()->setProperty ("cache_control", cache);

    auto toolList = toolbox.definitions();
    juce::Array<juce::var> cachedTools;

    if (const auto* list = toolList.getArray())
    {
        for (int i = 0; i < list->size(); ++i)
        {
            auto copy = juce::JSON::parse (juce::JSON::toString (list->getReference (i), true));

            if (i == list->size() - 1)
                copy.getDynamicObject()->setProperty ("cache_control", cache);

            cachedTools.add (copy);
        }
    }

    auto body = object();
    auto* fields = body.getDynamicObject();
    fields->setProperty ("system", juce::Array<juce::var> { system });
    fields->setProperty ("tools", cachedTools);
    fields->setProperty ("messages", compacted());
    fields->setProperty ("max_tokens", maxAnswerTokens);

    server.chat (body, [this, flag = alive] (const juce::Result& result, const juce::var& answer)
    {
        if (*flag)
            onAnswer (result, answer);
    });
}

void AiAgent::onAnswer (const juce::Result& result, const juce::var& answer)
{
    if (! busy)
        return;

    const auto* content = answer.getProperty ("content", {}).getArray();

    if (result.failed() || content == nullptr)
    {
        // Nothing came of the request's first step: the conversation goes back to before it.
        if (steps == 0)
            messages.removeRange (turnStart, messages.size() - turnStart);

        finish (Outcome::problem, result.failed() ? result.getErrorMessage() : juce::String ("Fanan AI's answer was empty. Please try again."));
        return;
    }

    const auto stop = answer.getProperty ("stop", {}).toString();

    if (stop == "refused")
    {
        // A declined request is taken out of the conversation, so the next one can go on.
        messages.removeRange (turnStart, messages.size() - turnStart);
        finish (Outcome::problem, "Fanan AI declined that request.");
        return;
    }

    // Tools run only when the AI asked for them: in an answer that was cut off, a call may be incomplete.
    const bool runTools = stop == "tools";

    // The answer goes back into the conversation just as it came, its thinking blocks included
    // (they must go back unchanged with the tool results); only calls that won't run are left out.
    juce::Array<juce::var> kept;
    auto& entry = currentEntry();
    calls.clear();
    nextCall = 0;
    results.clear();
    said.clear();
    stopReason = stop;

    for (const auto& block : *content)
    {
        const auto type = block.getProperty ("type", {}).toString();

        if (type == "tool_use" && ! runTools)
            continue;

        kept.add (block);

        if (type == "text")
        {
            const auto text = block.getProperty ("text", {}).toString().trim();
            const auto before = entry.getProperty ("text", {}).toString();

            if (text.isNotEmpty())
                entry.getDynamicObject()->setProperty ("text", before.isEmpty() ? text : before + "\n\n" + text);

            said << text;
        }
        else if (type == "tool_use")
        {
            calls.push_back ({ block.getProperty ("id", {}).toString(), block.getProperty ("name", {}).toString(),
                               block.getProperty ("input", {}) });
        }
    }

    if (stop == "cut")
        kept.add (textBlock ("(My answer was cut off here.)"));
    else if (kept.isEmpty())
        kept.add (textBlock ("(Done.)"));

    messages.add (makeMessage ("assistant", kept));
    notify();
    runNextCall();
}

void AiAgent::runNextCall()
{
    // One call at a time, each waiting for the one before (a build takes seconds).
    while (nextCall < calls.size())
    {
        const auto call = calls[nextCall++];

        // Stopped: what's asked isn't done any more, but every call still gets an answer.
        if (stopping)
        {
            callDone (call, juce::var());
            continue;
        }

        auto finished = std::make_shared<bool> (false);
        auto now = std::make_shared<bool> (true);

        toolbox.run (call.name, call.input, [this, call, flag = alive, request = requestNumber, finished, now] (const juce::var& outcome)
        {
            if (! *flag || request != requestNumber || *finished)
                return;

            *finished = true;
            callDone (call, outcome);

            if (! *now)
                runNextCall();   // it finished later: carry on from here
        });

        *now = false;

        if (! *finished)
            return;              // it'll carry on when the tool is done
    }

    afterCalls();
}

void AiAgent::callDone (const Call& call, const juce::var& outcome)
{
    const auto failed = stopping || outcome.hasProperty ("error");

    if (! stopping && ! toolbox.looksOnly (call.name) && ! failed)
        changed = true;

    // The user sees what was done, in plain words. A step that failed stays out of sight: the AI
    // tries another way, or says simply what it couldn't do.
    if (const auto line = failed ? juce::String() : toolbox.describe (call.name, call.input, outcome); line.isNotEmpty())
    {
        auto step = object();
        step.getDynamicObject()->setProperty ("text", line);
        step.getDynamicObject()->setProperty ("ok", true);

        if (auto* shown = currentEntry().getProperty ("steps", {}).getArray())
            shown->add (step);

        notify();
    }

    auto toolResult = object();
    auto* r = toolResult.getDynamicObject();
    r->setProperty ("type", "tool_result");
    r->setProperty ("tool_use_id", call.id);
    // Words (a skill, say) go as they are; anything else as JSON.
    r->setProperty ("content", stopping ? juce::String ("The user stopped the request.")
                                        : (outcome.isString() ? outcome.toString() : juce::JSON::toString (outcome, true)));

    if (failed)
        r->setProperty ("is_error", true);

    results.add (toolResult);
}

void AiAgent::afterCalls()
{
    if (results.isEmpty())
    {
        const auto& entry = currentEntry();
        const auto* shown = entry.getProperty ("steps", {}).getArray();
        const bool saidNothing = entry.getProperty ("text", {}).toString().isEmpty() && (shown == nullptr || shown->isEmpty());

        // A question for the user in the last words: marked to show in yellow, or at least asked.
        const bool asks = said.contains ("==") || said.containsChar ('?');

        if (stopReason == "cut")
            finish (Outcome::problem, "That answer was too long and got cut off. Ask for less at once, or say \"continue\".");
        else
            finish (asks ? Outcome::question : Outcome::done, saidNothing ? juce::String ("Fanan AI finished without a reply.") : juce::String());

        return;
    }

    messages.add (makeMessage ("user", results));
    results.clear();

    if (stopping)
    {
        finish (Outcome::problem, "Stopped.", "(The user stopped me here.)");
        return;
    }

    if (++steps >= maxSteps)
    {
        finish (Outcome::question, "Fanan AI paused after many steps. Say \"continue\" to carry on.", "(I paused here after many steps.)");
        return;
    }

    callModel();
}

void AiAgent::finish (Outcome outcome, const juce::String& notice, const juce::String& note)
{
    busy = false;
    stopping = false;
    lastOutcome = outcome;

    // A request that ended mid-way leaves the conversation where the AI can pick it up.
    if (messages.size() > 0 && messages.getReference (messages.size() - 1).getProperty ("role", {}).toString() == "user"
        && messages.getReference (messages.size() - 1).getProperty ("content", {}).isArray())
    {
        messages.add (makeMessage ("assistant", juce::Array<juce::var> { textBlock (note.isNotEmpty() ? note : juce::String ("(The request ended here.)")) }));
    }

    if (notice.isNotEmpty())
        addEntry ("notice", notice);

    if (hooks.requestEnded) hooks.requestEnded (requestText, changed);
    notify();
}

void AiAgent::stop()
{
    if (! busy)
        return;

    stopping = true;       // the answer on its way is the last
    toolbox.cancel();      // and a tool still working stops
}

void AiAgent::clear()
{
    if (busy)
        return;

    ++requestNumber;
    messages.clear();
    entries.clear();
    notify();
}

juce::var AiAgent::saveConversation() const
{
    // As requests are sent: earlier ones short (no thinking), which is all carrying on needs.
    auto saved = object();
    auto* fields = saved.getDynamicObject();
    const auto all = [this]
    {
        juce::Array<juce::var> list;

        for (const auto& message : messages)
            list.add (message);

        return list;
    }();

    // Everything counts as earlier: nothing is under way when it's kept.
    juce::Array<juce::var> shortened;

    for (const auto& message : all)
    {
        const auto* blocks = message.getProperty ("content", {}).getArray();

        if (blocks == nullptr)
        {
            shortened.add (message);
            continue;
        }

        juce::StringArray lines;

        for (const auto& block : *blocks)
        {
            const auto type = block.getProperty ("type", {}).toString();

            if (type == "text")
            {
                if (const auto text = block.getProperty ("text", {}).toString().trim(); text.isNotEmpty())
                    lines.add (text.substring (0, 1500));
            }
            else if (type == "tool_use")
            {
                lines.add ("(Earlier step: " + block.getProperty ("name", {}).toString() + ")");
            }
            else if (type == "tool_result")
            {
                lines.add ("(Result: " + block.getProperty ("content", {}).toString().substring (0, 300) + ")");
            }
        }

        shortened.add (makeMessage (message.getProperty ("role", {}).toString(),
                                    lines.isEmpty() ? juce::String ("(No reply.)") : lines.joinIntoString ("\n")));
    }

    fields->setProperty ("messages", shortened);
    fields->setProperty ("entries", entries);
    return saved;
}

void AiAgent::restoreConversation (const juce::var& saved)
{
    if (busy)
        return;

    ++requestNumber;
    messages.clear();
    entries.clear();

    if (const auto* list = saved.getProperty ("messages", {}).getArray())
        for (const auto& message : *list)
            if (message.getProperty ("content", {}).isString()
                && (message.getProperty ("role", {}).toString() == "user" || message.getProperty ("role", {}).toString() == "assistant"))
                messages.add (makeMessage (message.getProperty ("role", {}).toString(), message.getProperty ("content", {}).toString()));

    // The AI service wants the conversation to go user, assistant, user...: anything else is dropped.
    juce::Array<juce::var> ordered;
    juce::String expected = "user";

    for (const auto& message : messages)
    {
        if (message.getProperty ("role", {}).toString() != expected)
            continue;

        ordered.add (message);
        expected = expected == "user" ? "assistant" : "user";
    }

    if (! ordered.isEmpty() && ordered.getReference (ordered.size() - 1).getProperty ("role", {}).toString() == "user")
        ordered.removeLast();

    messages = ordered;
    turnStart = messages.size();

    if (const auto* list = saved.getProperty ("entries", {}).getArray())
        for (const auto& entry : *list)
            if (entry.isObject())
                entries.add (juce::JSON::parse (juce::JSON::toString (entry, true)));

    notify();
}

juce::var AiAgent::getState() const
{
    auto state = object();
    auto* fields = state.getDynamicObject();
    fields->setProperty ("busy", busy);
    fields->setProperty ("stopping", stopping);
    fields->setProperty ("steps", steps);
    fields->setProperty ("entries", entries);
    return state;
}
