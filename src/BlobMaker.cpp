// D:\workspace\Fanan_web_hub\src\BlobMaker.cpp

#include "BlobMaker.h"

#include "stella_blob.h"
#include "Licences.h"
#include "Project.h"

#include <juce_events/juce_events.h>

#include <map>

namespace
{
    //==========================================================================
    // Reading the SFZ.

    /** A region's opcodes, in order, each once (a later one replaces it where it was). */
    struct Opcodes
    {
        std::vector<std::pair<std::string, std::string>> list;

        void set (const std::string& key, const std::string& value)
        {
            for (auto& [k, v] : list)
            {
                if (k == key)
                {
                    v = value;
                    return;
                }
            }

            list.emplace_back (key, value);
        }

        const std::string* get (const std::string& key) const
        {
            for (const auto& [k, v] : list)
                if (k == key)
                    return &v;

            return nullptr;
        }

        bool has (const std::string& key) const    { return get (key) != nullptr; }
    };

    /** A key's number from a note name ("c4", "C#4", "db3": c4 is 60) or a number; -1 if neither. */
    int noteNumber (const juce::String& text)
    {
        const auto t = text.trim();

        if (t.isEmpty())
            return -1;

        if (juce::CharacterFunctions::isDigit (t[0]) || (t[0] == '-' && t.length() > 1 && juce::CharacterFunctions::isDigit (t[1])))
            return t.getIntValue();

        static const int semitones[] { 9, 11, 0, 2, 4, 5, 7 };   // a b c d e f g
        const auto letter = juce::CharacterFunctions::toLowerCase (t[0]);

        if (letter < 'a' || letter > 'g')
            return -1;

        auto semitone = semitones[letter - 'a'];
        auto rest = t.substring (1);

        if (rest.startsWithChar ('#'))       { ++semitone; rest = rest.substring (1); }
        else if (rest.startsWithChar ('b'))  { --semitone; rest = rest.substring (1); }

        if (rest.isEmpty() || ! (juce::CharacterFunctions::isDigit (rest[0]) || rest[0] == '-'))
            return -1;

        return (rest.getIntValue() + 1) * 12 + semitone;
    }

    bool isKeyOpcode (const std::string& key)
    {
        static const char* const keys[] { "lokey", "hikey", "pitch_keycenter", "sw_lokey", "sw_hikey", "sw_last", "sw_down",
                                          "sw_up", "sw_previous", "sw_default", "loprev", "hiprev" };

        for (const auto* k : keys)
            if (key == k)
                return true;

        return false;
    }

    /** The text with every #include put in its place (paths from the main SFZ's folder). */
    juce::String withIncludes (const juce::File& file, const juce::File& root, int depth, juce::StringArray& notes)
    {
        const auto text = file.loadFileAsString();

        if (depth > 8 || ! text.contains ("#include"))
            return text;

        juce::String out;

        for (const auto& line : juce::StringArray::fromLines (text))
        {
            const auto trimmed = line.trimStart();

            if (trimmed.startsWith ("#include"))
            {
                const auto name = trimmed.fromFirstOccurrenceOf ("\"", false, false).upToFirstOccurrenceOf ("\"", false, false).replaceCharacter ('\\', '/');
                const auto included = root.getChildFile (name);

                if (name.isNotEmpty() && included.existsAsFile())
                    out << withIncludes (included, root, depth + 1, notes) << "\n";
                else
                    notes.addIfNotAlreadyThere ("#include \"" + name + "\" isn't there");

                continue;
            }

            out << line << "\n";
        }

        return out;
    }

    /** Comments gone: // to the line's end, and block comments. */
    juce::String withoutComments (const juce::String& text)
    {
        juce::String out;
        out.preallocateBytes (text.getNumBytesAsUTF8());
        auto p = text.getCharPointer();
        bool block = false;

        while (! p.isEmpty())
        {
            const auto c = *p;

            if (block)
            {
                if (c == '*' && *(p + 1) == '/')
                {
                    block = false;
                    p += 2;
                    continue;
                }

                if (c == '\n')
                    out << "\n";

                ++p;
                continue;
            }

            if (c == '/' && *(p + 1) == '*')
            {
                block = true;
                p += 2;
                continue;
            }

            if (c == '/' && *(p + 1) == '/')
            {
                while (! p.isEmpty() && *p != '\n')
                    ++p;

                continue;
            }

            out << juce::String::charToString (c);
            ++p;
        }

        return out;
    }

    struct Parsed
    {
        juce::String name;
        std::vector<Opcodes> regions;
        juce::StringArray notes;
    };

