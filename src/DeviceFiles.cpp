// D:\workspace\Fanan_web_hub\src\DeviceFiles.cpp

#include "DeviceFiles.h"
#include "SipFiles.h"
#include "SonifyAiData.h"

#include <juce_cryptography/juce_cryptography.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <string>
#include <thread>

namespace Devices
{
namespace
{
    constexpr size_t maxWasmSize = 16 * 1024 * 1024;
    constexpr size_t maxChatSize = 4 * 1024 * 1024;
    constexpr int maxEntries = 32;
    constexpr int compileTimeoutMs = 120000;
    constexpr size_t maxLicenceSize = 64 * 1024;

    juce::File& configuredFolder()
    {
        static juce::File folder;
        return folder;
    }

    /** An entry of the zip as text or bytes, when it's not too big. */
    bool readEntry (juce::ZipFile& zip, int index, size_t limit, juce::MemoryBlock& into)
    {
        const auto* entry = zip.getEntry (index);

        if (entry == nullptr || entry->uncompressedSize < 0 || static_cast<size_t> (entry->uncompressedSize) > limit)
            return false;

        std::unique_ptr<juce::InputStream> stream (zip.createStreamForEntry (index));

        if (stream == nullptr)
            return false;

        into.reset();
        juce::MemoryOutputStream out (into, false);
        const auto copied = out.writeFromInputStream (*stream, static_cast<juce::int64> (limit) + 1);
        out.flush();
        return copied >= 0 && static_cast<size_t> (copied) <= limit;
    }

    juce::String asText (const juce::MemoryBlock& data)
    {
        if (! juce::CharPointer_UTF8::isValidString (static_cast<const char*> (data.getData()), static_cast<int> (data.getSize())))
            return {};

        return juce::String::fromUTF8 (static_cast<const char*> (data.getData()), static_cast<int> (data.getSize()));
    }

    juce::String cleanName (const juce::String& name)
    {
        auto clean = juce::File::createLegalFileName (name.trim()).trim().substring (0, 60);
        return clean.isNotEmpty() ? clean : juce::String ("Device");
    }
}

//==============================================================================
DeviceFile DeviceFile::make (const juce::String& name)
{
    DeviceFile file;
    file.id = juce::Uuid().toDashedString();
    file.name = name.trim().isNotEmpty() ? name.trim().substring (0, 60) : juce::String ("New Device");
    return file;
}

bool DeviceFile::isValidSourceName (const juce::String& name)
{
    if (name.length() < 3 || name.length() > 40 || name.equalsIgnoreCase ("sonify_device.h"))
        return false;

    if (! (name.endsWith (".cpp") || name.endsWith (".h")))
        return false;

    return name.containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.")
        && name.indexOfChar ('.') == name.lastIndexOfChar ('.')
        && ! name.startsWithChar ('.');
}

juce::String DeviceFile::fingerprint() const
{
    juce::MemoryOutputStream all;

    for (const auto& [fileName, text] : sources)   // std::map: in order of name
    {
        all << fileName;
        all.writeByte (0);
        all << text;
        all.writeByte (0);
    }

    return juce::SHA256 (all.getData(), all.getDataSize()).toHexString();
}

juce::Result DeviceFile::read (const juce::File& file)
{
    juce::ZipFile zip (file);

    if (zip.getNumEntries() == 0 || zip.getNumEntries() > maxEntries)
        return juce::Result::fail (file.getFileName() + " isn't a Sonify device.");

    const auto manifestIndex = zip.getIndexOfFileName ("device.json");
    juce::MemoryBlock data;

    if (manifestIndex < 0 || ! readEntry (zip, manifestIndex, 65536, data))
        return juce::Result::fail (file.getFileName() + " isn't a Sonify device.");

    const auto manifest = juce::JSON::parse (asText (data));

    if (! manifest.isObject() || static_cast<int> (manifest.getProperty ("format", 0)) != 1)
        return juce::Result::fail (file.getFileName() + " isn't a Sonify device this Sonify can read.");

    DeviceFile loaded;
    loaded.id = manifest.getProperty ("id", {}).toString().substring (0, 80);
    loaded.name = manifest.getProperty ("name", "Device").toString().substring (0, 60);
    loaded.description = manifest.getProperty ("description", {}).toString().substring (0, 2000);
    loaded.vendor = manifest.getProperty ("vendor", {}).toString().substring (0, 60);
    loaded.version = juce::jmax (0, static_cast<int> (manifest.getProperty ("version", 0)));
    loaded.builtFrom = manifest.getProperty ("builtFrom", {}).toString().substring (0, 64);

    if (loaded.id.isEmpty())
        return juce::Result::fail (file.getFileName() + " isn't a Sonify device (it has no id).");

    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        const auto path = zip.getEntry (i)->filename;

        if (path.startsWith ("source/"))
        {
            const auto fileName = path.fromFirstOccurrenceOf ("source/", false, false);

            if (! isValidSourceName (fileName) || static_cast<int> (loaded.sources.size()) >= maxSources)
                continue;

            if (! readEntry (zip, i, static_cast<size_t> (maxSourceSize) * 4, data))
                return juce::Result::fail ("Part of " + file.getFileName() + " couldn't be read.");

            loaded.sources[fileName] = asText (data).substring (0, maxSourceSize);
        }
        else if (path == "module.wasm")
        {
            if (! readEntry (zip, i, maxWasmSize, data))
                return juce::Result::fail ("Part of " + file.getFileName() + " couldn't be read.");

            loaded.wasm = data;
        }
        else if (path == "chat.json")
        {
            if (readEntry (zip, i, maxChatSize, data))
                loaded.chat = juce::JSON::parse (asText (data));
        }
        else if (path == stella::licence::fileName)
        {
            // A protected one's: kept only when it's a license.soni (a damaged one protects nothing).
            stella::licence::Soni soni;

            if (readEntry (zip, i, maxLicenceSize, data) && Licences::readSoni (data.getData(), data.getSize(), soni))
                loaded.licence = data;
        }
    }

