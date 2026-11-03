cmake_minimum_required(VERSION 3.16)
project(P17Checks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Network Test)
set(ROOT "${ORAN7_SOURCE_ROOT}")
set(SERVICE "${ROOT}/src/cpp/Oran7MediaClient")
add_executable(policy_checks main.cpp
 "${SERVICE}/Oran7CloudMusicService.cpp" "${SERVICE}/Oran7CloudMusicService.h"
 "${SERVICE}/Oran7PlaybackViewModel.cpp" "${SERVICE}/Oran7PlaybackViewModel.h"
 "${SERVICE}/Oran7RoamFilterStrategy.cpp" "${SERVICE}/Oran7RoamFilterStrategy.h"
 "${SERVICE}/Oran7PlaybackState.cpp" "${SERVICE}/Oran7PlaybackState.h"
 "${SERVICE}/Oran7TrackListModel.cpp" "${SERVICE}/Oran7TrackListModel.h"
 "${SERVICE}/Oran7TrackLibrary.cpp" "${SERVICE}/Oran7TrackLibrary.h" "${SERVICE}/Oran7Track.h")
target_include_directories(policy_checks PRIVATE "${SERVICE}" "${ROOT}/src/cpp/GlobalSingleton")
target_link_libraries(policy_checks PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test
 Oran7Core)
set(CMAKE_AUTORCC ON)
get_target_property(POLICY_SOURCES policy_checks SOURCES)
list(REMOVE_ITEM POLICY_SOURCES main.cpp)
add_executable(playlist_qml_checks qml.cpp icons.qrc ${POLICY_SOURCES})
target_compile_definitions(playlist_qml_checks PRIVATE APP_ROOT="${ROOT}")
target_include_directories(playlist_qml_checks PRIVATE "${SERVICE}" "${ROOT}/src/cpp/GlobalSingleton")
target_link_libraries(playlist_qml_checks PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test
 Oran7Core)

add_executable(cover_cache_checks cover-cache.cpp
 "${SERVICE}/Oran7CoverNetworkFactory.cpp" "${SERVICE}/Oran7CoverNetworkFactory.h")
target_include_directories(cover_cache_checks PRIVATE "${SERVICE}")
target_link_libraries(cover_cache_checks PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Test)
