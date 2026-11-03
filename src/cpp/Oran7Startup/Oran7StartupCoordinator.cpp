#include "Oran7StartupCoordinator.h"
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QThread>
#include <QDebug>
#include <exception>

Oran7StartupCoordinator::Oran7StartupCoordinator(QQmlApplicationEngine& engine) : m_engine(&engine)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] { fail(QStringLiteral("启动等待超时，请关闭后重试")); });
    m_creationPoll.setInterval(8);
    connect(&m_creationPoll, &QTimer::timeout, this, &Oran7StartupCoordinator::pollCreation);
    m_transition.setDuration(320);
    m_transition.setStartValue(0.0);
    m_transition.setEndValue(1.0);
    m_transition.setEasingCurve(QEasingCurve::InOutCubic);
    connect(&m_transition, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& value)
            {
                if (m_stage != Oran7Stage::PreparingDisplay || !m_mainPresented || !m_main)
                    return;
                const qreal progress = value.toReal();
                m_main->setOpacity(m_mainOpacity * progress);
                // 主窗已逐渐显现后再淡出外部加载窗，避免同时变透明造成视觉空档。
                if (m_mainPresented && m_splash)
                    m_splash->setOpacity(1.0 - qBound(0.0, (progress - 0.25) / 0.75, 1.0));
                if (m_main)
                    m_main->update();
            });
    connect(&m_transition, &QVariantAnimation::finished, this,
            [this]
            {
                if (m_stage != Oran7Stage::PreparingDisplay || !m_mainPresented || !m_main)
                    return;
                m_transitionComplete = true;
                m_main->update();
            });
}
bool Oran7StartupCoordinator::active() const
{
    return m_stage != Oran7Stage::Idle && !finished() && !failed() && m_stage != Oran7Stage::Cancelled;
}
QQuickWindow* Oran7StartupCoordinator::mainWindow() const
{
    return m_main.data();
}
QQuickWindow* Oran7StartupCoordinator::splashWindow() const
{
    return m_splash.data();
}
QString Oran7StartupCoordinator::stageText() const
{
    switch (m_stage)
    {
    case Oran7Stage::Initializing:
        return QStringLiteral("正在初始化服务…");
    case Oran7Stage::LoadingUi:
        return QStringLiteral("正在加载主界面…");
    case Oran7Stage::Restoring:
        return QStringLiteral("正在恢复配置…");
    case Oran7Stage::PreparingDisplay:
        return QStringLiteral("正在准备显示…");
    case Oran7Stage::Ready:
        return QStringLiteral("准备就绪");
    case Oran7Stage::Failed:
        return QStringLiteral("启动失败");
    case Oran7Stage::Cancelled:
        return QStringLiteral("正在退出…");
    default:
        return QStringLiteral("正在准备启动…");
    }
}
void Oran7StartupCoordinator::setStage(Oran7Stage stage)
{
    if (m_stage == stage)
        return;
    m_stage = stage;
    qInfo().noquote() << "[Startup]" << stageText() << (m_clock.isValid() ? m_clock.elapsed() : 0) << "ms";
    emit changed();
}
void Oran7StartupCoordinator::start(Oran7Options options)
{
    Q_ASSERT(thread() == QThread::currentThread());
    if (!m_engine || m_stage != Oran7Stage::Idle)
        return;
    m_options = std::move(options);
    m_clock.start();
    m_timeout.start(qMax(1, m_options.timeoutMs));
    setStage(Oran7Stage::ShowingSplash);
    m_engine->load(m_options.splashUrl);
    for (auto* root : m_engine->rootObjects())
        if (root->objectName() == QStringLiteral("__Oran7StartupWindow__"))
            m_splash = qobject_cast<QQuickWindow*>(root);
    if (!m_splash)
    {
        fail(QStringLiteral("无法创建启动窗口"));
        emit quitRequested();
        return;
    }
    connect(m_splash, &QQuickWindow::sceneGraphError, this,
            [this](QQuickWindow::SceneGraphError, const QString& reason)
            {
                if (!active())
                    return;
                fail(reason);
                // 启动窗口自身无法渲染时不能留在不可见的错误页。
                emit quitRequested();
            });
    // 应用固定使用 basic 渲染循环；AutoConnection 在 GUI 帧提交边界直接观察。
    connect(m_splash, &QQuickWindow::frameSwapped, this,
            [this]
            {
                if (m_stage != Oran7Stage::ShowingSplash)
                    return;
                setStage(Oran7Stage::Initializing);
                // 先让启动窗口提交一帧，再进入现有服务初始化，不循环 processEvents。
                QTimer::singleShot(0, this, &Oran7StartupCoordinator::initialize);
            });
    m_splash->requestUpdate();
}
void Oran7StartupCoordinator::initialize()
{
    if (!m_engine || m_stage != Oran7Stage::Initializing)
        return;
    try
    {
        const auto error = m_options.initialize ? m_options.initialize() : QString();
        if (!active())
            return;
        if (!error.isEmpty())
        {
            fail(error);
            return;
        }
        setStage(Oran7Stage::LoadingUi);
        if (!active())
            return;
        m_component = new QQmlComponent(m_engine, m_engine);
        connect(m_component, &QQmlComponent::statusChanged, this,
                [this]
                {
                    if (!m_component || m_stage != Oran7Stage::LoadingUi)
                        return;
                    if (m_component->isError())
                    {
                        fail(m_component->errorString());
                        return;
                    }
                    if (m_component->isReady() && !m_creationScheduled)
                    {
                        m_creationScheduled = true;
                        QTimer::singleShot(0, this, &Oran7StartupCoordinator::createMain);
                    }
                });
        m_component->loadUrl(m_options.mainUrl, QQmlComponent::Asynchronous);
    }
    catch (const std::exception& error)
    {
        fail(QString::fromUtf8(error.what()));
    }
    catch (...)
    {
        fail(QStringLiteral("启动初始化失败"));
    }
}
void Oran7StartupCoordinator::createMain()
{
    if (!m_engine || m_stage != Oran7Stage::LoadingUi)
        return;
    // GUI 线程分帧创建对象，使用加载窗提供的孵化预算；不循环 processEvents。
    // 手工孵化的根不在 rootObjects() 内，完成后由同一个引擎 parent 独占。
    if (!m_splash)
    {
        fail(QStringLiteral("无法创建启动窗口"));
        return;
    }
    m_engine->setIncubationController(m_splash->incubationController());
    m_component->create(m_incubator);
    m_creationPoll.start();
}
void Oran7StartupCoordinator::pollCreation()
{
    if (!m_engine || m_stage != Oran7Stage::LoadingUi)
        return;
    if (m_incubator.isError())
    {
        QStringList errors;
        for (const auto& error : m_incubator.errors())
            errors.append(error.toString());
        fail(errors.join('\n'));
    }
    else if (m_incubator.isReady())
    {
        m_creationPoll.stop();
        auto* object = m_incubator.object();
        object->setParent(m_engine);
        QQmlEngine::setObjectOwnership(object, QQmlEngine::CppOwnership);
        m_incubator.clear();
        attachMain(object);
    }
}
void Oran7StartupCoordinator::attachMain(QObject* object)
{
    auto* window = qobject_cast<QQuickWindow*>(object);
    if (!window || window->objectName() != QStringLiteral("__Oran7Window__"))
    {
        fail(QStringLiteral("无法创建主界面"));
        return;
    }
    m_main = window;
    if (window->isVisible())
    {
        window->hide();
        fail(QStringLiteral("主界面提前显示，启动交接已中止"));
        return;
    }
    if (window->metaObject()->indexOfSignal("startupPageReadyChanged()") < 0)
    {
        fail(QStringLiteral("主界面缺少就绪通知"));
        return;
    }
    // 原生窗口必须曝光才建立交换链；QML 遮罩无法保护首个缓冲提交前的原生底色。
    // 保持窗口已显示/可渲染，但在实际页面帧提交前不让桌面合成器看到它。
    m_mainOpacity = window->opacity();
    window->setOpacity(0.0);
    connect(window, SIGNAL(startupPageReadyChanged()), this, SLOT(invalidatePresentation()));
    connect(window, &QQuickWindow::sceneGraphError, this,
            [this](QQuickWindow::SceneGraphError, const QString& reason) { fail(reason); });
    connect(window, &QQuickWindow::frameSwapped, this,
            [this]
            {
                if (m_stage != Oran7Stage::PreparingDisplay)
                    return;
                if (!m_main || !m_main->property("startupPageReady").toBool())
                {
                    m_coveredFrame = false;
                    return;
                }
                if (!m_coveredFrame)
                {
                    m_coveredFrame = true;
                    checkReadiness();
                    return;
                }
                if (!m_revealMain)
                    return;
                if (!m_mainPresented)
                {
                    // 已在零透明度下提交实际界面，才开始交叉渐变。
                    m_mainPresented = true;
                    m_transitionComplete = false;
                    m_transition.start();
                    m_main->update();
                    qInfo() << "[Startup] main content submitted; native presentation requested" << m_clock.elapsed()
                            << "ms";
                    return;
                }
                if (!m_transitionComplete)
                    return;
                // 原生窗口已呈现实际界面，才隐藏启动窗口。
                m_timeout.stop();
                setStage(Oran7Stage::Ready);
                // 完成交接后不接管运行期场景图错误或继续观察每一帧。
                if (m_main)
                    QObject::disconnect(m_main, nullptr, this, nullptr);
                releaseComponent();
                if (m_splash)
                    m_splash->hide();
                if (m_main)
                {
                    m_main->raise();
                    m_main->requestActivate();
                }
                emit ready();
            });
    connect(window, &QObject::destroyed, this,
            [this]
            {
                if (active())
                    fail(QStringLiteral("主界面在启动期间被销毁"));
            });
    setStage(Oran7Stage::Restoring);
    if (!active())
        return;
    try
    {
        // QWindowKit 在原生 HWND 建立时移除系统标题栏，会改变客户区高度。
        // 必须先在隐藏/零透明度下完成这一步，再按客户区口径恢复配置几何。
        // 不能等 show() 首次创建 HWND，也不能减去固定像素补偿（边框随 DPI 变化）。
        window->create();
        if (!active())
            return;
        const auto error = m_options.restore ? m_options.restore(window) : QString();
        if (!active())
            return;
        if (!error.isEmpty())
        {
            fail(error);
            return;
        }
        m_configurationApplied = true;
        setStage(Oran7Stage::PreparingDisplay);
        if (!active())
            return;
        if (m_options.maximized)
            window->showMaximized();
        else
            window->show();
        if (m_splash)
            m_splash->raise();
        window->requestUpdate();
    }
    catch (const std::exception& error)
    {
        fail(QString::fromUtf8(error.what()));
    }
    catch (...)
    {
        fail(QStringLiteral("恢复配置失败"));
    }
}
void Oran7StartupCoordinator::invalidatePresentation()
{
    if (!m_main || m_stage != Oran7Stage::PreparingDisplay)
        return;
    m_coveredFrame = false;
    m_mainPresented = false;
    resetTransition();
    m_main->setOpacity(0.0);
    if (m_revealMain)
    {
        m_revealMain = false;
        emit changed();
    }
    m_main->update();
}
void Oran7StartupCoordinator::checkReadiness()
{
    if (!m_main || m_stage != Oran7Stage::PreparingDisplay || !m_configurationApplied || !m_coveredFrame ||
        m_revealMain)
        return;
    if (!m_main->property("startupPageReady").toBool() || m_main->width() <= 0 || m_main->height() <= 0)
        return;
    m_revealMain = true;
    emit changed();
    m_main->update();
}
void Oran7StartupCoordinator::releaseComponent()
{
    m_creationPoll.stop();
    m_incubator.clear();
    if (m_component)
        m_component->deleteLater();
    m_component = nullptr;
}
void Oran7StartupCoordinator::resetTransition()
{
    m_transition.stop();
    m_transitionComplete = false;
    if (m_splash)
        m_splash->setOpacity(1.0);
}
void Oran7StartupCoordinator::fail(const QString& error)
{
    if (!active())
        return;
    m_timeout.stop();
    resetTransition();
    m_error = error;
    m_revealMain = false;
    setStage(Oran7Stage::Failed);
    releaseComponent();
    if (m_main)
        m_main->hide();
    if (m_splash)
    {
        m_splash->show();
        m_splash->raise();
    }
    qWarning().noquote() << "[Startup]" << error;
}
void Oran7StartupCoordinator::cancel()
{
    if (finished() || m_stage == Oran7Stage::Cancelled)
        return;
    m_timeout.stop();
    resetTransition();
    m_revealMain = false;
    setStage(Oran7Stage::Cancelled);
    releaseComponent();
    if (m_main)
        m_main->hide();
    emit quitRequested();
}
