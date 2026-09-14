/**
 * Oran7WhipPublisher.cpp — WHIP/WebRTC 推流器实现
 *
 * ============================================================================
 * 阅读指南：
 * ============================================================================
 *
 * 1. publishLoop()       — 最小数据循环：从编码器抽 AVPacket，发送
 * 2. open()              — 编排整个启动流程（SDP → WHIP → 启动线程）
 * 3. createVideoTrack()  — 和服务器 WHEP 发送端完全一样的 Packetizer 链
 * 4. whipPostOffer()     — WHIP HTTP 信令，阻塞 SDP 交换
 * 5. convertToAnnexB()   — H264 格式兼容处理
 *
 * ============================================================================
 * 快速验证（dryRun 模式，不需要网络和 libdatachannel）：
 * ============================================================================
 *
 *   Oran7WhipPublisher pub;
 *   Oran7WhipPublisher::Config cfg;
 *   cfg.dryRun = true;   // 关键：只拉包不打网
 *   pub.open(cfg, videoEncoder, audioEncoder);
 *   // 观察日志：每 3 秒输出 video_pkts / audio_pkts
 *   // 确认 getPacket() 能正常拉到数据后，再关掉 dryRun
 *
 * ============================================================================
 * 真正的推流（需要 libdatachannel + 媒体服务器运行中）：
 * ============================================================================
 *
 *   Oran7WhipPublisher pub;
 *   Oran7WhipPublisher::Config cfg;
 *   cfg.serverUrl = "http://192.168.80.132:8090";
 *   cfg.roomId    = "demo";
 *   cfg.publisherId = "pub1";
 *   pub.open(cfg, videoEncoder, audioEncoder);
 *   ...
 *   pub.close();
 */

#include "Oran7WhipPublisher.h"
#include "Oran7VideoEncoder.h"
#include "Oran7AudioEncoder.h"
#include "GlobalHelper.h"

#include <chrono>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>

// Qt Network — WHIP HTTP
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QThread>

// libdatachannel (optional)
#ifdef SPARK_MEDIA_PUBLISHER_WITH_LIBDATACHANNEL
#include <rtc/rtc.hpp>
#include <rtc/h264rtppacketizer.hpp>
#include <rtc/plihandler.hpp>
#include <rtc/rtcpnackresponder.hpp>
#include <rtc/rtcpsrreporter.hpp>
#include <rtc/rtppacketizer.hpp>
#endif

// ============================================================================
// 最小 HTTP 客户端（阻塞调用，依赖 Qt 事件循环）
// ============================================================================

namespace {

struct HttpResponse {
    int status = 0;
    std::string body;
    std::string resourcePath;   // Location 响应头
    std::string etag;           // ETag 响应头
};

static constexpr int kHttpTimeoutMs = 5000;
static const char kBearerToken[] = "123456";

static void setAuthHeaders(QNetworkRequest& req) {
    req.setRawHeader("Authorization", "Bearer 123456");
}

static HttpResponse httpPostSync(const std::string& url,
                                  const std::string& body,
                                  const std::string& contentType)
{
    HttpResponse r;
    QNetworkAccessManager mgr;
    QNetworkRequest req(QUrl(QString::fromStdString(url)));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QString::fromStdString(contentType));
    setAuthHeaders(req);
    req.setTransferTimeout(kHttpTimeoutMs);

    QEventLoop loop;
    QNetworkReply* reply = mgr.post(req, QByteArray::fromStdString(body));
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(kHttpTimeoutMs + 1000, &loop, &QEventLoop::quit);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        WARNING_LOG << "[whip] POST timeout:" << url.c_str();
        reply->deleteLater();
        return r;
    }

    r.status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    r.body   = reply->readAll().toStdString();
    r.resourcePath = reply->rawHeader("Location").toStdString();
    r.etag  = reply->rawHeader("ETag").toStdString();
    reply->deleteLater();
    return r;
}

static HttpResponse httpPatchSync(const std::string& url,
                                   const std::string& body,
                                   const std::string& contentType,
                                   const std::string& ifMatch = {})
{
    HttpResponse r;
    QNetworkAccessManager mgr;
    QNetworkRequest req(QUrl(QString::fromStdString(url)));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QString::fromStdString(contentType));
    setAuthHeaders(req);
    if (!ifMatch.empty())
        req.setRawHeader("If-Match", ifMatch.c_str());
    req.setTransferTimeout(kHttpTimeoutMs);

    QEventLoop loop;
    QNetworkReply* reply = mgr.sendCustomRequest(
        req, "PATCH", QByteArray::fromStdString(body));
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(kHttpTimeoutMs + 1000, &loop, &QEventLoop::quit);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        reply->deleteLater();
        return r;
    }

    r.status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    r.body   = reply->readAll().toStdString();
    r.etag  = reply->rawHeader("ETag").toStdString();
    reply->deleteLater();
    return r;
}

