#include "Oran7AsyncManager.h"

#include "NcmAsyncHandlers.h"
#include "Oran7Async.h"

#include <QFile>
#include <QJSEngine>
#include <QQmlEngine>
#include <QThread>

Oran7AsyncManager::Oran7AsyncManager(QObject* parent)
    : QObject(parent)
{
    s_instance = this;

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
    cancelAll();
}

void Oran7AsyncManager::beginShutdown()
{
    if (Oran7AsyncManager* inst = s_instance)
        inst->cancelAll();
}

bool Oran7AsyncManager::cancel(int taskId)
{
    auto it = m_tasks.find(taskId);
    if (it == m_tasks.end()) return false;
    it.value().cancel();
    m_tasks.erase(it);
    return true;
}

void Oran7AsyncManager::cancelAll()
{
    for (const auto& task : std::as_const(m_tasks)) task.cancel();
    m_tasks.clear();
}

int Oran7AsyncManager::run(const QString& operation, const QVariantMap& arguments,
                           QJSValue completed, QJSValue failed)
{
    if (Oran7Async::isShuttingDown()) return -1;
    const auto it = m_handlers.constFind(operation);
    if (it == m_handlers.constEnd()) {
        if (failed.isCallable())
            failed.call({ QJSValue(QStringLiteral("未知后台操作：%1").arg(operation)) });
        return -1;
    }
    const int taskId = m_nextTaskId++;
    const bool network = operation == QLatin1String("musicSearch")
        || operation == QLatin1String("fetchLikedSongs") || operation == QLatin1String("roamSongs");
    auto task = Oran7Async::run(this,
        [handler = it.value(), arguments]() -> QVariantMap {
            QVariantMap reply;
            try {
                reply[QStringLiteral("result")] = handler(arguments);
                reply[QStringLiteral("success")] = true;
            } catch (const std::exception& error) {
                reply[QStringLiteral("success")] = false;
                reply[QStringLiteral("error")] = QString::fromUtf8(error.what());
            } catch (...) {
                reply[QStringLiteral("success")] = false;
                reply[QStringLiteral("error")] = QStringLiteral("未知后台异常");
            }
            return reply;
        },
        [this, taskId, completed = std::move(completed), failed = std::move(failed)](QVariantMap reply) mutable {
            m_tasks.remove(taskId);
            QJSEngine* engine = qjsEngine(this);
            if (!engine) return;
            if (reply.value(QStringLiteral("success")).toBool()) {
                if (completed.isCallable())
                    completed.call({ engine->toScriptValue(reply.value(QStringLiteral("result"))) });
            } else if (failed.isCallable()) {
                failed.call({ QJSValue(reply.value(QStringLiteral("error")).toString()) });
            }
        }, network ? Oran7Async::Group::Network : Oran7Async::Group::General);
    if (!task.isValid()) return -1;
    m_tasks.insert(taskId, task);
    return taskId;
}
