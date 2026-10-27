// D:\workspace\Fanan_web_hub\src\AxisPacker.cpp

#include "AxisPacker.h"

#include "stella_blob.h"
#include "Licences.h"
#include "Project.h"

#include <juce_events/juce_events.h>

#include <algorithm>

namespace
{
    juce::String textOf (const std::string& text)    { return juce::String (juce::CharPointer_UTF8 (text.c_str())); }

    /** The license.soni of the protection chosen, and its bytes. "" when it's read, else why not. */
    juce::String soniFrom (AxisPacker::Protection kind, const juce::File& source, std::vector<std::uint8_t>& bytes,
                           stella::licence::Soni& soni)
    {
        if (kind == AxisPacker::Protection::project)
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

    juce::String nameOfConditioning (AxisPack::Conditioning conditioning)
    {
        return conditioning == AxisPack::Conditioning::asItIs ? "keep"
             : conditioning == AxisPack::Conditioning::trimmed ? "trim" : "level";
    }

    AxisPack::Conditioning conditioningFrom (const juce::String& text)
    {
        return text == "keep" ? AxisPack::Conditioning::asItIs
             : text == "trim" ? AxisPack::Conditioning::trimmed : AxisPack::Conditioning::levelled;
    }

    juce::String secondsText (double seconds)
    {
        return juce::String (seconds, seconds < 1.0 ? 3 : 2) + " s";
    }

    /** What a capture turned out to be, for its row. */
    juce::String saidOf (const AxisPack::Capture& capture)
    {
        juce::StringArray said;
        said.add (capture.submodels.size() == 1 ? juce::String ("one width")
                                                : juce::String ((int) capture.submodels.size()) + " widths");
        said.add (juce::String ((int) capture.submodels.back().weights.size()) + " weights");
        said.add (juce::String (juce::roundToInt (capture.sampleRate / 1000.0)) + " kHz");

        if (capture.hasCab)
            said.add ("cabinet in it");

        if (capture.esr >= 0.0)
            said.add (juce::String (capture.esr * 100.0, 2) + "% off the amp");

        return said.joinIntoString (", ");
    }

    juce::String saidOf (const AxisPack::Ir& ir)
    {
        juce::StringArray said;
        said.add (secondsText ((double) ir.frames() / juce::jmax (1.0, ir.sampleRate)));
        said.add (juce::String (juce::roundToInt (ir.sampleRate / 1000.0)) + " kHz");
        said.add (ir.channels() > 1 ? "stereo" : "mono");
        return said.joinIntoString (", ");
    }
}

//==============================================================================
class AxisPacker::Worker final : public juce::Thread
{
public:
    explicit Worker (AxisPacker& p) : juce::Thread ("AXIS Pack"), packer (p) {}