static int httpDeleteSync(const std::string& url,
                           const std::string& ifMatch = {})
{
    QNetworkAccessManager mgr;
    QNetworkRequest req(QUrl(QString::fromStdString(url)));
    setAuthHeaders(req);
    if (!ifMatch.empty())
        req.setRawHeader("If-Match", ifMatch.c_str());
    req.setTransferTimeout(kHttpTimeoutMs);

    QEventLoop loop;
    QNetworkReply* reply = mgr.deleteResource(req);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(kHttpTimeoutMs + 1000, &loop, &QEventLoop::quit);
    loop.exec();

    int status = 0;
    if (reply->isFinished()) {
        status = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
    } else {
        reply->abort();
    }
    reply->deleteLater();
    return status;
}

#ifdef SPARK_MEDIA_PUBLISHER_WITH_LIBDATACHANNEL

static uint32_t randomSsrc() {
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<uint32_t> dist(
        0x10000000, 0xFFFFFFFF);
    return dist(gen);
}

// h264_mf 硬编默认值，如果 initH264Bsf 能从 extradata 提取实际值则会覆盖
// 从编码器 extradata/SPS 提取的实际值，初始化前为空。initH264Bsf 填充
static std::string g_h264ProfileLevelId;

static std::string injectExtmap(const std::string& sdp) {
    static const char* extensions =
        "a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\r\n"
        "a=extmap:2 "
        "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\n"
        "a=extmap:3 http://www.ietf.org/id/"
        "draft-holmer-rmcat-transport-wide-cc-extensions-01\r\n"
        "a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid\r\n";

    std::istringstream iss(sdp);
    std::string line, out;
    out.reserve(sdp.size() + 512);
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        out += line + "\r\n";
        if (line.rfind("m=audio", 0) == 0 || line.rfind("m=video", 0) == 0)
            out += extensions;
    }
    return out;
}

static std::string pcStateStr(::rtc::PeerConnection::State s) {
    switch (s) {
    case ::rtc::PeerConnection::State::New:          return "new";
    case ::rtc::PeerConnection::State::Connecting:   return "connecting";
    case ::rtc::PeerConnection::State::Connected:    return "connected";
    case ::rtc::PeerConnection::State::Disconnected: return "disconnected";
    case ::rtc::PeerConnection::State::Failed:       return "failed";
    case ::rtc::PeerConnection::State::Closed:       return "closed";
    default: return "unknown";
    }
}

static std::string iceStateStr(::rtc::PeerConnection::IceState s) {
    switch (s) {
    case ::rtc::PeerConnection::IceState::New:          return "new";
    case ::rtc::PeerConnection::IceState::Checking:     return "checking";
    case ::rtc::PeerConnection::IceState::Connected:    return "connected";
    case ::rtc::PeerConnection::IceState::Completed:    return "completed";
    case ::rtc::PeerConnection::IceState::Failed:       return "failed";
    case ::rtc::PeerConnection::IceState::Disconnected: return "disconnected";
    case ::rtc::PeerConnection::IceState::Closed:       return "closed";
    default: return "unknown";
    }
}

static std::string gatherStateStr(::rtc::PeerConnection::GatheringState s) {
    switch (s) {
    case ::rtc::PeerConnection::GatheringState::New:        return "new";
    case ::rtc::PeerConnection::GatheringState::InProgress: return "gathering";
    case ::rtc::PeerConnection::GatheringState::Complete:   return "complete";
    default: return "unknown";
    }
}

#endif // SPARK_MEDIA_PUBLISHER_WITH_LIBDATACHANNEL

} // anonymous namespace

// ============================================================================
// Oran7WhipPublisher 实现
// ============================================================================

Oran7WhipPublisher::Oran7WhipPublisher() = default;

Oran7WhipPublisher::~Oran7WhipPublisher() {
    close();
}

// ============================================================================
// open() — 启动推流
// ============================================================================

