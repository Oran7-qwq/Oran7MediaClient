#pragma once

#include "Oran7TaskHandle.h"
#include <QDebug>
#include <QFutureWatcher>
#include <QPromise>
#include <QThreadPool>
#include <functional>
#include <exception>
#include <type_traits>
#include <utility>

// EXE、主题插件与 QML 共用 Oran7Core 内的一份线程池。
class Oran7Async
{
public:
    enum class Group { General, Network };
    static QThreadPool& pool();
    static void beginShutdown();
    static bool isShuttingDown();
    static void waitForDone();

    template<typename Work, typename Done>
    static Oran7TaskHandle run(QObject* context, Work&& work, Done&& done,
                              Group group = Group::General)
    {
        if (!context || isShuttingDown()) return {};
        using Result = std::invoke_result_t<std::decay_t<Work>>;
        auto canceled = std::make_shared<std::atomic_bool>(false);
        auto promise = std::make_shared<QPromise<Result>>();
        auto worker = std::make_shared<std::decay_t<Work>>(std::forward<Work>(work));
        promise->start();
        auto* watcher = new QFutureWatcher<Result>(context);
        QObject::connect(context, &QObject::destroyed, watcher,
            [canceled] { canceled->store(true); }, Qt::DirectConnection);
        QObject::connect(watcher, &QFutureWatcher<Result>::finished, context,
            [watcher, canceled, done = std::forward<Done>(done)]() mutable {
                watcher->deleteLater();
                if (canceled->load() || isShuttingDown()) return;
                try {
                    if constexpr (std::is_void_v<Result>) {
                        watcher->future().waitForFinished();
                        std::invoke(done);
                    } else {
                        std::invoke(done, watcher->future().takeResult());
                    }
                } catch (const std::exception& error) {
                    qWarning() << "后台任务异常：" << error.what();
                } catch (...) {
                    qWarning() << "后台任务发生未知异常";
                }
            });
        watcher->setFuture(promise->future());
        const bool accepted = submit([promise, canceled, worker]() mutable {
            try {
                if (!canceled->load()) {
                    if constexpr (std::is_void_v<Result>) std::invoke(*worker);
                    else promise->addResult(std::invoke(*worker));
                }
            } catch (...) {
                promise->setException(std::current_exception());
            }
            promise->finish();
        }, group);
        if (!accepted) {
            canceled->store(true);
            promise->finish();
            return {};
        }
        return Oran7TaskHandle(canceled);
    }

    template<typename Work>
    static Oran7TaskHandle runDetached(Work&& work)
    {
        if (isShuttingDown()) return {};
        auto canceled = std::make_shared<std::atomic_bool>(false);
        auto worker = std::make_shared<std::decay_t<Work>>(std::forward<Work>(work));
        if (!submit([canceled, worker] {
            if (!canceled->load()) std::invoke(*worker);
        }, Group::General)) return {};
        return Oran7TaskHandle(canceled);
    }

private:
    // 已接受的 Network 工作最多并行两个，不占用线程等待信号量。
    static bool submit(std::function<void()> work, Group group);
};
