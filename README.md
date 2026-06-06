#  Oran7MediaClient

<div align="center">

一个基于Qt6 和 FFmpeg 6.0 的现代化桌面多媒体客户端，支持音视频播放、B站直播等功能。

</div>

---

## ✨ 功能特性

### 🎬 视频播放
- 基于 **FFmpeg 6.0** 的多格式视频解码
- **Direct3D 11** 硬件加速渲染，低CPU占用
- 支持 Fit / Fill 等多种画面缩放模式
- 像素级对齐渲染，画质清晰

### 🎵 音乐播放
- 本地音乐文件播放（MP3 / FLAC / AAC / WAV 等）
- **歌词同步显示**（LRC 格式）
- 可视化音频频谱
- 播放列表管理（添加 / 删除 / 排序）
- 最近播放记录

### 📺 B站直播
- B站直播间地址解析与播放，提供登录接口拉取原画1080p高帧率

### 🎨 现代化 UI
- **QML 6.0** + QuickControls2 构建
- **毛玻璃背景模糊**效果
- **流畅动画过渡**
- **自定义主题系统**（JSON 配置，支持切换主题）
- 无边框窗口设计（基于 QWindowKit）
- 内置调试终端

### 🔧 其他特性
- 客户端-服务器架构（QTcpSocket）
- SQLite 本地数据持久化
- JSON 配置文件管理
- 窗口位置/大小记忆
- 音量设置持久化
- DPI 感知适配
- 独显优先调用（NVIDIA / AMD）

## 📌 预览

### Oran7MediaPlayer — 稳定 4K 60fps
![Oran7MediaPlayer 界面](image/screenshot_player.png)

### Oran7MusicPlayer
![Oran7MusicPlayer 界面1](image/screenshot_music1.png)
![Oran7MusicPlayer 界面2](image/screenshot_music2.png)

### GUI 设置
![GUI 设置界面](image/screenshot_settings.png)

---

---

## 🏗️ 技术架构


### 模块划分

| 模块 | 说明 |
|------|------|
| `GlobalSingleton` | 全局单例管理：应用上下文、配置管理、异步任务 |
| `Oran7MediaPlayer` | FFmpeg 播放器封装，B站直播地址解析 |
| `Oran7VideoGPURender` | D3D11 硬件加速视频渲染 |
| `Oran7ScreenCapture` | 屏幕录制模块 |
| `Oran7Sever` | 服务器端网络通信 |
| `Oran7Utils` | 工具库（日志、音效等） |
| `QtQuicCppExtend` | QML C++ 扩展（主题系统、窗口代理、文件助手） |

---

## 🚀 快速开始

### 环境要求

| 依赖          | 版本                           |
| ----------- | ---------------------------- |
| **Windows** | 11 (64-bit)                  |
| **Qt**      | 6.7.2 (MinGW 64-bit)         |
| **CMake**   | 3.16+                        |
| **编译器**     | MinGW 64-bit (GCC)           |
| **显卡**      | 支持 Direct3D 11 或 OpenGL 3.3+ |

> 💡 其他版本的 Qt6 也可以尝试，但建议使用 6.7.2 MinGW 64-bit。

### 构建步骤

```bash
# 1. 克隆仓库
git clone https://github.com/Oran7-qwq/Oran7MediaClientV0.7.git
cd Oran7MediaClientV0.7

# 2. 创建构建目录
mkdir build && cd build

# 3. 运行 CMake 配置
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=/d/Qt/6.7.2/mingw_64

# 4. 编译
cmake --build . --config Release

# 5. 运行（编译后 FFmpeg DLL 和 SQLite 会自动复制到输出目录）
./Oran7MediaClient.exe
```

### 目录结构

```
Oran7MediaClientV0.7/
├── main.cpp                  # 程序入口
├── main.qml                  # 主 QML 界面
├── CMakeLists.txt            # CMake 构建配置
├── LICENSE                   # MIT 许可证
├── PACKAGING_GUIDE.md        # 打包发布指南
├── app.rc                    # Windows 资源文件
├── src/
│   ├── cpp/
│   │   ├── GlobalSingleton/  # 全局单例模块
│   │   ├── Oran7MediaPlayer/ # FFmpeg 播放器
│   │   ├── Oran7VideoGPURender/ # D3D11 渲染
│   │   ├── Oran7ScreenCapture/  # 屏幕录制
│   │   ├── Oran7Sever/       # 服务器模块
│   │   ├── Oran7Utils/       # 工具库
│   │   └── QtQuicCppExtend/  # QML C++ 扩展
│   └── qml/
│       ├── LeftPage/         # 左侧面板
│       ├── RightPage/        # 右侧内容区
│       ├── BottomPage/       # 底部控制栏
│       ├── Components/       # 通用组件
│       ├── Settings/         # 设置页面
│       └── Basic/            # 基础组件
├── ffmpeg6.0/                # FFmpeg 6.0 头文件与库
├── 3rdparty/
│   └── qwindowkit/           # 无边框窗口库
├── config/                   # 配置文件
│   └── themeJson/            # 主题 JSON 配置
├── image/                    # 图片资源
├── sound/                    # 音频资源
├── shaders/                  # 着色器文件
└── SQLite/                   # SQLite 数据库
```

---

## 📦 打包发布

详细的打包步骤请参考 [PACKAGING_GUIDE.md](./PACKAGING_GUIDE.md)。

核心流程：
1. 构建 Release 版本
2. 使用 `windeployqt` 自动部署 Qt 依赖
3. 手动复制 MinGW 运行时库
4. 复制 QML 插件 `Oran7UI/`
5. 创建 `qt.conf` 配置文件

---

## 🔑 关键技术点

### D3D11 硬件渲染
视频帧通过 FFmpeg 解码后，直接上传到 D3D11 纹理，由 GPU 完成 YUV→RGB 转换和渲染，大幅降低 CPU 占用。

### 主题系统
基于 JSON 配置的完整主题引擎，支持自定义：
- 颜色（主色 / 辅色 / 背景色）
- 圆角半径
- 组件尺寸
- 字体样式

### 异步架构
- 文件扫描在后台线程执行，不阻塞 UI
- FFmpeg 解码独立线程
- 网络请求异步处理

---

##  贡献

欢迎提交 Issue 和 Pull Request！

本项目为个人开发项目，如果你有好的想法或发现了 bug，欢迎通过 GitHub Issues 反馈。

---

## 📄 许可证

本项目使用 **MIT License** - 详见 [LICENSE](./LICENSE) 文件。

### 第三方库许可

| 库 | 许可证 |
|----|--------|
| Qt 6.7 | LGPL-3.0 |
| FFmpeg | LGPL-2.1 |
| SDL2 | zlib License |
| SQLite | Public Domain |
| QWindowKit | MIT |

---

## 致谢

- [Qt](https://www.qt.io/) - 跨平台 C++ 框架
- [FFmpeg](https://ffmpeg.org/) - 音视频处理
- [SDL](https://www.libsdl.org/) - 音频输出
- [QWindowKit](https://github.com/stdware/qwindowkit) - 无边框窗口解决方案

---

<div align="center">

**Oran7MediaClient** © 2025-2026 Oran7

</div>