bool Oran7WhipPublisher::open(const Config& cfg,
                               std::shared_ptr<Oran7VideoEncoder> videoEnc,
                               std::shared_ptr<Oran7AudioEncoder> audioEnc)
{
    if (_running.exchange(true)) {
        if (_onError) _onError("WhipPublisher: already running");
        return false;
    }

    _cfg      = cfg;
    _videoEnc = videoEnc;
    _audioEnc = audioEnc;

    if (!_videoEnc || !_videoEnc->isOpened()) {
        _running.store(false);
        if (_onError) _onError("WhipPublisher: video encoder not ready");
        return false;
    }

    // ================================================================
    // dryRun 模式：只验证编码器→抽包链路，不碰网络
    // ================================================================
    if (_cfg.dryRun) {
        INFO_LOG << "WhipPublisher: DRY-RUN mode — packet diagnostics only";
        _loopThread = std::thread(&Oran7WhipPublisher::publishLoop, this);
        _inited = true;
        return true;
    }

    // ================================================================
    // 真正的推流模式（需要 libdatachannel）
    // ================================================================
#ifdef SPARK_MEDIA_PUBLISHER_WITH_LIBDATACHANNEL

    // 1. H264 BSF: AVCC → Annex-B
    if (!initH264Bsf()) {
        _running.store(false);
        if (_onError) _onError("WhipPublisher: BSF init failed");
        return false;
    }

    // 2. PeerConnection
    if (!initPeerConnection()) {
        _running.store(false);
        if (_onError) _onError("WhipPublisher: PeerConnection init failed");
        return false;
    }

    // 3. SDP Offer
    std::string offerSdp;
    if (!generateOffer(&offerSdp)) {
        _running.store(false);
        if (_onError) _onError("WhipPublisher: SDP offer failed");
        return false;
    }
    INFO_LOG << "[whip] offer SDP size:" << (int)offerSdp.size() << "bytes";

    // 4. WHIP POST → Answer（同时获取 resourcePath 和 ETag）
    std::string answerSdp;
    if (!whipPostOffer(offerSdp, &answerSdp)) {
        _running.store(false);
        if (_onError) _onError("WhipPublisher: WHIP POST failed");
        return false;
    }

    // 5. setRemoteDescription(answer)
    try {
        _pc->setRemoteDescription(::rtc::Description(answerSdp, "answer"));
        INFO_LOG << "[whip] setRemoteDescription(answer) ok";
    } catch (const std::exception& e) {
        _running.store(false);
        if (_onError) {
            _onError(std::string("[whip] setRemoteDescription: ") + e.what());
        }
        return false;
    }

    INFO_LOG << "[whip] session active — starting publish loop";
    _inited = true;
    _loopThread = std::thread(&Oran7WhipPublisher::publishLoop, this);
    return true;

#else
    _running.store(false);
    if (_onError) _onError(
        "WhipPublisher: libdatachannel not available. "
        "Use cfg.dryRun=true for packet diagnostics, "
        "or build with SPARK_MEDIA_PUBLISHER_WITH_LIBDATACHANNEL.");
    return false;
#endif
}

// ============================================================================
// close()
// ============================================================================

void Oran7WhipPublisher::close() {
    if (!_running.exchange(false))
        return;

    if (_loopThread.joinable())
        _loopThread.join();

#ifdef SPARK_MEDIA_PUBLISHER_WITH_LIBDATACHANNEL
    if (_pc) {
        try { _pc->close(); } catch (...) {}
        _pc.reset();
    }
    _videoTrack.reset();
    _audioTrack.reset();
    whipDelete();
#endif

    if (_h264Bsf) {
        av_bsf_free(&_h264Bsf);
        _h264Bsf = nullptr;
    }

    _videoEnc.reset();
    _audioEnc.reset();
    _inited = false;

    INFO_LOG << "WhipPublisher: closed";
}

// ============================================================================
// publishLoop() — 核心数据循环
// ============================================================================

// 检查 Annex-B 数据是否包含指定 NAL 类型
static bool annexbHasNalType(const uint8_t* buf, size_t n, uint8_t nalType) {
    if (n < 5) return false;
    for (size_t i = 0; i + 4 < n; ++i) {
        size_t sc = 0;
        if (buf[i]==0x00 && buf[i+1]==0x00 && buf[i+2]==0x00 && buf[i+3]==0x01) sc = 4;
        else if (buf[i]==0x00 && buf[i+1]==0x00 && buf[i+2]==0x01) sc = 3;
        else continue;
        size_t h = i + sc;
        if (h >= n) continue;
        if ((buf[h] & 0x1F) == nalType) return true;
    }
    return false;
}

