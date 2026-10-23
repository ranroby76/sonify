// D:\workspace\Fanan_web_hub\src\DawProject.cpp

#include "DawProject.h"
#include "Song.h"
#include "Vst3Plugin.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <map>

namespace
{
    constexpr double infinity = std::numeric_limits<double>::infinity();
    constexpr double tiny = 1.0e-6;

    juce::var object()
    {
        return juce::var (new juce::DynamicObject());
    }

    void put (juce::var& item, const juce::Identifier& key, const juce::var& value)
    {
        item.getDynamicObject()->setProperty (key, value);
    }

    /** A number as DAWproject writes them ("inf" and "-inf" too); fallback when there's none. */
    double numberIn (const juce::String& text, double fallback)
    {
        const auto trimmed = text.trim();

        if (trimmed.isEmpty())
            return fallback;

        if (trimmed.equalsIgnoreCase ("inf") || trimmed.equalsIgnoreCase ("+inf") || trimmed.equalsIgnoreCase ("infinity"))
            return infinity;

        if (trimmed.equalsIgnoreCase ("-inf") || trimmed.equalsIgnoreCase ("-infinity"))
            return -infinity;

        if (! trimmed.containsOnly ("0123456789+-.eE"))
            return fallback;

        const auto value = trimmed.getDoubleValue();
        return std::isnan (value) ? fallback : value;
    }

    double numberOf (const juce::XmlElement* element, const char* attribute, double fallback)
    {
        return element != nullptr && element->hasAttribute (attribute) ? numberIn (element->getStringAttribute (attribute), fallback) : fallback;
    }

    bool hasNumber (const juce::XmlElement& element, const char* attribute)
    {
        return element.hasAttribute (attribute) && std::isfinite (numberIn (element.getStringAttribute (attribute), infinity));
    }

    bool flagOf (const juce::XmlElement* element, const char* attribute, bool fallback)
    {
        if (element == nullptr || ! element->hasAttribute (attribute))
            return fallback;

        const auto text = element->getStringAttribute (attribute).trim();
        return text.equalsIgnoreCase ("true") || text == "1";
    }

    /** How DAWproject numbers are written here: plain, up to 9 decimals, no trailing zeros. */
    juce::String number (double value)
    {
        if (! std::isfinite (value))
            return value > 0 ? "inf" : "-inf";

        auto text = juce::String (value, 9);

        if (text.containsChar ('.'))
            text = text.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");

        return text == "-0" ? juce::String ("0") : text;
    }

    juce::String pathInside (juce::String path)
    {
        path = path.replaceCharacter ('\\', '/').trim();

        while (path.startsWith ("./"))
            path = path.substring (2);

        return path.trimCharactersAtStart ("/");
    }

    double dbOf (double gain)
    {
        return gain > 0.0 ? juce::jlimit (static_cast<double> (Track::silentDb), static_cast<double> (Track::maxDb), 20.0 * std::log10 (gain))
                          : static_cast<double> (Track::silentDb);
    }

    double gainOf (double db)
    {
        return db <= Track::silentDb ? 0.0 : std::pow (10.0, db / 20.0);
    }

    /** A volume parameter (or point) in dB, whatever unit it's in. */
    double volumeDb (double value, const juce::String& unit)
    {
        if (unit == "decibel")
            return std::isfinite (value) ? juce::jlimit (static_cast<double> (Track::silentDb), static_cast<double> (Track::maxDb), value)
                                         : static_cast<double> (value > 0 ? Track::maxDb : Track::silentDb);

        return dbOf (std::isfinite (value) ? value : 0.0);
    }

    /** A pan parameter (or point) as Sonify has it, -1..+1. */
    double panOf (double value, const juce::String& unit, double minimum)
    {
        if (! std::isfinite (value))
            return 0.0;

        if (unit == "percent")
            return juce::jlimit (-1.0, 1.0, minimum < 0.0 ? value / 100.0 : value / 50.0 - 1.0);

        return juce::jlimit (-1.0, 1.0, value * 2.0 - 1.0);
    }

    juce::String colourOf (const juce::XmlElement* element)
    {
        if (element == nullptr)
            return {};

        auto text = element->getStringAttribute ("color").trim().trimCharactersAtStart ("#");

        if (text.length() == 8 && text.containsOnly ("0123456789abcdefABCDEF"))
            text = text.substring (0, 6);   // #rrggbbaa

        return text.length() == 6 && text.containsOnly ("0123456789abcdefABCDEF") ? "ff" + text.toLowerCase() : juce::String();
    }

    juce::String htmlColour (juce::uint32 argb)
    {
        return "#" + juce::String::toHexString (static_cast<int> (argb & 0xffffffu)).paddedLeft ('0', 6);
    }

    //==============================================================================
    // .vstpreset: "VST3", a version, the class id (32 characters) and where the chunk list is;
    // the list: "List", how many, then each chunk's id, offset and size.
    struct PresetChunks
    {
        juce::String classId;
        juce::MemoryBlock component, controller;
        bool hasController = false;
    };

    bool readPreset (const juce::MemoryBlock& data, PresetChunks& chunks)
    {
        const auto* bytes = static_cast<const juce::uint8*> (data.getData());
        const auto size = static_cast<juce::uint64> (data.getSize());

        if (size < 48 || std::memcmp (bytes, "VST3", 4) != 0)
            return false;

        chunks.classId = juce::String::fromUTF8 (reinterpret_cast<const char*> (bytes + 8), 32);
        const auto listAt = juce::ByteOrder::littleEndianInt64 (bytes + 40);

        if (listAt > size - 8 || std::memcmp (bytes + listAt, "List", 4) != 0)
            return false;

        const auto count = static_cast<juce::uint64> (juce::ByteOrder::littleEndianInt (bytes + listAt + 4));
        bool found = false;

        for (juce::uint64 i = 0; i < count; ++i)
        {
            const auto at = listAt + 8 + i * 20;

            if (at > size - 20)
                return false;

            const auto offset = juce::ByteOrder::littleEndianInt64 (bytes + at + 4);
            const auto length = juce::ByteOrder::littleEndianInt64 (bytes + at + 12);

            if (offset > size || length > size - offset)
                return false;

            if (std::memcmp (bytes + at, "Comp", 4) == 0)
            {
                chunks.component.replaceAll (bytes + offset, static_cast<size_t> (length));
                found = true;
            }
            else if (std::memcmp (bytes + at, "Cont", 4) == 0)
            {
                chunks.controller.replaceAll (bytes + offset, static_cast<size_t> (length));
                chunks.hasController = true;
            }
        }

        return found;
    }

    juce::String cleanClassId (const juce::String& text)
    {
        return text.removeCharacters ("{}- ").trim().toUpperCase();
    }

    //==============================================================================
    // Import
    //==============================================================================
    /** A device Sonify can't play here, kept as it was (Track::kept). */
    struct KeptDevice
    {
        juce::String name, format, role, xml, stateFile;
        int place = 0;

        juce::var toVar() const
        {
            auto item = object();
            put (item, "name", name);
            put (item, "format", format);
            put (item, "role", role);
            put (item, "place", place);
            put (item, "xml", xml);

            if (stateFile.isNotEmpty())
                put (item, "stateFile", stateFile);

            return item;
        }
    };

    struct RawPoint
    {
        double ppq = 0.0, value = 0.0;
        bool hold = false;
    };

    /** Automation read from the arrangement, for the parameter it names. */
    struct RawLane
    {
        juce::String parameter, unit;
        std::vector<RawPoint> points;
    };

    struct SendIn
    {
        juce::String destination;
        double db = 0.0;
        bool on = true, pre = false;
    };

    /** A Sonify track to be (or Main). */
    struct Lane
    {
        const juce::XmlElement* trackXml = nullptr;
        const juce::XmlElement* channelXml = nullptr;
        juce::String kind;                  // instrument, audio, fx, bus, main ("": a regular track, decided by its content)
        juce::StringArray contentTypes;
        juce::String name, colour, trackId, channelId, destination;
        double volume = 0.0, pan = 0.0;
        bool mute = false, solo = false, mono = false;
        std::vector<SendIn> sends;

        juce::var instrument;
        juce::Array<juce::var> inserts, kept, midiClips, audioClips, automation;
        std::vector<RawLane> rawAutomation;

        // Its device parameters, by their ids in the file: the insert's place (-1: the instrument) and the parameter.
        std::map<juce::String, std::pair<int, juce::int64>> parameters;
        std::map<juce::String, std::pair<double, double>> parameterRanges;
        juce::String volumeId, panId;
    };

    /** What one of the arrangement's clips holds, gathered from all the clips inside it. */
    struct Gathered
    {
        double start = 0.0, end = 0.0;
        bool muted = false, hasNotes = false;
        std::vector<MidiNote> notes;                         // song positions
        std::map<juce::String, std::vector<std::pair<double, double>>> controllers;   // cc:N, bend, pressure: song positions, 0..1

        struct Piece
        {
            double start = 0.0, end = 0.0;                   // song positions
            juce::String file;
            double offset = 0.0, seconds = 0.0, fadeIn = 0.0, fadeOut = 0.0;
            bool muted = false;
        };

        std::vector<Piece> audio;
    };

    struct Fades
    {
        double in = 0.0, inAt = -infinity, out = 0.0, outAt = infinity;   // seconds; where they are (song positions)
    };

    /** A timeline's positions against the song's (quarter notes). */
    struct Place
    {
        std::function<double (double)> toSong, fromSong;
        bool seconds = false;   // its unit
    };

    class Importer
    {
    public:
        Importer (const juce::File& sourceFile, const juce::File& folder, const std::vector<DawProject::InstalledVst3>& plugins, juce::StringArray& notesOut)
            : source (sourceFile), songFolder (folder), installed (plugins), notes (notesOut), zip (sourceFile)
        {
        }

        juce::Result run (juce::File& songFile)
        {
            if (zip.getNumEntries() == 0)
                return juce::Result::fail ("It isn't a DAWproject file (it isn't a zip).");

            for (int i = 0; i < zip.getNumEntries(); ++i)
                if (const auto* entry = zip.getEntry (i))
                    entries[pathInside (entry->filename)] = i;

            juce::String text;

            if (! readText ("project.xml", text))
                return juce::Result::fail ("It isn't a DAWproject file: there's no project.xml in it.");

            root = juce::XmlDocument::parse (text);

            if (root == nullptr || ! root->hasTagName ("Project"))
                return juce::Result::fail ("Its project.xml can't be read.");

            collectIds (*root);
            readTempo();
            readStructure();

            if (const auto* arrangement = root->getChildByName ("Arrangement"))
            {
                if (const auto* timeline = arrangement->getChildByName ("Lanes"))
                    walk (*timeline, false, nullptr);

                if (arrangement->getChildByName ("Markers") != nullptr)
                    note ("Markers aren't in the song.");
            }

            if (const auto* scenes = root->getChildByName ("Scenes"); scenes != nullptr && scenes->getNumChildElements() > 0)
                note ("The clip launcher's scenes aren't in the song (only the arrangement is).");

            decideKinds();

            for (auto& lane : lanes)
                readDevices (lane);

            readDevices (main);

            for (auto& lane : lanes)
                resolveAutomation (lane);

            resolveAutomation (main);
            splitMixedTracks();

            const auto song = buildSong();
            songFile = songFolder.getChildFile (songFolder.getFileName() + Song::fileExtension);

            if (! songFile.replaceWithText (juce::JSON::toString (song)))
                return juce::Result::fail ("The song couldn't be written to " + songFile.getFullPathName() + ".");

            return juce::Result::ok();
        }

