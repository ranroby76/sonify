// D:\workspace\Fanan_web_hub\src\DeviceHost.h

#pragma once

#include <juce_core/juce_core.h>

#include "Licences.h"
#include "MidiFx.h"
#include "sonify_device.h"

#include <array>
#include <atomic>
#include <memory>
#include <vector>

//==============================================================================
/**
    Sonify devices running: each a program written against sonify_device.h (by Sonify AI in the
    Device Studio), compiled to WebAssembly, and run here in Wasmtime's sandbox. Whatever its code
    does, it can't reach Sonify's memory, files or the network; it can't use more than 64 MB; and
    a block that runs too long (an endless loop) is stopped: the device goes quiet, lets the
    notes through and says why, instead of freezing the audio.

    A Build is one compiled version of a device, shared by every place it plays: its WebAssembly,
    compiled once, and what it says it has (its settings and outputs). Songs keep the builds they
    use (by their hash), so a song plays on any computer, with or without the device's file.

    A DeviceFx is a build playing in a MIDI FX slot (or in Apply, or a test): an instance of its
    own (its own memory), fed each block's notes, time and settings. What it moves in Sonify
    (its outputs, and controls it names) comes out through takeMoves() on the message thread.
*/
namespace Devices
{
    /** Why a device stopped. */
    enum class Fault { none, tooSlow, crashed, outOfMemory, failedToStart };

    juce::String describeFault (Fault fault);   // "" for none

    struct OutputSpec
    {
        juce::String id, name;
    };

    class Module;

    /** One compiled version of a device. Message thread (shared; never changed once made). */
    struct Build
    {
        juce::String deviceId;           // the device it's a version of: the same for all its versions
        juce::String name, description;
        int version = 0;
        juce::String hash;               // its WebAssembly's (SHA-256, hex)
        juce::MemoryBlock wasm;
        MidiFx::TypeSpec spec;           // id "device", the name, description and settings
        std::vector<OutputSpec> outputs;
        std::shared_ptr<Module> module;
        juce::MemoryBlock licence;       // a protected one's license.soni (empty: not protected)
        stella::licence::Soni soni;      // ...as it reads: its product, maker and demo times

        /** Protected: it plays its demo until it's registered on this computer. */
        bool isProtected() const noexcept    { return soni.isValid(); }

        /** For the page and songs (no WebAssembly): { deviceId, name, description, version, hash,
            params: [{ id, name, min, max, value, unit, choices }], outputs: [{ id, name }],
            protection: { product, name, vendor, passSeconds, holdSeconds } (when it's protected) }. */
        juce::var describe() const;

        /** For songs: describe() plus the WebAssembly (base64), and a protected one's license.soni
            (base64: a song keeps its protection). */
        juce::var toVar() const;
    };

    /** Compiles a device's WebAssembly and asks it what it has. Null (and why) when it isn't a
        Sonify device, or it fails to start. licence: a protected one's license.soni (empty: not
        protected). Message thread; takes a moment (it compiles). */
    std::shared_ptr<Build> makeBuild (const juce::MemoryBlock& wasm, const juce::String& deviceId,
                                      const juce::String& name, const juce::String& description,
                                      int version, juce::String& error, const juce::MemoryBlock& licence = {});

    /** A build from a song (Build::toVar): the same build when it's known already. */
    std::shared_ptr<Build> buildFromVar (const juce::var& value, juce::String& error);

    /** The builds Sonify has this session, by device and hash (two devices can have the same
        WebAssembly: each keeps its own build). Without a device id, findBuild finds any build with
        that hash. Message thread. */
    void remember (const std::shared_ptr<Build>& build);
    std::shared_ptr<Build> findBuild (const juce::String& hash, const juce::String& deviceId = {});

    /** Lets go of every build (at exit, once every device is gone). */
    void forgetAll();

    /** A song's build that can't play here (Sonify can't run devices, or it's damaged), kept as
        the song had it, so saving the song again keeps it for a Sonify that can. */
    void keepUnplayable (const juce::var& buildVar);

    /** What a song should keep of a build: Build::toVar, or the copy kept as it came. The device's
        own first, then any with that hash (the same code). Void when neither is here. */
    juce::var songCopyOf (const juce::String& hash, const juce::String& deviceId = {});

    /** Whether devices can run in this Sonify (Wasmtime started); why not when they can't. */
    bool isAvailable (juce::String* why = nullptr);

    /** The audio device's rate, for devices made from now on. */
    void setSampleRate (double sampleRate);
    double currentSampleRate();

    /** Hooked into MidiFx::create for the "device" kind: settings as getMidiFx describes it,
        { device: { hash, ... }, params, maps }. Unknown build: a stand-in that lets the notes
        through and keeps its settings, so the song keeps it. */
    std::unique_ptr<MidiFx::Device> makeFx (const juce::String& typeId, const juce::var& settings);

