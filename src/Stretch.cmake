# D:\workspace\Fanan_web_hub\src\Stretch.cmake
#
# Audio > Sync Tempo, Follow Tempo and Pitch: Signalsmith Stretch (MIT), the time and pitch
# stretcher, with Signalsmith Linear (MIT), the FFT it runs on. Both are headers only.
#
# Downloaded once into <build>/deps at pinned commits (Stretch 1.3.2, as its header says, and
# Linear 0.6.4) with fanan_fetch_archive from WclapBridge.cmake: no Git needed. As there, a copy
# kept beside the project (deps\ next to CMakeLists.txt, copied there from a build folder) is
# used instead when it's this very download.
#
# Sets FANAN_STRETCH_INCLUDE_DIRS.

include_guard(GLOBAL)

set(FANAN_STRETCH_COMMIT "a670068d9aeb64913331d5cc29337b19a457a7df")   # signalsmith-stretch 1.3.2 (its header's version)
set(FANAN_LINEAR_COMMIT  "de55e6a50ffcf6f8f43f649692d94691c7025151")   # signalsmith-linear 0.6.4

fanan_fetch_archive(signalsmith-stretch
    "https://github.com/Signalsmith-Audio/signalsmith-stretch/archive/${FANAN_STRETCH_COMMIT}.zip" "" FANAN_STRETCH_DIR)

fanan_fetch_archive(signalsmith-linear
    "https://github.com/Signalsmith-Audio/linear/archive/${FANAN_LINEAR_COMMIT}.zip" "" FANAN_LINEAR_DIR)

set(FANAN_STRETCH_INCLUDE_DIRS "${FANAN_STRETCH_DIR}/include" "${FANAN_LINEAR_DIR}/include")
