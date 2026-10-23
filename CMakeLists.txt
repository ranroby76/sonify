# D:\workspace\Fanan_web_hub\CMakeLists.txt

cmake_minimum_required(VERSION 3.22)

project(FananWebHub VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------------------------
# JUCE 9
# ------------------------------------------------------------------------------
set(JUCE_DIR "D:/workspace/JUCE" CACHE PATH "JUCE 9 root folder")

if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at ${JUCE_DIR}")
endif()

# ------------------------------------------------------------------------------
# WebView2 SDK (Windows)
# JUCE links WebView2 statically and needs the Microsoft.Web.WebView2 NuGet
# package. A copy kept beside the project (nuget\ next to this file, copied there
# from a build folder) is used as it is; without one, the package is downloaded
# once into this build folder - nothing is installed anywhere else on the system.
# ------------------------------------------------------------------------------
if(WIN32)
    set(FANAN_WEBVIEW2_VERSION "1.0.3485.44")
    set(FANAN_WEBVIEW2_PACKAGES "${CMAKE_SOURCE_DIR}/nuget")

    if(EXISTS "${FANAN_WEBVIEW2_PACKAGES}/Microsoft.Web.WebView2.${FANAN_WEBVIEW2_VERSION}/build/native/include/WebView2.h")
        message(STATUS "Using the WebView2 SDK kept in ${FANAN_WEBVIEW2_PACKAGES}")
    else()
        set(FANAN_WEBVIEW2_PACKAGES "${CMAKE_BINARY_DIR}/nuget")
    endif()

    set(FANAN_WEBVIEW2_DIR "${FANAN_WEBVIEW2_PACKAGES}/Microsoft.Web.WebView2.${FANAN_WEBVIEW2_VERSION}")

    if(NOT EXISTS "${FANAN_WEBVIEW2_DIR}/build/native/include/WebView2.h")
        set(_webview2_archive "${CMAKE_BINARY_DIR}/nuget-download/webview2-${FANAN_WEBVIEW2_VERSION}.zip")

        message(STATUS "Downloading WebView2 SDK ${FANAN_WEBVIEW2_VERSION}")

        file(DOWNLOAD
            "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/${FANAN_WEBVIEW2_VERSION}/microsoft.web.webview2.${FANAN_WEBVIEW2_VERSION}.nupkg"
            "${_webview2_archive}"
            STATUS _webview2_status
            TLS_VERIFY ON)

        list(GET _webview2_status 0 _webview2_code)

        if(NOT _webview2_code EQUAL 0)
            file(REMOVE "${_webview2_archive}")
            message(FATAL_ERROR "WebView2 SDK download failed: ${_webview2_status}")
        endif()

        file(ARCHIVE_EXTRACT INPUT "${_webview2_archive}" DESTINATION "${FANAN_WEBVIEW2_DIR}")
    endif()

    set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${FANAN_WEBVIEW2_PACKAGES}")
endif()

add_subdirectory("${JUCE_DIR}" JUCE)

# ------------------------------------------------------------------------------
# WCLAP hosting: wclap-bridge + Wasmtime, downloaded once into build/deps.
# Brings its own CLAP 1.2.7 headers, so no separate CLAP folder is needed.
# ------------------------------------------------------------------------------
include(src/WclapBridge.cmake)

# ------------------------------------------------------------------------------
# Split into Stems: ONNX Runtime (Windows), downloaded once into build/deps.
# Without it Sonify still builds, and says stems can't be split.
# ------------------------------------------------------------------------------
include(src/OnnxRuntime.cmake)

# ------------------------------------------------------------------------------
# MP3 export: LAME, built from its source into build/deps as mp3lame.dll (LGPL: its own library).
# Without it Sonify still builds, and says MP3 export needs it.
# ------------------------------------------------------------------------------
include(src/Lame.cmake)

# ------------------------------------------------------------------------------
# Audio > Sync Tempo, Follow Tempo, Pitch: Signalsmith Stretch (MIT, headers only), and
# Audio to MIDI: Spotify's Basic Pitch model (Apache 2.0), both downloaded once into build/deps.
# ------------------------------------------------------------------------------
include(src/Stretch.cmake)
include(src/BasicPitch.cmake)

# ------------------------------------------------------------------------------
# MIDI tools: magenta.js (Google's Magenta, Apache 2.0) for the Magenta tools, downloaded once
# into build/deps; their models come later, from the MIDI tab's catalog.
# ------------------------------------------------------------------------------
include(src/MagentaJs.cmake)

# ------------------------------------------------------------------------------
# Video: VLC 3.0.21's runtime (libvlc and its plugins), downloaded once into build/deps (or the
# VLC folder named with -DFANAN_VLC_DIR=...) and copied next to Sonify.exe. Sonify loads it
# itself, so it builds without it, and says video needs VLC.
# ------------------------------------------------------------------------------
include(src/Vlc.cmake)

# ------------------------------------------------------------------------------
# Live streaming (the Live Show's cameras and stream), and the sound of the Live Show's videos read
# straight from them (VideoSound.cpp): FFmpeg 7.1, LGPL, shared, a fixed build downloaded once into
# build/deps (or the build named with -DFANAN_FFMPEG_DIR=...); its DLLs are linked late and copied
# next to Sonify.exe. On Windows configuring stops, saying why, when it can't be had.
# ------------------------------------------------------------------------------
include(src/FFmpeg.cmake)

# ------------------------------------------------------------------------------
# Sonify devices (the Device Studio): wasi-sdk's clang and WebAssembly libraries, downloaded once
# into build/deps and copied next to Sonify.exe as "Device Compiler", so devices are compiled on
# the user's computer. Devices run in the Wasmtime that WCLAP hosting brings.
# ------------------------------------------------------------------------------
include(src/DeviceCompiler.cmake)

# ------------------------------------------------------------------------------
# Export Plugin (the Plugins Creator Studio): VST3 3.8.1's pluginterfaces downloaded once into
# build/deps, and CLAP's headers (from WCLAP hosting), copied next to Sonify.exe as "Plugin SDKs";
# on Windows also llvm-mingw's 64-bit part as "Plugin Compiler", so plugin files are built on the
# user's computer (a Mac uses Apple's compiler, Linux the system's).
# ------------------------------------------------------------------------------
include(src/PluginCompiler.cmake)

# ------------------------------------------------------------------------------
# App
# ------------------------------------------------------------------------------
# The Sampling Studio's nodes load Audio Units straight from their bundles on a Mac (JUCE links
# AudioUnit and CoreAudioKit for it).
if(APPLE)
    set(FANAN_PLUGINHOST_AU TRUE)
else()
    set(FANAN_PLUGINHOST_AU FALSE)
endif()

juce_add_gui_app(FananWebHub
    PRODUCT_NAME "Sonify"
    COMPANY_NAME "Fanan"
    BUNDLE_ID "com.fanan.webhub"
    VERSION "${PROJECT_VERSION}"
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    PLUGINHOST_AU ${FANAN_PLUGINHOST_AU}
    # macOS asks before Sonify hears a microphone or sees a camera, with these words (other
    # platforms don't use them).
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Sonify records and plays through your audio inputs."
    CAMERA_PERMISSION_ENABLED TRUE
    CAMERA_PERMISSION_TEXT "Sonify shows your cameras in the Live Show Studio's streams."
    # Sonify's icon: Sonify.exe's (JUCE makes the .ico, and a Mac's .icns, from it), which the
    # window's title bar and the taskbar show too.
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/sonify_icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/sonify_icon.png")

target_sources(FananWebHub PRIVATE
    src/Main.cpp
    src/MainComponent.h
    src/MainComponent.cpp
    src/AudioEngine.h
    src/AudioEngine.cpp
    src/WclapPlugin.h
    src/WclapPlugin.cpp
    src/Transport.h
    src/Transport.cpp
    src/AudioRender.h
    src/AudioRender.cpp
    src/AudioToMidi.h
    src/AudioToMidi.cpp
    src/MainAudio.cpp
    src/TempoMap.h
    src/TempoMap.cpp
    src/Track.h
    src/Track.cpp
    src/Choir.h
    src/Choir.cpp
    src/MidiFx.h
    src/MidiFx.cpp
    src/Library.h
    src/Library.cpp
    src/EffectChain.h
    src/LatencyDelay.h
    src/EffectChain.cpp
    src/Sequence.h
    src/Sequence.cpp
    src/Song.h
    src/Song.cpp
    src/Edits.h
    src/Edits.cpp
    src/AudioMedia.h
    src/AudioMedia.cpp
    src/Plugin.h
    src/Plugin.cpp
    src/SonifyDsp.h
    src/BuiltinPlugins.h
    src/BuiltinPlugins.cpp
    src/BuiltinEffects.h
    src/BuiltinEffects.cpp
    src/BuiltinScreen.h
    src/BuiltinScreen.cpp
    src/GrexFx.h
    src/TestSynth.cpp
    src/Container.h
    src/Container.cpp
    src/ContainerCanvas.h
    src/ContainerCanvas.cpp
    src/ContainerCanvasMouse.cpp
    src/NodeTools.h
    src/NodeTools.cpp
    src/NodeRackPages.h
    src/NodeRackPages.cpp
    src/PlaylistPlayer.h
    src/PlaylistPlayer.cpp
    src/PlaylistScreen.h
    src/PlaylistScreen.cpp
    src/SonifySampler.h
    src/SonifySampler.cpp
    src/SamplerScreen.h
    src/SamplerScreen.cpp
    src/MasteringAssistant.h
    src/MasteringAssistant.cpp
    src/Mp3Encoder.h
    src/Mp3Encoder.cpp
    src/PodcastExport.h
    src/PodcastExport.cpp
    src/SessionPlayers.h
    src/SessionPlayers.cpp
    src/RecordingAnalysis.h
    src/RecordingAnalysis.cpp
    src/StemSplitter.h
    src/StemSplitter.cpp
    src/Vst3Plugin.h
    src/Vst3Plugin.cpp
    src/Cloud.h
    src/Cloud.cpp
    src/FananServer.h
    src/FananServer.cpp
    src/PluginScanner.h
    src/PluginScanner.cpp
    src/AiTools.h
    src/AiTools.cpp
    src/AiAgent.h
    src/AiAgent.cpp
    src/GuideAi.h
    src/GuideAi.cpp
    src/VoiceControl.h
    src/VoiceControl.cpp
    src/PhoneRemote.h
    src/PhoneRemote.cpp
    src/Preferences.h
    src/Preferences.cpp
    src/MidiSync.h
    src/MidiSync.cpp
    src/ControlSurface.h
    src/ControlSurface.cpp
    src/PadGrid.h
    src/PadGrid.cpp
    src/MidiLink.h
    src/MidiLink.cpp
    src/MainSettings.cpp
    src/MainLink.cpp
    src/MainMidiTools.cpp
    src/MainContainers.cpp
    src/MainAutomation.cpp
    src/MainTrackOptions.cpp
    src/SongBlocks.h
    src/SongBlocks.cpp
    src/MainBlocks.cpp
    src/VideoMedia.h
    src/VideoMedia.cpp
    src/VideoMp4.h
    src/VideoMp4.cpp
    src/VideoScreen.h
    src/VideoScreen.cpp
    src/VideoSound.h
    src/VideoSound.cpp
    src/MainVideo.cpp
    src/sonify_device.h
    src/DeviceHost.h
    src/DeviceHost.cpp
    src/DeviceFiles.h
    src/DeviceFiles.cpp
    src/DeviceStudio.h
    src/DeviceStudio.cpp
    src/MainDevices.cpp
    src/AnalogBoard.h
    src/AnalogBoard.cpp
    src/MainBoard.cpp
    src/MainGuide.cpp
    src/MainTracks.cpp
    src/MainScore.cpp
    src/MainImport.cpp
    src/DawProject.h
    src/DawProject.cpp
    src/MainDawProject.cpp
    src/LiveShow.h
    src/LiveShow.cpp
    src/LiveScreen.h
    src/LiveScreen.cpp
    src/LiveStream.h
    src/LiveStream.cpp
    src/LiveController.h
    src/LiveController.cpp
    src/MainLive.cpp
    src/MainLiveControl.cpp
    src/PageDropZone.h

    # The Sampling Studio: instruments sampled note by note into SFZs, and its Builder; see
    # MainSampling.cpp. What it makes plays in the Sonify Sampler (above).
    src/SfzFile.h
    src/SfzFile.cpp
    src/SamplingStudio.h
    src/SamplingStudio.cpp
    src/SamplingRack.h
    src/SamplingRack.cpp
    src/LoopFinder.h
    src/LoopFinder.cpp
    src/SfzLoops.h
    src/SfzLoops.cpp
    src/BlobMaker.h
    src/BlobMaker.cpp
    src/MainSampling.cpp
    src/MainBlobs.cpp

    # The Plugins Creator Studio: Stella AI Studio inside Sonify, see MainCreator.cpp; and the
    # plugins made there, on tracks: CreatorPlugin.cpp.
    src/MainCreator.cpp
    src/CreatorStudio.h
    src/CreatorStudio.cpp
    src/CreatorAudio.h
    src/CreatorAudio.cpp
    src/CreatorPlugin.h
    src/CreatorPlugin.cpp
    src/StellaLookAndFeel.h
    src/StellaLookAndFeel.cpp
    src/Settings.h
    src/Settings.cpp
    src/Project.h
    src/Project.cpp
    src/TopBar.h
    src/TopBar.cpp
    src/ChatPanel.h
    src/ChatPanel.cpp
    src/CodeReference.h
    src/CodeReference.cpp
    src/StellaAi.h
    src/StellaAi.cpp
    src/WasmPlugin.h
    src/WasmPlugin.cpp
    src/WasmElements.h
    src/WasmElements.cpp
    src/stella_element_host.h
    src/WasmCompiler.h
    src/WasmCompiler.cpp
    src/LivePreview.h
    src/LivePreview.cpp
    src/ProjectTools.h
    src/ProjectTools.cpp
    src/Skills.h
    src/Skills.cpp
    src/SoundCheck.h
    src/SoundCheck.cpp
    src/SoundMeasure.h
    src/SoundMeasure.cpp
    src/History.h
    src/History.cpp
    src/PresetBank.h
    src/PresetBank.cpp
    src/PresetLibrary.h
    src/PresetLibrary.cpp
    src/GuiLayout.h
    src/GuiLayout.cpp
    src/stella_keys.h
    src/PluginCanvas.h
    src/PluginCanvas.cpp
    src/PluginCanvasMenu.h
    src/PluginCanvas_Menu.cpp
    src/PluginCanvas_Mouse.cpp
    src/PluginCanvas_Elements.cpp
    src/PluginCanvas_Export.cpp
    src/PluginExporter.h
    src/PluginExporter.cpp
    src/PluginLibrary.h
    src/PluginLibrary.cpp
    src/stella_blob.h
    src/ObjectPaint.h
    src/ObjectPaint.cpp
    src/FontLibrary.h
    src/FontLibrary.cpp
    src/MeterPaint.h
    src/MeterPaint.cpp
    src/PanelParts.h
    src/PanelParts.cpp
    src/Ed25519.h
    src/Ed25519.cpp
    src/Licences.h
    src/Licences.cpp
    src/ProtectionPage.h
    src/ProtectionPage.cpp
    src/StoreClient.h
    src/StoreClient.cpp
    src/StorePage.h
    src/StorePage.cpp
    src/StorePublisher.h
    src/StorePublisher.cpp
    src/LayerStyleWindow.h
    src/LayerStyleWindow.cpp
    src/MaterialChoices.h
    src/Primitives.h
    src/Primitives.cpp
    src/Toolbox.h
    src/Toolbox.cpp
    src/ColourSwatch.h
    src/ColourSwatch.cpp
    src/UiHelpers.h
    src/Looks.h
    src/Looks.cpp
    src/LayerDoc.h
    src/LayerDoc.cpp
    src/LayerRender.h
    src/LayerRender.cpp
    src/LayerPanels.h
    src/LayerPanels.cpp
    src/LayerWorkspace.h
    src/LayerWorkspace.cpp
    src/BevelDialog.h
    src/BevelDialog.cpp
    src/TextureLibrary.h
    src/TextureLibrary.cpp
    src/BmpImage.h
    src/BmpImage.cpp
    src/KnobFile.h
    src/KnobFile.cpp
    src/KnobManEngine.h
    src/KnobManEngine.cpp
    src/GalleryBrowser.h
    src/GalleryBrowser.cpp
    src/ShapeDoc.h
    src/ShapeDoc.cpp
    src/ShapeEditor.h
    src/ShapeEditor.cpp
    src/ShapeWorkspace.h
    src/ShapeWorkspace.cpp
    src/KnobStudio.h
    src/KnobStudio.cpp
    src/Workspace.h
    src/Workspace.cpp)

# The web UI is compiled into the executable and served to the WebView from memory.
juce_add_binary_data(FananWebHubWeb SOURCES
    src/index.html
    src/hub.css
    src/hub-core.js
    src/hub-tempo.js
    src/hub-keys.js
    src/hub-group.js
    src/hub-transport.js
    src/hub-arrange.js
    src/hub-songchannel.js
    src/hub-audioeditor.js
    src/hub-tracks.js
    src/hub-trackoptions.js
    src/hub-panelsize.js
    src/hub-miditools.js
    src/hub-console.js
    src/hub-editor.js
    src/hub-quantize.js
    src/hub-choir.js
    src/hub-song.js
    src/hub-setup.js
    src/hub-screens.js
    src/hub-export.js
    src/hub-master.js
    src/hub-session.js
    src/hub-stems.js
    src/hub-aitools.js
    src/hub-studios.js
    src/hub-web.js
    src/hub-creator.js
    src/hub-cloud.js
    src/hub-content.js
    src/hub-scan.js
    src/hub-ai.js
    src/hub-browser.js
    src/hub-settings.js
    src/hub-link.js
    src/hub-blocks.js
    src/hub-video.js
    src/hub-devices.js
    src/hub-board.js
    src/hub-live.js
    src/hub-live.css
    src/hub-sampling.js
    src/hub-sampling.css
    src/hub-sampling-nodes.js
    src/hub-sampling-midi.js
    src/hub-sampling-blob.js
    src/hub-guide.js
    src/hub-guide.css
    src/sonify_guide.md
    src/hub-score.js
    src/hub-score.css
    src/opensheetmusicdisplay.min.js
    src/jspdf.umd.min.js
    src/svg2pdf.umd.min.js
    src/hub-import.js
    src/hub-plugins.js
    "assets/fanan logo.png"
    assets/Sonify.png
    assets/sonify_icon.png)

# Sonify AI's guides: how it works in the song and talks to the user, and how it builds devices
# in the Device Studio, sent with every request; and the API devices are written against.
juce_add_binary_data(SonifyAiData
    HEADER_NAME SonifyAiData.h
    NAMESPACE   SonifyAiData
    SOURCES
        src/sonify_ai_guide.md
        src/sonify_device_guide.md
        src/sonify_device.h)

# The Plugins Creator Studio's runtime: what each plugin made there is compiled with (by the
# device compiler, to WebAssembly), the programmed GUI elements' runtime with its font (Inter,
# SIL Open Font License: src/inter_license.txt), Sonify AI's builder guide, and what Export Plugin
# compiles natively around a plugin (its window, and its VST3, CLAP and Audio Unit sides).
juce_add_binary_data(StellaRuntimeData
    HEADER_NAME StellaRuntimeData.h
    NAMESPACE   StellaRuntimeData
    SOURCES
        src/stella_api.h
        src/stella_runtime.h
        src/stella_runtime.cpp
        src/stella_licence.h
        src/stella_blob.h
        src/stella_element_api.h
        src/stella_element_host.h
        src/stella_elements.h
        src/stella_elements.cpp
        src/stella_builtin_elements.cpp
        src/stella_view3d.h
        src/stella_builtin_3d.cpp
        src/stella_paint.h
        src/stella_builtin_paint.cpp
        src/stb_truetype.h
        src/inter_regular.ttf
        src/inter_bold.ttf
        src/stella_builder_guide.md
        src/stella_keys.h
        src/stella_gui.h
        src/stella_gui.cpp
        src/stella_plugin.h
        src/stb_image.h
        src/stella_clap.cpp
        src/stella_vst3.cpp
        src/stella_au.cpp
        src/stella_gui_win32.cpp
        src/stella_gui_mac.mm
        src/stella_gui_x11.cpp)

# Its Edit UI toolbox's primitives (src/primitive_*.json) and its Knob Studio's built-in looks
# (src/look_*.fklayers): a new file there shows after the next build.
file(GLOB FANAN_CREATOR_PRIMITIVES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/primitive_*.json")
file(GLOB FANAN_CREATOR_LOOKS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/look_*.fklayers")

juce_add_binary_data(StellaPrimitives
    HEADER_NAME StellaPrimitives.h
    NAMESPACE   StellaPrimitives
    SOURCES     ${FANAN_CREATOR_PRIMITIVES})

juce_add_binary_data(StellaLooks
    HEADER_NAME StellaLooks.h
    NAMESPACE   StellaLooks
    SOURCES     ${FANAN_CREATOR_LOOKS})

# Fonts for plugin panels' text: the fonts folder beside src (fonts/*.ttf, each SIL Open Font
# License: fonts/fonts_license.txt, built in with them). FontLibrary lists them by file name; a new
# file there shows after the next build.
file(GLOB FANAN_PANEL_FONTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/fonts/*.ttf")
set(FANAN_FONTS_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/fonts/fonts_license.txt")

if(NOT FANAN_PANEL_FONTS OR NOT EXISTS "${FANAN_FONTS_LICENSE}")
    message(FATAL_ERROR "The panel fonts are missing: put the fonts folder from the zip (30 .ttf files and "
                        "fonts_license.txt) at ${CMAKE_CURRENT_SOURCE_DIR}/fonts")
endif()

juce_add_binary_data(SonifyFonts
    HEADER_NAME SonifyFonts.h
    NAMESPACE   SonifyFonts
    SOURCES     ${FANAN_PANEL_FONTS} "${FANAN_FONTS_LICENSE}")

# The built-in effects' and the Test Synth's panels (src/panel_*.json, BuiltinScreen.cpp) and their
# factory presets (src/presets_*.json): a new file shows after the next build.
file(GLOB SONIFY_PANELS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/panel_*.json"
                                          "${CMAKE_CURRENT_SOURCE_DIR}/src/presets_*.json")

juce_add_binary_data(SonifyPanels
    HEADER_NAME SonifyPanels.h
    NAMESPACE   SonifyPanels
    SOURCES     ${SONIFY_PANELS})

# Fanan AI's skills (src/skill_*.md): know-how it reads when a job needs it (Skills.h). A new
# file shows after the next build.
file(GLOB FANAN_SKILLS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/skill_*.md")

juce_add_binary_data(FananSkills
    HEADER_NAME FananSkills.h
    NAMESPACE   FananSkills
    SOURCES     ${FANAN_SKILLS})

# Audio to MIDI's note model, built in when it was downloaded.
if(FANAN_BASIC_PITCH_MODEL)
    juce_add_binary_data(SonifyModelData
        HEADER_NAME SonifyModelData.h
        NAMESPACE   SonifyModelData
        SOURCES
            "${FANAN_BASIC_PITCH_MODEL}")
    target_link_libraries(FananWebHub PRIVATE SonifyModelData)
endif()

# magenta.js for the Magenta MIDI tools, built in when it was downloaded.
if(FANAN_MAGENTA_JS)
    juce_add_binary_data(SonifyMagentaData
        HEADER_NAME SonifyMagentaData.h
        NAMESPACE   SonifyMagentaData
        SOURCES
            "${FANAN_MAGENTA_JS}")
    target_link_libraries(FananWebHub PRIVATE SonifyMagentaData)
endif()

# Libraries from elsewhere (the stretcher, FFmpeg, Wasmtime, ONNX Runtime) are included as
# system headers: their own warnings stay out of the build's. MSVC: /external:W0 for them.
if(MSVC)
    target_compile_options(FananWebHub PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:/external:W0>)
endif()

# The time and pitch stretcher's headers; MSVC needs /bigobj for all its templates.
target_include_directories(FananWebHub SYSTEM PRIVATE ${FANAN_STRETCH_INCLUDE_DIRS})

# FFmpeg for live streaming and videos' sound: on Windows its DLLs are loaded at the first call
# (delay-loaded), so Sonify starts without them (LiveStream.cpp looks for them first, and says
# what's missing; VideoSound.cpp asks it before its own first call).
if(FANAN_FFMPEG_FOUND)
    target_compile_definitions(FananWebHub PRIVATE FANAN_FFMPEG=1)

    if(FANAN_FFMPEG_TARGET)
        target_link_libraries(FananWebHub PRIVATE ${FANAN_FFMPEG_TARGET})
    else()
        target_include_directories(FananWebHub SYSTEM PRIVATE "${FANAN_FFMPEG_INCLUDE_DIR}")
        target_link_libraries(FananWebHub PRIVATE ${FANAN_FFMPEG_LIBRARIES})

        if(MSVC)
            target_link_libraries(FananWebHub PRIVATE delayimp)

            foreach(_ffmpeg_dll IN LISTS FANAN_FFMPEG_DLLS)
                get_filename_component(_ffmpeg_dll_name "${_ffmpeg_dll}" NAME)

                # Only the ones Sonify links (avfilter's comes with avdevice's).
                if(_ffmpeg_dll_name MATCHES "^(avdevice|avformat|avcodec|swscale|swresample|avutil)-[0-9]+\\.dll$")
                    target_link_options(FananWebHub PRIVATE "/DELAYLOAD:${_ffmpeg_dll_name}")
                endif()
            endforeach()
        endif()
    endif()
endif()

# Sonify devices run in Wasmtime (DeviceHost.cpp), the same one WCLAP hosting links; so do the
# plugins made in the Plugins Creator Studio (WasmPlugin.cpp, WasmElements.cpp).
target_include_directories(FananWebHub SYSTEM PRIVATE "${WASMTIME_DIR}/include")

if(MSVC)
    set_source_files_properties(src/AudioRender.cpp PROPERTIES COMPILE_OPTIONS "/bigobj")
endif()

target_compile_definitions(FananWebHub PRIVATE
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:FananWebHub,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:FananWebHub,JUCE_VERSION>"
    # Export Plugin's tools where they are while developing (PluginExporter looks beside Sonify first).
    FANAN_PLUGIN_CLAP_DIR="${FANAN_PLUGIN_CLAP_DIR}"
    FANAN_PLUGIN_VST3_DIR="${FANAN_PLUGIN_VST3_DIR}")

if(FANAN_PLUGIN_COMPILER_DIR)
    target_compile_definitions(FananWebHub PRIVATE FANAN_PLUGIN_COMPILER_DIR="${FANAN_PLUGIN_COMPILER_DIR}")
endif()

# VST2 plugins in the Sampling Studio's nodes: only with Steinberg's VST2 SDK at hand (it can't be
# shipped with Sonify). Point FANAN_VST2_SDK at the folder holding pluginterfaces/vst2.x.
set(FANAN_VST2_SDK "D:/workspace/vst2sdk" CACHE PATH "Steinberg's VST2 SDK (the folder with pluginterfaces/vst2.x), for hosting VST2 plugins")

if(EXISTS "${FANAN_VST2_SDK}/pluginterfaces/vst2.x/aeffectx.h")
    message(STATUS "Sonify: VST2 SDK found in ${FANAN_VST2_SDK}: VST2 plugins load in the Sampling Studio")
    target_include_directories(FananWebHub SYSTEM PRIVATE "${FANAN_VST2_SDK}")
    target_compile_definitions(FananWebHub PRIVATE JUCE_PLUGINHOST_VST=1)
else()
    message(STATUS "Sonify: no VST2 SDK in ${FANAN_VST2_SDK}: VST2 plugins can't be loaded (the button says so)")
endif()

# LV2 plugins, on Linux.
if(UNIX AND NOT APPLE)
    target_compile_definitions(FananWebHub PRIVATE JUCE_PLUGINHOST_LV2=1)
endif()

if(WIN32)
    # JUCE 9 ships its own ASIO headers, so no external ASIO SDK path is needed.
    target_compile_definitions(FananWebHub PRIVATE
        JUCE_ASIO=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)
endif()

target_link_libraries(FananWebHub
    PRIVATE
        FananWebHubWeb
        SonifyAiData
        StellaRuntimeData
        StellaPrimitives
        StellaLooks
        FananSkills
        SonifyFonts
        SonifyPanels
        FananWclapBridge
        juce::juce_audio_utils
        juce::juce_audio_processors_headless
        juce::juce_audio_devices
        juce::juce_audio_basics
        juce::juce_gui_extra
        juce::juce_cryptography
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(WIN32)
    # Wasmtime is a DLL on Windows: copy it next to Sonify.exe, inside the build folder.
    add_custom_command(TARGET FananWebHub POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${FANAN_WASMTIME_DLL}" "$<TARGET_FILE_DIR:FananWebHub>"
        VERBATIM)
endif()

if(FANAN_ONNXRUNTIME_INCLUDE_DIR)
    # Only its header to build; Sonify loads onnxruntime.dll itself, copied next to Sonify.exe.
    target_include_directories(FananWebHub SYSTEM PRIVATE "${FANAN_ONNXRUNTIME_INCLUDE_DIR}")
    add_custom_command(TARGET FananWebHub POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${FANAN_ONNXRUNTIME_DLLS} "$<TARGET_FILE_DIR:FananWebHub>"
        VERBATIM)
endif()

if(TARGET fanan_mp3lame)
    # Sonify loads LAME itself: built with Sonify, then copied next to Sonify.exe.
    add_dependencies(FananWebHub fanan_mp3lame)
    add_custom_command(TARGET FananWebHub POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:fanan_mp3lame>" "$<TARGET_FILE_DIR:FananWebHub>"
        VERBATIM)
endif()

# The device compiler next to Sonify.exe, inside the build folder.
fanan_copy_device_compiler(FananWebHub)

# Export Plugin's headers (and on Windows its compiler) next to Sonify.exe, inside the build folder.
fanan_copy_plugin_tools(FananWebHub)

if(FANAN_FFMPEG_DLLS)
    # FFmpeg's DLLs (and its licence, as the LGPL asks) next to Sonify.exe.
    add_custom_command(TARGET FananWebHub POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${FANAN_FFMPEG_DLLS} "$<TARGET_FILE_DIR:FananWebHub>"
        VERBATIM)

    if(FANAN_FFMPEG_LICENSE)
        add_custom_command(TARGET FananWebHub POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${FANAN_FFMPEG_LICENSE}" "$<TARGET_FILE_DIR:FananWebHub>/FFmpeg-LICENSE.txt"
            VERBATIM)
    endif()
endif()

if(FANAN_VLC_RUNTIME_DIR)
    # Sonify loads libvlc itself: libvlc.dll, libvlccore.dll and the plugins folder next to
    # Sonify.exe (copied again only when changed, where CMake can tell).
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.26)
        set(_fanan_copy_folder copy_directory_if_different)
    else()
        set(_fanan_copy_folder copy_directory)
    endif()

    add_custom_command(TARGET FananWebHub POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${FANAN_VLC_RUNTIME_DIR}/libvlc.dll" "${FANAN_VLC_RUNTIME_DIR}/libvlccore.dll"
                "$<TARGET_FILE_DIR:FananWebHub>"
        COMMAND "${CMAKE_COMMAND}" -E ${_fanan_copy_folder} "${FANAN_VLC_RUNTIME_DIR}/plugins" "$<TARGET_FILE_DIR:FananWebHub>/plugins"
        VERBATIM)

    # VLC's list of its plugins (plugins\plugins.dat), as VLC's installer makes it: without it each
    # start of VLC looks into every plugin first (seconds; far longer under some virus scanners), and
    # the first video waits for that. VLC's own vlc-cache-gen makes it, copied next to Sonify.exe and
    # run there, so it looks only at the plugins there and writes only there (inside the build
    # folder). Made again only when the plugins change (put back when the copy above replaces it);
    # a failure leaves VLC slower to start, never the build.
    if(EXISTS "${FANAN_VLC_RUNTIME_DIR}/vlc-cache-gen.exe")
        set(_fanan_vlc_cache_script "${CMAKE_BINARY_DIR}/fanan_vlc_plugins_cache.cmake")
        file(WRITE "${_fanan_vlc_cache_script}" [=[
# Made by Sonify's CMakeLists.txt: VLC's plugins.dat for the plugins next to Sonify.exe.
# GEN: the vlc-cache-gen.exe next to Sonify.exe (with libvlc's DLLs); PLUGINS: the plugins folder there.
set(_stamp "${PLUGINS}/../vlc-plugins-cache.stamp")
set(_kept "${PLUGINS}/../vlc-plugins-cache.dat")
set(_due FALSE)

if(NOT EXISTS "${_stamp}" OR NOT EXISTS "${_kept}")
    set(_due TRUE)
else()
    file(GLOB_RECURSE _files LIST_DIRECTORIES false "${PLUGINS}/*.dll")

    foreach(_file IN LISTS _files)
        if("${_file}" IS_NEWER_THAN "${_stamp}" AND NOT "${_stamp}" IS_NEWER_THAN "${_file}")
            set(_due TRUE)
            break()
        endif()
    endforeach()
endif()

if(_due)
    unset(ENV{VLC_PLUGIN_PATH})
    execute_process(COMMAND "${GEN}" "${PLUGINS}" RESULT_VARIABLE _result OUTPUT_QUIET ERROR_QUIET TIMEOUT 300)

    if(_result STREQUAL "0" AND EXISTS "${PLUGINS}/plugins.dat")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E copy "${PLUGINS}/plugins.dat" "${_kept}" RESULT_VARIABLE _copied)

        if(_copied STREQUAL "0")
            file(TOUCH "${_stamp}")
        endif()
    else()
        message(STATUS "VLC's list of its plugins (plugins.dat) wasn't made (${_result}): VLC looks into them each time it starts.")
    endif()
else()
    # The plugins as they were: the list made for them, put back if the copy replaced it.
    set(_same FALSE)

    if(EXISTS "${PLUGINS}/plugins.dat")
        file(SHA256 "${PLUGINS}/plugins.dat" _now)
        file(SHA256 "${_kept}" _was)

        if(_now STREQUAL _was)
            set(_same TRUE)
        endif()
    endif()

    if(NOT _same)
        execute_process(COMMAND "${CMAKE_COMMAND}" -E copy "${_kept}" "${PLUGINS}/plugins.dat")
    endif()
endif()
]=])

        add_custom_command(TARGET FananWebHub POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${FANAN_VLC_RUNTIME_DIR}/vlc-cache-gen.exe" "$<TARGET_FILE_DIR:FananWebHub>"
            COMMAND "${CMAKE_COMMAND}" "-DGEN=$<TARGET_FILE_DIR:FananWebHub>/vlc-cache-gen.exe" "-DPLUGINS=$<TARGET_FILE_DIR:FananWebHub>/plugins"
                    -P "${_fanan_vlc_cache_script}"
            VERBATIM)
    endif()
endif()

if(MSVC)
    # The Visual C++ runtime DLLs (msvcp140.dll, vcruntime140.dll...) next to Sonify.exe, inside the
    # build folder, so Sonify (and what it loads) starts on a PC without Microsoft's VC++
    # Redistributable installed; the installer takes them from there with everything else.
    # Microsoft lists them as redistributable beside the program ("app-local").
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)   # only their list: nothing installed anywhere
    include(InstallRequiredSystemLibraries)

    if(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        add_custom_command(TARGET FananWebHub POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} "$<TARGET_FILE_DIR:FananWebHub>"
            VERBATIM)
    else()
        message(STATUS "Sonify: Visual C++ runtime DLLs not found: not copied next to Sonify.exe")
    endif()
endif()

# The knob gallery (assets/knob_gallery, Sonify knobs) as the folder knob_gallery next to Sonify.exe,
# inside the build folder: brought up to date on every build (a knob added there needs no code
# change), and so in the installer too. Built without it when the folder isn't there.
set(FANAN_KNOB_GALLERY "${CMAKE_CURRENT_SOURCE_DIR}/assets/knob_gallery")

if(IS_DIRECTORY "${FANAN_KNOB_GALLERY}")
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.26)
        set(_fanan_copy_gallery copy_directory_if_different)
    else()
        set(_fanan_copy_gallery copy_directory)
    endif()

    # On a Mac, in the app's Resources (Contents/MacOS is for the app's code only: signing refuses
    # anything else there).
    if(APPLE)
        set(_fanan_gallery_to "$<TARGET_BUNDLE_CONTENT_DIR:FananWebHub>/Resources/knob_gallery")
    else()
        set(_fanan_gallery_to "$<TARGET_FILE_DIR:FananWebHub>/knob_gallery")
    endif()

    add_custom_target(SonifyKnobGallery ALL
        COMMAND "${CMAKE_COMMAND}" -E ${_fanan_copy_gallery} "${FANAN_KNOB_GALLERY}" "${_fanan_gallery_to}"
        COMMENT "Sonify: the knob gallery next to Sonify.exe"
        VERBATIM)

    add_dependencies(SonifyKnobGallery FananWebHub)
else()
    message(STATUS "Sonify: no knob gallery in ${FANAN_KNOB_GALLERY}: built without it")
endif()
