---
status: ✅ A0/A1/A2 完成，A3 待实施
updated: 2026-11-05
---

# Audio：音效链

> 源码 `src/cpp/Oran7MediaPlayer/Audio/`（纯 C++，无 Qt）＋ `Oran7FFplayerCore` 的音频回调接线。
> 方案与阶段定义见 [[AUDIO - 音效均衡器方案]]；本文只记录当前代码结构与线程契约。

## 文件职责

| 文件 | 职责 | 线程 |
|---|---|---|
| `Oran7AudioParameters.h` | 固定大小参数快照（version/revision/enabled/前级/输出/ceiling/10 段增益） | 值对象 |
| `Oran7AudioParameterQueue.h` | SPSC 固定容量（8）参数队列；满拒绝、`popLatest()` 取最新完整快照 | GUI 写 / 音频线程读 |
| `Oran7AudioProcessor.h/.cpp` | 每播放器独占的 DSP：10 段 EQ、高低通、低音架、宽度/交叉馈送、压缩、前级/输出、最终限幅 | 音频线程 |
| `Oran7AudioVolumeRamp.h/.cpp` | 采样级 20ms 音量斜坡；支持 float/S16、静音推进、S16 饱和 | 音频线程 |
| `Oran7AudioEffects.h/.cpp`（`Oran7MediaClient/`） | GUI 权威：开关/预设/前级/输出/ceiling/发布重试 | GUI |
| `Oran7EqualizerModel.h/.cpp`（`Oran7MediaClient/`） | 固定十行均衡器模型 | GUI |

目录边界：`Audio/` 不依赖 Qt、不依赖 `Oran7MediaClient`；`Oran7FFplayerCore` 只通过头文件使用这些组件，便于 `tests/audio-dsp` 纯 C++ 编译。

## 接线

```
Oran7PlaybackController（GUI）
  ├─ 每个新播放器创建 Oran7AudioParameterQueue
  ├─ prepare_async 前 oran7mp_setAudioParameterQueue()
  └─ publishAudioParameters() 写当前播放器队列

Oran7FFplayerCore（音频线程）
  ├─ audio_open：协商 F32/S16；创建 Processor 并 prepare(rate, channels, frames)
  ├─ 回调：popLatest → setParameters
  ├─ seek：audio_discontinuity_ 代际变化 → reset + 丢弃旧 PCM + flush Sonic
  ├─ 重采样 → Sonic → Processor（仅 F32 且 enabled）→ VolumeRamp
  └─ audio_close：设备关闭后释放 Processor
```

## 当前行为（A2）

- 优先 `AUDIO_F32SYS` / `AV_SAMPLE_FMT_FLT`；失败回退 `AUDIO_S16SYS`。
- `enabled=0` 处理器旁路；开启后执行高通/低通、10 段 EQ、低音架、立体声宽度/交叉馈送、压缩、前级/输出与最终限幅。
- 频段/滤波/低音/宽度/交叉馈送按 20ms 平滑；压缩器 attack/release 由参数计算，无 lookahead。
- 宽度/交叉馈送只在 stereo 生效；其他布局保留参数但不处理。
- 音量由 `Oran7AudioVolumeRamp` 在 float/S16 域应用；`audio_volume` 百分比语义不变。
- seek 用代际号清理旧 serial 的 PCM、Sonic 积压和 DSP 历史。
- 播放器退役后旧队列不再接收新参数；新播放器通过 provider 带入当前完整参数。
- UI 位于音乐播放器设置窗口的「音效均衡器」Expand 组；EQ 编辑窗口只提供用户自定义预设（原四个系统预设已移除），可保存、重命名、删除，参数在退出时写回 `audio` 配置分组。
- 自定义预设保存完整设置快照（含 A2 字段），存 `audio.custom_presets`；保存/重命名/删除立即写回配置。
- S16 回退设备上 DSP 不生效（仅保留音量/旁路）；正常 Windows 设备优先 F32。

## 测试

| 检查 | 覆盖 |
|---|---|
| `tests/audio-dsp/parameter-queue.cpp` | 空/满/最新值/回绕/20 万次并发 |
| `tests/audio-dsp/processor.cpp` | 旁路位精确、平直、1kHz +6dB、高低通、低音架、宽度、压缩、限幅、零分配 |
| `tests/audio-dsp/volume-ramp.cpp` | 20ms 斜坡、S16 饱和、静音推进、非法输入 |
| `tests/audio-dsp/effects.cpp` | 预设/夹取/自定义标记/保存/重命名/删除/配置往返/A2 字段/reset |
| `tests/config/main.cpp` | audio 分组往返、非法 bands 回退 |
| `tests/playback/sdl-audio.cpp` | 设备开关/死锁、F32 协商 |
| `tests/playback/native-io.cpp` | 真实 core + dummy SDL 读取/退役 |

## A3 入口

A3 在现有 `process()` 中增加 Room：早期反射与 FDN 混响，独立验证性能、反馈稳定性和尾音；参数 DTO/配置/预设/UI 同批扩展。

