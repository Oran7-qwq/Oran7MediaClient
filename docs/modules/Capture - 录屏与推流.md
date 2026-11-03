---
status: ✅ P3-2/3 代码/离线完成，真实设备待验收
updated: 2026-10-27
---

# Capture：录屏与推流

源码 `src/cpp/Oran7ScreenCapture/`，编译进 EXE。画面沿用 ddagrab/D3D11 预览与 Media Foundation 编码，系统声音由 WASAPI 回环采集；输出为 MP4 或可选 WHIP/WebRTC。这里只实现客户端，不启动陈旧 TCP/Server。

## 所有权与线程边界

```
Client → unique_ptr<Oran7ScreenCaptureBridge>
       → unique_ptr<Oran7ScreenCaptureController>（GUI）
          ├─ 预览 Oran7DesktopCaptureWorker / QThread
          │    └─ unique_ptr<Oran7CaptureFrameConverter>（采集线程独占转换资源）
          ├─ unique_ptr<Oran7RecordingSession>（GUI 状态与请求）
          │    └─ 独立 QThread：Oran7RecordingBackend::run
          │         └─ RAII：视频/音频编码器、音频采集、桥接线程、MP4 或 WHIP
          └─ 可选 unique_ptr<Oran7SignalingClient>（GUI 的异步 WebSocket）
```

Session 不新增单例，无 QObject parent；控制器仍通过 Client.screenCapture() 借给 QML。任务只捕获配置值和共享 Oran7RecordingFeed，不访问 Client、QML 或 QObject 采集 worker。Feed 以 mutex 保护尺寸、硬件设备引用和编码入口；AVBufferRef 复制引用用 shared_ptr 自定义删除器释放。每次录制绑定递增 epoch，快速重启也重置视频时间戳。

原音频采集类从巨型抓屏文件拆为 `Oran7WasapiAudioWorker.h/.cpp`，配置名同步为 Oran7WasapiConfig；职责仍是系统默认输出设备的 loopback → S16/48k/stereo。析构停止并 join，初始化失败也复位运行状态。

## P3-3：采集与转换职责（2026-10-26）

Oran7ScreenCapture.h/.cpp 只保留控制器与原有公开接口。Oran7DesktopCaptureWorker 接收 outputIndex/drawMouse/fps 值与 frameSubmitter，不持界面对象；GUI 构造空壳后 moveToThread，start 才创建图与设备，finished → deleteLater 在采集线程销毁。P3-2 后控制器不持 worker 句柄，只共享取消/诊断控制值和 Feed；worker 仍由所属线程销毁。普通 stop 不阻塞，退出 shutdown 才同步等待。

| 资源/职责 | 所有者 | 释放/交接 |
|---|---|---|
| FFmpeg graph / hwdev | worker 的 unique_ptr + 自定义删除器 | 先销毁转换器，再 graph，再 hwdev；src/sink 借用图 |
| 抓取、错误、首帧、录制节流/时戳 | worker，采集线程 | 信号带现有 worker 代际，GUI 只消费当前代际 |
| 预览三槽/VideoProcessor、录制 NV12/读回、swscale | worker 独占的转换器 | 转换器析构释放 ComPtr/unique_ptr；不持 QML/编码器 |
| 预览 AVFrame 及克隆 | 安全帧回调与 QRhi 邮箱 | AVBufferRef 租约延长槽位寿命，旧尺寸/转换器销毁不破坏在途帧 |
| 录制原始帧及编码入口 | worker / 共享 RecordingFeed | 预览池满或 GPU 忙仅丢预览；录制仍用原始帧 |

BGRA 复制显式选择源 array slice；新尺寸槽位全部创建成功后才提交。保持原 SDR 色彩、GPU NV12 和 CPU 下载/swscale 回退行为，没有新增单例或第二条帧消费路径。MinGW WASAPI GUID 定义在音频实现文件，不再依赖控制器头的 initguid。

capture-conversion 使用 WARP 验证实际预览转换/租约/跨设备忙锁；CPU 回退只替换硬件下载边界，其余转换仍为生产代码。NV12/P010/RGBA VideoProcessor、真实 FFmpeg 硬件下载与吞吐由用户验证。Release 构建及完整 31 项离线检查通过，记录位于 build/refactor-work/P3-3/。

