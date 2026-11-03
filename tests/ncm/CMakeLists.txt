cmake_minimum_required(VERSION 3.16)
project(NcmChecks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Test)
set(ROOT "${ORAN7_SOURCE_ROOT}")
set(API "${ROOT}/src/ncm/api")
add_executable(ncm_checks main.cpp "${API}/Oran7NcmJsonMapper.cpp" "${API}/NcmAsyncHandlers.cpp"
 "${API}/NcmSongApi.cpp" "${API}/NcmPlaylistApi.cpp" "${API}/NcmUserApi.cpp"
 "${ROOT}/src/ncm/network/CurlHttpClient.h")
target_include_directories(ncm_checks PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${API}"
 "${ROOT}/src/ncm/network" "${ROOT}/src/ncm/protocol")
target_link_libraries(ncm_checks PRIVATE Qt6::Core Qt6::Test)