    private:
        const juce::File source, songFolder;
        const std::vector<DawProject::InstalledVst3>& installed;
        juce::StringArray& notes;
        juce::ZipFile zip;
        std::map<juce::String, int> entries;
        std::unique_ptr<juce::XmlElement> root;
        std::map<juce::String, const juce::XmlElement*> ids;
        TempoMap map;
        juce::String tempoId, signatureId;
        std::vector<Lane> lanes;
        Lane main;
        std::map<juce::String, juce::String> extracted;   // path inside -> file here
        juce::StringArray sendParameters;                 // the sends' levels, by their ids

        void note (const juce::String& text)
        {
            notes.addIfNotAlreadyThere (text);
        }

        //==============================================================================
        // The container
        int entryIndex (const juce::String& path) const
        {
            const auto wanted = pathInside (path);

            if (const auto found = entries.find (wanted); found != entries.end())
                return found->second;

            for (const auto& [name, index] : entries)
                if (name.equalsIgnoreCase (wanted))
                    return index;

            return -1;
        }

        bool readBlock (const juce::String& path, juce::MemoryBlock& block)
        {
            const auto index = entryIndex (path);

            if (index < 0)
                return false;

            std::unique_ptr<juce::InputStream> stream (zip.createStreamForEntry (index));

            if (stream == nullptr)
                return false;

            block.reset();
            stream->readIntoMemoryBlock (block);
            return true;
        }

        bool readText (const juce::String& path, juce::String& text)
        {
            juce::MemoryBlock block;

            if (! readBlock (path, block))
                return false;

            text = block.toString();
            return true;
        }

        /** A file inside the container, written into a folder of the song (once). */
        juce::File extract (const juce::String& path, const juce::String& folderName)
        {
            const auto key = pathInside (path);

            if (const auto found = extracted.find (key); found != extracted.end())
                return juce::File (found->second);

            const auto index = entryIndex (key);

            if (index < 0)
                return {};

            std::unique_ptr<juce::InputStream> stream (zip.createStreamForEntry (index));
            const auto folder = songFolder.getChildFile (folderName);

            if (stream == nullptr || ! folder.createDirectory())
                return {};

            auto name = juce::File::createLegalFileName (key.fromLastOccurrenceOf ("/", false, false));

            if (name.isEmpty())
                name = "File";

            auto target = folder.getChildFile (name);

            if (target.exists())
                target = target.getNonexistentSibling (true);

            {
                juce::FileOutputStream out (target);

                if (! out.openedOk() || out.writeFromInputStream (*stream, -1) < 0)
                    return {};
            }

            extracted[key] = target.getFullPathName();
            return target;
        }

        /** A FileReference's file here: inside the container (written into the song's folder), or
            beside the .dawproject (or anywhere, by an absolute path). */
        juce::File fileOf (const juce::XmlElement* reference, const juce::String& folderName)
        {
            if (reference == nullptr)
                return {};

            const auto path = reference->getStringAttribute ("path").trim();

            if (path.isEmpty())
                return {};

            if (! flagOf (reference, "external", false))
                return extract (path, folderName);

            const auto absolute = path.startsWithChar ('/') || path.startsWithChar ('\\')
                               || (path.length() > 2 && path[1] == ':' && juce::CharacterFunctions::isLetter (path[0]));
            return absolute ? juce::File (path) : source.getParentDirectory().getChildFile (path.replaceCharacter ('\\', '/'));
        }

        /** A FileReference's bytes (from inside the container, or from the file it names). */
        bool readReference (const juce::XmlElement& reference, juce::MemoryBlock& block)
        {
            if (! flagOf (&reference, "external", false))
                return readBlock (reference.getStringAttribute ("path"), block);

            const auto file = fileOf (&reference, {});
            return file.existsAsFile() && file.loadFileAsData (block);
        }

        //==============================================================================
        void collectIds (const juce::XmlElement& element)
        {
            if (const auto id = element.getStringAttribute ("id"); id.isNotEmpty())
                ids[id] = &element;

            for (const auto* child : element.getChildIterator())
                collectIds (*child);
        }

        const juce::XmlElement* byId (const juce::String& id) const
        {
            const auto found = ids.find (id);
            return found != ids.end() ? found->second : nullptr;
        }

        //==============================================================================
        // Tempo and time signature, with their changes
        void readTempo()
        {
            const auto* transport = root->getChildByName ("Transport");
            const auto* tempo = transport != nullptr ? transport->getChildByName ("Tempo") : nullptr;
            const auto* signature = transport != nullptr ? transport->getChildByName ("TimeSignature") : nullptr;
            auto bpm = numberOf (tempo, "value", 120.0);
            tempoId = tempo != nullptr ? tempo->getStringAttribute ("id") : juce::String();
            signatureId = signature != nullptr ? signature->getStringAttribute ("id") : juce::String();

            const auto clampTempo = [this] (double value)
            {
                if (! std::isfinite (value))
                    value = 120.0;

                if (value < TempoMap::minTempo || value > TempoMap::maxTempo)
                    note ("A tempo outside Sonify's (" + juce::String (TempoMap::minTempo, 0) + " to " + juce::String (TempoMap::maxTempo, 0)
                          + " BPM) was brought into it.");

                return juce::jlimit (TempoMap::minTempo, TempoMap::maxTempo, value);
            };

            std::vector<TempoMap::Tempo> tempos;
            const auto* arrangement = root->getChildByName ("Arrangement");

            if (const auto* automation = arrangement != nullptr ? arrangement->getChildByName ("TempoAutomation") : nullptr)
            {
                const auto seconds = automation->getStringAttribute ("timeUnit") == "seconds";
                std::vector<std::tuple<double, double, bool>> points;

                for (const auto* point : automation->getChildWithTagNameIterator ("RealPoint"))
                    if (hasNumber (*point, "time"))
                        points.emplace_back (numberOf (point, "time", 0.0), numberOf (point, "value", bpm),
                                             point->getStringAttribute ("interpolation") == "linear");

                std::stable_sort (points.begin(), points.end(), [] (const auto& a, const auto& b) { return std::get<0> (a) < std::get<0> (b); });

                if (seconds && ! points.empty())
                    note ("The tempo changes were set in seconds: their glides became steps.");

                TempoMap built;

                for (const auto& [time, value, linear] : points)
                {
                    auto ppq = time;

                    if (seconds)
                    {
                        built.setTempos (tempos.empty() ? std::vector<TempoMap::Tempo> { { 0.0, clampTempo (value), false } } : tempos);
                        ppq = built.ppqAt (time);
                    }

                    ppq = juce::jmax (0.0, ppq);

                    if (! tempos.empty() && std::abs (tempos.back().ppq - ppq) < tiny)
                        tempos.back() = { tempos.back().ppq, clampTempo (value), linear && ! seconds };
                    else
                        tempos.push_back ({ ppq, clampTempo (value), linear && ! seconds });
                }
            }

            if (tempos.empty() || tempos.front().ppq > tiny)
                tempos.insert (tempos.begin(), { 0.0, clampTempo (bpm), false });

            tempos.front().ppq = 0.0;
            tempos.back().ramp = false;
            map.setTempos (tempos);

            // Signatures by bar: each change at the bar it falls in (or the next one).
            auto numerator = juce::jlimit (1, 32, static_cast<int> (numberOf (signature, "numerator", 4.0)));
            auto denominator = static_cast<int> (numberOf (signature, "denominator", 4.0));

            if (! TempoMap::isValidDenominator (denominator))
                denominator = 4;

            std::vector<TempoMap::Signature> signatures { { 0, numerator, denominator } };
            map.setSignatures (signatures);

            if (const auto* automation = arrangement != nullptr ? arrangement->getChildByName ("TimeSignatureAutomation") : nullptr)
            {
                std::vector<std::tuple<double, int, int>> points;
                const auto seconds = automation->getStringAttribute ("timeUnit") == "seconds";

                for (const auto* point : automation->getChildWithTagNameIterator ("TimeSignaturePoint"))
                    if (hasNumber (*point, "time"))
                        points.emplace_back (seconds ? map.ppqAt (numberOf (point, "time", 0.0)) : numberOf (point, "time", 0.0),
                                             static_cast<int> (numberOf (point, "numerator", 4.0)), static_cast<int> (numberOf (point, "denominator", 4.0)));

                std::stable_sort (points.begin(), points.end(), [] (const auto& a, const auto& b) { return std::get<0> (a) < std::get<0> (b); });

                for (const auto& [ppq, top, bottom] : points)
                {
                    if (top < 1 || top > 32 || ! TempoMap::isValidDenominator (bottom))
                    {
                        note ("A time signature Sonify doesn't have (" + juce::String (top) + "/" + juce::String (bottom) + ") was left out.");
                        continue;
                    }

                    const auto bar = map.barAt (juce::jmax (0.0, ppq));
                    const auto onBar = std::abs (ppq - bar.start) < 1.0e-4;
                    const auto index = bar.index + (onBar ? 0 : 1);

                    if (! onBar)
                        note ("A time signature change that wasn't at the start of a bar moved to the next bar.");

                    if (index == signatures.back().bar)
                        signatures.back() = { index, top, bottom };
                    else if (index > signatures.back().bar)
                        signatures.push_back ({ index, top, bottom });

                    map.setSignatures (signatures);
                }
            }
        }

        //==============================================================================
        // Tracks and channels
        void readStructure()
        {
            if (const auto* structure = root->getChildByName ("Structure"))
                for (const auto* child : structure->getChildIterator())
                    readStructureItem (*child, {});

            if (main.channelXml == nullptr)
                main.kind = "main";
        }

        /** group: the channel of the group track it's in (its tracks play into it unless they say otherwise). */
        void readStructureItem (const juce::XmlElement& item, const juce::String& group)
        {
            if (item.hasTagName ("Track"))
            {
                const auto* channel = item.getChildByName ("Channel");
                const auto types = juce::StringArray::fromTokens (item.getStringAttribute ("contentType"), " ,", "");

                if (channel != nullptr)
                    addChannel (&item, *channel, types, group);
                else if (! types.contains ("tracks") && item.getChildByName ("Track") == nullptr)
                    addChannel (&item, item, types, group);   // a track without a channel of its own

                const auto inner = channel != nullptr ? channel->getStringAttribute ("id") : group;

                for (const auto* child : item.getChildWithTagNameIterator ("Track"))
                    readStructureItem (*child, inner);
            }
            else if (item.hasTagName ("Channel"))
            {
                addChannel (nullptr, item, {}, group);
            }
        }

