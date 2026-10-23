# D:\workspace\Fanan_web_hub\src\MagentaJs.cmake
#
# MIDI tools' Magenta models (Google's Magenta, Apache 2.0) run on the page through magenta.js,
# the @magenta/music package's browser build (dist/magentamusic.js, 2.5 MB, TensorFlow.js
# inside), built into Sonify as binary data. The models themselves are downloaded later, from
# the MIDI tab's catalog, each when the user installs it.
#
# The package comes once into <build>/deps from the npm registry at a pinned version, checked
# against its SHA-256. As with BasicPitch.cmake, a copy kept beside the project (deps\ next to
# CMakeLists.txt, copied there from a build folder) is used instead when it's this very file.
#
# Sets FANAN_MAGENTA_JS (empty without it: Sonify builds, and the MIDI tab says the Magenta
# tools need it; the built-in MIDI FX all work).

include_guard(GLOBAL)

set(FANAN_MAGENTA_VERSION "1.23.1")
set(FANAN_MAGENTA_SHA256 "7fcb1b4c0fddb152547a262c46832e2c38f10c8ff7f2237cbdcb6aee33e08885")
set(FANAN_MAGENTA_JS "")

if(NOT DEFINED FANAN_DEPS_DIR)
    set(FANAN_DEPS_DIR "${CMAKE_BINARY_DIR}/deps")
endif()

if(NOT DEFINED FANAN_KEPT_DEPS_DIR)
    set(FANAN_KEPT_DEPS_DIR "${CMAKE_SOURCE_DIR}/deps")
endif()

set(_mm_name "magenta-music-${FANAN_MAGENTA_VERSION}")
set(_mm_archive "${FANAN_DEPS_DIR}/${_mm_name}.tgz")
set(_mm_folder "${FANAN_DEPS_DIR}/${_mm_name}")
set(_mm_file "${_mm_folder}/package/dist/magentamusic.js")

# The copy kept beside the project, when it's this very package.
if(EXISTS "${FANAN_KEPT_DEPS_DIR}/${_mm_name}.tgz")
    file(SHA256 "${FANAN_KEPT_DEPS_DIR}/${_mm_name}.tgz" _mm_kept_hash)

    if(_mm_kept_hash STREQUAL FANAN_MAGENTA_SHA256)
        set(_mm_archive "${FANAN_KEPT_DEPS_DIR}/${_mm_name}.tgz")
        message(STATUS "Using ${_mm_name} kept in ${FANAN_KEPT_DEPS_DIR}")
    endif()
endif()

if(EXISTS "${_mm_archive}")
    file(SHA256 "${_mm_archive}" _mm_hash)

    if(NOT _mm_hash STREQUAL FANAN_MAGENTA_SHA256)
        file(REMOVE "${_mm_archive}")
        file(REMOVE_RECURSE "${_mm_folder}")
    endif()
endif()

if(NOT EXISTS "${_mm_archive}")
    message(STATUS "Downloading magenta.js ${FANAN_MAGENTA_VERSION}")
    file(MAKE_DIRECTORY "${FANAN_DEPS_DIR}")
    file(DOWNLOAD
        "https://registry.npmjs.org/@magenta/music/-/music-${FANAN_MAGENTA_VERSION}.tgz"
        "${_mm_archive}.part"
        STATUS _mm_status
        TLS_VERIFY ON)
    list(GET _mm_status 0 _mm_code)

    # Checked here rather than by file(DOWNLOAD): anything but this very package leaves Sonify
    # building without it rather than stopping the build.
    set(_mm_got_hash "")

    if(_mm_code EQUAL 0)
        file(SHA256 "${_mm_archive}.part" _mm_got_hash)
    endif()

    if(_mm_got_hash STREQUAL FANAN_MAGENTA_SHA256)
        file(RENAME "${_mm_archive}.part" "${_mm_archive}")
    else()
        file(REMOVE "${_mm_archive}.part")
        message(WARNING "Download of magenta.js failed (${_mm_status}): Sonify builds without the Magenta MIDI tools.")
        return()
    endif()
endif()

if(NOT EXISTS "${_mm_file}")
    file(REMOVE_RECURSE "${_mm_folder}")
    file(ARCHIVE_EXTRACT INPUT "${_mm_archive}" DESTINATION "${_mm_folder}" PATTERNS "package/dist/magentamusic.js")
endif()

if(NOT EXISTS "${_mm_file}")
    message(WARNING "magenta.js couldn't be unpacked: Sonify builds without the Magenta MIDI tools.")
    return()
endif()

set(FANAN_MAGENTA_JS "${_mm_file}")
