cmake_minimum_required(VERSION 3.16)
project(CaptureRecoveryChecks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick Qml Test Network WebSockets)
set(ROOT "${ORAN7_SOURCE_ROOT}")
set(CAP "${ROOT}/src/cpp/Oran7ScreenCapture")
# 直接取生产控制器代码，替换硬件 worker 实现；不启动真实桌面采集或修改系统分辨率。
file(READ "${CAP}/Oran7ScreenCapture.cpp" CAPTURE_SOURCE)
set(CONTROLLER_SOURCE "${CAPTURE_SOURCE}")
# 仅替换硬件枚举入口，重映射/校验/恢复逻辑仍直接使用生产代码。
string(FIND "${CONTROLLER_SOURCE}" "QVariantList Oran7ScreenCaptureController::monitors() const" MONITORS_START)
string(FIND "${CONTROLLER_SOURCE}" "QVariantMap Oran7ScreenCaptureController::captureSettings() const" MONITORS_END)
string(SUBSTRING "${CONTROLLER_SOURCE}" 0 ${MONITORS_START} BEFORE_MONITORS)
string(SUBSTRING "${CONTROLLER_SOURCE}" ${MONITORS_END} -1 AFTER_MONITORS)
set(CONTROLLER_SOURCE "${BEFORE_MONITORS}extern QVariantList oran7FixtureMonitors();\nQVariantList Oran7ScreenCaptureController::monitors() const { return oran7FixtureMonitors(); }\n${AFTER_MONITORS}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/controller.cpp" "${CONTROLLER_SOURCE}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CAP}/Oran7ScreenCapture.cpp")
add_executable(recovery_checks main.cpp "${CMAKE_CURRENT_BINARY_DIR}/controller.cpp"
    "${CAP}/Oran7ScreenCapture.h" "${CAP}/Oran7DesktopCaptureWorker.h"
    "${CAP}/Oran7CaptureFrameConverter.cpp" "${CAP}/Oran7RecordingSession.cpp"
    "${CAP}/Oran7SignalingClient.cpp"
    stubs/D3d11DeviceProvider.h stubs/D3d11Videoitem.h)
target_include_directories(recovery_checks PRIVATE stubs "${CAP}" "${ROOT}/src/cpp/Oran7Config" "${FFMPEG_ROOT_DIR}/include")
target_link_libraries(recovery_checks PRIVATE Qt6::Core Qt6::Gui Qt6::Quick Qt6::Test Qt6::Network Qt6::WebSockets
    "${FFMPEG_ROOT_DIR}/lib/avutil.lib" "${FFMPEG_ROOT_DIR}/lib/swscale.lib" "${FFMPEG_ROOT_DIR}/lib/avfilter.lib" d3d11 dxgi)