## P3-2：预览生命周期（2026-10-27）

唯一权威为控制器的 Oran7PreviewState：Idle → Starting → Running → Stopping → Idle，异常重建经 Recovering，最终失败进入 Error。previewState/previewBusy/previewError 提供只读投影，QML 不复制运行状态；RecordingSession 继续独立管理录制状态。

- start/stop/restartPreview 返回表示提交，stopped 在旧线程完全结束后发出；停止期间再次 start 只保留启动意图，旧会话完全释放前不创建新 worker。
- GUI 将取消写入共享 Oran7CaptureControl 并关闭 Feed、quit；不调用 worker QObject。finished 后用 wait(0) 零超时探测延迟删除/TLS 是否结束，未结束由 10ms 定时器稍后重试。唯一阻塞 wait 在 shutdown，Bridge 退出显式走 shutdown，保证渲染/设备依赖仍在。
- 参数目标、在途快照、已保存配置和最近可用目标分开存储。图 started 不完成请求，首帧后才提交配置；无渲染项时只保存，纯日志热更新不重启。
- 连续 apply 只保留最新请求，返回 requestId；captureRequestFinished(result) 用 requestId/ok/msg/superseded 标识终态。旧 captureApplyFinished 只转发最新异步请求，工作台直接按编号收取结果并保护新草稿。
- 首帧默认等待 5 秒；自动恢复最多 8 次、间隔 300ms，只在实际产帧后重置预算。参数失败/超时恢复最近可用目标（没有已验证目标时尝试启动配置基线），回滚失败进入 Error。GDI 输出身份及稀疏原始索引保持。
- 新参数失败前不落盘；成功只改最新快照的 capture 分组。预览忙时禁止开始录制及修改输出；录制准备/运行/停止期间仍禁止破坏采集资源的参数操作。
- 同步 monitors 枚举接口保持兼容，未宣称所有驱动调用均已异步。驱动若阻塞无法强制中断，界面保持 Stopping 与原因，只有它返回后才回收/重建。

Release 构建、完整 31 项离线检查及最终定向复验通过。恢复夹具 116 项断言覆盖慢调用/析构、停止后立即重启、旧错误晚到、请求覆盖、超时、保存失败/回滚失败、渲染项销毁及退出；QML 检查覆盖请求编号、迟到兼容通知、新草稿保留及后端忙状态。实际多显示器切换与持续录制仍由用户验收。

## 录制状态与接口

Oran7RecordingState：Idle → Preparing → Recording → Stopping → Idle；准备/运行失败在资源清理后进入 Error，可再次开始。

- startRecording() 的 bool 表示请求是否接受，不能代表文件已打开；最终看 recordingState/recordingStarted/errorOccurred。
- stopRecording() 只提交取消。准备中、录制中、停止中均 recordingBusy=true，拒绝重复开始及设置修改；停止完才解除忙状态。
- 首帧等待、编码器打开、WHIP HTTP/ICE 等待、音频桥接 join、编码 flush 和 MP4 收尾都在录制任务线程。准备/停止不阻塞 GUI。
- 首帧最多等待 5 秒，每个编码器打开最多轮询 3 秒；WHIP HTTP 单次有超时，ICE 连接等待 10 秒。取消为协作式，正在执行的 HTTP/驱动操作需返回后清理，界面保持“停止中”。不强杀线程。
- 旧代际和重复回调丢弃；共享 mailbox 防止析构后访问 QObject。Session.shutdown() 仅在析构/退出同步等待，保证依赖的渲染与设备资源仍存活。

控制器新增只读属性 recordingState、recordingBusy、recordingError、recordingStatistics、recordingOutputPath、recordingOptions；recordingStateChanged/recordingSettingsChanged 通知界面。recordingState 是英文枚举名，QML 显示中文。

## 参数、配置与界面

`Oran7RecordingConfig` 位于 Oran7Config，仍使用 schemaVersion=2 的增量默认值深合并，存储在 `recording` 段。旧配置无需手工修改，默认仍为 MP4。配置键集中在 Oran7ConfigKeys。

