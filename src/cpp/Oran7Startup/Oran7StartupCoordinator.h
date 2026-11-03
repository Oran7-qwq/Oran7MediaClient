#pragma once
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QElapsedTimer>
#include <QUrl>
#include <QQmlIncubator>
#include <QVariantAnimation>
#include <functional>

class QQmlApplicationEngine;
class QQmlComponent;
class QQuickWindow;

// 入口拥有的启动协调器，不是单例；所有窗口/阶段操作仅在 GUI 线程。
class Oran7StartupCoordinator : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString stageText READ stageText NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool failed READ failed NOTIFY changed)
    Q_PROPERTY(bool finished READ finished NOTIFY changed)
    Q_PROPERTY(bool preparingMain READ preparingMain NOTIFY changed)
    Q_PROPERTY(bool revealMain READ revealMain NOTIFY changed)
  public:
    enum class Oran7Stage
    {
        Idle,
        ShowingSplash,
        Initializing,
        LoadingUi,
        Restoring,
        PreparingDisplay,
        Ready,
        Failed,
        Cancelled
    };
    Q_ENUM(Oran7Stage)
    struct Oran7Options
    {
        QUrl splashUrl;
        QUrl mainUrl;
        std::function<QString()> initialize;
        std::function<QString(QQuickWindow*)> restore;
        bool maximized = false;
        int timeoutMs = 30000;
    };
    explicit Oran7StartupCoordinator(QQmlApplicationEngine& engine);
    void start(Oran7Options options);
    Q_INVOKABLE void cancel();
    QString stageText() const;
    QString error() const { return m_error; }
    bool active() const;
    bool failed() const { return m_stage == Oran7Stage::Failed; }
    bool finished() const { return m_stage == Oran7Stage::Ready; }
    bool preparingMain() const { return m_stage == Oran7Stage::PreparingDisplay || finished(); }
    bool revealMain() const { return m_revealMain; }
    Oran7Stage stage() const { return m_stage; }
    QQuickWindow* mainWindow() const;
    QQuickWindow* splashWindow() const;
  signals:
    void changed();
    void ready();
    void quitRequested();
  private slots:
    void checkReadiness();
    void invalidatePresentation();

  private:
    void initialize();
    void createMain();
    void attachMain(QObject* object);
    void fail(const QString& error);
    void setStage(Oran7Stage stage);
    void releaseComponent();
    void pollCreation();
    void resetTransition();
    QPointer<QQmlApplicationEngine> m_engine;
    QPointer<QQmlComponent> m_component;     // 引擎 parent 持有，终态提前释放。
    QPointer<QQuickWindow> m_splash, m_main; // 引擎持有两个根窗口。
    Oran7Options m_options;
    Oran7Stage m_stage = Oran7Stage::Idle;
    QTimer m_timeout;
    QTimer m_creationPoll;
    QQmlIncubator m_incubator{QQmlIncubator::Asynchronous};
    QVariantAnimation m_transition;
    QElapsedTimer m_clock;
    QString m_error;
    bool m_configurationApplied = false, m_coveredFrame = false, m_revealMain = false;
    bool m_mainPresented = false;
    bool m_transitionComplete = false;
    qreal m_mainOpacity = 1.0;
    bool m_creationScheduled = false;
};
