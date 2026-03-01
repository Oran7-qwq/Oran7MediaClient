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

# 查找OpenGL
find_package(OpenGL REQUIRED)

# 添加FFmpeg路径配置
set(FFMPEG_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg6.0")
message(STATUS "FFmpeg路径: ${FFMPEG_ROOT_DIR}")

set(PROJECT_SOURCES
    main.cpp
    qml.qrc
    res.qrc
)

qt_add_executable(Oran7MediaClient
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
    app.rc
    sever.cpp sever.h sonic.cpp sonic.h
    database.cpp database.h
    ffmsg.h
    ffmsgqueue.cpp ffmsgqueue.h
    ffplay_def.cpp ffplay_def.h
    ffplayer.cpp ffplayer.h
    globalhelper.cpp globalhelper.h
    ImageScaler.h
    loadingdatahandler.cpp loadingdatahandler.h
    loginhandler.cpp loginhandler.h
    oran7mediaplayer.cpp oran7mediaplayer.h
    protocolhandler.cpp protocolhandler.h
    appjsonconfigmanager.cpp appjsonconfigmanager.h
    client.cpp client.h
    asyncmanagers.h asyncmanagers.cpp
    applicationcontext.h applicationcontext.cpp
    filehelper.h filehelper.cpp
    d3d11videoitem.h d3d11videoitem.cpp
    bilibliliveroomaddresscatch.h bilibliliveroomaddresscatch.cpp
    oran7screencapture.h oran7screencapture.cpp
    d3d11deviceprovider.h d3d11deviceprovider.cpp
)

# ===================== ShaderEffect 自动编译 QSB =====================
# qt_add_shaders(Oran7MediaClient "VideoShaders"
#     PREFIX "/"
#     FILES
#         shaders/VideoPlayerRenderGLSL.vert
#         shaders/VideoPlayerRenderGLSL.frag
# )
# =====================================================================

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

        # 1 拷贝 ffmpeg6.0/bin 下所有内容到 exe 目录
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${FFMPEG_BIN_DIR}" "${APP_OUT_DIR}"

        # 2 拷贝 SQLite 文件夹到 exe 目录
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${SQLITE_DIR}" "${APP_OUT_DIR}/SQLite"
    )
endif()
# ===============================================================



# 在qt_add_executable之后立即添加FFmpeg的配置
target_include_directories(Oran7MediaClient
    PRIVATE ${FFMPEG_ROOT_DIR}/include
)

# 添加SDL2头文件路径
target_include_directories(Oran7MediaClient
    PRIVATE ${FFMPEG_ROOT_DIR}/include/SDL2
)

# 添加库文件搜索路径
target_link_directories(Oran7MediaClient
    PRIVATE ${FFMPEG_ROOT_DIR}/lib
)

# target_link_libraries，添加Sql和QuickControls2模块
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
    OpenGL::GL
  PRIVATE
    avcodec avformat avutil avdevice avfilter swresample swscale
  PRIVATE
    SDL2 SDL2main
    d3dcompiler dxgi
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
