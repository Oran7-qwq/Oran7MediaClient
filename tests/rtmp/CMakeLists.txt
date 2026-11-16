add_executable(rtmp_checks main.cpp "${ORAN7_SOURCE_ROOT}/src/cpp/Oran7ScreenCapture/Live/Oran7RtmpPublisher.cpp")
target_include_directories(rtmp_checks PRIVATE "${ORAN7_SOURCE_ROOT}/src/cpp/Oran7ScreenCapture/Live" "${FFMPEG_ROOT_DIR}/include")
target_link_libraries(rtmp_checks PRIVATE Qt6::Core Qt6::Network "${FFMPEG_ROOT_DIR}/lib/avutil.lib" "${FFMPEG_ROOT_DIR}/lib/avcodec.lib" "${FFMPEG_ROOT_DIR}/lib/avformat.lib")

target_compile_definitions(rtmp_checks PRIVATE APP_ROOT="${ORAN7_SOURCE_ROOT}")
set(CAP "${ORAN7_SOURCE_ROOT}/src/cpp/Oran7ScreenCapture")
add_executable(live_checks live.cpp "${CAP}/Live/Oran7LiveSession.cpp" "${CAP}/Live/Oran7LiveSession.h"
 "${CAP}/Live/Oran7LiveBackend.cpp" "${CAP}/Oran7AudioMixPipeline.cpp" "${CAP}/Live/Oran7LiveAudioMixer.cpp" "${CAP}/Live/Oran7RtmpPublisher.cpp"
 "${CAP}/Oran7RecordingSession.cpp" "${CAP}/Oran7RecordingSession.h"
 "${CAP}/Oran7VideoEncoder.cpp" "${CAP}/Oran7AudioEncoder.cpp" "${CAP}/Oran7WasapiAudioWorker.cpp" "${CAP}/Oran7WasapiPcmConverter.cpp")
target_include_directories(live_checks PRIVATE "${CAP}" "${CAP}/Live" "${ORAN7_SOURCE_ROOT}/src/cpp/GlobalSingleton" "${FFMPEG_ROOT_DIR}/include")
target_link_libraries(live_checks PRIVATE Qt6::Core Qt6::Network Oran7Core
 "${FFMPEG_ROOT_DIR}/lib/avutil.lib" "${FFMPEG_ROOT_DIR}/lib/avcodec.lib" "${FFMPEG_ROOT_DIR}/lib/avformat.lib"
 "${FFMPEG_ROOT_DIR}/lib/swresample.lib" ole32)
add_executable(live_audio_checks audio.cpp "${CAP}/Oran7AudioEncoder.cpp" "${CAP}/Live/Oran7LiveAudioMixer.cpp")
target_include_directories(live_audio_checks PRIVATE "${CAP}" "${ORAN7_SOURCE_ROOT}/src/cpp/GlobalSingleton" "${FFMPEG_ROOT_DIR}/include")
target_link_libraries(live_audio_checks PRIVATE Qt6::Core Oran7Core "${FFMPEG_ROOT_DIR}/lib/avutil.lib" "${FFMPEG_ROOT_DIR}/lib/avcodec.lib" "${FFMPEG_ROOT_DIR}/lib/swresample.lib")

add_executable(wasapi_pcm_checks pcm.cpp "${CAP}/Oran7WasapiPcmConverter.cpp")
target_include_directories(wasapi_pcm_checks PRIVATE "${CAP}" "${FFMPEG_ROOT_DIR}/include")
target_link_libraries(wasapi_pcm_checks PRIVATE "${FFMPEG_ROOT_DIR}/lib/avutil.lib" "${FFMPEG_ROOT_DIR}/lib/swresample.lib")

add_executable(live_mix_checks mix.cpp "${CAP}/Live/Oran7LiveAudioMixer.cpp")
target_include_directories(live_mix_checks PRIVATE "${CAP}" "${FFMPEG_ROOT_DIR}/include")
target_link_libraries(live_mix_checks PRIVATE "${FFMPEG_ROOT_DIR}/lib/avutil.lib")
