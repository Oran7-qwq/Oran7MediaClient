cmake_minimum_required(VERSION 3.16)
project(LyricsChecks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Network Concurrent)
set(ROOT "${ORAN7_SOURCE_ROOT}")
set(SERVICE "${ROOT}/src/cpp/Oran7MediaClient")
add_executable(lyrics_checks main.cpp "${SERVICE}/Oran7LyricsService.cpp" "${SERVICE}/Oran7LyricsService.h" "${ROOT}/src/cpp/GlobalSingleton/Oran7OnlineLyricsFetcher.h" "${ROOT}/src/cpp/GlobalSingleton/Oran7Async.cpp")
target_include_directories(lyrics_checks PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${SERVICE}" "${ROOT}/src/cpp/GlobalSingleton")
target_link_libraries(lyrics_checks PRIVATE Qt6::Core Qt6::Network Qt6::Concurrent)

add_executable(lyrics_island_checks island.cpp
 "${ROOT}/src/cpp/GlobalSingleton/Oran7LyricsModel.cpp"
 "${ROOT}/src/cpp/GlobalSingleton/Oran7LyricsModel.h"
 "${ROOT}/src/cpp/GlobalSingleton/Oran7LrcParser.cpp"
 "${ROOT}/src/cpp/QtQuicCppExtend/Oran7Api.cpp"
 "${ROOT}/src/cpp/QtQuicCppExtend/Oran7Api.h")
target_include_directories(lyrics_island_checks PRIVATE "${ROOT}/src/cpp/GlobalSingleton" "${ROOT}/src/cpp/QtQuicCppExtend")
target_compile_definitions(lyrics_island_checks PRIVATE APP_ROOT="${ROOT}")
target_link_libraries(lyrics_island_checks PRIVATE Oran7Core Qt6::Quick Qt6::Qml Qt6::Test)