    Parsed parse (const juce::File& sfz)
    {
        Parsed parsed;
        const auto root = sfz.getParentDirectory();
        auto text = withIncludes (sfz, root, 0, parsed.notes);

        // The instrument's own name, as the Sampling Studio and Sampler Arena write it.
        for (const auto& line : juce::StringArray::fromLines (text))
        {
            if (line.trimStart().startsWith ("// Family:"))
            {
                parsed.name = line.fromFirstOccurrenceOf ("// Family:", false, false).trim();
                break;
            }
        }

        if (parsed.name.isEmpty())
            parsed.name = sfz.getFileNameWithoutExtension();

        text = withoutComments (text);

        enum class Section { none, control, global, master, group, region, other };
        auto section = Section::none;
        Opcodes global, master, group, region;
        juce::String defaultPath;
        int noteOffset = 0, octaveOffset = 0;
        std::map<juce::String, juce::String> defines;

        const auto flush = [&]
        {
            if (section == Section::region)
                parsed.regions.push_back (region);
        };

        // A region: what its <global>, <master> and <group> said, then its own.
        const auto startRegion = [&]
        {
            region = global;

            for (const auto* level : { &master, &group })
                for (const auto& [k, v] : level->list)
                    region.set (k, v);
        };

        const auto apply = [&] (const juce::String& keyText, const juce::String& valueText)
        {
            const auto key = keyText.toLowerCase().toStdString();
            auto value = valueText.trim();

            if (section == Section::control)
            {
                if (key == "default_path")         defaultPath = value.replaceCharacter ('\\', '/');
                else if (key == "note_offset")     noteOffset = value.getIntValue();
                else if (key == "octave_offset")   octaveOffset = value.getIntValue();

                return;
            }

            auto* target = section == Section::global ? &global : section == Section::master ? &master
                         : section == Section::group ? &group : section == Section::region ? &region : nullptr;

            if (target == nullptr)
                return;

            if (key == "sample")
            {
                target->set ("sample", (value.startsWithChar ('*') ? value : defaultPath + value).toStdString());   // *sine: the player's own
                return;
            }

            const auto shifted = [&] (const juce::String& v) -> std::string
            {
                const auto n = noteNumber (v);
                return n >= 0 || v.trim().startsWithChar ('-') ? std::to_string (juce::jlimit (-1, 127, n + noteOffset + 12 * octaveOffset))
                                                                : v.toStdString();
            };

            if (key == "key")
            {
                const auto n = shifted (value);
                target->set ("lokey", n);
                target->set ("hikey", n);
                target->set ("pitch_keycenter", n);
                return;
            }

            if (key == "pitch_keycenter" && value.equalsIgnoreCase ("sample"))
            {
                target->set (key, "sample");
                return;
            }

            target->set (key, isKeyOpcode (key) ? shifted (value) : value.toStdString());
        };

        // An opcode's value runs to the next opcode (name=) or the line's end: a sample's name may
        // have spaces in it.
        const auto opcodes = [&] (const juce::String& line)
        {
            auto rest = line.trim();

            while (rest.isNotEmpty())
            {
                const auto equals = rest.indexOfChar ('=');

                if (equals <= 0)
                    break;

                const auto key = rest.substring (0, equals).trim();
                const auto valueAndMore = rest.substring (equals + 1);
                auto end = valueAndMore.length();

                for (int i = 0; i < valueAndMore.length(); ++i)
                {
                    if (! juce::CharacterFunctions::isWhitespace (valueAndMore[i]))
                        continue;

                    int j = i;

                    while (j < valueAndMore.length() && juce::CharacterFunctions::isWhitespace (valueAndMore[j]))
                        ++j;

                    int w = j;

                    while (w < valueAndMore.length() && (juce::CharacterFunctions::isLetterOrDigit (valueAndMore[w]) || valueAndMore[w] == '_'))
                        ++w;

                    if (w > j && w < valueAndMore.length() && valueAndMore[w] == '=')
                    {
                        end = i;
                        break;
                    }
                }

                apply (key, valueAndMore.substring (0, end).trim());
                rest = valueAndMore.substring (end).trim();
            }
        };

        for (auto line : juce::StringArray::fromLines (text))
        {
            line = line.trim();

            if (line.startsWith ("#define"))
            {
                const auto rest = line.substring (7).trim();
                const auto name = rest.upToFirstOccurrenceOf (" ", false, false).trim();

                if (name.startsWithChar ('$'))
                    defines[name] = rest.fromFirstOccurrenceOf (" ", false, false).trim();

                continue;
            }

            // $NAME: what its #define says.
            if (line.containsChar ('$') && ! defines.empty())
            {
                juce::String replaced;

                for (int i = 0; i < line.length();)
                {
                    if (line[i] == '$')
                    {
                        int j = i + 1;

                        while (j < line.length() && (juce::CharacterFunctions::isLetterOrDigit (line[j]) || line[j] == '_'))
                            ++j;

                        const auto name = line.substring (i, j);

                        if (const auto found = defines.find (name); found != defines.end())
                        {
                            replaced << found->second;
                            i = j;
                            continue;
                        }
                    }

                    replaced << juce::String::charToString (line[i]);
                    ++i;
                }

                line = replaced;
            }

            while (line.isNotEmpty())
            {
                const auto open = line.indexOfChar ('<');

                if (open != 0)
                {
                    opcodes (open < 0 ? line : line.substring (0, open));

                    if (open < 0)
                        break;

                    line = line.substring (open);
                }

                const auto close = line.indexOfChar ('>');

                if (close < 0)
                    break;

                const auto header = line.substring (1, close).trim().toLowerCase();
                line = line.substring (close + 1).trim();
                flush();

                // Each header starts the levels under it afresh.
                if (header == "control")       { section = Section::control; }
                else if (header == "global")   { section = Section::global; global = Opcodes(); master = Opcodes(); group = Opcodes(); }
                else if (header == "master")   { section = Section::master; master = Opcodes(); group = Opcodes(); }
                else if (header == "group")    { section = Section::group; group = Opcodes(); }
                else if (header == "region")   { section = Section::region; startRegion(); }
                else                           { section = Section::other; }
            }
        }

        flush();
        return parsed;
    }

