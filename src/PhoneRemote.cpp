// D:\workspace\Fanan_web_hub\src\PhoneRemote.cpp

#include "PhoneRemote.h"

PhoneRemote::PhoneRemote (FananServer& s)
    : server (s)
{
}

PhoneRemote::~PhoneRemote()
{
    *alive = false;   // an answer arriving later finds no one
    stopTimer();

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
        startTimer (pollMs);
        timerCallback();   // at once
    }
    else
    {
        stopTimer();

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
    // It works through Fanan's server, like Sonify AI.
    if (! server.isConfigured() || ! server.hasSite())
        return "Sonify Remote isn't set up in this build yet.";

    if (server.isBlocked())
        return "The internet is blocked (the switch at the top left): Sonify Remote works through it.";

    if (! server.isReady())
    {
        const auto status = server.getStatus();

        return status == FananServer::Status::checking   ? juce::String::fromUTF8 ("Connecting to Fanan's server\xe2\x80\xa6")
             : status == FananServer::Status::noInternet ? juce::String ("No internet connection: Sonify Remote works through it.")
                                                         : juce::String ("Fanan's server isn't responding right now.");
    }

    if (! server.isSignedIn())
        return "Sign in to use Sonify Remote: the phone signs in to the same Fanan account.";

    return {};
}

//==============================================================================
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
    const auto state = stateJson (getConversation != nullptr ? getConversation() : juce::var(), server.getCredits(), taken);
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

        // What the phone asked for: handed over once.
        if (const auto request = answer.getProperty ("request", {}); request.isObject())
        {
            taken = request.getProperty ("id", {}).toString();   // the phone sees it arrive

            if (request.getProperty ("kind", {}).toString() == "stop")
            {
                if (onStop != nullptr)
                    onStop();
            }
            else if (const auto text = request.getProperty ("text", {}).toString().trim(); text.isNotEmpty() && onAsk != nullptr)
            {
                onAsk (text.substring (0, maxRequestChars), static_cast<bool> (request.getProperty ("spoken", false)));
            }
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
juce::String PhoneRemote::stateJson (const juce::var& aiState, int credits, const juce::String& taken)
{
    juce::DynamicObject::Ptr top (new juce::DynamicObject());
    top->setProperty ("busy", static_cast<bool> (aiState.getProperty ("busy", false)));

    if (credits >= 0)
        top->setProperty ("credits", credits);

    top->setProperty ("taken", taken);
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
