#include "Oran7AudioEncoder.h"
#include "Live/Oran7LiveAudioMixer.h"
#include <cmath>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <vector>
#include <cstdio>
#include <cstdlib>
static void check(bool ok, const char* message)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    for (const auto* name : {"aac", "libfdk_aac"})
    {
        if (!avcodec_find_encoder_by_name(name))
            continue;
        Oran7AudioEncoder audio;
        Oran7AudioEncoder::EncoderConfig c;
        c.codecName = name;
        c.continuousPts = true;
        c.maxFrameQueue = 64;
        c.maxPacketQueue = 100;
        std::atomic_int errors{0};
        check(audio.configure(c, [&](const auto&) { ++errors; }) && audio.start(), "configure live audio");
        QElapsedTimer t;
        t.start();
        while (!audio.isOpened() && t.elapsed() < 2000)
            QThread::msleep(2);
        check(audio.isOpened(), "AAC opens");
        auto* decoder = avcodec_alloc_context3(avcodec_find_decoder(AV_CODEC_ID_AAC));
        auto* parameters = avcodec_parameters_alloc();
        check(decoder && parameters && avcodec_parameters_from_context(parameters,audio.codecContext())>=0 &&
              avcodec_parameters_to_context(decoder,parameters)>=0 && avcodec_open2(decoder,avcodec_find_decoder(AV_CODEC_ID_AAC),nullptr)>=0,
              "open real AAC decoder for mixed audio roundtrip");
        avcodec_parameters_free(&parameters);
        auto* decoded = av_frame_alloc();
        double decodedEnergy=0;int decodedSamples=0;
        std::vector<int64_t> pts;
        Oran7LiveAudioMixer mixer;
        auto drain = [&]
        {
            AVPacket p{};
            while (audio.getPacket(&p, 0))
            {
                pts.push_back(p.pts);
                check(avcodec_send_packet(decoder,&p)>=0,"decode actual mixed AAC packet");
                while(avcodec_receive_frame(decoder,decoded)==0) {
                    check(decoded->format==AV_SAMPLE_FMT_FLTP && decoded->sample_rate==48000 && decoded->ch_layout.nb_channels==2,
                          "AAC carries expected stereo output format");
                    auto* pcm=reinterpret_cast<const float*>(decoded->data[0]);
                    for(int n=0;n<decoded->nb_samples;++n) {
                        check(std::isfinite(pcm[n]) && std::abs(pcm[n])<.95f,"decoded mix has no NaN or overload noise");
                        decodedEnergy+=pcm[n]*pcm[n];++decodedSamples;
                    }
                    av_frame_unref(decoded);
                }
                av_packet_unref(&p);
            }
        };
        for (int i = 0; i < 120; ++i)
        {
            auto* frame = av_frame_alloc();
            frame->format = AV_SAMPLE_FMT_S16;
            frame->sample_rate = 48000;
            av_channel_layout_default(&frame->ch_layout, 2);
            frame->nb_samples = 480;
            // 每次 FIFO 清空可能遇到采集回调抖动，不能把它重新注入 AAC 时间轴。
            frame->pts = i * 480 + (i / 32 % 2 ? 4800 : 0);
            check(av_frame_get_buffer(frame, 0) >= 0, "allocate synthetic PCM");
            auto* pcm=reinterpret_cast<int16_t*>(frame->data[0]);
            for(int lane=0;lane<2;++lane) {
                for(int n=0;n<480;++n)pcm[n*2]=pcm[n*2+1]=int16_t(8000*std::sin(6.283185307179586*(lane?500:1000)*(i*480+n)/48000.0));
                frame->pts=i*480;
                check(mixer.push(lane,frame),"synthetic microphone and system feed the production mixer");
            }
            av_frame_free(&frame);
            frame=mixer.render({true,true,100,50});
            frame->pts=i*480+(i/32%2?4800:0);
            audio.pushFrame(frame);
            if (i < 10 || i >= 40)
                drain(); // 300ms 不消费编码包，模拟网络消费者暂停。
            QThread::msleep(10);
        }
        QThread::msleep(30);
        drain();
        audio.stop();
        drain();
        check(errors.load() == 0 && audio.droppedInputFrames() == 0 && audio.droppedOutputPackets() == 0,
              "jitter and short sender stall do not drop live audio");
        check(pts.size() >= 55, "full audio duration preserved");
        for (size_t i = 1; i < pts.size(); ++i)
            check(pts[i] - pts[i - 1] == 1024, "AAC PTS advances by exactly one encoded sample block");
        check(decodedSamples>55000 && std::sqrt(decodedEnergy/decodedSamples)>.12 && std::sqrt(decodedEnergy/decodedSamples)<.23,
              "mixed tones retain duration and audible amplitude through real AAC encode/decode");
        av_frame_free(&decoded);avcodec_free_context(&decoder);
    }
    std::puts("PASS: real AAC/FDK sample clock and 300ms consumer stall");
}