    /** A sample by its path in the SFZ: from its folder, letters' case aside where the disk minds it. */
    juce::File sampleFile (const juce::File& root, const juce::String& path)
    {
        const auto native = path.replaceCharacter ('\\', '/').trim();

        if (native.isEmpty())
            return {};

        const auto direct = juce::File::isAbsolutePath (native) ? juce::File (native) : root.getChildFile (native);

        if (direct.existsAsFile() || juce::File::isAbsolutePath (native))
            return direct;

        // Walk it, a folder at a time, matching names whatever their case.
        auto folder = root;

        for (const auto& part : juce::StringArray::fromTokens (native, "/", {}))
        {
            if (part.isEmpty() || part == ".")
                continue;

            if (part == "..")
            {
                folder = folder.getParentDirectory();
                continue;
            }

            auto next = folder.getChildFile (part);

            if (! next.exists())
                for (const auto& child : folder.findChildFiles (juce::File::findFilesAndDirectories, false, "*"))
                    if (child.getFileName().equalsIgnoreCase (part))
                        next = child;

            folder = next;
        }

        return folder;
    }

    stella::blob::Format formatFor (const juce::AudioFormatReader& reader, BlobMaker::Samples wanted)
    {
        switch (wanted)
        {
            case BlobMaker::Samples::s16:       return stella::blob::Format::s16;
            case BlobMaker::Samples::s24:       return stella::blob::Format::s24;
            case BlobMaker::Samples::f32:       return stella::blob::Format::f32;
            case BlobMaker::Samples::asTheyAre: break;
        }

        // Lossy files are made 24-bit (their own floats would double their size for nothing).
        const auto kind = reader.getFormatName().toLowerCase();

        if (kind.contains ("ogg") || kind.contains ("mp3") || kind.contains ("vorbis"))
            return stella::blob::Format::s24;

        if (reader.usesFloatingPointData || reader.bitsPerSample > 24)
            return stella::blob::Format::f32;

        return reader.bitsPerSample <= 16 ? stella::blob::Format::s16 : stella::blob::Format::s24;
    }

    juce::String samplesText (int n)    { return juce::String (n) + (n == 1 ? " sample" : " samples"); }

    juce::String textOf (const std::string& text)    { return juce::String (juce::CharPointer_UTF8 (text.c_str())); }

    bool sameFormula (const stella::licence::Formula& x, const stella::licence::Formula& y)
    {
        return x.a == y.a && x.b == y.b && x.m == y.m;
    }