    void run() override
    {
        AxisPack::Contents contents;
        juce::File blob;
        Conditioning conditioning;
        std::vector<std::uint8_t> soni;

        {
            const juce::ScopedLock hold (packer.lock);
            blob = packer.packFileOrDefault();
            conditioning = packer.conditioning;
            soni = packer.soni;
            contents.name = blob.getFileNameWithoutExtension();
            packer.pack = Pack();
            packer.pack.state = Pack::State::working;
            packer.pack.file = blob;
        }

        // ---------------------------------------------------------------- reading
        int read = 0;
        juce::StringArray usedNames;

        for (;;)
        {
            juce::File file;
            Item::Kind kind = Item::Kind::amp;
            int id = 0;

            {
                const juce::ScopedLock hold (packer.lock);

                if (! packer.stopping.load())
                {
                    for (auto& item : packer.items)
                    {
                        if (item.state == Item::State::waiting)
                        {
                            item.state = Item::State::working;
                            item.progress = 0.0;
                            item.message.clear();
                            file = item.file;
                            kind = item.kind;
                            id = item.id;
                            break;
                        }
                    }
                }
            }

            if (id == 0 || threadShouldExit() || packer.stopping.load())
                break;

            packer.changed();

            juce::String error, said, name;
            bool ok = false;

            if (kind == Item::Kind::amp)
            {
                AxisPack::Capture capture;
                ok = AxisPack::readNam (file, capture, error);

                if (ok)
                {
                    name = capture.displayName();
                    said = saidOf (capture);
                    contents.captures.push_back (std::move (capture));
                }
            }
            else
            {
                AxisPack::Ir ir;
                ok = AxisPack::readIr (file, conditioning, ir, error);

                if (ok)
                {
                    // AXIS finds a pack's cabinets by name ("pack:<name>", in a rig and in a song),
                    // so no two in one pack share one: a second "Mesa V30" is "Mesa V30 2".
                    const auto base = ir.displayName();
                    auto unique = base;

                    for (int n = 2; usedNames.contains (unique); ++n)
                        unique = base + " " + juce::String (n);

                    usedNames.add (unique);
                    ir.name = unique;
                    name = ir.displayName();
                    said = saidOf (ir);
                    contents.irs.push_back (std::move (ir));
                }
            }

            {
                const juce::ScopedLock hold (packer.lock);

                for (auto& item : packer.items)
                {
                    if (item.id != id)
                        continue;

                    item.state = ok ? Item::State::done : Item::State::failed;
                    item.progress = ok ? 1.0 : 0.0;
                    item.message = ok ? said : error;

                    if (ok && name.isNotEmpty())
                        item.name = name;
                }

                ++read;
            }

            packer.changed();
        }

        // ---------------------------------------------------------------- writing
        const bool stopped = threadShouldExit() || packer.stopping.load();

        if (stopped)
        {
            finishRun ("Stopped.", false);
            return;
        }

        if (contents.isEmpty())
        {
            finishRun (read == 0 ? "There's nothing listed to pack."
                                 : "None of the files listed could be read: their rows say why.", false);
            return;
        }

        juce::StringArray notes;
        auto lastReport = juce::Time::getMillisecondCounter();

        const auto problem = AxisPack::write (contents, blob, [this, &lastReport] (double progress)
        {
            if (threadShouldExit() || packer.stopping.load())
                return false;

            {
                const juce::ScopedLock hold (packer.lock);
                packer.pack.progress = progress;
            }

            if (const auto now = juce::Time::getMillisecondCounter(); now - lastReport > 120)
            {
                lastReport = now;
                packer.changed();
            }

            return true;
        }, soni, &notes);

        if (problem.isNotEmpty())
        {
            finishRun (problem, false);
            return;
        }

        int failed = 0;

        {
            const juce::ScopedLock hold (packer.lock);

            for (const auto& item : packer.items)
                if (item.state == Item::State::failed)
                    ++failed;
        }

        if (failed > 0)
            notes.insert (0, juce::String (failed) + (failed == 1 ? " file was" : " files were") + " left out: its row says why");

        {
            const juce::ScopedLock hold (packer.lock);
            packer.pack.amps = (int) contents.captures.size();
            packer.pack.cabs = (int) contents.irs.size();
            packer.pack.bytes = blob.getSize();
            packer.pack.locked = ! soni.empty();
        }

        finishRun (notes.joinIntoString ("; "), true);
    }

private:
    /** The run over: the pack's word, and anything still waiting left as it was. */
    void finishRun (const juce::String& message, bool ok)
    {
        {
            const juce::ScopedLock hold (packer.lock);

            packer.pack.state = ok ? Pack::State::done : Pack::State::failed;
            packer.pack.progress = ok ? 1.0 : 0.0;
            packer.pack.message = message;

            for (auto& item : packer.items)
            {
                if (item.state == Item::State::working)
                {
                    item.state = Item::State::waiting;
                    item.progress = 0.0;
                }
            }
        }

        packer.running.store (false);
        packer.changed();
    }

