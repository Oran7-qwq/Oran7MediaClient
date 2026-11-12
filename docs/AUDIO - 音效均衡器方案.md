---
status: ✅ A0/A1/A2 完成，A3 待实施
updated: 2026-11-05
---

# 音效与均衡器实施方案（AUDIO）

> 首批只交付：默认关闭、可实时调节、可保存预设的 **10 段 EQ + 前级 + 输出保护**。
> A0～A2 已实施，A3 待实施。真实设备听感由用户验收；代理只运行隔离检查，不启动主程序。

## 1. 范围与实施顺序

音效作用于本播放器内核处理的音乐、视频与直播音源。它不是操作系统全局音效；背景视频的 QtMultimedia、录屏/推流、麦克风与降噪不接入本链路。

| 阶段 | 交付内容 | 本阶段不做 |
|---|---|---|
| A0-a | 缓冲所有权、设备关闭顺序、参数邮箱、seek 清理 | 不增加音效 UI，先保持原播放能力 |
| A0-b | float 输出适配、音量斜坡、Sonic 兼容边界、旁路 | 不重写 Sonic，不声称变速器内部已浮点化 |
| A1 | 10 段图示 EQ、前级/输出增益、最终限幅、预设、面板与持久化 | 不加入压缩、混响等尚未实现的预设 |
| A2 | 高低通、低音架、压缩、立体声宽度、交叉馈送 | 在 A1 稳定后分项加入 |
| A3 | Room：早期反射与混响 | 独立验证性能、反馈稳定性和尾音 |
| A4 / 另立需求 | 参量 EQ、AutoEQ 导入、频谱、预设分享、可选 HRTF | 不阻塞 A0/A1，HRTF 单独确定数据与延迟方案 |

为降低首版复杂度，A1 固定中心频率为 31、62、125、250、500、1k、2k、4k、8k、16kHz，固定 peaking 类型与 Q=1.41，只开放每段增益（±12dB）。可调频率、Q 和单段开关延后到参量 EQ；届时补完整参数结构与配置迁移。

## 2. 当前代码基线

```text
解码线程 → sampq → SDL 音频回调
                    ├─ audio_resample_frame / swr：S16 交错 PCM
                    ├─ Sonic：非 1.0 倍速
                    ├─ SDL_MixAudioFormat：音量
                    └─ SDL 输出、音频时钟更新
```

主要入口：

| 内容 | 文件 / 函数 |
|---|---|
| 重采样、回调、设备、时钟 | `src/cpp/Oran7MediaPlayer/Oran7FFplayerCore.cpp`：`audio_resample_frame`、`sdl_audio_callback`、`audio_open`、`audio_close` |
| 缓冲与内核状态 | `src/cpp/Oran7MediaPlayer/Oran7FFplayerCore.h` |
| Sonic 格式与扩容 | `src/cpp/Oran7MediaPlayer/Sonic.cpp`：`addFloatSamplesToInputBuffer`、`sonicReadFloatFromStream` |
| 帧队列与 serial | `src/cpp/Oran7MediaPlayer/FFPlay_Def.cpp` 及内核 seek/取帧分支 |
| 准备、退役、音量 | `src/cpp/Oran7MediaClient/Oran7PlaybackController.h/.cpp` |
| 状态与配置组合 | Client、ConfigBinding、AppConfig |

实施前必须明确：

- 本地 Sonic 的 float 接口内部仍转换为 `short`。SDL 请求 F32 不代表变速链全程 float。
- 解码和重采样块长不固定为 1024。DSP 必须接受任意有效帧数，必要时分成有界小块。
- 既有回调已有队列锁、swr/Sonic 扩容及日志。新增 DSP 零分配，不等于整个回调已实时安全。
- `audio_buf` 可能借用 AVFrame 或重采样缓冲，不能默认可原地修改。当前 Sonic 先复制输入再申请输出，不能仅凭别名断言正常路径必然 UAF；失败路径的所有权仍需拆清。
- `audio_close()` 已由 `Oran7SdlAudioDevice` 收敛（关闭在内部加锁、Close 前不再 Lock；`tests/playback/sdl-audio.cpp` 覆盖旧顺序死锁）。A0-a 仍需复核 `decoder_abort → audio_close → decoder_destroy` 与回调停止的先后，并在关闭后再释放 DSP/Sonic/缓冲。

## 3. 分层、所有权与参数发布

```text
QML → Client.audioEffects → Oran7AudioEffects（GUI，参数/预设权威）
                                  │
                       每个播放器独立的参数邮箱
                                  │
                     Oran7AudioProcessor（纯 C++）
                                  │
                       所属播放器音频处理线程
```

- Client 以 `unique_ptr` 持有 AudioEffects，声明早于 PlaybackController；无 QObject parent。AudioEffects 再独占固定十行的 EqualizerModel。
- QML 通过 Client 借用对象，注册不可创建类型并设 CppOwnership；不新增全局单例。
- 每个播放器独占 DSP、独立可写 scratch 和自身参数邮箱。邮箱可用 `shared_ptr` 保证 GUI 发布端与播放器的寿命，但回调内不复制、替换或释放共享所有权。
- GUI 只发布参数值，不对运行中的 DSP 调用 prepare/reset/process。DSP 历史由音频处理线程独占。
- prepare 在设备协商完成、回调仍停用时执行；关闭流程确认回调结束后才释放 DSP、Sonic 与缓冲。设备关闭后不得再解锁该设备。

