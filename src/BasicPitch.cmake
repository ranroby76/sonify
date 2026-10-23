# D:\workspace\Fanan_web_hub\src\BasicPitch.cmake
#
# Audio > Audio to MIDI (Notes): Spotify's Basic Pitch model (Apache 2.0), nmp.onnx, 230 KB, built
# into Sonify as binary data and run through ONNX Runtime (OnnxRuntime.cmake), as Split into
# Stems runs its own.
#
# Downloaded once into <build>/deps from the basic-pitch repository at a pinned commit, checked
# against its SHA-256. As with Lame.cmake, a copy kept beside the project (deps\ next to
# CMakeLists.txt, copied there from a build folder) is used instead when it's this very file.
#
# Sets FANAN_BASIC_PITCH_MODEL (empty without it: Sonify builds, and says Audio to MIDI's Notes
# needs it; Drums still works).

include_guard(GLOBAL)

set(FANAN_BASIC_PITCH_COMMIT "fa5997af0a8210982619003269994a1be25eddf3")
set(FANAN_BASIC_PITCH_SHA256 "2c3c1d144bfa61ad236e92e169c13535c880469a12a047d4e73451f2c059a0ec")
set(FANAN_BASIC_PITCH_MODEL "")

if(NOT DEFINED FANAN_DEPS_DIR)
    set(FANAN_DEPS_DIR "${CMAKE_BINARY_DIR}/deps")
endif()

if(NOT DEFINED FANAN_KEPT_DEPS_DIR)
    set(FANAN_KEPT_DEPS_DIR "${CMAKE_SOURCE_DIR}/deps")
endif()

set(_bp_name "basic-pitch-${FANAN_BASIC_PITCH_COMMIT}")
set(_bp_file "${FANAN_DEPS_DIR}/${_bp_name}/nmp.onnx")

# The copy kept beside the project, when it's this very file.
if(EXISTS "${FANAN_KEPT_DEPS_DIR}/${_bp_name}/nmp.onnx")
    file(SHA256 "${FANAN_KEPT_DEPS_DIR}/${_bp_name}/nmp.onnx" _bp_kept_hash)

    if(_bp_kept_hash STREQUAL FANAN_BASIC_PITCH_SHA256)
        set(_bp_file "${FANAN_KEPT_DEPS_DIR}/${_bp_name}/nmp.onnx")
        message(STATUS "Using ${_bp_name} kept in ${FANAN_KEPT_DEPS_DIR}")
    endif()
endif()

if(EXISTS "${_bp_file}")
    file(SHA256 "${_bp_file}" _bp_hash)

    if(NOT _bp_hash STREQUAL FANAN_BASIC_PITCH_SHA256)
        file(REMOVE "${_bp_file}")
    endif()
endif()

if(NOT EXISTS "${_bp_file}")
    message(STATUS "Downloading the Basic Pitch model")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/spotify/basic-pitch/${FANAN_BASIC_PITCH_COMMIT}/basic_pitch/saved_models/icassp_2022/nmp.onnx"
        "${_bp_file}.part"
        STATUS _bp_status
        TLS_VERIFY ON)
    list(GET _bp_status 0 _bp_code)

    # Checked here rather than by file(DOWNLOAD): anything but this very file (an error page
    # from a proxy, say) leaves Sonify building without it rather than stopping the build.
    set(_bp_got_hash "")

    if(_bp_code EQUAL 0)
        file(SHA256 "${_bp_file}.part" _bp_got_hash)
    endif()

    if(_bp_got_hash STREQUAL FANAN_BASIC_PITCH_SHA256)
        file(RENAME "${_bp_file}.part" "${_bp_file}")
    else()
        file(REMOVE "${_bp_file}.part")
        message(WARNING "Download of the Basic Pitch model failed (${_bp_status}): Sonify builds without Audio to MIDI's Notes.")
        return()
    endif()
endif()

set(FANAN_BASIC_PITCH_MODEL "${_bp_file}")
