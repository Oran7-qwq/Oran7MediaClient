set(CAP "${ORAN7_SOURCE_ROOT}/src/cpp/Oran7ScreenCapture")
add_executable(capture_conversion_checks main.cpp
    "${CAP}/Oran7CaptureFrameConverter.cpp"
    "${CAP}/Oran7DesktopCaptureWorker.cpp" "${CAP}/Oran7DesktopCaptureWorker.h"
    "${CAP}/Oran7VideoEncoder.cpp")
target_include_directories(capture_conversion_checks PRIVATE "${CAP}" "${FFMPEG_ROOT_DIR}/include")
target_link_libraries(capture_conversion_checks PRIVATE Oran7Core Qt6::Gui Qt6::Test
    "${FFMPEG_ROOT_DIR}/lib/avutil.lib" "${FFMPEG_ROOT_DIR}/lib/avcodec.lib"
    "${FFMPEG_ROOT_DIR}/lib/avfilter.lib" "${FFMPEG_ROOT_DIR}/lib/swscale.lib" d3d11 dxgi ole32)
