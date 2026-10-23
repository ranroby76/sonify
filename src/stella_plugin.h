// D:\workspace\Fanan_web_hub\src\stella_plugin.h
//
// What every format of a plugin exported from the Plugins Creator Studio shares
// (stella_clap.cpp, stella_vst3.cpp, stella_au.cpp): the Stella runtime with the plugin's
// parameters and their values, the live data its window shows, its state as text, and running
// a stretch of audio with the host's tempo and transport. No JUCE: compiled on the user's
// computer by Export (PluginExporter) with the project's modules.
//
// Parameters keep stable ids in every format (a hash of "module.param"), so automation and
// saved songs survive rebuilds. State is "id=value" lines, so it survives parameters being
// added or reordered.
//
// A sampler's sample library (stella_blob.h): its blobs, by their files' names (in
// stella_plugin_info.h), found on this computer when the plugin starts, and read from the disk
// as they play.
//
// Protection (stella_licence.h): a protected export (STELLA_PROTECTED, with STELLA_LICENCE_NATIVE)
// finds its license.soni beside it (or inside it: an Audio Unit's Resources), checks this
// computer's key file against its formula, and plays the demo until it's registered. Its window's
// Registration (gui::Host's licence calls) registers it, and any protected sample library it
// plays (stella_blob.h), whose demo the runtime plays meanwhile. Exported plugins never send MIDI
// (a MIDI FX is one of Sonify's MIDI devices, not an export), so their demo is always the sound's.

#pragma once

#include "stella_blob.h"
#include "stella_gui.h"
#include "stella_plugin_info.h"
#include "stella_runtime.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if defined (__x86_64__) || defined (_M_X64) || defined (__i386__) || defined (_M_IX86)
 #include <xmmintrin.h>
 #define STELLA_HAS_SSE 1
#endif

// A protected export checks its own registration; any export (with the computer's side of
// stella_licence.h) registers the protected sample libraries it plays.
#if STELLA_PROTECTED && STELLA_LICENCE_NATIVE && ! defined (__wasm__)
 #define STELLA_PLUGIN_PROTECTED 1
#else
 #define STELLA_PLUGIN_PROTECTED 0
#endif

#if STELLA_LICENCE_NATIVE && ! defined (__wasm__)
 #define STELLA_PLUGIN_LIBRARY_LICENCES 1
#else
 #define STELLA_PLUGIN_LIBRARY_LICENCES 0
#endif

namespace stella::plugin
{
    //==========================================================================
    /** FNV-1a: the same number for the same text, build after build. */
    inline std::uint32_t hashOf (const std::string& text) noexcept
    {
        std::uint32_t hash = 2166136261u;

        for (const auto c : text)
        {
            hash ^= (std::uint8_t) c;
            hash *= 16777619u;
        }

        return hash;
    }

    /** A number as text with '.' for its decimal point, whatever the host's locale (snprintf
        writes the locale's: "0,5" under a German one). */
    inline std::string numberText (double value)
    {
        char text[64];
        std::snprintf (text, sizeof (text), "%.9g", value);
        std::string result;
        bool point = false;

        for (const char* c = text; *c != 0; ++c)
        {
            const bool plain = (*c >= '0' && *c <= '9') || *c == '-' || *c == '+' || *c == 'e' || *c == 'E';

            if (plain)                result += *c;
            else if (! point)         result += '.';   // the locale's point (maybe several bytes): one '.'
            point = point || ! plain;
        }

        return result;
    }

