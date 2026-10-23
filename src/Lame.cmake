# D:\workspace\Fanan_web_hub\src\Lame.cmake
#
# MP3 export: LAME 3.100, the standard MP3 encoder, built from its official source as a library
# of its own (mp3lame.dll next to Sonify.exe). LAME is LGPL: as a separate library it can ship
# with a commercial app, and anyone may swap it for their own build. Sonify loads it itself the
# first time an MP3 is written, so a build without it still runs (MP3 export says it's missing).
#
# The source is downloaded once into <build>/deps at the pinned version and checked against its
# SHA-256. As with WclapBridge.cmake, a copy kept beside the project (deps\ next to
# CMakeLists.txt, copied there from a build folder) is used instead when it's this very download.
#
# Defines the target fanan_mp3lame when it's there.

include_guard(GLOBAL)

set(FANAN_LAME_VERSION "3.100")
set(FANAN_LAME_SHA256 "ddfe36cab873794038ae2c1210557ad34857a4b6bdc515785d1da9e175b1da1e")
set(FANAN_LAME_URLS
    "https://downloads.sourceforge.net/project/lame/lame/${FANAN_LAME_VERSION}/lame-${FANAN_LAME_VERSION}.tar.gz"
    "http://archive.ubuntu.com/ubuntu/pool/main/l/lame/lame_${FANAN_LAME_VERSION}.orig.tar.gz")

if(NOT DEFINED FANAN_DEPS_DIR)
    set(FANAN_DEPS_DIR "${CMAKE_BINARY_DIR}/deps")
endif()

if(NOT DEFINED FANAN_KEPT_DEPS_DIR)
    set(FANAN_KEPT_DEPS_DIR "${CMAKE_SOURCE_DIR}/deps")
endif()

set(_lame_name "lame-${FANAN_LAME_VERSION}")
set(_lame_dir "${FANAN_DEPS_DIR}/${_lame_name}")
set(_lame_stamp "${FANAN_DEPS_DIR}/${_lame_name}.done")

# The copy kept beside the project, when its stamp names this very download and its folder is whole.
if(EXISTS "${FANAN_KEPT_DEPS_DIR}/${_lame_name}.done")
    file(READ "${FANAN_KEPT_DEPS_DIR}/${_lame_name}.done" _lame_kept)
    string(STRIP "${_lame_kept}" _lame_kept)
    file(GLOB _lame_kept_header "${FANAN_KEPT_DEPS_DIR}/${_lame_name}/lame-*/include/lame.h")

    if(_lame_kept STREQUAL FANAN_LAME_SHA256 AND _lame_kept_header)
        set(_lame_dir "${FANAN_KEPT_DEPS_DIR}/${_lame_name}")
        set(_lame_stamp "${FANAN_KEPT_DEPS_DIR}/${_lame_name}.done")
        message(STATUS "Using ${_lame_name} kept in ${FANAN_KEPT_DEPS_DIR}")
    endif()
endif()

# A stamp left behind by a folder that's gone or isn't whole (moved, renamed, half deleted):
# the source is downloaded and unpacked again.
file(GLOB _lame_header "${_lame_dir}/lame-*/include/lame.h")

if(EXISTS "${_lame_stamp}" AND NOT _lame_header)
    message(STATUS "${_lame_name} in ${_lame_dir} isn't whole: downloading it again")
    file(REMOVE "${_lame_stamp}")
endif()

