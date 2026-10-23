// D:\workspace\Fanan_web_hub\src\DeviceHost.cpp

#include "DeviceHost.h"
#include "DeviceFiles.h"

#include <juce_cryptography/juce_cryptography.h>

#include <wasmtime.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>

namespace Devices
{
namespace
{
    using IO = sonify::abi::IO;

    // The sandbox's limits. A block (about 10 ms of audio) that runs longer than this is stopped:
    // the device is stuck (an endless loop) or far too slow to play live.
    constexpr int tickMilliseconds = 2;
    constexpr std::uint64_t processDeadlineTicks = 40;      // about 80 ms
    constexpr std::uint64_t startDeadlineTicks = 1500;      // about 3 s to start
    constexpr std::int64_t memoryLimit = 64 * 1024 * 1024;
    constexpr size_t wasmStackLimit = 256 * 1024;
    constexpr size_t maxDescription = 8192;

    juce::String errorText (wasmtime_error_t* error)
    {
        if (error == nullptr)
            return {};

        wasm_name_t message;
        wasmtime_error_message (error, &message);
        const auto text = juce::String::fromUTF8 (message.data, static_cast<int> (message.size));
        wasm_byte_vec_delete (&message);
        wasmtime_error_delete (error);
        return text;
    }

    /** What a trap means for the device; the trap is deleted. */
    Fault faultOf (wasm_trap_t* trap, juce::String& text)
    {
        auto fault = Fault::crashed;

        if (trap == nullptr)
            return fault;

        wasmtime_trap_code_t code = 0;

        if (wasmtime_trap_code (trap, &code))
        {
            if (code == WASMTIME_TRAP_CODE_INTERRUPT)
                fault = Fault::tooSlow;
            else if (code == WASMTIME_TRAP_CODE_STACK_OVERFLOW)
                fault = Fault::outOfMemory;
        }

        wasm_message_t message;
        wasm_trap_message (trap, &message);
        text = juce::String::fromUTF8 (message.data, static_cast<int> (message.size)).trim();
        wasm_byte_vec_delete (&message);
        wasm_trap_delete (trap);
        return fault;
    }

    //==========================================================================
    // What a device's C library may ask of the system (WASI), answered here with nothing: no
    // files, no console, no environment, no clock that waits. The C library of wasi-sdk asks for
    // these; a device asking for anything else (a file opened, a socket) isn't built.
    // Each answers at once, never allocates, and never blocks: it may run on the audio thread.
    constexpr const char* wasiModule = "wasi_snapshot_preview1";
    constexpr std::int32_t wasiSuccess = 0, wasiBadFile = 8, wasiNotSupported = 52;

    /** The device's memory, as its call sees it (null: none, or out of range). */
    std::uint8_t* memoryAt (wasmtime_caller_t* caller, std::uint32_t address, std::uint32_t length)
    {
        wasmtime_extern_t item {};

        if (! wasmtime_caller_export_get (caller, "memory", 6, &item) || item.kind != WASMTIME_EXTERN_MEMORY)
            return nullptr;

        auto* context = wasmtime_caller_context (caller);
        auto* base = wasmtime_memory_data (context, &item.of.memory);
        const auto size = wasmtime_memory_data_size (context, &item.of.memory);

        if (base == nullptr || static_cast<std::uint64_t> (address) + length > size)
            return nullptr;

        return base + address;
    }

    bool writeU32 (wasmtime_caller_t* caller, std::uint32_t address, std::uint32_t value)
    {
        auto* at = memoryAt (caller, address, 4);

        if (at == nullptr)
            return false;

        std::memcpy (at, &value, 4);   // WebAssembly is little-endian, as the computers Sonify runs on
        return true;
    }

    bool writeU64 (wasmtime_caller_t* caller, std::uint32_t address, std::uint64_t value)
    {
        auto* at = memoryAt (caller, address, 8);

        if (at == nullptr)
            return false;

        std::memcpy (at, &value, 8);
        return true;
    }

    using RawArgs = wasmtime_val_raw_t*;
    std::uint32_t u32 (RawArgs a, size_t i)   { return static_cast<std::uint32_t> (a[i].i32); }

    wasm_trap_t* answer (RawArgs a, std::int32_t errnoValue)
    {
        a[0].i32 = errnoValue;
        return nullptr;
    }

    // (argc, argv buffer size) and (count, buffer size): none.
    wasm_trap_t* wasiSizesNone (void*, wasmtime_caller_t* caller, RawArgs a, size_t)
    {
        const auto ok = writeU32 (caller, u32 (a, 0), 0) && writeU32 (caller, u32 (a, 1), 0);
        return answer (a, ok ? wasiSuccess : 21 /* EFAULT */);
    }

    wasm_trap_t* wasiNothing (void*, wasmtime_caller_t*, RawArgs a, size_t)        { return answer (a, wasiSuccess); }
    wasm_trap_t* wasiBadFd (void*, wasmtime_caller_t*, RawArgs a, size_t)          { return answer (a, wasiBadFile); }
    wasm_trap_t* wasiNoSupport (void*, wasmtime_caller_t*, RawArgs a, size_t)      { return answer (a, wasiNotSupported); }

    // clock_time_get (id, precision, out): a steady clock, in nanoseconds.
    wasm_trap_t* wasiClockTime (void*, wasmtime_caller_t* caller, RawArgs a, size_t)
    {
        const auto nanos = static_cast<std::uint64_t> (juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks()) * 1.0e9);
        return answer (a, writeU64 (caller, u32 (a, 2), nanos) ? wasiSuccess : 21);
    }

    // clock_res_get (id, out): a microsecond.
    wasm_trap_t* wasiClockRes (void*, wasmtime_caller_t* caller, RawArgs a, size_t)
    {
        return answer (a, writeU64 (caller, u32 (a, 1), 1000) ? wasiSuccess : 21);
    }

    // random_get (buffer, length): plain pseudo-random bytes (devices' randomness, not secrets).
    wasm_trap_t* wasiRandom (void*, wasmtime_caller_t* caller, RawArgs a, size_t)
    {
        const auto length = juce::jmin (u32 (a, 1), static_cast<std::uint32_t> (1 << 20));
        auto* at = memoryAt (caller, u32 (a, 0), length);

        if (at == nullptr)
            return answer (a, 21);

        static std::atomic<std::uint64_t> seed { 0x9e3779b97f4a7c15ull };
        auto x = seed.fetch_add (0x9e3779b97f4a7c15ull) ^ static_cast<std::uint64_t> (juce::Time::getHighResolutionTicks());

        for (std::uint32_t i = 0; i < length; ++i)
        {
            x ^= x << 13; x ^= x >> 7; x ^= x << 17;
            at[i] = static_cast<std::uint8_t> (x >> 24);
        }

        return answer (a, wasiSuccess);
    }

