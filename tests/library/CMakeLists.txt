cmake_minimum_required(VERSION 3.16)
project(LibraryChecks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Concurrent Qml)
set(ROOT "${ORAN7_SOURCE_ROOT}")
set(SERVICE "${ROOT}/src/cpp/Oran7MediaClient")
add_executable(library_checks main.cpp
 "${SERVICE}/Oran7TrackListModel.cpp" "${SERVICE}/Oran7TrackListModel.h"
 "${SERVICE}/Oran7TrackLibrary.cpp" "${SERVICE}/Oran7TrackLibrary.h"
 "${SERVICE}/Oran7Track.h" "${SERVICE}/Oran7LocalLibraryService.cpp" "${SERVICE}/Oran7LocalLibraryService.h" "${SERVICE}/Oran7MediaInfoReader.cpp" "${ROOT}/src/cpp/GlobalSingleton/Oran7Async.cpp")
target_include_directories(library_checks PRIVATE "${SERVICE}" "${ROOT}/src/cpp/GlobalSingleton")
target_link_libraries(library_checks PRIVATE Qt6::Core Qt6::Gui Qt6::Concurrent Qt6::Qml)
