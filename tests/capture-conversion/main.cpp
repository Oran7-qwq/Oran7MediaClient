#include "Oran7CaptureFrameConverter.h"
#include "Oran7DesktopCaptureWorker.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTest>
#include <QThread>
#include <wrl/client.h>
#include <array>
#include <vector>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <cstdlib>
extern "C"
{
#include <libavutil/hwcontext_d3d11va.h>
}
using Microsoft::WRL::ComPtr;
using Frame = Oran7CaptureFrameConverter::Frame;
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
struct Oran7SoftwareDevice
{
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    Oran7SoftwareDevice()
    {
        check(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                          nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &context)),
              "create WARP software device");
    }
    ComPtr<ID3D11Texture2D> texture(int width, int height, UINT arraySize = 1)
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = arraySize;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> texture;
        check(SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &texture)), "synthetic BGRA texture");
        return texture;
    }
    void fill(ID3D11Texture2D* texture, UINT slice, uint32_t color)
    {
        D3D11_TEXTURE2D_DESC desc{};
        texture->GetDesc(&desc);
        std::vector<uint32_t> pixels(size_t(desc.Width) * desc.Height, color);
        context->UpdateSubresource(texture, slice, nullptr, pixels.data(), desc.Width * 4, 0);
    }
    uint32_t read(const AVFrame* frame)
    {
        auto* texture = reinterpret_cast<ID3D11Texture2D*>(frame->data[0]);
        ComPtr<IDXGIKeyedMutex> mutex;
        check(SUCCEEDED(texture->QueryInterface(IID_PPV_ARGS(&mutex))), "preview texture has keyed mutex");
        check(mutex->AcquireSync(0, 0) == S_OK, "read preview mutex");
        D3D11_TEXTURE2D_DESC desc{};
        texture->GetDesc(&desc);
        desc.BindFlags = desc.MiscFlags = 0;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> staging;
        check(SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &staging)), "preview staging");
        context->CopyResource(staging.Get(), texture);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        check(SUCCEEDED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "preview readback");
        const auto pixel = *static_cast<uint32_t*>(mapped.pData);
        context->Unmap(staging.Get(), 0);
        context->Flush();
        check(mutex->ReleaseSync(0) == S_OK, "read preview release");
        return pixel;
    }
};
static Frame source(ID3D11Texture2D* texture, int width, int height, int slice = 0)
{
    Frame frame(av_frame_alloc());
    check(bool(frame), "allocate synthetic hardware frame");
    frame->format = AV_PIX_FMT_D3D11;
    frame->width = width;
    frame->height = height;
    frame->data[0] = reinterpret_cast<uint8_t*>(texture); // 本测试由 ComPtr 保证源纹理存活。
    frame->data[1] = reinterpret_cast<uint8_t*>(intptr_t(slice));
    return frame;
}
static void previewChecks(Oran7SoftwareDevice& gpu)
{
    auto texture = gpu.texture(32, 24, 2);
    gpu.fill(texture.Get(), 0, 0xffff0000);
    gpu.fill(texture.Get(), 1, 0xff0000ff);
    auto input = source(texture.Get(), 20, 16, 1);
    Frame survivor;
    {
        Oran7CaptureFrameConverter converter(gpu.device.Get(), 60);
        auto first = converter.preview(input.get());
        check(first.error.isEmpty() && first.frame, "preview succeeds");
        check(first.frame->width == 20 && first.frame->height == 16, "visible size survives padded allocation");
        check(gpu.read(first.frame.get()) == 0xff0000ff, "preview copies selected array slice");
        Frame clone(av_frame_clone(first.frame.get()));
        auto second = converter.preview(input.get());
        auto third = converter.preview(input.get());
        check(second.frame && third.frame, "three distinct preview leases");
        QElapsedTimer timer;
        timer.start();
        auto dropped = converter.preview(input.get());
        check(!dropped.frame && dropped.error.isEmpty() && timer.elapsed() < 500,
              "full preview pool drops without blocking");
        first.frame.reset();
        check(!converter.preview(input.get()).frame, "clone retains slot lease");
        clone.reset();
        check(bool(converter.preview(input.get()).frame), "last clone release permits reuse");
        auto resized = gpu.texture(48, 32);
        gpu.fill(resized.Get(), 0, 0xff00ff00);
        auto larger = source(resized.Get(), 40, 28);
        auto next = converter.preview(larger.get());
        check(next.frame && gpu.read(next.frame.get()) == 0xff00ff00, "size change creates new pool");
        check(gpu.read(second.frame.get()) == 0xff0000ff, "old size lease is not overwritten");
        survivor = std::move(next.frame);
        input->data[1] = reinterpret_cast<uint8_t*>(intptr_t(2));
        check(!converter.preview(input.get()).error.isEmpty(), "invalid slice rejected");
        check(!converter.preview(nullptr).error.isEmpty(), "invalid input rejected");
    }
    check(gpu.read(survivor.get()) == 0xff00ff00, "frame outlives converter and pool");

    // 用消费者持锁耗尽三槽；与 CPU 租约占满是不同路径。
    Oran7CaptureFrameConverter converter(gpu.device.Get(), 60);
    input->data[1] = nullptr;
    Oran7SoftwareDevice reader;
    std::array<ComPtr<IDXGIKeyedMutex>, 3> locks;
    for (auto& lock : locks)
    {
        auto preview = converter.preview(input.get());
        check(bool(preview.frame), "obtain slot before simulated reader");
        auto* tex = reinterpret_cast<ID3D11Texture2D*>(preview.frame->data[0]);
        ComPtr<IDXGIResource> resource;
        HANDLE handle = nullptr;
        check(SUCCEEDED(tex->QueryInterface(IID_PPV_ARGS(&resource))) && SUCCEEDED(resource->GetSharedHandle(&handle)),
              "obtain shared slot handle");
        ComPtr<ID3D11Texture2D> imported;
        check(SUCCEEDED(reader.device->OpenSharedResource(handle, IID_PPV_ARGS(&imported))) &&
                  SUCCEEDED(imported->QueryInterface(IID_PPV_ARGS(&lock))) && lock->AcquireSync(0, 0) == S_OK,
              "foreign simulated reader owns keyed mutex");
    }
    QElapsedTimer timer;
    timer.start();
    auto busy = converter.preview(input.get());
    check(!busy.frame && busy.error.isEmpty() && timer.elapsed() < 500, "GPU busy skips preview without waiting");
    for (auto& lock : locks)
        check(lock->ReleaseSync(0) == S_OK, "release simulated reader");
    check(bool(converter.preview(input.get()).frame), "preview resumes after GPU reader");
}
static void recordingChecks(Oran7SoftwareDevice& gpu)
{
    // WARP 没有 FFmpeg 所需视频接口：仅替换下载边界，其余回退走生产实现。
    Frame downloaded(av_frame_alloc());
    downloaded->format = AV_PIX_FMT_BGRA;
    downloaded->width = 20;
    downloaded->height = 16;
    check(av_frame_get_buffer(downloaded.get(), 32) >= 0, "allocate synthetic downloaded frame");
    for (int y = 0; y < downloaded->height; ++y)
        std::memset(downloaded->data[0] + y * downloaded->linesize[0], 255, downloaded->width * 4);
    auto texture = gpu.texture(32, 24, 2);
    gpu.fill(texture.Get(), 0, 0xff000000);
    gpu.fill(texture.Get(), 1, 0xffffffff);
    auto input = source(texture.Get(), 20, 16, 1);
    input->opaque = downloaded.get();
    input->pts = 12345;
    // 无 VideoDevice 的转换器必走下载 + swscale 回退。
    Oran7CaptureFrameConverter converter(nullptr, 60,
                                         [](AVFrame* destination, const AVFrame* hardware, int)
                                         {
                                             check(intptr_t(hardware->data[1]) == 1 && hardware->width == 20 &&
                                                       hardware->height == 16,
                                                   "fallback downloader receives complete source frame and slice");
                                             return av_frame_ref(destination, static_cast<AVFrame*>(hardware->opaque));
                                         });
    auto output = converter.recording(input.get(), 16, 12);
    check(output && output->format == AV_PIX_FMT_NV12 && output->width == 16 && output->height == 12,
          "CPU fallback converts and scales to NV12");
    check(output->data[0][0] >= 230 && output->data[1][0] >= 125 && output->data[1][0] <= 131,
          "CPU fallback converts downloaded white pixels to limited range");
    check(output->pts == 12345 && output->color_range == AVCOL_RANGE_MPEG && output->colorspace == AVCOL_SPC_BT709,
          "recording timing and SDR color metadata retained");
    converter.beginRecording();
    check(bool(converter.recording(input.get(), 20, 16)), "new recording and target size reuse safely");
    check(!converter.recording(nullptr, 16, 12), "invalid recording frame rejected");
    // 在预览槽位占满时，录制仍从原始帧转换。
    Oran7CaptureFrameConverter preview(gpu.device.Get(), 60);
    std::array<Frame, 3> leases;
    for (auto& lease : leases)
        lease = std::move(preview.preview(input.get()).frame);
    check(!preview.preview(input.get()).frame && converter.recording(input.get(), 16, 12),
          "preview backpressure does not consume or block original recording source");
}
static void workerChecks()
{
    for (bool cancelFirst : {false, true})
    {
        QThread thread;
        // 空提交器使 start 在创建 DDA/设备之前失败；不抓取真实桌面。
        auto* worker = new Oran7DesktopCaptureWorker({}, 0, true, 60);
        auto feed = worker->recordingFeed();
        QSignalSpy errors(worker, &Oran7DesktopCaptureWorker::errorOccurred);
        QSignalSpy started(worker, &Oran7DesktopCaptureWorker::started);
        std::atomic_bool destroyedInWorker{false};
        QObject::connect(worker, &QObject::destroyed, [&] { destroyedInWorker = QThread::currentThread() == &thread; });
        if (cancelFirst)
            worker->stop();
        worker->moveToThread(&thread);
        QObject::connect(&thread, &QThread::started, worker,
                         [worker, &thread]
                         {
                             worker->start();
                             thread.quit();
                         });
        QObject::connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
        thread.start();
        check(thread.wait(5000), "worker startup failure/cancellation terminates");
        check(destroyedInWorker && !feed->snapshot().alive, "worker destroyed in capture thread and closes feed");
        check(started.isEmpty() && errors.size() == (cancelFirst ? 0 : 1),
              "startup error and early cancellation separated");
    }
}
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    Oran7SoftwareDevice gpu;
    previewChecks(gpu);
    recordingChecks(gpu);
    workerChecks();
    std::printf("PASS: %d capture conversion/lifetime checks; WARP only, no desktop capture.\n", checks);
    std::printf("Hardware acceptance pending: VideoProcessor NV12/P010/RGBA, FFmpeg hardware download and recording "
                "throughput.\n");
}
