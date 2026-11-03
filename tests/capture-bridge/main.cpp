#include "Oran7ScreenCaptureBridge.h"
#include "Oran7ScreenCapture.h"
#include <QGuiApplication>
#include <QQuickItem>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <cstdio>
#include <cstdlib>

static int checks = 0;
static void check(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
    ++checks;
}
class Oran7Facade : public QObject {
    Q_OBJECT
public:
    Oran7ScreenCaptureBridge& bridge;
    explicit Oran7Facade(Oran7ScreenCaptureBridge& value) : bridge(value) {}
    Q_INVOKABLE QObject* screenCapture() const { return bridge.controller(); }
};

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication application(argc, argv);
    QQuickItem firstHost, secondHost, videoItem;
    QQuickItem* lastHost = nullptr;
    QObject* renderedItem = &videoItem;
    int attachCalls = 0;
    bool attachSucceeds = true;
    auto bridge = std::make_unique<Oran7ScreenCaptureBridge>(Oran7ScreenCaptureBridge::Oran7RenderHooks{
        [&](QQuickItem* host) { ++attachCalls; lastHost = host; return attachSucceeds; },
        [&] { return renderedItem; }});
    QPointer<QObject> controller = bridge->controller();
    auto* native = qobject_cast<Oran7ScreenCaptureController*>(controller.data());
    auto stats = captures.back();
    check(native && !native->parent(), "控制器由 unique_ptr 独占，无重复 parent 所有权");
    check(QQmlEngine::objectOwnership(native) == QQmlEngine::CppOwnership, "返回 QML 的控制器显式由 C++ 持有");
    int errors = 0, applies = 0;
    bool lastApplyOk = true;
    QObject::connect(bridge.get(), &Oran7ScreenCaptureBridge::errorOccurred, [&](const QString& error) { if (!error.isEmpty()) ++errors; });
    QObject::connect(bridge.get(), &Oran7ScreenCaptureBridge::captureApplyFinished, [&](bool ok, const QString&) { ++applies; lastApplyOk = ok; });
    check(!native->start() && errors == 0, "初始错误通过队列转发，不同步重入 Client");
    QCoreApplication::processEvents();
    check(errors == 1, "P0 错误信号链路完整");
    check(!bridge->attachVideoItem(nullptr) && attachCalls == 0, "空宿主不修改渲染绑定");
    attachSucceeds = false;
    check(!bridge->attachVideoItem(&firstHost) && stats->binds == 0, "渲染附着失败不绑定控制器");
    attachSucceeds = true;
    renderedItem = nullptr;
    check(!bridge->attachVideoItem(&firstHost) && stats->binds == 0, "缺失实际渲染项时安全返回");
    renderedItem = &videoItem;
    check(bridge->attachVideoItem(&firstHost) && lastHost == &firstHost && stats->item == &videoItem, "绑定渲染桥实际持有的项");
    native->start();
    const auto beforeReparent = stats->stops;
    check(bridge->attachVideoItem(&secondHost) && lastHost == &secondHost && stats->binds == 1 && stats->stops == beforeReparent, "同渲染项换视觉宿主不停止预览");
    check(bridge->monitors().first().toMap().value("outputIndex") == 3, "显示器原始序号原样透传");
    check(bridge->settings().value("fps") == 60, "参数快照原样透传");
    bridge->loadSettings();
    check(stats->loads == 1, "启动配置由原控制器加载");
    const auto accepted = bridge->applySettings(7, 144, false, true);
    check(accepted.value("ok").toBool() && stats->settings.value("fps") == 144 && stats->settings.value("outputIndex") == 7 && !stats->settings.value("drawMouse").toBool(), "参数和值返回结构原样透传");
    check(applies == 0, "应用完成信号保持异步");
    QCoreApplication::processEvents();
    check(applies == 1 && lastApplyOk, "同步原控制器结果经队列可靠转发");
    emit native->captureApplyFinished(false, "已回滚");
    QCoreApplication::processEvents();
    check(applies == 2 && !lastApplyOk, "失败回滚结果原样转发");
    native->startRecording();
    check(bridge->recordingBusy(), "录制/推流忙状态委托原控制器");
    check(!bridge->applySettings(0, 60, true, false).value("ok").toBool(), "录制中修改由原策略拒绝");
    native->stopRecording();
    check(!bridge->recordingBusy(), "停止录制后忙状态更新");
    {
        QQmlEngine engine;
        Oran7Facade facade(*bridge);
        engine.rootContext()->setContextProperty("Client", &facade);
        QQmlComponent component(&engine);
        component.setData(R"(
import QtQml
QtObject {
    property var capture: Client.screenCapture()
    function preview() { return capture.start() }
    function record() { return capture.startRecording() }
    function finish() { capture.stopRecording(); return capture.isRecording() }
})", QUrl("qrc:/CaptureBridgeCheck.qml"));
        std::unique_ptr<QObject> root(component.create());
        check(root && !component.isError(), "真实 QML 可接收原 QObject 控制器句柄");
        QVariant returned;
        check(QMetaObject::invokeMethod(root.get(), "preview", Q_RETURN_ARG(QVariant, returned)) && returned.toBool(), "旧 QML 预览 start 调用保持可用");
        check(QMetaObject::invokeMethod(root.get(), "record", Q_RETURN_ARG(QVariant, returned)) && returned.toBool() && bridge->recordingBusy(), "旧 QML 录制 startRecording 调用保持可用");
        check(QMetaObject::invokeMethod(root.get(), "finish", Q_RETURN_ARG(QVariant, returned)) && !returned.toBool(), "旧 QML 停止/状态调用保持可用");
        root.reset();
        engine.collectGarbage();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        check(controller && !stats->destroyed && QQmlEngine::objectOwnership(controller) == QQmlEngine::CppOwnership, "页面销毁及 GC 不删除 unique_ptr 所有的控制器");
    }
    emit native->captureApplyFinished(true, "迟到应用结果");
    emit native->errorOccurred("迟到错误");
    const auto errorsBeforeShutdown = errors, appliesBeforeShutdown = applies;
    const auto stopCalls = stats->stops;
    bridge->shutdown();
    bridge->shutdown();
    QCoreApplication::processEvents();
    check(stats->stops == stopCalls + 1 && !stats->running, "shutdown 幂等停止采集");
    check(errors == errorsBeforeShutdown && applies == appliesBeforeShutdown, "关闭后丢弃排队的旧结果");
    const auto writes = stats->applies;
    check(!bridge->applySettings(0, 30, true, false).value("ok").toBool() && stats->applies == writes, "关闭后桥拒绝新参数写入");
    check(!bridge->attachVideoItem(&firstHost), "关闭后不再绑定新宿主");
    bridge->loadSettings();
    check(stats->loads == 1 && bridge->controller() == controller, "关闭保留兼容只读句柄，不重建控制器");
    bridge.reset();
    check(!controller && stats->destroyed && stats->renderAliveAtDestruction, "控制器先于渲染项释放且 QPointer 自动清空");
    {
        Oran7ScreenCaptureBridge missing({});
        check(!missing.attachVideoItem(&firstHost), "缺少渲染钩子安全返回");
        check(missing.controller() != controller.data(), "不同桥实例独立创建控制器");
        emit qobject_cast<Oran7ScreenCaptureController*>(missing.controller())->captureApplyFinished(true, "排队销毁");
    }
    QCoreApplication::processEvents();
    for (const auto& capture : captures) check(capture->destroyed, "所有测试控制器释放");
    std::printf("PASS: %d capture bridge checks, %zu controllers released\n", checks, captures.size());
}
#include "main.moc"