### 参数邮箱的具体约束

不采用“原子序号 + 普通结构体 memcpy”的 seqlock，事后复核序号不能消除 C++ 数据竞争。

A0 建议采用固定容量的 **SPSC 值队列**，每个播放器各一份：

1. GUI 唯一生产，所属音频线程唯一消费；退役播放器退出 GUI 发布名单，但保留自身邮箱到回调停止。
2. 载荷是固定大小、无 Qt/字符串/动态容器的参数值；使用 `std::uint32_t` 等标准类型。
3. 原子索引按 acquire/release 发布与回收槽位，不覆盖未读槽；验证目标平台索引原子操作无锁。
4. 队列满时，在 GUI 侧合并最新待发参数，用 GUI 定时器短间隔重试。用户停止拖动后，最后一次调整也必须最终送达。
5. 每个处理块最多消费固定容量个槽并取最新完整快照；不能无限追赶写者、等待 GUI 或执行分配/日志。
6. 新播放器在回调启动前带入完整当前参数及 revision；预设切换只发参数，不重建播放器。

不能直接把 atomic shared_ptr、普通双缓冲或同一邮箱多个消费者当作安全替代。队列算法、满队列补交、退役时序单独验收。

## 4. A0 的音频链路约定

### 4.1 信号顺序与格式

```text
重采样 → 变速适配（非 1.0 倍速）→ float PCM
       → 前级 → EQ → 后续可选效果 → 输出增益
       → 播放器音量斜坡 → 最终样本峰值限幅 → 设备格式适配/SDL
```

- EQ 放在变速之后，避免增强后的信号先进入 Sonic 的整数内部路径；DSP 使用实际输出采样率。
- SDL 优先请求 AUDIO_F32SYS，swr 目标为 interleaved FLT。未允许格式变更时，SDL 通常负责内部设备适配；不要把获得不同格式当作正常回退流程。
- F32 打开失败且确需兼容时，关闭失败设备后明确重试 S16。最终边界统一做转换、饱和、字节数计算；不能把 S16 缓冲强转成 float。
- A0 保留现有 Sonic。变速输入转整数前检查有限值并饱和，避免越界转换；1.0 倍速绕过 Sonic。不承诺变速路径位精确或全程 float。
- frameSize、bytesPerSecond、静音长度、偏移、Sonic 样本数和时钟换算全部使用协商格式，不残留固定的 channels×2/4 假设。

### 4.2 缓冲、实时边界与失败处理

- 重采样结果、Sonic 输出、DSP scratch 分开；输入允许只读借用，新增处理使用准备好的可写缓冲。
- 新 DSP 的 process 不分配、不加锁、不记录日志、不抛异常；准备阶段明确容量、声道数和处理块上限，大输入拆块。
- 转换/容量失败时输出已初始化静音并异步报告错误，不读取旧指针、不输出未写入样本、不因零样本无限空转。
- 既有 swr/Sonic 分配不能靠“放在块首”消除。本批记录其边界和处理耗时，不宣称整条回调零分配；若仍超预算，再单独评审加工线程与有界 PCM 队列。

### 4.3 音量、旁路与开关

- 保留现有百分比音量语义，做浮点换算，斜坡按实际样本数计算，目标约 20ms，不依赖回调次数。
- 总开关默认关闭：旁路前级、EQ、输出增益和音效限幅；播放器音量与必要的格式转换仍工作。
- Flat 仅表示 EQ 平直，不等于总关闭。音效开启时 Flat 仍经过输出保护，不能宣称整链路逐位一致。
- 开关、增益、整套预设均需平滑；不在切换瞬间临时分配资源，重新开启不播放关闭前残留历史。
- 位一致旁路测试仅针对纯 DSP、同格式、单位增益且开关过渡结束后的路径；不包含量化、变速或音量斜坡。

### 4.4 seek、EOF 与时钟

- seek 请求仅发布意图，清理与音频帧 serial/媒体代际关联，不能只用无身份的 reset 布尔值。
- 音频端切换到新 serial 时，一并丢弃旧 PCM 待输出部分、Sonic 积压、需要清除的 swr 延迟和 DSP 历史。连续 seek 不得让旧序列重新进入输出。
- EOF 明确 Sonic 排空及结束判定。后续混响尾音、停止/seek 是否丢弃尾音在 A3 单独定义。
- A0/A1 不新增算法缓冲延迟；保留并验证设备缓冲、变速与媒体时间的时钟逻辑。
- HRTF 暂不实施。未来按实际算法报告延迟，不能把一般因果 HRIR 都按线性相位 FIR 计算；128-tap 线性相位 FIR 在 48kHz 的群延迟约 1.32ms，而不是 2.7ms。

## 5. A1 的 DSP、预设与界面

### DSP 与输出保护

