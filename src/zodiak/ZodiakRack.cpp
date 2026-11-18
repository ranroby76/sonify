// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakRack.cpp
#include "ZodiakRack.h"
#include "../Preferences.h"     // SettingsFolder: Sonify's own folder
#include "../InternalPlugin.h"  // Internals: Sonify's own plugins

namespace zodiak::rack
{
    //==========================================================================
    //  The plugins' open windows
    //==========================================================================
    namespace
    {
        juce::Array<EditorCloser*>& closers()
        {
            static juce::Array<EditorCloser*> list;
            return list;
        }
    }

    EditorCloser::EditorCloser()    { closers().add (this); }
    EditorCloser::~EditorCloser()   { closers().removeAllInstancesOf (this); }

    void closeEditorsOf (juce::AudioProcessor* plugin)
    {
        if (plugin == nullptr) return;
        const auto list = closers();
        for (auto* c : list)
            if (closers().contains (c)) c->closeEditorOf (plugin);
        if (auto* ed = plugin->getActiveEditor())
            delete ed;
    }

    //==========================================================================
    //  Hosted
    //==========================================================================
    Hosted::~Hosted()
    {
        if (instance != nullptr)
        {
            closeEditorsOf (instance.get());
            instance->releaseResources();
            instance.reset();
        }
    }

    void Hosted::prepare (double sampleRate, int maxBlock)
    {
        if (instance == nullptr) return;
        const int chans = juce::jmax (2, instance->getTotalNumInputChannels(), instance->getTotalNumOutputChannels());
        io.setSize (chans, juce::jmax (1, maxBlock), false, true, false);
        noMidi.ensureSize (64);
        instance->setRateAndBufferSizeDetails (sampleRate, maxBlock);
        instance->prepareToPlay (sampleRate, maxBlock);
    }

    void Hosted::play (float* L, float* R, int n, juce::MidiBuffer& midi, bool isInstrument) noexcept
    {
        if (instance == nullptr || n <= 0) return;
        if (n > io.getNumSamples())
            n = io.getNumSamples();

        const int chans = io.getNumChannels();
        juce::AudioBuffer<float> part (io.getArrayOfWritePointers(), chans, n);

        if (isInstrument)
            part.clear();
        else
        {
            part.copyFrom (0, 0, L, n);
            part.copyFrom (1, 0, R, n);
            for (int c = 2; c < chans; ++c) part.clear (c, 0, n);
        }

        instance->processBlock (part, midi);

        const int outs = instance->getTotalNumOutputChannels();
        std::copy_n (part.getReadPointer (0), n, L);
        std::copy_n (part.getReadPointer (outs >= 2 ? 1 : 0), n, R);
    }

    //==========================================================================
    //  Rack
    //==========================================================================
    Rack::Rack()
    {
        fx.reserve ((size_t) kMaxInserts);
    }