    /** A number from text, whatever the host's locale (atof follows it, and stops at a '.' under a
        German one): '.' or ',' (written by older plugins under such a locale) as the point. 0 if
        there's no number, as atof (found, when given, says whether there was one). */
    inline double numberFrom (const char* s, bool* found = nullptr) noexcept
    {
        if (found != nullptr)
            *found = false;

        if (s == nullptr)
            return 0.0;

        while (*s == ' ' || *s == '\t')
            ++s;

        const bool negative = *s == '-';

        if (*s == '-' || *s == '+')
            ++s;

        std::uint64_t mantissa = 0;
        int exponent = 0, digits = 0;
        bool point = false;

        for (;; ++s)
        {
            if (*s >= '0' && *s <= '9')
            {
                ++digits;

                if (mantissa < 100000000000000000ull)   // 17 digits: the rest can't change a float
                {
                    mantissa = mantissa * 10u + (std::uint64_t) (*s - '0');
                    exponent -= point ? 1 : 0;
                }
                else
                {
                    exponent += point ? 0 : 1;
                }
            }
            else if ((*s == '.' || *s == ',') && ! point)
            {
                point = true;
            }
            else
            {
                break;
            }
        }

        if (digits == 0)
            return 0.0;

        if (found != nullptr)
            *found = true;

        if (*s == 'e' || *s == 'E')
        {
            const char* e = s + 1;
            const bool down = *e == '-';

            if (*e == '-' || *e == '+')
                ++e;

            if (*e >= '0' && *e <= '9')
            {
                int power = 0;

                for (; *e >= '0' && *e <= '9'; ++e)
                    power = power < 10000 ? power * 10 + (*e - '0') : power;

                exponent += down ? -power : power;
            }
        }

        // Dividing by an exact power of ten (exact up to 1e22) rounds once, so "0.1" is the float it was.
        double tens = 1.0;

        for (int k = exponent < 0 ? -exponent : exponent; k > 0 && tens < 1.0e300; --k)
            tens *= 10.0;

        const double value = exponent < 0 ? (double) mantissa / tens : (double) mantissa * tens;
        return negative ? -value : value;
    }

    /** Tiny numbers (denormals) are treated as zero while the plugin runs: they'd make the
        processor crawl in decaying filters and reverbs. The host's setting comes back after. */
    struct NoDenormals
    {
       #if STELLA_HAS_SSE
        NoDenormals() : saved (_mm_getcsr())   { _mm_setcsr (saved | 0x8040); }
        ~NoDenormals()                         { _mm_setcsr (saved); }
        unsigned int saved;
       #elif defined (__aarch64__)
        NoDenormals()
        {
            __asm__ __volatile__ ("mrs %0, fpcr" : "=r" (saved));
            const std::uint64_t flushed = saved | (1ull << 24);
            __asm__ __volatile__ ("msr fpcr, %0" : : "r" (flushed));
        }

        ~NoDenormals()    { __asm__ __volatile__ ("msr fpcr, %0" : : "r" (saved)); }
        std::uint64_t saved = 0;
       #endif
    };

    /** The host's tempo and position at the start of a block (120 and stopped when it says nothing). */
    struct Transport
    {
        double tempo = 120.0;
        double beat = 0.0;       // quarter notes since the song's start
        double barStart = 0.0;   // where the bar began, in quarter notes
        bool playing = false;
        int numerator = 4, denominator = 4;
    };

    //==========================================================================
    class Core : public gui::Host
    {
    public:
        static constexpr int scopeSize = 4096;

        /** What the window's Registration lists (gui::Host's LicenceItem). */
        using LicenceItems = decltype (std::declval<gui::Host&>().licenceItems());

        /** Creates the modules: false if the plugin can't run. */
        bool build()
        {
            built = runtime.build();

            if (! built)
                return false;

            const auto count = runtime.getNumParams();
            values = std::make_unique<std::atomic<float>[]> ((size_t) (count > 0 ? count : 1));

            for (int i = 0; i < count; ++i)
            {
                params.push_back (runtime.getParam (i));
                values[(size_t) i].store (params.back().def);
            }

            setUpLive();
            setUpLibrary();
            setUpLicence();
            return true;
        }

        bool isBuilt() const noexcept    { return built; }