| 参数 | 语义 |
|---|---|
| mode | 0=MP4，1=WHIP；setRecordMode 保留兼容入口 |
| output_directory / file_prefix | 空目录使用系统视频目录，指定目录必须为绝对路径；文件名自动追加时间与随机后缀 |
| width / height | 同时 0 为原始尺寸；显式分辨率至少 16、偶数，不超过 16384 |
| fps / bitrate_kbps | 录制帧率上限 1～240，码率 100～200000 kbps；不会补造超过采集帧率的画面 |
| audio_enabled | 可关闭系统音频；MP4 使用 libfdk_aac（不可用时回退原生 aac），WHIP 使用 Opus |
| whip_url / bearer_token | 完整 WHIP POST 接口地址与凭据；默认空，不再使用测试服务器和固定 token |
| stun_url / turn_url / turn_user / turn_password | 可选 ICE 服务 |
| signaling_url / signaling_user / signaling_password / room_id | 可选的既有房间发现信令，留空不启用 |

recordingSettings() 返回聚合草稿；applyRecordingSettings(map) 严格校验数值/类型/路径/URL 后一次提交并保存，失败返回 ok/msg。录制期间拒绝修改。P2-3 已将认证字段和连接 URL 分离到 DPAPI 密文；内存聚合草稿与运行协议不变，见 [[CREDENTIALS - 凭据加密与迁移]]。

ScreenCaptureStack 挂载 `Oran7RecordingWorkspace.qml`，主页面集中提供本地 MP4 录制设置。固定采用左侧画面来源、中间预览、右侧录制输出的三栏布局，缩窄页面也不换行。左右工具栏宽度固定为 230/260，预览随剩余空间伸缩且最小宽度为 280；整行最小宽度 790，空间不足时横向滚动。中栏由上方预览和下方独立 Oran7RecordingToolbar 组成，工具栏与预览同宽，集中系统声音开关、启停按钮、中文状态和输出路径；左右设置栏继续占满各自整列。工具栏内部在中栏较窄时分行，不改变外部三栏关系。全局下拉设置中的旧录屏面板及其独立草稿已移除。

两侧设置采用分组卡片和统一 34px 输入控件：左侧为显示器、采集设置和默认折叠的高级诊断；右侧为画质设置、文件保存。采集帧率提供 30/60/120/自定义，录制帧率提供 30/60/自定义；输出尺寸提供原始尺寸（后端仍为 0×0）、1080p、720p 和自定义。自定义入口只展开输入，不提前改写草稿；已保存的非标准参数自动显示。码率以 Mbps 编辑，最多三位小数，提交时换算为原有整数 kbps。空采集帧率在应用前拒绝，避免被后端钳制为 1 FPS。

显示器列表追加 Windows 友好名称（如“显示器 1：X71Q”）：QueryDisplayConfig / DisplayConfigGetDeviceInfo 查询活动显示路径，以 GDI 设备名关联 DXGI 输出，不按枚举顺序匹配。兼容镜像模式下同源多个显示器名称；读取不到时保留序号。下拉列表加宽，选中框可悬停查看完整名称；outputIndex 仍保持采集源的原始序号。本机只读查询已取得 X71Q，名称显示及缺省回退检查通过。

码率下拉提供 4/8/12/20/40/60 Mbps 六档及自定义，每项以主标题显示码率和档位、副标题说明画质侧重与文件体积。已有默认 40 Mbps 保持不变，非标准值自动显示自定义，选择自定义只展开输入。面板按目标视频码率估算每小时体积（十进制 GB，Mbps × 0.45；低于 1 GB 使用 MB），不含音频，实际大小可能浮动。说明明确区分磁盘占用与编码负载，避免把码率直接等同于 CPU/GPU 占用；档位不承诺固定分辨率下的无损画质。加宽的弹出列表限制窗口边距，防止右侧与底部越界。

固定目标码率下，每小时体积估算不随帧率变化；提高帧率会减少平均每帧可分配的数据。帧率设置要求采集 FPS ≥ 录制 FPS：两侧不合法的快捷档位禁用，自定义输入的上下限联动。草稿不合法时拒绝应用、保存和开始；提高时先应用采集，再保存录制，降低时先保存录制，再应用采集，避免独立提交产生临时不一致。C++ 控制器在应用采集、保存录制与开始录制三个入口再次校验实际/已保存值，不自动改写用户历史配置。