        void addChannel (const juce::XmlElement* track, const juce::XmlElement& channel, const juce::StringArray& types, const juce::String& group)
        {
            const auto role = channel.hasTagName ("Channel") ? channel.getStringAttribute ("role", "regular") : juce::String ("regular");
            const auto name = (track != nullptr ? track->getStringAttribute ("name") : juce::String()).trim();
            const auto channelName = channel.hasTagName ("Channel") ? channel.getStringAttribute ("name").trim() : juce::String();
            const auto shownName = name.isNotEmpty() ? name : channelName;

            if (role == "vca")
            {
                note ("VCA channels aren't in the song (" + (shownName.isNotEmpty() ? shownName : juce::String ("VCA")) + ").");
                return;
            }

            Lane lane;
            lane.trackXml = track;
            lane.channelXml = channel.hasTagName ("Channel") ? &channel : nullptr;
            lane.trackId = track != nullptr ? track->getStringAttribute ("id") : juce::String();
            lane.channelId = lane.channelXml != nullptr ? channel.getStringAttribute ("id") : juce::String();
            lane.name = shownName;
            lane.colour = colourOf (track);
            lane.contentTypes = types;

            if (lane.colour.isEmpty())
                lane.colour = colourOf (lane.channelXml);

            lane.destination = group;

            if (const auto* channelXml = lane.channelXml)
            {
                lane.destination = channelXml->getStringAttribute ("destination", group);
                lane.solo = flagOf (channelXml, "solo", false);
                lane.mono = static_cast<int> (numberOf (channelXml, "audioChannels", 2.0)) == 1;

                if (const auto* volume = channelXml->getChildByName ("Volume"))
                {
                    lane.volume = volumeDb (numberOf (volume, "value", 1.0), volume->getStringAttribute ("unit", "linear"));
                    lane.volumeId = volume->getStringAttribute ("id");
                }

                if (const auto* pan = channelXml->getChildByName ("Pan"))
                {
                    lane.pan = panOf (numberOf (pan, "value", 0.5), pan->getStringAttribute ("unit", "normalized"), numberOf (pan, "min", 0.0));
                    lane.panId = pan->getStringAttribute ("id");
                }

                if (const auto* mute = channelXml->getChildByName ("Mute"))
                    lane.mute = flagOf (mute, "value", false);

                if (const auto* sends = channelXml->getChildByName ("Sends"))
                {
                    for (const auto* send : sends->getChildWithTagNameIterator ("Send"))
                    {
                        SendIn in;
                        in.destination = send->getStringAttribute ("destination");
                        in.pre = send->getStringAttribute ("type") == "pre";

                        if (const auto* volume = send->getChildByName ("Volume"))
                        {
                            in.db = volumeDb (numberOf (volume, "value", 1.0), volume->getStringAttribute ("unit", "linear"));
                            sendParameters.add (volume->getStringAttribute ("id"));
                        }

                        if (const auto* enable = send->getChildByName ("Enable"))
                            in.on = flagOf (enable, "value", true);

                        if (in.destination.isNotEmpty())
                            lane.sends.push_back (in);
                    }
                }
            }

            if (role == "master")
            {
                if (main.channelXml == nullptr && main.trackXml == nullptr)
                {
                    lane.kind = "main";
                    main = std::move (lane);
                    return;
                }

                note ("A second master channel became a bus.");
                lane.kind = "bus";
            }
            else if (role == "effect")
            {
                lane.kind = "fx";
            }
            else if (role == "submix" || types.contains ("tracks"))
            {
                lane.kind = "bus";
            }

            if (lane.name.isEmpty())
                lane.name = lane.kind == "fx" ? "FX" : lane.kind == "bus" ? "Bus" : "Track " + juce::String (static_cast<int> (lanes.size()) + 1);

            lanes.push_back (std::move (lane));
        }

        Lane* laneFor (const juce::String& id)
        {
            if (id.isEmpty())
                return nullptr;

            if (main.trackId == id || main.channelId == id)
                return &main;

            for (auto& lane : lanes)
                if (lane.trackId == id || lane.channelId == id)
                    return &lane;

            return nullptr;
        }

        int indexOf (const juce::String& id) const
        {
            for (size_t i = 0; i < lanes.size(); ++i)
                if (id.isNotEmpty() && (lanes[i].trackId == id || lanes[i].channelId == id))
                    return static_cast<int> (i);

            return -1;
        }

        //==============================================================================
        // The arrangement
        void walk (const juce::XmlElement& element, bool seconds, Lane* lane)
        {
            if (const auto unit = element.getStringAttribute ("timeUnit"); unit == "seconds")
                seconds = true;
            else if (unit == "beats")
                seconds = false;

            if (const auto track = element.getStringAttribute ("track"); track.isNotEmpty())
            {
                lane = laneFor (track);

                if (lane == nullptr)
                    note ("Some of the arrangement belongs to a track that isn't in the song: it was left out.");
            }

            Place place;
            place.seconds = seconds;
            place.toSong = seconds ? std::function<double (double)> ([this] (double x) { return map.ppqAt (x); })
                                   : std::function<double (double)> ([] (double x) { return x; });
            place.fromSong = seconds ? std::function<double (double)> ([this] (double p) { return map.secondsAt (p); })
                                     : std::function<double (double)> ([] (double p) { return p; });

            if (element.hasTagName ("Lanes"))
            {
                for (const auto* child : element.getChildIterator())
                    walk (*child, seconds, lane);
            }
            else if (element.hasTagName ("Clips"))
            {
                if (lane != nullptr)
                    for (const auto* clip : element.getChildWithTagNameIterator ("Clip"))
                        topClip (*clip, place, *lane);
            }
            else if (element.hasTagName ("Points"))
            {
                if (lane != nullptr)
                    trackPoints (element, place, *lane);
            }
            else if (element.hasTagName ("Notes") || element.hasTagName ("Audio") || element.hasTagName ("Warps"))
            {
                // Content right on the track, outside any clip: one clip from the start of the song.
                if (lane != nullptr)
                {
                    Gathered gathered;
                    gathered.start = 0.0;
                    gathered.end = juce::jmax (0.0, place.toSong (extentOf (element, seconds)));
                    content (element, place, gathered.start, gathered.end, gathered, {}, 0);
                    keep (gathered, *lane);
                }
            }
            else if (element.hasTagName ("Markers") || element.hasTagName ("markers"))
            {
                note ("Markers aren't in the song.");
            }
            else if (element.hasTagName ("Video"))
            {
                note ("Video in the project isn't in the song.");
            }
        }

        /** How far a timeline's content goes, in its own unit (for clips that don't say). */
        double extentOf (const juce::XmlElement& element, bool seconds) const
        {
            double end = 0.0;

            if (element.hasTagName ("Notes"))
            {
                for (const auto* n : element.getChildWithTagNameIterator ("Note"))
                    end = juce::jmax (end, numberOf (n, "time", 0.0) + numberOf (n, "duration", 0.0));
            }
            else if (element.hasTagName ("Audio"))
            {
                end = numberOf (&element, "duration", 0.0);

                if (! seconds)
                    end = end * map.bpmAt (0.0) / 60.0;
            }
            else if (element.hasTagName ("Warps"))
            {
                for (const auto* w : element.getChildWithTagNameIterator ("Warp"))
                    end = juce::jmax (end, numberOf (w, "time", 0.0));
            }
            else if (element.hasTagName ("Clips"))
            {
                for (const auto* c : element.getChildWithTagNameIterator ("Clip"))
                    end = juce::jmax (end, numberOf (c, "time", 0.0) + numberOf (c, "duration", 0.0));
            }
            else if (element.hasTagName ("Lanes"))
            {
                for (const auto* child : element.getChildIterator())
                    end = juce::jmax (end, extentOf (*child, seconds));
            }

            return std::isfinite (end) ? end : 0.0;
        }

        const juce::XmlElement* contentOf (const juce::XmlElement& clip) const
        {
            if (const auto reference = clip.getStringAttribute ("reference"); reference.isNotEmpty())
                return byId (reference);

            for (const auto* child : clip.getChildIterator())
                if (child->hasTagName ("Notes") || child->hasTagName ("Audio") || child->hasTagName ("Warps") || child->hasTagName ("Clips")
                    || child->hasTagName ("Lanes") || child->hasTagName ("Points") || child->hasTagName ("Timeline")
                    || child->hasTagName ("Video") || child->hasTagName ("Markers") || child->hasTagName ("markers") || child->hasTagName ("ClipSlot"))
                    return child;

            return nullptr;
        }

        void topClip (const juce::XmlElement& clip, const Place& place, Lane& lane)
        {
            Gathered gathered;
            double start = 0.0, end = 0.0;

            if (! clipInto (clip, place, -infinity, infinity, gathered, {}, 0, &start, &end))
                return;

            gathered.start = start;
            gathered.end = end;
            keep (gathered, lane);
        }

        /** A clip on a timeline (place), seen through the window from..to (song positions). Its
            notes, controllers and audio go into gathered. depth 0: one on the arrangement. */
        bool clipInto (const juce::XmlElement& clip, const Place& place, double from, double to, Gathered& gathered,
                       Fades fades, int depth, double* shownStart = nullptr, double* shownEnd = nullptr)
        {
            if (depth > 16)
                return false;

            if (! flagOf (&clip, "enable", true))
            {
                if (depth > 0)
                    return false;   // switched off inside another clip: not heard

                gathered.muted = true;
            }

            const auto* inside = contentOf (clip);

            if (inside == nullptr)
                return false;

            const auto contentUnit = clip.getStringAttribute ("contentTimeUnit");
            const auto contentSeconds = contentUnit == "seconds" ? true : contentUnit == "beats" ? false : place.seconds;
            const auto time = numberOf (&clip, "time", 0.0);
            const auto playStart = numberOf (&clip, "playStart", 0.0);
            const auto start = place.toSong (time);

            // The content's positions against the song, from playStart at the clip's start.
            const auto mapping = [this, contentSeconds] (double contentFrom, double songFrom)
            {
                Place inner;
                inner.seconds = contentSeconds;

                if (contentSeconds)
                {
                    const auto secondsFrom = map.secondsAt (songFrom);
                    inner.toSong = [this, contentFrom, secondsFrom] (double x) { return map.ppqAt (secondsFrom + (x - contentFrom)); };
                    inner.fromSong = [this, contentFrom, secondsFrom] (double p) { return contentFrom + (map.secondsAt (p) - secondsFrom); };
                }
                else
                {
                    inner.toSong = [contentFrom, songFrom] (double x) { return songFrom + (x - contentFrom); };
                    inner.fromSong = [contentFrom, songFrom] (double p) { return contentFrom + (p - songFrom); };
                }

                return inner;
            };

            double end = 0.0;

            if (hasNumber (clip, "duration"))
                end = place.toSong (time + numberOf (&clip, "duration", 0.0));
            else if (hasNumber (clip, "playStop"))
                end = mapping (playStart, start).toSong (numberOf (&clip, "playStop", 0.0));
            else
                end = mapping (playStart, start).toSong (extentOf (*inside, contentSeconds));

            const auto windowFrom = juce::jmax (from, start);
            const auto windowTo = juce::jmin (to, end);

            if (windowTo - windowFrom < tiny)
                return false;

            if (shownStart != nullptr)   *shownStart = start;
            if (shownEnd != nullptr)     *shownEnd = end;

            // Its own fades (else the ones around it).
            if (hasNumber (clip, "fadeInTime") || hasNumber (clip, "fadeOutTime"))
            {
                const auto fadeUnit = clip.getStringAttribute ("fadeTimeUnit");
                const auto fadeSeconds = fadeUnit == "seconds" ? true : fadeUnit == "beats" ? false : contentSeconds;
                const auto fadeIn = std::abs (numberOf (&clip, "fadeInTime", 0.0));
                const auto fadeOut = std::abs (numberOf (&clip, "fadeOutTime", 0.0));
                fades = {};
                fades.inAt = start;
                fades.outAt = end;
                fades.in = fadeSeconds ? fadeIn : map.secondsAt (start + fadeIn) - map.secondsAt (start);
                fades.out = fadeSeconds ? fadeOut : map.secondsAt (end) - map.secondsAt (juce::jmax (0.0, end - fadeOut));
            }

            // Looped: from playStart to loopEnd, then loopStart to loopEnd over and over.
            const auto loopStart = numberOf (&clip, "loopStart", 0.0);
            const auto loopEnd = numberOf (&clip, "loopEnd", 0.0);
            const auto looped = hasNumber (clip, "loopStart") && hasNumber (clip, "loopEnd") && loopEnd - loopStart > tiny && playStart < loopEnd;

            if (! looped)
            {
                content (*inside, mapping (playStart, start), windowFrom, windowTo, gathered, fades, depth);
                return true;
            }

            if (inside->hasTagName ("Audio") || inside->hasTagName ("Warps"))
                note ("Looped audio clips play their loop once.");

            auto contentFrom = playStart;
            auto songFrom = start;

            for (int round = 0; round < 4096 && songFrom < windowTo - tiny; ++round)
            {
                const auto inner = mapping (contentFrom, songFrom);
                const auto songTo = inner.toSong (loopEnd);

                if (songTo - songFrom < tiny)
                    break;

                content (*inside, inner, juce::jmax (windowFrom, songFrom), juce::jmin (windowTo, songTo), gathered, fades, depth);

                if (inside->hasTagName ("Audio") || inside->hasTagName ("Warps"))
                    break;

                contentFrom = loopStart;
                songFrom = songTo;
            }

            return true;
        }

