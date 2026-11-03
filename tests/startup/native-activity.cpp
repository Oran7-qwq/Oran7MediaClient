#define NOMINMAX
#include "Oran7StartupActivityBackend.h"
#include <windows.h>
#include <QCoreApplication>
#include <QThread>
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
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    check(SUCCEEDED(com), "initialize COM for isolated composition test");
    // 隐藏原生父窗 + WARP，不采集桌面、不使用硬件 GPU、不启动主程序。
    HWND parent = CreateWindowExW(0, L"STATIC", L"Oran7ActivityFixture", WS_POPUP, 0, 0, 300, 30, nullptr, nullptr,
                                  GetModuleHandleW(nullptr), nullptr);
    check(parent, "hidden native fixture parent");
    {
        Oran7StartupActivityBackend backend;
        check(!backend.initialize(nullptr, QSize(200, 3), Qt::white, Qt::black, true), "invalid parent is rejected");
        check(backend.initialize(parent, QSize(200, 6), QColor("#beb7ff"), QColor("#323746"), true),
              "production composition surfaces and visuals initialize on WARP");
        check(backend.setRunning(true), "linear rightward repeat is accepted by DirectComposition");
        check(backend.present(QRect(10, 5, 200, 6), false, 1.0), "hidden composition target remains valid");
        struct Lookup
        {
            HWND owner;
            HWND popup = nullptr;
        } lookup{parent};
        EnumThreadWindows(
            GetCurrentThreadId(),
            [](HWND window, LPARAM data) -> BOOL
            {
                auto* lookup = reinterpret_cast<Lookup*>(data);
            wchar_t name[80]{};
            GetClassNameW(window, name, 80);
            if (GetWindow(window, GW_OWNER) == lookup->owner && wcscmp(name, L"Oran7StartupActivitySurface") == 0)
                    lookup->popup = window;
                return TRUE;
            },
            reinterpret_cast<LPARAM>(&lookup));
        check(lookup.popup && !(GetWindowLongPtrW(lookup.popup, GWL_STYLE) & WS_CHILD),
              "activity uses an owned popup outside the Qt child-window composition hierarchy");
        check((GetWindowLongPtrW(lookup.popup, GWL_EXSTYLE) & (WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW)) ==
                  (WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW),
              "activity neither activates nor creates a taskbar entry");
        POINT origin{10, 5};
        ClientToScreen(parent, &origin);
        RECT bounds{};
        GetWindowRect(lookup.popup, &bounds);
        check(bounds.left == origin.x && bounds.top == origin.y && bounds.right - bounds.left == 200,
              "activity converts host client bounds to owned-popup screen coordinates");
        check(!IsWindowVisible(lookup.popup), "hidden fixture never exposes a desktop popup");
        // 不泵 GUI 事件，提交的合成动画不需要应用逐帧更新；可见流畅度仍需运行验收。
        QThread::msleep(80);
        check(backend.present(QRect(10, 5, 200, 6), false, 0.5), "composition remains valid after a GUI-thread pause");
        check(backend.setRunning(false) && backend.setRunning(false), "stop is idempotent");
        check(backend.setRunning(true), "resume repeat animation");
        check(backend.initialize(parent, QSize(260, 8), Qt::red, Qt::black, true),
              "DPI/size rebuild releases the previous target");
        check(backend.setRunning(true), "rebuilt target accepts animation");
        DestroyWindow(parent);
        check(!backend.present(QRect(0, 0, 260, 8), false, 1), "destroyed native host is rejected safely");
    }
    CoUninitialize();
    std::printf("PASS: %d native activity checks; hidden native host, WARP only.\n", checks);
}
