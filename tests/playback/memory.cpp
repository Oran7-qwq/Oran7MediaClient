#include "../../src/cpp/Oran7MediaPlayer/Oran7MediaPlayer.h"
#include "../../src/cpp/Oran7MediaPlayer/Oran7AudioSpectrumTap.h"
#include "../../src/cpp/Oran7MediaPlayer/ffmsg.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QElapsedTimer>
#include <QThread>
#include <QtEndian>
#include <windows.h>
#include <psapi.h>
#include <atomic>
#include <cmath>
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
static SIZE_T privateBytes()
{
    PROCESS_MEMORY_COUNTERS_EX counters{};
    check(GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                               sizeof(counters)),
          "query isolated test process memory");
    return counters.PrivateUsage;
}
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    SDL_SetMainReady();
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    QTemporaryDir dir;
    const auto fileName = dir.filePath("synthetic.wav");
    QByteArray wav(44 + 384000, '\0');
    wav.replace(0, 4, "RIFF");
    wav.replace(8, 8, "WAVEfmt ");
    wav.replace(36, 4, "data");
    auto put16 = [&](int at, quint16 n) { qToLittleEndian(n, wav.data() + at); };
    auto put32 = [&](int at, quint32 n) { qToLittleEndian(n, wav.data() + at); };
    put32(4, wav.size() - 8);
    put32(16, 16);
    put16(20, 1);
    put16(22, 2);
    put32(24, 48000);
    put32(28, 192000);
    put16(32, 4);
    put16(34, 16);
    put32(40, wav.size() - 44);
    for (int frame = 0; frame < (wav.size() - 44) / 4; ++frame) {
        const auto sample = qint16(16000 * std::sin(6.283185307179586 * 1000 * frame / 48000));
        put16(44 + frame * 4, quint16(sample));
        put16(46 + frame * 4, quint16(sample));
    }
    QFile file(fileName);
    check(file.open(QIODevice::WriteOnly) && file.write(wav) == wav.size(), "write synthetic WAV");
    file.close();
    SIZE_T baseline = 0, peak = 0;
    DWORD baseHandles = 0;
    for (int cycle = 0; cycle < 32; ++cycle)
    {
        std::atomic<bool> prepared{false};
        auto player = std::make_unique<Oran7MediaPlayer>();
        check(player->oran7mp_create(
                  [&](void* raw)
                  {
                      auto* native = static_cast<Oran7MediaPlayer*>(raw);
                      AVMessage message;
                      while (native->oran7mp_get_msg(&message, 1) >= 0)
                      {
                          if (message.value == FFP_MSG_PREPARED)
                          {
                              native->oran7mp_start();
                              prepared = true;
                          }
                          msg_free_obj_res(&message);
                      }
                      return 0;
                  }) == 0,
              "create native player");
        player->oran7mp_set_data_source(fileName.toUtf8().constData());
        auto tap = cycle == 0 ? std::make_shared<Oran7AudioSpectrumTap>() : nullptr;
        if (tap) {
            tap->setActive(true);
            player->oran7mp_setAudioSpectrumTap(tap);
        }
        player->oran7mp_set_playback_rate(1.25); // 强制走 Sonic，验证变速器退出也释放。
        check(player->oran7mp_prepare_async() == 0, "open native decoder");
        QElapsedTimer timer;
        timer.start();
        while (!prepared.load() && timer.elapsed() < 4000)
        {
            QCoreApplication::processEvents();
            QThread::msleep(1);
        }
        check(prepared.load(), "prepare actual FFmpeg audio decoder");
        QThread::msleep(100); // 仅隔离检查中的 dummy SDL 回调，不打开真实音频设备。
        if (tap) {
            bool signal = false;
            Oran7AudioSpectrumTap::Block block;
            QElapsedTimer capture; capture.start();
            while (!signal && capture.elapsed() < 1500) {
                while (tap->pop(block)) {
                    check(block.rate == 48000 && block.epoch == tap->epoch(), "native SDL format and generation");
                    for (int index = 0; index < block.count; ++index)
                        signal |= std::abs(block.samples[index]) > .05f;
                }
                if (!signal) QThread::msleep(5);
            }
            check(signal, "native SDL callback publishes the device PCM while playing");
            tap->setActive(false);
        }
        if (cycle < 4) {
            check(player->oran7mp_pause()==0,"pause submits without waiting for output fade on calling thread");
            QElapsedTimer pauseWait;pauseWait.start();
            while(player->oran7mp_get_state()!=MP_STATE_PAUSED && pauseWait.elapsed()<700) QThread::msleep(2);
            check(player->oran7mp_get_state()==MP_STATE_PAUSED,"pause completes after the audio tail");
            check(player->oran7mp_start()==0,"resume submits after fade to silence");
            QThread::msleep(100);
            check(player->oran7mp_get_state()==MP_STATE_STARTED,"resume restarts audio after fade request");
        }
        player.reset();
        QCoreApplication::processEvents();
        check(SDL_WasInit(SDL_INIT_AUDIO | SDL_INIT_TIMER) == 0, "each player returns its SDL subsystem references");
        const auto memory = privateBytes();
        if (cycle == 7)
        {
            baseline = memory;
            GetProcessHandleCount(GetCurrentProcess(), &baseHandles);
        }
        if (cycle >= 8)
            peak = qMax(peak, memory);
    }
    DWORD handles = 0;
    GetProcessHandleCount(GetCurrentProcess(), &handles);
    std::printf("32 switch/retire cycles: baseline %.2f MiB, peak %.2f MiB, final %.2f MiB, handles %lu -> %lu\n",
                baseline / 1048576.0, peak / 1048576.0, privateBytes() / 1048576.0, baseHandles, handles);
    check(peak <= baseline + 16 * 1024 * 1024, "audio switching memory remains bounded after warm-up");
    check(handles <= baseHandles + 8, "retired players do not accumulate OS handles");
}