        /** A timeline inside a clip, seen through from..to (song positions). */
        void content (const juce::XmlElement& element, const Place& place, double from, double to, Gathered& gathered, const Fades& fades, int depth)
        {
            if (to - from < tiny)
                return;

            if (element.hasTagName ("Notes"))
            {
                gathered.hasNotes = true;

                for (const auto* n : element.getChildWithTagNameIterator ("Note"))
                {
                    const auto key = static_cast<int> (numberOf (n, "key", -1.0));
                    const auto time = numberOf (n, "time", 0.0);
                    const auto noteStart = place.toSong (time);

                    if (key < 0 || key > 127 || noteStart < from - tiny || noteStart >= to - tiny)
                        continue;

                    const auto noteEnd = juce::jmin (to, place.toSong (time + juce::jmax (0.0, numberOf (n, "duration", 0.25))));
                    MidiNote note;
                    note.pitch = key;
                    note.start = noteStart;
                    note.length = juce::jmax (1.0e-3, noteEnd - noteStart);
                    note.velocity = static_cast<float> (juce::jlimit (1.0 / 127.0, 1.0, numberOf (n, "vel", 0.8)));
                    gathered.notes.push_back (note);
                }
            }
            else if (element.hasTagName ("Lanes"))
            {
                for (const auto* child : element.getChildIterator())
                    content (*child, place, from, to, gathered, fades, depth);
            }
            else if (element.hasTagName ("Clips"))
            {
                for (const auto* clip : element.getChildWithTagNameIterator ("Clip"))
                    clipInto (*clip, place, from, to, gathered, fades, depth + 1);
            }
            else if (element.hasTagName ("Points"))
            {
                clipPoints (element, place, from, to, gathered);
            }
            else if (element.hasTagName ("Audio"))
            {
                audioPiece (element, place, from, to, gathered, fades, place.fromSong (from), place.seconds);
            }
            else if (element.hasTagName ("Warps"))
            {
                warpedPiece (element, place, from, to, gathered, fades);
            }
            else if (element.hasTagName ("Video"))
            {
                note ("Video in the project isn't in the song.");
            }
        }

        void audioPiece (const juce::XmlElement& audio, const Place& place, double from, double to, Gathered& gathered, const Fades& fades,
                         double offset, bool offsetInSeconds)
        {
            juce::ignoreUnused (place);
            const auto file = fileOf (audio.getChildByName ("File"), "Media");

            if (file == juce::File())
            {
                note ("An audio clip's file isn't in the DAWproject: the clip was left out.");
                return;
            }

            if (! file.existsAsFile())
                note ("An audio file is missing: " + file.getFullPathName());

            if (! offsetInSeconds)
                offset = offset * 60.0 / map.bpmAt (from);   // raw audio placed in beats: at the song's tempo

            Gathered::Piece piece;
            piece.start = from;
            piece.end = to;
            piece.file = file.getFullPathName();
            piece.offset = offset;
            piece.seconds = map.secondsAt (to) - map.secondsAt (from);

            // Starting before the file does: the clip starts where the file does.
            if (piece.offset < 0.0)
            {
                piece.start = map.ppqAt (map.secondsAt (from) - piece.offset);
                piece.seconds += piece.offset;
                piece.offset = 0.0;
            }

            if (piece.seconds <= 1.0e-4 || piece.end - piece.start < tiny)
                return;

            if (std::abs (piece.start - fades.inAt) < 1.0e-4)
                piece.fadeIn = fades.in;

            if (std::abs (piece.end - fades.outAt) < 1.0e-4)
                piece.fadeOut = fades.out;

            gathered.audio.push_back (piece);
        }

        void warpedPiece (const juce::XmlElement& warps, const Place& place, double from, double to, Gathered& gathered, const Fades& fades)
        {
            const auto* audio = warps.getChildByName ("Audio");

            if (audio == nullptr)
                return;

            if (warps.getStringAttribute ("contentTimeUnit", "seconds") != "seconds")
            {
                note ("An audio clip warped in beats was left out.");
                return;
            }

            std::vector<std::pair<double, double>> points;

            for (const auto* w : warps.getChildWithTagNameIterator ("Warp"))
                if (hasNumber (*w, "time") && hasNumber (*w, "contentTime"))
                    points.emplace_back (numberOf (w, "time", 0.0), numberOf (w, "contentTime", 0.0));

            std::stable_sort (points.begin(), points.end());

            if (points.size() < 2)
            {
                audioPiece (*audio, place, from, to, gathered, fades, points.empty() ? place.fromSong (from) : points.front().second, true);
                return;
            }

            // The file's seconds at a position on the warps' timeline (straight on past the ends).
            const auto fileAt = [&points] (double x)
            {
                size_t i = 1;

                while (i < points.size() - 1 && x > points[i].first)
                    ++i;

                const auto& [x0, y0] = points[i - 1];
                const auto& [x1, y1] = points[i];
                return std::abs (x1 - x0) < tiny ? y0 : y0 + (x - x0) * (y1 - y0) / (x1 - x0);
            };

            const auto fileFrom = fileAt (place.fromSong (from));
            const auto fileTo = fileAt (place.fromSong (to));
            const auto heard = map.secondsAt (to) - map.secondsAt (from);

            if (fileTo < fileFrom)
                note ("A reversed audio clip plays forwards.");
            else if (std::abs ((fileTo - fileFrom) - heard) > 0.01 * heard + 0.002)
                note ("Some audio clips were time-stretched there: here they play at their own speed (Audio > Sync Tempo can stretch them).");

            audioPiece (*audio, place, from, to, gathered, fades, juce::jmin (fileFrom, fileTo), true);
        }

        /** MIDI controllers inside a clip: CC, pitch bend and channel pressure. */
        void clipPoints (const juce::XmlElement& points, const Place& place, double from, double to, Gathered& gathered)
        {
            const auto* target = points.getChildByName ("Target");
            const auto expression = target != nullptr ? target->getStringAttribute ("expression") : juce::String();
            juce::String key;

            if (expression == "channelController")
            {
                const auto controller = static_cast<int> (numberOf (target, "controller", -1.0));

                if (controller >= 0 && controller < 128)
                    key = "cc:" + juce::String (controller);
            }
            else if (expression == "pitchBend")
            {
                key = "bend";
            }
            else if (expression == "channelPressure")
            {
                key = "pressure";
            }

            if (key.isEmpty())
            {
                note ("Automation inside clips (other than MIDI controllers, pitch bend and channel pressure) was left out.");
                return;
            }

            const auto unit = points.getStringAttribute ("unit", "normalized");
            auto& line = gathered.controllers[key];

            for (const auto* point : points.getChildWithTagNameIterator ("RealPoint"))
            {
                const auto at = place.toSong (numberOf (point, "time", 0.0));

                if (at < from - tiny || at > to + tiny)
                    continue;

                auto value = numberOf (point, "value", 0.0);

                if (unit != "normalized" && key.startsWith ("cc:") && value > 1.0)
                    value /= 127.0;

                line.emplace_back (at, juce::jlimit (0.0, 1.0, value));
            }

            gathered.hasNotes = true;
        }

        void trackPoints (const juce::XmlElement& points, const Place& place, Lane& lane)
        {
            const auto* target = points.getChildByName ("Target");

            if (target == nullptr)
                return;

            const auto parameter = target->getStringAttribute ("parameter");

            if (parameter.isEmpty())
            {
                note ("MIDI controller automation on tracks (outside clips) was left out.");
                return;
            }

            RawLane raw;
            raw.parameter = parameter;
            raw.unit = points.getStringAttribute ("unit");

            for (const auto* point : points.getChildIterator())
            {
                if (! point->hasTagName ("RealPoint"))
                {
                    if (point->hasTagName ("BoolPoint") || point->hasTagName ("IntegerPoint") || point->hasTagName ("EnumPoint"))
                        note ("Automation of switches and lists (mute, for one) was left out.");

                    continue;
                }

                if (hasNumber (*point, "time"))
                    raw.points.push_back ({ place.toSong (numberOf (point, "time", 0.0)), numberOf (point, "value", 0.0),
                                            point->getStringAttribute ("interpolation") == "hold" });
            }

            if (! raw.points.empty())
                lane.rawAutomation.push_back (std::move (raw));
        }

        /** A clip from the arrangement: a MIDI clip (when it had notes), and its audio clips. */
        void keep (const Gathered& gathered, Lane& lane)
        {
            const auto start = juce::jmax (0.0, gathered.start);
            const auto length = gathered.end - start;

            if (gathered.hasNotes && length > tiny)
            {
                juce::Array<juce::var> noteList;
                auto sorted = gathered.notes;
                std::stable_sort (sorted.begin(), sorted.end(), [] (const MidiNote& a, const MidiNote& b) { return a.start < b.start; });

                for (auto n : sorted)
                {
                    n.start -= start;

                    if (n.start >= -tiny)
                    {
                        n.start = juce::jmax (0.0, n.start);
                        noteList.add (n.toVar());
                    }
                }

                auto clip = object();
                put (clip, "start", start);
                put (clip, "length", length);
                put (clip, "offset", 0.0);
                put (clip, "notes", noteList);

                if (gathered.muted)
                    put (clip, "muted", true);

                juce::Array<juce::var> envelopes;

                for (const auto& [key, points] : gathered.controllers)
                {
                    auto sortedPoints = points;
                    std::stable_sort (sortedPoints.begin(), sortedPoints.end());
                    juce::Array<juce::var> list;

                    for (const auto& [at, value] : sortedPoints)
                    {
                        const auto time = at - start;

                        if (key == "bend")
                            list.add (juce::Array<juce::var> { time, juce::jlimit (MidiClip::pitchBendMin, MidiClip::pitchBendMax,
                                                                                  juce::roundToInt ((value - 0.5) * 16384.0)) });
                        else if (key == "pressure")
                            list.add (juce::Array<juce::var> { time, juce::roundToInt (value * 127.0) });
                        else
                            list.add (juce::Array<juce::var> { time, value * 127.0 });
                    }

                    if (key == "bend")
                    {
                        put (clip, "pitchBend", list);
                    }
                    else if (key == "pressure")
                    {
                        put (clip, "aftertouch", list);
                    }
                    else
                    {
                        auto line = object();
                        put (line, "key", key);
                        put (line, "points", list);
                        envelopes.add (line);
                    }
                }

                if (! envelopes.isEmpty())
                    put (clip, "envelopes", envelopes);

                lane.midiClips.add (clip);
            }

            for (const auto& piece : gathered.audio)
            {
                auto clip = object();
                put (clip, "start", juce::jmax (0.0, piece.start));
                put (clip, "length", piece.end - piece.start);
                put (clip, "offset", 0.0);
                put (clip, "notes", juce::Array<juce::var>());
                put (clip, "audioFile", piece.file);
                put (clip, "audioOffset", piece.offset);
                put (clip, "audioLength", piece.seconds);
                put (clip, "gain", 0.0);
                put (clip, "fadeIn", juce::jmin (piece.fadeIn, piece.seconds));
                put (clip, "fadeOut", juce::jmin (piece.fadeOut, piece.seconds));

                if (gathered.muted || piece.muted)
                    put (clip, "muted", true);

                lane.audioClips.add (clip);
            }
        }

