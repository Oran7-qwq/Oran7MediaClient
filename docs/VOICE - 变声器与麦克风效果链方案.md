---
status: 📝 方向修订：独立模块 + 可选虚拟麦克风驱动
updated: 2026-10-29
---

# 变声器与麦克风效果链（VOICE）

> 结论先说：变声器应该是一套**独立的 Voice Engine**，直播工作台只是它的一个消费者；要覆盖"游戏组队麦"这种系统级场景，还需要可选的**虚拟麦克风驱动**。
> 入口修订（2026-10-29）：**独立左栏页 + 托盘/热键 + 工作台内嵌**，三者共用同一引擎；不再把变声器绑死在直播工作台。
> 本文给出三种系统级实现路线（虚拟声卡 / APO / 第三方虚拟线）、成本与风险，以及分阶段落地。

## 0. 两种形态与入口（已定）

变声器有两个使用面，必须共用一套 DSP/参数/预设，不能各写一份：

| 形态 | 场景 | 入口 |
|---|---|---|
| **独立模块** | 游戏组队麦、Discord/微信语音、录旁白、自己玩 | 左栏"变声器"页 + 系统托盘 + 全局热键 |
| **工作台内嵌** | 直播推流、录制解说、练歌 | 工作台工具栏的"麦克风/变声"卡片 |

独立模块是"引擎 + 控制面板"；工作台是"引擎 + 录制/推流路由"。区别只在输出接哪里，DSP 完全一致。

### 工作台集成

现有 `Oran7RecordingToolbar.qml` 已经预留了"系统声音 / 麦克风·预览"两个卡片（`:138-207`），但注释明确写着"仅为 UI 草稿，不写入录制配置，也不宣称后端已生效"。V0 就是把第二个卡片变成真的，并把"变声"挂到它上面——不新增左栏项，不改工作台主布局。

### 布局

```
┌──────────────────────────────────────────────────────────────┐
│ [系统声音 🔊 ────●] [麦克风 🎤 ──●──] [变声 ✦]   [开始录制][停止] │  ← 工具栏（86px）
├──────────────────────────────────────────────────────────────┤
│ 预览 / 画面                                                   │
│                                                               │
│                                  ┌─────────────────────────┐  │
│                                  │ 变声面板（弹出/抽屉）    │  │
│                                  │ 预设：原声/少女/大叔/…  │  │
│                                  │ 音高 ──●──  共振峰 ──●──│  │
│                                  │ 干湿 ──●──  监听 [ off ]│  │
│                                  │ 降噪 [on]  AGC [on]     │  │
│                                  │ 输入电平 ▮▮▮▯▯          │  │
│                                  └─────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

- 工具栏第二个卡片保留"开关 + 音量 + 电平条"，点击卡片文字/齿轮打开变声面板。
- 变声面板是 `Popup`/右侧抽屉，不占预览面积；录制中也可打开。
- 设备选择放在面板顶部（下拉），因为工具栏宽度有限。

### 状态规则

| 项 | 规则 |
|---|---|
| 麦克风开关 | 关闭时整条麦克风链路不采集、不参与混音；状态持久化 |
| 设备切换 | 录制/预览 busy 时禁用（避免中途断流）；空闲时立即重建采集 |
| 变声预设/音高/干湿 | **录制中可实时改**，这是变声器的核心体验 |
| 监听开关 | 默认关；录制中可开关；检测到扬声器输出时提示啸叫风险 |
| 降噪/AGC | 录制中可改；默认开 |
| 电平表 | 采集线程写原子峰值，GUI 20Hz 读取；不在音频线程画 UI |
| 权限失败 | `E_ACCESSDENIED` → 提示并提供"打开麦克风隐私设置"（`ms-settings:privacy-microphone`） |
| 设备拔出 | 采集线程退出 → GUI 收到通知 → 恢复默认设备或提示 |

### 状态所有权

- `Oran7VoiceEffects`（C++，工作台/Client 独占）是唯一真相：设备、开关、预设、音高、干湿、监听、降噪。
- QML 只做绑定与命令，不保存第二份；不需要像 `captureDraft` 那样的草稿（设备/预设是用户显式动作，立即生效）。
- 录制状态仍归 `Oran7RecordingSession`，语音状态不复制 `busy`，只读取它来决定控件可用性。


## 1. 先纠正一个前提

当前项目**没有任何麦克风采集**：

- `Oran7WasapiAudioWorker` 只做**系统回环**（`GetDefaultAudioEndpoint(eRender, ...)` + `AUDCLNT_STREAMFLAGS_LOOPBACK`，`Oran7WasapiAudioWorker.cpp:185-209`），输出 S16/48k/立体声。
- 录制链路是 `WASAPI loopback → 音频桥线程 → Oran7AudioEncoder → MP4/WHIP`（`Oran7RecordingBackend.cpp:110-160`）。
- 没有 `QAudioSource`、没有输入设备枚举、没有麦克风权限处理。

所以"变声器"的第一步不是 DSP，而是**补一条麦克风输入链路**。这也正好是直播工作台目前最大的缺口（现在只能录"电脑里发出的声音"，不能录人声）。

## 2. 模块定位：Audio Input & Voice FX Rack

```
麦克风 capture ──→ [降噪/AGC/门限] ──→ [变声链] ──┐
系统回环 capture ────────────────────────────────┼──→ [混音] → [限幅] ──┬──→ 本地监听（耳机）
播放器 PCM（可选，练歌/连麦）──────────────────────┘                     ├──→ 录制音频编码（AAC）
                                                                        └──→ WHIP 音频轨（Opus）
