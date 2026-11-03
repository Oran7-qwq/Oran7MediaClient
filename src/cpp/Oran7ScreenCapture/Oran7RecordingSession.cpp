#include "Oran7RecordingSession.h"
#include <QPointer>
#include <exception>

Oran7RecordingSession::Oran7RecordingSession(QObject* parent)
    : QObject(parent), m_mailbox(std::make_shared<Oran7Mailbox>()) { m_mailbox->receiver = this; }
Oran7RecordingSession::~Oran7RecordingSession() { shutdown(); }
void Oran7RecordingSession::setState(Oran7RecordingState state) { m_state = state; emit changed(); }

bool Oran7RecordingSession::start(Work work) {
    if (m_shutdown || busy() || !work) return false;
    const auto generation = ++m_generation;
    m_cancel = std::make_shared<std::atomic_bool>(false);
    const auto cancelled = m_cancel;
    const auto mailbox = m_mailbox;
    auto result = std::make_shared<QString>();
    const auto post = [mailbox, generation](std::function<void(Oran7RecordingSession*)> call) {
        std::lock_guard<std::mutex> lock(mailbox->mutex);
        if (auto* receiver = mailbox->receiver) QMetaObject::invokeMethod(receiver, [receiver, generation, call] {
            if (receiver->m_generation == generation && !receiver->m_shutdown) call(receiver);
        }, Qt::QueuedConnection);
    };
    Oran7RecordingCallbacks callbacks;
    callbacks.started = [post](const QVariantMap& details) { post([details](auto* self) {
        if (self->m_state != Oran7RecordingState::Preparing) return;
        QPointer<Oran7RecordingSession> guard(self);
        self->setState(Oran7RecordingState::Recording);
        if (guard && self->m_state == Oran7RecordingState::Recording) emit self->started(details);
    }); };
    callbacks.statistics = [post](const QVariantMap& stats) { post([stats](auto* self) {
        if (!self->busy()) return;
        self->m_statistics = stats; emit self->changed();
    }); };
    callbacks.stopping = [post] { post([](auto* self) {
        if (self->busy() && self->m_state != Oran7RecordingState::Stopping) self->setState(Oran7RecordingState::Stopping);
    }); };
    m_thread.reset(QThread::create([work = std::move(work), cancelled, callbacks, result] {
        try { *result = work(*cancelled, callbacks); }
        catch (const std::exception&) { *result = QStringLiteral("录制任务异常，已回收资源"); }
        catch (...) { *result = QStringLiteral("录制任务发生未知异常"); }
    }));
    connect(m_thread.get(), &QThread::finished, this, [this, generation, result] {
        if (generation != m_generation || m_shutdown) return;
        auto* completed = m_thread.release(); completed->deleteLater();
        m_error = *result;
        QPointer<Oran7RecordingSession> guard(this);
        setState(m_error.isEmpty() ? Oran7RecordingState::Idle : Oran7RecordingState::Error);
        if (guard && generation == m_generation) emit finished(result->isEmpty(), *result);
    });
    m_error.clear(); m_statistics.clear();
    // 线程必须先启动，状态通知中的同步 stop/shutdown 才能可靠取消/等待它。
    m_thread->start();
    setState(Oran7RecordingState::Preparing);
    return true;
}
void Oran7RecordingSession::stop() {
    if (!busy() || m_state == Oran7RecordingState::Stopping) return;
    m_cancel->store(true);
    setState(Oran7RecordingState::Stopping);
}
void Oran7RecordingSession::shutdown() {
    if (m_shutdown) return;
    m_shutdown = true; ++m_generation;
    if (m_cancel) m_cancel->store(true);
    if (m_thread) { m_thread->wait(); m_thread.reset(); }
    std::lock_guard<std::mutex> lock(m_mailbox->mutex);
    m_mailbox->receiver = nullptr;
}
