set(PLAYER "${ORAN7_SOURCE_ROOT}/src/cpp/Oran7MediaPlayer")
set(CLIENT "${ORAN7_SOURCE_ROOT}/src/cpp/Oran7MediaClient")
add_executable(audio_visualizer_checks main.cpp
    "${PLAYER}/Oran7AudioSpectrumAnalyzer.cpp"
    "${CLIENT}/Oran7AudioVisualizer.cpp" "${CLIENT}/Oran7AudioVisualizer.h"
    "${CLIENT}/Oran7SpectrumItem.cpp" "${CLIENT}/Oran7SpectrumItem.h")
set_target_properties(audio_visualizer_checks PROPERTIES AUTOMOC ON)
target_include_directories(audio_visualizer_checks PRIVATE "${PLAYER}" "${CLIENT}"
    "${ORAN7_SOURCE_ROOT}/src/cpp/Oran7Config" "${FFMPEG_ROOT_DIR}/include")
target_compile_definitions(audio_visualizer_checks PRIVATE APP_ROOT="${ORAN7_SOURCE_ROOT}")
target_link_libraries(audio_visualizer_checks PRIVATE Oran7Core Qt6::Quick Qt6::Test
    "${FFMPEG_ROOT_DIR}/lib/avutil.lib" "${ORAN7_WINPTHREAD_LIBRARY}" "${ORAN7_SSP_LIBRARY}")