    /** A protection's license.soni: a Creator project's (source: its folder, or its project.stella),
        or a license.soni file's. "" when it's read, else why not. */
    juce::String soniFrom (BlobMaker::Protection kind, const juce::File& source, std::vector<std::uint8_t>& bytes,
                           stella::licence::Soni& soni)
    {
        if (kind == BlobMaker::Protection::project)
        {
            const auto folder = source.isDirectory() ? source : source.getParentDirectory();
            const auto file = folder.getChildFile (Project::fileName);

            if (! file.existsAsFile())
                return "\"" + folder.getFileName() + "\" isn't a Plugins Creator Studio project (it has no " + Project::fileName + ").";

            const auto info = juce::JSON::parse (file);
            auto name = info.getProperty ("name", {}).toString().trim();
            const auto vendor = info.getProperty ("vendor", {}).toString().trim();

            if (name.isEmpty())
                name = folder.getFileName();

            const auto protection = Licences::loadProtection (folder);

            if (! protection.isValid())
                return name + " isn't protected: turn its protection on first (Plugins Creator Studio, Protection).";

            soni = Licences::soniFor (protection, name, vendor);
            const auto data = Licences::soniData (soni);
            bytes.assign (static_cast<const std::uint8_t*> (data.getData()), static_cast<const std::uint8_t*> (data.getData()) + data.getSize());
        }
        else
        {
            if (! source.existsAsFile())
                return source.getFullPathName() + " isn't there.";

            juce::MemoryBlock data;

            if (source.getSize() > 65536 || ! source.loadFileAsData (data) || ! Licences::readSoni (data.getData(), data.getSize(), soni))
                return source.getFileName() + " isn't a license.soni Sonify can read.";

            bytes.assign (static_cast<const std::uint8_t*> (data.getData()), static_cast<const std::uint8_t*> (data.getData()) + data.getSize());
        }

        if (! soni.isValid() || soni.product.size() >= 64)
            return "Its product's protection is damaged (its id or its formula).";

        return {};
    }
}

//==============================================================================
juce::String BlobMaker::nameOf (const juce::File& sfz)
{
    juce::FileInputStream in (sfz);

    for (int i = 0; i < 40 && in.openedOk() && ! in.isExhausted(); ++i)
    {
        const auto line = in.readNextLine();

        if (line.trimStart().startsWith ("// Family:"))
            if (const auto family = line.fromFirstOccurrenceOf ("// Family:", false, false).trim(); family.isNotEmpty())
                return family;
    }

    return sfz.getFileNameWithoutExtension();
}

juce::String BlobMaker::sfzText (const juce::File& sfz, juce::StringArray& notes)
{
    return withoutComments (withIncludes (sfz, sfz.getParentDirectory(), 0, notes));
}