void Oran7WhipPublisher::publishLoop() {
    int64_t vidCount = 0, audCount = 0;
    int64_t vidBytes = 0, audBytes = 0;
    auto lastLog = std::chrono::steady_clock::now();

    INFO_LOG << "WhipPublisher: publishLoop started";

    while (_running.load()) {
        AVPacket pkt{};
        bool gotAny = false;

        // ---- 拉视频包 ----
        if (_videoEnc && _videoEnc->isRuning()) {
            if (_videoEnc->getPacket(&pkt, 2)) {
                vidCount++;
                vidBytes += pkt.size;

                if (!_cfg.dryRun) {
#ifdef SPARK_MEDIA_PUBLISHER_WITH_LIBDATACHANNEL
                    // BSF 转换（AVCC→Annex-B + SPS/PPS 注入）
                    if (!convertToAnnexB(&pkt)) {
                        av_packet_unref(&pkt);
                        continue;
                    }

                    // PLI 响应
                    bool pliTriggered = _forceKeyframe.exchange(false);
                    if (pliTriggered)
                        _videoEnc->requestKeyframe();
                    static int64_t lastIdrPts = 0;
                    if (pkt.flags & AV_PKT_FLAG_KEY) {
                        lastIdrPts = pkt.pts;
                        // PLI 后验证：关键帧应含 SPS/PPS/IDR
                        static int keyCheckCount = 0;
                        if (pliTriggered || keyCheckCount < 3) {
                            bool hSps = annexbHasNalType(pkt.data, pkt.size, 7);
                            bool hPps = annexbHasNalType(pkt.data, pkt.size, 8);
                            bool hIdr = annexbHasNalType(pkt.data, pkt.size, 5);
                            INFO_LOG << "[whip] KEYFRAME size=" << pkt.size
                                     << " sps=" << hSps << " pps=" << hPps
                                     << " idr=" << hIdr
                                     << (pliTriggered ? " (after PLI)" : "");
                            if (!hSps || !hPps)
                                WARNING_LOG << "[whip] KEYFRAME missing SPS/PPS"
                                               " — viewer will see corruption!";
                            keyCheckCount++;
                        }
                    } else if (lastIdrPts == 0 ||
                               (pkt.pts - lastIdrPts) > 180000) {
                        _videoEnc->requestKeyframe();
                    }

                    uint32_t rtpTs = videoRtpTimestamp(pkt.pts);

                    // Annex-B 校验
                    bool isAnnexB = (pkt.size >= 4 && pkt.data[0]==0x00 &&
                        pkt.data[1]==0x00 && (pkt.data[2]==0x01 ||
                        (pkt.data[2]==0x00 && pkt.data[3]==0x01)));
                    if (!isAnnexB) {
                        WARNING_LOG << "[whip] non-Annex-B packet blocked,"
                                    << " size=" << pkt.size;
                        av_packet_unref(&pkt);
                        continue;
                    }

                    // 关键帧缺 SPS/PPS → 从缓存补
                    bool isIdr = annexbHasNalType(pkt.data, pkt.size, 5);
                    static thread_local std::vector<uint8_t> auBuf;
                    const uint8_t* sendData = pkt.data;
                    size_t sendSize = pkt.size;
                    if (isIdr && !_spsPpsAnnexB.empty()) {
                        bool hSps = annexbHasNalType(pkt.data, pkt.size, 7);
                        bool hPps = annexbHasNalType(pkt.data, pkt.size, 8);
                        if (!hSps || !hPps) {
                            auBuf.clear();
                            auBuf.reserve(_spsPpsAnnexB.size() + pkt.size);
                            auBuf.insert(auBuf.end(), _spsPpsAnnexB.begin(),
                                         _spsPpsAnnexB.end());
                            auBuf.insert(auBuf.end(), pkt.data,
                                         pkt.data + pkt.size);
                            sendData = auBuf.data();
                            sendSize = auBuf.size();
                            INFO_LOG << "[whip] prepended SPS/PPS before IDR,"
                                     << " size=" << sendSize;
                        }
                    }

                    // ---- 诊断：保存 sendFrame 前的 Annex-B H.264 到文件 ----
                    {
                        static std::ofstream h264File;
                        static int h264Cnt = 0;
                        if (!h264File.is_open()) {
                            h264File.open("C:/temp/after_bsf.h264",
                                          std::ios::binary | std::ios::app);
                        }
                        if (h264File.is_open() && h264Cnt < 300) {
                            h264File.write(reinterpret_cast<const char*>(sendData),
                                           sendSize);
                            h264Cnt++;
                            if (h264Cnt == 300) {
                                h264File.close();
                                INFO_LOG << "H264 dump done: C:/temp/after_bsf.h264";
                            }
                        }
                    }
                    // ----

                    if (_videoTrack && _videoTrackOpen.load()) {
                        ::rtc::binary data(
                            reinterpret_cast<const std::byte*>(sendData),
                            reinterpret_cast<const std::byte*>(sendData) + sendSize);
                        ::rtc::FrameInfo info(rtpTs);
                        try {
                            _videoTrack->sendFrame(std::move(data), info);
                        } catch (const std::exception& e) {
                            static int errCount = 0;
                            if (++errCount <= 5) {
                                WARNING_LOG << "[whip] video sendFrame:"
                                           << e.what();
                            }
                        }
                    }
#endif
                }
                av_packet_unref(&pkt);
                gotAny = true;
            }
        }

        // ---- 拉音频包 ----
        if (_audioEnc && _audioEnc->isRunning()) {
            if (_audioEnc->getPacket(&pkt, 2)) {
                audCount++;
                audBytes += pkt.size;

                if (!_cfg.dryRun) {
#ifdef SPARK_MEDIA_PUBLISHER_WITH_LIBDATACHANNEL
                    uint32_t rtpTs = audioRtpTimestamp(pkt.pts);
                    if (_audioTrack && _audioTrackOpen.load()) {
                        ::rtc::binary data(
                            reinterpret_cast<const std::byte*>(pkt.data),
                            reinterpret_cast<const std::byte*>(pkt.data) + pkt.size);
                        ::rtc::FrameInfo info(rtpTs);
                        try {
                            _audioTrack->sendFrame(std::move(data), info);
                        } catch (const std::exception& e) {
                            static int errCount = 0;
                            if (++errCount <= 5) {
                                WARNING_LOG << "[whip] audio sendFrame:"
                                           << e.what();
                            }
                        }
                    }
#endif
                }
                av_packet_unref(&pkt);
                gotAny = true;
            }
        }

        auto now = std::chrono::steady_clock::now();
        if (now - lastLog >= std::chrono::seconds(3)) {
            if (_cfg.dryRun) {
                INFO_LOG << "[whip:dryrun]"
                         << "vid_pkts:" << vidCount
                         << "aud_pkts:" << audCount
                         << "vid_bytes:" << vidBytes
                         << "aud_bytes:" << audBytes;
            } else {
                INFO_LOG << "[whip]"
                         << "video_frames:" << vidCount
                         << "audio_frames:" << audCount;
            }
            lastLog = now;
        }

        if (!gotAny)
            std::this_thread::sleep_for(std::chrono::microseconds(500));
    }

    _videoSent.store(vidCount);
    _audioSent.store(audCount);
    _videoBytes.store(vidBytes);
    _audioBytes.store(audBytes);

    INFO_LOG << "WhipPublisher: publishLoop exit —"
             << "video:" << vidCount << "audio:" << audCount;
}

