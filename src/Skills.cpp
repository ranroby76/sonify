// D:\workspace\Fanan_web_hub\src\Skills.cpp

#include "Skills.h"

#include "FananSkills.h"

#include <algorithm>
#include <vector>

namespace
{
    struct Skill
    {
        juce::String name, readers, title, description, text;
    };

    /** Every skill built in, read once from its file (see Skills.h for the card). */
    const std::vector<Skill>& all()
    {
        static const std::vector<Skill> skills = []
        {
            std::vector<Skill> found;

            for (int i = 0; i < FananSkills::namedResourceListSize; ++i)
            {
                int size = 0;
                const auto* data = FananSkills::getNamedResource (FananSkills::namedResourceList[i], size);

                if (data == nullptr || size <= 0)
                    continue;

                auto content = juce::String::fromUTF8 (data, size).replace ("\r\n", "\n");

                // The file's path title is for the project, not for the AI.
                if (content.startsWith ("<!--"))
                    content = content.fromFirstOccurrenceOf ("-->", false, false);

                content = content.trimStart();

                if (! content.startsWith ("---"))
                    continue;

                const auto afterOpening = content.fromFirstOccurrenceOf ("---", false, false);
                const auto card = afterOpening.upToFirstOccurrenceOf ("\n---", false, false);

                Skill skill;
                skill.text = afterOpening.fromFirstOccurrenceOf ("\n---", false, false).trim();

                for (const auto& line : juce::StringArray::fromLines (card))
                {
                    const auto key = line.upToFirstOccurrenceOf (":", false, false).trim().toLowerCase();
                    const auto value = line.fromFirstOccurrenceOf (":", false, false).trim();

                    if (key == "name")              skill.name = value;
                    else if (key == "for")          skill.readers = value.toLowerCase();
                    else if (key == "title")        skill.title = value;
                    else if (key == "description")  skill.description = value;
                }

                if (skill.name.isNotEmpty() && skill.text.isNotEmpty())
                    found.push_back (skill);
            }

            std::sort (found.begin(), found.end(), [] (const Skill& a, const Skill& b) { return a.name < b.name; });
            return found;
        }();

        return skills;
    }

    bool isFor (const Skill& skill, Skills::For readers)
    {
        return skill.readers == "all" || skill.readers == (readers == Skills::For::plugins ? "plugins"
                                                         : readers == Skills::For::devices ? "devices" : "mixing");
    }

    const Skill* find (const juce::String& name)
    {
        const auto wanted = name.trim();

        for (const auto& skill : all())
            if (skill.name.equalsIgnoreCase (wanted) || skill.title.equalsIgnoreCase (wanted))
                return &skill;

        return nullptr;
    }

    juce::StringArray namesFor (Skills::For readers)
    {
        juce::StringArray names;

        for (const auto& skill : all())
            if (isFor (skill, readers))
                names.add (skill.name);

        return names;
    }
}

//==============================================================================
juce::String Skills::index (For readers)
{
    juce::String text;

    for (const auto& skill : all())
        if (isFor (skill, readers))
            text << "- " << skill.name << ": " << skill.description << "\n";

    if (text.isEmpty())
        return {};

    return "## Skills\n"
           "Your know-how for the jobs below is kept out of this guide, so every request stays small. Before you plan or write "
           "that kind of work, read the skill that fits with `read_skill` (it changes nothing and needs no plan); a skill you "
           "read stays with you for the rest of the request, so read it again in a later request when you need it again. "
           "Read only what the job needs: one or two skills, not all of them.\n"
         + text;
}

juce::var Skills::toolDefinition (For readers)
{
    auto* nameProperty = new juce::DynamicObject();
    nameProperty->setProperty ("type", "string");
    nameProperty->setProperty ("description", "The skill's name, from the guide's \"Skills\" list.");

    juce::Array<juce::var> names;

    for (const auto& name : namesFor (readers))
        names.add (name);

    if (! names.isEmpty())
        nameProperty->setProperty ("enum", names);

    auto* properties = new juce::DynamicObject();
    properties->setProperty ("name", juce::var (nameProperty));

    auto* schema = new juce::DynamicObject();
    schema->setProperty ("type", "object");
    schema->setProperty ("properties", juce::var (properties));
    schema->setProperty ("required", juce::Array<juce::var> { "name" });

    auto* definition = new juce::DynamicObject();
    definition->setProperty ("name", "read_skill");
    definition->setProperty ("description", "Reads one of your skills (the guide's \"Skills\" list): know-how for one kind of job, with recipes "
                                            "and complete code to build on. Changes nothing.");
    definition->setProperty ("input_schema", juce::var (schema));
    return juce::var (definition);
}

juce::Result Skills::read (For readers, const juce::String& name, juce::String& text)
{
    const auto* skill = find (name);

    if (skill == nullptr || ! isFor (*skill, readers))
    {
        const auto names = namesFor (readers);
        return juce::Result::fail ("There's no skill called \"" + name + "\"."
                                   + (names.isEmpty() ? juce::String() : " The skills are: " + names.joinIntoString (", ") + "."));
    }

    text = "Skill \"" + skill->name + "\":\n\n" + skill->text;
    return juce::Result::ok();
}

juce::String Skills::titleOf (const juce::String& name)
{
    if (const auto* skill = find (name); skill != nullptr && skill->title.isNotEmpty())
        return skill->title.toLowerCase();

    return name.replaceCharacter ('-', ' ');
}

juce::StringArray Skills::headings (const juce::String& name)
{
    juce::StringArray found;

    if (const auto* skill = find (name))
        for (const auto& line : juce::StringArray::fromLines (skill->text))
            if (line.startsWith ("### "))
                if (const auto heading = line.substring (4).trim(); heading.isNotEmpty())
                    found.addIfNotAlreadyThere (heading);

    return found;
}
