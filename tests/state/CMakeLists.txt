cmake_minimum_required(VERSION 3.16)
project(P1Checks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Concurrent)
set(APP_ROOT "${ORAN7_SOURCE_ROOT}")
add_executable(p1_checks checks.cpp
    "${APP_ROOT}/src/cpp/Oran7MediaClient/Oran7PlaybackState.cpp"
    "${APP_ROOT}/src/cpp/Oran7MediaClient/Oran7Track.h"
    "${APP_ROOT}/src/cpp/GlobalSingleton/Oran7Async.cpp"
    "${APP_ROOT}/src/cpp/GlobalSingleton/Oran7AsyncManager.cpp")
target_include_directories(p1_checks PRIVATE "${APP_ROOT}/src/cpp/Oran7MediaClient" "${APP_ROOT}/src/cpp/GlobalSingleton" "${APP_ROOT}/src/ncm/api")
target_link_libraries(p1_checks PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Concurrent)
