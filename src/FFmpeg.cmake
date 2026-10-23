# D:\workspace\Fanan_web_hub\src\FFmpeg.cmake
#
# Live streaming (LiveStream.cpp) and the Live Show's videos' sound (VideoSound.cpp): FFmpeg 7.1
# for 64-bit Windows, LGPL, shared (BtbN's builds: headers, import libraries and DLLs, no GPL
# parts). Sonify links its DLLs late (delay-loaded) and looks for them itself before the first
# call. The DLLs and FFmpeg's licence are copied next to Sonify.exe, inside the build folder.
#
# The build is a fixed one: BtbN's last 7.1 build, from the monthly release of 2026-07-31 (their
# monthly releases are kept for two years; their "latest" no longer has 7.1, which is why it's
# named exactly), its SHA-256 written here, so a download can never come out another. It's
# downloaded once into <build>/deps. As with the other downloads, a copy kept beside the project
# (deps\ next to CMakeLists.txt, copied there from a build folder) is used instead when it's this
# very one, and an FFmpeg folder already on the computer can be named instead
# (cmake -DFANAN_FFMPEG_DIR=...), the one with include, lib and bin in it (a shared build).
#
# On Windows Sonify isn't built without it: when it can't be had, configuring stops and says
# why. Elsewhere, FFmpeg's development packages are used when pkg-config finds them.
#
# Sets FANAN_FFMPEG_FOUND, and FANAN_FFMPEG_INCLUDE_DIR, FANAN_FFMPEG_LIBRARIES (Windows: the
# import libraries), FANAN_FFMPEG_DLLS (their DLLs), FANAN_FFMPEG_LICENSE, or FANAN_FFMPEG_TARGET
# (pkg-config's).

include_guard(GLOBAL)

set(FANAN_FFMPEG_FOUND OFF)
set(FANAN_FFMPEG_INCLUDE_DIR "")
set(FANAN_FFMPEG_LIBRARIES "")
set(FANAN_FFMPEG_DLLS "")
set(FANAN_FFMPEG_LICENSE "")
set(FANAN_FFMPEG_TARGET "")
set(FANAN_FFMPEG_DIR "" CACHE PATH "A shared FFmpeg 7 build for 64-bit Windows (include, lib, bin); empty: downloaded into the build folder")

# macOS and Linux: whether the FFmpeg pkg-config finds is used. A Mac build that's given to others
# turns it off (-DFANAN_SYSTEM_FFMPEG=OFF): Homebrew's FFmpeg lives outside the app (and isn't LGPL
# only), so the app would need it installed to start.
option(FANAN_SYSTEM_FFMPEG "macOS and Linux: use the FFmpeg pkg-config finds (live streaming, videos' sound)" ON)

# The libraries Sonify calls.
set(_ffmpeg_parts avdevice avformat avcodec swscale swresample avutil)

if(NOT WIN32 AND NOT FANAN_SYSTEM_FFMPEG)
    message(STATUS "FANAN_SYSTEM_FFMPEG is off: Sonify builds without live streaming.")
    return()
endif()

if(NOT WIN32)
    find_package(PkgConfig QUIET)

    if(PKG_CONFIG_FOUND)
        pkg_check_modules(FANAN_FFMPEG_PC QUIET IMPORTED_TARGET
                          libavdevice libavformat libavcodec libswscale libswresample libavutil)

        if(FANAN_FFMPEG_PC_FOUND)
            set(FANAN_FFMPEG_TARGET PkgConfig::FANAN_FFMPEG_PC)
            set(FANAN_FFMPEG_FOUND ON)
            message(STATUS "Using FFmpeg from pkg-config for live streaming")
        endif()
    endif()

    if(NOT FANAN_FFMPEG_FOUND)
        message(STATUS "FFmpeg's development packages aren't there: Sonify builds without live streaming.")
    endif()

    return()
endif()

# A folder with FFmpeg in it: its root, if it has the headers, import libraries and DLLs.
function(_fanan_ffmpeg_use root)
    foreach(_part IN LISTS _ffmpeg_parts)
        if(NOT EXISTS "${root}/lib/${_part}.lib")
            return()
        endif()

        file(GLOB _dll "${root}/bin/${_part}-*.dll")

        if(NOT _dll)
            return()
        endif()
    endforeach()

    if(NOT EXISTS "${root}/include/libavformat/avformat.h")
        return()
    endif()

    set(_libraries "")
    set(_dlls "")

    foreach(_part IN LISTS _ffmpeg_parts)
        list(APPEND _libraries "${root}/lib/${_part}.lib")
    endforeach()

    # Every DLL it has (avdevice needs avfilter's, say).
    file(GLOB _dlls "${root}/bin/*.dll")
    file(GLOB _license "${root}/LICENSE*")

    set(FANAN_FFMPEG_FOUND ON PARENT_SCOPE)
    set(FANAN_FFMPEG_INCLUDE_DIR "${root}/include" PARENT_SCOPE)
    set(FANAN_FFMPEG_LIBRARIES "${_libraries}" PARENT_SCOPE)
    set(FANAN_FFMPEG_DLLS "${_dlls}" PARENT_SCOPE)

    if(_license)
        list(GET _license 0 _license)
        set(FANAN_FFMPEG_LICENSE "${_license}" PARENT_SCOPE)
    endif()
endfunction()

