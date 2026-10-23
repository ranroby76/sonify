// D:\workspace\Fanan_web_hub\src\BlobMaker.h

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

//==============================================================================
/**
    The Sampling Studio's SFZ to Blob: an SFZ and its samples packed into one blob (stella_blob.h),
    the file a sampler made in the Plugins Creator Studio plays from the disk (its sample library,
    PluginLibrary.h). Several at once: a list, made one after another on a thread of its own.

    The SFZ is read as SFZ players read it: <control> (default_path, note_offset, octave_offset),
    <global>, <master>, <group> and <region>, each region with what's above it; #include and
    #define; comments. Every region's opcodes go into the blob (keys as MIDI note numbers), and
    its sample once however many regions play it: WAV, AIFF, FLAC, Ogg or MP3, kept as it is (16
    or 24-bit, float) or made 16-bit, 24-bit or float. A WAV's own loop (its smpl chunk) is the
    region's loop when the SFZ gives none (loop_continuous, as the standard has it). A region whose
    sample is missing is left out, and said.

    The blob goes beside its SFZ (named after it), or into one folder; it's written whole under
    another name first, so a blob that's there is always complete.

    Protection: blobs are made open (every sampler plays them), or protected with a product's
    protection (stella_licence.h): a Plugins Creator Studio project's (its protection.json) or a
    license.soni file's. A protected blob is locked: its head names the product, and its key comes
    from the product's serial formula (stella_blob.h), so it opens only with that product's
    license.soni beside it, which is written into the blob's folder with it (one folder, one
    product: a folder with another product's license.soni is left alone, and said). A plugin
    playing it plays its demo until the computer is registered for the product.

    Threads: the list and its choices on the message thread; convert() on any.
*/
class BlobMaker final
{
public:
    enum class Samples { asTheyAre, s16, s24, f32 };
    enum class Protection { none, project, soniFile };

    struct Item
    {
        enum class State { waiting, working, done, failed };

        int id = 0;
        juce::File sfz, blob;
        juce::String name;
        State state = State::waiting;
        double progress = 0.0;
        juce::String message;            // failed: why; done: what was left out, when anything was
        int regions = 0, samples = 0;
        juce::int64 bytes = 0;
        bool locked = false;             // made with the protection
    };

    /** keptFile: its choices, kept for next time. */
    explicit BlobMaker (const juce::File& keptFile);
    ~BlobMaker();

    //==========================================================================
    // Message thread

    /** SFZs, or folders (every SFZ in them and their folders). One already listed waits again. */
    void add (const juce::Array<juce::File>& files);
    void remove (int id);
    void clear();                        // every one but the one being made

    /** Where blobs go: beside each SFZ, or into the folder. */
    void setBeside (bool besideEachSfz);
    void setFolder (const juce::File& folder);
    void setSamples (Samples samples);

    /** The protection the blobs are made with: none, a Creator project's (source: its folder or
        its project.stella), or a license.soni file's. "" when it's set; else why not (it stays
        as it was). */
    juce::String setProtection (Protection kind, const juce::File& source);

    Protection getProtection() const noexcept    { return protection; }
    juce::File getProtectionSource() const       { return protectionSource; }

    /** The waiting ones, one after another (the protection read again first: a project's may have
        changed). "" when it starts, else why not. */
    juce::String start();

    /** The one being made stops (nothing of it is left), and the rest wait. */
    void stop();

    bool isRunning() const noexcept      { return running.load(); }

    /** The list and the choices, for the page. */
    juce::var toVar() const;

    /** Called on the message thread when the list or its progress changed. */
    std::function<void()> onChange;

    //==========================================================================
    // Any thread
    struct Result
    {
        bool ok = false;
        juce::String name, problem;
        juce::StringArray notes;         // what was left out
        int regions = 0, samples = 0;
        juce::int64 bytes = 0;
        bool locked = false;
    };

    /** One SFZ into one blob. progress (0..1): false calls it off. soni: the bytes of the
        license.soni of the product the blob is locked to (written beside it too); empty: an open
        blob, as before. */
    static Result convert (const juce::File& sfz, const juce::File& blob, Samples samples,
                           const std::function<bool (double)>& progress,
                           const std::vector<std::uint8_t>& soni = {});

    /** The instrument's name, as the SFZ says it ("// Family:"), else its file's. */
    static juce::String nameOf (const juce::File& sfz);

    /** An SFZ's text as convert() reads it: every #include put in its place (paths from the SFZ's
        folder; one that isn't there, in notes) and its comments gone (line ends kept). The Sonify
        Sampler reads SFZs with it too, so an instrument maps the same in both. */
    static juce::String sfzText (const juce::File& sfz, juce::StringArray& notes);

private:
    class Worker;

    void changed();
    void keep() const;
    juce::File blobFor (const juce::File& sfz) const;
    juce::String readProtection();       // the chosen protection's license.soni, again: "" or why not

    juce::File keptFile, folder;
    bool beside = true;
    Samples samples = Samples::asTheyAre;

    // The protection (under the lock: the worker takes soni).
    Protection protection = Protection::none;
    juce::File protectionSource;
    std::vector<std::uint8_t> soni;      // its license.soni's bytes (empty: none, or it can't be read)
    juce::String protectionProduct, protectionName, protectionVendor, protectionProblem;

    mutable juce::CriticalSection lock;  // the list, between the message thread and the worker
    std::vector<Item> items;
    int nextId = 1;
    std::atomic<bool> running { false }, stopping { false };
    std::unique_ptr<Worker> worker;

    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    std::atomic<bool> changePending { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlobMaker)
};
