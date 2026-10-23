# D:\workspace\Fanan_web_hub\src\OnnxRuntime.cmake
#
# ONNX Runtime, for Split into Stems: Microsoft's Microsoft.ML.OnnxRuntime NuGet package at a
# pinned version, downloaded once into <build>/deps. As with WclapBridge.cmake, a copy kept beside
# the project (deps\ next to CMakeLists.txt, copied there from a build folder) is used instead when
# it's this very download, so a new build folder downloads nothing. Sonify loads onnxruntime.dll
# itself the first time stems are split, so building needs only the package's header; the DLLs
# are copied next to Sonify.exe.
#
# Sets FANAN_ONNXRUNTIME_INCLUDE_DIR (empty without it: Sonify builds, and says stem splitting
# isn't there) and FANAN_ONNXRUNTIME_DLLS.

include_guard(GLOBAL)

set(FANAN_ONNXRUNTIME_VERSION "1.20.1")
set(FANAN_ONNXRUNTIME_INCLUDE_DIR "")
set(FANAN_ONNXRUNTIME_DLLS "")

if(NOT WIN32)
    return()
endif()

if(NOT DEFINED FANAN_DEPS_DIR)
    set(FANAN_DEPS_DIR "${CMAKE_BINARY_DIR}/deps")
endif()

if(NOT DEFINED FANAN_KEPT_DEPS_DIR)
    set(FANAN_KEPT_DEPS_DIR "${CMAKE_SOURCE_DIR}/deps")
endif()

set(_ort_name "onnxruntime-${FANAN_ONNXRUNTIME_VERSION}")
set(_ort_url "https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/${FANAN_ONNXRUNTIME_VERSION}/microsoft.ml.onnxruntime.${FANAN_ONNXRUNTIME_VERSION}.nupkg")
set(_ort_dir "${FANAN_DEPS_DIR}/${_ort_name}")
set(_ort_stamp "${FANAN_DEPS_DIR}/${_ort_name}.done")

# The copy kept beside the project, when its stamp names this very download.
if(EXISTS "${FANAN_KEPT_DEPS_DIR}/${_ort_name}.done")
    file(READ "${FANAN_KEPT_DEPS_DIR}/${_ort_name}.done" _ort_kept_url)
    string(STRIP "${_ort_kept_url}" _ort_kept_url)

    if(_ort_kept_url STREQUAL _ort_url)
        set(_ort_dir "${FANAN_KEPT_DEPS_DIR}/${_ort_name}")
        set(_ort_stamp "${FANAN_KEPT_DEPS_DIR}/${_ort_name}.done")
        message(STATUS "Using ${_ort_name} kept in ${FANAN_KEPT_DEPS_DIR}")
    endif()
endif()

# Downloaded and unpacked once; the stamp is written only when that's whole.
if(NOT EXISTS "${_ort_stamp}")
    file(REMOVE_RECURSE "${_ort_dir}")
    set(_ort_archive "${FANAN_DEPS_DIR}/downloads/${_ort_name}.nupkg")
    message(STATUS "Downloading ${_ort_name}")

    file(DOWNLOAD "${_ort_url}" "${_ort_archive}" STATUS _ort_status TLS_VERIFY ON)
    list(GET _ort_status 0 _ort_code)

    if(NOT _ort_code EQUAL 0)
        file(REMOVE "${_ort_archive}")
        message(WARNING "Download of ${_ort_name} failed (${_ort_status}): Sonify builds without Split into Stems.")
        return()
    endif()

    file(ARCHIVE_EXTRACT INPUT "${_ort_archive}" DESTINATION "${_ort_dir}")
    file(REMOVE "${_ort_archive}")
    file(WRITE "${_ort_stamp}" "${_ort_url}\n")
endif()

# The header, wherever the package keeps it, and the 64-bit Windows DLLs.
file(GLOB_RECURSE _ort_header "${_ort_dir}/build/native/include/onnxruntime_c_api.h")

if(NOT _ort_header)
    file(GLOB_RECURSE _ort_header "${_ort_dir}/*/onnxruntime_c_api.h")
endif()

file(GLOB _ort_dlls "${_ort_dir}/runtimes/win-x64/native/*.dll")

if(NOT _ort_header OR NOT _ort_dlls)
    message(WARNING "${_ort_name} in ${_ort_dir} has no header or DLL for 64-bit Windows: Sonify builds without Split into Stems. "
                    "Delete ${_ort_stamp} to download it again.")
    return()
endif()

list(GET _ort_header 0 _ort_header)
get_filename_component(FANAN_ONNXRUNTIME_INCLUDE_DIR "${_ort_header}" DIRECTORY)
set(FANAN_ONNXRUNTIME_DLLS ${_ort_dlls})
