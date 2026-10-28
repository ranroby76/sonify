// D:\workspace\Fanan_web_hub\src\DeviceFiles.h

#pragma once

#include <juce_core/juce_core.h>

#include "Licences.h"

#include <atomic>
#include <map>

//==============================================================================
/**
    Sonify devices on disk, and the compiler that makes them.

    A device is one file, <name>.sip, with every other Sonify plugin in <user data>\Plugins
    (SipFiles.h: its kind "midi"; before, a <name>.sonidev in <user data>\Devices): a zip holding
        sip.json        what kind of Sonify plugin it is: a MIDI FX (Sip::Manifest)
        device.json     what it is: its id (the same for good), name, description, version
        source\*        its code (C++ against sonify_device.h)
        module.wasm     its last good build (any Sonify plays it without compiling)
        chat.json       the conversation that built it, so Sonify AI can carry on
        license.soni    a protected one's: its product and serial formula (stella_licence.h)
    Share the file and anyone's Sonify can play it, or open it in the Device Studio.

    A protected MIDI FX (its maker turned protection on in the Protection tab) carries its
    license.soni inside: until it's registered on a computer, it plays its demo there (notes
    pass for a while, then are held back for a while). Its maker's computer keeps its
    protection (protection.json) in My Devices\Protection\<its id>: every save writes
    license.soni from it. Anywhere else, the file keeps the license.soni it came with, and
    the MIDI FX plays but never opens for editing (isLocked).

    The compiler ships with Sonify ("Device Compiler" next to it: clang and the WebAssembly
    libraries of wasi-sdk), so nothing has to be installed.
*/
namespace Devices
{
    constexpr const char* fileExtension = ".sip";         // SipFiles.h: one format for every Sonify plugin
    constexpr const char* oldFileExtension = ".sonidev";  // what it was before (read, never written)

    //==========================================================================
    struct DeviceFile
    {
        juce::String id;                                // made once, never changes
        juce::String name { "New Device" }, description;
        juce::String vendor;                            // who made it (the developer)
        int version = 0;                                // counts the builds
        std::map<juce::String, juce::String> sources;   // file name -> its text
        juce::MemoryBlock wasm;                         // the last good build (empty: never built)
        juce::String builtFrom;                         // the sources' fingerprint when wasm was built
        juce::var chat;                                 // the builder's conversation
        juce::MemoryBlock licence;                      // license.soni, as it came in the file (empty: not protected)

        static constexpr int maxSources = 8;
        static constexpr int maxSourceSize = 200000;    // characters, each

        /** A new device: a fresh id. */
        static DeviceFile make (const juce::String& name);

        juce::Result read (const juce::File& file);
        juce::Result write (const juce::File& file) const;   // safely: a whole new file or none

        /** The license.soni a save writes: on its maker's computer, from its protection now (none
            while it's turned off); anywhere else, the one it came with. */
        juce::MemoryBlock licenceToWrite() const;

        /** Protected (it carries a license.soni), and someone else's: this computer doesn't hold
            its protection. It plays, but isn't opened for editing. */
        bool isLocked() const;

        /** What a file says it is, without reading its code, build or conversation (quick). */
        struct Info
        {
            juce::String id, name, description, builtFrom;
            int version = 0;
            bool built = false;
            bool locked = false;                        // protected, and someone else's (isLocked)
        };

        static juce::Result readInfo (const juce::File& file, Info& info);

        /** The sources' fingerprint (what was built from them, and whether they changed since). */
        juce::String fingerprint() const;
        bool isBuiltFromSources() const                 { return ! wasm.isEmpty() && builtFrom == fingerprint(); }

        /** A source file's name Sonify takes: letters, digits, _ and -, ending .cpp or .h. */
        static bool isValidSourceName (const juce::String& name);
    };

    //==========================================================================
    /** The devices' own data: <user data>\Devices (made when first needed), holding what isn't in
        their files: Protection and Reference code. Sonify sets where its user data is; until it
        does, Documents\Sonify\Devices. */
    juce::File myDevicesFolder();
    void setMyDevicesFolder (const juce::File& folder);

    /** Where the devices' files are: the Sonify plugins' folder, <user data>\Plugins (Sip::folder). */
    juce::File devicesFolder();

    /** Whether a file is a MIDI device: a .sip of kind midi, or an old .sonidev. */
    bool isDeviceFile (const juce::File& file);

    /** My Devices: the MIDI FX among the .sip files, by name. */
    juce::Array<juce::File> listMyDevices();

    /** A free file in the plugins' folder for a device of this name. */
    juce::File freeFileFor (const juce::String& name, const juce::File& except = {});

    /** Where a MIDI FX's protection (protection.json, Licences::loadProtection) is kept on its
        maker's computer: <user data>\Devices\Protection\<its id>. */
    juce::File protectionFolderOf (const juce::String& deviceId);

    /** Whether this computer is the MIDI FX's maker's: it holds its protection. */
    bool isMakerOf (const juce::String& deviceId);

    //==========================================================================
    struct Compiler
    {
        /** The compiler's folder: SONIFY_DEVICE_COMPILER when set, else "Device Compiler" next to
            Sonify. Nonexistent when it isn't there. */
        static juce::File folder();

        /** The C++ library's headers in the compiler at folder (share/wasi-sysroot/include/
            wasm32-wasip1/c++/v1), named to clang with -stdlib++-isystem: clang finds them on its
            own only through an empty include/c++/v1, which the installer leaves out. */
        static juce::File libcxxHeaders (const juce::File& folder);

        static bool isAvailable (juce::String* why = nullptr);

        struct Result
        {
            bool ok = false;
            juce::String log;                // the compiler's messages, its file paths left out
            juce::MemoryBlock wasm;
            double seconds = 0.0;
        };

        /** Compiles a device's sources (with sonify_device.h) into WebAssembly. Takes a few
            seconds: call it on a worker thread. */
        static Result compile (const std::map<juce::String, juce::String>& sources, std::atomic<bool>* cancelled = nullptr);

        /** The API devices are written against (sonify_device.h, as built into Sonify). */
        static juce::String apiHeader();
    };
}
