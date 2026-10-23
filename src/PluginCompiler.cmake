# D:\workspace\Fanan_web_hub\src\PluginCompiler.cmake
#
# Export Plugin's tools (PluginExporter): the plugin formats' headers, and on Windows the compiler
# that builds the plugin files on the user's computer. They ship with Sonify:
#   llvm-mingw 20260922 (Windows): copied next to Sonify.exe as "Plugin Compiler", only its
#     64-bit Windows part (the compiler, the linker, and the C and C++ headers and libraries)
#   VST3 3.8.1's pluginterfaces (MIT licensed) and CLAP's headers (the ones WCLAP hosting brings,
#     also MIT): copied as "Plugin SDKs" (on macOS into the app's Resources)
# A Mac uses Apple's own compiler (its Command Line Tools), Linux the system's C++ compiler.
#
# Downloaded once into <build>/deps at pinned versions (or taken from the copy kept beside the
# project), as WclapBridge.cmake does: include this after it. Sets FANAN_PLUGIN_COMPILER_DIR (the
# unpacked llvm-mingw; empty off Windows), FANAN_PLUGIN_VST3_DIR (the folder holding
# pluginterfaces/) and FANAN_PLUGIN_CLAP_DIR (the folder holding clap/).

include_guard(GLOBAL)

set(FANAN_PLUGIN_COMPILER_DIR "")

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    fanan_fetch_archive(llvm-mingw
        "https://github.com/mstorsjo/llvm-mingw/releases/download/20260922/llvm-mingw-20260922-ucrt-x86_64.zip"
        "e3ad77d117a4bea19a7a3b333341824d79a5a371004a10e25b8504e7b3047666" FANAN_PLUGIN_COMPILER_DIR)
endif()

fanan_fetch_archive(vst3-pluginterfaces
    "https://codeload.github.com/steinbergmedia/vst3_pluginterfaces/tar.gz/refs/tags/v3.8.1_build_84" "" _fanan_vst3_interfaces)

# The headers are included as "pluginterfaces/...", so they need a folder of that name.
set(FANAN_PLUGIN_VST3_DIR "${FANAN_DEPS_DIR}/vst3-sdk")

if(NOT EXISTS "${FANAN_PLUGIN_VST3_DIR}/pluginterfaces/base/funknown.h")
    file(COPY "${_fanan_vst3_interfaces}/" DESTINATION "${FANAN_PLUGIN_VST3_DIR}/pluginterfaces")
endif()

set(FANAN_PLUGIN_CLAP_DIR "${FANAN_CLAP_DIR}/include")

# ------------------------------------------------------------------------------
# fanan_copy_plugin_tools(target)
# After each build, beside the target's executable (on macOS in the app's Resources; only what
# changed is copied again):
#     Plugin SDKs/clap/include/clap          CLAP's headers
#     Plugin SDKs/vst3/pluginterfaces        VST3's interfaces
#     Plugin Compiler/                       (Windows) llvm-mingw's 64-bit Windows part:
#         bin/                               clang, its 64-bit Windows driver, lld and their DLLs
#         lib/clang                          clang's own headers, and its runtime for 64-bit Windows
#         x86_64-w64-mingw32/                the Windows headers and libraries
#         generic-w64-mingw32/               (the headers, where llvm-mingw keeps them shared)
# ------------------------------------------------------------------------------
function(fanan_copy_plugin_tools target)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.26")
        set(copy_folder copy_directory_if_different)
    else()
        set(copy_folder copy_directory)
    endif()

    if(APPLE)
        set(sdks "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/Plugin SDKs")
    else()
        set(sdks "$<TARGET_FILE_DIR:${target}>/Plugin SDKs")
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${sdks}/clap/include" "${sdks}/vst3"
        COMMAND "${CMAKE_COMMAND}" -E ${copy_folder} "${FANAN_PLUGIN_CLAP_DIR}/clap" "${sdks}/clap/include/clap"
        COMMAND "${CMAKE_COMMAND}" -E ${copy_folder} "${FANAN_PLUGIN_VST3_DIR}/pluginterfaces" "${sdks}/vst3/pluginterfaces"
        VERBATIM)

    if(NOT FANAN_PLUGIN_COMPILER_DIR)
        return()
    endif()

    set(from "${FANAN_PLUGIN_COMPILER_DIR}")
    set(to "$<TARGET_FILE_DIR:${target}>/Plugin Compiler")

    # From bin, what building a 64-bit Windows plugin runs: clang, its 64-bit Windows driver,
    # the linker, and the DLLs and settings they need (not the other targets' drivers, nor the
    # debugger and the other tools).
    file(GLOB _fanan_mingw_bin LIST_DIRECTORIES false "${from}/bin/*")
    set(tools "")

    foreach(file IN LISTS _fanan_mingw_bin)
        get_filename_component(name "${file}" NAME)

        if(name MATCHES "^(clang|clang\\+\\+|clang-[0-9]+|ld\\.lld|lld|lld-link|x86_64-w64-mingw32-.*)\\.exe$" OR name MATCHES "\\.(dll|cfg)$")
            list(APPEND tools "${file}")
        endif()
    endforeach()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${to}/bin" "${to}/lib"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${tools} "${to}/bin"
        COMMAND "${CMAKE_COMMAND}" -E ${copy_folder} "${from}/x86_64-w64-mingw32" "${to}/x86_64-w64-mingw32"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${from}/LICENSE.TXT" "${to}/LICENSE.TXT"
        VERBATIM)

    # clang's own headers, and its runtime for 64-bit Windows (not the other targets', nor the sanitizers).
    file(GLOB _fanan_clang_versions LIST_DIRECTORIES true "${from}/lib/clang/*")

    foreach(version IN LISTS _fanan_clang_versions)
        if(NOT IS_DIRECTORY "${version}")
            continue()
        endif()

        get_filename_component(number "${version}" NAME)
        set(runtime "${version}/lib/windows/libclang_rt.builtins-x86_64.a")

        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${to}/lib/clang/${number}/lib/windows"
            COMMAND "${CMAKE_COMMAND}" -E ${copy_folder} "${version}/include" "${to}/lib/clang/${number}/include"
            VERBATIM)

        if(EXISTS "${runtime}")
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${runtime}" "${to}/lib/clang/${number}/lib/windows"
                VERBATIM)
        endif()
    endforeach()

    # The headers: in the 64-bit folder itself, or shared (a link to them there, where links are kept).
    if(IS_DIRECTORY "${from}/generic-w64-mingw32" AND (IS_SYMLINK "${from}/x86_64-w64-mingw32/include"
                                                       OR NOT EXISTS "${from}/x86_64-w64-mingw32/include/windows.h"))
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E ${copy_folder} "${from}/generic-w64-mingw32" "${to}/generic-w64-mingw32"
            VERBATIM)
    endif()

    if(IS_DIRECTORY "${from}/include")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E ${copy_folder} "${from}/include" "${to}/include"
            VERBATIM)
    endif()
endfunction()
