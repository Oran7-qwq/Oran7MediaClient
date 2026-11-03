#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlPropertyMap>
#include <QQuickWindow>
#include <QQuickItem>
#include <QElapsedTimer>
#include <QThread>
#include <QJSValue>
#include <QFontDatabase>
#include <QTest>
#include <cstdio>
#include <cstdlib>

static int checks = 0;
static void check(bool ok, const char* message) { if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); } ++checks; }
static void settle() { QElapsedTimer t; t.start(); while(t.elapsed() < 150) { QCoreApplication::processEvents(); QThread::msleep(2); } }
static QQuickItem* findItem(QQuickItem* item, const QString& name) {
    if (item->objectName() == name) return item;
    for (auto* child : item->childItems()) if (auto* found = findItem(child, name)) return found;
    return nullptr;
}
int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen"); qputenv("QT_QUICK_BACKEND", "software"); qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");
    QGuiApplication app(argc, argv);
    check(QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR", "C:/Windows") + "/Fonts/msyh.ttc") >= 0, "offscreen Chinese font loaded");
    QFont::insertSubstitution(QString::fromUtf8("微软雅黑"), "Microsoft YaHei");
    app.setFont(QFont("Microsoft YaHei"));
    QQmlPropertyMap theme;
    theme.insert("isDark",true);
    theme.insert("Primary", QVariantMap{{"durationMid", 100}, {"durationVerySlow", 100}, {"durationFast", 100}});
    theme.insert("Oran7MainGUI", QVariantMap{{"themeColor", "#beb7ff"}, {"isDarkMode", true}});
    theme.insert("Oran7ScreenCapture", QVariantMap{{"ScaleMode", "Fit"}});
    qmlRegisterSingletonInstance("Oran7UI.Impl", 1, 0, "Oran7Theme", &theme);
    qmlRegisterSingletonType(QUrl::fromLocalFile(QStringLiteral(APP_ROOT) + "/src/qml/Settings/GlobalSettings/Oran7MainUiSetting.qml"), "Oran7MainUiSetting", 1, 0, "Oran7MainUiSetting");
    const auto fakeClientId = qmlRegisterSingletonType(QUrl::fromLocalFile(QStringLiteral(APP_ROOT) + "/tests/recording-workspace/FakeClient.qml"), "Client", 1, 0, "Client");
    QQmlApplicationEngine engine;
    QStringList warnings;
    QObject::connect(&engine, &QQmlEngine::warnings, [&](const QList<QQmlError>& list) { for (const auto& error : list) { warnings << error.toString(); std::fprintf(stderr, "%s\n", qPrintable(error.toString())); } });
    engine.load(QUrl::fromLocalFile(QStringLiteral(APP_ROOT) + "/tests/recording-workspace/Fixture.qml"));
    check(!engine.rootObjects().isEmpty(), "workspace loads");
    settle();
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    check(window, "offscreen window");
    auto* workspace = findItem(window->contentItem(), "workspace");
    auto* source = findItem(workspace, "recordingSourcePanel");
    auto* output = findItem(workspace, "recordingOutputPanel");
    auto* preview = findItem(workspace, "recordingPreviewPanel");
    auto* center = findItem(workspace, "recordingCenterColumn");
    auto* toolbar = findItem(workspace, "recordingCenterToolbar");
    check(source && output && preview, "three panels instantiated");
    check(center && toolbar && source->x() < center->x() && center->x() < output->x() && preview->width() >= 320, "wide studio layout");
    check(toolbar->parentItem() == preview->parentItem() && toolbar->y() >= preview->y() + preview->height()
        && toolbar->width() == preview->width(), "toolbar belongs directly beneath center preview");
    auto* previewBadge = findItem(preview, "recordingPreviewBadge");
    auto* previewTools = findItem(preview, "recordingPreviewTools");
    check(previewBadge->mapToItem(preview, QPointF(previewBadge->width(), 0)).x() <= preview->width() - 6,
        "preview status badge remains inside the frame");
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/wide.png");
    auto* pageState = workspace->property("pageState").value<QObject*>();
    auto draft = [&](const char* name) { return pageState->property(name).value<QJSValue>().toVariant().toMap(); };
    auto click = [&](const char* name) {
        auto* button = findItem(workspace, name);
        check(button && QMetaObject::invokeMethod(button, "clicked"), name);
        settle();
    };
    auto edit = [&](const char* name, const QString& value) {
        auto* field = findItem(workspace, name);
        field->setProperty("text", value); QMetaObject::invokeMethod(field, "textEdited"); settle();
    };
    check(!findItem(workspace, "recordingAdvancedGroup")->isVisible(), "advanced diagnostics initially collapsed");
    // clicked 信号的调用不模拟 Button 自己的 checkable 切换，显式设置后检查内容展开。
    findItem(workspace, "recordingAdvancedToggle")->setProperty("checked", true); settle();
    check(findItem(workspace, "recordingAdvancedGroup")->isVisible(), "advanced diagnostics can expand");
    findItem(workspace, "recordingAdvancedToggle")->setProperty("checked", false);
    check(!findItem(workspace, "captureFpsChoicesOption0")->isEnabled(), "capture rate below recording cap is disabled");
    click("captureFpsChoicesOption2");
    check(draft("captureDraft").value("fps").toInt() == 120 && !findItem(workspace, "captureCustomFps")->isVisible(), "capture shortcut edits existing draft");
    click("captureFpsChoicesOption3");
    check(findItem(workspace, "captureCustomFps")->isVisible() && draft("captureDraft").value("fps").toInt() == 120, "custom capture rate expands without changing value");
    edit("captureCustomFps", "75");
    check(draft("captureDraft").value("fps").toInt() == 75, "custom capture frame rate retained");
    edit("captureCustomFps", "");
    QVariant applyResult;
    QMetaObject::invokeMethod(pageState, "applyCapture", Q_RETURN_ARG(QVariant, applyResult));
    check(!applyResult.toBool() && !pageState->property("applying").toBool(), "empty custom capture rate is rejected before applying");
    click("captureFpsChoicesOption1");
    check(!findItem(workspace, "captureCustomFps")->isVisible(), "capture preset collapses custom input");
    auto* sizeChoice = findItem(workspace, "recordingSizePreset");
    QMetaObject::invokeMethod(sizeChoice, "activated", Q_ARG(int, 1)); settle();
    check(draft("outputDraft").value("width").toInt() == 1920 && draft("outputDraft").value("height").toInt() == 1080
        && sizeChoice->property("currentIndex").toInt() == 1 && !findItem(workspace, "recordingCustomSize")->isVisible(), "1080p shortcut submits both dimensions and synchronizes selection");
    QMetaObject::invokeMethod(sizeChoice, "activated", Q_ARG(int, 3)); settle();
    check(findItem(workspace, "recordingCustomSize")->isVisible(), "custom dimensions expand for matching preset values");
    QMetaObject::invokeMethod(sizeChoice, "activated", Q_ARG(int, 0)); settle();
    check(draft("outputDraft").value("width").toInt() == 0 && draft("outputDraft").value("height").toInt() == 0, "original size preserves backend zero convention");
    click("outputFpsChoicesOption2"); edit("recordingFps", "50");
    check(draft("outputDraft").value("fps").toInt() == 50 && findItem(workspace, "recordingFps")->isVisible(), "nonstandard output rate stays editable");
    click("outputFpsChoicesOption1");
    check(draft("outputDraft").value("fps").toInt() == 60 && !findItem(workspace, "recordingFps")->isVisible(), "output rate preset restores compact view");
    auto* bitrateChoice = findItem(workspace, "recordingBitratePreset");
    check(bitrateChoice->property("currentIndex").toInt() == 4 && !findItem(workspace, "recordingBitrate")->isVisible(), "existing 40 Mbps selects matching tier without rewriting settings");
    auto* bitratePopup = bitrateChoice->property("popup").value<QObject*>();
    QMetaObject::invokeMethod(bitratePopup, "open"); settle();
    check(bitratePopup->property("visible").toBool() && bitratePopup->property("width").toReal() >= 350, "bitrate popup opens with room for title and description");
    auto popupFits = [&] {
        auto* content = qobject_cast<QQuickItem*>(bitratePopup->property("contentItem").value<QObject*>());
        const auto topLeft = content->mapToScene(QPointF());
        const auto bottomRight = content->mapToScene(QPointF(content->width(), content->height()));
        return topLeft.x() >= 0 && topLeft.y() >= 0 && bottomRight.x() <= window->width() && bottomRight.y() <= window->height();
    };
    check(popupFits(), "expanded bitrate descriptions stay inside wide window");
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/picker.png");
    QMetaObject::invokeMethod(bitratePopup, "close");
    QMetaObject::invokeMethod(bitrateChoice, "activated", Q_ARG(int, 1)); settle();
    check(draft("outputDraft").value("bitrateKbps").toInt() == 8000
        && workspace->property("bitrateStorageEstimate").toString().contains("3.6 GB"), "8 Mbps writes integer kbps and estimates decimal GB per hour");
    QMetaObject::invokeMethod(bitrateChoice, "activated", Q_ARG(int, 6)); settle();
    check(findItem(workspace, "recordingBitrate")->isVisible() && draft("outputDraft").value("bitrateKbps").toInt() == 8000, "custom tier reveals current value without overwriting draft");
    edit("recordingBitrate", "0.1");
    check(workspace->property("bitrateStorageEstimate").toString().contains("45 MB"), "low bitrate estimate avoids rounding to zero GB");
    edit("recordingBitrate", "12.345");
    check(draft("outputDraft").value("bitrateKbps").toInt() == 12345, "Mbps input converts exactly to existing integer kbps");
    workspace->setProperty("customBitrate", false); settle();
    check(bitrateChoice->property("currentIndex").toInt() == 6 && findItem(workspace, "recordingBitrate")->isVisible(), "nonstandard saved bitrate automatically uses custom tier");
    QMetaObject::invokeMethod(bitrateChoice, "activated", Q_ARG(int, 4)); settle();
    check(draft("outputDraft").value("bitrateKbps").toInt() == 40000 && !findItem(workspace, "recordingBitrate")->isVisible(), "choosing preset restores compact bitrate control");
    const QString longPath = "C:/very-long-recordings-directory/session/archive";
    edit("recordingDirectory", longPath);
    check(draft("outputDraft").value("outputDirectory").toString() == longPath
        && findItem(workspace, "recordingDirectoryDisplay")->property("elide").toInt() == Qt::ElideMiddle, "path remains complete in draft with middle elision in display");
    workspace->setProperty("customCaptureFps", true); workspace->setProperty("customOutputFps", true); workspace->setProperty("customOutputSize", true);
    findItem(workspace, "recordingAdvancedToggle")->setProperty("checked", true); settle();
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/custom.png");
    findItem(workspace, "recordingAdvancedToggle")->setProperty("checked", false);
    QMetaObject::invokeMethod(pageState, "reset");
    workspace->setProperty("customCaptureFps", false); workspace->setProperty("customOutputFps", false); workspace->setProperty("customOutputSize", false); workspace->setProperty("customBitrate", false);
    settle();
    // 实际打开下拉列表，覆盖委托创建及未连接显示器项，而不只检查静态布局。
    auto* monitorChoice = findItem(workspace, "recordingMonitor");
    check(monitorChoice->property("displayText").toString() == QString::fromUtf8("显示器 5：X7IQ"), "monitor name appends hardware model while retaining original output index");
    const auto monitors = pageState->property("monitors").value<QJSValue>().toVariant().toList();
    check(monitors.last().toMap().value("name").toString() == QString::fromUtf8("显示器 8"), "missing hardware model keeps plain display label");
    auto* monitorPopup = monitorChoice->property("popup").value<QObject*>();
    QMetaObject::invokeMethod(monitorPopup, "open"); settle();
    check(monitorPopup->property("visible").toBool(), "monitor picker opens with detail delegates");
    QMetaObject::invokeMethod(monitorPopup, "close");
    window->setHeight(520); settle();
    check(!findItem(workspace, "applyCaptureSettings") && !findItem(workspace, "saveRecordingOutput"), "manual apply/save buttons removed");
    for (const auto& pair : {qMakePair(source, "restartCapturePreview")}) {
        auto* action = findItem(workspace, pair.second);
        const qreal beforeY = action->mapToItem(pair.first, QPointF()).y();
        auto* panelScroll = findItem(pair.first, pair.first->objectName() + "Scroll");
        auto* scrolling = panelScroll->property("contentItem").value<QObject*>();
        scrolling->setProperty("contentY", qMax(0.0, scrolling->property("contentHeight").toReal() - scrolling->property("height").toReal())); settle();
        check(qAbs(action->mapToItem(pair.first, QPointF()).y() - beforeY) < 0.1
            && beforeY + action->height() <= pair.first->height(), "footer action remains fixed while settings scroll");
        scrolling->setProperty("contentY", 0);
    }
    window->setHeight(780); settle();
    window->setWidth(850); window->setHeight(680); settle();
    QMetaObject::invokeMethod(bitratePopup, "open"); settle();
    check(popupFits(), "bitrate popup repositions within narrow and shorter window");
    QMetaObject::invokeMethod(bitratePopup, "close"); settle();
    check(source->y() == center->y() && center->y() == output->y()
        && source->x() < center->x() && center->x() < output->x(), "medium layout keeps tools beside preview");
    check(toolbar->width() == preview->width() && toolbar->y() >= preview->height()
        && findItem(toolbar, "stopLocalRecording")->mapToItem(toolbar, QPointF(0,0)).x()
            + findItem(toolbar, "stopLocalRecording")->width() <= toolbar->width(), "compact center toolbar keeps actions inside its bounds");
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/medium.png");
    window->setWidth(560); settle();
    check(source->y() == center->y() && center->y() == output->y()
        && source->x() < center->x() && center->x() < output->x()
        && source->width() == 230 && output->width() == 260 && preview->width() >= 280, "small layout preserves three columns and usable widths");
    auto* scroll = findItem(workspace, "recordingWorkspaceScroll");
    auto* studio = findItem(workspace, "recordingStudioGrid");
    check(scroll && studio && studio->width() > scroll->width(), "narrow workspace scrolls horizontally");
    auto* flickable = scroll->property("contentItem").value<QObject*>();
    flickable->setProperty("contentX", studio->width() - scroll->width()); settle();
    check(flickable->property("contentX").toReal() > 0
        && output->mapToItem(workspace, QPointF(output->width(), 0)).x() <= workspace->width(), "right toolbar reachable by scrolling");
    window->setWidth(1320); settle();
    QVariant result;
    QVariant frameRateResult;
    check(QMetaObject::invokeMethod(window, "runFrameRateChecks", Q_RETURN_ARG(QVariant, frameRateResult)) && frameRateResult.toInt() == 9, "cross-panel frame-rate save/apply/start checks");
    check(QMetaObject::invokeMethod(window, "runChecks", Q_RETURN_ARG(QVariant, result)), "page interaction checks execute");
    check(result.toInt() == 24, "24 source/save/start/rollback/reconnect checks");
    auto* fake = window->property("fake").value<QObject*>();
    check(fake, "fixture controller");
    auto* microphone=findItem(toolbar,"recordingMicrophone");
    auto* microphoneVolume=findItem(toolbar,"recordingMicrophoneVolume");
    auto* systemVolume=findItem(toolbar,"recordingSystemVolume");
    check(microphone && microphoneVolume && systemVolume && !microphoneVolume->isEnabled(), "audio volume and microphone UI exists with microphone initially off");
    auto* systemToggle=findItem(toolbar,"recordingAudio");
    if (!systemToggle->property("checked").toBool()) {
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,systemToggle->mapToScene(QPointF(systemToggle->width()/2,systemToggle->height()/2)).toPoint());
        settle();settle();
    }
    const auto originalOptions=fake->property("options").value<QJSValue>().toVariant();
    const int originalSaves=fake->property("saves").toInt();
    QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,microphone->mapToScene(QPointF(microphone->width()/2,microphone->height()/2)).toPoint());
    settle();
    check(toolbar->property("microphoneEnabled").toBool() && microphoneVolume->isEnabled(), "microphone UI toggle enables its volume control");
    microphoneVolume->forceActiveFocus();QTest::keyClick(window,Qt::Key_Left);
    systemVolume->forceActiveFocus();QTest::keyClick(window,Qt::Key_Left);settle();
    check(toolbar->property("microphoneVolume").toDouble()<100 && toolbar->property("systemVolume").toDouble()<100, "audio sliders edit their UI values");
    check(fake->property("saves").toInt()==originalSaves && fake->property("options").value<QJSValue>().toVariant()==originalOptions, "UI-only audio controls do not change backend recording settings");
    auto stableGeometry=[&] {
        auto* controls=findItem(toolbar,"recordingControlRow");
        auto* status=findItem(toolbar,"recordingStatusRow");
        check(toolbar->height()<=90 && controls && status && status->y()>=controls->y()+controls->height(), "toolbar is compact control row plus one status row");
        auto centerY=[&](const char* name) { auto* item=findItem(toolbar,name); return item->mapToItem(toolbar,QPointF(0,item->height()/2)).y(); };
        check(qAbs(centerY("recordingSystemAudioTool")-centerY("recordingMicrophoneTool"))<1 && qAbs(centerY("recordingSystemAudioTool")-centerY("startLocalRecording"))<2, "audio and recording actions stay on one line at all widths");
        check(qAbs(centerY("recordingStatus")-centerY("recordingOutputLocation"))<2, "status and output path share one footer line");

        auto snapshot=[&] { return QVariantList{preview->y(),preview->height(),toolbar->y(),toolbar->height(),findItem(toolbar,"startLocalRecording")->width(),findItem(toolbar,"stopLocalRecording")->width()}; };
        check(previewBadge->mapToItem(preview, QPointF(previewBadge->width(), 0)).x() <= preview->width() - 6
            && previewTools->mapToItem(preview, QPointF()).x() >= 6,
            "preview badge and floating tools remain within narrow frame");
        const auto before=snapshot();
        const auto previousMessage=pageState->property("message");
        for (const auto* phase : {"Preparing","Recording","Stopping","Error","Idle"}) {
            fake->setProperty("recordingState",phase);
            fake->setProperty("recordingBusy",QString(phase)!="Idle" && QString(phase)!="Error");
            fake->setProperty("recordingOutputPath",QString(phase)=="Preparing"?QString():QString("C:/Videos/very-long-folder/recording-with-long-file-name.mp4"));
            pageState->setProperty("message",QString::fromUtf8("很长的状态提示不会挤压预览画面，也不会因为文件路径出现而改变占位。"));
            settle();
            check(snapshot()==before,"preview and toolbar geometry stays fixed across recording states and output-path appearance");
        }
        fake->setProperty("recordingOutputPath",QString());
        pageState->setProperty("message",previousMessage);settle();
    };
    stableGeometry();
    window->setWidth(850);settle();stableGeometry();
    auto* audioTools=findItem(toolbar,"recordingAudioTools");
    auto* audioFlick=audioTools->property("contentItem").value<QObject*>();
    check(findItem(toolbar,"recordingSystemAudioTool")->width()==220 && findItem(toolbar,"recordingMicrophoneTool")->width()==220 && audioFlick->property("contentWidth").toReal()>audioFlick->property("width").toReal(), "audio modules keep compact widths and scroll in narrow preview column");
    audioFlick->setProperty("contentX",audioFlick->property("contentWidth").toReal()-audioFlick->property("width").toReal());settle();
    check(microphone->mapToItem(audioTools,QPointF(microphone->width(),0)).x()<=audioTools->width(), "microphone tools remain reachable by horizontal scrolling");
    window->setWidth(1320);settle();
    fake->setProperty("recordingBusy", true); fake->setProperty("recordingState", "Recording"); settle();
    check(findItem(workspace, "recordingStatus")->property("text").toString() == QString::fromUtf8("正在录制"), "recording status overrides old settings messages");
    check(!findItem(workspace, "startLocalRecording")->isEnabled() && !findItem(workspace, "recordingDirectory")->isEnabled()
        && !findItem(workspace, "recordingAudio")->isEnabled() && !findItem(workspace, "restartCapturePreview")->isEnabled(), "actual controls locked while busy");
    check(!microphone->isEnabled() && !microphoneVolume->isEnabled() && !systemVolume->isEnabled(), "new audio UI locks during recording");
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/recording.png");
    auto* stop = findItem(workspace, "stopLocalRecording");
    check(stop->isEnabled() && QMetaObject::invokeMethod(stop, "clicked"), "actual stop button submits request");
    settle(); check(!stop->isEnabled() && !findItem(workspace, "recordingFilePrefix")->isEnabled(), "stopping keeps controls locked");
    fake->setProperty("recordingBusy", false); fake->setProperty("recordingState", "Idle"); settle();
    auto* widthField = findItem(workspace, "recordingWidth");
    widthField->setProperty("text", ""); check(QMetaObject::invokeMethod(widthField, "textEdited"), "dimension field editing");
    settle(); check(widthField->property("text").toString().isEmpty(), "empty dimension remains editable for backend validation");
    widthField->setProperty("text", "1920"); QMetaObject::invokeMethod(widthField, "textEdited");
    auto* heightField = findItem(workspace, "recordingHeight"); heightField->setProperty("text", "1080"); QMetaObject::invokeMethod(heightField, "textEdited");
    check(QMetaObject::invokeMethod(heightField, "editingFinished"), "field completion requests automatic save");
    settle(); settle();
    check(fake->property("options").value<QJSValue>().toVariant().toMap().value("width").toInt() == 1920, "actual field edits aggregate into saved settings");
    for (int step = 0; step <= 14; ++step) {
        QVariant outcome;
        check(QMetaObject::invokeMethod(window, "runAutoStep", Q_RETURN_ARG(QVariant, outcome), Q_ARG(QVariant, step)) && outcome.toBool(), "automatic save/apply asynchronous workflow");
        settle(); settle();
    }
    workspace->setProperty("customOutputFps", true); settle();
    auto* fpsField = findItem(workspace, "recordingFps");
    window->requestActivate(); fpsField->forceActiveFocus(); settle();
    QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
    QTest::keyClick(window, Qt::Key_2); QTest::keyClick(window, Qt::Key_5);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, fpsField->mapToScene(QPointF(12, 12)).toPoint());
    check(fpsField->hasActiveFocus(), "clicking inside FPS field keeps editing focus");
    const auto previewClick=preview->mapToScene(QPointF(preview->width()/2, preview->height()/2)).toPoint();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, previewClick); QTest::qWait(40);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, previewClick);
    settle(); settle();
    check(!fpsField->hasActiveFocus() && fake->property("options").value<QJSValue>().toVariant().toMap().value("fps").toInt() == 25, "clicking preview clears FPS focus and automatically saves without Enter");
    check(!pageState->property("outputDirty").toBool() && findItem(workspace, "recordingOutputPanelStatus")->property("text").toString() == QString::fromUtf8("已保存"), "focus-loss save clears pending status after real keyboard editing");
    auto typeFps = [&](const QString& digits) {
        fpsField->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
        for (const auto digit : digits) QTest::keyClick(window, static_cast<Qt::Key>(Qt::Key_0 + digit.digitValue()));
    };
    const int savesBeforeTyping=fake->property("saves").toInt();
    typeFps("24"); QTest::qWait(1000);
    check(fake->property("saves").toInt()==savesBeforeTyping, "typing does not save before idle deadline");
    typeFps("26"); QTest::qWait(1400);
    check(fake->property("saves").toInt()==savesBeforeTyping, "new keystrokes restart two-second idle deadline");
    QTest::qWait(1000); settle();
    check(fpsField->hasActiveFocus() && fake->property("options").value<QJSValue>().toVariant().toMap().value("fps").toInt()==26 && !pageState->property("outputDirty").toBool(), "two seconds idle saves final value and status without Enter or focus loss");
    typeFps("27"); QTest::keyClick(window, Qt::Key_Return); settle(); settle();
    check(fake->property("options").value<QJSValue>().toVariant().toMap().value("fps").toInt()==27, "Enter still commits immediately");
    auto* fpsPreset = findItem(workspace, "outputFpsChoicesOption0");
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, fpsPreset->mapToScene(QPointF(fpsPreset->width()/2, fpsPreset->height()/2)).toPoint());
    settle(); settle();
    check(fake->property("options").value<QJSValue>().toVariant().toMap().value("fps").toInt() == 30, "outside-click observer preserves preset button clicks");
    for (const auto* name : {"recordingPreviewInfo", "recordingPreviewScale"}) {
        auto* button=findItem(workspace,name);
        const bool previous=button->property("checked").toBool();
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,button->mapToScene(QPointF(button->width()/2,button->height()/2)).toPoint());
        settle();
        check(button->property("checked").toBool()!=previous,"styled preview button toggles through actual mouse input");
    }
    QVariant requestChecks;
    check(QMetaObject::invokeMethod(window, "runRequestIdChecks", Q_RETURN_ARG(QVariant, requestChecks)) && requestChecks.toInt() == 8,
        "request IDs, stale replies, pending drafts and preview busy use production page state");
    engine.load(QUrl::fromLocalFile(QStringLiteral(APP_ROOT) + "/tests/recording-workspace/PageFixture.qml"));
    settle(); check(engine.rootObjects().size() == 2, "production page and preview load with fake service");
    auto* client = engine.singletonInstance<QObject*>(fakeClientId);
    check(client && client->property("attempts").toInt() > 0 && client->property("previewStarts").toInt() == 0, "failed render attachment does not start preview");
    client->setProperty("attachAllowed", true);
    QElapsedTimer retry; retry.start(); while (client->property("previewStarts").toInt() == 0 && retry.elapsed() < 2000) { QCoreApplication::processEvents(); QThread::msleep(2); }
    check(client->property("previewStarts").toInt() == 1, "retry starts preview after successful attachment");
    check(client->property("scale").toInt() == 1, "page initializes Fit without opening global settings");
    const auto originalPanelColor = source->property("color");
    const auto originalTextColor = workspace->property("textColor");
    auto* settings = engine.singletonInstance<QObject*>(qmlTypeId("Oran7MainUiSetting", 1, 0, "Oran7MainUiSetting"));
    check(settings, "dropdown style singleton available");
    settings->setProperty("itemHeight", 48);
    settings->setProperty("textPixelSize", 24);
    settings->setProperty("fontFamily", "Times New Roman");
    theme.insert("Oran7MainGUI", QVariantMap{{"themeColor", "#00ff00"}, {"isDarkMode", false}});
    theme.insert("isDark",false);
    settle();
    check(source->property("color") == originalPanelColor && toolbar->property("color") == originalPanelColor
        && workspace->property("textColor") == originalTextColor && workspace->property("textPixelSize").toInt() == 16,
        "dropdown palette/fonts do not leak into workspace");
    workspace->setProperty("panelColor", QColor("#224466"));
    workspace->setProperty("textColor", QColor("#fff0aa"));
    workspace->setProperty("accentColor", QColor("#ff8844"));
    settle();
    check(source->property("color").value<QColor>() == QColor("#224466")
        && toolbar->property("color").value<QColor>() == QColor("#224466")
        && toolbar->property("textColor").value<QColor>() == QColor("#fff0aa"), "workspace properties style panels and toolbar");
    auto* audio = findItem(toolbar, "recordingAudio");
    check(audio->property("onColor").value<QColor>() == QColor("#ff8844")
        && audio->height() == 20, "reused switch uses injected workspace colors and sizing");
    settle();
    check(warnings.isEmpty(), "no QML warnings or binding loops");
    std::printf("PASS: %d layout/runtime checks + %d recording workflow checks\n", checks, result.toInt());
}
