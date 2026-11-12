#include "Oran7AudioEncoder.h"
#include <libswresample/swresample.h>

Oran7AudioEncoder::Oran7AudioEncoder() {}

Oran7AudioEncoder::~Oran7AudioEncoder()
{
    stop();

    while (!_frameQueue.empty()) {
        av_frame_free(&_frameQueue.front());
        _frameQueue.pop();
    }

    while (!_pktQueue.empty()) {
        AVPacket* p = _pktQueue.front();
        _pktQueue.pop();
        av_packet_free(&p);
    }

    if (_avctx)
        avcodec_free_context(&_avctx);
    if (_swr)
        swr_free(&_swr);
}

bool Oran7AudioEncoder::configure(const EncoderConfig &cfg, ErrorCallback onError)
{
    _onError = std::move(onError);

    _codec = avcodec_find_encoder_by_name(cfg.codecName.c_str());
    if (!_codec)
    {
        if (_onError)
            _onError("Audio encoder not found: " + cfg.codecName);
        return false;
    }

    // 从编码器支持的格式列表中选第一个（opus → FLT, aac → S16/FLTP）
    if (_codec->sample_fmts && _codec->sample_fmts[0] != AV_SAMPLE_FMT_NONE)
        _codecSampleFmt = _codec->sample_fmts[0];

    _avctx = avcodec_alloc_context3(_codec);
    if (!_avctx)
    {
        if (_onError)
            _onError("avcodec_alloc_context3(audio)failed");
        return false;
    }

    // 音频编码参数
    _avctx->codec_type  = AVMEDIA_TYPE_AUDIO;
    _avctx->sample_rate = cfg.sampleRate;
    av_channel_layout_default(&_avctx->ch_layout, cfg.channels);
    _avctx->time_base  = AVRational{1, cfg.sampleRate};
    // FFmpeg 原生 opus 编码器标记为 experimental，需要跳过严格检测
    _avctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    _avctx->bit_rate   = cfg.bitrateBps;
    _avctx->sample_fmt = _codecSampleFmt;

    // avcodec_open2 在线程内执行
    _cfg        = cfg;
    _configured = true;
    return true;
}

const AVCodecContext *Oran7AudioEncoder::codecContext() const
{
    return _avctx;
}

bool Oran7AudioEncoder::start()
{
    if(_running.exchange(true)) return true;

    if (!_configured || !_avctx) {
        _running.store(false);
        if (_onError)
            _onError("Oran7VideoEncoder::start(): not configured.");
        return false;
    }

    _frameIndex    = 0;
    _encode_thread = std::thread(&Oran7AudioEncoder::encodeLoop, this);
    return true;
}

bool Oran7AudioEncoder::stop()
{
    _running.store(false);

    _frameCond.notify_all();
    _pktCond.notify_all();

    if(_encode_thread.joinable())
        _encode_thread.join();

    return true;
}

void Oran7AudioEncoder::pushFrame(AVFrame *frame)
{
    if(!frame) return;
    std::lock_guard<std::mutex> lock(_frame_mtx);
    if (!_running.load()) { av_frame_free(&frame); return; }
    if(_frameQueue.size() >= kMaxFrameQueue)
    {
        AVFrame *drop = _frameQueue.front();
        _frameQueue.pop();
        av_frame_free(&drop);
    }

    _frameQueue.push(frame);
    _frameCond.notify_one();
}

bool Oran7AudioEncoder::getPacket(AVPacket *pkt, int timeoutMs)
{
    std::unique_lock<std::mutex> lock(_pkt_mtx);
    if(timeoutMs > 0 && !_pktCond.wait_for(lock,
            std::chrono::milliseconds(timeoutMs), [this](){
                return !_pktQueue.empty() || !_running.load();
        }))
    {
        return false;//超时等待
    }

    if(_pktQueue.empty()) return false;

    AVPacket* front = _pktQueue.front();
    _pktQueue.pop();
    av_packet_move_ref(pkt,front);
    av_packet_free(&front);
    return true;
}

/**
 * =========================================
 * @note Oran7AudioEncoder::private::
 * =========================================
 */

void Oran7AudioEncoder::encodeLoop()
{
    int ret = avcodec_open2(_avctx,_codec, nullptr);
    if(ret < 0){
        if(_onError) _onError("avcodec_open2(audio) failed");
        _running.store(false);
        return;
    }
    _opened.store(true);

    // 如果编码器要求的格式不是 S16，初始化重采样器（S16 → 目标格式）
    if (_codecSampleFmt != AV_SAMPLE_FMT_S16) {
        AVChannelLayout chLayout;
        av_channel_layout_default(&chLayout, _cfg.channels);
        swr_alloc_set_opts2(&_swr,
                            &chLayout, _codecSampleFmt, _cfg.sampleRate,
                            &chLayout, AV_SAMPLE_FMT_S16, _cfg.sampleRate,
                            0, nullptr);
        av_channel_layout_uninit(&chLayout);
        if (_swr) swr_init(_swr);
    }

    while(_running.load())
    {
        AVFrame* frame = nullptr;
        {
            std::unique_lock<std::mutex> lock(_frame_mtx);
            _frameCond.wait(lock,[this](){
                return !_frameQueue.empty() || !_running.load();
            });
            if(!_running.load()) break;
            if(_frameQueue.empty()) continue;
            frame = _frameQueue.front();
            _frameQueue.pop();
        }
        processFrame(frame);
        av_frame_free(&frame);
    }
    flushFrames();
}

