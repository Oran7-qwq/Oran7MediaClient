# 内存增长排查与首轮修复

日期：2026-11-11。未启动或操控主程序；对用户已运行进程只读采样，验证使用隔离程序。

## 已观察到的现象

- 用户反馈音乐长期运行曾达到约 1.7 GB；同一首歌连续播放数分钟后稳定；视频切回音乐可能多出约 20 MB。用户背景为图片模式。
- 现场采样的私有内存依次约 445、510、525、533 MB，工作集约 299～352 MB。采样期间有自然切歌，不能据此把每次变化归因到某一个组件；本轮没有现场捕捉到 1.7 GB 峰值。
- 只读线程起点分类：272 个线程起自 NVIDIA 驱动，原生音频设备只有一个 SDLAudio 线程。线程总数多不能单独证明旧播放器没有释放。
- 按当前进程启动时间 17:21:19 过滤日志，音频播放器创建 11 次、旧音频线程退出/设备关闭 10 次，符合一个当前会话。此前 17:13 的采集失败重试属于更早实例，不能拿它解释当前进程。

## 确认并修复的持有点

| 位置 | 原行为 | 修复 |
|---|---|---|
| Oran7FFplayerCore / 硬解 | 成员持有的 AVBufferRef 未在会话结束时 unref | 解码器与帧队列结束后释放，重复建立前也归还旧引用 |
| Oran7FFplayerCore / Sonic | 修改速率时会释放前一个变速器，但播放器退出未释放最后一个 | 停止音频回调后销毁并清空 |
| Oran7FFmpegDecoderCore | EAGAIN 留存在成员中的 AVPacket 不属队列，销毁时遗漏 | 初始化整个包并在 decoder_destroy unref |
| Oran7FFplayerCore / 声道布局 | 每帧临时布局未完整释放，源目标存在浅拷贝隐患 | 临时布局 RAII，源目标深拷贝、分别 uninit |
| Oran7FFplayerCore / 线程对象 | 视频刷新线程 join 后未删除线程包装对象 | join 后 delete/null |
| SDL 子系统 | 每个会话 Init 累加引用，无对应归还，且初始化未使用的 SDL 视频子系统 | 仅 Audio/Timer；生命周期互斥、每实例配对退出 |
| D3D11VideoRenderNode | 停止只着黑色，仍保留上段视频的大尺寸纹理、CPU 转换帧及 swscale 缓冲 | 邮箱独立释放序号；在 afterFrameEnd 释放，而非破坏 prepare/render 多 pass 边界 |
| 视频画布 | 永久关闭的 BlurCard 和临时 debugCapture 仍引用渲染项 | 删除这些未启用的捕获入口 |

资源释放不等于 Windows 工作集立即下降，驱动/堆可能保留分配池。若整个主窗口已隐藏，没有 Qt 帧提交，则图像资源释放会等到下一次窗口帧；页面隐藏但主窗口继续绘制时，帧末仍能执行释放。暂停的有效视频帧不主动丢弃。

背景视频补充隐藏/最小化暂停门控，保留启动隐藏预热；用户当前是图片背景，此项不作为本次增长归因。

## 验证

新增 native_memory_checks：真实 Oran7MediaPlayer/FFmpeg + 合成 WAV + dummy SDL，32 次创建、1.25 倍速播放与销毁；不使用账号、真实声音、硬件 GPU 或主窗口。预热后比较私有提交与句柄，并逐次验证 SDL 引用归零。

本轮结果：

```
32 switch/retire cycles: baseline 13.31 MiB, peak 14.59 MiB, final 14.56 MiB, handles 221 -> 221
```

同时通过播放控制、慢网络取消、SDL 生命周期、WARP 普通/HDR/多 pass、视频空间和背景就绪检查。离线音频测试不包含 GUI、封面缓存或 NVIDIA 驱动，不能据此宣称 1.7 GB 峰值全部修复。

## 新版运行取证

2026-11-12 起，`[MemoryDiag]` 由下拉式 GUI → 主界面设置 → 通用基础设置 → 开发者模式实时控制，无需重启或重新构建，默认关闭。普通模式不采样、不输出。开发者模式下，Oran7PlaybackController 在 prepared、retired，以及有播放器时每 30 秒记录 privateMiB、workingMiB、handles、retiring、active。不输出媒体地址、曲名或账号；不调用 EmptyWorkingSet、不强行 GC。

重新启动新版后，对比同曲播放、连续切歌、视频→音乐三种场景的 prepared/retired 与 periodic 数值。如果私有内存在每次 retired 后仍持续增长，需要继续对 GUI 封面/文字/驱动缓存做对照取证；如果退休计数上升不回落，则沿退役任务继续查。GPU 显存和进程私有提交是不同指标，需要分别记录，不能混为 1.7 GB 的同一来源。

只读内存区域/线程分类脚本：`build/refactor-work/memory/audit.py <PID>`。脚本不读内存内容、不暂停线程、不注入进程、不输出凭据。
