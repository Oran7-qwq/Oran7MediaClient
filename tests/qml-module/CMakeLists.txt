add_executable(qml_module_checks main.cpp)
target_include_directories(qml_module_checks PRIVATE
    "${ORAN7_SOURCE_ROOT}/src/cpp/QtQuicCppExtend"
    "${ORAN7_SOURCE_ROOT}/src/cpp/GlobalSingleton"
    "${ORAN7_SOURCE_ROOT}/src/cpp/Oran7Utils")
target_compile_definitions(qml_module_checks PRIVATE ORAN7_QML_IMPORT_ROOT="${PROJECT_BINARY_DIR}")
target_link_libraries(qml_module_checks PRIVATE Oran7UI_Impl Oran7Core Qt6::Quick Qt6::Qml)
add_dependencies(qml_module_checks Oran7UI_Implplugin)
