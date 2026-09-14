#ifndef COOKIESTORE_H
#define COOKIESTORE_H

#include <QNetworkCookieJar>
#include <QString>
#include <QVariantMap>

#include <memory>

/**
 * @brief Cookie 持久化后端抽象：v1 = AppConfigManager(JSON 明文)
 *
 * 为后续 DPAPI/Keychain 等安全存储预留替换接口。
 */
class CookiePersistence
{
public:
    virtual ~CookiePersistence() = default;
    virtual QVariantMap load() = 0;  // {MUSIC_U, __csrf, nickname}
    virtual void save(const QVariantMap& data) = 0;
    virtual void clear() = 0;
};

/// 基于 AppConfigManager 的 JSON 明文持久化（键前缀 "netease_auth"，与存量 config.json 一致）
class AppJsonCookiePersistence : public CookiePersistence
{
public:
    QVariantMap load() override;
    void save(const QVariantMap& data) override;
    void clear() override;
};

/**
 * @brief 网易云会话 Cookie 存储
 *
 * 继承 QNetworkCookieJar（对齐架构分层），但作为数据持有者使用，
 * 不装成 QNetworkAccessManager 的自动 cookie jar。
 */
class CookieStore : public QNetworkCookieJar
{
    Q_OBJECT

public:
    explicit CookieStore(QObject* parent = nullptr);

    void setPersistence(std::unique_ptr<CookiePersistence> p);

    void setAuthCookie(const QString& name, const QString& value); // MUSIC_U / __csrf
    void setNickname(const QString& nickname);

    QString cookieValue(const QString& name) const;
    QString musicU() const;
    QString csrf() const;
    QString nickname() const;

    QString cookieString() const;       // "MUSIC_U=..; __csrf=.."
    QString ffmpegCookieString() const; // 同 cookieString()
    bool isLoggedIn() const;            // !musicU().isEmpty()

    bool load();
    bool save();
    void clear();

signals:
    void cookiesChanged();

private:
    QString m_musicU;
    QString m_csrf;
    QString m_nickname;
    std::unique_ptr<CookiePersistence> m_persistence;
};

#endif // COOKIESTORE_H
