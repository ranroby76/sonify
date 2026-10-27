// D:\workspace\Fanan_web_hub\src\WasmCompiler.h

#pragma once

#include <juce_core/juce_core.h>

#include "Project.h"

#include <atomic>

//==============================================================================
/** A protected build: the product's id (Licences::ProjectProtection, "sp-...") and its
    demo's times, built in (stella_product.h, with STELLA_PROTECTED). No product: the plain
    build. Made from a product id alone, the demo has the standard times. */
struct WasmProtection
{
    juce::String product;
    double playSeconds = 10.0, silentSeconds = 2.0;   // a MIDI FX's: notes passing, then held back

    WasmProtection() = default;
    WasmProtection (const juce::String& productId) : product (productId) {}
    WasmProtection (const juce::String& productId, double play, double silent)
        : product (productId), playSeconds (play), silentSeconds (silent) {}

    bool isProtected() const noexcept    { return product.isNotEmpty(); }
};

//==============================================================================
/**
    Turns a project into a plugin the studio can play live: the project's modules plus
    the Stella runtime, compiled to WebAssembly by the compiler that ships with the
    studio (wasi-sdk). Nothing has to be installed on the user's computer.

    A project holds:
        graph.json   which modules the plugin has and how they're wired
        modules/     one .cpp file per module type, written against stella_api.h
        elements/    programmed GUI elements, one .cpp file each, written against
                     stella_element_api.h: compiled into a module of their own (the GUI's)
*/
class WasmCompiler final
{
public:
    struct Result
    {
        bool ok = false;
        juce::String log;        // the compiler's messages (errors, warnings)
        juce::File wasm;         // the compiled plugin, when ok
        double seconds = 0.0;
    };

    /** A protected build (WasmProtection). */
    using Protection = WasmProtection;

    /** Compiles the project. Takes a few seconds: call it on a worker thread. protection: a
        protected product's build, with its id and its demo inside (stella_licence.h), made in a
        work folder of its own (the live build's stays as it is). The compiler is stopped when
        cancelled turns true, or when it takes too long. */
    static Result compile (const juce::File& projectFolder, const juce::String& projectId, const Protection& protection = {},
                           const std::atomic<bool>* cancelled = nullptr);

    /** Whether a product id can be built into a plugin: letters, digits, '-', '_' and '.'. */
    static bool isValidProductId (const juce::String& productId);

    /** The bundled compiler's folder, or an empty File if it's missing. */
    static juce::File findCompiler();

    static constexpr const char* graphFileName = "graph.json";

    /** graph.json as C++ (stella_graph.cpp), for the live build and for exports. */
    static juce::Result generateGraphSource (const juce::File& graphFile, juce::String& source);

    /** Writes stella_api.h, stella_runtime.h and stella_runtime.cpp into a build folder. */
    static bool writeRuntime (const juce::File& folder);

    //==========================================================================
    static constexpr const char* elementsFolderName = "elements";

    /** A programmed element as its file declares it (STELLA_ELEMENT), before any build. */
    struct ElementSource
    {
        juce::String name, description, file;
    };

    static juce::Array<ElementSource> findElements (const juce::File& projectFolder);

    /** The project's own code (modules/ and elements/) checked before any build: it may bring in
        only files of the project and the compiler's own headers (SourceGuard). "" when fine. */
    static juce::String checkSources (const juce::File& projectFolder);

    /** The toolbox's own elements made the same way: the drop-down list (stella_builtin_elements.cpp),
        the 3D displays (stella_builtin_3d.cpp) and the panel painter (stella_builtin_paint.cpp).
        Every build has them; a project's element of the same name replaces one. */
    static juce::Array<ElementSource> builtInElements();

    /** Compiles the elements folder's .cpp files, with the toolbox's own, into the GUI's module
        (stopped as compile is). */
    static Result compileElements (const juce::File& projectFolder, const juce::String& projectId,
                                   const std::atomic<bool>* cancelled = nullptr);

    /** Writes the elements' runtime (stella_element_api.h and the rest, and the font as
        stella_fonts.h) into a build folder, for the live build and for exports. */
    static bool writeElementRuntime (const juce::File& folder);
};
