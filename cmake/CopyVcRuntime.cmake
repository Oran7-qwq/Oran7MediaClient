# libx265.dll 由 MSVC 构建，直接依赖 VC++ 2015-2022 运行库。
# 这里把运行库复制到 exe 目录，避免干净机器启动时报：
#   由于找不到 VCRUNTIME140_1.dll，无法继续执行代码。
# 运行库来源优先使用系统目录；没有安装 VC++ 运行库的构建机会给出警告，
# 但不会中断构建，目标机仍需安装 Microsoft Visual C++ 2015-2022 Redistributable (x64)。

foreach(dll_name vcruntime140.dll vcruntime140_1.dll msvcp140.dll)
    set(dll_source "$ENV{SystemRoot}/System32/${dll_name}")
    if(EXISTS "${dll_source}")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${dll_source}" "${APP_OUT_DIR}/${dll_name}"
            RESULT_VARIABLE copy_result)
        if(NOT copy_result EQUAL 0)
            message(WARNING "复制 ${dll_name} 到 ${APP_OUT_DIR} 失败")
        endif()
    else()
        message(WARNING
            "未找到 ${dll_name}；目标机需要安装 Microsoft Visual C++ 2015-2022 Redistributable (x64)")
    endif()
endforeach()