        /** Before playing, and again when the sample rate or the largest block changes. */
        bool prepare (double newSampleRate, int largestBlock)
        {
            sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
            maxFrames = largestBlock > 0 ? std::min (largestBlock, 8192) : 512;
            const blob::Library::Scope scope (library);
            prepared = built && runtime.prepare (sampleRate, maxFrames);
            return prepared;
        }

        bool isPrepared() const noexcept      { return prepared; }
        int getMaxFrames() const noexcept     { return maxFrames; }
        double getSampleRate() const noexcept { return sampleRate; }

        void reset()
        {
            if (built)
            {
                const blob::Library::Scope scope (library);
                runtime.reset();
            }

            notes.clear();
        }

        //======================================================================
        int numParams() const noexcept                     { return (int) params.size(); }
        const ParamInfo& paramInfo (int index) const       { return params[(size_t) index]; }
        float valueOf (int index) const noexcept           { return values[(size_t) index].load (std::memory_order_relaxed); }

        float clampValue (int index, float value) const noexcept
        {
            const auto& p = params[(size_t) index];
            return std::min (p.max, std::max (p.min, value));
        }

        /** The host (or a preset, or the state) sets a value. */
        void storeValue (int index, float value) noexcept
        {
            if (index >= 0 && index < (int) params.size())
                values[(size_t) index].store (clampValue (index, value), std::memory_order_relaxed);
        }

        int indexOfParam (const std::string& id) const noexcept
        {
            for (size_t i = 0; i < params.size(); ++i)
                if (params[i].id == id)
                    return (int) i;

            return -1;
        }

        /** A value as text, its unit after it (whole numbers on a wide range). */
        std::string valueText (int index, double value) const
        {
            const auto& p = params[(size_t) index];
            const auto decimals = (p.max - p.min) >= 100.0f ? 0 : ((p.max - p.min) >= 10.0f ? 1 : 2);
            char text[128];
            std::snprintf (text, sizeof (text), "%.*f%s%s", decimals, value, p.unit.empty() ? "" : " ", p.unit.c_str());
            return text;
        }

        //======================================================================
        // The audio thread.
        Shared& shared() noexcept    { return runtime.getShared(); }

        /** The window keyboard's notes, at the start of a block: added to events (count of them
            so far, at most capacity). */
        std::uint32_t takeWindowNotes (Event* events, std::uint32_t count, std::uint32_t capacity) noexcept
        {
            int note = 0, velocity = 0;

            // Only as many as there's room for: the rest wait for the next block (a lost release would hang).
            while (count < capacity && notes.pop (note, velocity))
                events[count++] = { 0, (std::uint8_t) (velocity > 0 ? 0x90 : 0x80), (std::uint8_t) note, (std::uint8_t) velocity, 0 };

            return count;
        }

