#include "Oran7RtmpPublisher.h"
#include <QUrl>
#include <algorithm>

Oran7RtmpPublisher::Oran7RtmpPublisher(std::shared_ptr<std::atomic_bool> cancelled) : m_cancelled(std::move(cancelled))
{
}
Oran7RtmpPublisher::~Oran7RtmpPublisher()
{
    close();
}
void Oran7RtmpPublisher::arm(int milliseconds)
{
    m_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
}
int Oran7RtmpPublisher::interrupt(void* opaque)
{
    auto* self = static_cast<Oran7RtmpPublisher*>(opaque);
    return (self->m_cancelled && self->m_cancelled->load()) || std::chrono::steady_clock::now() >= self->m_deadline;
}
bool Oran7RtmpPublisher::available()
{
    return av_guess_format("flv", nullptr, nullptr) && avio_find_protocol_name("rtmp://127.0.0.1/live/test");
}
QString Oran7RtmpPublisher::validate(const QString& server, const QString& key)
{
    const QUrl url(server, QUrl::StrictMode);
    if (server.size() > 8192 || server.contains(QChar('\0')) || server.contains('\r') || server.contains('\n') ||
        !url.isValid() || url.scheme() != "rtmp" || url.host().isEmpty() || !url.userInfo().isEmpty() ||
        url.hasQuery() || url.hasFragment())
        return QStringLiteral("推流服务器须为有效 RTMP 地址；签名参数请放在推流码中");
    if (key.trimmed().isEmpty() || key.size() > 8192 || key.contains(QChar('\0')) || key.contains('\r') ||
        key.contains('\n') || key.startsWith('/') || key.contains('#') || key.contains("://"))
        return QStringLiteral("请输入有效的推流码");
    return {};
}
bool Oran7RtmpPublisher::open(const QString& server, const QString& key, const AVCodecParameters* video,
                              AVRational videoTimeBase, const AVCodecParameters* audio, AVRational audioTimeBase,
                              int timeoutMs)
{
    close();
    m_error = validate(server, key);
    m_failed = false;
    m_payloadBytes = m_videoPackets = 0;
    if (!m_error.isEmpty())
        return false;
    if (!m_cancelled || m_cancelled->load())
    {
        m_error = QStringLiteral("推流已取消");
        return false;
    }
    if (!available() || !video || video->codec_id != AV_CODEC_ID_H264 || video->codec_type != AVMEDIA_TYPE_VIDEO ||
        videoTimeBase.num <= 0 || videoTimeBase.den <= 0 ||
        (audio && (audio->codec_id != AV_CODEC_ID_AAC || audio->codec_type != AVMEDIA_TYPE_AUDIO ||
                   audioTimeBase.num <= 0 || audioTimeBase.den <= 0)))
    {
        m_error = QStringLiteral("RTMP 输出需要 H.264 视频和可选 AAC 音频");
        return false;
    }
    m_timeoutMs = std::clamp(timeoutMs, 100, 15000);
    auto endpoint = server;
    while (endpoint.endsWith('/'))
        endpoint.chop(1);
    const auto url = (endpoint + '/' + key).toUtf8();
    if (avformat_alloc_output_context2(&m_context, nullptr, "flv", url.constData()) < 0 || !m_context)
    {
        m_error = QStringLiteral("无法创建 RTMP 封装器");
        return false;
    }
    m_context->interrupt_callback = {&Oran7RtmpPublisher::interrupt, this};
    m_context->max_interleave_delta = 100000;
    auto add = [&](const AVCodecParameters* params, AVRational tb)
    {
        auto* stream = avformat_new_stream(m_context, nullptr);
        if (!stream || avcodec_parameters_copy(stream->codecpar, params) < 0)
            return static_cast<AVStream*>(nullptr);
        stream->time_base = tb;
        return stream;
    };
    m_video = add(video, videoTimeBase);
    if (audio)
        m_audio = add(audio, audioTimeBase);
    if (!m_video || (audio && !m_audio))
    {
        m_error = QStringLiteral("无法创建推流媒体轨道");
        close();
        return false;
    }
    AVDictionary* options = nullptr;
    av_dict_set_int(&options, "rw_timeout", m_timeoutMs * 1000LL, 0);
    const QUrl serverUrl(server);
    if (serverUrl.path().isEmpty() || serverUrl.path() == "/")
    {
        // 直播姬本地接收端没有应用路径；不能将推流码自动解析成 app。
        av_dict_set(&options, "rtmp_app", "", 0);
        av_dict_set(&options, "rtmp_playpath", key.toUtf8().constData(), 0);
        av_dict_set(&options, "rtmp_tcurl", (endpoint + '/').toUtf8().constData(), 0);
    }

    arm(m_timeoutMs);
    const int opened =
        avio_open2(&m_context->pb, url.constData(), AVIO_FLAG_WRITE, &m_context->interrupt_callback, &options);
    av_dict_free(&options);
    if (opened < 0)
    {
        m_error = QStringLiteral("RTMP 连接失败、超时或已取消");
        m_failed = true;
        close();
        return false;
    }
    av_dict_set(&options, "flvflags", "no_duration_filesize", 0);
    arm(m_timeoutMs);
    const int header = avformat_write_header(m_context, &options);
    av_dict_free(&options);
    if (header < 0)
    {
        m_error = QStringLiteral("RTMP 媒体头发送失败");
        m_failed = true;
        close();
        return false;
    }
    m_header = true;
    return true;
}
bool Oran7RtmpPublisher::write(const AVPacket* packet, bool video, AVRational tb)
{
    auto* stream = video ? m_video : m_audio;
    if (!m_header || m_failed || !stream || !packet || tb.num <= 0 || tb.den <= 0)
        return false;
    auto* copy = av_packet_clone(packet);
    if (!copy)
    {
        m_error = QStringLiteral("推流数据包分配失败");
        m_failed = true;
        return false;
    }
    const int bytes = copy->size;
    av_packet_rescale_ts(copy, tb, stream->time_base);
    copy->stream_index = stream->index;
    copy->pos = -1;
    arm(m_timeoutMs);
    const int result = av_interleaved_write_frame(m_context, copy);
    av_packet_free(&copy);
    if (result < 0)
    {
        m_error = QStringLiteral("RTMP 发送失败、超时或已取消");
        m_failed = true;
        return false;
    }
    m_payloadBytes += std::max(0, bytes);
    if (video)
        ++m_videoPackets;
    return true;
}
void Oran7RtmpPublisher::close()
{
    if (!m_context)
        return;
    arm(1000);
    if (m_header && !m_failed && !(m_cancelled && m_cancelled->load()))
        av_write_trailer(m_context);
    if (m_context->pb)
        avio_closep(&m_context->pb);
    avformat_free_context(m_context);
    m_context = nullptr;
    m_video = m_audio = nullptr;
    m_header = false;
}