        //==============================================================================
        /** A regular track's kind: by its content type, else by what it plays. */
        void decideKinds()
        {
            for (auto& lane : lanes)
            {
                if (lane.kind.isNotEmpty())
                {
                    if (! lane.midiClips.isEmpty() || ! lane.audioClips.isEmpty())
                        note ("Clips on FX channels and group tracks were left out.");

                    lane.midiClips.clear();
                    lane.audioClips.clear();
                    continue;
                }

                const auto notesType = lane.contentTypes.contains ("notes");
                const auto audioType = lane.contentTypes.contains ("audio");

                if (notesType && ! audioType)
                    lane.kind = "instrument";
                else if (audioType && ! notesType)
                    lane.kind = "audio";
                else
                    lane.kind = (! lane.audioClips.isEmpty() && lane.midiClips.isEmpty()) ? "audio" : "instrument";
            }
        }

        /** A track with both notes and audio: the other kind's clips on a track of their own, after it. */
        void splitMixedTracks()
        {
            for (size_t i = 0; i < lanes.size(); ++i)
            {
                auto& lane = lanes[i];
                const auto audioOnInstrument = lane.kind == "instrument" && ! lane.audioClips.isEmpty();
                const auto notesOnAudio = lane.kind == "audio" && ! lane.midiClips.isEmpty();

                if (! audioOnInstrument && ! notesOnAudio)
                    continue;

                Lane extra;
                extra.kind = audioOnInstrument ? "audio" : "instrument";
                extra.name = lane.name + (audioOnInstrument ? " Audio" : " Notes");
                extra.colour = lane.colour;
                extra.destination = lane.destination;
                extra.volume = lane.volume;
                extra.pan = lane.pan;
                extra.mute = lane.mute;
                extra.solo = lane.solo;

                if (audioOnInstrument)
                    std::swap (extra.audioClips, lane.audioClips);
                else
                    std::swap (extra.midiClips, lane.midiClips);

                note ("\"" + lane.name + "\" had notes and audio: its " + (audioOnInstrument ? "audio" : "notes")
                      + " went onto a track of its own after it.");
                lanes.insert (lanes.begin() + static_cast<std::ptrdiff_t> (i + 1), std::move (extra));
                ++i;
            }
        }

        //==============================================================================
        // Devices
        const DawProject::InstalledVst3* findVst3 (const juce::String& classId) const
        {
            int uniqueId = 0;

            if (! DawProject::juceIdOfClass (classId, uniqueId))
                return nullptr;

            const auto suffix = juce::String::toHexString (uniqueId);

            for (const auto& plugin : installed)
                if (plugin.id.startsWithIgnoreCase ("VST3-") && plugin.id.fromLastOccurrenceOf ("-", false, false).equalsIgnoreCase (suffix))
                    return &plugin;

            return nullptr;
        }

        void readDevices (Lane& lane)
        {
            const auto* devices = lane.channelXml != nullptr ? lane.channelXml->getChildByName ("Devices") : nullptr;

            if (devices == nullptr)
                return;

            int place = 0;

            for (const auto* device : devices->getChildIterator())
            {
                const auto devicePlace = place++;
                const auto format = device->getTagName();
                const auto role = device->getStringAttribute ("deviceRole");
                const auto name = device->getStringAttribute ("deviceName", format).trim();
                const auto* enabled = device->getChildByName ("Enabled");
                const auto on = enabled == nullptr || flagOf (enabled, "value", true);
                const auto* stateReference = device->getChildByName ("State");

                const auto keepIt = [&] (const juce::String& why)
                {
                    KeptDevice kept;
                    kept.name = name;
                    kept.format = format;
                    kept.role = role;
                    kept.place = devicePlace;
                    kept.xml = device->toString (juce::XmlElement::TextFormat().withoutHeader().singleLine());

                    if (stateReference != nullptr)
                    {
                        const auto state = fileOf (stateReference, "Plugin States");

                        if (state.existsAsFile())
                        {
                            auto copy = state;

                            // One beside the .dawproject: a copy in the song's folder, so it stays with the song.
                            if (! state.isAChildOf (songFolder))
                            {
                                copy = songFolder.getChildFile ("Plugin States").getChildFile (state.getFileName());

                                if (copy.exists())
                                    copy = copy.getNonexistentSibling (true);

                                if (! copy.getParentDirectory().createDirectory() || ! state.copyFileTo (copy))
                                    copy = state;
                            }

                            kept.stateFile = copy.getFullPathName();
                        }
                    }

                    lane.kept.add (kept.toVar());
                    note (name + " (" + DawProject::formatName (format) + ", on \"" + (lane.kind == "main" ? juce::String ("Main") : lane.name) + "\") "
                          + why + ": it's kept as a placeholder with its settings, and Export DAWproject writes it back.");
                };

                if (! device->hasTagName ("Vst3Plugin"))
                {
                    keepIt (format == "ClapPlugin" ? "is a CLAP plugin, which Sonify's tracks don't play"
                          : format == "Vst2Plugin" ? "is a VST2 plugin, which Sonify's tracks don't play"
                          : format == "AuPlugin"   ? "is an Audio Unit, which Sonify's tracks don't play"
                                                   : "is the other DAW's own device");
                    continue;
                }

                juce::MemoryBlock preset;
                bool hasPreset = false;

                if (stateReference != nullptr)
                    hasPreset = readReference (*stateReference, preset);

                auto classId = cleanClassId (device->getStringAttribute ("deviceID"));

                if (classId.length() != 32 && hasPreset)
                    classId = cleanClassId (DawProject::classIdOfPreset (preset));

                const auto* plugin = findVst3 (classId);

                if (plugin == nullptr)
                {
                    keepIt ("isn't installed here (or isn't in the Browser yet)");
                    continue;
                }

                const auto asInstrument = role == "instrument";
                const auto fits = asInstrument ? (lane.kind == "instrument" && lane.instrument.isVoid())
                                               : (role == "audioFX" || role == "analyzer" || role.isEmpty());

                if (! fits)
                {
                    keepIt (asInstrument ? "is an instrument on a track that has no place for one" : "is a note effect");
                    continue;
                }

                auto item = object();
                put (item, "file", plugin->file);
                put (item, "pluginId", plugin->id);
                put (item, "name", plugin->name);

                if (hasPreset)
                {
                    const auto state = DawProject::presetToState (preset);

                    if (state.getSize() > 0)
                        put (item, "state", juce::Base64::toBase64 (state.getData(), state.getSize()));
                    else
                        note (name + "'s settings couldn't be read: it starts as it does when it's added.");
                }
                else
                {
                    note (name + " came without its settings: it starts as it does when it's added.");
                }

                const auto slot = asInstrument ? -1 : lane.inserts.size();

                if (asInstrument)
                {
                    lane.instrument = item;
                }
                else
                {
                    put (item, "bypass", ! on);
                    lane.inserts.add (item);
                }

                // Its parameters, for the automation that names them.
                if (const auto* parameters = device->getChildByName ("Parameters"))
                {
                    for (const auto* parameter : parameters->getChildIterator())
                    {
                        const auto id = parameter->getStringAttribute ("id");

                        if (id.isEmpty() || ! parameter->hasAttribute ("parameterID"))
                            continue;

                        const auto vstId = parameter->getStringAttribute ("parameterID").getLargeIntValue();
                        lane.parameters[id] = { slot, static_cast<juce::int64> (static_cast<juce::uint32> (vstId)) };

                        if (parameter->getStringAttribute ("unit", "normalized") != "normalized" && hasNumber (*parameter, "min") && hasNumber (*parameter, "max"))
                            lane.parameterRanges[id] = { numberOf (parameter, "min", 0.0), numberOf (parameter, "max", 1.0) };
                    }
                }
            }
        }

        //==============================================================================
        /** The arrangement's automation, now the devices are in: volume, pan and plugin parameters. */
        void resolveAutomation (Lane& lane)
        {
            for (const auto& raw : lane.rawAutomation)
            {
                juce::var item;
                std::function<double (double)> value;
                const auto* parameter = byId (raw.parameter);
                const auto unit = raw.unit.isNotEmpty() ? raw.unit : (parameter != nullptr ? parameter->getStringAttribute ("unit") : juce::String());

                if (raw.parameter == lane.volumeId && lane.volumeId.isNotEmpty())
                {
                    item = object();
                    put (item, "target", "volume");
                    value = [unit] (double v) { return volumeDb (v, unit.isNotEmpty() ? unit : "linear"); };
                }
                else if (raw.parameter == lane.panId && lane.panId.isNotEmpty())
                {
                    item = object();
                    put (item, "target", "pan");
                    const auto minimum = numberOf (parameter, "min", 0.0);
                    value = [unit, minimum] (double v) { return panOf (v, unit.isNotEmpty() ? unit : "normalized", minimum); };
                }
                else if (const auto found = lane.parameters.find (raw.parameter); found != lane.parameters.end())
                {
                    item = object();
                    put (item, "target", "param");
                    put (item, "insert", found->second.first);
                    put (item, "param", found->second.second);
                    const auto range = lane.parameterRanges.find (raw.parameter);
                    const auto low = range != lane.parameterRanges.end() ? range->second.first : 0.0;
                    const auto high = range != lane.parameterRanges.end() ? range->second.second : 1.0;
                    value = [low, high] (double v) { return std::abs (high - low) < tiny ? 0.0 : juce::jlimit (0.0, 1.0, (v - low) / (high - low)); };
                }
                else
                {
                    if (raw.parameter == tempoId || raw.parameter == signatureId)
                        note ("Tempo automation on a track lane was left out (the arrangement's own is in).");
                    else if (sendParameters.contains (raw.parameter))
                        note ("Send level automation was left out.");
                    else
                        note ("Automation of parameters of plugins that didn't come across was left out.");

                    continue;
                }

                auto sorted = raw.points;
                std::stable_sort (sorted.begin(), sorted.end(), [] (const RawPoint& a, const RawPoint& b) { return a.ppq < b.ppq; });
                juce::Array<juce::var> points;

                for (size_t i = 0; i < sorted.size(); ++i)
                {
                    const auto at = juce::jmax (0.0, sorted[i].ppq);
                    const auto v = value (sorted[i].value);
                    points.add (juce::Array<juce::var> { at, v });

                    // A step: held until just before the next point.
                    if (sorted[i].hold && i + 1 < sorted.size() && sorted[i + 1].ppq - at > 0.002)
                        points.add (juce::Array<juce::var> { sorted[i + 1].ppq - 0.001, v });
                }

                put (item, "points", points);
                lane.automation.add (item);
            }
        }

