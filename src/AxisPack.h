// D:\workspace\Fanan_web_hub\src\AxisPack.h

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

//==============================================================================
/**
    AXIS's amp captures and cabinet IRs, packed into one blob.

    AXIS plays two kinds of thing nobody writes by hand: NAM captures (.nam, Neural Amp Modeler's
    own files: a neural network trained on a real amp) and cabinet IRs (.wav: a cabinet and its
    microphone, as a short recording). Shipping them loose means a folder of hundreds of files
    beside Sonify; so Sonify's own blob (stella_blob.h) holds them all in one file instead, the
    same file format the Sampling Studio's SFZ to Blob makes, with the same encryption, the same
    open or locked keys, and the same license.soni protection. write() makes them.

    Whatever is packed, a user can still drop a .nam or a .wav of their own anywhere and load it
    one by one: readNam() and readIr() read a loose file into the very same thing a pack gives
    back, so the rest of AXIS never knows the difference.

    What's in a pack
    ----------------
    The blob's samples hold the numbers: a capture's weights as one mono float "sample" (the
    model's own sample rate), an IR as a mono or stereo float sample. The blob's index holds
    everything else, in lines of its own (stella::blob::Index::extra), which every other reader of
    blobs passes over:

        kind <tab> axis <tab> 1
        amp  <tab> name=.. maker=.. make=.. model=.. tone=.. rate=.. loudness=.. gain=..
                   cab=0|1 esr=.. nam=.. subs=<n>
        sub  <tab> amp=<n> sample=<n> arch=WaveNet max=.. weights=<n> loudness=.. gain=..
                   config=<base64 of the submodel's config, as NAM's own JSON>
        cab  <tab> name=.. maker=.. make=.. model=.. mic=.. sample=<n>

    A capture's config travels as NAM wrote it, word for word, because NAM's architectures keep
    growing (0.7.0's WaveNet has per-layer kernels, gating, FiLM blocks and a bottleneck that
    earlier ones didn't). Only the weights are turned into floats, and they are all the weight
    there is: the config of a capture is a couple of kilobytes, its weights a quarter of a
    megabyte as text. So a packed capture loads by reading floats, with a small config parsed
    once and no JSON of any size anywhere.

    SlimmableContainer
    ------------------
    A NAM file whose architecture is "SlimmableContainer" isn't a model: it's two or more models of
    the same amp at different widths, cheapest first, and its own top-level weights array is empty.
    A reader that doesn't know this plays silence. readNam() unwraps it, so a Capture always holds
    its submodels plainly, widest last, however the file was written; a plain WaveNet or LSTM file
    simply has one. AXIS runs the widest (Capture::widest()); the hidden SLIM setting picks a
    narrower one where the capture has one to pick.

    gear_type
    ---------
    A capture whose gear_type says "amp_cab" was taken through a cabinet, so the cabinet is already
    in it and AXIS must leave its own IR stage out of the way (Capture::hasCab). It's in the pack,
    so AXIS knows without being told.

    Threads: readNam, readIr, write and Pack's loading on any one thread each; nothing here is
    for the audio thread.
*/
namespace AxisPack
{
    /** The index line that says a blob is a pack of these, and what reads it. */
    constexpr const char* indexKind = "axis";
    constexpr int indexVersion = 1;

    /** What NAM captures are trained at, and what AXIS runs them at. */
    constexpr double captureRate = 48000.0;

    /** The most of an IR that's kept when it's conditioned (seconds): a cabinet is over long
        before this, and what's past it is the room. */
    constexpr double irSeconds = 0.5;

    //==========================================================================
    /** One amp capture, from a .nam file or from a pack. */
    struct Capture
    {
        /** One model of the amp. A plain capture has one; a slimmable one has several of the same
            amp at different widths, cheapest first. */
        struct Submodel
        {
            juce::String architecture { "WaveNet" };   // NAM's own word: WaveNet, LSTM
            double maxValue = 1.0;                     // what SlimmableContainer picks it by (1.0: the widest)
            double loudness = 0.0, gain = 1.0;         // this width's own numbers, when it has them
            juce::String config;                       // its config, as NAM's JSON, word for word
            std::vector<float> weights;

            bool isValid() const noexcept    { return ! weights.empty() && config.isNotEmpty(); }
        };

        juce::String name, maker;                      // metadata name, modeled_by
        juce::String gearMake, gearModel;              // gear_make, gear_model
        juce::String gearType, toneType;               // gear_type (amp, amp_cab...), tone_type
        juce::String namVersion;                       // the file format's version ("0.7.0")
        double sampleRate = captureRate;
        double loudness = 0.0;                         // dB the capture plays at, for levelling one against another
        double gain = 1.0;
        double esr = -1.0;                             // how far it was from the real amp in training (-1: not said)
        bool hasCab = false;                           // gear_type says a cabinet is in it: no IR after it

