cmake_minimum_required(VERSION 3.16)

project(Oran7MediaClient VERSION 0.7 LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "本工程当前仅支持 Windows Qt/MinGW。")
endif()
option(ORAN7_BUILD_APP "构建主程序" ON)
option(BUILD_TESTING "构建离线检查（不启动主程序或访问真实服务）" OFF)
include(cmake/Oran7Dependencies.cmake)
include(CTest)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 生成 compile_commands.json，供 Qt Creator clangd 代码模型使用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
    Network
    WebSockets
    Concurrent
)

# 标准 Qt 翻译资源随构建编译并嵌入共享层，无需部署额外 qm 文件。
find_package(Qt6 REQUIRED COMPONENTS LinguistTools)
qt_add_lrelease(TS_FILES translations/oran7_en_US.ts QM_FILES_OUTPUT_VARIABLE ORAN7_QM_FILES)
add_custom_target(oran7_update_translations
    COMMAND Qt6::lupdate "${CMAKE_CURRENT_SOURCE_DIR}/main.qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/translations/Oran7UiMessages.h"
        -locations relative -no-obsolete -ts "${CMAKE_CURRENT_SOURCE_DIR}/translations/oran7_en_US.ts"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "更新中英文 UI 翻译源目录（不自动翻译新增文案）")


# 设置 Qt 策略以避免 QTP0001 警告
qt_policy(SET QTP0001 NEW)

# 3rdparty QWindowKit (QWKQuick) 依赖配置
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

# ============================================================
# libdatachannel — WHIP/WebRTC 推流
# ============================================================
# 说明：从 spark-meeting 源码移入，带完整 deps（libjuice/libsrtp/usrsctp/plog）
# 依赖：OpenSSL（MinGW 下通过 MSYS2 安装: pacman -S mingw-w64-x86_64-openssl）
option(SPARK_MEDIA_PUBLISHER_WITH_LIBDATACHANNEL
       "Enable WHIP/WebRTC publishing via libdatachannel" ON)

if(SPARK_MEDIA_PUBLISHER_WITH_LIBDATACHANNEL AND ORAN7_BUILD_APP)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(PREFER_SYSTEM_LIB OFF CACHE BOOL "" FORCE)
    set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
    set(NO_TESTS ON CACHE BOOL "" FORCE)
    set(NO_WEBSOCKET ON CACHE BOOL "" FORCE)
    set(RTC_UPDATE_VERSION_HEADER OFF CACHE BOOL "" FORCE)

    # libsrtp 的 printf 格式警告 + libjuice 的 GetProcAddress 函数指针强转警告：全部静默
    # （-Wno-error=cast-function-type 只是把报错降级成警告，仍会刷屏，须用 -Wno-cast-function-type）
    set(SAVED_C_FLAGS "${CMAKE_C_FLAGS}")
    set(SAVED_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-format -Wno-cast-function-type")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-cast-function-type")

    add_subdirectory(3rdparty/libdatachannel libdatachannel_build)

    set(CMAKE_C_FLAGS "${SAVED_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${SAVED_CXX_FLAGS}")
    unset(SAVED_C_FLAGS)
    unset(SAVED_CXX_FLAGS)
    # 主程序链接静态目标，默认构建无需再生成未使用的共享版本。
    if(TARGET datachannel-static AND TARGET datachannel)
        set_target_properties(datachannel PROPERTIES EXCLUDE_FROM_ALL TRUE)
    endif()
endif()
# ============================================================

