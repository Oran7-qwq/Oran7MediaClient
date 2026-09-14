#include "Oran7AsyncManager.h"

#include "NcmAsyncHandlers.h"
#include "Oran7Async.h"

#include <QFile>
#include <QFutureWatcher>
#include <QJSEngine>
#include <QQmlEngine>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>

Oran7AsyncManager::Oran7AsyncManager(QObject* parent)
    : QObject(parent)
{
    s_instance = this;
    m_pool.setMaxThreadCount(QThread::idealThreadCount());

    // 示例/工具处理器
    registerHandler(QStringLiteral("test"),
        [](const QVariantMap& args) -> QVariant {
            const int a = args.value(QStringLiteral("a")).toInt();
            const int b = args.value(QStringLiteral("b")).toInt();
            QThread::msleep(500);   // 模拟耗时
            return a + b;
        });

    registerHandler(QStringLiteral("deleteMusic"),
        [](const QVariantMap& args) -> QVariant {
            const QStringList files = args.value(QStringLiteral("files")).toStringList();
            QVariantList failed;
            for (const QString& file : files) {
                if (!QFile::moveToTrash(file))
                    failed.append(file);
            }
            return failed;
        });

    // ---- 网易云网络批量加载（QML 大量 ListModel 统一走 Oran7AsyncManager）----
    // 阻塞式 curl handler 在线程池执行；Cookie/CSRF 由 QML 侧在 GUI 线程快照后传入
    registerHandler(QStringLiteral("musicSearch"),    &ncm::musicSearchHandler);
    registerHandler(QStringLiteral("fetchLikedSongs"), &ncm::fetchLikedSongsHandler);
    registerHandler(QStringLiteral("roamSongs"),       &ncm::roamSongsHandler);
}

void Oran7AsyncManager::registerHandler(const QString& operation, Handler handler)
{
    m_handlers.insert(operation, std::move(handler));
}

Oran7AsyncManager::~Oran7AsyncManager()
{
    if (s_instance == this)
        s_instance = nullptr;
    m_pool.waitForDone();
}

void Oran7AsyncManager::beginShutdown()
{
    if (Oran7AsyncManager* inst = s_instance)
        inst->m_pool.waitForDone();
}

int Oran7AsyncManager::run(const QString& operation, const QVariantMap& arguments,
                           QJSValue completed, QJSValue failed)
{
    const auto it = m_handlers.constFind(operation);
    if (it == m_handlers.constEnd()) {
        if (failed.isCallable()) {
            failed.call({ QJSValue(QStringLiteral("Unknown operation: %1").arg(operation)) });
        }
        return -1;
    }

    // 退出流程：不再接受新任务，避免在退出阶段往线程池投递工作
    if (Oran7Async::isShuttingDown())
        return -1;

    const int taskId = m_nextTaskId++;
    Handler handler = it.value();

    auto* watcher = new QFutureWatcher<QVariantMap>(this);
    // completed / failed 是 QJSValue，只在 GUI 线程的 finished lambda 里使用，绝不 capture 进 worker
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this,
        [this, watcher, completed = std::move(completed), failed = std::move(failed)]() mutable {
            const QVariantMap reply = watcher->result();
            watcher->deleteLater();

            QJSEngine* engine = qjsEngine(this);
            if (!engine)
                return;

            const bool success = reply.value(QStringLiteral("success")).toBool();
            if (success) {
                if (completed.isCallable()) {
                    const QJSValue result = engine->toScriptValue(reply.value(QStringLiteral("result")));
                    completed.call({ result });
                }
            } else {
                if (failed.isCallable()) {
                    failed.call({ QJSValue(reply.value(QStringLiteral("error")).toString()) });
                }
            }
        });

    watcher->setFuture(QtConcurrent::run(&m_pool,
        [handler = std::move(handler), arguments]() -> QVariantMap {
            QVariantMap reply;
            try {
                QVariant result = handler(arguments);
                reply[QStringLiteral("success")] = true;
                reply[QStringLiteral("result")] = std::move(result);
            } catch (const std::exception& e) {
                reply[QStringLiteral("success")] = false;
                reply[QStringLiteral("error")] = QString::fromUtf8(e.what());
            } catch (...) {
                reply[QStringLiteral("success")] = false;
                reply[QStringLiteral("error")] = QStringLiteral("Unknown exception");
            }
            return reply;
        }));

    return taskId;
}
