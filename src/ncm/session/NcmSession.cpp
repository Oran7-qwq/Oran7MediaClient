#include "NcmSession.h"

#include "CookieStore.h"
#include "CurlHttpClient.h"
#include "GlobalHelper.h"
#include "HttpClient.h"
#include "NcmAuthApi.h"
#include "NcmPlaylistApi.h"
#include "NcmSongApi.h"
#include "NcmUserApi.h"
#include "QrLoginController.h"
#include "WebLoginController.h"

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
    : QObject(parent)
    , m_httpClient(new HttpClient(this))
    , m_curlClient(new CurlHttpClient(this))
    , m_cookieStore(new CookieStore(this))
    , m_authApi(new NcmAuthApi(m_curlClient, this))
    , m_userApi(new NcmUserApi(m_curlClient, this))
    , m_songApi(new NcmSongApi(m_curlClient, this))
    , m_playlistApi(new NcmPlaylistApi(m_httpClient, m_curlClient, this))
    , m_qrController(new QrLoginController(this, m_authApi, m_userApi, this))
    , m_webLoginController(new WebLoginController(this, m_userApi, this))
{
    // 鉴权请求显式注入 Cookie；扫码请求不注入。
    // song/url 必须带 os=pc + appver，否则返回的 CDN 地址会被热链保护(403 origin failed)；
    // 且 song/url 必须走 curl 通道（QNAM 对 VIP 歌返回 code:-110）。
    m_httpClient->setCookieProvider([this]() {
        return m_cookieStore->cookieString() + QStringLiteral("; os=pc; appver=8.10.35");
    });
    m_userApi->setCookieProvider([this]() { return m_cookieStore->cookieString(); });
    m_userApi->setCsrfProvider([this]() { return m_cookieStore->csrf(); });
    m_songApi->setCookieProvider([this]() {
        return m_cookieStore->cookieString() + QStringLiteral("; os=pc; appver=8.10.35");
    });
    m_playlistApi->setCookieProvider([this]() { return m_cookieStore->cookieString(); });
    m_playlistApi->setCsrfProvider([this]() { return m_cookieStore->csrf(); });

    // 转发 Cookie 校验结果，供网络监测器（Oran7NetworkMonitor）消费
    connect(m_userApi, &NcmUserApi::verifyResult, this, &NcmSession::verifyResult);
}

NcmSession::~NcmSession() = default;

bool NcmSession::isLoggedIn() const { return m_cookieStore->isLoggedIn(); }
QString NcmSession::userName() const { return m_cookieStore->nickname(); }
QString NcmSession::csrfToken() const { return m_cookieStore->csrf(); }

QString NcmSession::cookieString() const { return m_cookieStore->cookieString(); }
QString NcmSession::ffmpegCookieString() const { return m_cookieStore->ffmpegCookieString(); }

CookieStore* NcmSession::cookieStore() const { return m_cookieStore; }
QrLoginController* NcmSession::qrController() const { return m_qrController; }
WebLoginController* NcmSession::webLoginController() const { return m_webLoginController; }
NcmUserApi* NcmSession::userApi() const { return m_userApi; }
NcmSongApi* NcmSession::songApi() const { return m_songApi; }
NcmPlaylistApi* NcmSession::playlistApi() const { return m_playlistApi; }

void NcmSession::load()
{
    m_cookieStore->load();
    NETWORK_LOG << "NcmSession: load, isLoggedIn=" << isLoggedIn()
                << " user=" << userName();
    emit loginStatusChanged();
}

void NcmSession::save()
{
    m_cookieStore->save();
}

void NcmSession::logout()
{
    m_cookieStore->clear();
    emit loginStatusChanged();
    NETWORK_LOG << "NcmSession: logout";
}

void NcmSession::verifyLogin()
{
    m_userApi->verifyLogin();
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
