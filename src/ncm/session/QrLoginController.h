#ifndef QRLOGINCONTROLLER_H
#define QRLOGINCONTROLLER_H

#include <QImage>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>

class NcmAuthApi;
class NcmSession;
class NcmUserApi;
class QQuickImageProvider;

/**
 * @brief 扫码登录状态机控制器（QML 单例 NcmQrLogin）
 *
 * 流程：start → createQrKey → 本地生成二维码 → 每 3s checkQr 轮询
 *       801/802 → 继续，803 → 提取 Cookie 写会话并取昵称，800 → 过期。
 */
class QrLoginController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(LoginState loginState READ loginState NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString qrCodeSource READ qrCodeSource NOTIFY qrCodeChanged FINAL)

public:
    enum class LoginState {
        Idle,
        Generating,
        WaitingScan,
        Scanned,
        Success,
        Expired,
        Error
    };
    Q_ENUM(LoginState)

    explicit QrLoginController(NcmSession* session, NcmAuthApi* authApi, NcmUserApi* userApi,
                               QObject* parent = nullptr);

    Q_INVOKABLE void start();    // 幂等：活跃状态忽略
    Q_INVOKABLE void cancel();   // 停轮询，回 Idle
    Q_INVOKABLE void refresh();  // 过期/出错后重新来

    LoginState loginState() const;
    QString qrCodeSource() const;   // "image://ncmqr/latest?v=N"
    QImage qrImage() const;         // 供 image provider 取图
    QQuickImageProvider* createQrImageProvider() const;

signals:
    void stateChanged();
    void qrCodeChanged();
    void loginSucceeded(const QString& nickname);
    void loginFailed(const QString& message);

private slots:
    void onQrKeyReady(const QString& unikey, const QString& loginUrl);
    void onQrCheckFinished(int code, const QJsonObject& data);
    void pollTick();

private:
    void setState(LoginState state);
    QImage generateQrImage(const QString& text) const;
    void fetchAccountAfterLogin();

    NcmSession* m_session = nullptr;
    NcmAuthApi* m_authApi = nullptr;
    NcmUserApi* m_userApi = nullptr;
    QTimer m_pollTimer;
    bool m_checkPending = false;   // 防重入：上一次 poll 未回，本次 tick 跳过
    int m_qrVersion = 0;           // 二维码版本号，拼进 source 强制刷新
    QString m_unikey;
    QImage m_qrImage;
    LoginState m_state = LoginState::Idle;
};

#endif // QRLOGINCONTROLLER_H
