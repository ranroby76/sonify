# ============================================================================
#  AXIS — Sonify guitar pedal family
#  JUCE 9 · VST3 / CLAP / Standalone · WebView UI
#
#  Project root : C:\workspace\SONIFY PLUGIN axis   <- this file lives here
#  Sources      : C:\workspace\SONIFY PLUGIN axis\src
#  JUCE 9       : C:\workspace\JUCE
#
#  The root path contains spaces, so keep every path quoted:
#     cd "C:\workspace\SONIFY PLUGIN axis"
#     cmake -B build -G "Visual Studio 17 2022" -A x64
#     cmake --build build --config Release
#
#  Everything is written inside .\build — nothing is copied anywhere else.
# ============================================================================
cmake_minimum_required(VERSION 3.22)

project(AXIS VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- paths to the SDKs already on this machine -----------------------------
set(AXIS_JUCE_PATH "C:/workspace/JUCE"
    CACHE PATH "Path to the JUCE 9 source tree")
set(AXIS_CLAP_EXT_PATH "C:/workspace/clap-juce-extensions-main"
    CACHE PATH "Path to clap-juce-extensions")

if(NOT EXISTS "${AXIS_JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE not found at ${AXIS_JUCE_PATH}. "
        "Re-run cmake with -DAXIS_JUCE_PATH=<path to JUCE>")
endif()

add_subdirectory("${AXIS_JUCE_PATH}" JUCE EXCLUDE_FROM_ALL)

set(AXIS_FORMATS VST3 Standalone)

# ============================================================================
#  WebView2 SDK (Windows only)
#
#  JUCE's WebBrowserComponent needs Microsoft's WebView2 headers to compile,
#  and they do not ship with JUCE or with Visual Studio. The SDK is a NuGet
#  package, so it is fetched here at configure time and unpacked into the
#  build folder.
#
#  It is downloaded once; later configures reuse what is already unpacked.
#  To keep it across clean builds, point AXIS_WEBVIEW2_DIR somewhere outside
#  the build folder.
# ============================================================================
set(AXIS_WEBVIEW2_VERSION "1.0.2792.45"
    CACHE STRING "Microsoft.Web.WebView2 NuGet version to fetch")
set(AXIS_WEBVIEW2_DIR "${CMAKE_BINARY_DIR}/_deps/webview2"
    CACHE PATH "Where the WebView2 SDK is unpacked")

set(AXIS_HAVE_WEBVIEW2 FALSE)

if(WIN32)
    set(_wv2_header "${AXIS_WEBVIEW2_DIR}/build/native/include/WebView2.h")

    if(NOT EXISTS "${_wv2_header}")
        set(_wv2_zip "${CMAKE_BINARY_DIR}/webview2.zip")

        set(_wv2_urls
            "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${AXIS_WEBVIEW2_VERSION}"
            "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2")

        foreach(_url IN LISTS _wv2_urls)
            message(STATUS "AXIS: downloading WebView2 SDK from ${_url}")

            file(DOWNLOAD "${_url}" "${_wv2_zip}"
                 STATUS _dl_status
                 TLS_VERIFY ON
                 SHOW_PROGRESS)

            list(GET _dl_status 0 _dl_code)

            if(_dl_code EQUAL 0)
                break()
            endif()

            list(GET _dl_status 1 _dl_msg)
            message(STATUS "AXIS: download failed (${_dl_msg})")
        endforeach()

        if(EXISTS "${_wv2_zip}")
            file(MAKE_DIRECTORY "${AXIS_WEBVIEW2_DIR}")
            file(ARCHIVE_EXTRACT INPUT "${_wv2_zip}"
                 DESTINATION "${AXIS_WEBVIEW2_DIR}")
            file(REMOVE "${_wv2_zip}")
        endif()
    endif()

    if(EXISTS "${_wv2_header}")
        set(AXIS_HAVE_WEBVIEW2 TRUE)
        message(STATUS "AXIS: WebView2 SDK ready at ${AXIS_WEBVIEW2_DIR}")
    else()
        message(WARNING
            "AXIS: the WebView2 SDK could not be fetched.\n"
            "The plugin will build, but the UI will fall back to the legacy "
            "Windows browser control and will not render correctly.\n"
            "Fix: download Microsoft.Web.WebView2 from nuget.org, unpack it "
            "(a .nupkg is a .zip), and re-run cmake with\n"
            "  -DAXIS_WEBVIEW2_DIR=<folder containing build/native/include>")
    endif()
endif()

# ---- the plugin target -----------------------------------------------------
juce_add_plugin(AXIS
    COMPANY_NAME                "Fanan"
    BUNDLE_ID                   site.fananteam.axis
    PLUGIN_MANUFACTURER_CODE    Fnan
    PLUGIN_CODE                 Axis
    PRODUCT_NAME                "AXIS"
    DESCRIPTION                 "Modular guitar pedal family"

    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    NEEDS_WEB_BROWSER           TRUE

    COPY_PLUGIN_AFTER_BUILD     FALSE      # build folder only — never install

    FORMATS                     ${AXIS_FORMATS})

# ---- the web UI, embedded as binary data -----------------------------------
file(GLOB_RECURSE AXIS_UI_FILES
     "${CMAKE_CURRENT_SOURCE_DIR}/src/Resources/ui/*")

juce_add_binary_data(AXIS_WebUI
    HEADER_NAME "AxisWebUI.h"
    NAMESPACE   AxisWebUI
    SOURCES     ${AXIS_UI_FILES})

set_target_properties(AXIS_WebUI PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# ---- sources ---------------------------------------------------------------
target_sources(AXIS PRIVATE
    src/Source/PluginProcessor.cpp
    src/Source/PluginEditor.cpp)

target_include_directories(AXIS PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Source")

target_compile_definitions(AXIS PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

# ---- wire the WebView2 SDK in --------------------------------------------
if(AXIS_HAVE_WEBVIEW2)
    target_compile_definitions(AXIS PUBLIC
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)

    target_include_directories(AXIS PUBLIC
        "${AXIS_WEBVIEW2_DIR}/build/native/include")

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_wv2_arch x64)
    else()
        set(_wv2_arch x86)
    endif()

    set(_wv2_lib "${AXIS_WEBVIEW2_DIR}/build/native/${_wv2_arch}/WebView2LoaderStatic.lib")

    if(EXISTS "${_wv2_lib}")
        target_link_libraries(AXIS PRIVATE "${_wv2_lib}")
    else()
        message(WARNING "AXIS: WebView2LoaderStatic.lib not found at ${_wv2_lib}")
    endif()
endif()

target_link_libraries(AXIS PRIVATE
    AXIS_WebUI
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# ---- CLAP ------------------------------------------------------------------
if(EXISTS "${AXIS_CLAP_EXT_PATH}/CMakeLists.txt")
    add_subdirectory("${AXIS_CLAP_EXT_PATH}" clap-juce-extensions EXCLUDE_FROM_ALL)
    clap_juce_extensions_plugin(TARGET AXIS
        CLAP_ID "site.fananteam.axis"
        CLAP_FEATURES audio-effect distortion "guitar")
    message(STATUS "AXIS: CLAP target enabled")
else()
    message(STATUS "AXIS: clap-juce-extensions not found — CLAP target skipped")
endif()
