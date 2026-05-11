# Oran7MediaClient 打包发布指南

本文档详细说明了如何将 Oran7MediaClient 项目打包为可发布的 Windows 版本。

## 前置条件

- Windows 10/11 操作系统
- Qt 6.7.2 (MinGW 64-bit)
- 已完成 Release 版本构建
- Git Bash 或类似命令行工具

## 打包步骤

### 1. 检查项目构建状态

首先确认 Release 版本已经成功构建：

```bash
# 查看项目目录
ls -la "C:/Users/funny/QtProject.doc/Oran7MediaClient"

# 检查 Release 构建目录是否存在
ls -la "C:/Users/funny/QtProject.doc/Oran7MediaClient/build/Desktop_Qt_6_7_2_MinGW_64_bit-Release"

# 确认主程序存在
ls -lh "C:/Users/funny/QtProject.doc/Oran7MediaClient/build/Desktop_Qt_6_7_2_MinGW_64_bit-Release/Oran7MediaClient.exe"
```

### 2. 创建发布目录

```bash
# 创建发布目录
mkdir -p "C:/Users/funny/Desktop/Oran7MediaClient-Release"
```

### 3. 复制构建文件

```bash
# 复制所有 Release 构建文件到发布目录
cp -r "C:/Users/funny/QtProject.doc/Oran7MediaClient/build/Desktop_Qt_6_7_2_MinGW_64_bit-Release/"* "C:/Users/funny/Desktop/Oran7MediaClient-Release/"
```

### 4. 运行 windeployqt 部署 Qt 依赖

```bash
# 进入发布目录
cd "C:/Users/funny/Desktop/Oran7MediaClient-Release"

# 运行 windeployqt 自动部署 Qt 依赖
/d/Qt/6.7.2/mingw_64/bin/windeployqt Oran7MediaClient.exe

# 如果需要更完整的部署，可以指定 QML 源目录
/d/Qt/6.7.2/mingw_64/bin/windeployqt --qmldir "C:/Users/funny/QtProject.doc/Oran7MediaClient/src/qml" --release --no-translations Oran7MediaClient.exe
```

### 5. 清理临时构建文件

```bash
cd "C:/Users/funny/Desktop/Oran7MediaClient-Release"

# 删除不需要的临时文件和目录
rm -rf .cmake .ninja_deps .ninja_log .qsb .qt .qtc .qtc_clangd .rcc
rm -rf 3rdparty CMakeFiles Oran7MediaClient_autogen
rm -rf Oran7UI_Impl_autogen Oran7UI_Implplugin_autogen
rm -rf Testing _build _install meta_types qmltypes out-amd64-Release
rm -f *.a *.ninja build.ninja cmake_install.cmake qtcsettings.cmake
rm -f oran7ui_impl_qmltyperegistrations.cpp Oran7UI_Implplugin_*.cpp
rm -f CMakeCache.txt
```

### 6. 复制应用程序资源文件

```bash
# 复制 QML 源文件
cp -r "C:/Users/funny/QtProject.doc/Oran7MediaClient/src/qml" "C:/Users/funny/Desktop/Oran7MediaClient-Release/"
cp "C:/Users/funny/QtProject.doc/Oran7MediaClient/main.qml" "C:/Users/funny/Desktop/Oran7MediaClient-Release/"

# 复制图片资源
cp -r "C:/Users/funny/QtProject.doc/Oran7MediaClient/image" "C:/Users/funny/Desktop/Oran7MediaClient-Release/"

# 复制音频资源
cp -r "C:/Users/funny/QtProject.doc/Oran7MediaClient/sound" "C:/Users/funny/Desktop/Oran7MediaClient-Release/"

# 复制着色器文件
cp -r "C:/Users/funny/QtProject.doc/Oran7MediaClient/shaders" "C:/Users/funny/Desktop/Oran7MediaClient-Release/"
```

### 7. 复制 MinGW 运行时库

```bash
# 查找 MinGW 运行时库
find /d/Qt/6.7.2/mingw_64/bin -name "libgcc*.dll" -o -name "libstdc++*.dll" -o -name "libwinpthread*.dll"

# 复制 MinGW 运行时库（通常包括以下三个文件）
cp /d/Qt/6.7.2/mingw_64/bin/libgcc_s_seh-1.dll "C:/Users/funny/Desktop/Oran7MediaClient-Release/"
cp /d/Qt/6.7.2/mingw_64/bin/libstdc++-6.dll "C:/Users/funny/Desktop/Oran7MediaClient-Release/"
cp /d/Qt/6.7.2/mingw_64/bin/libwinpthread-1.dll "C:/Users/funny/Desktop/Oran7MediaClient-Release/"
```

### 8. 创建 qt.conf 配置文件

