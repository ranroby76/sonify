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
# Video.
#   Windows: VLC 3.0.21's runtime (libvlc and its plugins), downloaded once into build/deps (or
#            the VLC folder named with -DFANAN_VLC_DIR=...) and copied next to Sonify.exe.
#            Sonify loads it itself, so it builds without it, and says video needs VLC.
#   Linux:   the libvlc the machine has (the package "vlc", or libvlc5 with vlc-plugin-base),
#            loaded by name at the first video; nothing is downloaded or shipped.
#   macOS:   AVFoundation, part of the system (src/VideoMedia_Apple.mm) - no VLC at all, so
#            there is nothing to download, ship, sign or notarize for video on a Mac.
# ------------------------------------------------------------------------------
if(NOT APPLE)
    include(src/Vlc.cmake)
endif()

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
    src/PluginHeader.h
    src/PluginHeader.cpp
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
    src/AiMix.h
    src/AiMix.cpp
    src/MixAnalysis.h
    src/MixAnalysis.cpp
    src/MainAiMix.cpp
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
    src/VideoMedia_Apple.h
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
    src/SipFiles.h
    src/SipFiles.cpp
    src/DeviceStudio.h
    src/DeviceStudio.cpp
    src/MainDevices.cpp
    src/AnalogBoard.h
    src/AnalogBoard.cpp
    src/MasterSection.h
    src/MasterSection.cpp
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
    src/AxisPack.h
    src/AxisPack.cpp
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
    src/SourceGuard.h
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
    src/qrcode.js
    src/hub-ai.js
    src/phone-remote.html
    src/hub-browser.js
    src/hub-settings.js
    src/hub-link.js
    src/hub-blocks.js
    src/hub-video.js
    src/hub-devices.js
    src/hub-board.js
    src/hub-aimix.js
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
        src/sonify_app_events.md
        src/sonify_mix_guide.md
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

# The built-in effects' and Cayman's panels (src/panel_*.json, BuiltinScreen.cpp), their factory
# presets (src/presets_*.json) and the pictures they show (Sonify's logo): a new file shows after the
# next build.
file(GLOB SONIFY_PANELS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/panel_*.json"
                                          "${CMAKE_CURRENT_SOURCE_DIR}/src/presets_*.json")
list(APPEND SONIFY_PANELS "${CMAKE_CURRENT_SOURCE_DIR}/assets/Sonify.png")

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