    // fd_write (fd, iovs, count, written): the console goes nowhere; it's taken as written.
    wasm_trap_t* wasiWrite (void*, wasmtime_caller_t* caller, RawArgs a, size_t)
    {
        const auto fd = u32 (a, 0), iovs = u32 (a, 1), count = juce::jmin (u32 (a, 2), static_cast<std::uint32_t> (1024));

        if (fd != 1 && fd != 2)
            return answer (a, wasiBadFile);

        const auto* list = memoryAt (caller, iovs, count * 8);

        if (list == nullptr)
            return answer (a, 21);

        std::uint64_t total = 0;

        for (std::uint32_t i = 0; i < count; ++i)
        {
            std::uint32_t length = 0;
            std::memcpy (&length, list + i * 8 + 4, 4);
            total += length;
        }

        return answer (a, writeU32 (caller, u32 (a, 3), static_cast<std::uint32_t> (juce::jmin<std::uint64_t> (total, 0xffffffffu))) ? wasiSuccess : 21);
    }

    // proc_exit: abort() and exit() stop the device.
    wasm_trap_t* wasiExit (void*, wasmtime_caller_t*, RawArgs, size_t)
    {
        static const char message[] = "the device called exit";
        return wasmtime_trap_new (message, sizeof (message) - 1);
    }

    struct WasiStub
    {
        const char* name;
        const char* signature;     // parameters, then '>' and results: i for i32, I for i64
        wasmtime_func_unchecked_callback_t function;
    };

    const WasiStub wasiStubs[] = {
        { "args_get", "ii>i", wasiNothing },          { "args_sizes_get", "ii>i", wasiSizesNone },
        { "environ_get", "ii>i", wasiNothing },       { "environ_sizes_get", "ii>i", wasiSizesNone },
        { "clock_res_get", "ii>i", wasiClockRes },    { "clock_time_get", "iIi>i", wasiClockTime },
        { "fd_close", "i>i", wasiBadFd },             { "fd_fdstat_get", "ii>i", wasiBadFd },
        { "fd_fdstat_set_flags", "ii>i", wasiBadFd }, { "fd_prestat_get", "ii>i", wasiBadFd },
        { "fd_prestat_dir_name", "iii>i", wasiBadFd },{ "fd_read", "iiii>i", wasiBadFd },
        { "fd_seek", "iIii>i", wasiBadFd },           { "fd_write", "iiii>i", wasiWrite },
        { "fd_sync", "i>i", wasiBadFd },              { "fd_tell", "ii>i", wasiBadFd },
        { "proc_exit", "i>", wasiExit },              { "random_get", "ii>i", wasiRandom },
        { "sched_yield", ">i", wasiNothing },         { "poll_oneoff", "iiii>i", wasiNoSupport }
    };

    const WasiStub* findStub (const juce::String& name)
    {
        for (const auto& stub : wasiStubs)
            if (name == stub.name)
                return &stub;

        return nullptr;
    }

    wasm_functype_t* functionType (const char* signature)
    {
        std::vector<wasm_valtype_t*> params, results;
        auto* into = &params;

        for (const char* c = signature; *c != 0; ++c)
        {
            if (*c == '>')
                into = &results;
            else
                into->push_back (wasm_valtype_new (static_cast<wasm_valkind_t> (*c == 'I' ? WASM_I64 : WASM_I32)));
        }

        wasm_valtype_vec_t p, r;
        wasm_valtype_vec_new (&p, params.size(), params.data());
        wasm_valtype_vec_new (&r, results.size(), results.data());
        return wasm_functype_new (&p, &r);
    }

    //==========================================================================
    /** Wasmtime for devices: an engine with interruption, and the clock that interrupts. */
    class Engine
    {
    public:
        static Engine& get()
        {
            static Engine engine;
            return engine;
        }

        wasm_engine_t* engine = nullptr;
        wasmtime_linker_t* linker = nullptr;
        juce::String problem;

    private:
        Engine()
        {
            auto* config = wasm_config_new();

            if (config == nullptr)
            {
                problem = "The device engine couldn't start.";
                return;
            }

            wasmtime_config_epoch_interruption_set (config, true);
            wasmtime_config_max_wasm_stack_set (config, wasmStackLimit);
            wasmtime_config_cranelift_opt_level_set (config, WASMTIME_OPT_LEVEL_SPEED);
            wasmtime_config_wasm_threads_set (config, false);
            engine = wasm_engine_new_with_config (config);

            if (engine == nullptr)
            {
                problem = "The device engine couldn't start.";
                return;
            }

            linker = wasmtime_linker_new (engine);

            // Not WASI itself (its files, clocks that sleep and console aren't for devices): the
            // little the C library asks for, answered here.
            for (const auto& stub : wasiStubs)
            {
                auto* type = functionType (stub.signature);
                const auto error = errorText (wasmtime_linker_define_func_unchecked (linker, wasiModule, std::strlen (wasiModule),
                                                                                     stub.name, std::strlen (stub.name), type,
                                                                                     stub.function, nullptr, nullptr));
                wasm_functype_delete (type);

                if (error.isNotEmpty())
                {
                    problem = "The device engine couldn't start: " + error;
                    return;
                }
            }

            running.store (true);
            ticker = std::thread ([this]
            {
                while (running.load())
                {
                    std::this_thread::sleep_for (std::chrono::milliseconds (tickMilliseconds));
                    wasmtime_engine_increment_epoch (engine);
                }
            });
        }

        ~Engine()
        {
            running.store (false);

            if (ticker.joinable())
                ticker.join();

            // The engine and linker are left to the end of the process: a device still alive at
            // exit (a leak elsewhere) must not find its engine gone.
        }

        std::atomic<bool> running { false };
        std::thread ticker;
    };

    //==========================================================================
    // Made on first use, so it goes before JUCE's own bookkeeping at exit.
    struct Registry
    {
        std::mutex buildsLock;
        std::map<juce::String, std::weak_ptr<Build>> builds;        // by device and hash (registryKey)
        std::vector<std::shared_ptr<Build>> keptBuilds;              // remembered for the session

        std::map<juce::String, juce::var> unplayable;                // by device and hash: as a song had it

        std::mutex fxLock;
        std::map<juce::int64, DeviceFx*> liveFx;
    };

    Registry& registry()
    {
        static Registry r;
        return r;
    }

    /** A build's place in the registry: its device and its WebAssembly. Two devices can have the
        same WebAssembly (an older copy of one, or the same code): each keeps its own build. */
    juce::String registryKey (const juce::String& deviceId, const juce::String& hash)
    {
        return deviceId + "/" + hash;
    }

    std::atomic<juce::int64> nextSerial { 1 };
    std::atomic<double> sampleRateNow { 48000.0 };