        /** Runs frames (up to getMaxFrames()) from offset in the host's buffers. The stretch's
            events are already in shared().events, numEvents of them, timed from its start.
            transport: the host's at the start of the whole block. */
        void run (const float* const* inputs, int inputChannels, float* const* outputs, int outputChannels,
                  int offset, int frames, std::uint32_t numEvents, const Transport& transport) noexcept
        {
            auto& s = runtime.getShared();
            s.numEvents = std::min (numEvents, (std::uint32_t) maxEvents);

            // What the keyboard shows down: notes from anywhere.
            for (std::uint32_t e = 0; e < s.numEvents; ++e)
            {
                const auto type = s.events[e].status & 0xf0;

                if (type == 0x90 || type == 0x80)
                    notes.sounding (s.events[e].data1 & 0x7f, type == 0x90 && s.events[e].data2 > 0);
            }

            for (size_t i = 0; i < params.size(); ++i)
                s.params[i] = values[i].load (std::memory_order_relaxed);

            for (int c = 0; c < 2; ++c)
            {
                const float* in = inputs != nullptr && inputChannels > 0 ? inputs[c < inputChannels ? c : 0] : nullptr;

                if (in != nullptr)
                    std::memcpy (s.in[c], in + offset, sizeof (float) * (size_t) frames);
                else
                    std::memset (s.in[c], 0, sizeof (float) * (size_t) frames);
            }

            // Where this stretch starts in the song: the block's start, moved on while it plays.
            const auto tempo = transport.tempo > 1.0 && transport.tempo < 2000.0 ? transport.tempo : 120.0;
            const auto beat = transport.beat + (transport.playing ? (double) offset * tempo / (60.0 * sampleRate) : 0.0);
            const auto whole = std::floor (beat);
            s.transport.tempo = (float) tempo;
            s.transport.beatWhole = (std::int32_t) whole;
            s.transport.beatFraction = (float) (beat - whole);
            s.transport.barStart = (float) transport.barStart;
            s.transport.playing = transport.playing ? 1 : 0;
            s.transport.numerator = transport.numerator > 0 ? transport.numerator : 4;
            s.transport.denominator = transport.denominator > 0 ? transport.denominator : 4;

            {
                const blob::Library::Scope scope (library);   // what the modules' stella_blob_* calls reach
                runtime.process (frames);
            }

            captureLive ((std::uint32_t) frames);

            for (int c = 0; c < outputChannels; ++c)
                if (outputs[c] != nullptr)
                    std::memcpy (outputs[c] + offset, s.out[c < 2 ? c : 1], sizeof (float) * (size_t) frames);
        }

        //======================================================================
        // State: one "id=value" line per parameter.
        std::string saveState() const
        {
            std::string text = "stella-state 1\n";

            for (size_t i = 0; i < params.size(); ++i)
                text += params[i].id + "=" + numberText ((double) values[i].load (std::memory_order_relaxed)) + "\n";

            return text;
        }

        bool loadState (const std::string& text)
        {
            if (text.rfind ("stella-state", 0) != 0)
                return false;

            size_t start = 0;

            while (start < text.size())
            {
                auto end = text.find ('\n', start);

                if (end == std::string::npos)
                    end = text.size();

                const auto line = text.substr (start, end - start);
                const auto equals = line.find ('=');

                if (equals != std::string::npos)
                    storeValue (indexOfParam (line.substr (0, equals)), (float) numberFrom (line.c_str() + equals + 1));

                start = end + 1;
            }

            return true;
        }

        //======================================================================
        // What the window needs (gui::Host), on the main thread. Edits are the format's own.
        int findParam (const char* id) override    { return id != nullptr ? indexOfParam (id) : -1; }

        float getValue (int param) override
        {
            return param >= 0 && param < (int) params.size() ? values[(size_t) param].load (std::memory_order_relaxed) : 0.0f;
        }

        void getRange (int param, float& min, float& max, float& def, float& skew) override
        {
            if (param < 0 || param >= (int) params.size())
                return;

            min = params[(size_t) param].min;
            max = params[(size_t) param].max;
            def = params[(size_t) param].def;
            skew = params[(size_t) param].skew;
        }

        float takeLevel (int slot, bool rms) override
        {
            if (slot < 0 || slot >= (int) live.size())
                return 0.0f;

            const auto& source = live[(size_t) slot];

            if (source.display >= 0)
                return displayValues[(size_t) source.display].load (std::memory_order_relaxed);

            if (source.tap < 0)
                return 0.0f;

            return rms ? tapRms[(size_t) source.tap].load (std::memory_order_relaxed)
                       : tapPeak[(size_t) source.tap].exchange (0.0f, std::memory_order_relaxed);
        }

