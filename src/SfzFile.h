// D:\workspace\Fanan_web_hub\src\SfzFile.h

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <utility>
#include <vector>

//==============================================================================
/**
    SFZ instruments as Sonify writes them (the Sampling Studio and its Builder): Sampler Arena's
    own dialect, byte for byte, so either app (and any SFZ player) plays the other's.

    The file: UTF-8 without a byte order mark (plain ASCII when the name is), Windows line ends
    (CR LF). Three comment lines (who wrote it, the instrument's name, how many keys were
    sampled), a blank line, an empty <global> and a blank line; then a <region> for each sample,
    its header alone on its line, one opcode per line (sample=, lokey=, hikey=, pitch_keycenter=,
    then lovel= and hivel= together on one line, then amp_veltrack=100) and a blank line after
    it. No <control>, no default_path, no key=: the samples are in a samples folder beside it,
    named with a backslash (sample=samples\Piano_C4_V124.wav). Round robins (takes of the same
    note played in turn) add seq_length= and seq_position=, a line each, after amp_veltrack.

    A sample with a loop (Create Legato Loop, or the Loop Editor) adds loop_mode=loop_sustain,
    loop_start=, loop_end= and loop_crossfade= (when it has one, in seconds), a line each, after
    amp_veltrack, as Sampler Arena does.

    The samples: 16 or 24-bit PCM (or 32-bit float) WAV with the plain 16-byte fmt chunk (never
    the extensible one) and no JUNK chunk, then a smpl chunk naming the key it was sampled at (its
    unity note) and its loop, when it has one.

    The Sample Builder's instruments are written as Sampler Arena's Sample Builder writes them:
    a comment, a <group> with a short attack and release, then a region for each sample with its
    keys on one line (builderText).

    Names: a key as Sampler Arena writes it in file names, "C4", "Cs4" (middle C, note 60, is
    C4); a sample as <name>_<key>_V<velocity>.wav (_RR<take> after it for a round robin's takes),
    the name with spaces, the characters Windows forbids in a name and control characters made
    underscores.
*/
namespace Sfz
{
    /** A key's name as Sampler Arena writes it in file names: "C4", "Cs4", "C-1". */
    juce::String keyName (int note);

    /** A name made fit for folders, files and the SFZ: spaces, the characters Windows forbids
        (: / \ * ? " < > |) and control characters become underscores, and so does a dot at its
        end. Never empty ("Sample"). */
    juce::String safeName (const juce::String& name);

    /** The keys a sampled key plays: from 3 keys below it (no lower than the key above the one
        below it) up to the key before the next one's zone, so a sample stretches further up than
        down; the lowest from the bottom of the range and the highest to its top (fill: the whole
        keyboard, 0 to 127). Every key of the range plays one sample, never two. roots: sorted,
        each once. */
    struct Zone
    {
        int root = 60, low = 60, high = 60;
    };

    std::vector<Zone> zones (const std::vector<int>& roots, int rangeLow, int rangeHigh, bool fill);

    /** A key's velocity ranges, from the velocities its samples were played at (sorted, each
        once): each from just above the middle between it and the one below to the middle between
        it and the one above, the first from 0 and the last to 127 (one alone: 0 to 127). */
    std::vector<std::pair<int, int>> velocityRanges (const std::vector<int>& velocities);

    /** The velocity the Sampling Studio plays a layer at: a single layer at 127, otherwise 3
        below the top of the layer's share of 1..127 (layer 0 the softest). */
    int layerVelocity (int layer, int layers);

    /** One sample's place in the instrument. */
    struct Region
    {
        juce::String sample;                    // its file's name, in the samples folder
        int lokey = 0, hikey = 127, keycenter = 60;
        int lovel = 0, hivel = 127;
        int seqLength = 0, seqPosition = 0;     // round robins: how many take turns, and which this is (0: none)
        juce::int64 loopStart = -1, loopEnd = -1;   // its loop (sample frames), none when loopEnd <= loopStart
        double loopCrossfade = 0.0;             // seconds (0: none written)
    };

    /** The instrument's text (lines end in a line feed; write() makes them CR LF). keys: how many
        keys were sampled. */
    juce::String text (const juce::String& name, int keys, const std::vector<Region>& regions);

    /** A Sample Builder instrument's text, as Sampler Arena's builder writes it. samplesFolder:
        "samples\\" when the samples are in a samples folder beside it, else empty. */
    juce::String builderText (const juce::String& name, const juce::String& samplesFolder, const std::vector<Region>& regions);

    /** A region's loop opcodes (empty without a loop). The crossfade in seconds, as the SFZ
        standard has it (Sampler Arena wrote samples, which other players read as seconds). */
    juce::String loopOpcodes (juce::int64 loopStart, juce::int64 loopEnd, double crossfadeSeconds);

    /** A loop_crossfade value in seconds: as the standard has it, or a whole number of samples
        as Sampler Arena writes it (10 or more, without a point). */
    double readCrossfade (const juce::String& value, double sampleRate);

    /** A sample's .loop sidecar (beside it, Sampler Arena's: loop_start=, loop_end=, crossfade=,
        score=): written, read (false when there's none) and removed. */
    bool writeLoopFile (const juce::File& wav, juce::int64 loopStart, juce::int64 loopEnd, int crossfade, float score);
    bool readLoopFile (const juce::File& wav, juce::int64& loopStart, juce::int64& loopEnd, int& crossfade);
    void removeLoopFile (const juce::File& wav);

    /** A WAV's smpl chunk made again with this unity note and loop (none: loopEnd <= loopStart),
        and the file cleaned as Sampler Arena cleans it: no JUNK, bext or LIST chunk, the plain
        16-byte fmt chunk, one smpl chunk. The audio itself is copied as it is. */
    bool setWavLoop (const juce::File& wav, int rootNote, juce::int64 loopStart, juce::int64 loopEnd);

    /** Writes the text as UTF-8 (no byte order mark) with CR LF line ends. */
    bool write (const juce::File& file, const juce::String& text);

    /** A sample as Sonify writes them: 16 or 24-bit PCM or 32-bit float, one or two channels (the
        buffer's first two; channels 1 for one alone), its key (and its loop, when loopEnd >
        loopStart) in a smpl chunk; gain applied as it's written (PCM clipped at full scale).
        Written whole to a temporary file first, then put in place. */
    bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& audio, int numSamples, double sampleRate,
                   int rootNote, float gain = 1.0f, int bitDepth = 24, int channels = 0,
                   juce::int64 loopStart = -1, juce::int64 loopEnd = -1);
}
