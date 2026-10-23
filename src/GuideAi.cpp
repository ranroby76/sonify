// D:\workspace\Fanan_web_hub\src\GuideAi.cpp

#include "GuideAi.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr int maxAnswerTokens = 1500;
    constexpr int keptMessages = 6;            // the last three questions and answers go with a new one
    constexpr int keptHistory = 20;            // more than that is let go
    constexpr int maxQuestionLength = 2000;
    constexpr int maxReads = 3;                // reading steps for one question
    constexpr int maxChaptersPerRead = 4;
    constexpr int maxFound = 3;                // chapters that go with the question
    constexpr int maxFoundLength = 16000;      // their text, at most

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

    /** How Fanan AI answers here. The guide's contents follow it. */
    const char* const instructions =
        "You're Fanan AI, in Sonify's Guide (Sonify is a music studio, a DAW, by Fanan). Here you answer the user's "
        "questions about how to use Sonify and each of its studios, from Sonify's user guide. In the Guide you only "
        "answer: you change nothing in Sonify, and your one tool is read_guide (the AI Assistant, the button at the "
        "lower right of the transport, is where Fanan AI works in the song).\n"
        "\n"
        "The guide:\n"
        "- Its contents are below: each \"# \" line is a tab of the guide, each \"## \" line a chapter in it, with the "
        "headings inside that chapter.\n"
        "- The chapters a quick word search found come with the question. When they don't hold the answer, call "
        "read_guide with the chapters that do (up to 4 at a time), then answer. Sonify's buttons, menus and steps are "
        "only in the guide: never answer from what other music software does.\n"
        "\n"
        "How to answer:\n"
        "- Answer the question first, directly, in short plain words: what to click or press, in order. Name buttons, "
        "menus, tabs and keys exactly as the guide does, in **bold**.\n"
        "- Steps as lines starting \"1.\", \"2.\"...; other lists as lines starting \"- \". No headings, tables, code or links.\n"
        "- Say only what the guide says Sonify does. When the guide doesn't cover something, say you're not sure Sonify "
        "has it, and point to the nearest chapter. Never invent a button, a menu, a shortcut or a feature.\n"
        "- At the end, point to one or two chapters to read more, written exactly as [[Tab > Chapter]] with the names "
        "as the contents have them, e.g. [[Studio > Recording]]. The Guide turns them into links.\n"
        "- Answer in the language the user writes in (keep the names of buttons and menus as the guide has them).\n"
        "- Keep it under about 180 words, unless the question asks for a longer walkthrough.\n"
        "- The user may say which chapter they're reading: use it to understand what \"this\" or \"here\" means.\n"
        "\n"
        "The guide's contents:\n"
        "\n";

    juce::var readGuideTool()
    {
        auto item = object();
        item.getDynamicObject()->setProperty ("type", "string");

        auto list = object();
        list.getDynamicObject()->setProperty ("type", "array");
        list.getDynamicObject()->setProperty ("items", item);
        list.getDynamicObject()->setProperty ("description", "Chapters as the contents name them, \"Tab > Chapter\", e.g. \"Studio > Recording\".");

        auto properties = object();
        properties.getDynamicObject()->setProperty ("chapters", list);

        auto schema = object();
        schema.getDynamicObject()->setProperty ("type", "object");
        schema.getDynamicObject()->setProperty ("properties", properties);
        schema.getDynamicObject()->setProperty ("required", juce::Array<juce::var> { "chapters" });

        auto cache = object();
        cache.getDynamicObject()->setProperty ("type", "ephemeral");

        auto tool = object();
        tool.getDynamicObject()->setProperty ("name", "read_guide");
        tool.getDynamicObject()->setProperty ("description", "Gives the full text of chapters of Sonify's user guide (up to 4 at a time). "
                                                             "Read the chapters that answer the user's question before answering.");
        tool.getDynamicObject()->setProperty ("input_schema", schema);
        tool.getDynamicObject()->setProperty ("cache_control", cache);
        return tool;
    }

    //==============================================================================
    // The quick word search.

    bool isStopWord (const juce::String& word)
    {
        static const juce::StringArray words { "the", "and", "how", "can", "what", "does", "doe", "with", "for", "you", "your",
                                               "this", "that", "from", "into", "sonify", "use", "want", "there", "where", "when",
                                               "which", "make", "get", "set", "its", "are", "was", "will", "would", "should",
                                               "could", "have", "has", "not", "but", "all", "any", "one", "out", "about", "then",
                                               "than", "them", "they", "way", "why", "who", "please", "need", "like", "just",
                                               "more", "some", "also", "only", "each", "here", "it's", "i'm", "don't", "can't" };
        return words.contains (word);
    }

    juce::String stem (juce::String word)
    {
        if (word.length() > 5 && word.endsWith ("ing"))
            return word.dropLastCharacters (3);

        if (word.length() > 4 && word.endsWith ("ed"))
            return word.dropLastCharacters (2);

        if (word.length() > 3 && word.endsWith ("s") && ! word.endsWith ("ss"))
            return word.dropLastCharacters (1);

        return word;
    }

    juce::StringArray wordsOf (const juce::String& text)
    {
        juce::StringArray words;
        juce::String word;

        auto take = [&]
        {
            if (word.length() >= 3 && ! isStopWord (word))
                words.add (stem (word));

            word.clear();
        };

        const auto lower = text.toLowerCase();

        for (auto p = lower.getCharPointer(); ! p.isEmpty(); ++p)
        {
            const auto c = *p;

            if (juce::CharacterFunctions::isLetterOrDigit (c) || (c == '\'' && word.isNotEmpty()))
                word += c;
            else
                take();
        }

        take();
        return words;
    }

    juce::String plainName (const juce::String& name)
    {
        auto s = name.trim().removeCharacters ("[]\"*").trim().toLowerCase();

        while (s.contains ("  "))
            s = s.replace ("  ", " ");

        return s;
    }
}