目录输入框内嵌文件夹按钮，非编辑时中间省略，悬停查看完整路径；获得焦点后编辑完整值。面板标题显示待保存/应用中/已保存状态；左侧底部保留固定的「重连预览」，右侧移除保存按钮并释放底部空间，设置区独立滚动。共享内联控件 Oran7StudioPanel/Oran7SettingsGroup/Oran7Input/Oran7Choice/Oran7QuickChoices/Oran7Action 统一外观；选择框在模型初始化后延迟绑定草稿索引，避免实际源和显示名称不一致。

`Oran7RecordingPageState.qml` 只持有采集与输出草稿及提交调度；录制状态始终读控制器。显示器选择映射真实 outputIndex，未连接输出不可选择。2026-10-25 改为自动保存，移除「应用画面设置」「保存录制设置」按钮：下拉/快捷档位/开关修改后合并 180ms 内的连续操作，文本与自定义数值在 editingFinished 后提交，输入过程中不重启采集或写入半成品。采集变更通过原 applyCaptureSettings 异步应用，失败读取回滚值；录制参数失败保留编辑值并提示，再次编辑可重试。同步日志变更无需等待异步通知。待提交输出在采集异步应用结束后继续保存。升帧率先提交采集、降帧率先保存录制，继续接受后端校验。开始录制前冲刷未完成变更；需要重建采集时等成功完成再继续开始，失败不开始。Preparing/Recording/Stopping 及采集参数应用期间锁定设置，不因自动保存重建正在录制的链路。页面初始化/回读参数不触发自动保存。

自动保存后的离屏检查通过 80 项布局/运行与 24 项原编排流程检查，包含真实 Timer 的连续修改合并、文本结束编辑、保存失败重试、异步应用后续提交、录制前等待与失败取消、双侧帧率自动提交顺序；无 QML 警告。Release 编译通过，未启动主程序。

外部点击失焦补充：工作台顶部透明 Item 用 PointHandler 被动监听鼠标按下，点击输入框外部时明确清除该输入框 focus，再将焦点交给工作台，触发原 editingFinished 自动保存。仅 forceActiveFocus 到外层不能保证清除内部焦点。点击输入框内部继续编辑，按钮和滚动区域保留原有交互。所有工作台 Oran7Input（含帧率、码率、尺寸、目录、前缀）复用此行为；校验仍沿用原输入与后端规则。离屏实际鼠标检查覆盖输入框内点击、预览点击失焦保存及快捷按钮继续响应，累计 83 项布局/运行与 24 项编排流程通过，Release 编译通过。

输入提交再次完善：用户反馈失焦仍可能停留在待保存，按用户要求为 Oran7Input 增加 onTextChanged 空闲计时，编辑中每次变化重置 2000ms Timer，停止输入后提交到原自动保存队列。新增 commitRequested 信号，统一接收空闲超时、editingFinished 和已获得过焦点后的 activeFocus=false；失焦提交不再仅依赖 TextInput 的 editingFinished 判定，回车/失焦时取消输入空闲计时。初始化无焦点时不启动计时，后端继续校验并拒绝无效值。重复提交由草稿差异判断避免重复写入。检查改用真实 Ctrl+A 与数字键输入，覆盖失焦后实际配置及顶部已保存状态、截止前不保存、继续输入重置计时、保持焦点空闲保存、回车立即提交，累计 88 项布局/运行与 24 项编排流程通过，Release 构建通过，未启动主程序。

工作台样式由组件自身的 panelColor、baseColor、textColor、accentColor、hoverColor、groupColor、inputColor、borderColor、mutedTextColor、controlHeight/controlPixelSize、预览/错误颜色、字体和开关尺寸等 property 提供。底部工具栏和复用开关显式接收样式，不绑定 Oran7MainUiSetting 的下拉 GUI 样式。修改下拉设置配色、字体或行高不会改变工作台；调用方可以单独覆盖工作台样式。