BlobMaker::Result BlobMaker::convert (const juce::File& sfz, const juce::File& blob, Samples wanted,
                                      const std::function<bool (double)>& progress, const std::vector<std::uint8_t>& soni)
{
    Result result;

    if (! sfz.existsAsFile())
    {
        result.problem = sfz.getFileName() + " isn't there any more.";
        return result;
    }

    // Protected: locked to the product, its key from the product's formula; its license.soni goes
    // beside it (one folder, one product).
    const bool locked = ! soni.empty();
    const auto soniFile = blob.getSiblingFile (stella::licence::fileName);
    stella::licence::Soni product;
    std::uint8_t key[32] {};
    bool replacesFormula = false;

    if (locked)
    {
        if (! Licences::readSoni (soni.data(), soni.size(), product) || ! product.isValid() || product.product.size() >= 64)
        {
            result.problem = "The protection's license.soni can't be read: choose the protection again.";
            return result;
        }

        if (soniFile.existsAsFile())
        {
            stella::licence::Soni there;

            if (! Licences::readSoni (soniFile, there))
            {
                result.problem = soniFile.getFullPathName() + " isn't a license.soni Sonify can read: move it away, or make the blob in another folder.";
                return result;
            }

            if (there.product != product.product)
            {
                result.problem = "Its folder already has another product's license.soni (" + textOf (there.name.empty() ? there.product : there.name)
                               + "): one folder, one product. Make it in another folder.";
                return result;
            }

            replacesFormula = ! sameFormula (there.formula, product.formula);
        }

        stella::blob::licenceKey (product, key);
    }

    auto parsed = parse (sfz);
    result.name = parsed.name;
    result.notes = parsed.notes;

    // The samples, each once; what they are.
    struct SampleInfo
    {
        juce::File file;
        stella::blob::SampleEntry entry;
        juce::int64 loopStart = -1, loopEnd = -1;
        int unityNote = -1;
    };

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    const auto root = sfz.getParentDirectory();
    std::vector<SampleInfo> infos;
    std::map<juce::String, int> byPath;
    juce::StringArray missing, generated;
    std::vector<std::pair<int, Opcodes*>> used;   // each region kept: its sample

    for (auto& region : parsed.regions)
    {
        const auto* path = region.get ("sample");

        if (path == nullptr || path->empty())
            continue;

        const auto pathText = juce::String (juce::CharPointer_UTF8 (path->c_str()));

        if (pathText.startsWithChar ('*'))
        {
            generated.addIfNotAlreadyThere (pathText);   // *sine, *noise...: made by players, not a file
            continue;
        }

        const auto file = sampleFile (root, pathText);
        const auto key = file.getFullPathName();
        int index = -1;

        if (const auto found = byPath.find (key); found != byPath.end())
        {
            index = found->second;
        }
        else
        {
            std::unique_ptr<juce::AudioFormatReader> reader (file.existsAsFile() ? formats.createReaderFor (file) : nullptr);

            if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate < 1000.0 || reader->numChannels < 1)
            {
                missing.addIfNotAlreadyThere (pathText.replaceCharacter ('\\', '/').fromLastOccurrenceOf ("/", false, false));
                byPath[key] = -1;
                continue;
            }

            SampleInfo info;
            info.file = file;
            info.entry.name = file.getRelativePathFrom (root).replaceCharacter ('\\', '/').toStdString();
            info.entry.frames = reader->lengthInSamples;
            info.entry.rate = reader->sampleRate;
            info.entry.channels = reader->numChannels > 1 ? 2 : 1;
            info.entry.format = formatFor (*reader, wanted);

            const auto& meta = reader->metadataValues;

            if (meta.getValue ("NumSampleLoops", "0").getIntValue() > 0)
            {
                info.loopStart = meta.getValue ("Loop0Start", "-1").getLargeIntValue();
                info.loopEnd = meta.getValue ("Loop0End", "-1").getLargeIntValue();
            }

            if (meta.containsKey ("MidiUnityNote"))
                info.unityNote = meta.getValue ("MidiUnityNote", "60").getIntValue();

            index = (int) infos.size();
            byPath[key] = index;
            infos.push_back (std::move (info));
        }

        if (index >= 0)
            used.emplace_back (index, &region);
    }

    if (! missing.isEmpty())
        result.notes.add (samplesText (missing.size()) + " missing (" + missing.joinIntoString (", ").substring (0, 160) + ")");

    if (! generated.isEmpty())
        result.notes.add ("generated sounds left out (" + generated.joinIntoString (", ") + ")");

    if (used.empty())
    {
        result.problem = parsed.regions.empty() ? sfz.getFileName() + " has no regions."
                                                : "None of " + sfz.getFileName() + "'s samples can be read"
                                                      + (missing.isEmpty() ? juce::String (".") : " (" + missing.joinIntoString (", ").substring (0, 160) + ").");
        return result;
    }

    // The index: the samples, and every region with its opcodes (the WAV's own loop and key where
    // the SFZ says nothing of them).
    stella::blob::Index index;
    index.name = parsed.name.toStdString();
    juce::int64 totalFrames = 0;

    for (const auto& info : infos)
    {
        index.samples.push_back (info.entry);
        totalFrames += info.entry.frames;
    }

    for (const auto& [sampleIndex, region] : used)
    {
        const auto& info = infos[(size_t) sampleIndex];
        stella::blob::RegionEntry entry;
        entry.sample = sampleIndex;

        Opcodes opcodes = *region;

        if (info.loopEnd > info.loopStart && info.loopStart >= 0)
        {
            if (! opcodes.has ("loop_start") && ! opcodes.has ("loopstart") && ! opcodes.has ("loop_end") && ! opcodes.has ("loopend"))
            {
                opcodes.set ("loop_start", std::to_string (info.loopStart));
                opcodes.set ("loop_end", std::to_string (info.loopEnd));
            }

            if (! opcodes.has ("loop_mode") && ! opcodes.has ("loopmode"))
                opcodes.set ("loop_mode", "loop_continuous");
        }

        if (const auto* centre = opcodes.get ("pitch_keycenter"); centre != nullptr && *centre == "sample")
            opcodes.set ("pitch_keycenter", std::to_string (info.unityNote >= 0 ? info.unityNote : 60));

        for (const auto& [key, value] : opcodes.list)
            if (key != "sample")
                entry.opcodes.emplace_back (key, value);

        index.regions.push_back (std::move (entry));
    }

    // Written whole under another name first: a blob that's there is always complete.
    if (! blob.getParentDirectory().createDirectory())
    {
        result.problem = "Couldn't make the folder " + blob.getParentDirectory().getFullPathName();
        return result;
    }

    const auto part = blob.getSiblingFile (blob.getFileName() + ".part");
    stella::blob::Writer writer;
    std::string error;

    if (! writer.open (part.getFullPathName().toStdString(), index, error, locked ? key : nullptr, locked ? product.product : std::string()))
    {
        result.problem = juce::String (error);
        return result;
    }

    constexpr int chunk = 65536;
    juce::AudioBuffer<float> buffer (2, chunk);
    juce::int64 done = 0;

    for (const auto& info : infos)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (info.file));

        if (reader == nullptr)
        {
            writer.abandon();
            result.problem = info.file.getFileName() + " couldn't be read again.";
            return result;
        }

        const auto stereo = info.entry.channels > 1;

        for (juce::int64 position = 0; position < info.entry.frames; position += chunk)
        {
            const auto frames = (int) juce::jmin<juce::int64> (chunk, info.entry.frames - position);
            buffer.clear();
            reader->read (&buffer, 0, frames, position, true, stereo);

            if (! writer.write (buffer.getReadPointer (0), stereo ? buffer.getReadPointer (1) : nullptr, frames, error))
            {
                writer.abandon();
                result.problem = juce::String (error);
                return result;
            }

            done += frames;

            if (progress != nullptr && ! progress ((double) done / (double) juce::jmax ((juce::int64) 1, totalFrames)))
            {
                writer.abandon();
                result.problem = "Stopped.";
                return result;
            }
        }
    }

    if (! writer.finish (error))
    {
        part.deleteFile();
        result.problem = juce::String (error);
        return result;
    }

    // A protected blob's license.soni beside it (left as it is when it's the same file).
    if (locked)
    {
        juce::MemoryBlock there;

        if (! (soniFile.existsAsFile() && soniFile.loadFileAsData (there) && there.matches (soni.data(), soni.size()))
            && ! soniFile.replaceWithData (soni.data(), soni.size()))
        {
            part.deleteFile();
            result.problem = "Couldn't write its license.soni (" + soniFile.getFullPathName() + "): without it, it can't be played.";
            return result;
        }

        if (replacesFormula)
            result.notes.add ("the license.soni there was replaced: blobs made there with the product's old formula don't open any more");
    }

    if (blob.exists() && ! blob.deleteFile())
    {
        part.deleteFile();
        result.problem = "Couldn't replace " + blob.getFullPathName() + " (is it open somewhere?).";
        return result;
    }

    if (! part.moveFileTo (blob))
    {
        part.deleteFile();
        result.problem = "Couldn't write " + blob.getFullPathName();
        return result;
    }

    result.ok = true;
    result.locked = locked;
    result.regions = (int) index.regions.size();
    result.samples = (int) index.samples.size();
    result.bytes = blob.getSize();
    return result;
}