//==============================================================================
GuideAi::GuideAi (FananServer& s, const juce::String& guideText)
    : server (s)
{
    parse (guideText);
    system = juce::String (instructions) + contents();
}

GuideAi::~GuideAi()
{
    alive->store (false);
}

void GuideAi::parse (const juce::String& guideText)
{
    // "# Tab", "## Chapter", then the chapter's lines (an HTML comment line is the file's title).
    juce::String tab;
    juce::StringArray lines;
    lines.addLines (guideText);

    auto close = [this]
    {
        if (! chapters.empty())
            chapters.back().text = chapters.back().text.trim();
    };

    for (const auto& line : lines)
    {
        const auto trimmed = line.trimEnd();

        if (trimmed.trimStart().startsWith ("<!--"))
            continue;

        if (trimmed.startsWith ("# "))
        {
            close();
            tab = trimmed.substring (2).trim();
            continue;
        }

        if (trimmed.startsWith ("## "))
        {
            close();
            chapters.push_back ({});
            chapters.back().tab = tab;
            chapters.back().title = trimmed.substring (3).trim();
            continue;
        }

        if (! chapters.empty())
            chapters.back().text << trimmed << "\n";
    }

    close();

    for (auto& chapter : chapters)
    {
        for (const auto& word : wordsOf (chapter.text))
            ++chapter.words[word];

        chapter.titleWords = wordsOf (chapter.title);
        chapter.tabWords = wordsOf (chapter.tab);

        juce::StringArray lines;
        lines.addLines (chapter.text);

        for (const auto& line : lines)
            if (line.startsWith ("### "))
                chapter.headingWords.addArray (wordsOf (line.substring (4)));
    }
}

juce::String GuideAi::contents() const
{
    juce::String text, tab;

    for (const auto& chapter : chapters)
    {
        if (chapter.tab != tab || text.isEmpty())
        {
            tab = chapter.tab;
            text << (text.isEmpty() ? "" : "\n") << "# " << tab << "\n";
        }

        juce::StringArray headings, lines;
        lines.addLines (chapter.text);

        for (const auto& line : lines)
            if (line.startsWith ("### "))
                headings.add (line.substring (4).trim());

        text << "## " << chapter.title;

        if (! headings.isEmpty())
            text << ": " << headings.joinIntoString ("; ");

        text << "\n";
    }

    return text;
}

int GuideAi::find (const juce::String& name) const
{
    const auto wanted = plainName (name);

    if (wanted.isEmpty())
        return -1;

    // "Tab > Chapter"
    if (wanted.contains (">"))
    {
        const auto tab = wanted.upToFirstOccurrenceOf (">", false, false).trim();
        const auto title = wanted.fromFirstOccurrenceOf (">", false, false).trim();

        for (size_t i = 0; i < chapters.size(); ++i)
            if (plainName (chapters[i].tab) == tab && plainName (chapters[i].title) == title)
                return (int) i;

        return find (title);
    }

    // A chapter by its title alone, or a tab (its first chapter).
    for (size_t i = 0; i < chapters.size(); ++i)
        if (plainName (chapters[i].title) == wanted)
            return (int) i;

    for (size_t i = 0; i < chapters.size(); ++i)
        if (plainName (chapters[i].tab) == wanted)
            return (int) i;

    return -1;
}

juce::String GuideAi::chapterBlock (int index) const
{
    const auto& chapter = chapters[(size_t) index];
    return "# " + chapter.tab + "\n## " + chapter.title + "\n" + chapter.text;
}