// ============================================================================
// WebRTC / libdatachannel 初始化
// ============================================================================

#ifdef SPARK_MEDIA_PUBLISHER_WITH_LIBDATACHANNEL

bool Oran7WhipPublisher::initPeerConnection() {
    ::rtc::Configuration rtcCfg;
    rtcCfg.enableIceTcp = true;
    rtcCfg.enableIceUdpMux = true;

    if (!_cfg.stunUrl.empty())
        rtcCfg.iceServers.emplace_back(_cfg.stunUrl);
    if (!_cfg.turnUrl.empty()) {
        ::rtc::IceServer turn(_cfg.turnUrl);
        turn.username = _cfg.turnUser;
        turn.password = _cfg.turnPass;
        rtcCfg.iceServers.push_back(turn);
    }

    _pc = std::make_shared<::rtc::PeerConnection>(rtcCfg);

    _pc->onStateChange([](::rtc::PeerConnection::State s) {
        INFO_LOG << "[whip] pc state:" << pcStateStr(s).c_str();
    });
    _pc->onIceStateChange([](::rtc::PeerConnection::IceState s) {
        INFO_LOG << "[whip] ice state:" << iceStateStr(s).c_str();
    });
    _pc->onGatheringStateChange([](::rtc::PeerConnection::GatheringState s) {
        INFO_LOG << "[whip] gathering:" << gatherStateStr(s).c_str();
    });

    _pc->onLocalCandidate([this](::rtc::Candidate cand) {
        INFO_LOG << "[whip] local ICE:" << std::string(cand).c_str();
        // 收集本地 ICE 并发送到服务器
        if (_onIceCandidate)
            _onIceCandidate(cand.candidate(), cand.mid());
        // 直接发送 Trickle ICE
        std::string mid = cand.mid();
        std::string line = std::string(cand); // 已经是 a=candidate:... 格式
        whipPatchIce(mid, line);
    });

    _pc->onTrack([](std::shared_ptr<::rtc::Track> track) {
        if (track)
            INFO_LOG << "[whip] unexpected onTrack mid:"
                     << track->mid().c_str();
    });

    _cname = "oran7-publisher";
    INFO_LOG << "[whip] PeerConnection created";
    return true;
}

bool Oran7WhipPublisher::createVideoTrack(std::string* out_mid) {
    if (!_pc) return false;

    _videoSsrc = randomSsrc();

    ::rtc::Description::Video desc("1",
                                    ::rtc::Description::Direction::SendOnly);

    const auto& plid = _h264ProfileLevelId.empty()
        ? g_h264ProfileLevelId : _h264ProfileLevelId;
    if (plid.empty()) {
        WARNING_LOG << "[whip] profile-level-id not yet extracted from SPS"
                       " — SDP may not match actual codec output!";
    }
    std::string fmtp = std::string(
        "level-asymmetry-allowed=1;packetization-mode=1;"
        "profile-level-id=") + (plid.empty() ? "42c01f" : plid);
    desc.addH264Codec(_cfg.videoPayloadType, fmtp);
    desc.setBitrate(_cfg.videoBitrateKbps);
    desc.addSSRC(_videoSsrc, _cname);

    _videoTrack = _pc->addTrack(desc);
    if (!_videoTrack) {
        if (_onError) _onError("addTrack(video) failed");
        return false;
    }

    if (out_mid) *out_mid = _videoTrack->mid();
    INFO_LOG << "[whip] addTrack(video) ok mid:"
             << _videoTrack->mid().c_str()
             << "ssrc:" << _videoSsrc;

    _videoTrack->onOpen([this]() {
        _videoTrackOpen.store(true);
        INFO_LOG << "[whip] video track OPENED";
    });
    _videoTrack->onClosed([this]() {
        _videoTrackOpen.store(false);
        WARNING_LOG << "[whip] video track CLOSED";
    });

    // ---- Packetizer 链（和服务器 WHEP 发送端完全一样）----
    auto rtpCfg = std::make_shared<::rtc::RtpPacketizationConfig>(
        _videoSsrc, _cname, _cfg.videoPayloadType,
        ::rtc::RtpPacketizer::VideoClockRate);

    auto packetizer = std::make_shared<::rtc::H264RtpPacketizer>(
        ::rtc::H264RtpPacketizer::Separator::StartSequence,
        rtpCfg, 1000);

    packetizer->addToChain(
        std::make_shared<::rtc::RtcpSrReporter>(rtpCfg));
    packetizer->addToChain(
        std::make_shared<::rtc::RtcpNackResponder>());
    packetizer->addToChain(
        std::make_shared<::rtc::PliHandler>([this]() {
            auto nowUs = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            int64_t last = _lastPliUs.load();
            if (last != 0 && (nowUs - last) < 500000)
                return;
            _lastPliUs.store(nowUs);
            _forceKeyframe.store(true);

            static std::atomic<uint64_t> pliCnt{0};
            auto n = pliCnt.fetch_add(1) + 1;
            if (n <= 5 || (n % 50 == 0))
                INFO_LOG << "[whip] PLI received count:" << n;
        }));

    _videoTrack->setMediaHandler(packetizer);
    INFO_LOG << "[whip] video chain: H264 -> SR -> NACK -> PLI";
    return true;
}

