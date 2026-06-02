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
    QuickTemplates2
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
# 添加 QWindowKit 宏定义
add_compile_definitions(
    QWK_QUICK_LIBRARY=1
    QWK_CORE_LIBRARY=1
)

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
#添加Oran7Utils模块配置路径
set(ORAN7_UTILS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/Oran7Utils")

set(PROJECT_MAIN_SOURCES
    main.cpp
    qml.qrc res.qrc
)

file(GLOB_RECURSE CORE_SOURCES "src/cpp/*.cpp" "src/cpp/*.h")
# 排除已移至 Oran7Core 共享库的单例文件（避免 EXE/DLL 各自编译一份）
list(FILTER CORE_SOURCES EXCLUDE REGEX "src/cpp/GlobalSingleton/(GlobalHelper|AppJsonConfigManager|Oran7ThemeProfileManager)\\.")
# 排除 QtQuicCppExtend 目录（用于 QML 模块）
#list(FILTER CORE_SOURCES EXCLUDE REGEX "src/cpp/QtQuicCppExtend/.*")


# ======== Oran7Core 共享库（单例唯一所在，EXE 和 QML 插件共享）========
set(ORAN7_CORE_SOURCES
    src/cpp/GlobalSingleton/GlobalHelper.h
    src/cpp/GlobalSingleton/GlobalHelper.cpp
    src/cpp/GlobalSingleton/AppJsonConfigManager.h
    src/cpp/GlobalSingleton/AppJsonConfigManager.cpp
    src/cpp/GlobalSingleton/Oran7ThemeProfileManager.h
    src/cpp/GlobalSingleton/Oran7ThemeProfileManager.cpp
)

qt_add_library(Oran7Core SHARED ${ORAN7_CORE_SOURCES})

target_include_directories(Oran7Core
    PUBLIC
        ${GLOBAL_SINGLETON_DIR}
    PRIVATE
        ${QtQuicCppExtend_DIR}
        ${ORAN7_UTILS_DIR}
)

target_link_libraries(Oran7Core PUBLIC Qt6::Core Qt6::Gui Qt6::Qml)

set_target_properties(Oran7Core PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)


# ======== 创建 QML 模块用于导出 C++ 类 ======
qt_add_qml_module(Oran7UI_Impl
    URI "Oran7UI.Impl"
    VERSION 1.0
    DESIGNER_SUPPORTED
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Oran7UI/Impl"
    QML_FILES
        # 这里可以添加需要导出的 QML 文件
    RESOURCES
        # 添加主题 JSON 资源到 QML 模块
        # src/resource/themeJson/Index.json
    SOURCES
        src/cpp/QtQuicCppExtend/Oran7WindowAgent.h
        src/cpp/QtQuicCppExtend/Oran7WindowAgent.cpp
        src/cpp/QtQuicCppExtend/Oran7Api.h
        src/cpp/QtQuicCppExtend/Oran7Api.cpp
        src/cpp/QtQuicCppExtend/Oran7Theme.h
        src/cpp/QtQuicCppExtend/Oran7Theme.cpp
        src/cpp/QtQuicCppExtend/Oran7FileHelper.h
        src/cpp/QtQuicCppExtend/Oran7FileHelper.cpp
        src/cpp/QtQuicCppExtend/Oran7Theme_p.h
        src/cpp/QtQuicCppExtend/Oran7ThemeFunctions.h
        src/cpp/QtQuicCppExtend/Oran7ThemeFunctions.cpp
        src/cpp/QtQuicCppExtend/Oran7ColorGenerator.h
        src/cpp/QtQuicCppExtend/Oran7ColorGenerator.cpp
        src/cpp/QtQuicCppExtend/Oran7RadiusGenerator.h
        src/cpp/QtQuicCppExtend/Oran7RadiusGenerator.cpp
        src/cpp/QtQuicCppExtend/Oran7SizeGenerator.h
        src/cpp/QtQuicCppExtend/Oran7SizeGenerator.cpp
)

qt_add_resources(Oran7UI_Impl "resource"
      PREFIX "/"
      FILES
          config/themeJson/Index.json
          config/themeJson/Oran7MainGUI.json
          config/themeJson/Oran7CaptionBar.json
          config/themeJson/Oran7ProgressSlider.json
          config/themeJson/Oran7MusicPlaylistView.json
          config/themeJson/Oran7MusicPlayControls.json

          config/default.json
)

# 为 QML 模块添加包含目录
target_include_directories(Oran7UI_Impl
    PRIVATE
        ${GLOBAL_SINGLETON_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/qwindowkit/src/quick
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/qwindowkit/src/core
        ${QtQuicCppExtend_DIR}
        ${ORAN7_UTILS_DIR}
)
# 为 QML 模块链接必要的库
target_link_libraries(Oran7UI_Impl
    PRIVATE
        Qt6::Quick
        Qt6::Qml
        QWKCore
        QWKQuick
        Oran7Core
)

#==================< qt_add_executable：Oran7MediaClient >============================

qt_add_executable(Oran7MediaClient
    MANUAL_FINALIZATION
    ${PROJECT_MAIN_SOURCES}
    ${CORE_SOURCES}
)

# ===================== ShaderEffect 自动编译 QSB ================
qt_add_shaders(Oran7MediaClient "shaders"
    PREFIX "/"
    FILES
        shaders/video.vert
        shaders/video.frag
)
# =======================================================

# ======== 配置资源直接嵌入 EXE（兜底 DLL 的 QRC 注册失败）=========
qt_add_resources(Oran7MediaClient "config_resource"
      PREFIX "/"
      FILES
          config/themeJson/Index.json
          config/themeJson/Oran7MainGUI.json
          config/themeJson/Oran7CaptionBar.json
          config/themeJson/Oran7ProgressSlider.json
          config/themeJson/Oran7MusicPlaylistView.json

          config/default.json
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
    ${ORAN7_UTILS_DIR}
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
    # 链接核心单例库（Config、Theme 等）
    Oran7Core
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
    QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
)

include(GNUInstallDirs)
install(TARGETS Oran7MediaClient
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(QT_VERSION_MAJOR EQUAL 6)
    qt_import_qml_plugins(Oran7MediaClient)
    qt_finalize_executable(Oran7MediaClient)
endif()