2026-10-25 工作台其余控件统一：新增资源 Oran7StudioButton.qml，统一主操作/次操作/停止按钮的圆角、悬停、按下、选中、键盘焦点及禁用状态。用于底部开始/停止、预览画面信息/适应、还原设置和重连预览；颜色从工作台传入。底部系统声音独立成卡片，标明开启状态及「不含麦克风」，状态区使用状态点及较小字号，输出路径支持省略与悬停查看。录制/准备/收尾期间优先显示实时状态，避免旧保存提示覆盖录制状态。窄中栏保留内部换行，不改变左右工具栏位置。真实 QML 离屏累计 91 项布局/运行与 24 项编排流程通过，无 QML 警告；验证新按钮真实鼠标切换、忙状态锁定、样式隔离及输入自动保存。

当前音频源仅为默认播放设备：Oran7WasapiAudioWorker 选择 eRender/eConsole，并启用 AUDCLNT_STREAMFLAGS_LOOPBACK。没有独立 eCapture 麦克风源、输入设备选择或系统声/麦克风混音链路。按用户要求工作台先提供麦克风开关与音量 UI，明确标注尚未接入；需要实际录制麦克风时仍须实现输入采集、重采样/时间戳对齐、混音和独立设置，不能只添加 GUI 开关。

底部音频工具扩展：系统声音、麦克风各使用 192px 固定宽度模块，提供开关、0～100% 音量滑块与百分比，中栏较窄时音频工具横向滚动；开始/停止操作另占固定区域。系统声音开关继续使用原真实 audioEnabled 配置；systemVolume、microphoneEnabled、microphoneVolume 仅为工具栏 UI 草稿，不写入配置，不改变后端音频，界面分别标明音量待接入/麦克风录音待接入。录制期间锁定这些工具。

修正录制开始时预览挤压：工具栏在同一宽度档位下采用固定高度，开始/停止按钮固定宽度，状态提示单行省略且预留行高，输出路径始终保留 16px 行高。避免路径从空变为有值、状态换行、按钮文案变化触发预览高度变化。中栏最小高度随预览 200px + 工具栏固定占位计算，小窗口保留滚动。离屏累计 108 项布局/运行与 24 项原编排流程通过，验证宽/窄中栏 Preparing/Recording/Stopping/Error/Idle 各阶段预览与工具栏几何不变、路径出现、真实音频 UI 操作、窄栏滚动可达、未接入控件不写后端及忙状态锁定。Release 编译通过，未启动主程序，真实录屏效果由用户验收。

本地页面明确使用 mode=0；从旧 WHIP 模式进入时只创建 MP4 草稿，保存或开始时才切换模式。合并配置时从后端重新取得完整参数，保留 WHIP/ICE/信令字段。按用户要求，WHIP 功能本轮冻结，原 Oran7RecordingSettingsPopup 保留源码/资源但不再由录屏页打开；不扩展推流界面或连接真实服务。

预览仍沿用原采集链路，不因翻页或编辑输出草稿重建。渲染挂载失败时保留首次启动状态并重试，挂载成功后才自动启动预览；Fit/Fill 在录屏页初始化并可直接切换，无需打开全局设置。

## 资源收尾与失败路径

### 录制转换与编码路径（2026-10-25 修正）

录制优先在采集设备的独立 VideoProcessor 上完成色彩转换与缩放，再回读 NV12，减少 BGRA 整帧下载和 CPU sws_scale 的串行开销。输出明确使用 BT.709、有限范围，关闭驱动自动后处理；读取时按 RowPitch 分别复制 Y/UV 平面。设备或格式不支持时保留原 CPU 下载/转换兜底，每轮录制可重试 GPU 路径；仍有 CPU 回读和编码输入复制，不是端到端零拷贝。

本地 MP4 的 MF 显式请求 hw_encoding=1、H.264 High Profile、camera_record 场景，硬件打开失败再尝试软件 MF；WHIP 保持原来的 live_streaming 场景、默认 profile 和 MF 选择方式。持有 D3D11 的 AVBufferRef 不会自动让 FFmpeg MF 选择硬编。帧提供明确 duration，MP4 从实际编码上下文复制 profile、色彩和其他流参数。send_frame 返回 EAGAIN 时取走输出后重送原帧；停止后排空已接受的有界输入队列，保留 flush 数据包。实时队列溢出仍丢旧帧并记录数量，不能保证任意硬件和负载都达到设定上限。