- EQ 采用 RBJ peaking biquad、DF2T，double 计算系数；状态精度通过低频噪声和性能测试确定。每声道独立状态，不新承诺播放器尚未支持的多声道布局。
- 拒绝 NaN/Inf，校验增益/Q 等范围；按设备 Nyquist 检查频率，无法表示的高频段临时旁路并提示，不破坏原设置。
- 参数按样本时间平滑，小块更新系数或采用经过稳定性验证的过渡；不能只在大块边界跳换系数。
- A1 同时交付最终限幅：位于所有增益/效果之后、最终格式适配之前，之后不得再增加增益。
- 音效开启时保护默认启用，初始 ceiling=-1dBFS；总关闭时整体旁路。MVP 定义为零新增缓冲延迟的样本峰值保护，不宣传 true-peak 或零失真。
- 输出必须满足定义的 ceiling。未标定的 tanh 不能代替 ceiling 保证；多声道链接策略须覆盖全部处理声道，立体声用共同衰减避免声像漂移。
- 预设合理保留前级余量，不以“增益和的一半”作为安全保证；极端增强仍由最终保护兜底。

### 预设与 UI

- 不提供系统内置预设；预设全部由用户通过「保存当前设置」创建，保存十段增益、前级、输出、ceiling 及 A2 字段。
- 夜间压缩、交叉馈送、混响等效果通过通用参数调整，不再单独造预设。
- 预设使用稳定 id；修改参数显示「自定义」。自定义预设可重命名/删除，内置预设已移除。
- reset 恢复平直参数并清空当前预设标识，但保留总开关状态；应用预设不擅自开启总开关。
- 新音效面板继承 Oran7SettingsPanel，沿用已有设置积木；十段区域在窄面板内横向滚动，不挤压标签与数值。
- 主题、字体、中英翻译、菜单焦点/ESC/关闭时序沿用现有规则；新增 QML 入 qml.qrc，翻译只走显式维护目标。

## 6. 接口与完整配置模型

Client.audioEffects 是唯一入口。首版只暴露总开关、前级/输出增益、固定十行 EQ 模型、ceiling、当前预设及应用/保存/删除/重置命令。

控制器在 prepare_async 前绑定该播放器邮箱；运行后只发布参数，不通过 GUI setter 重新 prepare DSP。AutoEQ 接口延后，届时返回具名结果/错误状态，不用 QString* 作为 QML 输出参数。

A1 配置结构示意：

```json
{
  "audio": {
    "version": 1,
    "enabled": false,
    "preset_id": "custom",
    "settings": {
      "preamp_db": 0.0,
      "band_gains_db": [0,0,0,0,0,0,0,0,0,0],
      "output_gain_db": 0.0,
      "limiter_ceiling_db": -1.0
    },
    "custom_presets": []
  }
}
```

- 顶层 schemaVersion 暂保持 2（新增可选组），audio.version 明确音效结构版本。
- A1 固定频率/Q/类型属于 v1 算法定义，集中定义一次，不是可调却遗漏保存的字段。所有可调参数默认值只来自 default.json，调用点不重复默认值。
- 自定义预设保存 `{id, name, version, settings}`，settings 与当前设置使用同一完整结构。总开关属于用户全局选择，不随预设保存/切换。
- 后续每个新增可调项都要同时补 DTO、配置、预设、UI 与往返测试。参量 EQ 改变 bands 结构时提升 audio.version 并迁移旧十段增益，不能静默丢值。
- 旧配置缺组/缺键按现有 normalize 补齐；保留未知字段。未来音效版本不能强转 v1 再覆盖，应停用不支持的音效并保留原数据。
- AudioEffects 只产出值对象，Client 组合配置，AppConfigManager 原子提交。拖动只更新内存和邮箱，正常退出保存；自定义预设保存/删除走明确的即时配置提交入口，失败反馈 UI。
- 不另建配置文件，不重载配置后再保存，不覆盖其他分组的最新值。

## 7. 后续效果的约束

| 能力 | 实施前补齐 |
|---|---|
| 高低通、低音架 | Nyquist 边界、关断状态、扫频过渡、完整保存字段 |
| 压缩器 | 阈值/比率/attack/release/knee/makeup；自动补偿不等于响度匹配 |
| 宽度 | 先验证 M/S；另加去相关延迟需独立做单声道兼容/相关性测试 |
| Crossfeed | stereo 才生效；其他布局保留意图但标注不可用，明确归一化与切换 |
| Room/FDN | 反馈稳定性、预分配、size 过渡、尾音与 seek/EOF 语义 |
| HRTF | 具体数据版本/许可、IR 延迟、重采样/插值和干湿对齐，另行评估 |

不沿用未测量的“现代 CPU 每块 0.15ms”承诺。记录机器、采样率、声道、块长与最坏参数，按处理耗时分位数相对音频截止时间评估；整体 CPU 仅作辅助指标。

## 8. 测试、验收与回退

| 阶段 | 必须覆盖 |
|---|---|
| A0 | 1.0/变速、三档音量、F32/S16 适配、缓冲失败/零样本、全长静音初始化、打开失败与关闭顺序 |
| A0 | 参数并发、满队列补交、最后更新可达、消费有界、快速切歌/退役；seek/连续 seek/EOF 清理旧 serial |
| A0 | 新 DSP 无分配/锁/日志，覆盖使用的 C 分配器而非只统计 operator new；既有管线开销单列 |
| A1 | 旁路与 Flat 语义、正弦/扫频/冲激、任意块长、低采样率边界及受支持声道布局 |
| A1 | 连续拖动/预设/开关过渡、NaN/Inf/非法参数、静音状态衰减与 denormal 性能 |
| A1 | 音效开启且过渡稳定时，多频带/高增益/输出增益/变速/音量组合后的最终峰值；关闭/淡化按旁路约定，不只测一个正弦或有无 NaN |
| A1 | 当前设置/自定义预设全字段往返、缺键/未来版本保护、保存失败、中英文和设置面板时序 |

