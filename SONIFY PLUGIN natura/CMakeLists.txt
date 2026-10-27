# C:\workspace\SONIFY PLUGIN natura\CMakeLists.txt
# =============================================================================
# NATURA - Sonify modeling synth (13 archetypes, articulations, Grex synthesis)
#
# Built against JUCE 9.  All sources live flat in src\.
# Every build product stays inside this project's build folder: no copy or
# install step to the system plugin folders.
#
# CLAP: JUCE 9.0.1 has no CLAP of its own, so it comes from
# clap-juce-extensions when that folder is found (it is optional - without it
# the VST3 and the Standalone still build).
# =============================================================================
cmake_minimum_required(VERSION 3.22)
project(Natura VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- JUCE -------------------------------------------------------------------
if(NOT DEFINED JUCE_DIR OR "${JUCE_DIR}" STREQUAL "")
    if(DEFINED JUCE_PATH AND EXISTS "${JUCE_PATH}/CMakeLists.txt")
        set(JUCE_DIR "${JUCE_PATH}" CACHE PATH "Path to JUCE")
    elseif(EXISTS "C:/workspace/JUCE/CMakeLists.txt")
        set(JUCE_DIR "C:/workspace/JUCE" CACHE PATH "Path to JUCE")
    elseif(EXISTS "D:/workspace/JUCE/CMakeLists.txt")
        set(JUCE_DIR "D:/workspace/JUCE" CACHE PATH "Path to JUCE")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
        set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE" CACHE PATH "Path to JUCE")
    endif()
endif()
if(NOT JUCE_DIR OR NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found. Pass -DJUCE_DIR=C:/workspace/JUCE")
endif()

# ---- clap-juce-extensions (optional) ----------------------------------------
if(NOT DEFINED CLAP_EXTENSIONS_DIR OR "${CLAP_EXTENSIONS_DIR}" STREQUAL "")
    foreach(cand
            "${CLAP_EXTENSIONS_PATH}"
            "C:/workspace/clap-juce-extensions-main"
            "C:/workspace/libs/CLAP_Extensions"
            "D:/workspace/clap-juce-extensions-main"
            "${CMAKE_CURRENT_SOURCE_DIR}/clap-juce-extensions")
        if(cand AND EXISTS "${cand}/CMakeLists.txt")
            set(CLAP_EXTENSIONS_DIR "${cand}" CACHE PATH "Path to clap-juce-extensions")
            break()
        endif()
    endforeach()
endif()
set(NATURA_HAVE_CLAP FALSE)
if(CLAP_EXTENSIONS_DIR AND EXISTS "${CLAP_EXTENSIONS_DIR}/CMakeLists.txt")
    set(NATURA_HAVE_CLAP TRUE)
endif()

set(CMAKE_UNITY_BUILD OFF)
add_subdirectory("${JUCE_DIR}" JUCE)

if(NATURA_HAVE_CLAP)
    set(CLAP_JUCE_EXTENSIONS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CLAP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CLAP_HELPERS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${CLAP_EXTENSIONS_DIR}" clap-juce-extensions EXCLUDE_FROM_ALL)
    message(STATUS "Natura: CLAP from ${CLAP_EXTENSIONS_DIR}")
else()
    message(STATUS "Natura: clap-juce-extensions not found - building VST3 + Standalone only")
endif()

# ---- the plugin ---------------------------------------------------------------
juce_add_plugin(Natura
    COMPANY_NAME "Fanan"
    BUNDLE_ID com.fanan.natura
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_AUTO_MANIFEST FALSE
    PLUGIN_MANUFACTURER_CODE Fnan
    PLUGIN_CODE Ntra
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    HARDENED_RUNTIME_ENABLED TRUE
    FORMATS VST3 Standalone
    PRODUCT_NAME "Natura"
)

if(NATURA_HAVE_CLAP)
    clap_juce_extensions_plugin(TARGET Natura
        CLAP_ID "com.fanan.natura"
        CLAP_FEATURES instrument synthesizer
    )
endif()

juce_generate_juce_header(Natura)

target_sources(Natura PRIVATE
    src/ModelDsp.h
    src/ModelBase.h
    src/OscEngine.h
    src/ModelsKeys.h
    src/ModelsOrgan.h
    src/ModelsStrings.h
    src/ModelsWind.h
    src/Models.h
    src/Articulations.h
    src/Params.h
    src/Archetypes.h
    src/SynthCommon.h
    src/NaturaVoice.h
    src/FxChain.h
    src/NaturaEngine.h
    src/NaturaEngine.cpp
    src/PresetManager.h
    src/Theme.h
    src/FananLookAndFeel.h
    src/FananLookAndFeel.cpp
    src/GrexWidgets.h
    src/NaturaPanels.h
    src/PluginProcessor.h
    src/PluginProcessor.cpp
    src/PluginEditor.h
    src/PluginEditor.cpp
)

target_compile_definitions(Natura PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

if(MSVC)
    target_compile_options(Natura PRIVATE /utf-8 /fp:fast)
else()
    target_compile_options(Natura PRIVATE -ffast-math)
endif()

target_link_libraries(Natura
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)