用户样本的实际平均帧率为 56.6/61.9 FPS，视频码率约 12.8/15.9 Mbps，均为 2560×1440、Constrained Baseline。修正后的本机合成对照中，1440p GPU NV12 转换约 2.3～3.4ms/帧，CPU 兜底约 13.7～24ms/帧（随负载波动）。真实设备上的色条检查覆盖 1440p/1080p、Y/UV、范围与缩放；中央动态区域测试完整生成 240 帧/4秒及 480 帧/4秒，实际视频码率约 45.3/64.5 Mbps（目标 40/60，短片有启动波动）。全屏随机噪声压力测试仍发生过队列溢出，不能据此宣称真实游戏场景稳定 120 FPS。

检查编译生产转换函数体及实际编码器/封装器，使用合成纹理，不启动主程序、不采集桌面/声音、不修改屏幕设置。原录制编排/失败收尾 52 项、真实软件编码/AAC/MP4 回读及异步失败 155 项回归通过。实际游戏画质和长时间吞吐由用户复测。

Windows 显示分辨率或桌面模式变化时，FFmpeg ddagrab 可能返回 AVERROR_OUTPUT_CHANGED / AVERROR_INPUT_CHANGED / AVERROR_EXTERNAL。worker 将这类运行期错误作为采集失效通知，控制器保留预览启动意图，退役旧 worker 并在 300ms 后重建整个采集设备和共享纹理池，最多重试 8 次。初始化成功不重置预算，实际产出首帧后才重置；主动停止会取消定时重试。普通首次初始化失败仍直接报错，不无限重启。

退役先失效 worker 代际；旧 stopped、错误与共享句柄通知不能操作新会话。旧线程退出后清理 Qt 侧纹理引用，避免新尺寸暂时套用旧纹理。若失效时正在录制，正常停止录制并提示、收尾已有文件，再恢复预览；不会把原文件的编码会话静默接到新 Feed。重建耗尽则停止并报告错误。采集设备由 ComPtr 持有，不额外 AddRef。

每次创建 worker 前重新枚举抓屏设备的 DXGI 输出，以运行期 GDI deviceName 跟踪原输出并更新其原始 output_idx；原输出消失时回退到首个已连接输出，并保存参数、通知页面刷新。暂时没有可用输出时保留身份，按上述有限预算等待桌面稳定。序号校验按实际 outputIndex 成员和 attached 状态，不能按列表长度推断合法性。显示器编号 DISPLAY2 不等于 DXGI output_idx=1。

已挂载预览失败停止后，应用设置会重新启动采集；左侧底部另提供「重连预览」，无参数变化也可恢复，录制或应用参数期间禁止重连。成功/错误反馈显示在工作台。自动源重映射会同步已保存参数，保留用户未应用草稿。应用恢复耗尽也会发送失败完成通知，避免页面一直处于应用中。

2026-10-25 控制器恢复检查累计 49 项：直接编译生产控制器部分及真实录制 Session，硬件枚举/worker/渲染/配置/编码任务使用替身，覆盖临时初始化失败、连续失效、旧结束通知、有限重试、停止取消、耗尽后手动启动、录制收尾、序号重映射、稀疏序号、已断开/空输出拒绝、停止后应用恢复及录制重连保护。未实际修改 Windows 分辨率或采集桌面；真实 DXGI/D3D11 切换效果由用户验收。

录制资源按同一路径关闭：关 Feed 编码入口 → 停音频桥接并 join → 停音频采集 → flush/停止音视频编码器 → MP4 写尾或 WHIP 关闭会话。任何准备失败均回收已创建资源，不能发 recordingStarted。

MP4 使用 QTemporaryFile 在目标目录预留唯一文件名，编码输出打开后再提交保留；准备失败自动删除本任务尚未提交的文件，不覆盖已有录像。容器检查打开/写入/写尾结果。编码线程即便已因错误把 running 置 false，stop 仍必须 join，避免 std::thread 析构 terminate；停止后拒收迟到帧。

