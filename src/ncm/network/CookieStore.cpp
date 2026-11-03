#include "CookieStore.h"
#include "Oran7AppConfigManager.h"

Oran7DpapiCookiePersistence::Oran7DpapiCookiePersistence() : Oran7DpapiCookiePersistence(Oran7AppConfigManager::ins()) {}
Oran7DpapiCookiePersistence::Oran7DpapiCookiePersistence(Oran7AppConfigManager& config) : m_config(config) {}

QVariantMap Oran7DpapiCookiePersistence::load()
{
    m_lastError = m_config.credentialError("netease");
    const auto saved = m_config.snapshot().auth.netease;
    return {{"MUSIC_U", saved.musicU}, {"__csrf", saved.csrf}, {"nickname", saved.nickname}};
}

void Oran7DpapiCookiePersistence::save(const QVariantMap& data)
{
    auto saved = m_config.snapshot();
    saved.auth.netease = {data.value("MUSIC_U").toString(), data.value("__csrf").toString(), data.value("nickname").toString()};
    m_config.updateConfig(saved);
    m_lastError = m_config.saveConfig() ? m_config.credentialError("netease") : m_config.lastError();
}

void Oran7DpapiCookiePersistence::clear()
{
    auto& config = m_config;
    m_lastError = config.clearCredentialGroup("netease") ? QString() : config.lastError();
}

CookieStore::CookieStore(QObject* parent)
    : CookieStore(std::make_unique<Oran7DpapiCookiePersistence>(), parent)
{
}

CookieStore::CookieStore(std::unique_ptr<CookiePersistence> persistence, QObject* parent)
    : QNetworkCookieJar(parent), m_persistence(std::move(persistence))
{
}

void CookieStore::setPersistence(std::unique_ptr<CookiePersistence> p)
{
    m_persistence = std::move(p);
}

void CookieStore::setAuthCookie(const QString& name, const QString& value)
{
    if (name == QLatin1String("MUSIC_U"))
        m_musicU = value;
    else if (name == QLatin1String("__csrf"))
        m_csrf = value;
    emit cookiesChanged();
}

void CookieStore::setNickname(const QString& nickname)
{
    m_nickname = nickname;
    emit cookiesChanged();
}

QString CookieStore::cookieValue(const QString& name) const
{
    if (name == QLatin1String("MUSIC_U"))
        return m_musicU;
    if (name == QLatin1String("__csrf"))
        return m_csrf;
    return QString();
}

QString CookieStore::musicU() const { return m_musicU; }
QString CookieStore::csrf() const { return m_csrf; }
QString CookieStore::nickname() const { return m_nickname; }

QString CookieStore::cookieString() const
{
    QStringList parts;
    if (!m_musicU.isEmpty()) parts << QStringLiteral("MUSIC_U=%1").arg(m_musicU);
    if (!m_csrf.isEmpty())   parts << QStringLiteral("__csrf=%1").arg(m_csrf);
    return parts.join(QStringLiteral("; "));
}

QString CookieStore::ffmpegCookieString() const { return cookieString(); }

bool CookieStore::isLoggedIn() const { return !m_musicU.isEmpty(); }

bool CookieStore::load()
{
    if (!m_persistence)
        return false;
    const QVariantMap data = m_persistence->load();
    m_musicU   = data.value("MUSIC_U").toString();
    m_csrf     = data.value("__csrf").toString();
    m_nickname = data.value("nickname").toString();
    emit cookiesChanged();
    return m_persistence->lastError().isEmpty();
}

bool CookieStore::save()
{
    if (!m_persistence)
        return false;
    m_persistence->save({
        { QStringLiteral("MUSIC_U"),  m_musicU },
        { QStringLiteral("__csrf"),   m_csrf },
        { QStringLiteral("nickname"), m_nickname }
    });
    return m_persistence->lastError().isEmpty();
}

void CookieStore::clear()
{
    m_musicU.clear();
    m_csrf.clear();
    m_nickname.clear();
    if (m_persistence)
        m_persistence->clear();
    emit cookiesChanged();
}