std::vector<int> GuideAi::likely (const juce::String& question, int reading) const
{
    // Each word of the question counts for more the fewer chapters have it, and most in a title
    // (then in a heading inside the chapter, then in the tab's name).
    const auto count = (int) chapters.size();
    std::vector<double> scores ((size_t) count, 0.0);
    juce::StringArray questionWords = wordsOf (question);
    questionWords.removeDuplicates (false);

    for (const auto& word : questionWords)
    {
        int having = 0;

        for (const auto& chapter : chapters)
            if (chapter.words.count (word) > 0 || chapter.titleWords.contains (word) || chapter.tabWords.contains (word))
                ++having;

        if (having == 0)
            continue;

        const auto weight = std::log (1.0 + (double) count / (double) having);

        for (int i = 0; i < count; ++i)
        {
            const auto& chapter = chapters[(size_t) i];
            const auto found = chapter.words.find (word);
            auto score = found != chapter.words.end() ? (double) juce::jmin (found->second, 4) : 0.0;

            if (chapter.titleWords.contains (word))
                score += 5.0;

            if (chapter.headingWords.contains (word))
                score += 3.0;

            if (chapter.tabWords.contains (word))
                score += 2.0;

            scores[(size_t) i] += weight * score;
        }
    }

    std::vector<int> order;

    for (int i = 0; i < count; ++i)
        if (scores[(size_t) i] > 0.0 && i != reading)
            order.push_back (i);

    std::stable_sort (order.begin(), order.end(), [&] (int a, int b) { return scores[(size_t) a] > scores[(size_t) b]; });

    std::vector<int> picked;
    int length = 0;

    auto pick = [&] (int index)
    {
        const auto size = chapterBlock (index).length();

        if ((int) picked.size() < maxFound && length + size <= maxFoundLength)
        {
            picked.push_back (index);
            length += size;
        }
    };

    if (juce::isPositiveAndBelow (reading, count))
        pick (reading);

    const auto best = order.empty() ? 0.0 : scores[(size_t) order.front()];

    for (const auto index : order)
        if (scores[(size_t) index] >= best * 0.35)
            pick (index);

    std::sort (picked.begin(), picked.end());
    return picked;
}

juce::String GuideAi::read (const juce::var& input) const
{
    juce::StringArray names;

    if (const auto* list = input.getProperty ("chapters", {}).getArray())
        for (const auto& name : *list)
            names.add (name.toString());
    else if (input.getProperty ("chapters", {}).isString())
        names.add (input.getProperty ("chapters", {}).toString());

    juce::StringArray parts;
    std::vector<int> given;

    for (int i = 0; i < names.size(); ++i)
    {
        if (i >= maxChaptersPerRead)
        {
            parts.add ("(Only 4 chapters at a time: ask again for the others.)");
            break;
        }

        const auto index = find (names[i]);

        if (index < 0)
            parts.add ("(There's no chapter \"" + names[i] + "\": use the names in the contents, as \"Tab > Chapter\".)");
        else if (std::find (given.begin(), given.end(), index) == given.end())
        {
            given.push_back (index);
            parts.add (chapterBlock (index));
        }
    }

    return parts.isEmpty() ? juce::String ("(No chapter was asked for: give \"chapters\", as \"Tab > Chapter\".)")
                           : parts.joinIntoString ("\n\n");
}

//==============================================================================
void GuideAi::ask (const juce::String& question, const juce::String& reading)
{
    const auto text = question.trim().substring (0, maxQuestionLength);

    if (text.isEmpty() || busy)
        return;

    entries.push_back ({ "user", text });

    // Where the user is reading, so "this" and "here" have a meaning.
    const auto readingIndex = reading.isNotEmpty() ? find (reading) : -1;
    asking = (readingIndex >= 0 ? "(I'm reading the chapter " + chapters[(size_t) readingIndex].tab + " > "
                                      + chapters[(size_t) readingIndex].title + ".)\n\n"
                                : juce::String())
           + text;

    // The chapters that likely answer it go with it.
    juce::String first;
    const auto found = likely (text, readingIndex);

    if (! found.empty())
    {
        first << "Guide chapters a quick word search found for this question (read_guide gives any other):\n\n";

        for (const auto index : found)
            first << chapterBlock (index) << "\n\n";

        first << "---\n\n";
    }

    first << asking;

    turn.clear();
    turn.add (makeMessage ("user", first));
    reads = 0;
    busy = true;
    ++asked;
    send();
    changed();
}

void GuideAi::stop()
{
    if (! busy)
        return;

    ++asked;
    busy = false;
    turn.clear();
    entries.push_back ({ "notice", "Stopped." });
    changed();
}

void GuideAi::clear()
{
    ++asked;
    busy = false;
    entries.clear();
    history.clear();
    turn.clear();
    changed();
}

