// D:\workspace\Fanan_web_hub\src\stella_blob.h
//
// Blobs: a sampled instrument packed in one file (.blob), played from the disk as it sounds, so
// an instrument of many gigabytes plays with little memory (as Kontakt's libraries do). The
// Sampling Studio's SFZ to Blob makes them (BlobMaker.cpp) from an SFZ and its samples; plugins
// made in the Plugins Creator Studio play them through stella::blob (stella_api.h): in Sonify
// (WasmPlugin plays them for the plugin's sandbox) and in exported plugins (compiled in).
//
// The file (little-endian):
//   head, 256 bytes, plain:
//      0  "SNFYBLOB"            8  version (1)          12 the head's size (256)
//     16  key kind (0 open)    20  flags (bit 0: encrypted)   24 nonce (12 bytes)
//     40  index offset         48  index size          56 samples' offset     64 samples' size
//     72  product (64 bytes, UTF-8: a locked blob's product; empty: open)
//    136  the instrument's name (96 bytes, UTF-8, cut short when longer)
//   index, encrypted: UTF-8 text, a line each, a tab between fields:
//      sonify-blob <tab> 1
//      name <tab> <the instrument's name>
//      sample <tab> frames=.. <tab> rate=.. <tab> channels=1|2 <tab> format=s16|s24|f32 <tab> offset=.. <tab> name=..
//      region <tab> sample=<n> <tab> <opcode>=<value> ...   every opcode the SFZ gave it (flattened:
//          its <global>, <master> and <group> in it), keys as MIDI note numbers
//   samples, encrypted: each sample's frames, interleaved, from its offset (16-byte aligned)
//
// Everything after the head is encrypted with ChaCha20 (RFC 8439), its keystream counted from the
// head's end, so any part of the file is read and decrypted on its own. An open blob's key is
// Sonify's own: every blob reader opens it.
//
// Protected ("locked") blobs: key kind 1, the head's product field names the product (a Plugins
// Creator Studio project's, or a license.soni's: stella_licence.h). Their key is the product's:
//     SHA-256 of  "sonify-blob-key-1" 0x00  <product id, UTF-8> 0x00  A  B  M
//     (the product's serial formula, as license.soni has it: each number 8 bytes, little-endian)
// so a locked blob opens with the license.soni of its product, which SFZ to Blob writes in the
// same folder as the blobs (one folder, one product). Reading one (STELLA_LICENCE_NATIVE: Sonify,
// exported plugins) finds license.soni in the blob's own folder; without it, it can't load.
// Each protected library in use is tracked: registered on this computer or not
// (licencesInUse, registerSerial); stella_blob_demo says 1 while one in use isn't, with its demo's
// times, and the plugin plays its demo. A key given with the entry (Entry::key) opens a locked
// blob too, without any of that.
//
// Playing: when an instrument loads, the start of each sample (its head) is read into memory,
// and each loop with what follows it; the rest is read from the disk while it plays, ahead of the
// voice, by one thread for every plugin in the program (another loads the instruments). A voice
// plays a region at a speed (its pitch), round its loop (with its crossfade), and on past it once
// it's released (loop_sustain). Loops end where the region's loop_end says (that frame isn't
// played: the Sonify Sampler's and Sampler Arena's way).
//
// Plain C++17, no JUCE: the same code in Sonify and in exported plugins. STELLA_BLOB_IMPLEMENTATION
// in one file compiles the engine; STELLA_BLOB_PLUGIN there too gives an exported plugin the
// stella_blob_* functions its modules call, and the folders it finds its blobs in.
// STELLA_LICENCE_NATIVE (protected blobs read with their license.soni) must be defined before
// stella_licence.h is first included in that file: the engine includes it, and it's included once.

#ifndef STELLA_BLOB_H_INCLUDED
#define STELLA_BLOB_H_INCLUDED

#include "stella_api.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace stella::licence
{
    struct Soni;   // stella_licence.h: what license.soni holds
}

namespace stella::blob
{
    constexpr std::uint32_t fileVersion = 1;
    constexpr std::uint32_t headBytes = 256;
    constexpr const char* fileExtension = ".blob";

    constexpr int maxInstruments = 128;      // in one plugin's library
    constexpr int maxVoices = 128;           // sounding at once, in one plugin
    constexpr int ringFrames = 16384;        // what each voice keeps read ahead of it

    enum KeyKind : std::uint32_t { openKey = 0, productKey = 1 };

    /** How a sample's frames are kept: 16 or 24-bit PCM, or 32-bit float. */
    enum class Format : int { s16 = 0, s24 = 1, f32 = 2 };

    inline int bytesOf (Format format) noexcept           { return format == Format::s16 ? 2 : (format == Format::s24 ? 3 : 4); }
    inline const char* nameOf (Format format) noexcept    { return format == Format::s16 ? "s16" : (format == Format::s24 ? "s24" : "f32"); }

    //==========================================================================
    /** ChaCha20 (RFC 8439): the keystream XORed at any position of the stream. */
    class Cipher
    {
    public:
        void setKey (const std::uint8_t key[32], const std::uint8_t nonce[12]) noexcept;
        void apply (std::uint8_t* data, std::size_t size, std::uint64_t position) const noexcept;

    private:
        std::uint32_t state[16] {};
    };

    /** Sonify's own key: the one every open blob is encrypted with. */
    void openKeyBytes (std::uint8_t key[32]) noexcept;

    /** A file by its UTF-8 name (on Windows too). */
    std::FILE* openFile (const std::string& path, bool forWriting);
    bool seekFile (std::FILE* file, std::uint64_t position);
    bool fileExists (const std::string& path);

    //==========================================================================
    // The index: what the SFZ to Blob writes, and every reader reads.
    struct SampleEntry
    {
        std::string name;                    // its file's name, as the SFZ had it
        std::int64_t frames = 0;
        double rate = 44100.0;
        int channels = 1;                    // 1 or 2
        Format format = Format::s16;
        std::uint64_t offset = 0;            // from the start of the samples
    };

    struct RegionEntry
    {
        int sample = -1;
        std::vector<std::pair<std::string, std::string>> opcodes;   // in the order they came, later ones win
    };

    struct Index
    {
        std::string name;
        std::vector<SampleEntry> samples;
        std::vector<RegionEntry> regions;
    };

    std::string indexText (const Index& index);
    bool parseIndex (const std::string& text, Index& index, std::string& error);

    /** A blob's head. */
    struct Head
    {
        std::uint32_t version = fileVersion, keyKind = openKey, flags = 1;
        std::uint8_t nonce[12] {};
        std::uint64_t indexOffset = 0, indexSize = 0, dataOffset = 0, dataSize = 0;
        std::string product, name;
    };

    bool readHead (std::FILE* file, Head& head, std::string& error);

    /** What's in a blob, read from its head and index (for lists). A locked blob without its key
        (nor its license.soni beside it) says its name and product, and locked. */
    struct Summary
    {
        std::string name, product;
        int regions = 0, samples = 0;
        std::uint64_t bytes = 0;
        bool locked = false;
        std::string licenceName;             // a protected one opened by the license.soni beside it: its product's name
    };

    bool summarise (const std::string& path, Summary& summary, std::string& error, const std::uint8_t* key = nullptr);

    //==========================================================================
    // Protected blobs (see the top of this file).

    /** A product's blob key, from its license.soni (its id and serial formula). */
    void licenceKey (const licence::Soni& soni, std::uint8_t key[32]) noexcept;

    /** A protected library being played (its blobs loaded now), as the Registration window lists it. */
    struct LicenceInfo
    {
        std::string product, name;
        bool registered = false;
        float playSeconds = 10, silentSeconds = 2;
    };

    /** The protected libraries loaded now (in this program: every plugin's), each once. */
    std::vector<LicenceInfo> licencesInUse();

    /** Tries a serial (typed or pasted) on every protected library in use that isn't registered:
        the first it fits keeps it (its key file) and plays fully from then on. "" when one took
        it, else why not. */
    std::string registerSerial (const std::string& serial);

    /** A protected library in use: its license.soni (for Sonify's Registration window). */
    bool licenceOf (const std::string& product, licence::Soni& soni);

    /** Reads the key files again for the libraries in use that aren't registered (the product
        was registered elsewhere: its plugin's window, Sonify's). The loading thread does it every
        2 seconds by itself. */
    void recheckLicences();

    /** 1 while any blob of a protected library this computer isn't registered for is loaded (in
        this program), with that library's demo times; else 0. Lock-free, never blocks: any thread. */
    int demoInUse (float* playSeconds, float* silentSeconds) noexcept;

    //==========================================================================
    /** Writes a blob: open() with the index (every sample's frames, rate, channels and format
        known; their offsets are worked out here), then each sample's frames in order with
        write(), then finish(). */
    class Writer
    {
    public:
        Writer() = default;
        ~Writer();

        /** key: null for an open blob (Sonify's key); product: a locked blob's product. */
        bool open (const std::string& path, Index& index, std::string& error,
                   const std::uint8_t* key = nullptr, const std::string& product = {});

        /** The next frames of the sample being written (right: null for a mono one). */
        bool write (const float* left, const float* right, int frames, std::string& error);

        bool finish (std::string& error);

        /** Closes it and removes what was written. */
        void abandon();

        std::uint64_t getBytes() const noexcept    { return position; }

    private:
        bool put (const std::uint8_t* data, std::size_t size, std::string& error);   // encrypted, at position
        bool pad (std::uint64_t to, std::string& error);

        std::FILE* file = nullptr;
        std::string path;
        Cipher cipher;
        std::vector<SampleEntry> samples;
        std::uint64_t dataOffset = 0, position = 0;
        std::size_t current = 0;
        std::int64_t framesDone = 0;
        std::vector<std::uint8_t> buffer;
    };

    //==========================================================================
    class Instrument;

    /** One instrument of a plugin's library: its name, and the file the host found for it
        (empty: missing). A locked blob's key comes with it. */
    struct Entry
    {
        std::string name, path;
        std::uint8_t key[32] {};
        bool hasKey = false;
    };

    /** A plugin's sample library: its instruments, loaded as its modules use them, and the voices
        playing them. One per plugin instance.

        The message thread sets its instruments. The audio thread calls the rest between begin()
        and end() (Scope does both, and makes it the library the stella_blob_* functions reach):
        never blocking, never allocating. */
    class Library
    {
    public:
        Library();
        ~Library();

        Library (const Library&) = delete;
        Library& operator= (const Library&) = delete;

        /** Message thread. The first one starts loading at once; the others when they're used. */
        void setEntries (const std::vector<Entry>& entries);

        /** Any thread: how an instrument is (State), and why it can't play when it can't. */
        int stateOf (int instrument) const noexcept;
        std::string problemOf (int instrument) const;

        //======================================================================
        // The audio thread
        void begin() noexcept;
        void end() noexcept;

        int count() const noexcept;
        int name (int instrument, char* text, int size) const noexcept;
        int state (int instrument) const noexcept;
        void use (int instrument) noexcept;
        int regions (int instrument) const noexcept;
        bool region (int instrument, int index, Region& info) const noexcept;
        float opcode (int instrument, int index, const char* opcodeName, float fallback) const noexcept;
        int find (int instrument, int note, int velocity, int kind, int* found, int maxFound) noexcept;
        int start (int instrument, int index, double fromFrame) noexcept;
        int render (int voice, float* left, float* right, int frames, float ratio) noexcept;
        void release (int voice) noexcept;
        void stop (int voice) noexcept;
        bool playing (int voice) const noexcept;

        /** 1 while one of this library's loaded instruments is a protected library's this
            computer isn't registered for (its demo's times then), else 0 (stella_blob_demo).
            Lock-free: the audio thread, every block. */
        int demo (float* playSeconds, float* silentSeconds) const noexcept;

        /** The library the stella_blob_* functions reach on this thread (exported plugins). */
        static Library* current() noexcept;

        struct Scope
        {
            explicit Scope (Library& library) noexcept;
            ~Scope();

            Scope (const Scope&) = delete;
            Scope& operator= (const Scope&) = delete;

        private:
            Library& library;
            Library* previous;
        };

        struct State;

    private:
        std::shared_ptr<State> d;
    };

    /** An exported plugin's blob, by its file's name: where it is on this computer, or empty.
        Looked for, in order: in the folders and files listed in <app data>/<vendor>/<plugin>/
        library.txt (a line each; Sonify writes the creator's own there when it exports), in
        <Documents>/<vendor>/<plugin>, and beside the plugin's file (and up to three folders above
        it: a bundle's). Compiled into exported plugins only (STELLA_BLOB_PLUGIN). */
    std::string findPluginBlob (const std::string& fileName, const char* vendor, const char* plugin);
}

#endif   // STELLA_BLOB_H_INCLUDED

//==============================================================================
//==============================================================================
// The engine: in one file only (it may include this header again after defining it).
#if defined (STELLA_BLOB_IMPLEMENTATION) && ! defined (STELLA_BLOB_IMPLEMENTED)
#define STELLA_BLOB_IMPLEMENTED 1

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <random>
#include <thread>

#if ! defined (_WIN32)
 #include <sys/types.h>
#endif