//==============================================================================
class BlobMaker::Worker final : public juce::Thread
{
public:
    explicit Worker (BlobMaker& m) : juce::Thread ("SFZ to Blob"), maker (m) {}

    void run() override
    {
        while (! threadShouldExit())
        {
            juce::File sfz, blob;
            Samples samples;
            std::vector<std::uint8_t> soni;
            int id = 0;

            {
                const juce::ScopedLock hold (maker.lock);

                // Stopped: none is taken (the rest keep waiting).
                if (! maker.stopping.load())
                {
                    for (auto& item : maker.items)
                    {
                        if (item.state == Item::State::waiting)
                        {
                            item.state = Item::State::working;
                            item.progress = 0.0;
                            item.message.clear();
                            item.blob = maker.blobFor (item.sfz);
                            sfz = item.sfz;
                            blob = item.blob;
                            id = item.id;
                            break;
                        }
                    }
                }

                samples = maker.samples;
                soni = maker.soni;
            }

            if (id == 0 || maker.stopping.load())
                break;

            maker.changed();
            auto lastReport = juce::Time::getMillisecondCounter();

            const auto result = convert (sfz, blob, samples, [this, id, &lastReport] (double progress)
            {
                if (threadShouldExit() || maker.stopping.load())
                    return false;

                {
                    const juce::ScopedLock hold (maker.lock);

                    for (auto& item : maker.items)
                        if (item.id == id)
                            item.progress = progress;
                }

                if (const auto now = juce::Time::getMillisecondCounter(); now - lastReport > 120)
                {
                    lastReport = now;
                    maker.changed();
                }

                return true;
            }, soni);

            {
                const juce::ScopedLock hold (maker.lock);

                for (auto& item : maker.items)
                {
                    if (item.id != id)
                        continue;

                    if (! result.ok && maker.stopping.load())
                    {
                        item.state = Item::State::waiting;   // stopped: it waits again
                        item.progress = 0.0;
                    }
                    else
                    {
                        item.state = result.ok ? Item::State::done : Item::State::failed;
                        item.progress = result.ok ? 1.0 : 0.0;
                        item.message = result.ok ? result.notes.joinIntoString ("; ") : result.problem;
                        item.name = result.name.isNotEmpty() ? result.name : item.name;
                        item.regions = result.regions;
                        item.samples = result.samples;
                        item.bytes = result.bytes;
                        item.locked = result.locked;
                    }
                }
            }

            maker.changed();
        }

        {
            // One taken just as it was stopped waits again (nothing else would ever take it).
            const juce::ScopedLock hold (maker.lock);

            for (auto& item : maker.items)
            {
                if (item.state == Item::State::working)
                {
                    item.state = Item::State::waiting;
                    item.progress = 0.0;
                }
            }
        }

        maker.running.store (false);
        maker.changed();
    }

private:
    BlobMaker& maker;
};