```

要点：

- **效果只作用于麦克风**，系统声/音乐只参与混音，不被变声。
- 混音后再限幅，保证推流/录制不削波。
- 本地监听走**独立低延迟输出**，不要绕经编码器，否则延迟 100ms+，无法跟唱。
- 三路输出共享同一份混音结果，但监听可以单独静音/调音量。

### 与现有模块的边界

| 现有 | 变化 |
|---|---|
| `Oran7WasapiAudioWorker` | 拆成"渲染回环采集"和"麦克风采集"两种模式，或新增 `Oran7MicCaptureWorker`；输出统一为 float32/48k |
| `Oran7RecordingBackend` | 音频源从"单个 loopback"改为"混音器"；效果链在混音前 |
| `Oran7AudioEncoder` | 不变（吃混音后的 PCM） |
| `Oran7WhipPublisher` | 不变（Opus 轨道吃同一编码器） |
| `Oran7RecordingFeed` | 目前只共享视频编码器；音频混音状态另建共享对象，不要塞进视频 Feed |
| 直播工作台 UI | 新增"麦克风/变声"卡片；录制文件列表与剪辑作为工作台内的 Tab |
| 新左栏页 | **变声器**（独立模块）；练歌房为可选下游（§7） |

## 2.5 系统级虚拟麦克风（游戏/语音软件）

这是"变声器"从应用内功能变成系统工具的关键，也是成本最高的一步。

### 为什么用户态做不到

游戏/语音软件（Discord、微信、CS、Valorant…）枚举的是 **Windows 音频采集端点**（MMDevice），然后通过 WASAPI/Media Foundation 读取。一个普通 Qt 进程无法把自己"插入"到别人的采集链路里。想让它们看到"Oran7 Microphone"，只有三条路：

| 路线 | 原理 | 优点 | 代价 | 结论 |
|---|---|---|---|---|
| **A. 虚拟音频设备驱动** | 基于 WDK `sysvad` 派生一个虚拟声卡，暴露一个采集端点；用户态服务把变声后的 PCM 写进驱动 | 所有应用都能选到"Oran7 Mic"；支持独占/共享；体验最干净 | 内核驱动开发 + EV 证书 + 微软 attestation 签名 + 安装器 + 蓝屏/兼容性测试；约 1–3 个月 | **目标形态**，但必须单独立项 |
| **B. APO / 音频过滤驱动** | 在真实麦克风端点上装一个 Audio Processing Object，在 `audiodg.exe` 里处理 | 不新增设备，游戏里仍选原麦克风；安装包比完整驱动小 | 仍是签名驱动包；独占/RAW 模式会绕过；影响该端点上的所有应用；调试困难 | 备选，不如 A 直观 |
| **C. 第三方虚拟线** | 用户装 VB-Cable/VoiceMeeter，我们的应用把变声结果输出到"虚拟线"，游戏选"虚拟线 Output" | 零驱动开发，立刻能用 | 依赖第三方安装与授权；设备名不是我们的；多一步配置 | **短期最现实** |
| D. API Hook / 注入 | 拦截游戏的 WASAPI 调用 | 看似"无驱动" | 反作弊必封、极不稳定、法律与口碑风险 | ❌ 绝对不做 |

### 推荐路线：C 先用，A 后做

```
现在：Oran7 Voice Engine → 输出到扬声器/虚拟线（用户装 VB-Cable）
以后：Oran7 Voice Engine → IOCTL/共享内存 → Oran7 Virtual Mic 驱动 → 游戏采集
```

这样独立变声器页、托盘、热键、预设都能先落地，不被驱动阻塞；驱动作为**可选组件**单独安装（"安装系统麦克风"按钮 → 下载/安装驱动）。

### 虚拟麦克风驱动的架构（若做 A）

```
[Oran7 Voice Engine（用户态，Qt/C++）]
   │  48k float 环形缓冲 + IOCTL / 共享内存
   ▼
