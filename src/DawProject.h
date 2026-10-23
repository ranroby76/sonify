// D:\workspace\Fanan_web_hub\src\DawProject.h

#pragma once

#include <juce_core/juce_core.h>

#include "AudioEngine.h"

//==============================================================================
/**
    DAWproject files (.dawproject, the open format Bitwig, Studio One, Cubase, Cubasis, VST Live
    and n-Track read and write): File > Import DAWproject... and File > Export DAWproject....

    A .dawproject is a zip: project.xml (tracks, channels, plugins, clips, notes, automation),
    metadata.xml (title and so on), the audio files, and each plugin's state as a file of its own
    (a VST3 plugin's as a .vstpreset).

    Import makes a new song (its own folder, as File > New Song does) and nothing else:
      - the tempo and time signature, with their changes;
      - tracks: instrument tracks (notes), audio tracks (audio clips), FX channels (effect
        channels) with the sends to them, and buses (group tracks, the tracks in them playing
        into them); each one's name, colour, volume, pan, mute and solo; Main's volume and mute;
      - clips: MIDI clips with their notes (loops played out as notes), controllers (CC, pitch
        bend, channel pressure); audio clips with their window onto the file and their fades;
      - automation of the tracks' and Main's volume and pan, and of the parameters of the VST3
        plugins that came across;
      - VST3 plugins installed here (found by their class id), with their settings.
    Plugins that can't play here (not installed, CLAP, AU, VST2, another DAW's own devices) stay
    in the song as placeholders (Track::kept): their names and settings come with the song when
    it's saved, and Export DAWproject writes them back where they were.

    Export writes the song as one: the tracks, buses, FX channels and Main, VST3 plugins with
    their settings (.vstpreset), the placeholders an import kept, the clips (audio files inside the
    file), the tempo and time signature changes and the automation. Sonify's own instruments and
    effects (and WCLAP plugins, which no other DAW plays) aren't in it: each track's comment, and
    the project's, list them.
*/
namespace DawProject
{
    inline constexpr const char* fileExtension = ".dawproject";

    /** A VST3 plugin in Sonify's Browser, as the import looks for it. */
    struct InstalledVst3
    {
        juce::String file, id, name;
    };

    /** Reads source into a new song: songFolder/<songFolder's name>.fsong, its audio in
        songFolder/Media and the placeholders' settings in songFolder/Plugin States. songFile is
        the song written. notes: what didn't come across as it was. Message thread. */
    juce::Result importProject (const juce::File& source, const juce::File& songFolder, const std::vector<InstalledVst3>& installed,
                                juce::File& songFile, juce::StringArray& notes);

    /** The title a .dawproject gives its song (its metadata's, or its file's name). */
    juce::String titleOf (const juce::File& source);

    /** Everything an export puts in the file, gathered on the message thread; write() makes the
        file from it, on any thread. */
    struct Package
    {
        juce::String projectXml, metadataXml;
        std::vector<std::pair<juce::File, juce::String>> files;         // on disk -> path inside
        std::vector<std::pair<juce::MemoryBlock, juce::String>> blocks;  // made here (plugin states) -> path inside

        juce::Result write (const juce::File& target) const;
    };

    /** The engine's song as a DAWproject. notes: what isn't in it. Message thread (plugins give
        their settings there). */
    Package exportProject (AudioEngine& engine, const juce::String& title, juce::StringArray& notes);

    /** Writes a package on a thread of its own, then calls done on the message thread. */
    class Writer final : private juce::Thread
    {
    public:
        Writer (Package package, const juce::File& target, std::function<void (juce::Result)> done);
        ~Writer() override;

    private:
        void run() override;

        Package package;
        juce::File target;
        std::function<void (juce::Result)> done;
    };

    /** A device's kind as people know it, from its DAWproject element ("Vst3Plugin": "VST3",
        "ClapPlugin": "CLAP"; another DAW's own devices: "built-in device"). */
    juce::String formatName (const juce::String& element);

    //==============================================================================
    // VST3 settings: Sonify keeps a VST3 plugin's as JUCE's host does (AudioPluginInstance::
    // getStateInformation); other hosts as a .vstpreset. Both hold the same two parts (the
    // plugin's component state and its controller state).

    /** A .vstpreset's settings as Sonify keeps them (empty when it isn't a .vstpreset). */
    juce::MemoryBlock presetToState (const juce::MemoryBlock& preset);

    /** The class id a .vstpreset names (32 hex digits; empty when it isn't one). */
    juce::String classIdOfPreset (const juce::MemoryBlock& preset);

    /** The id JUCE gives a VST3 class (PluginDescription::uniqueId) from its 32 hex digits:
        the end of a VST3 plugin's id in Sonify. False when it isn't a class id. */
    bool juceIdOfClass (const juce::String& classId, int& uniqueId);
}
