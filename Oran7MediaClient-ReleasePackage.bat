@echo off
setlocal enabledelayedexpansion

REM ============================================================
REM  Oran7MediaClient 一键打包脚本 (Windows .bat)
REM
REM  用法:
REM    Oran7MediaClient-ReleasePackage.bat [-idir <源构建目录>] [-odir <打包目标目录>]
REM
REM  示例:
REM    Oran7MediaClient-ReleasePackage.bat -idir build\Desktop_Qt_6_7_3_MinGW_64_bit-Release ^
REM                  -odir C:\Users\funny\Desktop\Oran7MediaClient-Release\Oran7MediaClient
REM
REM  参数说明:
REM    -idir  源构建目录（必须包含 Oran7MediaClient.exe），缺省用 Release 构建目录
REM    -odir  打包目标目录，缺省 C:\Users\funny\Desktop\Oran7MediaClient-Release\Oran7MediaClient
REM    -h / -help  显示本帮助
REM
REM  交互模式: 不带参数直接运行，会依次提示输入源路径与目标路径，直接回车使用缺省值
REM  上次目录复用: 检测到记录（Oran7MediaClient-ReleasePackage.ini）时询问 [Y/N]，回车默认 Y 直接复用上次目录
REM
REM  前提: 已用 CMake + Ninja 构建 Release 版本
REM ============================================================

set "PROJECT_DIR=%~dp0"
set "QT_DIR=D:\Qt\6.7.3\mingw_64"
set "OPENSSL_BIN=C:\msys64\mingw64\bin"
set "INI=%PROJECT_DIR%Oran7MediaClient-ReleasePackage.ini"

set "IDIR="
set "ODIR="

REM ---- 解析命令行参数 ----
:parse
if "%~1"=="" goto :endparse
if /i "%~1"=="-idir" (
    set "IDIR=%~2"
    shift
    shift
    goto :parse
)
if /i "%~1"=="-odir" (
    set "ODIR=%~2"
    shift
    shift
    goto :parse
)
if /i "%~1"=="-h" goto :usage
if /i "%~1"=="-help" goto :usage
echo [错误] 未知参数: %~1
goto :usage

:endparse

REM ---- 上次目录复用 / 交互输入 ----
if not "%IDIR%"=="" if not "%ODIR%"=="" goto :post_input
goto :check_last

:check_last
if not exist "%INI%" goto :prompt_input
set "LAST_IDIR="
set "LAST_ODIR="
for /f "usebackq tokens=1,* delims==" %%a in ("%INI%") do (
    if "%%a"=="IDIR" set "LAST_IDIR=%%b"
    if "%%a"=="ODIR" set "LAST_ODIR=%%b"
)
if not defined LAST_IDIR goto :prompt_input
if not defined LAST_ODIR goto :prompt_input
set /p "REUSE=检测到上次打包目录记录，直接使用 [Y/N]，回车默认 Y: "
if /i not "%REUSE%"=="N" goto :use_last
goto :prompt_input

:use_last
if "%IDIR%"=="" set "IDIR=%LAST_IDIR%"
if "%ODIR%"=="" set "ODIR=%LAST_ODIR%"
echo 使用上次记录: %IDIR% ^| %ODIR%
goto :prompt_input

:prompt_input
if "%IDIR%"=="" set /p "IDIR=请输入源构建目录（包含 Oran7MediaClient.exe），直接回车使用默认: "
if "%IDIR%"=="" set "IDIR=%PROJECT_DIR%build\Desktop_Qt_6_7_3_MinGW_64_bit-Release"
if "%ODIR%"=="" set /p "ODIR=请输入打包目标目录，直接回车使用默认: "
if "%ODIR%"=="" set "ODIR=C:\Users\funny\Desktop\Oran7MediaClient-Release\Oran7MediaClient"

:post_input

echo ============================================================
echo  Oran7MediaClient 打包
echo  项目目录: %PROJECT_DIR%
echo  构建目录: %IDIR%
echo  发布目录: %ODIR%
echo ============================================================

REM ---- 校验构建产物 ----
if not exist "%IDIR%\Oran7MediaClient.exe" (
    echo [错误] 未找到 Oran7MediaClient.exe:
    echo        "%IDIR%\Oran7MediaClient.exe"
    echo        请先用 CMake/Ninja 构建 Release，再运行本脚本。
    exit /b 1
)

REM 1. 创建发布目录
echo [1/10] 创建发布目录...
if exist "%ODIR%" rmdir /s /q "%ODIR%"
mkdir "%ODIR%" 2>nul
if not exist "%ODIR%" (
    echo [错误] 无法创建发布目录: "%ODIR%"
    exit /b 1
)