    MidiFx::Unit unitFromName (const juce::String& name)
    {
        static const std::pair<const char*, MidiFx::Unit> units[] = {
            { "number", MidiFx::Unit::number }, { "percent", MidiFx::Unit::percent }, { "semitones", MidiFx::Unit::semitones },
            { "steps", MidiFx::Unit::steps }, { "pitch", MidiFx::Unit::pitch }, { "rate", MidiFx::Unit::rate },
            { "choice", MidiFx::Unit::choice }, { "toggle", MidiFx::Unit::toggle }, { "ms", MidiFx::Unit::ms },
            { "bars", MidiFx::Unit::bars }, { "decimal", MidiFx::Unit::decimal } };

        for (const auto& [n, unit] : units)
            if (name == n)
                return unit;

        return MidiFx::Unit::number;
    }

    juce::String unitName (MidiFx::Unit unit)
    {
        switch (unit)
        {
            case MidiFx::Unit::number:    return "number";
            case MidiFx::Unit::percent:   return "percent";
            case MidiFx::Unit::semitones: return "semitones";
            case MidiFx::Unit::steps:     return "steps";
            case MidiFx::Unit::pitch:     return "pitch";
            case MidiFx::Unit::rate:      return "rate";
            case MidiFx::Unit::choice:    return "choice";
            case MidiFx::Unit::toggle:    return "toggle";
            case MidiFx::Unit::ms:        return "ms";
            case MidiFx::Unit::bars:      return "bars";
            case MidiFx::Unit::decimal:   return "decimal";
            case MidiFx::Unit::track:     return "track";
        }

        return "number";
    }

    /** A param as a device or a song describes it; false when it makes no sense. */
    bool paramFromVar (const juce::var& item, MidiFx::ParamSpec& p)
    {
        p.id = item.getProperty ("id", {}).toString().substring (0, 64);
        p.name = item.getProperty ("name", {}).toString().substring (0, 64);
        p.unit = unitFromName (item.getProperty ("unit", "number").toString());

        const auto number = [&item] (const char* name, double fallback)
        {
            const auto value = static_cast<double> (item.getProperty (name, fallback));
            return std::isfinite (value) ? value : fallback;
        };

        p.min = static_cast<float> (number ("min", 0.0));
        p.max = static_cast<float> (number ("max", 1.0));
        p.defaultValue = static_cast<float> (number ("default", number ("value", p.min)));

        // Choices: "A|B|C" from a device, a list from a song.
        const auto choices = item.getProperty ("choices", {});

        if (const auto* list = choices.getArray())
            for (const auto& c : *list)
                p.choices.add (c.toString().substring (0, 64));
        else
            p.choices.addTokens (choices.toString(), "|", "");

        p.choices.removeEmptyStrings();

        if (p.unit == MidiFx::Unit::rate)
        {
            p.choices = MidiFx::rateNames();
            p.min = 0.0f;
            p.max = static_cast<float> (p.choices.size() - 1);
        }
        else if (p.unit == MidiFx::Unit::choice)
        {
            p.min = 0.0f;
            p.max = static_cast<float> (juce::jmax (0, p.choices.size() - 1));
        }
        else if (p.unit == MidiFx::Unit::toggle)
        {
            p.min = 0.0f;
            p.max = 1.0f;
        }

        if (p.id.isEmpty() || ! (p.max >= p.min))
            return false;

        if (p.name.isEmpty())
            p.name = p.id;

        p.defaultValue = juce::jlimit (p.min, p.max, p.defaultValue);
        return true;
    }

    juce::var paramToVar (const MidiFx::ParamSpec& p)
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("id", p.id);
        item->setProperty ("name", p.name);
        item->setProperty ("min", p.min);
        item->setProperty ("max", p.max);
        item->setProperty ("value", p.defaultValue);
        item->setProperty ("unit", unitName (p.unit));

        if (! p.choices.isEmpty())
        {
            juce::Array<juce::var> choices;

            for (const auto& c : p.choices)
                choices.add (c);

            item->setProperty ("choices", choices);
        }

        return juce::var (item.get());
    }

    /** The settings and outputs a device describes ({ abi, params, outputs, wanted... }), in the
        order the device has them (Sonify passes values and takes moves by that order). Returns
        what's wrong with them ("" when nothing): a device is only built when they're right. */
    juce::String readDescription (const juce::var& description, std::vector<MidiFx::ParamSpec>& params, std::vector<OutputSpec>& outputs)
    {
        juce::StringArray seen, problems;

        if (static_cast<int> (description.getProperty ("paramsWanted", 0)) > MidiFx::maxParams)
            problems.add ("it has more than " + juce::String (MidiFx::maxParams) + " settings");

        if (static_cast<int> (description.getProperty ("outputsWanted", 0)) > sonify::abi::maxOutputs)
            problems.add ("it has more than " + juce::String (sonify::abi::maxOutputs) + " outputs");

        if (const auto* list = description.getProperty ("params", {}).getArray())
        {
            for (const auto& item : *list)
            {
                MidiFx::ParamSpec p;
                const auto id = item.getProperty ("id", {}).toString();

                if (! paramFromVar (item, p))
                    problems.add ("its setting \"" + id + "\" has no id, or its lowest value is above its highest");
                else if (seen.contains (p.id))
                    problems.add ("two of its settings have the id \"" + p.id + "\"");

                // Kept in its place either way, so the values Sonify passes stay in step.
                seen.add (p.id);

                if (static_cast<int> (params.size()) < MidiFx::maxParams)
                    params.push_back (std::move (p));
            }
        }

        seen.clear();

        if (const auto* list = description.getProperty ("outputs", {}).getArray())
        {
            for (const auto& item : *list)
            {
                OutputSpec o { item.getProperty ("id", {}).toString().substring (0, 64), item.getProperty ("name", {}).toString().substring (0, 64) };

                if (o.id.isEmpty())
                    problems.add ("one of its outputs has no id");
                else if (seen.contains (o.id))
                    problems.add ("two of its outputs have the id \"" + o.id + "\"");

                seen.add (o.id);

                if (o.name.isEmpty())
                    o.name = o.id;

                if (static_cast<int> (outputs.size()) < sonify::abi::maxOutputs)
                    outputs.push_back (std::move (o));
            }
        }

        return problems.joinIntoString ("; ");
    }

    /** What a module asks of its host: nothing but the C library's few WASI calls answered here.
        Returns what it asks for that devices can't have ("" when nothing). */
    juce::String unsupportedImports (const wasmtime_module_t* module)
    {
        wasm_importtype_vec_t imports;
        wasmtime_module_imports (module, &imports);
        juce::StringArray wanted;

        for (size_t i = 0; i < imports.size; ++i)
        {
            const auto* from = wasm_importtype_module (imports.data[i]);
            const auto* name = wasm_importtype_name (imports.data[i]);
            const auto moduleName = juce::String::fromUTF8 (from->data, static_cast<int> (from->size));
            const auto itemName = juce::String::fromUTF8 (name->data, static_cast<int> (name->size));

            if (moduleName != wasiModule || findStub (itemName) == nullptr)
                wanted.addIfNotAlreadyThere (itemName.startsWith ("path_") || itemName.startsWith ("fd_") ? "files"
                                           : itemName.startsWith ("sock_") ? "the network"
                                           : itemName.containsIgnoreCase ("thread") ? "threads"
                                           : moduleName + "." + itemName);
        }

        wasm_importtype_vec_delete (&imports);
        return wanted.joinIntoString (", ");
    }

    juce::String hashOf (const juce::MemoryBlock& data)
    {
        return juce::SHA256 (data.getData(), data.getSize()).toHexString();
    }
}