WHIP 保留 libdatachannel：POST/PATCH/DELETE 从同一配置取 Bearer，Location 解析为同源资源地址；不把 token 转发到跨域资源。ICE/轨道/PLI/gathering 回调只持有共享事件值，gathering 不再捕获栈变量；候选者入队，由发布线程串行发送 PATCH。连接达到 connected 后才报告录制就绪。失败 open 同样执行完整 close，删除固定 C:/temp H264 dump。

可选信令仍使用原消息协议，但用户名、密码、地址来自配置；停止只 flush/close，不在 GUI 嵌套事件循环等待。信令是辅助房间发现功能，WHIP 协议本身不依赖它。

## 保留边界与验收

P1-6 的异步化原本只覆盖录制/推流；P3-2 已补齐预览停止、参数重启/恢复的异步线程交接。显示器枚举/校验仍保留同步接口；退出仍同步等待，不提前释放设备。

独立检查覆盖状态/取消/重入、替身设备下的实际 backend 编排与 RAII、真实软件编码+MP4 回读、离屏真实 QML 面板及本机 HTTP 凭据传递。2026-10-25 工作台累计 53 项布局/控件/样式隔离/真实页面挂载检查与 20 项草稿/保存/回滚/启停流程检查，覆盖固定三栏、窄屏横向滚动、底部固定按钮、自定义/快捷选项、Mbps 换算、目录完整值、初始显示器索引同步、忙状态锁定、保留 WHIP 参数及渲染挂载重试。宽屏、窄屏及自定义展开截图已复查，无 QML 警告。测试使用替身服务和占位预览，未启动主程序、未采集桌面/系统声音、未连接真实 WHIP/信令服务。实际 h264_mf、WASAPI、D3D11 与 WebRTC 连通性由用户运行验收。

验收建议：未开预览点击开始、准备中取消、重复开始/停止、连续录制两次、中文输出路径、音频开关、输出尺寸/帧率、无效目录与服务、推流状态、退出时文件完整。后续 P1-7 QML 业务下沉与 P1-9 启动窗口仍独立安排。

2026-10-26 底部工具栏压缩：固定 86px，两行布局，上行以 32px 内联音频工具与开始/停止按钮组成，下行合并状态和保存路径。窄中栏仅音频区横向滚动，不再把操作按钮换到下一行；状态变化不改变预览占位。音量/麦克风仍为原 UI 草稿，未新增后端录音功能。工作台交互/布局检查及资源导入检查通过，Release 构建通过，未启动主程序。

## P2-7 预览同步补充（2026-10-26）

预览三槽改为 keyed mutex + AVFrame 槽位租约，worker 只持创建时取得的共享邮箱回调；原 GUI 打开共享池/worker 调用 Item 的路径已删除。忙槽零超时跳过预览，录制仍处理原始帧；同步异常沿用有限恢复，池尺寸变化后旧纹理由最后一个租约释放。QRhi 节点负责在 Qt 设备打开共享纹理并同步复制，详见 [[Render - D3D11视频渲染]]。WARP 离线像素检查不代替真实显示器和持续录制验收。

## 工作台样式首版：专业录制台与轻量浮层（2026-10-27）

Oran7RecordingPreview 独立承接预览外框：固定 58px 来源栏展示已提交的显示器与采集上限，录制时用红色状态标记和细边框提示；画面信息/适应按钮位于画布右下角的半透明浮层，支持悬停和键盘焦点。窄中栏折叠非必要状态文字，左右栏及整体占位不变。未启动/启动/恢复时的遮罩只展示后端状态，不销毁实际 Loader，不改 QRhi 帧消费链。

底部仍为固定 86px、操作行与状态行；音频模块保持 220px 并横向滚动，加入 Oran7RecordingIcon 统一线条图标、细边框、轻微渐变和滑块/按钮过渡。系统音频开关仍提交原配置；麦克风及音量仍为既有 UI 草稿，没有虚构电平、实测 FPS 或录制计时。全部颜色由自身 property/工作台传入，不绑定下拉设置样式。

离屏工作台检查覆盖宽/窄屏、状态标记/浮层边界、工具栏固定几何、录制状态切换与真实按钮交互；国际化和资源导入检查通过。宽屏/窄屏/录制状态截图由占位预览夹具生成，不是真实桌面采集，材料位于 build/refactor-work/recording-studio-style/。
