// D:\workspace\Fanan_web_hub\src\AxisPacker.h

#pragma once

#include "AxisPack.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

//==============================================================================
/**
    The Sampling Studio's AXIS Pack: cabinet impulse responses (.wav, .aiff, .flac) packed into
    one blob (AxisPack.h), the file AXIS plays its IR cabinets from. (AXIS models its amps itself,
    so a pack is cabinets only; the pack's format can still carry NAM captures, unused.) The Sampling Studio's own
    SFZ to Blob beside it (BlobMaker.h) does the same for sampled instruments, and this works the
    same way: a list, each one read in turn, and the same protection.

    One list, one pack: unlike SFZ to Blob, which makes a blob of each SFZ, everything listed here
    goes into the same file (so there's one file beside Sonify instead of hundreds). A file that
    can't be read is left out of the pack and its row says why; the rest still pack.

    Protection: open packs, which every copy of AXIS plays, or locked to a product (a Plugins
    Creator Studio project's protection, or a license.soni file's), which only opens with that
    product's license.soni in the pack's folder. IRs someone lends you are worth locking:
    nobody can lift them back out of a locked pack.

    Threads: the list and its choices on the message thread; the reading and the writing on a
    thread of its own.
*/
class AxisPacker final
{
public:
    using Conditioning = AxisPack::Conditioning;

    enum class Protection { none, project, soniFile };

    struct Item
    {
        enum class State { waiting, working, done, failed };
        enum class Kind { amp, cab };

        int id = 0;
        juce::File file;
        Kind kind = Kind::amp;
        juce::String name;
        State state = State::waiting;
        double progress = 0.0;
        juce::String message;            // failed: why; done: what it turned out to be
    };

    /** How the pack's own writing is going. */
    struct Pack
    {
        enum class State { none, working, done, failed };

        State state = State::none;
        double progress = 0.0;
        juce::File file;
        juce::String message;
        int amps = 0, cabs = 0;
        juce::int64 bytes = 0;
        bool locked = false;
    };

    /** keptFile: its choices, kept for next time. */
    explicit AxisPacker (const juce::File& keptFile);
    ~AxisPacker();

    //==========================================================================
    // Message thread

    /** IR files, or folders (every IR in them and their folders). One already listed waits
        again. */
    void add (const juce::Array<juce::File>& files);
    void remove (int id);
    void clear();                        // every one but the one being read

    /** Where the pack is written. */
    void setPackFile (const juce::File& file);
    juce::File getPackFile() const;

    void setConditioning (Conditioning conditioning);

    /** The protection the pack is made with: none, a Creator project's (source: its folder or its
        project.stella), or a license.soni file's. "" when it's set; else why not (it stays as it
        was). */
    juce::String setProtection (Protection kind, const juce::File& source);

    Protection getProtection() const noexcept    { return protection; }
    juce::File getProtectionSource() const;

    /** Everything listed read, and then the pack written. "" when it starts, else why not. */
    juce::String start();

    /** The run stops (nothing of the pack is left behind). */
    void stop();

    bool isRunning() const noexcept      { return running.load(); }

    /** The list, the pack and the choices, for the page. */
    juce::var toVar() const;

    /** Called on the message thread when the list or its progress changed. */
    std::function<void()> onChange;

    //==========================================================================
    /** What a pack holds, for AXIS's own lists and for the page: "" when it could be read, else
        why not. Nothing to do with the list above. */
    static juce::String describe (const juce::File& blob, int& amps, int& cabs, juce::String& name, bool& locked);

private:
    class Worker;

    void changed();
    void keep() const;
    juce::String readProtection();       // the chosen protection's license.soni, again: "" or why not
    juce::File packFileOrDefault() const;

    juce::File keptFile, packFile;
    Conditioning conditioning = Conditioning::levelled;

    // The protection (under the lock: the worker takes soni).
    Protection protection = Protection::none;
    juce::File protectionSource;
    std::vector<std::uint8_t> soni;
    juce::String protectionProduct, protectionName, protectionVendor, protectionProblem;

    mutable juce::CriticalSection lock;  // the list and the pack, between the message thread and the worker
    std::vector<Item> items;
    Pack pack;
    int nextId = 1;
    std::atomic<bool> running { false }, stopping { false };
    std::unique_ptr<Worker> worker;

    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    std::atomic<bool> changePending { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AxisPacker)
};
