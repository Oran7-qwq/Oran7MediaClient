#ifndef ORAN7ASYNC_MANAGER_H
#define ORAN7ASYNC_MANAGER_H

#include <QHash>
#include <QJSValue>
#include <QObject>
#include <QThreadPool>
#include <QVariantMap>

#include <functional>

/**
 * @brief QML 侧异步单例：Async.run(operation, args, completed, failed)
 *
 * 后台耗时的 C++ lambda（Handler）进线程池；QML 的完成/失败回调（QJSValue）
 * 只在 GUI 线程执行，绝不跨线程调用 QML 函数对象。
 *
 * 用法（QML）：
 *   import Oran7Async 1.0
 *   Oran7AsyncManager.run("test", { a: 100, b: 200 },
 *       result => { console.log(result) },
 *       error => { console.error(error) });
 */
class Oran7AsyncManager : public QObject
{
    Q_OBJECT
public:
    using Handler = std::function<QVariant(const QVariantMap&)>;

    explicit Oran7AsyncManager(QObject* parent = nullptr);
    ~Oran7AsyncManager() override;

    /// 进入退出流程：置位后 run() 不再投递新任务，并等待 m_pool 中任务全部结束，
    /// 避免 QML 引擎销毁该单例时 QThreadPool 随成员析构、线程仍在等待。
    static void beginShutdown();

    /// 注册后台处理器：返回值为结果；异常会被捕获转成 failed 回调
    void registerHandler(const QString& operation, Handler handler);

    /// 执行一个已注册的后台操作。成功回调 completed(result)，失败回调 failed(error)。返回 taskId，未知操作返回 -1。
    Q_INVOKABLE int run(const QString& operation, const QVariantMap& arguments,
                        QJSValue completed, QJSValue failed = QJSValue());

private:
    QHash<QString, Handler> m_handlers;
    QThreadPool m_pool;
    int m_nextTaskId = 1;
    static inline Oran7AsyncManager* s_instance = nullptr;
};

#endif // ORAN7ASYNC_MANAGER_H