# The Tomahawk sampler (src/TomahawkSampler.h): Grex's engine, effects and instrument editor as
# they are in Grex (src/tomahawk, juce_dsp for its chorus), and the pictures its window shows: each
# instrument family's icon (assets/instruments icons), and the packs' pictures for a family without
# one when they're there (SP_CLASSIC.png, else SP_WORLD.png, and SP_ORIENTAL.png, in
# assets/instrumet images or assets: optional).
file(GLOB TOMAHAWK_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/tomahawk/*.h")

target_sources(FananWebHub PRIVATE
    src/TomahawkSampler.h
    src/TomahawkSampler.cpp
    src/TomahawkScreen.h
    src/TomahawkScreen.cpp
    src/tomahawk/Channel.cpp
    src/tomahawk/TomahawkVoice.cpp
    ${TOMAHAWK_HEADERS})

# Grex's files are UTF-8 (its comments): MSVC reads them as such. They're Grex's, written for
# JUCE 8, so JUCE 9's deprecation warnings about them are left out of the build's.
set(TOMAHAWK_COMPILED src/TomahawkSampler.cpp src/TomahawkScreen.cpp src/tomahawk/Channel.cpp src/tomahawk/TomahawkVoice.cpp)

if(MSVC)
    set_source_files_properties(${TOMAHAWK_COMPILED} PROPERTIES COMPILE_OPTIONS "/utf-8;/bigobj;/wd4996")
else()
    set_source_files_properties(${TOMAHAWK_COMPILED} PROPERTIES COMPILE_OPTIONS "-Wno-deprecated-declarations")
endif()

# The family icons (assets/instruments icons, one per family folder: accordion.png, brass.png...).
file(GLOB TOMAHAWK_IMAGES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/instruments icons/*.png")

# The packs' pictures, only for a family without an icon, when they're there (optional).
foreach(_pack_image SP_CLASSIC SP_WORLD SP_ORIENTAL)
    foreach(_image_home "${CMAKE_CURRENT_SOURCE_DIR}/assets/instrumet images" "${CMAKE_CURRENT_SOURCE_DIR}/assets")
        if(EXISTS "${_image_home}/${_pack_image}.png")
            list(APPEND TOMAHAWK_IMAGES "${_image_home}/${_pack_image}.png")
            break()
        endif()
    endforeach()
endforeach()

if(NOT TOMAHAWK_IMAGES)
    message(FATAL_ERROR "The Tomahawk's pictures are missing: put the family icons in ${CMAKE_CURRENT_SOURCE_DIR}/assets/instruments icons")
endif()

juce_add_binary_data(TomahawkImages
    HEADER_NAME TomahawkImages.h
    NAMESPACE   TomahawkImages
    SOURCES     ${TOMAHAWK_IMAGES})

target_link_libraries(FananWebHub PRIVATE TomahawkImages juce::juce_dsp)

# ------------------------------------------------------------------------------
# Sonify's own plugins, built into Sonify.exe (src/InternalPlugin.h)
#
# Each is written the way a plugin is - a juce::AudioProcessor with a window of its own - and
# hosted exactly as a VST3 is (the same parameters, the same state in songs, the same window over
# the Hub), only made in Sonify instead of loaded from a file: no plugin file exists and nothing
# is installed. The Browser lists them with the other built-ins, under Sonify.exe.
#
# Each one's folder under src is its own (src/natura, src/zax, src/axis), and Sonify reaches it
# only through the entries at the bottom of InternalPlugin.cpp.
# ------------------------------------------------------------------------------
file(GLOB NATURA_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/natura/*.h")
file(GLOB ZAX_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/zax/*.h")
file(GLOB AXIS_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/axis/*.h"
                                         "${CMAKE_CURRENT_SOURCE_DIR}/src/axis/dsp/*.h")

foreach(_internal natura zax axis)
    if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/${_internal}")
        message(FATAL_ERROR "Sonify: src/${_internal} is missing: put that plugin's source there "
                            "(${CMAKE_CURRENT_SOURCE_DIR}/src/${_internal})")
    endif()
endforeach()

target_sources(FananWebHub PRIVATE
    src/InternalPlugin.h
    src/InternalPlugin.cpp

    # Their presets: one .sonifypreset file each, in the folder Sonify's own preset manager reads.
    # One copy for them all; each includes it as "../SonifyPresets.h".
    src/SonifyPresets.h

    # Natura: Sonify's modelling synth.
    src/natura/PluginProcessor.cpp
    src/natura/PluginEditor.cpp
    src/natura/NaturaEngine.cpp
    src/natura/FananLookAndFeel.cpp
    ${NATURA_HEADERS}

    # Zax: Sonify's drum synth. Everything but its two sides is in its headers, and its effects
    # are Grex's, from Sonify's own src/GrexFx.h (one copy for the whole program).
    src/zax/PluginProcessor.cpp
    src/zax/PluginEditor.cpp
    ${ZAX_HEADERS}

    # AXIS: Sonify's guitar board. Its panel is a web page of its own (src/axis/Resources/ui,
    # compiled in below), shown in the one WebView2 profile every web view in Sonify uses.
    src/axis/PluginProcessor.cpp
    src/axis/PluginEditor.cpp
    ${AXIS_HEADERS})

# AXIS's panel: its page compiled into Sonify, which its editor serves from memory (AxisWebUI.h),
# the way Sonify's own page is served. A file added there shows after the next build.
file(GLOB_RECURSE AXIS_UI_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/axis/Resources/ui/*")

if(NOT AXIS_UI_FILES)
    message(FATAL_ERROR "AXIS's panel is missing: put its ui folder in ${CMAKE_CURRENT_SOURCE_DIR}/src/axis/Resources/ui")
endif()

juce_add_binary_data(AxisWebUI
    HEADER_NAME AxisWebUI.h
    NAMESPACE   AxisWebUI
    SOURCES     ${AXIS_UI_FILES})

target_link_libraries(FananWebHub PRIVATE AxisWebUI)

# Natura's sampled attacks (src/natura/AttackLibrary.h): the first moments of real notes, cut from
# VSCO 2 Community Edition and the Versilian Community Sample Library (both CC0), compiled in.
set(NATURA_ATTACKS "${CMAKE_CURRENT_SOURCE_DIR}/src/natura/attacks/natura_attacks.bin")

if(NOT EXISTS "${NATURA_ATTACKS}")
    message(FATAL_ERROR "Natura's attacks are missing: put natura_attacks.bin in ${CMAKE_CURRENT_SOURCE_DIR}/src/natura/attacks")
endif()

juce_add_binary_data(NaturaAttacks
    HEADER_NAME NaturaAttacks.h
    NAMESPACE   NaturaAttacks
    SOURCES     "${NATURA_ATTACKS}")

target_link_libraries(FananWebHub PRIVATE NaturaAttacks)

# Natura's tone layer (src/natura/ToneLayer.h): harmonic spectra of real instruments' steady tones,
# from the same two CC0 libraries, and its designed characters (duduk, klezmer, zurna...), compiled in.
set(NATURA_TONES "${CMAKE_CURRENT_SOURCE_DIR}/src/natura/attacks/natura_tones.bin")

if(NOT EXISTS "${NATURA_TONES}")
    message(FATAL_ERROR "Natura's tone layer is missing: put natura_tones.bin in ${CMAKE_CURRENT_SOURCE_DIR}/src/natura/attacks")
endif()

juce_add_binary_data(NaturaTones
    HEADER_NAME NaturaTones.h
    NAMESPACE   NaturaTones
    SOURCES     "${NATURA_TONES}")

target_link_libraries(FananWebHub PRIVATE NaturaTones)

# Natura's own knob looks (src/natura/knobs/*.fklayers: the captain's steer knob of its MACROS),
# compiled in. Built without them when the folder is empty: the knobs then wear Sonify's example knob.
file(GLOB NATURA_KNOBS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/natura/knobs/*.fklayers")

if(NATURA_KNOBS)
    juce_add_binary_data(NaturaKnobs
        HEADER_NAME NaturaKnobs.h
        NAMESPACE   NaturaKnobs
        SOURCES     ${NATURA_KNOBS})

    target_link_libraries(FananWebHub PRIVATE NaturaKnobs)
else()
    message(STATUS "Natura: no knob looks in ${CMAKE_CURRENT_SOURCE_DIR}/src/natura/knobs: its macros wear the example knob")
endif()

# Their files are UTF-8 (their comments, and Grex's parts among them): MSVC reads them as such, and
# needs /bigobj for the drum voices' tables. Grex's parts were written for JUCE 8, so JUCE 9's
# deprecation warnings about them are left out of the build's, as the Tomahawk's are.
set(FANAN_INTERNAL_COMPILED
    src/natura/PluginProcessor.cpp src/natura/PluginEditor.cpp
    src/natura/NaturaEngine.cpp src/natura/FananLookAndFeel.cpp
    src/zax/PluginProcessor.cpp src/zax/PluginEditor.cpp
    src/axis/PluginProcessor.cpp src/axis/PluginEditor.cpp)

if(MSVC)
    set_source_files_properties(${FANAN_INTERNAL_COMPILED} PROPERTIES COMPILE_OPTIONS "/utf-8;/bigobj;/wd4996")
else()
    set_source_files_properties(${FANAN_INTERNAL_COMPILED} PROPERTIES COMPILE_OPTIONS "-Wno-deprecated-declarations")
endif()

# ------------------------------------------------------------------------------
# Video on a Mac: AVFoundation in place of VLC (src/VideoMedia_Apple.mm; the rest of VideoMedia
# - the cache, the sound lined up with the picture, the strip of small pictures - is the same on
# every platform). Nothing is downloaded or shipped for it, and nothing extra to sign.
# ------------------------------------------------------------------------------
if(APPLE)
    target_sources(FananWebHub PRIVATE src/VideoMedia_Apple.mm)

    # Its own file is Objective-C++ without ARC, as every .mm of Sonify's is.
    set_source_files_properties(src/VideoMedia_Apple.mm PROPERTIES COMPILE_OPTIONS "-fno-objc-arc")

    target_link_libraries(FananWebHub PRIVATE
        "-framework AVFoundation"
        "-framework CoreMedia"
        "-framework CoreVideo"
        "-framework QuartzCore"
        "-framework CoreGraphics")
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

# Folders Sonify reads next to Sonify.exe, inside the build folder: the knob gallery and the
# Tomahawk's presets. Each is copied by a target Sonify depends on, so it's brought up to date on
# every build however the build is started (the whole build, the FananWebHub target alone, or
# Visual Studio), even when Sonify.exe isn't relinked. On a Mac they go in the app's Resources
# (Contents/MacOS is for the app's code only: signing refuses anything else there).
get_target_property(FANAN_EXE_DIR FananWebHub RUNTIME_OUTPUT_DIRECTORY)

if(APPLE)
    set(FANAN_BESIDE_EXE "${FANAN_EXE_DIR}/Sonify.app/Contents/Resources")
else()
    set(FANAN_BESIDE_EXE "${FANAN_EXE_DIR}")
endif()

function(fanan_copy_beside_exe target from name)
    file(GLOB_RECURSE _files LIST_DIRECTORIES false "${from}/*")
    list(LENGTH _files _count)

    if(_count EQUAL 0)
        message(WARNING "Sonify: ${from} is empty: nothing to copy for ${name}")
    else()
        message(STATUS "Sonify: ${name} (${_count} files) from ${from}")
    endif()

    add_custom_target(${target}
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${from}" "${FANAN_BESIDE_EXE}/${name}"
        COMMENT "Sonify: ${name} next to Sonify.exe"
        VERBATIM)

    add_dependencies(FananWebHub ${target})
endfunction()

# The knob gallery (assets/knob_gallery, Sonify knobs): a knob added there needs no code change,
# and it's in the installer too. Built without it when the folder isn't there.
set(FANAN_KNOB_GALLERY "${CMAKE_CURRENT_SOURCE_DIR}/assets/knob_gallery")

if(IS_DIRECTORY "${FANAN_KNOB_GALLERY}")
    fanan_copy_beside_exe(SonifyKnobGallery "${FANAN_KNOB_GALLERY}" knob_gallery)
else()
    message(STATUS "Sonify: no knob gallery in ${FANAN_KNOB_GALLERY}: built without it")
endif()

# AXIS's pack (assets/AXIS.blob: cabinets' impulse responses in one file), beside Sonify.exe where
# AXIS looks for it first. Built without it when it isn't there:
# AXIS then has its four modelled cabinets, and IR files loaded one by one.
set(FANAN_AXIS_PACK "${CMAKE_CURRENT_SOURCE_DIR}/assets/AXIS.blob")

if(EXISTS "${FANAN_AXIS_PACK}")
    add_custom_target(SonifyAxisPack
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${FANAN_BESIDE_EXE}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${FANAN_AXIS_PACK}" "${FANAN_BESIDE_EXE}/AXIS.blob"
        COMMENT "Sonify: AXIS.blob next to Sonify.exe"
        VERBATIM)

    add_dependencies(FananWebHub SonifyAxisPack)
else()
    message(STATUS "Sonify: no AXIS pack in ${FANAN_AXIS_PACK}: AXIS built with its modelled cabinets only")
endif()

# The Tomahawk's presets: Tomahawk_classic_instruments_presets and Tomahawk_oriental_instruments_presets,
# found at the project's root, in assets or in Tomahawk_Sampler. A base gain or base octave set in
# the Tomahawk's window is written into that source copy too, so the next build keeps it.
set(TOMAHAWK_PRESETS_DIR "")

foreach(_presets_home "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/assets" "${CMAKE_CURRENT_SOURCE_DIR}/Tomahawk_Sampler")
    if(NOT TOMAHAWK_PRESETS_DIR AND (IS_DIRECTORY "${_presets_home}/Tomahawk_classic_instruments_presets"
                                     OR IS_DIRECTORY "${_presets_home}/Tomahawk_oriental_instruments_presets"))
        set(TOMAHAWK_PRESETS_DIR "${_presets_home}")
    endif()
endforeach()

if(TOMAHAWK_PRESETS_DIR)
    set_property(SOURCE src/tomahawk/TomahawkVoice.cpp APPEND PROPERTY
                 COMPILE_DEFINITIONS "TOMAHAWK_PRESETS_SOURCE_DIR=\"${TOMAHAWK_PRESETS_DIR}\"")

    foreach(_pack classic oriental)
        set(_presets "Tomahawk_${_pack}_instruments_presets")

        if(IS_DIRECTORY "${TOMAHAWK_PRESETS_DIR}/${_presets}")
            fanan_copy_beside_exe(SonifyTomahawkPresets_${_pack} "${TOMAHAWK_PRESETS_DIR}/${_presets}" ${_presets})
        else()
            message(WARNING "Sonify: ${_presets} isn't in ${TOMAHAWK_PRESETS_DIR}")
        endif()
    endforeach()
else()
    message(WARNING "Sonify: the Tomahawk's presets weren't found: put the folders Tomahawk_classic_instruments_presets "
                    "and Tomahawk_oriental_instruments_presets in ${CMAKE_CURRENT_SOURCE_DIR}")
endif()
