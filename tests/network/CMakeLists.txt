cmake_minimum_required(VERSION 3.16)
project(P0Checks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Concurrent)
set(APP_ROOT "${ORAN7_SOURCE_ROOT}")
add_executable(p0_network_checks
    network_checks.cpp
    "${APP_ROOT}/src/ncm/api/NcmSongApi.cpp"
    "${APP_ROOT}/src/ncm/api/Oran7NcmJsonMapper.cpp"
    "${APP_ROOT}/src/ncm/api/NcmSongApi.h"
    "${APP_ROOT}/src/ncm/network/CurlHttpClient.h"
    "${APP_ROOT}/src/cpp/GlobalSingleton/Oran7Async.cpp")
target_include_directories(p0_network_checks PRIVATE
    "${APP_ROOT}/src/ncm/api"
    "${APP_ROOT}/src/ncm/network"
    "${APP_ROOT}/src/ncm/protocol"
    "${APP_ROOT}/src/cpp/GlobalSingleton")
target_link_libraries(p0_network_checks PRIVATE Qt6::Core Qt6::Gui Qt6::Concurrent)
