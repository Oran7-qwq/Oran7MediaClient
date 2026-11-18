cmake_minimum_required(VERSION 3.16)
project(RecordingPageChecks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Test)
set(ROOT "${ORAN7_SOURCE_ROOT}")
add_executable(page_checks main.cpp icons.qrc
    "${ROOT}/src/cpp/Oran7MediaClient/Oran7ProcessLoadMonitor.cpp"
    "${ROOT}/src/cpp/Oran7MediaClient/Oran7ProcessLoadMonitor.h"
    "${ROOT}/src/cpp/QtQuicCppExtend/Oran7PointerParallax.cpp"
    "${ROOT}/src/cpp/QtQuicCppExtend/Oran7PointerParallax.h")
target_include_directories(page_checks PRIVATE
    "${ROOT}/src/cpp/GlobalSingleton"
    "${ROOT}/src/cpp/Oran7MediaClient"
    "${ROOT}/src/cpp/QtQuicCppExtend")
target_compile_definitions(page_checks PRIVATE APP_ROOT="${ROOT}")
target_link_libraries(page_checks PRIVATE Oran7Core Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test pdh)
