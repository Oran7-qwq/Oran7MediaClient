#include "CookieStore.h"

#include "AppJsonConfigManager.h"

QVariantMap AppJsonCookiePersistence::load()
{
    QVariantMap data;
    data["MUSIC_U"]  = AppConfigManager::ins().getValueQVariant("netease_auth.MUSIC_U").toString();
    data["__csrf"]   = AppConfigManager::ins().getValueQVariant("netease_auth.__csrf").toString();
    data["nickname"] = AppConfigManager::ins().getValueQVariant("netease_auth.nickname").toString();
    return data;
}

void AppJsonCookiePersistence::save(const QVariantMap& data)
{
    AppConfigManager::ins().setValueQVariant("netease_auth.MUSIC_U",  data.value("MUSIC_U"));
    AppConfigManager::ins().setValueQVariant("netease_auth.__csrf",   data.value("__csrf"));
    AppConfigManager::ins().setValueQVariant("netease_auth.nickname", data.value("nickname"));
    AppConfigManager::ins().saveConfig();
}

void AppJsonCookiePersistence::clear()
{
    AppConfigManager::ins().setValueQVariant("netease_auth.MUSIC_U", "");
    AppConfigManager::ins().setValueQVariant("netease_auth.__csrf", "");
    AppConfigManager::ins().setValueQVariant("netease_auth.nickname", "");
    AppConfigManager::ins().saveConfig();
}

CookieStore::CookieStore(QObject* parent)
    : QNetworkCookieJar(parent)
    , m_persistence(std::make_unique<AppJsonCookiePersistence>())
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
    return true;
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
    return true;
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
