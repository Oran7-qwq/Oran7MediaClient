#pragma once
#include <QObject>
#include <QThread>
#include <QVariantMap>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

struct Oran7RecordingCallbacks {
    std::function<void(const QVariantMap&)> started;
    std::function<void(const QVariantMap&)> statistics;
    std::function<void()> stopping;
};

// GUI 仅提交请求；任务在独立线程中准备、运行和回收全部录制资源。
class Oran7RecordingSession : public QObject {
    Q_OBJECT
public:
    enum class Oran7RecordingState { Idle, Preparing, Recording, Stopping, Error };
    Q_ENUM(Oran7RecordingState)
    using Work = std::function<QString(const std::atomic_bool&, const Oran7RecordingCallbacks&)>;
    explicit Oran7RecordingSession(QObject* parent = nullptr);
    ~Oran7RecordingSession() override;
    bool start(Work work);
    void stop();
    void shutdown(); // 仅应用退出/析构同步等待，不用于普通录制启停。
    Oran7RecordingState state() const { return m_state; }
    bool busy() const { return m_state == Oran7RecordingState::Preparing || m_state == Oran7RecordingState::Recording || m_state == Oran7RecordingState::Stopping; }
    QString error() const { return m_error; }
    QVariantMap statistics() const { return m_statistics; }
signals:
    void changed();
    void started(const QVariantMap& details);
    void finished(bool success, const QString& error);
private:
    struct Oran7Mailbox { std::mutex mutex; Oran7RecordingSession* receiver{}; };
    void setState(Oran7RecordingState state);
    std::shared_ptr<Oran7Mailbox> m_mailbox;
    std::shared_ptr<std::atomic_bool> m_cancel;
    std::unique_ptr<QThread> m_thread;
    Oran7RecordingState m_state = Oran7RecordingState::Idle;
    QString m_error;
    QVariantMap m_statistics;
    quint64 m_generation = 0;
    bool m_shutdown = false;
};