        //==============================================================================
        juce::var buildSong()
        {
            auto song = object();
            put (song, "format", Song::formatName);
            put (song, "version", Song::formatVersion);
            put (song, "name", songFolder.getFileName());

            auto transport = object();
            put (transport, "bpm", map.getTempos().front().bpm);
            put (transport, "numerator", map.getSignatures().front().numerator);
            put (transport, "denominator", map.getSignatures().front().denominator);
            put (transport, "tempoMap", map.toVar());
            put (transport, "loop", false);
            put (transport, "position", 0.0);
            put (song, "transport", transport);

            auto mainItem = object();
            put (mainItem, "volume", main.volume);
            put (mainItem, "mute", main.mute);
            put (mainItem, "inserts", main.inserts);

            juce::Array<juce::var> mainPoints;

            for (const auto& lane : main.automation)
                if (lane.getProperty ("target", {}).toString() == "volume")
                    if (const auto* points = lane.getProperty ("points", {}).getArray())
                        mainPoints.addArray (*points);

            put (mainItem, "automation", mainPoints);

            if (! main.kept.isEmpty())
                put (mainItem, "kept", main.kept);

            if (! main.instrument.isVoid())
                note ("An instrument on the master channel was left out.");

            put (song, "main", mainItem);

            juce::Array<juce::var> tracks;

            for (const auto& lane : lanes)
            {
                auto track = object();
                put (track, "kind", lane.kind);
                put (track, "name", lane.name);
                put (track, "nameIsAutomatic", false);

                if (lane.colour.isNotEmpty())
                    put (track, "colour", lane.colour);

                put (track, "volume", lane.volume);
                put (track, "pan", lane.pan);
                put (track, "mute", lane.mute);
                put (track, "solo", lane.solo);
                put (track, "output", 0);

                if (lane.kind == "audio")
                    put (track, "mono", lane.mono);

                put (track, "instrument", lane.instrument);
                put (track, "inserts", lane.inserts);
                put (track, "automation", lane.automation);
                put (track, "clips", lane.kind == "audio" ? lane.audioClips : lane.midiClips);

                if (! lane.kept.isEmpty())
                    put (track, "kept", lane.kept);

                // Where it plays: a bus (a group or submix channel it plays into), else Main.
                const auto destination = indexOf (lane.destination);

                if (destination >= 0 && lanes[static_cast<size_t> (destination)].kind == "bus" && &lanes[static_cast<size_t> (destination)] != &lane)
                {
                    if (lane.kind == "bus")
                        note ("Groups inside groups: \"" + lane.name + "\" plays into Main (buses don't play into buses).");
                    else
                        put (track, "bus", destination);
                }
                else if (destination >= 0 && lanes[static_cast<size_t> (destination)].kind != "bus")
                {
                    note ("\"" + lane.name + "\" played into another track's channel: it plays into Main.");
                }

                juce::Array<juce::var> sends;

                for (const auto& send : lane.sends)
                {
                    const auto target = indexOf (send.destination);

                    if (target < 0 || lanes[static_cast<size_t> (target)].kind != "fx")
                    {
                        note ("Sends to anything but an effect channel were left out.");
                        continue;
                    }

                    if (send.pre)
                        note ("Pre-fader sends are post-fader here.");

                    auto item = object();
                    put (item, "sidechain", false);
                    put (item, "level", send.db);
                    put (item, "on", send.on);
                    put (item, "track", target);
                    sends.add (item);
                }

                put (track, "sends", sends);
                tracks.add (track);
            }

            put (song, "tracks", tracks);
            put (song, "selectedTrack", 0);
            return song;
        }
    };

    //==============================================================================
    // Export
    //==============================================================================
    class Exporter
    {
    public:
        Exporter (AudioEngine& e, juce::StringArray& notesOut) : engine (e), notes (notesOut) {}

        DawProject::Package run (const juce::String& title)
        {
            DawProject::Package package;
            const auto& map = engine.getTempoMap();
            const auto& tracks = engine.getTracks();

            juce::XmlElement project ("Project");
            project.setAttribute ("version", "1.0");

            auto* application = project.createNewChildElement ("Application");
            application->setAttribute ("name", "Sonify");
            application->setAttribute ("version", JUCE_APPLICATION_VERSION_STRING);

            // The transport: the song's first tempo and signature (their changes are the arrangement's).
            auto* transport = project.createNewChildElement ("Transport");
            auto* tempo = transport->createNewChildElement ("Tempo");
            const auto tempoId = newId();
            tempo->setAttribute ("id", tempoId);
            tempo->setAttribute ("name", "Tempo");
            tempo->setAttribute ("unit", "bpm");
            tempo->setAttribute ("min", number (TempoMap::minTempo));
            tempo->setAttribute ("max", number (TempoMap::maxTempo));
            tempo->setAttribute ("value", number (map.getTempos().front().bpm));

            auto* signature = transport->createNewChildElement ("TimeSignature");
            const auto signatureId = newId();
            signature->setAttribute ("id", signatureId);
            signature->setAttribute ("numerator", map.getSignatures().front().numerator);
            signature->setAttribute ("denominator", map.getSignatures().front().denominator);

            // Ids first: tracks name the channels they play into, which may come after them.
            const auto masterTrackId = newId();
            const auto masterChannelId = newId();

            for (const auto& track : tracks)
            {
                trackIds[track->id] = newId();
                channelIds[track->id] = newId();
            }

            auto* structure = project.createNewChildElement ("Structure");
            auto* arrangement = new juce::XmlElement ("Arrangement");
            arrangement->setAttribute ("id", newId());
            auto* arrangementLanes = arrangement->createNewChildElement ("Lanes");
            arrangementLanes->setAttribute ("id", newId());
            arrangementLanes->setAttribute ("timeUnit", "beats");

            // Main.
            {
                auto* masterTrack = structure->createNewChildElement ("Track");
                masterTrack->setAttribute ("id", masterTrackId);
                masterTrack->setAttribute ("name", "Master");
                masterTrack->setAttribute ("loaded", "true");

                auto* channel = masterTrack->createNewChildElement ("Channel");
                channel->setAttribute ("id", masterChannelId);
                channel->setAttribute ("role", "master");
                channel->setAttribute ("audioChannels", 2);
                channel->setAttribute ("solo", "false");

                std::map<std::pair<int, juce::int64>, juce::String> parameterIds;
                juce::StringArray left;
                auto* devices = new juce::XmlElement ("Devices");
                writeDevices (*devices, nullptr, engine.getMainInserts(), engine.getMainKept(), {}, parameterIds, left, package);
                finishChannel (*channel, devices, engine.isMasterMuted(), 0.0, {}, engine.getMasterVolume(), masterVolumeId, masterPanId);

                if (! left.isEmpty())
                    leaveOut (*masterTrack, "Main", left);

                std::vector<AutomationPoint> mainPoints (engine.getMainAutomation().begin(), engine.getMainAutomation().end());

                if (! mainPoints.empty())
                {
                    auto* lanes = arrangementLanes->createNewChildElement ("Lanes");
                    lanes->setAttribute ("id", newId());
                    lanes->setAttribute ("track", masterTrackId);
                    writePoints (*lanes, masterVolumeId, "linear", mainPoints, [] (double v) { return gainOf (v); });
                }
            }

            // The tracks, those in a bus inside it (as a group track).
            std::map<int, std::vector<const Track*>> inBus;

            for (const auto& track : tracks)
                if (const auto bus = engine.getTrackBus (track->id); bus != 0 && track->kind != Track::Kind::bus)
                    if (const auto* busTrack = engine.findTrack (bus); busTrack != nullptr && busTrack->kind == Track::Kind::bus)
                        inBus[bus].push_back (track.get());

            for (const auto& track : tracks)
            {
                const auto bus = engine.getTrackBus (track->id);

                if (inBus.count (bus) > 0 && std::find (inBus[bus].begin(), inBus[bus].end(), track.get()) != inBus[bus].end())
                    continue;   // written inside its bus

                auto* written = writeTrack (*structure, *track, masterChannelId, *arrangementLanes, package);

                if (track->kind == Track::Kind::bus)
                    for (const auto* member : inBus[track->id])
                        writeTrack (*written, *member, channelIds[track->id], *arrangementLanes, package);
            }

            // The arrangement's tempo and signature changes.
            const auto& tempos = map.getTempos();

            if (tempos.size() > 1)
            {
                auto* points = arrangement->createNewChildElement ("TempoAutomation");
                points->setAttribute ("id", newId());
                points->setAttribute ("unit", "bpm");
                points->createNewChildElement ("Target")->setAttribute ("parameter", tempoId);

                for (const auto& change : tempos)
                {
                    auto* point = points->createNewChildElement ("RealPoint");
                    point->setAttribute ("time", number (change.ppq));
                    point->setAttribute ("value", number (change.bpm));
                    point->setAttribute ("interpolation", change.ramp ? "linear" : "hold");
                }
            }

            if (const auto& signatures = map.getSignatures(); signatures.size() > 1)
            {
                auto* points = arrangement->createNewChildElement ("TimeSignatureAutomation");
                points->setAttribute ("id", newId());
                points->createNewChildElement ("Target")->setAttribute ("parameter", signatureId);

                for (const auto& change : signatures)
                {
                    auto* point = points->createNewChildElement ("TimeSignaturePoint");
                    point->setAttribute ("time", number (map.ppqOfBar (change.bar)));
                    point->setAttribute ("numerator", change.numerator);
                    point->setAttribute ("denominator", change.denominator);
                }
            }

            project.addChildElement (arrangement);

            package.projectXml = project.toString();

            juce::XmlElement metadata ("MetaData");
            metadata.createNewChildElement ("Title")->addTextElement (title);

            if (! allLeftOut.isEmpty())
                metadata.createNewChildElement ("Comment")->addTextElement ("Made in Sonify. Not in this file (Sonify's own devices): "
                                                                            + allLeftOut.joinIntoString (", ") + ".");

            package.metadataXml = metadata.toString();

            for (const auto& [file, path] : audioPaths)
                package.files.emplace_back (juce::File (file), path);

            if (! allLeftOut.isEmpty())
                notes.add ("Sonify's own instruments and effects (and WCLAP plugins), which no other DAW plays: "
                           + allLeftOut.joinIntoString (", ") + ". The tracks' comments in the file list them.");

            return package;
        }

    private:
        AudioEngine& engine;
        juce::StringArray& notes;
        int lastId = 0;
        std::map<int, juce::String> trackIds, channelIds;
        juce::String masterVolumeId, masterPanId;
        std::map<juce::String, juce::String> audioPaths;   // file here -> path inside
        std::map<juce::String, juce::String> keptPaths;    // a placeholder's settings file -> path inside
        juce::StringArray usedPaths, allLeftOut;
        std::map<juce::String, std::tuple<double, int, double>> audioInfo;   // file -> sample rate, channels, seconds
        int stateCount = 0;

        juce::String newId()
        {
            return "id" + juce::String (++lastId);
        }

        void note (const juce::String& text)
        {
            notes.addIfNotAlreadyThere (text);
        }

        juce::String uniquePath (const juce::String& folder, const juce::String& name)
        {
            auto legal = juce::File::createLegalFileName (name);

            if (legal.isEmpty())
                legal = "file";

            auto path = folder + "/" + legal;

            for (int n = 2; usedPaths.contains (path, true); ++n)
                path = folder + "/" + legal.upToLastOccurrenceOf (".", false, false) + " (" + juce::String (n) + ")"
                     + (legal.containsChar ('.') ? "." + legal.fromLastOccurrenceOf (".", false, false) : juce::String());

            usedPaths.add (path);
            return path;
        }

        void leaveOut (juce::XmlElement& track, const juce::String& where, const juce::StringArray& left)
        {
            track.setAttribute ("comment", "Not in this file (Sonify's own): " + left.joinIntoString (", "));

            for (const auto& name : left)
                allLeftOut.addIfNotAlreadyThere (name + " (" + where + ")");
        }