//==============================================================================
juce::String describeFault (Fault fault)
{
    switch (fault)
    {
        case Fault::none:           return {};
        case Fault::tooSlow:        return "It stopped: it took too long (it may be stuck in a loop).";
        case Fault::crashed:        return "It stopped: it crashed.";
        case Fault::outOfMemory:    return "It stopped: it ran out of memory.";
        case Fault::failedToStart:  return "It couldn't start.";
    }

    return {};
}

bool isAvailable (juce::String* why)
{
    auto& engine = Engine::get();

    if (why != nullptr)
        *why = engine.problem;

    return engine.engine != nullptr && engine.linker != nullptr && engine.problem.isEmpty();
}

//==============================================================================
class Module
{
public:
    explicit Module (wasmtime_module_t* m) : module (m) {}
    ~Module()   { if (module != nullptr) wasmtime_module_delete (module); }

    wasmtime_module_t* const module;

    JUCE_DECLARE_NON_COPYABLE (Module)
};

//==============================================================================
/** One device's sandbox: a store of its own (its memory) and the module's exports in it. */
class Instance
{
public:
    static std::unique_ptr<Instance> start (const Build& build, double sampleRate, juce::String& error, Fault& fault,
                                            juce::String* description = nullptr)
    {
        fault = Fault::failedToStart;
        auto& engine = Engine::get();

        if (! isAvailable (&error) || build.module == nullptr)
        {
            if (error.isEmpty())
                error = "It isn't compiled.";

            return {};
        }

        std::unique_ptr<Instance> instance (new Instance());
        instance->store = wasmtime_store_new (engine.engine, nullptr, nullptr);

        if (instance->store == nullptr)
        {
            error = "Its sandbox couldn't be made.";
            return {};
        }

        instance->context = wasmtime_store_context (instance->store);
        wasmtime_store_limiter (instance->store, memoryLimit, 10000, 4, 4, 2);
        wasmtime_context_set_epoch_deadline (instance->context, startDeadlineTicks);

        wasm_trap_t* trap = nullptr;
        wasmtime_instance_t made {};

        if (const auto e = errorText (wasmtime_linker_instantiate (engine.linker, instance->context, build.module->module, &made, &trap)); e.isNotEmpty())
        {
            error = "It couldn't start: " + e;
            return {};
        }

        if (trap != nullptr)
        {
            juce::String text;
            faultOf (trap, text);
            error = "It crashed while starting: " + text;
            return {};
        }

        const auto exported = [&] (const char* name, wasmtime_extern_t& into)
        {
            return wasmtime_instance_export_get (instance->context, &made, name, std::strlen (name), &into);
        };

        wasmtime_extern_t item {};

        if (! exported ("memory", item) || item.kind != WASMTIME_EXTERN_MEMORY)
        {
            error = "It isn't a Sonify device (it has no memory).";
            return {};
        }

        instance->memory = item.of.memory;

        const auto function = [&] (const char* name, wasmtime_func_t& into)
        {
            wasmtime_extern_t f {};

            if (! exported (name, f) || f.kind != WASMTIME_EXTERN_FUNC)
                return false;

            into = f.of.func;
            return true;
        };

        wasmtime_func_t initialize {}, abiVersion {}, create {}, io {}, describe {};

        if (! function ("sonify_abi", abiVersion) || ! function ("sonify_create", create) || ! function ("sonify_io", io)
            || ! function ("sonify_describe", describe) || ! function ("sonify_process", instance->processFunction))
        {
            error = "It isn't a Sonify device: SONIFY_DEVICE (TheClass) is missing at the end of its code.";
            return {};
        }

        // sonify_process is called unchecked on the audio thread: it must take nothing and give nothing.
        {
            auto* type = wasmtime_func_type (instance->context, &instance->processFunction);
            const auto right = type != nullptr && wasm_functype_params (type)->size == 0 && wasm_functype_results (type)->size == 0;

            if (type != nullptr)
                wasm_functype_delete (type);

            if (! right)
            {
                error = "It isn't a Sonify device (its sonify_process is of another kind).";
                return {};
            }
        }

        // A reactor: its C++ starts (globals, constructors) before anything else.
        if (function ("_initialize", initialize) && ! instance->call (initialize, nullptr, 0, nullptr, 0, error, fault))
            return {};

        wasmtime_val_t result {};

        if (! instance->call (abiVersion, nullptr, 0, &result, 1, error, fault))
            return {};

        if (result.kind != WASMTIME_I32 || result.of.i32 != sonify::abi::version)
        {
            error = "It was made for another version of Sonify.";
            fault = Fault::failedToStart;
            return {};
        }

        wasmtime_val_t rate {};
        rate.kind = WASMTIME_F64;
        rate.of.f64 = sampleRate > 0.0 ? sampleRate : 48000.0;

        if (! instance->call (create, &rate, 1, &result, 1, error, fault))
            return {};

        if (! instance->call (io, nullptr, 0, &result, 1, error, fault))
            return {};

        if (result.kind != WASMTIME_I32)
        {
            error = "It isn't a Sonify device (its sonify_io is of another kind).";
            fault = Fault::failedToStart;
            return {};
        }

        instance->ioOffset = static_cast<std::uint32_t> (result.of.i32);

        if (instance->getIO() == nullptr)
        {
            error = "It isn't a Sonify device (its shared block is out of place).";
            fault = Fault::failedToStart;
            return {};
        }

        if (description != nullptr)
        {
            if (! instance->call (describe, nullptr, 0, &result, 1, error, fault))
                return {};

            if (result.kind != WASMTIME_I32)
            {
                error = "It isn't a Sonify device (its sonify_describe is of another kind).";
                fault = Fault::failedToStart;
                return {};
            }

            *description = instance->readText (static_cast<std::uint32_t> (result.of.i32), maxDescription);
        }

        fault = Fault::none;
        return instance;
    }

    ~Instance()
    {
        if (store != nullptr)
            wasmtime_store_delete (store);
    }