juce::var GuideAi::getState() const
{
    juce::Array<juce::var> shown;

    for (const auto& entry : entries)
    {
        auto item = object();
        item.getDynamicObject()->setProperty ("role", entry.role);
        item.getDynamicObject()->setProperty ("text", entry.text);
        shown.add (item);
    }

    auto state = object();
    state.getDynamicObject()->setProperty ("busy", busy);
    state.getDynamicObject()->setProperty ("entries", shown);
    return state;
}

//==============================================================================
juce::var GuideAi::request() const
{
    // The instructions, the contents and the tool are the same every time: marked for caching.
    auto cache = object();
    cache.getDynamicObject()->setProperty ("type", "ephemeral");

    auto block = textBlock (system);
    block.getDynamicObject()->setProperty ("cache_control", cache);

    // The last few questions and answers (starting with a question), then the one under way.
    juce::Array<juce::var> messages;
    auto from = juce::jmax (0, history.size() - keptMessages);

    while (from < history.size() && history.getReference (from).getProperty ("role", {}).toString() != "user")
        ++from;

    for (int i = from; i < history.size(); ++i)
        messages.add (history.getReference (i));

    messages.addArray (turn);

    auto body = object();
    auto* fields = body.getDynamicObject();
    fields->setProperty ("system", juce::Array<juce::var> { block });
    fields->setProperty ("tools", juce::Array<juce::var> { readGuideTool() });
    fields->setProperty ("messages", messages);
    fields->setProperty ("max_tokens", maxAnswerTokens);
    return body;
}

void GuideAi::send()
{
    server.chat (request(), [this, flag = alive, thisQuestion = asked] (const juce::Result& result, const juce::var& answer)
    {
        if (flag->load() && busy && thisQuestion == asked)
            answered (result, answer);
    });
}

void GuideAi::answered (const juce::Result& result, const juce::var& answer)
{
    if (result.failed())
    {
        fail (result.getErrorMessage());
        return;
    }

    const auto ending = answer.getProperty ("stop", {}).toString();

    if (ending == "refused")
    {
        fail ("Fanan AI declined that question.");
        return;
    }

    juce::Array<juce::var> kept;
    juce::String text;
    juce::Array<juce::var> calls;

    if (const auto* content = answer.getProperty ("content", {}).getArray())
    {
        for (const auto& block : *content)
        {
            const auto type = block.getProperty ("type", {}).toString();

            if (type == "tool_use" && ending != "tools")
                continue;   // in an answer that was cut off, a call may be incomplete

            kept.add (block);

            if (type == "text")
            {
                const auto words = block.getProperty ("text", {}).toString().trim();

                if (words.isNotEmpty())
                    text << (text.isNotEmpty() ? "\n\n" : "") << words;
            }
            else if (type == "tool_use")
            {
                calls.add (block);
            }
        }
    }

    if (text.isEmpty())
        text = answer.getProperty ("reply", {}).toString().trim();

    // Fanan AI reads chapters: they go back to it, and it answers in the next step.
    if (! calls.isEmpty() && reads < maxReads)
    {
        ++reads;
        juce::Array<juce::var> results;

        for (const auto& call : calls)
        {
            auto output = call.getProperty ("name", {}).toString() == "read_guide"
                              ? read (call.getProperty ("input", {}))
                              : juce::String ("(There's no such tool here: only read_guide.)");

            if (reads == maxReads)
                output << "\n\n(That was the last reading step for this question: answer now, from what you have.)";

            auto block = object();
            block.getDynamicObject()->setProperty ("type", "tool_result");
            block.getDynamicObject()->setProperty ("tool_use_id", call.getProperty ("id", {}));
            block.getDynamicObject()->setProperty ("content", output);
            results.add (block);
        }

        turn.add (makeMessage ("assistant", kept));
        turn.add (makeMessage ("user", results));
        send();
        return;
    }

    if (text.isEmpty())
    {
        fail (calls.isEmpty() ? "Fanan AI didn't answer that. Please ask again."
                              : "Fanan AI couldn't find that in the guide. Please ask again, in other words.");
        return;
    }

    if (ending == "cut")
        text << "\n\n(The answer was cut off here.)";

    finish (text);
}

void GuideAi::finish (const juce::String& answer)
{
    history.add (makeMessage ("user", asking));
    history.add (makeMessage ("assistant", answer));

    while (history.size() > keptHistory)
        history.removeRange (0, 2);

    entries.push_back ({ "ai", answer });
    turn.clear();
    busy = false;
    changed();
}

void GuideAi::fail (const juce::String& message)
{
    // Not answered: the question stays out of what's sent next.
    entries.push_back ({ "notice", message.isNotEmpty() ? message : juce::String ("Fanan AI couldn't answer that. Please try again.") });
    turn.clear();
    busy = false;
    changed();
}

void GuideAi::changed()
{
    if (onChanged != nullptr)
        onChanged();
}
