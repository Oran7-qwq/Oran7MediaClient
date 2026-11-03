add_executable(localization_checks main.cpp icons.qrc)
target_include_directories(localization_checks PRIVATE "${ORAN7_SOURCE_ROOT}/src/cpp/QtQuicCppExtend"
    "${ORAN7_SOURCE_ROOT}/src/cpp/Oran7Utils")
target_compile_definitions(localization_checks PRIVATE APP_ROOT="${ORAN7_SOURCE_ROOT}"
    ORAN7_QML_IMPORT_ROOT="${PROJECT_BINARY_DIR}" ORAN7_LUPDATE_TOOL="$<TARGET_FILE:Qt6::lupdate>")
target_link_libraries(localization_checks PRIVATE Oran7Core Oran7UI_Impl Qt6::Quick Qt6::Qml Qt6::Test)
add_dependencies(localization_checks Oran7UI_Implplugin)
