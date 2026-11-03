// 与现有恢复场景共用生产控制器，只替换设备 I/O 和配置落盘。
static void asyncChecks(D3D11VideoItem& item, Oran7AppConfigManager& config)
{
    const auto reset = [&]
    {
        check(liveWorkers == 0, "prior capture workers fully released");
        outputs = {QVariantMap{{"outputIndex", 0}, {"attached", true}, {"deviceName", "DISPLAY-A"}}};
        auto saved = config.snapshot();
        saved.capture = {0, 60, true, false};
        saved.recording.fps = 60;
        config.updateConfig(saved);
        config.writes.clear();
        config.saveFails = false;
        mode = 0;
        starts = 0;
        maxWorkers = 0;
        destructorDelay = 0;
    };
    const auto ready = [](Oran7ScreenCaptureController& capture)
    { until([&] { return capture.previewStateName() == "Running"; }); };
    const auto prepare = [&](Oran7ScreenCaptureController& capture)
    {
        capture.setVideoItem(&item);
        capture.loadCaptureSettings();
        capture.findChild<QTimer*>("captureRecoveryDelay")->setInterval(10);
    };
    reset();
    {
        Oran7ScreenCaptureController capture;
        prepare(capture);
        mode = 4;
        capture.start();
        until([] { return starts == 1; });
        QSignalSpy errors(&capture, &Oran7ScreenCaptureController::errorOccurred);
        int ticks = 0;
        QTimer heartbeat;
        heartbeat.setInterval(5);
        QObject::connect(&heartbeat, &QTimer::timeout, [&] { ++ticks; });
        heartbeat.start();
        QElapsedTimer elapsed;
        elapsed.start();
        capture.stop();
        check(elapsed.elapsed() < 60 && capture.previewBusy(), "stop returns while slow driver is still running");
        mode = 0;
        capture.start();
        ready(capture);
        check(ticks >= 8 && errors.isEmpty(),
              "GUI advances and cancelled generation cannot deliver late error/first frame");
        check(starts == 2 && maxWorkers == 1,
              "start during stop waits for old worker release without overlapping sessions");
    }
    reset();
    {
        Oran7ScreenCaptureController capture;
        prepare(capture);
        capture.start();
        ready(capture);
        destructorDelay = 220;
        int ticks = 0;
        qint64 maxGap = 0;
        QElapsedTimer gap;
        gap.start();
        QTimer heartbeat;
        heartbeat.setInterval(5);
        QObject::connect(&heartbeat, &QTimer::timeout,
                         [&]
                         {
                             ++ticks;
                             maxGap = qMax(maxGap, gap.restart());
                         });
        heartbeat.start();
        check(capture.restartPreview(), "reconnect accepted while current session runs");
        ready(capture);
        check(ticks >= 10 && maxGap < 150, "worker deferred destruction/TLS cleanup never blocks GUI reaping");
        check(maxWorkers == 1, "new worker is created only after full native thread completion");
        destructorDelay = 0;
    }
    reset();
    {
        Oran7ScreenCaptureController capture;
        prepare(capture);
        capture.start();
        ready(capture);
        QSignalSpy results(&capture, &Oran7ScreenCaptureController::captureRequestFinished);
        QSignalSpy legacy(&capture, &Oran7ScreenCaptureController::captureApplyFinished);
        mode = 5;
        const auto first = capture.applyCaptureSettings(0, 120, true, false);
        until([] { return starts == 2; });
        QTest::qWait(20);
        check(capture.previewStateName() == "Starting" && legacy.isEmpty() && config.snapshot().capture.fps == 60,
              "graph started without a frame neither commits settings nor completes apply");
        mode = 0;
        const auto second = capture.applyCaptureSettings(0, 90, false, true);
        ready(capture);
        check(results.size() == 2 && results[0][0].toMap().value("superseded").toBool(),
              "replaced request gets a numbered terminal result");
        check(results[0][0].toMap().value("requestId") == first.value("requestId") &&
                  results[1][0].toMap().value("requestId") == second.value("requestId") &&
                  results[1][0].toMap().value("ok").toBool(),
              "only latest request succeeds under its own ID");
        check(legacy.size() == 1 && legacy[0][0].toBool(), "superseded legacy result cannot end latest apply");
        check(config.snapshot().capture.fps == 90 && config.writes.size() == 1 && config.writes[0].fps == 90,
              "only first-frame-confirmed latest settings persist");
        check(maxWorkers == 1, "continuous apply remains bounded to one live worker");
    }
    reset();
    {
        Oran7ScreenCaptureController capture;
        prepare(capture);
        capture.start();
        ready(capture);
        mode = 5;
        QSignalSpy results(&capture, &Oran7ScreenCaptureController::captureRequestFinished);
        capture.applyCaptureSettings(0, 120, true, false);
        until([] { return starts == 2; });
        capture.stop();
        until([&] { return capture.previewStateName() == "Idle"; });
        check(results.size() == 1 && !results[0][0].toMap().value("ok").toBool(),
              "explicit stop terminates outstanding apply");
        check(config.snapshot().capture.fps == 60 && config.writes.empty(), "cancelled settings do not leak to disk");
    }
    reset();
    {
        Oran7ScreenCaptureController capture;
        prepare(capture);
        capture.start();
        ready(capture);
        QSignalSpy results(&capture, &Oran7ScreenCaptureController::captureRequestFinished);
        mode = 6;
        capture.applyCaptureSettings(0, 120, true, false);
        until([&] { return results.size() == 1; });
        ready(capture);
        check(!results[0][0].toMap().value("ok").toBool() && starts == 3 && config.snapshot().capture.fps == 60,
              "failed candidate restores proven previous source without persisting candidate");
        mode = 7;
        capture.applyCaptureSettings(0, 120, true, false);
        until([&] { return capture.previewStateName() == "Error"; });
        check(results.size() == 2 && !capture.previewBusy() && starts == 5,
              "rollback failure ends in recoverable Error rather than infinite retry");
    }
    reset();
    {
        Oran7ScreenCaptureController capture;
        prepare(capture);
        capture.start();
        ready(capture);
        QSignalSpy results(&capture, &Oran7ScreenCaptureController::captureRequestFinished);
        config.saveFails = true;
        capture.applyCaptureSettings(0, 120, true, false);
        until([&] { return results.size() == 1; });
        ready(capture);
        check(!results[0][0].toMap().value("ok").toBool() && config.snapshot().capture.fps == 60,
              "save failure restores capture group and usable preview");
        config.saveFails = false;
    }
    reset();
    {
        Oran7ScreenCaptureController capture;
        prepare(capture);
        capture.start();
        ready(capture);
        mode = 4;
        QSignalSpy errors(&capture, &Oran7ScreenCaptureController::errorOccurred);
        QSignalSpy results(&capture, &Oran7ScreenCaptureController::captureRequestFinished);
        capture.applyCaptureSettings(0, 120, true, false);
        until([] { return starts == 2; });
        mode = 0;
        const auto latest = capture.applyCaptureSettings(0, 90, true, false);
        ready(capture);
        check(errors.isEmpty() && config.snapshot().capture.fps == 90 && results.size() == 2,
              "stale driver failure cannot rollback or reject newer settings");
        check(results[1][0].toMap().value("requestId") == latest.value("requestId") &&
                  results[1][0].toMap().value("ok").toBool(),
              "newer settings complete after old driver returns");
    }
    reset();
    {
        Oran7ScreenCaptureController capture;
        prepare(capture);
        capture.start();
        ready(capture);
        capture.findChild<QTimer*>("captureFirstFrameTimeout")->setInterval(30);
        mode = 5;
        QSignalSpy results(&capture, &Oran7ScreenCaptureController::captureRequestFinished);
        capture.applyCaptureSettings(0, 120, true, false);
        until([] { return starts == 2; });
        auto other = config.snapshot();
        other.recording.bitrateKbps = 50000;
        config.updateConfig(other);
        until([&] { return results.size() == 1; });
        mode = 0;
        ready(capture);
        check(!results[0][0].toMap().value("ok").toBool() && config.snapshot().capture.fps == 60,
              "pending apply timeout reports failure and restores prior settings");
        check(config.snapshot().recording.bitrateKbps == 50000,
              "rollback preserves unrelated config changes made during apply");
    }
    reset();
    {
        Oran7ScreenCaptureController capture;
        auto transient = std::make_unique<D3D11VideoItem>();
        capture.setVideoItem(transient.get());
        capture.loadCaptureSettings();
        capture.start();
        ready(capture);
        transient.reset();
        until([&] { return capture.previewStateName() == "Error"; });
        check(liveWorkers == 0, "destroyed rendering item retires capture safely");
    }
    reset();
    {
        Oran7ScreenCaptureController capture;
        prepare(capture);
        capture.findChild<QTimer*>("captureFirstFrameTimeout")->setInterval(25);
        mode = 5;
        capture.start();
        until([&] { return capture.previewStateName() == "Error"; });
        check(starts == 9 && !capture.previewBusy(), "graph-only startup times out with finite rebuild budget");
        mode = 0;
        check(capture.restartPreview(), "explicit retry resets exhausted recovery budget");
        ready(capture);
    }
    reset();
    {
        Oran7ScreenCaptureController capture;
        prepare(capture);
        mode = 4;
        capture.start();
        until([] { return starts == 1; });
        QSignalSpy errors(&capture, &Oran7ScreenCaptureController::errorOccurred);
        capture.shutdown();
        check(liveWorkers == 0 && capture.previewStateName() == "Idle",
              "shutdown synchronously releases slow worker before dependencies");
        capture.shutdown();
        QTest::qWait(30);
        check(!capture.start() && errors.isEmpty(), "shutdown is idempotent and rejects late callbacks/new starts");
    }
    reset();
}
