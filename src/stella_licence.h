// D:\workspace\Fanan_web_hub\src\stella_licence.h
//
// The protection of what's made in Sonify and leaves it (plugins, MIDI FX and blobs), the same
// in Sonify itself, in a plugin's WebAssembly, and in an exported VST3 / CLAP / AU plugin.
//
// How it works:
//   - Every protected product has its own serial formula, made once by the Protection tab and
//     kept with the project. It is saved, with the product's id, name and demo times, in a
//     file called "license.soni" that ships next to the plugin (its folder, or its .clap file)
//     or inside it (an Audio Unit, a Sonify plugin's folder, a MIDI FX's file), and next to a
//     protected blob. license.soni isn't a sound file: it's scrambled, so it looks like noise.
//   - Every computer has an ID: 9 digits, made from the motherboard's UUID (the C: drive's
//     volume number when the board reports none), the same in Sonify and in every exported
//     plugin on that computer (computerId).
//   - The buyer gives the creator that ID; the creator's site runs the formula on it and
//     gives back a serial: 10 digits. The creator gets the formula as JavaScript, PHP and
//     Excel (the Protection tab), so Fanan has no part in it.
//   - The serial is typed or pasted into the plugin's Registration window, checked against
//     the formula, and kept in a key file in the user's application data ("Sonify Licences"
//     folder, one file per product: <product id>.key). Every start, the plugin reads its
//     license.soni and the key file and checks the serial still fits this computer's ID.
//   - Not registered: the demo. A plugin plays its sound for a while, then goes silent for a
//     while (each change faded over 10 ms); a MIDI FX passes notes for a while, then holds
//     them back for a while. The creator picks both times.
//
// The formula, three numbers A, B and M per product:
//     serial = (ID x A + B) mod M, written with 10 digits (zeros in front when shorter)
// ID x A stays below 9 x 10^15: exact in 64-bit integers (PHP), and in JavaScript with BigInt.
//
// Plain C++17, no JUCE. STELLA_LICENCE_NATIVE (an exported plugin; Sonify sets it in
// Licences.cpp) adds what needs the computer: its ID, the key files, finding license.soni.
// The token Sonify writes into a plugin's shared block (tokenFor) is how a plugin running
// inside Sonify (WebAssembly) learns it's registered: Sonify does the checking for it.

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace stella::licence
{
    constexpr double playSeconds = 10.0, silentSeconds = 2.0, fadeSeconds = 0.01;   // the demo, unless the creator chose others
    constexpr double midiPassSeconds = 10.0, midiHoldSeconds = 3.0;

    constexpr int idLength = 9, serialLength = 10;
    constexpr std::uint64_t minA = 1000000ull, maxA = 8999999ull;              // ID x A < 9 x 10^15
    constexpr std::uint64_t minM = 6000000000ull, maxM = 9999999999ull;        // a 10-digit serial
    constexpr const char* fileName = "license.soni";

    //==========================================================================
    /** A product's token: FNV-1a of "sonify-licence:" and its id, never 0. Sonify writes it into
        the plugin's shared block while the product is registered on this computer. */
    inline std::uint32_t tokenFor (const char* product) noexcept
    {
        std::uint32_t hash = 2166136261u;

        for (const auto* p = "sonify-licence:"; *p != 0; ++p)
            hash = (hash ^ (std::uint8_t) *p) * 16777619u;

        for (const auto* p = product; p != nullptr && *p != 0; ++p)
            hash = (hash ^ (std::uint8_t) *p) * 16777619u;

        hash ^= 0x5a17c0deu;
        return hash != 0 ? hash : 1u;
    }

    //==========================================================================
    /** An ID or a serial as typed or pasted, with only its digits left (dashes, spaces and
        anything else taken out). */
    inline std::string clean (const char* text)
    {
        std::string out;

        for (const auto* p = text; p != nullptr && *p != 0; ++p)
            if (*p >= '0' && *p <= '9')
                out += *p;

        return out;
    }

    /** Easier to read and type: an ID as 482-039-175, a serial as 16114-17732. */
    inline std::string grouped (const std::string& code)
    {
        const size_t group = code.size() == (size_t) idLength ? 3 : code.size() == (size_t) serialLength ? 5 : 4;
        std::string out;

        for (size_t i = 0; i < code.size(); ++i)
        {
            if (i > 0 && i % group == 0)
                out += '-';

            out += code[i];
        }

        return out;
    }

    //==========================================================================
    /** One product's serial formula: serial = (ID x A + B) mod M, as its creator's site runs it. */
    struct Formula
    {
        std::uint64_t a = 0;                           // minA .. maxA
        std::uint64_t b = 0;                           // 0 .. m-1
        std::uint64_t m = 0;                           // minM .. maxM

        bool isValid() const noexcept
        {
            return a >= minA && a <= maxA && m >= minM && m <= maxM && b < m;
        }

        /** The serial for an ID (any form: dashes, spaces), 10 digits; empty when the ID isn't
            9 digits. */
        std::string serialFor (const char* id) const
        {
            const auto code = clean (id);

            if ((int) code.size() != idLength || ! isValid())
                return {};

            std::uint64_t value = 0;

            for (auto c : code)
                value = value * 10 + (std::uint64_t) (c - '0');

            const auto serial = (value * a + b) % m;      // value * a < 9 x 10^15: no overflow
            auto text = std::to_string (serial);

            while ((int) text.size() < serialLength)
                text.insert (text.begin(), '0');

            return text;
        }

        /** Whether a serial (as typed: dashes, spaces...) is the one for this ID. */
        bool fits (const char* id, const char* serial) const
        {
            const auto expected = serialFor (id);
            return ! expected.empty() && expected == clean (serial);
        }

        /** A new formula from good randomness (the Protection tab passes 64 bytes). */
        static Formula make (const std::uint8_t* random, size_t size) noexcept
        {
            std::uint64_t state = 0x9e3779b97f4a7c15ull;

            for (size_t i = 0; i < size; ++i)
                state = (state ^ random[i]) * 0x100000001b3ull;

            auto next = [&state]() noexcept
            {
                state ^= state >> 12;
                state ^= state << 25;
                state ^= state >> 27;
                return state * 0x2545f4914f6cdd1dull;
            };

            Formula f;
            f.a = minA + next() % (maxA - minA + 1);
            f.m = minM + next() % (maxM - minM + 1);
            f.b = next() % f.m;
            return f;
        }
    };

    //==========================================================================
    /** What license.soni holds. */
    struct Soni
    {
        std::string product;                            // the product's id, for good (its key file is named after it)
        std::string name;                               // what it's called, as the Registration window says it
        std::string vendor;                             // who made it
        Formula formula;
        float playSeconds = (float) licence::playSeconds, silentSeconds = (float) licence::silentSeconds;   // the sound's demo
        float passSeconds = (float) midiPassSeconds, holdSeconds = (float) midiHoldSeconds;                // a MIDI FX's demo

        bool isValid() const noexcept    { return ! product.empty() && formula.isValid(); }
    };

    namespace detail
    {
        constexpr std::uint8_t magic[4] = { 0x53, 0x4f, 0x4e, 0x49 };   // "SONI"
        constexpr std::uint8_t version = 2;   // 2: the one-line formula (A, B, M)
        constexpr size_t fileSize = 4096;                              // padded with noise to this

        /** The keystream the payload is scrambled with: the same for every file (it hides the
            formula from a glance, nothing more). */
        struct Stream
        {
            std::uint32_t a = 0x7f4a7c15u, b = 0x9e3779b9u, c = 0x85ebca6bu, d = 0xc2b2ae35u;

            std::uint8_t next() noexcept
            {
                const auto t = a ^ (a << 11);
                a = b; b = c; c = d;
                d = d ^ (d >> 19) ^ t ^ (t >> 8);
                return (std::uint8_t) (d >> 24);
            }
        };

        inline std::uint32_t fnv (const std::uint8_t* data, size_t size) noexcept
        {
            std::uint32_t hash = 2166136261u;

            for (size_t i = 0; i < size; ++i)
                hash = (hash ^ data[i]) * 16777619u;

            return hash;
        }

        inline void put32 (std::vector<std::uint8_t>& out, std::uint32_t v)
        {
            for (int i = 0; i < 4; ++i)
                out.push_back ((std::uint8_t) (v >> (8 * i)));
        }

        inline void putFloat (std::vector<std::uint8_t>& out, float f)
        {
            std::uint32_t v = 0;
            std::memcpy (&v, &f, 4);
            put32 (out, v);
        }

        inline void putText (std::vector<std::uint8_t>& out, const std::string& text)
        {
            const auto size = text.size() < 1000 ? text.size() : (size_t) 1000;
            out.push_back ((std::uint8_t) (size & 0xff));
            out.push_back ((std::uint8_t) (size >> 8));
            out.insert (out.end(), text.begin(), text.begin() + (std::ptrdiff_t) size);
        }

        struct Reader
        {
            const std::uint8_t* data;
            size_t size, at = 0;
            bool ok = true;

            std::uint32_t get32() noexcept
            {
                if (at + 4 > size) { ok = false; return 0; }
                std::uint32_t v = 0;
                for (int i = 0; i < 4; ++i)
                    v |= (std::uint32_t) data[at + (size_t) i] << (8 * i);
                at += 4;
                return v;
            }

            float getFloat() noexcept
            {
                const auto v = get32();
                float f = 0.0f;
                std::memcpy (&f, &v, 4);
                return f;
            }

            std::string getText()
            {
                if (at + 2 > size) { ok = false; return {}; }
                const auto length = (size_t) data[at] | ((size_t) data[at + 1] << 8);
                at += 2;
                if (length > 1000 || at + length > size) { ok = false; return {}; }
                std::string text ((const char*) data + at, length);
                at += length;
                return text;
            }
        };

        inline float sane (float value, float fallback) noexcept
        {
            return value == value && value >= 0.0f && value <= 3600.0f ? value : fallback;   // NaN or out of range: the default
        }
    }

    /** license.soni's bytes for a product. noise: bytes (any) the file is padded out with. */
    inline std::vector<std::uint8_t> write (const Soni& soni, const std::uint8_t* noise, size_t noiseSize)
    {
        std::vector<std::uint8_t> payload;
        detail::putText (payload, soni.product);
        detail::putText (payload, soni.name);
        detail::putText (payload, soni.vendor);
        for (auto v : { soni.formula.a, soni.formula.b, soni.formula.m })
        {
            detail::put32 (payload, (std::uint32_t) (v & 0xffffffffu));
            detail::put32 (payload, (std::uint32_t) (v >> 32));
        }

        detail::putFloat (payload, soni.playSeconds);
        detail::putFloat (payload, soni.silentSeconds);
        detail::putFloat (payload, soni.passSeconds);
        detail::putFloat (payload, soni.holdSeconds);

        std::vector<std::uint8_t> out (detail::magic, detail::magic + 4);
        out.push_back (detail::version);
        out.push_back (0);
        out.push_back (0);
        out.push_back (0);
        detail::put32 (out, (std::uint32_t) payload.size());
        detail::put32 (out, detail::fnv (payload.data(), payload.size()));

        detail::Stream stream;

        for (auto byte : payload)
            out.push_back ((std::uint8_t) (byte ^ stream.next()));

        for (size_t i = 0; out.size() < detail::fileSize; ++i)
            out.push_back (noise != nullptr && noiseSize > 0 ? (std::uint8_t) (noise[i % noiseSize] ^ stream.next()) : stream.next());

        return out;
    }

    /** license.soni's bytes read back; false (soni untouched) when they aren't one. */
    inline bool read (const std::uint8_t* data, size_t size, Soni& soni)
    {
        if (data == nullptr || size < 16 || std::memcmp (data, detail::magic, 4) != 0 || data[4] != detail::version)
            return false;

        detail::Reader header { data, size, 8 };
        const auto length = header.get32();
        const auto check = header.get32();

        if (! header.ok || length < 16 || length > size - 16)
            return false;

        std::vector<std::uint8_t> payload (length);
        detail::Stream stream;

        for (size_t i = 0; i < length; ++i)
            payload[i] = (std::uint8_t) (data[16 + i] ^ stream.next());

        if (detail::fnv (payload.data(), payload.size()) != check)
            return false;

        detail::Reader in { payload.data(), payload.size() };
        Soni found;
        found.product = in.getText();
        found.name = in.getText();
        found.vendor = in.getText();
        auto get64 = [&in]
        {
            const auto low = (std::uint64_t) in.get32();
            return low | ((std::uint64_t) in.get32() << 32);
        };

        found.formula.a = get64();
        found.formula.b = get64();
        found.formula.m = get64();

        found.playSeconds = detail::sane (in.getFloat(), (float) playSeconds);
        found.silentSeconds = detail::sane (in.getFloat(), (float) silentSeconds);
        found.passSeconds = detail::sane (in.getFloat(), (float) midiPassSeconds);
        found.holdSeconds = detail::sane (in.getFloat(), (float) midiHoldSeconds);

        if (! in.ok || ! found.isValid())
            return false;

        soni = found;
        return true;
    }

    //==========================================================================
    /** A computer's ID from what identifies it (its motherboard's UUID, or the system drive's
        volume number): only its letters and digits count, in any case, so Sonify and the
        plugins get the same ID however they read it. 9 digits, never starting with 0. */
    inline std::string computerIdFrom (const std::string& source)
    {
        std::string text = "sonify-computer:";

        for (auto c : source)
        {
            if (c >= 'a' && c <= 'z')
                c = (char) (c - 'a' + 'A');

            if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z'))
                text += c;
        }

        std::uint64_t hash = 0xcbf29ce484222325ull;   // FNV-1a, 64 bits

        for (auto c : text)
            hash = (hash ^ (std::uint8_t) c) * 0x100000001b3ull;

        hash ^= hash >> 31;
        return std::to_string (100000000ull + hash % 900000000ull);
    }

    /** A key file's text: the product and the serial, as the plugin keeps them. */
    inline std::string keyText (const std::string& product, const std::string& serial)
    {
        return "Sonify licence key\nproduct=" + product + "\nserial=" + grouped (clean (serial.c_str())) + "\n";
    }

    /** The serial in a key file's text, for the product (empty: none, or another product's). */
    inline std::string serialInKey (const std::string& text, const std::string& product)
    {
        std::string foundProduct, serial;
        size_t at = 0;

        while (at < text.size())
        {
            auto end = text.find ('\n', at);
            if (end == std::string::npos) end = text.size();
            auto line = text.substr (at, end - at);
            at = end + 1;

            while (! line.empty() && (line.back() == '\r' || line.back() == ' '))
                line.pop_back();

            if (line.rfind ("product=", 0) == 0) foundProduct = line.substr (8);
            if (line.rfind ("serial=", 0) == 0)  serial = line.substr (7);
        }

        return foundProduct == product ? serial : std::string();
    }

    //==========================================================================
    /** The demo's silences, counted in samples from when it started playing. */
    class DemoGate
    {
    public:
        /** The creator's demo times (seconds); 0 or less: the defaults. */
        void setTimes (double play, double silent) noexcept
        {
            playTime = play > 0.0 ? play : playSeconds;
            silentTime = silent > 0.0 ? silent : silentSeconds;
        }

        /** Licensed: the sound as it is. Otherwise its gain follows the demo's cycle. */
        void process (float* left, float* right, int frames, double sampleRate, bool licensed) noexcept
        {
            if (frames <= 0)
                return;

            if (licensed)
            {
                position += (std::uint64_t) frames;
                return;
            }

            const auto rate = sampleRate > 1000.0 ? sampleRate : 48000.0;
            const auto play = (std::uint64_t) (playTime * rate);
            const auto cycle = play + (std::uint64_t) (silentTime * rate);
            const auto fade = (std::uint64_t) (fadeSeconds * rate) + 1;

            if (cycle == 0 || play == 0)
                return;

            for (int i = 0; i < frames; ++i, ++position)
            {
                const auto inCycle = position % cycle;
                float gain = 1.0f;

                if (inCycle >= play)
                    gain = 0.0f;                                                           // the silence
                else if (inCycle + fade > play)
                    gain = (float) (play - inCycle) / (float) fade;                        // fading out into it
                else if (inCycle < fade && position >= cycle)
                    gain = (float) inCycle / (float) fade;                                 // fading in after it

                if (gain < 1.0f)
                {
                    if (left != nullptr)  left[i] *= gain;
                    if (right != nullptr) right[i] *= gain;
                }
            }
        }

        /** A MIDI FX's demo: whether notes pass at this point of the cycle (then moves on). */
        bool passesMidi (int frames, double sampleRate, bool licensed) noexcept
        {
            const auto rate = sampleRate > 1000.0 ? sampleRate : 48000.0;
            const auto pass = (std::uint64_t) (playTime * rate);
            const auto cycle = pass + (std::uint64_t) (silentTime * rate);
            const auto here = cycle > 0 ? position % cycle : 0;
            position += (std::uint64_t) (frames > 0 ? frames : 0);
            return licensed || cycle == 0 || here < pass;
        }

        void reset() noexcept    { position = 0; }

    private:
        std::uint64_t position = 0;
        double playTime = playSeconds, silentTime = silentSeconds;
    };

   #if STELLA_PROTECTED
    #ifndef STELLA_PRODUCT
     #include "stella_product.h"   // written by the build: #define STELLA_PRODUCT "<the product's id>" (and the demo's times)
    #endif

    /** The product's id, built into the plugin. */
    inline const char* product() noexcept    { return STELLA_PRODUCT; }

    /** Whether the token Sonify wrote is this plugin's. */
    inline bool isLicensed (std::uint32_t token) noexcept    { return token != 0 && token == tokenFor (STELLA_PRODUCT); }
   #endif
}