    //==========================================================================
    /** A move of one of Sonify's controls by a device. */
    struct Move
    {
        juce::int64 instance = 0;        // the DeviceFx (its serial)
        sonify::abi::ControlKind kind = sonify::abi::controlTarget;
        int output = 0;                  // controlOutput: which of its outputs
        float value = 0.0f;              // 0 .. 1
        char target[sonify::abi::targetSize] {};   // controlTarget: what to move (MIDI Link's key)
    };

    /** What devices moved since the last call, the last move of each control only. Message thread. */
    void takeMoves (std::vector<Move>& into);

    //==========================================================================
    class Instance;

    class DeviceFx;

    /** Keeps the build alive for the spec MidiFx::Device holds on to (it's made first). */
    struct BuildHolder
    {
        BuildHolder (std::shared_ptr<const Build> b, const juce::var& missingSettings);

        const MidiFx::TypeSpec& holderSpec() const noexcept   { return build != nullptr ? build->spec : missingSpec; }

        std::shared_ptr<const Build> build;
        MidiFx::TypeSpec missingSpec;    // a build that isn't here: its settings as the song had them
    };

    class DeviceFx final : private BuildHolder,
                           public MidiFx::Device
    {
    public:
        /** A build playing. An instance that can't start leaves the device quiet (letting the
            notes through) with its fault set. */
        DeviceFx (std::shared_ptr<const Build> build, double sampleRate = 48000.0);

        /** A stand-in for a build that isn't here: lets the notes through and keeps what it had. */
        DeviceFx (const juce::var& missingSettings);

        ~DeviceFx() override;

        const std::shared_ptr<const Build>& getBuild() const noexcept   { return build; }
        bool isMissing() const noexcept                                 { return build == nullptr; }
        juce::int64 getSerial() const noexcept                          { return serial; }
        Fault getFault() const noexcept                                 { return static_cast<Fault> (fault.load()); }

        /** Why it couldn't start (when it couldn't). */
        const juce::String& getStartError() const noexcept              { return startError; }

        /** Microseconds a block takes, on average lately (0 until it has played). */
        double getAverageMicroseconds() const noexcept                  { return averageMicros.load(); }

        /** A protected MIDI FX that isn't registered on this computer: notes pass for a while, then
            are held back for a while (its license.soni's times; the notes sounding end as a hold
            starts, so nothing hangs). */
        bool isInDemo() const noexcept    { return licensed != nullptr && ! licensed->load (std::memory_order_relaxed); }

        void process (const MidiFx::EventList& in, MidiFx::EventList& out, const MidiFx::Time& time) noexcept override;
        bool isBusy() const noexcept override;
        void describeInto (juce::DynamicObject& item) const override;

        //======================================================================
        // Message thread.

        /** Its outputs' maps: for each output id, what it moves (MIDI Link's Target, as data). */
        juce::var maps = juce::var (new juce::DynamicObject());
        juce::var missingSettings;       // a stand-in: what the song said

        /** What it logged since the last call (Device Studio shows the device it's trying). */
        juce::String takeLog();

        /** Whether its moves of Sonify's controls go out (offline runs never send them: they keep them). */
        std::atomic<bool> controlsLive { true };

        /** The moves it made in offline runs (the Device Studio's tests): kept, not sent. */
        std::vector<Move> offlineMoves;

    protected:
        void reset() noexcept override;

    private:
        void passThrough (const MidiFx::EventList& in, MidiFx::EventList& out) noexcept;
        void stopWith (Fault why) noexcept;
        void play (const MidiFx::EventList& in, MidiFx::EventList& out, const MidiFx::Time& time) noexcept;
        void applyDemo (MidiFx::EventList& out, int firstNew, const MidiFx::Time& time) noexcept;

        friend void takeMoves (std::vector<Move>&);

        std::unique_ptr<Instance> instance;
        juce::String startError;
        const juce::int64 serial;
        juce::AbstractFifo movesFifo { 256 };
        std::array<Move, 256> moves {};
        std::atomic<int> fault { 0 };
        std::atomic<double> averageMicros { 0.0 };
        bool resetPending = true;

        // The demo (audio thread): its registration's flag (null: not protected), its cycle
        // (counted in the song's own time), the notes it let out, and whether it holds them now.
        std::shared_ptr<std::atomic<bool>> licensed;
        stella::licence::DemoGate demoGate;
        double demoCarry = 0.0;          // the part of a sample the cycle hasn't counted yet
        MidiFx::Voices demoNotes;
        bool demoGap = false;

        juce::AbstractFifo logFifo { 8192 };
        std::vector<char> logBuffer = std::vector<char> (8192);

        JUCE_DECLARE_NON_COPYABLE (DeviceFx)
    };

    /** The DeviceFx with this serial, while it lives. Message thread. */
    DeviceFx* findFx (juce::int64 serial);
}
