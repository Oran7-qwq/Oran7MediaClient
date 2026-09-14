#include "Oran7VideoEncoder.h"
// #include "GlobalHelper.h"

#ifdef _WIN32
#include <combaseapi.h>
#endif

Oran7VideoEncoder::Oran7VideoEncoder()
{

}

Oran7VideoEncoder::~Oran7VideoEncoder()
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
}

bool Oran7VideoEncoder::configure(const EncoderConfig& cfg,ErrorCallback onError)
{
    _onError  = std::move(onError);

    // 1. 查找编码器
    _codec = avcodec_find_encoder_by_name(cfg.codecName.c_str());
    if (!_codec) {
        if (_onError)
            _onError("Encoder not found: " + cfg.codecName);
        return false;
    }

    // 2. 分配编码器上下文
    _avctx = avcodec_alloc_context3(_codec);
    if (!_avctx) {
        if (_onError)
            _onError("avcodec_alloc_context3 failed");
        return false;
    }

    // 3. 通用基础参数
    _avctx->width     = cfg.width;
    _avctx->height    = cfg.height;
    _avctx->time_base = AVRational{1, 90000};
    _avctx->framerate = AVRational{cfg.fps, 1};
    _avctx->bit_rate  = (int64_t)cfg.bitrateKbps * 1000;

    // 4. 路径分流：只设路径相关的参数
    if (cfg.hwDevice) {
        // ---- 硬编 h264_mf（输入 CPU NV12，MF 内部走 GPU 管线）----
        _avctx->pix_fmt        = AV_PIX_FMT_NV12;
        _avctx->gop_size        = cfg.gopSize;
        _avctx->max_b_frames    = 0;
        _avctx->rc_max_rate     = _avctx->bit_rate;
        _avctx->rc_buffer_size  = _avctx->bit_rate / 2;  // 0.5 秒 VBV，够 IDR 不延时

        auto setOpt = [](AVCodecContext* ctx, const char* k, const char* v) {
            int r = av_opt_set(ctx->priv_data, k, v, 0);
            if (r < 0) WARNING_LOG << "[h264_mf] opt rejected: " << k << "=" << v;
        };
        setOpt(_avctx, "rate_control", "cbr");
        setOpt(_avctx, "scenario", "live_streaming");
    } else {
        // ---- 软编 libx264 ----
        _avctx->pix_fmt     = AV_PIX_FMT_YUV420P;
        _avctx->gop_size     = cfg.gopSize;
        _avctx->max_b_frames = 0;
        _avctx->thread_count = cfg.threadCount;
        av_opt_set(_avctx->priv_data, "preset",  cfg.preset.c_str(),  0);
        av_opt_set(_avctx->priv_data, "tune",    cfg.tune.c_str(),    0);
        av_opt_set(_avctx->priv_data, "profile", cfg.profile.c_str(), 0);
    }

    // 5. 暂存配置，avcodec_open2 延迟到编码线程执行（h264_mf 需要 MTA COM）
    _cfg = cfg;
    _configured = true;
    return true;
}

bool Oran7VideoEncoder::start()
{
    if (_running.exchange(true))
        return true;

    if (!_configured || !_avctx) {
        if (_onError)
            _onError("Oran7VideoEncoder::start(): not configured.");
        return false;
    }

    _frameIndex    = 0;
    _encode_thread = std::thread(&Oran7VideoEncoder::encodeLoop, this);
    return true;
}

bool Oran7VideoEncoder::stop()
{
    if (!_running.exchange(false))
        return false;              // 已经停了

    // 唤醒 encodeLoop 使其退出
    _cond.notify_all();
    _pktCond.notify_all();
    _pktSpaceCond.notify_all();

    if (_encode_thread.joinable())
        _encode_thread.join();

    return true;
}

void Oran7VideoEncoder::pushFrame(AVFrame *frame)
{
    if(!frame) return;
    std::lock_guard<std::mutex> lock(_frame_mtx);
    if(_frameQueue.size() >= _kMaxQueued)
    {
        auto* drop = _frameQueue.front();
        _frameQueue.pop();
        av_frame_free(&drop);//丢弃旧帧
    }
    //INFO_LOG<<"pushFrame:"<<"frame->pts:"<<frame->pts<<" frameFormat"<<frame->format;
    _frameQueue.push(frame);
    _cond.notify_one();
}

bool Oran7VideoEncoder::getPacket(AVPacket *pkt, int timeoutMs)
{
    std::unique_lock<std::mutex> lock(_pkt_mtx);
    if (!_pktCond.wait_for(lock,
           std::chrono::milliseconds(timeoutMs), [this]() {
               return !_pktQueue.empty() || !_running.load();
           }))
    {
        return false;  // 超时
    }
    if (_pktQueue.empty()) return false;

    AVPacket* front = _pktQueue.front();
    _pktQueue.pop();
    av_packet_move_ref(pkt, front);
    av_packet_free(&front);

    lock.unlock();
    _pktSpaceCond.notify_one();  // 编码线程可能正被阻塞等空间
    return true;
}

