#pragma once

#include <QByteArray>
#include <QVariantMap>

// 场景单独保存。构建和离线检查可传入临时路径。
class Oran7CaptureSceneStore
{
  public:
    explicit Oran7CaptureSceneStore(QString filePath);
    QVariantMap load();
    QVariantMap save(const QVariantMap& library);

  private:
    QString m_path;
    QByteArray m_committed;
    bool m_existed = false;
    bool m_writable = false;
};
