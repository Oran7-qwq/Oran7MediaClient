cmake_minimum_required(VERSION 3.16)
project(PlaybackChecks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Concurrent)
set(ROOT "${ORAN7_SOURCE_ROOT}")
add_executable(playback_checks main.cpp
    "${ROOT}/src/cpp/Oran7MediaClient/Oran7PlaybackController.cpp"
    "${ROOT}/src/cpp/Oran7MediaClient/Oran7PlaybackController.h"
    "${ROOT}/src/cpp/Oran7MediaClient/Oran7PlaybackState.cpp"
    "${ROOT}/src/cpp/Oran7MediaClient/Oran7Track.h"
    "${ROOT}/src/cpp/Oran7MediaClient/Oran7PlaybackState.h")
target_include_directories(playback_checks PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${ROOT}/src/cpp/Oran7MediaClient" "${ROOT}/src/cpp/GlobalSingleton" "${ROOT}/src/cpp/Oran7MediaPlayer")
target_link_libraries(playback_checks PRIVATE Oran7Core Qt6::Core Qt6::Concurrent)

add_executable(sdl_audio_checks sdl-audio.cpp
    "${ROOT}/src/cpp/Oran7MediaPlayer/Oran7SdlAudioDevice.cpp")
target_compile_definitions(sdl_audio_checks PRIVATE SDL_MAIN_HANDLED)
target_include_directories(sdl_audio_checks PRIVATE
    "${ROOT}/src/cpp/Oran7MediaPlayer" "${FFMPEG_ROOT_DIR}/include")
target_link_libraries(sdl_audio_checks PRIVATE Qt6::Core "${FFMPEG_ROOT_DIR}/lib/SDL2.lib")

find_package(Qt6 REQUIRED COMPONENTS Network)
set(NATIVE "${ROOT}/src/cpp/Oran7MediaPlayer")
add_executable(native_io_checks native-io.cpp
    "${NATIVE}/Oran7MediaPlayer.cpp" "${NATIVE}/Oran7FFplayerCore.cpp"
    "${NATIVE}/Oran7FFmpegDecoderCore.cpp" "${NATIVE}/Oran7SdlAudioDevice.cpp"
    "${NATIVE}/Audio/Oran7AudioProcessor.cpp"
    "${NATIVE}/Audio/Oran7AudioVolumeRamp.cpp"
    "${NATIVE}/FFPlay_Def.cpp" "${NATIVE}/FFMsgQueue.cpp" "${NATIVE}/Sonic.cpp")
target_compile_definitions(native_io_checks PRIVATE SDL_MAIN_HANDLED)
target_include_directories(native_io_checks PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/native-fixtures"
    "${NATIVE}" "${ROOT}/src/cpp/GlobalSingleton" "${FFMPEG_ROOT_DIR}/include")
target_link_libraries(native_io_checks PRIVATE Oran7Core Qt6::Core Qt6::Network
    "${FFMPEG_ROOT_DIR}/lib/avformat.lib" "${FFMPEG_ROOT_DIR}/lib/avcodec.lib"
    "${FFMPEG_ROOT_DIR}/lib/avutil.lib" "${FFMPEG_ROOT_DIR}/lib/swresample.lib"
    "${FFMPEG_ROOT_DIR}/lib/swscale.lib" "${FFMPEG_ROOT_DIR}/lib/SDL2.lib" d3d11 dxgi)

# 真实 Native/FFmpeg，合成 WAV + dummy SDL；不启动主程序、不访问账号或硬件设备。
get_target_property(memorySources native_io_checks SOURCES)
list(REMOVE_ITEM memorySources native-io.cpp)
add_executable(native_memory_checks memory.cpp ${memorySources})
target_compile_definitions(native_memory_checks PRIVATE SDL_MAIN_HANDLED)
target_include_directories(native_memory_checks PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/native-fixtures"
    "${NATIVE}" "${ROOT}/src/cpp/GlobalSingleton" "${FFMPEG_ROOT_DIR}/include")
get_target_property(memoryLibraries native_io_checks LINK_LIBRARIES)
target_link_libraries(native_memory_checks PRIVATE ${memoryLibraries} psapi)

add_executable(video_pacing_checks video-pacing.cpp ${memorySources})
target_compile_definitions(video_pacing_checks PRIVATE SDL_MAIN_HANDLED
    ORAN7_TEST_FFMPEG="${FFMPEG_ROOT_DIR}/bin/ffmpeg.exe")
target_include_directories(video_pacing_checks PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/native-fixtures"
    "${NATIVE}" "${ROOT}/src/cpp/GlobalSingleton" "${FFMPEG_ROOT_DIR}/include")
target_link_libraries(video_pacing_checks PRIVATE ${memoryLibraries})
