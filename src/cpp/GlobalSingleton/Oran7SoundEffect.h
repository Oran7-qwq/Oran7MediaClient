#ifndef ORAN7SOUNDEFFECT_H
#define ORAN7SOUNDEFFECT_H

#include <QObject>
#include <QAudioSink>
#include <QAudioFormat>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QFile>
#include <QBuffer>
#include <QByteArray>

#include "Oran7Definitions.h"

class Oran7SoundEffect : public QObject
{
    Q_OBJECT

    //<--- in SettingWindow ---
    SOUND_EFFECT(playOpen,":/sound/OpenSound.wav");

    SOUND_EFFECT(playClose,":/sound/CloseSound.wav");

    SOUND_EFFECT(expandItem,":/sound/OpenSound.wav");

public:
    explicit Oran7SoundEffect(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)

    qreal volume() const { return m_volume; }
    void setVolume(qreal v) {
        if (qFuzzyCompare(m_volume, v)) return;
        m_volume = qBound(0.0, v, 1.0);
        emit volumeChanged();
    }

signals:
    void volumeChanged();

private:
    // 解析 WAV 文件，提取 PCM 数据和音频格式
    QByteArray loadWav(const QString &path, QAudioFormat &fmt)
    {
        QByteArray pcm;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return pcm;

        // 跳过 RIFF header (12 bytes: "RIFF" + size + "WAVE")
        if (f.read(12).size() != 12)
            return pcm;

        // 遍历子 chunk
        forever
        {
            char id[4];
            if (f.read(id, 4) != 4) break;
            quint32 size = 0;
            if (f.read(reinterpret_cast<char *>(&size), 4) != 4) break;

            if (qstrncmp(id, "fmt ", 4) == 0)
            {
                quint16 audioFmt = 0, channels = 0, bits = 0;
                quint32 sampleRate = 0;
                f.read(reinterpret_cast<char *>(&audioFmt), 2);
                f.read(reinterpret_cast<char *>(&channels), 2);
                f.read(reinterpret_cast<char *>(&sampleRate), 4);
                f.seek(f.pos() + 6); // byteRate + blockAlign
                f.read(reinterpret_cast<char *>(&bits), 2);
                if (size > 16)
                    f.seek(f.pos() + (size - 16));

                if (audioFmt != 1) // 仅支持 PCM
                    return pcm;

                fmt.setSampleRate(sampleRate);
                fmt.setChannelCount(channels);
                fmt.setSampleFormat(bits == 16 ? QAudioFormat::Int16
                                               : QAudioFormat::UInt8);
            }
            else if (qstrncmp(id, "data", 4) == 0)
            {
                pcm = f.read(size);
                break;
            }
            else
            {
                f.seek(f.pos() + size);
            }
        }
        return pcm;
    }

    // 每次播放时用当前默认设备创建新 QAudioSink，用 QBuffer 让 sink 自己按需读取
    void playPcm(const QByteArray &pcm, const QAudioFormat &fmt)
    {
        if (pcm.isEmpty())
            return;

        QAudioDevice dev = QMediaDevices::defaultAudioOutput();
        QAudioSink *sink = new QAudioSink(dev, fmt, this);
        sink->setVolume(m_volume);

        // QBuffer 作为 sink 的数据源，sink 会按缓冲区大小按需读取
        QBuffer *buffer = new QBuffer(sink);
        buffer->setData(pcm);
        buffer->open(QIODevice::ReadOnly);

        connect(sink, &QAudioSink::stateChanged, this, [sink](QAudio::State st) {
            if (st == QAudio::IdleState || st == QAudio::StoppedState)
                sink->deleteLater(); // buffer 作为 sink 的子对象也会被一起销毁
        });

        sink->start(buffer);
    }

    qreal m_volume = 1.0;
};

#endif // ORAN7SOUNDEFFECT_H