    /** The shared block (it moves when the device's memory grows: ask again after each call). */
    IO* getIO() const noexcept
    {
        auto* base = wasmtime_memory_data (context, &memory);
        const auto size = wasmtime_memory_data_size (context, &memory);

        if (base == nullptr || static_cast<size_t> (ioOffset) + sizeof (IO) > size || ioOffset % alignof (IO) != 0)
            return nullptr;

        return reinterpret_cast<IO*> (base + ioOffset);
    }

    size_t getMemorySize() const noexcept
    {
        return wasmtime_memory_data_size (context, &memory);
    }

    /** One block. Audio thread: nothing here allocates unless the device traps. */
    Fault process() noexcept
    {
        wasmtime_context_set_epoch_deadline (context, processDeadlineTicks);
        wasm_trap_t* trap = nullptr;
        wasmtime_val_raw_t none[1] {};
        auto* error = wasmtime_func_call_unchecked (context, &processFunction, none, 0, &trap);

        if (error == nullptr && trap == nullptr)
            return Fault::none;

        juce::String text;
        auto fault = Fault::crashed;

        if (trap != nullptr)
            fault = faultOf (trap, text);

        if (error != nullptr)
            text = errorText (error);   // exit() or abort() in its code

        // Out of memory shows as a crash (its allocation failed): told apart by how much it had.
        if (fault == Fault::crashed && static_cast<std::int64_t> (getMemorySize()) > memoryLimit - 2 * 1024 * 1024)
            fault = Fault::outOfMemory;

        return fault;
    }

private:
    Instance() = default;

    bool call (const wasmtime_func_t& f, const wasmtime_val_t* args, size_t numArgs, wasmtime_val_t* results, size_t numResults,
               juce::String& error, Fault& fault)
    {
        wasm_trap_t* trap = nullptr;
        auto* e = wasmtime_func_call (context, &f, args, numArgs, results, numResults, &trap);

        if (e != nullptr)
        {
            error = "It couldn't start: " + errorText (e);
            fault = Fault::failedToStart;
            return false;
        }

        if (trap != nullptr)
        {
            juce::String text;
            fault = faultOf (trap, text);
            error = fault == Fault::tooSlow ? juce::String ("It took too long to start (it may be stuck in a loop).")
                                            : "It crashed while starting: " + text;
            return false;
        }

        return true;
    }

    juce::String readText (std::uint32_t offset, size_t max) const
    {
        const auto* base = wasmtime_memory_data (context, &memory);
        const auto size = wasmtime_memory_data_size (context, &memory);

        if (base == nullptr || offset >= size)
            return {};

        const auto* text = reinterpret_cast<const char*> (base + offset);
        const auto room = juce::jmin (max, size - offset);
        size_t length = 0;

        while (length < room && text[length] != 0)
            ++length;

        if (! juce::CharPointer_UTF8::isValidString (text, static_cast<int> (length)))
            return {};

        return juce::String::fromUTF8 (text, static_cast<int> (length));
    }

    wasmtime_store_t* store = nullptr;
    wasmtime_context_t* context = nullptr;
    wasmtime_memory_t memory {};
    wasmtime_func_t processFunction {};
    std::uint32_t ioOffset = 0;

    JUCE_DECLARE_NON_COPYABLE (Instance)
};

//==============================================================================
juce::var Build::describe() const
{
    juce::DynamicObject::Ptr item (new juce::DynamicObject());
    item->setProperty ("deviceId", deviceId);
    item->setProperty ("name", name);
    item->setProperty ("description", description);
    item->setProperty ("version", version);
    item->setProperty ("hash", hash);

    juce::Array<juce::var> params, outs;

    for (const auto& p : spec.params)
        params.add (paramToVar (p));

    for (const auto& o : outputs)
    {
        juce::DynamicObject::Ptr out (new juce::DynamicObject());
        out->setProperty ("id", o.id);
        out->setProperty ("name", o.name);
        outs.add (juce::var (out.get()));
    }

    item->setProperty ("params", params);
    item->setProperty ("outputs", outs);

    if (isProtected())
    {
        juce::DynamicObject::Ptr protection (new juce::DynamicObject());
        protection->setProperty ("product", juce::String (soni.product));
        protection->setProperty ("name", juce::String (juce::CharPointer_UTF8 (soni.name.c_str())));
        protection->setProperty ("vendor", juce::String (juce::CharPointer_UTF8 (soni.vendor.c_str())));
        protection->setProperty ("passSeconds", (double) soni.passSeconds);
        protection->setProperty ("holdSeconds", (double) soni.holdSeconds);
        item->setProperty ("protection", juce::var (protection.get()));
    }

    return juce::var (item.get());
}

juce::var Build::toVar() const
{
    auto item = describe();
    item.getDynamicObject()->setProperty ("wasm", juce::Base64::toBase64 (wasm.getData(), wasm.getSize()));

    if (isProtected())
        item.getDynamicObject()->setProperty ("licence", juce::Base64::toBase64 (licence.getData(), licence.getSize()));

    return item;
}

std::shared_ptr<Build> makeBuild (const juce::MemoryBlock& wasm, const juce::String& deviceId, const juce::String& name,
                                  const juce::String& description, int version, juce::String& error, const juce::MemoryBlock& licence)
{
    if (! isAvailable (&error))
        return {};

    if (wasm.getSize() < 8 || std::memcmp (wasm.getData(), "\0asm", 4) != 0)
    {
        error = "It isn't WebAssembly.";
        return {};
    }

    // The same WebAssembly as one already here: that build, named as asked.
    const auto hash = hashOf (wasm);

    if (auto known = findBuild (hash, deviceId); known != nullptr && known->deviceId == deviceId && known->name == name
                                       && known->description == description && known->version == version
                                       && known->licence == licence)
        return known;

    auto build = std::make_shared<Build>();
    build->deviceId = deviceId;
    build->name = name.isNotEmpty() ? name : juce::String ("Device");
    build->description = description;
    build->version = version;
    build->hash = hash;
    build->wasm = wasm;

    // A protected one: its license.soni as it reads (one that doesn't read protects nothing).
    if (! licence.isEmpty() && Licences::readSoni (licence.getData(), licence.getSize(), build->soni))
        build->licence = licence;
    else
        build->soni = {};

    if (auto known = findBuild (hash); known != nullptr && known->module != nullptr)
    {
        build->module = known->module;   // compiled once
    }
    else
    {
        wasmtime_module_t* module = nullptr;
        const auto e = errorText (wasmtime_module_new (Engine::get().engine, static_cast<const std::uint8_t*> (wasm.getData()),
                                                       wasm.getSize(), &module));

        if (e.isNotEmpty() || module == nullptr)
        {
            error = "It isn't valid WebAssembly: " + e;
            return {};
        }

        build->module = std::make_shared<Module> (module);

        if (const auto wanted = unsupportedImports (module); wanted.isNotEmpty())
        {
            error = "It uses " + wanted + ", which devices can't have: no files, network, threads or waiting. Leave that out.";
            return {};
        }
    }

    // It starts once here, to say what settings and outputs it has (and that it starts at all).
    juce::String text;
    auto fault = Fault::none;

    if (Instance::start (*build, sampleRateNow.load(), error, fault, &text) == nullptr)
        return {};

    const auto parsed = juce::JSON::parse (text);

    if (! parsed.isObject())
    {
        error = "It didn't describe itself (its settings).";
        return {};
    }

    build->spec.id = "device";
    build->spec.name = build->name;
    build->spec.description = description;

    if (const auto problem = readDescription (parsed, build->spec.params, build->outputs); problem.isNotEmpty())
    {
        error = "Its settings aren't right: " + problem + ".";
        return {};
    }

    return build;
}