if(FANAN_FFMPEG_DIR)
    _fanan_ffmpeg_use("${FANAN_FFMPEG_DIR}")

    if(FANAN_FFMPEG_FOUND)
        message(STATUS "Using FFmpeg in ${FANAN_FFMPEG_DIR}")
        return()
    endif()

    message(WARNING "FANAN_FFMPEG_DIR (${FANAN_FFMPEG_DIR}) isn't a shared FFmpeg build (include, lib, bin): FFmpeg is downloaded instead.")
endif()

if(NOT DEFINED FANAN_DEPS_DIR)
    set(FANAN_DEPS_DIR "${CMAKE_BINARY_DIR}/deps")
endif()

if(NOT DEFINED FANAN_KEPT_DEPS_DIR)
    set(FANAN_KEPT_DEPS_DIR "${CMAKE_SOURCE_DIR}/deps")
endif()

set(_ffmpeg_name "ffmpeg-n7.1.5-12-g1fdbca85aa-win64-lgpl-shared-7.1")
set(_ffmpeg_url "https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2026-07-31-14-10/${_ffmpeg_name}.zip")
set(_ffmpeg_sha256 "0f376f96fb38554ccefb1b2ae9c7c6a7b351f0e60a372b38262c320e8392c5d0")
set(_ffmpeg_dir "${FANAN_DEPS_DIR}/${_ffmpeg_name}")
set(_ffmpeg_stamp "${FANAN_DEPS_DIR}/${_ffmpeg_name}.done")

# The copy kept beside the project, when its stamp names this very download.
if(EXISTS "${FANAN_KEPT_DEPS_DIR}/${_ffmpeg_name}.done")
    file(READ "${FANAN_KEPT_DEPS_DIR}/${_ffmpeg_name}.done" _ffmpeg_kept)
    string(STRIP "${_ffmpeg_kept}" _ffmpeg_kept)

    if(_ffmpeg_kept STREQUAL _ffmpeg_url)
        set(_ffmpeg_dir "${FANAN_KEPT_DEPS_DIR}/${_ffmpeg_name}")
        set(_ffmpeg_stamp "${FANAN_KEPT_DEPS_DIR}/${_ffmpeg_name}.done")
        message(STATUS "Using ${_ffmpeg_name} kept in ${FANAN_KEPT_DEPS_DIR}")
    endif()
endif()

# Why it can't be had, and what to do: configuring stops here.
function(_fanan_ffmpeg_missing why)
    message(FATAL_ERROR "Sonify needs FFmpeg for live streaming, and ${why}\n"
                        "Check the internet connection and configure again. Or download ${_ffmpeg_url} "
                        "yourself, unpack it, and configure with -DFANAN_FFMPEG_DIR=<the folder with include, lib and bin in it>.")
endfunction()

# Downloaded and unpacked once (about 60 MB); the stamp is written only when that's whole and
# the zip is this very one.
if(NOT EXISTS "${_ffmpeg_stamp}")
    file(REMOVE_RECURSE "${_ffmpeg_dir}")
    set(_ffmpeg_archive "${FANAN_DEPS_DIR}/downloads/${_ffmpeg_name}.zip")
    message(STATUS "Downloading ${_ffmpeg_name} (about 60 MB)")

    file(DOWNLOAD "${_ffmpeg_url}" "${_ffmpeg_archive}" STATUS _ffmpeg_status TLS_VERIFY ON)
    list(GET _ffmpeg_status 0 _ffmpeg_code)

    if(NOT _ffmpeg_code EQUAL 0)
        list(GET _ffmpeg_status 1 _ffmpeg_reason)
        file(REMOVE "${_ffmpeg_archive}")
        _fanan_ffmpeg_missing("its download failed (${_ffmpeg_reason}).")
    endif()

    file(SHA256 "${_ffmpeg_archive}" _ffmpeg_hash)

    if(NOT _ffmpeg_hash STREQUAL _ffmpeg_sha256)
        file(REMOVE "${_ffmpeg_archive}")
        _fanan_ffmpeg_missing("the file downloaded isn't the one expected (its SHA-256 differs; it may have been cut short).")
    endif()

    file(ARCHIVE_EXTRACT INPUT "${_ffmpeg_archive}" DESTINATION "${_ffmpeg_dir}")
    file(REMOVE "${_ffmpeg_archive}")
    file(WRITE "${_ffmpeg_stamp}" "${_ffmpeg_url}\n")
endif()

# The zip holds one folder (named as the zip) with everything in it.
file(GLOB _ffmpeg_found LIST_DIRECTORIES false "${_ffmpeg_dir}/*/include/libavformat/avformat.h" "${_ffmpeg_dir}/include/libavformat/avformat.h")

if(_ffmpeg_found)
    list(GET _ffmpeg_found 0 _ffmpeg_found)
    get_filename_component(_ffmpeg_root "${_ffmpeg_found}" DIRECTORY)   # libavformat
    get_filename_component(_ffmpeg_root "${_ffmpeg_root}" DIRECTORY)    # include
    get_filename_component(_ffmpeg_root "${_ffmpeg_root}" DIRECTORY)    # the build
    _fanan_ffmpeg_use("${_ffmpeg_root}")
endif()

if(NOT FANAN_FFMPEG_FOUND)
    _fanan_ffmpeg_missing("${_ffmpeg_name} in ${_ffmpeg_dir} isn't a whole shared build. Delete ${_ffmpeg_stamp} to download it again.")
endif()

message(STATUS "Using ${_ffmpeg_name} for live streaming")