# 添加全局单例 GlobalSingleton 配置路径
set(GLOBAL_SINGLETON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/GlobalSingleton")
# Client 门面及其拥有的播放业务服务（仅编译进 EXE）
set(ORAN7_MEDIA_CLIENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/Oran7MediaClient")

# 添加FFmpeg路径配置
# FFMPEG_ROOT_DIR 由 cmake/Oran7Dependencies.cmake 的 cache 配置提供。
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
#添加网易云扫码登录 ncm 模块配置路径
set(NCM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/ncm")

set(PROJECT_MAIN_SOURCES
    main.cpp
    qml.qrc res.qrc
)

# ==================== brotli 解码器（B站弹幕 protover=3 满速流）====================
set(BROTLI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/brotli")
set(BROTLI_SOURCES
    ${BROTLI_DIR}/dec/bit_reader.c
    ${BROTLI_DIR}/dec/decode.c
    ${BROTLI_DIR}/dec/huffman.c
    ${BROTLI_DIR}/dec/state.c
    ${BROTLI_DIR}/common/constants.c
    ${BROTLI_DIR}/common/context.c
    ${BROTLI_DIR}/common/dictionary.c
    ${BROTLI_DIR}/common/platform.c
    ${BROTLI_DIR}/common/shared_dictionary.c
    ${BROTLI_DIR}/common/transform.c
)

# CONFIGURE_DEPENDS：每次构建时自动重扫源文件，新增 .cpp/.h 无需手动重新配置 CMake
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "src/cpp/*.cpp" "src/cpp/*.h")
# 排除已移至 Oran7Core 共享库的单例文件（避免 EXE/DLL 各自编译一份）
list(FILTER CORE_SOURCES EXCLUDE REGEX "src/cpp/Oran7Config/.*")
# 主题仅在 QML 模块实现，EXE 经链接导入同一实例和元对象。
list(FILTER CORE_SOURCES EXCLUDE REGEX "src/cpp/QtQuicCppExtend/Oran7Theme(_p)?\\.(cpp|h)$")

# 网易云会话、解析与网络模块
file(GLOB_RECURSE NCM_SOURCES CONFIGURE_DEPENDS "${NCM_DIR}/*.cpp" "${NCM_DIR}/*.h")
# 排除 QtQuicCppExtend 目录（用于 QML 模块）
#list(FILTER CORE_SOURCES EXCLUDE REGEX "src/cpp/QtQuicCppExtend/.*")


# ======== Oran7Core 共享库（单例唯一所在，EXE 和 QML 插件共享）========
set(ORAN7_CORE_SOURCES
    src/cpp/Oran7Config/Oran7CredentialStore.h
    src/cpp/Oran7Config/Oran7CredentialStore.cpp
    src/cpp/GlobalSingleton/Oran7UiPreferences.h
    src/cpp/GlobalSingleton/Oran7UiPreferences.cpp
    src/cpp/GlobalSingleton/Oran7LogSink.h
    src/cpp/GlobalSingleton/Oran7LogSink.cpp
    src/cpp/Oran7Config/Oran7AppConfig.h
    src/cpp/Oran7Config/Oran7AppConfig.cpp
    src/cpp/Oran7Config/Oran7ConfigKeys.h
    src/cpp/GlobalSingleton/Oran7GlobalHelper.h
    src/cpp/GlobalSingleton/Oran7GlobalHelper.cpp
    src/cpp/GlobalSingleton/Oran7Async.h
    src/cpp/GlobalSingleton/Oran7Async.cpp
    src/cpp/GlobalSingleton/Oran7AppConfigManager.h
    src/cpp/GlobalSingleton/Oran7AppConfigManager.cpp
    src/cpp/GlobalSingleton/Oran7ThemeProfileManager.h
    src/cpp/GlobalSingleton/Oran7ThemeProfileManager.cpp
)

# 共享服务清单同时决定 EXE 排除项，新增服务不能再被 glob 编进第二份。
foreach(_oran7_core_source IN LISTS ORAN7_CORE_SOURCES)
    list(REMOVE_ITEM CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${_oran7_core_source}")
endforeach()
unset(_oran7_core_source)
qt_add_library(Oran7Core SHARED ${ORAN7_CORE_SOURCES})

target_include_directories(Oran7Core
    PUBLIC
        ${GLOBAL_SINGLETON_DIR}
    PRIVATE
        ${QtQuicCppExtend_DIR}
        ${ORAN7_UTILS_DIR}
)

qt_add_resources(Oran7Core "oran7_translations" PREFIX "/i18n"
    BASE "${CMAKE_CURRENT_BINARY_DIR}" FILES ${ORAN7_QM_FILES})

target_link_libraries(Oran7Core PUBLIC Qt6::Core Qt6::Gui Qt6::Qml)
target_link_libraries(Oran7Core PRIVATE crypt32)
if(MINGW)
    # 新 QObject 显式导出元对象时，仍保留共享层已有的自动导出接口。
    target_link_options(Oran7Core PRIVATE "-Wl,--export-all-symbols")
endif()

set_target_properties(Oran7Core PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)


# QML 导出类唯一编译归属为 Oran7UI_Impl；保留原注册名与插件输出目录。
set(ORAN7_QML_SOURCES
        src/cpp/QtQuicCppExtend/Oran7WindowAgent.h
        src/cpp/QtQuicCppExtend/Oran7WindowAgent.cpp
        src/cpp/QtQuicCppExtend/Oran7Api.h
        src/cpp/QtQuicCppExtend/Oran7Api.cpp
        src/cpp/QtQuicCppExtend/Oran7Theme.h
        src/cpp/QtQuicCppExtend/Oran7Theme.cpp
        src/cpp/QtQuicCppExtend/Oran7FileHelper.h
        src/cpp/QtQuicCppExtend/Oran7FileHelper.cpp
        src/cpp/QtQuicCppExtend/Oran7ThemeEvaluator_p.h
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
foreach(_oran7_qml_source IN LISTS ORAN7_QML_SOURCES)
    list(REMOVE_ITEM CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${_oran7_qml_source}")
endforeach()
unset(_oran7_qml_source)

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
    SOURCES ${ORAN7_QML_SOURCES}
)

target_compile_definitions(Oran7UI_Impl PRIVATE ORAN7_UI_IMPL_LIBRARY)

qt_add_resources(Oran7UI_Impl "resource"
      PREFIX "/"
      FILES
          config/themeJson/Index.json
          config/themeJson/Oran7MainGUI.json
          config/themeJson/Oran7CaptionBar.json
          config/themeJson/Oran7ProgressSlider.json
          config/themeJson/Oran7MusicPlaylistView.json
          config/themeJson/Oran7MusicPlayControls.json
          config/themeJson/Oran7MusicLyricsWindow.json
          config/themeJson/Oran7VideoPlayer.json
          config/themeJson/Oran7ScreenCapture.json
          config/themeJson/Oran7MusicLyricsIsland.json

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

if(ORAN7_BUILD_APP)
#==================< qt_add_executable：Oran7MediaClient >============================

qt_add_executable(Oran7MediaClient
    MANUAL_FINALIZATION
    ${PROJECT_MAIN_SOURCES}
    ${CORE_SOURCES}
    ${NCM_SOURCES}
    ${BROTLI_SOURCES}
)

# 告知 SDL2 不要劫持 main（避免 SDL_main 与 Qt 的 qMain 冲突）
target_compile_definitions(Oran7MediaClient PRIVATE SDL_MAIN_HANDLED)

# libdatachannel WHIP/WebRTC 推流支持
if(SPARK_MEDIA_PUBLISHER_WITH_LIBDATACHANNEL)
    if(TARGET datachannel-static)
        target_compile_definitions(Oran7MediaClient PRIVATE
            SPARK_MEDIA_PUBLISHER_WITH_LIBDATACHANNEL
            RTC_ENABLE_MEDIA=1)
        target_link_libraries(Oran7MediaClient PRIVATE datachannel-static)
    elseif(TARGET datachannel)
        target_compile_definitions(Oran7MediaClient PRIVATE
            SPARK_MEDIA_PUBLISHER_WITH_LIBDATACHANNEL
            RTC_ENABLE_MEDIA=1)
        target_link_libraries(Oran7MediaClient PRIVATE datachannel)
    else()
        message(WARNING "libdatachannel subdir added but no target found. "
                "WHIP publisher will fall back to dryRun mode.")
    endif()
endif()

# ===================== ShaderEffect 自动编译 QSB ================
qt_add_shaders(Oran7MediaClient "shaders"
    PREFIX "/"
    FILES
        shaders/video.vert
        shaders/video.frag
        shaders/hdrComposite.vert
        shaders/hdrComposite.frag
        shaders/gradientText.frag
        shaders/octgrams.frag
        shaders/fractal.frag
        shaders/clouds.frag
)
# =======================================================

# ======== 配置资源直接嵌入 EXE=========
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
    set(SQLITE_DIR "${ORAN7_SQLITE_ROOT}")

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

        #拷贝 MSYS2 zlib1.dll（B站弹幕 zlib 解压用，同 OpenSSL DLL 的处理方式）
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ORAN7_ZLIB_RUNTIME}" "${APP_OUT_DIR}/zlib1.dll"
    )
endif()
# ========================================================

# 在qt_add_executable之后添加模块配置文件路径
target_include_directories(Oran7MediaClient
    PRIVATE
    ${GLOBAL_SINGLETON_DIR}
    ${ORAN7_MEDIA_CLIENT_DIR}

    ${FFMPEG_ROOT_DIR}/include
    ${FFMPEG_ROOT_DIR}/include/SDL2
    ${FFMPEG_ROOT_DIR}/lib

    ${ORAN7_PLAYER_DIR}
    ${ORAN7_SCREEN_CAPTURE_DIR}
    ${ORAN7_VIDEO_GPU_RENDER_DIR}
    ${ORAN7_SEVER_DIR}
    ${ORAN7_UTILS_DIR}
    ${QtQuicCppExtend_DIR}
    ${NCM_DIR}
    ${NCM_DIR}/crypto
    ${NCM_DIR}/network
    ${NCM_DIR}/protocol
    ${NCM_DIR}/api
    ${NCM_DIR}/session
    ${BROTLI_DIR}/include
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
    Qt6::Network
    Qt6::WebSockets
    Qt6::Concurrent
    Qt6::GuiPrivate
    # QWindowKit 库
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
    dxgi dwmapi ole32 uuid dbghelp d3d11 dcomp
    # MSYS2 libwinpthread：解决 MSYS2 OpenSSL 引入的 nanosleep64 等符号。
    # Qt MinGW 11.2 的 libwinpthread 没有这些 64 位时间函数，MSYS2 版本提供。
    # MSYS2 libwinpthread 编译时带了 -fstack-protector，需要 Qt MinGW 的 libssp 配合。
    # 全部放末尾，确保 Qt 自身符号优先，只填充缺失项。
    "${ORAN7_WINPTHREAD_LIBRARY}"
    # MSYS2 libcrypto：网易云 weapi 加密（AES/RSA）
    "${ORAN7_CRYPTO_LIBRARY}"
    # MSYS2 zlib：B站直播弹幕 zlib 压缩解压（qUncompress 不能解裸 zlib 流）
    "${ORAN7_ZLIB_LIBRARY}"
    "${ORAN7_SSP_LIBRARY}"
)

# 仅给需要 OpenSSL 的源文件加 include 路径，避免 MSYS2 头文件全局遮蔽 MinGW 标准头
set_source_files_properties(
    ${NCM_DIR}/crypto/CryptoProvider.cpp
    ${NCM_DIR}/crypto/WeapiCrypto.cpp
    PROPERTIES INCLUDE_DIRECTORIES "${OPENSSL_ROOT_DIR}/include"
)

# B站弹幕 zlib 解压同样只给该源文件加 MSYS2 include（zlib.h）
set_source_files_properties(
    ${ORAN7_PLAYER_DIR}/BilibiliDanmakuClient.cpp
    PROPERTIES INCLUDE_DIRECTORIES "${ORAN7_ZLIB_ROOT}/include"
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

endif() # ORAN7_BUILD_APP

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