bool Oran7WhipPublisher::createAudioTrack(std::string* out_mid) {
    if (!_pc) return false;

    _audioSsrc = randomSsrc();

    ::rtc::Description::Audio desc("0",
                                    ::rtc::Description::Direction::SendOnly);
    desc.addOpusCodec(_cfg.audioPayloadType);
    desc.addSSRC(_audioSsrc, _cname);

    _audioTrack = _pc->addTrack(desc);
    if (!_audioTrack) {
        WARNING_LOG << "[whip] addTrack(audio) failed - no audio";
        return false;
    }

    if (out_mid) *out_mid = _audioTrack->mid();
    INFO_LOG << "[whip] addTrack(audio) ok mid:"
             << _audioTrack->mid().c_str()
             << "ssrc:" << _audioSsrc;

    _audioTrack->onOpen([this]() {
        _audioTrackOpen.store(true);
        INFO_LOG << "[whip] audio track OPENED";
    });
    _audioTrack->onClosed([this]() {
        _audioTrackOpen.store(false);
        WARNING_LOG << "[whip] audio track CLOSED";
    });

    auto rtpCfg = std::make_shared<::rtc::RtpPacketizationConfig>(
        _audioSsrc, _cname, _cfg.audioPayloadType, 48000);

    auto packetizer = std::make_shared<::rtc::OpusRtpPacketizer>(rtpCfg);
    packetizer->addToChain(
        std::make_shared<::rtc::RtcpSrReporter>(rtpCfg));
    packetizer->addToChain(
        std::make_shared<::rtc::RtcpNackResponder>());

    _audioTrack->setMediaHandler(packetizer);
    INFO_LOG << "[whip] audio chain: Opus -> SR -> NACK";
    return true;
}

bool Oran7WhipPublisher::generateOffer(std::string* out_sdp) {
    if (!_pc || !out_sdp) return false;

    createAudioTrack(nullptr);
    if (!createVideoTrack(nullptr))
        return false;

    _pc->setLocalDescription();

    {
        std::mutex mtx;
        std::condition_variable cv;
        bool done = false;
        _pc->onGatheringStateChange(
            [&](::rtc::PeerConnection::GatheringState s) {
                if (s == ::rtc::PeerConnection::GatheringState::Complete) {
                    std::lock_guard<std::mutex> lock(mtx);
                    done = true;
                    cv.notify_one();
                }
            });
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(3), [&]{ return done; });
    }

    auto desc = _pc->localDescription();
    if (!desc) return false;

    std::string sdp = std::string(desc.value());
    sdp = injectExtmap(sdp);
    *out_sdp = sdp;
    return true;
}

// ============================================================================
// WHIP HTTP — 修正为匹配 spark-meeting 服务器端点
// ============================================================================
//
// 服务器端点（main.cpp）:
//   POST   /whip                     → 发送 SDP Offer，返回 Answer + Location
//   PATCH  /whip/resource/{session}  → Trickle ICE，body 是 SDP fragment
//   DELETE /whip/resource/{session}  → 终止会话
//
// 鉴权: Authorization: Bearer 123456
//
// ICE 格式: Content-Type: application/trickle-ice-sdpfrag
//           body: a=mid:0\r\na=candidate:...\r\n

bool Oran7WhipPublisher::whipPostOffer(const std::string& offerSdp,
                                        std::string* outAnswer)
{
    std::string url = _cfg.serverUrl + "/whip";

    INFO_LOG << "[whip] POST offer to:" << url.c_str();

    auto r = httpPostSync(url, offerSdp, "application/sdp");

    INFO_LOG << "[whip] POST response: status=" << r.status
             << " body_size=" << (int)r.body.size()
             << " location=" << r.resourcePath.c_str();

    if (r.status != 201 && r.status != 200) {
        if (_onError) {
            _onError(std::string("[whip] POST offer: HTTP ") +
                     std::to_string(r.status) + " body=" + r.body);
        }
        return false;
    }

    // 保存服务器返回的资源路径和 ETag，后续 PATCH/DELETE 要用
    _whipResourcePath = r.resourcePath;
    _whipEtag = r.etag;
    INFO_LOG << "[whip] resource=" << _whipResourcePath.c_str()
             << " etag=" << _whipEtag.c_str();

    *outAnswer = r.body;
    return true;
}

