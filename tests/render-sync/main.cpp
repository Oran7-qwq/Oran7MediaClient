#include "D3d11Videoitem.h"
#include "Oran7CaptureTextureSlot.h"
#include <QGuiApplication>
#include <QQuickGraphicsDevice>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlContext>
#include <QSignalSpy>
#include <QTest>
#include <rhi/qrhi.h>
#include <thread>
#include <atomic>
#include <cstdio>
#include <cstdlib>

static int checks = 0;
static void check(bool ok, const char* label)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", label);
        std::exit(1);
    }
    ++checks;
}
static AVFrame* cpuFrame(int width, int height, QColor color)
{
    auto* frame = av_frame_alloc();
    check(frame, "allocate synthetic frame");
    frame->format = AV_PIX_FMT_BGRA;
    frame->width = width;
    frame->height = height;
    check(av_frame_get_buffer(frame, 32) >= 0, "synthetic frame buffer");
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            auto* pixel = frame->data[0] + y * frame->linesize[0] + x * 4;
            pixel[0] = color.blue();
            pixel[1] = color.green();
            pixel[2] = color.red();
            pixel[3] = 255;
        }
    return frame;
}
static void mailboxChecks()
{
    Oran7VideoFrameMailbox mailbox;
    mailbox.submit(cpuFrame(8, 8, Qt::red));
    mailbox.submit(cpuFrame(16, 12, Qt::blue));
    auto latest = mailbox.take();
    check(latest.frame && latest.frame->width == 16 && !mailbox.take().frame, "latest frame consumed exactly once");
    Oran7VideoInfo info;
    info.srcSize = QSize(16, 12);
    info.decodeDevice = "synthetic";
    check(mailbox.releaseSerial() == 0, "media submission does not request resource retirement");
    mailbox.clear();
    check(mailbox.releaseSerial() == 1, "clear independently marks retained resources for frame-end release");
    mailbox.publish(latest.epoch, info);
    check(!mailbox.drain().infoChanged, "old frame information rejected after clear");
    check(mailbox.take().clear, "clear command survives without media frame");
    const auto current = mailbox.take().epoch;
    mailbox.publish(current, info);
    const auto delivery = mailbox.drain();
    check(delivery.infoChanged && delivery.sizeChanged && delivery.info.decodeDevice == "synthetic" &&
              delivery.sourceSize == delivery.info.srcSize,
          "whole info snapshot and source size agree");
    auto slot = std::make_shared<Oran7CaptureTextureSlot>();
    auto* leased = oran7MakeCaptureFrame(slot, 8, 8);
    check(leased && slot.use_count() == 2, "capture frame retains texture slot lease");
    auto* clone = av_frame_clone(leased);
    av_frame_free(&leased);
    check(slot.use_count() == 2, "cloned frame prevents pool slot overwrite");
    av_frame_free(&clone);
    check(slot.use_count() == 1, "last frame release makes pool slot reusable");
    auto item = std::make_unique<D3D11VideoItem>();
    auto submit = item->frameSubmitter();
    item.reset();
    submit(cpuFrame(4, 4, Qt::red));
    QCoreApplication::processEvents();
    check(true, "late producer safely drops frame after item destruction");
    std::thread producer(
        [&]
        {
            for (int n = 0; n < 2000; ++n)
            {
                auto* frame = av_frame_alloc();
                frame->pts = n;
                mailbox.submit(frame);
            }
        });
    qint64 last = -1;
    for (int n = 0; n < 2000; ++n)
    {
        auto packet = mailbox.take();
        if (packet.frame)
        {
            check(packet.frame->pts > last, "concurrent consumption remains monotonic");
            last = packet.frame->pts;
        }
    }
    producer.join();
    auto final = mailbox.take();
    if (final.frame)
        check(final.frame->pts > last, "final concurrent frame remains monotonic");
    mailbox.close();
    mailbox.submit(av_frame_alloc());
    check(!mailbox.take().frame, "closed mailbox rejects late submission");
}
int main(int argc, char** argv)
{
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString& message)
                           { std::fprintf(stderr, "%s\n", qPrintable(message)); });
    // 仅使用 WARP 软件设备及重定向纹理，不创建可见窗口、不访问桌面采集或硬件 GPU。
    qputenv("QT_QUICK_BACKEND", "rhi");
    qputenv("QSG_RHI_BACKEND", "d3d11");
    QQuickWindow::setSceneGraphBackend("rhi");
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    QGuiApplication app(argc, argv);
    mailboxChecks();
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    check(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr,
                                      0, D3D11_SDK_VERSION, &device, nullptr, &context)),
          "create isolated WARP software device");
    QQuickRenderControl control;
    QQuickWindow window(&control);
    window.setGraphicsDevice(QQuickGraphicsDevice::fromDeviceAndContext(device.Get(), context.Get()));
    window.setGeometry(0, 0, 128, 96);
    window.setColor(Qt::black);
    window.contentItem()->setSize(QSizeF(128, 96));
    check(control.initialize(), "initialize actual Qt Quick QRhi on WARP");
    auto* rhi = control.rhi();
    check(rhi && rhi->backend() == QRhi::D3D11, "actual D3D11 QRhi backend");
    std::unique_ptr<QRhiTexture> output(rhi->newTexture(QRhiTexture::RGBA8, QSize(128, 96), 1,
                                                        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
    check(output->create(), "create redirected output texture");
    std::unique_ptr<QRhiRenderBuffer> depth(rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, QSize(128, 96), 1));
    check(depth->create(), "create stencil target");
    QRhiTextureRenderTargetDescription description{QRhiColorAttachment(output.get())};
    description.setDepthStencilBuffer(depth.get());
    std::unique_ptr<QRhiTextureRenderTarget> target(rhi->newTextureRenderTarget(description));
    std::unique_ptr<QRhiRenderPassDescriptor> pass(target->newCompatibleRenderPassDescriptor());
    target->setRenderPassDescriptor(pass.get());
    check(target->create(), "create redirected render target");
    window.setRenderTarget(QQuickRenderTarget::fromRhiRenderTarget(target.get()));
    auto item = std::make_unique<D3D11VideoItem>();
    item->setParentItem(window.contentItem());
    item->setSize(QSizeF(80, 60));
    item->setPosition(QPointF(24, 18));
    item->setContentRect(QRectF(0, 0, 80, 60));
    QSignalSpy sizes(item.get(), &D3D11VideoItem::sourceSizeChanged);
    QSignalSpy info(item.get(), &D3D11VideoItem::sendVideoFrameInfo);
    const auto render = [&]
    {
        QCoreApplication::processEvents();
        control.polishItems();
        control.beginFrame();
        control.sync();
        control.render();
        QRhiReadbackResult readback;
        auto* updates = rhi->nextResourceUpdateBatch();
        updates->readBackTexture(QRhiReadbackDescription(output.get()), &readback);
        control.commandBuffer()->resourceUpdate(updates);
        control.endFrame();
        rhi->finish();
        check(readback.pixelSize == QSize(128, 96) && !readback.data.isEmpty(), "actual QRhi output readback");
        return QImage(reinterpret_cast<const uchar*>(readback.data.constData()), 128, 96, QImage::Format_RGBA8888)
            .copy();
    };
    item->submitFrame(cpuFrame(16, 12, Qt::red));
    auto image = render();
    image.save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/qrhi-red.png");
    check(image.pixelColor(64, 48).red() > 245 && image.pixelColor(5, 5).red() < 5,
          "software frame draws at transformed item position via QRhi");
    QCoreApplication::processEvents();
    check(sizes.count() == 1 && info.count() == 1, "prepared frame publishes first complete GUI snapshot");
    image.save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/qrhi-red.png");
    // opacity、Fit 留边与场景裁剪由真实 QRhi 输出验证。
    item->setOpacity(0.5);
    image = render();
    check(image.pixelColor(64, 48).red() > 110 && image.pixelColor(64, 48).red() < 145,
          "QRhi shader respects inherited opacity");
    item->setOpacity(1);
    item->setContentRect(QRectF(0, 10, 80, 40));
    image = render();
    check(image.pixelColor(64, 22).red() < 5 && image.pixelColor(64, 48).red() > 245, "Fit content geometry preserved");
    item->renderBlackFrame();
    image = render();
    image.save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/qrhi-clear.png");
    check(image.pixelColor(64, 48).red() < 5, "stop clears queued frame and GPU target");
    image = render();
    check(image.pixelColor(64, 48).red() < 5, "frame-end resource retirement preserves black on subsequent frames");
    item->setVisible(false);
    item->renderBlackFrame();
    render();
    item->setVisible(true);
    item->submitFrame(cpuFrame(32, 24, Qt::blue));
    image = render();
    check(image.pixelColor(64, 48).blue() > 245, "resume with new source dimensions rebuilds texture bindings");
    image.save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/qrhi-blue.png");
    // 离屏 ShaderEffectSource 会让同一个节点在主画面/纹理 pass 被多次准备和绘制。
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("videoSource", item.get());
    QQmlComponent effect(&engine);
    effect.setData("import QtQuick\nShaderEffectSource { width: 40; height: 30; sourceItem: videoSource; live: true }",
                   QUrl());
    std::unique_ptr<QObject> effectObject(effect.create());
    check(bool(effectObject), "create real offscreen texture consumer");
    auto* effectItem = qobject_cast<QQuickItem*>(effectObject.get());
    effectItem->setParentItem(window.contentItem());
    item->submitFrame(cpuFrame(48, 36, Qt::green));
    image = render();
    check(image.pixelColor(64, 48).green() > 110 && image.pixelColor(10, 10).green() > 110,
          "main and offscreen passes share latest prepared image with independent matrices");
    image.save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/qrhi-multipass.png");
    // 首个 pass 消费完成时立即提交下一帧，验证同一 Qt 帧的第二个 pass 不会抢走新帧。
    auto* firstPassFrame = cpuFrame(48, 36, Qt::green);
    auto deferred = std::make_unique<std::function<void()>>(
        [submit = item->frameSubmitter(), next = cpuFrame(48, 36, Qt::blue)] { submit(next); });
    firstPassFrame->buf[1] = av_buffer_create(
        nullptr, 0,
        [](void* opaque, uint8_t*)
        {
            std::unique_ptr<std::function<void()>> publish(static_cast<std::function<void()>*>(opaque));
            (*publish)();
        },
        deferred.get(), 0);
    check(firstPassFrame->buf[1], "between-pass producer fixture");
    deferred.release();
    item->submitFrame(firstPassFrame);
    image = render();
    check(image.pixelColor(64, 48).green() > 110 && image.pixelColor(10, 10).green() > 110,
          "late submission between passes cannot split the visible Qt frame");
    image = render();
    check(image.pixelColor(64, 48).blue() > 245 && image.pixelColor(10, 10).blue() > 245,
          "between-pass submission is consumed on next Qt frame");
    effectItem->setParentItem(nullptr);
    effectObject.reset();
    for (int n = 0; n < 12; ++n)
    {
        const auto color = n % 2 ? QColor(Qt::red) : QColor(Qt::blue);
        item->submitFrame(cpuFrame(48, 36, color));
        image = render();
        check(image.pixelColor(64, 48) == color, "same-size consecutive frames refresh imported QRhi texture");
    }
    // 第二个 WARP 设备模拟采集设备；真实跨设备 keyed mutex 与共享纹理互操作。
    ComPtr<ID3D11Device> captureDevice;
    ComPtr<ID3D11DeviceContext> captureContext;
    check(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr,
                                      0, D3D11_SDK_VERSION, &captureDevice, nullptr, &captureContext)),
          "second isolated WARP device");
    auto captureSlot = std::make_shared<Oran7CaptureTextureSlot>();
    D3D11_TEXTURE2D_DESC captureDesc{};
    captureDesc.Width = 16;
    captureDesc.Height = 12;
    captureDesc.MipLevels = 1;
    captureDesc.ArraySize = 1;
    captureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    captureDesc.SampleDesc.Count = 1;
    captureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    captureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
    check(SUCCEEDED(captureDevice->CreateTexture2D(&captureDesc, nullptr, &captureSlot->texture)) &&
              SUCCEEDED(captureSlot->texture.As(&captureSlot->mutex)),
          "keyed shared capture texture");
    check(captureSlot->mutex->AcquireSync(0, 0) == S_OK, "producer acquires shared surface");
    auto source = Oran7VideoFrame(cpuFrame(16, 12, Qt::cyan));
    captureContext->UpdateSubresource(captureSlot->texture.Get(), 0, nullptr, source->data[0], source->linesize[0], 0);
    captureContext->Flush();
    check(captureSlot->mutex->ReleaseSync(0) == S_OK, "producer releases shared surface");
    bool cyan = false;
    for (int n = 0; n < 20 && !cyan; ++n)
    {
        item->submitFrame(oran7MakeCaptureFrame(captureSlot, 16, 12));
        image = render();
        cyan = image.pixelColor(64, 48) == QColor(Qt::cyan);
        if (!cyan)
            QTest::qWait(2);
    }
    check(cyan, "actual shared capture texture opens on Qt device and draws via QRhi");
    check(captureSlot.use_count() == 1, "rendered preview releases CPU slot lease");
    check(captureSlot->mutex->AcquireSync(0, 20) == S_OK, "producer can reuse consumed slot");
    item->submitFrame(oran7MakeCaptureFrame(captureSlot, 16, 12));
    QElapsedTimer busy;
    busy.start();
    image = render();
    check(busy.elapsed() < 1000 && image.pixelColor(64, 48) == QColor(Qt::cyan),
          "busy shared texture is dropped without blocking render or corrupting prior frame");
    captureSlot->mutex->ReleaseSync(0);
    item->setParentItem(nullptr);
    render();
    item->setParentItem(window.contentItem());
    item->submitFrame(cpuFrame(16, 12, Qt::yellow));
    image = render();
    check(image.pixelColor(64, 48) == QColor(Qt::yellow), "item reattachment recreates render node safely");
    QQuickItem clipped(window.contentItem());
    clipped.setSize(QSizeF(40, 96));
    clipped.setClip(true);
    item->setParentItem(&clipped);
    image = render();
    check(image.pixelColor(30, 48) == QColor(Qt::yellow) && image.pixelColor(64, 48) == QColor(Qt::black),
          "Qt scenegraph scissor clips QRhi video output");
    auto* provider = D3D11DeviceProvider::instance();
    provider->attachWindow(&window);
    check(provider->isReady(), "late device subscription initializes from existing scenegraph");
    {
        QQuickWindow other;
        provider->attachWindow(&other);
        check(!provider->isReady(), "new uninitialized host clears old device");
        QMetaObject::invokeMethod(&window, "sceneGraphInitialized", Qt::DirectConnection);
        check(!provider->isReady(), "old host signal cannot replace current host device");
        provider->attachWindow(&window);
    }
    check(provider->isReady(), "destroying previous host preserves current device");
    QMetaObject::invokeMethod(&window, "sceneGraphInvalidated", Qt::DirectConnection);
    check(!provider->isReady(), "invalidated scenegraph clears device");
    QMetaObject::invokeMethod(&window, "sceneGraphInitialized", Qt::DirectConnection);
    check(provider->isReady(), "same host scenegraph reinitialization restores device subscription");
    item->setParentItem(nullptr);
    item.reset();
    window.setRenderTarget({});
    control.invalidate();
    target.reset();
    pass.reset();
    depth.reset();
    output.reset();
    std::printf("PASS: %d render synchronization checks\n", checks);
}
