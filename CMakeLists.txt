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

#添加全局单例GlobalSingleton 配置路径
set(GLOBAL_SIGNALTON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/code/GlobalSingleton")

# 添加FFmpeg路径配置
set(FFMPEG_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg6.0")
#添加Oran7MediaPlayer模块配置路径
set(ORAN7_PLAYER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/code/Oran7MediaPlayer")
#添加Oran7ScreenPlayer模块配置路径
set(ORAN7_SCREEN_CAPTURE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/code/Oran7ScreenCapture")
#添加Oran7VideoGPURender模块配置路径
set(ORAN7_VIDEO_GPU_RENDER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/code/Oran7VideoGPURender")
#添加Oran7Sever模块配置路径
set(ORAN7_SEVER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/code/Oran7Sever")

set(PROJECT_MAIN_SOURCES
    main.cpp
    qml.qrc
    res.qrc
)

qt_add_executable(Oran7MediaClient
    MANUAL_FINALIZATION
    ${PROJECT_MAIN_SOURCES}

    filehelper.h filehelper.cpp #USED BY QML

    #GlobalSingleton
    code/GlobalSingleton/appjsonconfigmanager.h
    code/GlobalSingleton/appjsonconfigmanager.cpp
    code/GlobalSingleton/asyncmanagers.h
    code/GlobalSingleton/asyncmanagers.cpp
    code/GlobalSingleton/applicationcontext.h
    code/GlobalSingleton/applicationcontext.cpp
    code/GlobalSingleton/globalhelper.h
    code/GlobalSingleton/globalhelper.cpp
    code/GlobalSingleton/client.h
    code/GlobalSingleton/client.cpp

    #Oran7MediaPlayer
    code/Oran7MediaPlayer/ImageScaler.h
    code/Oran7MediaPlayer/ffmsg.h
    code/Oran7MediaPlayer/ffmsgqueue.h
    code/Oran7MediaPlayer/ffmsgqueue.cpp
    code/Oran7MediaPlayer/ffplay_def.h
    code/Oran7MediaPlayer/ffplay_def.cpp
    code/Oran7MediaPlayer/ffplayer.h
    code/Oran7MediaPlayer/ffplayer.cpp
    code/Oran7MediaPlayer/oran7mediaplayer.h
    code/Oran7MediaPlayer/oran7mediaplayer.cpp
    code/Oran7MediaPlayer/sonic.h
    code/Oran7MediaPlayer/sonic.cpp
    code/Oran7MediaPlayer/bilibliliveroomaddresscatch.h
    code/Oran7MediaPlayer/bilibliliveroomaddresscatch.cpp

    #Oran7ScreenCapture
    code/Oran7ScreenCapture/oran7screencapture.h
    code/Oran7ScreenCapture/oran7screencapture.cpp

    #Oran7VideoGPURender
    code/Oran7VideoGPURender/d3d11videoitem.h
    code/Oran7VideoGPURender/d3d11videoitem.cpp
    code/Oran7VideoGPURender/d3d11deviceprovider.h
    code/Oran7VideoGPURender/d3d11deviceprovider.cpp

    #Oran7Sever
    code/Oran7Sever/sever.h
    code/Oran7Sever/sever.cpp
    code/Oran7Sever/database.h
    code/Oran7Sever/database.cpp
    code/Oran7Sever/protocolhandler.h
    code/Oran7Sever/protocolhandler.cpp
    code/Oran7Sever/loadingdatahandler.h
    code/Oran7Sever/loadingdatahandler.cpp
    code/Oran7Sever/loginhandler.h
    code/Oran7Sever/loginhandler.cpp
)

# ===================== ShaderEffect 自动编译 QSB ================
# qt_add_shaders(Oran7MediaClient "VideoShaders"
#     PREFIX "/"
#     FILES
#         shaders/VideoPlayerRenderGLSL.vert
#         shaders/VideoPlayerRenderGLSL.frag
# )
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
    ${GLOBAL_SIGNALTON_DIR}

    ${FFMPEG_ROOT_DIR}/include
    ${FFMPEG_ROOT_DIR}/include/SDL2
    ${FFMPEG_ROOT_DIR}/lib

    ${ORAN7_PLAYER_DIR}
    ${ORAN7_SCREEN_CAPTURE_DIR}
    ${ORAN7_VIDEO_GPU_RENDER_DIR}
    ${ORAN7_SEVER_DIR}

    ${CMAKE_CURRENT_SOURCE_DIR}
)

# FFMPEG库目录
target_link_directories(Oran7MediaClient
    PRIVATE
        ${FFMPEG_ROOT_DIR}/lib
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
