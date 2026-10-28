// D:\workspace\Fanan_web_hub\src\WasmCompiler.cpp

#include "WasmCompiler.h"

#include "StellaRuntimeData.h"
#include "DeviceFiles.h"   // Sonify's compiler, shared with its devices
#include "SourceGuard.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace
{
    // Sonify ships one WebAssembly compiler, for its devices and for the Plugins Creator Studio:
    // wasi-sdk's clang (run as C++), its own headers named from the folder as it's given.
    juce::File clangIn (const juce::File& sdk)
    {
       #if JUCE_WINDOWS
        return sdk.getChildFile ("bin").getChildFile ("clang.exe");
       #else
        return sdk.getChildFile ("bin").getChildFile ("clang");
       #endif
    }

    /** The compiler, its sysroot and its own headers: the start of every compile's arguments. */
    juce::StringArray compilerArgs (const juce::File& sdk)
    {
        juce::StringArray args;
        args.add (clangIn (sdk).getFullPathName());
        args.add ("--driver-mode=g++");
        args.add ("--target=wasm32-wasip1");
        args.add ("--sysroot=" + sdk.getChildFile ("share").getChildFile ("wasi-sysroot").getFullPathName());
        args.addArray ({ "-fintegrated-cc1", "-fno-color-diagnostics" });   // as Sonify's device builds run it

        // The C++ library's headers (<cmath>, <vector>...), named here: clang finds them on its own
        // only through an empty include/c++/v1 folder, and Sonify's installer leaves empty folders out.
        if (const auto libcxx = Devices::Compiler::libcxxHeaders (sdk); libcxx.isDirectory())
            { args.add ("-stdlib++-isystem"); args.add (libcxx.getFullPathName()); }

        if (const auto versions = sdk.getChildFile ("lib").getChildFile ("clang").findChildFiles (juce::File::findDirectories, false); ! versions.isEmpty())
            args.add ("-resource-dir=" + versions.getFirst().getFullPathName());

        return args;
    }

    /** Where builds are made: Sonify's own, outside the project, so the project stays clean. */
    juce::File workRoot()
    {
        return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Sonify Plugins Creator");
    }

    const char* const missingCompiler = "Sonify's plugin compiler (the \"Device Compiler\" folder next to Sonify) is missing, "
                                        "so plugins can't be built here. Reinstall Sonify.";

    /** One run of the compiler: its messages in log; true when it ended well. Reading its messages
        waits for them, so a watch of its own stops it when it's asked to stop or takes too long
        (the reading then ends, as it does when the compiler exits). */
    bool runCompiler (const juce::StringArray& args, juce::String& log, int timeoutMs, const std::atomic<bool>* cancelled)
    {
        juce::ChildProcess clang;

        if (! clang.start (args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
        {
            log = "The plugin compiler couldn't be started.";
            return false;
        }

        const auto started = juce::Time::getMillisecondCounterHiRes();
        std::atomic<bool> finished { false }, stopped { false }, timedOut { false };

        std::thread watch ([&]
        {
            while (! finished.load())
            {
                if (cancelled != nullptr && cancelled->load())
                    stopped.store (true);
                else if (juce::Time::getMillisecondCounterHiRes() - started > timeoutMs)
                    timedOut.store (true);

                if (stopped.load() || timedOut.load())
                {
                    clang.kill();
                    return;
                }

                std::this_thread::sleep_for (std::chrono::milliseconds (10));
            }
        });

        juce::MemoryOutputStream messages;
        char buffer[4096];

        for (;;)
        {
            const auto read = clang.readProcessOutput (buffer, sizeof (buffer));

            if (read > 0)
            {
                messages.write (buffer, static_cast<size_t> (read));
                continue;
            }

            if (! clang.isRunning() || stopped.load() || timedOut.load())
                break;

            juce::Thread::sleep (5);
        }

        finished.store (true);
        watch.join();
        log = messages.toString();

        if (stopped.load())
        {
            log = "Stopped.";
            return false;
        }

        if (timedOut.load())
        {
            log = (log.trim() + "\n\nThe build took too long and was stopped.").trim();
            return false;
        }

        clang.waitForProcessToFinish (5000);
        return clang.getExitCode() == 0;
    }

    /** A C++ string literal for any text. */
    juce::String literal (const juce::String& text)
    {
        juce::String result ("\"");

        for (auto c : text)
        {
            if (c == '"' || c == '\\')
                result << '\\' << juce::String::charToString (c);
            else if (c >= 32 && c < 127)
                result << juce::String::charToString (c);
        }

        return result + "\"";
    }

    bool isIdentifier (const juce::String& text)
    {
        return text.isNotEmpty()
            && text.containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")
            && ! juce::CharacterFunctions::isDigit (text[0]);
    }

    /** "voices.out" -> ("voices", "out"). Port names may have spaces ("out L"). */
    bool splitEnd (const juce::String& end, juce::String& module, juce::String& port)
    {
        const auto dot = end.indexOfChar ('.');

        if (dot <= 0 || dot >= end.length() - 1)
            return false;

        module = end.substring (0, dot).trim();
        port = end.substring (dot + 1).trim();
        return module.isNotEmpty() && port.isNotEmpty();
    }

    bool writeResource (const juce::File& file, const char* data, int size)
    {
        return file.replaceWithData (data, (size_t) size);
    }

    /** Bytes as the body of a C++ array, 24 to a line. */
    void appendBytes (juce::MemoryOutputStream& out, const char* data, int size)
    {
        static const char* const hex = "0123456789abcdef";
        const auto* bytes = reinterpret_cast<const unsigned char*> (data);
        char line[24 * 5 + 2];

        for (int i = 0; i < size; i += 24)
        {
            auto* c = line;

            for (int j = i; j < juce::jmin (size, i + 24); ++j)
            {
                *c++ = '0'; *c++ = 'x';
                *c++ = hex[bytes[j] >> 4];
                *c++ = hex[bytes[j] & 15];
                *c++ = ',';
            }

            *c++ = '\n';
            out.write (line, (size_t) (c - line));
        }
    }

    juce::String fontsHeader()
    {
        juce::MemoryOutputStream out;
        out << "// Generated by Sonify's Plugins Creator Studio: the Inter font (SIL Open Font License 1.1, rsms.me/inter), regular and bold.\n"
            << "#pragma once\n\nnamespace stella::ui::fonts\n{\n    static const unsigned char regular[] =\n    {\n";
        appendBytes (out, StellaRuntimeData::inter_regular_ttf, StellaRuntimeData::inter_regular_ttfSize);
        out << "    };\n\n    static const unsigned char bold[] =\n    {\n";
        appendBytes (out, StellaRuntimeData::inter_bold_ttf, StellaRuntimeData::inter_bold_ttfSize);
        out << "    };\n}\n";
        return out.toString();
    }

    struct Resource
    {
        const char* name;
        const char* data;
        int size;
    };

    std::vector<Resource> elementRuntimeFiles()
    {
        return { { "stella_element_api.h", StellaRuntimeData::stella_element_api_h, StellaRuntimeData::stella_element_api_hSize },
                 { "stella_element_host.h", StellaRuntimeData::stella_element_host_h, StellaRuntimeData::stella_element_host_hSize },
                 { "stella_elements.h", StellaRuntimeData::stella_elements_h, StellaRuntimeData::stella_elements_hSize },
                 { "stella_elements.cpp", StellaRuntimeData::stella_elements_cpp, StellaRuntimeData::stella_elements_cppSize },
                 { "stella_builtin_elements.cpp", StellaRuntimeData::stella_builtin_elements_cpp, StellaRuntimeData::stella_builtin_elements_cppSize },
                 { "stella_view3d.h", StellaRuntimeData::stella_view3d_h, StellaRuntimeData::stella_view3d_hSize },
                 { "stella_builtin_3d.cpp", StellaRuntimeData::stella_builtin_3d_cpp, StellaRuntimeData::stella_builtin_3d_cppSize },
                 { "stella_paint.h", StellaRuntimeData::stella_paint_h, StellaRuntimeData::stella_paint_hSize },
                 { "stella_builtin_paint.cpp", StellaRuntimeData::stella_builtin_paint_cpp, StellaRuntimeData::stella_builtin_paint_cppSize },
                 { "stb_truetype.h", StellaRuntimeData::stb_truetype_h, StellaRuntimeData::stb_truetype_hSize } };
    }

    /** Which version of the elements' runtime this studio carries: its compiled form is kept by it. */
    juce::String elementRuntimeKey()
    {
        std::uint64_t hash = 14695981039346656037ull;

        auto add = [&hash] (const char* data, int size)
        {
            for (int i = 0; i < size; ++i)
                hash = (hash ^ (std::uint8_t) data[i]) * 1099511628211ull;
        };

        for (const auto& file : elementRuntimeFiles())
            add (file.data, file.size);

        add (StellaRuntimeData::inter_regular_ttf, StellaRuntimeData::inter_regular_ttfSize);
        add (StellaRuntimeData::inter_bold_ttf, StellaRuntimeData::inter_bold_ttfSize);
        return juce::String::toHexString ((juce::int64) hash);
    }

    bool isQuote (juce::juce_wchar c)    { return c == '"'; }

    /** The STELLA_ELEMENT (Name, "description") lines of a file (or another macro's). */
    void scanElements (const juce::String& text, const juce::String& file, juce::Array<WasmCompiler::ElementSource>& found,
                       const juce::String& macro = "STELLA_ELEMENT")
    {
        for (int at = text.indexOf (macro); at >= 0; at = text.indexOf (at + macro.length(), macro))
        {
            auto p = text.getCharPointer() + at + macro.length();
            auto skipSpace = [&p] { while (! p.isEmpty() && p.isWhitespace()) ++p; };

            skipSpace();

            if (*p != '(')
                continue;

            ++p;
            skipSpace();
            juce::String name;

            while (! p.isEmpty() && (p.isLetterOrDigit() || *p == '_'))
                name += juce::String::charToString (*p++);

            skipSpace();

            if (name.isEmpty() || *p != ',')
                continue;

            ++p;
            juce::String description;

            // One or more string literals in a row.
            for (;;)
            {
                skipSpace();

                if (! isQuote (*p))
                    break;

                ++p;

                while (! p.isEmpty() && ! isQuote (*p))
                {
                    if (*p == '\\' && ! (p + 1).isEmpty())
                        ++p;

                    description += juce::String::charToString (*p++);
                }

                if (! p.isEmpty())
                    ++p;
            }

            found.add ({ name, description, file });
        }
    }
}

//==============================================================================
juce::File WasmCompiler::findCompiler()
{
    // Sonify's own: "Device Compiler" next to it (or SONIFY_DEVICE_COMPILER while developing).
    const auto sdk = Devices::Compiler::folder();

    if (clangIn (sdk).existsAsFile() && sdk.getChildFile ("share").getChildFile ("wasi-sysroot").isDirectory())
        return sdk;

    return {};
}

//==============================================================================
juce::String WasmCompiler::checkSources (const juce::File& projectFolder)
{
    // The compiler reads whatever file a source names, and its messages go back to Fanan AI:
    // nothing outside the project gets read that way.
    for (const auto& folder : { projectFolder.getChildFile ("modules"), projectFolder.getChildFile (elementsFolderName) })
        if (const auto problem = SourceGuard::problemInFolder (folder, projectFolder); problem.isNotEmpty())
            return problem;

    return {};
}

juce::Result WasmCompiler::generateGraphSource (const juce::File& graphFile, juce::String& source)
{
    juce::var graph;
    const auto parsed = juce::JSON::parse (graphFile.loadFileAsString(), graph);

    if (parsed.failed() || ! graph.isObject())
        return juce::Result::fail ("graph.json isn't valid JSON: " + parsed.getErrorMessage());

    const auto* modules = graph.getProperty ("modules", {}).getArray();
    const auto* wires = graph.getProperty ("wires", {}).getArray();

    if (modules == nullptr || modules->isEmpty())
        return juce::Result::fail ("graph.json has no modules.");

    juce::String moduleRows, wireRows;

    for (const auto& module : *modules)
    {
        const auto id = module.getProperty ("id", {}).toString().trim();
        const auto type = module.getProperty ("type", {}).toString().trim();

        if (! isIdentifier (id) || id == "plugin")
            return juce::Result::fail ("graph.json: \"" + id + "\" can't be a module id (letters, digits and _ only, and not \"plugin\").");

        if (! isIdentifier (type))
            return juce::Result::fail ("graph.json: \"" + type + "\" isn't a module type name.");

        moduleRows << "        { " << literal (id) << ", " << literal (type) << " },\n";
    }

    int numWires = 0;

    if (wires != nullptr)
    {
        for (const auto& wire : *wires)
        {
            juce::String fromModule, fromPort, toModule, toPort;

            if (! splitEnd (wire.getProperty ("from", {}).toString(), fromModule, fromPort)
                || ! splitEnd (wire.getProperty ("to", {}).toString(), toModule, toPort))
                return juce::Result::fail ("graph.json: a wire needs \"from\" and \"to\" like \"filter.out\".");

            wireRows << "        { " << literal (fromModule) << ", " << literal (fromPort) << ", "
                     << literal (toModule) << ", " << literal (toPort) << " },\n";
            ++numWires;
        }
    }

    if (numWires == 0)
        wireRows = "        { \"\", \"\", \"\", \"\" },\n";   // an array can't be empty; the count says 0

    source.clear();
    source << "// Generated by Sonify's Plugins Creator Studio from graph.json. Don't edit: it's rewritten on every build.\n\n"
           << "#include \"stella_runtime.h\"\n\n"
           << "namespace stella\n{\n"
           << "    extern const GraphModule graphModules[]\n    {\n" << moduleRows << "    };\n\n"
           << "    extern const int numGraphModules = " << modules->size() << ";\n\n"
           << "    extern const GraphWire graphWires[]\n    {\n" << wireRows << "    };\n\n"
           << "    extern const int numGraphWires = " << numWires << ";\n"
           << "}\n";

    return juce::Result::ok();
}

bool WasmCompiler::writeRuntime (const juce::File& folder)
{
    return writeResource (folder.getChildFile ("stella_api.h"), StellaRuntimeData::stella_api_h, StellaRuntimeData::stella_api_hSize)
        && writeResource (folder.getChildFile ("stella_runtime.h"), StellaRuntimeData::stella_runtime_h, StellaRuntimeData::stella_runtime_hSize)
        && writeResource (folder.getChildFile ("stella_runtime.cpp"), StellaRuntimeData::stella_runtime_cpp, StellaRuntimeData::stella_runtime_cppSize)
        && writeResource (folder.getChildFile ("stella_licence.h"), StellaRuntimeData::stella_licence_h, StellaRuntimeData::stella_licence_hSize)
        && writeResource (folder.getChildFile ("stella_blob.h"), StellaRuntimeData::stella_blob_h, StellaRuntimeData::stella_blob_hSize);
}

//==============================================================================
juce::Array<WasmCompiler::ElementSource> WasmCompiler::findElements (const juce::File& projectFolder)
{
    juce::Array<ElementSource> found;
    auto files = projectFolder.getChildFile (elementsFolderName).findChildFiles (juce::File::findFiles, false, "*.cpp");
    files.sort();

    for (const auto& file : files)
        scanElements (file.loadFileAsString(), juce::String (elementsFolderName) + "/" + file.getFileName(), found);

    return found;
}

juce::Array<WasmCompiler::ElementSource> WasmCompiler::builtInElements()
{
    juce::Array<ElementSource> found;
    scanElements (juce::String::fromUTF8 (StellaRuntimeData::stella_builtin_elements_cpp, StellaRuntimeData::stella_builtin_elements_cppSize),
                  "the toolbox", found, "STELLA_BUILTIN_ELEMENT");
    scanElements (juce::String::fromUTF8 (StellaRuntimeData::stella_builtin_3d_cpp, StellaRuntimeData::stella_builtin_3d_cppSize),
                  "the toolbox", found, "STELLA_BUILTIN_ELEMENT");
    scanElements (juce::String::fromUTF8 (StellaRuntimeData::stella_builtin_paint_cpp, StellaRuntimeData::stella_builtin_paint_cppSize),
                  "the toolbox", found, "STELLA_BUILTIN_ELEMENT");
    return found;
}

bool WasmCompiler::writeElementRuntime (const juce::File& folder)
{
    for (const auto& file : elementRuntimeFiles())
        if (! writeResource (folder.getChildFile (file.name), file.data, file.size))
            return false;

    const auto fonts = folder.getChildFile ("stella_fonts.h");
    const auto text = fontsHeader();
    return (fonts.existsAsFile() && fonts.loadFileAsString() == text) || fonts.replaceWithText (text);
}

WasmCompiler::Result WasmCompiler::compileElements (const juce::File& projectFolder, const juce::String& projectId,
                                                    const std::atomic<bool>* cancelled)
{
    Result result;
    const auto started = juce::Time::getMillisecondCounterHiRes();
    const auto elements = projectFolder.getChildFile (elementsFolderName);

    // Built even with no files of the project's own: the toolbox's drop-down list is one.
    auto files = elements.findChildFiles (juce::File::findFiles, false, "*.cpp");
    files.sort();

    const auto sdk = findCompiler();

    if (sdk == juce::File())
    {
        result.log = missingCompiler;
        return result;
    }

    if (const auto problem = checkSources (projectFolder); problem.isNotEmpty())
    {
        result.log = problem;
        return result;
    }

    const auto studioTemp = workRoot();
    const auto work = studioTemp.getChildFile ("build-" + juce::File::createLegalFileName (projectId));
    const auto runtime = studioTemp.getChildFile ("element-runtime");

    if ((! work.isDirectory() && ! work.createDirectory()) || (! runtime.isDirectory() && ! runtime.createDirectory()))
    {
        result.log = "Couldn't create the build folder " + work.getFullPathName();
        return result;
    }

    auto baseArgs = [&sdk]
    {
        auto args = compilerArgs (sdk);
        args.addArray ({ "-O2", "-std=c++17", "-fno-exceptions", "-fno-rtti", "-Wall" });
        return args;
    };

    auto run = [cancelled] (const juce::StringArray& args, juce::String& log, int timeoutMs)
    {
        return runCompiler (args, log, timeoutMs, cancelled);
    };

    // The runtime (drawing, text, the font) and the toolbox's elements (the drop-down list, the
    // 3D displays, the panel painter) are compiled once per studio version and kept.
    const auto key = elementRuntimeKey();
    const auto object = runtime.getChildFile ("runtime-" + key + ".o");
    const auto builtIns = runtime.getChildFile ("builtins-" + key + ".o");
    const auto builtIns3d = runtime.getChildFile ("builtins3d-" + key + ".o");
    const auto builtInsPaint = runtime.getChildFile ("builtinspaint-" + key + ".o");

    if (! writeElementRuntime (runtime))
    {
        result.log = "Couldn't write into the build folder " + runtime.getFullPathName();
        return result;
    }

    for (const auto& [source, target] : { std::pair<const char*, juce::File> { "stella_elements.cpp", object },
                                          std::pair<const char*, juce::File> { "stella_builtin_elements.cpp", builtIns },
                                          std::pair<const char*, juce::File> { "stella_builtin_3d.cpp", builtIns3d },
                                          std::pair<const char*, juce::File> { "stella_builtin_paint.cpp", builtInsPaint } })
    {
        if (target.existsAsFile())
            continue;

        const auto partial = runtime.getChildFile ("building.o");
        auto args = baseArgs();
        args.add ("-I" + runtime.getFullPathName());
        args.add ("-c");
        args.add (runtime.getChildFile (source).getFullPathName());
        args.add ("-o");
        args.add (partial.getFullPathName());

        juce::String log;

        if (! run (args, log, 120000) || ! partial.existsAsFile() || ! partial.moveFileTo (target))
        {
            result.log = "The GUI elements' runtime didn't compile:\n" + log.trim();
            return result;
        }
    }

    const auto output = work.getChildFile ("elements.wasm");
    output.deleteFile();

    auto args = baseArgs();
    args.add ("-mexec-model=reactor");
    args.add ("-I" + runtime.getFullPathName());
    args.add ("-I" + elements.getFullPathName());
    args.add ("-o");
    args.add (output.getFullPathName());
    args.add (object.getFullPathName());
    args.add (builtIns.getFullPathName());
    args.add (builtIns3d.getFullPathName());
    args.add (builtInsPaint.getFullPathName());

    for (const auto& file : files)
        args.add (file.getFullPathName());

    args.add ("-Wl,--strip-all");

    juce::String log;
    const bool linked = run (args, log, 60000);

    result.log = log.replace (elements.getFullPathName() + juce::File::getSeparatorString(), juce::String (elementsFolderName) + "/")
                    .replace (runtime.getFullPathName() + juce::File::getSeparatorString(), "")
                    .trim();
    result.ok = linked && output.existsAsFile();
    result.wasm = result.ok ? output : juce::File();
    result.seconds = (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0;

    if (! result.ok && result.log.isEmpty())
        result.log = "The GUI elements' compiler stopped without saying why.";

    return result;
}

//==============================================================================
bool WasmCompiler::isValidProductId (const juce::String& productId)
{
    return productId.isNotEmpty() && productId.length() <= 80
        && productId.containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.");
}

WasmCompiler::Result WasmCompiler::compile (const juce::File& projectFolder, const juce::String& projectId, const Protection& protection,
                                            const std::atomic<bool>* cancelled)
{
    Result result;
    const auto& protectedProduct = protection.product;

    if (protectedProduct.isNotEmpty() && ! isValidProductId (protectedProduct))
    {
        result.log = "Its product id isn't one that can be built in: " + protectedProduct;
        return result;
    }

    const auto started = juce::Time::getMillisecondCounterHiRes();

    const auto sdk = findCompiler();

    if (sdk == juce::File())
    {
        result.log = missingCompiler;
        return result;
    }

    if (const auto problem = checkSources (projectFolder); problem.isNotEmpty())
    {
        result.log = problem;
        return result;
    }

    // The work folder is the studio's own, outside the project: the project stays clean.
    const auto work = workRoot().getChildFile ((protectedProduct.isNotEmpty() ? "protected-" : "build-") + juce::File::createLegalFileName (projectId));

    if (! work.isDirectory() && ! work.createDirectory())
    {
        result.log = "Couldn't create the build folder " + work.getFullPathName();
        return result;
    }

    juce::String graphSource;

    if (const auto graph = generateGraphSource (projectFolder.getChildFile (graphFileName), graphSource); graph.failed())
    {
        result.log = graph.getErrorMessage();
        return result;
    }

    const auto output = work.getChildFile ("plugin.wasm");
    output.deleteFile();

    if (! writeRuntime (work) || ! work.getChildFile ("stella_graph.cpp").replaceWithText (graphSource))
    {
        result.log = "Couldn't write into the build folder " + work.getFullPathName();
        return result;
    }

    const auto modules = projectFolder.getChildFile ("modules");
    auto moduleFiles = modules.findChildFiles (juce::File::findFiles, false, "*.cpp");
    moduleFiles.sort();

    auto args = compilerArgs (sdk);
    args.addArray ({ "-O2", "-std=c++17", "-fno-exceptions", "-fno-rtti", "-mexec-model=reactor", "-Wall" });
    args.add ("-I" + work.getFullPathName());
    args.add ("-I" + modules.getFullPathName());

    // A protected product: its id and its demo's times built in (from a header: quotes don't
    // survive every command line). It plays its demo until it's registered on the computer.
    if (protectedProduct.isNotEmpty())
    {
        const auto seconds = [] (double value, double fallback)
        {
            // Whole milliseconds, as (ms / 1000.0): the same text whatever the computer's language.
            const auto ms = juce::roundToInt ((value > 0.0 && value <= 3600.0 ? value : fallback) * 1000.0);
            return "(" + juce::String (ms) + " / 1000.0)";
        };

        const juce::String header = "// Written by Sonify's Protection tab for this build: the product's id and its demo.\n"
                                    "#pragma once\n"
                                    "#define STELLA_PRODUCT \"" + protectedProduct + "\"\n"
                                    "#define STELLA_DEMO_PLAY " + seconds (protection.playSeconds, 10.0) + "      // seconds of sound (a MIDI FX: of notes passing)\n"
                                    "#define STELLA_DEMO_SILENT " + seconds (protection.silentSeconds, 2.0) + "    // seconds of silence (a MIDI FX: of notes held back)\n";

        if (! work.getChildFile ("stella_product.h").replaceWithText (header))
        {
            result.log = "Couldn't write into the build folder " + work.getFullPathName();
            return result;
        }

        args.add ("-DSTELLA_PROTECTED=1");
    }

    args.add ("-o");
    args.add (output.getFullPathName());
    args.add (work.getChildFile ("stella_runtime.cpp").getFullPathName());
    args.add (work.getChildFile ("stella_graph.cpp").getFullPathName());

    for (const auto& file : moduleFiles)
        args.add (file.getFullPathName());

    args.add ("-Wl,--strip-all");

    juce::String log;
    const bool compiled = runCompiler (args, log, 120000, cancelled);

    // Shorter messages: "modules/LowPass.cpp:12:3: error ..." rather than full paths.
    log = log.replace (modules.getFullPathName() + juce::File::getSeparatorString(), "modules/")
             .replace (work.getFullPathName() + juce::File::getSeparatorString(), "")
             .trim();

    result.log = log;
    result.ok = compiled && output.existsAsFile();
    result.wasm = result.ok ? output : juce::File();
    result.seconds = (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0;

    if (! result.ok && result.log.isEmpty())
        result.log = "The plugin compiler stopped without saying why.";

    return result;
}