    *this = std::move (loaded);
    return juce::Result::ok();
}

juce::Result DeviceFile::readInfo (const juce::File& file, Info& info)
{
    juce::ZipFile zip (file);

    if (zip.getNumEntries() == 0 || zip.getNumEntries() > maxEntries)
        return juce::Result::fail (file.getFileName() + " isn't a Sonify device.");

    const auto manifestIndex = zip.getIndexOfFileName ("device.json");
    juce::MemoryBlock data;

    if (manifestIndex < 0 || ! readEntry (zip, manifestIndex, 65536, data))
        return juce::Result::fail (file.getFileName() + " isn't a Sonify device.");

    const auto manifest = juce::JSON::parse (asText (data));

    if (! manifest.isObject() || static_cast<int> (manifest.getProperty ("format", 0)) != 1)
        return juce::Result::fail (file.getFileName() + " isn't a Sonify device this Sonify can read.");

    info.id = manifest.getProperty ("id", {}).toString().substring (0, 80);
    info.name = manifest.getProperty ("name", "Device").toString().substring (0, 60);
    info.description = manifest.getProperty ("description", {}).toString().substring (0, 2000);
    info.version = juce::jmax (0, static_cast<int> (manifest.getProperty ("version", 0)));
    info.builtFrom = manifest.getProperty ("builtFrom", {}).toString().substring (0, 64);
    info.built = zip.getIndexOfFileName ("module.wasm") >= 0;
    info.locked = zip.getIndexOfFileName (stella::licence::fileName) >= 0 && ! isMakerOf (info.id);
    return info.id.isNotEmpty() ? juce::Result::ok() : juce::Result::fail (file.getFileName() + " isn't a Sonify device (it has no id).");
}