    void Rack::prepare (double sampleRate, int maxBlock)
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        sr = sampleRate;
        block = maxBlock;
        for (auto& i : fx)
        {
            if (i->builtIn != nullptr) i->builtIn->prepare (sampleRate);
            if (i->plugin != nullptr)  i->plugin->prepare (sampleRate, maxBlock);
        }
    }

    int Rack::addBuiltIn (int kind)
    {
        auto ins = std::make_unique<Insert>();
        ins->type = Insert::builtInType;
        ins->builtIn = std::make_unique<inserts::BuiltIn> (kind);
        ins->builtIn->prepare (sr);

        const juce::SpinLock::ScopedLockType sl (lock);
        if ((int) fx.size() >= kMaxInserts) return 0;
        ins->uid = nextUid++;
        const int uid = ins->uid;
        fx.push_back (std::move (ins));
        return uid;
    }

    int Rack::addPlugin (std::unique_ptr<Hosted> plugin)
    {
        if (plugin == nullptr) return 0;
        auto ins = std::make_unique<Insert>();
        ins->type = Insert::pluginType;
        ins->plugin = std::move (plugin);

        int uid = 0;
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            if ((int) fx.size() < kMaxInserts)
            {
                ins->uid = uid = nextUid++;
                fx.push_back (std::move (ins));
            }
        }
        return uid;     // a full list: `ins` (and the plugin) go here, off the lock
    }

    bool Rack::replaceWithPlugin (int uid, std::unique_ptr<Hosted> plugin)
    {
        if (plugin == nullptr) return false;
        auto ins = std::make_unique<Insert>();
        ins->type = Insert::pluginType;
        ins->plugin = std::move (plugin);

        std::unique_ptr<Insert> old;
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            for (auto& i : fx)
                if (i->uid == uid)
                {
                    ins->uid = uid;
                    ins->on.store (i->on.load());
                    old = std::move (i);
                    i = std::move (ins);
                    break;
                }
        }
        return old != nullptr;      // the old one is destroyed here, off the lock
    }

    bool Rack::removeInsert (int uid)
    {
        std::unique_ptr<Insert> old;
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            for (auto it = fx.begin(); it != fx.end(); ++it)
                if ((*it)->uid == uid)
                {
                    old = std::move (*it);
                    fx.erase (it);
                    break;
                }
        }
        return old != nullptr;
    }

    void Rack::clearInserts()
    {
        std::vector<std::unique_ptr<Insert>> old;
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            old.swap (fx);
            fx.reserve ((size_t) kMaxInserts);
        }
    }

    Insert* Rack::findInsert (int uid)
    {
        for (auto& i : fx)
            if (i->uid == uid) return i.get();
        return nullptr;
    }

    void Rack::clearAll()
    {
        clearInserts();
    }

    //==========================================================================
    //  Loading
    //==========================================================================
    namespace
    {
        juce::AudioPluginFormatManager& formats()
        {
            struct Formats
            {
                Formats()   { juce::addDefaultFormatsToManager (manager); }
                juce::AudioPluginFormatManager manager;
            };
            static Formats f;
            return f.manager;
        }
    }

    juce::String loadTitle()
    {
       #if JUCE_MAC
        return JUCE_PLUGINHOST_VST ? "Load any VST3, VST or AU" : "Load any VST3 or AU";
       #else
        return JUCE_PLUGINHOST_VST ? "Load any VST3 or VST" : "Load any VST3";
       #endif
    }

    juce::String loadPatterns()
    {
       #if JUCE_MAC
        return "*.vst3;*.vst;*.component";
       #elif JUCE_WINDOWS
        return JUCE_PLUGINHOST_VST ? "*.vst3;*.dll" : "*.vst3";
       #else
        return JUCE_PLUGINHOST_VST ? "*.vst3;*.so" : "*.vst3";
       #endif
    }

    namespace
    {
        juce::File folderPreset()
        {
            return SettingsFolder::get().getChildFile ("Zodiak").getChildFile ("Plugin Folder.xml");
        }

        juce::File systemVst3Folder()
        {
           #if JUCE_WINDOWS
            return juce::File ("C:\\Program Files\\Common Files\\VST3");
           #elif JUCE_MAC
            return juce::File ("/Library/Audio/Plug-Ins/VST3");
           #else
            return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".vst3");
           #endif
        }
    }

    juce::File pluginFolder()
    {
        if (auto xml = juce::XmlDocument::parse (folderPreset()))
            if (xml->hasTagName ("ZODIAK_PLUGIN_FOLDER"))
            {
                const auto path = xml->getStringAttribute ("path");
                if (juce::File::isAbsolutePath (path) && juce::File (path).isDirectory()) return juce::File (path);
            }
        return systemVst3Folder();
    }

    void rememberPluginFolder (const juce::File& file)
    {
        const auto plugin = normalisePluginFile (file);
        const auto dir = plugin.getParentDirectory();
        if (! dir.isDirectory() || dir == pluginFolder()) return;
        juce::XmlElement xml ("ZODIAK_PLUGIN_FOLDER");
        xml.setAttribute ("path", dir.getFullPathName());
        folderPreset().getParentDirectory().createDirectory();
        xml.writeTo (folderPreset());
    }

    juce::File normalisePluginFile (const juce::File& chosen)
    {
        // <name>.vst3\Contents\<arch>\<name>.vst3: the bundle is the plugin
        for (auto f = chosen.getParentDirectory(); f.exists() && f != f.getParentDirectory(); f = f.getParentDirectory())
            if (f.isDirectory() && f.hasFileExtension ("vst3"))
                return f;
        return chosen;
    }

    //==========================================================================
    //  Sonify's own plugins
    //==========================================================================
    namespace
    {
        bool isZodiak (const Internals::Entry& e)
        {
            return e.id.endsWithIgnoreCase ("zodiak");
        }
    }

    std::vector<std::pair<juce::String, juce::String>> sonifyPlugins (bool instruments)
    {
        std::vector<std::pair<juce::String, juce::String>> list;
        for (const auto& e : Internals::entries())
        {
            if (isZodiak (e) || e.make == nullptr) continue;
            const bool isInstrument = e.role == Plugin::Role::instrument;
            if (isInstrument == instruments)
                list.emplace_back (e.id, e.name);
        }
        return list;
    }

    bool isSonifyPlugin (const juce::String& id)
    {
        const auto* e = Internals::find (id);
        return e != nullptr && ! isZodiak (*e) && e->make != nullptr;
    }

    void pickSource (bool wantInstrument, juce::Component* near, std::function<void (const Source&)> picked)
    {
        auto openChooser = [picked]
        {
            static std::unique_ptr<juce::FileChooser> chooser;
            chooser = std::make_unique<juce::FileChooser> (loadTitle(), pluginFolder(), loadPatterns());
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [picked] (const juce::FileChooser& fc)
                {
                    const auto file = fc.getResult();
                    if (file == juce::File()) return;
                    rememberPluginFolder (file);
                    picked (Source (normalisePluginFile (file)));
                });
        };

        const auto own = sonifyPlugins (wantInstrument);
        if (own.empty()) { openChooser(); return; }

        juce::PopupMenu menu;
        menu.addSectionHeader ("Sonify");
        for (size_t i = 0; i < own.size(); ++i)
            menu.addItem ((int) i + 1, own[i].second);
        menu.addSeparator();
        menu.addItem (1000, loadTitle() + "...");

        auto options = juce::PopupMenu::Options();
        if (near != nullptr) options = options.withTargetComponent (near);

        menu.showMenuAsync (options, [own, picked, openChooser] (int result)
        {
            if (result == 1000)                                 openChooser();
            else if (result >= 1 && result <= (int) own.size()) picked (Source::sonify (own[(size_t) result - 1].first));
        });
    }

    namespace
    {
        void loadSonifyPlugin (const juce::String& id, double sampleRate, int maxBlock,
                               std::function<void (std::unique_ptr<Hosted>, const juce::String&)> done)
        {
            const auto* e = Internals::find (id);
            if (e == nullptr || isZodiak (*e) || e->make == nullptr)
            {
                done (nullptr, "This Sonify has no plugin " + id.fromLastOccurrenceOf (".", false, false) + ".");
                return;
            }

            auto processor = e->make();
            if (processor == nullptr)
            {
                done (nullptr, e->name + " did not open.");
                return;
            }

            processor->enableAllBuses();
            auto h = std::make_unique<Hosted>();
            h->instance = std::move (processor);
            h->sonifyId = e->id;
            h->name = e->name;
            h->prepare (sampleRate, maxBlock);
            done (std::move (h), {});
        }
    }

    void loadPlugin (const Source& source, double sampleRate, int maxBlock, bool wantInstrument,
                     std::function<void (std::unique_ptr<Hosted>, const juce::String&)> done)
    {
        if (source.isSonify())
        {
            loadSonifyPlugin (source.sonifyId, sampleRate, maxBlock, std::move (done));
            return;
        }

        const auto file = normalisePluginFile (source.file);
        auto& fm = formats();

        juce::OwnedArray<juce::PluginDescription> found;
        for (auto* format : fm.getFormats())
            if (format->fileMightContainThisPluginType (file.getFullPathName()))
                format->findAllTypesForFile (found, file.getFullPathName());

        if (found.isEmpty())
        {
            done (nullptr, file.getFileName() + " is not a plugin Zodiak can load.");
            return;
        }

        // The first of the kind asked for (a shell holding several: the first one)
        juce::PluginDescription desc = *found.getFirst();
        for (auto* d : found)
            if (d->isInstrument == wantInstrument) { desc = *d; break; }

        fm.createPluginInstanceAsync (desc, sampleRate, maxBlock,
            [file, desc, sampleRate, maxBlock, done] (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error)
            {
                if (instance == nullptr)
                {
                    done (nullptr, error.isNotEmpty() ? error : desc.name + " did not open.");
                    return;
                }

                instance->enableAllBuses();
                auto h = std::make_unique<Hosted>();
                h->instance = std::move (instance);
                h->file = file;
                h->name = desc.name;
                h->prepare (sampleRate, maxBlock);
                done (std::move (h), {});
            });
    }

    juce::String saveState (const Hosted& h)
    {
        if (h.instance == nullptr) return {};
        juce::MemoryBlock mb;
        h.instance->getStateInformation (mb);
        return mb.toBase64Encoding();
    }

    void restoreState (Hosted& h, const juce::String& base64)
    {
        if (h.instance == nullptr || base64.isEmpty()) return;
        juce::MemoryBlock mb;
        if (mb.fromBase64Encoding (base64) && mb.getSize() > 0)
            h.instance->setStateInformation (mb.getData(), (int) mb.getSize());
    }
}
