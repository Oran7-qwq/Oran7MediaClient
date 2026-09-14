#include "Oran7Async.h"

#include <QThread>
#include <QThreadPool>

#include <atomic>

namespace {

// 跨 DLL/EXE 共享的退出标志与线程池：Oran7Async 被 Oran7Core.dll、
// Oran7UI_Impl.dll、Oran7MediaClient.exe 三个模块共同使用，
// 若用头文件内函数局部 static，各模块会各自持有一份，导致退出时
// 部分线程池未排空即被析构（QWaitCondition: Destroyed while threads...）。
// 因此统一放到 Oran7Core 动态库内，通过导出符号让所有模块共享同一实例。
std::atomic<bool> g_shuttingDown{false};

} // namespace

QThreadPool& Oran7Async::pool()
{
    static QThreadPool pool;
    static const bool init = [] {
        pool.setMaxThreadCount(QThread::idealThreadCount());
        return true;
    }();
    Q_UNUSED(init);
    return pool;
}

void Oran7Async::beginShutdown()
{
    g_shuttingDown.store(true, std::memory_order_release);
}

bool Oran7Async::isShuttingDown()
{
    return g_shuttingDown.load(std::memory_order_acquire);
}

void Oran7Async::waitForDone()
{
    pool().waitForDone();
}