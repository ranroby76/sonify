# D:\workspace\Fanan_web_hub\src\DeviceCompiler.cmake
#
# The Device Studio's compiler: wasi-sdk 25 (clang, and the C and C++ libraries for
# WebAssembly). Sonify devices are compiled with it on the user's computer, so it ships with
# Sonify: the part it needs is copied next to Sonify.exe as "Device Compiler" (about 110 MB on
# Windows). Nothing is installed anywhere else.
#
# Downloaded once into <build>/deps at a pinned version (checked against its SHA-256), or taken
# from the copy kept beside the project (deps\ next to CMakeLists.txt), as WclapBridge.cmake
# does: include this after it. Sets FANAN_DEVICE_COMPILER_DIR (the unpacked wasi-sdk), or leaves
# it empty where there's no wasi-sdk for the platform (Sonify then says devices can't be built
# there; built ones still play).

include_guard(GLOBAL)

set(FANAN_WASI_SDK_VERSION "25")
set(_fanan_wasi_sdk_base "https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-${FANAN_WASI_SDK_VERSION}")
set(FANAN_DEVICE_COMPILER_DIR "")

if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        fanan_fetch_archive(wasi-sdk
            "${_fanan_wasi_sdk_base}/wasi-sdk-25.0-x86_64-windows.tar.gz"
            "ebd4868f95a6e764bf29283e3bdf54df0df3902a29737176da9c17da218b939b" FANAN_DEVICE_COMPILER_DIR)
    endif()
elseif(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        fanan_fetch_archive(wasi-sdk
            "${_fanan_wasi_sdk_base}/wasi-sdk-25.0-arm64-macos.tar.gz"
            "e1e529ea226b1db0b430327809deae9246b580fa3cae32d31c82dfe770233587" FANAN_DEVICE_COMPILER_DIR)
    else()
        fanan_fetch_archive(wasi-sdk
            "${_fanan_wasi_sdk_base}/wasi-sdk-25.0-x86_64-macos.tar.gz"
            "55e3ff3fee1a15678a16eeccba0129276c9f6be481bc9c283e7f9f65bf055c11" FANAN_DEVICE_COMPILER_DIR)
    endif()
else()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        fanan_fetch_archive(wasi-sdk
            "${_fanan_wasi_sdk_base}/wasi-sdk-25.0-arm64-linux.tar.gz"
            "47fccad8b2498f2239e05e1115c3ffc652bf37e7de2f88fb64b2d663c976ce2d" FANAN_DEVICE_COMPILER_DIR)
    else()
        fanan_fetch_archive(wasi-sdk
            "${_fanan_wasi_sdk_base}/wasi-sdk-25.0-x86_64-linux.tar.gz"
            "52640dde13599bf127a95499e61d6d640256119456d1af8897ab6725bcf3d89c" FANAN_DEVICE_COMPILER_DIR)
    endif()
endif()

if(NOT FANAN_DEVICE_COMPILER_DIR)
    message(STATUS "No device compiler for this platform: Sonify devices can be played here, not built.")
endif()

# ------------------------------------------------------------------------------
# fanan_copy_device_compiler(target)
# After each build, the compiler's parts next to the target's executable, in
# "Device Compiler" (on macOS in the app's Resources; only what changed is copied again):
#     bin/clang, bin/wasm-ld             the compiler and the linker
#     lib/clang                          clang's own headers and runtime
#     lib/lib*.so.* / lib*.dylib         (Linux and macOS: the libraries clang runs on)
#     share/wasi-sysroot/include/...     the C and C++ headers for WebAssembly (clang looks for
#                                        include/c++/v1 to use them, though it's empty)
#     share/wasi-sysroot/lib/wasm32-wasip1   the C and C++ libraries
# ------------------------------------------------------------------------------
function(fanan_copy_device_compiler target)
    if(NOT FANAN_DEVICE_COMPILER_DIR)
        return()
    endif()

    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.26")
        set(copy_folder copy_directory_if_different)
    else()
        set(copy_folder copy_directory)
    endif()

    set(from "${FANAN_DEVICE_COMPILER_DIR}")

    if(APPLE)
        # In the app's Resources (Contents/MacOS is for the app itself).
        set(to "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/Device Compiler")
    else()
        set(to "$<TARGET_FILE_DIR:${target}>/Device Compiler")
    endif()
    set(sysroot "${from}/share/wasi-sysroot")

    if(WIN32)
        set(clang_from "${from}/bin/clang.exe")
        set(clang_to "${to}/bin/clang.exe")
        set(ld_from "${from}/bin/wasm-ld.exe")
        set(ld_to "${to}/bin/wasm-ld.exe")
        set(libraries "")
    else()
        # clang and wasm-ld are links there; the files they lead to are copied under their names.
        get_filename_component(clang_from "${from}/bin/clang" REALPATH)
        set(clang_to "${to}/bin/clang")
        get_filename_component(ld_from "${from}/bin/wasm-ld" REALPATH)
        set(ld_to "${to}/bin/wasm-ld")
        file(GLOB libraries "${from}/lib/lib*.so.*" "${from}/lib/lib*.dylib")
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${to}/bin" "${to}/lib"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${clang_from}" "${clang_to}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${ld_from}" "${ld_to}"
        COMMAND "${CMAKE_COMMAND}" -E ${copy_folder} "${from}/lib/clang" "${to}/lib/clang"
        COMMAND "${CMAKE_COMMAND}" -E ${copy_folder} "${sysroot}/include/wasm32-wasip1" "${to}/share/wasi-sysroot/include/wasm32-wasip1"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${to}/share/wasi-sysroot/include/c++/v1"
        COMMAND "${CMAKE_COMMAND}" -E ${copy_folder} "${sysroot}/lib/wasm32-wasip1" "${to}/share/wasi-sysroot/lib/wasm32-wasip1"
        VERBATIM)

    if(libraries)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${libraries} "${to}/lib"
            VERBATIM)
    endif()
endfunction()