REM 2. 复制构建产物（含 POST_BUILD 拷来的 ffmpeg6.0/bin、SQLite/、zlib1.dll）
echo [2/10] 复制构建产物...
robocopy "%IDIR%" "%ODIR%" /E /NFL /NDL /NJH /NJS /NP >nul 2>&1
if %errorlevel% geq 8 (
    echo [错误] robocopy 复制构建产物失败，退出码 %errorlevel%
    exit /b 1
)

REM 3. 运行 windeployqt 部署 Qt 依赖
echo [3/10] 部署 Qt 依赖 (windeployqt)...
pushd "%ODIR%"
"%QT_DIR%\bin\windeployqt.exe" --release --no-translations --qmldir "%PROJECT_DIR%src\qml" Oran7MediaClient.exe
echo windeployqt 退出码: %errorlevel%
popd

REM 4. 清理临时构建文件
echo [4/10] 清理临时文件...
for %%d in (.cmake .ninja_deps .ninja_log .qsb .qt .qtc .qtc_clangd .rcc 3rdparty CMakeFiles Oran7MediaClient_autogen Oran7UI_Impl_autogen Oran7UI_Implplugin_autogen Oran7Core_autogen Testing _build _install meta_types qmltypes out-amd64-Release libdatachannel_build) do (
    if exist "%ODIR%\%%d" rmdir /s /q "%ODIR%\%%d" >nul 2>&1
)
for %%f in (*.a *.ninja build.ninja cmake_install.cmake qtcsettings.cmake oran7ui_impl_qmltyperegistrations.cpp CMakeCache.txt CMakeCache.txt.prev compile_commands.json LibDataChannelConfig.cmake LibDataChannelConfigVersion.cmake LibJuiceConfig.cmake LibJuiceConfigVersion.cmake a.txt) do (
    if exist "%ODIR%\%%f" del /q "%ODIR%\%%f"
)
del /q "%ODIR%\Oran7UI_Implplugin_*.cpp" 2>nul

REM 5. 复制源码资源（qrc 未覆盖的散装资源）
echo [5/10] 复制资源文件...
robocopy "%PROJECT_DIR%src\qml" "%ODIR%\src\qml" /E /NFL /NDL /NJH /NJS /NP >nul 2>&1
copy /y "%PROJECT_DIR%main.qml" "%ODIR%\" >nul 2>&1
robocopy "%PROJECT_DIR%image" "%ODIR%\image" /E /NFL /NDL /NJH /NJS /NP >nul 2>&1
robocopy "%PROJECT_DIR%sound" "%ODIR%\sound" /E /NFL /NDL /NJH /NJS /NP >nul 2>&1
robocopy "%PROJECT_DIR%shaders" "%ODIR%\shaders" /E /NFL /NDL /NJH /NJS /NP >nul 2>&1

REM 6. 复制 MinGW 运行时库
echo [6/10] 复制 MinGW 运行时库...
copy /y "%QT_DIR%\bin\libgcc_s_seh-1.dll" "%ODIR%\" >nul 2>&1
copy /y "%QT_DIR%\bin\libstdc++-6.dll" "%ODIR%\" >nul 2>&1
copy /y "%QT_DIR%\bin\libwinpthread-1.dll" "%ODIR%\" >nul 2>&1

REM 6b. 复制 OpenSSL DLL（网易云 weapi 加密需要）
if exist "%OPENSSL_BIN%\libcrypto-3-x64.dll" (
    copy /y "%OPENSSL_BIN%\libcrypto-3-x64.dll" "%ODIR%\" >nul 2>&1
    echo 已复制 libcrypto-3-x64.dll
) else (
    echo [警告] 未找到 libcrypto-3-x64.dll，网易云加密可能无法工作！
)
if exist "%OPENSSL_BIN%\libssl-3-x64.dll" (
    copy /y "%OPENSSL_BIN%\libssl-3-x64.dll" "%ODIR%\" >nul 2>&1
    echo 已复制 libssl-3-x64.dll
) else (
    echo [警告] 未找到 libssl-3-x64.dll，网易云加密可能无法工作！
)

REM 7. 创建 qt.conf
echo [7/10] 创建 qt.conf...
(
echo [Paths]
echo Imports = qml
echo Qml2Imports = qml
) > "%ODIR%\qt.conf"

