#ifndef COOKIESTORE_H
#define COOKIESTORE_H

#include <QNetworkCookieJar>
#include <QString>
#include <QVariantMap>

#include <memory>
class Oran7AppConfigManager;

/**
 * @brief Cookie 持久化后端抽象；默认后端经配置管理器使用当前 Windows 用户 DPAPI。
 */
class CookiePersistence
{
public:
    virtual ~CookiePersistence() = default;
    virtual QVariantMap load() = 0;  // {MUSIC_U, __csrf, nickname}
    virtual void save(const QVariantMap& data) = 0;
    virtual void clear() = 0;
    virtual QString lastError() const { return {}; }
};

/// 配置管理器协调加密文件与普通配置引用的提交，解密结果只留在内存快照。
class Oran7DpapiCookiePersistence : public CookiePersistence
{
public:
    Oran7DpapiCookiePersistence();
    explicit Oran7DpapiCookiePersistence(Oran7AppConfigManager& config);
    QVariantMap load() override;
    void save(const QVariantMap& data) override;
    void clear() override;
    QString lastError() const override { return m_lastError; }
private:
    Oran7AppConfigManager& m_config; // 借用：宿主配置管理器覆盖此适配器生命周期。
    QString m_lastError;
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
    explicit CookieStore(std::unique_ptr<CookiePersistence> persistence, QObject* parent = nullptr);

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
    QString persistenceError() const { return m_persistence ? m_persistence->lastError() : QStringLiteral("未设置凭据存储"); }

signals:
    void cookiesChanged();

private:
    QString m_musicU;
    QString m_csrf;
    QString m_nickname;
    std::unique_ptr<CookiePersistence> m_persistence;
};

#endif // COOKIESTORE_H
