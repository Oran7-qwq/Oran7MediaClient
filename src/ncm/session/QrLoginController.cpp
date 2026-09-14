#include "QrLoginController.h"

#include "CookieStore.h"
#include "GlobalHelper.h"
#include "NcmAuthApi.h"
#include "NcmSession.h"
#include "NcmUserApi.h"

#include <QJsonObject>
#include <QPainter>
#include <QQuickImageProvider>

#include "qrcodegen.hpp"

// ---- 本地二维码图片提供者（无 Q_OBJECT，不需 moc）----
namespace {

class NcmQrImageProvider : public QQuickImageProvider
{
public:
    explicit NcmQrImageProvider(QrLoginController* ctrl)
        : QQuickImageProvider(QQuickImageProvider::Image)
        , m_ctrl(ctrl)
    {
    }

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override
    {
        QImage img = m_ctrl->qrImage();
        NETWORK_LOG << "NcmQrImageProvider: requestImage id=" << id
                    << " imgNull=" << img.isNull()
                    << " imgSize=" << img.size();
        if (img.isNull()) {
            // 二维码尚未生成时返回 1x1 空白占位图，避免 QML Image 报 Image.Error
            img = QImage(1, 1, QImage::Format_ARGB32);
            img.fill(Qt::white);
        }
        if (size)
            *size = img.size();
        if (requestedSize.isValid())
            img = img.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        return img;
    }

private:
    QrLoginController* m_ctrl = nullptr;
};

// 从 "MUSIC_U=xxx; __csrf=yyy" 提取指定 cookie
QString extractCookie(const QString& cookieStr, const QString& name)
{
    const QStringList parts = cookieStr.split(';');
    for (const QString& part : parts) {
        const QString p = part.trimmed();
        if (p.startsWith(name + QLatin1Char('=')))
            return p.mid(name.length() + 1);
    }
    return QString();
}

} // namespace

QrLoginController::QrLoginController(NcmSession* session, NcmAuthApi* authApi,
                                     NcmUserApi* userApi, QObject* parent)
    : QObject(parent)
    , m_session(session)
    , m_authApi(authApi)
    , m_userApi(userApi)
{
    m_pollTimer.setInterval(3000);
    connect(&m_pollTimer, &QTimer::timeout, this, &QrLoginController::pollTick);
    connect(m_authApi, &NcmAuthApi::qrKeyReady, this, &QrLoginController::onQrKeyReady);
    connect(m_authApi, &NcmAuthApi::qrCheckFinished, this, &QrLoginController::onQrCheckFinished);
    connect(m_authApi, &NcmAuthApi::apiError, this, [this](const QString& msg) {
        m_checkPending = false; // 复位防重入，避免网络抖动后轮询卡死
        // 仅"生成二维码"阶段失败置 Error；轮询阶段（WaitingScan/Scanned）网络抖动不打断流程，
        // 下个 tick 自动重试
        if (m_state == LoginState::Generating) {
            setState(LoginState::Error);
            emit loginFailed(msg);
        }
    });
}

QrLoginController::LoginState QrLoginController::loginState() const { return m_state; }

QString QrLoginController::qrCodeSource() const
{
    return QStringLiteral("image://ncmqr/latest?v=%1").arg(m_qrVersion);
}

QImage QrLoginController::qrImage() const { return m_qrImage; }

QQuickImageProvider* QrLoginController::createQrImageProvider() const
{
    return new NcmQrImageProvider(const_cast<QrLoginController*>(this));
}

void QrLoginController::setState(LoginState state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged();
    }
}

void QrLoginController::start()
{
    // 幂等：已在生成/等待扫码/已扫码阶段忽略
    if (m_state == LoginState::Generating || m_state == LoginState::WaitingScan
        || m_state == LoginState::Scanned)
        return;

    setState(LoginState::Generating);
    m_authApi->createQrKey();
}

void QrLoginController::cancel()
{
    m_pollTimer.stop();
    m_checkPending = false;
    m_qrImage = QImage();
    setState(LoginState::Idle);
}

void QrLoginController::refresh()
{
    cancel();
    start();
}

void QrLoginController::onQrKeyReady(const QString& unikey, const QString& loginUrl)
{
    m_unikey = unikey;
    m_qrImage = generateQrImage(loginUrl);
    ++m_qrVersion;
    emit qrCodeChanged();

    setState(LoginState::WaitingScan);
    m_pollTimer.start();
    NETWORK_LOG << "QrLoginController: qr ready, version=" << m_qrVersion;
}

