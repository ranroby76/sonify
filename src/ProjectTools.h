// D:\workspace\Fanan_web_hub\src\ProjectTools.h

#pragma once

#include <juce_core/juce_core.h>

#include "CodeReference.h"
#include "LivePreview.h"
#include "Project.h"

#include <atomic>
#include <functional>
#include <memory>
#include <optional>

class GuiLayout;

//==============================================================================
/**
    Sonify AI's hands: the tools it calls to look at the project, write its modules, wire
    the graph and build it. The studio runs every call itself, on its own terms: files
    only inside the project's modules folder and graph.json (and the user's own code in
    reference/, read only), and builds through the live preview, so whatever the AI builds
    plays at once.

    Two sets of tools: in a conversation, the AI can look, design the GUI (set_layout for a
    whole design, edit_layout for part of it, by the element indexes the project's
    description lists) and hand over to the builder (start_building); the builder can also
    change the code and build it. Both can read their skills (read_skill, Skills.h), listen to
    the plugin (analyze_sound, SoundCheck.h) and paint pictures for its panel with a
    programmed element (paint_picture); the last two take a moment, on a thread of their own.
*/
class ProjectTools final
{
public:
    ProjectTools (Project& project, LivePreview& preview);
    ~ProjectTools();

    /** The tool definitions sent with each request. */
    juce::var getDefinitions (bool builder) const;

    /** What the AI is told about the project with every request. */
    juce::String describeProject() const;

    /** Whether the open project has ever built and played. */
    bool hasBuiltOnce() const noexcept    { return project.isOpen() && project.getInfo().firstBuilt.isNotEmpty(); }

    /** The plugin's design brief, design.md in the project: written first, by Fanan AI's
        foundation builder (the strongest model), then read by the builder that builds it, and
        kept up to date. Conversations aren't kept: this is the plugin's memory. */
    static constexpr const char* designFileName = "design.md";
    bool hasDesign() const;

    /** The user's own code (CodeReference.h): reference/ in the open project, read only, which
        Fanan AI reads (read_file) and searches (search_reference) as it needs. Brought before
        there's a project, it waits in a folder of its own and goes into the project Fanan AI
        creates (or the next one that opens); code still waiting when Sonify starts is gone. */
    CodeReference getReference() const;
    CodeReference::Added addReference (const juce::Array<juce::File>& sources);
    static constexpr const char* referenceFolderName = "reference";

    /** The builder guide with the DSP API: the same with every request (so it's cached). */
    static const juce::String& getGuide();

    using Done = std::function<void (const juce::String& result, bool isError)>;

    /** Runs one tool call on the message thread. done is called when it's finished (for
        build, once the plugin is built and playing, or has failed). */
    void run (const juce::String& name, const juce::var& input, Done done);

    /** One short line for the chat about what a call is doing (empty: nothing to show). */
    static juce::String describeCall (const juce::String& name, const juce::var& input);

    /** Called on the message thread when the AI has changed the GUI layout. */
    std::function<void()> onLayoutChanged;

    /** Called on the message thread when the AI has saved a preset. */
    std::function<void()> onPresetsChanged;

    /** Creates and opens a new project (the studio's New, done for the AI). */
    std::function<juce::Result (const juce::String& name, PluginKind kind)> onCreateProject;

    /** The type the user chose beside Send for the plugin they're starting (none: not chosen).
        create_project makes that type, and only that: the AI never picks it. */
    void setChosenKind (std::optional<PluginKind> kind)    { chosenKind = kind; }

private:
    static juce::File waitingReferenceFolder();
    juce::Result resolve (const juce::String& path, juce::File& file, bool forWriting) const;
    juce::String buildResult (bool ok) const;

    /** What's wrong with a GUI the AI made (parameters, pictures or looks that don't exist),
        as lines for its tool result; before is the GUI it replaced. */
    juce::String checkLayout (const GuiLayout& layout, const GuiLayout& before) const;

    void analyzeSound (const juce::var& input, Done done);
    void paintPicture (const juce::var& input, Done done);
    void makeLook (const juce::var& input, Done done);   // make_look: a look in a material

    Project& project;
    LivePreview& preview;
    std::optional<PluginKind> chosenKind;

    // analyze_sound and paint_picture work here, and answer on the message thread while the
    // tools are still alive.
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    juce::ThreadPool worker { juce::ThreadPoolOptions{}.withThreadName ("Fanan AI tools").withNumberOfThreads (1) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectTools)
};