        void readScope (int slot, float* destination, int numSamples) override
        {
            const auto scope = slot >= 0 && slot < (int) live.size() ? live[(size_t) slot].scope : -1;
            const auto count = std::min (numSamples, scopeSize);
            const auto write = scope >= 0 ? scopeWrite[(size_t) scope].load (std::memory_order_acquire) : 0u;

            for (int i = 0; i < numSamples; ++i)
            {
                const auto age = (std::uint32_t) (count - i);
                destination[i] = scope >= 0 && i < count && age <= write ? scopeRing[(size_t) scope][(write - age) % (std::uint32_t) scopeSize] : 0.0f;
            }
        }

        void playNote (int note, float velocity) override
        {
            notes.push (note, velocity > 0.0f ? std::min (127, std::max (1, (int) std::lround (velocity * 127.0f))) : 0);
        }

        bool isNoteDown (int note) override    { return notes.isDown (note); }

        //======================================================================
        // Registration (the window's Registration, on the main thread): the plugin itself when it's
        // protected, then the protected sample libraries it plays.
        LicenceItems licenceItems() override
        {
            LicenceItems items;

           #if STELLA_PLUGIN_PROTECTED
            // Registered meanwhile in another of its windows (or another plugin): checked again.
            if (registrations.load() != registrationsSeen)
                checkRegistration();

            items.push_back ({});
            items.back().name = soniFound && ! soni.name.empty() ? soni.name : std::string (info::name);
            items.back().registered = registered.load (std::memory_order_relaxed);
           #endif

           #if STELLA_PLUGIN_LIBRARY_LICENCES
            for (const auto& library : blob::licencesInUse())
            {
                items.push_back ({});
                items.back().name = ! library.name.empty() ? library.name : library.product;
                items.back().registered = library.registered;
            }
           #endif

            return items;
        }

        std::string licenceComputerId() override
        {
           #if STELLA_PLUGIN_LIBRARY_LICENCES
            return licence::grouped (licence::native::computerId());
           #else
            return {};
           #endif
        }

        std::string licenceRegister (const std::string& serial) override
        {
            std::string why;

           #if STELLA_PLUGIN_PROTECTED
            if (! registered.load (std::memory_order_relaxed))
            {
                why = licence::native::registerSerial (soni, serial);   // soni not found: says license.soni is missing

                if (why.empty())
                {
                    ++registrations;
                    checkRegistration();
                    return {};
                }
            }
           #endif

           #if STELLA_PLUGIN_LIBRARY_LICENCES
            bool libraryWaits = false;

            for (const auto& library : blob::licencesInUse())
                libraryWaits = libraryWaits || ! library.registered;

            if (libraryWaits)
            {
                const auto libraryWhy = blob::registerSerial (serial);

                if (libraryWhy.empty())
                {
                    ++registrations;
                    return {};
                }

                if (why.empty())
                    why = libraryWhy;
            }
           #endif

            (void) serial;
            return why.empty() ? std::string ("Nothing here needs registering.") : why;
        }

        /** Whether this computer is registered for the plugin (a plugin that isn't protected: yes). */
        bool isRegistered() const noexcept
        {
           #if STELLA_PLUGIN_PROTECTED
            return registered.load (std::memory_order_relaxed);
           #else
            return true;
           #endif
        }

        gui::Notes notes;   // the window keyboard's notes, and the notes sounding

    private:
        /** Its license.soni (the creator's demo times in it), and whether it's registered here. */
        void setUpLicence()
        {
           #if STELLA_PLUGIN_PROTECTED
            soniFound = licence::native::findSoni (licence::product(), soni);

            if (soniFound)
                runtime.setDemoTimes (soni.playSeconds, soni.silentSeconds);

            checkRegistration();
           #endif
        }

       #if STELLA_PLUGIN_PROTECTED
        void checkRegistration()
        {
            registrationsSeen = registrations.load();
            const auto now = soniFound && licence::native::isRegistered (soni);
            registered.store (now, std::memory_order_relaxed);
            runtime.setLicensed (now);
        }
       #endif

        /** What one watch slot reads: a tap (its level), a module display, or a scope. */
        struct LiveSource
        {
            int tap = -1, display = -1, scope = -1;
        };

