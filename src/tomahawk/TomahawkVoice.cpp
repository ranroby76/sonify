// D:\workspace\Fanan_web_hub\src\tomahawk\TomahawkVoice.cpp

#include "TomahawkVoice.h"
#include "GlobalMacros.h"

#include <cmath>
#include <optional>
#include <utility>

namespace Tomahawk
{
    namespace
    {
        std::function<juce::File()>& userFolderFunction()
        {
            static std::function<juce::File()> folder;
            return folder;
        }

        /** A pack's folder under the library: the Tomahawk's names, then Grex's. */
        juce::File packFolder (const juce::File& root, int pack)
        {
            static const char* const names[numPacks][3] { { "classic", "sounds_world", "world" },
                                                          { "oriental", "sounds_oriental", nullptr } };

            for (const auto* name : names[pack])
                if (name != nullptr)
                    if (const auto folder = root.getChildFile (name); folder.isDirectory())
                        return folder;

            return {};
        }

        bool holdsPacks (const juce::File& folder)
        {
            if (! folder.isDirectory())
                return false;

            for (int pack = 0; pack < numPacks; ++pack)
                if (packFolder (folder, pack).isDirectory())
                    return true;

            return false;
        }

        /** The instruments read already, shared by every Tomahawk playing the same one. */
        struct DecodedCache
        {
            juce::CriticalSection lock;
            std::map<juce::String, std::weak_ptr<Betel::Channel::PresetVoice>> voices;

            std::shared_ptr<Betel::Channel::PresetVoice> find (const juce::String& key)
            {
                const juce::ScopedLock sl (lock);

                if (const auto it = voices.find (key); it != voices.end())
                    return it->second.lock();

                return {};
            }

            void keep (const juce::String& key, const std::shared_ptr<Betel::Channel::PresetVoice>& voice)
            {
                const juce::ScopedLock sl (lock);

                for (auto it = voices.begin(); it != voices.end();)
                    it = it->second.expired() ? voices.erase (it) : std::next (it);

                voices[key] = voice;
            }
        };

        DecodedCache& decodedCache()
        {
            static DecodedCache cache;
            return cache;
        }

        /** Grex's SamplePlayerEngine::instrumentGainLinear: base x trim, 100 % always the base. */
        float instrumentGainLinear (float gainPercent, float baseUnityDb) noexcept
        {
            const float base = juce::Decibels::decibelsToGain (juce::jlimit (-40.0f, 40.0f, baseUnityDb));
            return base * juce::jlimit (0.0f, 200.0f, gainPercent) * 0.01f;
        }