        juce::XmlElement* realParameter (juce::XmlElement& parent, const juce::String& tag, const juce::String& id, const juce::String& name,
                                          const juce::String& unit, double minimum, double maximum, double value)
        {
            auto* element = parent.createNewChildElement (tag);
            element->setAttribute ("id", id);
            element->setAttribute ("name", name);
            element->setAttribute ("unit", unit);
            element->setAttribute ("min", number (minimum));
            element->setAttribute ("max", number (maximum));
            element->setAttribute ("value", number (value));
            return element;
        }

        /** A channel's mixer, after its devices: Mute, Pan, Sends, Volume (the format's order). */
        void finishChannel (juce::XmlElement& channel, juce::XmlElement* devices, bool mute, double pan, std::unique_ptr<juce::XmlElement> sends,
                            double volumeDbValue, juce::String& volumeId, juce::String& panId)
        {
            if (devices->getNumChildElements() > 0)
                channel.addChildElement (devices);
            else
                delete devices;

            auto* muteElement = channel.createNewChildElement ("Mute");
            muteElement->setAttribute ("id", newId());
            muteElement->setAttribute ("name", "Mute");
            muteElement->setAttribute ("value", mute ? "true" : "false");

            panId = newId();
            realParameter (channel, "Pan", panId, "Pan", "normalized", 0.0, 1.0, (pan + 1.0) / 2.0);

            if (sends != nullptr && sends->getNumChildElements() > 0)
                channel.addChildElement (sends.release());

            volumeId = newId();
            const auto gain = gainOf (volumeDbValue);
            realParameter (channel, "Volume", volumeId, "Volume", "linear", 0.0, juce::jmax (2.0, gainOf (Track::maxDb)), gain);
        }

        /** A plugin as a device: VST3 (with its .vstpreset); anything else is listed in left. */
        void writePlugin (juce::XmlElement& devices, Plugin& plugin, bool instrument, bool bypassed, int slot,
                          const std::vector<AutomationLane>* automation, std::map<std::pair<int, juce::int64>, juce::String>& parameterIds,
                          juce::StringArray& left, DawProject::Package& package)
        {
            auto* vst3 = dynamic_cast<Vst3Plugin*> (&plugin);
            const auto& descriptor = plugin.getDescriptor();

            if (vst3 == nullptr || plugin.getFormatName() != "VST3")
            {
                left.add (descriptor.name);
                return;
            }

            const auto preset = vst3->getVst3Preset();
            const auto classId = DawProject::classIdOfPreset (preset);

            auto* device = devices.createNewChildElement ("Vst3Plugin");
            device->setAttribute ("id", newId());
            device->setAttribute ("name", descriptor.name);
            device->setAttribute ("deviceName", descriptor.name);
            device->setAttribute ("deviceRole", instrument ? "instrument" : "audioFX");

            if (classId.isNotEmpty())
                device->setAttribute ("deviceID", classId);

            if (descriptor.vendor.isNotEmpty())
                device->setAttribute ("deviceVendor", descriptor.vendor);

            if (descriptor.version.isNotEmpty())
                device->setAttribute ("pluginVersion", descriptor.version);

            device->setAttribute ("loaded", "true");

            // The parameters its automation names.
            if (automation != nullptr)
            {
                juce::XmlElement* parameters = nullptr;

                for (const auto& lane : *automation)
                {
                    if (lane.target.kind != AutomationTarget::Kind::parameter || lane.target.slot != slot || lane.points.empty())
                        continue;

                    const auto key = std::make_pair (slot, static_cast<juce::int64> (lane.target.paramId));

                    if (parameterIds.count (key) > 0)
                        continue;

                    const auto index = plugin.findParameter (lane.target.paramId);

                    if (index < 0)
                        continue;

                    if (parameters == nullptr)
                        parameters = device->createNewChildElement ("Parameters");

                    const auto id = newId();
                    auto* parameter = realParameter (*parameters, "RealParameter", id, plugin.getParameters()[static_cast<size_t> (index)].name,
                                                     "normalized", 0.0, 1.0, juce::jlimit (0.0, 1.0, plugin.readParameterValue (index)));
                    parameter->setAttribute ("parameterID", static_cast<int> (lane.target.paramId));
                    parameterIds[key] = id;
                }
            }

            auto* enabled = device->createNewChildElement ("Enabled");
            enabled->setAttribute ("id", newId());
            enabled->setAttribute ("name", "On/Off");
            enabled->setAttribute ("value", bypassed ? "false" : "true");

            if (preset.getSize() > 0)
            {
                const auto path = uniquePath ("plugins", descriptor.name + " " + juce::String (++stateCount) + ".vstpreset");
                auto* state = device->createNewChildElement ("State");
                state->setAttribute ("path", path);
                package.blocks.emplace_back (preset, path);
            }
            else
            {
                note (descriptor.name + "'s settings couldn't be read: it's in the file as it starts.");
            }
        }

        /** A kept placeholder, as it came (its own ids made afresh, its settings file inside). */
        std::unique_ptr<juce::XmlElement> keptDevice (const juce::var& kept, DawProject::Package& package)
        {
            auto device = juce::XmlDocument::parse (kept.getProperty ("xml", {}).toString());

            if (device == nullptr)
                return nullptr;

            std::function<void (juce::XmlElement&)> renumber = [this, &renumber] (juce::XmlElement& element)
            {
                if (element.hasAttribute ("id"))
                    element.setAttribute ("id", newId());

                for (auto* child : element.getChildIterator())
                    renumber (*child);
            };

            renumber (*device);

            if (auto* state = device->getChildByName ("State"))
            {
                const juce::File file (kept.getProperty ("stateFile", {}).toString());

                if (kept.getProperty ("stateFile", {}).toString().isNotEmpty() && file.existsAsFile())
                {
                    auto& path = keptPaths[file.getFullPathName()];   // one copy of a file several share

                    if (path.isEmpty())
                    {
                        path = uniquePath ("plugins", file.getFileName());
                        package.files.emplace_back (file, path);
                    }

                    state->setAttribute ("path", path);
                    state->removeAttribute ("external");
                }
                else
                {
                    device->removeChildElement (state, true);
                    note (kept.getProperty ("name", "A placeholder").toString() + "'s settings file is missing: it's in the file without them.");
                }
            }

            return device;
        }

        void writeDevices (juce::XmlElement& devices, Plugin* instrument, const EffectChain& inserts, const juce::var& kept,
                           const std::vector<AutomationLane>* automation, std::map<std::pair<int, juce::int64>, juce::String>& parameterIds,
                           juce::StringArray& left, DawProject::Package& package)
        {
            juce::XmlElement made ("Made");

            if (instrument != nullptr)
                writePlugin (made, *instrument, true, false, AudioEngine::instrumentSlot, automation, parameterIds, left, package);

            for (int i = 0; i < inserts.size(); ++i)
                if (const auto* insert = inserts.at (i); insert != nullptr && insert->plugin != nullptr)
                    writePlugin (made, *insert->plugin, false, insert->bypassed.load(), insert->id, automation, parameterIds, left, package);

            std::vector<std::unique_ptr<juce::XmlElement>> list;

            while (auto* child = made.getFirstChildElement())
            {
                made.removeChildElement (child, false);
                list.emplace_back (child);
            }

            // The placeholders where they were among the devices.
            if (const auto* items = kept.getArray())
            {
                std::vector<std::pair<int, juce::var>> sorted;

                for (const auto& item : *items)
                    sorted.emplace_back (static_cast<int> (item.getProperty ("place", 0)), item);

                std::stable_sort (sorted.begin(), sorted.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });

                for (const auto& [place, item] : sorted)
                    if (auto device = keptDevice (item, package))
                        list.insert (list.begin() + juce::jlimit (0, static_cast<int> (list.size()), place), std::move (device));
            }

            for (auto& device : list)
                devices.addChildElement (device.release());
        }

        void writePoints (juce::XmlElement& lanes, const juce::String& parameter, const juce::String& unit,
                          const std::vector<AutomationPoint>& points, const std::function<double (double)>& value)
        {
            auto* element = lanes.createNewChildElement ("Points");
            element->setAttribute ("id", newId());
            element->setAttribute ("unit", unit);
            element->createNewChildElement ("Target")->setAttribute ("parameter", parameter);

            for (const auto& point : points)
            {
                auto* written = element->createNewChildElement ("RealPoint");
                written->setAttribute ("time", number (juce::jmax (0.0, point.ppq)));
                written->setAttribute ("value", number (value (point.value)));
                written->setAttribute ("interpolation", "linear");
            }
        }

        std::tuple<double, int, double> infoOf (const juce::String& path)
        {
            if (const auto found = audioInfo.find (path); found != audioInfo.end())
                return found->second;

            std::tuple<double, int, double> info { 0.0, 0, 0.0 };

            if (std::unique_ptr<juce::AudioFormatReader> reader (engine.getMedia().getFormats().createReaderFor (juce::File (path))); reader != nullptr)
                info = { reader->sampleRate, static_cast<int> (reader->numChannels),
                         reader->sampleRate > 0.0 ? static_cast<double> (reader->lengthInSamples) / reader->sampleRate : 0.0 };

            audioInfo[path] = info;
            return info;
        }

        juce::String audioPathOf (const juce::String& file)
        {
            if (const auto found = audioPaths.find (file); found != audioPaths.end())
                return found->second;

            const auto path = uniquePath ("audio", juce::File (file).getFileName());
            audioPaths[file] = path;
            return path;
        }

        void writeClips (juce::XmlElement& lanes, const Track& track)
        {
            if (track.clips.empty())
                return;

            auto* clips = lanes.createNewChildElement ("Clips");
            clips->setAttribute ("id", newId());

            for (const auto& clip : track.clips)
            {
                auto* element = clips->createNewChildElement ("Clip");
                element->setAttribute ("time", number (clip.start));
                element->setAttribute ("duration", number (clip.length));

                if (clip.muted)
                    element->setAttribute ("enable", "false");

                if (clip.isAudio())
                {
                    if (! juce::File (clip.audioFile).existsAsFile())
                    {
                        note ("An audio clip's file is missing (" + clip.audioFile + "): the clip is in the file, its sound isn't.");
                        clips->removeChildElement (element, true);
                        continue;
                    }

                    element->setAttribute ("contentTimeUnit", "seconds");
                    element->setAttribute ("playStart", number (clip.audioOffset));
                    element->setAttribute ("playStop", number (clip.audioOffset + clip.audioLength));

                    if (clip.fadeIn > 0.0 || clip.fadeOut > 0.0)
                    {
                        element->setAttribute ("fadeTimeUnit", "seconds");
                        element->setAttribute ("fadeInTime", number (clip.fadeIn));
                        element->setAttribute ("fadeOutTime", number (clip.fadeOut));
                    }

                    if (std::abs (clip.audioGainDb) > 0.01)
                        note ("Audio clips' own volume isn't in the file (DAWproject has no place for it).");

                    if (clip.silence.on || clip.effect != nullptr || ! clip.envelopes.empty())
                        note ("Audio clips' Silence Areas, own effects and own automation aren't in the file.");

                    auto [rate, channels, seconds] = infoOf (clip.audioFile);

                    if (rate <= 0.0)
                    {
                        note ("Some audio files couldn't be read for their details: the file names them as 48 kHz stereo.");
                        rate = 48000.0;
                        channels = 2;
                        seconds = clip.audioOffset + clip.audioLength;
                    }

                    auto* audio = element->createNewChildElement ("Audio");
                    audio->setAttribute ("id", newId());
                    audio->setAttribute ("timeUnit", "seconds");
                    audio->setAttribute ("duration", number (seconds));
                    audio->setAttribute ("channels", channels);
                    audio->setAttribute ("sampleRate", juce::roundToInt (rate));
                    audio->createNewChildElement ("File")->setAttribute ("path", audioPathOf (clip.audioFile));
                }
                else
                {
                    element->setAttribute ("playStart", number (clip.offset));

                    if (! clip.pitchBend.empty() || ! clip.aftertouch.empty() || ! clip.envelopes.empty())
                        note ("MIDI clips' pitch bend, aftertouch and controller lines aren't in the file (their notes are).");

                    auto* notesElement = element->createNewChildElement ("Notes");
                    notesElement->setAttribute ("id", newId());

                    for (const auto& n : clip.notes)
                    {
                        if (n.muted)
                            continue;

                        auto* written = notesElement->createNewChildElement ("Note");
                        written->setAttribute ("time", number (n.start));
                        written->setAttribute ("duration", number (n.length));
                        written->setAttribute ("channel", 0);
                        written->setAttribute ("key", n.pitch);
                        written->setAttribute ("vel", number (juce::jlimit (0.0, 1.0, static_cast<double> (n.velocity))));
                        written->setAttribute ("rel", number (0.5));
                    }
                }
            }
        }

