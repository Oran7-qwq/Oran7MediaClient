add_executable(logging_checks main.cpp
    "${ORAN7_SOURCE_ROOT}/src/cpp/Oran7Utils/Oran7FFmpegLogBridge.cpp")
target_include_directories(logging_checks PRIVATE
    "${ORAN7_SOURCE_ROOT}/src/cpp/GlobalSingleton"
    "${ORAN7_SOURCE_ROOT}/src/cpp/Oran7Utils"
    "${FFMPEG_ROOT_DIR}/include")
target_link_libraries(logging_checks PRIVATE Oran7Core Qt6::Core Qt6::Qml
    "${FFMPEG_ROOT_DIR}/lib/avutil.lib")