        std::vector<Submodel> submodels;               // widest last

        /** The submodel AXIS plays by default: the widest there is (-1: none). */
        int widest() const noexcept;

        /** The one a slim value 0..1 asks for, as SlimmableContainer means it (its maxValue), or
            the widest when nothing narrower fits. -1: none. */
        int forSlim (double slim) const noexcept;

        bool isValid() const noexcept;

        /** What to call it in a list: its metadata name, else its make and model, else its file's. */
        juce::String displayName() const;
    };

    //==========================================================================
    /** One cabinet IR, from a .wav (or any file JUCE reads) or from a pack. */
    struct Ir
    {
        juce::String name, maker;
        juce::String gearMake, gearModel, mic;
        double sampleRate = captureRate;
        std::vector<float> left, right;                // right empty: mono

        int frames() const noexcept      { return (int) left.size(); }
        int channels() const noexcept    { return right.empty() ? 1 : 2; }
        bool isValid() const noexcept    { return ! left.empty() && (right.empty() || right.size() == left.size()); }
        juce::String displayName() const;
    };

    /** What's done to an IR as it's read: kept as the file has it, brought to 48 kHz and cut to
        irSeconds, or that and brought to one level with the others as well. */
    enum class Conditioning { asItIs, trimmed, levelled };

    //==========================================================================
    // Loose files, loaded one by one.

    /** A .nam read, its SlimmableContainer unwrapped. False: why not (error). */
    bool readNam (const juce::File& file, Capture& into, juce::String& error);

    /** The same from a .nam's text, for a file already in hand. */
    bool readNamText (const juce::String& json, const juce::String& fallbackName, Capture& into, juce::String& error);

    /** An IR read (WAV, AIFF, FLAC...), conditioned as asked. False: why not (error). */
    bool readIr (const juce::File& file, Conditioning conditioning, Ir& into, juce::String& error);

    /** The files AXIS loads, for file choosers. */
    juce::String namWildcards();
    juce::String irWildcards();
    bool isNamFile (const juce::File& file);
    bool isIrFile (const juce::File& file);

    //==========================================================================
    // Packs.

    /** What a pack holds, as its index says, without reading any of the numbers. */
    struct Entry
    {
        enum class Kind { amp, cab };

        Kind kind = Kind::amp;
        int index = 0;                       // its place among its own kind, as the pack lists it
        juce::String name, maker, gearMake, gearModel;
        juce::String toneType, mic;
        double sampleRate = captureRate;
        bool hasCab = false;                 // amps: the cabinet is in the capture
        int submodels = 0;                   // amps
        int frames = 0, channels = 1;        // cabs
    };

    /** A pack opened: its list straight away, its captures and IRs one at a time. A locked pack
        needs its product's license.soni, as every locked blob does (stella_blob.h). */
    class Pack final
    {
    public:
        Pack();
        ~Pack();

        /** False: why not (error). A blob that isn't a pack says so. */
        bool open (const juce::File& blob, juce::String& error);
        void close();

        bool isOpen() const noexcept;
        const juce::File& getFile() const noexcept;
        juce::String getName() const;            // what the pack is called
        bool isLocked() const noexcept;
        juce::String getProduct() const;         // a locked pack's product ("": open)

        const std::vector<Entry>& getEntries() const noexcept;
        int count (Entry::Kind kind) const noexcept;

        /** An amp capture by its place among the amps. False: why not (error). */
        bool loadCapture (int ampIndex, Capture& into, juce::String& error) const;

        /** A cabinet IR by its place among the cabs. False: why not (error). */
        bool loadIr (int cabIndex, Ir& into, juce::String& error) const;

        /** Whether a blob is an AXIS pack at all, without opening it whole (its head and index). */
        static bool looksLikePack (const juce::File& blob);

    private:
        class Impl;
        std::unique_ptr<Impl> impl;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Pack)
    };

    //==========================================================================
    /** Everything a pack is made of, in hand. */
    struct Contents
    {
        juce::String name { "AXIS" };            // what the pack is called (the blob's name)
        std::vector<Capture> captures;
        std::vector<Ir> irs;

        bool isEmpty() const noexcept    { return captures.empty() && irs.empty(); }
    };

    /** A pack written whole (under another name first: a pack that's there is always complete).
        soni: the bytes of the license.soni of the product it's locked to, which is written in its
        folder with it; empty for an open pack, which every copy of AXIS plays.
        progress (0..1): false calls it off. "" when it's written, else why not. */
    juce::String write (const Contents& contents, const juce::File& blob,
                        const std::function<bool (double)>& progress,
                        const std::vector<std::uint8_t>& soni = {},
                        juce::StringArray* notes = nullptr);
}
