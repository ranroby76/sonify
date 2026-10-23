# D:\workspace\Fanan_web_hub\src\WclapBridge.cmake
#
# Builds WebCLAP's wclap-bridge (loads a WCLAP through Wasmtime and hands back an
# ordinary CLAP plugin factory) as the static library FananWclapBridge.
#
# Everything is downloaded once into <build>/deps at pinned versions - no Git
# needed, nothing installed outside the build folder. A copy kept beside the
# project (deps\ next to CMakeLists.txt, copied there from a build folder) is used
# instead wherever it holds the same pinned download, so a new build folder
# downloads nothing. On Windows, Wasmtime is linked as a DLL and FANAN_WASMTIME_DLL
# names the file to copy next to the app.

include_guard(GLOBAL)

set(FANAN_WCLAP_BRIDGE_COMMIT "cd11d22afbe2af350f24cd56e6e0536e5ca86452")
set(FANAN_WCLAP_CPP_COMMIT    "4ad30038a25162e18183d28fa293a9e613b6f593")
set(FANAN_CLAP_COMMIT         "29ffcc273be7c7c651f6c9953b99e69700e2387a")   # CLAP 1.2.7, as pinned by wclap-bridge
set(FANAN_WASMTIME_VERSION    "39.0.1")

set(FANAN_DEPS_DIR "${CMAKE_BINARY_DIR}/deps")
set(FANAN_KEPT_DEPS_DIR "${CMAKE_SOURCE_DIR}/deps")   # optional: a copy kept beside the project

# ------------------------------------------------------------------------------
# Download + unpack an archive once. Returns the single top-level folder inside it.
# ------------------------------------------------------------------------------
function(fanan_fetch_archive name url sha256 out_var)
    set(dest "${FANAN_DEPS_DIR}/${name}")
    set(stamp "${FANAN_DEPS_DIR}/${name}.done")

    # The copy kept beside the project, when it's this very download (its stamp names
    # the same URL, so another version or platform is never mixed in): used as it is.
    set(kept_stamp "${FANAN_KEPT_DEPS_DIR}/${name}.done")

    if(EXISTS "${kept_stamp}")
        file(READ "${kept_stamp}" kept_url)
        string(STRIP "${kept_url}" kept_url)

        if(kept_url STREQUAL url)
            set(dest "${FANAN_KEPT_DEPS_DIR}/${name}")
            set(stamp "${kept_stamp}")
            message(STATUS "Using ${name} kept in ${FANAN_KEPT_DEPS_DIR}")
        endif()
    endif()

    if(NOT EXISTS "${stamp}")
        file(REMOVE_RECURSE "${dest}")

        get_filename_component(archive_name "${url}" NAME)
        set(archive "${FANAN_DEPS_DIR}/downloads/${name}-${archive_name}")

        message(STATUS "Downloading ${name}")

        if(sha256)
            file(DOWNLOAD "${url}" "${archive}" STATUS status TLS_VERIFY ON EXPECTED_HASH SHA256=${sha256})
        else()
            file(DOWNLOAD "${url}" "${archive}" STATUS status TLS_VERIFY ON)
        endif()

        list(GET status 0 code)

        if(NOT code EQUAL 0)
            file(REMOVE "${archive}")
            message(FATAL_ERROR "Download of ${name} failed: ${status}")
        endif()

        file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${dest}")
        file(WRITE "${stamp}" "${url}\n")
    endif()

    file(GLOB children LIST_DIRECTORIES true "${dest}/*")
    list(LENGTH children count)

    if(NOT count EQUAL 1)
        message(FATAL_ERROR "Unexpected layout after unpacking ${name} into ${dest}")
    endif()

    set(${out_var} "${children}" PARENT_SCOPE)
endfunction()

# ------------------------------------------------------------------------------
# Replace one exact piece of source text. Safe to run on every configure: a file
# that already carries the replacement is left alone.
# ------------------------------------------------------------------------------
function(fanan_patch_source file old new)
    file(READ "${file}" content)

    string(FIND "${content}" "${new}" already_patched)

    if(NOT already_patched EQUAL -1)
        return()
    endif()

    string(FIND "${content}" "${old}" found)

    if(found EQUAL -1)
        message(FATAL_ERROR "Patch target not found in ${file} - the pinned source changed?")
    endif()

    string(REPLACE "${old}" "${new}" content "${content}")
    file(WRITE "${file}" "${content}")
