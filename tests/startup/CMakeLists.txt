set(STARTUP "${ORAN7_SOURCE_ROOT}/src/cpp/Oran7Startup")
add_executable(startup_checks main.cpp "${STARTUP}/Oran7StartupCoordinator.cpp" "${STARTUP}/Oran7StartupCoordinator.h")
target_include_directories(startup_checks PRIVATE "${STARTUP}")
target_compile_definitions(startup_checks PRIVATE APP_ROOT="${ORAN7_SOURCE_ROOT}")
target_link_libraries(startup_checks PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)

add_executable(background_checks background.cpp "${STARTUP}/Oran7BackgroundFrameMonitor.cpp" "${STARTUP}/Oran7BackgroundFrameMonitor.h")
target_include_directories(background_checks PRIVATE "${STARTUP}")
target_compile_definitions(background_checks PRIVATE APP_ROOT="${ORAN7_SOURCE_ROOT}")
target_link_libraries(background_checks PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Multimedia Qt6::Test)

add_executable(startup_cancel_checks cancel.cpp "${STARTUP}/Oran7StartupCoordinator.cpp" "${STARTUP}/Oran7StartupCoordinator.h")
target_include_directories(startup_cancel_checks PRIVATE "${STARTUP}")
target_compile_definitions(startup_cancel_checks PRIVATE APP_ROOT="${ORAN7_SOURCE_ROOT}")
target_link_libraries(startup_cancel_checks PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)

foreach(target startup_checks startup_cancel_checks)
    target_sources(${target} PRIVATE "${STARTUP}/Oran7StartupActivityItem.cpp" "${STARTUP}/Oran7StartupActivityItem.h"
        "${STARTUP}/Oran7StartupActivityBackend.cpp" "${STARTUP}/Oran7StartupActivityBackend.h")
    target_link_libraries(${target} PRIVATE d3d11 dcomp dxgi user32)
endforeach()

add_executable(native_activity_checks native-activity.cpp "${STARTUP}/Oran7StartupActivityBackend.cpp")
target_include_directories(native_activity_checks PRIVATE "${STARTUP}")
target_link_libraries(native_activity_checks PRIVATE Qt6::Core Qt6::Gui d3d11 dcomp dxgi user32 ole32)

add_executable(window_minimum_checks window-minimum.cpp "${STARTUP}/Oran7StartupCoordinator.cpp" "${STARTUP}/Oran7StartupCoordinator.h"
    "${ORAN7_SOURCE_ROOT}/src/cpp/QtQuicCppExtend/Oran7WindowAgent.cpp" "${ORAN7_SOURCE_ROOT}/src/cpp/QtQuicCppExtend/Oran7WindowAgent.h")
target_include_directories(window_minimum_checks PRIVATE "${STARTUP}" "${ORAN7_SOURCE_ROOT}/src/cpp/QtQuicCppExtend")
target_link_libraries(window_minimum_checks PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test QWKQuick user32)
