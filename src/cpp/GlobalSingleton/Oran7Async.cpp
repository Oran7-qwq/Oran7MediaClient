#include "Oran7Async.h"

#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <deque>

namespace {
std::atomic<bool> g_shuttingDown{false};
QMutex g_submitMutex;
QMutex g_networkMutex;
std::deque<std::function<void()>> g_networkQueue;
int g_networkWorkers = 0;
}

QThreadPool& Oran7Async::pool()
{
    static QThreadPool pool;
    static const bool initialized = [] {
        // 留出通用任务执行空间，网络组最多占两个工作线程。
        pool.setMaxThreadCount(qMax(4, QThread::idealThreadCount()));
        return true;
    }();
    Q_UNUSED(initialized);
    return pool;
}

bool Oran7Async::submit(std::function<void()> work, Group group)
{
    QMutexLocker submitLocker(&g_submitMutex);
    if (isShuttingDown()) return false;
    if (group == Group::General) {
        pool().start(std::move(work));
        return true;
    }
    QMutexLocker locker(&g_networkMutex);
    g_networkQueue.push_back(std::move(work));
    if (g_networkWorkers >= 2) return true;
    ++g_networkWorkers;
    pool().start([] {
        for (;;) {
            std::function<void()> task;
            {
                QMutexLocker locker(&g_networkMutex);
                if (g_networkQueue.empty()) { --g_networkWorkers; return; }
                task = std::move(g_networkQueue.front());
                g_networkQueue.pop_front();
            }
            task();
        }
    });
    return true;
}

void Oran7Async::beginShutdown()
{
    QMutexLocker locker(&g_submitMutex);
    g_shuttingDown.store(true, std::memory_order_release);
}

bool Oran7Async::isShuttingDown()
{
    return g_shuttingDown.load(std::memory_order_acquire);
}

void Oran7Async::waitForDone()
{
    // 已接受的网络队列也由池内的两个排队执行器排空。
    pool().waitForDone();
}
