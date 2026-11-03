#ifndef ORAN7WHIPPUBLISHER_H
#define ORAN7WHIPPUBLISHER_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <condition_variable>

// 前向声明 - 编码器
class Oran7VideoEncoder;
class Oran7AudioEncoder;

// 前向声明 - libdatachannel (if available)
#ifdef SPARK_MEDIA_PUBLISHER_WITH_LIBDATACHANNEL
namespace rtc {
class PeerConnection;
class Track;
} // namespace rtc
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
}

/**
 * @brief Oran7WhipPublisher - 将编码器产出的音视频包通过 WHIP/WebRTC 推到媒体服务器
 *
 * ============================================================================
 * 架构定位：替换 Oran7Mp4Muxer
 * ============================================================================
 *
 *   之前: Oran7VideoEncoder ──→ getPacket() ──→ Oran7Mp4Muxer ──→ MP4 文件
 *         Oran7AudioEncoder ──→ getPacket()
 *
 *   之后: Oran7VideoEncoder ──→ getPacket() ──→ Oran7WhipPublisher ──→ 媒体服务器
 *         Oran7AudioEncoder ──→ getPacket()               │
 *                                                   WHIP/WebRTC
 *
 * ============================================================================
 * 生命周期：
 * ============================================================================
 *
 *   1. 构造 Oran7WhipPublisher()
 *   2. open(config, videoEncoder, audioEncoder)
 *        ├── 创建 PeerConnection + 音视频 Track
 *        ├── 生成 SDP Offer
 *        ├── WHIP POST → 拿到 Answer
 *        ├── 交换 ICE 候选者
 *        └── 启动 publishLoop() 线程
 *   3. publishLoop() 线程运行中:
 *        ├── encoder->getPacket() 拉视频包 → BSF转换 → sendFrame()
 *        └── encoder->getPacket() 拉音频包 → sendFrame()
 *   4. close() → 停止循环 → DELETE WHIP → 清理资源
 *
 * ============================================================================
 * 和 spark-meeting RtcPublisher / 服务器 WHEP 发送端的对应关系：
 * ============================================================================
 *
 *   服务器 HandleWhepOffer：
 *     track->setMediaHandler(packetizer链)
 *     onFrame(data, info) { for (out) out->sendFrame(data, info); }
 *                    ↑                                ↑
 *   Oran7WhipPublisher:             完全一样的 Packetizer 链 + sendFrame()
 *                    ↑
 *   数据源不同: getPacket() 代替 onFrame()
 */
class Oran7WhipPublisher {
public:
    /** 错误/日志回调 */
    using ErrorCallback = std::function<void(const std::string&)>;

    /** ICE 候选者回调：当收集到本地 ICE 候选时触发（给 WHIP 层发送用） */
    using IceCandidateCallback =
        std::function<void(const std::string& candidate, const std::string& mid)>;

    /**
     * @brief 推流配置
     */
    struct Config {
        /** 媒体服务器地址，如 "http://192.168.80.132:8090" */
        std::string serverUrl;
        std::string bearerToken;

        /** 房间 ID / 流 ID */
        std::string roomId;

        /** 发布者 ID */
        std::string publisherId;

        /** STUN 服务器地址，如 "stun:192.168.80.132:3478" */
        std::string stunUrl;

        /** TURN 服务器（可选） */
        std::string turnUrl;
        std::string turnUser;
        std::string turnPass;

        /** 视频码率 (kbps) */
        int videoBitrateKbps = 1500;

        /** H264 payload type (默认 96) */
        int videoPayloadType = 103; // 匹配浏览器 WHEP Offer 中 H264 PT

        /** Opus payload type (默认 111) */
        int audioPayloadType = 111;

        /**
         * @brief 干跑模式：只从编码器拉包并打印统计，不做任何网络操作
         *
         * 用途：验证"编码器抽包"链路正常工作后，再开真正的推流。
         * 用法：先设 dryRun=true 跑通数据流，确认 getPacket 正常产出后再关掉。
         */
        bool dryRun = false;
    };

    Oran7WhipPublisher();
    ~Oran7WhipPublisher();

    // 禁止拷贝
    Oran7WhipPublisher(const Oran7WhipPublisher&) = delete;
    Oran7WhipPublisher& operator=(const Oran7WhipPublisher&) = delete;

    /**
     * @brief 打开推流会话
     *
     * 调用线程：录制任务线程；HTTP 使用该线程的局部 Qt 事件循环，禁止在 GUI 调用。
     * 阻塞直到 WHIP SDP 协商完成，然后启动后台推流线程。
     *
     * @param cfg        推流配置
     * @param videoEnc   视频编码器（已 start 且 isOpened）
     * @param audioEnc   音频编码器（已 start 且 isOpened，可为 nullptr）
     * @return true 成功，false 失败
     */
    bool open(const Config& cfg,
              std::shared_ptr<Oran7VideoEncoder> videoEnc,
              std::shared_ptr<Oran7AudioEncoder> audioEnc);