新增检查放 tests/audio-dsp，DSP 数学测试与真实回调适配/生命周期检查分开，不用模型替身替代接线验证。稳定性测试采用等效音频时长，遵守 checks 超时。

每批跑受影响的 playback/state/resolver/config 及新增检查，最终完整 checks、只编译 Release。实际听感、设备兼容、音画同步、蓝牙和真实 CPU 对比由用户验收，不代跑主程序或音频回环采集。

关闭总开关只回退新增音效，不会撤销 A0 的格式/缓冲改造。A0 独立成批：设备或变速回归时能恢复上一稳定实现，不能把关闭音效称为完整管线回滚。

## 9. 参考、依赖与文件落点

首批不引入新的 DSP 库或 HRTF 数据。自研有限范围的 EQ/增益/样本峰值保护，不按“约 200 行”估计整个交付成本。

| 来源 | 本次核对与使用边界 |
|---|---|
| [W3C EQ Cookbook](https://www.w3.org/TR/audio-eq-cookbook/) | RBJ 公式参考，参数边界与实现仍需测试 |
| [C++ 数据竞争规则](https://eel.is/c++draft/intro.races) | 普通结构体并发读写不能靠事后序号复核合法化 |
| [SDL2 OpenAudioDevice](https://wiki.libsdl.org/SDL2/SDL_OpenAudioDevice) | 区分回调格式、允许变更项与设备内部转换 |
| [DSPFilters Biquad.h](https://github.com/vinniefalco/DSPFilters/blob/master/shared/DSPFilters/include/DspFilters/Biquad.h) | 文件头有 MIT 授权，不能由根目录无 LICENSE 推断无授权；若复用按具体 commit/文件保留声明 |
| [OpenAL Soft README](https://github.com/kcat/openal-soft/blob/master/README.md) | 代码 LGPL-2-or-later，默认 HRTF 数据 Apache-2.0；当前不打包，后续选择数据时固定版本并核对归属/NOTICE |
| AutoEQ、Steam Audio、libbs2b、Freeverb 等 | 本期不引入；不沿用原稿整表“已核实”，需要时逐个核对所选版本、文件和数据 |
| 现有 FFmpeg/Sonic | 沿用现有依赖约束，已链接不等于任意滤镜/源码/数据都可直接使用 |

不在 SDL 回调中新增 afilter 图。比较其他方案时考虑实时边界、参数更新、维护和授权，不以“十个滤镜必须重建整个图”作统一断言。

未来实施的主要位置：

```text
src/cpp/Oran7MediaPlayer/Audio/    参数值、逐播放器邮箱、DSP、独立缓冲
src/cpp/Oran7MediaClient/         AudioEffects、EqualizerModel、Controller/Client 组合
src/cpp/Oran7MediaPlayer/         设备、回调、Sonic 边界、serial/时钟接线
src/cpp/Oran7Config/              AudioConfig 映射/版本/校验（仅 Core）
src/qml/Settings/SettingWindows/Oran7AudioSettingWindow.qml
src/qml/Components/Oran7GlobalComponents/Oran7EqualizerBand.qml
tests/audio-dsp/                 数学、邮箱、适配器、生命周期检查
config/default.json、qml.qrc、translations/、CMakeLists.txt、设置容器
```

项目自有类统一 Oran7 前缀，音效服务/处理器仅 EXE 编译，配置值仅 Core 编译；保持现有 URI、Client 门面和退役协议。实施后补 `docs/modules/Audio - 音效链.md` 及相应 ADR。

关联：[[Player - 播放内核]]、[[ASYNC - 异步与线程]]、[[RUNTIME_STATE - 运行时状态]]、[[Config - 配置与持久化]]、[[UI - QML界面]]、[[BUILD - 构建与验证]]。

## 10. A0 预准备（2026-11-05）

> 本轮只做基线复核、接口/测试契约与实施顺序，不改 `src/`、不改 `main.cpp`、不启动主程序。

### 10.1 基线复核结果

| 方案假设（2026-10-27） | 2026-11-05 实际 | 结论 |
|---|---|---|
| `audio_close()` 关闭后再解锁设备 | 已改为 `Oran7SdlAudioDevice`（内部互斥、`closeLocked()` 只 `SDL_CloseAudioDevice`，不再 `LockAudioDevice`）；`tests/playback/sdl-audio.cpp` 用隔离子进程覆盖旧死锁 | **A0-a 的设备顺序已部分完成**；仍需复核回调停止与 DSP 释放先后 |
| `audio_buf` 可能借用 AVFrame | `audio_resample_frame`：重采样时 `audio_buf = audio_buf1`（`:1439`）；未重采样时 `audio_buf = af->frame->data[0]`（`:1445`），随后 `frame_queue_next`（`:1453`） | **必须修**：借用的帧数据在回调消费前可能被复用；A0 统一复制到稳定 scratch |
| Sonic 输出与输入别名 | 回调里 `av_fast_malloc(&audio_buf1, ...)` 作为输出（`:1568`），而 `audio_buf` 可能正指向 `audio_buf1`（`:1439`） | **必须修**：输入/输出缓冲彻底分离 |
| 输出格式 S16 | `audio_open` 请求 `AUDIO_S16SYS`（`:1643`），`audio_hw_params->fmt = AV_SAMPLE_FMT_S16`（`:1662`） | A0-b 待做 |
| 音量在 SDL 混音 | `SDL_MixAudioFormat(..., AUDIO_S16SYS, ...)`（`:1609`） | A0-b 移到 float 域并做斜坡 |
| 无参数通道 | 全仓库无 `AudioParameter` / `AudioProcessor` / `audio_dsp` | A0 新增 |
| seek 有清理 | `read_thread` 中 `packet_queue_flush(&audioq)`（`:405`） | 缺音频侧 serial/DSP 清理 |
| 测试可复用 | `tests/playback/sdl-audio.cpp`（设备开关/死锁）、`native-io.cpp`（真实 FFmpeg 读取/退役）已存在 | 新增 `tests/audio-dsp`，并扩展 playback 的音频适配检查 |

### 10.2 A0 必须先修的缺陷

1. **借用帧生命周期**：无重采样时 `audio_buf` 指向解码帧，`frame_queue_next` 之后才消费，属于隐式悬空；A0 必须 `av_frame_ref` 或复制到 scratch。
2. **Sonic 输入/输出别名**：`audio_buf1` 同时是重采样输出与 Sonic 输出，扩容时可能移动/释放正在读的输入。
3. **静音路径尺寸计算**：`audio_buf_size = frame_size * SDL_AUDIO_MIN_BUFFER_SIZE / frame_size`（`:1507`）等价于固定字节数，切到 F32 后必须按协商格式重算。
4. **格式假设**：`SDL_MixAudioFormat(AUDIO_S16SYS)`、`audio_tgt.channels * 2` 语义、`bytes_per_sec` 计算都要随 `audio_tgt.fmt` 走。
5. **无 serial 清理**：连续 seek 时旧 PCM/Sonic/DSP 历史没有身份，不能只用 bool reset。
6. **无声明的实时边界**：现有回调已有 swr/Sonic 分配与日志；A0 不改这部分，但要把新增 DSP 的零分配边界单独测出来。

### 10.3 预准备接口（先定契约）

A0 先落三个纯 C++ 组件，不依赖 Qt：

```cpp
// Oran7AudioParameters.h —— 固定大小、可平凡复制；不含 Qt/字符串/容器
struct Oran7AudioParameters {
    static constexpr int kBandCount = 10;
    static constexpr int kVersion = 1;
    int   version = kVersion;
    std::uint32_t enabled = 0;          // 0/1；跨线程只用标准定宽类型
    float preampDb = 0.0f;
    float outputGainDb = 0.0f;
    float limiterCeilingDb = -1.0f;
    float bandGainsDb[kBandCount] = {};
};

// Oran7AudioParameterQueue.h —— 单生产者(GUI)/单消费者(音频线程)
class Oran7AudioParameterQueue {
public:
    static constexpr int kCapacity = 8;
    bool push(const Oran7AudioParameters& value);      // GUI；满返回 false
    bool popLatest(Oran7AudioParameters* out);         // 音频；取最新完整快照
    bool empty() const;
private:
    // 原子 head/tail + 固定槽位；槽位只在 acquire/release 下发布/回收
};

// Oran7AudioProcessor.h —— 每播放器独占；A0 只做旁路/增益/限幅骨架
class Oran7AudioProcessor {
public:
    bool prepare(int sampleRate, int channels, int maxFrames); // 回调停用期间
    // 输入只读，输出可写；允许 in==out 由实现拷贝保证
    void process(const float* in, float* out, int frames);
    void reset();                                             // 音频线程
    void setParameters(const Oran7AudioParameters&);
    bool enabled() const;
    int latencyFrames() const { return 0; }                   // A0 无新增延迟
};
```

接线（A0 只打通，不改变听感）：

- `Oran7MediaPlayer::oran7mp_setAudioParameterQueue(std::shared_ptr<Oran7AudioParameterQueue>)`。
- `Oran7FFplayerCore` 在 `audio_open()` 成功后 `processor->prepare(audio_tgt.freq, channels, maxFrames)`；回调里先处理、再 Sonic、再音量。
- `Oran7PlaybackController::prepare()` 在 `oran7mp_prepare_async()` **之前**注入队列（`:115-124`）。
- 队列为空/未注入时处理器整体旁路，行为与当前版本一致。

### 10.4 实施顺序（每步可独立提交）

| 步骤 | 内容 | 验证 |
|---|---|---|
| A0-1 | `Oran7AudioParameters` + `Oran7AudioParameterQueue` + 纯 C++ 测试 | `tests/audio-dsp`：满队列、合并、最后值可达、并发读写无撕裂 |
| A0-2 | 稳定 PCM 所有权：借用帧复制到 scratch；Sonic 输入/输出分离；静音尺寸按格式计算 | `native-io` 回归；新增回调级检查：任意块长、零样本、失败路径静音 |
| A0-3 | F32 协商 + S16 回退；float 音量斜坡；格式适配边界 | 扩展 `sdl-audio`：F32 dummy 设备、F32 失败回退 S16、不同采样率 |
| A0-4 | 注入参数队列 + 旁路处理器 + serial reset（seek/切歌） | `tests/audio-dsp` 并发；连续 seek/切歌不残留旧 PCM |
| A0-5 | 文档回填、全量 checks、听感验收 | `ctest --preset checks`；用户试听 |

### 10.5 测试落点与命令

```text
tests/audio-dsp/                纯 DSP/队列：无 Qt、无 SDL、无设备
tests/playback/sdl-audio.cpp    F32/S16 打开、关闭顺序（扩展）
tests/playback/native-audio.cpp 新：真实 core + dummy SDL，回调级适配/生命周期
```

```bat
cmake --preset checks
cmake --build --preset checks
ctest --preset checks
```

只编译 Release；不启动主程序、不采真实音频、不做打包验收。

### 10.6 待确认

| # | 问题 | 建议 |
|---|---|---|
| 1 | F32 打开失败是否回退 S16？ | 回退一次并记录；不影响关闭顺序 |
| 2 | 借用帧是否一律复制？ | 是，统一 scratch；复制成本可测且低于悬空风险 |
| 3 | 参数队列容量/补交 | 容量 8；GUI 满时保留最新值，16ms 定时器重试 |
| 4 | A0 是否含音量斜坡？ | 含（A0-b），否则 A1 开 EQ 时仍有 zipper |
| 5 | 关总开关的语义 | 旁路新增音效，但保留 A0 的格式/缓冲/音量改造 |

### 10.7 A0-1 / A0-2 实施记录（2026-11-05）

**A0-1 参数契约与队列**

- 新增 `src/cpp/Oran7MediaPlayer/Audio/Oran7AudioParameters.h`：固定大小值对象（`version/revision/enabled/preampDb/outputGainDb/limiterCeilingDb/bandGainsDb[10]`），`static_assert` 可平凡复制、标准布局；跨线程只用定宽类型与 float。
- 新增 `src/cpp/Oran7MediaPlayer/Audio/Oran7AudioParameterQueue.h`：SPSC、容量 8（2 的幂）、读写索引分缓存行；满时拒绝而不覆盖；`popLatest()` 一次取走当前可见槽位并返回最新完整快照；断言 32 位原子无锁。
- 新增 `tests/audio-dsp/parameter-queue.cpp` 与 `tests/audio-dsp/CMakeLists.txt`，覆盖空/满/最新值/回绕/20 万次并发发布；`tests/CMakeLists.txt` 注册 `audio-dsp` 套件。
- 结果：`audio_parameter_queue_checks` 通过（1/1），并发检查中 revision 单调、快照字段一致。

**A0-2 PCM 所有权与 Sonic 缓冲**

- `Oran7FFplayerCore::audio_resample_frame()` 无重采样路径不再借用 `af->frame->data[0]`，复制到稳定 `audio_buf1` 后再 `frame_queue_next`。
- Sonic 输出改用独立 `audio_sonic_buf`，不再复用可能正被输入引用的 `audio_buf1`。
- 静音长度改为 `audio_tgt.frame_size * SDL_AUDIO_MIN_BUFFER_SIZE`，清掉 `frame_size * MIN / frame_size` 的无效计算。
- `stream_component_close()` 释放 `audio_sonic_buf`，避免切歌/关闭泄漏。
- 结果：`oran7.playback` 3/3（playback/sdl-audio/native-io）通过；本轮未改输出格式与音量语义，正常播放听感应与改造前一致。

**尚未做**

- A0-3：F32/S16 协商、float 音量斜坡、格式适配边界。
- 因此当前版本仍是 S16 输出、SDL 混音音量；处理器已接入但尚未进入采样路径。

### 10.8 A0-4 实施记录（2026-11-05）

- 新增 `src/cpp/Oran7MediaPlayer/Audio/Oran7AudioProcessor.h/.cpp`：每播放器独占的处理器骨架，`prepare/process/reset/setParameters` 契约固定；A0 只做旁路，零分配、零锁、零日志，`latencyFrames()==0`。
- `Oran7FFplayerCore`：
  - 增加 `setAudioParameterQueue()`、`audio_processor_`、`audio_parameter_queue_`；
  - `audio_open()` 协商完成后创建并 `prepare(rate, channels, maxFrames)`；`audio_close()` 在设备关闭后释放处理器；
  - 回调每块从队列取一次最新参数，并按 `audio_discontinuity_` 代际清理旧 PCM、Sonic 积压与处理器历史；
  - `read_thread` 在 seek flush 后递增代际。
- `Oran7MediaPlayer` 增加 `oran7mp_setAudioParameterQueue()` 门面转发。
- `Oran7PlaybackController`：每个新播放器创建独立参数队列并在 `prepare_async` 前注入；新增 `publishAudioParameters()`/`audioParameterQueue()`。旧播放器退役后不再接收新参数。
- 新增 `tests/audio-dsp/processor.cpp`：prepare 校验、旁路位精确、同址、启停快照、空指针/零帧、单声道、零分配。
- 验证：
  - `audio_parameter_queue_checks`、`audio_processor_checks` 通过（2/2）；
  - `oran7.playback` 3/3 通过；
  - Release `Oran7MediaClient` 目标编译通过。

**A0-4 当时行为**：默认参数 `enabled=0`，且输出仍是 S16，处理器不会被调用；听感与改造前一致。`enabled=1` 在当时也不会改变声音——A0-3 完成后处理器才真正进入采样路径。

### 10.9 A0-3 实施记录（2026-11-05）

- `audio_open()` 优先请求 `AUDIO_F32SYS`，`audio_tgt.fmt = AV_SAMPLE_FMT_FLT`；打开失败或实际格式不符时关闭设备并重试 `AUDIO_S16SYS`。两条路径都由 `audio_tgt.fmt` 驱动，不再把 S16 缓冲当 float 使用。
- 回调顺序按方案 §4.1：重采样 → Sonic（变速）→ DSP（仅 F32 且 `enabled`）→ 音量斜坡 → SDL。DSP 输出使用独立 `audio_dsp_buf`，不与 `audio_buf1`/`audio_sonic_buf` 复用。
- 新增 `Oran7AudioVolumeRamp`（`Audio/Oran7AudioVolumeRamp.h/.cpp`）：采样级 20ms 线性斜坡，支持 float/S16、静音推进、S16 饱和；`applyFloat/applyS16` 零分配、零锁。
- 删除 `SDL_MixAudioFormat` 音量路径；`audio_volume` 百分比语义不变（0…SDL_MIX_MAXVOLUME → 0…1），打开设备时直接落在当前音量，避免淡入。
- `stream_component_close()` 释放 `audio_dsp_buf`。
- 测试：
  - 新增 `audio_volume_ramp_checks`（20ms 斜坡、S16 饱和、静音推进、非法输入）；
  - `sdl_audio_checks` 增加 F32 dummy 设备协商检查；
  - `native_io_checks` 通过真实 core + dummy SDL 验证 F32 路径；
  - `oran7.audio-dsp` 3/3、`oran7.playback` 3/3 通过；Release `Oran7MediaClient` 编译通过。
- 全量 CTest 仍为 41/42，唯一失败是既有的 `localization_checks`（工作区 QML 新文案未同步 TS），与音频链路无关。

**A0 收尾状态**：F32 优先、S16 回退、音量斜坡、参数队列、旁路处理器、seek 代际清理均已接好；`enabled=0` 时处理器旁路，听感应与改造前一致。下一步进入 A1：10 段 EQ + 前级/输出增益 + 最终限幅 + 预设/面板/配置。

### 10.10 A1 实施记录（2026-11-05）

**DSP**

- `Oran7AudioProcessor` 实现 10 段 RBJ peaking biquad（31Hz…16kHz，Q=1.41）、前级增益、输出增益、链接式样本峰值限幅；DF2T、double 系数/状态。
- 参数按 32 帧控制块平滑：频段增益约 20ms 逼近，前级/输出逐样本一阶平滑；限幅 instant attack + 50ms release，声像共用增益。
- `enabled=0` 或结构版本不支持时整体旁路；`channels > 8` 时旁路。
- 仍不新增算法延迟，`latencyFrames()==0`。

**状态与配置**

- 新增 `Oran7AudioEffects`（GUI 权威：开关/预设/前级/输出/ceiling/10 段模型）与 `Oran7EqualizerModel`（固定十行，roles: frequency/gainDb/label）。
- 新增 `Oran7AudioConfig` 与 `audio.*` 配置键：version/enabled/preset_id/preamp_db/output_gain_db/limiter_ceiling_db/band_gains_db/custom_presets。
- `Oran7ConfigBinding` 增加 audio 读取/写回；`Oran7MediaClient` 在 restore/save 边界加载/保存，并暴露 `Client.audioEffects`。
- 每个播放器独立队列；`Oran7PlaybackController` 在 `prepare_async` 前用 provider 注入当前完整参数，旧播放器退役后不再接收新值。
- 参数发布失败时 GUI 每 16ms 重试，保证最终值送达。

**UI**

- 音乐播放器设置窗口新增独立 `Oran7SettingItem` + `Oran7ExpandItem`「音效均衡器」组合：
  - 总开关、用户自定义预设（无内置预设）；
  - 10 段竖条（`Oran7EqualizerBand`），-12…+12dB；
  - 前级、输出、限幅上限；
  - 重置为平直。
- 新 QML 已加入 `qml.qrc`；文案使用 `qsTranslate("Oran7Ui", ...)`。

**测试**

- `audio_processor_checks`：平直位精确、1kHz +6dB 响应、限幅 ceiling、旁路、零分配。
- `audio_effects_checks`：预设/夹取/自定义标记/配置往返。
- `config_checks`：audio 分组往返与非法 bands 回退。
- `oran7.audio-dsp` 4/4、`oran7.playback` 3/3、`oran7.config` 1/1 通过；Release 编译通过。

**已知边界**

- S16 回退设备上 EQ/限幅不生效（仅保留音量/旁路）；正常 Windows 设备优先 F32。
- 自定义预设保存/重命名/删除已实现（A2 收尾补充）；AutoEQ 导入、频谱显示留到 A4。
- 全量 CTest 仍只有既有的 `localization_checks` 失败（工作区 QML 新文案未同步 TS），与音频链路无关。

### 10.11 A2 实施记录（2026-11-05）

**DSP**

- 新增高通（20–200Hz）、低通（8k–20kHz）二阶 Butterworth（Q=0.707）；默认 20Hz/20kHz 视为关闭。
- 新增低音架（low-shelf，默认 120Hz，0–12dB）。
- 新增立体声宽度（M/S，0…2；仅 stereo 生效）与交叉馈送（700Hz 低通 + 0.3ms 延迟，三档强度）。
- 新增前馈链接式压缩器：threshold/ratio/attack/release/knee/makeup，软膝、声像共用增益；无 lookahead。
- 处理顺序：前级 → 高通 → 低通 → 10 段 EQ → 低音架 → 宽度/交叉馈送 → 压缩 → 输出增益 → 限幅。
- 频段/滤波频率/低音/宽度/交叉馈送按 20ms 平滑；压缩器 attack/release 系数在参数变化时重算。
- `Oran7AudioParameters` 版本提升到 2；结构版本不匹配时旁路。

**配置与 UI**

- 新增 `audio.high_pass_hz/low_pass_hz/bass_boost_db/bass_freq_hz/stereo_width/crossfeed/compressor_*` 键，`Oran7AudioConfig.version=2`；旧 v1 配置按默认值补齐。
- 音乐播放器设置「音效均衡器」组新增：低音增强、高通、低通、立体声宽度、交叉馈送、压缩器开关/阈值/比率/补偿。
- 重置为平直同时复位滤波/宽度/交叉馈送/压缩器；预设仍只控制 EQ/前级/输出/ceiling，不覆盖 A2 字段。

**测试**

- `audio_processor_checks` 新增：高通 50Hz 衰减、低通 12kHz 衰减、60Hz 低音架提升、宽度 0 抵消纯侧信号、压缩器降低大声输入。
- `audio_effects_checks` 新增：A2 字段往返、配置 version 2、reset 复位。
- `config_checks` 覆盖 audio 分组（含新字段默认值）。
- `oran7.audio-dsp` 4/4、`oran7.config` 1/1 通过；Release 编译通过。

**已知边界**

- 交叉馈送/宽度只在 stereo 生效；其他声道布局保留参数但不处理。
- 压缩器无 lookahead，峰值可能轻微过冲，由最终限幅兜底。
- 压缩器 attack/release/knee 暂未在 UI 暴露（配置保留字段，默认 10/150/3）。
- S16 回退设备仍不运行 DSP。

### 10.12 自定义预设管理（2026-11-05）

- `Oran7AudioEffects::presets()` 只返回用户自定义项：`{id, label, custom}`；四个系统预设已完全移除。
- 新增 `saveCurrentAsPreset(name)`、`suggestedPresetName()`、`renamePreset(id,name)`、`deletePreset(id)`；保存的是**完整设置快照**（EQ、前级/输出/ceiling、滤波、低音架、宽度/交叉馈送、压缩参数）。
- 自定义项存 `audio.custom_presets`，`presetsChanged` 触发 Client 立即写回配置（不再等退出）。
- 默认命名 `自定义1/2/3…`，自动跳过已用名称；id 使用 `custom-<UUID>`。
- 删除当前正在使用的自定义预设时，`presetId` 回退为 `custom`（当前参数保留）。
- EQ 窗口顶部操作行新增「保存」按钮（56px 宽），弹出输入框；预设下拉每项右侧新增 `⋯`，仅自定义项显示，点击打开重命名/删除弹窗。
- 「归零」「保存」两个按钮改为短标签 + 56px 宽，去掉左侧徽标占位。
- `audio_effects_checks` 覆盖保存、应用、重命名、删除、配置恢复；初始无预设。

### 10.13 均衡器交互优化（2026-11-05）

- 10 段 EQ 移至独立的 Oran7EqualizerWindow（760×428），从原设置组的“打开面板”进入；十段横向排布、清晰的频率/增益标签、预设、启用开关与仅频段归零。A2 及前级/输出/限幅设置继续使用原面板。
- 修正竖向 Slider 的 visualPosition 重复反转：向上增强、向下衰减，手柄和零点填充使用同一坐标方向。
- 窗口直接绑定现有 Client.audioEffects.equalizer，onMoved 即发布参数，不创建临时音效副本、不等关闭才生效。
- Client 对音效修改防抖 400ms 写回配置；关闭编辑器及退出时提交尚未保存的修改，启动加载阶段不触发自动保存。实时参数发布不受落盘延时影响。
- 窗口沿用取色器的焦点登记/ESC 关闭机制，淡出后释放计数并销毁，保存不需要额外“应用”按钮。
- equalizer_ui_checks 使用生产 QML 和音效模型、替身主题、隔离窗口验证十列布局、上下拖动、松开前实时发布、配置快照、预设同步、最终提交与关闭焦点释放；不打开实际音频设备或主程序。

**独立面板样式统一**

- 设置页入口改为“标签 + 整行圆角框”，新增可复用的 Oran7PreferenceAction，基于 Oran7StudioButton，并与语言/字体选择器保持徽标、边框、间距和悬停色一致。
- 均衡器内预设直接复用 Oran7PreferenceChoice，启用开关复用 Oran7SwitchToggleItem；归零和关闭使用自绘按钮。
- Oran7PreferenceChoice 增加可选 popupOwner：子菜单打开时保留宿主窗口，关闭后交还焦点登记；默认空值不改变语言/字体/取色菜单的行为。ESC 优先关闭子菜单，宿主关闭时也安全清理菜单与焦点计数。
- equalizer_ui_checks 额外覆盖真实菜单选预设、嵌套菜单 ESC 返回宿主、带打开菜单关闭窗口；实时参数和自动保存策略沿用上一版。