// Protected blobs' license.soni, and (STELLA_LICENCE_NATIVE) this computer's ID and key files.
#include "stella_licence.h"

namespace stella::blob
{
    namespace detail
    {
        inline std::uint32_t rotl (std::uint32_t v, int c) noexcept    { return (v << c) | (v >> (32 - c)); }

        inline void quarter (std::uint32_t& a, std::uint32_t& b, std::uint32_t& c, std::uint32_t& d) noexcept
        {
            a += b; d ^= a; d = rotl (d, 16);
            c += d; b ^= c; b = rotl (b, 12);
            a += b; d ^= a; d = rotl (d, 8);
            c += d; b ^= c; b = rotl (b, 7);
        }

        inline std::uint32_t get32 (const std::uint8_t* p) noexcept
        {
            return (std::uint32_t) p[0] | ((std::uint32_t) p[1] << 8) | ((std::uint32_t) p[2] << 16) | ((std::uint32_t) p[3] << 24);
        }

        inline std::uint64_t get64 (const std::uint8_t* p) noexcept
        {
            return (std::uint64_t) get32 (p) | ((std::uint64_t) get32 (p + 4) << 32);
        }

        inline void put32 (std::uint8_t* p, std::uint32_t v) noexcept
        {
            p[0] = (std::uint8_t) v; p[1] = (std::uint8_t) (v >> 8); p[2] = (std::uint8_t) (v >> 16); p[3] = (std::uint8_t) (v >> 24);
        }

        inline void put64 (std::uint8_t* p, std::uint64_t v) noexcept
        {
            put32 (p, (std::uint32_t) v);
            put32 (p + 4, (std::uint32_t) (v >> 32));
        }

        inline std::int64_t nowMs() noexcept
        {
            return (std::int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::steady_clock::now().time_since_epoch()).count();
        }

        /** Text without tabs or line ends, for the index. */
        inline std::string clean (const std::string& text)
        {
            std::string out (text);

            for (auto& c : out)
                if (c == '\t' || c == '\n' || c == '\r')
                    c = ' ';

            return out;
        }

        inline std::string numberText (double value)
        {
            char text[64];
            std::snprintf (text, sizeof (text), "%.10g", value);

            for (auto* c = text; *c != 0; ++c)
                if (*c == ',')
                    *c = '.';   // a locale with a decimal comma: the index always has a point

            return text;
        }

        /** A number as the index and SFZs write it ("44100", "-3.5", "1e-3"), whatever the
            program's locale says a decimal point is (atof reads "0.5" as 0 where it's a comma).
            What follows the number is ignored; no number: 0. */
        inline double number (const char* text) noexcept
        {
            if (text == nullptr)
                return 0.0;

            const auto* p = text;

            while (*p == ' ' || *p == '\t')
                ++p;

            bool negative = false;

            if (*p == '+' || *p == '-')
                negative = *p++ == '-';

            constexpr std::uint64_t most = 100000000000000000ull;   // 17 digits kept: a double's worth
            std::uint64_t digits = 0;
            int scale = 0;
            bool any = false;

            for (; *p >= '0' && *p <= '9'; ++p, any = true)
            {
                if (digits < most)
                    digits = digits * 10 + (std::uint64_t) (*p - '0');
                else
                    ++scale;
            }

            if (*p == '.')
            {
                for (++p; *p >= '0' && *p <= '9'; ++p, any = true)
                {
                    if (digits < most)
                    {
                        digits = digits * 10 + (std::uint64_t) (*p - '0');
                        --scale;
                    }
                }
            }

            if (! any)
                return 0.0;

            if (*p == 'e' || *p == 'E')
            {
                const auto* q = p + 1;
                bool down = false;

                if (*q == '+' || *q == '-')
                    down = *q++ == '-';

                int exponent = 0;

                for (; *q >= '0' && *q <= '9'; ++q)
                    if (exponent < 10000)
                        exponent = exponent * 10 + (*q - '0');

                scale += down ? -exponent : exponent;
            }

            auto value = (double) digits;

            if (scale > 0)
                value *= std::pow (10.0, (double) scale);
            else if (scale < 0)
                value /= std::pow (10.0, (double) -scale);

            return negative ? -value : value;
        }

        /** SHA-256 (FIPS 180-4): a product's blob key. */
        class Sha256
        {
        public:
            void add (const void* data, std::size_t size) noexcept
            {
                const auto* p = static_cast<const std::uint8_t*> (data);
                total += size;

                while (size > 0)
                {
                    const auto n = std::min (size, (std::size_t) 64 - used);
                    std::memcpy (buffer + used, p, n);
                    used += n;
                    p += n;
                    size -= n;

                    if (used == 64)
                    {
                        block (buffer);
                        used = 0;
                    }
                }
            }

            void add32 (std::uint32_t v) noexcept
            {
                const std::uint8_t bytes[4] { (std::uint8_t) v, (std::uint8_t) (v >> 8), (std::uint8_t) (v >> 16), (std::uint8_t) (v >> 24) };
                add (bytes, 4);
            }

            void finish (std::uint8_t out[32]) noexcept
            {
                const auto bits = total * 8;
                const std::uint8_t one = 0x80, zero = 0;
                add (&one, 1);

                while (used != 56)
                    add (&zero, 1);

                std::uint8_t length[8];

                for (int i = 0; i < 8; ++i)
                    length[i] = (std::uint8_t) (bits >> (56 - 8 * i));

                add (length, 8);

                for (int i = 0; i < 8; ++i)
                    for (int b = 0; b < 4; ++b)
                        out[4 * i + b] = (std::uint8_t) (h[i] >> (24 - 8 * b));
            }

        private:
            static std::uint32_t rotr (std::uint32_t v, int c) noexcept    { return (v >> c) | (v << (32 - c)); }

            void block (const std::uint8_t* p) noexcept
            {
                static const std::uint32_t k[64] {
                    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
                    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
                    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
                    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
                    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
                    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
                    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
                    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };

                std::uint32_t w[64];

                for (int i = 0; i < 16; ++i)
                    w[i] = ((std::uint32_t) p[4 * i] << 24) | ((std::uint32_t) p[4 * i + 1] << 16) | ((std::uint32_t) p[4 * i + 2] << 8) | (std::uint32_t) p[4 * i + 3];

                for (int i = 16; i < 64; ++i)
                {
                    const auto s0 = rotr (w[i - 15], 7) ^ rotr (w[i - 15], 18) ^ (w[i - 15] >> 3);
                    const auto s1 = rotr (w[i - 2], 17) ^ rotr (w[i - 2], 19) ^ (w[i - 2] >> 10);
                    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
                }

                auto a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];