juce::Result DeviceFile::write (const juce::File& file) const
{
    juce::DynamicObject::Ptr manifest (new juce::DynamicObject());
    manifest->setProperty ("format", 1);
    manifest->setProperty ("kind", "midi");
    manifest->setProperty ("id", id);
    manifest->setProperty ("name", name);
    manifest->setProperty ("description", description);
    manifest->setProperty ("vendor", vendor);
    manifest->setProperty ("version", version);
    manifest->setProperty ("builtFrom", builtFrom);
    manifest->setProperty ("app", "Sonify");

    juce::ZipFile::Builder zip;
    const auto now = juce::Time::getCurrentTime();

    const auto addText = [&zip, &now] (const juce::String& path, const juce::String& text)
    {
        const auto utf8 = text.toUTF8();
        zip.addEntry (std::make_unique<juce::MemoryInputStream> (utf8.getAddress(), utf8.sizeInBytes() - 1, true), 9, path, now);
    };

    // What kind of Sonify plugin it is, first: a MIDI FX.
    Sip::Manifest sip;
    sip.kind = Sip::Kind::midi;
    sip.id = id;
    sip.name = name;
    sip.vendor = vendor;
    sip.version = version;
    addText (Sip::manifestName, Sip::manifestText (sip));

    addText ("device.json", juce::JSON::toString (juce::var (manifest.get())));

    for (const auto& [fileName, text] : sources)
        addText ("source/" + fileName, text);

    if (! wasm.isEmpty())
        zip.addEntry (std::make_unique<juce::MemoryInputStream> (wasm, true), 9, "module.wasm", now);

    if (! chat.isVoid())
        addText ("chat.json", juce::JSON::toString (chat, true));

    if (const auto soni = licenceToWrite(); ! soni.isEmpty())
        zip.addEntry (std::make_unique<juce::MemoryInputStream> (soni, true), 0, stella::licence::fileName, now);

    if (! file.getParentDirectory().createDirectory())
        return juce::Result::fail ("Couldn't make the folder " + file.getParentDirectory().getFullPathName() + ".");

    juce::TemporaryFile temp (file);

    {
        juce::FileOutputStream out (temp.getFile());

        if (! out.openedOk() || ! zip.writeToStream (out, nullptr))
            return juce::Result::fail ("Couldn't save " + file.getFileName() + ".");

        out.flush();

        if (out.getStatus().failed())
            return juce::Result::fail ("Couldn't save " + file.getFileName() + ": " + out.getStatus().getErrorMessage());
    }

    if (! temp.overwriteTargetFileWithTemporary())
        return juce::Result::fail ("Couldn't save " + file.getFileName() + ".");

    return juce::Result::ok();
}

juce::MemoryBlock DeviceFile::licenceToWrite() const
{
    // Its maker's computer: from its protection as it is now.
    if (isMakerOf (id))
    {
        const auto protection = Licences::loadProtection (protectionFolderOf (id));
        return protection.isValid() ? Licences::soniData (Licences::soniFor (protection, name, vendor)) : juce::MemoryBlock();
    }

    return licence;
}

bool DeviceFile::isLocked() const
{
    return ! licence.isEmpty() && ! isMakerOf (id);
}

//==============================================================================
void setMyDevicesFolder (const juce::File& folder)
{
    configuredFolder() = folder;
}

juce::File myDevicesFolder()
{
    auto folder = configuredFolder();

    if (folder == juce::File())
        folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Sonify").getChildFile ("Devices");

    return folder;
}

juce::File devicesFolder()
{
    return Sip::folder();
}

bool isDeviceFile (const juce::File& file)
{
    if (file.hasFileExtension (oldFileExtension))
        return file.existsAsFile();

    Sip::Manifest manifest;
    return Sip::isSipFile (file) && Sip::readManifest (file, manifest) && manifest.kind == Sip::Kind::midi;
}

juce::Array<juce::File> listMyDevices()
{
    return Sip::list (Sip::Kind::midi);
}

juce::File freeFileFor (const juce::String& name, const juce::File& except)
{
    return Sip::freeFile (cleanName (name), except);
}

juce::File protectionFolderOf (const juce::String& deviceId)
{
    return myDevicesFolder().getChildFile ("Protection").getChildFile (juce::File::createLegalFileName (deviceId.trim()));
}

bool isMakerOf (const juce::String& deviceId)
{
    return deviceId.trim().isNotEmpty() && Licences::hasProtection (protectionFolderOf (deviceId));
}

