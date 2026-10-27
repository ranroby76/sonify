# C:\workspace\SONIFY PLUGIN zax\CMakeLists.txt
#
# Zax - Sonify's GM drum synth (Fanan).  JUCE 9: VST3 + Standalone (+ AU on a Mac),
# CLAP through clap-juce-extensions when it is found.  Everything is built into
# the build folder only: nothing is copied anywhere else.

cmake_minimum_required(VERSION 3.22)
project(Zax VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "" FORCE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

# ---- JUCE -------------------------------------------------------------------
set(JUCE_DIR "C:/workspace/JUCE" CACHE PATH "JUCE 9 folder")
if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at ${JUCE_DIR} - pass -DJUCE_DIR=<your JUCE folder>")
endif()
add_subdirectory("${JUCE_DIR}" JUCE)

# ---- CLAP (optional) --------------------------------------------------------
set(CLAP_EXT_DIR "" CACHE PATH "clap-juce-extensions folder (empty = search)")
if(NOT CLAP_EXT_DIR)
    foreach(candidate
            "C:/workspace/libs/CLAP_Extensions"
            "C:/workspace/libs/clap-juce-extensions"
            "C:/workspace/clap-juce-extensions-main"
            "${CMAKE_CURRENT_SOURCE_DIR}/../clap-juce-extensions")
        if(EXISTS "${candidate}/CMakeLists.txt")
            set(CLAP_EXT_DIR "${candidate}")
            break()
        endif()
    endforeach()
endif()
if(CLAP_EXT_DIR AND EXISTS "${CLAP_EXT_DIR}/CMakeLists.txt")
    add_subdirectory("${CLAP_EXT_DIR}" clap-juce-extensions EXCLUDE_FROM_ALL)
    set(ZAX_WITH_CLAP ON)
    message(STATUS "Zax: CLAP from ${CLAP_EXT_DIR}")
else()
    set(ZAX_WITH_CLAP OFF)
    message(STATUS "Zax: clap-juce-extensions not found - building without CLAP")
endif()

# ---- The plugin -------------------------------------------------------------
set(ZAX_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND ZAX_FORMATS AU)
endif()

juce_add_plugin(Zax
    COMPANY_NAME                "Fanan"
    COMPANY_WEBSITE             "https://fanan.com"
    BUNDLE_ID                   "com.fanan.zax"
    PLUGIN_MANUFACTURER_CODE    Fana
    PLUGIN_CODE                 Zaxd
    PRODUCT_NAME                "Zax"
    DESCRIPTION                 "GM drum synth"
    FORMATS                     ${ZAX_FORMATS}
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Drum
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice)

juce_generate_juce_header(Zax)

target_sources(Zax PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp)

target_include_directories(Zax PRIVATE src)

target_compile_definitions(Zax PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(WIN32)
    target_compile_definitions(Zax PUBLIC JUCE_ASIO=1)   # the Standalone: JUCE's bundled ASIO
endif()

if(MSVC)
    target_compile_options(Zax PRIVATE /utf-8 /bigobj)
endif()

target_link_libraries(Zax
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(ZAX_WITH_CLAP)
    clap_juce_extensions_plugin(TARGET Zax
        CLAP_ID       "com.fanan.zax"
        CLAP_FEATURES instrument drum-machine stereo)
endif()
