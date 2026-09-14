#ifndef ORAN7MP4MUXER_H
#define ORAN7MP4MUXER_H

#include "Oran7VideoEncoder.h"
#include "Oran7AudioEncoder.h"

extern "C"{
#include <libavformat/avformat.h>
#include <libavcodec/bsf.h>
}

class Oran7Mp4Muxer
{
public:
    Oran7Mp4Muxer();
    ~Oran7Mp4Muxer();

    bool open(const char* filePath, int width, int height,
              AVRational frameRate,
              std::shared_ptr<Oran7VideoEncoder>& _video_encoder,
              std::shared_ptr<Oran7AudioEncoder>& audio_encoder);
    void close();
    void drainRemaining();
    bool isRunning(){return this->_running.load();}

    using ErrorCallback = std::function<void(const std::string&)>;
    void setErrorCallback(ErrorCallback cb) { _onError =std::move(cb); }
private:
    void muxLoop();
    void flush();

    std::atomic<bool> _running{false};
    std::thread _thread;
    bool _opened = false;

    std::shared_ptr<Oran7VideoEncoder> _video_encoder;
    std::shared_ptr<Oran7AudioEncoder> _audio_encoder;

    AVFormatContext* _muxFmtCtx = nullptr;
    AVStream* _videoStream = nullptr;
    AVStream* _audioStream = nullptr;

    int64_t _writeIndex = 0;
    ErrorCallback _onError;
};

#endif // ORAN7MP4MUXER_H
