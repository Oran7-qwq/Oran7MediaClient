#include "D3d11Videoitem.h"
#include "Oran7HdrCompositor.h"
#include "Oran7VideoColorSpace.h"
#include <QGuiApplication>
#include <QQuickGraphicsDevice>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QSignalSpy>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <qfloat16.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
extern "C"
{
#include <libavutil/mastering_display_metadata.h>
}

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
static int pqCode(float nits)
{
    const double y = std::pow(nits / 10000.0, 0.1593017578125);
    return int(std::round(1023 * std::pow((0.8359375 + 18.8515625 * y) / (1 + 18.6875 * y), 78.84375)));
}
static AVFrame* rgb10(int r, int g, int b, AVColorTransferCharacteristic transfer = AVCOL_TRC_SMPTE2084)
{
    auto* frame = av_frame_alloc();
    frame->width = 32;
    frame->height = 16;
    frame->format = AV_PIX_FMT_GBRP10LE;
    frame->color_primaries = AVCOL_PRI_BT2020;
    frame->color_trc = transfer;
    frame->colorspace = AVCOL_SPC_RGB;
    frame->color_range = AVCOL_RANGE_JPEG;
    check(av_frame_get_buffer(frame, 32) >= 0, "allocate RGB 10-bit fixture");
    const int value[3] = {g, b, r};
    for (int p = 0; p < 3; ++p)
        for (int y = 0; y < frame->height; ++y)
            for (int x = 0; x < frame->width; ++x)
                reinterpret_cast<uint16_t*>(frame->data[p] + y * frame->linesize[p])[x] = value[p];
    return frame;
}
static AVFrame* yuv10(int luma, int u = 512, int v = 512)
{
    auto* frame = av_frame_alloc();
    frame->width = 32;
    frame->height = 16;
    frame->format = AV_PIX_FMT_YUV420P10LE;
    frame->color_primaries = AVCOL_PRI_BT2020;
    frame->color_trc = AVCOL_TRC_SMPTE2084;
    frame->colorspace = AVCOL_SPC_BT2020_NCL;
    frame->color_range = AVCOL_RANGE_MPEG;
    check(av_frame_get_buffer(frame, 32) >= 0, "allocate limited-range 10-bit YUV fixture");
    const int value[3] = {luma, u, v};
    for (int p = 0; p < 3; ++p)
        for (int y = 0; y < (p ? 8 : 16); ++y)
            for (int x = 0; x < (p ? 16 : 32); ++x)
                reinterpret_cast<uint16_t*>(frame->data[p] + y * frame->linesize[p])[x] = value[p];
    return frame;
}

