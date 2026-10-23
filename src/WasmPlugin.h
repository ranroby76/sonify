// D:\workspace\Fanan_web_hub\src\WasmPlugin.h

#pragma once

#include "stella_blob.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct wasm_engine_t;

//==============================================================================
/**
    One plugin being built, running live in a sandbox (WebAssembly, run by Wasmtime).

    Whatever the AI-written code does, it can't reach the studio's memory, files or
    network; it can't use more than 512 MB; and a block that runs too long (an endless
    loop, say) is stopped and the plugin goes silent instead of freezing the audio.

    Talks to the Stella runtime compiled into the plugin (stella_runtime.cpp): audio in
    and out, MIDI events and parameter values pass through one shared block of memory.

    A sampler reads its sample library (blobs) through Sonify: the plugin's stella_blob_*
    calls (stella_api.h) reach the library set here, which reads the samples from the disk. A
    protected blob library this computer isn't registered for plays its demo: the plugin's
    stella_blob_demo call says so (stella::blob::Library::demo).
*/
class WasmPlugin final
{
public:
    struct MidiEvent
    {
        std::uint32_t frame;
        std::uint8_t status, data1, data2;
    };

    /** Compiles and starts a plugin. Takes a while the first time: call it on a worker thread
        (the same bytes again only start, from what was compiled then). On failure returns
        nullptr and says why in error. */
    static std::unique_ptr<WasmPlugin> load (const std::vector<std::uint8_t>& wasm, double sampleRate,
                                             int maxFrames, std::string& error);
    ~WasmPlugin();

    /** The store's licence token for the next blocks (Licences::runtimeToken while this
        computer holds a licence for the plugin's product, else 0): a plugin the store sells
        plays its demo without its own token (stella_licence.h). Any thread. */
    void setLicenceToken (std::uint32_t token) noexcept;

    /** The plugin's sample library: its instruments by name, and the blob found for each (an
        empty path: missing). Message thread; any time, playing or not. */
    void setLibrary (const std::vector<stella::blob::Entry>& instruments);

    /** How an instrument of the library is (stella::blob's notLoaded, loading, ready,
        unavailable), and why it can't play. Any thread. */
    int getLibraryState (int instrument) const noexcept;
    std::string getLibraryProblem (int instrument) const;

    /** The host's tempo and transport where a block starts, for tempo sync. */
    struct Transport
    {
        double tempo = 120.0;        // beats (quarter notes) a minute
        double beat = 0.0;           // where the block starts, in beats since the song's start
        double barStart = 0.0;       // where its bar began, in beats
        bool playing = false;        // the song plays (its position moves)
        int numerator = 4, denominator = 4;
    };

    /** Audio thread, before process: the next block's transport. A plugin built before tempo
        sync existed isn't told; a long block split up goes on counting the beats. */
    void setTransport (const Transport&) noexcept;

    /** Audio thread. Any of the pointers may be null (a missing channel). Long blocks are
        split up. Returns false once the plugin has failed: from then on it's silent. */
    bool process (const float* inL, const float* inR, float* outL, float* outR, int numFrames,
                  const MidiEvent* events, int numEvents) noexcept;

    int getNumParameters() const noexcept;
    void setParameter (int index, float value) noexcept;   // any thread: used from the next block
    float getParameter (int index) const noexcept;

    /** Modules are numbered as in graph.json. A bypassed module passes its first input
        straight to its first output. Any thread; used from the next block. */
    void setBypass (int moduleIndex, bool bypassed) noexcept;
    bool isBypassed (int moduleIndex) const noexcept;

    //==============================================================================
    // Watching signals. A signal is a module's output port (moduleIndex >= 0), the plugin's
    // input (pluginInput, port 0 or 1) or its output (pluginOutput, port 0 or 1).
    static constexpr int pluginInput = -1, pluginOutput = -2, off = -100;
    static constexpr int maxTaps = 32, maxScopes = 4;

    /** The signals the GUI's meters watch (up to 32), as (moduleIndex, port). Any thread. */
    void setTaps (const std::vector<std::pair<int, int>>& taps) noexcept;

    /** A tap's highest level since the last call (0..1+), and its RMS in the latest block. */
    float takeTapPeak (int tap) noexcept;
    float getTapRms (int tap) const noexcept;

    /** Scope slots 0..3 each copy one signal (slot 0 is the schematic's probe). */
    void setScope (int slot, int moduleIndex, int port) noexcept;

    /** The newest numSamples of a scope slot, oldest first (zeros before it started).
        Any thread; for a scope it doesn't matter if a block lands mid-read. */
    void readScope (int slot, float* destination, int numSamples) const noexcept;

    /** The values modules show (stella::Module::display), numbered as in the description. */
    int getNumDisplays() const noexcept;
    float getDisplay (int index) const noexcept;

    /** JSON from the plugin: its modules (ports) and parameters (ids, names, ranges). */
    const std::string& getDescription() const noexcept;

    bool hasFailed() const noexcept;
    std::string getFailure() const;   // why it stopped, once hasFailed()

    int getMaxFrames() const noexcept;
    double getSampleRate() const noexcept;

    /** The studio's one Wasmtime engine, with the clock that stops a stuck call (each call
        gets a deadline in its ticks, 10 ms apart): the GUI's programmed elements use it too. */
    static wasm_engine_t* engine();

private:
    WasmPlugin();

    struct Impl;
    std::unique_ptr<Impl> impl;
};
