cmake_minimum_required(VERSION 3.16)
project(RecordingChecks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Test Gui Quick Network WebSockets)
set(ROOT "${ORAN7_SOURCE_ROOT}")
set(CAP "${ROOT}/src/cpp/Oran7ScreenCapture")
add_executable(session_checks main.cpp "${CAP}/Oran7RecordingSession.cpp" "${CAP}/Oran7RecordingSession.h")
target_include_directories(session_checks PRIVATE "${CAP}")
target_link_libraries(session_checks PRIVATE Qt6::Core Qt6::Test)
add_executable(backend_checks "${CAP}/Oran7AudioMixPipeline.cpp" "${CAP}/Live/Oran7LiveAudioMixer.cpp" "${CAP}/Oran7WasapiPcmConverter.cpp" backend.cpp "${CAP}/Oran7RecordingBackend.cpp")
set_target_properties(backend_checks PROPERTIES AUTOMOC OFF)
target_include_directories(backend_checks PRIVATE "${CAP}" "${ROOT}/src/cpp/GlobalSingleton" "${ROOT}/src/cpp/Oran7VideoGPURender" "${FFMPEG_ROOT_DIR}/include")
target_link_libraries(backend_checks PRIVATE Qt6::Core Qt6::Gui Qt6::Quick Qt6::Network Qt6::WebSockets
 "${FFMPEG_ROOT_DIR}/lib/avutil.lib" "${FFMPEG_ROOT_DIR}/lib/avcodec.lib" "${FFMPEG_ROOT_DIR}/lib/swresample.lib" d3d11 d3dcompiler)
add_executable(media_checks media.cpp "${CAP}/Oran7VideoEncoder.cpp" "${CAP}/Oran7AudioEncoder.cpp" "${CAP}/Oran7Mp4Muxer.cpp")
set_target_properties(media_checks PROPERTIES AUTOMOC OFF)
target_include_directories(media_checks PRIVATE "${CAP}" "${ROOT}/src/cpp/GlobalSingleton" "${FFMPEG_ROOT_DIR}/include")
target_link_libraries(media_checks PRIVATE Qt6::Core Oran7Core
 "${FFMPEG_ROOT_DIR}/lib/avutil.lib" "${FFMPEG_ROOT_DIR}/lib/avcodec.lib" "${FFMPEG_ROOT_DIR}/lib/avformat.lib" "${FFMPEG_ROOT_DIR}/lib/swresample.lib" ole32)
add_executable(http_checks http.cpp)
add_executable(recording_throughput_checks throughput.cpp "${CAP}/Oran7VideoEncoder.cpp"
    "${CAP}/Oran7AudioEncoder.cpp" "${CAP}/Oran7Mp4Muxer.cpp")
set_target_properties(recording_throughput_checks PROPERTIES AUTOMOC OFF)
target_include_directories(recording_throughput_checks PRIVATE "${CAP}"
    "${ROOT}/src/cpp/GlobalSingleton" "${FFMPEG_ROOT_DIR}/include")
get_target_property(recordingLibraries media_checks LINK_LIBRARIES)
target_link_libraries(recording_throughput_checks PRIVATE ${recordingLibraries})
set_target_properties(http_checks PROPERTIES AUTOMOC OFF OUTPUT_NAME whip_local_http_checks)
target_include_directories(http_checks PRIVATE "${CAP}" "${ROOT}/src/cpp/GlobalSingleton" "${FFMPEG_ROOT_DIR}/include")
target_link_libraries(http_checks PRIVATE Qt6::Core Qt6::Gui Qt6::Network
 "${FFMPEG_ROOT_DIR}/lib/avutil.lib" "${FFMPEG_ROOT_DIR}/lib/avcodec.lib")
add_executable(recording_mix_media_checks mix-media.cpp "${CAP}/Oran7AudioCaptureBackend.cpp" "${CAP}/Oran7AudioMixPipeline.cpp"
    "${CAP}/Live/Oran7LiveAudioMixer.cpp" "${CAP}/Oran7WasapiPcmConverter.cpp"
    "${CAP}/Oran7VideoEncoder.cpp" "${CAP}/Oran7AudioEncoder.cpp" "${CAP}/Oran7Mp4Muxer.cpp")
target_include_directories(recording_mix_media_checks PRIVATE "${CAP}" "${ROOT}/src/cpp/GlobalSingleton" "${FFMPEG_ROOT_DIR}/include")
target_link_libraries(recording_mix_media_checks PRIVATE ${recordingLibraries})
add_executable(simultaneous_output_checks simultaneous.cpp "${CAP}/Oran7CaptureOutputRouter.cpp"
    "${CAP}/Oran7CaptureFrameConverter.cpp" "${CAP}/Oran7VideoEncoder.cpp" "${CAP}/Oran7AudioEncoder.cpp"
    "${CAP}/Oran7Mp4Muxer.cpp" "${CAP}/Live/Oran7RtmpPublisher.cpp")
target_include_directories(simultaneous_output_checks PRIVATE "${CAP}" "${ROOT}/src/cpp/GlobalSingleton" "${FFMPEG_ROOT_DIR}/include")
target_link_libraries(simultaneous_output_checks PRIVATE ${recordingLibraries} Qt6::Network d3d11 dxgi)
target_compile_definitions(simultaneous_output_checks PRIVATE APP_ROOT="${ORAN7_SOURCE_ROOT}")
target_link_libraries(simultaneous_output_checks PRIVATE "${FFMPEG_ROOT_DIR}/lib/swscale.lib")
add_executable(output_shutdown_checks shutdown.cpp "${CAP}/Oran7RecordingSession.cpp"
    "${CAP}/Oran7VideoEncoder.cpp" "${CAP}/Oran7AudioEncoder.cpp" "${CAP}/Oran7Mp4Muxer.cpp"
    "${CAP}/Live/Oran7RtmpPublisher.cpp")
target_include_directories(output_shutdown_checks PRIVATE "${CAP}" "${ROOT}/src/cpp/GlobalSingleton" "${FFMPEG_ROOT_DIR}/include")
target_link_libraries(output_shutdown_checks PRIVATE ${recordingLibraries} Qt6::Network)
