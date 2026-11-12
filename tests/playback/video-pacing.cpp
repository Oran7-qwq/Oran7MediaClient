#include "../../src/cpp/Oran7MediaPlayer/Oran7MediaPlayer.h"
#include "../../src/cpp/Oran7MediaPlayer/Oran7VideoRefreshScheduler.h"
#include "../../src/cpp/Oran7MediaPlayer/ffmsg.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>
#include <atomic>
#include <cstdio>
#include <cstdlib>

static void check(bool ok, const char* label)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", label);
        std::exit(1);
    }
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    SDL_SetMainReady();
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    using Scheduler = Oran7VideoRefreshScheduler;
    check(std::abs(Scheduler::remaining(1, 1 + 1.0 / 240) - 1.0 / 240) < 1e-8, "240fps waits less than 10ms");
    check(std::abs(Scheduler::remaining(1, 1.0003) - .0003) < 1e-8, "sub-ms deadline is not rounded to 1ms");
    check(Scheduler::remaining(2, 1) == 0 && Scheduler::remaining(1, 2) == .01, "late and idle waits bounded");
    QTemporaryDir directory;
    check(directory.isValid(), "temporary synthetic media directory");
    for (int scenario : {0, 1, 2})
    {
        const bool audio = scenario != 0;
        const bool vfr = scenario == 2;
        const auto path = directory.filePath(QString("synthetic-%1.mp4").arg(scenario));
        QStringList args{"-v", "error", "-y", "-f", "lavfi", "-i", "testsrc2=size=160x96:rate=240"};
        if (audio)
            args << "-f" << "lavfi" << "-i" << "sine=frequency=440:sample_rate=48000";
        args << "-t" << "2" << "-c:v" << "mpeg4" << "-threads" << "1" << "-q:v" << "5";
        if (vfr)
            args << "-vf" << "select=lt(mod(n*11\\,24)\\,11)" << "-fps_mode" << "vfr";
        if (audio)
            args << "-c:a" << "aac";
        else
            args << "-an";
        args << path;
        QProcess generator;
        generator.start(QStringLiteral(ORAN7_TEST_FFMPEG), args);
        check(generator.waitForFinished(15000) && generator.exitCode() == 0, "generate 240fps synthetic video");
        std::atomic<int> frames{0};
        std::atomic<bool> ended{false}, monotonic{true};
        std::atomic<double> lastPts{-1};
        auto player = std::make_unique<Oran7MediaPlayer>();
        check(player->oran7mp_create(
                  [&](void* raw)
                  {
                      auto* native = static_cast<Oran7MediaPlayer*>(raw);
                      AVMessage message;
                      while (native->oran7mp_get_msg(&message, 1) >= 0)
                      {
                          if (message.value == FFP_MSG_PREPARED)
                              native->oran7mp_start();
                          if (message.value == FFP_MSG_PLAY_FNISH)
                              ended = true;
                          msg_free_obj_res(&message);
                      }
                      return 0;
                  }) == 0,
              "create actual native playback core");
        player->AddVideoRefreshCallback(
            [&](const Frame* frame, AVFrame* owned)
            {
                if (frame->pts <= lastPts.load())
                    monotonic = false;
                lastPts = frame->pts;
                ++frames;
                av_frame_free(&owned);
                return 0;
            });
        player->oran7mp_set_data_source(path.toUtf8().constData());
        QElapsedTimer elapsed;
        elapsed.start();
        check(player->oran7mp_prepare_async() == 0, "prepare software video without GUI or GPU");
        while (!ended && elapsed.elapsed() < 6000)
        {
            QCoreApplication::processEvents();
            QThread::msleep(1);
        }
        const auto millis = elapsed.elapsed();
        player.reset();
        std::printf("source240fps audio=%d vfr=%d: presented=%d/%d elapsed=%lldms last_pts=%.4f\n", audio, vfr,
                    frames.load(), vfr ? 220 : 480, static_cast<long long>(millis), lastPts.load());
        check(ended && monotonic && lastPts > 1.95, "timeline completes in presentation order");
        check(millis >= 1700 && millis < 4000, "playback retains real-time duration");
        check(frames >= (vfr ? 180 : 390), "presentation follows real PTS rather than fixed 10ms or nominal rate");
    }
}
