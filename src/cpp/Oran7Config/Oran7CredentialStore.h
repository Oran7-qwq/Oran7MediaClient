#pragma once

#include <QJsonObject>
#include <QStringList>

// 不持有业务对象；密文按分组/不可变代际保存，普通配置的引用是唯一提交点。
class Oran7CredentialStore
{
  public:
    explicit Oran7CredentialStore(QString directory);
    QString directory() const { return m_directory; }
    bool read(const QString& group, const QString& revision, QJsonObject* data, QString* error) const;
    bool writeVerified(const QString& group, const QJsonObject& data, QString* revision, QString* error) const;

    static QStringList groups();
    static bool validRevision(const QString& revision);
    static QJsonObject extract(const QJsonObject& config, const QString& group);
    static void remove(QJsonObject* config, const QString& group);
    static void apply(QJsonObject* config, const QString& group, const QJsonObject& data);
    static QJsonObject publicConfig(QJsonObject config, const QJsonObject& refs);

  private:
    QString m_directory;
};
