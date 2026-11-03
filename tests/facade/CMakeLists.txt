cmake_minimum_required(VERSION 3.16)
project(FacadeChecks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Network Qml Sql)
set(ROOT "${ORAN7_SOURCE_ROOT}")
add_executable(facade_checks main.cpp
    "${ROOT}/src/cpp/Oran7Config/Oran7AppConfig.cpp"
    "${ROOT}/src/cpp/Oran7MediaClient/Oran7ConfigBinding.cpp"
    "${ROOT}/src/cpp/Oran7MediaClient/Oran7TrackMetadataService.cpp"
    "${ROOT}/src/cpp/Oran7MediaClient/Oran7LegacyServerClient.cpp"
    "${ROOT}/src/cpp/Oran7MediaClient/Oran7PlaybackState.cpp"
    "${ROOT}/src/cpp/Oran7MediaClient/Oran7Track.h"
    "${ROOT}/src/cpp/Oran7MediaClient/Oran7NetworkSongResolver.cpp"
    "${ROOT}/src/ncm/api/NcmSongApi.cpp"
    "${ROOT}/src/ncm/api/Oran7NcmJsonMapper.cpp"
    "${ROOT}/src/ncm/network/CurlHttpClient.h")
target_include_directories(facade_checks PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${ROOT}/src/cpp/Oran7MediaClient" "${ROOT}/src/cpp/GlobalSingleton" "${ROOT}/src/ncm/api"
    "${ROOT}/src/ncm/network" "${ROOT}/src/ncm/protocol")
target_link_libraries(facade_checks PRIVATE Qt6::Core Qt6::Network Qt6::Qml Qt6::Sql)