//==============================================================================
juce::File Compiler::folder()
{
    if (const auto custom = juce::SystemStats::getEnvironmentVariable ("SONIFY_DEVICE_COMPILER", {}); custom.isNotEmpty())
        return juce::File (custom);

   #if JUCE_MAC
    // In the app's Resources (Contents/MacOS is for the app itself).
    return juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory()
               .getSiblingFile ("Resources").getChildFile ("Device Compiler");
   #else
    return juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory().getChildFile ("Device Compiler");
   #endif
}

juce::File Compiler::libcxxHeaders (const juce::File& folder)
{
    return folder.getChildFile ("share").getChildFile ("wasi-sysroot").getChildFile ("include")
                 .getChildFile ("wasm32-wasip1").getChildFile ("c++").getChildFile ("v1");
}

namespace
{
    juce::File clangIn (const juce::File& folder)
    {
       #if JUCE_WINDOWS
        return folder.getChildFile ("bin").getChildFile ("clang.exe");
       #else
        return folder.getChildFile ("bin").getChildFile ("clang");
       #endif
    }

    /** One run of the compiler: what it said and how it ended. */
    struct Step
    {
        bool stopped = false, timedOut = false, started = false;
        int exitCode = -1;
        juce::String messages;
    };

    Step runStep (const juce::StringArray& args, std::atomic<bool>* cancelled, double since)
    {
        Step step;
        juce::ChildProcess process;

        if (! process.start (args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
            return step;

        step.started = true;

        // Reading its messages waits for them, so a watch of its own stops it when it's asked to
        // stop or takes too long (the reading then ends, as it does when the compiler exits).
        std::atomic<bool> finished { false }, stopped { false }, timedOut { false };

        std::thread watch ([&]
        {
            while (! finished.load())
            {
                if (cancelled != nullptr && cancelled->load())
                    stopped.store (true);
                else if (juce::Time::getMillisecondCounterHiRes() - since > compileTimeoutMs)
                    timedOut.store (true);

                if (stopped.load() || timedOut.load())
                {
                    process.kill();
                    return;
                }

                std::this_thread::sleep_for (std::chrono::milliseconds (10));
            }
        });

        juce::MemoryOutputStream messages;
        char buffer[4096];

        for (;;)
        {
            const auto read = process.readProcessOutput (buffer, sizeof (buffer));

            if (read > 0)
            {
                if (messages.getDataSize() < 64 * 1024)
                    messages.write (buffer, static_cast<size_t> (read));

                continue;
            }

            if (! process.isRunning() || stopped.load() || timedOut.load())
                break;

            juce::Thread::sleep (5);
        }

        finished.store (true);
        watch.join();

        step.stopped = stopped.load();
        step.timedOut = timedOut.load();
        step.exitCode = step.stopped || step.timedOut ? -1 : static_cast<int> (process.getExitCode());
        step.messages = messages.toString();
        return step;
    }

    /** A source as the compiler reads its directives: lines joined where they end in \, comments
        gone, %: read as #. */
    juce::String directivesOf (const juce::String& text)
    {
        const auto in = text.toStdString();
        std::string out;
        out.reserve (in.size());

        enum class In { code, lineComment, blockComment, quoted } state = In::code;
        char quote = 0;

        for (size_t i = 0; i < in.size(); ++i)
        {
            const auto c = in[i];
            const auto next = i + 1 < in.size() ? in[i + 1] : '\0';

            if (c == '\\' && (next == '\n' || (next == '\r' && i + 2 < in.size() && in[i + 2] == '\n')))
            {
                i += next == '\r' ? 2 : 1;
                continue;
            }

            switch (state)
            {
                case In::code:
                    if (c == '/' && next == '/')            { state = In::lineComment; out += ' '; ++i; }
                    else if (c == '/' && next == '*')       { state = In::blockComment; out += ' '; ++i; }
                    else if (c == '%' && next == ':')       { out += '#'; ++i; }
                    else if (c == '"' || (c == '\'' && (out.empty() || ! std::isalnum (static_cast<unsigned char> (out.back())))))
                                                            { state = In::quoted; quote = c; out += c; }
                    else                                    { out += c; }
                    break;

                case In::lineComment:
                    if (c == '\n')                          { state = In::code; out += c; }
                    break;

                case In::blockComment:
                    if (c == '*' && next == '/')            { state = In::code; ++i; }
                    else if (c == '\n')                     { out += c; }
                    break;

                case In::quoted:
                    out += c;

                    if (c == '\\' && next != '\0')          { out += next; ++i; }
                    else if (c == quote || c == '\n')       { state = In::code; }
                    break;
            }
        }

        return juce::String::fromUTF8 (out.data(), static_cast<int> (out.size()));
    }

    /** What's named in an include ("a.h" or <a.h>) after its keyword; "" when it isn't written
        out (made by a macro). */
    juce::String includeTarget (const juce::String& afterKeyword)
    {
        const auto quote = afterKeyword.indexOfChar ('"');
        const auto angle = afterKeyword.indexOfChar ('<');

        if (quote >= 0 && (angle < 0 || quote < angle))
            return afterKeyword.substring (quote + 1).upToFirstOccurrenceOf ("\"", false, false);

        if (angle >= 0)
            return afterKeyword.substring (angle + 1).upToFirstOccurrenceOf (">", false, false);

        return {};
    }

    bool isOwnOrStandard (const juce::String& target)
    {
        return target.isNotEmpty() && ! target.startsWithChar ('/') && ! target.containsChar ('\\')
            && ! target.containsChar (':') && ! target.contains ("..");
    }

    /** The files a compiler's dependency file lists (make's format: spaces escaped, lines joined). */
    juce::StringArray filesInDepFile (const juce::String& text)
    {
        juce::StringArray files;
        const auto body = text.fromFirstOccurrenceOf (":", false, false).toStdString();   // after "deps:"
        std::string current;

        auto finish = [&]
        {
            if (! current.empty())
                files.add (juce::String::fromUTF8 (current.data(), static_cast<int> (current.size())));

            current.clear();
        };

        for (size_t i = 0; i < body.size(); ++i)
        {
            const auto c = body[i];
            const auto next = i + 1 < body.size() ? body[i + 1] : '\0';

            if (c == '\\' && (next == '\n' || next == '\r'))
            {
                finish();
                ++i;
            }
            else if (c == '\\' && (next == ' ' || next == '#'))
            {
                current += next;
                ++i;
            }
            else if (c == '$' && next == '$')
            {
                current += '$';
                ++i;
            }
            else if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            {
                finish();
            }
            else
            {
                current += c;
            }
        }

        finish();
        return files;
    }

    /** A path with its . and .. worked out and / between its parts (case left out where the
        computer's files ignore it), to compare. */
    juce::String comparable (const juce::File& base, juce::String path)
    {
        path = path.replaceCharacter ('\\', '/');

        const auto absolute = path.startsWithChar ('/') || (path.length() > 1 && path[1] == ':');

        if (! absolute)
            path = base.getFullPathName().replaceCharacter ('\\', '/') + "/" + path;

        juce::StringArray parts;
        parts.addTokens (path, "/", {});
        juce::StringArray kept;

        for (const auto& part : parts)
        {
            if (part.isEmpty() || part == ".")
                continue;

            if (part == "..")
            {
                if (kept.size() > 0)
                    kept.remove (kept.size() - 1);
            }
            else
            {
                kept.add (part);
            }
        }

        auto result = "/" + kept.joinIntoString ("/");

       #if JUCE_WINDOWS || JUCE_MAC
        result = result.toLowerCase();
       #endif

        return result;
    }

    bool isWithin (const juce::String& path, const juce::String& folder)
    {
        return path.startsWith (folder + "/");
    }
}

bool Compiler::isAvailable (juce::String* why)
{
    const auto f = folder();
    const auto ok = clangIn (f).existsAsFile() && f.getChildFile ("share").getChildFile ("wasi-sysroot").isDirectory();

    if (why != nullptr)
        *why = ok ? juce::String() : juce::String ("The device compiler isn't next to Sonify (its \"Device Compiler\" folder), so devices can't be built here. Devices already built still play.");

    return ok;
}

juce::String Compiler::apiHeader()
{
    return juce::String::fromUTF8 (SonifyAiData::sonify_device_h, SonifyAiData::sonify_device_hSize);
}

Compiler::Result Compiler::compile (const std::map<juce::String, juce::String>& sources, std::atomic<bool>* cancelled)
{
    Result result;
    const auto started = juce::Time::getMillisecondCounterHiRes();
    juce::String why;

    if (! isAvailable (&why))
    {
        result.log = why;
        return result;
    }

    bool anyCpp = false;

    for (const auto& [fileName, text] : sources)
        anyCpp = anyCpp || fileName.endsWith (".cpp");

    if (! anyCpp)
    {
        result.log = "There's no .cpp file to build.";
        return result;
    }

    // A device's code includes its own files and the standard library's headers, nothing else
    // of the computer's (its files would travel in the device), and embeds nothing. Said plainly
    // here; what holds it is the compiler's own list of what it read, checked after.
    for (const auto& [fileName, text] : sources)
    {
        for (auto line : juce::StringArray::fromLines (directivesOf (text)))
        {
            line = line.trim();

            if (line.contains ("__has_embed") || line.contains ("incbin")
                 || (line.startsWithChar ('#') && line.substring (1).trimStart().startsWith ("embed")))
            {
                result.log = fileName + ": devices can't embed files.";
                return result;
            }

            juce::String target;
            bool bad = false;

            if (line.startsWithChar ('#'))
            {
                const auto directive = line.substring (1).trimStart();

                for (auto keyword : { "include_next", "include", "import" })
                {
                    if (directive.startsWith (keyword))
                    {
                        target = includeTarget (directive.substring (juce::String (keyword).length()));
                        bad = ! isOwnOrStandard (target);
                        break;
                    }
                }
            }

            for (int at = line.indexOf ("__has_include"); at >= 0 && ! bad; at = line.indexOf (at + 1, "__has_include"))
            {
                target = includeTarget (line.substring (at).fromFirstOccurrenceOf ("(", false, false));
                bad = ! isOwnOrStandard (target);
            }

            if (bad)
            {
                result.log = fileName + ": devices include only their own files and the standard headers, not "
                           + (target.isEmpty() ? juce::String ("a name made by a macro") : "\"" + target + "\"") + ".";
                return result;
            }
        }
    }

    // A folder of its own for each build, gone afterwards.
    const auto work = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("Sonify Device Build " + juce::Uuid().toString().substring (0, 12));

    struct Cleanup { juce::File folder; ~Cleanup() { folder.deleteRecursively(); } } cleanup { work };

    if (! work.createDirectory() || ! work.getChildFile ("sonify_device.h").replaceWithText (apiHeader(), false, false, "\n"))
    {
        result.log = "Couldn't make a folder to build in.";
        return result;
    }

    juce::Array<juce::File> units;

    for (const auto& [fileName, text] : sources)
    {
        if (! DeviceFile::isValidSourceName (fileName))
            continue;

        const auto file = work.getChildFile (fileName);

        if (! file.replaceWithText (text, false, false, "\n"))
        {
            result.log = "Couldn't write " + fileName + " to build it.";
            return result;
        }

        if (fileName.endsWith (".cpp"))
            units.add (file);
    }

    const auto tools = folder();
    const auto output = work.getChildFile ("module.wasm");
    juce::StringArray common {
        clangIn (tools).getFullPathName(),
        "--driver-mode=g++", "--target=wasm32-wasip1",
        "--sysroot=" + tools.getChildFile ("share").getChildFile ("wasi-sysroot").getFullPathName(),
        "-O2"
    };

    // The C++ library's headers (<cmath>, <vector>...), named here: clang finds them on its own only
    // through an empty include/c++/v1 folder, and Sonify's installer leaves empty folders out.
    if (const auto libcxx = libcxxHeaders (tools); libcxx.isDirectory())
        { common.add ("-stdlib++-isystem"); common.add (libcxx.getFullPathName()); }

    // Its own headers named from the folder as it's given here (found on its own, a link on the
    // way would be followed, and they'd seem to be from elsewhere).
    if (const auto versions = tools.getChildFile ("lib").getChildFile ("clang").findChildFiles (juce::File::findDirectories, false); ! versions.isEmpty())
        common.add ("-resource-dir=" + versions.getFirst().getFullPathName());

    // The compiler's messages, without its folders' paths.
    auto clean = [&] (juce::String log)
    {
        for (const auto& [folderPath, shownAs] : { std::pair { work.getFullPathName(), juce::String (".") },
                                                   std::pair { tools.getFullPathName(), juce::String ("compiler") } })
        {
            const auto prefix = folderPath + juce::File::getSeparatorString();
            log = log.replace (prefix, {}).replace (prefix.replaceCharacter ('\\', '/'), {})
                     .replace (folderPath, shownAs).replace (folderPath.replaceCharacter ('\\', '/'), shownAs);
        }

        return log;
    };

    auto ended = [&] (const Step& step)
    {
        if (! step.started)
            result.log = "The device compiler couldn't start.";
        else if (step.stopped || step.timedOut)
            result.log = step.stopped ? "Stopped." : "The build took too long and was stopped.";
        else
            return false;

        return true;
    };

    const auto ownFolder = comparable (work, work.getFullPathName());
    const auto toolsFolder = comparable (work, tools.getFullPathName());
    juce::StringArray objects;
    juce::String log;
    bool compiled = true;

    // Each file on its own (the compiler working in its own process, so stopping it stops it),
    // with inline assembly off: assembly reads files of its own.
    for (const auto& unit : units)
    {
        const auto object = work.getChildFile (unit.getFileNameWithoutExtension() + ".o");
        const auto depFile = work.getChildFile (unit.getFileNameWithoutExtension() + ".d");

        auto args = common;
        args.addArray (juce::StringArray {
            "-std=c++20", "-fno-exceptions", "-fno-rtti", "-fintegrated-cc1", "-fno-gnu-inline-asm",
            "-Wall", "-Wno-unused-parameter", "-Wno-unused-private-field", "-ferror-limit=12", "-fno-color-diagnostics",
            "-ffile-prefix-map=" + work.getFullPathName() + "=.",
            "-ffile-prefix-map=" + tools.getFullPathName() + "=compiler",
            "-I" + work.getFullPathName(),
            "-MD", "-MT", "deps", "-MF", depFile.getFullPathName(),
            "-c", unit.getFullPathName(), "-o", object.getFullPathName()
        });

        const auto step = runStep (args, cancelled, started);

        if (ended (step))
            return result;

        // Everything it read is the device's own or the compiler's (checked first: its messages
        // could show what it read).
        for (const auto& read : filesInDepFile (depFile.loadFileAsString()))
        {
            const auto path = comparable (work, read);

            if (! isWithin (path, ownFolder) && ! isWithin (path, toolsFolder))
            {
                result.log = unit.getFileName() + ": devices include only their own files and the standard headers, not \""
                           + read.replaceCharacter ('\\', '/').fromLastOccurrenceOf ("/", false, false) + "\".";
                return result;
            }
        }

        log += step.messages;

        if (step.exitCode != 0 || ! object.existsAsFile())
        {
            compiled = false;
            continue;
        }

        objects.add (object.getFullPathName());
    }

    if (compiled)
    {
        // The stack first in memory: running past it traps, instead of writing over the device's own data.
        auto args = common;
        args.addArray (juce::StringArray { "-mexec-model=reactor", "-o", output.getFullPathName() });
        args.addArray (objects);
        args.addArray (juce::StringArray { "-Wl,--strip-all", "-Wl,--stack-first", "-Wl,-z,stack-size=262144", "-Wl,--max-memory=67108864" });

        const auto step = runStep (args, cancelled, started);

        if (ended (step))
            return result;

        log += step.messages;
        compiled = step.exitCode == 0;
    }

    result.log = clean (log).trim().substring (0, 12000);
    result.seconds = (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0;

    if (compiled && output.existsAsFile() && output.getSize() > 0 && output.loadFileAsData (result.wasm))
        result.ok = true;
    else if (result.log.isEmpty())
        result.log = "The build failed.";

    return result;
}
}
