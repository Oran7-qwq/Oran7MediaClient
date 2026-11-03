cmake_minimum_required(VERSION 3.16)
project(TrackModelChecks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Test)
set(ROOT "${ORAN7_SOURCE_ROOT}")
set(SERVICE "${ROOT}/src/cpp/Oran7MediaClient")
add_executable(track_model_checks main.cpp
 "${SERVICE}/Oran7PlaybackState.cpp"
 "${SERVICE}/Oran7TrackListModel.cpp" "${SERVICE}/Oran7TrackListModel.h"
 "${SERVICE}/Oran7TrackLibrary.cpp" "${SERVICE}/Oran7TrackLibrary.h" "${SERVICE}/Oran7Track.h")
target_include_directories(track_model_checks PRIVATE "${SERVICE}")
target_compile_definitions(track_model_checks PRIVATE APP_ROOT="${ROOT}")
target_link_libraries(track_model_checks PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