//==============================================================================
// What needs the computer: an exported plugin, and Sonify (Licences.cpp). Not in WebAssembly.
#if STELLA_LICENCE_NATIVE && ! defined (__wasm__)

 #if defined (_WIN32)
  #ifndef NOMINMAX
   #define NOMINMAX
  #endif
  #ifndef WIN32_LEAN_AND_MEAN
   #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
 #else
  #include <dlfcn.h>
  #include <sys/stat.h>
  #include <unistd.h>
  #include <cstdlib>
 #endif

 #include <cstdio>

namespace stella::licence::native
{
   #if defined (_WIN32)
    inline std::wstring wide (const std::string& utf8)
    {
        if (utf8.empty())
            return {};

        const auto size = MultiByteToWideChar (CP_UTF8, 0, utf8.data(), (int) utf8.size(), nullptr, 0);
        std::wstring out ((size_t) (size > 0 ? size : 0), L'\0');

        if (size > 0)
            MultiByteToWideChar (CP_UTF8, 0, utf8.data(), (int) utf8.size(), out.data(), size);

        return out;
    }

    inline std::string utf8 (const wchar_t* text)
    {
        if (text == nullptr || *text == 0)
            return {};

        const auto size = WideCharToMultiByte (CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
        std::string out ((size_t) (size > 1 ? size - 1 : 0), '\0');

        if (size > 1)
            WideCharToMultiByte (CP_UTF8, 0, text, -1, out.data(), size, nullptr, nullptr);

        return out;
    }

    constexpr char separator = '\\';
   #else
    constexpr char separator = '/';
   #endif

    /** A UUID's 16 bytes as hex, or "" when it's a dummy (all the same byte: zeros, F's). */
    inline std::string uuidText (const unsigned char* uuid)
    {
        bool same = true;

        for (int i = 1; i < 16; ++i)
            same = same && uuid[i] == uuid[0];

        if (same)
            return {};

        static const char* hex = "0123456789ABCDEF";
        std::string out;

        for (int i = 0; i < 16; ++i)
        {
            out += hex[uuid[i] >> 4];
            out += hex[uuid[i] & 15];
        }

        return out;
    }

    /** What identifies this computer (see computerIdFrom): its motherboard's UUID (the BIOS
        system UUID; a Mac's hardware UUID), or when the board reports none, the system drive's
        volume number. Empty when neither can be read. */
    inline std::string computerSource()
    {
       #if defined (_WIN32)
        // The SMBIOS tables, as Windows hands them out (no administrator rights needed): the
        // System Information structure (type 1) holds the UUID at offset 8.
        const auto size = GetSystemFirmwareTable ('RSMB', 0, nullptr, 0);

        if (size > 8)
        {
            std::vector<unsigned char> raw (size);

            if (GetSystemFirmwareTable ('RSMB', 0, raw.data(), size) == size)
            {
                const auto length = (size_t) raw[4] | ((size_t) raw[5] << 8) | ((size_t) raw[6] << 16) | ((size_t) raw[7] << 24);
                const auto* table = raw.data() + 8;
                const auto end = length < (size_t) size - 8 ? length : (size_t) size - 8;
                size_t at = 0;

                while (at + 4 <= end)
                {
                    const auto type = table[at], formatted = table[at + 1];

                    if (formatted < 4 || at + formatted > end)
                        break;

                    if (type == 1 && formatted >= 0x19)
                    {
                        const auto text = uuidText (table + at + 8);

                        if (! text.empty())
                            return text;

                        break;
                    }

                    if (type == 127)
                        break;

                    // Past the structure and its strings (they end with two zero bytes).
                    auto next = at + formatted;

                    while (next + 1 < end && ! (table[next] == 0 && table[next + 1] == 0))
                        ++next;

                    at = next + 2;
                }
            }
        }

        // No usable UUID: the Windows drive's volume serial number.
        wchar_t windows[MAX_PATH] = {};

        if (GetWindowsDirectoryW (windows, MAX_PATH) >= 2)
        {
            const wchar_t root[] = { windows[0], L':', L'\\', 0 };
            DWORD serial = 0;

            if (GetVolumeInformationW (root, nullptr, 0, &serial, nullptr, nullptr, nullptr, 0) && serial != 0)
            {
                char text[24] = {};
                std::snprintf (text, sizeof (text), "VOL%08lX", (unsigned long) serial);
                return text;
            }
        }

        return {};
       #elif defined (__APPLE__)
        unsigned char uuid[16] = {};
        const struct timespec wait = { 1, 0 };

        if (gethostuuid (uuid, &wait) != 0)
            return {};

        return uuidText (uuid);
       #else
        // The board's UUID (readable when the system allows it), else the system's machine-id.
        for (const char* path : { "/sys/class/dmi/id/product_uuid", "/etc/machine-id", "/var/lib/dbus/machine-id" })
        {
            if (auto* f = std::fopen (path, "rb"))
            {
                char line[128] = {};
                const auto got = std::fread (line, 1, sizeof (line) - 1, f);
                std::fclose (f);

                if (got > 0)
                    return std::string (line, got);
            }
        }

        return {};
       #endif
    }

    /** This computer's ID, 9 digits without dashes ("" when the computer can't be read). */
    inline std::string computerId()
    {
        const auto source = computerSource();
        return source.empty() ? std::string() : computerIdFrom (source);
    }

    inline bool readFile (const std::string& path, std::vector<std::uint8_t>& out, size_t limit = 1 << 20)
    {
       #if defined (_WIN32)
        auto* f = _wfopen (wide (path).c_str(), L"rb");
       #else
        auto* f = std::fopen (path.c_str(), "rb");
       #endif

        if (f == nullptr)
            return false;

        out.clear();
        std::uint8_t buffer[4096];

        for (;;)
        {
            const auto got = std::fread (buffer, 1, sizeof (buffer), f);

            if (got == 0)
                break;

            out.insert (out.end(), buffer, buffer + got);

            if (out.size() > limit)
            {
                std::fclose (f);
                return false;
            }
        }

        std::fclose (f);
        return true;
    }

    inline bool makeFolder (const std::string& path)
    {
       #if defined (_WIN32)
        const auto w = wide (path);
        return CreateDirectoryW (w.c_str(), nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
       #else
        return mkdir (path.c_str(), 0755) == 0 || access (path.c_str(), F_OK) == 0;
       #endif
    }

    /** The folder key files are kept in: "Sonify Licences" in the user's application data
        (Windows: %APPDATA%; Mac: ~/Library/Application Support; Linux: ~/.config). Made when
        making is true. Empty when there's none. */
    inline std::string keyFolder (bool making)
    {
        std::string base;

       #if defined (_WIN32)
        if (const auto* appData = _wgetenv (L"APPDATA"))
            base = utf8 (appData);
       #elif defined (__APPLE__)
        if (const auto* home = std::getenv ("HOME"))
            base = std::string (home) + "/Library/Application Support";
       #else
        if (const auto* config = std::getenv ("XDG_CONFIG_HOME"); config != nullptr && *config != 0)
            base = config;
        else if (const auto* home = std::getenv ("HOME"))
            base = std::string (home) + "/.config";
       #endif

        if (base.empty())
            return {};

        const auto folder = base + separator + "Sonify Licences";

        if (making && ! makeFolder (folder))
            return {};

        return folder;
    }

    /** The key file's path for a product. */
    inline std::string keyFile (const std::string& product, bool making = false)
    {
        const auto folder = keyFolder (making);
        return folder.empty() || product.empty() ? std::string() : folder + separator + product + ".key";
    }

    /** The serial kept on this computer for a product (empty: none). */
    inline std::string keptSerial (const std::string& product)
    {
        std::vector<std::uint8_t> bytes;

        if (! readFile (keyFile (product), bytes, 16 * 1024))
            return {};

        return serialInKey (std::string (bytes.begin(), bytes.end()), product);
    }

    /** Keeps a serial for a product (writes its key file). */
    inline bool keepSerial (const std::string& product, const std::string& serial)
    {
        const auto path = keyFile (product, true);

        if (path.empty())
            return false;

        const auto text = keyText (product, serial);

       #if defined (_WIN32)
        auto* f = _wfopen (wide (path).c_str(), L"wb");
       #else
        auto* f = std::fopen (path.c_str(), "wb");
       #endif

        if (f == nullptr)
            return false;

        const auto wrote = std::fwrite (text.data(), 1, text.size(), f) == text.size();
        return std::fclose (f) == 0 && wrote;
    }

    /** The full path of the binary this code is in (the plugin's .dll/.clap/.so, or the
        executable inside a bundle). */
    inline std::string modulePath()
    {
       #if defined (_WIN32)
        HMODULE module = nullptr;

        if (! GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                  reinterpret_cast<LPCWSTR> (&modulePath), &module))
            return {};

        std::wstring buffer (32768, L'\0');
        const auto length = GetModuleFileNameW (module, buffer.data(), (DWORD) buffer.size());
        buffer.resize (length);
        return utf8 (buffer.c_str());
       #else
        Dl_info info {};

        if (dladdr (reinterpret_cast<void*> (&modulePath), &info) == 0 || info.dli_fname == nullptr)
            return {};

        return info.dli_fname;
       #endif
    }

    inline std::string parentOf (const std::string& path)
    {
        auto end = path.size();

        while (end > 1 && (path[end - 1] == '/' || path[end - 1] == '\\'))
            --end;

        const auto at = path.find_last_of ("/\\", end - 1);
        return at == std::string::npos ? std::string() : path.substr (0, at);
    }

    /** license.soni for a product: looked for next to the binary and the folders above it (a
        VST3 or AU bundle's folder, the folder a .clap file is in), and in a bundle's Resources.
        The first one that is this product's. */
    inline bool findSoni (const std::string& product, Soni& soni, std::string* where = nullptr)
    {
        std::vector<std::string> folders;
        auto folder = parentOf (modulePath());

        for (int up = 0; up < 5 && ! folder.empty(); ++up)
        {
            folders.push_back (folder);
            folders.push_back (folder + separator + "Resources");
            folder = parentOf (folder);
        }

        for (const auto& candidate : folders)
        {
            std::vector<std::uint8_t> bytes;
            Soni found;
            const auto path = candidate + separator + fileName;

            if (readFile (path, bytes, 64 * 1024) && read (bytes.data(), bytes.size(), found) && found.product == product)
            {
                soni = found;

                if (where != nullptr)
                    *where = path;

                return true;
            }
        }

        return false;
    }

    /** license.soni from a given folder (a blob's): false when it isn't there or isn't one. */
    inline bool soniIn (const std::string& folder, Soni& soni)
    {
        std::vector<std::uint8_t> bytes;
        return readFile (folder + separator + fileName, bytes, 64 * 1024) && read (bytes.data(), bytes.size(), soni);
    }

    /** Whether this computer is registered for the product its license.soni describes. */
    inline bool isRegistered (const Soni& soni)
    {
        if (! soni.isValid())
            return false;

        const auto id = computerId();
        const auto serial = keptSerial (soni.product);
        return ! id.empty() && ! serial.empty() && soni.formula.fits (id.c_str(), serial.c_str());
    }

    /** Registers this computer with a serial (typed or pasted): checks it fits, then keeps it.
        Returns "" when it's done, else why not. */
    inline std::string registerSerial (const Soni& soni, const std::string& serial)
    {
        if (! soni.isValid())
            return "This plugin's license.soni file is missing or damaged. Reinstall it with its license.soni.";

        const auto id = computerId();

        if (id.empty())
            return "This computer's ID can't be read.";

        if (clean (serial.c_str()).size() != (size_t) serialLength)
            return "A serial has 10 digits.";

        if (! soni.formula.fits (id.c_str(), serial.c_str()))
            return "That serial isn't for this computer's ID. Check you sent the right ID.";

        if (! keepSerial (soni.product, serial))
            return "The serial is right, but it couldn't be saved on this computer.";

        return {};
    }
}
#endif
