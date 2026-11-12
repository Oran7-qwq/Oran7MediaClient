#pragma once
#include <QString>
extern "C"
{
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

// 保留帧的编码色彩信息，直到 GPU 完成传递函数、色域和输出映射。
struct Oran7VideoColorSpace
{
    int bitDepth = 8;
    int transfer = 0; // 0: SDR，1: PQ，2: HLG
    bool bt2020 = false;
    bool fullRange = false;
    float kr = 0.2126f;
    float kb = 0.0722f;
    float peakNits = 1000.0f;
    QString transferName;
    QString primariesName;
    QString matrixName;
    static Oran7VideoColorSpace fromFrame(const AVFrame* frame, AVPixelFormat storageFormat);
};