bool Oran7WhipPublisher::whipPatchIce(const std::string& mid,
                                       const std::string& candidate)
{
    // 使用 Location 返回的资源路径
    std::string url;
    if (!_whipResourcePath.empty()) {
        // 拼接服务器地址 + 资源路径
        url = _cfg.serverUrl + _whipResourcePath;
    } else {
        // fallback：还没拿到 resource path 时不发
        return true;
    }

    // 构造 SDP fragment 格式 body（不是 JSON！）
    std::string body;
    if (!mid.empty())
        body += "a=mid:" + mid + "\r\n";
    body += candidate + "\r\n";  // candidate 已经是 a=candidate:... 格式

    auto r = httpPatchSync(url, body,
                           "application/trickle-ice-sdpfrag",
                           _whipEtag);

    // 更新 ETag
    if (!r.etag.empty())
        _whipEtag = r.etag;

    if (r.status != 200 && r.status != 204) {
        static int errCount = 0;
        if (++errCount <= 3) {
            WARNING_LOG << "[whip] PATCH ice failed: status="
                       << r.status;
        }
    }
    return (r.status == 200 || r.status == 204);
}

bool Oran7WhipPublisher::whipPollCandidates(
    std::vector<std::string>* outCandidates)
{
    if (_whipResourcePath.empty())
        return true;

    std::string url = _cfg.serverUrl + _whipResourcePath;

    auto r = httpPatchSync(url, "",
                           "application/trickle-ice-sdpfrag",
                           _whipEtag);
    if (!r.etag.empty())
        _whipEtag = r.etag;

    if (!r.body.empty() && r.body.find("candidate:") != std::string::npos)
        outCandidates->push_back(r.body);
    return (r.status == 200 || r.status == 204);
}

bool Oran7WhipPublisher::whipDelete() {
    if (_whipResourcePath.empty())
        return true;

    std::string url = _cfg.serverUrl + _whipResourcePath;

    int status = httpDeleteSync(url, _whipEtag);
    INFO_LOG << "[whip] DELETE session: status=" << status;
    return true;
}

// ============================================================================
// RTP 时间戳
// ============================================================================

uint32_t Oran7WhipPublisher::videoRtpTimestamp(int64_t pts) {
    int64_t base = _videoTsBaseUs.load();
    if (base < 0) {
        _videoTsBaseUs.store(pts);
        _videoTsBase90k.store(
            static_cast<uint32_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()
                ).count() & 0xFFFFFFFFu));
        base = pts;
    }
    return _videoTsBase90k.load() + static_cast<uint32_t>(pts - base);
}

uint32_t Oran7WhipPublisher::audioRtpTimestamp(int64_t pts) {
    int64_t base = _audioTsBaseUs.load();
    if (base < 0) {
        _audioTsBaseUs.store(pts);
        _audioTsBase48k.store(
            static_cast<uint32_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()
                ).count() & 0xFFFFFFFFu));
        base = pts;
    }
    return _audioTsBase48k.load() + static_cast<uint32_t>(pts - base);
}

#endif // SPARK_MEDIA_PUBLISHER_WITH_LIBDATACHANNEL

// ============================================================================
// H264 BSF: AVCC → Annex-B
// ============================================================================