endfunction()

# ------------------------------------------------------------------------------
# Sources
# ------------------------------------------------------------------------------
fanan_fetch_archive(wclap-bridge
    "https://github.com/WebCLAP/wclap-bridge/archive/${FANAN_WCLAP_BRIDGE_COMMIT}.zip" "" WCLAP_BRIDGE_DIR)

fanan_fetch_archive(wclap-cpp
    "https://github.com/WebCLAP/wclap-cpp/archive/${FANAN_WCLAP_CPP_COMMIT}.zip" "" WCLAP_CPP_DIR)

fanan_fetch_archive(clap
    "https://github.com/free-audio/clap/archive/${FANAN_CLAP_COMMIT}.zip" "" FANAN_CLAP_DIR)

set(_wasmtime_base "https://github.com/bytecodealliance/wasmtime/releases/download/v${FANAN_WASMTIME_VERSION}")

if(WIN32)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "WCLAP hosting needs a 64-bit Windows build (-A x64)")
    endif()

    fanan_fetch_archive(wasmtime
        "${_wasmtime_base}/wasmtime-v${FANAN_WASMTIME_VERSION}-x86_64-windows-c-api.zip"
        "4939453176b7a86cf4f2cd9839da100a4e30f0505a4a471f2dba92a012e4975c" WASMTIME_DIR)
