#pragma once

#include <QQmlNetworkAccessManagerFactory>
#include <QString>

// 由 main 持有并覆盖 QML 引擎生命周期；每个网络管理器在调用线程内创建自己的磁盘缓存。
class Oran7CoverNetworkFactory final : public QQmlNetworkAccessManagerFactory
{
  public:
    explicit Oran7CoverNetworkFactory(QString cacheDirectory);
    QNetworkAccessManager* create(QObject* parent) override;

  private:
    const QString m_cacheDirectory;
};
