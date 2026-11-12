#include "Oran7Mp4Muxer.h"

Oran7Mp4Muxer::Oran7Mp4Muxer() {}

Oran7Mp4Muxer::~Oran7Mp4Muxer()
{
    this->close();
}

bool Oran7Mp4Muxer::open(const char* filePath, int width, int height, AVRational frameRate,
                         std::shared_ptr<Oran7VideoEncoder>& video_encoder,
                         std::shared_ptr<Oran7AudioEncoder>& audio_encoder, const char* format)
{
    if (!video_encoder || !filePath || !format || (strcmp(format, "mp4") && strcmp(format, "flv")))
        return false;
    _video_encoder = video_encoder;

    // 1. 创建输出上下文
    avformat_alloc_output_context2(&_muxFmtCtx, nullptr, format, filePath);
    if (!_muxFmtCtx)
        return false;

    // 2.创建视频流
    _videoStream = avformat_new_stream(_muxFmtCtx, nullptr);
    if (!_videoStream)
    {
        avformat_free_context(_muxFmtCtx);
        _muxFmtCtx = nullptr;
        return false;
    }

    _videoStream->time_base = video_encoder->encoderTimeBase(); // {1, 90000}
    _videoStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    _videoStream->codecpar->codec_id = AV_CODEC_ID_H264;
    _videoStream->codecpar->width = width;
    _videoStream->codecpar->height = height;

    // 3.打开输出文件
    if (avio_open(&_muxFmtCtx->pb, filePath, AVIO_FLAG_WRITE) < 0)
    {
        avformat_free_context(_muxFmtCtx);
        _muxFmtCtx = nullptr;
        return false;
    }

    const AVCodecContext* enc_ctx = _video_encoder->codecContext();
    if (!enc_ctx || avcodec_parameters_from_context(_videoStream->codecpar, enc_ctx) < 0)
    {
        avio_closep(&_muxFmtCtx->pb);
        avformat_free_context(_muxFmtCtx);
        _muxFmtCtx = nullptr;
        return false;
    }
    _videoStream->avg_frame_rate = av_inv_q(frameRate);

    // 3.5 创建音频流
    if (audio_encoder)
    {
        _audio_encoder = audio_encoder;
        _audioStream = avformat_new_stream(_muxFmtCtx, nullptr);
        if (!_audioStream)
        {
            avio_closep(&_muxFmtCtx->pb);
            avformat_free_context(_muxFmtCtx);
            _muxFmtCtx = nullptr;
            return false;
        }
        _audioStream->time_base = audio_encoder->encoderTimeBase();
        const AVCodecContext* a_ctx = _audio_encoder->codecContext();
        if (!a_ctx || avcodec_parameters_from_context(_audioStream->codecpar, a_ctx) < 0)
        {
            avio_closep(&_muxFmtCtx->pb);
            avformat_free_context(_muxFmtCtx);
            _muxFmtCtx = nullptr;
            return false;
        }
    }

    // 4. 写文件头
    if (avformat_write_header(_muxFmtCtx, nullptr) < 0)
    {
        avio_closep(&_muxFmtCtx->pb);
        avformat_free_context(_muxFmtCtx);
        _muxFmtCtx = nullptr;
        return false;
    }

    _opened = true;
    _running.store(true);
    _thread = std::thread(&Oran7Mp4Muxer::muxLoop, this);
    return true;
}

void Oran7Mp4Muxer::close()
{
    _running.store(false);
    if (_thread.joinable())
        _thread.join();
    // muxLoop 退出后，排空编码器 flush 残留包
    if (_opened)
        drainRemaining();
    // 按容器格式写入收尾信息（MP4 的 moov / FLV 的元数据）。
    flush();
}

void Oran7Mp4Muxer::drainRemaining()
{
    AVPacket pkt{};
    if (_video_encoder)
    {
        while (_video_encoder->getPacket(&pkt, 5))
        {
            if (!writePacket(&pkt, _videoStream, _video_encoder->encoderTimeBase()))
                return;
        }
    }
    if (_audio_encoder)
    {
        while (_audio_encoder->getPacket(&pkt, 5))
        {
            if (!writePacket(&pkt, _audioStream, _audio_encoder->encoderTimeBase()))
                return;
        }
    }
}

void Oran7Mp4Muxer::muxLoop()
{
    int64_t vidWrites = 0, audWrites = 0;
    int64_t lastVideoWrites = 0;
    auto lastLog = std::chrono::steady_clock::now();
    while (_running.load())
    {
        AVPacket pkt{};
        bool got = false;

        // 两路都只取已有包：AAC 每秒约 47 包，不能每写一帧视频就等待它。
        // 有界批量消费保证公平，也能在一次调度唤醒后赶上积压。
        if (_video_encoder)
        {
            for (int batch = 0; batch < 8 && _running.load(); ++batch)
            {
                if (!_video_encoder->getPacket(&pkt, 0))
                    break;
                if (!writePacket(&pkt, _videoStream, _video_encoder->encoderTimeBase()))
                    return;
                vidWrites++;
                _writeIndex++;
                got = true;
            }
        }

        // 视频暂时没有输出时，同样不阻塞已就绪的音频。
        if (_audio_encoder)
        {
            for (int batch = 0; batch < 8 && _running.load(); ++batch)
            {
                if (!_audio_encoder->getPacket(&pkt, 0))
                    break;
                if (!writePacket(&pkt, _audioStream, _audio_encoder->encoderTimeBase()))
                    return;
                audWrites++;
                got = true;
            }
        }

        // 每 3 秒诊断
        auto now = std::chrono::steady_clock::now();
        if (Oran7GlobalHelper::developerMode() && now - lastLog >= std::chrono::seconds(3))
        {
            const double elapsed = std::chrono::duration<double>(now - lastLog).count();
            INFO_LOG << "[Muxer] video_fps=" << (vidWrites - lastVideoWrites) / elapsed << "video_packets=" << vidWrites
                     << "audio_packets=" << audWrites;
            lastVideoWrites = vidWrites;
            lastLog = now;
        }

        if (!got)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    INFO_LOG << "[Muxer] exit vid=" << vidWrites << " aud=" << audWrites;
}

void Oran7Mp4Muxer::flush()
{
    if (_muxFmtCtx)
    {
        if (_opened && av_write_trailer(_muxFmtCtx) < 0 && _onError)
            _onError("录制文件收尾失败");
        if (avio_closep(&_muxFmtCtx->pb) < 0 && _onError)
            _onError("录制文件关闭失败");
        avformat_free_context(_muxFmtCtx);
        _muxFmtCtx = nullptr;
    }
    _opened = false;
}

bool Oran7Mp4Muxer::writePacket(AVPacket* packet, AVStream* stream, AVRational sourceTimeBase)
{
    // FLV 在写头后使用毫秒时间基；仅设置 packet.time_base 不会转换 PTS/DTS。
    av_packet_rescale_ts(packet, sourceTimeBase, stream->time_base);
    packet->time_base = stream->time_base;
    packet->stream_index = stream->index;
    const int status = av_interleaved_write_frame(_muxFmtCtx, packet);
    av_packet_unref(packet);
    if (status >= 0)
        return true;
    _running.store(false);
    if (_onError)
        _onError("录制文件写入失败");
    return false;
}
