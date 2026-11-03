add_executable(theme_checks main.cpp)
target_include_directories(theme_checks PRIVATE
    "${ORAN7_SOURCE_ROOT}/src/cpp/QtQuicCppExtend"
    "${ORAN7_SOURCE_ROOT}/src/cpp/GlobalSingleton"
    "${ORAN7_SOURCE_ROOT}/src/cpp/Oran7Utils")
target_compile_definitions(theme_checks PRIVATE ORAN7_QML_IMPORT_ROOT="${PROJECT_BINARY_DIR}")
target_link_libraries(theme_checks PRIVATE Oran7UI_Impl Oran7Core Qt6::Quick Qt6::Qml Qt6::Test)
add_dependencies(theme_checks Oran7UI_Implplugin)
