#include "Oran7Mp4Muxer.h"

Oran7Mp4Muxer::Oran7Mp4Muxer()
{

}

Oran7Mp4Muxer::~Oran7Mp4Muxer()
{
    this->close();
}

bool Oran7Mp4Muxer::open(const char *filePath, int width, int height,AVRational frameRate,
                         std::shared_ptr<Oran7VideoEncoder> &video_encoder,
                         std::shared_ptr<Oran7AudioEncoder> &audio_encoder)
{
    if(!video_encoder || !filePath) return false;
    _video_encoder = video_encoder;

    // 1. 创建输出上下文
    avformat_alloc_output_context2(&_muxFmtCtx, nullptr, nullptr,filePath);
    if (!_muxFmtCtx) return false;

    // 2.创建视频流
    _videoStream = avformat_new_stream(_muxFmtCtx,nullptr);
    if(!_videoStream){
        avformat_free_context(_muxFmtCtx);
        _muxFmtCtx = nullptr; return false;
    }

    _videoStream->time_base = video_encoder->encoderTimeBase();  // {1, 90000}
    _videoStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    _videoStream->codecpar->codec_id   = AV_CODEC_ID_H264;
    _videoStream->codecpar->width      = width;
    _videoStream->codecpar->height     = height;

    // 3.打开输出文件
    if(avio_open(&_muxFmtCtx->pb,filePath,AVIO_FLAG_WRITE) < 0){
        avformat_free_context(_muxFmtCtx);
        _muxFmtCtx = nullptr; return false;
    }

    const AVCodecContext* enc_ctx = _video_encoder->codecContext();
    if (enc_ctx && enc_ctx->extradata_size > 0) {
        _videoStream->codecpar->extradata =(uint8_t*)av_mallocz(enc_ctx->extradata_size +AV_INPUT_BUFFER_PADDING_SIZE);
        _videoStream->codecpar->extradata_size = enc_ctx->extradata_size;
        memcpy(_videoStream->codecpar->extradata, enc_ctx->extradata,enc_ctx->extradata_size);
    }

    // 3.5 创建音频流
    if (audio_encoder) {
        _audio_encoder = audio_encoder;
        _audioStream = avformat_new_stream(_muxFmtCtx, nullptr);
        _audioStream->time_base     = AVRational{1, 48000};
        _audioStream->codecpar->codec_type  = AVMEDIA_TYPE_AUDIO;
        _audioStream->codecpar->codec_id    = AV_CODEC_ID_AAC;
        _audioStream->codecpar->sample_rate = 48000;
        _audioStream->codecpar->ch_layout   = AV_CHANNEL_LAYOUT_STEREO;

        const AVCodecContext* a_ctx = _audio_encoder->codecContext();
        if (a_ctx && a_ctx->extradata_size > 0) {
            _audioStream->codecpar->extradata =
                (uint8_t*)av_mallocz(a_ctx->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
            _audioStream->codecpar->extradata_size = a_ctx->extradata_size;
            memcpy(_audioStream->codecpar->extradata, a_ctx->extradata,
                   a_ctx->extradata_size);
        }
        _audioStream->codecpar->frame_size = 1024;  // libfdk_aac 固定帧大小
    }

    // 4. 写文件头
    if(avformat_write_header(_muxFmtCtx,nullptr) < 0){
        avformat_free_context(_muxFmtCtx);
        _muxFmtCtx = nullptr; return false;
    }

    _opened = true;
    _running.store(true);
    _thread = std::thread(&Oran7Mp4Muxer::muxLoop,  this);
    return true;
}

void Oran7Mp4Muxer::close()
{
    if (!_running.exchange(false)) return;
    if (_thread.joinable())
        _thread.join();
    // muxLoop 退出后，排空编码器 flush 残留包
    drainRemaining();
    // 最后写 moov atom
    flush();
}

void Oran7Mp4Muxer::drainRemaining()
{
    AVPacket pkt;
    if (_video_encoder) {
        while (_video_encoder->getPacket(&pkt, 5)) {
            pkt.stream_index = _videoStream->index;
            pkt.time_base = _video_encoder->encoderTimeBase();
            av_interleaved_write_frame(_muxFmtCtx, &pkt);
            av_packet_unref(&pkt);
        }
    }
    if (_audio_encoder) {
        while (_audio_encoder->getPacket(&pkt, 5)) {
            pkt.stream_index = _audioStream->index;
            pkt.time_base = _audio_encoder->encoderTimeBase();
            av_interleaved_write_frame(_muxFmtCtx, &pkt);
            av_packet_unref(&pkt);
        }
    }
}

void Oran7Mp4Muxer::muxLoop()
{
    int64_t vidWrites = 0, audWrites = 0;
    while (_running.load()) {
        AVPacket pkt{};
        bool got = false;

        // 拉视频包（1ms 超时）
        if (_video_encoder) {
            pkt = {};
            if (_video_encoder->getPacket(&pkt, 1)) {
                pkt.stream_index = _videoStream->index;
                pkt.time_base = _video_encoder->encoderTimeBase();  // 告诉 muxer PTS 单位
                av_interleaved_write_frame(_muxFmtCtx, &pkt);
                av_packet_unref(&pkt);
                vidWrites++;
                _writeIndex++;
                got = true;
            }
        }

        // 拉音频包（1ms 超时）
        if (_audio_encoder) {
            pkt = {};
            if (_audio_encoder->getPacket(&pkt, 1)) {
                pkt.stream_index = _audioStream->index;
                pkt.time_base = _audio_encoder->encoderTimeBase();
                av_interleaved_write_frame(_muxFmtCtx, &pkt);
                av_packet_unref(&pkt);
                audWrites++;
                got = true;
            }
        }

        // 每 3 秒诊断
        static auto lastLog = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - lastLog >= std::chrono::seconds(3)) {
            printf("mux: vid=%lld aud=%lld\n", vidWrites, audWrites);
            lastLog = now;
        }

        if (!got)
            std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
    printf("mux exit: vid=%lld aud=%lld\n", vidWrites, audWrites);
}

void Oran7Mp4Muxer::flush()
{
    if (_muxFmtCtx) {
        av_write_trailer(_muxFmtCtx);
        avio_closep(&_muxFmtCtx->pb);
        avformat_free_context(_muxFmtCtx);
        _muxFmtCtx = nullptr;
    }
    _opened = false;
}