void QrLoginController::onQrCheckFinished(int code, const QJsonObject& data)
{
    m_checkPending = false;
    NETWORK_LOG << "QrLoginController: poll code=" << code;

    // 注：曾尝试对空响应做退避，但 IP 级风控下退避无意义，反而降低抓到 802/803 的密度，
    // 保持 3s 恒定轮询（Python 探测 12/12 通过的节奏）
    switch (code) {
    case 800: // 二维码已过期
        m_pollTimer.stop();
        setState(LoginState::Expired);
        break;
    case 801: // 等待扫码
        setState(LoginState::WaitingScan);
        break;
    case 802: // 已扫码，待手机确认
        setState(LoginState::Scanned);
        break;
    case 8821: // 需要行为验证码（风控拦截），扫码端需完成验证
        m_pollTimer.stop();
        setState(LoginState::Error);
        emit loginFailed(QStringLiteral("登录需要行为验证（风控），请稍后再试"));
        NETWORK_LOG << "QrLoginController: 8821 behavior captcha required";
        break;
    case 803: { // 登录成功
        m_pollTimer.stop();
        // 提取 Cookie：cookie 字符串可能带 "MUSIC_U=..; __csrf=.."
        QString cookieStr = data["cookie"].toString();
        if (cookieStr.isEmpty())
            cookieStr = data["data"].toObject()["cookie"].toString();
        const QString musicU = extractCookie(cookieStr, QStringLiteral("MUSIC_U"));
        const QString csrf   = extractCookie(cookieStr, QStringLiteral("__csrf"));

        if (!musicU.isEmpty()) {
            m_session->cookieStore()->setAuthCookie(QStringLiteral("MUSIC_U"), musicU);
            m_session->cookieStore()->setAuthCookie(QStringLiteral("__csrf"), csrf);

            // 经典端点在 803 里直接带昵称，立即显示；account/get 再兜底刷新
            QString nickname = data["data"].toObject()["nickname"].toString();
            if (nickname.isEmpty())
                nickname = data["nickname"].toString();
            if (!nickname.isEmpty())
                m_session->cookieStore()->setNickname(nickname);

            m_session->cookieStore()->save();
            m_session->setLoggedIn(true);
        } else {
            NETWORK_LOG << "QrLoginController: 803 but no MUSIC_U in cookie";
        }

        setState(LoginState::Success);
        emit loginSucceeded(m_session->userName());
        fetchAccountAfterLogin();
        break;
    }
    default: // 未知 code，等下一次轮询
        break;
    }
}

void QrLoginController::pollTick()
{
    if (m_checkPending)
        return; // 上一次 poll 未回，跳过本次，防重入
    m_checkPending = true;
    m_authApi->checkQr(m_unikey);
}

void QrLoginController::fetchAccountAfterLogin()
{
    connect(m_userApi, &NcmUserApi::accountFetched, this,
            [this](const QString& nickname) {
                m_session->setUserName(nickname);
                m_session->save();
                emit loginSucceeded(nickname);
                NETWORK_LOG << "QrLoginController: account fetched, user=" << nickname;
            },
            Qt::UniqueConnection);
    // 失败不致命（已登录态不受影响），由 apiError 静默兜底
    m_userApi->fetchAccount();
}

QImage QrLoginController::generateQrImage(const QString& text) const
{
    using qrcodegen::QrCode;
    const QrCode qr = QrCode::encodeText(text.toUtf8().constData(), QrCode::Ecc::MEDIUM);

    const int border = 4;   // quiet zone
    const int scale  = 4;   // 每个模块 4px，约 200 出头，弹窗内足够清晰
    const int size   = (qr.getSize() + border * 2) * scale;

    QImage image(size, size, QImage::Format_ARGB32);
    image.fill(Qt::white); // 整图白底（含 quiet zone）

    QPainter painter(&image);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    for (int y = 0; y < qr.getSize(); ++y) {
        for (int x = 0; x < qr.getSize(); ++x) {
            if (qr.getModule(x, y))
                painter.fillRect((x + border) * scale, (y + border) * scale, scale, scale, Qt::black);
        }
    }
    return image;
}