# Downloaded (from the first address that answers) and unpacked once; the stamp is written only
# when that's whole and the archive is the one expected.
if(NOT EXISTS "${_lame_stamp}")
    file(REMOVE_RECURSE "${_lame_dir}")
    set(_lame_archive "${FANAN_DEPS_DIR}/downloads/${_lame_name}.tar.gz")
    set(_lame_got FALSE)

    foreach(_lame_url IN LISTS FANAN_LAME_URLS)
        message(STATUS "Downloading ${_lame_name}")
        file(DOWNLOAD "${_lame_url}" "${_lame_archive}" STATUS _lame_status TLS_VERIFY ON)
        list(GET _lame_status 0 _lame_code)

        # Checked here rather than by file(DOWNLOAD): a failed address then just moves on to the next.
        if(_lame_code EQUAL 0)
            file(SHA256 "${_lame_archive}" _lame_hash)

            if(_lame_hash STREQUAL FANAN_LAME_SHA256)
                set(_lame_got TRUE)
                break()
            endif()

            set(_lame_status "the archive isn't the one expected")
        endif()

        file(REMOVE "${_lame_archive}")
    endforeach()

    if(NOT _lame_got)
        message(WARNING "Download of ${_lame_name} failed (${_lame_status}): Sonify builds without MP3 export.")
        return()
    endif()

    file(ARCHIVE_EXTRACT INPUT "${_lame_archive}" DESTINATION "${_lame_dir}")
    file(REMOVE "${_lame_archive}")
    file(WRITE "${_lame_stamp}" "${FANAN_LAME_SHA256}\n")
endif()

# The archive holds one folder, lame-3.100.
file(GLOB _lame_roots LIST_DIRECTORIES true "${_lame_dir}/*")
list(FILTER _lame_roots INCLUDE REGEX "/lame-[^/]*$")
set(_lame_root "")

if(_lame_roots)
    list(GET _lame_roots 0 _lame_root)
endif()

if(NOT _lame_root OR NOT EXISTS "${_lame_root}/include/lame.h")
    message(WARNING "${_lame_name} in ${_lame_dir} isn't whole: Sonify builds without MP3 export. Delete ${_lame_stamp} to download it again.")
    return()
endif()

# Its configuration, the same for every compiler: standard headers only (LAME's configMS.h names
# its own integer types, which newer Visual C++ headers can clash with).
set(_lame_config "${CMAKE_BINARY_DIR}/lame-config")
file(MAKE_DIRECTORY "${_lame_config}")
file(WRITE "${_lame_config}/config.h.new" [=[
/* LAME's configuration for Sonify's build. */
#ifndef LAME_CONFIG_H
#define LAME_CONFIG_H
#define STDC_HEADERS 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_LIMITS_H 1
#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STRCHR 1
#define HAVE_MEMCPY 1
#define PACKAGE "lame"
#define PROTOTYPES 1
#define USE_FAST_LOG 1
#define LAME_LIBRARY_BUILD 1
typedef long double ieee854_float80_t;
typedef double      ieee754_float64_t;
typedef float       ieee754_float32_t;
#if defined(__SSE__) || defined(__x86_64__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
#define HAVE_XMMINTRIN_H 1
#endif
#endif
]=])
configure_file("${_lame_config}/config.h.new" "${_lame_config}/config.h" COPYONLY)

set(_lame_sources)

foreach(_lame_file VbrTag bitstream encoder fft gain_analysis id3tag lame mpglib_interface newmdct presets
                   psymodel quantize quantize_pvt reservoir set_get tables takehiro util vbrquantize version)
    list(APPEND _lame_sources "${_lame_root}/libmp3lame/${_lame_file}.c")
endforeach()

list(APPEND _lame_sources "${_lame_root}/libmp3lame/vector/xmm_quantize_sub.c")

add_library(fanan_mp3lame SHARED ${_lame_sources})
set_target_properties(fanan_mp3lame PROPERTIES
    OUTPUT_NAME mp3lame
    C_VISIBILITY_PRESET default
    WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_include_directories(fanan_mp3lame PRIVATE
    "${_lame_config}" "${_lame_root}/include" "${_lame_root}/libmp3lame" "${_lame_root}/mpglib")
target_compile_definitions(fanan_mp3lame PRIVATE HAVE_CONFIG_H)

if(MSVC)
    target_compile_definitions(fanan_mp3lame PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
    target_compile_options(fanan_mp3lame PRIVATE /W0)
else()
    target_compile_options(fanan_mp3lame PRIVATE -w)
    target_link_libraries(fanan_mp3lame PRIVATE m)
endif()