elseif(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        fanan_fetch_archive(wasmtime
            "${_wasmtime_base}/wasmtime-v${FANAN_WASMTIME_VERSION}-aarch64-macos-c-api.tar.xz"
            "afd838069897a4a5246f178c5db95f68d0aa288cd71499970c74ebc8bced970f" WASMTIME_DIR)
    else()
        fanan_fetch_archive(wasmtime
            "${_wasmtime_base}/wasmtime-v${FANAN_WASMTIME_VERSION}-x86_64-macos-c-api.tar.xz"
            "0ff4f203106fe2b9b7b940a670282c41b01450b44c2157693a5bfb4848391a09" WASMTIME_DIR)
    endif()
else()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        fanan_fetch_archive(wasmtime
            "${_wasmtime_base}/wasmtime-v${FANAN_WASMTIME_VERSION}-aarch64-linux-c-api.tar.xz"
            "959f9ca5ccd2c9d997576a2ba460e9df529a7705e84dbdc8ab89ae9e0fdd1d51" WASMTIME_DIR)
    else()
        fanan_fetch_archive(wasmtime
            "${_wasmtime_base}/wasmtime-v${FANAN_WASMTIME_VERSION}-x86_64-linux-c-api.tar.xz"
            "f1e2b41f9c1d8d097506ad584e7d56140ebfdc6b6e5ae7cbfc8c829dfff3487e" WASMTIME_DIR)
    endif()
endif()

# ------------------------------------------------------------------------------
# Patches
# ------------------------------------------------------------------------------

# 1. MSVC crash on the first WCLAP load: the arena was dereferenced and moved inside
#    one braced list. Sequencing the dereference first fixes it (found by pdwclap).
fanan_patch_source("${WCLAP_CPP_DIR}/include/wclap/memory-arena.hpp"
    "return {*arena, std::move(arena)};"
    "Arena &arenaRef = *arena;\n\t\treturn {arenaRef, std::move(arena)};")

# 2. Give the plugin a real host identity and CLAP version. Without it, plugins that
#    check the host version refuse to instantiate (wclap-bridge pull request #8).
fanan_patch_source("${WCLAP_BRIDGE_DIR}/source/_generic/wclap-plugin-factory.h"
    "auto hostPtr = scoped.copyAcross(module.hostTemplate);"
    "auto wclapHost = module.hostTemplate;\n\t\twclapHost.clap_version = WCLAP_VERSION_INIT;\n\t\twclapHost.name = scoped.writeString(host && host->name ? host->name : \"\");\n\t\twclapHost.vendor = scoped.writeString(host && host->vendor ? host->vendor : \"\");\n\t\twclapHost.url = scoped.writeString(host && host->url ? host->url : \"\");\n\t\twclapHost.version = scoped.writeString(host && host->version ? host->version : \"\");\n\t\tauto hostPtr = scoped.copyAcross(wclapHost);")

# 3. Wasmtime's compile cache (turns a 1 s plugin reload into ~20 ms) defaults to the
#    user profile. The app supplies the cache settings instead - a folder next to the
#    executable - through fanan_wasmtime_cache_config(), defined in WclapPlugin.cpp.
#    A null result turns the cache off.
fanan_patch_source("${WCLAP_BRIDGE_DIR}/source/wclap-instance-wasmtime/wclap-instance-wasmtime.cpp"
    "static std::atomic<bool> globalEpochRunning = false;"
    "extern \"C\" const char *fanan_wasmtime_cache_config(void); // provided by Fanan Web Hub\nstatic std::atomic<bool> globalEpochRunning = false;")

fanan_patch_source("${WCLAP_BRIDGE_DIR}/source/wclap-instance-wasmtime/wclap-instance-wasmtime.cpp"
    "auto error = wasmtime_config_cache_config_load(config, nullptr);"
    "const char *fananCacheConfig = fanan_wasmtime_cache_config();\n\tauto error = fananCacheConfig ? wasmtime_config_cache_config_load(config, fananCacheConfig) : nullptr;")

# ------------------------------------------------------------------------------
# webview-gui stand-in
# The bridge includes "webview-gui/clap-webview-gui.h" and "webview-gui/helpers.h".
# The Hub's stand-ins sit flat in src; this copies them into that folder layout
# inside the build folder (and again whenever they change).
# ------------------------------------------------------------------------------
set(FANAN_WEBVIEW_STUB_DIR "${FANAN_DEPS_DIR}/webview-gui-stub")

configure_file("${CMAKE_CURRENT_LIST_DIR}/clap-webview-gui.h"
               "${FANAN_WEBVIEW_STUB_DIR}/webview-gui/clap-webview-gui.h" COPYONLY)

configure_file("${CMAKE_CURRENT_LIST_DIR}/helpers.h"
               "${FANAN_WEBVIEW_STUB_DIR}/webview-gui/helpers.h" COPYONLY)

# ------------------------------------------------------------------------------
# Library
# ------------------------------------------------------------------------------
add_library(FananWclapBridge STATIC
    "${WCLAP_BRIDGE_DIR}/source/wclap-bridge.cpp"
    "${WCLAP_BRIDGE_DIR}/source/wclap-instance-wasmtime/wclap-instance-wasmtime.cpp")

target_compile_features(FananWclapBridge PUBLIC cxx_std_20)

target_include_directories(FananWclapBridge
    PUBLIC
        "${WCLAP_BRIDGE_DIR}/include"
        "${FANAN_CLAP_DIR}/include"
    PRIVATE
        "${WCLAP_CPP_DIR}/include"
        "${WASMTIME_DIR}/include"
        # Stand-in for the webview-gui module: the Hub shows plugin UIs in its own web view.
        "${FANAN_WEBVIEW_STUB_DIR}")

if(MSVC)
    target_compile_options(FananWclapBridge PRIVATE /bigobj)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(FananWclapBridge PRIVATE -Wno-changes-meaning)
endif()

if(WIN32)
    # Linked as a DLL: the static wasmtime.lib is a known source of runtime crashes on Windows.
    target_link_libraries(FananWclapBridge PUBLIC "${WASMTIME_DIR}/lib/wasmtime.dll.lib")
    set(FANAN_WASMTIME_DLL "${WASMTIME_DIR}/lib/wasmtime.dll")
elseif(APPLE)
    target_link_libraries(FananWclapBridge PUBLIC "${WASMTIME_DIR}/lib/libwasmtime.a")
else()
    find_package(Threads REQUIRED)
    target_link_libraries(FananWclapBridge PUBLIC "${WASMTIME_DIR}/lib/libwasmtime.a" Threads::Threads ${CMAKE_DL_LIBS} m)
endif()