[Oran7 Virtual Mic（内核态，sysvad 派生）]
   │  暴露 "Oran7 Virtual Microphone" 采集端点
   ▼
[游戏 / Discord / 微信 / OBS]  ← 选这个麦克风
```

- **DSP 全部留在用户态**：驱动只做环形缓冲、格式转换、时间戳；改预设不需要重装驱动。
- **控制协议要版本化**：驱动与引擎版本不匹配时拒绝启动并提示，避免"无声/爆音"。
- **失效安全**：引擎崩溃/退出时驱动输出静音，不能输出垃圾数据或卡死。
- **延迟**：驱动 5–10ms + 引擎 5ms，目标端到端 <20ms。
- **安装**：管理员权限、UAC、卸载/回滚、驱动仓库清理；安装失败要有明确错误码。

### 签名与分发（必须提前算成本）

- EV 代码签名证书（约 $300–500/年）+ Microsoft Partner Center attestation 签名（免费但需要 EV）。
- 驱动必须 64 位、通过 HLK/attestation；不签名无法在默认 Windows 上安装。
- 杀毒/反作弊可能对未知虚拟声卡报警；需要白名单沟通与长期签名信誉。
- 企业/网吧环境可能禁止安装驱动；要提供"免驱动模式（虚拟线）"兜底。

### 反作弊与兼容性

- 签名良好的虚拟音频设备通常被视为普通声卡，但**不能保证**所有反作弊/游戏允许；部分游戏会检测虚拟设备。
- 不做注入、不做 API Hook、不改游戏进程。
- 需要一份"已知不兼容游戏"清单与回退建议（改用真实麦克风）。

## 3. 延迟预算（决定体验成败）

| 环节 | 目标 | 说明 |
|---|---|---|
| WASAPI 采集 | 5–10ms | 事件驱动；共享模式；必要时 `IAudioClient3` 低周期 |
| 降噪/变声 DSP | <2ms | 10ms 块，预分配，无锁 |
| 混音/限幅 | <0.2ms | 同上 |
| 本地监听输出 | 5–10ms | 独立 WASAPI 渲染，不复用 SDL 播放器（SDL 缓冲会再加 20–40ms） |
| **端到端监听** | **< 30ms** | 超过 40ms 跟唱/说话会明显别扭 |
| 推流/录制链路 | 采集+DSP+编码 20ms | 与监听并行，互不阻塞 |

设计原则：**监听链路和编码链路解耦**。编码器队列满时只能丢帧/降级，不能反压监听。

## 4. 变声能力分级

### V1：DSP 变声（低延迟、可离线、无模型）

| 效果 | 实现 | 难度 | 备注 |
|---|---|---|---|
| 音高 ±12 半音 | Sonic（仓库已有，Apache-2.0/public domain）或 signalsmith-stretch（MIT） | 低 | 会连带改变共振峰 → 花栗鼠/低沉 |
| 机器人 | 环形调制（载波 50–200Hz）+ 轻失真 | 低 | 经典效果 |
| 电台/广播 | 带通 300–3400Hz + 压缩 + 噪声门 | 低 | 复用 EQ/压缩 |
| 怪兽 | 音高 -5 + 低频提升 + 软削波 | 低 | — |
| 空灵 | 音高 +7~+12 + 混响 + 短延迟 | 低 | 复用空间模块 |
| 和声 | 2–3 个移调副本（±3/±7 半音）+ 去相关延迟 | 中 | CPU ×2–3 |
| 电音（音高修正） | YIN/MPM 基频检测 + 向最近半音修正 + 平滑 | 中 | 不要用 Auto-Tune 商标 |
| 共振峰偏移 | STFT + 倒谱包络弯折（或 LPC 包络） | 中高 | **决定"像不像女声/男声"**，V2 再做 |
| 降噪 | rnnoise（BSD-3）或 speexdsp（BSD 系） | 低 | 48k/10ms 帧，约 1% CPU |
| AGC/门限 | speexdsp 或自写 | 低 | 直播必备 |

> 关键认知：**只调音高 = 花栗鼠**。要"自然变声"必须有共振峰控制；V1 先用预设组合（音高+EQ 倾斜）做到"能玩"，V2 补共振峰。

### V2：共振峰与音乐化

- 共振峰偏移（独立于音高）：让"大叔→少女"听起来自然。
- 和声/二重唱、延迟/混响、齿音抑制（de-esser）。
- 闪避（ducking）：说话时自动压低背景音乐，练歌/播客可用。
- 噪声门 + 呼吸声抑制。

### V3：AI 变声（可选，重）

- RVC / so-vits-svc 类模型，ONNX Runtime（MIT）推理。
- 现实约束：模型 50–200MB、端到端延迟 50–200ms、CPU 占用高、GPU 更佳；不适合实时监听，适合"录制后转换"或"直播但接受延迟"。
- **不随包分发模型**：模型授权、训练数据授权、肖像/声音同意都有风险；只做"模型管理器 + 用户自备模型"。
- 参考项目 `w-okada/voice-changer` 的许可证为 NOASSERTION（非标准），**不移植代码**，只参考交互与管线。

## 5. 架构与线程

```
GUI（Oran7VoiceEffects）──seqlock 参数块──▶ 音频线程（Oran7VoiceProcessor）
                                                  │
