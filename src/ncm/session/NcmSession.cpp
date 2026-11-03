#include "NcmSession.h"

#include "CookieStore.h"
#include "CurlHttpClient.h"
#include "Oran7GlobalHelper.h"
#include "NcmPlaylistApi.h"
#include "NcmSongApi.h"
#include "NcmUserApi.h"
#include "WebLoginController.h"
#include <QQmlEngine>

static NcmSession* s_instance = nullptr;

NcmSession& NcmSession::instance()
{
    if (!s_instance)
        s_instance = new NcmSession();
    return *s_instance;
}

void NcmSession::cleanup()
{
    // 事件循环已关闭，析构里不 stop QTimer / 不主动断网，直接 delete 即可
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }
}

NcmSession::NcmSession(QObject* parent)
    : QObject(parent), m_cookieStore(std::make_unique<CookieStore>()),
      m_curlClient(std::make_unique<CurlHttpClient>()),
      m_userApi(std::make_unique<NcmUserApi>(m_curlClient.get())),
      m_songApi(std::make_unique<NcmSongApi>(m_curlClient.get())),
      m_playlistApi(std::make_unique<NcmPlaylistApi>(m_curlClient.get())),
      m_webLoginController(std::make_unique<WebLoginController>(this, m_userApi.get()))
{
    for (QObject* object : {static_cast<QObject*>(m_cookieStore.get()), static_cast<QObject*>(m_curlClient.get()),
            static_cast<QObject*>(m_userApi.get()), static_cast<QObject*>(m_songApi.get()),
            static_cast<QObject*>(m_playlistApi.get()), static_cast<QObject*>(m_webLoginController.get())})
        QQmlEngine::setObjectOwnership(object, QQmlEngine::CppOwnership);
    m_userApi->setCookieProvider([this]() { return m_cookieStore->cookieString(); });
    m_userApi->setCsrfProvider([this]() { return m_cookieStore->csrf(); });
    m_songApi->setCookieProvider([this]() {
        return m_cookieStore->cookieString() + QStringLiteral("; os=pc; appver=8.10.35");
    });
    m_playlistApi->setCookieProvider([this]() { return m_cookieStore->cookieString(); });
    m_playlistApi->setCsrfProvider([this]() { return m_cookieStore->csrf(); });

    // 转发 Cookie 校验结果，供网络监测器（Oran7NetworkMonitor）消费
    connect(m_userApi.get(), &NcmUserApi::verifyResult, this, [this](quint64 requestId, int valid) {
        if (requestId == m_verifyRequestId) emit verifyResult(valid);
    });
}

NcmSession::~NcmSession() = default;

bool NcmSession::isLoggedIn() const { return m_cookieStore->isLoggedIn(); }
QString NcmSession::userName() const { return m_cookieStore->nickname(); }
QString NcmSession::csrfToken() const { return m_cookieStore->csrf(); }

QString NcmSession::cookieString() const { return m_cookieStore->cookieString(); }
QString NcmSession::ffmpegCookieString() const { return m_cookieStore->ffmpegCookieString(); }

CookieStore* NcmSession::cookieStore() const { return m_cookieStore.get(); }
WebLoginController* NcmSession::webLoginController() const { return m_webLoginController.get(); }
NcmUserApi* NcmSession::userApi() const { return m_userApi.get(); }
NcmSongApi* NcmSession::songApi() const { return m_songApi.get(); }
NcmPlaylistApi* NcmSession::playlistApi() const { return m_playlistApi.get(); }

void NcmSession::load()
{
    if (!m_cookieStore->load()) WARNING_LOG << "NcmSession: 本地凭据未恢复，请重新登录";
    NETWORK_LOG << "NcmSession: load, isLoggedIn=" << isLoggedIn()
                ;
    emit loginStatusChanged();
}

void NcmSession::save()
{
    if (!m_cookieStore->save()) WARNING_LOG << "NcmSession: 凭据加密保存失败";
}

void NcmSession::logout()
{
    m_cookieStore->clear();
    if (!m_cookieStore->persistenceError().isEmpty()) WARNING_LOG << "NcmSession: 持久化退出登录失败，请重试";
    emit loginStatusChanged();
    NETWORK_LOG << "NcmSession: logout";
}

void NcmSession::verifyLogin()
{
    m_verifyRequestId = NcmRequest::nextId();
    m_userApi->verifyLogin(m_verifyRequestId);
}

void NcmSession::setLoggedIn(bool v)
{
    Q_UNUSED(v);
    // 登录态由 CookieStore 的 MUSIC_U 决定，此方法供登录链路通知刷新
    emit loginStatusChanged();
}

void NcmSession::setUserName(const QString& n)
{
    m_cookieStore->setNickname(n);
    emit loginStatusChanged();
}

void NcmSession::setCsrf(const QString& c)
{
    m_cookieStore->setAuthCookie(QStringLiteral("__csrf"), c);
    emit loginStatusChanged();
}