```bash
# 创建 qt.conf 文件以正确配置 Qt 插件路径
cat > "C:/Users/funny/Desktop/Oran7MediaClient-Release/qt.conf" << 'EOF'
[Paths]
Imports = qml
Qml2Imports = qml
EOF
```

### 9. **关键步骤：复制 QML 插件**

⚠️ **这是最重要的步骤！** 没有这个插件，程序将无法启动。

```bash
# 复制 Oran7UI QML 插件目录
cp -r "C:/Users/funny/QtProject.doc/Oran7MediaClient/build/Desktop_Qt_6_7_2_MinGW_64_bit-Release/Oran7UI" "C:/Users/funny/Desktop/Oran7MediaClient-Release/"
```

验证插件是否正确复制：

```bash
# 检查关键插件文件是否存在
ls -lh "C:/Users/funny/Desktop/Oran7MediaClient-Release/Oran7UI/Impl/Oran7UI_Implplugin.dll"
ls -lh "C:/Users/funny/Desktop/Oran7MediaClient-Release/Oran7UI_Impl.dll"
```

### 10. 创建使用说明文档

```bash
cat > "C:/Users/funny/Desktop/Oran7MediaClient-Release/README.txt" << 'EOF'
Oran7MediaClient - Release 版本
================================

使用说明:
1. 双击 Oran7MediaClient.exe 启动应用程序
2. 无需安装，直接运行即可
3. 首次运行会自动创建配置文件和数据库

包含内容:
- 主程序: Oran7MediaClient.exe
- Oran7UI QML 模块插件
- FFmpeg 相关库和工具 (用于音视频处理)
- SDL2 库 (用于音频输出)
- Qt6 运行时库和插件
- Qt QML 模块和界面组件
- SQLite 数据库支持
- MinGW 运行时库
- 应用程序资源文件 (图片、声音、着色器等)

系统要求:
- Windows 10/11 (64位)
- 至少 4GB 内存推荐
- 显卡支持 Direct3D 11 或 OpenGL 3.3 或更高版本

注意事项:
- 首次运行可能需要防火墙权限
- 如遇到缺少 DLL 的问题，请确保 Windows 系统已更新到最新版本
- 建议将整个文件夹放在非系统盘运行
- 程序使用 Direct3D 11 作为渲染后端，确保显卡驱动已更新
- 配置文件会自动保存在用户 AppData 目录中

版本信息:
- 版本: 0.7
- Qt 版本: 6.7.2
- 编译器: MinGW 64-bit
EOF
```

### 11. 验证打包结果

```bash
# 检查打包目录大小
du -sh "C:/Users/funny/Desktop/Oran7MediaClient-Release"

# 检查关键文件是否存在
ls -lh "C:/Users/funny/Desktop/Oran7MediaClient-Release/Oran7MediaClient.exe"
ls -lh "C:/Users/funny/Desktop/Oran7MediaClient-Release/Oran7UI/Impl/Oran7UI_Implplugin.dll"
ls -lh "C:/Users/funny/Desktop/Oran7MediaClient-Release/qt.conf"

# 统计 DLL 文件数量
cd "C:/Users/funny/Desktop/Oran7MediaClient-Release" && find . -name "*.dll" | wc -l

# 测试程序是否可以运行
cd "C:/Users/funny/Desktop/Oran7MediaClient-Release"
./Oran7MediaClient.exe
```

## 完整打包脚本

可以将以上步骤整合为一个完整的打包脚本：