std::shared_ptr<Build> buildFromVar (const juce::var& value, juce::String& error)
{
    const auto hash = value.getProperty ("hash", {}).toString();
    const auto deviceId = value.getProperty ("deviceId", {}).toString().substring (0, 80);

    // This device's build, when it's here (another device's with the same WebAssembly isn't it).
    if (auto known = findBuild (hash, deviceId))
        return known;

    juce::MemoryBlock wasm;

    {
        juce::MemoryOutputStream decoded (wasm, false);

        if (! juce::Base64::convertFromBase64 (decoded, value.getProperty ("wasm", {}).toString()))
            wasm.reset();
    }

    if (wasm.isEmpty())
    {
        error = "The song doesn't hold this device.";
        return {};
    }

    if (hashOf (wasm) != hash)
    {
        error = "The song's copy of this device is damaged.";
        return {};
    }

    // A protected one's license.soni, as the song kept it.
    juce::MemoryBlock licence;

    if (const auto kept = value.getProperty ("licence", {}).toString(); kept.isNotEmpty())
    {
        juce::MemoryOutputStream decoded (licence, false);

        if (! juce::Base64::convertFromBase64 (decoded, kept))
            licence.reset();
    }

    auto build = makeBuild (wasm, deviceId,
                            value.getProperty ("name", {}).toString().substring (0, 80),
                            value.getProperty ("description", {}).toString().substring (0, 2000),
                            static_cast<int> (value.getProperty ("version", 0)), error, licence);

    if (build != nullptr)
        remember (build);

    return build;
}

void remember (const std::shared_ptr<Build>& build)
{
    if (build == nullptr)
        return;

    auto& r = registry();
    const std::lock_guard<std::mutex> lock (r.buildsLock);
    r.builds[registryKey (build->deviceId, build->hash)] = build;

    if (std::find (r.keptBuilds.begin(), r.keptBuilds.end(), build) == r.keptBuilds.end())
        r.keptBuilds.push_back (build);
}

std::shared_ptr<Build> findBuild (const juce::String& hash, const juce::String& deviceId)
{
    if (hash.isEmpty())
        return {};

    auto& r = registry();
    const std::lock_guard<std::mutex> lock (r.buildsLock);

    if (deviceId.isNotEmpty())
    {
        const auto found = r.builds.find (registryKey (deviceId, hash));
        return found != r.builds.end() ? found->second.lock() : nullptr;
    }

    // No device named: any build with this WebAssembly.
    for (const auto& [key, known] : r.builds)
        if (auto build = known.lock(); build != nullptr && build->hash == hash)
            return build;

    return nullptr;
}

void forgetAll()
{
    auto& r = registry();
    const std::lock_guard<std::mutex> lock (r.buildsLock);
    r.keptBuilds.clear();
    r.builds.clear();
    r.unplayable.clear();
}

void keepUnplayable (const juce::var& buildVar)
{
    const auto hash = buildVar.getProperty ("hash", {}).toString();

    if (hash.isEmpty() || ! buildVar.hasProperty ("wasm"))
        return;

    auto& r = registry();
    const std::lock_guard<std::mutex> lock (r.buildsLock);
    r.unplayable[registryKey (buildVar.getProperty ("deviceId", {}).toString(), hash)] = buildVar;
}

juce::var songCopyOf (const juce::String& hash, const juce::String& deviceId)
{
    // The device's own first; then any with this WebAssembly (the same code: a song that keeps
    // one copy of it plays both devices, each named as its MIDI FX has it).
    if (auto build = findBuild (hash, deviceId))
        return build->toVar();

    if (deviceId.isNotEmpty())
        if (auto build = findBuild (hash))
            return build->toVar();

    auto& r = registry();
    const std::lock_guard<std::mutex> lock (r.buildsLock);

    if (const auto found = r.unplayable.find (registryKey (deviceId, hash)); found != r.unplayable.end())
        return found->second;

    for (const auto& [key, kept] : r.unplayable)
        if (kept.getProperty ("hash", {}).toString() == hash)
            return kept;

    return {};
}

void setSampleRate (double sampleRate)
{
    if (sampleRate > 0.0)
        sampleRateNow.store (sampleRate);
}

double currentSampleRate()
{
    return sampleRateNow.load();
}

//==============================================================================
BuildHolder::BuildHolder (std::shared_ptr<const Build> b, const juce::var& missingSettings)
    : build (std::move (b))
{
    if (build != nullptr)
        return;

    // A stand-in keeps the device's settings as the song described them.
    const auto device = missingSettings.getProperty ("device", {});
    missingSpec.id = "device";
    missingSpec.name = device.getProperty ("name", "Device").toString().substring (0, 80);
    missingSpec.description = device.getProperty ("description", {}).toString().substring (0, 2000);
    std::vector<OutputSpec> outputs;
    readDescription (device, missingSpec.params, outputs);
}

//==============================================================================
DeviceFx::DeviceFx (std::shared_ptr<const Build> b, double sampleRate)
    : BuildHolder (std::move (b), {}),
      MidiFx::Device (BuildHolder::holderSpec()),
      serial (nextSerial.fetch_add (1))
{
    if (build != nullptr)
    {
        auto why = Fault::none;
        instance = Instance::start (*build, sampleRate, startError, why);

        if (instance == nullptr)
            fault.store (static_cast<int> (why == Fault::none ? Fault::failedToStart : why));

        // A protected MIDI FX plays its demo until it's registered on this computer.
        if (build->isProtected())
        {
            Licences::isRegistered (build->soni);   // its flag follows what's found now
            licensed = Licences::flagFor (juce::String (build->soni.product));
            demoGate.setTimes (build->soni.passSeconds, build->soni.holdSeconds);
        }
    }

    auto& r = registry();
    const std::lock_guard<std::mutex> lock (r.fxLock);
    r.liveFx[serial] = this;
}