bool Oran7VideoEncoder::enqueuePacket(AVPacket* pkt)
{
    std::unique_lock<std::mutex> lock(_pkt_mtx);
    // 阻塞直到队列有空间或编码器被停止
    _pktSpaceCond.wait(lock, [this]() {
        return _pktQueue.size() < _kMaxPktQueued || !_running.load();
    });
    if (!_running.load()) {
        av_packet_free(&pkt);
        return false;
    }
    _pktQueue.push(pkt);
    lock.unlock();
    _pktCond.notify_one();
    return true;
}

/**
 * =======================================
 *  @note PRIVATE
 * =======================================
 */

 /**
  * @brief 编码器最外层循环
  */
void Oran7VideoEncoder::encodeLoop()
{
    // h264_mf 要求 COM MTA 模式，Qt 主线程是 STA
    // 编码线程独立初始化 COM
    bool comInited = false;
    if (_cfg.hwDevice) {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        comInited = SUCCEEDED(hr);
    }

    int ret = avcodec_open2(_avctx, _codec, nullptr);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        if (_onError)
            _onError("avcodec_open2 failed: " + std::string(errbuf));
        _running.store(false);
        if (comInited) CoUninitialize();
        return;
    }
    _opened.store(true);

    while (_running.load())
    {
        //INFO_LOG<<"VideoEncode Loop...";
        AVFrame *frame = nullptr;
        {
            std::unique_lock<std::mutex> lock(_frame_mtx);
            // 满足以下情况之一:
            // 1.帧队列不空,有数据进入了 -> 停止等待，继续编码
            // 2.编码线程被外部停止 -> 停止等待，直接退出
            _cond.wait(lock,[this](){
                return !_frameQueue.empty() || !_running.load(); 
            });
            if(!_running.load()) break;
            if(_frameQueue.empty()) continue;
            frame = _frameQueue.front();
            _frameQueue.pop();
        }
        // 送入AVCocde编码
        processFrame(frame);
        _frameIndex++;
        av_frame_free(&frame); //编码完成，清空该帧内存
    }
    flushFrames();
    if (comInited) CoUninitialize();
}

/**
 * @brief 编码内核处理，调用ffmpeg-api ;自动分配-硬编码/软编码
 *
 * - EAGAIN：编码器还需要再喂帧才有输出，正常退出 while 循环，encodeLoop会取下一帧继续
 *  - EOF：flush 阶段编码器吐完所有缓冲包后返回这个，正常退出
 *  - 其他 <0：真正的错误，记日志，退出循环
 *  - 0：有包，入队然后继续循环——因为 send_frame 一次可能触发多次receive_packet
 */
void Oran7VideoEncoder::processFrame(AVFrame *frame)
{
    // 响应 PLI 请求：强制编码器产 IDR
    if (_forceKeyframe.exchange(false)) {
        frame->pict_type = AV_PICTURE_TYPE_I;
        frame->key_frame = 1;
    }
    /*! @brief Begining avcodec send frame*/
    int ret = avcodec_send_frame(_avctx, frame);
    //INFO_LOG<<"avcodec_send_frame->ret:"<<ret;
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
        if (_onError)
            _onError("avcodec_send_frame failed: " +std::to_string(ret));
        return;
    }

    while (true)
    {
        AVPacket* pkt = av_packet_alloc();
        if (!pkt) {
            if (_onError)
                _onError("Oran7VideoEncoder::processFrame av_packet_alloc() failed.");
            return;
        }

        ret = avcodec_receive_packet(_avctx, pkt);
        //INFO_LOG<<"avcodec_receive_packet->ret:"<<ret;
        if (ret == 0)
        {
            // 拿包——阻塞入队，绝不丢弃编码后的 H264
            if (!enqueuePacket(pkt))
                break;  // 编码器已停
            continue;
        }

        if (ret == AVERROR(EAGAIN))
        {
            // 编码器需要更多输入帧，本轮结束
            av_packet_free(&pkt);
            break;
        }

        if (ret == AVERROR_EOF)
        {
            // 编码器已刷空
            av_packet_free(&pkt);
            break;
        }

        // ret < 0 通用错误
        av_packet_free(&pkt);
        if (_onError)_onError("avcodec_receive_packet error: " +std::to_string(ret));
        break;
    }
}

void Oran7VideoEncoder::flushFrames()
{
    /*! @note 发送 null 帧触发编码器刷新缓冲 */
    int ret = avcodec_send_frame(_avctx, nullptr);
    if (ret < 0 && ret != AVERROR_EOF)
        return;

    while (true) {
        AVPacket* pkt = av_packet_alloc();
        if (!pkt) return;

        ret = avcodec_receive_packet(_avctx, pkt);
        if (ret == 0) {
            if (!enqueuePacket(pkt))
                break;
            continue;
        }

        av_packet_free(&pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        return;
    }
}