void Oran7AudioEncoder::processFrame(AVFrame *frame)
{
    const int ch    = _cfg.channels;
    const int chunk = frame->nb_samples * ch;
    const int16_t* src = (const int16_t*)frame->data[0];

    // FIFO 空时，记录这一帧的 PTS 作为下块输出的基准
    if (_fifoUsed == 0)
        _fifoBasePts = frame->pts;

    // 拼入 FIFO
    _fifoBuf.insert(_fifoBuf.end(), src, src + chunk);
    _fifoUsed += frame->nb_samples;

    const int frameSize = _avctx->frame_size > 0
                              ? _avctx->frame_size : 1024;

    while (_fifoUsed >= frameSize) {
        const int need = frameSize * ch;
        const int16_t* data = _fifoBuf.data();

        // 1. 先构造 S16 中间帧（FIFO 里永远是 S16）
        AVFrame* s16Frame = av_frame_alloc();
        s16Frame->format      = AV_SAMPLE_FMT_S16;
        s16Frame->sample_rate = _cfg.sampleRate;
        s16Frame->ch_layout   = _avctx->ch_layout;
        s16Frame->nb_samples  = frameSize;
        av_frame_get_buffer(s16Frame, 0);
        memcpy(s16Frame->data[0], data, need * sizeof(int16_t));
        s16Frame->pts = _fifoBasePts;

        // 2. 如果编码器要 FLT，做 swr_convert
        AVFrame* encFrame = s16Frame;
        if (_swr) {
            encFrame = av_frame_alloc();
            encFrame->format      = _codecSampleFmt;
            encFrame->sample_rate = _cfg.sampleRate;
            encFrame->ch_layout   = _avctx->ch_layout;
            encFrame->nb_samples  = frameSize;
            av_frame_get_buffer(encFrame, 0);
            encFrame->pts = _fifoBasePts;
            swr_convert(_swr,
                        encFrame->data, frameSize,
                        (const uint8_t**)s16Frame->data, frameSize);
        }

        int ret = avcodec_send_frame(_avctx, encFrame);
        av_frame_free(&encFrame);
        if (_swr) av_frame_free(&s16Frame);  // 用 swr 时 s16Frame 是独立的

        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
            if (_onError)
                _onError("audio avcodec_send_frame: " + std::to_string(ret));
            break;
        }

        // receive 循环
        while (true) {
            AVPacket* pkt = av_packet_alloc();
            if (!pkt) break;
            ret = avcodec_receive_packet(_avctx, pkt);
            if (ret == 0) {
                std::lock_guard<std::mutex> lock(_pkt_mtx);
                if (_pktQueue.size() >= kMaxPktQueued) {
                    AVPacket* drop = _pktQueue.front();
                    _pktQueue.pop();
                    av_packet_free(&drop);
                }
                _pktQueue.push(pkt);
                _pktCond.notify_one();
                continue;
            }
            av_packet_free(&pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            break;
        }

        // 移除已消费数据，平移 PTS
        _fifoBuf.erase(_fifoBuf.begin(), _fifoBuf.begin() + need);
        _fifoUsed -= frameSize;
        _fifoBasePts += frameSize;
    }
}

void Oran7AudioEncoder::flushFrames()
{
    const int frameSize = _avctx->frame_size > 0
                              ? _avctx->frame_size : 1024;
    const int ch = _cfg.channels;

    // 尾巴不足一整帧的部分：补零凑满，当作最后一帧发出
    if (_fifoUsed > 0 && _fifoUsed < frameSize) {
        int need = frameSize * ch;
        _fifoBuf.resize(need, 0);           // 尾部补零
        _fifoUsed = frameSize;
    }

    while (_fifoUsed >= frameSize) {
        const int need = frameSize * ch;
        const int16_t* data = _fifoBuf.data();

        AVFrame* s16Frame = av_frame_alloc();
        s16Frame->format      = AV_SAMPLE_FMT_S16;
        s16Frame->sample_rate = _cfg.sampleRate;
        s16Frame->ch_layout   = _avctx->ch_layout;
        s16Frame->nb_samples  = frameSize;
        av_frame_get_buffer(s16Frame, 0);
        memcpy(s16Frame->data[0], data, need * sizeof(int16_t));
        s16Frame->pts = _fifoBasePts;
        _fifoBasePts += frameSize;

        AVFrame* encFrame = s16Frame;
        if (_swr) {
            encFrame = av_frame_alloc();
            encFrame->format      = _codecSampleFmt;
            encFrame->sample_rate = _cfg.sampleRate;
            encFrame->ch_layout   = _avctx->ch_layout;
            encFrame->nb_samples  = frameSize;
            av_frame_get_buffer(encFrame, 0);
            encFrame->pts = s16Frame->pts;
            swr_convert(_swr,
                        encFrame->data, frameSize,
                        (const uint8_t**)s16Frame->data, frameSize);
        }

        avcodec_send_frame(_avctx, encFrame);
        av_frame_free(&encFrame);
        if (_swr) av_frame_free(&s16Frame);

        _fifoBuf.erase(_fifoBuf.begin(), _fifoBuf.begin() + need);
        _fifoUsed -= frameSize;
    }

    // 发送 null 帧触发编码器刷新缓冲
    int ret = avcodec_send_frame(_avctx, nullptr);
    if (ret < 0 && ret != AVERROR_EOF)
        return;

    while (true) {
        AVPacket* pkt = av_packet_alloc();
        if (!pkt) return;

        ret = avcodec_receive_packet(_avctx, pkt);
        if (ret == 0) {
            std::lock_guard<std::mutex> lock(_pkt_mtx);
            if (_pktQueue.size() >= kMaxPktQueued) {
                AVPacket* drop = _pktQueue.front();
                _pktQueue.pop();
                av_packet_free(&drop);
            }
            _pktQueue.push(pkt);
            _pktCond.notify_one();
            continue;
        }
        av_packet_free(&pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        return;
    }
}
