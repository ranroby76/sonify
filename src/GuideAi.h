// D:\workspace\Fanan_web_hub\src\GuideAi.h

#pragma once

#include <juce_core/juce_core.h>

#include "FananServer.h"

#include <atomic>
#include <map>
#include <memory>
#include <vector>

//==============================================================================
/**
    The Guide's "Ask anything about Sonify": Fanan AI answering the user's questions about how to
    use Sonify and each of its studios, from Sonify's own guide (sonify_guide.md, the same text
    the Guide studio shows). It only answers: it changes nothing in the song (the AI Assistant
    does that).

    The whole guide is longer than one request may carry, so Fanan AI gets its contents (every tab
    and chapter, with their headings) and reads the chapters it needs with one tool, read_guide.
    The chapters a quick word search finds for the question, and the one the user is reading, go
    with the question, so most answers need no reading step.

    Each step is one request to Fanan's server, with the Fanan account's credits, like any Fanan
    AI request; the instructions and the contents are the same every time, marked for caching.
    The conversation is kept short (the last few questions and answers go with each one), and
    only for this run of Sonify.

    Message thread only.
*/
class GuideAi final
{
public:
    GuideAi (FananServer& server, const juce::String& guideText);
    ~GuideAi();

    /** A question; reading: the chapter the user has open ("Studio > Recording"), or empty. */
    void ask (const juce::String& question, const juce::String& reading);

    /** The question under way is let go: its answer, when it comes, is dropped. */
    void stop();

    /** The conversation starts afresh. */
    void clear();

    bool isBusy() const noexcept    { return busy; }

    /** The Guide's "Translate this page to": one chapter's text (Markdown, as the guide has it) goes to
        Fanan AI, with the language's name, and comes back in that language. id is the page's own number
        for it. Apart from the questions: a translation and a question can be under way together. */
    void translate (int id, const juce::String& language, const juce::String& text);

    /** A translation is done: ok with its text, or not with why (a message the user can read). */
    std::function<void (int id, bool ok, const juce::String& textOrError)> onTranslated;

    /** What the Guide's panel shows: { busy, entries: [{ role: "user" | "ai" | "notice", text }] }. */
    juce::var getState() const;

    /** The state changed (a question went, an answer came). */
    std::function<void()> onChanged;

    //==============================================================================
    /** For tests: the request the next step would send, and what read_guide gives. */
    juce::var requestForTest() const                            { return request(); }
    juce::String readForTest (const juce::var& input) const     { return read (input); }
    int chapterCount() const noexcept                           { return (int) chapters.size(); }

private:
    struct Chapter
    {
        juce::String tab, title, text;
        std::map<juce::String, int> words;          // its words and how often each comes, for the quick search
        juce::StringArray titleWords, tabWords, headingWords;
    };

    struct Entry
    {
        juce::String role, text;
    };

    void parse (const juce::String& guideText);
    juce::String contents() const;
    int find (const juce::String& name) const;
    juce::String chapterBlock (int index) const;
    std::vector<int> likely (const juce::String& question, int reading) const;
    juce::String read (const juce::var& input) const;

    juce::var request() const;
    void send();
    void answered (const juce::Result& result, const juce::var& answer);
    void finish (const juce::String& answer);
    void fail (const juce::String& message);
    void changed();

    FananServer& server;
    std::vector<Chapter> chapters;
    juce::String system;                 // how to answer, then the guide's contents
    std::vector<Entry> entries;          // what the panel shows
    juce::Array<juce::var> history;      // earlier questions and answers, as plain text, in turn
    juce::Array<juce::var> turn;         // the question under way: its messages, reading steps included
    juce::String asking;                 // the question under way, as it goes into the history
    int reads = 0;                       // reading steps in the question under way
    bool busy = false;
    int asked = 0;                       // moves on with each question, Stop and Clear: a late answer is dropped
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuideAi)
};