DeviceFx::DeviceFx (const juce::var& settings)
    : BuildHolder (nullptr, settings),
      MidiFx::Device (BuildHolder::holderSpec()),
      missingSettings (settings),
      serial (nextSerial.fetch_add (1))
{
    auto& r = registry();
    const std::lock_guard<std::mutex> lock (r.fxLock);
    r.liveFx[serial] = this;
}

DeviceFx::~DeviceFx()
{
    {
        auto& r = registry();
        const std::lock_guard<std::mutex> lock (r.fxLock);
        r.liveFx.erase (serial);
    }

    instance.reset();
}

void DeviceFx::passThrough (const MidiFx::EventList& in, MidiFx::EventList& out) noexcept
{
    for (int i = 0; i < in.size(); ++i)
        emit (out, in[i]);
}

void DeviceFx::stopWith (Fault why) noexcept
{
    fault.store (static_cast<int> (why));
}

void DeviceFx::reset() noexcept
{
    resetPending = true;
}

bool DeviceFx::isBusy() const noexcept
{
    return MidiFx::Device::isBusy();
}

void DeviceFx::process (const MidiFx::EventList& in, MidiFx::EventList& out, const MidiFx::Time& time) noexcept
{
    const auto firstNew = out.size();
    play (in, out, time);

    if (licensed != nullptr)
        applyDemo (out, firstNew, time);
}

void DeviceFx::applyDemo (MidiFx::EventList& out, int firstNew, const MidiFx::Time& time) noexcept
{
    // Its demo's time, counted in the song's own (so a bounce has the same holds as playing):
    // the block's length in samples of a fixed rate, the part of a sample left over kept.
    constexpr double rate = 48000.0;
    const auto seconds = juce::jmax (0.0, time.end - time.start) * 60.0 / juce::jmax (1.0, time.bpm);
    const auto exact = seconds * rate + demoCarry;
    const auto frames = juce::jlimit (0, 1 << 30, (int) std::floor (exact));
    demoCarry = juce::jlimit (0.0, 1.0, exact - (double) frames);
    const bool gap = ! demoGate.passesMidi (frames, rate, licensed->load (std::memory_order_relaxed));

    // Into a hold: the notes it let out end now (note-offs after its own events), and nothing
    // of its own comes out until the hold is over.
    const auto ownEnd = out.size();

    if (gap && ! demoGap)
        demoNotes.releaseAll (out, time.start);

    demoGap = gap;
    int kept = firstNew;

    for (int i = firstNew; i < out.size(); ++i)
    {
        const auto e = out[i];

        if (gap)
        {
            if (i < ownEnd)
                continue;
        }
        else if (e.isNoteOn())
        {
            demoNotes.on (e.channel(), e.pitch());
        }
        else if (e.isNoteOff())
        {
            demoNotes.off (e.channel(), e.pitch());
        }

        out[kept++] = e;
    }

    out.truncate (kept);
}

void DeviceFx::play (const MidiFx::EventList& in, MidiFx::EventList& out, const MidiFx::Time& time) noexcept
{
    using namespace sonify;

    emitDue (out, time.end);

    if (instance == nullptr || getFault() != Fault::none)
    {
        passThrough (in, out);
        return;
    }

    auto* io = instance->getIO();

    if (io == nullptr)
    {
        stopWith (Fault::crashed);
        flush (out, time.start);
        passThrough (in, out);
        return;
    }

    io->reset = resetPending ? 1 : 0;
    resetPending = false;
    io->start = time.start;
    io->end = time.end;
    io->bpm = time.bpm;
    io->offline = time.offline ? 1 : 0;

    io->numPieces = juce::jmin (time.getNumPieces(), abi::maxPieces);

    for (int i = 0; i < io->numPieces; ++i)
    {
        const auto& p = time.getPiece (i);
        io->pieces[i] = { p.clock, p.perSample, p.song, p.offset, p.length, p.playing ? 1 : 0, p.segment };
    }

    io->numMarks = juce::jmin (time.getNumMarks(), abi::maxMarks);

    for (int i = 0; i < io->numMarks; ++i)
    {
        const auto& m = time.getMark (i);
        io->marks[i] = { m.ppq, m.barLength, m.bpm, m.segment, m.barLine ? 1 : 0, m.bar, m.keyRoot, m.keyNotes, 0 };
    }

    for (int i = 0; i < abi::maxParams; ++i)
        io->params[i] = get (i);

    io->numIn = juce::jmin (in.size(), abi::maxEvents);

    for (int i = 0; i < io->numIn; ++i)
    {
        const auto& e = in[i];
        auto& slot = io->in[i];
        slot.time = e.time;
        slot.data[0] = e.data[0];
        slot.data[1] = e.data[1];
        slot.data[2] = e.data[2];
        slot.size = e.size;
    }

    io->numOut = 0;
    io->numControls = 0;
    io->logLength = 0;
    io->log[0] = 0;

    const auto startTicks = juce::Time::getHighResolutionTicks();
    const auto result = instance->process();
    const auto micros = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - startTicks) * 1.0e6;
    const auto average = averageMicros.load (std::memory_order_relaxed);
    averageMicros.store (average <= 0.0 ? micros : average + (micros - average) * 0.05, std::memory_order_relaxed);

    if (result != Fault::none)
    {
        // Stopped: what it was playing ends now, and from here the notes go through as they come.
        stopWith (result);
        flush (out, time.start);
        passThrough (in, out);
        return;
    }

    io = instance->getIO();   // its memory may have grown (and moved)

    if (io == nullptr)
    {
        stopWith (Fault::crashed);
        flush (out, time.start);
        passThrough (in, out);
        return;
    }

    // What it played: now, or kept for later. Anything malformed is left out: each kind of
    // message has its own length (program change and channel pressure two bytes, the rest three),
    // and nothing is kept for later than this horizon (beats), so a far-off time can't fill the room.
    constexpr double horizon = 1024.0;
    const auto numOut = juce::jlimit (0, abi::maxEvents, static_cast<int> (io->numOut));

    for (int i = 0; i < numOut; ++i)
    {
        const auto& o = io->out[i];

        if ((o.data[0] & 0x80) == 0 || o.data[0] >= 0xf0 || ! std::isfinite (o.time))
            continue;

        const auto kind = o.data[0] & 0xf0;
        const int length = kind == 0xc0 || kind == 0xd0 ? 2 : 3;

        if (o.size != length || (o.data[1] & 0x80) != 0 || (length == 3 && (o.data[2] & 0x80) != 0))
            continue;

        MidiFx::Event e;
        e.time = juce::jlimit (time.start, time.end + horizon, o.time);
        e.data[0] = o.data[0];
        e.data[1] = o.data[1];
        e.data[2] = o.size >= 3 ? o.data[2] : 0;
        e.size = o.size;

        if (e.time < time.end)
            emit (out, e);
        else
            schedule (e);
    }

    // What it moved in Sonify.
    const auto numControls = juce::jlimit (0, abi::maxControls, static_cast<int> (io->numControls));

    for (int i = 0; i < numControls; ++i)
    {
        const auto& c = io->controls[i];

        if (! std::isfinite (c.value) || (c.kind != abi::controlTarget && c.kind != abi::controlOutput))
            continue;

        Move move;
        move.instance = serial;
        move.kind = static_cast<abi::ControlKind> (c.kind);
        move.output = c.output;
        move.value = juce::jlimit (0.0f, 1.0f, c.value);
        std::memcpy (move.target, c.target, sizeof (move.target));
        move.target[sizeof (move.target) - 1] = 0;

        if (move.kind == abi::controlOutput && ! juce::isPositiveAndBelow (move.output, abi::maxOutputs))
            continue;

        // Offline (a clip's Apply, a test), nothing in Sonify moves: kept for whoever asks.
        if (time.offline || ! controlsLive.load (std::memory_order_relaxed))
        {
            if (offlineMoves.size() < 4096)
                offlineMoves.push_back (move);

            continue;
        }

        const auto scope = movesFifo.write (1);

        if (scope.blockSize1 > 0)
            moves[static_cast<size_t> (scope.startIndex1)] = move;
    }

    // What it logged.
    const auto logLength = juce::jlimit (0, abi::logSize - 1, static_cast<int> (io->logLength));

    if (logLength > 0)
    {
        const auto scope = logFifo.write (juce::jmin (logLength, logFifo.getFreeSpace()));

        if (scope.blockSize1 > 0)
            std::memcpy (logBuffer.data() + scope.startIndex1, io->log, static_cast<size_t> (scope.blockSize1));

        if (scope.blockSize2 > 0)
            std::memcpy (logBuffer.data() + scope.startIndex2, io->log + scope.blockSize1, static_cast<size_t> (scope.blockSize2));
    }
}