//==============================================================================
BlobMaker::BlobMaker (const juce::File& kept)
    : keptFile (kept)
{
    const auto saved = juce::JSON::parse (keptFile);

    if (saved.isObject())
    {
        beside = (bool) saved.getProperty ("beside", true);

        if (const auto path = saved.getProperty ("folder", {}).toString(); juce::File::isAbsolutePath (path))
            folder = juce::File (path);

        const auto kind = saved.getProperty ("samples", "keep").toString();
        samples = kind == "s16" ? Samples::s16 : kind == "s24" ? Samples::s24 : kind == "f32" ? Samples::f32 : Samples::asTheyAre;

        const auto protectionKind = saved.getProperty ("protection", "none").toString();
        const auto source = saved.getProperty ("protectionSource", {}).toString();

        if ((protectionKind == "project" || protectionKind == "file") && juce::File::isAbsolutePath (source))
        {
            protection = protectionKind == "project" ? Protection::project : Protection::soniFile;
            protectionSource = juce::File (source);
            readProtection();   // what it says now (gone or changed: its problem, shown on the page)
        }
    }
}

BlobMaker::~BlobMaker()
{
    alive->store (false);
    stopping.store (true);

    if (worker != nullptr)
        worker->stopThread (20000);
}

void BlobMaker::changed()
{
    if (changePending.exchange (true))
        return;

    juce::MessageManager::callAsync ([this, stillAlive = alive]
    {
        if (! stillAlive->load())
            return;

        changePending.store (false);

        if (onChange != nullptr)
            onChange();
    });
}

void BlobMaker::keep() const
{
    juce::DynamicObject::Ptr saved (new juce::DynamicObject());
    saved->setProperty ("beside", beside);
    saved->setProperty ("folder", folder.getFullPathName());
    saved->setProperty ("samples", samples == Samples::s16 ? "s16" : samples == Samples::s24 ? "s24" : samples == Samples::f32 ? "f32" : "keep");
    saved->setProperty ("protection", protection == Protection::project ? "project" : protection == Protection::soniFile ? "file" : "none");
    saved->setProperty ("protectionSource", protectionSource.getFullPathName());
    keptFile.replaceWithText (juce::JSON::toString (juce::var (saved.get())));
}

juce::File BlobMaker::blobFor (const juce::File& sfz) const
{
    const auto where = beside || ! folder.isDirectory() ? sfz.getParentDirectory() : folder;
    return where.getChildFile (sfz.getFileNameWithoutExtension() + stella::blob::fileExtension);
}

void BlobMaker::add (const juce::Array<juce::File>& files)
{
    juce::Array<juce::File> found;

    for (const auto& file : files)
    {
        if (file.isDirectory())
        {
            auto inside = file.findChildFiles (juce::File::findFiles, true, "*.sfz");
            inside.sort();
            found.addArray (inside);
        }
        else if (file.hasFileExtension ("sfz"))
        {
            found.add (file);
        }
    }

    {
        const juce::ScopedLock hold (lock);

        for (const auto& sfz : found)
        {
            bool listed = false;

            for (auto& item : items)
            {
                if (item.sfz == sfz)
                {
                    listed = true;

                    if (item.state != Item::State::working)
                    {
                        item.state = Item::State::waiting;
                        item.progress = 0.0;
                        item.message.clear();
                    }
                }
            }

            if (! listed && items.size() < 2000)
            {
                Item item;
                item.id = nextId++;
                item.sfz = sfz;
                item.name = nameOf (sfz);
                items.push_back (item);
            }
        }
    }

    changed();
}

void BlobMaker::remove (int id)
{
    {
        const juce::ScopedLock hold (lock);
        items.erase (std::remove_if (items.begin(), items.end(),
                                     [id] (const Item& item) { return item.id == id && item.state != Item::State::working; }),
                     items.end());
    }

    changed();
}

void BlobMaker::clear()
{
    {
        const juce::ScopedLock hold (lock);
        items.erase (std::remove_if (items.begin(), items.end(), [] (const Item& item) { return item.state != Item::State::working; }),
                     items.end());
    }

    changed();
}

void BlobMaker::setBeside (bool besideEachSfz)
{
    {
        const juce::ScopedLock hold (lock);   // the worker reads them
        beside = besideEachSfz;
    }

    keep();
    changed();
}

