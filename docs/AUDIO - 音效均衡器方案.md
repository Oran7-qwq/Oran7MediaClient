---
status: 📝 评审后修订，待实施
updated: 2026-10-27
---

# 音效与均衡器实施方案（AUDIO）

> 首批只交付：默认关闭、可实时调节、可保存预设的 **10 段 EQ + 前级 + 输出保护**。
> 当前只修订方案，不修改代码、不引入依赖、不启动主程序。A0 验收通过再做 A1，其余效果后续逐项选择。

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
- 现有 `audio_close()` 有关闭设备后再解锁设备的顺序。A0-a 必须审计并纠正，不能直接插入处理器销毁。

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

- 首批只做 Flat、人声、低音、明亮四类纯 EQ 预设；每个定义十段增益、前级、输出增益和 ceiling，曲线在 A1 听感验收中确认。
- 夜间压缩、交叉馈送、混响预设等对应效果完成后再加入，不展示无效功能。
- 预设使用稳定 id；修改参数显示 Custom。自定义预设明确重名/覆盖交互，内置预设不可删除。
- reset 恢复 Flat 参数但保留总开关状态；应用预设不擅自开启总开关。
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
    "preset_id": "flat",
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
