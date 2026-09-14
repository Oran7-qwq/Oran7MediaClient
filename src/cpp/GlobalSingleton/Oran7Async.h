#ifndef ORAN7ASYNC_H
#define ORAN7ASYNC_H

#include <QFutureWatcher>
#include <QThread>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrentRun>

#include <functional>
#include <type_traits>
#include <utility>

/**
 * @brief 通用异步执行器（C++ 侧）：把 lambda 提交到线程池，完成后自动切回 context（GUI）线程调用 done。
 *
 * 用法：
 *   Oran7Async::run(this,
 *       []() -> Result {  后台线程：耗时工作  },
 *       [this](Result r) {  GUI 线程：更新 UI  });
 *
 * done 通过 QFutureWatcher 的 finished 信号跨线程自动调度到 context 所在线程执行，
 * 因此 done 里可以安全访问 QObject 成员、emit 信号、操作 QML。
 */
class Oran7Async
{
public:
    /// 线程池实例。实现位于 Oran7Async.cpp（Oran7Core 共享库内），
    /// 保证跨 EXE / Oran7UI_Impl.dll 只有一个实例，避免各模块持有一份。
    static QThreadPool& pool();

    /// 进入退出流程：之后 run/runDetached 不再向线程池投递新任务，
    /// 避免退出/静态析构阶段 Logger 等仍在后台创建线程（拒绝访问）。
    static void beginShutdown();

    static bool isShuttingDown();

    /// 等待线程池中所有已投递任务执行完毕（应在 beginShutdown 后调用）。
    static void waitForDone();

    template<typename Work, typename Done>
    static void run(QObject* context, Work&& work, Done&& done)
    {
        if (isShuttingDown())
            return;

        using Result = std::invoke_result_t<std::decay_t<Work>>;

        auto* watcher = new QFutureWatcher<Result>(context);
        QObject::connect(watcher, &QFutureWatcher<Result>::finished, context,
            [watcher, done = std::forward<Done>(done)]() mutable {
                if constexpr (std::is_void_v<Result>) {
                    // QFutureWatcher<void> 没有 result()；finished 信号已表示完成，直接回调
                    watcher->deleteLater();
                    std::invoke(done);
                } else {
                    Result result = watcher->result();
                    watcher->deleteLater();
                    std::invoke(done, std::move(result));
                }
            });
        watcher->setFuture(QtConcurrent::run(&pool(), std::forward<Work>(work)));
    }

    /**
     * @brief 线程安全的 fire-and-forget：从任意线程提交到线程池执行，不创建 QFutureWatcher/QObject 父子关系。
     *        适合日志输出等"后台执行、无需回主线程"的场景（避免跨线程父对象告警）。
     * @note 返回的 QFuture 被丢弃，任务仍会在线程池执行；异常由任务自身负责。
     */
    template<typename Work>
    static void runDetached(Work&& work)
    {
        if (isShuttingDown())
            return;
        // 不需要 QFuture，用 QThreadPool::start 提交到后台线程执行（无 nodiscard 告警）
        pool().start(std::forward<Work>(work));
    }
};

#endif // ORAN7ASYNC_H