void BlobMaker::setFolder (const juce::File& newFolder)
{
    {
        const juce::ScopedLock hold (lock);
        folder = newFolder;
        beside = false;
    }

    keep();
    changed();
}

void BlobMaker::setSamples (Samples newSamples)
{
    {
        const juce::ScopedLock hold (lock);
        samples = newSamples;
    }

    keep();
    changed();
}

juce::String BlobMaker::readProtection()
{
    std::vector<std::uint8_t> bytes;
    stella::licence::Soni read;
    const auto problem = protection == Protection::none ? juce::String() : soniFrom (protection, protectionSource, bytes, read);
    const juce::ScopedLock hold (lock);

    soni = problem.isEmpty() ? std::move (bytes) : std::vector<std::uint8_t>();
    protectionProblem = problem;
    protectionProduct = textOf (read.product);
    protectionName = textOf (read.name);
    protectionVendor = textOf (read.vendor);
    return problem;
}

juce::String BlobMaker::setProtection (Protection kind, const juce::File& source)
{
    if (running.load())
        return "Wait until the blobs being made are done.";

    std::vector<std::uint8_t> bytes;
    stella::licence::Soni read;

    if (kind != Protection::none)
        if (const auto problem = soniFrom (kind, source, bytes, read); problem.isNotEmpty())
            return problem;   // it stays as it was

    {
        const juce::ScopedLock hold (lock);
        protection = kind;
        protectionSource = kind == Protection::project && source.existsAsFile() ? source.getParentDirectory()
                         : kind == Protection::none ? juce::File() : source;
        soni = std::move (bytes);
        protectionProblem.clear();
        protectionProduct = textOf (read.product);
        protectionName = textOf (read.name);
        protectionVendor = textOf (read.vendor);
    }

    keep();
    changed();
    return {};
}

juce::String BlobMaker::start()
{
    if (running.load())
        return {};

    // The protection as it is now (a project's may have changed since it was chosen).
    if (protection != Protection::none)
    {
        if (const auto problem = readProtection(); problem.isNotEmpty())
        {
            changed();
            return "The blobs can't be protected: " + problem;
        }
    }

    if (worker != nullptr)
        worker->stopThread (20000);

    stopping.store (false);
    running.store (true);
    worker = std::make_unique<Worker> (*this);
    worker->startThread();
    changed();
    return {};
}

void BlobMaker::stop()
{
    stopping.store (true);
    changed();
}

juce::var BlobMaker::toVar() const
{
    juce::DynamicObject::Ptr state (new juce::DynamicObject());
    juce::Array<juce::var> list;

    {
        const juce::ScopedLock hold (lock);

        for (const auto& item : items)
        {
            juce::DynamicObject::Ptr row (new juce::DynamicObject());
            row->setProperty ("id", item.id);
            row->setProperty ("name", item.name);
            row->setProperty ("sfz", item.sfz.getFullPathName());
            row->setProperty ("blob", item.state == Item::State::waiting ? blobFor (item.sfz).getFullPathName() : item.blob.getFullPathName());
            row->setProperty ("state", item.state == Item::State::waiting ? "waiting" : item.state == Item::State::working ? "working"
                                     : item.state == Item::State::done ? "done" : "failed");
            row->setProperty ("progress", item.progress);
            row->setProperty ("message", item.message);
            row->setProperty ("regions", item.regions);
            row->setProperty ("samples", item.samples);
            row->setProperty ("bytes", (double) item.bytes);
            row->setProperty ("locked", item.locked);
            list.add (juce::var (row.get()));
        }
    }

    juce::DynamicObject::Ptr guard (new juce::DynamicObject());

    {
        const juce::ScopedLock hold (lock);
        guard->setProperty ("kind", protection == Protection::project ? "project" : protection == Protection::soniFile ? "file" : "none");
        guard->setProperty ("source", protectionSource.getFullPathName());
        guard->setProperty ("product", protectionProduct);
        guard->setProperty ("name", protectionName);
        guard->setProperty ("vendor", protectionVendor);
        guard->setProperty ("problem", protectionProblem);
    }

    state->setProperty ("kind", "blob");
    state->setProperty ("items", list);
    state->setProperty ("beside", beside);
    state->setProperty ("folder", folder.getFullPathName());
    state->setProperty ("samples", samples == Samples::s16 ? "s16" : samples == Samples::s24 ? "s24" : samples == Samples::f32 ? "f32" : "keep");
    state->setProperty ("protection", juce::var (guard.get()));
    state->setProperty ("running", running.load() && ! stopping.load());
    return juce::var (state.get());
}