        void setUpLive()
        {
            auto& s = runtime.getShared();

            for (int slot = 0; slot < gui::numWatchSlots(); ++slot)
            {
                const auto watch = gui::watchOf (slot);
                LiveSource source;
                std::int32_t node = 0, port = 0;

                if (watch.source != nullptr && watch.isDisplay)
                {
                    source.display = runtime.findDisplay (watch.source);
                }
                else if (watch.source != nullptr && runtime.findSignal (watch.source, node, port))
                {
                    if (watch.level && numTaps < maxTaps)
                    {
                        s.taps[numTaps] = { node, port, 0.0f, 0.0f };
                        source.tap = numTaps++;
                    }

                    if (watch.samples && numScopes < maxScopes)
                    {
                        s.scopeNode[numScopes] = node;
                        s.scopePort[numScopes] = port;
                        scopeRing[(size_t) numScopes].assign ((size_t) scopeSize, 0.0f);
                        source.scope = numScopes++;
                    }
                }

                live.push_back (source);
            }

            s.numTaps = (std::uint32_t) numTaps;
        }

        /** The sample library's instruments, each with its blob as found on this computer. */
        void setUpLibrary()
        {
            std::vector<blob::Entry> entries;

            for (int i = 0; i < info::numLibrary; ++i)
            {
                blob::Entry entry;
                entry.name = info::library[i].name;
                entry.path = blob::findPluginBlob (info::library[i].file, info::vendor[0] != 0 ? info::vendor : "Made in Sonify", info::name);
                entries.push_back (entry);
            }

            if (! entries.empty())
                library.setEntries (entries);
        }

        /** After each stretch: the levels, samples and displays the window shows. */
        void captureLive (std::uint32_t frames) noexcept
        {
            auto& s = runtime.getShared();

            for (int t = 0; t < numTaps; ++t)
            {
                auto held = tapPeak[(size_t) t].load (std::memory_order_relaxed);
                const auto peak = s.taps[t].peak;

                while (peak > held && ! tapPeak[(size_t) t].compare_exchange_weak (held, peak, std::memory_order_relaxed)) {}

                tapRms[(size_t) t].store (s.taps[t].rms, std::memory_order_relaxed);
            }

            for (int sc = 0; sc < numScopes; ++sc)
            {
                auto write = scopeWrite[(size_t) sc].load (std::memory_order_relaxed);

                for (std::uint32_t i = 0; i < frames; ++i)
                    scopeRing[(size_t) sc][(write + i) % (std::uint32_t) scopeSize] = s.scope[sc][i];

                scopeWrite[(size_t) sc].store (write + frames, std::memory_order_release);
            }

            for (const auto& source : live)
                if (source.display >= 0 && source.display < maxDisplays)
                    displayValues[(size_t) source.display].store (s.displays[source.display], std::memory_order_relaxed);
        }

        Runtime runtime;
        blob::Library library;
        bool built = false, prepared = false;
        double sampleRate = 48000.0;
        int maxFrames = 512;

        std::vector<ParamInfo> params;
        std::unique_ptr<std::atomic<float>[]> values;

        std::vector<LiveSource> live;
        int numTaps = 0, numScopes = 0;
        std::array<std::atomic<float>, maxTaps> tapPeak {}, tapRms {};
        std::array<std::vector<float>, maxScopes> scopeRing;
        std::array<std::atomic<std::uint32_t>, maxScopes> scopeWrite {};
        std::array<std::atomic<float>, maxDisplays> displayValues {};

       #if STELLA_PLUGIN_PROTECTED
        licence::Soni soni;                          // invalid until found
        bool soniFound = false;
        std::atomic<bool> registered { false };
        int registrationsSeen = 0;
       #endif

        /** Registrations made in this process (any of its plugins' windows): the others check again. */
        static inline std::atomic<int> registrations { 0 };
    };
}