                for (int i = 0; i < 64; ++i)
                {
                    const auto t1 = hh + (rotr (e, 6) ^ rotr (e, 11) ^ rotr (e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
                    const auto t2 = (rotr (a, 2) ^ rotr (a, 13) ^ rotr (a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
                    hh = g; g = f; f = e; e = d + t1;
                    d = c; c = b; b = a; a = t1 + t2;
                }

                h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
            }

            std::uint32_t h[8] { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
            std::uint8_t buffer[64] {};
            std::size_t used = 0;
            std::uint64_t total = 0;
        };

        /** Copies UTF-8 text into a fixed field, cut on a character's boundary. */
        inline void putText (std::uint8_t* field, std::size_t size, const std::string& text) noexcept
        {
            std::memset (field, 0, size);
            auto length = std::min (text.size(), size - 1);

            while (length > 0 && length < text.size() && ((std::uint8_t) text[length] & 0xc0) == 0x80)
                --length;

            std::memcpy (field, text.data(), length);
        }

        inline std::string getText (const std::uint8_t* field, std::size_t size)
        {
            std::size_t length = 0;

            while (length < size && field[length] != 0)
                ++length;

            return std::string ((const char*) field, length);
        }

       #if defined (_WIN32)
        inline std::wstring widen (const std::string& text)
        {
            std::wstring out;
            out.reserve (text.size());

            for (std::size_t i = 0; i < text.size();)
            {
                const auto c = (std::uint8_t) text[i];
                std::uint32_t code = 0;
                int extra = 0;

                if (c < 0x80)                { code = c; }
                else if ((c & 0xe0) == 0xc0) { code = c & 0x1f; extra = 1; }
                else if ((c & 0xf0) == 0xe0) { code = c & 0x0f; extra = 2; }
                else if ((c & 0xf8) == 0xf0) { code = c & 0x07; extra = 3; }
                else                         { code = 0xfffd; }

                ++i;

                for (int k = 0; k < extra && i < text.size(); ++k, ++i)
                    code = (code << 6) | ((std::uint8_t) text[i] & 0x3f);

                if (code >= 0x10000)
                {
                    code -= 0x10000;
                    out += (wchar_t) (0xd800 + (code >> 10));
                    out += (wchar_t) (0xdc00 + (code & 0x3ff));
                }
                else
                {
                    out += (wchar_t) code;
                }
            }

            return out;
        }
       #endif

        /** Samples as floats: from 16 and 24-bit PCM as the audio file readers make them (an exact
            round trip), or float as it is. */
        inline void decode (const std::uint8_t* in, Format format, std::size_t count, float* out) noexcept
        {
            if (format == Format::s16)
            {
                for (std::size_t i = 0; i < count; ++i, in += 2)
                    out[i] = (float) (std::int16_t) ((std::uint16_t) in[0] | ((std::uint16_t) in[1] << 8)) * (1.0f / 32768.0f);
            }
            else if (format == Format::s24)
            {
                for (std::size_t i = 0; i < count; ++i, in += 3)
                {
                    auto v = (std::int32_t) ((std::uint32_t) in[0] | ((std::uint32_t) in[1] << 8) | ((std::uint32_t) in[2] << 16));

                    if (v & 0x800000)
                        v -= 0x1000000;

                    out[i] = (float) v * (1.0f / 8388608.0f);
                }
            }
            else
            {
                for (std::size_t i = 0; i < count; ++i, in += 4)
                {
                    const auto bits = get32 (in);
                    float v;
                    std::memcpy (&v, &bits, 4);
                    out[i] = v == v ? v : 0.0f;
                }
            }
        }

        inline void encode (const float* in, Format format, std::size_t count, std::uint8_t* out) noexcept
        {
            for (std::size_t i = 0; i < count; ++i)
            {
                const auto x = in[i] == in[i] ? in[i] : 0.0f;

                if (format == Format::s16)
                {
                    const auto v = (std::int32_t) std::lround (std::max (-32768.0, std::min (32767.0, (double) x * 32768.0)));
                    *out++ = (std::uint8_t) v;
                    *out++ = (std::uint8_t) (v >> 8);
                }
                else if (format == Format::s24)
                {
                    const auto v = (std::int32_t) std::lround (std::max (-8388608.0, std::min (8388607.0, (double) x * 8388608.0)));
                    *out++ = (std::uint8_t) v;
                    *out++ = (std::uint8_t) (v >> 8);
                    *out++ = (std::uint8_t) (v >> 16);
                }
                else
                {
                    std::uint32_t bits;
                    std::memcpy (&bits, &x, 4);
                    put32 (out, bits);
                    out += 4;
                }
            }
        }

        inline bool formatFrom (const std::string& text, Format& format) noexcept
        {
            if (text == "s16") { format = Format::s16; return true; }
            if (text == "s24") { format = Format::s24; return true; }
            if (text == "f32") { format = Format::f32; return true; }
            return false;
        }

        inline bool same (const char* a, const std::string& b) noexcept    { return b == a; }
    }

    //==========================================================================
    void Cipher::setKey (const std::uint8_t key[32], const std::uint8_t nonce[12]) noexcept
    {
        state[0] = 0x61707865u; state[1] = 0x3320646eu; state[2] = 0x79622d32u; state[3] = 0x6b206574u;

        for (int i = 0; i < 8; ++i)
            state[4 + i] = detail::get32 (key + 4 * i);

        state[12] = 0;

        for (int i = 0; i < 3; ++i)
            state[13 + i] = detail::get32 (nonce + 4 * i);
    }

    void Cipher::apply (std::uint8_t* data, std::size_t size, std::uint64_t position) const noexcept
    {
        auto counter = position / 64;
        auto skip = (std::size_t) (position % 64);
        std::uint8_t stream[64];

        while (size > 0)
        {
            std::uint32_t x[16];
            std::memcpy (x, state, sizeof (x));
            x[12] = (std::uint32_t) counter;

            std::uint32_t w[16];
            std::memcpy (w, x, sizeof (w));

            for (int round = 0; round < 10; ++round)
            {
                detail::quarter (w[0], w[4], w[8],  w[12]);
                detail::quarter (w[1], w[5], w[9],  w[13]);
                detail::quarter (w[2], w[6], w[10], w[14]);
                detail::quarter (w[3], w[7], w[11], w[15]);
                detail::quarter (w[0], w[5], w[10], w[15]);
                detail::quarter (w[1], w[6], w[11], w[12]);
                detail::quarter (w[2], w[7], w[8],  w[13]);
                detail::quarter (w[3], w[4], w[9],  w[14]);
            }

            for (int i = 0; i < 16; ++i)
                detail::put32 (stream + 4 * i, w[i] + x[i]);

            const auto n = std::min (size, (std::size_t) 64 - skip);

            for (std::size_t i = 0; i < n; ++i)
                data[i] ^= stream[skip + i];

            data += n;
            size -= n;
            skip = 0;
            ++counter;
        }
    }

    void openKeyBytes (std::uint8_t key[32]) noexcept
    {
        static const char* const phrase = "Sonify blob: open to every reader of Fanan's blobs";
        std::uint32_t h = 2166136261u;

        for (int i = 0; i < 32; ++i)
        {
            for (const auto* p = phrase; *p != 0; ++p)
                h = (h ^ (std::uint8_t) *p) * 16777619u;

            h ^= (std::uint32_t) (i + 1) * 0x9e3779b9u;
            h *= 16777619u;
            key[i] = (std::uint8_t) (h >> 24);
        }
    }

    void licenceKey (const licence::Soni& soni, std::uint8_t key[32]) noexcept
    {
        static const char tag[] = "sonify-blob-key-1";
        const std::uint8_t zero = 0;
        detail::Sha256 hash;
        hash.add (tag, sizeof (tag) - 1);
        hash.add (&zero, 1);
        hash.add (soni.product.data(), soni.product.size());
        hash.add (&zero, 1);
        for (auto v : { soni.formula.a, soni.formula.b, soni.formula.m })   // each 8 bytes, little-endian
        {
            hash.add32 ((std::uint32_t) (v & 0xffffffffu));
            hash.add32 ((std::uint32_t) (v >> 32));
        }

        hash.finish (key);
    }

    //==========================================================================
    std::FILE* openFile (const std::string& path, bool forWriting)
    {
        if (path.empty())
            return nullptr;

       #if defined (_WIN32)
        return _wfopen (detail::widen (path).c_str(), forWriting ? L"wb" : L"rb");
       #else
        return std::fopen (path.c_str(), forWriting ? "wb" : "rb");
       #endif
    }

    bool seekFile (std::FILE* file, std::uint64_t position)
    {
       #if defined (_WIN32)
        return _fseeki64 (file, (long long) position, SEEK_SET) == 0;
       #else
        return fseeko (file, (off_t) position, SEEK_SET) == 0;
       #endif
    }

    static std::uint64_t sizeOf (std::FILE* file)
    {
       #if defined (_WIN32)
        if (_fseeki64 (file, 0, SEEK_END) != 0)
            return 0;

        return (std::uint64_t) _ftelli64 (file);
       #else
        if (fseeko (file, 0, SEEK_END) != 0)
            return 0;

        return (std::uint64_t) ftello (file);
       #endif
    }

    bool fileExists (const std::string& path)
    {
        if (auto* file = openFile (path, false))
        {
            std::fclose (file);
            return true;
        }

        return false;
    }

    //==========================================================================
    std::string indexText (const Index& index)
    {
        std::string text = "sonify-blob\t1\n";
        text += "name\t" + detail::clean (index.name) + "\n";

        for (const auto& s : index.samples)
        {
            text += "sample\tframes=" + std::to_string (s.frames) + "\trate=" + detail::numberText (s.rate)
                  + "\tchannels=" + std::to_string (s.channels) + "\tformat=" + nameOf (s.format)
                  + "\toffset=" + std::to_string (s.offset) + "\tname=" + detail::clean (s.name) + "\n";
        }

        for (const auto& r : index.regions)
        {
            text += "region\tsample=" + std::to_string (r.sample);

            for (const auto& [key, value] : r.opcodes)
                if (! key.empty() && key != "sample")
                    text += "\t" + detail::clean (key) + "=" + detail::clean (value);

            text += "\n";
        }

        return text;
    }

    bool parseIndex (const std::string& text, Index& index, std::string& error)
    {
        index = Index();
        std::size_t start = 0;
        bool header = false;

        while (start < text.size())
        {
            auto end = text.find ('\n', start);

            if (end == std::string::npos)
                end = text.size();

            std::vector<std::string> fields;
            std::size_t f = start;

            while (f <= end)
            {
                auto tab = text.find ('\t', f);

                if (tab == std::string::npos || tab > end)
                    tab = end;

                fields.push_back (text.substr (f, tab - f));
                f = tab + 1;
            }

            start = end + 1;

            if (fields.empty() || fields[0].empty())
                continue;

            if (fields[0] == "sonify-blob")
            {
                if (fields.size() < 2 || std::atoi (fields[1].c_str()) != 1)
                {
                    error = "It's a blob of a newer kind than this player knows.";
                    return false;
                }

                header = true;
                continue;
            }

            if (! header)
                break;

            if (fields[0] == "name")
            {
                index.name = fields.size() > 1 ? fields[1] : std::string();
                continue;
            }

            auto valueOf = [&fields] (const char* key, std::string& value)
            {
                const auto prefix = std::string (key) + "=";

                for (std::size_t i = 1; i < fields.size(); ++i)
                {
                    if (fields[i].compare (0, prefix.size(), prefix) == 0)
                    {
                        value = fields[i].substr (prefix.size());
                        return true;
                    }
                }

                return false;
            };

            if (fields[0] == "sample")
            {
                SampleEntry s;
                std::string v;

                if (valueOf ("frames", v))   s.frames = std::atoll (v.c_str());
                if (valueOf ("rate", v))     s.rate = detail::number (v.c_str());
                if (valueOf ("channels", v)) s.channels = std::atoi (v.c_str());
                if (valueOf ("offset", v))   s.offset = (std::uint64_t) std::atoll (v.c_str());
                if (valueOf ("name", v))     s.name = v;

                if (! valueOf ("format", v) || ! detail::formatFrom (v, s.format) || s.frames < 0
                    || s.rate < 1000.0 || s.rate > 768000.0 || (s.channels != 1 && s.channels != 2))
                {
                    error = "Its sample list is damaged.";
                    return false;
                }

                index.samples.push_back (s);
                continue;
            }

            if (fields[0] == "region")
            {
                RegionEntry r;

                for (std::size_t i = 1; i < fields.size(); ++i)
                {
                    const auto equals = fields[i].find ('=');

                    if (equals == std::string::npos || equals == 0)
                        continue;

                    auto key = fields[i].substr (0, equals);
                    auto value = fields[i].substr (equals + 1);

                    if (key == "sample")
                        r.sample = std::atoi (value.c_str());
                    else
                        r.opcodes.emplace_back (std::move (key), std::move (value));
                }

                if (r.sample < 0 || r.sample >= (int) index.samples.size())
                {
                    error = "Its regions are damaged.";
                    return false;
                }

                index.regions.push_back (std::move (r));
            }
        }

        if (! header)
        {
            error = "It isn't a blob, or its key is wrong.";
            return false;
        }

        return true;
    }

    //==========================================================================
    bool readHead (std::FILE* file, Head& head, std::string& error)
    {
        std::uint8_t bytes[headBytes];

        if (file == nullptr || ! seekFile (file, 0) || std::fread (bytes, 1, headBytes, file) != headBytes)
        {
            error = "It can't be read.";
            return false;
        }

        if (std::memcmp (bytes, "SNFYBLOB", 8) != 0)
        {
            error = "It isn't a blob.";
            return false;
        }

        head.version = detail::get32 (bytes + 8);

        if (head.version != fileVersion || detail::get32 (bytes + 12) != headBytes)
        {
            error = "It's a blob of a newer kind than this player knows.";
            return false;
        }

        head.keyKind = detail::get32 (bytes + 16);
        head.flags = detail::get32 (bytes + 20);
        std::memcpy (head.nonce, bytes + 24, 12);
        head.indexOffset = detail::get64 (bytes + 40);
        head.indexSize = detail::get64 (bytes + 48);
        head.dataOffset = detail::get64 (bytes + 56);
        head.dataSize = detail::get64 (bytes + 64);
        head.product = detail::getText (bytes + 72, 64);
        head.name = detail::getText (bytes + 136, 96);

        if (head.indexOffset < headBytes || head.indexSize == 0 || head.indexSize > (64u << 20) || head.dataOffset < head.indexOffset + head.indexSize)
        {
            error = "It's damaged.";
            return false;
        }

        return true;
    }

    /** The key a blob's head asks for: Sonify's own, or the one given (a locked blob's). */
    static bool keyFor (const Head& head, const std::uint8_t* given, std::uint8_t key[32])
    {
        if (head.keyKind == openKey)
        {
            openKeyBytes (key);
            return true;
        }

        if (given == nullptr)
            return false;

        std::memcpy (key, given, 32);
        return true;
    }

    /** A locked blob's key from the license.soni in its own folder (STELLA_LICENCE_NATIVE): false,
        and why, when it isn't there or is another product's. */
    static bool soniKeyFor (const std::string& path, const Head& head, std::uint8_t key[32], licence::Soni& soni, std::string& error)
    {
       #if STELLA_LICENCE_NATIVE && ! defined (__wasm__)
        if (head.product.empty())
        {
            error = "It's locked, but its head names no product (it's damaged).";
            return false;
        }

        auto folder = licence::native::parentOf (path);

        if (folder.empty())
            folder = ".";

        if (! licence::native::soniIn (folder, soni))
        {
            error = "It's protected (" + head.product + "): it can't load without its " + licence::fileName
                  + ", which must be in the same folder (" + folder + ").";
            return false;
        }

        if (soni.product != head.product)
        {
            error = "The " + std::string (licence::fileName) + " beside it is another product's ("
                  + (soni.name.empty() ? soni.product : soni.name) + "), not this library's (" + head.product + ").";
            return false;
        }

        licenceKey (soni, key);
        return true;
       #else
        (void) path; (void) key; (void) soni;
        error = "It's locked: it plays once " + (head.product.empty() ? std::string ("its product") : head.product) + " is registered.";
        return false;
       #endif
    }

    static bool readIndex (std::FILE* file, const Head& head, const Cipher& cipher, Index& index, std::string& error)
    {
        std::string text ((std::size_t) head.indexSize, '\0');

        if (! seekFile (file, head.indexOffset) || std::fread (&text[0], 1, text.size(), file) != text.size())
        {
            error = "It's cut short.";
            return false;
        }

        if ((head.flags & 1) != 0)
            cipher.apply (reinterpret_cast<std::uint8_t*> (&text[0]), text.size(), head.indexOffset - headBytes);

        return parseIndex (text, index, error);
    }

    bool summarise (const std::string& path, Summary& summary, std::string& error, const std::uint8_t* key)
    {
        summary = Summary();
        auto* file = openFile (path, false);

        if (file == nullptr)
        {
            error = "It isn't there.";
            return false;
        }

        Head head;
        std::uint8_t bytes[32];
        bool ok = readHead (file, head, error);

        if (ok)
        {
            summary.name = head.name;
            summary.product = head.product;
            summary.bytes = sizeOf (file);

            bool opens = keyFor (head, key, bytes);
            licence::Soni soni;
            std::string why;

            if (! opens && soniKeyFor (path, head, bytes, soni, why))
            {
                opens = true;
                summary.licenceName = soni.name.empty() ? soni.product : soni.name;
            }

            if (! opens)
            {
                summary.locked = true;
            }
            else
            {
                Cipher cipher;
                cipher.setKey (bytes, head.nonce);
                Index index;

                if (readIndex (file, head, cipher, index, error))
                {
                    summary.name = index.name.empty() ? head.name : index.name;
                    summary.regions = (int) index.regions.size();
                    summary.samples = (int) index.samples.size();
                }
                else
                {
                    if (! summary.licenceName.empty())
                        error = "Its " + std::string (licence::fileName) + " doesn't open it: it isn't the one it was made with.";

                    ok = false;
                }
            }
        }

        std::fclose (file);
        return ok;
    }

    //==========================================================================
    Writer::~Writer()
    {
        abandon();
    }

    bool Writer::open (const std::string& filePath, Index& index, std::string& error, const std::uint8_t* key, const std::string& product)
    {
        abandon();

        // The samples' places, each 16-byte aligned.
        std::uint64_t offset = 0;

        for (auto& s : index.samples)
        {
            s.channels = s.channels > 1 ? 2 : 1;
            s.offset = offset;
            offset += (std::uint64_t) s.frames * (std::uint64_t) (s.channels * bytesOf (s.format));
            offset = (offset + 15) & ~(std::uint64_t) 15;
        }

        const auto text = indexText (index);
        const auto indexOffset = (std::uint64_t) headBytes;
        dataOffset = (indexOffset + text.size() + 15) & ~(std::uint64_t) 15;

        Head head;
        head.keyKind = key != nullptr ? productKey : openKey;
        head.indexOffset = indexOffset;
        head.indexSize = text.size();
        head.dataOffset = dataOffset;
        head.dataSize = offset;
        head.product = key != nullptr ? product : std::string();
        head.name = index.name;

        // A fresh nonce for every blob.
        std::random_device device;
        std::mt19937_64 mixer (((std::uint64_t) device() << 32) ^ (std::uint64_t) device() ^ (std::uint64_t) detail::nowMs());

        for (auto& b : head.nonce)
            b = (std::uint8_t) (mixer() >> 56);

        std::uint8_t keyBytes[32];

        if (key != nullptr)
            std::memcpy (keyBytes, key, 32);
        else
            openKeyBytes (keyBytes);

        cipher.setKey (keyBytes, head.nonce);

        file = openFile (filePath, true);

        if (file == nullptr)
        {
            error = "Couldn't write " + filePath;
            return false;
        }

        path = filePath;
        samples = index.samples;
        current = 0;
        framesDone = 0;

        std::uint8_t bytes[headBytes] {};
        std::memcpy (bytes, "SNFYBLOB", 8);
        detail::put32 (bytes + 8, fileVersion);
        detail::put32 (bytes + 12, headBytes);
        detail::put32 (bytes + 16, head.keyKind);
        detail::put32 (bytes + 20, 1);
        std::memcpy (bytes + 24, head.nonce, 12);
        detail::put64 (bytes + 40, head.indexOffset);
        detail::put64 (bytes + 48, head.indexSize);
        detail::put64 (bytes + 56, head.dataOffset);
        detail::put64 (bytes + 64, head.dataSize);
        detail::putText (bytes + 72, 64, head.product);
        detail::putText (bytes + 136, 96, head.name);

        if (std::fwrite (bytes, 1, headBytes, file) != headBytes)
        {
            error = "Couldn't write " + filePath + " (is the disk full?)";
            abandon();
            return false;
        }

        position = headBytes;

        if (! put (reinterpret_cast<const std::uint8_t*> (text.data()), text.size(), error) || ! pad (dataOffset, error))
        {
            abandon();
            return false;
        }

        // Samples with no frames take no room: move past them.
        while (current < samples.size() && samples[current].frames == 0)
            ++current;

        return true;
    }

    bool Writer::put (const std::uint8_t* data, std::size_t size, std::string& error)
    {
        buffer.assign (data, data + size);
        cipher.apply (buffer.data(), buffer.size(), position - headBytes);

        if (std::fwrite (buffer.data(), 1, buffer.size(), file) != buffer.size())
        {
            error = "Couldn't write " + path + " (is the disk full?)";
            return false;
        }

        position += size;
        return true;
    }

    bool Writer::pad (std::uint64_t to, std::string& error)
    {
        if (to <= position)
            return true;

        const std::vector<std::uint8_t> zeros ((std::size_t) (to - position), 0);
        return put (zeros.data(), zeros.size(), error);
    }

    bool Writer::write (const float* left, const float* right, int frames, std::string& error)
    {
        if (file == nullptr)
        {
            error = "The blob isn't open.";
            return false;
        }

        while (frames > 0)
        {
            if (current >= samples.size())
            {
                error = "More frames than the samples have.";
                return false;
            }

            const auto& s = samples[current];
            const auto n = (int) std::min<std::int64_t> (frames, s.frames - framesDone);

            if (! pad (dataOffset + s.offset + (std::uint64_t) framesDone * (std::uint64_t) (s.channels * bytesOf (s.format)), error))
                return false;

            std::vector<float> interleaved ((std::size_t) n * (std::size_t) s.channels);

            for (int i = 0; i < n; ++i)
            {
                if (s.channels == 2)
                {
                    interleaved[(std::size_t) i * 2] = left[i];
                    interleaved[(std::size_t) i * 2 + 1] = right != nullptr ? right[i] : left[i];
                }
                else
                {
                    interleaved[(std::size_t) i] = right != nullptr ? 0.5f * (left[i] + right[i]) : left[i];
                }
            }

            std::vector<std::uint8_t> bytes (interleaved.size() * (std::size_t) bytesOf (s.format));
            detail::encode (interleaved.data(), s.format, interleaved.size(), bytes.data());

            if (! put (bytes.data(), bytes.size(), error))
                return false;

            framesDone += n;
            frames -= n;
            left += n;

            if (right != nullptr)
                right += n;

            if (framesDone >= s.frames)
            {
                ++current;
                framesDone = 0;

                while (current < samples.size() && samples[current].frames == 0)
                    ++current;
            }
        }

        return true;
    }

    bool Writer::finish (std::string& error)
    {
        if (file == nullptr)
        {
            error = "The blob isn't open.";
            return false;
        }

        if (current < samples.size())
        {
            error = "Not every sample was written.";
            abandon();
            return false;
        }

        std::uint64_t end = 0;

        if (! samples.empty())
        {
            const auto& last = samples.back();
            end = last.offset + (std::uint64_t) last.frames * (std::uint64_t) (last.channels * bytesOf (last.format));
        }

        const bool ok = pad (dataOffset + end, error) && std::fflush (file) == 0;

        if (std::fclose (file) != 0 || ! ok)
        {
            file = nullptr;

            if (error.empty())
                error = "Couldn't finish writing " + path + " (is the disk full?)";

           #if defined (_WIN32)
            _wremove (detail::widen (path).c_str());   // a UTF-8 path: Windows' narrow calls would miss it
           #else
            std::remove (path.c_str());
           #endif
            return false;
        }

        file = nullptr;
        return true;
    }

    void Writer::abandon()
    {
        if (file != nullptr)
        {
            std::fclose (file);
            file = nullptr;

           #if defined (_WIN32)
            _wremove (detail::widen (path).c_str());
           #else
            std::remove (path.c_str());
           #endif
        }
    }

    //==========================================================================
    //==========================================================================
    // Protected libraries in use: a record for each product whose blobs are loaded (its
    // instruments share it), and whether this computer is registered for it. The audio thread
    // only reads the atomics.

    struct LicenceRecord
    {
        licence::Soni soni;                          // as its license.soni says (never changes)
        std::atomic<bool> registered { false };
        int instruments = 0;                         // loaded now (under the book's lock)
    };

    namespace
    {
        // For demoInUse: constant-initialised, so the audio thread never meets a first-use guard.
        std::atomic<int> unregisteredInstruments { 0 };   // loaded now, of products this computer isn't registered for
        std::atomic<float> demoPlay { (float) licence::playSeconds }, demoSilent { (float) licence::silentSeconds };
    }

    class LicenceBook
    {
    public:
        /** Never destroyed: instruments may go at the program's very end. */
        static LicenceBook& get()
        {
            static auto* book = new LicenceBook();
            return *book;
        }

        /** An instrument of a product has loaded (the loading thread). */
        std::shared_ptr<LicenceRecord> take (const licence::Soni& soni, bool registeredNow)
        {
            const std::lock_guard<std::mutex> hold (lock);

            for (auto it = records.begin(); it != records.end();)
                it = it->second.expired() ? records.erase (it) : std::next (it);

            auto record = records[soni.product].lock();

            if (record == nullptr)
            {
                record = std::make_shared<LicenceRecord>();
                record->soni = soni;
                record->registered.store (registeredNow);
                records[soni.product] = record;
            }

            ++record->instruments;

            if (! record->registered.load())
            {
                demoPlay.store (record->soni.playSeconds);
                demoSilent.store (record->soni.silentSeconds);
                unregisteredInstruments.fetch_add (1);
            }

            return record;
        }

        /** One of its instruments has gone. */
        void give (LicenceRecord& record)
        {
            const std::lock_guard<std::mutex> hold (lock);
            --record.instruments;

            if (! record.registered.load())
                unregisteredInstruments.fetch_sub (1);
        }

        void setRegistered (LicenceRecord& record, bool now)
        {
            const std::lock_guard<std::mutex> hold (lock);

            if (record.registered.load() == now)
                return;

            record.registered.store (now, std::memory_order_release);
            unregisteredInstruments.fetch_add (now ? -record.instruments : record.instruments);
        }

        /** The records with instruments loaded now, a product each. */
        std::vector<std::shared_ptr<LicenceRecord>> inUse()
        {
            const std::lock_guard<std::mutex> hold (lock);
            std::vector<std::shared_ptr<LicenceRecord>> out;

            for (const auto& entry : records)
                if (auto record = entry.second.lock(); record != nullptr && record->instruments > 0)
                    out.push_back (std::move (record));

            return out;
        }

    private:
        LicenceBook() = default;

        std::mutex lock;
        std::map<std::string, std::weak_ptr<LicenceRecord>> records;
    };

    std::vector<LicenceInfo> licencesInUse()
    {
        std::vector<LicenceInfo> out;

        for (const auto& record : LicenceBook::get().inUse())
        {
            LicenceInfo info;
            info.product = record->soni.product;
            info.name = record->soni.name.empty() ? record->soni.product : record->soni.name;
            info.registered = record->registered.load();
            info.playSeconds = record->soni.playSeconds;
            info.silentSeconds = record->soni.silentSeconds;
            out.push_back (std::move (info));
        }

        return out;
    }

    bool licenceOf (const std::string& product, licence::Soni& soni)
    {
        for (const auto& record : LicenceBook::get().inUse())
        {
            if (record->soni.product == product)
            {
                soni = record->soni;
                return true;
            }
        }

        return false;
    }

    void recheckLicences()
    {
       #if STELLA_LICENCE_NATIVE && ! defined (__wasm__)
        for (const auto& record : LicenceBook::get().inUse())
            if (! record->registered.load() && licence::native::isRegistered (record->soni))
                LicenceBook::get().setRegistered (*record, true);
       #endif
    }

    std::string registerSerial (const std::string& serial)
    {
       #if STELLA_LICENCE_NATIVE && ! defined (__wasm__)
        std::string why;
        bool any = false;

        for (const auto& record : LicenceBook::get().inUse())
        {
            if (record->registered.load())
                continue;

            any = true;
            const auto problem = licence::native::registerSerial (record->soni, serial);   // checks it fits, keeps it (keepSerial)

            if (problem.empty())
            {
                if (! licence::native::isRegistered (record->soni))
                    return "The serial is right, but it couldn't be read back from this computer's key file.";

                LicenceBook::get().setRegistered (*record, true);
                recheckLicences();   // another library of the same product's
                return {};
            }

            if (why.empty())
                why = problem;
        }

        return any ? why : std::string ("Nothing here needs registering.");
       #else
        (void) serial;
        return "Protected libraries can't be registered in this build.";
       #endif
    }

    int demoInUse (float* playSeconds, float* silentSeconds) noexcept
    {
        if (unregisteredInstruments.load (std::memory_order_acquire) <= 0)
            return 0;

        if (playSeconds != nullptr)   *playSeconds = demoPlay.load (std::memory_order_relaxed);
        if (silentSeconds != nullptr) *silentSeconds = demoSilent.load (std::memory_order_relaxed);
        return 1;
    }

    //==========================================================================
    //==========================================================================
    // Playing

    struct LoadedSample
    {
        std::int64_t frames = 0;
        double rate = 44100.0;
        int channels = 1;
        Format format = Format::s16;
        std::uint64_t offset = 0;            // in the file
        int bytesPerFrame = 2;
        std::int64_t headFrames = 0;
        std::vector<float> head;             // its first headFrames, interleaved
    };

    struct LoopCache
    {
        int sample = -1;
        std::int64_t start = 0, end = 0;     // what it holds of the sample: [start, end)
        std::vector<float> data;             // interleaved
    };

    struct LoadedRegion
    {
        Region info {};
        int sample = -1;
        int loop = -1;                       // its loop cache
        bool loops = false;                  // it goes round its loop (loop_continuous or loop_sustain, a loop that's there)
        std::int64_t offset = 0, end = 0, loopStart = 0, loopEnd = 0, fade = 0;
        float lorand = 0.0f, hirand = 1.0f;
        int seqGroup = -1;
        int swLast = -1;
        std::vector<std::pair<std::string, std::string>> opcodes;
    };

    class Instrument
    {
    public:
        ~Instrument()
        {
            if (file != nullptr)
                std::fclose (file);

            if (licenceRecord != nullptr)
                LicenceBook::get().give (*licenceRecord);
        }

        /** Frames of a sample, decrypted and as floats (interleaved as it is). Any thread. */
        bool readFrames (const LoadedSample& s, std::int64_t first, std::int64_t count, float* out, std::vector<std::uint8_t>& scratch)
        {
            if (count <= 0)
                return true;

            const auto bytes = (std::size_t) count * (std::size_t) s.bytesPerFrame;
            const auto where = s.offset + (std::uint64_t) first * (std::uint64_t) s.bytesPerFrame;

            if (scratch.size() < bytes)
                scratch.resize (bytes);

            {
                const std::lock_guard<std::mutex> hold (fileLock);

                if (file == nullptr || ! seekFile (file, where) || std::fread (scratch.data(), 1, bytes, file) != bytes)
                {
                    std::memset (out, 0, sizeof (float) * (std::size_t) count * (std::size_t) s.channels);
                    return false;
                }
            }

            if (encrypted)
                cipher.apply (scratch.data(), bytes, where - headBytes);

            detail::decode (scratch.data(), s.format, (std::size_t) count * (std::size_t) s.channels, out);
            return true;
        }

        std::string path, name;
        std::FILE* file = nullptr;
        std::mutex fileLock;
        Cipher cipher;
        bool encrypted = true;
        std::vector<LoadedSample> samples;
        std::vector<LoadedRegion> regions;
        std::vector<LoopCache> loops;
        int seqGroups = 0;
        int swLow = -1, swHigh = -1, swDefault = -1;
        std::size_t memory = 0;
        std::shared_ptr<LicenceRecord> licenceRecord;   // a protected library's (opened by its license.soni); set before it's published
    };

    namespace detail
    {
        inline int noteValue (const std::string& text, int fallback)
        {
            if (text.empty())
                return fallback;

            if (std::isdigit ((unsigned char) text[0]) || text[0] == '-')
                return std::atoi (text.c_str());

            return fallback;
        }

        inline const std::string* opcodeOf (const LoadedRegion& r, const char* key)
        {
            const std::string* found = nullptr;

            for (const auto& [k, v] : r.opcodes)
                if (k == key)
                    found = &v;   // the last one given wins

            return found;
        }

        inline double numberOf (const LoadedRegion& r, const char* key, double fallback)
        {
            if (const auto* v = opcodeOf (r, key); v != nullptr && ! v->empty())
                return number (v->c_str());

            return fallback;
        }

        inline int clampInt (double v, int lo, int hi)    { return (int) std::max ((double) lo, std::min ((double) hi, std::floor (v + 0.5))); }
    }

    /** Reads an instrument: its index, every sample's head, and every loop's frames. shouldStop:
        called off. */
    static std::shared_ptr<Instrument> loadInstrument (const std::string& path, const std::uint8_t* key, std::string& error,
                                                       const std::atomic<bool>& shouldStop)
    {
        auto made = std::make_shared<Instrument>();
        made->path = path;
        made->file = openFile (path, false);

        if (made->file == nullptr)
        {
            error = "It isn't there.";
            return nullptr;
        }

        Head head;

        if (! readHead (made->file, head, error))
            return nullptr;

        std::uint8_t keyBytes[32];
        licence::Soni soni;
        bool bySoni = false;   // a protected library's, opened by the license.soni beside it

        if (! keyFor (head, key, keyBytes))
        {
            if (! soniKeyFor (path, head, keyBytes, soni, error))
                return nullptr;

            bySoni = true;
        }

        made->cipher.setKey (keyBytes, head.nonce);
        made->encrypted = (head.flags & 1) != 0;

        Index index;

        if (! readIndex (made->file, head, made->cipher, index, error))
        {
            if (bySoni)
                error = "Its " + std::string (licence::fileName) + " doesn't open it: it isn't the one it was made with.";

            return nullptr;
        }

        const auto fileBytes = sizeOf (made->file);
        made->name = index.name.empty() ? head.name : index.name;

        // The samples: where each is, and how much of each start is kept in memory (about 96 MB
        // for the whole instrument, from 8192 to 65536 frames each).
        std::int64_t totalChannels = 0;

        for (const auto& s : index.samples)
            totalChannels += s.channels;

        const auto headFrames = std::max<std::int64_t> (8192, std::min<std::int64_t> (65536, (96ll << 20) / 4 / std::max<std::int64_t> (1, totalChannels)));
        std::vector<std::uint8_t> scratch;

        for (const auto& s : index.samples)
        {
            LoadedSample loaded;
            loaded.frames = s.frames;
            loaded.rate = s.rate;
            loaded.channels = s.channels;
            loaded.format = s.format;
            loaded.bytesPerFrame = s.channels * bytesOf (s.format);
            loaded.offset = head.dataOffset + s.offset;

            if (loaded.offset + (std::uint64_t) loaded.frames * (std::uint64_t) loaded.bytesPerFrame > fileBytes)
            {
                error = "It's cut short (was it copied whole?).";
                return nullptr;
            }

            loaded.headFrames = std::min (loaded.frames, headFrames);
            loaded.head.resize ((std::size_t) loaded.headFrames * (std::size_t) loaded.channels);

            if (! made->readFrames (loaded, 0, loaded.headFrames, loaded.head.data(), scratch))
            {
                error = "It can't be read.";
                return nullptr;
            }

            made->memory += loaded.head.size() * sizeof (float);
            made->samples.push_back (std::move (loaded));

            if (shouldStop.load())
            {
                error = "Called off.";
                return nullptr;
            }
        }

        // The regions.
        std::map<std::vector<int>, int> groups;

        for (auto& entry : index.regions)
        {
            LoadedRegion r;
            r.sample = entry.sample;
            r.opcodes = std::move (entry.opcodes);
            const auto& s = made->samples[(std::size_t) r.sample];
            auto& info = r.info;

            const auto text = [&r] (const char* opcodeName) -> std::string
            {
                const auto* v = detail::opcodeOf (r, opcodeName);
                return v != nullptr ? *v : std::string();
            };

            info.sample = r.sample;
            info.lokey = std::max (0, std::min (127, detail::noteValue (text ("lokey"), 0)));
            info.hikey = std::max (0, std::min (127, detail::noteValue (text ("hikey"), 127)));
            info.keycenter = std::max (0, std::min (127, detail::noteValue (text ("pitch_keycenter"), 60)));
            info.lovel = detail::clampInt (detail::numberOf (r, "lovel", 0), 0, 127);
            info.hivel = detail::clampInt (detail::numberOf (r, "hivel", 127), 0, 127);

            if (info.lokey > info.hikey) std::swap (info.lokey, info.hikey);
            if (info.lovel > info.hivel) std::swap (info.lovel, info.hivel);

            const auto triggerText = text ("trigger");
            info.trigger = triggerText == "release" || triggerText == "release_key" ? releaseTrigger
                         : triggerText == "first" ? firstTrigger : triggerText == "legato" ? legatoTrigger : attackTrigger;

            info.seqLength = detail::clampInt (detail::numberOf (r, "seq_length", 1), 1, 100);
            info.seqPosition = detail::clampInt (detail::numberOf (r, "seq_position", 1), 1, 100);
            info.group = (std::int32_t) detail::numberOf (r, "group", 0);
            info.offBy = (std::int32_t) detail::numberOf (r, "off_by", 0);

            info.volume = (float) detail::numberOf (r, "volume", 0.0);

            if (const auto amplitude = detail::numberOf (r, "amplitude", 100.0); amplitude != 100.0)
                info.volume += amplitude > 0.0 ? (float) (20.0 * std::log10 (amplitude / 100.0)) : -144.0f;

            info.pan = (float) std::max (-100.0, std::min (100.0, detail::numberOf (r, "pan", 0.0)));
            info.tune = (float) (detail::numberOf (r, "tune", 0.0) + 100.0 * detail::numberOf (r, "transpose", 0.0));
            info.keytrack = (float) detail::numberOf (r, "pitch_keytrack", 100.0);
            info.veltrack = (float) detail::numberOf (r, "amp_veltrack", 100.0);
            info.ampAttack = (float) std::max (0.0, detail::numberOf (r, "ampeg_attack", 0.0));
            info.ampHold = (float) std::max (0.0, detail::numberOf (r, "ampeg_hold", 0.0));
            info.ampDecay = (float) std::max (0.0, detail::numberOf (r, "ampeg_decay", 0.0));
            info.ampSustain = (float) std::max (0.0, std::min (100.0, detail::numberOf (r, "ampeg_sustain", 100.0)));
            info.ampRelease = (float) std::max (0.0, detail::numberOf (r, "ampeg_release", 0.001));
            info.channels = s.channels;
            info.sampleRate = (float) s.rate;

            // Where it plays: from its offset to its end (end= is its last frame).
            r.offset = std::max<std::int64_t> (0, std::min<std::int64_t> (s.frames > 0 ? s.frames - 1 : 0, (std::int64_t) detail::numberOf (r, "offset", 0)));
            r.end = s.frames;

            if (const auto* e = detail::opcodeOf (r, "end"); e != nullptr && std::atoll (e->c_str()) > 0)
                r.end = std::min<std::int64_t> (s.frames, std::atoll (e->c_str()) + 1);

            // Its loop.
            const auto mode = text ("loop_mode").empty() ? text ("loopmode") : text ("loop_mode");
            info.loopMode = mode == "one_shot" ? oneShot : mode == "loop_continuous" ? loopContinuous
                          : mode == "loop_sustain" ? loopSustain : noLoop;

            r.loopStart = (std::int64_t) detail::numberOf (r, "loop_start", detail::numberOf (r, "loopstart", -1.0));
            r.loopEnd = (std::int64_t) detail::numberOf (r, "loop_end", detail::numberOf (r, "loopend", -1.0));
            r.loopEnd = std::min (r.loopEnd, r.end);
            const bool hasLoop = r.loopStart >= 0 && r.loopEnd > r.loopStart + 1;

            if (hasLoop)
            {
                // The crossfade: seconds as the standard has it, or (a whole number of 10 or more)
                // samples as Sampler Arena writes it; never longer than the loop, nor than what's
                // before it.
                const auto fadeText = text ("loop_crossfade");
                const auto number = detail::number (fadeText.c_str());
                const auto frames = fadeText.find ('.') == std::string::npos && number >= 10.0 ? number : number * s.rate;
                r.fade = std::max<std::int64_t> (0, std::min<std::int64_t> ({ (std::int64_t) frames, r.loopEnd - r.loopStart, r.loopStart }));
            }

            info.loopStart = (std::int32_t) std::max<std::int64_t> (-1, std::min<std::int64_t> (0x7fffffff, r.loopStart));
            info.loopEnd = (std::int32_t) std::max<std::int64_t> (-1, std::min<std::int64_t> (0x7fffffff, r.loopEnd));
            info.crossfade = (float) ((double) r.fade / s.rate);
            info.offset = (std::int32_t) std::min<std::int64_t> (0x7fffffff, r.offset);
            info.end = (std::int32_t) std::min<std::int64_t> (0x7fffffff, r.end);

            r.loops = hasLoop && (info.loopMode == loopContinuous || info.loopMode == loopSustain);

            if (r.loops)
            {
                // The loop's frames (and what's before it, for the crossfade, and after it, for
                // the release), unless the head has them all.
                const auto cacheStart = std::max<std::int64_t> (0, r.loopStart - r.fade - 4);
                const auto cacheEnd = std::min<std::int64_t> (s.frames, r.loopEnd + std::max<std::int64_t> (s.headFrames, 8192) + 4);

                if (cacheEnd > s.headFrames)
                {
                    if (cacheEnd - cacheStart > (8ll << 20))
                    {
                        r.loops = false;   // a loop of minutes: played through
                    }
                    else
                    {
                        for (std::size_t c = 0; c < made->loops.size(); ++c)
                            if (made->loops[c].sample == r.sample && made->loops[c].start == cacheStart && made->loops[c].end == cacheEnd)
                                r.loop = (int) c;

                        if (r.loop < 0)
                        {
                            LoopCache cache;
                            cache.sample = r.sample;
                            cache.start = cacheStart;
                            cache.end = cacheEnd;
                            cache.data.resize ((std::size_t) (cacheEnd - cacheStart) * (std::size_t) s.channels);

                            if (! made->readFrames (s, cacheStart, cacheEnd - cacheStart, cache.data.data(), scratch))
                            {
                                error = "It can't be read.";
                                return nullptr;
                            }

                            made->memory += cache.data.size() * sizeof (float);
                            r.loop = (int) made->loops.size();
                            made->loops.push_back (std::move (cache));
                        }
                    }
                }
            }

            // Random choice, keyswitches, round robins.
            r.lorand = (float) detail::numberOf (r, "lorand", 0.0);
            r.hirand = (float) detail::numberOf (r, "hirand", 1.0);
            r.swLast = detail::noteValue (text ("sw_last"), -1);

            if (const auto low = detail::noteValue (text ("sw_lokey"), -1), high = detail::noteValue (text ("sw_hikey"), -1); low >= 0 && high >= low)
            {
                made->swLow = made->swLow < 0 ? low : std::min (made->swLow, low);
                made->swHigh = std::max (made->swHigh, high);
            }

            if (const auto initial = detail::noteValue (text ("sw_default"), -1); initial >= 0)
                made->swDefault = initial;

            if (info.seqLength > 1)
            {
                const std::vector<int> groupKey { info.lokey, info.hikey, info.lovel, info.hivel, info.seqLength, info.trigger == releaseTrigger ? 1 : 0 };
                auto found = groups.find (groupKey);

                if (found == groups.end())
                    found = groups.emplace (groupKey, (int) groups.size()).first;

                r.seqGroup = found->second;
            }

            made->regions.push_back (std::move (r));

            if (shouldStop.load())
            {
                error = "Called off.";
                return nullptr;
            }
        }

        if (made->regions.empty())
        {
            error = "It has no regions.";
            return nullptr;
        }

        // A keyswitch range without a key of its own: from the lowest to the highest sw_last.
        if (made->swLow < 0)
        {
            for (const auto& r : made->regions)
            {
                if (r.swLast >= 0)
                {
                    made->swLow = made->swLow < 0 ? r.swLast : std::min (made->swLow, r.swLast);
                    made->swHigh = std::max (made->swHigh, r.swLast);
                }
            }
        }

        made->seqGroups = (int) groups.size();

       #if STELLA_LICENCE_NATIVE && ! defined (__wasm__)
        // A protected library in use: registered on this computer, or its demo plays.
        if (bySoni)
            made->licenceRecord = LicenceBook::get().take (soni, licence::native::isRegistered (soni));
       #endif

        return made;
    }

    //==========================================================================
    // A library's state, shared with the loading and disk threads (it lives until they let go).

    struct Slot
    {
        char name[128] {};
        std::string path, problem;           // the message and loading threads' (under the library's lock)
        std::uint8_t key[32] {};
        bool hasKey = false;
        std::uint32_t entryGeneration = 0;   // set again: what was being loaded isn't wanted

        std::atomic<Instrument*> ready { nullptr };
        std::atomic<int> state { stella::blob::notLoaded };
        std::atomic<std::uint32_t> generation { 0 };   // moved on when its instrument goes
        std::atomic<int> voices { 0 };
        std::atomic<std::int64_t> lastUse { -1000000000ll };
        std::shared_ptr<Instrument> holder;  // the loading thread's

        // The audio thread's.
        std::uint16_t seq[512] {};
        int swLast = -1;
    };

    struct Voice
    {
        // The audio thread's.
        bool active = false, ended = false, looping = false;
        int slot = -1;
        std::uint32_t slotGeneration = 0, serial = 0;
        Instrument* instrument = nullptr;
        const LoadedRegion* region = nullptr;
        const LoadedSample* sample = nullptr;
        const LoopCache* cache = nullptr;
        double position = 0.0;
        std::int64_t seekBase = 0;

        // Shared with the disk thread.
        std::atomic<int> streaming { 0 };
        std::atomic<std::uint32_t> gen { 0 }, ringGen { 0xffffffffu };
        std::atomic<std::int64_t> seekTo { 0 }, writeFrame { 0 }, readFrame { 0 }, fillEnd { 0 };
        std::atomic<Instrument*> streamInstrument { nullptr };
        std::atomic<const LoadedSample*> streamSample { nullptr };
        float* ring = nullptr;               // ringFrames frames, two channels
    };

    struct Library::State
    {
        std::atomic<bool> dead { false };
        mutable std::mutex lock;             // the slots' paths, keys and problems; the graveyard
        std::atomic<int> numSlots { 0 };
        Slot slots[maxInstruments];
        Voice voices[maxVoices];
        std::unique_ptr<float[]> rings;
        std::atomic<std::uint64_t> entered { 0 }, left { 0 };
        std::int64_t now = 0;                // ms, at the block's start
        std::uint32_t random = 0x9e3779b9u, nextSerial = 1;
        bool registered = false;

        /** Instruments taken off their slots, kept until no voice can still play them. */
        struct Grave
        {
            std::shared_ptr<Instrument> instrument;
            std::uint64_t entered = 0;
        };

        std::vector<Grave> graveyard;

        /** A slot's instrument goes (the loading or message thread, under the lock). */
        void retire (Slot& slot)
        {
            slot.ready.store (nullptr);
            slot.generation.fetch_add (1);

            if (slot.holder != nullptr)
                graveyard.push_back ({ std::move (slot.holder), entered.load() });

            slot.holder.reset();
            slot.state.store (stella::blob::notLoaded);
        }
    };

    //==========================================================================
    /** The program's blob threads: one loads instruments, one reads ahead of every voice. */
    class Hub
    {
    public:
        /** Never destroyed: its threads stop when the last library goes, and a program ending with
            plugins still alive leaves them to the system (nothing of it is torn down under them). */
        static Hub& get()
        {
            static Hub* hub = new Hub();
            return *hub;
        }

        void add (const std::shared_ptr<Library::State>& state)
        {
            const std::lock_guard<std::mutex> serial (startStop);
            std::lock_guard<std::mutex> hold (lock);
            libraries.push_back (state);

            if (! running.load())
            {
                if (loader.joinable()) loader.join();
                if (disk.joinable())   disk.join();

                running.store (true);
                loader = std::thread ([this] { loadLoop(); });
                disk = std::thread ([this] { diskLoop(); });
            }
        }

        void remove (const Library::State* state)
        {
            // Held until the threads have stopped: an add meanwhile would set running again, and
            // the stopping threads would never see it false.
            const std::lock_guard<std::mutex> serial (startStop);
            std::thread stoppedLoader, stoppedDisk;

            {
                std::lock_guard<std::mutex> hold (lock);

                libraries.erase (std::remove_if (libraries.begin(), libraries.end(),
                                                 [state] (const std::shared_ptr<Library::State>& s) { return s.get() == state; }),
                                 libraries.end());

                if (libraries.empty() && running.load())
                {
                    running.store (false);
                    stoppedLoader = std::move (loader);
                    stoppedDisk = std::move (disk);
                }
            }

            wake.notify_all();

            if (stoppedLoader.joinable()) stoppedLoader.join();
            if (stoppedDisk.joinable())   stoppedDisk.join();

            // What they kept alive for a while goes with them.
            if (! running.load())
            {
                std::lock_guard<std::mutex> hold (lock);
                kept.clear();
            }
        }

    private:
        Hub() = default;

        std::vector<std::shared_ptr<Library::State>> snapshot()
        {
            std::lock_guard<std::mutex> hold (lock);
            return libraries;
        }

        //======================================================================
        /** An instrument from the ones already loaded (by any plugin), or loaded now. */
        std::shared_ptr<Instrument> instrumentFor (const std::string& path, const std::uint8_t* key, std::string& error, const std::atomic<bool>& stop)
        {
            auto name = path;

            if (key != nullptr)
                for (int i = 0; i < 32; ++i)
                    name += (char) ('a' + (key[i] & 15));

            {
                std::lock_guard<std::mutex> hold (lock);

                if (const auto found = cache.find (name); found != cache.end())
                    if (auto alive = found->second.lock())
                        return alive;
            }

            auto loaded = loadInstrument (path, key, error, stop);

            if (loaded != nullptr)
            {
                std::lock_guard<std::mutex> hold (lock);
                cache[name] = loaded;
                kept[name] = { loaded, detail::nowMs() };
            }

            return loaded;
        }

        void loadLoop()
        {
            std::int64_t lastLicenceCheck = detail::nowMs();

            while (running.load())
            {
                for (auto& library : snapshot())
                    if (! library->dead.load())
                        tend (*library);

                // Instruments no plugin plays any more stay a while (a plugin built again finds them).
                {
                    const auto now = detail::nowMs();
                    std::lock_guard<std::mutex> hold (lock);

                    for (auto& library : libraries)
                    {
                        const std::lock_guard<std::mutex> holdLibrary (library->lock);

                        for (int i = 0; i < maxInstruments; ++i)
                            if (const auto& holder = library->slots[i].holder; holder != nullptr)
                                for (auto& [name, entry] : kept)
                                    if (entry.first == holder)
                                        entry.second = now;
                    }

                    for (auto it = kept.begin(); it != kept.end();)
                        it = now - it->second.second > 30000 ? kept.erase (it) : std::next (it);

                    for (auto it = cache.begin(); it != cache.end();)
                        it = it->second.expired() ? cache.erase (it) : std::next (it);
                }

                // Protected libraries in use that aren't registered: registered elsewhere meanwhile?
                if (const auto now = detail::nowMs(); now - lastLicenceCheck > 2000)
                {
                    lastLicenceCheck = now;
                    recheckLicences();
                }

                std::unique_lock<std::mutex> hold (lock);
                wake.wait_for (hold, std::chrono::milliseconds (25), [this] { return ! running.load(); });
            }
        }

        /** One library: what's wanted loaded, what isn't any more let go, the graveyard emptied. */
        void tend (Library::State& d)
        {
            const auto now = detail::nowMs();
            const auto count = d.numSlots.load();

            for (int i = 0; i < count && running.load() && ! d.dead.load(); ++i)
            {
                auto& slot = d.slots[i];
                const auto sinceUse = now - slot.lastUse.load();
                const auto state = slot.state.load();

                if (state == stella::blob::notLoaded && sinceUse < 2000)
                {
                    std::string path;
                    std::uint8_t key[32];
                    bool hasKey;
                    std::uint32_t generation;

                    {
                        const std::lock_guard<std::mutex> hold (d.lock);
                        path = slot.path;
                        std::memcpy (key, slot.key, 32);
                        hasKey = slot.hasKey;
                        generation = slot.entryGeneration;
                        slot.state.store (stella::blob::loading);   // under the lock: a retire after it leaves notLoaded
                    }

                    std::string error;
                    auto loaded = path.empty() ? nullptr : instrumentFor (path, hasKey ? key : nullptr, error, d.dead);

                    if (path.empty())
                        error = "Its blob isn't on this computer.";

                    const std::lock_guard<std::mutex> hold (d.lock);

                    if (slot.entryGeneration != generation)
                        continue;   // the library changed meanwhile

                    if (loaded == nullptr)
                    {
                        slot.problem = error;
                        slot.state.store (stella::blob::unavailable);
                        continue;
                    }

                    std::memset (slot.seq, 0, sizeof (slot.seq));
                    slot.swLast = loaded->swDefault;
                    slot.problem.clear();
                    slot.holder = loaded;
                    slot.ready.store (loaded.get(), std::memory_order_release);
                    slot.state.store (stella::blob::ready);
                }
                else if (state == stella::blob::ready && sinceUse > 10000 && slot.voices.load() == 0)
                {
                    const std::lock_guard<std::mutex> hold (d.lock);
                    d.retire (slot);
                }
            }

            // The graveyard: an instrument goes once the plugin has played a block since it was
            // taken off (its voices are let go at a block's start), and the disk thread is past it.
            std::vector<std::shared_ptr<Instrument>> freed;

            {
                const std::lock_guard<std::mutex> hold (d.lock);
                const auto left = d.left.load();

                for (auto it = d.graveyard.begin(); it != d.graveyard.end();)
                {
                    if (left > it->entered)
                    {
                        freed.push_back (std::move (it->instrument));
                        it = d.graveyard.erase (it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }

            if (! freed.empty())
            {
                const std::lock_guard<std::mutex> pass (diskPass);
                freed.clear();
            }
        }

        //======================================================================
        void diskLoop()
        {
            std::vector<float> frames ((std::size_t) maxChunk * 2);
            std::vector<std::uint8_t> scratch;

            while (running.load())
            {
                bool worked = false;

                {
                    const std::lock_guard<std::mutex> pass (diskPass);

                    for (auto& library : snapshot())
                    {
                        if (library->dead.load() || library->rings == nullptr)
                            continue;

                        for (auto& voice : library->voices)
                            if (voice.streaming.load (std::memory_order_acquire) == 1)
                                worked = fill (voice, frames, scratch) || worked;
                    }
                }

                if (! worked)
                    std::this_thread::sleep_for (std::chrono::milliseconds (2));
            }
        }

        /** Reads ahead of one voice. True when it read something. */
        static bool fill (Voice& v, std::vector<float>& frames, std::vector<std::uint8_t>& scratch)
        {
            const auto g = v.gen.load (std::memory_order_acquire);
            auto* instrument = v.streamInstrument.load (std::memory_order_acquire);
            const auto* sample = v.streamSample.load (std::memory_order_acquire);

            if (instrument == nullptr || sample == nullptr || v.gen.load (std::memory_order_acquire) != g)
                return false;

            if (v.ringGen.load (std::memory_order_relaxed) != g)
            {
                v.writeFrame.store (v.seekTo.load (std::memory_order_relaxed), std::memory_order_relaxed);
                v.ringGen.store (g, std::memory_order_release);
            }

            const auto w = v.writeFrame.load (std::memory_order_relaxed);
            const auto r = v.readFrame.load (std::memory_order_acquire);
            const auto end = std::min (v.fillEnd.load (std::memory_order_relaxed), sample->frames);
            const auto target = std::min (end, r + ringFrames);

            if (target <= w || (target - w < minChunk && target < end))
                return false;

            const auto n = std::min<std::int64_t> (target - w, maxChunk);
            instrument->readFrames (*sample, w, n, frames.data(), scratch);

            if (v.gen.load (std::memory_order_acquire) != g)
                return true;

            const auto channels = sample->channels;

            for (std::int64_t k = 0; k < n; ++k)
            {
                auto* to = v.ring + (std::size_t) ((w + k) % ringFrames) * 2;
                const auto* from = frames.data() + (std::size_t) k * (std::size_t) channels;
                to[0] = from[0];
                to[1] = channels > 1 ? from[1] : from[0];
            }

            v.writeFrame.store (w + n, std::memory_order_release);
            return true;
        }

        static constexpr std::int64_t minChunk = 2048, maxChunk = 8192;

        std::mutex lock;                     // the libraries, the cache
        std::condition_variable wake;
        std::vector<std::shared_ptr<Library::State>> libraries;
        std::map<std::string, std::weak_ptr<Instrument>> cache;
        std::map<std::string, std::pair<std::shared_ptr<Instrument>, std::int64_t>> kept;
        std::mutex diskPass;                 // held while the disk thread reads
        std::mutex startStop;                // add and remove, each whole (a stop's joins too)
        std::atomic<bool> running { false };
        std::thread loader, disk;
    };

    //==========================================================================
    namespace
    {
        thread_local Library* currentLibrary = nullptr;

        /** A voice by its number ((serial << 8) | its place), while it's the one that number gave. */
        inline Voice* voiceFor (Library::State& d, int handle) noexcept
        {
            if (handle <= 0 || (handle & 0xff) >= maxVoices)
                return nullptr;

            auto& v = d.voices[handle & 0xff];
            return v.active && v.serial == ((std::uint32_t) handle >> 8) ? &v : nullptr;
        }

        inline void endVoice (Library::State& d, Voice& v) noexcept
        {
            if (! v.active)
                return;

            v.active = false;
            v.streaming.store (0, std::memory_order_release);

            if (v.slot >= 0 && v.slot < maxInstruments)
                d.slots[v.slot].voices.fetch_sub (1);
        }

        inline const LoadedRegion* regionOf (const Library::State& d, int instrument, int index, Instrument*& owner) noexcept
        {
            owner = nullptr;

            if (instrument < 0 || instrument >= d.numSlots.load (std::memory_order_relaxed))
                return nullptr;

            auto* inst = d.slots[instrument].ready.load (std::memory_order_acquire);

            if (inst == nullptr || index < 0 || index >= (int) inst->regions.size())
                return nullptr;

            owner = inst;
            return &inst->regions[(std::size_t) index];
        }
    }

    //==========================================================================
    Library::Library()
        : d (std::make_shared<State>())
    {
    }

    Library::~Library()
    {
        d->dead.store (true);

        if (d->registered)
            Hub::get().remove (d.get());
    }

    void Library::setEntries (const std::vector<Entry>& entries)
    {
        const auto count = (int) std::min<std::size_t> (entries.size(), (std::size_t) maxInstruments);

        {
            const std::lock_guard<std::mutex> hold (d->lock);
            const auto before = d->numSlots.load();
            d->numSlots.store (0);

            for (int i = 0; i < std::max (before, count); ++i)
            {
                auto& slot = d->slots[i];
                d->retire (slot);
                slot.entryGeneration++;
                slot.problem.clear();

                if (i < count)
                {
                    const auto& entry = entries[(std::size_t) i];
                    slot.path = entry.path;
                    std::memcpy (slot.key, entry.key, 32);
                    slot.hasKey = entry.hasKey;
                    detail::putText (reinterpret_cast<std::uint8_t*> (slot.name), sizeof (slot.name), entry.name);
                    slot.lastUse.store (i == 0 ? detail::nowMs() : -1000000000ll);   // the first loads at once
                }
                else
                {
                    slot.path.clear();
                    slot.name[0] = 0;
                }
            }

            if (count > 0 && d->rings == nullptr)
            {
                d->rings.reset (new float[(std::size_t) maxVoices * ringFrames * 2]());

                for (int v = 0; v < maxVoices; ++v)
                    d->voices[v].ring = d->rings.get() + (std::size_t) v * ringFrames * 2;
            }

            d->numSlots.store (count);
        }

        if (count > 0 && ! d->registered)
        {
            d->registered = true;
            Hub::get().add (d);
        }
    }

    int Library::stateOf (int instrument) const noexcept
    {
        return instrument >= 0 && instrument < d->numSlots.load() ? d->slots[instrument].state.load() : stella::blob::unavailable;
    }

    std::string Library::problemOf (int instrument) const
    {
        const std::lock_guard<std::mutex> hold (d->lock);
        return instrument >= 0 && instrument < maxInstruments ? d->slots[instrument].problem : std::string();
    }

    //==========================================================================
    void Library::begin() noexcept
    {
        d->entered.fetch_add (1);
        d->now = detail::nowMs();

        // Voices of an instrument that's gone stop here, before anything plays them; so do voices
        // whose sample has ended (a module that never stops them doesn't run out of voices).
        for (auto& v : d->voices)
            if (v.active && (v.ended || v.slot < 0 || d->slots[v.slot].generation.load() != v.slotGeneration))
                endVoice (*d, v);
    }

    void Library::end() noexcept
    {
        d->left.fetch_add (1);
    }

    int Library::count() const noexcept
    {
        return d->numSlots.load (std::memory_order_relaxed);
    }

    int Library::name (int instrument, char* text, int size) const noexcept
    {
        if (text == nullptr || size <= 0)
            return 0;

        text[0] = 0;

        if (instrument < 0 || instrument >= count())
            return 0;

        const auto& slot = d->slots[instrument];
        int length = 0;

        while (length < size - 1 && length < (int) sizeof (slot.name) && slot.name[length] != 0)
        {
            text[length] = slot.name[length];
            ++length;
        }

        text[length] = 0;
        return length;
    }

    int Library::state (int instrument) const noexcept
    {
        return instrument >= 0 && instrument < count() ? d->slots[instrument].state.load (std::memory_order_relaxed) : stella::blob::unavailable;
    }

    void Library::use (int instrument) noexcept
    {
        if (instrument >= 0 && instrument < count())
            d->slots[instrument].lastUse.store (d->now != 0 ? d->now : detail::nowMs(), std::memory_order_relaxed);
    }

    int Library::regions (int instrument) const noexcept
    {
        if (instrument < 0 || instrument >= count())
            return 0;

        const auto* inst = d->slots[instrument].ready.load (std::memory_order_acquire);
        return inst != nullptr ? (int) inst->regions.size() : 0;
    }

    bool Library::region (int instrument, int index, Region& info) const noexcept
    {
        Instrument* owner = nullptr;

        if (const auto* r = regionOf (*d, instrument, index, owner))
        {
            info = r->info;
            return true;
        }

        info = Region {};
        return false;
    }

    float Library::opcode (int instrument, int index, const char* opcodeName, float fallback) const noexcept
    {
        Instrument* owner = nullptr;
        const auto* r = regionOf (*d, instrument, index, owner);

        if (r == nullptr || opcodeName == nullptr)
            return fallback;

        const char* found = nullptr;

        for (const auto& [key, value] : r->opcodes)
            if (std::strcmp (key.c_str(), opcodeName) == 0)
                found = value.c_str();

        return found != nullptr && *found != 0 ? (float) detail::number (found) : fallback;
    }

    int Library::find (int instrument, int note, int velocity, int kind, int* found, int maxFound) noexcept
    {
        if (instrument < 0 || instrument >= count() || found == nullptr || maxFound <= 0)
            return 0;

        auto& slot = d->slots[instrument];
        const auto* inst = slot.ready.load (std::memory_order_acquire);

        if (inst == nullptr)
            return 0;

        // A keyswitch: the articulation changes, and the key itself doesn't sound.
        if (inst->swLow >= 0 && note >= inst->swLow && note <= inst->swHigh)
        {
            if (kind != releaseTrigger)
                slot.swLast = note;

            return 0;
        }

        d->random ^= d->random << 13;
        d->random ^= d->random >> 17;
        d->random ^= d->random << 5;
        const auto chance = (float) (d->random >> 8) / 16777216.0f;
        const bool wantRelease = kind == releaseTrigger;

        int touched[64];
        int numTouched = 0, numFound = 0;

        for (int k = 0; k < (int) inst->regions.size(); ++k)
        {
            const auto& r = inst->regions[(std::size_t) k];
            const auto& info = r.info;

            if ((info.trigger == releaseTrigger) != wantRelease
                || note < info.lokey || note > info.hikey || velocity < info.lovel || velocity > info.hivel
                || chance < r.lorand || chance >= r.hirand
                || (r.swLast >= 0 && r.swLast != slot.swLast))
                continue;

            if (r.seqGroup >= 0)
            {
                bool seen = false;

                for (int t = 0; t < numTouched; ++t)
                    seen = seen || touched[t] == r.seqGroup;

                if (! seen && numTouched < 64)
                    touched[numTouched++] = r.seqGroup;

                const auto turn = slot.seq[r.seqGroup & 511] % info.seqLength;

                if (turn + 1 != info.seqPosition)
                    continue;
            }

            if (numFound < maxFound)
                found[numFound++] = k;
        }

        for (int t = 0; t < numTouched; ++t)
            slot.seq[touched[t] & 511]++;

        return numFound;
    }

    int Library::start (int instrument, int index, double fromFrame) noexcept
    {
        if (instrument < 0 || instrument >= count())
            return 0;

        auto& slot = d->slots[instrument];
        auto* inst = slot.ready.load (std::memory_order_acquire);

        if (inst == nullptr || index < 0 || index >= (int) inst->regions.size())
            return 0;

        Voice* v = nullptr;

        for (auto& candidate : d->voices)
        {
            if (! candidate.active)
            {
                v = &candidate;
                break;
            }
        }

        if (v == nullptr)
            return 0;

        slot.voices.fetch_add (1);

        if (slot.ready.load() != inst)
        {
            slot.voices.fetch_sub (1);
            return 0;
        }

        const auto& r = inst->regions[(std::size_t) index];
        const auto& s = inst->samples[(std::size_t) r.sample];

        v->active = true;
        v->ended = r.end <= r.offset;
        v->slot = instrument;
        v->slotGeneration = slot.generation.load();
        v->instrument = inst;
        v->region = &r;
        v->sample = &s;
        v->cache = r.loop >= 0 ? &inst->loops[(std::size_t) r.loop] : nullptr;
        v->looping = r.loops;

        const auto first = fromFrame >= 0.0 ? std::min ((double) std::max<std::int64_t> (0, r.end - 1), fromFrame) : (double) r.offset;
        v->position = first;

        // What the disk reads ahead of it: past its head, and (while it loops) up to its loop's
        // frames, which are in memory; past them once it's let go.
        const auto head = s.headFrames;
        const auto whole = (std::int64_t) std::floor (first);
        std::int64_t seek, fillEnd = s.frames;

        if (whole < head)
            seek = head;
        else if (v->cache != nullptr && whole >= v->cache->start && whole < v->cache->end)
            seek = v->cache->end;
        else
            seek = std::max (head, whole - 2);

        if (v->cache != nullptr && seek < v->cache->start)
            fillEnd = v->cache->start;

        v->seekBase = seek;
        v->readFrame.store (seek, std::memory_order_relaxed);
        v->seekTo.store (seek, std::memory_order_relaxed);
        v->fillEnd.store (fillEnd, std::memory_order_relaxed);
        v->streamInstrument.store (inst, std::memory_order_relaxed);
        v->streamSample.store (&s, std::memory_order_relaxed);
        v->gen.fetch_add (1, std::memory_order_release);
        v->streaming.store (s.frames > head && d->rings != nullptr ? 1 : 0, std::memory_order_release);

        d->nextSerial = (d->nextSerial + 1) & 0x7fffff;

        if (d->nextSerial == 0)
            d->nextSerial = 1;

        v->serial = d->nextSerial;
        return (int) ((v->serial << 8) | (std::uint32_t) (v - d->voices));
    }

    int Library::render (int voice, float* left, float* right, int frames, float ratio) noexcept
    {
        if (frames <= 0)
            return 0;

        auto silence = [&] (int from)
        {
            for (int i = from; i < frames; ++i)
            {
                if (left != nullptr)  left[i] = 0.0f;
                if (right != nullptr) right[i] = 0.0f;
            }
        };

        auto* v = voiceFor (*d, voice);

        if (v == nullptr || v->ended || d->slots[v->slot].generation.load (std::memory_order_relaxed) != v->slotGeneration)
        {
            if (v != nullptr)
                v->ended = true;

            silence (0);
            return 0;
        }

        const auto& r = *v->region;
        const auto& s = *v->sample;
        const auto channels = s.channels;
        const auto head = s.headFrames;
        const auto* cache = v->cache;
        const auto end = r.end;
        const auto loopLength = (double) (r.loopEnd - r.loopStart);
        const auto step = ratio > 0.0f && ratio == ratio ? std::min (64.0, (double) ratio) : 0.0;

        // The disk's frames this block: what's read ahead, once it's read for this start.
        const bool ringReady = v->streaming.load (std::memory_order_relaxed) == 1
                            && v->ringGen.load (std::memory_order_acquire) == v->gen.load (std::memory_order_relaxed);
        const auto written = ringReady ? v->writeFrame.load (std::memory_order_acquire) : (std::int64_t) 0;
        const auto readFrom = v->readFrame.load (std::memory_order_relaxed);

        auto frameAt = [&] (std::int64_t f, float& l, float& rr) noexcept
        {
            if (v->looping && f >= r.loopEnd)
                f -= r.loopEnd - r.loopStart;

            if (f < 0 || f >= end)
            {
                l = rr = 0.0f;
                return;
            }

            const float* p;

            if (f < head)
                p = s.head.data() + (std::size_t) f * (std::size_t) channels;
            else if (cache != nullptr && f >= cache->start && f < cache->end)
                p = cache->data.data() + (std::size_t) (f - cache->start) * (std::size_t) channels;
            else if (ringReady && f >= readFrom && f < written)
            {
                const auto* q = v->ring + (std::size_t) (f % ringFrames) * 2;
                l = q[0];
                rr = q[1];
                return;
            }
            else
            {
                l = rr = 0.0f;   // not read yet
                return;
            }

            l = p[0];
            rr = channels > 1 ? p[1] : p[0];
        };

        auto hermite = [] (float xm1, float x0, float x1, float x2, float t) noexcept
        {
            const auto c1 = 0.5f * (x1 - xm1);
            const auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            return ((c3 * t + c2) * t + c1) * t + x0;
        };

        auto valueAt = [&] (double position, float& l, float& rr) noexcept
        {
            const auto whole = (std::int64_t) std::floor (position);
            const auto t = (float) (position - (double) whole);
            float l0, r0, l1, r1, l2, r2, l3, r3;
            frameAt (whole - 1, l0, r0);
            frameAt (whole, l1, r1);
            frameAt (whole + 1, l2, r2);
            frameAt (whole + 2, l3, r3);
            l = hermite (l0, l1, l2, l3, t);
            rr = hermite (r0, r1, r2, r3, t);
        };

        int produced = 0;

        for (; produced < frames; ++produced)
        {
            if (v->looping)
                while (v->position >= (double) r.loopEnd)
                    v->position -= loopLength;

            if (v->position >= (double) end)
            {
                v->ended = true;
                break;
            }

            float l, rr;
            valueAt (v->position, l, rr);

            // The loop's crossfade: its end fades into what comes before its start (equal power).
            if (v->looping && r.fade > 0 && v->position >= (double) (r.loopEnd - r.fade))
            {
                const auto t = (float) ((v->position - (double) (r.loopEnd - r.fade)) / (double) r.fade);
                float bl, br;
                valueAt (v->position - loopLength, bl, br);
                const auto out = std::cos (t * 1.5707963f), in = std::sin (t * 1.5707963f);
                l = l * out + bl * in;
                rr = rr * out + br * in;
            }

            if (left != nullptr && right != nullptr)
            {
                left[produced] = l;
                right[produced] = rr;
            }
            else if (left != nullptr)
            {
                left[produced] = 0.5f * (l + rr);
            }

            v->position += step;
        }

        silence (produced);

        // What it's past of the disk's frames can be read over.
        const auto now = (std::int64_t) std::floor (v->position) - 3;

        if (now > readFrom && now >= v->seekBase)
            v->readFrame.store (now, std::memory_order_release);

        return produced;
    }

    void Library::release (int voice) noexcept
    {
        auto* v = voiceFor (*d, voice);

        if (v == nullptr || ! v->looping || v->region->info.loopMode != loopSustain)
            return;

        // On past the loop. Inside the loop's frames (in memory), the disk reads what follows
        // them; before them, it just reads on.
        v->looping = false;
        const auto& s = *v->sample;
        const auto whole = (std::int64_t) std::floor (v->position);

        if (v->cache != nullptr && whole >= v->cache->start && v->cache->end < s.frames && v->seekBase != v->cache->end && d->rings != nullptr)
        {
            v->seekBase = v->cache->end;
            v->readFrame.store (v->cache->end, std::memory_order_relaxed);
            v->seekTo.store (v->cache->end, std::memory_order_relaxed);
            v->fillEnd.store (s.frames, std::memory_order_relaxed);
            v->gen.fetch_add (1, std::memory_order_release);
            v->streaming.store (1, std::memory_order_release);
        }
        else
        {
            v->fillEnd.store (s.frames, std::memory_order_relaxed);
        }
    }

    void Library::stop (int voice) noexcept
    {
        if (auto* v = voiceFor (*d, voice))
            endVoice (*d, *v);
    }

    bool Library::playing (int voice) const noexcept
    {
        const auto* v = voiceFor (*d, voice);
        return v != nullptr && ! v->ended;
    }

    int Library::demo (float* playSeconds, float* silentSeconds) const noexcept
    {
        const auto n = count();

        for (int i = 0; i < n; ++i)
        {
            const auto* inst = d->slots[i].ready.load (std::memory_order_acquire);
            const auto* record = inst != nullptr ? inst->licenceRecord.get() : nullptr;

            if (record != nullptr && ! record->registered.load (std::memory_order_acquire))
            {
                if (playSeconds != nullptr)   *playSeconds = record->soni.playSeconds;
                if (silentSeconds != nullptr) *silentSeconds = record->soni.silentSeconds;
                return 1;
            }
        }

        return 0;
    }

    Library* Library::current() noexcept
    {
        return currentLibrary;
    }

    Library::Scope::Scope (Library& l) noexcept
        : library (l), previous (currentLibrary)
    {
        currentLibrary = &library;
        library.begin();
    }

    Library::Scope::~Scope()
    {
        library.end();
        currentLibrary = previous;
    }
}

//==============================================================================
// An exported plugin: what its modules call (stella_api.h), and where its blobs are.
#if STELLA_BLOB_PLUGIN

#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <shlobj.h>
#else
 #include <dlfcn.h>
 #include <pwd.h>
 #include <unistd.h>
#endif

extern "C"
{
    using stella::blob::Library;

    int stella_blob_count()                                       { auto* l = Library::current(); return l != nullptr ? l->count() : 0; }
    int stella_blob_name (int i, char* text, int size)            { auto* l = Library::current(); return l != nullptr ? l->name (i, text, size) : 0; }
    int stella_blob_state (int i)                                 { auto* l = Library::current(); return l != nullptr ? l->state (i) : stella::blob::unavailable; }
    void stella_blob_use (int i)                                  { if (auto* l = Library::current()) l->use (i); }
    int stella_blob_regions (int i)                               { auto* l = Library::current(); return l != nullptr ? l->regions (i) : 0; }

    int stella_blob_region (int i, int r, stella::blob::Region* info)
    {
        auto* l = Library::current();
        return l != nullptr && info != nullptr && l->region (i, r, *info) ? 1 : 0;
    }

    float stella_blob_opcode (int i, int r, const char* name, float fallback)
    {
        auto* l = Library::current();
        return l != nullptr ? l->opcode (i, r, name, fallback) : fallback;
    }

    int stella_blob_find (int i, int note, int velocity, int trigger, int* found, int maxFound)
    {
        auto* l = Library::current();
        return l != nullptr ? l->find (i, note, velocity, trigger, found, maxFound) : 0;
    }

    int stella_blob_start (int i, int r, double from)              { auto* l = Library::current(); return l != nullptr ? l->start (i, r, from) : 0; }

    int stella_blob_render (int voice, float* left, float* right, int frames, float ratio)
    {
        if (auto* l = Library::current())
            return l->render (voice, left, right, frames, ratio);

        for (int i = 0; i < frames; ++i)
        {
            if (left != nullptr)  left[i] = 0.0f;
            if (right != nullptr) right[i] = 0.0f;
        }

        return 0;
    }

    void stella_blob_release (int voice)                          { if (auto* l = Library::current()) l->release (voice); }
    void stella_blob_stop (int voice)                             { if (auto* l = Library::current()) l->stop (voice); }
    int stella_blob_playing (int voice)                           { auto* l = Library::current(); return l != nullptr && l->playing (voice) ? 1 : 0; }

    /** 1 while the plugin's library (outside a library's scope: any in the plugin) plays a
        protected library this computer isn't registered for, with its demo's times. */
    int stella_blob_demo (float* playSeconds, float* silentSeconds)
    {
        if (auto* l = Library::current())
            return l->demo (playSeconds, silentSeconds);

        return stella::blob::demoInUse (playSeconds, silentSeconds);
    }
}

namespace stella::blob
{
    namespace detail
    {
        /** A name fit for a folder: the characters Windows forbids, and control characters, made
            underscores; no dot or space at its end. */
        inline std::string folderName (const char* text)
        {
            std::string out (text != nullptr ? text : "");

            for (auto& c : out)
                if ((unsigned char) c < 32 || std::strchr ("<>:\"/\\|?*", c) != nullptr)
                    c = '_';

            while (! out.empty() && (out.back() == '.' || out.back() == ' '))
                out.pop_back();

            return out.empty() ? std::string ("Plugin") : out;
        }

        inline std::string joined (std::string folder, const std::string& name)
        {
            if (folder.empty())
                return name;

           #if defined (_WIN32)
            if (folder.back() != '\\' && folder.back() != '/')
                folder += '\\';
           #else
            if (folder.back() != '/')
                folder += '/';
           #endif

            return folder + name;
        }

        inline std::string parentOf (const std::string& path)
        {
            const auto slash = path.find_last_of ("/\\");
            return slash == std::string::npos || slash == 0 ? std::string() : path.substr (0, slash);
        }

        inline std::string fileNameOf (const std::string& path)
        {
            const auto slash = path.find_last_of ("/\\");
            return slash == std::string::npos ? path : path.substr (slash + 1);
        }

        inline bool sameName (const std::string& a, const std::string& b)
        {
           #if defined (_WIN32) || defined (__APPLE__)
            if (a.size() != b.size())
                return false;

            for (std::size_t i = 0; i < a.size(); ++i)
                if (std::tolower ((unsigned char) a[i]) != std::tolower ((unsigned char) b[i]))
                    return false;

            return true;
           #else
            return a == b;
           #endif
        }

        static void anchor() {}

       #if defined (_WIN32)
        inline std::string narrow (const wchar_t* text)
        {
            if (text == nullptr)
                return {};

            const auto size = WideCharToMultiByte (CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);

            if (size <= 1)
                return {};

            std::string out ((std::size_t) size - 1, '\0');
            WideCharToMultiByte (CP_UTF8, 0, text, -1, &out[0], size, nullptr, nullptr);
            return out;
        }

        inline std::string knownFolder (const GUID& id)
        {
            PWSTR path = nullptr;
            std::string out;

            if (SHGetKnownFolderPath (id, 0, nullptr, &path) == S_OK)
                out = narrow (path);

            if (path != nullptr)
                CoTaskMemFree (path);

            return out;
        }

        inline std::string documentsFolder()
        {
            static const GUID documents { 0xFDD39AD0, 0x238F, 0x46AF, { 0xAD, 0xB4, 0x6C, 0x85, 0x48, 0x03, 0x69, 0xC7 } };
            return knownFolder (documents);
        }

        inline std::string dataFolder()
        {
            static const GUID roaming { 0x3EB685DB, 0x65F9, 0x4CF6, { 0xA0, 0x3A, 0xE3, 0xEF, 0x65, 0x72, 0x9F, 0x3D } };
            return knownFolder (roaming);
        }

        inline std::string pluginFile()
        {
            HMODULE module = nullptr;

            if (! GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                      reinterpret_cast<LPCWSTR> (&anchor), &module))
                return {};

            wchar_t path[4096] {};
            const auto length = GetModuleFileNameW (module, path, 4096);
            return length > 0 && length < 4096 ? narrow (path) : std::string();
        }
       #else
        inline std::string homeFolder()
        {
            if (const auto* home = std::getenv ("HOME"); home != nullptr && *home != 0)
                return home;

            if (const auto* entry = getpwuid (getuid()); entry != nullptr && entry->pw_dir != nullptr)
                return entry->pw_dir;

            return {};
        }

        inline std::string documentsFolder()    { return joined (homeFolder(), "Documents"); }

        inline std::string dataFolder()
        {
           #if defined (__APPLE__)
            return joined (joined (homeFolder(), "Library"), "Application Support");
           #else
            if (const auto* config = std::getenv ("XDG_CONFIG_HOME"); config != nullptr && *config != 0)
                return config;

            return joined (homeFolder(), ".config");
           #endif
        }

        inline std::string pluginFile()
        {
            Dl_info info {};
            return dladdr (reinterpret_cast<void*> (&anchor), &info) != 0 && info.dli_fname != nullptr ? std::string (info.dli_fname) : std::string();
        }
       #endif
    }

    std::string findPluginBlob (const std::string& fileName, const char* vendor, const char* plugin)
    {
        const auto ownFolder = detail::joined (detail::folderName (vendor), detail::folderName (plugin));

        // The plugin's list: folders, or the blobs themselves.
        if (auto* list = openFile (detail::joined (detail::joined (detail::dataFolder(), ownFolder), "library.txt"), false))
        {
            std::string text;
            char buffer[4096];

            for (std::size_t n; (n = std::fread (buffer, 1, sizeof (buffer), list)) > 0;)
                text.append (buffer, n);

            std::fclose (list);
            std::size_t start = 0;

            while (start < text.size())
            {
                auto end = text.find_first_of ("\r\n", start);

                if (end == std::string::npos)
                    end = text.size();

                auto line = text.substr (start, end - start);
                start = end + 1;

                while (! line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '"'))
                    line.pop_back();

                while (! line.empty() && (line.front() == ' ' || line.front() == '\t' || line.front() == '"'))
                    line.erase (0, 1);

                if (line.empty() || line[0] == '#')
                    continue;

                if (detail::sameName (detail::fileNameOf (line), fileName) && fileExists (line))
                    return line;

                if (const auto inside = detail::joined (line, fileName); fileExists (inside))
                    return inside;
            }
        }

        if (const auto home = detail::joined (detail::joined (detail::documentsFolder(), ownFolder), fileName); fileExists (home))
            return home;

        auto folder = detail::parentOf (detail::pluginFile());

        for (int up = 0; up < 4 && ! folder.empty(); ++up, folder = detail::parentOf (folder))
            if (const auto beside = detail::joined (folder, fileName); fileExists (beside))
                return beside;

        return {};
    }
}

#endif   // STELLA_BLOB_PLUGIN
#endif   // STELLA_BLOB_IMPLEMENTATION
