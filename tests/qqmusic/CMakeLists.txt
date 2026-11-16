set(QQ_SERVICE "${ORAN7_SOURCE_ROOT}/src/cpp/Oran7MediaClient/qqmusic")
add_executable(qqmusic_login_checks main.cpp
    "${QQ_SERVICE}/Oran7QQMusicAuth.cpp" "${QQ_SERVICE}/Oran7QQMusicAuth.h"
    "${QQ_SERVICE}/Oran7QQMusicWebLogin.cpp" "${QQ_SERVICE}/Oran7QQMusicWebLogin.h"
    "${QQ_SERVICE}/Oran7QQMusicSession.cpp" "${QQ_SERVICE}/Oran7QQMusicSession.h")
target_include_directories(qqmusic_login_checks PRIVATE "${QQ_SERVICE}"
    "${ORAN7_SOURCE_ROOT}/src/cpp/GlobalSingleton")
target_compile_definitions(qqmusic_login_checks PRIVATE APP_ROOT="${ORAN7_SOURCE_ROOT}")
target_link_libraries(qqmusic_login_checks PRIVATE Oran7Core Qt6::Core Qt6::Network Qt6::WebSockets Qt6::Test)

get_target_property(QQ_LOGIN_SOURCES qqmusic_login_checks SOURCES)
list(REMOVE_ITEM QQ_LOGIN_SOURCES main.cpp)
set(QQ_CLIENT "${ORAN7_SOURCE_ROOT}/src/cpp/Oran7MediaClient")
add_executable(qqmusic_business_checks business.cpp ${QQ_LOGIN_SOURCES}
    "${QQ_SERVICE}/Oran7QQMusicApi.cpp" "${QQ_SERVICE}/Oran7QQMusicApi.h"
    "${QQ_SERVICE}/Oran7QQMusicService.cpp" "${QQ_SERVICE}/Oran7QQMusicService.h"
    "${QQ_SERVICE}/Oran7QQMusicWebProtocol.cpp" "${QQ_SERVICE}/Oran7QQMusicWebProtocol.h"
    "${QQ_CLIENT}/Oran7TrackLibrary.cpp" "${QQ_CLIENT}/Oran7TrackLibrary.h"
    "${QQ_CLIENT}/Oran7TrackListModel.cpp" "${QQ_CLIENT}/Oran7TrackListModel.h"
    "${QQ_CLIENT}/Oran7Track.h" "${QQ_CLIENT}/Oran7PlaybackState.cpp"
    "${QQ_CLIENT}/Oran7PlaybackViewModel.cpp" "${QQ_CLIENT}/Oran7CloudMusicService.cpp"
    "${QQ_CLIENT}/Oran7RoamFilterStrategy.cpp")
target_include_directories(qqmusic_business_checks PRIVATE "${QQ_SERVICE}" "${QQ_CLIENT}"
    "${ORAN7_SOURCE_ROOT}/src/cpp/GlobalSingleton")
target_link_libraries(qqmusic_business_checks PRIVATE Oran7Core Qt6::Core Qt6::Network Qt6::WebSockets Qt6::Test bcrypt)