    AxisPacker& packer;
};

//==============================================================================
AxisPacker::AxisPacker (const juce::File& kept)
    : keptFile (kept)
{
    const auto saved = juce::JSON::parse (keptFile);

    if (saved.isObject())
    {
        if (const auto path = saved.getProperty ("pack", {}).toString(); juce::File::isAbsolutePath (path))
            packFile = juce::File (path);

        conditioning = conditioningFrom (saved.getProperty ("irs", "level").toString());

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

AxisPacker::~AxisPacker()
{
    alive->store (false);
    stopping.store (true);

    if (worker != nullptr)
        worker->stopThread (20000);
}

void AxisPacker::changed()
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

void AxisPacker::keep() const
{
    juce::DynamicObject::Ptr saved (new juce::DynamicObject());
    saved->setProperty ("pack", packFile.getFullPathName());
    saved->setProperty ("irs", nameOfConditioning (conditioning));
    saved->setProperty ("protection", protection == Protection::project ? "project" : protection == Protection::soniFile ? "file" : "none");
    saved->setProperty ("protectionSource", protectionSource.getFullPathName());
    keptFile.replaceWithText (juce::JSON::toString (juce::var (saved.get())));
}

juce::File AxisPacker::packFileOrDefault() const
{
    if (packFile != juce::File() && packFile.getParentDirectory().isDirectory())
        return packFile;

    // Nowhere set yet: beside the first thing listed, called AXIS.
    const auto folder = items.empty() ? juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                      : items.front().file.getParentDirectory();
    return folder.getChildFile (juce::String ("AXIS") + stella::blob::fileExtension);
}

juce::File AxisPacker::getPackFile() const
{
    const juce::ScopedLock hold (lock);
    return packFileOrDefault();
}

juce::File AxisPacker::getProtectionSource() const
{
    const juce::ScopedLock hold (lock);
    return protectionSource;
}

//==============================================================================
void AxisPacker::add (const juce::Array<juce::File>& files)
{
    juce::Array<juce::File> found;

    for (const auto& file : files)
    {
        // Cabinets only: AXIS models its own amps.
        if (file.isDirectory())
        {
            auto inside = file.findChildFiles (juce::File::findFiles, true, AxisPack::irWildcards());
            inside.sort();

            for (const auto& one : inside)
                if (AxisPack::isIrFile (one))
                    found.add (one);
        }
        else if (AxisPack::isIrFile (file))
        {
            found.add (file);
        }
    }

    {
        const juce::ScopedLock hold (lock);

        for (const auto& file : found)
        {
            bool listed = false;

            for (auto& item : items)
            {
                if (item.file == file)
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

            if (! listed && items.size() < 4000)
            {
                Item item;
                item.id = nextId++;
                item.file = file;
                item.kind = Item::Kind::cab;
                item.name = file.getFileNameWithoutExtension().replaceCharacter ('_', ' ').trim();
                items.push_back (item);
            }
        }
    }

    changed();
}

void AxisPacker::remove (int id)
{
    {
        const juce::ScopedLock hold (lock);
        items.erase (std::remove_if (items.begin(), items.end(),
                                     [id] (const Item& item) { return item.id == id && item.state != Item::State::working; }),
                     items.end());
    }

    changed();
}

void AxisPacker::clear()
{
    {
        const juce::ScopedLock hold (lock);
        items.erase (std::remove_if (items.begin(), items.end(), [] (const Item& item) { return item.state != Item::State::working; }),
                     items.end());
    }

    changed();
}

void AxisPacker::setPackFile (const juce::File& file)
{
    {
        const juce::ScopedLock hold (lock);
        packFile = file.hasFileExtension ("blob") ? file : file.withFileExtension (stella::blob::fileExtension);
    }

    keep();
    changed();
}

void AxisPacker::setConditioning (Conditioning newConditioning)
{
    {
        const juce::ScopedLock hold (lock);
        conditioning = newConditioning;
    }

    keep();
    changed();
}

//==============================================================================
juce::String AxisPacker::readProtection()
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

juce::String AxisPacker::setProtection (Protection kind, const juce::File& source)
{
    if (running.load())
        return "Wait until the pack being made is done.";

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

//==============================================================================
juce::String AxisPacker::start()
{
    if (running.load())
        return {};

    {
        const juce::ScopedLock hold (lock);

        if (items.empty())
            return "Nothing is listed: Add IRs... or Add Folder... picks the files.";
    }

    // The protection as it is now (a project's may have changed since it was chosen).
    if (protection != Protection::none)
    {
        if (const auto problem = readProtection(); problem.isNotEmpty())
        {
            changed();
            return "The pack can't be protected: " + problem;
        }
    }

    if (worker != nullptr)
        worker->stopThread (20000);

    {
        // Everything listed goes into the pack, so everything is read again.
        const juce::ScopedLock hold (lock);

        for (auto& item : items)
        {
            item.state = Item::State::waiting;
            item.progress = 0.0;
            item.message.clear();
        }
    }

    stopping.store (false);
    running.store (true);
    worker = std::make_unique<Worker> (*this);
    worker->startThread();
    changed();
    return {};
}

void AxisPacker::stop()
{
    stopping.store (true);
    changed();
}

//==============================================================================
juce::var AxisPacker::toVar() const
{
    juce::DynamicObject::Ptr state (new juce::DynamicObject());
    juce::Array<juce::var> list;
    juce::DynamicObject::Ptr made (new juce::DynamicObject());
    juce::DynamicObject::Ptr guard (new juce::DynamicObject());
    juce::File where;

    {
        const juce::ScopedLock hold (lock);

        for (const auto& item : items)
        {
            juce::DynamicObject::Ptr row (new juce::DynamicObject());
            row->setProperty ("id", item.id);
            row->setProperty ("name", item.name);
            row->setProperty ("file", item.file.getFullPathName());
            row->setProperty ("kind", item.kind == Item::Kind::amp ? "amp" : "cab");
            row->setProperty ("state", item.state == Item::State::waiting ? "waiting" : item.state == Item::State::working ? "working"
                                     : item.state == Item::State::done ? "done" : "failed");
            row->setProperty ("progress", item.progress);
            row->setProperty ("message", item.message);
            list.add (juce::var (row.get()));
        }

        where = packFileOrDefault();

        made->setProperty ("state", pack.state == Pack::State::working ? "working" : pack.state == Pack::State::done ? "done"
                                  : pack.state == Pack::State::failed ? "failed" : "none");
        made->setProperty ("progress", pack.progress);
        made->setProperty ("message", pack.message);
        made->setProperty ("amps", pack.amps);
        made->setProperty ("cabs", pack.cabs);
        made->setProperty ("bytes", (double) pack.bytes);
        made->setProperty ("locked", pack.locked);

        guard->setProperty ("kind", protection == Protection::project ? "project" : protection == Protection::soniFile ? "file" : "none");
        guard->setProperty ("source", protectionSource.getFullPathName());
        guard->setProperty ("product", protectionProduct);
        guard->setProperty ("name", protectionName);
        guard->setProperty ("vendor", protectionVendor);
        guard->setProperty ("problem", protectionProblem);

        state->setProperty ("irs", nameOfConditioning (conditioning));
    }

    state->setProperty ("kind", "axisPack");
    state->setProperty ("items", list);
    state->setProperty ("pack", where.getFullPathName());
    state->setProperty ("made", juce::var (made.get()));
    state->setProperty ("protection", juce::var (guard.get()));
    state->setProperty ("running", running.load() && ! stopping.load());
    return juce::var (state.get());
}

//==============================================================================
juce::String AxisPacker::describe (const juce::File& blob, int& amps, int& cabs, juce::String& name, bool& locked)
{
    amps = cabs = 0;
    name.clear();
    locked = false;

    AxisPack::Pack pack;
    juce::String error;

    if (! pack.open (blob, error))
        return error;

    amps = pack.count (AxisPack::Entry::Kind::amp);
    cabs = pack.count (AxisPack::Entry::Kind::cab);
    name = pack.getName();
    locked = pack.isLocked();
    return {};
}
