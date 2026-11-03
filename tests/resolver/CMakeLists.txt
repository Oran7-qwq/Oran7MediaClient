cmake_minimum_required(VERSION 3.16)
project(ResolverChecks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Concurrent)
set(ROOT "${ORAN7_SOURCE_ROOT}")
add_executable(resolver_checks main.cpp
    "${ROOT}/src/cpp/Oran7MediaClient/Oran7NetworkSongResolver.cpp"
    "${ROOT}/src/cpp/Oran7MediaClient/Oran7NetworkSongResolver.h"
    "${ROOT}/src/ncm/api/NcmSongApi.cpp"
    "${ROOT}/src/ncm/api/Oran7NcmJsonMapper.cpp"
    "${ROOT}/src/ncm/api/NcmSongApi.h"
    "${ROOT}/src/ncm/network/CurlHttpClient.h"
    "${ROOT}/src/cpp/GlobalSingleton/Oran7Async.cpp")
target_include_directories(resolver_checks PRIVATE
    "${ROOT}/src/cpp/Oran7MediaClient" "${ROOT}/src/cpp/GlobalSingleton"
    "${ROOT}/src/ncm/api" "${ROOT}/src/ncm/network" "${ROOT}/src/ncm/protocol")
target_link_libraries(resolver_checks PRIVATE Qt6::Core Qt6::Gui Qt6::Concurrent)
