cmake_minimum_required(VERSION 3.16)

project(Oran7MediaClient VERSION 0.7 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找Qt6组件
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Quick
    QuickControls2
    Qml
    Sql
    OpenGL
    OpenGLWidgets
    ShaderTools
    Multimedia
)

# 设置 Qt 策略以避免 QTP0001 警告
qt_policy(SET QTP0001 NEW)

# 3rdparty QWindowKit (QWKQuick) 配置 - 参考 HuskarUI 方法
set(QWINDOWKIT_BUILD_STATIC ON)
set(QWINDOWKIT_BUILD_WIDGETS OFF)
set(QWINDOWKIT_BUILD_QUICK ON)
set(QWINDOWKIT_INSTALL OFF)
add_subdirectory(3rdparty/qwindowkit)#QWindowKit 使用 add_subdirectory 方式构建

# 添加全局单例 GlobalSingleton 配置路径
set(GLOBAL_SINGLETON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/GlobalSingleton")

# 添加FFmpeg路径配置
set(FFMPEG_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg6.0")
#添加Oran7MediaPlayer模块配置路径
set(ORAN7_PLAYER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/Oran7MediaPlayer")
#添加Oran7ScreenPlayer模块配置路径
set(ORAN7_SCREEN_CAPTURE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/Oran7ScreenCapture")
#添加Oran7VideoGPURender模块配置路径
set(ORAN7_VIDEO_GPU_RENDER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/Oran7VideoGPURender")
#添加Oran7Sever模块配置路径
set(ORAN7_SEVER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/Oran7Sever")
#添加QtQuicCppExtend模块配置路径
set(QtQuicCppExtend_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/QtQuicCppExtend")

set(PROJECT_MAIN_SOURCES
    main.cpp
    qml.qrc
    res.qrc
)

file(GLOB_RECURSE SOURCES
    "src/cpp/*.cpp"
    "src/cpp/*.h"
)

# 创建 QML 模块用于导出 C++ 类
qt_add_qml_module(Oran7UI_Impl
    URI "Oran7UI.Impl"
    VERSION 1.0
    DESIGNER_SUPPORTED
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Oran7UI/Impl"
    QML_FILES
        # 这里可以添加需要导出的 QML 文件
    SOURCES
        src/cpp/QtQuicCppExtend/Oran7WindowAgent.h
        src/cpp/QtQuicCppExtend/Oran7WindowAgent.cpp
        src/cpp/QtQuicCppExtend/Oran7Api.h
        src/cpp/QtQuicCppExtend/Oran7Api.cpp
)

# 为 QML 模块添加包含目录
target_include_directories(Oran7UI_Impl
    PRIVATE
        ${QtQuicCppExtend_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/qwindowkit/src/quick
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/qwindowkit/src/core
)

# 为 QML 模块链接必要的库
target_link_libraries(Oran7UI_Impl
    PRIVATE
        Qt6::Quick
        Qt6::Qml
        QWKCore
        QWKQuick
)

# 添加 QWindowKit 宏定义
add_compile_definitions(
    QWK_QUICK_LIBRARY=1
    QWK_CORE_LIBRARY=1
)

qt_add_executable(Oran7MediaClient
    MANUAL_FINALIZATION
    ${PROJECT_MAIN_SOURCES}
    ${SOURCES}
)

# ===================== ShaderEffect 自动编译 QSB ================
qt_add_shaders(Oran7MediaClient "shaders"
    PREFIX "/"
    FILES
        shaders/video.vert
        shaders/video.frag
)
# =======================================================

# ===================== 运行时依赖自动拷贝 =====================
if(WIN32)
    # ffmpeg/bin 目录
    set(FFMPEG_BIN_DIR "${FFMPEG_ROOT_DIR}/bin")

    # SQLite 目录
    set(SQLITE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SQLite")

    # exe 输出目录（Debug/Release）
    set(APP_OUT_DIR "$<TARGET_FILE_DIR:Oran7MediaClient>")

    add_custom_command(TARGET Oran7MediaClient POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Copying runtime deps to: ${APP_OUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${APP_OUT_DIR}"

        #拷贝 ffmpeg6.0/bin 下所有内容到 exe 目录
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${FFMPEG_BIN_DIR}" "${APP_OUT_DIR}"

        #拷贝 SQLite 文件夹到 exe 目录
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${SQLITE_DIR}" "${APP_OUT_DIR}/SQLite"
    )
endif()
# ========================================================

# 在qt_add_executable之后添加模块配置文件路径
target_include_directories(Oran7MediaClient
    PRIVATE
    ${GLOBAL_SINGLETON_DIR}

    ${FFMPEG_ROOT_DIR}/include
    ${FFMPEG_ROOT_DIR}/include/SDL2
    ${FFMPEG_ROOT_DIR}/lib

    ${ORAN7_PLAYER_DIR}
    ${ORAN7_SCREEN_CAPTURE_DIR}
    ${ORAN7_VIDEO_GPU_RENDER_DIR}
    ${ORAN7_SEVER_DIR}
    ${QtQuicCppExtend_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# FFMPEG库目录
target_link_directories(Oran7MediaClient
    PRIVATE
        ${FFMPEG_ROOT_DIR}/lib
)

# target_link_libraries，添加 Sql 和 QuickControls2 模块
target_link_libraries(Oran7MediaClient
  PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::Sql
    Qt6::QuickControls2
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::ShaderTools
    Qt6::Multimedia
    Qt6::GuiPrivate
    # QWindowKit 库（参考 HuskarUI 方法）
    QWKCore
    QWKQuick
    # 链接创建的 QML 模块库
    Oran7UI_Impl
    # FFmpeg 库
    avcodec avformat avutil avdevice avfilter swresample swscale
    # SDL2 库
    SDL2 SDL2main
    # Windows 系统库
    d3dcompiler dxgi dwmapi
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
if(Qt6_VERSION VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.Oran7MediaClient)
endif()

set_target_properties(Oran7MediaClient PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS Oran7MediaClient
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(QT_VERSION_MAJOR EQUAL 6)
    qt_import_qml_plugins(Oran7MediaClient)
    qt_finalize_executable(Oran7MediaClient)
endif()