WASAPI 采集线程 ──ring buffer──▶ 混音/DSP ──┬──▶ 监听输出线程
                                            ├──▶ 编码器输入队列
                                            └──▶ 电平表（原子值 → GUI 定时器读取）
```

- 参数块复用 `AUDIO` 方案里的无锁 seqlock（GUI 唯一写者，音频线程只读）。
- 音频线程禁止：`new/delete`、mutex、Qt 信号、日志、文件。
- 设备热插拔：`IMMNotificationClient` 回调只置原子标志，GUI 定时器轮询后重建链路。
- 麦克风格式（44.1/48/96k、单/双声道）在采集侧统一到 48k stereo float，DSP 只认一种格式。
- 监听输出与采集设备可能是同一个（扬声器）→ **默认关闭监听**，检测到扬声器时弹"可能啸叫"提示。

## 6. C++ 接口

```cpp
// 纯 DSP，无 Qt
class Oran7VoiceProcessor {
public:
    void prepare(int sampleRate, int channels);
    void process(float* interleaved, int frames);   // 原地
    void reset();
    void setParameters(std::shared_ptr<Oran7VoiceParameterBlock>);
    int latencyFrames() const;
};

// QML 状态（EXE，Client 或工作台独占）
class Oran7VoiceEffects : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled ...)
    Q_PROPERTY(QString preset ...)
    Q_PROPERTY(double pitchSemitones ...)
    Q_PROPERTY(double formantSemitones ...)
    Q_PROPERTY(double mix ...)          // 干湿比
    Q_PROPERTY(bool monitorEnabled ...)
    Q_PROPERTY(bool noiseSuppression ...)
    Q_PROPERTY(QString inputDeviceId ...)
    Q_PROPERTY(double inputGainDb ...)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(double inputLevel READ inputLevel NOTIFY inputLevelChanged)
    Q_INVOKABLE void refreshDevices();
    Q_INVOKABLE void applyPreset(const QString& name);
    Q_INVOKABLE void reset();
    Q_INVOKABLE bool savePreset(const QString& name);
};
```

- 设备枚举与采集：新增 `Oran7MicCaptureWorker`（WASAPI `eCapture`，事件驱动），接口与 `Oran7WasapiAudioWorker` 对齐：`start(config)` / `stop()` / `getPcmFrame()`。
- 混音器：`Oran7VoiceMixer`，输入 `mic/system/music` 三路 ring buffer，输出 float32。
- 参数注入：`Oran7RecordingBackend` 在创建音频编码器前拿到 `shared_ptr<Oran7VoiceProcessor>`；录制停止时按代际销毁。
- 配置：`voice` 分组（见 §8），由 `Oran7ConfigBinding`/Client 在启动/退出边界读写。

## 7. 可选下游：练歌房 / 翻唱（本期不做）

变声器 + 播放器 + 歌词 + 录制可以组合出一个真正新颖的左栏页，但它**不是本期交付**，也不阻塞工作台集成：

- **页面**：`KaraokeStack.qml`（左栏"练歌房"），等 V1/V2 变声稳定后再评估。
- **流程**：选歌 → 歌词滚动 → 麦克风 + 变声 → 混音监听 → 录制/导出。
- **复用**：播放内核、`Oran7LyricsService`、录制会话、Voice FX。
- **前置**：V2 的共振峰、监听延迟 <30ms、录制混音稳定。

同一套能力也可以扩展为：播客录制（人声+背景乐）、语音备忘录、直播解说。

## 8. 配置

```json
"voice": {
  "enabled": false,
  "input_device": "",
  "input_gain_db": 0.0,
  "monitor_enabled": false,
  "noise_suppression": true,
  "agc_enabled": true,
  "preset": "natural",
  "pitch_semitones": 0.0,
  "formant_semitones": 0.0,
  "mix": 1.0,
  "effects": ["pitch", "eq", "compressor"],
  "custom_presets": []
}
```

- 默认关闭、默认监听关闭（防啸叫）。
- 设备 id 用 WASAPI endpoint id（不是显示名），设备不存在时回退默认。
- 与 `AUDIO` 的 `audio` 分组分开：`audio` 是"播放输出音效"，`voice` 是"麦克风输入效果"。

## 9. 分阶段

### V0：引擎 + 独立模块（无变声，但已经有用）

- 输入设备枚举/热插拔、WASAPI 采集、48k float 统一、电平表。
- 噪声门 + 降噪 + AGC；混音器可接录制/推流，也可只走监听。
- **独立左栏页"变声器"**：设备、增益、电平、监听、试音；托盘常驻 + 全局热键。
- 工作台"麦克风·预览"卡片替换为真实控件，复用同一引擎。
- 验收：独立页能试音/录音；直播/录制能录到人声；插拔设备不崩；监听延迟 <30ms。

**V0 文件清单**

```
新增：
  src/cpp/Oran7Voice/Oran7MicCaptureWorker.h/.cpp
  src/cpp/Oran7Voice/Oran7VoiceMixer.h/.cpp
  src/cpp/Oran7Voice/Oran7VoiceProcessor.h/.cpp（V0 只做 NS/AGC/门限/增益）
  src/cpp/Oran7Voice/Oran7VoiceEffects.h/.cpp（QObject 控制面）
  src/qml/RightPage/MainStackView/VoiceStack.qml
  src/qml/Components/Oran7GlobalComponents/Oran7VoicePanel.qml
  tests/voice/（采集格式、混音、延迟、零分配）

