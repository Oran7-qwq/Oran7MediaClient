# 所有默认检查均离线：替身网络/设备、临时文件、离屏 QML，不启动主程序。
set(ORAN7_SOURCE_ROOT "${PROJECT_SOURCE_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
find_package(Qt6 REQUIRED COMPONENTS Test)

# 每次 CTest 都由同一个运行脚本注入实际依赖位置，不引用旧 Release 目录。
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/run-check-$<CONFIG>.cmake" CONTENT
"cmake_minimum_required(VERSION 3.16)
set(ENV{PATH} \"$<TARGET_FILE_DIR:Oran7Core>;$<TARGET_FILE_DIR:Qt6::Core>;${FFMPEG_ROOT_DIR}/bin;${ORAN7_MSYS2_ROOT}/bin;\$ENV{PATH}\")
set(ENV{QT_QPA_PLATFORM} offscreen)
set(ENV{QT_QUICK_BACKEND} software)
set(ENV{QT_QUICK_CONTROLS_STYLE} Fusion)
execute_process(COMMAND \"\${ORAN7_TEST_PROGRAM}\" WORKING_DIRECTORY \"${ORAN7_SOURCE_ROOT}\" RESULT_VARIABLE result)
if(NOT result STREQUAL \"0\")
    message(FATAL_ERROR \"离线检查失败：\${ORAN7_TEST_PROGRAM}，结果：\${result}\")
endif()
")

function(oran7_register_checks suite)
    get_property(checks DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${suite}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(check IN LISTS checks)
        get_target_property(kind "${check}" TYPE)
        if(kind STREQUAL "EXECUTABLE")
            set(artifacts "${CMAKE_CURRENT_BINARY_DIR}/artifacts/${check}")
            file(MAKE_DIRECTORY "${artifacts}")
            target_compile_definitions("${check}" PRIVATE ORAN7_TEST_ARTIFACT_DIR="${artifacts}")
            add_test(NAME "oran7.${suite}.${check}"
                COMMAND "${CMAKE_COMMAND}" "-DORAN7_TEST_PROGRAM=$<TARGET_FILE:${check}>"
                -P "${CMAKE_CURRENT_BINARY_DIR}/run-check-$<CONFIG>.cmake")
            set_tests_properties("oran7.${suite}.${check}" PROPERTIES
                LABELS "offline;${suite}" TIMEOUT 120 RUN_SERIAL TRUE)
        endif()
    endforeach()
endfunction()

foreach(suite IN ITEMS network state track-model ncm transport library lyrics resolver playback
        facade render-bridge render-sync capture-bridge config recording-workspace playlist settings-animation
        startup capture-recovery capture-conversion recording pure qml-module logging credentials theme localization)
    add_subdirectory("${suite}")
    oran7_register_checks("${suite}")
endforeach()

add_test(NAME oran7.resources.qml_imports
    COMMAND "${CMAKE_COMMAND}" "-DORAN7_SOURCE_ROOT=${ORAN7_SOURCE_ROOT}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/resources/check-imports.cmake")
set_tests_properties(oran7.resources.qml_imports PROPERTIES LABELS "offline;resources" TIMEOUT 30)
