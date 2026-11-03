cmake_minimum_required(VERSION 3.16)
project(CaptureBridgeChecks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)
set(ROOT "${ORAN7_SOURCE_ROOT}")
add_executable(capture_bridge_checks main.cpp Oran7ScreenCapture.h
    "${ROOT}/src/cpp/Oran7MediaClient/Oran7ScreenCaptureBridge.h"
    "${ROOT}/src/cpp/Oran7MediaClient/Oran7ScreenCaptureBridge.cpp")
target_include_directories(capture_bridge_checks PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${ROOT}/src/cpp/Oran7MediaClient")
target_link_libraries(capture_bridge_checks PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