bool Oran7WhipPublisher::initH264Bsf() {
    // 1. 从 extradata 提取 SPS/PPS + profile-level-id
    // 2. 初始化 h264_mp4toannexb BSF 做格式转换

    const AVCodecContext* encCtx = _videoEnc->codecContext();
    if (!encCtx) {
        WARNING_LOG << "[whip] no encoder ctx";
        return false;
    }

    // ======== SPS/PPS 提取 ========
    if (encCtx->extradata && encCtx->extradata_size > 0) {
        const uint8_t* d = encCtx->extradata;
        const int n = encCtx->extradata_size;
        {
            std::ostringstream hex;
            for (int i = 0; i < std::min(n, 64); ++i) {
                char buf[8];
                snprintf(buf, sizeof(buf), "%02x ", d[i]);
                hex << buf;
            }
            INFO_LOG << "[whip] extradata[" << n << "]: " << hex.str().c_str();
        }

        bool isAnnexB = (n>=4 && d[0]==0x00 && d[1]==0x00 &&
                         (d[2]==0x01 || (d[2]==0x00 && d[3]==0x01)));

        if (isAnnexB) {
            _spsPpsAnnexB.assign(d, d + n);
            for (int i = 0; i + 6 < n; ++i) {
                int sc=0;
                if (d[i]==0&&d[i+1]==0&&d[i+2]==0&&d[i+3]==1) sc=4;
                else if (d[i]==0&&d[i+1]==0&&d[i+2]==1) sc=3;
                else continue;
                int hdr=i+sc;
                if (hdr+4>=n) continue;
                if ((d[hdr]&0x1F)==7) {
                    char b[8];
                    snprintf(b,sizeof(b),"%02x%02x%02x",d[hdr+1],d[hdr+2],d[hdr+3]);
                    _h264ProfileLevelId=b; g_h264ProfileLevelId=_h264ProfileLevelId;
                    INFO_LOG << "[whip] profile-level-id:" << _h264ProfileLevelId.c_str();
                    break;
                }
            }
        } else if (n>=7 && d[0]==1) {
            int off=5; off+=1; int nSps=d[off++]&0x1F; _spsPpsAnnexB.clear();
            auto pSC=[](std::vector<uint8_t>&v){v.push_back(0);v.push_back(0);v.push_back(0);v.push_back(1);};
            for (int i=0;i<nSps;++i){int l=(d[off]<<8)|d[off+1];off+=2;
                if (i==0&&l>=4){char b[8];snprintf(b,sizeof(b),"%02x%02x%02x",d[off+1],d[off+2],d[off+3]);
                    _h264ProfileLevelId=b; g_h264ProfileLevelId=_h264ProfileLevelId;
                    INFO_LOG << "[whip] profile-level-id:" << _h264ProfileLevelId.c_str();}
                pSC(_spsPpsAnnexB);_spsPpsAnnexB.insert(_spsPpsAnnexB.end(),d+off,d+off+l);off+=l;}
            int nPps=d[off++];
            for (int i=0;i<nPps;++i){int l=(d[off]<<8)|d[off+1];off+=2;
                pSC(_spsPpsAnnexB);_spsPpsAnnexB.insert(_spsPpsAnnexB.end(),d+off,d+off+l);off+=l;}
            INFO_LOG << "[whip] SPS/PPS extracted, size:" << _spsPpsAnnexB.size();
        }
    } else {
        WARNING_LOG << "[whip] no extradata, SPS/PPS may be in-band";
    }

    // ======== BSF 初始化（格式转换 AVCC→Annex-B）========
    const AVBitStreamFilter* filter = av_bsf_get_by_name("h264_mp4toannexb");
    if (!filter) {
        if (_onError) _onError("[whip] BSF h264_mp4toannexb not found");
        return false;
    }
    int ret = av_bsf_alloc(filter, &_h264Bsf);
    if (ret < 0 || !_h264Bsf) {
        if (_onError) _onError("[whip] BSF alloc failed");
        return false;
    }
    ret = avcodec_parameters_from_context(_h264Bsf->par_in, encCtx);
    if (ret < 0) {
        if (_onError) _onError("[whip] BSF par_in failed");
        av_bsf_free(&_h264Bsf); _h264Bsf = nullptr;
        return false;
    }
    ret = av_bsf_init(_h264Bsf);
    if (ret < 0) {
        if (_onError) _onError("[whip] BSF init failed");
        av_bsf_free(&_h264Bsf); _h264Bsf = nullptr;
        return false;
    }
    INFO_LOG << "[whip] h264_mp4toannexb BSF ready";
    return true;
}

bool Oran7WhipPublisher::convertToAnnexB(AVPacket* pkt) {
    if (!_h264Bsf || !pkt) return true;

    int ret = av_bsf_send_packet(_h264Bsf, pkt);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
        static int ec = 0; if (++ec <= 5) WARNING_LOG << "[whip] BSF send:" << ret;
        return false;
    }

    // 循环 drain 直到 EAGAIN，取不到正常输出则失败
    bool gotOutput = false;
    while (true) {
        AVPacket tmp{};
        ret = av_bsf_receive_packet(_h264Bsf, &tmp);
        if (ret == 0) {
            // 把 BSF 输出拷贝回 pkt
            av_packet_unref(pkt);
            av_packet_move_ref(pkt, &tmp);
            gotOutput = true;
            // 继续循环 drain 剩余包（正常情况一个输入出一个输出）
            continue;
        }
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        static int ec = 0; if (++ec <= 5) WARNING_LOG << "[whip] BSF recv:" << ret;
        break;
    }
    return gotOutput;  // 没拿到输出 = 失败
}

// ============================================================================
// 诊断接口
// ============================================================================

int Oran7WhipPublisher::videoPktQueued() const {
    return _videoEnc ? _videoEnc->queuedPktCount() : -1;
}

int Oran7WhipPublisher::audioPktQueued() const {
    return _audioEnc ? _audioEnc->queuedPktCount() : -1;
}

std::string Oran7WhipPublisher::streamId() const {
    // _whipResourcePath 格式: "/whip/resource/0073fd0af53c71cd-0"
    // 提取最后一段
    auto pos = _whipResourcePath.rfind('/');
    if (pos != std::string::npos)
        return _whipResourcePath.substr(pos + 1);
    return {};
}
