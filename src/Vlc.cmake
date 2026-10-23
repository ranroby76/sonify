# D:\workspace\Fanan_web_hub\src\Vlc.cmake
#
# Video: VLC 3.0.21 for 64-bit Windows (libvlc.dll, libvlccore.dll and its plugins folder),
# as On Stage uses it. Sonify loads libvlc itself the first time a video is used, so building
# needs nothing of VLC's: its files are only copied next to Sonify.exe, inside the build folder.
#
# A VLC folder already on the computer can be named instead (cmake -DFANAN_VLC_DIR=...), the
# one with libvlc.dll and plugins in it (On Stage's vlc-3.0.21, say). Otherwise VideoLAN's zip
# is downloaded once into <build>/deps and checked against the SHA-256 VideoLAN publishes with
# it. As with the other downloads, a copy kept beside the project (deps\ next to
# CMakeLists.txt, copied there from a build folder) is used instead when it's this very one.
#
# Sets FANAN_VLC_RUNTIME_DIR (empty without it: Sonify builds, and says video needs VLC).

include_guard(GLOBAL)

set(FANAN_VLC_VERSION "3.0.21")
set(FANAN_VLC_RUNTIME_DIR "")
set(FANAN_VLC_DIR "" CACHE PATH "A VLC 3 folder for 64-bit Windows (libvlc.dll and plugins); empty: downloaded into the build folder")

if(NOT WIN32)
    return()
endif()

if(FANAN_VLC_DIR)
    if(EXISTS "${FANAN_VLC_DIR}/libvlc.dll" AND EXISTS "${FANAN_VLC_DIR}/libvlccore.dll" AND IS_DIRECTORY "${FANAN_VLC_DIR}/plugins")
        set(FANAN_VLC_RUNTIME_DIR "${FANAN_VLC_DIR}")
        message(STATUS "Using VLC in ${FANAN_VLC_DIR}")
        return()
    endif()

    message(WARNING "FANAN_VLC_DIR (${FANAN_VLC_DIR}) has no libvlc.dll, libvlccore.dll and plugins: VLC is downloaded instead.")
endif()

if(NOT DEFINED FANAN_DEPS_DIR)
    set(FANAN_DEPS_DIR "${CMAKE_BINARY_DIR}/deps")
endif()

if(NOT DEFINED FANAN_KEPT_DEPS_DIR)
    set(FANAN_KEPT_DEPS_DIR "${CMAKE_SOURCE_DIR}/deps")
endif()

set(_vlc_name "vlc-${FANAN_VLC_VERSION}-win64")
set(_vlc_url "https://download.videolan.org/pub/videolan/vlc/${FANAN_VLC_VERSION}/win64/${_vlc_name}.zip")
set(_vlc_dir "${FANAN_DEPS_DIR}/${_vlc_name}")
set(_vlc_stamp "${FANAN_DEPS_DIR}/${_vlc_name}.done")

# The copy kept beside the project, when its stamp names this very download.
if(EXISTS "${FANAN_KEPT_DEPS_DIR}/${_vlc_name}.done")
    file(READ "${FANAN_KEPT_DEPS_DIR}/${_vlc_name}.done" _vlc_kept)
    string(STRIP "${_vlc_kept}" _vlc_kept)

    if(_vlc_kept STREQUAL _vlc_url)
        set(_vlc_dir "${FANAN_KEPT_DEPS_DIR}/${_vlc_name}")
        set(_vlc_stamp "${FANAN_KEPT_DEPS_DIR}/${_vlc_name}.done")
        message(STATUS "Using ${_vlc_name} kept in ${FANAN_KEPT_DEPS_DIR}")
    endif()
endif()

# Downloaded and unpacked once (about 78 MB); the stamp is written only when that's whole and
# the zip is the one VideoLAN says it is.
if(NOT EXISTS "${_vlc_stamp}")
    file(REMOVE_RECURSE "${_vlc_dir}")
    set(_vlc_archive "${FANAN_DEPS_DIR}/downloads/${_vlc_name}.zip")
    set(_vlc_sums "${FANAN_DEPS_DIR}/downloads/${_vlc_name}.zip.sha256")
    message(STATUS "Downloading ${_vlc_name} (about 78 MB)")

    file(DOWNLOAD "${_vlc_url}.sha256" "${_vlc_sums}" STATUS _vlc_status TLS_VERIFY ON)
    list(GET _vlc_status 0 _vlc_code)

    if(_vlc_code EQUAL 0)
        file(DOWNLOAD "${_vlc_url}" "${_vlc_archive}" STATUS _vlc_status TLS_VERIFY ON)
        list(GET _vlc_status 0 _vlc_code)
    endif()

    if(NOT _vlc_code EQUAL 0)
        file(REMOVE "${_vlc_archive}" "${_vlc_sums}")
        message(WARNING "Download of ${_vlc_name} failed (${_vlc_status}): Sonify builds without video. "
                        "Name a VLC 3 folder with -DFANAN_VLC_DIR=... to use one already on the computer.")
        return()
    endif()

    file(READ "${_vlc_sums}" _vlc_expected)
    string(REGEX MATCH "[0-9a-fA-F]+" _vlc_expected "${_vlc_expected}")
    string(TOLOWER "${_vlc_expected}" _vlc_expected)
    file(SHA256 "${_vlc_archive}" _vlc_hash)

    if(NOT _vlc_hash STREQUAL _vlc_expected)
        file(REMOVE "${_vlc_archive}" "${_vlc_sums}")
        message(WARNING "${_vlc_name}.zip isn't the one VideoLAN published (its SHA-256 differs): Sonify builds without video.")
        return()
    endif()

    file(ARCHIVE_EXTRACT INPUT "${_vlc_archive}" DESTINATION "${_vlc_dir}")
    file(REMOVE "${_vlc_archive}" "${_vlc_sums}")
    file(WRITE "${_vlc_stamp}" "${_vlc_url}\n")
endif()

# The zip holds one folder (vlc-3.0.21) with everything in it.
file(GLOB _vlc_found LIST_DIRECTORIES false "${_vlc_dir}/*/libvlc.dll" "${_vlc_dir}/libvlc.dll")

if(NOT _vlc_found)
    message(WARNING "${_vlc_name} in ${_vlc_dir} has no libvlc.dll: Sonify builds without video. "
                    "Delete ${_vlc_stamp} to download it again.")
    return()
endif()

list(GET _vlc_found 0 _vlc_found)
get_filename_component(FANAN_VLC_RUNTIME_DIR "${_vlc_found}" DIRECTORY)
