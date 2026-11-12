cmake_minimum_required(VERSION 3.16)
project(AudioDspChecks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ROOT "${ORAN7_SOURCE_ROOT}")

# 纯 C++ 检查：不链接 Qt，不打开音频设备，不启动播放器。
add_executable(audio_parameter_queue_checks parameter-queue.cpp)
target_include_directories(audio_parameter_queue_checks PRIVATE
    "${ROOT}/src/cpp/Oran7MediaPlayer/Audio")
target_link_libraries(audio_parameter_queue_checks PRIVATE
    "${ORAN7_WINPTHREAD_LIBRARY}" "${ORAN7_SSP_LIBRARY}")

add_executable(audio_processor_checks processor.cpp
    "${ROOT}/src/cpp/Oran7MediaPlayer/Audio/Oran7AudioProcessor.cpp")
target_include_directories(audio_processor_checks PRIVATE
    "${ROOT}/src/cpp/Oran7MediaPlayer/Audio")
target_link_libraries(audio_processor_checks PRIVATE
    "${ORAN7_WINPTHREAD_LIBRARY}" "${ORAN7_SSP_LIBRARY}")

add_executable(audio_volume_ramp_checks volume-ramp.cpp
    "${ROOT}/src/cpp/Oran7MediaPlayer/Audio/Oran7AudioVolumeRamp.cpp")
target_include_directories(audio_volume_ramp_checks PRIVATE
    "${ROOT}/src/cpp/Oran7MediaPlayer/Audio")
target_link_libraries(audio_volume_ramp_checks PRIVATE
    "${ORAN7_WINPTHREAD_LIBRARY}" "${ORAN7_SSP_LIBRARY}")

add_executable(audio_effects_checks effects.cpp
    "${ROOT}/src/cpp/Oran7MediaClient/Oran7AudioEffects.cpp"
    "${ROOT}/src/cpp/Oran7MediaClient/Oran7EqualizerModel.cpp")
target_include_directories(audio_effects_checks PRIVATE
    "${ROOT}/src/cpp/Oran7MediaClient" "${ROOT}/src/cpp/Oran7Config"
    "${ROOT}/src/cpp/Oran7MediaPlayer")
target_link_libraries(audio_effects_checks PRIVATE Qt6::Core Qt6::Qml
    "${ORAN7_WINPTHREAD_LIBRARY}" "${ORAN7_SSP_LIBRARY}")

add_executable(equalizer_ui_checks equalizer-ui.cpp
    "${ROOT}/src/cpp/Oran7MediaClient/Oran7AudioEffects.cpp"
    "${ROOT}/src/cpp/Oran7MediaClient/Oran7EqualizerModel.cpp"
    "${ROOT}/src/cpp/QtQuicCppExtend/Oran7Api.cpp")
target_include_directories(equalizer_ui_checks PRIVATE "${ROOT}/src/cpp/Oran7MediaClient" "${ROOT}/src/cpp/QtQuicCppExtend")
target_compile_definitions(equalizer_ui_checks PRIVATE APP_ROOT="${ROOT}")
target_link_libraries(equalizer_ui_checks PRIVATE Qt6::Quick Qt6::Qml Qt6::Test
    "${ORAN7_WINPTHREAD_LIBRARY}" "${ORAN7_SSP_LIBRARY}")