    /**
     * @brief 关闭推流会话
     *
     * 1. 停止 publishLoop 线程
     * 2. 关闭 PeerConnection
     * 3. DELETE WHIP 会话
     */
    void close();

    /** 推流循环是否在运行 */
    bool isRunning() const { return _running.load(); }

    /** 设置错误回调 */
    void setErrorCallback(ErrorCallback cb) { _onError = std::move(cb); }

    /** 设置 ICE 候选回调 */
    void setIceCallback(IceCandidateCallback cb) { _onIceCandidate = std::move(cb); }

    // ======================== 诊断接口 ========================
    int64_t videoFramesSent() const { return _videoSent.load(); }
    int64_t audioFramesSent() const { return _audioSent.load(); }
    int64_t videoBytesSent() const { return _videoBytes.load(); }
    int64_t audioBytesSent() const { return _audioBytes.load(); }
    int videoPktQueued() const;
    int audioPktQueued() const;

    /** WHIP 会话创建后，返回服务器分配的资源路径中的 stream_id */
    std::string streamId() const;
    std::string connectionState() const;
    std::string iceState() const;

private:
    // ======================== 推流主循环 ========================
    void publishLoop();

    // ======================== WebRTC 初始化 ========================
    bool initPeerConnection();
    bool createVideoTrack(std::string* out_mid);
    bool createAudioTrack(std::string* out_mid);
    bool generateOffer(std::string* out_sdp);

    // ======================== WHIP 信令 ========================
    bool whipPostOffer(const std::string& offerSdp, std::string* outAnswer);
    bool whipPatchIce(const std::string& mid, const std::string& candidate);
    bool whipPollCandidates(std::vector<std::string>* outCandidates);
    bool whipDelete();

    // ======================== H264 格式转换 ========================
    bool initH264Bsf();
    bool convertToAnnexB(AVPacket* pkt);

    // ======================== RTP 时间戳 ========================
    uint32_t videoRtpTimestamp(int64_t pts);
    uint32_t audioRtpTimestamp(int64_t pts);

    // ======================== 成员变量 ========================

    // 配置
    Config _cfg;
    struct Oran7Events {
        std::mutex mutex;
        std::condition_variable gathered;
        std::string connection = "new", ice = "new";
        bool gatheringDone = false;
        std::vector<std::pair<std::string, std::string>> candidates;
        std::atomic<bool> videoOpen{false}, audioOpen{false}, forceKeyframe{true};
        std::atomic<int64_t> lastPliUs{0};
    };
    std::shared_ptr<Oran7Events> _events = std::make_shared<Oran7Events>();
    bool _inited = false;

    // 编码器引用
    std::shared_ptr<Oran7VideoEncoder> _videoEnc;
    std::shared_ptr<Oran7AudioEncoder> _audioEnc;

    // 运行状态
    std::atomic<bool> _running{false};
    std::thread _loopThread;

    // 错误回调
    ErrorCallback _onError;

    // ICE 候选回调（内部桥接使用，dryRun 时无操作）
    IceCandidateCallback _onIceCandidate;
    std::string _whipResourcePath;
    std::string _whipEtag;

#ifdef SPARK_MEDIA_PUBLISHER_WITH_LIBDATACHANNEL
    // ---- libdatachannel 核心对象 ----
    std::shared_ptr<::rtc::PeerConnection> _pc;
    std::shared_ptr<::rtc::Track> _videoTrack;
    std::shared_ptr<::rtc::Track> _audioTrack;

    // SSRC
    uint32_t _videoSsrc = 0;
    uint32_t _audioSsrc = 0;
    std::string _cname;

    // RTP 时间戳基准
    std::atomic<int64_t> _videoTsBaseUs{-1};
    std::atomic<uint32_t> _videoTsBase90k{0};
    std::atomic<int64_t> _audioTsBaseUs{-1};
    std::atomic<uint32_t> _audioTsBase48k{0};
#endif

    // H264 extradata 缓存：手动提取 SPS/PPS 以确保关键帧完整
    AVBSFContext* _h264Bsf = nullptr;
    std::vector<uint8_t> _spsPpsAnnexB;  // Annex-B 格式的 SPS+PPS
    std::string _h264ProfileLevelId;     // 从编码器 extradata 提取的实际值

    // 统计计数器
    std::atomic<int64_t> _videoSent{0};
    std::atomic<int64_t> _audioSent{0};
    std::atomic<int64_t> _videoBytes{0};
    std::atomic<int64_t> _audioBytes{0};
};

#endif // ORAN7WHIPPUBLISHER_H