int main(int argc, char** argv)
{
    qputenv("QT_QUICK_BACKEND", "rhi");
    qputenv("QSG_RHI_BACKEND", "d3d11");
    QQuickWindow::setSceneGraphBackend("rhi");
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    QGuiApplication app(argc, argv);
    {
        Oran7VideoFrame frame(yuv10(512));
        auto color = Oran7VideoColorSpace::fromFrame(frame.get(), AV_PIX_FMT_YUV420P10LE);
        check(color.bitDepth == 10 && color.transfer == 1 && color.bt2020 && !color.fullRange,
              "identify source bit depth, PQ, gamut and range");
        auto* mastering = av_mastering_display_metadata_create_side_data(frame.get());
        mastering->has_luminance = 1;
        mastering->max_luminance = {4000, 1};
        check(Oran7VideoColorSpace::fromFrame(frame.get(), AV_PIX_FMT_P010LE).peakNits == 4000,
              "read mastering luminance");
        auto* light = av_content_light_metadata_create_side_data(frame.get());
        light->MaxCLL = 1500;
        check(Oran7VideoColorSpace::fromFrame(frame.get(), AV_PIX_FMT_P010LE).peakNits == 1500,
              "content peak takes priority over mastering display peak");
    }
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    check(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr,
                                      0, D3D11_SDK_VERSION, &device, nullptr, &context)),
          "create WARP device");
    QQuickRenderControl control;
    QQuickWindow window(&control);
    window.setGraphicsDevice(QQuickGraphicsDevice::fromDeviceAndContext(device.Get(), context.Get()));
    window.setGeometry(0, 0, 64, 32);
    window.contentItem()->setSize(QSizeF(64, 32));
    window.setColor(Qt::transparent);
    check(control.initialize(), "initialize isolated production scenegraph");
    auto* rhi = control.rhi();
    std::unique_ptr<QRhiRenderBuffer> depth(rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, QSize(64, 32), 1));
    check(depth->create(), "create depth-stencil attachment");
    auto item = std::make_unique<D3D11VideoItem>();
    item->setParentItem(window.contentItem());
    item->setSize(QSizeF(64, 32));
    item->setContentRect(QRectF(0, 0, 64, 32));
    QSignalSpy info(item.get(), &D3D11VideoItem::sendVideoFrameInfo);
    std::unique_ptr<QRhiTexture> output;
    std::unique_ptr<QRhiTextureRenderTarget> target;
    std::unique_ptr<QRhiRenderPassDescriptor> descriptor;
    Oran7HdrCompositor compositor;
    const auto setTarget = [&](QRhiTexture::Format format, bool nativeHdr)
    {
        window.setRenderTarget({});
        compositor.release();
        target.reset();
        descriptor.reset();
        output.reset();
        output.reset(
            rhi->newTexture(format, QSize(64, 32), 1, QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
        check(output->create(), "create precision-specific output");
        QRhiTextureRenderTargetDescription d{QRhiColorAttachment(output.get())};
        d.setDepthStencilBuffer(depth.get());
        target.reset(rhi->newTextureRenderTarget(d));
        descriptor.reset(target->newCompatibleRenderPassDescriptor());
        target->setRenderPassDescriptor(descriptor.get());
        check(target->create(), "create output target");
        window.setRenderTarget(QQuickRenderTarget::fromRhiRenderTarget(target.get()));
        window.setProperty("_oran7_hdr_composite", nativeHdr);
    };
    const auto render = [&](bool finalComposite = false, bool nativeCopy = false)
    {
        QCoreApplication::processEvents();
        control.polishItems();
        control.beginFrame();
        control.sync();
        const float white = window.property("_oran7_sdr_white_nits").isValid()
                                ? window.property("_oran7_sdr_white_nits").toFloat()
                                : 80.0f;
        if (finalComposite)
            check(compositor.prepare(rhi, target.get(), white), "prepare final HDR compositor");
        control.render();
        if (finalComposite)
            check(compositor.render(control.commandBuffer(), nativeCopy ? nullptr : output.get()),
                  "execute final output conversion");
        QRhiReadbackResult result;
        auto* updates = rhi->nextResourceUpdateBatch();
        updates->readBackTexture(QRhiReadbackDescription(output.get()), &result);
        control.commandBuffer()->resourceUpdate(updates);
        control.endFrame();
        rhi->finish();
        check(result.pixelSize == QSize(64, 32) && !result.data.isEmpty(), "read actual rendered pixels");
        QCoreApplication::processEvents();
        return result.data;
    };
    const auto pixel = [](const QByteArray& data)
    {
        const auto* p = reinterpret_cast<const qfloat16*>(data.constData()) + (16 * 64 + 32) * 4;
        return std::array<float, 4>{float(p[0]), float(p[1]), float(p[2]), float(p[3])};
    };
    setTarget(QRhiTexture::RGBA8, false);
    item->submitFrame(yuv10(64));
    auto bytes = render();
    check(static_cast<unsigned char>(bytes[(16 * 64 + 32) * 4]) < 2, "limited 10-bit HDR black maps to SDR black");
    item->submitFrame(yuv10(940));
    bytes = render();
    check(static_cast<unsigned char>(bytes[(16 * 64 + 32) * 4]) > 250, "HDR highlight maps to SDR white");
    item->submitFrame(rgb10(pqCode(100), pqCode(100), pqCode(100)));
    bytes = render();
    const int middle = static_cast<unsigned char>(bytes[(16 * 64 + 32) * 4]);
    check(middle > 145 && middle < 170, "PQ luminance is tone mapped rather than displayed as encoded gamma");
    setTarget(QRhiTexture::RGBA16F, true);
    item->submitFrame(rgb10(pqCode(1000), pqCode(1000), pqCode(1000)));
    auto value = pixel(render(true));
    std::fprintf(stderr, "PQ 1000 nit: %f %f %f\n", value[0], value[1], value[2]);
    check(value[0] > 12.0f && value[0] < 13.0f, "FP16 scRGB preserves 1000 nit highlights above 1.0");
    check(std::abs(value[0] - value[1]) < 0.03 && std::abs(value[1] - value[2]) < 0.03,
          "neutral HDR stays neutral through BT2020 conversion");
    {
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("videoSource", item.get());
        QQmlComponent component(&engine);
        component.setData(
            "import QtQuick\nShaderEffectSource { width: 16; height: 8; sourceItem: videoSource; live: true; z: 10 }",
            QUrl());
        std::unique_ptr<QObject> effect(component.create());
        check(bool(effect), "create ordinary 8-bit offscreen consumer on HDR window");
        qobject_cast<QQuickItem*>(effect.get())->setParentItem(window.contentItem());
        item->submitFrame(rgb10(pqCode(1000), pqCode(1000), pqCode(1000)));
        const auto data = render(true);
        check(pixel(data)[0] > 12.0, "8-bit offscreen pass does not tone map main HDR output");
        const auto* mini = reinterpret_cast<const qfloat16*>(data.constData()) + (4 * 64 + 8) * 4;
        check(float(mini[0]) > 0.95 && float(mini[0]) < 1.05,
              "8-bit offscreen consumer uses SDR mapping without clipping raw PQ");
    }
    item->submitFrame(rgb10(pqCode(100), 0, 0));
    value = pixel(render(true));
    check(value[0] > 2.0 && value[1] < -0.14 && value[2] < -0.02,
          "wide gamut negative channels survive FP16 and final output conversion");
    item->submitFrame(rgb10(600, 600, 600));
    auto first = pixel(render(true));
    item->submitFrame(rgb10(601, 601, 601));
    auto next = pixel(render(true));
    check(next[0] > first[0] + 0.005f, "neighboring 10-bit levels are not quantized to 8-bit");
    window.setProperty("_oran7_sdr_white_nits", 240.0f);
    item->submitFrame(rgb10(pqCode(1000), pqCode(1000), pqCode(1000)));
    value = pixel(render(true));
    check(value[0] > 12.0f && value[0] < 13.0f, "Windows SDR white setting does not change HDR absolute luminance");
    window.setProperty("_oran7_sdr_white_nits", 80.0f);
    item->submitFrame(rgb10(1023, 1023, 1023, AVCOL_TRC_ARIB_STD_B67));
    value = pixel(render(true));
    check(value[0] > 12.2 && value[0] < 12.8, "HLG reference white maps to 1000 nit scRGB");
    const int y100 = int(std::round(64 + pqCode(100) / 1023.0 * 876));
    item->submitFrame(yuv10(y100));
    const auto softwareYuv = pixel(render(true));
    check(softwareYuv[0] > 1.2 && softwareYuv[0] < 1.3, "planar CPU Main10 retains PQ and limited range");
    // 真正的 P010 GPU 纹理、非零数组 slice、可见裁剪；不依赖视频处理器或硬件解码器。
    D3D11_TEXTURE2D_DESC d{};
    d.Width = 32;
    d.Height = 16;
    d.MipLevels = 1;
    d.ArraySize = 2;
    d.Format = DXGI_FORMAT_P010;
    d.SampleDesc.Count = 1;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> p010;
    check(SUCCEEDED(device->CreateTexture2D(&d, nullptr, &p010)), "create actual P010 texture");
    std::vector<uint16_t> packed(32 * 24, uint16_t(512 << 6));
    std::fill(packed.begin(), packed.begin() + 32 * 16, uint16_t(y100 << 6));
    context->UpdateSubresource(p010.Get(), 1, nullptr, packed.data(), 32 * 2, 0);
    auto* hw = av_frame_alloc();
    hw->format = AV_PIX_FMT_D3D11;
    hw->width = 24;
    hw->height = 12;
    hw->color_trc = AVCOL_TRC_SMPTE2084;
    hw->color_primaries = AVCOL_PRI_BT2020;
    hw->colorspace = AVCOL_SPC_BT2020_NCL;
    hw->color_range = AVCOL_RANGE_MPEG;
    hw->data[0] = reinterpret_cast<uint8_t*>(p010.Get());
    hw->data[1] = reinterpret_cast<uint8_t*>(1);
    p010->AddRef();
    hw->buf[0] = av_buffer_create(
        nullptr, 0, [](void* p, uint8_t*) { static_cast<ID3D11Texture2D*>(p)->Release(); }, p010.Get(), 0);
    item->submitFrame(hw);
    value = pixel(render(true, true));
    std::fprintf(stderr, "P010/CPU: %f / %f\n", value[0], softwareYuv[0]);
    check(std::abs(value[0] - softwareYuv[0]) < 0.02, "native P010 plane views match high precision CPU YUV");
    // 普通 UI 的 sRGB 灰色、预乘透明度及原生复制分支也必须正确。
    auto* sdr = av_frame_alloc();
    sdr->format = AV_PIX_FMT_BGRA;
    sdr->width = 32;
    sdr->height = 16;
    check(av_frame_get_buffer(sdr, 32) >= 0, "allocate ordinary SDR frame");
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 32; ++x)
        {
            auto* p = sdr->data[0] + y * sdr->linesize[0] + x * 4;
            p[0] = p[1] = p[2] = 128;
            p[3] = 255;
        }
    item->setOpacity(0.5);
    item->submitFrame(sdr);
    value = pixel(render(true, true));
    check(std::abs(value[0] - 0.1079) < 0.005 && std::abs(value[3] - 0.5) < 0.005,
          "SDR gamma and premultiplied alpha remain correct on HDR window");
    item->setOpacity(1);
    item->renderBlackFrame();
    value = pixel(render(true));
    check(value[0] < 0.001 && value[1] < 0.001 && value[2] < 0.001, "stop clears HDR highlights");
    item->setParentItem(nullptr);
    item.reset();
    window.setRenderTarget({});
    control.invalidate();
    compositor.release();
    target.reset();
    descriptor.reset();
    depth.reset();
    output.reset();
    std::printf("PASS: %d HDR color/render checks\n", checks);
}