        juce::XmlElement* writeTrack (juce::XmlElement& parent, const Track& track, const juce::String& destination,
                                      juce::XmlElement& arrangementLanes, DawProject::Package& package)
        {
            const auto isBus = track.kind == Track::Kind::bus;
            const auto isFx = track.kind == Track::Kind::fx;
            const auto isAudio = track.kind == Track::Kind::audio;

            auto* element = parent.createNewChildElement ("Track");
            element->setAttribute ("id", trackIds[track.id]);
            element->setAttribute ("name", track.name);
            element->setAttribute ("color", htmlColour (track.colour));
            element->setAttribute ("loaded", "true");

            if (isBus)
                element->setAttribute ("contentType", "tracks");
            else if (! isFx)
                element->setAttribute ("contentType", isAudio ? "audio" : "notes");

            if (track.output != 0)
                note ("Tracks playing into Sub outputs play into the master channel there.");

            auto* channel = element->createNewChildElement ("Channel");
            channel->setAttribute ("id", channelIds[track.id]);
            channel->setAttribute ("role", isBus ? "submix" : isFx ? "effect" : "regular");
            channel->setAttribute ("audioChannels", isAudio && track.monoMode.load() ? 1 : 2);
            channel->setAttribute ("solo", track.solo.load() ? "true" : "false");
            channel->setAttribute ("destination", destination);

            std::map<std::pair<int, juce::int64>, juce::String> parameterIds;
            juce::StringArray left;
            auto* devices = new juce::XmlElement ("Devices");
            writeDevices (*devices, track.instrument.get(), track.inserts, track.kept, &track.automation, parameterIds, left, package);

            if (track.midiFx.size() > 0)
                left.add ("MIDI FX");

            // Sends to FX channels (sidechains have no place in the format).
            auto sends = std::make_unique<juce::XmlElement> ("Sends");

            for (const auto& send : track.sends)
            {
                if (send.sidechain)
                {
                    note ("Sidechain sends aren't in the file.");
                    continue;
                }

                const auto target = channelIds.find (send.target);

                if (target == channelIds.end())
                    continue;

                auto* written = sends->createNewChildElement ("Send");
                written->setAttribute ("id", newId());
                written->setAttribute ("destination", target->second);
                written->setAttribute ("type", "post");

                auto* enable = written->createNewChildElement ("Enable");
                enable->setAttribute ("id", newId());
                enable->setAttribute ("name", "On/Off");
                enable->setAttribute ("value", send.on ? "true" : "false");

                realParameter (*written, "Volume", newId(), "Send", "linear", 0.0, juce::jmax (2.0, gainOf (Track::maxDb)), gainOf (send.levelDb));
            }

            juce::String volumeId, panId;
            finishChannel (*channel, devices, track.mute.load(), track.pan.load(), std::move (sends), track.volumeDb.load(), volumeId, panId);

            if (! left.isEmpty())
                leaveOut (*element, track.name, left);

            // Its lanes: clips and automation.
            auto* lanes = arrangementLanes.createNewChildElement ("Lanes");
            lanes->setAttribute ("id", newId());
            lanes->setAttribute ("track", trackIds[track.id]);

            if (! isBus && ! isFx)
                writeClips (*lanes, track);

            for (const auto& lane : track.automation)
            {
                if (lane.points.empty())
                    continue;

                if (lane.target.kind == AutomationTarget::Kind::volume)
                {
                    writePoints (*lanes, volumeId, "linear", lane.points, [] (double v) { return gainOf (v); });
                }
                else if (lane.target.kind == AutomationTarget::Kind::pan)
                {
                    writePoints (*lanes, panId, "normalized", lane.points, [] (double v) { return (juce::jlimit (-1.0, 1.0, v) + 1.0) / 2.0; });
                }
                else if (const auto found = parameterIds.find ({ lane.target.slot, static_cast<juce::int64> (lane.target.paramId) }); found != parameterIds.end())
                {
                    writePoints (*lanes, found->second, "normalized", lane.points, [] (double v) { return juce::jlimit (0.0, 1.0, v); });
                }
                else
                {
                    note ("Automation of Sonify's own instruments and effects isn't in the file.");
                }
            }

            if (lanes->getNumChildElements() == 0)
                arrangementLanes.removeChildElement (lanes, true);

            return element;
        }
    };
}

//==============================================================================
juce::String DawProject::formatName (const juce::String& element)
{
    if (element == "Vst3Plugin")   return "VST3";
    if (element == "Vst2Plugin")   return "VST2";
    if (element == "ClapPlugin")   return "CLAP";
    if (element == "AuPlugin")     return "AU";
    return "built-in device";
}

juce::MemoryBlock DawProject::presetToState (const juce::MemoryBlock& preset)
{
    PresetChunks chunks;

    if (! readPreset (preset, chunks))
        return {};

    juce::XmlElement state ("VST3PluginState");
    state.createNewChildElement ("IComponent")->addTextElement (chunks.component.toBase64Encoding());

    if (chunks.hasController)
        state.createNewChildElement ("IEditController")->addTextElement (chunks.controller.toBase64Encoding());

    juce::MemoryBlock result;
    juce::AudioProcessor::copyXmlToBinary (state, result);
    return result;
}

juce::String DawProject::classIdOfPreset (const juce::MemoryBlock& preset)
{
    const auto* bytes = static_cast<const juce::uint8*> (preset.getData());

    if (preset.getSize() < 48 || std::memcmp (bytes, "VST3", 4) != 0)
        return {};

    const auto text = juce::String::fromUTF8 (reinterpret_cast<const char*> (bytes + 8), 32);
    return text.containsOnly ("0123456789abcdefABCDEF") && text.length() == 32 ? text.toUpperCase() : juce::String();
}

bool DawProject::juceIdOfClass (const juce::String& classId, int& uniqueId)
{
    const auto text = cleanClassId (classId);

    if (text.length() != 32 || ! text.containsOnly ("0123456789ABCDEF"))
        return false;

    // JUCE's: the class id as four 32-bit numbers (as it's written), each added in after * 31.
    juce::uint32 value = 0;

    for (int i = 0; i < 4; ++i)
        value = value * 31u + static_cast<juce::uint32> (text.substring (i * 8, i * 8 + 8).getHexValue64());

    uniqueId = static_cast<int> (value);
    return true;
}

juce::String DawProject::titleOf (const juce::File& source)
{
    juce::ZipFile zip (source);

    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        if (const auto* entry = zip.getEntry (i); entry != nullptr && pathInside (entry->filename).equalsIgnoreCase ("metadata.xml"))
        {
            std::unique_ptr<juce::InputStream> stream (zip.createStreamForEntry (i));

            if (stream == nullptr)
                break;

            if (const auto metadata = juce::XmlDocument::parse (stream->readEntireStreamAsString()))
                if (const auto* title = metadata->getChildByName ("Title"))
                    if (const auto text = title->getAllSubText().trim(); text.isNotEmpty())
                        return text;

            break;
        }
    }

    return source.getFileNameWithoutExtension();
}

juce::Result DawProject::importProject (const juce::File& source, const juce::File& songFolder, const std::vector<InstalledVst3>& installed,
                                        juce::File& songFile, juce::StringArray& notes)
{
    if (! source.existsAsFile())
        return juce::Result::fail (source.getFullPathName() + " isn't there.");

    Importer importer (source, songFolder, installed, notes);
    return importer.run (songFile);
}

DawProject::Package DawProject::exportProject (AudioEngine& engine, const juce::String& title, juce::StringArray& notes)
{
    engine.captureClipEffects();
    Exporter exporter (engine, notes);
    return exporter.run (title);
}

juce::Result DawProject::Package::write (const juce::File& target) const
{
    juce::ZipFile::Builder builder;
    const auto now = juce::Time::getCurrentTime();

    const auto addText = [&builder, now] (const juce::String& text, const juce::String& path)
    {
        builder.addEntry (std::make_unique<juce::MemoryInputStream> (juce::MemoryBlock (text.toRawUTF8(), text.getNumBytesAsUTF8()), true),
                          9, path, now);
    };

    addText (projectXml, "project.xml");
    addText (metadataXml, "metadata.xml");

    juce::int64 total = projectXml.getNumBytesAsUTF8() + metadataXml.getNumBytesAsUTF8();

    for (const auto& [block, path] : blocks)
    {
        builder.addEntry (std::make_unique<juce::MemoryInputStream> (block, true), 6, path, now);
        total += static_cast<juce::int64> (block.getSize());
    }

    for (const auto& [file, path] : files)
    {
        if (! file.existsAsFile())
            return juce::Result::fail (file.getFullPathName() + " isn't there any more.");

        builder.addFile (file, 0, path);   // audio as it is: it doesn't shrink
        total += file.getSize();
    }

    // A zip without its 64-bit extension (as this one is) holds up to 4 GB.
    if (total > static_cast<juce::int64> (0xF0000000u))
        return juce::Result::fail ("The song's audio is more than 4 GB, more than one DAWproject file holds.");

    if (! target.getParentDirectory().createDirectory())
        return juce::Result::fail ("The folder " + target.getParentDirectory().getFullPathName() + " couldn't be made.");

    juce::TemporaryFile temporary (target);

    {
        auto out = temporary.getFile().createOutputStream();

        if (out == nullptr || ! builder.writeToStream (*out, nullptr))
            return juce::Result::fail ("The file couldn't be written (is the disk full?).");

        out->flush();

        if (out->getStatus().failed())
            return juce::Result::fail (out->getStatus().getErrorMessage());
    }

    if (! temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail ("The file couldn't be put in place of " + target.getFullPathName() + " (is it open in another app?).");

    return juce::Result::ok();
}

//==============================================================================
DawProject::Writer::Writer (Package packageToWrite, const juce::File& targetFile, std::function<void (juce::Result)> whenDone)
    : juce::Thread ("DAWproject export"), package (std::move (packageToWrite)), target (targetFile), done (std::move (whenDone))
{
    startThread();
}

DawProject::Writer::~Writer()
{
    stopThread (-1);   // an export under way finishes first
}

void DawProject::Writer::run()
{
    const auto result = package.write (target);

    juce::MessageManager::callAsync ([whenDone = done, result]
    {
        if (whenDone)
            whenDone (result);
    });
}