        /** Grex's setChannelBandFilterNorm: the band filter's handles, 0..1, as Hz. */
        float bandHz (float norm) noexcept
        {
            return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, norm));
        }
    }

    void readFactory (Library::Contents& into);

    //==============================================================================
    const Library::Sound* Library::Contents::find (int flag) const
    {
        const auto it = sounds.find (flag);
        return it != sounds.end() ? &it->second : nullptr;
    }

    const Betel::InstrumentPreset* Library::Contents::presetFor (int flag) const
    {
        if (const auto it = factory.find (flag); it != factory.end())
            return &it->second;

        return nullptr;
    }

    std::vector<const Library::Sound*> Library::Contents::soundsIn (int pack, const juce::String& category) const
    {
        std::vector<const Sound*> list;

        for (const auto& [f, sound] : sounds)
            if (sound.pack == pack && sound.category == category)
                list.push_back (&sound);

        return list;
    }

    bool Library::Contents::hasPack (int pack) const
    {
        for (const auto& [f, sound] : sounds)
            if (sound.pack == pack)
                return true;

        return false;
    }

    //==============================================================================
    Library& Library::get()
    {
        static Library library;
        return library;
    }

    void Library::setUserFolder (std::function<juce::File()> folder)
    {
        userFolderFunction() = std::move (folder);
    }

    juce::File Library::userFolder()
    {
        const auto base = userFolderFunction() != nullptr ? userFolderFunction()()
                                                          : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Sonify");
        return base.getChildFile ("Tomahawk");
    }

    const char* Library::packName (int pack) noexcept
    {
        switch (pack)
        {
            case packOriental:  return "ORIENTAL";
            default:            return "CLASSIC";
        }
    }

    juce::File Library::findRoot (const juce::File& chosen)
    {
        if (holdsPacks (chosen))
            return chosen;

        if (const auto inside = chosen.getChildFile ("Tomahawk_Sampler"); holdsPacks (inside))
            return inside;

        if (holdsPacks (chosen.getParentDirectory()))
            for (int pack = 0; pack < numPacks; ++pack)
                if (packFolder (chosen.getParentDirectory(), pack) == chosen)
                    return chosen.getParentDirectory();

        return {};
    }

    juce::File Library::savedRoot() const
    {
        const auto settings = juce::JSON::parse (userFolder().getChildFile ("settings.json").loadFileAsString());
        const auto path = settings.getProperty ("library", {}).toString();
        return juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();
    }

    void Library::saveRoot (const juce::File& root) const
    {
        auto* settings = new juce::DynamicObject();
        settings->setProperty ("library", root.getFullPathName());

        const auto file = userFolder().getChildFile ("settings.json");
        file.getParentDirectory().createDirectory();
        file.replaceWithText (juce::JSON::toString (juce::var (settings), false));
    }

    std::shared_ptr<const Library::Contents> Library::contents()
    {
        {
            const juce::ScopedLock sl (lock);

            if (tried || current != nullptr)
                return current;

            tried = true;
        }

        // First use: where it was located last, else where it's usually put (beside Sonify or
        // up from it, the user's folder, Documents).
        juce::Array<juce::File> places;
        places.add (savedRoot());

        for (auto dir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
             dir.exists() && places.size() < 8; dir = dir.getParentDirectory())
        {
            places.add (dir.getChildFile ("Tomahawk_Sampler"));

            if (dir.isRoot())
                break;
        }

        places.add (userFolder().getParentDirectory().getChildFile ("Tomahawk_Sampler"));
        places.add (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Tomahawk_Sampler"));

        juce::File root;

        for (const auto& place : places)
            if (place != juce::File() && (root = findRoot (place)) != juce::File())
                break;

        auto made = root != juce::File() ? scan (root) : std::make_shared<Contents>();

        if (root == juce::File())
            readFactory (*made);

        {
            const juce::ScopedLock sl (lock);

            if (current == nullptr)
                current = std::move (made);

            return current;
        }
    }

    bool Library::locate (const juce::File& chosen, juce::String& problem)
    {
        const auto root = findRoot (chosen);

        if (root == juce::File())
        {
            problem = "That folder isn't the Tomahawk library: it should hold the classic and oriental folders (the Tomahawk_Sampler folder, as it comes out of the zip).";
            return false;
        }

        auto made = scan (root);

        if (! made->isInstalled())
        {
            problem = "No sounds were found in " + root.getFullPathName() + (made->report.isNotEmpty() ? ":\n\n" + made->report : juce::String ("."));
            return false;
        }

        saveRoot (root);
        publish (std::move (made));
        return true;
    }

    void Library::rescan()
    {
        const auto now = contents();
        publish (scan (now != nullptr ? now->root : juce::File()));
    }

    void Library::reloadPresets()
    {
        const auto now = contents();
        auto made = std::make_shared<Contents> (now != nullptr ? *now : Contents());
        readFactory (*made);
        publish (std::move (made));
    }

    void Library::publish (std::shared_ptr<Contents> made)
    {
        {
            const juce::ScopedLock sl (lock);
            current = std::move (made);
            tried = true;
        }

        listeners.call ([] (Listener& l) { l.libraryChanged(); });
    }


    juce::File Library::factoryPresetsFolder (int pack)
    {
        // Beside Sonify.exe; on a Mac in the app's Resources, where the build puts them.
        auto folder = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();

       #if JUCE_MAC
        folder = folder.getSiblingFile ("Resources");
       #endif

        return folder.getChildFile (pack == packOriental ? "Tomahawk_oriental_instruments_presets"
                                                         : "Tomahawk_classic_instruments_presets");
    }

    juce::File Library::savedPresetsFolder (int pack)
    {
        // Sonify's own settings folder (AppData\Roaming\Fanan\Sonify, Library/Fanan/Sonify on a Mac).
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Fanan").getChildFile ("Sonify").getChildFile ("Tomahawk")
                   .getChildFile (factoryPresetsFolder (pack).getFileName());
    }

    /** Every melodic preset under a folder (recursive, as Grex reads them), by flag, with the
        file each came from. */
    static void readPresets (const juce::File& folder, std::map<int, Betel::InstrumentPreset>& into,
                             std::map<int, juce::File>& files)
    {
        if (! folder.isDirectory())
            return;

        auto found = folder.findChildFiles (juce::File::findFiles, true, juce::String ("*") + Betel::InstrumentPresetIO::kExtension);
        found.sort();

        for (const auto& f : found)
        {
            auto preset = Betel::InstrumentPresetIO::read (f);

            if (! preset.ok || preset.isDrum)
                continue;

            files[preset.flag] = f;
            into[preset.flag] = std::move (preset);
        }
    }

    /** Where a pack's presets beside Sonify.exe are: there, else the pack's own instruments_presets. */
    static juce::File presetsHome (const Library::Contents& library, int pack)
    {
        auto folder = Library::factoryPresetsFolder (pack);

        if (! folder.isDirectory() && library.root != juce::File())
            folder = packFolder (library.root, pack).getChildFile ("instruments_presets");

        return folder;
    }

    /** The presets: beside Sonify.exe (else each pack's own instruments_presets), then the ones
        saved in Sonify's AppData folder, which win. */
    void readFactory (Library::Contents& into)
    {
        into.factory.clear();
        into.factoryFiles.clear();

        for (int pack = 0; pack < numPacks; ++pack)
            readPresets (presetsHome (into, pack), into.factory, into.factoryFiles);

        for (int pack = 0; pack < numPacks; ++pack)
            readPresets (Library::savedPresetsFolder (pack), into.factory, into.factoryFiles);
    }

    juce::File Library::Contents::presetFileFor (int flag) const
    {
        if (const auto it = factoryFiles.find (flag); it != factoryFiles.end())
            return it->second;

        return {};
    }

    //==============================================================================
    // Grex's SamplePlayerEngine::setSoundLibraryFolders, for the two packs.
    std::shared_ptr<Library::Contents> Library::scan (const juce::File& root) const
    {
        auto made = std::make_shared<Contents>();
        made->root = root;

        juce::StringArray clashes, unnumbered;
        int loadedPerPack[numPacks] {};
        bool packPresent[numPacks] {};

        for (int pack = 0; pack < numPacks && root != juce::File(); ++pack)
        {
            const auto folder = packFolder (root, pack);

            if (! folder.isDirectory())
                continue;                                   // pack simply not installed

            packPresent[pack] = true;

            auto files = folder.findChildFiles (juce::File::findFiles, true, "*.frb");
            files.sort();                                   // the same order every time

            for (const auto& f : files)
            {
                const juce::String base = f.getFileNameWithoutExtension();

                // Drum-kit blobs: *_kit.frb, and NNN_Name.frb kits.
                if (base.endsWithIgnoreCase ("_kit"))
                    continue;

                int i = 0;

                while (i < base.length() && juce::CharacterFunctions::isDigit (base[i]))
                    ++i;

                if (i == 0)
                {
                    if (unnumbered.size() < 40)
                        unnumbered.add (juce::String ("   ") + packName (pack) + "  " + f.getFileName());

                    continue;
                }

                if (base[i] == '_')
                    continue;

                const int soundFlag = base.substring (0, i).getIntValue();

                if (soundFlag < 0)
                    continue;

                if (const auto it = made->sounds.find (soundFlag); it != made->sounds.end())
                {
                    clashes.add (juce::String (soundFlag) + "  kept: " + it->second.file.getFileName()
                                 + "   ignored: " + f.getFileName() + "  (" + packName (pack) + ")");
                    continue;
                }

                Sound sound;
                sound.flag = soundFlag;
                sound.pack = pack;
                sound.file = f;

                const int dash = base.indexOfChar ('-');
                sound.name = Betel::prettyName (dash >= 0 ? base.substring (dash + 1).trim() : base.substring (i).trim());

                // The first folder under the pack's, however deep the file is.
                for (auto dir = f.getParentDirectory(); dir.isDirectory() && dir != folder; dir = dir.getParentDirectory())
                {
                    sound.category = Betel::prettyName (dir.getFileName());

                    if (dir.getParentDirectory() == folder)
                        break;
                }

                if (sound.category.isNotEmpty() && ! made->categories[(size_t) pack].contains (sound.category))
                    made->categories[(size_t) pack].add (sound.category);

                made->sounds[soundFlag] = std::move (sound);
                ++loadedPerPack[pack];
            }

            made->categories[(size_t) pack].sort (true);
        }

        juce::StringArray report;

        for (int pack = 0; pack < numPacks; ++pack)
            report.add (juce::String (packName (pack)).paddedRight (' ', 12)
                        + (packPresent[pack] ? juce::String (loadedPerPack[pack]) + " sound(s)"
                                             : juce::String ("not installed")));

        if (! unnumbered.isEmpty())
        {
            report.add ({});
            report.add ("Skipped, no number at the start of the name:");
            report.addArray (unnumbered);
        }

        if (! clashes.isEmpty())
        {
            report.add ({});
            report.add ("The same number twice (the second isn't loaded):");
            report.addArray (clashes);
        }

        made->report = report.joinIntoString ("\n");
        readFactory (*made);
        return made;
    }

    //==============================================================================
    struct Voice::Job
    {
        juce::File file;
        int generation = 0;
    };

    /** Reads instruments away from the message and audio threads. */
    class Voice::Loader final : public juce::Thread
    {
    public:
        explicit Loader (Voice& v) : juce::Thread ("Tomahawk loader"), voice (v) {}

        ~Loader() override
        {
            signalThreadShouldExit();
            wake.signal();
            stopThread (20000);
        }

        void load (Job job)
        {
            {
                const juce::ScopedLock sl (jobLock);
                next = std::move (job);
            }

            if (! isThreadRunning())
                startThread (juce::Thread::Priority::normal);

            wake.signal();
        }

    private:
        void run() override
        {
            while (! threadShouldExit())
            {
                std::optional<Job> job;

                {
                    const juce::ScopedLock sl (jobLock);
                    job = std::move (next);
                    next.reset();
                }

                if (! job.has_value())
                {
                    wake.wait (-1);
                    continue;
                }

                juce::String error;
                const auto key = job->file.getFullPathName();
                auto made = decodedCache().find (key);

                if (made == nullptr)
                {
                    BlobReader reader;

                    if (! job->file.existsAsFile())
                        error = job->file.getFileName() + " isn't in the Tomahawk library any more.";
                    else if (! reader.open (job->file.getFullPathName().toStdString(), kAccessCode))
                        error = job->file.getFileName() + " couldn't be read (" + juce::String (reader.getError()) + ").";
                    else if ((made = Betel::Channel::decodeInstrument (reader, 0)) == nullptr)
                        error = job->file.getFileName() + " has no sound in it.";

                    if (made != nullptr)
                        decodedCache().keep (key, made);
                }

                if (! threadShouldExit())
                    voice.install (*job, std::move (made), error);
            }
        }

        Voice& voice;
        juce::CriticalSection jobLock;
        std::optional<Job> next;
        juce::WaitableEvent wake;
    };

    //==============================================================================
    Voice::Voice()
        : channel (std::make_unique<Betel::Channel>()),
          rack (std::make_unique<Betel::SectionSendFx>())
    {
        // The right hand's first channel, as in Grex: the band filter, the solo-only
        // features (attack glide) and the neutral voice stamp all key off it.
        channel->setIndex (Betel::Channel::kFirstSoloChannel);
        channel->setSoloChannel (true);
        channel->setSharedPresetCache (&presetCache);

        loader = std::make_unique<Loader> (*this);
        Library::get().addListener (this);

        const juce::ScopedLock sl (stateLock);
        commit (params);
    }

    Voice::~Voice()
    {
        Library::get().removeListener (this);
        loader = nullptr;
    }

    void Voice::prepare (double sampleRate, int maxBlockSize)
    {
        maxBlock = juce::jmax (32, maxBlockSize);

        const juce::SpinLock::ScopedLockType sl (audioLock);
        channel->prepare (sampleRate, maxBlock);
        rack->prepare (sampleRate, maxBlock);
        bus.setSize (2, maxBlock, false, true, true);
        pedalDown = false;
        sustained.fill (false);
        held.fill (false);
    }

    void Voice::setBendRange (float semitones) noexcept
    {
        channel->pitchBendRange.store (juce::jlimit (1.0f, 12.0f, semitones));
    }

    void Voice::setLowVelocityResponse (float amount) noexcept
    {
        // Grex's setSoloLowVelBoost: 0..100 as the global velocity curve 0..+1.
        channel->setGlobalVelCurve (juce::jlimit (0.0f, 100.0f, amount) / 100.0f);
    }

    //==============================================================================
    void Voice::render (float* left, float* right, int numSamples, const juce::MidiBuffer& midi, double bpm) noexcept
    {
        const juce::SpinLock::ScopedTryLockType sl (audioLock);

        if (! sl.isLocked() || numSamples <= 0 || bus.getNumSamples() == 0)
            return;

        // The editor's keyboard, from the block's start.
        {
            const auto scope = editorFifo.read (editorFifo.getNumReady());

            const auto play = [this] (int start, int size)
            {
                for (int i = start; i < start + size; ++i)
                {
                    const auto& n = editorNotes[(size_t) i];

                    if (n.velocity > 0)
                        channel->noteOn (n.note, n.velocity, true);
                    else
                        channel->noteOff (n.note, true);
                }
            };

            play (scope.startIndex1, scope.blockSize1);
            play (scope.startIndex2, scope.blockSize2);
        }

        const auto handle = [this] (const juce::MidiMessage& m)
        {
            if (m.isNoteOn())
            {
                const int note = m.getNoteNumber();
                held[(size_t) note] = true;
                sustained[(size_t) note] = false;
                channel->noteOn (note, m.getVelocity());
            }
            else if (m.isNoteOff())
            {
                const int note = m.getNoteNumber();
                held[(size_t) note] = false;

                if (pedalDown)
                    sustained[(size_t) note] = true;
                else
                    channel->noteOff (note);
            }
            else if (m.isSustainPedalOn())
            {
                pedalDown = true;
            }
            else if (m.isSustainPedalOff())
            {
                pedalDown = false;

                for (int note = 0; note < 128; ++note)
                    if (std::exchange (sustained[(size_t) note], false) && ! held[(size_t) note])
                        channel->noteOff (note);
            }
            else if (m.isAllNotesOff() || m.isAllSoundOff())
            {
                channel->allNotesOff();
                sustained.fill (false);
                held.fill (false);
                pedalDown = false;
            }
            else if (m.isPitchWheel())
            {
                channel->pitchBend (((float) m.getPitchWheelValue() - 8192.0f) / 8192.0f);
            }
            else if (m.isController())
            {
                // Grex's SamplePlayerEngine::controlChange: volume and expression.
                const float linear = (float) juce::jlimit (0, 127, m.getControllerValue()) / 127.0f;

                if (m.getControllerNumber() == 7)
                    channel->channelVolume.store (linear);
                else if (m.getControllerNumber() == 11)
                    channel->channelExpression.store (linear);
            }
        };

        // As Grex's SamplePlayerEngine renders its right-hand bus: the channel into the bus
        // and its sends, the rack's returns added, then out.
        const auto renderTo = [&] (int from, int to)
        {
            for (int at = from; at < to;)
            {
                const int n = juce::jmin (to - at, maxBlock, bus.getNumSamples());

                bus.clear (0, 0, n);
                bus.clear (1, 0, n);
                rack->clearSends (n);
                channel->renderBlock (bus, n, bpm, rack.get());
                rack->process (bus, n, bpm);

                juce::FloatVectorOperations::copy (left + at, bus.getReadPointer (0), n);
                juce::FloatVectorOperations::copy (right + at, bus.getReadPointer (1), n);
                at += n;
            }
        };

        int at = 0;

        for (const auto metadata : midi)
        {
            const int position = juce::jlimit (0, numSamples, metadata.samplePosition);

            if (position > at)
            {
                renderTo (at, position);
                at = position;
            }

            handle (metadata.getMessage());
        }

        renderTo (at, numSamples);
    }
    //==============================================================================
    SlotParams Voice::voicedFor (int f) const
    {
        // The sound's saved .ins: a whole voice, or (a gain-only file) its gain alone.
        SlotParams p;

        if (const auto library = Library::get().contents())
        {
            if (const auto* preset = library->presetFor (f))
            {
                if (! preset->gainOnly)
                {
                    p = preset->params;
                }
                else
                {
                    p.gainPercent = preset->params.gainPercent;
                    p.baseUnityDb = preset->params.baseUnityDb;
                    p.baseOctave = preset->params.baseOctave;
                }
            }
        }

        return p;
    }

    void Voice::takeCalibration (SlotParams& p, int f)
    {
        // The base gain and base octave belong to the sound (its preset), whatever else holds:
        // as Grex reads a solo slot's base unity from the .ins.
        p.baseUnityDb = 0.0f;
        p.baseOctave = 0;

        if (const auto library = Library::get().contents())
        {
            if (const auto* preset = library->presetFor (f))
            {
                p.baseUnityDb = preset->params.baseUnityDb;
                p.baseOctave = preset->params.baseOctave;
            }
        }
    }

    void Voice::commit (const SlotParams& p)
    {
        // Grex's SoundsTab::commitSlotParamsToEngine for a solo slot, with its host's
        // handlers: the sweetener on its own route, the bulk params, the band filter, no
        // Funkey on the right hand, the per-sound gain over its base, the base octave (Grex's
        // hidden octave bias).
        channel->applySweetenerParams (p.sweet);
        channel->applyParams (slotParamsToChannelParams (p));
        channel->setBandFilterHz (bandHz (p.filterHpNorm), bandHz (p.filterLpNorm));
        channel->setFunkeyFx (Betel::Channel::FunkeyFx {});
        channel->instrumentGain.store (instrumentGainLinear (p.gainPercent, p.baseUnityDb));
        channel->setOctaveBias (juce::jlimit (-3, 3, p.baseOctave));

        // The chorus, reverb and delay are the sound's own: Grex's rack, every effect on and at
        // full level, set from the sound's settings. What reaches each is the sound's own
        // amount (its MIX / WET, through applyParams above, while the effect is on).
        using S = Betel::SectionSendFx;

        for (int slot = 0; slot < S::kNumSlots; ++slot)
        {
            rack->enabled[(size_t) slot].store (true);
            rack->level[(size_t) slot].store (1.0f);
        }

        // Grex's SectionFxBridge::write, from the sound's settings.
        rack->chorusRateHz.store (0.1f + juce::jlimit (0.0f, 1.0f, p.chorusRate) * (3.0f - 0.1f));
        rack->chorusDepth.store (juce::jlimit (0.0f, 1.0f, p.chorusDepth));
        rack->reverbSize.store (juce::jlimit (0.0f, 1.0f, p.reverbSize));
        rack->reverbDamp.store (juce::jlimit (0.0f, 1.0f, p.reverbDamp));
        rack->reverbTail.store (juce::jlimit (0.0f, 1.0f, p.reverbTail));
        rack->reverbPreDelay.store (juce::jlimit (0.0f, 1.0f, p.reverbPreDelay));
        rack->reverbHpNorm.store (juce::jlimit (0.0f, 1.0f, p.reverbHpNorm));
        rack->reverbLpNorm.store (juce::jlimit (0.0f, 1.0f, p.reverbLpNorm));
        rack->reverbErMix.store (juce::jlimit (0.0f, 1.0f, p.reverbErMix));
        rack->reverbErSize.store (juce::jlimit (0.0f, 1.0f, p.reverbErSize));
        rack->delayFeedback.store (juce::jlimit (0.0f, 0.95f, p.delayFeedback));
        rack->delayDampHz.store (juce::jlimit (200.0f, 20000.0f, p.delayDampHz));
        rack->delayHpHz.store (juce::jlimit (20.0f, 2000.0f, p.delayHpHz));
        rack->delaySmoothMs.store (juce::jlimit (1.0f, 500.0f, p.delaySmoothMs));
        rack->delayTimeSig.store (juce::jlimit (0, 1, p.delayTimeSig));
        rack->delayDiv.store (juce::jmax (0, p.delayDiv));
    }

    void Voice::selectInstrument (int newFlag)
    {
        {
            const juce::ScopedLock sl (stateLock);
            flag = newFlag;
            params = voicedFor (newFlag);
            commit (params);
            start (newFlag);
        }

        notify();
    }

    void Voice::start (int f)
    {
        juce::File file;
        instrumentName.clear();

        if (const auto library = Library::get().contents())
        {
            if (const auto* sound = library->find (f))
            {
                file = sound->file;
                instrumentName = sound->name;
            }
        }

        ++generation;
        problem.clear();

        if (f < 0)
        {
            loading.store (Loading::none);
            return;
        }

        if (file == juce::File())
        {
            const auto library = Library::get().contents();
            problem = library != nullptr && library->isInstalled()
                        ? juce::String ("Sound ") + juce::String (f) + " isn't in the Tomahawk library."
                        : juce::String ("The Tomahawk library isn't located yet: Locate Library shows where it is.");
            loading.store (Loading::failed);
            return;
        }

        loading.store (Loading::loading);
        loader->load ({ file, generation });
    }

    void Voice::install (const Job& job, std::shared_ptr<Betel::Channel::PresetVoice> made, const juce::String& error)
    {
        std::shared_ptr<Betel::Channel::PresetVoice> previous;

        {
            const juce::ScopedLock sl (stateLock);

            if (job.generation != generation)
                return;                                      // another was asked for since

            if (made == nullptr)
            {
                problem = error;
                loading.store (Loading::failed);
            }
            else
            {
                {
                    const juce::SpinLock::ScopedLockType audio (audioLock);
                    channel->adoptInstrument (flag, made, true);
                }

                previous = std::exchange (heldBlob, made);
                commit (params);
                loading.store (Loading::ready);
            }
        }

        previous.reset();   // the last one, outside the locks
        notify();
    }

    void Voice::notify()
    {
        juce::MessageManager::callAsync ([safe = juce::WeakReference<Voice> (this)]
        {
            if (auto* v = safe.get())
                if (v->onChanged != nullptr)
                    v->onChanged();
        });
    }

    void Voice::libraryChanged()
    {
        // A sound that couldn't be found may be there now; one playing keeps playing.
        bool retry = false;

        {
            const juce::ScopedLock sl (stateLock);

            if (flag >= 0 && loading.load() == Loading::failed)
            {
                start (flag);
                retry = true;
            }
        }

        if (! retry)
            notify();
    }

    int Voice::getFlag() const
    {
        const juce::ScopedLock sl (stateLock);
        return flag;
    }

    juce::String Voice::getInstrumentName() const
    {
        const juce::ScopedLock sl (stateLock);
        return instrumentName;
    }

    juce::String Voice::getProblem() const
    {
        const juce::ScopedLock sl (stateLock);
        return problem;
    }

    SlotParams Voice::getParams() const
    {
        const juce::ScopedLock sl (stateLock);
        return params;
    }

    void Voice::setParams (const SlotParams& p)
    {
        const juce::ScopedLock sl (stateLock);
        params = p;
        commit (params);
    }

    //==============================================================================
    juce::File Voice::savePreset (juce::String& why)
    {
        SlotParams current;

        {
            const juce::ScopedLock sl (stateLock);
            current = params;
        }

        return writePreset ([current] (SlotParams& p) { p = current; }, true, why);
    }

    //==============================================================================
    // A sound's preset written: Save Preset (whole: every setting, the file becomes a full preset)
    // or the base gain or octave (the rest of the file as it was, as Grex's
    // InstrumentPresetIO::writeBaseUnity does). Into the sound's file in the presets' folders, or a
    // new one in its category there; where that can't be written (Sonify installed in Program
    // Files), the same place in Sonify's AppData folder. While developing (the build says where
    // the presets come from), the change goes into that source copy as well, so the next build's
    // copy beside Sonify.exe keeps it.
    juce::File Voice::writePreset (std::function<void (SlotParams&)> change, bool whole, juce::String& why)
    {
        const int f = getFlag();
        const auto library = Library::get().contents();
        const auto* sound = library != nullptr ? library->find (f) : nullptr;

        if (sound == nullptr)
        {
            why = "Pick a sound of the library first.";
            return {};
        }

        auto file = library->presetFileFor (f);
        Betel::InstrumentPreset preset;

        if (file.existsAsFile())
            preset = Betel::InstrumentPresetIO::read (file);

        if (! preset.ok)
        {
            // None yet: a gain-only one (a base alone is calibration), in the sound's category
            // of its pack's presets.
            juce::File packFolderOfSound, categoryFolder;

            for (auto dir = sound->file.getParentDirectory(); dir.isDirectory() && dir != library->root; dir = dir.getParentDirectory())
            {
                categoryFolder = packFolderOfSound;
                packFolderOfSound = dir;
            }

            auto presets = presetsHome (*library, sound->pack);

            if (categoryFolder != juce::File())
                presets = presets.getChildFile (categoryFolder.getFileName());

            file = presets.getChildFile (juce::String (f) + "-" + Betel::InstrumentPresetIO::sanitiseName (sound->name)
                                         + Betel::InstrumentPresetIO::kExtension);

            preset.ok = true;
            preset.gainOnly = true;
            preset.flag = f;
            preset.displayName = sound->name;
            preset.frbFileName = sound->file.getFileName();
        }

        change (preset.params);

        if (whole)
            preset.gainOnly = false;

        juce::ValueTree root (Betel::InstrumentPresetIO::kRootTag);
        root.setProperty ("version", Betel::InstrumentPresetIO::kVersion, nullptr);
        root.setProperty ("type", "melodic", nullptr);
        root.setProperty ("set", "solo", nullptr);
        root.setProperty ("scope", preset.gainOnly ? "gain" : "full", nullptr);
        root.setProperty ("flag", f, nullptr);
        root.setProperty ("name", preset.displayName.isNotEmpty() ? preset.displayName : sound->name, nullptr);
        root.setProperty ("frb", preset.frbFileName.isNotEmpty() ? preset.frbFileName : sound->file.getFileName(), nullptr);
        root.addChild (BetelStateXml::saveSlot (preset.params), -1, nullptr);
        BetelStateXml::roundNumbersTo2Decimals (root);

        const auto xml = root.createXml();

        if (xml == nullptr)
        {
            why = "The preset couldn't be made.";
            return {};
        }

        const auto writeAt = [&xml] (const juce::File& target)
        {
            return target.getParentDirectory().createDirectory().wasOk() && xml->writeTo (target);
        };

        if (! writeAt (file))
        {
            // Sonify.exe's folder can't be written: the same place in Sonify's AppData folder.
            const auto saved = Library::savedPresetsFolder (sound->pack);
            const auto home = presetsHome (*library, sound->pack);

            const auto fallback = file.isAChildOf (home)  ? saved.getChildFile (file.getRelativePathFrom (home))
                                : file.isAChildOf (saved) ? file
                                                          : saved.getChildFile (file.getParentDirectory().getFileName())
                                                                 .getChildFile (file.getFileName());

            if (fallback == file || ! writeAt (fallback))
            {
                why = "It couldn't be saved in " + file.getFullPathName()
                    + (fallback != file ? " nor in " + fallback.getFullPathName() : juce::String()) + ".";
                return {};
            }

            file = fallback;
        }

       #ifdef TOMAHAWK_PRESETS_SOURCE_DIR
        {
            const auto factory = Library::factoryPresetsFolder (sound->pack);
            const juce::File source (juce::String (TOMAHAWK_PRESETS_SOURCE_DIR));

            if (file.isAChildOf (factory) && source.isDirectory())
            {
                const auto copy = source.getChildFile (factory.getFileName()).getChildFile (file.getRelativePathFrom (factory));
                copy.getParentDirectory().createDirectory();
                xml->writeTo (copy);
            }
        }
       #endif

        Library::get().reloadPresets();
        return file;
    }

    juce::File Voice::setBaseGain (float db, juce::String& why)
    {
        db = juce::jlimit (-40.0f, 40.0f, db);

        const auto file = writePreset ([db] (SlotParams& p)
        {
            p.baseUnityDb = db;
            p.gainPercent = 100.0f;   // the handle returns to the middle: the base IS what 100 means now
        }, false, why);

        if (file != juce::File())
        {
            const juce::ScopedLock sl (stateLock);
            params.baseUnityDb = db;
            params.gainPercent = 100.0f;
            commit (params);
        }

        return file;
    }

    juce::File Voice::setBaseOctave (int octave, juce::String& why)
    {
        octave = juce::jlimit (-3, 3, octave);

        const auto file = writePreset ([octave] (SlotParams& p) { p.baseOctave = octave; }, false, why);

        if (file != juce::File())
        {
            const juce::ScopedLock sl (stateLock);
            params.baseOctave = octave;
            commit (params);
        }

        return file;
    }

    //==============================================================================
    void Voice::editorNoteOn (int note, int velocity)
    {
        if (note < 0 || note > 127)
            return;

        const auto scope = editorFifo.write (1);

        if (scope.blockSize1 > 0)
            editorNotes[(size_t) scope.startIndex1] = { note, juce::jlimit (1, 127, velocity) };
    }

    void Voice::editorNoteOff (int note)
    {
        if (note < 0 || note > 127)
            return;

        const auto scope = editorFifo.write (1);

        if (scope.blockSize1 > 0)
            editorNotes[(size_t) scope.startIndex1] = { note, 0 };
    }

    void Voice::getSoundingNotes (uint32_t out[4]) const noexcept
    {
        channel->getSoundingNotes (out);
    }

    void Voice::getSweetenerGr (float gr[4]) const noexcept
    {
        gr[0] = channel->getSweetSoftenGrDb();
        gr[1] = channel->getSweetPeakGrDb();
        gr[2] = channel->getSweetTameGrDb();
        gr[3] = channel->getSweetRoundGrDb();
    }

    //==============================================================================
    juce::ValueTree Voice::saveState() const
    {
        const juce::ScopedLock sl (stateLock);

        juce::ValueTree state ("Tomahawk");
        state.setProperty ("flag", flag, nullptr);

        auto slot = BetelStateXml::saveSlot (params);
        slot.setProperty ("source", "library", nullptr);
        state.appendChild (slot, nullptr);
        return state;
    }

    void Voice::loadState (const juce::ValueTree& state)
    {
        {
            const juce::ScopedLock sl (stateLock);

            flag = state.isValid() ? (int) state.getProperty ("flag", -1) : -1;

            // The song's settings, as they were (a preset since doesn't speak over them), but
            // the sound's base gain and octave as its preset has them now.
            params = voicedFor (flag);

            for (const auto& child : state)
            {
                if (child.hasType ("Slot") && child.getProperty ("source", "library").toString() == "library")
                {
                    params = SlotParams();
                    BetelStateXml::loadSlot (child, params, BetelStateXml::SlotLoadContext::Preset);
                    break;
                }
            }

            takeCalibration (params, flag);

            {
                const juce::SpinLock::ScopedLockType audio (audioLock);
                channel->clearPreset();
            }

            commit (params);
            start (flag);
        }

        notify();
    }
}