juce::String DeviceFx::takeLog()
{
    const auto ready = logFifo.getNumReady();

    if (ready == 0)
        return {};

    std::string text;
    const auto scope = logFifo.read (ready);

    if (scope.blockSize1 > 0)
        text.append (logBuffer.data() + scope.startIndex1, static_cast<size_t> (scope.blockSize1));

    if (scope.blockSize2 > 0)
        text.append (logBuffer.data() + scope.startIndex2, static_cast<size_t> (scope.blockSize2));

    for (auto& c : text)
        if (c == 0)
            c = ' ';

    // Whatever the device wrote: text that isn't UTF-8 (or was cut mid-character) is made safe.
    if (! juce::CharPointer_UTF8::isValidString (text.data(), static_cast<int> (text.size())))
        for (auto& c : text)
            if (static_cast<unsigned char> (c) > 127)
                c = '?';

    return juce::String::fromUTF8 (text.data(), static_cast<int> (text.size()));
}

void DeviceFx::describeInto (juce::DynamicObject& item) const
{
    if (isInDemo())
        item.setProperty ("demo", true);   // the page says so beside it, with Register

    // Protected by someone else: it plays, but its maker's work isn't opened for editing here.
    if (build != nullptr && build->isProtected() && ! isMakerOf (build->deviceId))
        item.setProperty ("locked", true);

    if (build != nullptr)
    {
        item.setProperty ("device", build->describe());
    }
    else
    {
        item.setProperty ("device", missingSettings.getProperty ("device", {}));
        item.setProperty ("missing", true);
    }

    item.setProperty ("maps", juce::JSON::parse (juce::JSON::toString (maps, true)));

    if (const auto text = describeFault (getFault()); text.isNotEmpty())
        item.setProperty ("fault", text);
}

DeviceFx* findFx (juce::int64 serial)
{
    auto& r = registry();
    const std::lock_guard<std::mutex> lock (r.fxLock);
    const auto found = r.liveFx.find (serial);
    return found != r.liveFx.end() ? found->second : nullptr;
}

void takeMoves (std::vector<Move>& into)
{
    into.clear();
    auto& r = registry();
    const std::lock_guard<std::mutex> lock (r.fxLock);

    for (auto& [serial, fx] : r.liveFx)
    {
        auto ready = fx->movesFifo.getNumReady();

        while (ready-- > 0)
        {
            const auto scope = fx->movesFifo.read (1);

            if (scope.blockSize1 == 0)
                break;

            const auto& move = fx->moves[static_cast<size_t> (scope.startIndex1)];

            // Only the last move of each control counts.
            const auto same = std::find_if (into.begin(), into.end(), [&move] (const Move& m)
            {
                return m.instance == move.instance && m.kind == move.kind
                    && (move.kind == sonify::abi::controlOutput ? m.output == move.output
                                                                : std::strncmp (m.target, move.target, sizeof (m.target)) == 0);
            });

            if (same != into.end())
                *same = move;
            else
                into.push_back (move);
        }
    }
}

//==============================================================================
std::unique_ptr<MidiFx::Device> makeFx (const juce::String& typeId, const juce::var& settings)
{
    if (typeId != "device")
        return nullptr;

    const auto description = settings.getProperty ("device", {});
    const auto hash = description.getProperty ("hash", {}).toString();
    const auto deviceId = description.getProperty ("deviceId", {}).toString();
    auto build = findBuild (hash, deviceId);

    // Not this device's build, but its WebAssembly is here, another device's (an older copy of it,
    // the same code, or a song that kept one copy for both): the same code made this device's,
    // named as its MIDI FX has it (and the store's as that one is).
    if (build == nullptr && deviceId.isNotEmpty())
    {
        if (const auto same = findBuild (hash))
        {
            juce::String error;
            build = makeBuild (same->wasm, deviceId, description.getProperty ("name", {}).toString().substring (0, 80),
                               description.getProperty ("description", {}).toString().substring (0, 2000),
                               static_cast<int> (description.getProperty ("version", 0)), error, same->licence);

            if (build != nullptr)
                remember (build);
            else
                build = same;
        }
    }

    std::unique_ptr<DeviceFx> fx;

    if (build != nullptr)
        fx = std::make_unique<DeviceFx> (build, sampleRateNow.load());
    else
        fx = std::make_unique<DeviceFx> (settings);

    if (const auto* params = settings.getProperty ("params", {}).getDynamicObject())
    {
        for (size_t i = 0; i < fx->spec.params.size(); ++i)
        {
            const auto& value = params->getProperty (fx->spec.params[i].id);

            if (value.isDouble() || value.isInt() || value.isInt64() || value.isBool())
                fx->set (static_cast<int> (i), static_cast<float> (static_cast<double> (value)));
        }
    }

    if (settings.getProperty ("maps", {}).isObject())
        fx->maps = juce::JSON::parse (juce::JSON::toString (settings.getProperty ("maps", {}), true));

    return fx;
}
}