修改：
  Oran7WasapiAudioWorker.h/.cpp        拆出采集/回环共用格式转换
  Oran7RecordingBackend.cpp            音频源从 loopback 改为 VoiceMixer
  Oran7RecordingToolbar.qml            替换"麦克风·预览"占位卡片 + 变声入口
  Oran7MediaClient.h/.cpp              持有 VoiceEffects、配置映射、暴露 Q_PROPERTY
  Oran7AppConfig.h/.cpp + ConfigKeys   新增 voice 分组
  config/default.json                  voice 默认值（关闭/监听关）
  BasicConfig.qml / RightPage.qml      新增 voicePage 枚举与导航
  LeftPage.qml                         新增"变声器"入口
  main.cpp                             托盘/热键/单例注册（如需要）
  qml.qrc                              新 QML
  translations/Oran7UiMessages.h       新文案
```

**不动的东西**：`Oran7RecordingSession` 的状态机、`Oran7RecordingFeed`（视频）、WHIP/MP4 封装逻辑。音频只是在"进编码器之前"多了一级混音。

### V1：DSP 变声（可玩）

- 音高、机器人、电台、怪兽、空灵、和声、混响。
- 预设网格 + 干湿比 + 试听；参数持久化。
- 验收：预设切换无明显爆音；音高准确；CPU <3%。

### V2：自然变声

- 共振峰偏移、电音修正、闪避、齿音抑制。
- 验收："男→女/女→男"主观可辨且不花栗鼠；长时间运行稳定。

### V3：AI 变声（可选）

- ONNX Runtime + 用户自备模型；录制后转换优先，实时模式标注延迟。
- 验收：模型加载/回退、延迟提示、CPU/GPU 占用可接受；不随包分发模型。

### S0：独立模块（不依赖驱动）

- 左栏"变声器"页 + 系统托盘 + 全局热键（按住说话/切换预设）。
- 独立窗口：设备、预设、音高/共振峰、监听、录制/试听。
- 输出可选手动路由到第三方虚拟线（检测到 VB-Cable/VoiceMeeter 时提示"路由到虚拟线"）。
- 验收：不开游戏也能试音/录音；托盘常驻；热键在游戏前台可用。

### S1：驱动内虚拟麦克风（可选组件，单独立项）

- `sysvad`（MS-PL）派生虚拟采集设备；用户态引擎通过 IOCTL/共享内存喂 PCM。
- EV 证书 + attestation 签名 + 安装/卸载器；引擎与驱动版本握手。
- 验收：游戏/Discord 的设备列表出现 "Oran7 Virtual Microphone"；采样率/独占模式兼容；崩溃时输出静音。

### S2：游戏配置

- 按进程名匹配游戏 → 自动切预设/增益；启动游戏时提示"是否切换麦克风"。
- 已知不兼容清单与回退；一键恢复真实麦克风。

## 10. 测试与验收

离线（`tests/voice`）：

- 采集格式转换：44.1/48/96k、单/双声道、16/24/32bit。
- 音高准确性：220Hz 正弦 +12 半音 → 440Hz ±1%。
- 延迟：环回脉冲测量，监听链路 <30ms。
- 稳定性：30 分钟白噪/语音，无 NaN、无丢帧、无 denormal 拖慢。
- 零分配：`prepare()` 后 `process()` 不得调用 `new`。
- 参数并发：写线程狂改预设，读线程无撕裂。
- 反馈保护：监听开启 + 扬声器输出时给出警告/自动静音。

用户验收（不代跑）：

- 直播/录制人声清晰、无回声；耳机监听无啸叫。
- 蓝牙耳机（HFP）与 USB 麦克风切换；设备热插拔。
- 变声预设主观听感；长时间直播稳定性。

## 11. 许可证与参考（已核实）

| 项目 | 许可证 | 用法 |
|---|---|---|
| Sonic | Apache-2.0 / public domain（仓库内 `Sonic.h:6`） | 音高/时间伸缩，直接用 |
| signalsmith-stretch | **MIT** | 更高质量的移调，可替换 Sonic |
| rnnoise | **BSD-3** | 降噪 |
| speexdsp | BSD 系（以 COPYING 为准） | AGC/AEC/降噪备选 |
| ONNX Runtime | **MIT** | V3 推理 |
| RVC（代码） | **MIT** | V3 参考；模型不随包分发 |
| w-okada/voice-changer | **NOASSERTION**（非标准） | ❌ 不移植，只参考交互 |
| Rubber Band | GPL-2 | ❌ 不用（闭源不兼容） |
| WORLD | 修改版 BSD（需核实） | 共振峰/声码器备选 |
| Windows-driver-samples（sysvad） | **MS-PL** | 虚拟麦克风驱动的基础；保留版权与许可声明 |
| Scream（虚拟声卡） | **MS-PL** | 驱动/IOCTL 结构参考（它是输出设备，不是麦克风） |
| VB-Cable / VoiceMeeter | 专有/捐赠版 | ❌ 不随包分发；只检测并引导用户自行安装 |

## 12. 风险

| 风险 | 对策 |
|---|---|
| Windows 麦克风隐私权限关闭 | 明确错误提示 + 跳转设置引导 |
| 监听啸叫 | 默认关闭；检测扬声器；一键静音 |
| 蓝牙 HFP 8/16k | 检测设备类型并提示"音质受限" |
| 独占模式被 Discord/OBS 占用 | 共享模式回退 |
| 变声延迟影响说话 | 监听链路独立、低延迟；AI 模式明确标注 |
| AI 模型授权/声音同意 | 不随包分发；用户自备；使用提示 |
| 变声被用于冒充 | 产品文案注明"娱乐用途/需对方同意" |
| **驱动签名/安装失败** | EV 证书 + attestation；安装错误码与回退到虚拟线 |
| **反作弊拦截虚拟设备** | 不做注入/Hook；提供不兼容清单与"恢复真实麦克风" |
| **驱动蓝屏/兼容性** | sysvad 派生、静态分析 + 多版本 Win10/11 测试、驱动隔离 |
| **免驱动模式依赖第三方** | 检测 VB-Cable/VoiceMeeter；只引导不打包 |

## 13. 待确认

| # | 问题 | 状态 / 建议 |
|---|---|---|
| 1 | 变声器入口 | ✅ **已修订（2026-10-29）：独立左栏页 + 托盘/热键 + 工作台内嵌，共用同一引擎** |
| 2 | 系统级虚拟麦克风走哪条路？ | 待定；建议 **先用第三方虚拟线（C）**，驱动（A）单独立项 |
| 3 | 是否愿意承担驱动签名/安装/反作弊兼容成本？ | 待定；需要 EV 证书与长期维护，属于"另一个产品" |
| 4 | V1 是否接受"音高+EQ"的简单变声（会有花栗鼠感）？ | 待定；建议接受，V2 补共振峰 |
| 5 | 练歌房左栏页？ | 待定；本期不做，V2 后再评估 |
| 6 | AI 变声是否纳入路线？ | 待定；建议放 V3，可选，不阻塞 V1/V2 |
| 7 | 监听默认关还是开？ | 待定；建议默认关，检测耳机后提示可开 |

## 14. 关联

- 播放输出音效：[[AUDIO - 音效均衡器方案]]（复用参数块、biquad、限幅；混响待 AUDIO A3）
- 录制/推流：[[Capture - 录屏与推流]]
- 线程规则：[[ASYNC - 异步与线程]]
- 配置：[[Config - 配置与持久化]]
- UI 约定：[[UI - QML界面]]