```bash
#!/bin/bash

# Oran7MediaClient 打包脚本
# 使用方法: bash package_release.sh

PROJECT_DIR="C:/Users/funny/QtProject.doc/Oran7MediaClient"
BUILD_DIR="$PROJECT_DIR/build/Desktop_Qt_6_7_2_MinGW_64_bit-Release"
RELEASE_DIR="C:/Users/funny/Desktop/Oran7MediaClient-Release"
QT_DIR="/d/Qt/6.7.2/mingw_64"

echo "开始打包 Oran7MediaClient..."

# 1. 创建发布目录
echo "创建发布目录..."
mkdir -p "$RELEASE_DIR"

# 2. 复制构建文件
echo "复制构建文件..."
cp -r "$BUILD_DIR/"* "$RELEASE_DIR/"

# 3. 运行 windeployqt
echo "部署 Qt 依赖..."
cd "$RELEASE_DIR"
$QT_DIR/bin/windeployqt Oran7MediaClient.exe

# 4. 清理临时文件
echo "清理临时文件..."
rm -rf .cmake .ninja_deps .ninja_log .qsb .qt .qtc .qtc_clangd .rcc
rm -rf 3rdparty CMakeFiles Oran7MediaClient_autogen
rm -rf Oran7UI_Impl_autogen Oran7UI_Implplugin_autogen
rm -rf Testing _build _install meta_types qmltypes out-amd64-Release
rm -f *.a *.ninja build.ninja cmake_install.cmake qtcsettings.cmake
rm -f oran7ui_impl_qmltyperegistrations.cpp Oran7UI_Implplugin_*.cpp
rm -f CMakeCache.txt

# 5. 复制资源文件
echo "复制资源文件..."
cp -r "$PROJECT_DIR/src/qml" "$RELEASE_DIR/"
cp "$PROJECT_DIR/main.qml" "$RELEASE_DIR/"
cp -r "$PROJECT_DIR/image" "$RELEASE_DIR/"
cp -r "$PROJECT_DIR/sound" "$RELEASE_DIR/"
cp -r "$PROJECT_DIR/shaders" "$RELEASE_DIR/"

# 6. 复制 MinGW 运行时库
echo "复制 MinGW 运行时库..."
cp $QT_DIR/bin/libgcc_s_seh-1.dll "$RELEASE_DIR/"
cp $QT_DIR/bin/libstdc++-6.dll "$RELEASE_DIR/"
cp $QT_DIR/bin/libwinpthread-1.dll "$RELEASE_DIR/"

# 7. 创建 qt.conf
echo "创建 qt.conf..."
cat > "$RELEASE_DIR/qt.conf" << 'EOF'
[Paths]
Imports = qml
Qml2Imports = qml
EOF

# 8. 复制 QML 插件（关键步骤！）
echo "复制 QML 插件..."
cp -r "$BUILD_DIR/Oran7UI" "$RELEASE_DIR/"

# 9. 创建 README
echo "创建 README..."
cat > "$RELEASE_DIR/README.txt" << 'EOF'
Oran7MediaClient - Release 版本
================================

使用说明:
1. 双击 Oran7MediaClient.exe 启动应用程序
2. 无需安装，直接运行即可

系统要求:
- Windows 10/11 (64位)
- 至少 4GB 内存推荐
- 显卡支持 Direct3D 11 或 OpenGL 3.3 或更高版本
EOF

# 10. 验证打包结果
echo "验证打包结果..."
echo "打包目录大小:"
du -sh "$RELEASE_DIR"
echo "DLL 文件数量:"
cd "$RELEASE_DIR" && find . -name "*.dll" | wc -l

echo "打包完成！发布目录: $RELEASE_DIR"
```

## 常见问题

### Q1: 程序无法启动，鼠标闪烁后无反应
**A:** 检查是否缺少 `Oran7UI/Impl/Oran7UI_Implplugin.dll` 文件。这是必需的 QML 插件。

### Q2: 提示缺少 DLL 文件
**A:** 确保已复制所有 MinGW 运行时库（libgcc_s_seh-1.dll, libstdc++-6.dll, libwinpthread-1.dll）

### Q3: windeployqt 找不到 Qt 模块
**A:** 确保 Qt 路径正确，或者直接在 Qt 安装目录的 bin 文件夹中运行 windeployqt

### Q4: 打包后程序无法找到资源文件
**A:** 检查 qt.conf 文件是否正确创建，确保资源文件（image、sound、shaders）都已复制

## 文件结构说明

打包后的目录结构：

```
Oran7MediaClient-Release/
├── Oran7MediaClient.exe              # 主程序
├── Oran7UI_Impl.dll                  # QML 模块实现
├── Oran7UI/                          # QML 模块目录
│   └── Impl/
│       ├── Oran7UI_Implplugin.dll    # QML 插件（关键！）
│       ├── qmldir                    # QML 模块定义
│       └── *.qmltypes                # QML 类型定义
├── Qt6*.dll                          # Qt 运行时库
├── SDL2.dll                          # SDL2 音频库
├── av*.dll                           # FFmpeg 库
├── platforms/                        # 平台插件
│   └── qwindows.dll                  # Windows 平台插件
├── qml/                              # Qt QML 模块
│   ├── QtQuick/
│   ├── QtQml/
│   └── ...
├── image/                            # 图片资源
├── sound/                            # 音频资源
├── shaders/                          # 着色器文件
├── SQLite/                           # SQLite 数据库
├── qt.conf                           # Qt 配置文件
└── README.txt                        # 使用说明
```

## 总结

打包过程的关键点：
1. **windeployqt** - 自动部署 Qt 依赖
2. **MinGW 运行时库** - 程序运行必需
3. **QML 插件** - 最重要但容易被遗漏
4. **资源文件** - 图片、声音等应用资源
5. **qt.conf** - 配置 Qt 插件路径

按照以上步骤操作，可以成功打包出可在其他 Windows 系统上运行的独立版本。