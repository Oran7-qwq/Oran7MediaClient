# 依赖保留现行安装位置作为默认值，可在 preset、命令行或 CMake GUI 中覆盖。
set(ORAN7_QT_ROOT "D:/Qt/6.7.3/mingw_64" CACHE PATH "Qt 6.7.3 MinGW 安装目录")
set(ORAN7_MSYS2_ROOT "C:/msys64/mingw64" CACHE PATH "MSYS2 MinGW64 依赖目录")
set(FFMPEG_ROOT_DIR "${PROJECT_SOURCE_DIR}/ffmpeg6.0" CACHE PATH "FFmpeg 与 SDL2 目录")
set(ORAN7_SQLITE_ROOT "${PROJECT_SOURCE_DIR}/SQLite" CACHE PATH "SQLite 运行时目录")
set(OPENSSL_ROOT_DIR "${ORAN7_MSYS2_ROOT}" CACHE PATH "OpenSSL 目录")
set(ORAN7_ZLIB_ROOT "${ORAN7_MSYS2_ROOT}" CACHE PATH "zlib 目录")
list(PREPEND CMAKE_PREFIX_PATH "${ORAN7_QT_ROOT}")

set(ORAN7_WINPTHREAD_LIBRARY "${ORAN7_MSYS2_ROOT}/lib/libwinpthread.a" CACHE FILEPATH "MSYS2 winpthread 静态库")
set(ORAN7_CRYPTO_LIBRARY "${OPENSSL_ROOT_DIR}/lib/libcrypto.dll.a" CACHE FILEPATH "OpenSSL crypto 导入库")
set(ORAN7_ZLIB_LIBRARY "${ORAN7_ZLIB_ROOT}/lib/libz.dll.a" CACHE FILEPATH "zlib 导入库")
set(ORAN7_ZLIB_RUNTIME "${ORAN7_ZLIB_ROOT}/bin/zlib1.dll" CACHE FILEPATH "zlib 运行时 DLL")

# 从实际编译器查询，避免把 GCC 版本号写进工程链接路径。
execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -print-file-name=libssp.a
    OUTPUT_VARIABLE _oran7_ssp OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _oran7_ssp_result)
set(ORAN7_SSP_LIBRARY "${_oran7_ssp}" CACHE FILEPATH "当前 MinGW 的 SSP 静态库")
foreach(_oran7_library IN ITEMS ORAN7_WINPTHREAD_LIBRARY ORAN7_CRYPTO_LIBRARY
        ORAN7_ZLIB_LIBRARY ORAN7_SSP_LIBRARY)
    if(NOT EXISTS "${${_oran7_library}}")
        message(FATAL_ERROR "${_oran7_library} 不存在：${${_oran7_library}}。请调整对应 cache 路径。")
    endif()
endforeach()
unset(_oran7_ssp)
unset(_oran7_ssp_result)
unset(_oran7_library)