REM 8. 复制 QML 插件（关键步骤！缺失则程序无法启动）
echo [8/10] 复制 QML 插件 (Oran7UI)...
if exist "%ODIR%\Oran7UI" rmdir /s /q "%ODIR%\Oran7UI"
robocopy "%IDIR%\Oran7UI" "%ODIR%\Oran7UI" /E /NFL /NDL /NJH /NJS /NP >nul 2>&1

REM 9. 生成 README
echo [9/10] 生成 README...
(
echo Oran7MediaClient - Release 版本
echo ================================
echo.
echo 使用说明:
echo 1. 双击 Oran7MediaClient.exe 启动应用程序
echo 2. 无需安装，直接运行即可
echo 3. 首次运行会自动创建配置文件和数据库
echo.
echo 包含内容:
echo - 主程序: Oran7MediaClient.exe
echo - Oran7UI QML 模块插件
echo - FFmpeg 相关库和工具 ^(用于音视频处理^)
echo - SDL2 库 ^(用于音频输出^)
echo - Qt6 运行时库和插件
echo - Qt QML 模块和界面组件
echo - SQLite 数据库支持
echo - MinGW 运行时库
echo - 应用程序资源文件 ^(图片、声音、着色器等^)
echo.
echo 系统要求:
echo - Windows 10/11 ^(64位^)
echo - 至少 4GB 内存推荐
echo - 显卡支持 Direct3D 11 或 OpenGL 3.3 或更高版本
echo.
echo 注意事项:
echo - 首次运行可能需要防火墙权限
echo - 如遇到缺少 DLL 的问题，请确保 Windows 系统已更新到最新版本
echo - 建议将整个文件夹放在非系统盘运行
echo - 程序使用 Direct3D 11 作为渲染后端，确保显卡驱动已更新
echo - 配置文件会自动保存在用户 AppData 目录中
echo.
echo 版本信息:
echo - 版本: 0.7
echo - Qt 版本: 6.7.3
echo - 编译器: MinGW 64-bit
) > "%ODIR%\README.txt"

REM 10. 验证打包结果
echo [10/10] 验证打包结果...
echo 关键文件检查:
if exist "%ODIR%\Oran7MediaClient.exe" (echo   [OK] Oran7MediaClient.exe) else (echo   [失败] Oran7MediaClient.exe 缺失)
if exist "%ODIR%\Oran7UI\Impl\Oran7UI_Implplugin.dll" (echo   [OK] Oran7UI QML 插件) else (echo   [失败] Oran7UI QML 插件缺失)
if exist "%ODIR%\Oran7UI_Impl.dll" (echo   [OK] Oran7UI_Impl.dll) else (echo   [失败] Oran7UI_Impl.dll 缺失)
if exist "%ODIR%\qt.conf" (echo   [OK] qt.conf) else (echo   [失败] qt.conf 缺失)
if exist "%ODIR%\libcrypto-3-x64.dll" (echo   [OK] libcrypto-3-x64.dll) else (echo   [警告] libcrypto-3-x64.dll 缺失)
if exist "%ODIR%\zlib1.dll" (echo   [OK] zlib1.dll) else (echo   [警告] zlib1.dll 缺失)
if exist "%ODIR%\SQLite\sqlite3.dll" (echo   [OK] SQLite 运行时) else (echo   [警告] SQLite 缺失)

REM 记录本次使用的目录，供下次 [Y/N] 复用
> "%INI%" echo IDIR=%IDIR%
>> "%INI%" echo ODIR=%ODIR%

echo.
echo 打包完成！发布目录:
echo   "%ODIR%"
exit /b 0

:usage
echo.
echo 用法: Oran7MediaClient-ReleasePackage.bat [-idir ^<源构建目录^>] [-odir ^<打包目标目录^>]
echo.
echo 交互模式: 不带参数直接运行，会提示输入源路径与目标路径，回车用缺省值
echo 上次目录复用: 有记录（Oran7MediaClient-ReleasePackage.ini）时询问 [Y/N]，回车默认 Y 复用上次目录
echo.
echo   -idir  源构建目录（必须包含 Oran7MediaClient.exe）
echo         缺省: build\Desktop_Qt_6_7_3_MinGW_64_bit-Release
echo   -odir  打包目标目录
echo         缺省: C:\Users\funny\Desktop\Oran7MediaClient-Release\Oran7MediaClient
echo   -h, -help  显示本帮助
echo.
echo 示例:
echo   Oran7MediaClient-ReleasePackage.bat -idir build\Desktop_Qt_6_7_3_MinGW_64_bit-Release -odir D:\pkg\Oran7MediaClient
exit /b 1