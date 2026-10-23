// D:\workspace\Fanan_web_hub\src\Licences.h

#pragma once

#include <juce_core/juce_core.h>

#include "stella_licence.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>

//==============================================================================
/**
    Sonify's side of the protection of what's made in it (stella_licence.h holds the shared
    core: the formula, license.soni, the key files, the demo).

    Every protected product (a plugin made in the Plugins Creator Studio, a MIDI FX, a blob
    library) has its own serial formula, made once in the Protection tab and kept with its
    project (protection.json). It travels in license.soni, next to the plugin or inside it.
    The buyer gives the creator this computer's ID (computerId); the creator's own site turns
    it into a serial with the formula; the buyer types or pastes it into the Registration
    window (showRegistration), and it's kept in the product's key file in the user's
    application data, the same file every exported copy of the product reads. Fanan takes no
    part in it.

    Not registered: the demo. A plugin plays its sound for a while, then goes silent for a
    while; a MIDI FX lets notes through for a while, then holds them back for a while. The
    creator picks both times.

    Message thread, except the flags (flagFor), which the audio thread reads.
*/
namespace Licences
{
    //==========================================================================
    /** This computer's ID, as people see it: 9 digits, like 482-039-175 ("" when it can't be
        read). The same in Sonify and in every exported plugin on this computer. */
    juce::String computerId();

    /** A new serial formula (its three numbers A, B and M), from good randomness. */
    stella::licence::Formula newFormula();

    /** A new product id: "sp-" and 16 letters and digits, from good randomness. */
    juce::String newProductId();

    //==========================================================================
    /** license.soni, read from a file or from its bytes (false: it isn't one). */
    bool readSoni (const juce::File& file, stella::licence::Soni& soni);
    bool readSoni (const void* data, size_t size, stella::licence::Soni& soni);

    /** license.soni written into a file, or as bytes (padded with fresh noise each time). */
    bool writeSoni (const juce::File& file, const stella::licence::Soni& soni);
    juce::MemoryBlock soniData (const stella::licence::Soni& soni);

    //==========================================================================
    /** Whether this computer is registered for the product (its key file holds a serial that
        fits this computer's ID through the formula). Also sets the product's flag. */
    bool isRegistered (const stella::licence::Soni& soni);

    /** Registers this computer with a serial (typed or pasted: 10 digits, dashes and spaces don't matter):
        "" when it's done (the key file is written and the flag turns true), else why not. */
    juce::String registerSerial (const stella::licence::Soni& soni, const juce::String& serial);

    /** The flag a protected product plays by: true while this computer is registered for it.
        Shared, and read on the audio thread. It knows what isRegistered or registerSerial last
        found for the product, so check the product with isRegistered first; it turns true the
        moment it's registered. */
    std::shared_ptr<std::atomic<bool>> flagFor (const juce::String& productId);

    /** What a protected plugin's WebAssembly expects in its shared block while it's registered
        (stella::licence::tokenFor). */
    std::uint32_t runtimeToken (const juce::String& productId);

    /** The Registration window for a product: its name and whether it's registered, this
        computer's ID with Copy ID, a serial field (typing and Paste), Register, and what came
        of it. A serial that isn't this product's is tried on the protected blob libraries in
        use too. afterRegistering runs on the message thread once something got registered.
        isMidiFx: the demo is described as a MIDI FX's (notes held back, not silence). */
    void showRegistration (const stella::licence::Soni& soni, std::function<void()> afterRegistering, bool isMidiFx = false);

    //==========================================================================
    /** A Creator project's protection, as the Protection tab keeps it: protection.json in the
        project's folder (a MIDI FX's: Devices::protectionFolderOf). */
    struct ProjectProtection
    {
        bool on = false;
        juce::String product;                                  // "sp-..." (made once, never changes)
        stella::licence::Formula formula;                      // A, B and M: made once, when it's first turned on
        double playSeconds = 10.0, silentSeconds = 2.0;        // sound demo
        double passSeconds = 10.0, holdSeconds = 3.0;          // MIDI FX demo

        bool isValid() const;                                  // on, with a product and a good formula
    };

    constexpr const char* protectionFileName = "protection.json";

    /** <folder>/protection.json (missing or unreadable: off, with nothing made yet). */
    ProjectProtection loadProtection (const juce::File& projectFolder);
    bool saveProtection (const juce::File& projectFolder, const ProjectProtection& protection);

    /** Whether a folder holds a protection.json at all (its maker's computer). */
    bool hasProtection (const juce::File& projectFolder);

    /** What license.soni holds for a project's product. */
    stella::licence::Soni soniFor (const ProjectProtection& protection, const juce::String& name, const juce::String& vendor);
}
